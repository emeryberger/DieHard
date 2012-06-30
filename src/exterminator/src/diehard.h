// -*- C++ -*-

/**
 * @file   diehard.h
 * @brief  The DieHard allocator.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#ifndef DH_DIEHARD_H
#define DH_DIEHARD_H

#if !defined(DIEHARD_REPLICATED)
#error "Must define DIEHARD_REPLICATED to 0 or 1."
#endif

// NB: This include must appear first.
#include "platformspecific.h"

#include <assert.h>
#include <stdlib.h>

//#define MAX_HEAP_SIZE (12*16*1024*1024)
#define MAX_HEAP_SIZE (12*4*1024*1024)
//#define MAX_HEAP_SIZE (12*32*1024*1024)

#include "sassert.h"
#include "checkpoweroftwo.h"
#include "randomnumbergenerator.h"
#include "theoneheap.h"
#include "staticlog.h"
#include "partitioninfo.h"
#include "spinlock.h"
#include "callsite.h"
#include "patchinfo.h"
#include "heap_config.h"

extern RandomNumberGenerator& getRNG();
extern unsigned long global_ct;
extern unsigned long _death_time;

#undef INSTRUMENT_RETENTION

// Successful search odds: (1 + ln(1/(1-lambda)))/lambda
// For lambda (load threshold) = Numerator/Denominator
// : expected probes to find an empty slot
//  = 1/(1-Numerator/Denominator)
//  = 1/((D-N)/D)
//  = Denominator / (Denominator - Numerator)
//
// e.g. if N/D = 1/2, expected probes = 2/(2-1) = 2.
// if N/D = 1/infinity, expected probes = inf/(inf-1) = 1.

/**
 * @class  DieHardHeap
 * @brief  The DieHard randomized allocator.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @param  MaxHeapSize  the size of the heap (a power of two).
 * @param  LoadFactorThresholdNumerator  the numerator of the threshold fraction.
 * @param  LoadFactorThresholdDenominator  the denominator of the threshold fraction.
 */

template <int MaxHeapSize,
	  int LoadFactorThresholdNumerator = 1,
	  int LoadFactorThresholdDenominator = 2,
    typename PMD = NullMetadata>
class DieHardHeap {
public:
  
  /// @param seed   the seed for the random number generator.
  DieHardHeap ()
#ifdef INSTRUMENT_RETENTION
    : retention(0), max_retention(0)
#endif
  {
    // Get memory for the heap and initialize it.
    _heap = (char*) MmapWrapper::map (MaxHeapSize);
    MmapWrapper::protect ((void *) (((size_t) _redzone_1 + (REDZONE_SIZE/2 - 1)) & ~(REDZONE_SIZE/2 - 1)), REDZONE_SIZE/2);
    MmapWrapper::protect ((void *) (((size_t) _redzone_2 + (REDZONE_SIZE/2 - 1)) & ~(REDZONE_SIZE/2 - 1)), REDZONE_SIZE/2);
    initializeHeap ();
    
    _heap_config_flags = 0;
#ifdef DIEHARD_DIEFAST
    _heap_config_flags |= DIEHARD_DIEFAST_FLAG;
#endif 

    //    fprintf (stderr, "initialized!\n");
  }
  
  ~DieHardHeap (void) {
    // Dispose of the heap.
    MmapWrapper::unmap ((void *) _heap, MaxHeapSize);
  }
  
private:

#ifdef INSTRUMENT_RETENTION
  int retention;
  int max_retention;
#endif

  
  // Prohibit copying and assignment.
  DieHardHeap (const DieHardHeap&);
  DieHardHeap& operator= (const DieHardHeap&);

  // Ensure that the load factor fraction makes sense.  It has to be
  // between zero and one, and the denominator cannot be 0.

  enum { EnsureLoadFactorLessThanOne = 
	 sassert<(LoadFactorThresholdNumerator < LoadFactorThresholdDenominator)>::VALUE };

  enum { EnsureLoadFactorMoreThanZero =
	 sassert<(LoadFactorThresholdNumerator > 0)>::VALUE };

  enum { EnsureLoadFactorDefined = 
	 sassert<(LoadFactorThresholdDenominator > 0)>::VALUE };

  /**
   * @brief Initialize the heap.
   * @param seed  the seed for the random number generator.
   */
  NO_INLINE void initializeHeap ()
  {

    // Initialize the size class table.  DO NOT CHANGE THESE VALUES!
    // But if you do, they must be powers of two.

    size_t sizeTable[] = { 8, 16, 32, 64, 128, 256, 512,
			   1024, 2048, 4096, 8192, MaxObjectSize };

    for (int i = 0; i < NumberOfClasses; i++) {
      _size[i] = sizeTable[i];
    }

    //sentinel = getRNG().next();
    sentinel = 0x9efbdbb3;

    if (_heap == NULL) {
      fprintf (stderr, "The mmap call failed to obtain enough memory.\n");
      abort();
    }

    // Compute the load factor for each partition.
    const float loadFactor = 
      1.0 * LoadFactorThresholdNumerator * PartitionSize
      / (1.0 * LoadFactorThresholdDenominator);

    // Initialize each partition (size class).
    for (int i = NumberOfClasses - 1; i >= 0; i--) {
      _pInfo[i].initialize (i,
                            _size[i],
                            (int) loadFactor / _size[i],
                            const_cast<char *>(_heap) + i * PartitionSize,
                            PartitionSize);
    }

    // get the dynamic library base address
    dl_base = (void*)0xffffffff;
    dl_iterate_phdr(dumpHeapDLCallback,&dl_base);
    //fprintf(stderr,"got dynamic library start address at 0x%x\n",dl_base);
    //fprintf(stderr,"brk function address 0x%x\n",brk);

    // HACK
    int foo;
    stack_base = &foo;

    global_ct = 0;

    char * dt_string = getenv("DH_DEATHTIME");
    if(dt_string)
      _death_time = atol(dt_string);
    else
      _death_time = 0;

    //fprintf(stderr,"got death time as %d\n",_death_time);
  }

  /// How many size classes there are.
  enum { NumberOfClasses = 12 };

  /// The allocatable space of each size class.
  enum { PartitionSize = MaxHeapSize / NumberOfClasses };

  /// The maximum size for any object managed here (i.e., not with mmap).
  enum { MaxObjectSize = 16 * 1024 };


  CheckPowerOfTwo<MaxObjectSize> ch1;
  CheckPowerOfTwo<PartitionSize> ch2;
  CheckPowerOfTwo<PartitionSize / MaxObjectSize> ch3;

  enum { REDZONE_SIZE = 16384 };

  // An area we will page-protect to reduce the risk of buffer overflows.
  char _redzone_1[REDZONE_SIZE];

  /// The start of the heap.
  char * _heap;

  /// The information about each "partition" (size class).
  PartitionInfo<PMD> _pInfo[NumberOfClasses];

  /// The sizes corresponding to each size class.
  size_t _size[NumberOfClasses];

  // An area we will page-protect to reduce the risk of buffer overflows.
  char _redzone_2[REDZONE_SIZE];

private:

  /// Computes the size class for a given size.
  static inline int sizeClass (size_t sz) {
    return log2(sz) - StaticLog<sizeof(double)>::VALUE;
  }

  /// Given a size class, returns the maximum size.
  inline size_t classToSize (int index) const {
    assert (NumberOfClasses == (sizeof(_size) / sizeof(int)));
    assert (index >= 0);
    assert (index < (sizeof(_size) / sizeof(int)));
    size_t val = 1 << (index+StaticLog<sizeof(double)>::VALUE);
    assert (_size[index] == val);
    return val;
  }

  /// Finds out which partition the pointer is in.
  inline int getSizeClass (void * ptr) const {
    assert ((PartitionSize & (PartitionSize - 1)) == 0);
    int partitionNumber = ((int) ((char *) ptr - _heap)) >> StaticLog<PartitionSize>::VALUE;
    // Above is power-of-two optimized version of:
    //   int partitionNumber = ((int) ((char *) ptr - _heap)) / PartitionSize;
    assert (partitionNumber >= 0);
    assert (partitionNumber < NumberOfClasses);
    return partitionNumber;
  }

  /// @return one allocated object.
  inline char * allocateObject (size_t sz) {
    if (sz > MaxObjectSize) {
      return (char *) allocateDirect (sz);
    }

    // malloc requires that the minimum request (even 0) be large
    // enough to hold a double.
    if (sz < sizeof(double)) {
      sz = sizeof(double);
    }
    
    // Figure out which size class we're in = which partition to allocate from.
    int sc = sizeClass (sz);

    while (sc < NumberOfClasses) {
      // Allocate only if we would not be crossing the load factor threshold.
      PartitionInfo<PMD>& pi = _pInfo[sc];
      char * ptr = pi.allocObject();
      if (ptr) {
        return ptr;
      }
      // We're out of memory in this partition.  Request an object
      // from the next size class up (twice as large as requested).
      sc++;
    }

#if 1
    char buf[255];
    sprintf (buf, "returning NULL for request of %d (class = %d)\n", sz, sizeClass(sz));
    fprintf (stderr, buf);
#endif

    return NULL;
  }

  /// Allocates an object directly using memory-mapping.
  NO_INLINE void * allocateDirect (size_t sz) {
    // round up to word size
    sz = (sz + sizeof(void *) - 1) & ~(sizeof(void*) - 1);
    _directLock.lock();
    void * ptr = MmapWrapper::map (sz);
    if (ptr != NULL) {
      // Register this object.
      setLargeObjectEntry (ptr, sz);

#if DIEHARD_REPLICATED
      // Fill in the space with random values.
      unsigned long * l = (unsigned long *) ptr;
      register unsigned long lv1 = getRNG().next();
      register unsigned long lv2 = getRNG().next();
      for (int i = 0; i < sz / sizeof(double); i ++) {
        // Minimum object size is two longs (a double), so we unroll the
        // loop a bit for efficiency.
        *l++ = lv1;
        *l++ = lv2;
      }
#else
      memset (ptr, 0, sz);
#endif
    }
    _directLock.unlock();
    return ptr;
  }

  /// Frees a large object via munmap.
  void freeDirect (void * ptr) {
    _directLock.lock();
    size_t sz = getLargeObjectSize (ptr); 
    if (sz != 0) {
      MmapWrapper::unmap (ptr, sz);
      clearLargeObjectEntry (ptr);
    }
    _directLock.unlock();
  }

  class LargeObjectManager {
  public:

    inline int getIndex (void * ptr) const {
      return (size_t) ptr >> StaticLog<MaxObjectSize>::VALUE;
    }

    inline size_t get (void * ptr) const {
      return values[getIndex(ptr)];
    }

    inline void set (void * ptr, size_t sz) {
      if(values[getIndex(ptr)] == 0) ct++;
      values[getIndex(ptr)] = sz;
      addrs [getIndex(ptr)] = ptr;
    }

    inline void clear (void * ptr) {
      if(values[getIndex(ptr)] != 0) ct--;
      values[getIndex(ptr)] = 0;
      addrs [getIndex(ptr)] = 0;
    }

    void dump_los(FILE * out) {
      //fprintf(stderr,"dumping %d los entries", ct);
      fwrite(&ct,sizeof(unsigned long),1,out);
      int tmp_ct = 0;
      for(unsigned long int i = 0; 
          i < (1 << (32 - StaticLog<MaxObjectSize>::VALUE)); 
          i++) {
        if(values[i] != 0) {
          tmp_ct++;
          void * addr = addrs[i];
          fwrite(&addr,sizeof(void *),1,out);
          fwrite(&values[i],sizeof(unsigned long),1,out);
          fwrite((void *)addr, 1, values[i], out);
        }
      }
      assert(tmp_ct == ct);
    }

  private:
    size_t values[1 << (32 - StaticLog<MaxObjectSize>::VALUE)];
    void * addrs [1 << (32 - StaticLog<MaxObjectSize>::VALUE)];
    unsigned long int ct;
  };

  SpinLockType _directLock;
  LargeObjectManager _lgManager;
  
  size_t getLargeObjectSize (void * ptr) {
    return _lgManager.get (ptr);
  }

  void clearLargeObjectEntry (void * ptr) {
    _lgManager.clear (ptr);
  }

  void setLargeObjectEntry (void * ptr, size_t sz) {
    _lgManager.set (ptr, sz);
  }

public:

  /// @return an object with randomized contents.
  inline void * malloc (size_t sz) {
    global_ct++;
#ifdef EXTERMINATOR
    // XXX FIXME this stupid bug where we alloc then immediately free something which finds the overflow: need to wait long enough to initialize the object
    if(global_ct == _death_time) {
      fputs("Suicide is painless...\n",stderr);
      raise(SIGUSR2);
    }
#endif

#ifdef INSTRUMENT_RETENTION
    retention += pad;
    if(retention > max_retention) {
      max_retention = retention;
      fprintf(stderr,"retention now: %d\n", retention);
    }
#endif

    char * ptr = allocateObject (sz);
    if (ptr == NULL) {
      // Now we are stuck - we need to allocate the object using mmap.
      ptr = (char *) allocateDirect (sz);
    }
    return ptr;
  }

  /// Relinquishes an object.
  inline void free (void * ptr) {
    if (ptr != NULL) {
#ifdef INSTRUMENT_RETENTION
      int pad = _patch.getPadding(getAllocCallsite(ptr));
      retention -= pad;
#endif

      // Free any object that's in the heap.
      if (isOnHeap(ptr)) {
        _pInfo[getSizeClass(ptr)].freeObject (ptr);
      } else {
        // Outside of the heap => this object is either invalid, or it
        // was allocated by mmap.
        freeDirect (ptr);
      }
    }
  }

  /// @return true iff this object is on the heap.
  bool isOnHeap (void * ptr) const {
    return (((size_t) ptr >= (size_t) _heap) &&
	    ((size_t) ptr < (size_t) (_heap + MaxHeapSize)));
  }

  /// @return the size of a given object.
  inline size_t getSize (void * ptr) {
    if (isOnHeap(ptr)) {
      int sc = getSizeClass (ptr);
      return classToSize (sc);
    } else {
      return getLargeObjectSize (ptr);
    }
  }

  void dump_heap(bool corrupt) {
    if(corrupt) {
      _heap_config_flags |= DIEHARD_BAD_REPLICA;
    }
    
    char *buf = getenv("DH_COREFILE");
    if(!buf) {
      fputs("no core file specified!\n",stderr);
      return;
    }
    FILE * out = fopen(buf,"w");

    // XXX FIXME SEE NOTE IN malloc()
    //global_ct--;

    char obuf[256];
    sprintf(obuf,"global allocs: %d\n",global_ct);
    fputs(obuf,stderr);

    unsigned long int num_tmp = NumberOfClasses;

    // Dynamic load offset address
    fwrite(&global_ct,sizeof(unsigned long),1,out);
    fwrite(&sentinel,sizeof(unsigned long),1,out);
    fwrite(&dl_base,sizeof(unsigned long int),1,out);
    fwrite(&stack_base,sizeof(unsigned long), 1,out);
    fwrite(&_heap_config_flags,sizeof(unsigned long int),1,out);
    fwrite(&num_tmp,sizeof(unsigned long int),1,out);
    for(int sc = 0; sc < NumberOfClasses; sc++) {
      _pInfo[sc].dump_heap(out);
    }
    _lgManager.dump_los(out);
    fclose(out);
    //fprintf(stderr,"done!");
  }

protected:
  inline unsigned long getAllocCallsite(void * ptr) {
    if(!isOnHeap(ptr)) return 0;
    return _pInfo[getSizeClass(ptr)].getAllocCallsite (ptr);
  }

  inline unsigned long getObjectID(void * ptr) {
    if(!isOnHeap(ptr)) return 0;
    return _pInfo[getSizeClass(ptr)].getObjectID (ptr);
  }
};

#endif
