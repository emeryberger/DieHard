/* -*- C++ -*- */

/*

  Heap Layers: An Extensible Memory Allocation Infrastructure
  
  Copyright (C) 2000-2005 by Emery Berger
  http://www.cs.umass.edu/~emery
  emery@cs.umass.edu
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#ifndef _STLALLOCATOR_H_
#define _STLALLOCATOR_H_

#include <stdio.h>
#include <new>
#include <stdlib.h>

#include <memory> // STL

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

namespace HL {

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

  void construct (const_pointer p, const T& val) { new ((pointer) p) T (val); }
  void construct (pointer p, T& val) { new (p) T (val); }
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

  template <typename T, class S>
  inline bool operator!=(const STLAllocator<T,S>& a, const STLAllocator<T,S>& b) {
    return (&a != &b);
  }

  template <typename T, class S>
  inline bool operator==(const STLAllocator<T,S>& a, const STLAllocator<T,S>& b) {
    return (&a == &b);
  }

#if 0

  #include <memory> // STL
  #include <stdlib.h>

  template <typename T, class Super>
  class STLAllocator : public std::allocator<T>, public Super {
  public:
    inline T * allocate (std::size_t n,
			 const void * = 0) {
      if (n) {
	return reinterpret_cast<T *>(Super::malloc (sizeof(T) * n));
      } else {
	return 0;
      }
    }
    
    inline void deallocate (void * p, std::size_t) {
      Super::free (p);
    }
    
    inline void deallocate (T * p, std::size_t) {
      Super::free (p);
    }
    
  };

#endif

}


#endif
