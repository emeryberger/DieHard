/* -*- C++ -*- */

/**
 * @file bitmapheap.h
 * @class BitmapHeap
 * @brief Keeps pool of pages, managed
 * @author Vitaliy Lvin
 */

#ifndef __QIH_BITMAPHEAP_H__
#define __QIH_BITMAPHEAP_H__

#include <sys/mman.h>

#include "bitmap.h"
#include "mman.h"
#include "randomnumbergenerator.h"
#include "util.h"

#ifdef IV

#include "iv.h"
//sampling period, in pages allocated, of number of allocation stalls
#define PERIOD 400 
//weight placed on the past behavior
#define RESPONSE_FACTOR 0.4
#endif


namespace QIH {

  template <class SuperHeap, class InternalHeap> 
  class BitmapHeap : public SuperHeap {
  private:

    const static int MAX_FULL_FACTOR = 2;

    char *pool, *poolEnd;

    size_t poolSize;

    BitMap<InternalHeap> used, allocated;

    RandomNumberGenerator rnd;

    unsigned int singlePageCount;   

    unsigned int splitCount;

#if defined(VMSIG) 
    void *victimBuffer[VM_MAX_RELINQUISH];
    unsigned int victimCount, victimIndex;
    bool victimLock;
#endif
 
#ifdef IV
    unsigned int totalPageCount;
    unsigned int lastCheck;
    double pastStallRate;
#endif

    inline bool isFull() {
      if (singlePageCount > (poolSize / MAX_FULL_FACTOR)) {
	logWrite(INFO, "Heap is more then 1/%d full\n", MAX_FULL_FACTOR);
	return true;
      }
      else return false;
    }

    inline bool isManaged(int pos) {
      return (pos >= 0) && (pos < poolSize);
    }

    inline bool isManagable(size_t sz) {
      return sz <= QIH_PAGE_SIZE;
    }
	    
    inline bool isAllocated(int pos) {
      return isManaged(pos) && allocated.isSet(pos);
    }

    inline int getRandomIndex() {
      return rnd.next() % poolSize;
    }

    inline void *getAddress(int index) {
      return (void*)(pool + index * QIH_PAGE_SIZE);
    }

    inline int getIndex(char *ptr) {
      return ((size_t)ptr - (size_t)pool) / QIH_PAGE_SIZE;
    }

    inline void *singlePageAlloc(void) {
      
      //single-page requests
	
      int pos = 0;
	
      do {
	pos = getRandomIndex();
      }
      while (!allocated.tryToSet(pos));
      
      //      allocated.tryToSet(pos);
      
      //don't allow pages before and after to be used as well
    //   if (isManaged(pos - 1))
// 	used.tryToSet(pos - 1);
      
//       if (isManaged(pos + 1))
// 	used.tryToSet(pos + 1);
      
      void *ptr = getAddress(pos);
      
      Munprotect(ptr, QIH_PAGE_SIZE); //re-enable access
      
      Memset0(ptr, QIH_PAGE_SIZE); //clear the page 
	
      singlePageCount++;

#ifdef IV
      totalPageCount++;
      checkAllocStallRate();
#endif

      return ptr;
    }

    inline void *multiPageAlloc(size_t sz) {

      	//any requests for more then 1 page get satisfied directly through mmap
	void *ptr = SuperHeap::malloc(sz + QIH_PAGE_SIZE); //add a guard page

	if (!ptr) {
	  logWrite(WARNING, "Failed to allocate %d bytes, returning NULL\n", sz);
	  return NULL;
	}

	//protect the guard paged
	Mprotect((void*)((char*)ptr + sz), QIH_PAGE_SIZE);

#ifdef IV
	totalPageCount += sz / QIH_PAGE_SIZE;
	checkAllocStallRate();
#endif

	return ptr;
    }

    inline void singlePageDealloc(void *ptr) {
      int pos = getIndex((char*)ptr);
	
      if (!allocated.isSet(pos)) {
	logWrite(WARNING, "Attempting to free page %p that wasn't allocated\n", 
		 ptr);
	return;
      }

      allocated.reset(pos);

#if defined(VMSIG)
      addVictim(pos); //add to victim buffer
#else
      //      used.reset(pos);
#endif

      //unprotect pages around
//       if (isManaged(pos - 1) && !isAllocated(pos - 2))
// 	used.reset(pos - 1);
      
//       if (isManaged(pos + 1) && !isAllocated(pos + 2))
// 	used.reset(pos + 1);
      
      Mprotect(ptr, QIH_PAGE_SIZE); //protect
      
      Mdiscard(ptr, QIH_PAGE_SIZE); //instruct vm manager to reuse the page
     
      singlePageCount--;
    }

    inline void multiPageDealloc(void *ptr, size_t sz) {
      SuperHeap::free(ptr, sz + QIH_PAGE_SIZE); //not managed through bitmap   
    }

    void splitPool(char *pool, unsigned int pages) {

      if (pages <= 4) return;
      
      splitCount++;

      char *middle = pool + pages * QIH_PAGE_SIZE / 2;

      /*MsetTrap*/mprotect(middle, QIH_PAGE_SIZE, PROT_NONE);
      /*MremoveTrap*/mprotect(middle, QIH_PAGE_SIZE, PROT_READ | PROT_WRITE);

      logWrite(FATAL, "Split on page %p\n", middle);

      splitPool(pool, pages / 2);
      splitPool(middle, pages / 2);

    }

#if defined(VMSIG) //|| defined(IV)
    inline void addVictim(int index) {
      if (victimLock) {
	used.reset(index);
	return;
      }

      if (victimCount == VM_MAX_RELINQUISH) {//buffer full 
	used.reset(getIndex(static_cast<char*>(victimBuffer[victimIndex]))); //evict an entry
	logWrite(DEBUG, "%d: Victim buffer full\n", getAllocationClock());
      }
      else //not full 
	victimCount++; 

      victimBuffer[victimIndex++] = getAddress(index); //add a new page, increase the index

      if (victimIndex == VM_MAX_RELINQUISH) //wrap around the index
	victimIndex = 0;
    } 
#endif

#ifdef IV
    inline void checkAllocStallRate(void) {

      if ((totalPageCount - lastCheck) > PERIOD) {

	lastCheck = totalPageCount;

	if (end_allocstall_collection() != 0) {
	  logWrite(FATAL, "Failed to get a reading of allocstalls\n");
	}
	else {
	  
	  double newStallRate = (double)get_last_allocstall_count();

	  if (newStallRate > pastStallRate) {


	    increaseMdiscardRate();
	  }
	  else {
	    decreaseMdiscardRate();
	    //	    pastStallRate = RESPONSE_FACTOR * pastStallRate + (1 - RESPONSE_FACTOR) * newStallRate;
	  }

	  pastStallRate = (RESPONSE_FACTOR * pastStallRate) + (1 - RESPONSE_FACTOR) * newStallRate;
	  //	  logWrite(FATAL, 
	  //	   "Observed %ld allocstalls in last %d allocated pagese, pastStallRate is %.5f\n", 
	  //		   get_last_allocstall_count(), PERIOD, pastStallRate);

	}

	start_allocstall_collection();
      }
    }
#endif


  public: //----------------------------------------------------------------------------


    BitmapHeap<SuperHeap, InternalHeap>() :
      pool (NULL), poolEnd (NULL), poolSize(0),
      rnd (0xDEADBEEF, 0x01001001),
					  singlePageCount(0)//, splitCount(0)
#ifdef VMSIG
      , victimCount(0), victimIndex(0), victimLock(false)
#endif

#ifdef IV
      , totalPageCount(0), lastCheck(0), pastStallRate(0.0)
#endif
    {
      char *ps = getenv("PAGE_POOL_SIZE");

      if (ps) 
	poolSize = atoi(ps);
      else  
	poolSize = PAGE_POOL_SIZE;
      

      stdoutWrite(INFO, "Preallocating %d pages (%dMB)\n", poolSize, poolSize * QIH_PAGE_SIZE / (1024 * 1024) );

      logWrite(INFO, "Preallocating %d pages (%dMB)\n", poolSize, poolSize * QIH_PAGE_SIZE / (1024 * 1024) );

      pool = (char *)SuperHeap::malloc((poolSize + 1) * QIH_PAGE_SIZE);
	  
      poolEnd = pool + poolSize * QIH_PAGE_SIZE;

      if (pool != NULL) {
	    
	MrandomAccess(pool, (poolSize + 1) * QIH_PAGE_SIZE); //no access pattern

#if 0	    
 	stdoutWrite(FATAL, "Splitting the pool...\n");
 	splitPool(pool, poolSize);
 	stdoutWrite(FATAL, "Done! Performed %d splits\n", splitCount);
#endif

	Mprotect(pool, (poolSize + 1) * QIH_PAGE_SIZE);   //protect the pool
	    
// 	used.reserve(poolSize); //initialize bitmap
// 	used.clear();
		
	allocated.reserve(poolSize);
	allocated.clear();
      }
      else logWrite(WARNING, "Failed to pre-allocate memory pool\n");

#ifdef IV
      start_allocstall_collection();
#endif 

    }
	
    inline void *malloc (size_t sz) {

      if (!isManagable(sz) || !pool || isFull()) {

	return multiPageAlloc(sz);
	
      } else { 

	return singlePageAlloc();

      }

    }

    inline void free (void * ptr, size_t sz) {

      if (isManaged((char*)ptr))
	singlePageDealloc(ptr);
      else {
	multiPageDealloc(ptr, sz);
      }
    }

    inline bool isManaged(void *ptr) {
      return (static_cast<char*>(ptr) >= pool) && (static_cast<char*>(ptr) < poolEnd);
    }

    inline bool isAllocated (void *page) {
      return isManaged(page) && isAllocated(getIndex((char*)page));
    }

#if defined(VMSIG) //|| defined(IV)
    inline int getVictimBuffer(void **buf) {
      if (!buf || !victimCount) return 0;
      
      victimLock = true;

      memcpy(buf, victimBuffer, sizeof(void*) * victimCount);

      return victimCount;
    }

    inline void clearVictimBuffer() {
      for (unsigned int i = 0; i < victimCount; i++)
	used.reset(getIndex(static_cast<char*>(victimBuffer[i])));

      victimCount = 0;
      victimIndex = 0;

      victimLock = false;
    }
#endif

  };

}

#endif
