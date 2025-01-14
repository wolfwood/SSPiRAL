CFLAGS = -march=native -pipe -O2 -ftree-vectorize -flto -floop-nest-optimize

all: spiral spiral-clang

spiral: spiral.c
	gcc $(CFLAGS) -o $@ $<

spiral-clang: spiral.c
	clang -O3 -march=native -o $@ $<
