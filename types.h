#include <vector>
#include <unordered_map>

#include <iostream>

//typedef unsigned long long ulong;
class Layout;

typedef std::unordered_map<ulong, Layout> layout_lookup;

struct GlobalStats {
 public:
  static ulong N;
  static ulong M;

  static ulong nodesInLayout;

  static void setN(ulong n);
};

struct DataNode : GlobalStats {
 private:
  ulong node;

 public:
  DataNode(ulong n);
  DataNode getNext();
  bool hasNext();
};

class Score : GlobalStats {
 private:
  std::vector<ulong> score;

 public:
  void grow(ulong i);
  void add(Score& s);
  void normalize();

  friend std::ostream& operator<<(std::ostream& os, const Score& s);
};

struct Layout : GlobalStats {
 private:
  ulong nodes;
  Score score;
  bool alive;

 public:
  Layout();
  Layout(ulong name, Layout l);
  void generateLayouts(layout_lookup& ll);

  bool checkIfAlive();
  bool recurseCheck(ulong n, ulong i = 1);


  void normalize() {score.normalize();}
  friend std::ostream& operator<<(std::ostream& os, const Layout& l);
};

struct MetaLayout : GlobalStats {
  std::unordered_map<ulong, Layout> layouts;
};
