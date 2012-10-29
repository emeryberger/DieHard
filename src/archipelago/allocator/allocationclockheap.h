// -*- C++ -*-

#ifndef __QIH_ALLOCATION_CLOCK_HEAP_H__
#define __QIH_ALLOCATION_CLOCK_HEAP_H__

#include <sys/types.h>

#include "log.h"

namespace QIH {

  template<class SuperHeap>
  class AllocationClockHeap : public SuperHeap {
  private:
    unsigned long clock;
    
  public:
    AllocationClockHeap() : clock(0) {
   }
    
    inline void *malloc(size_t sz) {
      void *ptr = SuperHeap::malloc(sz); 
      
      clock++;
      
      logWrite(DEBUG, "%d: allocated %p to %p, size %d \n", 
	       clock, ptr, (size_t)ptr + sz, sz);
      
      return ptr;
    }
    
    inline void free(void *ptr) {
      size_t size = SuperHeap::getSize(ptr);
      
      if (size ==0) {
	logWrite(WARNING, "Attempting to delete an object %p that wasn't allocated!\n", 
		 ptr);
	return;
      }
      
      SuperHeap::free(ptr);
      
      clock++;
      
      logWrite(DEBUG, "%d: freed %p to %p \n", clock, ptr, (size_t)ptr + size);

    }
    
    inline void free(void *ptr, size_t sz) {
      free(ptr);
    }
    
    inline unsigned long getAllocationClock() {
      return clock;
    }
  };
  
}

#endif
