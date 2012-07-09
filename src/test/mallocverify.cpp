/*

  A stress test designed to expose bugs in memory allocator implementations.

  It spawns a number of threads that repeatedly allocate and free
  objects at random, of random sizes.

  It verifies (with high probability) that allocated objects never overlap.

 */


#include <string.h> // memset
#include <stdlib.h> // abort

#include <iostream>
using namespace std;

#include "../include/rng/randomnumbergenerator.h"


class AllocationDriver {
public:
  AllocationDriver (long n)
    : objectsAllocated (0)
  {
    for (int i = 0; i < MAX_OBJECTS; i++) {
      isAllocated[i] = false;
      objectMap[i] = 0;
      objectSize[i] = 0;
    }
  }

  void run (int iterations) {
    // First, initialize the region.

    for (int i = 0; i < MAX_OBJECTS; i++) {
      allocateOne();
    }
    checkValidity();

    // Now run the iterations.
    for (int i = 0; i < iterations; i++) {
      if (i % 1000 == 0) {
	cout << "iteration " << i << ": objects = " << objectsAllocated << endl;
      }
      if (rng2.next() % 2 == 0) {
	freeOne();
      } else {
	allocateOne();
      }
      checkValidity();
    }
  }

  
private:

  void freeOne() {
    if (objectsAllocated == 0) {
      return;
    }
    // Randomly probe until we find an allocated object, and free it
    // (first trashing the contents).
    while (true) {
      unsigned int index = rng.next() % MAX_OBJECTS;
      if (isAllocated[index]) {

	// We now overwrite the contents with a random value (plus an
	// index value - 1), making it highly unlikely that this will
	// preserve its contents.

	size_t sz = objectSize[index];
	unsigned long fillValue = rng.next();
	unsigned long * ptr =  (unsigned long *) objectMap[index];

	for (int i = 0; i < sz / sizeof(unsigned long); i++) {
	  ptr[i] = fillValue + i - 1;
	}

	delete [] objectMap[index];
	objectMap[index]   = 0;
	objectSize[index]  = 0;
	isAllocated[index] = false;
	objectsAllocated--;
	return;
      }
    }
  }

  void allocateOne() {
    if ((objectsAllocated + 1) * MAX_FULL_FRACTION > MAX_OBJECTS) {
      return;
    }
    // Randomly probe until we find a free object.
    while (true) {
      unsigned int index = rng.next() % MAX_OBJECTS;
      if (!isAllocated[index]) {
	// Got one. Mark it as allocated and fill it with a sentinel value.

	objectsAllocated++;
	isAllocated[index] = true;

	size_t sz = rng.next() % MAX_SIZE;
	objectMap[index]   = new char[sz];
	objectSize[index]  = sz;

	unsigned long fillValue = index;
	unsigned long * ptr =  (unsigned long *) objectMap[index];

	for (int i = 0; i < sz / sizeof(unsigned long); i++) {
	  ptr[i] = fillValue + i;
	}

	return;
      }
    }
  }

  bool checkValidity() {
    for (int index = 0; index < MAX_OBJECTS; index++) {
      if (isAllocated[index]) {
	// Check the contents.
	unsigned long fillValue = index;
	unsigned long * ptr =  (unsigned long *) objectMap[index];

	for (int i = 0; i < objectSize[index] / sizeof(unsigned long); i++) {
	  if (ptr[i] != fillValue + i) {
	    cout << "Wrong value detected for object " << index
		 << " at index " << i << ", objects allocated = "
		 << objectsAllocated << endl;
	    abort();
	    return false;
	  }
	}
      }
    }
    return true;
  }

  enum { MAX_OBJECTS = 10000 };
  enum { MAX_SIZE = 128 };
  enum { MAX_FULL_FRACTION = 2 }; // 1/F = max fullness.

  RandomNumberGenerator rng, rng2;
  unsigned int objectsAllocated;

  bool isAllocated[MAX_OBJECTS];
  char * objectMap[MAX_OBJECTS];
  size_t objectSize[MAX_OBJECTS];

};

void * worker (void * v)
{
  long n = (long) v;
  AllocationDriver a (n);
  a.run (100000);
  return NULL;
}

int
main()
{
  enum { NUM_THREADS = 8 };

  pthread_t t[NUM_THREADS];

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_create (&t[i], NULL, worker, (void *) i);
  }
  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join (t[i], NULL);
  }

  return 0;
}
