module types;

typedef ulong NodeName;
typedef ulong LayoutName;
alias ulong Counter;

import tango.io.Stdout;
import tango.util.Convert;

struct Node{
	static:
	// required before use
	void setDataSize(ulong n){
		N = cast(NodeName)n;
		maxNode = (cast(NodeName)1 << n)-cast(NodeName)1;
	}

	NodeName N, maxNode;
	const NodeName one = cast(NodeName)1;

	static int opApply(int delegate(ref NodeName) dg){
		int result = 0;
		for(NodeName i = cast(NodeName)1; i <= maxNode; i++){
			result = dg(i);
			if(result)break;
		}

		return result;
	}

	static int opApply(int delegate(ref LayoutName) dg){
		int result = 0;
		for(LayoutName i = cast(LayoutName)1; i < (1UL << maxNode); i <<= 1){
			result = dg(i);
			if(result)break;
		}

		return result;
	}
}

class Layout{
	LayoutName name;
	Score score;
	MetaLayout markov;

	this(){
		score = new Score;
	}

	this(LayoutName n, Score s, MetaLayout m){
		name = n;

		score = new Score;
		score.score.length = s.score.length;
		score += s;

		markov = m;
	}

	static Layout[] startingSet(){
		Layout[] layouts;

		foreach(Layout l; Layout){
			layouts ~= l;
		}

		return layouts;
	}

	void generateLayouts(ref Layout[LayoutName] nextLayouts, MetaLayout m){
		foreach(LayoutName node ; Node){
			LayoutName nym = name | node;

			if(nym != name){
				Layout* l = nym in nextLayouts;
				if(l !is null){
					nextLayouts[nym].score += score;
				}else{
					Layout ell = new Layout(nym, score, m);

					if(alive){
						ell.alive = true;
					}else{
						ell.alive = ell.checkIfAlive();
					}

					if(ell.alive){
						ell.score.score ~= [1];
					}else{
						ell.score.score ~= [0];
					}

					nextLayouts[nym] = ell;
				}
			}
		}
	}

private:
	bool alive;

	bool checkIfAlive(){
		bool flag = true;

		for(NodeName dataNode = cast(NodeName)1; dataNode <= Node.maxNode; dataNode <<= 1){
			LayoutName dataLayout = cast(LayoutName)1 << (dataNode - cast(NodeName)1);

			if((name & dataLayout) == 0){
				if(!recurseCheck(dataNode)){
					return false;
				}
			}
		}

		return flag;
	}

	bool recurseCheck(NodeName bitmap, NodeName i = Node.one){
		for(; i <= Node.maxNode; i++){
			LayoutName nodeLayout = cast(LayoutName)1 << (i - Node.one);

			if(name & nodeLayout){
				NodeName newBitmap = i ^ bitmap;

				if(newBitmap == 0 || recurseCheck(newBitmap, i+Node.one)){
					return true;
				}
			}
		}

		return false;
	}


	static int opApply(int delegate(ref Layout) dg){
		int result = 0;

		foreach(LayoutName nym ; Node){
			Layout l = new Layout;
			l.name = nym;
			l.score = new Score;
			l.score.score.length = 1;

			result = dg(l);
			if(result)break;
		}

		return result;
	}
}

// markov mode state
class MetaLayout{
	Layout[] layouts;

	Counter name;
	bool alive;
	Counter coalescedNodes;

	synchronized this(bool dethNode, Layout[] ls = null){
		if(dethNode){
			coalescedNodes++;
		}else{
			layouts = ls;
			coalescedNodes += ls.length;
		}

		name = count;
		count++;
	}

	void generateLayouts(ref Layout[LayoutName] next){
		foreach(Layout l; layouts){
			l.generateLayouts(next, this);
			//l = null;
		}
	}

	void printMe(){
		if(name == 0){
			Stdout("n0 [label=\"Death\"]").newline;
		}else{
			Stdout("n" ~ to!(char[])(name) ~ " [label=\"" ~
						 to!(char[])(layouts[0].score.score.length) ~ " " ~ to!(char[])(layouts.length) ~"\"]").newline;
		}
	}

private:
	static Counter count;
}

class Score{
	Score opPostInc(){
		score[$]++;
		return this;
	}

	Score opAddAssign(Score other){
		if(score is null){
			score = other.score.dup;
		}else{
			assert(other.score.length <= score.length);
			for(Counter i = 0; i < other.score.length; i++){
				score[i] += other.score[i];
			}

		}

		return this;
	}

	hash_t toHash(){
		Counter hash;

		foreach(Counter s; score){
			hash += s;
		}

		return hash;
	}

	int opEquals(Object o){
		Score s = cast(Score)o;

		if(s.score.length != score.length)
			return 0;

		foreach(int i, Counter val; score){
			if(val != s.score[i]){
				return 0;
			}
		}

		return 1;
	}

	int opCmp(Object o){
		Score s = cast(Score)o;

		foreach(int i, Counter val; score){
			if(val != s.score[i]){
				return val - score[i];
			}
		}

		return 0;
	}

private:
	Counter score[];
}
