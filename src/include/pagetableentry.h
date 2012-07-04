// -*- C++ -*-

#ifndef DH_PAGETABLEENTRY_H
#define DH_PAGETABLEENTRY_H

class PageTableEntry {
public:

  PageTableEntry ()
    : _pageNumber (0),
      _heap (0),
      _pageIndex (0)
  {}

  PageTableEntry (unsigned long pNum,
		  void * b,
		  unsigned int idx) 
    : _pageNumber (pNum),
      _heap (b),
      _pageIndex (idx)
  {
    //fprintf(stderr,"%p: %p, %d\n",pNum, b, idx);
  }

 PageTableEntry (const PageTableEntry & rhs)
  : _pageNumber (rhs._pageNumber),
    _heap (rhs._heap),
    _pageIndex (rhs._pageIndex)
  {
  }

  void * getHeap() const {
    return _heap;
  }

  unsigned int getPageIndex() const {
    return _pageIndex;
  }

  bool isValid() const {
    return _heap != 0;
  }
  
  unsigned long hashCode() const {
    return _pageNumber;
  }

private:
  unsigned long		_pageNumber;
  void * 		_heap;
  unsigned int 		_pageIndex;
  uintptr_t 		_align; // EDB for alignment?
};

#endif
