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

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

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
#if defined(_WIN32)
    unsigned int buf;
    BCryptGenRandom(NULL, (PUCHAR)&buf, sizeof(buf), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return buf;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    unsigned int buf;
    read(fd, (void *)&buf, sizeof(buf));
    close(fd);
    return buf;
#endif
  }
};

#endif
