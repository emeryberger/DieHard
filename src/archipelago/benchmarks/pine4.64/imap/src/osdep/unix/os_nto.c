/*
 * Program:	Operating-system dependent routines -- QNX Neutrino RTP version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	1 August 1993
 * Last Edited:	10 April 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include "tcp_unix.h"		/* must be before osdep includes tcp.h */
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
#include <shadow.h>
#include <sys/select.h>
#include "misc.h"

#define DIR_SIZE(d) d->d_reclen

#include "fs_unix.c"
#include "ftl_unix.c"
#include "nl_unix.c"
#include "env_unix.c"
#include "tcp_unix.c"
#include "gr_wait.c"
#include "tz_sv4.c"
#include "gethstid.c"
#include "flocksim.c"
#include "utime.c"

/* QNX local readdir()
 * Accepts: directory structure
 * Returns: direct struct or NIL if failed
 */

#undef readdir

struct direct *Readdir (DIR *dirp)
{
  static struct direct dc;
  struct dirent *de = readdir (dirp);
  if (!de) return NIL;		/* end of data */
  dc.d_fileno = 0;		/* could get from de->stat.st_ino */
  dc.d_namlen = strlen (strcpy (dc.d_name,de->d_name));
  dc.d_reclen = sizeof (dc);
  return &dc;
}
