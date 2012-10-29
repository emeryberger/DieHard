#include <sys/types.h>


#include "objectperpageheap.h"
#include "pagedescription.h"
#include "sparseheaps.h"

namespace QIH {

  unsigned int getAllocationClock() {
    return ObjectPerPageHeap::getInstance()->getAllocationClock();
  }

  bool tryRestorePage(void *ptr) {
#ifdef OPP_ONLY_HEAP
    return false;
#else
    return ObjectPerPageHeap::getInstance()->tryRestorePage(ptr);
#endif
  }

  void reclaimObject(PageDescription *pd) {
    ObjectPerPageHeap::getInstance()->reclaim(pd);
  }
}

/*
void  *TheCustomHeapType::malloc(size_t sz) {
  return QIH::ObjectPerPageHeap::getInstance()->malloc(sz);
}

void   TheCustomHeapType::free(void *ptr) {
  QIH::ObjectPerPageHeap::getInstance()->free(ptr);
}

size_t TheCustomHeapType::getSize(void *ptr) {
  return QIH::ObjectPerPageHeap::getInstance()->getSize(ptr);
}

*/
