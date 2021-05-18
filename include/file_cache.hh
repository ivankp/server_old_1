#ifndef IVANP_FILE_CACHE_HH
#define IVANP_FILE_CACHE_HH

#include <string_view>
#include <ctime>

namespace ivanp {

struct locked_cache_view {
  const char* data = nullptr;
  size_t size = 0;
  bool gz = false;
  int fd = -1;

  ~locked_cache_view();
  locked_cache_view(const locked_cache_view&) = delete;
  locked_cache_view& operator=(const locked_cache_view&) = delete;
  locked_cache_view(locked_cache_view&& o) noexcept
  : data(o.data), size(o.size), gz(o.gz) {
    o.data = nullptr;
    o.size = 0;
    o.fd = -1;
  }
  locked_cache_view& operator=(locked_cache_view&& o) noexcept {
    std::swap(data,o.data);
    std::swap(size,o.size);
    std::swap(gz,o.gz);
    std::swap(fd,o.fd);
    return *this;
  }
  operator std::string_view() const noexcept {
    return data ? { data, size } : { };
  }
};

inline size_t file_cache_max_size = 1 << 20;

locked_cache_view file_cache(const char* name, bool gz);

}

#endif
