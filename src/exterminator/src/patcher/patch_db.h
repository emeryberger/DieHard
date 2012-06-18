#ifndef __PATCH_DB_H__
#define __PATCH_DB_H__

#include<map>
using namespace std;

class overflow_element {
public:
  overflow_element(unsigned long cs, size_t d) : callsite(cs), delta(d) {}

  void write(FILE * out) {
    fprintf(out,"overflow 0x%x %d\n",callsite,delta);
  }

  size_t get_delta() { return delta; }

  unsigned long callsite;
  size_t delta;
};

class dangle_element {
public:
  dangle_element(unsigned long as, unsigned long fs, int td) : _allocSite(as), _freeSite(fs),
    _td(td) {}

  void write(FILE * out) {
    fprintf(out,"dangle 0x%x 0x%x %d\n",_allocSite, _freeSite, _td);
  }

  int get_td() { return _td; }

  unsigned long _allocSite, _freeSite;
  int _td;
};

void output_patch();
void add_overflow_patch(overflow_element p);
void add_dangle_patch(dangle_element p);
overflow_element get_overflow_patch(unsigned long callsite);
bool has_overflow_patch(unsigned long);
dangle_element get_dangle_patch(unsigned long, unsigned long);
bool has_dangle_patch(unsigned long, unsigned long);
void update_static_info(pair<unsigned int, unsigned int> key, int h, int t, int drag);
void inspect_static_info();
void read_patch();

#endif
