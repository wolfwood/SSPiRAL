// calculate reliability of a SSPiRAL array for a given N

#define _GNU_SOURCE
#include <search.h>

#include <stdio.h>
// memset
#include <string.h>
// assert
#include <assert.h>

#include "types.h"
#include "binomial_coeff.h"
#include "liveness.h"
#include "mymmap.h"


/* iterators */

/*  Stop tracking names completely, iterating nodes as 'nodes' -- integer identifiers -- rather than bitmask 'layout' format.
 *   this saves a bunch of xors on this end, and can avoid representation transformation in the work function, particularly deadness.
 *   the layout position is not named directly, it is simply a loop counter buried in the work function.
 */
void walkOrderedNameless(const uint limit, void(*func)(const uint limit, const node_t *Is, void *arg), void *arg) {
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

void walkNamelessDeltas6(const uint limit, void(*func)(const uint limit, const node_t *Is, const score_t idx, const score_t *deltaCoeffs, void *arg),
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

  // a rectangular array with unused elements is faster than a triangular array
  // we still zero the unused elements, the first acts as a sentinel but the
  // others avoid calling binomialCoeff() with bad bounds
  for (int i = M; i >= 0; --i) {
    for (int j = limit; j >= 0; --j) {
      befores[j][i] = i < limit-j ? 0 : binomialCoeff(i, limit - j);
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

void walkCombinadicDeltas29(const uint limit, void(*func)(const uint limit, const score_t idx, const score_t *deltaCoeffs, void *arg),
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
      befores[j][i] = i < limit-j ? 0 : binomialCoeff(i, limit - j);
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

void sumChildLayoutScoresDeltas(
#ifdef VERIFY
    layout_t name, const node_t *Is,
#endif
    const struct Layout *curr, const layout_t layoutsInCurr, struct MetaLayout *curr_ml,
    uint idx, const uint *deltaCoefs, const int coefs_len,
    struct MetaLayout *next_ml, const int scores_len) {

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

/* core work functions, applied with walkOrdered */
void FirstPassWork(
#ifdef VERIFY
    const layout_t name,
#endif
    const uint limit, const node_t *Is, void *_arg) {
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

void IntermediateZoneDeltaWork(
#ifdef VERIFY
    const layout_t name,
#endif
    const uint limit, const node_t *Is, const uint idx, const uint *deltaCoefs, void *_arg) {
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
      name, Is,
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

void TerminalCombinadicDeltaWork(
#ifdef VERIFY
    const layout_t name, const node_t *Is,
#endif
    const uint limit, const uint idx, const uint *deltaCoefs, void *_arg) {

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
      name, Is,
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


// hide the unused code
#include "alternates.c"


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

  tdestroy(arg.rootp, &noop);

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

#ifdef VERIFY
    walkOrdered(i, &IntermediateZoneWork, (void*)&argz);
#else
    //walkOrderedNameless2(i, &IntermediateZoneWork, (void *) &argz);
    GLUE(walkNamelessDeltas, IVER)(i, &IntermediateZoneDeltaWork, (void *) &argz);
#endif

    assert(next_num == argz.pos);

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

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_num = next_num;
    curr_size = next_size;
    struct MetaLayout *temp = curr_ml;
    curr_ml = next_ml;
    next_ml = temp;
    memset(next_ml, 0, ML_SIZE*sizeof(struct MetaLayout));

    tdestroy(argz.rootp, &noop);

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

    // rotate arrays
    myunmap(curr, curr_size);
    curr = next;
    curr_num = next_num;
    curr_size = next_size;
    struct MetaLayout *temp = curr_ml;
    curr_ml = next_ml;
    next_ml = temp;
    memset(next_ml, 0, ML_SIZE*sizeof(struct MetaLayout));

    tdestroy(argz.rootp, &noop);
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
