CFLAGS = -O2 -DNDEBUG

all: libbrokenmalloc.so libtrackalloc.so libdetfaults.so

clean:
	rm libbrokenmalloc.so libtrackalloc.so

libbrokenmalloc.so: libbrokenmalloc.cpp
	sh compile-brokenmalloc

libdetfaults.so: libdetfaults.cpp
	sh compile-detfaults

libtrackalloc.so: libtrackalloc.cpp
	sh compile-trackalloc
