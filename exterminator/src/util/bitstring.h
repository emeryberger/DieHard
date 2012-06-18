// -*- C++ -*-

/**
 * @file bitstring.h
 * @brief A bit string data structure for use in memory allocators.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#ifndef _BITSTRING_H_
#define _BITSTRING_H_

#include <assert.h>

template <int N>
class BitString {
public:

  BitString (void) {
    clear();
  }

  inline void clear (void) {
    for (int i = 0; i < NUM_ULONGS; i++) {
      B[i] = ALL_ALLOCATED;
    }
    firstPos = 0;
  }

  // Bit = 1 == allocated.    
  int allocate (int length, int alignment) {
    // Start at the first entry.
    for (int bitPos = firstPos; bitPos < N; ) {
      // If the whole entry here is allocated,
      // skip to the next one.
      if (B[bitPos >> SHIFTS_PER_ULONG] == ALL_ALLOCATED) {
	bitPos = ((bitPos >> SHIFTS_PER_ULONG) + 1) << SHIFTS_PER_ULONG;
	// Adjust for alignment if needed.
	while ((bitPos % alignment) != 0) {
	  bitPos++;
	}
	continue;
      }
      // If this bit is set, skip to the next one.
      if (get(bitPos)) {
	bitPos += alignment;
	continue;
      }
      // Got something free -- let's see if we have a run of the requisite length.
      bool gotOne = true;
      for (int j = 0; j < length; j++) {
	if (get(bitPos + j)) {
	  gotOne = false;
	  bitPos += j;
	  // Adjust for alignment if needed.
	  while ((bitPos % alignment) != 0) {
	    bitPos++;
	  }
	  break;
	}
      }
      if (gotOne) {
	// We found one - claim it!
	for (int j = 0; j < length; j++) {
	  set(bitPos + j);
	}
	return bitPos;
      }
    }
    // Error - none found.
    return -1;
  }

  void free (int bitPos, int length) {
    for (int j = bitPos; j < bitPos + length; j++) {
      reset (j);
    }
  }

#if 0
  inline int first (void) const {
    for (int i = 0; i < NUM_ULONGS; i++) {
      if (B[i] > 0) {
	unsigned long index = 1U << (BITS_PER_ULONG-1);
	for (int j = 0; j < BITS_PER_ULONG; j++) {
	  if (B[i] & index) {
	    return j + i * BITS_PER_ULONG;
	  }
	  index >>= 1;
	}
      }
    }
    return -1;
  }
#endif

  inline int firstAfter (const int index) const {
#if 0
    for (int i = index; i < N; i++ ) {
      int ind = i >> SHIFTS_PER_ULONG;
      if (B[ind] & (1U << (i & (BITS_PER_ULONG - 1)))) {
	return i;
      }
    }
    return -1;
#else
    int indmask = index - (index >> SHIFTS_PER_ULONG) * BITS_PER_ULONG;
    for (int i = index >> SHIFTS_PER_ULONG; i < NUM_ULONGS; i++) {
      if (B[i]) {
	unsigned long bitval = B[i];
	bitval &= ~((1 << (indmask & (BITS_PER_ULONG - 1))) - 1);
	if (bitval) {
	  return (i * BITS_PER_ULONG + lsb(bitval));
	}
      }
      indmask = indmask - BITS_PER_ULONG;
      if (indmask < 0) {
	indmask = 0;
      }
    }
    return -1;
#endif
  }

  inline bool get (const int index) const {
    assert (index >= 0);
    assert (index < N);
    int ind = index >> SHIFTS_PER_ULONG;
    assert (ind >= 0);
    assert (ind < NUM_ULONGS);
    return (B[ind] & (1U << (index & (BITS_PER_ULONG - 1))));
  }

  inline void set (const int index) {
    assert (index >= 0);
    assert (index < N);
    int ind = index >> SHIFTS_PER_ULONG;
    assert (ind >= 0);
    assert (ind < NUM_ULONGS);
    B[ind] |= (1U << (index & (BITS_PER_ULONG - 1)));
    if (index < firstPos) {
      firstPos = index;
    }
  }

  inline void reset (const int index) {
    assert (index >= 0);
    assert (index < N);
    int ind = index >> SHIFTS_PER_ULONG;
    assert (ind >= 0);
    assert (ind < NUM_ULONGS);
    B[ind] &= ~(1U << (index & (BITS_PER_ULONG - 1)));
  }

  unsigned long operator() (int index) {
    assert (index >= 0);
    assert (index < NUM_ULONGS);
    return B[index];
  }


private:

#if 1 //  (sizeof(unsigned long),4)
  enum { BITS_PER_ULONG = 32 };
  enum { SHIFTS_PER_ULONG = 5 };
#else
  enum { BITS_PER_ULONG = 64 };
  enum { SHIFTS_PER_ULONG = 6 };
#endif

  enum { ALL_ALLOCATED = (unsigned long) -1 };
  enum { MAX_BITS = (N + BITS_PER_ULONG - 1) & ~(BITS_PER_ULONG - 1) };
  enum { NUM_ULONGS = MAX_BITS / BITS_PER_ULONG };

  unsigned long B[NUM_ULONGS];

  /// The first set position.
  int firstPos;

  inline static int lsb (unsigned long b) {
    static const int index32[32] = { 0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9 };
    const unsigned long debruijn32 = 0x077CB531UL;
    b &= (unsigned long) -((signed long) b);
    b *= debruijn32;
    b >>= 27;
    assert (b >= 0);
    assert (b < 32);
    return index32[b];
  }

};


#endif
