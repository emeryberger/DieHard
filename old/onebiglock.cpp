#include <pthread.h>

extern "C" {
  int __pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *);
  int __pthread_mutex_lock(pthread_mutex_t *mutex);
  int __pthread_mutex_trylock(pthread_mutex_t *mutex);
  int __pthread_mutex_unlock(pthread_mutex_t *mutex);
  int __pthread_mutexattr_init (pthread_mutexattr_t * attr);
  int __pthread_mutexattr_gettype(const pthread_mutexattr_t *,
				 int *);
  int __pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);
}

class Lock {
public:

  Lock() {
    __pthread_mutexattr_init (&_realLockAttr);
    __pthread_mutexattr_settype (&_realLockAttr, PTHREAD_MUTEX_RECURSIVE);
    __pthread_mutex_init (&_realLock, &_realLockAttr);
  }

  void lock() {
    __pthread_mutex_lock (&_realLock);
  }

  void unlock() {
    __pthread_mutex_unlock (&_realLock);
  }
  
  void trylock() {
    __pthread_mutex_trylock (&_realLock);
  }


private:
  pthread_mutexattr_t _realLockAttr;
  pthread_mutex_t _realLock;
  double _dummy[64]; // cache padding to avoid false sharing
};


static Lock theOneBigLock;

int pthread_mutex_lock(pthread_mutex_t *mutex) {
  theOneBigLock.lock();
  return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
  theOneBigLock.trylock();
  return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
  theOneBigLock.unlock();
  return 0;
}

