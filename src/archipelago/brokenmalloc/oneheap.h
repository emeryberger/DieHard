#ifndef _ONEHEAP_H_
#define _ONEHEAP_H_

template <class TheHeap>
class OneHeap {
public:

  static inline void * malloc (size_t sz) {
    return getHeap().malloc (sz);
  }

  static inline bool free (void * ptr) {
    return getHeap().free (ptr);
  }

  static inline size_t getSize (void * ptr) {
    return getHeap().getSize (ptr);
  }

private:

  static inline TheHeap& getHeap (void) {
    static TheHeap _heap;
    return _heap;
  }

};


#endif
