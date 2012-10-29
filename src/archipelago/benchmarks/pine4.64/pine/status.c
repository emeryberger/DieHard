#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: status.c 14081 2005-09-12 22:04:25Z hubert@u.washington.edu $";
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
   1989-2005 by the University of Washington.

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
     status.c
     Functions that manage the status line (third from the bottom)
       - put messages on the queue to be displayed
       - display messages on the queue with timers 
       - check queue to figure out next timeout
       - prompt for yes/no type of questions
  ====*/

#include "headers.h"

/*
 * Internal queue of messages.  The circular, double-linked list's 
 * allocated on demand, and cleared as each message is displayed.
 */
typedef struct message {
    char	   *text;
    unsigned	    flags:8;
    unsigned	    shown:1;
    int		    min_display_time, max_display_time;
    struct message *next, *prev;
} SMQ_T;

#define	LAST_MESSAGE(X)	((X) == (X)->next)
#define	RAD_BUT_COL	0
#define WANT_TO_BUF     2500


/*
 * Keymenu for Modal Message display screen
 */
static struct key modal_message_keys[] =
       {NULL_MENU,
	NULL_MENU,
	{"Ret","Finished",{MC_EXIT,2,{ctrl('m'),ctrl('j')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU
       };
INST_KEY_MENU(modal_message_keymenu, modal_message_keys);


/*
 * Internal prototypes
 */
int  output_message PROTO((SMQ_T *));
int pre_screen_config_want_to PROTO((char *, int, int));
void radio_help PROTO((int, int, HelpType));
void draw_radio_prompt PROTO((int, int, int, char *));
void pause_for_current_message PROTO(());
int  messages_in_queue PROTO(());
void delay_cmd_cue PROTO((int));
int  modal_bogus_input PROTO((int));
ESCKEY_S *construct_combined_esclist PROTO((ESCKEY_S *, ESCKEY_S *));



/*----------------------------------------------------------------------
     Manage the second line from the bottom where status and error messages
are displayed. A small queue is set up and messages are put on the queue
by calling one of the q_status_message routines. Even though this is a queue
most of the time message will go right on through. The messages are 
displayed just before the read for the next command, or when a read times
out. Read timeouts occur every minute or so for new mail checking and every
few seconds when there are still messages on the queue. Hopefully, this scheme 
will not let messages fly past that the user can't see.
  ----------------------------------------------------------------------*/


static SMQ_T *message_queue = NULL;
static short  needs_clearing = 0, /* Flag set by want_to()
                                              and optionally_enter() */
	      prevstartcol;
static char   prevstatusbuff[MAX_SCREEN_COLS+1];
static time_t displayed_time;


/*----------------------------------------------------------------------
        Put a message for the status line on the queue

  Args: time    -- the min time in seconds to display the message
        message -- message string

  Result: queues message on queue represented by static variables

    This puts a single message on the queue to be shown.
  ----------*/
void
q_status_message(flags, min_time, max_time, message)
    int   flags;
    int   min_time,max_time;
    char *message;
{
    SMQ_T *new;
    char  *clean_msg;
    size_t mlen;

    /*
     * By convention, we have min_time equal to zero in messages which we
     * think are not as important, so-called comfort messages. We have
     * min_time >= 3 for messages which we think the user should see for
     * sure. Some users don't like to wait so we've provided a way for them
     * to live on the edge.
     *    status_msg_delay == -1  => min time == min(0, min_time)
     *    status_msg_delay == -2  => min time == min(1, min_time)
     *    status_msg_delay == -3  => min time == min(2, min_time)
     *    ...
     */
    if(ps_global->status_msg_delay < 0)
      min_time = min(-1 - ps_global->status_msg_delay, min_time);

    /*
     * The 41 is room for 40 escaped control characters plus a
     * terminating null.
     */
    mlen = strlen(message) + 40;
    clean_msg = (char *)fs_get(mlen + 1);
    istrncpy(clean_msg, message, mlen);		/* does the cleaning */
    clean_msg[mlen] = '\0';

    /* Hunt to last message -- if same already queued, move on... */
    if(new = message_queue){
	while(new->next != message_queue)
	  new = new->next;

	if(!strcmp(new->text, message)){
	    new->shown = 0;

	    if(new->min_display_time < min_time)
	      new->min_display_time = min_time;

	    if(new->max_display_time < max_time)
	      new->max_display_time = max_time;

	    if(clean_msg)
	      fs_give((void **)&clean_msg);

	    return;
	}
	else if(flags & SM_INFO){
	    if(clean_msg)
	      fs_give((void **)&clean_msg);

	    return;
	}
    }

    new = (SMQ_T *)fs_get(sizeof(SMQ_T));
    memset(new, 0, sizeof(SMQ_T));
    new->text = clean_msg;
    new->min_display_time = min_time;
    new->max_display_time = max_time;
    new->flags            = flags;
    if(message_queue){
	new->next = message_queue;
	new->prev = message_queue->prev;
	new->prev->next = message_queue->prev = new;
    }
    else
      message_queue = new->next = new->prev = new;

    dprint(9, (debugfile, "q_status_message(%s)\n",
	   clean_msg ? clean_msg : "?"));
}


/*----------------------------------------------------------------------
        Put a message with 1 printf argument on queue for status line
 
    Args: min_t -- minimum time to display message for
          max_t -- minimum time to display message for
          s -- printf style control string
          a -- argument for printf
 
   Result: message queued
  ----*/

/*VARARGS1*/
void
q_status_message1(flags, min_t, max_t, s, a)
    int	  flags;
    int   min_t, max_t;
    char *s;
    void *a;
{
    sprintf(tmp_20k_buf, s, a);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 2 printf argument on queue for status line

    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf

  Result: message queued
  ---*/

/*VARARGS1*/
void
q_status_message2(flags, min_t, max_t, s, a1, a2)
    int   flags;
    int   min_t, max_t;
    char *s;
    void *a1, *a2;
{
    sprintf(tmp_20k_buf, s, a1, a2);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 3 printf argument on queue for status line

    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf

  Result: message queued
  ---*/

/*VARARGS1*/
void
q_status_message3(flags, min_t, max_t, s, a1, a2, a3)
    int   flags;
    int   min_t, max_t;
    char *s;
    void *a1, *a2, *a3;
{
    sprintf(tmp_20k_buf, s, a1, a2, a3);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 4 printf argument on queue for status line


    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf
          a4 -- argument for printf

  Result: message queued
  ----------------------------------------------------------------------*/
/*VARARGS1*/
void
q_status_message4(flags, min_t, max_t, s, a1, a2, a3, a4)
    int   flags;
    int   min_t, max_t;
    char *s;
    void *a1, *a2, *a3, *a4;
{
    sprintf(tmp_20k_buf, s, a1, a2, a3, a4);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*VARARGS1*/
void
q_status_message5(flags, min_t, max_t, s, a1, a2, a3, a4, a5)
    int   flags;
    int   min_t, max_t;
    char *s;
    void *a1, *a2, *a3, *a4, *a5;
{
    sprintf(tmp_20k_buf, s, a1, a2, a3, a4, a5);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*VARARGS1*/
void
q_status_message6(flags, min_t, max_t, s, a1, a2, a3, a4, a5, a6)
    int   flags;
    int   min_t, max_t;
    char *s;
    void *a1, *a2, *a3, *a4, *a5, *a6;
{
    sprintf(tmp_20k_buf, s, a1, a2, a3, a4, a5, a6);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*----------------------------------------------------------------------
        Put a message with 7 printf argument on queue for status line


    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf
          a4 -- argument for printf
          a5 -- argument for printf
          a6 -- argument for printf
          a7 -- argument for printf


  Result: message queued
  ----------------------------------------------------------------------*/
/*VARARGS1*/
void
q_status_message7(flags, min_t, max_t, s, a1, a2, a3, a4, a5, a6, a7)
    int   flags;
    int   min_t, max_t;
    char *s;
    void *a1, *a2, *a3, *a4, *a5, *a6, *a7;
{
    sprintf(tmp_20k_buf, s, a1, a2, a3, a4, a5, a6, a7);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*VARARGS1*/
void
q_status_message8(flags, min_t, max_t, s, a1, a2, a3, a4, a5, a6, a7, a8)
    int   flags;
    int   min_t, max_t;
    char *s;
    void *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8;
{
    sprintf(tmp_20k_buf, s, a1, a2, a3, a4, a5, a6, a7, a8);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*----------------------------------------------------------------------
     Mark the status line as dirty so it gets cleared next chance
 ----*/
void
mark_status_dirty()
{
    mark_status_unknown();
    needs_clearing++;
}


/*----------------------------------------------------------------------
    Cause status line drawing optimization to be turned off, because we
    don't know what the status line looks like.
 ----*/
void
mark_status_unknown()
{
    prevstartcol = -1;
    prevstatusbuff[0]  = '\0';
}



/*----------------------------------------------------------------------
     Wait a suitable amount of time for the currently displayed message
 ----*/
void
pause_for_current_message()
{
    if(message_queue){
	int w;

	if(w = status_message_remaining()){
	    delay_cmd_cue(1);
	    sleep(w);
	    delay_cmd_cue(0);
	}

	d_q_status_message();
    }
}


/*----------------------------------------------------------------------
    Time remaining for current message's minimum display
 ----*/
int
status_message_remaining()
{
    if(message_queue){
	int d = (int)(min(displayed_time - time(0), 0))
					  + message_queue->min_display_time;
	return((d > 0) ? d : 0);
    }

    return(0);
}


/*----------------------------------------------------------------------
        Find out how many messages are queued for display

  Args:   dtime -- will get set to minimum display time for current message

  Result: number of messages in the queue.

  ---------*/
int
messages_queued(dtime)
    long *dtime;
{
    if(message_queue && dtime)
      *dtime = (long)max(message_queue->min_display_time, 1L);

    return((ps_global->in_init_seq) ? 0 : messages_in_queue());
}



/*----------------------------------------------------------------------
       Return number of messages in queue
  ---------*/
int
messages_in_queue()
{
    int	   n = message_queue ? 1 : 0;
    SMQ_T *p = message_queue;

    while(n && (p = p->next) != message_queue)
      n++;

    return(n);
}



/*----------------------------------------------------------------------
     Return last message queued
  ---------*/
char *
last_message_queued()
{
    SMQ_T *p, *r = NULL;

    if(p = message_queue){
	do
	  if(p->flags & SM_ORDER)
	    r = p;
	while((p = p->next) != message_queue);
    }

    return(r ? r->text : NULL);
}



/*----------------------------------------------------------------------
       Update status line, clearing or displaying a message

   Arg: command -- The command that is about to be executed

  Result: status line cleared or
             next message queued is displayed or
             current message is redisplayed.
	     if next message displayed, it's min display time
	     is returned else if message already displayed, it's
	     time remaining on the display is returned, else 0.

   This is called when ready to display the next message, usually just
before reading the next command from the user. We pass in the nature
of the command because it affects what we do here. If the command just
executed by the user is a redraw screen, we don't want to reset or go to 
next message because it might not have been seen.  Also if the command
is just a noop, which are usually executed when checking for new mail 
and happen every few minutes, we don't clear the message.

If it was really a command and there's nothing more to show, then we
clear, because we know the user has seen the message. In this case the
user might be typing commands very quickly and miss a message, so
there is a time stamp and time check that each message has been on the
screen for a few seconds.  If it hasn't we just return and let it be
taken care of next time.

At slow terminal output speeds all of this can be for naught, the amount
of time it takes to paint the screen when the whole screen is being painted
is greater than the second or two delay so the time stamps set here have
nothing to do with when the user actually sees the message.
----------------------------------------------------------------------*/
int
display_message(command)
    int command;
{
    if(ps_global == NULL || ps_global->ttyo == NULL
       || ps_global->ttyo->screen_rows < 1 || ps_global->in_init_seq)
      return(0);

    /*---- Deal with any previously displayed messages ----*/
    if(message_queue && message_queue->shown) {
	int rv = -1;

	if(command == ctrl('L')) {	/* just repaint it, and go on */
	    mark_status_unknown();
	    mark_keymenu_dirty();
	    mark_titlebar_dirty();
	    rv = 0;
	}
	else {				/* ensure sufficient time's passed */
	    time_t now;
	    int    diff;

	    now  = time(0);
	    diff = (int)(min(displayed_time - now, 0))
			+ ((command == NO_OP_COMMAND || command == NO_OP_IDLE)
			    ? message_queue->max_display_time
			    : message_queue->min_display_time);
            dprint(9, (debugfile,
		       "STATUS: diff:%d, displayed: %ld, now: %ld\n",
		       diff, (long) displayed_time, (long) now));
            if(diff > 0)
	      rv = diff;			/* check again next time  */
	    else if(LAST_MESSAGE(message_queue)
		    && (command == NO_OP_COMMAND || command == NO_OP_IDLE)
		    && message_queue->max_display_time)
	      rv = 0;				/* last msg, no cmd, has max */
	}

	if(rv >= 0){				/* leave message displayed? */
	    if(prevstartcol < 0)		/* need to redisplay it? */
	      output_message(message_queue);

	    return(rv);
	}
	  
	d_q_status_message();			/* remove it from queue and */
	needs_clearing++;			/* clear the line if needed */
    }

    if(!message_queue && (command == ctrl('L') || needs_clearing)) {
	int inverse;
	struct variable *vars = ps_global->vars;
	char *last_bg = NULL;

	dprint(9, (debugfile, "Clearing status line\n"));
	inverse = InverseState();	/* already in inverse? */
	if(inverse && pico_usingcolor() && VAR_STATUS_FORE_COLOR &&
	   VAR_STATUS_BACK_COLOR){
	    last_bg = pico_get_last_bg_color();
	    pico_set_nbg_color();   /* so ClearLine will clear in bg color */
	}

	ClearLine(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global));
	if(last_bg){
	    (void)pico_set_bg_color(last_bg);
	    if(last_bg)
	      fs_give((void **)&last_bg);
	}

	mark_status_unknown();
	if(command == ctrl('L')){
	    mark_keymenu_dirty();
	    mark_titlebar_dirty();
	}
    }

    /*---- Display any queued messages, weeding 0 disp times ----*/
    while(message_queue && !message_queue->shown)
      if(message_queue->min_display_time || LAST_MESSAGE(message_queue)){
	  displayed_time = time(0);
	  output_message(message_queue);
      }
      else{
	  if(message_queue->text){
	      char   buf[1000];
	      char  *append = " [not actually shown]";
	      char  *ptr;
	      size_t len;

	      len = strlen(message_queue->text) + strlen(append);
	      if(len < sizeof(buf))
		ptr = buf;
	      else
		ptr = (char *) fs_get((len+1) * sizeof(char));

	      strcpy(ptr, message_queue->text);
	      strcat(ptr, append);
	      add_review_message(ptr, -1);
	      if(ptr != buf)
		fs_give((void **) &ptr);
	  }

	  d_q_status_message();
      }

    needs_clearing = 0;				/* always cleared or written */
    dprint(9, (debugfile,
               "STATUS cmd:%d, max:%d, min%d\n", command, 
	       (message_queue) ? message_queue->max_display_time : -1,
	       (message_queue) ? message_queue->min_display_time : -1));
    fflush(stdout);
    return(0);
}



/*----------------------------------------------------------------------
     Display all the messages on the queue as quickly as possible
  ----*/
void
flush_status_messages(skip_last_pause)
    int skip_last_pause;
{
    while(message_queue){
	if(LAST_MESSAGE(message_queue)
	   && skip_last_pause
	   && message_queue->shown)
	  break;

	if(message_queue->shown)
	  pause_for_current_message();

	while(message_queue && !message_queue->shown)
	  if(message_queue->min_display_time
	     || LAST_MESSAGE(message_queue)){
	      displayed_time = time(0);
	      output_message(message_queue);
	  }
	  else
	    d_q_status_message();
    }
}



/*----------------------------------------------------------------------
     Make sure any and all SM_ORDER messages get displayed.

     Note: This flags the message line as having nothing displayed.
           The idea is that it's a function called by routines that want
	   the message line for a prompt or something, and that they're
	   going to obliterate the message anyway.
 ----*/
void
flush_ordered_messages()
{
    SMQ_T *start = NULL;

    while(message_queue && message_queue != start){
	if(message_queue->shown)
	  pause_for_current_message(); /* changes "message_queue" */

	while(message_queue && !message_queue->shown
	      && message_queue != start)
	  if(message_queue->flags & SM_ORDER){
	      if(message_queue->min_display_time){
		  displayed_time = time(0);
		  output_message(message_queue);
	      }
	      else
		d_q_status_message();
	  }
	  else if(!start)
	    start = message_queue;
    }
}


     
/*----------------------------------------------------------------------
      Remove a message from the message queue.
  ----*/
void
d_q_status_message()
{
    if(message_queue){
	dprint(9, (debugfile, "d_q_status_message(%.40s)\n",
	       message_queue->text ? message_queue->text : "?"));
	if(!LAST_MESSAGE(message_queue)){
	    SMQ_T *p = message_queue;
	    p->next->prev = p->prev;
	    message_queue = p->prev->next = p->next;
	    if(p->text)
	      fs_give((void *)&p->text);
	    fs_give((void **)&p);
	}
	else{
	    if(message_queue->text)
	      fs_give((void **)&message_queue->text);
	    fs_give((void **)&message_queue);
	}
    }
}



/*----------------------------------------------------------------------
    Actually output the message to the screen

  Args: message            -- The message to output
	from_alarm_handler -- Called from alarm signal handler.
			      We don't want to add this message to the review
			      message list since we may mess with the malloc
			      arena here, and the interrupt may be from
			      the middle of something malloc'ing.
 ----*/
int
status_message_write(message, from_alarm_handler)
    char *message;
    int   from_alarm_handler;
{
    int  col, row, max_length, invert;
    char obuff[MAX_SCREEN_COLS + 1];
    struct variable *vars = ps_global->vars;
    COLOR_PAIR *lastc = NULL, *newc;

    if(!from_alarm_handler)
      add_review_message(message, -1);

    invert = !InverseState();	/* already in inverse? */
    row = max(0, ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global));

    /* Put [] around message and truncate to screen width */
    max_length = ps_global->ttyo != NULL ? ps_global->ttyo->screen_cols : 80;
    max_length = min(max_length, MAX_SCREEN_COLS);
    obuff[0] = '[';
    obuff[1] = '\0';
    strncat(obuff, message, max_length - 2);
    obuff[max_length - 1] = '\0';
    strcat(obuff, "]");

    if(prevstartcol == -1 || strcmp(obuff, prevstatusbuff)){
	/*
	 * Try to optimize drawing in this case.  If the length of the string
	 * changed, it is very likely a lot different, so probably not
	 * worth looking at.  Just go down the two strings drawing the
	 * characters that have changed.
	 */
	if(prevstartcol != -1 && strlen(obuff) == strlen(prevstatusbuff)){
	    char *p, *q, *uneq_str;
	    int   column, start_col;

	    if(pico_usingcolor() && VAR_STATUS_FORE_COLOR &&
	       VAR_STATUS_BACK_COLOR &&
	       pico_is_good_color(VAR_STATUS_FORE_COLOR) &&
	       pico_is_good_color(VAR_STATUS_BACK_COLOR)){
		lastc = pico_get_cur_color();
		if(lastc){
		    newc = new_color_pair(VAR_STATUS_FORE_COLOR,
					  VAR_STATUS_BACK_COLOR);
		    (void)pico_set_colorp(newc, PSC_NONE);
		    free_color_pair(&newc);
		}
	    }
	    else if(invert)
	      StartInverse();

	    q = prevstatusbuff;
	    p = obuff;
	    col = column = prevstartcol;

	    while(*q){
		/* skip over string of equal characters */
		while(*q && *p == *q){
		    q++;
		    p++;
		    column++;
		}

		if(!*q)
		  break;

		uneq_str  = p;
		start_col = column;

		/* find end of string of unequal characters */
		while(*q && *p != *q){
		    *q++ = *p++;  /* update prevstatusbuff */
		    column++;
		}

		/* tie off and draw the changed chars */
		*p = '\0';
		PutLine0(row, start_col, uneq_str);

		if(*q){
		    p++;
		    q++;
		    column++;
		}
	    }

	    if(lastc){
		(void)pico_set_colorp(lastc, PSC_NONE);
		free_color_pair(&lastc);
	    }
	    else if(invert)
	      EndInverse();

	    /* move cursor to a consistent position */
	    MoveCursor(row, 0);
	    fflush(stdout);
	}
	else{
	    if(pico_usingcolor())
	      lastc = pico_get_cur_color();

	    if(!invert && pico_usingcolor() && VAR_STATUS_FORE_COLOR &&
	       VAR_STATUS_BACK_COLOR &&
	       pico_is_good_color(VAR_STATUS_FORE_COLOR) &&
	       pico_is_good_color(VAR_STATUS_BACK_COLOR))
	      pico_set_nbg_color();	/* so ClearLine uses bg color */

	    ClearLine(row);

	    if(pico_usingcolor() && VAR_STATUS_FORE_COLOR &&
	       VAR_STATUS_BACK_COLOR &&
	       pico_is_good_color(VAR_STATUS_FORE_COLOR) &&
	       pico_is_good_color(VAR_STATUS_BACK_COLOR)){
		if(lastc){
		    newc = new_color_pair(VAR_STATUS_FORE_COLOR,
					  VAR_STATUS_BACK_COLOR);
		    (void)pico_set_colorp(newc, PSC_NONE);
		    free_color_pair(&newc);
		}
	    }
	    else if(invert){
		if(lastc)
		  free_color_pair(&lastc);

		StartInverse();
	    }


	    col = Centerline(row, obuff);
	    if(lastc){
		(void)pico_set_colorp(lastc, PSC_NONE);
		free_color_pair(&lastc);
	    }
	    else if(invert)
	      EndInverse();

	    MoveCursor(row, 0);
	    fflush(stdout);
	    strcpy(prevstatusbuff, obuff);
	    prevstartcol = col;
	}
    }
    else
      col = prevstartcol;

    return(col);
}



/*----------------------------------------------------------------------
    Write the given status message to the display.

  Args: mq_entry -- pointer to message queue entry to write.
 
 ----*/
int 
output_message(mq_entry)
    SMQ_T *mq_entry;
{
    int rv = 0;

    dprint(9, (debugfile, "output_message(%s)\n",
	   (mq_entry && mq_entry->text) ? mq_entry->text : "?"));

    if((mq_entry->flags & SM_DING) && F_OFF(F_QUELL_BEEPS, ps_global))
      Writechar(BELL, 0);			/* ring bell */
      /* flush() handled below */

    if(!(mq_entry->flags & SM_MODAL)){
	rv = status_message_write(mq_entry->text, 0);
    	if(ps_global->status_msg_delay > 0){
	    MoveCursor(ps_global->ttyo->screen_rows-FOOTER_ROWS(ps_global), 0);
	    fflush(stdout);
	    sleep(ps_global->status_msg_delay);
	}

	mq_entry->shown = 1;
    }
    else if (!mq_entry->shown){
	int	  i      = 0,
		  pad    = max(0, (ps_global->ttyo->screen_cols - 59) / 2);
	char	 *p, *q, *s, *t;
	SMQ_T	 *m;
	SCROLL_S  sargs;
	
	/* Count the number of modal messsages and add up their lengths. */
	for(m = mq_entry->next; m != mq_entry; m = m->next)
	  if((m->flags & SM_MODAL) && !m->shown){
	      i++;
	  }

	sprintf(tmp_20k_buf, "%*s%s\n%*s%s\n%*s%s\n%*s%s\n%*s%s\n%*s%s\n%*s%s\n\n", 
		/*        1         2         3         4         5         6*/
		/*23456789012345678901234567890123456789012345678901234567890*/
		pad, "",
		"***********************************************************",
		pad, "", i ? 
		"* What follows are advisory messages.  After reading them *" :
		"* What follows is an advisory message.  After reading it  *", 
		
		pad, "",
		"* simply hit \"Return\" to continue your Pine session.      *", 
		pad, "",
		"*                                                         *",
		pad, "", i ?
		"* To review these messages later, press 'J' from the      *" :
		"* To review this message later, press 'J' from the        *",
		pad, "",
		"* MAIN MENU.                                              *",
		pad, "",
		"***********************************************************");
	t = tmp_20k_buf + strlen(tmp_20k_buf);	

	m = mq_entry;
	do{
	    if((m->flags & SM_MODAL) && !m->shown){
		int   indent;

		indent = ps_global->ttyo->screen_cols > 80 
		         ? (ps_global->ttyo->screen_cols - 80) / 3 : 0;
		
		if(t - tmp_20k_buf > 19000){
		    sprintf(t, "\n%*s* * *  Running out of buffer space   * * *", indent, "");
		    t += strlen(t);
		    sprintf(t, "\n%*s* * * Press RETURN for more messages * * *", indent, "");
		    break;
		}
		
		add_review_message(m->text, -1);
		
		if (p = strstr(m->text, "[ALERT]")){
		    sprintf(t, "%*.*s\n", indent + p - m->text, p - m->text, m->text);
		    t += strlen(t);

		    for(p += 7; *p && isspace((unsigned char)*p); p++)
		      ;
		    indent += 8;
		}
		else{
		    p = m->text;
		}
		
		while(strlen(p) > ps_global->ttyo->screen_cols - 2 * indent){
		    for(q = p + ps_global->ttyo->screen_cols - 2 * indent; 
			q > p && !isspace((unsigned char)*q); q--)
		      ;

		    sprintf(t, "\n%*.*s", indent + q - p, q - p, p);
		    t += strlen(t);
		    p = q + 1;
		}

		sprintf(t, "\n%*s%s", indent, "", p);
		t += strlen(t);
		
		if(i--){
		    sprintf(t, "\n\n%*s\n", pad + 30, "-  -  -");
		    t += strlen(t);
		}

		m->shown = 1;
	    }
	    m = m->next;
	} while(m != mq_entry);

	s = cpystr(tmp_20k_buf);
	ClearLine(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global));

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text	  = s;
	sargs.text.src	  = CharStar;
	sargs.bar.title	  = "Status Message";
	sargs.bogus_input = modal_bogus_input;
	sargs.no_stat_msg = 1;
	sargs.keys.menu   = &modal_message_keymenu;
	setbitmap(sargs.keys.bitmap);

	scrolltool(&sargs);

	fs_give((void **)&s);
	ps_global->mangled_screen = 1;
    }
    return(rv);
}



/*----------------------------------------------------------------------
    Write or clear delay cue

  Args: on -- whether to turn it on or not
 
 ----*/
void
delay_cmd_cue(on)
    int on;
{
    int l, screen_edge = 0;
    COLOR_PAIR *lastc;
    struct variable *vars = ps_global->vars;

    if(prevstartcol >= 0 && (l = strlen(prevstatusbuff))){
	screen_edge = (prevstartcol == 0) || 
	  (prevstartcol + l >= ps_global->ttyo->screen_cols);
	MoveCursor(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global),
		   prevstartcol ? max(prevstartcol - 1, 0) : 0);
	lastc = pico_set_colors(VAR_STATUS_FORE_COLOR, VAR_STATUS_BACK_COLOR,
				PSC_REV|PSC_RET);

	Write_to_screen(on ? (screen_edge ? ">" : "[>") : 
			(screen_edge ? "[" : " ["));

	MoveCursor(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global),
		   min(prevstartcol + l, ps_global->ttyo->screen_cols) - 1);

	Write_to_screen(on ? (screen_edge ? "<" : "<]") : 
			(screen_edge ? "]" : "] "));

	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}

	MoveCursor(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global), 0);
    }

    fflush(stdout);
#ifdef	_WINDOWS
    mswin_setcursor ((on) ? MSWIN_CURSOR_BUSY : MSWIN_CURSOR_ARROW);
#endif
}


/*
 * modal_bogus_input - used by scrolltool to complain about
 *		       invalid user input.
 */
int
modal_bogus_input(ch)
    int ch;		
{
    char s[MAX_SCREEN_COLS+1];
		
    sprintf(s, "Command \"%.*s\" not allowed.  Press RETURN to continue Pine.",
	    sizeof(s)-70, pretty_command(ch));
    status_message_write(s, 0);
    Writechar(BELL, 0);
    return(0);
}




/*
 * want_to's array passed to radio_buttions...
 */
static ESCKEY_S yorn[] = {
    {'y', 'y', "Y", "Yes"},
    {'n', 'n', "N", "No"},
    {-1, 0, NULL, NULL}
};

int
pre_screen_config_want_to(question, dflt, on_ctrl_C)
     char    *question;
     int      dflt, on_ctrl_C;
{
    int ret = 0;
    char rep[WANT_TO_BUF], *p;
#ifdef _WINDOWS
    rep[0] = '\0';
    mswin_flush();
    if(strlen(question) + 3 < WANT_TO_BUF)
      sprintf(rep, "%s ?", question);
    switch (mswin_yesno (*rep ? rep : question)) {
      default:
      case 0:   return (on_ctrl_C);
      case 1:   return ('y');
      case 2:   return ('n');
    }
#endif
    while(!ret){
      fprintf(stdout, "%s? [%c]:", question, dflt);
      fgets(rep, WANT_TO_BUF, stdin);
      if((p = strpbrk(rep, "\r\n")) != NULL)
	*p = '\0';
      switch(*rep){
        case 'Y':
        case 'y':
	  ret = (int)'y';
	  break;
        case 'N':
        case 'n':
	  ret = (int)'n';
	  break;
        case '\0':
	  ret = dflt;
	  break;
        default:
	  break;
      }
    }
    return ret;
}

/*----------------------------------------------------------------------
     Ask a yes/no question in the status line

   Args: question     -- string to prompt user with
         dflt         -- The default answer to the question (should probably
			 be y or n)
         on_ctrl_C    -- Answer returned on ^C
	 help         -- Two line help text
	 flags        -- Flags to modify behavior
			 WT_FLUSH_IN      - Discard pending input.
			 WT_SEQ_SENSITIVE - Caller is sensitive to sequence
			                    number changes caused by
					    unsolicited expunges while we're
					    viewing a message.

 Result: Messes up the status line,
         returns y, n, dflt, on_ctrl_C, or SEQ_EXCEPTION
  ---*/
int
want_to(question, dflt, on_ctrl_C, help, flags)
    char	*question;
    HelpType	help;
    int		dflt, on_ctrl_C, flags;
{
    char *q2;
    int	  rv;

    if(!ps_global->ttyo)
      return(pre_screen_config_want_to(question, dflt, on_ctrl_C));
#ifdef _WINDOWS
    if (mswin_usedialog ()) {
	mswin_flush ();
	switch (mswin_yesno (question)) {
	default:
	case 0:		return (on_ctrl_C);
	case 1:		return ('y');
	case 2:		return ('n');
        }
    }
#endif

    /*----
       One problem with adding the (y/n) here is that shrinking the 
       screen while in radio_buttons() will cause it to get chopped
       off. It would be better to truncate the question passed in
       here and leave the full "(y/n) [x] : " on.
      ----*/
    q2 = fs_get(strlen(question) + 6);
    sprintf(q2, "%.*s? ", ps_global->ttyo->screen_cols - 6, question);
    if(on_ctrl_C == 'n')	/* don't ever let cancel == 'n' */
      on_ctrl_C = 0;

    rv = radio_buttons(q2,
	(ps_global->ttyo->screen_rows > 4) ? - FOOTER_ROWS(ps_global) : -1,
	yorn, dflt, on_ctrl_C, help, flags, NULL);
    fs_give((void **)&q2);

    return(rv);
}


int
one_try_want_to(question, dflt, on_ctrl_C, help, flags)
    char      *question;
    HelpType   help;
    int    dflt, on_ctrl_C, flags;
{
    char     *q2;
    int	      rv;

    q2 = fs_get(strlen(question) + 6);
    sprintf(q2, "%.*s? ", ps_global->ttyo->screen_cols - 6, question);
    rv = radio_buttons(q2,
	(ps_global->ttyo->screen_rows > 4) ? - FOOTER_ROWS(ps_global) : -1,
	yorn, dflt, on_ctrl_C, help, flags | RB_ONE_TRY, NULL);
    fs_give((void **)&q2);

    return(rv);
}



/*----------------------------------------------------------------------
    Prompt user for a choice among alternatives

Args --  prompt:    The prompt for the question/selection
         line:      The line to prompt on, if negative then relative to bottom
         esc_list:  ESC_KEY_S list of keys
         dflt:	    The selection when the <CR> is pressed (should probably
		      be one of the chars in esc_list)
         on_ctrl_C: The selection when ^C is pressed
         help_text: Text to be displayed on bottom two lines
	 flags:     Logically OR'd flags modifying our behavior to:
		RB_FLUSH_IN    - Discard any pending input chars.
		RB_ONE_TRY     - Only give one chance to answer.  Returns
				 on_ctrl_C value if not answered acceptably
				 on first try.
		RB_NO_NEWMAIL  - Quell the usual newmail check.
		RB_SEQ_SENSITIVE - The caller is sensitive to sequence number
				   changes so return on_ctrl_C if an
				   unsolicited expunge happens while we're
				   viewing a message.
		RB_RET_HELP    - Instead of the regular internal handling
				 way of handling help_text, this just causes
				 radio_buttons to return 3 when help is
				 asked for, so that the caller handles it
				 instead.
	
	 Note: If there are enough keys in the esc_list to need a second
	       screen, and there is no help, then the 13th key will be
	       put in the help position.

Result -- Returns the letter pressed. Will be one of the characters in the
          esc_list argument, or dflt, or on_ctrl_C, or SEQ_EXCEPTION.

This will pause for any new status message to be seen and then prompt the user.
The prompt will be truncated to fit on the screen. Redraw and resize are
handled along with ^Z suspension. Typing ^G will toggle the help text on and
off. Character types that are not buttons will result in a beep (unless one_try
is set).
  ----*/
int
radio_buttons(prompt, line, esc_list, dflt, on_ctrl_C, help_text, flags,
	      passed_in_km_popped)
    char     *prompt;
    int	      line;
    ESCKEY_S *esc_list;
    int       dflt;
    int       on_ctrl_C;
    HelpType  help_text;
    int	      flags;
    int      *passed_in_km_popped;
{
    register int     ch, real_line;
    char            *q, *ds = NULL;
    int              max_label, i, start, maxcol, fkey_table[12];
    int		     km_popped = 0;
    struct key	     rb_keys[12];
    struct key_menu  rb_keymenu;
    bitmap_t	     bitmap;
    struct variable *vars = ps_global->vars;
    COLOR_PAIR      *lastc = NULL, *promptc = NULL;
#ifdef	_WINDOWS
    int		     cursor_shown;
#endif

#ifdef _WINDOWS
    if (mswin_usedialog ()) {
	MDlgButton		button_list[25];
	int			b;
	int			i;
	int			ret;
	char			**help;

	memset (&button_list, 0, sizeof (MDlgButton) * 25);
	b = 0;

	if(flags & RB_RET_HELP){
	    if(help_text != NO_HELP)
		panic("RET_HELP and help in radio_buttons!");

	    button_list[b].ch = '?';
	    button_list[b].rval = 3;
	    button_list[b].name = "?";
	    button_list[b].label = "Help";
	    ++b;
	}

	for (i = 0; esc_list && esc_list[i].ch != -1 && i < 23; ++i) {
	  if(esc_list[i].ch != -2){
	    button_list[b].ch = esc_list[i].ch;
	    button_list[b].rval = esc_list[i].rval;
	    button_list[b].name = esc_list[i].name;
	    button_list[b].label = esc_list[i].label;
	    ++b;
	  }
	}
	button_list[b].ch = -1;
	
	help = get_help_text (help_text);

	ret = mswin_select (prompt, button_list, dflt, on_ctrl_C, 
			help, flags);
	free_list_array(&help);
	return (ret);
    }
#endif

    if(passed_in_km_popped){
	km_popped = *passed_in_km_popped;
	if(FOOTER_ROWS(ps_global) == 1 && km_popped){
	    FOOTER_ROWS(ps_global) = 3;
	}
    }

    suspend_busy_alarm();
    flush_ordered_messages();		/* show user previous status msgs */
    mark_status_dirty();		/* clear message next display call */
    real_line = line > 0 ? line : ps_global->ttyo->screen_rows + line;
    MoveCursor(real_line, RAD_BUT_COL);
    CleartoEOLN();

    /*---- Find longest label ----*/
    max_label = 0;
    for(i = 0; esc_list && esc_list[i].ch != -1 && i < 11; i++){
      if(esc_list[i].ch == -2) /* -2 means to skip this key and leave blank */
	continue;
      if(esc_list[i].name)
        max_label = max(max_label, strlen(esc_list[i].name));
    }

    maxcol = ps_global->ttyo->screen_cols - max_label - 1;

    /*
     * We need to be able to truncate q, so copy it in case it is
     * a readonly string.
     */
    q = cpystr(prompt);

    /*---- Init structs for keymenu ----*/
    for(i = 0; i < 12; i++)
      memset((void *)&rb_keys[i], 0, sizeof(struct key));

    memset((void *)&rb_keymenu, 0, sizeof(struct key_menu));
    rb_keymenu.how_many = 1;
    rb_keymenu.keys     = rb_keys;

    /*---- Setup key menu ----*/
    start = 0;
    clrbitmap(bitmap);
    memset(fkey_table, NO_OP_COMMAND, 12 * sizeof(int));
    if(flags & RB_RET_HELP && help_text != NO_HELP)
      panic("RET_HELP and help in radio_buttons!");

    /* if shown, always at position 0 */
    if(help_text != NO_HELP || flags & RB_RET_HELP){
	rb_keymenu.keys[0].name  = "?";
	rb_keymenu.keys[0].label = "Help";
	setbitn(0, bitmap);
	fkey_table[0] = ctrl('G');
	start++;
    }

    if(on_ctrl_C){
	rb_keymenu.keys[1].name  = "^C";
	rb_keymenu.keys[1].label = "Cancel";
	setbitn(1, bitmap);
	fkey_table[1] = ctrl('C');
	start++;
    }

    start = start ? 2 : 0;
    /*---- Show the usual possible keys ----*/
    for(i=start; esc_list && esc_list[i-start].ch != -1; i++){
	/*
	 * If we have an esc_list item we'd like to put in the non-existent
	 * 13th slot, and there is no help, we put it in the help slot
	 * instead.  We're hacking now...!
	 *
	 * We may also have invisible esc_list items that don't show up
	 * on the screen.  We use this when we have two different keys
	 * which are synonyms, like ^P and KEY_UP.  If all the slots are
	 * already full we can still fit invisible keys off the screen to
	 * the right.  A key is invisible if it's label is "".
	 */
	if(i >= 12){
	    if(esc_list[i-start].label
	       && esc_list[i-start].label[0] != '\0'){  /* visible */
		if(i == 12){  /* special case where we put it in help slot */
		    if(help_text != NO_HELP)
		  panic("Programming botch in radio_buttons(): too many keys");

		    if(esc_list[i-start].ch != -2)
		      setbitn(0, bitmap); /* the help slot */

		    fkey_table[0] = esc_list[i-start].ch;
		    rb_keymenu.keys[0].name  = esc_list[i-start].name;
		    if(esc_list[i-start].ch != -2
		       && esc_list[i-start].rval == dflt
		       && esc_list[i-start].label){
			ds = (char *)fs_get((strlen(esc_list[i-start].label)+3)
					    * sizeof(char));
			sprintf(ds, "[%s]", esc_list[i-start].label);
			rb_keymenu.keys[0].label = ds;
		    }
		    else
		      rb_keymenu.keys[0].label = esc_list[i-start].label;
		}
		else
		  panic("Botch in radio_buttons(): too many keys");
	    }
	}
	else{
	    if(esc_list[i-start].ch != -2)
	      setbitn(i, bitmap);

	    fkey_table[i] = esc_list[i-start].ch;
	    rb_keymenu.keys[i].name  = esc_list[i-start].name;
	    if(esc_list[i-start].ch != -2
	       && esc_list[i-start].rval == dflt
	       && esc_list[i-start].label){
		ds = (char *)fs_get((strlen(esc_list[i-start].label) + 3)
				    * sizeof(char));
		sprintf(ds, "[%s]", esc_list[i-start].label);
		rb_keymenu.keys[i].label = ds;
	    }
	    else
	      rb_keymenu.keys[i].label = esc_list[i-start].label;
	}
    }

    for(; i < 12; i++)
      rb_keymenu.keys[i].name = NULL;

    ps_global->mangled_footer = 1;

#ifdef	_WINDOWS
    cursor_shown = mswin_showcaret(1);
#endif

    if(pico_usingcolor() && VAR_PROMPT_FORE_COLOR &&
       VAR_PROMPT_BACK_COLOR &&
       pico_is_good_color(VAR_PROMPT_FORE_COLOR) &&
       pico_is_good_color(VAR_PROMPT_BACK_COLOR)){
	lastc = pico_get_cur_color();
	if(lastc){
	    promptc = new_color_pair(VAR_PROMPT_FORE_COLOR,
				     VAR_PROMPT_BACK_COLOR);
	    (void)pico_set_colorp(promptc, PSC_NONE);
	}
    }
    else
      StartInverse();

    draw_radio_prompt(real_line, RAD_BUT_COL, maxcol, q);

    while(1){
        fflush(stdout);

	/*---- Paint the keymenu ----*/
	if(lastc)
	  (void)pico_set_colorp(lastc, PSC_NONE);
	else
	  EndInverse();

	draw_keymenu(&rb_keymenu, bitmap, ps_global->ttyo->screen_cols,
		     1 - FOOTER_ROWS(ps_global), 0, FirstMenu);
	if(promptc)
	  (void)pico_set_colorp(promptc, PSC_NONE);
	else
	  StartInverse();

	MoveCursor(real_line, min(RAD_BUT_COL+strlen(q), maxcol+1));

	if(flags & RB_FLUSH_IN)
	  flush_input();

  newcmd:
	/* Timeout 5 min to keep imap mail stream alive */
        ch = read_char(600);
        dprint(2, (debugfile,
                   "Want_to read: %s (%d)\n", pretty_command(ch), ch));
	if(isascii(ch) && isupper((unsigned char)ch))
	  ch = tolower((unsigned char)ch);

	if(F_ON(F_USE_FK,ps_global)
	   && ((isascii(ch) && isalpha((unsigned char)ch) && !strchr("YyNn",ch))
	       || ((ch >= PF1 && ch <= PF12)
		   && (ch = fkey_table[ch - PF1]) == NO_OP_COMMAND))){
	    /*
	     * The funky test above does two things.  It maps
	     * esc_list character commands to function keys, *and* prevents
	     * character commands from input while in function key mode.
	     * NOTE: this breaks if we ever need more than the first
	     * twelve function keys...
	     */
	    if(flags & RB_ONE_TRY){
		ch = on_ctrl_C;
	        goto out_of_loop;
	    }
	    Writechar(BELL, 0);
	    continue;
	}

        switch(ch) {

          default:
	    for(i = 0; esc_list && esc_list[i].ch != -1; i++)
	      if(ch == esc_list[i].ch){
		  int len, n;

		  MoveCursor(real_line,len=min(RAD_BUT_COL+strlen(q),maxcol+1));
		  for(n = 0, len = ps_global->ttyo->screen_cols - len;
		      esc_list[i].label && esc_list[i].label[n] && len > 0;
		      n++, len--)
		    Writechar(esc_list[i].label[n], 0);

		  ch = esc_list[i].rval;
		  goto out_of_loop;
	      }

	    if(flags & RB_ONE_TRY){
		ch = on_ctrl_C;
	        goto out_of_loop;
	    }
	    Writechar(BELL, 0);
	    break;

          case ctrl('M'):
          case ctrl('J'):
            ch = dflt;
	    for(i = 0; esc_list && esc_list[i].ch != -1; i++)
	      if(ch == esc_list[i].rval){
		  int len, n;

		  MoveCursor(real_line,len=min(RAD_BUT_COL+strlen(q),maxcol+1));
		  for(n = 0, len = ps_global->ttyo->screen_cols - len;
		      esc_list[i].label && esc_list[i].label[n] && len > 0;
		      n++, len--)
		    Writechar(esc_list[i].label[n], 0);
		  break;
	      }
            goto out_of_loop;

          case ctrl('C'):
	    if(on_ctrl_C || (flags & RB_ONE_TRY)){
		ch = on_ctrl_C;
		goto out_of_loop;
	    }

	    Writechar(BELL, 0);
	    break;


          case '?':
          case ctrl('G'):
	    if(FOOTER_ROWS(ps_global) == 1 && km_popped == 0){
		km_popped++;
		FOOTER_ROWS(ps_global) = 3;
		line = -3;
		real_line = ps_global->ttyo->screen_rows + line;
		if(lastc)
		  (void)pico_set_colorp(lastc, PSC_NONE);
		else
		  EndInverse();

		clearfooter(ps_global);
		if(promptc)
		  (void)pico_set_colorp(promptc, PSC_NONE);
		else
		  StartInverse();

		draw_radio_prompt(real_line, RAD_BUT_COL, maxcol, q);
		break;
	    }

	    if(flags & RB_RET_HELP){
		ch = 3;
		goto out_of_loop;
	    }
	    else if(help_text != NO_HELP && FOOTER_ROWS(ps_global) > 1){
		mark_keymenu_dirty();
		if(lastc)
		  (void)pico_set_colorp(lastc, PSC_NONE);
		else
		  EndInverse();

		MoveCursor(real_line + 1, RAD_BUT_COL);
		CleartoEOLN();
		MoveCursor(real_line + 2, RAD_BUT_COL);
		CleartoEOLN();
		radio_help(real_line, RAD_BUT_COL, help_text);
		sleep(5);
		MoveCursor(real_line, min(RAD_BUT_COL+strlen(q), maxcol+1));
		if(promptc)
		  (void)pico_set_colorp(promptc, PSC_NONE);
		else
		  StartInverse();
	    }
	    else
	      Writechar(BELL, 0);

            break;
            

          case NO_OP_COMMAND:
	    goto newcmd;		/* misunderstood escape? */

          case NO_OP_IDLE:		/* UNODIR, keep the stream alive */
	    if(flags & RB_NO_NEWMAIL)
	      goto newcmd;

	    i = new_mail(0, VeryBadTime, NM_DEFER_SORT);
	    if(sp_expunge_count(ps_global->mail_stream)
	       && flags & RB_SEQ_SENSITIVE){
		if(on_ctrl_C)
		  ch = on_ctrl_C;
		else
		  ch = SEQ_EXCEPTION;

		goto out_of_loop;
	    }

	    if(i < 0)
	      break;		/* no changes, get on with life */
            /* Else fall into redraw to adjust displayed numbers and such */


          case KEY_RESIZE:
          case ctrl('L'):
            real_line = line > 0 ? line : ps_global->ttyo->screen_rows + line;
	    if(lastc)
	      (void)pico_set_colorp(lastc, PSC_NONE);
	    else
	      EndInverse();

            ClearScreen();
            redraw_titlebar();
            if(ps_global->redrawer != NULL)
              (*ps_global->redrawer)();
	    if(FOOTER_ROWS(ps_global) == 3 || km_popped)
              redraw_keymenu();

	    maxcol = ps_global->ttyo->screen_cols - max_label - 1;
	    if(promptc)
	      (void)pico_set_colorp(promptc, PSC_NONE);
	    else
	      StartInverse();

	    draw_radio_prompt(real_line, RAD_BUT_COL, maxcol, q);
            break;

        } /* switch */
    }

  out_of_loop:

#ifdef	_WINDOWS
    if(!cursor_shown)
      mswin_showcaret(0);
#endif

    fs_give((void **)&q);
    if(ds)
      fs_give((void **)&ds);

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
	if(promptc)
	  free_color_pair(&promptc);
    }
    else
      EndInverse();

    fflush(stdout);
    resume_busy_alarm(0);
    if(km_popped){
	FOOTER_ROWS(ps_global) = 1;
	clearfooter(ps_global);
	ps_global->mangled_body = 1;
	if(passed_in_km_popped)
	  *passed_in_km_popped = km_popped;	/* return current value */
    }

    return(ch);
}


#define OTHER_RETURN_VAL 1300
#define KEYS_PER_LIST 8

/*
 * This should really be part of radio_buttons itself, I suppose. It was
 * easier to do it this way. This is for when there are more than 12
 * possible commands. We could have all the radio_buttons calls call this
 * instead of radio_buttons, or rename this to radio_buttons.
 *
 * Radio_buttons is limited to 10 visible commands unless there is no Help,
 * in which case it is 11 visible commands.
 * Double_radio_buttons is limited to 16 visible commands because it uses
 * slots 3 and 4 for readability and the OTHER CMD.
 */
int
double_radio_buttons(prompt, line, esc_list, dflt, on_ctrl_C, help_text,
		     flags)
    char     *prompt;
    int	      line;
    ESCKEY_S *esc_list;
    int       dflt;
    int       on_ctrl_C;
    HelpType  help_text;
    int	      flags;
{
    ESCKEY_S *list = NULL, *list1 = NULL, *list2 = NULL;
    int       count, i = 0, j;
    int       v = OTHER_RETURN_VAL, listnum = 0;
    int       preserve_km_popped_for_radio_buttons = 0;

#ifdef _WINDOWS
    if(mswin_usedialog())
      return(radio_buttons(prompt, line, esc_list, dflt, on_ctrl_C,
			   help_text, flags, NULL));
#endif

    /* check to see if it will all fit in one */
    while(esc_list && esc_list[i].ch != -1)
      i++;

    i++;		/* for ^C */
    if(help_text != NO_HELP)
      i++;
    
    if(i <= 12)
      return(radio_buttons(prompt, line, esc_list, dflt, on_ctrl_C,
			   help_text, flags, NULL));

    /*
     * Won't fit, split it into two lists.
     *
     * We can fit at most 8 items in the visible list. The rest of
     * the commands have to be invisible. Each of list1 and list2 should
     * have no more than 8 visible (name != "" || label != "") items.
     */
    list1 = (ESCKEY_S *)fs_get((KEYS_PER_LIST+1) * sizeof(*list1));
    memset(list1, 0, (KEYS_PER_LIST+1) * sizeof(*list1));
    list2 = (ESCKEY_S *)fs_get((KEYS_PER_LIST+1) * sizeof(*list2));
    memset(list2, 0, (KEYS_PER_LIST+1) * sizeof(*list2));

    for(j=0,i=0; esc_list[i].ch != -1 && j < KEYS_PER_LIST; j++,i++)
      list1[j] = esc_list[i];
    
    list1[j].ch = -1;

    for(j=0; esc_list[i].ch != -1 && j < KEYS_PER_LIST; j++,i++)
      list2[j] = esc_list[i];

    list2[j].ch = -1;

    list = construct_combined_esclist(list1, list2);

    while(v == OTHER_RETURN_VAL){
	int prompt_line;

	/*
	 * There are some layering violations going on with this
	 * km_popped stuff. The line is passed in from high above us
	 * normally, but with this mechanism here we're hardwiring the
	 * knowledge that FOOTER_ROWS is either 1 or 3, and using that
	 * below. Radio_buttons itself always puts back the correct
	 * FOOTER_ROWS before exiting and then restores it based on
	 * the passed in km_popped and line, which is the second
	 * layering violation. We could fix it but it's probably not
	 * worth the effort.
	 */
	if(line == -1 && preserve_km_popped_for_radio_buttons)
	  prompt_line = -3;
	else
	  prompt_line = line;

	v = radio_buttons(prompt, prompt_line, list, dflt, on_ctrl_C,
			  help_text,flags,
			  &preserve_km_popped_for_radio_buttons);
	if(v == OTHER_RETURN_VAL){
	    fs_give((void **)&list);
	    listnum = 1 - listnum;
	    list = construct_combined_esclist(listnum ? list2 : list1,
					      listnum ? list1 : list2);
	}
    }

    if(list)
      fs_give((void **)&list);
    if(list1)
      fs_give((void **)&list1);
    if(list2)
      fs_give((void **)&list2);

    return(v);
}


ESCKEY_S *
construct_combined_esclist(list1, list2)
    ESCKEY_S *list1, *list2;
{
    ESCKEY_S *list;
    int       i, j=0, count;
    
    count = 2;	/* for blank key and for OTHER key */
    for(i=0; list1 && list1[i].ch != -1; i++)
      count++;
    for(i=0; list2 && list2[i].ch != -1; i++)
      count++;
    
    list = (ESCKEY_S *) fs_get((count + 1) * sizeof(*list));
    memset(list, 0, (count + 1) * sizeof(*list));

    list[j].ch    = -2;			/* leave blank */
    list[j].rval  = 0;
    list[j].name  = NULL;
    list[j++].label = NULL;

    list[j].ch    = 'o';
    list[j].rval  = OTHER_RETURN_VAL;
    list[j].name  = "O";
    list[j].label = "OTHER CMDS";

    /*
     * Make sure that O for OTHER CMD or the return val for OTHER CMD
     * isn't used for something else.
     */
    for(i=0; list1 && list1[i].ch != -1; i++){
	if(list1[i].rval == list[j].rval)
	  panic("1bad rval in d_r");
	if(F_OFF(F_USE_FK,ps_global) && list1[i].ch == list[j].ch)
	  panic("1bad ch in ccl");
    }

    for(i=0; list2 && list2[i].ch != -1; i++){
	if(list2[i].rval == list[j].rval)
	  panic("2bad rval in d_r");
	if(F_OFF(F_USE_FK,ps_global) && list2[i].ch == list[j].ch)
	  panic("2bad ch in ccl");
    }

    j++;

    /* the visible set */
    for(i=0; list1 && list1[i].ch != -1; i++){
	if(i >= KEYS_PER_LIST && list1[i].label[0] != '\0')
	  panic("too many visible keys in ccl");

	list[j++] = list1[i];
    }

    /* the rest are invisible */
    for(i=0; list2 && list2[i].ch != -1; i++){
	list[j] = list2[i];
	list[j].label = "";
	list[j++].name  = "";
    }

    list[j].ch = -1;

    return(list);
}


/*----------------------------------------------------------------------

  ----*/
void
radio_help(line, column, help)
     int line, column;
     HelpType help;
{
    char **text;

#if defined(HELPFILE)
    text = get_help_text(help);
#else
    text = help;
#endif
    if(text == NULL)
      return;
    
    MoveCursor(line + 1, column);
    CleartoEOLN();
    if(text[0])
      PutLine0(line + 1, column, text[0]);

    MoveCursor(line + 2, column);
    CleartoEOLN();
    if(text[1])
      PutLine0(line + 2, column, text[1]);

#if defined(HELPFILE)
    free_list_array(&text);
#endif
    fflush(stdout);
}


/*----------------------------------------------------------------------
   Paint the screen with the radio buttons prompt
  ----*/
void
draw_radio_prompt(line, start_c, max_c, q)
    int       line, start_c, max_c;
    char     *q;
{
    int x, len, save = -1;

    len = (int)strlen(q);
    if(len > max_c - start_c + 1){
	save = q[max_c - start_c + 1];
	q[max_c - start_c + 1] = '\0';
	len = max_c - start_c + 1;
    }

    PutLine0(line, start_c, q);
    x = start_c + len;
    MoveCursor(line, x);
    while(x++ < ps_global->ttyo->screen_cols)
      Writechar(' ', 0);

    MoveCursor(line, start_c + len);
    fflush(stdout);
    if(save > 0)
      q[max_c - start_c + 1] = save;
}
