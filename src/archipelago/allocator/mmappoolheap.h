/* -*- C++ -*- */
#ifndef __QIH_MMAPPOOLHEAP_H__
#define __QIH_MMAPPOOLHEAP_H__

/**
 * @file mmappoolheap.h
 * @class MmapPoolHeap
 * @brief Keeps pool of pages
 * @author Vitaliy Lvin
 */

#include <sys/mman.h>
#include <stack> 

#include "stlallocator.h"
#include "util.h"

namespace QIH {

    template <class SuperHeap, class ListHeap, int poolSize> 
    class MmapPoolHeap : public SuperHeap {
    private:

	typedef std::stack<void*, std::deque<void*, HL::STLAllocator<void *, ListHeap> > > StackType;

	StackType freePages;

	inline void allocatePool() {
	    void *pool = SuperHeap::malloc(poolSize * QIH_PAGE_SIZE);
    
	    if (!pool) //out of memory
		return;
	    
	    madvise (pool, poolSize * QIH_PAGE_SIZE, MADV_RANDOM); //random access
	    
	    Mprotect(pool, poolSize * QIH_PAGE_SIZE); //protect pool

	    for (int i = 0; i < poolSize; i++)
		freePages.push((void*)((int)pool + QIH_PAGE_SIZE * i));
	}


    public:
	MmapPoolHeap<SuperHeap, ListHeap, poolSize>() {
	    allocatePool();
	}

	void * malloc (size_t sz) {
	    void * ptr = NULL;

	    if (sz > QIH_PAGE_SIZE)
		ptr = SuperHeap::malloc (sz);

	    else {

		if (freePages.empty()) 
		    allocatePool();

		if (!freePages.empty()) {
		    ptr = freePages.top();
		    freePages.pop();

		    Munprotect(ptr, QIH_PAGE_SIZE); //allow access
		    Memset0(ptr, QIH_PAGE_SIZE); //clear
		}
	    }

	    ASSERT_LOG((unsigned int)ptr & 0x00000fff, 
		       "MmapPoolHeap returned address %p, which is not on a page boundary!\n", ptr);
	    return ptr;
	}


	void free (void * ptr, size_t sz) {
    
	    if (sz > QIH_PAGE_SIZE)  
		SuperHeap::free(ptr, sz);
	    else {
		freePages.push(ptr);

		Mdiscard(ptr, QIH_PAGE_SIZE);
		Mprotect(ptr, QIH_PAGE_SIZE); //protect
	    }
	}
    };

}


#endif
