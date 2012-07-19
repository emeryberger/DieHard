// -*- C++ -*-

#ifndef DH_SHUFFLEHEAP_H
#define DH_SHUFFLEHEAP_H

#include <algorithm>
#include <stdlib.h>

#include "array.h"
#include "modulo.h"
#include "randomnumbergenerator.h"

#define FIXED_SIZE 0

template <int NObjects,

#if FIXED_SIZE
	  size_t Size,
#endif

	  class SuperHeap>
class ShuffleHeap : public SuperHeap {
public:

  ShuffleHeap()
    : _initialized (false)
  {
#if FIXED_SIZE
    fill (Size);
#endif
  }


  inline void * malloc (size_t sz) {
#if FIXED_SIZE
    assert (sz <= Size);
    const size_t reqSize = Size;
#else
    static size_t reqSize = 0;
    if (reqSize == 0) {
      void * ptr = SuperHeap::malloc(sz);
      size_t s   = SuperHeap::getSize(ptr);
      SuperHeap::free (ptr);
      reqSize = s;
    }
    if (!_initialized) {
      fill (reqSize);
    }
#endif
    // Get an item from the superheap and swap it with a
    // randomly-chosen object from the array. This is one step of an
    // in-place Fisher-Yates shuffle.
    void * ptr = SuperHeap::malloc (reqSize);
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

  void fill (size_t sz) {
    // Fill up the buffer from the superheap.
    for (int i = 0; i < NObjects; i++) {
      _buffer(i) = SuperHeap::malloc (sz);
    }
    // Now shuffle it (Fisher-Yates).
    for (int i = NObjects - 1; i >= 1; i--) {
      // Pick a random integer, 0 ≤ j ≤ i, and swap with item i.
      int j = _rng.next() % (i+1);
      swap (_buffer(i), _buffer(j));
    }
    _initialized = true;
  }

  bool _initialized;

  /// The random number generator, used for shuffling and random selection.
  RandomNumberGenerator 	_rng;

  /// The buffer used to hold shuffled objects for heap requests.
  Array<NObjects, void *> 	_buffer;

};


#endif
