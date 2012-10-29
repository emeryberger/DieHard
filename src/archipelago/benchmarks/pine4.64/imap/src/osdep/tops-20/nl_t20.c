/*
 * Program:	Windows/TOPS-20 newline routines
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
				/* flush destination buffer if too small */
  if (*dst && (srcl > *dstl)) fs_give ((void **) dst);
  if (!*dst) {			/* make a new buffer if needed */
    *dst = (char *) fs_get ((size_t) (*dstl = srcl) + 1);
    if (dstl) *dstl = srcl;	/* return new buffer length to main program */
  }
				/* copy strings */
  if (srcl) memcpy (*dst,src,(size_t) srcl);
  *(*dst + srcl) = '\0';	/* tie off destination */
  return srcl;			/* return length */
}


/* Length of string after strcrlfcpy applied
 * Accepts: source string
 * Returns: length of string
 */

unsigned long strcrlflen (STRING *s)
{
  return SIZE (s);		/* no-brainer on DOS! */
}
