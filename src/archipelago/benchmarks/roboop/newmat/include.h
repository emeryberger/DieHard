//$$ include.h           include files required by various versions of C++

#if USE_REAP && defined(_WIN32)
#pragma comment (lib, "libreap.lib")
#endif


#ifndef INCLUDE_LIB
#define INCLUDE_LIB

//#define use_namespace                   // define name spaces

//#define SETUP_C_SUBSCRIPTS              // allow element access via A[i][j]

// Activate just one of the following 3 statements

#define SimulateExceptions              // use simulated exceptions
//#define UseExceptions                   // use C++ exceptions
//#define DisableExceptions               // don't use exceptions


#define TEMPS_DESTROYED_QUICKLY         // for compilers that delete
					// temporaries too quickly

//#define TEMPS_DESTROYED_QUICKLY_R       // the same thing but applied
					// to return from functions only

//#define DO_FREE_CHECK                   // check news and deletes balance

#define USING_DOUBLE                    // elements of type double
//#define USING_FLOAT                   // elements of type float

// activate the following statement if your compiler defines bool

//#define bool_LIB 0


//*********************** end of options set by user ********************


#define DEFAULT_HEADER                  // use AT&T style header
                                        // if no other compiler is recognised

#ifdef _MSC_VER                         // Microsoft
   #include <stdlib.h>

//   reactivate these statements to run under MSC version 7.0
//   typedef int jmp_buf[9];
//   extern "C"
//   {
//      int __cdecl setjmp(jmp_buf);
//      void __cdecl longjmp(jmp_buf, int);
//   }

   #ifdef WANT_STREAM
      #include <iostream>
      #include <iomanip>
      using namespace std;
   #endif
   #ifdef WANT_MATH
      #include <math.h>
      #include <float.h>
   #endif
   #undef DEFAULT_HEADER
#endif

#ifdef __ZTC__                          // Zortech
   #include <stdlib.h>
   #ifdef WANT_STREAM
      #include <iostream.hpp> 
      #include <iomanip.hpp>
      #define flush ""                  // not defined in iomanip?
   #endif
   #ifdef WANT_MATH
      #include <math.h>
      #include <float.h>
   #endif
   #undef DEFAULT_HEADER
#endif

#if defined __BCPLUSPLUS__ || defined __TURBOC__  // Borland or Turbo
   #include <stdlib.h>
   #ifdef WANT_STREAM
      #include <iostream.h>
      #include <iomanip.h>
   #endif
   #ifdef WANT_MATH
      #include <math.h>
      #define SystemV                // optional in Borland
      #include <values.h>            // Borland has both float and values
                                     // but values.h returns +INF for
                                     // MAXDOUBLE in BC5
   #endif
   #undef DEFAULT_HEADER
#endif

#ifdef __GNUG__                         // Gnu C++
   #include <stdlib.h>
   #ifdef WANT_STREAM
      #include <iostream>
      #include <iomanip>
using namespace std;
   #endif
   #ifdef WANT_MATH
      #include <math.h>
      #include <float.h>
   #endif
   #undef DEFAULT_HEADER
#endif

#ifdef Glock                            // Glockenspiel
   extern "C" { #include <stdlib.h> }
   #ifdef WANT_STREAM
      #include <stream.hxx>
      #include <iomanip.hxx>
   #endif
   #ifdef WANT_MATH
      extern "C" { #include <math.h> }
      extern "C" { #include <float.h> }
   #endif
   #define NO_LONG_NAMES                // very long names don't work
   #undef DEFAULT_HEADER
#endif

#ifdef __WATCOMC__                      // Watcom C/C++
   #include <stdlib.h>
   #ifdef WANT_STREAM
      #include <iostream.h>
      #include <iomanip.h>
   #endif
   #ifdef WANT_MATH
      #include <math.h>
      #include <float.h>
   #endif
   #undef DEFAULT_HEADER
#endif


#ifdef macintosh                        // MPW C++ on the Mac
#include <stdlib.h>
#ifdef WANT_STREAM
#include <iostream.h>
#include <iomanip.h>
#endif
#ifdef WANT_MATH
#include <float.h>
#include <math.h>
#endif
#undef DEFAULT_HEADER
#endif

#ifdef DEFAULT_HEADER                   // for example AT&T
#define ATandT
#include <stdlib.h>
#ifdef WANT_STREAM
#include <iostream.h>
#include <iomanip.h>
#endif
#ifdef WANT_MATH
#include <math.h>
#define SystemV                         // use System V
#include <values.h>
#endif
#endif

#ifdef use_namespace
namespace RBD_COMMON {
#endif


#ifdef USING_FLOAT                      // set precision type to float
typedef float Real;
typedef double long_Real;
#endif

#ifdef USING_DOUBLE                     // set precision type to double
typedef double Real;
typedef long double long_Real;
#endif


#ifdef use_namespace
}
#endif


#ifdef use_namespace
namespace RBD_COMMON {}
namespace RBD_LIBRARIES                 // access all my libraries
{
   using namespace RBD_COMMON;
}
#endif


#endif
