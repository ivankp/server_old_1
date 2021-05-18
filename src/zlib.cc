#include "zlib.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#define ZLIB_CONST
#include <zlib.h>

#include <algorithm>
#include <bit>

#include "scope_guard.hh"
#include "error.hh"
// #include "debug.hh"

namespace zlib {

const char* zerrmsg(int code) noexcept {
  switch (code) {
    case Z_STREAM_END   : return "Z_STREAM_END";
    case Z_NEED_DICT    : return "Z_NEED_DICT";
    case Z_ERRNO        : return "Z_ERRNO";
    case Z_STREAM_ERROR : return "Z_STREAM_ERROR";
    case Z_DATA_ERROR   : return "Z_DATA_ERROR";
    case Z_MEM_ERROR    : return "Z_MEM_ERROR";
    case Z_BUF_ERROR    : return "Z_BUF_ERROR";
    case Z_VERSION_ERROR: return "Z_VERSION_ERROR";
    default             : return "Z_OK";
  }
}

void deflate_alloc(
  const char* in, size_t in_size,
  char*& out, size_t& out_size,
  bool gz
) {
  const size_t chunk = std::min(std::bit_ceil(in_size/2),(size_t)1<<20);

  z_stream zs;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  zs.opaque = Z_NULL;

  int ret;
  if ((ret = ::deflateInit2(
    &zs,
    Z_BEST_COMPRESSION,
    Z_DEFLATED,
    gz ? 15|16 : 15,
    8,
    Z_DEFAULT_STRATEGY
  )) != Z_OK) ERROR("deflateInit2(): ",zerrmsg(ret));

  scope_guard deflate_end([&]{ (void)::deflateEnd(&zs); });

  zs.next_in = reinterpret_cast<const unsigned char*>(in);
  zs.avail_in = in_size;

  size_t used = 0;
  do {
    used += chunk;
    if (out_size < used)
      out = reinterpret_cast<char*>(realloc(out,used));

    zs.next_out = reinterpret_cast<unsigned char*>(out + (used - chunk));
    zs.avail_out = chunk;
    // while ((ret = ::deflate(&zs, Z_FINISH)) == Z_OK || ret == Z_BUF_ERROR) { }
    // if (ret != Z_STREAM_END) ERROR("deflate(): ",zerrmsg(ret));
    ::deflate(&zs, Z_FINISH);
  } while (zs.avail_out == 0);
  if (zs.avail_in != 0) {
    free(out);
    out = nullptr;
    ERROR("uncompressed data remains");
  }

  out = reinterpret_cast<char*>(realloc(out,(out_size = used - zs.avail_out)));
}

} // end namespace zlib
