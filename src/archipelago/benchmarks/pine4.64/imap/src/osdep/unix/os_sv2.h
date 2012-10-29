/*
 * Program:	Operating-system dependent routines -- SVR2 version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	10 April 1992
 * Last Edited:	10 April 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <unistd.h>
#include <string.h>
#define char void
#include <memory.h>
#undef char
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/file.h>
#include <ustat.h>


/* Many versions of SysV get this wrong */

#define setpgrp(a,b) Setpgrp(a,b)


/* Different names between BSD and SVR4 */

#define L_SET SEEK_SET
#define L_INCR SEEK_CUR
#define L_XTND SEEK_END

#define lstat stat
#define random lrand48

#define SIGSTOP SIGQUIT

#define S_IFLNK 0120000


/* syslog() emulation */

#define LOG_MAIL	(2<<3)	/* mail system */
#define LOG_DAEMON	(3<<3)	/* system daemons */
#define LOG_AUTH	(4<<3)	/* security/authorization messages */
#define LOG_ALERT	1	/* action must be taken immediately */
#define LOG_CONS	0x02	/* log on the console if errors in sending */
#define LOG_ODELAY	0x04	/* delay open until syslog() is called */
#define LOG_NDELAY	0x08	/* don't delay open */
#define LOG_NOWAIT	0x10	/* if forking to log on console, don't wait() */


/* For setitimer() emulation */

#define ITIMER_REAL	0


/* For opendir() emulation */

typedef struct _dirdesc {
  int dd_fd;
  long dd_loc;
  long dd_size;
  char *dd_buf;
} DIR;

struct passwd *getpwent (void);
struct passwd *getpwuid (int uid);
struct passwd *getpwnam (char *name);
struct group *getgrnam (char *name);

char *getenv (char *name);
long gethostid (void);
void *memmove (void *s,void *ct,size_t n);
char *strstr (char *cs,char *ct);
char *strerror (int n);
unsigned long strtoul (char *s,char **endp,int base);
DIR *opendir (char * name);
int closedir (DIR *d);
struct direct *readdir (DIR *d);
typedef int (*select_t) (struct direct *name);
typedef int (*compar_t) (void *d1,void *d2);
int scandir (char *dirname,struct direct ***namelist,select_t select,
	     compar_t compar);
int fsync (int fd);
int openlog (ident,logopt,facility);
int syslog (priority,message,parameters ...);
void *malloc (size_t byteSize);
void free (void *ptr);
void *realloc (void *oldptr,size_t newsize);


#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "flocksim.h"
