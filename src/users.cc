#include "users.hh"

#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <algorithm>
#include <limits>

#include "error.hh"
#include "debug.hh"
#include "bcrypt/bcrypt.hh"

namespace {

[[ gnu::const ]]
unsigned next_pow2(unsigned x) {
  // http://locklessinc.com/articles/next_pow2/
  x -= 1;
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  return x + 1;
}

std::mt19937 rng(
  std::chrono::system_clock::now().time_since_epoch().count());

constexpr char charset[] =
  "0123456789"
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz";

std::uniform_int_distribution<unsigned> charset_dist(0,sizeof(charset)-2);
std::uniform_int_distribution<char> char_dist(
  std::numeric_limits<char>::min(),
  std::numeric_limits<char>::max()
);

void rndstr(char* str, unsigned len) noexcept {
  std::generate_n(str, len, []{ return charset[charset_dist(rng)]; });
}

}

users_table::users_table(const char* filename) {
  // mmap the file
  struct stat sb;
  fd = ::open(filename, O_RDWR|O_CREAT, 0644);
  if (fd == -1) THROW_ERRNO("open()");
  if (::fstat(fd, &sb) == -1) THROW_ERRNO("fstat()");
  if (!S_ISREG(sb.st_mode)) THROW_ERRNO("not a file");
  const unsigned f_len = sb.st_size;
  m_len = std::max(next_pow2(f_len),1u<<20);
  if (m_len != f_len)
    if (::ftruncate(fd,m_len) == -1) THROW_ERRNO("ftruncate()");

  m = reinterpret_cast<char*>(::mmap(0,m_len,PROT_READ,MAP_SHARED,fd,0));
  if (m == MAP_FAILED) THROW_ERRNO("mmap()");

  // create hash tables
  for (const char *p=m, *end=p+f_len; ; ) {
    if (end-p < prefix_len+2) ERROR(filename," file is corrupted");
    p += pw_len;
    by_cookie.emplace(p,cookie_len);
    p += cookie_len;
    p += by_name.emplace(p).first->size()+1;
    if (p==end || *p=='\0') break;
    if (p > end) ERROR(filename," file is corrupted");
  }
}

void users_table::expand() {
}

users_table::~users_table() {
  if (m && ::munmap(m,m_len) == -1)
    std::cerr << IVANP_ERROR_PREF "munmap(): "
      << std::strerror(errno) << std::endl;
  if (fd && ::close(fd) == -1)
    std::cerr << IVANP_ERROR_PREF "close(): "
      << std::strerror(errno) << std::endl;
}

const char* users_table::operator[](std::string_view name) const noexcept {
  const auto user = by_name.find(name);
  return user != by_name.end() ? user->data() : nullptr;
}
char* users_table::operator[](std::string_view name) noexcept {
  const auto user = by_name.find(name);
  return user != by_name.end() ? const_cast<char*>(user->data()) : nullptr;
}

// TODO: return char* user
const char*
users_table::cookie_login(std::string_view cookie) const {
  const auto user = by_cookie.find(cookie);
  return user != by_cookie.end() ? user->data()+cookie_len : nullptr;
}
const char*
users_table::pw_login(std::string_view name, const char* pw) const {
  const char* const user = (*this)[name];
  return (user && bcrypt_check(pw,user-prefix_len)) ? user : nullptr;
}

std::string rndstr(unsigned len) noexcept {
  std::string pw(len,'\0');
  rndstr(pw.data(),pw.size());
  return pw;
}

void users_table::reset_cookie(char* user) noexcept {
  do { // generate unique cookie
    rndstr(user-cookie_len,cookie_len);
  } while (by_cookie.find({user-cookie_len,cookie_len})!=by_cookie.end());
}

void users_table::reset_pw(char* user, const char* pw) {
  reset_cookie(user);

  char salt[16];
  std::generate_n(salt,sizeof(salt),[]{ return char_dist(rng); });
  bcrypt_hash(user-prefix_len,pw,salt,sizeof(salt));
}
