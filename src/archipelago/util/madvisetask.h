// -*- C++ -*-

#ifndef __QIH_MADVISEDONTNEEDTASK_H__
#define __QIH_MADVISEDONTNEEDTASK_H__

#include <unistd.h>
#include <sys/mman.h>

#include "futuretaskrunner.h"
#include "internalheap.h"

namespace QIH {

  class MadviseTask : public SafeHeapObject {
   private:
     void *addr;
     size_t size;
     int advice;
   public:
     MadviseTask(void *ptr, size_t sz, int a) : addr(ptr), size(sz), advice(a) {}
     inline void perform(void) {
       madvise(addr, size, advice);
     }
   };

}

#endif
