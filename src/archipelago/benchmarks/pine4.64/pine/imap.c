#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: imap.c 14081 2005-09-12 22:04:25Z hubert@u.washington.edu $";
#endif
/*----------------------------------------------------------------------

            T H E    P I N E    M A I L   S Y S T E M

   Laurence Lundblade and Mike Seibel
   Networks and Distributed Computing
   Computing and Communications
   University of Washington
   Administration Builiding, AG-44
   Seattle, Washington, 98195, USA
   Internet: lgl@CAC.Washington.EDU
             mikes@CAC.Washington.EDU

   Please address all bugs and comments to "pine-bugs@cac.washington.edu"


   Pine and Pico are registered trademarks of the University of Washington.
   No commercial use of these trademarks may be made without prior written
   permission of the University of Washington.

   Pine, Pico, and Pilot software and its included text are Copyright
   1989-2004 by the University of Washington.

   The full text of our legal notices is contained in the file called
   CPYRIGHT, included with this distribution.


   USENET News reading additions in part by L Lundblade / NorthWestNet, 1993
   lgl@nwnet.net

   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    * 			Copyright (c) 1986, 1987 Dave Taylor              *
    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************
 

  ----------------------------------------------------------------------*/

/*======================================================================
    imap.c
    The call back routines for the c-client/imap
       - handles error messages and other notification
       - handles prelimirary notification of new mail and expunged mail
       - prompting for imap server login and password 

 ====*/

#include "headers.h"


/*
 * struct used to keep track of password/host/user triples.
 * The problem is we only want to try user names and passwords if
 * we've already tried talking to this host before.
 * 
 */
typedef struct _mmlogin_s {
    char	   *user,
		   *passwd;
    unsigned	    altflag:1;
    unsigned	    ok_novalidate:1;
    unsigned	    warned:1;
    STRLIST_S	   *hosts;
    struct _mmlogin_s *next;
} MMLOGIN_S;


typedef	struct _se_app_s {
    char *folder;
    long  flags;
} SE_APP_S;


/*
 * Internal prototypes
 */
void  mm_login_alt_cue PROTO((NETMBX *));
long  imap_seq_exec PROTO((MAILSTREAM *, char *,
			   long (*) PROTO((MAILSTREAM *, long, void *)),
			   void *));
long  imap_seq_exec_append PROTO((MAILSTREAM *, long, void *));
int   imap_get_ssl PROTO((MMLOGIN_S *, STRLIST_S *, int *, int *));
char *imap_get_user PROTO((MMLOGIN_S *, STRLIST_S *));
int   imap_get_passwd PROTO((MMLOGIN_S *, char *, char *, STRLIST_S *, int));
void  imap_set_passwd PROTO((MMLOGIN_S **, char *, char *, STRLIST_S *, int,
			     int, int));
int   imap_same_host PROTO((STRLIST_S *, STRLIST_S *));
int   answer_cert_failure PROTO((int, MSGNO_S *, SCROLL_S *));
char *ps_get PROTO((size_t));
long  pine_tcptimeout_noscreen PROTO((long, long));
#ifdef	PASSFILE
char  xlate_in PROTO((int));
char  xlate_out PROTO((char));
char *passfile_name PROTO((char *, char *, size_t));
int   read_passfile PROTO((char *, MMLOGIN_S **));
void  write_passfile PROTO((char *, MMLOGIN_S *));
int   get_passfile_passwd PROTO((char *, char *, char *, STRLIST_S *, int));
int   is_using_passfile PROTO(());
void  set_passfile_passwd PROTO((char *, char *, char *, STRLIST_S *, int));
char *get_passfile_user PROTO((char *, STRLIST_S *));
void  update_passfile_hostlist PROTO((char *, char *, STRLIST_S *, int));
#endif


/*
 * Exported globals setup by searching functions to tell mm_searched
 * where to put message numbers that matched the search criteria,
 * and to allow mm_searched to return number of matches.
 */
MAILSTREAM *mm_search_stream;
long	    mm_search_count  = 0L;
MAILSTATUS  mm_status_result;
MAILSTATUS *pine_cached_status;    /* to implement status for #move folder */

MM_LIST_S  *mm_list_info;

/*
 * Local global to hook cached list of host/user/passwd triples to.
 */
static	MMLOGIN_S	*mm_login_list     = NULL;
static	MMLOGIN_S	*cert_failure_list = NULL;

/*
 * Instead of storing cached passwords in free storage, store them in this
 * private space. This makes it easier and more reliable when we want
 * to zero this space out. We only store passwords here (char *) so we
 * don't need to worry about alignment.
 */
static	char		private_store[512];

static	char *details_cert, *details_host, *details_reason;



/*----------------------------------------------------------------------
      Write imap debugging information into log file

   Args: strings -- the string for the debug file

 Result: message written to the debug log file
  ----*/
void
mm_dlog(string)
    char *string;
{
    char *p, *q = NULL, save, *continued;
    int   more = 1;

#ifdef	_WINDOWS
    mswin_imaptelemetry(string);
#endif
#ifdef	DEBUG
    continued = "";
    p = string;
#ifdef DEBUGJOURNAL
    /* because string can be really long and we don't want to lose any of it */
    if(p)
      more = 1;

    while(more){
	if(q){
	    *q = save;
	    p = q;
	    continued = "(Continuation line) ";
	}

	if(strlen(p) > 63000){
	    q = p + 60000;
	    save = *q;
	    *q = '\0';
	}
	else
	  more = 0;
#endif
	dprint((ps_global->debug_imap >= 4 && debug < 4) ? debug : 4,
	       (debugfile, "IMAP DEBUG %s%s: %s\n",
		   continued ? continued : "",
		   debug_time(1, ps_global->debug_timestamp), p ? p : "?"));
#ifdef DEBUGJOURNAL
    }
#endif
#endif
}



/*----------------------------------------------------------------------
      Queue imap log message for display in the message line

   Args: string -- The message 
         errflg -- flag set to 1 if pertains to an error

 Result: Message queued for display

 The c-client/imap reports most of it's status and errors here
  ---*/
void
mm_log(string, errflg)
    char *string;
    long  errflg;
{
    char        message[sizeof(ps_global->c_client_error)];
    char       *occurence;
    int         was_capitalized;
    time_t      now;
    struct tm  *tm_now;

    now = time((time_t *)0);
    tm_now = localtime(&now);

    dprint(((errflg == TCPDEBUG) && ps_global->debug_tcp) ? 1 :
           (errflg == TCPDEBUG) ? 10 : 2,
	   (debugfile,
	    "IMAP %2.2d:%2.2d:%2.2d %d/%d mm_log %s: %s\n",
	    tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
	    tm_now->tm_mon+1, tm_now->tm_mday,
            (!errflg)            ? "babble"  : 
	     (errflg == ERROR)    ? "error"   :
	      (errflg == WARN)     ? "warning" :
	       (errflg == PARSE)    ? "parse"   :
		(errflg == TCPDEBUG) ? "tcp"     :
		 (errflg == BYE)      ? "bye"     : "unknown",
	    string ? string : "?"));

    if(errflg == ERROR && !strncmp(string, "[TRYCREATE]", 11)){
	ps_global->try_to_create = 1;
	return;
    }
    else if(ps_global->try_to_create
       || !strncmp(string, "[CLOSED]", 8)
       || (sp_dead_stream(ps_global->mail_stream) && strstr(string, "No-op")))
      /*
       * Don't display if creating new folder OR
       * warning about a dead stream ...
       */
      return;

    /*---- replace all "mailbox" with "folder" ------*/
    strncpy(message, string, sizeof(message));
    message[sizeof(message) - 1] = '\0';
    occurence = srchstr(message, "mailbox");
    while(occurence) {
	if(!*(occurence+7)
	   || isspace((unsigned char) *(occurence+7))
	   || *(occurence+7) == ':'){
	    was_capitalized = isupper((unsigned char) *occurence);
	    rplstr(occurence, 7, (errflg == PARSE ? "address" : "folder"));
	    if(was_capitalized)
	      *occurence = (errflg == PARSE ? 'A' : 'F');
	}
	else
	  occurence += 7;

        occurence = srchstr(occurence, "mailbox");
    }

    /*---- replace all "GSSAPI" with "Kerberos" ------*/
    occurence = srchstr(message, "GSSAPI");
    while(occurence) {
	if(!*(occurence+6)
	   || isspace((unsigned char) *(occurence+6))
	   || *(occurence+6) == ':')
	  rplstr(occurence, 6, "Kerberos");
	else
	  occurence += 6;

        occurence = srchstr(occurence, "GSSAPI");
    }

    if(errflg == ERROR)
      ps_global->mm_log_error = 1;

    if(errflg == PARSE || (errflg == ERROR && ps_global->noshow_error))
      strncpy(ps_global->c_client_error, message,
	      sizeof(ps_global->c_client_error));

    if(ps_global->noshow_error
       || (ps_global->noshow_warn && errflg == WARN)
       || !(errflg == ERROR || errflg == WARN))
      return; /* Only care about errors; don't print when asked not to */

    /*---- Display the message ------*/
    q_status_message((errflg == ERROR) ? (SM_ORDER | SM_DING) : SM_ORDER,
		     3, 5, message);
    strncpy(ps_global->last_error, message, sizeof(ps_global->last_error));
    ps_global->last_error[sizeof(ps_global->last_error) - 1] = '\0';
}



/*----------------------------------------------------------------------
         recieve notification from IMAP

  Args: stream  --  Mail stream message is relavant to 
        string  --  The message text
        errflg  --  Set if it is a serious error

 Result: message displayed in status line

 The facility is for general notices, such as connection to server;
 server shutting down etc... It is used infrequently.
  ----------------------------------------------------------------------*/
void
mm_notify(stream, string, errflg)
    MAILSTREAM *stream;
    char       *string;
    long        errflg;
{
    time_t      now;
    struct tm  *tm_now;

    now = time((time_t *)0);
    tm_now = localtime(&now);

    /* be sure to log the message... */
#ifdef DEBUG
    if(ps_global->debug_imap || ps_global->debugmem)
      dprint(errflg == TCPDEBUG ? 7 : 2,
	     (debugfile,
	      "IMAP %2.2d:%2.2d:%2.2d %d/%d mm_notify %s: %s: %s\n",
	      tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
	      tm_now->tm_mon+1, tm_now->tm_mday,
              (!errflg)            ? "babble"     : 
	       (errflg == ERROR)    ? "error"   :
	        (errflg == WARN)     ? "warning" :
	         (errflg == PARSE)    ? "parse"   :
		  (errflg == TCPDEBUG) ? "tcp"     :
		   (errflg == BYE)      ? "bye"     : "unknown",
	      (stream && stream->mailbox) ? stream->mailbox : "-no folder-",
	      string ? string : "?"));
#endif

    sprintf(ps_global->last_error, "%.50s : %.*s",
	    (stream && stream->mailbox) ? stream->mailbox : "-no folder-",
	    min(MAX_SCREEN_COLS, sizeof(ps_global->last_error)-70),
	    string);
    ps_global->last_error[ps_global->ttyo ? ps_global->ttyo->screen_cols
		    : sizeof(ps_global->last_error)-1] = '\0';

    /*
     * Then either set special bits in the pine struct or
     * display the message if it's tagged as an "ALERT" or
     * its errflg > NIL (i.e., WARN, or ERROR)
     */
    if(errflg == BYE)
      /*
       * We'd like to sp_mark_stream_dead() here but we can't do that because
       * that might call mail_close and we are already in a c-client callback.
       * So just set the dead bit and clean it up later.
       */
      sp_set_dead_stream(stream, 1);
    else if(!strncmp(string, "[TRYCREATE]", 11))
      ps_global->try_to_create = 1;
    else if(!strncmp(string, "[REFERRAL ", 10))
      ;  /* handled in the imap_referral() callback */
    else if(!strncmp(string, "[ALERT]", 7))
      q_status_message2(SM_MODAL, 3, 3,
		        "Alert received while accessing \"%.200s\":  %.200s",
			(stream && stream->mailbox)
			  ? stream->mailbox : "-no folder-",
			rfc1522_decode((unsigned char *)(tmp_20k_buf+10000),
				       SIZEOF_20KBUF-10000, string, NULL));
    else if(!strncmp(string, "[UNSEEN ", 8)){
	char *p;
	long  n = 0;

	for(p = string + 8; isdigit(*p); p++)
	  n = (n * 10) + (*p - '0');

	sp_set_first_unseen(stream, n);
    }
    else if(!strncmp(string, "[READ-ONLY]", 11)
	    && !(stream && stream->mailbox && IS_NEWS(stream)))
      q_status_message2(SM_ORDER | SM_DING, 3, 3, "%.200s : %.200s",
			(stream && stream->mailbox)
			  ? stream->mailbox : "-no folder-",
			string + 11);
    else if((errflg && errflg != BYE && errflg != PARSE)
	    && !ps_global->noshow_error
	    && !(errflg == WARN
	         && (ps_global->noshow_warn || (stream && stream->unhealthy))))
      q_status_message(SM_ORDER | ((errflg == ERROR) ? SM_DING : 0),
			   3, 6, ps_global->last_error);
}



/*----------------------------------------------------------------------
       receive notification of new mail from imap daemon

   Args: stream -- The stream the message count report is for.
         number -- The number of messages now in folder.
 
  Result: Sets value in pine state indicating new mailbox size

     Called when the number of messages in the mailbox goes up.  This
 may also be called as a result of an expunge. It increments the
 new_mail_count based on a the difference between the current idea of
 the maximum number of messages and what mm_exists claims. The new mail
 notification is done in newmail.c

 Only worry about the cases when the number grows, as mm_expunged
 handles shrinkage...

 ----*/
void
mm_exists(stream, number)
    MAILSTREAM    *stream;
    unsigned long  number;
{
    long     new_this_call, n;
    int	     exbits = 0, lflags = 0;
    MSGNO_S *msgmap;

#ifdef DEBUG
    if(ps_global->debug_imap > 1 || ps_global->debugmem)
      dprint(3,
	   (debugfile, "=== mm_exists(%lu,%s) called ===\n", number,
     !stream ? "(no stream)" : !stream->mailbox ? "(null)" : stream->mailbox));
#endif

    msgmap = sp_msgmap(stream);
    if(!msgmap)
      return;

    if(mn_get_nmsgs(msgmap) != (long) number){
	sp_set_mail_box_changed(stream, 1);
	/* titlebar will be affected */
	if(ps_global->mail_stream == stream)
	  ps_global->mangled_header = 1;
    }

    if(mn_get_nmsgs(msgmap) < (long) number){
	new_this_call = (long) number - mn_get_nmsgs(msgmap);
	sp_set_new_mail_count(stream,
			      sp_new_mail_count(stream) + new_this_call);
	if(ps_global->mail_stream != stream)
	  sp_set_recent_since_visited(stream,
			      sp_recent_since_visited(stream) + new_this_call);

	mn_add_raw(msgmap, new_this_call);

	/*
	 * Set local "recent" and "hidden" bits...
	 */
	for(n = 0; n < new_this_call; n++, number--){
	    if(msgno_exceptions(stream, number, "0", &exbits, FALSE))
	      exbits |= MSG_EX_RECENT;
	    else
	      exbits = MSG_EX_RECENT;

	    msgno_exceptions(stream, number, "0", &exbits, TRUE);

	    if(SORT_IS_THREADED(msgmap))
	      lflags |= MN_USOR;

	    /*
	     * If we're zoomed, then hide this message too since
	     * it couldn't have possibly been selected yet...
	     */
	    lflags |= (any_lflagged(msgmap, MN_HIDE) ? MN_HIDE : 0);
	    if(lflags)
	      set_lflag(stream, msgmap, mn_get_total(msgmap) - n, lflags, 1);
	}
    }
}



/*----------------------------------------------------------------------
    Receive notification from IMAP that a single message has been expunged

   Args: stream -- The stream/folder the message is expunged from
         rawno  -- The raw message number that was expunged

mm_expunged is always called on an expunge.  Simply remove all 
reference to the expunged message, shifting internal mappings as
necessary.
  ----*/
void
mm_expunged(stream, rawno)
    MAILSTREAM    *stream;
    unsigned long  rawno;
{
    MESSAGECACHE *mc;
    long          i;
    int           is_current = 0;
    MSGNO_S      *msgmap;

#ifdef DEBUG
    if(ps_global->debug_imap > 1 || ps_global->debugmem)
      dprint(3,
	   (debugfile, "mm_expunged(%s,%lu)\n",
	       stream
		? (stream->mailbox
		    ? stream->mailbox
		    : "(no stream)")
		: "(null)", rawno));
#endif

    msgmap = sp_msgmap(stream);
    if(!msgmap)
      return;

    if(ps_global->mail_stream == stream)
      is_current++;

    if(i = mn_raw2m(msgmap, (long) rawno)){
	dprint(7, (debugfile, "mm_expunged: rawno=%lu msgno=%ld nmsgs=%ld max_msgno=%ld flagged_exld=%ld\n", rawno, i, mn_get_nmsgs(msgmap), mn_get_total(msgmap), msgmap->flagged_exld));

	sp_set_mail_box_changed(stream, 1);
	sp_set_expunge_count(stream, sp_expunge_count(stream) + 1);

	if(is_current){
	    reset_check_point(stream);
	    ps_global->mangled_header = 1;

	    /* flush invalid cache entries */
	    while(i <= mn_get_total(msgmap))
	      clear_index_cache_ent(i++);

	    /* expunged something we're viewing? */
	    if(!ps_global->expunge_in_progress
	       && (mn_is_cur(msgmap, mn_raw2m(msgmap, (long) rawno))
		   && (ps_global->prev_screen == mail_view_screen
		       || ps_global->prev_screen == attachment_screen))){
		ps_global->next_screen = mail_index_screen;
		q_status_message(SM_ORDER | SM_DING , 3, 3,
				 "Message you were viewing is gone!");
	    }
	}
    }
    else{
	dprint(7, (debugfile,
	       "mm_expunged: rawno=%lu was excluded, flagged_exld was %d\n",
	       rawno, msgmap->flagged_exld));
	dprint(7, (debugfile, "             nmsgs=%ld max_msgno=%ld\n",
	       mn_get_nmsgs(msgmap), mn_get_total(msgmap)));
	if(rawno > 0L && rawno <= stream->nmsgs)
	  mc = mail_elt(stream, rawno);

	if(!mc){
	    dprint(7, (debugfile, "             cannot get mail_elt(%lu)\n",
		   rawno));
	}
	else{
	    dprint(7, (debugfile, "             mail_elt(%lu)->spare2=%d\n",
		   rawno, (int) (mc->spare2)));
	}
    }

    if(SORT_IS_THREADED(msgmap)
       && (SEP_THRDINDX()
	   || ps_global->thread_disp_style != THREAD_NONE)){
	long cur;

	/*
	 * When we're sorting with a threaded method an expunged
	 * message may cause the rest of the sort to be wrong. This
	 * isn't so bad if we're just looking at the index. However,
	 * it also causes the thread tree (PINETHRD_S) to become
	 * invalid, so if we're using a threading view we need to
	 * sort in order to fix the tree and to protect fetch_thread().
	 */
	sp_set_need_to_rethread(stream, 1);

	/*
	 * If we expunged the current message which was a member of the
	 * viewed thread, and the adjustment to current will take us
	 * out of that thread, fix it if we can, by backing current up
	 * into the thread. We'd like to just check after mn_flush_raw
	 * below but the problem is that the elts won't change until
	 * after we return from mm_expunged. So we have to manually
	 * check the other messages for CHID2 flags instead of thinking
	 * that we can expunge the current message and then check. It won't
	 * work because the elt will still refer to the expunged message.
	 */
	if(sp_viewing_a_thread(stream)
	   && get_lflag(stream, NULL, rawno, MN_CHID2)
	   && mn_total_cur(msgmap) == 1
	   && mn_is_cur(msgmap, mn_raw2m(msgmap, (long) rawno))
	   && (cur = mn_get_cur(msgmap)) > 1L
	   && cur < mn_get_total(msgmap)
	   && !get_lflag(stream, msgmap, cur + 1L, MN_CHID2)
	   && get_lflag(stream, msgmap, cur - 1L, MN_CHID2))
	  mn_set_cur(msgmap, cur - 1L);
    }

    /*
     * Keep on top of our special flag counts.
     * 
     * NOTE: This is allowed since mail_expunged releases
     * data for this message after the callback.
     */
    if(rawno > 0L && rawno <= stream->nmsgs && (mc = mail_elt(stream, rawno))){
	if(mc->spare)
	  msgmap->flagged_hid--;

	if(mc->spare2)
	  msgmap->flagged_exld--;

	if(mc->spare3)
	  msgmap->flagged_tmp--;

	if(mc->spare4)
	  msgmap->flagged_chid--;

	if(mc->spare8)
	  msgmap->flagged_chid2--;

	if(mc->spare5)
	  msgmap->flagged_coll--;

	if(mc->spare6)
	  msgmap->flagged_stmp--;

	if(mc->spare7)
	  msgmap->flagged_usor--;

	if(mc->spare || mc->spare4)
	  msgmap->flagged_invisible--;

	free_pine_elt(&mc->sparep);
    }

    /*
     * if it's in the sort array, flush it, otherwise
     * decrement raw sequence numbers greater than "rawno"
     */
    mn_flush_raw(msgmap, (long) rawno);
}



/*---------------------------------------------------------------------- 
        receive notification that search found something           

 Input:  mail stream and message number of located item

 Result: nothing, not used by pine
  ----*/
void
mm_searched(stream, rawno)
    MAILSTREAM    *stream;
    unsigned long  rawno;
{
    MESSAGECACHE *mc;

    if(rawno > 0L && stream && rawno <= stream->nmsgs
       && (mc = mail_elt(stream, rawno))){
	mc->searched = 1;
	if(stream == mm_search_stream)
	  mm_search_count++;
    }
}



/*----------------------------------------------------------------------
      Get login and password from user for IMAP login
  
  Args:  mb -- The mail box property struct
         user   -- Buffer to return the user name in 
         passwd -- Buffer to return the passwd in
         trial  -- The trial number or number of attempts to login
    user is at least size NETMAXUSER
    passwd is apparently at least MAILTMPLEN, but mrc has asked us to
      use a max size of about 100 instead

 Result: username and password passed back to imap
  ----*/
void
mm_login(mb, user, pwd, trial)
    NETMBX *mb;
    char   *user;
    char   *pwd;
    long    trial;
{
    char      prompt[1000], *last, *logleadin;
    char      port[20], non_def_port[20], insecure[20];
    char      defuser[NETMAXUSER];
    char      hostleadin[20], hostname[200], defubuf[200];
    char      pwleadin[50];
    char      hostlist0[MAILTMPLEN], hostlist1[MAILTMPLEN];
    char     *insec = " (INSECURE)";
    STRLIST_S hostlist[2];
    HelpType  help ;
    int       len, rc, q_line, flags;
    int       oespace, avail, need, save_dont_use;
    struct servent *sv;
#define NETMAXPASSWD 100

    dprint(9, (debugfile, "mm_login trial=%ld user=%s service=%s%s%s\n",
	       trial, mb->user ? mb->user : "(null)",
	       mb->service ? mb->service : "(null)",
	       mb->port ? " port=" : "",
	       mb->port ? comatose(mb->port) : ""));
    q_line = -(ps_global->ttyo ? ps_global->ttyo->footer_rows : 3);

    /* make sure errors are seen */
    if(ps_global->ttyo)
      flush_status_messages(0);

    /*
     * Add port number to hostname if going through a tunnel or something
     */
    non_def_port[0] = '\0';
    if(mb->port && mb->service &&
       (sv = getservbyname(mb->service, "tcp")) &&
       (mb->port != ntohs(sv->s_port))){
	sprintf(non_def_port, ":%lu", mb->port);
	dprint(9, (debugfile, "mm_login: using non-default port=%s\n",
		   non_def_port ? non_def_port : "?"));
    }

    /*
     * set up host list for sybil servers...
     */
    if(*non_def_port){
	strncpy(hostlist0, mb->host, sizeof(hostlist0)-1);
	hostlist0[sizeof(hostlist0)-1] = '\0';
	strncat(hostlist0, non_def_port, sizeof(hostlist0)-strlen(hostlist0)-1);
	hostlist0[sizeof(hostlist0)-1] = '\0';
	hostlist[0].name = hostlist0;
	if(mb->orighost && mb->orighost[0] && strucmp(mb->host, mb->orighost)){
	    strncpy(hostlist1, mb->orighost, sizeof(hostlist1)-1);
	    hostlist1[sizeof(hostlist1)-1] = '\0';
	    strncat(hostlist1, non_def_port, sizeof(hostlist1)-strlen(hostlist1)-1);
	    hostlist1[sizeof(hostlist1)-1] = '\0';
	    hostlist[0].next = &hostlist[1];
	    hostlist[1].name = hostlist1;
	    hostlist[1].next = NULL;
	}
	else
	  hostlist[0].next = NULL;
    }
    else{
	hostlist[0].name = mb->host;
	if(mb->orighost && mb->orighost[0] && strucmp(mb->host, mb->orighost)){
	    hostlist[0].next = &hostlist[1];
	    hostlist[1].name = mb->orighost;
	    hostlist[1].next = NULL;
	}
	else
	  hostlist[0].next = NULL;
    }

    if(hostlist[0].name){
	dprint(9, (debugfile, "mm_login: host=%s\n",
	       hostlist[0].name ? hostlist[0].name : "?"));
	if(hostlist[0].next && hostlist[1].name){
	    dprint(9, (debugfile, "mm_login: orighost=%s\n", hostlist[1].name));
	}
    }

    /*
     * Initialize user name with either 
     *     1) /user= value in the stream being logged into,
     *  or 2) the user name we're running under.
     *
     * Note that VAR_USER_ID is not yet initialized if this login is
     * the one to access the remote config file. In that case, the user
     * can supply the username in the config file name with /user=.
     */
    if(trial == 0L){
	strncpy(user, (*mb->user) ? mb->user :
		       ps_global->VAR_USER_ID ? ps_global->VAR_USER_ID : "",
		       NETMAXUSER);
	user[NETMAXUSER-1] = '\0';

	/* try last working password associated with this host. */
	if(imap_get_passwd(mm_login_list, pwd, user, hostlist,
	   (mb->sslflag||mb->tlsflag))){
	    dprint(9, (debugfile, "mm_login: found a password to try\n"));
	    return;
	}

#ifdef	PASSFILE
	/* check to see if there's a password left over from last session */
	if(get_passfile_passwd(ps_global->pinerc, pwd,
			       user, hostlist, (mb->sslflag||mb->tlsflag))){
	    imap_set_passwd(&mm_login_list, pwd, user,
			    hostlist, (mb->sslflag||mb->tlsflag), 0, 0);
	    update_passfile_hostlist(ps_global->pinerc, user, hostlist,
				     (mb->sslflag||mb->tlsflag));
	    dprint(9, (debugfile, "mm_login: found a password in passfile to try\n"));
	    return;
	}
#endif

	/*
	 * If no explicit user name supplied and we've not logged in
	 * with our local user name, see if we've visited this
	 * host before as someone else.
	 */
	if(!*mb->user &&
	   ((last = imap_get_user(mm_login_list, hostlist))
#ifdef	PASSFILE
	    ||
	    (last = get_passfile_user(ps_global->pinerc, hostlist))
#endif
								   )){
	    strncpy(user, last, NETMAXUSER);
	    dprint(9, (debugfile, "mm_login: found user=%s\n",
		   user ? user : "?"));

	    /* try last working password associated with this host/user. */
	    if(imap_get_passwd(mm_login_list, pwd, user, hostlist,
	       (mb->sslflag||mb->tlsflag))){
		dprint(9, (debugfile,
		       "mm_login: found a password for user=%s to try\n",
		       user ? user : "?"));
		return;
	    }

#ifdef	PASSFILE
	    /* check to see if there's a password left over from last session */
	    if(get_passfile_passwd(ps_global->pinerc, pwd,
				   user, hostlist, (mb->sslflag||mb->tlsflag))){
		imap_set_passwd(&mm_login_list, pwd, user,
				hostlist, (mb->sslflag||mb->tlsflag), 0, 0);
		update_passfile_hostlist(ps_global->pinerc, user, hostlist,
					 (mb->sslflag||mb->tlsflag));
		dprint(9, (debugfile,
		  "mm_login: found a password for user=%s in passfile to try\n",
		  user ? user : "?"));
		return;
	    }
#endif
	}

#if !defined(DOS) && !defined(OS2)
	if(!*mb->user && !*user &&
	   (last = (ps_global->ui.login && ps_global->ui.login[0])
				    ? ps_global->ui.login : NULL)
								 ){
	    strncpy(user, last, NETMAXUSER);
	    dprint(9, (debugfile, "mm_login: found user=%s\n",
		   user ? user : "?"));

	    /* try last working password associated with this host. */
	    if(imap_get_passwd(mm_login_list, pwd, user, hostlist,
	       (mb->sslflag||mb->tlsflag))){
		dprint(9, (debugfile, "mm_login:ui: found a password to try\n"));
		return;
	    }

#ifdef	PASSFILE
	    /* check to see if there's a password left over from last session */
	    if(get_passfile_passwd(ps_global->pinerc, pwd,
				   user, hostlist, (mb->sslflag||mb->tlsflag))){
		imap_set_passwd(&mm_login_list, pwd, user,
				hostlist, (mb->sslflag||mb->tlsflag), 0, 0);
		update_passfile_hostlist(ps_global->pinerc, user, hostlist,
					 (mb->sslflag||mb->tlsflag));
		dprint(9, (debugfile, "mm_login:ui: found a password in passfile to try\n"));
		return;
	    }
#endif
	}
#endif
    }

    user[NETMAXUSER-1] = '\0';

    /*
     * Even if we have a user now, user gets a chance to change it.
     */
    ps_global->mangled_footer = 1;
    if(!*mb->user){

	help = NO_HELP;

	/*
	 * Instead of offering user with a value that the user can edit,
	 * we offer [user] as a default so that the user can type CR to
	 * use it. Otherwise, the user has to type in whole name.
	 */
	strncpy(defuser, user, sizeof(defuser)-1);
	defuser[sizeof(defuser)-1] = '\0';
	user[0] = '\0';

	/*
	 * Need space for "+ HOST: "
	 *                hostname
	 *                " (INSECURE)"
	 *                ENTER LOGIN NAME
	 *                " [defuser] : "
	 *                about 15 chars for input
	 */

	sprintf(hostleadin, "%sHOST: ",
		(!ps_global->ttyo && (mb->sslflag||mb->tlsflag)) ? "+ " : "");

	strncpy(hostname, mb->host, sizeof(hostname)-1);
	hostname[sizeof(hostname)-1] = '\0';

	/*
	 * Add port number to hostname if going through a tunnel or something
	 */
	if(*non_def_port)
	  strncpy(port, non_def_port, sizeof(port));
	else
	  port[0] = '\0';
	
	insecure[0] = '\0';
	/* if not encrypted and SSL/TLS is supported */
	if(!(mb->sslflag||mb->tlsflag) &&
	   mail_parameters(NIL, GET_SSLDRIVER, NIL))
	  strncpy(insecure, insec, sizeof(insecure));

	logleadin = "  ENTER LOGIN NAME";

	sprintf(defubuf, "%s%.100s%s : ", (*defuser) ? " [" : "",
					  (*defuser) ? defuser : "",
					  (*defuser) ? "]" : "");
	/* space reserved after prompt */
	oespace = max(min(15, (ps_global->ttyo ? ps_global->ttyo->screen_cols : 80)/5), 6);

	avail = ps_global->ttyo ? ps_global->ttyo->screen_cols : 80;
	need = strlen(hostleadin) + strlen(hostname) + strlen(port) +
	       strlen(insecure) + strlen(logleadin) + strlen(defubuf) + oespace;

	if(avail < need){
	    /* reduce length of logleadin */
	    len = strlen(logleadin);
	    logleadin = " LOGIN";
	    need -= (len - strlen(logleadin));

	    if(avail < need){
		/* get two or three spaces from hostleadin */
		len = strlen(hostleadin);
		sprintf(hostleadin, "%sHST:",
		  (!ps_global->ttyo && (mb->sslflag||mb->tlsflag)) ? "+" : "");
		need -= (len - strlen(hostleadin));

		/* get rid of port */
		if(avail < need && strlen(port) > 0){
		    need -= strlen(port);
		    port[0] = '\0';
		}

		if(avail < need){
		    int reduce_to;

		    /*
		     * Reduce space for hostname. Best we can do is 6 chars
		     * with hos...
		     */
		    reduce_to = (need - avail < strlen(hostname) - 6) ? (strlen(hostname)-(need-avail)) : 6;
		    len = strlen(hostname);
		    strncpy(hostname+reduce_to-3, "...", 4);
		    need -= (len - strlen(hostname));

		    if(avail < need && strlen(insecure) > 0){
			if(need - avail <= 3){
			    need -= 3;
			    insecure[strlen(insecure)-4] = ')';
			    insecure[strlen(insecure)-3] = '\0';
			}
			else{
			    need -= strlen(insecure);
			    insecure[0] = '\0';
			}
		    }

		    if(avail < need){
			if(strlen(defubuf) > 3){
			    len = strlen(defubuf);
			    strncpy(defubuf, " [..] :", 9);
			    need -= (len - strlen(defubuf));
			}

			if(avail < need)
			  strncpy(defubuf, ":", 2);

			/*
			 * If it still doesn't fit, optionally_enter gets
			 * to worry about it.
			 */
		    }
		}
	    }
	}

	sprintf(prompt, "%.20s%.200s%.20s%.20s%.50s%.200s",
		hostleadin, hostname, port, insecure, logleadin, defubuf);

	while(1) {
	    if(ps_global->ttyo)
	      mm_login_alt_cue(mb);

	    flags = OE_APPEND_CURRENT;
	    save_dont_use = ps_global->dont_use_init_cmds;
	    ps_global->dont_use_init_cmds = 1;
#ifdef _WINDOWS
	    if(!ps_global->ttyo){
		if(!*user && *defuser)
		  strcpy(user, defuser);
		rc = os_login_dialog(mb, user, NETMAXUSER, pwd, NETMAXPASSWD,
#ifdef PASSFILE
				is_using_passfile() ? 1 :
#endif /* PASSFILE */
				0, 0);
		ps_global->dont_use_init_cmds = save_dont_use;
		if(rc == 0 && *user && *pwd)
		  goto nopwpmt;
	    }
	    else
#endif /* _WINDOWS */
	    rc = optionally_enter(user, q_line, 0, NETMAXUSER,
				  prompt, NULL, help, &flags);
	    ps_global->dont_use_init_cmds = save_dont_use;

	    if(rc == 3) {
		help = help == NO_HELP ? h_oe_login : NO_HELP;
		continue;
	    }

	    /* default */
	    if(rc == 0 && !*user)
	      strncpy(user, defuser, NETMAXUSER);
	      
	    if(rc != 4)
	      break;
	}

	if(rc == 1 || !user[0]) {
	    user[0]   = '\0';
	    pwd[0] = '\0';
	}
    }
    else
      strncpy(user, mb->user, NETMAXUSER);

    user[NETMAXUSER-1] = '\0';
    pwd[NETMAXPASSWD-1] = '\0';

    if(!user[0])
      return;

    /*
     * Now that we have a user, we can check in the cache again to see
     * if there is a password there. Try last working password associated
     * with this host and user.
     */
    if(trial == 0L && !*mb->user){
	if(imap_get_passwd(mm_login_list, pwd, user, hostlist,
	   (mb->sslflag||mb->tlsflag)))
	  return;

#ifdef	PASSFILE
	if(get_passfile_passwd(ps_global->pinerc, pwd,
			       user, hostlist, (mb->sslflag||mb->tlsflag))){
	    imap_set_passwd(&mm_login_list, pwd, user,
			    hostlist, (mb->sslflag||mb->tlsflag), 0, 0);
	    return;
	}
#endif
    }

    /*
     * Didn't find password in cache or this isn't the first try. Ask user.
     */
    help = NO_HELP;

    /*
     * Need space for "+ HOST: "
     *                hostname
     *                " (INSECURE) "
     *                "  USER: "
     *                user
     *                "  ENTER PASSWORD: "
     *                about 15 chars for input
     */
    
    sprintf(hostleadin, "%sHOST: ",
	    (!ps_global->ttyo && (mb->sslflag||mb->tlsflag)) ? "+ " : "");

    strncpy(hostname, mb->host, sizeof(hostname)-1);
    hostname[sizeof(hostname)-1] = '\0';

    /*
     * Add port number to hostname if going through a tunnel or something
     */
    if(*non_def_port)
      strncpy(port, non_def_port, sizeof(port));
    else
      port[0] = '\0';
    
    insecure[0] = '\0';
    /* if not encrypted and SSL/TLS is supported */
    if(!(mb->sslflag||mb->tlsflag) &&
       mail_parameters(NIL, GET_SSLDRIVER, NIL))
      strncpy(insecure, insec, sizeof(insecure));
    
    logleadin = "  USER: ";

    strncpy(defubuf, user, sizeof(defubuf)-1);
    defubuf[sizeof(defubuf)-1] = '\0';

    strncpy(pwleadin, "  ENTER PASSWORD: ", sizeof(pwleadin)-1);
    pwleadin[sizeof(pwleadin)-1] = '\0';

    /* space reserved after prompt */
    oespace = max(min(15, (ps_global->ttyo ? ps_global->ttyo->screen_cols : 80)/5), 6);

    avail = ps_global->ttyo ? ps_global->ttyo->screen_cols : 80;
    need = strlen(hostleadin) + strlen(hostname) + strlen(port) +
	   strlen(insecure) + strlen(logleadin) + strlen(defubuf) +
	   strlen(pwleadin) + oespace;
    
    if(avail < need){
	logleadin = logleadin + 1;
	need--;
	rplstr(pwleadin, 1, "");
	need--;

	if(avail < need){
	    /* get two or three spaces from hostleadin */
	    len = strlen(hostleadin);
	    sprintf(hostleadin, "%sHST:",
	      (!ps_global->ttyo && (mb->sslflag||mb->tlsflag)) ? "+" : "");
	    need -= (len - strlen(hostleadin));

	    /* get rid of port */
	    if(avail < need && strlen(port) > 0){
		need -= strlen(port);
		port[0] = '\0';
	    }

	    if(avail < need){
		len = strlen(pwleadin);
		strncpy(pwleadin, " PASSWORD: ", sizeof(pwleadin)-1);
		pwleadin[sizeof(pwleadin)-1] = '\0';
		need -= (len - strlen(pwleadin));
	    }
	}

	if(avail < need){
	    int reduce_to;

	    /*
	     * Reduce space for hostname. Best we can do is 6 chars
	     * with hos...
	     */
	    reduce_to = (need - avail < strlen(hostname) - 6) ? (strlen(hostname)-(need-avail)) : 6;
	    len = strlen(hostname);
	    strncpy(hostname+reduce_to-3, "...", 4);
	    need -= (len - strlen(hostname));
	    
	    if(avail < need && strlen(insecure) > 0){
		if(need - avail <= 3){
		    need -= 3;
		    insecure[strlen(insecure)-4] = ')';
		    insecure[strlen(insecure)-3] = '\0';
		}
		else{
		    need -= strlen(insecure);
		    insecure[0] = '\0';
		}
	    }

	    if(avail < need){
		len = strlen(logleadin);
		logleadin = " ";
		need -= (len - strlen(logleadin));

		if(avail < need){
		    reduce_to = (need - avail < strlen(defubuf) - 6) ? (strlen(defubuf)-(need-avail)) : 0;
		    if(reduce_to)
		      strncpy(defubuf+reduce_to-3, "...", 4);
		    else
		      defubuf[0] = '\0';
		}
	    }
	}
    }

    sprintf(prompt, "%.20s%.200s%.20s%.20s%.50s%.200s%.50s",
	    hostleadin, hostname, port, insecure, logleadin, defubuf, pwleadin);

    *pwd = '\0';
    while(1) {
	if(ps_global->ttyo)
	  mm_login_alt_cue(mb);

	save_dont_use = ps_global->dont_use_init_cmds;
	ps_global->dont_use_init_cmds = 1;
	flags = OE_PASSWD;
#ifdef _WINDOWS
	if(!ps_global->ttyo)
	  rc = os_login_dialog(mb, user, NETMAXUSER, pwd, NETMAXPASSWD, 0, 1);
	else
#endif
        rc = optionally_enter(pwd, q_line, 0, NETMAXPASSWD,
			      prompt, NULL, help, &flags);
	ps_global->dont_use_init_cmds = save_dont_use;

        if(rc == 3) {
            help = help == NO_HELP ? h_oe_passwd : NO_HELP;
        }
	else if(rc == 4){
	}
	else
          break;
    }

    if(rc == 1 || !pwd[0]) {
        user[0] = pwd[0] = '\0';
        return;
    }

 nopwpmt:
    /* remember the password for next time */
    if(F_OFF(F_DISABLE_PASSWORD_CACHING,ps_global))
      imap_set_passwd(&mm_login_list, pwd, user, hostlist,
		      (mb->sslflag||mb->tlsflag), 0, 0);
#ifdef	PASSFILE
    /* if requested, remember it on disk for next session */
    set_passfile_passwd(ps_global->pinerc, pwd, user, hostlist,
			(mb->sslflag||mb->tlsflag));
#endif
}



void
mm_login_alt_cue(mb)
    NETMBX *mb;
{
    if(ps_global->ttyo){
	COLOR_PAIR  *lastc;

	lastc = pico_set_colors(ps_global->VAR_TITLE_FORE_COLOR,
				ps_global->VAR_TITLE_BACK_COLOR,
				PSC_REV | PSC_RET);

	mark_titlebar_dirty();
	PutLine0(0, ps_global->ttyo->screen_cols - 1, 
		 (mb->sslflag||mb->tlsflag) ? "+" : " ");

	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}

	fflush(stdout);
    }
}




/*----------------------------------------------------------------------
       Receive notification of an error writing to disk
      
  Args: stream  -- The stream the error occured on
        errcode -- The system error code (errno)
        serious -- Flag indicating error is serious (mail may be lost)

Result: If error is non serious, the stream is marked as having an error
        and deletes are disallowed until error clears
        If error is serious this goes modal, allowing the user to retry
        or get a shell escape to fix the condition. When the condition is
        serious it means that mail existing in the mailbox will be lost
        if Pine exits without writing, so we try to induce the user to 
        fix the error, go get someone that can fix the error, or whatever
        and don't provide an easy way out.
  ----*/
long
mm_diskerror (stream, errcode, serious)
    MAILSTREAM *stream;
    long        errcode;
    long        serious;
{
    int  i, j;
    char *p, *q, *s;
    static ESCKEY_S de_opts[] = {
	{'r', 'r', "R", "Retry"},
	{'f', 'f', "F", "FileBrowser"},
	{'s', 's', "S", "ShellPrompt"},
	{-1, 0, NULL, NULL}
    };
#define	DE_COLS		(ps_global->ttyo->screen_cols)
#define	DE_LINE		(ps_global->ttyo->screen_rows - 3)

#define	DE_FOLDER(X)	(((X) && (X)->mailbox) ? (X)->mailbox : "<no folder>")
#define	DE_PMT	\
   "Disk error!  Choose Retry, or the File browser or Shell to clean up: "
#define	DE_STR1		"SERIOUS DISK ERROR WRITING: \"%s\""
#define	DE_STR2	\
   "The reported error number is %s.  The last reported mail error was:"
    static char *de_msg[] = {
	"Please try to correct the error preventing Pine from saving your",
	"mail folder.  For example if the disk is out of space try removing",
	"unneeded files.  You might also contact your system administrator.",
	"",
	"Both Pine's File Browser and an option to enter the system's",
	"command prompt are offered to aid in fixing the problem.  When",
	"you believe the problem is resolved, choose the \"Retry\" option.",
	"Be aware that messages may be lost or this folder left in an",
	"inaccessible condition if you exit or kill Pine before the problem",
	"is resolved.",
	NULL};
    static char *de_shell_msg[] = {
	"\n\nPlease attempt to correct the error preventing saving of the",
	"mail folder.  If you do not know how to correct the problem, contact",
	"your system administrator.  To return to Pine, type \"exit\".",
	NULL};

    dprint(0, (debugfile,
       "\n***** DISK ERROR on stream %s. Error code %ld. Error is %sserious\n",
	       DE_FOLDER(stream), errcode, serious ? "" : "not "));
    dprint(0, (debugfile, "***** message: \"%s\"\n\n",
	   ps_global->last_error ? ps_global->last_error : "?"));

    if(!serious) {
	sp_set_io_error_on_stream(stream, 1);
        return (1) ;
    }

    while(1){
	/* replace pine's body display with screen full of explanatory text */
	ClearLine(2);
	PutLine1(2, max((DE_COLS - sizeof(DE_STR1)
					    - strlen(DE_FOLDER(stream)))/2, 0),
		 DE_STR1, DE_FOLDER(stream));
	ClearLine(3);
	PutLine1(3, 4, DE_STR2, long2string(errcode));
	     
	PutLine0(4, 0, "       \"");
	removing_leading_white_space(ps_global->last_error);
	for(i = 4, p = ps_global->last_error; *p && i < DE_LINE; ){
	    for(s = NULL, q = p; *q && q - p < DE_COLS - 16; q++)
	      if(isspace((unsigned char)*q))
		s = q;

	    if(*q && s)
	      q = s;

	    while(p < q)
	      Writechar(*p++, 0);

	    if(*(p = q)){
		ClearLine(++i);
		PutLine0(i, 0, "        ");
		while(*p && isspace((unsigned char)*p))
		  p++;
	    }
	    else{
		Writechar('\"', 0);
		CleartoEOLN();
		break;
	    }
	}

	ClearLine(++i);
	for(j = ++i; i < DE_LINE && de_msg[i-j]; i++){
	    ClearLine(i);
	    PutLine0(i, 0, "  ");
	    Write_to_screen(de_msg[i-j]);
	}

	while(i < DE_LINE)
	  ClearLine(i++);

	switch(radio_buttons(DE_PMT, -FOOTER_ROWS(ps_global), de_opts,
			     'r', 0, NO_HELP, RB_FLUSH_IN | RB_NO_NEWMAIL,
			     NULL)){
	  case 'r' :				/* Retry! */
	    ps_global->mangled_screen = 1;
	    return(0L);

	  case 'f' :				/* File Browser */
	    {
		char full_filename[MAXPATH+1], filename[MAXPATH+1];

		filename[0] = '\0';
		build_path(full_filename, ps_global->home_dir, filename,
			   sizeof(full_filename));
		file_lister("DISK ERROR", full_filename, MAXPATH+1, 
                             filename, MAXPATH+1, FALSE, FB_SAVE);
	    }

	    break;

	  case 's' :
	    EndInverse();
	    end_keyboard(ps_global ? F_ON(F_USE_FK,ps_global) : 0);
	    end_tty_driver(ps_global);
	    for(i = 0; de_shell_msg[i]; i++)
	      puts(de_shell_msg[i]);

	    /*
	     * Don't use our piping mechanism to spawn a subshell here
	     * since it will the server (thus reentering c-client).
	     * Bad thing to do.
	     */
#ifdef	_WINDOWS
#else
	    system("csh");
#endif
	    init_tty_driver(ps_global);
	    init_keyboard(F_ON(F_USE_FK,ps_global));
	    break;
	}

	if(ps_global->redrawer)
	  (*ps_global->redrawer)();
    }
}


void
mm_fatal(message)
    char *message;
{
    panic(message);
}


void
mm_flags(stream, rawno)
    MAILSTREAM    *stream;
    unsigned long  rawno;
{
    /*
     * The idea here is to clean up any data pine might have cached
     * that has anything to do with the indicated message number.
     */
    if(stream == ps_global->mail_stream){
	long        msgno, t;
	PINETHRD_S *thrd;

	if(scores_are_used(SCOREUSE_GET) & SCOREUSE_STATEDEP)
	  clear_msg_score(stream, rawno);

	msgno = mn_raw2m(sp_msgmap(stream), (long) rawno);

	/* if in thread index */
	if(THRD_INDX()){
	    if((thrd = fetch_thread(stream, rawno))
	       && thrd->top
	       && (thrd = fetch_thread(stream, thrd->top))
	       && thrd->rawno
	       && (t = mn_raw2m(sp_msgmap(stream), thrd->rawno)))
	      clear_index_cache_ent(t);
	}
	else if(THREADING()){
	    if(msgno > 0L)
	      clear_index_cache_ent(msgno);

	    /*
	     * If a parent is collapsed, clear that parent's
	     * index cache entry.
	     */
	    if((thrd = fetch_thread(stream, rawno)) && thrd->parent){
		thrd = fetch_thread(stream, thrd->parent);
		while(thrd){
		    if(get_lflag(stream, NULL, thrd->rawno, MN_COLL)
		       && (t = mn_raw2m(sp_msgmap(stream), (long) thrd->rawno)))
		      clear_index_cache_ent(t);

		    if(thrd->parent)
		      thrd = fetch_thread(stream, thrd->parent);
		    else
		      thrd = NULL;
		}
	    }
	}
	else if(msgno > 0L)
	  clear_index_cache_ent(msgno);

	if(msgno && mn_is_cur(sp_msgmap(stream), msgno))
	  ps_global->mangled_header = 1;
    }
	    
    /*
     * We count up flag changes here. The
     * dont_count_flagchanges variable tries to prevent us from
     * counting when we're just fetching flags.
     */
    if(!(ps_global->dont_count_flagchanges
	 && stream == ps_global->mail_stream)){
	int exbits;

	check_point_change(stream);

	/* we also note flag changes for filtering purposes */
	if(msgno_exceptions(stream, rawno, "0", &exbits, FALSE))
	  exbits |= MSG_EX_STATECHG;
	else
	  exbits = MSG_EX_STATECHG;

	msgno_exceptions(stream, rawno, "0", &exbits, TRUE);
    }
}


long
pine_mail_status(stream, mailbox, flags)
    MAILSTREAM *stream;
    char       *mailbox;
    long        flags;
{
    return(pine_mail_status_full(stream, mailbox, flags, NULL, NULL));
}


long
pine_mail_status_full(stream, mailbox, flags, uidvalidity, uidnext)
    MAILSTREAM    *stream;
    char          *mailbox;
    long           flags;
    unsigned long *uidvalidity, *uidnext;
{
    char        source[MAILTMPLEN], *target = NULL;
    long        ret;
    MAILSTATUS  cache, status;
    MAILSTREAM *ourstream = NULL;

    if(check_for_move_mbox(mailbox, source, sizeof(source), &target)){
	DRIVER *d;

	memset(&status, 0, sizeof(status));
	memset(&cache,  0, sizeof(cache));

	/* tell mm_status() to write partial return here */
	pine_cached_status = &status;

	flags |= (SA_UIDVALIDITY | SA_UIDNEXT | SA_MESSAGES);

	/* do status of destination */

	stream = sp_stream_get(target, SP_SAME);

	/* should never be news, don't worry about mulnewrsc flag*/
	if((ret = pine_mail_status_full(stream, target, flags, uidvalidity,
					uidnext))
	   && !status.recent){

	    /* do status of source */
	    pine_cached_status = &cache;
	    stream = sp_stream_get(source, SP_SAME);

	    if(!stream){
		DRIVER *d;

		if((d = mail_valid (NIL, source, (char *) NIL))
		   && !strcmp(d->name, "imap")){
		    long openflags =OP_HALFOPEN|OP_SILENT|SP_USEPOOL|SP_TEMPUSE;

		    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
		      openflags |= OP_TRYALT;

		    stream = pine_mail_open(NULL, source, openflags, NULL);
		    ourstream = stream;
		}
		else if(F_ON(F_ENABLE_MULNEWSRCS, ps_global)
			&& d && (!strucmp(d->name, "news")
				 || !strucmp(d->name, "nntp")))
		  flags |= SA_MULNEWSRC;

	    }

	    if(mail_status(stream, source, flags)){
		DRIVER *d;
		int     is_news = 0;

		/*
		 * If the target has recent messages, then we don't come
		 * through here. We just use the answer from the target.
		 *
		 * If not, then we leave the target results in "status" and
		 * run a mail_status on the source that puts its results
		 * in "cache". Combine the results from cache with the
		 * results that were already in status.
		 *
		 * We count all messages in the source mailbox as recent and
		 * unseen, even if they are not recent or unseen in the source,
		 * because they will be recent and unseen in the target
		 * when we open it. (Not quite true. It is possible that some
		 * messages from a POP server will end up seen instead
		 * of unseen.
		 * It is also possible that it is correct. If we add unseen, or
		 * if we add messages, we could get it wrong. As far as I
		 * can tell, Pine doesn't ever even use status.unseen, so it
		 * is currently academic anyway.)  Hubert 2003-03-05
		 * (Does now 2004-06-02 in next_folder.)
		 *
		 * However, we don't want to count all messages as recent if
		 * the source mailbox is NNTP or NEWS, because we won't be
		 * deleting those messages from the source.
		 * We only count recent.
		 *
		 * There are other cases that are trouble. One in particular
		 * is an IMAP-to-NNTP proxy, where the messages can't be removed
		 * from the mailbox but they can be deleted. If we count
		 * messages in the source as being recent and it turns out they
		 * were all deleted already, then we incorrectly say the folder
		 * has recent messages when it doesn't. We can recover from that
		 * case at some cost by actually opening the folder the first
		 * time if there are not recents, and then checking to see if
		 * everything is deleted. Subsequently, we store the uid values
		 * (which are returned by status) so that we can know if the
		 * mailbox changed or not. The problem being solved is that
		 * the TAB command indicates new messages in a folder when there
		 * really aren't any. An alternative would be to use the is_news
		 * half of the if-else in all cases. A problem with that is
		 * that there could be non-recent messages sitting in the
		 * source mailbox that we never discover. Hubert 2003-03-28
		 */

		if((d = mail_valid (NIL, source, (char *) NIL))
		   && (!strcmp(d->name, "nntp") || !strcmp(d->name, "news")))
		  is_news++;

		if(is_news && cache.flags & SA_RECENT){
		    status.messages += cache.recent;
		    status.recent   += cache.recent;
		    status.unseen   += cache.recent;
		    status.uidnext  += cache.recent;
		}
		else{
		    if(uidvalidity && *uidvalidity
		       && uidnext && *uidnext
		       && cache.flags & SA_UIDVALIDITY
		       && cache.uidvalidity == *uidvalidity
		       && cache.flags & SA_UIDNEXT
		       && cache.uidnext == *uidnext){
			; /* nothing changed in source mailbox */
		    }
		    else if(cache.flags & SA_RECENT && cache.recent){
			status.messages += cache.recent;
			status.recent   += cache.recent;
			status.unseen   += cache.recent;
			status.uidnext  += cache.recent;
		    }
		    else if(!(cache.flags & SA_MESSAGES) || cache.messages){
			long openflags = OP_SILENT | SP_USEPOOL | SP_TEMPUSE;
			long delete_count, not_deleted = 0L;

			/* Actually open it up and check */
			if(F_ON(F_PREFER_ALT_AUTH, ps_global))
			  openflags |= OP_TRYALT;

			if(!ourstream)
			  stream = NULL;

			if(ourstream
			   && !same_stream_and_mailbox(source, ourstream)){
			    pine_mail_close(ourstream);
			    ourstream = stream = NULL;
			}

			if(!stream){
			    stream = pine_mail_open(stream, source, openflags,
						    NULL);
			    ourstream = stream;
			}

			if(stream){
			    delete_count = count_flagged(stream, F_DEL);
			    not_deleted = stream->nmsgs - delete_count;
			}

			status.messages += not_deleted;
			status.recent   += not_deleted;
			status.unseen   += not_deleted;
			status.uidnext  += not_deleted;
		    }

		    if(uidvalidity && cache.flags & SA_UIDVALIDITY)
		      *uidvalidity = cache.uidvalidity;

		    if(uidnext && cache.flags & SA_UIDNEXT)
		      *uidnext = cache.uidnext;
		}
	    }
	}

	/*
	 * Do the regular mm_status callback which we've been intercepting
	 * into different locations above.
	 */
	pine_cached_status = NIL;
	if(ret)
	  mm_status(NULL, mailbox, &status);
    }
    else{
	if(!stream){
	    DRIVER *d;

	    if((d = mail_valid (NIL, mailbox, (char *) NIL))
	       && !strcmp(d->name, "imap")){
		long openflags = OP_HALFOPEN|OP_SILENT|SP_USEPOOL|SP_TEMPUSE;

		if(F_ON(F_PREFER_ALT_AUTH, ps_global))
		  openflags |= OP_TRYALT;

		/*
		 * We just use this to find the answer.
		 * We're asking for trouble if we do a STATUS on a
		 * selected mailbox. I don't believe this happens in pine.
		 * It does now (2004-06-02) in next_folder if the
		 * F_TAB_USES_UNSEEN option is set and the folder was
		 * already opened.
		 */
		stream = sp_stream_get(mailbox, SP_MATCH);
		if(stream){ 
		    memset(&status, 0, sizeof(status));
		    if(flags & SA_MESSAGES){
			status.flags |= SA_MESSAGES;
			status.messages = stream->nmsgs;
		    }

		    if(flags & SA_RECENT){
			status.flags |= SA_RECENT;
			status.recent = stream->recent;
		    }

		    if(flags & SA_UNSEEN){
		        long i;
			SEARCHPGM *srchpgm;
			MESSAGECACHE *mc;

			srchpgm = mail_newsearchpgm();
			srchpgm->unseen = 1;
		      
			pine_mail_search_full(stream, NULL, srchpgm,
					      SE_NOPREFETCH | SE_FREE);
			status.flags |= SA_UNSEEN;
			status.unseen = 0L;
			for(i = 1L; i <= stream->nmsgs; i++)
			  if((mc = mail_elt(stream, i)) && mc->searched)
			    status.unseen++;
		    }

		    if(flags & SA_UIDVALIDITY){
			status.flags |= SA_UIDVALIDITY;
			status.uidvalidity = stream->uid_validity;
		    }

		    if(flags & SA_UIDNEXT){
			status.flags |= SA_UIDNEXT;
			status.uidnext = stream->uid_last + 1L;
		    }

		    mm_status(NULL, mailbox, &status);
		    return T;  /* that's what c-client returns when success */
		}

		if(!stream)
		  stream = sp_stream_get(mailbox, SP_SAME);

		if(!stream){
		    stream = pine_mail_open(NULL, mailbox, openflags, NULL);
		    ourstream = stream;
		}
	    }
	    else if(F_ON(F_ENABLE_MULNEWSRCS, ps_global)
		    && d && (!strucmp(d->name, "news")
			     || !strucmp(d->name, "nntp")))
	      flags |= SA_MULNEWSRC;
	}

	ret = mail_status(stream, mailbox, flags);	/* non #move case */
    }

    if(ourstream)
      pine_mail_close(ourstream);

    return ret;
}


/*
 * Check for a mailbox name that is a legitimate #move mailbox.
 *
 * Args   mbox     -- The mailbox name to check
 *      sourcebuf  -- Copy the source mailbox name into this buffer
 *      sbuflen    -- Length of sourcebuf
 *      targetptr  -- Set the pointer this points to to point to the
 *                      target mailbox name in the original string
 *
 * Returns  1 - is a #move mailbox
 *          0 - not
 */
int
check_for_move_mbox(mbox, sourcebuf, sbuflen, targetptr)
    char  *mbox;
    char  *sourcebuf;
    size_t sbuflen;
    char **targetptr;
{
    char delim, *s;
    int  i;

    if(mbox && (mbox[0] == '#')
       && ((mbox[1] == 'M') || (mbox[1] == 'm'))
       && ((mbox[2] == 'O') || (mbox[2] == 'o'))
       && ((mbox[3] == 'V') || (mbox[3] == 'v'))
       && ((mbox[4] == 'E') || (mbox[4] == 'e'))
       && (delim = mbox[5])
       && (s = strchr(mbox+6, delim))
       && (i = s++ - (mbox + 6))
       && (!sourcebuf || i < sbuflen)){

	if(sourcebuf){
	    strncpy(sourcebuf, mbox+6, i);	/* copy source mailbox name */
	    sourcebuf[i] = '\0';
	}

	if(targetptr)
	  *targetptr = s;
	
	return 1;
    }

    return 0;
}


/*
 *
 */
void
mm_status(stream, mailbox, status)
    MAILSTREAM *stream;
    char       *mailbox;
    MAILSTATUS *status;
{
    /*
     * We implement mail_status for the #move namespace by adding a wrapper
     * routine, pine_mail_status. It may have to call the real mail_status
     * twice for #move folders and combine the results. It sets
     * pine_cached_status to point to a local status variable to store the
     * intermediate results.
     */
    if(pine_cached_status != NULL)
      *pine_cached_status = *status;
    else
      mm_status_result = *status;

#ifdef DEBUG
    if(status){
	if(pine_cached_status)
	  dprint(2, (debugfile,
		 "mm_status: Preliminary pass for #move\n"));

	dprint(2, (debugfile, "mm_status: Mailbox \"%s\"",
	       mailbox ? mailbox : "?"));
	if(status->flags & SA_MESSAGES)
	  dprint(2, (debugfile, ", %lu messages", status->messages));

	if(status->flags & SA_RECENT)
	  dprint(2, (debugfile, ", %lu recent", status->recent));

	if(status->flags & SA_UNSEEN)
	  dprint(2, (debugfile, ", %lu unseen", status->unseen));

	if(status->flags & SA_UIDVALIDITY)
	  dprint(2, (debugfile, ", %lu UID validity", status->uidvalidity));

	if(status->flags & SA_UIDNEXT)
	  dprint(2, (debugfile, ", %lu next UID", status->uidnext));

	dprint(2, (debugfile, "\n"));
    }
#endif
}


/*
 *
 */
void
mm_list(stream, delimiter, mailbox, attributes)
    MAILSTREAM *stream;
    int		delimiter;
    char       *mailbox;
    long	attributes;
{
#ifdef DEBUG
    if(ps_global->debug_imap > 2 || ps_global->debugmem)
      dprint(5,
              (debugfile, "mm_list \"%s\": delim: '%c', %s%s%s%s%s%s\n",
	       mailbox ? mailbox : "?", delimiter ? delimiter : 'X',
	       (attributes & LATT_NOINFERIORS) ? ", no inferiors" : "",
	       (attributes & LATT_NOSELECT) ? ", no select" : "",
	       (attributes & LATT_MARKED) ? ", marked" : "",
	       (attributes & LATT_UNMARKED) ? ", unmarked" : "",
	       (attributes & LATT_HASCHILDREN) ? ", has children" : "",
	       (attributes & LATT_HASNOCHILDREN) ? ", has no children" : ""));
#endif

    if(!mm_list_info->stream || stream == mm_list_info->stream)
      (*mm_list_info->filter)(stream, mailbox, delimiter,
			      attributes, mm_list_info->data,
			      mm_list_info->options);
}


/*
 *
 */
void
mm_lsub(stream, delimiter, mailbox, attributes)
  MAILSTREAM *stream;
  int	      delimiter;
  char	     *mailbox;
  long	      attributes;
{
#ifdef DEBUG
    if(ps_global->debug_imap > 2 || ps_global->debugmem)
      dprint(5,
              (debugfile, "LSUB \"%s\": delim: '%c', %s%s%s%s%s%s\n",
	       mailbox ? mailbox : "?", delimiter ? delimiter : 'X',
	       (attributes & LATT_NOINFERIORS) ? ", no inferiors" : "",
	       (attributes & LATT_NOSELECT) ? ", no select" : "",
	       (attributes & LATT_MARKED) ? ", marked" : "",
	       (attributes & LATT_UNMARKED) ? ", unmarked" : "",
	       (attributes & LATT_HASCHILDREN) ? ", has children" : "",
	       (attributes & LATT_HASNOCHILDREN) ? ", has no children" : ""));
#endif

    if(!mm_list_info->stream || stream == mm_list_info->stream)
      (*mm_list_info->filter)(stream, mailbox, delimiter,
			      attributes, mm_list_info->data,
			      mm_list_info->options);
}


long
pine_tcptimeout_noscreen(elapsed, sincelast)
    long elapsed, sincelast;
{
    long rv = 1L;
    char pmt[128];

#ifdef _WINDOWS
    mswin_killsplash();
#endif

    if(elapsed >= (long)ps_global->tcp_query_timeout){
	sprintf(pmt,
	 "Waited %s seconds for server reply.  Break connection to server",
		long2string(elapsed));
	if(want_to(pmt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y')
	  return(0L);
    }

    return(rv);
}

/*
 * pine_tcptimeout - C-client callback to handle tcp-related timeouts.
 */
long
pine_tcptimeout(elapsed, sincelast)
    long elapsed, sincelast;
{
    long rv = 1L;			/* keep trying by default */
    int	 ch;

#ifdef	DEBUG
    dprint(1, (debugfile, "tcptimeout: waited %s seconds\n",
	       long2string(elapsed)));
    if(debugfile)
      fflush(debugfile);
#endif

#ifdef _WINDOWS
    mswin_killsplash();
#endif

    if(ps_global->noshow_timeout)
      return(rv);

    if(!ps_global->ttyo)
      return(pine_tcptimeout_noscreen(elapsed, sincelast));

    suspend_busy_alarm();
    
    /*
     * Prompt after a minute (since by then things are probably really bad)
     * A prompt timeout means "keep trying"...
     */
    if(elapsed >= (long)ps_global->tcp_query_timeout){
	int clear_inverse;

	ClearLine(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global));
	if(clear_inverse = !InverseState())
	  StartInverse();

	Writechar(BELL, 0);

	PutLine1(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global), 0,
       "Waited %s seconds for server reply.  Break connection to server? ",
	   long2string(elapsed));
	CleartoEOLN();
	fflush(stdout);
	flush_input();
	ch = read_char(7);
	if(ch == 'y' || ch == 'Y')
	  rv = 0L;

	if(clear_inverse)
	  EndInverse();

	ClearLine(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global));
    }

    if(rv == 1L){			/* just warn 'em something's up */
	q_status_message1(SM_ORDER, 0, 0,
		  "Waited %s seconds for server reply.  Still Waiting...",
		  long2string(elapsed));
	flush_status_messages(0);	/* make sure it's seen */
    }

    mark_status_dirty();		/* make sure it get's cleared */

    resume_busy_alarm((rv == 1) ? 3 : 0);

    return(rv);
}


/*
 * C-client callback to handle SSL/TLS certificate validation failures
 *
 * Returning 0 means error becomes fatal
 *           Non-zero means certificate problem is ignored and SSL session is
 *             established
 *
 *  We remember the answer and won't re-ask for subsequent open attempts to
 *  the same hostname.
 */
long
pine_sslcertquery(reason, host, cert)
    char *reason, *host, *cert;
{
    char      tmp[500];
    char     *unknown = "<unknown>";
    long      rv = 0L;
    STRLIST_S hostlist;
    int       ok_novalidate = 0, warned = 0;

    dprint(1, (debugfile, "sslcertificatequery: host=%s reason=%s cert=%s\n",
			   host ? host : "?", reason ? reason : "?",
			   cert ? cert : "?"));

    hostlist.name = host ? host : "";
    hostlist.next = NULL;

    /*
     * See if we've been asked about this host before.
     */
    if(imap_get_ssl(cert_failure_list, &hostlist, &ok_novalidate, &warned)){
	/* we were asked before, did we say Yes? */
	if(ok_novalidate)
	  rv++;

	if(rv){
	    dprint(5, (debugfile,
		       "sslcertificatequery: approved automatically\n"));
	    return(rv);
	}

	dprint(1, (debugfile, "sslcertificatequery: we were asked before and said No, so ask again\n"));
    }

    if(ps_global->ttyo){
	static struct key ans_certquery_keys[] =
	   {HELP_MENU,
	    NULL_MENU,
	    {"Y","Yes, continue",{MC_YES,1,{'y'}},KS_NONE},
	    {"D","[Details]",{MC_VIEW_HANDLE,3,{'d',ctrl('M'),ctrl('J')}},KS_NONE},
	    {"N","No",{MC_NO,1,{'n'}},KS_NONE},
	    NULL_MENU,
	    PREVPAGE_MENU,
	    NEXTPAGE_MENU,
	    PRYNTTXT_MENU,
	    WHEREIS_MENU,
	    FWDEMAIL_MENU,
	    {"S", "Save", {MC_SAVETEXT,1,{'s'}}, KS_SAVE}};
	INST_KEY_MENU(ans_certquery_keymenu, ans_certquery_keys);
	SCROLL_S  sargs;
	STORE_S  *in_store, *out_store;
	gf_io_t   pc, gc;
	HANDLE_S *handles = NULL;
	int       the_answer = 'n';

	if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS)) ||
	   !(out_store = so_get(CharStar, NULL, EDIT_ACCESS)))
	  goto try_wantto;

	so_puts(in_store, "<HTML><P>There was a failure validating the SSL/TLS certificate for the server");

	so_puts(in_store, "<P><CENTER>");
	so_puts(in_store, host ? host : unknown);
	so_puts(in_store, "</CENTER>");

	so_puts(in_store, "<P>The reason for the failure was");

	/* squirrel away details */
	if(details_host)
	  fs_give((void **)&details_host);
	if(details_reason)
	  fs_give((void **)&details_reason);
	if(details_cert)
	  fs_give((void **)&details_cert);
	
	details_host   = cpystr(host ? host : unknown);
	details_reason = cpystr(reason ? reason : unknown);
	details_cert   = cpystr(cert ? cert : unknown);

	so_puts(in_store, "<P><CENTER>");
	sprintf(tmp, "%.300s (<A HREF=\"X-Pine-Cert:\">details</A>)",
		reason ? reason : unknown);

	so_puts(in_store, tmp);
	so_puts(in_store, "</CENTER>");

	so_puts(in_store, "<P>We have not verified the identity of your server. If you ignore this certificate validation problem and continue, you could end up connecting to an imposter server.");

	so_puts(in_store, "<P>If the certificate validation failure was expected and permanent you may avoid seeing this warning message in the future by adding the option");

	so_puts(in_store, "<P><CENTER>");
	so_puts(in_store, "/novalidate-cert");
	so_puts(in_store, "</CENTER>");

	so_puts(in_store, "<P>to the name of the folder you attempted to access. In other words, wherever you see the characters");

	so_puts(in_store, "<P><CENTER>");
	so_puts(in_store, host ? host : unknown);
	so_puts(in_store, "</CENTER>");

	so_puts(in_store, "<P>in your configuration, replace those characters with");

	so_puts(in_store, "<P><CENTER>");
	so_puts(in_store, host ? host : unknown);
	so_puts(in_store, "/novalidate-cert");
	so_puts(in_store, "</CENTER>");

	so_puts(in_store, "<P>Answer \"Yes\" to ignore the warning and continue, \"No\" to cancel the open of this folder.");

	so_seek(in_store, 0L, 0);
	init_handles(&handles);
	gf_filter_init();
	gf_link_filter(gf_html2plain,
		       gf_html2plain_opt(NULL,
					 ps_global->ttyo->screen_cols, NULL,
					 &handles, GFHP_LOCAL_HANDLES));
	gf_set_so_readc(&gc, in_store);
	gf_set_so_writec(&pc, out_store);
	gf_pipe(gc, pc);
	gf_clear_so_writec(out_store);
	gf_clear_so_readc(in_store);

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.handles  = handles;
	sargs.text.text     = so_text(out_store);
	sargs.text.src      = CharStar;
	sargs.text.desc     = "help text";
	sargs.bar.title     = "SSL/TLS CERTIFICATE VALIDATION FAILURE";
	sargs.proc.tool     = answer_cert_failure;
	sargs.proc.data.p   = (void *)&the_answer;
	sargs.keys.menu     = &ans_certquery_keymenu;
	/* don't want to re-enter c-client */
	sargs.quell_newmail = 1;
	setbitmap(sargs.keys.bitmap);
	sargs.help.text     = h_tls_validation_failure;
	sargs.help.title    = "HELP FOR CERT VALIDATION FAILURE";

	scrolltool(&sargs);

	if(the_answer == 'y')
	  rv++;

	ps_global->mangled_screen = 1;
	ps_global->painted_body_on_startup = 0;
	ps_global->painted_footer_on_startup = 0;
	so_give(&in_store);
	so_give(&out_store);
	free_handles(&handles);
	if(details_host)
	  fs_give((void **)&details_host);
	if(details_reason)
	  fs_give((void **)&details_reason);
	if(details_cert)
	  fs_give((void **)&details_cert);
    }
    else{
	/*
	 * If screen hasn't been initialized yet, use want_to.
	 */
try_wantto:
	memset((void *)tmp, 0, sizeof(tmp));
	strncpy(tmp,
		reason ? reason : "SSL/TLS certificate validation failure",
		sizeof(tmp)/2);
	strncat(tmp, ": Continue anyway ", sizeof(tmp)/2);

	if(want_to(tmp, 'n', 'x', NO_HELP, WT_NORM) == 'y')
	  rv++;
    }

    if(rv == 0)
      q_status_message1(SM_ORDER, 1, 3, "Open of %.200s cancelled",
			host ? host : unknown);

    imap_set_passwd(&cert_failure_list, "", "", &hostlist, 0, rv ? 1 : 0, 0);

    dprint(5, (debugfile, "sslcertificatequery: %s\n",
			   rv ? "approved" : "rejected"));

    return(rv);
}

char *
pine_newsrcquery(MAILSTREAM *stream, char *mulname, char *name)
{
    char buf[MAILTMPLEN];

    if((can_access(mulname, ACCESS_EXISTS) == 0)
       || !(can_access(name, ACCESS_EXISTS) == 0))
      return(mulname);

    sprintf(buf,
	    "Rename newsrc \"%.15s%s\" for use as new host-specific newsrc",
	    last_cmpnt(name), 
	    strlen(last_cmpnt(name)) > 15 ? "..." : "");
    if(want_to(buf, 'n', 'n', NO_HELP, WT_NORM) == 'y')
      rename_file(name, mulname);
    return(mulname);
}

int
url_local_certdetails(url)
    char *url;
{
    if(!struncmp(url, "x-pine-cert:", 12)){
	STORE_S  *store;
	SCROLL_S  sargs;
	char     *folded;

	if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    q_status_message(SM_ORDER | SM_DING, 7, 10,
			     "Error allocating space for details.");
	    return(0);
	}

	so_puts(store, "Host given by user:\n\n  ");
	so_puts(store, details_host);
	so_puts(store, "\n\nReason for failure:\n\n  ");
	so_puts(store, details_reason);
	so_puts(store, "\n\nCertificate being verified:\n\n");
	folded = fold(details_cert, ps_global->ttyo->screen_cols, ps_global->ttyo->screen_cols, 0, 0, "  ", "  ");
	so_puts(store, folded);
	fs_give((void **)&folded);
	so_puts(store, "\n");

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text     = so_text(store);
	sargs.text.src      = CharStar;
	sargs.text.desc     = "Details";
	sargs.bar.title     = "CERT VALIDATION DETAILS";
	sargs.help.text     = NO_HELP;
	sargs.help.title    = NULL;
	sargs.quell_newmail = 1;
	sargs.help.text     = h_tls_failure_details;
	sargs.help.title    = "HELP FOR CERT VALIDATION DETAILS";

	scrolltool(&sargs);

	so_give(&store);	/* free resources associated with store */
	ps_global->mangled_screen = 1;
	return(1);
    }

    return(0);
}


/*
 * C-client callback to handle SSL/TLS certificate validation failures
 */
void
pine_sslfailure(host, reason, flags)
    char *host, *reason;
    unsigned long flags;
{
    static struct key ans_certfail_keys[] =
       {NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"C","[Continue]",{MC_YES,3,{'c',ctrl('J'),ctrl('M')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
    INST_KEY_MENU(ans_certfail_keymenu, ans_certfail_keys);
    SCROLL_S sargs;
    STORE_S *store;
    int      the_answer = 'n', indent, len, cols;
    char     buf[500], buf2[500];
    char    *folded;
    char    *hst = host ? host : "<unknown>";
    char    *rsn = reason ? reason : "<unknown>";
    char    *notls = "/notls";
    STRLIST_S hostlist;
    int       ok_novalidate = 0, warned = 0;


    dprint(1, (debugfile, "sslfailure: host=%s reason=%s\n",
	   hst ? hst : "?",
	   rsn ? rsn : "?"));

    if(flags & NET_SILENT)
      return;

    hostlist.name = host ? host : "";
    hostlist.next = NULL;

    /*
     * See if we've been told about this host before.
     */
    if(imap_get_ssl(cert_failure_list, &hostlist, &ok_novalidate, &warned)){
	/* we were told already */
	if(warned){
	    sprintf(buf, "SSL/TLS failure for %.80s: %.300s", hst, rsn);
	    mm_log(buf, ERROR);
	    return;
	}
    }

    cols = ps_global->ttyo ? ps_global->ttyo->screen_cols : 80;
    cols--;

    if(!(store = so_get(CharStar, NULL, EDIT_ACCESS)))
      return;

    strncpy(buf, "There was an SSL/TLS failure for the server", sizeof(buf));
    folded = fold(buf, cols, cols, 0, 0, "", "");
    so_puts(store, folded);
    fs_give((void **)&folded);
    so_puts(store, "\n");

    if((len=strlen(hst)) <= cols){
	if((indent=((cols-len)/2)) > 0)
	  so_puts(store, repeat_char(indent, SPACE));

	so_puts(store, hst);
	so_puts(store, "\n");
    }
    else{
	strncpy(buf, hst, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	folded = fold(buf, cols, cols, 0, 0, "", "");
	so_puts(store, folded);
	fs_give((void **)&folded);
    }

    so_puts(store, "\n");

    strncpy(buf, "The reason for the failure was", sizeof(buf));
    folded = fold(buf, cols, cols, 0, 0, "", "");
    so_puts(store, folded);
    fs_give((void **)&folded);
    so_puts(store, "\n");

    if((len=strlen(rsn)) <= cols){
	if((indent=((cols-len)/2)) > 0)
	  so_puts(store, repeat_char(indent, SPACE));

	so_puts(store, rsn);
	so_puts(store, "\n");
    }
    else{
	strncpy(buf, rsn, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	folded = fold(buf, cols, cols, 0, 0, "", "");
	so_puts(store, folded);
	fs_give((void **)&folded);
    }

    so_puts(store, "\n");

    strncpy(buf, "This is just an informational message. With the current setup, SSL/TLS will not work. If this error re-occurs every time you run Pine, your current setup is not compatible with the configuration of your mail server. You may want to add the option", sizeof(buf));
    folded = fold(buf, cols, cols, 0, 0, "", "");
    so_puts(store, folded);
    fs_give((void **)&folded);
    so_puts(store, "\n");

    if((len=strlen(notls)) <= cols){
	if((indent=((cols-len)/2)) > 0)
	  so_puts(store, repeat_char(indent, SPACE));

	so_puts(store, notls);
	so_puts(store, "\n");
    }
    else{
	strncpy(buf, notls, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	folded = fold(buf, cols, cols, 0, 0, "", "");
	so_puts(store, folded);
	fs_give((void **)&folded);
    }

    so_puts(store, "\n");

    strncpy(buf, "to the name of the mail server you are attempting to access. In other words, wherever you see the characters",
	    sizeof(buf));
    folded = fold(buf, cols, cols, 0, 0, "", "");
    so_puts(store, folded);
    fs_give((void **)&folded);
    so_puts(store, "\n");

    if((len=strlen(hst)) <= cols){
	if((indent=((cols-len)/2)) > 0)
	  so_puts(store, repeat_char(indent, SPACE));

	so_puts(store, hst);
	so_puts(store, "\n");
    }
    else{
	strncpy(buf, hst, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	folded = fold(buf, cols, cols, 0, 0, "", "");
	so_puts(store, folded);
	fs_give((void **)&folded);
    }

    so_puts(store, "\n");

    strncpy(buf, "in your configuration, replace those characters with",
	    sizeof(buf));
    folded = fold(buf, cols, cols, 0, 0, "", "");
    so_puts(store, folded);
    fs_give((void **)&folded);
    so_puts(store, "\n");

    sprintf(buf2, "%.100s%.100s", hst, notls);
    if((len=strlen(buf2)) <= cols){
	if((indent=((cols-len)/2)) > 0)
	  so_puts(store, repeat_char(indent, SPACE));

	so_puts(store, buf2);
	so_puts(store, "\n");
    }
    else{
	strncpy(buf, buf2, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	folded = fold(buf, cols, cols, 0, 0, "", "");
	so_puts(store, folded);
	fs_give((void **)&folded);
    }

    so_puts(store, "\n");

    if(ps_global->ttyo){
	strncpy(buf, "Type RETURN to continue.", sizeof(buf));
	folded = fold(buf, cols, cols, 0, 0, "", "");
	so_puts(store, folded);
	fs_give((void **)&folded);
    }

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text = so_text(store);
    sargs.text.src  = CharStar;
    sargs.text.desc = "help text";
    sargs.bar.title = "SSL/TLS FAILURE";
    sargs.proc.tool = answer_cert_failure;
    sargs.proc.data.p = (void *)&the_answer;
    sargs.keys.menu = &ans_certfail_keymenu;
    setbitmap(sargs.keys.bitmap);
    /* don't want to re-enter c-client */
    sargs.quell_newmail = 1;
    sargs.help.text     = h_tls_failure;
    sargs.help.title    = "HELP FOR TLS/SSL FAILURE";

    if(ps_global->ttyo)
      scrolltool(&sargs);
    else{
	char **q, **qp;
	char  *p;
	unsigned char c;
	int    cnt = 0;
	
	/*
	 * The screen isn't initialized yet, which should mean that this
	 * is the result of a -p argument. Display_args_err knows how to deal
	 * with the uninitialized screen, so we mess with the data to get it
	 * in shape for display_args_err. This is pretty hacky.
	 */
	
	so_seek(store, 0L, 0);		/* rewind */
	/* count the lines */
	while(so_readc(&c, store))
	  if(c == '\n')
	    cnt++;
	
	qp = q = (char **)fs_get((cnt+1) * sizeof(char *));
	memset(q, 0, (cnt+1) * sizeof(char *));

	so_seek(store, 0L, 0);		/* rewind */
	p = buf;
	while(so_readc(&c, store)){
	    if(c == '\n'){
		*p = '\0';
		*qp++ = cpystr(buf);
		p = buf;
	    }
	    else
	      *p++ = c;
	}

	display_args_err(NULL, q, 0);
	free_list_array(&q);
    }

    ps_global->mangled_screen = 1;
    ps_global->painted_body_on_startup = 0;
    ps_global->painted_footer_on_startup = 0;
    so_give(&store);

    imap_set_passwd(&cert_failure_list, "", "", &hostlist, 0, ok_novalidate, 1);
}


int
answer_cert_failure(cmd, msgmap, sparms)
    int	       cmd;
    MSGNO_S   *msgmap;
    SCROLL_S  *sparms;
{
    int rv = 1;

    ps_global->next_screen = SCREEN_FUN_NULL;

    switch(cmd){
      case MC_YES :
	*(int *)(sparms->proc.data.p) = 'y';
	break;

      case MC_NO :
	*(int *)(sparms->proc.data.p) = 'n';
	break;

      default:
	panic("Unexpected command in answer_cert_failure");
	break;
    }

    return(rv);
}


char *
imap_referral(stream, ref, code)
    MAILSTREAM *stream;
    char       *ref;
    long	code;
{
    char *buf = NULL;

    if(ref && !struncmp(ref, "imap://", 7)){
	char *folder = NULL;
	long  uid_val, uid;
	int rv;

	rv = url_imap_folder(ref, &folder, &uid, &uid_val, NULL, 1);
	switch(code){
	  case REFAUTHFAILED :
	  case REFAUTH :
	    if((rv & URL_IMAP_IMAILBOXLIST) && (rv & URL_IMAP_ISERVERONLY))
	      buf = cpystr(folder);

	    break;

	  case REFSELECT :
	  case REFCREATE :
	  case REFDELETE :
	  case REFRENAME :
	  case REFSUBSCRIBE :
	  case REFUNSUBSCRIBE :
	  case REFSTATUS :
	  case REFCOPY :
	  case REFAPPEND :
	    if(rv & URL_IMAP_IMESSAGELIST)
	      buf = cpystr(folder);

	    break;

	  default :
	    break;
	}

	if(folder)
	  fs_give((void **) &folder);
    }

    return(buf);
}


long
imap_proxycopy(stream, sequence, mailbox, flags)
    MAILSTREAM *stream;
    char       *sequence, *mailbox;
    long	flags;
{
    SE_APP_S args;

    args.folder = mailbox;
    args.flags  = flags;

    return(imap_seq_exec(stream, sequence, imap_seq_exec_append, &args));
}


long
imap_seq_exec(stream, sequence, func, args)
    MAILSTREAM	*stream;
    char	*sequence;
    long       (*func) PROTO((MAILSTREAM *, long, void *));
    void	*args;
{
    unsigned long i,j,x;
    MESSAGECACHE *mc;

    while (*sequence) {		/* while there is something to parse */
	if (*sequence == '*') {	/* maximum message */
	    if(!(i = stream->nmsgs)){
		mm_log ("No messages, so no maximum message number",ERROR);
		return(0L);
	    }

	    sequence++;		/* skip past * */
	}
	else if (!(i = strtoul ((const char *) sequence,&sequence,10))
		 || (i > stream->nmsgs)){
	    mm_log ("Sequence invalid",ERROR);
	    return(0L);
	}

	switch (*sequence) {	/* see what the delimiter is */
	  case ':':			/* sequence range */
	    if (*++sequence == '*') {	/* maximum message */
		if (stream->nmsgs) j = stream->nmsgs;
		else {
		    mm_log ("No messages, so no maximum message number",ERROR);
		    return NIL;
		}

		sequence++;		/* skip past * */
	    }
				/* parse end of range */
	    else if (!(j = strtoul ((const char *) sequence,&sequence,10)) ||
		     (j > stream->nmsgs)) {
		mm_log ("Sequence range invalid",ERROR);
		return NIL;
	    }

	    if (*sequence && *sequence++ != ',') {
		mm_log ("Sequence range syntax error",ERROR);
		return NIL;
	    }

	    if (i > j) {		/* swap the range if backwards */
		x = i; i = j; j = x;
	    }
				/* mark each item in the sequence */
	    while (i <= j)
	      if(!(*func)(stream, j--, args)){
		  if(j > 0L && stream && j <= stream->nmsgs
		     && (mc = mail_elt(stream, j)))
		    mc->sequence = T;

		  return(0L);
	      }

	    break;
	  case ',':			/* single message */
	    ++sequence;		/* skip the delimiter, fall into end case */
	  case '\0':		/* end of sequence, mark this message */
	    if(!(*func)(stream, i, args)){
		if(i > 0L && stream && i <= stream->nmsgs
		   && (mc = mail_elt(stream, i)))
		  mc->sequence = T;

		return(0L);
	    }

	    break;
	  default:			/* anything else is a syntax error! */
	    mm_log ("Sequence syntax error",ERROR);
	    return NIL;
	}
    }

    return T;			/* successfully parsed sequence */
}


long
imap_seq_exec_append(stream, msgno, args)
    MAILSTREAM *stream;
    long	msgno;
    void       *args;
{
    char	 *save_folder, flags[64], date[64];
    CONTEXT_S    *cntxt = NULL;
    int		  our_stream = 0;
    long	  rv = 0L;
    MAILSTREAM   *save_stream;
    SE_APP_S	 *sa = (SE_APP_S *) args;
    MESSAGECACHE *mc;
    STORE_S      *so;

    save_folder = (strucmp(sa->folder, ps_global->inbox_name) == 0)
		    ? ps_global->VAR_INBOX_PATH : sa->folder;

    save_stream = save_msg_stream(cntxt, save_folder, &our_stream);

    if(so = so_get(CharStar, NULL, WRITE_ACCESS)){
	/* store flags before the fetch so UNSEEN bit isn't flipped */
	mc = (msgno > 0L && stream && msgno <= stream->nmsgs)
		? mail_elt(stream, msgno) : NULL;

	flag_string(mc, F_ANS|F_FLAG|F_SEEN, flags);
	if(mc && mc->day)
	  mail_date(date, mc);
	else
	  *date = '\0';

	rv = save_fetch_append(stream, msgno, NULL,
			       save_stream, save_folder, NULL,
			       mc ? mc->rfc822_size : 0L, flags, date, so);
	if(rv < 0 || sp_expunge_count(stream)){
	    cmd_cancelled("Attached message Save");
	    rv = 0L;
	}
	/* else whatever broke in save_fetch_append shoulda bitched */

	so_give(&so);
    }
    else{
	dprint(1, (debugfile, "Can't allocate store for save: %s\n",
		   error_description(errno)));
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Problem creating space for message text.");
    }

    if(our_stream)
      mail_close(save_stream);

    return(rv);
}


/*----------------------------------------------------------------------
  This can be used to prevent the flickering of the check_cue char
  caused by numerous (5000+) fetches by c-client.  Right now, the only
  practical use found is newsgroup subsciption.

  check_cue_display will check if this global is set, and won't clear
  the check_cue_char if set.
  ----*/
void
set_read_predicted(i)
     int i;
{
    ps_global->read_predicted = i==1;
#ifdef _WINDOWS
    if(!i && F_ON(F_SHOW_DELAY_CUE, ps_global))
      check_cue_display("  ");
#endif
}


/*----------------------------------------------------------------------
   Exported method to retrieve logged in user name associated with stream

   Args: host -- host to find associated login name with.

 Result: 
  ----*/
void *
pine_block_notify(reason, data)
    int   reason;
    void *data;
{
    switch(reason){
      case BLOCK_SENSITIVE:		/* sensitive code, disallow alarms */
	return(alrm_signal_block());
	break;

      case BLOCK_NONSENSITIVE:		/* non-sensitive code, allow alarms */
	alrm_signal_unblock(data);
	break;

      case BLOCK_TCPWRITE:		/* blocked on TCP write */
      case BLOCK_FILELOCK:		/* blocked on file locking */
#ifdef	_WINDOWS
	if(F_ON(F_SHOW_DELAY_CUE, ps_global))
	  check_cue_display(">");

	mswin_setcursor(MSWIN_CURSOR_BUSY);
#endif
	break;

      case BLOCK_DNSLOOKUP:		/* blocked on DNS lookup */
      case BLOCK_TCPOPEN:		/* blocked on TCP open */
      case BLOCK_TCPREAD:		/* blocked on TCP read */
      case BLOCK_TCPCLOSE:		/* blocked on TCP close */
#ifdef	_WINDOWS
	if(F_ON(F_SHOW_DELAY_CUE, ps_global))
	  check_cue_display("<");

	mswin_setcursor(MSWIN_CURSOR_BUSY);
#endif
	break;

      default :
      case BLOCK_NONE:			/* not blocked */
#ifdef	_WINDOWS
	if(F_ON(F_SHOW_DELAY_CUE, ps_global))
	  check_cue_display(" ");
#endif
	break;

    }

    return(NULL);
}



/*----------------------------------------------------------------------
   Exported method to retrieve logged in user name associated with stream

   Args: host -- host to find associated login name with.

 Result: 
  ----*/
char *
cached_user_name(host)
    char *host;
{
    MMLOGIN_S *l;
    STRLIST_S *h;

    if((l = mm_login_list) && host)
      do
	for(h = l->hosts; h; h = h->next)
	  if(!strucmp(host, h->name))
	    return(l->user);
      while(l = l->next);

    return(NULL);
}


int
imap_same_host(hl1, hl2)
    STRLIST_S *hl1, *hl2;
{
    STRLIST_S *lp;

    for( ; hl1; hl1 = hl1->next)
      for(lp = hl2; lp; lp = lp->next)
      if(!strucmp(hl1->name, lp->name))
	return(TRUE);

    return(FALSE);
}


/*
 * For convenience, we use the same m_list structure (but a different
 * instance) for storing a list of hosts we've asked the user about when
 * SSL validation fails. If this function returns TRUE, that means we
 * have previously asked the user about this host. Ok_novalidate == 1 means
 * the user said yes, it was ok. Ok_novalidate == 0 means the user
 * said no. Warned means we warned them already.
 */
int
imap_get_ssl(m_list, hostlist, ok_novalidate, warned)
    MMLOGIN_S *m_list;
    STRLIST_S *hostlist;
    int       *ok_novalidate;
    int       *warned;
{
    MMLOGIN_S *l;
    
    for(l = m_list; l; l = l->next)
      if(imap_same_host(l->hosts, hostlist)){
	  if(ok_novalidate)
	    *ok_novalidate = l->ok_novalidate;

	  if(warned)
	    *warned = l->warned;

	  return(TRUE);
      }

    return(FALSE);
}


/*
 * Just trying to guess the username the user might want to use on this
 * host, the user will confirm.
 */
char *
imap_get_user(m_list, hostlist)
    MMLOGIN_S *m_list;
    STRLIST_S *hostlist;
{
    MMLOGIN_S *l;
    
    for(l = m_list; l; l = l->next)
      if(imap_same_host(l->hosts, hostlist))
	return(l->user);

    return(NULL);
}


/*
 * If we have a matching hostname, username, and altflag in our cache,
 * attempt to login with the password from the cache.
 */
int
imap_get_passwd(m_list, passwd, user, hostlist, altflag)
    MMLOGIN_S *m_list;
    char      *passwd, *user;
    STRLIST_S *hostlist;
    int	       altflag;
{
    MMLOGIN_S *l;
    
    dprint(9, (debugfile,
	       "imap_get_passwd: checking user=%s alt=%d host=%s%s%s\n",
	       user ? user : "(null)",
	       altflag,
	       hostlist->name ? hostlist->name : "",
	       (hostlist->next && hostlist->next->name) ? ", " : "",
	       (hostlist->next && hostlist->next->name) ? hostlist->next->name
						        : ""));
    for(l = m_list; l; l = l->next)
      if(imap_same_host(l->hosts, hostlist)
	 && *user
	 && !strcmp(user, l->user)
	 && l->altflag == altflag){
	  strncpy(passwd, l->passwd, NETMAXPASSWD);
	  passwd[NETMAXPASSWD-1] = '\0';
	  dprint(9, (debugfile, "imap_get_passwd: match\n"));
	  dprint(10, (debugfile, "imap_get_passwd: trying passwd=\"%s\"\n",
		      passwd ? passwd : "?"));
	  return(TRUE);
      }

    dprint(9, (debugfile, "imap_get_passwd: no match\n"));
    return(FALSE);
}



void
imap_set_passwd(l, passwd, user, hostlist, altflag, ok_novalidate, warned)
    MMLOGIN_S **l;
    char       *passwd, *user;
    STRLIST_S  *hostlist;
    int         altflag, ok_novalidate, warned;
{
    STRLIST_S **listp;
    size_t      len;

    dprint(9, (debugfile, "imap_set_passwd\n"));
    for(; *l; l = &(*l)->next)
      if(imap_same_host((*l)->hosts, hostlist)
	 && !strcmp(user, (*l)->user)
	 && altflag == (*l)->altflag)
	if(strcmp(passwd, (*l)->passwd) ||
	   (*l)->ok_novalidate != ok_novalidate ||
	   (*l)->warned != warned)
	  break;
	else
	  return;

    if(!*l){
	*l = (MMLOGIN_S *)fs_get(sizeof(MMLOGIN_S));
	memset(*l, 0, sizeof(MMLOGIN_S));
    }

    len = strlen(passwd);
    if(!(*l)->passwd || strlen((*l)->passwd) < len)
      (*l)->passwd = ps_get(len+1);

    strncpy((*l)->passwd, passwd, len+1);

    (*l)->altflag = altflag;
    (*l)->ok_novalidate = ok_novalidate;
    (*l)->warned = warned;

    if(!(*l)->user)
      (*l)->user = cpystr(user);

    dprint(9, (debugfile, "imap_set_passwd: user=%s altflag=%d\n",
	   (*l)->user ? (*l)->user : "?",
	   (*l)->altflag));

    for( ; hostlist; hostlist = hostlist->next){
	for(listp = &(*l)->hosts;
	    *listp && strucmp((*listp)->name, hostlist->name);
	    listp = &(*listp)->next)
	  ;

	if(!*listp){
	    *listp = (STRLIST_S *)fs_get(sizeof(STRLIST_S));
	    (*listp)->name = cpystr(hostlist->name);
	    dprint(9, (debugfile, "imap_set_passwd: host=%s\n",
		       (*listp)->name ? (*listp)->name : "?"));
	    (*listp)->next = NULL;
	}
    }

    dprint(10, (debugfile, "imap_set_passwd: passwd=\"%s\"\n",
	   passwd ? passwd : "?"));
}



void
imap_flush_passwd_cache()
{
    MMLOGIN_S *l;

    memset((void *)private_store, 0, sizeof(private_store));
    if(panicking)
      return;

    while(l = mm_login_list){
	mm_login_list = mm_login_list->next;
	if(l->user)
	  fs_give((void **) &l->user);

	free_strlist(&l->hosts);

	fs_give((void **) &l);
    }

    while(l = cert_failure_list){
	cert_failure_list = cert_failure_list->next;
	if(l->user)
	  fs_give((void **) &l->user);

	free_strlist(&l->hosts);

	fs_give((void **) &l);
    }
}


/*
 * Mimics fs_get except it only works for char * (no alignment hacks), it
 * stores in a static array so it is easy to zero it out (that's the whole
 * purpose), allocations always happen at the end (no free).
 * If we go past array limit, we don't break, we just use free storage.
 * Should be awfully rare, though.
 */
char *
ps_get(size)
    size_t size;
{
    static char *last  = private_store;
    char        *block = NULL;

    /* there is enough space */
    if(size <= sizeof(private_store) - (last - private_store)){
	block = last;
	last += size;
    }
    else{
	dprint(2, (debugfile,
		   "Out of password caching space in private_store\n"));
	dprint(2, (debugfile,
		   "Using free storage instead\n"));
	block = fs_get(size);
    }

    return(block);
}


#ifdef	PASSFILE
/* 
 * Specific functions to support caching username/passwd/host
 * triples on disk for use from one session to the next...
 */

#define	FIRSTCH		0x20
#define	LASTCH		0x7e
#define	TABSZ		(LASTCH - FIRSTCH + 1)

static	int		xlate_key;
static	MMLOGIN_S	*passfile_cache = NULL;
static  int             using_passfile;



/*
 * xlate_in() - xlate_in the given character
 */
char
xlate_in(c)
    int	c;
{
    register int  eti;

    eti = xlate_key;
    if((c >= FIRSTCH) && (c <= LASTCH)){
        eti += (c - FIRSTCH);
	eti -= (eti >= 2*TABSZ) ? 2*TABSZ : (eti >= TABSZ) ? TABSZ : 0;
        return((xlate_key = eti) + FIRSTCH);
    }
    else
      return(c);
}



/*
 * xlate_out() - xlate_out the given character
 */
char
xlate_out(c)
    char c;
{
    register int  dti;
    register int  xch;

    if((c >= FIRSTCH) && (c <= LASTCH)){
        xch  = c - (dti = xlate_key);
	xch += (xch < FIRSTCH-TABSZ) ? 2*TABSZ : (xch < FIRSTCH) ? TABSZ : 0;
        dti  = (xch - FIRSTCH) + dti;
	dti -= (dti >= 2*TABSZ) ? 2*TABSZ : (dti >= TABSZ) ? TABSZ : 0;
        xlate_key = dti;
        return(xch);
    }
    else
      return(c);
}



char *
passfile_name(pinerc, path, len)
    char *pinerc, *path;
    size_t len;
{
    struct stat  sbuf;
    char	*p = NULL;
    int		 i, j;

    if(!path || !((pinerc && pinerc[0]) || ps_global->passfile))
      return(NULL);

    if(ps_global->passfile)
      strncpy(path, ps_global->passfile, len-1);
    else{
	if((p = last_cmpnt(pinerc)) && *(p-1) && *(p-1) != PASSFILE[0])
	  for(i = 0; pinerc < p && i < len; pinerc++, i++)
	    path[i] = *pinerc;
	else
	  i = 0;

	for(j = 0; (i < len) && (path[i] = PASSFILE[j]); i++, j++)
	  ;
	
    }

    path[len-1] = '\0';

    dprint(9, (debugfile, "Looking for passfile \"%s\"\n",
	   path ? path : "?"));

#if	defined(DOS) || defined(OS2)
    return((stat(path, &sbuf) == 0
	    && ((sbuf.st_mode & S_IFMT) == S_IFREG))
	     ? path : NULL);
#else
    /* First, make sure it's ours and not sym-linked */
    if(lstat(path, &sbuf) == 0
       && ((sbuf.st_mode & S_IFMT) == S_IFREG)
       && sbuf.st_uid == getuid()){
	/* if too liberal permissions, fix them */
	if((sbuf.st_mode & 077) != 0)
	  if(chmod(path, sbuf.st_mode & ~077) != 0)
	    return(NULL);
	
	return(path);
    }
    else
	return(NULL);
#endif
}



/*
 * Passfile lines are
 *
 *   passwd TAB user TAB hostname TAB flags [ TAB orig_hostname ] \n
 *
 * In pine4.40 and before there was no orig_hostname, and there still isn't
 * if it is the same as hostname.
 */
int
read_passfile(pinerc, l)
    char       *pinerc;
    MMLOGIN_S **l;
{
    char  tmp[MAILTMPLEN], *ui[5];
    int   i, j, n;
    FILE *fp;

    dprint(9, (debugfile, "read_passfile\n"));
    /* if there's no password to read, bag it!! */
    if(!passfile_name(pinerc, tmp, sizeof(tmp)) || !(fp = fopen(tmp, "r")))
      return(0);

    using_passfile = 1;
    for(n = 0; fgets(tmp, sizeof(tmp), fp); n++){
	/*** do any necessary DEcryption here ***/
	xlate_key = n;
	for(i = 0; tmp[i]; i++)
	  tmp[i] = xlate_out(tmp[i]);

	if(i && tmp[i-1] == '\n')
	  tmp[i-1] = '\0';			/* blast '\n' */

	dprint(10, (debugfile, "read_passfile: %s\n", tmp ? tmp : "?"));
	ui[0] = ui[1] = ui[2] = ui[3] = ui[4] = NULL;
	for(i = 0, j = 0; tmp[i] && j < 5; j++){
	    for(ui[j] = &tmp[i]; tmp[i] && tmp[i] != '\t'; i++)
	      ;					/* find end of data */

	    if(tmp[i])
	      tmp[i++] = '\0';			/* tie off data */
	}

	dprint(9, (debugfile, "read_passfile: calling imap_set_passwd\n"));
	if(ui[0] && ui[1] && ui[2]){		/* valid field? */
	    STRLIST_S hostlist[2];
	    int	      flags = ui[3] ? atoi(ui[3]) : 0;

	    hostlist[0].name = ui[2];
	    if(ui[4]){
		hostlist[0].next = &hostlist[1];
		hostlist[1].name = ui[4];
		hostlist[1].next = NULL;
	    }
	    else{
		hostlist[0].next = NULL;
	    }

	    imap_set_passwd(l, ui[0], ui[1], hostlist, flags & 0x01, 0, 0);
	}
    }

    fclose(fp);
    return(1);
}



void
write_passfile(pinerc, l)
    char      *pinerc;
    MMLOGIN_S *l;
{
    char  tmp[MAILTMPLEN];
    int   i, n;
    FILE *fp;

    dprint(9, (debugfile, "write_passfile\n"));
    /* if there's no password to read, bag it!! */
    if(!passfile_name(pinerc, tmp, sizeof(tmp)) || !(fp = fopen(tmp, "w")))
      return;

    for(n = 0; l; l = l->next, n++){
	/*** do any necessary ENcryption here ***/
	sprintf(tmp, "%.100s\t%.100s\t%.100s\t%d%s%s\n", l->passwd, l->user,
		l->hosts->name, l->altflag,
		(l->hosts->next && l->hosts->next->name) ? "\t" : "",
		(l->hosts->next && l->hosts->next->name) ? l->hosts->next->name
							 : "");
	dprint(10, (debugfile, "write_passfile: %s", tmp ? tmp : "?"));
	xlate_key = n;
	for(i = 0; tmp[i]; i++)
	  tmp[i] = xlate_in(tmp[i]);

	fputs(tmp, fp);
    }

    fclose(fp);
}



/*
 * get_passfile_passwd - return the password contained in the special passord
 *            cache.  The file is assumed to be in the same directory
 *            as the pinerc with the name defined above.
 */
int
get_passfile_passwd(pinerc, passwd, user, hostlist, altflag)
    char      *pinerc, *passwd, *user;
    STRLIST_S *hostlist;
    int	       altflag;
{
    dprint(9, (debugfile, "get_passfile_passwd\n"));
    return((passfile_cache || read_passfile(pinerc, &passfile_cache))
	     ? imap_get_passwd(passfile_cache, passwd,
			       user, hostlist, altflag)
	     : 0);
}

int
is_using_passfile()
{
    return(using_passfile ? 1 : 0);
}

/*
 * Just trying to guess the username the user might want to use on this
 * host, the user will confirm.
 */
char *
get_passfile_user(pinerc, hostlist)
    char      *pinerc;
    STRLIST_S *hostlist;
{
    return((passfile_cache || read_passfile(pinerc, &passfile_cache))
	     ? imap_get_user(passfile_cache, hostlist)
	     : NULL);
}



/*
 * set_passfile_passwd - set the password file entry associated with
 *            cache.  The file is assumed to be in the same directory
 *            as the pinerc with the name defined above.
 */
void
set_passfile_passwd(pinerc, passwd, user, hostlist, altflag)
    char      *pinerc, *passwd, *user;
    STRLIST_S *hostlist;
    int	       altflag;
{
    dprint(9, (debugfile, "set_passfile_passwd\n"));
    if((passfile_cache || read_passfile(pinerc, &passfile_cache))
       && !ps_global->nowrite_passfile
       && want_to("Preserve password on DISK for next login", 'y', 'x',
		  NO_HELP, WT_NORM) == 'y'){
	imap_set_passwd(&passfile_cache, passwd, user, hostlist, altflag, 0, 0);
	write_passfile(pinerc, passfile_cache);
    }
}


/*
 * Passfile lines are
 *
 *   passwd TAB user TAB hostname TAB flags [ TAB orig_hostname ] \n
 *
 * In pine4.40 and before there was no orig_hostname.
 * This routine attempts to repair that.
 */
void
update_passfile_hostlist(pinerc, user, hostlist, altflag)
    char      *pinerc;
    char      *user;
    STRLIST_S *hostlist;
    int        altflag;
{
    MMLOGIN_S *l;
    STRLIST_S *h1, *h2;
    
    for(l = passfile_cache; l; l = l->next)
      if(imap_same_host(l->hosts, hostlist)
	 && *user
	 && !strcmp(user, l->user)
	 && l->altflag == altflag){
	  break;
      }
    
    if(l && l->hosts && hostlist && !l->hosts->next && hostlist->next
       && hostlist->next->name
       && !ps_global->nowrite_passfile){
	l->hosts->next = (STRLIST_S *)fs_get(sizeof(STRLIST_S));
	l->hosts->next->name = cpystr(hostlist->next->name);
	l->hosts->next->next = NULL;
	write_passfile(pinerc, passfile_cache);
    }
}
#endif	/* PASSFILE */
