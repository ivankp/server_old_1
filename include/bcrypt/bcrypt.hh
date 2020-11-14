#ifndef IVANP_BCRYPT_HH
#define IVANP_BCRYPT_HH

#include <string>
#include <stdexcept>

extern "C" {
char* crypt_rn(const char* key, const char* setting, char* data, int size);
char* crypt_gensalt_rn(
  const char* prefix, unsigned long count, const char* input, int size,
  char* output, int output_size);
}

std::string bcrypt_hash(
  const char* pw, const char* rand, int nrand /*16*/, int work = 12
) {
  char salt[64];
  char hash[64];
  if (work < 4 || 31 < work) work = 12;
  if (!crypt_gensalt_rn("$2a$", work, rand, nrand, salt, sizeof(salt)))
    throw std::runtime_error("bcrypt failed to salt");
  if (!crypt_rn(pw, salt, hash, sizeof(hash)))
    throw std::runtime_error("bcrypt failed to hash");
  return { hash, sizeof(hash) };
}

bool bcrypt_check(const char* pw, const char* hash0) {
  char hash[64];
  if (!crypt_rn(pw, hash0, hash, sizeof(hash)))
    throw std::runtime_error("bcrypt failed to hash");

  const unsigned char
    *u1 = reinterpret_cast<const unsigned char*>(hash0),
    *u2 = reinterpret_cast<const unsigned char*>(hash);

  // constant time string comparison
  int cmp = 0;
  for (int i=0; i<64; ++i)
    cmp |= (u1[i] ^ u2[i]);

  return !cmp;
}

#endif