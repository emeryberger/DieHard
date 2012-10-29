/*
 * Program:	Mail Delivery Module
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	5 April 1993
 * Last Edited:	14 July 2002
 *
 * The IMAP tools software provided in this Distribution is
 * Copyright 2002 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <stdio.h>
#include <pwd.h>
#include <errno.h>
extern int errno;		/* just in case */
#include <sysexits.h>
#include <sys/file.h>
#include <sys/stat.h>
#include "mail.h"
#include "osdep.h"
#include "misc.h"
#include "linkage.h"


/* Globals */

char *version = "2002(14)";	/* tmail release version */
int debug = NIL;		/* debugging (don't fork) */
int trycreate = NIL;		/* flag saying gotta create before appending */
int critical = NIL;		/* flag saying in critical code */
char *sender = NIL;		/* message origin */
char *inbox = NIL;		/* inbox file */


/* Function prototypes */

void file_string_init (STRING *s,void *data,unsigned long size);
char file_string_next (STRING *s);
void file_string_setpos (STRING *s,unsigned long i);
int main (int argc,char *argv[]);
int deliver (FILE *f,unsigned long msglen,char *user);
long ibxpath (MAILSTREAM *ds,char **mailbox,char *path);
int deliver_safely (MAILSTREAM *prt,STRING *st,char *mailbox,char *path,
		    uid_t uid,char *tmp);
int delivery_unsafe (char *path,uid_t uid,struct stat *sbuf,char *tmp);
int fail (char *string,int code);
char *getusername (char *s,char **t);


/* File string driver for file stringstructs */

STRINGDRIVER file_string = {
  file_string_init,		/* initialize string structure */
  file_string_next,		/* get next byte in string structure */
  file_string_setpos		/* set position in string structure */
};


/* Cache buffer for file stringstructs */

#define CHUNKLEN 16384
char chunk[CHUNKLEN];

/* Initialize file string structure for file stringstruct
 * Accepts: string structure
 *	    pointer to string
 *	    size of string
 */

void file_string_init (STRING *s,void *data,unsigned long size)
{
  s->data = data;		/* note fd */
  s->size = size;		/* note size */
  s->chunk = chunk;
  s->chunksize = (unsigned long) CHUNKLEN;
  SETPOS (s,0);			/* set initial position */
}


/* Get next character from file stringstruct
 * Accepts: string structure
 * Returns: character, string structure chunk refreshed
 */

char file_string_next (STRING *s)
{
  char c = *s->curpos++;	/* get next byte */
  SETPOS (s,GETPOS (s));	/* move to next chunk */
  return c;			/* return the byte */
}


/* Set string pointer position for file stringstruct
 * Accepts: string structure
 *	    new position
 */

void file_string_setpos (STRING *s,unsigned long i)
{
  if (i > s->size) i = s->size;	/* don't permit setting beyond EOF */
  s->offset = i;		/* set new offset */
  s->curpos = s->chunk;		/* reset position */
				/* set size of data */
  if (s->cursize = min (s->chunksize,SIZE (s))) {
				/* move to that position in the file */
    fseek ((FILE *) s->data,s->offset,SEEK_SET);
    fread (s->curpos,sizeof (char),(unsigned int) s->cursize,(FILE *) s->data);
  }
}

/* Main program */

int main (int argc,char *argv[])
{
  FILE *f = NIL;
  int pid,c,ret = 0;
  unsigned long msglen,status = 0;
  char *s,tmp[MAILTMPLEN];
  uid_t ruid = getuid ();
  struct passwd *pwd;
  openlog ("tmail",LOG_PID,LOG_MAIL);
#include "linkage.c"
				/* make sure have some arguments */
  if (--argc < 1) _exit (fail ("usage: tmail [-D] user[+folder]",EX_USAGE));
				/* process all flags */
  while (argc && (*(s = *++argv)) == '-') {
    argc--;			/* gobble this argument */
    switch (s[1]) {		/* what is this flag? */
    case 'D':			/* debug */
      debug = T;		/* don't fork */
      break;
    case 'd':			/* obsolete flag meaning multiple users */
      break;
    case 'I':			/* inbox specifier */
      if (argc--) inbox = cpystr (*++argv);
      else _exit (fail ("missing argument to -I",EX_USAGE));
      break;
    case 'f':			/* new name for this flag */
    case 'r':			/* flag giving return path */
      if (argc--) sender = cpystr (*++argv);
      else _exit (fail ("missing argument to -r",EX_USAGE));
      break;
    default:			/* anything else */
      _exit (fail ("unknown switch",EX_USAGE));
    }
  }

  if (!argc) ret = fail ("no recipients",EX_USAGE);
  else if (!(f = tmpfile ())) ret = fail ("can't make temp file",EX_TEMPFAIL);
  else {			/* build delivery headers */
    if (sender) fprintf (f,"Return-Path: <%s>\015\012",sender);
				/* start Received line: */
    fprintf (f,"Received: via tmail-%s",version);
				/* not root or daemon? */
    if (ruid && !((pwd = getpwnam ("daemon")) && (ruid == pwd->pw_uid))) {
      pwd = getpwuid (ruid);	/* get unprivileged user's information */
      if (inbox) {
	if (pwd) sprintf (tmp,"user %.80s",pwd->pw_name);
	else sprintf (tmp,"UID %ld",(long) ruid);
	strcat (tmp," is not privileged to use -I");
	_exit (fail (tmp,EX_USAGE));
      }
      fputs (" (invoked by ",f);
      if (pwd) fprintf (f,"user %s",pwd->pw_name);
      else fprintf (f,"UID %ld",(long) ruid);
      fputs (")",f);
    }
				/* write "for" if single recipient */
    if (argc == 1) fprintf (f," for %s",*argv);
    fputs ("; ",f);
    rfc822_date (tmp);
    fputs (tmp,f);
    fputs ("\015\012",f);
				/* copy text from standard input */
    if (!fgets (tmp,MAILTMPLEN-1,stdin) || !(s = strchr (tmp,'\n')) ||
	(s == tmp) || s[1]) _exit (fail ("bad first message line",EX_USAGE));
    if (s[-1] == '\015') {	/* nuke leading "From " line */
      if ((tmp[0] != 'F') || (tmp[1] != 'r') || (tmp[2] != 'o') ||
	  (tmp[3] != 'm') || (tmp[4] != ' ')) fputs (tmp,f);
      while ((c = getchar ()) != EOF) putc (c,f);
    }
    else {
      mm_log ("tmail called with LF-only newlines",WARN);
      if ((tmp[0] != 'F') || (tmp[1] != 'r') || (tmp[2] != 'o') ||
	  (tmp[3] != 'm') || (tmp[4] != ' ')) {
	*s++ = '\015';		/* overwrite NL with CRLF */
	*s++ = '\012';
	*s = '\0';		/* tie off string */
	fputs (tmp,f);		/* write line */
      }
				/* copy text from standard input */
      while ((c = getchar ()) != EOF) {
				/* add CR if needed */
	if (c == '\012') putc ('\015',f);
	putc (c,f);
      }
    }
    msglen = ftell (f);		/* size of message */
    fflush (f);			/* make sure all changes written out */

    if (ferror (f)) ret = fail ("error writing temp file",EX_TEMPFAIL);
    else if (!msglen) ret = fail ("empty message",EX_TEMPFAIL);
				/* single delivery */
    else if (argc == 1) ret = deliver (f,msglen,*argv);
    else do {			/* multiple delivery uses daughter forks */
      if ((pid = fork ()) < 0) ret = fail (strerror (errno),EX_OSERR);
      else if (pid) {		/* mother process */
	grim_pid_reap_status (pid,NIL,(void *) status);
				/* normal termination? */
	if (!ret) ret = (status & 0xff) ? EX_SOFTWARE : (status & 0xff00) >> 8;
      }
				/* daughter process */
      else _exit (deliver (f,msglen,*argv));
    } while (--argc && *argv++);
    mm_dlog (ret ? "error in delivery" : "all recipients delivered");
  }
  if (f) fclose (f);		/* all done with temporary file */
  _exit (ret);			/* normal exit */
  return 0;			/* stupid gcc */
}

/* Deliver message to recipient list
 * Accepts: file description of message temporary file
 *	    size of message temporary file in bytes
 *	    recipient name
 * Returns: NIL if success, else error code
 */

int deliver (FILE *f,unsigned long msglen,char *user)
{
  MAILSTREAM *ds = NIL;
  DRIVER *dv = NIL;
  char *s,*t,*mailbox,tmp[MAILTMPLEN],path[MAILTMPLEN];
  struct passwd *pwd;
  STRING st;
  struct stat sbuf;
  uid_t duid;
  uid_t euid = geteuid ();
				/* get user record */
  if (!(pwd = getpwnam (getusername (user,&mailbox)))) {
    sprintf (tmp,"no such user as %.80s",user);
    return fail (tmp,EX_NOUSER);
  }
				/* absurd is absurd */
  if (mailbox && (strlen (mailbox) > 256))
    return fail ("absurd folder name",EX_NOUSER);
				/* big security hole if this is allowed */
  if (!(duid = pwd->pw_uid)) return fail ("mail to root prohibited",EX_NOUSER);
  if (duid != euid) {		/* avoid obnoxious initgroups() msg if self */
    setgid (pwd->pw_gid);	/* initialize groups */
    initgroups (user,pwd->pw_gid);
    if (setuid (duid)) {	/* log in as that user */
      sprintf (tmp,"unable to log in UID %ld from UID %ld",
	       (long) duid,(long) euid);
      return fail (tmp,EX_NOUSER);
    }
  }
				/* can't use pwd after this point */
  env_init (pwd->pw_name,pwd->pw_dir);
  sprintf (tmp,"delivering to %.80s+%.80s",user,mailbox ? mailbox : "INBOX");
  mm_dlog (tmp);
				/* prepare stringstruct */
  INIT (&st,file_string,(void *) f,msglen);
  if (mailbox) {		/* non-INBOX name */
    switch (mailbox[0]) {	/* make sure a valid name */
    default:			/* other names, try to deliver if not INBOX */
      if (!strstr (mailbox,"..") && !strstr (mailbox,"//") &&
	  !strstr (mailbox,"/~") && mailboxfile (path,mailbox) && path[0] &&
	  !deliver_safely (NIL,&st,mailbox,path,duid,tmp)) return NIL;
    case '%': case '*':		/* wildcards not valid */
    case '#':			/* namespace name not valid */
    case '/':			/* absolute path names not valid */
    case '~':			/* user names not valid */
      sprintf (tmp,"invalid mailbox name %.80s+%.80s",user,mailbox);
      mm_log (tmp,WARN);
      break;
    }
    mm_dlog ("retrying delivery to INBOX");
    SETPOS (&st,0);		/* rewind stringstruct just in case */
  }

				/* -I specified and not "-I INBOX"? */
  if (inbox && !(((inbox[0] == 'I') || (inbox[0] == 'i')) &&
		 ((inbox[1] == 'N') || (inbox[1] == 'n')) &&
		 ((inbox[2] == 'B') || (inbox[2] == 'b')) &&
		 ((inbox[3] == 'O') || (inbox[3] == 'o')) &&
		 ((inbox[4] == 'X') || (inbox[4] == 'x')) && !inbox[5])) {
				/* "-I #driver.xxx/name"? */
    if ((*inbox == '#') && ((inbox[1] == 'd') || (inbox[1] == 'D')) &&
	((inbox[2] == 'r') || (inbox[2] == 'R')) &&
	((inbox[3] == 'i') || (inbox[3] == 'I')) &&
	((inbox[4] == 'v') || (inbox[4] == 'V')) &&
	((inbox[5] == 'e') || (inbox[5] == 'E')) &&
	((inbox[6] == 'r') || (inbox[6] == 'R')) && (inbox[7] == '.') &&
	(s = strchr (inbox+8,'/'))) {
      *s = '\0';		/* temporarily tie off driver name */
      if (!((dv = mail_parameters (NIL,GET_DRIVER,(void *) (inbox+8))) &&
	    (mailboxfile (path,s[1] ? s + 1 : "&&&&&") == path) &&
	    (s[1] || ((t = strstr (path,"&&&&&")) && strcpy (t,"INBOX"))))) {
	path[0] = '\0';		/* bad -I argument, no path resolved */
	sprintf (tmp,"Unable to resolve driver in %.80s, -I ignored",inbox);
	mm_log (tmp,WARN);
      }
      *s = '/';			/* restore delimiter */
    }
				/* resolve "-I other" specification */
    else if (mailboxfile (path,inbox) && path[0]) {
				/* resolution succeeded, impute driver */
      if (!strcmp (inbox,"mail.txt"))
	dv = mail_parameters (NIL,GET_DRIVER,(void *) "tenex");
      else if (!strcmp (inbox,"INBOX.MTX"))
	dv = mail_parameters (NIL,GET_DRIVER,(void *) "mtx");
      else if (!strcmp (inbox,"mbox"))
	dv = mail_parameters (NIL,GET_DRIVER,(void *) "unix");
    }
    else {			/* bad -I argument */
      path[0] = '\0';		/* no path resolved */
      sprintf (tmp,"Unable to resolve %.80s, -I ignored",inbox);
      mm_log (tmp,WARN);
    }
    if (*path) {		/* -I successfully resolved a path? */
      MAILSTREAM dpr;
      dpr.dtb = dv;
      if (dv) ds = &dpr;
				/* supplicate to the Evil One if necessary */
      if (lstat (path,&sbuf) && !path_create (ds,path)) {
				/* the Evil One rejected the plea */
	sprintf (tmp,"Unable to create %.80s, -I ignored",path);
	mm_log (tmp,WARN);
      }
				/* now attempt delivery */
      else return deliver_safely (ds,&st,inbox,path,duid,tmp);
    }
  }

				/* no -I, resolve "INBOX" into path */
  if (mailboxfile (path,mailbox = "INBOX") && !path[0]) {
				/* clear box, get generic INBOX prototype */
    if (!(ds = mail_open (NIL,"INBOX",OP_PROTOTYPE)))
      fatal ("no INBOX prototype");
				/* standard system driver? */
    if (!strcmp (ds->dtb->name,"unix") || !strcmp (ds->dtb->name,"mmdf")) {
      strcpy (path,sysinbox ());/* use system INBOX */
      if (!lstat (path,&sbuf))	/* deliver to existing system INBOX */
	return deliver_safely (ds,&st,mailbox,path,duid,tmp);
    }
    else {			/* other driver, try ~/INBOX */
      if ((mailboxfile (path,"&&&&&") == path) &&
	  (s = strstr (path,"&&&&&")) && strcpy (s,"INBOX") &&
	  !lstat (path,&sbuf)){	/* deliver to existing ~/INBOX */
	sprintf (tmp,"#driver.%s/INBOX",ds->dtb->name);
	return deliver_safely (ds,&st,cpystr (tmp),path,duid,tmp);
      }
    }
				/* not dummy, deliver to driver imputed path */
    if (strcmp (ds->dtb->name,"dummy"))
      return (ibxpath (ds,&mailbox,path) && !lstat (path,&sbuf)) ?
	deliver_safely (ds,&st,mailbox,path,duid,tmp) :
	  fail ("unable to resolve INBOX path",EX_CANTCREAT);
				/* dummy, empty imputed append path exist? */
    if (ibxpath (ds = default_proto (T),&mailbox,path) &&
	!lstat (path,&sbuf) && !sbuf.st_size)
      return deliver_safely (ds,&st,mailbox,path,duid,tmp);
				/* impute path that we will create */
    if (!ibxpath (ds = default_proto (NIL),&mailbox,path))
      return fail ("unable to resolve INBOX",EX_CANTCREAT);
  }
				/* black box, must create, get create proto */
  else if (lstat (path,&sbuf)) ds = default_proto (NIL);
  else {			/* black box, existing file */
				/* empty file, get append prototype */
    if (!sbuf.st_size) ds = default_proto (T);
				/* non-empty, get prototype from its data */
    else if (!(ds = mail_open (NIL,"INBOX",OP_PROTOTYPE)))
      fatal ("no INBOX prototype");
				/* error if unknown format */
    if (!strcmp (ds->dtb->name,"phile"))
      return fail ("unknown format INBOX",EX_UNAVAILABLE);
				/* otherwise can deliver to it */
    return deliver_safely (ds,&st,mailbox,path,duid,tmp);
  }
  sprintf (tmp,"attempting to create mailbox %.80s path %.80s",mailbox,path);
  mm_dlog (tmp);
				/* supplicate to the Evil One */
  if (!path_create (ds,path)) return fail ("can't create INBOX",EX_CANTCREAT);
  sprintf (tmp,"created %.80s",path);
  mm_dlog (tmp);
				/* deliver the message */
  return deliver_safely (ds,&st,mailbox,path,duid,tmp);
}

/* Resolve INBOX from driver prototype into mailbox name and filesystem path
 * Accepts: driver prototype
 * 	    pointer to mailbox name string pointer
 *	    buffer to return mailbox path
 * Returns: T if success, NIL if error
 */

long ibxpath (MAILSTREAM *ds,char **mailbox,char *path)
{
  char *s,tmp[MAILTMPLEN];
  long ret = T;
  if (!strcmp (ds->dtb->name,"unix") || !strcmp (ds->dtb->name,"mmdf"))
    strcpy (path,sysinbox ());	/* use system INBOX for unix and MMDF */
  else if (!strcmp (ds->dtb->name,"tenex"))
    ret = (mailboxfile (path,"mail.txt") == path) ? T : NIL;
  else if (!strcmp (ds->dtb->name,"mtx"))
    ret = (mailboxfile (path,"INBOX.MTX") == path) ? T : NIL;
  else if (!strcmp (ds->dtb->name,"mbox"))
    ret = (mailboxfile (path,"mbox") == path) ? T : NIL;
				/* better not be a namespace driver */
  else if (ds->dtb->flags & DR_NAMESPACE) return NIL;
				/* INBOX in home directory */
  else ret = ((mailboxfile (path,"&&&&&") == path) &&
	      (s = strstr (path,"&&&&&")) && strcpy (s,"INBOX")) ? T : NIL;
  if (ret) {			/* don't bother if lossage */
    sprintf (tmp,"#driver.%s/INBOX",ds->dtb->name);
    *mailbox = cpystr (tmp);	/* name of INBOX in this namespace */
  }
  return ret;
}

/* Deliver safely
 * Accepts: prototype stream to force mailbox format
 *	    stringstruct of message temporary file or NIL for check only
 *	    mailbox name
 *	    filesystem path name
 *	    user id
 *	    scratch buffer for messages
 * Returns: NIL if success, else error code
 */

int deliver_safely (MAILSTREAM *prt,STRING *st,char *mailbox,char *path,
		    uid_t uid,char *tmp)
{
  struct stat sbuf;
  int i = delivery_unsafe (path,uid,&sbuf,tmp);
  if (i) return i;		/* give up now if delivery unsafe */
				/* directory, not file */
  if ((sbuf.st_mode & S_IFMT) == S_IFDIR) {
    if (sbuf.st_mode & 0001) {	/* listable directories may be worrisome */
      sprintf (tmp,"WARNING: directory %.80s is listable",path);
      mm_log (tmp,WARN);
    }
  }
  else {			/* file, not directory */
    if (sbuf.st_nlink != 1) {	/* multiple links may be worrisome */
      sprintf (tmp,"WARNING: multiple links to file %.80s",path);
      mm_log (tmp,WARN);
    }
    if (sbuf.st_mode & 0111) {	/* executable files may be worrisome */
      sprintf (tmp,"WARNING: file %.80s is executable",path);
      mm_log (tmp,WARN);
    }
  }
  if (sbuf.st_mode & 0002) {	/* public-write files may be worrisome */
    sprintf (tmp,"WARNING: file %.80s is publicly-writable",path);
    mm_log (tmp,WARN);
  }
  if (sbuf.st_mode & 0004) {	/* public-write files may be worrisome */
    sprintf (tmp,"WARNING: file %.80s is publicly-readable",path);
    mm_log (tmp,WARN);
  }
				/* so far, so good */
  sprintf (tmp,"%s appending to %.80s (%s %.80s)",
	   prt ? prt->dtb->name : "default",mailbox,
	   ((sbuf.st_mode & S_IFMT) == S_IFDIR) ? "directory" : "file",path);
  mm_dlog (tmp);
				/* do the append now! */
  if (!mail_append (prt,mailbox,st)) {
    sprintf (tmp,"message delivery failed to %.80s",path);
    return fail (tmp,EX_CANTCREAT);
  }
				/* note success */
  sprintf (tmp,"delivered to %.80s",path);
  mm_log (tmp,NIL);
				/* make sure nothing evil this way comes */
  return delivery_unsafe (path,uid,&sbuf,tmp);
}

/* Verify that delivery is safe
 * Accepts: path name
 *	    user id
 *	    stat buffer
 *	    scratch buffer for messages
 * Returns: NIL if delivery is safe, error code if unsafe
 */

int delivery_unsafe (char *path,uid_t uid,struct stat *sbuf,char *tmp)
{
  u_short type;
  sprintf (tmp,"Verifying safe delivery to %.80s by UID %ld",path,(long) uid);
  mm_dlog (tmp);
				/* prepare message just in case */
  sprintf (tmp,"delivery to %.80s unsafe: ",path);
				/* unsafe if can't get its status */
  if (lstat (path,sbuf)) strcat (tmp,strerror (errno));
  else if (sbuf->st_uid != uid)	/* unsafe if UID does not match */
    sprintf (tmp + strlen (tmp),"uid mismatch (%ld != %ld)",
	     (long) sbuf->st_uid,(long) uid);
				/* unsafe if not a regular file */
  else if (((type = sbuf->st_mode & (S_IFMT | S_ISUID | S_ISGID)) != S_IFREG)&&
	   (type != S_IFDIR)) {
    strcat (tmp,"can't deliver to ");
				/* unsafe if setuid */
    if (type & S_ISUID) strcat (tmp,"setuid file");
				/* unsafe if setgid */
    else if (type & S_ISGID) strcat (tmp,"setgid file");
    else switch (type) {
    case S_IFCHR: strcat (tmp,"character special"); break;
    case S_IFBLK: strcat (tmp,"block special"); break;
    case S_IFLNK: strcat (tmp,"symbolic link"); break;
    case S_IFSOCK: strcat (tmp,"socket"); break;
    default:
      sprintf (tmp + strlen (tmp),"file type %07o",(unsigned int) type);
    }
  }
  else return NIL;		/* OK to deliver */
  return fail (tmp,EX_CANTCREAT);
}

/* Report an error
 * Accepts: string to output
 */

int fail (char *string,int code)
{
  mm_log (string,ERROR);	/* pass up the string */
#if T
  switch (code) {
  case EX_USAGE:
  case EX_OSERR:
  case EX_SOFTWARE:
  case EX_NOUSER:
  case EX_CANTCREAT:
  case EX_UNAVAILABLE:
    code = EX_TEMPFAIL;		/* coerce these to TEMPFAIL */
  default:
    break;
  }
#endif
  return code;			/* error code to return */
}


/* Get user name from username+mailbox specifier
 * Accepts: username/mailbox specifier
 *	    pointer to return location for mailbox specifier
 * Returns: user name, mailbox specifier value NIL if INBOX, patches out +
 */

char *getusername (char *s,char **t)
{
  char tmp[MAILTMPLEN];
  if (*t = strchr (s,'+')) {	/* have a mailbox specifier? */
    *(*t)++ = '\0';		/* yes, tie off user name */
				/* user+ and user+INBOX same as user */
    if (!**t || !strcmp ("INBOX",ucase (strcpy (tmp,*t)))) *t = NIL;
  }
  return s;			/* return user name */
}

/* Co-routines from MAIL library */


/* Message matches a search
 * Accepts: MAIL stream
 *	    message number
 */

void mm_searched (MAILSTREAM *stream,unsigned long msgno)
{
  fatal ("mm_searched() call");
}


/* Message exists (i.e. there are that many messages in the mailbox)
 * Accepts: MAIL stream
 *	    message number
 */

void mm_exists (MAILSTREAM *stream,unsigned long number)
{
  fatal ("mm_exists() call");
}


/* Message expunged
 * Accepts: MAIL stream
 *	    message number
 */

void mm_expunged (MAILSTREAM *stream,unsigned long number)
{
  fatal ("mm_expunged() call");
}


/* Message flags update seen
 * Accepts: MAIL stream
 *	    message number
 */

void mm_flags (MAILSTREAM *stream,unsigned long number)
{
}

/* Mailbox found
 * Accepts: MAIL stream
 *	    delimiter
 *	    mailbox name
 *	    mailbox attributes
 */

void mm_list (MAILSTREAM *stream,int delimiter,char *name,long attributes)
{
  fatal ("mm_list() call");
}


/* Subscribed mailbox found
 * Accepts: MAIL stream
 *	    delimiter
 *	    mailbox name
 *	    mailbox attributes
 */

void mm_lsub (MAILSTREAM *stream,int delimiter,char *name,long attributes)
{
  fatal ("mm_lsub() call");
}


/* Mailbox status
 * Accepts: MAIL stream
 *	    mailbox name
 *	    mailbox status
 */

void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status)
{
  fatal ("mm_status() call");
}

/* Notification event
 * Accepts: MAIL stream
 *	    string to log
 *	    error flag
 */

void mm_notify (MAILSTREAM *stream,char *string,long errflg)
{
  char tmp[MAILTMPLEN];
  tmp[11] = '\0';		/* see if TRYCREATE */
  if (!strcmp (ucase (strncpy (tmp,string,11)),"[TRYCREATE]")) trycreate = T;
  mm_log (string,errflg);	/* just do mm_log action */
}


/* Log an event for the user to see
 * Accepts: string to log
 *	    error flag
 */

void mm_log (char *string,long errflg)
{
  if (trycreate)mm_dlog(string);/* debug logging only if trycreate in effect */
  else {			/* ordinary logging */
    fprintf (stderr,"%s\n",string);
    switch (errflg) {  
    case NIL:			/* no error */
      syslog (LOG_INFO,"%s",string);
      break;
    case PARSE:			/* parsing problem */
    case WARN:			/* warning */
      syslog (LOG_WARNING,"%s",string);
      break;
    case ERROR:			/* error */
    default:
      syslog (LOG_ERR,"%s",string);
      break;
    }
  }
}


/* Log an event to debugging telemetry
 * Accepts: string to log
 */

void mm_dlog (char *string)
{
  if (debug) fprintf (stderr,"%s\n",string);
  syslog (LOG_DEBUG,"%s",string);
}

/* Get user name and password for this host
 * Accepts: parse of network mailbox name
 *	    where to return user name
 *	    where to return password
 *	    trial count
 */

void mm_login (NETMBX *mb,char *username,char *password,long trial)
{
  fatal ("mm_login() call");
}


/* About to enter critical code
 * Accepts: stream
 */

void mm_critical (MAILSTREAM *stream)
{
  critical = T;			/* note in critical code */
}


/* About to exit critical code
 * Accepts: stream
 */

void mm_nocritical (MAILSTREAM *stream)
{
  critical = NIL;		/* note not in critical code */
}


/* Disk error found
 * Accepts: stream
 *	    system error code
 *	    flag indicating that mailbox may be clobbered
 * Returns: T if user wants to abort
 */

long mm_diskerror (MAILSTREAM *stream,long errcode,long serious)
{
  return T;
}


/* Log a fatal error event
 * Accepts: string to log
 */

void mm_fatal (char *string)
{
  printf ("?%s\n",string);	/* shouldn't happen normally */
}
