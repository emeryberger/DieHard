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
 
#undef flock			/* name is used as a struct for fcntl */
#undef fork			/* make damn sure that we don't use vfork!! */
#include "nfstest.c"		/* get NFS tester */

#ifndef NSIG			/* don't know if this can happen */
#define NSIG 32			/* a common maximum */
#endif

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
				/* lock zero bytes at byte 0 */
  fl.l_whence = SEEK_SET; fl.l_start = fl.l_len = 0;
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

  /*  Make fcntl() locking of NFS files be a no-op the way it is with flock()
   * on BSD.  This is because the rpc.statd/rpc.lockd daemons don't work very
   * well and cause cluster-wide hangs if you exercise them at all.  The
   * result of this is that you lose the ability to detect shared mail_open()
   * on NFS-mounted files.  If you are wise, you'll use IMAP instead of NFS
   * for mail files.
   *
   *  Sun alleges that it doesn't matter, because they say they have fixed all
   * the rpc.statd/rpc.lockd bugs.  This is absolutely not true; huge amounts
   * of user and support time have been wasted in cluster-wide hangs.
   */
  if (test_nfs (fd) || mail_parameters (NIL,GET_DISABLEFCNTLLOCK,NIL))
    return 0;			/* fcntl() locking disabled, return success */
				/* do the lock */
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

/* Master/slave procedures for safe fcntl() locking.
 *
 *  The purpose of this nonsense is to work around a bad bug in fcntl()
 * locking.  The cretins who designed it decided that a close() should
 * release any locks made by that process on the file opened on that
 * file descriptor.  Never mind that the lock wasn't made on that file
 * descriptor, but rather on some other file descriptor.
 *
 *  This bug is on every implementation of fcntl() locking that I have
 * tested.  Fortunately, on BSD systems, OSF/1, and Linux, we can use the
 * flock() system call which doesn't have this bug.
 *
 *  Note that OSF/1, Linux, and some BSD systems have both broken fcntl()
 * locking and the working flock() locking.
 *
 *  The program below can be used to demonstrate this problem.  Be sure to
 * let it run long enough for all the sleep() calls to finish.
 */

#if 0
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>

main ()
{
  struct flock fl;
  int fd,fd2;
  char *file = "a.a";
  if ((fd = creat (file,0666)) < 0)
    perror ("TEST FAILED: can't create test file"),_exit (errno);
  close (fd);
  if (fork ()) {		/* parent */
    if ((fd = open (file,O_RDWR,0)) < 0) abort();
				/* lock applies to entire file */
    fl.l_whence = fl.l_start = fl.l_len = 0;
    fl.l_pid = getpid ();	/* shouldn't be necessary */
    fl.l_type = F_RDLCK;
    if (fcntl (fd,F_SETLKW,&fl) == -1) abort ();
    sleep (5);
    if ((fd2 = open (file,O_RDWR,0)) < 0) abort ();
    sleep (1);
    puts ("parent test ready -- will hang here if locking works correctly");
    close (fd2);
    wait (0);
    puts ("OS BUG: child terminated");
    _exit (0);
  }
  else {			/* child */
    sleep (2);
    if ((fd = open (file,O_RDWR,0666)) < 0) abort ();
    puts ("child test ready -- child will hang if no bug");
				/* lock applies to entire file */
    fl.l_whence = fl.l_start = fl.l_len = 0;
    fl.l_pid = getpid ();	/* shouldn't be necessary */
    fl.l_type = F_WRLCK;
    if (fcntl (fd,F_SETLKW,&fl) == -1) abort ();
    puts ("OS BUG: child got lock");
  }
}
#endif

/*  Beware of systems such as AIX which offer flock() as a compatibility
 * function that is just a jacket into fcntl() locking.  The program below
 * is a variant of the program above, only using flock().  It can be used
 * to test to see if your system has real flock() or just a jacket into
 * fcntl().
 *
 *  Be sure to let it run long enough for all the sleep() calls to finish.
 * If the program hangs, then flock() works and you can dispense with the
 * use of this module (you lucky person!).
 */

#if 0
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/file.h>

main ()
{
  int fd,fd2;
  char *file = "a.a";
  if ((fd = creat (file,0666)) < 0)
    perror ("TEST FAILED: can't create test file"),_exit (errno);
  close (fd);
  if (fork ()) {		/* parent */
    if ((fd = open (file,O_RDWR,0)) < 0) abort();
    if (flock (fd,LOCK_SH) == -1) abort ();
    sleep (5);
    if ((fd2 = open (file,O_RDWR,0)) < 0) abort ();
    sleep (1);
    puts ("parent test ready -- will hang here if flock() works correctly");
    close (fd2);
    wait (0);
    puts ("OS BUG: child terminated");
    _exit (0);
  }
  else {			/* child */
    sleep (2);
    if ((fd = open (file,O_RDWR,0666)) < 0) abort ();
    puts ("child test ready -- child will hang if no bug");
    if (flock (fd,LOCK_EX) == -1) abort ();
    puts ("OS BUG: child got lock");
  }
}
#endif

/* Master/slave details
 *
 *  On broken systems, we invoke an inferior fork to execute any driver
 * dispatches which are likely to tickle this bug; specifically, any
 * dispatch which may fiddle with a mailbox that is already selected.  As
 * of this writing, these are: delete, rename, status, scan, copy, and append.
 *
 *  Delete and rename are pretty marginal, yet there are certain clients
 * (e.g. Outlook Express) that really want to delete or rename the selected
 * mailbox.  The same is true of status, but there are people (such as the
 * authors of Entourage) who don't understand why status of the selected
 * mailbox is bad news.
 *
 *  However, in copy and append it is reasonable to do this to a selected
 * mailbox.  Although scanning the selected mailbox isn't particularly
 * sensible, it's hard to avoid due to wildcards.
 *
 *  It is still possible for an application to trigger the bug by doing
 * mail_open() on the same mailbox twice.  Don't do it.
 *
 *  Once the slave is invoked, the master only has to read events from the
 * slave's output (see below for these events) and translate these events
 * to the appropriate c-client callback.  When end of file occurs on the pipe,
 * the master reads the slave's exit status and uses that as the function
 * return.  The append master is slightly more complicated because it has to
 * send data back to the slave (see below).
 *
 *  The slave takes callback events from the driver which otherwise would
 * pass to the main program.  Only those events which a slave can actually
 * encounter are covered here; for example mm_searched() and mm_list() are
 * not covered since a slave never does the operations that trigger these.
 * Certain other events (mm_exists(), mm_expunged(), mm_flags()) are discarded
 * by the slave since the master will generate these events for itself.
 *
 *  The other events cause the slave to write a newline-terminated string to
 * its output.  The first character of string indicates the event: S for
 * mm_status(), N for mm_notify(), L for mm_log(), C for mm_critical(), X for
 * mm_nocritical(), D for mm_diskerror(), F for mm_fatal(), and "A" for append
 * argument callback.  Most of these events also carry data, which carried as
 * text space-delimited in the string.
 *
 *  Append argument callback requires the master to provide the slave with
 * data in the slave's input.  The first thing that the master provides is
 * either a "+" (master has data for the slave) or a "-" (master has no data).
 * If the master has data, it will then send the flags, internal date, and
 * message text, each as <text octet count><SPACE><text>.
 */

/*  It should be alright for lockslavep to be a global, since it will always
 * be zero in the master (which is where threads would be).  The slave won't
 * ever thread, since any driver which threads in its methods probably can't
 * use fcntl() locking so won't have DR_LOCKING in its driver flags 
 *
 *  lockslavep can not be a static, since it's used by the dispatch macros.
 */

int lockslavep = 0;		/* non-zero means slave process for locking */
static int lockproxycopy = 0;	/* non-zero means redo copy as proxy */
FILE *slavein = NIL;		/* slave input */
FILE *slaveout = NIL;		/* slave output */


/* Common master
 * Accepts: permitted stream
 *	    append callback (append calls only, else NIL)
 *	    data for callback (append calls only, else NIL)
 * Returns: (master) T if slave succeeded, NIL if slave failed
 *	    (slave) NIL always, with lockslavep non-NIL
 */

static long master (MAILSTREAM *stream,append_t af,void *data)
{
  MAILSTREAM *st;
  MAILSTATUS status;
  STRING *message;
  FILE *pi,*po;
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  long ret = NIL;
  unsigned long i,j;
  int c,pid,pipei[2],pipeo[2];
  char *s,*t,tmp[MAILTMPLEN];
  lockproxycopy = NIL;		/* not doing a lock proxycopy */
				/* make pipe from slave */
  if (pipe (pipei) < 0) mm_log ("Can't create input pipe",ERROR);
  else if (pipe (pipeo) < 0) {
    mm_log ("Can't create output pipe",ERROR);
    close (pipei[0]); close (pipei[1]);
  }
  else if ((pid = fork ()) < 0) {/* make slave */
    mm_log ("Can't create execution process",ERROR);
    close (pipei[0]); close (pipei[1]);
    close (pipeo[0]); close (pipeo[1]);
  }
  else if (lockslavep = !pid) {	/* are we slave or master? */
    alarm (0);			/* slave doesn't have alarms or signals */
    for (c = 0; c < NSIG; c++) signal (c,SIG_DFL);
    if (!(slavein = fdopen (pipeo[0],"r")) ||
	!(slaveout = fdopen (pipei[1],"w")))
      fatal ("Can't do slave pipe buffered I/O");
    close (pipei[0]);		/* close parent's side of the pipes */
    close (pipeo[1]);
  }

  else {			/* master process */
    void *blockdata = (*bn) (BLOCK_SENSITIVE,NIL);
    close (pipei[1]);		/* close slave's side of the pipes */
    close (pipeo[0]);
    if (!(pi = fdopen (pipei[0],"r")) || !(po = fdopen (pipeo[1],"w")))
      fatal ("Can't do master pipe buffered I/O");
				/* do slave events until EOF */
				/* read event */
    while (fgets (tmp,MAILTMPLEN,pi)) {
				/* tie off event at end of line */
      if (s = strchr (tmp,'\n')) *s = '\0';
      else {
	s = "Execution process event string too long: ";
	sprintf (t = (char *) fs_get (strlen (s) + strlen (tmp) + 1),"%s:%s",
		 s,tmp);
	fatal (t);
      }
      switch (tmp[0]) {		/* analyze event */
      case 'A':			/* append callback */
	if ((*af) (NIL,data,&s,&t,&message)) {
	  if (i = message ? SIZE (message) : 0) {
	    if (!s) s = "";	/* default values */
	    if (!t) t = "";
	  }
	  else s = t = "";	/* no flags or date if no message */
	  errno = NIL;		/* reset last error */
				/* build response */
	  sprintf (tmp,"+%lu %s%lu %s%lu ",strlen (s),s,strlen (t),t,i);
	  if (fputs (tmp,po) == EOF) {
	    sprintf (tmp,"Failed to pipe command \"+%lu %s%lu %s%lu \": %s",
		     strlen (s),s,strlen (t),t,i,strerror (errno));
	    fatal (tmp);
	  }
				/* write message text */
	  if (i) do if (putc (c = 0xff & SNX (message),po) == EOF) {
	    sprintf (tmp,"Failed to pipe %lu bytes (of %lu), last=%u: %s",
		     i,message->size,c,strerror (errno));
	    fatal (tmp);
	  } while (--i);
	}
	else putc ('-',po);	/* append error */
	fflush (po);
	break;
      case '&':			/* slave wants a proxycopy? */
	lockproxycopy = T;
	break;

      case 'L':			/* mm_log() */
	i = strtoul (tmp+1,&s,10);
	if (!s || (*s++ != ' ')) {
	  s = "Invalid log event arguments: ";
	  sprintf (t = (char *) fs_get (strlen (s) + strlen (tmp) + 1),"%s:%s",
		   s,tmp);
	  fatal (t);
	}
	mm_log (s,i);
	break;
      case 'N':			/* mm_notify() */
	st = (MAILSTREAM *) strtoul (tmp+1,&s,16);
	if (s && (*s++ == ' ')) {
	  i = strtoul (s,&s,10);/* get severity */
	  if (s && (*s++ == ' ')) {
	    mm_notify ((st == stream) ? stream : NIL,s,i);
	    break;
	  }
	}
	s = "Invalid notify event arguments: ";
	sprintf (t = (char *) fs_get (strlen (s) + strlen (tmp) + 1),"%s:%s",
		 s,tmp);
	fatal (t);

      case 'S':			/* mm_status() */
	st = (MAILSTREAM *) strtoul (tmp+1,&s,16);
	if (s && (*s++ == ' ')) {
	  status.flags = strtoul (s,&s,10);
	  if (s && (*s++ == ' ')) {
	    status.messages = strtoul (s,&s,10);
	    if (s && (*s++ == ' ')) {
	      status.recent = strtoul (s,&s,10);
	      if (s && (*s++ == ' ')) {
		status.unseen = strtoul (s,&s,10);
		if (s && (*s++ == ' ')) {
		  status.uidnext = strtoul (s,&s,10);
		  if (s && (*s++ == ' ')) {
		    status.uidvalidity = strtoul (s,&s,10);
		    if (s && (*s++ == ' ')) {
		      mm_status ((st == stream) ? stream : NIL,s,&status);
		      break;
		    }
		  }
		}
	      }
	    }
	  }
	}
	s = "Invalid status event arguments: ";
	sprintf (t = (char *) fs_get (strlen (s) + strlen (tmp) + 1),"%s:%s",
		 s,tmp);
	fatal (t);
      case 'C':			/* mm_critical() */
	st = (MAILSTREAM *) strtoul (tmp+1,&s,16);
	mm_critical ((st == stream) ? stream : NIL);
	break;
      case 'X':			/* mm_nocritical() */
	st = (MAILSTREAM *) strtoul (tmp+1,&s,16);
	mm_nocritical ((st == stream) ? stream : NIL);
	break;

      case 'D':			/* mm_diskerror() */
	st = (MAILSTREAM *) strtoul (tmp+1,&s,16);
	if (s && (*s++ == ' ')) {
	  i = strtoul (s,&s,10);
	  if (s && (*s++ == ' ')) {
	    j = (long) strtoul (s,NIL,10);
	    if (st == stream)	/* let's hope it's on usable stream */
	      putc (mm_diskerror (stream,(long) i,j) ? '+' : '-',po);
	    else if (j) {	/* serious diskerror on slave-created stream */
	      mm_log ("Retrying disk write to avoid mailbox corruption!",WARN);
	      sleep (5);	/* give some time for it to clear up */
	      putc ('-',po);	/* don't abort */
	    }
	    else {		/* recoverable on slave-created stream */
	      mm_log ("Error on disk write",ERROR);
	      putc ('+',po);	/* so abort it */
	    }
	    fflush (po);	/* force it out either way */
	    break;
	  }
	}
	s = "Invalid diskerror event arguments: ";
	sprintf (t = (char *) fs_get (strlen (s) + strlen (tmp) + 1),"%s:%s",
		 s,tmp);
	fatal (t);
      case 'F':			/* mm_fatal() */
	mm_fatal (tmp+1);
	break;
      default:			/* random lossage */
	s = "Unknown event from execution process: ";
	sprintf (t = (char *) fs_get (strlen (s) + strlen (tmp) + 1),"%s:%s",
		 s,tmp);
	fatal (t);
      }
    }
    fclose (pi); fclose (po);	/* done with the pipes */
				/* get slave status */
    grim_pid_reap_status (pid,NIL,&ret);
    if (ret & 0177) {		/* signal or stopped */
      sprintf (tmp,"Execution process terminated abnormally (%lx)",ret);
      mm_log (tmp,ERROR);
      ret = NIL;
    }
    else ret >>= 8;		/* return exit code */
    (*bn) (BLOCK_NONSENSITIVE,blockdata);
  }
  return ret;			/* return status */
}

/* Safe driver calls */


/* Safely delete mailbox
 * Accepts: driver to call under slave
 *	    MAIL stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long safe_delete (DRIVER *dtb,MAILSTREAM *stream,char *mbx)
{
  long ret = master (stream,NIL,NIL);
  if (lockslavep) exit ((*dtb->mbxdel) (stream,mbx));
  return ret;
}


/* Safely rename mailbox
 * Accepts: driver to call under slave
 *	    MAIL stream
 *	    old mailbox name
 *	    new mailbox name (or NIL for delete)
 * Returns: T on success, NIL on failure
 */

long safe_rename (DRIVER *dtb,MAILSTREAM *stream,char *old,char *newname)
{
  long ret = master (stream,NIL,NIL);
  if (lockslavep) exit ((*dtb->mbxren) (stream,old,newname));
  return ret;
}


/* Safely get status of mailbox
 * Accepts: driver to call under slave
 *	    MAIL stream
 *	    mailbox name
 *	    status flags
 * Returns: T on success, NIL on failure
 */

long safe_status (DRIVER *dtb,MAILSTREAM *stream,char *mbx,long flags)
{
  long ret = master (stream,NIL,NIL);
  if (lockslavep) exit ((*dtb->status) (stream,mbx,flags));
  return ret;
}


/* Scan file for contents
 * Accepts: file name
 *	    desired contents
 * Returns: NIL if contents not found, T if found
 */

long safe_scan_contents (char *name,char *contents,unsigned long csiz,
			 unsigned long fsiz)
{
  long ret = master (NIL,NIL,NIL);
  if (lockslavep) exit (dummy_scan_contents (name,contents,csiz,fsiz));
  return ret;
}

/* Safely copy message to mailbox
 * Accepts: driver to call under slave
 *	    MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    copy options
 * Returns: T if success, NIL if failed
 */

long safe_copy (DRIVER *dtb,MAILSTREAM *stream,char *seq,char *mbx,long flags)
{
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  long ret = master (stream,NIL,NIL);
  if (lockslavep) {
				/* don't do proxycopy in slave */
    if (pc) mail_parameters (stream,SET_MAILPROXYCOPY,(void *) slaveproxycopy);
    exit ((*dtb->copy) (stream,seq,mbx,flags));
  }
				/* do any proxycopy in master */
  if (lockproxycopy && pc) return (*pc) (stream,seq,mbx,flags);
  return ret;
}


/* Append package for slave */

typedef struct append_data {
  int first;			/* flag indicating first message */
  char *flags;			/* message flags */
  char *date;			/* message date */
  char *msg;			/* message text */
  STRING message;		/* message stringstruct */
} APPENDDATA;


/* Safely append message to mailbox
 * Accepts: driver to call under slave
 *	    MAIL stream
 *	    destination mailbox
 *	    append callback
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

long safe_append (DRIVER *dtb,MAILSTREAM *stream,char *mbx,append_t af,
		  void *data)
{
  long ret = master (stream,af,data);
  if (lockslavep) {
    APPENDDATA ad;
    ad.first = T;		/* initialize initial append package */
    ad.flags = ad.date = ad.msg = NIL;
    exit ((*dtb->append) (stream,mbx,slave_append,&ad));
  }
  return ret;
}

/* Slave callbacks */


/* Message exists (i.e. there are that many messages in the mailbox)
 * Accepts: MAIL stream
 *	    message number
 */

void slave_exists (MAILSTREAM *stream,unsigned long number)
{
  /* this event never passed by slaves */
}


/* Message expunged
 * Accepts: MAIL stream
 *	    message number
 */

void slave_expunged (MAILSTREAM *stream,unsigned long number)
{
  /* this event never passed by slaves */
}


/* Message status changed
 * Accepts: MAIL stream
 *	    message number
 */

void slave_flags (MAILSTREAM *stream,unsigned long number)
{
  /* this event never passed by slaves */
}


/* Mailbox status
 * Accepts: MAIL stream
 *	    mailbox name
 *	    mailbox status
 */

void slave_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status)
{
  fprintf (slaveout,"S%lx %lu %lu %lu %lu %lu %lu %s\n",
	  (unsigned long) stream,status->flags,status->messages,status->recent,
	  status->unseen,status->uidnext,status->uidvalidity,mailbox);
  fflush (slaveout);
}

/* Notification event
 * Accepts: MAIL stream
 *	    string to log
 *	    error flag
 */

void slave_notify (MAILSTREAM *stream,char *string,long errflg)
{
  fprintf (slaveout,"N%lx %lu %s\n",(unsigned long) stream,errflg,string);
  fflush (slaveout);
}


/* Log an event for the user to see
 * Accepts: string to log
 *	    error flag
 */

void slave_log (char *string,long errflg)
{
  fprintf (slaveout,"L%lu %s\n",errflg,string);
  fflush (slaveout);
}


/* About to enter critical code
 * Accepts: stream
 */

void slave_critical (MAILSTREAM *stream)
{
  fprintf (slaveout,"C%lx\n",(unsigned long) stream);
  fflush (slaveout);
}


/* About to exit critical code
 * Accepts: stream
 */

void slave_nocritical (MAILSTREAM *stream)
{
  fprintf (slaveout,"X%lx\n",(unsigned long) stream);
  fflush (slaveout);
}

/* Disk error found
 * Accepts: stream
 *	    system error code
 *	    flag indicating that mailbox may be clobbered
 * Returns: abort flag
 */

long slave_diskerror (MAILSTREAM *stream,long errcode,long serious)
{
  int c;
  fprintf (slaveout,"D%lx %lu %lu\n",(unsigned long) stream,errcode,serious);
  fflush (slaveout);
  if ((c = getc (slavein)) == '+') return LONGT;
  if (c != '-') slave_fatal ("Unknown master response for diskerror");
  return NIL;
}


/* Log a fatal error event
 * Accepts: string to log
 */

void slave_fatal (char *string)
{
  syslog (LOG_ALERT,"IMAP toolkit slave process crash: %.100s",string);
  fprintf (slaveout,"F%s\n",string);
  fflush (slaveout);
  abort ();			/* die */
}

/* Append read buffer
 * Accepts: number of bytes to read
 *	    error message if fails
 * Returns: read-in string
 */

static char *slave_append_read (unsigned long n,char *error)
{
#if 0
  unsigned long i;
#endif
  int c;
  char *t,tmp[MAILTMPLEN];
  char *s = (char *) fs_get (n + 1);
  s[n] = '\0';
#if 0
  /* This doesn't work on Solaris with GCC.  I think that it's a C library
   * bug, since the problem only shows up if the application does fread()
   * on some other file
   */
  for (t = s; n && ((i = fread (t,1,n,slavein)); t += i,n -= i);
#else
  for (t = s; n && ((c = getc (slavein)) != EOF); *t++ = c,--n);
#endif
  if (n) {
    sprintf (tmp,"Error reading %s with %lu bytes remaining",error,n);
    slave_fatal (tmp);
  }
  return s;
}

/* Append message callback
 * Accepts: MAIL stream
 *	    append data package
 *	    pointer to return initial flags
 *	    pointer to return message internal date
 *	    pointer to return stringstruct of message or NIL to stop
 * Returns: T if success (have message or stop), NIL if error
 */

long slave_append (MAILSTREAM *stream,void *data,char **flags,char **date,
		   STRING **message)
{
  char tmp[MAILTMPLEN];
  unsigned long n;
  int c;
  APPENDDATA *ad = (APPENDDATA *) data;
				/* flush text of previous message */
  if (ad->flags) fs_give ((void **) &ad->flags);
  if (ad->date) fs_give ((void **) &ad->date);
  if (ad->msg) fs_give ((void **) &ad->msg);
  *flags = *date = NIL;		/* assume no flags or date */
  fputs ("A\n",slaveout);	/* tell master we're doing append callback */
  fflush (slaveout);
  switch (c = getc (slavein)) {	/* what did master say? */
  case '+':			/* have message, get size of flags */
    for (n = 0; isdigit (c = getc (slavein)); n *= 10, n += (c - '0'));
    if (c != ' ') {
      sprintf (tmp,"Missing delimiter after flag size %lu: %c",n,c);
      slave_fatal (tmp);
    }
    if (n) *flags = ad->flags = slave_append_read (n,"flags");
				/* get size of date */
    for (n = 0; isdigit (c = getc (slavein)); n *= 10, n += (c - '0'));
    if (c != ' ') {
      sprintf (tmp,"Missing delimiter after date size %lu: %c",n,c);
      slave_fatal (tmp);
    }
    if (n) *date = ad->date = slave_append_read (n,"date");
				/* get size of message */
    for (n = 0; isdigit (c = getc (slavein)); n *= 10, n += (c - '0'));
    if (c != ' ') {
      sprintf (tmp,"Missing delimiter after message size %lu: %c",n,c);
      slave_fatal (tmp);
    }
    if (n) {			/* make buffer for message */
      ad->msg = slave_append_read (n,"message");
				/* initialize stringstruct */
      INIT (&ad->message,mail_string,(void *) ad->msg,n);
      ad->first = NIL;		/* no longer first message */
      *message = &ad->message;	/* return message */
    }
    else *message = NIL;	/* empty message */
    return LONGT;
  case '-':			/* error */
    *message = NIL;		/* set stop */
    break;
  default:			/* unknown event */
    sprintf (tmp,"Unknown master response for append: %c",c);
    slave_fatal (tmp);
  }
  return NIL;			/* return failure */
}

/* Proxy copy across mailbox formats
 * Accepts: mail stream
 *	    sequence to copy on this stream
 *	    destination mailbox
 *	    option flags
 * Returns: T if success, else NIL
 */

long slaveproxycopy (MAILSTREAM *stream,char *sequence,char *mailbox,
		     long options)
{
  fputs ("&\n",slaveout);	/* redo copy as append */
  fflush (slaveout);
  return NIL;			/* failure for now */
}
