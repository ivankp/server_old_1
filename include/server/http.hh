#ifndef IVANP_HTTP_HH
#define IVANP_HTTP_HH

#include <map>
#include <iterator>
#include <array>

#include "socket.hh"
#include "error.hh"

namespace ivanp::http {

extern const std::map<int,const char*> status_codes;
const char* mimes(const char*) noexcept;

class error: public ivanp::error {
  int code;
public:
  template <typename... T> [[ gnu::always_inline ]]
  error(int code, T&&... x)
  : ivanp::error(std::forward<T>(x)...), code(code) { }

  friend socket operator<<(socket fd, const error& e) {
    return fd << status_codes.at(e.code);
  }
};

#define HTTP_ERROR(code,...) \
  throw ivanp::http::error(code, IVANP_ERROR_PREF, __VA_ARGS__);

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
      HTTP_ERROR(400,"form_data missing key \"",key,"\"");
    }
  }

  auto begin() const { return params.begin(); }
  auto end  () const { return params.end  (); }
};

struct request {
  const char *method { }, *path { }, *protocol { };
  std::multimap<const char*,const char*,chars_less> header;
  std::string_view data;

  inline static size_t own_buffer_max_size = 1 << 20;

private:
  char* own_buffer = nullptr;

public:
  request(const socket, char* buffer, size_t size);

  request(const request&) = delete;
  request& operator=(const request&) = delete;

  request(request&& o) noexcept = default;
  request& operator=(request&& o) noexcept = default;

  ~request() { free(own_buffer); }

  operator bool() const noexcept { return method; }

  form_data get_params() const noexcept { return { path }; }

  auto operator[](const auto& x) const { return header.equal_range(x); }

  float qvalue(const char* field, std::string_view value) const;

  // requires form field names and values to be
  // prefixed by 16 bit length (big endian) and null-terminated
  void post_form_data(auto&& f) const {
    const char* p = data.data();
    const char* const data_end = p + data.size();
    unsigned len;
    for (;;) {
      if (p == data_end) break;
      len = 0;
      len += *reinterpret_cast<const uint8_t*>(p++);
      len <<= 8;
      len += *reinterpret_cast<const uint8_t*>(p++);
      const char* end = p + len;
      if (!(end < data_end && *end == '\0'))
        HTTP_ERROR(400,"invalid POST form data");
      f(std::string_view(p,len));
      p = end + 1;
    }
  }
  auto post_form_data(const auto&... field) const
  requires requires (std::string_view s) { ((s==field) && ...); }
  {
    std::array<std::string_view,sizeof...(field)> values { };
    post_form_data([&,i=-1](std::string_view s) mutable {
      if (i==-1) {
        if (!( (++i, s==field) || ... ))
          HTTP_ERROR(400,"unexpected POST form field");
      } else { values[i] = s; i = -1; }
    });
    return values;
  }
};

std::string header(
  std::string_view mime, size_t len, std::string_view more={}
);

void send_file(
  socket, const char* name, bool gz=false
);

void send_str(
  socket, std::string_view str, std::string_view mime, bool gz=false
);

} // end namespace http

#endif
