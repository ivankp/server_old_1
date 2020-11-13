#ifndef IVANP_BASE64_HH
#define IVANP_BASE64_HH

#include <cstdint>
#include <vector>
#include <string>

std::string base64_encode(const char* ptr, size_t len) noexcept;
std::vector<char> base64_decode(const char* ptr, size_t len) noexcept;

#endif
