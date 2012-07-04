#ifndef DH_RANDOMMINIHEAP_DIEHARD
#define DH_RANDOMMINIHEAP_DIEHARD

#include "randomminiheap-core.h"

template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn>
class RandomMiniHeapDieHard :
  public RandomMiniHeapCore<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, true> {
public:

  /// @return the space remaining from this point in this object
  /// @nb Returns zero if this object is not managed by this heap.
  inline size_t getSize (void * ptr) {
    if (!inBounds(ptr)) {
      return 0;
    }

    // Compute offset corresponding to the pointer.
    size_t offset = computeOffset (ptr);
    
    // Return the space remaining in the object from this point.
    size_t remainingSize =     
      ObjectSize - modulo<ObjectSize>(offset);

    return remainingSize;
  }


  bool free (void * ptr) {
    bool didFree = SuperHeap::free (ptr);
    if (didFree) {
      if (DieFastOn) {
	// Check for overflows into adjacent objects,
	// then fill the freed object with a known random value.
	//	checkOverflowError (ptr, index);
	DieFast::fill (ptr, ObjectSize, SuperHeap::_freedValue);
      }
    }
    return didFree;
  }

  typedef RandomMiniHeapCore<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, true> SuperHeap;

protected:

  void activate() {
    if (!SuperHeap::_isHeapActivated) {

      // Go get memory for the heap.
      _miniHeap = (char *)
	Allocator::malloc (NObjects * ObjectSize);
      
      if (_miniHeap) {
	// Inform the OS that these pages will be accessed randomly.
	MadviseWrapper::random (_miniHeap, NObjects * ObjectSize);
	// Activate the bitmap.
	SuperHeap::_miniHeapBitmap.reserve (NObjects);
	if (DieFastOn) {
	  // Fill the empty space.
	  DieFast::fill (_miniHeap, NObjects * ObjectSize, SuperHeap::_freedValue);
	}
	
      } else {
	assert (0);
      }
      SuperHeap::_isHeapActivated = true;
    }
  }


  unsigned int computeIndex (void * ptr) const {
    size_t offset = computeOffset (ptr);
    if (IsPowerOfTwo<ObjectSize>::VALUE) {
      return (offset >> StaticLog<ObjectSize>::VALUE);
    } else {
      return (offset / ObjectSize);
    }
  }

  /// @return the distance of the object from the start of the heap.
  inline size_t computeOffset (void * ptr) const {
    size_t offset = ((size_t) ptr - (size_t) _miniHeap);
    return offset;
  }

  void * getObject (unsigned int index) {
    assert ((unsigned long) index < NObjects);

    if (!SuperHeap::_isHeapActivated) {
      return NULL;
    }

    return (void *) &((typename SuperHeap::ObjectStruct *) _miniHeap)[index];
  }

  /// @return true iff the index is valid for this heap.
  bool inBounds (void * ptr) const {
    if ((ptr < _miniHeap) || (ptr >= _miniHeap + NObjects * ObjectSize)
	|| (_miniHeap == NULL)) {
      return false;
    } else {
      return true;
    }
  }


  /// The heap pointer.
  char * _miniHeap;

};


#endif
