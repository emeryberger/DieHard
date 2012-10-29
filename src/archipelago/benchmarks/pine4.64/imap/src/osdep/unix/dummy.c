/*
 * Program:	Dummy routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	9 May 1991
 * Last Edited:	10 November 2004
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
#include "dummy.h"
#include "misc.h"
#include "mx.h"			/* highly unfortunate */

/* Function prototypes */

DRIVER *dummy_valid (char *name);
void *dummy_parameters (long function,void *value);
void dummy_list_work (MAILSTREAM *stream,char *dir,char *pat,char *contents,
		      long level);
long dummy_listed (MAILSTREAM *stream,char delimiter,char *name,
		   long attributes,char *contents);
long dummy_subscribe (MAILSTREAM *stream,char *mailbox);
MAILSTREAM *dummy_open (MAILSTREAM *stream);
void dummy_close (MAILSTREAM *stream,long options);
long dummy_ping (MAILSTREAM *stream);
void dummy_check (MAILSTREAM *stream);
void dummy_expunge (MAILSTREAM *stream);
long dummy_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options);
long dummy_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data);

/* Dummy routines */


/* Driver dispatch used by MAIL */

DRIVER dummydriver = {
  "dummy",			/* driver name */
  DR_LOCAL|DR_MAIL,		/* driver flags */
  (DRIVER *) NIL,		/* next driver */
  dummy_valid,			/* mailbox is valid for us */
  dummy_parameters,		/* manipulate parameters */
  dummy_scan,			/* scan mailboxes */
  dummy_list,			/* list mailboxes */
  dummy_lsub,			/* list subscribed mailboxes */
  dummy_subscribe,		/* subscribe to mailbox */
  NIL,				/* unsubscribe from mailbox */
  dummy_create,			/* create mailbox */
  dummy_delete,			/* delete mailbox */
  dummy_rename,			/* rename mailbox */
  mail_status_default,		/* status of mailbox */
  dummy_open,			/* open mailbox */
  dummy_close,			/* close mailbox */
  NIL,				/* fetch message "fast" attributes */
  NIL,				/* fetch message flags */
  NIL,				/* fetch overview */
  NIL,				/* fetch message structure */
  NIL,				/* fetch header */
  NIL,				/* fetch text */
  NIL,				/* fetch message data */
  NIL,				/* unique identifier */
  NIL,				/* message number from UID */
  NIL,				/* modify flags */
  NIL,				/* per-message modify flags */
  NIL,				/* search for message based on criteria */
  NIL,				/* sort messages */
  NIL,				/* thread messages */
  dummy_ping,			/* ping mailbox to see if still alive */
  dummy_check,			/* check for new messages */
  dummy_expunge,		/* expunge deleted messages */
  dummy_copy,			/* copy messages to another mailbox */
  dummy_append,			/* append string message to mailbox */
  NIL				/* garbage collect stream */
};

				/* prototype stream */
MAILSTREAM dummyproto = {&dummydriver};

/* Dummy validate mailbox
 * Accepts: mailbox name
 * Returns: our driver if name is valid, NIL otherwise
 */

DRIVER *dummy_valid (char *name)
{
  char *s,tmp[MAILTMPLEN];
  struct stat sbuf;
				/* must be valid local mailbox */
  if (name && *name && (*name != '{') && (s = mailboxfile (tmp,name))) {
				/* indeterminate clearbox INBOX */
    if (!*s) return &dummydriver;
    else if (!stat (s,&sbuf)) switch (sbuf.st_mode & S_IFMT) {
    case S_IFREG:
    case S_IFDIR:
      return &dummydriver;
    }
				/* blackbox INBOX does not exist yet */
    else if (!compare_cstring (name,"INBOX")) return &dummydriver;
  }
  return NIL;
}


/* Dummy manipulate driver parameters
 * Accepts: function code
 *	    function-dependent value
 * Returns: function-dependent return value
 */

void *dummy_parameters (long function,void *value)
{
  void *ret = NIL;
  switch ((int) function) {
  case GET_INBOXPATH:
    if (value) ret = dummy_file ((char *) value,"INBOX");
    break;
  }
  return ret;
}

/* Dummy scan mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 *	    string to scan
 */

void dummy_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents)
{
  char *s,test[MAILTMPLEN],file[MAILTMPLEN];
  long i;
  if (!pat || !*pat) {		/* empty pattern? */
    if (dummy_canonicalize (test,ref,"*")) {
				/* tie off name at root */
      if (s = strchr (test,'/')) *++s = '\0';
      else test[0] = '\0';
      dummy_listed (stream,'/',test,LATT_NOSELECT,NIL);
    }
  }
				/* get canonical form of name */
  else if (dummy_canonicalize (test,ref,pat)) {
				/* found any wildcards? */
    if (s = strpbrk (test,"%*")) {
				/* yes, copy name up to that point */
      strncpy (file,test,i = s - test);
      file[i] = '\0';		/* tie off */
    }
    else strcpy (file,test);	/* use just that name then */
    if (s = strrchr (file,'/')){/* find directory name */
      *++s = '\0';		/* found, tie off at that point */
      s = file;
    }
				/* silly case */
    else if ((file[0] == '~') || (file[0] == '#')) s = file;
				/* do the work */
    dummy_list_work (stream,s,test,contents,0);
				/* always an INBOX */
    if (pmatch ("INBOX",ucase (test)))
      dummy_listed (stream,NIL,"INBOX",LATT_NOINFERIORS,contents);
  }
}


/* Dummy list mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void dummy_list (MAILSTREAM *stream,char *ref,char *pat)
{
  dummy_scan (stream,ref,pat,NIL);
}

/* Dummy list subscribed mailboxes
 * Accepts: mail stream
 *	    reference
 *	    pattern to search
 */

void dummy_lsub (MAILSTREAM *stream,char *ref,char *pat)
{
  void *sdb = NIL;
  char *s,*t,test[MAILTMPLEN],tmp[MAILTMPLEN];
  int showuppers = pat[strlen (pat) - 1] == '%';
				/* get canonical form of name */
  if (dummy_canonicalize (test,ref,pat) && (s = sm_read (&sdb))) do
    if (*s != '{') {
      if (!compare_cstring (s,"INBOX") &&
	  pmatch ("INBOX",ucase (strcpy (tmp,test))))
	mm_lsub (stream,NIL,s,LATT_NOINFERIORS);
      else if (pmatch_full (s,test,'/')) mm_lsub (stream,'/',s,NIL);
      else while (showuppers && (t = strrchr (s,'/'))) {
	*t = '\0';		/* tie off the name */
	if (pmatch_full (s,test,'/')) mm_lsub (stream,'/',s,LATT_NOSELECT);
      }
    }
  while (s = sm_read (&sdb));	/* until no more subscriptions */
}


/* Dummy subscribe to mailbox
 * Accepts: mail stream
 *	    mailbox to add to subscription list
 * Returns: T on success, NIL on failure
 */

long dummy_subscribe (MAILSTREAM *stream,char *mailbox)
{
  char *s,tmp[MAILTMPLEN];
  struct stat sbuf;
				/* must be valid local mailbox */
  if ((s = mailboxfile (tmp,mailbox)) && *s && !stat (s,&sbuf)
#if 0	/* disable this temporarily for Netscape */
      &&
      ((sbuf.st_mode & S_IFMT) == S_IFREG)
#endif
      ) return sm_subscribe (mailbox);
  sprintf (tmp,"Can't subscribe %.80s: not a mailbox",mailbox);
  MM_LOG (tmp,ERROR);
  return NIL;
}

/* Dummy list mailboxes worker routine
 * Accepts: mail stream
 *	    directory name to search
 *	    search pattern
 *	    string to scan
 *	    search level
 */

void dummy_list_work (MAILSTREAM *stream,char *dir,char *pat,char *contents,
		      long level)
{
  DIR *dp;
  struct direct *d;
  struct stat sbuf;
  int ismx;
  char tmp[MAILTMPLEN];
				/* punt if bogus name */
  if (!mailboxdir (tmp,dir,NIL)) return;
  if (dp = opendir (tmp)) {	/* do nothing if can't open directory */
				/* list it if not at top-level */
    if (!level && dir && pmatch_full (dir,pat,'/'))
      dummy_listed (stream,'/',dir,LATT_NOSELECT,contents);
				/* scan directory, ignore . and .. */
    ismx = (!stat (strcat (tmp,MXINDEXNAME),&sbuf) &&
	    ((sbuf.st_mode & S_IFMT) == S_IFREG));
    if (!dir || dir[strlen (dir) - 1] == '/') while (d = readdir (dp))
      if (((d->d_name[0] != '.') ||
	   (((int) mail_parameters (NIL,GET_HIDEDOTFILES,NIL)) ? NIL :
	    (d->d_name[1] && (((d->d_name[1] != '.') || d->d_name[2]) &&
			      strcmp (d->d_name+1,MXINDEXNAME+2))))) &&
	  (strlen (d->d_name) <= NETMAXMBX)) {
				/* see if name is useful */
	if (dir) sprintf (tmp,"%s%s",dir,d->d_name);
	else strcpy (tmp,d->d_name);
				/* make sure useful and can get info */
	if ((pmatch_full (tmp,pat,'/') ||
	     pmatch_full (strcat (tmp,"/"),pat,'/') || dmatch (tmp,pat,'/')) &&
	    mailboxdir (tmp,dir,d->d_name) && tmp[0] && !stat (tmp,&sbuf)) {
				/* now make name we'd return */
	  if (dir) sprintf (tmp,"%s%s",dir,d->d_name);
	  else strcpy (tmp,d->d_name);
				/* only interested in file type */
	  switch (sbuf.st_mode & S_IFMT) {
	  case S_IFDIR:		/* directory? */
	    if (pmatch_full (tmp,pat,'/')) {
	      if (!dummy_listed (stream,'/',tmp,LATT_NOSELECT,contents)) break;
	      strcat (tmp,"/");	/* set up for dmatch call */
	    }
				/* try again with trailing / */
	    else if (pmatch_full (strcat (tmp,"/"),pat,'/') &&
		     !dummy_listed (stream,'/',tmp,LATT_NOSELECT,contents))
	      break;
	    if (dmatch (tmp,pat,'/') &&
		(level < (long) mail_parameters (NIL,GET_LISTMAXLEVEL,NIL)))
	      dummy_list_work (stream,tmp,pat,contents,level+1);
	    break;
	  case S_IFREG:		/* ordinary name */
				/* ignore all-digit names from mx */
	    /* Must use ctime for systems that don't update mtime properly */
	    if (!(ismx && mx_select (d)) && pmatch_full (tmp,pat,'/') &&
		compare_cstring (tmp,"INBOX"))
	      dummy_listed (stream,'/',tmp,LATT_NOINFERIORS +
			    ((sbuf.st_size && (sbuf.st_atime < sbuf.st_ctime))?
			     LATT_MARKED : LATT_UNMARKED),contents);
	    break;
	  }
	}
      }
    closedir (dp);		/* all done, flush directory */
  }
}

/* Scan file for contents
 * Accepts: file name
 *	    desired contents
 * Returns: NIL if contents not found, T if found
 */

#define BUFSIZE 4*MAILTMPLEN

long dummy_scan_contents (char *name,char *contents,unsigned long csiz,
			  unsigned long fsiz)
{
  int fd;
  unsigned long ssiz,bsiz;
  char *buf;
				/* forget it if can't select or open */
  if ((fd = open (name,O_RDONLY,NIL)) >= 0) {
				/* get buffer including slop */    
    buf = (char *) fs_get (BUFSIZE + (ssiz = 4 * ((csiz / 4) + 1)) + 1);
    memset (buf,'\0',ssiz);	/* no slop area the first time */
    while (fsiz) {		/* until end of file */
      read (fd,buf+ssiz,bsiz = min (fsiz,BUFSIZE));
      if (search ((unsigned char *) buf,bsiz+ssiz,
		  (unsigned char *) contents,csiz)) break;
      memcpy (buf,buf+BUFSIZE,ssiz);
      fsiz -= bsiz;		/* note that we read that much */
    }
    fs_give ((void **) &buf);	/* flush buffer */
    close (fd);			/* finished with file */
    if (fsiz) return T;		/* found */
  }
  return NIL;			/* not found */
}


/* Mailbox found
 * Accepts: MAIL stream
 *	    hierarchy delimiter
 *	    mailbox name
 *	    attributes
 *	    contents to search before calling mm_list()
 * Returns: NIL if should abort hierarchy search, else T (currently always)
 */

long dummy_listed (MAILSTREAM *stream,char delimiter,char *name,
		   long attributes,char *contents)
{
  DRIVER *d = NIL;
  unsigned long csiz;
  struct stat sbuf;
  char *s,tmp[MAILTMPLEN];
				/* don't \NoSelect dir if it has a driver */
  if ((attributes & LATT_NOSELECT) && (d = mail_valid (NIL,name,NIL)) &&
      (d != &dummydriver)) attributes &= ~LATT_NOSELECT;
  if (!contents ||		/* notify main program */
      (!(attributes & LATT_NOSELECT) && (csiz = strlen (contents)) &&
       (s = mailboxfile (tmp,name)) &&
       (*s || (s = mail_parameters (NIL,GET_INBOXPATH,tmp))) &&
       !stat (s,&sbuf) && (csiz <= sbuf.st_size) &&
       SAFE_SCAN_CONTENTS (d,tmp,contents,csiz,sbuf.st_size)))
    mm_list (stream,delimiter,name,attributes);
  return T;
}

/* Dummy create mailbox
 * Accepts: mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long dummy_create (MAILSTREAM *stream,char *mailbox)
{
  char *s,tmp[MAILTMPLEN];
  long ret = NIL;
				/* validate name */
  if (!(compare_cstring (mailbox,"INBOX") && (s = dummy_file (tmp,mailbox)))) {
    sprintf (tmp,"Can't create %.80s: invalid name",mailbox);
    MM_LOG (tmp,ERROR);
  }
				/* create the name, done if made directory */
  else if ((ret = dummy_create_path (stream,tmp,get_dir_protection(mailbox)))&&
	   (s = strrchr (s,'/')) && !s[1]) return T;
  return ret ? set_mbx_protections (mailbox,tmp) : NIL;
}

/* Dummy create path
 * Accepts: mail stream
 *	    path name to create
 *	    directory mode
 * Returns: T on success, NIL on failure
 */

long dummy_create_path (MAILSTREAM *stream,char *path,long dirmode)
{
  struct stat sbuf;
  char c,*s,tmp[MAILTMPLEN];
  int fd;
  long ret = NIL;
  char *t = strrchr (path,'/');
  int wantdir = t && !t[1];
  int mask = umask (0);
  if (wantdir) *t = '\0';	/* flush trailing delimiter for directory */
  if (s = strrchr (path,'/')) {	/* found superior to this name? */
    c = *++s;			/* remember first character of inferior */
    *s = '\0';			/* tie off to get just superior */
				/* name doesn't exist, create it */
    if ((stat (path,&sbuf) || ((sbuf.st_mode & S_IFMT) != S_IFDIR)) &&
	!dummy_create_path (stream,path,dirmode)) {
      umask (mask);		/* restore mask */
      return NIL;
    }
    *s = c;			/* restore full name */
  }
  if (wantdir) {		/* want to create directory? */
    ret = !mkdir (path,(int) dirmode);
    *t = '/';			/* restore directory delimiter */
  }
				/* create file */
  else if ((fd = open (path,O_WRONLY|O_CREAT|O_EXCL,
		       (int) mail_parameters(NIL,GET_MBXPROTECTION,NIL))) >= 0)
    ret = !close (fd);
  if (!ret) {			/* error? */
    sprintf (tmp,"Can't create mailbox node %.80s: %.80s",path,strerror (errno));
    MM_LOG (tmp,ERROR);
  }
  umask (mask);			/* restore mask */
  return ret;			/* return status */
}

/* Dummy delete mailbox
 * Accepts: mail stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long dummy_delete (MAILSTREAM *stream,char *mailbox)
{
  struct stat sbuf;
  char *s,tmp[MAILTMPLEN];
  if (!(s = dummy_file (tmp,mailbox))) {
    sprintf (tmp,"Can't delete - invalid name: %.80s",s);
    MM_LOG (tmp,ERROR);
  }
				/* no trailing / (workaround BSD kernel bug) */
  if ((s = strrchr (tmp,'/')) && !s[1]) *s = '\0';
  if (stat (tmp,&sbuf) || ((sbuf.st_mode & S_IFMT) == S_IFDIR) ?
      rmdir (tmp) : unlink (tmp)) {
    sprintf (tmp,"Can't delete mailbox %.80s: %.80s",mailbox,strerror (errno));
    MM_LOG (tmp,ERROR);
    return NIL;
  }
  return T;			/* return success */
}

/* Mail rename mailbox
 * Accepts: mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long dummy_rename (MAILSTREAM *stream,char *old,char *newname)
{
  struct stat sbuf;
  char c,*s,tmp[MAILTMPLEN],mbx[MAILTMPLEN],oldname[MAILTMPLEN];
				/* no trailing / allowed */
  if (!dummy_file (oldname,old) || !(s = dummy_file (mbx,newname)) ||
      ((s = strrchr (s,'/')) && !s[1])) {
    sprintf (mbx,"Can't rename %.80s to %.80s: invalid name",old,newname);
    MM_LOG (mbx,ERROR);
    return NIL;
  }
  if (s) {			/* found superior to destination name? */
    c = *++s;			/* remember first character of inferior */
    *s = '\0';			/* tie off to get just superior */
				/* name doesn't exist, create it */
    if ((stat (mbx,&sbuf) || ((sbuf.st_mode & S_IFMT) != S_IFDIR)) &&
	!dummy_create (stream,mbx)) return NIL;
    *s = c;			/* restore full name */
  }
				/* rename of non-ex INBOX creates dest */
  if (!compare_cstring (old,"INBOX") && stat (oldname,&sbuf))
    return dummy_create (NIL,mbx);
  if (rename (oldname,mbx)) {
    sprintf (tmp,"Can't rename mailbox %.80s to %.80s: %.80s",old,newname,
	     strerror (errno));
    MM_LOG (tmp,ERROR);
    return NIL;
  }
  return T;			/* return success */
}

/* Dummy open
 * Accepts: stream to open
 * Returns: stream on success, NIL on failure
 */

MAILSTREAM *dummy_open (MAILSTREAM *stream)
{
  int fd;
  char err[MAILTMPLEN],tmp[MAILTMPLEN];
  struct stat sbuf;
				/* OP_PROTOTYPE call */
  if (!stream) return &dummyproto;
  err[0] = '\0';		/* no error message yet */
				/* can we open the file? */
  if (!dummy_file (tmp,stream->mailbox))
    sprintf (err,"Can't open this name: %.80s",stream->mailbox);
  else if ((fd = open (tmp,O_RDONLY,NIL)) < 0) {
				/* no, error unless INBOX */
    if (compare_cstring (stream->mailbox,"INBOX"))
      sprintf (err,"%.80s: %.80s",strerror (errno),stream->mailbox);
  }
  else {			/* file had better be empty then */
    fstat (fd,&sbuf);		/* sniff at its size */
    close (fd);
    if ((sbuf.st_mode & S_IFMT) != S_IFREG)
      sprintf (err,"Can't open %.80s: not a selectable mailbox",
	       stream->mailbox);
    else if (sbuf.st_size)	/* bogus format if non-empty */
      sprintf (err,"Can't open %.80s (file %.80s): not in valid mailbox format",
	       stream->mailbox,tmp);
  }
  if (err[0]) {			/* if an error happened */
    MM_LOG (err,stream->silent ? WARN : ERROR);
    return NIL;
  }
  else if (!stream->silent) {	/* only if silence not requested */
    mail_exists (stream,0);	/* say there are 0 messages */
    mail_recent (stream,0);	/* and certainly no recent ones! */
    stream->uid_validity = time (0);
  }
  stream->inbox = T;		/* note that it's an INBOX */
  return stream;		/* return success */
}


/* Dummy close
 * Accepts: MAIL stream
 *	    options
 */

void dummy_close (MAILSTREAM *stream,long options)
{
				/* return silently */
}

/* Dummy ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long dummy_ping (MAILSTREAM *stream)
{
  MAILSTREAM *test;
  if (time (0) >=		/* time to do another test? */
      ((time_t) (stream->gensym +
		 (long) mail_parameters (NIL,GET_SNARFINTERVAL,NIL)))) {
				/* has mailbox format changed? */
    if ((test = mail_open (NIL,stream->mailbox,OP_PROTOTYPE)) &&
	(test->dtb != stream->dtb) &&
	(test = mail_open (NIL,stream->mailbox,NIL))) {
				/* preserve some resources */
      test->original_mailbox = stream->original_mailbox;
      stream->original_mailbox = NIL;
      test->sparep = stream->sparep;
      stream->sparep = NIL;
      test->sequence = stream->sequence;
      mail_close ((MAILSTREAM *) /* flush resources used by dummy stream */
		  memcpy (fs_get (sizeof (MAILSTREAM)),stream,
			  sizeof (MAILSTREAM)));
				/* swap the streams */
      memcpy (stream,test,sizeof (MAILSTREAM));
      fs_give ((void **) &test);/* flush test now that copied */
				/* make sure application knows */
      mail_exists (stream,stream->recent = stream->nmsgs);
    }
				/* still hasn't changed */
    else stream->gensym = time (0);
  }
  return T;
}


/* Dummy check mailbox
 * Accepts: MAIL stream
 * No-op for readonly files, since read/writer can expunge it from under us!
 */

void dummy_check (MAILSTREAM *stream)
{
  dummy_ping (stream);		/* invoke ping */
}


/* Dummy expunge mailbox
 * Accepts: MAIL stream
 */

void dummy_expunge (MAILSTREAM *stream)
{
				/* return silently */
}

/* Dummy copy message(s)
 * Accepts: MAIL stream
 *	    sequence
 *	    destination mailbox
 *	    options
 * Returns: T if copy successful, else NIL
 */

long dummy_copy (MAILSTREAM *stream,char *sequence,char *mailbox,long options)
{
  if ((options & CP_UID) ? mail_uid_sequence (stream,sequence) :
      mail_sequence (stream,sequence)) fatal ("Impossible dummy_copy");
  return NIL;
}


/* Dummy append message string
 * Accepts: mail stream
 *	    destination mailbox
 *	    append callback function
 *	    data for callback
 * Returns: T on success, NIL on failure
 */

long dummy_append (MAILSTREAM *stream,char *mailbox,append_t af,void *data)
{
  struct stat sbuf;
  int fd = -1;
  int e;
  char tmp[MAILTMPLEN];
  MAILSTREAM *ts = default_proto (T);
  if (compare_cstring (mailbox,"INBOX") && dummy_file (tmp,mailbox) &&
      ((fd = open (tmp,O_RDONLY,NIL)) < 0)) {
    if ((e = errno) == ENOENT)	/* failed, was it no such file? */
      MM_NOTIFY (stream,"[TRYCREATE] Must create mailbox before append",NIL);
    sprintf (tmp,"%.80s: %.80s",strerror (e),mailbox);
    MM_LOG (tmp,ERROR);		/* pass up error */
    return NIL;			/* always fails */
  }
  if (fd >= 0) {		/* found file? */
    fstat (fd,&sbuf);		/* get its size */
    close (fd);			/* toss out the fd */
    if (sbuf.st_size) ts = NIL;	/* non-empty file? */
  }
  if (ts) return (*ts->dtb->append) (stream,mailbox,af,data);
  sprintf (tmp,"Indeterminate mailbox format: %.80s",mailbox);
  MM_LOG (tmp,ERROR);
  return NIL;
}

/* Dummy mail generate file string
 * Accepts: temporary buffer to write into
 *	    mailbox name string
 * Returns: local file string or NIL if failure
 */

char *dummy_file (char *dst,char *name)
{
  char *s = mailboxfile (dst,name);
				/* return our standard inbox */
  return (s && !*s) ? strcpy (dst,sysinbox ()) : s;
}


/* Dummy canonicalize name
 * Accepts: buffer to write name
 *	    reference
 *	    pattern
 * Returns: T if success, NIL if failure
 */

long dummy_canonicalize (char *tmp,char *ref,char *pat)
{
  if (ref) {			/* preliminary reference check */
    if (*ref == '{') return NIL;/* remote reference not allowed */
    else if (!*ref) ref = NIL;	/* treat empty reference as no reference */
  }
  switch (*pat) {
  case '#':			/* namespace name */
    if (mailboxfile (tmp,pat)) strcpy (tmp,pat);
    else return NIL;		/* unknown namespace */
    break;
  case '{':			/* remote names not allowed */
    return NIL;
  case '/':			/* rooted name */
  case '~':			/* home directory name */
    if (!ref || (*ref != '#')) {/* non-namespace reference? */
      strcpy (tmp,pat);		/* yes, ignore */
      break;
    }
				/* fall through */
  default:			/* apply reference for all other names */
    if (!ref) strcpy (tmp,pat);	/* just copy if no namespace */
    else if ((*ref != '#') || mailboxfile (tmp,ref)) {
				/* wants root of name? */
      if (*pat == '/') strcpy (strchr (strcpy (tmp,ref),'/'),pat);
				/* otherwise just append */
      else sprintf (tmp,"%s%s",ref,pat);
    }
    else return NIL;		/* unknown namespace */
  }
  return T;
}
