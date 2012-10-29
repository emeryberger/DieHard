#include <linux/vmcomm.h>
#include <signal.h>
//#include <linux/signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "vmcomm.h"

#define PAGESIZE        4096
#define BALOON_SIZE     1024*1024*1024
#define BALOON_PAGES    256*1024
//#define SIGVM           35
//#define VM_SWAPPING_OUT 0x1

typedef char page_t[4096];

void vm_handler(int signun, siginfo_t *siginfo, void *context) {
  printf("Received a signal\n");
} 

int main() {
  int i;
  size_t *firstWord;
  struct sigaction siga;
  page_t *baloon;

  //if (sigaction(SIGVM, NULL, &siga) == -1) {
  //  printf("Failed to get old signal handler info\n");
  //  exit(1);
  //}
  sigemptyset(&(siga.sa_mask));     
  siga.sa_flags     = SA_SIGINFO;//| SA_NODEFER;
  siga.sa_sigaction = &vm_handler;

  if (sigaction(SIGVM, &siga, NULL) == -1) {
    printf("Failed to set new signal handler\n");
    exit(1);
  }

  if (vm_register(VM_SWAPPING_OUT)) {
    printf("vm_register failed\n");
    exit(1);
  }

  baloon = (page_t*)mmap(NULL, BALOON_SIZE, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (baloon == MAP_FAILED)
    printf("mmap of %d pages failes\n", BALOON_PAGES);

  for (i = 0; i < BALOON_PAGES; i++) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xDEADBEEF;
  }

  for (i = 0; i < BALOON_PAGES; i++) {
    firstWord = (size_t*)(baloon[i]);
    *firstWord = 0xBEEFDEAD;
  }

  munmap((void*)baloon, BALOON_SIZE);

  return 0;
}
