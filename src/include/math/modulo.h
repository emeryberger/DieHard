// -*- C++ -*-

#ifndef DH_MODULO_H
#define DH_MODULO_H

#include <stdlib.h>

#include "heaplayers.h"

// Use a fast modulus function if possible.

template <unsigned long B>
inline unsigned long modulo (unsigned long v) {
  static_assert(B > 0, "Modulus must be positive.");

  enum { Pow2 = IsPowerOfTwo<B>::VALUE };
  if (Pow2) {
    return (v & (B-1));
  } else {
    return (v % B);
  }
}

#endif
