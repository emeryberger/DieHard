CC=g++
FLAGS=-g -pipe -shared #-fPIC -march=nocona -m64
DEFINES=-D_REENTRANT=1
INCLUDES=-I.
LIBS=-ldl

all: libbrokenmalloc.so liboverflowinjector.so libtrackalloc.so

libbrokenmalloc.so: libbrokenmalloc.cpp
	${CC} ${FLAGS} -o $@ ${INCLUDES} ${DEFINES} libbrokenmalloc.cpp ${LIBS}

liboverflowinjector.so: liboverflowinjector.cpp
	${CC} ${FLAGS} -o $@ ${INCLUDES} ${DEFINES} liboverflowinjector.cpp ${LIBS}

libtrackalloc.so: libtrackalloc.cpp
		${CC} ${FLAGS} -o $@ ${INCLUDES} ${DEFINES} libtrackalloc.cpp ${LIBS}

clean:
	rm -fr *.so


