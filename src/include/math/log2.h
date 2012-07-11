// -*- C++ -*-

#ifndef DH_LOG2_H
#define DH_LOG2_H

#include <stdlib.h>

/// Quickly calculate the CEILING of the log (base 2) of the argument.
#if defined(_WIN32)
static inline int log2 (size_t sz) 
{
  int retval;
  sz = (sz << 1) - 1;
  __asm {
    bsr eax, sz
      mov retval, eax
      }
  return retval;
}
#elif defined(__GNUC__) && defined(__i386__)
static inline int log2 (size_t sz) 
{
  sz = (sz << 1) - 1;
  asm ("bsrl %0, %0" : "=r" (sz) : "0" (sz));
  return (int) sz;
}
#elif defined(__GNUC__) && defined(__x86_64__)
static inline int log2 (size_t sz) 
{
  sz = (sz << 1) - 1;
  asm ("bsrq %0, %0" : "=r" (sz) : "0" (sz));
  return (int) sz;
}
#elif defined(__GNUC__)
// Just use the intrinsic.
static inline int log2 (size_t sz) 
{
  sz = (sz << 1) - 1;
  return (sizeof(unsigned long) * 8) - __builtin_clzl(sz) - 1;
}
#else
static inline int log2 (size_t v) {
#if 0
  static const int MultiplyDeBruijnBitPosition[32] = 
    {
      0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
      31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
    };
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  // 0x218A392CD3D5DBF is a 64-bit deBruijn number.
  return MultiplyDeBruijnBitPosition[(v * 0x077CB531UL) >> 27];
#else
  int log = 0;
  unsigned int value = 1;
  while (value < v) {
    value <<= 1;
    log++;
  }
  return log;
#endif
}
#endif

#endif
