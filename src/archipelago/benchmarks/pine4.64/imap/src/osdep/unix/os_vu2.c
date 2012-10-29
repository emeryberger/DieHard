/*
 * Program:	Operating-system dependent routines -- VAX Ultrix 2.3 version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *
 * Date:	11 May 1989
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
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
#include "misc.h"

#define NFDBITS	(sizeof(long) * 8)

#define	FD_SET(n, p)	((p)->fds_bits[(n)/NFDBITS] |= \
					(1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= \
					~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & \
					(1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	bzero((char *)(p), sizeof(*(p)))


/* Old Ultrix has its own wierd inet_addr() that returns a in_addr struct. */

/* Portable inet_addr () that returns a u_long
 * Accepts: dotted host string
 * Returns: u_long
 */

u_long portable_inet_addr (char *hostname)
{
  struct in_addr *in = &inet_addr (hostname);
  return in->s_addr;
}


#define inet_addr portable_inet_addr

#include "fs_unix.c"
#include "ftl_unix.c"
#include "nl_unix.c"
#include "env_unix.c"
#define fork vfork
#include "tcp_unix.c"
#include "gr_wait.c"
#include "memmove.c"
#include "strerror.c"
#include "strstr.c"
#include "strtoul.c"
#include "tz_nul.c"
