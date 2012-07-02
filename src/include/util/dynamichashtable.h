// -*- C++ -*-

#ifndef DH_DYNAMICHASHTABLE_H
#define DH_DYNAMICHASHTABLE_H

#include <new>
#include <stdint.h>

#include "guard.h"
#include "posixlock.h"
#include "checkpoweroftwo.h"

template <class KEY_TYPE,
	  class VALUE_TYPE,
	  class SourceHeap,
	  size_t INIT_SIZE = 4096>

class DynamicHashTable {

  typedef unsigned int UINT;

  // The reciprocal of the maximum load factor for the hash table.  In
  // other words, 1/LOAD_FACTOR_RECIPROCAL is how full the hash table
  // can get.
  enum { LOAD_FACTOR_RECIPROCAL = 8 };

  // When we grow the hash table, we multiply its size by this expansion factor.
  // NOTE: This *must* be a power of two.
  enum { ExpansionFactor = 2 };

public:

  DynamicHashTable() :
    _size (INIT_SIZE / sizeof(VALUE_TYPE)),
    _mask (_size-1),
    _entries (allocTable (_size))
  {
    CheckPowerOfTwo<ExpansionFactor> verify1;
    CheckPowerOfTwo<INIT_SIZE / sizeof(VALUE_TYPE)> verify2;
  }
  
  ~DynamicHashTable() {
    for (UINT i = 0; i < _size; i++) {
      _entries[i].~VALUE_TYPE();
    }
  }

  VALUE_TYPE * get (KEY_TYPE ptr) {
    Guard<HL::PosixLockType> l (_lock);

    VALUE_TYPE * ret = find (ptr);
    return ret;
  }

  /** Inserts the given site into the map.
   *  Precondition: no site with a matching hash value is in the map.
   */
  // XXX: change to bitwise operations for speed
  void insert (const VALUE_TYPE& s) 
  {
    Guard<HL::PosixLockType> l (_lock);

    //fprintf(stderr,"inserting, now %d/%d\n",_numElements,_size);
    
    if (LOAD_FACTOR_RECIPROCAL * (_numElements+1) > _size) {
      grow();
    } 
    
    insertOne (s);
  }

private:

  void insertOne (const VALUE_TYPE& s) 
  {
    assert (!find (s));
    _numElements++;

    int begin = s.getHashCode() & _mask;
    int lim = (begin - 1 + _size) & _mask;

    // NB: we don't check slot lim, but we're actually guaranteed never to get 
    // there since the load factor can't be 1.0
    for (int i = begin; i != lim; i = (i+1)&_mask) {
      if (_entries[i].isValid()) {
        assert(_entries[i].getHashCode() != s.getHashCode());
        continue;
      } else {
	_entries[i] = s;
	return;
      }
    }

    assert(false);
  }

  void grow() 
  {
    //fprintf(stderr,"growing, old map size: %d\n",_numElements);

    // Save old values.
    size_t old_size = _size;
    VALUE_TYPE * old_entries = _entries;
    unsigned int old_elt_count = _numElements;
    old_elt_count = old_elt_count;

    // Get a new hash table.
    _size *= ExpansionFactor;
    _mask = _size-1;
    _entries = allocTable (_size);
    _numElements = 0;
	
    // rehash

    unsigned int ct = 0;

    for (unsigned int i = 0; i < old_size; i++) {
      if (old_entries[i].isValid()) {
        ct++;
        insertOne (old_entries[i]);
      }
    }

    //fprintf(stderr,"new map size: %d\n",_numElements);

    assert (ct == old_elt_count);

    _sh.free (old_entries);
  }


  VALUE_TYPE * find (KEY_TYPE key)
  {
    int begin = key & _mask;
    int lim = (begin - 1 + _size) & _mask;

    int probes = 0;
    probes = probes;

    for (int i = begin; i != lim; i = (i+1) & _mask) {
      //fprintf(stderr,"probing entry %d\n",i);
#if 0
      probes++;
      if(probes % 10 == 0) {
        // XXX: fix hash function to lower clustering?
	char buf[255];
	sprintf (buf, "probed a lot of times: %d - %d\n", probes, _size);
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

    abort();
    return 0;
  }

  VALUE_TYPE * allocTable (int nElts)
  {
    //fprintf(stderr,"allocating %d bytes\n",nElts*sizeof(VALUE_TYPE));
    void * ptr = 
      _sh.malloc (nElts * sizeof(VALUE_TYPE));
    return new (ptr) VALUE_TYPE[nElts];
  }

  HL::PosixLockType _lock;

  SourceHeap _sh;
  char * _addrspace;
  size_t _size;
  size_t _mask;
  VALUE_TYPE * _entries;
  size_t _numElements;
};

#endif // PAGETABLE_H_
