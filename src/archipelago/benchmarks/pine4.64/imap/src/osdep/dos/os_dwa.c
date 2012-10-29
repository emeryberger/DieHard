/*
 * Program:	Operating-system dependent routines -- DOS (Waterloo) version
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

#include <tcp.h>		/* must be before TCPSTREAM definition */
#include "tcp_dwa.h"		/* must be before osdep include our tcp.h */
#include "mail.h"
#include "osdep.h"
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <sys\timeb.h>
#include "misc.h"

/* Undo compatibility definition */

#undef tcp_open

#include "fs_dos.c"
#include "ftl_dos.c"
#include "nl_dos.c"
#include "env_dos.c"
#include "tcp_dwa.c"


/* Return my local host name
 * Returns: my local host name
 */

char *mylocalhost (void)
{
  if (!myLocalHost) {
    char *s,hname[32],tmp[MAILTMPLEN];
    long myip;

    if (!sock_initted++) sock_init();
    tcp_cbrk (0x01);		/* turn off ctrl-break catching */
    /*
     * haven't discovered a way to find out the local host's 
     * name with wattcp yet.
     */
    if (myip = gethostid ()) sprintf (s = tmp,"[%s]",inet_ntoa (hname,myip));
    else s = "random-pc";
    myLocalHost = cpystr (s);
  }
  return myLocalHost;
}
