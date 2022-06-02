// -*- C++ -*-

#ifndef DH_PAGETABLEENTRY_H
#define DH_PAGETABLEENTRY_H

class PageTableEntry {
public:

  PageTableEntry ()
    : _pageNumber (0),
      _heap (0),
      _objectIndex (0)
  {}

  PageTableEntry (unsigned long pNum,
		  void * b,
		  unsigned int idx) 
    : _pageNumber (pNum),
      _heap (b),
      _objectIndex (idx)
  {
    //fprintf(stderr,"%p: %p, %d\n",pNum, b, idx);
  }

 PageTableEntry (const PageTableEntry & rhs) = default;

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
};

#endif
