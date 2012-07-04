// -*- C++ -*-

#ifndef DH_DYNAMICHASHTABLE_H
#define DH_DYNAMICHASHTABLE_H

#include <new>
#include <stdint.h>

#include "heaplayers.h"

//#include "guard.h"
//#include "posixlock.h"
#include "checkpoweroftwo.h"

template <class VALUE_TYPE,
	  size_t INIT_SIZE = 4096,
	  class SourceHeap = HL::MallocHeap>

class DynamicHashTable {

  class StoredObject {
  public:

    StoredObject()
      : _isValid (false)
    {}

    StoredObject (const StoredObject& s) {
      _isValid = s._isValid;
      _value   = s._value;
    }

    void markValid() {
      _isValid = true;
    }
    bool isValid() const {
      return _isValid;
    }
    VALUE_TYPE& get() {
      return _value;
    }
    void put (const VALUE_TYPE& v) {
      _value = v;
      markValid();
    }
      
  private:
    bool _isValid;
    VALUE_TYPE _value;
  };

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
    _entries (allocTable (_size)),
    _numElements (0)
  {
    CheckPowerOfTwo<ExpansionFactor> verify1;
    CheckPowerOfTwo<INIT_SIZE / sizeof(VALUE_TYPE)> verify2;
  }
  
  ~DynamicHashTable() {
    _sh.free (_entries);
  }

  bool get (unsigned long k, VALUE_TYPE& value) {
    Guard<HL::PosixLockType> l (_lock);

    return find (k, value);
  }

  /// @brief Insert the given object into the map.
  void insert (const VALUE_TYPE& s) 
  {
    Guard<HL::PosixLockType> l (_lock);
    
#if 0
    {
      char buf[255];
      sprintf (buf, "inserting, now %d/%d\n", _numElements, _size);
      fprintf (stderr, buf);
    }
#endif

    if ((_numElements+1) > _size / LOAD_FACTOR_RECIPROCAL) {
      grow();
    } 
    
    insertOne (s);
    _numElements++;

  }

private:

  void insertOne (const VALUE_TYPE& s) 
  {
    int begin = s.hashCode() & _mask;
    int lim = (begin - 1 + _size) & _mask;

    // NB: we don't check slot lim, but we're actually guaranteed never to get 
    // there since the load factor can't be 1.0
    for (int i = begin; i != lim; i = (i+1)&_mask) {
      if (!_entries[i].isValid()) {
	_entries[i].put (s);
	_entries[i].markValid();
	return;
      }
    }

    // We should never get here.
    assert (false);
  }

  void grow() 
  {
#if 0
    {
      char buf[255];
      sprintf (buf, "GROWING, now %d/%d\n", _numElements, _size);
      fprintf (stderr, buf);
    }
#endif

    // Save old values.
    size_t old_size = _size;
    StoredObject * old_entries = _entries;
    unsigned int old_elt_count = _numElements;
    old_elt_count = old_elt_count;

    // Make room for a new table, growing the current one.
    _size *= ExpansionFactor;
    _mask = _size-1;
    _entries = allocTable (_size);

    if (_entries == NULL)
      abort();

    // Rehash all the elements.

    unsigned int ct = 0;

    for (unsigned int i = 0; i < old_size; i++) {
      if (old_entries[i].isValid()) {
        ct++;
        insertOne (old_entries[i].get());
      }
    }

    assert (ct == _numElements);
    _sh.free (old_entries);
  }


  /// @brief Find the entry for a given key.
  bool find (unsigned long key, VALUE_TYPE& value)
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
      if (_entries[i].isValid()) {

        if (_entries[i].get().hashCode() == key) {
          value = _entries[i].get();
	  return true;
	}

      } else {
	// Not found.
        return false;
      }
    }

    // path cannot be reached---must find empty bin since load factor < 1.0

    abort();
    return 0;
  }

  StoredObject * allocTable (int nElts)
  {
    void * ptr = 
      _sh.malloc (nElts * sizeof(StoredObject));
    return new (ptr) StoredObject[nElts];
  }

  HL::PosixLockType _lock;

  SourceHeap _sh;
  size_t _size;
  size_t _mask;
  StoredObject * _entries;
  size_t _numElements;
};

#endif // PAGETABLE_H_
