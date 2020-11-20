#ifndef IVANP_SERVER_USERS_HH
#define IVANP_SERVER_USERS_HH

#include <unordered_set>
#include <string_view>

class users_table {
public:
  static constexpr unsigned
        pw_len = bcrypt_hash_len,
    cookie_len = 16,
    prefix_len = cookie_len + pw_len;

private:
  int fd;
  char* m;
  unsigned m_len;

  struct user_ptr { unsigned p; };

  struct user_ptr_eq {
    using is_transparent = void;
    const users_table* t;
    bool operator()(user_ptr a, user_ptr b) const noexcept {
      return a.p == b.p;
    }
    auto operator()(user_ptr u, const auto& b) const noexcept {
      return std::string_view(t->m+u.p) == b;
    }
    auto operator()(const auto& a, user_ptr u) const noexcept {
      return std::string_view(t->m+u.p) == a;
    }
  };

  struct cookie_hash {
    using is_transparent = void;
    const users_table* t;
    auto operator()(user_ptr u) const noexcept {
      return std::hash<std::string_view>{}({t->m+u.p-cookie_len,cookie_len});
    }
    auto operator()(const auto& x) const noexcept {
      return std::hash<std::string_view>{}(x);
    }
  };

  struct name_hash {
    using is_transparent = void;
    const users_table* t;
    auto operator()(user_ptr u) const noexcept {
      return std::hash<std::string_view>{}({t->m+u.p});
    }
    auto operator()(const auto& x) const noexcept {
      return std::hash<std::string_view>{}(x);
    }
  };

  std::unordered_set<user_ptr,cookie_hash,user_ptr_eq> by_cookie;
  std::unordered_set<user_ptr,  name_hash,user_ptr_eq> by_name;

  void expand();

public:
  explicit users_table() noexcept: fd(0), m(nullptr), m_len(0) { }
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

  bool empty() const noexcept { return m; }
  const char* operator[](std::string_view name) const noexcept;
  char* operator[](std::string_view name) noexcept;

  const char* cookie_login(std::string_view cookie) const;
  const char* pw_login(std::string_view name, std::string_view pw) const;

  void reset_cookie(char* user) noexcept;
  void reset_pw(char* user, const char* pw);
};

inline std::string_view get_cookie(const char* user) noexcept {
  return { user-users_table::cookie_len, users_table::cookie_len };
}

std::string rndstr(unsigned len) noexcept;

#endif
