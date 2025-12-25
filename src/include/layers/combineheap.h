// -*- C++ -*-

#ifndef DH_COMBINEHEAP_H
#define DH_COMBINEHEAP_H

#include "heaplayers.h"
#include "platformspecific.h"
// #include "gcd.h"

template <class SmallHeap,
	  class BigHeap>
class CombineHeap {
public:

  enum { Alignment = gcd<(int) SmallHeap::Alignment,
	 (int) BigHeap::Alignment>::value };

  virtual ~CombineHeap (void) {}

  ATTRIBUTE_ALWAYS_INLINE void * malloc (size_t sz) {
    void * ptr;
    // Small allocations are the common case
    if (likely(sz <= SmallHeap::MAX_SIZE)) {
      ptr = _small.malloc (sz);
    } else {
      ptr = _big.malloc (sz);
    }
    assert (((size_t) ptr % Alignment) == 0);
    return ptr;
  }

  ATTRIBUTE_ALWAYS_INLINE bool free (void * ptr) {
    // Try small heap first (common case)
    if (likely(_small.free (ptr))) {
      return true;
    }
    return _big.free (ptr);
  }

  inline size_t getSize (void * ptr) {
    size_t sz = _small.getSize (ptr);
    if (sz == 0) {
      sz = _big.getSize (ptr);
    }
    return sz;
  }

private:
  SmallHeap _small;
  BigHeap _big;
};

#endif
