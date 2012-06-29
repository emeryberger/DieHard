// -*- C++ -*-

/**
 * @file   mmapwrapper.h
 * @brief  Provides platform-independent interface to memory map functions.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */


#ifndef _MMAPWRAPPER_H_
#define _MMAPWRAPPER_H_

#if defined(_WIN32)
#include <windows.h>
#else
// UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#endif


#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

#if 0 // executable heap
#define MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE | PROT_EXEC)
#else
#define MMAP_PROTECTION_MASK (PROT_READ | PROT_WRITE)
#endif

class MmapWrapper {
public:

#if defined(_WIN32) 

#define MMAP_PROTECTION PAGE_READWRITE

  // Microsoft Windows has 4K pages aligned to a 64K boundary.
  enum { Size = 4 * 1024 };
  enum { Alignment = 64 * 1024 };

  static void * map (size_t sz) {
    void * ptr;
    ptr = VirtualAlloc (NULL, sz, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, MMAP_PROTECTION);
    return  ptr;
  }
  
  static void unmap (void * ptr, size_t) {
    VirtualFree (ptr, 0, MEM_RELEASE);
  }

  static void protect (void * ptr, size_t sz) {
    DWORD oldProtection;
    VirtualProtect (ptr, sz, PAGE_NOACCESS, &oldProtection);
  }

  static void unprotect (void * ptr, size_t sz) {
    DWORD oldProtection;
    VirtualProtect (ptr, sz, MMAP_PROTECTION, &oldProtection);
  }


#else

#if defined(__SVR4)
  // Solaris aligns 8K pages to a 64K boundary.
  enum { Size = 8 * 1024 };
  enum { Alignment = 64 * 1024 };
#else
  // Linux and most other operating systems align memory to a 4K boundary.
  enum { Size = 4 * 1024 };
  enum { Alignment = 4 * 1024 };
#endif

  static void * map (size_t sz) {

    if (sz == 0) {
      return NULL;
    }

    void * ptr;

#if 0
    char buf[255];
    // FIX ME
    sprintf (buf, "map %d\n", sz);
    fprintf (stderr, buf);
#endif

#if defined(MAP_ALIGN) && defined(MAP_ANON)
    // Request memory aligned to the Alignment value above.
    ptr = mmap ((char *) Alignment, sz, MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ALIGN | MAP_ANON, -1, 0);
#elif !defined(MAP_ANONYMOUS)
    static int fd = ::open ("/dev/zero", O_RDWR);
    ptr = mmap (NULL, sz, MMAP_PROTECTION_MASK, MAP_PRIVATE, fd, 0);
#else
    ptr = mmap (NULL, sz, MMAP_PROTECTION_MASK, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

    if (ptr == MAP_FAILED) {
      return NULL;
    } else {
      //fprintf(stderr, "mapped chunk at addr 0x%x, size 0x%x\n",ptr,sz);
      return ptr;
    }
  }

  static void unmap (void * ptr, size_t sz) {
    munmap (reinterpret_cast<char *>(ptr), sz);
  }

  static void protect (void * ptr, size_t sz) {
    mprotect ((char *) ptr, sz, PROT_NONE);
  }

  static void unprotect (void * ptr, size_t sz) {
    mprotect ((char *) ptr, sz, MMAP_PROTECTION_MASK);
  }

   
#endif

};

#endif
