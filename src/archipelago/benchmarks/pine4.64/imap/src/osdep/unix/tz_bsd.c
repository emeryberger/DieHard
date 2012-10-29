/*
 * Program:	BSD-style Time Zone String
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	30 August 1994
 * Last Edited:	7 November 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Append local timezone name
 * Accepts: destination string
 */

void rfc822_timezone (char *s,void *t)
{
				/* append timezone from tm struct */
  sprintf (s + strlen (s)," (%.50s)",((struct tm *) t)->tm_zone);
}
