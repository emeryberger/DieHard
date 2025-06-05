// -*- C++ -*-

#ifndef MWC64_H_
#define MWC64_H_

#include <stdint.h>

#include "realrandomvalue.h"

class MWC64 {

  uint64_t _x, _c, _t;

  inline uint64_t MWC() {
    _t = (_x << 58) + _c;
    _c = _x >> 6;
    _x += _t;
    _c += (_x < _t);
    return _x;
  }

public:
  
  void seed(uint64_t seed1, uint64_t seed2)
  {
    _x = seed1;
    _x <<= 32;
    _x += seed2;
    _c = 123456123456123456ULL;
  }

  
  MWC64()
  {
    auto a = RealRandomValue::value();
    auto b = RealRandomValue::value();
    seed(a, b);
  }

  MWC64 (uint64_t seed1, uint64_t seed2)
  {
    seed(seed1, seed2);
  }

  inline uint64_t next()
  {
    return MWC();
  }

};

#endif
