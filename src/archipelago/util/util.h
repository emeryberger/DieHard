// -*- C++ -*-

#ifndef __QIH_UTIL_H__
#define __QIH_UTIL_H__

#include <sys/types.h>
#include "processor.h"

namespace QIH {

  extern size_t QIH_PAGE_SIZE;
  
  /*
   * Different coloring functions. Set the define to point to one of them
   */
  extern void *getRandomColoring(void *page, size_t sz);
  
  extern void *getPageRandomColoring(void *page, size_t sz);
  
  extern void *getWraparoundColoring(void *page, size_t sz);
  
  extern void *getWraparoundColoringEx(void *page, size_t sz);
  
  extern size_t getActualSize(void *ptr, size_t hint);

  extern const char* getLastError();
}


/*
 * Default coloring, can be overriden with compier -D flag
 */
#ifndef getObjectColoring
#define getObjectColoring getRandomColoring    
#endif

/*
 * Helper function that mask off low-order bits to get page address
 */
#define getPageStart(A) (void *)((size_t)(A) & PAGE_MASK)

#define MIN(A, B) (((A) < (B)) ? (A) : (B))
#define MAX(A, B) (((A) > (B)) ? (A) : (B))

#endif
