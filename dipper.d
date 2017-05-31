import std.stdio;

import types;

void main() {
  writeln(GlobalStats.N, " of ", GlobalStats.M);

  Layout[] curr;

  curr = [new Layout()];

  for (GlobalStats.nodesInLayout = 0; GlobalStats.nodesInLayout < GlobalStats.M; ++GlobalStats.nodesInLayout) {
    lookup_t next;
    foreach(ref l; curr) {
      l.generateLayouts(next);
    }

    curr = next.values;
  }

  foreach(ref l; curr) {
    l.print();
  }
}
