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
    for (const char *p=m, *end=p+m_len; ; ) {
      if (end-p < prefix_len+2) ERROR(filename," file is corrupted");
      p += pw_len;
      by_cookie.emplace(p,cookie_len);
      p += cookie_len;
      p += by_name.emplace(p).first->size()+1;
      if (p==end) break;
      if (p > end) ERROR(filename," file is corrupted");
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
  if (user == by_name.end()) return nullptr;
  // TODO: check password
  return user->data();
}
const char*
users_table::cookie_login(std::string_view cookie) const {
  const auto user = by_cookie.find(cookie);
  return user != by_cookie.end() ? user->data()+cookie_len : nullptr;
}

const char*
users_table::operator[](std::string_view name) const noexcept {
  const auto user = by_name.find(name);
  return user != by_name.end() ? user->data() : nullptr;
}
