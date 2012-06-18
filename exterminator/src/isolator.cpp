/**
 * @file   isolator.cpp
 * @brief  For the replicated version of Exterminator: runs the isolator
 * @author Gene Novark <http://www.cs.umass.edu/~emery>
 * @note   Copyright (C) 2006 by Emery Berger, University of Massachusetts Amherst.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>

#include "selector.h"

// race race race

static int ct;

void runPatcher() {
  printf("running patcher\n");

  char buf[256];
  sprintf(buf,"./patcher/patcher DHcore-%d-*",harnessPid);
  system(buf);
  
  kill(getppid(),SIGUSR2);

  printf("isolator completed\n");

  exit(0);
}

void dumpedCallback(int signum) {
  int tmp_ct = ct-1; //__sync_sub_and_fetch(&ct,1); 
  
  fprintf(stderr,"ct now: %d\n",ct);

  if(tmp_ct == 0) {
    runPatcher();
  }
}

void isolator() {
  signal(SIGUSR1,dumpedCallback);

  fprintf(stderr,"***running isolator %d\n", getpid());

  int ct = NUMBER_OF_REPLICAS;

  for(int i = 0; i < NUMBER_OF_REPLICAS; i++) {
    int foo = kill(executablePid[i],SIGUSR1);
    if(foo) {
      if(--ct == 0) { //__sync_sub_and_fetch(&ct,1) == 0) {
        runPatcher();
      }
    }
  }

  while(1) pause();
}
