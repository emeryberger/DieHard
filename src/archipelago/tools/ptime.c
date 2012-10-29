/*
 * Originally written by Adam Dingle (adam@medovina.org) 
 * for Gel2 programming language while he was at Google.
 * Google released Gel2 and this utility under MIT License.
 * See http://code.google.com/p/gel2/ and LICENSE.ptime for details.
 *
 * Modified by Vitaliy Lvin (vlvin@cs.umass.edu), 2007
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // for asprintf()
#endif

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/resource.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>  // for PAGE_SIZE
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_FORMAT "cpu = %.2f sec\nvirtual = %uk, resident = %uk, writeable = %uk"

FILE *outfile;
char **myenv;
char *format;
char *filename;
char **args;
FILE *pidfile = NULL;

unsigned max_vm = 0, max_rss = 0, max_writeable = 0;
struct rusage rusage;

unsigned umax(unsigned u, unsigned v) {
  return (u > v ? u : v);
}

float timeval_to_sec(struct timeval *tv) {
  return tv->tv_sec + tv->tv_usec / 1000000.0f;
}


/*
 * Obtains the amount of writable(?) memory
 * Written by Adam, not sure what for... 
 */ 
int writeable_size(const char *maps_name) {
  FILE *maps = fopen(maps_name, "r");

  if (!maps)
    return 0;
  
  unsigned writeable = 0;
  
  while (true) {
    unsigned start_addr, end_addr;
    char permission[4];
    
    if (fscanf(maps, "%8x-%8x %4c", &start_addr, &end_addr, permission) != 3)
      break;
    
    if (permission[1] == 'w' && permission[3] == 'p')
      writeable += (end_addr - start_addr);
    
    while (fgetc(maps) != '\n');
  }
  
  fclose(maps);

  return writeable;
}


/*
 * Creates a new environment, based on the old one,
 * by adding a given number of new strings. 
 * To be used with execve and alike.
 * Note: memory for the new environment is obtained through malloc
 * and should be freed appropriately
 */
char **extend_environment(char **old_env, int inc, char **new_strings) {
   //count the number of environment strings
  int i, count = 0;
  char **p = old_env;
 
  while (*p != NULL) {
    count++;
    p++;
  }

  //allocated space for the new environment
  //note that we need the space for terminating NULL
  char **new_env = (char **)malloc(sizeof(char*) * (count + inc + 1));

  //copy the old one
  memcpy(new_env, old_env, sizeof(char*) * count);

  //add new strings
  for (i = 0; i < inc; i++)
    new_env[count + i] = new_strings[i];

  //terminate with null
  new_env[count + 1] = NULL;

  return new_env;
}

/*
 * Parses arguments
 */

int parse_args(int argc, char *argv[]) {

  if (argc < 2) {
    puts("usage: ptime [-o outfile] [-e env string] [-f format] [-p pid file] <program> [arg...]");
    return 0;
  }

  int index = 1;

  //initialize 
  outfile = stdout;
  myenv = environ;
  format = DEFAULT_FORMAT;

  while (index < argc) {

    if (strcmp(argv[index], "-o") == 0) { //check for output file
      outfile = fopen(argv[index + 1], "w+");
      index += 2;
      continue;
    }

    if (strcmp(argv[index], "-e") == 0) { //check the preloaded libs
      myenv = extend_environment(myenv, 1, &argv[index + 1]);
      index += 2;
      continue;
    }

    if (strcmp(argv[index], "-f") == 0) { //check for new format
      format = argv[index + 1];
      index += 2;
      continue;
    }

    if (strcmp(argv[index], "-p") == 0) { //check if pid of the child needs to be saved
      pidfile = fopen(argv[index + 1], "w+");
      index += 2;
      continue; 
    }

    //whatever remains, is the program name and its arguments
    filename = argv[index];
    args = &argv[index];

    break; //done
  }

  return 1;
}

/*
 * Check on child, read its running time
 */
int update_time(pid_t pid) { 
  pid_t p1 = wait4(pid, NULL, WNOHANG, &rusage);

  if (p1 == -1)
    exit(-1);

  if (p1 != 0)
    return 0; // child done
  else 
    return 1; // need to wait some more
}

int resident_size(char *statm_name) {
  FILE *statm = fopen(statm_name, "r");
  if (statm != NULL) {
    int vm, rss,
      res = fscanf(statm, "%d %d", &vm, &rss);
    fclose(statm);
    
    if (res == 2)

      return rss * PAGE_SIZE;
  }

  return 0;
}

int virtual_size(char *statm_name) {
  FILE *statm = fopen(statm_name, "r");
  if (statm != NULL) {
    int vm, rss,
      res = fscanf(statm, "%d %d", &vm, &rss);
    fclose(statm);
    
    if (res == 2)

      return vm * PAGE_SIZE;
  }

  return 0;
}


int main(int argc, char *argv[]) {
  //parse the args
  if (!parse_args(argc, argv))
    return 1;

  //fork
  pid_t pid = fork();

  if (!pid)  
    // child process
    return execve(filename, args, myenv);
  
  //parent

  if (pidfile) { //save pid of the child (to be killed externally)
    fprintf(pidfile, "%d", pid);
    fclose(pidfile);
  }

  //generate respective proc filenames
  char *statm_name;
  asprintf(&statm_name, "/proc/%d/statm", pid);
  char *maps_name;
  asprintf(&maps_name, "/proc/%d/maps", pid);
  

  while (true) {
    //collect time stats and check for child completion
    if (!update_time(pid))
      break;

    sleep(1);

    //collect memory stats
    max_vm        = umax(max_vm,        virtual_size(statm_name));
    max_rss       = umax(max_rss,       resident_size(statm_name));
    max_writeable = umax(max_writeable, writeable_size(maps_name));
  }
  
  //output the stats
  float cpu_time = timeval_to_sec(&rusage.ru_utime) +
                   timeval_to_sec(&rusage.ru_stime);

  fprintf(outfile, format, cpu_time, 
	  max_vm / 1024, max_rss / 1024, max_writeable / 1024);
  fprintf(outfile, "\n");
  return 0;
}
