CFLAGS = -O2 -DNDEBUG

all: libdiehard.so

clean:
	rm libdiehard.so

libdiehard.so: libdiehard.cpp diehard.h cpuinfo.h ifclass.h spinlock.h \
	wrapper.cpp partitioninfo.h
	sh compile

libbrokenmalloc.so: libbrokenmalloc.cpp
	sh compile-brokenmalloc
