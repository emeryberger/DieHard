/*
 * Program:	Mac environment routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	26 January 1992
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


static char *myHomeDir = NIL;	/* home directory name */
static char *myLocalHost = NIL;	/* local host name */
static char *myNewsrc = NIL;	/* newsrc file name */

#include "pmatch.c"		/* include wildcard pattern matcher */

/* Environment manipulate parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *env_parameters (long function,void *value)
{
  void *ret = NIL;
  char tmp[MAILTMPLEN];
  switch ((int) function) {
  case SET_HOMEDIR:
    if (myHomeDir) fs_give ((void **) &myHomeDir);
    myHomeDir = cpystr ((char *) value);
  case GET_HOMEDIR:
				/* set home directory if not defined */
    if (!myHomeDir) myHomeDir = cpystr ("");
    ret = (void *) myHomeDir;
    break;
  case SET_LOCALHOST:
    myLocalHost = cpystr ((char *) value);
  case GET_LOCALHOST:
    ret = (void *) myLocalHost ? myLocalHost : "random-mac";
    break;
  case SET_NEWSRC:
    if (myNewsrc) fs_give ((void **) &myNewsrc);
    myNewsrc = cpystr ((char *) value);
  case GET_NEWSRC:
    if (!myNewsrc) {		/* set news file name if not defined */
      sprintf (tmp,"%s:News State",myhomedir ());
      myNewsrc = cpystr (tmp);
    }
    ret = (void *) myNewsrc;
    break;
  }
  return ret;
}

/* Write current time
 * Accepts: destination string
 *	    format of date and time
 *
 * This depends upon the ReadLocation() call in System 7 and the
 * user properly setting his location/timezone in the Map control
 * panel.
 * Nothing is done about the gmtFlags.dlsDelta byte yet, since I
 * don't know how it's supposed to work.
 */

static void do_date (char *date,char *fmt)
{
  long tz,tzm;
  time_t ti = time (0);
  struct tm *t = localtime (&ti);
  MachineLocation loc;
  ReadLocation (&loc);		/* get location/timezone poop */
				/* get sign-extended time zone */
  tz = (loc.gmtFlags.gmtDelta & 0x00ffffff) |
    ((loc.gmtFlags.gmtDelta & 0x00800000) ? 0xff000000 : 0);
  tz /= 60;			/* get timezone in minutes */
  tzm = tz % 60;		/* get minutes from the hour */
				/* output time */
  strftime (date,MAILTMPLEN,fmt,t);
				/* now output time zone */
  sprintf (date += strlen (date),"%+03ld%02ld",tz/60,tzm >= 0 ? tzm : -tzm);
}


/* Write current time in RFC 822 format
 * Accepts: destination string
 */

void rfc822_date (char *date)
{
  do_date (date,"%a, %d %b %Y %H:%M:%S ");
}


/* Write current time in internal format
 * Accepts: destination string
 */

void internal_date (char *date)
{
  do_date (date,"%2d-%b-%Y %H:%M:%S ");
}

/* Return my local host name
 * Returns: my local host name
 */

char *mylocalhost (void)
{
  return (char *) mail_parameters (NIL,GET_LOCALHOST,NIL);
}


/* Return my home directory name
 * Returns: my home directory name
 */

char *myhomedir ()
{
  return (char *) mail_parameters (NIL,GET_HOMEDIR,NIL);
}


/* Determine default prototype stream to user
 * Accepts: type (NIL for create, T for append)
 * Returns: default prototype stream
 */

MAILSTREAM *default_proto (long type)
{
  extern MAILSTREAM dummyproto;
  return &dummyproto;		/* return default driver's prototype */
}

/* Block until event satisfied
 * Called as: while (wait_condition && wait ());
 * Returns T if OK, NIL if user wants to abort
 *
 * Allows user to run a desk accessory, select a different window, or go
 * to another application while waiting for the event to finish.  COMMAND/.
 * will abort the wait.
 * Assumes the Apple menu has the apple character as its first character,
 * and that the main program has disabled all other menus.
 */

long wait ()
{
  EventRecord event;
  WindowPtr window;
  MenuInfo **m;
  long r;
  Str255 tmp;
				/* wait for an event */
  WaitNextEvent (everyEvent,&event,(long) 6,NIL);
  switch (event.what) {		/* got one -- what is it? */
  case mouseDown:		/* mouse clicked */
    switch (FindWindow (event.where,&window)) {
    case inMenuBar:		/* menu bar item? */
				/* yes, interesting event? */	
      if (r = MenuSelect (event.where)) {
				/* round-about test for Apple menu */
	  if ((*(m = GetMHandle (HiWord (r))))->menuData[1] == appleMark) {
				/* get desk accessory name */ 
	  GetItem (m,LoWord (r),tmp);
	  OpenDeskAcc (tmp);	/* fire it up */
	  SetPort (window);	/* put us back at our window */
	}
	else SysBeep (60);	/* the fool forgot to disable it! */
      }
      HiliteMenu (0);		/* unhighlight it */
      break;
    case inContent:		/* some window was selected */
      if (window != FrontWindow ()) SelectWindow (window);
      break;
    default:			/* ignore all others */
      break;
    }
    break;
  case keyDown:			/* key hit - if COMMAND/. then punt */
    if ((event.modifiers & cmdKey) && (event.message & charCodeMask) == '.')
      return NIL;
    break;
  default:			/* ignore all others */
    break;
  }
  return T;			/* try wait test again */
}

/* Return random number
 */

long random ()
{
  return (long) rand () << 16 + rand ();
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
