/* -*- C++ -*- */

#ifndef __QIH_DIEHARDNIH_H__
#define __QIH_DIEHARDNIH_H__

#include <sys/types.h>

#include "pagedescription.h"

namespace QIH {

  extern unsigned int getAllocationClock();

  extern bool tryRestorePage(void *);

  extern void reclaimObject(PageDescription*); 

#ifndef OPP_ONLY_HEAP  
  extern bool pageUnused(void*);
#if defined(VMSIG) //|| defined(IV)
  extern int getVictimBuffer(void **buf);

  extern void clearVictimBuffer();
#endif
#endif
}

#endif
