/*
 * Program:	TOPS-20 TCP/IP routines
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
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Dedication:
 * This file is dedicated with affection to the TOPS-20 operating system, which
 * set standards for user and programmer friendliness that have still not been
 * equaled by more `modern' operating systems.
 * Wasureru mon ka!!!!
 */

/* TCP input buffer */

#define BUFLEN 8192

/* TCP I/O stream (must be before osdep.h is included) */

#define TCPSTREAM struct tcp_stream
TCPSTREAM {
  char *host;			/* host name */
  unsigned long port;		/* port number */
  char *localhost;		/* local host name */
  int jfn;			/* jfn for connection */
  char ibuf[BUFLEN];		/* input buffer */
};


/* All of these should be in JSYS.H, but just in case... */

#ifndef _GTHPN
#define _GTHPN 12
#endif

#ifndef _GTDPN
#define _GTDPN 12
#endif

#ifndef GTDOM
#define GTDOM (FLD (1,JSYS_CLASS) | 501
#endif
