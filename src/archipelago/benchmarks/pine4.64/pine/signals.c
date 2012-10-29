#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: signals.c 13860 2004-11-04 22:31:46Z hubert $";
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


   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    * 			Copyright (c) 1986, 1987 Dave Taylor              *
    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************
 

  ----------------------------------------------------------------------*/

/*======================================================================
     signals.c
     Different signal handlers for different signals
       - Catches all the abort signals, cleans up tty modes and then coredumps
       - Not much to do for SIGHUP
       - Not much to do for SIGTERM
       - turn SIGWINCH into a KEY_RESIZE command
       - No signals for ^Z/suspend, but do it here anyway
       - Also set up the signal handlers, and hold signals for
         critical imap sections of code.

 ====*/

#include "headers.h"

/* nto defines SA_RESTART but doesn't support it yet (00/12/29 - hubert) */
#if defined(NTO)
#undef SA_RESTART
#endif

/*
 * call used by TERM and HUP handlers to quickly close streams
 */
void  fast_clean_up PROTO(());
void  suspend_notice PROTO((char *));
void  suspend_warning PROTO(());
void  alarm_signal_reset PROTO(());


              /* SigType is defined in os.h and is either int or void */
static	SigType	auger_in_signal SIG_PROTO((int));
static	SigType	winch_signal SIG_PROTO((int));
static	SigType	usr2_signal SIG_PROTO((int));
static	SigType	alarm_signal SIG_PROTO((int));
static	SigType	intr_signal SIG_PROTO((int));

static  int now_handling_alarms;
static  int cleanup_called_from_sig_handler;



/*----------------------------------------------------------------------
    Install handlers for all the signals we care to catch
  ----------------------------------------------------------------------*/
void
init_signals()
{
    dprint(9, (debugfile, "init_signals()\n"));
    init_sighup();
    now_handling_alarms++;
#ifdef	_WINDOWS
    /* Only one signal works. */
    signal(SIGALRM, (void *)alarm_signal);
#else
#ifdef	OS2
    dont_interrupt();
    signal(SIGALRM, (void *)alarm_signal);
#else
#if	defined(DOS)
    dont_interrupt();
#else
#ifdef DEBUG
#define	CUSHION_SIG	(debug < 7)
#else
#define	CUSHION_SIG	(1)
#endif

    if(CUSHION_SIG){
	signal(SIGILL,  auger_in_signal); 
	signal(SIGTRAP, auger_in_signal);
	signal(SIGEMT,  auger_in_signal);
	signal(SIGBUS,  auger_in_signal);
	signal(SIGSEGV, auger_in_signal);
	signal(SIGSYS,  auger_in_signal);
	signal(SIGQUIT, auger_in_signal);
	/* Don't catch SIGFPE cause it's rare and we use it in a hack below*/
    }
    
    init_sigwinch();

    /*
     * Set up SIGUSR2 to catch signal from other software using the 
     * c-client to tell us that other access to the folder is being 
     * attempted.  THIS IS A TEST: if it turns out that simply
     * going R/O when another pine is started or the same folder is opened,
     * then we may want to install a smarter handler that uses idle time
     * or even prompts the user to see if it's ok to give up R/O access...
     */
    signal(SIGUSR2, usr2_signal);
    
#ifdef SA_RESTART
    {
	struct sigaction sa;

    sa.sa_handler = alarm_signal;
    memset(&sa.sa_mask, 0, sizeof(sa.sa_mask));
    sa.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &sa, NULL);
    }
#else
    signal(SIGALRM, alarm_signal);
#endif


    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, SIG_IGN);

#ifdef SIGTSTP
    /* Some unexplained behaviour on Ultrix 4.2 (Hardy) seems to be
       resulting in Pine getting sent a SIGTSTP. Ignore it here.
       probably better to ignore it than let it happen in any case
     */
    signal(SIGTSTP, SIG_IGN); 
#endif /* SIGTSTP */

#ifdef	SIGCHLD
    signal(SIGCHLD,  child_signal);
#endif
#endif	/* !DOS */
#endif	/* !OS2 */
#endif	/* !_WINDOWS */
}



/*----------------------------------------------------------------------
    Return all signal handling back to normal
  ----------------------------------------------------------------------*/
void
end_signals(blockem)
    int blockem;
{
    now_handling_alarms = 0;

#ifdef	_WINDOWS
    signal(SIGALRM, blockem ? SIG_IGN : SIG_DFL);
    signal(SIGHUP,  blockem ? SIG_IGN : SIG_DFL);
#else
#ifdef	OS2
    interrupt_ok();
    signal(SIGALRM, blockem ? SIG_IGN : SIG_DFL);
    signal(SIGHUP,  blockem ? SIG_IGN : SIG_DFL);
#else
#ifdef	DOS
    interrupt_ok();
#else
#ifndef	SIG_ERR
#define	SIG_ERR	(SigType (*)())-1
#endif

    dprint(5, (debugfile, "end_signals(%d)\n", blockem));
    if(signal(SIGILL,  blockem ? SIG_IGN : SIG_DFL) == SIG_ERR){
        fprintf(stderr, "Error resetting signals: %s\n",
                error_description(errno));
        exit(-1);
    }

    signal(SIGTRAP, blockem ? SIG_IGN : SIG_DFL);
    signal(SIGEMT,  blockem ? SIG_IGN : SIG_DFL);
    signal(SIGBUS,  blockem ? SIG_IGN : SIG_DFL);
    signal(SIGSEGV, blockem ? SIG_IGN : SIG_DFL);
    signal(SIGSYS,  blockem ? SIG_IGN : SIG_DFL);
#ifdef RESIZING
    signal(SIGWINCH, blockem ? SIG_IGN : SIG_DFL);
#endif
    signal(SIGQUIT, blockem ? SIG_IGN : SIG_DFL);
#ifdef SIGTSTP
    signal(SIGTSTP, blockem ? SIG_IGN : SIG_DFL);
#endif /* SIGTSTP */
    signal(SIGHUP,  blockem ? SIG_IGN : SIG_DFL);
    signal(SIGALRM, blockem ? SIG_IGN : SIG_DFL);
    signal(SIGTERM, blockem ? SIG_IGN : SIG_DFL);
    signal(SIGINT,  blockem ? SIG_IGN : SIG_DFL);
#endif	/* !DOS */
#endif	/* !OS2 */
#endif	/* !_WINDOWS */
}


/*----------------------------------------------------------------------
     Handle signals caused by aborts -- SIGSEGV, SIGILL, etc

Call panic which cleans up tty modes and then core dumps
  ----------------------------------------------------------------------*/
static SigType
auger_in_signal SIG_PROTO ((int sig))
{
    char buf[100], *s;

    panicking = 1;
    end_signals(1);			/* don't catch any more signals */
    imap_flush_passwd_cache();

#if defined(SIGNALHASARG)
    s = comatose(sig);
#else
    s = "?";
#endif

    sprintf(buf, "Received abort signal(sig=%.9s)", s);
    panic(buf);				/* clean up and get out */

    exit(-1);				/* in case panic doesn't kill us */
}


/*----------------------------------------------------------------------
   Install signal handler to deal with hang up signals -- SIGHUP, SIGTERM
  
  ----------------------------------------------------------------------*/
void
init_sighup()
{
#if	!(defined(DOS) && !defined(_WINDOWS))
#if	defined(_WINDOWS) || defined(OS2)
    signal(SIGHUP, (void *) hup_signal);
#else
    signal(SIGHUP, hup_signal);
#endif
#endif
#if	!(defined(DOS) || defined(OS2))
    signal(SIGTERM, term_signal);
#endif
}


/*----------------------------------------------------------------------
   De-Install signal handler to deal with hang up signals -- SIGHUP, SIGTERM
  
  ----------------------------------------------------------------------*/
void
end_sighup()
{
#if	!(defined(DOS) && !defined(_WINDOWS))
    signal(SIGHUP, SIG_IGN);
#endif
#if	!(defined(DOS) || defined(OS2))
    signal(SIGTERM, SIG_IGN);
#endif
}


/*----------------------------------------------------------------------
      handle hang up signal -- SIGHUP

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
SigType
hup_signal()
{
    end_signals(1);			/* don't catch any more signals */
    dprint(1, (debugfile, "\n\n** Received SIGHUP **\n\n\n\n"));
    cleanup_called_from_sig_handler = 1;
    fast_clean_up();
#if	defined(DEBUG)
    if(debugfile)
      fclose(debugfile);
#endif 

    _exit(0);				/* cleaning up can crash */
}


/*----------------------------------------------------------------------
      Timeout when no user input for a long, long time.
      Treat it pretty much the same as if we got a HUP.
      Only difference is we sometimes turns the timeout off (when composing).
  ----------------------------------------------------------------------*/
void
user_input_timeout_exit(to_hours)
    int to_hours;
{
    char msg[40];

    dprint(1, (debugfile,
	   "\n\n** Exiting: user input timeout (%d hours) **\n\n\n\n",
	   to_hours));
    sprintf(msg, "\n\nPine timed out (No user input for %d %s)\n", to_hours,
	    to_hours > 1 ? "hours" : "hour");
    fast_clean_up();
    end_screen(msg, 0);
    end_titlebar();
    end_keymenu();
    end_keyboard(F_ON(F_USE_FK,ps_global));
    end_tty_driver(ps_global);
    end_signals(0);
#if	defined(DEBUG) && (!defined(DOS) || defined(_WINDOWS))
    if(debugfile)
      fclose(debugfile);
#endif 
    exit(0);
}


/*----------------------------------------------------------------------
      handle terminate signal -- SIGTERM

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
SigType
term_signal()
{
#if !defined(DOS) && !defined(OS2)
    end_signals(1);			/* don't catch any more signals */
    dprint(1, (debugfile, "\n\n** Received SIGTERM **\n\n\n\n"));
    cleanup_called_from_sig_handler = 1;
    fast_clean_up();
#if	defined(DEBUG) && (!defined(DOS) || defined(_WINDOWS))
    if(debugfile)
      fclose(debugfile);
#endif 
    printf("\n\nPine finished. Received terminate signal\n\n");
#endif	/* !DOS */
    exit(0);
}


/*----------------------------------------------------------------------
     Handle cleaning up mail streams and tty modes...
Not much to do. Rely on periodic mail file check pointing.  Don't try
cleaning up screen or flushing output since stdout is likely already
gone.  To be safe, though, we'll at least restore the original tty mode.
Also delete any remnant _DATAFILE_ from sending-filters.
  ----------------------------------------------------------------------*/
void
fast_clean_up()
{
    int         i;
    MAILSTREAM *m;

    dprint(1, (debugfile, "fast_clean_up()\n"));

    if(filter_data_file(0))
        unlink(filter_data_file(0));
    if(ps_global->expunge_in_progress){
	PineRaw(0);
	return;
    }

    /*
     * This gets rid of temporary cache files for remote addrbooks.
     */
    completely_done_with_adrbks();

    /*
     * This flushes out deferred changes and gets rid of temporary cache
     * files for remote config files.
     */
    if(ps_global->prc){
	if(ps_global->prc->outstanding_pinerc_changes)
	  write_pinerc(ps_global, Main,
		       cleanup_called_from_sig_handler ? WRP_NOUSER : WRP_NONE);

	if(ps_global->prc->rd)
	  rd_close_remdata(&ps_global->prc->rd);
	
	free_pinerc_s(&ps_global->prc);
    }

    /* as does this */
    if(ps_global->post_prc){
	if(ps_global->post_prc->outstanding_pinerc_changes)
	  write_pinerc(ps_global, Post,
		       cleanup_called_from_sig_handler ? WRP_NOUSER : WRP_NONE);

	if(ps_global->post_prc->rd)
	  rd_close_remdata(&ps_global->post_prc->rd);
	
	free_pinerc_s(&ps_global->post_prc);
    }

    /*
     * Can't figure out why this section is inside the ifdef, but no
     * harm leaving it that way, I guess.
     */
#if !defined(DOS) && !defined(OS2)
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && !m->lock)
	  pine_mail_actually_close(m);
    }

    PineRaw(0);

#endif	/* !DOS */

    imap_flush_passwd_cache();

    dprint(1, (debugfile, "done with fast_clean_up\n"));
}


#if !defined(DOS) && !defined(OS2)
/*----------------------------------------------------------------------
      handle hang up signal -- SIGUSR2

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
static SigType
usr2_signal SIG_PROTO((int sig))
{
    char        c, *mbox, mboxbuf[20];
    int         i;
    MAILSTREAM *stream;
    NETMBX      mb;

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	stream = ps_global->s_pool.streams[i];
	if(stream
	   && sp_flagged(stream, SP_LOCKED)
	   && !sp_dead_stream(stream)
	   && !stream->lock
	   && !stream->rdonly
	   && stream->mailbox
	   && (c = *stream->mailbox) != '{' && c != '*'){
	    pine_mail_check(stream);		/* write latest state   */
	    stream->rdonly = 1;			/* and become read-only */
	    (void) pine_mail_ping(stream);
	    mbox = stream->mailbox;
	    if(!strucmp(stream->mailbox, ps_global->inbox_name)
	       || !strcmp(stream->mailbox, ps_global->VAR_INBOX_PATH)
	       || !strucmp(stream->original_mailbox, ps_global->inbox_name)
	       || !strcmp(stream->original_mailbox, ps_global->VAR_INBOX_PATH))
	      mbox = "INBOX";
	    else if(mail_valid_net_parse(stream->mailbox, &mb) && mb.mailbox)
	      mbox = mb.mailbox;
	    
	    q_status_message1(SM_ASYNC, 3, 7,
	     "Another email program is accessing %.20s. Session now Read-Only.",
	     short_str((mbox && *mbox) ? mbox : "folder",
		       mboxbuf, 19, FrontDots));
	    dprint(1, (debugfile, "** folder %s went read-only **\n\n",
			stream->mailbox));
	}
    }
}
#endif



/*----------------------------------------------------------------------
   Install signal handler to deal with window resize signal -- SIGWINCH
  
  ----------------------------------------------------------------------*/
void
init_sigwinch ()
{
#ifdef RESIZING
    signal(SIGWINCH, winch_signal);
#endif
}


#ifdef RESIZING
/*----------------------------------------------------------------------
   Handle window resize signal -- SIGWINCH
  
   The planned strategy is just force a redraw command. This is similar
  to new mail handling which forces a noop command. The signals are
  help until pine reads input. Then a KEY_RESIZE is forced into the command
  stream .
   Note that ready_for_winch is only non-zero inside the read_char function,
  so the longjmp only ever happens there, and it is really just a jump
  from lower down in the function up to the top of that function.  Its
  purpose is to return a KEY_RESIZE from read_char when interrupted
  out of the select lower down in read_char.
  ----------------------------------------------------------------------*/
extern jmp_buf  winch_state;
extern int      ready_for_winch, winch_occured;

SigType
static winch_signal SIG_PROTO((int sig))
{
    clear_cursor_pos();
    init_sigwinch();
    winch_cleanup();
}
#endif

/*
 * winch_cleanup - 
 */
void
winch_cleanup()
{
#ifdef	RESIZING
    if(ready_for_winch)
      longjmp(winch_state, 1);
    else
      winch_occured = 1;
#endif
}


#ifdef	SIGCHLD
/*----------------------------------------------------------------------
   Handle child status change -- SIGCHLD
  
   The strategy here is to install the handler when we spawn a child, and
   to let the system tell us when the child's state has changed.  In the
   mean time, we can do whatever.  Typically, "whatever" is spinning in a
   loop alternating between sleep and new_mail calls intended to keep the
   IMAP stream alive.

  ----------------------------------------------------------------------*/
extern short    child_signalled, child_jump;
extern jmp_buf  child_state;

SigType
child_signal()
{
#ifdef	BACKGROUND_POST
    if(post_reap())
      return;
#endif

    child_signalled = 1;
    if(child_jump)
      longjmp(child_state, 1);
}
#endif


static unsigned       alarm_increment;
static int            dotcount;
static char           busy_message[MAX_BM + 1];
static int            busy_alarm_outstanding;
static int            busy_len;
static int            final_message;
#ifdef	DOS
static int            callcount;
#endif
static percent_done_t percent_done_ptr;
static char *display_chars[] = {
	"<\\> ",
	"<|> ",
	"</> ",
	"<-> ",
	"    ",
	"<-> "
};
#define DISPLAY_CHARS_ROWS 6
#define DISPLAY_CHARS_COLS 4

/*
 * Turn on a busy alarm.
 *
 *    seconds -- alarm fires every seconds seconds
 *        msg -- the busy message to print in status line
 *    pc_func -- if non-null, call this function to get the percent done,
 *		   (an integer between 0 and 100).  If null, append dots.
 *   init_msg -- if non-zero, force out an immediate status message instead
 *                 of waiting for first alarm (except see comments in code)
 *
 *   Returns:  0 If busy alarm was already set up before we got here
 *             1 If busy alarm was not already set up.
 */
int
busy_alarm(seconds, msg, pc_func, init_msg)
    unsigned       seconds;
    char          *msg;
    percent_done_t pc_func;
    int            init_msg;
{
    int retval = 1;

    if(!now_handling_alarms)
      return 0;

    dprint(9,(debugfile, "busy_alarm(%d, %s, %p, %d)\n",
	seconds, msg ? msg : "Busy", pc_func, init_msg));

    /*
     * If we're already busy'ing, and we don't have something special,
     * just leave it alone.
     */
    if(busy_alarm_outstanding){
	retval = 0;
	if(!msg && !pc_func)
	  return(retval);
    }

    if(seconds){
	busy_alarm_outstanding = 1;
	alarm_increment = seconds;
	dotcount = 0;
	percent_done_ptr = pc_func;
#ifdef	DOS
	callcount = 0;
#endif

	if(msg){
	    strncpy(busy_message, msg, MAX_BM);
	    final_message = 1;
	}
	else{
	    strcpy(busy_message, "Busy");
	    final_message = 0;
	}

	busy_message[MAX_BM] = '\0';
	busy_len = strlen(busy_message);

	if(init_msg){
	    char progress[MAX_SCREEN_COLS+1];
	    int space_left, slots_used;

	    final_message = 1;
	    space_left = (ps_global->ttyo ? ps_global->ttyo->screen_cols
					  : 80) -
					  busy_len - 2;  /* 2 is for [] */
	    slots_used = max(0, min(space_left-3, 10));

	    if(percent_done_ptr && slots_used >= 4)
	      sprintf(progress, "%s |%*s|", busy_message, slots_used, "");
	    else{
		dotcount++;
		sprintf(progress, "%s%*s", busy_message,
		    DISPLAY_CHARS_COLS + 1, "");
	    }


	    if(status_message_remaining()){
		char buf[sizeof(progress) + 30];
		char  *append = " [not actually shown]";

		strncpy(buf, progress, sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';

		strncat(buf, append, sizeof(buf) - strlen(buf) - 1);
		buf[sizeof(buf)-1] = '\0';

		add_review_message(buf, -1);
	    }
	    else{
		q_status_message(SM_ORDER, 0, 1, progress);

		/*
		 * We use display_message so that the initial message will
		 * be forced out only if there is not a previous message
		 * currently being displayed that hasn't been displayed for
		 * its min display time yet.  In that case, we don't want
		 * to force out the initial message.
		 */
		display_message('x');
	    }
	}
	
#ifdef _WINDOWS
	mswin_setcursor (MSWIN_CURSOR_BUSY);
#endif
	fflush(stdout);
    }

    /* set alarm */
    if(F_OFF(F_DISABLE_ALARM, ps_global))
      alarm(seconds);

    return(retval);
}


/*
 * If final_message was set when busy_alarm was called:
 *   and message_pri = -1 -- no final message queued
 *                 else final message queued with min equal to message_pri
 */
void
cancel_busy_alarm(message_pri)
    int message_pri;
{
    dprint(9,(debugfile, "cancel_busy_alarm(%d)\n", message_pri));

    alarm(0);

    if(busy_alarm_outstanding){
	int space_left, slots_used;

	busy_alarm_outstanding = 0;

	if(final_message && message_pri >= 0){
	    char progress[MAX_SCREEN_COLS+1];

	    space_left = (ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) -
		busy_len - 2;  /* 2 is for [] */
	    slots_used = max(0, min(space_left-3, 10));

	    if(percent_done_ptr && slots_used >= 4){
		int left, right;

		right = (slots_used - 4)/2;
		left  = slots_used - 4 - right;
		sprintf(progress, "%s |%*s100%%%*s|",
		    busy_message, left, "", right, "");
		q_status_message(SM_ORDER,
		    message_pri>=2 ? max(message_pri,3) : 0,
		    message_pri+2, progress);
	    }
	    else{
		sprintf(progress, "%s%*sDONE", busy_message,
		    DISPLAY_CHARS_COLS - 4 + 1, "");
		q_status_message(SM_ORDER,
		    message_pri>=2 ? max(message_pri,3) : 0,
		    message_pri+2, progress);
	    }
	}
	else
	  mark_status_dirty();
    }
}


/*
 * suspend_busy_alarm - continue previously installed busy_alarm.
 */
void
suspend_busy_alarm()
{
    dprint(9,(debugfile, "suspend_busy_alarm\n"));

    if(busy_alarm_outstanding)
      alarm(0);
}


/*
 * resume_busy_alarm - continue previously installed busy_alarm.
 */
void
resume_busy_alarm(pause)
    unsigned pause;
{
    dprint(9,(debugfile, "resume_busy_alarm\n"));

    if(busy_alarm_outstanding && F_OFF(F_DISABLE_ALARM, ps_global))
      alarm(pause ? pause : alarm_increment);
}


/*
 * Block our alarm signals when we enter sensitive code, and unblock when
 * we leave. These are called from a c-client callback.
 *
 * We use sigmask being defined to tell us if sigblock will work. This isn't
 * perfect but we think that if it is defined, then it will work. We know
 * of cases where it would still work even if it isn't defined (Solaris).
 * Since we can't think of an easy way to detect that, we'll play it safe and
 * use regular alarm(0) to turn off alarms in those cases.
 */
#ifdef	sigmask
static int alrm_depth = 0;
static int alrm_mask = 0;
#endif

void *
alrm_signal_block()
{
#ifdef	sigmask
    if(!alrm_depth++)
      alrm_mask = sigblock(sigmask(SIGALRM));

    return(NULL);
#else
    return((void *) alarm(0));
#endif
}


void
alrm_signal_unblock(data)
    void *data;
{
#ifdef	sigmask
    switch(alrm_depth){
      case 1:			/* pop */
	sigsetmask(alrm_mask);
	alrm_depth = 0;

      case 0:			/* unbalanced unblocks */
	break;

      default:			/* nested unblock */
	alrm_depth--;
	break;
    }
#else
    if((unsigned int) data)
      alarm((unsigned int) data);
#endif
}


static SigType
alarm_signal SIG_PROTO((int sig))
{
    int space_left, slots_used;
    char dbuf[MAX_SCREEN_COLS+1];

    /*
     * In case something was queue'd before this alarm's busy_handler
     * was installed.  If so, just act like nothing happened...
     */
    if(status_message_remaining()){
	alarm_signal_reset();
	return;
    }

    /*
     * Why do we need this? We can get alarm signals delivered to us
     * even after we've turned off alarms with cancel_busy_alarm, because
     * in pine_block_notify we block and unblock alarms, which can then
     * be delivered after the unblock. If !busy_alarm_outstanding that means
     * this alarm has already been canceled. So we don't want to print
     * anything and we don't want to reset the alarm (alarm_signal_reset()
     * checks and doesn't reset).
     */
    if(!busy_alarm_outstanding)
      return;

    space_left = (ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) -
	busy_len - 2;  /* 2 is for [] */
    slots_used = max(0, min(space_left-3, 10));

    if(percent_done_ptr && slots_used >= 4){
	int completed, pd;
	char *s;

	pd = (*percent_done_ptr)();
	pd = min(max(0, pd), 100);

	completed = (pd * slots_used) / 100;
	sprintf(dbuf, "%s |%s%s%*s|", busy_message,
	    completed > 1 ? repeat_char(completed-1, pd==100 ? ' ' : '-') : "",
	    (completed > 0 && pd != 100) ? ">" : "",
	    slots_used - completed, "");

	if(slots_used == 10){
	    s = dbuf + strlen(dbuf) - 8;
	    if(pd < 10){
		s++; s++;
		*s++ = '0' + pd;
	    }
	    else if(pd < 100){
		s++;
		*s++ = '0' + pd / 10;
		*s++ = '0' + pd % 10;
	    }
	    else{
		*s++ = '1';
		*s++ = '0';
		*s++ = '0';
	    }

	    *s   = '%';
	}
    }
    else{
	char b[DISPLAY_CHARS_COLS + 2];
	int md = DISPLAY_CHARS_ROWS - 2;
	int ind;

	ind = (dotcount == 0) ? md :
	       (dotcount == 1) ? md + 1 : ((dotcount-2) % md);

	if(space_left >= DISPLAY_CHARS_COLS + 1){
	    b[0] = SPACE;
	    strcpy(b+1, display_chars[ind]);
	}
	else if(space_left >= 2){
	    b[0] = '.';
	    b[1] = '.';
	    b[2] = '.';
	    b[space_left] = '\0';
	}
	else
	  b[0] = '\0';

	sprintf(dbuf, "%s%s", busy_message, b);
    }

    status_message_write(dbuf, 1);
    dotcount++;
    fflush(stdout);

    alarm_signal_reset();
}


/*
 * alarm_signal_reset - prepare for another alarm
 */
void
alarm_signal_reset()
{
    if(F_ON(F_DISABLE_ALARM, ps_global))
      return;

#if	!defined(DOS) || defined(_WINDOWS)
    signal(SIGALRM, alarm_signal);
#endif
    if(busy_alarm_outstanding)
      alarm(alarm_increment);
}


#ifdef	DOS
/*
 * For systems that don't have an alarm() call.
 */
void
fake_alarm_blip()
{
    static time_t   last_alarm = -1;
    time_t	    now;

    if(busy_alarm_outstanding &&
       ++callcount % 5 == 0 &&
       (long)((now=time((time_t *)0)) - last_alarm) >= (long)alarm_increment){
	last_alarm = now;
	/* manually execute code that an alarm would cause to execute */
	alarm_signal SIG_PROTO((1));
    }
}
#endif


/*
 * Command interrupt support.
 */

static SigType
intr_signal SIG_PROTO((int sig))
{
    ps_global->intr_pending = 1;
}


void
intr_allow()
{
    if(signal(SIGINT, intr_signal) == intr_signal)
      return;				/* already installed */

    intr_proc(1);			/* turn on interrupt char */
}


void
intr_disallow()
{
    if(signal(SIGINT, SIG_IGN) == SIG_IGN)	/* already off! */
      return;

    our_sigunblock(SIGINT);		/* unblock signal after longjmp */
    ps_global->intr_pending = 0;
    intr_proc(0);			/* turn off interrupt char */
}


void
intr_handling_on()
{
    if(signal(SIGINT, intr_signal) == intr_signal)
      return;				/* already installed */

    intr_proc(1);
    if(ps_global && ps_global->ttyo)
      draw_cancel_keymenu();
}


void
intr_handling_off()
{
    if(signal(SIGINT, SIG_IGN) == SIG_IGN)	/* already off! */
      return;

    ps_global->intr_pending = 0;
    intr_proc(0);
    if(ps_global && ps_global->ttyo)
      blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);

    ps_global->mangled_footer = 1;
}


/*----------------------------------------------------------------------
     Set or reset what needs to be set when coming out of pico to run
     an alternate editor.

   Args:   come_back -- If come_back is 0 we're going out of our environment
		          to set up for an external editor.
		        If come_back is 1 we're coming back into pine.
  ----------------------------------------------------------------------*/
int
ttyfix(come_back) 
    int come_back;
{
#if	defined(DEBUG) && (!defined(DOS) || defined(_WINDOWS))
    if(debugfile)
      fflush(debugfile);
#endif

    if(come_back){
#ifdef OS2
	enter_text_mode(NULL);
#endif
	init_screen();
	init_tty_driver(ps_global);
	init_keyboard(F_ON(F_USE_FK,ps_global));
#ifdef OS2
	dont_interrupt();
#endif
	fix_windsize(ps_global);
    }
    else{
	EndInverse();
	end_keyboard(F_ON(F_USE_FK,ps_global));
	end_tty_driver(ps_global);
	end_screen(NULL, 0);
#ifdef OS2
	interrupt_ok();
#endif
    }

    return(0);
}



/*----------------------------------------------------------------------
     Suspend Pine. Reset tty and suspend. Suspend is finished when this returns

   Args:  The pine structure

 Result:  Execution suspended for a while. Screen will need redrawing 
          after this is done.

 Instead of the usual handling of ^Z by catching a signal, we actually read
the ^Z and then clean up the tty driver, then kill ourself to stop, and 
pick up where we left off when execution resumes.
  ----------------------------------------------------------------------*/
int
do_suspend() 
{
    struct pine *pine = ps_global;
    time_t now;
    int   isremote, retval;
#if defined(DOS) || defined(OS2)
    int   result, orig_cols, orig_rows;
#endif
#ifdef	DOS
    static char *shell = NULL;
#define	STD_SHELL	"COMMAND.COM"
#else
#ifdef	OS2
    static char *shell = NULL;
#define	STD_SHELL	"CMD.EXE"
#endif
#endif

    if(!have_job_control()){
	bogus_command(ctrl('Z'), F_ON(F_USE_FK, pine) ? "F1" : "?");
	return(NO_OP_COMMAND);
    }

    if(F_OFF(F_CAN_SUSPEND, pine)){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Pine suspension not enabled - see help text");
	return(NO_OP_COMMAND);
    }

#ifdef	_WINDOWS
    mswin_minimize();
    return(NO_OP_COMMAND);
#else

    isremote = (ps_global->mail_stream && ps_global->mail_stream->mailbox
		&& (*ps_global->mail_stream->mailbox == '{'
		    || (*ps_global->mail_stream->mailbox == '*'
			&& *(ps_global->mail_stream->mailbox + 1) == '{')));

    now = time((time_t *)0);
    dprint(1, (debugfile, "\n\n --- %s - SUSPEND ----  %s",
	       isremote ? "REMOTE" : "LOCAL", ctime(&now)));
    ttyfix(0);

#if defined(DOS) || defined(OS2)
    suspend_notice("exit");
    if (!shell){
	char *p;

	if (!((shell = getenv("SHELL")) || (shell = getenv("COMSPEC"))))
	  shell = STD_SHELL;

	shell = cpystr(shell);			/* copy in free storage */
	for(p = shell; p = strchr(p, '/'); p++)
	  *p = '\\';
    }

    result = system(shell);
#else
    if(F_ON(F_SUSPEND_SPAWNS, ps_global)){
	PIPE_S *syspipe;

	if(syspipe = open_system_pipe(NULL,NULL,NULL,PIPE_USER|PIPE_RESET,0)){
	    suspend_notice("exit");
#ifndef	SIGCHLD
	    if(isremote)
	      suspend_warning();
#endif
	    (void) close_system_pipe(&syspipe, NULL, 0);
	}
    }
    else{
	suspend_notice("fg");

	if(isremote)
	  suspend_warning();

	stop_process();
    }
#endif	/* DOS */

    now = time((time_t *)0);
    dprint(1, (debugfile, "\n\n ---- RETURN FROM SUSPEND ----  %s",
               ctime(&now)));
    
    ttyfix(1);

#if defined(DOS) || defined(OS2)
    orig_cols = pine->ttyo->screen_cols;
    orig_rows = pine->ttyo->screen_rows;
#endif

    fix_windsize(pine);

#if defined(DOS) || defined(OS2)
    if(orig_cols != pine->ttyo->screen_cols ||
       orig_rows != pine->ttyo->screen_rows)
	retval = KEY_RESIZE;
    else
#endif
      retval = ctrl('L');;
 
#if	defined(DOS) || defined(OS2)
    if(result == -1)
      q_status_message1(SM_ORDER | SM_DING, 3, 4,
			"Error loading \"%.200s\"", shell);
#endif

    if(isremote && !pine_mail_ping(ps_global->mail_stream))
      q_status_message(SM_ORDER | SM_DING, 4, 9,
		       "Suspended for too long, IMAP connection broken");

    return(retval);
#endif	/* !_WINDOWS */
}



/*----------------------------------------------------------------------
 ----*/
void
suspend_notice(s)
    char *s;
{
    printf("\nPine suspended. Give the \"%s\" command to come back.\n", s);
    fflush(stdout);
}



/*----------------------------------------------------------------------
 ----*/
void
suspend_warning()
{
    puts("Warning: Your IMAP connection will be closed if Pine");
    puts("is suspended for more than 30 minutes\n");
    fflush(stdout);
}



/*----------------------------------------------------------------------
 ----*/
void
fix_windsize(pine)
    struct pine *pine;
{
    int old_width = pine->ttyo->screen_cols;

    dprint(9, (debugfile, "fix_windsize()\n"));
    mark_keymenu_dirty();
    mark_status_dirty();
    mark_titlebar_dirty();
    clear_cursor_pos();
    get_windsize(pine->ttyo);

    if(old_width != pine->ttyo->screen_cols)
      clear_index_cache();
}


SigType (*hold_int) SIG_PROTO((int));
SigType (*hold_term) SIG_PROTO((int));
#if !defined(DOS) && !defined(OS2)
SigType (*hold_hup) SIG_PROTO((int));
SigType (*hold_usr2) SIG_PROTO((int));
#endif
static int critical_depth = 0;

/*----------------------------------------------------------------------
     Ignore signals when imap is running through critical code

 Args: stream -- The stream on which critical operation is proceeding
 ----*/

void 
mm_critical(stream)
     MAILSTREAM *stream;
{
    stream = stream; /* For compiler complaints that this isn't used */

    if(++critical_depth == 1){

#if !defined(DOS) && !defined(OS2)
	hold_hup  = signal(SIGHUP, SIG_IGN);
	hold_usr2 = signal(SIGUSR2, SIG_IGN);
#endif
	hold_int  = signal(SIGINT, SIG_IGN);
	hold_term = signal(SIGTERM, SIG_IGN);
    }

    dprint(9, (debugfile, "IMAP critical (depth now %d) on %s\n",
              critical_depth,
	      (stream && stream->mailbox) ? stream->mailbox : "<no folder>" ));
}



/*----------------------------------------------------------------------
   Reset signals after critical imap code
 ----*/
void
mm_nocritical(stream)
     MAILSTREAM *stream;
{ 
    stream = stream; /* For compiler complaints that this isn't used */

    if(--critical_depth == 0){

#if !defined(DOS) && !defined(OS2)
	(void)signal(SIGHUP, hold_hup);
	(void)signal(SIGUSR2, hold_usr2);
#endif
	(void)signal(SIGINT, hold_int);
	(void)signal(SIGTERM, hold_term);
    }

    critical_depth = max(critical_depth, 0);

    dprint(9, (debugfile, "Done with IMAP critical (depth now %d) on %s\n",
              critical_depth,
	      (stream && stream->mailbox) ? stream->mailbox : "<no folder>" ));
}


