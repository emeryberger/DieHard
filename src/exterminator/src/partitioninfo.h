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
#include <algorithm>

#include "log2.h"
#include "mmapalloc.h"
#include "bumpalloc.h"
#include "staticlog.h"
#include "randomnumbergenerator.h"
#include "bitmap.h"
#include "callsite.h"
#include "heap_config.h"

#if !defined(DIEHARD_REPLICATED)
#error "Must define DIEHARD_REPLICATED to 0 or 1."
#endif

extern unsigned long global_ct;
extern unsigned long _death_time;

extern RandomNumberGenerator& getRNG (void);

void trap();

void dumpHeapHandler(int signum, siginfo_t * act, void * old);
void dumpHeapForFucksSake() {
  dumpHeapHandler(0,NULL,NULL);
}

#if DIEHARD_DIEFAST == 1
  void diefast_error() {
    _heap_config_flags |= DIEHARD_BAD_REPLICA;
    if(_death_time == 0) {
      if(!getenv("DH_REPLICATED"))
        abort();
      else {
        puts("killing with USR1\n");
        kill(atol(getenv("DH_REPLICATED")),SIGUSR1);
        atexit(dumpHeapForFucksSake);
      }
    }
  }
#endif

class NullMetadata {
protected:
  void elementsChanged(size_t new_sz) {}
  inline unsigned long getAllocCallsite(int idx) {}
  inline unsigned long getObjectID(int idx) {}
  inline void allocEvent(int idx) {}
  inline void freeEvent(int idx) {}
  void dumpMetadata(FILE *) { abort(); };
};

/**
 * @class PartitionInfo
 * @brief All the information needed to manage one partition (size class).
 *
 */

template <typename Metadata = NullMetadata>
class PartitionInfo : public Metadata {
public:

  PartitionInfo (void)
    : _initialized(false)
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
                             size_t len)
  {
    // No objects requested yet for this partition.

#if DIEHARD_MULTITHREADED != 1
    _threshold = threshold;
    _requested = 0;
#endif
    // Since the number of elements are known to be a power of two,
    // we eliminate an expensive mod operation (%) in
    // allocObject(). We check that they are indeed a power of two
    // here.
    _elements = len / sz;
    assert ((_elements & (_elements - 1)) == 0);
    _elementsShift = log2 (_elements);
    _mask = _elements - 1;

    assert (threshold > 0);

    // Set the local values.
    // _threshold = threshold;
    _size = sz;
    _sizeClass = i;
    _partitionStart = start;

    // Initialize all bitmap entries to false.
    _allocated.reserve (_elements);
    _allocated.clear();
    _hasData.reserve (_elements);
    _hasData.clear();

    Metadata::elementsChanged(_elements);
    
    // XXX: patcher expects all partitions to contain proper sentinel value, so we must greedily initialize them
    initializePartition();
  }

  /// Fills the partition space with random values.
  NO_INLINE bool initializePartition (void) {
    //fprintf(stderr,"initializing partition of size: %d",_size);
#if DIEHARD_REPLICATED
    fillRandom (_partitionStart, _elements * _size);
#else
    fill (_partitionStart,_elements * _size, sentinel);
#endif
    _initialized = true;
    return true;
  }

  /// Randomly choose one of the available objects.
  inline char * allocObject (void) {
    if(!_initialized)
      initializePartition();

    // Probed the heap enough to ensure that the odds are at least
    // 2^MAX_ITERATIONS to 1 against missing an item.

    int iterations = 0;

    do {

#if DIEHARD_MULTITHREADED != 1
      if (_requested >= _threshold) {
        fprintf(stderr,"failing ALLOC due to threshold limit: %d >= %d\n",_requested,_threshold);
        return NULL;
      }
#endif

      const unsigned long rng = getRNG().next();
      const int objectIndex = rng & _mask;
      // Above is optimized (power-of-two) version of:
      // const int objectIndex = (unsigned long) _rng.next() % _elements;

      // printf ("%x - locking %d (%d) - elements = %d, rng = %d\n", this, objectIndex, objectIndex % NUMBER_OF_LOCKS, _elements, rng);

      assert (objectIndex >= 0);
      assert (objectIndex < _elements);

      char * ptr = (char *) _partitionStart + (objectIndex * _size);
      
      bool success = _allocated.tryToSet (objectIndex);
      
#if DIEHARD_MULTITHREADED == 1
      iterations++;
#endif
      
      if (success) {
        // Got it.
#if DIEHARD_MULTITHREADED != 1
        _requested++;
#endif

#if DIEHARD_DIEFAST == 1
        if (!_hasData.isSet(objectIndex) && checkNot (ptr, _size, sentinel)) {
          char buf[255];
          sprintf (buf, "This slot has been corrupted\n");
          fputs (buf,stderr);
          diefast_error();
          // object is now marked allocated, it's a ``bad block''
          continue;
        }
#endif

        _hasData.tryToSet(objectIndex);
        fill(ptr,_size,0);

        Metadata::allocEvent(objectIndex);
        
        return ptr;
      }
      
    } while (iterations < MAX_ITERATIONS);
    fprintf(stderr,"failing due to iteration count\n");
    return NULL;
  }

  inline void forceFreeIndex (void * ptr, int objectIndex) {
#if DIEHARD_MULTITHREADED != 1
    _requested--;
#endif
    _allocated.reset (objectIndex);

    Metadata::freeEvent(objectIndex);

    //    fprintf(stderr,"forced free allocated %x, freed %x\n",_allocSite[objectIndex],_freeSite[objectIndex]);
#if DIEHARD_DIEFAST == 1
    if(!(_heap_config_flags & DIEHARD_BAD_REPLICA) && getRNG().next() % 2 == 0) {
      fill(ptr, _size, sentinel);
      _hasData.reset(objectIndex);
    }
    checkOverflow(ptr,objectIndex);
#endif
  }

  /// Free an object.
  inline void freeObject (void * ptr) {
    const int offsetFromPartition = ((char *) ptr - _partitionStart);

    // Check to make sure this object is valid: a valid object from
    // this partition must be a multiple of the partition's object
    // size.
    if ((offsetFromPartition & ~(_size - 1)) == offsetFromPartition) {
      // We have a valid object, so we free it.

      const int objectIndex
	= offsetFromPartition >> (_sizeClass + StaticLog<sizeof(double)>::VALUE);
      // Above is optimized (power-of-two) version of:
      //  const int objectIndex = offsetFromPartition / _size;

      // printf ("locking %d (%d)\n", objectIndex, objectIndex % NUMBER_OF_LOCKS);

      forceFreeIndex(ptr,objectIndex);
    }
  }  

  inline unsigned long getAllocCallsite(void * ptr) {
    const int offsetFromPartition = ((char *) ptr - _partitionStart);

    // Check to make sure this object is valid: a valid object from
    // this partition must be a multiple of the partition's object
    // size.
    if ((offsetFromPartition & ~(_size - 1)) == offsetFromPartition) {
      // We have a valid object, so we free it.

      const int objectIndex
        = offsetFromPartition >> (_sizeClass + StaticLog<sizeof(double)>::VALUE);
      // Above is optimized (power-of-two) version of:
      //  const int objectIndex = offsetFromPartition / _size;
      
      return Metadata::getAllocCallsite(objectIndex);
    }
    abort();
  }

  inline unsigned long getObjectID(void * ptr) {
    const int offsetFromPartition = ((char *) ptr - _partitionStart);

    // Check to make sure this object is valid: a valid object from
    // this partition must be a multiple of the partition's object
    // size.
    if ((offsetFromPartition & ~(_size - 1)) == offsetFromPartition) {
      // We have a valid object, so we free it.

      const int objectIndex
        = offsetFromPartition >> (_sizeClass + StaticLog<sizeof(double)>::VALUE);
      // Above is optimized (power-of-two) version of:
      //  const int objectIndex = offsetFromPartition / _size;
      
      return Metadata::getObjectID(objectIndex);
    }
    abort();
  }

#if DIEHARD_DIEFAST == 1
  /// @return true if the given value is not found in the buffer.
  /// @param ptr   the start of the buffer
  /// @param sz    the size of the buffer
  /// @param val   the value to check for
  static bool checkNot (void * ptr, size_t sz, size_t val) {
    size_t * l = (size_t *) ptr;
    for (int i = 0; i < sz / sizeof(size_t); i++) {
      if (l[i] != val)
        return true;
    }
    return false;
  }

  /// Checks to see if the predecessor and successor have been overflowed
  void checkOverflow(void * ptr, int index) {
    if(0 && (index > 0) && (!_hasData.isSet(index-1))) {
      void * p = (void *)((char*)ptr-_size);
      if(checkNot(p, _size, sentinel)) {
        char buf[255];
        sprintf (buf, "Underflow detected in (%x,%x) when freeing %x.\n",
                 p,
                 (void *) (((char *) p) + _size - 1),
                 ptr);
        fputs(buf,stderr);
        _allocated.tryToSet(index-1);
        diefast_error();
      }
    }
    if((index < _elements-1) && (!_hasData.isSet(index+1))) {
      void * p = (void *)((char*)ptr+_size);
      if(checkNot(p, _size, sentinel)) {
        char buf[255];
        sprintf (buf, "Overflow detected in (%x,%x) when freeing %x.\n",
                 p,
                 (void *) (((char *) p) + _size - 1),
                 ptr);
        fputs(buf,stderr);
        _allocated.tryToSet(index+1);
        diefast_error();
      }
    }
  }
#endif

  /// Fill a range with random values.
  inline bool fillRandom (void * ptr, size_t sz) {
    unsigned long * l = (unsigned long *) ptr;
    register unsigned long lv1 = getRNG().next();
    register unsigned long lv2 = getRNG().next();
    for (int i = 0; i < sz / sizeof(double); i ++) {
      // Minimum object size is a double (two longs), so we unroll the
      // loop a bit for efficiency.
      *l++ = lv1;
      *l++ = lv2;
    }
    return true;
  }

  /// @brief fills the buffer with the desired value.
  /// @param ptr   the start of the buffer
  /// @param sz    the size of the buffer
  /// @param val   the value to fill the buffer with
  /// @note  the size must be larger than, and a multiple of, sizeof(double).
  static inline void fill (void * ptr, size_t sz, size_t val) {
    assert (sz >= sizeof(double));
    assert (sz % sizeof(double) == 0);
    size_t * l = (size_t *) ptr;
    for (int i = 0; i < sz / sizeof(double); i++) {
      // Minimum object size is two longs (a double), so we unroll the
      // loop a bit for efficiency.
      *l++ = val;
      *l++ = val;
    }
  }

  void dump_heap(FILE * out) {
    // write size, number of elements, partition start address
    fwrite(&_size,sizeof(unsigned long),1,out);
    fwrite(&_elements,sizeof(unsigned long),1,out);
    fwrite(&_partitionStart,sizeof(unsigned long),1,out);
    //fprintf(stderr,"\npartition started 0x%x\n",_partitionStart);
    // dump allocation bitmap
    _allocated.dump_bitmap(out);
    _hasData.dump_bitmap(out);
    
    // dump metadata
    Metadata::dumpMetadata(out);

    // finally dump the actual memory
    fwrite(_partitionStart,_size,_elements,out);
  }

private:

  // Prohibit copying and assignment.
  PartitionInfo (const PartitionInfo&);
  PartitionInfo& operator= (const PartitionInfo&);

  /// The allocation bitmap, grabbed in 1MB chunks from MmapAlloc.
  class BitMapSource : public TheOneHeap<BumpAlloc<1024 * 1024, MmapAlloc> > {};

  /// The maximum number of times we will probe in the heap for an object.
  enum { MAX_ITERATIONS = 24 };

  /// True if this partition has been use for allocation
  bool _initialized;

  /// Bit map of allocated objects.
  BitMap<BitMapSource> _allocated;
  
  /// Bit map of whether slot contains valid data
  BitMap<BitMapSource> _hasData;

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
