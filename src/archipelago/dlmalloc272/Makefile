BASEDIR=..
include ${BASEDIR}/.common

dlmalloc.o: dlmalloc.c
	${CC} ${CCFLAGS} -c dlmalloc.c

clean:
	rm -f *.o

all: dlmalloc.o 