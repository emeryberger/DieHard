/*
 * Program:	TCP/IP routines
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
 * Last Edited:	7 January 2002
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2002 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Dummy definition overridden by TCP routines */

#ifndef TCPSTREAM
#define TCPSTREAM void
#endif


/* Function prototypes */

void *tcp_parameters (long function,void *value);
TCPSTREAM *tcp_open (char *host,char *service,unsigned long port);
TCPSTREAM *tcp_aopen (NETMBX *mb,char *service,char *usrbuf);
char *tcp_getline (TCPSTREAM *stream);
long tcp_getbuffer (TCPSTREAM *stream,unsigned long size,char *buffer);
long tcp_getdata (TCPSTREAM *stream);
long tcp_soutr (TCPSTREAM *stream,char *string);
long tcp_sout (TCPSTREAM *stream,char *string,unsigned long size);
void tcp_close (TCPSTREAM *stream);
char *tcp_host (TCPSTREAM *stream);
char *tcp_remotehost (TCPSTREAM *stream);
unsigned long tcp_port (TCPSTREAM *stream);
char *tcp_localhost (TCPSTREAM *stream);
char *tcp_clientaddr (void);
char *tcp_clienthost (void);
char *tcp_serveraddr (void);
char *tcp_serverhost (void);
long tcp_serverport (void);
char *tcp_canonical (char *name);
