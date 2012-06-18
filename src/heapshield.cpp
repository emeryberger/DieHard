/**
 *
 * HeapShield
 *
 * prevents library-based heap overflow attacks
 * for any memory allocator that maintains object size
 * GIVEN AN INTERIOR POINTER.
 *
 * Copyright (C) 2006 Emery Berger, University of Massachusetts Amherst
 *
 *
 **/

#if defined(_WIN32)
//#error "This file does not work for Windows. Sorry."
#endif

#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#ifndef CUSTOM_PREFIX
#define CUSTOM_PREFIX(x) x
#endif

#undef memcpy
#undef strcpy

inline static 
bool onStack (void * ptr) {
  int a;
  int b;
  if ((size_t) &b > (size_t) &a) {
    // Stack grows up.
    return ((size_t) ptr < (size_t) &a);
  } else {
    // Stack grows down.
    return ((size_t) ptr > (size_t) &b);
  }
}


extern "C" {
  typedef char * strcat_function_type (char *, const char *);
  typedef char * strncat_function_type (char *, const char *, size_t);
  typedef char * strcpy_function_type (char *, const char *);
  typedef char * strncpy_function_type (char *, const char *, size_t);
  typedef void * memcpy_function_type (void *, const void *, size_t);
  typedef size_t fread_function_type (void *, size_t, size_t, FILE *);
  size_t CUSTOM_PREFIX(malloc_usable_size) (void * ptr);
}

#if !defined(_WIN32)
static strcat_function_type * my_strcat_fn;
static strncat_function_type * my_strncat_fn;
static strcpy_function_type * my_strcpy_fn;
static strncpy_function_type * my_strncpy_fn;
static memcpy_function_type * my_memcpy_fn;
static fread_function_type * my_fread_fn;
#endif

class Initialize {
public:
  Initialize (void)
  {
    initialize();
  }
private:

  static void initialize (void) {
    static bool initialized = false;
    if (!initialized) {
#if !defined(_WIN32)
      my_strcat_fn = (strcat_function_type *) dlsym (RTLD_NEXT, "strcat");
      if (!my_strcat_fn)
	fprintf (stderr, "Error dynamically loading strcat not found.\n");

      my_strncat_fn = (strncat_function_type *) dlsym (RTLD_NEXT, "strncat");
      if (!my_strncat_fn)
	fprintf (stderr, "Error dynamically loading strncat not found.\n");
	
      my_strcpy_fn = (strcpy_function_type *) dlsym (RTLD_NEXT, "strcpy");
      if (!my_strcpy_fn)
	fprintf (stderr, "Error dynamically loading strcpy\n");
	
      my_strncpy_fn = (strncpy_function_type *) dlsym (RTLD_NEXT, "strncpy");
      if (!my_strncpy_fn)
	fprintf (stderr, "Error dynamically loading strncpy\n");
	
      my_memcpy_fn = (memcpy_function_type *) dlsym (RTLD_NEXT, "memcpy");
      if (!my_memcpy_fn)
	fprintf (stderr, "Error dynamically loading memcpy\n");
	
      my_fread_fn = (fread_function_type *) dlsym (RTLD_NEXT, "fread");
      if (!my_fread_fn)
	fprintf (stderr, "Error dynamically loading fread\n");
#endif
      initialized = true;
    }
  }
};


static size_t local_strlen (const char * str)
{
  int len = 0;
  char * ch = (char *) str;
  size_t maxLen;
  if (onStack((void *) str)) {
    maxLen = UINT_MAX;
  } else {
    maxLen = local_usable_size ((void *) str);
  }
  while (*ch != '\0') {
    len++;
    ch++;
  }
  if (len > maxLen) {
    len = maxLen;
  }
  return len;
}


static char * local_strcat (char * dest, const char * src) 
{
  size_t d = local_strlen (dest);
  char * dptr = dest + d;
  size_t s = local_strlen (src);
  char * sptr = (char *) src;
  for (int i = 0; i <= s; i++) {
    *dptr++ = *sptr++;
  }
  return dest;
}

static char * local_strncat (char * dest, const char * src, size_t sz)
{
  size_t d = local_strlen (dest);
  char * dptr = dest + d;
  size_t s = local_strlen (src);
  char * sptr = (char *) src;
  for (int i = 0; i <= s && i < sz; i++) {
    *dptr++ = *sptr++;
  }
  // Add a trailing nul.
  *dptr = '\0';
  return dest;
}

static char * local_strncpy (char * dest, const char * src, size_t n)
{
  char * sptr = (char *) src;
  char * dptr = dest;
  int destLength = local_usable_size (dest) - 1;
  int lengthToCopy = (n < destLength) ? n : destLength;
  while (lengthToCopy && (*dptr++ = *sptr++)) {
    lengthToCopy--;
  }
  *dptr = '\0';
  return dest;
}


static char * local_strcpy (char * dest, const char * src) 
{
  return local_strncpy (dest, src, INT_MAX);
}

extern "C" void * __cdecl local_strdup (const char * s) {
  size_t len = local_strlen((char *) s);
  char * n = (char *) local_malloc (len + 1);
  if (n) {
    local_strncpy (n, s, len);
  }
  return n;
}


#if !defined(_WIN32)

extern "C" {

  static void * local_memcpy (void * dest, const void * src, size_t n)
  {
    Initialize me;
    return (*my_memcpy_fn) (dest, src, n);
  }

  size_t fread (void * ptr, size_t size, size_t nmemb, FILE * stream)
  {
    size_t sz = CUSTOM_PREFIX(malloc_usable_size) (ptr);
    Initialize me;
    if (sz == -1) {
      return (*my_fread_fn)(ptr, size, nmemb, stream);
    } else {
      size_t total = size * nmemb;
      size_t s = (total < sz) ? total : sz;
      if (sz < total) {
	// Overflow.
	//fprintf (stderr, "Overflow detected in fread!\n");
      }
      return (*my_fread_fn)(ptr, 1, s, stream);
    }
  }

  void * memcpy (void * dest, const void * src, size_t n) {
    size_t sz;
    if (onStack(dest)) {
      sz = n;
    } else {
      sz = CUSTOM_PREFIX(malloc_usable_size) (dest);
      if (sz == -1) {
	sz = n;
      } else {
	size_t s = (n < sz) ? n : sz;
	if (sz < n) {
#if 0
	  // Overflow.
	  fprintf (stderr, "Overflow detected in memcpy: dest (%x) size = %d, n = %d\n", dest, sz, n);
#endif
	}
	sz = s;
      }
    }
    return local_memcpy (dest, src, sz);
  }

  int sprintf (char * str, const char * format, ...) {
    va_list ap;
    va_start (ap, format);
    if (onStack(str)) {
      int r = vsprintf (str, format, ap);
      va_end (ap);
      return r;
    }
    else {
      size_t sz = CUSTOM_PREFIX(malloc_usable_size) (str);
      if (sz == -1) {
	// Not from here.
	int r = vsprintf (str, format, ap);
	va_end (ap);
	return r;
      } else {
	int r = vsnprintf (str, sz, format, ap);
	va_end (ap);
	return r;
      }
    }
  }

  int snprintf (char * str, size_t n, const char * format, ...) {
    va_list ap;
    va_start (ap, format);
    if (onStack(str)) {
      int r = vsnprintf (str, n, format, ap);
      va_end (ap);
      return r;
    }
    size_t sz = CUSTOM_PREFIX(malloc_usable_size) (str);
    if (sz == -1) {
      int r = vsnprintf (str, n, format, ap);
      va_end (ap);
      return r;
    }
    size_t s = (n < sz) ? n : sz;
    int r = vsnprintf (str, s, format, ap);
    va_end (ap);
    return r;
  }

  char * gets (char * s) {
    size_t sz;
    if (onStack(s)) {
      sz = INT_MAX;
    } else {
      sz = CUSTOM_PREFIX(malloc_usable_size)(s);
    }
    return fgets (s, sz, stdin);
  }

  char * strcpy (char * dest, const char * src) {
    if (onStack(dest)) {
      return local_strcpy (dest, src);
    }
    Initialize me;
    size_t sz = CUSTOM_PREFIX(malloc_usable_size) (dest);
    if (sz == -1) {
      return local_strcpy (dest, src);
    } else {
      // DieFast: check for an error (would we have overflowed?)
#if 0
      if (sz < strlen(src) + 1) {
	fprintf (stderr, "Overflow detected in strcpy! dest (%x) size = %d, src (%x) length = %d (%s)\n", dest, sz, src, strlen(src), src); 
      }
#endif
      return local_strncpy (dest, src, sz);
    }
  }

  char * strncpy (char * dest, const char * src, size_t n) {
    if (onStack(dest)) {
      return local_strncpy (dest, src, n);
    }
    Initialize me;
    size_t sz = CUSTOM_PREFIX(malloc_usable_size) (dest);
    if (sz == -1) {
      return local_strncpy (dest, src, n);
    } else {
      size_t s = (n < sz) ? n : sz;
      // DieFast: check for an error (would we have overflowed?)
      if (n > s) {
	// fprintf (stderr, "Overflow detected in strncpy!\n");
      }
      return local_strncpy (dest, src, s);
    }
  }

  char * strcat (char * dest, const char * src) {
    Initialize me;
    if (onStack(dest)) {
      return (*my_strcat_fn) (dest, src);
    }
    size_t sz = CUSTOM_PREFIX(malloc_usable_size) (dest + strlen(dest));
    if (sz == -1) {
      return (*my_strcat_fn) (dest, src);
    } else {
      // DieFast: check for an error (would we have overflowed?)
      //if (strlen(src) + strlen(dest) > sz) {
	//	fprintf (stderr, "Overflow detected in strcat!\n");
      //}
      return (*my_strncat_fn) (dest, src, sz);
    }
  }

  char * strncat (char * dest, const char * src, size_t n) {
    Initialize me;
    if (onStack(dest)) {
      return (*my_strncat_fn) (dest, src, n);
    }      
    size_t sz = CUSTOM_PREFIX(malloc_usable_size) (dest + strlen(dest));
    if (sz == -1) {
      return (*my_strncat_fn) (dest, src, n);
    } else {
      size_t s = (n < (sz-1)) ? n : (sz-1);
#if 0
      // DieFast: check for an error (would we have overflowed?)
      if (strlen(dest) + s > sz) {
	// fprintf (stderr, "Overflow detected in strncat!\n");
      }
#endif
      return (*my_strncat_fn) (dest, src, s);
    }
  }


}
#endif

