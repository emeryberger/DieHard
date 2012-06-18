/* -*- C++ -*- */

#ifndef _SIZEHEAP_H_
#define _SIZEHEAP_H_

/**
 * @file sizeheap.h
 * @brief Contains UseSizeHeap and SizeHeap.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#include <assert.h>

#include "addheap.h"

#if 1

/**
 * @class UseSizeHeap
 * @brief Adds a getSize method to access the size of an allocated object.
 * @see SizeHeap
 */

namespace HL {

template <class Super>
class UseSizeHeap : public Super {
public:
  
  inline UseSizeHeap (void) {}
  
  inline static size_t getSize (const void * ptr) {
    return ((freeObject *) ptr - 1)->sz;
  }

protected:
  union freeObject {
    size_t sz;
    double _dummy; // for alignment.
  };
};

/**
 * @class SizeHeap
 * @brief Allocates extra room for the size of an object.
 */

template <class SuperHeap>
class SizeHeap : public UseSizeHeap<SuperHeap> {
  typedef typename UseSizeHeap<SuperHeap>::freeObject freeObject;
public:
  inline SizeHeap (void) {}
  inline void * malloc (const size_t sz) {
    // Add room for a size field.
    freeObject * ptr = (freeObject *)
      SuperHeap::malloc (sz + sizeof(freeObject));
    // Store the requested size.
    ptr->sz = sz;
    return (void *) (ptr + 1);
  }
  inline bool free (void * ptr) {
    return SuperHeap::free ((freeObject *) ptr - 1);
  }
};

};

#else

template <class Super>
class SizeHeap : public Super {
public:
  
  inline void * malloc (size_t sz) {
    // Add room for a size field.
	  assert (sizeof(size_t) <= sizeof(double));
    void * ptr = Super::malloc (sz + sizeof(double));
    // Store the requested size.
    *((size_t *) ptr) = sz;
    return (void *) ((double *) ptr + 1);
  }
  
  inline void free (void * ptr) {
    void * origPtr = (void *) ((double *) ptr - 1);
	Super::free (origPtr);
  }

  inline static size_t getSize (void * ptr) {
    return *((size_t *) ((double *) ptr - 1));
  }
};



template <class Super>
class UseSizeHeap : public Super {
public:
  
  inline static size_t getSize (void * ptr) {
    return *((size_t *) ((double *) ptr - 1));
  }
};

#endif

#endif
