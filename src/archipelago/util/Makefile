BASEDIR=..

include ${BASEDIR}/.common

INCLUDES=${INCLUDEALL}


log.o: log.h log.cc
	${CC} ${CCFLAGS} ${INCLUDES} -c log.cc


util.o: util.cc util.h
	${CC} ${CCFLAGS} ${INCLUDES} -c util.cc	

randomnumbergenerator.o: randomnumbergenerator.h marsaglia.h
	${CC} ${CCFLAGS} ${INCLUDES} -c randomnumbergenerator.cc

vmcomm.o: vmcomm.h vmcomm.c
	${CC} ${CCFLAGS} -c vmcomm.c

iv.o: iv.h iv.c
	${CC} ${CCFLAGS} -c iv.c

mman.o: mman.h mman.cc
	${CC} ${CCFLAGS} ${INCLUDES} -c mman.cc	

asyncdiscard.o: asyncdiscard.cc
	${CC} ${CCFLAGS} ${INCLUDES} -c asyncdiscard.cc		

clean:
	rm -f *.o *.so

all: log.o util.o
