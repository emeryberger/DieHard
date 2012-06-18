// -*- C++ -*-

/**
 * @file   diehard.h
 * @brief  The DieHard allocator.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#ifndef _DIEHARD_H_
#define _DIEHARD_H_

#if !defined(DIEHARD_REPLICATED)
#error "Must define DIEHARD_REPLICATED to 0 or 1."
#endif

// NB: This include must appear first.
#include "platformspecific.h"

#include <assert.h>
#include <stdlib.h>


// #define MAX_HEAP_SIZE (12*128*1024*1024)


#define MAX_HEAP_SIZE (12*32*1024*1024)
// #define MAX_HEAP_SIZE (12*8*1024*1024)

// #define MAX_HEAP_SIZE (12*2*32*1024*1024)

#include "sassert.h"
#include "ifclass.h"
#include "checkpoweroftwo.h"
#include "oneheap.h"
#include "staticlog.h"
#include "partitioninfo.h"
#include "spinlock.h"

extern size_t getRandom();

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
	  int LoadFactorThresholdDenominator = 2>
class DieHardHeap {
public:
  
  /// @param seed   the seed for the random number generator.
  DieHardHeap (int seed = 0)
    : _heap (NULL),
      _seed (seed)
  {
  }
  
  ~DieHardHeap (void) {
    // Dispose of the heap.
    if (_heap)
      MmapWrapper::unmap ((void *) _heap, MaxHeapSize);
  }
  
private:

  void init (void) {
    // Get memory for the heap and initialize it.
    _heap = (char*) MmapWrapper::map (MaxHeapSize); //  + 4 * MaxObjectSize);
#if 0
    _heap = (char *) ((size_t) (_heap + 1) & ~(2 * MaxObjectSize - 1));
    MmapWrapper::protect ((void *) (((size_t) _redzone_1 + (REDZONE_SIZE/2 - 1)) & ~(REDZONE_SIZE/2 - 1)), REDZONE_SIZE/2);
    MmapWrapper::protect ((void *) (((size_t) _redzone_2 + (REDZONE_SIZE/2 - 1)) & ~(REDZONE_SIZE/2 - 1)), REDZONE_SIZE/2);
#endif

    initializeHeap (_seed);
    //    fprintf (stderr, "initialized!\n");
  }
  
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
  NO_INLINE void initializeHeap (int seed)
  {
    if (_heap == NULL)
      return;

    // Initialize the size class table.  DO NOT CHANGE THESE VALUES!
    // But if you do, they should be powers of two: if not,
    // change the template argument to _elements to false.

    size_t sizeTable[] = { 8, 16, 32, 64, 128, 256, 512,
			   1024, 2048, 4096, 8192, MaxObjectSize };

    for (int i = 0; i < NumberOfClasses; i++) {
      _size[i] = sizeTable[i];
    }

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
			    PartitionSize,
			    seed + i + 1);
    }
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

  /// The seed for the RNG.
  int _seed;

  /// The start of the heap.
  char * _heap;

  /**
   * The information about each "partition" (size class): template arg
   * true iff elements are always a power of two.
   */
  PartitionInfo<true> _pInfo[NumberOfClasses];

  /// The sizes corresponding to each size class.
  size_t _size[NumberOfClasses];

  // An area we will page-protect to reduce the risk of buffer overflows.
  char _redzone_2[REDZONE_SIZE];

private:

  class SlowCalculatePartitionNumber {
  public:
    inline int operator()(char * p) const {
      return ((int) p) / PartitionSize;
    }
  };

  class FastCalculatePartitionNumber {
  public:
    // Only for powers-of-two.
    inline int operator()(char * p) const {
      return ((int) p) >> StaticLog<PartitionSize>::VALUE;
    }
  };

  /// Compute the size class for a given size.
  static inline int sizeClass (size_t sz) {
    return log2(sz) - StaticLog<sizeof(double)>::VALUE;
  }

  /// Given a size class, return the maximum size.
  inline size_t classToSize (int index) const {
    assert (NumberOfClasses == (sizeof(_size) / sizeof(int)));
    assert (index >= 0);
    assert (index < (sizeof(_size) / sizeof(int)));
    size_t val = 1 << (index+StaticLog<sizeof(double)>::VALUE);
    assert (_size[index] == val);
    return val;
  }

  /// Find out which partition the pointer is in.
  inline int getSizeClass (void * ptr) const {
    assert ((PartitionSize & (PartitionSize - 1)) == 0);
    IfClass<IsPowerOfTwo<PartitionSize>::VALUE,
      FastCalculatePartitionNumber, SlowCalculatePartitionNumber> calculator;
    const int partitionNumber = calculator ((char*) ((char *) ptr - _heap));
    assert (partitionNumber >= 0);
    assert (partitionNumber < NumberOfClasses);
    return partitionNumber;
  }

  /// @return one allocated object.
  inline char * allocateObject (size_t sz) {
    if (sz > MaxObjectSize) {
      return (char *) allocateDirect (sz);
    }

    if (!_heap) {
      init();
      if (!_heap) {
	return NULL;
      }
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
      char * ptr = _pInfo[sc].allocObject();
      if (ptr) {
	return ptr;
      }
      // We're out of memory in this partition.  Request an object
      // from the next size class up (twice as large as requested).
      sc++;
    }

#if 0
    char buf[255];
    sprintf (buf, "returning NULL for request of %d (class = %d)\n", sz, sizeClass(sz));
    fprintf (stderr, buf);
#endif

    return NULL;
  }

  /// Allocate an object directly using memory-mapping.
  NO_INLINE void * allocateDirect (size_t sz) {
    _directLock.lock();
    void * ptr = MmapWrapper::map (sz);
    if (ptr != NULL) {
      // Register this object.
      setLargeObjectEntry (ptr, sz);

#if DIEHARD_REPLICATED
      // Fill in the space with random values.
      unsigned long * l = (unsigned long *) ptr;
      register unsigned long lv1 = getRandom();
      register unsigned long lv2 = getRandom();
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

  /// Free a large object via munmap.
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
      values[getIndex(ptr)] = sz;
    }

    inline void clear (void * ptr) {
      values[getIndex(ptr)] = 0;
    }

  private:
    size_t values[1 << (32 - StaticLog<MaxObjectSize>::VALUE)];
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
    char * ptr = allocateObject (sz);
    if (ptr == NULL) {
      // Now we are stuck - we need to allocate the object using mmap.
      ptr = (char *) allocateDirect (sz);
    }
    return ptr;
  }

  /// Relinquish an object.
  inline bool free (void * ptr) {
    if (ptr != NULL) {
      // Free any object that's in the heap.
      if (isOnHeap(ptr)) {
	_pInfo[getSizeClass(ptr)].freeObject (ptr);
      } else {
	// Outside of the heap => this object is either invalid, or it
	// was allocated by mmap.
	freeDirect (ptr);
      }
    }
    return true; // This is for compatibility with newer versions of DieHard
  }

  /// @return true iff this object is on the heap.
  bool isOnHeap (void * ptr) const {
    return (((size_t) ptr >= (size_t) _heap) &&
	    ((size_t) ptr < (size_t) (_heap + MaxHeapSize)));
  }


  bool allocatedFromDieHard (void * ptr) {
    if (isOnHeap(ptr)) {
      return true;
    }
    _directLock.lock();
    size_t sz = getLargeObjectSize (ptr); 
    bool result = false;
    if (sz != 0) {
      result = true;
    }
    _directLock.unlock();
    return result;
  }


  /// @return the size of a given object.
  inline size_t getSize (void * ptr) {
    if (isOnHeap(ptr)) {
      int sc = getSizeClass (ptr);
      size_t sz = classToSize (sc);
      return sz - (((size_t) ptr - (size_t) _heap) % sz);
    } else {
      return getLargeObjectSize (ptr);
    }
  }

};

#endif
