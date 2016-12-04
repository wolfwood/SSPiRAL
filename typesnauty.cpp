#include "types.h"
#include "canonicalizer.h"

void Layout::generateNautyLayouts(layout_lookup& ll) {
  score.normalize();

  int orbits[MAXN];
  get_orbits(nodes, orbits);

  node_t k = M;
  for (node_t i = 0; i < M; ++i) {
    layout_t temp = nodes | nauty2layout(orbits[i]);
    //std::cout << nauty2node(i) << " " << nauty2node(orbits[i]) << std::endl;

    if ((temp != nodes) && (k == M || orbits[k] < orbits[i])) {
      score_t multiplier = 1;
      k = i;

      for (node_t j = i+1; j < M; ++j) {
	if (orbits[j] == orbits[i]) {
	  ++multiplier;
	}
      }

      //if (GlobalStats::nodesInLayout) {
	multiplier *= score.copies();

	//multiplier /= GlobalStats::nodesInLayout;
      //}
      //multiplier = score.copies();
      //std::cout << nodes << " " << temp << " : " << multiplier << std::endl;

      auto result = ll.find(temp);

      if (result != ll.end()) {
	result->second.score.addMul(score, multiplier);
      } else {
	ll.emplace(std::piecewise_construct,
		   std::forward_as_tuple(temp),
		   std::forward_as_tuple(temp, *this, multiplier));
      }
    }
  }
}
