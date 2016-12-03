/* -*- C++ -*- */

#ifndef __QIH_MYDLMALLOCHEAP_H__
#define __QIH_MYDLMALLOCHEAP_H__

#include <string.h>

#include "dlmalloc.h"
#include "heaplayers.h"


namespace QIH {

  class PrivateDlmallocHeap {
  public:
    inline void * malloc (size_t sz) {
      return dlmalloc(sz);
    }
    
    inline void free (void * ptr) {
      dlfree(ptr);
    }
    
    inline void free (void * ptr, size_t sz) {
      dlfree(ptr);
    }
    
    inline size_t getSize (void * ptr) {
      return dlmalloc_usable_size (ptr);
    }
    
    enum {Alignment = 4};
  };
  
  typedef PrivateDlmallocHeap InternalDataHeap;
  typedef HL::OneHeap<HL::LockedHeap<HL::SpinLockType, HL::MmapHeap> > SafeDataHeap;
  typedef HL::PerClassHeap<InternalDataHeap> InternalHeapObject;
  typedef HL::PerClassHeap<SafeDataHeap> SafeHeapObject;
  
  template<typename ObjectType> 
  class InternalSTLAlloc : public HL::STLAllocator<ObjectType, InternalDataHeap> {};

  template<typename ObjectType> 
  class SafeSTLAlloc : public HL::STLAllocator<ObjectType, SafeDataHeap> {};

}
#endif
