#include <inttypes.h>
#include <stdio.h>

uint64_t reverse(const uint64_t n)
{
  const uint64_t size = 64;
  uint64_t r, i;
  for (r = 0, i = 0; i < size; ++i)
    r |= ((n >> i) & 1) << (size - i - 1);
  return r;
}

int main()
{
        uint64_t sum = 0;
        uint64_t a;
        for (a = 0; a < (uint64_t)1 << 30; ++a)
                sum += reverse(a);
        printf("%" PRIu64 "\n", sum);
        return 0;
}
