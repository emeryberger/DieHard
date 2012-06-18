#include "platformspecific.h"

#include <algorithm>

#include "callsite.h"
#include "patchinfo.h"
#include "mmapwrapper.h"

#define PQUEUE_SIZE 1024

struct pqueue_entry {
  void * ptr;
  unsigned long long death_time;
  unsigned int index;
  
  pqueue_entry & operator=(const pqueue_entry & rhs) {
    ptr = rhs.ptr;
    death_time = rhs.death_time;
    index = rhs.index;
    return *this;
  }
};

struct pqueue_entry_comparator {
  bool operator()(const pqueue_entry & x, const pqueue_entry & y) {
    return y.death_time < x.death_time;
  }
};

template <typename SuperHeap>
class CorrectingHeap : public SuperHeap {
 public:
  CorrectingHeap () : pqueue_size(0) {
    _patch.init(); 
    pqueue = (pqueue_entry *) MmapWrapper::map(PQUEUE_SIZE * sizeof(pqueue_entry));
  }

  inline void * malloc (size_t sz) {
    int pad = _patch.getPadding(get_callsite());
    if(pad != 0) {
      //fprintf(stderr,"padding allocation at 0x%x with %d bytes\n",get_callsite(),pad);
      sz += pad;
    }

    // Free any lingering objects which are set to be executed at this time
    while(pqueue_size && pqueue[0].death_time <= global_ct) {
      SuperHeap::free(pqueue[0].ptr);
      //fprintf(stderr,"delayed free of 0x%x\n",pqueue[0].ptr);
      std::pop_heap(pqueue,pqueue+pqueue_size,pqueue_entry_comparator());
      pqueue_size--;
    }

    return SuperHeap::malloc(sz);
  }

  inline void free (void * ptr) {
    // check to see if object needs to be extended
    int td = _patch.getLifeExtension(SuperHeap::getAllocCallsite(ptr),get_callsite());
    //fprintf(stderr,"%x %x = %d\n",_allocSite[objectIndex],get_callsite(),td);
    if(!td) {
      SuperHeap::free(ptr);
    } else {
      pqueue_entry entry;
      entry.ptr = ptr;
      entry.death_time = td+SuperHeap::getObjectID(ptr);
      assert(pqueue_size < PQUEUE_SIZE);
      fprintf(stderr,"pqueue_size: %d\n",pqueue_size);
      pqueue[pqueue_size++] = entry;
      //fprintf(stderr,"0x%x\n",ptr);
      //fprintf(stderr,"entry is: 0x%x, %d\n",pqueue[pqueue_size-1].ptr,pqueue[pqueue_size-1].death_time);
      std::push_heap(pqueue,pqueue + pqueue_size, pqueue_entry_comparator());
      fprintf(stderr,"pushed onto queue: 0x%x, %d will be %d, size now: %d\n",ptr,global_ct,td+global_ct,pqueue_size);
    }
  }
  
 private:
  CorrectingHeap (const CorrectingHeap&);
  CorrectingHeap& operator= (const CorrectingHeap&);

  // The patch info
  PatchInfo _patch;

  // Global count of allocs
  unsigned long global_ct;

  /// Priority queue for handling dangling pointers
  pqueue_entry * pqueue;
  unsigned int pqueue_size;
};
