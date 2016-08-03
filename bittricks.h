#include <inttypes.h>

uint64_t reverse(const uint64_t n) {
  const uint64_t size = 64;
  uint64_t r = 0, i = 0;

  for (; i < size; ++i) {
    r |= ((n >> i) & 1) << (size - i - 1);
  }

  return r;
}

constexpr uint64_t reverse(const uint64_t n, const uint64_t size) {
  uint64_t r = 0, i = 0;

  for (; i < size; ++i) {
    r |= ((n >> i) & 1) << (size - i - 1);
  }

  return r;
}
