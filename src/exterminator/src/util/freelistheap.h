/* -*- C++ -*- */

#ifndef _FREELISTHEAP_H_
#define _FREELISTHEAP_H_

/**
 * @class FreelistHeap
 * @brief Manage freed memory on a linked list.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @warning This is for one "size class" only.
 * 
 * Note that the linked list is threaded through the freed objects,
 * meaning that such objects must be at least the size of a pointer.
 */

#include <assert.h>

#ifndef NULL
#define NULL 0
#endif

namespace HL {

template <class SuperHeap>
class FreelistHeap : public SuperHeap {
public:
  
  inline FreelistHeap (void)
    : myFreeList (0)
    {}

  inline void * malloc (const size_t sz) {
    // Check the free list first.
    void * ptr = myFreeList;
    // If it's empty, get more memory;
    // otherwise, advance the free list pointer.
    if (ptr == 0) {
      ptr = SuperHeap::malloc (sz);
    } else {
      myFreeList = myFreeList->next;
    }
    return ptr;
  }
  
  inline void free (void * ptr) {
    if (ptr == 0) {
      return;
    }
    // Thread this pointer onto the current free list.
    freeObject * thisObj = reinterpret_cast<freeObject *>(ptr);
    thisObj->next = myFreeList;
    myFreeList = thisObj;
  }

  inline void clear (void) {
    myFreeList = 0;
    SuperHeap::clear();
  }

private:

  /// The linked list pointer we embed in the freed objects.
  class freeObject {
  public:
    freeObject * next;
  };

  /// The head of the linked list of freed objects.
  freeObject * myFreeList;

};

}

#endif
