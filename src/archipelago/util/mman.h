/* -*- C++ -*- */

#ifndef __QIH_MMAN_H__
#define __QIH_MMAN_H__

/*
 * Wrappers around standard memory management functions
 */

#include <sys/mman.h>

/*
 * Include header for patched kernel
 */

#if defined(VMCOMM)

#include "vmcomm.h"

#elif defined(VMSIG)

#include "vmsig.h"

#endif

/*
 * Deal with linux abusing POSIX sematics of madvise. 
 * Linux doesn't have real MADV_DONTNEED
 */

#ifndef MADV_FREE

#ifdef MADV_FREE_PRESENT

#define MADV_FREE 5

#else

#define MADV_FREE MADV_DONTNEED

#endif

#endif

/*
 * Enable or disabled clearing pages
 */

#ifdef DETECT_OVERFLOWS
#define MEMSET0_ENABLED
#endif


#ifdef MEMSET0_ENABLED

#define Memset0(A, B) memset((A), 0, (B))

#else

#define Memset0(A, B)

#endif

/*
 * Enable or disable protection of unused allocated pages in the system
 */

#ifdef PROTECTION_ENABLED

#define Mprotect(A, B)   mprotect((A), (B), PROT_NONE)
#define Munprotect(A, B) mprotect((A), (B), PROT_WRITE | PROT_READ)

#else

#define Mprotect(A, B)
#define Munprotect(A, B)

#endif

/*
 * General (unconditional) wrappers
 */

#define MsetTrap(A, B) mprotect((A), (B), PROT_NONE)
#define MremoveTrap(A,B) mprotect((A), (B), PROT_WRITE | PROT_READ)

#define MrandomAccess(A, B) madvise((A), (B), MADV_RANDOM)

/*
 * Enable different ways of returning pages to the system
 */

#ifdef MADVISE //straight-up madvise

#define Mdiscard(A, B) madvise((A), (B), MADV_FREE); //mprotect((A), (B), PROT_WRITE | PROT_READ);

#elif defined(MADVISE_RATELIMITED) //rate-limited

namespace QIH {

  extern int Mdiscard(void *, size_t);

  extern void increaseMdiscardRate(void);

  extern void decreaseMdiscardRate(void);

}

#elif defined(MADVISE_ASYNC) //move madvise to a separate thread

//#include "madvisetask.h"
namespace QIH {

  extern void asyncMdiscard(void *);

}

#define Mdiscard(A, B) asyncMdiscard(A)

#else //don't madvise at all

#define Mdiscard(A, B) 

#endif
 
#endif
