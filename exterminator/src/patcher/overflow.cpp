#include <cstdio>
#include <iostream>
#include <cassert>
#include <limits>
#include "functions.h"
#include "patch_db.h"
#include "heap_structure.h"
#include "worklist.h"

/*
 * Overflow algorithm:
 * 
 * Given an OID which is not congruent over all replicas, the ``error'' replicas and a set of ``good'' replicas, 
 */

void print_object(pinfo * p, int dx) {
  unsigned long * start = p->DXtoPointer(dx);
  for(int i = 0; i < p->size_in_words; i++) {
    printf("%d: 0x%x ",p->replica_id,*(start+i));
    if(p->_rep->valid_pointer(*(start+i))) {
      pair<int,unsigned long> desc = p->_rep->alloc_desc(*(start+i));
      printf("= { %d %d }",desc.first, desc.second);
    }
    printf("\n");
  }
}

double correlation(const vector<struct pinfo *> pgroup, pinfo * perr, int oid, int offset, const vector<bool> & mask) {
  double ret = 1.0;

  // Victim start address
  char * O_err_start = (char*)perr->DXtoPointer(perr->IDtoDX(oid)+offset);

  for(int i = 0; i < pgroup.size(); i++) {
    int ct = 0;

    pinfo * pi = pgroup[i];
    assert(pi != perr);

    // if oid doesn't exist in pi
    if(oid >= pi->_rep->global_allocs) continue;

    // Potential victim DX in replica i
    int dx = pi->IDtoDX(oid) + offset;
    if(dx >= pi->elements) {
      //fprintf(stderr,"dx out of heap range\n");
      continue;
    }

    // IDtoDX returns -1, object DNE
    if(dx - offset < 0) continue;

    // In worst case, can't say anything about these
    if(pi->DXhasData(dx)) continue;

    /*
      XXX: update this to do what I want, i.e. update the mask
    if(pi->DXtoID(dx) && pgroup.size() > 1) {
      // If potential victim is allocated
      // and checks out on all the good replicas
      status st = compare_slot(pgroup,pgroup[i]->DXtoID(dx));
      if(st == OK) continue;
    }
    */

    char * O_i_start = (char*)pi->DXtoPointer(dx);
    
    for(int k = 0; k < perr->size;) {
      if( !(k % 4) && pi->isSentinel((unsigned long *)(O_i_start+k)) ) {
        k+=4;
        continue;
      }

      if(mask[k] && *(O_err_start+k) == *(O_i_start+k) ) {
        ret *= 1.0/256.0;
      }
      
      k++;
    }
  }

  if(offset == 1) fprintf(stderr,"returning %f\n",1.0-ret);

  return 1.0-ret;
}

/*
  Policy decision required here: If we have more than 2 replicas, the
  overflow's victim bytes may differ between replicas, since a
  victimized object may later write a value to the memory which was
  overwritten.  We need some soundly-motivated way of dealing with
  this sort of discrepancy, since the probability of such things
  happening tends to infinity with the number of replicas.

  Current policy: report the largest byte which is consistent across ALL replicas
  Not scalable!
*/
int overflow_offset(const vector<struct pinfo *> pgroup, pinfo * perr, int oid, int offset, const vector<bool> & mask) {
  int ct = numeric_limits<int>::max();
  
  unsigned long * O_err_start = perr->DXtoPointer(perr->IDtoDX(oid)+offset);

  for(int i = 0; i < pgroup.size(); i++) {
    pinfo *pi = pgroup[i];

    if(pi == perr) continue;

    int acc = 0;

    // if oid doesn't exist in pi
    if(oid >= pi->_rep->global_allocs) continue;

    int dx = pgroup[i]->IDtoDX(oid);
    if(dx == -1) continue;
    dx += offset;
    if(dx >= pgroup[i]->elements) {
      //fprintf(stderr,"dx out of heap range\n");
      continue;
    }

    // if we can reasonably suspect that the overflowed area was overwritten by a legal alloc, then continue
    if(pi->DXtoID(dx) > oid) {
      continue;
    }

    // XXX: extra sanity checking should be somewhere else
    //    assert(pgroup[i]->IDtoAllocSite(oid) == perr->IDtoAllocSite(oid));

    unsigned long * O_i_start = pgroup[i]->DXtoPointer(dx);
    bool is_sentinel = pgroup[i]->DXtoID(dx) == 0;
    
    for(int k = 0; k < perr->size_in_words; k++) {
      // XXX FIXME
      if(!mask[4*k]) continue;

      if(is_sentinel && pi->isSentinel(O_i_start+k)) continue;

      int st = (int)compare_words((O_err_start + k),(O_i_start + k),
                                  perr, pgroup[i], pgroup[i]->DXallocated(dx));
      if(st == PTR_DIFF_ID) st = 0;

      if(st == OK) {
        acc += sizeof(void*);
      } else {
        //fprintf(stderr,"got status %x\n",st);
        while(st & 1) {
          st >>= 1; // clear the least significant bit set
          acc++;
        }
        break;
      }
    }

    if(acc < ct) ct = acc;
  }

  return (ct == numeric_limits<int>::max() ? 0 : ct);
}

// DO NOT USE item.oid in this function
bool check_overflow(worklist_item item) {
  struct pinfo * perr = item.perr;
  //size_t size = partition->size_in_words;
  int dx = item.dx;

  if(item.pgroup.size() < 2) return false;

  // XXX: ?
  //assert(perr->DXallocated(dx));

  double correl_threshold = 0.0; // (double)(item.pgroup.size()-1);

  double best_c = correl_threshold;
  int best_j;
  int best_d;

  // search for forward overflows
  for(int j = dx-1; j >= 0; j--) {
    /// XXX: handle case where overflow target has been freed
    //if(!perr->DXallocated(j)) continue;
    if(0 == perr->DXtoID(j)) continue;
    status st = compare_slot(item.pgroup,perr->DXtoID(j));

    // st == DNE can occur if the oid has been clobbered in all replicas
    if(st == OK || st == ALLOC_ONLY_ONE || st == DNE) {
      double c = correlation(item.pgroup,perr,perr->DXtoID(j),dx-j, item.mask);
      if(c > best_c) {
        int d = overflow_offset(item.pgroup,perr,perr->DXtoID(j),dx-j, item.mask);
        if(d > 0) {
          best_c = c;
          best_j = j;
          best_d = d;
          fprintf(stderr,"found one after %d, correlation %f, offset %d\n",dx-j,c,d);
        }
      }
    }
  }
  
  if(best_c > correl_threshold) {
    // create a patch
    fprintf(stderr, "overflow patch: %d %d\n",perr->DXtoID(best_j),(dx-best_j-1)*perr->size + best_d);
    do_overflow(perr->DXtoAllocSite(best_j),(dx-best_j-1)*perr->size + best_d);
  }
}

/**
 * Creates a patch for the callsite to pad its allocations with delta bytes
 */
void do_overflow(unsigned long callsite, size_t delta) {
  if(has_overflow_patch(callsite)) {
    overflow_element overflow = get_overflow_patch(callsite);
    if(overflow.get_delta() > delta) return;
  }
   
  //fprintf(stderr, "installing new overflow patch\n");
  add_overflow_patch(overflow_element(callsite,delta));
}
