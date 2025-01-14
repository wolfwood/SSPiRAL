CFLAGS = -march=native -DNORMALIZE -DNDEBUG

CFLAGS_GCC = -pipe -O2 -ftree-vectorize -flto
CFLAGS_CLANG = -O3


all: spiral spiral-clang

spiral: spiral.c Makefile
	gcc $(CFLAGS) $(CFLAGS_GCC) -o $@ $<

spiral-clang: spiral.c Makefile
	clang $(CFLAGS) $(CFLAGS_CLANG) -o $@ $<
