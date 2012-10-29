/*
 * Program:	Mail library test program
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	8 July 1988
 * Last Edited:	7 April 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 *
 * This original version of this file is
 * Copyright 1988 Stanford University
 * and was developed in the Symbolic Systems Resources Group of the Knowledge
 * Systems Laboratory at Stanford University in 1987-88, and was funded by the
 * Biomedical Research Technology Program of the NationalInstitutes of Health
 * under grant number RR-00785.
 */

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include "c-client.h"
#include "imap4r1.h"

/* Excellent reasons to hate ifdefs, and why my real code never uses them */

#ifndef unix
# define unix 0
#endif

#if unix
# define UNIXLIKE 1
# define MACOS 0
# include <pwd.h>
#else
# define UNIXLIKE 0
# ifdef noErr
#  define MACOS 1
#  include <Memory.h>
# else
#  define MACOS 0
# endif
#endif

char *curhst = NIL;		/* currently connected host */
char *curusr = NIL;		/* current login user */
char personalname[MAILTMPLEN];	/* user's personal name */

static char *hostlist[] = {	/* SMTP server host list */
  "mailhost",
  "localhost",
  NIL
};

static char *newslist[] = {	/* Netnews server host list */
  "news",
  NIL
};

int main (void);
void mm (MAILSTREAM *stream,long debug);
void overview_header (MAILSTREAM *stream,unsigned long uid,OVERVIEW *ov,
		      unsigned long msgno);
void header (MAILSTREAM *stream,long msgno);
void display_body (BODY *body,char *pfx,long i);
void status (MAILSTREAM *stream);
void prompt (char *msg,char *txt);
void smtptest (long debug);

/* Main program - initialization */

int main ()
{
  MAILSTREAM *stream = NIL;
  void *sdb = NIL;
  char *s,tmp[MAILTMPLEN];
  long debug;
#include "linkage.c"
#if MACOS
  {
    size_t *base = (size_t *) 0x000908;
				/* increase stack size on a Mac */
    SetApplLimit ((Ptr) (*base - (size_t) 65535L));
  }
#endif
  curusr = cpystr (((s = myusername ()) && *s) ? s : "somebody");
#if UNIXLIKE
  {
    char *suffix;
    struct passwd *pwd = getpwnam (curusr);
    if (pwd) {
      strcpy (tmp,pwd->pw_gecos);
				/* dyke out the office and phone poop */
      if (suffix = strchr (tmp,',')) suffix[0] = '\0';
      strcpy (personalname,tmp);/* make a permanent copy of it */
    }
    else personalname[0] = '\0';
  }
#else
  personalname[0] = '\0';
#endif
  curhst = cpystr (mylocalhost ());
  puts ("MTest -- C client test program");
  if (!*personalname) prompt ("Personal name: ",personalname);
				/* user wants protocol telemetry? */
  prompt ("Debug protocol (y/n)?",tmp);
  ucase (tmp);
  debug = (tmp[0] == 'Y') ? T : NIL;
  do {
    prompt ("Mailbox ('?' for help): ",tmp);
    if (!strcmp (tmp,"?")) {
      puts ("Enter INBOX, mailbox name, or IMAP mailbox as {host}mailbox");
      puts ("Known local mailboxes:");
      mail_list (NIL,NIL,"%");
      if (s = sm_read (&sdb)) {
	puts ("Local subscribed mailboxes:");
	do (mm_lsub (NIL,NIL,s,NIL));
	while (s = sm_read (&sdb));
      }
      puts ("or just hit return to quit");
    }
    else if (tmp[0]) stream = mail_open (stream,tmp,debug ? OP_DEBUG : NIL);
  } while (!stream && tmp[0]);
  mm (stream,debug);		/* run user interface if opened */
#if MACOS
				/* clean up resolver */
  if (resolveropen) CloseResolver ();
#endif
  return NIL;
}

/* MM command loop
 * Accepts: MAIL stream
 */

void mm (MAILSTREAM *stream,long debug)
{
  void *sdb = NIL;
  char cmd[MAILTMPLEN];
  char *s,*arg;
  unsigned long i;
  unsigned long last = 0;
  BODY *body;
  status (stream);		/* first report message status */
  while (stream) {
    prompt ("MTest>",cmd);	/* prompt user, get command */
				/* get argument */
    if (arg = strchr (cmd,' ')) *arg++ = '\0';
    switch (*ucase (cmd)) {	/* dispatch based on command */
    case 'B':			/* Body command */
      if (arg) last = atoi (arg);
      else if (!last) {
	puts ("?Missing message number");
	break;
      }
      if (last && (last <= stream->nmsgs)) {
	mail_fetchstructure (stream,last,&body);
	if (body) display_body (body,NIL,(long) 0);
	else puts ("%No body information available");
      }
      else puts ("?Bad message number");
      break;
    case 'C':			/* Check command */
      mail_check (stream);
      status (stream);
      break;
    case 'D':			/* Delete command */
      if (arg) last = atoi (arg);
      else {
	if (last == 0) {
	  puts ("?Missing message number");
	  break;
	}
	arg = cmd;
	sprintf (arg,"%lu",last);
      }
      if (last && (last <= stream->nmsgs))
	mail_setflag (stream,arg,"\\DELETED");
      else puts ("?Bad message number");
      break;
    case 'E':			/* Expunge command */
      mail_expunge (stream);
      last = 0;
      break;
    case 'F':			/* Find command */
      if (!arg) {
	arg = "%";
	if (s = sm_read (&sdb)) {
	  puts ("Local network subscribed mailboxes:");
	  do if (*s == '{') (mm_lsub (NIL,NIL,s,NIL));
	  while (s = sm_read (&sdb));
	}
      }
      puts ("Subscribed mailboxes:");
      mail_lsub (((arg[0] == '{') && (*stream->mailbox == '{')) ? stream : NIL,
		 NIL,arg);
      puts ("Known mailboxes:");
      mail_list (((arg[0] == '{') && (*stream->mailbox == '{')) ? stream : NIL,
		 NIL,arg);
      break;
    case 'G':
      mail_gc (stream,GC_ENV|GC_TEXTS|GC_ELT);
      break;
    case 'H':			/* Headers command */
      if (arg) {
	if (!(last = atoi (arg))) {
	  mail_search (stream,arg);
	  for (i = 1; i <= stream->nmsgs; ++i)
	    if (mail_elt (stream,i)->searched) header (stream,i);
	  break;
	}
      }
      else if (last == 0) {
	puts ("?Missing message number");
	break;
      }
      if (last && (last <= stream->nmsgs)) header (stream,last);
      else puts ("?Bad message number");
      break;
    case 'L':			/* Literal command */
      if (arg) last = atoi (arg);
      else if (!last) {
	puts ("?Missing message number");
	break;
      }
      if (last && (last <= stream->nmsgs))
	puts (mail_fetch_message (stream,last,NIL,NIL));
      else puts ("?Bad message number");
      break;
    case 'M':
      mail_status (NIL,arg ? arg : stream->mailbox,
		   SA_MESSAGES|SA_RECENT|SA_UNSEEN|SA_UIDNEXT|SA_UIDVALIDITY);
      break;
    case 'N':			/* New mailbox command */
      if (!arg) {
	puts ("?Missing mailbox");
	break;
      }
				/* get the new mailbox */
      while (!(stream = mail_open (stream,arg,debug))) {
	prompt ("Mailbox: ",arg);
	if (!arg[0]) break;
      }
      last = 0;
      status (stream);
      break;
    case 'O':			/* Overview command */
      if (!arg) {
	puts ("?Missing UID");
	break;
      }
      mail_fetch_overview (stream,arg,overview_header);
      break;
    case 'P':			/* Ping command */
      mail_ping (stream);
      status (stream);
      break;
    case 'Q':			/* Quit command */
      mail_close (stream);
      stream = NIL;
      break;
    case 'S':			/* Send command */
      smtptest (debug);
      break;
    case '\0':			/* null command (type next message) */
      if (!last || (last++ >= stream->nmsgs)) {
	puts ("%No next message");
	break;
      }
    case 'T':			/* Type command */
      if (arg) last = atoi (arg);
      else if (!last) {
	puts ("?Missing message number");
	break;
      }
      if (last && (last <= stream->nmsgs)) {
	STRINGLIST *lines = mail_newstringlist ();
	STRINGLIST *cur = lines;
	cur->text.size = strlen ((char *) (cur->text.data = (unsigned char *)
					   cpystr ("Date")));
	cur = cur->next = mail_newstringlist ();
	cur->text.size = strlen ((char *) (cur->text.data = (unsigned char *)
					   cpystr ("From")));
	cur = cur->next = mail_newstringlist ();
	cur->text.size = strlen ((char *) (cur->text.data = (unsigned char *)
					   cpystr (">From")));
	cur = cur->next = mail_newstringlist ();
	cur->text.size = strlen ((char *) (cur->text.data = (unsigned char *)
					   cpystr ("Subject")));
	cur = cur->next = mail_newstringlist ();
	cur->text.size = strlen ((char *) (cur->text.data = (unsigned char *)
					   cpystr ("To")));
	cur = cur->next = mail_newstringlist ();
	cur->text.size = strlen ((char *) (cur->text.data = (unsigned char *)
					   cpystr ("cc")));
	cur = cur->next = mail_newstringlist ();
	cur->text.size = strlen ((char *) (cur->text.data = (unsigned char *)
					   cpystr ("Newsgroups")));
	printf ("%s",mail_fetchheader_full (stream,last,lines,NIL,NIL));
	puts (mail_fetchtext (stream,last));
	mail_free_stringlist (&lines);
      }
      else puts ("?Bad message number");
      break;
    case 'U':			/* Undelete command */
      if (arg) last = atoi (arg);
      else {
	if (!last) {
	  puts ("?Missing message number");
	  break;
	}
	arg = cmd;
	sprintf (arg,"%lu",last);
      }
      if (last > 0 && last <= stream->nmsgs)
	mail_clearflag (stream,arg,"\\DELETED");
      else puts ("?Bad message number");
      break;
    case 'X':			/* Xit command */
      mail_expunge (stream);
      mail_close (stream);
      stream = NIL;
      break;
    case '+':
      mail_debug (stream); debug = T;
      break;
    case '-':
      mail_nodebug (stream); debug = NIL;
      break;
    case '?':			/* ? command */
      puts ("Body, Check, Delete, Expunge, Find, GC, Headers, Literal,");
      puts (" MailboxStatus, New Mailbox, Overview, Ping, Quit, Send, Type,");
      puts ("Undelete, Xit, +, -, or <RETURN> for next message");
      break;
    default:			/* bogus command */
      printf ("?Unrecognized command: %s\n",cmd);
      break;
    }
  }
}

/* MM display header
 * Accepts: IMAP2 stream
 *	    message number
 */

void overview_header (MAILSTREAM *stream,unsigned long uid,OVERVIEW *ov,
		      unsigned long msgno)
{
  if (ov) {
    unsigned long i;
    char *t,tmp[MAILTMPLEN];
    ADDRESS *adr;
    MESSAGECACHE *elt = mail_elt (stream,msgno);
    MESSAGECACHE selt;
    tmp[0] = elt->recent ? (elt->seen ? 'R': 'N') : ' ';
    tmp[1] = (elt->recent | elt->seen) ? ' ' : 'U';
    tmp[2] = elt->flagged ? 'F' : ' ';
    tmp[3] = elt->answered ? 'A' : ' ';
    tmp[4] = elt->deleted ? 'D' : ' ';
    mail_parse_date (&selt,ov->date);
    sprintf (tmp+5,"%4lu) ",elt->msgno);
    mail_date (tmp+11,&selt);
    tmp[17] = ' ';
    tmp[18] = '\0';
    memset (tmp+18,' ',(size_t) 20);
    tmp[38] = '\0';		/* tie off with null */
				/* get first from address from envelope */
    for (adr = ov->from; adr && !adr->host; adr = adr->next);
    if (adr) {			/* if a personal name exists use it */
      if (!(t = adr->personal))
	sprintf (t = tmp+400,"%s@%s",adr->mailbox,adr->host);
      memcpy (tmp+18,t,(size_t) min (20,(long) strlen (t)));
    }
    strcat (tmp," ");
    if (i = elt->user_flags) {
      strcat (tmp,"{");
      while (i) {
	strcat (tmp,stream->user_flags[find_rightmost_bit (&i)]);
	if (i) strcat (tmp," ");
      }
      strcat (tmp,"} ");
    }
    sprintf (tmp + strlen (tmp),"%.25s (%lu chars)",
	     ov->subject ? ov->subject : " ",ov->optional.octets);
    puts (tmp);
  }
  else printf ("%%No overview for UID %lu\n",uid);
}

/* MM display header
 * Accepts: IMAP2 stream
 *	    message number
 */

void header (MAILSTREAM *stream,long msgno)
{
  unsigned long i;
  char tmp[MAILTMPLEN];
  char *t;
  MESSAGECACHE *cache = mail_elt (stream,msgno);
  mail_fetchstructure (stream,msgno,NIL);
  tmp[0] = cache->recent ? (cache->seen ? 'R': 'N') : ' ';
  tmp[1] = (cache->recent | cache->seen) ? ' ' : 'U';
  tmp[2] = cache->flagged ? 'F' : ' ';
  tmp[3] = cache->answered ? 'A' : ' ';
  tmp[4] = cache->deleted ? 'D' : ' ';
  sprintf (tmp+5,"%4lu) ",cache->msgno);
  mail_date (tmp+11,cache);
  tmp[17] = ' ';
  tmp[18] = '\0';
  mail_fetchfrom (tmp+18,stream,msgno,(long) 20);
  strcat (tmp," ");
  if (i = cache->user_flags) {
    strcat (tmp,"{");
    while (i) {
      strcat (tmp,stream->user_flags[find_rightmost_bit (&i)]);
      if (i) strcat (tmp," ");
    }
    strcat (tmp,"} ");
  }
  mail_fetchsubject (t = tmp + strlen (tmp),stream,msgno,(long) 25);
  sprintf (t += strlen (t)," (%lu chars)",cache->rfc822_size);
  puts (tmp);
}

/* MM display body
 * Accepts: BODY structure pointer
 *	    prefix string
 *	    index
 */

void display_body (BODY *body,char *pfx,long i)
{
  char tmp[MAILTMPLEN];
  char *s = tmp;
  PARAMETER *par;
  PART *part;			/* multipart doesn't have a row to itself */
  if (body->type == TYPEMULTIPART) {
				/* if not first time, extend prefix */
    if (pfx) sprintf (tmp,"%s%ld.",pfx,++i);
    else tmp[0] = '\0';
    for (i = 0,part = body->nested.part; part; part = part->next)
      display_body (&part->body,tmp,i++);
  }
  else {			/* non-multipart, output oneline descriptor */
    if (!pfx) pfx = "";		/* dummy prefix if top level */
    sprintf (s," %s%ld %s",pfx,++i,body_types[body->type]);
    if (body->subtype) sprintf (s += strlen (s),"/%s",body->subtype);
    if (body->description) sprintf (s += strlen (s)," (%s)",body->description);
    if (par = body->parameter) do
      sprintf (s += strlen (s),";%s=%s",par->attribute,par->value);
    while (par = par->next);
    if (body->id) sprintf (s += strlen (s),", id = %s",body->id);
    switch (body->type) {	/* bytes or lines depending upon body type */
    case TYPEMESSAGE:		/* encapsulated message */
    case TYPETEXT:		/* plain text */
      sprintf (s += strlen (s)," (%lu lines)",body->size.lines);
      break;
    default:
      sprintf (s += strlen (s)," (%lu bytes)",body->size.bytes);
      break;
    }
    puts (tmp);			/* output this line */
				/* encapsulated message? */
    if ((body->type == TYPEMESSAGE) && !strcmp (body->subtype,"RFC822") &&
	(body = body->nested.msg->body)) {
      if (body->type == TYPEMULTIPART) display_body (body,pfx,i-1);
      else {			/* build encapsulation prefix */
	sprintf (tmp,"%s%ld.",pfx,i);
	display_body (body,tmp,(long) 0);
      }
    }
  }
}

/* MM status report
 * Accepts: MAIL stream
 */

void status (MAILSTREAM *stream)
{
  unsigned long i;
  char *s,date[MAILTMPLEN];
  THREADER *thr;
  AUTHENTICATOR *auth;
  rfc822_date (date);
  puts (date);
  if (stream) {
    if (stream->mailbox)
      printf (" %s mailbox: %s, %lu messages, %lu recent\n",
	      stream->dtb->name,stream->mailbox,stream->nmsgs,stream->recent);
    else puts ("%No mailbox is open on this stream");
    if (stream->user_flags[0]) {
      printf ("Keywords: %s",stream->user_flags[0]);
      for (i = 1; i < NUSERFLAGS && stream->user_flags[i]; ++i)
	printf (", %s",stream->user_flags[i]);
      puts ("");
    }
    if (!strcmp (stream->dtb->name,"imap")) {
      if (LEVELIMAP4rev1 (stream)) s = "IMAP4rev1 (RFC 3501)";
      else if (LEVEL1730 (stream)) s = "IMAP4 (RFC 1730)";
      else if (LEVELIMAP2bis (stream)) s = "IMAP2bis";
      else if (LEVEL1176 (stream)) s = "IMAP2 (RFC 1176)";
      else s = "IMAP2 (RFC 1064)";
      printf ("%s server %s\n",s,imap_host (stream));
      if (LEVELIMAP4 (stream)) {
	if (i = imap_cap (stream)->auth) {
	  s = "";
	  printf ("Mutually-supported SASL mechanisms:");
	  while (auth = mail_lookup_auth (find_rightmost_bit (&i) + 1)) {
	    printf (" %s",auth->name);
	    if (!strcmp (auth->name,"PLAIN"))
	      s = "\n  [LOGIN will not be listed here if PLAIN is supported]";
	  }
	  puts (s);
	}
	printf ("Supported standard extensions:\n");
	if (LEVELACL (stream)) puts (" Access Control lists (RFC 2086)");
	if (LEVELQUOTA (stream)) puts (" Quotas (RFC 2087)");
	if (LEVELLITERALPLUS (stream))
	  puts (" Non-synchronizing literals (RFC 2088)");
	if (LEVELIDLE (stream)) puts (" IDLE unsolicited update (RFC 2177)");
	if (LEVELMBX_REF (stream)) puts (" Mailbox referrals (RFC 2193)");
	if (LEVELLOG_REF (stream)) puts (" Login referrals (RFC 2221)");
	if (LEVELANONYMOUS (stream)) puts (" Anonymous access (RFC 2245)");
	if (LEVELNAMESPACE (stream)) puts (" Multiple namespaces (RFC 2342)");
	if (LEVELUIDPLUS (stream)) puts (" Extended UID behavior (RFC 2359)");
	if (LEVELSTARTTLS (stream))
	  puts (" Transport Layer Security (RFC 2595)");
	if (LEVELLOGINDISABLED (stream))
	  puts (" LOGIN command disabled (RFC 2595)");
	if (LEVELID (stream))
	  puts (" Implementation identity negotiation (RFC 2971)");
	if (LEVELCHILDREN (stream))
	  puts (" LIST children announcement (RFC 3348)");
	if (LEVELMULTIAPPEND (stream))
	  puts (" Atomic multiple APPEND (RFC 3502)");
	if (LEVELBINARY (stream))
	  puts (" Binary body content (RFC 3516)");
	puts ("Supported draft extensions:");
	if (LEVELUNSELECT (stream)) puts (" Mailbox unselect");
	if (LEVELSASLIR (stream)) puts (" SASL initial client response");
	if (LEVELSORT (stream)) puts (" Server-based sorting");
	if (LEVELTHREAD (stream)) {
	  printf (" Server-based threading:");
	  for (thr = imap_cap (stream)->threader; thr; thr = thr->next)
	    printf (" %s",thr->name);
	  putchar ('\n');
	}
	if (LEVELSCAN (stream)) puts (" Mailbox text scan");
	if (i = imap_cap (stream)->extlevel) {
	  printf ("Supported BODYSTRUCTURE extensions:");
	  switch (i) {
	  case BODYEXTLOC: printf (" location");
	  case BODYEXTLANG: printf (" language");
	  case BODYEXTDSP: printf (" disposition");
	  case BODYEXTMD5: printf (" MD5\n");
	  }
	}
      }
      else putchar ('\n');
    }
  }
}


/* Prompt user for input
 * Accepts: pointer to prompt message
 *          pointer to input buffer
 */

void prompt (char *msg,char *txt)
{
  printf ("%s",msg);
  gets (txt);
}

/* Interfaces to C-client */


void mm_searched (MAILSTREAM *stream,unsigned long number)
{
}


void mm_exists (MAILSTREAM *stream,unsigned long number)
{
}


void mm_expunged (MAILSTREAM *stream,unsigned long number)
{
}


void mm_flags (MAILSTREAM *stream,unsigned long number)
{
}


void mm_notify (MAILSTREAM *stream,char *string,long errflg)
{
  mm_log (string,errflg);
}


void mm_list (MAILSTREAM *stream,int delimiter,char *mailbox,long attributes)
{
  putchar (' ');
  if (delimiter) putchar (delimiter);
  else fputs ("NIL",stdout);
  putchar (' ');
  fputs (mailbox,stdout);
  if (attributes & LATT_NOINFERIORS) fputs (", no inferiors",stdout);
  if (attributes & LATT_NOSELECT) fputs (", no select",stdout);
  if (attributes & LATT_MARKED) fputs (", marked",stdout);
  if (attributes & LATT_UNMARKED) fputs (", unmarked",stdout);
  putchar ('\n');
}


void mm_lsub (MAILSTREAM *stream,int delimiter,char *mailbox,long attributes)
{
  putchar (' ');
  if (delimiter) putchar (delimiter);
  else fputs ("NIL",stdout);
  putchar (' ');
  fputs (mailbox,stdout);
  if (attributes & LATT_NOINFERIORS) fputs (", no inferiors",stdout);
  if (attributes & LATT_NOSELECT) fputs (", no select",stdout);
  if (attributes & LATT_MARKED) fputs (", marked",stdout);
  if (attributes & LATT_UNMARKED) fputs (", unmarked",stdout);
  putchar ('\n');
}


void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status)
{
  printf (" Mailbox %s",mailbox);
  if (status->flags & SA_MESSAGES) printf (", %lu messages",status->messages);
  if (status->flags & SA_RECENT) printf (", %lu recent",status->recent);
  if (status->flags & SA_UNSEEN) printf (", %lu unseen",status->unseen);
  if (status->flags & SA_UIDVALIDITY) printf (", %lu UID validity",
					      status->uidvalidity);
  if (status->flags & SA_UIDNEXT) printf (", %lu next UID",status->uidnext);
  printf ("\n");
}


void mm_log (char *string,long errflg)
{
  switch ((short) errflg) {
  case NIL:
    printf ("[%s]\n",string);
    break;
  case PARSE:
  case WARN:
    printf ("%%%s\n",string);
    break;
  case ERROR:
    printf ("?%s\n",string);
    break;
  }
}


void mm_dlog (char *string)
{
  puts (string);
}


void mm_login (NETMBX *mb,char *user,char *pwd,long trial)
{
  char tmp[MAILTMPLEN];
  if (curhst) fs_give ((void **) &curhst);
  curhst = (char *) fs_get (1+strlen (mb->host));
  strcpy (curhst,mb->host);
  if (*mb->user) {
    strcpy (user,mb->user);
    sprintf (tmp,"{%s/%s/user=\"%s\"} password: ",mb->host,mb->service,mb->user);
  }
  else {
    sprintf (tmp,"{%s/%s} username: ",mb->host,mb->service);
    prompt (tmp,user);
    strcpy (tmp,"Password: ");
  }
  if (curusr) fs_give ((void **) &curusr);
  curusr = cpystr (user);
  strcpy (pwd,getpass (tmp));
}


void mm_critical (MAILSTREAM *stream)
{
}


void mm_nocritical (MAILSTREAM *stream)
{
}


long mm_diskerror (MAILSTREAM *stream,long errcode,long serious)
{
#if UNIXLIKE
  kill (getpid (),SIGSTOP);
#else
  abort ();
#endif
  return NIL;
}


void mm_fatal (char *string)
{
  printf ("?%s\n",string);
}

/* SMTP tester */

void smtptest (long debug)
{
  SENDSTREAM *stream = NIL;
  char line[MAILTMPLEN];
  char *text = (char *) fs_get (8*MAILTMPLEN);
  ENVELOPE *msg = mail_newenvelope ();
  BODY *body = mail_newbody ();
  msg->from = mail_newaddr ();
  msg->from->personal = cpystr (personalname);
  msg->from->mailbox = cpystr (curusr);
  msg->from->host = cpystr (curhst);
  msg->return_path = mail_newaddr ();
  msg->return_path->mailbox = cpystr (curusr);
  msg->return_path->host = cpystr (curhst);
  prompt ("To: ",line);
  rfc822_parse_adrlist (&msg->to,line,curhst);
  if (msg->to) {
    prompt ("cc: ",line);
    rfc822_parse_adrlist (&msg->cc,line,curhst);
  }
  else {
    prompt ("Newsgroups: ",line);
    if (*line) msg->newsgroups = cpystr (line);
    else {
      mail_free_body (&body);
      mail_free_envelope (&msg);
      fs_give ((void **) &text);
      return;
    }
  }
  prompt ("Subject: ",line);
  msg->subject = cpystr (line);
  puts (" Msg (end with a line with only a '.'):");
  body->type = TYPETEXT;
  *text = '\0';
  while (gets (line)) {
    if (line[0] == '.') {
      if (line[1] == '\0') break;
      else strcat (text,".");
    }
    strcat (text,line);
    strcat (text,"\015\012");
  }
  body->contents.text.data = (unsigned char *) text;
  body->contents.text.size = strlen (text);
  rfc822_date (line);
  msg->date = (char *) fs_get (1+strlen (line));
  strcpy (msg->date,line);
  if (msg->to) {
    puts ("Sending...");
    if (stream = smtp_open (hostlist,debug)) {
      if (smtp_mail (stream,"MAIL",msg,body)) puts ("[Ok]");
      else printf ("[Failed - %s]\n",stream->reply);
    }
  }
  else {
    puts ("Posting...");
    if (stream = nntp_open (newslist,debug)) {
      if (nntp_mail (stream,msg,body)) puts ("[Ok]");
      else printf ("[Failed - %s]\n",stream->reply);
    }
  }
  if (stream) smtp_close (stream);
  else puts ("[Can't open connection to any server]");
  mail_free_envelope (&msg);
  mail_free_body (&body);
}
