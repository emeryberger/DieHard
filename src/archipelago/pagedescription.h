/* -*- C++ -*- */

#ifndef __QIH_PAGEDESCRIPTION_H__
#define __QIH_PAGEDESCRIPTION_H__

#include <unistd.h>

#include "internalheap.h"
#include "log.h"

namespace QIH {

  //class describing a page

  class PageQueue;

  class PageDescription : public InternalHeapObject {
    friend class PageQueue;
  private:

    void *oldAddress; //old address of the object
    void *newAddress; //address in the cache

    size_t size;      //size of the object
#ifdef DETECT_OVERFLOWS
    size_t orig_size; //size as declared
#endif
    PageDescription *next; //next descriptor
    
    void *pos; //FIXME: HACK HACK HACK

    bool deleted; //status bits
    bool evicted;

  public:

    //public constructor
    
    PageDescription(const PageDescription &d)  : 
      oldAddress(d.oldAddress), newAddress(d.newAddress), 
      size(d.size), 
#ifdef DETECT_OVERFLOWS
      orig_size(d.orig_size),
#endif 
      next(d.next), pos(d.pos),
      deleted(d.deleted), evicted(d.evicted)
    {}

    PageDescription(void *realAddr, size_t sz)  :
      oldAddress(realAddr), newAddress(NULL), 
      size(sz), 
#ifdef DETECT_OVERFLOWS     
      orig_size(sz), 
#endif
      next(NULL), pos(NULL),
      deleted(false), evicted(false)
    {}

    PageDescription(void *realAddr, void *cacheAddr, size_t sz) :
      oldAddress(realAddr), newAddress(cacheAddr), 
      size(sz), 
#ifdef DETECT_OVERFLOWS
      orig_size(sz), 
#endif
      next(NULL), pos(NULL),
      deleted(false), evicted(false)
    {}
    
    PageDescription(void *realAddr, size_t sz, PageDescription *pd) :
      oldAddress(realAddr), newAddress(NULL), 
      size(sz), 
#ifdef DETECT_OVERFLOWS
      orig_size(sz), 
#endif
      next(NULL), pos(NULL),
      deleted(false), evicted(false)
    {
      if (pd != NULL)
	pd->setNextDescription(this);
    }
      
    PageDescription(void *realAddr, void *cacheAddr, size_t sz, PageDescription *pd) :
      oldAddress(realAddr), newAddress(cacheAddr), 
      size(sz), 
#ifdef DETECT_OVERFLOWS
      orig_size(sz), 
#endif
      next(NULL), 
      deleted(false), evicted(false)
    {
      
      if (pd != NULL)
	pd->setNextDescription(this);
    }

    //size

    inline size_t getSize() { 
      return size;       
    }
#ifdef DETECT_OVERFLOWS    
    inline size_t getOriginalSize() { 
      return orig_size;  
    } 
#endif
    //location

    inline void *getRealAddress(){ 
      return oldAddress; 
    }

    inline void *getAddressInCache() { 
      return newAddress; 
    }
      
    inline void *getRealPage(){ 
      return getPageStart(oldAddress); 
    }
      
    //next descriptor

    inline PageDescription *getNextDescription() { 
      return next; 
    }

    //status

    inline bool isDeleted(){ 
      return deleted; 
    }

    inline bool isEvicted(){ 
      return evicted; 
    }   

    //mutators

    inline void updateSize(/*size_t sz*/)  { 
      //      size = sz; 
      //last word on the page
      size_t *curWord = (size_t *)((size_t)getRealPage() + QIH_PAGE_SIZE - sizeof(size_t)); 

      while ((curWord >= getRealAddress()) && (!*curWord)) curWord--; //walk until start or non-zero
      
      if ((curWord >= getRealAddress()) && *curWord) {
	//found non-zero word
	size = (size_t)curWord + sizeof(size_t) - (size_t)getRealAddress();
#ifdef DETECT_OVERFLOWS
	if (size > orig_size) {
	  logWrite(ERROR, "Detected an overflow of object %p by %d bytes\n",
		   oldAddress,
		   size - orig_size);
	
	  stdoutWrite(ERROR, "Detected an overflow of object %p by %d bytes\n",
		      oldAddress,
		      size - orig_size);
	}
#endif

      } else {
	//page is zero up to start
	logWrite(INFO, "Page %p is empty up to the object start of %p\n", pd->getRealPage(), pd->getRealAddress());
	size = 0;
      }

    }
      
    inline void setAddressInCache(void *addr) { 
      newAddress = addr; 
    }

    inline void markDeleted(){ 
      deleted = true; 
    }

    inline void setEvicted(bool e) { 
      evicted = e; 
    }

    inline void setNextDescription(PageDescription *n){ 
      next = n; 
    }    

    //operators

    inline bool operator == (const PageDescription &p)  { 
      return oldAddress == p.oldAddress; 
    }
  };
}
#endif /*__QIH_PAGEDESCRIPTION_H__*/
