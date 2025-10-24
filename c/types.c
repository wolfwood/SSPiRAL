// functions for working with SSPiRAL data types

#include "types.h"

node_t MfromN(node_t n) { return ((node_t) 1 << n) - 1; }

layout_t node2layout(node_t n) {
  return ((layout_t) 1) << (n - 1);
}
