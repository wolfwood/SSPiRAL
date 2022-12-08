#include "types.h"
#include <iostream>
#include <forward_list>
#include "canonicalizer.h"

int main (int argc, char** argv) {
  // process args and initialize
  GlobalStats::setN(5);

  init_canon(GlobalStats::N, GlobalStats::M);

  std::cout << GlobalStats::N << " of " << GlobalStats::M << std::endl;

  layout_lookup temp1, temp2;

  temp1[0] = Layout();

  for (GlobalStats::nodesInLayout = 0; GlobalStats::nodesInLayout < GlobalStats::M; ++GlobalStats::nodesInLayout) {
    layout_lookup& curr = (GlobalStats::nodesInLayout % 2) ? temp2 : temp1;
    layout_lookup& next = (GlobalStats::nodesInLayout % 2) ? temp1 : temp2;

    next.clear();

    std::cout << GlobalStats::nodesInLayout << std::endl;

    // for each layout in each metalist generate the next layer of layouts and calculate their scores
    for (auto it = curr.begin(); it != curr.end();) {
      (*it).second.generateNautyLayouts(next);
      //std::cout << "do stufff" <<std::endl;

      it = curr.erase(it);
    }
  }

  for (auto l : temp1) {
    l.second.normalize();
    std::cout << l.second;
  }

  for (auto l : temp2) {
    l.second.normalize();
    std::cout << l.second;
  }

  return 0;
}
