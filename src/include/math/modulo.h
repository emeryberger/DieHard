// -*- C++ -*-

#ifndef DH_MODULO_H
#define DH_MODULO_H

#include <stdlib.h>

#include "heaplayers.h"

// Use a fast modulus function if possible.

template <int B>
inline size_t modulo (size_t v) {
  sassert<(B > 0)> modulus_must_be_positive;
  modulus_must_be_positive = modulus_must_be_positive;

  enum { Pow2 = IsPowerOfTwo<B>::VALUE };
  if (Pow2) {
    return (v & (B-1));
  } else {
    return (v % B);
  }
}

#endif
