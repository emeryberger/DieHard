// -*- C++ -*-

#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#if defined(__sparc) && !defined(__GNUC__)
  extern "C" volatile unsigned long MyInterlockedExchange (unsigned long *, unsigned long);
#endif


class Atomic {
public:

  inline static volatile unsigned long
  exchange (unsigned long * oldval, unsigned long newval) {
#if defined(_WIN32) && defined(_MSC_VER)
    return InterlockedExchange ((volatile LONG *) oldval, newval);
#elif defined(__sparc) && !defined(__GNUC__)
    return MyInterlockedExchange (oldval, newval);
#elif defined(__sparc) && defined(__GNUC__)
  asm volatile ("swap [%1],%0"
		:"=r" (newval)
		:"r" (oldval), "0" (newval)
		: "memory");
#elif defined(__i386__)
  asm volatile ("lock; xchgl %0, %1"
		: "=r" (newval)
		: "m" (*oldval), "0" (newval)
		: "memory");
#elif defined(__sgi)
  newval = test_and_set (oldval, newval);
#elif defined(__ppc) || defined(__powerpc__) || defined(PPC)
  // Contributed by Maged Michael.
  int ret; 
  asm volatile ( 
		"La..%=0:    lwarx %0,0,%1 ;" 
		"      cmpw  %0,%2;" 
		"      beq La..%=1;" 
		"      stwcx. %2,0,%1;" 
		"      bne- La..%=0;" 
		"La..%=1:    isync;" 
                : "=&r"(ret) 
                : "r"(oldval), "r"(newval) 
                : "cr0", "memory"); 
  return ret;
#elif defined(__arm__)
  // Contributed by Bo Granlund.
  long result;
  asm volatile (
		       "\n\t"
		       "swp     %0,%2,[%1] \n\t"
		       ""
		       : "=&r"(result)
		       : "r"(oldval), "r"(newval)
		       : "memory");
  return (result);
#else
#error "No implementation is available for this platform."
#endif
  return newval;
}

};

#endif
