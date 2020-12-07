// -*- C++ -*-

/**
 * @file  realrandomvalue.h
 * @brief Uses a real source of randomness to generate some random values.
 * @author Emery Berger <http://www.emeryberger.com>
 * @note   Copyright (C) 2017 by Emery Berger, University of Massachusetts Amherst.
 */


#ifndef DH_REALRANDOMVALUE_H
#define DH_REALRANDOMVALUE_H

#include <fcntl.h>
#include <unistd.h>

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
    int fd = open("/dev/urandom", O_RDONLY);
    unsigned int buf;
    read(fd, (void *)&buf, sizeof(buf));
    return buf;
  }
};

#endif
