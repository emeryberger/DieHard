/* -*- C++ -*- */

#ifndef __QIH_SINGLETONHEAP_H__
#define __QIH_SINGLETONHEAP_H__

namespace QIH {

  template<class SuperHeap>
  class SingletonHeap : public SuperHeap {
  private:
    SingletonHeap() : SuperHeap() {}
  public:
    static SingletonHeap<SuperHeap> *getInstance() {
      static char thBuf[sizeof(SingletonHeap<SuperHeap>)];
      static SingletonHeap<SuperHeap> *th = new (thBuf) SingletonHeap<SuperHeap>();
      return th;
    }
  };
}

#endif 
