#ifndef IVANP_FILE_DESC_HH
#define IVANP_FILE_DESC_HH

#include <string_view>
#include <utility>

namespace ivanp {

class file_desc {
protected:
  int fd;

public:
  file_desc() noexcept: fd(-1) { }
  file_desc(int fd) noexcept: fd(fd) { }
  file_desc(file_desc&& o) noexcept: fd(o.fd) { o.fd = -1; }
  file_desc& operator=(file_desc&& o) noexcept {
    std::swap(fd,o.fd);
    return *this;
  }
  file_desc(const file_desc&) = default;
  file_desc& operator=(const file_desc&) = default;

  bool operator==(file_desc o) const noexcept { return fd == o.fd; }
  bool operator!=(file_desc o) const noexcept { return fd != o.fd; }

  int operator*() const noexcept { return fd; }
  bool is_valid() const noexcept { return fd >= 0; }

  void write(std::string_view);
  void operator<<(std::string_view buffer) { write(buffer); }

  size_t read(char* buffer, size_t size) const;
  template <typename T>
  size_t read(T& buffer) const { return read(buffer.data(),buffer.size()); }

  void close();
};

class uniq_file_desc: public file_desc {
public:
  using file_desc::file_desc;
  ~uniq_file_desc() { if (is_valid()) close(); }

  uniq_file_desc(uniq_file_desc&&) = default;
  uniq_file_desc& operator=(uniq_file_desc&&) = default;
  uniq_file_desc(const uniq_file_desc&) = delete;
  uniq_file_desc& operator=(const uniq_file_desc&) = delete;
};

} // end namespace ivanp

#endif
