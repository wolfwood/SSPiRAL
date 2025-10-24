// supply binomial coefficients from a lookup table

#include "types.h"
#include "binomial_coeff.h"

int *coeffs[M + 1];

// over-allocates, and zeros [n][n+1] to allow us to skip the (n < k) check below
void initCoeffs() {
  coeffs[0] = calloc(2, sizeof(int));

  coeffs[0][0] = 1;

  for (int i = 1; i <= M; ++i) {
    int limit = i + 1;
    coeffs[i] = calloc(limit + 1, sizeof(int));

    coeffs[i][0] = 1;
    coeffs[i][limit - 1] = 1;

    for (int j = 1; j < limit - 1; ++j) {
      coeffs[i][j] = coeffs[i - 1][j] + coeffs[i - 1][j - 1];
    }
  }
}

inline int binomialCoeff(const int n, const int k) {
  //if ( n < k) {
  //  return 0;
  //}

  //if ( n - k < k) {
  //  k = n - k;
  //}

  return coeffs[n][k];
}
