_CFLAGS = -march=native -DNORMALIZE
CFLAGS = $(_CFLAGS) -DNDEBUG
CFLAGS_DEBUG = $(_CFLAGS) -g
CFLAGS_VERIFY = $(CFLAGS_DEBUG) -DVERIFY

CFLAGS_GCC = -pipe -O2 -ftree-vectorize -flto
CFLAGS_CLANG = -O3

GCC_GRAPHITE = $(shell gcc -v 2>&1 | grep -q with-isl; echo $$?)

ifeq ($(GCC_GRAPHITE), 0)
	CFLAGS_GCC += -fgraphite-identity -floop-nest-optimize
endif


UTIL = liveness.c types.c binomial_coeff.c mymmap.c
SRC = spiral.c $(UTIL)
HEADER = $(patsubst %.c,%.h,$(UTIL))


all: spiral spiral-clang

spiral: $(SRC) $(HEADER)
	gcc $(CFLAGS) $(CFLAGS_GCC) -o $@ $(SRC)

spiral-clang: $(SRC) $(HEADER)
	clang $(CFLAGS) $(CFLAGS_CLANG) -o $@ $(SRC)

debug-clang: $(SRC) $(HEADER)
	clang $(DEBUG_CFLAGS) $(CFLAGS_CLANG) -o $@ $(SRC)

sanitize: $(SRC) $(HEADER) alternates.c
	clang $(CFLAGS_DEBUG) $(CFLAGS_CLANG) -fsanitize=address,undefined,leak,integer -o $@ $(SRC)

sanitize-verify: $(SRC) $(HEADER) alternates.c
	clang $(CFLAGS_VERIFY) $(CFLAGS_CLANG) -fsanitize=address,undefined,leak,integer -o $@ $(SRC)

sanitize-mem: $(SRC) $(HEADER) alternates.c
	clang $(CFLAGS_DEBUG) $(CFLAGS_CLANG) -fsanitize=memory -o $@ $(SRC)

unused: $(SRC)
	gcc -ffunction-sections -Wl,--gc-sections,--print-gc-sections -o /dev/null $^ 2>&1 | rg ".*\.text\.(\w*).*" -r '$$1'

.PHONY: unused

clean:
	-rm spiral spiral-clang debug-clang sanitize sanitize-verify sanitize-mem
