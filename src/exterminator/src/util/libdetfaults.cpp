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
#include "theoneheap.h"
#include "realrandomvalue.h"
#include "callsite.h"

using namespace HL;

#pragma weak global_ct
extern unsigned long global_ct;

unsigned long seed1() {
	return atoi(getenv("LBM_SEED1"));
}

unsigned long seed2() {
	return atoi(getenv("LBM_SEED2"));
}

inline int log2(size_t sz) {
  int r = 0;
  while(sz >>= 1) {
    r++;
  }
  return r;
}

class TheCustomHeapType : public RealMallocHeap {
public:
  TheCustomHeapType (void)
    :
#if defined(linux) || defined(_WIN32)
    //rng (RealRandomValue::value(), RealRandomValue::value())
    rng (seed1(),seed2())
#else
    rng (getpid(), getpid())
#endif
    ,
    currentAllocationTime (0),
    currentOraclePosition (0)
  {
    // Pull out options from environment variables.
    // fprintf (stderr, "FAULT INJECTOR RUNNING.\n");

    set (underflowAmount, (char *) "INJECTED_UNDERFLOW_AMOUNT", 0);
    set (underflowThreshold, (char *) "INJECTED_UNDERFLOW_THRESHOLD", 32);
    set (underflowFrequency, (char *) "INJECTED_UNDERFLOW_FREQUENCY", (float) 0.0);
    set (danglingPointerFrequency, (char *) "INJECTED_DANGLING_FREQUENCY", (float) 0.0);
    set (danglingPointerDistance, (char *) "INJECTED_DANGLING_DISTANCE", 0);

    readLogFile();
  }

  // should randomly undersize objects by a certain amount,
  // and prematurely free objects. wait time, distr.
  // 'nursery'; model how progs can have errors

  // survival time ratio: with clones / without

  // visit swat paper 

  inline void * malloc (size_t sz) {
    void * ptr;
    static int injected = 0;
    static bool dangled_something = false;

    static unsigned int alloc_site, free_site;
    // (a) SIZE THRESHOLD
    // (b) FREQUENCY OF OVERFLOWS
    // (c) HOW MUCH OF AN OVERFLOW
    
    size_t actualRequestSize = sz;
    if (injected == 0 && sz > underflowThreshold) {
      if (((float) rng.next() / (float) INT_MAX) <= underflowFrequency) {
        actualRequestSize = (sz - underflowAmount);
        alloc_site = get_callsite(0);
      }
    }

    if(get_callsite(0) == alloc_site) {
      actualRequestSize = (sz-underflowAmount);
    }	

    ptr = RealMallocHeap::malloc (actualRequestSize);

    if(sz != actualRequestSize && log2(actualRequestSize-1) != log2(sz-1)) { 
      char buf[256];
      sprintf(buf,"%d @ %d: underflowing %d to %d at 0x%x\n",injected,global_ct,sz,actualRequestSize,ptr);
      fputs(buf,stderr);
    }

    if (danglingPointerFrequency > 0.0) {

      allocatedObjects[currentAllocationTime] = ptr;
      liveObjects[ptr] = 1;
    
      // Now make some dangling pointers.
      char buf[255];

#if 0
      sprintf (buf, "time now %d\n", currentAllocationTime);
      fprintf (stderr, buf);
#endif

      int i = currentOraclePosition;

      // Find any already-allocated object...
      while ((i < oracleLength)
             && (oracle[i].allocated < currentAllocationTime)) {

        // To be freed less than the dangling pointer distance from now....
        if (currentAllocationTime + danglingPointerDistance >= oracle[i].freed) {

          void * p = allocatedObjects[oracle[i].allocated];

          if ((malloc_usable_size(p) < 16384)
              && (liveObjects.find(p) != liveObjects.end())
              // THIS NEXT LINE IS VOODOO - SOMETIMES OBJECTS WITH
              // this condition will have been freed already (present
              // in the map, and sometimes not (they'll be there).
              // Seemingly nondeterministically.
              && (oracle[i].freed-currentAllocationTime > 0)) {

            // Not already freed.

#if 0
            sprintf (buf, "(%d) checking object allocated at time %d will be freed at time %d (in %d allocations)\n", currentAllocationTime, oracle[i].allocated, oracle[i].freed, oracle[i].freed - currentAllocationTime);
            fprintf (stderr, buf);
#endif

	  
            // We have a pointer we can free prematurely,
            // leading to a dangling pointer that will be correctly freed
            // in no more than 'danglingPointerDistance' allocations.
            // So flip that coin:

            unsigned int coin = rng.next();

            bool freeIt =
              (((float) coin / (float) INT_MAX)
               <= danglingPointerFrequency);

            if (freeIt) {

              // Free it.
#if 1
              sprintf (buf, "!!!!! (%d) about to free %x (allocated at time %d, supposed to be freed at time %d)\n", currentAllocationTime, p, oracle[i].allocated, oracle[i].freed);
              fprintf (stderr, buf);
#endif
	    
              dangled_something = true;

              liveObjectsType::iterator it = 
                liveObjects.find (p);
	    
              if (it != liveObjects.end()) {
                // Found it.
                // Safe to free.
                RealMallocHeap::free (p);
                liveObjects.erase (it);
              }
            }
          } else {
            //sprintf (buf, "!!!! skipped allocated at time %d because not in map\n",oracle[i].allocated);
            //fputs(buf,stderr);
          }
        }
        i++;
      }
      currentOraclePosition = i;
      currentAllocationTime++;
    }
    return ptr;
  }

 
  inline void free (void * ptr) {
    // Need to check if we're freeing an object that we've already freed,
    // so we don't do it twice.
    if (danglingPointerFrequency > 0.0) {
      liveObjectsType::iterator it = 
        liveObjects.find (ptr);
      if (it == liveObjects.end()) {
        // Nope.
      } else {
        liveObjects.erase (it);
        RealMallocHeap::free (ptr);
      }
    } else {
      RealMallocHeap::free (ptr);
    }
  }

private:

  int currentAllocationTime;

  template <class TYPE>
  void set (TYPE& var, const char * name, TYPE defaultValue) {
    char * str = getenv(name);
    if (str) {
      convert (var, str);
    } else {
      var = defaultValue;
    }
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
    sprintf (buf, "%s = %d\n", name, var);
  }

  void sprintVar (char * buf, const char * name, float var) {
    sprintf (buf, "%s = %f\n", name, var);
  }
  
  void readLogFile (void) {
    // Read in the log file, all in one go, storing it into 'oracle'.
    int logfile = open ("log", O_RDONLY);
    if (logfile < 0) {
	perror("failed to open oracle log file\n");
      return;
    }
    struct stat statbuf;
    fstat (logfile, &statbuf);
    oracle = (oracleRecord *) MmapWrapper::map (statbuf.st_size);
    oracleLength = read (logfile, oracle, statbuf.st_size) / sizeof(oracleRecord);
    close (logfile);
  }

  typedef struct {
    int freed;
    int allocated;
  } oracleRecord;

  oracleRecord * oracle;
  int oracleLength;
  int currentOraclePosition;

  class TopHeap : public SizeHeap<BumpAlloc<MmapWrapper::Size, MmapAlloc> > {};

  /// The source of memory for local containers.
  class SourceHeap : public TheOneHeap<KingsleyHeap<FreelistHeap<TopHeap>, TopHeap> > {};

  RandomNumberGenerator rng;

  typedef std::map<int, void *,
		   std::less<int>,
		   STLAllocator<pair<const int, void *>, SourceHeap> >
  allocatedObjectsType;

  typedef std::map<void *, int,
		   std::less<void *>,
		   STLAllocator<pair<void * const, int>, SourceHeap> >
  liveObjectsType;

  allocatedObjectsType allocatedObjects;
  liveObjectsType liveObjects;

  int underflowAmount;
  int underflowThreshold;
  float underflowFrequency;
  float danglingPointerFrequency;
  int danglingPointerDistance;
};


inline static TheCustomHeapType * getCustomHeap (void) {
  static char thBuf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * th = new (thBuf) TheCustomHeapType;
  return th;
}

#if defined(_WIN32)
#pragma warning(disable:4273)
#endif

#include "wrapper.cpp"
