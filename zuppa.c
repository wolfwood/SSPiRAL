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
// memset
#include <string.h>
// assert
#include <assert.h>

// unlink
#include <unistd.h>


#include <stdbool.h>


/* types */
typedef uint8_t node_t;
typedef uint32_t layout_t;  // needs >= M bits
typedef uint32_t score_t;   // enough bits to hold M choose M/2 ?


/* vars */
//const node_t N = 4;
#define N (node_t)3
//const node_t M = 15;
#define M (node_t)7

//const node_t SCORE_SIZE = ((M+1) / 2) - N;
#define SCORE_SIZE (((M+1) / 2) - N)

#define twoMB (2*1024*1024)

/* util */
node_t MfromN(node_t n) { return ((node_t)1 << n) - 1;}
layout_t node2layout(node_t n) {
  return ((layout_t)1) << (n - 1);
}

int binomialCoeff(int n, int k) {
  int C[k+1];
  memset(C, 0, sizeof(C));

  C[0] = 1;  // nC0 is 1

  for (int i = 1; i <= n; i++) {
    // Compute next row of pascal triangle using
    // the previous row
    for (int j = i < k ? i : k; j > 0; j--) {
      C[j] = C[j] + C[j-1];
    }
  }
  return C[k];
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

#ifndef NFILEBACKED
  int err = unlink(fname);

  if (-1 == err) {
    perror("unlink failed: ");
    exit(1);
  }
#endif


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
      //if (!iterativeSimpleCheck(n)) {
      if (!recurseCheck(name, n, 1)) {
	return false;
      }
    }
  }
  return true;
}

void printLayouts(layout_t name, uint limit, layout_t *Is, void *arg){
  printf("%u -", name);
  for (int j = 1; j <= N; ++j) {
    printf(" %u", Is[j]);
  }
  printf("\n");
}

void printNames(layout_t name, uint limit, layout_t *Is, void *arg){
  printf("%u\n", name);
}

void walk(const uint limit, void(*func)(layout_t name, uint limit, layout_t *Is, void* arg), void* arg) {
  uint i = 1;
  layout_t Is[limit+1], name = 0;

  for (uint i = 0; i <= limit; ++i) {
      Is[i] = 0;
  }


  while (0 < i) {
    if(0 == Is[i]) {
      Is[i] = Is[i-1] + 1;
    } else {
      name ^= node2layout(Is[i]);
      ++Is[i];
    }

    name |= node2layout(Is[i]);

    if (limit == i) {
      (*func)(name, limit, Is, arg);
    }

    if(M == Is[i]) {
      name ^= node2layout(Is[i]);
      Is[i] = 0;
      --i;
    } else if (limit > i) {
      ++i;
    }
  }
}

void walkOrdered(const uint limit, void(*func)(layout_t name, uint limit, layout_t *Is, void* arg), void* arg) {
  uint i = 1;
  layout_t Is[limit+1], name = 0;

  for (uint i = 1; i <= limit; ++i) {
      Is[i] = 0;
  }

  Is[0] = node2layout(M) << 1;

  while (0 < i) {

    if(0 == Is[i]) {
      Is[i] = Is[i-1];
    } else {
      name ^= Is[i];
    }
    Is[i] >>= 1;


    name |= Is[i];

    if (limit == i) {
      (*func)(name, limit, Is, arg);
    }

    if(1 == Is[i]) {
      name ^= Is[i];
      Is[i] = 0;
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

bool isAlive(struct Score* s, uint i) {
  return s->scores[i];
}

struct Score* binSearch(struct Score* arr, uint64_t len, const layout_t name) {
  if (name == arr[len/2].name) {
    return &arr[len/2];
  }

  if (name > arr[len/2].name) {
    return binSearch(arr, len/2, name);
  }

  return binSearch(&arr[(len/2) + 1], len/2, name);
}

struct FirstBlushArgs {
  struct Score *curr;
  layout_t pos;
};

void FirstBlushWork(layout_t name, uint limit, layout_t *Is, void* _arg) {
  struct FirstBlushArgs *args = _arg;

  args->curr[args->pos].name = name;
  args->curr[args->pos].scores[0] = checkIfAlive(name);

  ++(args->pos);
}

struct IntermediateZoneArgs {
  struct Score *curr;
  struct Score *next;
  layout_t pos;
  node_t nodesInLayout;
  uint64_t layoutsInCurr;
};

void IntermediateZoneWork(layout_t name, uint limit, layout_t *Is, void* _arg) {
  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Score *next = &args->next[args->pos];
  next->name = name;

  // initialize score
  for (int j = 0; j < (args->nodesInLayout - N); ++j) {
    next->scores[j] = 0;
  }

  // add each child in
  for (int i = 1; i <= args->nodesInLayout; ++i) {
    struct Score *temp = binSearch(args->curr, args->layoutsInCurr, name ^ Is[i]);

    for (int j = 0; j < (args->nodesInLayout - N); ++j) {
      next->scores[j] += temp->scores[j];
    }
  }

  // if there are no live children, check if alive
  if (0 == next->scores[args->nodesInLayout - N]) {
    next->scores[args->nodesInLayout - N] = checkIfAlive(name);
  }

  // normalize - wtf is this even? - overcounting but I forget why
#ifdef NORMALIZE
  for (int j = 1; j < (args->nodesInLayout - N); ++j) {
    assert(0 == next->scores[(args->nodesInLayout - N) - j -1] % j);
    next->scores[(args->nodesInLayout - N) - j -1] /= j;
  }
#endif

  ++(args->pos);
}

void TerminalWork(layout_t name, uint limit, layout_t *Is, void* _arg) {
  struct IntermediateZoneArgs *args = _arg;

  // convenient alias
  struct Score *next = &args->next[args->pos];
  next->name = name;

  // initialize score
  for (int j = 0; j < (((M+1)/2) - N); ++j) {
    next->scores[j] = 0;
  }

  // add each child in
  for (int i = 1; i <= args->nodesInLayout; ++i) {
    struct Score *temp = binSearch(args->curr, args->layoutsInCurr, name ^ Is[i]);

    for (int j = 0; j < (((M+1)/2) - N); ++j) {
      next->scores[j] += temp->scores[j];
    }
  }

  // normalize - wtf is this even? - overcounting but I forget why
#ifdef NORMALIZE
  for (int j = 1; j < ((M/2) - N); ++j) {
    assert(0 == next->scores[((M/2) - N) - j -1] % j);
    next->scores[((M/2) - N) - j -1] /= j;
  }
#endif
  ++(args->pos);
}


int main(int argc, char** argv) {
  //node_t nodesInLayout = N;

  struct Score *curr, *next;

  printf("%d of %d\n", N, M);

  uint64_t next_size, curr_size = binomialCoeff(M, N) * sizeof(struct Score);
  curr = mymap(&curr_size);

  /* populate first scores array, test liveness for all layouts */
  struct FirstBlushArgs arg;
  arg.curr = curr;
  arg.pos = 0;

  walkOrdered(N, &FirstBlushWork, (void*)&arg);

  node_t i;
  for (i = N+1; i <= ((M+1)/2); ++i) {
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

    walkOrdered(i, &IntermediateZoneWork, (void*)&argz);

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_size = next_size;

#ifdef BENCH
    if (8 == i) {
      break;
    }
#endif
  }

#ifndef BENCH
  for (; i <= M; ++i) {
    // set up next
    next_size = binomialCoeff(M, i) * sizeof(struct Score);
    next = mymap(&next_size);

    // do the work
    struct IntermediateZoneArgs argz;
    argz.curr = curr;
    argz.next = next;
    argz.pos = 0;
    argz.nodesInLayout = i;
    argz.layoutsInCurr = curr_size / sizeof(struct Score);

    walkOrdered(i, &TerminalWork, (void*)&argz);

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_size = next_size;
  }

  assert(curr_size == sizeof(struct Score));
#endif

  for (int j = 0; j < ((M+1)/2); ++j) {
    printf("%d %d %d\n", j, binomialCoeff(M, j), binomialCoeff(M, j));
  }

  for (int j = ((M/2) - N); j >= 0; --j) {
    printf("%d %d %d\n", (M/2) + N - j, curr[0].scores[j], binomialCoeff(M, (M/2) + N - j));
  }

  for (int j = M - N + 1; j <= M; ++j) {
    printf("%d %d %d\n", j, 0, binomialCoeff(M, j));
  }
}
