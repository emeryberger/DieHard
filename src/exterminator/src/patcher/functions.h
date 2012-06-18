#ifndef __FUNCS_H__
#define __FUNCS_H__

#include <cstdlib>
#include <vector>
#include <set>
#include <map>
using namespace std;

#include "worklist.h"

enum status { OK = (1 << sizeof(void *))-1, PTR_DIFF_ID, SENTINEL, ALLOC_ONLY_ONE, DNE, CALLSITE_MISMATCH };

// Overflow functions

void do_overflow(unsigned long callsite, size_t delta);
bool check_overflow(worklist_item item);

// Dangling pointer functions
void do_dang_ptr(unsigned long, unsigned long, int);
//status compare_alloc_ids(unsigned long w1, unsigned long w2,int r1, int r2);
//status check_dang_ptr(worklist_item item);
void dang_ptr_read_search(const map<pair<int,int>,pair<int,int> > & canary_score);

// driver functions
bool get_bit(unsigned long * bstart, unsigned int index);
status compare_range(unsigned long * v1, unsigned long *v2, pinfo * p1, pinfo * p2, size_t num_words,bool allocated);
status compare_words(unsigned long * v1, unsigned long * v2, pinfo * p1, pinfo * p2, bool check_ids);
status compare_slot(const std::vector<struct pinfo *> & partitions, int oid);
std::vector<bool> create_mask(pinfo *, const std::vector<pinfo *> &, int);
bool check_sentinel(pinfo * p, int dx);

// numerics
double eval_probability(int h, int t);
#endif 
