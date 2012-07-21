#include <stdio.h>
#include <stdlib.h>


#include <bitset>
#include <iostream>
#include <iomanip>

using namespace std;

// last 4 bits are not significant since objects are aligned to
// 16-byte boundaries.
enum { LO_INSIG_BITS = 4 }; 

// we don't care about the bits above 1000 either.
enum { HI_INSIG_BITS = 5};

int
main()
{
  const int count = 1000;
  char * arr[count];
  for (int j = 0; j < 1000; j++) {
    for (int i = 0; i < count; i++) {
      long rnd = lrand48() & ~((1<<HI_INSIG_BITS) - 1);
      arr[i] = new char[8];
      //      std::bitset<16> binary ((size_t) arr[i] >> 4);
      long val = ((size_t) arr[i] >> LO_INSIG_BITS) & ((1<<HI_INSIG_BITS) - 1);
      std::bitset<16> binary (rnd + val);
      cout << binary << endl;
    }
    
    for (int i = 0; i < count; i++) {
      delete [] arr[i];
    }
  }
  return 0;
}
