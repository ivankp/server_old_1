#ifndef IVANP_ZLIB_HH
#define IVANP_ZLIB_HH

#include <cstddef>

namespace zlib {

void deflate_alloc(
  const char* in, size_t in_size,
  char*& out, size_t& out_size,
  bool gz = true
);

}

#endif
