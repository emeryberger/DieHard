/* -*- C++ -*- */

/**
 * @file   stlallocator.h
 * @brief  An allocator adapter for STL.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2005 by Emery Berger, University of Massachusetts Amherst.
 */

#ifndef _STLALLOCATOR_H_
#define _STLALLOCATOR_H_

#include <stdio.h>
#include <new>
#include <stdlib.h>

#include <memory>

using namespace std;

/**
 * @class STLAllocator
 * @brief An allocator adapter for STL.
 * 
 * This mixin lets you use any Heap Layers allocator as the allocator
 * for an STL container.
 *
 * Example:
 * <TT>
 *   typedef STLAllocator<int, MyHeapType> MyAllocator;<BR>
 *   list<int, MyAllocator> l;<BR>
 * </TT>
 */

template <class T, class Super>
class STLAllocator : public Super {
public:

  typedef T value_type;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;
  typedef T * pointer;
  typedef const T * const_pointer;
  typedef T& reference;
  typedef const T& const_reference;

  STLAllocator (void) {}
  virtual ~STLAllocator (void) {}

  STLAllocator (const STLAllocator& s)
    : Super (s)
  {}

#if defined(_WIN32)
  char * _Charalloc (size_type n) {
    return (char *) allocate (n);
  }
#endif

#if defined(__SUNPRO_CC)
  inline void * allocate (size_type n,
			  const void * = 0) {
    if (n) {
      return reinterpret_cast<void *>(Super::malloc (sizeof(T) * n));
    } else {
      return (void *) 0;
    }
  }
#else
  inline pointer allocate (size_type n,
			  const void * = 0) {
    if (n) {
      return reinterpret_cast<pointer>(Super::malloc (sizeof(T) * n));
    } else {
      return 0;
    }
  }
#endif

  inline void deallocate (void * p, size_type) {
    Super::free (p);
  }

  inline void deallocate (pointer p, size_type) {
    Super::free (p);
  }
  
  pointer address (reference x) const { return &x; }
  const_pointer address (const_reference x) const { return &x; }

  void construct (pointer p, const T& val) { new (p) T (val); }
  void destroy (pointer p) { p->~T(); }

  /// Make the maximum size be the largest possible object.
  size_type max_size(void) const
  {
    size_type n = (size_type)(-1);
    return (n > 0 ? n : (size_type)(n));
  }

  template <class U> STLAllocator( const STLAllocator<U, Super> &) {}
  template <class U> struct rebind { typedef STLAllocator<U,Super> other; };

};

template <class TYPE, class T, class O>
inline bool operator!=(const STLAllocator<TYPE, T>& a, const STLAllocator<TYPE,O>& b) {
  return ((void *) &a != (void *) &b);
}

template <class TYPE, class T, class O>
inline bool operator==(const STLAllocator<TYPE, T>& a, const STLAllocator<TYPE,O>& b) {
  return ((void *) &a == (void *) &b);
}


#endif
