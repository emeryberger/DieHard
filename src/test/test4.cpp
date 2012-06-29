#include <string.h>
#include <iostream>
using namespace std;

char sad[] = "don't be sad.\n";
char happy[] = "just be happy.\n";
const size_t bufferSize = 8;

int main() {
  char * ch = new char[bufferSize];
  strcpy (ch, sad);
  cout << "ch = " << ch << endl;
  delete [] ch;

  cout << "deleted." << endl;

  char * f = new char[bufferSize];
  cout << "&f[8] = " << &f[8] << endl;

  ch = new char[bufferSize];
  strcpy (ch, happy);
  cout << "ch = " << ch << endl;
  delete [] ch;

  char * g = new char[bufferSize];
  cout << "g = " << g << endl;
  cout << "&g[8] = " << &g[8] << endl;
  return 0;
}
