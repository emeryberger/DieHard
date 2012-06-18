#include <iostream>
#include <string.h>
using namespace std;

int main()
{
  char * ch = new char[16];
  strcpy (ch, "Hello, world!\n");
  cout << "string = " << ch << endl;
  cout << "  addr = " << (void *) ch << endl;
  delete [] ch;
  char * ch2 = new char[16];
  strcpy (ch2, "Goodbye, mundo!\n");
  cout << "string = " << ch << endl;
  cout << "  addr (ch2) = " << (void *) ch2 << endl;
  return 0;
}
