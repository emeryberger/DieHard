CC=gcc
CCFLAGS=-g -Wall

all: malloc-test

malloc-test: malloc-test.o
	${CC} ${CCFLAGS} -o $@ malloc-test.o

malloc-test.o: malloc-test.c
	${CC} ${CCFLAGS} -c malloc-test.c

clean:
	rm -f malloc-test malloc-test.o