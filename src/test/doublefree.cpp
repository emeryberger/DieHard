#include <iostream>

using namespace std;

#include <stdlib.h>
#include <stdio.h>

int main()
{
  char * ch1 = (char *) malloc(8);
  char * ch = (char *) malloc(8);
  free (ch);
  free (ch);
  for (int i = 0; i < 10; i++) {
    ch = (char *) malloc(8);
    cout << "ch = " << (void *) ch << endl;
  }
  return 0;
}
