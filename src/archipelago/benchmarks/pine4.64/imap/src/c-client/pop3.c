/*
 * Program:	Post Office Protocol 3 (POP3) client routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	6 June 1994
 * Last Edited:	5 March 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


#include "mail.h"
#include "osdep.h"
#include <ctype.h>
#include <stdio.h>
#include <time.h>
#include "rfc822.h"
#include "misc.h"
#include "netmsg.h"
#include "flstring.h"

/* Parameters */

#define POP3TCPPORT (long) 110	/* assigned TCP contact port */
#define POP3SSLPORT (long) 995	/* assigned SSL TCP contact port */
#define IDLETIMEOUT (long) 10	/* defined in RFC 1939 */


/* POP3 I/O stream local data */
	
typedef struct pop3_local {
  NETSTREAM *netstream;		/* TCP I/O stream */
  char *response;		/* last server reply */
  char *reply;			/* text of last server reply */
  unsigned long cached;		/* current cached message uid */
  unsigned long hdrsize;	/* current cached header size */
  FILE *txt;			/* current cached file descriptor */
  struct {
    unsigned int capa : 1;	/* server has CAPA, definitely new */
    unsigned int expire : 1;	/* server has EXPIRE */
    unsigned int logindelay : 1;/* server has LOGIN-DELAY */
    unsigned int stls : 1;	/* server has STLS */
    unsigned int pipelining : 1;/* server has PIPELINING */
    unsigned int respcodes : 1;	/* server has RESP-CODES */
    unsigned int top : 1;	/* server has TOP */
    unsigned int uidl : 1;	/* server has UIDL */
    unsigned int user : 1;	/* server has USER */
    char *implementation;	/* server implementation string */
    long delaysecs;		/* minimum time between login (neg variable) */
    long expiredays;		/* server-guaranteed minimum retention days */
				/* supported authenticators */
    unsigned int sasl : MAXAUTHENTICATORS;
  } cap;
  unsigned int sensitive : 1;	/* sensitive data in progress */
  unsigned int loser : 1;	/* server is a loser */
  unsigned int saslcancel : 1;	/* SASL cancelled by protocol */
} POP3LOCAL;


/* Convenient access to local data */

#define LOCAL ((POP3LOCAL *) stream->local)

/* Function prototypes */

DRIVER *pop3_valid (char *name);
void *pop3_parameters (long function,void *value);
void pop3_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void pop3_list (MAILSTREAM *stream,char *ref,char *pat);
void pop3_lsub (MAILSTREAM *stream,char *ref,char *pat);
long pop3_subscribe (MAILSTREAM *stream,char *mailbox);
long pop3_unsubscribe (MAILSTREAM *stream,char *mailbox);
long pop3_create (MAILSTREAM *stream,char *mailbox);
long pop3_delete (MAILSTREAM *stream,char *mailbox);
long pop3_rename (MAILSTREAM *stream,char *old,char *newname);
long pop3_status (MAILSTREAM *stream,char *mbx,long flags);
MAILSTREAM *pop3_open (MAILSTREAM *stream);
long pop3_capa (MAILSTREAM *stream,long flags);
long pop3_auth (MAILSTREAM *stream,NETMBX *mb,char *pwd,char *usr);
void *pop3_challenge (void *stream,unsigned long *len);
long pop3_response (void *stream,char *s,unsigned long size);
void pop3_close (MAILSTREAM *stream,long options);
void pop3_fetchfast (MAILSTREAM *stream,char *sequence,long flags);
char *pop3_header (MAILSTREAM *stream,unsigned long msgno,unsigned long *size,
		   long flags);
long pop3_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags);
unsigned long pop3_cache (MAILSTREAM *stream,MESSAGECACHE *elt);
long pop3_ping (MAILSTREAM *stream);
void pop3_check (MAILSTREAM *stream);
void pop3_expunge (MAILSTREAM *stream);
long pop3_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options);
long pop3_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);

long pop3_send_num (MAILSTREAM *stream,char *command,unsigned long n);
long pop3_send (MAILSTREAM *stream,char *command,char *args);
long pop3_reply (MAILSTREAM *stream);
long pop3_fake (MAILSTREAM *stream,char *text);

/* POP3 mail routines */


/* Driver dispatch used by MAIL */

DRIVER pop3driver = {
  "pop3",			/* driver name */
				/* driver flags */
#ifdef INADEQUATE_MEMORY
  DR_LOWMEM |
#endif
  DR_MAIL|DR_NOFAST|DR_CRLF|DR_NOSTICKY|DR_NOINTDATE|DR_NONEWMAIL,
  (DRIVER *) NIL,		/* next driver */
  pop3_valid,			/* mailbox is valid for us */
  pop3_parameters,		/* manipulate parameters */
  pop3_scan,			/* scan mailboxes */
  pop3_list,			/* find mailboxes */
  pop3_lsub,			/* find subscribed mailboxes */
  pop3_subscribe,		/* subscribe to mailbox */
  pop3_unsubscribe,		/* unsubscribe from mailbox */
  pop3_create,			/* create mailbox */
  pop3_delete,			/* delete mailbox */
  pop3_rename,			/* rename mailbox */
  pop3_status,			/* status of mailbox */
  pop3_open,			/* open mailbox */
  pop3_close,			/* close mailbox */
  pop3_fetchfast,		/* fetch message "fast" attributes */
  NIL,				/* fetch message flags */
  NIL,				/* fetch overview */
  NIL,				/* fetch message structure */
  pop3_header,			/* fetch message header */
  pop3_text,			/* fetch message text */
  NIL,				/* fetch message */
  NIL,				/* unique identifier */
  NIL,				/* message number from UID */
  NIL,				/* modify flags */
  NIL,				/* per-message modify flags */
  NIL,				/* search for message based on criteria */
  NIL,				/* sort messages */
  NIL,				/* thread messages */
  pop3_ping,			/* ping mailbox to see if still alive */
  pop3_check,			/* check for new messages */
  pop3_expunge,			/* expunge deleted messages */
  pop3_copy,			/* copy messages to another mailbox */
  pop3_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM pop3proto = {&pop3driver};

				/* driver parameters */
static unsigned long pop3_maxlogintrials = MAXLOGINTRIALS;
static long pop3_port = 0;
static long pop3_sslport = 0;

/* POP3 mail validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *pop3_valid (char *name)
{
  NETMBX mb;
  char mbx[MAILTMPLEN];
  return (mail_valid_net_parse (name,&mb) &&
	  !strcmp (mb.service,pop3driver.name) && !mb.authuser[0] &&
	  !strcmp (ucase (strcpy (mbx,mb.mailbox)),"INBOX")) ?
	    &pop3driver : NIL;
}


/* News manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *pop3_parameters (long function,void *value)
{
  switch ((int) function) {
  case SET_MAXLOGINTRIALS:
    pop3_maxlogintrials = (unsigned long) value;
    break;
  case GET_MAXLOGINTRIALS:
    value = (void *) pop3_maxlogintrials;
    break;
  case SET_POP3PORT:
    pop3_port = (long) value;
    break;
  case GET_POP3PORT:
    value = (void *) pop3_port;
    break;
  case SET_SSLPOPPORT:
    pop3_sslport = (long) value;
    break;
  case GET_SSLPOPPORT:
    value = (void *) pop3_sslport;
    break;
  case GET_IDLETIMEOUT:
    value = (void *) IDLETIMEOUT;
    break;
  default:
    value = NIL;		/* error case */
    break;
  }
  return value;
}

/* POP3 mail scan mailboxes for string
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void pop3_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  char tmp[MAILTMPLEN];
  if ((ref && *ref) ?		/* have a reference */
      (pop3_valid (ref) && pmatch ("INBOX",pat)) :
      (mail_valid_net (pat,&pop3driver,NIL,tmp) && pmatch ("INBOX",tmp)))
    mm_log ("Scan not valid for POP3 mailboxes",ERROR);
}


/* POP3 mail find list of all mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void pop3_list (MAILSTREAM *stream,char *ref,char *pat)
{
  char tmp[MAILTMPLEN];
  if (ref && *ref) {		/* have a reference */
    if (pop3_valid (ref) && pmatch ("INBOX",pat)) {
      strcpy (strchr (strcpy (tmp,ref),'}')+1,"INBOX");
      mm_list (stream,NIL,tmp,LATT_NOINFERIORS);
    }
  }
  else if (mail_valid_net (pat,&pop3driver,NIL,tmp) && pmatch ("INBOX",tmp)) {
    strcpy (strchr (strcpy (tmp,pat),'}')+1,"INBOX");
    mm_list (stream,NIL,tmp,LATT_NOINFERIORS);
  }
}

/* POP3 mail find list of subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void pop3_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  void *sdb = NIL;
  char *s,mbx[MAILTMPLEN];
  if (*pat == '{') {		/* if remote pattern, must be POP3 */
    if (!pop3_valid (pat)) return;
    ref = NIL;			/* good POP3 pattern, punt reference */
  }
				/* if remote reference, must be valid POP3 */
  if (ref && (*ref == '{') && !pop3_valid (ref)) return;
				/* kludgy application of reference */
  if (ref && *ref) sprintf (mbx,"%s%s",ref,pat);
  else strcpy (mbx,pat);

  if (s = sm_read (&sdb)) do if (pop3_valid (s) && pmatch (s,mbx))
    mm_lsub (stream,NIL,s,NIL);
  while (s = sm_read (&sdb));	/* until no more subscriptions */
}


/* POP3 mail subscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to add to subscription list
 * Returns: T on success, NIL on failure
 */

long pop3_subscribe (MAILSTREAM *stream,char *mailbox)
{
  return sm_subscribe (mailbox);
}


/* POP3 mail unsubscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to delete from subscription list
 * Returns: T on success, NIL on failure
 */

long pop3_unsubscribe (MAILSTREAM *stream,char *mailbox)
{
  return sm_unsubscribe (mailbox);
}

/* POP3 mail create mailbox
 * Accepts: mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long pop3_create (MAILSTREAM *stream,char *mailbox)
{
  return NIL;			/* never valid for POP3 */
}


/* POP3 mail delete mailbox
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long pop3_delete (MAILSTREAM *stream,char *mailbox)
{
  return NIL;			/* never valid for POP3 */
}


/* POP3 mail rename mailbox
 * Accepts: mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long pop3_rename (MAILSTREAM *stream,char *old,char *newname)
{
  return NIL;			/* never valid for POP3 */
}

/* POP3 status
 * Accepts: mail stream
 *	    mailbox name
 *	    status flags
 * Returns: T on success, NIL on failure
 */

long pop3_status (MAILSTREAM *stream,char *mbx,long flags)
{
  MAILSTATUS status;
  unsigned long i;
  long ret = NIL;
  MAILSTREAM *tstream =
    (stream && LOCAL->netstream && mail_usable_network_stream (stream,mbx)) ?
      stream : mail_open (NIL,mbx,OP_SILENT);
  if (tstream) {		/* have a usable stream? */
    status.flags = flags;	/* return status values */
    status.messages = tstream->nmsgs;
    status.recent = tstream->recent;
    if (flags & SA_UNSEEN)	/* must search to get unseen messages */
      for (i = 1,status.unseen = 0; i <= tstream->nmsgs; i++)
	if (!mail_elt (tstream,i)->seen) status.unseen++;
    status.uidnext = tstream->uid_last + 1;
    status.uidvalidity = tstream->uid_validity;
				/* pass status to main program */
    mm_status (tstream,mbx,&status);
    if (stream != tstream) mail_close (tstream);
    ret = LONGT;
  }
  return ret;			/* success */
}

/* POP3 mail open
 * Accepts: stream to open
 * Returns: stream on success, NIL on failure
 */

MAILSTREAM *pop3_open (MAILSTREAM *stream)
{
  unsigned long i,j;
  char *s,*t,tmp[MAILTMPLEN],usr[MAILTMPLEN];
  NETMBX mb;
  MESSAGECACHE *elt;
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return &pop3proto;
  mail_valid_net_parse (stream->mailbox,&mb);
  usr[0] = '\0';		/* initially no user name */
  if (stream->local) fatal ("pop3 recycle stream");
				/* /anonymous not supported */
  if (mb.anoflag || stream->anonymous) {
    mm_log ("Anonymous POP3 login not available",ERROR);
    return NIL;
  }
				/* /readonly not supported either */
  if (mb.readonlyflag || stream->rdonly) {
    mm_log ("Read-only POP3 access not available",ERROR);
    return NIL;
  }
				/* copy other switches */
  if (mb.dbgflag) stream->debug = T;
  if (mb.secflag) stream->secure = T;
  mb.trysslflag = stream->tryssl = (mb.trysslflag || stream->tryssl) ? T : NIL;
  stream->local =		/* instantiate localdata */
    (void *) memset (fs_get (sizeof (POP3LOCAL)),0,sizeof (POP3LOCAL));
  stream->sequence++;		/* bump sequence number */
  stream->perm_deleted = T;	/* deleted is only valid flag */

  if ((LOCAL->netstream =	/* try to open connection */
       net_open (&mb,NIL,pop3_port ? pop3_port : POP3TCPPORT,
		 (NETDRIVER *) mail_parameters (NIL,GET_SSLDRIVER,NIL),
		 "*pop3s",pop3_sslport ? pop3_sslport : POP3SSLPORT)) &&
      pop3_reply (stream)) {
    mm_log (LOCAL->reply,NIL);	/* give greeting */
    if (!pop3_auth (stream,&mb,tmp,usr)) pop3_close (stream,NIL);
    else if (pop3_send (stream,"STAT",NIL)) {
      int silent = stream->silent;
      stream->silent = T;
      sprintf (tmp,"{%.200s:%lu/pop3",
	       (int) mail_parameters (NIL,GET_TRUSTDNS,NIL) ?
	       net_host (LOCAL->netstream) : mb.host,
	       net_port (LOCAL->netstream));
      if (mb.tlsflag) strcat (tmp,"/tls");
      if (mb.notlsflag) strcat (tmp,"/notls");
      if (mb.sslflag) strcat (tmp,"/ssl");
      if (mb.novalidate) strcat (tmp,"/novalidate-cert");
      if (LOCAL->loser = mb.loser) strcat (tmp,"/loser");
      if (stream->secure) strcat (tmp,"/secure");
      sprintf (tmp + strlen (tmp),"/user=\"%s\"}%s",usr,mb.mailbox);
      stream->inbox = T;	/* always INBOX */
      fs_give ((void **) &stream->mailbox);
      stream->mailbox = cpystr (tmp);
				/* notify upper level */
      mail_exists (stream,stream->uid_last = strtoul (LOCAL->reply,NIL,10));
      mail_recent (stream,stream->nmsgs);
				/* instantiate elt */
      for (i = 0; i < stream->nmsgs;) {
	elt = mail_elt (stream,++i);
	elt->valid = elt->recent = T;
	elt->private.uid = i;
      }

				/* trust LIST output if new server */
      if (!LOCAL->loser && LOCAL->cap.capa && pop3_send (stream,"LIST",NIL)) {
	while ((s = net_getline (LOCAL->netstream)) && (*s != '.')) {
	  if ((i = strtoul (s,&t,10)) && (i <= stream->nmsgs) &&
	      (j = strtoul (t,NIL,10))) mail_elt (stream,i)->rfc822_size = j;
	  fs_give ((void **) &s);
	}
				/* flush final dot */
	if (s) fs_give ((void **) &s);
	else {			/* lost connection */
	  mm_log ("POP3 connection broken while itemizing messages",ERROR);
	  pop3_close (stream,NIL);
	  return NIL;
	}
      }
      stream->silent = silent;	/* notify main program */
      mail_exists (stream,stream->nmsgs);
				/* notify if empty */
      if (!(stream->nmsgs || stream->silent)) mm_log ("Mailbox is empty",WARN);
    }
    else {			/* error in STAT */
      mm_log (LOCAL->reply,ERROR);
      pop3_close (stream,NIL);	/* too bad */
    }
  }
  else {			/* connection failed */
    if (LOCAL->reply) mm_log (LOCAL->reply,ERROR);
    pop3_close (stream,NIL);	/* failed, clean up */
  }
  return LOCAL ? stream : NIL;	/* if stream is alive, return to caller */
}

/* POP3 capabilities
 * Accepts: stream
 *	    authenticator flags
 * Returns: T on success, NIL on failure
 */

long pop3_capa (MAILSTREAM *stream,long flags)
{
  unsigned long i;
  char *s,*t,*args;
  if (LOCAL->cap.implementation)/* zap all old capabilities */
    fs_give ((void **) &LOCAL->cap.implementation);
  memset (&LOCAL->cap,0,sizeof (LOCAL->cap));
				/* get server capabilities */
  if (pop3_send (stream,"CAPA",NIL)) LOCAL->cap.capa = T;
  else {
    LOCAL->cap.user = T;	/* guess worst-case old server */
    return NIL;			/* no CAPA on this server */
  }
  while ((t = net_getline (LOCAL->netstream)) && (t[1] || (*t != '.'))) {
    if (stream->debug) mm_dlog (t);
				/* get optional capability arguments */
    if (args = strchr (t,' ')) *args++ = '\0';
    if (!compare_cstring (t,"STLS")) LOCAL->cap.stls = T;
    else if (!compare_cstring (t,"PIPELINING")) LOCAL->cap.pipelining = T;
    else if (!compare_cstring (t,"RESP-CODES")) LOCAL->cap.respcodes = T;
    else if (!compare_cstring (t,"TOP")) LOCAL->cap.top = T;
    else if (!compare_cstring (t,"UIDL")) LOCAL->cap.uidl = T;
    else if (!compare_cstring (t,"USER")) LOCAL->cap.user = T;
    else if (!compare_cstring (t,"IMPLEMENTATION") && args)
      LOCAL->cap.implementation = cpystr (args);
    else if (!compare_cstring (t,"EXPIRE") && args) {
      LOCAL->cap.expire = T;	/* note that it is present */
      if (s = strchr(args,' ')){/* separate time from possible USER */
	*s++ = '\0';
				/* in case they add something after USER */
	if ((strlen (s) > 4) && (s[4] == ' ')) s[4] = '\0';
      }
      LOCAL->cap.expire =	/* get expiration time */
	(!compare_cstring (args,"NEVER")) ? 65535 :
	  ((s && !compare_cstring (s,"USER")) ? -atoi (args) : atoi (args));
    }
    else if (!compare_cstring (t,"LOGIN-DELAY") && args) {
      LOCAL->cap.logindelay = T;/* note that it is present */
      if (s = strchr(args,' ')){/* separate time from possible USER */
	*s++ = '\0';
				/* in case they add something after USER */
	if ((strlen (s) > 4) && (s[4] == ' ')) s[4] = '\0';
      }
				/* get delay time */
      LOCAL->cap.delaysecs = (s && !compare_cstring (s,"USER")) ?
	-atoi (args) : atoi (args);
    }
    else if (!compare_cstring (t,"SASL") && args)
      for (args = strtok (args," "); args; args = strtok (NIL," "))
	if ((i = mail_lookup_auth_name (args,flags)) &&
	    (--i < MAXAUTHENTICATORS))
	  LOCAL->cap.sasl |= (1 << i);
    fs_give ((void **) &t);
  }
  if (t) {			/* flush end of text indicator */
    if (stream->debug) mm_dlog (t);
    fs_give ((void **) &t);
  }
  return LONGT;
}

/* POP3 authenticate
 * Accepts: stream to login
 *	    parsed network mailbox structure
 *	    scratch buffer of length MAILTMPLEN
 *	    place to return user name
 * Returns: T on success, NIL on failure
 */

long pop3_auth (MAILSTREAM *stream,NETMBX *mb,char *pwd,char *usr)
{
  unsigned long i,trial,auths = 0;
  char *t;
  AUTHENTICATOR *at;
  long ret = NIL;
  long flags = (stream->secure ? AU_SECURE : NIL) |
    (mb->authuser[0] ? AU_AUTHUSER : NIL);
  long capaok = pop3_capa (stream,flags);
  NETDRIVER *ssld = (NETDRIVER *) mail_parameters (NIL,GET_SSLDRIVER,NIL);
  sslstart_t stls = (sslstart_t) mail_parameters (NIL,GET_SSLSTART,NIL);
				/* server has TLS? */
  if (stls && LOCAL->cap.stls && !mb->sslflag && !mb->notlsflag &&
      pop3_send (stream,"STLS",NIL)) {
    mb->tlsflag = T;		/* TLS OK, get into TLS at this end */
    LOCAL->netstream->dtb = ssld;
    if (!(LOCAL->netstream->stream =
	  (*stls) (LOCAL->netstream->stream,mb->host,NET_TLSCLIENT |
		   (mb->novalidate ? NET_NOVALIDATECERT : NIL)))) {
				/* drat, drop this connection */
      if (LOCAL->netstream) net_close (LOCAL->netstream);
      LOCAL->netstream= NIL;
      return NIL;		/* TLS negotiation failed */
    }
    pop3_capa (stream,flags);	/* get capabilities now that TLS in effect */
  }
  else if (mb->tlsflag) {	/* user specified /tls but can't do it */
    mm_log ("Unable to negotiate TLS with this server",ERROR);
    return NIL;
  }
  				/* get authenticators from capabilities */
  if (capaok) auths = LOCAL->cap.sasl;
				/* get list of authenticators */
  else if (pop3_send (stream,"AUTH",NIL)) {
    while ((t = net_getline (LOCAL->netstream)) && (t[1] || (*t != '.'))) {
      if (stream->debug) mm_dlog (t);
      if ((i = mail_lookup_auth_name (t,flags)) && (--i < MAXAUTHENTICATORS))
	auths |= (1 << i);
      fs_give ((void **) &t);
    }
    if (t) {			/* flush end of text indicator */
      if (stream->debug) mm_dlog (t);
      fs_give ((void **) &t);
    }
  }
				/* disable LOGIN if PLAIN also advertised */
  if ((i = mail_lookup_auth_name ("PLAIN",NIL)) && (--i < MAXAUTHENTICATORS) &&
      (auths & (1 << i)) &&
      (i = mail_lookup_auth_name ("LOGIN",NIL)) && (--i < MAXAUTHENTICATORS))
    auths &= ~(1 << i);

  if (auths) {			/* got any authenticators? */
    if ((int) mail_parameters (NIL,GET_TRUSTDNS,NIL)) {
				/* remote name for authentication */
      strncpy (mb->host,(int) mail_parameters (NIL,GET_SASLUSESPTRNAME,NIL) ?
	       net_remotehost (LOCAL->netstream) : net_host (LOCAL->netstream),
	       NETMAXHOST-1);
      mb->host[NETMAXHOST-1] = '\0';
    }
    for (t = NIL, LOCAL->saslcancel = NIL; !ret && LOCAL->netstream && auths &&
	 (at = mail_lookup_auth (find_rightmost_bit (&auths)+1)); ) {
      if (t) {			/* previous authenticator failed? */
	sprintf (pwd,"Retrying using %.80s authentication after %.80s",
		 at->name,t);
	mm_log (pwd,NIL);
	fs_give ((void **) &t);
      }
      trial = 0;		/* initial trial count */
      do {
	if (t) {
	  sprintf (pwd,"Retrying %s authentication after %.80s",at->name,t);
	  mm_log (pwd,WARN);
	  fs_give ((void **) &t);
	}
	LOCAL->saslcancel = NIL;
	if (pop3_send (stream,"AUTH",at->name)) {
				/* hide client authentication responses */
	  if (!(at->flags & AU_SECURE)) LOCAL->sensitive = T;
	  if ((*at->client) (pop3_challenge,pop3_response,"pop",mb,stream,
			     &trial,usr) && LOCAL->response) {
	    if (*LOCAL->response == '+') ret = LONGT;
				/* if main program requested cancellation */
	    else if (!trial) mm_log ("POP3 Authentication cancelled",ERROR);
	  }
	  LOCAL->sensitive=NIL;	/* unhide */
	}
				/* remember response if error and no cancel */
	if (!ret && trial) t = cpystr (LOCAL->reply);
      } while (!ret && trial && (trial < pop3_maxlogintrials) &&
	       LOCAL->netstream);
    }
    if (t) {			/* previous authenticator failed? */
      if (!LOCAL->saslcancel) {	/* don't do this if a cancel */
	sprintf (pwd,"Can not authenticate to POP3 server: %.80s",t);
	mm_log (pwd,ERROR);
      }
      fs_give ((void **) &t);
    }
  }

  else if (stream->secure)
    mm_log ("Can't do secure authentication with this server",ERROR);
  else if (mb->authuser[0])
    mm_log ("Can't do /authuser with this server",ERROR);
  else if (!LOCAL->cap.user) mm_log ("Can't login to this server",ERROR);
  else {			/* traditional login */
    trial = 0;			/* initial trial count */
    do {
      pwd[0] = 0;		/* prompt user for password */
      mm_login (mb,usr,pwd,trial++);
      if (pwd[0]) {		/* send login sequence if have password */
	if (pop3_send (stream,"USER",usr)) {
	  LOCAL->sensitive = T;	/* hide this command */
	  if (pop3_send (stream,"PASS",pwd)) ret = LONGT;
	  LOCAL->sensitive=NIL;	/* unhide */
	}
	if (!ret) {		/* failure */
	  mm_log (LOCAL->reply,WARN);
	  if (trial == pop3_maxlogintrials)
	    mm_log ("Too many login failures",ERROR);
	}
      }
				/* user refused to give password */
      else mm_log ("Login aborted",ERROR);
    } while (!ret && pwd[0] && (trial < pop3_maxlogintrials) &&
	     LOCAL->netstream);
  }
  memset (pwd,0,MAILTMPLEN);	/* erase password */
				/* get capabilities if logged in */
  if (ret && capaok) pop3_capa (stream,flags);
  return ret;
}

/* Get challenge to authenticator in binary
 * Accepts: stream
 *	    pointer to returned size
 * Returns: challenge or NIL if not challenge
 */

void *pop3_challenge (void *s,unsigned long *len)
{
  char tmp[MAILTMPLEN];
  void *ret = NIL;
  MAILSTREAM *stream = (MAILSTREAM *) s;
  if (stream && LOCAL->response &&
      (*LOCAL->response == '+') && (LOCAL->response[1] == ' ') &&
      !(ret = rfc822_base64 ((unsigned char *) LOCAL->reply,
			     strlen (LOCAL->reply),len))) {
    sprintf (tmp,"POP3 SERVER BUG (invalid challenge): %.80s",LOCAL->reply);
    mm_log (tmp,ERROR);
  }
  return ret;
}


/* Send authenticator response in BASE64
 * Accepts: MAIL stream
 *	    string to send
 *	    length of string
 * Returns: T if successful, else NIL
 */

long pop3_response (void *s,char *response,unsigned long size)
{
  MAILSTREAM *stream = (MAILSTREAM *) s;
  unsigned long i,j,ret;
  char *t,*u;
  if (response) {		/* make CRLFless BASE64 string */
    if (size) {
      for (t = (char *) rfc822_binary ((void *) response,size,&i),u = t,j = 0;
	   j < i; j++) if (t[j] > ' ') *u++ = t[j];
      *u = '\0';		/* tie off string for mm_dlog() */
      if (stream->debug) mail_dlog (t,LOCAL->sensitive);
				/* append CRLF */
      *u++ = '\015'; *u++ = '\012'; *u = '\0';
      ret = net_sout (LOCAL->netstream,t,u - t);
      fs_give ((void **) &t);
    }
    else ret = net_sout (LOCAL->netstream,"\015\012",2);
  }
  else {			/* abort requested */
    ret = net_sout (LOCAL->netstream,"*\015\012",3);
    LOCAL->saslcancel = T;	/* mark protocol-requested SASL cancel */
  }
  pop3_reply (stream);		/* get response */
  return ret;
}

/* POP3 mail close
 * Accepts: MAIL stream
 *	    option flags
 */

void pop3_close (MAILSTREAM *stream,long options)
{
  int silent = stream->silent;
  if (LOCAL) {			/* only if a file is open */
    if (LOCAL->netstream) {	/* close POP3 connection */
      stream->silent = T;
      if (options & CL_EXPUNGE) pop3_expunge (stream);
      stream->silent = silent;
      pop3_send (stream,"QUIT",NIL);
      mm_notify (stream,LOCAL->reply,BYE);
    }
				/* close POP3 connection */
    if (LOCAL->netstream) net_close (LOCAL->netstream);
				/* clean up */
    if (LOCAL->cap.implementation)
      fs_give ((void **) &LOCAL->cap.implementation);
    if (LOCAL->txt) fclose (LOCAL->txt);
    LOCAL->txt = NIL;
    if (LOCAL->response) fs_give ((void **) &LOCAL->response);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
  }
}

/* POP3 mail fetch fast information
 * Accepts: MAIL stream
 *	    sequence
 *	    option flags
 * This is ugly and slow
 */

void pop3_fetchfast (MAILSTREAM *stream,char *sequence,long flags)
{
  unsigned long i;
  MESSAGECACHE *elt;
				/* get sequence */
  if (stream && LOCAL && ((flags & FT_UID) ?
			  mail_uid_sequence (stream,sequence) :
			  mail_sequence (stream,sequence)))
    for (i = 1; i <= stream->nmsgs; i++)
      if ((elt = mail_elt (stream,i))->sequence &&
	  !(elt->day && elt->rfc822_size)) {
	ENVELOPE **env = NIL;
	ENVELOPE *e = NIL;
	if (!stream->scache) env = &elt->private.msg.env;
	else if (stream->msgno == i) env = &stream->env;
	else env = &e;
	if (!*env || !elt->rfc822_size) {
	  STRING bs;
	  unsigned long hs;
	  char *ht = (*stream->dtb->header) (stream,i,&hs,NIL);
				/* need to make an envelope? */
	  if (!*env) rfc822_parse_msg (env,NIL,ht,hs,NIL,BADHOST,
				       stream->dtb->flags);
				/* need message size too, ugh */
	  if (!elt->rfc822_size) {
	    (*stream->dtb->text) (stream,i,&bs,FT_PEEK);
	    elt->rfc822_size = hs + SIZE (&bs) - GETPOS (&bs);
	  }
	}
				/* if need date, have date in envelope? */
	if (!elt->day && *env && (*env)->date)
	  mail_parse_date (elt,(*env)->date);
				/* sigh, fill in bogus default */
	if (!elt->day) elt->day = elt->month = 1;
	mail_free_envelope (&e);
      }
}

/* POP3 fetch header as text
 * Accepts: mail stream
 *	    message number
 *	    pointer to return size
 *	    flags
 * Returns: header text
 */

char *pop3_header (MAILSTREAM *stream,unsigned long msgno,unsigned long *size,
		   long flags)
{
  unsigned long i;
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  FILE *f = NIL;
  *size = 0;			/* initially no header size */
  if ((flags & FT_UID) && !(msgno = mail_msgno (stream,msgno))) return "";
				/* have header text already? */
  if (!(elt = mail_elt (stream,msgno))->private.msg.header.text.data) {
				/* if have CAPA and TOP, assume good TOP */
    if (!LOCAL->loser && LOCAL->cap.top) {
      sprintf (tmp,"TOP %lu 0",mail_uid (stream,msgno));
      if (pop3_send (stream,tmp,NIL))
	f = netmsg_slurp (LOCAL->netstream,&i,
			  &elt->private.msg.header.text.size);
    }
				/* otherwise load the cache with the message */
    else if (elt->private.msg.header.text.size = pop3_cache (stream,elt))
      f = LOCAL->txt;
    if (f) {			/* got it, make sure at start of file */
      fseek (f,(unsigned long) 0,L_SET);
				/* read header from the cache */
      fread (elt->private.msg.header.text.data = (unsigned char *)
	     fs_get ((size_t) elt->private.msg.header.text.size + 1),
	     (size_t) 1,(size_t) elt->private.msg.header.text.size,f);
				/* tie off header text */
      elt->private.msg.header.text.data[elt->private.msg.header.text.size] =
	'\0';
				/* close if not the cache */
      if (f != LOCAL->txt) fclose (f);
    }
  }
				/* return size of text */
  if (size) *size = elt->private.msg.header.text.size;
  return elt->private.msg.header.text.data ?
    (char *) elt->private.msg.header.text.data : "";
}

/* POP3 fetch body
 * Accepts: mail stream
 *	    message number
 *	    pointer to stringstruct to initialize
 *	    flags
 * Returns: T if successful, else NIL
 */

long pop3_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  MESSAGECACHE *elt;
  INIT (bs,mail_string,(void *) "",0);
  if ((flags & FT_UID) && !(msgno = mail_msgno (stream,msgno))) return NIL;
  elt = mail_elt (stream,msgno);
  pop3_cache (stream,elt);	/* make sure cache loaded */
  if (!LOCAL->txt) return NIL;	/* error if don't have a file */
  if (!(flags & FT_PEEK)) {	/* mark seen if needed */
    elt->seen = T;
    mm_flags (stream,elt->msgno);
  }
  INIT (bs,file_string,(void *) LOCAL->txt,elt->rfc822_size);
  SETPOS (bs,LOCAL->hdrsize);	/* skip past header */
  return T;
}

/* POP3 cache message
 * Accepts: mail stream
 *	    message number
 * Returns: header size
 */

unsigned long pop3_cache (MAILSTREAM *stream,MESSAGECACHE *elt)
{
				/* already cached? */
  if (LOCAL->cached != mail_uid (stream,elt->msgno)) {
				/* no, close current file */
    if (LOCAL->txt) fclose (LOCAL->txt);
    LOCAL->txt = NIL;
    LOCAL->cached = LOCAL->hdrsize = 0;
    if (pop3_send_num (stream,"RETR",elt->msgno) &&
	(LOCAL->txt = netmsg_slurp (LOCAL->netstream,&elt->rfc822_size,
				    &LOCAL->hdrsize)))
				/* set as current message number */
      LOCAL->cached = mail_uid (stream,elt->msgno);
    else elt->deleted = T;
  }
  return LOCAL->hdrsize;
}

/* POP3 mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long pop3_ping (MAILSTREAM *stream)
{
  return pop3_send (stream,"NOOP",NIL);
}


/* POP3 mail check mailbox
 * Accepts: MAIL stream
 */

void pop3_check (MAILSTREAM *stream)
{
  if (pop3_ping (stream)) mm_log ("Check completed",NIL);
}


/* POP3 mail expunge mailbox
 * Accepts: MAIL stream
 */

void pop3_expunge (MAILSTREAM *stream)
{
  char tmp[MAILTMPLEN];
  unsigned long i = 1,n = 0;
  while (i <= stream->nmsgs) {
    if (mail_elt (stream,i)->deleted && pop3_send_num (stream,"DELE",i)) {
				/* expunging currently cached message? */
      if (LOCAL->cached == mail_uid (stream,i)) {
				/* yes, close current file */
	if (LOCAL->txt) fclose (LOCAL->txt);
	LOCAL->txt = NIL;
	LOCAL->cached = LOCAL->hdrsize = 0;
      }
      mail_expunged (stream,i);
      n++;
    }
    else i++;			/* try next message */
  }
  if (!stream->silent) {	/* only if not silent */
    if (n) {			/* did we expunge anything? */
      sprintf (tmp,"Expunged %lu messages",n);
      mm_log (tmp,(long) NIL);
    }
    else mm_log ("No messages deleted, so no update needed",(long) NIL);
  }
}

/* POP3 mail copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    option flags
 * Returns: T if copy successful, else NIL
 */

long pop3_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  if (pc) return (*pc) (stream,sequence,mailbox,options);
  mm_log ("Copy not valid for POP3",ERROR);
  return NIL;
}


/* POP3 mail append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    append callback
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

long pop3_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  mm_log ("Append not valid for POP3",ERROR);
  return NIL;
}

/* Internal routines */


/* Post Office Protocol 3 send command with number argument
 * Accepts: MAIL stream
 *	    command
 *	    number
 * Returns: T if successful, NIL if failure
 */

long pop3_send_num (MAILSTREAM *stream,char *command,unsigned long n)
{
  char tmp[MAILTMPLEN];
  sprintf (tmp,"%lu",mail_uid (stream,n));
  return pop3_send (stream,command,tmp);
}


/* Post Office Protocol 3 send command
 * Accepts: MAIL stream
 *	    command
 *	    command argument
 * Returns: T if successful, NIL if failure
 */

long pop3_send (MAILSTREAM *stream,char *command,char *args)
{
  long ret;
  char *s = (char *) fs_get (strlen (command) + (args ? strlen (args) + 1: 0)
			     + 3);
  mail_lock (stream);		/* lock up the stream */
  if (!LOCAL->netstream) ret = pop3_fake (stream,"POP3 connection lost");
  else {			/* build the complete command */
    if (args) sprintf (s,"%s %s",command,args);
    else strcpy (s,command);
    if (stream->debug) mail_dlog (s,LOCAL->sensitive);
    strcat (s,"\015\012");
				/* send the command */
    ret = net_soutr (LOCAL->netstream,s) ? pop3_reply (stream) :
      pop3_fake (stream,"POP3 connection broken in command");
  }
  fs_give ((void **) &s);
  mail_unlock (stream);		/* unlock stream */
  return ret;
}

/* Post Office Protocol 3 get reply
 * Accepts: MAIL stream
 * Returns: T if success reply, NIL if error reply
 */

long pop3_reply (MAILSTREAM *stream)
{
  char *s;
				/* flush old reply */
  if (LOCAL->response) fs_give ((void **) &LOCAL->response);
  				/* get reply */
  if (!(LOCAL->response = net_getline (LOCAL->netstream)))
    return pop3_fake (stream,"POP3 connection broken in response");
  if (stream->debug) mm_dlog (LOCAL->response);
  LOCAL->reply = (s = strchr (LOCAL->response,' ')) ? s + 1 : LOCAL->response;
				/* return success or failure */
  return (*LOCAL->response =='+') ? T : NIL;
}


/* Post Office Protocol 3 set fake error
 * Accepts: MAIL stream
 *	    error text
 * Returns: NIL, always
 */

long pop3_fake (MAILSTREAM *stream,char *text)
{
  mm_notify (stream,text,BYE);	/* send bye alert */
  if (LOCAL->netstream) net_close (LOCAL->netstream);
  LOCAL->netstream = NIL;	/* farewell, dear TCP stream */
				/* flush any old reply */
  if (LOCAL->response) fs_give ((void **) &LOCAL->response);
  LOCAL->reply = text;		/* set up pseudo-reply string */
  return NIL;			/* return error code */
}
