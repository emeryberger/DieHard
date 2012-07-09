// -*- C++ -*-

#ifndef BROKENHEAP_H
#define BROKENHEAP_H

template <class SuperHeap>
class BrokenHeap : public SuperHeap {
public:
  BrokenHeap()
    : _count (0),
      _previousMalloc (0)
  {}
  
  void * malloc (size_t sz) {
    // Return every pointer obtained from malloc TWICE, after a delay.
    // This tends to cause problems. :)
    _count++;
    if (_count == 1) {
      _previousMalloc = SuperHeap::malloc (sz);
      return _previousMalloc;
    }
    if (_count >= 1000) {
      _count = 0;
      return _previousMalloc;
    }
    return SuperHeap::malloc (sz);
  }

private:
  int _count;
  void * _previousMalloc;

};

#endif
