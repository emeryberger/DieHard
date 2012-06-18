/* -*- C++ -*- */

/**
 * @file segheap.h
 * @brief Definitions of SegHeap and StrictSegHeap.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#ifndef _SEGHEAP_H_
#define _SEGHEAP_H_

  /**
   * @class SegHeap
   * @brief A segregated-fits collection of (homogeneous) heaps.
   * @author Emery Berger
   *
   * Note that one extra heap is used for objects that are "too big".
   *
   * @param NumBins The number of bins (subheaps).
   * @param getSizeClass Function to compute size class from size.
   * @param getClassMaxSize Function to compute the largest size for a given size class.
   * @param LittleHeap The subheap class.
   * @param BigHeap The parent class, used for "big" objects.
   *
   * Example:<BR>
   * <TT>
   *  int myFunc (size_t sz); // The size-to-class function.<BR>
   *  size_t myFunc2 (int); // The class-to-size function.<P>
   *  // The heap. Use freelists for these small objects,<BR>
   *  // but defer to malloc for large objects.<P>
   *
   * SegHeap<4, myFunc, myFunc2, freelistHeap<mallocHeap>, mallocHeap> mySegHeap;
   * </TT>
   **/

#include <assert.h>
#include "bitstring.h"

  namespace HL {

    template <int NumBins,
	      int (*getSizeClass) (const size_t),
	      size_t (*getClassMaxSize) (const int),
	      class LittleHeap,
	      class BigHeap>
    class SegHeap : public LittleHeap {
    private:

      typedef int (*scFunction) (const size_t);
      typedef size_t (*csFunction) (const int);

    public:

      inline SegHeap (void)
	: memoryHeld (0),
	  maxObjectSize (((csFunction) getClassMaxSize) (NumBins - 1))
      {
	for (int i = 0; i < NUM_ULONGS; i++) {
	  binmap[i] = 0;
	}
      }

      inline ~SegHeap (void) {}

      inline size_t getMemoryHeld (void) const {
	return memoryHeld;
      }

      // new...
      size_t getSize (void * ptr) {
	return LittleHeap::getSize (ptr);
      }

      inline void * malloc (const size_t sz) {
	void * ptr = NULL;
	if (sz > maxObjectSize) {
	  goto GET_MEMORY;
	}

	{
	  const int sc = ((scFunction) getSizeClass)(sz);
	  assert (sc >= 0);
	  assert (sc < NumBins);
	  int idx = sc;
	  int block = idx2block (idx);
	  unsigned long map = binmap[block];
	  unsigned long bit = idx2bit (idx);
    
	  for (;;) {
	    if (bit > map || bit == 0) {
	      do {
		if (++block >= NUM_ULONGS) {
		  goto GET_MEMORY;
		  // return bigheap.malloc (sz);
		}
	      } while ( (map = binmap[block]) == 0);

	      idx = block << SHIFTS_PER_ULONG;
	      bit = 1;
	    }
	
	    while ((bit & map) == 0) {
	      bit <<= 1;
	      assert(bit != 0);
	      idx++;
	    }
      
	    assert (idx < NumBins);
	    ptr = myLittleHeap[idx].malloc (sz);
      
	    if (ptr == NULL) {
	      binmap[block] = map &= ~bit; // Write through
	      idx++;
	      bit <<= 1;
	    } else {
	      return ptr;
	    }
	  }
	}

      GET_MEMORY:
	if (ptr == NULL) {
	  // There was no free memory in any of the bins.
	  // Get some memory.
	  ptr = bigheap.malloc (sz);
	}
    
	return ptr;
      }


      inline bool free (void * ptr) {
	// printf ("Free: %x (%d bytes)\n", ptr, getSize(ptr));
	const size_t objectSize = getSize(ptr); // was bigheap.getSize(ptr)
	if (objectSize > maxObjectSize) {
	  // printf ("free up! (size class = %d)\n", objectSizeClass);
	  return bigheap.free (ptr);
	} else {
	  int objectSizeClass = ((scFunction) getSizeClass) (objectSize);
	  assert (objectSizeClass >= 0);
	  assert (objectSizeClass < NumBins);
	  // Put the freed object into the right sizeclass heap.
	  assert (getClassMaxSize(objectSizeClass) >= objectSize);
#if 1
	  while (((csFunction) getClassMaxSize)(objectSizeClass) > objectSize) {
	    objectSizeClass--;
	  }
#endif
	  assert (((csFunction) getClassMaxSize)(objectSizeClass) <= objectSize);
	  if (objectSizeClass > 0) {
	    assert (objectSize >= ((csFunction) getClassMaxSize)(objectSizeClass - 1));
	  }


	  myLittleHeap[objectSizeClass].free (ptr);
	  mark_bin (objectSizeClass);
	  memoryHeld += objectSize;
	  return true;
	}
      }


      void clear (void) {
	int i;
	for (i = 0; i < NumBins; i++) {
	  myLittleHeap[i].clear();
	}
	for (int j = 0; j < NUM_ULONGS; j++) {
	  binmap[j] = 0;
	}
	// bitstring.clear();
	bigheap.clear();
	memoryHeld = 0;
      }

    private:

      enum { BITS_PER_ULONG = 32 };
      enum { SHIFTS_PER_ULONG = 5 };
      enum { MAX_BITS = (NumBins + BITS_PER_ULONG - 1) & ~(BITS_PER_ULONG - 1) };


    private:

      static inline int idx2block (int i) {
	int blk = i >> SHIFTS_PER_ULONG;
	assert (blk < NUM_ULONGS);
	assert (blk >= 0);
	return blk;
      }

      static inline unsigned long idx2bit (int i) {
	unsigned long bit = ((1U << ((i) & ((1U << SHIFTS_PER_ULONG)-1))));
	return bit;
      }


    protected:

      BigHeap bigheap;

      enum { NUM_ULONGS = MAX_BITS / BITS_PER_ULONG };
      unsigned long binmap[NUM_ULONGS];

      inline int get_binmap (int i) const {
	return binmap[i >> SHIFTS_PER_ULONG] & idx2bit(i);
      }

      inline void mark_bin (int i) {
	binmap[i >> SHIFTS_PER_ULONG] |=  idx2bit(i);
      }

      inline void unmark_bin (int i) {
	binmap[i >> SHIFTS_PER_ULONG] &= ~(idx2bit(i));
      }

      size_t memoryHeld;

      const size_t maxObjectSize;

      // The little heaps.
      LittleHeap myLittleHeap[NumBins];

    };

  };


/**
 * @class StrictSegHeap
 * @brief A "strict" segregated-fits collection of (homogeneous) heaps.
 * 
 * One extra heap is used for objects that are "too big".  Unlike
 * SegHeap, StrictSegHeap does not perform splitting to satisfy memory
 * requests. If no memory is available from the appropriate bin,
 * malloc returns NULL.
 *
 * @sa SegHeap
 *
 * @param NumBins The number of bins (subheaps).
 * @param getSizeClass Function to compute size class from size.
 * @param getClassMaxSize Function to compute the largest size for a given size class.
 * @param LittleHeap The subheap class.
 * @param BigHeap The parent class, used for "big" objects.
 */

namespace HL {

  template <int NumBins,
	    int (*getSizeClass) (const size_t),
	    size_t (*getClassMaxSize) (const int),
	    class LittleHeap,
	    class BigHeap>
  class StrictSegHeap :
    public SegHeap<NumBins, getSizeClass, getClassMaxSize, LittleHeap, BigHeap>
  {
  private:

    typedef SegHeap<NumBins, getSizeClass, getClassMaxSize, LittleHeap, BigHeap> super;

    typedef int (*scFunction) (const size_t);
    typedef size_t (*csFunction) (const int);

  public:

    void freeAll (void) {
      int i;
      for (i = 0; i < NumBins; i++) {
	const size_t sz = ((csFunction) getClassMaxSize)(i);
	void * ptr;
	while ((ptr = super::myLittleHeap[i].malloc (sz)) != NULL) {
	  super::bigheap.free (ptr);
	}
      }
      for (int j = 0; j < super::NUM_ULONGS; j++) {
	super::binmap[j] = 0;
      }
      super::memoryHeld = 0;
    }

  
    /**
     * Malloc from exactly one available size.
     * (don't look in every small heap, as in SegHeap).
     */

    inline void * malloc (const size_t sz) {
      void * ptr = NULL;
      if (sz <= super::maxObjectSize) {
	const int sizeClass = ((scFunction) getSizeClass) (sz);
	assert (sizeClass >= 0);
	assert (sizeClass < NumBins);
	ptr = super::myLittleHeap[sizeClass].malloc (sz);
      }
      if (!ptr) {
	ptr = super::bigheap.malloc (sz);
      }
      return ptr;
    }

    inline bool free (void * ptr) {
      const size_t objectSize = super::getSize(ptr);
      if (objectSize > super::maxObjectSize) {
	return super::bigheap.free (ptr);
      } else {
	int objectSizeClass = ((scFunction) getSizeClass) (objectSize);
	assert (objectSizeClass >= 0);
	assert (objectSizeClass < NumBins);
	// Put the freed object into the right sizeclass heap.
	assert (getClassMaxSize(objectSizeClass) >= objectSize);
	while (((csFunction) getClassMaxSize)(objectSizeClass) > objectSize) {
	  objectSizeClass--;
	}
	assert (((csFunction) getClassMaxSize)(objectSizeClass) <= objectSize);
	if (objectSizeClass > 0) {
	  assert (objectSize >= ((csFunction) getClassMaxSize)(objectSizeClass - 1));
	}

	super::myLittleHeap[objectSizeClass].free (ptr);
	super::memoryHeld += objectSize;
	return true;
      }
    }

  };

};


#endif
