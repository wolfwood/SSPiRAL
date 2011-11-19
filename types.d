module types;

typedef uint NodeName;
typedef ulong LayoutName;

struct Layout{
	LayoutName name;
}

class MetaLayout{
	Layout[] layouts;

	uint name;
}

class Score{
	uint score[];
}