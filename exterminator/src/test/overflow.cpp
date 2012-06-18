#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

void foo() {
  fputs("exiting!\n",stderr);
}

int main()
{
  atexit(foo);

  for(int i = 0; i < 1000; i++) {
    char * ch = (char *) malloc(16);
    for(int j = 0; j < 17; j++) {
      ch[j] = i; 
    }
  }

  int foo;

  puts("overflow microbenchmark done\n");
  fflush (stdout);

  //scanf("%d",&foo);
  raise(SIGSEGV);
  return 3;
}
