CFLAGS = -I../static -I.. -O2 -DNDEBUG

all: diehard

clean:
	rm diehard

diehard: diehard.cpp voter.cpp reader.cpp spawnexecutable.cpp selector.h fingerprinter.h
	g++ $(CFLAGS) diehard.cpp voter.cpp reader.cpp spawnexecutable.cpp -o diehard

