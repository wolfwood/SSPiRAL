
#include "types.h"

ulong GlobalStats::N=0;
ulong GlobalStats::M=0;
ulong GlobalStats::nodesInLayout=0;

void GlobalStats::setN(ulong n){
  if (n > 1 && n < 64) {
    N = n;
    M = (1 << n) - 1;
    nodesInLayout = 0;
  } else {

  }
}

DataNode::DataNode(ulong n) {
  node = n;
}

DataNode DataNode::getNext() {
  return DataNode(node + 1);
}

bool DataNode::hasNext() {
  return !(node == M);
}


void Score::grow() {
  score.push_back(0);
}
