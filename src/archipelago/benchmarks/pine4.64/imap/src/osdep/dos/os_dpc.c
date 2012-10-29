/*
 * Program:	Operating-system dependent routines -- MS-DOS (PC/TCP) version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	11 April 1989
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Private function prototypes */

#include "tcp_dos.h"		/* must be before osdep includes tcp.h */
#include "mail.h"
#include "osdep.h"
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <sys\timeb.h>
#include <4bsddefs.h>
#include <sys\socket.h>
#include <errno.h>
#include <arpa\inet.h>
#include <netinet\in.h>
#include <netdb.h>
#include "misc.h"


#include "fs_dos.c"
#include "ftl_dos.c"
#include "nl_dos.c"
#include "env_dos.c"
#undef write
#include "tcp_dos.c"


/* Return my local host name
 * Returns: my local host name
 */

char *mylocalhost (void)
{
  if (!myLocalHost) {		/* known yet */
    char *s,tmp[MAILTMPLEN];
    long myip;
				/* see if known host name */
    if (!gethostname (tmp,MAILTMPLEN-1)) s = tmp;
				/* no, try IP address */
    else if (myip = gethostid ()) {
      struct in_addr in;
      in.s_addr = myip;
      sprintf (s = tmp,"[%s]",inet_ntoa (in));
    }
				/* older kernel, look harder. */
    else if (getconf ("ifcust","ip-address",tmp+1,MAILTMPLEN-2)) {
      *(s = tmp) = '[';		/* wrap the brackets around it */
      strcat (tmp,"]");
    }
    else s = "random-pc";	/* say what? */
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
