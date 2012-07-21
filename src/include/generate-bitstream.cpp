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
  char ** arr = new char *[1000000];

#if 1
  // Warm-up period:
  for (int k = 0; k < 100; k++) {
    for (int j = 0; j < 1000000; j++) {
      arr[j] = new char[8];
    }
    for (int j = 0; j < 1000000; j++) {
      delete [] arr[j];
    }
  }
#endif
  const int count = 1000;
  for (int j = 0; j < 1000; j++) {
    for (int i = 0; i < count; i++) {
      arr[i] = new char[8];

      long rnd = lrand48() & ~((1<<HI_INSIG_BITS) - 1);
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
