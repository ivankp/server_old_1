#include "socket.hh"

#include <unistd.h>
#include <thread>

#include "error.hh"

namespace ivanp {

size_t socket::read(char* buffer, size_t size) const {
  size_t nread = 0;
  for (;;) {
    const auto ret = ::read(fd, buffer, size);
    if (ret < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::yield();
        continue;
      } else THROW_ERRNO("read()");
    } else return nread += ret;
  }
  return nread;
}

void socket::write(const char* data, size_t size) const {
  while (size) {
    const auto ret = ::write(fd, data, size);
    if (ret < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        std::this_thread::yield();
        continue;
      } else THROW_ERRNO("write()");
    }
    data += ret;
    size -= ret;
  }
}

void socket::close() const noexcept {
  ::close(fd);
}

} // end namespace ivanp
