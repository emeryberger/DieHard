// -*- C++ -*-

/**
 * @file  realrandomvalue.h
 * @brief Uses a real source of randomness to generate some random values.
 * @author Emery Berger <http://www.emeryberger.com>
 * @note   Copyright (C) 2017 by Emery Berger, University of Massachusetts Amherst.
 */


#ifndef DH_REALRANDOMVALUE_H
#define DH_REALRANDOMVALUE_H

#include <random>

/**
 * @class RealRandomValue
 * @brief Uses a real source of randomness to generate some random values.
 * @author Emery Berger <http://www.emeryberger.com>
 */

class RealRandomValue {
public:
  RealRandomValue()
  {}

  static unsigned int value() {
    // Now just a thin wrapper over the C++11 random device.
    std::random_device rnd;
    return rnd();
  }
};

#endif
