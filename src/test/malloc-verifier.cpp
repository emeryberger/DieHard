/*

  A stress test designed to expose bugs in memory allocator implementations.

  It spawns a number of threads that repeatedly allocate and free
  objects at random, of random sizes.

  It verifies (with high probability) that allocated objects never overlap.

 */


#include <stdlib.h> // abort
#include <limits.h>

#include <iostream>
using namespace std;

#include "../include/rng/mwc.h"


unsigned long numIterations = 1000;

class AllocationDriver {
public:
  AllocationDriver (long n)
    : objectsAllocated (0),
      memoryAllocated (0),
      rng (1011, 2037)
  {
    for (int i = 0; i < MAX_OBJECTS; i++) {
      isAllocated[i] = false;
      objectMap[i] = 0;
      objectSize[i] = 0;
    }
  }

  void run (unsigned long iterations = numIterations) {
    // First, initialize the region.

    for (int i = 0; i < MAX_OBJECTS; i++) {
      mallocOne();
    }
    checkValidity();

    // Now run the iterations.

    for (unsigned long i = 0; i < iterations; i++) {

      if (i % 1000 == 0) {
	cout << "iteration " << i << ": objects = " << objectsAllocated << ", memory = " << memoryAllocated << endl;
	checkValidity();
      }
      unsigned long r = rng.next();

      if (r % 1000 == 0) {
	// Periodically flush.
	int numToFree = rng.next() % objectsAllocated;
	cout << "PERIODIC FLUSH: Freeing " << numToFree << " objects." << endl;
	for (int j = 0; j < numToFree; j++) {
	  freeOne();
	}
      }
				  
      if (r % 2 == 0) {
	for (int j = 0; j < rng.next() % (MAX_OBJECTS / MAX_FULL_FRACTION); j++) {
	  mallocOne();
	}
      } else {
	freeOne();
      }
    }

    cout << "objects = " << objectsAllocated << ", memory = " << memoryAllocated << endl;
  }

  
private:

  bool freeOne() {
    if (objectsAllocated == 0) {
      false;
    }
    // Randomly probe until we find an allocated object, and free it
    // (first trashing the contents).
    while (true) {
      unsigned int index = rng.next() % MAX_OBJECTS;
      if (isAllocated[index]) {

	scramble (index);

	delete [] objectMap[index];

	objectsAllocated--;
	memoryAllocated -= objectSize[index];

	objectMap[index]   = 0;
	objectSize[index]  = 0;
	isAllocated[index] = false;

	return true;
      }
    }
  }

  bool mallocOne() {
    if ((objectsAllocated + 1) * MAX_FULL_FRACTION > MAX_OBJECTS) {
      return false;
    }
    // Randomly probe until we find a free object.
    while (true) {
      unsigned int index = rng.next() % MAX_OBJECTS;
      if (!isAllocated[index]) {
	// Got one. Mark it as allocated and fill it with a sentinel value.

	isAllocated[index] = true;

	// FIX ME: we are now rounding to a power of two.
	//	size_t sz = rng.next() % MAX_SIZE;

	size_t sz = rng.next() % LOG_MAX_SIZE;
	sz = 1 << sz;

	objectMap[index]   = new char[sz];
	objectSize[index]  = sz;

	objectsAllocated++;
	memoryAllocated += objectSize[index];

	fill (index);

	return true;
      }
    }
  }

  bool checkValidity() {
    for (int index = 0; index < MAX_OBJECTS; index++) {
      if (isAllocated[index]) {
	// Check the contents.
	bool isValid = check (index);

	if (!isValid) {
	  cout << "Wrong value detected for object " << index
	       << ", objects allocated = " << objectsAllocated << endl;
	  abort();
	  return false;
	}
      }
    }
    return true;
  }

  // Fill with a sentinel value (the index, plus position).
  void fill (unsigned long index) {
    unsigned long fillValue = index;
    unsigned long * ptr     =  (unsigned long *) objectMap[index];
    size_t sz               = objectSize[index];
    for (int i = 0; i < sz / sizeof(unsigned long); i++) {
      ptr[i] = fillValue + i;
    }
  }

  // Check that this object is intact.
  bool check (unsigned long index) {
    unsigned long fillValue = index;
    unsigned long * ptr     =  (unsigned long *) objectMap[index];
    size_t sz               = objectSize[index];
    for (int i = 0; i < sz / sizeof(unsigned long); i++) {
      if (ptr[i] != fillValue + i) {
	return false;
      }
    }
    return true;
  }

  // Scramble the contents of a particular object.
  void scramble (unsigned long index) {
    // Overwrite the contents with a random value (plus an index value
    // - 1), making it highly likely that if this overwrites any
    // objects we are using, we will notice it.
    unsigned long fillValue = rng.next();
    unsigned long * ptr     = (unsigned long *) objectMap[index];
    size_t sz               = objectSize[index];
    for (int i = 0; i < sz / sizeof(unsigned long); i++) {
      ptr[i] = fillValue + i - 1;
    }
  }

  enum { MAX_OBJECTS = 500000 };
  enum { MAX_SIZE = 128 };
  enum { LOG_MAX_SIZE = 7 };
  enum { MAX_FULL_FRACTION = 2 }; // 1/F = max fullness.

  MWC rng;
  unsigned int objectsAllocated;
  unsigned long memoryAllocated;

  bool isAllocated[MAX_OBJECTS];
  char * objectMap[MAX_OBJECTS];
  size_t objectSize[MAX_OBJECTS];

};

void * worker (void * v)
{
  long n = (long) v;
  AllocationDriver * a = new AllocationDriver (n);
  a->run ();
  return NULL;
}

int
main (int argc, char * argv[])
{
  enum { MAX_THREADS = 256 };

  int numThreads = 1;

  if (argc < 2) {
    cerr << "Usage: " << argv[0] << " numThreads (numIterations)" << endl;
    return -1;
  }

  numThreads = atoi(argv[1]);
  if (numThreads > MAX_THREADS) {
    cerr << "Number of threads cannot exceed " << MAX_THREADS << endl;
    return -1;
  }

  if (argc == 3) {
    numIterations = atoi(argv[2]);
  }


  pthread_t t[numThreads];

  for (int i = 0; i < numThreads; i++) {
    pthread_create (&t[i], NULL, worker, (void *) i);
  }
  for (int i = 0; i < numThreads; i++) {
    pthread_join (t[i], NULL);
  }

  return 0;
}
