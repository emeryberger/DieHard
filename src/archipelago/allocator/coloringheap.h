/* -*- C++ -*- */

/**
 * @file coloringheap.h
 * @class ColoringHeap
 * @brief Colors the objects
 * @author Vitaliy Lvin
 */

#ifndef __QIH_COLORINGHEAP_H__
#define __QIH_COLORINGHEAP_H__

#include <sys/types.h>

#include "util.h"

#define REMAINDER_MASK 0xFFF //FIXME: hack, page size may change

namespace QIH {

    template <class SuperHeap>
    class ColoringHeap : public SuperHeap {
    public:

	inline void *malloc(size_t sz) {
    
	    void *ptr = SuperHeap::malloc(sz); //allocate from the parent
    
	    //apply coloring

	    if (sz < QIH_PAGE_SIZE) 
		ptr = getObjectColoring(ptr, sz); //objects smaller then page - apply coloring directly
	    else 
		ptr = getObjectColoring(ptr, sz & REMAINDER_MASK); //objects larger then page - color the leftover
	    

	    return ptr; // return to the user
	}

	inline void free (void *ptr, size_t sz) {
	    SuperHeap::free(getPageStart(ptr), sz); //free at parent
	}
    };

}


#endif
