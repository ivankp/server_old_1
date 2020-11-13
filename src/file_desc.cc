#include "file_desc.hh"
#include <unistd.h>
#include "error.hh"

namespace ivanp {

size_t file_desc::read(char* buffer, size_t size) const {
  const auto n = ::read(fd, buffer, size);
  if (n < 0) ERROR("read(fd=",fd,"): ", std::strerror(errno));
  return n;
}

void file_desc::write(std::string_view s) {
  if (::write(fd, s.data(), s.size()) < 0)
    ERROR("write(fd=",fd,"): ", std::strerror(errno));
}

void file_desc::close() {
  if (::close(fd) != 0) ERROR("close(fd=",fd,"): ", std::strerror(errno));
}

} // end namespace ivanp
