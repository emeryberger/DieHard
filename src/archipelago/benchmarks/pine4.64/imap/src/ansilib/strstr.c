/*
 * Program:	Substring search
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

/* Return pointer to first occurance in string of a substring
 * Accepts: source pointer
 *	    substring pointer
 * Returns: pointer to substring in source or NIL if not found
 */

char *strstr (char *cs,char *ct)
{
  char *s;
  char *t;
  while (cs = strchr (cs,*ct)) {/* for each occurance of the first character */
				/* see if remainder of string matches */
    for (s = cs + 1, t = ct + 1; *t && *s == *t; s++, t++);
    if (!*t) return cs;		/* if ran out of substring then have match */
    cs++;			/* try from next character */
  }
  return NIL;			/* not found */
}
