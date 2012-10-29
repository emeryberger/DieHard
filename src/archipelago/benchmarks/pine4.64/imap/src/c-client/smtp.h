/*
 * Program:	Simple Mail Transfer Protocol (SMTP) routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	27 July 1988
 * Last Edited:	10 March 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 *
 * This original version of this file is
 * Copyright 1988 Stanford University
 * and was developed in the Symbolic Systems Resources Group of the Knowledge
 * Systems Laboratory at Stanford University in 1987-88, and was funded by the
 * Biomedical Research Technology Program of the NationalInstitutes of Health
 * under grant number RR-00785.
 */

/* Constants (should be in smtp.c) */

#define SMTPTCPPORT (long) 25	/* assigned TCP contact port */
#define SUBMITTCPPORT (long) 587/* assigned TCP contact port */


/* SMTP open options
 * For compatibility with the past, SOP_DEBUG must always be 1.
 */

#define SOP_DEBUG (long) 1	/* debug protocol negotiations */
#define SOP_DSN (long) 2	/* DSN requested */
				/* DSN notification, none set mean NEVER */
#define SOP_DSN_NOTIFY_FAILURE (long) 4
#define SOP_DSN_NOTIFY_DELAY (long) 8
#define SOP_DSN_NOTIFY_SUCCESS (long) 16
				/* DSN return full msg vs. header */

#define SOP_DSN_RETURN_FULL (long) 32
#define SOP_8BITMIME (long) 64	/* 8-bit MIME requested */
#define SOP_SECURE (long) 256	/* don't do non-secure authentication */
#define SOP_TRYSSL (long) 512	/* try SSL first */
#define SOP_TRYALT SOP_TRYSSL	/* old name */
				/* reserved for application use */
#define SOP_RESERVED (unsigned long) 0xff000000


/* Compatibility support names */

#define smtp_open(hostlist,options) \
  smtp_open_full (NIL,hostlist,"smtp",NIL,options)


/* Function prototypes */

void *smtp_parameters (long function,void *value);
SENDSTREAM *smtp_open_full (NETDRIVER *dv,char **hostlist,char *service,
			    unsigned long port,long options);
SENDSTREAM *smtp_close (SENDSTREAM *stream);
long smtp_mail (SENDSTREAM *stream,char *type,ENVELOPE *msg,BODY *body);
