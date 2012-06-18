#ifndef __HEAP_STRUCTURE_H__
#define __HEAP_STRUCTURE_H__

#include <vector>
#include <map>
using namespace std;

#include "functions.h"
#include "../heap_config.h"

struct slot_info_t {
  unsigned long id;
  unsigned long alloc_callsite;
  unsigned long free_callsite;
  unsigned long free_time;
};

class replica;

// Partition info structure
class pinfo {
 public:
  // element size
  unsigned long size;
  unsigned long size_in_words;
  // number of elements
  unsigned long elements;
  unsigned long * bitmap_start;
  unsigned long * has_data_start;
  slot_info_t   * _slot_info;
  unsigned long * heap_start;
  // the lowest VA (in the program, not this reader) contained in this partition
  unsigned long vaddr_start;
  unsigned long vaddr_end;

  int replica_id;
  replica * _rep;

public:
  /*
   * VA: the virtual address in the original running replica
   * Pointer: the location of the word in this address space
   * Callsite: The address of the instruction which calls malloc in the original replica
   * ID: allocation id
   */
  pinfo(replica * rep) : _rep(rep) {
    fprintf(stderr,"new pinfo, _rep : 0x%x\n",_rep);
  }

  unsigned long VAtoID(unsigned long ptr);

  unsigned long VAtoAllocSite(unsigned long ptr);
  unsigned long VAtoFreeSite(unsigned long ptr);

  unsigned long * VAtoPointer(unsigned long ptr);
  unsigned long PointerToVA(unsigned long * ptr);

  bool IDallocated(int which);

  bool DXallocated(int which);

  unsigned long IDtoAllocSite(int idx);
  unsigned long DXtoAllocSite(int idx);

  unsigned long IDtoFreeSite(int idx);
  unsigned long DXtoFreeSite(int idx);

  unsigned long IDtoFreeTime(int idx);
  unsigned long DXtoFreeTime(int idx);

  int IDtoDX(int id);

  int DXtoID(int idx);

  unsigned long IDtoVA(int idx);

  unsigned long * IDtoPointer(int idx);

  unsigned long * DXtoPointer(int idx);

  bool isSentinel(unsigned long *);
  bool IDhasData(int);
  bool DXhasData(int);

  int pmap(int idx);
};

// information on a single replica
class replica {
public:
  int replica_id;

  // info on all the partitions
  vector<pinfo> partitions;

  // LOS stuff
  // VA to size map
  map<unsigned long, size_t> los_size;
  // VA to ptr map
  map<unsigned long, unsigned long *> los_map;

  // The "base' of the dynamically loaded library 'segment'
  // used to distinguish PCs in the binary itself vs shared libs
  // Also used as an offset to canonicalize pointers
  unsigned long ptr_base;
  unsigned long stack_base;

  unsigned long heap_config_flags;
  unsigned long global_allocs;
  unsigned long sentinel_value;

  // Is this value a valid pointer for this replica's heap?
  // XXX: verify alignment?
  bool valid_pointer(unsigned long val) {
    for(int i = 0; i < partitions.size(); i++) {
      if(val >= partitions[i].vaddr_start && 
         val <  partitions[i].vaddr_end) {
        return true;
      }
    }
    // does this pointer point into any of the large objects?
    for(map<unsigned long, size_t>::const_iterator j = los_size.begin();
        j != los_size.end(); j++) {
      if(val >= j->first && val < j->first + j->second) {
        return true;
      }
    }
    return false;
  }

  // returns a pair (partition, alloc id) given a VA ptr
  pair<int, unsigned long> alloc_desc(unsigned long val) {
    for(int i = 0; i < partitions.size(); i++) {
      if(val >= partitions[i].vaddr_start && 
         val <  partitions[i].vaddr_end) {
        return make_pair(i,partitions[i].VAtoID(val));
      }
    }
    // does this pointer point into any of the large objects?
    for(map<unsigned long, size_t>::const_iterator j = los_size.begin();
        j != los_size.end(); j++) {
      if(val >= j->first && val < j->first + j->second) {
        // XXX: does this work? no, FIXME
        return make_pair(partitions.size(),0xd33f);
      }
    }
    return make_pair(-1,0);
  }

  // return a canonical representation of the pointer, modulo randomized
  // dynamic library loading addresses
  unsigned long canonical_ptr(unsigned long ptr) {
    unsigned long foo;
    foo = ptr ^ stack_base;
    if(foo < 0xffff) { 
      // close enough
      return stack_base - ptr; 
    }
    return ptr - ptr_base;
  }

  void read_partition(unsigned long * & cursor) {
    fprintf(stderr,"%x\n",this);
    
    struct pinfo info(this);
    
    info.size = *cursor++;
    info.size_in_words = info.size/sizeof(unsigned long);
    info.elements = *cursor++;
    info.vaddr_start = *cursor++;
    info.vaddr_end = info.vaddr_start + (info.size * info.elements);
    //fprintf(stderr, "%d elements of size %d at 0x%x. allocated %d\n",
    //        info.elements, info.size, info.vaddr_start, info.allocated-1);
    // read bitmap
    info.bitmap_start = cursor;
    cursor += info.elements/(sizeof(unsigned long)*8);
    info.has_data_start = cursor;
    cursor += info.elements/(sizeof(unsigned long)*8);
    // read alloc id map
    info._slot_info = (slot_info_t *)cursor;
    cursor += info.elements*4;
    // read actual heap data
    info.heap_start = cursor;
    cursor += info.size_in_words*info.elements;

    info.replica_id = replica_id;

    fprintf(stderr,"partitions.size() == %d\n",partitions.size());
    partitions.push_back(info);
    fprintf(stderr,"partitions.size() == %d\n",partitions.size());
  }

  void verify_canaries() {
    for(int i = 0; i < partitions.size(); i++) {
      struct pinfo * p = &partitions[i];

      for(int k = 0; k < p->elements; k++) {
        if(!p->DXhasData(k) && !check_sentinel(p,k)) {
          //fprintf(stderr,"Dead canary partition %d dx %d, so now BAD replica\n",i,k);
          makeBadReplica();
        }
      }
    }
  }

  void read_los_entry(unsigned long * & cursor) {
    unsigned long vaddr = *cursor++;
    unsigned long size = *cursor++;
    los_size[vaddr] = size;
    los_map[vaddr] = cursor;
    //fprintf(stderr,"los vaddr: 0x%x, size: %d, cursor: 0x%x\n",vaddr,size,cursor);
    cursor += size/sizeof(unsigned long);
  }

  // Builds the map from allocation ID to VA for each partition
  void make_partition_maps() {
    // XXX: this needs to be a dictionary not a vector in order to
    // use a constant amount of space, instead of linear in the
    // number of allocs
    pmap.resize(global_allocs+1,make_pair(-1,-1));

    for(int i = 0; i < partitions.size(); i++) {
      struct pinfo & info = partitions[i];

      int ct = 0;

      for(int j = 0; j < info.elements; j++) {
        /*if(!get_bit(info.bitmap_start,j)) {
          continue;
          }*/

        if(get_bit(info.bitmap_start,j)) {
          ct++;
        }

        int id = info.DXtoID(j);

        assert(id <= global_allocs);

        //fprintf(stderr,"%d: object %d at index %d\n",i,id,j);
        pmap[id] = make_pair(i,j);
      }
      
      fprintf(stderr,"%d live objects\n",ct);
    }
  }

  void read_core(void * core) {
    fprintf(stderr,"replica addr %x\n",this);
    unsigned long * cursor = (unsigned long*)core;
    // read dylib randomization offset
    global_allocs = *cursor++;
    sentinel_value = *cursor++;
    fprintf(stderr,"global allocs: %d\n",global_allocs);
    ptr_base = *cursor++;
    stack_base = *cursor++;
    heap_config_flags = *cursor++;
    fprintf(stderr,"ptr_base: 0x%x\n",ptr_base);
    // read number of partitions
    int number_of_partitions = *cursor++;
    fprintf(stderr,"got %d partitions\n",number_of_partitions);
    for(int i = 0; i < number_of_partitions; i++) {
      read_partition(cursor);
    }
    fprintf(stderr,"los start at 0x%x\n",cursor);
    int num_los_entries = *cursor++;
    fprintf(stderr, "got %d los entries\n",num_los_entries);
    for(int i = 0; i < num_los_entries; i++) {
      read_los_entry(cursor);
    }
    make_partition_maps();
    if(isBadReplica()) {
      printf("BAD replica\n");
    } else {
      printf("GOOD replica\n");
    }
    verify_canaries();
  }

  bool isDieFast() {
    return heap_config_flags & DIEHARD_DIEFAST_FLAG;
  }

  bool isBadReplica() {
    return heap_config_flags & DIEHARD_BAD_REPLICA;
  }

  void makeBadReplica() { 
    heap_config_flags |= DIEHARD_BAD_REPLICA;
  }

  // alloc ID to offset table
  vector<pair<int,unsigned long> > pmap;

};

extern vector<replica *> replicas;

#endif
