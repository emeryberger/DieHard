#include <string.h>

#include "stlallocator.h"

struct slot_info_t {
  unsigned long id;
  unsigned long alloc_callsite;
  unsigned long free_callsite;
  unsigned long free_time;
};

class ExterminatorMetadata {
 protected:
  void elementsChanged(size_t new_el) {
    fprintf(stderr,"metadata size now: %d\n",new_el);

    assert(new_el >= _elements);

    size_t new_info_sz = sizeof(slot_info_t)*new_el;
    size_t old_info_sz = sizeof(slot_info_t)*_elements;

    slot_info_t * new_info = 
      reinterpret_cast<slot_info_t *>(_slot_info_allocator.malloc(new_info_sz));
    bzero(new_info,new_info_sz);

    if(_slot_info) {
      memcpy(new_info,_slot_info,old_info_sz);
      _slot_info_allocator.free(_slot_info);
    }

    _elements  = new_el;
    _slot_info = new_info;
  }

  inline void allocEvent(int idx) {
    unsigned long pc = get_callsite();
    //fprintf(stderr, "callsite: 0x%x\n",pc);
    _slot_info[idx].alloc_callsite = pc;
    _slot_info[idx].id = global_ct;
  }

  inline void freeEvent(int idx) {
    _slot_info[idx].free_callsite = get_callsite();
    _slot_info[idx].free_time = global_ct;
  }

  inline unsigned long getAllocCallsite(int idx) {
    return _slot_info[idx].alloc_callsite;
  }

  inline unsigned long getObjectID(int idx) {
    return _slot_info[idx].id;
  }

  void dumpMetadata(FILE * out) {
    fwrite(_slot_info,sizeof(slot_info_t),_elements,out);
  }
  
 private:
  unsigned long _elements;
  slot_info_t * _slot_info;
  MmapAlloc _slot_info_allocator;
};
