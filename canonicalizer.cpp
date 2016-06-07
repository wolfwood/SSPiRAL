//#define MAXN 64
#include "nauty.h"
#include "nautinv.h"
#include "canonicalizer.h"

#include <iostream>

static uint64_t num_data = 0;
static uint64_t num_nodes = 0;

void init_canon(uint64_t data_nodes, uint64_t all_nodes) {
  num_data = data_nodes;
  num_nodes = all_nodes;
}
/*
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

  std::cout << "{ ";
  for (auto i = 0; i < n; ++i) {
    //std::cout << "    " << i << " , " << lab[i] << std::endl;

    uint64_t node = (1 << i);

    if (layout & node) {
      uint64_t rename = (1 << (lab[i]));

      if (newlayout & rename) {
	std::cout << "eff dat" << std::endl;
      }

      std::cout << "( " << i+1 << ", " << lab[i]+1 << " ) ";
      newlayout |= rename;
    }
  }

  std::cout << " } ";

  return newlayout;
}
*/

inline uint64_t node2nauty(uint64_t node) {
  return node - 1;
}

inline uint64_t nauty2node(uint64_t nauty) {
  return nauty + 1;
}

inline uint64_t node2layout(uint64_t node) {
  return 1 << (node - 1);
}

inline uint64_t nauty2layout(uint64_t node) {
  return 1 << node;
}

uint64_t di_canonicalize(uint64_t layout) {
  graph g[MAXN*MAXM], cg[MAXN*MAXM];
  int lab[MAXN],ptn[MAXN],orbits[MAXN];
  static DEFAULTOPTIONS_DIGRAPH(options);
  statsblk stats;
  int n,m;

  uint64_t newlayout = 0;


  options.getcanon = true;
  options.defaultptn = true;
  options.digraph = true;

  n = num_nodes;
  m = SETWORDSNEEDED(n);

  nauty_check(WORDSIZE,m,n,NAUTYVERSIONID);

  EMPTYGRAPH(g,m,n);



  for (auto j = 1; j <= num_nodes; ++j) {

    //uint64_t node_id = j+1, layout_bit = 1 << j, nauty_id = j;

    if((layout & node2layout(j)) != 0) {
      std::cout << "  " << j << " -> " << j << std::endl;

      ADDONEARC(g, node2nauty(j),node2nauty(j),m);
    }

    for (auto i = 1; i <= num_nodes; i<<=1) {
      //uint64_t data_id = i, data_layout_id = 1<<(i-1), data_nauty_id = i -1;
      if ((i & j) != 0 && i != j) {
	std::cout << "  " << i << " -> " << j << std::endl;

	ADDONEARC(g,node2nauty(i),node2nauty(j),m);
      }
    }
  }

  densenauty(g,lab,ptn,orbits,&options,&stats,m,n,cg);

  if (stats.errstatus) {
    std::cout << "ERROR: " << stats.errstatus;
  }

  std::cout << "{ ";
  for (auto i = 0; i < n; ++i) {
    //std::cout << "    " << lab[i] << " , " << i << std::endl;

    //uint64_t node = (1 << lab[i]);

    if (layout & nauty2layout(lab[i])) {
      //uint64_t rename = (1 << i);

      if (newlayout & nauty2layout(i)) {
	std::cout << "eff dat" << std::endl;
      }

      std::cout << "( " << nauty2node(lab[i]) << ", " << nauty2node(i) << " ) ";
      newlayout |= nauty2layout(i);
    }
  }

  std::cout << " } ";

  return newlayout;
}