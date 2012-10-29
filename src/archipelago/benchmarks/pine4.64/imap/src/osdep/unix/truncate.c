/*
 * Program:	Truncate a file
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	30 June 1994
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Emulator for ftruncate() call
 * Accepts: file descriptor
 *	    length
 * Returns: 0 if success, -1 if failure
 */

int ftruncate (int fd,off_t length)
{
  struct flock fb;
  fb.l_whence = 0;
  fb.l_len = 0;
  fb.l_start = length;
  fb.l_type = F_WRLCK;		/* write lock on file space */
  return fcntl (fd,F_FREESP,&fb);
}
