// -*- C++ -*-

#ifndef DH_LOG2_H
#define DH_LOG2_H

#include <stdlib.h>

#if __cplusplus >= 202002L
#include <bit>
#endif

/// Quickly calculate the CEILING of the log (base 2) of the argument.
/// For sz=16, returns 4. For sz=17, returns 5.

#if __cplusplus >= 202002L
// C++20: Use std::bit_width which compiles to optimal instructions
static inline constexpr int log2 (size_t sz)
{
  // std::bit_width(n) returns floor(log2(n)) + 1 for n > 0
  // For ceiling: bit_width(sz - 1) works for sz > 1
  // We need ceiling, so for power-of-2: bit_width(16) - 1 = 4
  // For non-power-of-2: bit_width(17 - 1) = bit_width(16) = 5... wait that's wrong
  // Actually: bit_width(n) = floor(log2(n)) + 1
  // ceiling(log2(n)) = bit_width(n-1) for n > 1
  // But we want: log2(16) = 4, log2(17) = 5
  // bit_width(16) = 5, bit_width(15) = 4
  // So: ceiling(log2(sz)) = bit_width(sz - 1) for sz > 1, special case sz=1 -> 0
  return sz <= 1 ? 0 : static_cast<int>(std::bit_width(sz - 1));
}
#elif defined(_WIN32)
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
  int log = 0;
  unsigned int value = 1;
  while (value < v) {
    value <<= 1;
    log++;
  }
  return log;
}
#endif

#endif
