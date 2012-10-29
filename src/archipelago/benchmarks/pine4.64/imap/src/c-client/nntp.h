/*
 * Program:	Network News Transfer Protocol (NNTP) routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	10 February 1992
 * Last Edited:	22 October 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Constants (should be in nntp.c) */

#define NNTPTCPPORT (long) 119	/* assigned TCP contact port */


/* NNTP open options
 * For compatibility with the past, NOP_DEBUG must always be 1.
 */

#define NOP_DEBUG (long) 0x1	/* debug protocol negotiations */
#define NOP_READONLY (long) 0x2	/* read-only open */
#define NOP_TRYSSL (long) 0x4	/* try SSL first */
				/* reserved for application use */
#define NOP_RESERVED (unsigned long) 0xff000000


/* Compatibility support names */

#define nntp_open(hostlist,options) \
  nntp_open_full (NIL,hostlist,"nntp",NIL,options)


/* Function prototypes */

SENDSTREAM *nntp_open_full (NETDRIVER *dv,char **hostlist,char *service,
			    unsigned long port,long options);
SENDSTREAM *nntp_close (SENDSTREAM *stream);
long nntp_mail (SENDSTREAM *stream,ENVELOPE *msg,BODY *body);
