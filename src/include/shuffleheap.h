// -*- C++ -*-

#ifndef DH_SHUFFLEHEAP_H
#define DH_SHUFFLEHEAP_H

#include <algorithm>
#include <stdlib.h>

#include "array.h"
//#include "modulo.h"
#include "randomnumbergenerator.h"

template <size_t ChunkSize,
	  size_t MaxSize,
	  class SuperHeap>
class ShuffleHeap : public SuperHeap {
public:

  ShuffleHeap()
  {
    for (auto i = 0; i < NumBins; i++) {
      _is_filled[i] = false;
    }
  }

  inline void * malloc (size_t sz) {
    if (sz > MaxSize) {
      return SuperHeap::malloc(sz);
    }
    auto binIndex = SuperHeap::get_size_class(sz);
    auto reqSz = SuperHeap::get_class_size(binIndex);
    if (!_is_filled[binIndex]) {
      fill(binIndex);
      _is_filled[binIndex] = true;
    }
    // Get an item from the superheap and swap it with a
    // randomly-chosen object from the array. This is one step of an
    // in-place Fisher-Yates shuffle.
    void * ptr = SuperHeap::malloc(reqSz);
    int j = _rng.next() % (ChunkSize / reqSz);
    swap (_buffer[binIndex](j), ptr);
    return ptr;
  }


  inline void free (void * ptr) {
    // Choose a random item and evict it to the superheap,
    // replacing it with the one we are now freeing.
    auto reqSz = SuperHeap::getSize(ptr);
    auto binIndex = SuperHeap::get_size_class(reqSz);
    int j = _rng.next() % (ChunkSize / reqSz);
    swap (_buffer[binIndex](j), ptr);
    SuperHeap::free(ptr);
  }

 
private:

  void fill (int index) {
    const auto reqSz = SuperHeap::get_class_size(index);
    const auto NObjects = ChunkSize / reqSz;
    // Fill up the buffer from the superheap.
    for (int i = 0; i < NObjects; i++) {
      _buffer[index](i) = SuperHeap::malloc(reqSz);
    }
#if 0
    // Now shuffle it (Fisher-Yates).
    for (int i = NObjects - 1; i >= 1; i--) {
      // Pick a random integer, 0 ≤ j ≤ i, and swap with item i.
      int j = _rng.next() % (i+1);
      swap (_buffer(i), _buffer(j));
    }
#endif
  }


  static auto constexpr NumBins = ChunkSize / 8;
  
  bool _is_filled[NumBins];
  
  /// The random number generator, used for shuffling and random selection.
  RandomNumberGenerator 	_rng;

  /// The buffer used to hold shuffled objects for heap requests.
  Array<ChunkSize / 8, void *> 	_buffer[NumBins];

};


#endif
