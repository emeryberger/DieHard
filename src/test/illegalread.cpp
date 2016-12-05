#include <stdlib.h>
#include <stdio.h>

int main()
{
  const int len = 4;
  char * ch = (char *) malloc(len);
  for (int i = 0; i < 100; i++) {
    printf ("Object at %p: ", (void *) ch);
    for (int j = 0; j < 500; j++) {
      printf ("%d ", ch[j % len]);
    }
    printf ("\n");
    free (ch);
    ch = (char *) malloc(len);
  }
  return 0;
}
