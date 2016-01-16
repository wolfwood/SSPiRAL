CXX=clang++
CXXFLAGS=-std=c++14 -O3 -march=native -Werror
CXXDEBUGFLAGS=-std=c++14 -O0 -march=native -g -Werror

NAUTYFLAGS=-DWORDSIZE=64 -DMAXN=WORDSIZE -I nauty25r9

SRCS=cpiral.cpp types.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

# cpiral
all: test canon

cpiral: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

test: tester.cpp types.cpp types.h
	$(CXX) $(CXXFLAGS) -o $@ tester.cpp types.cpp

canon: canon_test.cpp canonicalizer.cpp canonicalizer.h
	$(CXX) $(CXXFLAGS) $(NAUTYFLAGS) -o $@ canon_test.cpp canonicalizer.cpp nauty25r9/nauty.a
# clang++ -std=c++14 -O3 -march=native -I nauty25r9 -o myprog -DWORDSIZE=64 -DMAXN=WORDSIZE canon_test.cpp canonicalizer.cpp nauty25r9/nauty1.a

clean:
	rm test canon cpiral

# gcc -o myprog -DWORDSIZE=64 -DMAXN=WORDSIZE myprog.c nautyL1.a
