CC = g++
INC = -I. -I../heaplayers -I../heaplayers/util
CFLAGS = -pipe -Wall -g
DEBUG_CFLAGS = -g -DDEBUG -DMYASSERT
OPT_CFLAGS = -O3 -UDEBUG -DNDEBUG -mcpu=pentium4 -mtune=pentium4
OPT_FLAGS64 = -fPIC -O3 -UDEBUG -DNDEBUG -march=nocona -m64
LIBS=-lpthread -D_REENTRANT=1

all: stock_mlockmem ptime

#memtrace lrusim

clean:
	rm -f *.o *.so lrusim lrusim2 lrusim3 stock_mlockmem ptime memtrace malloctrace memtrace_debug

memtrace_debug:
	$(CC) $(INC) $(CM_CFLAGS) $(DB_CFLAGS) -shared libmemtrace.cpp -o libmemtrace.so -ldl

memtrace:
	$(CC) $(INC) $(CM_CFLAGS) $(OP_CFLAGS) -shared libmemtrace.cpp -o libmemtrace.so -ldl

malloctrace:
	$(CC) $(INC) $(CM_CFLAGS) $(OP_CFLAGS) -shared libmalloctrace.cpp -o libmalloctrace.so -ldl

lrusim:
	$(CC) $(CM_CFLAGS) $(OP_CFLAGS) lrusim.cpp -o lrusim

lrusim2:
	$(CC) $(CM_CFLAGS) $(OP_CFLAGS) lrusim2.cpp -o lrusim2

lrusim3:
	$(CC) $(CM_CFLAGS) $(OP_CFLAGS) lrusim3.cpp -o lrusim3

ptime: ptime.c
	${CC} ${CFLAGS} ${OPT_FLAGS64} -g ptime.c -o ptime
	#gcc ptime.c -o ptime

stock_mlockmem: stock_mlockmem.c
	${CC} ${CFLAGS} ${OPT_FLAGS64} stock_mlockmem.c -o stock_mlockmem
