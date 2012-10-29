/*
 * Program:	Operating-system dependent routines -- Altos System V version
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

#include "tcp_unix.h"		/* must be before osdep includes tcp.h */
#include "mail.h"
#include "osdep.h"
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/tiuser.h>
#include <sys/stropts.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pwd.h>
#include <sys/socket.h>
#include "misc.h"

extern int sys_nerr;
extern char *sys_errlist[];

#define toint(c)	((c)-'0')
#define isodigit(c)	(((unsigned)(c)>=060)&((unsigned)(c)<=067))

#define DIR_SIZE(d) d->d_reclen

#define pid_t short		/* may not be known on all ASV systems */

#include "strstr.c"
#include "fs_unix.c"
#include "ftl_unix.c"
#include "nl_unix.c"
#include "env_unix.c"
#include "tcp_unix.c"
#include "gr_waitp.c"
#include "strerror.c"
#include "flocksim.c"
#include "scandir.c"
#include "strtoul.c"
#include "tz_sv4.c"
#include "gethstid.c"
#include "memmove.c"
#include "fsync.c"
