CFLAGS = -O2 -DNDEBUG

all: libbrokenmalloc.so libtrackalloc.so

clean:
	rm libbrokenmalloc.so libtrackalloc.so

libbrokenmalloc.so: libbrokenmalloc.cpp
	sh compile-brokenmalloc

libtrackalloc.so: libtrackalloc.cpp
	sh compile-trackalloc
