/*
 * Program:	File descriptor string routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	15 April 1997
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include "mail.h"
#include "osdep.h"
#include "misc.h"
#include "fdstring.h"

/* String driver for fd stringstructs */

static void fd_string_init (STRING *s,void *data,unsigned long size);
static char fd_string_next (STRING *s);
static void fd_string_setpos (STRING *s,unsigned long i);

STRINGDRIVER fd_string = {
  fd_string_init,		/* initialize string structure */
  fd_string_next,		/* get next byte in string structure */
  fd_string_setpos		/* set position in string structure */
};


/* Initialize string structure for fd stringstruct
 * Accepts: string structure
 *	    pointer to string
 *	    size of string
 */

static void fd_string_init (STRING *s,void *data,unsigned long size)
{
  FDDATA *d = (FDDATA *) data;
  s->data = (void *) d->fd;	/* note fd */
  s->data1 = d->pos;		/* note file offset */
  s->size = size;		/* note size */
  s->curpos = s->chunk = d->chunk;
  s->chunksize = (unsigned long) d->chunksize;
  s->offset = 0;		/* initial position */
				/* and size of data */
  s->cursize = min (s->chunksize,size);
				/* move to that position in the file */
  lseek (d->fd,d->pos,L_SET);
  read (d->fd,s->chunk,(size_t) s->cursize);
}

/* Get next character from fd stringstruct
 * Accepts: string structure
 * Returns: character, string structure chunk refreshed
 */

static char fd_string_next (STRING *s)
{
  char c = *s->curpos++;	/* get next byte */
  SETPOS (s,GETPOS (s));	/* move to next chunk */
  return c;			/* return the byte */
}


/* Set string pointer position for fd stringstruct
 * Accepts: string structure
 *	    new position
 */

static void fd_string_setpos (STRING *s,unsigned long i)
{
  if (i > s->size) i = s->size;	/* don't permit setting beyond EOF */
  s->offset = i;		/* set new offset */
  s->curpos = s->chunk;		/* reset position */
				/* set size of data */
  if (s->cursize = min (s->chunksize,SIZE (s))) {
				/* move to that position in the file */
    lseek ((int) s->data,s->data1 + s->offset,L_SET);
    read ((int) s->data,s->curpos,(size_t) s->cursize);
  }
}
