// -*- C++ -*-

#ifndef DH_COMBINEHEAP_H
#define DH_COMBINEHEAP_H

#include "heaplayers.h"
// #include "gcd.h"

template <class SmallHeap,
	  class BigHeap>
class CombineHeap {
public:

  enum { Alignment = gcd<(int) SmallHeap::Alignment,
	 (int) BigHeap::Alignment>::value };

  virtual ~CombineHeap (void) {}

  inline void * malloc (size_t sz) {
    void * ptr;
    if (sz > SmallHeap::MAX_SIZE) {
      ptr = _big.malloc (sz);
    } else {
      ptr = _small.malloc (sz);
    }
    assert (((size_t) ptr % Alignment) == 0);
    return ptr;
  }

  inline bool free (void * ptr) {
    if (_small.free (ptr)) {
      return true;
    } else {
      return _big.free (ptr);
    }
  }

  inline size_t getSize (void * ptr) {
    size_t sz = _small.getSize (ptr);
    if (sz == 0) {
      sz = _big.getSize (ptr);
    }
    return sz;
  }

  void lock() {
    _small.lock();
  }

  void unlock() {
    _small.unlock();
  }
  
private:
  SmallHeap _small;
  BigHeap _big;
};

#endif
