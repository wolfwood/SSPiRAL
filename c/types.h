// spiral types, defines and constants

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>


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

// maximum number of unique MetaLayouts (131 for N=5)
#define ML_SIZE (131+1)


/* util */
node_t MfromN(node_t n);
layout_t node2layout(node_t n);


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


/* work function args, passed from iterators */
struct FirstPassArgs {
  struct Layout *curr;
  layout_t pos;
  struct MetaLayout *ml;
  layout_t ml_idx;
  void *rootp;
};

// also used for terminal passes
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


/* mechanism to allow testing of different function versions programmatically,
   e.g. with hyperfine */
#define GLUE_HELPER(x, y) x##y
#define GLUE(x, y) GLUE_HELPER(x, y)

#ifndef IVER
#define IVER 6
#endif
#ifndef TVER
#define TVER 29
#endif
