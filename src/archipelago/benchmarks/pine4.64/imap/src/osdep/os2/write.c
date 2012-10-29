/*
 * Program:	Write data, treating partial writes as an error
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	26 May 1995
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/*  The whole purpose of this unfortunate routine is to deal with DOS and
 * certain cretinous versions of UNIX which decided that the "bytes actually
 * written" return value from write() gave them license to use that for things
 * that are really errors, such as disk quota exceeded, maximum file size
 * exceeded, disk full, etc.
 * 
 *  BSD won't screw us this way on the local filesystem, but who knows what
 * some NFS-mounted filesystem will do.
 */

#undef write

/* Write data to file
 * Accepts: file descriptor
 *	    I/O vector structure
 *	    number of vectors in structure
 * Returns: number of bytes written if successful, -1 if failure
 */

long maxposint = (long)((((unsigned long) 1) << ((sizeof(int) * 8) - 1)) - 1);

long safe_write (int fd,char *buf,long nbytes)
{
  long i,j;
  if (nbytes > 0) for (i = nbytes; i; i -= j,buf += j) {
    while (((j = write (fd,buf,(int) min (maxposint,i))) < 0) &&
	   (errno == EINTR));
    if (j < 0) return j;
  }
  return nbytes;
}
