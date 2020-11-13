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

namespace detail::cat {

template <typename T>
[[ gnu::always_inline ]]
inline size_t size(T& x) noexcept {
  if constexpr (std::is_same_v<T,char>)
    return 1;
  else
    return x.size();
}

}

template <typename... T>
[[ gnu::always_inline ]]
inline std::string cat(T&&... x) {
  if constexpr ((... && (
    std::is_convertible_v<T,std::string_view> ||
    std::is_same_v<std::decay_t<T>,char>
  ))) {
    return [](auto... x){
      std::string s;
      s.reserve((... + detail::cat::size(x)));
      (s += ... += x);
      return s;
    }(std::conditional_t<
        std::is_convertible_v<T,std::string_view>,
        std::string_view, T
      >(x) ...
    );
  } else {
    std::stringstream s;
    (s << ... << std::forward<T>(x));
    return std::move(s).str();
  }
}

// ------------------------------------------------------------------

struct chars_less {
  using is_transparent = void;
  bool operator()(const char* a, const char* b) const noexcept {
    return strcmp(a,b) < 0;
  }
  template <typename T>
  bool operator()(const T& a, const char* b) const noexcept {
    return strncmp(a.data(),b,a.size()) < 0;
  }
  template <typename T>
  bool operator()(const char* a, const T& b) const noexcept {
    return strncmp(a,b.data(),b.size()) < 0;
  }
};

inline bool starts_with(const char* str, const char* prefix) noexcept {
  for (;; ++str, ++prefix) {
    if (!*prefix) break;
    if (*str != *prefix) return false;
  }
  return true;
}

inline bool ends_with(const char* str, const char* suffix) noexcept {
  const auto n1 = strlen(str);
  const auto n2 = strlen(suffix);
  if (n1<n2) return false;
  return starts_with(str+(n1-n2),suffix);
}

// ------------------------------------------------------------------

inline const char* cstr(const char* x) noexcept { return x; }
inline const char* cstr(const std::string& x) noexcept { return x.c_str(); }
inline const char* cstr(std::string_view x) noexcept { return x.data(); }

} // end namespace ivanp

#endif
