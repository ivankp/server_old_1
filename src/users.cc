#include "users.hh"

#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <random>
#include <chrono>
#include <algorithm>
#include <iterator>
#include <limits>

#include "bcrypt/bcrypt.hh"
#include "error.hh"
// #include "debug.hh"

namespace {

constexpr unsigned next_pow2(unsigned x) noexcept {
  // http://locklessinc.com/articles/next_pow2/
  x -= 1;
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  return x + 1;
}

constexpr unsigned divceil(unsigned a, unsigned b) noexcept {
  // https://stackoverflow.com/a/63436491/2640636
  return a / b + (a % b != 0);
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

template <typename... T>
[[gnu::always_inline]]
inline void maybe_realloc(
  unsigned need, unsigned& cap, T*&... m
) {
  if (need > cap) {
    cap *= 2;
    (..., [&](){
      if (void* const r = realloc(m,cap*sizeof(T))) {
        m = static_cast<T*>(r);
      } else {
        cap /= 2;
        THROW_ERRNO("realloc()");
      }
    }());
  }
}

void rotate_right(auto a, auto b) noexcept {
  auto rb = std::make_reverse_iterator(b);
  std::rotate(rb, rb+1, std::make_reverse_iterator(a));
}

struct name_cmp {
  const char* m;
  bool operator()(unsigned a, unsigned b) const noexcept {
    return strcmp(m+a,m+b) < 0;
  }
  bool operator()(unsigned a, const char* b) const noexcept {
    return strcmp(m+a,b) < 0;
  }
  bool operator()(const char* a, unsigned b) const noexcept {
    return strcmp(a,m+b) < 0;
  }
  bool operator()(const char* a, const char* b) const noexcept {
    return strcmp(a,b) < 0;
  }
};
struct cookie_cmp {
  const char* m;
  bool operator()(unsigned a, unsigned b) const noexcept {
    return memcmp(m+a,m+b,users_table::cookie_len) < 0;
  }
  bool operator()(unsigned a, const char* b) const noexcept {
    return memcmp(m+a,b,users_table::cookie_len) < 0;
  }
  bool operator()(const char* a, unsigned b) const noexcept {
    return memcmp(a,m+b,users_table::cookie_len) < 0;
  }
  bool operator()(const char* a, const char* b) const noexcept {
    return memcmp(a,b,users_table::cookie_len) < 0;
  }
};

} // end namespace

std::string rndstr(unsigned len) noexcept {
  std::string pw(len,'\0');
  rndstr(pw.data(),pw.size());
  return pw;
}

users_table::users_table(const char* filename) {
  struct stat sb;
  fd = ::open(filename, O_RDWR|O_CREAT, 0644);
  if (fd == -1) THROW_ERRNO("open()");
  if (::fstat(fd, &sb) == -1) THROW_ERRNO("fstat()");
  if (!S_ISREG(sb.st_mode)) THROW_ERRNO("not a file");
  f_len = sb.st_size;
  m_len = std::max(
    next_pow2(f_len),
    next_pow2(max_user_len) * 8
  );
  m = static_cast<char*>(malloc(m_len));
  if (::read(fd,m,f_len) == -1) THROW_ERRNO("read()");

  u_cap = std::max(next_pow2(divceil(f_len,max_user_len)),1u<<5);
  by_name   = static_cast<unsigned*>(malloc(u_cap*sizeof(unsigned)));
  by_cookie = static_cast<unsigned*>(malloc(u_cap*sizeof(unsigned)));
  if (f_len) {
    for (const char *p=m, *end=p+f_len; ; ) {
      if (end-p < prefix_len+2) ERROR(filename," file is corrupted");
      p += prefix_len;
      append_user(p);
      while (*p != '\0') ++p;
      ++p;
      if (p==end) break;
      if (p > end) ERROR(filename," file is corrupted");
    }

    std::sort(by_name,by_name+u_len,name_cmp{m});
    std::sort(by_cookie,by_cookie+u_len,cookie_cmp{m});
  }
}

void users_table::swap(users_table& o) noexcept {
  std::swap_ranges(
    reinterpret_cast<char*>(this),
    reinterpret_cast<char*>(this)+sizeof(users_table),
    reinterpret_cast<char*>(&o)
  );
}

users_table::~users_table() {
  free(m);
  free(by_name);
  free(by_cookie);
  if (fd!=-1) ::close(fd);
}

void users_table::append_user(const char* user) {
  const unsigned u = user - m;
  maybe_realloc(u_len+1,u_cap,by_name,by_cookie);
  by_name  [u_len] = u;
  by_cookie[u_len] = u-cookie_len;
  ++u_len;
}

unsigned users_table::find_by_name(const char* name) const noexcept {
  if (m < name && name < (m+f_len)) return name-m;
  auto a = by_name;
  const auto b = a + u_len;
  auto cmp = name_cmp{m};
  a = std::lower_bound(a, b, name, cmp);
  return (a==b || cmp(name,*a)) ? 0 : *a;
}
unsigned users_table::find_by_cookie(const char* cookie) const noexcept {
  auto a = by_cookie;
  const auto b = a + u_len;
  auto cmp = cookie_cmp{m};
  a = std::lower_bound(a, b, cookie, cmp);
  return (a==b || cmp(cookie,*a)) ? 0 : *a+cookie_len;
}

const char* users_table::cookie_login(const char* cookie) const noexcept {
  const unsigned u = find_by_cookie(cookie);
  return u ? m+u : nullptr;
}
const char* users_table::pw_login(const char* name, const char* pw) const {
  const unsigned u = find_by_name(name);
  return (u && bcrypt_check(pw,m+u-prefix_len)) ? m+u : nullptr;
}

void users_table::reset_cookie_impl(char* user) noexcept {
  char tmp[cookie_len];
  for (;;) { // generate unique cookie
    rndstr(tmp,cookie_len);
    if (!find_by_cookie(tmp)) {
      memcpy(user-cookie_len,tmp,cookie_len);
      break;
    }
  }
  std::sort(by_cookie,by_cookie+u_len,cookie_cmp{m});
}
void users_table::reset_pw_impl(char* user, const char* pw) {
  char salt[16];
  std::generate_n(salt,sizeof(salt),[]{ return char_dist(rng); });
  bcrypt_hash(user-prefix_len,pw,salt,sizeof(salt));
  reset_cookie_impl(user);
}

void users_table::reset_cookie(const char* name) {
  unsigned u = find_by_name(name);
  // TODO: block
  reset_cookie_impl(m+u);
  u -= cookie_len;
  if (::pwrite(fd,m+u,cookie_len,u) == -1) THROW_ERRNO("pwrite()");
}
void users_table::reset_pw(const char* name, const char* pw) {
  unsigned u = find_by_name(name);
  // TODO: block
  reset_pw_impl(m+u,pw);
  u -= prefix_len;
  if (::pwrite(fd,m+u,prefix_len,u) == -1) THROW_ERRNO("pwrite()");
}

const char* users_table::new_user(const char* name, const char* pw) {
  unsigned len = 0;
  for (;;) {
    if (name[len]=='\0') {
      if (len==0) return 0;
      ++len; // include null terminator
      break;
    }
    if (++len > max_name_len) return 0;
  }
  if (find_by_name(name)) return 0;

  // TODO: block
  const unsigned user_len = prefix_len + len;
  maybe_realloc(f_len+user_len,m_len,m);

  char* const user = m+f_len+prefix_len;
  memcpy(user,name,len);
  reset_pw_impl(user,pw);

  if (::write(fd,user-prefix_len,user_len) == -1) THROW_ERRNO("write()");

  // index the new user
  const auto pos_name = std::upper_bound(
    by_name, by_name+u_len, user, name_cmp{m});
  const auto pos_cookie = std::upper_bound(
    by_cookie, by_cookie+u_len, user-cookie_len, cookie_cmp{m});
  append_user(user);
  rotate_right(pos_name,by_name+u_len);
  rotate_right(pos_cookie,by_cookie+u_len);

  // this must be the last step
  // so that in case of failure previous steps can be ignored
  f_len += user_len;

  return user;
}
