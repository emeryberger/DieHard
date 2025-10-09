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
#include <atomic>
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

#include "printf.h"


/*************************  define the DieHard heap ************************/

class TheLargeHeap : public OneHeap<LargeHeap<MmapWrapper> > { };

#include "rng/mwc64.h"

template <int K, class Super>
class ShardHeap {
public:

  enum { Alignment = Super::Alignment };
  enum { MAX_SIZE = Super::MAX_SIZE };

  // Disable locking since we are already thread-safe.
  
  void lock() {
    return;
  }
  
  void unlock() {
    return;
  }

  inline int getIndex() {
    // Pick a subheap randomly; we use the same one for the lifetime
    // of the thread.
    static __thread auto index = -1;
    if (index == -1) {
      index = computeIndex();
    }
    return index;
  }
  
  void * malloc(size_t sz) {
    return _heap[getIndex()].malloc(sz);
  }
  
  auto free(void * ptr) {
    return _heap[getIndex()].free(ptr);
  }
  
  size_t getSize(void * ptr) {
    return _heap[getIndex()].getSize(ptr);
  }
  
private:

  int computeIndex() {
    // power of two choices; pick the least contended heap
    static __thread MWC64 rng;
    auto r1 = rng.next() % K;
    auto r2 = rng.next() % K;
    int index;
    if (_occupied[r1]++ < _occupied[r2]++) {
      _occupied[r2]--;
      index = r1;
    } else {
      _occupied[r1]--;
      index = r2;
    }
    return index;
  }
  
  LockedHeap<PosixLockType, Super> _heap[K];
  std::atomic<int> _occupied[K];
};

#define USE_SHARDED_DIEHARD 1

#if !USE_SHARDED_DIEHARD

typedef
 ANSIWrapper<
  LockedHeap<PosixLockType,
	     CombineHeap<DieHardHeap<Numerator, Denominator, 1048576, // 65536,
				     (DIEHARD_DIEFAST == 1),
				     (DIEHARD_DIEHARDER == 1)>,
			 TheLargeHeap> > >
TheDieHardHeap;

#else // USE_SHARDED_DIEHARD

typedef
 ANSIWrapper<
  CombineHeap<ShardHeap<32, DieHardHeap<Numerator, Denominator, 1048576, // 65536,
				       (DIEHARD_DIEFAST == 1),
				       (DIEHARD_DIEHARDER == 1)>>,
	      TheLargeHeap> >
TheDieHardHeap;

#endif


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
#if 0
    static long mallocs = 0L;
    mallocs++;
    if (mallocs % 10000 == 0) {
      printf_("xxmalloc %lu = %p\n", sz, ptr);
    }
#endif
    return ptr;
  }

  void xxfree (void * ptr) {
    getCustomHeap()->free (ptr);
  }

  size_t xxmalloc_usable_size (void * ptr) {
    return getCustomHeap()->getSize (ptr);
  }

  void xxmalloc_lock() {
    getCustomHeap()->lock();
  }

  void xxmalloc_unlock() {
    getCustomHeap()->unlock();
  }

  void * xxmemalign(size_t alignment, size_t sz) {
    // NOTE: this logic works because DieHard only allocates power-of-two objects, naturally aligned.
    // Changing that behavior will break this code.
    auto ptr = xxmalloc(sz < alignment ? alignment : sz);
    assert(static_cast<uintptr_t>(ptr) % alignment == 0);
    return ptr;
  }
  
  
}
