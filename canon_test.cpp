#include "canonicalizer.h"
#include <iostream>

int main() {
  auto data_nodes = 4;
  auto total_nodes = 15;

  init_canon(data_nodes, total_nodes);

  uint64_t limit = (1 << total_nodes) -1;

  for(auto i = 1; i <= limit; ++i) {
    auto foo = canonicalize(i);
    //std::cout << foo << std::endl;
    std::cout << i << " - " << foo << std::endl;
  }

  return 0;
}
