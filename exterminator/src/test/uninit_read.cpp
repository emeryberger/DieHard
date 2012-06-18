#include <stdlib.h>
#include <stdio.h>
#include <time.h>

main()
{
  for(int i = 0; i < 1000; i++) {
    char * ch = (char *) malloc(16);
    for(int i = 0; i < 17; i++) {
      ch[i] = random(); 
    }
  }

  int foo;
  scanf("%d",&foo);
}
