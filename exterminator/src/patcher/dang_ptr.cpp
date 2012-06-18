#include <cstdio>
#include <set>
#include <algorithm>
#include <iterator>
#include <cmath>
using namespace std;

#include "heap_structure.h"
#include "functions.h"
#include "patch_db.h"

void do_dang_ptr(unsigned long allocSite, unsigned long freeSite, int td) {  
  if(td == 0) td = 1;
  td *= 2;
  if(has_dangle_patch(allocSite,freeSite)) {
    dangle_element dangle = get_dangle_patch(allocSite, freeSite);
    if(dangle.get_td() > td) return;
  }
   
  fprintf(stderr, "dangling pointer error from alloc 0x%x, free 0x%x\n",allocSite,freeSite);
  add_dangle_patch(dangle_element(allocSite,freeSite,td));
}

void dang_ptr_read_search(const map<pair<int,int>, pair<int,int> > & canary_score) {

  for(map<pair<int,int>, pair<int,int> >::const_iterator i = canary_score.begin();
      i != canary_score.end(); i++) {

    pair<int,int> id = i->first;
    int obs = i->second.first;
    int h   = i->second.second;
    int t   = obs-h;

    double conf = eval_probability(h,t);

    if(conf < 0.0001) {
      fprintf(stderr,"confidence %f, %d %d (h = %d, t = %d), 0x%x\n",conf,id.first,id.second,h,t,
              replicas[replicas.size()-1]->partitions[id.first].IDtoVA(id.second));
    }      

    unsigned int aSite = replicas[0]->partitions[id.first].IDtoAllocSite(id.second);
    unsigned int dSite = replicas[0]->partitions[id.first].IDtoFreeSite(id.second);

    if(dSite != 0)
      update_static_info(make_pair(aSite,dSite),h,t,replicas[0]->global_allocs - id.second);

    // XXX: Why 1 in 2 million?
    if(conf < 0.0000005) {
      fprintf(stderr,"dangled oid ostensibly %d . %d\n",id.first,id.second);
      do_dang_ptr(aSite,dSite,replicas[0]->global_allocs - id.second);
    }
  }

  inspect_static_info();
}

/*
void dang_ptr_read_search(const vector<set<pair<int,int> > > & dead_canaries) {
  int i;
  for(i = 0; i < replicas.size(); i++) {
    if(replicas[i].isBadReplica()) break;
  }

  if(i == replicas.size()) return;

  // candidates first contains every object which is dead on all bad replicas

  set<pair<int,int> > candidates = dead_canaries[i];

  for(; i < replicas.size(); i++) {
    if(!replicas[i].isBadReplica()) continue;
    
    set<pair<int,int> > tmp(candidates);
    candidates.clear();
    set_intersection(tmp.begin(),tmp.end(),
                     dead_canaries[i].begin(),dead_canaries[i].end(),
                     inserter(candidates,candidates.begin()));
  }

  fprintf(stderr,"sizeof int set is %d\n",candidates.size());
  
  // now remove objects which were bad on some good replica

  for(i = 0; i < replicas.size(); i++) {
    fprintf(stderr,"set %d of size %d\n",i,dead_canaries[i].size());

    if(replicas[i].isBadReplica()) continue;

    for(set<pair<int,int> >::const_iterator j = dead_canaries[i].begin(); j != dead_canaries[i].end(); j++) {
      candidates.erase(*j);
    }
  }

  fprintf(stderr,"candidate set size (final) is: %d\n",candidates.size());

  if(candidates.size() == 1) {
    int pnum = (*candidates.begin()).first;
    int oid = (*candidates.begin()).second;
    fprintf(stderr,"dangled oid ostensibly %d\n",oid);
    unsigned int aSite = replicas[0].partitions[pnum].IDtoAllocSite(oid);
    unsigned int dSite = replicas[0].partitions[pnum].IDtoFreeSite(oid);
    do_dang_ptr(aSite,dSite,replicas[0].partitions[pnum].allocated - oid);
  }
}
*/
