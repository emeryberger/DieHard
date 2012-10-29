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
    currentOraclePosition (0)
    , injections(0)
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
    // (a) SIZE THRESHOLD
    // (b) FREQUENCY OF OVERFLOWS
    // (c) HOW MUCH OF AN OVERFLOW
    
    size_t actualRequestSize = sz;
    if (sz > underflowThreshold) {
      if (((float) rng.next() / (float) INT_MAX) <= underflowFrequency) {
	actualRequestSize = (sz - underflowAmount);
      }
    }
    ptr = RealMallocHeap::malloc (actualRequestSize);

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

	  if ((malloc_usable_size(p) <= 4096)
	      && (liveObjects.find(p) != liveObjects.end())) {

	    // Not already freed.

#if 0
	    sprintf (buf, "(%d) checking object allocated at time %d will be freed at time %d (in %d allocations)\n", currentAllocationTime, oracle[i].allocated, oracle[i].freed, oracle[i].freed - currentAllocationTime);
	    fprintf (stderr, buf);
#endif
	  
	    // We have a pointer we can free prematurely,
	    // leading to a dangling pointer that will be correctly freed
	    // in no more than 'danglingPointerDistance' allocations.
	    // So flip that coin:

	    bool freeIt =
	      (((float) rng.next() / (float) INT_MAX)
	       <= danglingPointerFrequency);

	    if (freeIt) {

	      // Free it.
#if 0
	      sprintf (buf, "!!!!! (%d) about to free %x (allocated at time %d, supposed to be freed at time %d)\n", currentAllocationTime, p, oracle[i].allocated, oracle[i].freed);
	      fprintf (stderr, buf);
#endif
	    
	      liveObjectsType::iterator it = 
		liveObjects.find (p);
	    
	      if (it != liveObjects.end()) {
		// Found it.
		// Safe to free.
		injections += 1;
		RealMallocHeap::free (p);
		liveObjects.erase (it);
	      }
	    }
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

  inline int getInjections() {
    return injections;
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
//     char buf[255];
//     sprintVar (buf, name, var);
//     fprintf (stderr, buf);
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
  class SourceHeap : public OneHeap<KingsleyHeap<FreelistHeap<TopHeap>, TopHeap> > {};

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
  int injections;
};


inline static TheCustomHeapType * getCustomHeap (void) {
  static char thBuf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * th = new (thBuf) TheCustomHeapType;
  return th;
}

// void __attribute__((destructor)) report(void) {
//   char buf[255];
//   sprintf(buf, "\n\nInjected %d faults\n\n", getCustomHeap()->getInjections());
//   puts(buf);
// }


#if defined(_WIN32)
#pragma warning(disable:4273)
#endif

#include "wrapper.cpp"
