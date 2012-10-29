/* -*- C++ -*- */

#ifndef __QIH_FIXEDACTIVEHEAP_H__
#define __QIH_FIXEDACTIVEHEAP_H__

#include <sys/types.h>

#include "coldcache.h"
#include "livepagetracker.h"
#include "pagedescription.h"

namespace QIH {

  template <class SuperHeap>
  class FixedActiveHeap : public SuperHeap {
  private:
    
    typedef LivePageTracker<ColdCache> LiveTrackerType;

    LiveTrackerType *liveTracker;

    typedef ColdCache CacheType;
    
    CacheType *cache;

    void destroyDescriptions(PageDescription *pd) {
      PageDescription *next;
      
      while (pd != NULL) {

#ifdef DETECT_OVERFLOWS
	pd->updateSize();
#endif

	logWrite(DEBUG, "%d: deregistered page %p with object %p of size %d\n",
		 getAllocationClock(), 
		 pd->getRealPage(), pd->getRealAddress(), pd->getSize());

	next = pd->getNextDescription();

	if (pd->isEvicted()) {
	  
	  cache->deletePage(pd); //delete page from cold cache
	}
	
	delete pd; //delete the description

	pd = next;
      }
    }
  public:
    
    FixedActiveHeap() : 
      liveTracker(LiveTrackerType::getInstance()),
      cache(CacheType::getInstance()) 
    {   }

    inline void *malloc(size_t sz) {
      void *ptr = SuperHeap::malloc(sz); //allocate from the parent
      
      if (ptr)
 	liveTracker->registerObject(ptr, sz); //register pages
      
      return ptr; // return to the user
    }
    
    inline void free(void *ptr) {
      destroyDescriptions(liveTracker->unregisterObject(ptr)); 

      SuperHeap::free(ptr); //free at parent
    }
    
    inline void free(void *ptr, size_t sz) {
      free(ptr);
    }

    inline void destroy(void *ptr) {
      SuperHeap::free(ptr);
    }  

    inline bool tryRestorePage(void *ptr) {
      return liveTracker->tryRestorePage(ptr);
    }
  };
}
#endif
