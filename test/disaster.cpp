#include <stdio.h>
#include <string.h>
#include <signal.h>

#if defined(_WIN32)
#include <exception>
#endif
 
int c = 0;
const int rpt_rate = 1000;
const int total_iter = 2000; // 2 million
 
void print( int c, const char* s ) {
  if( ! (c%rpt_rate) ) printf("%d. %s",c,s);
}
 
void s(int sig) {
  printf("signal:%d, failed at iteration: %d\n", sig, c );
  fflush(stdout);
}
 
void un() {
  printf("unexpected exception, failed at iteration: %d\n", c );
  fflush(stdout);
}
 
int main() {
  signal( SIGABRT, s );
  signal( SIGFPE , s );
  signal( SIGILL , s );
  signal( SIGSEGV, s );
#if defined(_WIN32)
  std::set_unexpected( un );
#endif
 
  while ( c++<total_iter ) 
    {
      char* str1 = new char[8];
      char* str2 = new char[48];
 
      // Invalid frees.
      // sarbak -- works
      for (int j = 0; j < 10; j++) delete ((int *) &j);
      print(c,"performed invalid frees.\n");
 
      // Heap overflow through a library call.
      // sarbak -- works
      strcpy (str1, str2);
      print(c,"performed heap overflow.\n");
 
      // free of an uninitialized pointer
      //char* uninitializedPtr;
      //delete [] uninitializedPtr;
      // !!!a.) sarbak -- fails immediately -- print(c,"performed frees of an uninitialized ptr.\n");
 
      // Double frees, dangling pointer (premature free).
      // !!!b.) sarbak -- double frees get segv in debug mode in about 2 dozen iterations
      // !!!b.) sarbak -- ditto for release build within 9-18 iterations
#if 1
      delete [] str1;
      delete [] str1;
      delete [] str1;
#endif
      print(c,"performed double frees.\n");
 
      if( ! (c%rpt_rate) ) printf ("%s\n", str2);
 
      // normally, the heap overflows below will not always cause your
      // application to crash, just corruption occurs.
      // sarbak -- works
      char* str3 = new char[8];
      char* str4 = new char[8];
      sprintf (str3, "7654321");
      print(c,"performed heap overflow.\n");
      sprintf (str4, "1234567");
  
      print(c,"performed heap overflow.\n");
      if( ! (c%rpt_rate) ) {
	printf ("'%s' should NOT equal '%s'.\n", str3, str4);
	printf ("str1 = '%s'\n", str1);
      }
 
      delete[] str2;
      delete[] str3;
      delete[] str4;
 
      print(c,"deleted remaining memory.\n");
    }
  return 0;
}

