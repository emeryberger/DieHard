// -*- C++ -*-

#ifndef DH_SHUFFLEHEAP_H
#define DH_SHUFFLEHEAP_H

#include <algorithm>
#include <stdlib.h>

#include "array.h"
#include "modulo.h"
#include "randomnumbergenerator.h"

template <int NObjects,
	  size_t Size,
	  class SuperHeap>
class ShuffleHeap : public SuperHeap {
public:

  ShuffleHeap()
  {
    // Fill up the buffer from the superheap.
    for (int i = 0; i < NObjects; i++) {
      _buffer(i) = SuperHeap::malloc (Size);
    }
    // Now shuffle it (Fisher-Yates).
    for (int i = NObjects - 1; i >= 1; i--) {
      // Pick a random integer, 0 ≤ j ≤ i, and swap with item i.
      int j = _rng.next() % (i+1);
      swap (_buffer(i), _buffer(j));
    }
  }


  inline void * malloc (size_t sz) {
    assert (sz <= Size);
    // Get an item from the superheap and swap it with a
    // randomly-chosen object from the array. This is one step of an
    // in-place Fisher-Yates shuffle.
    void * ptr = SuperHeap::malloc (Size);
    int j = modulo<NObjects>(_rng.next());
    swap (_buffer(j), ptr);
    return ptr;
  }


  inline void free (void * ptr) {
    // Choose a random item and evict it to the superheap,
    // replacing it with the one we are now freeing.
    int j = modulo<NObjects>(_rng.next());
    swap (_buffer(j), ptr);
    SuperHeap::free (ptr);
  }

 
private:

  /// The random number generator, used for shuffling and random selection.
  RandomNumberGenerator 	_rng;

  /// The buffer used to hold shuffled objects for heap requests.
  Array<NObjects, void *> 	_buffer;

};


#endif
