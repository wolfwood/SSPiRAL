#include <vector>
#include <unordered_map>

#include <iostream>

class Layout;

typedef ulong node_t;    // this needs >= N bits
typedef ulong layout_t;  // needs >= M bits
typedef ulong score_t;   // enough bits to hold M choose M/2 ?

typedef std::unordered_map<layout_t, Layout> layout_lookup;

struct GlobalStats {
 public:
  static node_t N;
  static node_t M;

  static node_t nodesInLayout;

  static void setN(node_t n);
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
  std::vector<score_t> score;

 public:
  void grow(score_t i);
  void add(Score& s);
  void addMul(Score& s, score_t multiplier);
  void normalize();

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
  bool recurseCheck(layout_t n, node_t i = 1);


  void normalize() {score.normalize();}
  friend std::ostream& operator<<(std::ostream& os, const Layout& l);
};

struct MetaLayout : GlobalStats {
  std::unordered_map<layout_t, Layout> layouts;
};
