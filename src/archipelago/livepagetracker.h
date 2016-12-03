/* -*- C++ -*- */

#ifndef __QIH_PAGETRACKER_H__
#define __QIH_PAGETRACKER_H__

#include "heaplayers.h"

#include "coldcache.h"
#include "log.h"
#include "myassert.h"
#include "internalheap.h"
#include "pagedescription.h"
#include "pagequeue.h"
#include "sparseheaps.h"
#include "util.h"

using namespace std;

namespace QIH {

  /*
   * Fifo Policy
   */
  template <class Cache>
  class LivePageTracker {
  private:
    PageQueue activePages;
    
    HL::MyHashMap<void*, PageDescription*, InternalDataHeap> objectsToPages;

    int MAX_SIZE;

    Cache *cache;

    inline void evictPage() {
      if (activePages.getSize() == 0) return; //no objects

      PageDescription *target = activePages.pop();

      cache->deflatePage(target);//compress
		
      logWrite(DEBUG, "%d: evicted page %p into the cold space\n", 
	       getAllocationClock(),  target->getRealPage());

    }

    inline void insertPage(PageDescription *pd) { 
      while (activePages.getSize() >= MAX_SIZE) //buffer full
      	evictPage(); //get more space
    
      activePages.push(pd);  //add to the queue
    }
	     
    LivePageTracker() :  
	MAX_SIZE(0), 
	cache(Cache::getInstance()) {

 	char *ps = getenv("HOT_SPACE_SIZE");
 	if (ps) 
 	  MAX_SIZE = atoi(ps);
 	else  
	  MAX_SIZE = HOT_SPACE_SIZE;

	
	stdoutWrite(INFO, "Hot space size is set to %d.\n", MAX_SIZE);

	logWrite(INFO, "Hot space size is set to %d.\n", MAX_SIZE);

    }
	
  public:
	
    inline void registerObject(void *ptr, size_t sz) { 
    
      if ((sz == 0) || (!ptr)) return;

      logWrite(DEBUG, "%d: Registering object %p of size %d.\n", getAllocationClock(), ptr, sz);

      char *page = (char*)getPageStart(ptr);//first page
      char *end  = (char*)ptr + sz; //byte after last

      ASSERT_LOG(objectsToPages.get(ptr) == NULL, "Attempting to re-register %p\n", page);

      size_t size = MIN(sz, QIH_PAGE_SIZE);

      PageDescription *pd = new PageDescription(ptr, size); //first page descriptor

      objectsToPages.set(ptr, pd); //add to the map

      insertPage(pd);

      logWrite(DEBUG, "%d: registered page %p with object %p of size %d\n", 
	       getAllocationClock(), page, ptr, size);

      page += QIH_PAGE_SIZE; //next page

      while (page < end) {

	size = MIN((size_t)(end - page), QIH_PAGE_SIZE);

	pd = new PageDescription(page, size, pd);

	insertPage(pd);

	logWrite(DEBUG, "%d: registered page %p with objects %p of size %d\n", 
		 getAllocationClock(), page, page, size);

	page += QIH_PAGE_SIZE;
      }
    }
	
    inline PageDescription *unregisterObject(void *ptr) {

      PageDescription *pd = objectsToPages.get(ptr); 
   
      if (!pd) return NULL;

      objectsToPages.erase(ptr); //erase from map

      activePages.removeObject(pd); //remove from the queue

      return pd;
    }

    inline bool tryRestorePage(void *ptr) {
      PageDescription *pd = cache->getDeflatedPage(ptr);
       
      if (pd && !pd->isDeleted()) {

 	cache->inflatePage(ptr);
     	
 	insertPage(pd);

      	return true;
      } else 
	return false;
    }  

    inline static LivePageTracker<Cache> *getInstance() {
      static char buf[sizeof(LivePageTracker<Cache>)];
    
      static LivePageTracker<Cache> *instance = new (buf) LivePageTracker<Cache>();
    
      return instance;
    }
  };
}	
#endif
