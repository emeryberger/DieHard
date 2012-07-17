// -*- C++ -*-

#ifndef DH_SHUFFLEHEAP_H
#define DH_SHUFFLEHEAP_H

#include <stdlib.h>

#include "array.h"
#include "randomnumbergenerator.h"

/*

To initialize an array a to a randomly shuffled copy of source whose length is not known:
  a[0] ← source.next
  size = 1
  while source.moreDataAvailable
      j ← random integer with 0 ≤ j ≤ size
      a[size] ← a[j]
      a[j] ← source.next
      size += 1

 */


template <int NObjects, size_t Size, class SuperHeap>
class ShuffleHeap : public SuperHeap {
public:

  ShuffleHeap()
    : _length (0)
  {}
  
  void * malloc (size_t sz) {
    assert (Size >= sz);
    if (_length == 0) {
      // Empty: try to refill.
      if (!refill()) {
	return NULL;
      }
    }
    _length--;
    void * ptr = _item(_length);
    return ptr;
  }

  void free (void * ptr) {
    if (_length == NObjects) {
      // Full: free to the super heap.
      SuperHeap::free (ptr);
      return;
    }
    randomPlace (ptr);
  }

  
private:

  void randomPlace (void * ptr) {
    assert (_length < NObjects);
    // Put the item into a random location in the array,
    // swapping that item with the end of the array.
    // This is one step of an in-place Fisher-Yates shuffle.
    int j = _rng.next() % (_length + 1);
    _item(_length) = _item(j);
    _item(j)       = ptr;
    _length++;
  }

  bool refill() {
    for (int i = 0; i < NObjects; i++) {
      void * ptr = SuperHeap::malloc (Size);
      if (!ptr) {
	return false;
      }
      free (ptr);
    }
    return true;
  }

  RandomNumberGenerator 	_rng;
  int 				_length;
  Array<NObjects, void *> 	_item;
};


#endif
