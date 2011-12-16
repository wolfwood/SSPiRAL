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
			markov.generateLayouts(next, layoutSize);
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

		MetaLayout last;
		char[] r;
		foreach(uint rank, MetaLayout markov; curr.sort.reverse){
			char[] n = markov.nodeName;
			if(n != ""){
				r ~= " " ~ n ~ ";";
			}

			markov.printMe(rank);

			if(rank == 0){
				markov.printScore();
			}

			markov.printOrderConstraint(last);

			last = markov;
		}
		if(r !is null){
			Stdout("{rank=same; l" ~ to!(char[])(layoutSize) ~ ";" ~ r ~ "}").newline;
		}

		curr = mapping.values;
	}

	// footer
	Stdout("nodesep=.05\nranksep=.5\n}").newline;

	return 0;
}