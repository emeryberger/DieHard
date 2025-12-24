/*

to compile:

% clang++ -I../Heap-Layers -Imath -Istatic -Irng -fPIC -shared -g3 libshuffle.cpp ../Heap-Layers/wrappers/gnuwrapper.cpp -o libshuffle.so ~/git/Malloc-Implementations/allocators/TLSF/TLSF-2.4.6/src/tlsf.o -lpthread

*/


#include <stdio.h>

#include <iostream>
using namespace std;

#include <heaplayers>

#include "shuffleheap.h"
#include "tlsfheap.h"

class MemSource : public OneHeap<BumpAlloc<65536, MmapHeap, 16> > {
public:
  enum { Alignment = 16 };
};


#if 1 // USE HEAP LAYERS HEAP

class TopHeap : public SizeHeap<UniqueHeap<ZoneHeap<MmapHeap, 65536> > > {};

// Shuffled freelist heap.
class Shuffler :
  public ANSIWrapper<
  LockedHeap<PosixLockType,
	     ShuffleHeap<4096,
			 2048,
			 KingsleyHeap<AdaptHeap<DLList, TopHeap>, TopHeap>
			 >
	       > > {};


#endif

#if 0

// Shuffled TLSF heap
class Shuffler :
  public ANSIWrapper<
  LockedHeap<PosixLockType,
	     KingsleyHeap<
	       ShuffleHeap<256, TLSFHeap>,
	       // TLSFHeap,
	       TLSFHeap > > > {};

#endif

#if 0
// Straight-up TLSF
class Shuffler : public LockedHeap<PosixLockType, TLSFHeap> {};
#endif

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

char *itob(unsigned long x)
{
  static char buff[sizeof(unsigned long) * 8 + 2];
  int i;
  unsigned long j = sizeof(unsigned long) * 8 - 1;
  
  // NUL terminator.
  buff[sizeof(unsigned long) * 8] = 0;

  for(i=0;i<sizeof(unsigned long) * 8; i++)
    {
      if (x & 1) {
	buff[j] = '1';
      } else {
	buff[j] = '0';
      }
      x >>= 1;
      j--;
    }
  return buff;
}

class TheCustomHeapType : public Shuffler {
public:
  TheCustomHeapType() {
#if 0
    char buf[255];
    sprintf (buf, "output-%d", getpid());
    fd = open (buf, O_CREAT | O_RDWR | O_APPEND, S_IRWXU);
#endif
  }
  void * malloc (size_t sz) {
    return Shuffler::malloc(sz);
    static int allocs = 0;
    ++allocs;

    void * ptr = Shuffler::malloc (sz);

    unsigned long val = ((size_t) ptr >> LO_INSIG_BITS);

    char * b = itob(val);
    char buf[255];
    sprintf (buf, "%s\n", b);
    char * p = (char *) &buf[(sizeof(unsigned long) * 8) - HI_SIG_BITS];
    write (fd, p, strlen(p));

    if (allocs == 1000) {
      sync();
      allocs = 0;
    }
    return ptr;
  }
private:    
  // 64-byte blocks. (2^6 = 64)
  enum { LO_INSIG_BITS = 6 }; 

  // 4096 sets. (2^12 = 4096)
  enum { HI_SIG_BITS = 12 };
  int fd;
};

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

int main()
{
  for (auto i = 0; i < 10; i++) {
    auto ptr = xxmalloc(32);
    std::cout << ptr << std::endl;
    xxfree(ptr);
  }
  return 0;
}

