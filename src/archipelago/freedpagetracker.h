/* -*- C++ -*- */

#ifndef __QIH_FREEDPAGETRACKER_H__
#define __QIH_FREEDPAGETRACKER_H__

#include "log.h"
#include "util.h"
#include "myassert.h"
#include "livecache.h"
#include "myhashmap.h"
#include "sparseheaps.h"
#include "stlallocator.h"
#include "mydlmallocheap.h"
#include "pagedescription.h"

using namespace std;

namespace QIH {

  /*
   * Fifo Policy
   */
  template <class Cache, class Heap>
  class FreedPageTrackerFifo {
  private:
    PageQueue objects;
    
    unsigned int activeCount;

    Cache *cache;

    Heap *heap;

//     inline void evictObject() {
//       if (activeCount == 0) return; //no objects

//       PageDescription *next, *pd = objects.pop(); //get the front object
     
//       heap->destroy(pd->getRealAddress()); //free the memory with the heap

//       while (pd != NULL) { //for all descriptions
// 	next = pd->getNextDescription();

// 	if (pd->isEvicted()) //free cold storage
// 	  cache->deletePage(pd->getRealPage());
	
// 	if (pd->inQueue())
// 	  pd->markDestroyed(); //schedule for freeing
// 	else
// 	  delete pd; //free the description

// 	pd = next;
//       }

//       activeCount--;

//     }

    PageTrackerFifo() : activeCount(0) , cache(Cache::getInstance()), heap(Heap::getInstance()) {
    }
	
  public:
	
//     void registerObject(PageDescription *pd) { 
    
//       if ((sz == 0) || (!ptr)) return;

//       char *page = (char*)getPageStart(ptr);//first page
//       char *end  = (char*)ptr + sz; //byte after last

//       ASSERT_LOG(objectsToPages.get(ptr) == NULL, "Attempting to re-register %p\n", page);

//       PageDescription *pd = new PageDescription(ptr, MIN(sz, QIH_PAGE_SIZE)); //first page descriptor

//       objectsToPages.set(ptr, pd); //add to the map

//       reservePage();

//       activePages.push_back(pd); 

//       logWrite(DEBUG, "%d: registered page %p\n", getAllocationClock(), page);

//       page += QIH_PAGE_SIZE; //next page

//       while (page < end) {
// 	//last = pd;

// 	pd = new PageDescription(page, MIN(end - page, QIH_PAGE_SIZE), pd);

// 	//last->setNextDescription(pd); //link'em

// 	reservePage();

// 	activePages.push_back(pd); 
      
// 	logWrite(DEBUG, "%d: registered page %p\n", getAllocationClock(), page);

// 	page += QIH_PAGE_SIZE;
//       }
//     }
	
//     PageDescription *unregisterObject(void *ptr) {

//       PageDescription *page, *pd;

//       page = pd = objectsToPages.get(ptr); 
   
//       if (!pd) return NULL;

//       objectsToPages.erase(ptr); //erase from map

//       do { //mark all pages deleted
// 	pd->markDeleted();
      
// 	logWrite(DEBUG, "%d: unregistered page %p\n", 
// 		 getAllocationClock(), pd->getRealPage());

// 	pd = pd->getNextDescription();

// 	activeCount--; 
//       }	    
//       while(pd != NULL);
      
//       return page;
//     }

//     bool tryRestorePage(void *ptr) {
//       if (cache->inflatePage(ptr)){
// 	reservePage();
// 	return true;
//       } else
// 	return false;
//     }  

    static FreedPageTrackerFifo<Cache, Heap> *getInstance() {
      static char buf[sizeof(FreedPageTrackerFifo<size, Cache, Heap>)];
    
      static FreedPageTrackerFifo<size, Cache, Heap> *instance = new (buf) FreedPageTrackerFifo<size, Cache, Heap>();
    
      return instance;
    }
  };
}	

#endif
