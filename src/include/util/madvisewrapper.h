// -*- C++ -*-

#ifndef DH_MADVISEWRAPPER_H
#define DH_MADVISEWRAPPER_H

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#endif


#include "heaplayers.h"
// #include "mmapwrapper.h"

class MadviseWrapper {
public:

  static void random (void * ptr, size_t sz) {
#if defined(_WIN32)
    // For now, do nothing; there is no equivalent functionality in
    // Windows.
    ptr = ptr;
    sz = sz;
#else
    // Assume Unix platform.
    madvise (ptr, sz, MADV_RANDOM);
#endif
  }

  static void prefetch (void * ptr, size_t sz) {
#if defined(_WIN32)
    // For now, do nothing. // FIX ME
    ptr = ptr;
    sz = sz;
#else
    // Assume Unix platform.
    madvise (ptr, sz, MADV_WILLNEED);
#endif
  }

  // Release the given range of memory to the OS (without unmapping it).
  static void discard (void * ptr, size_t sz) {
    if ((size_t) ptr % HL::MmapWrapper::Size == 0) {
      // Extra sanity check in case the declared alignment is wrong!
#if defined(_WIN32)
      VirtualAlloc (ptr, sz, MEM_RESET, PAGE_NOACCESS);
#elif defined(__APPLE__)
      madvise (ptr, sz, MADV_DONTNEED);
      madvise (ptr, sz, MADV_FREE);
#else
      // Assume Unix platform.
      madvise (ptr, sz, MADV_DONTNEED);
#endif
    }
  }

  static void huge (void * ptr, size_t sz) {
#if defined(linux) && defined(MADV_HUGEPAGE)
    madvise (ptr, sz, MADV_HUGEPAGE);
#endif
  }


};

#endif

