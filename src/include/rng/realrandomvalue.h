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
#include <cstdlib>
#include <sys/stat.h>

#include <random>

#include "printf.h"

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
    // If the environment variable DIEHARD_RANDOM_SEED is set, use its
    // value as the given seed.
    const char* varName = "DIEHARD_RANDOM_SEED";
    char* varValue = getenv(varName);
    if (varValue) {
      auto seed = atoi(varValue);
      return seed;
    }
    int fd = open("/dev/urandom", O_RDONLY);
    unsigned int buf;
    read(fd, (void *)&buf, sizeof(buf));
    // If the environment variable DIEHARD_OUTPUT_SEED_FILE is set,
    // write the seed to it.
    {
      const char* varName = "DIEHARD_OUTPUT_SEED_FILE";
      char* filename = getenv(varName);
      if (filename) {
	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
	  char str[256];
	  snprintf_(str, sizeof(str), "%u\n", buf);
	  write(fd, str, strlen(str));
	  close(fd);
	}
      }
    }
    return buf;
  }
};

#endif
