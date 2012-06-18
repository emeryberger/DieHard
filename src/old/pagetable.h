// -*- C++ -*-

#ifndef __PAGE_TABLE_H__
#define __PAGE_TABLE_H__

#include <sys/mman.h>
#include <stdint.h>
#include <diehard.h>

class RandomMiniHeapBase;

class PageTableEntry {
public:
 PageTableEntry (uintptr_t pNum,
		 RandomMiniHeapBase * b,
		 unsigned int idx) 
   : pageNumber(pNum), heap(b), pageIndex(idx)
  {
    //fprintf(stderr,"%p: %p, %d\n",pNum, b, idx);
  }

 PageTableEntry(const PageTableEntry & rhs): pageNumber(rhs.pageNumber),
    heap(rhs.heap), pageIndex(rhs.pageIndex) {
  }
  
  uintptr_t pageNumber;
  RandomMiniHeapBase * heap;
  unsigned int pageIndex;
  uintptr_t align;
  
  bool isValid() const {
    return heap != 0;
  }
  
  uintptr_t getHashCode() const {
    return pageNumber;
  }
};

class PageTable {
  typedef uintptr_t KEY_TYPE;
  typedef PageTableEntry VALUE_TYPE;

  typedef unsigned int UINT;

  // Initial table size in bytes
  static const size_t INIT_SIZE = PAGE_SIZE;

  //  enum { FOO = 1 / (sizeof(VALUE_TYPE) & (sizeof(VALUE_TYPE) - 1)) };

 public:
 PageTable() : _entries(NULL), _map_size(INIT_SIZE/sizeof(VALUE_TYPE)), _mask(_map_size-1) {
    _entries = (VALUE_TYPE*)allocTable(_map_size);
   
   //fprintf(stderr,"allocated %p for %d entries, mask %d\n",_entries,_map_size,_mask);

    //fprintf(stderr,"initialized page table, min = %lx, max = %lx\n",_addrspace,_addrspace+ADDRSPACE_SIZE);
  }
  
  ~PageTable() {
    for(UINT i = 0; i < _map_size; i++) {
      _entries[i].~VALUE_TYPE();
    }
  }

  void * allocatePage(RandomMiniHeapBase * heap, unsigned int idx) {
    uintptr_t pageNumber;
    void * ret;
    
    ret = mmap(0, PAGE_SIZE,PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    
    //    fprintf(stderr,"allocated page %p\n",ret);
    
    pageNumber = ((uintptr_t)ret) >> 12;

    insert(PageTableEntry(pageNumber,heap,idx));

    return ret;
  }
  
  void * allocatePageRange(RandomMiniHeapBase * heap, unsigned int idx, size_t ct) {
    uintptr_t pageNumber;
    void * ret;
    
    ret = mmap(0,ct*PAGE_SIZE,PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    pageNumber = ((uintptr_t)ret) >> 12;
    
    for (size_t i = 0; i < ct; i++) {
      insert (PageTableEntry(pageNumber+i,heap,idx));
    }
        
    return ret;
  }

  PageTableEntry * getPageTableEntry(uintptr_t ptr) const {
    uintptr_t pageNumber = ptr >> 12;
    PageTableEntry * ret = find(pageNumber);

    //assert(ret->isValid());

    return ret;
  }

  static PageTable & getInstance() {
    static char buf[sizeof(PageTable)];
    static PageTable * instance = new (buf) PageTable;
    return *instance;
  }

private:
  void grow() {
    //fprintf(stderr,"growing, old map size: %d\n",_num_elts);

    VALUE_TYPE * old_entries = _entries;
    size_t old_map_size = _map_size;

    _entries = (VALUE_TYPE *)allocTable(_map_size*2);
    _map_size *= 2;
    _mask = _map_size-1;
    
    unsigned int old_elt_count = _num_elts;
    old_elt_count = old_elt_count;

    _num_elts = 0;
	
    // rehash

    unsigned int ct = 0;

    for(unsigned int i = 0; i < old_map_size; i++) {
      if(old_entries[i].isValid()) {
        ct++;
        insert(old_entries[i]);
      }
    }

    //fprintf(stderr,"new map size: %d\n",_num_elts);

    assert (ct == old_elt_count);

    munmap(old_entries,_map_size/2*sizeof(VALUE_TYPE));
  }

  /** Inserts the given site into the map.
   *  Precondition: no site with a matching hash value is in the map.
   */
  // XXX: change to bitwise operations for speed
  void insert(const VALUE_TYPE & s) {
    //fprintf(stderr,"inserting, now %d/%d\n",_num_elts,_map_size);
    
    if(_num_elts+1 > 0.125*_map_size) {
      grow();
    } 
    
    _num_elts++;

    int begin = s.getHashCode() & _mask;
    int lim = (begin - 1 + _map_size) & _mask;

    // NB: we don't check slot lim, but we're actually guaranteed never to get 
    // there since the load factor can't be 1.0
    for(int i = begin; i != lim; i = (i+1)&_mask) {
      if(_entries[i].isValid()) {
        assert(_entries[i].getHashCode() != s.getHashCode());
        continue;
      } else {
	// invoke copy constructor via placement new
	new (&_entries[i]) VALUE_TYPE(s);
	return;
      }
    }

    assert(false);
  }

  VALUE_TYPE * find(KEY_TYPE key) const {
    int begin = key & _mask;
    int lim = (begin - 1 + _map_size) & _mask;

    int probes = 0;
    probes = probes;

    for(int i = begin; i != lim; i = (i+1) & _mask) {
      //fprintf(stderr,"probing entry %d\n",i);
#if 0
      probes++;
      if(probes % 10 == 0) {
        // XXX: fix hash function to lower clustering?
	char buf[255];
	sprintf (buf, "probed a lot of times: %d - %d\n", probes, _map_size);
	printf (buf);
      }
#endif
      //fprintf(stderr,"address is %p\n",&_entries[i]);
      ///fprintf(stderr,"content %d\n",*((int *)(&_entries[i])));

      if(_entries[i].isValid()) {
        //fprintf(stderr,"address is %p\n",&_entries[i]);

        if(_entries[i].getHashCode() == key) {
          return &_entries[i];
        } else { 
          continue;
        }
      } else {
        return 0;
      }
    }

    // path cannot be reached---must find empty bin since load factor < 1.0
    //printf("failing to find key %p\n",key);
    
    abort();
    return 0;
  }

  /*
  VALUE_TYPE * findOrInsert(KEY_TYPE key) {
    int begin = key & _mask;
    int lim = (begin - 1 + _map_size) & _mask;

    int probes = 0;

    for(int i = begin; i != lim; i = (i+1) & _mask) {
      probes++;
      if(probes == 10) {
	// XXX: fix hash function to lower clustering?
	//printf("probed a lot of times\n");
      }
      if(_entries[i].isValid()) {
	if(_entries[i].getHashCode() == key) {
	  return &_entries[i];
	} else { 
	  continue;
	}
      } else {
	if(_num_elts+1 > 0.5*_map_size) {
	  grow();
	  return findOrInsert(key);
	} else {
	  _num_elts++;
	  if(!(_num_elts % 100)) {
	    char buf[80];
	    sprintf(buf,"now %d active callsites\n",_num_elts);
	    //OutputDebugString(buf);
	  }
	  return new (&_entries[i]) VALUE_TYPE(key);
	}
      }
    }

    // path cannot be reached---must find empty bin since load factor < 1.0
    printf("failing to find key 0x%x\n",key);
    
    abort();
    return NULL;
  }
  */
    
  void * allocTable(int nElts) {
    //fprintf(stderr,"allocating %d bytes\n",nElts*sizeof(VALUE_TYPE));
    
    return mmap(0,nElts*sizeof(VALUE_TYPE), PROT_READ | PROT_WRITE, 
		MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  }
  
  VALUE_TYPE * _entries;
  char * _addrspace;
  size_t _map_size;
  size_t _mask;
  size_t _num_elts;

  /// A local random number generator.
  threadlocal<RandomNumberGenerator> _random;
};

#endif // __PAGE_TABLE_H__
