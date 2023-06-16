#include <stdio.h>
#include <stdlib.h>

// O_APPEND
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// mmap
#include <sys/mman.h>

#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)
// types
#include <stdint.h>
#include <stdbool.h>
// memset
#include <string.h>
// assert
#include <assert.h>

// unlink
#include <unistd.h>

#include <search.h>

/* types */
typedef uint8_t node_t;
typedef uint32_t layout_t;  // needs >= M bits
typedef uint32_t score_t;   // enough bits to hold M choose M/2 ?
typedef uint8_t mlidx_t;

/* vars */
//const node_t N = 4;
#define N (node_t)5
//const node_t M = 15;
#define M (node_t)31

//const node_t SCORE_SIZE = ((M+1) / 2) - N;
#define SCORE_SIZE (((M+1) / 2) - N)

#define twoMB (2*1024*1024)
#define oneGB (1024*1024*1024)

// maximum number of unique MetaLayouts (131 for N=5)
#define ML_SIZE (131+1)

/* util */
node_t MfromN(node_t n) { return ((node_t) 1 << n) - 1; }

layout_t node2layout(node_t n) {
  return ((layout_t) 1) << (n - 1);
}

static int *coeffs[M + 1];

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

int binomialCoeff(int n, int k) {
  //if ( n < k) {
  //  return 0;
  //}

  //if ( n - k < k) {
  //  k = n - k;
  //}

  return coeffs[n][k];
}

void *mymap(uint64_t *size) {
#ifdef FILEBACKED
  /*if (*size % twoMB) {
   *size = ((size/twoMB) +1) * twoMB;
   }*/

  char fname[] = "/mnt/media/deletemeXXXXXX";

  int tfd = mkstemp(fname);

  if (-1 == tfd) {
    perror("mkstemp failed: ");
    exit(1);
  }

  int err = unlink(fname);

  if (-1 == err) {
    perror("unlink failed: ");
    exit(1);
  }

  off_t foo = lseek(tfd, *size, SEEK_CUR);

  if (-1 == foo) {
    perror("seek failed: ");
    exit(1);
  }

  foo = write(tfd, " ", 1);

  void* temp = mmap(NULL, *size, PROT_WRITE, MAP_NORESERVE|MAP_SHARED, tfd, 0);
#else
  uint64_t rounding = twoMB;

  if (rounding && *size % rounding) {
    *size = ((*size/rounding) +1) * rounding;
  }

  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;

  if (rounding == oneGB) {
    mmap_flags |= MAP_HUGETLB|MAP_HUGE_1GB;
  } else if (rounding == twoMB) {
    mmap_flags |= MAP_HUGETLB|MAP_HUGE_2MB;
  }

  // XXX - adjust length for mummap to page alignment
  void *temp = mmap(NULL, *size, PROT_WRITE, mmap_flags, -1, 0);
#endif

  if (MAP_FAILED == temp) {
    perror("mmap failed: ");
    exit(1);
  }

  return temp;
}

void myunmap(void *ptr, uint64_t size) {
  int err = munmap(ptr, size);
  if (0 != err) {
    perror("unmap failed: ");
    exit(1);
  }
}

// XXX: depth should ever be more than N, see NOTES
bool deadnessCheck(const node_t *Is, const int len) {
  int j = len;

  //  for(node_t n = 1 << (N-1); n > 0; n >>= 1) {
  for (node_t n = 1; n < M; n <<= 1) {
    while (j > 1 && (Is[j] + 1) < n) {
      --j;
    }

    if ((Is[j] + 1) != n) {
      node_t temp = n;

      node_t Js[len + 1];
      const node_t SENTINEL = 3;

      for (uint k = 1; k <= len; ++k) {
        Js[k] = SENTINEL;
      }

      // one greater than the highest acceptable value - gets fed into the next element
      Js[0] = 0;

      bool alive = false;
      uint i = 1;

      while (0 < i) {
        if (0 == Js[i]) {
          //temp ^= Is[i];
          Js[i] = SENTINEL;
          --i;
        } else {
          if (SENTINEL == Js[i]) {
            Js[i] = 2;
          }
          temp ^= Is[i] + 1;

          if (0 == temp) {
            // continue outer loop
            alive = true;
            break;
          }

          --Js[i];
          if (len > i) {
            ++i;
          }
        }
      }

      if (!alive) {
        return true;
      }
    }
  }

  return false;
}

bool recurseCheck(layout_t name, node_t n, node_t i) {
  for (; i <= M; ++i) {
    layout_t l = ((layout_t) 1) << (i - 1);

    if (l & name) {
      layout_t temp = i ^n;

      if (temp == 0 || recurseCheck(name, temp, i + 1)) {
        return true;
      }
    }
  }

  return false;
}

bool checkIfAlive(layout_t name) {
  for (node_t n = 1; n <= M; n <<= 1) {
    layout_t l = node2layout(n);

    if ((l & name) == 0) {
      if (!recurseCheck(name, n, 1)) {
        return false;
      }
    }
  }
  return true;
}


// Original algo (lost in the git log) was all about layout bitmasks, and an inefficient but intuitively exhaustive Aliveness algo.
//   more efficient, non recursive algo represents a given layout as an unzipped array of nodes, rather than a space efficient bitmap
/*  Turns out the most efficient iteration strategy is to track the bitmask representation for updating the candidate
 *   layout name, used for the original deadness checking algo and for validation of the revised strategies, separately
 *   from the list of nodes in the layout, which can be used directly for child score summation and deadness checking,
 *   rather than reconstucting the list, used to build the name in the first place, back fron the name again.
 */
void walkOrdered(const uint limit, void(*func)(layout_t name, uint limit, node_t *Is, void *arg), void *arg) {
  uint i = 1;
  layout_t ells[limit + 1], name = 0;
  node_t Is[limit + 1];

  for (uint i = 1; i <= limit; ++i) {
    ells[i] = 0;
  }

  ells[0] = node2layout(M) << 1;
  Is[0] = M;

  while (0 < i) {
    if (0 == ells[i]) {
      ells[i] = ells[i - 1];
      Is[i] = Is[i - 1];
    } else {
      name ^= ells[i];
    }
    ells[i] >>= 1;
    --Is[i];

    name |= ells[i];

    if (limit == i) {
      (*func)(name, limit, Is, arg);
    }

    if (1 == ells[i]) {
      name ^= ells[i];
      ells[i] = 0;
      --i;
    } else {
      if (limit > i) {
        ++i;
      }
    }
  }
}

/*   In this case, doing an extra bitshift is more expensive than the separate 'ells' node-in-layout-representation bitmask array
 */
void walkOrdered2(const uint limit, void(*func)(layout_t name, uint limit, node_t *Is, void *arg), void *arg) {
  uint i = 1;
  layout_t name = 0;
  node_t Is[limit + 1];
  const node_t SENTINEL = 255;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    } else {
      name ^= (layout_t) 1 << Is[i];
    }
    --Is[i];

    name |= (layout_t) 1 << Is[i];

    if (limit == i) {
      (*func)(name, limit, Is, arg);
    }

    if (0 == Is[i]) {
      name ^= (layout_t) 1 << Is[i];
      Is[i] = SENTINEL;
      --i;
    } else {
      if (limit > i) {
        ++i;
      }
    }
  }
}

/*  Stop tracking names completely, iterating nodes as 'nodes' -- integer identifiers -- rather than bitmask 'layout' format.
 *   this saves a bunch of xors on this end, and can avoid representation transformation in the work function, particularly deadness.
 *   the layout position is not named directly, it is simply a loop counter buried in the work function.
 */
void walkOrderedNameless(const uint limit, void(*func)(uint limit, node_t *Is, void *arg), void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    if (limit == i) {
      (*func)(limit, Is, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

void walkOrderedNameless2(const uint limit, void(*func)(uint limit, node_t *Is, void *arg), void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  while (true) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    if (limit == i) {
      (*func)(limit, Is, arg);
    }

    if (0 == Is[i]) {
      --i;
      if(i == 0){break;}
    } else if (limit > i) {
      ++i;
    }
  }
}


// more sensible bounds? single LUT but favor afters, rather than befores
void walkNamelessDeltas1(const uint limit, void(*func)(uint limit, node_t *Is, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  //uint32_t afterCoefs[limit -1];

  uint32_t afters[limit + 1][M + 1];

  uint32_t *as[limit + 1];

  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
    }
    --Is[i];
    --as[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 1; i < (limit); ++i) {
        uint before = *(((uint32_t *) as[i]) + (M + 1));
        uint after = *as[i + 1];

        assert(before > after);
        deltaCoefs[i - 1] = before - after;
        index += after;
      }

      (*func)(limit, Is, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// more sensible bounds? single LUT but favor afters, rather than befores
void walkNamelessDeltas2(const uint limit, void(*func)(uint limit, node_t *Is, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];

  uint32_t afters[limit + 1][M + 1];

  uint32_t *as[limit + 1];

  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (true) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
    }
    --Is[i];
    --as[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 1; i < (limit); ++i) {
        uint before = *(((uint32_t *) as[i]) + (M + 1));
        uint after = *as[i + 1];

        assert(before > after);
        deltaCoefs[i - 1] = before - after;
        index += after;
      }

      (*func)(limit, Is, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
      if(0 == i){break;}
    } else if (limit > i) {
      ++i;
    }
  }
}

// use single LUT
void walkNamelessDeltas3(const uint limit, void(*func)(uint limit, node_t *Is, score_t idx, score_t *deltaCoeffs, void *arg),
                         void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --Is[i];
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (M + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, Is, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

void walkNamelessDeltas4(const uint limit, void(*func)(uint limit, node_t *Is, score_t idx, score_t *deltaCoeffs, void *arg),
                         void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  while (true) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --Is[i];
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (M + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, Is, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
      if(0 == i){break;}
    } else if (limit > i) {
      ++i;
    }
  }
}

// switch sentinels
void walkNamelessDeltas5(const uint limit, void(*func)(uint limit, node_t *Is, score_t idx, score_t *deltaCoeffs, void *arg),
                         void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    bs[i] = NULL;
    for (int j = M; j >= 0; --j) {
      if (SENTINEL == befores[i][j]) {
        bs[i] = &(befores[i][j]);
        break;
      }
    }
    assert(bs[i] != NULL);
  }

  while (true) {
    if (SENTINEL == *bs[i]) {
      Is[i] = Is[i - 1];
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --Is[i];
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (M + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, Is, index, deltaCoefs, arg);
    }

    if (0 == *bs[i]) {
      --i;
      if(0 == i){break;}
    } else if (limit > i) {
      ++i;
    }
  }
}

void walkNamelessDeltas6(const uint limit, void(*func)(uint limit, node_t *Is, score_t idx, score_t *deltaCoeffs, void *arg),
                         void *arg) {
  uint i = limit;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = Is[i - 1] - 1;
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
  }

  while (true) {
    uint index = 0;

    for (int i = 2; i <= (limit); ++i) {
      uint before = *bs[i - 1];
      uint after = *(((uint32_t *) bs[i]) - (M + 1));

      assert(before > after);
      deltaCoefs[i - 2] = before - after;
      index += after;
    }

    (*func)(limit, Is, index, deltaCoefs, arg);

    if (0 == *bs[i]) {
      do{
        --i;
        --Is[i];
        --bs[i];
      } while (0 == *bs[i] && 0 < i);
      if(0 == i){break;}
      do {
        ++i;
        Is[i] = Is[i - 1] - 1;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
      } while (i < limit);
    } else {
      --Is[i];
      --bs[i];
    }
  }
}

void walkNamelessDeltas7(const uint limit, void(*func)(uint limit, node_t *Is, score_t idx, score_t *deltaCoeffs, void *arg),
                         void *arg) {
  uint i = limit;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit - 1];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];


  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }
  befores[limit][0] = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  bs[0] = &befores[0][M];
  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);

  uint index = 0;
  for (uint i = 1; i <= limit; ++i) {
    Is[i] = Is[i - 1] - 1;
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
    if (i > 1){index += after;}
  }

  while (true) {
    (*func)(limit, Is, index, deltaCo, arg);

    if (0 == *bs[i]) {
      index -= *as[i];
      do{
        --i;
        --Is[i];
        --bs[i];
        index -= *as[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if(0 == i){break;}
      if (1 == i) {
        ++i;
        Is[i] = Is[i - 1] - 1;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M);
        as[i] = ((uint32_t *) as[i - 1]) + (M);
        deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
        index = *as[i];
      } else {
        deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
        index += *as[i];
      }
      do {
        ++i;
        Is[i] = Is[i - 1] - 1;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
        index += *as[i];
      } while (i < limit);
    } else {
      --Is[i];
      --bs[i];
      --as[i];
      ++deltaCoefs[i - 1];
      --index;
    }
  }
}

/// XXX switch to befores
void walkNamelessDeltas27(const uint limit, void(*func)(uint limit, node_t *Is, score_t idx, score_t *deltaCoeffs, void *arg),
                         void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];

  uint32_t afters[limit + 1][M + 1];

  uint32_t *as[limit + 1];

  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  afters[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    as[i] = NULL;
    for (int j = M; j >= 0; --j) {
      if (SENTINEL == afters[i][j]) {
        as[i] = &(afters[i][j]);
        break;
      }
    }
    assert(as[i] != NULL);
  }
  /*for (uint i = 1; i <= limit; ++i) {
    as[i] = ((uint32_t *) as[i - 1]) + (M);
  }*/

  while (true) {
    if (SENTINEL == *as[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
    }
    --Is[i];
    --as[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 1; i < (limit); ++i) {
        uint before = *(((uint32_t *) as[i]) + (M + 1));
        uint after = *as[i + 1];

        assert(before > after);
        deltaCoefs[i - 1] = before - after;
        index += after;
      }

      (*func)(limit, Is, index, deltaCoefs, arg);
    }

    if (0 == *as[i]) {
      --i;
      if(0 == i){break;}
    } else if (limit > i) {
      ++i;
    }
  }
}

// unfinished. attempts to cache only the necessary binomial coefficients for the iteration at hand, in the order needed
void walkCombinadically2(const uint limit,
                         void(*func)(uint limit, score_t idx, score_t *beforeCoeffs, score_t *afterCoeffs, void *arg),
                         void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  int co[limit][limit];

  for (layout_t min_node = limit - 1; min_node >= 0; --min_node) {
    for (layout_t node = M - 1; node >= min_node; --node) {
      co[limit - 1 - min_node][M - 1 - node] = coeffs[node][min_node + 1];
    }
  }


  layout_t *beforeCoeffs[limit - 1];
  layout_t *afterCoeffs[limit - 1];
  layout_t idx = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
    afterCoeffs[i - 1] = 0;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;


  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    if (limit == i) {
      //(*func)(limit, idx, beforeCoeffs, afterCoeffs, arg);
    }

    if ((limit - i) >= Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// an attempt to push the before and after array stores down in to loop from the kernel. didn't pan out in this form
void walkCombinadically1(const uint limit,
                         void(*func)(uint limit, score_t idx, score_t *beforeCoeffs, score_t *afterCoeffs, void *arg),
                         void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;


  layout_t beforeCoefs[limit];
  layout_t afterCoefs[limit];
  layout_t idx = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
    afterCoefs[i - 1] = 0;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    //if (1 < i) {
    //idx -= afterCoefs[i - 1];
    afterCoefs[i - 1] = coeffs[Is[i]][limit - i + 1];
    //idx += afterCoefs[i - 1];

    //beforeCoefs[i - 2] = coeffs[Is[i-1]][limit - (i - 1)];

    //if (limit - i <= Is[i]) {
    //assert(beforeCoefs[i - 2] > afterCoefs[i - 2]);
    //}
    //}

    if (limit == i) {
      idx = 0;

      for (uint k = 1; k < limit; ++k) {
        idx += afterCoefs[k];
      }

      (*func)(limit, idx, beforeCoefs, &afterCoefs[1], arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      beforeCoefs[i - 1] = coeffs[Is[i]][limit - i];
      ++i;
    }
  }
}


/*  Terminal Work -- layouts with greater than M/2 nodes which are guaranteed to be alive, do not require deadness checking,
 *   but still dominate the computation.  The primary work in this case is simply looking up the children of a layout, to sum
 *   their reliability scores. Initially finding children was done by masking out bits in the name to compute child names and
 *   hashing or searching the prev array for them. however, it is more space and time efficient to directly compute child positions
 *   using combinadic numbers, which give a predictable ordering.  initially the lambda did this work per-layout but there is reusable
 *   computation between sibling layouts and so pulling that work into the iteration, and simply passing the arrays of component nodes
 *   contribution to the combinadic value of a child, computed for both when they come before the node the chile is missing, or after,
 *   along with the index of the first child, allows us to call sumChildLayoutScoresInner, skipping another representation transformation
 *   and reducing overhead slightly. at this point the state maintained by the iteration has very little resemblence to the initial
 *   representations of a layout.
 */
void walkCombinadically(const uint limit,
                        void(*func)(uint limit, score_t idx, score_t *beforeCoeffs, score_t *afterCoeffs, void *arg),
                        void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    if (limit == i) {
      uint beforeCoefs[limit - 1];
      uint afterCoefs[limit - 1];
      uint index = 0;

      // pre-calculate the combinadic components for each node, for both coming before and after the 'missing' node
      for (int i = 0; i < (limit - 1); ++i) {
        uint before = binomialCoeff(Is[i + 1], limit - (i + 1));
        uint after = binomialCoeff(Is[i + 2], limit - (i + 2) + 1);
        assert(before > after);
        beforeCoefs[i] = before;
        afterCoefs[i] = after;
        index += after;
      }

      (*func)(limit, index, beforeCoefs, afterCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

/*  Rather than passing the before and after and index, so that the sumChildLayoutScoresInner can iterate the children by subtracting
 *   after from the index and adding before for each node (except the first was already removed and the last was already added), it saves
 *   time to simply pass the index and the deltas between befores and afters for a single addition. sumChildLayoutScores can benefit from
 *   this as well. speedup was small, but measurable.
 */
void walkCombinadicDeltas3(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    if (limit == i) {
      uint32_t deltaCoefs[limit - 1];
      uint index = 0;

      // pre-calculate the combinadic components for each node, for both coming before and after the 'missing' node
      for (int i = 0; i < (limit - 1); ++i) {
        uint before = binomialCoeff(Is[i + 1], limit - (i + 1));
        uint after = binomialCoeff(Is[i + 2], limit - (i + 2) + 1);
        assert(before > after);
        deltaCoefs[i] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// pulls index calculatio out of the kernel, but the reversing subtraction is noticably worse than the wasted summation
void walkCombinadicDeltas5(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  uint32_t afterCoefs[limit - 1];
  for (int i = 0; i < (limit - 1); ++i) {
    afterCoefs[i] = 0;
  }
  uint index = 0;

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    if (i > 1) {
      uint before = binomialCoeff(Is[i - 1], limit - (i - 1));
      uint after = binomialCoeff(Is[i], limit - (i) + 1);
      assert(before > after);
      index -= afterCoefs[i - 2];
      afterCoefs[i - 2] = after;
      deltaCoefs[i - 2] = before - after;
      index += after;
    }

    if (limit == i) {
      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

/*  Moves delta calculation out of the kernel, and saves afters for summing up the index
 *   better than Deltas5
 */
void walkCombinadicDeltas4(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  uint32_t afterCoefs[limit - 1];

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    if (i > 1) {
      uint before = binomialCoeff(Is[i - 1], limit - (i - 1));
      uint after = binomialCoeff(Is[i], limit - (i) + 1);
      assert(before > after);
      afterCoefs[i - 2] = after;
      deltaCoefs[i - 2] = before - after;
    }

    if (limit == i) {
      uint index = 0;
      for (int i = 0; i < (limit - 1); ++i) {
        index += afterCoefs[i];
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// tries to remove a branch from Deltas4 -- 4% worse (now its better???)
void walkCombinadicDeltas7(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit];
  uint32_t afterCoefs[limit];

  uint32_t *d = deltaCoefs + 1;

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];


    uint before = binomialCoeff(Is[i - 1], limit - (i - 1));
    uint after = binomialCoeff(Is[i], limit - (i) + 1);
    assert(before > after);
    afterCoefs[i - 1] = after;
    deltaCoefs[i - 1] = before - after;

    if (limit == i) {
      uint index = 0;
      for (int i = 1; i < (limit); ++i) {
        index += afterCoefs[i];
      }

      (*func)(limit, index, d, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

/*  takes Deltas4 and looks up all the binomialCoeffs for befores and afters. its trading one lookup table for another so not
 *   completely sure why this is the best optimization so far. table has more regular access patterns and maybe is smaller?
 *   not completely compact, should be triangular, or rows of limit length
 *  could try making accesses go in ascending order? combine tables?
 */
void walkCombinadicDeltas6(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;
  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  uint32_t afterCoefs[limit - 1];

  uint befores[limit + 1][M + 1];
  uint afters[limit + 1][M + 1];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];

    }
    --Is[i];

    if (i > 1) {
      //uint before = binomialCoeff(Is[i-1], limit - (i-1));
      //uint after  = binomialCoeff(Is[i], limit - (i) + 1);
      uint before = befores[i - 1][Is[i - 1]];
      uint after = afters[i][Is[i]];

      //assert(after == after2);
      //assert(before == before2);

      assert(before > after);
      afterCoefs[i - 2] = after;
      deltaCoefs[i - 2] = before - after;
    }

    if (limit == i) {
      uint index = 0;
      for (int i = 0; i < (limit - 1); ++i) {
        index += afterCoefs[i];
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

/*  iterates rows of befores and afters with arrays of pointers, to avoid array lookups. adds overhead because pointer deref isn't cheap
 */
void walkCombinadicDeltas8(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  uint32_t afterCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t afters[limit + 1][M + 1];

  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];
  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      //bs[i] = &befores[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --Is[i];
    --bs[i];
    --as[i];

    if (i > 1) {
      //uint before2 = befores[i-1][Is[i-1]];
      //uint after2  = afters[i][Is[i]];
      uint before = *bs[i - 1];
      uint after = *as[i];

      //assert(before2 == before);
      //assert(after == after2);
      assert(before > after);
      afterCoefs[i - 2] = after;
      deltaCoefs[i - 2] = before - after;
    }

    if (limit == i) {
      uint index = 0;
      for (int i = 0; i < (limit - 1); ++i) {
        index += afterCoefs[i];
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// computes index directly from as[] instead of involving deltaCoeffs slight saving
void walkCombinadicDeltas9(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  //uint32_t afterCoefs[limit -1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t afters[limit + 1][M + 1];

  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];
  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      //bs[i] = &befores[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --Is[i];
    --bs[i];
    --as[i];

    if (i > 1) {
      //uint before2 = befores[i-1][Is[i-1]];
      //uint after2  = afters[i][Is[i]];
      uint before = *bs[i - 1];
      uint after = *as[i];

      //assert(before2 == before);
      //assert(after == after2);
      assert(before > after);
      //afterCoefs[i-2] = after;
      deltaCoefs[i - 2] = before - after;
    }

    if (limit == i) {
      uint index = 0;
      //for (int i = 0; i < (limit -1); ++i) {
      //index += afterCoefs[i];
      //}

      for (int i = 2; i <= (limit); ++i) {
        index += *as[i];
      }


      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

/*  moves delta calculation back into the kernel. like delta9  10 Initializes as[] and bs[] with pointer math relative to the prior row, instead of a lookup.
 *   table is now iterated instead of lookup'd finally breaks even(ish) with Delta6 which introduced the lookup
 */
void walkCombinadicDeltas10(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  //uint32_t afterCoefs[limit -1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t afters[limit + 1][M + 1];

  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];
  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      //bs[i] = &befores[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --Is[i];
    --bs[i];
    --as[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *as[i];

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// more sensible bounds?
void walkCombinadicDeltas11(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  //uint32_t afterCoefs[limit -1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t afters[limit + 1][M + 1];

  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];
  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      //bs[i] = &befores[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --Is[i];
    --bs[i];
    --as[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 1; i < (limit); ++i) {
        uint before = *bs[i];
        uint after = *as[i + 1];

        assert(before > after);
        deltaCoefs[i - 1] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// use single LUT
void walkCombinadicDeltas12(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --Is[i];
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (M + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// more sensible bounds? single LUT but favor afters, rather than befores
void walkCombinadicDeltas13(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  //uint32_t afterCoefs[limit -1];

  uint32_t afters[limit + 1][M + 1];

  uint32_t *as[limit + 1];

  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
    }
    --Is[i];
    --as[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 1; i < (limit); ++i) {
        uint before = *(((uint32_t *) as[i]) + (M + 1));
        uint after = *as[i + 1];

        assert(before > after);
        deltaCoefs[i - 1] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// bounds check
void walkCombinadicDeltas14(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;
  uint32_t deltaCoefs[limit - 1];
  uint32_t afters[limit + 1][M + 1];

  uint32_t *as[limit + 1];

  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
      //as[i] = &afters[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
    }
    --Is[i];
    --as[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *(((uint32_t *) as[i - 1]) + (M + 1));
        uint after = *as[i];

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// eliminate Is[]
void walkCombinadicDeltas15(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    bs[i] = NULL;
    for (int j = M; j >= 0; --j) {
      if (SENTINEL == befores[i][j]) {
        bs[i] = &(befores[i][j]);
        break;
      }
    }
    assert(bs[i] != NULL);
  }

  while (0 < i) {
    if (SENTINEL == *bs[i]) {
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (M + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == *bs[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// different sentinel
void walkCombinadicDeltas44(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  for (uint i = 1; i <= limit; ++i) {
    bs[i] = &(befores[i][limit - i - 1]);
  }

  while (0 < i) {
    if (&befores[i][limit - i - 1] == bs[i]) {
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (M + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (&befores[i][limit - i - 1] == bs[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// square up befores
void walkCombinadicDeltas16(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  const int rowsize = (M - limit) + 1;

  uint32_t befores[limit + 1][rowsize + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][rowsize];

  for (int j = limit; j > 0; --j) {
    for (int i = rowsize; i > 0; --i) {
      int x = M - j - (rowsize - i);
      //printf("%d %d %d ", j, i, x);
      befores[j][i] = binomialCoeff(x, limit - j);
      //printf("%d\n", befores[j][i]);
    }

    befores[j][0] = 0;
  }

  befores[limit][1] = 0;
  bs[limit] = &befores[limit][1];
  for (uint i = 1; i < limit; ++i) {
    bs[i] = &befores[i][0];
    /*for (int j = rowsize; j >= 0; --j) {
      if (SENTINEL == befores[i][j]) {
	      bs[i] = &(befores[i][j]);
	      printf("%d %d\n", i, j);
	      break;
      }
    }
    assert(bs[i] != NULL);*/
  }

  while (0 < i) {
    if (SENTINEL == *bs[i]) {
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (rowsize + 1 + 1);
    }
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (rowsize + 1 + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == *bs[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// square up befores, better bounds?
void walkCombinadicDeltas17(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  const int rowsize = (M - limit) + 1;

  uint32_t befores[limit + 1][rowsize + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][rowsize];

  for (int j = limit; j > 0; --j) {
    for (int i = rowsize; i > 0; --i) {
      int x = M - j - (rowsize - i);
      //printf("%d %d %d ", j, i, x);
      befores[j][i] = binomialCoeff(x, limit - j);
      //printf("%d\n", befores[j][i]);
    }

    befores[j][0] = 0;
  }

  befores[limit][1] = 0;

  for (uint i = 1; i <= limit; ++i) {
    bs[i] = NULL;
    for (int j = rowsize; j >= 0; --j) {
      if (SENTINEL == befores[i][j]) {
        bs[i] = &(befores[i][j]);
        //printf("%d %d\n", i, j);
        break;
      }
    }
    assert(bs[i] != NULL);
  }

  while (0 < i) {
    if (SENTINEL == *bs[i]) {
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (rowsize + 1 + 1);
    }
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 1; i < (limit); ++i) {
        uint before = *bs[i];
        uint after = *(((uint32_t *) bs[i + 1]) - (rowsize + 1 + 1));

        assert(before > after);
        deltaCoefs[i - 1] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == *bs[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

// square up befores, better bounds?
void walkCombinadicDeltas18(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  const int rowsize = (M - limit) + 1;

  uint32_t befores[limit + 1][rowsize + 1];
  uint32_t *bs[limit + 1];

  for (int j = limit; j > 0; --j) {
    for (int i = rowsize; i > 0; --i) {
      int x = M - j - (rowsize - i);
      //printf("%d %d %d ", j, i, x);
      befores[j][i] = binomialCoeff(x, limit - j);
      //printf("%d\n", befores[j][i]);
    }

    befores[j][0] = 0;
  }
  befores[limit][1] = 0;

  bs[0] = &befores[0][rowsize];
  for (uint i = 1; i < limit; ++i) {
    bs[i] = &befores[i][0];
  }
  bs[limit] = &befores[limit][1];

  while (0 < i) {
    if (SENTINEL == *bs[i]) {
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (rowsize + 1 + 1);
    }
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 1; i < (limit); ++i) {
        uint before = *bs[i];
        uint after = *(((uint32_t *) bs[i + 1]) - (rowsize + 1 + 1));

        assert(before > after);
        deltaCoefs[i - 1] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == *bs[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

void walkCombinadicDeltas19(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    bs[i] = NULL;
    for (int j = M; j >= 0; --j) {
      if (SENTINEL == befores[i][j]) {
        bs[i] = &(befores[i][j]);
        break;
      }
    }
    assert(bs[i] != NULL);
  }

  while (true) {
    if (SENTINEL == *bs[i]) {
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --bs[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (M + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == *bs[i]) {
      --i;
      if (0 == i) {break;}
    } else if (limit > i) {
      ++i;
    }
  }
}

void walkCombinadicDeltas20(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);

    /*bs[i] = NULL;
    for (int j = M; j >= 0; --j) {
      if (SENTINEL == befores[i][j]) {
        bs[i] = &(befores[i][j]);
        break;
      }
    }
    assert(bs[i] != NULL);*/
  }

  while (true) {
    /*if (SENTINEL == *bs[i]) {
      //bs[i] = &befores[i][Is[i]];
      bs[i] = ((uint32_t *) bs[i - 1]) + (M + 1);
    }
    --bs[i];
*/
    //if (limit == i) {
      uint index = 0;

      for (int i = 2; i <= (limit); ++i) {
        uint before = *bs[i - 1];
        uint after = *(((uint32_t *) bs[i]) - (M + 1));

        assert(before > after);
        deltaCoefs[i - 2] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    //}

    if (0 == *bs[i]) {
      do{
        --i;
        --bs[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) {break;}
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
      } while (i < limit);
    } else {
      --bs[i];
    }
  }
}

void walkCombinadicDeltas21(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }
  befores[0][M-1] = 1;
  befores[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
  }

  while (true) {
    uint index = 0;

    for (int i = 2; i <= (limit); ++i) {
      uint before = *bs[i - 1];
      uint after = *(((uint32_t *) bs[i]) - (M + 1));

      assert(before > after);
      deltaCoefs[i - 2] = before - after;
      index += after;
    }

    (*func)(limit, index, deltaCoefs, arg);

    if (0 == *bs[i]) {
      do{
        --i;
        --bs[i];
      } while (0 == *bs[i]);
      if (0 == i) {break;}
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M);
      } while (i < limit);
    } else {
      --bs[i];
    }
  }
}

void walkCombinadicDeltas22(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
  }

  while (true) {
    uint index = 0;

    for (int i = 1; i < (limit); ++i) {
      uint before = *bs[i];
      uint after = *(((uint32_t *) bs[i + 1]) - (M + 1));

      assert(before > after);
      deltaCoefs[i - 1] = before - after;
      index += after;
    }

    (*func)(limit, index, deltaCoefs, arg);

    if (0 == *bs[i]) {
      do{
        --i;
        --bs[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) {break;}
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
      } while (i < limit);
    } else {
      --bs[i];
    }
  }
}

void walkCombinadicDeltas23(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  const int rowsize = (M - limit) + 1;

  uint32_t befores[limit + 1][rowsize + 1];
  uint32_t *bs[limit + 1];

  for (int j = limit; j > 0; --j) {
    for (int i = rowsize; i > 0; --i) {
      int x = M - j - (rowsize - i);
      befores[j][i] = binomialCoeff(x, limit - j);
    }

    befores[j][0] = 0;
  }

  befores[limit][1] = 0;

  for (uint i = 0; i <= limit; ++i) {
    bs[i] = &befores[i][rowsize];
  }

  while (true) {
    uint index = 0;

    for (int i = 2; i <= (limit); ++i) {
      uint before = *bs[i - 1];
      uint after = *(((uint32_t *) bs[i]) - (rowsize + 1 + 1));

      assert(before > after);
      deltaCoefs[i - 2] = before - after;
      index += after;
    }

    (*func)(limit, index, deltaCoefs, arg);

    if (0 == *bs[i]) {
      do{
        --i;
        --bs[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) {break;}
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (rowsize + 1);
      } while (i < limit);
    } else {
      --bs[i];
    }
  }
}

void walkCombinadicDeltas24(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
  }

  while (true) {
    uint index = 0;

    for (int i = 2; i <= (limit); ++i) {
      uint before = *bs[i - 1];
      uint after = *as[i];

      assert(before > after);
      deltaCoefs[i - 2] = before - after;
      index += after;
    }

    (*func)(limit, index, deltaCoefs, arg);

    if (0 == *bs[i]) {
      do{
        --i;
        --bs[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) {break;}
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
      } while (i < limit);
    } else {
      --bs[i]; // XXX: y?
      --as[i];
    }
  }
}


void walkCombinadicDeltas25(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
  }

  while (true) {
    uint index = 0;

    for (int i = 2; i <= (limit); ++i) {
      uint after = *as[i];
      index += after;
    }

    /*
    for (int i = 1; i <= (limit); ++i) {
      uint before = *bs[i - 1];
      uint after = *as[i];

      assert(before > after);
      deltaCoefs[i - 1] = before - after;
    }*/

    (*func)(limit, index, deltaCo, arg);

    if (0 == *bs[i]) {
      do{
        --i;
        --bs[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) {break;}
      deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      } while (i < limit);
    } else {
      --bs[i];
      //deltaCoefs[i - 1] += *as[i];
      --as[i];
      deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      //deltaCoefs[i - 1] -= *as[i];
      // shoooould be equal to --deltaCoefs[i - 1];--index;
    }
  }
}

void walkCombinadicDeltas26(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
  }

  while (true) {
    uint index = 0;

    for (int i = 2; i <= (limit); ++i) {
      uint after = *as[i];
      index += after;
    }

    /*
    for (int i = 1; i <= (limit); ++i) {
      uint before = *bs[i - 1];
      uint after = *as[i];

      assert(before > after);
      deltaCoefs[i - 1] = before - after;
    }*/

    (*func)(limit, index, deltaCo, arg);

    if (0 == *bs[i]) {
      do{
        --i;
        --bs[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) {break;}
      deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      } while (i < limit);
    } else {
      --bs[i];
      deltaCoefs[i - 1] += *as[i];
      --as[i];
      //deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      deltaCoefs[i - 1] -= *as[i];
      // shoooould be equal to --deltaCoefs[i - 1];--index;
    }
  }
}

void walkCombinadicDeltas27(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);

  uint index = 0;
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
    if (i > 1){index += after;}
  }

  while (true) {
    (*func)(limit, index, deltaCo, arg);

    if (0 == *bs[i]) {
      do{
        --i;
        --bs[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) {break;}
      deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      } while (i < limit);
      index=0;
      for (int i = 2; i <= (limit); ++i) {
        uint after = *as[i];
        index += after;
      }
    } else {
      --bs[i];
      deltaCoefs[i - 1] += *as[i];
      index -= *as[i];
      --as[i];
      //deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      deltaCoefs[i - 1] -= *as[i];
      index += *as[i];
      // shoooould be equal to --deltaCoefs[i - 1];--index;
      //--deltaCoefs[i - 1];
    }
  }
}

void walkCombinadicDeltas28(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);

  uint index = 0;
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
    if (i > 1){index += after;}
  }

  while (true) {
    (*func)(limit, index, deltaCo, arg);

    if (0 == *bs[i]) {
      index -= *as[i];
      do {
        --i;
        --bs[i];
        index -= *as[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) { break; }
      if (1 == i) {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M);
        as[i] = ((uint32_t *) as[i - 1]) + (M);
        deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
        index = *as[i];
      } else {
        deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
        index += *as[i];
      }
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
        index += *as[i];
      } while (i < limit);
    } else {
      --bs[i];
      deltaCoefs[i - 1] += *as[i];
      index -= *as[i];
      --as[i];
      deltaCoefs[i - 1] -= *as[i];
      index += *as[i];
      // shoooould be equal to --deltaCoefs[i - 1];--index;
      //--deltaCoefs[i - 1];
    }
  }
}


void walkCombinadicDeltas29(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);

  uint index = 0;
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
    if (i > 1){index += after;}
  }

  while (true) {
    (*func)(limit, index, deltaCo, arg);

    if (0 == *bs[i]) {
      index -= *as[i];
      do {
        --i;
        --bs[i];
        index -= *as[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) { break; }
      if (1 == i) {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M);
        as[i] = ((uint32_t *) as[i - 1]) + (M);
        deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
        index = *as[i];
      } else {
        deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
        index += *as[i];
      }
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
        index += *as[i];
      } while (i < limit);
    } else {
      --bs[i];
      --as[i];
      // shoooould be equal to --deltaCoefs[i - 1];--index;
      ++deltaCoefs[i - 1];
      --index;
    }
  }
}

void walkCombinadicDeltas30(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);

  uint index = 0;
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
    if (i > 1){index += after;}
  }

  while (true) {
    (*func)(limit, index, deltaCo, arg);

    if (0 == *bs[i]) {
      index -= *as[i];
      do {
        --i;
        --bs[i];
        index -= *as[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) { break; }
      if (1 == i) {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M);
        as[i] = ((uint32_t *) as[i - 1]) + (M);
        index = *as[i];
      } else {
        index += *as[i];
      }
      deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
        index += *as[i];
      } while (i < limit);
    } else {
      --bs[i];
      --as[i];
      ++deltaCoefs[i - 1];
      --index;
    }
  }
}

void walkCombinadicDeltas31(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);

  uint index = 0;
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
    if (i > 1){index += after;}
  }

  while (true) {
    (*func)(limit, index, deltaCo, arg);

    if (0 == *bs[i]) {
      index -= *as[i];
      do {
        --i;
        --bs[i];
        index -= *as[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) { break; }
      if (1 == i) {
        index = 0;
      } else {
        index += *as[i];
        deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
      }
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
        index += *as[i];
      } while (i < limit);
    } else {
      --bs[i];
      --as[i];
      ++deltaCoefs[i - 1];
      --index;
    }
  }
}


void walkCombinadicDeltas32(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit];
  uint32_t *deltaCo = &deltaCoefs[1];

  uint32_t befores[limit + 1][M + 1];
  uint32_t *bs[limit + 1];
  uint32_t *as[limit + 1];

  bs[0] = &befores[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = binomialCoeff(i, limit - j);
    }
  }

  befores[limit][0] = 0;

  as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);

  uint index = 0;
  for (uint i = 1; i <= limit; ++i) {
    bs[i] = ((uint32_t *) bs[i - 1]) + (M);
    as[i] = ((uint32_t *) as[i - 1]) + (M);
    uint before = *bs[i - 1];
    uint after = *as[i];
    assert(before > after);
    deltaCoefs[i - 1] = before - after;
    if (i > 1){index += after;}
  }

  while (true) {
    (*func)(limit, index, deltaCo, arg);

    if (0 == *bs[i]) {
      do {
        --i;
        --bs[i];
        --as[i];
      } while (0 == *bs[i] && 0 < i);
      if (0 == i) { break; }
      if (1 == i) {
      } else {
        deltaCoefs[i - 1] = *bs[i - 1] - *as[i];
      }
      do {
        ++i;
        bs[i] = ((uint32_t *) bs[i - 1]) + (M );
        as[i] = ((uint32_t *) as[i - 1]) + (M );
        deltaCoefs[i - 1] = *bs[i-1] - *as[i];
      } while (i < limit);
      index=0;
      for (int i = 2; i <= (limit); ++i) {
        uint after = *as[i];
        index += after;
      }
    } else {
      --bs[i];
      --as[i];
      ++deltaCoefs[i - 1];
      --index;
    }
  }
}

void walkCombinadicDeltas36(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = limit;
  const node_t SENTINEL = 0;

  // one greater than the highest acceptable value - gets fed into the next element
  uint32_t deltaCoefs[limit - 1];

  uint32_t afters[limit + 1][M + 1];
  uint32_t *as[limit + 1];

  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  afters[limit][0] = 0;

  for (uint i = 1; i <= limit; ++i) {
    as[i] = ((uint32_t *) as[i - 1]) + (M);
  }

  while (true) {
    uint index = 0;

    for (int i = 2; i <= (limit); ++i) {
      uint before = *(((uint32_t *) as[i - 1]) + (M + 1));
      uint after = *as[i];

      assert(before > after);
      deltaCoefs[i - 2] = before - after;
      index += after;
    }

    (*func)(limit, index, deltaCoefs, arg);

    if (0 == *as[i]) {
      do{
        --i;
        --as[i];
      } while (0 == *as[i] && 0 < i);
      if (0 == i) {break;}
      do {
        ++i;
        as[i] = ((uint32_t *) as[i - 1]) + (M );
      } while (i < limit);
    } else {
      --as[i];
    }
  }
}

void walkCombinadicDeltas37(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                            void *arg) {
  uint i = 1;
  const node_t SENTINEL = 0;

  uint32_t deltaCoefs[limit - 1];
  uint32_t afters[limit + 1][M + 1];
  uint32_t *as[limit + 1];

  // one greater than the highest acceptable value - gets fed into the next element
  as[0] = &afters[0][M];

  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      afters[j][i] = binomialCoeff(i, limit - j + 1);
    }
  }

  for (uint i = 1; i <= limit; ++i) {
    as[i] = NULL;
    for (int j = M; j >= 0; --j) {
      if (SENTINEL == afters[i][j]) {
        as[i] = &(afters[i][j]);
        printf("%d %d\n", i, j);
        break;
      }
    }
    assert(as[i] != NULL);
  }

  /*for (uint i = 1; i <= limit; ++i) {
    as[i] = &(afters[i][limit - i]);
  }*/

  while (0 < i) {
    if (SENTINEL == *as[i]) {
      //as[i] = &afters[i][Is[i]];
      as[i] = ((uint32_t *) as[i - 1]) + (M + 1);
    }
    --as[i];

    if (limit == i) {
      uint index = 0;

      for (int i = 1; i < (limit); ++i) {
        uint before = *(((uint32_t *) as[i]) + (M + 1));
        uint after = *as[i + 1];

        assert(before > after);
        deltaCoefs[i - 1] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == *as[i]) {
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

#define GLUE_HELPER(x, y) x##y
#define GLUE(x, y) GLUE_HELPER(x, y)


#ifndef IVER
#define IVER 6
#endif
#ifndef TVER
#define TVER 29
#endif

/// tries to make a rectangular lut for kernel lookups
void walkCombinadicDeltas(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                          void *arg) {
  const uint row_len = M - limit + 1;
  int co[limit][row_len + 1];

  int i = 1;
  node_t Is[limit + 1];
  const node_t SENTINEL = 255;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  for (int min_node = limit - 1; min_node >= 0; --min_node) {
    for (int node = M - (limit - 1 - min_node); node >= min_node; --node) {
      co[(limit - 1) - min_node][node - min_node] = coeffs[node][min_node + 1];
    }
  }

  while (0 < i) {
    if (SENTINEL == Is[i]) {
      Is[i] = Is[i - 1];
    }
    --Is[i];

    if (limit == i) {
      uint32_t deltaCoefs[limit - 1];
      uint index = 0;

      // pre-calculate the combinadic components for each node, for both coming before and after the 'missing' node
      for (int k = 0; k < (limit - 1); ++k) {
        // ??? uint before = *(&co[k][Is[k+1] - (limit - 1 - k)] + row_len + 2);//binomialCoeff(Is[i+1],limit-(i+1));
        uint before = co[k][Is[k + 1] - (limit - 1 - k)] + row_len + 2;//binomialCoeff(Is[i+1],limit-(i+1));
        uint after = co[k + 1][Is[k + 2] - (limit - 1 - (k + 1))]; //binomialCoeff(Is[i+2], limit - (i+2) + 1);
        assert(before > after);
        deltaCoefs[k] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (0 == Is[i]) {
      Is[i] = SENTINEL;
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

void walkCombinadicDeltas2(const uint limit, void(*func)(uint limit, score_t idx, score_t *deltaCoeffs, void *arg),
                           void *arg) {
  const uint row_len = M - limit + 1;
  int i = 0;
  int co[limit][row_len + 1];

  for (int min_node = limit - 1; min_node >= 0; --min_node) {
    for (int node = M - (limit - 1 - min_node); node >= min_node; --node) {
      co[(limit - 1) - min_node][node - min_node] = coeffs[node][min_node + 1];
    }
  }

  for (int k = 0; k < limit; ++k) {
    for (int j = 0; j <= row_len; ++j) {
      printf(" %d", co[k][j]);
    }
    printf("\n");
  }

  int *beforeCoeffs[limit];

  for (uint j = 0; j < limit; ++j) {
    beforeCoeffs[j] = &co[j][row_len];
  }

  while (i > 0) {
    if (&co[i][0] >= beforeCoeffs[i]) {
      beforeCoeffs[i] = beforeCoeffs[i - 1] + row_len + 1;
    } else {
      beforeCoeffs[i] -= 1;
    }

    if (limit - 1 == i && &co[i][0] <= beforeCoeffs[i]) {
      uint32_t deltaCoefs[limit - 1];
      uint index = 0;

      // pre-calculate the combinadic components for each node, for both coming before and after the 'missing' node
      for (int k = 0; k < (limit - 1); ++k) {
        uint before = *(beforeCoeffs[k] + row_len + 2);//binomialCoeff(Is[i+1],limit-(i+1));
        uint after = *beforeCoeffs[k + 1]; //binomialCoeff(Is[i+2], limit - (i+2) + 1);
        assert(before > after);
        deltaCoefs[k] = before - after;
        index += after;
      }

      (*func)(limit, index, deltaCoefs, arg);
    }

    if (&co[i][0] > beforeCoeffs[i]) {
      --i;
    } else if (limit - 1 > i) {
      ++i;
    }
  }
}

/* core structure */
struct Layout {
#ifdef VERIFY
  layout_t name;
#endif
  mlidx_t scoreIdx;
};

struct MetaLayout {
  score_t scores[SCORE_SIZE];
};

/* libc Tree functions */
int scoreCompare(const void *_a, const void *_b) {
  struct MetaLayout *a = (struct MetaLayout *) _a;
  struct MetaLayout *b = (struct MetaLayout *) _b;

  //for (int i = SCORE_SIZE - 1; i >= 0; --i) {
  for (int i = 0; i < SCORE_SIZE; ++i) {
    /*if (a->scores[i] < b->scores[i]) {
      return -1;
    } else if (a->scores[i] > b->scores[i]) {
      return 1;
      }*/
    score_t val = a->scores[i] - b->scores[i];

    if (val) {
      return val;
    }
  }

  return 0;
}

void noop(void *nodep) {
  return;
}

void printScore(const void *nodep, const VISIT which, const int depth) {
  if (postorder == which || leaf == which) {
    struct MetaLayout **s = (struct MetaLayout **) nodep;
    uint bodycount = 0;
    uint64_t weightedbodycount = 0;

    for (uint i = 0; i < SCORE_SIZE; ++i) {
      printf("  %u", (*s)->scores[i]);
      bodycount += (*s)->scores[i];
      weightedbodycount += (*s)->scores[i] * (SCORE_SIZE - i);
    }
    printf(" - %u -- %lu\n", bodycount, weightedbodycount);
  }
}

bool onlyOnce;
struct MetaLayout *shorty;
int sfd;

void shortestScore(const void *nodep, const VISIT which, const int depth) {
  if (leaf == which && onlyOnce) {
    struct MetaLayout **s = (struct MetaLayout **) nodep;
    shorty = (*s);

    onlyOnce = false;
  }
}

uint directLookup(const node_t *Is, int len, const int oddManOut) {
  uint idx = 0;

  // Is[] is 1 indexed
  for (int i = 1; i < oddManOut; ++i) {
    idx += binomialCoeff(Is[i], len - i);
  }

  for (int i = oddManOut + 1; i <= len; ++i) {
    idx += binomialCoeff(Is[i], len - i + 1);
  }

  return idx;
}

void sumChildLayoutScoresDeltas(
#ifdef VERIFY
    layout_t name, const node_t *Is,
#endif
    struct Layout *curr, layout_t layoutsInCurr, struct MetaLayout *curr_ml,
    uint idx, uint *deltaCoefs, int coefs_len,
    struct MetaLayout *next_ml, int scores_len) {

  {
    const struct MetaLayout *temp_ml = &curr_ml[curr[layoutsInCurr - 1 - idx].scoreIdx];
#ifdef VERIFY
    assert(curr[layoutsInCurr - 1 - idx].name == (name ^ ((layout_t)1 << Is[1])));
#endif

    for (int j = 0; j < scores_len; ++j) {
      next_ml->scores[j] = temp_ml->scores[j];
    }
  }

  // add each child in
  for (int i = 0; i < coefs_len; ++i) {
    //struct Layout *temp = &args->curr[args->layoutsInCurr - 1 - directLookup(Is, len, i)];
    idx += deltaCoefs[i];

    const struct MetaLayout *temp_ml = &curr_ml[curr[layoutsInCurr - 1 - idx].scoreIdx];
#ifdef VERIFY
    assert(curr[layoutsInCurr - 1 - idx].name == (name ^ ((layout_t)1 << Is[i+2])));
#endif

    for (int j = 0; j < scores_len; ++j) {
      next_ml->scores[j] += temp_ml->scores[j];
    }
  }
}

void sumChildLayoutScores(
#ifdef VERIFY
    layout_t name,
#endif
    struct Layout *curr, layout_t layoutsInCurr, struct MetaLayout *curr_ml,
    const node_t *Is, int Is_len, struct MetaLayout *next_ml, int scores_len) {
  uint32_t deltaCoefs[Is_len - 1];
  uint idx = 0;

  // pre-calculate the combinadic components for each node, for both coming before and after the 'missing' node
  for (int i = 1; i < Is_len; ++i) {
    uint before = binomialCoeff(Is[i], Is_len - i);
    uint after = binomialCoeff(Is[i + 1], Is_len - (i + 1) + 1);

    deltaCoefs[i - 1] = before - after;
    assert(before > after);

    idx += after;
  }

  sumChildLayoutScoresDeltas(
#ifdef VERIFY
      name, Is,
#endif
      curr, layoutsInCurr, curr_ml, idx, deltaCoefs, Is_len - 1, next_ml, scores_len);
}

void sumChildLayoutScoresInner(
#ifdef VERIFY
    layout_t name, const node_t *Is,
#endif
    struct Layout *curr, layout_t layoutsInCurr, struct MetaLayout *curr_ml,
    uint idx, uint *beforeCoefs, uint *afterCoefs, int coefs_len,
    struct MetaLayout *next_ml, int scores_len) {

  {
    const struct MetaLayout *temp_ml = &curr_ml[curr[layoutsInCurr - 1 - idx].scoreIdx];
#ifdef VERIFY
    assert(curr[layoutsInCurr - 1 - idx].name == (name ^ ((layout_t)1 << Is[1])));
#endif

    for (int j = 0; j < scores_len; ++j) {
      next_ml->scores[j] = temp_ml->scores[j];
    }
  }

  // add each child in
  for (int i = 0; i < coefs_len; ++i) {
    //struct Layout *temp = &args->curr[args->layoutsInCurr - 1 - directLookup(Is, len, i)];
    idx -= afterCoefs[i];
    idx += beforeCoefs[i];

    const struct MetaLayout *temp_ml = &curr_ml[curr[layoutsInCurr - 1 - idx].scoreIdx];
#ifdef VERIFY
    assert(curr[layoutsInCurr - 1 - idx].name == (name ^ ((layout_t)1 << Is[i+2])));
#endif

    for (int j = 0; j < scores_len; ++j) {
      next_ml->scores[j] += temp_ml->scores[j];
    }
  }
}

void sumChildLayoutScores2(
#ifdef VERIFY
    layout_t name,
#endif
    struct Layout *curr, layout_t layoutsInCurr, struct MetaLayout *curr_ml,
    const node_t *Is, int Is_len, struct MetaLayout *next_ml, int scores_len) {
  uint beforeCoefs[Is_len - 1];
  uint afterCoefs[Is_len - 1];
  uint idx = 0;

  // pre-calculate the combinadic components for each node, for both coming before and after the 'missing' node
  for (int i = 0; i < (Is_len - 1); ++i) {
    uint before = binomialCoeff(Is[i + 1], Is_len - (i + 1));
    uint after = binomialCoeff(Is[i + 2], Is_len - (i + 2) + 1);
    assert(before > after);
    beforeCoefs[i] = before;
    afterCoefs[i] = after;
    idx += after;
  }

  sumChildLayoutScoresInner(
#ifdef VERIFY
      name, Is,
#endif
      curr, layoutsInCurr, curr_ml, idx, beforeCoefs, afterCoefs, Is_len - 1, next_ml, scores_len);
}

/* core work functions, applied with walkOrdered */
struct FirstPassArgs {
  struct Layout *curr;
  layout_t pos;
  struct MetaLayout *ml;
  layout_t ml_idx;
  void *rootp;
};

void FirstPassWork(
#ifdef VERIFY
    layout_t name,
#endif
    uint limit, node_t *Is, void *_arg) {
  struct FirstPassArgs *args = _arg;

  struct MetaLayout *next_ml = &args->ml[args->ml_idx];

#ifdef VERIFY
  args->curr[args->pos].name = name;
#endif
  next_ml->scores[0] = deadnessCheck(Is, limit);

#ifdef VERIFY
  assert(next_ml->scores[0] == !checkIfAlive(name));
#endif

  struct MetaLayout *tmp = *(struct MetaLayout **) tsearch(next_ml, &(args->rootp), &scoreCompare);

  assert(!scoreCompare(tmp, next_ml));

  args->curr[args->pos].scoreIdx = tmp - args->ml;

  if (args->ml_idx == args->curr[args->pos].scoreIdx) {
    ++args->ml_idx;
    assert(ML_SIZE > args->ml_idx);
  }

  ++(args->pos);
}

struct IntermediateZoneArgs {
  struct Layout *curr;
  struct MetaLayout *curr_ml;
  struct Layout *next;
  struct MetaLayout *next_ml;
  layout_t ml_idx;
  layout_t pos;
  uint64_t layoutsInCurr;
#ifdef VERIFY
  score_t dethklok;
#endif
  void *rootp;
};

void IntermediateZoneWork(
#ifdef VERIFY
    layout_t name,
#endif
    uint limit, node_t *Is, void *_arg) {
  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Layout *next = &args->next[args->pos];
  struct MetaLayout *next_ml = &args->next_ml[args->ml_idx];
#ifdef VERIFY
  next->name = name;
#endif

  sumChildLayoutScores(
#ifdef VERIFY
      name,
#endif
      args->curr, args->layoutsInCurr, args->curr_ml, Is, limit, next_ml, (limit - N));


  // if there are no live children, check if alive
  if (limit == next_ml->scores[limit - N - 1]) {
    next_ml->scores[limit - N] = deadnessCheck(Is, limit);
#ifdef VERIFY
    if (1 == next_ml->scores[limit - N]) {
      ++args->dethklok;
      assert(!checkIfAlive(name));
    } else {
      assert(checkIfAlive(name));
    }
#endif
  } else {
    next_ml->scores[limit - N] = 0;
  }

  // normalization is better pushed to the end, after dedup (as long as you don't overflow the comunters). just do lookups with raw values to save time
#ifdef OLDNORMALIZE
  for (int j = 2; j <= (limit - N); ++j) {
    assert(0 == next_ml->scores[(limit - N) - j] % j);
    next_ml->scores[(limit - N) - j] /= j;
  }
#endif

  // next_ml is a temp, unless its unique, then we store it
  next->scoreIdx = (*(struct MetaLayout **) tsearch(&(args->next_ml[args->ml_idx]), &(args->rootp), &scoreCompare))
                   - args->next_ml;

  if (args->ml_idx == next->scoreIdx) {
    ++args->ml_idx;
    assert(ML_SIZE > args->ml_idx);
  }

  ++(args->pos);
}


void IntermediateZoneDeltaWork(
#ifdef VERIFY
    layout_t name,
#endif
    uint limit, node_t *Is, uint idx, uint *deltaCoefs, void *_arg) {
  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Layout *next = &args->next[args->pos];
  struct MetaLayout *next_ml = &args->next_ml[args->ml_idx];
#ifdef VERIFY
  next->name = name;
#endif

//  sumChildLayoutScores(
#ifdef VERIFY
     // name,
#endif
      //args->curr, args->layoutsInCurr, args->curr_ml, Is, limit, next_ml, (limit - N));
  // add each child in
  sumChildLayoutScoresDeltas(
#ifdef VERIFY
      name,
#endif
      args->curr, args->layoutsInCurr, args->curr_ml, idx, deltaCoefs, limit - 1, next_ml, (limit - N));


  // if there are no live children, check if alive
  if (limit == next_ml->scores[limit - N - 1]) {
    next_ml->scores[limit - N] = deadnessCheck(Is, limit);
#ifdef VERIFY
    if (1 == next_ml->scores[limit - N]) {
      ++args->dethklok;
      assert(!checkIfAlive(name));
    } else {
      assert(checkIfAlive(name));
    }
#endif
  } else {
    next_ml->scores[limit - N] = 0;
  }

#ifdef OLDNORMALIZE
  for (int j = 2; j <= (limit - N); ++j) {
    assert(0 == next_ml->scores[(limit - N) - j] % j);
    next_ml->scores[(limit - N) - j] /= j;
  }
#endif

  // next_ml is a temp, unless its unique, then we store it
  next->scoreIdx = (*(struct MetaLayout **) tsearch(&(args->next_ml[args->ml_idx]), &(args->rootp), &scoreCompare))
                   - args->next_ml;

  if (args->ml_idx == next->scoreIdx) {
    ++args->ml_idx;
    assert(ML_SIZE > args->ml_idx);
  }

  ++(args->pos);
}

void TerminalWork(
#ifdef VERIFY
    layout_t name,
#endif
    uint limit, node_t *Is, void *_arg) {

  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Layout *next = &args->next[args->pos];
  struct MetaLayout *next_ml = &args->next_ml[args->ml_idx];
#ifdef VERIFY
  next->name = name;
#endif

  // add each child in
  sumChildLayoutScores(
#ifdef VERIFY
      name,
#endif
      args->curr, args->layoutsInCurr, args->curr_ml, Is, limit, next_ml, SCORE_SIZE);


#ifdef OLDNORMALIZE
  for (int j = 0; j < SCORE_SIZE; ++j) {
    uint adjustment = limit - (M/2);
    assert(0 == next_ml->scores[SCORE_SIZE - j - 1] % (j + adjustment));
    next_ml->scores[SCORE_SIZE - j - 1] /= (j + adjustment);
  }
#endif

  // next_ml is a temp, unless its unique, then we store it
  next->scoreIdx = (*(struct MetaLayout **) tsearch(&(args->next_ml[args->ml_idx]), &(args->rootp), &scoreCompare))
                   - args->next_ml;

  if (args->ml_idx == next->scoreIdx) {
    ++args->ml_idx;
    assert(ML_SIZE > args->ml_idx);
  }

  ++(args->pos);
}

void TerminalCombinadicWork(
#ifdef VERIFY
    layout_t name,
#endif
    uint limit, uint idx, uint *beforeCoefs, uint *afterCoefs, void *_arg) {

  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Layout *next = &args->next[args->pos];
  struct MetaLayout *next_ml = &args->next_ml[args->ml_idx];
#ifdef VERIFY
  next->name = name;
#endif

  // add each child in
  sumChildLayoutScoresInner(
#ifdef VERIFY
      name,
#endif
      args->curr, args->layoutsInCurr, args->curr_ml, idx, beforeCoefs, afterCoefs, limit - 1, next_ml, SCORE_SIZE);


#ifdef OLDNORMALIZE
  for (int j = 0; j < SCORE_SIZE; ++j) {
    uint adjustment = limit - (M/2);
    assert(0 == next_ml->scores[SCORE_SIZE - j - 1] % (j + adjustment));
    next_ml->scores[SCORE_SIZE - j - 1] /= (j + adjustment);
  }
#endif

  // next_ml is a temp, unless its unique, then we store it
  next->scoreIdx = (*(struct MetaLayout **) tsearch(&(args->next_ml[args->ml_idx]), &(args->rootp), &scoreCompare))
                   - args->next_ml;

  if (args->ml_idx == next->scoreIdx) {
    ++args->ml_idx;
    assert(ML_SIZE > args->ml_idx);
  }

  ++(args->pos);
}

void TerminalCombinadicDeltaWork(
#ifdef VERIFY
    layout_t name,
#endif
    uint limit, uint idx, uint *deltaCoefs, void *_arg) {

  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Layout *next = &args->next[args->pos];
  struct MetaLayout *next_ml = &args->next_ml[args->ml_idx];
#ifdef VERIFY
  next->name = name;
#endif

  // add each child in
  sumChildLayoutScoresDeltas(
#ifdef VERIFY
      name,
#endif
      args->curr, args->layoutsInCurr, args->curr_ml, idx, deltaCoefs, limit - 1, next_ml, SCORE_SIZE);


#ifdef OLDNORMALIZE
  for (int j = 0; j < SCORE_SIZE; ++j) {
    uint adjustment = limit - (M/2);
    assert(0 == next_ml->scores[SCORE_SIZE - j - 1] % (j + adjustment));
    next_ml->scores[SCORE_SIZE - j - 1] /= (j + adjustment);
  }
#endif

  //printf("%d\n", args->pos);

  // next_ml is a temp, unless its unique, then we store it
  next->scoreIdx = (*(struct MetaLayout **) tsearch(&(args->next_ml[args->ml_idx]), &(args->rootp), &scoreCompare))
                   - args->next_ml;

  if (args->ml_idx == next->scoreIdx) {
    ++args->ml_idx;
    assert(ML_SIZE > args->ml_idx);
  }

  ++(args->pos);
}

struct ShortyPrinterArgs {
  struct Layout *next;
  layout_t pos;
  struct MetaLayout *next_ml;
  //void* rootp;
};

void ShortyPrinterWork(
    layout_t name,
    uint limit, node_t *Is, void *_arg) {

  struct ShortyPrinterArgs *args = _arg;

  // convenient alias
  struct Layout *next = &args->next[args->pos];
  struct MetaLayout *next_ml = &args->next_ml[next->scoreIdx];

  if (next_ml == shorty) {
    dprintf(sfd, "%u\n", name);
  }

  ++(args->pos);
}

int main(int argc, char **argv) {
  struct Layout *curr, *next;

  initCoeffs();

  printf("%d of %d\n", N, M);

  struct MetaLayout *curr_ml = calloc(ML_SIZE, sizeof(struct MetaLayout));
  struct MetaLayout *next_ml = calloc(ML_SIZE, sizeof(struct MetaLayout));

  uint64_t next_num, next_size, curr_num = binomialCoeff(M, N), curr_size = curr_num * sizeof(struct Layout);
  curr = mymap(&curr_size);

  /* populate first scores array, test liveness for all layouts */
  struct FirstPassArgs arg;
  arg.curr = curr;
  arg.pos = 0;
  arg.ml = curr_ml;
  arg.ml_idx = 0;
  arg.rootp = NULL;

#ifdef VERIFY
  walkOrdered(N, &FirstPassWork, (void*)&arg);
#else
  walkOrderedNameless(N, &FirstPassWork, (void *) &arg);
#endif

  //tdestroy(arg.rootp, &noop);

  node_t i;
  for (i = N + 1; i <= (M / 2); ++i) {
    // set up next
    next_num = binomialCoeff(M, i);
    next_size = next_num * sizeof(struct Layout);
    next = mymap(&next_size);

    printf("-%d\n", i);

    // do the work
    struct IntermediateZoneArgs argz;
    argz.curr = curr;
    argz.next = next;
    argz.pos = 0;
    argz.layoutsInCurr = curr_num;
    argz.curr_ml = curr_ml;
    argz.next_ml = next_ml;
    argz.ml_idx = 0;
    argz.rootp = NULL;
#ifdef VERIFY
    argz.dethklok = 0;
#endif

#ifndef SKIP
#ifdef VERIFY
    walkOrdered(i, &IntermediateZoneWork, (void*)&argz);
#else
    //walkOrderedNameless2(i, &IntermediateZoneWork, (void *) &argz);
    GLUE(walkNamelessDeltas, IVER)(i, &IntermediateZoneDeltaWork, (void *) &argz);
#endif

    assert(next_num == argz.pos);
#endif

#ifdef VERIFY
    if ((M/2) == i) {
      assert(M == argz.dethklok);
    }
#endif

#ifdef NORMALIZE
    for (mlidx_t k = 0; k < argz.ml_idx; ++k) {
      for (int j = 2; j <= (i - N); ++j) {
  assert(0 == argz.next_ml[k].scores[(i - N) - j] % j);
  argz.next_ml[k].scores[(i - N) - j] /= j;
      }
    }
#endif

#ifdef PRINTSCORE
    printf(" - %u\n", argz.ml_idx);
    twalk(argz.rootp, &printScore);
#endif

#ifdef SHORTIES
    onlyOnce = true;
    twalk(argz.rootp, &shortestScore);

    char shortfilename[128];

    sprintf(shortfilename, "shorties/%u/%u", N, i);

    sfd = open(shortfilename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

    if(sfd < 0){
      perror(NULL);
    }

    struct ShortyPrinterArgs ergs;

    ergs.next = next;
    ergs.next_ml = next_ml;
    ergs.pos = 0;

    walkOrdered(i, &ShortyPrinterWork, (void*)&ergs);

    close(sfd);
#endif

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_num = next_num;
    curr_size = next_size;
    struct MetaLayout *temp = curr_ml;
    curr_ml = next_ml;
    next_ml = temp;
    //memset(next_ml, 0, ML_SIZE*sizeof(struct MetaLayout));

    //tdestroy(argz.rootp, &noop);

#ifdef BENCH
    if (BENCH == i) {
      break;
    }
#endif
  }

#ifndef BENCH
  for (; i <= M; ++i) {
    // set up next
    next_num = binomialCoeff(M, i);
    next_size = next_num * sizeof(struct Layout);
    next = mymap(&next_size);

    printf("%d\n", i);

    // do the work
    struct IntermediateZoneArgs argz;
    argz.curr = curr;
    argz.next = next;
    argz.pos = 0;
    argz.layoutsInCurr = curr_num;
    argz.curr_ml = curr_ml;
    argz.next_ml = next_ml;
    argz.ml_idx = 0;
    argz.rootp = NULL;

#ifdef VERIFY
    walkOrdered(i, &TerminalWork, (void*)&argz);
#else
    //walkOrderedNameless(i, &TerminalWork, (void*)&argz);
    //walkCombinadically(i, &TerminalCombinadicWork, (void*)&argz);
    //walkCombinadicDeltas4(i, &TerminalCombinadicDeltaWork, (void*)&argz);
    //walkCombinadicDeltas2(i, &TerminalCombinadicDeltaWork, (void*)&argz);
    GLUE(walkCombinadicDeltas, TVER)(i, &TerminalCombinadicDeltaWork, (void *) &argz);
#endif

    //printf("poz %d\n", argz.pos);

    assert(next_num == argz.pos);

#ifdef NORMALIZE
    for (mlidx_t k = 0; k < argz.ml_idx; ++k) {
      for (int j = 0; j < SCORE_SIZE; ++j) {
  uint adjustment = i - (M/2);
  assert(0 == argz.next_ml[k].scores[SCORE_SIZE - j - 1] % (j + adjustment));
  argz.next_ml[k].scores[SCORE_SIZE - j - 1] /= (j + adjustment);
      }
    }
#endif

#ifdef PRINTSCORE
    printf(" - %u\n", argz.ml_idx);
    twalk(argz.rootp, &printScore);
#endif

#ifdef SHORTIES
    onlyOnce = true;
    twalk(argz.rootp, &shortestScore);

    char shortfilename[128];

    sprintf(shortfilename, "shorties/%u/%u", N, i);

    sfd = open(shortfilename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

    if(sfd < 0){
      perror(NULL);
    }

    struct ShortyPrinterArgs ergs;

    ergs.next = next;
    ergs.next_ml = next_ml;
    ergs.pos = 0;

    walkOrdered(i, &ShortyPrinterWork, (void*)&ergs);

    close(sfd);
#endif

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_num = next_num;
    curr_size = next_size;
    struct MetaLayout *temp = curr_ml;
    curr_ml = next_ml;
    next_ml = temp;
    //memset(next_ml, 0, ML_SIZE*sizeof(struct MetaLayout));

    //tdestroy(argz.rootp, &noop);
  }

  assert(curr_num == 1);
#endif

  for (int j = 0; j < ((M + 1) / 2); ++j) {
    uint64_t total = binomialCoeff(M, j);
    printf("%d %lu %lu\n", j, total, total);
  }

  for (int j = SCORE_SIZE - 1; j >= 0; --j) {
    uint64_t total = binomialCoeff(M, (M / 2) + SCORE_SIZE - j);
    printf("%d %lu %lu\n", (M / 2) + SCORE_SIZE - j, total - curr_ml[0].scores[j], total);
  }

  for (int j = M - N + 1; j <= M; ++j) {
    printf("%d %u %u\n", j, 0, binomialCoeff(M, j));
  }
}
