/*
 * Program:	Operating-system dependent routines -- AIX/RT version
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
#include <memory.h>
#include <sys/types.h>
#define direct dirent
#include <dirent.h>
#include <fcntl.h>
#include <sys/syslog.h>
#include <sys/file.h>
#include <ustat.h>


/* Different names between BSD and SVR4 */

#define L_SET SEEK_SET
#define L_INCR SEEK_CUR
#define L_XTND SEEK_END

#define random lrand48

#define SIGSTOP SIGQUIT


/* For setitimer() emulation */

#define ITIMER_REAL 0

struct passwd *getpwent (void);
struct passwd *getpwuid (int uid);
struct passwd *getpwnam (char *name);
char *getenv (char *name);
long gethostid (void);
void *memmove (void *s,void *ct,size_t n);
char *strstr (char *cs,char *ct);
char *strerror (int n);
unsigned long strtoul (char *s,char **endp,int base);
typedef int (*select_t) (struct direct *name);
typedef int (*compar_t) (void *d1,void *d2);
int scandir (char *dirname,struct direct ***namelist,select_t select,
	     compar_t compar);
void *malloc (size_t byteSize);
void free (void *ptr);
void *realloc (void *oldptr,size_t newsize);
int openlog (ident,logopt,facility);
int syslog (priority,message,parameters ...);

#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "flocksim.h"
