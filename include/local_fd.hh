#ifndef IVANP_LOCAL_FD_HH
#define IVANP_LOCAL_FD_HH

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

#include <stdexcept>
#include "string.hh"

namespace ivanp {

struct local_fd {
  int fd;
  local_fd(const char* name)
  : fd(::open([](const char* name){
      if (name && *name) return name;
      throw std::runtime_error("empty file name");
    }(name), O_RDONLY))
  {
    using ivanp::cat;
    if (fd < 0) throw std::runtime_error(cat(
      "open(",name,"): ",std::strerror(errno)));
  }
  ~local_fd() { ::close(fd); }
  operator int() const noexcept { return fd; }
};

}

#endif
