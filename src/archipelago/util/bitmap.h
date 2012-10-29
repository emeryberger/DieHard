// -*- C++ -*-


/**
 * @file   bitmap.h
 * @brief  A bitmap class.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */


#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "staticlog.h"

#ifndef _BITMAP_H_
#define _BITMAP_H_

/**
 * @class BitMap
 * @brief Manages a dynamically-sized bitmap.
 * @param Heap  the source of memory for the bitmap.
 */

template <class Heap>
class BitMap : private Heap {
private:

  typedef size_t WORD;

public:

#if 0
  BitMap (void)
    : _bitmap (NULL),
      _elements (0)
  {}

  void reserve (int nelts) {
    if (_bitmap != NULL) {
      Heap::free (_bitmap);
    }
    _bitmap = new (Heap::malloc (nelts * sizeof(bool))) bool[nelts];
    _elements = nelts;
  }

  void clear (void) {
    for (int i = 0; i < _elements; i++) {
      _bitmap[i] = false;
    }
  }

  inline bool tryToSet (int index) {
    if (_bitmap[index]) {
      return false;
    } else {
      _bitmap[index] = true;
      return true;
    }
  }

  inline bool reset (int index) {
    if (!_bitmap[index]) {
      return false;
    } else {
      _bitmap[index] = false;
      return true;
    }
  }

  inline bool isSet (int index) const {
    return _bitmap[index];
  }

private:

  bool * _bitmap;
  int _elements;
  
#else

  BitMap (void)
    : _bitarray (NULL),
      _elements (0)
  {
  }

  /**
   * @brief Sets aside space for a certain number of elements.
   * @param  nelts  the number of elements needed.
   */
  
  void reserve (int nelts) {
    if (_bitarray) {
      Heap::free (_bitarray);
    }
    // Round up the number of elements.
    _elements = WORDBITS * ((nelts + WORDBITS - 1) / WORDBITS);
    // Allocate the right number of chars.
    int nchars = _elements / WORDBYTES;
    void * buf = Heap::malloc (nchars);
    _bitarray = (WORD *) buf;
    clear();
  }

  /// Clears out the bitmap array.
  void clear (void) {
    if (_bitarray != NULL) {
      int nchars = _elements / WORDBYTES;
      memset (_bitarray, 0, nchars); // 0 = false
    }
  }

  /// @return true iff the bit was not set (but it is now).
  inline bool tryToSet (int index) {
    assert (index >= 0);
    assert (index < _elements);
    const int item = index >> WORDBITSHIFT;
    const int position = index & (WORDBITS - 1);
    assert (item >= 0);
    assert (item < _elements / WORDBYTES);
    unsigned long oldvalue;
    const WORD mask = getMask(position);
    oldvalue = _bitarray[item];
    _bitarray[item] |= mask;
    return !(oldvalue & mask);
  }

  inline bool reset (int index) {
    assert (index >= 0);
    assert (index < _elements);
    const int item = index >> WORDBITSHIFT;
    const int position = index & (WORDBITS - 1);
    assert (item >= 0);
    assert (item < _elements / WORDBYTES);
    unsigned long oldvalue;
    oldvalue = _bitarray[item];
    WORD newvalue = oldvalue &  ~(getMask(position));
    _bitarray[item] = newvalue;
    return (oldvalue != newvalue);
  }

  inline bool isSet (int index) const {
    const int item = index >> WORDBITSHIFT;
    const int position = index - (item << WORDBITSHIFT);
    assert (item >= 0);
    assert (item < _elements / WORDBYTES);
    return (_bitarray[item] & (getMask(position)));
  }

private:

  inline static WORD getMask (int pos) {
    return 1UL << pos;
  }

  /// The number of bits in a WORD.
  enum { WORDBITS = sizeof(WORD) * 8 };

  /// The number of BYTES in a WORD.
  enum { WORDBYTES = sizeof(WORD) };

  /// The log of the number of bits in a WORD, for shifting.
  enum { WORDBITSHIFT = StaticLog<WORDBITS>::VALUE };

  /// The bit array itself.
  WORD * _bitarray;
  
  /// The number of elements in the array.
  int _elements;

#endif

};

#endif
