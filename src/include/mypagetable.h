// -*- C++ -*-

#ifndef DH_MYPAGETABLE_H
#define DH_MYPAGETABLE_H

#include "heaplayers.h"
// #include "mmapwrapper.h"
// #include "singleton.h"
// #include "cpuinfo.h"
// #include "mmapheap.h"

#include "madvisewrapper.h"
#include "dynamichashtable.h"
#include "pagetableentry.h"

#include "randommmap.h"

#define WE_ALREADY_HAVE_ASLR 0

class MyPageTableClass {
public:

  bool getPageTableEntry (void * addr, PageTableEntry& pe) {
    uintptr_t pageNumber = computePageNumber (addr);
    return _table.get (pageNumber, pe);
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
      _table.insert (PageTableEntry (pageNumber+i, heap, idx));
    }
    return ret;
  }

private:

  static uintptr_t computePageNumber (void * addr) {
    return (uintptr_t) addr >> StaticLog<CPUInfo::PageSize>::VALUE;
  }

  RandomMmap _mapper;
  DynamicHashTable<PageTableEntry, 4096, MmapHeap> _table;

};


class MyPageTable : public singleton<MyPageTableClass> {};

#endif
