#include <stdlib.h>
#include <stdio.h>
#include <new>
#include <vector>
#include <map>

class std::bad_alloc;

int anyThreadCreated = 1;

#include "stlallocator.h"
#include "realmallocheap.h"
#include "randomnumbergenerator.h"
#include "freelistheap.h"
#include "kingsleyheap.h"
#include "sizeheap.h"
#include "freelistheap.h"
#include "bumpalloc.h"
#include "mmapalloc.h"
#include "oneheap.h"
#include "realrandomvalue.h"

using namespace HL;

class TheCustomHeapType : public RealMallocHeap {
public:
  TheCustomHeapType (void)
    :
#if defined(linux) || defined(_WIN32)
    rng (RealRandomValue::value(), RealRandomValue::value())
#else
    rng (getpid(), getpid())
#endif
    ,
    currentAllocationTime (0),
    injectedOverflows (0)
  {
    // Pull out options from environment variables.

    set (overflowAmount, (char *) "INJECTED_OVERFLOW_AMOUNT", 0);
    set (overflowFrequency, (char *) "INJECTED_OVERFLOW_FREQUENCY", (float) 0.0);
    set (overflowAge, (char*) "INJECTED_OVERFLOW_AGE", 20);

  }

  inline void * malloc (size_t sz) {
    void * ptr = RealMallocHeap::malloc(sz);

    currentAllocationTime++;

    float coin =  (float) rng.next() / (float) INT_MAX;

    if ((coin <= overflowFrequency)) {// && (currentAllocationTime > 10000)) 
      victims.push_back(AllocatedObject(ptr, sz, computeOverflowTime()));
    }

    checkVictims();

    return ptr;
  }

 
  inline void free (void * ptr) {
    removeFromVictims(ptr);
    RealMallocHeap::free (ptr);
    currentAllocationTime++;
  }

  inline void displaySettings() {
    display (overflowAmount, (char *) "INJECTED_OVERFLOW_AMOUNT");
    display (overflowFrequency, (char *) "INJECTED_OVERFLOW_FREQUENCY");
    display (overflowAge, (char*) "INJECTED_OVERFLOW_AGE");
  }

  inline void displayStats() {
    display (injectedOverflows, (char *) "INJECTED_OVERFLOWS");
  }

private:

  template <class TYPE>
  void set (TYPE& var, const char * name, TYPE defaultValue) {
    char * str = getenv(name);
    if (str) {
      convert (var, str);
    } else {
      var = defaultValue;
    }
  }

  template <class TYPE>
  void display (TYPE& var, const char *name) {
    char buf[255];
    sprintVar (buf, name, var);
    fprintf (stderr, buf);
  }

  void convert (int& var, char * str) {
    var = atoi(str);
  }

  void convert (float& var, char * str) {
    var = atof(str);
  }

  void sprintVar (char * buf, const char * name, int var) {
    char fmt[128];
    sprintf (fmt, "* %%s = %%-%dd *\n", 65 - strlen(name));
    sprintf (buf, fmt, name, var);
  }

  void sprintVar (char * buf, const char * name, float var) {
    char fmt[128];
    sprintf (fmt, "* %%s = %%-%df *\n", 65 - strlen(name));
    sprintf (buf, fmt, name, var);
  }
  
  class TopHeap : public SizeHeap<BumpAlloc<MmapWrapper::Size, MmapAlloc> > {};

  /// The source of memory for local containers.
  class SourceHeap : public OneHeap<KingsleyHeap<FreelistHeap<TopHeap>, TopHeap> > {};

  struct AllocatedObject {

    int overflowTime;
    void *address;
    size_t size;

    AllocatedObject(void *addr, size_t sz, int overflowTime) {
      address      = addr;
      size         = sz;
      overflowTime = overflowTime;
    }

    inline bool ready(int time) {
      return time >= overflowTime;
    }
    
    inline bool overflow(int amount) {
      volatile int *start = (int*)((char*)address + size);
      
      if (((size_t)start + amount + 4096) > (size_t)&start) //stack guard
	return false;
      
      for (int i = 0; i < amount / 4; i++) {
	*start = 0xDEADBEEF;
	start++;
      }
      return true;
    }
  };
  
  inline int computeOverflowTime() {
    return currentAllocationTime + overflowAge;// + (rng.next() % overflowAge);
  }

  inline void checkVictims() {
    unsigned int i = 0;
    while (i < victims.size()) {
      if (victims[i].ready(currentAllocationTime)) {
	if (victims[i].overflow(overflowAmount))
	  injectedOverflows++;;
	victims.erase(victims.begin() + i);
      }
      else i++;
    }
  }
  
  inline void removeFromVictims(void *addr) {
    unsigned int i = 0;
    while (i < victims.size()) {
      if (victims[i].address == addr) 
	victims.erase(victims.begin() + i);
      else i++;
    }
  }
  
  std::vector<AllocatedObject, STLAllocator<AllocatedObject, SourceHeap> > victims;

  RandomNumberGenerator rng;
  
  int overflowAmount;
  float overflowFrequency;
  int overflowAge;

  int currentAllocationTime;
  int injectedOverflows;

};


inline static TheCustomHeapType * getCustomHeap (void) {
  static char thBuf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * th = new (thBuf) TheCustomHeapType;
  return th;
}

// void __attribute__((constructor)) prologue(void) {
  
//   fputs("************************************************************************\n", stderr);  
//   fputs("*                    Overflow injector v1.01                           *\n", stderr);     
//   fputs("* Copyright (c) 2007 Vitaliy Lvin, University of Massachusetts Amherst *\n", stderr);     
//   fputs("************************************************************************\n", stderr);       

//   getCustomHeap()->displaySettings();
  
//   fputs("************************************************************************\n", stderr);       
// }


// void __attribute__((destructor)) epilogue(void) {

//   fputs("************************************************************************\n", stderr);       
//   getCustomHeap()->displayStats();
//   fputs("* Injector finished                                                    *\n", stderr);
//   fputs("************************************************************************\n", stderr);       
// }

#if defined(_WIN32)
#pragma warning(disable:4273)
#endif

#include "wrapper.cpp"
