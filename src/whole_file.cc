#include "whole_file.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "error.hh"

std::string whole_file(const char* filename) {
  struct stat sb;
  const int fd = ::open(filename, O_RDONLY);
  if (fd == -1) THROW_ERRNO("open(",filename,')');
  if (::fstat(fd, &sb) == -1) THROW_ERRNO("fstat()");
  if (!S_ISREG(sb.st_mode)) THROW_ERRNO("not a file");
  std::string m(sb.st_size,'\0');
  if (::read(fd,m.data(),m.size()) == -1) THROW_ERRNO("read()");
  return m;
}
