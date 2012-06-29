// -*- C++ -*-

/**
 * @file   theoneheap.h
 * @brief  Directs all calls to a single instance of a heap.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef _THEONEHEAP_H_
#define _THEONEHEAP_H_

/**
 * @class  TheOneHeap
 * @param  SuperHeap  the heap that will be the target of all allocations
 * @brief  Directs all calls to a single instance of a heap.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

template <class SuperHeap>
class TheOneHeap {
public:
  TheOneHeap (void)
  {}
  
  inline void * malloc (const size_t sz) {
    return getHeap()->malloc (sz);
  }
  inline void free (void * ptr) {
    getHeap()->free (ptr);
  }
  inline int remove (void * ptr) {
    return getHeap()->remove (ptr);
  }
  inline void clear (void) {
    getHeap()->clear();
  }
  inline size_t getSize (void * ptr) {
    return getHeap()->getSize (ptr);
  }

private:

  inline static SuperHeap * getHeap (void) {
    static SuperHeap theHeap;
    return &theHeap;
  }
};

#endif
