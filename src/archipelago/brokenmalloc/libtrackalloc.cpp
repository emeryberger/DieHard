#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <new>
#include <vector>
#include <set>
#include <map>

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
#include "mmapwrapper.h"

class std::bad_alloc;

using namespace HL;

int anyThreadCreated = 1;

#include "realmallocheap.h"

class TheCustomHeapType : public RealMallocHeap {
public:
  TheCustomHeapType (void)
    : currentAllocationTime (0),
      logfile (-1)
  {
  }

  typedef struct {
    int freed;
    int allocated;
  } oracleRecord;

  inline void * malloc (size_t sz) {
    static int initialized = initializeMe();
    void * ptr = RealMallocHeap::malloc (sz);
    theMap[ptr] = currentAllocationTime;
    currentAllocationTime++;
    return ptr;
  }

  inline void free (void * ptr) {
    if (ptr != 0) {
      static int initialized = initializeMe();
      //char buf[255];
      //    sprintf (buf, "%d %d\n", currentAllocationTime, theMap[ptr]);
      //write (logfile, buf, strlen(buf));
      int allocated = theMap[ptr];
      oracleRecord r;
      r.freed = currentAllocationTime;
      r.allocated = allocated;
      write (logfile, &r, sizeof(oracleRecord));
      //    fprintf (stderr, "free\n");
      theMap.erase (theMap.find (ptr));
    }
  }

  void closeUpShop (void) {
    fsync (logfile);
    close (logfile);
  }

private:

  int initializeMe (void) {
    if (logfile == -1) {
      logfile = open ("log", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    }
    return logfile;
  }

  int logfile;
  int currentAllocationTime;

  class TopHeap : public SizeHeap<BumpAlloc<MmapWrapper::Size, MmapAlloc> > {};

  /// The source of memory for local containers.
  class SourceHeap : public OneHeap<KingsleyHeap<FreelistHeap<TopHeap>, TopHeap> > {};

  typedef std::map<void *, int, std::less<void *>,
		   STLAllocator<std::pair<void * const,int>, SourceHeap> > mapType;

  mapType theMap;

};




inline static TheCustomHeapType * getCustomHeap (void) {
  static char thBuf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * th = new (thBuf) TheCustomHeapType;
  return th;
}

void exit (int s) {
  getCustomHeap()->closeUpShop();
  _exit (s);
}

#if defined(_WIN32)
#pragma warning(disable:4273)
#endif

#include "wrapper.cpp"
