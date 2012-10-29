/*
 * Program:	Error number to string
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
 
/* Return implementation-defined string corresponding to error
 * Accepts: error number
 * Returns: string for that error
 */

char *strerror (int n)
{
  return (n >= 0 && n < sys_nerr) ? sys_errlist[n] : NIL;
}
