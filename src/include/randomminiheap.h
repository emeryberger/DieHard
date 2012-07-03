// -*- C++ -*-

/**
 * @file   randomminiheap.h
 * @brief  Randomly allocates a particular object size in a range of memory.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 *
 * Copyright (C) 2006-12 Emery Berger, University of Massachusetts Amherst
 */

#ifndef DH_RANDOMMINIHEAP_H
#define DH_RANDOMMINIHEAP_H

#include <assert.h>

//extern "C" void reportDoubleFreeError (void);
//extern "C" void reportInvalidFreeError (void);
//extern "C" void reportOverflowError (void);


#include "heaplayers.h"

#include "bitmap.h"
#include "check.h"
#include "checkedarray.h"
#include "checkpoweroftwo.h"
// #include "cpuinfo.h"
#include "diefast.h"
#include "madvisewrapper.h"
#include "modulo.h"
#include "mypagetable.h"
#include "randomnumbergenerator.h"
// #include "sassert.h"
#include "staticlog.h"

class RandomMiniHeapBase {
public:

  virtual void * malloc (size_t) = 0; // { abort(); return 0; }
  virtual bool free (void *) = 0; // { abort(); return true; }
  virtual size_t getSize (void *) = 0; // { abort(); return 0; }
  virtual void activate (void) = 0; // { abort(); }
  virtual ~RandomMiniHeapBase () {}
};


/**
 * @class RandomMiniHeap
 * @brief Randomly allocates objects of a given size.
 * @param Numerator the heap multiplier numerator.
 * @param Denominator the heap multiplier denominator.
 * @param ObjectSize the object size managed by this heap.
 * @param NObjects the number of objects in this heap.
 * @param Allocator the source heap for allocations.
 * @sa    RandomHeap
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 **/
template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn,
	  bool DieHarderOn>
class RandomMiniHeap : public RandomMiniHeapBase {

  /// Check values for sanity checking.
  enum { CHECK1 = 0xEEDDCCBB, CHECK2 = 0xBADA0101 };
  
  enum { NumPages =
	 (NObjects * ObjectSize + CPUInfo::PageSize - 1)
	 / CPUInfo::PageSize };

  enum { NumPointers =
	 (ObjectSize <= CPUInfo::PageSize) ?
	 (NumPages * 1) :
	 NObjects };
	
  enum { ObjectsPerPage = 
	 (ObjectSize <= CPUInfo::PageSize) ?
	 CPUInfo::PageSize / ObjectSize :
	 1 };

  /// A convenience struct.
  typedef struct {
    char obj[ObjectSize];
  } ObjectStruct;

public:

  typedef RandomMiniHeapBase SuperHeap;

  RandomMiniHeap (void)
    : _check1 ((size_t) CHECK1),
      _freedValue (_random.next() | 1), // Enforce invalid pointer value.
      _isHeapActivated (false),
      _check2 ((size_t) CHECK2)
  {
    Check<RandomMiniHeap *> sanity (this);

    // All these need to be powers of two.

    CheckPowerOfTwo<ObjectSize>	invariant1;
    CheckPowerOfTwo<CPUInfo::PageSize> invariant2;
    CheckPowerOfTwo<ObjectsPerPage> invariant3;

    // Object size must be a multiple of page size (if they are bigger
    // than a page).
    sassert<((ObjectSize <= CPUInfo::PageSize)
	     || CPUInfo::PageSize * (ObjectSize / CPUInfo::PageSize) == ObjectSize)>
    invariant4;

    invariant1 = invariant1;
    invariant2 = invariant2;
    invariant3 = invariant3;
    invariant4 = invariant4;
  }


  /// @return an allocated object of size ObjectSize
  /// @param sz   requested object size
  /// @note May return NULL even though there is free space.
  inline void * malloc (size_t sz)
  {
    Check<RandomMiniHeap *> sanity (this);
    sz = sz; // to prevent warnings

    // Ensure size is reasonable.
    assert (sz <= ObjectSize);

    void * ptr = NULL;

    // Try to allocate an object from the bitmap.
    unsigned int index = modulo<NObjects> (_random.next());

    bool didMalloc = _miniHeapBitmap.tryToSet (index);

    if (!didMalloc) {
      return NULL;
    }

    // Get the address of the indexed object.
    assert ((unsigned long) index < NObjects);
    ptr = getObject (index);

    unsigned int computedIndex = computeIndex (ptr);
    assert (index == computedIndex);
    
    if (DieFastOn) {
      // Check to see if this object was overflowed.
      if (DieFast::checkNot (ptr, ObjectSize, _freedValue)) {
	//	reportOverflowError();
      }
    }

    // Make sure the returned object is the right size.
    assert (getSize(ptr) == ObjectSize);

    return ptr;
  }


  /// @return the space remaining from this point in this object
  /// @nb Returns zero if this object is not managed by this heap.
  inline size_t getSize (void * ptr) {
    Check<const RandomMiniHeap *> sanity (this);

    if (!inBounds(ptr)) {
      return 0;
    }

    size_t remainingSize;

    if (DieHarderOn) {

      // Return the space remaining in the object from this point.
      if (ObjectSize <= CPUInfo::PageSize) {
	remainingSize = ObjectSize - modulo<ObjectSize>(reinterpret_cast<uintptr_t>(ptr));
      } else {
	uintptr_t start = (uintptr_t) _miniHeapMap(getPageIndex(ptr));
	remainingSize = ObjectSize - ((uintptr_t) ptr - start);
      }

    } else {
      // Compute offset corresponding to the pointer.
      size_t offset = computeOffset (ptr);
      
      // Return the space remaining in the object from this point.
      remainingSize =     
	ObjectSize - modulo<ObjectSize>(offset);

    }

    return remainingSize;
  }

  /// @brief Relinquishes ownership of this pointer.
  /// @return true iff the object was on this heap and was freed by this call.
  inline bool free (void * ptr) {
    Check<RandomMiniHeap *> sanity (this);

    // Return false if the pointer is out of range.
    if (!inBounds(ptr)) {
      return false;
    }

    unsigned int index = computeIndex (ptr);
    assert (((unsigned long) index < NObjects));

    bool didFree = true;

    // Reset the appropriate bit in the bitmap.
    if (_miniHeapBitmap.reset (index)) {
      // We actually reset the bit, so this was not a double free.
      if (DieHarderOn) {
	// Trash the object: REQUIRED for DieHarder.
	memset (ptr, 0, ObjectSize);
      } else {
	if (DieFastOn) {
	  // Check for overflows into adjacent objects,
	  // then fill the freed object with a known random value.
	  checkOverflowError (ptr, index);
	  DieFast::fill (ptr, ObjectSize, _freedValue);
	}
      }
    } else {
      //      reportDoubleFreeError();
      didFree = false;
    }
    return didFree;
  }

  /// Sanity check.
  void check (void) const {
    assert ((_check1 == CHECK1) &&
	    (_check2 == CHECK2));
  }

private:

  /// @brief Activates the heap, making it ready for allocations.
  NO_INLINE void activate (void) {
    if (!_isHeapActivated) {
      if (DieHarderOn) {

	_miniHeapBitmap.reserve (NObjects);
	
	for (unsigned int i = 0; i < NumPointers; i++) {
	  activatePage (i);
	}

      } else {

	// Go get memory for the heap and the bitmap, making it ready
	// for allocations.
	_miniHeap = (char *)
	  Allocator::malloc (NObjects * ObjectSize);
	// Inform the OS that these pages will be accessed randomly.
	MadviseWrapper::random (_miniHeap, NObjects * ObjectSize);
	if (_miniHeap) {
	  _miniHeapBitmap.reserve (NObjects);
	  if (DieFastOn) {
	    DieFast::fill (_miniHeap, NObjects * ObjectSize, _freedValue);
	  }
	  
	} else {
	  assert (0);
	}
      }
      _isHeapActivated = true;
    }
  }

  /// @brief Activates the page or range of pages corresponding to the given index.
  inline void activatePage (unsigned int idx) {
    if (!DieHarderOn) {
      assert (0);
      return;
    }

    void * pages = MyPageTable::getInstance().allocatePages (this, idx, NumPages);

    _miniHeapMap(idx) = pages;
    
#if 0
    if (DieFastOn) {
      // Fill the contents with a known value.
      DieFast::fill (pages, NumPages * CPUInfo::PageSize, _freedValue);
    }
#endif
  }
  
  // Disable copying and assignment.
  RandomMiniHeap (const RandomMiniHeap&);
  RandomMiniHeap& operator= (const RandomMiniHeap&);

  /// @return the object at the given index.
  inline void * getObject (unsigned int index) {
    assert ((unsigned long) index < NObjects);
    if (!_isHeapActivated) {
      return NULL;
    }

    if (!DieHarderOn) {
      return (void *) &((ObjectStruct *) _miniHeap)[index];
    }
   
    if (ObjectSize > CPUInfo::PageSize) {
      return _miniHeapMap(index);
    } else {
      unsigned int mapIdx = (index >> StaticLog<ObjectsPerPage>::VALUE);
      unsigned int whichObject = index & (ObjectsPerPage-1);
      return (void *)
	&((ObjectStruct *) _miniHeapMap(mapIdx))[whichObject];
    }

  }

  /// @return the index corresponding to the given object.
  inline unsigned int computeIndex (void * ptr) const {

    if (!DieHarderOn) {

      size_t offset = computeOffset (ptr);
      if (IsPowerOfTwo<ObjectSize>::VALUE) {
	return (offset >> StaticLog<ObjectSize>::VALUE);
      } else {
	return (offset / ObjectSize);
      }

    } else {

      unsigned int pageIdx = getPageIndex (ptr);
      
      if (ObjectsPerPage == 1) {
	return pageIdx;
      }

      unsigned long offset = ((unsigned long) ptr) & (CPUInfo::PageSize-1);
      
      // Below calculation depends on ObjectSize being a power of two.

      unsigned int ret =
	(unsigned int) ((pageIdx * ObjectsPerPage) + 
			(offset >> StaticLog<ObjectSize>::VALUE));
	
      assert (ret < NObjects);
      return ret;
    }
  }

  /// @return the distance of the object from the start of the heap.
  inline size_t computeOffset (void * ptr) const {
    if (DieHarderOn) {
      assert (0);
      return 0;
    }
    size_t offset = ((size_t) ptr - (size_t) _miniHeap);
    return offset;
  }

  /// @brief Checks for overflows.
  /// NOTE: Disabled for DieHarder since it currently
  /// does not take into account non-contiguity.
  void checkOverflowError (void * ptr, unsigned int index)
  {
    ptr = ptr;
    index = index;
    if (!DieHarderOn) {
      // Check predecessor.
      if (!_miniHeapBitmap.isSet (index - 1)) {
	void * p = (void *) (((ObjectStruct *) ptr) - 1);
	if (DieFast::checkNot (p, ObjectSize, _freedValue)) {
	  //	reportOverflowError();
	}
      }
      // Check successor.
      if ((index < (NObjects - 1)) &&
	  (!_miniHeapBitmap.isSet (index + 1))) {
	void * p = (void *) (((ObjectStruct *) ptr) + 1);
	if (DieFast::checkNot (p, ObjectSize, _freedValue)) {
	  //	reportOverflowError();
	}
      }
    }

  }

  /// @return true iff the index is valid for this heap.
  inline bool inBounds (void * ptr) const {
    if (!DieHarderOn) {
      if ((ptr < _miniHeap) || (ptr >= _miniHeap + NObjects * ObjectSize)
	  || (_miniHeap == NULL)) {
	return false;
      } else {
	return true;
      }

    } else {

      if (!_isHeapActivated) {
	return false;
      }

      PageTableEntry entry;
      bool result = MyPageTable::getInstance().getPageTableEntry (ptr, entry);
      
      if (!result) {
	return false;
      }

      return (entry.getHeap() == this);
    }
  }


  /// @return true iff heap is currently active.
  inline bool isActivated (void) const {
    return _isHeapActivated;
  }
  
  inline unsigned int getPageIndex (void * ptr) const {
    PageTableEntry entry;
    bool result = MyPageTable::getInstance().getPageTableEntry (ptr, entry);

    if (result) {
      return entry.getPageIndex();
    } else {
      // This should never happen.
      assert (false);
      return 0;
    }
  }

  /// Sanity check value.
  const size_t _check1;

  /// A local random number generator.
  RandomNumberGenerator _random;
  
  /// A random value used to overwrite freed space for debugging (with DieFast).
  const size_t _freedValue;

  /// The bitmap for this heap (if DieHarderOn is true).
  BitMap<Allocator> _miniHeapBitmap;

  /// Sparse page pointer structure (if DieHarderOn is true).
  CheckedArray<NumPointers, void *, Allocator> _miniHeapMap;

  /// The heap pointer (if DieHarderOn is false).
  char * _miniHeap;

  /// True iff the heap has been activated.
  bool _isHeapActivated;

  /// Sanity check value.
  const size_t _check2;

};


#endif

