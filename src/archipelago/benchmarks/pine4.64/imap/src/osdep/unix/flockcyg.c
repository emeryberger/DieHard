/*
 * Program:	flock emulation via fcntl() locking
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
 * Last Edited:	21 October 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Cygwin does not seem to have the design flaw in fcntl() locking that
 * most other systems do (see flocksim.c for details).  If some cretin
 * decides to implement that design flaw, then Cygwin will have to use
 * flocksim.  Also, we don't test NFS either.
 *
 * However, Cygwin does have the Windows misfeature (introduced in NT 4.0)
 * that you can not write to any segment which has a shared lock, and you
 * can't lock a zero-byte segment either.  This screws up the shared-write
 * mailbox drivers (mbx, mtx, mx, and tenex).  As a workaround, we'll only
 * lock the first byte of the file, meaning that you can't write that byte
 * shared.  It's been suggested to lock the maximum off_t type, but that
 * risks having a future version of Windows (or Cygwin) deciding that this
 * also means "no lock".
 */

#undef flock			/* name is used as a struct for fcntl */

/* Emulator for flock() call
 * Accepts: file descriptor
 *	    operation bitmask
 * Returns: 0 if successful, -1 if failure under BSD conditions
 */

int flocksim (int fd,int op)
{
  char tmp[MAILTMPLEN];
  int logged = 0;
  struct flock fl;
				/* lock one bytes at byte 0 */
  fl.l_whence = SEEK_SET; fl.l_start = 0; fl.l_len = 1;
  fl.l_pid = getpid ();		/* shouldn't be necessary */
  switch (op & ~LOCK_NB) {	/* translate to fcntl() operation */
  case LOCK_EX:			/* exclusive */
    fl.l_type = F_WRLCK;
    break;
  case LOCK_SH:			/* shared */
    fl.l_type = F_RDLCK;
    break;
  case LOCK_UN:			/* unlock */
    fl.l_type = F_UNLCK;
    break;
  default:			/* default */
    errno = EINVAL;
    return -1;
  }
  while (fcntl (fd,(op & LOCK_NB) ? F_SETLK : F_SETLKW,&fl))
    if (errno != EINTR) {
      /* Can't use switch here because these error codes may resolve to the
       * same value on some systems.
       */
      if ((errno != EWOULDBLOCK) && (errno != EAGAIN) && (errno != EACCES)) {
	sprintf (tmp,"Unexpected file locking failure: %s",strerror (errno));
				/* give the user a warning of what happened */
	MM_NOTIFY (NIL,tmp,WARN);
	if (!logged++) syslog (LOG_ERR,"%s",tmp);
	if (op & LOCK_NB) return -1;
	sleep (5);		/* slow things down for loops */
      }
				/* return failure for non-blocking lock */
      else if (op & LOCK_NB) return -1;
    }
  return 0;			/* success */
}
