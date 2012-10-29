// -*- C++ -*-

#ifndef __QIH_LOG_H__
#define __QIH_LOG_H__

#include <stdio.h>

namespace QIH {

  enum debug_level {FATAL = 0, ERROR = 1, WARNING = 2, DEBUG = 3, INFO = 4, ALL = 5};
 
#ifdef NO_LOGGING

#define logInit()        

#define logWrite(L, ...) 

#define logClose()       

#else 

  extern enum debug_level currentDebugLevel;

  //  extern void _logInit();

  extern void _logWrite(enum debug_level level, const char *fmt, ...);

  extern void _logClose();

  //#define logInit() _logInit()

#define logWrite(L, ...) _logWrite((L), __VA_ARGS__)

#define logClose() _logClose()

#endif

  void stdoutWrite(enum debug_level level, const char *fmt, ...); 

  void reportError(const char *fmt, ...);
}
										
#endif

