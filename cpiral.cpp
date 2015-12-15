#include "types.h"

int main (int argc, char** argv) {

  // process args and initialize
  GlobalStats::setN(3);

  // -- collection of metalayouts
  std::forward_List<MetaList> markov;

  // initialize metalayout for layouts of 1 node


  for (GlobalStats::nodesInLayout = 0; GlobalStats::nodesInLayout <= GlobalStats::M; GlobalStats::nodesInLayout++) {
    std::unordered_map<ulong, Layout> next;

    // for each layout in each metalist generate the next layer of layouts and calculate their scores
    for (auto m : markov) {

    }

    // generate score to metalayout mapping
    for (auto l : next) {

    }

    // sort markov by reverse score order, then print markov nodes

  }

  return 0;
}
`
