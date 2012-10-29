#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "iv.h"

#define PROC_STRING_SIZE 8
#define PROC_FILENAME    "/proc/ivhs/feedback"

static int started = 0;
static long count  = 0;
 
int read_proc(char *buf) {
  int triggerFd;
  int res;

  triggerFd = open(PROC_FILENAME, O_RDONLY | O_RSYNC); 

  if (triggerFd == -1) 
    return -1; 

  res = read(triggerFd, buf, PROC_STRING_SIZE); 
      
  close(triggerFd); 

  return res;
}

int start_allocstall_collection(void) {

  char buf[9];
  int res;

  if (started) //don't restart
    return 1;

  res = read_proc(buf); 

  if (res == -1) 
    return -1; //failed to read the file
  else if (res > 0) 
    read_proc(buf); //counter was left in a bad state, read again to clear

  started = 1; //started

  return 0; //done
}

int end_allocstall_collection(void) {
  
  char buf[9];
  long res;

  if (!started)
    return -1; //collection hasn't been started

  started = 0; //we will definitely stop collection

  memset(buf, 0, 9);

  res = read_proc(buf);

  if (res <= 0)
    return -1; //failed to read the file

  count = atol(buf);

  return 0;
}

long get_last_allocstall_count() {
  return count;
}


int page_state_write 
