#ifndef IVANP_BCRYPT_HH
#define IVANP_BCRYPT_HH

#include <cstring>
#include <stdexcept>
#include "bcrypt/bcrypt_common.hh"

extern "C" {
char* crypt_rn(const char* key, const char* setting, char* data, int size);
char* crypt_gensalt_rn(
  const char* prefix, unsigned long count, const char* input, int size,
  char* output, int output_size);
}

void bcrypt_hash(
  char* m, const char* pw, const char* rand, int nrand /*16*/, int work = 10
) {
  char salt[64];
  char hash[64];
  if (work < 4 || 31 < work) work = 10;
  if (!crypt_gensalt_rn("$2a$", work, rand, nrand, salt, sizeof(salt)))
    throw std::runtime_error("bcrypt failed to salt");
  if (!crypt_rn(pw, salt, hash, sizeof(hash)))
    throw std::runtime_error("bcrypt failed to hash");
  ::memcpy(m,hash,bcrypt_hash_len);
}

bool bcrypt_check(const char* pw, const char* hash0) {
  char hash[64];
  if (!crypt_rn(pw, hash0, hash, sizeof(hash)))
    throw std::runtime_error("bcrypt failed to hash");

  // constant time string comparison
  int cmp = 0;
  for (unsigned i=0; i<bcrypt_hash_len; ++i)
    cmp |= ( reinterpret_cast<const unsigned char*>(hash0)[i] ^
             reinterpret_cast<const unsigned char*>(hash )[i] );

  return !cmp;
}

#endif
