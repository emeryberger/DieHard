#CFLAGS = -O2 -DNDEBUG
CFLAGS = -g

all: diehard dieinject libdiehard.so

clean:
	rm diehard dieinject libdiehard.so libdiefast.so

diehard: diehard.cpp voter.cpp reader.cpp spawnexecutable.cpp selector.h fingerprinter.h isolator.cpp
	g++ $(CFLAGS) diehard.cpp voter.cpp reader.cpp spawnexecutable.cpp isolator.cpp -o diehard

dieinject: dieinject.cpp voter.cpp reader.cpp spawnexecutable.cpp selector.h fingerprinter.h isolator.cpp
	g++ $(CFLAGS) dieinject.cpp voter.cpp reader.cpp spawnexecutable.cpp isolator.cpp -o dieinject

libdiehard.so: libdiehard.cpp diehard.h bumpalloc.h mmapalloc.h randomnumbergenerator.h theoneheap.h
	sh compile

libbrokenmalloc.so: libbrokenmalloc.cpp
	sh compile-brokenmalloc
