#include "lex_str_sort.hh"
#include <charconv>

namespace ivanp {
namespace {

[[gnu::const]]
char to_upper(char c) noexcept {
  return (c < 'a' || 'z' < c) ? c : c-(char)32;
}

enum : char { NUMB, LETT, SYMB, CTRL, EXTD };

[[gnu::const]]
char char_cat(char c) noexcept {
  if (c <   '\0') return EXTD;
  if (c <    ' ') return CTRL;
  if (c <    '0') return SYMB;
  if (c <    ':') return NUMB;
  if (c <    'A') return SYMB;
  if (c <    '[') return LETT;
  if (c < '\x7F') return SYMB;
  return CTRL;
}

}

bool lex_str_less(std::string_view a, std::string_view b) noexcept {
  const char* s1 = a.data();
  const char* s2 = b.data();
  const char* const z1 = s1 + a.size();
  const char* const z2 = s2 + b.size();

  for (char c1, c2; s1!=z1 && s2!=z2; ) {
    c1 = to_upper(*s1);
    c2 = to_upper(*s2);

    const char t1 = char_cat(c1);
    const char t2 = char_cat(c2);
    if (t1 != t2) return t1 < t2;

    if (t1 == NUMB) {
      unsigned u1=0, u2=0;
      s1 = std::from_chars(s1,z1,u1).ptr;
      s2 = std::from_chars(s2,z2,u2).ptr;
      if (u1 != u2) return u1 < u2;
      continue;
    } else if (c1 != c2) return c1 < c2;

    ++s1, ++s2;
  }

  return (z1-s1) < (z2-s2);
}

}
