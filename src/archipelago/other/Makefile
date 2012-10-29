CC=gcc
CPP=g++
FLAGS=-g -pipe -Wall
INCLUDE=-I.. -I../util
all: vmcomm_test

vmcomm_test: vmcomm_test.o ../util/vmcomm.o
	${CC} ${FLAGS} -o $@ vmcomm_test.o ../util/vmcomm.o

vmcomm_test.o: vmcomm_test.c
	${CC} ${INCLUDE} ${FLAGS} -c vmcomm_test.c


iv_test: iv_test.o
	${CPP} ${FLAGS} -o $@ iv_test.o

iv_test.o: iv_test.c
	${CPP} ${FLAGS} -c iv_test.c

../util/vmcomm.o: 
	cd ../util; make vmcomm.o

clean:
	rm -f *.o vmcomm_test iv_test

