module markovulator;

import tango.io.Stdout;
import tango.util.Convert;

import types;

int main(char[][] argv){
	if(argv.length < 2){
		Stdout("Usage: java Ululator <width>").newline;
		return 1;
	}

	// --- initialization --
	Stdout("digraph markovmodel {").newline;

	Node.setDataSize(to!(uint)(argv[1]));
	MetaLayout.setup();
	MetaLayout[] curr;
	curr = [new MetaLayout(false, Layout.startingSet())];


	// --- work loop ---
	for(Counter layoutSize = 1; layoutSize <= Node.maxNode; layoutSize++){
		Layout[LayoutName] next;

		foreach(MetaLayout markov; curr){
			markov.printChildEdge();
			markov.generateLayouts(next);
		}

		MetaLayout[Score] mapping;

		// generate score to markov mapping
		foreach(Layout l; next){
			MetaLayout* markov = l.score in mapping;

			if(markov !is null){
				markov.addLayout(l);
			}else{
				mapping[l.score] = new MetaLayout(false, [l]);

				//XXX:  alive, etc.
			}
			//l = null;
		}

		foreach(MetaLayout markov; curr){
			markov.printMe();
		}

		curr = mapping.values;
	}


	// footer
	Stdout("}").newline;

	return 0;
}