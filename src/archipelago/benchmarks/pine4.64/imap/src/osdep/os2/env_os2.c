/*
 * Program:	OS/2 environment routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	1 August 1988
 * Last Edited:	18 January 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


static char *myLocalHost = NIL;	/* local host name */
static char *myHomeDir = NIL;	/* home directory name */
static char *myNewsrc = NIL;	/* newsrc file name */
static short no822tztext = NIL;	/* disable RFC [2]822 timezone text */

#include "write.c"		/* include safe writing routines */
#include "pmatch.c"		/* include wildcard pattern matcher */


/* Get all authenticators */

#include "auths.c"

/* Environment manipulate parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *env_parameters (long function,void *value)
{
  void *ret = NIL;
  switch ((int) function) {
  case SET_HOMEDIR:
    myHomeDir = cpystr ((char *) value);
  case GET_HOMEDIR:
    ret = (void *) myHomeDir;
    break;
  case SET_LOCALHOST:
    myLocalHost = cpystr ((char *) value);
  case GET_LOCALHOST:
    ret = (void *) myLocalHost;
    break;
  case SET_NEWSRC:
    if (myNewsrc) fs_give ((void **) &myNewsrc);
    myNewsrc = cpystr ((char *) value);
  case GET_NEWSRC:
    if (!myNewsrc) {		/* set news file name if not defined */
      char tmp[MAILTMPLEN];
      sprintf (tmp,"%s\\newsrc",myhomedir ());
      myNewsrc = cpystr (tmp);
    }
    ret = (void *) myNewsrc;
    break;
  case SET_DISABLE822TZTEXT:
    no822tztext = value ? T : NIL;
  case GET_DISABLE822TZTEXT:
    ret = (void *) (no822tztext ? VOIDT : NIL);
    break;
  }
  return ret;
}

/* Write current time
 * Accepts: destination string
 *	    optional format of day-of-week prefix
 *	    format of date and time
 *	    flag whether to append symbolic timezone
 */

static void do_date (char *date,char *prefix,char *fmt,int suffix)
{
  time_t tn = time (0);
  struct tm *t = gmtime (&tn);
  int zone = t->tm_hour * 60 + t->tm_min;
  int julian = t->tm_yday;
  t = localtime (&tn);		/* get local time now */
				/* minus UTC minutes since midnight */
  zone = t->tm_hour * 60 + t->tm_min - zone;
  /* julian can be one of:
   *  36x  local time is December 31, UTC is January 1, offset -24 hours
   *    1  local time is 1 day ahead of UTC, offset +24 hours
   *    0  local time is same day as UTC, no offset
   *   -1  local time is 1 day behind UTC, offset -24 hours
   * -36x  local time is January 1, UTC is December 31, offset +24 hours
   */
  if (julian = t->tm_yday -julian)
    zone += ((julian < 0) == (abs (julian) == 1)) ? -24*60 : 24*60;
  if (prefix) {			/* want day of week? */
    sprintf (date,prefix,days[t->tm_wday]);
    date += strlen (date);	/* make next sprintf append */
  }
				/* output the date */
  sprintf (date,fmt,t->tm_mday,months[t->tm_mon],t->tm_year+1900,
	   t->tm_hour,t->tm_min,t->tm_sec,zone/60,abs (zone) % 60);
  if (suffix) {			/* append timezone suffix if desired */
    char *tz;
    tzset ();			/* get timezone from TZ environment stuff */
    tz = tzname[daylight ? (((struct tm *) t)->tm_isdst > 0) : 0];
    if (tz && tz[0]) {
      char *s;
      for (s = tz; *s; s++) if (*s & 0x80) return;
      sprintf (date + strlen (date)," (%.50s)",tz);
    }
  }
}


/* Write current time in RFC 822 format
 * Accepts: destination string
 */

void rfc822_date (char *date)
{
  do_date (date,"%s, ","%d %s %d %02d:%02d:%02d %+03d%02d",
	   no822tztext ? NIL : T);
}


/* Write current time in fixed-width RFC 822 format
 * Accepts: destination string
 */

void rfc822_fixed_date (char *date)
{
  do_date (date,NIL,"%02d %s %4d %02d:%02d:%02d %+03d%02d",NIL);
}


/* Write current time in internal format
 * Accepts: destination string
 */

void internal_date (char *date)
{
  do_date (date,NIL,"%02d-%s-%d %02d:%02d:%02d %+03d%02d",NIL);
}

/* Return my home directory name
 * Returns: my home directory name
 */

char *myhomedir ()
{
  if (!myHomeDir) {		/* get home directory name if not yet known */
    char *s;
    if ((s = getenv ("PINEHOME")) || (s = getenv ("HOME")) ||
	(s = getenv ("ETC"))) {
      myHomeDir = cpystr (s);
      while (s = strchr (myHomeDir,'/')) *s = '\\';
      if ((s = strrchr (myHomeDir,'\\')) && !s[1]) *s = '\0';
    }
    else myHomeDir = cpystr ("");
  }
  return myHomeDir;
}


/* Return mailbox file name
 * Accepts: destination buffer
 *	    mailbox name
 * Returns: file name
 */

char *mailboxfile (char *dst,char *name)
{
  char *s;
  char *ext = (char *) mail_parameters (NIL,GET_EXTENSION,NIL);
				/* forbid extraneous extensions */
  if ((s = strchr ((s = strrchr (name,'\\')) ? s : name,'.')) &&
      ((ext = (char *) mail_parameters (NIL,GET_EXTENSION,NIL)) ||
       strchr (s+1,'.'))) return NIL;
				/* absolute path name? */
  if ((*name == '\\') || (name[1] == ':')) strcpy (dst,name);
  else sprintf (dst,"%s\\%s",myhomedir (),name);
  if (ext) sprintf (dst + strlen (dst),".%s",ext);
  return dst;
}

/* Lock file name
 * Accepts: return buffer for file name
 *	    file name
 *	    locking to be placed on file if non-NIL
 * Returns: file descriptor of lock or -1 if error
 */

int lockname (char *lock,char *fname,int op)
{
  int ld;
  char c,*s;
  if (!((s = lockdir (lock,getenv ("TEMP"),NIL)) ||
	(s = lockdir (lock,getenv ("TMP"),NIL)) ||
	(s = lockdir (lock,getenv ("TMPDIR"),NIL)) ||
				/* C:\TEMP is last resort */
	(s = lockdir (lock,defaultDrive (),"TEMP")))) {
    mm_log ("Unable to find temporary directory",ERROR);
    return -1;
  }
				/* generate file name */
  while (c = *fname++) switch (c) {
  case '/': case '\\': case ':':
    *s++ = '!';			/* convert bad chars to ! */
    break;
  default:
    *s++ = c;
    break;
  }
  *s++ = c;			/* tie off name */
				/* get the lock */
  if (((ld = open (lock,O_BINARY|O_RDWR|O_CREAT,S_IREAD|S_IWRITE)) >= 0) && op)
    flock (ld,op);		/* apply locking function */
  return ld;			/* return locking file descriptor */
}

/* Build lock directory, check to see if it exists
 * Accepts: return buffer for lock directory
 *	    first part of possible name
 *	    optional second part
 * Returns: pointer to end of buffer if buffer has a good name, else NIL
 */

char *lockdir (char *lock,char *first,char *last)
{
  struct stat sbuf;
  char c,*s;
  if (first && *first) {	/* first part must be non-NIL */
				/* copy first part */
    for (s = lock; *first; c = *s++ = *first++);
    if (last && *last) {	/* copy last part if specified */
				/* write trailing \ in case not in first */
      if (c != '\\') *s++ = '\\';
      while (*last) c = *s++ = *last++;
    }
    if (c == '\\') --s;		/* delete trailing \ if any */
    *s = '\0';			/* tie off name at this point */
    return stat (lock,&sbuf) ? NIL : s;
  }
  return NIL;			/* failed */
}


/* Unlock file descriptor
 * Accepts: file descriptor
 *	    lock file name from lockfd()
 */

void unlockfd (int fd,char *lock)
{
  flock (fd,LOCK_UN);		/* unlock it */
  close (fd);			/* close it */
}


/* Determine default prototype stream to user
 * Accepts: type (NIL for create, T for append)
 * Returns: default prototype stream
 */

MAILSTREAM *default_proto (long type)
{
  extern MAILSTREAM DEFAULTPROTO;
  return &DEFAULTPROTO;		/* return default driver's prototype */
}

/* Global data */

static unsigned rndm = 0;	/* initial `random' number */


/* Return random number
 */

long random ()
{
  if (!rndm) srand (rndm = (unsigned) time (0L));
  return (long) rand ();
}


/* Emulator for BSD syslog() routine
 * Accepts: priority
 *	    message
 *	    parameters
 */

void syslog (int priority,const char *message,...)
{
}


/* Emulator for BSD openlog() routine
 * Accepts: identity
 *	    options
 *	    facility
 */

void openlog (const char *ident,int logopt,int facility)
{
}
