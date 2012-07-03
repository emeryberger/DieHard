/* -*- C++ -*- */

/**
 * @file   libdiehard.cpp
 * @brief  Replaces malloc and friends with DieHard versions.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005-2011 by Emery Berger, University of Massachusetts Amherst.
 */

// The undef below ensures that any pthread_* calls get strong
// linkage.  Otherwise, our versions here won't replace them.  It is
// IMPERATIVE that this line appear before any files get included.

#undef __GXX_WEAK__ 

#include <stdlib.h>
#include <new>

class std::bad_alloc;

// The heap multiplier.

enum { Numerator = 4, Denominator = 3 };
// enum { Numerator = 2, Denominator = 1 };
// enum { Numerator = 3, Denominator = 1 };
// enum { Numerator = 4, Denominator = 1 };
// enum { Numerator = 8, Denominator = 1 };
// enum { Numerator = 16, Denominator = 1 };

#include "heaplayers.h"
// #include "ansiwrapper.h"
// #include "mmapwrapper.h"
// #include "lockedheap.h"
// #include "posixlock.h"
// #include "oneheap.h"

#include "combineheap.h"
#include "diehard.h"
#include "randomnumbergenerator.h"
#include "realrandomvalue.h"
#include "largeheap.h"
#include "diehardheap.h"
// #include "reentrantheap.h"
#include "version.h"


/*************************  define the DieHard heap ************************/

class TheLargeHeap : public OneHeap<LargeHeap<MmapWrapper> > {};

#include "debugheap.h"

#if 1 // multi-threaded

// typedef ANSIWrapper<LockedHeap<PosixLockType, DebugHeap<CombineHeap<DieHardHeap<Numerator, Denominator, 65536, (DIEHARD_DIEFAST == 1)>,
//						   TheLargeHeap> > > >

typedef ANSIWrapper<LockedHeap<PosixLockType, CombineHeap<DieHardHeap<Numerator, Denominator, 65536, (DIEHARD_DIEFAST == 1), (DIEHARD_DIEHARDER == 1)>,
					 TheLargeHeap> > >
  TheDieHardHeap;

#else

typedef ANSIWrapper<CombineHeap<DieHardHeap<Numerator, Denominator, 65536, (DIEHARD_DIEFAST == 1), (DIEHARD_DIEHARDER == 1)>,
					 TheLargeHeap> >
  TheDieHardHeap;

#endif


class TheCustomHeapType : public TheDieHardHeap {};

inline static TheCustomHeapType * getCustomHeap (void) {
  static char buf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * _theCustomHeap = 
    new (buf) TheCustomHeapType;
  return _theCustomHeap;
}

#if defined(_WIN32)
#pragma warning(disable:4273)
#endif

extern "C" {

  void * xxmalloc (size_t sz) {
    return getCustomHeap()->malloc (sz);
  }

  void xxfree (void * ptr) {
    getCustomHeap()->free (ptr);
  }

  size_t xxmalloc_usable_size (void * ptr) {
    return getCustomHeap()->getSize (ptr);
  }

  void xxmalloc_lock() {
    getCustomHeap()->lock();
  }

  void xxmalloc_unlock() {
    getCustomHeap()->unlock();
  }

}


#if 0 // Disabled.

/*************************  error-reporting functions ************************/

#if !defined(_WIN32)

extern "C" void reportOverflowError (void) {
#if 0
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (an overflow).\n");
  }
#endif
}

extern "C" void reportDoubleFreeError (void) {
#if 0
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (a double free),\nbut DieHard has prevented it from doing any harm.\n");
  }
#endif
}

extern "C" void reportInvalidFreeError (void) {
#if 0
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (an invalid free),\nbut DieHard has prevented it from doing any harm.\n");
  }
#endif
}

// #include "heapshield.cpp"

#endif

/*************************  thread-intercept functions ************************/

#if !defined(_WIN32)

#include <pthread.h>
#include <dlfcn.h>

extern "C" {

  typedef void * (*threadFunctionType) (void *);

  typedef  
  int (*pthread_create_function) (pthread_t *thread,
				  const pthread_attr_t *attr,
				  threadFunctionType start_routine,
				  void *arg);

  int pthread_create (pthread_t *thread,
		      const pthread_attr_t *attr,
		      void * (*start_routine) (void *),
		      void * arg)
#ifndef __SVR4
    throw()
#endif
  {
    static pthread_create_function real_pthread_create = NULL;
    if (real_pthread_create == NULL) {
      real_pthread_create = (pthread_create_function) dlsym (RTLD_NEXT, "pthread_create");
    }
    return (*real_pthread_create)(thread, attr, start_routine, arg);
  }
}
#endif


#if defined(_WIN32)
#ifndef CUSTOM_DLLNAME
#define CUSTOM_DLLNAME DllMain
#endif

#ifndef DIEHARD_PRE_ACTION
#define DIEHARD_PRE_ACTION
#endif

#ifndef DIEHARD_POST_ACTION
#define DIEHARD_POST_ACTION
#endif
#endif // _WIN32

/*************************  default DLL initializer for Windows ************************/

#if defined(_WIN32) && !defined(NO_WIN32_DLL)

extern "C" {

BOOL WINAPI CUSTOM_DLLNAME (HANDLE hinstDLL, DWORD fdwReason, LPVOID lpreserved)
{
  char buf[255];

  switch (fdwReason) {

  case DLL_PROCESS_ATTACH:
    sprintf (buf, "Using the DieHarder application-hardening system (Windows DLL, version %d.%d).\nCopyright (C) 2011 Emery Berger, University of Massachusetts Amherst.\n", VERSION_MAJOR_NUMBER, VERSION_MINOR_NUMBER);
    fprintf (stderr, buf);
    DIEHARD_PRE_ACTION;
    break;

  case DLL_PROCESS_DETACH:
    DIEHARD_POST_ACTION;
    break;

  case DLL_THREAD_ATTACH:
    break;

  case DLL_THREAD_DETACH:
    break;

  default:
    return TRUE;
  }
  
  return TRUE;
}

}

#endif // WIN32

#endif
