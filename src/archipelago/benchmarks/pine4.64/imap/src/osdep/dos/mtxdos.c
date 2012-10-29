/*
 * Program:	MTX mail routines
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
 * Last Edited:	8 March 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include "mail.h"
#include "osdep.h"
#include <time.h>
#include <sys\stat.h>
#include <dos.h>
#include "rfc822.h"
#include "dummy.h"
#include "misc.h"
#include "fdstring.h"

/* Build parameters */

#define CHUNK 4096


/* MTX I/O stream local data */
	
typedef struct mtx_local {
  int fd;			/* file descriptor for I/O */
  off_t filesize;		/* file size parsed */
  unsigned char *buf;		/* temporary buffer */
} MTXLOCAL;


/* Drive-dependent data passed to init method */

typedef struct mtx_data {
  int fd;			/* file data */
  unsigned long pos;		/* initial position */
} MTXDATA;


/* Convenient access to local data */

#define LOCAL ((MTXLOCAL *) stream->local)


/* Function prototypes */

DRIVER *mtx_valid (char *name);
long mtx_isvalid (char *name,char *tmp);
void *mtx_parameters (long function,void *value);
void mtx_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void mtx_list (MAILSTREAM *stream,char *ref,char *pat);
void mtx_lsub (MAILSTREAM *stream,char *ref,char *pat);
long mtx_create (MAILSTREAM *stream,char *mailbox);
long mtx_delete (MAILSTREAM *stream,char *mailbox);
long mtx_rename (MAILSTREAM *stream,char *old,char *newname);
MAILSTREAM *mtx_open (MAILSTREAM *stream);
void mtx_close (MAILSTREAM *stream,long options);
char *mtx_header (MAILSTREAM *stream,unsigned long msgno,
		  unsigned long *length,long flags);
long mtx_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags);
void mtx_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt);
long mtx_ping (MAILSTREAM *stream);
void mtx_check (MAILSTREAM *stream);
void mtx_expunge (MAILSTREAM *stream);
long mtx_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options);
long mtx_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);

char *mtx_file (char *dst,char *name);
long mtx_badname (char *tmp,char *s);
long mtx_parse (MAILSTREAM *stream);
void mtx_update_status (MAILSTREAM *stream,unsigned long msgno);
unsigned long mtx_hdrpos (MAILSTREAM *stream,unsigned long msgno,
			  unsigned long *size);

/* MTX mail routines */


/* Driver dispatch used by MAIL */

DRIVER mtxdriver = {
  "mtx",			/* driver name */
				/* driver flags */
  DR_LOCAL|DR_MAIL|DR_LOWMEM|DR_CRLF|DR_NOSTICKY,
  (DRIVER *) NIL,		/* next driver */
  mtx_valid,			/* mailbox is valid for us */
  mtx_parameters,		/* manipulate parameters */
  mtx_scan,			/* scan mailboxes */
  mtx_list,			/* list mailboxes */
  mtx_lsub,			/* list subscribed mailboxes */
  NIL,				/* subscribe to mailbox */
  NIL,				/* unsubscribe from mailbox */
  mtx_create,			/* create mailbox */
  mtx_delete,			/* delete mailbox */
  mtx_rename,			/* rename mailbox */
  mail_status_default,		/* status of mailbox */
  mtx_open,			/* open mailbox */
  mtx_close,			/* close mailbox */
  NIL,				/* fetch message "fast" attributes */
  NIL,				/* fetch message flags */
  NIL,				/* fetch overview */
  NIL,				/* fetch message envelopes */
  mtx_header,			/* fetch message header */
  mtx_text,			/* fetch message body */
  NIL,				/* fetch partial message text */
  NIL,				/* unique identifier */
  NIL,				/* message number */
  NIL,				/* modify flags */
  mtx_flagmsg,			/* per-message modify flags */
  NIL,				/* search for message based on criteria */
  NIL,				/* sort messages */
  NIL,				/* thread messages */
  mtx_ping,			/* ping mailbox to see if still alive */
  mtx_check,			/* check for new messages */
  mtx_expunge,			/* expunge deleted messages */
  mtx_copy,			/* copy messages to another mailbox */
  mtx_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM mtxproto = {&mtxdriver};

/* MTX mail validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *mtx_valid (char *name)
{
  char tmp[MAILTMPLEN];
  return mtx_isvalid (name,tmp) ? &mtxdriver : (DRIVER *) NIL;
}


/* MTX mail test for valid mailbox
 * Accepts: mailbox name
 * Returns: T if valid, NIL otherwise
 */

long mtx_isvalid (char *name,char *tmp)
{
  int fd;
  long ret = NIL;
  char *s;
  struct stat sbuf;
  errno = EINVAL;		/* assume invalid argument */
				/* if file, get its status */
  if ((*name != '{') && mailboxfile (tmp,name) && !stat (tmp,&sbuf)) {
    if (!sbuf.st_size)errno = 0;/* empty file */
    else if ((fd = open (tmp,O_BINARY|O_RDONLY,NIL)) >= 0) {
      memset (tmp,'\0',MAILTMPLEN);
      if ((read (fd,tmp,64) >= 0) && (s = strchr (tmp,'\015')) &&
	  (s[1] == '\012')) {	/* valid format? */
	*s = '\0';		/* tie off header */
				/* must begin with dd-mmm-yy" */
	ret = (((tmp[2] == '-' && tmp[6] == '-') ||
		(tmp[1] == '-' && tmp[5] == '-')) &&
	       (s = strchr (tmp+18,',')) && strchr (s+2,';')) ? T : NIL;
      }
      else errno = -1;		/* bogus format */
      close (fd);		/* close the file */
    }
  }
				/* in case INBOX but not mtx format */
  else if ((errno == ENOENT) && ((name[0] == 'I') || (name[0] == 'i')) &&
	   ((name[1] == 'N') || (name[1] == 'n')) &&
	   ((name[2] == 'B') || (name[2] == 'b')) &&
	   ((name[3] == 'O') || (name[3] == 'o')) &&
	   ((name[4] == 'X') || (name[4] == 'x')) && !name[5]) errno = -1;
  return ret;			/* return what we should */
}


/* MTX manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *mtx_parameters (long function,void *value)
{
  return NIL;
}

/* MTX mail scan mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void mtx_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  if (stream) dummy_scan (NIL,ref,pat,contents);
}


/* MTX mail list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void mtx_list (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_list (stream,ref,pat);
}


/* MTX mail list subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void mtx_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  if (stream) dummy_lsub (stream,ref,pat);
}

/* MTX mail create mailbox
 * Accepts: MAIL stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long mtx_create (MAILSTREAM *stream,char *mailbox)
{
  return dummy_create (stream,mailbox);
}


/* MTX mail delete mailbox
 * Accepts: MAIL stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long mtx_delete (MAILSTREAM *stream,char *mailbox)
{
  return dummy_delete (stream,mailbox);
}


/* MTX mail rename mailbox
 * Accepts: MAIL stream
 *	    old mailbox name
 *	    new mailbox name (or NIL for delete)
 * Returns: T on success, NIL on failure
 */

long mtx_rename (MAILSTREAM *stream,char *old,char *newname)
{
  return dummy_rename (stream,old,newname);
}

/* MTX mail open
 * Accepts: stream to open
 * Returns: stream on success, NIL on failure
 */

MAILSTREAM *mtx_open (MAILSTREAM *stream)
{
  long i;
  int fd;
  char *s;
  char tmp[MAILTMPLEN];
				/* return prototype for OP_PROTOTYPE call */
  if (!stream) return &mtxproto;
  if (stream->local) fatal ("mtx recycle stream");
  if (!mailboxfile (tmp,stream->mailbox))
    return (MAILSTREAM *) mtx_badname (tmp,stream->mailbox);
				/* open, possibly creating INBOX */
  if (((fd = open (tmp,O_BINARY|(stream->rdonly ? O_RDONLY:O_RDWR),NIL)) < 0)&&
      (compare_cstring (stream->mailbox,"INBOX") ||
       ((fd = open (tmp,O_BINARY|O_RDWR|O_CREAT|O_EXCL,S_IREAD|S_IWRITE))<0))){
    sprintf (tmp,"Can't open mailbox: %s",strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
  stream->local = fs_get (sizeof (MTXLOCAL));
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
  if (!mtx_ping (stream)) return NIL;
  if (!stream->nmsgs) mm_log ("Mailbox is empty",(long) NIL);
  stream->perm_seen = stream->perm_deleted =
    stream->perm_flagged = stream->perm_answered = stream->perm_draft =
      stream->rdonly ? NIL : T;
  stream->perm_user_flags = stream->rdonly ? NIL : 0xffffffff;
  return stream;		/* return stream to caller */
}

/* MTX mail close
 * Accepts: MAIL stream
 *	    close options
 */

void mtx_close (MAILSTREAM *stream,long options)
{
  if (stream && LOCAL) {	/* only if a file is open */
    int silent = stream->silent;
    stream->silent = T;
    if (options & CL_EXPUNGE) mtx_expunge (stream);
    close (LOCAL->fd);		/* close the local file */
    if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
				/* nuke the local data */
    fs_give ((void **) &stream->local);
    stream->dtb = NIL;		/* log out the DTB */
  }
}

/* MTX mail fetch message header
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned header text length
 *	    option flags
 * Returns: message header in RFC822 format
 */

char *mtx_header (MAILSTREAM *stream,unsigned long msgno,unsigned long *length,
		  long flags)
{
  *length = 0;			/* default to empty */
  if (flags & FT_UID) return "";/* UID call "impossible" */
				/* get to header position */
  lseek (LOCAL->fd,mtx_hdrpos (stream,msgno,length),L_SET);
				/* is buffer big enough? */
  if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
  LOCAL->buf = (char *) fs_get ((size_t) *length + 1);
  LOCAL->buf[*length] = '\0';	/* tie off string */
				/* slurp the data */
  read (LOCAL->fd,LOCAL->buf,(size_t) *length);
  return LOCAL->buf;
}


/* MTX mail fetch message text (body only)
 * Accepts: MAIL stream
 *	    message # to fetch
 *	    pointer to returned header text length
 *	    option flags
 * Returns: T, always
 */

long mtx_text (MAILSTREAM *stream,unsigned long msgno,STRING *bs,long flags)
{
  MESSAGECACHE *elt;
  FDDATA d;
  unsigned long hdrsize,hdrpos;
				/* UID call "impossible" */
  if (flags & FT_UID) return NIL;
  elt = mail_elt (stream,msgno);/* if message not seen */
  if (elt->seen && !(flags & FT_PEEK)) {
    elt->seen = T;		/* mark message as seen */
				/* recalculate status */
    mtx_update_status (stream,msgno);
    mm_flags (stream,msgno);
  }
				/* get location of text data */
  hdrpos = mtx_hdrpos (stream,msgno,&hdrsize);
  d.fd = LOCAL->fd;		/* set initial stringstruct */
  d.pos = hdrpos + hdrsize;
				/* flush old buffer */
  if (LOCAL->buf) fs_give ((void **) &LOCAL->buf);
  d.chunk = LOCAL->buf = (char *) fs_get ((size_t) d.chunksize = CHUNK);
  INIT (bs,fd_string,(void *) &d,elt->rfc822_size - hdrsize);
  return T;			/* success */
}

/* MTX mail per-message modify flags
 * Accepts: MAIL stream
 *	    message cache element
 */

void mtx_flagmsg (MAILSTREAM *stream,MESSAGECACHE *elt)
{
				/* recalculate status */
  mtx_update_status (stream,elt->msgno);
}


/* MTX mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream still alive, NIL if not
 */

long mtx_ping (MAILSTREAM *stream)
{
				/* punt if stream no longer alive */
  if (!(stream && LOCAL)) return NIL;
				/* parse mailbox, punt if parse dies */
  return (mtx_parse (stream)) ? T : NIL;
}


/* MTX mail check mailbox (reparses status too)
 * Accepts: MAIL stream
 */

void mtx_check (MAILSTREAM *stream)
{
  unsigned long i = 1;
  if (mtx_ping (stream)) {	/* ping mailbox */
				/* get new message status */
    while (i <= stream->nmsgs) mail_elt (stream,i++);
    mm_log ("Check completed",(long) NIL);
  }
}

/* MTX mail expunge mailbox
 * Accepts: MAIL stream
 */

void mtx_expunge (MAILSTREAM *stream)
{
  unsigned long i = 1;
  unsigned long j,k,m,recent;
  unsigned long n = 0;
  unsigned long delta = 0;
  MESSAGECACHE *elt;
  char tmp[MAILTMPLEN];
				/* do nothing if stream dead */
  if (!mtx_ping (stream)) return;
  if (stream->rdonly) {		/* won't do on readonly files! */
    mm_log ("Expunge ignored on readonly mailbox",WARN);
    return;
  }
  mm_critical (stream);		/* go critical */
  recent = stream->recent;	/* get recent now that pinged */ 
  while (i <= stream->nmsgs) {	/* for each message */
    elt = mail_elt (stream,i);	/* get cache element */
				/* number of bytes to smash or preserve */
    k = elt->private.special.text.size + elt->rfc822_size;
    if (elt->deleted) {		/* if deleted */
      if (elt->recent) --recent;/* if recent, note one less recent message */
      delta += k;		/* number of bytes to delete */
      mail_expunged (stream,i);	/* notify upper levels */
      n++;			/* count up one more deleted message */
    }
    else if (i++ && delta) {	/* preserved message */
				/* first byte to preserve */
      j = elt->private.special.offset;
      do {			/* read from source position */
	m = min (k,(unsigned long) MAILTMPLEN);
	lseek (LOCAL->fd,j,SEEK_SET);
	read (LOCAL->fd,tmp,(size_t) m);
				/* write to destination position */
	lseek (LOCAL->fd,j - delta,SEEK_SET);
	write (LOCAL->fd,tmp,(size_t) m);
	j += m;			/* next chunk, perhaps */
      } while (k -= m);		/* until done */
      elt->private.special.offset -= delta;
    }
  }
  if (n) {			/* truncate file after last message */
    chsize (LOCAL->fd,LOCAL->filesize -= delta);
    sprintf (tmp,"Expunged %ld messages",n);
    mm_log (tmp,(long) NIL);	/* output the news */
  }
  else mm_log ("No messages deleted, so no update needed",(long) NIL);
  mm_nocritical (stream);	/* release critical */
				/* notify upper level of new mailbox size */
  mail_exists (stream,stream->nmsgs);
  mail_recent (stream,recent);
}

/* MTX mail copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    copy options
 * Returns: T if success, NIL if failed
 */

long mtx_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  char tmp[MAILTMPLEN];
  struct stat sbuf;
  MESSAGECACHE *elt;
  unsigned long i,j,k;
  long ret = LONGT;
  int fd;
  mailproxycopy_t pc =
    (mailproxycopy_t) mail_parameters (stream,GET_MAILPROXYCOPY,NIL);
  if (!((options & CP_UID) ? mail_uid_sequence (stream,sequence) :
	mail_sequence (stream,sequence))) return NIL;
  if (!mtx_isvalid (mailbox,tmp)) switch (errno) {
  case ENOENT:			/* no such file? */
    mm_notify (stream,"[TRYCREATE] Must create mailbox before append",
	       (long) NIL);
    return NIL;
  case 0:			/* merely empty file? */
    break;
  case EINVAL:			/* name is bogus */
    if (pc) return (*pc) (stream,sequence,mailbox,options);
    return mtx_badname (tmp,mailbox);
  default:			/* file exists, but not valid format */
    if (pc) return (*pc) (stream,sequence,mailbox,options);
    sprintf (tmp,"Not a MTX-format mailbox: %s",mailbox);
    mm_log (tmp,ERROR);
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
  for (i = 1; ret && (i <= stream->nmsgs); i++)
    if ((elt = mail_elt (stream,i))->sequence) {
      lseek (LOCAL->fd,elt->private.special.offset,SEEK_SET);
				/* number of bytes to copy */
      k = elt->private.special.text.size + elt->rfc822_size;
      do {			/* read from source position */
	j = min (k,(long) MAILTMPLEN);
	read (LOCAL->fd,tmp,(size_t) j);
	if (write (fd,tmp,(size_t) j) < 0) {
	  sprintf (tmp,"Unable to write message: %s",strerror (errno));
	  mm_log (tmp,ERROR);
	  chsize (fd,sbuf.st_size);
	  j = k;
	  ret = NIL;		/* note error */
	  break;
	}
      } while (k -= j);		/* until done */
    }
  close (fd);			/* close the file */
  mm_nocritical (stream);	/* release critical */
				/* delete all requested messages */
  if (ret && (options & CP_MOVE)) for (i = 1; i <= stream->nmsgs; i++)
    if ((elt = mail_elt (stream,i))->sequence) {
      elt->deleted = T;		/* mark message deleted */
				/* recalculate status */
      mtx_update_status (stream,i);
    }
  return ret;
}

/* MTX mail append message from stringstruct
 * Accepts: MAIL stream
 *	    destination mailbox
 *	    append callback
 *	    data for callback
 * Returns: T if append successful, else NIL
 */

long mtx_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  struct stat sbuf;
  int fd,ld,c;
  char *flags,*date,tmp[MAILTMPLEN],file[MAILTMPLEN];
  FILE *df;
  MESSAGECACHE elt;
  long f;
  unsigned long i,uf;
  STRING *message;
  long ret = LONGT;
				/* default stream to prototype */
  if (!stream) stream = &mtxproto;
				/* make sure valid mailbox */
  if (!mtx_isvalid (mailbox,tmp)) switch (errno) {
  case ENOENT:			/* no such file? */
    if (((mailbox[0] == 'I') || (mailbox[0] == 'i')) &&
	((mailbox[1] == 'N') || (mailbox[1] == 'n')) &&
	((mailbox[2] == 'B') || (mailbox[2] == 'b')) &&
	((mailbox[3] == 'O') || (mailbox[3] == 'o')) &&
	((mailbox[4] == 'X') || (mailbox[4] == 'x')) && !mailbox[5])
      dummy_create (NIL,"INBOX.MTX");
    else {
      mm_notify (stream,"[TRYCREATE] Must create mailbox before append",NIL);
      return NIL;
    }
				/* falls through */
  case 0:			/* merely empty file? */
    break;
  case EINVAL:
    return mtx_badname (tmp,mailbox);
  default:
    sprintf (tmp,"Not a MTX-format mailbox: %.80s",mailbox);
    mm_log (tmp,ERROR);
    return NIL;
  }
				/* get first message */
  if (!(*af) (stream,data,&flags,&date,&message)) return NIL;
				/* open destination mailbox */
  if (!mailboxfile (file,mailbox) ||
      ((fd = open (file,O_BINARY|O_WRONLY|O_APPEND|O_CREAT,
		   S_IREAD|S_IWRITE)) < 0) || !(df = fdopen (fd,"ab"))) {
    sprintf (tmp,"Can't open append mailbox: %s",strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
  mm_critical (stream);		/* go critical */
  fstat (fd,&sbuf);		/* get current file size */

  errno = 0;
  do {				/* parse flags */
    if (!SIZE (message)) {	/* guard against zero-length */
      mm_log ("Append of zero-length message",ERROR);
      ret = NIL;
      break;
    }
    f = mail_parse_flags (stream,flags,&i);
				/* reverse bits (dontcha wish we had CIRC?) */
    for (uf = 0; i; uf |= 1 << (29 - find_rightmost_bit (&i)));
    if (date) {			/* parse date if given */
      if (!mail_parse_date (&elt,date)) {
	sprintf (tmp,"Bad date in append: %.80s",date);
	mm_log (tmp,ERROR);
	ret = NIL;		/* mark failure */
	break;
      }
      mail_date (tmp,&elt);	/* write preseved date */
    }
    else internal_date (tmp);	/* get current date in IMAP format */
				/* write header */
    if (fprintf (df,"%s,%lu;%010lo%02lo\015\012",tmp,i = SIZE (message),uf,
		 (unsigned long) f) < 0) ret = NIL;
    else {			/* write message */
      if (i) do c = 0xff & SNX (message);
      while ((putc (c,df) != EOF) && --i);
				/* get next message */
      if (i || !(*af) (stream,data,&flags,&date,&message)) ret = NIL;
    }
  } while (ret && message);
				/* revert file if failure */
  if (!ret || (fflush (df) == EOF)) {
    chsize (fd,sbuf.st_size);	/* revert file */
    close (fd);			/* make sure fclose() doesn't corrupt us */
    if (errno) {
      sprintf (tmp,"Message append failed: %s",strerror (errno));
      mm_log (tmp,ERROR);
    }
    ret = NIL;
  }
  fclose (df);			/* close the file */ 
  mm_nocritical (stream);	/* release critical */
  return ret;
}


/* Return bad file name error message
 * Accepts: temporary buffer
 *	    file name
 * Returns: long NIL always
 */

long mtx_badname (char *tmp,char *s)
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

long mtx_parse (MAILSTREAM *stream)
{
  struct stat sbuf;
  MESSAGECACHE *elt = NIL;
  unsigned char *s,*t,*x,lbuf[65];
  char tmp[MAILTMPLEN];
  long i;
  long curpos = LOCAL->filesize;
  long nmsgs = stream->nmsgs;
  long recent = stream->recent;
  fstat (LOCAL->fd,&sbuf);	/* get status */
  if (sbuf.st_size < curpos) {	/* sanity check */
    sprintf (tmp,"Mailbox shrank from %ld to %ld!",curpos,sbuf.st_size);
    mm_log (tmp,ERROR);
    mtx_close (stream,NIL);
    return NIL;
  }
				/* while there is stuff to parse */
  while (i = sbuf.st_size - curpos) {
				/* get to that position in the file */
    lseek (LOCAL->fd,curpos,SEEK_SET);
    if ((i = read (LOCAL->fd,lbuf,64)) <= 0) {
      sprintf (tmp,"Unable to read internal header at %ld, size = %ld: %s",
	       curpos,sbuf.st_size,i ? strerror (errno) : "no data read");
      mm_log (tmp,ERROR);
      mtx_close (stream,NIL);
      return NIL;
    }
    lbuf[i] = '\0';		/* tie off buffer just in case */
    if (!((s = strchr (lbuf,'\015')) && (s[1] == '\012'))) {
      sprintf (tmp,"Unable to find end of line at %ld in %ld bytes, text: %s",
	       curpos,i,(char *) lbuf);
      mm_log (tmp,ERROR);
      mtx_close (stream,NIL);
      return NIL;
    }
    *s = '\0';			/* tie off header line */
    i = (s + 2) - lbuf;		/* note start of text offset */
    if (!((s = strchr (lbuf,',')) && (t = strchr (s+1,';')))) {
      sprintf (tmp,"Unable to parse internal header at %ld: %s",curpos,
	       (char *) lbuf);
      mm_log (tmp,ERROR);
      mtx_close (stream,NIL);
      return NIL;
    }

    *s++ = '\0'; *t++ = '\0';	/* tie off fields */
				/* intantiate an elt for this message */
    (elt = mail_elt (stream,++nmsgs))->valid = T;
    elt->private.uid = ++stream->uid_last;
				/* note file offset of header */
    elt->private.special.offset = curpos;
				/* as well as offset from header of message */
    elt->private.special.text.size = i;
				/* header size not known yet */
    elt->private.msg.header.text.size = 0;
				/* parse the header components */
    if (!(mail_parse_date (elt,lbuf) &&
	  (elt->rfc822_size = strtol (x = s,(char **) &s,10)) && (!(s && *s))&&
	  isdigit (t[0]) && isdigit (t[1]) && isdigit (t[2]) &&
	  isdigit (t[3]) && isdigit (t[4]) && isdigit (t[5]) &&
	  isdigit (t[6]) && isdigit (t[7]) && isdigit (t[8]) &&
	  isdigit (t[9]) && isdigit (t[10]) && isdigit (t[11]) && !t[12])) {
      sprintf (tmp,"Unable to parse internal header elements at %ld: %s,%s;%s",
	       curpos,(char *) lbuf,(char *) x,(char *) t);
      mtx_close (stream,NIL);
      return NIL;
    }
				/* update current position to next header */
    curpos += i + elt->rfc822_size;
				/* calculate system flags */
    if ((i = ((t[10]-'0') * 8) + t[11]-'0') & fSEEN) elt->seen = T;
    if (i & fDELETED) elt->deleted = T;
    if (i & fFLAGGED) elt->flagged = T;
    if (i & fANSWERED) elt->answered = T;
    if (i & fDRAFT) elt->draft = T;
    if (curpos > sbuf.st_size) {
      sprintf (tmp,"Last message (at %lu) runs past end of file (%lu > %lu)",
	       elt->private.special.offset,curpos,sbuf.st_size);
      mm_log (tmp,ERROR);
      mtx_close (stream,NIL);
      return NIL;
    }
  }
				/* update parsed file size */
  LOCAL->filesize = sbuf.st_size;
  mail_exists (stream,nmsgs);	/* notify upper level of new mailbox size */
  mail_recent (stream,recent);	/* and of change in recent messages */
  return T;			/* return the winnage */
}

/* Update status string
 * Accepts: MAIL stream
 *	    message number
 */

void mtx_update_status (MAILSTREAM *stream,unsigned long msgno)
{
  char tmp[MAILTMPLEN];
  MESSAGECACHE *elt = mail_elt (stream,msgno);
  unsigned long j,k = 0;
				/* not if readonly you don't */
  if (stream->rdonly || !elt->valid) return;
  j = elt->user_flags;		/* get user flags */
				/* reverse bits (dontcha wish we had CIRC?) */
  while (j) k |= 1 << 29 - find_rightmost_bit (&j);
  sprintf (tmp,"%010lo%02o",k,	/* print new flag string */
	   fOLD + (fSEEN * elt->seen) + (fDELETED * elt->deleted) +
	   (fFLAGGED * elt->flagged) + (fANSWERED * elt->answered) +
	   (fDRAFT * elt->draft));
				/* get to that place in the file */
  lseek (LOCAL->fd,(off_t) elt->private.special.offset +
	 elt->private.special.text.size - 14,SEEK_SET);
  write (LOCAL->fd,tmp,12);	/* write new flags */
}

/* MTX locate header for a message
 * Accepts: MAIL stream
 *	    message number
 *	    pointer to returned header size
 * Returns: position of header in file
 */

unsigned long mtx_hdrpos (MAILSTREAM *stream,unsigned long msgno,
			  unsigned long *size)
{
  unsigned long siz;
  size_t i = 0;
  int q = 0;
  char *s,tmp[MAILTMPLEN];
  MESSAGECACHE *elt = mail_elt (stream,msgno);
  long pos = elt->private.special.offset + elt->private.special.text.size;
				/* is header size known? */
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
      switch (q) {		/* sniff at buffer */
      case 0:			/* first character */
	q = (*s++ == '\015') ? 1 : 0;
	break;
      case 1:			/* second character */
	q = (*s++ == '\012') ? 2 : 0;
	break;
      case 2:			/* third character */
	q = (*s++ == '\015') ? 3 : 0;
	break;
      case 3:			/* fourth character */
	if (*s++ == '\012') {	/* have the sequence? */
				/* yes, note for later */
	  elt->private.msg.header.text.size = (*size = siz);
	  return pos;		/* return to caller */
	}
	q = 0;			/* lost... */
	break;
      }
    }
  }
  return pos;			/* have position */
}
