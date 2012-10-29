/*
 * Program:	Network News Transfer Protocol (NNTP) routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	10 February 1992
 * Last Edited:	5 March 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


#include <ctype.h>
#include <stdio.h>
#include "mail.h"
#include "osdep.h"
#include "nntp.h"
#include "rfc822.h"
#include "misc.h"
#include "newsrc.h"
#include "netmsg.h"
#include "flstring.h"
#include "utf8.h"

/* Constants */

#define NNTPSSLPORT (long) 563	/* assigned SSL TCP contact port */
#define NNTPGREET (long) 200	/* NNTP successful greeting */
				/* NNTP successful greeting w/o posting priv */
#define NNTPGREETNOPOST (long) 201
#define NNTPEXTOK (long) 202	/* NNTP extensions OK */
#define NNTPGOK (long) 211	/* NNTP group selection OK */
#define NNTPGLIST (long) 215	/* NNTP group list being returned */
#define NNTPARTICLE (long) 220	/* NNTP article file */
#define NNTPHEAD (long) 221	/* NNTP header text */
#define NNTPBODY (long) 222	/* NNTP body text */
#define NNTPOVER (long) 224	/* NNTP overview text */
#define NNTPOK (long) 240	/* NNTP OK code */
#define NNTPAUTHED (long) 281	/* NNTP successful authentication */
				/* NNTP successful authentication with data */
#define NNTPAUTHEDDATA (long) 282
#define NNTPREADY (long) 340	/* NNTP ready for data */
#define NNTPWANTAUTH2 (long) 380/* NNTP authentication needed (old) */
#define NNTPWANTPASS (long) 381	/* NNTP password needed */
#define NNTPTLSSTART (long) 382	/* NNTP continue with TLS negotiation */
#define NNTPCHALLENGE (long) 383/* NNTP challenge, want response */
#define NNTPSOFTFATAL (long) 400/* NNTP soft fatal code */
#define NNTPWANTAUTH (long) 480	/* NNTP authentication needed */
#define NNTPBADCMD (long) 500	/* NNTP unrecognized command */
#define IDLETIMEOUT (long) 3	/* defined in NNTPEXT WG base draft */


/* NNTP I/O stream local data */
	
typedef struct nntp_local {
  SENDSTREAM *nntpstream;	/* NNTP stream for I/O */
  unsigned int dirty : 1;	/* disk copy of .newsrc needs updating */
  unsigned int tlsflag : 1;	/* TLS session */
  unsigned int notlsflag : 1;	/* TLS not used in session */
  unsigned int sslflag : 1;	/* SSL session */
  unsigned int novalidate : 1;	/* certificate not validated */
  unsigned int xover : 1;	/* supports XOVER */
  unsigned int xhdr : 1;	/* supports XHDR */
  char *name;			/* remote newsgroup name */
  char *user;			/* mailbox user */
  char *newsrc;			/* newsrc file */
  char *over_fmt;		/* overview format */
  unsigned long msgno;		/* current text message number */
  FILE *txt;			/* current text */
  unsigned long txtsize;	/* current text size */
} NNTPLOCAL;


/* Convenient access to local data */

#define LOCAL ((NNTPLOCAL *) stream->local)


/* Convenient access to protocol-specific data */

#define NNTP stream->protocol.nntp


/* Convenient access to extensions */

#define EXTENSION LOCAL->nntpstream->protocol.nntp.ext

/* Function prototypes */

DRIVER *nntp_valid (char *name);
DRIVER *nntp_isvalid (char *name,char *mbx);
void *nntp_parameters (long function,void *value);
void nntp_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void nntp_list (MAILSTREAM *stream,char *ref,char *pat);
void nntp_lsub (MAILSTREAM *stream,char *ref,char *pat);
long nntp_canonicalize (char *ref,char *pat,char *pattern,char *wildmat);
long nntp_subscribe (MAILSTREAM *stream,char *mailbox);
long nntp_unsubscribe (MAILSTREAM *stream,char *mailbox);
long nntp_create (MAILSTREAM *stream,char *mailbox);
long nntp_delete (MAILSTREAM *stream,char *mailbox);
long nntp_rename (MAILSTREAM *stream,char *old,char *newname);
long nntp_status (MAILSTREAM *stream,char *mbx,long flags);
long nntp_getmap (MAILSTREAM *stream,char *name,
		  unsigned long first,unsigned long last,
		  unsigned long rnmsgs,unsigned long nmsgs,char *tmp);
MAILSTREAM *nntp_mopen (MAILSTREAM *stream);
void nntp_mclose (MAILSTREAM *stream,long options);
void nntp_fetchfast (MAILSTREAM *stream,char *sequence,long flags);
void nntp_flags (MAILSTREAM *stream,char *sequence,long flags);
long nntp_overview (MAILSTREAM *stream,overview_t ofn);
long nntp_parse_overview (OVERVIEW *ov,char *text,MESSAGECACHE *elt);
long nntp_over (MAILSTREAM *stream,char *sequence);
char *nntp_header (MAILSTREAM *stream,unsigned long msgno,unsigned long *size,
		   long flags);
long nntp_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags);
FILE *nntp_article (MAILSTREAM *stream,char *msgid,unsigned long *size,
		    unsigned long *hsiz);
void nntp_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt);
long nntp_search (MAILSTREAM *stream,char *charset,SEARCHPGM *pgm,long flags);
long nntp_search_msg (MAILSTREAM *stream,unsigned long msgno,SEARCHPGM *pgm,
		      OVERVIEW *ov);
unsigned long *nntp_sort (MAILSTREAM *stream,char *charset,SEARCHPGM *spg,
			  SORTPGM *pgm,long flags);
SORTCACHE **nntp_sort_loadcache (MAILSTREAM *stream,SORTPGM *pgm,
				 unsigned long start,unsigned long last,
				 long flags);
THREADNODE *nntp_thread (MAILSTREAM *stream,char *type,char *charset,
			 SEARCHPGM *spg,long flags);
long nntp_ping (MAILSTREAM *stream);
void nntp_check (MAILSTREAM *stream);
void nntp_expunge (MAILSTREAM *stream);
long nntp_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options);
long nntp_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);

long nntp_extensions (SENDSTREAM *stream,long flags);
long nntp_send (SENDSTREAM *stream,char *command,char *args);
long nntp_send_work (SENDSTREAM *stream,char *command,char *args);
long nntp_send_auth (SENDSTREAM *stream,long flags);
long nntp_send_auth_work (SENDSTREAM *stream,NETMBX *mb,char *pwd,long flags);
void *nntp_challenge (void *s,unsigned long *len);
long nntp_response (void *s,char *response,unsigned long size);
long nntp_reply (SENDSTREAM *stream);
long nntp_fake (SENDSTREAM *stream,char *text);
long nntp_soutr (void *stream,char *s);

/* Driver dispatch used by MAIL */

DRIVER nntpdriver = {
  "nntp",			/* driver name */
				/* driver flags */
#ifdef INADEQUATE_MEMORY
  DR_LOWMEM |
#endif
  DR_NEWS|DR_READONLY|DR_NOFAST|DR_NAMESPACE|DR_CRLF|DR_RECYCLE|DR_XPOINT |
    DR_NOINTDATE|DR_NONEWMAIL|DR_HALFOPEN,
  (DRIVER *) NIL,		/* next driver */
  nntp_valid,			/* mailbox is valid for us */
  nntp_parameters,		/* manipulate parameters */
  nntp_scan,			/* scan mailboxes */
  nntp_list,			/* find mailboxes */
  nntp_lsub,			/* find subscribed mailboxes */
  nntp_subscribe,		/* subscribe to mailbox */
  nntp_unsubscribe,		/* unsubscribe from mailbox */
  nntp_create,			/* create mailbox */
  nntp_delete,			/* delete mailbox */
  nntp_rename,			/* rename mailbox */
  nntp_status,			/* status of mailbox */
  nntp_mopen,			/* open mailbox */
  nntp_mclose,			/* close mailbox */
  nntp_fetchfast,		/* fetch message "fast" attributes */
  nntp_flags,			/* fetch message flags */
  nntp_overview,		/* fetch overview */
  NIL,				/* fetch message structure */
  nntp_header,			/* fetch message header */
  nntp_text,			/* fetch message text */
  NIL,				/* fetch message */
  NIL,				/* unique identifier */
  NIL,				/* message number from UID */
  NIL,				/* modify flags */
  nntp_flagmsg,			/* per-message modify flags */
  nntp_search,			/* search for message based on criteria */
  nntp_sort,			/* sort messages */
  nntp_thread,			/* thread messages */
  nntp_ping,			/* ping mailbox to see if still alive */
  nntp_check,			/* check for new messages */
  nntp_expunge,			/* expunge deleted messages */
  nntp_copy,			/* copy messages to another mailbox */
  nntp_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM nntpproto = {&nntpdriver};


				/* driver parameters */
static unsigned long nntp_maxlogintrials = MAXLOGINTRIALS;
static long nntp_port = 0;
static long nntp_sslport = 0;
static unsigned long nntp_range = 0;
static long nntp_hidepath = 0;

/* NNTP validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *nntp_valid (char *name)
{
  char tmp[MAILTMPLEN];
  return nntp_isvalid (name,tmp);
}


/* NNTP validate mailbox work routine
 * Accepts: mailbox name
 *	    buffer for returned mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *nntp_isvalid (char *name,char *mbx)
{
  NETMBX mb;
  if (!mail_valid_net_parse (name,&mb) || strcmp (mb.service,nntpdriver.name)||
      mb.anoflag) return NIL;
  if (mb.mailbox[0] != '#') strcpy (mbx,mb.mailbox);
			/* namespace format name */
  else if ((mb.mailbox[1] == 'n') && (mb.mailbox[2] == 'e') &&
	   (mb.mailbox[3] == 'w') && (mb.mailbox[4] == 's') &&
	   (mb.mailbox[5] == '.')) strcpy (mbx,mb.mailbox+6);
  else return NIL;		/* bogus name */
  return &nntpdriver;
}

/* News manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *nntp_parameters (long function,void *value)
{
  switch ((int) function) {
  case SET_MAXLOGINTRIALS:
    nntp_maxlogintrials = (unsigned long) value;
    break;
  case GET_MAXLOGINTRIALS:
    value = (void *) nntp_maxlogintrials;
    break;
  case SET_NNTPPORT:
    nntp_port = (long) value;
    break;
  case GET_NNTPPORT:
    value = (void *) nntp_port;
    break;
  case SET_SSLNNTPPORT:
    nntp_sslport = (long) value;
    break;
  case GET_SSLNNTPPORT:
    value = (void *) nntp_sslport;
    break;
  case SET_NNTPRANGE:
    nntp_range = (unsigned long) value;
    break;
  case GET_NNTPRANGE:
    value = (void *) nntp_range;
    break;
  case SET_NNTPHIDEPATH:
    nntp_hidepath = (long) value;
    break;
  case GET_NNTPHIDEPATH:
    value = (void *) nntp_hidepath;
    break;
  case GET_NEWSRC:
    if (value)
      value = (void *) ((NNTPLOCAL *) ((MAILSTREAM *) value)->local)->newsrc;
    break;
  case GET_IDLETIMEOUT:
    value = (void *) IDLETIMEOUT;
    break;
  case ENABLE_DEBUG:
    if (value)
      ((NNTPLOCAL *) ((MAILSTREAM *) value)->local)->nntpstream->debug = T;
    break;
  case DISABLE_DEBUG:
    if (value)
      ((NNTPLOCAL *) ((MAILSTREAM *) value)->local)->nntpstream->debug = NIL;
    break;
  default:
    value = NIL;		/* error case */
    break;
  }
  return value;
}

/* NNTP mail scan mailboxes for string
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void nntp_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  char tmp[MAILTMPLEN];
  if (nntp_canonicalize (ref,pat,tmp,NIL))
    mm_log ("Scan not valid for NNTP mailboxes",ERROR);
}


/* NNTP list newsgroups
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void nntp_list (MAILSTREAM *stream,char *ref,char *pat)
{
  MAILSTREAM *st = stream;
  char *s,*t,*lcl,pattern[MAILTMPLEN],name[MAILTMPLEN],wildmat[MAILTMPLEN];
  int showuppers = pat[strlen (pat) - 1] == '%';
  if (!pat || !*pat) {
    if (nntp_canonicalize (ref,"*",pattern,NIL)) {
				/* tie off name at root */
      if ((s = strchr (pattern,'}')) && (s = strchr (s+1,'.'))) *++s = '\0';
      else pattern[0] = '\0';
      mm_list (stream,'.',pattern,NIL);
    }
  }
				/* ask server for open newsgroups */
  else if (nntp_canonicalize (ref,pat,pattern,wildmat) &&
	   ((stream && LOCAL && LOCAL->nntpstream) ||
	    (stream = mail_open (NIL,pattern,OP_HALFOPEN|OP_SILENT))) &&
	   ((nntp_send (LOCAL->nntpstream,"LIST ACTIVE",
			wildmat[0] ? wildmat : NIL) == NNTPGLIST) ||
	    (nntp_send (LOCAL->nntpstream,"LIST",NIL) == NNTPGLIST))) {
				/* namespace format name? */
    if (*(lcl = strchr (strcpy (name,pattern),'}') + 1) == '#') lcl += 6;
				/* process data until we see final dot */
    while (s = net_getline (LOCAL->nntpstream->netstream)) {
      if ((*s == '.') && !s[1]){/* end of text */
	fs_give ((void **) &s);
	break;
      }
      if (t = strchr (s,' ')) {	/* tie off after newsgroup name */
	*t = '\0';
	strcpy (lcl,s);		/* make full form of name */
				/* report if match */
	if (pmatch_full (name,pattern,'.')) mm_list (stream,'.',name,NIL);
	else while (showuppers && (t = strrchr (lcl,'.'))) {
	  *t = '\0';		/* tie off the name */
	  if (pmatch_full (name,pattern,'.'))
	    mm_list (stream,'.',name,LATT_NOSELECT);
	}
      }
      fs_give ((void **) &s);	/* clean up */
    }
    if (stream != st) mail_close (stream);
  }
}

/* NNTP list subscribed newsgroups
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void nntp_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  void *sdb = NIL;
  char *s,mbx[MAILTMPLEN];
				/* return data from newsrc */
  if (nntp_canonicalize (ref,pat,mbx,NIL)) newsrc_lsub (stream,mbx);
  if (*pat == '{') {		/* if remote pattern, must be NNTP */
    if (!nntp_valid (pat)) return;
    ref = NIL;			/* good NNTP pattern, punt reference */
  }
				/* if remote reference, must be valid NNTP */
  if (ref && (*ref == '{') && !nntp_valid (ref)) return;
				/* kludgy application of reference */
  if (ref && *ref) sprintf (mbx,"%s%s",ref,pat);
  else strcpy (mbx,pat);

  if (s = sm_read (&sdb)) do if (nntp_valid (s) && pmatch (s,mbx))
    mm_lsub (stream,NIL,s,NIL);
  while (s = sm_read (&sdb));	/* until no more subscriptions */
}

/* NNTP canonicalize newsgroup name
 * Accepts: reference
 *	    pattern
 *	    returned single pattern
 *	    returned wildmat pattern
 * Returns: T on success, NIL on failure
 */

long nntp_canonicalize (char *ref,char *pat,char *pattern,char *wildmat)
{
  char *s;
  DRIVER *ret;
  if (ref && *ref) {		/* have a reference */
    if (!nntp_valid (ref)) return NIL;
    strcpy (pattern,ref);	/* copy reference to pattern */
				/* # overrides mailbox field in reference */
    if (*pat == '#') strcpy (strchr (pattern,'}') + 1,pat);
				/* pattern starts, reference ends, with . */
    else if ((*pat == '.') && (pattern[strlen (pattern) - 1] == '.'))
      strcat (pattern,pat + 1);	/* append, omitting one of the period */
    else strcat (pattern,pat);	/* anything else is just appended */
  }
  else strcpy (pattern,pat);	/* just have basic name */
  if ((ret = wildmat ?		/* if valid and wildmat */
       nntp_isvalid (pattern,wildmat) : nntp_valid (pattern)) && wildmat) {
				/* don't return wildmat if specials present */
    if (strpbrk (wildmat,",?![\\]")) wildmat[0] = '\0';
				/* replace all % with * */
    for (s = wildmat; s = strchr (s,'%'); *s = '*');
  }
  return ret ? LONGT : NIL;
}

/* NNTP subscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to add to subscription list
 * Returns: T on success, NIL on failure
 */

long nntp_subscribe (MAILSTREAM *stream,char *mailbox)
{
  char mbx[MAILTMPLEN];
  return nntp_isvalid (mailbox,mbx) ? newsrc_update (stream,mbx,':') : NIL;
}


/* NNTP unsubscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to delete from subscription list
 * Returns: T on success, NIL on failure
 */

long nntp_unsubscribe (MAILSTREAM *stream,char *mailbox)
{
  char mbx[MAILTMPLEN];
  return nntp_isvalid (mailbox,mbx) ? newsrc_update (stream,mbx,'!') : NIL;
}

/* NNTP create mailbox
 * Accepts: mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long nntp_create (MAILSTREAM *stream,char *mailbox)
{
  return NIL;			/* never valid for NNTP */
}


/* NNTP delete mailbox
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long nntp_delete (MAILSTREAM *stream,char *mailbox)
{
  return NIL;			/* never valid for NNTP */
}


/* NNTP rename mailbox
 * Accepts: mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long nntp_rename (MAILSTREAM *stream,char *old,char *newname)
{
  return NIL;			/* never valid for NNTP */
}

/* NNTP status
 * Accepts: mail stream
 *	    mailbox name
 *	    status flags
 * Returns: T on success, NIL on failure
 */

long nntp_status (MAILSTREAM *stream,char *mbx,long flags)
{
  MAILSTATUS status;
  NETMBX mb;
  unsigned long i,j,k,rnmsgs;
  long ret = NIL;
  char *s,*name,*state,tmp[MAILTMPLEN];
  char *old = (stream && !stream->halfopen) ? LOCAL->name : NIL;
  MAILSTREAM *tstream = NIL;
  if (!(mail_valid_net_parse (mbx,&mb) && !strcmp (mb.service,"nntp") &&
	*mb.mailbox &&
	((mb.mailbox[0] != '#') ||
	 ((mb.mailbox[1] == 'n') && (mb.mailbox[2] == 'e') &&
	  (mb.mailbox[3] == 'w') && (mb.mailbox[4] == 's') &&
	  (mb.mailbox[5] == '.'))))) {
    sprintf (tmp,"Invalid NNTP name %s",mbx);
    mm_log (tmp,ERROR);
    return NIL;
  }
				/* note mailbox name */
  name = (*mb.mailbox == '#') ? mb.mailbox+6 : mb.mailbox;
				/* stream to reuse? */
  if (!(stream && LOCAL->nntpstream &&
	mail_usable_network_stream (stream,mbx)) &&
      !(tstream = stream =
	mail_open (NIL,mbx,OP_HALFOPEN|OP_SILENT|
		   ((flags & SA_MULNEWSRC) ? OP_MULNEWSRC : NIL))))
    return NIL;			/* can't reuse or make a new one */

  if (nntp_send (LOCAL->nntpstream,"GROUP",name) == NNTPGOK) {
    status.flags = flags;	/* status validity flags */
    k = strtoul (LOCAL->nntpstream->reply + 4,&s,10);
    i = strtoul (s,&s,10);	/* first assigned UID */
				/* next UID to be assigned */
    status.uidnext = (j = strtoul (s,NIL,10)) + 1;
				/* maximum number of messages */
    rnmsgs = status.messages = (i | j) ? status.uidnext - i : 0;
    if (k > status.messages) {	/* check for absurdity */
      sprintf (tmp,"NNTP SERVER BUG (impossible message count): %lu > %lu",
	       k,status.messages);
      mm_log (tmp,WARN);
    }
				/* restrict article range if needed */
    if (nntp_range && (status.messages > nntp_range)) {
      i = status.uidnext - (status.messages = nntp_range);
      if (k > nntp_range) k = nntp_range;
    }
				/* initially zero */
    status.recent = status.unseen = 0;
    if (!status.messages);	/* empty case */
				/* use server guesstimate in simple case */
    else if (!(flags & (SA_RECENT | SA_UNSEEN))) status.messages = k;

				/* have newsrc state? */
    else if (state = newsrc_state (stream,name)) {
				/* yes, get the UID/sequence map */
      if (nntp_getmap (stream,name,i,status.uidnext - 1,rnmsgs,
		       status.messages,tmp)) {
				/* calculate true count */
	for (status.messages = 0;
	     (s = net_getline (LOCAL->nntpstream->netstream)) &&
	       strcmp (s,"."); ) {
				/* only count if in range */
	  if (((k = atol (s)) >= i) && (k < status.uidnext)) {
	    newsrc_check_uid (state,k,&status.recent,&status.unseen);
	    status.messages++;
	  }
	  fs_give ((void **) &s);
	}
	if (s) fs_give ((void **) &s);
      }
				/* assume c-client/NNTP map is entire range */
      else while (i < status.uidnext)
	newsrc_check_uid (state,i++,&status.recent,&status.unseen);
      fs_give ((void **) &state);
    }
				/* no .newsrc state, all messages new */
    else status.recent = status.unseen = status.messages;
				/* UID validity is a constant */
    status.uidvalidity = stream->uid_validity;
				/* pass status to main program */
    mm_status (stream,mbx,&status);
    ret = T;			/* succes */
  }
				/* flush temporary stream */
  if (tstream) mail_close (tstream);
				/* else reopen old newsgroup */
  else if (old && nntp_send (LOCAL->nntpstream,"GROUP",old) != NNTPGOK) {
    mm_log (LOCAL->nntpstream->reply,ERROR);
    stream->halfopen = T;	/* go halfopen */
  }
  return ret;			/* success */
}

/* NNTP get map
 * Accepts: stream
 *	    newsgroup name
 *	    first UID in map range
 *	    last UID in map range
 *	    reported total number of messages in newsgroup
 *	    calculated number of messages in range
 *	    temporary buffer
 * Returns: T on success, NIL on failure
 */

long nntp_getmap (MAILSTREAM *stream,char *name,
		  unsigned long first,unsigned long last,
		  unsigned long rnmsgs,unsigned long nmsgs,char *tmp)
{
  short trylistgroup = NIL;
  if (rnmsgs > (nmsgs * 8))	/* small subrange? */
    trylistgroup = T;		/* yes, can try LISTGROUP if [X]HDR fails */
  else switch ((int) nntp_send (LOCAL->nntpstream,"LISTGROUP",name)) {
  case NNTPGOK:			/* got data */
    return LONGT;
  default:			/* else give up if server claims LISTGROUP */
    if (EXTENSION.listgroup) return NIL;
  }
				/* build range */
  sprintf (tmp,"%lu-%lu",first,last);
  if (EXTENSION.hdr)		/* have HDR extension? */
    return (nntp_send (LOCAL->nntpstream,"HDR Date",tmp) == NNTPHEAD) ?
      LONGT : NIL;
  if (LOCAL->xhdr)		/* try the experimental extension then */
    switch ((int) nntp_send (LOCAL->nntpstream,"XHDR Date",tmp)) {
    case NNTPHEAD:		/* got an overview? */
      return LONGT;
    case NNTPBADCMD:		/* unknown command? */
      LOCAL->xhdr = NIL;	/* disable future XHDR attempts */
    }
  if (trylistgroup &&		/* no [X]HDR, maybe do LISTGROUP after all */
      (nntp_send (LOCAL->nntpstream,"LISTGROUP",name) == NNTPGOK))
    return LONGT;
  return NIL;
}

/* NNTP open
 * Accepts: stream to open
 * Returns: stream on success, NIL on failure
 */

MAILSTREAM *nntp_mopen (MAILSTREAM *stream)
{
  unsigned long i,j,k,nmsgs,rnmsgs;
  char *s,*mbx,tmp[MAILTMPLEN];
  FILE *f;
  NETMBX mb;
  char *newsrc = (char *) mail_parameters (NIL,GET_NEWSRC,NIL);
  newsrcquery_t nq = (newsrcquery_t) mail_parameters (NIL,GET_NEWSRCQUERY,NIL);
  SENDSTREAM *nstream = NIL;
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return &nntpproto;
  mail_valid_net_parse (stream->mailbox,&mb);
				/* note mailbox anme */
  mbx = (*mb.mailbox == '#') ? mb.mailbox+6 : mb.mailbox;
  if (LOCAL) {			/* recycle stream */
    nstream = LOCAL->nntpstream;/* remember NNTP protocol stream */
    sprintf (tmp,"Reusing connection to %s",net_host (nstream->netstream));
    if (!stream->silent) mm_log (tmp,(long) NIL);
    if (stream->rdonly) mb.readonlyflag = T;
    if (LOCAL->tlsflag) mb.tlsflag = T;
    if (LOCAL->notlsflag) mb.notlsflag = T;
    if (LOCAL->sslflag) mb.sslflag = T;
    if (LOCAL->novalidate) mb.novalidate = T;
    if (LOCAL->nntpstream->loser) mb.loser = T;
    if (stream->secure) mb.secflag = T;
    LOCAL->nntpstream = NIL;	/* keep nntp_mclose() from punting it */
    nntp_mclose (stream,NIL);	/* do close action */
    stream->dtb = &nntpdriver;	/* reattach this driver */
  }
				/* copy flags */
  if (mb.dbgflag) stream->debug = T;
  if (mb.readonlyflag) stream->rdonly = T;
  if (mb.secflag) stream->secure = T;
  mb.trysslflag = stream->tryssl = (mb.trysslflag || stream->tryssl) ? T : NIL;
  if (!nstream) {		/* open NNTP now if not already open */
    char *hostlist[2];
    hostlist[0] = strcpy (tmp,mb.host);
    if (mb.port || nntp_port)
      sprintf (tmp + strlen (tmp),":%lu",mb.port ? mb.port : nntp_port);
    if (mb.tlsflag) strcat (tmp,"/tls");
    if (mb.notlsflag) strcat (tmp,"/notls");
    if (mb.sslflag) strcat (tmp,"/ssl");
    if (mb.novalidate) strcat (tmp,"/novalidate-cert");
    if (mb.loser) strcat (tmp,"/loser");
    if (mb.secflag) strcat (tmp,"/secure");
    if (mb.user[0]) sprintf (tmp + strlen (tmp),"/user=\"%s\"",mb.user);
    hostlist[1] = NIL;
    if (!(nstream = nntp_open (hostlist,NOP_READONLY |
			       (stream->debug ? NOP_DEBUG : NIL)))) return NIL;
  }

				/* always zero messages if halfopen */
  if (stream->halfopen) i = j = k = rnmsgs = nmsgs = 0;
				/* otherwise open the newsgroup */
  else if (nntp_send (nstream,"GROUP",mbx) == NNTPGOK) {
    k = strtoul (nstream->reply + 4,&s,10);
    i = strtoul (s,&s,10);
    stream->uid_last = j = strtoul (s,&s,10);
    rnmsgs = nmsgs = (i | j) ? 1 + j - i : 0;
    if (k > nmsgs) {		/* check for absurdity */
      sprintf (tmp,"NNTP SERVER BUG (impossible message count): %lu > %lu",
	       k,nmsgs);
      mm_log (tmp,WARN);
    }
				/* restrict article range if needed */
    if (nntp_range && (nmsgs > nntp_range)) i = 1 + j - (nmsgs = nntp_range);
  }
  else {			/* no such newsgroup */
    mm_log (nstream->reply,ERROR);
    nntp_close (nstream);	/* punt stream */
    return NIL;
  }
				/* instantiate local data */
  stream->local = memset (fs_get (sizeof (NNTPLOCAL)),0,sizeof (NNTPLOCAL));
  LOCAL->nntpstream = nstream;
				/* save state for future recycling */
  if (mb.tlsflag) LOCAL->tlsflag = T;
  if (mb.notlsflag) LOCAL->notlsflag = T;
  if (mb.sslflag) LOCAL->sslflag = T;
  if (mb.novalidate) LOCAL->novalidate = T;
  if (mb.loser) LOCAL->nntpstream->loser = T;
				/* assume present until proven otherwise */
  LOCAL->xhdr = LOCAL->xover = T;
  LOCAL->name = cpystr (mbx);	/* copy newsgroup name */
  if (stream->mulnewsrc) {	/* want to use multiple .newsrc files? */
    strcpy (tmp,newsrc);
    s = tmp + strlen (tmp);	/* end of string */
    *s++ = '-';			/* hyphen delimiter and host */
    lcase (strcpy (s,(long) mail_parameters (NIL,GET_NEWSRCCANONHOST,NIL) ?
		   net_host (nstream->netstream) : mb.host));
    LOCAL->newsrc = cpystr (nq ? (*nq) (stream,tmp,newsrc) : tmp);
  }
  else LOCAL->newsrc = cpystr (newsrc);
  if (mb.user[0]) LOCAL->user = cpystr (mb.user);
  stream->sequence++;		/* bump sequence number */
  stream->rdonly = stream->perm_deleted = T;
				/* UIDs are always valid */
  stream->uid_validity = 0xbeefface;
  sprintf (tmp,"{%s:%lu/nntp",(int) mail_parameters (NIL,GET_TRUSTDNS,NIL) ?
	   net_host (nstream->netstream) : mb.host,
	   net_port (nstream->netstream));
  if (LOCAL->tlsflag) strcat (tmp,"/tls");
  if (LOCAL->notlsflag) strcat (tmp,"/notls");
  if (LOCAL->sslflag) strcat (tmp,"/ssl");
  if (LOCAL->novalidate) strcat (tmp,"/novalidate-cert");
  if (LOCAL->nntpstream->loser) strcat (tmp,"/loser");
  if (stream->secure) strcat (tmp,"/secure");
  if (stream->rdonly) strcat (tmp,"/readonly");
  if (LOCAL->user) sprintf (tmp + strlen (tmp),"/user=\"%s\"",LOCAL->user);
  if (stream->halfopen) strcat (tmp,"}<no_mailbox>");
  else sprintf (tmp + strlen (tmp),"}#news.%s",mbx);
  fs_give ((void **) &stream->mailbox);
  stream->mailbox = cpystr (tmp);

  if (EXTENSION.over &&		/* get overview format if have OVER */
      (nntp_send (LOCAL->nntpstream,"LIST","OVERVIEW.FMT") == NNTPGLIST) &&
      (f = netmsg_slurp (LOCAL->nntpstream->netstream,&k,NIL))) {
    fread (LOCAL->over_fmt = (char *) fs_get ((size_t) k + 3),
	   (size_t) 1,(size_t) k,f);
    LOCAL->over_fmt[k] = '\0';
    fclose (f);			/* flush temp file */
  }
  if (nmsgs) {			/* if any messages exist */
    short silent = stream->silent;
    stream->silent = T;		/* don't notify main program yet */
    mail_exists (stream,nmsgs);	/* silently set the cache to the guesstimate */
				/* get UID/sequence map, nuke holes */
    if (nntp_getmap (stream,mbx,i,j,rnmsgs,nmsgs,tmp)) {
      for (nmsgs = 0;		/* calculate true count */
	   (s = net_getline (nstream->netstream)) && strcmp (s,"."); ) {
	if ((k = atol (s)) > j){/* discard too high article numbers */
	  sprintf (tmp,"NNTP SERVER BUG (out of range article ID): %lu > %lu",
		   k,j);
	  mm_notify (stream,tmp,NIL);
	  stream->unhealthy = T;
	}
	else if (k >= i) {	/* silently ignore too-low article numbers */
				/* guard against server returning extra msgs */
	  if (nmsgs == stream->nmsgs) mail_exists (stream,nmsgs+1);
				/* create elt for this message, set UID */
	  mail_elt (stream,++nmsgs)->private.uid = k;
	}
	fs_give ((void **) &s);
      }
      if (s) fs_give ((void **) &s);
    }
				/* assume c-client/NNTP map is entire range */
    else for (k = 1; k <= nmsgs; k++) mail_elt (stream,k)->private.uid = i++;
    stream->unhealthy = NIL;	/* set healthy */
    stream->nmsgs = 0;		/* whack it back down */
    stream->silent = silent;	/* restore old silent setting */
    mail_exists (stream,nmsgs);	/* notify upper level that messages exist */
				/* read .newsrc entries */
    mail_recent (stream,newsrc_read (mbx,stream));
  }
  else {			/* empty newsgroup or halfopen */
    if (!(stream->silent || stream->halfopen)) {
      sprintf (tmp,"Newsgroup %s is empty",mbx);
      mm_log (tmp,WARN);
    }
    mail_exists (stream,(long) 0);
    mail_recent (stream,(long) 0);
  }
  return stream;		/* return stream to caller */
}

/* NNTP close
 * Accepts: MAIL stream
 *	    option flags
 */

void nntp_mclose (MAILSTREAM *stream,long options)
{
  unsigned long i;
  MESSAGECACHE *elt;
  if (LOCAL) {			/* only if a file is open */
    nntp_check (stream);	/* dump final checkpoint */
    if (LOCAL->over_fmt) fs_give ((void **) &LOCAL->over_fmt);
    if (LOCAL->name) fs_give ((void **) &LOCAL->name);
    if (LOCAL->user) fs_give ((void **) &LOCAL->user);
    if (LOCAL->newsrc) fs_give ((void **) &LOCAL->newsrc);
    if (LOCAL->txt) fclose (LOCAL->txt);
				/* close NNTP connection */
    if (LOCAL->nntpstream) nntp_close (LOCAL->nntpstream);
    for (i = 1; i <= stream->nmsgs; i++)
      if ((elt = mail_elt (stream,i))->private.data)
	fs_give ((void **) &elt->private.data);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
  }
}

/* NNTP fetch fast information
 * Accepts: MAIL stream
 *	    sequence
 *	    option flags
 * This is ugly and slow
 */

void nntp_fetchfast (MAILSTREAM *stream,char *sequence,long flags)
{
  unsigned long i;
  MESSAGECACHE *elt;
				/* get sequence */
  if (stream && LOCAL && ((flags & FT_UID) ?
			  mail_uid_sequence (stream,sequence) :
			  mail_sequence (stream,sequence)))
    for (i = 1; i <= stream->nmsgs; i++) {
      if ((elt = mail_elt (stream,i))->sequence && (elt->valid = T) &&
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
}

/* NNTP fetch flags
 * Accepts: MAIL stream
 *	    sequence
 *	    option flags
 */

void nntp_flags (MAILSTREAM *stream,char *sequence,long flags)
{
  unsigned long i;
  if ((flags & FT_UID) ?	/* validate all elts */
      mail_uid_sequence (stream,sequence) : mail_sequence (stream,sequence))
    for (i = 1; i <= stream->nmsgs; i++) mail_elt (stream,i)->valid = T;
}

/* NNTP fetch overview
 * Accepts: MAIL stream, sequence bits set
 *	    overview return function
 * Returns: T if successful, NIL otherwise
 */

long nntp_overview (MAILSTREAM *stream,overview_t ofn)
{
  unsigned long i,j,k,uid;
  char c,*s,*t,*v,tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  OVERVIEW ov;
  if (!LOCAL->nntpstream->netstream) return NIL;
				/* scan sequence to load cache */
  for (i = 1; i <= stream->nmsgs; i++)
				/* have cached overview yet? */
    if ((elt = mail_elt (stream,i))->sequence && !elt->private.data) {
      for (j = i + 1;		/* no, find end of cache gap range */
	   (j <= stream->nmsgs) && (elt = mail_elt (stream,j))->sequence &&
	   !elt->private.data; j++);
				/* make NNTP range */
      sprintf (tmp,(i == (j - 1)) ? "%lu" : "%lu-%lu",mail_uid (stream,i),
	       mail_uid (stream,j - 1));
      i = j;			/* advance beyond gap */
				/* ask server for overview data to cache */
      if (nntp_over (stream,tmp)) {
	while ((s = net_getline (LOCAL->nntpstream->netstream)) &&
	       strcmp (s,".")) {
				/* death to embedded newlines */
	  for (t = v = s; c = *v++;)
	    if ((c != '\012') && (c != '\015')) *t++ = c;
	  *t++ = '\0';		/* tie off string in case it was shortened */
				/* cache the overview if found its sequence */
	  if ((uid = atol (s)) && (k = mail_msgno (stream,uid)) &&
	      (t = strchr (s,'\t'))) {
	    if ((elt = mail_elt (stream,k))->private.data)
	      fs_give ((void **) &elt->private.data);
	    elt->private.data = (unsigned long) cpystr (t + 1);
	  }
	  else {		/* shouldn't happen, snarl if it does */
	    sprintf (tmp,"Server returned data for unknown UID %lu",uid);
	    mm_notify (stream,tmp,WARN);
	    stream->unhealthy = T;
	  }
				/* flush the overview */
	  fs_give ((void **) &s);
	}
	stream->unhealthy = NIL;/* set healthy */
				/* flush the terminating dot */
	if (s) fs_give ((void **) &s);
      }
      else i = stream->nmsgs;	/* OVER failed, punt cache load */
    }

				/* now scan sequence to return overviews */
  if (ofn) for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence) {
      uid = mail_uid (stream,i);/* UID for this message */
				/* parse cached overview */
      if (nntp_parse_overview (&ov,s = (char *) elt->private.data,elt))
	(*ofn) (stream,uid,&ov,i);
      else {			/* parse failed */
	(*ofn) (stream,uid,NIL,i);
	if (s && *s) {		/* unusable cached entry? */
	  sprintf (tmp,"Unable to parse overview for UID %lu: %.500s",uid,s);
	  mm_notify (stream,tmp,WARN);
	  stream->unhealthy = T;
				/* erase it from the cache */
	  fs_give ((void **) &s);
	}
	stream->unhealthy = NIL;/* set healthy */
				/* insert empty cached text as necessary */
	if (!s) elt->private.data = (unsigned long) cpystr ("");
      }
				/* clean up overview data */
      if (ov.from) mail_free_address (&ov.from);
      if (ov.subject) fs_give ((void **) &ov.subject);
    }
  return T;
}

/* Send OVER to NNTP server
 * Accepts: mail stream
 *	    sequence to send
 * Returns: T if success and overviews will follow, else NIL
 */

long nntp_over (MAILSTREAM *stream,char *sequence)
{
  unsigned char *s;
				/* test for Netscape Collabra server */
  if (EXTENSION.over && LOCAL->xover &&
      nntp_send (LOCAL->nntpstream,"OVER","0") == NNTPOVER) {
    /* "Netscape-Collabra/3.52 03615 NNTP" responds to the OVER command with
     * a bogus "Subject:From:Date:Bytes:Lines" response followed by overviews
     * which lack the Message-ID and References:.  This violates the draft
     * NNTP specification (draft-ietf-nntpext-base-18.txt as of this writing).
     * XOVER works fine.
     */
    while ((s = net_getline (LOCAL->nntpstream->netstream)) && strcmp (s,".")){
      if (!isdigit (*s)) {	/* is it that fetid piece of reptile dung? */
	EXTENSION.over = NIL;	/* sure smells like it */
	mm_log ("Working around Netscape Collabra bug",WARN);
      }
      fs_give ((void **) &s);	/* flush the overview */
    }
    if (s) fs_give ((void **) &s);
				/* don't do this test again */
    if (EXTENSION.over) LOCAL->xover = NIL;
  }
  if (EXTENSION.over)		/* have OVER extension? */
    return (nntp_send (LOCAL->nntpstream,"OVER",sequence) == NNTPOVER) ?
      LONGT : NIL;
  if (LOCAL->xover)		/* try the experiment extension then */
    switch ((int) nntp_send (LOCAL->nntpstream,"XOVER",sequence)) {
    case NNTPOVER:		/* got an overview? */
      return LONGT;
    case NNTPBADCMD:		/* unknown command? */
      LOCAL->xover = NIL;	/* disable future XOVER attempts */
    }
  return NIL;
}

/* Parse OVERVIEW struct from cached NNTP OVER response
 * Accepts: struct to load
 *	    cached OVER response
 *	    internaldate
 * Returns: T if success, NIL if fail
 */

long nntp_parse_overview (OVERVIEW *ov,char *text,MESSAGECACHE *elt)
{
  char *t;
				/* nothing in overview yet */
  memset ((void *) ov,0,sizeof (OVERVIEW));
				/* no cached data */
  if (!(text && *text)) return NIL;
  ov->subject = cpystr (text);	/* make hackable copy of overview */
				/* find end of Subject */
  if (t = strchr (ov->subject,'\t')) {
    *t++ = '\0';		/* tie off Subject, point to From */
				/* find end of From */
    if (ov->date = strchr (t,'\t')) {
      *ov->date++ = '\0';	/* tie off From, point to Date */
				/* load internaldate too */
      if (!elt->day) mail_parse_date (elt,ov->date);
				/* parse From */
      rfc822_parse_adrlist (&ov->from,t,BADHOST);
				/* find end of Date */
      if (ov->message_id = strchr (ov->date,'\t')) {
				/* tie off Date, point to Message-ID */
	*ov->message_id++ = '\0';
				/* find end of Message-ID */
	if (ov->references = strchr (ov->message_id,'\t')) {
				/* tie off Message-ID, point to References */
	  *ov->references++ = '\0';
				/* fine end of References */
	  if (t = strchr (ov->references,'\t')) {
	    *t++ = '\0';	/* tie off References, point to octet size */
				/* parse size of message in octets */
	    ov->optional.octets = atol (t);
				/* find end of size */
	    if (t = strchr (t,'\t')) {
				/* parse size of message in lines */
	      ov->optional.lines = atol (++t);
				/* find Xref */
	      if (ov->optional.xref = strchr (t,'\t'))
		*ov->optional.xref++ = '\0';
	    }
	  }
	}
      }
    }
  }
  return ov->references ? T : NIL;
}

/* NNTP fetch header as text
 * Accepts: mail stream
 *	    message number
 *	    pointer to return size
 *	    flags
 * Returns: header text
 */

char *nntp_header (MAILSTREAM *stream,unsigned long msgno,unsigned long *size,
		   long flags)
{
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  FILE *f;
  *size = 0;
  if ((flags & FT_UID) && !(msgno = mail_msgno (stream,msgno))) return "";
				/* have header text? */
  if (!(elt = mail_elt (stream,msgno))->private.msg.header.text.data) {
    sprintf (tmp,"%lu",mail_uid (stream,msgno));
				/* get header text */
    switch (nntp_send (LOCAL->nntpstream,"HEAD",tmp)) {
    case NNTPHEAD:
      if (f = netmsg_slurp (LOCAL->nntpstream->netstream,size,NIL)) {
	fread (elt->private.msg.header.text.data =
	       (unsigned char *) fs_get ((size_t) *size + 3),
	       (size_t) 1,(size_t) *size,f);
	fclose (f);		/* flush temp file */
				/* tie off header with extra CRLF and NUL */
	elt->private.msg.header.text.data[*size] = '\015';
	elt->private.msg.header.text.data[++*size] = '\012';
	elt->private.msg.header.text.data[++*size] = '\0';
	elt->private.msg.header.text.size = *size;
	elt->valid = T;		/* make elt valid now */
	break;
      }
				/* fall into default case */
    default:			/* failed, mark as deleted and empty */
      elt->valid = elt->deleted = T;
    case NNTPSOFTFATAL:		/* don't mark deleted if stream dead */
      *size = elt->private.msg.header.text.size = 0;
      break;
    }
  }
				/* just return size of text */
  else *size = elt->private.msg.header.text.size;
  return elt->private.msg.header.text.data ?
    (char *) elt->private.msg.header.text.data : "";
}

/* NNTP fetch body
 * Accepts: mail stream
 *	    message number
 *	    pointer to stringstruct to initialize
 *	    flags
 * Returns: T if successful, else NIL
 */

long nntp_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  INIT (bs,mail_string,(void *) "",0);
  if ((flags & FT_UID) && !(msgno = mail_msgno (stream,msgno))) return NIL;
  elt = mail_elt (stream,msgno);
				/* different message, flush cache */
  if (LOCAL->txt && (LOCAL->msgno != msgno)) {
    fclose (LOCAL->txt);
    LOCAL->txt = NIL;
  }
  LOCAL->msgno = msgno;		/* note cached message */
  if (!LOCAL->txt) {		/* have file for this message? */
    sprintf (tmp,"%lu",elt->private.uid);
    switch (nntp_send (LOCAL->nntpstream,"BODY",tmp)) {
    case NNTPBODY:
      if (LOCAL->txt = netmsg_slurp (LOCAL->nntpstream->netstream,
				     &LOCAL->txtsize,NIL)) break;
				/* fall into default case */
    default:			/* failed, mark as deleted */
      elt->deleted = T;
    case NNTPSOFTFATAL:		/* don't mark deleted if stream dead */
      return NIL;
    }
  }
  if (!(flags & FT_PEEK)) {	/* mark seen if needed */
    elt->seen = T;
    mm_flags (stream,elt->msgno);
  }
  INIT (bs,file_string,(void *) LOCAL->txt,LOCAL->txtsize);
  return T;
}

/* NNTP fetch article from message ID (for news: URL support)
 * Accepts: mail stream
 *	    message ID
 *	    pointer to return total message size
 *	    pointer to return file size
 * Returns: FILE * to message if successful, else NIL
 */

FILE *nntp_article (MAILSTREAM *stream,char *msgid,unsigned long *size,
		    unsigned long *hsiz)
{
  return (nntp_send (LOCAL->nntpstream,"ARTICLE",msgid) == NNTPARTICLE) ?
    netmsg_slurp (LOCAL->nntpstream->netstream,size,hsiz) : NIL;
}


/* NNTP per-message modify flag
 * Accepts: MAIL stream
 *	    message cache element
 */

void nntp_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt)
{
  if (!LOCAL->dirty) {		/* only bother checking if not dirty yet */
    if (elt->valid) {		/* if done, see if deleted changed */
      if (elt->sequence != elt->deleted) LOCAL->dirty = T;
      elt->sequence = T;	/* leave the sequence set */
    }
				/* note current setting of deleted flag */
    else elt->sequence = elt->deleted;
  }
}

/* NNTP search messages
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    option flags
 * Returns: T on success, NIL on failure
 */

long nntp_search (MAILSTREAM *stream,char *charset,SEARCHPGM *pgm,long flags)
{
  unsigned long i;
  MESSAGECACHE *elt;
  OVERVIEW ov;
  if (charset && *charset &&	/* convert if charset not US-ASCII or UTF-8 */
      !(((charset[0] == 'U') || (charset[0] == 'u')) &&
	((((charset[1] == 'S') || (charset[1] == 's')) &&
	  (charset[2] == '-') &&
	  ((charset[3] == 'A') || (charset[3] == 'a')) &&
	  ((charset[4] == 'S') || (charset[4] == 's')) &&
	  ((charset[5] == 'C') || (charset[5] == 'c')) &&
	  ((charset[6] == 'I') || (charset[6] == 'i')) &&
	  ((charset[7] == 'I') || (charset[7] == 'i')) && !charset[8]) ||
	 (((charset[1] == 'T') || (charset[1] == 't')) &&
	  ((charset[2] == 'F') || (charset[2] == 'f')) &&
	  (charset[3] == '-') && (charset[4] == '8') && !charset[5])))) {
    if (utf8_text (NIL,charset,NIL,T)) utf8_searchpgm (pgm,charset);
    else return NIL;		/* charset unknown */
  }
  if (flags & SO_OVERVIEW) {	/* only if specified to use overview */
				/* identify messages that will be searched */
    for (i = 1; i <= stream->nmsgs; ++i)
      mail_elt (stream,i)->sequence = nntp_search_msg (stream,i,pgm,NIL);
    nntp_overview (stream,NIL);	/* load the overview cache */
  }
				/* init in case no overview at cleanup */
  memset ((void *) &ov,0,sizeof (OVERVIEW));
				/* otherwise do default search */
  for (i = 1; i <= stream->nmsgs; ++i) {
    if (((flags & SO_OVERVIEW) && ((elt = mail_elt (stream,i))->sequence) &&
	 nntp_parse_overview (&ov,(char *) elt->private.data,elt)) ?
	nntp_search_msg (stream,i,pgm,&ov) :
	mail_search_msg (stream,i,NIL,pgm)) {
      if (flags & SE_UID) mm_searched (stream,mail_uid (stream,i));
      else {			/* mark as searched, notify mail program */
	mail_elt (stream,i)->searched = T;
	if (!stream->silent) mm_searched (stream,i);
      }
    }
				/* clean up overview data */
    if (ov.from) mail_free_address (&ov.from);
    if (ov.subject) fs_give ((void **) &ov.subject);
  }
  return LONGT;
}

/* NNTP search message
 * Accepts: MAIL stream
 *	    message number
 *	    search program
 *	    overview to search (NIL means preliminary pass)
 * Returns: T if found, NIL otherwise
 */

long nntp_search_msg (MAILSTREAM *stream,unsigned long msgno,SEARCHPGM *pgm,
		      OVERVIEW *ov)
{
  unsigned short d;
  MESSAGECACHE *elt = mail_elt (stream,msgno);
  SEARCHHEADER *hdr;
  SEARCHOR *or;
  SEARCHPGMLIST *not;
  if (pgm->msgno || pgm->uid) {	/* message set searches */
    SEARCHSET *set;
				/* message sequences */
    if (set = pgm->msgno) {	/* must be inside this sequence */
      while (set) {		/* run down until find matching range */
	if (set->last ? ((msgno < set->first) || (msgno > set->last)) :
	    msgno != set->first) set = set->next;
	else break;
      }
      if (!set) return NIL;	/* not found within sequence */
    }
    if (set = pgm->uid) {	/* must be inside this sequence */
      unsigned long uid = mail_uid (stream,msgno);
      while (set) {		/* run down until find matching range */
	if (set->last ? ((uid < set->first) || (uid > set->last)) :
	    uid != set->first) set = set->next;
	else break;
      }
      if (!set) return NIL;	/* not found within sequence */
    }
  }

  /* Fast data searches */
				/* message flags */
  if ((pgm->answered && !elt->answered) ||
      (pgm->unanswered && elt->answered) ||
      (pgm->deleted && !elt->deleted) ||
      (pgm->undeleted && elt->deleted) ||
      (pgm->draft && !elt->draft) ||
      (pgm->undraft && elt->draft) ||
      (pgm->flagged && !elt->flagged) ||
      (pgm->unflagged && elt->flagged) ||
      (pgm->recent && !elt->recent) ||
      (pgm->old && elt->recent) ||
      (pgm->seen && !elt->seen) ||
      (pgm->unseen && elt->seen)) return NIL;
				/* keywords */
  if ((pgm->keyword && !mail_search_keyword (stream,elt,pgm->keyword,LONGT)) ||
      (pgm->unkeyword && mail_search_keyword (stream,elt,pgm->unkeyword,NIL)))
    return NIL;
  if (ov) {			/* only do this if real searching */
    MESSAGECACHE delt;
				/* size ranges */
    if ((pgm->larger && (ov->optional.octets <= pgm->larger)) ||
	(pgm->smaller && (ov->optional.octets >= pgm->smaller))) return NIL;
				/* date ranges */
    if ((pgm->sentbefore || pgm->senton || pgm->sentsince ||
	 (pgm->before || pgm->on || pgm->since)) &&
	(!mail_parse_date (&delt,ov->date) ||
	 !(d = mail_shortdate (delt.year,delt.month,delt.day)) ||
	 (pgm->sentbefore && (d >= pgm->sentbefore)) ||
	 (pgm->senton && (d != pgm->senton)) ||
	 (pgm->sentsince && (d < pgm->sentsince)) ||
	 (pgm->before && (d >= pgm->before)) ||
	 (pgm->on && (d != pgm->on)) ||
	 (pgm->since && (d < pgm->since)))) return NIL;
    if ((pgm->from && !mail_search_addr (ov->from,pgm->from)) ||
	(pgm->subject && !mail_search_header_text (ov->subject,pgm->subject))||
	(pgm->message_id &&
	 !mail_search_header_text (ov->message_id,pgm->message_id)) ||
	(pgm->references &&
	 !mail_search_header_text (ov->references,pgm->references)))
      return NIL;


				/* envelope searches */
    if (pgm->bcc || pgm->cc || pgm->to || pgm->return_path || pgm->sender ||
	pgm->reply_to || pgm->in_reply_to || pgm->newsgroups ||
	pgm->followup_to) {
      ENVELOPE *env = mail_fetchenvelope (stream,msgno);
      if (!env) return NIL;	/* no envelope obtained */
				/* search headers */
      if ((pgm->bcc && !mail_search_addr (env->bcc,pgm->bcc)) ||
	  (pgm->cc && !mail_search_addr (env->cc,pgm->cc)) ||
	  (pgm->to && !mail_search_addr (env->to,pgm->to)))
	return NIL;
      /* These criteria are not supported by IMAP and have to be emulated */
      if ((pgm->return_path &&
	   !mail_search_addr (env->return_path,pgm->return_path)) ||
	  (pgm->sender && !mail_search_addr (env->sender,pgm->sender)) ||
	  (pgm->reply_to && !mail_search_addr (env->reply_to,pgm->reply_to)) ||
	  (pgm->in_reply_to &&
	   !mail_search_header_text (env->in_reply_to,pgm->in_reply_to)) ||
	  (pgm->newsgroups &&
	   !mail_search_header_text (env->newsgroups,pgm->newsgroups)) ||
	  (pgm->followup_to &&
	   !mail_search_header_text (env->followup_to,pgm->followup_to)))
	return NIL;
    }

				/* search header lines */
    for (hdr = pgm->header; hdr; hdr = hdr->next) {
      char *t,*e,*v;
      SIZEDTEXT s;
      STRINGLIST sth,stc;
      sth.next = stc.next = NIL;/* only one at a time */
      sth.text.data = hdr->line.data;
      sth.text.size = hdr->line.size;
				/* get the header text */
      if ((t = mail_fetch_header (stream,msgno,NIL,&sth,&s.size,
				  FT_INTERNAL | FT_PEEK)) && strchr (t,':')) {
	if (hdr->text.size) {	/* anything matches empty search string */
				/* non-empty, copy field data */
	  s.data = (unsigned char *) fs_get (s.size + 1);
				/* for each line */
	  for (v = (char *) s.data, e = t + s.size; t < e;) switch (*t) {
	  default:		/* non-continuation, skip leading field name */
	    while ((t < e) && (*t++ != ':'));
	    if ((t < e) && (*t == ':')) t++;
	  case '\t': case ' ':	/* copy field data  */
	    while ((t < e) && (*t != '\015') && (*t != '\012')) *v++ = *t++;
	    *v++ = '\n';	/* tie off line */
	    while (((*t == '\015') || (*t == '\012')) && (t < e)) t++;
	  }
				/* calculate true size */
	  s.size = v - (char *) s.data;
	  *v = '\0';		/* tie off results */
	  stc.text.data = hdr->text.data;
	  stc.text.size = hdr->text.size;
				/* search header */
	  if (mail_search_header (&s,&stc)) fs_give ((void **) &s.data);
	  else {		/* search failed */
	    fs_give ((void **) &s.data);
	    return NIL;
	  }
	}
      }
      else return NIL;		/* no matching header text */
    }
				/* search strings */
    if ((pgm->text &&
	 !mail_search_text (stream,msgno,NIL,pgm->text,LONGT))||
	(pgm->body && !mail_search_text (stream,msgno,NIL,pgm->body,NIL)))
      return NIL;
  }
				/* logical conditions */
  for (or = pgm->or; or; or = or->next)
    if (!(nntp_search_msg (stream,msgno,or->first,ov) ||
	  nntp_search_msg (stream,msgno,or->second,ov))) return NIL;
  for (not = pgm->not; not; not = not->next)
    if (nntp_search_msg (stream,msgno,not->pgm,ov)) return NIL;
  return T;
}

/* NNTP sort messages
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    sort program
 *	    option flags
 * Returns: vector of sorted message sequences or NIL if error
 */

unsigned long *nntp_sort (MAILSTREAM *stream,char *charset,SEARCHPGM *spg,
			  SORTPGM *pgm,long flags)
{
  unsigned long i,start,last;
  SORTCACHE **sc;
  mailcache_t mailcache = (mailcache_t) mail_parameters (NIL,GET_CACHE,NIL);
  unsigned long *ret = NIL;
  sortresults_t sr = (sortresults_t) mail_parameters (NIL,GET_SORTRESULTS,NIL);
  if (spg) {			/* only if a search needs to be done */
    int silent = stream->silent;
    stream->silent = T;		/* don't pass up mm_searched() events */
				/* search for messages */
    mail_search_full (stream,charset,spg,NIL);
    stream->silent = silent;	/* restore silence state */
  }
				/* initialize progress counters */
  pgm->nmsgs = pgm->progress.cached = 0;
				/* pass 1: count messages to sort */
  for (i = 1,start = last = 0; i <= stream->nmsgs; ++i)
    if (mail_elt (stream,i)->searched) {
      pgm->nmsgs++;
				/* have this in the sortcache already? */
      if (!((SORTCACHE *) (*mailcache) (stream,i,CH_SORTCACHE))->date) {
				/* no, record as last message */
	last = mail_uid (stream,i);
				/* and as first too if needed */
	if (!start) start = last;
      }
    }
  if (pgm->nmsgs) {		/* pass 2: load sort cache */
    sc = nntp_sort_loadcache (stream,pgm,start,last,flags);
				/* pass 3: sort messages */
    if (!pgm->abort) ret = mail_sort_cache (stream,pgm,sc,flags);
    fs_give ((void **) &sc);	/* don't need sort vector any more */
  }
				/* empty sort results */
  else ret = (unsigned long *) memset (fs_get (sizeof (unsigned long)),0,
				       sizeof (unsigned long));
				/* also return via callback if requested */
  if (sr) (*sr) (stream,ret,pgm->nmsgs);
  return ret;
}

/* Mail load sortcache
 * Accepts: mail stream, already searched
 *	    sort program
 *	    first UID to OVER
 *	    last UID to OVER
 *	    option flags
 * Returns: vector of sortcache pointers matching search
 */

SORTCACHE **nntp_sort_loadcache (MAILSTREAM *stream,SORTPGM *pgm,
				 unsigned long start,unsigned long last,
				 long flags)
{
  unsigned long i;
  char c,*s,*t,*v,tmp[MAILTMPLEN];
  SORTPGM *pg;
  SORTCACHE **sc,*r;
  MESSAGECACHE telt;
  ADDRESS *adr = NIL;
  mailcache_t mailcache = (mailcache_t) mail_parameters (NIL,GET_CACHE,NIL);
				/* verify that the sortpgm is OK */
  for (pg = pgm; pg; pg = pg->next) switch (pg->function) {
  case SORTARRIVAL:		/* sort by arrival date */
  case SORTSIZE:		/* sort by message size */
  case SORTDATE:		/* sort by date */
  case SORTFROM:		/* sort by first from */
  case SORTSUBJECT:		/* sort by subject */
    break;
  case SORTTO:			/* sort by first to */
    mm_notify (stream,"[NNTPSORT] Can't do To-field sorting in NNTP",WARN);
    break;
  case SORTCC:			/* sort by first cc */
    mm_notify (stream,"[NNTPSORT] Can't do cc-field sorting in NNTP",WARN);
    break;
  default:
    fatal ("Unknown sort function");
  }

  if (start) {			/* messages need to be loaded in sortcache? */
				/* yes, build range */
    if (start != last) sprintf (tmp,"%lu-%lu",start,last);
    else sprintf (tmp,"%lu",start);
				/* get it from the NNTP server */
    if (!nntp_over (stream,tmp)) return mail_sort_loadcache (stream,pgm);
    while ((s = net_getline (LOCAL->nntpstream->netstream)) && strcmp (s,".")){
				/* death to embedded newlines */
      for (t = v = s; c = *v++;) if ((c != '\012') && (c != '\015')) *t++ = c;
      *t++ = '\0';		/* tie off resulting string */
				/* parse OVER response */
      if ((i = mail_msgno (stream,atol (s))) &&
	  (t = strchr (s,'\t')) && (v = strchr (++t,'\t'))) {
	*v++ = '\0';		/* tie off subject */
				/* put stripped subject in sortcache */
	r = (SORTCACHE *) (*mailcache) (stream,i,CH_SORTCACHE);
	  r->refwd = mail_strip_subject (t,&r->subject);
	if (t = strchr (v,'\t')) {
	  *t++ = '\0';		/* tie off from */
	  if (adr = rfc822_parse_address (&adr,adr,&v,BADHOST,0)) {
	    r->from = adr->mailbox;
	    adr->mailbox = NIL;
	    mail_free_address (&adr);
	  }
	  if (v = strchr (t,'\t')) {
	    *v++ = '\0';	/* tie off date */
	    if (mail_parse_date (&telt,t)) r->date = mail_longdate (&telt);
	    if ((v = strchr (v,'\t')) && (v = strchr (++v,'\t')))
	      r->size = atol (++v);
	  }
	}
      }
      fs_give ((void **) &s);
    }
    if (s) fs_give ((void **) &s);
  }

				/* calculate size of sortcache index */
  i = pgm->nmsgs * sizeof (SORTCACHE *);
				/* instantiate the index */
  sc = (SORTCACHE **) memset (fs_get ((size_t) i),0,(size_t) i);
				/* see what needs to be loaded */
  for (i = 1; !pgm->abort && (i <= stream->nmsgs); i++)
    if ((mail_elt (stream,i))->searched) {
      sc[pgm->progress.cached++] =
	r = (SORTCACHE *) (*mailcache) (stream,i,CH_SORTCACHE);
      r->pgm = pgm;	/* note sort program */
      r->num = (flags & SE_UID) ? mail_uid (stream,i) : i;
      if (!r->date) r->date = r->num;
      if (!r->arrival) r->arrival = mail_uid (stream,i);
      if (!r->size) r->size = 1;
      if (!r->from) r->from = cpystr ("");
      if (!r->to) r->to = cpystr ("");
      if (!r->cc) r->cc = cpystr ("");
      if (!r->subject) r->subject = cpystr ("");
    }
  return sc;
}


/* NNTP thread messages
 * Accepts: mail stream
 *	    thread type
 *	    character set
 *	    search program
 *	    option flags
 * Returns: thread node tree
 */

THREADNODE *nntp_thread (MAILSTREAM *stream,char *type,char *charset,
			 SEARCHPGM *spg,long flags)
{
  return mail_thread_msgs (stream,type,charset,spg,flags,nntp_sort);
}

/* NNTP ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long nntp_ping (MAILSTREAM *stream)
{
  return (nntp_send (LOCAL->nntpstream,"STAT",NIL) != NNTPSOFTFATAL);
}


/* NNTP check mailbox
 * Accepts: MAIL stream
 */

void nntp_check (MAILSTREAM *stream)
{
				/* never do if no updates */
  if (LOCAL->dirty) newsrc_write (LOCAL->name,stream);
  LOCAL->dirty = NIL;
}


/* NNTP expunge mailbox
 * Accepts: MAIL stream
 */

void nntp_expunge (MAILSTREAM *stream)
{
  if (!stream->silent) mm_log ("Expunge ignored on readonly mailbox",NIL);
}

/* NNTP copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    option flags
 * Returns: T if copy successful, else NIL
 */

long nntp_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  if (pc) return (*pc) (stream,sequence,mailbox,options);
  mm_log ("Copy not valid for NNTP",ERROR);
  return NIL;
}


/* NNTP append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    append callback
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

long nntp_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  mm_log ("Append not valid for NNTP",ERROR);
  return NIL;
}

/* NNTP open connection
 * Accepts: network driver
 *	    service host list
 *	    port number
 *	    service name
 *	    NNTP open options
 * Returns: SEND stream on success, NIL on failure
 */

SENDSTREAM *nntp_open_full (NETDRIVER *dv,char **hostlist,char *service,
			    unsigned long port,long options)
{
  SENDSTREAM *stream = NIL;
  NETSTREAM *netstream = NIL;
  NETMBX mb;
  char tmp[MAILTMPLEN];
  long extok = LONGT;
  NETDRIVER *ssld = (NETDRIVER *) mail_parameters (NIL,GET_SSLDRIVER,NIL);
  sslstart_t stls = (sslstart_t) mail_parameters (NIL,GET_SSLSTART,NIL);
  if (!(hostlist && *hostlist)) mm_log ("Missing NNTP service host",ERROR);
  else do {			/* try to open connection */
    sprintf (tmp,"{%.200s/%.20s}",*hostlist,service ? service : "nntp");
    if (!mail_valid_net_parse (tmp,&mb) || mb.anoflag) {
      sprintf (tmp,"Invalid host specifier: %.80s",*hostlist);
      mm_log (tmp,ERROR);
    }
    else {			/* light tryssl flag if requested */
      mb.trysslflag = (options & NOP_TRYSSL) ? T : NIL;
				/* default port */
      if (mb.port) port = mb.port;
      else if (!port) port = nntp_port ? nntp_port : NNTPTCPPORT;
      if (netstream =		/* try to open ordinary connection */
	  net_open (&mb,dv,port,
		    (NETDRIVER *) mail_parameters (NIL,GET_SSLDRIVER,NIL),
		    "*nntps",nntp_sslport ? nntp_sslport : NNTPSSLPORT)) {
	stream = (SENDSTREAM *) fs_get (sizeof (SENDSTREAM));
				/* initialize stream */
	memset ((void *) stream,0,sizeof (SENDSTREAM));
	stream->netstream = netstream;
	stream->host = cpystr ((int) mail_parameters (NIL,GET_TRUSTDNS,NIL) ?
			       net_host (netstream) : mb.host);
	stream->debug = (mb.dbgflag || (options & NOP_DEBUG)) ? T : NIL;
	if (mb.loser) stream->loser = T;
				/* process greeting */
	switch ((int) nntp_reply (stream)) {
	case NNTPGREET:		/* allow posting */
	  NNTP.post = T;
	  mm_notify (NIL,stream->reply + 4,(long) NIL);
	  break;
	case NNTPGREETNOPOST:	/* posting not allowed, must be readonly */
	  NNTP.post = NIL;
	  break;
	default:
	  mm_log (stream->reply,ERROR);
	  stream = nntp_close (stream);
	  break;
	}
      }
    }
  } while (!stream && *++hostlist);

				/* get extensions */
  if (stream && extok)
    extok = nntp_extensions (stream,(mb.secflag ? AU_SECURE : NIL) |
			     (mb.authuser[0] ? AU_AUTHUSER : NIL));
  if (stream && !dv && stls && NNTP.ext.starttls &&
      !mb.sslflag && !mb.notlsflag &&
      (nntp_send_work (stream,"STARTTLS",NNTP.ext.multidomain ? mb.host : NIL)
       == NNTPTLSSTART)) {
    mb.tlsflag = T;		/* TLS OK, get into TLS at this end */
    stream->netstream->dtb = ssld;
				/* negotiate TLS */
    if (stream->netstream->stream =
	(*stls) (stream->netstream->stream,mb.host,
		 NET_TLSCLIENT | (mb.novalidate ? NET_NOVALIDATECERT:NIL)))
      extok = nntp_extensions (stream,(mb.secflag ? AU_SECURE : NIL) |
			       (mb.authuser[0] ? AU_AUTHUSER : NIL));
    else {
      sprintf (tmp,"Unable to negotiate TLS with this server: %.80s",mb.host);
      mm_log (tmp,ERROR);
				/* close without doing QUIT */
      if (stream->netstream) net_close (stream->netstream);
      stream->netstream = NIL;
      stream = nntp_close (stream);
    }
  }
  else if (mb.tlsflag) {	/* user specified /tls but can't do it */
    mm_log ("Unable to negotiate TLS with this server",ERROR);
    return NIL;
  }
  if (stream) {			/* have a session? */
    if (mb.user[0]) {		/* yes, have user name? */
      if ((int) mail_parameters (NIL,GET_TRUSTDNS,NIL)) {
				/* remote name for authentication */
	strncpy (mb.host,(int) mail_parameters (NIL,GET_SASLUSESPTRNAME,NIL) ?
		 net_remotehost (netstream) : net_host (netstream),
		 NETMAXHOST-1);
	mb.host[NETMAXHOST-1] = '\0';
      }
      if (!nntp_send_auth_work (stream,&mb,tmp,NIL))
	stream = nntp_close (stream);
    }
				/* authenticate if no-post and not readonly */
    else if (!(NNTP.post || (options & NOP_READONLY) ||
	       nntp_send_auth (stream,NIL))) stream = nntp_close (stream);
  }

				/* in case server demands MODE READER */
  if (stream) switch ((int) nntp_send_work (stream,"MODE","READER")) {
  case NNTPGREET:
    NNTP.post = T;
    break;
  case NNTPGREETNOPOST:
    NNTP.post = NIL;
    break;
  case NNTPWANTAUTH:		/* server wants auth first, do so and retry */
  case NNTPWANTAUTH2:		/* remote name for authentication */
    if ((int) mail_parameters (NIL,GET_TRUSTDNS,NIL)) {
      strncpy (mb.host,(int) mail_parameters (NIL,GET_SASLUSESPTRNAME,NIL) ?
	       net_remotehost (netstream) : net_host (netstream),NETMAXHOST-1);
      mb.host[NETMAXHOST-1] = '\0';
    }
    if (nntp_send_auth_work (stream,&mb,tmp,NIL))
      switch ((int) nntp_send (stream,"MODE","READER")) {
      case NNTPGREET:
	NNTP.post = T;
	break;
      case NNTPGREETNOPOST:
	NNTP.post = NIL;
	break;
      }
    else stream = nntp_close (stream);
    break;
  }
  if (stream) {			/* looks like we have a stream? */
				/* yes, make sure can post if not readonly */
    if (!(NNTP.post || (options & NOP_READONLY))) stream = nntp_close (stream);
    else if (extok) nntp_extensions (stream,(mb.secflag ? AU_SECURE : NIL) |
				     (mb.authuser[0] ? AU_AUTHUSER : NIL));
  }
  return stream;
}

/* NNTP extensions
 * Accepts: stream
 *	    authenticator flags
 * Returns: T on success, NIL on failure
 */

long nntp_extensions (SENDSTREAM *stream,long flags)
{
  unsigned long i;
  char *t,*args;
				/* zap all old extensions */
  memset (&NNTP.ext,0,sizeof (NNTP.ext));
  if (stream->loser) return NIL;/* nothing at all for losers */
				/* get server extensions */
  switch ((int) nntp_send_work (stream,"LIST","EXTENSIONS")) {
  case NNTPEXTOK:		/* what NNTP base spec says */
  case NNTPGLIST:		/* some servers do this instead */
    break;
  default:			/* no LIST EXTENSIONS on this server */
    return NIL;
  }
  NNTP.ext.ok = T;		/* server offers extensions */
  while ((t = net_getline (stream->netstream)) && (t[1] || (*t != '.'))) {
    if (stream->debug) mm_dlog (t);
				/* get optional capability arguments */
    if (args = strchr (t,' ')) *args++ = '\0';
    if (!compare_cstring (t,"LISTGROUP")) NNTP.ext.listgroup = T;
    else if (!compare_cstring (t,"OVER")) NNTP.ext.over = T;
    else if (!compare_cstring (t,"HDR")) NNTP.ext.hdr = T;
    else if (!compare_cstring (t,"PAT")) NNTP.ext.pat = T;
    else if (!compare_cstring (t,"STARTTLS")) NNTP.ext.starttls = T;
    else if (!compare_cstring (t,"MULTIDOMAIN")) NNTP.ext.multidomain = T;

    else if (!compare_cstring (t,"AUTHINFO") && args) {
      char *sasl = NIL;
      for (args = strtok (args," "); args; args = strtok (NIL," ")) {
	if (!compare_cstring (args,"USER")) NNTP.ext.authuser = T;
	else if (((args[0] == 'S') || (args[0] == 's')) &&
		 ((args[1] == 'A') || (args[1] == 'a')) &&
		 ((args[2] == 'S') || (args[2] == 's')) &&
		 ((args[3] == 'L') || (args[3] == 'l')) && (args[4] == ':'))
	  sasl = args + 5;
      }
      if (sasl) {		/* if SASL, look up authenticators */
	for (sasl = strtok (sasl,","); sasl; sasl = strtok (NIL,","))
	  if ((i = mail_lookup_auth_name (sasl,flags)) &&
	      (--i < MAXAUTHENTICATORS))
	    NNTP.ext.sasl |= (1 << i);
				/* disable LOGIN if PLAIN also advertised */
	if ((i = mail_lookup_auth_name ("PLAIN",NIL)) &&
	    (--i < MAXAUTHENTICATORS) && (NNTP.ext.sasl & (1 << i)) &&
	    (i = mail_lookup_auth_name ("LOGIN",NIL)) &&
	    (--i < MAXAUTHENTICATORS)) NNTP.ext.sasl &= ~(1 << i);
      }
    }
    fs_give ((void **) &t);
  }
  if (t) {			/* flush end of text indicator */
    if (stream->debug) mm_dlog (t);
    fs_give ((void **) &t);
  }
  return LONGT;
}

/* NNTP close connection
 * Accepts: SEND stream
 * Returns: NIL always
 */

SENDSTREAM *nntp_close (SENDSTREAM *stream)
{
  if (stream) {			/* send "QUIT" */
    if (stream->netstream) nntp_send (stream,"QUIT",NIL);
				/* do close actions */
    if (stream->netstream) net_close (stream->netstream);
    if (stream->host) fs_give ((void **) &stream->host);
    if (stream->reply) fs_give ((void **) &stream->reply);
    fs_give ((void **) &stream);/* flush the stream */
  }
  return NIL;
}

/* NNTP deliver news
 * Accepts: SEND stream
 *	    message envelope
 *	    message body
 * Returns: T on success, NIL on failure
 */

long nntp_mail (SENDSTREAM *stream,ENVELOPE *env,BODY *body)
{
  long ret;
  char *s,path[MAILTMPLEN],tmp[8*MAILTMPLEN];
  /* Gabba gabba hey, we need some brain damage to send netnews!!!
   *
   * First, we give ourselves a frontal lobotomy, and put in some UUCP
   *  syntax.  It doesn't matter that it's completely bogus UUCP, and
   *  that UUCP has nothing to do with anything we're doing.  It's been
   *  alleged that "Path: not-for-mail" is also acceptable, but we won't
   *  make assumptions unless the user says so.
   *
   * Second, we bop ourselves on the head with a ball-peen hammer.  How
   *  dare we be so presumptious as to insert a *comment* in a Date:
   *  header line.  Why, we were actually trying to be nice to a human
   *  by giving a symbolic timezone (such as PST) in addition to a
   *  numeric timezone (such as -0800).  But the gods of news transport
   *  will have none of this.  Unix weenies, tried and true, rule!!!
   *
   * Third, Netscape Collabra server doesn't give the NNTPWANTAUTH error
   *  until after requesting and receiving the entire message.  So we can't
   *  call rely upon nntp_send()'s to do the auth retry.
   */
				/* RFC-1036 requires this cretinism */
  sprintf (path,"Path: %s!%s\015\012",net_localhost (stream->netstream),
	   env->sender ? env->sender->mailbox :
	   (env->from ? env->from->mailbox : "not-for-mail"));
				/* here's another cretinism */
  if (s = strstr (env->date," (")) *s = NIL;
  do if ((ret = nntp_send_work (stream,"POST",NIL)) == NNTPREADY)
				/* output data, return success status */
    ret = (net_soutr (stream->netstream,
		      nntp_hidepath ? "Path: not-for-mail\015\012" : path) &&
	   rfc822_output (tmp,env,body,nntp_soutr,stream->netstream,T)) ?
	     nntp_send_work (stream,".",NIL) :
	       nntp_fake (stream,"NNTP connection broken (message text)");
  while (((ret == NNTPWANTAUTH) || (ret == NNTPWANTAUTH2)) &&
	 nntp_send_auth (stream,LONGT));
  if (s) *s = ' ';		/* put the comment in the date back */
  if (ret == NNTPOK) return LONGT;
  else if (ret < 400) {		/* if not an error reply */
    sprintf (tmp,"Unexpected NNTP posting reply code %ld",ret);
    mm_log (tmp,WARN);		/* so someone looks at this eventually */
    if ((ret >= 200) && (ret < 300)) return LONGT;
  }
  return NIL;
}

/* NNTP send command
 * Accepts: SEND stream
 *	    text
 * Returns: reply code
 */

long nntp_send (SENDSTREAM *stream,char *command,char *args)
{
  long ret;
  switch ((int) (ret = nntp_send_work (stream,command,args))) {
  case NNTPWANTAUTH:		/* authenticate and retry */
  case NNTPWANTAUTH2:
    if (nntp_send_auth (stream,LONGT))
      ret = nntp_send_work (stream,command,args);
    else {			/* we're probably hosed, nuke the session */
      nntp_send (stream,"QUIT",NIL);
				/* close net connection */
      if (stream->netstream) net_close (stream->netstream);
      stream->netstream = NIL;
    }
  default:			/* all others just return */
    break;
  }
  return ret;
}


/* NNTP send command worker routine
 * Accepts: SEND stream
 *	    text
 * Returns: reply code
 */

long nntp_send_work (SENDSTREAM *stream,char *command,char *args)
{
  long ret;
  char *s = (char *) fs_get (strlen (command) + (args ? strlen (args) + 1 : 0)
			     + 3);
  if (!stream->netstream) ret = nntp_fake (stream,"NNTP connection lost");
  else {			/* build the complete command */
    if (args) sprintf (s,"%s %s",command,args);
    else strcpy (s,command);
    if (stream->debug) mail_dlog (s,stream->sensitive);
    strcat (s,"\015\012");
				/* send the command */
    ret = net_soutr (stream->netstream,s) ? nntp_reply (stream) :
      nntp_fake (stream,"NNTP connection broken (command)");
  }
  fs_give ((void **) &s);
  return ret;
}

/* NNTP send authentication if needed
 * Accepts: SEND stream
 *	    flags (non-NIL to get new extensions)
 * Returns: T if need to redo command, NIL otherwise
 */

long nntp_send_auth (SENDSTREAM *stream,long flags)
{
  NETMBX mb;
  char tmp[MAILTMPLEN];
				/* remote name for authentication */
  sprintf (tmp,"{%.200s/nntp",(int) mail_parameters (NIL,GET_TRUSTDNS,NIL) ?
	   ((int) mail_parameters (NIL,GET_SASLUSESPTRNAME,NIL) ?
	    net_remotehost (stream->netstream) : net_host (stream->netstream)):
	   stream->host);
  if (stream->netstream->dtb ==
      (NETDRIVER *) mail_parameters (NIL,GET_SSLDRIVER,NIL))
    strcat (tmp,"/ssl");
  strcat (tmp,"}<none>");
  mail_valid_net_parse (tmp,&mb);
  return nntp_send_auth_work (stream,&mb,tmp,flags);
}

/* NNTP send authentication worker routine
 * Accepts: SEND stream
 *	    NETMBX structure
 *	    scratch buffer of length MAILTMPLEN
 *	    flags (non-NIL to get new extensions)
 * Returns: T if authenticated, NIL otherwise
 */

long nntp_send_auth_work (SENDSTREAM *stream,NETMBX *mb,char *pwd,long flags)
{
  unsigned long trial,auths;
  char tmp[MAILTMPLEN],usr[MAILTMPLEN];
  AUTHENTICATOR *at;
  char *lsterr = NIL;
  long ret = NIL;
				/* try SASL first */
  for (auths = NNTP.ext.sasl, stream->saslcancel = NIL;
       !ret && stream->netstream && auths &&
       (at = mail_lookup_auth (find_rightmost_bit (&auths) + 1)); ) {
    if (lsterr) {		/* previous authenticator failed? */
      sprintf (tmp,"Retrying using %s authentication after %.80s",
	       at->name,lsterr);
      mm_log (tmp,NIL);
      fs_give ((void **) &lsterr);
    }
    trial = 0;			/* initial trial count */
    tmp[0] = '\0';		/* empty buffer */
    if (stream->netstream) do {
      if (lsterr) {
	sprintf (tmp,"Retrying %s authentication after %.80s",at->name,lsterr);
	mm_log (tmp,WARN);
	fs_give ((void **) &lsterr);
      }
      stream->saslcancel = NIL;
      if (nntp_send (stream,"AUTHINFO SASL",at->name)) {
				/* hide client authentication responses */
	if (!(at->flags & AU_SECURE)) stream->sensitive = T;
	if ((*at->client) (nntp_challenge,nntp_response,"nntp",mb,stream,
			   &trial,usr)) {
	  if (stream->replycode == NNTPAUTHED) ret = LONGT;
				/* if main program requested cancellation */
	  else if (!trial) mm_log ("NNTP Authentication cancelled",ERROR);
	}
	stream->sensitive = NIL;/* unhide */
      }
				/* remember response if error and no cancel */
      if (!ret && trial) lsterr = cpystr (stream->reply);
    } while (!ret && stream->netstream && trial &&
	     (trial < nntp_maxlogintrials));
  }

  if (lsterr) {			/* SAIL failed? */
    if (!stream->saslcancel) {	/* don't do this if a cancel */
      sprintf (tmp,"Can not authenticate to NNTP server: %.80s",lsterr);
      mm_log (tmp,ERROR);
    }
    fs_give ((void **) &lsterr);
  }
  else if (mb->secflag)		/* no SASL, can't do /secure */
    mm_log ("Can't do secure authentication with this server",ERROR);
  else if (mb->authuser[0])	/* or /authuser */
    mm_log ("Can't do /authuser with this server",ERROR);
  /* Always try AUTHINFO USER, even if NNTP.ext.authuser isn't set.  There
   * are servers that require it but don't return it as an extension.
   */
  else for (trial = 0, pwd[0] = 'x';
	    !ret && pwd[0] && (trial < nntp_maxlogintrials) &&
	      stream->netstream; ) {
    pwd[0] = NIL;		/* get user name and password */
    mm_login (mb,usr,pwd,trial++);
				/* do the authentication */
    if (pwd[0]) switch ((int) nntp_send_work (stream,"AUTHINFO USER",usr)) {
    case NNTPBADCMD:		/* give up if unrecognized command */
      mm_log (NNTP.ext.authuser ? stream->reply :
	      "Can't do AUTHINFO USER to this server",ERROR);
      trial = nntp_maxlogintrials;
      break;
    case NNTPAUTHED:		/* successful authentication */
      ret = LONGT;		/* guess no password was needed */
      break;
    case NNTPWANTPASS:		/* wants password */
      stream->sensitive = T;	/* hide this command */
      if (nntp_send_work (stream,"AUTHINFO PASS",pwd) == NNTPAUTHED)
	ret = LONGT;		/* password OK */
      stream->sensitive = NIL;	/* unhide */
      if (ret) break;		/* OK if successful */
    default:			/* authentication failed */
      mm_log (stream->reply,WARN);
      if (trial == nntp_maxlogintrials)
	mm_log ("Too many NNTP authentication failures",ERROR);
    }
				/* user refused to give a password */
    else mm_log ("Login aborted",ERROR);
  }
  memset (pwd,0,MAILTMPLEN);	/* erase password */
				/* get new extensions if needed */
  if (ret && flags) nntp_extensions (stream,(mb->secflag ? AU_SECURE : NIL) |
				     (mb->authuser[0] ? AU_AUTHUSER : NIL));
  return ret;
}

/* Get challenge to authenticator in binary
 * Accepts: stream
 *	    pointer to returned size
 * Returns: challenge or NIL if not challenge
 */

void *nntp_challenge (void *s,unsigned long *len)
{
  char tmp[MAILTMPLEN];
  void *ret = NIL;
  SENDSTREAM *stream = (SENDSTREAM *) s;
  if ((stream->replycode == NNTPCHALLENGE) &&
      !(ret = rfc822_base64 ((unsigned char *) stream->reply + 4,
			     strlen (stream->reply + 4),len))) {
    sprintf (tmp,"NNTP SERVER BUG (invalid challenge): %.80s",stream->reply+4);
    mm_log (tmp,ERROR);
  }
  return ret;
}


/* Send authenticator response in BASE64
 * Accepts: MAIL stream
 *	    string to send
 *	    length of string
 * Returns: T, always
 */

long nntp_response (void *s,char *response,unsigned long size)
{
  SENDSTREAM *stream = (SENDSTREAM *) s;
  unsigned long i,j;
  char *t,*u;
  if (response) {		/* make CRLFless BASE64 string */
    if (size) {
      for (t = (char *) rfc822_binary ((void *) response,size,&i),u = t,j = 0;
	   j < i; j++) if (t[j] > ' ') *u++ = t[j];
      *u = '\0';		/* tie off string */
      i = nntp_send_work (stream,t,NIL);
      fs_give ((void **) &t);
    }
    else i = nntp_send_work (stream,"",NIL);
  }
  else {			/* abort requested */
    i = nntp_send_work (stream,"*",NIL);
    stream->saslcancel = T;	/* mark protocol-requested SASL cancel */
  }
  return LONGT;
}

/* NNTP get reply
 * Accepts: SEND stream
 * Returns: reply code
 */

long nntp_reply (SENDSTREAM *stream)
{
				/* flush old reply */
  if (stream->reply) fs_give ((void **) &stream->reply);
  				/* get reply */
  if (!(stream->reply = net_getline (stream->netstream)))
    return nntp_fake (stream,"NNTP connection broken (response)");
  if (stream->debug) mm_dlog (stream->reply);
				/* handle continuation by recursion */
  if (stream->reply[3] == '-') return nntp_reply (stream);
				/* return response code */
  return stream->replycode = atol (stream->reply);
}


/* NNTP set fake error
 * Accepts: SEND stream
 *	    error text
 * Returns: error code
 */

long nntp_fake (SENDSTREAM *stream,char *text)
{
  if (stream->netstream) {	/* close net connection if still open */
    net_close (stream->netstream);
    stream->netstream = NIL;
  }
				/* flush any old reply */
  if (stream->reply) fs_give ((void **) &stream->reply);
  				/* set up pseudo-reply string */
  stream->reply = (char *) fs_get (20+strlen (text));
  sprintf (stream->reply,"%ld %s",NNTPSOFTFATAL,text);
  return NNTPSOFTFATAL;		/* return error code */
}

/* NNTP filter mail
 * Accepts: stream
 *	    string
 * Returns: T on success, NIL on failure
 */

long nntp_soutr (void *stream,char *s)
{
  char c,*t;
				/* "." on first line */
  if (s[0] == '.') net_soutr (stream,".");
				/* find lines beginning with a "." */
  while (t = strstr (s,"\015\012.")) {
    c = *(t += 3);		/* remember next character after "." */
    *t = '\0';			/* tie off string */
				/* output prefix */
    if (!net_soutr (stream,s)) return NIL;
    *t = c;			/* restore delimiter */
    s = t - 1;			/* push pointer up to the "." */
  }
				/* output remainder of text */
  return *s ? net_soutr (stream,s) : T;
}
