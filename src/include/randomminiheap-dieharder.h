// -*- C++ -*-

#ifndef DH_RANDOMMINIHEAP_DIEHARDER
#define DH_RANDOMMINIHEAP_DIEHARDER

#include "randomminiheap-core.h"
#include "dieharder-pagetable.h"

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

  // There is one bitmap entry per object for large objects (bigger
  // than a page), and one per page for small objects.
  enum { NumEntries =
	 (ObjectSize <= CPUInfo::PageSize) ?
	 (SuperHeap::NumPages * 1) :
	 NObjects };


  void activate() {
    if (!SuperHeap::_isHeapActivated) {

      SuperHeap::_miniHeapBitmap.reserve (NObjects);

      for (int i = 0; i < NObjects; i += SuperHeap::ObjectsPerPage) {
	
	// Assume we already have full ASLR for now (FIX ME).
	void * pagesAddr = MmapWrapper::map (SuperHeap::PagesPerObject * CPUInfo::PageSize);
	
	// Fill with a known value for DieFast. FIX ME
	
	// Now initialize info for each page.
	
	for (int p = 0; p < SuperHeap::PagesPerObject; p++) {
	  
	  void * pageAddr = (void *) ((char *) pagesAddr + p * CPUInfo::PageSize);
	  
	  // Add an entry in the heap map that points to this page.
	  unsigned int mapIdx = (i >> StaticLog<SuperHeap::ObjectsPerPage>::VALUE);
	  _miniHeapMap(mapIdx) = pageAddr;
	  
	  // Now add a reverse entry for this page to point to this object index.
	  uintptr_t pageNumber = DieHarder::computePageNumber (pageAddr);
	  DieHarder::pageTable.getInstance().insert (pageNumber, this, i * SuperHeap::ObjectsPerPage);
	}
      }
      SuperHeap::_isHeapActivated = true;
    }
  }

  /// @return the index in the bitmap corresponding to the given object.
  unsigned int computeIndex (void * ptr) const {
    // Look up the stored index for this page.
    unsigned int pageIdx = getPageIndex (ptr);
    
    // If this was a big object, just return the index (since there is
    // only one index entry for that object).
    if (SuperHeap::ObjectsPerPage == 1) {
      return pageIdx;
    }

    // ptr belongs to a small object.
    // First, find out where it is on the page.
    unsigned long offset = ((unsigned long) ptr) & (CPUInfo::PageSize-1);
    
    // Now compute its index: the number of objects in PRECEDING pages
    // PLUS the object index WITHIN the page.
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

    if (ObjectSize > CPUInfo::PageSize) {
      return _miniHeapMap(index);
    } else {
      unsigned int mapIdx = (index >> StaticLog<SuperHeap::ObjectsPerPage>::VALUE);
      unsigned int whichObject = index & (SuperHeap::ObjectsPerPage-1);
      return (void *)
	&((typename SuperHeap::ObjectStruct *) _miniHeapMap(mapIdx))[whichObject];
    }
  }

  inline unsigned int getPageIndex (void * ptr) const {
    return DieHarder::pageTable.getInstance().getPageIndex (ptr);
  }

  /// @return true iff the index is valid for this heap.
  bool inBounds (void * ptr) const {
    if (!SuperHeap::_isHeapActivated) {
      return false;
    }
    return (DieHarder::pageTable.getInstance().getHeap (ptr) == this);
  }

  /// Sparse page pointer structure: _miniHeapMap[i] = ith page.
  CheckedArray<SuperHeap::NumPages, void *, Allocator> _miniHeapMap;

};



#endif


