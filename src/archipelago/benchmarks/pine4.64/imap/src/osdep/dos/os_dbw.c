/*
 * Program:	Operating-system dependent routines -- MS-DOS (B&W) version
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
#include "misc.h"
#include "stdlib.h"
#include "bwtcp.h"


#include "fs_dos.c"
#include "ftl_dos.c"
#include "nl_dos.c"
#include "env_dos.c"
#undef write
#define read soread
#define write sowrite
#define close soclose 
#include "tcp_dos.c"


/* Return my local host name
 * Returns: my local host name
 */

char *mylocalhost (void)
{
  char *s;
  if (!myLocalHost) {		/* known yet? */
				/* get local host name from DISPLAY env var */
    if (!((s = getenv ("DISPLAY")) || (s = getenv ("display")))) {
      mm_log ("Environment variable 'DISPLAY' is not set", ERROR);
      s = "random-pc";
    }
    myLocalHost = cpystr (s);
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
  char *s = *host;		/* in case of error */
  sin->sin_addr.s_addr = rhost (host);
  if (sin->sin_addr.s_addr == -1) {
    *host = s;			/* error, restore old host name */
    return NIL;
  }
  *host = cpystr (*host);	/* make permanent copy of name */
  return T;			/* success */
}
