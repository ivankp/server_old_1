#ifndef IVANP_WEBSOCKET_HH
#define IVANP_WEBSOCKET_HH

#include "http.hh"

namespace ivanp::websocket {

struct head {
  using type = unsigned char;

  type opcode: 4 = 0;
  type rsv   : 3 = 0;
  type fin   : 1 = 1;
  type len   : 7 = 0;
  type mask  : 1 = 0;

  enum : type {
    cont='\x0', text='\x1', bin='\x2', close='\x8', ping='\x9', pong='\xA'
  };
};

struct frame: head {
  std::string_view payload;

  uint16_t code() const noexcept;

  auto* operator->() const noexcept { return &payload; }
  auto  operator* () const noexcept { return  payload; }

  auto data() const noexcept { return payload.data(); }
  auto size() const noexcept { return payload.size(); }
  operator std::string_view() const noexcept { return payload; }

  template <typename T>
  friend decltype(auto) operator<<(T& o, const frame& f) noexcept {
    return o << f.payload;
  }

  bool control(file_desc&);
};

void handshake(file_desc&, const http::request& req);
frame parse_frame(char* buff, size_t size);
frame receive_frame(file_desc&, char* buff, size_t size);
void send_frame(
  file_desc& sock, char* buffer, size_t size,
  std::string_view message, head::type opcode = head::text
);

}

#endif
