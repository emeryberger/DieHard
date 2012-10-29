/*
 * Program:	Operating-system dependent routines -- OS/2 emx version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	14 March 1996
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
extern int errno;		/* just in case */
#include "tcp_os2.h"		/* must be before osdep includes tcp.h */
#include "mail.h"
#include "osdep.h"
#include "misc.h"
#include "fs_os2.c"
#include "ftl_os2.c"
#include "nl_os2.c"
#include "env_os2.c"
#include "tcp_os2.c"

/* Return my local host name
 * Returns: my local host name
 */

char *mylocalhost (void)
{
  if (!myLocalHost) {		/* known yet? */
    char *s,tmp[MAILTMPLEN];
    struct hostent *he;
				/* could we get local id? */
    gethostname (tmp,MAILTMPLEN-1);
    if (he = gethostbyname (lcase (tmp))) {
      if (he->h_name) s = he->h_name;
      else sprintf (s = tmp,"[%i.%i.%i.%i]",he->h_addr[0],he->h_addr[1],
		    he->h_addr[2],he->h_addr[3]);
    }
    else s = "random-pc";
    myLocalHost = cpystr (s);	/* record for subsequent use */
  }
  return myLocalHost;
}


/* Look up host address
 * Accepts: pointer to pointer to host name
 *	    socket address block
 * Returns: non-zero with host address in socket, official host name in host;
 *	    else NIL
 */

long lookuphost (char **host,struct sockaddr_in *sin)
{
  long ret = -1;
  char tmp[MAILTMPLEN];
  struct hostent *hn = gethostbyname (lcase (strcpy (tmp,*host)));
  if (!hn) return NIL;		/* got a host name? */
  *host = cpystr (hn->h_name);	/* set official name */
				/* copy host addresses */
  memcpy (&sin->sin_addr,hn->h_addr,hn->h_length);
  return T;
}


/*
 * Emulator for BSD syslog() routine.
 */

void syslog (int priority,const char *message,...)
{
}
