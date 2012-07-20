// -*- C++ -*-

#ifndef DH_ARRAY_H
#define DH_ARRAY_H

#include <cassert>

  template <unsigned long N, typename T>
  class Array {
  public:

    inline T& operator()(unsigned long index) {
      assert (index < N);
      return _item[index];
    }

    inline const T& operator()(unsigned long index) const {
      assert (index < N);
      return _item[index];
    }

  private:

    T _item[N];

  };


#endif
