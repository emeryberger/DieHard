/*
 * Program:	Mac newline routines
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
 * Last Edited:	27 April 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Copy string with CRLF newlines
 * Accepts: destination string
 *	    pointer to size of destination string buffer
 *	    source string
 *	    length of source string
 * Returns: length of copied string
 */

unsigned long strcrlfcpy (unsigned char **dst,unsigned long *dstl,
			  unsigned char *src,unsigned long srcl)
{
  long i,j;
  unsigned char c,*d = src;
  if (*dst) {			/* destination provided? */
    if ((i = srcl * 2) > *dstl)	/* calculate worst-case situation */
      for (i = j = srcl; j; --j) if (*d++ == '\015') i++;
				/* flush destination buffer if too small */
    if (i > *dstl) fs_give ((void **) dst);
  }
				/* make a new buffer if needed */
  if (!*dst) *dst = (char *) fs_get ((*dstl = i) + 1);
  d = *dst;			/* destination string */
  if (srcl) do {		/* copy string */
    c = *d++ = *src++;		/* copy character */
				/* append line feed to bare CR */
    if ((c == '\015') && (*src != '\012')) *d++ = '\012';
  } while (--srcl);
  *d = '\0';			/* tie off destination */
  return d - *dst;		/* return length */
}


/* Length of string after strcrlfcpy applied
 * Accepts: source string
 * Returns: length of string
 */

unsigned long strcrlflen (STRING *s)
{
  unsigned long pos = GETPOS (s);
  unsigned long i = SIZE (s);
  unsigned long j = i;
  while (j--) if ((SNX (s) == '\015') && ((CHR (s) != '\012') || !j)) i++;
  SETPOS (s,pos);		/* restore old position */
  return i;
}
