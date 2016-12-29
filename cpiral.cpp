#include "types.h"
#include <iostream>
#include <forward_list>

#ifdef SPIRALNAUTY
#include "canonicalizer.h"
#endif

int main (int argc, char** argv) {
  // process args and initialize
  //GlobalStats::setN(2);

#ifdef SPIRALNAUTY
  init_canon(GlobalStats::N, GlobalStats::M);
#endif

  std::cout << GlobalStats::N << " of " << GlobalStats::M << std::endl;

  layout_lookup temp1, temp2;

  temp1[0] = Layout();

#ifdef BENCH
  for (GlobalStats::nodesInLayout = 0; GlobalStats::nodesInLayout < BENCH; ++GlobalStats::nodesInLayout) {
#else
  for (GlobalStats::nodesInLayout = 0; GlobalStats::nodesInLayout < GlobalStats::M; ++GlobalStats::nodesInLayout) {
#endif
    layout_lookup& curr = (GlobalStats::nodesInLayout % 2) ? temp2 : temp1;
    layout_lookup& next = (GlobalStats::nodesInLayout % 2) ? temp1 : temp2;

    next.clear();

    // for each layout in each metalist generate the next layer of layouts and calculate their scores
    for (auto it = curr.begin(); it != curr.end();) {
#ifdef SPIRALNAUTY
      (*it).second.generateNautyLayouts(next);
#else
      (*it).second.generateLayouts(next);
#endif

      it = curr.erase(it);
    }
  }

#ifndef BENCH
  for (auto l : temp1) {
    l.second.normalize();
    std::cout << l.second;
  }

  for (auto l : temp2) {
    l.second.normalize();
    std::cout << l.second;
  }
#endif

  return 0;
}
