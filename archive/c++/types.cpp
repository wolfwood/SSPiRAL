/*
#ifndef SPIRAL_N
#define SPIRAL_N 4
#endif
*/

#include <cassert>
#include "types.h"
#include <limits>

const node_t GlobalStats::N=SPIRAL_N ;
const node_t GlobalStats::M=MfromN(N);
node_t GlobalStats::nodesInLayout=0;

/*void GlobalStats::setN(node_t n){
  if (n > 1 && n < 7) {
    N = n;
    M = MfromN(N);
    nodesInLayout = 0;
  } else {
    assert(false); // This is not an appropriate value for N
  }
  }*/

void Score::grow(score_t alive, score_t cnt) {
  score.emplace_back(alive, cnt);
#ifdef COUNT
  count.emplace_back(1);
#endif
}

void Score::grow(bool live, score_t cnt) {
  if (live) {
    score.emplace_back(cnt, cnt);
  } else {
    score.emplace_back(0, cnt);
  }
#ifdef COUNT
  count.emplace_back(1);
#endif
}

score_t Score::copies() {
  if (score.size() == 0) {
    return 1;
  } else {
    return score.back().second;
  }
}

Layout::Layout(layout_t name, Layout l, score_t copies) : nodes(name), score(l.score) {
  score.mul(copies);

  if (l.alive) {
    alive = true;
  } else {
    alive = checkIfAlive();
  }

  score.grow(alive, copies);
}

Layout::Layout() : score() {
  nodes = 0;
  alive = false;
}

bool Layout::checkIfAlive() {
#if 1
  // information theory says no
  if (GlobalStats::N > GlobalStats::nodesInLayout+1) {
    return false;
  }
#endif

  for(node_t n = 1; n <= GlobalStats::M; n <<= 1) {
    layout_t l = layout_t(1) << (n - 1);

    if ((l & nodes) == 0) {
      if (!iterativeSimpleCheck(n)) {
      //if (!iterativeCheck(n)) {
	//assert(!recurseCheck(n));
	//assert(!iterativeCheck(n));
	return false;
      } else {
	//assert(recurseCheck(n));
	//assert(iterativeCheck(n));
      }
    }
  }

  return true;
}

bool Layout::iterativeCheck(node_t n) {
  // lazy so over allocate and use depth as a sentinel
  node_t Is[GlobalStats::nodesInLayout+2];
  int64_t depth = 1;
  node_t i = 0;
  node_t temp = n;

  while (0 < depth) {
    ++i;
    layout_t l = layout_t(1) << (i - 1);

    if (l & nodes) {
      Is[depth] = i;
      temp ^= i;

      /*
      node_t t = n;
      for (node_t j = 1; j <= depth; ++j) {
	t ^= Is[j];
      }

      assert(t == temp);
      */
      if (temp == 0) {
	return true;
      }

      if (GlobalStats::M > i ){//&& depth < GlobalStats::N) {
	++depth;
	Is[depth] = 0;
      } else {
	temp ^= Is[depth]; // undo the use of this node
	--depth;
	temp ^= Is[depth]; // undo the use of this node
	i = Is[depth];
      }
    } else if (GlobalStats::M == i) {
      --depth;
      temp ^= Is[depth]; // undo the use of this node
      i = Is[depth];
    }
  }

  return false;
}

bool Layout::iterativeSimpleCheck(node_t n) {
  node_t Is[GlobalStats::nodesInLayout+1];
  int64_t depth = 0;
  Is[depth] = 0;

  while (0 <= depth) {
    assert(GlobalStats::nodesInLayout+1 >= depth);
    ++Is[depth];
    layout_t l = layout_t(1) << (Is[depth] - 1);

    if (l & nodes) {
      node_t temp = n;
      for (node_t i = 0; i <= depth; ++i) {
	temp ^= Is[i];
      }

      if (temp == 0) {
	return true;
      }

      if (GlobalStats::M > Is[depth]) {
	++depth;
	Is[depth] = Is[depth-1];
      } else {
	--depth;
      }
    } else if (GlobalStats::M == Is[depth]) {
      --depth;
    }
  }

  return false;
}

bool Layout::recurseCheck(node_t n, node_t i) {
  for(; i <= GlobalStats::M; ++i) {
    layout_t l = layout_t(1) << (i - 1);

    if (l & nodes) {
      layout_t temp = i ^ n;

      if (temp == 0 || recurseCheck(temp, i+1)) {
	return true;
      }
    }
  }

  return false;
}

void Layout::generateLayouts(layout_lookup& ll) {
#ifndef DENORMALIZE
  score.normalize();
#endif

  for ( layout_t n = 1; n <= layout_t(~(std::numeric_limits<layout_t>::max() << GlobalStats::M)); n <<= 1) {
   layout_t temp = nodes | n;

    if (temp != nodes) {
      auto result = ll.find(temp);

      if (result != ll.end()) {
	result->second.score.add(score);
      } else {
#ifdef GOOGLE
	ll.insert(std::pair<layout_t, Layout>(temp, Layout(temp, *this)));
#else
#ifdef SPARSEPP
	ll.insert(std::pair<layout_t, Layout>(temp, Layout(temp, *this)));
#else
	ll.emplace(std::piecewise_construct,
		   std::forward_as_tuple(temp),
		   std::forward_as_tuple(temp, *this));
#endif
#endif

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

#ifdef COUNT
  auto c_it = count.begin();
  auto c_o_it = s.count.begin();
#endif

  for (;it != score.end() && o_it != s.score.end();) {
    it->first += o_it->first;
    it->second += o_it->second;

    ++it;
    ++o_it;

#ifdef COUNT
    *c_it += *c_o_it;
    ++c_it;
    ++c_o_it;
#endif
  }
}

void Score::addMul(Score& s, score_t multiplier) {
  auto it = score.begin();
  auto o_it = s.score.begin();

#ifdef COUNT
  auto c_it = count.begin();
  auto c_o_it = s.count.begin();
#endif
  for (;it != score.end() && o_it != s.score.end();) {
    it->first += o_it->first * multiplier;
    it->second += o_it->second * multiplier;

    ++it;
    ++o_it;

#ifdef COUNT
    *c_it += *c_o_it;
    ++c_it;
    ++c_o_it;
#endif
  }
}

void Score::mul(score_t multiplier) {
  if (multiplier > 1) {
    for (auto &s : score) {
      s.first *= multiplier;
      s.second *= multiplier;
    }
  }
}

std::ostream& operator<<(std::ostream& os, const Score& s) {
  node_t i = 0;

#ifdef COUNT
  auto c = s.count.crbegin();
#endif
  for (auto sco = s.score.crbegin(); sco != s.score.crend(); ++sco) {
    os << score_t(i) << " " << sco->first << " " << sco->second << " ";

#ifdef COUNT
    os << *c;
    ++c;
#endif
    os << std::endl;
    ++i;
  }

  os << score_t(i) << " 0 1" << std::endl;

  return os;
}

void Score::normalize() {
  //if (score.size() > 3) {
  for(node_t i = 1; i < score.size(); ++i) {
    score[score.size() - i - 1].first /= i;
    score[score.size() - i - 1].second /= i;
  }
  //}

  /*if (score.size()) {
    score.back().first /= count;
    score.back().second /= count;
    }*/
}
