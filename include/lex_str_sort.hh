#ifndef IVANP_LEX_STR_SORT_HH
#define IVANP_LEX_STR_SORT_HH

#include <string>
#include <string_view>
#include <algorithm>

namespace ivanp {

bool lex_str_less(std::string_view a, std::string_view b) noexcept;

template <typename A, typename B>
void lex_str_sort(A first, B last) {
  std::sort(first,last,lex_str_less);
}

template <typename T>
void lex_str_sort(T&& xs) {
  lex_str_sort(
    std::begin(std::forward<T>(xs)),
    std::end(std::forward<T>(xs))
  );
}

}

#endif
