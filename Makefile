CXX=clang++
CXXFLAGS=-std=c++14 -O2

SRCS=cpiral.cpp types.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: cpiral test

cpiral: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)

test: tester.cpp types.cpp
	$(CXX) $(CXXFLAGS) -o $@ tester.cpp types.cpp
