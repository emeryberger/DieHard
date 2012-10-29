/*
 * Program:	String return successive tokens
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *
 * Date:	11 May 1989
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Find token in string
 * Accepts: source pointer or NIL to use previous source
 *	    vector of token delimiters pointer
 * Returns: pointer to next token
 */

static char *ts = NIL;		/* string to locate tokens */

char *strtok (char *s,char *ct)
{
  char *t;
  if (!s) s = ts;		/* use previous token if none specified */
  if (!(s && *s)) return NIL;	/* no tokens */
				/* find any leading delimiters */
  do for (t = ct, ts = NIL; *t; t++) if (*t == *s) {
    if (*(ts = ++s)) break;	/* yes, restart seach if more in string */
    return ts = NIL;		/* else no more tokens */
  } while (ts);			/* continue until no more leading delimiters */
				/* can we find a new delimiter? */
  for (ts = s; *ts; ts++) for (t = ct; *t; t++) if (*t == *ts) {
    *ts++ = '\0';		/* yes, tie off token at that point */
    return s;			/* return our token */
  }
  ts = NIL;			/* no more tokens */
  return s;			/* return final token */
}
