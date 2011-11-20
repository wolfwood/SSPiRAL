module types;

typedef ulong NodeName;
typedef ulong LayoutName;
alias ulong Counter;

import tango.io.Stdout;

struct Node{
	static:
	// required before use
	void setDataSize(ulong n){
		N = cast(NodeName)n;
		maxNode = (cast(NodeName)1 << n)-cast(NodeName)1;
	}

	NodeName N, maxNode;

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

	this(){
		score = new Score;
	}

	static Layout[] startingSet(){
		Layout[] layouts;

		foreach(Layout l; Layout){
			layouts ~= l;
		}

		return layouts;
	}

	void generateLayouts(ref Layout[LayoutName] nextLayouts){
		foreach(LayoutName node ; Node){
			LayoutName nym = name | node;

			if(nym != name){
				Layout* l = nym in nextLayouts;
				if(l !is null){
					nextLayouts[nym].score += score;
				}else{
					Layout ell = new Layout;
					ell.name = nym;
					ell.score += score;

					//XXX:
					//if(alive)
					//l is still alive
					//else
					// run isAlive

					nextLayouts[nym] = ell;
				}
			}
		}
	}

private:
	/+int opApply(int delegate(ref Layout) dg){
		int result = 0;

		foreach(NodeName nym ; Node){
			LayoutName next = name | cast(LayoutName)nym;

			if(next != name){
				Layout* l = new Layout;

				l.name = next;

				result = dg(*l);
				if(result)break;
			}
		}

		return result;
	}+/


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
			l.generateLayouts(next);
			//l = null;
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

private:
	Counter score[];
}