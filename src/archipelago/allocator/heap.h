/* -*- C++ -*- */
#ifndef __QIH_HEAP_H__
#define __QIH_HEAP_H__

namespace QIH {
  class Heap {
  public:
    virtual void  destroy(void *ptr)   = 0;
    virtual void *malloc(size_t sz)    = 0;
    virtual void  free(void *ptr)      = 0;
    virtual int   getAllocationClock() = 0;
  }
}

#endif
