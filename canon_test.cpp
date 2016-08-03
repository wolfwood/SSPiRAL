#include "canonicalizer.h"
#include <iostream>

int main() {
  auto data_nodes = 3;
  auto total_nodes = 7;

  init_canon(data_nodes, total_nodes);

  uint64_t limit = (1 << total_nodes) -1;

  for(uint64_t i = 1; i <= limit; ++i) {
    auto foo = di_canonicalize(i);
    //std::cout << foo << std::endl;
    std::cout << i << " - " << foo << std::endl;
  }

  return 0;
}
