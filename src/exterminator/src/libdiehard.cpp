/* -*- C++ -*- */

/**
 * @file   libdiehard.cpp
 * @brief  Replaces malloc and friends with DieHard versions.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#include <signal.h>
#include <stdlib.h>
#include <new>
#include <link.h>

class std::bad_alloc;

volatile int anyThreadCreated = 1;

#include "correct.h"
#include "diehard.h"
#include "realrandomvalue.h"
#include "version.h"
#include "partition_metadata.h"

// Dynamic library load base address
void * dl_base;
void * stack_base;
unsigned long sentinel;

unsigned long global_ct;
unsigned long _death_time;

unsigned long _heap_config_flags;

/*
  We have to intercept time and date functions (times, getrusage).
  For now, we simply use bogus time functions with known values.

 */


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

void dumpHeapHandler(int signum, siginfo_t * act, void * old);

void reloadPatch(int signum) {
  // XXX: fill in
}

template <class Super>
class TheCustomHeapTemplate : public Super {
public:
  TheCustomHeapTemplate (void)
  {
#if defined(linux)
    fprintf (stderr, "Using the DieHard runtime system (Linux version).\nCopyright (C) 2005 Emery Berger, University of Massachusetts Amherst.\n");
#endif
    signal(SIGRTMIN+4,reloadPatch);
    
    struct sigaction sa_str;

    sa_str.sa_sigaction = dumpHeapHandler;
    sigfillset(&sa_str.sa_mask);
    sa_str.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV,&sa_str,NULL);
    sigaction(SIGABRT,&sa_str,NULL);
    sigaction(SIGUSR1,&sa_str,NULL);
    // Notification that we're a "bad" replica + dump your core
    sigaction(SIGUSR2,&sa_str,NULL);
  }
};

void trap() { raise(SIGTRAP); }

typedef TheCustomHeapTemplate<CorrectingHeap<DieHardHeap<MAX_HEAP_SIZE, 1, 2, ExterminatorMetadata> > > TheCustomHeapType;

static TheCustomHeapType * getCustomHeap (void) {
  static char theHeapBuf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * theCustomHeap
    = new ((void *) theHeapBuf) TheCustomHeapType;
  return theCustomHeap;
}

void dumpHeapHandler(int signum, siginfo_t * act, void * foo) {
  fprintf(stderr,"libdiehard caught signal (%d) handler, pid = %d\n",signum,getpid());
  getCustomHeap()->dump_heap(signum != SIGUSR1 && signum != SIGUSR2);

  if(signum != SIGUSR1) {
    fprintf(stderr,"libdiehard raising %d on pid %d\n",signum,getpid());

    struct sigaction sigfoo;

    sigfoo.sa_handler = SIG_DFL;

    sigaction(signum, &sigfoo, NULL);

    raise(signum);
    exit(4);
  } else {
    fprintf(stderr,"finished dumping core\n");
    kill(act->si_pid,SIGUSR1);
  }
}



#if defined(_WIN32)
#pragma warning(disable:4273)
#endif

#include "wrapper.cpp"

#if defined(_WIN32)
#ifndef CUSTOM_DLLNAME
#define CUSTOM_DLLNAME DllMain
#endif

#ifndef DIEHARD_PRE_ACTION
#define DIEHARD_PRE_ACTION
#endif
#endif // _WIN32

/*
  Thread-specific random number generator stuff.
*/

#if defined(_WIN32)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL __thread
#endif // _WIN32

static THREAD_LOCAL double rngBuf[sizeof(RandomNumberGenerator)/sizeof(double) + 1];
static THREAD_LOCAL RandomNumberGenerator * threadSpecificRNG = NULL;

static int threadCount = 0;
static unsigned long mv1 = 0;
static unsigned long mv2 = 0;

static void initializeRNG (void) {
#if !defined(_WIN32)
    // XXX: Use deterministic randomness for now
  /*
  long v1, v2;
    char * seedstr = getenv("DH_SEED");
    v1 = v2 = 0xdeadbeef;
    v1 = v2 = atoi(seedstr);
  */
  long v1 = RealRandomValue::value();
  long v2 = RealRandomValue::value();
#else
  long v1;
  long v2;
  if (mv1 == 0) {
    mv1 = RealRandomValue::value();
    mv2 = RealRandomValue::value();
  }
  v1 = mv1 + threadCount;
  v2 = mv2 + threadCount;
#endif
  threadSpecificRNG = (RandomNumberGenerator *) rngBuf;
  new ((char *) rngBuf) RandomNumberGenerator(v1, v2);
  //  int tc = threadCount;
  //  printf ("initialized an rng (%x) with v1=%d, v2=%d\n", (void *) threadSpecificRNG, v1, v2);
}

inline RandomNumberGenerator& getRNG (void) {
  if (threadSpecificRNG == NULL) {
    initializeRNG();
    threadCount++;
  }
  return *threadSpecificRNG;
}

#if defined(_WIN32)

extern "C" {

BOOL WINAPI CUSTOM_DLLNAME (HANDLE hinstDLL, DWORD fdwReason, LPVOID lpreserved)
{
  char buf[255];

  switch (fdwReason) {

  case DLL_PROCESS_ATTACH:
    initializeRNG ();
    sprintf (buf, "Using the DieHard runtime system (Windows DLL, version %d.%d).\nCopyright (C) 2005 Emery Berger, University of Massachusetts Amherst.\n", VERSION_MAJOR_NUMBER, VERSION_MINOR_NUMBER);
    fprintf (stderr, buf);
    DIEHARD_PRE_ACTION;
    break;

  case DLL_PROCESS_DETACH:
    DIEHARD_POST_ACTION;
    break;

  case DLL_THREAD_ATTACH:
    //    threadCount++;
    initializeRNG ();
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

