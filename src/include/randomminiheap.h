// -*- C++ -*-

/**
 * @file   randomminiheap.h
 * @brief  Randomly allocates a particular object size in a range of memory.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 *
 * Copyright (C) 2006-12 Emery Berger, University of Massachusetts Amherst
 */

#ifndef DH_RANDOMMINIHEAP_H
#define DH_RANDOMMINIHEAP_H

#include <assert.h>

#include "heaplayers.h"
// #include "cpuinfo.h"
// #include "sassert.h"

#include "bitmap.h"
#include "check.h"
#include "checkedarray.h"
#include "checkpoweroftwo.h"
#include "diefast.h"
#include "madvisewrapper.h"
#include "modulo.h"
// #include "mypagetable.h"
#include "randomnumbergenerator.h"
#include "staticlog.h"

#include "heaplayers.h"
// #include "mmapwrapper.h"
// #include "singleton.h"
// #include "cpuinfo.h"
// #include "mmapheap.h"

#include "madvisewrapper.h"
#include "dynamichashtable.h"
#include "pagetableentry.h"

#include "randommmap.h"


#include "randomminiheap-core.h"
#include "randomminiheap-diehard.h"
#include "randomminiheap-dieharder.h"


template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn,
	  bool DieHarderOn>
class RandomMiniHeap;


template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn>
class RandomMiniHeap<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, true> :
  public RandomMiniHeapDieHarder<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn> {};

template <int Numerator,
	  int Denominator,
	  unsigned long ObjectSize,
	  unsigned long NObjects,
	  class Allocator,
	  bool DieFastOn>
class RandomMiniHeap<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn, false> :
  public RandomMiniHeapDieHard<Numerator, Denominator, ObjectSize, NObjects, Allocator, DieFastOn> {};


#endif

