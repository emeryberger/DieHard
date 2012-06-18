// -*- C++ -*-

/**
 * @file  realrandomvalue.h
 * @brief Uses a real source of randomness to generate some random values.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */


#ifndef _REALRANDOMVALUE_H_
#define _REALRANDOMVALUE_H_

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

class RealRandomValue {
public:
  RealRandomValue (void)
  {}

  static unsigned int value (void) {
#if defined(linux)
    static int randomSourceFile = open ("/dev/urandom", O_RDONLY);
    unsigned int v;
    if (randomSourceFile > -1) {
      read (randomSourceFile, &v, sizeof(int));
    } else {
      fprintf (stderr, "Warning! Could not find /dev/urandom!\n");
      v = 0xdeadbeef; // BROKEN - NO RANDOM SOURCE.
    }
    return v;
    
/*
    // XXX: Use deterministic randomness for now
    char * seedstr = getenv("DH_SEED");
    if(!seedstr) return 0xdeadbeef;
    else return atoi(seedstr);
*/   
#elif defined(_WIN32)
    // Use low-order values of the time function, with yields intermixed.
    LARGE_INTEGER l1, l2, l3;
    QueryPerformanceCounter(&l1);
    Sleep(0);
    QueryPerformanceCounter(&l2);
    Sleep(0);
    QueryPerformanceCounter(&l3);
    unsigned int v = l1.LowPart * l2.LowPart * l3.LowPart;
    return v;
#else
    // Not really random...
    struct timeval tp;
    gettimeofday (&tp, NULL);
    return getpid() * tp.tv_usec * tp.tv_sec;
#endif
  }
};

#endif
