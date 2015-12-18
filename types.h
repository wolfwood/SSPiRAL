#include <vector>
#include <unordered_map>

//typedef unsigned long long ulong;


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
  void grow();

};

struct Layout : GlobalStats {
 private:
  ulong nodes;
  Score score;

 public:
  Layout();
  Layout getNext();
  bool hasNext();
};

struct MetaLayout : GlobalStats {
  std::unordered_map<ulong, Layout> layouts;
};

struct MarkovState : GlobalStats {

};
