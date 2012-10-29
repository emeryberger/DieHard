/*
 * Program:	UNIX environment routines
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
 * Last Edited:	13 September 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <grp.h>
#include <signal.h>
#include <sys/wait.h>

/* c-client environment parameters */

static char *myUserName = NIL;	/* user name */
static char *myHomeDir = NIL;	/* home directory name */
static char *myMailboxDir = NIL;/* mailbox directory name */
static char *myLocalHost = NIL;	/* local host name */
static char *myNewsrc = NIL;	/* newsrc file name */
static char *mailsubdir = NIL;	/* mail subdirectory name */
static char *sysInbox = NIL;	/* system inbox name */
static char *newsActive = NIL;	/* news active file */
static char *newsSpool = NIL;	/* news spool */
				/* anonymous home directory */
static char *anonymousHome = NIL;
static char *ftpHome = NIL;	/* ftp export home directory */
static char *publicHome = NIL;	/* public home directory */
static char *sharedHome = NIL;	/* shared home directory */
static char *blackBoxDir = NIL;	/* black box directory name */
				/* black box default home directory */
static char *blackBoxDefaultHome = NIL;
static short anonymous = NIL;	/* is anonymous */
static short blackBox = NIL;	/* is a black box */
static short closedBox = NIL;	/* is a closed box */
static short restrictBox = NIL;	/* is a restricted box */
static short has_no_life = NIL;	/* is a cretin with no life */
				/* flock() emulator is a no-op */
static short disableFcntlLock = NIL;
static short hideDotFiles = NIL;/* hide files whose names start with . */
				/* advertise filesystem root */
static short advertisetheworld = NIL;
				/* only advertise own mailboxes and #shared */
static short limitedadvertise = NIL;
				/* disable automatic shared namespaces */
static short noautomaticsharedns = NIL;
static short no822tztext = NIL;	/* disable RFC [2]822 timezone text */
static short netfsstatbug = NIL;/* compensate for broken stat() on network
				 * filesystems (AFS and old NFS).  Don't do
				 * this unless you really have to!
				 */

				/* allow user config files */
static short allowuserconfig = NIL;
				/* 1 = disable plaintext, 2 = if not SSL */
static long disablePlaintext = NIL;
static long list_max_level = 20;/* maximum level of list recursion */
				/* default file protection */
static long mbx_protection = 0600;
				/* default directory protection */
static long dir_protection = 0700;
				/* default lock file protection */
static long lock_protection = MANDATORYLOCKPROT;
				/* default ftp file protection */
static long ftp_protection = 0644;
static long ftp_dir_protection = 0755;
				/* default public file protection */
static long public_protection = 0666;
static long public_dir_protection = 0777;
				/* default shared file protection */
static long shared_protection = 0660;
static long shared_dir_protection = 0770;
static long locktimeout = 5;	/* default lock timeout */
				/* default prototypes */
static MAILSTREAM *createProto = NIL;
static MAILSTREAM *appendProto = NIL;
				/* default user flags */
static char *userFlags[NUSERFLAGS] = {NIL};
static NAMESPACE *nslist[3];	/* namespace list */
static int logtry = 3;		/* number of server login tries */
				/* block notification */
static blocknotify_t mailblocknotify = mm_blocknotify;
				/* logout function */
static logouthook_t maillogouthook = NIL;
				/* logout data */
static void *maillogoutdata = NIL;

/* Note: setting disableLockWarning means that you assert that the
 * so-modified copy of this software will NEVER be used:
 *  1) in conjunction with any software which expects .lock files
 *  2) to access NFS-mounted files and directories
 *
 * Unless both of these conditions apply, then do not set this flag.
 * Instead, read the FAQ (item 7.10) and either use 1777 protection
 * on the mail spool, or install mlock.
 */
				/* disable warning if can't make .lock file */
static short disableLockWarning = NIL;

/* UNIX namespaces */

				/* personal mh namespace */
static NAMESPACE nsmhf = {"#mh/",'/',NIL,NIL};
static NAMESPACE nsmh = {"#mhinbox",NIL,NIL,&nsmhf};
				/* home namespace */
static NAMESPACE nshome = {"",'/',NIL,&nsmh};
				/* UNIX other user namespace */
static NAMESPACE nsunixother = {"~",'/',NIL,NIL};
				/* black box other user namespace */
static NAMESPACE nsblackother = {"/",'/',NIL,NIL};
				/* public (anonymous OK) namespace */
static NAMESPACE nspublic = {"#public/",'/',NIL,NIL};
				/* netnews namespace */
static NAMESPACE nsnews = {"#news.",'.',NIL,&nspublic};
				/* FTP export namespace */
static NAMESPACE nsftp = {"#ftp/",'/',NIL,&nsnews};
				/* shared (no anonymous) namespace */
static NAMESPACE nsshared = {"#shared/",'/',NIL,&nsftp};
				/* world namespace */
static NAMESPACE nsworld = {"/",'/',NIL,&nsshared};
				/* only shared and public namespaces */
static NAMESPACE nslimited = {"#shared/",'/',NIL,&nspublic};

#include "write.c"		/* include safe writing routines */
#include "crexcl.c"		/* include exclusive create */
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
  case GET_NAMESPACE:
    ret = (void *) nslist;
    break;
  case SET_USERNAME:
    if (myUserName) fs_give ((void **) &myUserName);
    myUserName = cpystr ((char *) value);
  case GET_USERNAME:
    ret = (void *) myUserName;
    break;
  case SET_HOMEDIR:
    if (myHomeDir) fs_give ((void **) &myHomeDir);
    myHomeDir = cpystr ((char *) value);
  case GET_HOMEDIR:
    ret = (void *) myHomeDir;
    break;
  case SET_LOCALHOST:
    if (myLocalHost) fs_give ((void **) &myLocalHost);
    myLocalHost = cpystr ((char *) value);
  case GET_LOCALHOST:
    ret = (void *) myLocalHost;
    break;
  case SET_NEWSRC:
    if (myNewsrc) fs_give ((void **) &myNewsrc);
    myNewsrc = cpystr ((char *) value);
  case GET_NEWSRC:
    ret = (void *) myNewsrc;
    break;
  case SET_NEWSACTIVE:
    if (newsActive) fs_give ((void **) &newsActive);
    newsActive = cpystr ((char *) value);
  case GET_NEWSACTIVE:
    ret = (void *) newsActive;
    break;
  case SET_NEWSSPOOL:
    if (newsSpool) fs_give ((void **) &newsSpool);
    newsSpool = cpystr ((char *) value);
  case GET_NEWSSPOOL:
    ret = (void *) newsSpool;
    break;

  case SET_ANONYMOUSHOME:
    if (anonymousHome) fs_give ((void **) &anonymousHome);
    anonymousHome = cpystr ((char *) value);
  case GET_ANONYMOUSHOME:
    if (!anonymousHome) anonymousHome = cpystr (ANONYMOUSHOME);
    ret = (void *) anonymousHome;
    break;
  case SET_FTPHOME:
    if (ftpHome) fs_give ((void **) &ftpHome);
    ftpHome = cpystr ((char *) value);
  case GET_FTPHOME:
    ret = (void *) ftpHome;
    break;
  case SET_PUBLICHOME:
    if (publicHome) fs_give ((void **) &publicHome);
    publicHome = cpystr ((char *) value);
  case GET_PUBLICHOME:
    ret = (void *) publicHome;
    break;
  case SET_SHAREDHOME:
    if (sharedHome) fs_give ((void **) &sharedHome);
    sharedHome = cpystr ((char *) value);
  case GET_SHAREDHOME:
    ret = (void *) sharedHome;
    break;
  case SET_SYSINBOX:
    if (sysInbox) fs_give ((void **) &sysInbox);
    sysInbox = cpystr ((char *) value);
  case GET_SYSINBOX:
    ret = (void *) sysInbox;
    break;
  case SET_LISTMAXLEVEL:
    list_max_level = (long) value;
  case GET_LISTMAXLEVEL:
    ret = (void *) list_max_level;
    break;

  case SET_MBXPROTECTION:
    mbx_protection = (long) value;
  case GET_MBXPROTECTION:
    ret = (void *) mbx_protection;
    break;
  case SET_DIRPROTECTION:
    dir_protection = (long) value;
  case GET_DIRPROTECTION:
    ret = (void *) dir_protection;
    break;
  case SET_LOCKPROTECTION:
    lock_protection = (long) value;
  case GET_LOCKPROTECTION:
    ret = (void *) lock_protection;
    break;
  case SET_FTPPROTECTION:
    ftp_protection = (long) value;
  case GET_FTPPROTECTION:
    ret = (void *) ftp_protection;
    break;
  case SET_PUBLICPROTECTION:
    public_protection = (long) value;
  case GET_PUBLICPROTECTION:
    ret = (void *) public_protection;
    break;
  case SET_SHAREDPROTECTION:
    shared_protection = (long) value;
  case GET_SHAREDPROTECTION:
    ret = (void *) shared_protection;
    break;
  case SET_FTPDIRPROTECTION:
    ftp_dir_protection = (long) value;
  case GET_FTPDIRPROTECTION:
    ret = (void *) ftp_dir_protection;
    break;
  case SET_PUBLICDIRPROTECTION:
    public_dir_protection = (long) value;
  case GET_PUBLICDIRPROTECTION:
    ret = (void *) public_dir_protection;
    break;
  case SET_SHAREDDIRPROTECTION:
    shared_dir_protection = (long) value;
  case GET_SHAREDDIRPROTECTION:
    ret = (void *) shared_dir_protection;
    break;

  case SET_LOCKTIMEOUT:
    locktimeout = (long) value;
  case GET_LOCKTIMEOUT:
    ret = (void *) locktimeout;
    break;
  case SET_DISABLEFCNTLLOCK:
    disableFcntlLock = value ? T : NIL;
  case GET_DISABLEFCNTLLOCK:
    ret = (void *) (disableFcntlLock ? VOIDT : NIL);
    break;
  case SET_LOCKEACCESERROR:
    disableLockWarning = value ? NIL : T;
  case GET_LOCKEACCESERROR:
    ret = (void *) (disableLockWarning ? NIL : VOIDT);
    break;
  case SET_HIDEDOTFILES:
    hideDotFiles = value ? T : NIL;
  case GET_HIDEDOTFILES:
    ret = (void *) (hideDotFiles ? VOIDT : NIL);
    break;
  case SET_DISABLEPLAINTEXT:
    disablePlaintext = (long) value;
  case GET_DISABLEPLAINTEXT:
    ret = (void *) disablePlaintext;
    break;
  case SET_CHROOTSERVER:
    closedBox = value ? T : NIL;
  case GET_CHROOTSERVER:
    ret = (void *) (closedBox ? VOIDT : NIL);
    break;
  case SET_ADVERTISETHEWORLD:
    advertisetheworld = value ? T : NIL;
  case GET_ADVERTISETHEWORLD:
    ret = (void *) (advertisetheworld ? VOIDT : NIL);
    break;
  case SET_LIMITEDADVERTISE:
    limitedadvertise = value ? T : NIL;
  case GET_LIMITEDADVERTISE:
    ret = (void *) (limitedadvertise ? VOIDT : NIL);
    break;
  case SET_DISABLEAUTOSHAREDNS:
    noautomaticsharedns = value ? T : NIL;
  case GET_DISABLEAUTOSHAREDNS:
    ret = (void *) (noautomaticsharedns ? VOIDT : NIL);
    break;
  case SET_DISABLE822TZTEXT:
    no822tztext = value ? T : NIL;
  case GET_DISABLE822TZTEXT:
    ret = (void *) (no822tztext ? VOIDT : NIL);
    break;

  case SET_USERHASNOLIFE:
    has_no_life = value ? T : NIL;
  case GET_USERHASNOLIFE:
    ret = (void *) (has_no_life ? VOIDT : NIL);
    break;
  case SET_NETFSSTATBUG:
    netfsstatbug = value ? T : NIL;
  case GET_NETFSSTATBUG:
    ret = (void *) (netfsstatbug ? VOIDT : NIL);
    break;
  case SET_BLOCKNOTIFY:
    mailblocknotify = (blocknotify_t) value;
  case GET_BLOCKNOTIFY:
    ret = (void *) mailblocknotify;
    break;
  case SET_LOGOUTHOOK:
    maillogouthook = (logouthook_t) value;
  case GET_LOGOUTHOOK:
    ret = maillogouthook;
    break;
  case SET_LOGOUTDATA:
    maillogoutdata = (void *) value;
  case GET_LOGOUTDATA:
    ret = maillogoutdata;
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
				/* append timezone suffix if desired */
  if (suffix) rfc822_timezone (date,(void *) t);
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

/* Initialize server
 * Accepts: server name for syslog or NIL
 *	    /etc/services service name or NIL
 *	    alternate /etc/services service name or NIL
 *	    clock interrupt handler
 *	    kiss-of-death interrupt handler
 *	    hangup interrupt handler
 *	    termination interrupt handler
 */

void server_init (char *server,char *service,char *sslservice,
		  void *clkint,void *kodint,void *hupint,void *trmint)
{
				/* only do this if for init call */
  if (server && service && sslservice) {
    int mask;
    long port;
    struct servent *sv;
				/* set server name in syslog */
    openlog (server,LOG_PID,LOG_MAIL);
    fclose (stderr);		/* possibly save a process ID */
    dorc (NIL,NIL);		/* do systemwide configuration */
    /* Use SSL if SSL service, or if server starts with "s" and not service */
    if (((port = tcp_serverport ()) >= 0)) {
      if ((sv = getservbyname (service,"tcp")) && (port == ntohs (sv->s_port)))
	syslog (LOG_DEBUG,"%s service init from %s",service,tcp_clientaddr ());
      else if ((sv = getservbyname (sslservice,"tcp")) &&
	       (port == ntohs (sv->s_port))) {
	syslog (LOG_DEBUG,"%s SSL service init from %s",sslservice,
		tcp_clientaddr ());
	ssl_server_init (server);
      }
      else {			/* not service or SSL service port */
	syslog (LOG_DEBUG,"port %ld service init from %s",port,
		tcp_clientaddr ());
	if (*server == 's') ssl_server_init (server);
      }
    }
    switch (mask = umask (022)){/* check old umask */
    case 0:			/* definitely unreasonable */
    case 022:			/* don't need to change it */
      break;
    default:			/* already was a reasonable value */
      umask (mask);		/* so change it back */
    }
  }
  arm_signal (SIGALRM,clkint);	/* prepare for clock interrupt */
  arm_signal (SIGUSR2,kodint);	/* prepare for Kiss Of Death */
  arm_signal (SIGHUP,hupint);	/* prepare for hangup */
  arm_signal (SIGTERM,trmint);	/* prepare for termination */
}

/* Wait for stdin input
 * Accepts: timeout in seconds
 * Returns: T if have input on stdin, else NIL
 */

long server_input_wait (long seconds)
{
  fd_set rfd,efd;
  struct timeval tmo;
  FD_ZERO (&rfd);
  FD_ZERO (&efd);
  FD_SET (0,&rfd);
  FD_SET (0,&efd);
  tmo.tv_sec = seconds; tmo.tv_usec = 0;
  return select (1,&rfd,0,&efd,&tmo) ? LONGT : NIL;
}

/* Return UNIX password entry for user name
 * Accepts: user name string
 * Returns: password entry
 *
 * Tries all-lowercase form of user name if given user name fails
 */

static struct passwd *pwuser (unsigned char *user)
{
  unsigned char *s;
  struct passwd *pw = getpwnam (user);
  if (!pw) {			/* failed, see if any uppercase characters */
    for (s = user; *s && !isupper (*s); s++);
    if (*s) {			/* yes, try all lowercase form */
      pw = getpwnam (s = lcase (cpystr (user)));
      fs_give ((void **) &s);
    }
  }
  return pw;
}


/* Validate password for user name
 * Accepts: user name string
 *	    password string
 *	    argument count
 *	    argument vector
 * Returns: password entry if validated
 *
 * Tries password+1 if password fails and starts with space
 */

static struct passwd *valpwd (char *user,char *pwd,int argc,char *argv[])
{
  char *s;
  struct passwd *pw;
  struct passwd *ret = NIL;
  if (auth_md5.server) {	/* using CRAM-MD5 authentication? */
    if (s = auth_md5_pwd (user)) {
      if (!strcmp (s,pwd) || ((*pwd == ' ') && pwd[1] && !strcmp (s,pwd+1)))
	ret = pwuser (user);	/* validated, get passwd entry for user */
      memset (s,0,strlen (s));	/* erase sensitive information */
      fs_give ((void **) &s);
    }
  }
  else if (pw = pwuser (user)) {/* can get user? */
    s = cpystr (pw->pw_name);	/* copy returned name in case we need it */
    if (*pwd && !(ret = checkpw (pw,pwd,argc,argv)) &&
	(*pwd == ' ') && pwd[1] && (ret = pwuser (s)))
      ret = checkpw (pw,pwd+1,argc,argv);
    fs_give ((void **) &s);	/* don't need copy of name any more */
  }
  return ret;
}

/* Server log in
 * Accepts: user name string
 *	    password string
 *	    authenticating user name string
 *	    argument count
 *	    argument vector
 * Returns: T if password validated, NIL otherwise
 */

long server_login (char *user,char *pwd,char *authuser,int argc,char *argv[])
{
  struct passwd *pw = NIL;
  int level = LOG_NOTICE;
  char *err = "failed";
				/* cretins still haven't given up */
  if ((strlen (user) >= NETMAXUSER) ||
      (authuser && (strlen (authuser) >= NETMAXUSER))) {
    level = LOG_ALERT;		/* escalate this alert */
    err = "SYSTEM BREAK-IN ATTEMPT";
    logtry = 0;			/* render this session useless */
  }
  else if (logtry-- <= 0) err = "excessive login failures";
  else if (disablePlaintext) err = "disabled";
  else if (!(authuser && *authuser)) pw = valpwd (user,pwd,argc,argv);
  else if (valpwd (authuser,pwd,argc,argv)) pw = pwuser (user);
  if (pw && pw_login (pw,authuser,pw->pw_name,NIL,argc,argv)) return T;
  syslog (level|LOG_AUTH,"Login %s user=%.64s auth=%.64s host=%.80s",err,
	  user,(authuser && *authuser) ? authuser : user,tcp_clienthost ());
  sleep (3);			/* slow down possible cracker */
  return NIL;
}

/* Authenticated server log in
 * Accepts: user name string
 *	    authenticating user name string
 *	    argument count
 *	    argument vector
 * Returns: T if password validated, NIL otherwise
 */

long authserver_login (char *user,char *authuser,int argc,char *argv[])
{
  return pw_login (pwuser (user),authuser,user,NIL,argc,argv);
}


/* Log in as anonymous daemon
 * Accepts: argument count
 *	    argument vector
 * Returns: T if successful, NIL if error
 */

long anonymous_login (int argc,char *argv[])
{
				/* log in Mr. A. N. Onymous */
  return pw_login (getpwnam (ANONYMOUSUSER),NIL,NIL,
		   (char *) mail_parameters (NIL,GET_ANONYMOUSHOME,NIL),
		   argc,argv);
}

/* Finish log in and environment initialization
 * Accepts: passwd struct for loginpw()
 *	    optional authentication user name
 *	    user name (NIL for anonymous)
 *	    home directory (NIL to use directory from passwd struct)
 *	    argument count
 *	    argument vector
 * Returns: T if successful, NIL if error
 */

long pw_login (struct passwd *pw,char *auser,char *user,char *home,int argc,
	       char *argv[])
{
  struct group *gr;
  char **t;
  long ret = NIL;
  if (pw && pw->pw_uid) {	/* must have passwd struct for non-UID 0 */
				/* make safe copies of user and home */
    if (user) user = cpystr (pw->pw_name);
    home = cpystr (home ? home : pw->pw_dir);
				/* authorization ID .NE. authentication ID? */
    if (user && auser && *auser && compare_cstring (auser,user)) {
				/* scan list of mail administrators */
      if ((gr = getgrnam (ADMINGROUP)) && (t = gr->gr_mem)) while (*t && !ret)
	if (!compare_cstring (auser,*t++))
	  ret = pw_login (pw,NIL,user,home,argc,argv);
      syslog (LOG_NOTICE|LOG_AUTH,"%s %.80s override of user=%.80s host=%.80s",
	      ret ? "Admin" : "Failed",auser,user,tcp_clienthost ());
    }
    else if (closedBox) {	/* paranoid site, lock out other directories */
      if (chdir (home) || chroot (home))
	syslog (LOG_NOTICE|LOG_AUTH,
		"Login %s failed: unable to set chroot=%.80s host=%.80s",
		pw->pw_name,home,tcp_clienthost ());
      else if (loginpw (pw,argc,argv)) ret = env_init (user,NIL);
      else fatal ("Login failed after chroot");
    }
				/* normal login */
    else if (((pw->pw_uid == geteuid ()) || loginpw (pw,argc,argv)) &&
	     (ret = env_init (user,home))) chdir (myhomedir ());
    fs_give ((void **) &home);	/* clean up */
    if (user) fs_give ((void **) &user);
  }
  endpwent ();			/* in case shadow passwords in pw data */
  return ret;			/* return status */
}

/* Initialize environment
 * Accepts: user name (NIL for anonymous)
 *	    home directory name
 * Returns: T, always
 */

long env_init (char *user,char *home)
{
  extern MAILSTREAM CREATEPROTO;
  extern MAILSTREAM EMPTYPROTO;
  struct passwd *pw;
  struct stat sbuf;
  char tmp[MAILTMPLEN];
  if (myUserName) fatal ("env_init called twice!");
				/* initially nothing in namespace list */
  nslist[0] = nslist[1] = nslist[2] = NIL;
				/* myUserName must be set before dorc() call */
  myUserName = cpystr (user ? user : ANONYMOUSUSER);
  dorc (NIL,NIL);		/* do systemwide configuration */
  if (!home) {			/* closed box server */
				/* standard user can only reference home */
    if (user) nslist[0] = &nshome;
    else {			/* anonymous user */
      nslist[0] = &nsblackother; /* set root */
      anonymous = T;		/* flag as anonymous */
    }
    myHomeDir = cpystr ("");	/* home directory is root */
    sysInbox = cpystr ("INBOX");/* make system INBOX */
  }
  else {			/* open or black box */
    closedBox = NIL;		/* definitely not a closed box */
    if (user) {			/* remember user name and home directory */
      if (blackBoxDir) {	/* build black box directory name */
	sprintf (tmp,"%s/%s",blackBoxDir,myUserName);
				/* must exist */
	if (!((!stat (home = tmp,&sbuf) && (sbuf.st_mode & S_IFDIR)) ||
	      (blackBoxDefaultHome &&
	       !stat (home = blackBoxDefaultHome,&sbuf) &&
	       (sbuf.st_mode & S_IFDIR)))) fatal ("no home");
	sysInbox = (char *) fs_get (strlen (home) + 7);
				/* set system INBOX */
	sprintf (sysInbox,"%s/INBOX",home);
	blackBox = T;		/* mark that it's a black box */
				/* mbox meaningless if black box */
	mail_parameters (NIL,DISABLE_DRIVER,(void *) "mbox");
      }
      nslist[0] = &nshome;	/* home namespace */
				/* limited advertise namespaces */
      if (limitedadvertise) nslist[2] = &nslimited;
      else if (blackBox) {	/* black box namespaces */
	nslist[1] = &nsblackother;
	nslist[2] = &nsshared;
      }
      else {			/* open box namespaces */
	nslist[1] = &nsunixother;
	nslist[2] = advertisetheworld ? &nsworld : &nsshared;
      }
    }
    else {
      nslist[2] = &nsftp;	/* anonymous user */
      sprintf (tmp,"%s/INBOX",
	       home = (char *) mail_parameters (NIL,GET_ANONYMOUSHOME,NIL));
      sysInbox = cpystr (tmp);	/* make system INBOX */
      anonymous = T;		/* flag as anonymous */
    }
    myHomeDir = cpystr (home);	/* set home directory */
  }

  if (allowuserconfig) {	/* allow user config files */
    dorc (strcat (strcpy (tmp,myHomeDir),"/.mminit"),T);
    dorc (strcat (strcpy (tmp,myHomeDir),"/.imaprc"),NIL);
  }
  if (!closedBox && !noautomaticsharedns) {
				/* #ftp namespace */
    if (!ftpHome && (pw = getpwnam ("ftp"))) ftpHome = cpystr (pw->pw_dir);
				/* #public namespace */
    if (!publicHome && (pw = getpwnam ("imappublic")))
      publicHome = cpystr (pw->pw_dir);
				/* #shared namespace */
    if (!anonymous && !sharedHome && (pw = getpwnam ("imapshared")))
      sharedHome = cpystr (pw->pw_dir);
  }
  if (!myLocalHost) mylocalhost ();
  if (!myNewsrc) myNewsrc = cpystr(strcat (strcpy (tmp,myHomeDir),"/.newsrc"));
  if (!newsActive) newsActive = cpystr (ACTIVEFILE);
  if (!newsSpool) newsSpool = cpystr (NEWSSPOOL);
				/* force default prototype to be set */
  if (!createProto) createProto = &CREATEPROTO;
  if (!appendProto) appendProto = &EMPTYPROTO;
				/* re-do open action to get flags */
  (*createProto->dtb->open) (NIL);
  endpwent ();			/* close pw database */
  return T;
}
 
/* Return my user name
 * Accepts: pointer to optional flags
 * Returns: my user name
 */

char *myusername_full (unsigned long *flags)
{
  struct passwd *pw;
  struct stat sbuf;
  char *s;
  unsigned long euid;
  char *ret = UNLOGGEDUSER;
				/* no user name yet and not root? */
  if (!myUserName && (euid = geteuid ())) {
				/* yes, look up getlogin() user name or EUID */
    if (((s = (char *) getlogin ()) && *s && (strlen (s) < NETMAXUSER) &&
	 (pw = getpwnam (s)) && (pw->pw_uid == euid)) ||
	(pw = getpwuid (euid)))
      env_init (pw->pw_name,
		((s = getenv ("HOME")) && *s && (strlen (s) < NETMAXMBX) &&
		 !stat (s,&sbuf) && ((sbuf.st_mode & S_IFMT) == S_IFDIR)) ?
		s : pw->pw_dir);
    else fatal ("Unable to look up user name");
  }
  if (myUserName) {		/* logged in? */
    if (flags) *flags = anonymous ? MU_ANONYMOUS : MU_LOGGEDIN;
    ret = myUserName;		/* return user name */
  }
  else if (flags) *flags = MU_NOTLOGGEDIN;
  return ret;
}


/* Return my local host name
 * Returns: my local host name
 */

char *mylocalhost ()
{
  char tmp[MAILTMPLEN];
  if (!myLocalHost) {
    gethostname(tmp,MAILTMPLEN);/* get local host name */
    myLocalHost = cpystr (tcp_canonical (tmp));
  }
  return myLocalHost;
}

/* Return my home directory name
 * Returns: my home directory name
 */

char *myhomedir ()
{
  if (!myHomeDir) myusername ();/* initialize if first time */
  return myHomeDir ? myHomeDir : "";
}


/* Return my home mailbox name
 * Returns: my home directory name
 */

static char *mymailboxdir ()
{
  char *home = myhomedir ();
				/* initialize if first time */
  if (!myMailboxDir && myHomeDir) {
    if (mailsubdir) {
      char tmp[MAILTMPLEN];
      sprintf (tmp,"%s/%s",home,mailsubdir);
      myMailboxDir = cpystr (tmp);/* use pre-defined subdirectory of home */
    }
    else myMailboxDir = cpystr (home);
  }
  return myMailboxDir ? myMailboxDir : "";
}


/* Return system standard INBOX
 * Accepts: buffer string
 */

char *sysinbox ()
{
  char tmp[MAILTMPLEN];
  if (!sysInbox) {		/* initialize if first time */
    sprintf (tmp,"%s/%s",MAILSPOOL,myusername ());
    sysInbox = cpystr (tmp);	/* system inbox is from mail spool */
  }
  return sysInbox;
}

/* Return mailbox directory name
 * Accepts: destination buffer
 *	    directory prefix
 *	    name in directory
 * Returns: file name or NIL if error
 */

char *mailboxdir (char *dst,char *dir,char *name)
{
  char tmp[MAILTMPLEN];
  if (dir || name) {		/* if either argument provided */
    if (dir) {
      if (strlen (dir) > NETMAXMBX) return NIL;
      strcpy (tmp,dir);		/* write directory prefix */
    }
    else tmp[0] = '\0';		/* otherwise null string */
    if (name) {
      if (strlen (name) > NETMAXMBX) return NIL;
      strcat (tmp,name);	/* write name in directory */
    }
				/* validate name, return its name */
    if (!mailboxfile (dst,tmp)) return NIL;
  }
				/* no arguments, wants mailbox directory */
  else strcpy (dst,mymailboxdir ());
  return dst;			/* return the name */
}

/* Return mailbox file name
 * Accepts: destination buffer
 *	    mailbox name
 * Returns: file name or empty string for driver-selected INBOX or NIL if error
 */

char *mailboxfile (char *dst,char *name)
{
  struct passwd *pw;
  char *s;
  if (!name || !*name || (*name == '{') || (strlen (name) > NETMAXMBX) ||
      ((anonymous || blackBox || restrictBox || (*name == '#')) &&
       (strstr (name,"..") || strstr (name,"//") || strstr (name,"/~"))))
    dst = NIL;			/* invalid name */
  else switch (*name) {		/* determine mailbox type based upon name */
  case '#':			/* namespace name */
				/* #ftp/ namespace */
    if (((name[1] == 'f') || (name[1] == 'F')) &&
	((name[2] == 't') || (name[2] == 'T')) &&
	((name[3] == 'p') || (name[3] == 'P')) &&
	(name[4] == '/') && ftpHome) sprintf (dst,"%s/%s",ftpHome,name+5);
				/* #public/ and #shared/ namespaces */
    else if ((((name[1] == 'p') || (name[1] == 'P')) &&
	      ((name[2] == 'u') || (name[2] == 'U')) &&
	      ((name[3] == 'b') || (name[3] == 'B')) &&
	      ((name[4] == 'l') || (name[4] == 'L')) &&
	      ((name[5] == 'i') || (name[5] == 'I')) &&
	      ((name[6] == 'c') || (name[6] == 'C')) &&
	      (name[7] == '/') && (s = publicHome)) ||
	     (!anonymous && ((name[1] == 's') || (name[1] == 'S')) &&
	      ((name[2] == 'h') || (name[2] == 'H')) &&
	      ((name[3] == 'a') || (name[3] == 'A')) &&
	      ((name[4] == 'r') || (name[4] == 'R')) &&
	      ((name[5] == 'e') || (name[5] == 'E')) &&
	      ((name[6] == 'd') || (name[6] == 'D')) &&
	      (name[7] == '/') && (s = sharedHome)))
      sprintf (dst,"%s/%s",s,compare_cstring (name+8,"INBOX") ?
	       name+8 : "INBOX");
    else dst = NIL;		/* unknown namespace */
    break;

  case '/':			/* root access */
    if (anonymous) dst = NIL;	/* anonymous forbidden to do this */
    else if (blackBox) {	/* other user access if blackbox */
      if (restrictBox & RESTRICTOTHERUSER) dst = NIL;
				/* see if other user INBOX */
      else if ((s = strchr (name+1,'/')) && !compare_cstring (s+1,"INBOX")) {
	*s = '\0';		/* temporarily tie off string */
	sprintf (dst,"%s/%s/INBOX",blackBoxDir,name+1);
	*s = '/';		/* in case caller cares */
      }
      else sprintf (dst,"%s/%s",blackBoxDir,name+1);
    }
    else if ((restrictBox & RESTRICTROOT) && strcmp (name,sysinbox ()))
      dst = NIL;		/* restricted and not access to sysinbox */
    else strcpy (dst,name);	/* unrestricted, copy root name */
    break;
  case '~':			/* other user access */
				/* bad syntax or anonymous can't win */
    if (!*++name || anonymous) dst = NIL;
				/* ~/ equivalent to ordinary name */
    else if (*name == '/') sprintf (dst,"%s/%s",mymailboxdir (),name+1);
				/* other user forbidden if closed/restricted */
    else if (closedBox || (restrictBox & RESTRICTOTHERUSER)) dst = NIL;
    else if (blackBox) {	/* black box form of other user */
				/* see if other user INBOX */
      if ((s = strchr (name,'/')) && compare_cstring (s+1,"INBOX")) {
	*s = '\0';		/* temporarily tie off string */
	sprintf (dst,"%s/%s/INBOX",blackBoxDir,name);
	*s = '/';		/* in case caller cares */
      }
      else sprintf (dst,"%s/%s",blackBoxDir,name);
    }
    else {			/* clear box other user */
				/* copy user name */
      for (s = dst; *name && (*name != '/'); *s++ = *name++);
      *s++ = '\0';		/* tie off user name, look up in passwd file */
      if ((pw = getpwnam (dst)) && pw->pw_dir) {
	if (*name) name++;	/* skip past the slash */
				/* canonicalize case of INBOX */
	if (!compare_cstring (name,"INBOX")) name = "INBOX";
				/* remove trailing / from directory */
	if ((s = strrchr (pw->pw_dir,'/')) && !s[1]) *s = '\0';
				/* don't allow ~root/ if restricted root */
	if ((restrictBox & RESTRICTROOT) && !*pw->pw_dir) dst = NIL;
				/* build final name w/ subdir if needed */
	else if (mailsubdir) sprintf (dst,"%s/%s/%s",pw->pw_dir,mailsubdir,name);
	else sprintf (dst,"%s/%s",pw->pw_dir,name);
      }
      else dst = NIL;		/* no such user */
    }
    break;

  case 'I': case 'i':		/* possible INBOX */
    if (!compare_cstring (name+1,"NBOX")) {
				/* if restricted, use INBOX in mailbox dir */
      if (anonymous || blackBox || closedBox)
	sprintf (dst,"%s/INBOX",mymailboxdir ());
      else *dst = '\0';		/* otherwise driver selects the name */
      break;
    }
				/* drop into to ordinary name case */
  default:			/* ordinary name is easy */
    sprintf (dst,"%s/%s",mymailboxdir (),name);
    break;
  }
  return dst;			/* return final name */
}

/* Dot-lock file locker
 * Accepts: file name to lock
 *	    destination buffer for lock file name
 *	    open file description on file name to lock
 * Returns: T if success, NIL if failure
 */

long dotlock_lock (char *file,DOTLOCK *base,int fd)
{
  int i = locktimeout * 60;
  int j,mask,retry,pi[2],po[2];
  char *s,tmp[MAILTMPLEN];
  struct stat sb;
				/* flush absurd file name */
  if (strlen (file) > 512) return NIL;
				/* build lock filename */
  sprintf (base->lock,"%s.lock",file);
				/* assume no pipe */
  base->pipei = base->pipeo = -1;
  do {				/* make sure not symlink */
    if (!(j = chk_notsymlink (base->lock,&sb))) return NIL;
				/* time out if file older than 5 minutes */
    if ((j > 0) && ((time (0)) >= (sb.st_ctime + locktimeout * 60))) i = 0;
				/* try to create the lock */
    switch (retry = crexcl (base->lock)) {
    case -1:			/* OK to retry */
      if (!(i%15)) {		/* time to notify? */
	sprintf (tmp,"Mailbox %.80s is locked, will override in %d seconds...",
		 file,i);
	MM_LOG (tmp,WARN);
      }
      sleep (1);		/* wait 1 second before next try */
      break;
    case NIL:			/* failure, can't retry */
      i = 0;
      break;
    case T:			/* success, make sure others can break lock */
      chmod (base->lock,(int) lock_protection);
      return LONGT;
    }
  } while (i--);		/* until out of retries */
  if (retry < 0) {		/* still returning retry after locktimeout? */
    if (!(j = chk_notsymlink (base->lock,&sb))) return NIL;
    if ((j > 0) && ((time (0)) < (sb.st_ctime + locktimeout * 60))) {
      sprintf (tmp,"Mailbox vulnerable - seizing %ld second old lock",
	       (long) (time (0) - sb.st_ctime));
      MM_LOG (tmp,WARN);
    }
    mask = umask (0);		/* want our lock protection */
    unlink (base->lock);	/* try to remove the old file */
				/* seize the lock */
    if ((i = open (base->lock,O_WRONLY|O_CREAT,(int) lock_protection)) >= 0) {
      close (i);		/* don't need descriptor any more */
      sprintf (tmp,"Mailbox %.80s lock overridden",file);
      MM_LOG (tmp,NIL);
      chmod (base->lock,(int) lock_protection);
      umask (mask);		/* restore old umask */
      return LONGT;
    }
    umask (mask);		/* restore old umask */
  }

  if (fd >= 0) switch (errno) {
  case EACCES:			/* protection failure? */
				/* make command pipes */
    if (!closedBox && !stat (LOCKPGM,&sb) && (pipe (pi) >= 0)) {
      if (pipe (po) >= 0) {
	if (!(j = fork ())) {	/* make inferior process */
	  if (!fork ()) {	/* make grandchild so it's inherited by init */
	    char *argv[4];
				/* prepare argument vector */
	    sprintf (tmp,"%d",fd);
	    argv[0] = LOCKPGM; argv[1] = tmp;
	    argv[2] = file; argv[3] = NIL;
				/* set parent's I/O to my O/I */
	    dup2 (pi[1],1); dup2 (pi[1],2); dup2 (po[0],0);
				/* close all unnecessary descriptors */
	    for (j = max (20,max (max (pi[0],pi[1]),max(po[0],po[1])));
		 j >= 3; --j) if (j != fd) close (j);
				/* be our own process group */
	    setpgrp (0,getpid ());
				/* now run it */
	    execv (argv[0],argv);
	  }
	  _exit (1);		/* child is done */
	}
	else if (j > 0) {	/* reap child; grandchild now owned by init */
	  grim_pid_reap (j,NIL);
				/* read response from locking program */
	  if ((read (pi[0],tmp,1) == 1) && (tmp[0] == '+')) {
				/* success, record pipes */
	    base->pipei = pi[0]; base->pipeo = po[1];
				/* close child's side of the pipes */
	    close (pi[1]); close (po[0]);
	    return LONGT;
	  }
	}
	close (po[0]); close (po[1]);
      }
      close (pi[0]); close (pi[1]);
    }
				/* find directory/file delimiter */
    if (s = strrchr (base->lock,'/')) {
      *s = '\0';		/* tie off at directory */
      sprintf(tmp,		/* generate default message */
	      "Mailbox vulnerable - directory %.80s must have 1777 protection",
	      base->lock);
				/* definitely not 1777 if can't stat */
      mask = stat (base->lock,&sb) ? 0 : (sb.st_mode & 1777);
      *s = '/';			/* restore lock name */
      if (mask != 1777) {	/* default warning if not 1777 */
	if (!disableLockWarning) MM_LOG (tmp,WARN);
	break;
      }
    }
  default:
    sprintf (tmp,"Mailbox vulnerable - error creating %.80s: %s",
	     base->lock,strerror (errno));
    if (!disableLockWarning) MM_LOG (tmp,WARN);
    break;
  }
  base->lock[0] = '\0';		/* don't use lock files */
  return NIL;
}

/* Dot-lock file unlocker
 * Accepts: lock file name
 * Returns: T if success, NIL if failure
 */

long dotlock_unlock (DOTLOCK *base)
{
  long ret = LONGT;
  if (base && base->lock[0]) {
    if (base->pipei >= 0) {	/* if running through a pipe unlocker */
      ret = (write (base->pipeo,"+",1) == 1);
				/* nuke the pipes */
      close (base->pipei); close (base->pipeo);
    }
    else ret = !unlink (base->lock);
  }
  return ret;
}

/* Lock file name
 * Accepts: scratch buffer
 *	    file name
 *	    type of locking operation (LOCK_SH or LOCK_EX)
 *	    pointer to return PID of locker
 * Returns: file descriptor of lock or negative if error
 */

int lockname (char *lock,char *fname,int op,long *pid)
{
  struct stat sbuf;
  *pid = 0;			/* no locker PID */
  return stat (fname,&sbuf) ? -1 : lock_work (lock,&sbuf,op,pid);
}


/* Lock file descriptor
 * Accepts: file descriptor
 *	    lock file name buffer
 *	    type of locking operation (LOCK_SH or LOCK_EX)
 * Returns: file descriptor of lock or negative if error
 */

int lockfd (int fd,char *lock,int op)
{
  struct stat sbuf;
  return fstat (fd,&sbuf) ? -1 : lock_work (lock,&sbuf,op,NIL);
}

/* Lock file name worker
 * Accepts: lock file name
 *	    pointer to stat() buffer
 *	    type of locking operation (LOCK_SH or LOCK_EX)
 *	    pointer to return PID of locker
 * Returns: file descriptor of lock or negative if error
 */

int lock_work (char *lock,void *sb,int op,long *pid)
{
  struct stat lsb,fsb;
  struct stat *sbuf = (struct stat *) sb;
  char tmp[MAILTMPLEN];
  long i;
  int fd;
  int mask = umask (0);
  if (pid) *pid = 0;		/* initialize return PID */
				/* make temporary lock file name */
  sprintf (lock,"%s/.%lx.%lx",closedBox ? "" : "/tmp",
	   (unsigned long) sbuf->st_dev,(unsigned long) sbuf->st_ino);
  while (T) {			/* until get a good lock */
    do switch ((int) chk_notsymlink (lock,&lsb)) {
    case 1:			/* exists just once */
      if (((fd = open (lock,O_RDWR,lock_protection)) >= 0) ||
	  (errno != ENOENT) || (chk_notsymlink (lock,&lsb) >= 0)) break;
    case -1:			/* name doesn't exist */
      fd = open (lock,O_RDWR|O_CREAT|O_EXCL,lock_protection);
      break;
    default:			/* multiple hard links */
      MM_LOG ("hard link to lock name",ERROR);
      syslog (LOG_CRIT,"SECURITY PROBLEM: hard link to lock name: %.80s",lock);
    case 0:			/* symlink (already did syslog) */
      umask (mask);		/* restore old mask */
      return -1;		/* fail: no lock file */
    } while ((fd < 0) && (errno == EEXIST));
    if (fd < 0) {		/* failed to get file descriptor */
      syslog (LOG_INFO,"Mailbox lock file %s open failure: %s",lock,
	      strerror (errno));
      if (!closedBox) {		/* more explicit snarl for bad configuration */
	if (stat ("/tmp",&lsb))
	  syslog (LOG_CRIT,"SYSTEM ERROR: no /tmp: %s",strerror (errno));
	else if ((lsb.st_mode & 01777) != 01777)
	  MM_LOG ("Can't lock for write: /tmp must have 1777 protection",WARN);
      }
      umask (mask);		/* restore old mask */
      return -1;		/* fail: can't open lock file */
    }

				/* non-blocking form */
    if (op & LOCK_NB) i = flock (fd,op);
    else {			/* blocking form */
      (*mailblocknotify) (BLOCK_FILELOCK,NIL);
      i = flock (fd,op);
      (*mailblocknotify) (BLOCK_NONE,NIL);
    }
    if (i) {			/* failed, get other process' PID */
      if (pid && !fstat (fd,&fsb) && (i = min (fsb.st_size,MAILTMPLEN-1)) &&
	  (read (fd,tmp,i) == i) && !(tmp[i] = 0) && ((i = atol (tmp)) > 0))
	*pid = i;
      close (fd);		/* failed, give up on lock */
      umask (mask);		/* restore old mask */
      return -1;		/* fail: can't lock */
    }
				/* make sure this lock is good for us */
    if (!lstat (lock,&lsb) && ((lsb.st_mode & S_IFMT) != S_IFLNK) &&
	!fstat (fd,&fsb) && (lsb.st_dev == fsb.st_dev) &&
	(lsb.st_ino == fsb.st_ino) && (fsb.st_nlink == 1)) break;
    close (fd);			/* lock not right, drop fd and try again */
  }
				/* make sure mode OK (don't use fchmod()) */
  chmod (lock,(int) lock_protection);
  umask (mask);			/* restore old mask */
  return fd;			/* success */
}

/* Check to make sure not a symlink
 * Accepts: file name
 *	    stat buffer
 * Returns: -1 if doesn't exist, NIL if symlink, else number of hard links
 */

long chk_notsymlink (char *name,void *sb)
{
  struct stat *sbuf = (struct stat *) sb;
				/* name exists? */
  if (lstat (name,sbuf)) return -1;
				/* forbid symbolic link */
  if ((sbuf->st_mode & S_IFMT) == S_IFLNK) {
    MM_LOG ("symbolic link on lock name",ERROR);
    syslog (LOG_CRIT,"SECURITY PROBLEM: symbolic link on lock name: %.80s",
	    name);
    return NIL;
  }
  return (long) sbuf->st_nlink;	/* return number of hard links */
}


/* Unlock file descriptor
 * Accepts: file descriptor
 *	    lock file name from lockfd()
 */

void unlockfd (int fd,char *lock)
{
				/* delete the file if no sharers */
  if (!flock (fd,LOCK_EX|LOCK_NB)) unlink (lock);
  flock (fd,LOCK_UN);		/* unlock it */
  close (fd);			/* close it */
}

/* Set proper file protection for mailbox
 * Accepts: mailbox name
 *	    actual file path name
 * Returns: T, always
 */

long set_mbx_protections (char *mailbox,char *path)
{
  struct stat sbuf;
  int mode = (int) mbx_protection;
  if (*mailbox == '#') {	/* possible namespace? */
      if (((mailbox[1] == 'f') || (mailbox[1] == 'F')) &&
	  ((mailbox[2] == 't') || (mailbox[2] == 'T')) &&
	  ((mailbox[3] == 'p') || (mailbox[3] == 'P')) &&
	  (mailbox[4] == '/')) mode = (int) ftp_protection;
      else if (((mailbox[1] == 'p') || (mailbox[1] == 'P')) &&
	       ((mailbox[2] == 'u') || (mailbox[2] == 'U')) &&
	       ((mailbox[3] == 'b') || (mailbox[3] == 'B')) &&
	       ((mailbox[4] == 'l') || (mailbox[4] == 'L')) &&
	       ((mailbox[5] == 'i') || (mailbox[5] == 'I')) &&
	       ((mailbox[6] == 'c') || (mailbox[6] == 'C')) &&
	       (mailbox[7] == '/')) mode = (int) public_protection;
      else if (((mailbox[1] == 's') || (mailbox[1] == 'S')) &&
	       ((mailbox[2] == 'h') || (mailbox[2] == 'H')) &&
	       ((mailbox[3] == 'a') || (mailbox[3] == 'A')) &&
	       ((mailbox[4] == 'r') || (mailbox[4] == 'R')) &&
	       ((mailbox[5] == 'e') || (mailbox[5] == 'E')) &&
	       ((mailbox[6] == 'd') || (mailbox[6] == 'D')) &&
	       (mailbox[7] == '/')) mode = (int) shared_protection;
  }
				/* if a directory */
  if (!stat (path,&sbuf) && ((sbuf.st_mode & S_IFMT) == S_IFDIR)) {
				/* set owner search if allow read or write */
    if (mode & 0600) mode |= 0100;
    if (mode & 060) mode |= 010;/* set group search if allow read or write */
    if (mode & 06) mode |= 01;	/* set world search if allow read or write */
				/* preserve directory SGID bit */
    if (sbuf.st_mode & S_ISGID) mode |= S_ISGID;
  }
  chmod (path,mode);		/* set the new protection, ignore failure */
  return LONGT;
}

/* Get proper directory protection
 * Accepts: mailbox name
 * Returns: directory mode, always
 */

long get_dir_protection (char *mailbox)
{
  if (*mailbox == '#') {	/* possible namespace? */
      if (((mailbox[1] == 'f') || (mailbox[1] == 'F')) &&
	  ((mailbox[2] == 't') || (mailbox[2] == 'T')) &&
	  ((mailbox[3] == 'p') || (mailbox[3] == 'P')) &&
	  (mailbox[4] == '/')) return ftp_dir_protection;
      else if (((mailbox[1] == 'p') || (mailbox[1] == 'P')) &&
	       ((mailbox[2] == 'u') || (mailbox[2] == 'U')) &&
	       ((mailbox[3] == 'b') || (mailbox[3] == 'B')) &&
	       ((mailbox[4] == 'l') || (mailbox[4] == 'L')) &&
	       ((mailbox[5] == 'i') || (mailbox[5] == 'I')) &&
	       ((mailbox[6] == 'c') || (mailbox[6] == 'C')) &&
	       (mailbox[7] == '/')) return public_dir_protection;
      else if (((mailbox[1] == 's') || (mailbox[1] == 'S')) &&
	       ((mailbox[2] == 'h') || (mailbox[2] == 'H')) &&
	       ((mailbox[3] == 'a') || (mailbox[3] == 'A')) &&
	       ((mailbox[4] == 'r') || (mailbox[4] == 'R')) &&
	       ((mailbox[5] == 'e') || (mailbox[5] == 'E')) &&
	       ((mailbox[6] == 'd') || (mailbox[6] == 'D')) &&
	       (mailbox[7] == '/')) return shared_dir_protection;
  }
  return dir_protection;
}

/* Determine default prototype stream to user
 * Accepts: type (NIL for create, T for append)
 * Returns: default prototype stream
 */

MAILSTREAM *default_proto (long type)
{
  myusername ();		/* make sure initialized */
				/* return default driver's prototype */
  return type ? appendProto : createProto;
}


/* Set up user flags for stream
 * Accepts: MAIL stream
 * Returns: MAIL stream with user flags set up
 */

MAILSTREAM *user_flags (MAILSTREAM *stream)
{
  int i;
  myusername ();		/* make sure initialized */
  for (i = 0; i < NUSERFLAGS && userFlags[i]; ++i)
    if (!stream->user_flags[i]) stream->user_flags[i] = cpystr (userFlags[i]);
  return stream;
}


/* Return nth user flag
 * Accepts: user flag number
 * Returns: flag
 */

char *default_user_flag (unsigned long i)
{
  myusername ();		/* make sure initialized */
  return userFlags[i];
}

/* Process rc file
 * Accepts: file name
 *	    .mminit flag
 * Don't even think of using this feature.
 */

void dorc (char *file,long flag)
{
  int i;
  char *s,*t,*k,tmp[MAILTMPLEN],tmpx[MAILTMPLEN];
  extern MAILSTREAM CREATEPROTO;
  extern MAILSTREAM EMPTYPROTO;
  DRIVER *d;
  FILE *f;
				/* no file or ill-advised usage */
  if ((f = fopen (file ? file : SYSCONFIG,"r")) &&
      (s = fgets (tmp,MAILTMPLEN,f)) && (t = strchr (s,'\n')) &&
      (flag || (!strncmp (s,RISKPHRASE,sizeof (RISKPHRASE)-1) &&
		(s = fgets (tmp,MAILTMPLEN,f)) && (t = strchr (s,'\n'))))) do {
    *t++ = '\0';		/* tie off line, find second space */
    if ((k = strchr (s,' ')) && (k = strchr (++k,' '))) {
      *k++ = '\0';		/* tie off two words */
      if (!compare_cstring (s,"set keywords") && !userFlags[0]) {
	k = strtok (k,", ");	/* yes, get first keyword */
				/* copy keyword list */
	for (i = 0; k && i < NUSERFLAGS; ++i) if (strlen (k) <= MAXUSERFLAG) {
	  if (userFlags[i]) fs_give ((void **) &userFlags[i]);
	  userFlags[i] = cpystr (k);
	  k = strtok (NIL,", ");
	}
	if (flag) break;	/* found "set keywords" in .mminit */
      }
      else if (!flag) {		/* none of these valid in .mminit */

	if (myUserName) {	/* only valid if logged in */
	  if (!compare_cstring (s,"set new-folder-format")) {
	    if (!compare_cstring (k,"same-as-inbox"))
	      createProto = ((d = mail_valid (NIL,"INBOX",NIL)) &&
			     compare_cstring (d->name,"dummy")) ?
			       ((*d->open) (NIL)) : &CREATEPROTO;
	    else if (!compare_cstring (k,"system-standard"))
	      createProto = &CREATEPROTO;
	    else {		/* see if a driver name */
	      for (d = (DRIVER *) mail_parameters (NIL,GET_DRIVERS,NIL);
		   d && compare_cstring (d->name,k); d = d->next);
	      if (d) createProto = (*d->open) (NIL);
	      else {		/* duh... */
		sprintf (tmpx,"Unknown new folder format in %s: %s",file,k);
		MM_LOG (tmpx,WARN);
	      }
	    }
	  }
	  if (!compare_cstring (s,"set empty-folder-format")) {
	    if (!compare_cstring (k,"same-as-inbox"))
	      appendProto = ((d = mail_valid (NIL,"INBOX",NIL)) &&
			     compare_cstring (d->name,"dummy")) ?
			       ((*d->open) (NIL)) : &EMPTYPROTO;
	    else if (!compare_cstring (k,"system-standard"))
	      appendProto = &EMPTYPROTO;
	    else {		/* see if a driver name */
	      for (d = (DRIVER *) mail_parameters (NIL,GET_DRIVERS,NIL);
		   d && compare_cstring (d->name,k); d = d->next);
	      if (d) appendProto = (*d->open) (NIL);
	      else {		/* duh... */
		sprintf (tmpx,"Unknown empty folder format in %s: %s",file,k);
		MM_LOG (tmpx,WARN);
	      }
	    }
	  }
	}

	if (!compare_cstring (s,"set local-host")) {
	  fs_give ((void **) &myLocalHost);
	  myLocalHost = cpystr (k);
	}
	else if (!compare_cstring (s,"set news-active-file")) {
	  fs_give ((void **) &newsActive);
	  newsActive = cpystr (k);
	}
	else if (!compare_cstring (s,"set news-spool-directory")) {
	  fs_give ((void **) &newsSpool);
	  newsSpool = cpystr (k);
	}
	else if (!compare_cstring (s,"set news-state-file")) {
	  fs_give ((void **) &myNewsrc);
	  myNewsrc = cpystr (k);
	}
	else if (!compare_cstring (s,"set ftp-export-directory")) {
	  fs_give ((void **) &ftpHome);
	  ftpHome = cpystr (k);
	}
	else if (!compare_cstring (s,"set public-home-directory")) {
	  fs_give ((void **) &publicHome);
	  publicHome = cpystr (k);
	}
	else if (!compare_cstring (s,"set shared-home-directory")) {
	  fs_give ((void **) &sharedHome);
	  sharedHome = cpystr (k);
	}
	else if (!compare_cstring (s,"set system-inbox")) {
	  fs_give ((void **) &sysInbox);
	  sysInbox = cpystr (k);
	}
	else if (!compare_cstring (s,"set mail-subdirectory")) {
	  fs_give ((void **) &mailsubdir);
	  mailsubdir = cpystr (k);
	}
	else if (!compare_cstring (s,"set from-widget"))
	  mail_parameters (NIL,SET_FROMWIDGET,
			   compare_cstring (k,"header-only") ?
			   VOIDT : NIL);

	else if (!compare_cstring (s,"set rsh-command"))
	  mail_parameters (NIL,SET_RSHCOMMAND,(void *) k);
	else if (!compare_cstring (s,"set rsh-path"))
	  mail_parameters (NIL,SET_RSHPATH,(void *) k);
	else if (!compare_cstring (s,"set ssh-command"))
	  mail_parameters (NIL,SET_SSHCOMMAND,(void *) k);
	else if (!compare_cstring (s,"set ssh-path"))
	  mail_parameters (NIL,SET_SSHPATH,(void *) k);
	else if (!compare_cstring (s,"set tcp-open-timeout"))
	  mail_parameters (NIL,SET_OPENTIMEOUT,(void *) atol (k));
	else if (!compare_cstring (s,"set tcp-read-timeout"))
	  mail_parameters (NIL,SET_READTIMEOUT,(void *) atol (k));
	else if (!compare_cstring (s,"set tcp-write-timeout"))
	  mail_parameters (NIL,SET_WRITETIMEOUT,(void *) atol (k));
	else if (!compare_cstring (s,"set rsh-timeout"))
	  mail_parameters (NIL,SET_RSHTIMEOUT,(void *) atol (k));
	else if (!compare_cstring (s,"set ssh-timeout"))
	  mail_parameters (NIL,SET_SSHTIMEOUT,(void *) atol (k));
	else if (!compare_cstring (s,"set maximum-login-trials"))
	  mail_parameters (NIL,SET_MAXLOGINTRIALS,(void *) atol (k));
	else if (!compare_cstring (s,"set lookahead"))
	  mail_parameters (NIL,SET_LOOKAHEAD,(void *) atol (k));
	else if (!compare_cstring (s,"set prefetch"))
	  mail_parameters (NIL,SET_PREFETCH,(void *) atol (k));
	else if (!compare_cstring (s,"set close-on-error"))
	  mail_parameters (NIL,SET_CLOSEONERROR,(void *) atol (k));
	else if (!compare_cstring (s,"set imap-port"))
	  mail_parameters (NIL,SET_IMAPPORT,(void *) atol (k));
	else if (!compare_cstring (s,"set pop3-port"))
	  mail_parameters (NIL,SET_POP3PORT,(void *) atol (k));
	else if (!compare_cstring (s,"set uid-lookahead"))
	  mail_parameters (NIL,SET_UIDLOOKAHEAD,(void *) atol (k));
	else if (!compare_cstring (s,"set try-ssl-first"))
	  mail_parameters (NIL,SET_TRYSSLFIRST,(void *) atol (k));

	else if (!compare_cstring (s,"set mailbox-protection"))
	  mbx_protection = atol (k);
	else if (!compare_cstring (s,"set directory-protection"))
	  dir_protection = atol (k);
	else if (!compare_cstring (s,"set lock-protection"))
	  lock_protection = atol (k);
	else if (!compare_cstring (s,"set ftp-protection"))
	  ftp_protection = atol (k);
	else if (!compare_cstring (s,"set public-protection"))
	  public_protection = atol (k);
	else if (!compare_cstring (s,"set shared-protection"))
	  shared_protection = atol (k);
	else if (!compare_cstring (s,"set ftp-directory-protection"))
	  ftp_dir_protection = atol (k);
	else if (!compare_cstring (s,"set public-directory-protection"))
	  public_dir_protection = atol (k);
	else if (!compare_cstring (s,"set shared-directory-protection"))
	  shared_dir_protection = atol (k);
	else if (!compare_cstring (s,"set dot-lock-file-timeout"))
	  locktimeout = atoi (k);
	else if (!compare_cstring (s,"set disable-fcntl-locking"))
	  disableFcntlLock = atoi (k);
	else if (!compare_cstring (s,"set disable-lock-warning"))
	  disableLockWarning = atoi (k);
	else if (!compare_cstring (s,"set hide-dot-files"))
	  hideDotFiles = atoi (k);
	else if (!compare_cstring (s,"set list-maximum-level"))
	  list_max_level = atol (k);
	else if (!compare_cstring (s,"set trust-dns"))
	  mail_parameters (NIL,SET_TRUSTDNS,(void *) atol (k));
	else if (!compare_cstring (s,"set sasl-uses-ptr-name"))
	  mail_parameters (NIL,SET_SASLUSESPTRNAME,(void *) atol (k));
	else if (!compare_cstring (s,"set network-filesystem-stat-bug"))
	  netfsstatbug = atoi (k);

	else if (!file) {	/* only allowed in system init */
	  if (!compare_cstring (s,"set black-box-directory") &&
	      !blackBoxDir) blackBoxDir = cpystr (k);
	  else if (!compare_cstring(s,"set black-box-default-home-directory")&&
		   blackBoxDir && !blackBoxDefaultHome)
	    blackBoxDefaultHome = cpystr (k);
	  else if (!compare_cstring (s,"set anonymous-home-directory") &&
		   !anonymousHome) anonymousHome = cpystr (k);
	  else if (!compare_cstring (s,"set disable-plaintext"))
	    disablePlaintext = atoi (k);
	  else if (!compare_cstring (s,"set allowed-login-attempts"))
	    logtry = atoi (k);
	  else if (!compare_cstring (s,"set chroot-server"))
	    closedBox = atoi (k);
	  else if (!compare_cstring (s,"set restrict-mailbox-access")) {
	    for (k = strtok (k,", "); k; k = strtok (NIL,", ")) {
	      if (!compare_cstring (k,"root")) restrictBox |= RESTRICTROOT;
	      else if (!compare_cstring (k,"otherusers"))
		restrictBox |= RESTRICTOTHERUSER;
	      else if (!compare_cstring (k,"all")) restrictBox = -1;
	    }
	  }
	  else if (!compare_cstring (s,"set advertise-the-world"))
	    advertisetheworld = atoi (k);
	  else if (!compare_cstring (s,"set limited-advertise"))
	    limitedadvertise = atoi (k);
	  else if (!compare_cstring
		   (s,"set disable-automatic-shared-namespaces"))
	    noautomaticsharedns = atoi (k);
	  else if (!compare_cstring (s,"set allow-user-config"))
	    allowuserconfig = atoi (k);
	  else if (!compare_cstring (s,"set allow-reverse-dns"))
	    mail_parameters (NIL,SET_ALLOWREVERSEDNS,(void *) atol (k));
	}
      }
    }
  } while ((s = fgets (tmp,MAILTMPLEN,f)) && (t = strchr (s,'\n')));
  if (f) fclose (f);		/* flush the file */
}

/* INBOX create function for tmail/dmail use only
 * Accepts: mail stream
 *	    path name buffer, preloaded with driver-dependent path
 * Returns: T on success, NIL on failure
 *
 * This routine is evil and a truly incredible kludge.  It is private for
 * tmail/dmail and is not supported for any other application.
 */

long path_create (MAILSTREAM *stream,char *path)
{
  long ret;
  short rsave = restrictBox;
  restrictBox = NIL;		/* can't restrict */
  if (blackBox) {		/* if black box */
				/* toss out driver dependent names */
    printf (path,"%s/INBOX",mymailboxdir ());
    blackBox = NIL;		/* well that's evil - evil is going on */
    ret = mail_create (stream,path);
    blackBox = T;		/* restore the box */
  }
				/* easy thing otherwise */
  else ret = mail_create (stream,path);
  restrictBox = rsave;		/* restore restrictions */
  return ret;
}

/* Default block notify routine
 * Accepts: reason for calling
 *	    data
 * Returns: data
 */

void *mm_blocknotify (int reason,void *data)
{
  void *ret = data;
  switch (reason) {
  case BLOCK_SENSITIVE:		/* entering sensitive code */
    ret = (void *) alarm (0);
    break;
  case BLOCK_NONSENSITIVE:	/* exiting sensitive code */
    if ((unsigned int) data) alarm ((unsigned int) data);
    break;
  default:			/* ignore all other reasons */
    break;
  }
  return ret;
}
