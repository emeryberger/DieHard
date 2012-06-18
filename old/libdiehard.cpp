/* -*- C++ -*- */

/**
 * @file   libdiehard.cpp
 * @brief  Replaces malloc and friends with DieHard versions.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005-2006 by Emery Berger, University of Massachusetts Amherst.
 */

// The undef below ensures that any pthread_* calls get strong
// linkage.  Otherwise, our versions here won't replace them.  It is
// IMPERATIVE that this line appear before any files get included.

#undef __GXX_WEAK__ 

#include <stdlib.h>
#include <new>

class std::bad_alloc;

volatile int anyThreadCreated = 0;

// The heap multiplier.

enum { Numerator = 8, Denominator = 7 };
// enum { Numerator = 4, Denominator = 3 };
// enum { Numerator = 2, Denominator = 1 };
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
// #include "reentrantheap.h"
#include "version.h"



/*************************  stubs for non-deterministic functions  ************************/

/*
  For DieHard's replicated mode, we have to intercept time and date
  functions (times, getrusage).  For now, we simply use bogus time
  functions with known values.

 */

#if DIEHARD_REPLICATED

#if defined(linux)

#include <dlfcn.h>
#include <sys/times.h>
#include <sys/time.h>
#include <sys/resource.h>

extern "C" {

  clock_t times (struct tms *buf) {
    // We'll make our own bogus, fixed time.
    static int count = 1;
    if (buf != NULL) {
      buf->tms_utime = count;
      buf->tms_stime = count;
      buf->tms_cutime = count;
      buf->tms_cstime = count;
      count++;
    }
    return 0;
  }
  
  int getrusage (int who, struct rusage *usage) {
    static int count = 1;
    if (usage != NULL) {
      usage->ru_utime.tv_sec = count;
      usage->ru_utime.tv_usec = count;
      usage->ru_stime.tv_sec = count;
      usage->ru_stime.tv_usec = count;
      usage->ru_maxrss = 0;
      usage->ru_ixrss = 0;
      usage->ru_idrss = 0;
      usage->ru_isrss = 0;
      usage->ru_minflt = 0;
      usage->ru_majflt = 0;
      usage->ru_nswap = 0;
      usage->ru_inblock = 0;
      usage->ru_oublock = 0;
      usage->ru_msgsnd = 0;
      usage->ru_msgrcv = 0;
      usage->ru_nsignals = 0;
      usage->ru_nvcsw = 0;
      usage->ru_nivcsw = 0;
      count++;
    }
    return 0;
  }

  clock_t clock (void) {
    return (clock_t) -1;
  }
}

#endif // linux

#endif // replicated

/*************************  define the DieHard heap ************************/

class TheLargeHeap : public OneHeap<LargeHeap<MmapWrapper> > {};

//typedef ANSIWrapper<LockHeap<ReentrantHeap<CombineHeap<DieHardHeap<Numerator, Denominator, 65536, (DIEHARD_DIEFAST == 1)>,
//				 TheLargeHeap> > > >

#if 1

typedef ANSIWrapper<LockHeap<CombineHeap<DieHardHeap<Numerator, Denominator, 65536, (DIEHARD_DIEFAST == 1)>,
					 TheLargeHeap> > >
  TheDieHardHeap;

#else

typedef ANSIWrapper<CombineHeap<DieHardHeap<Numerator, Denominator, 65536, (DIEHARD_DIEFAST == 1)>,
					 TheLargeHeap> >
  TheDieHardHeap;

#endif


class TheCustomHeapType : public TheDieHardHeap {};

#if 0 // defined(_WIN32)
static TheCustomHeapType _theCustomHeap;

static TheCustomHeapType * getCustomHeap (void) {
  return &_theCustomHeap;
}
#else

inline static TheCustomHeapType * getCustomHeap (void) {
  static char buf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * _theCustomHeap = 
    new (buf) TheCustomHeapType;
  return _theCustomHeap;
}

#endif

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

//// DISABLED! #include "wrapper.cpp"

/*************************  error-reporting functions ************************/

#if !defined(_WIN32)

extern "C" void reportOverflowError (void) {
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (an overflow).\n");
  }
}

extern "C" void reportDoubleFreeError (void) {
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (a double free),\nbut DieHard has prevented it from doing any harm.\n");
  }
}
extern "C" void reportInvalidFreeError (void) {
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (an invalid free),\nbut DieHard has prevented it from doing any harm.\n");
  }
}

// #include "heapshield.cpp"

#endif

/*************************  thread-intercept functions ************************/

#if !defined(_WIN32)

// Intercept pthread_create calls to set "anyThreadCreated".
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
    anyThreadCreated = 1;

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
    sprintf (buf, "Using the DieHard application-hardening system (Windows DLL, version %d.%d).\nCopyright (C) 2006 Emery Berger, University of Massachusetts Amherst.\n", VERSION_MAJOR_NUMBER, VERSION_MINOR_NUMBER);
    fprintf (stderr, buf);
    DIEHARD_PRE_ACTION;
    break;

  case DLL_PROCESS_DETACH:
    DIEHARD_POST_ACTION;
    break;

  case DLL_THREAD_ATTACH:
    anyThreadCreated = 1;
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

