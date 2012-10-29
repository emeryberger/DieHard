
/* -*- C++ -*- */

#ifndef __QIH_FIXEDSIZEOBJHEAP_H__
#define __QIH_FIXEDSIZEOBJHEAP_H__

/**
 * @class FixedObjSizeHeap
 * @brief Allocates memory for objects in multiples of a size
 * returned by a function (e.g. page size/getpagesize)
 * @param ObjSizeFunc 
 */

#include <sys/types.h>

#include "myassert.h"

namespace QIH {

  template <class SuperHeap, int (*ObjSizeFunc)(void)>
  class FixedSizeObjHeap : public SuperHeap {
  private:
    
    size_t objSize;
    
  public:
    
    FixedSizeObjHeap() :
      objSize(ObjSizeFunc()) //save the size for future use
    {
    }
    
    inline void *malloc(size_t sz) {
      return SuperHeap::malloc(fixSize(sz));
    }
    
    inline void free(void *ptr, size_t sz) {
      SuperHeap::free(ptr, fixSize(sz));
    }
    
    inline size_t fixSize(size_t sz) {
      size_t effective_size = (objSize * (sz / objSize)) + (((sz % objSize) > 0) ? objSize : 0);
      ASSERT_LOG(effective_size >= sz, "Rounded up the size incorrectly\n!");
      return effective_size;
    }

  };

}

#endif
