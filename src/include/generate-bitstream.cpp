#include <stdio.h>
#include <stdlib.h>


#include <bitset>
#include <iostream>
#include <iomanip>

using namespace std;

// Parameters for Core Duo.

// 64-byte blocks. (2^6 = 64)
enum { LO_INSIG_BITS = 6 }; 

// 4096 sets. (2^12 = 4096)
enum { HI_SIG_BITS = 12 };

int
main()
{
  char ** arr = new char *[1000000];

#if 0
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
      
      size_t src = (size_t) arr[i];
      //      size_t src = lrand48();

      long val = (src >> LO_INSIG_BITS);

      std::bitset<HI_SIG_BITS> binary (val);

      cout << binary << endl;
    }
    
    for (int i = 0; i < count; i++) {
      delete [] arr[i];
    }
  }
  return 0;
}
