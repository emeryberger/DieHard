/*
 * Program:	Test for NFS file -- old version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	10 April 2001
 * Last Edited:	10 April 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Test for NFS
 * Accepts: file descriptor
 * Returns: T if NFS file, NIL otherwise
 */

long test_nfs (int fd)
{
  struct stat sbuf;
  struct ustat usbuf;
  return (!fstat (fd,&sbuf) && !ustat (sbuf.st_dev,&usbuf) &&
	  !++usbuf.f_tinode) ? LONGT : NIL;
}
