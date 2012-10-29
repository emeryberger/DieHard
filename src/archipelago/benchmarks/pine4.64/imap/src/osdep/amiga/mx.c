/*
 * Program:	MX mail routines
 *
 * Author(s):	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	3 May 1996
 * Last Edited:	4 November 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


#include <stdio.h>
#include <ctype.h>
#include <errno.h>
extern int errno;		/* just in case */
#include "mail.h"
#include "osdep.h"
#include <pwd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "mx.h"
#include "misc.h"
#include "dummy.h"

/* MX I/O stream local data */
	
typedef struct mx_local {
  int fd;			/* file descriptor of open index */
  char *dir;			/* spool directory name */
  unsigned char *buf;		/* temporary buffer */
  unsigned long buflen;		/* current size of temporary buffer */
  unsigned long cachedtexts;	/* total size of all cached texts */
  time_t scantime;		/* last time directory scanned */
} MXLOCAL;


/* Convenient access to local data */

#define LOCAL ((MXLOCAL *) stream->local)


/* Function prototypes */

DRIVER *mx_valid (char *name);
int mx_isvalid (char *name,char *tmp);
void *mx_parameters (long function,void *value);
void mx_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void mx_list (MAILSTREAM *stream,char *ref,char *pat);
void mx_lsub (MAILSTREAM *stream,char *ref,char *pat);
void mx_list_work (MAILSTREAM *stream,char *dir,char *pat,long level);
long mx_subscribe (MAILSTREAM *stream,char *mailbox);
long mx_unsubscribe (MAILSTREAM *stream,char *mailbox);
long mx_create (MAILSTREAM *stream,char *mailbox);
long mx_delete (MAILSTREAM *stream,char *mailbox);
long mx_rename (MAILSTREAM *stream,char *old,char *newname);
MAILSTREAM *mx_open (MAILSTREAM *stream);
void mx_close (MAILSTREAM *stream,long options);
void mx_fast (MAILSTREAM *stream,char *sequence,long flags);
char *mx_fast_work (MAILSTREAM *stream,MESSAGECACHE *elt);
char *mx_header (MAILSTREAM *stream,unsigned long msgno,unsigned long *length,
		 long flags);
long mx_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags);
void mx_flag (MAILSTREAM *stream,char *sequence,char *flag,long flags);
void mx_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt);
long mx_ping (MAILSTREAM *stream);
void mx_check (MAILSTREAM *stream);
void mx_expunge (MAILSTREAM *stream);
long mx_copy (MAILSTREAM *stream,char *sequence,char *mailbox,
	      long options);
long mx_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);

int mx_numsort (const void *d1,const void *d2);
char *mx_file (char *dst,char *name);
long mx_lockindex (MAILSTREAM *stream);
void mx_unlockindex (MAILSTREAM *stream);
void mx_setdate (char *file,MESSAGECACHE *elt);


/* MX mail routines */


/* Driver dispatch used by MAIL */

DRIVER mxdriver = {
  "mx",				/* driver name */
				/* driver flags */
  DR_MAIL|DR_LOCAL|DR_NOFAST|DR_CRLF|DR_LOCKING,
  (DRIVER *) NIL,		/* next driver */
  mx_valid,			/* mailbox is valid for us */
  mx_parameters,		/* manipulate parameters */
  mx_scan,			/* scan mailboxes */
  mx_list,			/* find mailboxes */
  mx_lsub,			/* find subscribed mailboxes */
  mx_subscribe,			/* subscribe to mailbox */
  mx_unsubscribe,		/* unsubscribe from mailbox */
  mx_create,			/* create mailbox */
  mx_delete,			/* delete mailbox */
  mx_rename,			/* rename mailbox */
  mail_status_default,		/* status of mailbox */
  mx_open,			/* open mailbox */
  mx_close,			/* close mailbox */
  mx_fast,			/* fetch message "fast" attributes */
  NIL,				/* fetch message flags */
  NIL,				/* fetch overview */
  NIL,				/* fetch message envelopes */
  mx_header,			/* fetch message header only */
  mx_text,			/* fetch message body only */
  NIL,				/* fetch partial message test */
  NIL,				/* unique identifier */
  NIL,				/* message number */
  mx_flag,			/* modify flags */
  mx_flagmsg,			/* per-message modify flags */
  NIL,				/* search for message based on criteria */
  NIL,				/* sort messages */
  NIL,				/* thread messages */
  mx_ping,			/* ping mailbox to see if still alive */
  mx_check,			/* check for new messages */
  mx_expunge,			/* expunge deleted messages */
  mx_copy,			/* copy messages to another mailbox */
  mx_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM mxproto = {&mxdriver};


/* MX mail validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *mx_valid (char *name)
{
  char tmp[MAILTMPLEN];
  return mx_isvalid (name,tmp) ? &mxdriver : NIL;
}

/* MX mail test for valid mailbox
 * Accepts: mailbox name
 *	    temporary buffer to use
 * Returns: T if valid, NIL otherwise
 */

int mx_isvalid (char *name,char *tmp)
{
  struct stat sbuf;
  errno = NIL;			/* zap error */
				/* validate name as directory */
  return (!stat (MXINDEX (tmp,name),&sbuf) &&
	  ((sbuf.st_mode & S_IFMT) == S_IFREG));
}


/* MX manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *mx_parameters (long function,void *value)
{
  return NIL;
}


/* MX scan mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void mx_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  if (stream) MM_LOG ("Scan not valid for mx mailboxes",ERROR);
}

/* MX list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void mx_list (MAILSTREAM *stream,char *ref,char *pat)
{
  char *s,test[MAILTMPLEN],file[MAILTMPLEN];
  long i = 0;
				/* get canonical form of name */
  if (stream && dummy_canonicalize (test,ref,pat)) {
				/* found any wildcards? */
    if (s = strpbrk (test,"%*")) {
				/* yes, copy name up to that point */
      strncpy (file,test,i = s - test);
      file[i] = '\0';		/* tie off */
    }
    else strcpy (file,test);	/* use just that name then */
				/* find directory name */
    if (s = strrchr (file,'/')) {
      *s = '\0';		/* found, tie off at that point */
      s = file;
    }
				/* do the work */
    mx_list_work (stream,s,test,0);
  }
}


/* MX list subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void mx_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_lsub (NIL,ref,pat);
}

/* MX list mailboxes worker routine
 * Accepts: mail stream
 *	    directory name to search
 *	    search pattern
 *	    search level
 */

void mx_list_work (MAILSTREAM *stream,char *dir,char *pat,long level)
{
  DIR *dp;
  struct direct *d;
  struct stat sbuf;
  char *cp,*np,curdir[MAILTMPLEN],name[MAILTMPLEN];
  if (dir && *dir) {		/* make mailbox and directory names */
    sprintf (name,"%s/",dir);	/* print name starts at directory */
    mx_file (curdir,dir);	/* path starts from mx_file() name */
  }
  else {			/* no directory, use mailbox home dir */
    mx_file (curdir,mailboxdir (name,NIL,NIL));
    name[0] = '\0';		/* dummy name */
  }
  if (dp = opendir (curdir)) {	/* open directory */
    np = name + strlen (name);
    cp = curdir + strlen (strcat (curdir,"/"));
    while (d = readdir (dp)) {	/* scan, ignore . and numeric names */
      if ((d->d_name[0] != '.') && !mx_select (d)) {
	if (level < (long) mail_parameters (NIL,GET_LISTMAXLEVEL,NIL)) {
	  strcpy (cp,d->d_name);/* make directory name */
	  strcpy (np,d->d_name);/* make mx name of directory name */
	  if (dmatch (name,pat,'/') && !stat (curdir,&sbuf) &&
	      ((sbuf.st_mode &= S_IFMT) == S_IFDIR))
	    mx_list_work (stream,name,pat,level+1);
	}
      }
      else if (!strcmp (d->d_name,MXINDEXNAME+1) && pmatch_full (dir,pat,'/'))
	mm_list (stream,'/',dir,NIL);
    }
    closedir (dp);		/* all done, flush directory */
  }
}

/* MX mail subscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to add to subscription list
 * Returns: T on success, NIL on failure
 */

long mx_subscribe (MAILSTREAM *stream,char *mailbox)
{
  return sm_subscribe (mailbox);
}


/* MX mail unsubscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to delete from subscription list
 * Returns: T on success, NIL on failure
 */

long mx_unsubscribe (MAILSTREAM *stream,char *mailbox)
{
  return sm_unsubscribe (mailbox);
}

/* MX mail create mailbox
 * Accepts: mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long mx_create (MAILSTREAM *stream,char *mailbox)
{
  int fd;
  char *s,tmp[MAILTMPLEN],mbx[MAILTMPLEN];
				/* assume error */
  sprintf (tmp,"Can't create mailbox %.80s: invalid MX-format name",mailbox);
				/* make sure valid name */
  for (s = mailbox; s && *s;) {
    if (isdigit (*s)) s++;	/* digit, check this node further... */
				/* all digit node, barf */
    else if (*s == '/') s = NIL;
				/* non-digit in node, skip to next node */
    else if (s = strchr (s+1,'/')) s++;
    else tmp[0] = NIL;		/* no more nodes, good name */
  }
  if (tmp[0]);			/* was there an error in the name? */
				/* must not already exist */
  else if (mx_isvalid (mailbox,tmp))
    sprintf (tmp,"Can't create mailbox %.80s: mailbox already exists",mailbox);
				/* create directory */
  else if (!dummy_create_path (stream,strcat (mx_file (mbx,mailbox),"/"),
			       get_dir_protection (mailbox)))
    sprintf (tmp,"Can't create mailbox leaf %.80s: %s",
	     mailbox,strerror (errno));
  else {			/* create index file */
    int mask = umask (0);
    if (((fd = open (MXINDEX (tmp,mailbox),O_WRONLY|O_CREAT|O_EXCL,
		     (int) mail_parameters (NIL,GET_MBXPROTECTION,mailbox)))<0)
	|| close (fd))
      sprintf (tmp,"Can't create mailbox index %.80s: %s",
	       mailbox,strerror (errno));
	
    else {			/* success */
      set_mbx_protections (mailbox,mbx);
      set_mbx_protections (mailbox,tmp);
      tmp[0] = NIL;
    }
    umask (mask);		/* restore mask */
  }
  if (!tmp[0]) return LONGT;	/* success */
  MM_LOG (tmp,ERROR);		/* some error */
  return NIL;
}

/* MX mail delete mailbox
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long mx_delete (MAILSTREAM *stream,char *mailbox)
{
  DIR *dirp;
  struct direct *d;
  char *s;
  char tmp[MAILTMPLEN];
  if (!mx_isvalid (mailbox,tmp))
    sprintf (tmp,"Can't delete mailbox %.80s: no such mailbox",mailbox);
				/* delete index */
  else if (unlink (MXINDEX (tmp,mailbox)))
    sprintf (tmp,"Can't delete mailbox %.80s index: %s",
	     mailbox,strerror (errno));
  else {			/* get directory name */
    *(s = strrchr (tmp,'/')) = '\0';
    if (dirp = opendir (tmp)) {	/* open directory */
      *s++ = '/';		/* restore delimiter */
				/* massacre messages */
      while (d = readdir (dirp)) if (mx_select (d)) {
	strcpy (s,d->d_name);	/* make path */
	unlink (tmp);		/* sayonara */
      }
      closedir (dirp);		/* flush directory */
    }
				/* try to remove the directory */
    if (rmdir (mx_file (tmp,mailbox))) {
      sprintf (tmp,"Can't delete name %.80s: %s",mailbox,strerror (errno));
      mm_log (tmp,WARN);
    }
    return T;			/* always success */
  }
  MM_LOG (tmp,ERROR);		/* something failed */
  return NIL;
}

/* MX mail rename mailbox
 * Accepts: MX mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long mx_rename (MAILSTREAM *stream,char *old,char *newname)
{
  char c,*s,tmp[MAILTMPLEN],tmp1[MAILTMPLEN];
  struct stat sbuf;
  if (!mx_isvalid (old,tmp))
    sprintf (tmp,"Can't rename mailbox %.80s: no such mailbox",old);
				/* new mailbox name must not be valid */
  else if (mx_isvalid (newname,tmp))
    sprintf (tmp,"Can't rename to mailbox %.80s: destination already exists",
	     newname);
				/* success if can rename the directory */
  else {			/* found superior to destination name? */
    if (s = strrchr (mx_file (tmp1,newname),'/')) {
      c = *++s;			/* remember first character of inferior */
      *s = '\0';		/* tie off to get just superior */
				/* name doesn't exist, create it */
      if ((stat (tmp1,&sbuf) || ((sbuf.st_mode & S_IFMT) != S_IFDIR)) &&
	  !dummy_create_path (stream,tmp1,get_dir_protection (newname)))
	return NIL;
      *s = c;			/* restore full name */
    }
    if (!rename (mx_file (tmp,old),tmp1)) {
				/* recreate file if renamed INBOX */
      if (!compare_cstring (old,"INBOX")) mx_create (NIL,"INBOX");
      return T;
    }
    sprintf (tmp,"Can't rename mailbox %.80s to %.80s: %s",
	     old,newname,strerror (errno));
  }
  MM_LOG (tmp,ERROR);		/* something failed */
  return NIL;
}

/* MX mail open
 * Accepts: stream to open
 * Returns: stream on success, NIL on failure
 */

MAILSTREAM *mx_open (MAILSTREAM *stream)
{
  char tmp[MAILTMPLEN];
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return user_flags (&mxproto);
  if (stream->local) fatal ("mx recycle stream");
  stream->local = fs_get (sizeof (MXLOCAL));
				/* note if an INBOX or not */
  stream->inbox = !compare_cstring (stream->mailbox,"INBOX");
  mx_file (tmp,stream->mailbox);/* get directory name */
  LOCAL->dir = cpystr (tmp);	/* copy directory name for later */
				/* make temporary buffer */
  LOCAL->buf = (char *) fs_get ((LOCAL->buflen = MAXMESSAGESIZE) + 1);
  LOCAL->scantime = 0;		/* not scanned yet */
  LOCAL->fd = -1;		/* no index yet */
  LOCAL->cachedtexts = 0;	/* no cached texts */
  stream->sequence++;		/* bump sequence number */
				/* parse mailbox */
  stream->nmsgs = stream->recent = 0;
  if (mx_ping (stream) && !(stream->nmsgs || stream->silent))
    MM_LOG ("Mailbox is empty",(long) NIL);
  stream->perm_seen = stream->perm_deleted = stream->perm_flagged =
    stream->perm_answered = stream->perm_draft = stream->rdonly ? NIL : T;
  stream->perm_user_flags = stream->rdonly ? NIL : 0xffffffff;
  stream->kwd_create = (stream->user_flags[NUSERFLAGS-1] || stream->rdonly) ?
    NIL : T;			/* can we create new user flags? */
  return stream;		/* return stream to caller */
}

/* MX mail close
 * Accepts: MAIL stream
 *	    close options
 */

void mx_close (MAILSTREAM *stream,long options)
{
  if (LOCAL) {			/* only if a file is open */
    int silent = stream->silent;
    stream->silent = T;		/* note this stream is dying */
    if (options & CL_EXPUNGE) mx_expunge (stream);
    if (LOCAL->dir) fs_give ((void **) &LOCAL->dir);
				/* free local scratch buffer */
    if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
    stream->silent = silent;	/* reset silent state */
  }
}

/* MX mail fetch fast information
 * Accepts: MAIL stream
 *	    sequence
 *	    option flags
 */

void mx_fast (MAILSTREAM *stream,char *sequence,long flags)
{
  unsigned long i;
  MESSAGECACHE *elt;
  if (stream && LOCAL &&
      ((flags & FT_UID) ? mail_uid_sequence (stream,sequence) :
       mail_sequence (stream,sequence)))
    for (i = 1; i <= stream->nmsgs; i++)
      if ((elt = mail_elt (stream,i))->sequence) mx_fast_work (stream,elt);
}


/* MX mail fetch fast information
 * Accepts: MAIL stream
 *	    message cache element
 * Returns: name of message file
 */

char *mx_fast_work (MAILSTREAM *stream,MESSAGECACHE *elt)
{
  struct stat sbuf;
  struct tm *tm;
				/* build message file name */
  sprintf (LOCAL->buf,"%s/%lu",LOCAL->dir,elt->private.uid);
  if (!elt->rfc822_size) {	/* have size yet? */
    stat (LOCAL->buf,&sbuf);	/* get size of message */
				/* make plausible IMAPish date string */
    tm = gmtime (&sbuf.st_mtime);
    elt->day = tm->tm_mday; elt->month = tm->tm_mon + 1;
    elt->year = tm->tm_year + 1900 - BASEYEAR;
    elt->hours = tm->tm_hour; elt->minutes = tm->tm_min;
    elt->seconds = tm->tm_sec;
    elt->zhours = 0; elt->zminutes = 0; elt->zoccident = 0;
    elt->rfc822_size = sbuf.st_size;
  }
  return LOCAL->buf;		/* return file name */
}

/* MX mail fetch message header
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned header text length
 *	    option flags
 * Returns: message header in RFC822 format
 */

char *mx_header (MAILSTREAM *stream,unsigned long msgno,unsigned long *length,
		 long flags)
{
  unsigned long i;
  int fd;
  MESSAGECACHE *elt;
  *length = 0;			/* default to empty */
  if (flags & FT_UID) return "";/* UID call "impossible" */
  elt = mail_elt (stream,msgno);/* get elt */
  if (!elt->private.msg.header.text.data) {
				/* purge cache if too big */
    if (LOCAL->cachedtexts > max (stream->nmsgs * 4096,2097152)) {
      mail_gc (stream,GC_TEXTS);/* just can't keep that much */
      LOCAL->cachedtexts = 0;
    }
    if ((fd = open (mx_fast_work (stream,elt),O_RDONLY,NIL)) < 0) return "";
				/* is buffer big enough? */
    if (elt->rfc822_size > LOCAL->buflen) {
      fs_give ((void **) &LOCAL->buf);
      LOCAL->buf = (char *) fs_get ((LOCAL->buflen = elt->rfc822_size) + 1);
    }
				/* slurp message */
    read (fd,LOCAL->buf,elt->rfc822_size);
				/* tie off file */
    LOCAL->buf[elt->rfc822_size] = '\0';
    close (fd);			/* flush message file */
				/* find end of header */
    if (elt->rfc822_size < 4) i = 0;
    else for (i = 4; (i < elt->rfc822_size) &&
	      !((LOCAL->buf[i - 4] == '\015') &&
		(LOCAL->buf[i - 3] == '\012') &&
		(LOCAL->buf[i - 2] == '\015') &&
		(LOCAL->buf[i - 1] == '\012')); i++);
				/* copy header */
    cpytxt (&elt->private.msg.header.text,LOCAL->buf,i);
    cpytxt (&elt->private.msg.text.text,LOCAL->buf+i,elt->rfc822_size - i);
				/* add to cached size */
    LOCAL->cachedtexts += elt->rfc822_size;
  }
  *length = elt->private.msg.header.text.size;
  return (char *) elt->private.msg.header.text.data;
}

/* MX mail fetch message text (body only)
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned stringstruct
 *	    option flags
 * Returns: T on success, NIL on failure
 */

long mx_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  unsigned long i;
  MESSAGECACHE *elt;
				/* UID call "impossible" */
  if (flags & FT_UID) return NIL;
  elt = mail_elt (stream,msgno);
				/* snarf message if don't have it yet */
  if (!elt->private.msg.text.text.data) {
    mx_header (stream,msgno,&i,flags);
    if (!elt->private.msg.text.text.data) return NIL;
  }
				/* mark as seen */
  if (!(flags & FT_PEEK) && mx_lockindex (stream)) {
    elt->seen = T;
    mx_unlockindex (stream);
    MM_FLAGS (stream,msgno);
  }
  INIT (bs,mail_string,elt->private.msg.text.text.data,
	elt->private.msg.text.text.size);
  return T;
}

/* MX mail modify flags
 * Accepts: MAIL stream
 *	    sequence
 *	    flag(s)
 *	    option flags
 */

void mx_flag (MAILSTREAM *stream,char *sequence,char *flag,long flags)
{
  mx_unlockindex (stream);	/* finished with index */
}


/* MX per-message modify flags
 * Accepts: MAIL stream
 *	    message cache element
 */

void mx_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt)
{
  mx_lockindex (stream);	/* lock index if not already locked */
}

/* MX mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long mx_ping (MAILSTREAM *stream)
{
  MAILSTREAM *sysibx = NIL;
  MESSAGECACHE *elt,*selt;
  struct stat sbuf;
  char *s,tmp[MAILTMPLEN];
  int fd;
  unsigned long i,j,r,old;
  long nmsgs = stream->nmsgs;
  long recent = stream->recent;
  int silent = stream->silent;
  if (stat (LOCAL->dir,&sbuf)) return NIL;
  stream->silent = T;		/* don't pass up mm_exists() events yet */
  if (sbuf.st_ctime != LOCAL->scantime) {
    struct direct **names = NIL;
    long nfiles = scandir (LOCAL->dir,&names,mx_select,mx_numsort);
    if (nfiles < 0) nfiles = 0;	/* in case error */
    old = stream->uid_last;
				/* note scanned now */
    LOCAL->scantime = sbuf.st_ctime;
				/* scan directory */
    for (i = 0; i < nfiles; ++i) {
				/* if newly seen, add to list */
      if ((j = atoi (names[i]->d_name)) > old) {
				/* swell the cache */
	mail_exists (stream,++nmsgs);
	stream->uid_last = (elt = mail_elt (stream,nmsgs))->private.uid = j;
	elt->valid = T;		/* note valid flags */
	if (old) {		/* other than the first pass? */
	  elt->recent = T;	/* yup, mark as recent */
	  recent++;		/* bump recent count */
	}
      }
      fs_give ((void **) &names[i]);
    }
				/* free directory */
    if (s = (void *) names) fs_give ((void **) &s);
  }
  stream->nmsgs = nmsgs;	/* don't upset mail_uid() */

				/* if INBOX, snarf from system INBOX  */
  if (mx_lockindex (stream) && stream->inbox) {
    old = stream->uid_last;
				/* paranoia check */
    if (!strcmp (sysinbox (),stream->mailbox)) {
      stream->silent = silent;
      return NIL;
    }
    MM_CRITICAL (stream);	/* go critical */
    stat (sysinbox (),&sbuf);	/* see if anything there */
				/* can get sysinbox mailbox? */
    if (sbuf.st_size && (sysibx = mail_open (sysibx,sysinbox (),OP_SILENT))
	&& (!sysibx->rdonly) && (r = sysibx->nmsgs)) {
      for (i = 1; i <= r; ++i) {/* for each message in sysinbox mailbox */
				/* build file name we will use */
	sprintf (LOCAL->buf,"%s/%lu",LOCAL->dir,++old);
				/* snarf message from Berkeley mailbox */
	selt = mail_elt (sysibx,i);
	if (((fd = open (LOCAL->buf,O_WRONLY|O_CREAT|O_EXCL,
			 S_IREAD|S_IWRITE)) >= 0) &&
	    (s = mail_fetchheader_full (sysibx,i,NIL,&j,FT_PEEK)) &&
	    (write (fd,s,j) == j) &&
	    (s = mail_fetchtext_full (sysibx,i,&j,FT_PEEK)) &&
	    (write (fd,s,j) == j) && !fsync (fd) && !close (fd)) {
				/* swell the cache */
	  mail_exists (stream,++nmsgs);
	  stream->uid_last =	/* create new elt, note its file number */
	    (elt = mail_elt (stream,nmsgs))->private.uid = old;
	  recent++;		/* bump recent count */
				/* set up initial flags and date */
	  elt->valid = elt->recent = T;
	  elt->seen = selt->seen;
	  elt->deleted = selt->deleted;
	  elt->flagged = selt->flagged;
	  elt->answered = selt->answered;
	  elt->draft = selt->draft;
	  elt->day = selt->day;elt->month = selt->month;elt->year = selt->year;
	  elt->hours = selt->hours;elt->minutes = selt->minutes;
	  elt->seconds = selt->seconds;
	  elt->zhours = selt->zhours; elt->zminutes = selt->zminutes;
	  elt->zoccident = selt->zoccident;
	  mx_setdate (LOCAL->buf,elt);
	}
	else {			/* failed to snarf */
	  if (fd) {		/* did it ever get opened? */
	    close (fd);		/* close descriptor */
	    unlink (LOCAL->buf);/* flush this file */
	  }
	  stream->silent = silent;
	  return NIL;		/* note that something is badly wrong */
	}
	sprintf (tmp,"%lu",i);	/* delete it from the sysinbox */
	mail_flag (sysibx,tmp,"\\Deleted",ST_SET);
      }
      stat (LOCAL->dir,&sbuf);	/* update scan time */
      LOCAL->scantime = sbuf.st_ctime;      
      mail_expunge (sysibx);	/* now expunge all those messages */
    }
    if (sysibx) mail_close (sysibx);
    MM_NOCRITICAL (stream);	/* release critical */
  }
  mx_unlockindex (stream);	/* done with index */
  stream->silent = silent;	/* can pass up events now */
  mail_exists (stream,nmsgs);	/* notify upper level of mailbox size */
  mail_recent (stream,recent);
  return T;			/* return that we are alive */
}

/* MX mail check mailbox
 * Accepts: MAIL stream
 */

void mx_check (MAILSTREAM *stream)
{
  if (mx_ping (stream)) MM_LOG ("Check completed",(long) NIL);
}


/* MX mail expunge mailbox
 * Accepts: MAIL stream
 */

void mx_expunge (MAILSTREAM *stream)
{
  MESSAGECACHE *elt;
  unsigned long i = 1;
  unsigned long n = 0;
  unsigned long recent = stream->recent;
  if (mx_lockindex (stream)) {	/* lock the index */
    MM_CRITICAL (stream);	/* go critical */
    while (i <= stream->nmsgs) {/* for each message */
				/* if deleted, need to trash it */
      if ((elt = mail_elt (stream,i))->deleted) {
	sprintf (LOCAL->buf,"%s/%lu",LOCAL->dir,elt->private.uid);
	if (unlink (LOCAL->buf)) {/* try to delete the message */
	  sprintf (LOCAL->buf,"Expunge of message %lu failed, aborted: %s",i,
		   strerror (errno));
	  MM_LOG (LOCAL->buf,(long) NIL);
	  break;
	}
				/* note uncached */
	LOCAL->cachedtexts -= ((elt->private.msg.header.text.data ?
				elt->private.msg.header.text.size : 0) +
			       (elt->private.msg.text.text.data ?
				elt->private.msg.text.text.size : 0));
	mail_gc_msg (&elt->private.msg,GC_ENV | GC_TEXTS);
	if(elt->recent)--recent;/* if recent, note one less recent message */
	mail_expunged(stream,i);/* notify upper levels */
	n++;			/* count up one more expunged message */
      }
      else i++;			/* otherwise try next message */
    }
    if (n) {			/* output the news if any expunged */
      sprintf (LOCAL->buf,"Expunged %lu messages",n);
      MM_LOG (LOCAL->buf,(long) NIL);
    }
    else MM_LOG ("No messages deleted, so no update needed",(long) NIL);
    MM_NOCRITICAL (stream);	/* release critical */
    mx_unlockindex (stream);	/* finished with index */
  }
				/* notify upper level of new mailbox size */
  mail_exists (stream,stream->nmsgs);
  mail_recent (stream,recent);
}

/* MX mail copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    copy options
 * Returns: T if copy successful, else NIL
 */

long mx_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  STRING st;
  MESSAGECACHE *elt;
  struct stat sbuf;
  int fd;
  unsigned long i,j;
  char *t,flags[MAILTMPLEN],date[MAILTMPLEN];
				/* copy the messages */
  if ((options & CP_UID) ? mail_uid_sequence (stream,sequence) :
      mail_sequence (stream,sequence))
    for (i = 1; i <= stream->nmsgs; i++) 
      if ((elt = mail_elt (stream,i))->sequence) {
	if ((fd = open (mx_fast_work (stream,elt),O_RDONLY,NIL))<0) return NIL;
	fstat (fd,&sbuf);	/* get size of message */
				/* is buffer big enough? */
	if (sbuf.st_size > LOCAL->buflen) {
	  fs_give ((void **) &LOCAL->buf);
	  LOCAL->buf = (char *) fs_get ((LOCAL->buflen = sbuf.st_size) + 1);
	}
				/* slurp message */
	read (fd,LOCAL->buf,sbuf.st_size);
				/* tie off file */
	LOCAL->buf[sbuf.st_size] = '\0';
	close (fd);		/* flush message file */
	INIT (&st,mail_string,(void *) LOCAL->buf,sbuf.st_size);
				/* init flag string */
	flags[0] = flags[1] = '\0';
	if (j = elt->user_flags) do
	  if (t = stream->user_flags[find_rightmost_bit (&j)])
	    strcat (strcat (flags," "),t);
	while (j);
	if (elt->seen) strcat (flags," \\Seen");
	if (elt->deleted) strcat (flags," \\Deleted");
	if (elt->flagged) strcat (flags," \\Flagged");
	if (elt->answered) strcat (flags," \\Answered");
	if (elt->draft) strcat (flags," \\Draft");
	flags[0] = '(';		/* open list */
	strcat (flags,")");	/* close list */
	mail_date (date,elt);	/* generate internal date */
	if (!mail_append_full (NIL,mailbox,flags,date,&st)) return NIL;
	if (options & CP_MOVE) elt->deleted = T;
      }
  return T;			/* return success */
}

/* MX mail append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    append callback
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

long mx_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  MESSAGECACHE *elt,selt;
  MAILSTREAM *astream;
  int fd;
  char *flags,*date,*s,tmp[MAILTMPLEN];
  STRING *message;
  long f,i,size;
  unsigned long uf;
  long ret = LONGT;
				/* default stream to prototype */
  if (!stream) stream = user_flags (&mxproto);
				/* N.B.: can't use LOCAL->buf for tmp */
				/* make sure valid mailbox */
  if (!mx_isvalid (mailbox,tmp)) switch (errno) {
  case ENOENT:			/* no such file? */
    if (!compare_cstring (mailbox,"INBOX")) mx_create (NIL,"INBOX");
    else {
      MM_NOTIFY (stream,"[TRYCREATE] Must create mailbox before append",NIL);
      return NIL;
    }
				/* falls through */
  case 0:			/* merely empty file? */
    break;
  case EINVAL:
    sprintf (tmp,"Invalid MX-format mailbox name: %.80s",mailbox);
    MM_LOG (tmp,ERROR);
    return NIL;
  default:
    sprintf (tmp,"Not a MX-format mailbox: %.80s",mailbox);
    MM_LOG (tmp,ERROR);
    return NIL;
  }
				/* get first message */
  if (!MM_APPEND (af) (stream,data,&flags,&date,&message)) return NIL;
  if (!(astream = mail_open (NIL,mailbox,OP_SILENT))) {
    sprintf (tmp,"Can't open append mailbox: %s",strerror (errno));
    MM_LOG (tmp,ERROR);
    return NIL;
  }
  MM_CRITICAL (stream);	/* go critical */

				/* lock the index */
  if (mx_lockindex (astream)) do {
    if (!SIZE (message)) {	/* guard against zero-length */
      MM_LOG ("Append of zero-length message",ERROR);
      ret = NIL;
      break;
    }
				/* parse flags */
    f = mail_parse_flags (astream,flags,&uf);
    if (date) {			/* want to preserve date? */
				/* yes, parse date into an elt */
      if (!mail_parse_date (&selt,date)) {
	sprintf (tmp,"Bad date in append: %.80s",date);
	MM_LOG (tmp,ERROR);
	ret = NIL;
	break;
      }
    }
    mx_file (tmp,mailbox);	/* make message name */
    sprintf (tmp + strlen (tmp),"/%lu",++astream->uid_last);
    if ((fd = open (tmp,O_WRONLY|O_CREAT|O_EXCL,S_IREAD|S_IWRITE)) < 0) {
      sprintf (tmp,"Can't create append message: %s",strerror (errno));
      MM_LOG (tmp,ERROR);
      ret = NIL;
      break;
    }
				/* copy message */
    s = (char *) fs_get (size = SIZE (message));
    for (i = 0; i < size; s[i++] = SNX (message));
				/* write the data */
    if ((write (fd,s,size) < 0) || fsync (fd)) {
      unlink (tmp);		/* delete mailbox */
      sprintf (tmp,"Message append failed: %s",strerror (errno));
      MM_LOG (tmp,ERROR);
      ret = NIL;
    }
    fs_give ((void **) &s);	/* flush the buffer */
    close (fd);			/* close the file */
    if (ret) {			/* set the date for this message */
      if (date) mx_setdate (tmp,&selt);
				/* swell the cache */
      mail_exists (astream,++astream->nmsgs);
				/* copy flags */
      (elt = mail_elt (astream,astream->nmsgs))->private.uid=astream->uid_last;
      if (f&fSEEN) elt->seen = T;
      if (f&fDELETED) elt->deleted = T;
      if (f&fFLAGGED) elt->flagged = T;
      if (f&fANSWERED) elt->answered = T;
      if (f&fDRAFT) elt->draft = T;
      elt->user_flags |= uf;
				/* get next message */
      if (!MM_APPEND (af) (stream,data,&flags,&date,&message)) ret = NIL;
    }
  } while (ret && message);
  else {
    MM_LOG ("Message append failed: unable to lock index",ERROR);
    ret = NIL;
  }
  mx_unlockindex (astream);	/* unlock index */
  MM_NOCRITICAL (stream);	/* release critical */
  mail_close (astream);
  return ret;
}

/* Internal routines */


/* MX file name selection test
 * Accepts: candidate directory entry
 * Returns: T to use file name, NIL to skip it
 */

int mx_select (struct direct *name)
{
  char c;
  char *s = name->d_name;
  while (c = *s++) if (!isdigit (c)) return NIL;
  return T;
}


/* MX file name comparision
 * Accepts: first candidate directory entry
 *	    second candidate directory entry
 * Returns: negative if d1 < d2, 0 if d1 == d2, postive if d1 > d2
 */

int mx_numsort (const void *d1,const void *d2)
{
  return atoi ((*(struct direct **) d1)->d_name) -
    atoi ((*(struct direct **) d2)->d_name);
}


/* MX mail build file name
 * Accepts: destination string
 *          source
 * Returns: destination
 */

char *mx_file (char *dst,char *name)
{
  char *s;
  if (!(mailboxfile (dst,name) && *dst)) return mailboxfile (dst,"~/INBOX");
				/* tie off unnecessary trailing / */
  if ((s = strrchr (dst,'/')) && !s[1]) *s = '\0';
  return dst;
}

/* MX read and lock index
 * Accepts: MAIL stream
 * Returns: T if success, NIL if failure
 */

long mx_lockindex (MAILSTREAM *stream)
{
  unsigned long uf,sf,uid;
  int k = 0;
  unsigned long msgno = 1;
  struct stat sbuf;
  char *s,*t,*idx,tmp[MAILTMPLEN];
  MESSAGECACHE *elt;
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  if ((LOCAL->fd < 0) &&	/* get index file, no-op if already have it */
      (LOCAL->fd = open (strcat (strcpy (tmp,LOCAL->dir),MXINDEXNAME),
			 O_RDWR|O_CREAT,S_IREAD|S_IWRITE)) >= 0) {
    (*bn) (BLOCK_FILELOCK,NIL);
    flock (LOCAL->fd,LOCK_EX);	/* get exclusive lock */
    (*bn) (BLOCK_NONE,NIL);
    fstat (LOCAL->fd,&sbuf);	/* get size of index */
				/* slurp index */
    read (LOCAL->fd,s = idx = (char *) fs_get (sbuf.st_size + 1),sbuf.st_size);
    idx[sbuf.st_size] = '\0';	/* tie off index */
				/* parse index */
    if (sbuf.st_size) while (s && *s) switch (*s) {
    case 'V':			/* UID validity record */
      stream->uid_validity = strtoul (s+1,&s,16);
      break;
    case 'L':			/* UID last record */
      stream->uid_last = strtoul (s+1,&s,16);
      break;
    case 'K':			/* keyword */
				/* find end of keyword */
      if (s = strchr (t = ++s,'\n')) {
	*s++ = '\0';		/* tie off keyword */
				/* copy keyword */
	if ((k < NUSERFLAGS) && !stream->user_flags[k] &&
	    (strlen (t) <= MAXUSERFLAG)) stream->user_flags[k] = cpystr (t);
	k++;			/* one more keyword */
      }
      break;

    case 'M':			/* message status record */
      uid = strtoul (s+1,&s,16);/* get UID for this message */
      if (*s == ';') {		/* get user flags */
	uf = strtoul (s+1,&s,16);
	if (*s == '.') {	/* get system flags */
	  sf = strtoul (s+1,&s,16);
	  while ((msgno <= stream->nmsgs) && (mail_uid (stream,msgno) < uid))
	    msgno++;		/* find message number for this UID */
	  if ((msgno <= stream->nmsgs) && (mail_uid (stream,msgno) == uid)) {
	    (elt = mail_elt (stream,msgno))->valid = T;
	    elt->user_flags=uf; /* set user and system flags in elt */
	    if (sf & fSEEN) elt->seen = T;
	    if (sf & fDELETED) elt->deleted = T;
	    if (sf & fFLAGGED) elt->flagged = T;
	    if (sf & fANSWERED) elt->answered = T;
	    if (sf & fDRAFT) elt->draft = T;
	  }
	  break;
	}
      }
    default:			/* bad news */
      sprintf (tmp,"Error in index: %.80s",s);
      MM_LOG (tmp,ERROR);
      *s = NIL;			/* ignore remainder of index */
    }
    else {			/* new index */
      stream->uid_validity = time (0);
      user_flags (stream);	/* init stream with default user flags */
    }
    fs_give ((void **) &idx);	/* flush index */
  }
  return (LOCAL->fd >= 0) ? T : NIL;
}

/* MX write and unlock index
 * Accepts: MAIL stream
 */

void mx_unlockindex (MAILSTREAM *stream)
{
  unsigned long i,j;
  off_t size = 0;
  char *s,tmp[MAILTMPLEN + 64];
  MESSAGECACHE *elt;
  if (LOCAL->fd >= 0) {
    lseek (LOCAL->fd,0,L_SET);	/* rewind file */
				/* write header */
    sprintf (s = tmp,"V%08lxL%08lx",stream->uid_validity,stream->uid_last);
    for (i = 0; (i < NUSERFLAGS) && stream->user_flags[i]; ++i)
      sprintf (s += strlen (s),"K%s\n",stream->user_flags[i]);
				/* write messages */
    for (i = 1; i <= stream->nmsgs; i++) {
				/* filled buffer? */
      if (((s += strlen (s)) - tmp) > MAILTMPLEN) {
	write (LOCAL->fd,tmp,j = s - tmp);
	size += j;
	*(s = tmp) = '\0';	/* dump out and restart buffer */
      }
      elt = mail_elt (stream,i);
      sprintf(s,"M%08lx;%08lx.%04x",elt->private.uid,elt->user_flags,(unsigned)
	      ((fSEEN * elt->seen) + (fDELETED * elt->deleted) +
	       (fFLAGGED * elt->flagged) + (fANSWERED * elt->answered) +
	       (fDRAFT * elt->draft)));
    }
				/* write tail end of buffer */
    if ((s += strlen (s)) != tmp) {
      write (LOCAL->fd,tmp,j = s - tmp);
      size += j;
    }
    ftruncate (LOCAL->fd,size);
    flock (LOCAL->fd,LOCK_UN);	/* unlock the index */
    close (LOCAL->fd);		/* finished with file */
    LOCAL->fd = -1;		/* no index now */
  }
}

/* Set date for message
 * Accepts: file name
 *	    elt containing date
 */

void mx_setdate (char *file,MESSAGECACHE *elt)
{
  time_t tp[2];
  tp[0] = time (0);		/* atime is now */
  tp[1] = mail_longdate (elt);	/* modification time */
  utime (file,tp);		/* set the times */
}
