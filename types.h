#if !defined(cpiral_types_h_guard_)
#define cpiral_types_h_guard_

#include <vector>
#include <iostream>

class Layout;

typedef uint8_t node_t;    // this needs >= N bits
#if SPIRAL_N < 4
typedef uint8_t layout_t;  // needs >= M bits
#ifndef DENORMALIZE
typedef uint16_t score_t;
#else
typedef uint16_t score_t;   // enough bits to hold M choose M/2 ?
#endif
#else
#if SPIRAL_N == 4
typedef uint16_t layout_t;  // needs >= M bits
#ifndef DENORMALIZE
typedef uint16_t score_t;
#else
typedef uint32_t score_t;   // enough bits to hold M choose M/2 ?
#endif
#else
#if SPIRAL_N == 5
typedef uint32_t layout_t;  // needs >= M bits
typedef uint32_t score_t;   // enough bits to hold M choose M/2 ?
#else
#if SPIRAL_N == 6
typedef uint64_t layout_t;  // needs >= M bits
typedef uint64_t score_t;   // enough bits to hold M choose M/2 ?
#else
#error SPIRAL_N must be <= 6 for now
#endif
#endif
#endif
#endif



#ifdef GOOGLE
#include <sparsehash/dense_hash_map>
typedef google::dense_hash_map<layout_t, Layout> layout_lookup;
#else
#ifdef SPARSEPP
#include "sparsepp/sparsepp.h"
typedef spp::sparse_hash_map<layout_t, Layout> layout_lookup;
#else
//#include <unordered_map>
#include <map>
typedef std::map<layout_t, Layout> layout_lookup;
#endif
#endif

struct GlobalStats {
 public:
  static const node_t N;
  static const node_t M;

  static node_t nodesInLayout;

  //static void setN(node_t n);
};
/*
struct DataNode : GlobalStats {
 private:
  ulong node;

 public:
  DataNode(ulong n);
};
*/
class Score : GlobalStats {
 private:
  std::vector<std::pair<score_t, score_t>> score;

#ifdef COUNT
  std::vector<score_t> count;
#endif

 public:
  void grow(score_t live, score_t count);
  void grow(bool alive, score_t count);
  void add(Score& s);
  void addMul(Score& s, score_t multiplier);
  void mul(score_t multiplier);
  void normalize();
  score_t copies();

  friend std::ostream& operator<<(std::ostream& os, const Score& s);
};

struct Layout : GlobalStats {
 private:
  layout_t nodes;
  Score score;
  bool alive;

 public:
  Layout();
  Layout(layout_t name, Layout l, score_t copies = 1);
  void generateLayouts(layout_lookup& ll);
  void generateNautyLayouts(layout_lookup& ll);

  bool checkIfAlive();
  bool recurseCheck(node_t n, node_t i = 1);
  bool iterativeCheck(node_t n);
  bool iterativeSimpleCheck(node_t n);

  void normalize() {score.normalize();}
  friend std::ostream& operator<<(std::ostream& os, const Layout& l);
};

struct MetaLayout : GlobalStats {
  std::unordered_map<layout_t, Layout> layouts;
};
#endif
