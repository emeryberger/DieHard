// -*- C++ -*-

/**
 * @file   randomminiheap.h
 * @brief  Randomly allocates a particular object size in a range of memory.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 *
 * Copyright (C) 2006 Emery Berger, University of Massachusetts Amherst
 */

#ifndef _RANDOMMINIHEAP_H_
#define _RANDOMMINIHEAP_H_

#include <assert.h>

extern "C" void reportDoubleFreeError (void);
extern "C" void reportInvalidFreeError (void);
extern "C" void reportOverflowError (void);


#include "bitmap.h"
#include "check.h"
#include "checkpoweroftwo.h"
#include "diefast.h"
#include "modulo.h"
#include "randomnumbergenerator.h"
#include "sassert.h"
#include "staticlog.h"
#include "threadlocal.h"
#include "pagetable.h"

class RandomMiniHeapBase {
public:

  virtual void * malloc (size_t) { abort(); return 0; }
  virtual bool free (void *) { abort(); return true; }
  virtual size_t getSize (void *) { abort(); return 0; }
  virtual void activate (void) { abort(); }
  virtual ~RandomMiniHeapBase () { abort(); }
};


/**
 * @class RandomMiniHeap
 * @brief Randomly allocates objects of a given size.
 * @param Numerator the heap multiplier numerator.
 * @param Denominator the heap multiplier denominator.
 * @param ObjectSize the object size managed by this heap.
 * @sa    RandomHeap
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 **/
template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn>
class RandomMiniHeap : public RandomMiniHeapBase {

  /// Check values for sanity checking.
  enum { CHECK1 = 0xEEDDCCBB, CHECK2 = 0xBADA0101 };
  
  enum { NumPages = (NObjects * ObjectSize) / PAGE_SIZE };
  enum { ObjectsPerPage = StaticIf<ObjectSize < PAGE_SIZE, PAGE_SIZE / ObjectSize, 1>::VALUE };

  /// A convenience struct.
  typedef struct {
    char obj[ObjectSize];
  } ObjectStruct;

  friend class Check<RandomMiniHeap *>;


public:

  typedef RandomMiniHeapBase SuperHeap;

  RandomMiniHeap (void)
    : _check1 ((size_t) CHECK1),
      _freedValue (((RandomNumberGenerator *) _random)->next() | 1), // Enforce invalid pointer value.
      _miniHeapMap (NULL),
      _isHeapIntact (true),
      _check2 ((size_t) CHECK2)
  {
    //    printf ("freed value = %ld\n", _freedValue);
    //    printf ("rng = %p\n", rng);

    Check<RandomMiniHeap *> sanity (this);

    /// Some sanity checking.
    CheckPowerOfTwo<ObjectSize>	_SizeIsPowerOfTwo;

    _SizeIsPowerOfTwo = _SizeIsPowerOfTwo; // to prevent warnings
  }

  bool isIntact (void) const {
    return _isHeapIntact;
  }

#if 0
  /*

   support for per-thread allocation. the idea is to submit multiple
   requests, proportional to the size of the random mini heap in order
   to minimize locked access to the heaps.

   for example:
   in a small heap, grab 1 (as normal)
   in the next heap (twice as big), grab 2
   etc.

   so: if you grab K objects total,
   you'll grab K/2 from the biggest heap,
   K/4 from the next smallest, etc.

   in other words, malloc of K objects will now grab locks log_2(K)
   times rather than K times.

   when we free objects, we will stash them into a buffer off to the side.
   odds are good that we will free 1/2 of the objects from the largest heap,
   etc.
   when the buffer fills, we go through all heaps and dump them in the
   right heaps.

   in other words, free of K objects will also grab locks log_2(K)
   times rather than K times.

   one caveat: we need to make sure that the miniheaps are empty
   enough for this to work properly -- so K can't be too large, or we
   need to ensure that we are below the fullness threshold by at least
   K.

   */

  inline bool manymalloc (size_t sz, int nObjects, void ** obj) {
    bool allGood = true;
    for (int i = 0; i < nObjects; i++) {
      void * ptr = malloc (sz);
      if (!ptr) {
	allGood = false;
      }
      obj[i] = ptr;
    }
    return allGood;
  }

  inline void manyfree (int nObjects, void ** obj) {
    for (int i = 0; i < nObjects; i++) {
      free (obj[i]);
    }
  }
#endif

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
    unsigned int index = (unsigned int) modulo<NObjects> (((RandomNumberGenerator *) _random)->next());

    bool didMalloc = _miniHeapBitmap.tryToSet (index);

    if (!didMalloc) {
      return NULL;
    }

    // Get the address of the indexed object.
    assert ((unsigned long) index < NObjects);
    ptr = getObject (index);
    
    //fprintf(stderr,"(%d %d) %p, %d, %d\n",ObjectSize,LogObjectsPerPage,ptr,index,computeIndex(ptr));
    assert (index == computeIndex(ptr));
    
    if (DieFastOn) {
      // Check to see if this object was overflowed.
      if (DieFast::checkNot (ptr, ObjectSize, _freedValue)) {
	_isHeapIntact = false;
	reportOverflowError();
      }
    }

    assert (getSize(ptr) >= sz);
    assert (getSize(ptr) == ObjectSize);

    return ptr;
  }


  /// @return the space remaining from this point in this object
  /// @nb Returns zero if this object is not managed by this heap.
  inline size_t getSize (void * ptr) {
    Check<RandomMiniHeap *> sanity (this);

    if (!inBounds(ptr)) {
      return 0;
    }

    // Return the space remaining in the object from this point.
    size_t remainingSize;
    if(ObjectSize <= PAGE_SIZE)
      remainingSize = ObjectSize - modulo<ObjectSize>(reinterpret_cast<uintptr_t>(ptr));
    else {
      uintptr_t start = (uintptr_t)_miniHeapMap[getPageIndex(ptr)];
      remainingSize = ObjectSize - ((uintptr_t)ptr - start);
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
      if (DieFastOn) {
	checkOverflowError (ptr, index);
	// Trash the object.
	DieFast::fill (ptr, ObjectSize, _freedValue);
      }
    } else {
      reportDoubleFreeError();
      didFree = false;
    }
    return didFree;
  }

private:


  /// @brief Activates the heap, making it ready for allocations.
  NO_INLINE void activate (void) {
    if (_miniHeapMap == NULL) {
      // Go get memory for the heap and the bitmap, making it ready
      // for allocations.
      size_t ct = (ObjectSize <= PAGE_SIZE) ? (size_t) NumPages : NObjects;
      
      _miniHeapMap = (void **)
        Allocator::malloc (ct * sizeof(void*));
      if (_miniHeapMap) {
        _miniHeapBitmap.reserve (NObjects);

        for (unsigned int i = 0; i < ct; i++) {
          activatePage(i);
        }
        
      } else {
        assert (0);
      }
    }
  }
  
  inline void activatePage(unsigned int idx) {
    void * page;
    
    if (ObjectSize <= PAGE_SIZE) {
      page = PageTable::getInstance().allocatePage(this,idx);
    } else {
      page = PageTable::getInstance().allocatePageRange(this,idx,ObjectSize / PAGE_SIZE);
    }
    
    //fprintf(stderr,"activated page %p\n",page);
    
    _miniHeapMap[idx] = page;
    
    if(DieFastOn) {
      DieFast::fill (page, ObjectSize < PAGE_SIZE ? PAGE_SIZE : ObjectSize, _freedValue);
    }
  }
  
  // Disable copying and assignment.
  RandomMiniHeap (const RandomMiniHeap&);
  RandomMiniHeap& operator= (const RandomMiniHeap&);

  /// Sanity check.
  void check (void) const {
    assert ((_check1 == CHECK1) &&
	    (_check2 == CHECK2));
  }

  /// @return the object at the given index.
  inline void * getObject (unsigned int index) const {
    assert ((unsigned long) index < NObjects);
    assert (_miniHeapMap != NULL);
    
    if(ObjectSize > PAGE_SIZE) {
      return _miniHeapMap[index];
    } else {
      unsigned int mapIdx = (index >> StaticLog<ObjectsPerPage>::VALUE);
      return (void *) &((ObjectStruct *) _miniHeapMap[mapIdx])[index & (ObjectsPerPage-1)];
    }
  }

  /// @return the index corresponding to the given object.
  inline unsigned int computeIndex (void * ptr) const {
    assert (inBounds(ptr));
    unsigned int pageIdx = getPageIndex (ptr);
    
    if(ObjectsPerPage == 1) {
      return pageIdx;
    }
    
    unsigned long offset = ((unsigned long)(ptr) & (PAGE_SIZE-1));
    
    //fprintf(stderr,"pidx = %d, offset = %d\n",pageIdx,offset);
    
    if (IsPowerOfTwo<ObjectSize>::VALUE) {
      unsigned int ret = (unsigned int) ((pageIdx * ObjectsPerPage) + 
                      (offset >> StaticLog<ObjectSize>::VALUE));
      
      assert(ret < NObjects);
      return ret;
    } else {
      assert(false); // SDH
      //return (int) (offset / ObjectSize);
    }
  }

  /// @brief Checks if the predecessor or successor have been overflowed.
  /// SDH: XXX needs to take into account non-contiguity
  void checkOverflowError (void * ptr, unsigned int index)
  {
    ptr = ptr;
    index = index;
#if 0 // FIX ME temporarily disabled.
    // Check predecessor.
    if (!_miniHeapBitmap.isSet (index - 1)) {
      void * p = (void *) (((ObjectStruct *) ptr) - 1);
      if (DieFast::checkNot (p, ObjectSize, _freedValue)) {
	_isHeapIntact = false;
	reportOverflowError();
      }
    }
    // Check successor.
    if ((index < (NObjects - 1)) &&
	(!_miniHeapBitmap.isSet (index + 1))) {
      void * p = (void *) (((ObjectStruct *) ptr) + 1);
      if (DieFast::checkNot (p, ObjectSize, _freedValue)) {
	_isHeapIntact = false;
	reportOverflowError();
      }
    }
#endif
  }

  /// @return true iff the index is invalid for this heap.
  inline bool inBounds (void * ptr) const {
    if (_miniHeapMap == NULL)
      return false;

    PageTableEntry * entry =
      PageTable::getInstance().getPageTableEntry ((uintptr_t)ptr);
    return entry && (entry->heap == this);
  }

  /// @return true iff heap is currently active.
  inline bool isActivated (void) const {
    return (_miniHeapMap != NULL);
  }
  
  inline unsigned int getPageIndex(void * ptr) const {
    PageTableEntry * entry = PageTable::getInstance().getPageTableEntry((uintptr_t)ptr);

    assert(entry != NULL);

    if(entry) {
      assert(entry->heap == this);
      return entry->pageIndex;
    }
   
    abort(); 
    return 0;
  }

  /// Sanity check value.
  const size_t _check1;

  /// A local random number generator.
  threadlocal<RandomNumberGenerator> _random;
  
  /// A random value used to overwrite freed space for debugging (with DieFast).
  const size_t _freedValue;

  /// The bitmap for this heap.
  BitMap<Allocator> _miniHeapBitmap;

  /// Sparse page pointer structure.
  void ** _miniHeapMap;

  /// True iff the heap is intact (and DieFastOn is true).
  bool _isHeapIntact;

  /// Sanity check value.
  const size_t _check2;

};


#endif

