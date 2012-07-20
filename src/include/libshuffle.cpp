/*

to compile:

% clang++ -I../Heap-Layers -Imath -Istatic -Irng -fPIC -shared -g3 libshuffle.cpp ../Heap-Layers/wrappers/gnuwrapper.cpp -o libshuffle.so ~/git/Malloc-Implementations/allocators/TLSF/TLSF-2.4.6/src/tlsf.o -lpthread

*/


#include <stdio.h>

#include <iostream>
using namespace std;

#include "shuffleheap.h"
#include "heaplayers.h"

extern "C" {
  void * tlsf_malloc (size_t);
  void   tlsf_free (void *);
  void * tlsf_memalign (size_t, size_t);
  size_t tlsf_get_object_size (void *);
  void   tlsf_activate();
}

class TLSFHeap {
public:

  TLSFHeap() {
    tlsf_activate();
  }

  enum { Alignment = 16 }; // I assume.

  void * malloc (size_t sz) {
    return tlsf_malloc (sz);
  }
  void free (void * ptr) {
    tlsf_free (ptr);
  }
  size_t getSize (void * ptr) {
    return tlsf_get_object_size (ptr);
  }
  void * memalign (size_t alignment, size_t sz) {
    return tlsf_memalign (alignment, sz);
  }

};


class MemSource : public OneHeap<BumpAlloc<65536, MmapHeap, 16> > {
public:
  enum { Alignment = 16 };
};

#if 0 // USE HEAP LAYERS HEAP

class Shuffler :
  public ANSIWrapper<
  LockedHeap<PosixLockType,
	     KingsleyHeap<
	       //	       ShuffleHeap<2,
	       SizeHeap<FreelistHeap<MemSource>
			     //			     >
			   >,
	       SizeHeap<MemSource> // mapHeap>
	       
	       > > > {};


#else // USE TLSF

class Shuffler :
  public ANSIWrapper<
  LockedHeap<PosixLockType,
	     KingsleyHeap<
	       ShuffleHeap<16, TLSFHeap>,
	       // TLSFHeap,
	       TLSFHeap > > > {};

#endif

class TheCustomHeapType : public LockedHeap<PosixLockType, TLSFHeap> {}; // Shuffler {};

inline static TheCustomHeapType * getCustomHeap (void) {
  static char buf[sizeof(TheCustomHeapType)];
  static TheCustomHeapType * _theCustomHeap = 
    new (buf) TheCustomHeapType;
  return _theCustomHeap;
}

#if defined(_WIN32)
#pragma warning(disable:4273)
#endif

extern "C" {

  void * xxmalloc (size_t sz) {
    return getCustomHeap()->malloc (sz);
  }

  void xxfree (void * ptr) {
    getCustomHeap()->free (ptr);
  }

  size_t xxmalloc_usable_size (void * ptr) {
    return getCustomHeap()->getSize (ptr);
  }

  void xxmalloc_lock() {
    getCustomHeap()->lock();
  }

#if 0
  void * xxmemalign (size_t alignment, size_t size) {
    return getCustomHeap()->memalign (alignment, size);
  }
#endif

  void xxmalloc_unlock() {
    getCustomHeap()->unlock();
  }

}

