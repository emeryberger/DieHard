// -*- C++ -*-

/**
 * @file   randomheap.h
 * @brief  Manages random "mini-heaps" for a particular object size.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 *
 * Copyright (C) 2006-11 Emery Berger, University of Massachusetts Amherst
 */


#ifndef DH_RANDOMHEAP_H
#define DH_RANDOMHEAP_H

#include <iostream>
#include <new>
#include <atomic>

using namespace std;

#include "heaplayers.h"

using namespace HL;

#include "check.h"
#include "log2.h"
#include "mmapalloc.h"
#include "randomnumbergenerator.h"
#include "staticlog.h"
#include "util/bitmap.h"
#include "util/atomicbitmap.h"

// Cache line size for alignment to prevent false sharing
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

template <int Numerator, int Denominator>
class RandomHeapBase {
public:

  virtual ~RandomHeapBase (void) {}

  virtual void * malloc (size_t) = 0;
  virtual bool free (void *) = 0;
  virtual size_t getSize (void *) const = 0;

};


/**
 * @class RandomHeap
 * @brief Randomly allocates objects of a given size.
 * @param Numerator the numerator of the heap multiplier.
 * @param Denominator the denominator of the heap multiplier.
 * @param ObjectSize the object size managed by this heap.
 * @param BitMapType the bitmap implementation to use (BitMap or AtomicBitMap).
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 **/

template <int Numerator,
	  int Denominator,
          size_t ObjectSize,
	  size_t AllocationGrain,
	  template <int Numerator2,
		    int Denominator2,
		    unsigned long ObjectSize2,
		    unsigned long NObjects,
		    class Allocator,
		    bool DieFastOn2,
		    bool DieHarderOn2,
		    template <class> class BitMapType2> class MiniHeap,
	  bool DieFastOn,
	  bool DieHarderOn,
	  template <class> class BitMapType = BitMap>
class RandomHeap : public RandomHeapBase<Numerator, Denominator> {

  /// The most miniheaps we will use without overflowing.
  /// We will support at most 2GB per size class.
#if __cplusplus > 199711L
  enum { MAX_MINIHEAPS = (int) 31 - staticlog(AllocationGrain) };
#else
  enum { MAX_MINIHEAPS = (int) 31 - StaticLog<AllocationGrain>::VALUE };
#endif

  /// The smallest miniheap size, which may be larger than AllocationGrain.
  enum { MIN_SIZE = (AllocationGrain > (Numerator * ObjectSize) / Denominator)
	 ? AllocationGrain
	 : (Numerator * ObjectSize) / Denominator };

  /// The minimum number of objects held by a miniheap.
  enum { MIN_OBJECTS = MIN_SIZE / ObjectSize };

  /// Check values for sanity checking.
  enum { CHECK1 = 0xCAFEBABE, CHECK2 = 0xDEADBEEF };

  friend class Check<RandomHeap *>;

public:

  RandomHeap (void)
    : _check1 ((size_t) CHECK1),
      _available (0UL),
      _inUse (0UL),
      _miniHeapsInUse (0),
      _chunksInUse (0),
      _check2 ((size_t) CHECK2),
      _growLock (false)
  {
    Check<RandomHeap *> sanity (this);

    // Some basic (static) sanity checks.
    static_assert(ObjectSize > 0, "Object is too small.");
    static_assert(Numerator >= Denominator, "Multiplier must be at least one.");
    static_assert(MIN_SIZE >= ObjectSize, "Minimum size must be at least as big as one object.");
    static_assert(sizeof(MiniHeapType<MIN_OBJECTS>) ==
		  sizeof(MiniHeapType<MIN_OBJECTS*2>),
		  "There can't be any dependencies on the number of objects.");

    // Fill the buffer with miniheaps. NB: the first two have the
    // same number of objects -- this simplifies the math for
    // selecting heaps.
    ::new ((char *) _buf)
	MiniHeapType<MIN_OBJECTS> ();
    StaticForLoop<1, MAX_MINIHEAPS-1, Initializer, char *>::run ((char *) _buf);
  }


  inline void * malloc (size_t sz) {
    Check<RandomHeap *> sanity (this);

    assert (sz <= ObjectSize);

    // If we're "out" of memory, get more.
    // Use atomic loads for lock-free fast path check.
    while (Numerator * _inUse.load(std::memory_order_relaxed) >=
           _available.load(std::memory_order_relaxed) * Denominator) {
      getAnotherMiniHeap();
    }

    void * ptr = nullptr;
    while (ptr == nullptr) {
      ptr = getObject(sz);
    }

    // Check to see if the object is all zeros. If not, we had an overflow.
    // NB: Currently disabled as it is fairly expensive.
#if 0
    if (DieHarderOn)
    {
      for (unsigned int i = 0; i < ObjectSize / sizeof(long); i += sizeof(long)) {
	if (((long *) ptr)[i] != 0) {
	  fprintf (stderr, "DieHarder: Buffer overflow encountered.\n");
	  abort();
	}
      }
    }
#endif

    // Atomically bump up the amount of space in use and return.
    _inUse.fetch_add(1, std::memory_order_relaxed);

    return ptr;
  }


  inline bool free (void * ptr) {
    Check<RandomHeap *> sanity (this);

    // Starting with the largest mini-heap, try to free the object.
    // If we find it, return true.
    // Load miniHeapsInUse atomically for the iteration bound.
    unsigned int numHeaps = _miniHeapsInUse.load(std::memory_order_acquire);
    for (int i = numHeaps - 1; i >= 0; i--) {

      if (getMiniHeap(i)->free (ptr)) {
        // Found it -- atomically drop the amount of space in use.
        _inUse.fetch_sub(1, std::memory_order_relaxed);
        return true;
      }
    }

    // We did not find the object to free.
    return false;
  }

  /// @return the space available from this point in the given object
  /// @note returns 0 if this object is not managed by this heap
  inline size_t getSize (void * ptr) const {
    Check<const RandomHeap *> sanity (this);

    // We start from the largest heap (most objects) and work our way
    // down to improve performance in the common case.
    // Load miniHeapsInUse atomically.
    unsigned int v = _miniHeapsInUse.load(std::memory_order_acquire);

    for (int i = v - 1; i >= 0; i--) {
      size_t sz = getMiniHeap(i)->getSize (ptr);
      if (sz != 0) {
        // Found it.
        return sz;
      }
    }
    // Didn't find it.
    return 0;
  }

  inline void check (void) const {
    assert ((_check1 == CHECK1) && (_check2 == CHECK2));
  }
		      

private:

  // Disable copying and assignment.
  RandomHeap (const RandomHeap&);
  RandomHeap& operator= (const RandomHeap&);

  // Pick a random heap, and get an object from it.

  inline void * getObject (size_t sz) {
    void * ptr = nullptr;
    while (!ptr) {
      size_t rnd = _random.next();
      // NB: _chunksInUse acts as a bitmask that eliminates the need for
      // an (expensive) modulus operation on _available -- the
      // expression is the same as "v = rnd % _available".
      // Load atomically to get a consistent snapshot.
      unsigned int chunks = _chunksInUse.load(std::memory_order_acquire);
      size_t v = rnd & chunks;
      // Compute the index as a log of v (+1 to avoid log2(0)).
      unsigned int index = log2(v + 1);
      ptr = getMiniHeap(index)->malloc (sz);
    }
    return ptr;
  }

  // The allocator for the mini heaps.

  template <size_t Value, class SuperHeap>
  class RoundUpHeap : public SuperHeap {
  public:
    void * malloc (size_t sz) {
      CheckPowerOfTwo<Value> verifyPowTwo;
      // Round up to the next multiple of a page.
      sz = (sz + Value - 1) & ~(Value - 1);
      return SuperHeap::malloc (sz);
    }
  };

  typedef OneHeap<RoundUpHeap<CPUInfo::PageSize,
			      BumpAlloc<CPUInfo::PageSize, MmapAlloc> > > TheAllocator;

  // The type of a mini heap.
  // Passes through the BitMapType template parameter for lock-free or standard allocation.
  template <unsigned long Number> class MiniHeapType
    : public MiniHeap<Numerator, Denominator, ObjectSize, Number, TheAllocator, DieFastOn, DieHarderOn, BitMapType> {};

  // The size of a mini heap.
  enum { MINIHEAP_SIZE = sizeof(MiniHeapType<MIN_OBJECTS>) };

  // An initializer, used by StaticForLoop, to instantiate a mini heap.
  template <int Index>
  class Initializer {
  public:
    static void run (char * buf) {
      ::new (buf + MINIHEAP_SIZE * Index)
	MiniHeapType<(MIN_OBJECTS * (1UL << (Index - 1)))>;
    }
  };


  /// @return the desired mini-heap.
  inline typename MiniHeapType<MIN_OBJECTS>::SuperHeap * getMiniHeap (unsigned int index) const {
    Check<const RandomHeap *> sanity (this);
    assert (index < MAX_MINIHEAPS);
    assert (index <= _miniHeapsInUse.load(std::memory_order_relaxed));
    return (typename MiniHeapType<MIN_OBJECTS>::SuperHeap *) &_buf[index * MINIHEAP_SIZE];
  }


  // Activate another mini heap to satisfy the current memory requests.
  // Uses a spinlock to ensure only one thread grows the heap at a time.
  NO_INLINE void getAnotherMiniHeap (void) {

    Check<RandomHeap *> sanity (this);

    // Try to acquire the grow lock using a simple spinlock.
    bool expected = false;
    if (!_growLock.compare_exchange_strong(expected, true,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
      // Another thread is growing the heap. Spin-wait until done.
      while (_growLock.load(std::memory_order_acquire)) {
        // Pause to reduce contention on the cache line.
#if defined(__x86_64__) || defined(__i386__)
        __builtin_ia32_pause();
#elif defined(__aarch64__)
        asm volatile("yield");
#endif
      }
      // Re-check if we still need to grow (the other thread may have done it).
      if (Numerator * _inUse.load(std::memory_order_relaxed) <
          _available.load(std::memory_order_relaxed) * Denominator) {
        return;
      }
      // Still need to grow, try again.
      expected = false;
      if (!_growLock.compare_exchange_strong(expected, true,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed)) {
        return; // Another thread got it; let them handle it.
      }
    }

    // We hold the lock. Double-check we still need to grow.
    unsigned int currentHeaps = _miniHeapsInUse.load(std::memory_order_relaxed);
    if (Numerator * _inUse.load(std::memory_order_relaxed) <
        _available.load(std::memory_order_relaxed) * Denominator) {
      // Another thread already grew the heap. Release lock and return.
      _growLock.store(false, std::memory_order_release);
      return;
    }

    if (currentHeaps < MAX_MINIHEAPS) {
      // Calculate the new available space.
      size_t newAvailable;
      if (currentHeaps == 0) {
        newAvailable = _available.load(std::memory_order_relaxed) + MIN_OBJECTS;
      } else {
        newAvailable = _available.load(std::memory_order_relaxed) +
                       (1 << (currentHeaps - 1)) * MIN_OBJECTS;
      }

      // Activate the new mini heap.
      getMiniHeap(currentHeaps)->activate();

      // Update counters atomically with release semantics.
      // Order matters: update _available and _chunksInUse before _miniHeapsInUse
      // so readers see consistent state.
      _available.store(newAvailable, std::memory_order_relaxed);
      unsigned int newChunks = (1 << currentHeaps) - 1;
      _chunksInUse.store(newChunks, std::memory_order_relaxed);

      // Memory fence to ensure all updates are visible before incrementing heap count.
      std::atomic_thread_fence(std::memory_order_release);

      _miniHeapsInUse.store(currentHeaps + 1, std::memory_order_release);

      assert ((unsigned long) ((newChunks + 1) * MIN_OBJECTS) == newAvailable);
      // Verifies that it is indeed, a power of two minus 1.
      assert (((newChunks + 1) & newChunks) == 0);
    }

    // Release the grow lock.
    _growLock.store(false, std::memory_order_release);
    check();
  }
     
  /// Local random source.
  RandomNumberGenerator _random;

  size_t _check1;

  /// The amount of space available (atomic for lock-free access).
  /// Aligned to cache line to prevent false sharing.
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> _available;

  /// The amount of space currently in use (allocated, atomic).
  /// Aligned to cache line to prevent false sharing.
  alignas(CACHE_LINE_SIZE) std::atomic<size_t> _inUse;

  /// The number of "mini-heaps" currently in use (atomic).
  /// Aligned to cache line to prevent false sharing.
  alignas(CACHE_LINE_SIZE) std::atomic<unsigned int> _miniHeapsInUse;

  /// The number of "chunks" in use (multiples of MIN_OBJECTS) minus 1 (atomic).
  std::atomic<unsigned int> _chunksInUse;

  /// Spinlock for heap growth synchronization.
  alignas(CACHE_LINE_SIZE) std::atomic<bool> _growLock;

  /// The buffer that holds the various mini heaps.
  char _buf[sizeof(MiniHeapType<MIN_OBJECTS>) * MAX_MINIHEAPS];

  size_t _check2;

};


#endif
