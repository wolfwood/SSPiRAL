module testCanon;

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

	for(LayoutName l = cast(LayoutName)1; l < ((cast(LayoutName)1) << Node.maxNode); l++){
		//Stdout(l);
		ConsolidatedLayoutName c = new ConsolidatedLayoutName;
		c = l;

		//Stdout(" ");
		c.print();
		Stdout.newline;
	}

	return 0;
}