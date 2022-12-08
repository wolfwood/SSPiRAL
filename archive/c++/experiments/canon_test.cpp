#include "canonicalizer.h"
#include <iostream>
#include "types.h"

int main() {
  auto data_nodes = SPIRAL_N;
  auto total_nodes = MfromN(data_nodes);

  init_canon(data_nodes, total_nodes);

  uint64_t limit = (1 << total_nodes) -1;

  for(uint64_t i = 0; i <= limit; ++i) {
    auto foo = canonicalize(i);
    //std::cout << foo << std::endl;
    std::cout << i << " - " << foo << " : " << std::endl;
  }

  return 0;
}
