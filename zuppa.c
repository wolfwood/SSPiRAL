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

// maximum number of unique MetaLayouts (131 for N=5)
#define ML_SIZE (131+1)

/* util */
node_t MfromN(node_t n) { return ((node_t)1 << n) - 1;}
layout_t node2layout(node_t n) {
  return ((layout_t)1) << (n - 1);
}

static int* coeffs[M+1];

void initCoeffs(){
  coeffs[0] = calloc(2, sizeof(int));

  coeffs[0][0] = 1;

  for (int i = 1; i <=M; ++i) {
    int limit = i + 1;
    coeffs[i] = calloc(limit + 1, sizeof(int));

    coeffs[i][0] = 1;
    coeffs[i][limit -1] = 1;

    for (int j = 1; j < limit - 1; ++j) {
      coeffs[i][j] = coeffs[i-1][j] + coeffs[i-1][j-1];
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

struct Layout* mymap(uint64_t* size) {
#ifndef NFILEBACKED
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

bool deadnessCheck(const node_t *Is, const int len) {
  int j = len;

  //  for(node_t n = 1 << (N-1); n > 0; n >>= 1) {
  for(node_t n = 1; n < M; n <<= 1) {
    while (j > 1 && (Is[j]+1) < n) {
      --j;
    }

    if ((Is[j]+1) != n) {
      node_t temp = n;

      node_t Js[len+1];
      const node_t SENTINEL = 3;

      for (uint k = 1; k <= len; ++k) {
	Js[k] = SENTINEL;
      }

      // one greater than the highest acceptable value - gets fed into the next element
      Js[0] = 0;

      bool alive = false;
      uint i = 1;

      while (0 < i) {
	if(0 == Js[i]) {
	  //temp ^= Is[i];
	  Js[i] = SENTINEL;
	  --i;
	} else {
	  if(SENTINEL == Js[i]) {
	    Js[i] = 2;
	  }
	  temp ^= Is[i] +1;

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

void walkOrdered(const uint limit, void(*func)(layout_t name, uint limit, node_t *Is, void* arg), void* arg) {
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

void walkOrdered2(const uint limit, void(*func)(layout_t name, uint limit, node_t *Is, void* arg), void* arg) {
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

void walkOrderedNameless(const uint limit, void(*func)(uint limit, node_t *Is, void* arg), void* arg) {
  uint i = 1;
  node_t Is[limit+1];
  const node_t SENTINEL = 0;

  for (uint i = 1; i <= limit; ++i) {
    Is[i] = SENTINEL;
  }

  // one greater than the highest acceptable value - gets fed into the next element
  Is[0] = M;

  while (0 < i) {
    if(SENTINEL == Is[i]) {
      Is[i] = Is[i-1];
    }
    --Is[i];

    if (limit == i) {
      (*func)(limit, Is, arg);
    }

    if(0 == Is[i]) {
      --i;
    } else if (limit > i) {
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
  struct MetaLayout* a = (struct MetaLayout*)_a;
  struct MetaLayout* b = (struct MetaLayout*)_b;

  for (int i = SCORE_SIZE -1; i >= 0; --i) {
    if(a->scores[i] < b->scores[i]) {
      return -1;
    } else if (a->scores[i] > b->scores[i]) {
      return 1;
    }
  }

  return 0;
}

void noop(void *nodep) {
  return;
}

void printScore(const void *nodep, const VISIT which, const int depth) {
  if (postorder == which || leaf == which) {
    struct MetaLayout** s = (struct MetaLayout**) nodep;

    for (uint i = 0; i < SCORE_SIZE; ++i) {
      printf("  %u", (*s)->scores[i]);
    }
    printf("\n");
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

/* core work functions, applied with walkOrdered */
struct FirstBlushArgs {
  struct Layout *curr;
  layout_t pos;
  struct MetaLayout *ml;
  layout_t ml_idx;
  void* rootp;
};

void FirstBlushWork(
#ifdef VERIFY
		    layout_t name,
#endif
		    uint limit, node_t *Is, void* _arg) {
  struct FirstBlushArgs *args = _arg;

  struct MetaLayout *next_ml = &args->ml[args->ml_idx];

#ifdef VERIFY
  args->curr[args->pos].name = name;
#endif
  next_ml->scores[0] = deadnessCheck(Is, limit);

#ifdef VERIFY
  assert(next_ml->scores[0] == !checkIfAlive(name));
#endif

  struct MetaLayout* tmp = *(struct MetaLayout**)tsearch(next_ml, &(args->rootp), &scoreCompare);

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
			  uint limit, node_t *Is, void* _arg) {
  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Layout *next = &args->next[args->pos];
  struct MetaLayout *next_ml = &args->next_ml[args->ml_idx];
#ifdef VERIFY
  next->name = name;
#endif

  // add each child in
  for (int i = 1; i <= limit; ++i) {
    struct Layout *temp = &args->curr[args->layoutsInCurr - 1 - directLookup(Is, limit, i)];
    struct MetaLayout *temp_ml = &args->curr_ml[temp->scoreIdx];
#ifdef VERIFY
    assert(temp->name == (name ^ ((layout_t)1 << Is[i])));
#endif

    if (1 == i) {
      for (int j = 0; j < (limit - N); ++j) {
	next_ml->scores[j] = temp_ml->scores[j];
      }
    } else {
      for (int j = 0; j < (limit - N); ++j) {
	next_ml->scores[j] += temp_ml->scores[j];
      }
    }
  }

  // if there are no live children, check if alive
  if (limit == next_ml->scores[limit - N - 1] ) {
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

  // normalize - wtf is this even? - overcounting but I forget why
#ifdef NORMALIZE
  for (int j = 2; j <= (limit - N); ++j) {
    assert(0 == next_ml->scores[(limit - N) - j] % j);
    next_ml->scores[(limit - N) - j] /= j;
  }
#endif

  // next_ml is a temp, unless its unique, then we store it
  next->scoreIdx = (*(struct MetaLayout**)tsearch(&(args->next_ml[args->ml_idx]), &(args->rootp), &scoreCompare))
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
		  uint limit, node_t *Is, void* _arg) {

  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Layout *next = &args->next[args->pos];
  struct MetaLayout *next_ml = &args->next_ml[args->ml_idx];
#ifdef VERIFY
  next->name = name;
#endif

  // add each child in
  for (int i = 1; i <= limit; ++i) {
    struct Layout *temp = &args->curr[args->layoutsInCurr - 1 - directLookup(Is, limit, i)];
    struct MetaLayout *temp_ml = &args->curr_ml[temp->scoreIdx];
#ifdef VERIFY
    assert(temp->name == (name ^ ((layout_t)1 << Is[i])));
#endif

    if (1 == i) {
      for (int j = 0; j < SCORE_SIZE; ++j) {
	next_ml->scores[j] = temp_ml->scores[j];
      }
    } else {
      for (int j = 0; j < SCORE_SIZE; ++j) {
	next_ml->scores[j] += temp_ml->scores[j];
      }
    }
  }

  // normalize - wtf is this even? - overcounting but I forget why
#ifdef NORMALIZE
  for (int j = 0; j < SCORE_SIZE; ++j) {
    uint adjustment = limit - (M/2);
    assert(0 == next_ml->scores[SCORE_SIZE - j - 1] % (j + adjustment));
    next_ml->scores[SCORE_SIZE - j - 1] /= (j + adjustment);
  }
#endif

  // next_ml is a temp, unless its unique, then we store it
  next->scoreIdx = (*(struct MetaLayout**)tsearch(&(args->next_ml[args->ml_idx]), &(args->rootp), &scoreCompare))
    - args->next_ml;

  if (args->ml_idx == next->scoreIdx) {
    ++args->ml_idx;
    assert(ML_SIZE > args->ml_idx);
  }

  ++(args->pos);
}


int main(int argc, char** argv) {
  struct Layout *curr, *next;

  initCoeffs();

  printf("%d of %d\n", N, M);

  struct MetaLayout *curr_ml = calloc(ML_SIZE, sizeof(struct MetaLayout));
  struct MetaLayout *next_ml = calloc(ML_SIZE, sizeof(struct MetaLayout));

  uint64_t next_size, curr_size = binomialCoeff(M, N) * sizeof(struct Layout);
  curr = mymap(&curr_size);

  /* populate first scores array, test liveness for all layouts */
  struct FirstBlushArgs arg;
  arg.curr = curr;
  arg.pos = 0;
  arg.ml = curr_ml;
  arg.ml_idx = 0;
  arg.rootp = NULL;

#ifdef VERIFY
  walkOrdered(N, &FirstBlushWork, (void*)&arg);
#else
  walkOrderedNameless(N, &FirstBlushWork, (void*)&arg);
#endif

  //tdestroy(arg.rootp, &noop);

  node_t i;
  for (i = N+1; i <= (M/2); ++i) {
    // set up next
    next_size = binomialCoeff(M, i) * sizeof(struct Layout);
    next = mymap(&next_size);

    printf("-%d\n", i);

    // do the work
    struct IntermediateZoneArgs argz;
    argz.curr = curr;
    argz.next = next;
    argz.pos = 0;
    argz.layoutsInCurr = curr_size / sizeof(struct Layout);
    argz.curr_ml = curr_ml;
    argz.next_ml = next_ml;
    argz.ml_idx = 0;
    argz.rootp = NULL;
#ifdef VERIFY
    argz.dethklok = 0;
#endif

#ifdef VERIFY
    walkOrdered(i, &IntermediateZoneWork, (void*)&argz);
#else
    walkOrderedNameless(i, &IntermediateZoneWork, (void*)&argz);
#endif

    assert((next_size / sizeof(struct Layout)) == argz.pos);
#ifdef VERIFY
    if ((M/2) == i) {
      assert(M == argz.dethklok);
    }
#endif

#ifdef PRINTSCORE
    printf(" - %u\n", argz.ml_idx);
    twalk(argz.rootp, &printScore);
#endif

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
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
    next_size = binomialCoeff(M, i) * sizeof(struct Layout);
    next = mymap(&next_size);

    printf("%d\n", i);

    // do the work
    struct IntermediateZoneArgs argz;
    argz.curr = curr;
    argz.next = next;
    argz.pos = 0;
    argz.layoutsInCurr = curr_size / sizeof(struct Layout);
    argz.curr_ml = curr_ml;
    argz.next_ml = next_ml;
    argz.ml_idx = 0;
    argz.rootp = NULL;

#ifdef VERIFY
    walkOrdered(i, &TerminalWork, (void*)&argz);
#else
    walkOrderedNameless(i, &TerminalWork, (void*)&argz);
#endif

    assert((next_size / sizeof(struct Layout)) == argz.pos);

#ifdef PRINTSCORE
    printf(" - %u\n", argz.ml_idx);
    twalk(argz.rootp, &printScore);
#endif

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_size = next_size;
    struct MetaLayout *temp = curr_ml;
    curr_ml = next_ml;
    next_ml = temp;
    //memset(next_ml, 0, ML_SIZE*sizeof(struct MetaLayout));

    //tdestroy(argz.rootp, &noop);
  }

  assert(curr_size == sizeof(struct Layout));
#endif

  for (int j = 0; j < ((M+1)/2); ++j) {
    uint64_t total = binomialCoeff(M, j);
    printf("%d %lu %lu\n", j, total, total);
  }

  for (int j = SCORE_SIZE - 1; j >= 0; --j) {
    uint64_t total = binomialCoeff(M, (M/2) + SCORE_SIZE - j);
    printf("%d %lu %lu\n", (M/2) + SCORE_SIZE - j, total - curr_ml[0].scores[j], total);
  }

  for (int j = M - N + 1; j <= M; ++j) {
    printf("%d %u %u\n", j, 0, binomialCoeff(M, j));
  }
}
