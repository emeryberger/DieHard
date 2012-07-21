/*

to compile:

(copy dlmalloc.c from Malloc-Implementations)

% clang -O3 -DNDEBUG -fPIC -DUSE_DL_PREFIX=1 -g -c dlmalloc.c 
% clang++ -O3 -DNDEBUG -g -I../Heap-Layers -Imath -Istatic -Irng -fPIC -shared -g3 libriff.cpp dlmalloc.o ../Heap-Layers/wrappers/gnuwrapper.cpp -o libriff.so -lpthread

*/


#include <dlfcn.h>
#include <stdio.h>

#include <iostream>
using namespace std;

#include "shuffleheap.h"
#include "heaplayers.h"

class Shuffler :
  public ANSIWrapper<
    LockedHeap<PosixLockType,
	     KingsleyHeap<
	       // LeaMallocHeap,
	       ShuffleHeap<16, LeaMallocHeap>,
	       LeaMallocHeap> > > {};

class TheCustomHeapType : public Shuffler {};
// class TheCustomHeapType : public ANSIWrapper<LockedHeap<PosixLockType, LeaMallocHeap> > {};

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

  void xxmalloc_unlock() {
    getCustomHeap()->unlock();
  }

}

