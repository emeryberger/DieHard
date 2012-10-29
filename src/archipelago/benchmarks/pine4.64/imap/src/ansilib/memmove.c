/*
 * Program:	Memory move
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
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */
 
/* Copy memory block
 * Accepts: destination pointer
 *	    source pointer
 *	    length
 * Returns: destination pointer
 */

void *memmove (void *s,void *ct,size_t n)
{
  bcopy (ct,s,n);		/* they should have this one */
  return s;
}
