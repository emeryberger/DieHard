#include <pthread.h>
#include <deque>

//#include "internalheap.h"
#include "madvisetask.h"
namespace QIH {

//   static std::deque<void*, SafeSTLAlloc<void*> > workQueue;

//   static pthread_t thread;
  
//   static pthread_cond_t hasWork;
  
//   static pthread_mutex_t queueLock;

//   static void *threadFunc(void *dummy) {
//     while (1) {
//       pthread_mutex_lock(&queueLock);

//       while (workQueue.empty())
// 	pthread_cond_wait(&hasWork, &queueLock);
	
//       void *addr = workQueue.front();
//       workQueue.pop_front();

//       pthread_mutex_unlock(&queueLock);

//       madvise(addr, 4096, 5); //FIXME: HACK

//     }

//     return NULL;
    
//   }

//   static inline void init(void) {
//     pthread_cond_init (&hasWork,   NULL);
//     pthread_mutex_init(&queueLock, NULL);
    
//     pthread_create(&thread, NULL, threadFunc, NULL);
      
//     pthread_detach(thread);  
//   }

  void asyncMdiscard(void *addr) {
//     static bool ready = false;

//     if (!ready) {
//       init();
//       ready = true;
//     }

//     pthread_mutex_lock(&queueLock);
//     workQueue.push_back(addr);
//     pthread_mutex_unlock(&queueLock);

//     pthread_cond_signal(&hasWork);

    TypedFutureTaskRunner<MadviseTask>::
      getInstance()->submitTask(new MadviseTask(addr, 4096, 5));
  }

}
