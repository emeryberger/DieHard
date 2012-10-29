/*
 * Program:	IPOP3D - IMAP to POP3 conversion server
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	1 November 1990
 * Last Edited:	16 September 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Parameter files */

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
extern int errno;		/* just in case */
#include <signal.h>
#include <time.h>
#include "c-client.h"


#define CRLF PSOUT ("\015\012")	/* primary output terpri */


/* Autologout timer */
#define KODTIMEOUT 60*5
#define LOGINTIMEOUT 60*3
#define TIMEOUT 60*10


/* Size of temporary buffers */
#define TMPLEN 1024


/* Server states */

#define AUTHORIZATION 0
#define TRANSACTION 1
#define UPDATE 2
#define LOGOUT 3

/* Eudora food */

#define STATUS "Status: %s%s\015\012"
#define SLEN (sizeof (STATUS)-3)


/* Global storage */

char *version = "2004.89";	/* server version */
short state = AUTHORIZATION;	/* server state */
short critical = NIL;		/* non-zero if in critical code */
MAILSTREAM *stream = NIL;	/* mailbox stream */
long idletime = 0;		/* time we went idle */
unsigned long nmsgs = 0;	/* current number of messages */
unsigned long ndele = 0;	/* number of deletes */
unsigned long last = 0;		/* highest message accessed */
unsigned long il = 0;		/* initial last message */
char challenge[128];		/* challenge */
char *host = NIL;		/* remote host name */
char *user = NIL;		/* user name */
char *pass = NIL;		/* password */
char *initial = NIL;		/* initial response */
long *msg = NIL;		/* message translation vector */
logouthook_t lgoh = NIL;	/* logout hook */
char *sayonara = "+OK Sayonara\015\012";


/* Function prototypes */

int main (int argc,char *argv[]);
void clkint ();
void kodint ();
void hupint ();
void trmint ();
int pass_login (char *t,int argc,char *argv[]);
char *apop_login (char *chal,char *user,char *md5,int argc,char *argv[]);
char *responder (void *challenge,unsigned long clen,unsigned long *rlen);
int mbxopen (char *mailbox);
long blat (char *text,long lines,unsigned long size);
void rset ();

/* Main program */

int main (int argc,char *argv[])
{
  unsigned long i,j,k;
  char *s,*t;
  char tmp[TMPLEN];
  time_t autologouttime;
  char *pgmname = (argc && argv[0]) ?
    (((s = strrchr (argv[0],'/')) || (s = strrchr (argv[0],'\\'))) ?
     s+1 : argv[0]) : "ipop3d";
				/* set service name before linkage */
  mail_parameters (NIL,SET_SERVICENAME,(void *) "pop");
#include "linkage.c"
				/* initialize server */
  server_init (pgmname,"pop3","pop3s",clkint,kodint,hupint,trmint);
  challenge[0] = '\0';		/* find the CRAM-MD5 authenticator */
  if (i = mail_lookup_auth_name ("CRAM-MD5",NIL)) {
    AUTHENTICATOR *a = mail_lookup_auth (i);
    if (a->server) {		/* have an MD5 enable file? */
				/* build challenge -- less than 128 chars */
      sprintf (challenge,"<%lx.%lx@%.64s>",(unsigned long) getpid (),
	       (unsigned long) time (0),tcp_serverhost ());
    }
  }
  /* There are reports of POP3 clients which get upset if anything appears
   * between the "+OK" and the "POP3" in the greeting.
   */
  PSOUT ("+OK POP3 ");
  if (!challenge[0]) {		/* if no MD5 enable, output host name */
    PSOUT (tcp_serverhost ());
    PBOUT (' ');
  }
  PSOUT (version);
  PSOUT (" server ready");
  if (challenge[0]) {		/* if MD5 enable, output challenge here */
    PBOUT (' ');
    PSOUT (challenge);
  }
  CRLF;
  PFLUSH ();			/* dump output buffer */
  autologouttime = time (0) + LOGINTIMEOUT;
				/* command processing loop */
  while ((state != UPDATE) && (state != LOGOUT)) {
    idletime = time (0);	/* get a command under timeout */
    alarm ((state == TRANSACTION) ? TIMEOUT : LOGINTIMEOUT);
    clearerr (stdin);		/* clear stdin errors */
    while (!PSIN (tmp,TMPLEN)){	/* read command line */
				/* ignore if some interrupt */
      if (ferror (stdin) && (errno == EINTR)) clearerr (stdin);
      else {
	char *e = ferror (stdin) ?
	  strerror (errno) : "Command stream end of file";
	alarm (0);		/* disable all interrupts */
	syslog (LOG_INFO,"%s while reading line user=%.80s host=%.80s",
		e,user ? user : "???",tcp_clienthost ());
	rset ();		/* try to gracefully close the stream */
	if (state == TRANSACTION) mail_close (stream);
	stream = NIL;
	state = LOGOUT;
				/* do logout hook if needed */
	if (lgoh = (logouthook_t) mail_parameters (NIL,GET_LOGOUTHOOK,NIL))
	  (*lgoh) (mail_parameters (NIL,GET_LOGOUTDATA,NIL));
	_exit (1);
      }
    }
    alarm (0);			/* make sure timeout disabled */
    idletime = 0;		/* no longer idle */

    if (!strchr (tmp,'\012'))	/* find end of line */
      PSOUT ("-ERR Command line too long\015\012");
    else if (!(s = strtok (tmp," \015\012")))
      PSOUT ("-ERR Null command\015\012");
    else {			/* dispatch based on command */
      ucase (s);		/* canonicalize case */
				/* snarf argument */
      t = strtok (NIL,"\015\012");
				/* QUIT command always valid */
      if (!strcmp (s,"QUIT")) state = UPDATE;
      else if (!strcmp (s,"CAPA")) {
	AUTHENTICATOR *auth;
	PSOUT ("+OK Capability list follows:\015\012");
	PSOUT ("TOP\015\012LOGIN-DELAY 180\015\012UIDL\015\012");
	if (s = ssl_start_tls (NIL)) fs_give ((void *) &s);
	else PSOUT ("STLS\015\012");
	if (i = !mail_parameters (NIL,GET_DISABLEPLAINTEXT,NIL))
	  PSOUT ("USER\015\012");
				/* display secure server authenticators */
	for (auth = mail_lookup_auth (1), s = "SASL"; auth; auth = auth->next)
	  if (auth->server && (i || (auth->flags & AU_SECURE))) {
	    if (s) {
	      PSOUT (s);
	      s = NIL;
	    }
	    PBOUT (' ');
	    PSOUT (auth->name);
	  }
	PSOUT (s ? ".\015\012" : "\015\012.\015\012");
      }

      else switch (state) {	/* else dispatch based on state */
      case AUTHORIZATION:	/* waiting to get logged in */
	if (!strcmp (s,"AUTH")) {
	  if (t && *t) {	/* mechanism given? */
	    if (host) fs_give ((void **) &host);
	    if (user) fs_give ((void **) &user);
	    if (pass) fs_give ((void **) &pass);
	    s = strtok (t," ");	/* get mechanism name */
				/* get initial response */
	    initial = strtok (NIL,"\015\012");
	    if (!(user = cpystr (mail_auth (s,responder,argc,argv)))) {
	      PSOUT ("-ERR Bad authentication\015\012");
	      syslog (LOG_INFO,"AUTHENTICATE %s failure host=%.80s",s,
		      tcp_clienthost ());
	    }
	    else if ((state = mbxopen ("INBOX")) == TRANSACTION)
	      syslog (LOG_INFO,"Auth user=%.80s host=%.80s nmsgs=%ld/%ld",
		      user,tcp_clienthost (),nmsgs,stream->nmsgs);
	    else syslog (LOG_INFO,"Auth user=%.80s host=%.80s no mailbox",
			 user,tcp_clienthost ());
	  }
	  else {
	    AUTHENTICATOR *auth;
	    PSOUT ("+OK Supported authentication mechanisms:\015\012");
	    i = !mail_parameters (NIL,GET_DISABLEPLAINTEXT,NIL);
	    for (auth = mail_lookup_auth (1); auth; auth = auth->next)
	      if (auth->server && (i || (auth->flags & AU_SECURE))) {
		PSOUT (auth->name);
		CRLF;
	      }
	    PBOUT ('.');
	    CRLF;
	  }
	}

	else if (!strcmp (s,"APOP")) {
	  if (challenge[0]) {	/* can do it if have an MD5 challenge */
	    if (host) fs_give ((void **) &host);
	    if (user) fs_give ((void **) &user);
	    if (pass) fs_give ((void **) &pass);
				/* get user name */
	    if (!(t && *t && (s = strtok (t," ")) && (t = strtok(NIL,"\012"))))
	      PSOUT ("-ERR Missing APOP argument\015\012");
	    else if (!(user = apop_login (challenge,s,t,argc,argv)))
	      PSOUT ("-ERR Bad APOP\015\012");
	    else if ((state = mbxopen ("INBOX")) == TRANSACTION)
	      syslog (LOG_INFO,"APOP user=%.80s host=%.80s nmsgs=%ld/%ld",
		      user,tcp_clienthost (),nmsgs,stream->nmsgs);
	    else syslog (LOG_INFO,"APOP user=%.80s host=%.80s no mailbox",
			 user,tcp_clienthost ());
	  }
	  else PSOUT ("-ERR Not supported\015\012");
	}
				/* (chuckle) */
	else if (!strcmp (s,"RPOP"))
	  PSOUT ("-ERR Nice try, bunkie\015\012");
	else if (!strcmp (s,"STLS")) {
	  if (t = ssl_start_tls (pgmname)) {
	    PSOUT ("-ERR STLS failed: ");
	    PSOUT (t);
	    CRLF;
	  }
	  else PSOUT ("+OK STLS completed\015\012");
	}
	else if (!mail_parameters (NIL,GET_DISABLEPLAINTEXT,NIL) &&
		 !strcmp (s,"USER")) {
	  if (host) fs_give ((void **) &host);
	  if (user) fs_give ((void **) &user);
	  if (pass) fs_give ((void **) &pass);
	  if (t && *t) {	/* if user name given */
				/* skip leading whitespace (bogus clients!) */
	    while (*t == ' ') ++t;
				/* remote user name? */
	    if (s = strchr (t,':')) {
	      *s++ = '\0';	/* tie off host name */
	      host = cpystr (t);/* copy host name */
	      user = cpystr (s);/* copy user name */
	    }
				/* local user name */
	    else user = cpystr (t);
	    PSOUT ("+OK User name accepted, password please\015\012");
	  }
	  else PSOUT ("-ERR Missing username argument\015\012");
	}
	else if (!mail_parameters (NIL,GET_DISABLEPLAINTEXT,NIL) &&
		 user && *user && !strcmp (s,"PASS"))
	  state = pass_login (t,argc,argv);
	else PSOUT ("-ERR Unknown AUTHORIZATION state command\015\012");
	break;

      case TRANSACTION:		/* logged in */
	if (!strcmp (s,"STAT")) {
	  for (i = 1,j = 0,k = 0; i <= nmsgs; i++)
	    if (msg[i] > 0) {	/* message still exists? */
	      j++;		/* count one more undeleted message */
	      k += mail_elt (stream,msg[i])->rfc822_size + SLEN;
	    }
	  sprintf (tmp,"+OK %lu %lu\015\012",j,k);
	  PSOUT (tmp);
	}
	else if (!strcmp (s,"LIST")) {
	  if (t && *t) {	/* argument do single message */
	    if ((i = strtoul (t,NIL,10)) && (i <= nmsgs) && (msg[i] > 0)) {
	      sprintf (tmp,"+OK %lu %lu\015\012",i,
		       mail_elt(stream,msg[i])->rfc822_size + SLEN);
	      PSOUT (tmp);
	    }
	    else PSOUT ("-ERR No such message\015\012");
	  }
	  else {		/* entire mailbox */
	    PSOUT ("+OK Mailbox scan listing follows\015\012");
	    for (i = 1,j = 0,k = 0; i <= nmsgs; i++) if (msg[i] > 0) {
	      sprintf (tmp,"%lu %lu\015\012",i,
		       mail_elt (stream,msg[i])->rfc822_size + SLEN);
	      PSOUT (tmp);
	    }
	    PBOUT ('.');	/* end of list */
	    CRLF;
	  }
	}
	else if (!strcmp (s,"UIDL")) {
	  if (t && *t) {	/* argument do single message */
	    if ((i = strtoul (t,NIL,10)) && (i <= nmsgs) && (msg[i] > 0)) {
	      sprintf (tmp,"+OK %lu %08lx%08lx\015\012",i,stream->uid_validity,
		       mail_uid (stream,msg[i]));
	      PSOUT (tmp);
	    }
	    else PSOUT ("-ERR No such message\015\012");
	  }
	  else {		/* entire mailbox */
	    PSOUT ("+OK Unique-ID listing follows\015\012");
	    for (i = 1,j = 0,k = 0; i <= nmsgs; i++) if (msg[i] > 0) {
	      sprintf (tmp,"%lu %08lx%08lx\015\012",i,stream->uid_validity,
		       mail_uid (stream,msg[i]));
	      PSOUT (tmp);
	    }
	    PBOUT ('.');	/* end of list */
	    CRLF;
	  }
	}

	else if (!strcmp (s,"RETR")) {
	  if (t && *t) {	/* must have an argument */
	    if ((i = strtoul (t,NIL,10)) && (i <= nmsgs) && (msg[i] > 0)) {
	      MESSAGECACHE *elt;
				/* update highest message accessed */
	      if (i > last) last = i;
	      sprintf (tmp,"+OK %lu octets\015\012",
		       (elt = mail_elt (stream,msg[i]))->rfc822_size + SLEN);
	      PSOUT (tmp);
				/* output header */
	      t = mail_fetch_header (stream,msg[i],NIL,NIL,&k,FT_PEEK);
	      blat (t,-1,k);
				/* output status */
	      sprintf (tmp,STATUS,elt->seen ? "R" : " ",
		       elt->recent ? " " : "O");
	      PSOUT (tmp);
	      CRLF;		/* delimit header and text */
				/* output text */
	      t = mail_fetch_text (stream,msg[i],NIL,&k,NIL);
	      blat (t,-1,k);
	      CRLF;		/* end of list */
	      PBOUT ('.');
	      CRLF;
	    }
	    else PSOUT ("-ERR No such message\015\012");
	  }
	  else PSOUT ("-ERR Missing message number argument\015\012");
	}
	else if (!strcmp (s,"DELE")) {
	  if (t && *t) {	/* must have an argument */
	    if ((i = strtoul (t,NIL,10)) && (i <= nmsgs) && (msg[i] > 0)) {
				/* update highest message accessed */
	      if (i > last) last = i;
				/* delete message */
	      sprintf (tmp,"%ld",msg[i]);
	      mail_setflag (stream,tmp,"\\Deleted");
	      msg[i] = -msg[i];	/* note that we deleted this message */
	      PSOUT ("+OK Message deleted\015\012");
	      ndele++;		/* one more message deleted */
	    }
	    else PSOUT ("-ERR No such message\015\012");
	  }
	  else PSOUT ("-ERR Missing message number argument\015\012");
	}

	else if (!strcmp (s,"NOOP"))
	  PSOUT ("+OK No-op to you too!\015\012");
	else if (!strcmp (s,"LAST")) {
	  sprintf (tmp,"+OK %lu\015\012",last);
	  PSOUT (tmp);
	}
	else if (!strcmp (s,"RSET")) {
	  rset ();		/* reset the mailbox */
	  PSOUT ("+OK Reset state\015\012");
	}
	else if (!strcmp (s,"TOP")) {
	  if (t && *t && (i =strtoul (t,&s,10)) && (i <= nmsgs) &&
	      (msg[i] > 0)) {
				/* skip whitespace */
	    while (*s == ' ') s++;
				/* make sure line count argument good */
	    if ((*s >= '0') && (*s <= '9')) {
	      MESSAGECACHE *elt = mail_elt (stream,msg[i]);
	      j = strtoul (s,NIL,10);
				/* update highest message accessed */
	      if (i > last) last = i;
	      PSOUT ("+OK Top of message follows\015\012");
				/* output header */
	      t = mail_fetch_header (stream,msg[i],NIL,NIL,&k,FT_PEEK);
	      blat (t,-1,k);
				/* output status */
	      sprintf (tmp,STATUS,elt->seen ? "R" : " ",
		       elt->recent ? " " : "O");
	      PSOUT (tmp);
	      CRLF;		/* delimit header and text */
	      if (j) {		/* want any text lines? */
				/* output text */
		t = mail_fetch_text (stream,msg[i],NIL,&k,FT_PEEK);
				/* tie off final line if full text output */
		if (j -= blat (t,j,k)) CRLF;
	      }
	      PBOUT ('.');	/* end of list */
	      CRLF;
	    }
	    else PSOUT ("-ERR Bad line count argument\015\012");
	  }
	  else PSOUT ("-ERR Bad message number argument\015\012");
	}
	else if (!strcmp (s,"XTND"))
	  PSOUT ("-ERR Sorry I can't do that\015\012");
	else PSOUT ("-ERR Unknown TRANSACTION state command\015\012");
	break;
      default:
        PSOUT ("-ERR Server in unknown state\015\012");
	break;
      }
    }
    PFLUSH ();			/* make sure output finished */
    if (autologouttime) {	/* have an autologout in effect? */
				/* cancel if no longer waiting for login */
      if (state != AUTHORIZATION) autologouttime = 0;
				/* took too long to login */
      else if (autologouttime < time (0)) {
	PSOUT ("-ERR Autologout\015\012");
	syslog (LOG_INFO,"Autologout host=%.80s",tcp_clienthost ());
	PFLUSH ();		/* make sure output blatted */
	state = LOGOUT;		/* sayonara */
      }
    }
  }
  if (stream && (state == UPDATE)) {
    mail_expunge (stream);
    syslog (LOG_INFO,"Logout user=%.80s host=%.80s nmsgs=%ld ndele=%ld",
	    user,tcp_clienthost (),stream->nmsgs,ndele);
    mail_close (stream);
  }
  else syslog (LOG_INFO,"Logout user=%.80s host=%.80s",user ? user : "???",
	       tcp_clienthost ());
  PSOUT (sayonara);		/* "now it's time to say sayonara..." */
  PFLUSH ();			/* make sure output finished */
				/* do logout hook if needed */
  if (lgoh = (logouthook_t) mail_parameters (NIL,GET_LOGOUTHOOK,NIL))
    (*lgoh) (mail_parameters (NIL,GET_LOGOUTDATA,NIL));
  exit (0);			/* all done */
  return 0;			/* stupid compilers */
}

/* Clock interrupt
 */

void clkint ()
{
  PSOUT ("-ERR Autologout; idle for too long\015\012");
  syslog (LOG_INFO,"Autologout user=%.80s host=%.80s",user ? user : "???",
	  tcp_clienthost ());
  PFLUSH ();			/* make sure output blatted */
  if (critical) state = LOGOUT;	/* badly hosed if in critical code */
  else {			/* try to gracefully close the stream */
    if ((state == TRANSACTION) && !stream->lock) {
      rset ();
      mail_close (stream);
    }
    state = LOGOUT;
    stream = NIL;
				/* do logout hook if needed */
    if (lgoh = (logouthook_t) mail_parameters (NIL,GET_LOGOUTHOOK,NIL))
      (*lgoh) (mail_parameters (NIL,GET_LOGOUTDATA,NIL));
    _exit (1);			/* die die die */
  }
}


/* Kiss Of Death interrupt
 */

void kodint ()
{
				/* only if idle */
  if (idletime && ((time (0) - idletime) > KODTIMEOUT)) {
    alarm (0);			/* disable all interrupts */
    server_init (NIL,NIL,NIL,SIG_IGN,SIG_IGN,SIG_IGN,SIG_IGN);
    PSOUT ("-ERR Received Kiss of Death\015\012");
    syslog (LOG_INFO,"Killed (lost mailbox lock) user=%.80s host=%.80s",
	    user ? user : "???",tcp_clienthost ());
    if (critical) state =LOGOUT;/* must defer if in critical code */
    else {			/* try to gracefully close the stream */
      if ((state == TRANSACTION) && !stream->lock) {
	rset ();
	mail_close (stream);
      }
      state = LOGOUT;
      stream = NIL;
				/* do logout hook if needed */
      if (lgoh = (logouthook_t) mail_parameters (NIL,GET_LOGOUTHOOK,NIL))
	(*lgoh) (mail_parameters (NIL,GET_LOGOUTDATA,NIL));
      _exit (1);		/* die die die */
    }
  }
}


/* Hangup interrupt
 */

void hupint ()
{
  alarm (0);			/* disable all interrupts */
  server_init (NIL,NIL,NIL,SIG_IGN,SIG_IGN,SIG_IGN,SIG_IGN);
  syslog (LOG_INFO,"Hangup user=%.80s host=%.80s",user ? user : "???",
	  tcp_clienthost ());
  if (critical) state = LOGOUT;	/* must defer if in critical code */
  else {			/* try to gracefully close the stream */
    if ((state == TRANSACTION) && !stream->lock) {
      rset ();
      mail_close (stream);
    }
    state = LOGOUT;
    stream = NIL;
				/* do logout hook if needed */
    if (lgoh = (logouthook_t) mail_parameters (NIL,GET_LOGOUTHOOK,NIL))
      (*lgoh) (mail_parameters (NIL,GET_LOGOUTDATA,NIL));
    _exit (1);			/* die die die */
  }
}


/* Termination interrupt
 */

void trmint ()
{
  alarm (0);			/* disable all interrupts */
  server_init (NIL,NIL,NIL,SIG_IGN,SIG_IGN,SIG_IGN,SIG_IGN);
  PSOUT ("-ERR Killed\015\012");
  syslog (LOG_INFO,"Killed user=%.80s host=%.80s",user ? user : "???",
	  tcp_clienthost ());
  if (critical) state = LOGOUT;	/* must defer if in critical code */
  /* Make no attempt at graceful closure since a shutdown may be in
   * progress, and we won't have any time to do mail_close() actions.
   */
  else _exit (1);		/* die die die */
}

/* Parse PASS command
 * Accepts: pointer to command argument
 * Returns: new state
 */

int pass_login (char *t,int argc,char *argv[])
{
  char tmp[TMPLEN];
				/* flush old passowrd */
  if (pass) fs_give ((void **) &pass);
  if (!(t && *t)) {		/* if no password given */
    PSOUT ("-ERR Missing password argument\015\012");
    return AUTHORIZATION;
  }
  pass = cpystr (t);		/* copy password argument */
  if (!host) {			/* want remote mailbox? */
				/* no, delimit user from possible admin */
    if (t = strchr (user,'*')) *t++ ='\0';
				/* attempt the login */
    if (server_login (user,pass,t,argc,argv)) {
      int ret = mbxopen ("INBOX");
      if (ret == TRANSACTION)	/* mailbox opened OK? */
	syslog (LOG_INFO,"%sLogin user=%.80s host=%.80s nmsgs=%ld/%ld",
		t ? "Admin " : "",user,tcp_clienthost (),nmsgs,stream->nmsgs);
      else syslog (LOG_INFO,"%sLogin user=%.80s host=%.80s no mailbox",
		   t ? "Admin " : "",user,tcp_clienthost ());
      return ret;
    }
  }
#ifndef DISABLE_POP_PROXY
				/* remote; build remote INBOX */
  else if (anonymous_login (argc,argv)) {
    syslog (LOG_INFO,"IMAP login to host=%.80s user=%.80s host=%.80s",host,
	    user,tcp_clienthost ());
    sprintf (tmp,"{%.128s/user=%.128s}INBOX",host,user);
				/* disable rimap just in case */
    mail_parameters (NIL,SET_RSHTIMEOUT,0);
    return mbxopen (tmp);
  }
#endif
				/* vague error message to confuse crackers */
  PSOUT ("-ERR Bad login\015\012");
  return AUTHORIZATION;
}

/* Authentication responder
 * Accepts: challenge
 *	    length of challenge
 *	    pointer to response length return location if non-NIL
 * Returns: response
 */

#define RESPBUFLEN 8*MAILTMPLEN

char *responder (void *challenge,unsigned long clen,unsigned long *rlen)
{
  unsigned long i,j;
  unsigned char *t,resp[RESPBUFLEN];
  if (initial) {		/* initial response given? */
    if (clen) return NIL;	/* not permitted */
				/* set up response */
    t = (unsigned char *) initial;
    initial = NIL;		/* no more initial response */
    return (char *) rfc822_base64 (t,strlen ((char *) t),rlen ? rlen : &i);
  }
  PSOUT ("+ ");
  for (t = rfc822_binary (challenge,clen,&i),j = 0; j < i; j++)
    if (t[j] > ' ') PBOUT (t[j]);
  fs_give ((void **) &t);
  CRLF;
  PFLUSH ();			/* dump output buffer */
  resp[RESPBUFLEN-1] = '\0';	/* last buffer character is guaranteed NUL */
  alarm (LOGINTIMEOUT);		/* get a response under timeout */
  clearerr (stdin);		/* clear stdin errors */
				/* read buffer */
  while (!PSIN ((char *) resp,RESPBUFLEN)) {
				/* ignore if some interrupt */
    if (ferror (stdin) && (errno == EINTR)) clearerr (stdin);
    else {
      char *e = ferror (stdin) ?
	strerror (errno) : "Command stream end of file";
      alarm (0);		/* disable all interrupts */
      server_init (NIL,NIL,NIL,SIG_IGN,SIG_IGN,SIG_IGN,SIG_IGN);
      syslog (LOG_INFO,"%s, while reading authentication host=%.80s",
	      e,tcp_clienthost ());
      state = UPDATE;
				/* do logout hook if needed */
      if (lgoh = (logouthook_t) mail_parameters (NIL,GET_LOGOUTHOOK,NIL))
	(*lgoh) (mail_parameters (NIL,GET_LOGOUTDATA,NIL));
      _exit (1);
    }
  }
  if (!(t = (unsigned char *) strchr ((char *) resp,'\012'))) {
    int c;
    while ((c = PBIN ()) != '\012') if (c == EOF) {
				/* ignore if some interrupt */
      if (ferror (stdin) && (errno == EINTR)) clearerr (stdin);
      else {
	char *e = ferror (stdin) ?
	  strerror (errno) : "Command stream end of file";
	alarm (0);		/* disable all interrupts */
	server_init (NIL,NIL,NIL,SIG_IGN,SIG_IGN,SIG_IGN,SIG_IGN);
	syslog (LOG_INFO,"%s, while reading auth char user=%.80s host=%.80s",
		e,user ? user : "???",tcp_clienthost ());
	state = UPDATE;
				/* do logout hook if needed */
	if (lgoh = (logouthook_t) mail_parameters (NIL,GET_LOGOUTHOOK,NIL))
	  (*lgoh) (mail_parameters (NIL,GET_LOGOUTDATA,NIL));
	_exit (1);
      }
    }
    return NIL;
  }
  alarm (0);			/* make sure timeout disabled */
  if (t[-1] == '\015') --t;	/* remove CR */
  *t = '\0';			/* tie off buffer */
  return (resp[0] != '*') ?
    (char *) rfc822_base64 (resp,t-resp,rlen ? rlen : &i) : NIL;
}

/* Select mailbox
 * Accepts: mailbox name
 * Returns: new state
 */

int mbxopen (char *mailbox)
{
  unsigned long i,j;
  char tmp[TMPLEN];
  MESSAGECACHE *elt;
  nmsgs = 0;			/* no messages yet */
  if (msg) fs_give ((void **) &msg);
				/* open mailbox */
  if (stream = mail_open (stream,mailbox,NIL)) {
    if (!stream->rdonly) {	/* make sure not readonly */
      if (j = stream->nmsgs) {	/* if mailbox non-empty */
	sprintf (tmp,"1:%lu",j);/* fetch fast information for all messages */
	mail_fetch_fast (stream,tmp,NIL);
	msg = (long *) fs_get ((stream->nmsgs + 1) * sizeof (long));
	for (i = 1; i <= j; i++) if (!(elt = mail_elt (stream,i))->deleted) {
	  msg[++nmsgs] = i;	/* note the presence of this message */
	  if (elt->seen) il = last = nmsgs;
	}
      }
      sprintf (tmp,"+OK Mailbox open, %lu messages\015\012",nmsgs);
      PSOUT (tmp);
      return TRANSACTION;
    }
    else sayonara = "-ERR Can't get lock.  Mailbox in use\015\012";
  }
  else sayonara = "-ERR Unable to open user's INBOX\015\012";
  syslog (LOG_INFO,"Error opening or locking INBOX user=%.80s host=%.80s",
	  user,tcp_clienthost ());
  return UPDATE;
}

/* Blat a string with dot checking
 * Accepts: string
 *	    maximum number of lines if greater than zero
 *	    maximum number of bytes to output
 * Returns: number of lines output
 *
 * This routine is uglier and kludgier than it should be, just to be robust
 * in the case of a message which doesn't end in a newline.  Yes, this routine
 * does truncate the last two bytes from the text.  Since it is normally a
 * newline and the main routine adds it back, it usually does not make a
 * difference.  But if it isn't, since the newline is required and the octet
 * counts have to match, there's no choice but to truncate.
 */

long blat (char *text,long lines,unsigned long size)
{
  char c,d,e;
  long ret = 0;
				/* no-op if zero lines or empty string */
  if (!(lines && (size-- > 2))) return 0;
  c = *text++; d = *text++;	/* collect first two bytes */
  if (c == '.') PBOUT ('.');	/* double string-leading dot if necessary */
  while (lines && --size) {	/* copy loop */
    e = *text++;		/* get next byte */
    PBOUT (c);			/* output character */
    if (c == '\012') {		/* end of line? */
      ret++; --lines;		/* count another line */
				/* double leading dot as necessary */
      if (lines && size && (d == '.')) PBOUT ('.');
    }
    c = d; d = e;		/* move to next character */
  }
  return ret;
}


/* Reset mailbox
 */

void rset ()
{
  unsigned long i;
  char tmp[20];
  if (nmsgs) {			/* undelete and unmark all of our messages */
    for (i = 1; i <= nmsgs; i++) { /*  */
      if (msg[i] < 0) {		/* ugly and inefficient, but trustworthy */
	sprintf (tmp,"%ld",msg[i] = -msg[i]);
	mail_clearflag (stream,tmp,i <= il ? "\\Deleted" : "\\Deleted \\Seen");
      }
      else if (i > il) {
	sprintf (tmp,"%ld",msg[i]);
	mail_clearflag (stream,tmp,"\\Seen");
      }
    }
    last = il;
  }
  ndele = 0;			/* no more deleted messages */
}

/* Co-routines from MAIL library */


/* Message matches a search
 * Accepts: MAIL stream
 *	    message number
 */

void mm_searched (MAILSTREAM *stream,unsigned long msgno)
{
  /* Never called */
}


/* Message exists (i.e. there are that many messages in the mailbox)
 * Accepts: MAIL stream
 *	    message number
 */

void mm_exists (MAILSTREAM *stream,unsigned long number)
{
  /* Can't use this mechanism.  POP has no means of notifying the client of
     new mail during the session. */
}


/* Message expunged
 * Accepts: MAIL stream
 *	    message number
 */

void mm_expunged (MAILSTREAM *stream,unsigned long number)
{
  unsigned long i = number + 1;
  msg[number] = 0;		/* I bet that this will annoy someone */
  while (i <= nmsgs) --msg[i++];
}


/* Message flag status change
 * Accepts: MAIL stream
 *	    message number
 */

void mm_flags (MAILSTREAM *stream,unsigned long number)
{
  /* This isn't used */
}


/* Mailbox found
 * Accepts: MAIL stream
 *	    hierarchy delimiter
 *	    mailbox name
 *	    mailbox attributes
 */

void mm_list (MAILSTREAM *stream,int delimiter,char *name,long attributes)
{
  /* This isn't used */
}


/* Subscribe mailbox found
 * Accepts: MAIL stream
 *	    hierarchy delimiter
 *	    mailbox name
 *	    mailbox attributes
 */

void mm_lsub (MAILSTREAM *stream,int delimiter,char *name,long attributes)
{
  /* This isn't used */
}


/* Mailbox status
 * Accepts: MAIL stream
 *	    mailbox name
 *	    mailbox status
 */

void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status)
{
  /* This isn't used */
}

/* Notification event
 * Accepts: MAIL stream
 *	    string to log
 *	    error flag
 */

void mm_notify (MAILSTREAM *stream,char *string,long errflg)
{
  mm_log (string,errflg);	/* just do mm_log action */
}


/* Log an event for the user to see
 * Accepts: string to log
 *	    error flag
 */

void mm_log (char *string,long errflg)
{
  switch (errflg) {
  case NIL:			/* information message */
  case PARSE:			/* parse glitch */
    break;			/* too many of these to log */
  case WARN:			/* warning */
    syslog (LOG_DEBUG,"%s",string);
    break;
  case BYE:			/* driver broke connection */
    if (state != UPDATE) {
      alarm (0);		/* disable all interrupts */
      server_init (NIL,NIL,NIL,SIG_IGN,SIG_IGN,SIG_IGN,SIG_IGN);
      syslog (LOG_INFO,"Mailbox closed (%.80s) user=%.80s host=%.80s",
	      string,user ? user : "???",tcp_clienthost ());
				/* do logout hook if needed */
      if (lgoh = (logouthook_t) mail_parameters (NIL,GET_LOGOUTHOOK,NIL))
	(*lgoh) (mail_parameters (NIL,GET_LOGOUTDATA,NIL));
      _exit (1);
    }
    break;
  case ERROR:			/* error that broke command */
  default:			/* default should never happen */
    syslog (LOG_NOTICE,"%s",string);
    break;
  }
}    


/* Log an event to debugging telemetry
 * Accepts: string to log
 */

void mm_dlog (char *string)
{
  /* Not doing anything here for now */
}


/* Get user name and password for this host
 * Accepts: parse of network mailbox name
 *	    where to return user name
 *	    where to return password
 *	    trial count
 */

void mm_login (NETMBX *mb,char *username,char *password,long trial)
{
				/* set user name */
  strncpy (username,*mb->user ? mb->user : user,NETMAXUSER-1);
  strncpy (password,pass,255);	/* and password */
  username[NETMAXUSER] = password[255] = '\0';
}

/* About to enter critical code
 * Accepts: stream
 */

void mm_critical (MAILSTREAM *stream)
{
  ++critical;
}


/* About to exit critical code
 * Accepts: stream
 */

void mm_nocritical (MAILSTREAM *stream)
{
  --critical;
}


/* Disk error found
 * Accepts: stream
 *	    system error code
 *	    flag indicating that mailbox may be clobbered
 * Returns: abort flag
 */

long mm_diskerror (MAILSTREAM *stream,long errcode,long serious)
{
  if (serious) {		/* try your damnest if clobberage likely */
    syslog (LOG_ALERT,
	    "Retrying after disk error user=%.80s host=%.80s mbx=%.80s: %.80s",
	    user,tcp_clienthost (),
	    (stream && stream->mailbox) ? stream->mailbox : "???",
	    strerror (errcode));
    alarm (0);			/* make damn sure timeout disabled */
    sleep (60);			/* give it some time to clear up */
    return NIL;
  }
  syslog (LOG_ALERT,"Fatal disk error user=%.80s host=%.80s mbx=%.80s: %.80s",
	  user,tcp_clienthost (),
	  (stream && stream->mailbox) ? stream->mailbox : "???",
	  strerror (errcode));
  return T;
}


/* Log a fatal error event
 * Accepts: string to log
 */

void mm_fatal (char *string)
{
  mm_log (string,ERROR);	/* shouldn't happen normally */
}
