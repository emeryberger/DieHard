/*
 * Program:	File routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	25 August 1993
 * Last Edited:	27 April 2004
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
#include <signal.h>
#include "mail.h"
#include "osdep.h"
#include <pwd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "rfc822.h"
#include "misc.h"
#include "dummy.h"

/* Types returned from phile_type() */

#define PTYPEBINARY 0		/* binary data */
#define PTYPETEXT 1		/* textual data */
#define PTYPECRTEXT 2		/* textual data with CR */
#define PTYPE8 4		/* textual 8bit data */
#define PTYPEISO2022JP 8	/* textual Japanese */
#define PTYPEISO2022KR 16	/* textual Korean */
#define PTYPEISO2022CN 32	/* textual Chinese */


/* PHILE I/O stream local data */
	
typedef struct phile_local {
  ENVELOPE *env;		/* file envelope */
  BODY *body;			/* file body */
  char tmp[MAILTMPLEN];		/* temporary buffer */
} PHILELOCAL;


/* Convenient access to local data */

#define LOCAL ((PHILELOCAL *) stream->local)


/* Function prototypes */

DRIVER *phile_valid (char *name);
int phile_isvalid (char *name,char *tmp);
void *phile_parameters (long function,void *value);
void phile_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void phile_list (MAILSTREAM *stream,char *ref,char *pat);
void phile_lsub (MAILSTREAM *stream,char *ref,char *pat);
long phile_create (MAILSTREAM *stream,char *mailbox);
long phile_delete (MAILSTREAM *stream,char *mailbox);
long phile_rename (MAILSTREAM *stream,char *old,char *newname);
long phile_status (MAILSTREAM *stream,char *mbx,long flags);
MAILSTREAM *phile_open (MAILSTREAM *stream);
int phile_type (unsigned char *s,unsigned long i,unsigned long *j);
void phile_close (MAILSTREAM *stream,long options);
ENVELOPE *phile_structure (MAILSTREAM *stream,unsigned long msgno,BODY **body,
			   long flags);
char *phile_header (MAILSTREAM *stream,unsigned long msgno,
		    unsigned long *length,long flags);
long phile_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags);
long phile_ping (MAILSTREAM *stream);
void phile_check (MAILSTREAM *stream);
void phile_expunge (MAILSTREAM *stream);
long phile_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options);
long phile_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);

/* File routines */


/* Driver dispatch used by MAIL */

DRIVER philedriver = {
  "phile",			/* driver name */
				/* driver flags */
  DR_LOCAL|DR_READONLY|DR_NOSTICKY,
  (DRIVER *) NIL,		/* next driver */
  phile_valid,			/* mailbox is valid for us */
  phile_parameters,		/* manipulate parameters */
  phile_scan,			/* scan mailboxes */
  phile_list,			/* list mailboxes */
  phile_lsub,			/* list subscribed mailboxes */
  NIL,				/* subscribe to mailbox */
  NIL,				/* unsubscribe from mailbox */
  dummy_create,			/* create mailbox */
  dummy_delete,			/* delete mailbox */
  dummy_rename,			/* rename mailbox */
  phile_status,			/* status of mailbox */
  phile_open,			/* open mailbox */
  phile_close,			/* close mailbox */
  NIL,				/* fetch message "fast" attributes */
  NIL,				/* fetch message flags */
  NIL,				/* fetch overview */
  phile_structure,		/* fetch message envelopes */
  phile_header,			/* fetch message header only */
  phile_text,			/* fetch message body only */
  NIL,				/* fetch partial message text */
  NIL,				/* unique identifier */
  NIL,				/* message number */
  NIL,				/* modify flags */
  NIL,				/* per-message modify flags */
  NIL,				/* search for message based on criteria */
  NIL,				/* sort messages */
  NIL,				/* thread messages */
  phile_ping,			/* ping mailbox to see if still alive */
  phile_check,			/* check for new messages */
  phile_expunge,		/* expunge deleted messages */
  phile_copy,			/* copy messages to another mailbox */
  phile_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM phileproto = {&philedriver};

/* File validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *phile_valid (char *name)
{
  char tmp[MAILTMPLEN];
  return phile_isvalid (name,tmp) ? &philedriver : NIL;
}


/* File test for valid mailbox
 * Accepts: mailbox name
 * Returns: T if valid, NIL otherwise
 */

int phile_isvalid (char *name,char *tmp)
{
  struct stat sbuf;
  char *s;
				/* INBOX never accepted, any other name is */
  return ((s = mailboxfile (tmp,name)) && *s && !stat (s,&sbuf) &&
	  !(sbuf.st_mode & S_IFDIR) &&
				/* only allow empty files if #ftp */
	  (sbuf.st_size || ((*name == '#') &&
			    ((name[1] == 'f') || (name[1] == 'F')) &&
			    ((name[2] == 't') || (name[2] == 'T')) &&
			    ((name[3] == 'p') || (name[3] == 'P')) &&
			    (name[4] == '/'))));
}

/* File manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *phile_parameters (long function,void *value)
{
  return NIL;
}

/* File mail scan mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void phile_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  if (stream) dummy_scan (NIL,ref,pat,contents);
}


/* File list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void phile_list (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_list (NIL,ref,pat);
}


/* File list subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void phile_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_lsub (NIL,ref,pat);
}


/* File status
 * Accepts: mail stream
 *	    mailbox name
 *	    status flags
 * Returns: T on success, NIL on failure
 */

long phile_status (MAILSTREAM *stream,char *mbx,long flags)
{
  char *s,tmp[MAILTMPLEN];
  MAILSTATUS status;
  struct stat sbuf;
  long ret = NIL;
  if ((s = mailboxfile (tmp,mbx)) && *s && !stat (s,&sbuf)) {
    status.flags = flags;	/* return status values */
    status.unseen = (stream && mail_elt (stream,1)->seen) ? 0 : 1;
    status.messages = status.recent = status.uidnext = 1;
    status.uidvalidity = sbuf.st_mtime;
				/* pass status to main program */
    mm_status (stream,mbx,&status);
    ret = LONGT;		/* success */
  }
  return ret;
}

/* File open
 * Accepts: Stream to open
 * Returns: Stream on success, NIL on failure
 */

MAILSTREAM *phile_open (MAILSTREAM *stream)
{
  int i,k,fd;
  unsigned long j,m;
  char *s,tmp[MAILTMPLEN];
  struct passwd *pw;
  struct stat sbuf;
  struct tm *t;
  MESSAGECACHE *elt;
  SIZEDTEXT *buf;
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return &phileproto;
  if (stream->local) fatal ("phile recycle stream");
				/* open associated file */
  if (!mailboxfile (tmp,stream->mailbox) || !tmp[0] || stat (tmp,&sbuf) ||
      (fd = open (tmp,O_RDONLY,NIL)) < 0) {
    sprintf (tmp,"Unable to open file %s",stream->mailbox);
    mm_log (tmp,ERROR);
    return NIL;
  }
  fs_give ((void **) &stream->mailbox);
  stream->mailbox = cpystr (tmp);
  stream->local = fs_get (sizeof (PHILELOCAL));
  mail_exists (stream,1);	/* make sure upper level knows */
  mail_recent (stream,1);
  elt = mail_elt (stream,1);	/* instantiate cache element */
  elt->valid = elt->recent = T;	/* mark valid flags */
  stream->sequence++;		/* bump sequence number */
  stream->rdonly = T;		/* make sure upper level knows readonly */
				/* instantiate a new envelope and body */
  LOCAL->env = mail_newenvelope ();
  LOCAL->body = mail_newbody ();

  t = gmtime (&sbuf.st_mtime);	/* get UTC time and Julian day */
  i = t->tm_hour * 60 + t->tm_min;
  k = t->tm_yday;
  t = localtime(&sbuf.st_mtime);/* get local time */
				/* calculate time delta */
  i = t->tm_hour * 60 + t->tm_min - i;
  if (k = t->tm_yday - k) i += ((k < 0) == (abs (k) == 1)) ? -24*60 : 24*60;
  k = abs (i);			/* time from UTC either way */
  elt->hours = t->tm_hour; elt->minutes = t->tm_min; elt->seconds = t->tm_sec;
  elt->day = t->tm_mday; elt->month = t->tm_mon + 1;
  elt->year = t->tm_year - (BASEYEAR - 1900);
  elt->zoccident = (k == i) ? 0 : 1;
  elt->zhours = k/60;
  elt->zminutes = k % 60;
  sprintf (tmp,"%s, %d %s %d %02d:%02d:%02d %c%02d%02d",
	   days[t->tm_wday],t->tm_mday,months[t->tm_mon],t->tm_year+1900,
	   t->tm_hour,t->tm_min,t->tm_sec,elt->zoccident ? '-' : '+',
	   elt->zhours,elt->zminutes);
				/* set up Date field */
  LOCAL->env->date = cpystr (tmp);

				/* fill in From field from file owner */
  LOCAL->env->from = mail_newaddr ();
  if (pw = getpwuid (sbuf.st_uid)) strcpy (tmp,pw->pw_name);
  else sprintf (tmp,"User-Number-%ld",(long) sbuf.st_uid);
  LOCAL->env->from->mailbox = cpystr (tmp);
  LOCAL->env->from->host = cpystr (mylocalhost ());
				/* set subject to be mailbox name */
  LOCAL->env->subject = cpystr (stream->mailbox);
				/* slurp the data */
  (buf = &elt->private.special.text)->size = sbuf.st_size;
  read (fd,buf->data = (unsigned char *) fs_get (buf->size + 1),buf->size);
  buf->data[buf->size] = '\0';
  close (fd);			/* close the file */
				/* analyze data type */
  if (i = phile_type (buf->data,buf->size,&j)) {
    LOCAL->body->type = TYPETEXT;
    LOCAL->body->subtype = cpystr ("PLAIN");
    if (!(i & PTYPECRTEXT)) {	/* change Internet newline format as needed */
      s = (char *) buf->data;	/* make copy of UNIX-format string */
      buf->data = NIL;		/* zap the buffer */
      buf->size = strcrlfcpy (&buf->data,&m,s,buf->size);
      fs_give ((void **) &s);	/* flush original UNIX-format string */
    }
    LOCAL->body->parameter = mail_newbody_parameter ();
    LOCAL->body->parameter->attribute = cpystr ("charset");
    LOCAL->body->parameter->value =
      cpystr ((i & PTYPEISO2022JP) ? "ISO-2022-JP" :
	      (i & PTYPEISO2022KR) ? "ISO-2022-KR" :
	      (i & PTYPEISO2022CN) ? "ISO-2022-CN" :
	      (i & PTYPE8) ? "X-UNKNOWN" : "US-ASCII");
    LOCAL->body->encoding = (i & PTYPE8) ? ENC8BIT : ENC7BIT;
    LOCAL->body->size.lines = j;
  }
  else {			/* binary data */
    LOCAL->body->type = TYPEAPPLICATION;
    LOCAL->body->subtype = cpystr ("OCTET-STREAM");
    LOCAL->body->parameter = mail_newbody_parameter ();
    LOCAL->body->parameter->attribute = cpystr ("name");
    LOCAL->body->parameter->value =
      cpystr ((s = (strrchr (stream->mailbox,'/'))) ? s+1 : stream->mailbox);
    LOCAL->body->encoding = ENCBASE64;
    buf->data = rfc822_binary (s = (char *) buf->data,buf->size,&buf->size);
    fs_give ((void **) &s);	/* flush originary binary contents */
  }
  phile_header (stream,1,&j,NIL);
  LOCAL->body->size.bytes = LOCAL->body->contents.text.size = buf->size;
  elt->rfc822_size = j + buf->size;
				/* only one message ever... */
  stream->uid_validity = sbuf.st_mtime;
  stream->uid_last = elt->private.uid = 1;
  return stream;		/* return stream alive to caller */
}

/* File determine data type
 * Accepts: data to examine
 *	    size of data
 *	    pointer to line count return
 * Returns: PTYPE mask of data type
 */

int phile_type (unsigned char *s,unsigned long i,unsigned long *j)
{
  int ret = PTYPETEXT;
  char *charvec = "bbbbbbbaaalaacaabbbbbbbbbbbebbbbaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
  *j = 0;			/* no lines */
				/* check type of every character */
  while (i--) switch (charvec[*s++]) {
  case 'A':
    ret |= PTYPE8;		/* 8bit character */
    break;
  case 'a':
    break;			/* ASCII character */
  case 'b':
    return PTYPEBINARY;		/* binary byte seen, stop immediately */
  case 'c':
    ret |= PTYPECRTEXT;		/* CR indicates Internet text */
    break;
  case 'e':			/* ESC */
    if (*s == '$') {		/* ISO-2022 sequence? */
      switch (s[1]) {
      case 'B': case '@': ret |= PTYPEISO2022JP; break;
      case ')':
	switch (s[2]) {
	case 'A': case 'E': case 'G': ret |= PTYPEISO2022CN; break;
	case 'C': ret |= PTYPEISO2022KR; break;
	}
      case '*':
	switch (s[2]) {
	case 'H': ret |= PTYPEISO2022CN; break;
	}
      case '+':
	switch (s[2]) {
	case 'I': case 'J': case 'K': case 'L': case 'M':
	  ret |= PTYPEISO2022CN; break;
	}
      }
    }
    break;
  case 'l':			/* newline */
    (*j)++;
    break;
  }
  return ret;			/* return type of data */
}

/* File close
 * Accepts: MAIL stream
 *	    close options
 */

void phile_close (MAILSTREAM *stream,long options)
{
  if (LOCAL) {			/* only if a file is open */
    fs_give ((void **) &mail_elt (stream,1)->private.special.text.data);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
  }
}

/* File fetch structure
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to return body
 *	    option flags
 * Returns: envelope of this message, body returned in body value
 *
 * Fetches the "fast" information as well
 */

ENVELOPE *phile_structure (MAILSTREAM *stream,unsigned long msgno,BODY **body,
			   long flags)
{
  if (body) *body = LOCAL->body;
  return LOCAL->env;		/* return the envelope */
}


/* File fetch message header
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned header text length
 *	    option flags
 * Returns: message header in RFC822 format
 */

char *phile_header (MAILSTREAM *stream,unsigned long msgno,
		    unsigned long *length,long flags)
{
  rfc822_header (LOCAL->tmp,LOCAL->env,LOCAL->body);
  *length = strlen (LOCAL->tmp);
  return LOCAL->tmp;
}


/* File fetch message text (body only)
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned stringstruct
 *	    option flags
 * Returns: T, always
 */

long phile_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  SIZEDTEXT *buf = &mail_elt (stream,msgno)->private.special.text;
  if (!(flags &FT_PEEK)) {	/* mark message as seen */
    mail_elt (stream,msgno)->seen = T;
    mm_flags (stream,msgno);
  }
  INIT (bs,mail_string,buf->data,buf->size);
  return T;
}

/* File ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 * No-op for readonly files, since read/writer can expunge it from under us!
 */

long phile_ping (MAILSTREAM *stream)
{
  return T;
}

/* File check mailbox
 * Accepts: MAIL stream
 * No-op for readonly files, since read/writer can expunge it from under us!
 */

void phile_check (MAILSTREAM *stream)
{
  mm_log ("Check completed",NIL);
}

/* File expunge mailbox
 * Accepts: MAIL stream
 */

void phile_expunge (MAILSTREAM *stream)
{
  mm_log ("Expunge ignored on readonly mailbox",NIL);
}

/* File copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    copy options
 * Returns: T if copy successful, else NIL
 */

long phile_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  char tmp[MAILTMPLEN];
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  if (pc) return (*pc) (stream,sequence,mailbox,options);
  sprintf (tmp,"Can't copy - file \"%s\" is not in valid mailbox format",
	   stream->mailbox);
  mm_log (tmp,ERROR);
  return NIL;
}


/* File append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    append callback function
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

long phile_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  char tmp[MAILTMPLEN],file[MAILTMPLEN];
  char *s = mailboxfile (file,mailbox);
  if (s && *s) 
    sprintf (tmp,"Can't append - not in valid mailbox format: %.80s",s);
  else sprintf (tmp,"Can't append - invalid name: %.80s",mailbox);
  mm_log (tmp,ERROR);
  return NIL;
}
