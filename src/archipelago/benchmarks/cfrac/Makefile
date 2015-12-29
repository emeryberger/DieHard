#
# An implementation of the Continued Fraction Algorithm CFRAC
#
# Dave Barrett
#

PDIR 	= .
# removed because I don't want to recompile every time I change header file
#INCF 	= pfactor.h

ifeq ($(OS),Windows_NT)
CC = cl
CXX = cl
CFLAGS = /Zi /MT /Ox /DNDEBUG /DNOMEMOPT=1 # /MD
CXXFLAGS = /Zi /MT /Ox /DNDEBUG /DNOMEMOPT=1
else
CC      = gcc
CXX     = g++
CFLAGS  = -m32 -O3 -DNDEBUG # -g # -O
CXXFLAGS  = -m32 -O3 -DNDEBUG -lm # -g # -O
endif

BINSRC  = cfrac.c
HDR	= primes.h seive.h pfactor.h

PSRC	= pops.c pconst.c pio.c \
	  pabs.c pneg.c pcmp.c podd.c phalf.c \
	  padd.c psub.c pmul.c pdivmod.c psqrt.c ppowmod.c \
	  atop.c ptoa.c itop.c utop.c ptou.c errorp.c \
	  pfloat.c pidiv.c pimod.c picmp.c 

FSRC	= primes.c pcfrac.c pgcd.c

FOBJS	= $(PSRC:.c=.o)
POBJS	= $(FSRC:.c=.o)

ifeq ($(OS),Windows_NT)
OBJS	= $(BINSRC:.c=.obj) $(PSRC:.c=.obj) $(FSRC:.c=.obj) getopt.obj 
%.obj: %.c
	$(CC) $(CFLAGS) -c $<
else
OBJS	= $(BINSRC:.c=.o) $(PSRC:.c=.o) $(FSRC:.c=.o)
endif

cfrac:	$(OBJS)
	$(CC) $(CXXFLAGS) -o cfrac $(OBJS) 

cfrac-dlmalloc: $(OBJS) dlmalloc.obj
	$(CXX) $(CXXFLAGS) -o cfrac-dlmalloc dlmalloc.obj $(OBJS) /link /force:multiple

cfrac-hoard:	$(OBJS)
	$(CXX) $(CXXFLAGS) usewinhoard.obj winhoard.lib -o cfrac-hoard $(OBJS)

cfrac-diehard:	$(OBJS)
	$(CXX) $(CXXFLAGS) usewinhard.obj winhard.lib -o cfrac-diehard $(OBJS)

SRC	= Makefile $(HDR) $(BINSRC)

dlmalloc.obj:	dlmalloc.c
	$(CC) $(CFLAGS) /c dlmalloc.c

pgcd.o:		$(INC)
pcfrac.o:	$(INCF) $(INC) primes.h
primes.o:	primes.h

clean:
		rm -f core
		rm -f $(OBJS) cfrac

# 
# producer only clean
#
pclean:	
		rm -f $(OBJS)
