#include <stdlib.h>
#include <stdio.h>

main()
{
  char * ch = (char *) malloc(1);
  for (int i = 0; i < 100; i++) {
    free (ch);
    free (ch);
    free ((void *) (ch + 1));
    ch = (char *) malloc(1);
  }

}
