// -*- C++ -*-

/**
 * @file   wrapper.cpp
 * @brief  Replaces malloc with appropriate calls to TheCustomHeapType.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#include <string.h> // for memcpy

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Disable warnings about long (> 255 chars) identifiers.
#pragma warning(disable:4786)
// Set inlining to the maximum possible depth.
#pragma inline_depth(255)
#pragma warning(disable: 4074)	// initializers put in compiler reserved area
#pragma init_seg(compiler)

#pragma comment(linker, "/merge:.CRT=.data")

#pragma comment(linker, "/disallowlib:libc.lib")
#pragma comment(linker, "/disallowlib:libcd.lib")
#pragma comment(linker, "/disallowlib:libcmt.lib")
#pragma comment(linker, "/disallowlib:libcmtd.lib")
#pragma comment(linker, "/disallowlib:msvcrtd.lib")
#endif

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
#define MYCDECL __cdecl
#if !defined(NO_INLINE)
#define NO_INLINE __declspec(noinline)
#endif
#pragma inline_depth(255)

#if !defined(NDEBUG)
#define __forceinline inline
#endif

#else
#define MYCDECL
#endif

inline void * __malloc_impl(size_t sz) {
  void * ptr = getCustomHeap()->malloc (sz);
  return ptr;
}

inline void __free_impl(void * ptr) {
  getCustomHeap()->free(ptr);
}

/***** generic malloc functions *****/

extern "C" void * MYCDECL CUSTOM_MALLOC (size_t sz)
{
  return __malloc_impl(sz);
}

extern "C" void * MYCDECL CUSTOM_CALLOC (size_t nelem, size_t elsize)
{
  size_t n = nelem * elsize;
  void * ptr = getCustomHeap()->malloc (n);
  // In non-replicated mode, it's already initialized to zero.
  if (ptr != NULL) {
    memset (ptr, 0, n);
  }

  return ptr;
}

extern "C" void * MYCDECL CUSTOM_MEMALIGN (size_t, size_t size)
{
  // NOTE: This function is deprecated and here just acts like malloc.
  return __malloc_impl(size);
}

extern "C" size_t MYCDECL CUSTOM_GETSIZE (void * ptr)
{
  if (ptr == NULL) {
    return 0;
  }
  return getCustomHeap()->getSize(ptr);
}

extern "C" void MYCDECL CUSTOM_FREE (void * ptr)
{
  __free_impl(ptr);
}


// for 4.3BSD compatibility.

extern "C" void MYCDECL CUSTOM_PREFIX(cfree) (void * ptr)
{
  __free_impl(ptr);
}

extern "C" void * MYCDECL CUSTOM_REALLOC (void * ptr, size_t sz)
{
  if (ptr == NULL) {
    ptr = __malloc_impl(sz);
    return ptr;
  }
  if (sz == 0) {
    __free_impl(ptr);
    return NULL;
  }

  size_t objSize = CUSTOM_GETSIZE (ptr);
  void * buf = __malloc_impl((size_t) (sz));

  if (buf != NULL) {
    // Copy the contents of the original object
    // up to the size of the new block.
    size_t minSize = (objSize < sz) ? objSize : sz;
    memcpy (buf, ptr, minSize);
  }

  // Free the old block.
  __free_impl(ptr);

  // Return a pointer to the new one.
  return buf;
}

#if defined(linux)
extern "C" char * MYCDECL CUSTOM_PREFIX(strndup) (const char * s, size_t sz)
{
  char * newString = NULL;
  if (s != NULL) {
    size_t cappedLength = strnlen (s, sz);
    if ((newString = (char *) __malloc_impl(cappedLength + 1))) {
      strncpy(newString, s, cappedLength);
      newString[cappedLength] = '\0';
    }
  }
  return newString;
}
#endif

extern "C" char * MYCDECL CUSTOM_PREFIX(strdup) (const char * s)
{
  char * newString = NULL;
  if (s != NULL) {
    if ((newString = (char *) CUSTOM_MALLOC(strlen(s) + 1))) {
      strcpy(newString, s);
    }
  }
  return newString;
}

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


namespace std {
  struct nothrow_t;
}


void * operator new (size_t sz)
{
  return __malloc_impl(sz);
}

void operator delete (void * ptr)
{
  __free_impl(ptr);
}

#if !defined(__SUNPRO_CC) || __SUNPRO_CC > 0x420
void * operator new (size_t sz, const std::nothrow_t&) throw() {
  return __malloc_impl(sz);
} 

#if !defined(_WIN32)
void * operator new[] (size_t size) throw(std::bad_alloc)
{
  return __malloc_impl(size);
}
#endif

void * operator new[] (size_t sz, const std::nothrow_t&) throw() {
  return __malloc_impl(sz);
} 

void operator delete[] (void * ptr)
{
  __free_impl(ptr);
}
#endif


/***** replacement functions for GNU libc extensions to malloc *****/

// A stub function to ensure that we capture mallopt.
// It does nothing and always returns a failure value (0).
extern "C" int MYCDECL CUSTOM_MALLOPT (int number, int value)
{
  // Always fail.
  return 0;
}

extern "C" void * MYCDECL CUSTOM_VALLOC (size_t sz)
{
  // Equivalent to memalign(pagesize, sz), which Hoard doesn't support
  // anyway...
  return CUSTOM_MALLOC (sz);
}


extern "C" void * MYCDECL CUSTOM_PVALLOC (size_t sz)
{
  // Rounds up to the next pagesize and then calls valloc. Hoard
  // doesn't support aligned memory requests
  return CUSTOM_MALLOC (sz);
}



#if 0 // defined(_WIN32)

#define _CRTDBG_ALLOC_MEM_DF        0x01  /* Turn on debug allocation */

#define _HEAPOK         (-2)
#define _HEAPEND        (-5)

#ifndef _HEAPINFO_DEFINED
typedef struct _heapinfo {
        int * _pentry;
        size_t _size;
        int _useflag;
        } _HEAPINFO;
#define _HEAPINFO_DEFINED
#endif

typedef void(*_CRT_ALLOC_HOOK)(void);
typedef void(*_CRT_DUMP_CLIENT)(void);
typedef int _CrtMemState;

#define EXPORT

extern "C" {
  EXPORT size_t CUSTOM_PREFIX(_msize)(void *memblock)
  {
    return CUSTOM_PREFIX(malloc_usable_size) (memblock);
  }

  EXPORT void *  CUSTOM_PREFIX(debug_operator_new) (unsigned int cb, int, const char *, int)
  {
    return CUSTOM_PREFIX(malloc)(cb);
  }

  EXPORT void    CUSTOM_PREFIX(debug_operator_delete) (void * p, int, const char *, int)
  {
    CUSTOM_PREFIX(free)(p);
  }

  EXPORT void * CUSTOM_PREFIX(nh_malloc_dbg)(size_t sz, size_t, int, int, const char *, int) {
    return CUSTOM_PREFIX(malloc) (sz);
  }


  /*---------------------------------------------------------------------------
   */
#if 0
#if defined(_MSC_VER)
  EXPORT void CUSTOM_PREFIX(assert_failed)(const char *assertion, const char *fileName, int lineNumber)
  { __asm int 3 }
#else
  EXPORT void CUSTOM_PREFIX(assert_failed)(const char *assertion, const char *fileName, int lineNumber) {}
#endif
#endif

  /*---------------------------------------------------------------------------
   */
  EXPORT void *CUSTOM_PREFIX(_calloc_dbg)(size_t num, size_t size, 
							int blockType, const char *filename, int linenumber)
  { return CUSTOM_PREFIX(calloc)(num, size); }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_CrtCheckMemory)(void) { return 1; }

  /*---------------------------------------------------------------------------
   */
  EXPORT void CUSTOM_PREFIX(_CrtDoForAllClientObjects)(void(*pfn)(void*,void*), void *context) { }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_CrtDumpMemoryLeaks)(void) { return 0; }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_CrtIsMemoryBlock)(const void *userData, unsigned int size, 
							    long *requestNumber, char **filename, int *linenumber) { return 1; }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_CrtIsValidHeapPointer)(const void *userData) { return 1; }

  /*---------------------------------------------------------------------------
   */
  EXPORT void CUSTOM_PREFIX(_CrtMemCheckpoint)(_CrtMemState *state) { }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_CrtMemDifference)(_CrtMemState *stateDiff, 
							    const _CrtMemState *oldState, const _CrtMemState *newState) 
  { return 0; }

  /*---------------------------------------------------------------------------
   */
  EXPORT void CUSTOM_PREFIX(_CrtMemDumpAllObjectsSince)(const _CrtMemState *state) { }

  /*---------------------------------------------------------------------------
   */
  EXPORT void CUSTOM_PREFIX(_CrtMemDumpStatistics)(const _CrtMemState *state) { }

  /*---------------------------------------------------------------------------
   */
  EXPORT _CRT_ALLOC_HOOK CUSTOM_PREFIX(_CrtSetAllocHook)(_CRT_ALLOC_HOOK allocHook) { return 0; }

  /*---------------------------------------------------------------------------
   */
  EXPORT long CUSTOM_PREFIX(_CrtSetBreakAlloc)(long lBreakAlloc) { return 0; }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_CrtSetDbgFlag)(int newFlag) { return _CRTDBG_ALLOC_MEM_DF; }

  /*---------------------------------------------------------------------------
   */
  EXPORT _CRT_DUMP_CLIENT CUSTOM_PREFIX(_CrtSetDumpClient)(_CRT_DUMP_CLIENT dumpClient) { return 0; }

  /*---------------------------------------------------------------------------
   */
  EXPORT void *CUSTOM_PREFIX(_malloc_dbg)(size_t size, int blockType, 
							const char *filename, int linenumber)
  { return CUSTOM_PREFIX(malloc)(size); }

  /*---------------------------------------------------------------------------
   */
  EXPORT void *CUSTOM_PREFIX(_expand_dbg)(void *userData, size_t newSize, int blockType, 
							const char *filename, int linenumber) { return NULL; }

  /*---------------------------------------------------------------------------
   */
  EXPORT void CUSTOM_PREFIX(_free_dbg)(void *userData, int blockType) { CUSTOM_PREFIX(free)(userData); }

  /*---------------------------------------------------------------------------
   */
  EXPORT size_t CUSTOM_PREFIX(_msize_dbg)(void *memblock, int blockType) 
  { return CUSTOM_PREFIX(_msize)(memblock); }

  /*---------------------------------------------------------------------------
   */
  EXPORT void * CUSTOM_PREFIX(_realloc_dbg)(void *userData, size_t newSize, 
							  int blockType, const char *filename, int linenumber)
  { return CUSTOM_PREFIX(realloc)(userData, newSize); }

  // #endif	// ? _DEBUG

  /*---------------------------------------------------------------------------
   */
  EXPORT void *CUSTOM_PREFIX(_expand)(void * mem, size_t sz) { return NULL; }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_heapchk)(void) { return _HEAPOK; }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_heapmin)(void) { return 0; }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_heapset)(unsigned int) { return _HEAPOK; }

  /*---------------------------------------------------------------------------
   */
  EXPORT int CUSTOM_PREFIX(_heapwalk)(_HEAPINFO *) { return _HEAPEND; }


  /*---------------------------------------------------------------------------
   */
  void *  CUSTOM_PREFIX(new_nothrow)(unsigned int sz,struct std::nothrow_t const &) {
    return CUSTOM_PREFIX(malloc)(sz);
  }
}
#endif
