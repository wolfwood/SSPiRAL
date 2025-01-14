CFLAGS = -march=native -DNORMALIZE -DNDEBUG

CFLAGS_GCC = -pipe -O2 -ftree-vectorize -flto
CFLAGS_CLANG = -O3

GCC_GRAPHITE = $(shell gcc -v 2>&1 | grep -q with-isl; echo $$?)

ifeq ($(GCC_GRAPHITE), 0)
	CFLAGS_GCC += -fgraphite-identity -floop-nest-optimize
endif


all: spiral spiral-clang

spiral: spiral.c Makefile
	gcc $(CFLAGS) $(CFLAGS_GCC) -o $@ $<

spiral-clang: spiral.c Makefile
	clang $(CFLAGS) $(CFLAGS_CLANG) -o $@ $<
