// -*- C++ -*-

/**
 * @file   partitioninfo.h
 * @brief  All the information needed to manage one partition (size class).
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef _PARTITIONINFO_H_
#define _PARTITIONINFO_H_

#include <assert.h>
#include <stdlib.h>

#include "log2.h"
#include "ifclass.h"
#include "mmapalloc.h"
#include "bumpalloc.h"
#include "staticlog.h"
#include "bitmap.h"


#if !defined(DIEHARD_REPLICATED)
#error "Must define DIEHARD_REPLICATED to 0 or 1."
#endif

extern size_t getRandom (void);

/**
 * @class PartitionInfo
 * @brief All the information needed to manage one partition (size class).
 *
 */

template <bool ElementsArePowerOfTwo>
class PartitionInfo {
public:

  PartitionInfo (void)
    : _initialized (false)
  {}

  ~PartitionInfo (void) {}

  /**
   * Initialize the partition.
   *
   * @param i          the index of this partition.
   * @param sz         the size of objects in this partition.
   * @param threshold  the maximum number of allocations allowed.
   * @param start      the starting address of the partition.
   * @param len        the length of the partition, in bytes.
   * @param seed       a seed for the random number generator.
   */
  
  NO_INLINE void initialize (int i,
			     size_t sz,
			     int threshold,
			     char * start,
			     size_t len,
			     int seed)
  {
    _size = sz;
    _sizeClass = i;
    _partitionStart = start;
#if DIEHARD_MULTITHREADED != 1
    _threshold = threshold;
    _requested = 0;
#endif
    _elements = (len / sz);
    _elementsShift = log2 (_elements);
    _mask = _elements - 1;

    // Since the number of elements are known to be a power of two,
    // we eliminate an expensive mod operation (%) in
    // allocObject(). We check that they are indeed a power of two
    // here.
    assert ((_elements & (_elements - 1)) == 0);
    assert (_threshold > 0);

    // Initialize all bitmap entries to false.
    _allocated.reserve (_elements);
    _allocated.clear();
  }


  /// Fills an object with random values.
  inline void scrambleObject (char * ptr) {
#if DIEHARD_REPLICATED
    fillRandom (ptr, _size);
#else
    memset (ptr, 0, _size);
#endif
  }

  /// Fills the partition space with random values.
  NO_INLINE bool initializePartition (void) {
#if DIEHARD_REPLICATED
    fillRandom (_partitionStart, _elements * _size);
#endif
    _initialized = true;
    return true;
  }

  /// Randomly choose one of the available objects.
  inline char * allocObject (void) {
    if (!_initialized)
      initializePartition();

    // Probe the heap enough to ensure that the odds are at least
    // 2^MAX_ITERATIONS to 1 against missing an item.

    int iterations = 0;

    do {

#if DIEHARD_MULTITHREADED != 1
      if (_requested >= _threshold) {
	return NULL;
      }
#endif

      IfClass<ElementsArePowerOfTwo, FastObjectIndex, SlowObjectIndex> calculator;
      const unsigned long rng = getRandom();
      const int objectIndex = calculator (rng, _elements, _mask);
      
      assert (objectIndex >= 0);
      assert (objectIndex < _elements);
      
      bool success = _allocated.tryToSet (objectIndex);
      
#if DIEHARD_MULTITHREADED == 1
      iterations++;
#endif

      if (success) {
	// Got it.
#if DIEHARD_MULTITHREADED != 1
	_requested++;
#endif
	char * ptr = (char *) _partitionStart + (objectIndex * _size);
	scrambleObject (ptr);
	return ptr;
      }

    } while (iterations < MAX_ITERATIONS);
    return NULL;
  }

  /// Free an object.
  inline void freeObject (void * ptr) {
    const int offsetFromPartition = ((char *) ptr - _partitionStart);

    // Check to make sure this object is valid: a valid object from
    // this partition must be a multiple of the partition's object
    // size.
    if ((offsetFromPartition & ~(_size - 1)) == offsetFromPartition) {
      // We have a valid object, so we free it.

      IfClass<ElementsArePowerOfTwo,
	FastIndexFromOffset,
	SlowIndexFromOffset> calculator;
      const int objectIndex = calculator (offsetFromPartition, _size, _sizeClass);
    
      bool reallyDidReset = _allocated.reset (objectIndex);

      if (!reallyDidReset) {
	// Already free!
	return;
      }

#if DIEHARD_MULTITHREADED != 1
      _requested--;
#endif

#if 1
      // This object is empty; if it's at least a page long, tell the
      // virtual memory manager that we don't need its space anymore.
      if (_size >= 4096) {
	MmapWrapper::dontneed (_partitionStart + objectIndex * _size, _size);
      }
#endif
    }
  }

  /// Fill a range with random values.
  inline bool fillRandom (void * ptr, size_t sz) {
    unsigned long * l = (unsigned long *) ptr;
    register unsigned long lv1 = getRandom();
    register unsigned long lv2 = getRandom();
    for (int i = 0; i < sz / sizeof(double); i ++) {
      // Minimum object size is a double (two longs), so we unroll the
      // loop a bit for efficiency.
      *l++ = lv1;
      *l++ = lv2;
    }
    return true;
  }

private:

  // Prohibit copying and assignment.
  PartitionInfo (const PartitionInfo&);
  PartitionInfo& operator= (const PartitionInfo&);

  /// The allocation bitmap, grabbed in 1MB chunks from MmapAlloc.
  class BitMapSource : 
    public OneHeap<BumpAlloc<MmapAlloc, 1024 * 1024> > {};

  class SlowObjectIndex {
  public:
    inline int operator() (unsigned long rng,
			   int elts,
			   unsigned long) const {
      return rng % elts;
    }
  };

  class FastObjectIndex {
  public:
    inline int operator() (unsigned long rng,
			   int,
			   unsigned long mask) const {
      return rng & mask;
    }
  };

  class SlowIndexFromOffset {
  public:
    inline int operator()(int offset, size_t size, int) const {
      return offset / size;
    }
  };

  class FastIndexFromOffset {
  public:
    inline int operator()(int offset, size_t, int sizeClass) const {
      return offset >> (sizeClass + StaticLog<sizeof(double)>::VALUE);
    }
  };


  /// The maximum number of times we will probe in the heap for an object.
  enum { MAX_ITERATIONS = 24 };

  /// True iff this partition has been used for allocation.
  bool _initialized;

  /// Bit map of allocated objects.
  BitMap<BitMapSource> _allocated;

  /// Size of objects in this partition.
  size_t _size;
    
  /// And the corresponding size class.
  int _sizeClass;

  /// The start of the partition.
  char * _partitionStart;

#if DIEHARD_MULTITHREADED != 1
  
  /// The maximum number of objects to allocate.
  int _threshold;

  /// Number of objects requested for this partition.
  int _requested;
#endif

  /// The total number of objects in this partition.
  int _elements;

  /// The log base 2 of _elements, for shifting.
  int _elementsShift;

  /// The number of _elements, minus 1 (for bitmasking).
  unsigned long _mask;
};

#endif
