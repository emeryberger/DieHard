/*
 * Program:	Rename file
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	20 May 1996
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */
 
/* Emulator for working Unix rename() call
 * Accepts: old file name
 *	    new file name
 * Returns: 0 if success, -1 if error with error in errno
 */

int Rename (char *oldname,char *newname)
{
  int ret;
  unlink (newname);		/* make sure the old name doesn't exist */
				/* link to new name, unlink old name */
  if (!(ret = link (oldname,newname))) unlink (oldname);
  return ret;
}


