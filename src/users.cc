#include "users.hh"

#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "error.hh"
#include "debug.hh"

users_table::users_table(const char* filename) {
  // mmap the file
  struct stat sb;
  const int fd = ::open(filename, O_RDONLY);
  if (fd == -1) THROW_ERRNO("open()");
  if (::fstat(fd, &sb) == -1) THROW_ERRNO("fstat()");
  if (!S_ISREG(sb.st_mode)) THROW_ERRNO("not a file");
  m_len = sb.st_size;
  if (m_len) {
    m = reinterpret_cast<char*>(::mmap(0,m_len,PROT_READ,MAP_SHARED,fd,0));
    if (m == MAP_FAILED) THROW_ERRNO("mmap()");
    if (::close(fd) == -1) THROW_ERRNO("close()");

    // create hash tables
    for (const char *p=m, *end=p+m_len; p!=end; ) {
      if (p > end) ERROR(filename," file is corrupted");
      by_cookie.emplace(p,p+cookie_len);
      p += prefix_len;
      const std::string_view name(p);
      by_name.emplace(name);
      p += name.size()+1;
    }
  } else {
    m = nullptr;
    INFO("33;1","empty ",filename," file");
  }
}

users_table::~users_table() {
  if (m) ::munmap(m,m_len);
}

const char*
users_table::pw_login(std::string_view name, std::string_view pw) const {
  const auto user = by_name.find(name);
  // TODO: check password
  return user != by_cookie.end() ? user->data() : nullptr;
}
const char*
users_table::cookie_login(std::string_view cookie) const {
  const auto user = by_cookie.find(cookie);
  return user != by_cookie.end() ? user->data()+prefix_len : nullptr;
}

std::string_view
users_table::operator[](std::string_view name) const noexcept {
  const auto user = by_name.find(name);
  return user != by_cookie.end() ? *user : std::string_view();
}
