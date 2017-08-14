// -*- C++ -*-

#ifndef DH_BUFFEREDLOCKEDHEAP_H
#define DH_BUFFEREDLOCKEDHEAP_H

#include <mutex>
#include <cstddef>

#ifndef thread_local
# if __STDC_VERSION__ >= 201112 && !defined __STDC_NO_THREADS__
#  define thread_local _Thread_local
# elif defined _WIN32 && ( \
       defined _MSC_VER || \
       defined __ICL || \
       defined __DMC__ || \
       defined __BORLANDC__ )
#  define thread_local __declspec(thread)
/* note that ICC (linux) and Clang are covered by __GNUC__ */
# elif defined __GNUC__ || \
       defined __SUNPRO_C || \
       defined __xlC__
#  define thread_local __thread
# else
#  error "Cannot define thread_local"
# endif
#endif

template <int Size, class LockType, class Super>
class BufferedLockedHeap : public Super {
public:

  enum { Alignment = Super::Alignment };

  void * malloc (size_t sz) {
    std::lock_guard<LockType> l (_theLock);
    void *ptr = Super::malloc (sz);
    if (ptr != nullptr) {
      return ptr;
    }
    // Emptying the freeBuffer and trying again
    emptyFreeBuffer();
    return Super::malloc (sz);
  }

  void free (void *ptr) {
    if (freeIndex == Size) {
      std::lock_guard<LockType> l (_theLock);
      emptyFreeBuffer();
    }
    if (freeBuffer == nullptr) {
      init();
    }
    freeBuffer[freeIndex] = ptr;
    freeIndex++;
  }

  inline size_t getSize (void * ptr) const {
    return Super::getSize (ptr);
  }

  inline size_t getSize (void * ptr) {
    return Super::getSize (ptr);
  }

  inline void lock (void) {
    _theLock.lock();
  }

  inline void unlock (void) {
    _theLock.unlock();
  }

  void clear (void) {
    std::lock_guard<LockType> l (_theLock);
    emptyFreeBuffer();
    Super::free (freeBuffer);
  }

private:

  void init (void) {
    std::lock_guard<LockType> l (_theLock);
    freeBuffer = reinterpret_cast<void **>
      (Super::malloc (sizeof(void *) * Size));
    freeIndex = 0;
  }

  void emptyFreeBuffer (void) {
    for (int i = 0; i < freeIndex; i++) {
      Super::free (freeBuffer[i]);
    }
    freeIndex = 0;
  }

  static thread_local void **freeBuffer;
  static thread_local int freeIndex;

  LockType _theLock;
};

template <int Size, class LockType, class Super>
thread_local void **BufferedLockedHeap<Size, LockType, Super>::freeBuffer = nullptr;
template <int Size, class LockType, class Super>
thread_local int BufferedLockedHeap<Size, LockType, Super>::freeIndex = 0;

#endif
