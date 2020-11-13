#include "websocket.hh"

#include <tuple>

#include <netinet/in.h>
#include <openssl/sha.h>

#include "base64.hh"
#include "debug.hh"

namespace {

constexpr union {
  unsigned char u;
  char c;
} none_set_byte = { .u = (unsigned char)0u };
constexpr union {
  unsigned char u;
  char c;
} all_set_byte = { .u = (unsigned char)~0u };
constexpr bool none_set(char c) noexcept { return c == none_set_byte.c; }
constexpr bool  all_set(char c) noexcept { return c ==  all_set_byte.c; }

template <typename T>
void buffread(char*& buff, T& x) {
  ::memcpy(&x,buff,sizeof(T));
  buff += sizeof(T);
}

}

namespace ivanp::websocket {

void handshake(file_desc& sock, const http::request& req) {
  auto check_header = [
    &req
  ](std::string_view name, const auto&... x) {
    const auto key_vals = req[name];
    const auto n = key_vals.size();
    if (n==0)
      HTTP_ERROR(400,"websocket handshake: missing ",name);
    if constexpr (sizeof...(x) == 0) {
      if (n>1) HTTP_ERROR(400,"multiple values for ",name);
      return std::string_view(*key_vals);
    } else {
      for (const char* a : key_vals) {
        for (const char *b = a; ; ++b) {
          if (*b==',' || *b=='\0') {
            const std::string_view val(a,b-a);
            if (((val==x) || ...)) return;
            if (*b=='\0') break;
            while (*++b==' ');
            a = b;
          }
        }
      }
      HTTP_ERROR(400,"websocket handshake: ",name," != ",x...);
    }
  };

  check_header("Upgrade","websocket");
  check_header("Connection","Upgrade");
  check_header("Sec-WebSocket-Version");
  const auto origin = check_header("Origin");
  const auto protocol = check_header("Sec-WebSocket-Protocol");
  const auto key = check_header("Sec-WebSocket-Key");
  TEST(origin)
  TEST(protocol)

  auto key2 = cat(key,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
  char hash[SHA_DIGEST_LENGTH];
  SHA1(
    reinterpret_cast<const unsigned char*>(key2.data()),
    key2.size(),
    reinterpret_cast<unsigned char*>(&hash)
  );
  key2 = base64_encode(hash,SHA_DIGEST_LENGTH);
  // TEST(key2)
  sock << cat(
    "HTTP/1.1 101 Switching Protocols\r\n"
    "Connection: Upgrade\r\n"
    "Upgrade: websocket\r\n"
    "Sec-WebSocket-Accept: ",key2,"\r\n"
    "Sec-WebSocket-Protocol: ",protocol,"\r\n\r\n"
  );

  INFO("35;1","New websocket ",*sock);
}

uint16_t frame::code() const noexcept {
  uint16_t code = 0;
  if (size() >= 2) {
    ::memcpy(&code,data(),2);
    code = ntohs(code);
  }
  return code;
}

frame receive_frame(file_desc& sock, char* buffer, size_t size) {
  const auto nread = sock.read(buffer,size);
  if (nread==0) ERROR("empty ws frame");
  auto frame = parse_frame(buffer,size);

  switch (frame.opcode) {
    case head::text:
    case head::bin:
      break; // proceed normally
    case head::close: {
      INFO("35;1","closing ws ",*sock,", code: ",frame.code());
      // TODO: send response
      sock.close();
      frame.payload = { };
    }; break;
    case head::ping: {
      INFO("35;1","ping from ",*sock);
      send_frame(sock,buffer,size,{},head::pong); // reply with pong
      frame.payload = { };
    }; break;
    case head::pong: {
      INFO("35;1","pong from ",*sock);
      send_frame(sock,buffer,size,{},head::ping); // reply with ping
      frame.payload = { };
    }; break;
  }

  return frame;
}

frame parse_frame(char* buff, size_t bufflen) {
  //    0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-------+-+-------------+-------------------------------+
  // |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
  // |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
  // |N|V|V|V|       |S|             |   (if payload len==126/127)   |
  // | |1|2|3|       |K|             |                               |
  // +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
  // |     Extended payload length continued, if payload len == 127  |
  // + - - - - - - - - - - - - - - - +-------------------------------+
  // |                               |Masking-key, if MASK set to 1  |
  // +-------------------------------+-------------------------------+
  // | Masking-key (continued)       |          Payload Data         |
  // +-------------------------------- - - - - - - - - - - - - - - - +
  // :                     Payload Data continued ...                :
  // + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
  // |                     Payload Data continued ...                |
  // +---------------------------------------------------------------+

  head head;
  static_assert(sizeof(head)==2);
  uint64_t len;

  buffread(buff,head);
  TEST(unsigned(head.len))

  if (head.fin == 0) ERROR("fin==0 not yet implemented");
  if (head.rsv != 0) ERROR("rsv!=0 not implemented");

  if (head.len<126) {
    len = head.len;
  } else if (head.len<127) {
    union {
      uint16_t tmp;
      uint8_t bytes[2];
    };
    buffread(buff,tmp);
    std::swap(bytes[0],bytes[1]);
    len = tmp;
  } else {
    union {
      uint64_t tmp;
      uint8_t bytes[8];
    };
    buffread(buff,tmp);
    for (int i=4; i; ) --i, std::swap(bytes[i],bytes[7-i]);
    len = tmp;
  }
  TEST(len)

  if (len > bufflen) ERROR("frame length exceeds buffer length");

  if (len) {
    if (head.mask) {
      char mask[4];
      buffread(buff,mask);

      int nset = 0;
      for (int i=0; i<4; ++i) {
        if (none_set(mask[i])) {
        } else if (all_set(mask[i])) {
          ++nset;
        } else {
          nset = -1;
          break;
        }
      }
      if (nset==0 || nset==4)
        ERROR((nset ? "all" : "no")," mask bits are set");

      for (uint64_t i=0; i<len; ++i)
        buff[i] ^= mask[i%4];
    } else ERROR("client sent no mask");
  }

  return { head, { buff, len } };
}

void send_frame(
  file_desc& sock, char* buffer, size_t size,
  std::string_view message, head::type opcode
) {
  //    0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-------+-+-------------+-------------------------------+
  // |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
  // |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
  // |N|V|V|V|       |S|             |   (if payload len==126/127)   |
  // | |1|2|3|       |K|             |                               |
  // +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
  // |     Extended payload length continued, if payload len == 127  |
  // + - - - - - - - - - - - - - - - +-------------------------------+
  // |                               |          Payload Data         |
  // +-------------------------------- - - - - - - - - - - - - - - - +
  // :                     Payload Data continued ...                :
  // + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
  // |                     Payload Data continued ...                |
  // +---------------------------------------------------------------+

  if (message.size()+10 > size) ERROR(
    "The message is too long for buffer of size ", size);
  ::memcpy(buffer += 10, message.data(), size = message.size());

  head head { .opcode = opcode };
  if (size<126) {
    head.len = size;
  } else if (size <= (uint16_t)-1) {
    head.len = 126;
    uint16_t size2 = size;
    ::memcpy(buffer-=2,&size2,2);
    size += 2;
  } else {
    head.len = 127;
    ::memcpy(buffer-=8,&size,8);
    size += 8;
  }
  ::memcpy(buffer-=2,&head,2);
  sock.write({buffer,size+2});
}

}
