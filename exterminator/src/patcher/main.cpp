#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
using namespace std;

#include "functions.h"
#include "heap_structure.h"
#include "patch_db.h"
#include "worklist.h"

#define BITSHIFT 5

void trap() { raise(SIGTRAP); }

// Get a given bit in a bitmap
bool get_bit(unsigned long * bstart, unsigned int index) {
  const int item = index >> BITSHIFT;
  const int position = index - (item << BITSHIFT);
  const unsigned long mask = (1 << position);

  return bstart[item] & mask;
}

vector<replica *> replicas;
vector<vector<struct pinfo *> > partition_groups;

// Returns true iff p is a singleton class
bool is_outlier(const pinfo * p, const vector<vector<struct pinfo *> > & eq) {
  for(int i = 0; i < eq.size(); i++) {
    if(eq[i].size() == 1 && eq[i][0] == p) return true;
  }
  return false;
}

/**
 * Compares two words for equivalence. Two words are equivalent if
 * they are the same value, or they are valid pointers and the
 * allocation id of the objects pointed to are the same.
 * 
 * This function also finds pointers that point to objects which have
 * been overwritten by a new allocation, and checks if a dangling
 * pointer error is possible.
 */
status compare_words(unsigned long * v1, unsigned long * v2, pinfo * p1, pinfo * p2, bool allocated) {
  unsigned long w1 = *v1;
  unsigned long w2 = *v2;

  int r1 = p1->replica_id;
  int r2 = p2->replica_id;

  // ``common'' case: the values are the same, meaning that it's 
  // equivalent non-pointer data.
  if(w1 == w2) {
    return OK;
  }

  if(!allocated) {
    if(p1->isSentinel(v1) && p2->isSentinel(v2)) return OK;
  }
   
  // Are the values valid pointer addresses (into the DieHard heap) in
  // the respective replicas?
  if(replicas[r1]->valid_pointer(w1) && replicas[r2]->valid_pointer(w2)) {
    // Is the object referenced the same in both replicas?
    pair<int,int> d1 = replicas[r1]->alloc_desc(w1);
    pair<int,int> d2 = replicas[r2]->alloc_desc(w2);
    if(d1 == d2 && d1.second > 0) {
      // if these are large objects, then continue, since we don't have alloc maps for them
      if(d1.second == d2.second && d1.first == 12) return OK;

      // Are we looking at an allocated object pointing to freed objects?
      pinfo * pt1 = &replicas[r1]->partitions[d1.first];
      pinfo * pt2 = &replicas[r2]->partitions[d2.first];
      if(allocated && !pt1->IDallocated(d1.second)) {
        //        fprintf(stderr,"mark\n");
      }
      if(allocated && !pt2->IDallocated(d2.second)) {
        //fprintf(stderr,"mark\n");
      }
      
      return OK;
    } else if (
	       (replicas[r1]->partitions[d1.first].PointerToVA(v1) - w1) ==
	       (replicas[r2]->partitions[d2.first].PointerToVA(v2) - w2)
	       ) {
      return OK;
    } else if(d1.second != 0 && d2.second != 0) {
      int q1 = replicas[r1]->partitions[d1.first].size-1;
      if((w1 & q1)  ==  
         (w2 & q1)) {
        // The allocation ids are different! Probably a dangling pointer error.
        // But we'll have free space corruption on the other replica, and this is too fragile.
        /*
        if(allocated) 
          return compare_alloc_ids(*v1,*v2,r1,r2);
        else */ 
          return PTR_DIFF_ID;
      }
    }
  }
  // Otherwise, assume the values are pointers into regions which are
  // not managed by DieHard.  Then we compute the canonical pointer
  // address (which ignores the randomized dynamic library loading).
  // If these are the same, then we assume the values are pointers and
  // are equivalent.  The probability of this test reporting a false
  // negative are equivalent to the probability of the values being
  // equal and being a false negative.
  else if(replicas[r1]->canonical_ptr(w1) == 
          replicas[r2]->canonical_ptr(w2)) {
    //fprintf(stderr,"canonical ptr values check out: 0x%x, 0x%x -> 0x%x\n",
    //        w1, w2, replicas[1].canonical_ptr(w2));
    return OK;
  }

  int ret = 0; 
  char * c1 = (char *)v1;
  char * c2 = (char *)v2;
  for(int i = sizeof(void *)-1; i >= 0; i--) {
    ret <<= 1;
    ret |= (c1[i] == c2[i]);
  }

  return (status)ret;
}

/**
 * Compares all the words in a given range.
 */
status compare_range(unsigned long * v1, unsigned long *v2, pinfo * p1, pinfo * p2, size_t num_words, bool allocated) {
  status st = OK;
  for(int i = 0; i < num_words; i++) {
    status tmp_st = compare_words((v1+i),(v2+i),p1,p2,allocated);
    if(tmp_st != OK) {
      st = tmp_st;
    }
  }
  return st;
}

template <typename T>
struct vec_size_compare : public less<size_t> {
  bool operator()(const T& x, const T & y) {
    return less<size_t>::operator()(x.size(), y.size());
  }
};

vector<vector<struct pinfo *> > eq_partition(const vector<struct pinfo *> & partitions, int oid) {
  vector<vector<struct pinfo *> > eq;

  status st = DNE;

  int i;
  eq.push_back(vector<struct pinfo *>());
  for(i = 0; i < partitions.size(); i++) {
    pinfo * pi = partitions[i];
    if(oid >= pi->_rep->global_allocs) 
      continue;
    if(pi->IDtoDX(oid) == -1)
      continue;
    if(!pi->IDhasData(oid) && check_sentinel(pi,pi->IDtoDX(oid)))
      continue;
    eq[0].push_back(partitions[i]);
    break;
  }
  
  bool allocated = eq[0][0]->IDallocated(oid) && eq[0][0]->IDhasData(oid);

  // partition the partitions (no pun intended) into equivalence classes
  for(i++; i < partitions.size(); i++) {
    pinfo * pi = partitions[i];

    if(oid >= pi->_rep->global_allocs) 
      continue;
    if(partitions[i]->IDtoDX(oid) == -1)
      continue;
    if(!pi->IDhasData(oid) && check_sentinel(pi,pi->IDtoDX(oid)))
      continue;

    int j;
    for(j = 0; j < eq.size(); j++) {
      status tmp_st;
      tmp_st = compare_range(eq[j][0]->IDtoPointer(oid),
                             partitions[i]->IDtoPointer(oid),
                             eq[j][0], partitions[i],
                             partitions[0]->size_in_words,
                             allocated);
      if(OK == tmp_st) {
        eq[j].push_back(partitions[i]);
        break;
      }
      st = min(st,tmp_st);
    }

    if(j == eq.size()) {
      // didn't match any existing partition
      eq.push_back(vector<struct pinfo *>(1,partitions[i]));
    }
  }

  sort(eq.begin(),eq.end(),vec_size_compare<vector<struct pinfo *> >());  

  if(1){
    // For debugging, print out the sizes of the eq classes
    fputs("eq classes: ",stderr);
    for(int i = 0; i < eq.size(); i++) {
      fprintf(stderr,"%d ",eq[i].size());
    }
    fputc('\n',stderr);
  }

  return eq;
}
    
void handle_discrepancy(const vector<struct pinfo *> & partitions, int oid) {
  // the object is invalid, do something about it!  
  vector<vector<struct pinfo *> > eq;

  eq = eq_partition(partitions,oid);

  {
    // XXX: don't handle "weird" cases
    if(eq[0].size() != 1) return;
    if(replicas.size() > 2 && eq.size() == replicas.size() /*&& XXX: what is this: st != PTR_DIFF_ID*/) 
      return;
  }

  for(int i = 0; i < (eq.size()-1) && eq[i].size() == 1; i++)
  {
    worklist_item item;
    //fprintf(stderr,"perr: %d\n",eq[i][0]);
    item.perr = eq[i][0];
    item.pgroup = eq[eq.size()-1];
    item.oid = oid;
    item.dx = item.perr->IDtoDX(oid);
    item.mask = create_mask(item.perr,item.pgroup,item.oid);
    check_overflow(item);
    //    check_dang_ptr(item);
  }
}

vector<bool> create_mask_sentinel(pinfo * p, int dx) {
  vector<bool> mask(p->size,false);
  unsigned long * v0 = p->DXtoPointer(dx);
  for(int i = 0; i < p->size_in_words; i++) {
    if(!p->isSentinel(v0+i)) {
      unsigned long bitmask = *(v0+i) ^ replicas[p->replica_id]->sentinel_value;
      for(int j = 0; j < sizeof(unsigned long); j++) {
        if(bitmask & 0xff) {
          mask[4*i+j] = true;
        }
        bitmask >>= sizeof(unsigned long);
      }
    }
  }
  return mask;
}

bool check_sentinel(pinfo * p, int dx) {
  unsigned long * v0 = p->DXtoPointer(dx);
  for(int i = 0; i < p->size_in_words; i++) {
    if(!p->isSentinel(v0+i)) return false;
  }
  return true;
}

/**
 * Compares all the words in the given allocation index on all the replicas
 */
status compare_slot(const vector<struct pinfo *> & partitions, int oid) {
  status st = OK;
  int k;

  assert(oid > 0);

  struct pinfo * p0 = NULL;
  unsigned long * v0;
  int r0;
  unsigned long a0;

  for(k = 0; k < partitions.size(); k++) {  
    pinfo * pk = partitions[k];
    if(oid >= pk->_rep->global_allocs || pk->IDtoDX(oid) < 0) {
      continue;
    } else if(!pk->IDhasData(oid) && 
              check_sentinel(pk,pk->IDtoDX(oid))) {
      continue;
    }
    p0 = pk;
    v0 = p0->IDtoPointer(oid);
    r0 = p0->replica_id;
    a0 = p0->IDtoAllocSite(oid);
    break;
  }
  
  if(!p0) return DNE;
  if(k == partitions.size()-1) return ALLOC_ONLY_ONE;

  bool allocated = p0->IDallocated(oid);

  for(k++; (st == OK) && k < partitions.size(); k++) {
    pinfo * pk = partitions[k];

    //if(!pk->IDallocated(oid) && replicas[pk->replica_id].isDieFast()) {
    if(oid >= pk->_rep->global_allocs || pk->IDtoDX(oid) < 0) {
      continue;
    } else if(!pk->IDallocated(oid) && 
              replicas[pk->replica_id]->isDieFast() && 
              check_sentinel(pk,pk->IDtoDX(oid))) {
      continue;
    } else if(pk->IDtoAllocSite(oid) != a0) {
      return CALLSITE_MISMATCH;
    } else {
      unsigned long * vk = pk->IDtoPointer(oid);
      status tmp_st = compare_range(v0,vk,p0,pk,p0->size_in_words,allocated);

      // if the object is NOT allocated, then it's allowed to point to
      // objects which differ in ID, because they may have been freed and
      // overwritten correctly
      if(!allocated && tmp_st == PTR_DIFF_ID) tmp_st == OK;

      if(tmp_st != OK) {
        st = tmp_st;
      }
    }
  }

  return st;
}

vector<bool> create_mask(pinfo * perr, const vector<pinfo *> & pgroup, int oid) {
  vector<bool> mask(perr->size,false);
  for(int i = 0; i < perr->size_in_words; i++) {
    unsigned long * verr = perr->IDtoPointer(oid)+i;
    for(int j = 0; j < pgroup.size(); j++) {
      unsigned long * vgood = pgroup[j]->IDtoPointer(oid)+i;
      bool allocated = perr->IDallocated(oid);
      if(OK != compare_words(verr,vgood,perr,pgroup[j],allocated)) {
        unsigned long bitmask = *(verr) ^ *(vgood); 
        for(int k = 0; k < sizeof(unsigned long); k++) {
          if(bitmask & 0xff) {
            mask[4*i+k] = true;
          }
          bitmask >>= sizeof(unsigned long);
        }
      }
    }
  }

  return mask;
}

/**
 * The main heap comparison function.
 */
void compare_heaps() {
  /// XXX fixme
  int min_allocated = replicas[0]->global_allocs;

  // we have to have the same number of partitions in each replica, otherwise we should just give up
  for(int i = 0; i < replicas.size(); i++) {
    assert(replicas[0]->partitions.size() == replicas[i]->partitions.size());
    if(replicas[i]->global_allocs < min_allocated)
      min_allocated = replicas[i]->global_allocs;
  }

  //vector<set<pair<int,int> > > canary_set(replicas.size());
  map<pair<int,int>, pair<int,int> > canary_score;

  // Compare each partition separately
  // PRIMARY LOOP 1:
  // * Compare all object data (live and freed)
  for(int j = 1; j < min_allocated; j++) {
    vector<struct pinfo *> partitions;
    int i = replicas[0]->pmap[j].first;
    for(int k = 0; k < replicas.size(); k++) {
      partitions.push_back(&replicas[k]->partitions[i]);
      
      // Sanity checking.
      // partitions must have same element size
      assert(partitions[0]->size == partitions[k]->size);
      assert(partitions[0]->elements == partitions[k]->elements);
    }
    
    partition_groups.push_back(partitions);

    // for debugger ease
    struct pinfo * p0 = partitions[0];
    struct pinfo * p1 = partitions[1];
    struct pinfo * p2 = partitions[2];

    // Compare the objects for validity
    status st = compare_slot(partitions, j);
    
    if(st == OK || st == ALLOC_ONLY_ONE || st == DNE) { 
      continue;
    }
    
    if(st == CALLSITE_MISMATCH) {
      fprintf(stderr,"Callsite mismatch at alloc id %d\n",j);
      // Object streams have diverged, so future allocations are garbage
      break;
    }
    
    // PTR targets may differ in dead objects due to reuse
    if(st == PTR_DIFF_ID && !p0->IDallocated(j)) continue;
    
    fprintf(stderr,
            "Found heap value discrepancy at alloc id %d\n",
            j);
    
    handle_discrepancy(partitions, j);
  }

  // PRIMARY LOOP 2:
  // * check empty/unallocated space for sentinel value
  for(int i = 0; i < replicas[0]->partitions.size(); i++) {
    vector<struct pinfo *> partitions;
    for(int j = 0; j < replicas.size(); j++)
      partitions.push_back(&replicas[j]->partitions[i]);
      
    for(int j = 0; j < partitions.size(); j++) {
      pinfo * pj = partitions[j];
      for(int k = 0; k < pj->elements; k++) {
        int oid = pj->DXtoID(k);
        
        if(oid) {
          // number of observations
          canary_score[make_pair(i,oid)].first++;
          if( (pj->DXhasData(k) && !replicas[pj->replica_id]->isBadReplica()) ||
              (!pj->DXhasData(k) && replicas[pj->replica_id]->isBadReplica()) )
            // number of points ( E[points] = 0.5 if object not dangled )
            canary_score[make_pair(i,oid)].second++;
        }
          
        if(!pj->DXhasData(k) && !check_sentinel(pj,k)) {
          int canaries;
          status st;
          vector<vector<struct pinfo *> > eq;

          fprintf(stderr,"dead canary\n");

          if(oid) {
            st = compare_slot(partitions,oid);
            eq = eq_partition(partitions,oid);

            canaries = 0;
            for(int l = 0; l < partitions.size(); l++) {
              if(oid >= partitions[l]->_rep->global_allocs) continue;
              if(!partitions[l]->IDhasData(oid))
                canaries++;
            }
          }

          if(!oid || is_outlier(pj,eq)) {
            fprintf(stderr,"corrupted canary, outlier, check for overflow\n");
            worklist_item item;
            item.dx = k;
            item.perr = pj;
            for(int l = 0; l < partitions.size(); l++) {
              if(l != j)
                item.pgroup.push_back(partitions[l]);
            }
            
            item.mask = create_mask_sentinel(pj,k);
            check_overflow(item);
          } else if(canaries == 1) {
            fprintf(stderr,"very weird case, only single dead canary but not an outlier!\n");
            fprintf(stderr,"this can happen when an object is completely overwritten\n");
            assert(false);
          } else {
            // multiple canaries corrupted in same way, so probabilistically a dangling pointer
            // XXX: base on free time
            fprintf(stderr, "multiple canaries, same corruption, so dangling pointer\n");
            do_dang_ptr(pj->IDtoAllocSite(oid),pj->IDtoFreeSite(oid),
                        (pj->_rep->global_allocs - pj->IDtoFreeTime(oid)));
          }
        }
      } 
    }
  }

  dang_ptr_read_search(canary_score);
}

int main(int argc, char* argv[]) {
  if(argc < 2) {
    fprintf(stderr,"Usage: %s <cores>\n",argv[0]);
    exit(-1);
  }

  const char * dh_patchfile = getenv("DH_PATCHFILE");

  read_patch();

  replicas.reserve(argc-1);

  for(int i = 0; i < argc-1; i++) {
    char *core_name = argv[i+1];
    int file = open(core_name,O_RDONLY);
    if(file == -1) {
      perror("Couldn't open core file:");
      exit(-1);
    }
    
    struct stat stat_buf;
    fstat(file,&stat_buf);
    
    void *core = mmap(0,stat_buf.st_size,PROT_READ,MAP_SHARED,file,0);
    if(core == MAP_FAILED) {
      perror("Couldn't mmap core file:");
    }
    
    replicas.push_back(new replica());
    replicas[i]->replica_id = i;
    
    replicas[i]->read_core(core);
  }

  compare_heaps();

  output_patch();
}
