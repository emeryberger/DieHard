#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#include <iostream>

using namespace std;

#include "mmapwrapper.h"


typedef struct {
  int freed;
  int allocated;
} oracleRecord;

oracleRecord * oracle;
int oracleLength;

main()
{
  // Read in the log file.
  int logfile = open ("log", O_RDONLY);
  struct stat statbuf;
  fstat (logfile, &statbuf);
  oracle = (oracleRecord *) MmapWrapper::map (statbuf.st_size);
  oracleLength = read (logfile, oracle, statbuf.st_size) / sizeof(oracleRecord);
  close (logfile);
  for (int i = 0; i < oracleLength; i++) {
    cout << oracle[i].freed << " " << oracle[i].allocated << endl;
  }
}
