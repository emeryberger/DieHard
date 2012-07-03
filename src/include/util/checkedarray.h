// -*- C++ -*-

#ifndef CHECKEDARRAY_H
#define CHECKEDARRAY_H

#include <cassert>

/**
 *
 * A fixed-size array with bounds-checking whose storage is only allocated on demand.
 *
 **/

template <unsigned long N,
	  typename T,
	  class Heap>
class CheckedArray {
public:

  CheckedArray()
    : _isActivated (false)
  {}

  inline T& operator()(unsigned long index) {
    assert (index < N);
    activate();
    return _item[index];
  }

private:

  void activate() {
    if (!_isActivated) {
      _item = new (Heap::malloc (sizeof(T) * N)) T[N];
      _isActivated = true;
    }
  }

  bool _isActivated;
  T * _item;

};


#endif
