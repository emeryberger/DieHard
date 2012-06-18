// -*- C++ -*-

/**
 * @file   macwrapper.cpp
 * @brief  Replaces malloc family on Macs with custom versions.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2010 by Emery Berger, University of Massachusetts Amherst.
 */

// g++ -g -W -Wall -arch i386 -arch x86_64 -DDIEHARD_REPLICATED=0 -DDIEHARD_MULTITHREADED=1 -DNDEBUG  -I. -D_REENTRANT=1 -compatibility_version 1 -current_version 1 -dynamiclib macdiehard.cpp -o libdiehard.dylib


//////////

// volatile int anyThreadCreated = 0;

// The heap multiplier.

// enum { Numerator = 4, Denominator = 3 };
enum { Numerator = 2, Denominator = 1 };
// enum { Numerator = 3, Denominator = 1 };
// enum { Numerator = 4, Denominator = 1 };
// enum { Numerator = 8, Denominator = 1 };
// enum { Numerator = 16, Denominator = 1 };

#include "diehard.h"
#include "ansiwrapper.h"
#include "combineheap.h"
#include "randomnumbergenerator.h"
#include "realrandomvalue.h"
#include "largeheap.h"
#include "mmapwrapper.h"
#include "lockheap.h"
#include "oneheap.h"
#include "diehardheap.h"
#include "version.h"

class TheLargeHeap : public OneHeap<LargeHeap<MmapWrapper> > {};

typedef ANSIWrapper<LockHeap<CombineHeap<DieHardHeap<Numerator, Denominator, 65536, (DIEHARD_DIEFAST == 1)>,
					 TheLargeHeap> > >
  TheDieHardHeap;


class TheCustomHeapType : public TheDieHardHeap {};

inline static TheCustomHeapType * getCustomHeap (void) {
  static char buf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * _theCustomHeap = 
    new (buf) TheCustomHeapType;
  return _theCustomHeap;
}

extern "C" {
  void * xxmalloc (size_t sz) {
    return getCustomHeap()->malloc (sz);
  }

  void xxfree (void * ptr) {
    getCustomHeap()->free (ptr);
  }

  size_t xxmalloc_usable_size (void * ptr) {
    return getCustomHeap()->getSize(ptr);
  }
}

#define DIEHARD_REPORT_ERRORS 0

//////////

extern "C" {

  void reportOverflowError (void) {
#if DIEHARD_REPORT_ERRORS
    static bool alertedAlready = false;
    if (!alertedAlready) {
      //      alertedAlready = true;
      fprintf (stderr, "DieHarder alert: This application has performed an illegal action (an overflow).\n");
    }
#endif
  }
  
  void reportDoubleFreeError (void) {
#if DIEHARD_REPORT_ERRORS
    static bool alertedAlready = false;
    if (!alertedAlready) {
      //      alertedAlready = true;
      fprintf (stderr, "DieHarder alert: This application has performed an illegal action (a double free),\nbut DieHarder has prevented it from doing any harm.\n");
    }
#endif
  }

  void reportInvalidFreeError (void) {
#if DIEHARD_REPORT_ERRORS
    static bool alertedAlready = false;
    if (!alertedAlready) {
      //      alertedAlready = true;
      fprintf (stderr, "DieHarder alert: This application has performed an illegal action (an invalid free),\nbut DieHarder has prevented it from doing any harm.\n");
    }
#endif
  }
}

#include "macwrapper.cpp"

