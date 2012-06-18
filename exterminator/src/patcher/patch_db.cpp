/**
 * This file maintains a database of patches to produce.
*/

#include <iostream>
#include <fstream>
#include <vector>
using namespace std;

#include "patch_db.h"
#include "functions.h"

map<unsigned long,overflow_element> overflowPatches;
map<pair<unsigned long, unsigned long>, dangle_element> danglePatches;  
map<pair<unsigned int, unsigned int>, pair<unsigned int, unsigned int> > static_info;
map<pair<unsigned int, unsigned int>, int> drag_map;
vector<pair<unsigned long,unsigned long> > updated_dangles;

/**
 * Append the generated patches to the file specified by the
 * DH_PATCHFILE environment variable.
 */
void output_patch() {
  const char * patch_fname = getenv("DH_PATCHFILE");

  FILE * out = fopen(patch_fname,"w");
  if(!out) {
    perror("could not open patch file for writing: ");
    exit(-1);
  }
  for(map<unsigned long,overflow_element>::iterator i = overflowPatches.begin();
      i != overflowPatches.end(); i++) {
    i->second.write(out);
  }
  for(map<pair<unsigned long, unsigned long>, dangle_element>::iterator i = danglePatches.begin();
      i != danglePatches.end(); i++) {
    i->second.write(out);
  }

  for(int i = 0; i < updated_dangles.size(); i++) {
    static_info.erase(updated_dangles[i]);
  }

  for(map<pair<unsigned int, unsigned int>, pair<unsigned int, unsigned int> >::const_iterator i = static_info.begin(); i != static_info.end(); i++) {
    fprintf(out,"stat 0x%x 0x%x %d %d\n",i->first.first, i->first.second, i->second.first, i->second.second);
  }
}

bool has_overflow_patch(unsigned long callsite) {
  return overflowPatches.find(callsite) != overflowPatches.end();
}

bool has_dangle_patch(unsigned long allocSite, unsigned long freeSite) {
  return danglePatches.find(make_pair(allocSite,freeSite)) != danglePatches.end();
}

/** Gets the patch for the given callsite. */
overflow_element get_overflow_patch(unsigned long callsite) {
  assert(has_overflow_patch(callsite));
  return (overflowPatches.find(callsite))->second;
}

dangle_element get_dangle_patch(unsigned long allocSite, unsigned long freeSite) {
  assert(has_dangle_patch(allocSite,freeSite));
  return (danglePatches.find(make_pair(allocSite,freeSite)))->second;
}

void add_overflow_patch(overflow_element patch) {
  overflowPatches.erase(patch.callsite);
  overflowPatches.insert(make_pair(patch.callsite,patch));
}

void add_dangle_patch(dangle_element patch) {
  danglePatches.erase(make_pair(patch._allocSite,patch._freeSite));
  danglePatches.insert(make_pair(make_pair(patch._allocSite,patch._freeSite), patch));
  updated_dangles.push_back(make_pair(patch._allocSite,patch._freeSite));
}

void update_static_info(pair<unsigned int, unsigned int> key, int h, int t, int drag) {
  static_info[key].first += h;
  static_info[key].second += t;
  if(!drag_map[key] || drag_map[key] > drag) drag_map[key] = drag;
}

void inspect_static_info() {
  for(map<pair<unsigned int, unsigned int>, pair<unsigned int, unsigned int> >::const_iterator i = static_info.begin(); i != static_info.end(); i++) {
    // XXX: threshold
    if(eval_probability(i->second.first,i->second.second) < 0.000001) {
      fprintf(stderr,"*** static threshold reached on 0x%x 0x%x (h = %d, t = %d)\n",
              i->first.first,i->first.second,i->second.first,i->second.second);
      do_dang_ptr(i->first.first,i->first.second,drag_map[i->first]);
    }
  }
}

/** Read in existing patches from file */
void read_patch() {
  const char * patch_fname = getenv("DH_PATCHFILE");
  if(!patch_fname) {
    fprintf(stderr,"No patch file name specified (set DH_PATCHFILE)\n");
    exit(-1);
  }

  ifstream in(patch_fname);

  string type;
  unsigned long alloc_site;

  in >> type >> std::hex >> alloc_site;

  while(in) {
    if(type == "overflow") {
      int padding;
      in >> std::dec >> padding;

      add_overflow_patch(overflow_element(alloc_site,padding));
    } else if(type == "dangle") {
      int td;
      unsigned long free_site;
      in >> std::hex >> free_site >> std::dec >> td;
      add_dangle_patch(dangle_element(alloc_site,free_site,td));
    } else if(type == "stat") {
      unsigned int free_site, h, t;
      in >> std::hex >> free_site >> std::dec >> h >> t;
      update_static_info(make_pair(alloc_site,free_site),h,t,0);
    }
    if(in)
      in >> type >> std::hex >> alloc_site;
  }

}
