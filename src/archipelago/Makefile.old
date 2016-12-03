BASEDIR=.

include ${BASEDIR}/.common

INCLUDES=${INCLUDEALL}

LIBS=-ldl# -lpthread

#------ SparseHeaps library ----------------------

sparseheaps: libsparseheaps.so

libsparseheaps.so: libsparseheaps.o signals.o dlmalloc272/dlmalloc.o util/util.o util/log.o util/mman.o #util/asyncdiscard.o
	${CC} ${CCFLAGS} -o $@ libsparseheaps.o signals.o dlmalloc272/dlmalloc.o util/util.o util/log.o util/mman.o ${LIBS}


#------ OPP allocator ----------------------------

libsparseheaps.o: libsparseheaps.cc 
	${CC} ${CCFLAGS} ${INCLUDES} -c libsparseheaps.cc

#------ Page Description -------------------------

#pagedescription.o: pagedescription.cc 
#	${CC} ${CCFLAGS} ${INCLUDES} -c pagedescription.cc

#------ Page Tracker -----------------------------

objectperpageheap.o: objectperpageheap.cc livepagetracker.h coldcache.h allocator/fixedactiveheap.h
	${CC} ${CCFLAGS} ${INCLUDES} -c objectperpageheap.cc

#------ Lea allocator ----------------------------

dlmalloc272/dlmalloc.o: 
	cd dlmalloc272 ; make dlmalloc.o

#------ Log facility -----------------------------

util/log.o: 
	cd util; make log.o

#------ Misc facility ----------------------------

util/util.o: 
	cd util; make util.o

#------ Rate-limied madvise ----------------------

util/mman.o: 
	cd util; make mman.o

#------ Allocstall monitor -----------------------

util/iv.o: 
	cd util; make iv.o

util/asyncdiscard.o:
	cd util; make asyncdiscard.o

#------ Signal handler ---------------------------

signals.o: signals.cc
	${CC} ${CCFLAGS} ${INCLUDES} -c signals.cc

#------ VMCOMM -----------------------------------

util/vmcomm.o: 
	cd util; make vmcomm.o

#------ Simple library ---------------------------

libsimplebitmapheap.so: libsimplebitmapheap.o dlmalloc.o
	${CC} ${CCFLAGS} -o $@ libsimplebitmapheap.o dlmalloc.o -ldl

libsimplebitmapheap.o: libsimplebitmapheap.cc
	${CC} ${CCFLAGS} ${INCLUDES} -c libsimplebitmapheap.cc

#------ Full rebuild -----------------------------

all: libsparseheaps.so

rebuild: clean all

#------ Cleanup ----------------------------------

clean:
	rm -f *.o *.so
	cd util; make clean
	cd dlmalloc272; make clean




