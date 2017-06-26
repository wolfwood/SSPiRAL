#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

uint binomialCoeff(uint64_t n, uint64_t k) {
  if ( n < k) {
    return 0;
  }

  int res = 1;

  // Since C(n, k) = C(n, n-k)
  if ( k > n - k ) {
    k = n - k;
  }

  // Calculate value of [n * (n-1) *---* (n-k+1)] / [k * (k-1) *----* 1]
  for (uint64_t i = 0; i < k; ++i) {
    res *= (n - i);
    res /= (i + 1);
  }

  return res;
}

int main (int argc, char** argv) {
  assert(argc == 3);

  uint64_t n = atoll(argv[1]);
  uint64_t k = atoll(argv[2]);

  printf("%u\n", binomialCoeff(n, k));

  return 0;
}
