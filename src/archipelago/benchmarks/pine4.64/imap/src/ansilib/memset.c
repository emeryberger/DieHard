/*
 * Program:	Set memory
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

/* Set a block of memory
 * Accepts: destination pointer
 *	    value to set
 *	    length
 * Returns: destination pointer
 */

void *memset (void *s,int c,size_t n)
{
  if (c) while (n) s[--n] = c;	/* this way if non-zero */
  else bzero (s,n);		/* they should have this one */
  return s;
}
