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
class RandomMiniHeapDieHarderBase :
  public RandomMiniHeapCore<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, true> 
{
public:

  typedef RandomMiniHeapCore<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, true> SuperHeap;

  bool free (void * ptr) {
    bool didFree = SuperHeap::free (ptr);
    if (didFree) {
      // Trash the object: REQUIRED for DieHarder.
      memset (ptr, 0, ObjectSize);
    }
    return didFree;
  }

  void activate() {
    if (!SuperHeap::_isHeapActivated) {

      // First, set up the bitmap with one entry per object.
      SuperHeap::_miniHeapBitmap.reserve (NObjects);

      // Now map the pages. Small objects that fit on a page will live
      // in separately-mapped individual page. Large objects are
      // allocated one at a time (individual mmap calls).

      for (int objectIndex = 0;
	   objectIndex < NObjects;
	   objectIndex += SuperHeap::ObjectsPerPage) {
	
	// Assume we already have full ASLR for now (FIX ME).
	// That is, that every map is randomly distributed through the address space.
	// We will otherwise need to use RandomMmap.
	void * pagesAddr =
	  MmapWrapper::map (SuperHeap::PagesPerObject * CPUInfo::PageSize);
	
	// Add an entry in the heap map that points to the start of this object.
	setPageFromIndex (objectIndex, pagesAddr);
	  
	// Fill with a known value for DieFast. FIX ME

	// Initialize bookkeeping information.

	for (int p = 0; p < SuperHeap::PagesPerObject; p++) {
	  
	  void * pageAddr = (void *) ((char *) pagesAddr + p * CPUInfo::PageSize);
	  
	  // Now add a reverse entry for this page to point to this object index.
	  // pageTable:: maps page number to (heap, page index).
	  uintptr_t pageNumber = DieHarder::computePageNumber (pageAddr);
	  DieHarder::pageTable.getInstance().insert (pageNumber,
						     this,
						     objectIndex);
	}
      }
      SuperHeap::_isHeapActivated = true;
    }
  }

private:

  /// @return true iff the index is valid for this heap.
  bool inBounds (void * ptr) const {
    if (!SuperHeap::_isHeapActivated) {
      return false;
    }
    return (DieHarder::pageTable.getInstance().getHeap (ptr) == this);
  }

protected:

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
      return getPageFromIndex (index);
    } else {
      unsigned int mapIdx = (index >> StaticLog<SuperHeap::ObjectsPerPage>::VALUE);
      unsigned int whichObject = index & (SuperHeap::ObjectsPerPage-1);
      return (void *)
	&((typename SuperHeap::ObjectStruct *) getPageFromIndex (index))[whichObject];
    }
  }


  // DieHarder maintains bookkeeping info for each page:
  // - given an address, find the corresponding object start
  //   (using pageTable, for free() and getSize())
  // - given the index of an object in the bitmap, find the object start
  //   (using _mapEntryToPage, for malloc()).

  inline unsigned int getPageIndex (void * ptr) const {
    return DieHarder::pageTable.getInstance().getPageIndex (ptr);
  }

  //@brief Add an entry in the heap map that points to this page.
  void setPageFromIndex (unsigned int index, void * pageAddr) {
    assert (((uintptr_t) pageAddr & (CPUInfo::PageSize - 1)) == 0);
    assert (modulo<SuperHeap::ObjectsPerPage>(index) == index);
    _mapEntryToPage(index) = pageAddr;
  }

  //@brief Get the page corresponding to this object index.
  void * getPageFromIndex (unsigned int index) {
    // Pick the index that is at the start of a page.
    index -= modulo<SuperHeap::ObjectsPerPage>(index);
    void * pageAddr = _mapEntryToPage(index);
    assert (((uintptr_t) pageAddr & (CPUInfo::PageSize - 1)) == 0);
    return pageAddr;
  }

  // There is one bitmap entry per page for small objects,
  // and one bitmap entry per object for large objects.
  enum { NumEntries =
	 (ObjectSize <= CPUInfo::PageSize) ?
	 (SuperHeap::NumPages * 1) :
	 NObjects };

  // Sparse page pointer structure:
  // maps entries (the one at the start of a page, for small objects;
  // or the index of individual large objects) to their start address.
  CheckedArray<NumEntries, void *, Allocator> _mapEntryToPage;

};

template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn,
	  bool BigObjects>
class RandomMiniHeapDieHarderChoose;



// SMALL objects (<= page size).

template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn>
class RandomMiniHeapDieHarderChoose<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, false> :
  public RandomMiniHeapDieHarderBase<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn> {

public:

  typedef RandomMiniHeapDieHarderBase<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn> SuperHeap;


  RandomMiniHeapDieHarderChoose()
  {
    sassert<(ObjectSize <= CPUInfo::PageSize)> verifySize;
  }

  inline size_t getSize (void * ptr) {
    size_t remainingSize;
    
    // Return the space remaining in the object from this point.
    // For a small object, we can just do pointer math because
    // allocations are page-aligned.
    remainingSize = ObjectSize - modulo<ObjectSize>(reinterpret_cast<uintptr_t>(ptr));
    return remainingSize;
  }

};



/// Large objects.

template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn>
class RandomMiniHeapDieHarderChoose<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, true>  : public RandomMiniHeapDieHarderBase<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn> {

public:

  typedef RandomMiniHeapDieHarderBase<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn> SuperHeap;

  RandomMiniHeapDieHarderChoose()
  {
    sassert<(ObjectSize >= CPUInfo::PageSize)> verifySize;
  }
  
  inline size_t getSize (void * ptr) {
    // For a large object, we need to find out where it begins, then
    // compute the pointer's distance from the end.
    
    // First, find the object index corresponding to this object.
    unsigned int index = SuperHeap::computeIndex (ptr);
    
    // Now look up the object start and then compute the distance to the end.
    void * start = SuperHeap::getObject (index);
    size_t remainingSize = ObjectSize - ((uintptr_t) ptr - (uintptr_t) start);
    
    return remainingSize;
  }

};
  

template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn>
class RandomMiniHeapDieHarder :
  public RandomMiniHeapDieHarderChoose<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, (ObjectSize >= CPUInfo::PageSize)> {};


#endif


