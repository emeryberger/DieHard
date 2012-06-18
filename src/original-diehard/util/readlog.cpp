#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>

#include "mmapwrapper.h"

typedef struct {
  int freed;
  int allocated;
} oracleRecord;

#include <iostream>
using namespace std;

int compareFunction (const void * left, const void * right) {
  oracleRecord * o1 = (oracleRecord *) left;
  oracleRecord * o2 = (oracleRecord *) right;
  if (o1->freed < o2->freed) {
    return -1;
  } else if (o1->freed > o2->freed) {
    return 1;
  } else {
    return 0;
  }
}

main()
{
  oracleRecord * oracle;
  int logfile = open ("log", O_RDONLY);
  struct stat statbuf;
  fstat (logfile, &statbuf);
  oracle = (oracleRecord *) MmapWrapper::map (statbuf.st_size);
  int oracleLength = read (logfile, oracle, statbuf.st_size) / sizeof(oracleRecord);
  close (logfile);

  for (int i = 0; i < oracleLength; i++) {
    cout << "(" << i << ") allocated: " << oracle[i].allocated << ", freed: " << oracle[i].freed << endl;
  }


}
