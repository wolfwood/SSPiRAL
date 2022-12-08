//#define MAXN 64
#include "nauty.h"
#include "nautinv.h"
#include "canonicalizer.h"

#include "bittricks.h"

#include <iostream>
#include <cassert>

static uint64_t num_data = 0;
static uint64_t num_nodes = 0;

extern int labelorg;

void init_canon(uint64_t data_nodes, uint64_t all_nodes) {
  num_data = data_nodes;
  num_nodes = all_nodes;
  //labelorg = 1;
}

uint64_t canonicalize(uint64_t layout) {
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

/*
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
*/

  for (auto j = 1; j <= num_nodes; ++j) {
    //uint64_t node_id = j+1, layout_bit = 1 << j, nauty_id = j;
    if((layout & node2layout(j)) != 0) {
      //std::cout << "  " << j << " -> " << j << std::endl;
      ADDONEARC(g, node2nauty(j),node2nauty(j),m);
    }

    for (auto i = 1; i <= num_nodes; i<<=1) {
      //uint64_t data_id = i, data_layout_id = 1<<(i-1), data_nauty_id = i -1;
      if ((i & j) != 0 && i != j) {
  	    //std::cout << "  " << i << " -> " << j << std::endl;
	      ADDONEARC(g,node2nauty(i),node2nauty(j),m);
      }
    }
  }

  /*
  int idx = 0;

  for (uint64_t i = 1; i <= num_data; ++i) {
    bool applied = false;
    for (uint64_t j = 1; j <= num_nodes; ++j){
      if (POPCOUNT(j) == i) {
        lab[idx] = node2nauty(j);
        ptn[idx] = i;
        ++idx;
        applied = true;
      }
    }
    if (applied) {
      ptn[idx-1] = 0;
    }
  }*/

  densenauty(g,lab,ptn,orbits,&options,&stats,m,n,cg);

  if (stats.errstatus) {
    std::cout << "ERROR: " << stats.errstatus;
  }


  std::cout << "{ ";
  for (auto i = 0; i < n; ++i) {
    std::cout << "    " << lab[i] << " , " << orbits[i] << " , " << ptn[i] << " , " << i << std::endl;
    if (layout & nauty2layout(lab[i])) {
      if (newlayout & nauty2layout(i)) {
	      std::cout << "eff dat" << std::endl;
      }

      //std::cout << "( " << nauty2node(lab[i]) << ", " << nauty2node(i) << " ) ";
      newlayout |= nauty2layout(i);
    }
  }
  std::cout << " } ";

  /*
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
  */
  return newlayout;
}


uint64_t node2nauty(uint64_t node) {
  //return (num_nodes -1) - (node - 1);
  return node - 1;
}

uint64_t nauty2node(uint64_t nauty) {
  //return num_nodes - (nauty);
  return nauty + 1;
}

uint64_t node2layout(uint64_t node) {
  return 1ULL << (node - 1);
}

uint64_t nauty2layout(uint64_t nauty) {
  //return 1 << ((num_nodes - 1) - node);
  return 1ULL << nauty;
}

void recurse_print_orbit(int* orbits, uint64_t start) {
  uint64_t val = orbits[start], resume_idx = num_nodes, resume_val = num_nodes;

  std::cout << nauty2node(start) << " ";

  for (uint64_t i = start+1; i < num_nodes; ++i) {
    if (orbits[i] == val) {
        std::cout << nauty2node(i) << " ";
    } else if (orbits[i] < resume_val && orbits[i] > val) {
      resume_idx = i;
      resume_val = orbits[i];
    }
  }

  if (resume_val < num_nodes) {
    std::cout << "- ";

    recurse_print_orbit(orbits, resume_idx);
  }
}

void print_orbit(int* orbits) {
  uint64_t k = 0;

  for (uint64_t i = 0; i < num_nodes; ++i) {
    if (i == 0 || orbits[k] < orbits[i]) {
      std::cout << nauty2node(i) << " ";
      k = i;

      for (uint64_t j = i+1; j < num_nodes; ++j) {
	      if (orbits[j] == orbits[i]) {
	        std::cout << nauty2node(j) << " ";
	      }
      }

      if (orbits[i] < node2nauty(num_nodes)) {
        std::cout << "- ";
      }
    }
  }

  std::cout << std::endl;
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
      //std::cout << "  " << j << " -> " << j << std::endl;
      ADDONEARC(g, node2nauty(j),node2nauty(j),m);
    }

    for (auto i = 1; i <= num_nodes; i<<=1) {
      //uint64_t data_id = i, data_layout_id = 1<<(i-1), data_nauty_id = i -1;
      if ((i & j) != 0 && i != j) {
  	    //std::cout << "  " << i << " -> " << j << std::endl;
	      ADDONEARC(g,node2nauty(i),node2nauty(j),m);
      }
    }
  }

  densenauty(g,lab,ptn,orbits,&options,&stats,m,n,cg);

  if (stats.errstatus) {
    std::cout << "ERROR: " << stats.errstatus;
  }

  //print_orbit(orbits);

  /*
  for (auto i = 0; i < num_nodes; ++i) {
    std::cout << nauty2node(i) << " - " << nauty2node(orbits[i]) << " ";
  }

  std::cout << std::endl;
  */
  std::cout << "{ ";
  for (auto i = 0; i < n; ++i) {
    //std::cout << "    " << lab[i] << " , " << i << std::endl;
    if (layout & nauty2layout(lab[i])) {
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

uint64_t get_orbits(uint64_t layout, int *orbits) {
  graph g[MAXN*MAXM], cg[MAXN*MAXM];
  int lab[MAXN],ptn[MAXN];
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
    if((layout & node2layout(j)) != 0) {
      ADDONEARC(g, node2nauty(j),node2nauty(j),m);
    }

    for (auto i = 1; i <= num_nodes; i<<=1) {
      if ((i & j) != 0 && i != j) {
	ADDONEARC(g,node2nauty(i),node2nauty(j),m);
      }
    }
  }

  densenauty(g,lab,ptn,orbits,&options,&stats,m,n,cg);

  if (stats.errstatus) {
    std::cout << "ERROR: " << stats.errstatus;
  }

  //recurse_print_orbit(orbits, 0);
  //std::cout << std::endl;

  //std::cout << "{ ";
  for (auto i = 0; i < n; ++i) {
    if (layout & nauty2layout(lab[i])) {
      if (newlayout & nauty2layout(i)) {
	std::cout << "eff dat" << std::endl;
      }

      //std::cout << "( " << nauty2node(lab[i]) << ", " << nauty2node(i) << " ) ";
      newlayout |= nauty2layout(i);
    }
  }

  //std::cout << " } ";

  return newlayout;
}
