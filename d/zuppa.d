import std.stdio;

import std.experimental.allocator.mmap_allocator;

import std.container.rbtree;


/* === basic data types === */
alias ubyte node_t;
alias uint layout_t;
alias uint score_t;
alias ubyte mlidx_t;


/* === fundemental simulation values === */
struct GlobalStats {
  /* >>> Edit This <<< */
  static immutable node_t N = 5;

  static immutable node_t M = 2^^N -1;

  static immutable score_t ScoreSize = (((M+1) / 2) - N);
  static immutable mlidx_t MLSize = 131+1;


  static node_t nodesInLayout = 0;


  static immutable score_t[M+1][M+1] coeffs;

  shared static this(){
    coeffs[0][0] = 1;

    for (uint i = 1; i <= M; ++i) {
      coeffs[i][0] = 1;
      coeffs[i][i] = 1;

      for (uint j = 1; j < i; ++j) {
	coeffs[i][j] = coeffs[i-1][j] + coeffs[i-1][j-1];
      }
    }
  }
}


/* === Helpers === */
pure layout_t node2layout(in node_t n) {
  return (cast(layout_t)1) << (n - 1);
}


/* === Simulation data types === */
struct Layout {
  version(VERIFY) {
    layout_t name;
  }

  mlidx_t scoreIdx;
}

struct MetaLayout {
  score_t[GlobalStats.ScoreSize] scores;

  bool opEquals()(auto ref const S b) const {
    if (scores.length == b.scores.length) {
      foreach(i, s; scores) {
	if (s != b.scores[i]) {
	  return false;
	}
      }
      return true;
    }

    return false;
  }

  auto opCmp(ref const MetaLayout b) const {
    if (scores.length == b.scores.length) {
      foreach_reverse(i, s; scores) {
	auto x = b.scores[i] - s;
	if (x) {
	  return x;
	}
      }
      return 0;
    } else {
      assert(0, "you did a bad!");
    }
  }

  void opAssign(ref const MetaLayout l) {
    scores[] = l.scores[];
  }

  void opOpAssign(string op)(auto ref const MetaLayout l) {
    mixin("scores[] "~op~"= l.scores[];");
  }
}

struct WorkContext {
private:
  Layout[] curr;
  layout_t pos;
  MetaLayout[] ml;
  mlidx_t ml_idx;
  mlidx_t[MetaLayout] lookup;
  node_t limit;

public:
  this(node_t limit) {
    //super(limit);
    this.limit = limit;

    curr = cast(Layout[])
      MmapAllocator.instance.allocate(GlobalStats.coeffs[GlobalStats.M][limit]*Layout.sizeof);

    ml.length = GlobalStats.MLSize;
  }

  void free(){
    MmapAllocator.instance.deallocate(curr);
  }

  MetaLayout* getScore() {return &ml[ml_idx];}

  version(VERIFY){
    auto getCurrName() const {return curr[pos].name;}
    auto getCurrName(layout_t idx) const {return curr[$ - idx - 1].name;}
    void setCurrName(layout_t n) {curr[pos].name = n;}
  }

  auto ref opIndex(in layout_t l) const {
    return ml[curr[$ - l - 1].scoreIdx];
  }

  void opAssign(ref const MetaLayout l) {
    ml[ml_idx] = l;
  }

  void opOpAssign(string op)(auto ref const MetaLayout l) {
    mixin("ml[ml_idx] "~op~"= l;");
  }

  void opUnary(string s)() if (s == "++") {
    /*
    curr[pos].scoreIdx = lookup.require(ml[ml_idx], ml_idx);

    if (curr[pos].scoreIdx == ml_idx) {
      ++ml_idx;
      }*/

    auto x = ml[ml_idx] in lookup;

    if (x is null) {
      lookup[ml[ml_idx]] = ml_idx;
      curr[pos].scoreIdx = ml_idx;

      ++ml_idx;
    } else {
      curr[pos].scoreIdx = *x;
    }

    ++pos;
  }
}

/* === Computational building blocks === */
class Iterator {
  immutable node_t limit;
  node_t[] Is;

  version(VERIFY) {
    layout_t name;
  }

  this(const node_t limit) {
    this.limit = limit;
    Is.length = limit+1;
  }

  pure bool checkIfAlive(layout_t name) {
    pure bool recurseCheck(layout_t name, node_t n, node_t i) {
      for(; i <= GlobalStats.M; ++i) {
	layout_t l = (cast(layout_t)1) << (i - 1);

	if (l & name) {
	  node_t temp = i ^ n;

	  if (temp == 0 || recurseCheck(name, temp, cast(node_t)(i+1))) {
	    return true;
	  }
	}
      }

      return false;
    }

    for(node_t n = 1; n <= GlobalStats.M; n <<= 1) {
      layout_t l = node2layout(n);

      if ((l & name) == 0) {
	if (!recurseCheck(name, n, cast(node_t)1)) {
	  return false;
	}
      }
    }
    return true;
  }

  // XXX: depth should ever be more than N, see NOTES
  bool deadnessCheck() {
    node_t j = limit;

    //  for(node_t n = 1 << (N-1); n > 0; n >>= 1) {
    for(node_t n = 1; n < GlobalStats.M; n <<= 1) {
      while (j > 1 && (Is[j]+1) < n) {
	--j;
      }

      if ((Is[j]+1) != n) {
	node_t temp = n;

	node_t[] Js;
	Js.length = limit+1;
	const node_t SENTINEL = 3;

	for (uint k = 1; k <= limit; ++k) {
	  Js[k] = SENTINEL;
	}

	// one greater than the highest acceptable value - gets fed into the next element
	Js[0] = 0;

	bool alive = false;
	uint i = 1;

	while (0 < i) {
	  if(0 == Js[i]) {
	    //temp ^= Is[i];
	    Js[i] = SENTINEL;
	    --i;
	  } else {
	    if(SENTINEL == Js[i]) {
	      Js[i] = 2;
	    }
	    temp ^= Is[i] +1;

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

  void WalkOrdered() {
    uint i = 1;
    Is[0] = GlobalStats.M;

    version(VERIFY) {
      layout_t[] ells;
      ells.length = limit+1;
      ells[0] = node2layout(GlobalStats.M) << 1;

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
    } else { // VERIFY
      while (0 < i) {
	if (0 == Is[i]) {
	  Is[i] = Is[i-1];
	}
	--Is[i];

	if (limit == i) {
	  oneStep();
	}

	if(0 == Is[i]) {
	  --i;
	} else if (limit > i) {
	  ++i;
	}
      }
    }
  }
  /*
  void WalkCombinadically() {
    uint i = 1;
    Is[0] = GlobalStats.M;

    while (0 < i) {
      if (0 == Is[i]) {
	Is[i] = Is[i-1];
      }
      --Is[i];

      if (limit == i) {
	oneStep();
      }

      if(0 == Is[i]) {
	--i;
      } else if (limit > i) {
	++i;
      }
    }
  }
  */
  void sumChildLayoutScores(ref WorkContext curr, ref const WorkContext prev) {
    score_t[] beforeCoefs;
    score_t[] afterCoefs;
    beforeCoefs.length = limit - 1;
    afterCoefs.length = limit - 1;

    uint idx = 0;

     // pre-calculate the combinadic components for each node, for both coming before and after the 'missing' node
    for (int i = 0; i < (limit -1); ++i) {
      auto before = GlobalStats.coeffs[Is[i+1]][(limit) - (i+1)];
      auto after  = GlobalStats.coeffs[Is[i+2]][(limit) - (i+2) + 1];
      assert(before > after);
      beforeCoefs[i] = before;
      afterCoefs[i] = after;
      idx += after;
    }

    version(VERIFY){
      assert(prev.getCurrName(idx) == (name ^ (cast(layout_t)1 << Is[1])));
      //assert(prev.getCurrName(idx) == (name ^ node2layout(Is[1])));
    }
    //curr.setScore(prev[idx]);
    curr = prev[idx];

    foreach (i, c; afterCoefs) {
      idx -= c;
      idx += beforeCoefs[i];

      version(VERIFY){
	assert(prev.getCurrName(idx) == (name ^ (cast(layout_t)1 << Is[i+2])));
      }

      curr += prev[idx];
    }
  }

protected:
  abstract void oneStep();
}


class FirstBlush : Iterator {
  WorkContext work;

  this(node_t limit, ref WorkContext w) {
    super(limit);

    work = w;

    //curr.length = GlobalStats.coeffs[GlobalStats.M][limit];

    //ml.length = 2;
    //ml[1].scores[0] = 1;
  }

  override void oneStep() {
    //alias next_ml = ml[ml_idx];
    /*
    bool dead = deadnessCheck();

    if (dead) {
      curr[pos].scoreIdx = 1;
    }

    version(VERIFY) {
      curr[pos].name = name;
      assert(ml[curr[pos].scoreIdx].scores[0] == (!checkIfAlive(name)));
    }

    ++pos;*/
    auto s = work.getScore();

    auto ded = deadnessCheck();
    if (ded) {
      s.scores[0] = 1;
    } else {
      s.scores[0] = 0;
    }

    version(VERIFY) {
      work.setCurrName(name);
      assert(ded == (!checkIfAlive(name)));
      assert(work.getScore().scores[0] == (!checkIfAlive(name)));
    }

    ++work;
  }
}

alias IntermediateZone = WorkZone!(true);
alias TerminalWork = WorkZone!(false);

class WorkZone(bool LifeDetector): Iterator {
  WorkContext work;
  const WorkContext prev;

  this(in node_t limit, ref WorkContext w, const ref WorkContext prev) {
    super(limit);
    work = w;
    this.prev = prev;
  }

  override void oneStep() {
    version(VERIFY) {
      work.setCurrName(name);
    }

    // args->curr, args->layoutsInCurr, args->curr_ml, Is, limit, next_ml, (limit - N));
    // args->curr, args->layoutsInCurr, args->curr_ml, Is, limit, next_ml, SCORE_SIZE);
    sumChildLayoutScores(work, prev);

    static if (LifeDetector) {
      auto s = work.getScore();

      if (limit == s.scores[limit - GlobalStats.N - 1]) {
	s.scores[limit - GlobalStats.N] = deadnessCheck();

	version(VERIFY) {
	  assert(!checkIfAlive(work.getCurrName()) == s.scores[limit - GlobalStats.N]);
	}
      } else {
	s.scores[limit - GlobalStats.N] = 0;
      }
    }

    ++work;
  }

  void normalize() {
    static if (LifeDetector) {
      for(mlidx_t k = 0; k < work.ml_idx; ++k) {
	for(uint j = 2; j <= (limit - GlobalStats.N); ++j){
	  assert(0 == work.ml[k].scores[limit - GlobalStats.N - j] % j);
	  work.ml[k].scores[limit - GlobalStats.N - j] /= j;
	}

      }
    } else {
      for(mlidx_t k = 0; k < work.ml_idx; ++k) {
	for(uint j = 0; j < GlobalStats.ScoreSize; ++j){
	  score_t adjustment = limit - (GlobalStats.M/2);
	  assert(0 == work.ml[k].scores[GlobalStats.ScoreSize - j - 1] % (j + adjustment));
	  work.ml[k].scores[GlobalStats.ScoreSize - j - 1] /= (j + adjustment);
	}
      }
    }
  }
}


int main(char[][]) {
  writeln(GlobalStats.N, " of ", GlobalStats.M);

  WorkContext curr, prev;

  { // setup initial context, has no ancestor
    prev = WorkContext(GlobalStats.N);

    auto it = new FirstBlush(GlobalStats.N, prev);

    it.WalkOrdered();
  }

  node_t i = GlobalStats.N+1;
  for (; i <= (GlobalStats.M/2); ++i) {
    writeln("-", i);

    curr = WorkContext(i);
    auto it = new IntermediateZone(i, curr, prev);

    it.WalkOrdered();

    it.normalize();
    prev.free();
    prev = curr;
  }

  for (; i <= GlobalStats.M; ++i) {
    writeln(i);

    curr = WorkContext(i);
    auto it = new TerminalWork(i, curr, prev);

    it.WalkOrdered();

    it.normalize();
    prev.free();
    prev = curr;
  }

  for (auto j = 0; j < ((GlobalStats.M+1)/2); ++j) {
    ulong total = GlobalStats.coeffs[GlobalStats.M][j];
    writeln(j, " ", total, " ", total);
  }

  for (int j = GlobalStats.ScoreSize - 1; j >= 0; --j) {
    ulong total = GlobalStats.coeffs[GlobalStats.M][(GlobalStats.M/2) + GlobalStats.ScoreSize - j];
    writeln((GlobalStats.M/2) + GlobalStats.ScoreSize - j, " ",
	    total - cast(ulong)curr.ml[0].scores[j], " ", total);
  }

  for (auto j = GlobalStats.M - GlobalStats.N + 1; j <= GlobalStats.M; ++j) {
    writeln(j, " ", 0, " ", GlobalStats.coeffs[GlobalStats.M][j]);
  }

  prev.free();


  return 0;
}
