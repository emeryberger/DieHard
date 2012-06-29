#include <stdlib.h>
#include <iostream>
using namespace std;

int main() {
  char * ch;
#if 0
  ch = (char *) malloc(8);
  free (ch);
  for (int i = 0; i < 10; i++) {
    cout << malloc(8) << endl;
  }
#endif
  ch = (char *) malloc(8);
  free (ch);
  free (ch);
  for (int i = 0; i < 10; i++) {
    cout << malloc(8) << endl;
  }
  return 0;
}
