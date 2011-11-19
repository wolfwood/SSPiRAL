module markovulator;

import tango.io.Stdout;
import tango.util.Convert;

import types;

int main(char[][] argv){
	if(argv.length < 2){
		Stdout("Usage: java Ululator <width>").newline;
		return 1;
	}

	Node.setDataSize(to!(uint)(argv[1]));





	return 0;
}