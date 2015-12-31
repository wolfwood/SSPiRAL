CXX=clang++
CXXFLAGS=-std=c++14 -O3 -march=native

SRCS=cpiral.cpp types.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: cpiral test

cpiral: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

test: tester.cpp types.cpp
	$(CXX) $(CXXFLAGS) -o $@ tester.cpp types.cpp

# gcc -o myprog -DWORDSIZE=64 -DMAXN=WORDSIZE myprog.c nautyL1.a
