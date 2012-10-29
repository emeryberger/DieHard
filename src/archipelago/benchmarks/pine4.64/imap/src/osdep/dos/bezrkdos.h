/*
 * Program:	Berkeley mail routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	24 June 1992
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Dedication:
 *  This file is dedicated with affection to those Merry Marvels of Musical
 * Madness . . .
 *  ->  The Incomparable Leland Stanford Junior University Marching Band  <-
 * who entertain, awaken, and outrage Stanford fans in the fact of repeated
 * losing seasons and shattered Rose Bowl dreams [Cardinal just don't have
 * HUSKY FEVER!!!].
 *
 */

/* Build parameters */

#define CHUNK 4096


/* Berkeley I/O stream local data */
	
typedef struct bezerk_local {
  int fd;			/* file descriptor for I/O */
  off_t filesize;		/* file size parsed */
  char *buf;			/* temporary buffer */
} BEZERKLOCAL;


/* Convenient access to local data */

#define LOCAL ((BEZERKLOCAL *) stream->local)

/* Function prototypes */

DRIVER *bezerk_valid (char *name);
long bezerk_isvalid (char *name,char *tmp);
int bezerk_valid_line (char *s,char **rx,int *rzn);
void *bezerk_parameters (long function,void *value);
void bezerk_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void bezerk_list (MAILSTREAM *stream,char *ref,char *pat);
void bezerk_lsub (MAILSTREAM *stream,char *ref,char *pat);
long bezerk_create (MAILSTREAM *stream,char *mailbox);
long bezerk_delete (MAILSTREAM *stream,char *mailbox);
long bezerk_rename (MAILSTREAM *stream,char *old,char *newname);
MAILSTREAM *bezerk_open (MAILSTREAM *stream);
void bezerk_close (MAILSTREAM *stream,long options);
char *bezerk_header (MAILSTREAM *stream,unsigned long msgno,
		     unsigned long *length,long flags);
long bezerk_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,
		  long flags);
long bezerk_ping (MAILSTREAM *stream);
void bezerk_check (MAILSTREAM *stream);
void bezerk_expunge (MAILSTREAM *stream);
long bezerk_copy (MAILSTREAM *stream,char *sequence,char *mailbox,
		  long options);
long bezerk_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);
int bezerk_append_msg (MAILSTREAM *stream,FILE *sf,char *flags,char *date,
		     STRING *msg);
void bezerk_gc (MAILSTREAM *stream,long gcflags);
char *bezerk_file (char *dst,char *name);
long bezerk_badname (char *tmp,char *s);
long bezerk_parse (MAILSTREAM *stream);
unsigned long bezerk_hdrpos (MAILSTREAM *stream,unsigned long msgno,
			     unsigned long *size);
