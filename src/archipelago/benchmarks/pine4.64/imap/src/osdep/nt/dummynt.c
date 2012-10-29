/*
 * Program:	Dummy routines for NT
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	24 May 1993
 * Last Edited:	26 August 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <direct.h>
#include "mail.h"
#include "osdep.h"
#include <sys\stat.h>
#include <dos.h>
#include "dummy.h"
#include "misc.h"

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
  char *s,*t,tmp[MAILTMPLEN];
  struct stat sbuf;
				/* must be valid local mailbox */
  if (name && *name && (*name != '{') && (s = mailboxfile (tmp,name))) {
				/* indeterminate INBOX */
    if (!*s) return &dummydriver;
				/* remove trailing \ */
    if ((t = strrchr (s,'\\')) && !t[1]) *t = '\0';
    if (!stat (s,&sbuf)) switch (sbuf.st_mode & S_IFMT) {
    case S_IFREG:		/* file */
    case S_IFDIR:		/* future use */
      return &dummydriver;
    }
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
  return NIL;
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
  long i = 0;
  if (!pat || !*pat) {		/* empty pattern? */
    if (dummy_canonicalize (test,ref,"*")) {
				/* tie off name at root */
      if (s = strchr (test,'\\')) *++s = '\0';
      else test[0] = '\0';
      dummy_listed (stream,'\\',test,LATT_NOSELECT,NIL);
    }
  }
				/* get canonical form of name */
  else if (dummy_canonicalize (test,ref,pat)) {
				/* found any wildcards? */
    if (s = strpbrk (test,"%*")) {
				/* yes, copy name up to that point */
      strncpy (file,test,(size_t) (i = s - test));
      file[i] = '\0';		/* tie off */
    }
    else strcpy (file,test);	/* use just that name then */
				/* find directory name */
    if (s = strrchr (file,'\\')) {
      *++s = '\0';		/* found, tie off at that point */
      s = file;
    }
				/* silly case */
    else if (file[0] == '#') s = file;
				/* do the work */
    dummy_list_work (stream,s,test,contents,0);
    if (pmatch ("INBOX",test))	/* always an INBOX */
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
  char *s,*t,test[MAILTMPLEN];
  int showuppers = pat[strlen (pat) - 1] == '%';
				/* get canonical form of name */
  if (dummy_canonicalize (test,ref,pat) && (s = sm_read (&sdb))) do
    if (*s != '{') {
      if (pmatch_full (s,test,'\\')) {
	if (pmatch (s,"INBOX")) mm_lsub (stream,NIL,s,LATT_NOINFERIORS);
	else mm_lsub (stream,'\\',s,NIL);
      }
      else while (showuppers && (t = strrchr (s,'\\'))) {
	*t = '\0';		/* tie off the name */
	if (pmatch_full (s,test,'\\')) mm_lsub (stream,'\\',s,LATT_NOSELECT);
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
  if ((s = mailboxfile (tmp,mailbox)) && *s && !stat (s,&sbuf) &&
      ((sbuf.st_mode & S_IFMT) == S_IFREG)) return sm_subscribe (mailbox);
  sprintf (tmp,"Can't subscribe %.80s: not a mailbox",mailbox);
  mm_log (tmp,ERROR);
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
  struct _finddata_t f;
  struct stat sbuf;
  long fhandle;
  char tmp[MAILTMPLEN];
				/* punt if bogus name */
  if (!mailboxdir (tmp,dir,NIL)) return;
				/* make directory wildcard */
  strcat (tmp,(tmp[strlen (tmp) -1] == '\\') ? "*.*" : "\\*.*");
				/* do nothing if can't open directory */
  if ((fhandle = _findfirst (tmp,&f)) >= 0) {  
				/* list it if not at top-level */
    if (!level && dir && pmatch_full (dir,pat,'\\'))
      dummy_listed (stream,'\\',dir,LATT_NOSELECT,contents);
				/* scan directory */
    if (!dir || dir[strlen (dir) -1] == '\\') do
      if (((f.name[0] != '.') ||
	   (f.name[1] && ((f.name[1] != '.') || f.name[2]))) &&
	  (strlen (f.name) <= NETMAXMBX)) {
				/* see if name is useful */
	if (dir) sprintf (tmp,"%s%s",dir,f.name);
	else strcpy (tmp,f.name);
				/* make sure useful and can get info */
	if ((pmatch_full (tmp,pat,'\\') ||
	     pmatch_full (strcat (tmp,"\\"),pat,'\\') ||
	     dmatch (tmp,pat,'\\')) &&
	    mailboxdir (tmp,dir,f.name) && tmp[0] && !stat (tmp,&sbuf)) {
				/* now make name we'd return */
	  if (dir) sprintf (tmp,"%s%s",dir,f.name);
	  else strcpy (tmp,f.name);
				/* only interested in file type */
	  switch (sbuf.st_mode & S_IFMT) {
	  case S_IFDIR:		/* directory? */
	    if (pmatch_full (tmp,pat,'\\')) {
	      if (!dummy_listed (stream,'\\',tmp,LATT_NOSELECT,contents))break;
	      strcat (tmp,"\\");/* set up for dmatch call */
	    }
				/* try again with trailing \ */
	    else if (pmatch_full (strcat (tmp,"\\"),pat,'\\') &&
		     !dummy_listed (stream,'\\',tmp,LATT_NOSELECT,contents))
	      break;
	    if (dmatch (tmp,pat,'\\') &&
		(level < (long) mail_parameters (NIL,GET_LISTMAXLEVEL,NIL)))
	      dummy_list_work (stream,tmp,pat,contents,level+1);
	    break;
	  case S_IFREG:		/* ordinary name */
	    if (pmatch_full (tmp,pat,'\\') && !pmatch ("INBOX",tmp))
	      dummy_listed (stream,'\\',tmp,LATT_NOINFERIORS,contents);
	    break;
	  }
	}
      }
    while (!_findnext (fhandle,&f));
    _findclose(fhandle);
  }
}

/* Mailbox found
 * Accepts: hierarchy delimiter
 *	    mailbox name
 *	    attributes
 *	    contents to search before calling mm_list()
 * Returns: T, always
 */

#define BUFSIZE 4*MAILTMPLEN

long dummy_listed (MAILSTREAM *stream,char delimiter,char *name,
		   long attributes,char *contents)
{
  struct stat sbuf;
  int fd;
  long csiz,ssiz,bsiz;
  char *s,*buf,tmp[MAILTMPLEN];
  if (contents) {		/* want to search contents? */
				/* forget it if can't select or open */
    if ((attributes & LATT_NOSELECT) || !(csiz = strlen (contents)) ||
	!(s = dummy_file (tmp,name)) || stat (s,&sbuf) ||
	(csiz > sbuf.st_size) || ((fd = open (tmp,O_RDONLY,NIL)) < 0))
      return T;
				/* get buffer including slop */    
    buf = (char *) fs_get (BUFSIZE + (ssiz = 4 * ((csiz / 4) + 1)) + 1);
    memset (buf,'\0',ssiz);	/* no slop area the first time */
    while (sbuf.st_size) {	/* until end of file */
      read (fd,buf+ssiz,bsiz = min (sbuf.st_size,BUFSIZE));
      if (search ((unsigned char *) buf,bsiz+ssiz,
		  (unsigned char *) contents,csiz)) break;
      memcpy (buf,buf+BUFSIZE,ssiz);
      sbuf.st_size -= bsiz;	/* note that we read that much */
    }
    fs_give ((void **) &buf);	/* flush buffer */
    close (fd);			/* finished with file */
    if (!sbuf.st_size) return T;/* not found */
  }
				/* notify main program */
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
  char tmp[MAILTMPLEN];
  if (compare_cstring (mailbox,"INBOX") && dummy_file (tmp,mailbox))
    return dummy_create_path (stream,tmp,NIL);
  sprintf (tmp,"Can't create %.80s: invalid name",mailbox);
  mm_log (tmp,ERROR);
  return NIL;
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
  char *t = strrchr (path,'\\');
  char *pt = (path[1] == ':') ? path + 2 : path;
  int wantdir = t && !t[1];
  if (wantdir) *t = '\0';	/* flush trailing delimiter for directory */
				/* found superior to this name? */
  if ((s = strrchr (pt,'\\')) && (s != pt)) {
    strncpy (tmp,path,(size_t) (s - path));
    tmp[s - path] = '\0';	/* make directory name for stat */
    c = *++s;			/* tie off in case need to recurse */
    *s = '\0';
				/* name doesn't exist, create it */
    if ((stat (tmp,&sbuf) || ((sbuf.st_mode & S_IFMT) != S_IFDIR)) &&
	!dummy_create_path (stream,path,dirmode)) return NIL;
    *s = c;			/* restore full name */
  }
  if (wantdir) {		/* want to create directory? */
    ret = !mkdir (path);
    *t = '\\';			/* restore directory delimiter */
  }
				/* create file */
  else if ((fd = open (path,O_WRONLY|O_CREAT|O_EXCL,S_IREAD|S_IWRITE)) >= 0)
    ret = !close (fd);		/* close file */
  if (!ret) {			/* error? */
    sprintf (tmp,"Can't create mailbox node %.80s: %.80s",path,
	     strerror (errno));
    mm_log (tmp,ERROR);
  }
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
    mm_log (tmp,ERROR);
  }
				/* no trailing \ */
  if ((s = strrchr (tmp,'\\')) && !s[1]) *s = '\0';
  if (stat (tmp,&sbuf) || ((sbuf.st_mode & S_IFMT) == S_IFDIR) ?
      rmdir (tmp) : unlink (tmp)) {
    sprintf (tmp,"Can't delete mailbox %.80s: %.80s",mailbox,strerror (errno));
    mm_log (tmp,ERROR);
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
  long ret = NIL;
				/* no trailing \ allowed */
  if (!dummy_file (oldname,old) || !(s = dummy_file (mbx,newname)) ||
      ((s = strrchr (s,'\\')) && !s[1])) {
    sprintf (mbx,"Can't rename %.80s to %.80s: invalid name",old,newname);
    mm_log (mbx,ERROR);
    return NIL;
  }
				/* found superior to destination name? */
  if (s && (s != mbx) && ((mbx[1] != ':') || (s != mbx + 2))) {
    c = s[1];			/* remember character after delimiter */
    *s = s[1] = '\0';		/* tie off name at delimiter */
				/* name doesn't exist, create it */
    if (stat (mbx,&sbuf) || ((sbuf.st_mode & S_IFMT) != S_IFDIR)) {
      *s = '\\';		/* restore delimiter */
      if (!dummy_create (stream,mbx)) return NIL;
    }
    else *s = '\\';		/* restore delimiter */
    s[1] = c;			/* restore character after delimiter */
  }
				/* rename of non-ex INBOX creates dest */
  if (!compare_cstring (old,"INBOX") && stat (oldname,&sbuf))
    return dummy_create (NIL,mbx);
  if (rename (oldname,mbx)) {
    sprintf (tmp,"Can't rename mailbox %.80s to %.80s: %.80s",old,newname,
	     strerror (errno));
    mm_log (tmp,ERROR);
    return NIL;
  }
  return LONGT;			/* return success */
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
    if (sbuf.st_size)		/* bogus format if non-empty */
      sprintf (err,"%.80s (file %.80s) is not in valid mailbox format",
	       stream->mailbox,tmp);
  }
  if (err[0]) {			/* if an error happened */
    mm_log (err,stream->silent ? WARN : ERROR);
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
				/* time to do another test? */
  if (time (0) >= ((time_t) (stream->gensym + 30))) {
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
      mm_notify (stream,"[TRYCREATE] Must create mailbox before append",
		 (long) NIL);
    sprintf (tmp,"%.80s: %.80s",strerror (e),mailbox);
    mm_log (tmp,ERROR);		/* pass up error */
    return NIL;			/* always fails */
  }
  if (fd >= 0) {		/* found file? */
    fstat (fd,&sbuf);		/* get its size */
    close (fd);			/* toss out the fd */
    if (sbuf.st_size) ts = NIL;	/* non-empty file? */
  }
  if (ts) return (*ts->dtb->append) (stream,mailbox,af,data);
  sprintf (tmp,"Indeterminate mailbox format: %.80s",mailbox);
  mm_log (tmp,ERROR);
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
  char dev[4];
				/* initially no device */
  dev[0] = dev[1] = dev[2] = dev[3] = '\0';
  if (ref) switch (*ref) {	/* preliminary reference check */
  case '{':			/* remote names not allowed */
    return NIL;			/* disallowed */
  case '\0':			/* empty reference string */
    break;
  default:			/* all other names */
    if (ref[1] == ':') {	/* start with device name? */
      dev[0] = *ref++; dev[1] = *ref++;
    }
    break;
  }
  if (pat[1] == ':') {		/* device name in pattern? */
    dev[0] = *pat++; dev[1] = *pat++;
    ref = NIL;			/* ignore reference */
  }
  switch (*pat) {
  case '#':			/* namespace names */
    if (mailboxfile (tmp,pat)) strcpy (tmp,pat);
    else return NIL;		/* unknown namespace */
    break;
  case '{':			/* remote names not allowed */
    return NIL;
  case '\\':			/* rooted name */
    ref = NIL;			/* ignore reference */
    break;
  }
				/* make sure device names are rooted */
  if (dev[0] && (*(ref ? ref : pat) != '\\')) dev[2] = '\\';
				/* build name */
  sprintf (tmp,"%s%s%s",dev,ref ? ref : "",pat);
  ucase (tmp);			/* force upper case */
  return T;
}
