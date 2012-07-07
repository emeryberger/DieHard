// -*- C++ -*-

#ifndef DIEHARDER_PAGETABLE_H
#define DIEHARDER_PAGETABLE_H

#include "heaplayers.h"
// #include "singleton.h"
// #include "cpuinfo.h"

#include "dynamichashtable.h"
// To move inside.

#include "randommmap.h"

#include "pagetableentry.h"

namespace DieHarder {

  static uintptr_t computePageNumber (void * addr) {
    return (uintptr_t) addr >> StaticLog<CPUInfo::PageSize>::VALUE;
  }

  class PageTable {
  public:

    void * getHeap (void * ptr) {
      PageTableEntry entry;
      uintptr_t pageNumber = computePageNumber (ptr);
      bool found = getEntry (pageNumber, entry);
      if (!found) {
	return NULL;
      } else {
	return entry.getHeap();
      }
    }


    unsigned int getObjectIndex (void * ptr) {
      PageTableEntry entry;
      uintptr_t pageNumber = computePageNumber (ptr);
      bool found = getEntry (pageNumber, entry);
      if (!found) {
	assert (false);
	return 0;
      } else {
	return entry.getObjectIndex();
      }
    }

    void insert (uintptr_t pageNumber, void * heap, unsigned int index) {
      PageTableEntry entry (pageNumber, heap, index);
      _table.insert (entry);
    }

  private:

    DynamicHashTable<PageTableEntry, CPUInfo::PageSize, MmapHeap> _table;

    bool getEntry (unsigned long pageNumber, PageTableEntry& entry) {
      return  _table.get (pageNumber, entry);
    }

  };

  singleton<PageTable>  pageTable;
  singleton<RandomMmap> mapper;

}

#endif
