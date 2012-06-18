/* -*- C++ -*- */

#ifndef _ADDHEAP_H_
#define _ADDHEAP_H_

/**
 * @file addheap.h
 * @brief Reserves space for a class in the head of every allocated object.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */


#include <assert.h>

namespace HL {

template <class Add, class Super>
class AddHeap : public Super {
public:
  
  inline void * malloc (size_t sz) {
	void * ptr = Super::malloc (sz + align(sizeof(Add)));
	void * newPtr = (void *) align ((size_t) ((Add *) ptr + 1));
	return ptr;
  }

  inline void free (void * ptr) {
	void * origPtr = (void *) ((Add *) ptr - 1);
	Super::free (origPtr);
  }
  
private:
	static inline size_t align (size_t sz) {
		return (sz + (sizeof(double) - 1)) & ~(sizeof(double) - 1);
	}

};

};
#endif
