// -*- C++ -*-

/**
 * @file   wyrand.h
 * @brief  Wyrand: A fast, high-quality PRNG.
 * @note   Based on wyhash by Wang Yi.
 *
 * Wyrand is significantly faster than MWC64 while maintaining excellent
 * statistical quality. It passes BigCrush and PractRand tests.
 */

#ifndef WYRAND_H_
#define WYRAND_H_

#include <stdint.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

class Wyrand {
  uint64_t _state;

public:
  Wyrand() : _state(0) {}

  Wyrand(uint64_t seed1, uint64_t seed2) {
    _state = seed1 ^ seed2;
    if (_state == 0) _state = 0x853c49e6748fea9bULL;
  }

  inline uint64_t next() {
    _state += 0xa0761d6478bd642fULL;
#if defined(_MSC_VER)
    // MSVC doesn't have __uint128_t, use platform-specific intrinsics
    uint64_t b = _state ^ 0xe7037ed1a0b428dbULL;
#if defined(_M_ARM64)
    // ARM64: use __umulh for high 64 bits
    uint64_t lo = _state * b;
    uint64_t hi = __umulh(_state, b);
#else
    // x64: use _umul128
    uint64_t hi, lo;
    lo = _umul128(_state, b, &hi);
#endif
    return hi ^ lo;
#else
    __uint128_t tmp = (__uint128_t)_state * (_state ^ 0xe7037ed1a0b428dbULL);
    return (uint64_t)((tmp >> 64) ^ tmp);
#endif
  }
};

#endif
