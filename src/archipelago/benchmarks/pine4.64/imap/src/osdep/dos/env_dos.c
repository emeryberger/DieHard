/*
 * Program:	DOS environment routines
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
 * Last Edited:	8 July 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


static char *myLocalHost = NIL;	/* local host name */
static char *myClientAddr = NIL;/* client host address */
static char *myClientHost = NIL;/* client host name */
static char *myServerAddr = NIL;/* server host address */
static char *myServerHost = NIL;/* server host name */
static char *myHomeDir = NIL;	/* home directory name */
static char *myNewsrc = NIL;	/* newsrc file name */
static long list_max_level = 5;	/* maximum level of list recursion */
static short no822tztext = NIL;	/* disable RFC [2]822 timezone text */
				/* home namespace */
static NAMESPACE nshome = {"",'\\',NIL,NIL};
				/* namespace list */
static NAMESPACE *nslist[3] = {&nshome,NIL,NIL};

#include "write.c"		/* include safe writing routines */
#include "pmatch.c"		/* include wildcard pattern matcher */


/* Dummy definitions to prevent errors */

#define server_login(user,pass,authuser,argc,argv) NIL
#define authserver_login(user,authuser,argc,argv) NIL
#define myusername() ""
#define MD5ENABLE "\\.nosuch.."


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
  case GET_NAMESPACE:
    ret = (void *) nslist;
    break;
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
      sprintf (tmp,"%s\\NEWSRC",myhomedir ());
      myNewsrc = cpystr (tmp);
    }
    ret = (void *) myNewsrc;
    break;
  case SET_LISTMAXLEVEL:
    list_max_level = (long) value;
  case GET_LISTMAXLEVEL:
    ret = (void *) list_max_level;
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
    tzset ();			/* get timezone from TZ environment stuff */
    sprintf (date + strlen (date)," (%.50s)",
	     tzname[daylight ? (((struct tm *) t)->tm_isdst > 0) : 0]);
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
  int i;
  char *s;
  if (!myHomeDir) {		/* get home directory name if not yet known */
    i = strlen (myHomeDir = cpystr ((s = getenv ("HOME")) ? s : ""));
    if (i && ((myHomeDir[i-1] == '\\') || (myHomeDir[i-1]=='/')))
      myHomeDir[i-1] = '\0';	/* tie off trailing directory delimiter */
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
  return ucase (dst);
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

/* Default mailgets routine on DOS
 * Accepts: readin function pointer
 *	    stream to use
 *	    number of bytes
 *	    identifier data
 * Returns: string read in, truncated if necessary
 *
 * This is a sample mailgets routine.  It simply truncates any data larger
 * than 63K.  On most systems, you generally don't use a mailgets
 * routine at all, but on DOS it's required to prevent the application from
 * crashing.
 */

static char *dos_gets_buf = NIL;

char *dos_default_gets (readfn_t f,void *stream,unsigned long size,
			GETS_DATA *md)
{
  readprogress_t *rp = mail_parameters (NIL,GET_READPROGRESS,NIL);
  char *ret,tmp[MAILTMPLEN+1];
  unsigned long i,j,dsc,rdi = 0;
  unsigned long dos_max = 63 * 1024;
  if (!dos_gets_buf)		/* one-time initialization */
    dos_gets_buf = (char *) fs_get ((size_t) dos_max + 1);
  ret = (md->flags & MG_COPY) ?
    ((char *) fs_get ((size_t) size + 1)) : dos_gets_buf;
  if (size > dos_max) {
    sprintf (tmp,"Mailbox %s, %s %lu[%.80s], %lu octets truncated to %ld",
	     md->stream->mailbox,(md->flags & MG_UID) ? "UID" : "#",
	     md->msgno,md->what,size,(long) dos_max);
    mm_log (tmp,WARN);		/* warn user */
    dsc = size - dos_max;	/* number of bytes to discard */
    size = dos_max;		/* maximum length string we can read */
  }
  else dsc = 0;			/* nothing to discard */
  dos_gets_buf[size] = '\0';	/* tie off string */
  if (rp) for (i = size; j = min ((long) MAILTMPLEN,(long) i); i -= j) {
    (*f) (stream,j,ret + rdi);
    (*rp) (md,rdi += j);
  }
  else (*f) (stream,size,dos_gets_buf);
				/* toss out everything after that */
  for (i = dsc; j = min ((long) MAILTMPLEN,(long) i); i -= j) {
    (*f) (stream,j,tmp);
    if (rp) (*rp) (md,rdi += j);
  }
  return ret;
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
