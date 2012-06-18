// -*- C++ -*-

#ifndef _REALMALLOCHEAP_H_
#define _REALMALLOCHEAP_H_

#include <dlfcn.h>
#include <stdio.h>

// Declarations for RealMallocHeap, below.
extern "C" {
  size_t malloc_usable_size (void *);
  typedef void * (*malloc_function_type) (size_t);
  typedef size_t (*malloc_usable_size_function_type) (void *);
  typedef void * (*free_function_type) (void *);
}

class RealMallocHeap {
public:

  RealMallocHeap (void)
    : real_malloc (0),
      real_malloc_usable_size (0),
      real_free (0),
      initializing (false),
      bufptr ((char *) buf)
  {}
  
  void * malloc (size_t sz) {
    if (initializing) {
      char * p = bufptr;
      bufptr += align (sz);
      return p;
    }
    if (real_malloc == 0) {
      initializing = true;
      real_malloc = (malloc_function_type) dlsym (RTLD_NEXT, "malloc");
      initializing = false;
    }
    return (*real_malloc)(sz);
  }

  void free (void * ptr) {
#if 0
      char buf[255];
      sprintf (buf, "free call to %x\n", ptr);
      fprintf (stderr, buf);
#endif
    if (real_free == 0) {
      real_free = (free_function_type) dlsym (RTLD_NEXT, "free");
    }
    (*real_free)(ptr);
  }
  
  inline size_t getSize (void * ptr) {
#if 0
      char buf[255];
      sprintf (buf, "getSize of %x\n", ptr);
      fprintf (stderr, buf);
#endif
    if (real_malloc_usable_size == 0) {
      real_malloc_usable_size = (malloc_usable_size_function_type) dlsym (RTLD_NEXT, "malloc_usable_size");
      if (real_malloc_usable_size == 0) {
	fprintf (stderr, "Could not find the malloc_usable_size function, which means you aren't using GNU libc.\n");
	abort();
      }
    }
    return (*real_malloc_usable_size)(ptr);
  }
  
private:
  inline static size_t align (size_t sz) {
    return (sz + (sizeof(double) - 1)) & ~(sizeof(double) - 1);
  }

  malloc_function_type real_malloc;
  malloc_usable_size_function_type real_malloc_usable_size;
  free_function_type real_free;
  bool initializing;
  char * bufptr;
  char buf[32768];
};

#endif
