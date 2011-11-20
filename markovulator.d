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
	Node.setDataSize(to!(uint)(argv[1]));
	MetaLayout deth = new MetaLayout(true);
	MetaLayout[] curr;
	curr = [new MetaLayout(false, Layout.startingSet())];

	Stdout("digraph markovmodel {").newline;


	// --- work loop ---
	for(Counter layoutSize = 1; layoutSize <= Node.maxNode; layoutSize++){
		Layout[LayoutName] next;

		foreach(MetaLayout markov; curr){
			 markov.generateLayouts(next);
		}

		MetaLayout[Score] mapping;

		// generate score to markov mapping
		foreach(Layout l; next){
			MetaLayout* markov = l.score in mapping;

			if(markov !is null){
				markov.layouts ~= l;
			}else{
				mapping[l.score] = new MetaLayout(false, [l]);

				//XXX:  alive, etc.
			}
			//l = null;
		}

		// print markov nodes


		curr = mapping.values;
	}


	// footer
	Stdout("}").newline;

	return 0;
}