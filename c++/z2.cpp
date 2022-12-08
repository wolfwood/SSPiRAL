//
// Created by wolfwood on 8/29/19.
//

#include <bits/stdc++.h>

#include <typeinfo>

typedef uint8_t  Node;
typedef uint32_t LayoutName;
typedef uint32_t SubScore;
typedef uint8_t  ScoreIndex;

typedef LayoutName Combinadic;


const Node N = 5;
const Node M = (1 << N) - 1;
const ScoreIndex ML_SIZE = 131 + 1;
const Node SCORE_SIZE = (((M+1) / 2) - N);

template<int dim = (M+1)>
using LUT = std::array<std::array<Combinadic,dim>,dim>;

constexpr LayoutName node2layout(const Node n) {
  return (static_cast<LayoutName>(1)) << (n - 1);
}


static constexpr auto binCoeffsGen() {
  LUT<M+1> lut = {0};

  lut[0][0] = 1;

  for (Combinadic i = 1; i <= M; ++i) {
    lut[i][0] = 1;
    lut[i][i] = 1;

    for (Combinadic j = 1; j < i; ++j) {
      lut[i][j] = lut[i - 1][j] + lut[i - 1][j - 1];
    }
  }

  return lut;
}

//static constexpr Combinadic coeffs[M+1][M+1] = {binCoeffsGen()};
static constexpr auto coeffs = binCoeffsGen();

constexpr Combinadic binomialCoeff(const Combinadic n, const Combinadic k) {
  /*if constexpr (true) {
    Combinadic val = 1;

    if (k > n - k) {
      k = n - k;
    }

    for (Combinadic i = 0; i < k; ++i) {
      val *= n - i;
      val /= i + 1;
    }

    return val;
  } else {*/
  return coeffs[(int)n][(int)k];
}


// Work Product, carried between iterations
template<Node limit, bool verify = false, Combinadic layout_count = binomialCoeff(M, limit), Node score_length = SCORE_SIZE /*limit - N < SCORE_SIZE ? limit - N + 1: SCORE_SIZE*/>
class WorkContext {
  //using Scores = SubScore[score_length];
  using Scores = std::array<SubScore, score_length>;

  struct LayoutFast {
    ScoreIndex idx;
  };

  struct LayoutCorrect : LayoutFast {
    LayoutName name;
  };

  typedef std::conditional_t<verify, LayoutCorrect, LayoutFast> Layout;

  Layout layouts[layout_count];
  Scores unique_scores[ML_SIZE];

  ScoreIndex curr_score;
  Combinadic curr_layout;

  std::map<Scores, ScoreIndex> unique;

public:
  static constexpr auto score_len = score_length;


  WorkContext() : curr_score(0), curr_layout(0), unique() {};

  //Scores& operator()(ScoreIdx idx) {return unique_scores[idx];}
  //const Scores& operator()(ScoreIdx idx) const {return unique_scores[idx];}

  /*
  void normalize() {
    if constexpr (limit <= M/2) {
      for (ScoreIndex k = 0; k < curr_score; ++k) {
	for (int j = 2; j <= (limit - N); ++j) {
	  assert(0 == unique_scores[k][limit - N - j] % j);
	  unique_scores[k][limit - N - j] /= j;
	}
      }
    } else {
      for (ScoreIndex k = 0; k < curr_score; ++k) {
	for (int j = 0; j < score_length; ++j) {
	  uint adjustment = limit - (M/2);
	  assert(0 == unique_scores[k][score_length - j - 1] % (j + adjustment));
	  unique_scores[k][score_length - j - 1] /= (j + adjustment);
	}
      }
`    }
  }*/

  template<Node lim = limit, typename = std::enable_if_t<(lim > N) && lim <= M/2>>
  void normalize() {
    for (ScoreIndex k = 0; k < curr_score; ++k) {
      for (int j = 2; j <= limit - N /*< score-length*/; ++j) {
	assert(0 == unique_scores[k][limit - N - j] % j);
	unique_scores[k][limit - N - j] /= j;
      }
    }
  }

  template<Node lim = limit>
  typename std::enable_if_t<(lim > M/2)>
  normalize() {
    for (ScoreIndex k = 0; k < curr_score; ++k) {
      for (int j = 0; j < score_length; ++j) {
	uint adjustment = limit - (M/2);
	assert(0 == unique_scores[k][score_length - j - 1] % (j + adjustment));
	unique_scores[k][score_length - j - 1] /= (j + adjustment);
      }
    }
  }

  Scores& score(){
    return unique_scores[curr_score];
  }

  Scores& operator[](const Combinadic idx) {
    return unique_scores[layouts[idx].idx];
  }

  const Scores& operator[](const Combinadic idx) const {
    return unique_scores[layouts[idx].idx];
  }

  /*Layout& layout(){
    return layouts[curr_layout];
    }*/

  void operator++() {
    auto result = unique.find(unique_scores[curr_score]);

    if (unique.end() != result) {
      layouts[curr_layout].idx = (*result).second;
    } else {
      layouts[curr_layout].idx = curr_score;
      unique.emplace(unique_scores[curr_score], curr_score);
      ++curr_score;
    }

    ++curr_layout;
  }

  template<Node lim = limit, typename = std::enable_if_t<lim == M>>
  void print() const {
    for (int j = 0; j < ((M + 1) / 2); ++j) {
      uint64_t total = binomialCoeff(M, j);
      printf("%d %lu %lu\n", j, total, total);
    }

    for (int j = SCORE_SIZE - 1; j >= 0; --j) {
      uint64_t total = binomialCoeff(M, (M / 2) + SCORE_SIZE - j);
      printf("%d %lu %lu\n", (M / 2) + SCORE_SIZE - j, total - unique_scores[0][j], total);
    }

    for (int j = M - N + 1; j <= M; ++j) {
      printf("%d %u %u\n", j, 0, binomialCoeff(M, j));
    }
  }
};



// most of the work of an various iterations, plus the state that doesn't live beyond it
template<Node limit, bool verify = false>
class Iterator {
protected:
  WorkContext<limit,verify>& curr;

  virtual void walk() = 0;
  virtual void oneStep() = 0;

public:
  Iterator(WorkContext<limit,verify>& c) : curr(c) {};

  virtual void operator()() = 0;
};


template<Node limit, bool verify = false>
class NamedIterator : public Iterator<limit, verify> {
protected:
  Node Is[limit + 1];

  using Iterator<limit, verify>::curr;

  bool deadnessCheck() const {
    int j = limit;

    //  for(node_t n = 1 << (N-1); n > 0; n >>= 1) {
    for (Node n = 1; n < M; n <<= 1) {
      while (j > 1 && (Is[j] + 1) < n) {
	--j;
      }

      if ((Is[j] + 1) != n) {
	Node temp = n;

	const Node SENTINEL = 3;
	Node Js[limit + 1];

	for (uint k = 1; k <= limit; ++k) {
	  Js[k] = SENTINEL;
	}

	// one greater than the highest acceptable value - gets fed into the next element
	Js[0] = 0;

	bool alive = false;
	uint i = 1;

	while (0 < i) {
	  if (0 == Js[i]) {
	    //temp ^= Is[i];
	    Js[i] = SENTINEL;
	    --i;
	  } else {
	    if (SENTINEL == Js[i]) {
	      Js[i] = 2;
	    }
	    temp ^= Is[i] + 1;

	    if (0 == temp) {
	      // continue outer loop
	      alive = true;
	      break;
	    }

 	    --Js[i];
	    if (limit > i) {
	      ++i;
	    }
	  }
	}

	if (!alive) {
	  return true;
	}
      }
    }

    return false;
  }

public:
  NamedIterator(WorkContext<limit,verify>& c) : Iterator<limit, verify>(c) {printf("Const NI\n");};
};


template <class T, class U>
concept bool Derived = std::is_base_of<U, T>::value;

template<Node limit, bool verify = false, Derived<Iterator<limit, verify>> T = NamedIterator<limit, verify>>
class SummingIterator: public T {
  using T::curr;
  const WorkContext<limit-1,verify>& prev;
  static constexpr bool named = std::is_same<NamedIterator<limit, verify>, T>::value;

  //std::enable_if_t<false, WorkContext<limit-1,verify>>& prev2;

  Combinadic index;
  Combinadic deltaCoeffs[limit];

  void sumChildLayoutScores() {
    auto& s = curr.score();

    Combinadic idx = index;

    auto p = prev[idx];

    for (uint i = 0; i < (prev.score_len/*limit -1 < SCORE_SIZE ? limit  - N : SCORE_SIZE*/); ++i) {
      s[i] = p[i];
    }

    for (uint j = 1; j < limit; ++j) {
      idx += deltaCoeffs[j];

      auto p = prev[idx];
      for (uint i = 0; i < prev.score_len; ++i) {
	s[i] += p[i];
      }
    }
  }

protected:
  void walk() override {
    //if constexpr (named) { using T::Is; }

    uint32_t befores[limit + 1][M + 1];
    uint32_t *bs[limit + 1];
    uint32_t *as[limit + 1];

    for (int i = M; i >= 0; --i) {
      for (int j = limit; j >= 0; --j) {
	befores[j][i] = binomialCoeff(i, limit - j);
      }
    }
    befores[limit][0] = 0;

    if constexpr (named) { NamedIterator<limit, verify>::Is[0] = M; }
    bs[0] = &befores[0][M];
    as[0] = ((uint32_t *)&befores[0][M]) - (M + 1);

    for (uint i = 1; i <= limit; ++i) {
      if constexpr (named) { NamedIterator<limit, verify>::Is[i] = NamedIterator<limit, verify>::Is[i - 1] - 1; }
      bs[i] = ((uint32_t *) bs[i - 1]) + M;
      as[i] = ((uint32_t *) as[i - 1]) + M;
      uint before = *bs[i - 1];
      uint after = *as[i];
      assert(before > after);
      deltaCoeffs[i - 1] = before - after;
      if (i > 1){ index += after; }
    }

    uint i = limit;
    while (true) {
      oneStep();

      if (0 == *bs[i]) {
	index -= *as[i];
	do {
	  --i;
	  if constexpr (named) { --NamedIterator<limit, verify>::Is[i]; }
	  --bs[i];
	  index -= *as[i];
	  --as[i];
	} while (0 == *bs[i] && 0 < i);
	if (0 == i) { break; }
	if (1 == i) {
	  ++i;
	  if constexpr (named) { NamedIterator<limit, verify>::Is[i] = NamedIterator<limit, verify>::Is[i - 1] - 1; }
	  bs[i] = ((uint32_t *) bs[i - 1]) + M;
	  as[i] = ((uint32_t *) as[i - 1]) + M;
	  deltaCoeffs[i - 1] = *bs[i - 1] - *as[i];
	  index = *as[i];
	} else {
	  deltaCoeffs[i - 1] = *bs[i - 1] - *as[i];
	  index += *as[i];
	}
	do {
	  ++i;
	  if constexpr (named) { NamedIterator<limit, verify>::Is[i] = NamedIterator<limit, verify>::Is[i - 1] - 1; }
	  bs[i] = ((uint32_t *) bs[i - 1]) + M;
	  as[i] = ((uint32_t *) as[i - 1]) + M;
	  deltaCoeffs[i - 1] = *bs[i-1] - *as[i];
	  index += *as[i];
	} while (i < limit);
      } else {
	if constexpr (named) { --NamedIterator<limit, verify>::Is[i]; }
	--bs[i];
	--as[i];
	++deltaCoeffs[i - 1];
	--index;
      }
    }
  }

  void oneStep() override {
    sumChildLayoutScores();

    if constexpr (named) {
      auto& s = curr.score();

      if (limit == s[limit - N - 1]) {
	s[/*curr.score_len - 1*/limit - N] = T::deadnessCheck();
      } else {
	s[limit - N] = 0;
      }
    }

    ++curr;
  }

public:
  SummingIterator(WorkContext<limit,verify>& c, const WorkContext<limit-1,verify>& p) : T(c), prev(p), index(0) {printf("Const SI\n");}

  void operator()() override{
    walk();
    curr.normalize();
  };
};

template<Node limit, bool verify = false>
class First : NamedIterator<limit, verify> {
protected:
  using NamedIterator<limit, verify>::curr;
  using NamedIterator<limit, verify>::deadnessCheck;

  void oneStep() override {
    if constexpr (verify) {
      if (deadnessCheck()) {
	curr.score()[0] = 1;
      } else {
	curr.score()[0] = 0;
	printf("%d", curr.score()[0]);
      }
    } else {
      curr.score()[0] = deadnessCheck();
    }

    ++curr;
  }

  using NamedIterator<limit, verify>::Is;

  /*
    std::enable_if_t<false>
  walk() override {
    printf("W\n");

    uint i = 1;
    const Node SENTINEL = 0;

    for (uint i = 1; i <= limit; ++i) {
      Is[i] = SENTINEL;
    }

    // one greater than the highest acceptable value - gets fed into the next element
    Is[0] = M;

    while (0 < i) {
      if (SENTINEL == Is[i]) {
	Is[i] = Is[i - 1];
      }
      --Is[i];

      if (limit == i) {
	oneStep();
      }

      if (0 == Is[i]) {
	--i;
      } else if (limit > i) {
	++i;
      }
    }
    }*/


  void walk() override {
    uint32_t befores[limit + 1][M + 1];
    uint32_t *bs[limit + 1];

    for (int i = M; i >= 0; --i) {
      for (int j = limit; j >= 0; --j) {
	befores[j][i] = binomialCoeff(i, limit - j);
      }
    }
    befores[limit][0] = 0;

    NamedIterator<limit, verify>::Is[0] = M;
    bs[0] = &befores[0][M];

    for (uint i = 1; i <= limit; ++i) {
      NamedIterator<limit, verify>::Is[i] = NamedIterator<limit, verify>::Is[i - 1] - 1;
      bs[i] = ((uint32_t *) bs[i - 1]) + M;
    }

    uint i = limit;
    while (true) {
      oneStep();

      if (0 == *bs[i]) {
	do {
	  --i;
	  --NamedIterator<limit, verify>::Is[i];
	  --bs[i];
	} while (0 == *bs[i] && 0 < i);
	if (0 == i) { break; }
	if (1 == i) {
	  ++i;
	  NamedIterator<limit, verify>::Is[i] = NamedIterator<limit, verify>::Is[i - 1] - 1;
	  bs[i] = ((uint32_t *) bs[i - 1]) + M;
	}
	do {
	  ++i;
	  NamedIterator<limit, verify>::Is[i] = NamedIterator<limit, verify>::Is[i - 1] - 1;
	  bs[i] = ((uint32_t *) bs[i - 1]) + M;
	} while (i < limit);
      } else {
	--NamedIterator<limit, verify>::Is[i];
	--bs[i];
      }
    }
  }


public:
  First(WorkContext<limit,verify>& c) : NamedIterator<limit, verify>(c) { printf("con\n");};


  void operator()() override{
    printf("F\n");
    walk();
  };
};

template<Node limit, bool verify = true>
requires (verify)
class FirstNamed : First<limit, verify> {
private:
  LayoutName name;

  static bool recurseCheck(LayoutName name, Node n, Node i) {
    for(; i <= M; ++i) {
      LayoutName l = ((LayoutName)1) << (i - 1);

      if (l & name) {
	Node temp = i ^ n;

	if (temp == 0 || recurseCheck(name, temp, static_cast<Node>(i+1))) {
	  return true;
	}
      }
    }

    return false;
  }

  bool checkIfAlive() const {
    for (Node n = 1; n <= M; n <<= 1) {
      LayoutName l = node2layout(n);

      if ((l & name) == 0) {
        if (!recurseCheck(name, n, static_cast<Node>(1))) {
          return false;
        }
      }
    }

    return true;
  }

protected:
  using First<limit, verify>::curr;
  using First<limit, verify>::deadnessCheck;

  void oneStep() override {
    bool ded = deadnessCheck();
    curr.score()[0] = ded;

    assert(ded  == (!checkIfAlive()));

    if (!ded) {
      printf("%d", curr.score()[0]);
    }

    ++curr;
  }

  using First<limit, verify>::Is;

  void walk() override {
    LayoutName ells[limit+1] = {0};
    uint i = 1;
    Is[0] = M;
    ells[0] = node2layout(M) << 1;

    while (0 < i) {
      if(0 == ells[i]) {
	ells[i] = ells[i-1];
	Is[i] = Is[i-1];
      } else {
	name ^= ells[i];
      }
      ells[i] >>= 1;
      --Is[i];

      name |= ells[i];

      if (limit == i) {
	oneStep();
      }

      if(1 == ells[i]) {
	name ^= ells[i];
	ells[i] = 0;
	--i;
      } else {
	if (limit > i) {
	  ++i;
	}
      }
    }
  }

public:
  FirstNamed(WorkContext<limit,verify>& c) : First<limit, verify>(c), name(0) { printf("con nam\n");};

  using First<limit, verify>::operator();
};

// Actual Types
template<Node limit, bool verify = false>
using FirstBlush = std::conditional_t<verify, FirstNamed<limit, true>, First<limit, verify>>;

template<Node limit, bool verify = false>
using IntermediateZone = SummingIterator<limit, verify, NamedIterator<limit, verify>>;

template<Node limit, bool verify = false>
using TerminalZone = SummingIterator<limit, verify, Iterator<limit, verify>>;


template<bool verify = false, Node limit = N>
requires (limit == N)
void rollup() {
  printf("=5\n");

  auto* curr = new(WorkContext<limit, verify>);

  {// make+run FirstBlush iterator
    auto f = FirstBlush<limit, verify>(*curr);f();
  }

  //printf("\n");
  rollup<verify, limit+1>(curr);
}

template<bool verify = false, Node limit>
requires (N < limit && limit <= M / 2)
void rollup(const WorkContext<limit-1, verify>* prev) {
  printf("-%d\n", limit);

  auto* curr = new(WorkContext<limit, verify>);

  {// make+run Intermediate phase iterator w/ curr and prev
    auto i = IntermediateZone<limit, verify>(*curr, *prev);i();
    delete prev;
  }

  rollup<verify, limit + 1>(curr);
}

template<bool verify = false, Node limit>
requires (M / 2 < limit && limit <= M)
void rollup(const WorkContext<limit-1, verify>* prev) {
  printf("%d\n", limit);

  auto* curr = new(WorkContext<limit, verify>);

  {// make+run Terminal phase iterator w/ curr and prev
    auto t = TerminalZone<limit, verify>(*curr, *prev);t();
    delete prev;
  }

  if constexpr (limit < M) {
    rollup<verify, limit + 1>(curr);
  } else {
    curr->print();
  }
}


// what it is
int main(){
  rollup<true>();
  //rollup<>();

  return 0;
}
