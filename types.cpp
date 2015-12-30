#include <cassert>
#include "types.h"

ulong GlobalStats::N=0;
ulong GlobalStats::M=0;
ulong GlobalStats::nodesInLayout=0;

void GlobalStats::setN(ulong n){
  if (n > 1 && n < 7) {
    N = n;
    M = (1 << n) - 1;
    nodesInLayout = 0;
  } else {
    assert(false); // This is not an appropriate value for N
  }
}

DataNode::DataNode(ulong n) {
  node = n;
}

void Score::grow(ulong i) {
  score.push_back(i);
}

Layout::Layout(ulong name, Layout l) : nodes(name), score(l.score) {
  if (l.alive) {
    alive = true;
  } else {
    alive = checkIfAlive();
  }

  if (alive) {
    score.grow(1);
  } else {
    score.grow(0);
  }
}

Layout::Layout() : score() {
  nodes = 0;
  alive = false;
}

bool Layout::checkIfAlive() {
  for(ulong n = 1; n <= M; n <<= 1) {
    ulong l = 1 << (n - 1);

    if ((l & nodes) == 0) {
      if (!recurseCheck(n)) {
	return false;
      }
    }
  }

  return true;
}

bool Layout::recurseCheck(ulong n, ulong i) {
  for(; i <= M; ++i) {
    ulong l = 1 << (i - 1);

    if (l & nodes) {
      ulong temp = i ^ n;

      if (temp == 0 || recurseCheck(temp, i+1)) {
	return true;
      }
    }
  }

  return false;
}

void Layout::generateLayouts(layout_lookup& ll) {
  score.normalize();

  for ( ulong n = 1; n <= ~(~(0UL) << M); n <<= 1) {
    ulong temp = nodes | n;

    if (temp != nodes) {
      auto result = ll.find(temp);

      if (result != ll.end()) {
	result->second.score.add(score);
      } else {
	ll.emplace(std::piecewise_construct,
		   std::forward_as_tuple(temp),
		   std::forward_as_tuple(temp, *this));

	//ll.emplace(temp, Layout(temp, *this));
      }
    }
  }
}

std::ostream& operator<<(std::ostream& os, const Layout& l) {
  os << l.score;

  return os;
}

void Score::add(Score& s) {
  auto it = score.begin();
  auto o_it = s.score.begin();

  for (;it != score.end() && o_it != s.score.end();) {
    *it += *o_it;

    ++it;
    ++o_it;
  }
}

std::ostream& operator<<(std::ostream& os, const Score& s) {
  ulong i = 0;
  for (auto sco = s.score.crbegin(); sco != s.score.crend(); ++sco) {
    os << i << " " << *sco << std::endl;
    ++i;
  }

  os << i << " 1" << std::endl;

  return os;
}

void Score::normalize() {
  if (score.size() > 3) {
    for(ulong i = 1; i < score.size(); ++i) {
      score[score.size() - i - 1] /= i;
    }
  }
}
