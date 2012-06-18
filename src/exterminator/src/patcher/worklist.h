#ifndef __WORKLIST_H__
#define __WORKLIST_H__

#include <vector>

struct worklist_item {
  std::vector<struct pinfo *> pgroup;
  struct pinfo * perr;
  int oid;
  int dx;
  std::vector<bool> mask;
};

#endif // __WORKLIST_H__
