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

#include "tcp_unix.h"		/* must be before osdep includes tcp.h */
#include "mail.h"
#include "osdep.h"
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/tiuser.h>
#include <sys/stropts.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pwd.h>
#include <shadow.h>
#include <sys/select.h>
#include "misc.h"

extern int sys_nerr;
extern char *sys_errlist[];

#define DIR_SIZE(d) d->d_reclen

#define toint(c)	((c)-'0')
#define isodigit(c)	(((unsigned)(c)>=060)&((unsigned)(c)<=067))


#include "fs_unix.c"
#include "ftl_unix.c"
#include "nl_unix.c"
#define env_init ENV_INIT
#include "env_unix.c"
#undef env_init
#define getpeername Getpeername
#define fork vfork
#include "tcp_unix.c"
#include "gr_waitp.c"
#include "flocksim.c"
#include "scandir.c"
#include "tz_sv4.c"
#include "utime.c"

/* Jacket around env_init() to work around PTX inetd braindamage */

static char may_need_server_init = T;

long env_init (char *user,char *home)
{
  if (may_need_server_init) {	/* maybe need to do server init cruft? */
    may_need_server_init = NIL;	/* not any more we don't */
    if (!getuid ()) {		/* if root, we're most likely a server */
      t_sync (0);		/* PTX inetd is stupid, stupid, stupid */
      ioctl (0,I_PUSH,"tirdwr");/*  it needs this cruft, else servers won't */
      dup2 (0,1);		/*  work.  How obnoxious!!! */
    }
  }
  ENV_INIT (user,home);		/* call the real routine */
}

/* Emulator for BSD gethostid() call
 * Returns: unique identifier for this machine
 */

long gethostid (void)
{
  struct sockaddr_in sin;
  int inet = t_open (TLI_TCP, O_RDWR, 0);
  if (inet < 0) return 0;
  getmyinaddr (inet,&sin,sizeof (sin));
  close (inet);
  return sin.sin_addr.s_addr;
}


/* Replaced version of getpeername() that jackets into getpeerinaddr()
 * Accepts: file descriptor
 *	    pointer to Internet socket addr
 *	    length
 * Returns: zero if success, data in socket addr
 */

int Getpeername (int s,struct sockaddr *name,int *namelen)
{
  return getpeerinaddr (s,(struct sockaddr_in *) name,*namelen);
}
