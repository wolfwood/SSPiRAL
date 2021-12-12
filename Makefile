CXXS=g++ clang++
CXXFLAGS=-std=c++14
CFLAGSgcc= -Werror -O3 -march=native -floop-interchange -ftree-loop-distribution -floop-strip-mine -floop-block -ftree-vectorize
CFLAGSclangcc= -Werror -march=native -flto -Ofast

CXXDEBUGFLAGS=-std=c++14 -g -Werror -flto

NAUTY=nauty26r11
NAUTYFLAGS=-DWORDSIZE=64 -DMAXN=WORDSIZE -I $(NAUTY).$* -fno-strict-aliasing

HDRS=types.h
SRCS=cpiral.cpp $(subst .h,.cpp,$(HDRS))
OBJS=$(subst .cpp,.o,$(SRCS))

EXES=$(patsubst %,bench.%,$(CXXS)) $(patsubst %,spiral.%,$(CXXS)) $(patsubst %,benchnauty.%,$(CXXS)) $(patsubst %,spiralnauty.%,$(CXXS))

# cpiral
all: $(EXES)


spiral.%: $(SRCS) $(HDRS)
	$* $(CXXFLAGS) ${CFLAGS$(*:%++=%cc)} -DSPIRAL_N=4 -o $@ $(SRCS)

spiralnauty.%: $(SRCS) $(HDRS) canonicalizer.cpp canonicalizer.h typesnauty.cpp $(NAUTY).%/nauty.a
	$* $(CXXFLAGS) ${CFLAGS$(*:%++=%cc)} $(NAUTYFLAGS) -DSPIRALNAUTY -DSPIRAL_N=3 -o $@ $(SRCS) canonicalizer.cpp typesnauty.cpp $(NAUTY).$*/nauty.a

spiral.debug.%: $(SRCS) $(HDRS)
	$* ${CXXDEBUGFLAGS} -DSPIRAL_N=4 -o $@ $(SRCS)

bench.%: $(SRCS) $(HDRS)
	$* $(CXXFLAGS) ${CFLAGS$(*:%++=%cc)} -DSPIRAL_N=5 -DBENCH=8 -o $@ $(SRCS)

benchnauty.%: $(SRCS) $(HDRS) canonicalizer.cpp canonicalizer.h typesnauty.cpp $(NAUTY).%/nauty.a
	$* $(CXXFLAGS) ${CFLAGS$(*:%++=%cc)} $(NAUTYFLAGS) -DSPIRALNAUTY -DSPIRAL_N=5 -DBENCH=8 -o $@ $(SRCS) canonicalizer.cpp typesnauty.cpp $(NAUTY).$*/nauty.a


$(NAUTY).tar.gz:
	wget http://pallini.di.uniroma1.it/$(NAUTY).tar.gz

$(NAUTY).%: $(NAUTY).tar.gz
	tar xf $<
	mv $(NAUTY) $@
	cd $@ && sed -i "s/ar crs/$(if $(filter $*,g++),gcc-ar,ar) crs/g" makefile.in
	cd $@ && CC=$(patsubst clangcc,clang,$(*:%++=%cc)) CFLAGS="${CFLAGS$(*:%++=%cc)}" ./configure

$(NAUTY).%/nauty.a: $(NAUTY).%
	cd $(@D) && $(MAKE)


clean:
	rm $(EXES)

#test.%:
#	echo sed -i 's/ar crs/$(if $(filter $*,g++),gcc-ar,ar) crs/g' $@/makefile.in

# gcc -o myprog -DWORDSIZE=64 -DMAXN=WORDSIZE myprog.c nautyL1.a
