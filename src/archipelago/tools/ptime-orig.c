#define _GNU_SOURCE  // for asprintf()

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>  // for PAGE_SIZE
#include <sys/wait.h>
#include <unistd.h>

void fatal(const char *s) {
  fprintf(stderr, "fatal error: %s\n", s);
  exit(1);
}

int writeable_size(const char *maps_name) {
  FILE *maps = fopen(maps_name, "r");
  if (!maps)
    return 0;
  
  unsigned writeable = 0;
  while (true) {
    unsigned start_addr, end_addr;
    char permission[4];
    if (fscanf(maps, "%8x-%8x %4c", &start_addr, &end_addr, permission) != 3)
      break;
    if (permission[1] == 'w' && permission[3] == 'p')
      writeable += (end_addr - start_addr);
    while (fgetc(maps) != '\n')
      ;
  }
  
  fclose(maps);
  return writeable;
}

unsigned umax(unsigned u, unsigned v) {
  return (u > v ? u : v);
}

float timeval_to_sec(struct timeval *tv) {
  return tv->tv_sec + tv->tv_usec / 1000000.0f;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    puts("usage: ptime <program> [arg...]");
    return 1;
  }
  
  pid_t pid = fork();
  if (!pid)  // child process
    return execvp(argv[1], &argv[1]);
  
  char *statm_name;
  asprintf(&statm_name, "/proc/%d/statm", pid);
  char *maps_name;
  asprintf(&maps_name, "/proc/%d/maps", pid);
  
  unsigned max_vm = 0, max_rss = 0, max_writeable = 0;
  struct rusage rusage;

  while (true) {
    pid_t p1 = wait4(pid, NULL, WNOHANG, &rusage);
    if (p1 == -1)
      fatal("wait4");
    if (p1 != 0)
      break;
    
    sleep(1);
    
    FILE *statm = fopen(statm_name, "r");
    if (statm != NULL) {
      int vm, rss;
      if (fscanf(statm, "%d %d", &vm, &rss) == 2) {
        max_vm = umax(max_vm, vm * PAGE_SIZE);
        max_rss = umax(max_rss, rss * PAGE_SIZE);
      }
      fclose(statm);
    }
    max_writeable = umax(max_writeable, writeable_size(maps_name));
  }
  
  float cpu_time = timeval_to_sec(&rusage.ru_utime) +
                   timeval_to_sec(&rusage.ru_stime);
  printf("cpu = %.2f sec\n", cpu_time);
  printf("virtual = %uk, resident = %uk, writeable = %uk\n",
         max_vm / 1024, max_rss / 1024, max_writeable / 1024);
  
  return 0;
}
