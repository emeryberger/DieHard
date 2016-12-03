/* -*- C++ -*- */

#ifndef __QIH_OBJECTPERPAGEHEAP_H__
#define __QIH_OBJECTPERPAGEHEAP_H__

/**
 * @class ObjectPerPageHeap
 * @brief Allocates 1 object per VM page
 * @author Vitaliy Lvin
 */

#include <unistd.h>

#include "allocationclockheap.h"

#include "heaplayers.h"

#ifndef MMAP_POOL_HEAP 
#include "bitmapheap.h"
#endif

#include "coloringheap.h"

#ifndef OPP_ONLY_HEAP
#include "fixedactiveheap.h"
#endif

#include "fixedsizeobjheap.h"
#include "internalheap.h"

#ifdef MMAP_POOL_HEAP
#include "mmappoolheap.h"
#endif

#include "safeinitheap.h"
#include "singletonheap.h"
#include "sizeinmapheap.h"

namespace QIH {

#ifdef MMAP_POOL_HEAP
  typedef FixedSizeObjHeap<MmapPoolHeap<HL::PrivateMmapHeap, InternalDataHeap, PAGE_POOL_SIZE>, getpagesize> ManagingHeap;
#else
  typedef FixedSizeObjHeap<BitmapHeap<HL::PrivateMmapHeap, InternalDataHeap>, getpagesize> ManagingHeap;
#endif

#ifdef NO_COLORING
  typedef ManagingHeap OptimizedManagingHeap;
#else
  typedef ColoringHeap<ManagingHeap> OptimizedManagingHeap;
#endif 

#ifdef OPP_ONLY_HEAP
  typedef AllocationClockHeap<SizeInMapHeap<OptimizedManagingHeap, InternalDataHeap> > InternalOppHeap;
#else 
  typedef FixedActiveHeap<AllocationClockHeap<SizeInMapHeap<OptimizedManagingHeap, InternalDataHeap> > > InternalOppHeap;
#endif 

#ifdef REPLACE_SIGACTION

  typedef SingletonHeap<SafeInitHeap<HL::LockedHeap<HL::SpinLockType, HL::ANSIWrapper<InternalOppHeap> >, HL::ANSIWrapper<HL::LockedHeap<HL::SpinLockType, HL::SizeHeap<HL::StaticHeap< 1024 * 1024> > > > > > ObjectPerPageHeap;
#else
  typedef SingletonHeap<HL::ANSIWrapper<InternalOppHeap> > ObjectPerPageHeap;
#endif    

}

#endif
