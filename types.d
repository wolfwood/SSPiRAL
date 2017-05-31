module types;

import std.stdio;

/* === core types === */
//static if (GlobalStats.N <= 8) {
alias ubyte node_t;
//}
static if (8 < GlobalStats.N) {
  static assert(false); // N too large
}

static if (8 >= GlobalStats.M) {
  alias ubyte layout_t;
} else static if (16 >= GlobalStats.M) {
  alias ushort layout_t;
} else static if (32 >= GlobalStats.M) {
  alias uint layout_t;
} else static if (64 >= GlobalStats.M) {
  alias ulong layout_t;
} else {
    static assert(false); // M to large
}

alias uint score_t;

alias Layout[layout_t] lookup_t;

/* === fundemental simulation values === */
struct GlobalStats {
  /* >>> Edit This <<< */
  static const node_t N = 4;

  static const node_t M = 2^^N -1;
  static node_t nodesInLayout = 0;
}

const bool COUNT = true;

/* === utility Functions === */
ubyte ubyte2layout(ubyte n) {
  ubyte t = cast(ubyte)(n - 1);
  return  t;
}


layout_t node2layout(node_t n) in {
  assert(0 != n);
} body {
  layout_t t = cast(layout_t)(1 << (n - 1));
  return  t;
}


/* === daya types === */
class Layout : Score {
  static const layout_t MaxLayout = 2^^GlobalStats.M -1;
  layout_t name;
  bool alive;

  this(){ /* defaults are gud */}

  this(layout_t name, const Layout parent) {
    this.name = name;

    if (parent.alive) {
      alive = true;
    } else {
      alive = checkIfAlive(name);
    }

    if (alive) {
      score = parent.score ~ [score_t(1)];
    } else {
      score = parent.score ~ [score_t(0)];
    }
    static if(COUNT) {
      count = parent.count ~ [score_t(1)];
    }
  }

  void generateLayouts(ref lookup_t nextLayouts) {
    for (layout_t n = 1; n <= MaxLayout; n <<= 1) {
      layout_t nym = name | n;

      if (nym != name) {
	if (auto l = nym in nextLayouts) {
	  l.score[0..$-1] += score[];

	  static if (COUNT) {
	    l.count[0..$-1] += count[];
	  }
	} else {
	  nextLayouts[nym] = new Layout(nym, this);
	}
      }
    }
  }

  bool checkIfAlive(const layout_t name) {
    if (GlobalStats.N > GlobalStats.nodesInLayout+1) {
      return false;
    }

    for (node_t n = 1; n <= GlobalStats.M; n <<= node_t(1)) {
      layout_t l = node2layout(n);

      if (0 == (l & name)) {
	if (!recurseCheck(n)) {
	  return false;
	}
      }
    }

    return true;
  }

  bool recurseCheck(const node_t bitmap, node_t i = cast(node_t)1) {
    for(; i <= GlobalStats.M; ++i) {
      layout_t l =  node2layout(i);

      if (l & name) {
	node_t temp = i ^ bitmap;

	if (temp == 0 || recurseCheck(temp, cast(node_t)(i+1)) ) {
	  return true;
	}
      }
    }

    return false;
  }
}

class Score {
  void print() {
    auto i = 0;
    foreach_reverse(j, v; score) {
      static if (COUNT) {
	writeln(i++, " " , v, " ", count[j]);
      } else {
	writeln(i++, " " , v);
      }
    }

    writeln(score.length, " 0 1");
  }

private:
  score_t[] score;
  static if (COUNT) {
    score_t[] count;
  }
}
