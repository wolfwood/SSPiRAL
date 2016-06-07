#include <inttypes.h>

void init_canon(uint64_t data_nodes, uint64_t all_nodes);
uint64_t canonicalize(uint64_t layout);
uint64_t di_canonicalize(uint64_t layout);

inline uint64_t node2nauty(uint64_t node);
inline uint64_t nauty2node(uint64_t nauty);
inline uint64_t node2layout(uint64_t node);
