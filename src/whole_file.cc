#include "whole_file.hh"
#include "local_fd.hh"

std::string whole_file(const char* name) {
  using ivanp::cat;
  ivanp::local_fd fd(name);
  struct stat sb;
  if (::fstat(fd,&sb) < 0)
    throw std::runtime_error(cat("fstat(",name,"): ",std::strerror(errno)));
  if (!S_ISREG(sb.st_mode))
    throw std::runtime_error(cat("\"",name,"\" is not a regular file"));
  std::string m(sb.st_size,'\0');
  if (::read(fd,m.data(),m.size()) < 0)
    throw std::runtime_error(cat("read(",name,"): ",std::strerror(errno)));
  return m;
}
