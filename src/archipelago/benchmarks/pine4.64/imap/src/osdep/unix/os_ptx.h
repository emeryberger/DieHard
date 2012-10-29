/*
 * Program:	Operating-system dependent routines -- PTX version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	11 May 1989
 * Last Edited:	10 April 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <string.h>

#include <sys/types.h>
#include <sys/dir.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include <stropts.h>		/* needed in daemons */
#include <syslog.h>
#include <sys/file.h>
#include <ustat.h>


/* Different names, equivalent things in BSD and SysV */

#define L_SET SEEK_SET
#define L_INCR SEEK_CUR
#define L_XTND SEEK_END

#define direct dirent
#define random lrand48


#define utime portable_utime
int portable_utime (char *file,time_t timep[2]);

long gethostid (void);
typedef int (*select_t) (struct direct *name);
typedef int (*compar_t) (void *d1,void *d2);
int scandir (char *dirname,struct direct ***namelist,select_t select,
	     compar_t compar);
long ENV_INIT (char *user,char *home);


#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "flocksim.h"
