//#define MAXN 64
#include "nauty.h"

#include "canonicalizer.h"

#include <iostream>

static uint64_t num_data = 0;
static uint64_t num_nodes = 0;

void init_canon(uint64_t data_nodes, uint64_t all_nodes) {
  num_data = data_nodes;
  num_nodes = all_nodes;
}

uint64_t canonicalize(uint64_t layout) {
  graph g[MAXN*MAXM], cg[MAXN*MAXM];
  int lab[MAXN],ptn[MAXN],orbits[MAXN];
  static DEFAULTOPTIONS_GRAPH(options);
  statsblk stats;
  int n,m;

  uint64_t newlayout = 0;


  options.getcanon = true;
  options.defaultptn = true;

  n = num_nodes;
  m = SETWORDSNEEDED(n);

  nauty_check(WORDSIZE,m,n,NAUTYVERSIONID);

  EMPTYGRAPH(g,m,n);


  for (auto i = 1; i <= n; ++i) {
    uint64_t temp_node = 1 << (i-1);

    if ((layout & temp_node) != 0) {
      for (auto j = 1; j <= num_data; ++j) {
	uint64_t component_node = 1 << (j-1);

	if ((i & component_node) != 0) {
	  //std::cout << "  " << i << " <-> " << component_node << std::endl;

	  ADDONEEDGE(g,i,component_node,m);
	}
      }
    }
  }

  densenauty(g,lab,ptn,orbits,&options,&stats,m,n,cg);

  if (stats.errstatus) {
    std::cout << "ERROR: " << stats.errstatus;
  }

  for (auto i = 0; i < n; ++i) {
    //std::cout << "    " << i << " , " << lab[i] << std::endl;

    if (i != lab[i]) {
      uint64_t rename = (1 << i);

      if ((layout & rename) != 0) {
	rename ^= (1 << (lab[i]));

	if ((layout & rename) != rename) {
	  layout ^= rename;
	}
      }
    }
  }

  return layout;
}
