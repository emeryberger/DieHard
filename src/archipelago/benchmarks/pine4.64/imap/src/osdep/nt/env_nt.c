/*
 * Program:	NT environment routines
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
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

static char *myUserName = NIL;	/* user name */
static char *myLocalHost = NIL;	/* local host name */
static char *myClientAddr = NIL;/* client host address */
static char *myClientHost = NIL;/* client host name */
static char *myServerAddr = NIL;/* server host address */
static char *myServerHost = NIL;/* server host name */
static char *myHomeDir = NIL;	/* home directory name */
static char *myNewsrc = NIL;	/* newsrc file name */
static char *sysInbox = NIL;	/* system inbox name */
static long list_max_level = 5;	/* maximum level of list recursion */
static short no822tztext = NIL;	/* disable RFC [2]822 timezone text */
				/* home namespace */
static NAMESPACE nshome = {"",'\\',NIL,NIL};
				/* UNIX other user namespace */
static NAMESPACE nsother = {"#user.",'\\',NIL,NIL};
				/* namespace list */
static NAMESPACE *nslist[3] = {&nshome,&nsother,NIL};
static long alarm_countdown = 0;/* alarm count down */
static void (*alarm_rang) ();	/* alarm interrupt function */
static unsigned int rndm = 0;	/* initial `random' number */
static int server_nli = 0;	/* server and not logged in */
static int logtry = 3;		/* number of login tries */
				/* block notification */
static blocknotify_t mailblocknotify = mm_blocknotify;
				/* callback to get username */
static userprompt_t mailusername = NIL;
static long is_nt = -1;		/* T if NT, NIL if not NT, -1 unknown */
static HINSTANCE netapi = NIL;
typedef NET_API_STATUS (CALLBACK *GETINFO) (LPCWSTR,LPCWSTR,DWORD,LPBYTE *);
static GETINFO getinfo = NIL;

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
  case GET_NAMESPACE:
    ret = (void *) nslist;
    break;
  case SET_USERPROMPT :
    mailusername = (userprompt_t) value;
  case GET_USERPROMPT :
    ret = (void *) mailusername;
    break;
  case SET_HOMEDIR:
    if (myHomeDir) fs_give ((void **) &myHomeDir);
    myHomeDir = cpystr ((char *) value);
  case GET_HOMEDIR:
    ret = (void *) myHomeDir;
    break;
  case SET_LOCALHOST:
    myLocalHost = cpystr ((char *) value);
  case GET_LOCALHOST:
    if (myLocalHost) fs_give ((void **) &myLocalHost);
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
  case SET_DISABLE822TZTEXT:
    no822tztext = value ? T : NIL;
  case GET_DISABLE822TZTEXT:
    ret = (void *) (no822tztext ? VOIDT : NIL);
    break;
  case SET_BLOCKNOTIFY:
    mailblocknotify = (blocknotify_t) value;
  case GET_BLOCKNOTIFY:
    ret = (void *) mailblocknotify;
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

/* Return random number
 */

long random (void)
{
  if (!rndm) srand (rndm = (unsigned) time (0L));
  return (long) rand ();
}


/* Set alarm timer
 * Accepts: new value
 * Returns: old alarm value
 */

long alarm (long seconds)
{
  long ret = alarm_countdown;
  alarm_countdown = seconds;
  return ret;
}


/* The clock ticked
 */

void CALLBACK clock_ticked (UINT IDEvent,UINT uReserved,DWORD dwUser,
			    DWORD dwReserved1,DWORD dwReserved2)
{
  if (alarm_rang && !--alarm_countdown) (*alarm_rang) ();
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
  if (!check_nt ()) {
    if (!auth_md5.server) fatal ("Can't run on Windows without MD5 database");
    server_nli = T;		/* Windows server not logged in */
  }
				/* only do this if for init call */
  if (server && service && sslservice) {
    long port;
    struct servent *sv;
				/* set server name in syslog */
    openlog (server,LOG_PID,LOG_MAIL);
    fclose (stderr);		/* possibly save a process ID */
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
				/* make sure stdout does binary */
    setmode (fileno (stdin),O_BINARY);
    setmode (fileno (stdout),O_BINARY);
    setmode (fileno (stderr),O_BINARY);
  }
  alarm_rang = clkint;		/* note the clock interrupt */
  timeBeginPeriod (1000);	/* set the timer interval */
  timeSetEvent (1000,1000,clock_ticked,NIL,TIME_PERIODIC);
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

/* Server log in
 * Accepts: user name string
 *	    password string
 *	    authenticating user name string
 *	    argument count
 *	    argument vector
 * Returns: T if password validated, NIL otherwise
 */

static int gotprivs = NIL;	/* once-only flag to grab privileges */

long server_login (char *user,char *pass,char *authuser,int argc,char *argv[])
{
  HANDLE hdl;
  LUID tcbpriv;
  TOKEN_PRIVILEGES tkp;
  char *s;
				/* need to get privileges? */
  if (!gotprivs++ && check_nt ()) {
				/* yes, note client host if specified */
    if (argc == 2) myClientHost = argv[1];
				/* get process token and TCB priv value */
    if (!(OpenProcessToken (GetCurrentProcess (),
			    TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,&hdl) &&
	  LookupPrivilegeValue ((LPSTR) NIL,SE_TCB_NAME,&tcbpriv)))
      return NIL;
    tkp.PrivilegeCount = 1;	/* want to enable this privilege */
    tkp.Privileges[0].Luid = tcbpriv;
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
				/* enable it */
    AdjustTokenPrivileges (hdl,NIL,&tkp,sizeof (TOKEN_PRIVILEGES),
			   (PTOKEN_PRIVILEGES) NIL,(PDWORD) NIL);
				/* make sure it won */
    if (GetLastError() != ERROR_SUCCESS) return NIL;
  }

				/* cretins still haven't given up */
  if ((strlen (user) >= MAILTMPLEN) ||
      (authuser && (strlen (authuser) >= MAILTMPLEN)))
    syslog (LOG_ALERT,"SYSTEM BREAK-IN ATTEMPT, host=%.80s",tcp_clienthost ());
  else if (logtry > 0) {	/* still have available logins? */
				/* authentication user not supported */
    if (authuser && *authuser && compare_cstring (authuser,user))
      mm_log ("Authentication id must match authorization id",ERROR);
    if (check_nt ()) {		/* NT: authserver_login() call not supported */
      if (!pass) mm_log ("Unsupported authentication mechanism",ERROR);
      else if ((		/* try to login and impersonate the guy */
#ifdef LOGIN32_LOGON_NETWORK
		LogonUser (user,".",pass,LOGON32_LOGON_NETWORK,
			   LOGON32_PROVIDER_DEFAULT,&hdl) ||
#endif
		LogonUser (user,".",pass,LOGON32_LOGON_INTERACTIVE,
			   LOGON32_PROVIDER_DEFAULT,&hdl) ||
		LogonUser (user,".",pass,LOGON32_LOGON_BATCH,
			   LOGON32_PROVIDER_DEFAULT,&hdl) ||
		LogonUser (user,".",pass,LOGON32_LOGON_SERVICE,
			   LOGON32_PROVIDER_DEFAULT,&hdl)) &&
	       ImpersonateLoggedOnUser (hdl)) return env_init (user,NIL);
    }
    else {			/* Win9x: done if from authserver_login() */
      if (!pass) server_nli = NIL;
				/* otherwise check MD5 database */
      else if (s = auth_md5_pwd (user)) {
				/* change NLI state based on pwd match */
	server_nli = strcmp (s,pass);
	memset (s,0,strlen (s));/* erase sensitive information */
	fs_give ((void **) &s);	/* flush erased password */
      }
				/* success if no longer NLI */
      if (!server_nli) return env_init (user,NIL);
    }
  }
  s = (logtry-- > 0) ? "Login failure" : "Excessive login attempts";
				/* note the failure in the syslog */
  syslog (LOG_INFO,"%s user=%.80s host=%.80s",s,user,tcp_clienthost ());
  sleep (3);			/* slow down possible cracker */
  return NIL;
}

/* Authenticated server log in
 * Accepts: user name string
 *	    authentication user name string
 *	    argument count
 *	    argument vector
 * Returns: T if password validated, NIL otherwise
 */

long authserver_login (char *user,char *authuser,int argc,char *argv[])
{
  return server_login (user,NIL,authuser,argc,argv);
}


/* Log in as anonymous daemon
 * Accepts: argument count
 *	    argument vector
 * Returns: T if successful, NIL if error
 */

long anonymous_login (int argc,char *argv[])
{
  return server_login ("Guest",NIL,NIL,argc,argv);
}


/* Initialize environment
 * Accepts: user name
 *          home directory, or NIL to use default
 * Returns: T, always
 */

long env_init (char *user,char *home)
{
  if (myUserName) fatal ("env_init called twice!");
  myUserName = cpystr (user);	/* remember user name */
  if (!myHomeDir)		/* only if home directory not set up yet */
    myHomeDir = (home && *home) ? cpystr (home) : win_homedir (user);
  return T;
}

/* Check if NT
 * Returns: T if NT, NIL if Win9x
 */

int check_nt (void)
{
  if (is_nt < 0) {		/* not yet set up? */
    OSVERSIONINFO ver;
    ver.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
    GetVersionEx (&ver);
    is_nt = (ver.dwPlatformId == VER_PLATFORM_WIN32_NT) ? T : NIL;
  }
  return is_nt;
}


/* Return Windows home directory
 * Accepts: user name
 * Returns: home directory
 */

char *win_homedir (char *user)
{
  char *s,*t,tmp[MAILTMPLEN];
  PUSER_INFO_1 ui;
				/* Win9x default */
  if (!check_nt ()) sprintf (tmp,"%s\\My Documents",defaultDrive ());
				/* get from user info on NT */
  else if ((netapi || (netapi = LoadLibrary ("netapi32.dll"))) &&
	   (getinfo ||
	    (getinfo = (GETINFO) GetProcAddress (netapi,"NetUserGetInfo"))) &&
	   MultiByteToWideChar (CP_ACP,0,user,strlen (user) + 1,
				(WCHAR *) tmp,MAILTMPLEN) &&
	   !(*getinfo) (NIL,(LPWSTR) &tmp,1,(LPBYTE *) &ui) &&
	   WideCharToMultiByte (CP_ACP,0,ui->usri1_home_dir,-1,
				tmp,MAILTMPLEN,NIL,NIL) && tmp[0]) {
				/* make sure doesn't end with delimiter */
    if ((*(s = tmp + strlen (tmp) - 1) == '\\') || (*s == '/')) *s = '\0';
  }
				/* no home dir, found Win2K user profile? */
  else if ((s = getenv ("USERPROFILE")) && (t = strrchr (s,'\\'))) {      
    strncpy (tmp,s,t-s);	/* copy up to user name */
    sprintf (tmp+(t-s),"\\%.100s\\My Documents",user);
  }
				/* last resort NT default */
  else sprintf (tmp,"%s\\users\\default",defaultDrive ());
  return cpystr (tmp);
}


/* Return default drive
 * Returns: default drive
 */

static char *defaultDrive (void)
{
  char *s = getenv ("SystemDrive");
  return (s && *s) ? s : "C:";
}

/* Return my user name
 * Accepts: pointer to optional flags
 * Returns: my user name
 */

char *myusername_full (unsigned long *flags)
{
  UCHAR usr[MAILTMPLEN];
  DWORD len = MAILTMPLEN;
  char *user,*path,*d,*p,pth[MAILTMPLEN];
  char *ret = "SYSTEM";
				/* get user name if don't have it yet */
  if (!myUserName && !server_nli &&
				/* use callback, else logon name */
      ((mailusername && (user = (char *) (*mailusername) ())) ||
       (GetUserName (usr,&len) && _stricmp (user = (char *) usr,"SYSTEM")))) {
				/* try HOMEPATH, then HOME */
    if (p = getenv ("HOMEPATH"))
      sprintf (path = pth,"%s%s",
	       (d = getenv ("HOMEDRIVE")) ? d : defaultDrive (),p);
    else if (!(path = getenv ("HOME")))
      sprintf (path = pth,"%s\\My Documents",defaultDrive ());
				/* make sure doesn't end with delimiter */
    if ((*(p = path + strlen (path) -1) == '\\') || (*p == '/')) *p = '\0';
    env_init (user,path);	/* initialize environment */
  }
  if (myUserName) {		/* logged in? */
    if (flags)			/* Guest is an anonymous user */
      *flags = _stricmp (myUserName,"Guest") ? MU_LOGGEDIN : MU_ANONYMOUS;
    ret = myUserName;		/* return user name */
  }
  else if (flags) *flags = MU_NOTLOGGEDIN;
  return ret;
}

/* Return my home directory name
 * Returns: my home directory name
 */

char *myhomedir ()
{
  if (!myHomeDir) myusername ();/* initialize if first time */
  return myHomeDir ? myHomeDir : "";
}


/* Return system standard INBOX
 * Accepts: buffer string
 */

char *sysinbox ()
{
  char tmp[MAILTMPLEN];
  if (!sysInbox) {		/* initialize if first time */
    if (check_nt ()) sprintf (tmp,MAILFILE,myUserName);
    else sprintf (tmp,"%s\\INBOX",myhomedir ());
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
  else strcpy (dst,myhomedir());/* no arguments, wants home directory */
  return dst;			/* return the name */
}

/* Return mailbox file name
 * Accepts: destination buffer
 *	    mailbox name
 * Returns: file name or empty string for driver-selected INBOX or NIL if error
 */

char *mailboxfile (char *dst,char *name)
{
  char *dir = myhomedir ();
  *dst = '\0';			/* default to empty string */
				/* check for INBOX */
  if (!compare_cstring (name,"INBOX")) return dst;
				/* reject names with / */
  if (strchr (name,'/')) return NIL;
  else if (*name == '#') {	/* namespace names */
    if (((name[1] == 'u') || (name[1] == 'U')) &&
	((name[2] == 's') || (name[2] == 'S')) &&
	((name[3] == 'e') || (name[3] == 'E')) &&
	((name[4] == 'r') || (name[4] == 'R')) && (name[5] == '.')) {
				/* copy user name */
      for (dir = dst,name += 6; *name && (*name != '\\'); *dir++ = *name++);
      *dir++ = '\0';		/* tie off user name */
      if (!(dir = win_homedir (dst))) return NIL;
				/* build resulting name */
      sprintf (dst,"%s\\%s",dir,name);
      fs_give ((void **) &dir);	/* clean up other user's home dir */
      return dst;
    }
    else return NIL;		/* unknown namespace name */
  }
				/* absolute path name? */
  else if ((*name == '\\') || (name[1] == ':')) return strcpy (dst,name);
				/* build resulting name */
  sprintf (dst,"%s\\%s",dir,name);
  return dst;			/* return it */
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
				/* Win2K and Win98 have TEMP under windir */
  if (!((s = lockdir (lock,getenv ("windir"),"TEMP")) ||
				/* NT4, NT3.x and Win95 use one of these */
	(s = lockdir (lock,getenv ("TEMP"),NIL)) ||
	(s = lockdir (lock,getenv ("TMP"),NIL)) ||
	(s = lockdir (lock,getenv ("TMPDIR"),NIL)) ||
				/* try one of these */
	(s = lockdir (lock,defaultDrive (),"WINNT\\TEMP")) ||
	(s = lockdir (lock,defaultDrive (),"WINDOWS\\TEMP")) ||
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
    for (s = lock; c = *first++; *s++ = (c == '/') ? '\\' : c);
    if (last && *last) {	/* copy last part if specified */
				/* write trailing \ in case not in first */
      if (s[-1] != '\\') *s++ = '\\';
      while (c = *last++) *s++ = (c == '/') ? '\\' : c;
    }
    if (s[-1] == '\\') --s;	/* delete trailing \ if any */
    *s = s[1] = '\0';		/* tie off name at this point */
    if (!stat (lock,&sbuf)) {	/* does the name exist? */
      *s++ = '\\';		/* yes, reinstall trailing \ */
      return s;			/* return the name */
    }
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
  extern MAILSTREAM CREATEPROTO,APPENDPROTO;
  return type ? &APPENDPROTO : &CREATEPROTO;
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
