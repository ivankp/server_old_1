#ifndef IVANP_SERVER_USERS_HH
#define IVANP_SERVER_USERS_HH

#include <unordered_set>
#include <string_view>

class users_table {
  char* m;
  size_t m_len;

  std::unordered_set<std::string_view>
    by_cookie, by_name;

public:
  static constexpr unsigned
        pw_len = 60,
    cookie_len = 16,
    prefix_len = cookie_len + pw_len;

  explicit users_table() noexcept: m(nullptr), m_len(0) { }
  explicit users_table(const char* filename);
  users_table(const users_table&) = delete;
  users_table& operator=(const users_table&) = delete;
  users_table(users_table&& o) noexcept
  : m(o.m), m_len(o.m_len),
    by_cookie(std::move(o.by_cookie)), by_name(std::move(o.by_name))
  { o.m = nullptr; }
  users_table& operator=(users_table&& o) noexcept {
    std::swap(m,o.m);
    std::swap(m_len,o.m_len);
    by_cookie = std::move(o.by_cookie);
    by_name = std::move(o.by_name);
    return *this;
  }
  ~users_table();

  const char* pw_login(std::string_view name, std::string_view pw) const;
  const char* cookie_login(std::string_view cookie) const;

  bool empty() const noexcept { return m; }

  const char* operator[](std::string_view name) const noexcept;
};

inline std::string_view get_cookie(const char* name) noexcept {
  return { name-users_table::cookie_len, users_table::cookie_len };
}

#endif
