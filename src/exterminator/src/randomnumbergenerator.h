// -*- C++ -*-

/**
 * @file   randomnumbergenerator.h
 * @brief  A generic interface to random number generators.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef _RANDOMNUMBERGENERATOR_H_
#define _RANDOMNUMBERGENERATOR_H_

// #include "mersennetwister.h"
#include "marsaglia.h"

class RandomNumberGenerator {
public:

  RandomNumberGenerator (unsigned long seed1, unsigned long seed2) 
    : mt (seed1, seed2)
  {
  }

  inline unsigned long next (void) {
    return mt.next();
  }

private:
  //  MersenneTwister mt;
  Marsaglia mt;
};

#endif
