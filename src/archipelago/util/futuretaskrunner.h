// -*- C++ -*-

#ifndef __QIH_FUTURE_H__
#define __QIH_FUTURE_H__

#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

#include <queue>

#include "internalheap.h"
#include "log.h"
#include "spinlock.h"

namespace QIH {

  template<class TaskType>
  class TypedFutureTaskRunner {
  private:
    std::deque<TaskType*, SafeSTLAlloc<TaskType*> > workQueue;

    pthread_t thread;

    pthread_cond_t hasWork;

    pthread_mutex_t queueLock;

    bool running;

    static void *threadFunc(void* ptr) {
      TypedFutureTaskRunner<TaskType> *f = static_cast<TypedFutureTaskRunner<TaskType>*>(ptr);
      f->runTasks();
      pthread_exit(NULL);
    }

    TypedFutureTaskRunner(void) : running(false)  {

      pthread_cond_init (&hasWork,   NULL);
      pthread_mutex_init(&queueLock, NULL);

      int res = pthread_create(&thread, NULL, threadFunc, (void*)this);

      if (!res) {
	logWrite(ERROR, "Failed to initialize background work thread, pthead_create returned %d\n", res);
      }
    }

  public:

    ~TypedFutureTaskRunner(void) {
      printf("destructor!\n");
      stop();
      pthread_cond_destroy (&hasWork  );
      pthread_mutex_destroy(&queueLock);
    }


    inline void runTasks()  {
      running = true;

      while (running) {
	pthread_mutex_lock(&queueLock); //lock

	if (workQueue.empty()) //sleep until queue has something
	  pthread_cond_wait(&hasWork, &queueLock);
	  
	if (!running) { //woken up by destructor, time to go
	  pthread_mutex_unlock(&queueLock);
	  return;
	}

	TaskType *task = workQueue.front();//pop a task off the front
	workQueue.pop_front();

	pthread_mutex_unlock(&queueLock); //unlock

	task->perform(); //do work

	delete task; //delete task object      
      }
    }

    inline static TypedFutureTaskRunner<TaskType>  *getInstance()  {
      static char mem[sizeof(TypedFutureTaskRunner<TaskType>)];
      static TypedFutureTaskRunner<TaskType> *instance = 
	new (mem) TypedFutureTaskRunner<TaskType>();
      return instance;
    }

    inline void submitTask(TaskType *task)  {
      if (running) {
	pthread_mutex_lock(&queueLock);
	workQueue.push_back(task);
	pthread_mutex_unlock(&queueLock);

	pthread_cond_signal(&hasWork);
	
      }
      else {
	task->perform(); //run the the context of the caller

	delete task;
      }
    }

    inline void stop() {
      running = false; //stop on the next check
      pthread_cond_signal(&hasWork); //wake up, if sleeping
      //pthread_join(thread, NULL); //wait till done
    }

  };


  class UniversalFutureTask : public SafeHeapObject {
  public:
     virtual ~UniversalFutureTask() {}
    
     virtual void perform(void) = 0;
    
  };

  typedef TypedFutureTaskRunner<UniversalFutureTask> UniversalFutureTaskRunner;
 
}


#endif
