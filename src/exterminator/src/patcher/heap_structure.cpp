#include "heap_structure.h" 

int pinfo::pmap(int idx) {
  assert(this == &_rep->partitions[_rep->pmap[idx].first]);
  return _rep->pmap[idx].second;
}

unsigned long pinfo::VAtoID(unsigned long ptr) {
  ptr = ptr-vaddr_start;
  ptr /= size;
  assert(ptr >= 0);
  assert(ptr < elements);
  return _slot_info[ptr].id;
}

unsigned long pinfo::VAtoAllocSite(unsigned long ptr) {
  ptr = ptr-vaddr_start;
  ptr /= size;
  return _slot_info[ptr].alloc_callsite;
}

unsigned long pinfo::VAtoFreeSite(unsigned long ptr) {
  ptr = ptr-vaddr_start;
  ptr /= size;
  return _slot_info[ptr].free_callsite;
}

unsigned long * pinfo::VAtoPointer(unsigned long ptr) {
  return (unsigned long *)(ptr-vaddr_start+(unsigned long)heap_start);
}

unsigned long pinfo::PointerToVA(unsigned long * ptr) {
  return ((unsigned long)(ptr))-(unsigned long)heap_start+vaddr_start;
}

bool pinfo::IDallocated(int which) {
  assert(which < _rep->global_allocs);
  return pmap(which) != -1 && DXallocated(_rep->pmap[which].second);
}

bool pinfo::DXallocated(int which) {
  assert(which >= 0);
  assert(which < elements);
  return get_bit(bitmap_start,which);
}

unsigned long pinfo::IDtoAllocSite(int idx) {
  return _slot_info[pmap(idx)].alloc_callsite;
}

unsigned long pinfo::DXtoAllocSite(int idx) {
  return _slot_info[idx].alloc_callsite;
}

unsigned long pinfo::IDtoFreeSite(int idx) {
  return _slot_info[pmap(idx)].free_callsite;
}

unsigned long pinfo::DXtoFreeSite(int idx) {
  return _slot_info[idx].free_callsite;
}

unsigned long pinfo::IDtoFreeTime(int idx) {
  return _slot_info[pmap(idx)].free_time;
}

unsigned long pinfo::DXtoFreeTime(int idx) {
  return _slot_info[idx].free_time;
}

int pinfo::IDtoDX(int id) {
  assert(id < _rep->global_allocs);
  return pmap(id);
}

int pinfo::DXtoID(int idx) {
  return _slot_info[idx].id;
}

unsigned long pinfo::IDtoVA(int idx) {
  unsigned long ret = vaddr_start + size*pmap(idx);
  assert(ret < vaddr_end);
  return ret;
  //    return vaddr_start + size_in_words*pmap[idx];
}

unsigned long * pinfo::IDtoPointer(int idx) {
  if(pmap(idx) != -1) 
    return heap_start + size_in_words*pmap(idx);
  else return (unsigned long *)(0xd00f);
}

unsigned long * pinfo::DXtoPointer(int idx) {
  return heap_start + size_in_words*idx;
}

bool pinfo::isSentinel(unsigned long * ptr) {
  return _rep->sentinel_value == *ptr;
}

bool pinfo::IDhasData(int which) {
  assert(which < _rep->global_allocs);
  return pmap(which) != -1 && DXhasData(pmap(which));
}

bool pinfo::DXhasData(int which) {
  assert(which >= 0);
  assert(which < elements);
  return get_bit(has_data_start,which);
}
