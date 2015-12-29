// -*- C++ -*-

#ifndef DH_RANDOMMMAP_H
#define DH_RANDOMMMAP_H

#include "heaplayers.h"
// #include "mmapwrapper.h"
// #include "mmapheap.h"
// #include "posixlock.h"
// #include "guard.h"

#include "madvisewrapper.h"
#include "mwc64.h"


class RandomMmap {
public:

  RandomMmap()
    : _pages (NULL)
  {
    unsigned long long bytes = CPUInfo::PageSize * (unsigned long long) PAGES;
    // Get a giant chunk of memory.

    _pages = (void *) MmapWrapper::map (bytes);

    if (_pages == NULL) {
      // Map failed!
      fprintf (stderr, "Unable to allocate memory.\n");
      abort();
    }
    // Tell the OS that it's going to be randomly accessed.
    MadviseWrapper::random (_pages, bytes);
    // And, if possible, that these pages should be mapped as superpages.
    MadviseWrapper::huge (_pages, bytes);
    // Protect all the pages.
    MmapWrapper::protect (_pages, bytes);
    // Initialize the bitmap.
    _bitmap.reserve (PAGES);
  }

  void * map (size_t sz) {
    Guard<PosixLockType> m (_lock);

    // Round up to the nearest number of pages required.
    unsigned long npages = (sz + CPUInfo::PageSize - 1) / CPUInfo::PageSize;

    // Randomly probe until we find a run of free pages.

  SEARCH_FOR_RUN_OF_FREE_PAGES:
    
    // Pick a random index that is not too close to the end
    // (otherwise, it would overrun the bitmap).
    unsigned long index = _rng.next() % (PAGES - npages + 1);
    assert (index + npages - 1 < PAGES);
    
    // Go through each page and try to set the bit for each one.
    
    for (unsigned int i = 0; i < npages; i++) {
      if (!_bitmap.tryToSet(index + i)) {
	// If we tried to set this bit but it was already set,
	// we did not find a run of enough free bits.
	// Reset all the bits up to i and we'll try again.
	for (unsigned long j = 0; j < i; j++) {
	  _bitmap.reset (index + j);
	}
	goto SEARCH_FOR_RUN_OF_FREE_PAGES;
      }
    }
      
    // We successfully set npages' worth of bits.
      
    // Compute the location of the memory to be returned.
    void * addr = (void *) ((char *) _pages + index * CPUInfo::PageSize);
    
    // Unprotect all the pages.
    MmapWrapper::unprotect (addr, npages * CPUInfo::PageSize);
    
    // Return the memory.
    return addr;
  }
  
  void unmap (void * ptr, size_t sz)
  {
    Guard<PosixLockType> m (_lock);

    // Round up to the nearest number of pages required.
    unsigned int npages = (sz + CPUInfo::PageSize - 1) / CPUInfo::PageSize;

    // Normalize the pointer (mask off the low-order bits).
    ptr = (void *) (((unsigned long) ptr) & ~(CPUInfo::PageSize-1));

    // Calculate its index.
    unsigned long index
      = ((unsigned long) ptr - (unsigned long) _pages) / CPUInfo::PageSize;

    // Mark the pages as unallocated.
    for (unsigned long i = 0; i < npages; i++) {
      _bitmap.reset (index + i);
    }
    // Re-protect the pages.
    MmapWrapper::protect (ptr, npages * CPUInfo::PageSize);

  }

private:

  // Disable copying and assignment.
  RandomMmap(const RandomMmap& r);
  RandomMmap& operator=(const RandomMmap& r);
  
#if __cplusplus > 199711L
  enum { BITS = 31 - staticlog(CPUInfo::PageSize) }; // size of address space, minus bits for pages.
#else
  enum { BITS = 31 - StaticLog<CPUInfo::PageSize>::VALUE }; // size of address space, minus bits for pages.
#endif

  enum { PAGES = (1ULL << BITS) }; // Must be a power of two.
  
  /// Random number generator for probing. note that it needs to produce 64-bit RNGs.
  MWC64 _rng;

  /// The bitmap that manages the heap: one bit per page.
  BitMap<MmapHeap> _bitmap;

  /// The range of available pages.
  void * _pages;

  /// The lock that guards these data structures.
  PosixLockType _lock;
};

#endif
