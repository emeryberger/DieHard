/* -*- C++ -*- */

/**
 * @file   libdiehard.cpp
 * @brief  Replaces malloc and friends with DieHard versions.
 * @author Emery Berger <http://www.emeryberger.org>
 * @note   Copyright (C) 2005-2017 by Emery Berger, University of Massachusetts Amherst.
 */

// The undef below ensures that any pthread_* calls get strong
// linkage.  Otherwise, our versions here won't replace them.  It is
// IMPERATIVE that this line appear before any files get included.

#undef __GXX_WEAK__ 

#include <stdlib.h>
#include <new>

// The heap multiplier.

enum { Numerator = 8, Denominator = 7 };

// enum { Numerator = 2, Denominator = 1 };
// enum { Numerator = 3, Denominator = 1 };
// enum { Numerator = 4, Denominator = 1 };
// enum { Numerator = 8, Denominator = 1 };
// enum { Numerator = 16, Denominator = 1 };

#include "heaplayers.h"

#include "combineheap.h"
#include "diehard.h"
#include "largeheap.h"
#include "diehardheap.h"
#include "util/bitmap.h"
#include "util/atomicbitmap.h"
#include "globalfreepool.h"
#include "objectownership.h"

/*************************  define the DieHard heap ************************/

class TheLargeHeap : public OneHeap<LargeHeap<MmapWrapper> > {
  typedef OneHeap<LargeHeap<MmapWrapper> > Super;
public:
  inline void * malloc(size_t sz) {
    auto ptr = Super::malloc(sz);
    return ptr;
  }
  inline auto free(void * ptr) {
    return Super::free(ptr);
  }
};


// Choose between original locked heap and new scalable design based on compile flag.
// DIEHARD_SCALABLE=1 enables the lock-free, per-thread heap design.

#if DIEHARD_SCALABLE

// Scalable design: Per-thread heaps with atomic bitmaps and cross-thread free pool.
// Each thread gets its own heap instance; cross-thread frees are batched and
// returned to the owner thread periodically via the OwnershipTrackingHeap.

typedef
 ThreadSpecificHeap<
  ANSIWrapper<
   OwnershipTrackingHeap<
    CombineHeap<DieHardHeap<Numerator, Denominator, 1048576,
                            (DIEHARD_DIEFAST == 1),
                            (DIEHARD_DIEHARDER == 1),
                            AtomicBitMap>,
                TheLargeHeap> > > >
TheDieHardHeap;

#else

// Original design: Single global heap protected by a lock.
// Simpler but doesn't scale well with multiple threads.

typedef
 ANSIWrapper<
  LockedHeap<PosixLockType,
	     CombineHeap<DieHardHeap<Numerator, Denominator, 1048576,
				     (DIEHARD_DIEFAST == 1),
				     (DIEHARD_DIEHARDER == 1)>,
			 TheLargeHeap> > >
TheDieHardHeap;

#endif // DIEHARD_SCALABLE

class TheCustomHeapType : public TheDieHardHeap {};

inline static TheCustomHeapType * getCustomHeap (void) {
  static char buf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * _theCustomHeap = 
    new (buf) TheCustomHeapType;
  return _theCustomHeap;
}

#if defined(_WIN32)
#pragma warning(disable:4273)
#endif

#include "printf.h"

#if !defined(_WIN32)
#include <unistd.h>

extern "C" {
  // For use by the replacement printf routines (see
  // https://github.com/emeryberger/printf)
  void _putchar(char ch) { ::write(1, (void *)&ch, 1); }
}
#endif

extern "C" {

  void * xxmalloc (size_t sz) {
    auto ptr = getCustomHeap()->malloc (sz);
    // printf_("xxmalloc %lu = %p\n", sz, ptr);
    return ptr;
  }

  void xxfree (void * ptr) {
    getCustomHeap()->free (ptr);
  }

  size_t xxmalloc_usable_size (void * ptr) {
    return getCustomHeap()->getSize (ptr);
  }

  void xxmalloc_lock() {
#if !DIEHARD_SCALABLE
    // Lock is only meaningful for the non-scalable (global locked) heap.
    // The scalable design uses per-thread heaps and doesn't need locking.
    getCustomHeap()->lock();
#endif
  }

  void xxmalloc_unlock() {
#if !DIEHARD_SCALABLE
    // Unlock is only meaningful for the non-scalable (global locked) heap.
    getCustomHeap()->unlock();
#endif
  }

  void * xxmemalign(size_t alignment, size_t sz) {
    // NOTE: this logic works because DieHard only allocates power-of-two objects, naturally aligned.
    // Changing that behavior will break this code.
    auto ptr = xxmalloc(sz < alignment ? alignment : sz);
    assert(static_cast<uintptr_t>(ptr) % alignment == 0);
    return ptr;
  }
  
  
}
