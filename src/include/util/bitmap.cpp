// Unit test for bitmap.h.

#include <assert.h>
#include <heaplayers.h>

#include "bitmap.h"

using namespace HL;

int
main()
{
  const int NUMTRIALS = 1000;
  for (int NUM = 100; NUM < 10000; NUM *= 2) {
    BitMap<MallocHeap> b;
    b.reserve (NUM);
    b.clear();
    for (int k = 0; k < NUMTRIALS; k++) {
      // Generate a random stream of bits.
      int rnd[NUM];
      for (int i = 0; i < NUM; i++) {
	rnd[i] = lrand48() % 2;
      }
      for (int i = 0; i < NUM; i++) {
	if (rnd[i] == 0) {
	  bool r = b.tryToSet (i);
	  assert (r);
	} else {
	  b.reset (i);
	}
      }
      for (int i = 0; i < NUM; i++) {
	if (rnd[i] == 0) {
	  assert (b.isSet (i));
	  b.reset (i);
	} else {
	  assert (!b.isSet (i));
	}
      }
    }
  }

  return 0;
}
