// -*- C++ -*-

#ifndef DH_TLSFHEAP_H
#define DH_TLSFHEAP_H

#include <heaplayers>

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

  enum { Alignment = MallocInfo::Alignment };

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

#endif
