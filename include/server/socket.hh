#ifndef IVANP_SOCKET_HH
#define IVANP_SOCKET_HH

#include <string_view>
#include <utility>

namespace ivanp {

class socket {
protected:
  int fd = -1;

public:
  socket() noexcept = default;
  socket(int fd) noexcept: fd(fd) { }
  socket& operator=(int fd) noexcept {
    this->fd = fd;
    return *this;
  }
  socket(socket&& o) noexcept: fd(o.fd) { o.fd = -1; }
  socket& operator=(socket&& o) noexcept {
    std::swap(fd,o.fd);
    return *this;
  }
  socket(const socket&) noexcept = default;
  socket& operator=(const socket&) noexcept = default;

  bool operator==(int o) const noexcept { return fd == o; }
  bool operator!=(int o) const noexcept { return fd != o; }

  operator int() const noexcept { return fd; }

  void write(const char* data, size_t size) const;
  void write(std::string_view s) const { write(s.data(),s.size()); }
  socket operator<<(std::string_view buffer) const {
    write(buffer);
    return *this;
  }

  size_t read(char* buffer, size_t size) const;
  template <typename T>
  size_t read(T& buffer) const { return read(buffer.data(),buffer.size()); }

  void close() const noexcept;
};

struct uniq_socket: socket {
  using socket::socket;
  ~uniq_socket() { close(); }

  uniq_socket(uniq_socket&&) noexcept = default;
  uniq_socket& operator=(uniq_socket&&) noexcept = default;
  uniq_socket(const uniq_socket&) = delete;
  uniq_socket& operator=(const uniq_socket&) = delete;
};

} // end namespace ivanp

#endif
