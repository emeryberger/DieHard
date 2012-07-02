// -*- C++ -*-

#ifndef MYPAGETABLE_H_
#define MYPAGETABLE_H_

#include "mmapheap.h"
#include "mmapwrapper.h"
#include "madvisewrapper.h"
#include "dynamichashtable.h"
#include "pagetableentry.h"
#include "singleton.h"
#include "cpuinfo.h"

#include "randommmap.h"

#define WE_ALREADY_HAVE_ASLR 0

class MyPageTableClass : public DynamicHashTable<uintptr_t, PageTableEntry, MmapHeap> {
public:

  PageTableEntry * getPageTableEntry (void * addr) {
    uintptr_t pageNumber = computePageNumber (addr);
    return DynamicHashTable<uintptr_t, PageTableEntry, MmapHeap>::get (pageNumber);
  }

  void * allocatePages (RandomMiniHeapBase * heap, unsigned int idx, size_t ct) {
#if WE_ALREADY_HAVE_ASLR
    void * ret = MmapWrapper::map (ct*CPUInfo::PageSize);
    MadviseWrapper::random (ret, ct * CPUInfo::PageSize);
#else
    void * ret = _mapper.map (ct * CPUInfo::PageSize);
#endif
    uintptr_t pageNumber = computePageNumber (ret);
    for (size_t i = 0; i < ct; i++) {
      insert (PageTableEntry (pageNumber+i,heap,idx));
    }
    return ret;
  }

private:

  static uintptr_t computePageNumber (void * addr) {
    return (uintptr_t) addr >> StaticLog<CPUInfo::PageSize>::VALUE;
  }

  RandomMmap _mapper;

};


class MyPageTable : public singleton<MyPageTableClass> {};

#endif
