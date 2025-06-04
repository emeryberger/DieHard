// -*- C++ -*-

/**
 * @file   randomnumbergenerator.h
 * @brief  A generic interface to random number generators.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#ifndef DH_RANDOMNUMBERGENERATOR_H
#define DH_RANDOMNUMBERGENERATOR_H

#include "mwc64.h"
#include "seedmanager.h"

#include "printf.h"

class RandomNumberGenerator {
public:
  RandomNumberGenerator() { 
    {
      static auto count = 0;
      printf_("Count = %d\n", count);
      count++;
    }
    
    // Get a unique seed pair from the SeedManager
    unsigned int seed1, seed2;
    SeedManager::getSeedPair(seed1, seed2);
    
    // Initialize the generator with these seeds
    mt.seed(seed1, seed2);
  }
  
  inline unsigned int next() {
    return mt.next();
  }

private:
  MWC64 mt;
};

#endif