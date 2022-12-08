#include <iostream>
#include <cassert>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

int main (int argc, char** argv) {
  assert(argc == 2);

  uint64_t val = atoll(argv[1]);

  if (errno) perror(NULL);

  std::cout << argv[1] << ":" << std::endl;
  for (uint64_t i = 0; i < sizeof(val)*8ULL;++i) {
    if (val & (1ULL << i)) {
      std::cout << " " << i+1;
    }
  }
  std::cout << std::endl;
}
