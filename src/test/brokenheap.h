#ifndef BROKENHEAP_H
#define BROKENHEAP_H

template <class SuperHeap>
class BrokenHeap : public SuperHeap {
public:
  BrokenHeap()
    : count (0),
      prev (NULL)
  {}

  void * malloc (size_t sz) {
    // Return every pointer obtained from malloc TWICE, after a delay.
    // This tends to cause problems. :)
    count++;
    if (count == 1) {
      prev = SuperHeap::malloc (sz);
      return prev;
    }
    if (count >= 100) {
      count = 0;
      return prev;
    }
    return SuperHeap::malloc (sz);
    }
  }

private:
  int count;
  void * prev;

};

#endif
