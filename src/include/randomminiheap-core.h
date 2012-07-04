// -*- C++ -*-

#ifndef RANDOMMINIHEAP_CORE
#define RANDOMMINIHEAP_CORE


class RandomMiniHeapBase {
public:

  virtual void * malloc (size_t) = 0;
  virtual bool free (void *) = 0;
  virtual size_t getSize (void *) = 0;
  virtual void activate (void) = 0;
  virtual ~RandomMiniHeapBase () {}
};


/**
 * @class RandomMiniHeapCore
 * @brief Randomly allocates objects of a given size.
 * @param Numerator the heap multiplier numerator.
 * @param Denominator the heap multiplier denominator.
 * @param ObjectSize the object size managed by this heap.
 * @param NObjects the number of objects in this heap.
 * @param Allocator the source heap for allocations.
 * @param DieFastOn true iff we are trying to detect errors.
 * @param DieHarderOn true iff we are trying to prevent security vulnerabilities.
 * @sa    RandomHeap
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 **/
template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn,
	  bool DieHarderOn>
class RandomMiniHeapCore : public RandomMiniHeapBase {
public:

  /// Check values for sanity checking.
  enum { CHECK1 = 0xEEDDCCBB, CHECK2 = 0xBADA0101 };
  
  /// Rounded-up number of pages required to hold the requested number
  /// of objects (NObjects) of the requested size (ObjectSize).
  enum { NumPages =
	 (NObjects * ObjectSize + CPUInfo::PageSize - 1)
	 / CPUInfo::PageSize };

  /// The number of objects on a page (or 1 if it is a large object).
  enum { ObjectsPerPage = 
	 (ObjectSize <= CPUInfo::PageSize) ?
	 CPUInfo::PageSize / ObjectSize :
	 1 };


  /// The number of pages per object (or 1 if it is a small object).
  enum { PagesPerObject = 
	 (ObjectSize <= CPUInfo::PageSize) ?
	 1 :
	 ObjectSize / CPUInfo::PageSize };


  typedef RandomMiniHeapBase SuperHeap;

  // Note: we force the lowest bit to 1 make the _freedValue an invalid
  // pointer value for many architectures.

  RandomMiniHeapCore()
    : _check1 ((size_t) CHECK1),
      _freedValue (_random.next() | 1), 
      _isHeapActivated (false),
      _check2 ((size_t) CHECK2)
  {
    Check<RandomMiniHeapCore *> sanity (this);

    // All these need to be powers of two.

    CheckPowerOfTwo<ObjectSize>	invariant1;
    CheckPowerOfTwo<CPUInfo::PageSize> invariant2;
    CheckPowerOfTwo<ObjectsPerPage> invariant3;

    // Object size must be a multiple of page size (if they are bigger
    // than a page).
    sassert<((ObjectSize <= CPUInfo::PageSize)
	     || CPUInfo::PageSize * (ObjectSize / CPUInfo::PageSize) == ObjectSize)>
    invariant4;

    invariant1 = invariant1;
    invariant2 = invariant2;
    invariant3 = invariant3;
    invariant4 = invariant4;
  }


  /// @return an allocated object of size ObjectSize
  /// @param sz   requested object size
  /// @note May return NULL even though there is free space.
  inline void * malloc (size_t sz)
  {
    Check<RandomMiniHeapCore *> sanity (this);
    sz = sz; // to prevent warnings

    // Ensure size is reasonable.
    assert (sz <= ObjectSize);

    void * ptr = NULL;

    // Try to allocate an object from the bitmap.
    unsigned int index = modulo<NObjects> (_random.next());

    bool didMalloc = _miniHeapBitmap.tryToSet (index);

    if (!didMalloc) {
      return NULL;
    }

    // Get the address of the indexed object.
    ptr = getObject (index);

    unsigned int computedIndex = computeIndex (ptr);
    assert (index == computedIndex);
    
    if (DieFastOn) {
      // Check to see if this object was overflowed.
      if (DieFast::checkNot (ptr, ObjectSize, _freedValue)) {
	//	reportOverflowError();
      }
    }

    // Make sure the returned object is the right size.
    assert (getSize(ptr) == ObjectSize);

    return ptr;
  }


  /// @brief Relinquishes ownership of this pointer.
  /// @return true iff the object was on this heap and was freed by this call.
  inline bool free (void * ptr) {
    Check<RandomMiniHeapCore *> sanity (this);

    // Return false if the pointer is out of range.
    if (!inBounds(ptr)) {
      return false;
    }

    unsigned int index = computeIndex (ptr);
    assert (((unsigned long) index < NObjects));

    bool didFree = true;

    // Reset the appropriate bit in the bitmap.
    if (_miniHeapBitmap.reset (index)) {
      // We actually reset the bit, so this was not a double free.
    } else {
      //      reportDoubleFreeError();
      didFree = false;
    }
    return didFree;
  }


  /// Sanity check.
  void check (void) const {
    assert ((_check1 == CHECK1) &&
	    (_check2 == CHECK2));
  }

  /// @brief Activates the heap, making it ready for allocations.
  virtual void activate() = 0;

protected:

  // Disable copying and assignment.
  RandomMiniHeapCore (const RandomMiniHeapCore&);
  RandomMiniHeapCore& operator= (const RandomMiniHeapCore&);

  /// @return The address of the object corresponding to this index in the bitmap.
  virtual void * getObject (unsigned int index) = 0;

  /// @return the index corresponding to the given object.
  virtual unsigned int computeIndex (void * ptr) const = 0;

  /// @return true iff the index is valid for this heap.
  virtual bool inBounds (void * ptr) const = 0;


#if 0
  /// @brief Checks for overflows.
  void checkOverflowError (void * ptr, unsigned int index)
  {
    ptr = ptr;
    index = index;
    if (!DieHarderOn) {
      // Check predecessor.
      if (!_miniHeapBitmap.isSet (index - 1)) {
	void * p = (void *) (((ObjectStruct *) ptr) - 1);
	if (DieFast::checkNot (p, ObjectSize, _freedValue)) {
	  //	reportOverflowError();
	}
      }
      // Check successor.
      if ((index < (NObjects - 1)) &&
	  (!_miniHeapBitmap.isSet (index + 1))) {
	void * p = (void *) (((ObjectStruct *) ptr) + 1);
	if (DieFast::checkNot (p, ObjectSize, _freedValue)) {
	  //	reportOverflowError();
	}
      }
    }
  }
#endif


  /// @return true iff heap is currently active.
  inline bool isActivated (void) const {
    return _isHeapActivated;
  }

  /// A struct that is exactly the size of objects from this heap,
  /// making it convenient for pointer math.
  typedef struct {
    char obj[ObjectSize];
  } ObjectStruct;


  /// Sanity check value.
  const size_t _check1;

  /// A local random number generator.
  RandomNumberGenerator _random;
  
  /// A random value used to overwrite freed space for debugging (with DieFast).
  const size_t _freedValue;

  /// True iff the heap has been activated.
  bool _isHeapActivated;

  /// The bitmap for this heap.
  BitMap<Allocator> _miniHeapBitmap;

  /// Sanity check value.
  const size_t _check2;

};

#endif
