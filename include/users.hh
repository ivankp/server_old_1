#ifndef IVANP_SERVER_USERS_HH
#define IVANP_SERVER_USERS_HH

#include <string>
#include <string_view>
#include "bcrypt/bcrypt_common.hh"

class users_table {
public:
  static constexpr unsigned
        pw_len = bcrypt_hash_len,
    cookie_len = 16,
    prefix_len = cookie_len + pw_len,
    max_name_len = 16,
    max_user_len = prefix_len + max_name_len + 1;

private:
  char* m = nullptr;
  unsigned *by_name = nullptr, *by_cookie = nullptr;
  unsigned m_len = 0, f_len = 0; // (1<<32)/93 > 46 million
  unsigned u_len = 0, u_cap = 0;
  int fd = -1;

  unsigned find_by_name(const char* name) const noexcept;
  unsigned find_by_cookie(const char* cookie) const noexcept;

  void reset_cookie_impl(char* user) noexcept;
  void reset_pw_impl(char* user, const char* pw);

  void append_user(const char* user);

public:
  explicit users_table() noexcept = default;
  explicit users_table(const char* filename);
  users_table(const users_table&) = delete;
  users_table& operator=(const users_table&) = delete;
  users_table(users_table&& o) noexcept {
    (*this) = std::move(o);
  }
  users_table& operator=(users_table&& o) noexcept;
  ~users_table();

  const char* operator[](const char* name) const noexcept {
    const unsigned u = find_by_name(name);
    return u ? m+u : nullptr;
  }

  const char* cookie_login(const char* cookie) const noexcept;
  const char* pw_login(const char* name, const char* pw) const;

  void reset_cookie(const char* name);
  void reset_pw(const char* name, const char* pw);

  const char* new_user(const char* name, const char* pw);
};

inline std::string_view get_cookie(const char* user) noexcept {
  return { user-users_table::cookie_len, users_table::cookie_len };
}

std::string rndstr(unsigned len) noexcept;

#endif
