/*
 * Program:	Standalone Mailbox Lock program
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	8 February 1999
 * Last Edited:	21 June 2004
 *
 * The IMAP tools software provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sysexits.h>
#include <syslog.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <stdlib.h>
#include <netdb.h>
#include <ctype.h>
#include <strings.h>

#define LOCKTIMEOUT 5		/* lock timeout in minutes */
#define LOCKPROTECTION 0775

#ifndef MAXHOSTNAMELEN		/* Solaris still sucks */
#define MAXHOSTNAMELEN 256
#endif

/* Fatal error
 * Accepts: Message string
 *	    exit code
 * Never returns
 */

void die (char *msg,int code)
{
  syslog (LOG_NOTICE,"(%u) %s",code,msg);
  write (1,"?",1);		/* indicate "impossible" failure */
  _exit (code);			/* sayonara */
}


int main (int argc,char *argv[])
{
  int ld;
  int tries = LOCKTIMEOUT * 60 - 1;
  size_t len;
  char *s,*lock,*hitch,tmp[1024];
  time_t t;
  struct stat sb,fsb;
  struct group *grp = getgrnam ("mail");
				/* get syslog */
  openlog (argv[0],LOG_PID,LOG_MAIL);
  if (!grp || (grp->gr_gid != getegid ())) die ("not setgid mail",EX_USAGE);
  if (argc != 3) die ("invalid arguments",EX_USAGE);
  for (s = argv[1]; *s; s++) if (!isdigit (*s)) die ("invalid fd",EX_USAGE);
  len = strlen (argv[2]);	/* make buffers */
  lock = (char *) malloc (len + 6);
  hitch = (char *) malloc (len + 6 + 40 + MAXHOSTNAMELEN);
  if (!lock || !hitch) die ("malloc failure",errno);
				/* get device/inode of file descriptor */
  if (fstat (atoi (argv[1]),&fsb)) die ("fstat failure",errno);
				/* better be a regular file */
  if ((fsb.st_mode & S_IFMT) != S_IFREG) die ("fd not regular file",EX_USAGE);
				/* now get device/inode of file */
  if (lstat (argv[2],&sb)) die ("lstat failure",errno);
				/* does it match? */
  if ((sb.st_mode & S_IFMT) != S_IFREG) die ("name not regular file",EX_USAGE);
  if ((sb.st_dev != fsb.st_dev) || (sb.st_ino != fsb.st_ino))
    die ("fd and name different",EX_USAGE);
				/* build lock filename */
  sprintf (lock,"%s.lock",argv[2]);
  if (!lstat (lock,&sb) && ((sb.st_mode & S_IFMT) != S_IFREG))
    die ("existing lock not regular file",EX_NOPERM);

  do {				/* until OK or out of tries */
    t = time (0);		/* get the time now */
    /* SUN-OS had an NFS
     * As kludgy as an albatross;
     * And everywhere that it was installed,
     * It was a total loss.
     * -- MRC 9/25/91
     */
				/* build hitching post file name */
    sprintf (hitch,"%s.%lu.%lu.",lock,(unsigned long) time (0),
	     (unsigned long) getpid ());
    len = strlen (hitch);	/* append local host name */
    gethostname (hitch + len,MAXHOSTNAMELEN);
				/* try to get hitching-post file */
    if ((ld = open (hitch,O_WRONLY|O_CREAT|O_EXCL,LOCKPROTECTION)) >= 0) {
				/* make sure others can break the lock */
      chmod (hitch,LOCKPROTECTION);
      close (ld);		/* close the hitching-post */
      link (hitch,lock);	/* tie hitching-post to lock, ignore failure */
      stat (hitch,&sb);		/* get its data */
      unlink (hitch);		/* flush hitching post */
      /* If link count .ne. 2, hitch failed.  Set ld to -1 as if open() failed
	 so we try again.  If extant lock file and time now is .gt. file time
	 plus timeout interval, flush the lock so can win next time around. */
      if ((ld = (sb.st_nlink != 2) ? -1 : 0) && (!stat (lock,&sb)) &&
	  (t > sb.st_ctime + LOCKTIMEOUT * 60)) unlink (lock);
    }
				/* give up immediately if protection failure */
    else if (errno == EACCES) tries = 0;
    if (ld < 0) {		/* lock failed */
      if (tries--) sleep (1);	/* sleep 1 second and try again */
      else {
	write (1,"-",1);	/* hard failure */
	_exit (EX_CANTCREAT);
      }
    }
  } while (ld < 0);
  write (1,"+",1);		/* indicate that all is well */
  read (0,tmp,1);		/* read continue signal from parent */
  unlink (lock);		/* flush the lock file */
  return EX_OK;
}
