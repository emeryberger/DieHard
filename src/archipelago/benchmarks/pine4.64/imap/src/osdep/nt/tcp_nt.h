/*
 * Program:	Winsock TCP/IP routines
 *
 * Author:	Mark Crispin
 *	        Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	11 April 1989
 * Last Edited:	23 December 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* TCP input buffer -- must be large enough to prevent overflow */

#define BUFLEN 16384		/* 32768 causes stdin read() to barf */

#include <winsock2.h>
#include <ws2tcpip.h>
#undef ERROR			/* quell conflicting definition diagnostic */


/* TCP I/O stream (must be before osdep.h is included) */

#define TCPSTREAM struct tcp_stream
TCPSTREAM {
  char *host;			/* host name */
  char *remotehost;		/* remote host name */
  unsigned long port;		/* port number */
  char *localhost;		/* local host name */
  SOCKET tcpsi;			/* tcp socket */
  SOCKET tcpso;			/* tcp socket */
  long ictr;			/* input counter */
  char *iptr;			/* input pointer */
  char ibuf[BUFLEN];		/* input buffer */
};
