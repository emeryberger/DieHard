/*
 * Program:	Berkeley mail routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	24 June 1992
 * Last Edited:	22 January 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Dedication:
 *  This file is dedicated with affection to those Merry Marvels of Musical
 * Madness . . .
 *  ->  The Incomparable Leland Stanford Junior University Marching Band  <-
 * who entertain, awaken, and outrage Stanford fans in the fact of repeated
 * losing seasons and shattered Rose Bowl dreams [Cardinal just don't have
 * HUSKY FEVER!!!].
 *
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include "mail.h"
#include "osdep.h"
#include <time.h>
#include <sys\stat.h>
#include <dos.h>
#include "bezrkdos.h"
#include "rfc822.h"
#include "dummy.h"
#include "misc.h"
#include "fdstring.h"

/* Build parameters */

#define CHUNK 4096


/* Berkeley I/O stream local data */
	
typedef struct bezerk_local {
  int fd;			/* file descriptor for I/O */
  off_t filesize;		/* file size parsed */
  char *buf;			/* temporary buffer */
} BEZERKLOCAL;


/* Convenient access to local data */

#define LOCAL ((BEZERKLOCAL *) stream->local)

/* Function prototypes */

DRIVER *bezerk_valid (char *name);
long bezerk_isvalid (char *name,char *tmp);
int bezerk_valid_line (char *s,char **rx,int *rzn);
void *bezerk_parameters (long function,void *value);
void bezerk_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void bezerk_list (MAILSTREAM *stream,char *ref,char *pat);
void bezerk_lsub (MAILSTREAM *stream,char *ref,char *pat);
long bezerk_create (MAILSTREAM *stream,char *mailbox);
long bezerk_delete (MAILSTREAM *stream,char *mailbox);
long bezerk_rename (MAILSTREAM *stream,char *old,char *newname);
MAILSTREAM *bezerk_open (MAILSTREAM *stream);
void bezerk_close (MAILSTREAM *stream,long options);
char *bezerk_header (MAILSTREAM *stream,unsigned long msgno,
		     unsigned long *length,long flags);
long bezerk_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,
		  long flags);
long bezerk_ping (MAILSTREAM *stream);
void bezerk_check (MAILSTREAM *stream);
void bezerk_expunge (MAILSTREAM *stream);
long bezerk_copy (MAILSTREAM *stream,char *sequence,char *mailbox,
		  long options);
long bezerk_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);
int bezerk_append_msg (MAILSTREAM *stream,FILE *sf,char *flags,char *date,
		     STRING *msg);
void bezerk_gc (MAILSTREAM *stream,long gcflags);
char *bezerk_file (char *dst,char *name);
long bezerk_badname (char *tmp,char *s);
long bezerk_parse (MAILSTREAM *stream);
unsigned long bezerk_hdrpos (MAILSTREAM *stream,unsigned long msgno,
			     unsigned long *size);

/* Berkeley mail routines */


/* Driver dispatch used by MAIL */

DRIVER bezerkdriver = {
  "bezerk",			/* driver name */
				/* driver flags */
  DR_LOCAL|DR_MAIL|DR_LOWMEM|DR_CRLF|DR_NOSTICKY,
  (DRIVER *) NIL,		/* next driver */
  bezerk_valid,			/* mailbox is valid for us */
  bezerk_parameters,		/* manipulate parameters */
  bezerk_scan,			/* scan mailboxes */
  bezerk_list,			/* list mailboxes */
  bezerk_lsub,			/* list subscribed mailboxes */
  NIL,				/* subscribe to mailbox */
  NIL,				/* unsubscribe from mailbox */
  bezerk_create,		/* create mailbox */
  bezerk_delete,		/* delete mailbox */
  bezerk_rename,		/* rename mailbox */
  mail_status_default,		/* status of mailbox */
  bezerk_open,			/* open mailbox */
  bezerk_close,			/* close mailbox */
  NIL,				/* fetch message "fast" attributes */
  NIL,				/* fetch message flags */
  NIL,				/* fetch overview */
  NIL,				/* fetch message envelopes */
  bezerk_header,		/* fetch message header */
  bezerk_text,			/* fetch message text */
  NIL,				/* fetch partial message text */
  NIL,				/* unique identifier */
  NIL,				/* message number */
  NIL,				/* modify flags */
  NIL,				/* per-message modify flags */
  NIL,				/* search for message based on criteria */
  NIL,				/* sort messages */
  NIL,				/* thread messages */
  bezerk_ping,			/* ping mailbox to see if still alive */
  bezerk_check,			/* check for new messages */
  bezerk_expunge,		/* expunge deleted messages */
  bezerk_copy,			/* copy messages to another mailbox */
  bezerk_append,		/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM bezerkproto = {&bezerkdriver};

/* Berkeley mail validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *bezerk_valid (char *name)
{
  char tmp[MAILTMPLEN];
  return bezerk_isvalid (name,tmp) ? &bezerkdriver : (DRIVER *) NIL;
}


/* Berkeley mail test for valid mailbox
 * Accepts: mailbox name
 * Returns: T if valid, NIL otherwise
 */

long bezerk_isvalid (char *name,char *tmp)
{
  int fd;
  long ret = NIL;
  struct stat sbuf;
  errno = EINVAL;		/* assume invalid argument */
				/* if file, get its status */
  if ((*name != '{') && mailboxfile (tmp,name) && !stat (tmp,&sbuf)) {
    if (!sbuf.st_size)errno = 0;/* empty file */
    else if ((fd = open (tmp,O_BINARY|O_RDONLY,NIL)) >= 0) {
      memset (tmp,'\0',MAILTMPLEN);
      errno = -1;		/* in case bezerk_valid_line fails */
      if (read (fd,tmp,MAILTMPLEN-1) >= 0)
	ret = bezerk_valid_line (tmp,NIL,NIL);
      close (fd);		/* close the file */
    }
  }
				/* in case INBOX but not bezerk format */
  else if ((errno == ENOENT) && ((name[0] == 'I') || (name[0] == 'i')) &&
	   ((name[1] == 'N') || (name[1] == 'n')) &&
	   ((name[2] == 'B') || (name[2] == 'b')) &&
	   ((name[3] == 'O') || (name[3] == 'o')) &&
	   ((name[4] == 'X') || (name[4] == 'x')) && !name[5]) errno = -1;
  return ret;			/* return what we should */
}

/* Validate line
 * Accepts: pointer to candidate string to validate as a From header
 *	    return pointer to end of date/time field
 *	    return pointer to offset from t of time (hours of ``mmm dd hh:mm'')
 *	    return pointer to offset from t of time zone (if non-zero)
 * Returns: t,ti,zn set if valid From string, else ti is NIL
 */

int bezerk_valid_line (char *s,char **rx,int *rzn)
{
  char *x;
  int zn;
  int ti = 0;
				/* line must begin with "From " */
  if ((*s != 'F') || (s[1] != 'r') || (s[2] != 'o') || (s[3] != 'm') ||
	(s[4] != ' ')) return NIL;
				/* find end of line */
  for (x = s + 5; *x && *x != '\012'; x++);
  if (!x) return NIL;		/* end of line not found */
  if (x[-1] == '\015') x--;	/* ignore CR */
  if ((x - s < 27)) return NIL;	/* line too short */
  if (x - s >= 41) {		/* possible search for " remote from " */
    for (zn = -1; x[zn] != ' '; zn--);
    if ((x[zn-1] == 'm') && (x[zn-2] == 'o') && (x[zn-3] == 'r') &&
	(x[zn-4] == 'f') && (x[zn-5] == ' ') && (x[zn-6] == 'e') &&
	(x[zn-7] == 't') && (x[zn-8] == 'o') && (x[zn-9] == 'm') &&
	(x[zn-10] == 'e') && (x[zn-11] == 'r') && (x[zn-12] == ' '))
      x += zn - 12;
  }
  if (x[-5] == ' ') {		/* ends with year? */
				/* no timezone? */
    if (x[-8] == ':') zn = 0,ti = -5;
				/* three letter timezone? */
    else if (x[-9] == ' ') ti = zn = -9;
				/* numeric timezone? */
    else if ((x[-11]==' ') && ((x[-10]=='+') || (x[-10]=='-'))) ti = zn = -11;
  }
  else if (x[-4] == ' ') {	/* no year and three leter timezone? */
    if (x[-9] == ' ') zn = -4,ti = -9;
  }
  else if (x[-6] == ' ') {	/* no year and numeric timezone? */
    if ((x[-11] == ' ') && ((x[-5] == '+') || (x[-5] == '-')))
      zn = -6,ti = -11;
  }
				/* time must be www mmm dd hh:mm[:ss] */
  if (ti && !((x[ti - 3] == ':') &&
	      (x[ti -= ((x[ti - 6] == ':') ? 9 : 6)] == ' ') &&
	      (x[ti - 3] == ' ') && (x[ti - 7] == ' ') &&
	      (x[ti - 11] == ' '))) return NIL;
  if (rx) *rx = x;		/* set return values */
  if (rzn) *rzn = zn;
  return ti;
}

/* Berkeley manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *bezerk_parameters (long function,void *value)
{
  return NIL;
}


/* Berkeley mail scan mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void bezerk_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  if (stream) dummy_scan (NIL,ref,pat,contents);
}


/* Berkeley mail list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void bezerk_list (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_list (stream,ref,pat);
}


/* Berkeley mail list subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void bezerk_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_lsub (stream,ref,pat);
}

/* Berkeley mail create mailbox
 * Accepts: MAIL stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long bezerk_create (MAILSTREAM *stream,char *mailbox)
{
  return dummy_create (stream,mailbox);
}


/* Berkeley mail delete mailbox
 * Accepts: MAIL stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long bezerk_delete (MAILSTREAM *stream,char *mailbox)
{
  return dummy_delete (stream,mailbox);
}


/* Berkeley mail rename mailbox
 * Accepts: MAIL stream
 *	    old mailbox name
 *	    new mailbox name (or NIL for delete)
 * Returns: T on success, NIL on failure
 */

long bezerk_rename (MAILSTREAM *stream,char *old,char *newname)
{
  return dummy_rename (stream,old,newname);
}

/* Berkeley mail open
 * Accepts: stream to open
 * Returns: stream on success, NIL on failure
 */

MAILSTREAM *bezerk_open (MAILSTREAM *stream)
{
  long i;
  int fd;
  char *s;
  char tmp[MAILTMPLEN];
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return &bezerkproto;
  if (stream->local) fatal ("bezerk recycle stream");
  if (!mailboxfile (tmp,stream->mailbox))
    return (MAILSTREAM *) bezerk_badname (tmp,stream->mailbox);
  if (((fd = open (tmp,O_BINARY|O_RDONLY,NIL)) < 0)) {
    sprintf (tmp,"Can't open mailbox: %s",strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
  stream->rdonly = T;		/* this driver is readonly */
  stream->local = fs_get (sizeof (BEZERKLOCAL));
				/* canonicalize the stream mailbox name */
  fs_give ((void **) &stream->mailbox);
  if (s = strchr ((s = strrchr (tmp,'\\')) ? s : tmp,'.')) *s = '\0';
  stream->mailbox = cpystr (tmp);
  LOCAL->fd = fd;		/* note the file */
  LOCAL->filesize = 0;		/* initialize parsed file size */
  LOCAL->buf = NIL;		/* initially no local buffer */
  stream->sequence++;		/* bump sequence number */
  stream->uid_validity = time (0);
				/* parse mailbox */
  stream->nmsgs = stream->recent = 0;
  if (!bezerk_ping (stream)) return NIL;
  if (!stream->nmsgs) mm_log ("Mailbox is empty",(long) NIL);
  stream->perm_seen = stream->perm_deleted =
    stream->perm_flagged = stream->perm_answered = stream->perm_draft = NIL;
  stream->perm_user_flags = NIL;
  return stream;		/* return stream to caller */
}

/* Berkeley mail close
 * Accepts: MAIL stream
 *	    close options
 */

void bezerk_close (MAILSTREAM *stream,long options)
{
  if (stream && LOCAL) {	/* only if a file is open */
    int silent = stream->silent;
    stream->silent = T;
    if (options & CL_EXPUNGE) bezerk_expunge (stream);
    close (LOCAL->fd);		/* close the local file */
    if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
  }
}

/* Berkeley mail fetch message header
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned header text length
 *	    option flags
 * Returns: message header in RFC822 format
 */

char *bezerk_header (MAILSTREAM *stream,unsigned long msgno,
		     unsigned long *length,long flags)
{
  char tmp[MAILTMPLEN];
  *length = 0;			/* default to empty */
  if (flags & FT_UID) return "";/* UID call "impossible" */
				/* get to header position */
  lseek (LOCAL->fd,bezerk_hdrpos (stream,msgno,length),L_SET);
				/* is buffer big enough? */
  if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
  LOCAL->buf = (char *) fs_get ((size_t) *length + 1);
  LOCAL->buf[*length] = '\0';	/* tie off string */
				/* slurp the data */
  read (LOCAL->fd,LOCAL->buf,(size_t) *length);
  return LOCAL->buf;
}


/* Berkeley mail fetch message text (body only)
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned header text length
 *	    option flags
 * Returns: T, always
 */

long bezerk_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  MESSAGECACHE *elt;
  FDDATA d;
  unsigned long hdrsize,hdrpos;
				/* UID call "impossible" */
  if (flags & FT_UID) return NIL;
  elt = mail_elt (stream,msgno);/* if message not seen */
				/* mark message as seen */
  if (elt->seen && !(flags & FT_PEEK)) {
    elt->seen = T;
    mm_flags (stream,msgno);
  }
				/* get location of text data */
  hdrpos = bezerk_hdrpos (stream,msgno,&hdrsize);
  d.fd = LOCAL->fd;		/* set initial stringstruct */
  d.pos = hdrpos + hdrsize;
				/* flush old buffer */
  if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
  d.chunk = LOCAL->buf = (char *) fs_get ((size_t) d.chunksize = CHUNK);
  INIT (bs,fd_string,(void *) &d,elt->rfc822_size - hdrsize);
  return T;			/* success */
}

/* Berkeley mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream still alive, NIL if not
 */

long bezerk_ping (MAILSTREAM *stream)
{
				/* punt if stream no longer alive */
  if (!(stream && LOCAL)) return NIL;
				/* parse mailbox, punt if parse dies */
  return (bezerk_parse (stream)) ? T : NIL;
}


/* Berkeley mail check mailbox (reparses status too)
 * Accepts: MAIL stream
 */

void bezerk_check (MAILSTREAM *stream)
{
  unsigned long i = 1;
  if (bezerk_ping (stream)) {	/* ping mailbox */
				/* get new message status */
    while (i <= stream->nmsgs) mail_elt (stream,i++);
    mm_log ("Check completed",(long) NIL);
  }
}

/* Berkeley mail expunge mailbox
 * Accepts: MAIL stream
 */

void bezerk_expunge (MAILSTREAM *stream)
{
  mm_log ("Expunge ignored on readonly mailbox",WARN);
}

/* Berkeley mail copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    copy options
 * Returns: T if success, NIL if failed
 */

long bezerk_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  char tmp[MAILTMPLEN];
  struct stat sbuf;
  MESSAGECACHE *elt;
  unsigned long i,j,k;
  int fd;
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  if (!((options & CP_UID) ? mail_uid_sequence (stream,sequence) :
	mail_sequence (stream,sequence))) return NIL;
				/* make sure valid mailbox */
  if (!bezerk_isvalid (mailbox,tmp) && errno) {
    if (errno == ENOENT)
      mm_notify (stream,"[TRYCREATE] Must create mailbox before append",
		 (long) NIL);
    else if (pc) return (*pc) (stream,sequence,mailbox,options);
    else if (mailboxfile (tmp,mailbox)) {
      sprintf (tmp,"Not a Bezerk-format mailbox: %s",mailbox);
      mm_log (tmp,ERROR);
    }
    else bezerk_badname (tmp,mailbox);
    return NIL;
  }
				/* open the destination */
  if (!mailboxfile (tmp,mailbox) ||
      (fd = open (tmp,O_BINARY|O_WRONLY|O_APPEND|O_CREAT,
		  S_IREAD|S_IWRITE)) < 0) {
    sprintf (tmp,"Unable to open copy mailbox: %s",strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }

  mm_critical (stream);		/* go critical */
  fstat (fd,&sbuf);		/* get current file size */
				/* for each requested message */
  for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence) {
      lseek (LOCAL->fd,elt->private.special.offset,SEEK_SET);
				/* number of bytes to copy */
      j = elt->private.msg.full.offset + elt->rfc822_size;
      do {			/* read from source position */
	k = min (j,(unsigned long) MAILTMPLEN);
	read (LOCAL->fd,tmp,(unsigned int) k);
	if (write (fd,tmp,(unsigned int) k) < 0) {
	  sprintf (tmp,"Unable to write message: %s",strerror (errno));
	  mm_log (tmp,ERROR);
	  chsize (fd,sbuf.st_size);
	  close (fd);		/* punt */
	  mm_nocritical (stream);
	  return NIL;
	}
      } while (j -= k);		/* until done */
    }
  close (fd);			/* close the file */
  mm_nocritical (stream);	/* release critical */
				/* delete all requested messages */
  if (options & CP_MOVE) for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence) elt->deleted = T;
  return T;
}

/* Berkeley mail append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    append callback
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

#define BUFLEN MAILTMPLEN

long bezerk_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  struct stat sbuf;
  int fd;
  unsigned long i,j;
  char *flags,*date,buf[BUFLEN],tmp[MAILTMPLEN],file[MAILTMPLEN];
  FILE *sf,*df;
  MESSAGECACHE elt;
  STRING *message;
  long ret = LONGT;
				/* default stream to prototype */
  if (!stream) stream = &bezerkproto;
				/* make sure valid mailbox */
  if (!bezerk_isvalid (mailbox,tmp) && errno) {
    if (errno == ENOENT) {
      if (((mailbox[0] == 'I') || (mailbox[0] == 'i')) &&
	  ((mailbox[1] == 'N') || (mailbox[1] == 'n')) &&
	  ((mailbox[2] == 'B') || (mailbox[2] == 'b')) &&
	  ((mailbox[3] == 'O') || (mailbox[3] == 'o')) &&
	  ((mailbox[4] == 'X') || (mailbox[4] == 'x')) && !mailbox[5])
	bezerk_create (NIL,"INBOX");
      else {
	mm_notify (stream,"[TRYCREATE] Must create mailbox before append",NIL);
	return NIL;
      }
    }
    else if (mailboxfile (tmp,mailbox)) {
      sprintf (tmp,"Not a Bezerk-format mailbox: %.80ss",mailbox);
      mm_log (tmp,ERROR);
    }
    else bezerk_badname (tmp,mailbox);
    return NIL;
  }
  tzset ();			/* initialize timezone stuff */
				/* get first message */
  if (!(*af) (stream,data,&flags,&date,&message)) return NIL;
  if (!(sf = tmpfile ())) {	/* must have scratch file */
    sprintf (tmp,"Unable to create scratch file: %.80s",strerror (errno));
    mm_log (tmp,ERROR);
  }

  do {				/* parse date */
    if (!date) rfc822_date (date = tmp);
    if (!mail_parse_date (&elt,date)) {
      sprintf (tmp,"Bad date in append: %.80s",date);
      mm_log (tmp,ERROR);
    }
    else {			/* user wants to suppress time zones? */
      if (mail_parameters (NIL,GET_NOTIMEZONES,NIL)) {
	time_t when = mail_longdate (&elt);
	date = ctime (&when);	/* use traditional date */
      }
				/* use POSIX-style date */
      else date = mail_cdate (tmp,&elt);
      if (!SIZE (message)) mm_log ("Append of zero-length message",ERROR);
      else if (!bezerk_append_msg (stream,sf,flags,date,message)) {
	sprintf (tmp,"Error writing scratch file: %.80s",strerror (errno));
	mm_log (tmp,ERROR);
      }
				/* get next message */
      else if ((*af) (stream,data,&flags,&date,&message)) continue;
    }
    fclose (sf);		/* punt scratch file */
    return NIL;			/* give up */
  } while (message);		/* until no more messages */
  if (fflush (sf) || fstat (fileno (sf),&sbuf)) {
    sprintf (tmp,"Error finishing scratch file: %.80s",strerror (errno));
    mm_log (tmp,ERROR);
    fclose (sf);		/* punt scratch file */
    return NIL;			/* give up */
  }
  i = sbuf.st_size;		/* size of scratch file */

  mm_critical (stream);		/* go critical */
				/* open the destination */
  if (!mailboxfile (tmp,mailbox) || 
      ((fd = open (tmp,O_BINARY|O_WRONLY|O_APPEND|O_CREAT,
		   S_IREAD|S_IWRITE)) < 0) ||
      !(df = fdopen (fd,"ab"))) {
    mm_nocritical (stream);	/* done with critical */
    sprintf (tmp,"Can't open append mailbox: %s",strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
  fstat (fd,&sbuf);		/* get current file size */
  while (i)			/* until written all bytes */
    if ((j = fread (buf,1,min ((long) BUFLEN,i),sf)) &&
	(fwrite (buf,1,j,df) == j)) i -= j;
  fclose (sf);			/* done with scratch file */
				/* make sure append wins */
  if (i || (fflush (df) == EOF)) {
    chsize (fd,sbuf.st_size);	/* revert file */
    close (fd);			/* make sure fclose() doesn't corrupt us */
    sprintf (buf,"Message append failed: %s",strerror (errno));
    mm_log (buf,ERROR);
    ret = NIL;			/* return error */
  }
  fclose (df);
  mm_nocritical (stream);	/* release critical */
  return ret;
}

/* Write single message to append scratch file
 * Accepts: MAIL stream
 *	    scratch file
 *	    flags
 *	    message stringstruct
 * Returns: NIL if write error, else T
 */

int bezerk_append_msg (MAILSTREAM *stream,FILE *sf,char *flags,char *date,
		     STRING *msg)
{
  int c;
  unsigned long i,uf;
  char tmp[MAILTMPLEN];
  long f = mail_parse_flags (stream,flags,&uf);
				/* build initial header */
  if ((fprintf (sf,"From %s@%s %sStatus: ",
		myusername (),mylocalhost (),date) < 0) ||
      (f&fSEEN && (putc ('R',sf) == EOF)) ||
      (fputs ("\nX-Status: ",sf) == EOF) ||
      (f&fDELETED && (putc ('D',sf) == EOF)) ||
      (f&fFLAGGED && (putc ('F',sf) == EOF)) ||
      (f&fANSWERED && (putc ('A',sf) == EOF)) ||
      (f&fDRAFT && (putc ('T',sf) == EOF)) ||
      (fputs ("\nX-Keywords:",sf) == EOF)) return NIL;
  while (uf)			/* write user flags */
    if (fprintf (sf," %s",stream->user_flags[find_rightmost_bit (&uf)]) < 0)
      return NIL;
				/* tie off flags */
  if (putc ('\n',sf) == EOF) return NIL;
  while (SIZE (msg)) {		/* copy text to scratch file */
				/* possible delimiter if line starts with F */
    if ((c = 0xff & SNX (msg)) == 'F') {
				/* copy line to buffer */
      for (i = 1,tmp[0] = c; SIZE (msg) && (c != '\n') && (i < MAILTMPLEN);)
	if (((c = 0xff & SNX (msg)) != '\r') || !(SIZE (msg)) ||
	    (CHR (msg) != '\n')) tmp[i++] = c;
      if ((i > 4) && (tmp[1] == 'r') && (tmp[2] == 'o') && (tmp[3] == 'm') &&
	  (tmp[4] == ' ')) {	/* possible "From " line? */
				/* yes, see if need to write a widget */
	if (((c != '\n') || bezerk_valid_line (tmp,NIL,NIL)) &&
	    (putc ('>',sf) == EOF)) return NIL;
      }
				/* write buffered text */
      if (fwrite (tmp,1,i,sf) != i) return NIL;
      if (c == '\n') continue;	/* all done if got a complete line */
    }
				/* copy line, toss out CR from CRLF */
    do if (((c == '\r') && SIZE (msg) && ((c = 0xff & SNX (msg)) != '\n') &&
	    (putc ('\r',sf) == EOF)) || (putc (c,sf) == EOF)) return NIL;
    while ((c != '\n') && SIZE (msg) && ((c = 0xff & SNX (msg)) ? c : T));
  }
				/* write trailing newline and return */
  return (putc ('\n',sf) == EOF) ? NIL : T;
}


/* Return bad file name error message
 * Accepts: temporary buffer
 *	    file name
 * Returns: long NIL always
 */

long bezerk_badname (char *tmp,char *s)
{
  sprintf (tmp,"Invalid mailbox name: %s",s);
  mm_log (tmp,ERROR);
  return (long) NIL;
}

/* Parse mailbox
 * Accepts: MAIL stream
 * Returns: T if parse OK
 *	    NIL if failure, stream aborted
 */

long bezerk_parse (MAILSTREAM *stream)
{
  struct stat sbuf;
  MESSAGECACHE *elt;
  char *s,*t,tmp[MAILTMPLEN + 1],*db,datemsg[100];
  long i;
  int j,ti,zn;
  long curpos = LOCAL->filesize;
  long nmsgs = stream->nmsgs;
  long recent = stream->recent;
  short silent = stream->silent;
  fstat (LOCAL->fd,&sbuf);	/* get status */
  if (sbuf.st_size < curpos) {	/* sanity check */
    sprintf (tmp,"Mailbox shrank from %ld to %ld!",curpos,sbuf.st_size);
    mm_log (tmp,ERROR);
    bezerk_close (stream,NIL);
    return NIL;
  }
  stream->silent = T;		/* don't pass up mm_exists() events yet */
  db = datemsg + strlen (strcpy (datemsg,"Unparsable date: "));
  while (sbuf.st_size - curpos){/* while there is data to read */
				/* get to that position in the file */
    lseek (LOCAL->fd,curpos,SEEK_SET);
				/* read first buffer's worth */
    read (LOCAL->fd,tmp,j = (int) min (i,(long) MAILTMPLEN));
    tmp[j] = '\0';		/* tie off buffer */
    if (!(ti = bezerk_valid_line (tmp,&t,&zn))) {
      mm_log ("Mailbox format invalidated (consult an expert), aborted",ERROR);
      bezerk_close (stream,NIL);
      return NIL;
    }

				/* swell the cache */
    mail_exists (stream,++nmsgs);
				/* instantiate an elt for this message */
    (elt = mail_elt (stream,nmsgs))->valid = T;
    elt->private.uid = ++stream->uid_last;
				/* note file offset of header */
    elt->private.special.offset = curpos;
				/* note offset of message */
    elt->private.msg.full.offset =
      (s = ((*t == '\015') ? (t + 2) : (t + 1))) - tmp;
				/* generate plausable IMAPish date string */
    db[2] = db[6] = db[20] = '-'; db[11] = ' '; db[14] = db[17] = ':';
				/* dd */
    db[0] = t[ti - 2]; db[1] = t[ti - 1];
				/* mmm */
    db[3] = t[ti - 6]; db[4] = t[ti - 5]; db[5] = t[ti - 4];
				/* hh */
    db[12] = t[ti + 1]; db[13] = t[ti + 2];
				/* mm */
    db[15] = t[ti + 4]; db[16] = t[ti + 5];
    if (t[ti += 6] == ':') {	/* ss if present */
      db[18] = t[++ti]; db[19] = t[++ti];
      ti++;			/* move to space */
    }
    else db[18] = db[19] = '0';	/* assume 0 seconds */
				/* yy -- advance over timezone if necessary */
    if (++zn == ++ti) ti += (((t[zn] == '+') || (t[zn] == '-')) ? 6 : 4);
    db[7] = t[ti]; db[8] = t[ti + 1]; db[9] = t[ti + 2]; db[10] = t[ti + 3];
    t = zn ? (t + zn) : "LCL";	/* zzz */
    db[21] = *t++; db[22] = *t++; db[23] = *t++;
    if ((db[21] != '+') && (db[21] != '-')) db[24] = '\0';
    else {			/* numeric time zone */
      db[20] = ' '; db[24] = *t++; db[25] = *t++; db[26] = '\0';
    }
				/* set internal date */
    if (!mail_parse_date (elt,db)) mm_log (datemsg,WARN);

    curpos += s - tmp;		/* advance position after header */
    t = strchr (s,'\012');	/* start of next line */
				/* find start of next message */
    while (!(bezerk_valid_line (s,NIL,NIL))) {
      if (t) {			/* have next line? */
	t++;			/* advance to new line */
	curpos += t - s;	/* update position and size */
	elt->rfc822_size += ((t - s) + ((t[-2] == '\015') ? 0 : 1));
	s = t;			/* move to next line */
	t = strchr (s,'\012');
      }
      else {			/* try next buffer */
	j = strlen (s);		/* length of unread data in buffer */
	if ((i = sbuf.st_size - curpos) && (i != j)) {
				/* get to that position in the file */
	  lseek (LOCAL->fd,curpos,SEEK_SET);
				/* read another buffer's worth */
	  read (LOCAL->fd,s = tmp,j = (int) min (i,(long) MAILTMPLEN));
	  tmp[j] = '\0';	/* tie off buffer */
	  if (!(t = strchr (s,'\012'))) fatal ("Line too long in mailbox");
	}
	else {
	  curpos += j;		/* last bit of data */
	  elt->rfc822_size += j;
	  break;
	}
      }
    }
  }
				/* update parsed file size */
  LOCAL->filesize = sbuf.st_size;
  stream->silent = silent;	/* can pass up events now */
  mail_exists (stream,nmsgs);	/* notify upper level of new mailbox size */
  mail_recent (stream,recent);	/* and of change in recent messages */
  return T;			/* return the winnage */
}

/* Berkeley locate header for a message
 * Accepts: MAIL stream
 *	    message number
 *	    pointer to returned header size
 * Returns: position of header in file
 */

unsigned long bezerk_hdrpos (MAILSTREAM *stream,unsigned long msgno,
			     unsigned long *size)
{
  long siz;
  size_t i = 0;
  char c = '\0';
  char *s;
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt = mail_elt (stream,msgno);
  long pos = elt->private.special.offset + elt->private.msg.full.offset;
				/* is size known? */
  if (!(*size = elt->private.msg.header.text.size)) {
				/* get to header position */
    lseek (LOCAL->fd,pos,SEEK_SET);
				/* search message for CRLF CRLF */
    for (siz = 1; siz <= elt->rfc822_size; siz++) {
      if (!i &&			/* buffer empty? */
	  (read (LOCAL->fd,s = tmp,
		 i = (size_t) min(elt->rfc822_size-siz,(long)MAILTMPLEN))<= 0))
	return pos;
      else i--;
				/* two newline sequence? */
      if ((c == '\012') && (*s == '\012')) {
				/* yes, note for later */
	elt->private.msg.header.text.size = (*size = siz);
	return pos;		/* return to caller */
      }
      else if ((c == '\012') && (*s == '\015')) {
				/* yes, note for later */
	elt->private.msg.header.text.size = (*size = siz + 1);
	return pos;		/* return to caller */
      }
      else c = *s++;		/* next character */
    }
  }
  return pos;			/* have position */
}
