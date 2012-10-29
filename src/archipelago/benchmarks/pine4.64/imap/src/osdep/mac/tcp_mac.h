/*
 * Program:	Macintosh TCP/IP routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	26 January 1992
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* TCP input buffer */

#define BUFLEN (size_t) 8192	/* TCP input buffer */


/* TCP I/O stream */

#define TCPSTREAM struct tcp_stream
TCPSTREAM {
  char *host;			/* host name */
  unsigned long port;		/* port number */
  char *localhost;		/* local host name */
  struct TCPiopb pb;		/* MacTCP parameter block */
  long ictr;			/* input counter */
  char *iptr;			/* input pointer */
  char ibuf[BUFLEN];		/* input buffer */
};

extern ResultUPP tcp_dns_upp;
pascal void tcp_dns_result (struct hostInfo *hostInfoPtr,char *userDataPtr);
