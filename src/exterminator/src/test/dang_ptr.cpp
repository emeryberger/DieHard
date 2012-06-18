#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>

#define ITERS 100

main()
{
  int ** stuff = new int*[ITERS];
  for(int i = 0; i < ITERS; i++) {
    int * ch = (int *) malloc(16);
    for(int j = 0; j < 4; j++) {
      ch[j] = i+1; 
    }
    stuff[i] = ch;
    free(ch);
  }
 
  printf("phase 2\n");
 
  for(int i = 0; i < ITERS; i++) {
    int * ch = (int *) malloc(16);
    for(int j = 0; j < 4; j++) {
      ch[j] = i+1; 
    }
  }
  
  for(int i = 0; i < ITERS; i++) {
    int * ch = stuff[i];
    for(int j = 0; j < 3; j++) {
      ch[j] = i+23; 
    }
  }

  for(int i = 0; i < ITERS*10; i++) {
    int * ch = (int *) malloc(16);
    for(int j = 0; j < 4; j++) {
      ch[j] = i+1;
    }
  } 

  printf("done\n");

  raise(SIGSEGV);

}
