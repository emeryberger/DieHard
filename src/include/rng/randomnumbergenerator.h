// -*- C++ -*-

/**
 * @file   randomnumbergenerator.h
 * @brief  A generic interface to random number generators.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef DH_RANDOMNUMBERGENERATOR_H
#define DH_RANDOMNUMBERGENERATOR_H


#include "mwc.h"
#include "realrandomvalue.h"


class RandomNumberGenerator {
public:

  RandomNumberGenerator() // (unsigned int seed1, unsigned int seed2) 
    : mt (RealRandomValue::value(), RealRandomValue::value()) {
    // : mt (362436069, 521288629)  {
  }

  inline unsigned int next (void) {
    return mt.next();
  }

private:

   MWC mt;

};

#endif
