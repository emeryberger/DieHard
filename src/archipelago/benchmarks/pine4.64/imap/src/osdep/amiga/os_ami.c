/*
 * Program:	Operating-system dependent routines -- Amiga version
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
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#define PINE		       /* to get full DIR description in <dirent.h> */
#include "tcp_ami.h"           /* must be before osdep includes tcp.h */
#include "mail.h"
#include "osdep.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
extern int errno;		/* just in case */
#include <pwd.h>
#include "misc.h"

extern int sys_nerr;
extern char *sys_errlist[];

#define DIR_SIZE(d) d->d_reclen /* for scandir.c */

#include "fs_ami.c"
#include "ftl_ami.c"
#include "nl_ami.c"
#include "env_ami.c"
#include "tcp_ami.c"
#include "log_std.c"
#include "gr_waitp.c"
#include "tz_bsd.c"
#include "scandir.c"
#include "gethstid.c"

#undef utime

/* Amiga has its own wierd utime() with an incompatible struct utimbuf that
 * does not match with the traditional time_t [2].
 */

/* Portable utime() that takes it args like real Unix systems
 * Accepts: file path
 *	    traditional utime() argument
 * Returns: utime() results
 */

int portable_utime (char *file,time_t timep[2])
{
  struct utimbuf times;
  times.actime = timep[0];	/* copy the portable values */
  times.modtime = timep[1];
  return utime (file,&times);	/* now call Amiga's routine */
}
