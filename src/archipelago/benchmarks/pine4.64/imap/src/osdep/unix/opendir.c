/*
 * Program:	Read directories
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	16 December 1993
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Emulator for BSD opendir() call
 * Accepts: directory name
 * Returns: directory structure pointer
 */

DIR *opendir (char *name)
{
  DIR *d = NIL;
  struct stat sbuf;
  int fd = open (name,O_RDONLY,NIL);
  errno = ENOTDIR;		/* default error is bogus directory */
  if ((fd >= 0) && !(fstat (fd,&sbuf)) && ((sbuf.st_mode&S_IFMT) == S_IFDIR)) {
    d = (DIR *) fs_get (sizeof (DIR));
				/* initialize structure */
    d->dd_loc = 0;
    read (fd,d->dd_buf = (char *) fs_get (sbuf.st_size),
	  d->dd_size = sbuf.st_size);
  }
  else if (d) fs_give ((void **) &d);
  if (fd >= 0) close (fd);
  return d;
}


/* Emulator for BSD closedir() call
 * Accepts: directory structure pointer
 */

int closedir (DIR *d)
{
				/* free storage */
  fs_give ((void **) &(d->dd_buf));
  fs_give ((void **) &d);
  return NIL;			/* return */
}


/* Emulator for BSD readdir() call
 * Accepts: directory structure pointer
 */

struct direct *readdir (DIR *d)
{
				/* loop through directory */
  while (d->dd_loc < d->dd_size) {
    struct direct *dp = (struct direct *) (d->dd_buf + d->dd_loc);
    d->dd_loc += sizeof (struct direct);
    if (dp->d_ino) return dp;	/* if have a good entry return it */
  }
  return NIL;			/* all done */
}
