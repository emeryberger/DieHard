// -*- C++ -*-

/**
 * @file  realrandomvalue.h
 * @brief Uses a real source of randomness to generate some random values.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */


#ifndef _REALRANDOMVALUE_H_
#define _REALRANDOMVALUE_H_

#include <stdio.h>

#if defined(linux)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

/**
 * @class RealRandomValue
 * @brief Uses a real source of randomness to generate some random values.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#if defined(linux)
#include <sched.h>
static void getTime (unsigned long& tlo, unsigned long& thi) {
  asm volatile ("rdtsc"
		: "=a"(tlo),
		"=d" (thi));
}
#endif

class RealRandomValue {
public:
  RealRandomValue (void)
  {}

  static unsigned int value (void) {
#if defined(_WIN32)
    // Use low-order values of the time function, with yields intermixed.
    unsigned int v;
    LARGE_INTEGER l1, l2, l3;
    QueryPerformanceCounter(&l1);
    Sleep(0);
    QueryPerformanceCounter(&l2);
    Sleep(0);
    QueryPerformanceCounter(&l3);
    v = l1.HighPart * l1.LowPart * l2.LowPart * l3.LowPart;
    return v;
#else
    // Not really random...
    unsigned int v;
    struct timeval tp;
    gettimeofday (&tp, NULL);
    v = getpid() + tp.tv_usec + tp.tv_sec;
    return v;
#endif
  }
};

#endif
