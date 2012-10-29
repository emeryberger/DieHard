#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "log.h"

namespace QIH {

  enum debug_level currentDebugLevel = 
#ifndef INITIAL_DEBUG_LEVEL
  FATAL
#else
  INITIAL_DEBUG_LEVEL
#endif
  ;

  static char *processName = 
#ifdef PROC_NAME
  PROC_NAME
#else
  "sparseheaps"
#endif
  ;

  static int logFile = -1;

  inline void _logInit() {
    char buf[64];

    snprintf(buf, 64, "./%s_%i.log", processName, getpid());

    logFile = open(buf, O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP);
  }


  void _logWrite(enum debug_level level, const char *fmt, ...) {
    if (level <= currentDebugLevel) {
      
      if (logFile == -1) { //make sure log file is opened
	_logInit();
	if (logFile == -1) return;
      }

      va_list fmtargs;
      char buf[129];

      va_start(fmtargs, fmt); //generate a string
      vsnprintf(buf, 128, fmt, fmtargs);
      va_end(fmtargs);
      
      switch (level) { //write the prefix
      case FATAL:   write(logFile, "FATAL:   ", 9);
	break;
      case ERROR:   write(logFile, "ERROR:   ", 9);
	break;
      case WARNING: write(logFile, "WARNING: ", 9);
	break;
      case DEBUG:   write(logFile, "DEBUG:   ", 9);
	break;
      case INFO :   write(logFile, "INFO :   ", 9);
	break;
      case ALL  :   write(logFile, "ALL  :   ", 9);
	break;
      }

      write(logFile, buf, strlen(buf));
    }
  }

  void _logClose() {
    fsync(logFile);
    close(logFile);
  }

  void stdoutWrite(enum debug_level level, const char *fmt, ...) { 
    char *prefix = "*** ";
    va_list fmtargs;
    char buf[129];
    char final[129];
    FILE *stream = stdout;

    if (level == FATAL) { 
      prefix = "*** Fatal error: ";
      stream = stderr;
    }
#ifdef SUPRESS_STDOUT
    else return;
#endif
    
    va_start(fmtargs, fmt); //generate a string
    vsnprintf(buf, 128, fmt, fmtargs);
    va_end(fmtargs);

    snprintf(final, 128, "%s%s", prefix, buf); //add a prefix
     
    fputs(final, stream);
  }


  void reportError(const char *fmt, ...) { 
    va_list fmtargs;
    char buf[129];
     
    va_start(fmtargs, fmt); //generate a string
    vsnprintf(buf, 128, fmt, fmtargs);
    va_end(fmtargs);

    fputs(buf, stderr);
  }


}
