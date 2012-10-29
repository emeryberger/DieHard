// -*- C++ -*-

/**
 * @file   wrapper.cpp
 * @brief  Replaces malloc with appropriate calls to TheCustomHeapType.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005-2006 by Emery Berger, University of Massachusetts Amherst.
 */

#include <string.h> // for memcpy
#include <errno.h>

#if !defined(CUSTOM_PREFIX)
#define CUSTOM_PREFIX(n) n
#endif

#define CUSTOM_MALLOC(x)     CUSTOM_PREFIX(malloc)(x)
#define CUSTOM_FREE(x)       CUSTOM_PREFIX(free)(x)
#define CUSTOM_REALLOC(x,y)  CUSTOM_PREFIX(realloc)(x,y)
#define CUSTOM_CALLOC(x,y)   CUSTOM_PREFIX(calloc)(x,y)
#define CUSTOM_MEMALIGN(x,y) CUSTOM_PREFIX(memalign)(x,y)
#define CUSTOM_GETSIZE(x)    CUSTOM_PREFIX(malloc_usable_size)(x)
#define CUSTOM_MALLOPT(x,y)  CUSTOM_PREFIX(mallopt)(x,y)
#define CUSTOM_VALLOC(x)     CUSTOM_PREFIX(valloc)(x)
#define CUSTOM_PVALLOC(x)    CUSTOM_PREFIX(pvalloc)(x)

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Disable warnings about long (> 255 chars) identifiers.
#pragma warning(disable:4786)
// Set inlining to the maximum possible depth.
#pragma inline_depth(255)
#define MYCDECL __cdecl
#if !defined(NO_INLINE)
#define NO_INLINE __declspec(noinline)
#endif
#if !defined(NDEBUG)
#define __forceinline inline
#endif

#else // not _WIN32
  #define MYCDECL
#endif

/***** generic malloc functions *****/

static int bigMallocs = 0;

extern "C" void * MYCDECL CUSTOM_MALLOC (size_t sz)
{
#if 0
  if (sz >= 4096) {
    bigMallocs++;
    char buf[255];
    sprintf (buf, "big mo mallocs = %d\n", bigMallocs);
    printf (buf);
  }
#endif
  void * ptr = getCustomHeap()->malloc (sz);
#if !DIEHARD_DIEFAST
  if (ptr) {
    memset (ptr, 0, sz);
  }
#endif
  return ptr;
}

extern "C" void * MYCDECL CUSTOM_CALLOC (size_t nelem, size_t elsize)
{
  size_t n = nelem * elsize;
  if (n == 0) {
    n = 1;
  }
  void * ptr = getCustomHeap()->malloc (n);
  if (ptr) {
    memset (ptr, 0, n);
  }
  return ptr;
}

extern "C" void * MYCDECL CUSTOM_MEMALIGN (size_t alignment, size_t size)
{
  // Check for non power-of-two alignment, or mistake in size.
  if ((alignment == 0) ||
      (alignment & (alignment - 1)))
    {
      return NULL;
    }
  void * buf = CUSTOM_MALLOC (2 * alignment + size);
  return (void *) (((size_t) buf + alignment - 1) & ~(alignment - 1));
}


extern "C" size_t MYCDECL CUSTOM_GETSIZE (void * ptr)
{
  if (ptr == NULL) {
    return 0;
  }
  size_t objSize = getCustomHeap()->getSize(ptr);
  return objSize;
}

extern "C" void MYCDECL CUSTOM_FREE (void * ptr)
{
  // FIX ME
  //  memset (ptr, 0, CUSTOM_GETSIZE(ptr));
  getCustomHeap()->free (ptr);
}


// for 4.3BSD compatibility.

extern "C" void MYCDECL CUSTOM_PREFIX(cfree) (void * ptr)
{
  getCustomHeap()->free (ptr);
}

extern "C" void * MYCDECL CUSTOM_REALLOC (void * ptr, size_t sz)
{
  if (ptr == NULL) {
    ptr = CUSTOM_MALLOC (sz);
    return ptr;
  }
  if (sz == 0) {
    CUSTOM_FREE (ptr);
    return NULL;
  }

  size_t objSize = CUSTOM_GETSIZE (ptr);

  void * buf = CUSTOM_MALLOC ((size_t) (sz));

  if (buf != NULL) {
    // Copy the contents of the original object
    // up to the size of the new block.
    size_t minSize = (objSize < sz) ? objSize : sz;
    memcpy (buf, ptr, minSize);
  }

  // Free the old block.
  CUSTOM_FREE (ptr);

  // Return a pointer to the new one.
  return buf;
}

#if defined(linux)
extern "C" char * MYCDECL CUSTOM_PREFIX(strndup) (const char * s, size_t sz)
{
  char * newString = NULL;
  if (s != NULL) {
    size_t cappedLength = strnlen (s, sz);
    if ((newString = (char *) CUSTOM_MALLOC(cappedLength + 1))) {
      strncpy(newString, s, cappedLength);
      newString[cappedLength] = '\0';
    }
  }
  return newString;
}
#endif

#if 0 // FIX ME
extern "C" char * MYCDECL CUSTOM_PREFIX(strdup) (const char * s)
{
  char * newString = NULL;
  if (s != NULL) {
    int len = strlen(s) + 1;
    if ((newString = (char *) CUSTOM_MALLOC(len))) {
      strncpy(newString, s, len);
    }
  }
  return newString;
}
#endif

#if defined(__GNUC__)
#include <wchar.h>
extern "C" wchar_t * MYCDECL CUSTOM_PREFIX(wcsdup) (const wchar_t * s)
{
  wchar_t * newString = NULL;
  if (s != NULL) {
    if ((newString = (wchar_t *) CUSTOM_MALLOC(wcslen(s) + 1))) {
      wcscpy(newString, s);
    }
  }
  return newString;
}
#endif

namespace std {
  struct nothrow_t;
}


void * operator new (size_t sz)
{
  return CUSTOM_MALLOC (sz);
}

void operator delete (void * ptr)
{
  CUSTOM_FREE (ptr);
}

#if !defined(__SUNPRO_CC) || __SUNPRO_CC > 0x420
void * operator new (size_t sz, const std::nothrow_t&) throw() {
  return CUSTOM_MALLOC(sz);
} 

void * operator new[] (size_t size) throw(std::bad_alloc)
{
  return CUSTOM_MALLOC(size);
}

void * operator new[] (size_t sz, const std::nothrow_t&) throw() {
  return CUSTOM_MALLOC (sz);
} 

void operator delete[] (void * ptr)
{
  CUSTOM_FREE (ptr);
}
#endif

#if !defined(_WIN32)
#include <dlfcn.h>
#include <limits.h>

#if !defined(RTLD_NEXT)
#define RTLD_NEXT ((void *) -1)
#endif


typedef char * getcwdFunction (char *, size_t);

extern "C"  char * MYCDECL CUSTOM_PREFIX(getcwd) (char * buf, size_t size)
{
  static getcwdFunction * real_getcwd
    = (getcwdFunction *) dlsym (RTLD_NEXT, "getcwd");
  
  if (!buf) {
    if (size == 0) {
      size = PATH_MAX;
    }
    buf = (char *) CUSTOM_PREFIX(malloc)(size);
  }
  return (real_getcwd)(buf, size);
}

#endif


/***** replacement functions for GNU libc extensions to malloc *****/

// A stub function to ensure that we capture mallopt.
// It does nothing and always returns a failure value (0).
extern "C" int MYCDECL CUSTOM_MALLOPT (int, int)
{
  // Always fail.
  return 0;
}

extern "C" void * MYCDECL CUSTOM_VALLOC (size_t sz)
{
  // Equivalent to memalign(pagesize, sz).
  // For convenience, we assume pages are 8K.
  void * ptr = CUSTOM_MEMALIGN (8192, sz);
  return ptr;
}


extern "C" void * MYCDECL CUSTOM_PVALLOC (size_t sz)
{
  // Rounds up to the next pagesize and then calls valloc.
  sz = (sz + 8191) & ~8191;
  return CUSTOM_VALLOC(sz);
}

extern "C" int posix_memalign (void **memptr, size_t alignment, size_t size)
{
  // Check for non power-of-two alignment.
  if ((alignment == 0) ||
      (alignment & (alignment - 1)))
    {
      return EINVAL;
    }
  void * ptr = CUSTOM_MEMALIGN (alignment, size);
  if (!ptr) {
    return ENOMEM;
  } else {
    *memptr = ptr;
    return 0;
  }
}

