// -*- C++ -*-

/**
 * @file   mmapalloc.h
 * @brief  Obtains memory from Mmap but doesn't allow it to be freed.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef _MMAPALLOC_H_
#define _MMAPALLOC_H_

#include "mmapwrapper.h"

/**
 * @class MmapAlloc
 * @brief Obtains memory from Mmap but doesn't allow it to be freed.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

class MmapAlloc {
public:
  void * malloc (size_t sz) {
    void * ptr = MmapWrapper::map (sz);
    return ptr;
  }
  void free (void *) {}
};

#endif
