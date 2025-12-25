// -*- C++ -*-

/**
 * @file   atomicbitmap.h
 * @brief  A lock-free bitmap class using atomic CAS operations.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005-2024 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef DH_ATOMICBITMAP_H
#define DH_ATOMICBITMAP_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <atomic>

#if __cplusplus >= 202002L
#include <bit>
#endif

#include "staticlog.h"

/**
 * @class AtomicBitMap
 * @brief Manages a dynamically-sized bitmap with lock-free operations.
 * @param Heap  the source of memory for the bitmap.
 *
 * This class provides the same interface as BitMap but uses atomic
 * compare-and-swap operations for thread-safe, lock-free access.
 */

template <class Heap>
class AtomicBitMap : private Heap {
public:

  AtomicBitMap (void)
    : _bitarray (nullptr),
      _elements (0)
  {
  }

  /**
   * @brief Sets aside space for a certain number of elements.
   * @param  nelts  the number of elements needed.
   * @note This method is NOT thread-safe and should only be called
   *       during initialization before concurrent access begins.
   */
  void reserve (unsigned long long nelts) {
    if (_bitarray) {
      Heap::free (_bitarray);
    }
    // Round up the number of elements.
    _elements = WORDBITS * ((nelts + WORDBITS - 1) / WORDBITS);
    // Allocate the right number of bytes.
    void * buf = Heap::malloc (_elements / 8);
    _bitarray = (AtomicWord *) buf;
    clear();
  }

  /// Clears out the bitmap array.
  /// @note NOT thread-safe - only call during initialization.
  void clear (void) {
    if (_bitarray != nullptr) {
      // Initialize all atomic words to zero
      size_t numWords = _elements / WORDBITS;
      for (size_t i = 0; i < numWords; i++) {
        _bitarray[i].store(0, std::memory_order_relaxed);
      }
      std::atomic_thread_fence(std::memory_order_release);
    }
  }

  /// @return true iff the bit was not set (but it is now).
  /// @note Thread-safe via atomic CAS.
  inline bool tryToSet (unsigned long long index) {
    unsigned int item, position;
    computeItemPosition (index, item, position);
    const WORD mask = getMask(position);

    WORD oldvalue = _bitarray[item].load(std::memory_order_relaxed);
    WORD newvalue;
    do {
      // If bit is already set, fail
      if (oldvalue & mask) {
        return false;
      }
      newvalue = oldvalue | mask;
    } while (!_bitarray[item].compare_exchange_weak(oldvalue, newvalue,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed));
    return true;
  }

  /// Clears the bit at the given index.
  /// @return true iff the bit was previously set.
  /// @note Thread-safe via atomic CAS.
  inline bool reset (unsigned long long index) {
    unsigned int item, position;
    computeItemPosition (index, item, position);
    const WORD mask = getMask(position);

    WORD oldvalue = _bitarray[item].load(std::memory_order_relaxed);
    WORD newvalue;
    do {
      newvalue = oldvalue & ~mask;
      // If bit is already clear, return false (was not set)
      if (oldvalue == newvalue) {
        return false;
      }
    } while (!_bitarray[item].compare_exchange_weak(oldvalue, newvalue,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed));
    return true;
  }

  /// @return true iff the bit at the given index is set.
  /// @note Thread-safe via atomic load.
  inline bool isSet (unsigned long long index) const {
    unsigned int item, position;
    computeItemPosition (index, item, position);
    WORD value = _bitarray[item].load(std::memory_order_acquire);
    return (value & getMask(position)) != 0;
  }

private:

  /// Given an index, compute its item (word) and position within the word.
  void computeItemPosition (unsigned long long index,
                            unsigned int& item,
                            unsigned int& position) const
  {
    assert (index < _elements);
    item = index >> WORDBITSHIFT;
    position = index & (WORDBITS - 1);
    assert (position == index - (item << WORDBITSHIFT));
    assert (item < _elements / WORDBYTES);
  }

  /// A synonym for the datatype corresponding to a word.
  typedef size_t WORD;

  /// Atomic version of WORD for lock-free operations.
  typedef std::atomic<WORD> AtomicWord;

  /// To find the bit in a word, do this: word & getMask(bitPosition)
  /// @return a "mask" for the given position.
  static constexpr WORD getMask (unsigned long long pos) {
    return ((WORD) 1) << pos;
  }

  /// The number of bits in a WORD.
  static constexpr size_t WORDBITS = sizeof(WORD) * 8;

  /// The number of BYTES in a WORD.
  static constexpr size_t WORDBYTES = sizeof(WORD);

  /// The log of the number of bits in a WORD, for shifting.
#if __cplusplus >= 202002L
  static constexpr size_t WORDBITSHIFT = std::bit_width(WORDBITS) - 1;
#elif __cplusplus > 199711L
  static constexpr size_t WORDBITSHIFT = staticlog(WORDBITS);
#else
  enum { WORDBITSHIFT = StaticLog<WORDBITS>::VALUE };
#endif

  /// The atomic bit array itself.
  AtomicWord * _bitarray;

  /// The number of elements in the array.
  unsigned long _elements;

};

#endif
