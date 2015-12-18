CXX=clang++
CXXFLAGS=-std=c++14

SRCS=cpiral.cpp types.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: cpiral

cpiral: $(SRCS)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS)
