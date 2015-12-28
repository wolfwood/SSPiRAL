#include "types.h"
#include <iostream>
#include <forward_list>

int main (int argc, char** argv) {
  // process args and initialize
  GlobalStats::setN(3);

  // -- collection of metalayouts
  std::forward_list<MetaLayout> markov;

  // initialize metalayout for layouts of 1 node



  for (GlobalStats::nodesInLayout = 0; GlobalStats::nodesInLayout <= GlobalStats::M; GlobalStats::nodesInLayout++) {
    layout_lookup next;

    // for each layout in each metalist generate the next layer of layouts and calculate their scores
    for (auto m : markov) {
      m.generateLayouts(next);
    }

    // generate score to metalayout mapping
    for (auto l : next) {

    }

    // sort markov by reverse score order, then print markov nodes

  }

  return 0;
}
