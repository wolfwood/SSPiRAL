//
// Created by wolfwood on 8/29/19.
//

#include <bits/stdc++.h>


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

constexpr Combinadic binomialCoeff(Combinadic n, Combinadic k) {
  {/*if constexpr (true) {
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
}


// Work Product, carried between iterations
template<Node limit, bool verify = false, Combinadic layout_count = binomialCoeff(M, limit), Node score_length = limit < SCORE_SIZE ? limit : SCORE_SIZE>
class WorkContext {
  //using Scores = SubScore[score_length];
  using Scores = std::array<SubScore, score_length>;

  struct Layout {
    using Ptr = ScoreIndex;

    /*if constexpr (verify) {
      LayoutName name;
    }*/
    Ptr idx;
  };

  Layout layouts[layout_count];
  Scores unique_scores[ML_SIZE];

  ScoreIndex curr_score;
  uint32_t curr_layout;

  std::map<Scores*, ScoreIndex> unique;

public:
  WorkContext() : curr_score(0), curr_layout(0), unique() {};

  //Scores& operator()(ScoreIdx idx) {return unique_scores[idx];}
  //const Scores& operator()(ScoreIdx idx) const {return unique_scores[idx];}



  void operator++() {
    auto result = unique.find(&unique_scores[curr_score]);

    if (unique.end() != result) {
      layouts[curr_layout].idx = (*result).second;
    } else {
      layouts[curr_layout].idx = curr_score;
      unique.emplace(&unique_scores[curr_score], curr_score);
      ++curr_score;
    }

    ++curr_layout;
  }
};


/*
// most of the work of an various iterations, plus the state that doesn't live beyond it
template<Node limit, bool named = false, bool verify = false>
class Iterator {

  WorkContext<limit,verify>& curr;

protected:
  // XXX
  void walk() {

  }

  // XXX
  virtual void oneStep() = 0;

public:
  Iterator(WorkContext<limit,verify>& c) : curr(c) {};


  virtual void operator()(){ walk(); };
};

template<Node limit, bool verify = false>
class NamedIterator : Iterator<limit, true, verify> {
  Node Is[limit + 1];

public:
  NamedIterator(WorkContext<limit,verify>& c) {super(c);};

  // XXX
  bool deadnessCheck() {

    return false;
  }

};

template<Node limit, bool named = false, bool verify = false, typename T = NamedIterator<limit, verify>>
class SummingIterator: T {
  const WorkContext<limit-1,verify>& prev;
  Combinadic deltaCoeffs[limit];

  // XXX
  void sumChildLayoutScores();

  // XXX
  void normalize();

public:
  SummingIterator(WorkContext<limit,verify>& c, const WorkContext<limit-1,verify>& p) : prev(p) {super(c);};

  void operator()(){ T::walk(); normalize();};
};


// Actual Types
template<Node limit, bool verify = false>
using FirstBlush = NamedIterator<limit, verify>;

template<Node limit, bool verify = false>
using IntermediateZone = SummingIterator<limit, true, verify, NamedIterator<limit, verify>>;

template<Node limit, bool verify = false>
using TerminalZone = SummingIterator<limit, false, verify, Iterator<limit, false, verify>>;
*/

template<Node limit, bool verify = false>
class BaseIterator{
protected:
  WorkContext<limit,verify>& curr;
  virtual void walk() = 0;
  virtual void oneStep() = 0;

public:
  BaseIterator(auto& c, auto& p = false) : curr(c) {};

  virtual void operator()() = 0;
};

template<Node limit, bool verify = false>
class NamedIterator : virtual public BaseIterator<limit, verify> {
  Node Is[limit + 1];

public:
  NamedIterator(auto& c, auto& p = false) : BaseIterator<limit, verify>(c, p) {};

  // XXX
  bool deadnessCheck() {

    return false;
  }
};

template<Node limit, bool verify = false>
class SummingIterator : virtual public BaseIterator<limit, verify> {
  Combinadic deltaCoeffs[limit];
  using BaseIterator<limit, verify>::curr;

  // XXX
  void sumChildLayoutScores();

protected:
  const WorkContext<limit-1,verify>& prev;

  // XXX
  void oneStep() override {
    ++curr;
  }
  // XXX
  void normalize(){};

public:
  SummingIterator(auto& c, const auto& p) : BaseIterator<limit, verify>(c, p), prev(p) {};

  //void operator()(){walk(); normalize();};
};

template<Node limit, bool verify = false>
class FirstIterator : virtual public NamedIterator<limit, verify> {
protected:
  // XXX
  void oneStep() override {}

public:
  FirstIterator(WorkContext<limit,verify>& c, auto & p = false) : NamedIterator<limit, verify>(c, p), BaseIterator<limit, verify> (c, p) {};
  //void operator()(){walk();}
};

/*
template<Node limit, bool named=true, bool summing=true>
class ProtoIterator : public NamedIterator<limit>, public SummingIterator<limit> {
public:
  //ProtoIterator(auto& c, const auto& p) : BaseIterator(c), SummingIterator(c, p) {};
};

template<Node limit, bool named = true, bool summing=false>
class ProtoIterator : public NamedIterator<limit> {}

template<Node limit, bool named=false, bool summing=true>
class ProtoIterator : public SummingIterator<limit> {}
*/

template <class T, class U>
concept bool Derived = std::is_base_of<U, T>::value;

//template <class T>
//concept bool Derived = true;

//template <class T, class U, class ...V>
//concept bool Derived = std::is_base_of<T, U>::value && Derived<T, V...>;

template< Node limit, bool verify = false, Derived<BaseIterator<limit, verify>> ...T>
class Iterator : virtual public T... {
  using T::oneStep...;
public:
  Iterator(auto& c, auto& p = false): T(c,p)..., BaseIterator<limit, verify>(c, p){}
  //Iterator() {};
  //Iterator(){}
  // XXX
  void walk() override {
    oneStep();
  };
  // XXX
  /*void operator()() {

  }*/
};

template<Node limit, bool verify = false>
class FirstBlush: public Iterator<limit, verify, NamedIterator<limit, verify>>{
  using BaseIterator<limit, verify>::curr;
protected:
  // XXX
  void oneStep() override {

    ++curr;
  };

public:
  FirstBlush(WorkContext<limit,verify>& c, auto& p) : Iterator<limit,verify, NamedIterator<limit, verify>>(c, p), NamedIterator<limit, verify>(c,p), BaseIterator<limit, verify>(c,p) {};
  void operator()() override {Iterator<limit,verify, NamedIterator<limit, verify>>::walk();}
};

template<Node limit, bool verify = false>
class IntermediateZone: public Iterator<limit, verify, NamedIterator<limit, verify>, SummingIterator<limit, verify>>{
public:
  IntermediateZone(auto& c, auto &p) : Iterator<limit, verify, NamedIterator<limit, verify>, SummingIterator<limit, verify>>(c, p), NamedIterator<limit, verify>(c, p), SummingIterator<limit, verify>(c, p), BaseIterator<limit, verify>(c, p) {};
  void operator()() override {Iterator<limit,verify, NamedIterator<limit, verify>, SummingIterator<limit, verify>>::walk(); SummingIterator<limit, verify>::normalize();}
};

template<Node limit, bool verify = false>
class TerminalZone: public Iterator<limit, verify, SummingIterator<limit, verify>>{
public:
  TerminalZone(auto& c, auto &p) : Iterator<limit, verify, SummingIterator<limit, verify>>(c, p), SummingIterator<limit, verify>(c, p), BaseIterator<limit, verify>(c, p) {};
  void operator()() override {Iterator<limit,verify, SummingIterator<limit, verify>>::walk();SummingIterator<limit, verify>::normalize();}
};

// computation
template<Node limit>
const auto rollup(const WorkContext<limit-1>* prev) {
  if constexpr (N == limit) {
    // assert?
  } else if constexpr (limit <= M / 2) {
    // make curr
    WorkContext<limit>* curr = new(WorkContext<limit>);

    // make+run Intermediate iterator w/ curr and prev
    {IntermediateZone<limit>(*curr, *prev)();}

    delete prev;

    return rollup<limit + 1>(curr);
  } else if constexpr (limit <= M) {
    // make curr
    WorkContext<limit>* curr = new(WorkContext<limit>);

    // make+run terminal iterator w/ curr and prev
    {TerminalZone<limit>(*curr, *prev)();}

    delete prev;

    if constexpr (limit < M) {
      return rollup<limit + 1>(curr);
    } else {
      // print?
      return curr;
    }
  }
}

template<Node limit = N>
const auto rollup() {
  // make curr
  WorkContext<limit>* curr = new(WorkContext<limit>);

  // make FirstBlush iterator
  {
    WorkContext<limit-1> p;
    FirstBlush<limit>(*curr, p)();
  }

  // run iterator on prev
  return rollup<limit+1>(curr);
}


// what it is
int main(){
  //WorkContext<N-1> prev;

  auto result = rollup();

  return 0;
}
