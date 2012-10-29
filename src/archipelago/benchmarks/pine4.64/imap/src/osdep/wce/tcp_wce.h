/*
 * Program:	Winsock TCP/IP routines
 *
 * Author:	Mike Seibel from Unix version by Mark Crispin
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

/* TCP input buffer -- must be large enough to prevent overflow */

#define BUFLEN 8192


/* TCP I/O stream (must be before osdep.h is included) */

#define TCPSTREAM struct tcp_stream
TCPSTREAM {
  char *host;			/* host name */
  char *remotehost;		/* remote host name */
  unsigned long port;		/* port number */
  char *localhost;		/* local host name */
  int tcpsi;			/* input tcp socket */
  int tcpso;			/* output tcp socket */
  long ictr;			/* input counter */
  char *iptr;			/* input pointer */
  char ibuf[BUFLEN];		/* input buffer */
};
