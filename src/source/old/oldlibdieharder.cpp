

/*************************  error-reporting functions ************************/

#if !defined(_WIN32)

extern "C" void reportOverflowError (void) {
#if 0
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (an overflow).\n");
  }
#endif
}

extern "C" void reportDoubleFreeError (void) {
#if 0
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (a double free),\nbut DieHard has prevented it from doing any harm.\n");
  }
#endif
}

extern "C" void reportInvalidFreeError (void) {
#if 0
  static bool alertedAlready = false;
  if (!alertedAlready) {
    alertedAlready = true;
    fprintf (stderr, "This application has performed an illegal action (an invalid free),\nbut DieHard has prevented it from doing any harm.\n");
  }
#endif
}

// #include "heapshield.cpp"

#endif

/*************************  thread-intercept functions ************************/

#if !defined(_WIN32)

#include <pthread.h>
#include <dlfcn.h>

extern "C" {

  typedef void * (*threadFunctionType) (void *);

  typedef  
  int (*pthread_create_function) (pthread_t *thread,
				  const pthread_attr_t *attr,
				  threadFunctionType start_routine,
				  void *arg);

  int pthread_create (pthread_t *thread,
		      const pthread_attr_t *attr,
		      void * (*start_routine) (void *),
		      void * arg)
#ifndef __SVR4
    throw()
#endif
  {
    static pthread_create_function real_pthread_create = NULL;
    if (real_pthread_create == NULL) {
      real_pthread_create = (pthread_create_function) dlsym (RTLD_NEXT, "pthread_create");
    }
    return (*real_pthread_create)(thread, attr, start_routine, arg);
  }
}
#endif


#if defined(_WIN32)
#ifndef CUSTOM_DLLNAME
#define CUSTOM_DLLNAME DllMain
#endif

#ifndef DIEHARD_PRE_ACTION
#define DIEHARD_PRE_ACTION
#endif

#ifndef DIEHARD_POST_ACTION
#define DIEHARD_POST_ACTION
#endif
#endif // _WIN32

/*************************  default DLL initializer for Windows ************************/

#if defined(_WIN32) && !defined(NO_WIN32_DLL)

extern "C" {

BOOL WINAPI CUSTOM_DLLNAME (HANDLE hinstDLL, DWORD fdwReason, LPVOID lpreserved)
{
  char buf[255];

  switch (fdwReason) {

  case DLL_PROCESS_ATTACH:
    sprintf (buf, "Using the DieHarder application-hardening system (Windows DLL, version %d.%d).\nCopyright (C) 2011 Emery Berger, University of Massachusetts Amherst.\n", VERSION_MAJOR_NUMBER, VERSION_MINOR_NUMBER);
    fprintf (stderr, buf);
    DIEHARD_PRE_ACTION;
    break;

  case DLL_PROCESS_DETACH:
    DIEHARD_POST_ACTION;
    break;

  case DLL_THREAD_ATTACH:
    break;

  case DLL_THREAD_DETACH:
    break;

  default:
    return TRUE;
  }
  
  return TRUE;
}

}

#endif // WIN32

