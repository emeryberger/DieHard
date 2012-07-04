#ifndef DH_RANDOMMINIHEAP_DIEHARDER
#define DH_RANDOMMINIHEAP_DIEHARDER

#include "randomminiheap-core.h"

template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn>
class RandomMiniHeapDieHarder :
  public RandomMiniHeapCore<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, false> {
public:

  typedef RandomMiniHeapCore<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, false> SuperHeap;

  inline size_t getSize (void * ptr) {
    size_t remainingSize;
    
    // Return the space remaining in the object from this point.
    if (ObjectSize <= CPUInfo::PageSize) {
      remainingSize = ObjectSize - modulo<ObjectSize>(reinterpret_cast<uintptr_t>(ptr));
    } else {
      uintptr_t start = (uintptr_t) _miniHeapMap(getPageIndex(ptr));
      remainingSize = ObjectSize - ((uintptr_t) ptr - start);
    }
    return remainingSize;
  }

  bool free (void * ptr) {
    bool didFree = SuperHeap::free (ptr);
    if (didFree) {
      // Trash the object: REQUIRED for DieHarder.
      memset (ptr, 0, ObjectSize);
    }
    return didFree;
  }

protected:

  /// There is one pointer per object for large objects (bigger than a page),
  /// and one per page for small objects.
  enum { NumPointers =
	 (ObjectSize <= CPUInfo::PageSize) ?
	 (SuperHeap::NumPages * 1) :
	 NObjects };


  void activate() {
    if (!SuperHeap::_isHeapActivated) {
      SuperHeap::_miniHeapBitmap.reserve (NObjects);
      
      for (unsigned int index = 0; index < NumPointers; index++) {
	void * pages = 
	  MyPageTable::getInstance().allocatePages (this, index, SuperHeap::NumPages);
	
	_miniHeapMap(index) = pages;
	
#if 0
	if (DieFastOn) {
	  // Fill the contents with a known value.
	  DieFast::fill (pages, NumPages * CPUInfo::PageSize, SuperHeap::_freedValue);
	}
#endif
      }
      SuperHeap::_isHeapActivated = true;
    }
  }

  unsigned int computeIndex (void * ptr) const {
    unsigned int pageIdx = getPageIndex (ptr);
    
    if (SuperHeap::ObjectsPerPage == 1) {
      return pageIdx;
    }
    
    unsigned long offset = ((unsigned long) ptr) & (CPUInfo::PageSize-1);
    
    // Below calculation depends on ObjectSize being a power of two.
    
    unsigned int ret =
      (unsigned int) ((pageIdx * SuperHeap::ObjectsPerPage) + 
		      (offset >> StaticLog<ObjectSize>::VALUE));
    
    assert (ret < NObjects);
    return ret;
  }


  void * getObject (unsigned int index) {
    assert ((unsigned long) index < NObjects);

    if (!SuperHeap::_isHeapActivated) {
      return NULL;
    }

    unsigned int mapIdx = (index >> StaticLog<SuperHeap::ObjectsPerPage>::VALUE);
    unsigned int whichObject = index & (SuperHeap::ObjectsPerPage-1);
    return (void *)
      &((typename SuperHeap::ObjectStruct *) _miniHeapMap(mapIdx))[whichObject];
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

  /// @return true iff the index is valid for this heap.
  bool inBounds (void * ptr) const {
    if (!SuperHeap::_isHeapActivated) {
      return false;
    }
    
    PageTableEntry entry;
    bool result = MyPageTable::getInstance().getPageTableEntry (ptr, entry);
    
    if (!result) {
      return false;
    }
    
    return (entry.getHeap() == this);
  }

  /// Sparse page pointer structure (if DieHarderOn is true).
  CheckedArray<NumPointers, void *, Allocator> _miniHeapMap;

};



#endif


