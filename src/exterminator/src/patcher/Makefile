CXXFLAGS=-g

all:	patcher

patcher:	dang_ptr.o overflow.o main.o patch_db.o heap_structure.o numerics.o
	g++ -o patcher $^

clean:
	rm *.o patcher
