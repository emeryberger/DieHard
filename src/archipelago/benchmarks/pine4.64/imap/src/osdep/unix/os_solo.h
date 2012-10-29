/*
 * Program:	Operating-system dependent routines -- Solaris version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	1 August 1988
 * Last Edited:	28 September 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <string.h>

#include <sys/types.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#include <syslog.h>
#include <sys/file.h>
#include <ustat.h>


/* Many versions of SysV get this wrong */

#define setpgrp(a,b) Setpgrp(a,b)


/* Different names, equivalent things in BSD and SysV */

/* L_SET is defined for some strange reason in <sys/file.h> on SVR4. */
#ifndef L_SET
#define L_SET SEEK_SET
#endif
#ifndef L_INCR
#define L_INCR SEEK_CUR
#endif
#ifndef L_XTND
#define L_XTND SEEK_END
#endif

#define direct dirent
#define random lrand48

#define scandir Scandir

#define utime portable_utime
int portable_utime (char *file,time_t timep[2]);

long gethostid (void);
typedef int (*select_t) (struct direct *name);
typedef int (*compar_t) (const void *d1,const void *d2);
int scandir (char *dirname,struct direct ***namelist,select_t select,
	     compar_t compar);


#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "flocksim.h"
