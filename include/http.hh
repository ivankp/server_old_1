#ifndef IVANP_HTTP_HH
#define IVANP_HTTP_HH

#include <map>
#include <iterator>

#include "file_desc.hh"
#include "error.hh"

namespace ivanp::http {

extern const std::map<const char*,const char*,chars_less> mimes;
extern const std::map<int,const char*> status_codes;

class error: public ivanp::error {
  int code;
public:
  template <typename... T>
  [[ gnu::always_inline ]]
  error(int code, T&&... x)
  : ivanp::error(std::forward<T>(x)...), code(code) { }
  void respond(file_desc&) const;
};

#define HTTP_ERROR(code,...) \
  throw ::ivanp::http::error(code, IVANP_ERROR_PREF, __VA_ARGS__);

class form_data {
  std::string mem;
public:
  std::map<const char*,const char*,chars_less> params;

  form_data(std::string_view str, bool q=true) noexcept;

  // can't copy or move because of small string optimization
  form_data(const form_data&) = delete;
  form_data& operator=(const form_data&) = delete;

  form_data(form_data&&) = delete;
  form_data& operator=(form_data&&) = delete;

  const char* path() const noexcept { return mem.data(); }

  template <typename K>
  const char* operator[](const K& key) const {
    try {
      return params.at(key);
    } catch (const std::out_of_range&) {
      ERROR("form_data missing key \"",key,"\"");
    }
  }
};

struct request {
  const char *method { }, *path { }, *protocol { };
  std::multimap<const char*,const char*,chars_less> header;
  std::string_view data;

private:
  class buffer {
    char* p { };
    buffer(char* p) noexcept: p(p) { }
  public:
    ~buffer() { delete[] p; }
    buffer() noexcept = default;

    buffer(const buffer&) = delete;
    buffer& operator=(const buffer&) = delete;

    buffer(buffer&& o) noexcept { std::swap(p,o.p); }
    buffer& operator=(buffer&& o) noexcept { std::swap(p,o.p); return *this; }

    char* operator()(size_t size) noexcept {
      *this = { new char[size] };
      return p;
    }
  };
  buffer mem { };

public:
  request(const file_desc&, char* buffer, size_t size, size_t max_size=0);

  request(const request&) = delete;
  request& operator=(const request&) = delete;

  request(request&& o) noexcept = default;
  request& operator=(request&& o) noexcept = default;

  form_data get_params() const noexcept { return { path }; }

  class range {
  public:
    struct iterator {
      decltype(header)::const_iterator it;
      auto& operator* () const noexcept { return it->second; }
      auto& operator->() const noexcept { return it; }
      auto& operator++() noexcept(noexcept(++it)) { ++it; return *this; }
      auto& operator--() noexcept(noexcept(--it)) { --it; return *this; }
      bool  operator==(const iterator& r) const noexcept { return it == r.it; }
      bool  operator!=(const iterator& r) const noexcept { return it != r.it; }
    };
  private:
    iterator a, b;
  public:
    template <typename T>
    range(T&& x)
    : a{std::get<0>(std::forward<T>(x))},
      b{std::get<1>(std::forward<T>(x))} { }
    auto begin() const noexcept { return a; }
    auto end  () const noexcept { return b; }
    auto size () const noexcept { return std::distance(a.it,b.it); }
    decltype(auto) operator*() const noexcept { return *a; }
  };

  template <typename K>
  range operator[](const K& x) const { return header.equal_range(x); }
};

std::string header(
  std::string_view mime, size_t len, std::string_view more={}
);

void send_file(
  file_desc& fd, std::string in_name,
  std::string_view mime={}, std::string_view more={}
);

}

#endif
