// -*- C++ -*-

#ifndef DH_PAGETABLEENTRY_H
#define DH_PAGETABLEENTRY_H

class PageTableEntry {
public:

  PageTableEntry ()
    : _pageNumber (0),
      _heap (0),
      _objectIndex (0),
      _align (0x1234)
  {}

  PageTableEntry (unsigned long pNum,
		  void * b,
		  unsigned int idx) 
    : _pageNumber (pNum),
      _heap (b),
      _objectIndex (idx),
      _align (0x1234)
  {
    //fprintf(stderr,"%p: %p, %d\n",pNum, b, idx);
  }

 PageTableEntry (const PageTableEntry & rhs)
  : _pageNumber (rhs._pageNumber),
    _heap (rhs._heap),
    _objectIndex (rhs._objectIndex)
  {
  }

  void * getHeap() const {
    return _heap;
  }

  unsigned int getObjectIndex() const {
    return _objectIndex;
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
  unsigned int 		_objectIndex;
  uintptr_t 		_align;
};

#endif
