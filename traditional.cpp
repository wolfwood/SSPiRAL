#include "types.h"
#include <iostream>
#include <forward_list>

int main (int argc, char** argv) {
  // process args and initialize
  GlobalStats::setN(2);

  std::cout << GlobalStats::N << " of " << GlobalStats::M << std::endl;

  layout_lookup temp1, temp2;

#ifdef GOOGLE
  temp1.set_empty_key(0xFFFFFFFF);
  temp2.set_empty_key(0xFFFFFFFF);
  temp1.set_deleted_key(0xFFFFFFFE);
  temp2.set_deleted_key(0xFFFFFFFE);
#endif


  temp1[0] = Layout();

  for (GlobalStats::nodesInLayout = 0; GlobalStats::nodesInLayout < GlobalStats::M; ++GlobalStats::nodesInLayout) {
    layout_lookup& curr = (GlobalStats::nodesInLayout % 2) ? temp2 : temp1;
    layout_lookup& next = (GlobalStats::nodesInLayout % 2) ? temp1 : temp2;

    next.clear();

    // for each layout in each metalist generate the next layer of layouts and calculate their scores
    for (auto it = curr.begin(); it != curr.end();) {
      (*it).second.generateLayouts(next);
      //std::cout << "do stufff" <<std::endl;

#ifdef GOOGLE
      curr.erase(it);
      ++it;
#else
      it = curr.erase(it);
#endif
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
