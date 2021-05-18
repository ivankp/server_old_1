#include "file_cache.hh"

#include <iostream>
#include <cstdlib>
#include <map>
#include <string>
#include <thread>
#include <shared_mutex>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "local_fd.hh"
#include "zlib.hh"
#include "error.hh"

namespace ivanp {
namespace {

struct cached_file {
  char *data = nullptr, *zdata = nullptr;
  size_t size = 0, zsize = 0;
  time_t time = 0;

  ~cached_file() {
    free( data); // ok to free nullptr
    free(zdata);
  }
};

std::map<std::string,cached_file> files;
std::shared_mutex mx_file_cache;

}

locked_cache_view::~locked_cache_view() {
  if (data) mx_file_cache.unlock_shared();
  if (fd != -1) ::close(fd);
}

locked_cache_view file_cache(const char* name, bool gz) {
  const int fd = PCALLR(open)(name,O_RDONLY);
  try {
    struct stat sb;
    PCALL(fstat)(fd,&sb);
    if (!S_ISREG(sb.st_mode)) ERROR("not a regular file");

    if (sb.st_size == 0) return { };

retry_cached:
    mx_file_cache.lock_shared();
    auto& f = files[name];
    const bool same_time = (sb.st_mtime == f.time);
    if (same_time && (gz ? f.zdata : f.data))
      goto return_cached;
    mx_file_cache.unlock_shared();

    if ((size_t)sb.st_size > file_cache_max_size)
      return { nullptr, sb.st_size, false, fd };
      // gz = false if too large to cache

    if (!mx_file_cache.try_lock()) {
      std::this_thread::yield();
      goto retry_cached;
    }

    if (!same_time) {
      f.time = sb.st_mtime;
      free(f.data);
      f.data = reinterpret_cast<char*>( malloc(f.size = sb.st_size) );
      try {
        PCALL(read)(fd, f.data, f.size);
      } catch (...) {
        mx_file_cache.unlock();
        throw;
      }
    }

    free(f.zdata);
    if (gz) {
      try {
        zlib::deflate_alloc(f.data,f.size,f.zdata,f.zsize);
      } catch (const std::exception& e) {
        REDERR << e.what() << std::endl;
        gz = false;
        f.zdata = nullptr;
        f.zsize = 0;
      }
    } else {
      f.zdata = nullptr;
      f.zsize = 0;
    }

    // TODO: correctly downgrade unique to shared lock
    mx_file_cache.unlock();
    mx_file_cache.lock_shared();

return_cached:
    if (gz) return { f.zdata, f.zsize, true, fd };
    else return { f.data, f.size, false, fd };
  } catch (...) {
    ::close(fd);
    throw;
  }
}
// TODO: free old files

} // end namespace ivanp
