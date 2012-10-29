/*
 * Program:	Safe File Lock for Linux
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	20 April 2005
 * Last Edited:	26 April 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */
 
#undef flock

#include <sys/vfs.h>
#ifndef NFS_SUPER_MAGIC
#define NFS_SUPER_MAGIC 0x6969
#endif

int safe_flock (int fd,int op)
{
  struct statfs sfbuf;
  char tmp[MAILTMPLEN];
  int e;
  int logged = 0;
  /* Check for NFS because Linux 2.6 broke flock() on NFS.  Instead of being
   * a no-op, flock() on NFS now returns ENOLCK.  Read
   *   https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=123415
   * for the gruesome details.
   */
				/* check filesystem type */
  while ((e = fstatfs (fd,&sfbuf)) && (errno == EINTR));
  if (!e) switch (sfbuf.f_type) {
  case NFS_SUPER_MAGIC:		/* always a fast no-op on NFS */
    break;
  default:			/* allow on other filesystem types */
				/* do the lock */
    while (flock (fd,op)) switch (errno) {
    case EINTR:			/* interrupt */
      break;
    case ENOLCK:		/* lock table is full */
      sprintf (tmp,"File locking failure: %s",strerror (errno));
      mm_log (tmp,WARN);	/* give the user a warning of what happened */
      if (!logged++) syslog (LOG_ERR,tmp);
				/* return failure if non-blocking lock */
      if (op & LOCK_NB) return -1;
      sleep (5);		/* slow down in case it loops */
      break;
    case EWOULDBLOCK:		/* file is locked, LOCK_NB should be set */
      if (op & LOCK_NB) return -1;
    case EBADF:			/* not valid open file descriptor */
    case EINVAL:		/* invalid operator */
    default:			/* other error code? */
      sprintf (tmp,"Unexpected file locking failure: %s",strerror (errno));
      fatal (tmp);
    }
    break;
  }
  return 0;			/* success */
}
