INC = ansi.h copyright.h espresso.h main.h mincov.h mincov_int.h port.h regex.h sparse.h sparse_int.h stdlib.h utility.h

SRC =  cofactor.c cols.c compl.c contain.c cubestr.c cvrin.c cvrm.c cvrmisc.c cvrout.c dominate.c equiv.c espresso.c essen.c exact.c expand.c gasp.c getopt.c gimpel.c globals.c hack.c indep.c irred.c main.c map.c matrix.c mincov.c opo.c pair.c part.c primes.c reduce.c rows.c set.c setc.c sharp.c sminterf.c solution.c sparse.c unate.c utility.c verify.c # dlmalloc28.c

SRC_SLOW =  cofactor.c cols.c compl.c contain.c cubestr.c cvrin.c cvrm.c cvrmisc.c cvrout.c dominate.c equiv.c espresso.c essen.c exact.c expand.c gasp.c getopt.c gimpel.c globals.c hack.c indep.c irred.c main-slow.c map.c matrix.c mincov.c opo.c pair.c part.c primes.c reduce.c rows.c set.c setc.c sharp.c sminterf.c solution.c sparse.c unate.c utility.c verify.c # dlmalloc28.c

ifeq ($(OS),Windows_NT)
CC = cl
CFLAGS = /DNDEBUG /Ox /MD /DWIN32 # -DHEAP_CHECKS=1
.SUFFIXES: .obj
OBJS=$(SRC:.c=.obj)
%.obj: %.c
	$(CC) $(CFLAGS) -c $< 
else
CC = gcc
CFLAGS = -DNDEBUG -O3 -g #-m64 -fPIC
OBJS=$(SRC:.c=.o)
OBJS_SLOW=$(SRC_SLOW:.c=.o)
.c.o:
	$(CC) $(CFLAGS) -c $<
endif

espresso: $(SRC) $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) -o espresso # /link /FORCE:MULTIPLE 
espresso-slow: $(SRC_SLOW} $(OBJS_SLOW) ${INC}
	$(CC) $(CFLAGS) $(OBJS_SLOW) -o espresso-slow
espresso-x: $(SRC) $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) xheap.obj -o espresso-x /link /FORCE:multiple

espresso-winhard: $(SRC) $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) usewinhard.obj winhard.lib -o espresso-hard

espresso-winhard-adaptive: $(SRC) $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) usewinhard-adaptive.obj winhard-adaptive.lib -o espresso-hard-adaptive

espresso-winhard-doa: $(SRC) $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) usewinhard-adaptive.obj winhard-diefast.lib -o espresso-hard-diefast

espresso-hoard: $(SRC) $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) usewinhoard.obj winhoard.lib -o espresso-hoard

espresso-hard: $(SRC) $(OBJS) $(INC)
	$(CC) $(CFLAGS) $(OBJS) usewinhard.obj winhard.lib -o espresso-hard

clean:
	-rm -f $(OBJS) espresso

