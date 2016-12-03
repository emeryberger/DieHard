/* -*- C++ -*- */

#ifndef __QIH_COLDCACHE_H__
#define __QIH_COLDCACHE_H__

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "internalheap.h"
#include "log.h"
#include "mman.h"
#include "myassert.h"
#include "pagedescription.h"
#include "sparseheaps.h"
#include "util.h"

namespace QIH {

  class ColdCache {
  private:

    InternalDataHeap heap; //heap for allocating cold storage

    //map to store hot-to-cold mappings
    HL::MyHashMap<void*, PageDescription*, InternalDataHeap> pageMap; 

//     inline size_t getObjectSize(PageDescription *pd) {
//       //last word on the page
//       size_t *curWord = (size_t *)((size_t)pd->getRealPage() + QIH_PAGE_SIZE - sizeof(size_t)); 

//       while ((curWord >= pd->getRealAddress()) && (!*curWord)) curWord--; //walk until start or non-zero
      
//       if ((curWord >= pd->getRealAddress()) && *curWord) {
// 	//found non-zero word
// 	size_t new_size = (size_t)curWord + sizeof(size_t) - (size_t)pd->getRealAddress();

// 	return new_size;

//       } else {
// 	//page is zero up to start
// 	logWrite(INFO, "Page %p is empty up to the object start of %p\n", pd->getRealPage(), pd->getRealAddress());
// 	return 0;
//       }
//     }
    
  public:

    inline static ColdCache *getInstance() {
      static char buf[sizeof(ColdCache)];
      
      static ColdCache *instance = new (buf) ColdCache();
      
      return instance;
    }

    
    //Compresses the page
    inline void deflatePage(PageDescription *pd) {

      if (!pd) return;

      ASSERT_LOG(pageMap.get(pd->getRealPage()) == NULL, "Compressing page %p twice!\n", pd->getRealPage());

      pd->updateSize(/*getObjectSize(pd)*/); //size of the actual object on the page
      
      void *newLoc = heap.malloc(pd->getSize()); //allocate memory in the cold cache

      //check for out-of-memory errors
      ASSERT_LOG(newLoc != NULL, "Failed to allocate %d bytes to compress %p\n", pd->getSize(), pd->getRealPage());
      pd->setAddressInCache(newLoc);
           
      pageMap.set(pd->getRealPage(), pd); //save the description
      
      memcpy(pd->getAddressInCache(), pd->getRealAddress(), pd->getSize()); //copy out the object
    
      MsetTrap(pd->getRealPage(), QIH_PAGE_SIZE); //protect

      Mdiscard(pd->getRealPage(), QIH_PAGE_SIZE); //dispose

      pd->setEvicted(true); //mark evicted

      logWrite(DEBUG, "%d: compressed page %p to %p\n", 
	       getAllocationClock(), pd->getRealPage(), pd->getAddressInCache());		      
    }

    //uncompresses the page
    inline bool inflatePage(void *page) {
      
      PageDescription *pageDesc = pageMap.get(page); //get the page description
      
      if (!pageDesc) return false; //no valid description found for this object

      MremoveTrap(pageDesc->getRealPage(), QIH_PAGE_SIZE); //allow access to the page

      //copy the object back
      memcpy(pageDesc->getRealAddress(), pageDesc->getAddressInCache(), pageDesc->getSize());
  
      heap.free(pageDesc->getAddressInCache()); //free the cold copy
      
      pageMap.erase(page); //remove page from the map
    
      logWrite(DEBUG, "%d: uncompressed page %p from %p\n", 
	       getAllocationClock(), page, pageDesc->getAddressInCache());

      pageDesc->setEvicted(false); //no longer evicted

      return true;
      
    }

    //checks if the page exists in the mapping
    inline PageDescription *getDeflatedPage(void *page){
      return pageMap.get(page);
    }

    //purges deleted pages (moves them into FreedCache in the future)
    inline void deletePage(void *page) {
      PageDescription *pageDesc = pageMap.get(page); //get the page description
    
      deletePage(pageDesc);
    }

    inline void deletePage(PageDescription *pageDesc) {
      if (!pageDesc) //page was not found
	return;

      heap.free(pageDesc->getAddressInCache()); //free the cold copy
      
      pageMap.erase(pageDesc->getRealPage()); //remove page from the map
    
      MremoveTrap(pageDesc->getRealPage(), QIH_PAGE_SIZE);

      logWrite(DEBUG, "%d: Removed contents of page %p from cold storage.\n", 
	       getAllocationClock(), pageDesc->getRealPage());
    }

  };

}

#endif
