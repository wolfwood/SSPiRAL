// binomial coefficient utils

#pragma once

void initCoeffs();
int binomialCoeff(int n, int k);

// XXX: some functions access the array directly instead of using binomialCoeff(...)
extern int *coeffs[M + 1];
