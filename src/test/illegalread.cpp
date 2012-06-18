#include <stdlib.h>
#include <stdio.h>

int main()
{
  char * ch = (char *) malloc(1);
  for (int i = 0; i < 100; i++) {
    printf ("Yay! I read something!\n");
    for (int j = 0; j < 500; j++) {
      printf ("%c", (char) ch[i]);
    }
    free (ch);
    ch = (char *) malloc(1);
    printf ("\n");
  }
  return 0;
}
