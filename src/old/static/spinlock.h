/* -*- C++ -*- */

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

#if defined(unix)
#include <sched.h>
#endif

#if defined(__SVR4)
#include <thread.h>
#endif

#if defined(__sgi)
#include <mutex.h>
#endif

#include "cpuinfo.h"

#if defined(_MSC_VER)

#if !defined(NO_INLINE)
#pragma inline_depth(255)
#define NO_INLINE __declspec(noinline)
#define INLINE __forceinline
#define inline __forceinline
#endif

#else

#endif

#include "atomic.h"

#if defined(_WIN32)
#define _WIN32_WINNT 0x0500

// NOTE: Below is the new "pause" instruction, which is inocuous for
// previous architectures, but crucial for Intel chips with
// hyperthreading.  See
// http://www.usenix.org/events/wiess02/tech/full_papers/nakajima/nakajima.pdf
// for discussion.

#define _MM_PAUSE {__asm{_emit 0xf3};__asm {_emit 0x90}}
#include <windows.h>
#else
#define _MM_PAUSE
#endif

extern volatile int anyThreadCreated;

class SpinLockType {
public:
  
  SpinLockType (void)
    : mutex (UNLOCKED)
  {}
  
  ~SpinLockType (void)
  {}

  inline bool lock (void) {
    // A yielding lock (with an initial spin).
    if (anyThreadCreated) {
      if (Atomic::exchange (const_cast<unsigned long *>(&mutex), LOCKED)
	  != UNLOCKED) {
	contendedLock();
      }
    } else {
      mutex = LOCKED;
    }
    return true;
  }

  inline bool trylock (void) {
    if (anyThreadCreated) {
      if (mutex == LOCKED) {
	return false;
      } else {
	if (Atomic::exchange (const_cast<unsigned long *>(&mutex), LOCKED)
	    == LOCKED) {
	  return false;
	} else {
	  return true;
	}
      }
    } else {
      mutex = LOCKED;
      return true;
    }
  }

 
  inline void unlock (void) {
    if (anyThreadCreated) {
#if 1
#if defined(_WIN32)
      __asm {}
#elif !defined(sparc)
      asm volatile ("" : : : "memory");
#endif 
#endif
      // SFENCE here?
      // MyInterlockedExchange (const_cast<unsigned long *>(&mutex), UNLOCKED);
    }
    mutex = UNLOCKED;
  }

private:

  NO_INLINE
  void contendedLock (void) {
    int spinCount = 1;
    do {
      if (Atomic::exchange (const_cast<unsigned long *>(&mutex), LOCKED)
	  == UNLOCKED) {
	// We got the lock.
	return;
      }
      // printf ("contention!\n");
      _MM_PAUSE;
      // Exponential back-off protocol.
      for (volatile int q = 0; q < spinCount; q++) {
      }
      spinCount <<= 1;
      if (spinCount > MAX_SPIN_LIMIT) {
	yieldProcessor();
	spinCount = 1;
      }
    } while (1);
  }

  // Is this system a multiprocessor?
  inline bool onMultiprocessor (void) {
    CPUInfo cpuInfo;
    return (cpuInfo.getNumProcessors() > 1);
  }

  inline void yieldProcessor (void) {
#if defined(_WIN32)
    Sleep(0);
#else
#if defined(__SVR4)
    thr_yield();
#else
    sched_yield();
#endif
#endif
  }

  enum { UNLOCKED = 0, LOCKED = 1 };
  
  enum { MAX_SPIN_LIMIT = 1024 };

  union {
    double _dummy;
    volatile unsigned long mutex;
  };

  double _spacing[8];

};

#endif
