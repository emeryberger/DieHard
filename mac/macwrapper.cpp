// -*- C++ -*-

/**
 * @file   macwrapper.cpp
 * @brief  Replaces malloc family on Macs with custom versions.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2010 by Emery Berger, University of Massachusetts Amherst.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc/malloc.h>
#include <errno.h>

/*
  To use this library,
  you only need to define the following allocation functions:
  
  - xxmalloc
  - xxfree
  - xxmalloc_usable_size
  - xxmalloc_lock
  - xxmalloc_unlock
  
  See the extern "C" block below for function prototypes and more
  details. YOU SHOULD NOT NEED TO MODIFY ANY OF THE CODE HERE TO
  SUPPORT ANY ALLOCATOR.


  LIMITATIONS:

  - This wrapper assumes that the underlying allocator will do "the
    right thing" when xxfree() is called with a pointer internal to an
    allocated object. Header-based allocators, for example, need not
    apply.

  - This wrapper also assumes that there is some way to lock all the
    heaps used by a given allocator; however, such support is only
    required by programs that also call fork(). In case your program
    does not, the lock and unlock calls given below can be no-ops.

*/

extern "C" {

  void * xxmalloc (size_t);
  void   xxfree (void *);

  // Takes a pointer and returns how much space it holds.
  size_t xxmalloc_usable_size (void *);

  // Locks the heap(s), used prior to any invocation of fork().
  void xxmalloc_lock (void);

  // Unlocks the heap(s), after fork().
  void xxmalloc_unlock (void);

}

//////////
//////////

// All replacement functions get the prefix macwrapper_.

#define MACWRAPPER_PREFIX(n) macwrapper_##n

#include <syslog.h>

extern "C" {

  void * MACWRAPPER_PREFIX(malloc) (size_t sz) {
#if 0
    // Just for debugging purposes.
    static unsigned long long counter = 0;
    static unsigned long mallocs = 0;
    //    static unsigned long long sizes = 0;
    //    sizes += sz;
    ++mallocs;
    if (mallocs % 500000 == 0) {
      char buf[255];
      sprintf (buf, "DieHarder in operation (%lld).\n", counter); // , mallocs); // , (float) sizes / (float) mallocs);
      syslog (LOG_ALERT, buf);
      mallocs = 0;
      counter++;
      //      fprintf (stderr, buf);
    }
#endif
    return xxmalloc(sz);
  }

  size_t MACWRAPPER_PREFIX(malloc_usable_size) (void * ptr) {
    if (ptr == NULL) {
      return 0;
    }
    size_t objSize = xxmalloc_usable_size (ptr);
    return objSize;
  }

  void   MACWRAPPER_PREFIX(free) (void * ptr) {
    xxfree (ptr);
  }

  size_t MACWRAPPER_PREFIX(malloc_good_size) (size_t sz) {
    void * ptr = xxmalloc(sz);
    size_t objSize = xxmalloc_usable_size(ptr);
    xxfree(ptr);
    return objSize;
  }

  static void * _extended_realloc (void * ptr, size_t sz, bool isReallocf) 
  {
    // NULL ptr = malloc.
    if (ptr == NULL) {
      return xxmalloc(sz);
    }

    // 0 size = free. We return a small object.  This behavior is
    // apparently required under Mac OS X and optional under POSIX.
    if (sz == 0) {
      xxfree (ptr);
      return xxmalloc(1);
    }

    size_t originalSize = xxmalloc_usable_size (ptr);
    size_t minSize = (originalSize < sz) ? originalSize : sz;

#if 0
    // Don't change size if the object is shrinking by less than half.
    if ((originalSize / 2 < sz) && (sz <= originalSize)) {
      // Do nothing.
      return ptr;
    }
#endif

    void * buf = xxmalloc (sz);

    if (buf != NULL) {
      // Successful malloc.
      // Copy the contents of the original object
      // up to the size of the new block.
      memcpy (buf, ptr, minSize);
      xxfree (ptr);
    } else {
      if (isReallocf) {
	// Free the old block if the new allocation failed.
	// Specific behavior for Mac OS X reallocf().
	xxfree (ptr);
      }
    }

    // Return a pointer to the new one.
    return buf;
  }

  void * MACWRAPPER_PREFIX(realloc) (void * ptr, size_t sz) {
    return _extended_realloc (ptr, sz, false);
  }

  void * MACWRAPPER_PREFIX(reallocf) (void * ptr, size_t sz) {
    return _extended_realloc (ptr, sz, true);
  }

  void * MACWRAPPER_PREFIX(calloc) (size_t elsize, size_t nelems) {
    size_t n = nelems * elsize;
    if (n == 0) {
      n = 1;
    }
    void * ptr = xxmalloc (n);
    if (ptr) {
      memset (ptr, 0, n);
    }
    return ptr;
  }

  char * MACWRAPPER_PREFIX(strdup) (const char * s)
  {
    char * newString = NULL;
    if (s != NULL) {
      int len = strlen(s) + 1;
      if ((newString = (char *) xxmalloc(len))) {
	memcpy (newString, s, len);
      }
    }
    return newString;
  }

  void * MACWRAPPER_PREFIX(memalign) (size_t alignment, size_t size)
  {
    // Check for non power-of-two alignment, or mistake in size.
    if ((alignment == 0) ||
	(alignment & (alignment - 1)))
      {
	return NULL;
      }
    // Try to just allocate an object of the requested size.
    // If it happens to be aligned properly, just return it.
    void * ptr = xxmalloc (size);
    if (((size_t) ptr & (alignment - 1)) == (size_t) ptr) {
      // It is already aligned just fine; return it.
      return ptr;
    }
    // It was not aligned as requested: free the object.
    xxfree (ptr);
    // Now get a big chunk of memory and align the object within it.
    // NOTE: this assumes that the underlying allocator will be able
    // to free the aligned object, or ignore the free request.
    void * buf = xxmalloc (2 * alignment + size);
    void * alignedPtr = (void *) (((size_t) buf + alignment - 1) & ~(alignment - 1));
    return alignedPtr;
  }

  int MACWRAPPER_PREFIX(posix_memalign)(void **memptr, size_t alignment, size_t size)
  {
    // Check for non power-of-two alignment.
    if ((alignment == 0) ||
	(alignment & (alignment - 1)))
      {
	return EINVAL;
      }
    void * ptr = MACWRAPPER_PREFIX(memalign) (alignment, size);
    if (!ptr) {
      return ENOMEM;
    } else {
      *memptr = ptr;
      return 0;
    }
  }

  void * MACWRAPPER_PREFIX(valloc) (size_t sz)
  {
    // Equivalent to memalign(pagesize, sz).
    void * ptr = MACWRAPPER_PREFIX(memalign) (PAGE_SIZE, sz);
    return ptr;
  }

}


/////////
/////////

extern "C" {
  // operator new
  void * _Znwm (unsigned long);
  void * _Znam (unsigned long);

  // operator delete
  void _ZdlPv (void *);
  void _ZdaPv (void *);

  // nothrow variants
  // operator new nothrow
  void * _ZnwmRKSt9nothrow_t ();
  void * _ZnamRKSt9nothrow_t ();
  // operator delete nothrow
  void _ZdaPvRKSt9nothrow_t (void *);

  void _malloc_fork_prepare (void);
  void _malloc_fork_parent (void);
  void _malloc_fork_child (void);

  // 32-bit only
#if 0
  void * _Znaj (unsigned int);
  void * _Znwj (unsigned int);
  void * _ZnwjRKSt9nothrow_t (unsigned int);
  void * _ZnajRKSt9nothrow_t (unsigned int);
#endif
}

static malloc_zone_t theDefaultZone;

extern "C" {

  unsigned MACWRAPPER_PREFIX(malloc_zone_batch_malloc)(malloc_zone_t *, size_t, void **, unsigned) {
    return 0;
  }

  void MACWRAPPER_PREFIX(malloc_zone_batch_free)(malloc_zone_t *, void **, unsigned) {
    // Do nothing.
  }

  bool MACWRAPPER_PREFIX(malloc_zone_check)(malloc_zone_t *) {
    // Just return true for all zones.
    return true;
  }

  void MACWRAPPER_PREFIX(malloc_zone_print)(malloc_zone_t *, bool) {
    // Do nothing.
  }

  void MACWRAPPER_PREFIX(malloc_zone_log)(malloc_zone_t *, void *) {
    // Do nothing.
  }

  const char * MACWRAPPER_PREFIX(malloc_get_zone_name)(malloc_zone_t *) {
    return theDefaultZone.zone_name;
  }

  void MACWRAPPER_PREFIX(malloc_set_zone_name)(malloc_zone_t *, const char *) {
    // do nothing.
  }

  malloc_zone_t * MACWRAPPER_PREFIX(malloc_create_zone)(vm_size_t,
							unsigned)
  {
    return &theDefaultZone;
  }
  
  void * MACWRAPPER_PREFIX(malloc_default_zone) (void) {
    return (void *) &theDefaultZone;
  }

  void MACWRAPPER_PREFIX(malloc_destroy_zone) (malloc_zone_t *) {
    // A stub for now. FIX ME
  }
  
  malloc_zone_t * MACWRAPPER_PREFIX(malloc_zone_from_ptr) (const void *) {
    return NULL;
  }
  
  void * MACWRAPPER_PREFIX(malloc_zone_malloc) (malloc_zone_t *, size_t size) {
    return MACWRAPPER_PREFIX(malloc) (size);
  }
  
  void * MACWRAPPER_PREFIX(malloc_zone_calloc) (malloc_zone_t *, size_t n, size_t size) {
    return MACWRAPPER_PREFIX(calloc) (n, size);
  }
  
  void * MACWRAPPER_PREFIX(malloc_zone_valloc) (malloc_zone_t *, size_t size) {
    return MACWRAPPER_PREFIX(valloc) (size);
  }
  
  void * MACWRAPPER_PREFIX(malloc_zone_realloc) (malloc_zone_t *, void * ptr, size_t size) {
    return MACWRAPPER_PREFIX(realloc) (ptr, size);
  }
  
  void * MACWRAPPER_PREFIX(malloc_zone_memalign) (malloc_zone_t *, size_t alignment, size_t size) {
    return MACWRAPPER_PREFIX(memalign) (alignment, size);
  }
  
  void MACWRAPPER_PREFIX(malloc_zone_free) (malloc_zone_t *, void * ptr) {
    xxfree (ptr);
  }
  
  void MACWRAPPER_PREFIX(malloc_zone_free_definite_size) (malloc_zone_t *, void * ptr, size_t) {
    xxfree (ptr);
  }

  void MACWRAPPER_PREFIX(malloc_zone_register) (malloc_zone_t *) {
  }

  void MACWRAPPER_PREFIX(malloc_zone_unregister) (malloc_zone_t *) {
  }

  int MACWRAPPER_PREFIX(malloc_jumpstart)(int) {
    return 1;
  }

  size_t MACWRAPPER_PREFIX(internal_malloc_zone_size) (malloc_zone_t *, const void * ptr) {
    return xxmalloc_usable_size ((void *) ptr);
  }

  void MACWRAPPER_PREFIX(_malloc_fork_prepare)(void) {
    /* Prepare the malloc module for a fork by insuring that no thread is in a malloc critical section */
    xxmalloc_lock();
  }

  void MACWRAPPER_PREFIX(_malloc_fork_parent)(void) {
    /* Called in the parent process after a fork() to resume normal operation. */
    xxmalloc_unlock();
  }

  void MACWRAPPER_PREFIX(_malloc_fork_child)(void) {
    /* Called in the child process after a fork() to resume normal operation.  In the MTASK case we also have to change memory inheritance so that the child does not share memory with the parent. */
    xxmalloc_unlock();
  }

}

extern "C" void vfree (void *);
extern "C" int malloc_jumpstart (int);

// The interposition data structure (just pairs of function pointers),
// used by the interposition table below.

typedef struct interpose_s {
  void *new_func;
  void *orig_func;
} interpose_t;

// A convenience macro for defining pairs of interpositions, replacing
// the original by the "mac-wrapped" version.

#define INTERPOSE_ON(x) { (void *) MACWRAPPER_PREFIX(x), (void *) x }

// Now interpose everything, putting them in the appropriate linker
// section.

__attribute__((used))
static const interpose_t interposers[]	\
__attribute__ ((section("__DATA, __interpose"))) = {
  INTERPOSE_ON(malloc),
  INTERPOSE_ON(valloc),
  INTERPOSE_ON(free),
  INTERPOSE_ON(realloc),
  INTERPOSE_ON(reallocf),
  INTERPOSE_ON(calloc),
  INTERPOSE_ON(malloc_good_size),
  INTERPOSE_ON(strdup),
  INTERPOSE_ON(posix_memalign),
  INTERPOSE_ON(malloc_create_zone),
  INTERPOSE_ON(malloc_default_zone),
  INTERPOSE_ON(malloc_destroy_zone),
  INTERPOSE_ON(malloc_zone_batch_malloc),
  INTERPOSE_ON(malloc_zone_batch_free),
  INTERPOSE_ON(malloc_zone_check),
  INTERPOSE_ON(malloc_zone_print),
  INTERPOSE_ON(malloc_zone_log),
  INTERPOSE_ON(malloc_set_zone_name),
  INTERPOSE_ON(malloc_zone_from_ptr),
  INTERPOSE_ON(malloc_zone_malloc),
  INTERPOSE_ON(malloc_zone_calloc),
  INTERPOSE_ON(malloc_zone_valloc),
  INTERPOSE_ON(malloc_zone_memalign),
  INTERPOSE_ON(malloc_zone_realloc),
  INTERPOSE_ON(malloc_zone_free),
  INTERPOSE_ON(malloc_zone_register),
  INTERPOSE_ON(malloc_zone_unregister),
  INTERPOSE_ON(malloc_jumpstart),
  INTERPOSE_ON(malloc_get_zone_name),
  INTERPOSE_ON(_malloc_fork_prepare),
  INTERPOSE_ON(_malloc_fork_parent),
  INTERPOSE_ON(_malloc_fork_child),

  { (void *) MACWRAPPER_PREFIX(free),     (void *) vfree },
  { (void *) MACWRAPPER_PREFIX(malloc_usable_size), (void *) malloc_size},
  { (void *) MACWRAPPER_PREFIX(malloc),   (void *) _Znwm },
  { (void *) MACWRAPPER_PREFIX(malloc),   (void *) _Znam },

  { (void *) MACWRAPPER_PREFIX(malloc),   (void *) _ZnwmRKSt9nothrow_t },
  { (void *) MACWRAPPER_PREFIX(malloc),   (void *) _ZnamRKSt9nothrow_t },

#if 0 // 32-bit only
  { (void *) MACWRAPPER_PREFIX(malloc),   (void *) _Znaj },
  { (void *) MACWRAPPER_PREFIX(malloc),   (void *) _Znwj },
  { (void *) MACWRAPPER_PREFIX(malloc),   (void *) _ZnwjRKSt9nothrow_t },
  { (void *) MACWRAPPER_PREFIX(malloc),   (void *) _ZnajRKSt9nothrow_t },
#endif

  { (void *) MACWRAPPER_PREFIX(free),     (void *) _ZdlPv },
  { (void *) MACWRAPPER_PREFIX(free),     (void *) _ZdaPv },
  { (void *) MACWRAPPER_PREFIX(free),     (void *) _ZdaPvRKSt9nothrow_t }
};


/*
  not implemented, from libgmalloc:

__interpose_malloc_freezedry
__interpose_malloc_get_all_zones
__interpose_malloc_printf
__interpose_malloc_zone_print_ptr_info

also:

  malloc_zone_statistics
  malloc_zone_log

*/

// A class to initialize exactly one malloc zone with the calls used
// by our replacement.

class initializeDefaultZone {
public:
  initializeDefaultZone() {

    // Set up console logging.
    //    openlog("DieHarder", LOG_PID | LOG_NDELAY | LOG_PERROR | LOG_CONS, LOG_USER);

    theDefaultZone.malloc  = MACWRAPPER_PREFIX(malloc_zone_malloc);
    theDefaultZone.calloc  = MACWRAPPER_PREFIX(malloc_zone_calloc);
    theDefaultZone.valloc  = MACWRAPPER_PREFIX(malloc_zone_valloc);
    theDefaultZone.free    = MACWRAPPER_PREFIX(malloc_zone_free);
    theDefaultZone.realloc = MACWRAPPER_PREFIX(malloc_zone_realloc);
    theDefaultZone.size    = MACWRAPPER_PREFIX(internal_malloc_zone_size);
    theDefaultZone.zone_name = "OneTrueZone";
    theDefaultZone.batch_malloc = NULL;
    theDefaultZone.batch_free   = NULL;
    theDefaultZone.introspect   = NULL;
    theDefaultZone.memalign     = NULL;
    theDefaultZone.free_definite_size = NULL;
  }
};

// Force initialization of the default zone.

static initializeDefaultZone initMe;


