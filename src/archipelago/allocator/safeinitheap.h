/* -*- C++ -*- */

#ifndef __QIH_SAFEINIT_HEAP_H__
#define __QIH_SAFEINIT_HEAP_H__

#include <set>

#include "internalheap.h"

namespace QIH {

  typedef void (*init_func)(void);

  template<class SuperHeap, class BackupHeap>
  class SafeInitHeap : public SuperHeap {

  private:
    BackupHeap myheap;

    typedef std::set<void*, less<void*>, HL::STLAllocator<void*,SafeDataHeap> > SetType; 

    SetType oldAddresses;

    int initialized;
    enum { Not_Initialized = 0, In_Progress = 2, Completed = 3 };

  public:

    SafeInitHeap(void) : initialized(Not_Initialized) {};

    inline void *malloc(size_t sz) {
      if (initialized == Completed) {
	return SuperHeap::malloc(sz);
      } else {
	void *ptr = myheap.malloc(sz);

	oldAddresses.insert(ptr);
	
	return ptr;
      }
    }

    inline void free(void *ptr) {
      SetType::iterator pos = oldAddresses.find(ptr);

      if ((initialized == Completed) && 
	  (pos == oldAddresses.end())) {
	//object has been allocated through regular allocator
	SuperHeap::free(ptr);

      } else {
	//object has been allocated through temporary allocator
	oldAddresses.erase(pos);
	myheap.free(ptr);
      }
    }

    inline void free(void *ptr, size_t sz) {
      free(ptr);
    }

    inline size_t getSize(void *ptr) {
      SetType::iterator pos = oldAddresses.find(ptr);
      if ((initialized == Completed) &&
	  (pos == oldAddresses.end())) {
	return SuperHeap::getSize(ptr);
      } else {
	return myheap.getSize(ptr);
      }
    }

    inline void initialize(init_func func) {
      if (initialized == Not_Initialized) {
	initialized = In_Progress;
	func();
	initialized = Completed;	  
      }
    }
  };

}

#endif
