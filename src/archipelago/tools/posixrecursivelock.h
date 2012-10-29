#ifndef _POSIXRECURSIVELOCK_H_
#define _POSIXRECURSIVELOCK_H_

// PosixRecursiveLockType: recursive lock
class PosixRecursiveLockType {

public:

	PosixRecursiveLockType() {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
		pthread_mutex_init(&mutex, &attr);
	}
  
	~PosixRecursiveLockType() {
		pthread_mutex_destroy(&mutex);
	}
  
	void lock() {
		pthread_mutex_lock(&mutex);
	}
  
	void unlock() {
		pthread_mutex_unlock(&mutex);
	}
  
private:

	pthread_mutex_t mutex;

};	// end of class PosixRecursiveLockType


#endif


