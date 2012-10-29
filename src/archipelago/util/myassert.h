#ifndef __MYASSERT_H__
#define __MYASSERT_H__

#include <stdio.h>
#include <assert.h>
#include "log.h"

#ifndef NO_ASSERTS
#define ASSERT_STDOUT(C, ...) if (!(C)) { \
                                  char __buf[100]; \
                                  snprintf(__buf, 100, __VA_ARGS__); \
                                  fputs(__buf, (O)); \
                                  logClose();\
                                  assert((C)); }


#define ASSERT_LOG(C, ...) if (!(C)) { \
                             logWrite(FATAL, __VA_ARGS__); \
                             logClose();\
                             assert((C)); }


#define ASSERT_STDERR(C, ...) if (!(C)) { \
                                  char __buf[100]; \
                                  snprintf(__buf, 100, __VA_ARGS__); \
                                  fputs(__buf, stderr); \
                                  logClose();\
                                  assert((C)); }
#else
#define ASSERT_STDOUT(C, ...)
#define ASSERT_LOG(C, ...)
#define ASSERT_STDERR(C, ...) 
#endif

#endif
