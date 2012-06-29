// -*- C++ -*-

/**
 * @file   staticlog.h
 * @brief  Statically returns the log (base 2) of a value.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef _STATICLOG_H_
#define _STATICLOG_H_

#include "staticif.h"

template <int N>
class StaticLog;

template <>
class StaticLog<1> {
public:
  enum { VALUE = 0 };
};

template <>
class StaticLog<2> {
public:
  enum { VALUE = 1 };
};

template <int N>
class StaticLog {
public:
  enum { VALUE = StaticIf<(N > 1), StaticLog<N/2>::VALUE + 1, 0>::VALUE };
};

#endif
