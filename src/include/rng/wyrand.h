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
    __uint128_t tmp = (__uint128_t)_state * (_state ^ 0xe7037ed1a0b428dbULL);
    return (uint64_t)((tmp >> 64) ^ tmp);
  }
};

#endif
