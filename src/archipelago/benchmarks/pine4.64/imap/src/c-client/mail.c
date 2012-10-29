/*
 * Program:	Mailbox Access routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	22 November 1989
 * Last Edited:	15 September 2005
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
#include <time.h>
#include "misc.h"
#include "rfc822.h"
#include "utf8.h"
#include "smtp.h"

char *UW_copyright = "The IMAP toolkit provided in this Distribution is\nCopyright 1988-2005 University of Washington.\nThe full text of our legal notices is contained in the file called\nCPYRIGHT, included with this Distribution.\n";


/* c-client global data */

				/* list of mail drivers */
static DRIVER *maildrivers = NIL;
				/* list of authenticators */
static AUTHENTICATOR *mailauthenticators = NIL;
				/* SSL driver pointer */
static NETDRIVER *mailssldriver = NIL;
				/* pointer to alternate gets function */
static mailgets_t mailgets = NIL;
				/* pointer to read progress function */
static readprogress_t mailreadprogress = NIL;
				/* mail cache manipulation function */
static mailcache_t mailcache = mm_cache;
				/* RFC-822 output generator */
static rfc822out_t mail822out = NIL;
				/* SMTP verbose callback */
static smtpverbose_t mailsmtpverbose = mm_dlog;
				/* proxy copy routine */
static mailproxycopy_t mailproxycopy = NIL;
				/* RFC-822 external line parse */
static parseline_t mailparseline = NIL;
				/* RFC-822 external phrase parser */
static parsephrase_t mailparsephrase = NIL;
				/* newsrc file name decision function */
static newsrcquery_t mailnewsrcquery = NIL;
				/* ACL results callback */
static getacl_t mailaclresults = NIL;
				/* list rights results callback */
static listrights_t maillistrightsresults = NIL;
				/* my rights results callback */
static myrights_t mailmyrightsresults = NIL;
				/* quota results callback */
static quota_t mailquotaresults = NIL;
				/* quota root results callback */
static quotaroot_t mailquotarootresults = NIL;
				/* sorted results callback */
static sortresults_t mailsortresults = NIL;
				/* threaded results callback */
static threadresults_t mailthreadresults = NIL;
				/* free elt extra stuff callback */
static freeeltsparep_t mailfreeeltsparep = NIL;
				/* free envelope extra stuff callback */
static freeenvelopesparep_t mailfreeenvelopesparep = NIL;
				/* free body extra stuff callback */
static freebodysparep_t mailfreebodysparep = NIL;
				/* free stream extra stuff callback */
static freestreamsparep_t mailfreestreamsparep = NIL;
				/* SSL start routine */
static sslstart_t mailsslstart = NIL;
				/* SSL certificate query */
static sslcertificatequery_t mailsslcertificatequery = NIL;
				/* SSL failure notify */
static sslfailure_t mailsslfailure = NIL;
static kinit_t mailkinit = NIL;	/* application kinit callback */
				/* snarf interval */
static long mailsnarfinterval = 60;
				/* snarf preservation */
static long mailsnarfpreserve = NIL;
				/* newsrc name uses canonical host */
static long mailnewsrccanon = LONGT;
				/* note network sent command */
static sendcommand_t mailsendcommand = NIL;

				/* supported threaders */
static THREADER mailthreadordsub = {
  "ORDEREDSUBJECT",mail_thread_orderedsubject,NIL
};
static THREADER mailthreadlist = {
  "REFERENCES",mail_thread_references,&mailthreadordsub
};

				/* server name */
static char *servicename = "unknown";
static int expungeatping = T;	/* mail_ping() may call mm_expunged() */
static int trysslfirst = NIL;	/* always try SSL first */
static int notimezones = NIL;	/* write timezones in "From " header */
static int trustdns = T;	/* do DNS canonicalization */
static int saslusesptrname = T;	/* SASL uses name from DNS PTR lookup */
				/* trustdns also must be set */
static int debugsensitive = NIL;/* debug telemetry includes sensitive data */

/* Default mail cache handler
 * Accepts: pointer to cache handle
 *	    message number
 *	    caching function
 * Returns: cache data
 */

void *mm_cache (MAILSTREAM *stream,unsigned long msgno,long op)
{
  size_t n;
  void *ret = NIL;
  unsigned long i;
  switch ((int) op) {		/* what function? */
  case CH_INIT:			/* initialize cache */
    if (stream->cache) {	/* flush old cache contents */
      while (stream->cachesize) {
	mm_cache (stream,stream->cachesize,CH_FREE);
	mm_cache (stream,stream->cachesize--,CH_FREESORTCACHE);
      }
      fs_give ((void **) &stream->cache);
      fs_give ((void **) &stream->sc);
      stream->nmsgs = 0;	/* can't have any messages now */
    }
    break;
  case CH_SIZE:			/* (re-)size the cache */
    if (!stream->cache)	{	/* have a cache already? */
				/* no, create new cache */
      n = (stream->cachesize = msgno + CACHEINCREMENT) * sizeof (void *);
      stream->cache = (MESSAGECACHE **) memset (fs_get (n),0,n);
      stream->sc = (SORTCACHE **) memset (fs_get (n),0,n);
    }
				/* is existing cache size large neough */
    else if (msgno > stream->cachesize) {
      i = stream->cachesize;	/* remember old size */
      n = (stream->cachesize = msgno + CACHEINCREMENT) * sizeof (void *);
      fs_resize ((void **) &stream->cache,n);
      fs_resize ((void **) &stream->sc,n);
      while (i < stream->cachesize) {
	stream->cache[i] = NIL;
	stream->sc[i++] = NIL;
      }
    }
    break;

  case CH_MAKEELT:		/* return elt, make if necessary */
    if (!stream->cache[msgno - 1])
      stream->cache[msgno - 1] = mail_new_cache_elt (msgno);
				/* falls through */
  case CH_ELT:			/* return elt */
    ret = (void *) stream->cache[msgno - 1];
    break;
  case CH_SORTCACHE:		/* return sortcache entry, make if needed */
    if (!stream->sc[msgno - 1]) stream->sc[msgno - 1] =
      (SORTCACHE *) memset (fs_get (sizeof (SORTCACHE)),0,sizeof (SORTCACHE));
    ret = (void *) stream->sc[msgno - 1];
    break;
  case CH_FREE:			/* free elt */
    mail_free_elt (&stream->cache[msgno - 1]);
    break;
  case CH_FREESORTCACHE:
    if (stream->sc[msgno - 1]) {
      if (stream->sc[msgno - 1]->from)
	fs_give ((void **) &stream->sc[msgno - 1]->from);
      if (stream->sc[msgno - 1]->to)
	fs_give ((void **) &stream->sc[msgno - 1]->to);
      if (stream->sc[msgno - 1]->cc)
	fs_give ((void **) &stream->sc[msgno - 1]->cc);
      if (stream->sc[msgno - 1]->subject)
	fs_give ((void **) &stream->sc[msgno - 1]->subject);
      if (stream->sc[msgno - 1]->original_subject)
	fs_give ((void **) &stream->sc[msgno - 1]->original_subject);
      if (stream->sc[msgno - 1]->unique &&
	  (stream->sc[msgno - 1]->unique != stream->sc[msgno - 1]->message_id))
	fs_give ((void **) &stream->sc[msgno - 1]->unique);
      if (stream->sc[msgno - 1]->message_id)
	fs_give ((void **) &stream->sc[msgno - 1]->message_id);
      if (stream->sc[msgno - 1]->references)
	mail_free_stringlist (&stream->sc[msgno - 1]->references);
      fs_give ((void **) &stream->sc[msgno - 1]);
    }
    break;
  case CH_EXPUNGE:		/* expunge cache slot */
    for (i = msgno - 1; msgno < stream->nmsgs; i++,msgno++) {
      if (stream->cache[i] = stream->cache[msgno])
	stream->cache[i]->msgno = msgno;
      stream->sc[i] = stream->sc[msgno];
    }
    stream->cache[i] = NIL;	/* top of cache goes away */
    stream->sc[i] = NIL;
    break;
  default:
    fatal ("Bad mm_cache op");
    break;
  }
  return ret;
}

/* Dummy string driver for complete in-memory strings */

static void mail_string_init (STRING *s,void *data,unsigned long size);
static char mail_string_next (STRING *s);
static void mail_string_setpos (STRING *s,unsigned long i);

STRINGDRIVER mail_string = {
  mail_string_init,		/* initialize string structure */
  mail_string_next,		/* get next byte in string structure */
  mail_string_setpos		/* set position in string structure */
};


/* Initialize mail string structure for in-memory string
 * Accepts: string structure
 *	    pointer to string
 *	    size of string
 */

static void mail_string_init (STRING *s,void *data,unsigned long size)
{
				/* set initial string pointers */
  s->chunk = s->curpos = (char *) (s->data = data);
				/* and sizes */
  s->size = s->chunksize = s->cursize = size;
  s->data1 = s->offset = 0;	/* never any offset */
}


/* Get next character from string
 * Accepts: string structure
 * Returns: character, string structure chunk refreshed
 */

static char mail_string_next (STRING *s)
{
  return *s->curpos++;		/* return the last byte */
}


/* Set string pointer position
 * Accepts: string structure
 *	    new position
 */

static void mail_string_setpos (STRING *s,unsigned long i)
{
  s->curpos = s->chunk + i;	/* set new position */
  s->cursize = s->chunksize - i;/* and new size */
}

/* Mail routines
 *
 *  mail_xxx routines are the interface between this module and the outside
 * world.  Only these routines should be referenced by external callers.
 *
 *  Note that there is an important difference between a "sequence" and a
 * "message #" (msgno).  A sequence is a string representing a sequence in
 * {"n", "n:m", or combination separated by commas} format, whereas a msgno
 * is a single integer.
 *
 */

/* Mail link driver
 * Accepts: driver to add to list
 */

void mail_link (DRIVER *driver)
{
  DRIVER **d = &maildrivers;
  while (*d) d = &(*d)->next;	/* find end of list of drivers */
  *d = driver;			/* put driver at the end */
  driver->next = NIL;		/* this driver is the end of the list */
}

/* Mail manipulate driver parameters
 * Accepts: mail stream
 *	    function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *mail_parameters (MAILSTREAM *stream,long function,void *value)
{
  void *r,*ret = NIL;
  DRIVER *d;
  AUTHENTICATOR *a;
  switch ((int) function) {
  case SET_INBOXPATH:
    fatal ("SET_INBOXPATH not permitted");
  case GET_INBOXPATH:
    if ((stream || (stream = mail_open (NIL,"INBOX",OP_PROTOTYPE))) &&
	stream->dtb->parameters)
      ret = (*stream->dtb->parameters) (function,value);
    break;
  case SET_THREADERS:
    fatal ("SET_THREADERS not permitted");
  case GET_THREADERS:		/* use stream dtb instead of global */
    ret = (stream && stream->dtb) ?
				/* KLUDGE ALERT: note stream passed as value */
      (*stream->dtb->parameters) (function,stream) : (void *) &mailthreadlist;
    break;
  case SET_NAMESPACE:
    fatal ("SET_NAMESPACE not permitted");
    break;
  case SET_NEWSRC:		/* too late on open stream */
    if (stream && (stream != ((*stream->dtb->open) (NIL))))
      fatal ("SET_NEWSRC not permitted");
    else ret = env_parameters (function,value);
    break;
  case GET_NAMESPACE:
  case GET_NEWSRC:		/* use stream dtb instead of environment */
    ret = (stream && stream->dtb) ?
				/* KLUDGE ALERT: note stream passed as value */
      (*stream->dtb->parameters) (function,stream) :
	env_parameters (function,value);
    break;
  case ENABLE_DEBUG:
    fatal ("ENABLE_DEBUG not permitted");
  case DISABLE_DEBUG:
    fatal ("DISABLE_DEBUG not permitted");

  case SET_DRIVERS:
    fatal ("SET_DRIVERS not permitted");
  case GET_DRIVERS:		/* always return global */
    ret = (void *) maildrivers;
    break;
  case SET_DRIVER:
    fatal ("SET_DRIVER not permitted");
  case GET_DRIVER:
    for (d = maildrivers; d && compare_cstring (d->name,(char *) value);
	 d = d->next);
    ret = (void *) d;
    break;
  case ENABLE_DRIVER:
    for (d = maildrivers; d && compare_cstring (d->name,(char *) value);
	 d = d->next);
    if (ret = (void *) d) d->flags &= ~DR_DISABLE;
    break;
  case DISABLE_DRIVER:
    for (d = maildrivers; d && compare_cstring (d->name,(char *) value);
	 d = d->next);
    if (ret = (void *) d) d->flags |= DR_DISABLE;
    break;
  case ENABLE_AUTHENTICATOR:	/* punt on this for the nonce */
    fatal ("ENABLE_AUTHENTICATOR not permitted");
  case DISABLE_AUTHENTICATOR:
    for (a = mailauthenticators;/* scan authenticators */
	 a && compare_cstring (a->name,(char *) value); a = a->next);
    if (a) {			/* if authenticator name found */
      a->client = NIL;		/* blow it away */
      a->server = NIL;
      ret = (void *) a;
    }
    break;

  case SET_GETS:
    mailgets = (mailgets_t) value;
  case GET_GETS:
    ret = (void *) mailgets;
    break;
  case SET_READPROGRESS:
    mailreadprogress = (readprogress_t) value;
  case GET_READPROGRESS:
    ret = (void *) mailreadprogress;
    break;
  case SET_CACHE:
    mailcache = (mailcache_t) value;
  case GET_CACHE:
    ret = (void *) mailcache;
    break;
  case SET_RFC822OUTPUT:
    mail822out = (rfc822out_t) value;
  case GET_RFC822OUTPUT:
    ret = (void *) mail822out;
    break;
  case SET_SMTPVERBOSE:
    mailsmtpverbose = (smtpverbose_t) value;
  case GET_SMTPVERBOSE:
    ret = (void *) mailsmtpverbose;
    break;
  case SET_MAILPROXYCOPY:
    mailproxycopy = (mailproxycopy_t) value;
  case GET_MAILPROXYCOPY:
    ret = (void *) mailproxycopy;
    break;
  case SET_PARSELINE:
    mailparseline = (parseline_t) value;
  case GET_PARSELINE:
    ret = (void *) mailparseline;
    break;
  case SET_PARSEPHRASE:
    mailparsephrase = (parsephrase_t) value;
  case GET_PARSEPHRASE:
    ret = (void *) mailparsephrase;
    break;
  case SET_NEWSRCQUERY:
    mailnewsrcquery = (newsrcquery_t) value;
  case GET_NEWSRCQUERY:
    ret = (void *) mailnewsrcquery;
    break;
  case SET_NEWSRCCANONHOST:
    mailnewsrccanon = (long) value;
  case GET_NEWSRCCANONHOST:
    ret = (void *) mailnewsrccanon;
    break;

  case SET_FREEENVELOPESPAREP:
    mailfreeenvelopesparep = (freeenvelopesparep_t) value;
  case GET_FREEENVELOPESPAREP:
    ret = (void *) mailfreeenvelopesparep;
    break;
  case SET_FREEELTSPAREP:
    mailfreeeltsparep = (freeeltsparep_t) value;
  case GET_FREEELTSPAREP:
    ret = (void *) mailfreeeltsparep;
    break;
  case SET_FREESTREAMSPAREP:
    mailfreestreamsparep = (freestreamsparep_t) value;
  case GET_FREESTREAMSPAREP:
    ret = (void *) mailfreestreamsparep;
    break;
  case SET_FREEBODYSPAREP:
    mailfreebodysparep = (freebodysparep_t) value;
  case GET_FREEBODYSPAREP:
    ret = (void *) mailfreebodysparep;
    break;

  case SET_SSLSTART:
    mailsslstart = (sslstart_t) value;
  case GET_SSLSTART:
    ret = (void *) mailsslstart;
    break;
  case SET_SSLCERTIFICATEQUERY:
    mailsslcertificatequery = (sslcertificatequery_t) value;
  case GET_SSLCERTIFICATEQUERY:
    ret = (void *) mailsslcertificatequery;
    break;
  case SET_SSLFAILURE:
    mailsslfailure = (sslfailure_t) value;
  case GET_SSLFAILURE:
    ret = (void *) mailsslfailure;
    break;
  case SET_KINIT:
    mailkinit = (kinit_t) value;
  case GET_KINIT:
    ret = (void *) mailkinit;
    break;
  case SET_SENDCOMMAND:
    mailsendcommand = (sendcommand_t) value;
  case GET_SENDCOMMAND:
    ret = (void *) mailsendcommand;
    break;

  case SET_SERVICENAME:
    servicename = (char *) value;
  case GET_SERVICENAME:
    ret = (void *) servicename;
    break;
  case SET_EXPUNGEATPING:
    expungeatping = (value ? T : NIL);
  case GET_EXPUNGEATPING:
    ret = (void *) (expungeatping ? VOIDT : NIL);
    break;
  case SET_SORTRESULTS:
    mailsortresults = (sortresults_t) value;
  case GET_SORTRESULTS:
    ret = (void *) mailsortresults;
    break;
  case SET_THREADRESULTS:
    mailthreadresults = (threadresults_t) value;
  case GET_THREADRESULTS:
    ret = (void *) mailthreadresults;
    break;
  case SET_SSLDRIVER:
    mailssldriver = (NETDRIVER *) value;
  case GET_SSLDRIVER:
    ret = (void *) mailssldriver;
    break;
  case SET_TRYSSLFIRST:
    trysslfirst = (value ? T : NIL);
  case GET_TRYSSLFIRST:
    ret = (void *) (trysslfirst ? VOIDT : NIL);
    break;
  case SET_NOTIMEZONES:
    notimezones = (value ? T : NIL);
  case GET_NOTIMEZONES:
    ret = (void *) (notimezones ? VOIDT : NIL);
    break;
  case SET_TRUSTDNS:
    trustdns = (value ? T : NIL);
  case GET_TRUSTDNS:
    ret = (void *) (trustdns ? VOIDT : NIL);
    break;
  case SET_SASLUSESPTRNAME:
    saslusesptrname = (value ? T : NIL);
  case GET_SASLUSESPTRNAME:
    ret = (void *) (saslusesptrname ? VOIDT : NIL);
    break;
  case SET_DEBUGSENSITIVE:
    debugsensitive = (value ? T : NIL);
  case GET_DEBUGSENSITIVE:
    ret = (void *) (debugsensitive ? VOIDT : NIL);
    break;

  case SET_ACL:
    mailaclresults = (getacl_t) value;
  case GET_ACL:
    ret = (void *) mailaclresults;
    break;
  case SET_LISTRIGHTS:
    maillistrightsresults = (listrights_t) value;
  case GET_LISTRIGHTS:
    ret = (void *) maillistrightsresults;
    break;
  case SET_MYRIGHTS:
    mailmyrightsresults = (myrights_t) value;
  case GET_MYRIGHTS:
    ret = (void *) mailmyrightsresults;
    break;
  case SET_QUOTA:
    mailquotaresults = (quota_t) value;
  case GET_QUOTA:
    ret = (void *) mailquotaresults;
    break;
  case SET_QUOTAROOT:
    mailquotarootresults = (quotaroot_t) value;
  case GET_QUOTAROOT:
    ret = (void *) mailquotarootresults;
    break;
  case SET_SNARFINTERVAL:
    mailsnarfinterval = (long) value;
  case GET_SNARFINTERVAL:
    ret = (void *) mailsnarfinterval;
    break;
  case SET_SNARFPRESERVE:
    mailsnarfpreserve = (long) value;
  case GET_SNARFPRESERVE:
    ret = (void *) mailsnarfpreserve;
    break;
  case SET_SNARFMAILBOXNAME:
    if (stream) {		/* have a stream? */
      if (stream->snarf.name) fs_give ((void **) &stream->snarf.name);
      stream->snarf.name = cpystr ((char *) value);
    }
    else fatal ("SET_SNARFMAILBOXNAME with no stream");
  case GET_SNARFMAILBOXNAME:
    if (stream) ret = (void *) stream->snarf.name;
    break;
  default:
    if (r = smtp_parameters (function,value)) ret = r;
    if (r = env_parameters (function,value)) ret = r;
    if (r = tcp_parameters (function,value)) ret = r;
    if (stream && stream->dtb) {/* if have stream, do for its driver only */
      if (r = (*stream->dtb->parameters) (function,value)) ret = r;
    }
				/* else do all drivers */
    else for (d = maildrivers; d; d = d->next)
      if (r = (d->parameters) (function,value)) ret = r;
    break;
  }
  return ret;
}

/* Mail validate mailbox name
 * Accepts: MAIL stream
 *	    mailbox name
 *	    purpose string for error message
 * Return: driver factory on success, NIL on failure
 */

DRIVER *mail_valid (MAILSTREAM *stream,char *mailbox,char *purpose)
{
  char tmp[MAILTMPLEN];
  DRIVER *factory = NIL;
				/* never allow names with newlines */
  if (strpbrk (mailbox,"\015\012")) {
    if (purpose) {		/* if want an error message */
      sprintf (tmp,"Can't %s with such a name",purpose);
      MM_LOG (tmp,ERROR);
    }
    return NIL;
  }
				/* validate name, find driver factory */
  if (strlen (mailbox) < (NETMAXHOST+(NETMAXUSER*2)+NETMAXMBX+NETMAXSRV+50))
    for (factory = maildrivers; factory && 
	 ((factory->flags & DR_DISABLE) ||
	  ((factory->flags & DR_LOCAL) && (*mailbox == '{')) ||
	  !(*factory->valid) (mailbox));
	 factory = factory->next);
				/* validate factory against non-dummy stream */
  if (factory && stream && (stream->dtb != factory) &&
      strcmp (stream->dtb->name,"dummy"))
				/* factory invalid; if dummy, use stream */
    factory = strcmp (factory->name,"dummy") ? NIL : stream->dtb;
  if (!factory && purpose) {	/* if want an error message */
    sprintf (tmp,"Can't %s %.80s: %s",purpose,mailbox,(*mailbox == '{') ?
	     "invalid remote specification" : "no such mailbox");
    MM_LOG (tmp,ERROR);
  }
  return factory;		/* return driver factory */
}

/* Mail validate network mailbox name
 * Accepts: mailbox name
 *	    mailbox driver to validate against
 *	    pointer to where to return host name if non-NIL
 *	    pointer to where to return mailbox name if non-NIL
 * Returns: driver on success, NIL on failure
 */

DRIVER *mail_valid_net (char *name,DRIVER *drv,char *host,char *mailbox)
{
  NETMBX mb;
  if (!mail_valid_net_parse (name,&mb) || strcmp (mb.service,drv->name))
    return NIL;
  if (host) strcpy (host,mb.host);
  if (mailbox) strcpy (mailbox,mb.mailbox);
  return drv;
}


/* Mail validate network mailbox name
 * Accepts: mailbox name
 *	    NETMBX structure to return values
 * Returns: T on success, NIL on failure
 */

long mail_valid_net_parse (char *name,NETMBX *mb)
{
  return mail_valid_net_parse_work (name,mb,"imap");
}

/* Mail validate network mailbox name worker routine
 * Accepts: mailbox name
 *	    NETMBX structure to return values
 *	    default service
 * Returns: T on success, NIL on failure
 */

long mail_valid_net_parse_work (char *name,NETMBX *mb,char *service)
{
  int i,j;
  char c,*s,*t,*v,tmp[MAILTMPLEN],arg[MAILTMPLEN];
				/* initialize structure */
  memset (mb,'\0',sizeof (NETMBX));
				/* must have host specification */
  if (*name++ != '{') return NIL;
  if (*name == '[') {		/* if domain literal, find its ending */
    if (!((v = strpbrk (name,"]}")) && (*v++ == ']'))) return NIL;
  }
				/* find end of host name */
  else if (!(v = strpbrk (name,"/:}"))) return NIL;
				/* validate length, find mailbox part */
  if (!((i = v - name) && (i < NETMAXHOST) && (t = strchr (v,'}')) &&
	((j = t - v) < MAILTMPLEN) && (strlen (t+1) < (size_t) NETMAXMBX)))
    return NIL;			/* invalid mailbox */
  strncpy (mb->host,name,i);	/* set host name */
  strncpy (mb->orighost,name,i);
  mb->host[i] = mb->orighost[i] = '\0';
  strcpy (mb->mailbox,t+1);	/* set mailbox name */
  if (t - v) {			/* any switches or port specification? */
    strncpy (t = tmp,v,j);	/* copy it */
    tmp[j] = '\0';		/* tie it off */
    c = *t++;			/* get first delimiter */
    do switch (c) {		/* act based upon the character */
    case ':':			/* port specification */
      if (mb->port || !(mb->port = strtoul (t,&t,10))) return NIL;
      c = t ? *t++ : '\0';	/* get delimiter, advance pointer */
      break;
    case '/':			/* switch */
				/* find delimiter */
      if (t = strpbrk (s = t,"/:=")) {
	c = *t;			/* remember delimiter for later */
	*t++ = '\0';		/* tie off switch name */
      }
      else c = '\0';		/* no delimiter */
      if (c == '=') {		/* parse switches which take arguments */
	if (*t == '"') {	/* quoted string? */
	  for (v = arg,i = 0,++t; (c = *t++) != '"';) {
	    if (!c) return NIL;	/* unterminated string */
				/* quote next character */
	    if (c == '\\') c = *t++;
	    if (!c) return NIL;	/* can't quote NUL either */
	    arg[i++] = c;
	  }
	  c = *t++;		/* remember delimiter for later */
	  arg[i] = '\0';	/* tie off argument */
	}
	else {			/* non-quoted argument */
	  if (t = strpbrk (v = t,"/:")) {
	    c = *t;		/* remember delimiter for later */
	    *t++ = '\0';	/* tie off switch name */
	  }
	  else c = '\0';	/* no delimiter */
	  i = strlen (v);	/* length of argument */
	}
	if (!compare_cstring (s,"service") && (i < NETMAXSRV) && !*mb->service)
	  lcase (strcpy (mb->service,v));
	else if (!compare_cstring (s,"user") && (i < NETMAXUSER) && !*mb->user)
	  strcpy (mb->user,v);
	else if (!compare_cstring (s,"authuser") && (i < NETMAXUSER) &&
		 !*mb->authuser) strcpy (mb->authuser,v);
	else return NIL;
      }

      else {			/* non-argument switch */
	if (!compare_cstring (s,"anonymous")) mb->anoflag = T;
	else if (!compare_cstring (s,"debug")) mb->dbgflag = T;
	else if (!compare_cstring (s,"readonly")) mb->readonlyflag = T;
	else if (!compare_cstring (s,"secure")) mb->secflag = T;
	else if (!compare_cstring (s,"norsh")) mb->norsh = T;
	else if (!compare_cstring (s,"loser")) mb->loser = T;
	else if (!compare_cstring (s,"tls") && !mb->notlsflag) mb->tlsflag = T;
	else if (!compare_cstring (s,"notls") && !mb->tlsflag)
	  mb->notlsflag = T;
	else if (!compare_cstring (s,"tryssl"))
	  mb->trysslflag = mailssldriver? T : NIL;
	else if (mailssldriver && !compare_cstring (s,"ssl")) mb->sslflag = T;
	else if (mailssldriver && !compare_cstring (s,"novalidate-cert"))
	  mb->novalidate = T;
				/* hack for compatibility with the past */
	else if (mailssldriver && !compare_cstring (s,"validate-cert"));
				/* service switches below here */
	else if (*mb->service) return NIL;
	else if (!compare_cstring (s,"imap") ||
		 !compare_cstring (s,"nntp") ||
		 !compare_cstring (s,"pop3") ||
		 !compare_cstring (s,"smtp") ||
		 !compare_cstring (s,"submit"))
	  lcase (strcpy (mb->service,s));
	else if (!compare_cstring (s,"imap2") ||
		 !compare_cstring (s,"imap2bis") ||
		 !compare_cstring (s,"imap4") ||
		 !compare_cstring (s,"imap4rev1"))
	  strcpy (mb->service,"imap");
	else if (!compare_cstring (s,"pop"))
	  strcpy (mb->service,"pop3");
	else return NIL;	/* invalid non-argument switch */
      }
      break;
    default:			/* anything else is bogus */
      return NIL;
    } while (c);		/* see if anything more to parse */
  }
				/* default mailbox name */
  if (!*mb->mailbox) strcpy (mb->mailbox,"INBOX");
				/* default service name */
  if (!*mb->service) strcpy (mb->service,service);
				/* /norsh only valid if imap */
  if (mb->norsh && strcmp (mb->service,"imap")) return NIL;
  return T;
}

/* Mail scan mailboxes for string
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    contents to search
 */

void mail_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  int remote = ((*pat == '{') || (ref && *ref == '{'));
  DRIVER *d;
  if (ref && (strlen (ref) > NETMAXMBX)) {
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Invalid LIST reference specification: %.80s",ref);
    MM_LOG (tmp,ERROR);
    return;
  }
  if (strlen (pat) > NETMAXMBX) {
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Invalid LIST pattern specification: %.80s",pat);
    MM_LOG (tmp,ERROR);
    return;
  }
  if (*pat == '{') ref = NIL;	/* ignore reference if pattern is remote */
  if (stream) {			/* if have a stream, do it for that stream */
    if ((d = stream->dtb) && d->scan &&
	!(((d->flags & DR_LOCAL) && remote)))
      (*d->scan) (stream,ref,pat,contents);
  }
				/* otherwise do for all DTB's */
  else for (d = maildrivers; d; d = d->next)
    if (d->scan && !((d->flags & DR_DISABLE) ||
		     ((d->flags & DR_LOCAL) && remote)))
      (d->scan) (NIL,ref,pat,contents);
}

/* Mail list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void mail_list (MAILSTREAM *stream,char *ref,char *pat)
{
  int remote = ((*pat == '{') || (ref && *ref == '{'));
  DRIVER *d = maildrivers;
  if (ref && (strlen (ref) > NETMAXMBX)) {
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Invalid LIST reference specification: %.80s",ref);
    MM_LOG (tmp,ERROR);
    return;
  }
  if (strlen (pat) > NETMAXMBX) {
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Invalid LIST pattern specification: %.80s",pat);
    MM_LOG (tmp,ERROR);
    return;
  }
  if (*pat == '{') ref = NIL;	/* ignore reference if pattern is remote */
  if (stream && stream->dtb) {	/* if have a stream, do it for that stream */
    if (!(((d = stream->dtb)->flags & DR_LOCAL) && remote))
      (*d->list) (stream,ref,pat);
  }
				/* otherwise do for all DTB's */
  else do if (!((d->flags & DR_DISABLE) ||
		((d->flags & DR_LOCAL) && remote)))
    (d->list) (NIL,ref,pat);
  while (d = d->next);		/* until at the end */
}

/* Mail list subscribed mailboxes
 * Accepts: mail stream
 *	    pattern to search
 */

void mail_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  int remote = ((*pat == '{') || (ref && *ref == '{'));
  DRIVER *d = maildrivers;
  if (ref && (strlen (ref) > NETMAXMBX)) {
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Invalid LSUB reference specification: %.80s",ref);
    MM_LOG (tmp,ERROR);
    return;
  }
  if (strlen (pat) > NETMAXMBX) {
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Invalid LSUB pattern specification: %.80s",pat);
    MM_LOG (tmp,ERROR);
    return;
  }
  if (*pat == '{') ref = NIL;	/* ignore reference if pattern is remote */
  if (stream && stream->dtb) {	/* if have a stream, do it for that stream */
    if (!(((d = stream->dtb)->flags & DR_LOCAL) && remote))
      (*d->lsub) (stream,ref,pat);
  }
				/* otherwise do for all DTB's */
  else do if (!((d->flags & DR_DISABLE) ||
		((d->flags & DR_LOCAL) && remote)))
    (d->lsub) (NIL,ref,pat);
  while (d = d->next);		/* until at the end */
}

/* Mail subscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to add to subscription list
 * Returns: T on success, NIL on failure
 */

long mail_subscribe (MAILSTREAM *stream,char *mailbox)
{
  DRIVER *factory = mail_valid (stream,mailbox,"subscribe to mailbox");
  return factory ?
    (factory->subscribe ?
     (*factory->subscribe) (stream,mailbox) : sm_subscribe (mailbox)) : NIL;
}


/* Mail unsubscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to delete from subscription list
 * Returns: T on success, NIL on failure
 */

long mail_unsubscribe (MAILSTREAM *stream,char *mailbox)
{
  DRIVER *factory = mail_valid (stream,mailbox,NIL);
  return (factory && factory->unsubscribe) ?
    (*factory->unsubscribe) (stream,mailbox) : sm_unsubscribe (mailbox);
}

/* Mail create mailbox
 * Accepts: mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long mail_create (MAILSTREAM *stream,char *mailbox)
{
  MAILSTREAM *ts;
  char *s,*t,tmp[MAILTMPLEN];
  size_t i;
  DRIVER *d;
				/* never allow names with newlines */
  if (s = strpbrk (mailbox,"\015\012")) {
    MM_LOG ("Can't create mailbox with such a name",ERROR);
    return NIL;
  }
  if (strlen (mailbox) >= (NETMAXHOST+(NETMAXUSER*2)+NETMAXMBX+NETMAXSRV+50)) {
    sprintf (tmp,"Can't create %.80s: %s",mailbox,(*mailbox == '{') ?
	     "invalid remote specification" : "no such mailbox");
    MM_LOG (tmp,ERROR);
    return NIL;
  }
				/* create of INBOX invalid */
  if (!compare_cstring (mailbox,"INBOX")) {
    MM_LOG ("Can't create INBOX",ERROR);
    return NIL;
  }
  for (s = mailbox; *s; s++) {	/* make sure valid name */
    if (*s & 0x80) {		/* reserved for future use with UTF-8 */
      MM_LOG ("Can't create mailbox name with 8-bit character",ERROR);
      return NIL;
    }
				/* validate modified UTF-7 */
    else if (*s == '&') while (*++s != '-') switch (*s) {
      case '\0':
	sprintf (tmp,"Can't create unterminated modified UTF-7 name: %.80s",
		 mailbox);
	MM_LOG (tmp,ERROR);
	return NIL;
      default:			/* must be alphanumeric */
	if (!isalnum (*s)) {
	  sprintf (tmp,"Can't create invalid modified UTF-7 name: %.80s",
		   mailbox);
	  MM_LOG (tmp,ERROR);
	  return NIL;
	}
      case '+':			/* valid modified BASE64 */
      case ',':
	break;			/* all OK so far */
      }
  }

				/* see if special driver hack */
  if ((mailbox[0] == '#') && ((mailbox[1] == 'd') || (mailbox[1] == 'D')) &&
      ((mailbox[2] == 'r') || (mailbox[2] == 'R')) &&
      ((mailbox[3] == 'i') || (mailbox[3] == 'I')) &&
      ((mailbox[4] == 'v') || (mailbox[4] == 'V')) &&
      ((mailbox[5] == 'e') || (mailbox[5] == 'E')) &&
      ((mailbox[6] == 'r') || (mailbox[6] == 'R')) && (mailbox[7] == '.')) {
				/* copy driver until likely delimiter */
    if ((s = strpbrk (t = mailbox+8,"/\\:")) && (i = s - t)) {
      strncpy (tmp,t,i);
      tmp[i] = '\0';
    } 
    else {
      sprintf (tmp,"Can't create mailbox %.80s: bad driver syntax",mailbox);
      MM_LOG (tmp,ERROR);
      return NIL;
    }
    for (d = maildrivers; d && strcmp (d->name,tmp); d = d->next);
    if (d) mailbox = ++s;	/* skip past driver specification */
    else {
      sprintf (tmp,"Can't create mailbox %.80s: unknown driver",mailbox);
      MM_LOG (tmp,ERROR);
      return NIL;
    }
  }
				/* use stream if one given or deterministic */
  else if ((stream && stream->dtb) ||
	   (((*mailbox == '{') || (*mailbox == '#')) &&
	    (stream = mail_open (NIL,mailbox,OP_PROTOTYPE | OP_SILENT))))
    d = stream->dtb;
  else if ((*mailbox != '{') && (ts = default_proto (NIL))) d = ts->dtb;
  else {			/* failed utterly */
    sprintf (tmp,"Can't create mailbox %.80s: indeterminate format",mailbox);
    MM_LOG (tmp,ERROR);
    return NIL;
  }
  return (*d->create) (stream,mailbox);
}

/* Mail delete mailbox
 * Accepts: mail stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long mail_delete (MAILSTREAM *stream,char *mailbox)
{
  DRIVER *dtb = mail_valid (stream,mailbox,"delete mailbox");
  if (!dtb) return NIL;
  if (((mailbox[0] == 'I') || (mailbox[0] == 'i')) &&
      ((mailbox[1] == 'N') || (mailbox[1] == 'n')) &&
      ((mailbox[2] == 'B') || (mailbox[2] == 'b')) &&
      ((mailbox[3] == 'O') || (mailbox[3] == 'o')) &&
      ((mailbox[4] == 'X') || (mailbox[4] == 'x')) && !mailbox[5]) {
    MM_LOG ("Can't delete INBOX",ERROR);
    return NIL;
  }
  return SAFE_DELETE (dtb,stream,mailbox);
}


/* Mail rename mailbox
 * Accepts: mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long mail_rename (MAILSTREAM *stream,char *old,char *newname)
{
  char tmp[MAILTMPLEN];
  DRIVER *dtb = mail_valid (stream,old,"rename mailbox");
  if (!dtb) return NIL;
  if ((*old != '{') && (*old != '#') && mail_valid (NIL,newname,NIL)) {
    sprintf (tmp,"Can't rename %.80s: mailbox %.80s already exists",
	     old,newname);
    MM_LOG (tmp,ERROR);
    return NIL;
  }
  return SAFE_RENAME (dtb,stream,old,newname);
}

/* Mail status of mailbox
 * Accepts: mail stream if open on this mailbox
 *	    mailbox name
 *	    status flags
 * Returns: T on success, NIL on failure
 */

long mail_status (MAILSTREAM *stream,char *mbx,long flags)
{
  DRIVER *dtb = mail_valid (stream,mbx,"get status of mailbox");
  if (!dtb) return NIL;		/* only if valid */
  if (stream && ((dtb != stream->dtb) ||
		 ((dtb->flags & DR_LOCAL) && strcmp (mbx,stream->mailbox) &&
		  strcmp (mbx,stream->original_mailbox))))
    stream = NIL;		/* stream not suitable */
  return SAFE_STATUS (dtb,stream,mbx,flags);
}


/* Mail status of mailbox default handler
 * Accepts: mail stream
 *	    mailbox name
 *	    status flags
 * Returns: T on success, NIL on failure
 */

long mail_status_default (MAILSTREAM *stream,char *mbx,long flags)
{
  MAILSTATUS status;
  unsigned long i;
  MAILSTREAM *tstream = NIL;
				/* make temporary stream (unless this mbx) */
  if (!stream && !(stream = tstream =
		   mail_open (NIL,mbx,OP_READONLY|OP_SILENT))) return NIL;
  status.flags = flags;		/* return status values */
  status.messages = stream->nmsgs;
  status.recent = stream->recent;
  if (flags & SA_UNSEEN)	/* must search to get unseen messages */
    for (i = 1,status.unseen = 0; i <= stream->nmsgs; i++)
      if (!mail_elt (stream,i)->seen) status.unseen++;
  status.uidnext = stream->uid_last + 1;
  status.uidvalidity = stream->uid_validity;
  MM_STATUS(stream,mbx,&status);/* pass status to main program */
  if (tstream) mail_close (tstream);
  return T;			/* success */
}

/* Mail open
 * Accepts: candidate stream for recycling
 *	    mailbox name
 *	    open options
 * Returns: stream to use on success, NIL on failure
 */

MAILSTREAM *mail_open (MAILSTREAM *stream,char *name,long options)
{
  int i;
  char c,*s,tmp[MAILTMPLEN];
  NETMBX mb;
  DRIVER *d;
  switch (name[0]) {		/* see if special handling */
  case '#':			/* possible special hacks */
    if (((name[1] == 'M') || (name[1] == 'm')) &&
	((name[2] == 'O') || (name[2] == 'o')) &&
	((name[3] == 'V') || (name[3] == 'v')) &&
	((name[4] == 'E') || (name[4] == 'e')) && (c = name[5]) &&
	(s = strchr (name+6,c)) && (i = s - (name + 6)) && (i < MAILTMPLEN)) {
      if (stream = mail_open (stream,s+1,options)) {
	strncpy (tmp,name+6,i);	/* copy snarf mailbox name */
	tmp[i] = '\0';		/* tie off name */
	mail_parameters (stream,SET_SNARFMAILBOXNAME,(void *) tmp);
	stream->snarf.options = options;
	mail_ping (stream);	/* do initial snarf */
				/* punt if can't do initial snarf */
	if (!stream->snarf.time) stream = mail_close (stream);
      }
      return stream;
    }
				/* special POP hack */
    else if (((name[1] == 'P') || (name[1] == 'p')) &&
	     ((name[2] == 'O') || (name[2] == 'o')) &&
	     ((name[3] == 'P') || (name[3] == 'p')) &&
	     mail_valid_net_parse_work (name+4,&mb,"pop3") &&
	!strcmp (mb.service,"pop3") && !mb.anoflag && !mb.readonlyflag) {
      if (stream = mail_open (stream,mb.mailbox,options)) {
	sprintf (tmp,"{%.255s",mb.host);
	if (mb.port) sprintf (tmp + strlen (tmp),":%lu",mb.port);
	if (mb.user[0]) sprintf (tmp + strlen (tmp),"/user=%.64s",mb.user);
	if (mb.dbgflag) strcat (tmp,"/debug");
	if (mb.secflag) strcat (tmp,"/secure");
	if (mb.tlsflag) strcat (tmp,"/tls");
	if (mb.notlsflag) strcat (tmp,"/notls");
	if (mb.sslflag) strcat (tmp,"/ssl");
	if (mb.trysslflag) strcat (tmp,"/tryssl");
	if (mb.novalidate) strcat (tmp,"/novalidate-cert");
	strcat (tmp,"/pop3/loser}");
	mail_parameters (stream,SET_SNARFMAILBOXNAME,(void *) tmp);
	mail_ping (stream);	/* do initial snarf */
      }
      return stream;		/* return local mailbox stream */
    }

    else if ((options & OP_PROTOTYPE) &&
	     ((name[1] == 'D') || (name[1] == 'd')) &&
	     ((name[2] == 'R') || (name[2] == 'r')) &&
	     ((name[3] == 'I') || (name[3] == 'i')) &&
	     ((name[4] == 'V') || (name[4] == 'v')) &&
	     ((name[5] == 'E') || (name[5] == 'e')) &&
	     ((name[6] == 'R') || (name[6] == 'r')) && (name[7] == '.')) {
      sprintf (tmp,"%.80s",name+8);
				/* tie off name at likely delimiter */
      if (s = strpbrk (tmp,"/\\:")) *s++ = '\0';
      else {
	sprintf (tmp,"Can't resolve mailbox %.80s: bad driver syntax",name);
	MM_LOG (tmp,ERROR);
	return mail_close (stream);
      }
      for (d = maildrivers; d && compare_cstring (d->name,tmp); d = d->next);
      if (d) return (*d->open) (NIL);
      sprintf (tmp,"Can't resolve mailbox %.80s: unknown driver",name);
      MM_LOG (tmp,ERROR);
      return mail_close (stream);
    }
				/* fall through to default case */
  default:			/* not special hack (but could be # name */
    d = mail_valid (NIL,name,(options & OP_SILENT) ?
		    (char *) NIL : "open mailbox");
  }

  if (d) {			/* must have a factory */
    char *oname = cpystr (name);
    if (options & OP_PROTOTYPE) return (*d->open) (NIL);
    if (stream) {		/* recycling requested? */
      if ((stream->dtb == d) && (d->flags & DR_RECYCLE) &&
	  ((d->flags & DR_HALFOPEN) || !(options & OP_HALFOPEN)) &&
	  mail_usable_network_stream (stream,name)) {
				/* yes, checkpoint if needed */
	if (d->flags & DR_XPOINT) mail_check (stream);
	mail_free_cache(stream);/* clean up stream */
	if (stream->mailbox) fs_give ((void **) &stream->mailbox);
	if (stream->original_mailbox)
	  fs_give ((void **) &stream->original_mailbox);
				/* flush user flags */
	for (i = 0; i < NUSERFLAGS; i++)
	  if (stream->user_flags[i]) fs_give ((void **)&stream->user_flags[i]);
      }
      else {			/* stream not recycleable, babble if net */
	if (!stream->silent && stream->dtb && !(stream->dtb->flags&DR_LOCAL) &&
	    mail_valid_net_parse (stream->mailbox,&mb)) {
	  sprintf (tmp,"Closing connection to %.80s",mb.host);
	  MM_LOG (tmp,(long) NIL);
	}
				/* flush the old stream */
	stream = mail_close (stream);
      }
    }
				/* check if driver does not support halfopen */
    else if ((options & OP_HALFOPEN) && !(d->flags & DR_HALFOPEN)) {
      fs_give ((void **) &oname);
      return NIL;
    }

				/* instantiate new stream if not recycling */
    if (!stream) (*mailcache) (stream = (MAILSTREAM *)
			       memset (fs_get (sizeof (MAILSTREAM)),0,
				       sizeof (MAILSTREAM)),(long) 0,CH_INIT);
    stream->dtb = d;		/* set dispatch */
				/* set mailbox name */
    stream->mailbox = cpystr (stream->original_mailbox = oname);
				/* initialize stream flags */
    stream->inbox = stream->lock = NIL;
    stream->debug = (options & OP_DEBUG) ? T : NIL;
    stream->rdonly = (options & OP_READONLY) ? T : NIL;
    stream->anonymous = (options & OP_ANONYMOUS) ? T : NIL;
    stream->scache = (options & OP_SHORTCACHE) ? T : NIL;
    stream->silent = (options & OP_SILENT) ? T : NIL;
    stream->halfopen = (options & OP_HALFOPEN) ? T : NIL;
    stream->secure = (options & OP_SECURE) ? T : NIL;
    stream->tryssl = (options & OP_TRYSSL) ? T : NIL;
    stream->mulnewsrc = (options & OP_MULNEWSRC) ? T : NIL;
    stream->perm_seen = stream->perm_deleted = stream->perm_flagged =
      stream->perm_answered = stream->perm_draft = stream->kwd_create = NIL;
    stream->uid_nosticky = (d->flags & DR_NOSTICKY) ? T : NIL;
    stream->uid_last = 0;	/* default UID validity */
    stream->uid_validity = time (0);
				/* have driver open, flush if failed */
    if (!(*d->open) (stream)) stream = mail_close (stream);
  }
  return stream;		/* return the stream */
}

/* Mail close
 * Accepts: mail stream
 *	    close options
 * Returns: NIL, always
 */

MAILSTREAM *mail_close_full (MAILSTREAM *stream,long options)
{
  int i;
  if (stream) {			/* make sure argument given */
				/* do the driver's close action */
    if (stream->dtb) (*stream->dtb->close) (stream,options);
    if (stream->mailbox) fs_give ((void **) &stream->mailbox);
    if (stream->original_mailbox)
      fs_give ((void **) &stream->original_mailbox);
    if (stream->snarf.name) fs_give ((void *) &stream->snarf.name);
    stream->sequence++;		/* invalidate sequence */
				/* flush user flags */
    for (i = 0; i < NUSERFLAGS; i++)
      if (stream->user_flags[i]) fs_give ((void **) &stream->user_flags[i]);
    mail_free_cache (stream);	/* finally free the stream's storage */
    if (mailfreestreamsparep && stream->sparep)
      (*mailfreestreamsparep) (&stream->sparep);
    if (!stream->use) fs_give ((void **) &stream);
  }
  return NIL;
}

/* Mail make handle
 * Accepts: mail stream
 * Returns: handle
 *
 *  Handles provide a way to have multiple pointers to a stream yet allow the
 * stream's owner to nuke it or recycle it.
 */

MAILHANDLE *mail_makehandle (MAILSTREAM *stream)
{
  MAILHANDLE *handle = (MAILHANDLE *) fs_get (sizeof (MAILHANDLE));
  handle->stream = stream;	/* copy stream */
				/* and its sequence */
  handle->sequence = stream->sequence;
  stream->use++;		/* let stream know another handle exists */
  return handle;
}


/* Mail release handle
 * Accepts: Mail handle
 */

void mail_free_handle (MAILHANDLE **handle)
{
  MAILSTREAM *s;
  if (*handle) {		/* only free if exists */
				/* resign stream, flush unreferenced zombies */
    if ((!--(s = (*handle)->stream)->use) && !s->dtb) fs_give ((void **) &s);
    fs_give ((void **) handle);	/* now flush the handle */
  }
}


/* Mail get stream handle
 * Accepts: Mail handle
 * Returns: mail stream or NIL if stream gone
 */

MAILSTREAM *mail_stream (MAILHANDLE *handle)
{
  MAILSTREAM *s = handle->stream;
  return (s->dtb && (handle->sequence == s->sequence)) ? s : NIL;
}

/* Mail fetch cache element
 * Accepts: mail stream
 *	    message # to fetch
 * Returns: cache element of this message
 * Can also be used to create cache elements for new messages.
 */

MESSAGECACHE *mail_elt (MAILSTREAM *stream,unsigned long msgno)
{
  if (msgno < 1 || msgno > stream->nmsgs) {
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Bad msgno %lu in mail_elt, nmsgs = %lu, mbx=%.80s",
	     msgno,stream->nmsgs,stream->mailbox ? stream->mailbox : "???");
    fatal (tmp);
  }
  return (MESSAGECACHE *) (*mailcache) (stream,msgno,CH_MAKEELT);
}


/* Mail fetch fast information
 * Accepts: mail stream
 *	    sequence
 *	    option flags
 *
 * Generally, mail_fetch_structure is preferred
 */

void mail_fetch_fast (MAILSTREAM *stream,char *sequence,long flags)
{
  				/* do the driver's action */
  if (stream->dtb && stream->dtb->fast)
    (*stream->dtb->fast) (stream,sequence,flags);
}


/* Mail fetch flags
 * Accepts: mail stream
 *	    sequence
 *	    option flags
 */

void mail_fetch_flags (MAILSTREAM *stream,char *sequence,long flags)
{
  				/* do the driver's action */
  if (stream->dtb && stream->dtb->msgflags)
    (*stream->dtb->msgflags) (stream,sequence,flags);
}

/* Mail fetch message overview
 * Accepts: mail stream
 *	    UID sequence to fetch
 *	    pointer to overview return function
 */

void mail_fetch_overview (MAILSTREAM *stream,char *sequence,overview_t ofn)
{
  if (stream->dtb && mail_uid_sequence (stream,sequence) &&
      !(stream->dtb->overview && (*stream->dtb->overview) (stream,ofn)) &&
      mail_ping (stream))
    mail_fetch_overview_default (stream,ofn);
}


/* Mail fetch message overview using sequence numbers instead of UIDs
 * Accepts: mail stream
 *	    sequence to fetch
 *	    pointer to overview return function
 */

void mail_fetch_overview_sequence (MAILSTREAM *stream,char *sequence,
				   overview_t ofn)
{
  if (stream->dtb && mail_sequence (stream,sequence) &&
      !(stream->dtb->overview && (*stream->dtb->overview) (stream,ofn)) &&
      mail_ping (stream))
    mail_fetch_overview_default (stream,ofn);
}


/* Mail fetch message overview default handler
 * Accepts: mail stream with sequence bits lit
 *	    pointer to overview return function
 */

void mail_fetch_overview_default (MAILSTREAM *stream,overview_t ofn)
{
  MESSAGECACHE *elt;
  ENVELOPE *env;
  OVERVIEW ov;
  unsigned long i;
  ov.optional.lines = 0;
  ov.optional.xref = NIL;
  for (i = 1; i <= stream->nmsgs; i++)
    if (((elt = mail_elt (stream,i))->sequence) &&
	(env = mail_fetch_structure (stream,i,NIL,NIL)) && ofn) {
      ov.subject = env->subject;
      ov.from = env->from;
      ov.date = env->date;
      ov.message_id = env->message_id;
      ov.references = env->references;
      ov.optional.octets = elt->rfc822_size;
      (*ofn) (stream,mail_uid (stream,i),&ov,i);
    }
}

/* Mail fetch message structure
 * Accepts: mail stream
 *	    message # to fetch
 *	    pointer to return body
 *	    option flags
 * Returns: envelope of this message, body returned in body value
 *
 * Fetches the "fast" information as well
 */

ENVELOPE *mail_fetch_structure (MAILSTREAM *stream,unsigned long msgno,
				BODY **body,long flags)
{
  ENVELOPE **env;
  BODY **b;
  MESSAGECACHE *elt;
  char c,*s,*hdr;
  unsigned long hdrsize;
  STRING bs;
				/* do the driver's action if specified */
  if (stream->dtb && stream->dtb->structure)
    return (*stream->dtb->structure) (stream,msgno,body,flags);
  if (flags & FT_UID) {		/* UID form of call */
    if (msgno = mail_msgno (stream,msgno)) flags &= ~FT_UID;
    else return NIL;		/* must get UID/msgno map first */
  }
  elt = mail_elt (stream,msgno);/* get elt for real message number */
  if (stream->scache) {		/* short caching */
    if (msgno != stream->msgno){/* garbage collect if not same message */
      mail_gc (stream,GC_ENV | GC_TEXTS);
      stream->msgno = msgno;	/* this is the current message now */
    }
    env = &stream->env;		/* get pointers to envelope and body */
    b = &stream->body;
  }
  else {			/* get pointers to elt envelope and body */
    env = &elt->private.msg.env;
    b = &elt->private.msg.body;
  }

  if (stream->dtb && ((body && !*b) || !*env || (*env)->incomplete)) {
    mail_free_envelope (env);	/* flush old envelope and body */
    mail_free_body (b);
				/* see if need to fetch the whole thing */
    if (body || !elt->rfc822_size) {
      s = (*stream->dtb->header) (stream,msgno,&hdrsize,flags & ~FT_INTERNAL);
				/* make copy in case body fetch smashes it */
      hdr = (char *) memcpy (fs_get ((size_t) hdrsize+1),s,(size_t) hdrsize);
      hdr[hdrsize] = '\0';	/* tie off header */
      (*stream->dtb->text) (stream,msgno,&bs,(flags & ~FT_INTERNAL) | FT_PEEK);
      if (!elt->rfc822_size) elt->rfc822_size = hdrsize + SIZE (&bs);
      if (body)			/* only parse body if requested */
	rfc822_parse_msg (env,b,hdr,hdrsize,&bs,BADHOST,stream->dtb->flags);
      else
	rfc822_parse_msg (env,NIL,hdr,hdrsize,NIL,BADHOST,stream->dtb->flags);
      fs_give ((void **) &hdr);	/* flush header */
    }
    else {			/* can save memory doing it this way */
      hdr = (*stream->dtb->header) (stream,msgno,&hdrsize,flags | FT_INTERNAL);
      if (hdrsize) {		/* in case null header */
	c = hdr[hdrsize];	/* preserve what's there */
	hdr[hdrsize] = '\0';	/* tie off header */
	rfc822_parse_msg (env,NIL,hdr,hdrsize,NIL,BADHOST,stream->dtb->flags);
	hdr[hdrsize] = c;	/* restore in case cached data */
      }
      else *env = mail_newenvelope ();
    }
  }
				/* if need date, have date in envelope? */
  if (!elt->day && *env && (*env)->date) mail_parse_date (elt,(*env)->date);
				/* sigh, fill in bogus default */
  if (!elt->day) elt->day = elt->month = 1;
  if (body) *body = *b;		/* return the body */
  return *env;			/* return the envelope */
}

/* Mail mark single message (internal use only)
 * Accepts: mail stream
 *	    elt to mark
 *	    fetch flags
 */

static void markseen (MAILSTREAM *stream,MESSAGECACHE *elt,long flags)
{
  unsigned long i;
  char sequence[20];
  MESSAGECACHE *e;
				/* non-peeking and needs to set \Seen? */
  if (!(flags & FT_PEEK) && !elt->seen) {
    if (stream->dtb->flagmsg){	/* driver wants per-message call? */
      elt->valid = NIL;		/* do pre-alteration driver call */
      (*stream->dtb->flagmsg) (stream,elt);
				/* set seen, do post-alteration driver call */
      elt->seen = elt->valid = T;
      (*stream->dtb->flagmsg) (stream,elt);
    }
    if (stream->dtb->flag) {	/* driver wants one-time call?  */
				/* better safe than sorry, save seq bits */
      for (i = 1; i <= stream->nmsgs; i++) {
	e = mail_elt (stream,i);
	e->private.sequence = e->sequence;
      }
				/* call driver to set the message */
      sprintf (sequence,"%lu",elt->msgno);
      (*stream->dtb->flag) (stream,sequence,"\\Seen",ST_SET);
				/* restore sequence bits */
      for (i = 1; i <= stream->nmsgs; i++) {
	e = mail_elt (stream,i);
	e->sequence = e->private.sequence;
      }
    }
				/* notify mail program of flag change */
    MM_FLAGS (stream,elt->msgno);
  }
}

/* Mail fetch message
 * Accepts: mail stream
 *	    message # to fetch
 *	    pointer to returned length
 *	    flags
 * Returns: message text
 */

char *mail_fetch_message (MAILSTREAM *stream,unsigned long msgno,
			  unsigned long *len,long flags)
{
  GETS_DATA md;
  SIZEDTEXT *t;
  STRING bs;
  MESSAGECACHE *elt;
  char *s,*u;
  unsigned long i,j;
  if (len) *len = 0;		/* default return size */
  if (flags & FT_UID) {		/* UID form of call */
    if (msgno = mail_msgno (stream,msgno)) flags &= ~FT_UID;
    else return "";		/* must get UID/msgno map first */
  }
				/* initialize message data identifier */
  INIT_GETS (md,stream,msgno,"",0,0);
				/* is data already cached? */
  if ((t = &(elt = mail_elt (stream,msgno))->private.msg.full.text)->data) {
    markseen (stream,elt,flags);/* mark message seen */
    return mail_fetch_text_return (&md,t,len);
  }
  if (!stream->dtb) return "";	/* not in cache, must have live driver */
  if (stream->dtb->msgdata) return
    ((*stream->dtb->msgdata) (stream,msgno,"",0,0,NIL,flags) && t->data) ?
      mail_fetch_text_return (&md,t,len) : "";
				/* ugh, have to do this the crufty way */
  u = mail_fetch_header (stream,msgno,NIL,NIL,&i,flags);
				/* copy in case text method stomps on it */
  s = (char *) memcpy (fs_get ((size_t) i),u,(size_t) i);
  if ((*stream->dtb->text) (stream,msgno,&bs,flags)) {
    t = &stream->text;		/* build combined copy */
    if (t->data) fs_give ((void **) &t->data);
    t->data = (unsigned char *) fs_get ((t->size = i + SIZE (&bs)) + 1);
    if (!elt->rfc822_size) elt->rfc822_size = t->size;
    else if (elt->rfc822_size != t->size) {
      char tmp[MAILTMPLEN];
      sprintf (tmp,"Calculated RFC822.SIZE (%lu) != reported size (%lu)",
	       t->size,elt->rfc822_size);
      mm_log (tmp,WARN);	/* bug trap */
    }
    memcpy (t->data,s,(size_t) i);
    for (u = (char *) t->data + i, j = SIZE (&bs); j;) {
      memcpy (u,bs.curpos,bs.cursize);
      u += bs.cursize;		/* update text */
      j -= bs.cursize;
      bs.curpos += (bs.cursize -1);
      bs.cursize = 0;
      (*bs.dtb->next) (&bs);	/* advance to next buffer's worth */
    } 
    *u = '\0';			/* tie off data */
    u = mail_fetch_text_return (&md,t,len);
  }
  else u = "";
  fs_give ((void **) &s);	/* finished with copy of header */
  return u;
}

/* Mail fetch message header
 * Accepts: mail stream
 *	    message # to fetch
 *	    MIME section specifier (#.#.#...#)
 *	    list of lines to fetch
 *	    pointer to returned length
 *	    flags
 * Returns: message header in RFC822 format
 *
 * Note: never calls a mailgets routine
 */

char *mail_fetch_header (MAILSTREAM *stream,unsigned long msgno,char *section,
			 STRINGLIST *lines,unsigned long *len,long flags)
{
  STRING bs;
  BODY *b = NIL;
  SIZEDTEXT *t = NIL,rt;
  MESSAGE *m = NIL;
  MESSAGECACHE *elt;
  char tmp[MAILTMPLEN];
  if (len) *len = 0;		/* default return size */
  if (section && (strlen (section) > (MAILTMPLEN - 20))) return "";
  if (flags & FT_UID) {		/* UID form of call */
    if (msgno = mail_msgno (stream,msgno)) flags &= ~FT_UID;
    else return "";		/* must get UID/msgno map first */
  }
  elt = mail_elt (stream,msgno);/* get cache data */
  if (section && *section) {	/* nested body header wanted? */
    if (!((b = mail_body (stream,msgno,section)) &&
	  (b->type == TYPEMESSAGE) && !strcmp (b->subtype,"RFC822")))
      return "";		/* lose if no body or not MESSAGE/RFC822 */
    m = b->nested.msg;		/* point to nested message */
  }
				/* else top-level message header wanted */
  else m = &elt->private.msg;
  if (m->header.text.data && mail_match_lines (lines,m->lines,flags)) {
    if (lines) textcpy (t = &stream->text,&m->header.text);
    else t = &m->header.text;	/* in cache, and cache is valid */
    markseen (stream,elt,flags);/* mark message seen */
  }

  else if (stream->dtb) {	/* not in cache, has live driver? */
    if (stream->dtb->msgdata) {	/* has driver section fetch? */
				/* build driver section specifier */
      if (section && *section) sprintf (tmp,"%s.HEADER",section);
      else strcpy (tmp,"HEADER");
      if ((*stream->dtb->msgdata) (stream,msgno,tmp,0,0,lines,flags)) {
	t = &m->header.text;	/* fetch data */
				/* don't need to postprocess lines */
	if (m->lines) lines = NIL;
	else if (lines) textcpy (t = &stream->text,&m->header.text);
      }
    }
    else if (b) {		/* nested body wanted? */
      if (stream->private.search.text) {
	rt.data = (unsigned char *) stream->private.search.text +
	  b->nested.msg->header.offset;
	rt.size = b->nested.msg->header.text.size;
	t = &rt;
      }
      else if ((*stream->dtb->text) (stream,msgno,&bs,flags & ~FT_INTERNAL)) {
	if ((bs.dtb->next == mail_string_next) && !lines) {
	  rt.data = (unsigned char *) bs.curpos + b->nested.msg->header.offset;
	  rt.size = b->nested.msg->header.text.size;
	  if (stream->private.search.string)
	    stream->private.search.text = bs.curpos;
	  t = &rt;		/* special hack to avoid extra copy */
	}
	else textcpyoffstring (t = &stream->text,&bs,
			       b->nested.msg->header.offset,
			       b->nested.msg->header.text.size);
      }
    }
    else {			/* top-level header fetch */
				/* mark message seen */
      markseen (stream,elt,flags);
      if (rt.data = (unsigned char *)
	  (*stream->dtb->header) (stream,msgno,&rt.size,flags)) {
				/* make a safe copy if need to filter */
	if (lines) textcpy (t = &stream->text,&rt);
	else t = &rt;		/* top level header */
      }
    }
  }
  if (!t || !t->data) return "";/* error if no string */
				/* filter headers if requested */
  if (lines) t->size = mail_filter ((char *) t->data,t->size,lines,flags);
  if (len) *len = t->size;	/* return size if requested */
  return (char *) t->data;	/* and text */
}

/* Mail fetch message text
 * Accepts: mail stream
 *	    message # to fetch
 *	    MIME section specifier (#.#.#...#)
 *	    pointer to returned length
 *	    flags
 * Returns: message text
 */

char *mail_fetch_text (MAILSTREAM *stream,unsigned long msgno,char *section,
		       unsigned long *len,long flags)
{
  GETS_DATA md;
  PARTTEXT *p;
  STRING bs;
  MESSAGECACHE *elt;
  BODY *b = NIL;
  char tmp[MAILTMPLEN];
  unsigned long i;
  if (len) *len = 0;		/* default return size */
  if (section && (strlen (section) > (MAILTMPLEN - 20))) return "";
  if (flags & FT_UID) {		/* UID form of call */
    if (msgno = mail_msgno (stream,msgno)) flags &= ~FT_UID;
    else return "";		/* must get UID/msgno map first */
  }
  elt = mail_elt (stream,msgno);/* get cache data */
  if (section && *section) {	/* nested body text wanted? */
    if (!((b = mail_body (stream,msgno,section)) &&
	  (b->type == TYPEMESSAGE) && !strcmp (b->subtype,"RFC822")))
      return "";		/* lose if no body or not MESSAGE/RFC822 */
    p = &b->nested.msg->text;	/* point at nested message */
				/* build IMAP-format section specifier */
    sprintf (tmp,"%s.TEXT",section);
    flags &= ~FT_INTERNAL;	/* can't win with this set */
  }
  else {			/* top-level message text wanted */
    p = &elt->private.msg.text;
    strcpy (tmp,"TEXT");
  }
				/* initialize message data identifier */
  INIT_GETS (md,stream,msgno,section,0,0);
  if (p->text.data) {		/* is data already cached? */
    markseen (stream,elt,flags);/* mark message seen */
    return mail_fetch_text_return (&md,&p->text,len);
  }
  if (!stream->dtb) return "";	/* not in cache, must have live driver */
  if (stream->dtb->msgdata) return
    ((*stream->dtb->msgdata) (stream,msgno,tmp,0,0,NIL,flags) && p->text.data)?
      mail_fetch_text_return (&md,&p->text,len) : "";
  if (!(*stream->dtb->text) (stream,msgno,&bs,flags)) return "";
  if (section && *section) {	/* nested is more complex */
    SETPOS (&bs,p->offset);
    i = p->text.size;		/* just want this much */
  }
  else i = SIZE (&bs);		/* want entire text */
  return mail_fetch_string_return (&md,&bs,i,len);
}

/* Mail fetch message body part MIME headers
 * Accepts: mail stream
 *	    message # to fetch
 *	    MIME section specifier (#.#.#...#)
 *	    pointer to returned length
 *	    flags
 * Returns: message text
 */

char *mail_fetch_mime (MAILSTREAM *stream,unsigned long msgno,char *section,
		       unsigned long *len,long flags)
{
  PARTTEXT *p;
  STRING bs;
  BODY *b;
  char tmp[MAILTMPLEN];
  if (len) *len = 0;		/* default return size */
  if (section && (strlen (section) > (MAILTMPLEN - 20))) return "";
  if (flags & FT_UID) {		/* UID form of call */
    if (msgno = mail_msgno (stream,msgno)) flags &= ~FT_UID;
    else return "";		/* must get UID/msgno map first */
  }
  flags &= ~FT_INTERNAL;	/* can't win with this set */
  if (!(section && *section && (b = mail_body (stream,msgno,section))))
    return "";			/* not valid section */
				/* in cache? */
  if ((p = &b->mime)->text.data) {
				/* mark message seen */
    markseen (stream,mail_elt (stream,msgno),flags);
    if (len) *len = p->text.size;
    return (char *) p->text.data;
  }
  if (!stream->dtb) return "";	/* not in cache, must have live driver */
  if (stream->dtb->msgdata) {	/* has driver fetch? */
				/* build driver section specifier */
    sprintf (tmp,"%s.MIME",section);
    if ((*stream->dtb->msgdata) (stream,msgno,tmp,0,0,NIL,flags) &&
	p->text.data) {
      if (len) *len = p->text.size;
      return (char *) p->text.data;
    }
    else return "";
  }
  if (len) *len = b->mime.text.size;
  if (!b->mime.text.size) {	/* empty MIME header -- mark seen anyway */
    markseen (stream,mail_elt (stream,msgno),flags);
    return "";
  }
				/* have to get it from offset */
  if (stream->private.search.text)
    return stream->private.search.text + b->mime.offset;
  if (!(*stream->dtb->text) (stream,msgno,&bs,flags)) {
    if (len) *len = 0;
    return "";
  }
  if (bs.dtb->next == mail_string_next) {
    if (stream->private.search.string) stream->private.search.text = bs.curpos;
    return bs.curpos + b->mime.offset;
  }
  return textcpyoffstring (&stream->text,&bs,b->mime.offset,b->mime.text.size);
}

/* Mail fetch message body part
 * Accepts: mail stream
 *	    message # to fetch
 *	    MIME section specifier (#.#.#...#)
 *	    pointer to returned length
 *	    flags
 * Returns: message body
 */

char *mail_fetch_body (MAILSTREAM *stream,unsigned long msgno,char *section,
		       unsigned long *len,long flags)
{
  GETS_DATA md;
  PARTTEXT *p;
  STRING bs;
  BODY *b;
  SIZEDTEXT *t;
  char *s,tmp[MAILTMPLEN];
  if (!(section && *section))	/* top-level text wanted? */
    return mail_fetch_message (stream,msgno,len,flags);
  else if (strlen (section) > (MAILTMPLEN - 20)) return "";
  flags &= ~FT_INTERNAL;	/* can't win with this set */
				/* initialize message data identifier */
  INIT_GETS (md,stream,msgno,section,0,0);
				/* kludge for old section 0 header */
  if (!strcmp (s = strcpy (tmp,section),"0") ||
      ((s = strstr (tmp,".0")) && !s[2])) {
    SIZEDTEXT ht;
    *s = '\0';			/* tie off section */
				/* this silly way so it does mailgets */
    ht.data = (unsigned char *) mail_fetch_header (stream,msgno,
						   tmp[0] ? tmp : NIL,NIL,
						   &ht.size,flags);
				/* may have UIDs here */
    md.flags = (flags & FT_UID) ? MG_UID : NIL;
    return mail_fetch_text_return (&md,&ht,len);
  }
  if (len) *len = 0;		/* default return size */
  if (flags & FT_UID) {		/* UID form of call */
    if (msgno = mail_msgno (stream,msgno)) flags &= ~FT_UID;
    else return "";		/* must get UID/msgno map first */
  }
				/* must have body */
  if (!(b = mail_body (stream,msgno,section))) return "";
				/* have cached text? */
  if ((t = &(p = &b->contents)->text)->data) {
				/* mark message seen */
    markseen (stream,mail_elt (stream,msgno),flags);
    return mail_fetch_text_return (&md,t,len);
  }
  if (!stream->dtb) return "";	/* not in cache, must have live driver */
  if (stream->dtb->msgdata) return
    ((*stream->dtb->msgdata)(stream,msgno,section,0,0,NIL,flags) && t->data) ?
      mail_fetch_text_return (&md,t,len) : "";
  if (len) *len = t->size;
  if (!t->size) {		/* empty body part -- mark seen anyway */
    markseen (stream,mail_elt (stream,msgno),flags);
    return "";
  }
				/* copy body from stringstruct offset */
  if (stream->private.search.text)
    return stream->private.search.text + p->offset;
  if (!(*stream->dtb->text) (stream,msgno,&bs,flags)) {
    if (len) *len = 0;
    return "";
  }
  if (bs.dtb->next == mail_string_next) {
    if (stream->private.search.string) stream->private.search.text = bs.curpos;
    return bs.curpos + p->offset;
  }
  SETPOS (&bs,p->offset);
  return mail_fetch_string_return (&md,&bs,t->size,len);
}

/* Mail fetch partial message text
 * Accepts: mail stream
 *	    message # to fetch
 *	    MIME section specifier (#.#.#...#)
 *	    offset of first designed byte or 0 to start at beginning
 *	    maximum number of bytes or 0 for all bytes
 *	    flags
 * Returns: T if successful, else NIL
 */

long mail_partial_text (MAILSTREAM *stream,unsigned long msgno,char *section,
			unsigned long first,unsigned long last,long flags)
{
  GETS_DATA md;
  PARTTEXT *p = NIL;
  MESSAGECACHE *elt;
  STRING bs;
  BODY *b;
  char tmp[MAILTMPLEN];
  unsigned long i;
  if (!mailgets) fatal ("mail_partial_text() called without a mailgets!");
  if (section && (strlen (section) > (MAILTMPLEN - 20))) return NIL;
  if (flags & FT_UID) {		/* UID form of call */
    if (msgno = mail_msgno (stream,msgno)) flags &= ~FT_UID;
    else return NIL;		/* must get UID/msgno map first */
  }
  elt = mail_elt (stream,msgno);/* get cache data */
  flags &= ~FT_INTERNAL;	/* bogus if this is set */
  if (section && *section) {	/* nested body text wanted? */
    if (!((b = mail_body (stream,msgno,section)) &&
	  (b->type == TYPEMESSAGE) && !strcmp (b->subtype,"RFC822")))
      return NIL;		/* lose if no body or not MESSAGE/RFC822 */
    p = &b->nested.msg->text;	/* point at nested message */
				/* build IMAP-format section specifier */
    sprintf (tmp,"%s.TEXT",section);
  }
  else {			/* else top-level message text wanted */
    p = &elt->private.msg.text;
    strcpy (tmp,"TEXT");
  }

				/* initialize message data identifier */
  INIT_GETS (md,stream,msgno,tmp,first,last);
  if (p->text.data) {		/* is data already cached? */
    INIT (&bs,mail_string,p->text.data,i = p->text.size);
    markseen (stream,elt,flags);/* mark message seen */
  }
  else {			/* else get data from driver */
    if (!stream->dtb) return NIL;
    if (stream->dtb->msgdata)	/* driver will handle this */
      return (*stream->dtb->msgdata) (stream,msgno,tmp,first,last,NIL,flags);
    if (!(*stream->dtb->text) (stream,msgno,&bs,flags)) return NIL;
    if (section && *section) {	/* nexted if more complex */
      SETPOS (&bs,p->offset);	/* offset stringstruct to data */
      i = p->text.size;		/* maximum size of data */
    }
    else i = SIZE (&bs);	/* just want this much */
  }
  if (i <= first) i = first = 0;/* first byte is beyond end of text */
				/* truncate as needed */
  else {			/* offset and truncate */
    SETPOS (&bs,first + GETPOS (&bs));
    i -= first;			/* reduced size */
    if (last && (i > last)) i = last;
  }
				/* do the mailgets thing */
  (*mailgets) (mail_read,&bs,i,&md);
  return T;			/* success */
}

/* Mail fetch partial message body part
 * Accepts: mail stream
 *	    message # to fetch
 *	    MIME section specifier (#.#.#...#)
 *	    offset of first designed byte or 0 to start at beginning
 *	    maximum number of bytes or 0 for all bytes
 *	    flags
 * Returns: T if successful, else NIL
 */

long mail_partial_body (MAILSTREAM *stream,unsigned long msgno,char *section,
			unsigned long first,unsigned long last,long flags)
{
  GETS_DATA md;
  PARTTEXT *p;
  STRING bs;
  BODY *b;
  SIZEDTEXT *t;
  unsigned long i;
  if (!(section && *section))	/* top-level text wanted? */
    return mail_partial_text (stream,msgno,NIL,first,last,flags);
  if (!mailgets) fatal ("mail_partial_body() called without a mailgets!");
  if (flags & FT_UID) {		/* UID form of call */
    if (msgno = mail_msgno (stream,msgno)) flags &= ~FT_UID;
    else return NIL;		/* must get UID/msgno map first */
  }
				/* must have body */
  if (!(b = mail_body (stream,msgno,section))) return NIL;
  flags &= ~FT_INTERNAL;	/* bogus if this is set */

				/* initialize message data identifier */
  INIT_GETS (md,stream,msgno,section,first,last);
				/* have cached text? */
  if ((t = &(p = &b->contents)->text)->data) {
				/* mark message seen */
    markseen (stream,mail_elt (stream,msgno),flags);
    INIT (&bs,mail_string,t->data,i = t->size);
  }
  else {			/* else get data from driver */
    if (!stream->dtb) return NIL;
    if (stream->dtb->msgdata)	/* driver will handle this */
      return (*stream->dtb->msgdata) (stream,msgno,section,first,last,NIL,
				      flags);
    if (!(*stream->dtb->text) (stream,msgno,&bs,flags)) return NIL;
    if (section && *section) {	/* nexted if more complex */
      SETPOS (&bs,p->offset);	/* offset stringstruct to data */
      i = t->size;		/* maximum size of data */
    }
    else i = SIZE (&bs);	/* just want this much */
  }
  if (i <= first) i = first = 0;/* first byte is beyond end of text */
  else {			/* offset and truncate */
    SETPOS (&bs,first + GETPOS (&bs));
    i -= first;			/* reduced size */
    if (last && (i > last)) i = last;
  }
				/* do the mailgets thing */
  (*mailgets) (mail_read,&bs,i,&md);
  return T;			/* success */
}

/* Mail return message text
 * Accepts: identifier data
 *	    sized text
 *	    pointer to returned length
 * Returns: text
 */

char *mail_fetch_text_return (GETS_DATA *md,SIZEDTEXT *t,unsigned long *len)
{
  STRING bs;
  if (len) *len = t->size;	/* return size */
  if (t->size && mailgets) {	/* have to do the mailgets thing? */
				/* silly but do it anyway for consistency */
    INIT (&bs,mail_string,t->data,t->size);
    return (*mailgets) (mail_read,&bs,t->size,md);
  }
  return t->size ? (char *) t->data : "";
}


/* Mail return message string
 * Accepts: identifier data
 *	    stringstruct
 *	    text length
 *	    pointer to returned length
 * Returns: text
 */

char *mail_fetch_string_return (GETS_DATA *md,STRING *bs,unsigned long i,
				unsigned long *len)
{
  if (len) *len = i;		/* return size */
				/* have to do the mailgets thing? */
  if (mailgets) return (*mailgets) (mail_read,bs,i,md);
				/* special hack to avoid extra copy */
  if (bs->dtb->next == mail_string_next) return bs->curpos;
				/* make string copy in memory */
  return textcpyoffstring (&md->stream->text,bs,GETPOS (bs),i);
}

/* Read data from stringstruct
 * Accepts: stringstruct
 *	    size of data to read
 *	    buffer to read into
 * Returns: T, always, stringstruct updated
 */

long mail_read (void *stream,unsigned long size,char *buffer)
{
  unsigned long i;
  STRING *s = (STRING *) stream;
  while (size) {		/* until satisfied */
    memcpy (buffer,s->curpos,i = min (s->cursize,size));
    buffer += i;		/* update buffer */
    size -= i;			/* note that we read this much */
    s->curpos += --i;		/* advance that many spaces minus 1 */
    s->cursize -= i;
    SNX (s);			/* now use SNX to advance the last byte */
  }
  return T;
}

/* Mail fetch UID
 * Accepts: mail stream
 *	    message number
 * Returns: UID or zero if dead stream
 */

unsigned long mail_uid (MAILSTREAM *stream,unsigned long msgno)
{
  unsigned long uid = mail_elt (stream,msgno)->private.uid;
  return uid ? uid :
    (stream->dtb && stream->dtb->uid) ? (*stream->dtb->uid) (stream,msgno) : 0;
}


/* Mail fetch msgno from UID
 * Accepts: mail stream
 *	    UID
 * Returns: msgno or zero if failed
 */

unsigned long mail_msgno (MAILSTREAM *stream,unsigned long uid)
{
  unsigned long msgno,delta,first,firstuid,last,lastuid,middle,miduid;
  if (stream->dtb) {		/* active stream? */
    if (stream->dtb->msgno)	/* direct way */
      return (*stream->dtb->msgno) (stream,uid);
    else if (stream->dtb->uid) {/* indirect way */
      /* Placeholder for now, since currently there are no drivers which
       * have a uid method but not a msgno method
       */
      for (msgno = 1; msgno <= stream->nmsgs; msgno++)
	if ((*stream->dtb->uid) (stream,msgno) == uid) return msgno;
    }
				/* binary search since have full map */
    else for (first = 1,last = stream->nmsgs, delta = (first <= last) ? 1 : 0;
	      delta &&
	      (uid >= (firstuid = mail_elt (stream,first)->private.uid)) &&
		(uid <= (lastuid = mail_elt (stream,last)->private.uid));) {
				/* done if match at an endpoint */
	if (uid == firstuid) return first;
	if (uid == lastuid) return last;
				/* have anything between endpoints? */
	if (delta = ((last - first) / 2)) {
	  if ((miduid = mail_elt (stream,middle = first + delta)->private.uid)
	      == uid)
	    return middle;	/* found match in middle */
	  else if (uid < miduid) last = middle - 1;
	  else first = middle + 1;
	}
    }
  }
  else {			/* dead stream, do linear search for UID */
    for (msgno = 1; msgno <= stream->nmsgs; msgno++)
      if (mail_elt (stream,msgno)->private.uid == uid) return msgno;
  }
  return 0;			/* didn't find the UID anywhere */
}

/* Mail fetch From string for menu
 * Accepts: destination string
 *	    mail stream
 *	    message # to fetch
 *	    desired string length
 * Returns: string of requested length
 */

void mail_fetchfrom (char *s,MAILSTREAM *stream,unsigned long msgno,
		     long length)
{
  char *t;
  char tmp[MAILTMPLEN];
  ENVELOPE *env = mail_fetchenvelope (stream,msgno);
  ADDRESS *adr = env ? env->from : NIL;
  memset (s,' ',(size_t)length);/* fill it with spaces */
  s[length] = '\0';		/* tie off with null */
				/* get first from address from envelope */
  while (adr && !adr->host) adr = adr->next;
  if (adr) {			/* if a personal name exists use it */
    if (!(t = adr->personal))
      sprintf (t = tmp,"%.256s@%.256s",adr->mailbox,adr->host);
    memcpy (s,t,(size_t) min (length,(long) strlen (t)));
  }
}


/* Mail fetch Subject string for menu
 * Accepts: destination string
 *	    mail stream
 *	    message # to fetch
 *	    desired string length
 * Returns: string of no more than requested length
 */

void mail_fetchsubject (char *s,MAILSTREAM *stream,unsigned long msgno,
			long length)
{
  ENVELOPE *env = mail_fetchenvelope (stream,msgno);
  memset (s,'\0',(size_t) length+1);
				/* copy subject from envelope */
  if (env && env->subject) strncpy (s,env->subject,(size_t) length);
  else *s = ' ';		/* if no subject then just a space */
}

/* Mail modify flags
 * Accepts: mail stream
 *	    sequence
 *	    flag(s)
 *	    option flags
 */

void mail_flag (MAILSTREAM *stream,char *sequence,char *flag,long flags)
{
  MESSAGECACHE *elt;
  unsigned long i,uf;
  long f;
  short nf;
  if (!stream->dtb) return;	/* no-op if no stream */
  if ((stream->dtb->flagmsg || !stream->dtb->flag) &&
      ((flags & ST_UID) ? mail_uid_sequence (stream,sequence) :
       mail_sequence (stream,sequence)) &&
      ((f = mail_parse_flags (stream,flag,&uf)) || uf))
    for (i = 1,nf = (flags & ST_SET) ? T : NIL; i <= stream->nmsgs; i++)
      if ((elt = mail_elt (stream,i))->sequence) {
	struct {		/* old flags */
	  unsigned int valid : 1;
	  unsigned int seen : 1;
	  unsigned int deleted : 1;
	  unsigned int flagged : 1;
	  unsigned int answered : 1;
	  unsigned int draft : 1;
	  unsigned long user_flags;
	} old;
	old.valid = elt->valid; old.seen = elt->seen;
	old.deleted = elt->deleted; old.flagged = elt->flagged;
	old.answered = elt->answered; old.draft = elt->draft;
	old.user_flags = elt->user_flags;
	elt->valid = NIL;	/* prepare for flag alteration */
	if (stream->dtb->flagmsg) (*stream->dtb->flagmsg) (stream,elt);
	if (f&fSEEN) elt->seen = nf;
	if (f&fDELETED) elt->deleted = nf;
	if (f&fFLAGGED) elt->flagged = nf;
	if (f&fANSWERED) elt->answered = nf;
	if (f&fDRAFT) elt->draft = nf;
				/* user flags */
	if (flags & ST_SET) elt->user_flags |= uf;
	else elt->user_flags &= ~uf;
	elt->valid = T;		/* flags now altered */
	if ((old.valid != elt->valid) || (old.seen != elt->seen) ||
	    (old.deleted != elt->deleted) || (old.flagged != elt->flagged) ||
	    (old.answered != elt->answered) || (old.draft != elt->draft) ||
	    (old.user_flags != elt->user_flags))
	  MM_FLAGS (stream,elt->msgno);
	if (stream->dtb->flagmsg) (*stream->dtb->flagmsg) (stream,elt);
      }
				/* call driver once */
  if (stream->dtb->flag) (*stream->dtb->flag) (stream,sequence,flag,flags);
}

/* Mail search for messages
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    option flags
 * Returns: T if successful, NIL if dead stream, NIL searchpgm or bad charset
 */

long mail_search_full (MAILSTREAM *stream,char *charset,SEARCHPGM *pgm,
		       long flags)
{
  unsigned long i;
  long ret = NIL;
  if (!(flags & SE_RETAIN))	/* clear search vector unless retaining */
    for (i = 1; i <= stream->nmsgs; ++i) mail_elt (stream,i)->searched = NIL;
  if (pgm && stream->dtb)	/* must have a search program and driver */
    ret = (*(stream->dtb->search ? stream->dtb->search : mail_search_default))
      (stream,charset,pgm,flags);
				/* flush search program if requested */
  if (flags & SE_FREE) mail_free_searchpgm (&pgm);
  return ret;
}


/* Mail search for messages default handler
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    option flags
 * Returns: T if successful, NIL if bad charset
 */

long mail_search_default (MAILSTREAM *stream,char *charset,SEARCHPGM *pgm,
			  long flags)
{
  unsigned long i;
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
  for (i = 1; i <= stream->nmsgs; ++i) if (mail_search_msg (stream,i,NIL,pgm)){
    if (flags & SE_UID) mm_searched (stream,mail_uid (stream,i));
    else {			/* mark as searched, notify mail program */
      mail_elt (stream,i)->searched = T;
      if (!stream->silent) mm_searched (stream,i);
    }
  }
  return LONGT;
}

/* Mail ping mailbox
 * Accepts: mail stream
 * Returns: stream if still open else NIL
 */

long mail_ping (MAILSTREAM *stream)
{
  unsigned long i,n,uf,len;
  char *s,*f,tmp[MAILTMPLEN],flags[MAILTMPLEN];
  MAILSTREAM *snarf;
  MESSAGECACHE *elt;
  STRING bs;
  long ret;
				/* do driver action */
  if ((ret = ((stream && stream->dtb) ? (stream->dtb->ping) (stream) : NIL)) &&
      stream->snarf.name &&	/* time to snarf? */
				/* prohibit faster than once/min */
      (time (0) > (time_t) (stream->snarf.time + min(60,mailsnarfinterval))) &&
      (snarf = mail_open (NIL,stream->snarf.name,
			  stream->snarf.options | OP_SILENT))) {
    if ((n = snarf->nmsgs) &&	/* yes, have messages to snarf? */
	mail_search_full (snarf,NIL,mail_criteria ("UNDELETED"),SE_FREE)) {
      for (i = 1; ret && (i <= n); i++)	/* for each message */
	if ((elt = mail_elt (snarf,i))->searched &&
	    (s = mail_fetch_message (snarf,i,&len,NIL)) && len) {
	  INIT (&bs,mail_string,s,len);
	  if (mailsnarfpreserve) {
				/* yes, make sure have fast data */
	    if (!elt->valid || !elt->day) {
	      sprintf (tmp,"%lu",n);
	      mail_fetch_fast (snarf,tmp,NIL);
	    }
				/* initialize flag string */
	    memset (flags,0,MAILTMPLEN);
				/* output system flags */
	    if (elt->seen) strcat (flags," \\Seen");
	    if (elt->flagged) strcat (flags," \\Flagged");
	    if (elt->answered) strcat (flags," \\Answered");
	    if (elt->draft) strcat (flags," \\Draft");
				/* any user flags? */
	    for (uf = elt->user_flags,s = flags + strlen (flags);
		 uf && (f = stream->user_flags[find_rightmost_bit (&uf)]) &&
		   ((MAILTMPLEN - (s - tmp)) > (long) (2 + strlen (f)));
		 s += strlen (s)) sprintf (s," %s",f);
	    ret = mail_append_full (stream,stream->mailbox,flags + 1,
				    mail_date (tmp,elt),&bs);
	  }
	  else ret = mail_append (stream,stream->mailbox,&bs);

	  if (ret) {		/* did snarf succeed? */
				/* driver has per-message (or no) flag call */
	    if (snarf->dtb->flagmsg || !snarf->dtb->flag) {
	      elt->valid = NIL;	/* prepare for flag alteration */
	      if (snarf->dtb->flagmsg) (*snarf->dtb->flagmsg) (snarf,elt);
	      elt->deleted = T;
	      elt->valid = T;	/* flags now altered */
	      if (snarf->dtb->flagmsg) (*snarf->dtb->flagmsg) (snarf,elt);
	    }
				/* driver has one-time flag call */
	    if (snarf->dtb->flag) {
	      sprintf (tmp,"%lu",i);
	      (*snarf->dtb->flag) (snarf,tmp,"\\Deleted",ST_SET);
	    }
	  }
	  else {		/* copy failed */
	    sprintf (tmp,"Unable to move message %lu from %s mailbox",
		     i,snarf->dtb->name);
	    mm_log (tmp,WARN);
	  }
	}
    }
				/* expunge the messages */
    mail_close_full (snarf,n ? CL_EXPUNGE : NIL);
    stream->snarf.time = time (0);
    /* Even if the snarf failed, we don't want to return NIL if the stream
     * is still alive.  Or at least that's what we currently think.
     */
  				/* redo the driver's action */
    ret = stream->dtb ? (*stream->dtb->ping) (stream) : NIL;
  }
  return ret;
}

/* Mail check mailbox
 * Accepts: mail stream
 */

void mail_check (MAILSTREAM *stream)
{
  				/* do the driver's action */
  if (stream->dtb) (*stream->dtb->check) (stream);
}


/* Mail expunge mailbox
 * Accepts: mail stream
 */

void mail_expunge (MAILSTREAM *stream)
{
  				/* do the driver's action */
  if (stream->dtb) (*stream->dtb->expunge) (stream);
}


/* Mail copy message(s)
 * Accepts: mail stream
 *	    sequence
 *	    destination mailbox
 *	    flags
 */

long mail_copy_full (MAILSTREAM *stream,char *sequence,char *mailbox,
		     long options)
{
  return SAFE_COPY (stream->dtb,stream,sequence,mailbox,options);
}

/* Append data package to use for old single-message mail_append() interface */

typedef struct mail_append_package {
  char *flags;			/* initial flags */
  char *date;			/* message internal date */
  STRING *message;		/* stringstruct of message */
} APPENDPACKAGE;


/* Single append message string
 * Accepts: mail stream
 *	    package pointer (cast as a void *)
 *	    pointer to return initial flags
 *	    pointer to return message internal date
 *	    pointer to return stringstruct of message to append
 * Returns: T, always
 */

static long mail_append_single (MAILSTREAM *stream,void *data,char **flags,
				char **date,STRING **message)
{
  APPENDPACKAGE *ap = (APPENDPACKAGE *) data;
  *flags = ap->flags;		/* get desired data from the package */
  *date = ap->date;
  *message = ap->message;
  ap->message = NIL;		/* so next callback puts a stop to it */
  return LONGT;			/* always return success */
}


/* Mail append message string
 * Accepts: mail stream
 *	    destination mailbox
 *	    initial flags
 *	    message internal date
 *	    stringstruct of message to append
 * Returns: T on success, NIL on failure
 */

long mail_append_full (MAILSTREAM *stream,char *mailbox,char *flags,char *date,
		       STRING *message)
{
  APPENDPACKAGE ap;
  ap.flags = flags;		/* load append package */
  ap.date = date;
  ap.message = message;
  return mail_append_multiple (stream,mailbox,mail_append_single,(void *) &ap);
}

/* Mail append message(s)
 * Accepts: mail stream
 *	    destination mailbox
 *	    append data callback
 *	    arbitrary data for callback use
 * Returns: T on success, NIL on failure
 */

long mail_append_multiple (MAILSTREAM *stream,char *mailbox,append_t af,
			   void *data)
{
  char *s,tmp[MAILTMPLEN];
  DRIVER *d = NIL;
				/* never allow names with newlines */
  if (strpbrk (mailbox,"\015\012")) {
    MM_LOG ("Can't append to mailbox with such a name",ERROR);
    return NIL;
  }
  if (strlen (mailbox) >= (NETMAXHOST+(NETMAXUSER*2)+NETMAXMBX+NETMAXSRV+50)) {
    sprintf (tmp,"Can't append %.80s: %s",mailbox,(*mailbox == '{') ?
	     "invalid remote specification" : "no such mailbox");
    MM_LOG (tmp,ERROR);
    return NIL;
  }
				/* see if special driver hack */
  if (strncmp (lcase (strcpy (tmp,mailbox)),"#driver.",8))
    d = mail_valid (stream,mailbox,NIL);
				/* it is, tie off name at likely delimiter */
  else if (!(s = strpbrk (tmp+8,"/\\:"))) {
    sprintf (tmp,"Can't append to mailbox %.80s: bad driver syntax",mailbox);
    MM_LOG (tmp,ERROR);
    return NIL;
  }
  else {			/* found delimiter */
    *s++ = '\0';
    for (d = maildrivers; d && strcmp (d->name,tmp+8); d = d->next);
    if (d) mailbox += s - tmp;/* skip past driver specification */
    else {
      sprintf (tmp,"Can't append to mailbox %.80s: unknown driver",mailbox);
      MM_LOG (tmp,ERROR);
      return NIL;
    }
  }
  if (!d) {			/* no driver, try for TRYCREATE if no stream */
    if (!stream && (stream = default_proto (T)) &&
	SAFE_APPEND (stream->dtb,stream,mailbox,af,data))
				/* timing race? */
      MM_NOTIFY (stream,"Append validity confusion",WARN);
				/* generate error message */
    else mail_valid (stream,mailbox,"append to mailbox");
    return NIL;
  }
				/* do append */
  return SAFE_APPEND (d,stream,mailbox,af,data);
}

/* Mail garbage collect stream
 * Accepts: mail stream
 *	    garbage collection flags
 */

void mail_gc (MAILSTREAM *stream,long gcflags)
{
  MESSAGECACHE *elt;
  unsigned long i;
  				/* do the driver's action first */
  if (stream->dtb && stream->dtb->gc) (*stream->dtb->gc) (stream,gcflags);
  stream->msgno = 0;		/* nothing cached now */
  if (gcflags & GC_ENV) {	/* garbage collect envelopes? */
    if (stream->env) mail_free_envelope (&stream->env);
    if (stream->body) mail_free_body (&stream->body);
  }
  if (gcflags & GC_TEXTS) {	/* free texts */
    if (stream->text.data) fs_give ((void **) &stream->text.data);
    stream->text.size = 0;
  }
				/* garbage collect per-message stuff */
  for (i = 1; i <= stream->nmsgs; i++) 
    if (elt = (MESSAGECACHE *) (*mailcache) (stream,i,CH_ELT))
      mail_gc_msg (&elt->private.msg,gcflags);
}


/* Mail garbage collect message
 * Accepts: message structure
 *	    garbage collection flags
 */

void mail_gc_msg (MESSAGE *msg,long gcflags)
{
  if (gcflags & GC_ENV) {	/* garbage collect envelopes? */
    mail_free_envelope (&msg->env);
    mail_free_body (&msg->body);
  }
  if (gcflags & GC_TEXTS) {	/* garbage collect texts */
    if (msg->full.text.data) fs_give ((void **) &msg->full.text.data);
    if (msg->header.text.data) {
      mail_free_stringlist (&msg->lines);
      fs_give ((void **) &msg->header.text.data);
    }
    if (msg->text.text.data) fs_give ((void **) &msg->text.text.data);
				/* now GC all body components */
    if (msg->body) mail_gc_body (msg->body);
  }
}

/* Mail garbage collect texts in BODY structure
 * Accepts: BODY structure
 */

void mail_gc_body (BODY *body)
{
  PART *part;
  switch (body->type) {		/* free contents */
  case TYPEMULTIPART:		/* multiple part */
    for (part = body->nested.part; part; part = part->next)
      mail_gc_body (&part->body);
    break;
  case TYPEMESSAGE:		/* encapsulated message */
    if (body->subtype && !strcmp (body->subtype,"RFC822")) {
      mail_free_stringlist (&body->nested.msg->lines);
      mail_gc_msg (body->nested.msg,GC_TEXTS);
    }
    break;
  default:
    break;
  }
  if (body->mime.text.data) fs_give ((void **) &body->mime.text.data);
  if (body->contents.text.data) fs_give ((void **) &body->contents.text.data);
}

/* Mail get body part
 * Accepts: mail stream
 *	    message number
 *	    section specifier
 * Returns: pointer to body
 */

BODY *mail_body (MAILSTREAM *stream,unsigned long msgno,unsigned char *section)
{
  BODY *b = NIL;
  PART *pt;
  unsigned long i;
				/* make sure have a body */
  if (section && *section && mail_fetchstructure (stream,msgno,&b) && b)
    while (*section) {		/* find desired section */
      if (isdigit (*section)) {	/* get section specifier */
				/* make sure what follows is valid */
	if (!(i = strtoul (section,(char **) &section,10)) ||
	    (*section && ((*section++ != '.') || !*section))) return NIL;
				/* multipart content? */
	if (b->type == TYPEMULTIPART) {
				/* yes, find desired part */
	  if (pt = b->nested.part) while (--i && (pt = pt->next));
	  if (!pt) return NIL;	/* bad specifier */
	  b = &pt->body;	/* note new body */
	}
				/* otherwise must be section 1 */
	else if (i != 1) return NIL;
				/* need to go down further? */
	if (*section) switch (b->type) {
	case TYPEMULTIPART:	/* multipart */
	  break;
	case TYPEMESSAGE:	/* embedded message */
	  if (!strcmp (b->subtype,"RFC822")) {
	    b = b->nested.msg->body;
	    break;
	  }
	default:		/* bogus subpart specification */
	  return NIL;
	}
      }
      else return NIL;		/* unknown section specifier */
    }
  return b;
}  

/* Mail output date from elt fields
 * Accepts: character string to write into
 *	    elt to get data data from
 * Returns: the character string
 */

const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char *mail_date (char *string,MESSAGECACHE *elt)
{
  sprintf (string,"%2d-%s-%d %02d:%02d:%02d %c%02d%02d",
	   elt->day ? elt->day : 1,
	   months[elt->month ? (elt->month - 1) : 0],
	   elt->year + BASEYEAR,elt->hours,elt->minutes,elt->seconds,
	   elt->zoccident ? '-' : '+',elt->zhours,elt->zminutes);
  return string;
}


/* Mail output extended-ctime format date from elt fields
 * Accepts: character string to write into
 *	    elt to get data data from
 * Returns: the character string
 */

char *mail_cdate (char *string,MESSAGECACHE *elt)
{
  char *fmt = "%s %s %2d %02d:%02d:%02d %4d %s%02d%02d\n";
  int d = elt->day ? elt->day : 1;
  int m = elt->month ? (elt->month - 1) : 0;
  int y = elt->year + BASEYEAR;
  const char *s = months[m];
  if (m < 2) {			/* if before March, */
    m += 10;			/* January = month 10 of previous year */
    y--;
  }
  else m -= 2;			/* March is month 0 */
  sprintf (string,fmt,days[(int) (d + 2 + ((7 + 31 * m) / 12)
#ifndef USEJULIANCALENDAR
#ifndef USEORTHODOXCALENDAR	/* Gregorian calendar */
				  + (y / 400)
#ifdef Y4KBUGFIX
				  - (y / 4000)
#endif
#else				/* Orthodox calendar */
				  + (2 * (y / 900)) + ((y % 900) >= 200)
				  + ((y % 900) >= 600)
#endif
				  - (y / 100)
#endif
				  + y + (y / 4)) % 7],
	   s,d,elt->hours,elt->minutes,elt->seconds,elt->year + BASEYEAR,
	   elt->zoccident ? "-" : "+",elt->zhours,elt->zminutes);
  return string;
}

/* Mail parse date into elt fields
 * Accepts: elt to write into
 *	    date string to parse
 * Returns: T if parse successful, else NIL 
 * This routine parses dates as follows:
 * . leading three alphas followed by comma and space are ignored
 * . date accepted in format: mm/dd/yy, mm/dd/yyyy, dd-mmm-yy, dd-mmm-yyyy,
 *    dd mmm yy, dd mmm yyyy
 * . two and three digit years interpreted according to RFC 2822 rules
 * . space or end of string required
 * . time accepted in format hh:mm:ss or hh:mm
 * . end of string accepted
 * . timezone accepted: hyphen followed by symbolic timezone, or space
 *    followed by signed numeric timezone or symbolic timezone
 * Examples of normal input:
 * . IMAP date-only (SEARCH):
 *    dd-mmm-yyyy
 * . IMAP date-time (INTERNALDATE):
 *    dd-mmm-yyyy hh:mm:ss +zzzz
 * . RFC-822:
 *    www, dd mmm yy hh:mm:ss zzz
 * . RFC-2822:
 *    www, dd mmm yyyy hh:mm:ss +zzzz
 */

long mail_parse_date (MESSAGECACHE *elt,unsigned char *s)
{
  unsigned long d,m,y;
  int mi,ms;
  struct tm *t;
  time_t tn;
  char tmp[MAILTMPLEN];
  static unsigned long maxyear = 0;
  if (!maxyear) {		/* know the end of time yet? */
    MESSAGECACHE tmpelt;
    memset (&tmpelt,0xff,sizeof (MESSAGECACHE));
    maxyear = BASEYEAR + tmpelt.year;
  }
				/* clear elt */
  elt->zoccident = elt->zhours = elt->zminutes =
    elt->hours = elt->minutes = elt->seconds =
      elt->day = elt->month = elt->year = 0;
				/* make a writeable uppercase copy */
  if (s && *s && (strlen (s) < (size_t)MAILTMPLEN)) s = ucase (strcpy (tmp,s));
  else return NIL;
				/* skip over possible day of week */
  if (isalpha (*s) && isalpha (s[1]) && isalpha (s[2]) && (s[3] == ',') &&
      (s[4] == ' ')) s += 5;
  while (*s == ' ') s++;	/* parse first number (probable month) */
  if (!(m = strtoul (s,(char **) &s,10))) return NIL;

  switch (*s) {			/* different parse based on delimiter */
  case '/':			/* mm/dd/yy format */
    if (isdigit (*++s) && (d = strtoul (s,(char **) &s,10)) &&
	(*s == '/') && isdigit (*++s)) {
      y = strtoul (s,(char **) &s,10);
      if (*s == '\0') break;	/* must end here */
    }
    return NIL;			/* bogon */
  case ' ':			/* dd mmm yy format */
    while (s[1] == ' ') s++;	/* slurp extra whitespace */
  case '-':			/* dd-mmm-yy format */
    d = m;			/* so the number we got is a day */
				/* make sure string long enough! */
    if (strlen (s) < (size_t) 5) return NIL;
    /* Some compilers don't allow `<<' and/or longs in case statements. */
				/* slurp up the month string */
    ms = ((s[1] - 'A') * 1024) + ((s[2] - 'A') * 32) + (s[3] - 'A');
    switch (ms) {		/* determine the month */
    case (('J'-'A') * 1024) + (('A'-'A') * 32) + ('N'-'A'): m = 1; break;
    case (('F'-'A') * 1024) + (('E'-'A') * 32) + ('B'-'A'): m = 2; break;
    case (('M'-'A') * 1024) + (('A'-'A') * 32) + ('R'-'A'): m = 3; break;
    case (('A'-'A') * 1024) + (('P'-'A') * 32) + ('R'-'A'): m = 4; break;
    case (('M'-'A') * 1024) + (('A'-'A') * 32) + ('Y'-'A'): m = 5; break;
    case (('J'-'A') * 1024) + (('U'-'A') * 32) + ('N'-'A'): m = 6; break;
    case (('J'-'A') * 1024) + (('U'-'A') * 32) + ('L'-'A'): m = 7; break;
    case (('A'-'A') * 1024) + (('U'-'A') * 32) + ('G'-'A'): m = 8; break;
    case (('S'-'A') * 1024) + (('E'-'A') * 32) + ('P'-'A'): m = 9; break;
    case (('O'-'A') * 1024) + (('C'-'A') * 32) + ('T'-'A'): m = 10; break;
    case (('N'-'A') * 1024) + (('O'-'A') * 32) + ('V'-'A'): m = 11; break;
    case (('D'-'A') * 1024) + (('E'-'A') * 32) + ('C'-'A'): m = 12; break;
    default: return NIL;	/* unknown month */
    }
    if (s[4] == *s) s += 5;	/* advance to year */
    else {			/* first three were OK, possibly full name */
      mi = *s;			/* note delimiter, skip alphas */
      for (s += 4; isalpha (*s); s++);
				/* error if delimiter not here */
      if (mi != *s++) return NIL;
    }
    while (*s == ' ') s++;	/* parse year */
    if (isdigit (*s)) {		/* must be a digit here */
      y = strtoul (s,(char **) &s,10);
      if (*s == '\0' || *s == ' ') break;
    }
  default:
    return NIL;			/* unknown date format */
  }
				/* minimal validity check of date */
  if ((d > 31) || (m > 12)) return NIL; 
  if (y < 49) y += 2000;	/* RFC 2282 rules for two digit years 00-49 */
  else if (y < 999) y += 1900;	/* 2-digit years 50-99 and 3-digit years */
				/* reject prehistoric and far future years */
  if ((y < BASEYEAR) || (y > maxyear)) return NIL;
				/* set values in elt */
  elt->day = d; elt->month = m; elt->year = y - BASEYEAR;

  ms = '\0';			/* initially no time zone string */
  if (*s) {			/* time specification present? */
				/* parse time */
    d = strtoul (s+1,(char **) &s,10);
    if (*s != ':') return NIL;
    m = strtoul (++s,(char **) &s,10);
    y = (*s == ':') ? strtoul (++s,(char **) &s,10) : 0;
				/* validity check time */
    if ((d > 23) || (m > 59) || (y > 60)) return NIL; 
				/* set values in elt */
    elt->hours = d; elt->minutes = m; elt->seconds = y;
    switch (*s) {		/* time zone specifier? */
    case ' ':			/* numeric time zone */
      while (s[1] == ' ') s++;	/* slurp extra whitespace */
      if (!isalpha (s[1])) {	/* treat as '-' case if alphabetic */
				/* test for sign character */
	if ((elt->zoccident = (*++s == '-')) || (*s == '+')) s++;
				/* validate proper timezone */
	if (isdigit(*s) && isdigit(s[1]) && isdigit(s[2]) && (s[2] < '6') &&
	    isdigit(s[3])) {
	  elt->zhours = (*s - '0') * 10 + (s[1] - '0');
	  elt->zminutes = (s[2] - '0') * 10 + (s[3] - '0');
	}
	return T;		/* all done! */
      }
				/* falls through */
    case '-':			/* symbolic time zone */
      if (!(ms = *++s)) ms = 'Z';
      else if (*++s) {		/* multi-character? */
	ms -= 'A'; ms *= 1024;	/* yes, make compressed three-byte form */
	ms += ((*s++ - 'A') * 32);
	if (*s) ms += *s++ - 'A';
	if (*s) ms = '\0';	/* more than three characters */
      }
    default:			/* ignore anything else */
      break;
    }
  }

  /*  This is not intended to be a comprehensive list of all possible
   * timezone strings.  Such a list would be impractical.  Rather, this
   * listing is intended to incorporate all military, North American, and
   * a few special cases such as Japan and the major European zone names,
   * such as what might be expected to be found in a Tenex format mailbox
   * and spewed from an IMAP server.  The trend is to migrate to numeric
   * timezones which lack the flavor but also the ambiguity of the names.
   *
   *  RFC-822 only recognizes UT, GMT, 1-letter military timezones, and the
   * 4 CONUS timezones and their summer time variants.  [Sorry, Canadian
   * Atlantic Provinces, Alaska, and Hawaii.]
   */
  switch (ms) {			/* determine the timezone */
				/* Universal */
  case (('U'-'A')*1024)+(('T'-'A')*32):
#ifndef STRICT_RFC822_TIMEZONES
  case (('U'-'A')*1024)+(('T'-'A')*32)+'C'-'A':
#endif
				/* Greenwich */
  case (('G'-'A')*1024)+(('M'-'A')*32)+'T'-'A':
  case 'Z': elt->zhours = 0; break;

    /* oriental (from Greenwich) timezones */
#ifndef STRICT_RFC822_TIMEZONES
				/* Middle Europe */
  case (('M'-'A')*1024)+(('E'-'A')*32)+'T'-'A':
#endif
#ifdef BRITISH_SUMMER_TIME
				/* British Summer */
  case (('B'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#endif
  case 'A': elt->zhours = 1; break;
#ifndef STRICT_RFC822_TIMEZONES
				/* Eastern Europe */
  case (('E'-'A')*1024)+(('E'-'A')*32)+'T'-'A':
#endif
  case 'B': elt->zhours = 2; break;
  case 'C': elt->zhours = 3; break;
  case 'D': elt->zhours = 4; break;
  case 'E': elt->zhours = 5; break;
  case 'F': elt->zhours = 6; break;
  case 'G': elt->zhours = 7; break;
  case 'H': elt->zhours = 8; break;
#ifndef STRICT_RFC822_TIMEZONES
				/* Japan */
  case (('J'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#endif
  case 'I': elt->zhours = 9; break;
  case 'K': elt->zhours = 10; break;
  case 'L': elt->zhours = 11; break;
  case 'M': elt->zhours = 12; break;

	/* occidental (from Greenwich) timezones */
  case 'N': elt->zoccident = 1; elt->zhours = 1; break;
  case 'O': elt->zoccident = 1; elt->zhours = 2; break;
#ifndef STRICT_RFC822_TIMEZONES
  case (('A'-'A')*1024)+(('D'-'A')*32)+'T'-'A':
#endif
  case 'P': elt->zoccident = 1; elt->zhours = 3; break;
#ifdef NEWFOUNDLAND_STANDARD_TIME
				/* Newfoundland */
  case (('N'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
    elt->zoccident = 1; elt->zhours = 3; elt->zminutes = 30; break;
#endif
#ifndef STRICT_RFC822_TIMEZONES
				/* Atlantic */
  case (('A'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#endif
	/* CONUS */
  case (('E'-'A')*1024)+(('D'-'A')*32)+'T'-'A':
  case 'Q': elt->zoccident = 1; elt->zhours = 4; break;
				/* Eastern */
  case (('E'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
  case (('C'-'A')*1024)+(('D'-'A')*32)+'T'-'A':
  case 'R': elt->zoccident = 1; elt->zhours = 5; break;
				/* Central */
  case (('C'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
  case (('M'-'A')*1024)+(('D'-'A')*32)+'T'-'A':
  case 'S': elt->zoccident = 1; elt->zhours = 6; break;
				/* Mountain */
  case (('M'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
  case (('P'-'A')*1024)+(('D'-'A')*32)+'T'-'A':
  case 'T': elt->zoccident = 1; elt->zhours = 7; break;
				/* Pacific */
  case (('P'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#ifndef STRICT_RFC822_TIMEZONES
  case (('Y'-'A')*1024)+(('D'-'A')*32)+'T'-'A':
#endif
  case 'U': elt->zoccident = 1; elt->zhours = 8; break;
#ifndef STRICT_RFC822_TIMEZONES
				/* Yukon */
  case (('Y'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#endif
  case 'V': elt->zoccident = 1; elt->zhours = 9; break;
#ifndef STRICT_RFC822_TIMEZONES
				/* Hawaii */
  case (('H'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#endif
  case 'W': elt->zoccident = 1; elt->zhours = 10; break;
				/* Nome/Bering/Samoa */
#ifdef NOME_STANDARD_TIME
  case (('N'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#endif
#ifdef BERING_STANDARD_TIME
  case (('B'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#endif
#ifdef SAMOA_STANDARD_TIME
  case (('S'-'A')*1024)+(('S'-'A')*32)+'T'-'A':
#endif
  case 'X': elt->zoccident = 1; elt->zhours = 11; break;
  case 'Y': elt->zoccident = 1; elt->zhours = 12; break;

  default:			/* unknown time zones treated as local */
    tn = time (0);		/* time now... */
    t = localtime (&tn);	/* get local minutes since midnight */
    mi = t->tm_hour * 60 + t->tm_min;
    ms = t->tm_yday;		/* note Julian day */
    if (t = gmtime (&tn)) {	/* minus UTC minutes since midnight */
      mi -= t->tm_hour * 60 + t->tm_min;
	/* ms can be one of:
	 *  36x  local time is December 31, UTC is January 1, offset -24 hours
	 *    1  local time is 1 day ahead of UTC, offset +24 hours
	 *    0  local time is same day as UTC, no offset
	 *   -1  local time is 1 day behind UTC, offset -24 hours
	 * -36x  local time is January 1, UTC is December 31, offset +24 hours
	 */
      if (ms -= t->tm_yday)	/* correct offset if different Julian day */
	mi += ((ms < 0) == (abs (ms) == 1)) ? -24*60 : 24*60;
      if (mi < 0) {		/* occidental? */
	mi = abs (mi);		/* yup, make positive number */
	elt->zoccident = 1;	/* and note west of UTC */
      }
      elt->zhours = mi / 60;	/* now break into hours and minutes */
      elt->zminutes = mi % 60;
    }
    break;
  }
  return T;
}

/* Mail n messages exist
 * Accepts: mail stream
 *	    number of messages
 */

void mail_exists (MAILSTREAM *stream,unsigned long nmsgs)
{
  char tmp[MAILTMPLEN];
  if (nmsgs > MAXMESSAGES) {
    sprintf (tmp,"Mailbox has more messages (%lu) exist than maximum (%lu)",
	     nmsgs,MAXMESSAGES);
    mm_log (tmp,ERROR);
  }
  else {
				/* make sure cache is large enough */
    (*mailcache) (stream,nmsgs,CH_SIZE);
    stream->nmsgs = nmsgs;	/* update stream status */
				/* notify main program of change */
    if (!stream->silent) MM_EXISTS (stream,nmsgs);
  }
}


/* Mail n messages are recent
 * Accepts: mail stream
 *	    number of recent messages
 */

void mail_recent (MAILSTREAM *stream,unsigned long recent)
{
  char tmp[MAILTMPLEN];
  if (recent <= stream->nmsgs) stream->recent = recent;
  else {
    sprintf (tmp,"Non-existent recent message(s) %lu, nmsgs=%lu",
	     recent,stream->nmsgs);
    mm_log (tmp,ERROR);
  }
}


/* Mail message n is expunged
 * Accepts: mail stream
 *	    message #
 */

void mail_expunged (MAILSTREAM *stream,unsigned long msgno)
{
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  if (msgno > stream->nmsgs) {
    sprintf (tmp,"Expunge of non-existent message %lu, nmsgs=%lu",
	     msgno,stream->nmsgs);
    mm_log (tmp,ERROR);
  }
  else {
    elt = (MESSAGECACHE *) (*mailcache) (stream,msgno,CH_ELT);
				/* notify main program of change */
    if (!stream->silent) MM_EXPUNGED (stream,msgno);
    if (elt) {			/* if an element is there */
      elt->msgno = 0;		/* invalidate its message number and free */
      (*mailcache) (stream,msgno,CH_FREE);
      (*mailcache) (stream,msgno,CH_FREESORTCACHE);
    }
				/* expunge the slot */
    (*mailcache) (stream,msgno,CH_EXPUNGE);
    --stream->nmsgs;		/* update stream status */
    if (stream->msgno) {	/* have stream pointers? */
				/* make sure the short cache is nuked */
      if (stream->scache) mail_gc (stream,GC_ENV | GC_TEXTS);
      else stream->msgno = 0;	/* make sure invalidated in any case */
    }
  }
}

/* Mail stream status routines */


/* Mail lock stream
 * Accepts: mail stream
 */

void mail_lock (MAILSTREAM *stream)
{
  if (stream->lock) {
    char tmp[MAILTMPLEN];
    sprintf (tmp,"Lock when already locked, mbx=%.80s",
	     stream->mailbox ? stream->mailbox : "???");
    fatal (tmp);
  }
  else stream->lock = T;	/* lock stream */
}


/* Mail unlock stream
 * Accepts: mail stream
 */
 
void mail_unlock (MAILSTREAM *stream)
{
  if (!stream->lock) fatal ("Unlock when not locked");
  else stream->lock = NIL;	/* unlock stream */
}


/* Mail turn on debugging telemetry
 * Accepts: mail stream
 */

void mail_debug (MAILSTREAM *stream)
{
  stream->debug = T;		/* turn on debugging telemetry */
  if (stream->dtb) (*stream->dtb->parameters) (ENABLE_DEBUG,stream);
}


/* Mail turn off debugging telemetry
 * Accepts: mail stream
 */

void mail_nodebug (MAILSTREAM *stream)
{
  stream->debug = NIL;		/* turn off debugging telemetry */
  if (stream->dtb) (*stream->dtb->parameters) (DISABLE_DEBUG,stream);
}


/* Mail log to debugging telemetry
 * Accepts: message
 *	    flag that data is "sensitive"
 */

void mail_dlog (char *string,long flag)
{
  mm_dlog ((debugsensitive || !flag) ? string : "<suppressed>");
}

/* Mail parse UID sequence
 * Accepts: mail stream
 *	    sequence to parse
 * Returns: T if parse successful, else NIL
 */

long mail_uid_sequence (MAILSTREAM *stream,unsigned char *sequence)
{
  unsigned long i,j,k,x,y;
  for (i = 1; i <= stream->nmsgs; i++) mail_elt (stream,i)->sequence = NIL;
  while (sequence && *sequence){/* while there is something to parse */
    if (*sequence == '*') {	/* maximum message */
      i = stream->nmsgs ? mail_uid (stream,stream->nmsgs) : stream->uid_last;
      sequence++;		/* skip past * */
    }
				/* parse and validate message number */
				/* parse and validate message number */
    else if (!isdigit (*sequence)) {
      MM_LOG ("Syntax error in sequence",ERROR);
      return NIL;
    }
    else if (!(i = strtoul (sequence,(char **) &sequence,10))) {
      MM_LOG ("UID may not be zero",ERROR);
      return NIL;
    }
    switch (*sequence) {	/* see what the delimiter is */
    case ':':			/* sequence range */
      if (*++sequence == '*') {	/* maximum message */
	j = stream->nmsgs ? mail_uid (stream,stream->nmsgs) : stream->uid_last;
	sequence++;		/* skip past * */
      }
				/* parse end of range */
      else if (!(j = strtoul (sequence,(char **) &sequence,10))) {
	MM_LOG ("UID sequence range invalid",ERROR);
	return NIL;
      }
      if (*sequence && *sequence++ != ',') {
	MM_LOG ("UID sequence range syntax error",ERROR);
	return NIL;
      }
      if (i > j) {		/* swap the range if backwards */
	x = i; i = j; j = x;
      }
      x = mail_msgno (stream,i);/* get msgnos */
      y = mail_msgno (stream,j);/* for both UIDS (don't && it) */
				/* easy if both UIDs valid */
      if (x && y) while (x <= y) mail_elt (stream,x++)->sequence = T;
				/* start UID valid, end is not */
      else if (x) while ((x <= stream->nmsgs) && (mail_uid (stream,x) <= j))
	mail_elt (stream,x++)->sequence = T;
				/* end UID valid, start is not */
      else if (y) for (x = 1; x <= y; x++) {
	if (mail_uid (stream,x) >= i) mail_elt (stream,x)->sequence = T;
      }
				/* neither is valid, ugh */
      else for (x = 1; x <= stream->nmsgs; x++)
	if (((k = mail_uid (stream,x)) >= i) && (k <= j))
	  mail_elt (stream,x)->sequence = T;
      break;
    case ',':			/* single message */
      ++sequence;		/* skip the delimiter, fall into end case */
    case '\0':			/* end of sequence, mark this message */
      if (x = mail_msgno (stream,i)) mail_elt (stream,x)->sequence = T;
      break;
    default:			/* anything else is a syntax error! */
      MM_LOG ("UID sequence syntax error",ERROR);
      return NIL;
    }
  }
  return T;			/* successfully parsed sequence */
}

/* Mail see if line list matches that in cache
 * Accepts: candidate line list
 *	    cached line list
 *	    matching flags
 * Returns: T if match, NIL if no match
 */

long mail_match_lines (STRINGLIST *lines,STRINGLIST *msglines,long flags)
{
  unsigned long i;
  unsigned char *s,*t;
  STRINGLIST *m;
  if (!msglines) return T;	/* full header is in cache */
				/* need full header but filtered in cache */
  if ((flags & FT_NOT) || !lines) return NIL;
  do {				/* make sure all present & accounted for */
    for (m = msglines; m; m = m->next) if (lines->text.size == m->text.size) {
      for (s = lines->text.data,t = m->text.data,i = lines->text.size;
	   i && ((islower (*s) ? (*s-('a'-'A')) : *s) ==
		 (islower (*t) ? (*t-('a'-'A')) : *t)); s++,t++,i--);
      if (!i) break;		/* this line matches */
    }
    if (!m) return NIL;		/* didn't find in the list */
  }
  while (lines = lines->next);
  return T;			/* all lines found */
}

/* Mail filter text by header lines
 * Accepts: text to filter, with trailing null
 *	    length of text
 *	    list of lines
 *	    fetch flags
 * Returns: new text size, text overwritten
 */

unsigned long mail_filter (char *text,unsigned long len,STRINGLIST *lines,
			   long flags)
{
  STRINGLIST *hdrs;
  int notfound;
  unsigned long i;
  char c,*s,*e,*t,tmp[MAILTMPLEN];
  char *src = text;
  char *dst = src;
  char *end = text + len;
  text[len] = '\012';		/* guard against running off buffer */
  while (src < end) {		/* process header */
				/* slurp header line name */
    for (s = src,e = s + MAILTMPLEN - 1,e = (e < end ? e : end),t = tmp;
	 (s < e) && ((c = (*s ? *s : (*s = ' '))) != ':') &&
	 ((c > ' ') ||
	  ((c != ' ') && (c != '\t') && (c != '\015') && (c != '\012')));
	 *t++ = *s++);
    *t = '\0';			/* tie off */
    notfound = T;		/* not found yet */
    if (i = t - tmp)		/* see if found in header */
      for (hdrs = lines; hdrs && notfound; hdrs = hdrs->next)
	if ((hdrs->text.size == i) && !compare_csizedtext (tmp,&hdrs->text))
	  notfound = NIL;
				/* skip header line if not wanted */
    if (i && ((flags & FT_NOT) ? !notfound : notfound))
      while (((*src++ != '\012') && (*src++ != '\012') && (*src++ != '\012') &&
	      (*src++ != '\012') && (*src++ != '\012') && (*src++ != '\012') &&
	      (*src++ != '\012') && (*src++ != '\012') && (*src++ != '\012') &&
	      (*src++ != '\012')) || ((*src == ' ') || (*src == '\t')));
    else if (src == dst) {	/* copy to self */
      while (((*src++ != '\012') && (*src++ != '\012') && (*src++ != '\012') &&
	      (*src++ != '\012') && (*src++ != '\012') && (*src++ != '\012') &&
	      (*src++ != '\012') && (*src++ != '\012') && (*src++ != '\012') &&
	      (*src++ != '\012')) || ((*src == ' ') || (*src == '\t')));
      dst = src;		/* update destination */
    }
    else			/* copy line and any continuation line */
      while ((((*dst++ = *src++) != '\012') && ((*dst++ = *src++) != '\012') &&
	      ((*dst++ = *src++) != '\012') && ((*dst++ = *src++) != '\012') &&
	      ((*dst++ = *src++) != '\012') && ((*dst++ = *src++) != '\012') &&
	      ((*dst++ = *src++) != '\012') && ((*dst++ = *src++) != '\012') &&
	      ((*dst++ = *src++) != '\012') && ((*dst++ = *src++) != '\012'))||
	     ((*src == ' ') || (*src == '\t')));
  }
  *dst = '\0';			/* tie off destination */
  return dst - text;
}

/* Local mail search message
 * Accepts: MAIL stream
 *	    message number
 *	    optional section specification
 *	    search program
 * Returns: T if found, NIL otherwise
 */

long mail_search_msg (MAILSTREAM *stream,unsigned long msgno,char *section,
		      SEARCHPGM *pgm)
{
  unsigned short d;
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt = mail_elt (stream,msgno);
  SEARCHHEADER *hdr;
  SEARCHOR *or;
  SEARCHPGMLIST *not;
  if (pgm->msgno || pgm->uid) {	/* message set searches */
    SEARCHSET *set;
				/* message sequences */
    if (pgm->msgno) {		/* inside this message sequence set */
      for (set = pgm->msgno; set; set = set->next)
	if (set->last ? ((set->first <= set->last) ?
			 ((msgno >= set->first) && (msgno <= set->last)) :
			 ((msgno >= set->last) && (msgno <= set->first))) :
	    msgno == set->first) break;
      if (!set) return NIL;	/* not found within sequence */
    }
    if (pgm->uid) {		/* inside this unique identifier set */
      unsigned long uid = mail_uid (stream,msgno);
      for (set = pgm->uid; set; set = set->next)
	if (set->last ? ((set->first <= set->last) ?
			 ((uid >= set->first) && (uid <= set->last)) :
			 ((uid >= set->last) && (uid <= set->first))) :
	    uid == set->first) break;
      if (!set) return NIL;	/* not found within sequence */
    }
  }

  /* Fast data searches */
				/* need to fetch fast data? */
  if ((!elt->rfc822_size && (pgm->larger || pgm->smaller)) ||
      (!elt->year && (pgm->before || pgm->on || pgm->since)) ||
      (!elt->valid && (pgm->answered || pgm->unanswered ||
		       pgm->deleted || pgm->undeleted ||
		       pgm->draft || pgm->undraft ||
		       pgm->flagged || pgm->unflagged ||
		       pgm->recent || pgm->old ||
		       pgm->seen || pgm->unseen ||
		       pgm->keyword || pgm->unkeyword))) {
    unsigned long i;
    MESSAGECACHE *ielt;
    for (i = elt->msgno;	/* find last unloaded message in range */
	 (i < stream->nmsgs) && (ielt = mail_elt (stream,i+1)) &&
	   ((!ielt->rfc822_size && (pgm->larger || pgm->smaller)) ||
	    (!ielt->year && (pgm->before || pgm->on || pgm->since)) ||
	    (!ielt->valid && (pgm->answered || pgm->unanswered ||
			      pgm->deleted || pgm->undeleted ||
			      pgm->draft || pgm->undraft ||
			      pgm->flagged || pgm->unflagged ||
			      pgm->recent || pgm->old ||
			      pgm->seen || pgm->unseen ||
			      pgm->keyword || pgm->unkeyword))); ++i);
    if (i == elt->msgno) sprintf (tmp,"%lu",elt->msgno);
    else sprintf (tmp,"%lu:%lu",elt->msgno,i);
    mail_fetch_fast (stream,tmp,NIL);
  }
				/* size ranges */
  if ((pgm->larger && (elt->rfc822_size <= pgm->larger)) ||
      (pgm->smaller && (elt->rfc822_size >= pgm->smaller))) return NIL;
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
      (pgm->unkeyword && !mail_search_keyword (stream,elt,pgm->unkeyword,NIL)))
    return NIL;
				/* internal date ranges */
  if (pgm->before || pgm->on || pgm->since) {
    d = mail_shortdate (elt->year,elt->month,elt->day);
    if (pgm->before && (d >= pgm->before)) return NIL;
    if (pgm->on && (d != pgm->on)) return NIL;
    if (pgm->since && (d < pgm->since)) return NIL;
  }

				/* envelope searches */
  if (pgm->sentbefore || pgm->senton || pgm->sentsince ||
      pgm->bcc || pgm->cc || pgm->from || pgm->to || pgm->subject ||
      pgm->return_path || pgm->sender || pgm->reply_to || pgm->in_reply_to ||
      pgm->message_id || pgm->newsgroups || pgm->followup_to ||
      pgm->references) {
    ENVELOPE *env;
    MESSAGECACHE delt;
    if (section) {		/* use body part envelope */
      BODY *body = mail_body (stream,msgno,section);
      env = (body && (body->type == TYPEMESSAGE) && body->subtype &&
	     !strcmp (body->subtype,"RFC822")) ? body->nested.msg->env : NIL;
    }
    else {			/* use top level envelope if no section */
      if (pgm->header && !stream->scache && !(stream->dtb->flags & DR_LOCAL))
	mail_fetch_header(stream,msgno,NIL,NIL,NIL,FT_PEEK|FT_SEARCHLOOKAHEAD);
      env = mail_fetchenvelope (stream,msgno);
    }
    if (!env) return NIL;	/* no envelope obtained */
				/* sent date ranges */
    if ((pgm->sentbefore || pgm->senton || pgm->sentsince) &&
	(!mail_parse_date (&delt,env->date) ||
	 !(d = mail_shortdate (delt.year,delt.month,delt.day)) ||
	 (pgm->sentbefore && (d >= pgm->sentbefore)) ||
	 (pgm->senton && (d != pgm->senton)) ||
	 (pgm->sentsince && (d < pgm->sentsince)))) return NIL;
				/* search headers */
    if ((pgm->bcc && !mail_search_addr (env->bcc,pgm->bcc)) ||
	(pgm->cc && !mail_search_addr (env->cc,pgm->cc)) ||
	(pgm->from && !mail_search_addr (env->from,pgm->from)) ||
	(pgm->to && !mail_search_addr (env->to,pgm->to)) ||
	(pgm->subject && !mail_search_header_text (env->subject,pgm->subject)))
      return NIL;
    /* These criteria are not supported by IMAP and have to be emulated */
    if ((pgm->return_path &&
	 !mail_search_addr (env->return_path,pgm->return_path)) ||
	(pgm->sender && !mail_search_addr (env->sender,pgm->sender)) ||
	(pgm->reply_to && !mail_search_addr (env->reply_to,pgm->reply_to)) ||
	(pgm->in_reply_to &&
	 !mail_search_header_text (env->in_reply_to,pgm->in_reply_to)) ||
	(pgm->message_id &&
	!mail_search_header_text (env->message_id,pgm->message_id)) ||
	(pgm->newsgroups &&
	 !mail_search_header_text (env->newsgroups,pgm->newsgroups)) ||
	(pgm->followup_to &&
	 !mail_search_header_text (env->followup_to,pgm->followup_to)) ||
	(pgm->references &&
	 !mail_search_header_text (env->references,pgm->references)))
      return NIL;
  }

				/* search header lines */
  for (hdr = pgm->header; hdr; hdr = hdr->next) {
    char *t,*e,*v;
    SIZEDTEXT s;
    STRINGLIST sth,stc;
    sth.next = stc.next = NIL;	/* only one at a time */
    sth.text.data = hdr->line.data;
    sth.text.size = hdr->line.size;
				/* get the header text */
    if ((t = mail_fetch_header (stream,msgno,NIL,&sth,&s.size,
				FT_INTERNAL | FT_PEEK |
				(section ? NIL : FT_SEARCHLOOKAHEAD))) &&
	strchr (t,':')) {
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
	  *v++ = '\n';		/* tie off line */
	  while (((*t == '\015') || (*t == '\012')) && (t < e)) t++;
	}
				/* calculate true size */
	s.size = v - (char *) s.data;
	*v = '\0';		/* tie off results */
	stc.text.data = hdr->text.data;
	stc.text.size = hdr->text.size;
				/* search header */
	if (mail_search_header (&s,&stc)) fs_give ((void **) &s.data);
	else {			/* search failed */
	  fs_give ((void **) &s.data);
	  return NIL;
	}
      }
    }
    else return NIL;		/* no matching header text */
  }
				/* search strings */
  if ((pgm->text && !mail_search_text (stream,msgno,section,pgm->text,LONGT))||
      (pgm->body && !mail_search_text (stream,msgno,section,pgm->body,NIL)))
    return NIL;
				/* logical conditions */
  for (or = pgm->or; or; or = or->next)
    if (!(mail_search_msg (stream,msgno,section,or->first) ||
	  mail_search_msg (stream,msgno,section,or->second))) return NIL;
  for (not = pgm->not; not; not = not->next)
    if (mail_search_msg (stream,msgno,section,not->pgm)) return NIL;
  return T;
}

/* Mail search message header null-terminated text
 * Accepts: header text
 *	    strings to search
 * Returns: T if search found a match
 */

long mail_search_header_text (char *s,STRINGLIST *st)
{
  SIZEDTEXT h;
				/* have any text? */
  if (h.data = (unsigned char *) s) {
    h.size = strlen (s);	/* yes, get its size */
    return mail_search_header (&h,st);
  }
  return NIL;
}


/* Mail search message header
 * Accepts: header as sized text
 *	    strings to search
 * Returns: T if search found a match
 */

long mail_search_header (SIZEDTEXT *hdr,STRINGLIST *st)
{
  SIZEDTEXT h;
  long ret = LONGT;
  utf8_mime2text (hdr,&h);	/* make UTF-8 version of header */
  while (h.size && ((h.data[h.size-1]=='\015') || (h.data[h.size-1]=='\012')))
    --h.size;			/* slice off trailing newlines */
  do if (h.size ?		/* search non-empty string */
	 !search (h.data,h.size,st->text.data,st->text.size) : st->text.size)
    ret = NIL;
  while (ret && (st = st->next));
  if (h.data != hdr->data) fs_give ((void **) &h.data);
  return ret;
}

/* Mail search message body
 * Accepts: MAIL stream
 *	    message number
 *	    optional section specification
 *	    string list
 *	    flags
 * Returns: T if search found a match
 */

long mail_search_text (MAILSTREAM *stream,unsigned long msgno,char *section,
		       STRINGLIST *st,long flags)
{
  BODY *body;
  long ret = NIL;
  STRINGLIST *s = mail_newstringlist ();
  mailgets_t omg = mailgets;
  if (stream->dtb->flags & DR_LOWMEM) mailgets = mail_search_gets;
				/* strings to search */
  for (stream->private.search.string = s; st;) {
    s->text.data = st->text.data;
    s->text.size = st->text.size;
    if (st = st->next) s = s->next = mail_newstringlist ();
  }
  stream->private.search.text = NIL;
  if (flags) {			/* want header? */
    SIZEDTEXT s,t;
    s.data = (unsigned char *)
      mail_fetch_header (stream,msgno,section,NIL,&s.size,FT_INTERNAL|FT_PEEK);
    utf8_mime2text (&s,&t);
    ret = mail_search_string (&t,"UTF-8",&stream->private.search.string);
    if (t.data != s.data) fs_give ((void **) &t.data);
  }
  if (!ret) {			/* still looking for match? */
				/* no section, get top-level body */
    if (!section) mail_fetchstructure (stream,msgno,&body);
				/* get body of nested message */
    else if ((body = mail_body (stream,msgno,section)) &&
	     (body->type == TYPEMULTIPART) && body->subtype &&
	     !strcmp (body->subtype,"RFC822")) body = body->nested.msg->body;
    if (body) ret = mail_search_body (stream,msgno,body,NIL,1,flags);
  }
  mailgets = omg;		/* restore former gets routine */
				/* clear searching */
  for (s = stream->private.search.string; s; s = s->next) s->text.data = NIL;
  mail_free_stringlist (&stream->private.search.string);
  stream->private.search.text = NIL;
  return ret;
}

/* Mail search message body text parts
 * Accepts: MAIL stream
 *	    message number
 *	    current body pointer
 *	    hierarchical level prefix
 *	    position at current hierarchical level
 *	    string list
 *	    flags
 * Returns: T if search found a match
 */

long mail_search_body (MAILSTREAM *stream,unsigned long msgno,BODY *body,
		       char *prefix,unsigned long section,long flags)
{
  long ret = NIL;
  unsigned long i;
  char *s,*t,sect[MAILTMPLEN];
  SIZEDTEXT st,h;
  PART *part;
  PARAMETER *param;
  if (prefix && (strlen (prefix) > (MAILTMPLEN - 20))) return NIL;
  sprintf (sect,"%s%lu",prefix ? prefix : "",section++);
  if (flags && prefix) {	/* want to search MIME header too? */
    st.data = (unsigned char *) mail_fetch_mime (stream,msgno,sect,&st.size,
						 FT_INTERNAL | FT_PEEK);
    if (stream->dtb->flags & DR_LOWMEM) ret = stream->private.search.result;
    else {
      utf8_mime2text (&st,&h);	/* make UTF-8 version of header */
      ret = mail_search_string (&h,"UTF-8",&stream->private.search.string);
      if (h.data != st.data) fs_give ((void **) &h.data);
    }
  }
  if (!ret) switch (body->type) {
  case TYPEMULTIPART:
				/* extend prefix if not first time */
    s = prefix ? strcat (sect,".") : "";
    for (i = 1,part = body->nested.part; part && !ret; i++,part = part->next)
      ret = mail_search_body (stream,msgno,&part->body,s,i,flags);
    break;
  case TYPEMESSAGE:
    if (!strcmp (body->subtype,"RFC822")) {
      if (flags) {		/* want to search nested message header? */
	st.data = (unsigned char *)
	  mail_fetch_header (stream,msgno,sect,NIL,&st.size,
			     FT_INTERNAL | FT_PEEK);
	if (stream->dtb->flags & DR_LOWMEM) ret =stream->private.search.result;
	else {
	  utf8_mime2text (&st,&h);/* make UTF-8 version of header */
	  ret = mail_search_string (&h,"UTF-8",&stream->private.search.string);
	  if (h.data != st.data) fs_give ((void **) &h.data);
	}
      }
      if (body = body->nested.msg->body)
	ret = (body->type == TYPEMULTIPART) ?
	  mail_search_body (stream,msgno,body,(prefix ? prefix : ""),
			    section - 1,flags) :
	mail_search_body (stream,msgno,body,strcat (sect,"."),1,flags);
      break;
    }
				/* non-MESSAGE/RFC822 falls into text case */

  case TYPETEXT:
    s = mail_fetch_body (stream,msgno,sect,&i,FT_INTERNAL | FT_PEEK);
    if (stream->dtb->flags & DR_LOWMEM) ret = stream->private.search.result;
    else {
      for (t = NIL,param = body->parameter; param && !t; param = param->next)
	if (!strcmp (param->attribute,"CHARSET")) t = param->value;
      switch (body->encoding) {	/* what encoding? */
      case ENCBASE64:
	if (st.data = (unsigned char *)
	    rfc822_base64 ((unsigned char *) s,i,&st.size)) {
	  ret = mail_search_string (&st,t,&stream->private.search.string);
	  fs_give ((void **) &st.data);
	}
	break;
      case ENCQUOTEDPRINTABLE:
	if (st.data = rfc822_qprint ((unsigned char *) s,i,&st.size)) {
	  ret = mail_search_string (&st,t,&stream->private.search.string);
	  fs_give ((void **) &st.data);
	}
	break;
      default:
	st.data = (unsigned char *) s;
	st.size = i;
	ret = mail_search_string (&st,t,&stream->private.search.string);
	break;
      }
    }
    break;
  }
  return ret;
}

/* Mail search text
 * Accepts: sized text to search
 *	    character set of sized text
 *	    string list of search keys
 * Returns: T if search found a match
 */

long mail_search_string (SIZEDTEXT *s,char *charset,STRINGLIST **st)
{
  void *t;
  SIZEDTEXT u;
  STRINGLIST **sc = st;
				/* convert to UTF-8 as best we can */
  if (!utf8_text (s,charset,&u,NIL)) utf8_text (s,NIL,&u,NIL);
  while (*sc) {			/* run down criteria list */
    if (search (u.data,u.size,(*sc)->text.data,(*sc)->text.size)) {
      t = (void *) (*sc);	/* found one, need to flush this */
      *sc = (*sc)->next;	/* remove it from the list */
      fs_give (&t);		/* flush the buffer */
    }
    else sc = &(*sc)->next;	/* move to next in list */
  }
  if (u.data != s->data) fs_give ((void **) &u.data);
  return *st ? NIL : LONGT;
}


/* Mail search keyword
 * Accepts: MAIL stream
 *	    elt to get flags from
 *	    keyword list
 *	    T for keyword search, NIL for unkeyword search
 * Returns: T if search found a match
 */

long mail_search_keyword (MAILSTREAM *stream,MESSAGECACHE *elt,STRINGLIST *st,
			  long flag)
{
  int i,j;
  unsigned long f = 0;
  unsigned long tf;
  do {
    for (i = 0; (j = (i < NUSERFLAGS) && stream->user_flags[i]); ++i)
      if (!compare_csizedtext (stream->user_flags[i],&st->text)) {
	f |= (1 << i);
	break;
      }
    if (flag && !j) return NIL;
  } while (st = st->next);
  tf = elt->user_flags & f;	/* get set flags which match */
  return flag ? (f == tf) : !tf;
}

/* Mail search an address list
 * Accepts: address list
 *	    string list
 * Returns: T if search found a match
 */

#define SEARCHBUFLEN (size_t) 2000
#define SEARCHBUFSLOP (size_t) 5

long mail_search_addr (ADDRESS *adr,STRINGLIST *st)
{
  ADDRESS *a,tadr;
  SIZEDTEXT txt;
  char tmp[MAILTMPLEN];
  size_t i = SEARCHBUFLEN;
  size_t k;
  long ret = NIL;
  if (adr) {
    txt.data = (unsigned char *) fs_get (i + SEARCHBUFSLOP);
				/* never an error or next */
    tadr.error = NIL,tadr.next = NIL;
				/* write address list */
    for (txt.size = 0,a = adr; a; a = a->next) {
      k = (tadr.mailbox = a->mailbox) ? 4 + 2*strlen (a->mailbox) : 3;
      if (tadr.personal = a->personal) k += 3 + 2*strlen (a->personal);
      if (tadr.adl = a->adl) k += 3 + 2*strlen (a->adl);
      if (tadr.host = a->host) k += 3 + 2*strlen (a->host);
      if (tadr.personal || tadr.adl) k += 2;
      if (k < (MAILTMPLEN-10)) { /* ignore ridiculous addresses */
	tmp[0] = '\0';
	rfc822_write_address (tmp,&tadr);
				/* resize buffer if necessary */
	if (((k = strlen (tmp)) + txt.size) > i)
	  fs_resize ((void **) &txt.data,SEARCHBUFSLOP + (i += SEARCHBUFLEN));
				/* add new address */
	memcpy (txt.data + txt.size,tmp,k);
	txt.size += k;
				/* another address follows */
	if (a->next) txt.data[txt.size++] = ',';
      }
    }
    txt.data[txt.size] = '\0';	/* tie off string */
    ret = mail_search_header (&txt,st);
    fs_give ((void **) &txt.data);
  }
  return ret;
}

/* Get string for low-memory searching
 * Accepts: readin function pointer
 *	    stream to use
 *	    number of bytes
 *	    gets data packet

 *	    mail stream
 *	    message number
 *	    descriptor string
 *	    option flags
 * Returns: NIL, always
 */

#define SEARCHSLOP 128

char *mail_search_gets (readfn_t f,void *stream,unsigned long size,
			GETS_DATA *md)
{
  unsigned long i;
  char tmp[MAILTMPLEN+SEARCHSLOP+1];
  SIZEDTEXT st;
				/* better not be called unless searching */
  if (!md->stream->private.search.string) {
    sprintf (tmp,"Search botch, mbx = %.80s, %s = %lu[%.80s]",
	     md->stream->mailbox,
	     (md->flags & FT_UID) ? "UID" : "msg",md->msgno,md->what);
    fatal (tmp);
  }
				/* initially no match for search */
  md->stream->private.search.result = NIL;
				/* make sure buffer clear */
  memset (st.data = (unsigned char *) tmp,'\0',
	  (size_t) MAILTMPLEN+SEARCHSLOP+1);
				/* read first buffer */
  (*f) (stream,st.size = i = min (size,(long) MAILTMPLEN),tmp);
				/* search for text */
  if (mail_search_string (&st,NIL,&md->stream->private.search.string))
    md->stream->private.search.result = T;
  else if (size -= i) {		/* more to do, blat slop down */
    memmove (tmp,tmp+MAILTMPLEN-SEARCHSLOP,(size_t) SEARCHSLOP);
    do {			/* read subsequent buffers one at a time */
      (*f) (stream,i = min (size,(long) MAILTMPLEN),tmp+SEARCHSLOP);
      st.size = i + SEARCHSLOP;
      if (mail_search_string (&st,NIL,&md->stream->private.search.string))
	md->stream->private.search.result = T;
      else memmove (tmp,tmp+MAILTMPLEN,(size_t) SEARCHSLOP);
    }
    while ((size -= i) && !md->stream->private.search.result);
  }
  if (size) {			/* toss out everything after that */
    do (*f) (stream,i = min (size,(long) MAILTMPLEN),tmp);
    while (size -= i);
  }
  return NIL;
}

/* Mail parse search criteria
 * Accepts: criteria
 * Returns: search program if parse successful, else NIL
 */

SEARCHPGM *mail_criteria (char *criteria)
{
  SEARCHPGM *pgm = NIL;
  char *criterion,tmp[MAILTMPLEN];
  int f;
  if (criteria) {		/* only if criteria defined */
				/* make writeable copy of criteria */
    criteria = cpystr (criteria);
				/* for each criterion */
    for (pgm = mail_newsearchpgm (), criterion = strtok (criteria," ");
	 criterion; (criterion = strtok (NIL," "))) {
      f = NIL;			/* init then scan the criterion */
      switch (*ucase (criterion)) {
      case 'A':			/* possible ALL, ANSWERED */
	if (!strcmp (criterion+1,"LL")) f = T;
	else if (!strcmp (criterion+1,"NSWERED")) f = pgm->answered = T;
	break;
      case 'B':			/* possible BCC, BEFORE, BODY */
	if (!strcmp (criterion+1,"CC")) f = mail_criteria_string (&pgm->bcc);
	else if (!strcmp (criterion+1,"EFORE"))
	  f = mail_criteria_date (&pgm->before);
	else if (!strcmp (criterion+1,"ODY"))
	  f = mail_criteria_string (&pgm->body);
	break;
      case 'C':			/* possible CC */
	if (!strcmp (criterion+1,"C")) f = mail_criteria_string (&pgm->cc);
	break;
      case 'D':			/* possible DELETED */
	if (!strcmp (criterion+1,"ELETED")) f = pgm->deleted = T;
	break;
      case 'F':			/* possible FLAGGED, FROM */
	if (!strcmp (criterion+1,"LAGGED")) f = pgm->flagged = T;
	else if (!strcmp (criterion+1,"ROM"))
	  f = mail_criteria_string (&pgm->from);
	break;
      case 'K':			/* possible KEYWORD */
	if (!strcmp (criterion+1,"EYWORD"))
	  f = mail_criteria_string (&pgm->keyword);
	break;

      case 'N':			/* possible NEW */
	if (!strcmp (criterion+1,"EW")) f = pgm->recent = pgm->unseen = T;
	break;
      case 'O':			/* possible OLD, ON */
	if (!strcmp (criterion+1,"LD")) f = pgm->old = T;
	else if (!strcmp (criterion+1,"N")) f = mail_criteria_date (&pgm->on);
	break;
      case 'R':			/* possible RECENT */
	if (!strcmp (criterion+1,"ECENT")) f = pgm->recent = T;
	break;
      case 'S':			/* possible SEEN, SINCE, SUBJECT */
	if (!strcmp (criterion+1,"EEN")) f = pgm->seen = T;
	else if (!strcmp (criterion+1,"INCE"))
	  f = mail_criteria_date (&pgm->since);
	else if (!strcmp (criterion+1,"UBJECT"))
	  f = mail_criteria_string (&pgm->subject);
	break;
      case 'T':			/* possible TEXT, TO */
	if (!strcmp (criterion+1,"EXT")) f = mail_criteria_string (&pgm->text);
	else if (!strcmp (criterion+1,"O"))
	  f = mail_criteria_string (&pgm->to);
	break;
      case 'U':			/* possible UN* */
	if (criterion[1] == 'N') {
	  if (!strcmp (criterion+2,"ANSWERED")) f = pgm->unanswered = T;
	  else if (!strcmp (criterion+2,"DELETED")) f = pgm->undeleted = T;
	  else if (!strcmp (criterion+2,"FLAGGED")) f = pgm->unflagged = T;
	  else if (!strcmp (criterion+2,"KEYWORD"))
	    f = mail_criteria_string (&pgm->unkeyword);
	  else if (!strcmp (criterion+2,"SEEN")) f = pgm->unseen = T;
	}
	break;
      default:			/* we will barf below */
	break;
      }
      if (!f) {			/* if can't identify criterion */
	sprintf (tmp,"Unknown search criterion: %.30s",criterion);
	MM_LOG (tmp,ERROR);
	mail_free_searchpgm (&pgm);
	break;
      }
    }
				/* no longer need copy of criteria */
    fs_give ((void **) &criteria);
  }
  return pgm;
}

/* Parse a date
 * Accepts: pointer to date integer to return
 * Returns: T if successful, else NIL
 */

int mail_criteria_date (unsigned short *date)
{
  STRINGLIST *s = NIL;
  MESSAGECACHE elt;
				/* parse the date and return fn if OK */
  int ret = (mail_criteria_string (&s) &&
	     mail_parse_date (&elt,(char *) s->text.data) &&
	     (*date = mail_shortdate (elt.year,elt.month,elt.day))) ?
	       T : NIL;
  if (s) mail_free_stringlist (&s);
  return ret;
}

/* Calculate shortdate from elt values
 * Accepts: year (0 = BASEYEAR)
 *	    month (1 = January)
 *	    day
 * Returns: shortdate
 */

unsigned short mail_shortdate (unsigned int year,unsigned int month,
			       unsigned int day)
{
  return (year << 9) + (month << 5) + day;
}

/* Parse a string
 * Accepts: pointer to stringlist
 * Returns: T if successful, else NIL
 */

int mail_criteria_string (STRINGLIST **s)
{
  unsigned long n;
  char e,*d,*end = " ",*c = strtok (NIL,"");
  if (!c) return NIL;		/* missing argument */
  switch (*c) {			/* see what the argument is */
  case '{':			/* literal string */
    n = strtoul (c+1,&d,10);	/* get its length */
    if ((*d++ == '}') && (*d++ == '\015') && (*d++ == '\012') &&
	(!(*(c = d + n)) || (*c == ' '))) {
      e = *--c;			/* store old delimiter */
      *c = '\377';		/* make sure not a space */
      strtok (c," ");		/* reset the strtok mechanism */
      *c = e;			/* put character back */
      break;
    }
  case '\0':			/* catch bogons */
  case ' ':
    return NIL;
  case '"':			/* quoted string */
    if (strchr (c+1,'"')) end = "\"";
    else return NIL;		/* falls through */
  default:			/* atomic string */
    if (d = strtok (c,end)) n = strlen (d);
    else return NIL;
    break;
  }
  while (*s) s = &(*s)->next;	/* find tail of list */
  *s = mail_newstringlist ();	/* make new entry */
				/* return the data */
  (*s)->text.data = (unsigned char *) cpystr (d);
  (*s)->text.size = n;
  return T;
}

/* Mail sort messages
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    sort program
 *	    option flags
 * Returns: vector of sorted message sequences or NIL if error
 */

unsigned long *mail_sort (MAILSTREAM *stream,char *charset,SEARCHPGM *spg,
			  SORTPGM *pgm,long flags)
{
  unsigned long *ret = NIL;
  if (stream->dtb)		/* do the driver's action */
    ret = (*(stream->dtb->sort ? stream->dtb->sort : mail_sort_msgs))
      (stream,charset,spg,pgm,flags);
				/* flush search/sort programs if requested */
  if (spg && (flags & SE_FREE)) mail_free_searchpgm (&spg);
  if (flags & SO_FREE) mail_free_sortpgm (&pgm);
  return ret;
}

/* Mail sort messages work routine
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    sort program
 *	    option flags
 * Returns: vector of sorted message sequences or NIL if error
 */

unsigned long *mail_sort_msgs (MAILSTREAM *stream,char *charset,SEARCHPGM *spg,
			       SORTPGM *pgm,long flags)
{
  unsigned long i;
  SORTCACHE **sc;
  unsigned long *ret = NIL;
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
  for (i = 1; i <= stream->nmsgs; ++i)
    if (mail_elt (stream,i)->searched) pgm->nmsgs++;
  if (pgm->nmsgs) {		/* pass 2: sort cache */
    sc = mail_sort_loadcache (stream,pgm);
				/* pass 3: sort messages */
    if (!pgm->abort) ret = mail_sort_cache (stream,pgm,sc,flags);
    fs_give ((void **) &sc);	/* don't need sort vector any more */
  }
				/* empty sort results */
  else ret = (unsigned long *) memset (fs_get (sizeof (unsigned long)),0,
				       sizeof (unsigned long));
				/* also return via callback if requested */
  if (mailsortresults) (*mailsortresults) (stream,ret,pgm->nmsgs);
  return ret;			/* return sort results */
}

/* Mail sort sortcache vector
 * Accepts: mail stream
 *	    sort program
 *	    sortcache vector
 *	    option flags
 * Returns: vector of sorted message sequences or NIL if error
 */

unsigned long *mail_sort_cache (MAILSTREAM *stream,SORTPGM *pgm,SORTCACHE **sc,
				long flags)
{
  unsigned long i,*ret;
				/* pass 3: sort messages */
  qsort ((void *) sc,pgm->nmsgs,sizeof (SORTCACHE *),mail_sort_compare);
				/* optional post sorting */
  if (pgm->postsort) (*pgm->postsort) ((void *) sc);
				/* pass 4: return results */
  ret = (unsigned long *) fs_get ((pgm->nmsgs+1) * sizeof (unsigned long));
  if (flags & SE_UID)		/* UID or msgno? */
    for (i = 0; i < pgm->nmsgs; i++) ret[i] = mail_uid (stream,sc[i]->num);
  else for (i = 0; i < pgm->nmsgs; i++) ret[i] = sc[i]->num;
  ret[pgm->nmsgs] = 0;		/* tie off message list */
  return ret;
}

/* Mail load sortcache
 * Accepts: mail stream, already searched
 *	    sort program
 * Returns: vector of sortcache pointers matching search
 */

static STRINGLIST maildateline = {{(unsigned char *) "date",4},NIL};
static STRINGLIST mailrnfromline = {{(unsigned char *) ">from",5},NIL};
static STRINGLIST mailfromline = {{(unsigned char *) "from",4},
				    &mailrnfromline};
static STRINGLIST mailtonline = {{(unsigned char *) "to",2},NIL};
static STRINGLIST mailccline = {{(unsigned char *) "cc",2},NIL};
static STRINGLIST mailsubline = {{(unsigned char *) "subject",7},NIL};

SORTCACHE **mail_sort_loadcache (MAILSTREAM *stream,SORTPGM *pgm)
{
  char *t,*v,*x,tmp[MAILTMPLEN];
  SORTPGM *pg;
  SORTCACHE *s,**sc;
  MESSAGECACHE *elt,telt;
  ENVELOPE *env;
  ADDRESS *adr = NIL;
  unsigned long i = (pgm->nmsgs) * sizeof (SORTCACHE *);
  sc = (SORTCACHE **) memset (fs_get ((size_t) i),0,(size_t) i);
				/* see what needs to be loaded */
  for (i = 1; !pgm->abort && (i <= stream->nmsgs); i++)
    if ((elt = mail_elt (stream,i))->searched) {
      sc[pgm->progress.cached++] =
	s = (SORTCACHE *) (*mailcache) (stream,i,CH_SORTCACHE);
      s->pgm = pgm;		/* note sort program */
      s->num = i;
				/* get envelope if cached */
      if (stream->scache) env = (i == stream->msgno) ? stream->env : NIL;
      else env = elt->private.msg.env;
      for (pg = pgm; pg; pg = pg->next) switch (pg->function) {
      case SORTARRIVAL:		/* sort by arrival date */
	if (!s->arrival) {
				/* internal date unknown but can get? */
	  if (!elt->day && !(stream->dtb->flags & DR_NOINTDATE)) {
	    sprintf (tmp,"%lu",i);
	    mail_fetch_fast (stream,tmp,NIL);
	  }
				/* wrong thing before 3-Jan-1970 */
	  s->arrival = elt->day ? mail_longdate (elt) : 1;
	}
	break;
      case SORTSIZE:		/* sort by message size */
	if (!s->size) {
	  if (!elt->rfc822_size) {
	    sprintf (tmp,"%lu",i);
	    mail_fetch_fast (stream,tmp,NIL);
	  }
	  s->size = elt->rfc822_size ? elt->rfc822_size : 1;
	}
	break;

      case SORTDATE:		/* sort by date */
	if (!s->date) {
	  if (env) t = env->date;
	  else if ((t = mail_fetch_header (stream,i,NIL,&maildateline,NIL,
					   FT_INTERNAL | FT_PEEK)) &&
		   (t = strchr (t,':')))
	    for (x = ++t; x = strpbrk (x,"\012\015"); x++)
	      switch (*(v = ((*x == '\015') && (x[1] == '\012')) ? x+2 : x+1)){
	      case ' ':		/* erase continuation newlines */
	      case '\t':
		memmove (x,v,strlen (v));
		break;
	      default:		/* tie off extraneous text */
		*x = x[1] = '\0';
	      }
				/* skip leading whitespace */
	  if (t) while ((*t == ' ') || (*t == '\t')) t++;
				/* parse date from Date: header */
	  if (!(t && mail_parse_date (&telt,t) && 
		(s->date = mail_longdate (&telt)))) {
				/* failed, use internal date */
	    if (!(s->date = s->arrival)) {
				/* internal date unknown but can get? */
	      if (!elt->day && !(stream->dtb->flags & DR_NOINTDATE)) {
		sprintf (tmp,"%lu",i);
		mail_fetch_fast (stream,tmp,NIL);
	      }
				/* wrong thing before 3-Jan-1970 */
	      s->date = (s->arrival = elt->day ? mail_longdate (elt) : 1);
	    }
	  }
	}
	break;

      case SORTFROM:		/* sort by first from */
	if (!s->from) {
	  if (env) s->from = env->from && env->from->mailbox ?
	    cpystr (env->from->mailbox) : NIL;
	  else if ((t = mail_fetch_header (stream,i,NIL,&mailfromline,NIL,
					   FT_INTERNAL | FT_PEEK)) &&
		   (t = strchr (t,':'))) {
	    for (x = ++t; x = strpbrk (x,"\012\015"); x++)
	      switch (*(v = ((*x == '\015') && (x[1] == '\012')) ? x+2 : x+1)){
	      case ' ':		/* erase continuation newlines */
	      case '\t':
		memmove (x,v,strlen (v));
		break;
	      case 'f':		/* continuation but with extra "From:" */
	      case 'F':
		if (v = strchr (v,':')) {
		  memmove (x,v+1,strlen (v+1));
		  break;
		}
	      default:		/* tie off extraneous text */
		*x = x[1] = '\0';
	      }
	    if (adr = rfc822_parse_address (&adr,adr,&t,BADHOST,0)) {
	      s->from = adr->mailbox;
	      adr->mailbox = NIL;
	      mail_free_address (&adr);
	    }
	  }
	  if (!s->from) s->from = cpystr ("");
	}
	break;

      case SORTTO:		/* sort by first to */
	if (!s->to) {
	  if (env) s->to = env->to && env->to->mailbox ?
	    cpystr (env->to->mailbox) : NIL;
	  else if ((t = mail_fetch_header (stream,i,NIL,&mailtonline,NIL,
					   FT_INTERNAL | FT_PEEK)) &&
		   (t = strchr (t,':'))) {
	    for (x = ++t; x = strpbrk (x,"\012\015"); x++)
	      switch (*(v = ((*x == '\015') && (x[1] == '\012')) ? x+2 : x+1)){
	      case ' ':		/* erase continuation newlines */
	      case '\t':
		memmove (x,v,strlen (v));
		break;
	      case 't':		/* continuation but with extra "To:" */
	      case 'T':
		if (v = strchr (v,':')) {
		  memmove (x,v+1,strlen (v+1));
		  break;
		}
	      default:		/* tie off extraneous text */
		*x = x[1] = '\0';
	      }
	    if (adr = rfc822_parse_address (&adr,adr,&t,BADHOST,0)) {
	      s->to = adr->mailbox;
	      adr->mailbox = NIL;
	      mail_free_address (&adr);
	    }
	  }
	  if (!s->to) s->to = cpystr ("");
	}
	break;

      case SORTCC:		/* sort by first cc */
	if (!s->cc) {
	  if (env) s->cc = env->cc && env->cc->mailbox ?
	    cpystr (env->cc->mailbox) : NIL;
	  else if ((t = mail_fetch_header (stream,i,NIL,&mailccline,NIL,
					   FT_INTERNAL | FT_PEEK)) &&
		   (t = strchr (t,':'))) {
	    for (x = ++t; x = strpbrk (x,"\012\015"); x++)
	      switch (*(v = ((*x == '\015') && (x[1] == '\012')) ? x+2 : x+1)){
	      case ' ':		/* erase continuation newlines */
	      case '\t':
		memmove (x,v,strlen (v));
		break;
	      case 't':		/* continuation but with extra "To:" */
	      case 'T':
		if (v = strchr (v,':')) {
		  memmove (x,v+1,strlen (v+1));
		  break;
		}
	      default:		/* tie off extraneous text */
		*x = x[1] = '\0';
	      }
	    if (adr = rfc822_parse_address (&adr,adr,&t,BADHOST,0)) {
	      s->cc = adr->mailbox;
	      adr->mailbox = NIL;
	      mail_free_address (&adr);
	    }
	  }
	  if (!s->cc) s->cc = cpystr ("");
	}
	break;

      case SORTSUBJECT:		/* sort by subject */
	if (!s->subject) {
				/* get subject from envelope if have one */
	  if (env) t = env->subject ? env->subject : "";
				/* otherwise snarf from header text */
	  else if ((t = mail_fetch_header (stream,i,NIL,&mailsubline,
					   NIL,FT_INTERNAL | FT_PEEK)) &&
		   (t = strchr (t,':')))
	    for (x = ++t; x = strpbrk (x,"\012\015"); x++)
	      switch (*(v = ((*x == '\015') && (x[1] == '\012')) ? x+2 : x+1)){
	      case ' ':		/* erase continuation newlines */
	      case '\t':
		memmove (x,v,strlen (v));
		break;
	      default:		/* tie off extraneous text */
		*x = x[1] = '\0';
	      }
	  else t = "";		/* empty subject */
				/* strip and cache subject */
	  s->refwd = mail_strip_subject (s->original_subject = cpystr (t),
					 &s->subject);
	}
	break;
      default:
	fatal ("Unknown sort function");
      }
    }
  return sc;
}

/* Strip subjects of extra spaces and leading and trailing cruft for sorting
 * Accepts: unstripped subject
 *	    pointer to return stripped subject, in cpystr form
 * Returns: T if subject had a re/fwd, NIL otherwise
 */

unsigned int mail_strip_subject (char *t,char **ret)
{
  SIZEDTEXT src,dst;
  unsigned long i,slen;
  char c,*s,*x;
  unsigned int refwd = NIL;
  if (src.size = strlen (t)) {	/* have non-empty subject? */
    src.data = (unsigned char *) t;
			/* Step 1 */
				/* make copy, convert MIME2 if needed */
    *ret = s = (utf8_mime2text (&src,&dst) && (src.data != dst.data)) ?
      (char *) dst.data : cpystr (t);
				/* convert spaces to tab, strip extra spaces */
    for (x = t = s, c = 'x'; *t; t++) {
      if (c != ' ') c = *x++ = ((*t == '\t') ? ' ' : *t);
      else if ((*t != '\t') && (*t != ' ')) c = *x++ = *t;
    }
    *x = '\0';			/* tie off string */
			/* Step 2 */
    for (slen = dst.size; s; slen = strlen (s))  {
      for (t = s + slen; t > s; ) switch (t[-1]) {
      case ' ': case '\t':	/* WSP */
	*--t = '\0';		/* just remove it */
	break;
      case ')':			/* possible "(fwd)" */
	if ((t >= (s + 5)) && (t[-5] == '(') &&
	    ((t[-4] == 'F') || (t[-4] == 'f')) &&
	    ((t[-3] == 'W') || (t[-3] == 'w')) &&
	    ((t[-2] == 'D') || (t[-2] == 'd'))) {
	  *(t -= 5) = '\0';	/* remove "(fwd)" */
	  refwd = T;		/* note a re/fwd */
	  break;
	}
      default:			/* not a subj-trailer */
	t = s;
	break;
      }
			/* Steps 3-5 */
      for (t = s; t; ) switch (*s) {
      case ' ': case '\t':	/* WSP */
	s = t = mail_strip_subject_wsp (s + 1);
	break;
      case 'r': case 'R':	/* possible "re" */
	if (((s[1] == 'E') || (s[1] == 'e')) &&
	    (t = mail_strip_subject_wsp (s + 2)) &&
	    (t = mail_strip_subject_blob (t)) && (*t == ':'))
	  s = ++t;		/* found "re" */
	else t = NIL;		/* found subj-middle */
	break;
      case 'f': case 'F':	/* possible "fw" or "fwd" */
	if (((s[1] == 'w') || (s[1] == 'W')) &&
	    (((s[2] == 'd') || (s[2] == 'D')) ?
	     (t = mail_strip_subject_wsp (s + 3)) :
	     (t = mail_strip_subject_wsp (s + 2))) &&
	    (t = mail_strip_subject_blob (t)) && (*t == ':'))
	  s = ++t;		/* found "re" */
	else t = NIL;		/* found subj-middle */
	break;
      case '[':			/* possible subj-blob */
	if ((t = mail_strip_subject_blob (s)) && *t) s = t;
	else t = NIL;		/* found subj-middle */
	break;
      default:
	t = NIL;		/* found subj-middle */
	break;
      }
			/* Step 6 */
				/* Netscape-style "[Fwd: ...]"? */
      if ((*s == '[') && ((s[1] == 'F') || (s[1] == 'f')) &&
	  ((s[2] == 'W') || (s[2] == 'w')) &&
	  ((s[3] == 'D') || (s[3] == 'd')) && (s[4] == ':') &&
	  (s[i = strlen (s) - 1] == ']')) {
	s[i] = '\0';		/* flush closing "]" */
	s += 5;			/* and leading "[Fwd:" */
	refwd = T;		/* definitely a re/fwd at this point */
      }
      else break;		/* don't need to loop back to step 2 */
    }
    if (s != (t = *ret)) {	/* removed leading text? */
      s = *ret = cpystr (s);	/* yes, make a fresh return copy */
      fs_give ((void **) &t);	/* flush old copy */
    }
  }
  else *ret = cpystr ("");	/* empty subject */
  return refwd;			/* return re/fwd state */
}

/* Strip subject wsp helper routine
 * Accepts: text
 * Returns: pointer to text after blob
 */

char *mail_strip_subject_wsp (char *s)
{
  while ((*s == ' ') || (*s == '\t')) s++;
  return s;
}


/* Strip subject blob helper routine
 * Accepts: text
 * Returns: pointer to text after any blob, NIL if blob-like but not blob
 */

char *mail_strip_subject_blob (char *s)
{
  if (*s != '[') return s;	/* not a blob, ignore */
				/* search for end of blob */
  while (*++s != ']') if ((*s == '[') || !*s) return NIL;
  return mail_strip_subject_wsp (s + 1);
}

/* Sort compare messages
 * Accept: first message sort cache element
 *	   second message sort cache element
 * Returns: -1 if a1 < a2, 0 if a1 == a2, 1 if a1 > a2
 */

int mail_sort_compare (const void *a1,const void *a2)
{
  int i = 0;
  SORTCACHE *s1 = *(SORTCACHE **) a1;
  SORTCACHE *s2 = *(SORTCACHE **) a2;
  SORTPGM *pgm = s1->pgm;
  if (!s1->sorted) {		/* this one sorted yet? */
    s1->sorted = T;
    pgm->progress.sorted++;	/* another sorted message */
  }
  if (!s2->sorted) {		/* this one sorted yet? */
    s2->sorted = T;
    pgm->progress.sorted++;	/* another sorted message */
  }
  do {
    switch (pgm->function) {	/* execute search program */
    case SORTDATE:		/* sort by date */
      i = compare_ulong (s1->date,s2->date);
      break;
    case SORTARRIVAL:		/* sort by arrival date */
      i = compare_ulong (s1->arrival,s2->arrival);
      break;
    case SORTSIZE:		/* sort by message size */
      i = compare_ulong (s1->size,s2->size);
      break;
    case SORTFROM:		/* sort by first from */
      i = compare_cstring (s1->from,s2->from);
      break;
    case SORTTO:		/* sort by first to */
      i = compare_cstring (s1->to,s2->to);
      break;
    case SORTCC:		/* sort by first cc */
      i = compare_cstring (s1->cc,s2->cc);
      break;
    case SORTSUBJECT:		/* sort by subject */
      i = compare_cstring (s1->subject,s2->subject);
      break;
    }
    if (pgm->reverse) i = -i;	/* flip results if necessary */
  }
  while (pgm = i ? NIL : pgm->next);
				/* return result, avoid 0 if at all possible */
  return i ? i : compare_ulong (s1->num,s2->num);
}

/* Return message date as an unsigned long seconds since time began
 * Accepts: message cache pointer
 * Returns: unsigned long of date
 *
 * This routine, like most UNIX systems, is clueless about leap seconds.
 * Thus, it treats 23:59:60 as equivalent to 00:00:00 the next day.
 *
 * This routine forces any early hours on 1-Jan-1970 in oriental timezones
 * to be 1-Jan-1970 00:00:00 UTC, so as to avoid negative longdates.
 */

unsigned long mail_longdate (MESSAGECACHE *elt)
{
  unsigned long m = elt->month ? elt->month : 1;
  unsigned long yr = elt->year + BASEYEAR;
				/* number of days since time began */
  unsigned long ret = (elt->day ? (elt->day - 1) : 0)
    + 30 * (m - 1) + ((m + (m > 8)) / 2)
#ifndef USEJULIANCALENDAR
#ifndef USEORTHODOXCALENDAR	/* Gregorian calendar */
    + ((yr / 400) - (BASEYEAR / 400)) - ((yr / 100) - (BASEYEAR / 100))
#ifdef Y4KBUGFIX
    - ((yr / 4000) - (BASEYEAR / 4000))
#endif
    - ((m < 3) ?
       !(yr % 4) && ((yr % 100) || (!(yr % 400)
#ifdef Y4KBUGFIX
				    && (yr % 4000)
#endif
				    )) : 2)
#else				/* Orthodox calendar */
    + ((2*(yr / 900)) - (2*(BASEYEAR / 900)))
    + (((yr % 900) >= 200) - ((BASEYEAR % 900) >= 200))
    + (((yr % 900) >= 600) - ((BASEYEAR % 900) >= 600))
    - ((yr / 100) - (BASEYEAR / 100))
    - ((m < 3) ?
       !(yr % 4) && ((yr % 100) || ((yr % 900) == 200) || ((yr % 900) == 600))
       : 2)
#endif
#endif
    + elt->year * 365 + (((unsigned long) (elt->year + (BASEYEAR % 4))) / 4);
  ret *= 24; ret += elt->hours;	/* date value in hours */
  ret *= 60; ret +=elt->minutes;/* date value in minutes */
  yr = (elt->zhours * 60) + elt->zminutes;
  if (elt->zoccident) ret += yr;/* occidental timezone, make UTC */
  else if (ret < yr) return 0;	/* still 31-Dec-1969 in UTC */
  else ret -= yr;		/* oriental timezone, make UTC */
  ret *= 60; ret += elt->seconds;
  return ret;
}

/* Mail thread messages
 * Accepts: mail stream
 *	    thread type
 *	    character set
 *	    search program
 *	    option flags
 * Returns: thread node tree or NIL if error
 */

THREADNODE *mail_thread (MAILSTREAM *stream,char *type,char *charset,
			 SEARCHPGM *spg,long flags)
{
  THREADNODE *ret = NIL;
  if (stream->dtb)		/* must have a live driver */
    ret = stream->dtb->thread ?	/* do driver's action if available */
      (*stream->dtb->thread) (stream,type,charset,spg,flags) :
	mail_thread_msgs (stream,type,charset,spg,flags,mail_sort_msgs);
				/* flush search/sort programs if requested */
  if (spg && (flags & SE_FREE)) mail_free_searchpgm (&spg);
  return ret;
}


/* Mail thread messages
 * Accepts: mail stream
 *	    thread type
 *	    character set
 *	    search program
 *	    option flags
 *	    sorter routine
 * Returns: thread node tree or NIL if error
 */

THREADNODE *mail_thread_msgs (MAILSTREAM *stream,char *type,char *charset,
			      SEARCHPGM *spg,long flags,sorter_t sorter)
{
  THREADER *t;
  for (t = &mailthreadlist; t; t = t->next)
    if (!compare_cstring (type,t->name)) {
      THREADNODE *ret = (*t->dispatch) (stream,charset,spg,flags,sorter);
      if (mailthreadresults) (*mailthreadresults) (stream,ret);
      return ret;
    }
  MM_LOG ("No such thread type",ERROR);
  return NIL;
}

/* Mail thread ordered subject
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    option flags
 *	    sorter routine
 * Returns: thread node tree
 */

THREADNODE *mail_thread_orderedsubject (MAILSTREAM *stream,char *charset,
					SEARCHPGM *spg,long flags,
					sorter_t sorter)
{
  THREADNODE *thr = NIL;
  THREADNODE *cur,*top,**tc;
  SORTPGM pgm,pgm2;
  SORTCACHE *s;
  unsigned long i,j,*lst,*ls;
				/* sort by subject+date */
  memset (&pgm,0,sizeof (SORTPGM));
  memset (&pgm2,0,sizeof (SORTPGM));
  pgm.function = SORTSUBJECT;
  pgm.next = &pgm2;
  pgm2.function = SORTDATE;
  if (lst = (*sorter) (stream,charset,spg,&pgm,flags & ~(SE_FREE | SE_UID))){
    if (*(ls = lst)) {		/* create thread */
				/* note first subject */
      cur = top = thr = mail_newthreadnode
	((SORTCACHE *) (*mailcache) (stream,*ls++,CH_SORTCACHE));
				/* note its number */
      cur->num = (flags & SE_UID) ? mail_uid (stream,*lst) : *lst;
      i = 1;			/* number of threads */
      while (*ls) {		/* build tree */
				/* subjects match? */
	s = (SORTCACHE *) (*mailcache) (stream,*ls++,CH_SORTCACHE);
	if (compare_cstring (top->sc->subject,s->subject)) {
	  i++;			/* have a new thread */
	  top = top->branch = cur = mail_newthreadnode (s);
	}
				/* start a child of the top */
	else if (cur == top) cur = cur->next = mail_newthreadnode (s);
				/* sibling of child */
	else cur = cur->branch = mail_newthreadnode (s);
				/* set to msgno or UID as needed */
	cur->num = (flags & SE_UID) ? mail_uid (stream,s->num) : s->num;
      }
				/* make threadnode cache */
      tc = (THREADNODE **) fs_get (i * sizeof (THREADNODE *));
				/* load threadnode cache */
      for (j = 0, cur = thr; cur; cur = cur->branch) tc[j++] = cur;
      if (i != j) fatal ("Threadnode cache confusion");
      qsort ((void *) tc,i,sizeof (THREADNODE *),mail_thread_compare_date);
      for (j = 0, --i; j < i; j++) tc[j]->branch = tc[j+1];
      tc[j]->branch = NIL;	/* end of root */
      thr = tc[0];		/* head of data */
      fs_give ((void **) &tc);
    }
    fs_give ((void **) &lst);
  }
  return thr;
}

/* Mail thread references
 * Accepts: mail stream
 *	    character set
 *	    search program
 *	    option flags
 *	    sorter routine
 * Returns: thread node tree
 */

#define REFHASHSIZE 1009	/* arbitrary prime for hash table size */

/*  Reference threading container, as described in Jamie Zawinski's web page
 * (http://www.jwz.org/doc/threading.html) for this algorithm.  These are
 * stored as extended data in the hash table (called "id_table" in JWZ's
 * document) and are maintained by the hash table routines.  The hash table
 * routines implement extended data as additional void* words at the end of
 * each bucket, hence these strange macros instead of a struct which would
 * have been more straightforward.
 */

#define THREADLINKS 3		/* number of thread links */

#define CACHE(data) ((SORTCACHE *) (data)[0])
#define PARENT(data) ((container_t) (data)[1])
#define SETPARENT(data,value) ((container_t) (data[1] = value))
#define SIBLING(data) ((container_t) (data)[2])
#define SETSIBLING(data,value) ((container_t) (data[2] = value))
#define CHILD(data) ((container_t) (data)[3])
#define SETCHILD(data,value) ((container_t) (data[3] = value))

THREADNODE *mail_thread_references (MAILSTREAM *stream,char *charset,
				    SEARCHPGM *spg,long flags,sorter_t sorter)
{
  MESSAGECACHE *elt,telt;
  ENVELOPE *env;
  SORTCACHE *s;
  STRINGLIST *st;
  HASHENT *he;
  THREADNODE **tc,*cur,*lst,*nxt,*sis,*msg;
  container_t con,nxc,prc,sib;
  void **sub;
  char *t,tmp[MAILTMPLEN];
  unsigned long j,nmsgs;
  unsigned long i = stream->nmsgs * sizeof (SORTCACHE *);
  SORTCACHE **sc = (SORTCACHE **) memset (fs_get ((size_t) i),0,(size_t) i);
  HASHTAB *ht = hash_create (REFHASHSIZE);
  THREADNODE *root = NIL;
  if (spg) {			/* only if a search needs to be done */
    int silent = stream->silent;
    stream->silent = T;		/* don't pass up mm_searched() events */
				/* search for messages */
    mail_search_full (stream,charset,spg,NIL);
    stream->silent = silent;	/* restore silence state */
  }

				/* create SORTCACHE vector of requested msgs */
  for (i = 1, nmsgs = 0; i <= stream->nmsgs; ++i)
    if (mail_elt (stream,i)->searched)
      (sc[nmsgs++] = (SORTCACHE *)(*mailcache)(stream,i,CH_SORTCACHE))->num =i;
	/* separate pass so can do overview fetch lookahead */
  for (i = 0; i < nmsgs; ++i) {	/* for each requested message */
				/* is anything missing in its SORTCACHE? */
    if (!((s = sc[i])->date && s->subject && s->message_id && s->references)) {
				/* driver has an overview mechanism? */
      if (stream->dtb && stream->dtb->overview) {
				/* yes, find following unloaded entries */
	for (j = i + 1; (j < nmsgs) && !sc[j]->references; ++j);
	sprintf (tmp,"%lu",mail_uid (stream,s->num));
	if (i != --j)		/* end of range different? */
	  sprintf (tmp + strlen (tmp),":%lu",mail_uid (stream,sc[j]->num));
				/* load via overview mechanism */
	mail_fetch_overview (stream,tmp,mail_thread_loadcache);
      }
				/* still missing data? */
      if (!s->date || !s->subject || !s->message_id || !s->references) {
				/* try to load data from envelope */
	if (env = mail_fetch_structure (stream,s->num,NIL,NIL)) {
	  if (!s->date && env->date && mail_parse_date (&telt,env->date))
	    s->date = mail_longdate (&telt);
	  if (!s->subject && env->subject)
	    s->refwd =
	      mail_strip_subject (s->original_subject = cpystr (env->subject),
				  &s->subject);
	  if (!s->message_id && env->message_id && *env->message_id)
	    s->message_id = mail_thread_parse_msgid (env->message_id,NIL);
	  if (!s->references &&	/* use References: or In-Reply-To: */
	      !(s->references = 
		mail_thread_parse_references (env->references,T)))
	    s->references = mail_thread_parse_references(env->in_reply_to,NIL);
	}
				/* last resort */
	if (!s->date && !(s->date = s->arrival)) {
				/* internal date unknown but can get? */
	  if (!(elt = mail_elt (stream,s->num))->day &&
	      !(stream->dtb->flags & DR_NOINTDATE)) {
	    sprintf (tmp,"%lu",s->num);
	    mail_fetch_fast (stream,tmp,NIL);
	  }
				/* wrong thing before 3-Jan-1970 */
	  s->date = (s->arrival = elt->day ? mail_longdate (elt) : 1);
	}
	if (!s->subject) s->subject = cpystr ("");
	if (!s->references) s->references = mail_newstringlist ();
      }
    }

			/* Step 1 (preliminary) */
				/* generate unique string */
    sprintf (tmp,"%s.%lx.%lx@%s",stream->mailbox,stream->uid_validity,
	     mail_uid (stream,s->num),mylocalhost ());
				/* flush old unique string if not message-id */
    if (s->unique && (s->unique != s->message_id))
      fs_give ((void **) &s->unique);
    s->unique = s->message_id ?	/* don't permit Message ID duplicates */
      (hash_lookup (ht,s->message_id) ? cpystr (tmp) : s->message_id) :
	(s->message_id = cpystr (tmp));
				/* add unique string to hash table */
    hash_add (ht,s->unique,s,THREADLINKS);
  }
			/* Step 1 */
  for (i = 0; i < nmsgs; ++i) {	/* for each message in sortcache */
			/* Step 1A */
    if ((st = (s = sc[i])->references) && st->text.data)
      for (con = hash_lookup_and_add (ht,(char *) st->text.data,NIL,
				      THREADLINKS); st = st->next; con = nxc) {
	nxc = hash_lookup_and_add (ht,(char *) st->text.data,NIL,THREADLINKS);
				/* only if no parent & won't introduce loop */
	if (!PARENT (nxc) && !mail_thread_check_child (con,nxc)) {
	  SETPARENT (nxc,con);	/* establish parent/child link */
				/* other children become sibling of this one */
	  SETSIBLING (nxc,CHILD (con));
	  SETCHILD (con,nxc);	/* set as child of parent */
	}
      }
    else con = NIL;		/* else message has no ancestors */
			/* Step 1B */
    if ((prc = PARENT ((nxc = hash_lookup (ht,s->unique)))) &&
	(prc != con)) {		/* break links if have a different parent */
      SETPARENT (nxc,NIL);	/* easy if direct child */
      if (nxc == CHILD (prc)) SETCHILD (prc,SIBLING (nxc));
      else {			/* otherwise hunt through sisters */
	for (sib = CHILD (prc); nxc != SIBLING (sib); sib = SIBLING (sib));
	SETSIBLING (sib,SIBLING (nxc));
      }
      SETSIBLING (nxc,NIL);	/* no more little sisters either */
      prc = NIL;		/* no more parent set */
    }
				/* need to set parent, and parent is good? */
    if (!prc && !mail_thread_check_child (con,nxc)) {
      SETPARENT (nxc,con);	/* establish parent/child link */
      if (con) {		/* if non-root parent, set parent's child */
	if (CHILD (con)) {	/* have a child already */
				/* find youngest daughter */
	  for (con = CHILD (con); SIBLING (con); con = SIBLING (con));
	  SETSIBLING (con,nxc);	/* add new baby sister */
	}
	else SETCHILD (con,nxc);/* set as only child */
      }
    }
  }
  fs_give ((void **) &sc);	/* finished with sortcache vector */

			/* Step 2 */
				/* search hash table for parentless messages */
  for (i = 0, prc = con = NIL; i < ht->size; i++)
    for (he = ht->table[i]; he; he = he->next)
      if (!PARENT ((nxc = he->data))) {
				/* sibling of previous parentless message */
	if (con) con = SETSIBLING (con,nxc);
	else prc = con = nxc;	/* first parentless message */
      }
  /*  Once the dummy containers are pruned, we no longer need the parent
   * information, so we can convert the containers to THREADNODEs.  Since
   * we don't need the id_table any more either, we can reset the hash table
   * and reuse it as a subject_table.  Resetting the hash table will also
   * destroy the containers.
   */
			/* Step 3 */
				/* prune dummies, convert to threadnode */
  root = mail_thread_c2node (stream,mail_thread_prune_dummy (prc,NIL),flags);
			/* Step 4 */
				/* make buffer for sorting */
  tc = (THREADNODE **) fs_get (nmsgs * sizeof (THREADNODE *));
				/* load threadcache and count nodes to sort */
  for (i = 0, cur = root; cur ; cur = cur->branch) tc[i++] = cur;
  if (i > 1) {			/* only if need to sort */
    qsort ((void *) tc,i,sizeof (THREADNODE *),mail_thread_compare_date);
				/* relink siblings */
    for (j = 0, --i; j < i; j++) tc[j]->branch = tc[j+1];
    tc[j]->branch = NIL;	/* end of root */
    root = tc[0];		/* establish new root */
  }
			/* Step 5A */
  hash_reset (ht);		/* discard containers, reset ht */
			/* Step 5B */
  for (cur = root; cur; cur = cur->branch)
    if ((t = (nxt = (cur->sc ? cur : cur->next))->sc->subject) && *t) {
				/* add new subject to hash table */
      if (!(sub = hash_lookup (ht,t))) hash_add (ht,t,cur,0);
				/* if one in table not dummy and */
      else if ((s = (lst = (THREADNODE *) sub[0])->sc) &&
				/* current dummy, or not re/fwd and table is */
	       (!cur->sc || (!nxt->sc->refwd && s->refwd)))
	sub[0] = (void *) cur;	/* replace with this message */
    }

			/* Step 5C */
  for (cur = root, sis = NIL; cur; cur = msg) {
				/* do nothing if current message or no sub */
    if (!(t = (cur->sc ? cur : cur->next)->sc->subject) || !*t ||
	((lst = (THREADNODE *) (sub = hash_lookup (ht,t))[0]) == cur))
      msg = (sis = cur)->branch;
    else if (!lst->sc) {	/* is message in the table a dummy? */
				/* find youngest daughter of msg in table */
      for (msg = lst->next; msg->branch; msg = msg->branch);
      if (!cur->sc) {		/* current message a dummy? */
	msg->branch = cur->next;/* current's daughter now dummy's youngest */
	msg = cur->branch;	/* continue scan at younger sister */
				/* now delete this node */
	cur->branch = cur->next = NIL;
	mail_free_threadnode (&cur);
      }
      else {			/* current message not a dummy */
	msg->branch = cur;	/* append as youngest daughter */
	msg = cur->branch;	/* continue scan at younger sister */
	cur->branch = NIL;	/* lose our younger sisters */
      }
    }
    else {			/* no dummies, is current re/fwd, table not? */
      if (cur->sc->refwd && !lst->sc->refwd) {
	if (lst->next) {	/* find youngest daughter of msg in table */
	  for (msg = lst->next; msg->branch; msg = msg->branch);
	  msg->branch = cur;	/* append as youngest daughter */
	}
	else lst->next = cur;	/* no children, so make the eldest daughter */
      }

      else {			/* no re/fwd, create a new dummy */
	msg = mail_newthreadnode (NIL);
	if (lst == root) {	/* msg in table is root? */
	  root = lst->branch;	/* younger sister becomes new root */
				/* no longer older sister either */
	  if (lst == sis) sis = NIL;
	}
	else {			/* find older sister of msg in table */
	  for (nxt = root; lst != nxt->branch; nxt = nxt->branch);
				/* remove from older sister */
	  nxt->branch = lst->branch;
	}
	msg->next = lst;	/* msg in table becomes child */
	lst->branch = cur;	/* current now little sister of msg in table */
	if (sis) {		/* have an elder sister? */
	  if (sis == lst)	/* rescan if lost her */
	    for (sis = root; cur != sis->branch; sis = sis->branch);
	  sis->branch = msg;	/* make dummy younger sister of big sister */
	}
	else root = msg;	/* otherwise this is the new root */
	sub[0] = sis = msg;	/* set new msg in table and new big sister */
      }
      msg = cur->branch;	/* continue scan at younger sister */
      cur->branch = NIL;	/* lose our younger sisters */
    }
    if (sis) sis->branch = msg;	/* older sister gets this as younger sister */
    else root = msg;		/* otherwise this is the new root */
  }
  hash_destroy (&ht);		/* finished with hash table */
			/* Step 6 */
				/* sort threads */
  root = mail_thread_sort (root,tc);
  fs_give ((void **) &tc);	/* finished with sort buffer */
  return root;			/* return sorted list */
}

/* Fetch overview callback to load sortcache for threading
 * Accepts: MAIL stream
 *	    UID of this message
 *	    overview of this message
 *	    msgno of this message
 */

void mail_thread_loadcache (MAILSTREAM *stream,unsigned long uid,OVERVIEW *ov,
			    unsigned long msgno)
{
  if (msgno && ov) {		/* just in case */
    MESSAGECACHE telt;
    SORTCACHE *s = (SORTCACHE *) (*mailcache) (stream,msgno,CH_SORTCACHE);
    if (!s->subject && ov->subject)
      s->refwd = mail_strip_subject (s->original_subject = cpystr(ov->subject),
				     &s->subject);
    if (!s->from && ov->from && ov->from->mailbox)
      s->from = cpystr (ov->from->mailbox);
    if (!s->date && ov->date && mail_parse_date (&telt,ov->date))
      s->date = mail_longdate (&telt);
    if (!s->message_id && ov->message_id)
      s->message_id = mail_thread_parse_msgid (ov->message_id,NIL);
    if (!s->references &&
	!(s->references = mail_thread_parse_references (ov->references,T)))
				/* don't do In-Reply-To with NNTP mailboxes */
      s->references = mail_newstringlist ();
    if (!s->size && ov->optional.octets) s->size = ov->optional.octets;
  }
}

/* Thread parse Message ID
 * Accepts: pointer to purported Message ID
 *	    pointer to return pointer
 * Returns: Message ID or NIL, return pointer updated
 */

char *mail_thread_parse_msgid (char *s,char **ss)
{
  char *ret = NIL;
  char *t = NIL;
  ADDRESS *adr;
  if (s) {			/* only for non-NIL strings */
    rfc822_skipws (&s);		/* skip whitespace */
				/* ignore phrases */
    if (((*s == '<') || (s = rfc822_parse_phrase (s))) &&
	(adr = rfc822_parse_routeaddr (s,&t,BADHOST))) {
				/* make return msgid */
      if (adr->mailbox && adr->host)
	sprintf (ret = (char *) fs_get (strlen (adr->mailbox) +
					strlen (adr->host) + 2),"%s@%s",
		 adr->mailbox,adr->host);
      mail_free_address (&adr);	/* don't need temporary address */
    }
  }
  if (ss) *ss = t;		/* update return pointer */
  return ret;
}


/* Thread parse references
 * Accepts: pointer to purported references
 *	    parse multiple references flag
 * Returns: references or NIL
 */

STRINGLIST *mail_thread_parse_references (char *s,long flag)
{
  char *t;
  STRINGLIST *ret = NIL;
  STRINGLIST *cur;
				/* found first reference? */
  if (t = mail_thread_parse_msgid (s,&s)) {
    (ret = mail_newstringlist ())->text.data = (unsigned char *) t;
				/* parse subsequent references */
    if (flag) for (cur = ret; t = mail_thread_parse_msgid (s,&s);
		   (cur = cur->next = mail_newstringlist ())->text.data =
		   (unsigned char *) t);
  }
  return ret;
}

/* Prune dummy messages
 * Accepts: candidate container to prune
 *	    older sibling of container, if any
 * Returns: container in this position, possibly pruned
 * All children and younger siblings are also pruned
 */

container_t mail_thread_prune_dummy (container_t msg,container_t ane)
{
				/* prune container and children */
  container_t ret = msg ? mail_thread_prune_dummy_work (msg,ane) : NIL;
				/* prune all younger sisters */
  if (ret) for (ane = ret; ane && (msg = SIBLING (ane)); ane = msg)
    msg = mail_thread_prune_dummy_work (msg,ane);
  return ret;
}


/* Prune dummy messages worker routine
 * Accepts: candidate container to prune
 *	    older sibling of container, if any
 * Returns: container in this position, possibly pruned
 * All children are also pruned
 */

container_t mail_thread_prune_dummy_work (container_t msg,container_t ane)
{
  container_t cur;
				/* get children, if any */
  container_t nxt = mail_thread_prune_dummy (CHILD (msg),NIL);
				/* just update children if container has msg */
  if (CACHE (msg)) SETCHILD (msg,nxt);
  else if (!nxt) {		/* delete dummy with no children */
    nxt = SIBLING (msg);	/* get younger sister */
    if (ane) SETSIBLING (ane,nxt);
				/* prune younger sister if exists */
    msg = nxt ? mail_thread_prune_dummy_work (nxt,ane) : NIL;
  }
				/* not if parent root & multiple children */
  else if ((cur = PARENT (msg)) || !SIBLING (nxt)) {
				/* OK to promote, try younger sister of aunt */
    if (ane) SETSIBLING (ane,nxt);
				/* otherwise promote to child of grandmother */
    else if (cur) SETCHILD (cur,nxt);
    SETPARENT (nxt,cur);	/* set parent as well */
				/* look for end of siblings in new container */
    for (cur = nxt; SIBLING (cur); cur = SIBLING (cur));
				/* reattach deleted container's siblings */
    SETSIBLING (cur,SIBLING (msg));
				/* prune and return new container */
    msg = mail_thread_prune_dummy_work (nxt,ane);
  }
  else SETCHILD (msg,nxt);	/* in case child pruned */
  return msg;			/* return this message */
}

/* Test that purported mother is not a child of purported daughter
 * Accepts: mother
 *	    purported daugher
 * Returns: T if circular parentage exists, else NIL
 */

long mail_thread_check_child (container_t mother,container_t daughter)
{
  if (mother) {			/* only if mother non-NIL */
    if (mother == daughter) return T;
    for (daughter = CHILD (daughter); daughter; daughter = SIBLING (daughter))
      if (mail_thread_check_child (mother,daughter)) return T;
  }
  return NIL;
}


/* Generate threadnodes from containers
 * Accepts: Mail stream
 *	    container
 *	    flags
 * Return: threadnode list
 */

THREADNODE *mail_thread_c2node (MAILSTREAM *stream,container_t con,long flags)
{
  THREADNODE *ret,*cur;
  SORTCACHE *s;
  container_t nxt;
				/* for each container */
  for (ret = cur = NIL; con; con = SIBLING (con)) {
    s = CACHE (con);		/* yes, get its sortcache */
				/* create node for it */
    if (ret) cur = cur->branch = mail_newthreadnode (s);
    else ret = cur = mail_newthreadnode (s);
				/* attach sequence or UID for non-dummy */
    if (s) cur->num = (flags & SE_UID) ? mail_uid (stream,s->num) : s->num;
				/* attach the children */
    if (nxt = CHILD (con)) cur->next = mail_thread_c2node (stream,nxt,flags);
  }
  return ret;
}

/* Sort thread tree by date
 * Accepts: thread tree to sort
 *	    qsort vector to sort
 * Returns: sorted thread tree
 */

THREADNODE *mail_thread_sort (THREADNODE *thr,THREADNODE **tc)
{
  unsigned long i,j;
  THREADNODE *cur;
				/* sort children of each thread */
  for (cur = thr; cur; cur = cur->branch)
    if (cur->next) cur->next = mail_thread_sort (cur->next,tc);
  /* Must do this in a separate pass since recursive call will clobber tc */
				/* load threadcache and count nodes to sort */
  for (i = 0, cur = thr; cur; cur = cur->branch) tc[i++] = cur;
  if (i > 1) {			/* only if need to sort */
    qsort ((void *) tc,i,sizeof (THREADNODE *),mail_thread_compare_date);
				/* relink root siblings */
    for (j = 0, --i; j < i; j++) tc[j]->branch = tc[j+1];
    tc[j]->branch = NIL;	/* end of root */
  }
  return i ? tc[0] : NIL;	/* return new head of list */
}


/* Thread compare date
 * Accept: first message sort cache element
 *	   second message sort cache element
 * Returns: -1 if a1 < a2, 1 if a1 > a2
 *
 * This assumes that a sort cache element is either a message (with a
 * sortcache entry) or a dummy with a message (with sortcache entry) child.
 * This is true of both the ORDEREDSUBJECT (no dummies) and REFERENCES
 * (dummies only at top-level, and with non-dummy children).
 *
 * If a new algorithm allows a dummy parent to have a dummy child, this
 * routine must be changed if it is to be used by that algorithm.
 *
 * Messages with bogus dates are always sorted at the top.
 */

int mail_thread_compare_date (const void *a1,const void *a2)
{
  THREADNODE *t1 = *(THREADNODE **) a1;
  THREADNODE *t2 = *(THREADNODE **) a2;
  return compare_ulong ((t1->sc ? t1->sc : t1->next->sc)->date,
			(t2->sc ? t2->sc : t2->next->sc)->date);
}

/* Mail parse sequence
 * Accepts: mail stream
 *	    sequence to parse
 * Returns: T if parse successful, else NIL
 */

long mail_sequence (MAILSTREAM *stream,unsigned char *sequence)
{
  unsigned long i,j,x;
  for (i = 1; i <= stream->nmsgs; i++) mail_elt (stream,i)->sequence = NIL;
  while (sequence && *sequence){/* while there is something to parse */
    if (*sequence == '*') {	/* maximum message */
      if (stream->nmsgs) i = stream->nmsgs;
      else {
	MM_LOG ("No messages, so no maximum message number",ERROR);
	return NIL;
      }
      sequence++;		/* skip past * */
    }
				/* parse and validate message number */
    else if (!isdigit (*sequence)) {
      MM_LOG ("Syntax error in sequence",ERROR);
      return NIL;
    }
    else if (!(i = strtoul (sequence,(char **) &sequence,10)) ||
	     (i > stream->nmsgs)) {
      MM_LOG ("Sequence out of range",ERROR);
      return NIL;
    }
    switch (*sequence) {	/* see what the delimiter is */
    case ':':			/* sequence range */
      if (*++sequence == '*') {	/* maximum message */
	if (stream->nmsgs) j = stream->nmsgs;
	else {
	  MM_LOG ("No messages, so no maximum message number",ERROR);
	  return NIL;
	}
	sequence++;		/* skip past * */
      }
				/* parse end of range */
      else if (!(j = strtoul (sequence,(char **) &sequence,10)) ||
	       (j > stream->nmsgs)) {
	MM_LOG ("Sequence range invalid",ERROR);
	return NIL;
      }
      if (*sequence && *sequence++ != ',') {
	MM_LOG ("Sequence range syntax error",ERROR);
	return NIL;
      }
      if (i > j) {		/* swap the range if backwards */
	x = i; i = j; j = x;
      }
				/* mark each item in the sequence */
      while (i <= j) mail_elt (stream,j--)->sequence = T;
      break;
    case ',':			/* single message */
      ++sequence;		/* skip the delimiter, fall into end case */
    case '\0':			/* end of sequence, mark this message */
      mail_elt (stream,i)->sequence = T;
      break;
    default:			/* anything else is a syntax error! */
      MM_LOG ("Sequence syntax error",ERROR);
      return NIL;
    }
  }
  return T;			/* successfully parsed sequence */
}

/* Parse flag list
 * Accepts: MAIL stream
 *	    flag list as a character string
 *	    pointer to user flags to return
 * Returns: system flags
 */

long mail_parse_flags (MAILSTREAM *stream,char *flag,unsigned long *uf)
{
  char *t,*n,*s,tmp[MAILTMPLEN],flg[MAILTMPLEN];
  short f = 0;
  long i,j;
  *uf = 0;			/* initially no user flags */
  if (flag && *flag) {		/* no-op if no flag string */
				/* check if a list and make sure valid */
    if (((i = (*flag == '(')) ^ (flag[strlen (flag)-1] == ')')) ||
	(strlen (flag) >= MAILTMPLEN)) {
      MM_LOG ("Bad flag list",ERROR);
      return NIL;
    }
				/* copy the flag string w/o list construct */
    strncpy (n = tmp,flag+i,(j = strlen (flag) - (2*i)));
    tmp[j] = '\0';
    while ((t = n) && *t) {	/* parse the flags */
      i = 0;			/* flag not known yet */
				/* find end of flag */
      if (n = strchr (t,' ')) *n++ = '\0';
      ucase (strcpy (flg,t));
      if (flg[0] == '\\') {	/* system flag? */
	switch (flg[1]) {	/* dispatch based on first character */
	case 'S':		/* possible \Seen flag */
	  if (flg[2] == 'E' && flg[3] == 'E' && flg[4] == 'N' && !flg[5])
	    i = fSEEN;
	  break;
	case 'D':		/* possible \Deleted or \Draft flag */
	  if (flg[2] == 'E' && flg[3] == 'L' && flg[4] == 'E' &&
	      flg[5] == 'T' && flg[6] == 'E' && flg[7] == 'D' && !flg[8])
	    i = fDELETED;
	  else if (flg[2] == 'R' && flg[3] == 'A' && flg[4] == 'F' &&
		   flg[5] == 'T' && !flg[6]) i = fDRAFT;
	  break;
	case 'F':		/* possible \Flagged flag */
	  if (flg[2] == 'L' && flg[3] == 'A' && flg[4] == 'G' &&
	      flg[5] == 'G' && flg[6] == 'E' && flg[7] == 'D' && !flg[8])
	    i = fFLAGGED;
	  break;
	case 'A':		/* possible \Answered flag */
	  if (flg[2] == 'N' && flg[3] == 'S' && flg[4] == 'W' &&
	      flg[5] == 'E' && flg[6] == 'R' && flg[7] == 'E' &&
	      flg[8] == 'D' && !flg[9]) i = fANSWERED;
	  break;
	default:		/* unknown */
	  break;
	}
	if (i) f |= i;		/* add flag to flags list */
      }

				/* user flag, search through table */
      else for (j = 0; !i && j < NUSERFLAGS && (s =stream->user_flags[j]); ++j)
	if (!compare_cstring (t,s)) *uf |= i = 1 << j;
      if (!i) {			/* didn't find a matching flag? */
	if (*t == '\\') {
	  sprintf (flg,"Unsupported system flag: %.80s",t);
	  MM_LOG (flg,WARN);
	}
				/* can we create it? */
	else if (stream->kwd_create && (j < NUSERFLAGS) &&
	    (strlen (t) <= MAXUSERFLAG)) {
	  for (s = t; t && *s; s++) switch (*s) {
	  default:		/* all other characters */
				/* SPACE, CTL, or not CHAR */
	    if ((*s > ' ') && (*s < 0x7f)) break;
	  case '*': case '%':	/* list_wildcards */
	  case '"': case '\\':	/* quoted-specials */
				/* atom_specials */
	  case '(': case ')': case '{':
	    sprintf (flg,"Invalid flag: %.80s",t);
	    MM_LOG (flg,WARN);
	    t = NIL;
	  }
	  if (t) {		/* only if valid */
	    *uf |= 1 << j;	/* set the bit */
	    stream->user_flags[j] = cpystr (t);
				/* if out of user flags */
	    if (j == NUSERFLAGS - 1) stream->kwd_create = NIL;
	  }
	}
	else {
	  sprintf (flg,"Unknown flag: %.80s",t);
	  MM_LOG (flg,WARN);
	}
      }
    }
  }
  return f;
}

/* Mail check network stream for usability with new name
 * Accepts: MAIL stream
 *	    candidate new name
 * Returns: T if stream can be used, NIL otherwise
 */

long mail_usable_network_stream (MAILSTREAM *stream,char *name)
{
  NETMBX smb,nmb,omb;
  return (stream && stream->dtb && !(stream->dtb->flags & DR_LOCAL) &&
	  mail_valid_net_parse (name,&nmb) &&
	  mail_valid_net_parse (stream->mailbox,&smb) &&
	  mail_valid_net_parse (stream->original_mailbox,&omb) &&
	  ((!compare_cstring (smb.host,
			      trustdns ? tcp_canonical (nmb.host) : nmb.host)&&
	    !strcmp (smb.service,nmb.service) &&
	    (!nmb.port || (smb.port == nmb.port)) &&
	    (nmb.anoflag == stream->anonymous) &&
	    (!nmb.user[0] || !strcmp (smb.user,nmb.user))) ||
	   (!compare_cstring (omb.host,nmb.host) &&
	    !strcmp (omb.service,nmb.service) &&
	    (!nmb.port || (omb.port == nmb.port)) &&
	    (nmb.anoflag == stream->anonymous) &&
	    (!nmb.user[0] || !strcmp (omb.user,nmb.user))))) ? LONGT : NIL;
}

/* Mail data structure instantiation routines */


/* Mail instantiate cache elt
 * Accepts: initial message number
 * Returns: new cache elt
 */

MESSAGECACHE *mail_new_cache_elt (unsigned long msgno)
{
  MESSAGECACHE *elt = (MESSAGECACHE *) memset (fs_get (sizeof (MESSAGECACHE)),
					       0,sizeof (MESSAGECACHE));
  elt->lockcount = 1;		/* initially only cache references it */
  elt->msgno = msgno;		/* message number */
  return elt;
}


/* Mail instantiate envelope
 * Returns: new envelope
 */

ENVELOPE *mail_newenvelope (void)
{
  return (ENVELOPE *) memset (fs_get (sizeof (ENVELOPE)),0,sizeof (ENVELOPE));
}


/* Mail instantiate address
 * Returns: new address
 */

ADDRESS *mail_newaddr (void)
{
  return (ADDRESS *) memset (fs_get (sizeof (ADDRESS)),0,sizeof (ADDRESS));
}

/* Mail instantiate body
 * Returns: new body
 */

BODY *mail_newbody (void)
{
  return mail_initbody ((BODY *) fs_get (sizeof (BODY)));
}


/* Mail initialize body
 * Accepts: body
 * Returns: body
 */

BODY *mail_initbody (BODY *body)
{
  memset ((void *) body,0,sizeof (BODY));
  body->type = TYPETEXT;	/* content type */
  body->encoding = ENC7BIT;	/* content encoding */
  return body;
}


/* Mail instantiate body parameter
 * Returns: new body part
 */

PARAMETER *mail_newbody_parameter (void)
{
  return (PARAMETER *) memset (fs_get (sizeof(PARAMETER)),0,sizeof(PARAMETER));
}


/* Mail instantiate body part
 * Returns: new body part
 */

PART *mail_newbody_part (void)
{
  PART *part = (PART *) memset (fs_get (sizeof (PART)),0,sizeof (PART));
  mail_initbody (&part->body);	/* initialize the body */
  return part;
}


/* Mail instantiate body message part
 * Returns: new body message part
 */

MESSAGE *mail_newmsg (void)
{
  return (MESSAGE *) memset (fs_get (sizeof (MESSAGE)),0,sizeof (MESSAGE));
}

/* Mail instantiate string list
 * Returns: new string list
 */

STRINGLIST *mail_newstringlist (void)
{
  return (STRINGLIST *) memset (fs_get (sizeof (STRINGLIST)),0,
				sizeof (STRINGLIST));
}


/* Mail instantiate new search program
 * Returns: new search program
 */

SEARCHPGM *mail_newsearchpgm (void)
{
  return (SEARCHPGM *) memset (fs_get (sizeof(SEARCHPGM)),0,sizeof(SEARCHPGM));
}


/* Mail instantiate new search program
 * Accepts: header line name   
 * Returns: new search program
 */

SEARCHHEADER *mail_newsearchheader (char *line,char *text)
{
  SEARCHHEADER *hdr = (SEARCHHEADER *) memset (fs_get (sizeof (SEARCHHEADER)),
					       0,sizeof (SEARCHHEADER));
  hdr->line.size = strlen ((char *) (hdr->line.data =
				     (unsigned char *) cpystr (line)));
  hdr->text.size = strlen ((char *) (hdr->text.data =
				     (unsigned char *) cpystr (text)));
  return hdr;
}


/* Mail instantiate new search set
 * Returns: new search set
 */

SEARCHSET *mail_newsearchset (void)
{
  return (SEARCHSET *) memset (fs_get (sizeof(SEARCHSET)),0,sizeof(SEARCHSET));
}


/* Mail instantiate new search or
 * Returns: new search or
 */

SEARCHOR *mail_newsearchor (void)
{
  SEARCHOR *or = (SEARCHOR *) memset (fs_get (sizeof (SEARCHOR)),0,
				      sizeof (SEARCHOR));
  or->first = mail_newsearchpgm ();
  or->second = mail_newsearchpgm ();
  return or;
}

/* Mail instantiate new searchpgmlist
 * Returns: new searchpgmlist
 */

SEARCHPGMLIST *mail_newsearchpgmlist (void)
{
  SEARCHPGMLIST *pgl = (SEARCHPGMLIST *)
    memset (fs_get (sizeof (SEARCHPGMLIST)),0,sizeof (SEARCHPGMLIST));
  pgl->pgm = mail_newsearchpgm ();
  return pgl;
}


/* Mail instantiate new sortpgm
 * Returns: new sortpgm
 */

SORTPGM *mail_newsortpgm (void)
{
  return (SORTPGM *) memset (fs_get (sizeof (SORTPGM)),0,sizeof (SORTPGM));
}


/* Mail instantiate new threadnode
 * Accepts: sort cache for thread node
 * Returns: new threadnode
 */

THREADNODE *mail_newthreadnode (SORTCACHE *sc)
{
  THREADNODE *thr = (THREADNODE *) memset (fs_get (sizeof (THREADNODE)),0,
					   sizeof (THREADNODE));
  if (sc) thr->sc = sc;		/* initialize sortcache */
  return thr;
}


/* Mail instantiate new acllist
 * Returns: new acllist
 */

ACLLIST *mail_newacllist (void)
{
  return (ACLLIST *) memset (fs_get (sizeof (ACLLIST)),0,sizeof (ACLLIST));
}


/* Mail instantiate new quotalist
 * Returns: new quotalist
 */

QUOTALIST *mail_newquotalist (void)
{
  return (QUOTALIST *) memset (fs_get (sizeof (QUOTALIST)),0,
			       sizeof (QUOTALIST));
}

/* Mail garbage collection routines */


/* Mail garbage collect body
 * Accepts: pointer to body pointer
 */

void mail_free_body (BODY **body)
{
  if (*body) {			/* only free if exists */
    mail_free_body_data (*body);/* free its data */
    fs_give ((void **) body);	/* return body to free storage */
  }
}


/* Mail garbage collect body data
 * Accepts: body pointer
 */

void mail_free_body_data (BODY *body)
{
  switch (body->type) {		/* free contents */
  case TYPEMULTIPART:		/* multiple part */
    mail_free_body_part (&body->nested.part);
    break;
  case TYPEMESSAGE:		/* encapsulated message */
    if (body->subtype && !strcmp (body->subtype,"RFC822")) {
      mail_free_stringlist (&body->nested.msg->lines);
      mail_gc_msg (body->nested.msg,GC_ENV | GC_TEXTS);
    }
    if (body->nested.msg) fs_give ((void **) &body->nested.msg);
    break;
  default:
    break;
  }
  if (body->subtype) fs_give ((void **) &body->subtype);
  mail_free_body_parameter (&body->parameter);
  if (body->id) fs_give ((void **) &body->id);
  if (body->description) fs_give ((void **) &body->description);
  if (body->disposition.type) fs_give ((void **) &body->disposition);
  if (body->disposition.parameter)
    mail_free_body_parameter (&body->disposition.parameter);
  if (body->language) mail_free_stringlist (&body->language);
  if (body->location) fs_give ((void **) &body->location);
  if (body->mime.text.data) fs_give ((void **) &body->mime.text.data);
  if (body->contents.text.data) fs_give ((void **) &body->contents.text.data);
  if (body->md5) fs_give ((void **) &body->md5);
  if (mailfreebodysparep && body->sparep)
      (*mailfreebodysparep) (&body->sparep);
}

/* Mail garbage collect body parameter
 * Accepts: pointer to body parameter pointer
 */

void mail_free_body_parameter (PARAMETER **parameter)
{
  if (*parameter) {		/* only free if exists */
    if ((*parameter)->attribute) fs_give ((void **) &(*parameter)->attribute);
    if ((*parameter)->value) fs_give ((void **) &(*parameter)->value);
				/* run down the list as necessary */
    mail_free_body_parameter (&(*parameter)->next);
				/* return body part to free storage */
    fs_give ((void **) parameter);
  }
}


/* Mail garbage collect body part
 * Accepts: pointer to body part pointer
 */

void mail_free_body_part (PART **part)
{
  if (*part) {			/* only free if exists */
    mail_free_body_data (&(*part)->body);
				/* run down the list as necessary */
    mail_free_body_part (&(*part)->next);
    fs_give ((void **) part);	/* return body part to free storage */
  }
}

/* Mail garbage collect message cache
 * Accepts: mail stream
 *
 * The message cache is set to NIL when this function finishes.
 */

void mail_free_cache (MAILSTREAM *stream)
{
				/* do driver specific stuff first */
  mail_gc (stream,GC_ELT | GC_ENV | GC_TEXTS);
				/* flush the cache */
  (*mailcache) (stream,(long) 0,CH_INIT);
}


/* Mail garbage collect cache element
 * Accepts: pointer to cache element pointer
 */

void mail_free_elt (MESSAGECACHE **elt)
{
				/* only free if exists and no sharers */
  if (*elt && !--(*elt)->lockcount) {
    mail_gc_msg (&(*elt)->private.msg,GC_ENV | GC_TEXTS);
    if (mailfreeeltsparep && (*elt)->sparep)
      (*mailfreeeltsparep) (&(*elt)->sparep);
    fs_give ((void **) elt);
  }
  else *elt = NIL;		/* else simply drop pointer */
}

/* Mail garbage collect envelope
 * Accepts: pointer to envelope pointer
 */

void mail_free_envelope (ENVELOPE **env)
{
  if (*env) {			/* only free if exists */
    if ((*env)->remail) fs_give ((void **) &(*env)->remail);
    mail_free_address (&(*env)->return_path);
    if ((*env)->date) fs_give ((void **) &(*env)->date);
    mail_free_address (&(*env)->from);
    mail_free_address (&(*env)->sender);
    mail_free_address (&(*env)->reply_to);
    if ((*env)->subject) fs_give ((void **) &(*env)->subject);
    mail_free_address (&(*env)->to);
    mail_free_address (&(*env)->cc);
    mail_free_address (&(*env)->bcc);
    if ((*env)->in_reply_to) fs_give ((void **) &(*env)->in_reply_to);
    if ((*env)->message_id) fs_give ((void **) &(*env)->message_id);
    if ((*env)->newsgroups) fs_give ((void **) &(*env)->newsgroups);
    if ((*env)->followup_to) fs_give ((void **) &(*env)->followup_to);
    if ((*env)->references) fs_give ((void **) &(*env)->references);
    if (mailfreeenvelopesparep && (*env)->sparep)
      (*mailfreeenvelopesparep) (&(*env)->sparep);
    fs_give ((void **) env);	/* return envelope to free storage */
  }
}


/* Mail garbage collect address
 * Accepts: pointer to address pointer
 */

void mail_free_address (ADDRESS **address)
{
  if (*address) {		/* only free if exists */
    if ((*address)->personal) fs_give ((void **) &(*address)->personal);
    if ((*address)->adl) fs_give ((void **) &(*address)->adl);
    if ((*address)->mailbox) fs_give ((void **) &(*address)->mailbox);
    if ((*address)->host) fs_give ((void **) &(*address)->host);
    if ((*address)->error) fs_give ((void **) &(*address)->error);
    if ((*address)->orcpt.type) fs_give ((void **) &(*address)->orcpt.type);
    if ((*address)->orcpt.addr) fs_give ((void **) &(*address)->orcpt.addr);
    mail_free_address (&(*address)->next);
    fs_give ((void **) address);/* return address to free storage */
  }
}


/* Mail garbage collect stringlist
 * Accepts: pointer to stringlist pointer
 */

void mail_free_stringlist (STRINGLIST **string)
{
  if (*string) {		/* only free if exists */
    if ((*string)->text.data) fs_give ((void **) &(*string)->text.data);
    mail_free_stringlist (&(*string)->next);
    fs_give ((void **) string);	/* return string to free storage */
  }
}

/* Mail garbage collect searchpgm
 * Accepts: pointer to searchpgm pointer
 */

void mail_free_searchpgm (SEARCHPGM **pgm)
{
  if (*pgm) {			/* only free if exists */
    mail_free_searchset (&(*pgm)->msgno);
    mail_free_searchset (&(*pgm)->uid);
    mail_free_searchor (&(*pgm)->or);
    mail_free_searchpgmlist (&(*pgm)->not);
    mail_free_searchheader (&(*pgm)->header);
    mail_free_stringlist (&(*pgm)->bcc);
    mail_free_stringlist (&(*pgm)->body);
    mail_free_stringlist (&(*pgm)->cc);
    mail_free_stringlist (&(*pgm)->from);
    mail_free_stringlist (&(*pgm)->keyword);
    mail_free_stringlist (&(*pgm)->subject);
    mail_free_stringlist (&(*pgm)->text);
    mail_free_stringlist (&(*pgm)->to);
    fs_give ((void **) pgm);	/* return program to free storage */
  }
}


/* Mail garbage collect searchheader
 * Accepts: pointer to searchheader pointer
 */

void mail_free_searchheader (SEARCHHEADER **hdr)
{
  if (*hdr) {			/* only free if exists */
    if ((*hdr)->line.data) fs_give ((void **) &(*hdr)->line.data);
    if ((*hdr)->text.data) fs_give ((void **) &(*hdr)->text.data);
    mail_free_searchheader (&(*hdr)->next);
    fs_give ((void **) hdr);	/* return header to free storage */
  }
}


/* Mail garbage collect searchset
 * Accepts: pointer to searchset pointer
 */

void mail_free_searchset (SEARCHSET **set)
{
  if (*set) {			/* only free if exists */
    mail_free_searchset (&(*set)->next);
    fs_give ((void **) set);	/* return set to free storage */
  }
}

/* Mail garbage collect searchor
 * Accepts: pointer to searchor pointer
 */

void mail_free_searchor (SEARCHOR **orl)
{
  if (*orl) {			/* only free if exists */
    mail_free_searchpgm (&(*orl)->first);
    mail_free_searchpgm (&(*orl)->second);
    mail_free_searchor (&(*orl)->next);
    fs_give ((void **) orl);	/* return searchor to free storage */
  }
}


/* Mail garbage collect search program list
 * Accepts: pointer to searchpgmlist pointer
 */

void mail_free_searchpgmlist (SEARCHPGMLIST **pgl)
{
  if (*pgl) {			/* only free if exists */
    mail_free_searchpgm (&(*pgl)->pgm);
    mail_free_searchpgmlist (&(*pgl)->next);
    fs_give ((void **) pgl);	/* return searchpgmlist to free storage */
  }
}


/* Mail garbage collect namespace
 * Accepts: poiner to namespace
 */

void mail_free_namespace (NAMESPACE **n)
{
  if (*n) {
    fs_give ((void **) &(*n)->name);
    mail_free_namespace (&(*n)->next);
    mail_free_body_parameter (&(*n)->param);
    fs_give ((void **) n);	/* return namespace to free storage */
  }
}

/* Mail garbage collect sort program
 * Accepts: pointer to sortpgm pointer
 */

void mail_free_sortpgm (SORTPGM **pgm)
{
  if (*pgm) {			/* only free if exists */
    mail_free_sortpgm (&(*pgm)->next);
    fs_give ((void **) pgm);	/* return sortpgm to free storage */
  }
}


/* Mail garbage collect thread node
 * Accepts: pointer to threadnode pointer
 */

void mail_free_threadnode (THREADNODE **thr)
{
  if (*thr) {			/* only free if exists */
    mail_free_threadnode (&(*thr)->branch);
    mail_free_threadnode (&(*thr)->next);
    fs_give ((void **) thr);	/* return threadnode to free storage */
  }
}


/* Mail garbage collect acllist
 * Accepts: pointer to acllist pointer
 */

void mail_free_acllist (ACLLIST **al)
{
  if (*al) {			/* only free if exists */
    if ((*al)->identifier) fs_give ((void **) &(*al)->identifier);
    if ((*al)->rights) fs_give ((void **) &(*al)->rights);
    mail_free_acllist (&(*al)->next);
    fs_give ((void **) al);	/* return acllist to free storage */
  }
}


/* Mail garbage collect quotalist
 * Accepts: pointer to quotalist pointer
 */

void mail_free_quotalist (QUOTALIST **ql)
{
  if (*ql) {			/* only free if exists */
    if ((*ql)->name) fs_give ((void **) &(*ql)->name);
    mail_free_quotalist (&(*ql)->next);
    fs_give ((void **) ql);	/* return quotalist to free storage */
  }
}

/* Link authenicator
 * Accepts: authenticator to add to list
 */

void auth_link (AUTHENTICATOR *auth)
{
  if (!auth->valid || (*auth->valid) ()) {
    AUTHENTICATOR **a = &mailauthenticators;
    while (*a) a = &(*a)->next;	/* find end of list of authenticators */
    *a = auth;			/* put authenticator at the end */
    auth->next = NIL;		/* this authenticator is the end of the list */
  }
}


/* Authenticate access
 * Accepts: mechanism name
 *	    responder function
 *	    argument count
 *	    argument vector
 * Returns: authenticated user name or NIL
 */

char *mail_auth (char *mechanism,authresponse_t resp,int argc,char *argv[])
{
  AUTHENTICATOR *auth;
  for (auth = mailauthenticators; auth; auth = auth->next)
    if (auth->server && !compare_cstring (auth->name,mechanism))
      return ((auth->flags & AU_SECURE) ||
	      !mail_parameters (NIL,GET_DISABLEPLAINTEXT,NIL)) ?
		(*auth->server) (resp,argc,argv) : NIL;
  return NIL;			/* no authenticator found */
}

/* Lookup authenticator index
 * Accepts: authenticator index
 * Returns: authenticator, or 0 if not found
 */

AUTHENTICATOR *mail_lookup_auth (unsigned long i)
{
  AUTHENTICATOR *auth = mailauthenticators;
  while (auth && --i) auth = auth->next;
  return auth;
}


/* Lookup authenticator name
 * Accepts: authenticator name
 *	    required authenticator flags
 * Returns: index in authenticator chain, or 0 if not found
 */

unsigned int mail_lookup_auth_name (char *mechanism,long flags)
{
  int i;
  AUTHENTICATOR *auth;
  for (i = 1, auth = mailauthenticators; auth; i++, auth = auth->next)
    if (auth->client && !(flags & ~auth->flags) &&
	!compare_cstring (auth->name,mechanism))
      return i;
  return 0;
}

/* Standard TCP/IP network driver */

static NETDRIVER tcpdriver = {
  tcp_open,			/* open connection */
  tcp_aopen,			/* open preauthenticated connection */
  tcp_getline,			/* get a line */
  tcp_getbuffer,		/* get a buffer */
  tcp_soutr,			/* output pushed data */
  tcp_sout,			/* output string */
  tcp_close,			/* close connection */
  tcp_host,			/* return host name */
  tcp_remotehost,		/* return remote host name */
  tcp_port,			/* return port number */
  tcp_localhost			/* return local host name */
};


/* Network open
 * Accepts: NETMBX specifier to open
 *	    default network driver
 *	    default port
 *	    SSL driver
 *	    SSL service name
 *	    SSL driver port
 * Returns: Network stream if success, else NIL
 */

NETSTREAM *net_open (NETMBX *mb,NETDRIVER *dv,unsigned long port,
		     NETDRIVER *ssld,char *ssls,unsigned long sslp)
{
  NETSTREAM *stream = NIL;
  char tmp[MAILTMPLEN];
  unsigned long flags = mb->novalidate ? NET_NOVALIDATECERT : 0;
  if (strlen (mb->host) >= NETMAXHOST) {
    sprintf (tmp,"Invalid host name: %.80s",mb->host);
    MM_LOG (tmp,ERROR);
  }
				/* use designated driver if given */
  else if (dv) stream = net_open_work (dv,mb->host,mb->service,port,mb->port,
				       flags);
  else if (mb->sslflag && ssld)	/* use ssl if sslflag lit */
    stream = net_open_work (ssld,mb->host,ssls,sslp,mb->port,flags);
				/* if trysslfirst and can open ssl... */
  else if ((mb->trysslflag || trysslfirst) && ssld &&
	   (stream = net_open_work (ssld,mb->host,ssls,sslp,mb->port,
				    flags | NET_SILENT | NET_TRYSSL))) {
    if (net_sout (stream,"",0)) mb->sslflag = T;
    else {
      net_close (stream);	/* flush fake SSL stream */
      stream = NIL;
    }
  }
				/* default to TCP driver */
  else stream = net_open_work (&tcpdriver,mb->host,mb->service,port,mb->port,
			       flags);
  return stream;
}

/* Network open worker routine
 * Accepts: network driver
 *	    host name
 *	    service name to look up port
 *	    port number if service name not found
 *	    port number to override service name
 *	    flags (passed on top of port)
 * Returns: Network stream if success, else NIL
 */

NETSTREAM *net_open_work (NETDRIVER *dv,char *host,char *service,
			  unsigned long port,unsigned long portoverride,
			  unsigned long flags)
{
  NETSTREAM *stream = NIL;
  void *tstream;
  if (service && (*service == '*')) {
    flags |= NET_NOOPENTIMEOUT;	/* mark that no timeout is desired */
    ++service;			/* no longer need the no timeout indicator */
  }
  if (portoverride) {		/* explicit port number? */
    service = NIL;		/* yes, override service name */
    port = portoverride;	/* use that instead of default port */
  }
  if (tstream = (*dv->open) (host,service,port | flags)) {
    stream = (NETSTREAM *) fs_get (sizeof (NETSTREAM));
    stream->stream = tstream;
    stream->dtb = dv;
  }
  return stream;
}


/* Network authenticated open
 * Accepts: network driver
 *	    NETMBX specifier
 *	    service specifier
 *	    return user name buffer
 * Returns: Network stream if success else NIL
 */

NETSTREAM *net_aopen (NETDRIVER *dv,NETMBX *mb,char *service,char *user)
{
  NETSTREAM *stream = NIL;
  void *tstream;
  if (!dv) dv = &tcpdriver;	/* default to TCP driver */
  if (tstream = (*dv->aopen) (mb,service,user)) {
    stream = (NETSTREAM *) fs_get (sizeof (NETSTREAM));
    stream->stream = tstream;
    stream->dtb = dv;
  }
  return stream;
}

/* Network receive line
 * Accepts: Network stream
 * Returns: text line string or NIL if failure
 */

char *net_getline (NETSTREAM *stream)
{
  return (*stream->dtb->getline) (stream->stream);
}


/* Network receive buffer
 * Accepts: Network stream (must be void * for use as readfn_t)
 *	    size in bytes
 *	    buffer to read into
 * Returns: T if success, NIL otherwise
 */

long net_getbuffer (void *st,unsigned long size,char *buffer)
{
  NETSTREAM *stream = (NETSTREAM *) st;
  return (*stream->dtb->getbuffer) (stream->stream,size,buffer);
}


/* Network send null-terminated string
 * Accepts: Network stream
 *	    string pointer
 * Returns: T if success else NIL
 */

long net_soutr (NETSTREAM *stream,char *string)
{
  return (*stream->dtb->soutr) (stream->stream,string);
}


/* Network send string
 * Accepts: Network stream
 *	    string pointer
 *	    byte count
 * Returns: T if success else NIL
 */

long net_sout (NETSTREAM *stream,char *string,unsigned long size)
{
  return (*stream->dtb->sout) (stream->stream,string,size);
}

/* Network close
 * Accepts: Network stream
 */

void net_close (NETSTREAM *stream)
{
  if (stream->stream) (*stream->dtb->close) (stream->stream);
  fs_give ((void **) &stream);
}


/* Network get host name
 * Accepts: Network stream
 * Returns: host name for this stream
 */

char *net_host (NETSTREAM *stream)
{
  return (*stream->dtb->host) (stream->stream);
}


/* Network get remote host name
 * Accepts: Network stream
 * Returns: host name for this stream
 */

char *net_remotehost (NETSTREAM *stream)
{
  return (*stream->dtb->remotehost) (stream->stream);
}

/* Network return port for this stream
 * Accepts: Network stream
 * Returns: port number for this stream
 */

unsigned long net_port (NETSTREAM *stream)
{
  return (*stream->dtb->port) (stream->stream);
}


/* Network get local host name
 * Accepts: Network stream
 * Returns: local host name
 */

char *net_localhost (NETSTREAM *stream)
{
  return (*stream->dtb->localhost) (stream->stream);
}
