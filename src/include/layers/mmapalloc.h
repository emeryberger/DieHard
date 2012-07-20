// -*- C++ -*-

#ifndef DH_MMAPALLOC_H
#define DH_MMAPALLOC_H

#include <stdio.h>

#include "heaplayers.h"

/**
 * @class MmapAlloc
 * @brief Obtains memory from Mmap but doesn't allow it to be freed.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

class MmapAlloc {
public:

  enum { Alignment = HL::MmapWrapper::Alignment };

  virtual ~MmapAlloc (void) {}

  void * malloc (size_t sz) {
    void * ptr = HL::MmapWrapper::map (sz);
    return ptr;
  }

};

#endif
