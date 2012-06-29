// -*- C++ -*-

/**
 * @file   checkpoweroftwo.h
 * @brief  Check statically if a number is a power of two.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 *
 **/


#ifndef _CHECKPOWEROFTWO_H_
#define _CHECKPOWEROFTWO_H_

#include "sassert.h"

/**
 * @class IsPowerOfTwo
 * @brief Sets VALUE to 1 iff the template argument is a power of two.
 *
 **/
template <unsigned long N>
class IsPowerOfTwo {
public:
  enum { VALUE = (!(N & (N - 1)) && N) };
};


/**
 * @class CheckPowerOfTwo
 * @brief Template meta-program: fails if number is not a power of two.
 *
 **/
template <unsigned long V>
class CheckPowerOfTwo {
  enum { Verify = sassert<IsPowerOfTwo<V>::VALUE>::VALUE };
};


#endif
