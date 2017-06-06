import std.stdio;

import types;

void main() {
  writeln(GlobalStats.N, " of ", GlobalStats.M);

  Layout[] curr;

  curr = [new Layout()];

  for (GlobalStats.nodesInLayout = 0; GlobalStats.nodesInLayout < GlobalStats.M; ++GlobalStats.nodesInLayout) {
    lookup_t next;

    assert(curr.length == binomialCoeff(GlobalStats.M, GlobalStats.nodesInLayout));

    version(BENCH) {
      writeln(GlobalStats.nodesInLayout);
    }

    foreach(ref l; curr) {
      l.generateLayouts(next);
    }

    version(BENCH) {
      if(GlobalStats.nodesInLayout == 8) {
	return;
      }
    }

    curr = next.values;
  }

  foreach(ref l; curr) {
    l.print();
  }
}
