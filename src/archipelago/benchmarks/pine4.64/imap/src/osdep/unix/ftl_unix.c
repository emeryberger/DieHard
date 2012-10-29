/*
 * Program:	UNIX crash management routines
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
 * Last Edited:	14 May 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Report a fatal error
 * Accepts: string to output
 */

void fatal (char *string)
{
  MM_FATAL (string);		/* pass up the string */
  syslog (LOG_ALERT,"IMAP toolkit crash: %.100s",string);
  abort ();			/* die horribly */
}
