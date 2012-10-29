#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char * argv[]) {
  unsigned int k, ret;
  int j = 2;

  if (argc < 2) {
    printf("usage: %s mem_size(MB) -t sleep_time -i initial size(MB) \
            -g growth size(MB)  -f final sleep time\n", argv[0]);
    exit(1);
  }

  unsigned int totalSize    = atoi(argv[1]);  // total memory wanted
  unsigned int initSize     = totalSize;      // initial size to be locked
  unsigned int growthSize   = 1;              // 1MB
  unsigned int intervalTime = 30;             // 2 seconds
  unsigned int sleepTime    = 60;             // 600 seconds

  // Parse commandline options
  while (j < argc) {
    if (strcmp(argv[j], "-t") == 0)
      intervalTime = atoi(argv[++j]);
    else if (strcmp(argv[j], "-i") == 0)
      initSize     = atoi(argv[++j]);
    else if (strcmp(argv[j], "-g") == 0)
      growthSize   = atoi(argv[++j]);
    else if (strcmp(argv[j], "-f") == 0)
      sleepTime    = atoi(argv[++j]);
    j++;
  }

  size_t currentBytes = (size_t)initSize << 20;
  size_t totalBytes   = (size_t)totalSize << 20;
  size_t growthBytes  = (size_t)growthSize << 20;

/*   size_t currentBytes = initSize << 20; */
/*   size_t totalBytes   = totalSize << 20; */
/*   size_t growthBytes  = growthSize << 20; */


  // print out settings
  printf("=================  Setting  ===============\n");
  printf("   Total memory to be mlocked:    %uMB\n", totalSize);
  printf("   Initially touched and mlocked: %uMB\n", initSize);
  printf("   Grow %uMB every %u seconds\n", growthSize, intervalTime);
  printf("   Finally sleep for %u seconds.\n", sleepTime);
  printf("===============================================\n");


  char * array = (char *)mmap(NULL, totalBytes, 
                              PROT_READ|PROT_WRITE|PROT_EXEC, 
                              MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
  if (!array) {
    printf("Allocation of %dMB with mmap failed\n", totalSize);
    exit(1);
  }

  printf("Touching initial %zd bytes...\n", currentBytes);
  for (k = 0; k < currentBytes; k += 4096) {
    array[k] = 1;
  }
  printf("Mlocking initial %zd bytes...\n", currentBytes);

  ret = mlock(array, currentBytes);

  if (ret) {
    printf("mlock failed: %d errno: %d -> %s \n", 
             ret, errno, strerror(errno));
    exit(1);
  }
/*   printf("Touching initial %zd bytes again...\n", currentBytes); */

/*   for (k = 0; k < currentBytes; k += 4096) { */
/*     array[k] = 1; */
/*   } */

  printf("Done\n");

  fd_set set;
  struct timeval timeout;
  //printf("Start working! ... \n");
  while (currentBytes < totalBytes) {
    unsigned int i, h;

    FD_ZERO(&set);
    FD_SET(0, &set);
    timeout.tv_sec = intervalTime;
    timeout.tv_usec = 0;
    select(FD_SETSIZE, &set, NULL, NULL, &timeout);

    for (h = 0; h < currentBytes; h+=4096) {
      array[h] = 'm';
    }

    currentBytes += growthBytes;
    currentBytes = (currentBytes <= totalBytes) ? 
                    currentBytes : totalBytes;  
    //printf("  Touching and mlocking %ld bytes...", currentBytes);
    for (i = h; i < currentBytes; i+=4096) {
      array[i] = 'm';
    }

    ret = mlock((array + h), growthBytes);

    if (ret) {
      printf("mlock failed: %d errno: %d -> %s ", 
             ret, errno, strerror(errno));
    }
    //printf("Done\n");
  }

  //printf("Final sleep ... \n");
  while (1) {
    unsigned int i;

    FD_ZERO(&set);
    timeout.tv_sec = sleepTime;
    timeout.tv_usec = 0;
    select(0, &set, NULL, NULL, &timeout);

    //printf("Waked up, touch again ... ");
    for (i = 0; i < totalBytes; i+=4096) {
      array[i] = 'm';
    }
    //printf("sleep again\n");

  }
}
