#ifndef IVANP_STRING_HH
#define IVANP_STRING_HH

#include <cstring>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace ivanp {

template <typename S, typename... T>
[[ gnu::always_inline ]]
inline S&& stream(S&& s, T&&... x) {
  (s << ... << std::forward<T>(x));
  return s;
}

inline std::string cat() noexcept { return { }; }
inline std::string cat(std::string x) noexcept { return x; }
inline std::string cat(const char* x) noexcept { return x; }

template <typename T>
[[ gnu::always_inline ]]
inline size_t str_size(T& x) noexcept {
  using type = std::decay_t<T>;
  if constexpr (std::is_same_v<type,char>)
    return 1;
  else if constexpr (
    std::is_pointer_v<type> &&
    std::is_same_v<std::decay_t<std::remove_pointer_t<type>>,char>
  )
    return strlen(x);
  else
    return x.size();
}

template <typename... T>
[[ gnu::always_inline ]]
inline std::string cat(T&&... x) {
  if constexpr ((... &&
    requires (std::string& s) {
      s += x;
      str_size(x);
    }
  )) {
    std::string s;
    s.reserve((... + str_size(x)));
    (s += ... += x);
    return s;
  } else {
    std::stringstream s;
    (s << ... << std::forward<T>(x));
    return std::move(s).str();
  }
}

// ------------------------------------------------------------------

inline const char* cstr(const char* x) noexcept { return x; }
inline char* cstr(char* x) noexcept { return x; }
inline const char* cstr(const std::string& x) noexcept { return x.data(); }
inline char* cstr(std::string& x) noexcept { return x.data(); }

// ------------------------------------------------------------------

struct chars_less {
  using is_transparent = void;
  bool operator()(const char* a, const char* b) const noexcept {
    return strcmp(a,b) < 0;
  }
  template <typename T>
  bool operator()(const T& a, const char* b) const noexcept {
    return a < b;
  }
  template <typename T>
  bool operator()(const char* a, const T& b) const noexcept {
    return a < b;
  }
};

inline bool starts_with(const char* str, const char* prefix) noexcept {
  for (;; ++str, ++prefix) {
    const char c = *prefix;
    if (!c) break;
    if (*str != c) return false;
  }
  return true;
}

inline bool ends_with(const char* str, const char* suffix) noexcept {
  const auto n1 = strlen(str);
  const auto n2 = strlen(suffix);
  if (n1<n2) return false;
  return starts_with(str+(n1-n2),suffix);
}

} // end namespace ivanp

#endif
