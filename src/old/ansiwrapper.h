/* -*- C++ -*- */

#ifndef _ANSIWRAPPER_H_
#define _ANSIWRAPPER_H_

#include <stdlib.h>

/*
 * @class ANSIWrapper
 * @brief Provide ANSI C behavior for malloc & free.
 *
 * Implements all prescribed ANSI behavior, including zero-sized
 * requests & aligned request sizes to a double word (or long word).
 */

template <class SuperHeap>
class ANSIWrapper : public SuperHeap {
public:
  
  inline void * malloc (size_t sz) {
    if (sz < sizeof(double)) {
      sz = sizeof(double);
    }
    return SuperHeap::malloc (sz);
  }
 
  inline bool free (void * ptr) {
    if (ptr != 0) {
      return SuperHeap::free (ptr);
    } else {
      return true;
    }
  }

  inline size_t getSize (void * ptr) {
    if (ptr) {
      return SuperHeap::getSize (ptr);
    } else {
      return 0;
    }
  }

};

#endif
