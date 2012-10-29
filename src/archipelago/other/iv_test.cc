#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "iv.h"

#define PAGESIZE        4096
#define BALOON_SIZE     1024*1024*1024
#define BALOON_PAGES    256*1024
//#define SIGVM           35
//#define VM_SWAPPING_OUT 0x1

typedef char mypage_t[4096];  

int main() {
  int i;
  size_t *firstWord;
  mypage_t *baloon;

  // QIH::IVMonitor *mon = QIH::IVMonitor::getInstance();

  baloon = (mypage_t*)mmap(NULL, BALOON_SIZE, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (baloon == MAP_FAILED)
    printf("mmap of %d pages failes\n", BALOON_PAGES);


  // mon->start();

  for (i = 0; i < BALOON_PAGES; i+=8) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xDEADBEEF;
  }

  //mon->end();

  // printf("After the first iteration, encountered %d page stalls\n", 
  // mon->getStallCount());

  //mon->reset();

  //  mon->start();

  for (i = 1; i < BALOON_PAGES; i+=8) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xBEEFDEAD;
  }

  //mon->end();

  //  printf("After the second iteration, encountered %d page stalls\n", 
  // mon->getStallCount());

  //mon->reset();

  //  mon->start();

  for (i = 2; i < BALOON_PAGES; i+=8) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xBEEFDEAD;
  }

  //mon->end();

  //  printf("After the third iteration, encountered %d page stalls\n", 
  // mon->getStallCount());

  //mon->reset();

  //  mon->start();

  for (i = 3; i < BALOON_PAGES; i+=8) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xBEEFDEAD;
  }

  //mon->end();

  //  printf("After the fourth iteration, encounterd %d page stalls\n", 
  // mon->getStallCount());

  //mon->reset();

  //  mon->start();

  for (i = 4; i < BALOON_PAGES; i+=8) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xBEEFDEAD;
  }

  //mon->end();

  //  printf("After the fifth iteration, encounterd %d page stalls\n", 
  // mon->getStallCount());

  //mon->reset();
    
  // mon->start();

  for (i = 5; i < BALOON_PAGES; i+=8) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xBEEFDEAD;
  }

  // mon->end();

  //  printf("After the sixth iteration, encounterd %d page stalls\n", 
  // mon->getStallCount());

  //mon->reset();

  // mon->start();

  for (i = 6; i < BALOON_PAGES; i+=8) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xBEEFDEAD;
  }

  //mon->end();

  //printf("After the seventh iteration, encounterd %d page stalls\n", 
  // mon->getStallCount());

  // mon->reset();

  //mon->start();

  for (i = 7; i < BALOON_PAGES; i+=8) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xBEEFDEAD;
  }

  //mon->end();

  //printf("After the eightth iteration, encounterd %d page stalls\n", 
  // mon->getStallCount());



  munmap((void*)baloon, BALOON_SIZE);

  return 0;
}
