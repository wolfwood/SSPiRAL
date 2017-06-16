#include <stdio.h>
#include <stdlib.h>

// O_APPEND
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// mmap
#include <sys/mman.h>
// types
#include <stdint.h>
#include <stdbool.h>
// memset
#include <string.h>
// assert
#include <assert.h>

// unlink
#include <unistd.h>


/* types */
typedef uint8_t node_t;
typedef uint32_t layout_t;  // needs >= M bits
typedef uint32_t score_t;   // enough bits to hold M choose M/2 ?


/* vars */
//const node_t N = 4;
#define N (node_t)5
//const node_t M = 15;
#define M (node_t)31

//const node_t SCORE_SIZE = ((M+1) / 2) - N;
#define SCORE_SIZE (((M+1) / 2) - N)

#define twoMB (2*1024*1024)


/* util */
node_t MfromN(node_t n) { return ((node_t)1 << n) - 1;}
layout_t node2layout(node_t n) {
  return ((layout_t)1) << (n - 1);
}

static int* coeffs[M+1];

void initCoeffs(){
  coeffs[0] = malloc(sizeof(int));

  coeffs[0][0] = 1;

  for (int i = 1; i <=M; ++i) {
    int limit = i + 1;
    coeffs[i] = calloc(limit, sizeof(int));

    coeffs[i][0] = 1;
    coeffs[i][limit -1] = 1;

    for (int j = 1; j < limit - 1; ++j) {
      coeffs[i][j] = coeffs[i-1][j] + coeffs[i-1][j-1];
    }
  }
}

int binomialCoeff(int n, int k) {
  if ( n < k) {
    return 0;
  }

  //if ( n - k < k) {
  //  k = n - k;
  //}

  return coeffs[n][k];
}

struct Score* mymap(uint64_t* size) {
#ifndef NFILEBACKED
  /*if (*size % twoMB) {
   *size = ((size/twoMB) +1) * twoMB;
   }*/

  char fname[] = "/home/wolfwood/deletemeXXXXXX";

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
  //|MAP_HUGETLB|MAP_HUGE_2MB - adjust length for mummap to page alignment
  void* temp = mmap(NULL, *size, PROT_WRITE,  MAP_NORESERVE|MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
#endif

  if (MAP_FAILED == temp) {
    perror("mmap failed: ");
    exit(1);
  }

  return temp;
}

void myunmap(void* ptr, uint64_t size) {
  int err = munmap(ptr, size);
  if (0 != err) {
    perror("unmap failed: ");
    exit(1);
  }
}

bool recurseCheck(layout_t name, node_t n, node_t i) {
  for(; i <= M; ++i) {
    layout_t l = ((layout_t)1) << (i - 1);

    if (l & name) {
      layout_t temp = i ^ n;

      if (temp == 0 || recurseCheck(name, temp, i+1)) {
	return true;
      }
    }
  }

  return false;
}

bool checkIfAlive(layout_t name) {
  for(node_t n = 1; n <= M; n <<= 1) {
    layout_t l = node2layout(n);

    if ((l & name) == 0) {
      if (!recurseCheck(name, n, 1)) {
	return false;
      }
    }
  }
  return true;
}

void walkOrdered2(const uint limit, void(*func)(layout_t name, uint limit, node_t *Is, void* arg), void* arg) {
  uint i = 1;
  layout_t ells[limit+1], name = 0;
  node_t Is[limit+1];

  for (uint i = 1; i <= limit; ++i) {
    ells[i] = 0;
  }

  ells[0] = node2layout(M) << 1;
  Is[0] = M;

  while (0 < i) {
    if(0 == ells[i]) {
      ells[i] = ells[i-1];
      Is[i] = Is[i-1];
    } else {
      name ^= ells[i];
    }
    ells[i] >>= 1;
    --Is[i];

    name |= ells[i];

    if (limit == i) {
      (*func)(name, limit, Is, arg);
    }

    if(1 == ells[i]) {
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

void walkOrdered(const uint limit, void(*func)(layout_t name, uint limit, node_t *Is, void* arg), void* arg) {
  uint i = 1;
  layout_t name = 0;
  node_t Is[limit+1];
  const node_t SENTINEL = 255;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  while (0 < i) {
    if(SENTINEL == Is[i]) {
      Is[i] = Is[i-1];
    } else {
      name ^= (layout_t)1 << Is[i];
    }
    --Is[i];

    name |= (layout_t)1 << Is[i];

    if (limit == i) {
      (*func)(name, limit, Is, arg);
    }

    if(0 == Is[i]) {
      name ^= (layout_t)1 << Is[i];
      Is[i] = SENTINEL;
      --i;
    } else {
      if (limit > i) {
	++i;
      }
    }
  }
}


/* core structure */
struct Score {
  layout_t name;
  score_t scores[SCORE_SIZE];
};

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

struct FirstBlushArgs {
  struct Score *curr;
  layout_t pos;
};

void FirstBlushWork(layout_t name, uint limit, node_t *Is, void* _arg) {
  struct FirstBlushArgs *args = _arg;

  args->curr[args->pos].name = name;
  args->curr[args->pos].scores[0] = !checkIfAlive(name);

  ++(args->pos);
}

struct IntermediateZoneArgs {
  struct Score *curr;
  struct Score *next;
  layout_t pos;
  node_t nodesInLayout;
  uint64_t layoutsInCurr;
#ifdef VERIFY
  score_t dethklok;
#endif

};

void IntermediateZoneWork(layout_t name, uint limit, node_t *Is, void* _arg) {
  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Score *next = &args->next[args->pos];
  next->name = name;

  // add each child in
  for (int i = 1; i <= args->nodesInLayout; ++i) {
    struct Score *temp = &args->curr[args->layoutsInCurr - 1 - directLookup(Is, args->nodesInLayout, i)];
    assert(temp->name == (name ^ ((layout_t)1 << Is[i])));

    for (int j = 0; j < (args->nodesInLayout - N); ++j) {
      next->scores[j] += temp->scores[j];
    }
  }

  // if there are no live children, check if alive
  if (args->nodesInLayout == next->scores[args->nodesInLayout - N - 1] ) {
    next->scores[args->nodesInLayout - N] = !checkIfAlive(name);
#ifdef VERIFY
    if (1 == next->scores[args->nodesInLayout - N]) {
      ++args->dethklok;
    }
#endif
  } else {
    next->scores[args->nodesInLayout - N] = 0;
  }

  // normalize - wtf is this even? - overcounting but I forget why
#ifdef NORMALIZE
  for (int j = 2; j <= (args->nodesInLayout - N); ++j) {
    assert(0 == next->scores[(args->nodesInLayout - N) - j] % j);
    next->scores[(args->nodesInLayout - N) - j] /= j;
  }
#endif

  ++(args->pos);
}

void TerminalWork(layout_t name, uint limit, node_t *Is, void* _arg) {
  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Score *next = &args->next[args->pos];
  next->name = name;

  // add each child in
  for (int i = 1; i <= args->nodesInLayout; ++i) {
    struct Score *temp = &args->curr[args->layoutsInCurr - 1 - directLookup(Is, args->nodesInLayout, i)];
    assert(temp->name == (name ^ ((layout_t)1 << Is[i])));

    for (int j = 0; j < SCORE_SIZE; ++j) {
      next->scores[j] += temp->scores[j];
    }
  }

  // normalize - wtf is this even? - overcounting but I forget why
#ifdef NORMALIZE
  for (int j = 0; j < SCORE_SIZE; ++j) {
    uint adjustment = args->nodesInLayout - (M/2);
    assert(0 == next->scores[SCORE_SIZE - j - 1] % (j + adjustment));
    next->scores[SCORE_SIZE - j - 1] /= (j + adjustment);
  }
#endif
  ++(args->pos);
}


int main(int argc, char** argv) {
  struct Score *curr, *next;

  initCoeffs();

  printf("%d of %d\n", N, M);

  uint64_t next_size, curr_size = binomialCoeff(M, N) * sizeof(struct Score);
  curr = mymap(&curr_size);

  /* populate first scores array, test liveness for all layouts */
  struct FirstBlushArgs arg;
  arg.curr = curr;
  arg.pos = 0;

  walkOrdered(N, &FirstBlushWork, (void*)&arg);

  node_t i;
  for (i = N+1; i <= (M/2); ++i) {
    // set up next
    next_size = binomialCoeff(M, i) * sizeof(struct Score);
    next = mymap(&next_size);

    printf("-%d\n", i);

    // do the work
    struct IntermediateZoneArgs argz;
    argz.curr = curr;
    argz.next = next;
    argz.pos = 0;
    argz.nodesInLayout = i;
    argz.layoutsInCurr = curr_size / sizeof(struct Score);
#ifdef VERIFY
    argz.dethklok = 0;
#endif

    walkOrdered(i, &IntermediateZoneWork, (void*)&argz);

    assert((next_size / sizeof(struct Score)) == argz.pos);
#ifdef VERIFY
    if ((M/2) == i) {
      assert(M == argz.dethklok);
    }
#endif
      // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_size = next_size;

#ifdef BENCH
    if (BENCH == i) {
      break;
    }
#endif
  }

#ifndef BENCH
  for (; i <= M; ++i) {
    // set up next
    next_size = binomialCoeff(M, i) * sizeof(struct Score);
    next = mymap(&next_size);

    printf("%d\n", i);

    // do the work
    struct IntermediateZoneArgs argz;
    argz.curr = curr;
    argz.next = next;
    argz.pos = 0;
    argz.nodesInLayout = i;
    argz.layoutsInCurr = curr_size / sizeof(struct Score);

    walkOrdered(i, &TerminalWork, (void*)&argz);

    assert((next_size / sizeof(struct Score)) == argz.pos);

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_size = next_size;
  }

  assert(curr_size == sizeof(struct Score));
#endif

  for (int j = 0; j < ((M+1)/2); ++j) {
    uint64_t total = binomialCoeff(M, j);
    printf("%d %lu %lu\n", j, total, total);
  }

  for (int j = SCORE_SIZE - 1; j >= 0; --j) {
    uint64_t total = binomialCoeff(M, (M/2) + SCORE_SIZE - j);
    printf("%d %lu %lu\n", (M/2) + SCORE_SIZE - j, total - curr[0].scores[j], total);
  }

  for (int j = M - N + 1; j <= M; ++j) {
    printf("%d %u %u\n", j, 0, binomialCoeff(M, j));
  }
}
