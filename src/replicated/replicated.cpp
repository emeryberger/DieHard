/*************************  stubs for non-deterministic functions  ************************/

/*
  For DieHard's replicated mode, we have to intercept time and date
  functions (times, getrusage).  For now, we simply use bogus time
  functions with known values.

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
