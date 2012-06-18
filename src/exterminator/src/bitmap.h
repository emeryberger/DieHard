// -*- C++ -*-


/**
 * @file   bitmap.h
 * @brief  A bitmap class.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */


#include <assert.h>
#include "staticlog.h"
#include "atomic.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef _BITMAP_H_
#define _BITMAP_H_

#ifndef DIEHARD_MULTITHREADED
#error "DIEHARD_MULTITHREADED must be 0 or 1."
#endif

/**
 * @class BitMap
 * @brief Manages a dynamically-sized bitmap.
 * @param Heap  the source of memory for the bitmap.
 */

template <class Heap>
class BitMap {
private:

  inline static unsigned long
  exchange (unsigned long * oldval, unsigned long newval) {
#if !(DIEHARD_MULTITHREADED) // NON-THREAD SAFE VERSION
    unsigned long v = *oldval;
    *oldval = newval;
    return v;
#else
    return Atomic::exchange (oldval, newval);
#endif
  }

public:

  BitMap (void)
    : _elements (0),
      _bitarray (NULL)
  {
  }

  /**
   * @brief Sets aside space for a certain number of elements.
   * @param  nelts  the number of elements needed.
   */
  
  void reserve (int nelts) {
    if (_elements > 0) {
      _theHeap.free (_bitarray);
    }
    void * buf = _theHeap.malloc (nelts / 8 + (nelts % 8));
    _bitarray = (unsigned long *) buf;
    _elements = nelts;
  }

  /// Clears out the bitmap array.
  void clear (void) {
    if (_bitarray != NULL) {
      memset (_bitarray, 0, _elements / 8 + (_elements % 8)); // 0 = false
    }
  }

  /// @return true iff the bit was not set (but it is now).
  inline bool tryToSet (int index) {
    const int item = index >> BITSHIFT;
    const int position = index - (item << BITSHIFT);
    const unsigned long mask = (1 << position);
    assert (item >= 0);
    assert (item < _elements / BITS);
    do {
      unsigned long oldvalue = _bitarray[item];
      unsigned long newvalue = oldvalue | mask;
      if (exchange (&_bitarray[item], newvalue) == oldvalue) {
	return !(oldvalue & mask);
      }
    } while (true);
  }

  inline bool isSet (int index) {
    const int item = index >> BITSHIFT;
    const int position = index - (item << BITSHIFT);
    assert (item >= 0);
    assert (item <= _elements / BITS);
    return (_bitarray[item] & (1UL << position));
  }

  inline void reset (int index) {
    const int item = index >> BITSHIFT;
    const int position = index - (item << BITSHIFT);
    do {
      unsigned long oldvalue = _bitarray[item];
      unsigned long newvalue = oldvalue &  ~(1UL << position);
      if (exchange (&_bitarray[item], newvalue) == oldvalue) {
	break;
      }
    } while (true);
  }

  void dump_bitmap(FILE * out) {
    fwrite(_bitarray,sizeof(unsigned long),_elements/BITS,out);
  }

private:

  /// The number of bits in an unsigned long.
  enum { BITS = sizeof(unsigned long) * 8 };

  /// The log of the number of bits in an unsigned long, for shifting.
  enum { BITSHIFT = StaticLog<BITS>::VALUE };

  /// The number of elements (bits) in the bitmap array.
  int _elements;

  /// The bit array itself.
  unsigned long * _bitarray;

  /// The source of memory.
  Heap _theHeap;
};

#endif
