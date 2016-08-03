CXX=g++
CXXFLAGS=-std=c++14 -O3 -march=native -Werror -flto
GCCFLAGS=-floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block -ftree-vectorize -flto=4
CXXDEBUGFLAGS=-std=c++14 -O0 -march=native -g -Werror -flto

NAUTY25FLAGS=-DWORDSIZE=64 -DMAXN=WORDSIZE -I nauty25r9
NAUTYFLAGS=-DWORDSIZE=64 -DMAXN=WORDSIZE -I nauty26r5.clang

SRCS=cpiral.cpp types.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

# cpiral
all: test canon canon.clang canon.gcc spiralnauty.gcc spiralnauty.clang spiralnauty

cpiral: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

test: tester.cpp types.cpp types.h
	$(CXX) $(CXXFLAGS) $(NAUTYFLAGS) -o $@ tester.cpp types.cpp
	$(CXX) $(CXXFLAGS) $(GCCFLAGS) $(NAUTYFLAGS) -o $@.gcc tester.cpp types.cpp
	clang++ $(CXXFLAGS) $(NAUTYFLAGS) -o $@.clang tester.cpp types.cpp

canon: canon_test.cpp canonicalizer.cpp canonicalizer.h
	$(CXX) $(CXXFLAGS) $(NAUTY25FLAGS) -o $@ canon_test.cpp canonicalizer.cpp nauty25r9/nauty.a

canon.clang: canon_test.cpp canonicalizer.cpp canonicalizer.h
	clang++ $(CXXFLAGS) $(NAUTYFLAGS) -o $@ canon_test.cpp canonicalizer.cpp nauty26r5.clang/nauty.a

canon.gcc: canon_test.cpp canonicalizer.cpp canonicalizer.h
	g++ $(CXXFLAGS) $(NAUTYFLAGS) $(GCCFLAGS) -o $@ canon_test.cpp canonicalizer.cpp nauty26r5.gcc/nauty.a

spiralnauty: spiralnauty.cpp types.cpp types.h canonicalizer.cpp canonicalizer.h
	g++ $(CXXFLAGS) $(NAUTY25FLAGS) -O1 -fno-lto -o $@ spiralnauty.cpp types.cpp canonicalizer.cpp nauty25r9/nauty.a

spiralnauty.clang: spiralnauty.cpp types.cpp types.h canonicalizer.cpp canonicalizer.h
	clang++ $(CXXFLAGS) $(NAUTYFLAGS) -o $@ spiralnauty.cpp types.cpp canonicalizer.cpp nauty26r5.clang/nauty.a

spiralnauty.gcc: spiralnauty.cpp types.cpp types.h canonicalizer.cpp canonicalizer.h
	g++ $(CXXFLAGS) $(NAUTYFLAGS) $(GCCFLAGS) -o $@ spiralnauty.cpp types.cpp canonicalizer.cpp nauty26r5.gcc/nauty.a

clean:
	rm test canon canon.gcc canon.clang cpiral

# gcc -o myprog -DWORDSIZE=64 -DMAXN=WORDSIZE myprog.c nautyL1.a
