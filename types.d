module types;

typedef uint NodeName;
typedef ulong LayoutName;

struct Node{
	static:
	// required before use
	void setDataSize(ulong n){
		N = cast(NodeName)n;
		maxNode = (cast(NodeName)2^N)-cast(NodeName)1;
	}

	NodeName N, maxNode;

	static int opApply(int delegate(ref NodeName) dg){
		int result = 0;
		for(NodeName i = 1; i <= maxNode; i++){
			result = dg(i);
			if(result)break;
		}

		return result;
	}
}

struct Layout{
	LayoutName name;

	static int opApply(int delegate(ref Layout) dg){
		int result = 0;

		return result;
	}
}

class MetaLayout{
	Layout[] layouts;

	uint name;
}

class Score{
	uint score[];
}