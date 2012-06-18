/*
  This program contains a number of memory errors, making it unlikely
  to run correctly in anything except DieHard.
*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <signal.h>

int main()
{
  char * str1 = new char[8];
  char * str2 = new char[48];
  sprintf (str1, "1234567");
  sprintf (str2, "Use DieHard now. Stop memory errors.");

  printf("str1 has %d bytes\n",strlen(str1)+1);
  printf("str2 has %d bytes\n",strlen(str2)+1);

  // Invalid frees.
  for (int j = 0; j < 10; j++)
    delete ((int *) &j);

  // Heap overflow through a library call.
  strcpy (str1, str2);

  // Double frees, dangling pointer (premature free).
  delete [] str1;
  delete [] str1;

  printf ("%s\n", str2);

  char * str3 = new char[8];
  char * str4 = new char[8];
  sprintf (str3, "7654321");
  sprintf (str4, "1234567");
  printf ("'%s' should NOT equal '%s'.\n", str3, str4);
  printf ("str1 = '%s'\n", str1);

  raise(SIGUSR1);

  return 0;
}
