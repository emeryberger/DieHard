#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailcmd.c 14081 2005-09-12 22:04:25Z hubert@u.washington.edu $";
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
     mailcmd.c
     The meat and pototoes of mail processing here:
       - initial command processing and dispatch
       - save message
       - capture address off incoming mail
       - jump to specific numbered message
       - open (broach) a new folder
       - search message headers (where is) command
  ====*/

#include "headers.h"
#include "../c-client/imap4r1.h"
#include "../c-client/utf8.h"


/*
 * Internal Prototypes
 */
void      cmd_delete PROTO((struct pine *, MSGNO_S *, int, CmdWhere));
void      cmd_undelete PROTO((struct pine *, MSGNO_S *, int));
void      cmd_reply PROTO((struct pine *, MSGNO_S *, int));
void      cmd_forward PROTO((struct pine *, MSGNO_S *, int));
void      cmd_bounce PROTO((struct pine *, MSGNO_S *, int));
void      cmd_print PROTO((struct pine *, MSGNO_S *, int, CmdWhere));
void      cmd_save PROTO((struct pine *, MAILSTREAM *, MSGNO_S *, int,
			  CmdWhere));
void      cmd_export PROTO((struct pine *, MSGNO_S *, int, int));
void      cmd_pipe PROTO((struct pine *, MSGNO_S *, int));
STORE_S	 *list_mgmt_text PROTO((RFC2369_S *, long));
void	  list_mgmt_screen PROTO((STORE_S *));
void      cmd_flag PROTO((struct pine *, MSGNO_S *, int));
int	  cmd_flag_prompt PROTO((struct pine *, struct flag_screen *, int));
void      role_compose PROTO((struct pine *));
void      free_flag_table PROTO((struct flag_table **));
long	  save PROTO((struct pine *, MAILSTREAM *,
		      CONTEXT_S *, char *, MSGNO_S *, int));
long	  save_fetch_append_cb PROTO((MAILSTREAM *, void *, char **, char **,
				      STRING **));

void      get_save_fldr_from_env PROTO((char *, int, ENVELOPE *,
					 struct pine *, long, char *));
void	  saved_date PROTO((char *, char *));
int	  save_ex_output_body PROTO((MAILSTREAM *, long, char *, BODY *,
				     unsigned long *, gf_io_t));
int	  save_ex_replace_body PROTO((char *, unsigned long *,BODY *,gf_io_t));
int	  save_ex_mask_types PROTO((char *, unsigned long *, gf_io_t));
int	  save_ex_explain_body PROTO((BODY *, unsigned long *, gf_io_t));
int	  save_ex_explain_parts PROTO((BODY *, int, unsigned long *, gf_io_t));
int	  save_ex_output_line PROTO((char *, unsigned long *, gf_io_t));
int	  create_for_save PROTO((MAILSTREAM *, CONTEXT_S *, char *));
void      set_keywords_in_msgid_msg PROTO((MAILSTREAM *, MESSAGECACHE *,
					   MAILSTREAM *, char *, long));
long      get_msgno_by_msg_id PROTO((MAILSTREAM *, char *, MSGNO_S *));
int	  select_sort PROTO((struct pine *, int, SortOrder *, int *));
void	  aggregate_select PROTO((struct pine *, MSGNO_S *, int, CmdWhere,int));
int	  select_number PROTO((MAILSTREAM *, MSGNO_S *, SEARCHSET **));
int	  select_thrd_number PROTO((MAILSTREAM *, MSGNO_S *, SEARCHSET **));
void      set_search_bit_for_thread PROTO((MAILSTREAM *, PINETHRD_S *,
					   SEARCHSET **));
int	  select_size PROTO((MAILSTREAM *, SEARCHSET **));
int	  select_date PROTO((MAILSTREAM *, MSGNO_S *, long, SEARCHSET **));
int	  select_text PROTO((MAILSTREAM *, MSGNO_S *, long, SEARCHSET **));
int	  select_flagged PROTO((MAILSTREAM *, SEARCHSET **));
int	  select_by_keyword PROTO((MAILSTREAM *, SEARCHSET **));
int	  select_by_rule PROTO((MAILSTREAM *, SEARCHSET **));
void	  search_headers PROTO((struct pine *, MAILSTREAM *, int, MSGNO_S *));
char	 *currentf_sequence PROTO((MAILSTREAM *, MSGNO_S *, long, long *,
				   int, char **, char **));
char	 *invalid_elt_sequence PROTO((MAILSTREAM *, MSGNO_S *));
char	 *selected_sequence PROTO((MAILSTREAM *, MSGNO_S *, long *, int));
int	  any_messages PROTO((MSGNO_S *, char *, char *));
int	  can_set_flag PROTO((struct pine *, char *, int));
int	  move_filtered_msgs PROTO((MAILSTREAM *, MSGNO_S *, char *, int,
				    char *));
void	  delete_filtered_msgs PROTO((MAILSTREAM *));
char	 *move_read_msgs PROTO((MAILSTREAM *, char *, char *, long));
int	  read_msg_prompt PROTO((long, char *));
char	 *move_read_incoming PROTO((MAILSTREAM *, CONTEXT_S *, char *,
				    char **, char *));
void	  cross_delete_crossposts PROTO((MAILSTREAM *));
void      menu_clear_cmd_binding PROTO((struct key_menu *, int));
int	  update_folder_spec PROTO((char *, char *));
SEARCHSET *visible_searchset PROTO((MAILSTREAM *, MSGNO_S *));
SEARCHSET *limiting_searchset PROTO((MAILSTREAM *, int));
void      set_some_flags PROTO((MAILSTREAM *, MSGNO_S *, long, char **,
				char **, int, char *));
int       some_filter_depends_on_active_state PROTO((void));
void      mail_expunge_prefilter PROTO((MAILSTREAM *, int));
unsigned  reset_startup_rule PROTO((MAILSTREAM *));
long      closest_jump_target PROTO((long, MAILSTREAM *, MSGNO_S *, int,
				     CmdWhere, char *));
long      get_level PROTO((int, int, SCROLL_S *));
int       in_searchset PROTO((SEARCHSET *, unsigned long));
char     *choose_a_rule PROTO((int));
PATGRP_S *nick_to_patgrp PROTO((char *, int));


#define SV_DELETE		0x1
#define SV_FOR_FILT		0x2
#define SV_FIX_DELS		0x4

typedef struct append_package {
  MAILSTREAM *stream;
  char *flags;
  char *date;
  STRING *msg;
  MSGNO_S *msgmap;
  long msgno;
  STORE_S *so;
} APPENDPACKAGE;

/*
 * List of Select options used by apply_* functions...
 */
static char *sel_pmt1 = "ALTER message selection : ";
ESCKEY_S sel_opts1[] = {
    {'a', 'a', "A", "unselect All"},
    {'c', 'c', "C", NULL},
    {'b', 'b', "B", "Broaden selctn"},
    {'n', 'n', "N", "Narrow selctn"},
    {'f', 'f', "F", "Flip selected"},
    {-1, 0, NULL, NULL}
};


char *sel_pmt2 = "SELECT criteria : ";
static ESCKEY_S sel_opts2[] = {
    {'a', 'a', "A", "select All"},
    {'c', 'c', "C", "select Cur"},
    {'n', 'n', "N", "Number"},
    {'d', 'd', "D", "Date"},
    {'t', 't', "T", "Text"},
    {'s', 's', "S", "Status"},
    {'z', 'z', "Z", "siZe"},
    {'k', 'k', "K", "Keyword"},
    {'r', 'r', "R", "Rule"},
    {-1, 0, NULL, NULL}
};


static ESCKEY_S sel_opts3[] = {
    {'d', 'd',  "D", "Del"},
    {'u', 'u',  "U", "Undel"},
    {'r', 'r',  "R", "Reply"},
    {'f', 'f',  "F", "Forward"},
    {'%', '%',  "%", "Print"},
    {'t', 't',  "T", "TakeAddr"},
    {'s', 's',  "S", "Save"},
    {'e', 'e',  "E", "Export"},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    {-1,    0, NULL, NULL}
};

static ESCKEY_S sel_opts4[] = {
    {'a', 'a', "A", "select All"},
    {'c', 'c', "C", "select Curthrd"},
    {'n', 'n', "N", "Number"},
    {'d', 'd', "D", "Date"},
    {'t', 't', "T", "Text"},
    {'s', 's', "S", "Status"},
    {'z', 'z', "Z", "siZe"},
    {'k', 'k', "K", "Keyword"},
    {'r', 'r', "R", "Rule"},
    {-1, 0, NULL, NULL}
};


static ESCKEY_S other_opts[] = {
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    {-1,    0, NULL, NULL}
};


static char *sel_flag = 
    "Select New, Deleted, Answered, or Important messages ? ";
static char *sel_flag_not = 
    "Select NOT New, NOT Deleted, NOT Answered or NOT Important msgs ? ";
static ESCKEY_S sel_flag_opt[] = {
    {'n', 'n', "N", "New"},
    {'*', '*', "*", "Important"},
    {'d', 'd', "D", "Deleted"},
    {'a', 'a', "A", "Answered"},
    {'!', '!', "!", "Not"},
    {-2, 0, NULL, NULL},
    {-2, 0, NULL, NULL},
    {-2, 0, NULL, NULL},
    {'r', 'r', "R", "Recent"},
    {'u', 'u', "U", "Unseen"},
    {-1, 0, NULL, NULL}
};


static ESCKEY_S sel_date_opt[] = {
    {0, 0, NULL, NULL},
    {ctrl('P'), 12, "^P", "Prev Day"},
    {ctrl('N'), 13, "^N", "Next Day"},
    {ctrl('X'), 11, "^X", "Cur Msg"},
    {ctrl('W'), 14, "^W", "Toggle When"},
    {KEY_UP,    12, "", ""},
    {KEY_DOWN,  13, "", ""},
    {-1, 0, NULL, NULL}
};


static char *sel_text =
    "Select based on To, From, Cc, Recip, Partic, Subject fields or All msg text ? ";
static char *sel_not_text =
    "Select based on NOT To, From, Cc, Recip, Partic, Subject or All msg text ? ";
static ESCKEY_S sel_text_opt[] = {
    {'f', 'f', "F", "From"},
    {'s', 's', "S", "Subject"},
    {'t', 't', "T", "To"},
    {'a', 'a', "A", "All Text"},
    {'c', 'c', "C", "Cc"},
    {'!', '!', "!", "Not"},
    {'r', 'r', "R", "Recipient"},
    {'p', 'p', "P", "Participant"},
    {'b', 'b', "B", "Body"},
    {-1, 0, NULL, NULL}
};

static ESCKEY_S choose_action[] = {
    {'c', 'c', "C", "Compose"},
    {'r', 'r', "R", "Reply"},
    {'f', 'f', "F", "Forward"},
    {'b', 'b', "B", "Bounce"},
    {-1, 0, NULL, NULL}
};

static char *select_num =
  "Enter comma-delimited list of numbers (dash between ranges): ";

static char *select_size_larger_msg =
  "Select messages with size larger than: ";

static char *select_size_smaller_msg =
  "Select messages with size smaller than: ";

static char *sel_size_larger  = "Larger";
static char *sel_size_smaller = "Smaller";
static ESCKEY_S sel_size_opt[] = {
    {0, 0, NULL, NULL},
    {ctrl('W'), 14, "^W", NULL},
    {-1, 0, NULL, NULL}
};

static ESCKEY_S sel_key_opt[] = {
    {0, 0, NULL, NULL},
    {ctrl('T'), 14, "^T", "To List"},
    {0, 0, NULL, NULL},
    {'!', '!', "!", "Not"},
    {-1, 0, NULL, NULL}
};

static ESCKEY_S flag_text_opt[] = {
    {'n', 'n', "N", "New"},
    {'*', '*', "*", "Important"},
    {'d', 'd', "D", "Deleted"},
    {'a', 'a', "A", "Answered"},
    {'!', '!', "!", "Not"},
    {ctrl('T'), 10, "^T", "To Flag Details"},
    {-1, 0, NULL, NULL}
};


/*----------------------------------------------------------------------
         The giant switch on the commands for index and viewing

  Input:  command  -- The command char/code
          in_index -- flag indicating command is from index
          orig_command -- The original command typed before pre-processing
  Output: force_mailchk -- Set to tell caller to force call to new_mail().

  Result: Manifold

          Returns 1 if the message number or attachment to show changed 
 ---*/
int
process_cmd(state, stream, msgmap, command, in_index, force_mailchk)
    struct pine *state;
    MAILSTREAM  *stream;
    MSGNO_S     *msgmap;
    int		 command;
    CmdWhere	 in_index;
    int		*force_mailchk;
{
    int           question_line, a_changed, we_cancel, flags = 0, ret;
    long          new_msgno, del_count, old_msgno, i, old_max_msgno;
    long          start;
    char         *newfolder, prompt[MAX_SCREEN_COLS+1];
    CONTEXT_S    *tc;
    COLOR_PAIR   *lastc = NULL;
    MESSAGECACHE *mc;
#if	defined(DOS) && !defined(_WINDOWS)
    extern long coreleft();
#endif

    dprint(4, (debugfile, "\n - process_cmd(cmd=%d) -\n", command));

    question_line         = -FOOTER_ROWS(state);
    state->mangled_screen = 0;
    state->mangled_footer = 0;
    state->mangled_header = 0;
    state->next_screen    = SCREEN_FUN_NULL;
    old_msgno             = mn_get_cur(msgmap);
    a_changed             = FALSE;
    *force_mailchk        = 0;

    switch (command) {
	/*------------- Help --------*/
      case MC_HELP :
	/*
	 * We're not using the h_mail_view portion of this right now because
	 * that call is being handled in scrolltool() before it gets
	 * here.  Leave it in case we change how it works.
	 */
	helper((in_index == MsgIndx)
		 ? h_mail_index
		 : (in_index == View)
		   ? h_mail_view
		   : h_mail_thread_index,
	       (in_index == MsgIndx)
	         ? "HELP FOR MESSAGE INDEX"
		 : (in_index == View)
		   ? "HELP FOR MESSAGE VIEW"
		   : "HELP FOR THREAD INDEX",
	       HLPD_NONE);
	dprint(4, (debugfile,"MAIL_CMD: did help command\n"));
	state->mangled_screen = 1;
	break;


          /*--------- Return to main menu ------------*/
      case MC_MAIN :
	state->next_screen = main_menu_screen;
#if	defined(DOS) && !defined(WIN32)
	flush_index_cache();		/* save room on PC */
#endif
	dprint(2, (debugfile,"MAIL_CMD: going back to main menu\n"));
	break;


          /*------- View message text --------*/
      case MC_VIEW_TEXT :
view_text:
	if(any_messages(msgmap, NULL, "to View")){
	    state->next_screen = mail_view_screen;
#if	defined(DOS) && !defined(WIN32)
	    flush_index_cache();		/* save room on PC */
#endif
	}

	break;


          /*------- View attachment --------*/
      case MC_VIEW_ATCH :
	state->next_screen = attachment_screen;
	dprint(2, (debugfile,"MAIL_CMD: going to attachment screen\n"));
	break;


          /*---------- Previous message ----------*/
      case MC_PREVITEM :
	if(any_messages(msgmap, NULL, NULL)){
	    if((i = mn_get_cur(msgmap)) > 1L){
		mn_dec_cur(stream, msgmap,
			   (in_index == View && THREADING()
			    && sp_viewing_a_thread(stream))
			     ? MH_THISTHD
			     : (in_index == View)
			       ? MH_ANYTHD : MH_NONE);
		if(i == mn_get_cur(msgmap)){
		    PINETHRD_S *thrd;

		    if(THRD_INDX_ENABLED()){
			mn_dec_cur(stream, msgmap, MH_ANYTHD);
			if(i == mn_get_cur(msgmap))
			  q_status_message1(SM_ORDER, 0, 2,
				      "Already on first %.200s in Zoomed Index",
				      THRD_INDX() ? "thread" : "message");
			else{
			    if(in_index == View
			       || F_ON(F_NEXT_THRD_WO_CONFIRM, state))
			      ret = 'y';
			    else
			      ret = want_to("View previous thread", 'y', 'x',
					    NO_HELP, WT_NORM);

			    if(ret == 'y'){
				q_status_message(SM_ORDER, 0, 2,
						 "Viewing previous thread");
				new_msgno = mn_get_cur(msgmap);
				mn_set_cur(msgmap, i);
				unview_thread(state, stream, msgmap);
				mn_set_cur(msgmap, new_msgno);
				if(THRD_AUTO_VIEW() && in_index == View){

				    thrd = fetch_thread(stream,
							mn_m2raw(msgmap,
								 new_msgno));
				    if(count_lflags_in_thread(stream, thrd,
							      msgmap,
							      MN_NONE) == 1){
					if(view_thread(state, stream, msgmap, 1)){
					    state->view_skipped_index = 1;
					    command = MC_VIEW_TEXT;
					    goto view_text;
					}
				    }
				}

				view_thread(state, stream, msgmap, 1);
				state->next_screen = SCREEN_FUN_NULL;
			    }
			    else
			      mn_set_cur(msgmap, i);	/* put it back */
			}
		    }
		    else
		      q_status_message1(SM_ORDER, 0, 2,
				  "Already on first %.200s in Zoomed Index",
				  THRD_INDX() ? "thread" : "message");
		}
	    }
	    else
	      q_status_message1(SM_ORDER, 0, 1, "Already on first %.200s",
				THRD_INDX() ? "thread" : "message");
	}

	break;


          /*---------- Next Message ----------*/
      case MC_NEXTITEM :
	if(mn_get_total(msgmap) > 0L
	   && ((i = mn_get_cur(msgmap)) < mn_get_total(msgmap))){
	    mn_inc_cur(stream, msgmap,
		       (in_index == View && THREADING()
		        && sp_viewing_a_thread(stream))
			 ? MH_THISTHD
			 : (in_index == View)
			   ? MH_ANYTHD : MH_NONE);
	    if(i == mn_get_cur(msgmap)){
		PINETHRD_S *thrd;

		if(THRD_INDX_ENABLED()){
		    if(!THRD_INDX())
		      mn_inc_cur(stream, msgmap, MH_ANYTHD);

		    if(i == mn_get_cur(msgmap)){
			if(any_lflagged(msgmap, MN_HIDE))
			  any_messages(NULL, "more", "in Zoomed Index");
			else
			  goto nfolder;
		    }
		    else{
			if(in_index == View
			   || F_ON(F_NEXT_THRD_WO_CONFIRM, state))
			  ret = 'y';
			else
			  ret = want_to("View next thread", 'y', 'x',
					NO_HELP, WT_NORM);

			if(ret == 'y'){
			    q_status_message(SM_ORDER, 0, 2,
					     "Viewing next thread");
			    new_msgno = mn_get_cur(msgmap);
			    mn_set_cur(msgmap, i);
			    unview_thread(state, stream, msgmap);
			    mn_set_cur(msgmap, new_msgno);
			    if(THRD_AUTO_VIEW() && in_index == View){

				thrd = fetch_thread(stream,
						    mn_m2raw(msgmap,
							     new_msgno));
				if(count_lflags_in_thread(stream, thrd,
							  msgmap,
							  MN_NONE) == 1){
				    if(view_thread(state, stream, msgmap, 1)){
					state->view_skipped_index = 1;
					command = MC_VIEW_TEXT;
					goto view_text;
				    }
				}
			    }

			    view_thread(state, stream, msgmap, 1);
			    state->next_screen = SCREEN_FUN_NULL;
			}
			else
			  mn_set_cur(msgmap, i);	/* put it back */
		    }
		}
		else if(THREADING()
			&& (thrd = fetch_thread(stream, mn_m2raw(msgmap, i)))
			&& thrd->next
			&& get_lflag(stream, NULL, thrd->rawno, MN_COLL)){
		       q_status_message(SM_ORDER, 0, 2,
			       "Expand collapsed thread to see more messages");
		}
		else
		  any_messages(NULL, "more", "in Zoomed Index");
	    }
	}
	else{
	    time_t now;
nfolder:
	    prompt[0] = '\0';
	    if(IS_NEWS(stream)
	       || (state->context_current->use & CNTXT_INCMNG)){
		char nextfolder[MAXPATH];

		strncpy(nextfolder, state->cur_folder, sizeof(nextfolder));
		nextfolder[sizeof(nextfolder)-1] = '\0';
		if(next_folder(NULL, nextfolder, nextfolder,
			       state->context_current, NULL, NULL))
		  strncpy(prompt, ".  Press TAB for next folder.",
			  sizeof(prompt));
		else
		  strncpy(prompt, ".  No more folders to TAB to.",
			  sizeof(prompt));
	    }

	    any_messages(NULL, (mn_get_total(msgmap) > 0L) ? "more" : NULL,
			 prompt[0] ? prompt : NULL);

	    if(!IS_NEWS(stream)
	       && ((now = time(0)) > state->last_nextitem_forcechk)){
		*force_mailchk = 1;
		/* check at most once a second */
		state->last_nextitem_forcechk = now;
	    }
	}

	break;


          /*---------- Delete message ----------*/
      case MC_DELETE :
	cmd_delete(state, msgmap, 0, in_index);
	break;
          

          /*---------- Undelete message ----------*/
      case MC_UNDELETE :
	cmd_undelete(state, msgmap, 0);
	break;


          /*---------- Reply to message ----------*/
      case MC_REPLY :
	cmd_reply(state, msgmap, 0);
	break;


          /*---------- Forward message ----------*/
      case MC_FORWARD :
	cmd_forward(state, msgmap, 0);
	break;


          /*---------- Quit pine ------------*/
      case MC_QUIT :
	state->next_screen = quit_screen;
	dprint(1, (debugfile,"MAIL_CMD: quit\n"));		    
	break;


          /*---------- Compose message ----------*/
      case MC_COMPOSE :
	state->prev_screen = (in_index == View) ? mail_view_screen
						: mail_index_screen;
#if	defined(DOS) && !defined(WIN32)
	flush_index_cache();		/* save room on PC */
#endif
	compose_screen(state);
	state->mangled_screen = 1;
	if (state->next_screen)
	  a_changed = TRUE;
	break;


          /*---------- Alt Compose message ----------*/
      case MC_ROLE :
	state->prev_screen = (in_index == View) ? mail_view_screen
						: mail_index_screen;
	role_compose(state);
	if(state->next_screen)
	  a_changed = TRUE;

	break;


          /*--------- Folders menu ------------*/
      case MC_FOLDERS :
	state->start_in_context = 1;

          /*--------- Top of Folders list menu ------------*/
      case MC_COLLECTIONS :
	state->next_screen = folder_screen;
#if	defined(DOS) && !defined(WIN32)
	flush_index_cache();		/* save room on PC */
#endif
	dprint(2, (debugfile,"MAIL_CMD: going to folder/collection menu\n"));
	break;


          /*---------- Open specific new folder ----------*/
      case MC_GOTO :
	tc = (state->context_last && !NEWS_TEST(state->context_current)) 
	       ? state->context_last : state->context_current;

	newfolder = broach_folder(question_line, 1, &tc);
#if	defined(DOS) && !defined(_WINDOWS)
	if(newfolder && *newfolder == '{' && coreleft() < 20000){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Not enough memory to open IMAP folder");
	    newfolder = NULL;
	}
#endif
	if(newfolder){
	    visit_folder(state, newfolder, tc, NULL, 0L);
	    a_changed = TRUE;
	}

	break;
    	  
    	    
          /*------- Go to Index Screen ----------*/
      case MC_INDEX :
#if	defined(DOS) && !defined(WIN32)
	flush_index_cache();		/* save room on PC */
#endif
	state->next_screen = mail_index_screen;
	break;

          /*------- Skip to next interesting message -----------*/
      case MC_TAB :
	if(THRD_INDX()){
	    PINETHRD_S *thrd;

	    /*
	     * If we're in the thread index, start looking after this
	     * thread. We don't want to match something in the current
	     * thread.
	     */
	    start = mn_get_cur(msgmap);
	    thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
	    if(mn_get_revsort(msgmap)){
		/* if reversed, top of thread is last one before next thread */
		if(thrd && thrd->top)
		  start = mn_raw2m(msgmap, thrd->top);
	    }
	    else{
		/* last msg of thread is at the ends of the branches/nexts */
		while(thrd){
		    start = mn_raw2m(msgmap, thrd->rawno);
		    if(thrd->branch)
		      thrd = fetch_thread(stream, thrd->branch);
		    else if(thrd->next)
		      thrd = fetch_thread(stream, thrd->next);
		    else
		      thrd = NULL;
		}
	    }

	    /*
	     * Flags is 0 in this case because we want to not skip
	     * messages inside of threads so that we can find threads
	     * which have some unseen messages even though the top-level
	     * of the thread is already seen.
	     * If new_msgno ends up being a message which is not visible
	     * because it isn't at the top-level, the current message #
	     * will be adjusted below in adjust_cur.
	     */
	    flags = 0;
	    new_msgno = next_sorted_flagged((F_UNDEL 
					     | F_UNSEEN
					     | ((F_ON(F_TAB_TO_NEW,state))
						 ? 0 : F_OR_FLAG)),
					    stream, start, &flags);
	}
	else if(THREADING() && sp_viewing_a_thread(stream)){
	    PINETHRD_S *thrd, *topthrd = NULL;

	    start = mn_get_cur(msgmap);

	    /*
	     * Things are especially complicated when we're viewing_a_thread
	     * from the thread index. First we have to check within the
	     * current thread for a new message. If none is found, then
	     * we search in the next threads and offer to continue in
	     * them. Then we offer to go to the next folder.
	     */
	    flags = NSF_SKIP_CHID;
	    new_msgno = next_sorted_flagged((F_UNDEL 
					     | F_UNSEEN
					     | ((F_ON(F_TAB_TO_NEW,state))
					       ? 0 : F_OR_FLAG)),
					    stream, start, &flags);
	    /*
	     * If we found a match then we are done, that is another message
	     * in the current thread index. Otherwise, we have to look
	     * further.
	     */
	    if(!(flags & NSF_FLAG_MATCH)){
		ret = 'n';
		while(1){

		    flags = 0;
		    new_msgno = next_sorted_flagged((F_UNDEL 
						     | F_UNSEEN
						     | ((F_ON(F_TAB_TO_NEW,
							      state))
							 ? 0 : F_OR_FLAG)),
						    stream, start, &flags);
		    /*
		     * If we got a match, new_msgno is a message in
		     * a different thread from the one we are viewing.
		     */
		    if(flags & NSF_FLAG_MATCH){
			thrd = fetch_thread(stream, mn_m2raw(msgmap,new_msgno));
			if(thrd && thrd->top)
			  topthrd = fetch_thread(stream, thrd->top);

			if(F_OFF(F_AUTO_OPEN_NEXT_UNREAD, state)){
			    static ESCKEY_S next_opt[] = {
				{'y', 'y', "Y", "Yes"},
				{'n', 'n', "N", "No"},
				{TAB, 'n', "Tab", "NextNew"},
				{-1, 0, NULL, NULL}
			    };

			    if(in_index)
			      sprintf(prompt, "View thread number %.10s? ",
				     topthrd ? comatose(topthrd->thrdno) : "?");
			    else
			      sprintf(prompt,
				     "View message in thread number %.10s? ",
				     topthrd ? comatose(topthrd->thrdno) : "?");
				    
			    ret = radio_buttons(prompt, -FOOTER_ROWS(state),
						next_opt, 'y', 'x', NO_HELP,
						RB_NORM, NULL);
			    if(ret == 'x'){
				cmd_cancelled(NULL);
				goto get_out;
			    }
			}
			else
			  ret = 'y';

			if(ret == 'y'){
			    unview_thread(state, stream, msgmap);
			    mn_set_cur(msgmap, new_msgno);
			    if(THRD_AUTO_VIEW()){

				if(count_lflags_in_thread(stream, topthrd,
				                         msgmap, MN_NONE) == 1){
				    if(view_thread(state, stream, msgmap, 1)){
					state->view_skipped_index = 1;
					command = MC_VIEW_TEXT;
					goto view_text;
				    }
				}
			    }

			    view_thread(state, stream, msgmap, 1);
			    state->next_screen = SCREEN_FUN_NULL;
			    break;
			}
			else if(ret == 'n' && topthrd){
			    /*
			     * skip to end of this thread and look starting
			     * in the next thread.
			     */
			    if(mn_get_revsort(msgmap)){
				/*
				 * if reversed, top of thread is last one
				 * before next thread
				 */
				start = mn_raw2m(msgmap, topthrd->rawno);
			    }
			    else{
				/*
				 * last msg of thread is at the ends of
				 * the branches/nexts
				 */
				thrd = topthrd;
				while(thrd){
				    start = mn_raw2m(msgmap, thrd->rawno);
				    if(thrd->branch)
				      thrd = fetch_thread(stream, thrd->branch);
				    else if(thrd->next)
				      thrd = fetch_thread(stream, thrd->next);
				    else
				      thrd = NULL;
				}
			    }
			}
			else if(ret == 'n')
			  break;
		    }
		    else
		      break;
		}
	    }
	}
	else{

	    start = mn_get_cur(msgmap);

	    if(THREADING()){
		PINETHRD_S *thrd;
		long        rawno;
		int         collapsed;

		/*
		 * If we are on a collapsed thread, start looking after the
		 * collapsed part.
		 */
		rawno = mn_m2raw(msgmap, start);
		thrd = fetch_thread(stream, rawno);
		collapsed = thrd && thrd->next
			    && get_lflag(stream, NULL, rawno, MN_COLL);

		if(collapsed){
		    if(mn_get_revsort(msgmap)){
			if(thrd && thrd->top)
			  start = mn_raw2m(msgmap, thrd->top);
		    }
		    else{
			while(thrd){
			    start = mn_raw2m(msgmap, thrd->rawno);
			    if(thrd->branch)
			      thrd = fetch_thread(stream, thrd->branch);
			    else if(thrd->next)
			      thrd = fetch_thread(stream, thrd->next);
			    else
			      thrd = NULL;
			}
		    }

		}
	    }

	    new_msgno = next_sorted_flagged((F_UNDEL 
					     | F_UNSEEN
					     | ((F_ON(F_TAB_TO_NEW,state))
						 ? 0 : F_OR_FLAG)),
					    stream, start, &flags);
	}

	/*
	 * If there weren't any unread messages left, OR there
	 * aren't any messages at all, we may want to offer to
	 * go on to the next folder...
	 */
	if(flags & NSF_FLAG_MATCH){
	    mn_set_cur(msgmap, new_msgno);
	    if(in_index != View)
	      adjust_cur_to_visible(stream, msgmap);
	}
	else{
	    int in_inbox = sp_flagged(stream, SP_INBOX);

	    if(state->context_current
	       && ((NEWS_TEST(state->context_current)
		    && context_isambig(state->cur_folder))
		   || ((state->context_current->use & CNTXT_INCMNG)
		       && (in_inbox
			   || folder_index(state->cur_folder,
					   state->context_current,
					   FI_FOLDER) >= 0)))){
		char	    nextfolder[MAXPATH];
		MAILSTREAM *nextstream = NULL;
		long	    recent_cnt;
		int         did_cancel = 0;

		strncpy(nextfolder, state->cur_folder, sizeof(nextfolder));
		nextfolder[sizeof(nextfolder)-1] = '\0';
		while(1){
		    if(!(next_folder(&nextstream, nextfolder, nextfolder,
				     state->context_current, &recent_cnt,
				     F_ON(F_TAB_NO_CONFIRM,state)
				       ? NULL : &did_cancel))){
			if(!in_inbox){
			    static ESCKEY_S inbox_opt[] = {
				{'y', 'y', "Y", "Yes"},
				{'n', 'n', "N", "No"},
				{TAB, 'z', "Tab", "To Inbox"},
				{-1, 0, NULL, NULL}
			    };

			    if(F_ON(F_RET_INBOX_NO_CONFIRM,state))
			      ret = 'y';
			    else{
				sprintf(prompt,
					"No more %ss.  Return to \"%.*s\"? ",
					(state->context_current->use&CNTXT_INCMNG)
					  ? "incoming folder" : "news group", 
					sizeof(prompt)-40, state->inbox_name);

				ret = radio_buttons(prompt, -FOOTER_ROWS(state),
						    inbox_opt, 'y', 'x',
						    NO_HELP, RB_NORM, NULL);
			    }

			    /*
			     * 'z' is a synonym for 'y'.  It is not 'y'
			     * so that it isn't displayed as a default
			     * action with square-brackets around it
			     * in the keymenu...
			     */
			    if(ret == 'y' || ret == 'z'){
				visit_folder(state, state->inbox_name,
					     state->context_current,
					     NULL, 0L);
				a_changed = TRUE;
			    }
			}
			else if (did_cancel)
			  cmd_cancelled(NULL);			
			else
			  q_status_message1(SM_ORDER, 0, 2, "No more %.200ss",
				     (state->context_current->use&CNTXT_INCMNG)
				        ? "incoming folder" : "news group");

			break;
		    }

		    {char *front, type[80], cnt[80], fbuf[MAX_SCREEN_COLS/2+1];
		     int rbspace, avail, need, take_back;

			/*
			 * View_next_
			 * Incoming_folder_ or news_group_ or folder_ or group_
			 * "foldername"
			 * _(13 recent) or _(some recent) or nothing
			 * ?_
			 */
			front = "View next";
			strncpy(type,
				(state->context_current->use & CNTXT_INCMNG)
				    ? "Incoming folder" : "news group",
				sizeof(type));
			sprintf(cnt, " (%.*s %.6s)", sizeof(cnt)-20,
				recent_cnt ? long2string(recent_cnt) : "some",
				F_ON(F_TAB_USES_UNSEEN, ps_global)
				    ? "unseen" : "recent");

			/*
			 * Space reserved for radio_buttons call.
			 * If we make this 3 then radio_buttons won't mess
			 * with the prompt. If we make it 2, then we get
			 * one more character to use but radio_buttons will
			 * cut off the last character of our prompt, which is
			 * ok because it is a space.
			 */
			rbspace = 2;
			avail = ps_global->ttyo ? ps_global->ttyo->screen_cols
						: 80;
			need = strlen(front)+1 + strlen(type)+1 +
			       + strlen(nextfolder)+2 + strlen(cnt) +
			       2 + rbspace;
			if(avail < need){
			    take_back = strlen(type);
			    strncpy(type,
				    (state->context_current->use & CNTXT_INCMNG)
					? "folder" : "group", sizeof(type));
			    take_back -= strlen(type);
			    need -= take_back;
			    if(avail < need){
				need -= strlen(cnt);
				cnt[0] = '\0';
			    }
			}

			sprintf(prompt, "%.*s %.*s \"%.*s\"%.*s? ",
				sizeof(prompt)/8, front,
				sizeof(prompt)/8, type,
				sizeof(prompt)/2,
				short_str(nextfolder, fbuf,
					  strlen(nextfolder) -
					    ((need>avail) ? (need-avail) : 0),
					  MidDots),
				sizeof(prompt)/8, cnt);
		    }

		    /*
		     * When help gets added, this'll have to become
		     * a loop like the rest...
		     */
		    if(F_OFF(F_AUTO_OPEN_NEXT_UNREAD, state)){
			static ESCKEY_S next_opt[] = {
			    {'y', 'y', "Y", "Yes"},
			    {'n', 'n', "N", "No"},
			    {TAB, 'n', "Tab", "NextNew"},
			    {-1, 0, NULL, NULL}
			};

			ret = radio_buttons(prompt, -FOOTER_ROWS(state),
					    next_opt, 'y', 'x', NO_HELP,
					    RB_NORM, NULL);
			if(ret == 'x'){
			    cmd_cancelled(NULL);
			    break;
			}
		    }
		    else
		      ret = 'y';

		    if(ret == 'y'){
			visit_folder(state, nextfolder,
				     state->context_current, nextstream,
				     DB_FROMTAB);
			/* visit_folder takes care of nextstream */
			nextstream = NULL;
			a_changed = TRUE;
			break;
		    }
		}

		if(nextstream)
		  pine_mail_close(nextstream);
	    }
	    else
	      any_messages(NULL,
			   (mn_get_total(msgmap) > 0L)
			     ? IS_NEWS(stream) ? "more undeleted" : "more new"
			     : NULL,
			   NULL);
	}

get_out:

	break;


          /*------- Zoom -----------*/
      case MC_ZOOM :
	/*
	 * Right now the way zoom is implemented is sort of silly.
	 * There are two per-message flags where just one and a 
	 * global "zoom mode" flag to suppress messags from the index
	 * should suffice.
	 */
	if(any_messages(msgmap, NULL, "to Zoom on")){
	    if(unzoom_index(state, stream, msgmap)){
		dprint(4, (debugfile, "\n\n ---- Exiting ZOOM mode ----\n"));
		q_status_message(SM_ORDER,0,2, "Index Zoom Mode is now off");
	    }
	    else if(i = zoom_index(state, stream, msgmap)){
		if(any_lflagged(msgmap, MN_HIDE)){
		    dprint(4,(debugfile,"\n\n ---- Entering ZOOM mode ----\n"));
		    q_status_message4(SM_ORDER, 0, 2,
				      "In Zoomed Index of %.200s%.200s%.200s%.200s.  Use \"Z\" to restore regular Index",
				      THRD_INDX() ? "" : comatose(i),
				      THRD_INDX() ? "" : " ",
				      THRD_INDX() ? "threads" : "message",
				      THRD_INDX() ? "" : plural(i));
		}
		else
		  q_status_message(SM_ORDER, 0, 2,
		     "All messages selected, so not entering Index Zoom Mode");
	    }
	    else
	      any_messages(NULL, "selected", "to Zoom on");
	}

	break;


          /*---------- print message on paper ----------*/
      case MC_PRINTMSG :
	if(any_messages(msgmap, NULL, "to print"))
	  cmd_print(state, msgmap, 0, in_index);

	break;


          /*---------- Take Address ----------*/
      case MC_TAKE :
	if(F_ON(F_ENABLE_ROLE_TAKE, state) ||
	   any_messages(msgmap, NULL, "to Take address from"))
	  cmd_take_addr(state, msgmap, 0);

	break;


          /*---------- Save Message ----------*/
      case MC_SAVE :
	if(any_messages(msgmap, NULL, "to Save"))
	  cmd_save(state, stream, msgmap, 0, in_index);

	break;


          /*---------- Export message ----------*/
      case MC_EXPORT :
	if(any_messages(msgmap, NULL, "to Export")){
	    cmd_export(state, msgmap, question_line, 0);
	    state->mangled_footer = 1;
	}

	break;


          /*---------- Expunge ----------*/
      case MC_EXPUNGE :
	dprint(2, (debugfile, "\n - expunge -\n"));
	if(IS_NEWS(stream) && stream->rdonly){
	    if((del_count = count_flagged(stream, F_DEL)) > 0L){
		state->mangled_footer = 1;
		sprintf(prompt, "Exclude %ld message%s from %.*s", del_count,
			plural(del_count), sizeof(prompt)-40,
			pretty_fn(state->cur_folder));
		if(F_ON(F_FULL_AUTO_EXPUNGE, state)
		   || (F_ON(F_AUTO_EXPUNGE, state)
		       && (state->context_current
		           && (state->context_current->use & CNTXT_INCMNG))
		       && context_isambig(state->cur_folder))
		   || want_to(prompt, 'y', 0, NO_HELP, WT_NORM) == 'y'){
		    if(F_ON(F_NEWS_CROSS_DELETE, ps_global))
		      cross_delete_crossposts(stream);
		    msgno_exclude_deleted(stream, msgmap);
		    clear_index_cache();

		    /*
		     * This is kind of surprising at first. For most sort
		     * orders, if the whole set is sorted, then any subset
		     * is also sorted. Not so for threaded sorts.
		     */
		    if(SORT_IS_THREADED(msgmap))
		      refresh_sort(stream, msgmap, SRT_NON);

		    state->mangled_body = 1;
		    state->mangled_header = 1;
		    q_status_message2(SM_ORDER, 0, 4,
				      "%.200s message%.200s excluded",
				      long2string(del_count),
				      plural(del_count));
		}
		else
		  any_messages(NULL, NULL, "Excluded");
	    }
	    else
	      any_messages(NULL, "deleted", "to Exclude");

	    break;
	}
	else if(READONLY_FOLDER(stream)){
	    q_status_message(SM_ORDER, 0, 4,
			     "Can't expunge. Folder is read-only");
	    break;
	}

	mail_expunge_prefilter(stream, MI_NONE);

	if(del_count = count_flagged(stream, F_DEL)){
	    int ret;

	    sprintf(prompt, "Expunge %ld message%s from %.*s", del_count,
		    plural(del_count), sizeof(prompt)-40,
		    pretty_fn(state->cur_folder));
	    state->mangled_footer = 1;
	    if(F_ON(F_FULL_AUTO_EXPUNGE, state)
	       || (F_ON(F_AUTO_EXPUNGE, state)
		   && ((!strucmp(state->cur_folder,state->inbox_name))
		       || (state->context_current->use & CNTXT_INCMNG))
		   && context_isambig(state->cur_folder))
	       || (ret=want_to(prompt, 'y', 0, NO_HELP, WT_NORM)) == 'y')
	      ret = 'y';

	    if(ret == 'x')
	      cmd_cancelled("Expunge");

	    if(ret != 'y')
	      break;
	}

	dprint(8,(debugfile, "Expunge max:%ld cur:%ld kill:%d\n",
		  mn_get_total(msgmap), mn_get_cur(msgmap), del_count));

	old_max_msgno = mn_get_total(msgmap);
	lastc = pico_set_colors(ps_global->VAR_TITLE_FORE_COLOR,
				ps_global->VAR_TITLE_BACK_COLOR,
				PSC_REV|PSC_RET);

	PutLine0(0, 0, "**");			/* indicate delay */

	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}

	MoveCursor(state->ttyo->screen_rows -FOOTER_ROWS(state), 0);
	fflush(stdout);

	we_cancel = busy_alarm(1, "Expunging", NULL, 0);
	delete_filtered_msgs(stream);
	ps_global->expunge_in_progress = 1;
	mail_expunge(stream);
	ps_global->expunge_in_progress = 0;
	if(we_cancel)
	  cancel_busy_alarm((sp_expunge_count(stream) > 0) ? 0 : -1);

	dprint(2,(debugfile,"expunge complete cur:%ld max:%ld\n",
		  mn_get_cur(msgmap), mn_get_total(msgmap)));
	/*
	 * This is only actually necessary if this causes the width of the
	 * message number field to change.  That is, it depends on the
	 * format the user is using as well as on the max_msgno.  Since it
	 * should be rare, we'll just do it whenever it happens.
	 * Also have to check for an increase in max_msgno on new mail.
	 */
	if(old_max_msgno >= 1000L && mn_get_total(msgmap) < 1000L
	   || old_max_msgno >= 10000L && mn_get_total(msgmap) < 10000L
	   || old_max_msgno >= 100000L && mn_get_total(msgmap) < 100000L){
	    clear_index_cache();
	    state->mangled_body = 1;
	}

	/*
	 * mm_exists and mm_expunged take care of updating max_msgno,
	 * selecting a new message should the selected get removed,
	 * and resetting the checkpoint counter.
	 */
	lastc = pico_set_colors(ps_global->VAR_TITLE_FORE_COLOR,
				ps_global->VAR_TITLE_BACK_COLOR,
				PSC_REV|PSC_RET);
	PutLine0(0, 0, "  ");			/* indicate delay's over */

	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}

	fflush(stdout);

	if(sp_expunge_count(stream) > 0){
	    /*
	     * This is kind of surprising at first. For most sort
	     * orders, if the whole set is sorted, then any subset
	     * is also sorted. Not so for threaded sorts.
	     */
	    if(SORT_IS_THREADED(msgmap))
	      refresh_sort(stream, msgmap, SRT_NON);
	}
	else{
	    if(del_count)
	      q_status_message1(SM_ORDER, 0, 3,
			        "No messages expunged from folder \"%.200s\"",
			        pretty_fn(state->cur_folder));
	    else
	      q_status_message(SM_ORDER, 0, 3,
			 "No messages marked deleted.  No messages expunged.");
	}

	break;


          /*------- Unexclude -----------*/
      case MC_UNEXCLUDE :
	if(!(IS_NEWS(stream) && stream->rdonly)){
	    q_status_message(SM_ORDER, 0, 3,
			     "Unexclude not available for mail folders");
	}
	else if(any_lflagged(msgmap, MN_EXLD)){
	    SEARCHPGM *pgm;
	    long       i;
	    int	       exbits;

	    /*
	     * Since excluded means "hidden deleted" and "killed",
	     * the count should reflect the former.
	     */
	    pgm = mail_newsearchpgm();
	    pgm->deleted = 1;
	    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);
	    for(i = 1L, del_count = 0L; i <= stream->nmsgs; i++)
	      if((mc = mail_elt(stream, i)) && mc->searched
		 && get_lflag(stream, NULL, i, MN_EXLD)
		 && !(msgno_exceptions(stream, i, "0", &exbits, FALSE)
		      && (exbits & MSG_EX_FILTERED)))
		del_count++;

	    if(del_count > 0L){
		state->mangled_footer = 1;
		sprintf(prompt, "UNexclude %ld message%s in %.*s", del_count,
			plural(del_count), sizeof(prompt)-40,
			pretty_fn(state->cur_folder));
		if(F_ON(F_FULL_AUTO_EXPUNGE, state)
		   || (F_ON(F_AUTO_EXPUNGE, state)
		       && (state->context_current
			   && (state->context_current->use & CNTXT_INCMNG))
		       && context_isambig(state->cur_folder))
		   || want_to(prompt, 'y', 0, NO_HELP, WT_NORM) == 'y'){
		    long save_cur_rawno;
		    int  were_viewing_a_thread;

		    save_cur_rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
		    were_viewing_a_thread = (THREADING()
					     && sp_viewing_a_thread(stream));

		    if(msgno_include(stream, msgmap, MI_NONE)){
			clear_index_cache();

			if(stream && stream->spare)
			  erase_threading_info(stream, msgmap);

			refresh_sort(stream, msgmap, SRT_NON);
		    }

		    if(were_viewing_a_thread){
			if(save_cur_rawno > 0L)
			  mn_set_cur(msgmap, mn_raw2m(msgmap,save_cur_rawno));

			view_thread(state, stream, msgmap, 1);
		    }

		    if(save_cur_rawno > 0L)
		      mn_set_cur(msgmap, mn_raw2m(msgmap,save_cur_rawno));

		    state->mangled_screen = 1;
		    q_status_message2(SM_ORDER, 0, 4,
				      "%.200s message%.200s UNexcluded",
				      long2string(del_count),
				      plural(del_count));

		    if(in_index != View)
		      adjust_cur_to_visible(stream, msgmap);
		}
		else
		  any_messages(NULL, NULL, "UNexcluded");
	    }
	    else
	      any_messages(NULL, "excluded", "to UNexclude");
	}
	else
	  any_messages(NULL, "excluded", "to UNexclude");

	break;


          /*------- Make Selection -----------*/
      case MC_SELECT :
	if(any_messages(msgmap, NULL, "to Select")){
	    aggregate_select(state, msgmap, question_line, in_index,
			     THRD_INDX());
	    if((in_index == MsgIndx || in_index == ThrdIndx)
	       && any_lflagged(msgmap, MN_SLCT) > 0L
	       && !any_lflagged(msgmap, MN_HIDE)
	       && F_ON(F_AUTO_ZOOM, state))
	      (void) zoom_index(state, stream, msgmap);
	}

	break;


          /*------- Toggle Current Message Selection State -----------*/
      case MC_SELCUR :
	if(any_messages(msgmap, NULL, NULL)
	   && (individual_select(state, msgmap, in_index)
	       || (F_OFF(F_UNSELECT_WONT_ADVANCE, state)
	           && !any_lflagged(msgmap, MN_HIDE)))
	   && (i = mn_get_cur(msgmap)) < mn_get_total(msgmap)){
	    /* advance current */
	    mn_inc_cur(stream, msgmap,
		       (in_index == View && THREADING()
		        && sp_viewing_a_thread(stream))
			 ? MH_THISTHD
			 : (in_index == View)
			   ? MH_ANYTHD : MH_NONE);
	}

	break;


          /*------- Apply command -----------*/
      case MC_APPLY :
	if(any_messages(msgmap, NULL, NULL)){
	    if(any_lflagged(msgmap, MN_SLCT) > 0L){
		if(apply_command(state, stream, msgmap, 0,
				 AC_NONE, question_line)
		   && F_ON(F_AUTO_UNZOOM, state))
		  unzoom_index(state, stream, msgmap);
	    }
	    else
	      any_messages(NULL, NULL, "to Apply command to.  Try \"Select\"");
	}

	break;


          /*-------- Sort command -------*/
      case MC_SORT :
	{
	    int were_threading = THREADING();
	    SortOrder sort = mn_get_sort(msgmap);
	    int	      rev  = mn_get_revsort(msgmap);

	    dprint(1, (debugfile,"MAIL_CMD: sort\n"));		    
	    if(select_sort(state, question_line, &sort, &rev)){
		/* $ command reinitializes threading collapsed/expanded info */
		if(SORT_IS_THREADED(msgmap) && !SEP_THRDINDX())
		  erase_threading_info(stream, msgmap);

		sort_folder(stream, msgmap, sort, rev, SRT_VRB|SRT_MAN);
	    }

	    state->mangled_footer = 1;

	    /*
	     * We've changed whether we are threading or not so we need to
	     * exit the index and come back in so that we switch between the
	     * thread index and the regular index. Sort_folder will have
	     * reset viewing_a_thread if necessary.
	     */
	    if(SEP_THRDINDX()
	       && ((!were_threading && THREADING())
	            || (were_threading && !THREADING()))){
		state->next_screen = mail_index_screen;
		state->mangled_screen = 1;
	    }
	}

	break;


          /*------- Toggle Full Headers -----------*/
      case MC_FULLHDR :
	state->full_header++;
	if(state->full_header == 1){
	    if(!(state->quote_suppression_threshold
	         && (state->some_quoting_was_suppressed || in_index != View)))
	      state->full_header++;
	}
	else if(state->full_header > 2)
	  state->full_header = 0;

	switch(state->full_header){
	  case 0:
	    q_status_message(SM_ORDER, 0, 3,
			     "Display of full headers is now off.");
	    break;

	  case 1:
	    q_status_message1(SM_ORDER, 0, 3,
			  "Quotes displayed, use %.200s to see full headers",
			  F_ON(F_USE_FK, state) ? "F9" : "H");
	    break;

	  case 2:
	    q_status_message(SM_ORDER, 0, 3,
			     "Display of full headers is now on.");
	    break;

	}

	a_changed = TRUE;
	break;


          /*------- Bounce -----------*/
      case MC_BOUNCE :
	cmd_bounce(state, msgmap, 0);
	break;


          /*------- Flag -----------*/
      case MC_FLAG :
	dprint(4, (debugfile, "\n - flag message -\n"));
	cmd_flag(state, msgmap, 0);
	break;


          /*------- Pipe message -----------*/
      case MC_PIPE :
	cmd_pipe(state, msgmap, 0);
	break;


          /*--------- Default, unknown command ----------*/
      default:
	panic("Unexpected command case");
	break;
    }

    return(a_changed || mn_get_cur(msgmap) != old_msgno);
}



/*----------------------------------------------------------------------
   Complain about bogus input

  Args: ch -- input command to complain about
	help -- string indicating where to get help

 ----*/
void
bogus_command(cmd, help)
    int   cmd;
    char *help;
{
    if(cmd == ctrl('Q') || cmd == ctrl('S'))
      q_status_message1(SM_ASYNC, 0, 2,
 "%.200s char received.  Set \"preserve-start-stop\" feature in Setup/Config.",
			pretty_command(cmd));
    else if(cmd == KEY_JUNK)
      q_status_message3(SM_ORDER, 0, 2,
		      "Invalid key pressed.%.200s%.200s%.200s",
		      (help) ? " Use " : "",
		      (help) ?  help   : "",
		      (help) ? " for help" : "");
    else
      q_status_message4(SM_ORDER, 0, 2,
	  "Command \"%.200s\" not defined for this screen.%.200s%.200s%.200s",
		      pretty_command(cmd),
		      (help) ? " Use " : "",
		      (help) ?  help   : "",
		      (help) ? " for help" : "");
}


/*----------------------------------------------------------------------
   Reveal Keymenu to Pine Command mappings

  Args: 

 ----*/
int
menu_command(keystroke, menu)
    int keystroke;
    struct key_menu *menu;
{
    int i, n;

    if(!menu)
      return(MC_UNKNOWN);

    if(F_ON(F_USE_FK,ps_global)){
	/* No alpha commands permitted in function key mode */
	if(keystroke < 0x0100 && isalpha((unsigned char) keystroke))
	  return(MC_UNKNOWN);

	/* Tres simple: compute offset, and test */
	if(keystroke >= F1 && keystroke <= F12){
	    n = (menu->which * 12) + (keystroke - F1);
	    if(bitnset(n, menu->bitmap))
	      return(menu->keys[n].bind.cmd);
	}
    }
    else if(keystroke >= F1 && keystroke <= F12)
      return(MC_UNKNOWN);

    /* if ascii, coerce lower case */
    if(keystroke < 0x0100 && isupper((unsigned char) keystroke))
      keystroke = tolower((unsigned char) keystroke);

    /* keep this here for Windows port */
    if((keystroke = validatekeys(keystroke)) == KEY_JUNK)
      return(MC_UNKNOWN);

    /* Scan the list for any keystroke/command binding */
    if(keystroke != NO_OP_COMMAND)
      for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
        if(bitnset(i, menu->bitmap))
	  for(n = menu->keys[i].bind.nch - 1; n >= 0; n--)
	    if(keystroke == menu->keys[i].bind.ch[n])
	      return(menu->keys[i].bind.cmd);

    /*
     * If explicit mapping failed, check feature mappings and
     * hardwired defaults...
     */
    if(F_ON(F_ENABLE_PRYNT,ps_global)
	&& (keystroke == 'y' || keystroke == 'Y')){
	/* SPECIAL CASE: Scan the list for print bindings */
	for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
	  if(bitnset(i, menu->bitmap))
	    if(menu->keys[i].bind.cmd == MC_PRINTMSG
	       || menu->keys[i].bind.cmd == MC_PRINTTXT)
	      return(menu->keys[i].bind.cmd);
    }

    if(F_ON(F_ENABLE_LESSTHAN_EXIT,ps_global)
       && (keystroke == '<' || keystroke == ','
	   || (F_ON(F_ARROW_NAV,ps_global) && keystroke == KEY_LEFT))){
	/* SPECIAL CASE: Scan the list for MC_EXIT bindings */
	for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
	  if(bitnset(i, menu->bitmap))
	    if(menu->keys[i].bind.cmd == MC_EXIT)
	      return(MC_EXIT);
    }

    /*
     * If no match after scanning bindings, try universally
     * bound keystrokes...
     */
    switch(keystroke){
      case KEY_MOUSE :
	return(MC_MOUSE);

      case ctrl('P') :
      case KEY_UP :
	return(MC_CHARUP);

      case ctrl('N') :
      case KEY_DOWN :
	return(MC_CHARDOWN);

      case ctrl('F') :
      case KEY_RIGHT :
	return(MC_CHARRIGHT);

      case ctrl('B') :
      case KEY_LEFT :
	return(MC_CHARLEFT);

      case ctrl('A') :
	return(MC_GOTOBOL);

      case ctrl('E') :
	return(MC_GOTOEOL);

      case  ctrl('L') :
	return(MC_REPAINT);

      case KEY_RESIZE :
	return(MC_RESIZE);

      case NO_OP_IDLE:
      case NO_OP_COMMAND:
	if(USER_INPUT_TIMEOUT(ps_global))
	  user_input_timeout_exit(ps_global->hours_to_timeout); /* no return */

	return(MC_NONE);

      default :
	break;
    }

    return(MC_UNKNOWN);			/* utter failure */
}



/*----------------------------------------------------------------------
   Set up a binding for cmd, with one key bound to it.
   Use menu_add_binding to add more keys to this binding.

  Args: menu   -- the keymenu
	key    -- the initial key to bind to
	cmd    -- the command to initialize to
	name   -- a pointer to the string to point name to
	label  -- a pointer to the string to point label to
	keynum -- which key in the keys array to initialize

 ----*/
void
menu_init_binding(menu, key, cmd, name, label, keynum)
    struct key_menu *menu;
    int              key, cmd;
    char            *name, *label;
    int              keynum;
{
    /* if ascii, coerce to lower case */
    if(key < 0x0100 && isupper((unsigned char)key))
      key = tolower((unsigned char)key);

    /* remove binding from any other key */
    menu_clear_cmd_binding(menu, cmd);

    menu->keys[keynum].name     = name;
    menu->keys[keynum].label    = label;
    menu->keys[keynum].bind.cmd = cmd;
    menu->keys[keynum].bind.nch = 0;
    menu->keys[keynum].bind.ch[menu->keys[keynum].bind.nch++] = key;
}


/*----------------------------------------------------------------------
   Add a key/command binding to the given keymenu structure

  Args: 

 ----*/
void
menu_add_binding(menu, key, cmd)
    struct key_menu *menu;
    int		     key, cmd;
{
    int i, n;

    /* NOTE: cmd *MUST* already have had a binding */
    for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
      if(menu->keys[i].bind.cmd == cmd){
	  for(n = menu->keys[i].bind.nch - 1;
	      n >= 0 && key != menu->keys[i].bind.ch[n];
	      n--)
	    ;

	  /* if ascii, coerce to lower case */
	  if(key < 0x0100 && isupper((unsigned char)key))
	    key = tolower((unsigned char)key);

	  if(n < 0)		/* not already bound, bind it */
	    menu->keys[i].bind.ch[menu->keys[i].bind.nch++] = key;

	  break;
      }
}


/*----------------------------------------------------------------------
   REMOVE a key/command binding from the given keymenu structure

  Args: 

 ----*/
int
menu_clear_binding(menu, key)
    struct key_menu *menu;
    int		     key;
{
    int i, n;

    /* if ascii, coerce to lower case */
    if(key < 0x0100 && isupper((unsigned char)key))
      key = tolower((unsigned char)key);

    for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
      for(n = menu->keys[i].bind.nch - 1; n >= 0; n--)
	if(key == menu->keys[i].bind.ch[n]){
	    int cmd = menu->keys[i].bind.cmd;

	    for(--menu->keys[i].bind.nch; n < menu->keys[i].bind.nch; n++)
	      menu->keys[i].bind.ch[n] = menu->keys[i].bind.ch[n+1];

	    return(cmd);
	}

    return(MC_UNKNOWN);
}


void
menu_clear_cmd_binding(menu, cmd)
    struct key_menu *menu;
    int		     cmd;
{
    int i;

    for(i = (menu->how_many * 12) - 1;  i >= 0; i--){
	if(cmd == menu->keys[i].bind.cmd){
	    menu->keys[i].name     = NULL;
	    menu->keys[i].label    = NULL;
	    menu->keys[i].bind.cmd = 0;
	    menu->keys[i].bind.nch = 0;
	    menu->keys[i].bind.ch[0] = 0;
	}
    }
}


int
menu_binding_index(menu, cmd)
    struct key_menu *menu;
    int		     cmd;
{
    int i;

    for(i = 0; i < menu->how_many * 12; i++)
      if(cmd == menu->keys[i].bind.cmd)
	return(i);

    return(-1);
}


/*----------------------------------------------------------------------
   Complain about command on empty folder

  Args: map -- msgmap 
	type -- type of message that's missing
	cmd -- string explaining command attempted

 ----*/
int
any_messages(map, type, cmd)
    MSGNO_S *map;
    char *type, *cmd;
{
    if(mn_get_total(map) <= 0L){
	q_status_message5(SM_ORDER, 0, 2, "No %.200s%.200s%.200s%.200s%.200s",
			  type ? type : "",
			  type ? " " : "",
			  THRD_INDX() ? "threads" : "messages",
			  (!cmd || *cmd != '.') ? " " : "",
			  cmd ? cmd : "in folder");
	return(FALSE);
    }

    return(TRUE);
}


/*----------------------------------------------------------------------
   test whether or not we have a valid stream to set flags on

  Args: state -- pine state containing vital signs
	cmd -- string explaining command attempted
	permflag -- associated permanent flag state

  Result: returns 1 if we can set flags, otw 0 and complains

 ----*/
int
can_set_flag(state, cmd, permflag)
    struct pine *state;
    char	*cmd;
{
    if((!permflag && READONLY_FOLDER(state->mail_stream))
       || sp_dead_stream(state->mail_stream)){
	q_status_message2(SM_ORDER | (sp_dead_stream(state->mail_stream)
				        ? SM_DING : 0),
			  0, 3,
			  "Can't %.200s message.  Folder is %.200s.", cmd,
			  (sp_dead_stream(state->mail_stream)) ? "closed" : "read-only");
	return(FALSE);
    }

    return(TRUE);
}



/*----------------------------------------------------------------------
   Complain about command on empty folder

  Args: type -- type of message that's missing
	cmd -- string explaining command attempted

 ----*/
void
cmd_cancelled(cmd)
    char *cmd;
{
    q_status_message1(SM_INFO, 0, 2, "%.200s cancelled", cmd ? cmd : "Command");
}
	

void
mail_expunge_prefilter(stream, flags)
    MAILSTREAM *stream;
    int         flags;
{
    int sfdo_state = 0,		/* Some Filter Depends On or Sets State  */
	sfdo_scores = 0,	/* Some Filter Depends On Scores */
	ssdo_state = 0;		/* Some Score Depends On State   */
    
    if(!stream || !sp_flagged(stream, SP_LOCKED))
      return;

    /*
     * An Expunge causes a re-examination of the filters to
     * see if any state changes have caused new matches.
     */
    
    sfdo_scores = (scores_are_used(SCOREUSE_GET) & SCOREUSE_FILTERS);
    if(sfdo_scores)
      ssdo_state = (scores_are_used(SCOREUSE_GET) & SCOREUSE_STATEDEP);

    if(!(sfdo_scores && ssdo_state))
      sfdo_state = some_filter_depends_on_active_state();


    if(sfdo_state || (sfdo_scores && ssdo_state)){
	if(sfdo_scores && ssdo_state)
	  clear_folder_scores(stream);

	reprocess_filter_patterns(stream, sp_msgmap(stream),
				  (flags & MI_CLOSING) |
				  MI_REFILTERING | MI_STATECHGONLY);
    }
}


/*----------------------------------------------------------------------
   Execute DELETE message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: with side effect of "current" message delete flag set

 ----*/
void
cmd_delete(state, msgmap, agg, in_index)
     struct pine *state;
     MSGNO_S     *msgmap;
     int	  agg;
     CmdWhere	  in_index;
{
    int	  lastmsg, opts;
    long  msgno, del_count = 0L, new;
    char *sequence = NULL, prompt[128];

    dprint(4, (debugfile, "\n - delete message -\n"));
    if(!(any_messages(msgmap, NULL, "to Delete")
	 && can_set_flag(state, "delete", state->mail_stream->perm_deleted)))
      return;

    if(sp_io_error_on_stream(state->mail_stream)){
	sp_set_io_error_on_stream(state->mail_stream, 0);
	pine_mail_check(state->mail_stream);		/* forces write */
    }

    if(agg){
	sequence = selected_sequence(state->mail_stream, msgmap, &del_count, 0);
	sprintf(prompt, "%ld%s message%s marked for deletion",
		del_count, (agg == 2) ? "" : " selected", plural(del_count));
    }
    else{
	long rawno;

	msgno	  = mn_get_cur(msgmap);
	rawno     = mn_m2raw(msgmap, msgno);
	del_count = 1L;				/* return current */
	sequence  = cpystr(long2string(rawno));
	lastmsg	  = (msgno >= mn_get_total(msgmap));
	sprintf(prompt, "%s%s marked for deletion",
		lastmsg ? "Last message" : "Message ",
		lastmsg ? "" : long2string(msgno));
    }

    dprint(3,(debugfile, "DELETE: msg %s\n", sequence ? sequence : "?"));
    new = sp_new_mail_count(state->mail_stream);
    mail_flag(state->mail_stream, sequence, "\\DELETED", ST_SET);
    fs_give((void **) &sequence);
    if(new != sp_new_mail_count(state->mail_stream))
      process_filter_patterns(state->mail_stream, state->msgmap,
			      sp_new_mail_count(state->mail_stream));

    if(!agg){

	advance_cur_after_delete(state, state->mail_stream, msgmap, in_index);

	if(IS_NEWS(state->mail_stream)
		|| ((state->context_current->use & CNTXT_INCMNG)
		    && context_isambig(state->cur_folder))){

	    opts = (NSF_TRUST_FLAGS | NSF_SKIP_CHID);
	    if(in_index == View)
	      opts &= ~NSF_SKIP_CHID;

	    (void)next_sorted_flagged(F_UNDEL|F_UNSEEN, state->mail_stream,
				      msgno, &opts);
	    if(!(opts & NSF_FLAG_MATCH)){
		char nextfolder[MAXPATH];

		strncpy(nextfolder, state->cur_folder, sizeof(nextfolder));
		nextfolder[sizeof(nextfolder)-1] = '\0';
		strncat(prompt, next_folder(NULL, nextfolder, nextfolder,
					   state->context_current, NULL, NULL)
				  ? ".  Press TAB for next folder."
				  : ".  No more folders to TAB to.",
			sizeof(prompt)-1-strlen(prompt));
	    }
	}
    }

    q_status_message(SM_ORDER, 0, 3, prompt);
}


void
advance_cur_after_delete(state, stream, msgmap, in_index)
    struct pine *state;
    MAILSTREAM  *stream;
    MSGNO_S     *msgmap;
    CmdWhere     in_index;
{
    long new_msgno, msgno;
    int  opts;

    new_msgno = msgno = mn_get_cur(msgmap);
    opts = NSF_TRUST_FLAGS;

    if(F_ON(F_DEL_SKIPS_DEL, state)){

	if(THREADING() && sp_viewing_a_thread(stream))
	  opts |= NSF_SKIP_CHID;

	new_msgno = next_sorted_flagged(F_UNDEL, stream, msgno, &opts);
    }
    else{
	mn_inc_cur(stream, msgmap,
		   (in_index == View && THREADING()
		    && sp_viewing_a_thread(stream))
		     ? MH_THISTHD
		     : (in_index == View)
		       ? MH_ANYTHD : MH_NONE);
	new_msgno = mn_get_cur(msgmap);
	if(new_msgno != msgno)
	  opts |= NSF_FLAG_MATCH;
    }

    /*
     * Viewing_a_thread is the complicated case because we want to ignore
     * other threads at first and then look in other threads if we have to.
     * By ignoring other threads we also ignore collapsed partial threads
     * in our own thread.
     */
    if(THREADING() && sp_viewing_a_thread(stream) && !(opts & NSF_FLAG_MATCH)){
	long rawno, orig_thrdno;
	PINETHRD_S *thrd, *topthrd = NULL;

	rawno = mn_m2raw(msgmap, msgno);
	thrd  = fetch_thread(stream, rawno);
	if(thrd && thrd->top)
	  topthrd = fetch_thread(stream, thrd->top);

	orig_thrdno = topthrd ? topthrd->thrdno : -1L;

	opts = NSF_TRUST_FLAGS;
	new_msgno = next_sorted_flagged(F_UNDEL, stream, msgno, &opts);

	/*
	 * If we got a match, new_msgno may be a message in
	 * a different thread from the one we are viewing, or it could be
	 * in a collapsed part of this thread.
	 */
	if(opts & NSF_FLAG_MATCH){
	    int         ret;
	    char        pmt[128];

	    topthrd = NULL;
	    thrd = fetch_thread(stream, mn_m2raw(msgmap,new_msgno));
	    if(thrd && thrd->top)
	      topthrd = fetch_thread(stream, thrd->top);
	    
	    /*
	     * If this match is in the same thread we're already in
	     * then we're done, else we have to ask the user and maybe
	     * switch threads.
	     */
	    if(!(orig_thrdno > 0L && topthrd
		 && topthrd->thrdno == orig_thrdno)){

		if(F_OFF(F_AUTO_OPEN_NEXT_UNREAD, state)){
		    if(in_index == View)
		      sprintf(pmt,
			     "View message in thread number %.10s",
			     topthrd ? comatose(topthrd->thrdno) : "?");
		    else
		      sprintf(pmt, "View thread number %.10s",
			     topthrd ? comatose(topthrd->thrdno) : "?");
			    
		    ret = want_to(pmt, 'y', 'x', NO_HELP, WT_NORM);
		}
		else
		  ret = 'y';

		if(ret == 'y'){
		    unview_thread(state, stream, msgmap);
		    mn_set_cur(msgmap, new_msgno);
		    if(THRD_AUTO_VIEW()
		       && (count_lflags_in_thread(stream, topthrd, msgmap,
						  MN_NONE) == 1)
		       && view_thread(state, stream, msgmap, 1)){
			state->view_skipped_index = 1;
			state->next_screen = mail_view_screen;
		    }
		    else{
			view_thread(state, stream, msgmap, 1);
			state->next_screen = SCREEN_FUN_NULL;
		    }
		}
		else
		  new_msgno = msgno;	/* stick with original */
	    }
	}
    }

    mn_set_cur(msgmap, new_msgno);
    if(in_index != View)
      adjust_cur_to_visible(stream, msgmap);
}



/*----------------------------------------------------------------------
   Execute UNDELETE message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: with side effect of "current" message delete flag UNset

 ----*/
void
cmd_undelete(state, msgmap, agg)
     struct pine *state;
     MSGNO_S     *msgmap;
     int	  agg;
{
    long	  del_count;
    char	 *sequence;
    int		  wasdeleted = FALSE;
    MESSAGECACHE *mc;

    dprint(4, (debugfile, "\n - undelete -\n"));
    if(!(any_messages(msgmap, NULL, "to Undelete")
	 && can_set_flag(state, "undelete", state->mail_stream->perm_deleted)))
      return;

    if(agg){
	del_count = 0L;				/* return current */
	sequence = selected_sequence(state->mail_stream, msgmap, &del_count, 1);
    }
    else{
	long rawno;
	int  exbits = 0;

	del_count = 1L;				/* return current */
	rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
	sequence  = cpystr(long2string(rawno));
	wasdeleted = (state->mail_stream
	              && rawno > 0L && rawno <= state->mail_stream->nmsgs
		      && (mc = mail_elt(state->mail_stream, rawno))
		       && mc->valid
		       && mc->deleted);
	/*
	 * Mark this message manually flagged so we don't re-filter it
	 * with a filter which only sets flags.
	 */
	if(msgno_exceptions(state->mail_stream, rawno, "0", &exbits, FALSE))
	  exbits |= MSG_EX_MANUNDEL;
	else
	  exbits = MSG_EX_MANUNDEL;

	msgno_exceptions(state->mail_stream, rawno, "0", &exbits, TRUE);
    }

    dprint(3,(debugfile, "UNDELETE: msg %s\n", sequence ? sequence : "?"));

    mail_flag(state->mail_stream, sequence, "\\DELETED", 0L);
    fs_give((void **) &sequence);

    if(del_count == 1L && !agg){
	update_titlebar_status();
	q_status_message(SM_ORDER, 0, 3,
			wasdeleted
			 ? "Deletion mark removed, message won't be deleted"
			 : "Message not marked for deletion; no action taken");
    }
    else
      q_status_message2(SM_ORDER, 0, 3,
			"Deletion mark removed from %.200s message%.200s",
			comatose(del_count), plural(del_count));

    if(sp_io_error_on_stream(state->mail_stream)){
	sp_set_io_error_on_stream(state->mail_stream, 0);
	pine_mail_check(state->mail_stream);		/* forces write */
    }
}



/*----------------------------------------------------------------------
   Execute FLAG message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: with side effect of "current" message FLAG flag set or UNset

 ----*/
void
cmd_flag(state, msgmap, agg)
    struct pine *state;
    MSGNO_S     *msgmap;
    int		 agg;
{
    char	  *flagit, *seq, *screen_text[20], **exp, **p, *answer = NULL;
    char          *q, **t;
    char          *keyword_array[2];
    long	   unflagged, flagged, flags, rawno;
    MESSAGECACHE  *mc = NULL;
    KEYWORD_S     *kw;
    int            i, cnt, is_set, trouble = 0;
    struct flag_table *fp, *ftbl = NULL;
    struct flag_screen flag_screen;
    static char   *flag_screen_text1[] = {
	"    Set desired flags for current message below.  An 'X' means set",
	"    it, and a ' ' means to unset it.  Choose \"Exit\" when finished.",
	NULL
    };
    static char   *flag_screen_text2[] = {
	"    Set desired flags below for selected messages.  A '?' means to",
	"    leave the flag unchanged, 'X' means to set it, and a ' ' means",
	"    to unset it.  Use the \"Return\" key to toggle, and choose",
	"    \"Exit\" when finished.",
	NULL
    };
    static struct  flag_table default_ftbl[] = {
	/*
	 * At some point when user defined flags are possible,
	 * it should just be a simple matter of grabbing this
	 * array from the heap and explicitly filling the
	 * non-system flags in at run time...
	 *  {NULL, h_flag_user, F_USER, 0, 0},
	 */
	{"Important", h_flag_important, F_FLAG, 0, 0, NULL, NULL},
	{"New",	  h_flag_new, F_SEEN, 0, 0, NULL, NULL},
	{"Answered",  h_flag_answered, F_ANS, 0, 0, NULL, NULL},
	{"Deleted",   h_flag_deleted, F_DEL, 0, 0, NULL, NULL},
	{NULL, NO_HELP, 0, 0, 0, NULL, NULL}
    };

    /* Only check for dead stream for now.  Should check permanent flags
     * eventually
     */
    if(!(any_messages(msgmap, NULL, "to Flag")
	 && can_set_flag(state, "flag", 1)))
      return;

    if(sp_io_error_on_stream(state->mail_stream)){
	sp_set_io_error_on_stream(state->mail_stream, 0);
	pine_mail_check(state->mail_stream);		/* forces write */
	return;
    }

    /* count how large ftbl will be */
    for(cnt = 0; default_ftbl[cnt].name; cnt++)
      ;
    
    /* add user flags */
    for(kw = ps_global->keywords; kw; kw = kw->next)
      cnt++;
    
    /* set up ftbl */
    ftbl = (struct flag_table *) fs_get((cnt+1) * sizeof(*ftbl));
    memset(ftbl, 0, (cnt+1) * sizeof(*ftbl));
    for(i = 0, fp = ftbl; default_ftbl[i].name; i++, fp++){
	fp->name = cpystr(default_ftbl[i].name);
	fp->help = default_ftbl[i].help;
	fp->flag = default_ftbl[i].flag;
	fp->set  = default_ftbl[i].set;
	fp->ukn  = default_ftbl[i].ukn;
    }

    for(kw = ps_global->keywords; kw; kw = kw->next){
	fp->name = cpystr(kw->nick ? kw->nick : kw->kw ? kw->kw : "");
	fp->keyword = cpystr(kw->kw ? kw->kw : "");
	if(kw->nick && kw->kw){
	    fp->comment = (char *) fs_get(strlen(kw->kw)+3);
	    sprintf(fp->comment, "(%.*s)", strlen(kw->kw), kw->kw);
	}

	fp->help = h_flag_user_flag;
	fp->flag = F_KEYWORD;
	fp->set  = 0;
	fp->ukn  = 0;
	fp++;
    }

    flag_screen.flag_table  = &ftbl;
    flag_screen.explanation = screen_text;

    if(agg){
	if(!pseudo_selected(msgmap)){
	    free_flag_table(&ftbl);
	    return;
	}

	exp = flag_screen_text2;
	for(fp = ftbl; fp->name; fp++){
	    fp->set = CMD_FLAG_UNKN;		/* set to unknown */
	    fp->ukn = TRUE;
	}
    }
    else if(state->mail_stream
	    && (rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
	    && rawno <= state->mail_stream->nmsgs
	    && (mc = mail_elt(state->mail_stream, rawno))){
	exp = flag_screen_text1;
	for(fp = &ftbl[0]; fp->name; fp++){
	    fp->ukn = 0;
	    if(fp->flag == F_KEYWORD){
		/* see if this keyword is defined for this message */
		fp->set = CMD_FLAG_CLEAR;
		if(user_flag_is_set(state->mail_stream,
				    rawno, fp->keyword))
		  fp->set = CMD_FLAG_SET;
	    }
	    else
	      fp->set = ((fp->flag == F_SEEN && !mc->seen)
		         || (fp->flag == F_DEL && mc->deleted)
		         || (fp->flag == F_FLAG && mc->flagged)
		         || (fp->flag == F_ANS && mc->answered))
			  ? CMD_FLAG_SET : CMD_FLAG_CLEAR;
	}
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Error accessing message data");
	free_flag_table(&ftbl);
	return;
    }

#ifdef _WINDOWS
    if (mswin_usedialog ()) {
	if (!os_flagmsgdialog (&ftbl[0])){
	    free_flag_table(&ftbl);
	    return;
	}
    }
    else
#endif	    
    {
	int use_maint_screen;
	int keyword_shortcut = 0;

	use_maint_screen = F_ON(F_FLAG_SCREEN_DFLT, ps_global);

	if(!use_maint_screen){
	    /*
	     * We're going to call cmd_flag_prompt(). We need
	     * to decide whether or not to offer the keyword setting
	     * shortcut. We'll offer it if the user has the feature
	     * enabled AND there are some possible keywords that could
	     * be set.
	     */
	    if(F_ON(F_FLAG_SCREEN_KW_SHORTCUT, ps_global)){
		for(fp = &ftbl[0]; fp->name && !keyword_shortcut; fp++){
		    if(fp->flag == F_KEYWORD){
			int first_char;
			ESCKEY_S *tp;

			first_char = (fp->name && fp->name[0])
					? fp->name[0] : -2;
			if(isascii(first_char) && isupper(first_char))
			  first_char = tolower((unsigned char) first_char);

			for(tp=flag_text_opt; tp->ch != -1; tp++)
			  if(tp->ch == first_char)
			    break;

			if(tp->ch == -1)
			  keyword_shortcut++;
		    }
		}
	    }

	    use_maint_screen = !cmd_flag_prompt(state, &flag_screen,
						keyword_shortcut);
	}

	if(use_maint_screen){
	    screen_text[0] = "";
	    for(p = &screen_text[1]; *exp; p++, exp++)
	      *p = *exp;

	    *p = NULL;

	    flag_maintenance_screen(state, &flag_screen);
	}
    }

    /* reaquire the elt pointer */
    mc = (state->mail_stream
	  && (rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
	  && rawno <= state->mail_stream->nmsgs)
	  ? mail_elt(state->mail_stream, rawno) : NULL;

    for(fp = ftbl; mc && fp->name; fp++){
	flags = -1;
	switch(fp->flag){
	  case F_SEEN:
	    if((!agg && fp->set != !mc->seen)
	       || (agg && fp->set != CMD_FLAG_UNKN)){
		flagit = "\\SEEN";
		if(fp->set){
		    flags     = 0L;
		    unflagged = F_SEEN;
		}
		else{
		    flags     = ST_SET;
		    unflagged = F_UNSEEN;
		}
	    }

	    break;

	  case F_ANS:
	    if((!agg && fp->set != mc->answered)
	       || (agg && fp->set != CMD_FLAG_UNKN)){
		flagit = "\\ANSWERED";
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNANS;
		}
		else{
		    flags     = 0L;
		    unflagged = F_ANS;
		}
	    }

	    break;

	  case F_DEL:
	    if((!agg && fp->set != mc->deleted)
	       || (agg && fp->set != CMD_FLAG_UNKN)){
		flagit = "\\DELETED";
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNDEL;
		}
		else{
		    flags     = 0L;
		    unflagged = F_DEL;
		}
	    }

	    break;

	  case F_FLAG:
	    if((!agg && fp->set != mc->flagged)
	       || (agg && fp->set != CMD_FLAG_UNKN)){
		flagit = "\\FLAGGED";
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNFLAG;
		}
		else{
		    flags     = 0L;
		    unflagged = F_FLAG;
		}
	    }

	    break;

	  case F_KEYWORD:
	    if(!agg){
		/* see if this keyword is defined for this message */
		is_set = CMD_FLAG_CLEAR;
		if(user_flag_is_set(state->mail_stream,
				    mn_m2raw(msgmap, mn_get_cur(msgmap)),
				    fp->keyword))
		  is_set = CMD_FLAG_SET;
	    }

	    if((!agg && fp->set != is_set)
	       || (agg && fp->set != CMD_FLAG_UNKN)){
		flagit = fp->keyword;
		keyword_array[0] = fp->keyword;
		keyword_array[1] = NULL;
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNKEYWORD;
		}
		else{
		    flags     = 0L;
		    unflagged = F_KEYWORD;
		}
	    }

	    break;

	  default:
	    break;
	}

	flagged = 0L;
	if(flags >= 0L
	   && (seq = currentf_sequence(state->mail_stream, msgmap,
				       unflagged, &flagged, unflagged & F_DEL,
				       (fp->flag == F_KEYWORD
				        && unflagged == F_KEYWORD)
					 ? keyword_array : NULL,
				       (fp->flag == F_KEYWORD
				        && unflagged == F_UNKEYWORD)
					 ? keyword_array : NULL))){
	    /*
	     * For user keywords, we may have to create the flag in
	     * the folder if it doesn't already exist and we are setting
	     * it (as opposed to clearing it). Mail_flag will
	     * do that for us, but it's failure isn't very friendly
	     * error-wise. So we try to make it a little smoother.
	     */
	    if(fp->flag != F_KEYWORD || !fp->set
	       || ((i=user_flag_index(state->mail_stream, flagit)) >= 0
	           && i < NUSERFLAGS))
	      mail_flag(state->mail_stream, seq, flagit, flags);
	    else{
		/* trouble, see if we can add the user flag */
		if(state->mail_stream->kwd_create)
		  mail_flag(state->mail_stream, seq, flagit, flags);
		else{
		    int some_defined = 0;

		    trouble++;
		    
		    for(i = 0; !some_defined && i < NUSERFLAGS; i++)
		      if(state->mail_stream->user_flags[i])
			some_defined++;
		    
		    if(some_defined)
		      q_status_message(SM_ORDER, 3, 4,
			       "No more keywords allowed in this folder!");
		    else
		      q_status_message(SM_ORDER, 3, 4,
				   "Cannot add keywords for this folder");
		}
	    }

	    fs_give((void **) &seq);
	    if(flagged && !trouble){
		sprintf(tmp_20k_buf, "%slagged%s%s%s%s%s message%s%s \"%s\"",
			(fp->set) ? "F" : "Unf",
			agg ? " " : "",
			agg ? long2string(flagged) : "",
			(agg && flagged != mn_total_cur(msgmap))
			  ? " (of " : "",
			(agg && flagged != mn_total_cur(msgmap))
			  ? comatose(mn_total_cur(msgmap)) : "",
			(agg && flagged != mn_total_cur(msgmap))
			  ? ")" : "",
			agg ? plural(flagged) : " ",
			agg ? "" : long2string(mn_get_cur(msgmap)),
			fp->name);
		q_status_message(SM_ORDER, 0, 2, answer = tmp_20k_buf);
	    }
	}
    }

    if(!answer)
      q_status_message(SM_ORDER, 0, 2, "No flags changed.");

  fini:
    free_flag_table(&ftbl);
    if(agg)
      restore_selected(msgmap);
}


int
user_flag_is_set(stream, rawno, keyword)
    MAILSTREAM   *stream;
    unsigned long rawno;
    char         *keyword;
{
    int           j, is_set = 0;
    MESSAGECACHE *mc;

    if(stream){
	if(rawno > 0L && stream
	   && rawno <= stream->nmsgs
	   && (mc = mail_elt(stream, rawno)) != NULL){
	    j = user_flag_index(stream, keyword);
	    if(j >= 0 && j < NUSERFLAGS && ((1 << j) & mc->user_flags))
	      is_set++;
	}
    }
	
    return(is_set);
}


/*
 * Returns the bit position of the keyword in stream, else -1.
 */
int
user_flag_index(stream, keyword)
    MAILSTREAM *stream;
    char       *keyword;
{
    int i, retval = -1;

    if(stream && keyword){
	for(i = 0; i < NUSERFLAGS; i++)
	  if(stream->user_flags[i] && !strucmp(keyword, stream->user_flags[i])){
	      retval = i;
	      break;
	  }
    }

    return(retval);
}


/*----------------------------------------------------------------------
   Offer concise status line flag prompt 

  Args: state --  Various satate info
        flags -- flags to offer setting

 Result: TRUE if flag to set specified in flags struct or FALSE otw

 ----*/
int
cmd_flag_prompt(state, flags, allow_keyword_shortcuts)
    struct pine	       *state;
    struct flag_screen *flags;
    int                 allow_keyword_shortcuts;
{
    int			r, setflag = 1, first_char;
    struct flag_table  *fp;
    ESCKEY_S           *ek;
    char               *ftext, *ftext_not;
    static char *flag_text =
  "Flag New, Deleted, Answered, or Important ? ";
    static char *flag_text_ak =
  "Flag New, Deleted, Answered, Important (or Keyword initial) ? ";
    static char *flag_text_not =
  "Flag NOT New, NOT Deleted, NOT Answered, or NOT Important ? ";
    static char *flag_text_ak_not =
  "Flag NOT New, NOT Deleted, NOT Answered, NOT Important, or NOT KI ? ";

    if(allow_keyword_shortcuts){
	int       cnt = 0;
	ESCKEY_S *dp, *sp, *tp;

	for(sp=flag_text_opt; sp->ch != -1; sp++)
	  cnt++;

	for(fp=(flags->flag_table ? *flags->flag_table : NULL); fp->name; fp++)
	  if(fp->flag == F_KEYWORD)
	    cnt++;

	/* set up an ESCKEY_S list which includes invisible keys for keywords */
	ek = (ESCKEY_S *) fs_get((cnt + 1) * sizeof(*ek));
	memset(ek, 0, (cnt+1) * sizeof(*ek));
	for(dp=ek, sp=flag_text_opt; sp->ch != -1; sp++, dp++)
	  *dp = *sp;

	for(fp=(flags->flag_table ? *flags->flag_table : NULL); fp->name; fp++){
	    if(fp->flag == F_KEYWORD){
		first_char = (fp->name && fp->name[0]) ? fp->name[0] : -2;
		if(isascii(first_char) && isupper(first_char))
		  first_char = tolower((unsigned char) first_char);

		/*
		 * Check to see if an earlier keyword in the list, or one of
		 * the builtin system letters already uses this character.
		 * If so, the first one wins.
		 */
		for(tp=ek; tp->ch != 0; tp++)
		  if(tp->ch == first_char)
		    break;

		if(tp->ch != 0)
		  continue;		/* skip it, already used that char */

		dp->ch    = first_char;
		dp->rval  = first_char;
		dp->name  = "";
		dp->label = "";
		dp++;
	    }
	}

	dp->ch = -1;
	ftext = flag_text_ak;
	ftext_not = flag_text_ak_not;
    }
    else{
	ek = flag_text_opt;
	ftext = flag_text;
	ftext_not = flag_text_not;
    }

    while(1){
	r = radio_buttons(setflag ? ftext : ftext_not,
			  -FOOTER_ROWS(state), ek, '*', SEQ_EXCEPTION-1,
			  NO_HELP, RB_NORM | RB_SEQ_SENSITIVE, NULL);
	/*
	 * It is SEQ_EXCEPTION-1 just so that it is some value that isn't
	 * being used otherwise. The keywords use up all the possible
	 * letters, so a negative number is good, but it has to be different
	 * from other negative return values.
	 */
	if(r == SEQ_EXCEPTION-1)	/* ol'cancelrooney */
	  return(TRUE);
	else if(r == 10)		/* return and goto flag screen */
	  return(FALSE);
	else if(r == '!')		/* flip intention */
	  setflag = !setflag;
	else
	  break;
    }

    for(fp = (flags->flag_table ? *flags->flag_table : NULL); fp->name; fp++){
	if(r == 'n' || r == '*' || r == 'd' || r == 'a'){
	    if((r == 'n' && fp->flag == F_SEEN)
	       || (r == '*' && fp->flag == F_FLAG)
	       || (r == 'd' && fp->flag == F_DEL)
	       || (r == 'a' && fp->flag == F_ANS)){
		fp->set = setflag ? CMD_FLAG_SET : CMD_FLAG_CLEAR;
		break;
	    }
	}
	else if(allow_keyword_shortcuts && fp->flag == F_KEYWORD){
	    first_char = (fp->name && fp->name[0]) ? fp->name[0] : -2;
	    if(isascii(first_char) && isupper(first_char))
	      first_char = tolower((unsigned char) first_char);

	    if(r == first_char){
		fp->set = setflag ? CMD_FLAG_SET : CMD_FLAG_CLEAR;
		break;
	    }
	}
    }

    if(ek != flag_text_opt)
      fs_give((void **) &ek);

    return(TRUE);
}


/*
 * (*ft) is an array of flag_table entries.
 */
void
free_flag_table(ft)
    struct flag_table **ft;
{
    struct flag_table *fp;

    if(ft && *ft){
	for(fp = (*ft); fp->name; fp++){
	    if(fp->name)
	      fs_give((void **) &fp->name);
	    
	    if(fp->keyword)
	      fs_give((void **) &fp->keyword);
	    
	    if(fp->comment)
	      fs_give((void **) &fp->comment);
	}
	
	fs_give((void **) ft);
    }
}



/*----------------------------------------------------------------------
   Execute REPLY message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: reply sent or not

 ----*/
void
cmd_reply(state, msgmap, agg)
     struct pine *state;
     MSGNO_S     *msgmap;
     int	  agg;
{
    if(any_messages(msgmap, NULL, "to Reply to")){
#if	defined(DOS) && !defined(WIN32)
	flush_index_cache();		/* save room on PC */
#endif
	if(agg && !pseudo_selected(msgmap))
	  return;

	reply(state, NULL);

	if(agg)
	  restore_selected(msgmap);

	state->mangled_screen = 1;
    }
}



/*----------------------------------------------------------------------
   Execute FORWARD message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: selected message[s] forwarded or not

 ----*/
void
cmd_forward(state, msgmap, agg)
     struct pine *state;
     MSGNO_S     *msgmap;
     int	  agg;
{
    if(any_messages(msgmap, NULL, "to Forward")){
#if	defined(DOS) && !defined(WIN32)
	flush_index_cache();		/* save room on PC */
#endif
	if(agg && !pseudo_selected(msgmap))
	  return;

	forward(state, NULL);

	if(agg)
	  restore_selected(msgmap);

	state->mangled_screen = 1;
    }
}



/*----------------------------------------------------------------------
   Execute BOUNCE message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: selected message[s] bounced or not

 ----*/
void
cmd_bounce(state, msgmap, agg)
     struct pine *state;
     MSGNO_S     *msgmap;
     int	  agg;
{
    if(any_messages(msgmap, NULL, "to Bounce")){
#if	defined(DOS) && !defined(WIN32)
	flush_index_cache();			/* save room on PC */
#endif
	if(agg && !pseudo_selected(msgmap))
	  return;

	bounce(state, NULL);
	if(agg)
	  restore_selected(msgmap);

	state->mangled_footer = 1;
    }
}



/*----------------------------------------------------------------------
   Execute save message command: prompt for folder and call function to save

  Args: screen_line    --  Line on the screen to prompt on
        message        --  The MESSAGECACHE entry of message to save

 Result: The folder lister can be called to make selection; mangled screen set

   This does the prompting for the folder name to save to, possibly calling 
 up the folder display for selection of folder by user.                 
 ----*/
void
cmd_save(state, stream, msgmap, agg, in_index)
    struct pine *state;
    MAILSTREAM  *stream;
    MSGNO_S	*msgmap;
    int		 agg;
    CmdWhere     in_index;
{
    char	      newfolder[MAILTMPLEN], nmsgs[32];
    int		      we_cancel = 0;
    long	      i, raw;
    CONTEXT_S	     *cntxt = NULL;
    ENVELOPE	     *e = NULL;
    SaveDel           del = DontAsk;

    dprint(4, (debugfile, "\n - saving message -\n"));

    if(agg && !pseudo_selected(msgmap))
      return;

    state->ugly_consider_advancing_bit = 0;
    if(F_OFF(F_SAVE_PARTIAL_WO_CONFIRM, state)
       && msgno_any_deletedparts(stream, msgmap)
       && want_to("Saved copy will NOT include entire message!  Continue",
		  'y', 'n', NO_HELP, WT_FLUSH_IN | WT_SEQ_SENSITIVE) != 'y'){
	cmd_cancelled("Save message");
	if(agg)
	  restore_selected(msgmap);

	return;
    }

    raw = mn_m2raw(msgmap, mn_get_cur(msgmap));

    if(mn_total_cur(msgmap) <= 1L){
	sprintf(nmsgs, "Msg #%ld ", mn_get_cur(msgmap));
	e = pine_mail_fetchstructure(stream, raw, NULL);
	if(!e) {
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Can't save message.  Error accessing folder");
	    restore_selected(msgmap);
	    return;
	}
    }
    else{
	sprintf(nmsgs, "%s msgs ", comatose(mn_total_cur(msgmap)));

	/* e is just used to get a default save folder from the first msg */
	e = pine_mail_fetchstructure(stream,
				     mn_m2raw(msgmap, mn_first_cur(msgmap)),
				     NULL);
    }

    del = (!READONLY_FOLDER(stream) && F_OFF(F_SAVE_WONT_DELETE, ps_global))
	     ? Del : NoDel;
    if(save_prompt(state, &cntxt, newfolder, sizeof(newfolder), nmsgs, e,
		   raw, NULL, &del)){
	we_cancel = busy_alarm(1, NULL, NULL, 0);
	i = save(state, stream, cntxt, newfolder, msgmap,
		 ((del == RetDel) ? SV_DELETE : 0) | SV_FIX_DELS);
	if(we_cancel)
	  cancel_busy_alarm(0);

	if(i == mn_total_cur(msgmap)){
	    if(mn_total_cur(msgmap) <= 1L){
		int need, avail = ps_global->ttyo->screen_cols - 2;
		int lennick, lenfldr;

		if(cntxt
		   && ps_global->context_list->next
		   && context_isambig(newfolder)){
		    lennick = min(strlen(cntxt->nickname), 500);
		    lenfldr = min(strlen(newfolder), 500);
		    need = 27 + strlen(long2string(mn_get_cur(msgmap))) +
			   lenfldr + lennick;
		    if(need > avail){
			if(lennick > 10){
			    need -= min(lennick-10, need-avail);
			    lennick -= min(lennick-10, need-avail);
			}

			if(need > avail && lenfldr > 10)
			  lenfldr -= min(lenfldr-10, need-avail);
		    }

		    sprintf(tmp_20k_buf,
			    "Message %.10s copied to \"%.99s\" in <%.99s>",
			    long2string(mn_get_cur(msgmap)),
			    short_str(newfolder, (char *)(tmp_20k_buf+1000),
				      lenfldr, MidDots),
			    short_str(cntxt->nickname,
				      (char *)(tmp_20k_buf+2000),
				      lennick, EndDots));
		}
		else{
		    char *f = " folder";

		    lenfldr = min(strlen(newfolder), 500);
		    need = 28 + strlen(long2string(mn_get_cur(msgmap))) +
			   lenfldr;
		    if(need > avail){
			need -= strlen(f);
			f = "";
			if(need > avail && lenfldr > 10)
			  lenfldr -= min(lenfldr-10, need-avail);
		    }

		    sprintf(tmp_20k_buf,
			    "Message %.10s copied to%.10s \"%.99s\"",
			    long2string(mn_get_cur(msgmap)), f,
			    short_str(newfolder, (char *)(tmp_20k_buf+1000),
				      lenfldr, MidDots));
		}
	    }
	    else
	      sprintf(tmp_20k_buf, "%s messages saved",
		      comatose(mn_total_cur(msgmap)));

	    if(del == RetDel)
	      strcat(tmp_20k_buf, " and deleted");

	    q_status_message(SM_ORDER, 0, 3, tmp_20k_buf);

	    if(!agg && F_ON(F_SAVE_ADVANCES, state)){
		if(sp_new_mail_count(stream))
		  process_filter_patterns(stream, msgmap,
					  sp_new_mail_count(stream));

		mn_inc_cur(stream, msgmap,
			   (in_index == View && THREADING()
			    && sp_viewing_a_thread(stream))
			     ? MH_THISTHD
			     : (in_index == View)
			       ? MH_ANYTHD : MH_NONE);
	    }

	    state->ugly_consider_advancing_bit = 1;
	}
    }

    if(agg)					/* straighten out fakes */
      restore_selected(msgmap);

    if(del == RetDel)
      update_titlebar_status();			/* make sure they see change */
}


void
role_compose(state)
    struct pine *state;
{
    int action;

    if(F_ON(F_ALT_ROLE_MENU, state) && mn_get_total(state->msgmap) > 0L){
	PAT_STATE  pstate;

	if(nonempty_patterns(ROLE_DO_ROLES, &pstate) && first_pattern(&pstate)){
	    action = radio_buttons("Compose, Forward, Reply, or Bounce? ",
				   -FOOTER_ROWS(state), choose_action,
				   'c', 'x', h_role_compose, RB_NORM, NULL);
	}
	else{
	    q_status_message(SM_ORDER, 0, 3,
			 "No roles available. Use Setup/Rules to add roles.");
	    return;
	}
    }
    else
      action = 'c';

    if(action == 'c' || action == 'r' || action == 'f' || action == 'b'){
	ACTION_S *role = NULL;
	void    (*prev_screen)() = NULL, (*redraw)() = NULL;

	redraw = state->redrawer;
	state->redrawer = NULL;
	prev_screen = state->prev_screen;
	role = NULL;
	state->next_screen = SCREEN_FUN_NULL;

	/* Setup role */
	if(role_select_screen(state, &role,
			      action == 'f' ? MC_FORWARD :
			       action == 'r' ? MC_REPLY :
				action == 'c' ? MC_COMPOSE : 0) < 0){
	    cmd_cancelled(action == 'f' ? "Forward" :
			  action == 'r' ? "Reply" :
			   action == 'c' ? "Composition" : "Bounce");
	    state->next_screen = prev_screen;
	    state->redrawer = redraw;
	    state->mangled_screen = 1;
	}
	else{
	    /*
	     * If default role was selected (NULL) we need to make
	     * up a role which won't do anything, but will cause
	     * compose_mail to think there's already a role so that
	     * it won't try to confirm the default.
	     */
	    if(role)
	      role = combine_inherited_role(role);
	    else{
		role = (ACTION_S *) fs_get(sizeof(*role));
		memset((void *) role, 0, sizeof(*role));
		role->nick = cpystr("Default Role");
	    }

	    state->redrawer = NULL;
	    switch(action){
	      case 'c':
		compose_mail(NULL, NULL, role, NULL, NULL);
		break;

	      case 'r':
		reply(state, role);
		break;

	      case 'f':
		forward(state, role);
		break;

	      case 'b':
		bounce(state, role);
		break;
	    }

	    if(role)
	      free_action(&role);
	    
	    state->next_screen = prev_screen;
	    state->redrawer = redraw;
	    state->mangled_screen = 1;
	}
    }
}


/*----------------------------------------------------------------------
   Do the dirty work of prompting the user for a folder name

  Args: 
        nfldr should be a buffer at least MAILTMPLEN long
	dela -- a pointer to a SaveDel. If it is
	  DontAsk on input, don't offer Delete prompt
	  Del     on input, offer Delete command with default of Delete
	  NoDel                                                NoDelete
	  RetDel and RetNoDel are return values
        

 Result: 

 ----*/
int
save_prompt(state, cntxt, nfldr, len_nfldr, nmsgs, env, rawmsgno, section, dela)
    struct pine  *state;
    CONTEXT_S   **cntxt;
    char	 *nfldr;
    size_t        len_nfldr;
    char	 *nmsgs;
    ENVELOPE	 *env;
    long	  rawmsgno;
    char	 *section;
    SaveDel      *dela;
{
    static char	      folder[MAILTMPLEN+1] = {'\0'};
    static CONTEXT_S *last_context = NULL;
    int		      rc, n, flags, last_rc = 0, saveable_count = 0, done = 0;
    int               context_was_set, delindex;
    char	      prompt[MAX_SCREEN_COLS+1], *p, expanded[MAILTMPLEN];
    char              *buf = tmp_20k_buf;
    HelpType	      help;
    SaveDel           del = DontAsk;
    char             *deltext = NULL;
    CONTEXT_S	     *tc;
    ESCKEY_S	      ekey[9];

    if(!cntxt)
      panic("no context ptr in save_prompt");

    context_was_set = ((*cntxt) != NULL);

    /* start with the default save context */
    if(!(*cntxt)
       && ((*cntxt) = default_save_context(state->context_list)) == NULL)
       (*cntxt) = state->context_list;

    if(!env || ps_global->save_msg_rule == MSG_RULE_LAST
	  || ps_global->save_msg_rule == MSG_RULE_DEFLT){
	if(ps_global->save_msg_rule == MSG_RULE_LAST && last_context){
	    if(!context_was_set)
	      (*cntxt) = last_context;
	}
	else{
	    strncpy(folder,ps_global->VAR_DEFAULT_SAVE_FOLDER,sizeof(folder)-1);
	    folder[sizeof(folder)-1] = '\0';
	}
    }
    else{
	get_save_fldr_from_env(folder, sizeof(folder), env, state,
			       rawmsgno, section);
	/* somebody expunged current message */
	if(sp_expunge_count(ps_global->mail_stream))
	  return(0);
    }


    /* how many context's can be saved to... */
    for(tc = state->context_list; tc; tc = tc->next)
      if(!NEWS_TEST(tc))
        saveable_count++;

    /* set up extra command option keys */
    rc = 0;
    ekey[rc].ch      = ctrl('T');
    ekey[rc].rval    = 2;
    ekey[rc].name    = "^T";
    ekey[rc++].label = "To Fldrs";

    if(saveable_count > 1){
	ekey[rc].ch      = ctrl('P');
	ekey[rc].rval    = 10;
	ekey[rc].name    = "^P";
	ekey[rc++].label = "Prev Collection";

	ekey[rc].ch      = ctrl('N');
	ekey[rc].rval    = 11;
	ekey[rc].name    = "^N";
	ekey[rc++].label = "Next Collection";
    }

    if(F_ON(F_ENABLE_TAB_COMPLETE, ps_global)){
	ekey[rc].ch      = TAB;
	ekey[rc].rval    = 12;
	ekey[rc].name    = "TAB";
	ekey[rc++].label = "Complete";
    }

    if(F_ON(F_ENABLE_SUB_LISTS, ps_global)){
	ekey[rc].ch      = ctrl('X');
	ekey[rc].rval    = 14;
	ekey[rc].name    = "^X";
	ekey[rc++].label = "ListMatches";
    }

    if(dela && (*dela == NoDel || *dela == Del)){
	ekey[rc].ch      = ctrl('R');
	ekey[rc].rval    = 15;
	ekey[rc].name    = "^R";
	delindex = rc++;
	del = *dela;
    }

    if(saveable_count > 1){
	ekey[rc].ch      = KEY_UP;
	ekey[rc].rval    = 10;
	ekey[rc].name    = "";
	ekey[rc++].label = "";

	ekey[rc].ch      = KEY_DOWN;
	ekey[rc].rval    = 11;
	ekey[rc].name    = "";
	ekey[rc++].label = "";
    }

    ekey[rc].ch = -1;

    *nfldr = '\0';
    help = NO_HELP;
    while(!done){
	/* only show collection number if more than one available */
	if(ps_global->context_list->next)
	  sprintf(prompt, "SAVE%s %sto folder in <%.16s%s> [%s] : ",
		  deltext ? deltext : "",
		  nmsgs, (*cntxt)->nickname, 
		  strlen((*cntxt)->nickname) > 16 ? "..." : "",
		  strsquish(buf, folder, 25));
	else
	  sprintf(prompt, "SAVE%s %sto folder [%s] : ",
		  deltext ? deltext : "",
		  nmsgs, strsquish(buf, folder, 40));

	/*
	 * If the prompt won't fit, try removing deltext.
	 */
	if(state->ttyo->screen_cols < strlen(prompt) + 6 && deltext){
	    if(ps_global->context_list->next)
	      sprintf(prompt, "SAVE %sto folder in <%.16s%s> [%s] : ",
		      nmsgs, (*cntxt)->nickname, 
		      strlen((*cntxt)->nickname) > 16 ? "..." : "",
		      strsquish(buf, folder, 25));
	    else
	      sprintf(prompt, "SAVE %sto folder [%s] : ",
		      nmsgs, strsquish(buf, folder, 40));
	}

	/*
	 * If the prompt still won't fit, remove the extra info contained
	 * in nmsgs.
	 */
	if(state->ttyo->screen_cols < strlen(prompt) + 6 && *nmsgs){
	    if(ps_global->context_list->next)
	      sprintf(prompt, "SAVE to folder in <%.16s%s> [%s] : ",
		      (*cntxt)->nickname, 
		      strlen((*cntxt)->nickname) > 16 ? "..." : "",
		      strsquish(buf, folder, 25));
	    else
	      sprintf(prompt, "SAVE to folder [%s] : ", 
		      strsquish(buf, folder, 25));
	}
	
	if(del != DontAsk)
	  ekey[delindex].label = (del == NoDel) ? "Delete" : "No Delete";

	flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE;
	rc = optionally_enter(nfldr, -FOOTER_ROWS(state), 0, len_nfldr,
			      prompt, ekey, help, &flags);

	switch(rc){
	  case -1 :
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Error reading folder name");
	    done--;
	    break;

	  case 0 :
	    removing_trailing_white_space(nfldr);
	    removing_leading_white_space(nfldr);

	    if(*nfldr || *folder){
		char *p, *name, *fullname = NULL;
		int   exists, breakout = FALSE;

		if(!*nfldr){
		    strncpy(nfldr, folder, len_nfldr-1);
		    nfldr[len_nfldr-1] = '\0';
		}
		if(!(name = folder_is_nick(nfldr, FOLDERS(*cntxt), 0)))
		    name = nfldr;

		if(update_folder_spec(expanded, name)){
		    strncpy(name = nfldr, expanded, len_nfldr-1);
		    nfldr[len_nfldr-1] = '\0';
		}

		exists = folder_name_exists(*cntxt, name, &fullname);

		if(exists == FEX_ERROR){
		    q_status_message1(SM_ORDER, 0, 3,
				      "Problem accessing folder \"%.200s\"",
				      nfldr);
		    done--;
		}
		else{
		    if(fullname){
			strncpy(name = nfldr, fullname, len_nfldr-1);
			nfldr[len_nfldr-1] = '\0';
			fs_give((void **) &fullname);
			breakout = TRUE;
		    }

		    if(exists & FEX_ISFILE){
			done++;
		    }
		    else if((exists & FEX_ISDIR)){
			tc = *cntxt;
			if(breakout){
			    CONTEXT_S *fake_context;
			    char	   tmp[MAILTMPLEN];
			    size_t	   l;

			    strncpy(tmp, name, sizeof(tmp)-2);
			    tmp[sizeof(tmp)-2-1] = '\0';
			    if(tmp[(l = strlen(tmp)) - 1] != tc->dir->delim){
				tmp[l] = tc->dir->delim;
				strcpy(&tmp[l+1], "[]");
			    }
			    else
			      strcat(tmp, "[]");

			    fake_context = new_context(tmp, 0);
			    nfldr[0] = '\0';
			    done = display_folder_list(&fake_context, nfldr,
						       1, folders_for_save);
			    free_context(&fake_context);
			}
			else if(tc->dir->delim
				&& (p = strrindex(name, tc->dir->delim))
				&& *(p+1) == '\0')
			  done = display_folder_list(cntxt, nfldr,
						     1, folders_for_save);
		    }
		    else{			/* Doesn't exist, create! */
			if(fullname = folder_as_breakout(*cntxt, name)){
			    strncpy(name = nfldr, fullname, len_nfldr-1);
			    nfldr[len_nfldr-1] = '\0';
			    fs_give((void **) &fullname);
			}

			switch(create_for_save(NULL, *cntxt, name)){
			  case 1 :		/* success */
			    done++;
			    break;
			  case 0 :		/* error */
			  case -1 :		/* declined */
			    done--;
			    break;
			}
		    }
		}

		break;
	    }
	    /* else fall thru like they cancelled */

	  case 1 :
	    cmd_cancelled("Save message");
	    done--;
	    break;

	  case 2 :
	    if(display_folder_list(cntxt, nfldr, 0, folders_for_save))
	      done++;

	    break;

	  case 3 :
            help = (help == NO_HELP) ? h_oe_save : NO_HELP;
	    break;

	  case 4 :				/* redraw */
	    break;

	  case 10 :				/* previous collection */
	    for(tc = (*cntxt)->prev; tc; tc = tc->prev)
	      if(!NEWS_TEST(tc))
		break;

	    if(!tc){
		CONTEXT_S *tc2;

		for(tc2 = (tc = (*cntxt))->next; tc2; tc2 = tc2->next)
		  if(!NEWS_TEST(tc2))
		    tc = tc2;
	    }

	    *cntxt = tc;
	    break;

	  case 11 :				/* next collection */
	    tc = (*cntxt);

	    do
	      if(((*cntxt) = (*cntxt)->next) == NULL)
		(*cntxt) = ps_global->context_list;
	    while(NEWS_TEST(*cntxt) && (*cntxt) != tc);
	    break;

	  case 12 :				/* file name completion */
	    if(!folder_complete(*cntxt, nfldr, &n)){
		if(n && last_rc == 12 && !(flags & OE_USER_MODIFIED)){
		    if(display_folder_list(cntxt, nfldr, 1, folders_for_save))
		      done++;			/* bingo! */
		    else
		      rc = 0;			/* burn last_rc */
		}
		else
		  Writechar(BELL, 0);
	    }

	    break;

	  case 14 :				/* file name completion */
	    if(display_folder_list(cntxt, nfldr, 2, folders_for_save))
	      done++;			/* bingo! */
	    else
	      rc = 0;			/* burn last_rc */

	    break;

	  case 15 :			/* Delete / No Delete */
	    del = (del == NoDel) ? Del : NoDel;
	    deltext = (del == NoDel) ? " (no delete)" : " (and delete)";
	    break;

	  default :
	    panic("Unhandled case");
	    break;
	}

	last_rc = rc;
    }

    ps_global->mangled_footer = 1;

    if(done < 0)
      return(0);

    if(*cntxt)
      last_context = *cntxt;		/* remember for next time */

    if(*nfldr){
	strncpy(folder, nfldr, sizeof(folder)-1);
	folder[sizeof(folder)-1] = '\0';
    }
    else{
	strncpy(nfldr, folder, len_nfldr-1);
	nfldr[len_nfldr-1] = '\0';
    }

    /* nickname?  Copy real name to nfldr */
    if(*cntxt
       && context_isambig(nfldr)
       && (p = folder_is_nick(nfldr, FOLDERS(*cntxt), 0))){
	strncpy(nfldr, p, len_nfldr-1);
	nfldr[len_nfldr-1] = '\0';
    }

    if(dela && (*dela == NoDel || *dela == Del))
      *dela = (del == NoDel) ? RetNoDel : RetDel;

    return(1);
}


/*----------------------------------------------------------------------
   Grope through envelope to find default folder name to save to

  Args: fbuf     --  Buffer to return result in
        nfbuf    --  Size of fbuf
        e        --  The envelope to look in
        state    --  Usual pine state
        rawmsgno --  Raw c-client sequence number of message
	section  --  Mime section of header data (for message/rfc822)

 Result: The appropriate default folder name is copied into fbuf.
 ----*/
void
get_save_fldr_from_env(fbuf, nfbuf, e, state, rawmsgno, section)
    char         fbuf[];
    int          nfbuf;
    ENVELOPE    *e;
    struct pine *state;
    long	 rawmsgno;
    char	*section;
{
    char     fakedomain[2];
    ADDRESS *tmp_adr = NULL;
    char     buf[max(MAXFOLDER,MAX_NICKNAME) + 1];
    char    *bufp;
    char    *folder_name = NULL;
    static char botch[] = "programmer botch, unknown message save rule";
    unsigned save_msg_rule;

    if(!e)
      return;

    /* copy this because we might change it below */
    save_msg_rule = state->save_msg_rule;

    /* first get the relevant address to base the folder name on */
    switch(save_msg_rule){
      case MSG_RULE_FROM:
      case MSG_RULE_NICK_FROM:
      case MSG_RULE_NICK_FROM_DEF:
      case MSG_RULE_FCC_FROM:
      case MSG_RULE_FCC_FROM_DEF:
      case MSG_RULE_RN_FROM:
      case MSG_RULE_RN_FROM_DEF:
        tmp_adr = e->from ? copyaddr(e->from)
			  : e->sender ? copyaddr(e->sender) : NULL;
	break;

      case MSG_RULE_SENDER:
      case MSG_RULE_NICK_SENDER:
      case MSG_RULE_NICK_SENDER_DEF:
      case MSG_RULE_FCC_SENDER:
      case MSG_RULE_FCC_SENDER_DEF:
      case MSG_RULE_RN_SENDER:
      case MSG_RULE_RN_SENDER_DEF:
        tmp_adr = e->sender ? copyaddr(e->sender)
			    : e->from ? copyaddr(e->from) : NULL;
	break;

      case MSG_RULE_REPLYTO:
      case MSG_RULE_NICK_REPLYTO:
      case MSG_RULE_NICK_REPLYTO_DEF:
      case MSG_RULE_FCC_REPLYTO:
      case MSG_RULE_FCC_REPLYTO_DEF:
      case MSG_RULE_RN_REPLYTO:
      case MSG_RULE_RN_REPLYTO_DEF:
        tmp_adr = e->reply_to ? copyaddr(e->reply_to)
			  : e->from ? copyaddr(e->from)
			  : e->sender ? copyaddr(e->sender) : NULL;
	break;

      case MSG_RULE_RECIP:
      case MSG_RULE_NICK_RECIP:
      case MSG_RULE_NICK_RECIP_DEF:
      case MSG_RULE_FCC_RECIP:
      case MSG_RULE_FCC_RECIP_DEF:
      case MSG_RULE_RN_RECIP:
      case MSG_RULE_RN_RECIP_DEF:
	/* news */
	if(state->mail_stream && IS_NEWS(state->mail_stream)){
	    char *tmp_a_string, *ng_name;

	    fakedomain[0] = '@';
	    fakedomain[1] = '\0';

	    /* find the news group name */
	    if(ng_name = strstr(state->mail_stream->mailbox,"#news"))
	      ng_name += 6;
	    else
	      ng_name = state->mail_stream->mailbox; /* shouldn't happen */

	    /* copy this string so rfc822_parse_adrlist can't blast it */
	    tmp_a_string = cpystr(ng_name);
	    /* make an adr */
	    rfc822_parse_adrlist(&tmp_adr, tmp_a_string, fakedomain);
	    fs_give((void **)&tmp_a_string);
	    if(tmp_adr && tmp_adr->host && tmp_adr->host[0] == '@')
	      tmp_adr->host[0] = '\0';
	}
	/* not news */
	else{
	    static char *fields[] = {"Resent-To", NULL};
	    char *extras, *values[sizeof(fields)/sizeof(fields[0])];

	    extras = pine_fetchheader_lines(state->mail_stream, rawmsgno,
					    section, fields);
	    if(extras){
		long i;

		memset(values, 0, sizeof(fields));
		simple_header_parse(extras, fields, values);
		fs_give((void **)&extras);

		for(i = 0; i < sizeof(fields)/sizeof(fields[0]); i++)
		  if(values[i]){
		      if(tmp_adr)		/* take last matching value */
			mail_free_address(&tmp_adr);

		      /* build a temporary address list */
		      fakedomain[0] = '@';
		      fakedomain[1] = '\0';
		      rfc822_parse_adrlist(&tmp_adr, values[i], fakedomain);
		      fs_give((void **)&values[i]);
		  }
	    }

	    if(!tmp_adr)
	      tmp_adr = e->to ? copyaddr(e->to) : NULL;
	}

	break;
      
      default:
	panic(botch);
	break;
    }

    /* For that address, lookup the fcc or nickname from address book */
    switch(save_msg_rule){
      case MSG_RULE_NICK_FROM:
      case MSG_RULE_NICK_SENDER:
      case MSG_RULE_NICK_REPLYTO:
      case MSG_RULE_NICK_RECIP:
      case MSG_RULE_FCC_FROM:
      case MSG_RULE_FCC_SENDER:
      case MSG_RULE_FCC_REPLYTO:
      case MSG_RULE_FCC_RECIP:
      case MSG_RULE_NICK_FROM_DEF:
      case MSG_RULE_NICK_SENDER_DEF:
      case MSG_RULE_NICK_REPLYTO_DEF:
      case MSG_RULE_NICK_RECIP_DEF:
      case MSG_RULE_FCC_FROM_DEF:
      case MSG_RULE_FCC_SENDER_DEF:
      case MSG_RULE_FCC_REPLYTO_DEF:
      case MSG_RULE_FCC_RECIP_DEF:
	switch(save_msg_rule){
	  case MSG_RULE_NICK_FROM:
	  case MSG_RULE_NICK_SENDER:
	  case MSG_RULE_NICK_REPLYTO:
	  case MSG_RULE_NICK_RECIP:
	  case MSG_RULE_NICK_FROM_DEF:
	  case MSG_RULE_NICK_SENDER_DEF:
	  case MSG_RULE_NICK_REPLYTO_DEF:
	  case MSG_RULE_NICK_RECIP_DEF:
	    bufp = get_nickname_from_addr(tmp_adr, buf, sizeof(buf));
	    break;

	  case MSG_RULE_FCC_FROM:
	  case MSG_RULE_FCC_SENDER:
	  case MSG_RULE_FCC_REPLYTO:
	  case MSG_RULE_FCC_RECIP:
	  case MSG_RULE_FCC_FROM_DEF:
	  case MSG_RULE_FCC_SENDER_DEF:
	  case MSG_RULE_FCC_REPLYTO_DEF:
	  case MSG_RULE_FCC_RECIP_DEF:
	    bufp = get_fcc_from_addr(tmp_adr, buf, sizeof(buf));
	    break;
	}

	if(bufp && *bufp){
	    istrncpy(fbuf, bufp, nfbuf - 1);
	    fbuf[nfbuf - 1] = '\0';
	}
	else
	  /* fall back to non-nick/non-fcc version of rule */
	  switch(save_msg_rule){
	    case MSG_RULE_NICK_FROM:
	    case MSG_RULE_FCC_FROM:
	      save_msg_rule = MSG_RULE_FROM;
	      break;

	    case MSG_RULE_NICK_SENDER:
	    case MSG_RULE_FCC_SENDER:
	      save_msg_rule = MSG_RULE_SENDER;
	      break;

	    case MSG_RULE_NICK_REPLYTO:
	    case MSG_RULE_FCC_REPLYTO:
	      save_msg_rule = MSG_RULE_REPLYTO;
	      break;

	    case MSG_RULE_NICK_RECIP:
	    case MSG_RULE_FCC_RECIP:
	      save_msg_rule = MSG_RULE_RECIP;
	      break;
	    
	    default:
	      istrncpy(fbuf, ps_global->VAR_DEFAULT_SAVE_FOLDER, nfbuf - 1);
	      fbuf[nfbuf - 1] = '\0';
	      break;
	  }

	break;
    }

    /* Realname */
    switch(save_msg_rule){
      case MSG_RULE_RN_FROM_DEF:
      case MSG_RULE_RN_FROM:
      case MSG_RULE_RN_SENDER_DEF:
      case MSG_RULE_RN_SENDER:
      case MSG_RULE_RN_RECIP_DEF:
      case MSG_RULE_RN_RECIP:
      case MSG_RULE_RN_REPLYTO_DEF:
      case MSG_RULE_RN_REPLYTO:
        /* Fish out the realname */
	if(tmp_adr && tmp_adr->personal && tmp_adr->personal[0]){
	    char *dummy = NULL;

	    folder_name = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						 SIZEOF_20KBUF,
						 tmp_adr->personal, &dummy);
	    if(dummy)
	      fs_give((void **)&dummy);
	}

	if(folder_name && folder_name[0]){
	    istrncpy(fbuf, folder_name, nfbuf - 1);
	    fbuf[nfbuf - 1] = '\0';
	}
	else{	/* fall back to other behaviors */
	    switch(save_msg_rule){
	      case MSG_RULE_RN_FROM:
	        save_msg_rule = MSG_RULE_FROM;
		break;

	      case MSG_RULE_RN_SENDER:
	        save_msg_rule = MSG_RULE_SENDER;
		break;

	      case MSG_RULE_RN_RECIP:
	        save_msg_rule = MSG_RULE_RECIP;
		break;

	      case MSG_RULE_RN_REPLYTO:
	        save_msg_rule = MSG_RULE_REPLYTO;
		break;

	      default:
		istrncpy(fbuf, ps_global->VAR_DEFAULT_SAVE_FOLDER, nfbuf - 1);
		fbuf[nfbuf - 1] = '\0';
		break;
	    }
	}

	break;
    }

    /* get the username out of the mailbox for this address */
    switch(save_msg_rule){
      case MSG_RULE_FROM:
      case MSG_RULE_SENDER:
      case MSG_RULE_REPLYTO:
      case MSG_RULE_RECIP:
	/*
	 * Fish out the user's name from the mailbox portion of
	 * the address and put it in folder.
	 */
	folder_name = (tmp_adr && tmp_adr->mailbox && tmp_adr->mailbox[0])
		      ? tmp_adr->mailbox : NULL;
	if(!get_uname(folder_name, fbuf, nfbuf)){
	    istrncpy(fbuf, ps_global->VAR_DEFAULT_SAVE_FOLDER, nfbuf - 1);
	    fbuf[nfbuf - 1] = '\0';
	}

	break;
    }

    if(tmp_adr)
      mail_free_address(&tmp_adr);
}



/*----------------------------------------------------------------------
        Do the work of actually saving messages to a folder

    Args: state -- pine state struct (for stream pointers)
	  stream  -- source stream, which msgmap refers to
	  context -- context to interpret name in if not fully qualified
	  folder  -- The folder to save the message in
          msgmap -- message map of currently selected messages
	  flgs -- Possible bits are
		    SV_DELETE   - delete after saving
		    SV_FOR_FILT - called from filtering function, not save
		    SV_FIX_DELS - remove Del mark before saving

  Result: Returns number of messages saved

  Note: There's a bit going on here; temporary clearing of deleted flags
	since they are *not* preserved, picking or creating the stream for
	copy or append, and dealing with errors...
	We try to preserve user keywords by setting them in the destination.
 ----*/
long
save(state, stream, context, folder, msgmap, flgs)
    struct pine  *state;
    MAILSTREAM	 *stream;
    CONTEXT_S    *context;
    char         *folder;
    MSGNO_S	 *msgmap;
    int		  flgs;
{
    int		  rv, rc, j, our_stream = 0, cancelled = 0;
    int           delete, filter, k, preserve_keywords;
    char	 *save_folder, *seq, flags[64], date[64], tmp[MAILTMPLEN];
    long	  i, nmsgs, rawno;
    STORE_S	 *so = NULL;
    MAILSTREAM	 *save_stream = NULL, *dstn_stream = NULL;
    MESSAGECACHE *mc, *mcdst;
#if	defined(DOS) && !defined(WIN32)
#define	SAVE_TMP_TYPE		TmpFileStar
#else
#define	SAVE_TMP_TYPE		CharStar
#endif

    delete = flgs & SV_DELETE;
    filter = flgs & SV_FOR_FILT;

    if(strucmp(folder, state->inbox_name) == 0){
	save_folder = state->VAR_INBOX_PATH;
	context = NULL;
    }
    else
      save_folder = folder;

    /*
     * If any of the messages have exceptional attachment handling
     * we have to fall thru below to do the APPEND by hand...
     */
    if(!msgno_any_deletedparts(stream, msgmap)){
	/*
	 * Compare the current stream (the save's source) and the stream
	 * the destination folder will need...
	 */
	context_apply(tmp, context, save_folder, sizeof(tmp));
        save_stream = (stream && stream->dtb
		       && stream->dtb->flags & DR_LOCAL) && !IS_REMOTE(tmp) ?
	  stream : context_same_stream(context, save_folder, stream);
    }

    /* if needed, this'll get set in mm_notify */
    ps_global->try_to_create = 0;
    rv = rc = 0;
    nmsgs = 0L;

    /*
     * At this point, if we have a save_stream, then none of the messages
     * being saved involve special handling that would require our use
     * of mail_append, so go with mail_copy since in the IMAP case it
     * means no data on the wire...
     */
    if(save_stream){
	char *dseq = NULL, *oseq;

	if((flgs & SV_FIX_DELS) &&
	   (dseq = currentf_sequence(stream, msgmap, F_DEL, NULL,
				     0, NULL, NULL)))
	  mail_flag(stream, dseq, "\\DELETED", 0L);

	seq = currentf_sequence(stream, msgmap, 0L, &nmsgs, 0, NULL, NULL);
	if(F_ON(F_AGG_SEQ_COPY, ps_global)
	   || (mn_get_sort(msgmap) == SortArrival && !mn_get_revsort(msgmap))){
	    
	    /*
	     * currentf_sequence() above lit all the elt "sequence"
	     * bits of the interesting messages.  Now, build a sequence
	     * that preserves sort order...
	     */
	    oseq = build_sequence(stream, msgmap, &nmsgs);
	}
	else{
	    oseq  = NULL;			/* no single sequence! */
	    nmsgs = 0L;
	    i = mn_first_cur(msgmap);		/* set first to copy */
	}

	do{
	    while(!(rv = (int) context_copy(context, save_stream,
				oseq ? oseq : long2string(mn_m2raw(msgmap, i)),
				save_folder))){
		if(rc++ || !ps_global->try_to_create)   /* abysmal failure! */
		  break;			/* c-client returned error? */

		if((context && context->use & CNTXT_INCMNG)
		   && context_isambig(save_folder)){
		    q_status_message(SM_ORDER, 3, 5,
		   "Can only save to existing folders in Incoming Collection");
		    break;
		}

		ps_global->try_to_create = 0;	/* reset for next time */
		if((j = create_for_save(save_stream,context,save_folder)) < 1){
		    if(j < 0)
		      cancelled = 1;		/* user cancels */

		    break;
		}
	    }

	    if(rv){				/* failure or finished? */
		if(oseq)			/* all done? */
		  break;
		else
		  nmsgs++;
	    }
	    else{				/* failure! */
		if(oseq)
		  nmsgs = 0L;			/* nothing copy'd */

		break;
	    }
	}
	while((i = mn_next_cur(msgmap)) > 0L);

	if(rv && delete)			/* delete those saved */
	  mail_flag(stream, seq, "\\DELETED", ST_SET);
	else if(dseq)				/* or restore previous state */
	  mail_flag(stream, dseq, "\\DELETED", ST_SET);

	if(dseq)				/* clean up */
	  fs_give((void **)&dseq);

	if(oseq)
	  fs_give((void **)&oseq);

	fs_give((void **)&seq);
    }
    else{
	/*
	 * Special handling requires mail_append.  See if we can
	 * re-use stream source messages are on...
	 */
	save_stream = context_same_stream(context, save_folder, stream);

	/*
	 * IF the destination's REMOTE, open a stream here so c-client
	 * doesn't have to open it for each aggregate save...
	 */
	if(!save_stream){
	    if(context_apply(tmp, context, save_folder, sizeof(tmp))[0] == '{'
	       && (save_stream = context_open(context, NULL,
					      save_folder,
				      OP_HALFOPEN | SP_USEPOOL | SP_TEMPUSE,
					      NULL)))
	      our_stream = 1;
	}

	/*
	 * Allocate a storage object to temporarily store the message
	 * object in.  Below it'll get mapped into a c-client STRING struct
	 * in preparation for handing off to context_append...
	 */
	if(!(so = so_get(SAVE_TMP_TYPE, NULL, WRITE_ACCESS))){
	    dprint(1, (debugfile, "Can't allocate store for save: %s\n",
		       error_description(errno)));
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Problem creating space for message text.");
	}

	/*
	 * get a sequence of invalid elt's so we can get their flags...
	 */
	if(seq = invalid_elt_sequence(stream, msgmap)){
	    mail_fetch_fast(stream, seq, 0L);
	    fs_give((void **) &seq);
	}

	/*
	 * If we're supposed set the deleted flag, clear the elt bit
	 * we'll use to build the sequence later...
	 */
	if(delete)
	  for(i = 1L; i <= stream->nmsgs; i++)
	    if((mc = mail_elt(stream, i)) != NULL)
	      mc->sequence = 0;

	nmsgs = 0L;

        /*
         * It may seem like we ought to just use the optional flags
	 * argument to APPEND to set the user keywords, and indeed that
	 * sometimes works. However there are some IMAP servers which
	 * don't implement the optional keywords arguments to APPEND correctly.
         * Imapd is one of these. They may fail to set the keywords in the
	 * appended message. It may or may not result in an APPEND failure
	 * so we can't even count on that. We have to just open up a
	 * stream to the destination and set them if there are any to
	 * be set.
	 */

	/* 
	 * if there is more than one message, do multiappend.
	 * otherwise, we can use our already open stream.
	 */
	if(!save_stream || !is_imap_stream(save_stream) ||
	   (LEVELMULTIAPPEND(save_stream) && mn_total_cur(msgmap) > 1)){
	    APPENDPACKAGE pkg;
	    STRING msg;

	    pkg.stream = stream;
	    pkg.flags = flags;
	    pkg.date = date;
	    pkg.msg = &msg;
	    pkg.msgmap = msgmap;

	    if ((pkg.so = so) && ((pkg.msgno = mn_first_cur(msgmap)) > 0L)) {
	        so_truncate(so, 0L);

		/* 
		 * we've gotta make sure this is a stream that we've
		 * opened ourselves.
		 */
		rc = 0;
		while(!(rv = context_append_multiple(context, 
						     our_stream ? save_stream
						     : NULL, save_folder,
						     save_fetch_append_cb,
						     (void *) &pkg,
						     stream))) {

		  if(rc++ || !ps_global->try_to_create)
		    break;
		  if((context && context->use & CNTXT_INCMNG)
		     && context_isambig(save_folder)){
		    q_status_message(SM_ORDER, 3, 5,
		   "Can only save to existing folders in Incoming Collection");
		    break;
		  }

		  ps_global->try_to_create = 0;
		  if((j = create_for_save(our_stream ? save_stream : NULL, 
					  context, save_folder)) < 1){
		    if(j < 0)
		      cancelled = 1;
		    break;
		  }
		}

		ps_global->noshow_error = 0;

		if(rv){
		    /*
		     * Success!  Count it, and if it's not already deleted and 
		     * it's supposed to be, mark it to get deleted later...
		     */
		  for(i = mn_first_cur(msgmap); so && i > 0L;
		      i = mn_next_cur(msgmap)){
		    nmsgs++;
		    if(delete){
		      rawno = mn_m2raw(msgmap, i);
		      mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
			    ? mail_elt(stream, rawno) : NULL;
		      if(mc && !mc->deleted)
			mc->sequence = 1;	/* mark for later deletion */
		    }
		  }
		}
	    }
	    else
	      cancelled = 1;		/* No messages to append! */

	    if(sp_expunge_count(stream))
	      cancelled = 1;		/* All bets are off! */
	}
	else 
	  for(i = mn_first_cur(msgmap); so && i > 0L; i = mn_next_cur(msgmap)){
	    so_truncate(so, 0L);

	    rawno = mn_m2raw(msgmap, i);
	    mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
		    ? mail_elt(stream, rawno) : NULL;

	    flag_string(mc, F_ANS|F_FLAG|F_SEEN, flags);
	      
	    if(mc && mc->day)
	      mail_date(date, mc);
	    else
	      *date = '\0';

	    rv = save_fetch_append(stream, mn_m2raw(msgmap, i),
				   NULL, save_stream, save_folder, context,
				   mc ? mc->rfc822_size : 0L, flags, date, so);

	    if(sp_expunge_count(stream))
	      rv = -1;			/* All bets are off! */

	    if(rv == 1){
		/*
		 * Success!  Count it, and if it's not already deleted and 
		 * it's supposed to be, mark it to get deleted later...
		 */
		nmsgs++;
		if(delete){
		    rawno = mn_m2raw(msgmap, i);
		    mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
			    ? mail_elt(stream, rawno) : NULL;
		    if(mc && !mc->deleted)
		      mc->sequence = 1;		/* mark for later deletion */
		}
	    }
	    else{
		if(rv == -1)
		  cancelled = 1;		/* else horrendous failure */

		break;
	    }
	}

	if(our_stream)
	  pine_mail_close(save_stream);

	if(so)
	  so_give(&so);

	if(delete && (seq = build_sequence(stream, NULL, NULL))){
	    mail_flag(stream, seq, "\\DELETED", ST_SET);
	    fs_give((void **)&seq);
	}
    }

    /* are any keywords set in our messages? */
    preserve_keywords = 0;
    for(i = mn_first_cur(msgmap); !preserve_keywords && i > 0L;
	i = mn_next_cur(msgmap)){
	rawno = mn_m2raw(msgmap, i);
	mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
		? mail_elt(stream, rawno) : NULL;
	if(mc && mc->user_flags)
	  preserve_keywords++;
    }

    if(preserve_keywords){
	MAILSTREAM *dstn_stream = NULL;
	int         already_open = 0;
	int         we_blocked_reuse = 0;

	if(sp_expunge_count(stream)){
	    q_status_message(SM_ORDER, 3, 5,
			   "Possible trouble setting keywords in destination");
	    goto get_out;
	}

	/*
	 * Possible problem created by our stream re-use
	 * strategy. If we are going to open a new stream
	 * here, we want to be sure not to re-use the
	 * stream we are saving _from_, so take it out of the
	 * re-use pool before we call open.
	 */
	if(sp_flagged(stream, SP_USEPOOL)){
	    we_blocked_reuse++;
	    sp_unflag(stream, SP_USEPOOL);
	}

	/* see if there is a stream open already */
	if(!dstn_stream){
	    dstn_stream = context_already_open_stream(context,
						      save_folder,
						      AOS_RW_ONLY);
	    already_open = dstn_stream ? 1 : 0;
	}

	if(!dstn_stream)
	  dstn_stream = context_open(context, NULL,
				     save_folder,
				     SP_USEPOOL | SP_TEMPUSE,
				     NULL);

	if(dstn_stream){
	    /* make sure dstn_stream knows about the new messages */
	    (void) pine_mail_ping(dstn_stream);

	    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){
		ENVELOPE *env;

		rawno = mn_m2raw(msgmap, i);
		mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
			? mail_elt(stream, rawno) : NULL;
		env = NULL;
		if(mc && mc->user_flags)
		  env = pine_mail_fetchstructure(stream, rawno, NULL);

		if(env && env->message_id && env->message_id[0])
		  set_keywords_in_msgid_msg(stream, mc, dstn_stream,
					    env->message_id,
					    mn_total_cur(msgmap)+10L);
	    }

	    if(!already_open)
	      pine_mail_close(dstn_stream);
	}

	if(we_blocked_reuse)
	  sp_set_flags(stream, sp_flags(stream) | SP_USEPOOL);
    }

get_out:

    ps_global->try_to_create = 0;		/* reset for next time */
    if(!cancelled && nmsgs < mn_total_cur(msgmap)){
	dprint(1, (debugfile, "FAILED save of msg %s (c-client sequence #)\n",
		   long2string(mn_m2raw(msgmap, mn_get_cur(msgmap)))));
	if((mn_total_cur(msgmap) > 1L) && nmsgs != 0){
	  /* this shouldn't happen cause it should be all or nothing */
	    sprintf(tmp_20k_buf,
		    "%ld of %ld messages saved before error occurred",
		    nmsgs, mn_total_cur(msgmap));
	    dprint(1, (debugfile, "\t%s\n", tmp_20k_buf));
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else if(mn_total_cur(msgmap) == 1){
	  sprintf(tmp_20k_buf,
		  "%s to folder \"%s\" FAILED",
		  filter ? "Filter" : "Save", 
		  strsquish(tmp_20k_buf+500, folder, 35));
	  dprint(1, (debugfile, "\t%s\n", tmp_20k_buf));
	  q_status_message(SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	}
	else{
	  sprintf(tmp_20k_buf,
		  "%s of %s messages to folder \"%s\" FAILED",
		  filter ? "Filter" : "Save", comatose(mn_total_cur(msgmap)),
		  strsquish(tmp_20k_buf+500, folder, 35));
	  dprint(1, (debugfile, "\t%s\n", tmp_20k_buf));
	  q_status_message(SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	}
    }

    return(nmsgs);
}

/* Append message callback
 * Accepts: MAIL stream
 *	    append data package
 *	    pointer to return initial flags
 *	    pointer to return message internal date
 *	    pointer to return stringstruct of message or NIL to stop
 * Returns: T if success (have message or stop), NIL if error
 */

long save_fetch_append_cb (stream, data, flags, date, message)
    MAILSTREAM *stream;
    void       *data;
    char      **flags;
    char      **date;
    STRING    **message;
{
    unsigned long size = 0;
    APPENDPACKAGE *pkg = (APPENDPACKAGE *) data;
    MESSAGECACHE *mc;
    char *fetch;
    int rc;
    unsigned long raw, hlen, tlen, mlen;

    if(pkg->so && (pkg->msgno > 0L)) {
	raw = mn_m2raw(pkg->msgmap, pkg->msgno);
	mc = (raw > 0L && pkg->stream && raw <= pkg->stream->nmsgs)
		? mail_elt(pkg->stream, raw) : NULL;
	if(mc){
	    size = mc->rfc822_size;
	    flag_string(mc, F_ANS|F_FLAG|F_SEEN, pkg->flags);
	}

	if(mc && mc->day)
	  mail_date(pkg->date, mc);
	else
	  *pkg->date = '\0';
	if(fetch = mail_fetch_header(pkg->stream, raw, NULL, NULL, &hlen,
				     FT_PEEK)){
	    if(!*pkg->date)
	      saved_date(pkg->date, fetch);
	}
	else
	  return(0);			/* fetchtext writes error */

	rc = MSG_EX_DELETE;		/* "rc" overloaded */
	if(msgno_exceptions(pkg->stream, raw, NULL, &rc, 0)){
	    char  section[64];
	    int	 failure = 0;
	    BODY *body;
	    gf_io_t  pc;

	    size = 0;			/* all bets off, abort sanity test  */
	    gf_set_so_writec(&pc, pkg->so);

	    section[0] = '\0';
	    if(!pine_mail_fetch_structure(pkg->stream, raw, &body, 0))
	      return(0);
	    
	    if(msgno_part_deleted(pkg->stream, raw, "")){
	       tlen = 0;
	       failure = !save_ex_replace_body(fetch, &hlen, body, pc);
	     }
	    else
	      failure = !(so_nputs(pkg->so, fetch, (long) hlen)
			  && save_ex_output_body(pkg->stream, raw, section,
						 body, &tlen, pc));

	    gf_clear_so_writec(pkg->so);

	    if(failure)
	      return(0);

	    q_status_message(SM_ORDER, 3, 3,
			     "NOTE: Deleted message parts NOT included in saved copy!");

	}
	else{
	    if(!so_nputs(pkg->so, fetch, (long) hlen))
	      return(0);

#if	defined(DOS) && !defined(WIN32)
	    mail_parameters(pkg->stream, SET_GETS, (void *)dos_gets);
	    append_file = (FILE *) so_text(pkg->so);
	    mail_gc(pkg->stream, GC_TEXTS);
#endif

	    fetch = pine_mail_fetch_text(pkg->stream, raw, NULL, &tlen, FT_PEEK);

#if	!(defined(DOS) && !defined(WIN32))
	    if(!(fetch && so_nputs(pkg->so, fetch, tlen)))
	      return(0);
#else
	    append_file = NULL;
	    mail_parameters(pkg->stream, SET_GETS, (void *)NULL);
	    mail_gc(pkg->stream, GC_TEXTS);
	    
	    if(!fetch)
	      return(0);
#endif
	}

	so_seek(pkg->so, 0L, 0);	/* rewind just in case */

#if	defined(DOS) && !defined(WIN32)
	d.fd  = fileno((FILE *)so_text(pkg->so));
	d.pos = 0L;
	mlen = filelength(d.fd);
#else
	mlen = hlen + tlen;
#endif

	if(size && mlen < size){
	    char buf[128];

	    if(sp_dead_stream(pkg->stream))
	      sprintf(buf, "Cannot save because current folder is Closed");
	    else
	      sprintf(buf, "Message to save shrank!  (#%ld: %ld --> %ld)",
		      raw, size, mlen);

	    q_status_message(SM_ORDER | SM_DING, 3, 4, buf);
	    dprint(1, (debugfile, "BOTCH: %s\n", buf));
	    return(0);
	}

#if	defined(DOS) && !defined(WIN32)
	INIT(pkg->msg, dawz_string, (void *)&d, mlen);
#else
	INIT(pkg->msg, mail_string, (void *)so_text(pkg->so), mlen);
#endif
      *message = pkg->msg;
					/* Next message */
      pkg->msgno = mn_next_cur(pkg->msgmap);
  }
  else					/* No more messages */
    *message = NIL;

  *flags = pkg->flags;
  *date = (pkg->date && *pkg->date) ? pkg->date : NIL;
  return LONGT;				/* Return success */
}

/*----------------------------------------------------------------------
   FETCH an rfc822 message header and body and APPEND to destination folder

  Args: 
        

 Result: 

 ----*/
int
save_fetch_append(stream, raw, sect, save_stream, save_folder,
		  context, size, flags, date, so)
    MAILSTREAM	  *stream;
    long	   raw;
    char	  *sect;
    MAILSTREAM	  *save_stream;
    char	  *save_folder;
    CONTEXT_S	  *context;
    unsigned long  size;
    char	  *flags, *date;
    STORE_S	  *so;
{
    int		   rc, rv, old_imap_server = 0;
    long	   j;
    char	  *fetch;
    unsigned long  hlen, tlen, mlen;
    STRING	   msg;
#if	defined(DOS) && !defined(WIN32)
    struct {					/* hack! stolen from dawz.c */
	int fd;
	unsigned long pos;
    } d;
    extern STRINGDRIVER dawz_string;
#endif

    if(fetch = mail_fetch_header(stream, raw, sect, NULL, &hlen, FT_PEEK)){
	/*
	 * If there's no date string, then caller found the
	 * MESSAGECACHE for this message element didn't already have it.
	 * So, parse the "internal date" by hand since fetchstructure
	 * hasn't been called yet for this particular message, and
	 * we don't want to call it now just to get the date since
	 * the full header has what we want.  Likewise, don't even
	 * think about calling mail_fetchfast either since it also
	 * wants to load mc->rfc822_size (which we could actually
	 * use but...), which under some drivers is *very*
	 * expensive to acquire (can you say NNTP?)...
	 */
	if(!*date)
	  saved_date(date, fetch);
    }
    else
      return(0);			/* fetchtext writes error */

    rc = MSG_EX_DELETE;			/* "rc" overloaded */
    if(msgno_exceptions(stream, raw, NULL, &rc, 0)){
	char	 section[64];
	int	 failure = 0;
	BODY	*body;
	gf_io_t  pc;

	size = 0;			/* all bets off, abort sanity test  */
	gf_set_so_writec(&pc, so);

	if(sect && *sect){
	    sprintf(section, "%s.1", sect);
	    if(!(body = mail_body(stream, raw, (unsigned char *) section)))
	      return(0);
	}
	else{
	    section[0] = '\0';
	    if(!pine_mail_fetch_structure(stream, raw, &body, 0))
	      return(0);
	}

	/*
	 * Walk the MIME structure looking for exceptional segments,
	 * writing them in the requested fashion.
	 *
	 * First, though, check for the easy case...
	 */
	if(msgno_part_deleted(stream, raw, sect ? sect : "")){
	    tlen = 0;
	    failure = !save_ex_replace_body(fetch, &hlen, body, pc);
	}
	else{
	    /*
	     * Otherwise, roll up your sleeves and get to work...
	     * start by writing msg header and then the processed body
	     */
	    failure = !(so_nputs(so, fetch, (long) hlen)
			&& save_ex_output_body(stream, raw, section,
					       body, &tlen, pc));
	}

	gf_clear_so_writec(so);

	if(failure)
	  return(0);

	q_status_message(SM_ORDER, 3, 3,
		    "NOTE: Deleted message parts NOT included in saved copy!");

    }
    else{
	/* First, write the header we just fetched... */
	if(!so_nputs(so, fetch, (long) hlen))
	  return(0);

#if	defined(DOS) && !defined(WIN32)
	/*
	 * Set append file and install dos_gets so message text
	 * is fetched directly to disk.
	 */
	mail_parameters(stream, SET_GETS, (void *)dos_gets);
	append_file = (FILE *) so_text(so);
	mail_gc(stream, GC_TEXTS);
#endif

	old_imap_server = is_imap_stream(stream) && !modern_imap_stream(stream);

	/* Second, go fetch the corresponding text... */
	fetch = pine_mail_fetch_text(stream, raw, sect, &tlen,
				     !old_imap_server ? FT_PEEK : 0);

	/*
	 * Special handling in case we're fetching a Message/rfc822
	 * segment and we're talking to an old server...
	 */
	if(fetch && *fetch == '\0' && sect && (hlen + tlen) != size){
	    so_seek(so, 0L, 0);
	    fetch = pine_mail_fetch_body(stream, raw, sect, &tlen, 0L);
	}

	/*
	 * Pre IMAP4 servers can't do a non-peeking mail_fetch_text,
	 * so if the message we are saving from was originally unseen,
	 * we have to change it back to unseen. Flags contains the
	 * string "SEEN" if the original message was seen.
	 */
	if(old_imap_server && (!flags || !srchstr(flags,"SEEN"))){
	    char seq[20];

	    strcpy(seq, long2string(raw));
	    mail_flag(stream, seq, "\\SEEN", 0);
	}


#if	!(defined(DOS) && !defined(WIN32))
	/* If fetch succeeded, write the result */
	if(!(fetch && so_nputs(so, fetch, tlen)))
	   return(0);
#else
	/*
	 * Clean up after our DOS hacks...
	 */
	append_file = NULL;
	mail_parameters(stream, SET_GETS, (void *)NULL);
	mail_gc(stream, GC_TEXTS);

	if(!fetch)
	  return(0);
#endif
    }

    so_seek(so, 0L, 0);			/* rewind just in case */

    /*
     * Set up a c-client string driver so we can hand the
     * collected text down to mail_append.
     *
     * NOTE: We only test the size if and only if we already
     *	     have it.  See, in some drivers, especially under
     *	     dos, its too expensive to get the size (full
     *	     header and body text fetch plus MIME parse), so
     *	     we only verify the size if we already know it.
     */
#if	defined(DOS) && !defined(WIN32)
    d.fd  = fileno((FILE *)so_text(so));
    d.pos = 0L;
    mlen = filelength(d.fd);
#else
    mlen = hlen + tlen;
#endif

    if(size && mlen < size){
	char buf[128];

	if(sp_dead_stream(stream))
	  sprintf(buf, "Cannot save because current folder is Closed");
	else
	  sprintf(buf, "Message to save shrank!  (#%ld: %ld --> %ld)",
		  raw, size, mlen);

	q_status_message(SM_ORDER | SM_DING, 3, 4, buf);
	dprint(1, (debugfile, "BOTCH: %s\n", buf));
	return(0);
    }

#if	defined(DOS) && !defined(WIN32)
    INIT(&msg, dawz_string, (void *)&d, mlen);
#else
    INIT(&msg, mail_string, (void *)so_text(so), mlen);
#endif

    rc = 0;
    while(!(rv = (int) context_append_full(context, save_stream,
					   save_folder, flags,
					   *date ? date : NULL,
					   &msg))){
	if(rc++ || !ps_global->try_to_create)	/* abysmal failure! */
	  break;				/* c-client returned error? */

	if(context && (context->use & CNTXT_INCMNG)
	   && context_isambig(save_folder)){
	    q_status_message(SM_ORDER, 3, 5,
	       "Can only save to existing folders in Incoming Collection");
	    break;
	}

	ps_global->try_to_create = 0;		/* reset for next time */
	if((j = create_for_save(save_stream,context,save_folder)) < 1){
	    if(j < 0)
	      rv = -1;			/* user cancelled */

	    break;
	}

	SETPOS((&msg), 0L);			/* reset string driver */
    }

    return(rv);
}


/*
 * save_ex_replace_body -
 *
 * NOTE : hlen points to a cell that has the byte count of "hdr" on entry
 *	  *BUT* which is to contain the count of written bytes on exit
 */
int
save_ex_replace_body(hdr, hlen, body, pc)
    char	   *hdr;
    unsigned long  *hlen;
    BODY	   *body;
    gf_io_t	    pc;
{
    unsigned long len;

    /*
     * "X-" out the given MIME headers unless they're
     * the same as we're going to substitute...
     */
    if(body->type == TYPETEXT
       && (!body->subtype || !strucmp(body->subtype, "plain"))
       && body->encoding == ENC7BIT){
	if(!gf_nputs(hdr, *hlen, pc))	/* write out header */
	  return(0);
    }
    else{
	int bol = 1;

	/*
	 * write header, "X-"ing out transport headers bothersome to
	 * software but potentially useful to the human recipient...
	 */
	for(len = *hlen; len; len--){
	    if(bol){
		unsigned long n;

		bol = 0;
		if(save_ex_mask_types(hdr, &n, pc))
		  *hlen += n;		/* add what we inserted */
		else
		  break;
	    }

	    if(*hdr == '\015' && *(hdr+1) == '\012'){
		bol++;			/* remember beginning of line */
		len--;			/* account for LF */
		if(gf_nputs(hdr, 2, pc))
		  hdr += 2;
		else
		  break;
	    }
	    else if(!(*pc)(*hdr++))
	      break;
	}

	if(len)				/* bytes remain! */
	  return(0);
    }

    /* Now, blat out explanatory text as the body... */
    if(save_ex_explain_body(body, &len, pc)){
	*hlen += len;
	return(1);
    }
    else
      return(0);
}



int
save_ex_output_body(stream, raw, section, body, len, pc)
    MAILSTREAM	  *stream;
    long	   raw;
    char	  *section;
    BODY	  *body;
    unsigned long *len;
    gf_io_t	   pc;
{
    char	  *txtp, newsect[128];
    unsigned long  ilen;

    txtp = mail_fetch_mime(stream, raw, section, len, FT_PEEK);

    if(msgno_part_deleted(stream, raw, section))
      return(save_ex_replace_body(txtp, len, body, pc));

    if(body->type == TYPEMULTIPART){
	char	  *subsect, boundary[128];
	int	   n, blen;
	PART	  *part = body->nested.part;
	PARAMETER *param;

	/* Locate supplied multipart boundary */
	for (param = body->parameter; param; param = param->next)
	  if (!strucmp(param->attribute, "boundary")){
	      sprintf(boundary, "--%.*s\015\012", sizeof(boundary)-10,
		      param->value);
	      blen = strlen(boundary);
	      break;
	  }

	if(!param){
	    q_status_message(SM_ORDER|SM_DING, 3, 3, "Missing MIME boundary");
	    return(0);
	}

	/*
	 * BUG: if multi/digest and a message deleted (which we'll
	 * change to text/plain), we need to coerce this composite
	 * type to multi/mixed !!
	 */
	if(!gf_nputs(txtp, *len, pc))		/* write MIME header */
	  return(0);

	/* Prepare to specify sub-sections */
	strncpy(newsect, section, sizeof(newsect));
	newsect[sizeof(newsect)-1] = '\0';
	subsect = &newsect[n = strlen(newsect)];
	if(n > 2 && !strcmp(&newsect[n-2], ".0"))
	  subsect--;
	else if(n)
	  *subsect++ = '.';

	n = 1;
	do {				/* for each part */
	    strcpy(subsect, int2string(n++));
	    if(gf_puts(boundary, pc)
		 && save_ex_output_body(stream, raw, newsect,
					&part->body, &ilen, pc))
	      *len += blen + ilen;
	    else
	      return(0);
	}
	while (part = part->next);	/* until done */

	sprintf(boundary, "--%.*s--\015\012", sizeof(boundary)-10,param->value);
	*len += blen + 2;
	return(gf_puts(boundary, pc));
    }

    /* Start by writing the part's MIME header */
    if(!gf_nputs(txtp, *len, pc))
      return(0);
    
    if(body->type == TYPEMESSAGE
       && (!body->subtype || !strucmp(body->subtype, "rfc822"))){
	/* write RFC 822 message's header */
	if((txtp = mail_fetch_header(stream,raw,section,NULL,&ilen,FT_PEEK))
	     && gf_nputs(txtp, ilen, pc))
	  *len += ilen;
	else
	  return(0);

	/* then go deal with its body parts */
	sprintf(newsect, "%.10s%s%s", section, section ? "." : "",
		(body->nested.msg->body->type == TYPEMULTIPART) ? "0" : "1");
	if(save_ex_output_body(stream, raw, newsect,
			       body->nested.msg->body, &ilen, pc)){
	    *len += ilen;
	    return(1);
	}

	return(0);
    }

    /* Write corresponding body part */
    if((txtp = pine_mail_fetch_body(stream, raw, section, &ilen, FT_PEEK))
       && gf_nputs(txtp, (long) ilen, pc) && gf_puts("\015\012", pc)){
	*len += ilen + 2;
	return(1);
    }

    return(0);
}



/*----------------------------------------------------------------------
    Mask off any header entries we don't want to show up in exceptional saves

Args:  hdr -- pointer to start of a header line
       pc -- function to write the prefix

  ----*/
int
save_ex_mask_types(hdr, len, pc)
    char	  *hdr;
    unsigned long *len;
    gf_io_t	   pc;
{
    char *s = NULL;

    if(!struncmp(hdr, "content-type:", 13))
      s = "Content-Type: Text/Plain; charset=US-ASCII\015\012X-";
    else if(!struncmp(hdr, "content-description:", 20))
      s = "Content-Description: Deleted Attachment\015\012X-";
    else if(!struncmp(hdr, "content-transfer-encoding:", 26)
	    || !struncmp(hdr, "content-disposition:", 20))
      s = "X-";

    return((*len = s ? strlen(s) : 0) ? gf_puts(s, pc) : 1);
}


int
save_ex_explain_body(body, len, pc)
    BODY	  *body;
    unsigned long *len;
    gf_io_t	   pc;
{
    unsigned long   ilen;
    char	  **blurbp;
    static char    *blurb[] = {
	"The following attachment was DELETED when this message was saved:",
	NULL
    };

    *len = 0;
    for(blurbp = blurb; *blurbp; blurbp++)
      if(save_ex_output_line(*blurbp, &ilen, pc))
	*len += ilen;
      else
	return(0);

    if(!save_ex_explain_parts(body, 0, &ilen, pc))
      return(0);

    *len += ilen;
    return(1);
}


int
save_ex_explain_parts(body, depth, len, pc)
    BODY	  *body;
    int		   depth;
    unsigned long *len;
    gf_io_t	   pc;
{
    char	  tmp[MAILTMPLEN], buftmp[MAILTMPLEN];
    unsigned long ilen;
    char *name = rfc2231_get_param(body->parameter, "name", NULL, NULL);

    if(body->type == TYPEMULTIPART) {   /* multipart gets special handling */
	PART *part = body->nested.part;	/* first body part */

	*len = 0;
	if(body->description && *body->description){
	    sprintf(tmp, "%*.*sA %s/%.*s%.10s%.100s%.10s segment described",
		    depth, depth, " ", body_type_names(body->type),
		    sizeof(tmp)-300, body->subtype ? body->subtype : "Unknown",
		    name ? " (Name=\"" : "",
		    name ? name : "",
		    name ? "\")" : "");
	    if(!save_ex_output_line(tmp, len, pc))
	      return(0);

	    sprintf(buftmp, "%.75s", body->description);
	    sprintf(tmp, "%*.*s  as \"%.50s\" containing:", depth, depth, " ",
		    (char *) rfc1522_decode((unsigned char *)tmp_20k_buf,
					    SIZEOF_20KBUF, buftmp, NULL));
	}
	else{
	    sprintf(tmp, "%*.*sA %s/%.*s%.10s%.100s%.10s segment containing:",
		    depth, depth, " ",
		    body_type_names(body->type),
		    sizeof(tmp)-300, body->subtype ? body->subtype : "Unknown",
		    name ? " (Name=\"" : "",
		    name ? name : "",
		    name ? "\")" : "");
	}

	if(save_ex_output_line(tmp, &ilen, pc))
	  *len += ilen;
	else
	  return(0);

	depth++;
	do				/* for each part */
	  if(save_ex_explain_parts(&part->body, depth, &ilen, pc))
	    *len += ilen;
	  else
	    return(0);
	while (part = part->next);	/* until done */
    }
    else{
	sprintf(tmp, "%*.*sA %s/%.*s%.10s%.100s%.10s segment of about %s bytes%s",
		depth, depth, " ",
		body_type_names(body->type), 
		sizeof(tmp)-300, body->subtype ? body->subtype : "Unknown",
		name ? " (Name=\"" : "",
		name ? name : "",
		name ? "\")" : "",
		comatose((body->encoding == ENCBASE64)
			   ? ((body->size.bytes * 3)/4)
			   : body->size.bytes),
		body->description ? "," : ".");
	if(!save_ex_output_line(tmp, len, pc))
	  return(0);

	if(body->description && *body->description){
	    sprintf(buftmp, "%.75s", body->description);
	    sprintf(tmp, "%*.*s   described as \"%.*s\"", depth, depth, " ",
		    sizeof(tmp)-100,
		    (char *) rfc1522_decode((unsigned char *)tmp_20k_buf,
					    SIZEOF_20KBUF, buftmp, NULL));
	    if(save_ex_output_line(tmp, &ilen, pc))
	      *len += ilen;
	    else
	      return(0);
	}
    }

    return(1);
}



int
save_ex_output_line(line, len, pc)
    char	  *line;
    unsigned long *len;
    gf_io_t	   pc;
{
    sprintf(tmp_20k_buf, "  [ %-*.*s ]\015\012", 68, 68, line);
    *len = strlen(tmp_20k_buf);
    return(gf_puts(tmp_20k_buf, pc));
}



/*----------------------------------------------------------------------
    Offer to create a non-existant folder to copy message[s] into

   Args: stream -- stream to use for creation
	 context -- context to create folder in
	 name -- name of folder to create

 Result: 0 if create failed (c-client writes error)
	 1 if create successful
	-1 if user declines to create folder
 ----*/
int
create_for_save(stream, context, folder)
    MAILSTREAM *stream;
    CONTEXT_S  *context;
    char       *folder;
{
    if(context && ps_global->context_list->next && context_isambig(folder)){
	if(context->use & CNTXT_INCMNG){
	    sprintf(tmp_20k_buf,
		"\"%.15s%s\" doesn't exist - Add it in FOLDER LIST screen",
		folder, (strlen(folder) > 15) ? "..." : "");
	    q_status_message(SM_ORDER, 3, 3, tmp_20k_buf);
	    return(0);		/* error */
	}

	sprintf(tmp_20k_buf,
		"Folder \"%.15s%s\" in <%.15s%s> doesn't exist. Create",
		folder, (strlen(folder) > 15) ? "..." : "",
		context->nickname,
		(strlen(context->nickname) > 15) ? "..." : "");
    }
    else
      sprintf(tmp_20k_buf,"Folder \"%.40s%s\" doesn't exist.  Create", 
	      folder, strlen(folder) > 40 ? "..." : "");

    if(want_to(tmp_20k_buf, 'y', 'n', NO_HELP, WT_SEQ_SENSITIVE) != 'y'){
	cmd_cancelled("Save message");
	return(-1);
    }

    return(context_create(context, NULL, folder));
}


/*----------------------------------------------------------------------
  Build flags string based on requested flags and what's set in messagecache

   Args: mc -- message cache element to dig the flags out of
	 flags -- flags to test
	 flagbuf -- place to write string representation of bits

 Result: flags represented in bits and mask written in flagbuf
 ----*/
void
flag_string(mc, flags, flagbuf)
    MESSAGECACHE *mc;
    long	  flags;
    char	 *flagbuf;
{
    char *p;

    *(p = flagbuf) = '\0';

    if(!mc)
      return;

    if((flags & F_DEL) && mc->deleted)
      sstrcpy(&p, "\\DELETED ");

    if((flags & F_ANS) && mc->answered)
      sstrcpy(&p, "\\ANSWERED ");

    if((flags & F_FLAG) && mc->flagged)
      sstrcpy(&p, "\\FLAGGED ");

    if((flags & F_SEEN) && mc->seen)
      sstrcpy(&p, "\\SEEN ");

    if(p != flagbuf)
      *--p = '\0';			/* tie off tmp_20k_buf   */
}



/*----------------------------------------------------------------------
   Save() helper function to create canonical date string from given header

   Args: date -- buf to recieve canonical date string
	 header -- rfc822 header to fish date string from

 Result: date filled with canonicalized date in header, or null string
 ----*/
void
saved_date(date, header)
    char *date, *header;
{
    char	 *d, *p, c;
    MESSAGECACHE  elt;

    *date = '\0';
    if((toupper((unsigned char)(*(d = header)))
	== 'D' && !strncmp(d, "date:", 5))
       || (d = srchstr(header, "\015\012date:"))){
	for(d += 7; *d == ' '; d++)
	  ;					/* skip white space */

	if(p = strstr(d, "\015\012")){
	    for(; p > d && *p == ' '; p--)
	      ;					/* skip white space */

	    c  = *p;
	    *p = '\0';				/* tie off internal date */
	}

	if(mail_parse_date(&elt, (unsigned char *) d))  /* normalize it */
	  mail_date(date, &elt);

	if(p)					/* restore header */
	  *p = c;
    }
}


/*
 * Find the last message in dstn_stream's folder that has message_id
 * equal to the argument. Set its keywords equal to the keywords which
 * are set in message mc from stream kw_stream.
 *
 * If you just saved the message you're looking for, it is a good idea
 * to send a ping over before you call this routine.
 *
 * Args:  kw_stream -- stream containing message mc
 *            mcsrc -- mail_elt for the source message
 *      dstn_stream -- where the new message is
 *       message_id -- the message id of the new message
 *            guess -- this is a positive integer, for example, 10. If it
 *                       is set we will first try to find the message_id
 *                       within the last "guess" messages in the folder,
 *                       unless the whole folder isn't much bigger than that
 */
void
set_keywords_in_msgid_msg(kw_stream, mcsrc, dstn_stream, message_id, guess)
    MAILSTREAM   *kw_stream;
    MESSAGECACHE *mcsrc;
    MAILSTREAM   *dstn_stream;
    char         *message_id;
    long          guess;
{
    SEARCHPGM *pgm = NULL;
    long        newmsgno;
    int         iter = 0, k;
    MESSAGECACHE *mc;
    extern MAILSTREAM *mm_search_stream;
    extern long        mm_search_count;

    if(!(kw_stream && dstn_stream))
      return;

    mm_search_count = 0L;
    mm_search_stream = dstn_stream;
    while(mm_search_count == 0L && iter++ < 2
	  && (pgm = mail_newsearchpgm()) != NULL){
	pgm->message_id = mail_newstringlist();
	pgm->message_id->text.data = (unsigned char *) cpystr(message_id);
	pgm->message_id->text.size = strlen(message_id);

	if(iter == 1){
	    /* lots of messages? restrict to last guess message on first try */
	    if(dstn_stream->nmsgs > guess + 40L){
		pgm->msgno = mail_newsearchset();
		pgm->msgno->first = dstn_stream->nmsgs - guess;
		pgm->msgno->first = min(max(pgm->msgno->first, 1),
					dstn_stream->nmsgs);
		pgm->msgno->last  = dstn_stream->nmsgs;
	    }
	    else
	      iter++;
	}

	pine_mail_search_full(dstn_stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);
	if(mm_search_count){
	    for(newmsgno=dstn_stream->nmsgs; newmsgno > 0L; newmsgno--)
	      if((mc = mail_elt(dstn_stream, newmsgno)) && mc->searched)
		break;
		  
	    if(newmsgno > 0L)
	      for(k = 0; k < NUSERFLAGS; k++)
		if(mcsrc && mcsrc->user_flags & (1 << k)
		   && kw_stream->user_flags[k]
		   && kw_stream->user_flags[k][0]){
		    int i;

		    /*
		     * Check to see if we know it is impossible to set
		     * this keyword before we try.
		     */
		    if(dstn_stream->kwd_create ||
		       (((i = user_flag_index(dstn_stream,
					      kw_stream->user_flags[k])) >= 0)
		        && i < NUSERFLAGS)){
		      mail_flag(dstn_stream, long2string(newmsgno),
			        kw_stream->user_flags[k], ST_SET);
		    }
		    else{
		      int some_defined = 0, w;
		      static time_t last_status_message = 0;
		      time_t now;
		      char b[200], c[200], *p;

		      for(i = 0; !some_defined && i < NUSERFLAGS; i++)
		        if(dstn_stream->user_flags[i])
			  some_defined++;
		    
		      /*
		       * Some unusual status message handling here. We'd
		       * like to print out one status message for every
		       * keyword missed, but if that happens every time
		       * you save to a particular folder, that would get
		       * annoying. So we only print out the first for each
		       * save or aggregate save.
		       */
		      if((now=time((time_t *) 0)) - last_status_message > 3){
		        last_status_message = now;
		        if(some_defined){
			  sprintf(b, "Losing keyword \"%.30s\". No more keywords allowed in ", keyword_to_nick(kw_stream->user_flags[k]));
			  w = min((ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - strlen(b) - 1 - 2, sizeof(c)-1);
			  p = short_str(STREAMNAME(dstn_stream), c, w,
					FrontDots);
			  q_status_message2(SM_ORDER, 3, 3, "%s%s!", b, p);
			}
			else{
			  sprintf(b, "Losing keyword \"%.30s\". Can't add keywords in ", keyword_to_nick(kw_stream->user_flags[k]));
			  w = min((ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - strlen(b) - 1 - 2, sizeof(b)-1);
			  p = short_str(STREAMNAME(dstn_stream), c, w,
					FrontDots);
			  q_status_message2(SM_ORDER, 3, 3, "%s%s!", b, p);
			}
		      }

		      if(some_defined){
			dprint(1, (debugfile, "Losing keyword \"%s\". No more keywords allowed in %s\n", kw_stream->user_flags[k], dstn_stream->mailbox ? dstn_stream->mailbox : "target folder"));
		      }
		      else{
			dprint(1, (debugfile, "Losing keyword \"%s\". Can't add keywords in %s\n", kw_stream->user_flags[k], dstn_stream->mailbox ? dstn_stream->mailbox : "target folder"));
		      }
		    }
		}
	}
    }
}


long
get_msgno_by_msg_id(stream, message_id, msgmap)
    MAILSTREAM   *stream;
    char         *message_id;
    MSGNO_S      *msgmap;
{
    SEARCHPGM  *pgm = NULL;
    long        hint = mn_m2raw(msgmap, mn_get_cur(msgmap));
    long        newmsgno = -1L;
    int         iter = 0, k;
    MESSAGECACHE *mc;
    extern MAILSTREAM *mm_search_stream;
    extern long        mm_search_count;

    if(!(message_id && message_id[0]))
      return(newmsgno);

    mm_search_count = 0L;
    mm_search_stream = stream;
    while(mm_search_count == 0L && iter++ < 3
	  && (pgm = mail_newsearchpgm()) != NULL){
	pgm->message_id = mail_newstringlist();
	pgm->message_id->text.data = (unsigned char *) cpystr(message_id);
	pgm->message_id->text.size = strlen(message_id);

	if(iter > 1 || hint > stream->nmsgs)
	  iter++;

	if(iter == 1){
	    /* restrict to hint message on first try */
	    pgm->msgno = mail_newsearchset();
	    pgm->msgno->first = pgm->msgno->last = hint;
	}
	else if(iter == 2){
	    /* restrict to last 50 messages on 2nd try */
	    pgm->msgno = mail_newsearchset();
	    if(stream->nmsgs > 100L)
	      pgm->msgno->first = stream->nmsgs-50L;
	    else{
		pgm->msgno->first = 1L;
		iter++;
	    }

	    pgm->msgno->last = stream->nmsgs;
	}

	pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);

	if(mm_search_count){
	    for(newmsgno=stream->nmsgs; newmsgno > 0L; newmsgno--)
	      if((mc = mail_elt(stream, newmsgno)) && mc->searched)
		break;
	}
    }

    return(mn_raw2m(msgmap, newmsgno));
}



/*----------------------------------------------------------------------
    Export a message to a plain file in users home directory

    Args: state -- pointer to struct holding a bunch of pine state
	 msgmap -- table mapping msg nums to c-client sequence nums
	  qline -- screen line to ask questions on
	    agg -- boolean indicating we're to operate on aggregate set

 Result: 
 ----*/
void
cmd_export(state, msgmap, qline, agg)
    struct pine *state;
    MSGNO_S     *msgmap;
    int          qline;
    int		 agg;
{
    char      filename[MAXPATH+1], full_filename[MAXPATH+1], *err;
    char      nmsgs[80];
    int       r, leading_nl, failure = 0, orig_errno, rflags = GER_NONE;
    int       flags = GE_IS_EXPORT | GE_SEQ_SENSITIVE;
    ENVELOPE *env;
    MESSAGECACHE *mc;
    BODY     *b;
    long      i, count = 0L, start_of_append, rawno;
    gf_io_t   pc;
    STORE_S  *store;
    struct variable *vars = ps_global->vars;
    ESCKEY_S export_opts[5];

    if(ps_global->restricted){
	q_status_message(SM_ORDER, 0, 3,
	    "Pine demo can't export messages to files");
	return;
    }

    if(agg && !pseudo_selected(msgmap))
      return;

    export_opts[i = 0].ch  = ctrl('T');
    export_opts[i].rval	   = 10;
    export_opts[i].name	   = "^T";
    export_opts[i++].label = "To Files";

#if	!defined(DOS) && !defined(MAC) && !defined(OS2)
    if(ps_global->VAR_DOWNLOAD_CMD && ps_global->VAR_DOWNLOAD_CMD[0]){
	export_opts[i].ch      = ctrl('V');
	export_opts[i].rval    = 12;
	export_opts[i].name    = "^V";
	export_opts[i++].label = "Downld Msg";
    }
#endif	/* !(DOS || MAC) */

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	export_opts[i].ch      =  ctrl('I');
	export_opts[i].rval    = 11;
	export_opts[i].name    = "TAB";
	export_opts[i++].label = "Complete";
    }

#if	0
    /* Commented out since it's not yet support! */
    if(F_ON(F_ENABLE_SUB_LISTS,ps_global)){
	export_opts[i].ch      = ctrl('X');
	export_opts[i].rval    = 14;
	export_opts[i].name    = "^X";
	export_opts[i++].label = "ListMatches";
    }
#endif

    /*
     * If message has attachments, add a toggle that will allow the user
     * to save all of the attachments to a single directory, using the
     * names provided with the attachments or part names. What we'll do is
     * export the message as usual, and then export the attachments into
     * a subdirectory that did not exist before. The subdir will be named
     * something based on the name of the file being saved to, but a
     * unique, new name.
     */
    if(!agg
       && state->mail_stream
       && (rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
       && rawno <= state->mail_stream->nmsgs
       && (env = pine_mail_fetchstructure(state->mail_stream, rawno, &b))
       && b
       && b->type == TYPEMULTIPART
       && b->subtype
       && strucmp(b->subtype, "ALTERNATIVE") != 0){
	PART *part;

	part = b->nested.part;	/* 1st part */
	if(part->next)
	  flags |= GE_ALLPARTS;
    }

    export_opts[i].ch = -1;
    filename[0] = '\0';

    if(mn_total_cur(msgmap) <= 1L)
      sprintf(nmsgs, "Msg #%ld", mn_get_cur(msgmap));
    else
      sprintf(nmsgs, "%.10s messages", comatose(mn_total_cur(msgmap)));

    r = get_export_filename(state, filename, NULL, full_filename,
			    sizeof(filename), nmsgs, "EXPORT",
			    export_opts, &rflags, qline, flags);

    if(r < 0){
	switch(r){
	  case -1:
	    cmd_cancelled("Export message");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
			      "Can't export to file outside of %.200s",
			      VAR_OPER_DIR);
	    break;
	}

	goto fini;
    }
#if	!defined(DOS) && !defined(MAC) && !defined(OS2)
    else if(r == 12){			/* Download */
	char     cmd[MAXPATH], *tfp = NULL;
	int	     next = 0;
	PIPE_S  *syspipe;
	STORE_S *so;
	gf_io_t  pc;

	if(ps_global->restricted){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Download disallowed in restricted mode");
	    goto fini;
	}

	err = NULL;
	tfp = temp_nam(NULL, "pd");
	build_updown_cmd(cmd, ps_global->VAR_DOWNLOAD_CMD_PREFIX,
			 ps_global->VAR_DOWNLOAD_CMD, tfp);
	dprint(1, (debugfile, "Download cmd called: \"%s\"\n", cmd));
	if(so = so_get(FileStar, tfp, WRITE_ACCESS|OWNER_ONLY)){
	    gf_set_so_writec(&pc, so);

	    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){
	      if(!(state->mail_stream
		 && (rawno = mn_m2raw(msgmap, i)) > 0L
		 && rawno <= state->mail_stream->nmsgs
		 && (mc = mail_elt(state->mail_stream, rawno))
		 && mc->valid))
	        mc = NULL;

	      if(!(env = pine_mail_fetchstructure(state->mail_stream,
						  mn_m2raw(msgmap, i), &b))
		 || !bezerk_delimiter(env, mc, pc, next++)
		 || !format_message(mn_m2raw(msgmap, mn_get_cur(msgmap)),
				    env, b, NULL, FM_NEW_MESS | FM_NOWRAP, pc)){
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
			   err = "Error writing tempfile for download");
		  break;
	      }
	    }

	    gf_clear_so_writec(so);
	    if(so_give(&so)){			/* close file */
		if(!err)
		  err = "Error writing tempfile for download";
	    }

	    if(!err){
		if(syspipe = open_system_pipe(cmd, NULL, NULL,
					      PIPE_USER | PIPE_RESET, 0))
		  (void) close_system_pipe(&syspipe, NULL, 0);
		else
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
				err = "Error running download command");
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 3,
			 err = "Error building temp file for download");

	if(tfp){
	    unlink(tfp);
	    fs_give((void **)&tfp);
	}

	if(!err)
	  q_status_message(SM_ORDER, 0, 3, "Download Command Completed");

	goto fini;
    }
#endif	/* !(DOS || MAC) */


    if(rflags & GER_APPEND)
      leading_nl = 1;
    else
      leading_nl = 0;

    dprint(5, (debugfile, "Opening file \"%s\" for export\n",
	   full_filename ? full_filename : "?"));

    if(!(store = so_get(FileStar, full_filename, WRITE_ACCESS))){
        q_status_message2(SM_ORDER | SM_DING, 3, 4,
		      "Error opening file \"%.200s\" to export message: %.200s",
                          full_filename, error_description(errno));
	goto fini;
    }
    else
      gf_set_so_writec(&pc, store);

    err = NULL;
    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap), count++){
	env = pine_mail_fetchstructure(state->mail_stream, mn_m2raw(msgmap, i),
				       &b);
	if(!env) {
	    err = "Can't export message. Error accessing mail folder";
	    failure = 1;
	    break;
	}

        if(!(state->mail_stream
	   && (rawno = mn_m2raw(msgmap, i)) > 0L
	   && rawno <= state->mail_stream->nmsgs
	   && (mc = mail_elt(state->mail_stream, rawno))
	   && mc->valid))
	  mc = NULL;

	start_of_append = so_tell(store);
	if(!bezerk_delimiter(env, mc, pc, leading_nl)
	   || !format_message(mn_m2raw(msgmap, i), env, b, NULL,
			      FM_NEW_MESS | FM_NOWRAP | FM_WRAPFLOWED, pc)){
	    orig_errno = errno;		/* save incase things are really bad */
	    failure    = 1;		/* pop out of here */
	    break;
	}

	leading_nl = 1;
    }

    gf_clear_so_writec(store);
    if(so_give(&store))				/* release storage */
      failure++;

    if(failure){
	truncate(full_filename, (off_t)start_of_append);
	if(err){
	    dprint(1, (debugfile, "FAILED Export: fetch(%ld): %s\n",
		       i, err ? err : "?"));
	    q_status_message(SM_ORDER | SM_DING, 3, 4, err);
	}
	else{
	    dprint(1, (debugfile, "FAILED Export: file \"%s\" : %s\n",
		       full_filename ? full_filename : "?",
		       error_description(orig_errno)));
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			      "Error exporting to \"%.200s\" : %.200s",
			      filename, error_description(orig_errno));
	}
    }
    else{
	if(rflags & GER_ALLPARTS && full_filename[0]){
	    char dir[MAXPATH+1];
	    char *p1, *p2, *p3;
	    char *att_name = "filename";
	    char  lfile[MAXPATH+1];
	    int  ok = 0, tries = 0, saved = 0, errs = 0;
	    ATTACH_S *a;

	    /*
	     * Now we want to save all of the attachments to a subdirectory.
	     * To make it easier for us and probably easier for the user, and
	     * to prevent the user from shooting himself in the foot, we
	     * make a new subdirectory so that we can't possibly step on
	     * any existing files, and we don't need any interaction with the
	     * user while saving.
	     *
	     * We'll just use the directory name full_filename.d or if that
	     * already exists and isn't empty, we'll try adding a suffix to
	     * that until we get something to use.
	     */

	    if(strlen(full_filename) + strlen(".d") + 1 > sizeof(dir)){
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  "Can't save attachments, filename too long: %.200s",
			  full_filename);
		goto fini;
	    }

	    ok = 0;
	    sprintf(dir, "%s.d", full_filename, S_FILESEP);

	    do {
		tries++;
		switch(r = is_writable_dir(dir)){
		  case 0:		/* exists and is a writable dir */
		    /*
		     * We could figure out if it is empty and use it in
		     * that case, but that sounds like a lot of work, so
		     * just fall through to default.
		     */

		  default:
		    if(strlen(full_filename) + strlen(".d") + 1 +
		       1 + strlen(long2string((long) tries)) > sizeof(dir)){
			q_status_message(SM_ORDER | SM_DING, 3, 4,
					      "Problem saving attachments");
			goto fini;
		    }

		    sprintf(dir, "%s.d_%s", full_filename,
			    long2string((long) tries));
		    break;

		  case 3:		/* doesn't exist, that's good! */
		    /* make new directory */
		    ok++;
		    break;
		}
	    } while(!ok && tries < 1000);
	    
	    if(tries >= 1000){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
					      "Problem saving attachments");
		goto fini;
	    }

	    /* create the new directory */
	    if(mkdir(dir, 0700)){
		q_status_message2(SM_ORDER | SM_DING, 3, 4,
		      "Problem saving attachments: %s: %s", dir,
		      error_description(errno));
		goto fini;
	    }

	    if(!(state->mail_stream
	         && (rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
	         && rawno <= state->mail_stream->nmsgs
	         && (env=pine_mail_fetchstructure(state->mail_stream,rawno,&b))
	         && b)){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
					      "Problem reading message");
		goto fini;
	    }

	    zero_atmts(state->atmts);
	    describe_mime(b, "", 1, 1, 0);

	    a = state->atmts;
	    if(a && a->description)		/* skip main body part */
	      a++;

	    for(; a->description != NULL; a++){
		/* skip over these parts of the message */
		if(MIME_MSG_A(a) || MIME_DGST_A(a) || MIME_VCARD_A(a))
		  continue;
		
		lfile[0] = '\0';
		if((a->body && a->body->disposition.type &&
		   (p1 = rfc2231_get_param(a->body->disposition.parameter,
					  att_name, NULL, NULL))) ||
		   (p1 = rfc2231_get_param(a->body->parameter,
					  att_name + 4, NULL, NULL))){

		    if(p1[0] == '=' && p1[1] == '?'){
			if(!(p2 = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, p1, NULL)))
			  p2 = p1;
			  
		    }
		    else
		      p2 = p1;

		    p3 = last_cmpnt(p2);
		    if(!p3)
		      p3 = p2;

		    strncpy(lfile, p3, sizeof(lfile)-1);
		    lfile[sizeof(lfile)-1] = '\0';

		    fs_give((void **) &p1);
		}
		
		if(lfile[0] == '\0')
		  sprintf(lfile, "part_%.*s", sizeof(lfile)-6,
			  a->number ? a->number : "?");

		if(strlen(dir) + strlen(S_FILESEP) + strlen(lfile) + 1
							    > sizeof(filename)){
		    dprint(2, (debugfile,
			   "FAILED Att Export: name too long: %s\n",
			   dir, S_FILESEP, lfile));
		    errs++;
		    continue;
		}

		sprintf(filename, "%s%s%s", dir, S_FILESEP, lfile);

		if(write_attachment_to_file(state->mail_stream, rawno,
					    a, GER_NONE, filename) == 1)
		  saved++;
		else
		  errs++;
	    }

	    if(errs){
		if(saved)
		  q_status_message1(SM_ORDER, 3, 3,
			"Errors saving some attachments, %s attachments saved",
			long2string((long) saved));
		else
		  q_status_message(SM_ORDER, 3, 3,
			"Problems saving attachments");
	    }
	    else{
		if(saved)
		  q_status_message2(SM_ORDER, 0, 3,
			"Saved %s attachments to %.200s",
			long2string((long) saved), dir);
		else
		  q_status_message(SM_ORDER, 3, 3, "No attachments to save");
	    }
	}
	else if(mn_total_cur(msgmap) > 1L)
	  q_status_message4(SM_ORDER,0,3,
			    "%.200s message%.200s %.200s to file \"%.200s\"",
			    long2string(count), plural(count),
			    rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "exported",
			    filename);
	else
	  q_status_message3(SM_ORDER,0,3,
			    "Message %.200s %.200s to file \"%.200s\"",
			    long2string(mn_get_cur(msgmap)),
			    rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "exported",
			    filename);
    }

  fini:
    if(agg)
      restore_selected(msgmap);
}


/*
 * Ask user what file to export to. Export from srcstore to that file.
 *
 * Args     ps -- pine struct
 *     srctext -- pointer to source text
 *     srctype -- type of that source text
 *  prompt_msg -- see get_export_filename
 *  lister_msg --      "
 *
 * Returns: != 0 : error
 *             0 : ok
 */
int
simple_export(ps, srctext, srctype, prompt_msg, lister_msg)
    struct pine *ps;
    void        *srctext;
    SourceType   srctype;
    char        *prompt_msg;
    char        *lister_msg;
{
    int r = 1, rflags = GER_NONE;
    char     filename[MAXPATH+1], full_filename[MAXPATH+1];
    STORE_S *store = NULL;
    struct variable *vars = ps->vars;
    static ESCKEY_S simple_export_opts[] = {
	{ctrl('T'), 10, "^T", "To Files"},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps)){
	simple_export_opts[r].ch    =  ctrl('I');
	simple_export_opts[r].rval  = 11;
	simple_export_opts[r].name  = "TAB";
	simple_export_opts[r].label = "Complete";
    }

    if(!srctext){
	q_status_message(SM_ORDER, 0, 2, "Error allocating space");
	r = -3;
	goto fini;
    }

    simple_export_opts[++r].ch = -1;
    filename[0] = '\0';
    full_filename[0] = '\0';

    r = get_export_filename(ps, filename, NULL, full_filename, sizeof(filename),
			    prompt_msg, lister_msg, simple_export_opts, &rflags,
			    -FOOTER_ROWS(ps), GE_IS_EXPORT);

    if(r < 0)
      goto fini;
    else if(!full_filename[0]){
	r = -1;
	goto fini;
    }

    dprint(5, (debugfile, "Opening file \"%s\" for export\n",
	   full_filename ? full_filename : "?"));

    if((store = so_get(FileStar, full_filename, WRITE_ACCESS)) != NULL){
	char *pipe_err;
	gf_io_t pc, gc;

	gf_set_so_writec(&pc, store);
	gf_set_readc(&gc, srctext, (srctype == CharStar)
					? strlen((char *)srctext)
					: 0L,
		     srctype);
	gf_filter_init();
	if((pipe_err = gf_pipe(gc, pc)) != NULL){
	    q_status_message2(SM_ORDER | SM_DING, 3, 3,
			      "Problem saving to \"%.200s\": %.200s",
			      filename, pipe_err);
	    r = -3;
	}
	else
	  r = 0;

	gf_clear_so_writec(store);
	if(so_give(&store)){
	    q_status_message2(SM_ORDER | SM_DING, 3, 3,
			      "Problem saving to \"%.200s\": %.200s",
			      filename, error_description(errno));
	    r = -3;
	}
    }
    else{
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Error opening file \"%.200s\" for export: %.200s",
			  full_filename, error_description(errno));
	r = -3;
    }

fini:
    switch(r){
      case  0:
	/* overloading full_filename */
	sprintf(full_filename, "%c%s",
		(prompt_msg && prompt_msg[0])
		  ? (islower((unsigned char)prompt_msg[0])
		    ? toupper((unsigned char)prompt_msg[0]) : prompt_msg[0])
		  : 'T',
	        (prompt_msg && prompt_msg[0]) ? prompt_msg+1 : "ext");
	q_status_message3(SM_ORDER,0,2,"%.200s %.200s to \"%.200s\"",
			  full_filename,
			  rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "exported",
			  filename);
	break;

      case -1:
	cmd_cancelled("Export");
	break;

      case -2:
	q_status_message1(SM_ORDER, 0, 2,
	    "Can't export to file outside of %.200s", VAR_OPER_DIR);
	break;
    }

    ps->mangled_footer = 1;
    return(r);
}



/*
 * Ask user what file to export to.
 *
 *       filename -- On input, this is the filename to start with. On exit,
 *                   this is the filename chosen. (but this isn't used)
 *       deefault -- This is the default value if user hits return. The
 *                   prompt will have [deefault] added to it automatically.
 *  full_filename -- This is the full filename on exit.
 *            len -- Minimum length of _both_ filename and full_filename.
 *     prompt_msg -- Message to insert in prompt.
 *     lister_msg -- Message to insert in file_lister.
 *           opts -- Key options.
 *                      There is a tangled relationship between the callers
 *                      and this routine as far as opts are concerned. Some
 *                      of the opts are handled here. In particular, r == 3,
 *                      r == 10, r == 11, and r == 13 are all handled here.
 *                      Don't use those values unless you want what happens
 *                      here. r == 12 and others are handled by the caller.
 *         rflags -- Return flags
 *                     GER_OVER      - overwrite of existing file
 *                     GER_APPEND    - append of existing file
 *                      else file did not exist before
 *
 *                     GER_ALLPARTS  - AllParts toggle was turned on
 *
 *          qline -- Command line to prompt on.
 *          flags -- Logically OR'd flags
 *                     GE_IS_EXPORT     - The command was an Export command
 *                                        so the prompt should include
 *                                        EXPORT:.
 *                     GE_SEQ_SENSITIVE - The command that got us here is
 *                                        sensitive to sequence number changes
 *                                        caused by unsolicited expunges.
 *                     GE_NO_APPEND     - We will not allow append to an
 *                                        existing file, only removal of the
 *                                        file if it exists.
 *                     GE_IS_IMPORT     - We are selecting for reading.
 *                                        No overwriting or checking for
 *                                        existence at all. Don't use this
 *                                        together with GE_NO_APPEND.
 *                     GE_ALLPARTS      - Turn on AllParts toggle.
 *
 *  Returns:  -1  cancelled
 *            -2  prohibited by VAR_OPER_DIR
 *            -3  other error, already reported here
 *             0  ok
 *            12  user chose 12 command from opts
 */
int
get_export_filename(ps, filename, deefault, full_filename, len, prompt_msg,
		    lister_msg, optsarg, rflags, qline, flags)
    struct pine *ps;
    char        *filename;
    char        *deefault;
    char        *full_filename;
    size_t       len;
    char        *prompt_msg;
    char        *lister_msg;
    ESCKEY_S     optsarg[];
    int         *rflags;
    int          qline;
    int          flags;
{
    char      dir[MAXPATH+1], dir2[MAXPATH+1];
    char      precolon[MAXPATH+1], postcolon[MAXPATH+1];
    char      filename2[MAXPATH+1], tmp[MAXPATH+1], *fn, *ill;
    int       l, i, r, fatal, homedir = 0, was_abs_path=0, avail, ret = 0;
    int       allparts = 0;
    char      prompt_buf[400];
    char      def[500];
    ESCKEY_S *opts = NULL;
    struct variable *vars = ps->vars;

    if(flags & GE_ALLPARTS){
	/*
	 * Copy the opts and add one to the end of the list.
	 */
	for(i = 0; optsarg[i].ch != -1; i++)
	  ;
	
	i++;

	opts = (ESCKEY_S *) fs_get((i+1) * sizeof(*opts));
	memset(opts, 0, (i+1) * sizeof(*opts));

	for(i = 0; optsarg[i].ch != -1; i++){
	    opts[i].ch = optsarg[i].ch;
	    opts[i].rval = optsarg[i].rval;
	    opts[i].name = optsarg[i].name;	/* no need to make a copy */
	    opts[i].label = optsarg[i].label;	/* " */
	}

	allparts = i;
	opts[i].ch      = ctrl('P');
	opts[i].rval    = 13;
	opts[i].name    = "^P";
	opts[i++].label = "AllParts";
	
	opts[i].ch = -1;
    }
    else
      opts = optsarg;

    if(rflags)
      *rflags = GER_NONE;

    if(F_ON(F_USE_CURRENT_DIR, ps))
      dir[0] = '\0';
    else if(VAR_OPER_DIR)
      strcpy(dir, VAR_OPER_DIR);
#if	defined(DOS) || defined(OS2)
    else if(VAR_FILE_DIR)
      strcpy(dir, VAR_FILE_DIR);
#endif
    else{
	dir[0] = '~';
	dir[1] = '\0';
	homedir=1;
    }

    strcpy(precolon, dir);
    postcolon[0] = '\0';
    if(deefault){
	strncpy(def, deefault, sizeof(def)-1);
	def[sizeof(def)-1] = '\0';
	removing_leading_and_trailing_white_space(def);
    }
    else
      def[0] = '\0';
    
    /* 6 is needed by optionally_enter */
    avail = max(20, ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - 6;

    /*---------- Prompt the user for the file name -------------*/
    while(1){
	int  oeflags;
	char dirb[50], fileb[50];
	int  l1, l2, l3, l4, l5, needed;
	char *p, p1[100], p2[100], *p3, p4[100], p5[100];

	sprintf(p1, "%.10sCopy ", 
		(flags & GE_IS_EXPORT) ? "EXPORT: " :
		  (flags & GE_IS_IMPORT) ? "IMPORT: " : "SAVE: ");
	l1 = strlen(p1);

	strncpy(p2, prompt_msg ? prompt_msg : "", sizeof(p2)-1);
	p2[sizeof(p2)-1] = '\0';
	l2 = strlen(p2);

	if(rflags && *rflags & GER_ALLPARTS)
	  p3 = " (and atts)";
	else
	  p3 = "";
	
	l3 = strlen(p3);

	sprintf(p4, " %.10s file%.10s%.30s",
		(flags & GE_IS_IMPORT) ? "from" : "to",
		is_absolute_path(filename) ? "" : " in ",
		is_absolute_path(filename) ? "" :
		  (!dir[0] ? "current directory"
			   : (dir[0] == '~' && !dir[1]) ? "home directory"
				     : short_str(dir,dirb,30,FrontDots)));
	l4 = strlen(p4);

	sprintf(p5, "%.2s%.50s%.1s: ",
		*def ? " [" : "",
		*def ? short_str(def,fileb,40,EndDots) : "",
		*def ? "]" : "");
	l5 = strlen(p5);

	if((needed = l1+l2+l3+l4+l5-avail) > 0){
	    sprintf(p4, " %.10s file%.10s%.30s",
		    (flags & GE_IS_IMPORT) ? "from" : "to",
		    is_absolute_path(filename) ? "" : " in ",
		    is_absolute_path(filename) ? "" :
		      (!dir[0] ? "current dir"
			       : (dir[0] == '~' && !dir[1]) ? "home dir"
					 : short_str(dir,dirb,10, FrontDots)));
	    l4 = strlen(p4);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0 && l5 > 0){
	    sprintf(p5, "%.2s%.50s%.1s: ",
		    *def ? " [" : "",
		    *def ? short_str(def,fileb,
				     max(15,l5-5-needed),EndDots) : "",
		    *def ? "]" : "");
	    l5 = strlen(p5);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0 && l2 > 0){

	    /*
	     * 14 is about the shortest we can make this, because there are
	     * fixed length strings of length 14 coming in here.
	     */
	    p = short_str(prompt_msg, p2, max(14,l2-needed), FrontDots);
	    if(p != p2){
		strncpy(p2, p, sizeof(p2)-1);
		p2[sizeof(p2)-1] = '\0';
	    }

	    l2 = strlen(p2);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0){
	    strncpy(p1, "Copy ", sizeof(p1)-1);
	    p1[sizeof(p1)-1] = '\0';
	    l1 = strlen(p1);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0 && l5 > 0){
	    sprintf(p5, "%.2s%.50s%.1s: ",
		    *def ? " [" : "",
		    *def ? short_str(def,fileb,
				     max(10,l5-5-needed),EndDots) : "",
		    *def ? "]" : "");
	    l5 = strlen(p5);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0 && l3 > 0){
	    if(needed <= l3 - strlen(" (+ atts)"))
	      p3 = " (+ atts)";
	    else if(needed <= l3 - strlen(" (atts)"))
	      p3 = " (atts)";
	    else if(needed <= l3 - strlen(" (+)"))
	      p3 = " (+)";
	    else if(needed <= l3 - strlen("+"))
	      p3 = "+";
	    else
	      p3 = "";

	    l3 = strlen(p3);
	}

	sprintf(prompt_buf, "%.50s%.50s%.50s%.50s%.50s", p1, p2, p3, p4, p5);
	oeflags = OE_APPEND_CURRENT |
		  ((flags & GE_SEQ_SENSITIVE) ? OE_SEQ_SENSITIVE : 0);
	r = optionally_enter(filename, qline, 0, len, prompt_buf,
			     opts, NO_HELP, &oeflags);

        /*--- Help ----*/
	if(r == 3){
	    /*
	     * Helps may not be right if you add another caller or change
	     * things. Check it out.
	     */
	    if(flags & GE_IS_IMPORT)
	      helper(h_ge_import, "HELP FOR IMPORT FILE SELECT", HLPD_SIMPLE);
	    else if(flags & GE_ALLPARTS)
	      helper(h_ge_allparts, "HELP FOR EXPORT FILE SELECT", HLPD_SIMPLE);
	    else
	      helper(h_ge_export, "HELP FOR EXPORT FILE SELECT", HLPD_SIMPLE);

	    ps->mangled_screen = 1;

	    continue;
        }
	else if(r == 10 || r == 11){	/* Browser or File Completion */
	    if(filename[0]=='~'){
	      if(filename[1] == C_FILESEP && filename[2]!='\0'){
		precolon[0] = '~';
		precolon[1] = '\0';
		for(i=0; filename[i+2] != '\0' && i+2 < len-1; i++)
		  filename[i] = filename[i+2];
		filename[i] = '\0';
		strncpy(dir, precolon, sizeof(dir)-1);
		dir[sizeof(dir)-1] = '\0';
	      }
	      else if(filename[1]=='\0' || 
		 (filename[1] == C_FILESEP && filename[2] == '\0')){
		precolon[0] = '~';
		precolon[1] = '\0';
		filename[0] = '\0';
		strncpy(dir, precolon, sizeof(dir)-1);
		dir[sizeof(dir)-1] = '\0';
	      }
	    }
	    else if(!dir[0] && !is_absolute_path(filename) && was_abs_path){
	      if(homedir){
		precolon[0] = '~';
		precolon[1] = '\0';
		strncpy(dir, precolon, sizeof(dir)-1);
		dir[sizeof(dir)-1] = '\0';
	      }
	      else{
		precolon[0] = '\0';
		dir[0] = '\0';
	      }
	    }
	    l = MAXPATH;
	    dir2[0] = '\0';
	    strncpy(tmp, filename, sizeof(tmp)-1);
	    tmp[sizeof(tmp)-1] = '\0';
	    if(*tmp && is_absolute_path(tmp))
	      fnexpand(tmp, sizeof(tmp));
	    if(strncmp(tmp,postcolon, strlen(postcolon)))
	      postcolon[0] = '\0';

	    if(*tmp && (fn = last_cmpnt(tmp))){
	        l -= fn - tmp;
		strncpy(filename2, fn, sizeof(filename2)-1);
		filename2[sizeof(filename2)-1] = '\0';
		if(is_absolute_path(tmp)){
		    strncpy(dir2, tmp, min(fn - tmp, sizeof(dir2)-1));
		    dir2[min(fn - tmp, sizeof(dir2)-1)] = '\0';
#ifdef _WINDOWS
		    if(tmp[1]==':' && tmp[2]=='\\' && dir2[2]=='\0'){
		      dir2[2] = '\\';
		      dir2[3] = '\0';
		    }
#endif
		    strncpy(postcolon, dir2, sizeof(postcolon)-1);
		    postcolon[sizeof(postcolon)-1] = '\0';
		    precolon[0] = '\0';
		}
		else{
		    char *p = NULL;
		    /*
		     * Just building the directory name in dir2,
		     * full_filename is overloaded.
		     */
		    sprintf(full_filename, "%.*s", min(fn-tmp,len-1), tmp);
		    strncpy(postcolon, full_filename, sizeof(postcolon)-1);
		    postcolon[sizeof(postcolon)-1] = '\0';
		    build_path(dir2, !dir[0] ? p = (char *)getcwd(NULL,MAXPATH)
					     : (dir[0] == '~' && !dir[1])
					       ? ps->home_dir
					       : dir,
			       full_filename, sizeof(dir2));
		    if(p)
		      free(p);
		}
	    }
	    else{
		if(is_absolute_path(tmp)){
		    strncpy(dir2, tmp, sizeof(dir2)-1);
		    dir2[sizeof(dir2)-1] = '\0';
#ifdef _WINDOWS
		    if(dir2[2]=='\0' && dir2[1]==':'){
		      dir2[2]='\\';
		      dir2[3]='\0';
		      strncpy(postcolon,dir2,sizeof(postcolon)-1);
		    }
#endif
		    filename2[0] = '\0';
		    precolon[0] = '\0';
		}
		else{
		    strncpy(filename2, tmp, sizeof(filename2)-1);
		    filename2[sizeof(filename2)-1] = '\0';
		    if(!dir[0])
		      (void)getcwd(dir2, sizeof(dir2));
		    else if(dir[0] == '~' && !dir[1]){
			strncpy(dir2, ps->home_dir, sizeof(dir2)-1);
			dir2[sizeof(dir2)-1] = '\0';
		    }
		    else{
			strncpy(dir2, dir, sizeof(dir2)-1);
			dir2[sizeof(dir2)-1] = '\0';
		    }
		    postcolon[0] = '\0';
		}
	    }

	    build_path(full_filename, dir2, filename2, len);
	    if(!strcmp(full_filename, dir2))
	      filename2[0] = '\0';
	    if(full_filename[strlen(full_filename)-1] == C_FILESEP 
	       && isdir(full_filename,NULL,NULL)){
	      if(strlen(full_filename) == 1)
		strncpy(postcolon, full_filename, sizeof(postcolon)-1);
	      else if(filename2[0])
		strncpy(postcolon, filename2, sizeof(postcolon)-1);
	      postcolon[sizeof(postcolon)-1] = '\0';
	      strncpy(dir2, full_filename, sizeof(dir2)-1);
	      dir2[sizeof(dir2)-1] = '\0';
	      filename2[0] = '\0';
	    }
#ifdef _WINDOWS  /* use full_filename even if not a valid directory */
	    else if(full_filename[strlen(full_filename)-1] == C_FILESEP){ 
	      strncpy(postcolon, filename2, sizeof(postcolon)-1);
	      postcolon[sizeof(postcolon)-1] = '\0';
	      strncpy(dir2, full_filename, sizeof(dir2)-1);
	      dir2[sizeof(dir2)-1] = '\0';
	      filename2[0] = '\0';
	    }
#endif
	    if(dir2[strlen(dir2)-1] == C_FILESEP && strlen(dir2)!=1
	       && strcmp(dir2+1, ":\\")) 
	      /* last condition to prevent stripping of '\\' 
		 in windows partition */
	      dir2[strlen(dir2)-1] = '\0';

	    if(r == 10){			/* File Browser */
		r = file_lister(lister_msg ? lister_msg : "EXPORT",
				dir2, MAXPATH+1, filename2, MAXPATH+1, 
                                TRUE,
				(flags & GE_IS_IMPORT) ? FB_READ : FB_SAVE);
#ifdef _WINDOWS
/* Windows has a special "feature" in which entering the file browser will
   change the working directory if the directory is changed at all (even
   clicking "Cancel" will change the working directory).
*/
		if(F_ON(F_USE_CURRENT_DIR, ps))
		  (void)getcwd(dir2,sizeof(dir2));
#endif
		if(isdir(dir2,NULL,NULL)){
		  strncpy(precolon, dir2, sizeof(precolon)-1);
		  precolon[sizeof(precolon)-1] = '\0';
		}
		strncpy(postcolon, filename2, sizeof(postcolon)-1);
		postcolon[sizeof(postcolon)-1] = '\0';
		if(r == 1){
		    build_path(full_filename, dir2, filename2, len);
		    if(isdir(full_filename, NULL, NULL)){
			strncpy(dir, full_filename, sizeof(dir)-1);
			dir[sizeof(dir)-1] = '\0';
			filename[0] = '\0';
		    }
		    else{
			fn = last_cmpnt(full_filename);
			strncpy(dir, full_filename,
				min(fn - full_filename, sizeof(dir)-1));
			dir[min(fn - full_filename, sizeof(dir)-1)] = '\0';
			if(fn - full_filename > 1)
			  dir[fn - full_filename - 1] = '\0';
		    }
		    
		    if(!strcmp(dir, ps->home_dir)){
			dir[0] = '~';
			dir[1] = '\0';
		    }

		    strncpy(filename, fn, len-1);
		    filename[len-1] = '\0';
		}
	    }
	    else{				/* File Completion */
	      if(!pico_fncomplete(dir2, filename2, l - 1))
		  Writechar(BELL, 0);
	      strncat(postcolon, filename2,
		      sizeof(postcolon)-1-strlen(postcolon));
	      
	      was_abs_path = is_absolute_path(filename);

	      if(!strcmp(dir, ps->home_dir)){
		dir[0] = '~';
		dir[1] = '\0';
	      }
	    }
	    strncpy(filename, postcolon, len-1);
	    filename[len-1] = '\0';
	    strncpy(dir, precolon, sizeof(dir)-1);
	    dir[sizeof(dir)-1] = '\0';

	    if(filename[0] == '~' && !filename[1]){
		dir[0] = '~';
		dir[1] = '\0';
		filename[0] = '\0';
	    }

	    continue;
	}
	else if(r == 12){	/* Download, caller handles it */
	    ret = r;
	    goto done;
	}
	else if(r == 13){	/* toggle AllParts bit */
	    if(rflags){
		if(*rflags & GER_ALLPARTS){
		    *rflags &= ~GER_ALLPARTS;
		    opts[allparts].label = "AllParts";
		}
		else{
		    *rflags |=  GER_ALLPARTS;
		    opts[allparts].label = "NoAllParts";
		}
	    }

	    continue;
	}
#if	0
	else if(r == 14){	/* List file names matching partial? */
	    continue;
	}
#endif
        else if(r == 1){	/* Cancel */
	    ret = -1;
	    goto done;
        }
        else if(r == 4){
	    continue;
	}
	else if(r != 0){
	    Writechar(BELL, 0);
	    continue;
	}

        removing_leading_and_trailing_white_space(filename);

	if(!*filename){
	    if(!*def){		/* Cancel */
		ret = -1;
		goto done;
	    }
	    
	    strncpy(filename, def, len-1);
	    filename[len-1] = '\0';
	}

#if	defined(DOS) || defined(OS2)
	if(is_absolute_path(filename)){
	    fixpath(filename, len);
	}
#else
	if(filename[0] == '~'){
	    if(fnexpand(filename, len) == NULL){
		char *p = strindex(filename, '/');
		if(p != NULL)
		  *p = '\0';
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
			  "Error expanding file name: \"%.200s\" unknown user",
			      filename);
		continue;
	    }
	}
#endif

	if(is_absolute_path(filename)){
	    strncpy(full_filename, filename, len-1);
	    full_filename[len-1] = '\0';
	}
	else{
	    if(!dir[0])
	      build_path(full_filename, (char *)getcwd(dir,sizeof(dir)),
			 filename, len);
	    else if(dir[0] == '~' && !dir[1])
	      build_path(full_filename, ps->home_dir, filename, len);
	    else
	      build_path(full_filename, dir, filename, len);
	}

        if((ill = filter_filename(full_filename, &fatal)) != NULL){
	    if(fatal){
		q_status_message1(SM_ORDER | SM_DING, 3, 3, "%.200s", ill);
		continue;
	    }
	    else{
/* BUG: we should beep when the key's pressed rather than bitch later */
		/* Warn and ask for confirmation. */
		sprintf(prompt_buf, "File name contains a '%s'.  %s anyway",
			ill, (flags & GE_IS_EXPORT) ? "Export" : "Save");
		if(want_to(prompt_buf, 'n', 0, NO_HELP,
		  ((flags & GE_SEQ_SENSITIVE) ? RB_SEQ_SENSITIVE : 0)) != 'y')
		  continue;
	    }
	}

	break;		/* Must have got an OK file name */
    }

    if(VAR_OPER_DIR && !in_dir(VAR_OPER_DIR, full_filename)){
	ret = -2;
	goto done;
    }

    if(!can_access(full_filename, ACCESS_EXISTS)){
	int rbflags;
	static ESCKEY_S access_opts[] = {
	    {'o', 'o', "O", "Overwrite"},
	    {'a', 'a', "A", "Append"},
	    {-1, 0, NULL, NULL}};

	rbflags = RB_NORM | ((flags & GE_SEQ_SENSITIVE) ? RB_SEQ_SENSITIVE : 0);

	if(flags & GE_NO_APPEND){
	    r = strlen(filename);
	    sprintf(prompt_buf,
		   "File \"%s%.*s\" already exists.  Overwrite it ",
		   (r > 20) ? "..." : "",
		   sizeof(prompt_buf)-100,
		   filename + ((r > 20) ? r - 20 : 0));
	    if(want_to(prompt_buf, 'n', 'x', NO_HELP, rbflags) == 'y'){
		if(rflags)
		  *rflags |= GER_OVER;

		if(unlink(full_filename) < 0){
		    q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  "Cannot remove old %.200s: %.200s",
				  full_filename, error_description(errno));
		}
	    }
	    else{
		ret = -1;
		goto done;
	    }
	}
	else if(!(flags & GE_IS_IMPORT)){
	    r = strlen(filename);
	    sprintf(prompt_buf,
		   "File \"%s%.*s\" already exists.  Overwrite or append it ? ",
		   (r > 20) ? "..." : "",
		   sizeof(prompt_buf)-100,
		   filename + ((r > 20) ? r - 20 : 0));
	    switch(radio_buttons(prompt_buf, -FOOTER_ROWS(ps_global),
				 access_opts, 'a', 'x', NO_HELP, rbflags,NULL)){
	      case 'o' :
		if(rflags)
		  *rflags |= GER_OVER;

		if(truncate(full_filename, (off_t)0) < 0)
		  /* trouble truncating, but we'll give it a try anyway */
		  q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  "Warning: Cannot truncate old %.200s: %.200s",
				  full_filename, error_description(errno));
		break;

	      case 'a' :
		if(rflags)
		  *rflags |= GER_APPEND;

		break;

	      case 'x' :
	      default :
		ret = -1;
		goto done;
	    }
	}
    }

done:
    if(opts && opts != optsarg)
      fs_give((void **) &opts);

    return(ret);
}


/*----------------------------------------------------------------------
  parse the config'd upload/download command

  Args: cmd -- buffer to return command fit for shellin'
	prefix --
	cfg_str --
	fname -- file name to build into the command

  Returns: pointer to cmd_str buffer or NULL on real bad error

  NOTE: One SIDE EFFECT is that any defined "prefix" string in the
	cfg_str is written to standard out right before a successful
	return of this function.  The call immediately following this
	function darn well better be the shell exec...
 ----*/
char *
build_updown_cmd(cmd, prefix, cfg_str, fname)
    char *cmd;
    char *prefix;
    char *cfg_str;
    char *fname;
{
    char *p;
    int   fname_found = 0;

    if(prefix && *prefix){
	/* loop thru replacing all occurances of _FILE_ */
	for(p = strcpy(cmd, prefix); (p = strstr(p, "_FILE_")); )
	  rplstr(p, 6, fname);

	fputs(cmd, stdout);
    }

    /* loop thru replacing all occurances of _FILE_ */
    for(p = strcpy(cmd, cfg_str); (p = strstr(p, "_FILE_")); ){
	rplstr(p, 6, fname);
	fname_found = 1;
    }

    if(!fname_found)
      sprintf(cmd + strlen(cmd), " %s", fname);

    dprint(4, (debugfile, "\n - build_updown_cmd = \"%s\" -\n",
	   cmd ? cmd : "?"));
    return(cmd);
}






/*----------------------------------------------------------------------
  Write a berzerk format message delimiter using the given putc function

    Args: e -- envelope of message to write
	  pc -- function to use 

    Returns: TRUE if we could write it, FALSE if there was a problem

    NOTE: follows delimiter with OS-dependent newline
 ----*/
int
bezerk_delimiter(env, mc, pc, leading_newline)
    ENVELOPE *env;
    MESSAGECACHE *mc;
    gf_io_t   pc;
    int	      leading_newline;
{
    MESSAGECACHE telt;
    time_t       when;
    char        *p;
    
    /* write "[\n]From mailbox[@host] " */
    if(!((leading_newline ? gf_puts(NEWLINE, pc) : 1)
	 && gf_puts("From ", pc)
	 && gf_puts((env && env->from) ? env->from->mailbox
				       : "the-concourse-on-high", pc)
	 && gf_puts((env && env->from && env->from->host) ? "@" : "", pc)
	 && gf_puts((env && env->from && env->from->host) ? env->from->host
							  : "", pc)
	 && (*pc)(' ')))
      return(0);

    if(mc && mc->valid)
      when = mail_longdate(mc);
    else if(env && env->date && env->date[0]
	    && mail_parse_date(&telt,env->date))
      when = mail_longdate(&telt);
    else
      when = time(0);

    p = ctime(&when);

    while(p && *p && *p != '\n')	/* write date */
      if(!(*pc)(*p++))
	return(0);

    if(!gf_puts(NEWLINE, pc))		/* write terminating newline */
      return(0);

    return(1);
}



/*----------------------------------------------------------------------
      Execute command to jump to a given message number

    Args: qline -- Line to ask question on

  Result: returns true if the use selected a new message, false otherwise

 ----*/
long
jump_to(msgmap, qline, first_num, sparms, in_index)
    MSGNO_S  *msgmap;
    int       qline, first_num;
    SCROLL_S *sparms;
    CmdWhere  in_index;
{
    char     jump_num_string[80], *j, prompt[70];
    HelpType help;
    int      rc;
    static ESCKEY_S jump_to_key[] = { {0, 0, NULL, NULL},
				      {ctrl('Y'), 10, "^Y", "First Msg"},
				      {ctrl('V'), 11, "^V", "Last Msg"},
				      {-1, 0, NULL, NULL} };

    dprint(4, (debugfile, "\n - jump_to -\n"));

#ifdef DEBUG
    if(sparms && sparms->jump_is_debug)
      return(get_level(qline, first_num, sparms));
#endif

    if(!any_messages(msgmap, NULL, "to Jump to"))
      return(0L);

    if(first_num && isdigit((unsigned char) first_num)){
	jump_num_string[0] = first_num;
	jump_num_string[1] = '\0';
    }
    else
      jump_num_string[0] = '\0';

    if(mn_total_cur(msgmap) > 1L){
	sprintf(prompt, "Unselect %.20s msgs in favor of number to be entered", 
		comatose(mn_total_cur(msgmap)));
	if((rc = want_to(prompt, 'n', 0, NO_HELP, WT_NORM)) == 'n')
	  return(0L);
    }

    sprintf(prompt, "%.10s number to jump to : ", in_index == ThrdIndx
						    ? "Thread"
						    : "Message");

    help = NO_HELP;
    while(1){
	int flags = OE_APPEND_CURRENT;

        rc = optionally_enter(jump_num_string, qline, 0,
                              sizeof(jump_num_string), prompt,
                              jump_to_key, help, &flags);
        if(rc == 3){
            help = help == NO_HELP
			? (in_index == ThrdIndx ? h_oe_jump_thd : h_oe_jump)
			: NO_HELP;
            continue;
        }
	else if(rc == 10 || rc == 11){
	    char warning[100];
	    long closest;

	    closest = closest_jump_target(rc == 10 ? 1L
					  : ((in_index == ThrdIndx)
					     ? msgmap->max_thrdno
					     : mn_get_total(msgmap)),
					  ps_global->mail_stream,
					  msgmap, 0,
					  in_index, warning);
	    /* ignore warning */
	    return(closest);
	}

	/*
	 * If we take out the *jump_num_string nonempty test in this if
	 * then the closest_jump_target routine will offer a jump to the
	 * last message. However, it is slow because you have to wait for
	 * the status message and it is annoying for people who hit J command
	 * by mistake and just want to hit return to do nothing, like has
	 * always worked. So the test is there for now. Hubert 2002-08-19
	 *
	 * Jumping to first/last message is now possible through ^Y/^V 
	 * commands above. jpf 2002-08-21
	 */
        if(rc == 0 && *jump_num_string != '\0'){
	    removing_trailing_white_space(jump_num_string);
	    removing_leading_white_space(jump_num_string);
            for(j=jump_num_string; isdigit((unsigned char)*j) || *j=='-'; j++)
	      ;

	    if(*j != '\0'){
	        q_status_message(SM_ORDER | SM_DING, 2, 2,
                           "Invalid number entered. Use only digits 0-9");
		jump_num_string[0] = '\0';
	    }
	    else{
		char warning[100];
		long closest, jump_num;

		if(*jump_num_string)
		  jump_num = atol(jump_num_string);
		else
		  jump_num = -1L;

		warning[0] = '\0';
		closest = closest_jump_target(jump_num, ps_global->mail_stream,
					      msgmap,
					      *jump_num_string ? 0 : 1,
					      in_index, warning);
		if(warning[0])
		  q_status_message(SM_ORDER | SM_DING, 2, 2, warning);

		if(closest == jump_num)
		  return(jump_num);

		if(closest == 0L)
		  jump_num_string[0] = '\0';
		else
		  strncpy(jump_num_string, long2string(closest),
			  sizeof(jump_num_string));
            }

            continue;
	}

        if(rc != 4)
          break;
    }

    return(0L);
}


#ifdef DEBUG
long
get_level(qline, first_num, sparms)
    int      qline, first_num;
    SCROLL_S *sparms;
{
    char     debug_num_string[80], *j, prompt[70];
    HelpType help;
    int      rc;
    long     debug_num;

    if(first_num && isdigit((unsigned char)first_num)){
	debug_num_string[0] = first_num;
	debug_num_string[1] = '\0';
	debug_num = atol(debug_num_string);
	*(int *)(sparms->proc.data.p) = debug_num;
	q_status_message1(SM_ORDER, 0, 3, "Show debug <= level %.200s",
			  comatose(debug_num));
	return(1L);
    }
    else
      debug_num_string[0] = '\0';

    sprintf(prompt, "Show debug <= this level (0-%d) : ", max(debug, 9));

    help = NO_HELP;
    while(1){
	int flags = OE_APPEND_CURRENT;

        rc = optionally_enter(debug_num_string, qline, 0,
                              sizeof(debug_num_string), prompt,
                              NULL, help, &flags);
        if(rc == 3){
            help = help == NO_HELP ? h_oe_debuglevel : NO_HELP;
            continue;
        }

        if(rc == 0){
	    removing_leading_and_trailing_white_space(debug_num_string);
            for(j=debug_num_string; isdigit((unsigned char)*j); j++)
	      ;

	    if(*j != '\0'){
	        q_status_message(SM_ORDER | SM_DING, 2, 2,
                           "Invalid number entered. Use only digits 0-9");
		debug_num_string[0] = '\0';
	    }
	    else{
		debug_num = atol(debug_num_string);
		if(debug_num < 0)
	          q_status_message(SM_ORDER | SM_DING, 2, 2,
				   "Number should be >= 0");
		else if(debug_num > max(debug,9))
	          q_status_message1(SM_ORDER | SM_DING, 2, 2,
				   "Maximum is %.200s", comatose(max(debug,9)));
		else{
		    *(int *)(sparms->proc.data.p) = debug_num;
		    q_status_message1(SM_ORDER, 0, 3,
				      "Show debug <= level %.200s",
				      comatose(debug_num));
		    return(1L);
		}
            }

            continue;
	}

        if(rc != 4)
          break;
    }

    return(0L);
}
#endif /* DEBUG */


/*
 * Returns the message number closest to target that isn't hidden.
 * Make warning at least 100 chars.
 * A return of 0 means there is no message to jump to.
 */
long
closest_jump_target(target, stream, msgmap, no_target, in_index, warning)
    long        target;
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    int         no_target;
    CmdWhere    in_index;
    char       *warning;
{
    long i, start, closest = 0L;
    char buf[80];
    long maxnum;

    warning[0] = '\0';
    maxnum = (in_index == ThrdIndx) ? msgmap->max_thrdno : mn_get_total(msgmap);

    if(no_target){
	target = maxnum;
	start = 1L;
	sprintf(warning, "No %.10s number entered, jump to end? ",
		(in_index == ThrdIndx) ? "thread" : "message");
    }
    else if(target < 1L)
      start = 1L - target;
    else if(target > maxnum)
      start = target - maxnum;
    else
      start = 1L;

    if(target > 0L && target <= maxnum)
      if(in_index == ThrdIndx
	 || !msgline_hidden(stream, msgmap, target, 0))
	return(target);

    for(i = start; target+i <= maxnum || target-i > 0L; i++){

	if(target+i > 0L && target+i <= maxnum &&
	   (in_index == ThrdIndx
	    || !msgline_hidden(stream, msgmap, target+i, 0))){
	    closest = target+i;
	    break;
	}

	if(target-i > 0L && target-i <= maxnum &&
	   (in_index == ThrdIndx
	    || !msgline_hidden(stream, msgmap, target-i, 0))){
	    closest = target-i;
	    break;
	}
    }

    strncpy(buf, long2string(closest), sizeof(buf));
    buf[sizeof(buf)-1] = '\0';

    if(closest == 0L)
      strcpy(warning, "Nothing to jump to");
    else if(target < 1L)
      sprintf(warning, "%.10s number (%.20s) must be at least %.20s",
	      (in_index == ThrdIndx) ? "Thread" : "Message",
	      long2string(target), buf);
    else if(target > maxnum)
      sprintf(warning, "%.10s number (%.20s) may be no more than %.20s",
	      (in_index == ThrdIndx) ? "Thread" : "Message",
	      long2string(target), buf);
    else if(!no_target)
      sprintf(warning,
	"Message number (%.20s) is not in \"Zoomed Index\" - Closest is(%.20s)",
	long2string(target), buf);

    return(closest);
}


/*----------------------------------------------------------------------
     Prompt for folder name to open, expand the name and return it

   Args: qline      -- Screen line to prompt on
         allow_list -- if 1, allow ^T to bring up collection lister

 Result: returns the folder name or NULL
         pine structure mangled_footer flag is set
         may call the collection lister in which case mangled screen will be set

 This prompts the user for the folder to open, possibly calling up
the collection lister if the user types ^T.
----------------------------------------------------------------------*/
char *
broach_folder(qline, allow_list, context)
    int qline, allow_list;
    CONTEXT_S **context;
{
    HelpType	help;
    static char newfolder[MAILTMPLEN];
    char        expanded[MAXPATH+1],
                prompt[MAX_SCREEN_COLS+1],
               *last_folder;
    CONTEXT_S  *tc, *tc2;
    ESCKEY_S    ekey[8];
    int		rc, n, flags, last_rc = 0, inbox, done = 0;

    /*
     * the idea is to provide a clue for the context the file name
     * will be saved in (if a non-imap names is typed), and to
     * only show the previous if it was also in the same context
     */
    help	   = NO_HELP;
    *expanded	   = '\0';
    *newfolder	   = '\0';
    last_folder	   = NULL;

    /*
     * There are three possibilities for the prompt's offered default.
     *  1) always the last folder visited
     *  2) if non-inbox current, inbox else last folder visited
     *  3) if non-inbox current, inbox else last folder visited in
     *     the first collection
     */
    if(ps_global->goto_default_rule == GOTO_LAST_FLDR){
	tc = (context && *context) ? *context : ps_global->context_current;
	inbox = 1;		/* fill in last_folder below */
    }
    else if(ps_global->goto_default_rule == GOTO_FIRST_CLCTN){
	tc = (ps_global->context_list->use & CNTXT_INCMNG)
	  ? ps_global->context_list->next : ps_global->context_list;
	ps_global->last_unambig_folder[0] = '\0';
    }
    else if(ps_global->goto_default_rule == GOTO_FIRST_CLCTN_DEF_INBOX){
	tc = (ps_global->context_list->use & CNTXT_INCMNG)
	  ? ps_global->context_list->next : ps_global->context_list;
	tc->last_folder[0] = '\0';
	inbox = 0;
	ps_global->last_unambig_folder[0] = '\0';
    }
    else{
	inbox = strucmp(ps_global->cur_folder,ps_global->inbox_name) == 0;
	if(!inbox)
	  tc = ps_global->context_list;		/* inbox's context */
	else if(ps_global->goto_default_rule == GOTO_INBOX_FIRST_CLCTN){
	    tc = (ps_global->context_list->use & CNTXT_INCMNG)
		  ? ps_global->context_list->next : ps_global->context_list;
	    ps_global->last_unambig_folder[0] = '\0';
	}
	else
	  tc = (context && *context) ? *context : ps_global->context_current;
    }

    /* set up extra command option keys */
    rc = 0;
    ekey[rc].ch	     = (allow_list) ? ctrl('T') : 0 ;
    ekey[rc].rval    = (allow_list) ? 2 : 0;
    ekey[rc].name    = (allow_list) ? "^T" : "";
    ekey[rc++].label = (allow_list) ? "ToFldrs" : "";

    if(ps_global->context_list->next){
	ekey[rc].ch      = ctrl('P');
	ekey[rc].rval    = 10;
	ekey[rc].name    = "^P";
	ekey[rc++].label = "Prev Collection";

	ekey[rc].ch      = ctrl('N');
	ekey[rc].rval    = 11;
	ekey[rc].name    = "^N";
	ekey[rc++].label = "Next Collection";
    }

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	ekey[rc].ch      = TAB;
	ekey[rc].rval    = 12;
	ekey[rc].name    = "TAB";
	ekey[rc++].label = "Complete";
    }

    if(F_ON(F_ENABLE_SUB_LISTS, ps_global)){
	ekey[rc].ch      = ctrl('X');
	ekey[rc].rval    = 14;
	ekey[rc].name    = "^X";
	ekey[rc++].label = "ListMatches";
    }

    if(ps_global->context_list->next){
	ekey[rc].ch      = KEY_UP;
	ekey[rc].rval    = 10;
	ekey[rc].name    = "";
	ekey[rc++].label = "";

	ekey[rc].ch      = KEY_DOWN;
	ekey[rc].rval    = 11;
	ekey[rc].name    = "";
	ekey[rc++].label = "";
    }

    ekey[rc].ch = -1;

    while(!done) {
	/*
	 * Figure out next default value for this context.  The idea
	 * is that in each context the last folder opened is cached.
	 * It's up to pick it out and display it.  This is fine
	 * and dandy if we've currently got the inbox open, BUT
	 * if not, make the inbox the default the first time thru.
	 */
	if(!inbox){
	    last_folder = ps_global->inbox_name;
	    inbox = 1;		/* pretend we're in inbox from here on out */
	}
	else
	  last_folder = (ps_global->last_unambig_folder[0])
			  ? ps_global->last_unambig_folder
			  : ((tc->last_folder[0]) ? tc->last_folder : NULL);

	if(last_folder)
	  sprintf(expanded, " [%.*s]", sizeof(expanded)-5, last_folder);
	else
	  *expanded = '\0';

	/* only show collection number if more than one available */
	if(ps_global->context_list->next){
	    sprintf(prompt, "GOTO %s in <%.20s> %.*s%s: ",
		    NEWS_TEST(tc) ? "news group" : "folder",
		    tc->nickname, sizeof(prompt)-50, expanded,
		    *expanded ? " " : "");
	}
	else
	  sprintf(prompt, "GOTO folder %.*s%s: ", sizeof(prompt)-20, expanded,
		  *expanded ? " " : "");

	flags = OE_APPEND_CURRENT;
        rc = optionally_enter(newfolder, qline, 0, sizeof(newfolder),
			      prompt, ekey, help, &flags);

	ps_global->mangled_footer = 1;

	switch(rc){
	  case -1 :				/* o_e says error! */
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Error reading folder name");
	    return(NULL);

	  case 0 :				/* o_e says normal entry */
	    removing_trailing_white_space(newfolder);
	    removing_leading_white_space(newfolder);

	    if(*newfolder){
		char *name, *fullname = NULL;
		int   exists, breakout = 0;

		if(!(name = folder_is_nick(newfolder, FOLDERS(tc),
					   FN_WHOLE_NAME)))
		  name = newfolder;

		if(update_folder_spec(expanded, name)){
		    strncpy(name = newfolder, expanded, sizeof(newfolder));
		    newfolder[sizeof(newfolder)-1] = '\0';
		}

		exists = folder_name_exists(tc, name, &fullname);

		if(fullname){
		    strncpy(name = newfolder, fullname, sizeof(newfolder));
		    newfolder[sizeof(newfolder)-1] = '\0';
		    fs_give((void **) &fullname);
		    breakout = TRUE;
		}

		/*
		 * if we know the things a folder, open it.
		 * else if we know its a directory, visit it.
		 * else we're not sure (it either doesn't really
		 * exist or its unLISTable) so try opening it anyway
		 */
		if(exists & FEX_ISFILE){
		    done++;
		    break;
		}
		else if((exists & FEX_ISDIR)){
		    if(breakout){
			CONTEXT_S *fake_context;
			char	   tmp[MAILTMPLEN];
			size_t	   l;

			strncpy(tmp, name, sizeof(tmp)-2);
			tmp[sizeof(tmp)-2-1] = '\0';
			if(tmp[(l = strlen(tmp)) - 1] != tc->dir->delim){
			    tmp[l] = tc->dir->delim;
			    strcpy(&tmp[l+1], "[]");
			}
			else
			  strcat(tmp, "[]");

			fake_context = new_context(tmp, 0);
			newfolder[0] = '\0';
			done = display_folder_list(&fake_context, newfolder,
						   1, folders_for_goto);
			free_context(&fake_context);
			break;
		    }
		    else if(!(tc->use & CNTXT_INCMNG)){
			done = display_folder_list(&tc, newfolder,
						   1, folders_for_goto);
			break;
		    }
		}
		else if((exists & FEX_ERROR)){
		    q_status_message1(SM_ORDER, 0, 3,
				      "Problem accessing folder \"%.200s\"",
				      newfolder);
		    return(NULL);
		}
		else{
		    done++;
		    break;
		}

		if(exists == FEX_ERROR)
		  q_status_message1(SM_ORDER, 0, 3,
				    "Problem accessing folder \"%.200s\"",
				    newfolder);
		else if(tc->use & CNTXT_INCMNG)
		  q_status_message1(SM_ORDER, 0, 3,
				    "Can't find Incoming Folder: %.200s",
				    newfolder);
		else if(context_isambig(newfolder))
		  q_status_message3(SM_ORDER, 0, 3,
				    "Can't find folder \"%.200s\" in %.*s",
				    newfolder, (void *) 50, tc->nickname);
		else
		  q_status_message1(SM_ORDER, 0, 3,
				    "Can't find folder \"%.200s\"",
				    newfolder);

		return(NULL);
	    }
	    else if(last_folder){
		strncpy(newfolder, last_folder, sizeof(newfolder));
		newfolder[sizeof(newfolder)-1] = '\0';
		done++;
		break;
	    }
	    /* fall thru like they cancelled */

	  case 1 :				/* o_e says user cancel */
	    cmd_cancelled("Open folder");
	    return(NULL);

	  case 2 :				/* o_e says user wants list */
	    if(display_folder_list(&tc, newfolder, 0, folders_for_goto))
	      done++;

	    break;

	  case 3 :				/* o_e says user wants help */
	    help = help == NO_HELP ? h_oe_broach : NO_HELP;
	    break;

	  case 4 :				/* redraw */
	    break;
	    
	  case 10 :				/* Previous collection */
	    tc2 = ps_global->context_list;
	    while(tc2->next && tc2->next != tc)
	      tc2 = tc2->next;

	    tc = tc2;
	    break;

	  case 11 :				/* Next collection */
	    tc = (tc->next) ? tc->next : ps_global->context_list;
	    break;

	  case 12 :				/* file name completion */
	    if(!folder_complete(tc, newfolder, &n)){
		if(n && last_rc == 12 && !(flags & OE_USER_MODIFIED)){
		    if(display_folder_list(&tc, newfolder, 1,folders_for_goto))
		      done++;			/* bingo! */
		    else
		      rc = 0;			/* burn last_rc */
		}
		else
		  Writechar(BELL, 0);
	    }

	    break;

	  case 14 :				/* file name completion */
	    if(display_folder_list(&tc, newfolder, 2, folders_for_goto))
	      done++;			/* bingo! */
	    else
	      rc = 0;			/* burn last_rc */

	    break;

	  default :
	    panic("Unhandled case");
	    break;
	}

	last_rc = rc;
    }

    dprint(2, (debugfile, "broach folder, name entered \"%s\"\n",
	   newfolder ? newfolder : "?"));

    /*-- Just check that we can expand this. It gets done for real later --*/
    strncpy(expanded, newfolder, sizeof(expanded));
    expanded[sizeof(expanded)-1] = '\0';
    if (! expand_foldername(expanded, sizeof(expanded))) {
        dprint(1, (debugfile,
                    "Error: Failed on expansion of filename %s (save)\n", 
    	  expanded ? expanded : "?"));
        return(NULL);
    }

    *context = tc;
    return(newfolder);
}


/*----------------------------------------------------------------------
    Check to see if user input is in form of old c-client mailbox speck

  Args: old --
	new -- 

 Result:  1 if the folder was successfully updatedn
          0 if not necessary
      
  ----*/
int
update_folder_spec(new, old)
    char *new, *old;
{
    char *p;
    int	  nntp = 0;

    if(*(p = old) == '*')		/* old form? */
      old++;

    if(*old == '{')			/* copy host spec */
      do
	switch(*new = *old++){
	  case '\0' :
	    return(FALSE);

	  case '/' :
	    if(!struncmp(old, "nntp", 4))
	      nntp++;

	    break;

	  default :
	    break;
	}
      while(*new++ != '}');

    if((*p == '*' && *old) || ((*old == '*') ? *++old : 0)){
	/*
	 * OK, some heuristics here.  If it looks like a newsgroup
	 * then we plunk it into the #news namespace else we
	 * assume that they're trying to get at a #public folder...
	 */
	for(p = old;
	    *p && (isalnum((unsigned char) *p) || strindex(".-", *p));
	    p++)
	  ;

	sstrcpy(&new, (*p && !nntp) ? "#public/" : "#news.");
	strcpy(new, old);
	return(TRUE);
    }

    return(FALSE);
}


/*----------------------------------------------------------------------
    Actually attempt to open given folder 

  Args: newfolder -- The folder name to open
        streamp   -- Candidate stream for recycling. This stream will either
	             be re-used, or it will be closed.

 Result:  1 if the folder was successfully opened
          0 if the folder open failed and went back to old folder
         -1 if open failed and no folder is left open
      
  Attempt to open the folder name given. If the open of the new folder
  fails then the previously open folder will remain open, unless
  something really bad has happened. The designate inbox will always be
  kept open, and when a request to open it is made the already open
  stream will be used. Making a folder the current folder requires
  setting the following elements of struct pine: mail_stream, cur_folder,
  current_msgno, max_msgno.

  The first time the inbox folder is opened, usually as Pine starts up,
  it will be actually opened.
  ----*/

do_broach_folder(newfolder, new_context, streamp, flags) 
    char        *newfolder;
    CONTEXT_S   *new_context;
    MAILSTREAM **streamp;
    unsigned long flags;
{
    MAILSTREAM *m, *strm, *stream = streamp ? *streamp : NULL;
    int         open_inbox, rv, old_tros, we_cancel = 0,
                do_reopen = 0, n, was_dead = 0, cur_already_set = 0;
    char        expanded_file[max(MAXPATH,MAILTMPLEN)+1],
	       *old_folder, *old_path, *p;
    long        openmode, rflags = 0L, pc = 0L, cur, raw;
    ENVELOPE   *env = NULL;
    char        status_msg[81];
    SortOrder	old_sort;
    SEARCHSET  *srchset = NULL;
    unsigned    perfolder_startup_rule;
    char        tmp1[MAILTMPLEN], tmp2[MAILTMPLEN], *lname, *mname;

#if	defined(DOS) && !defined(WIN32)
    openmode = OP_SHORTCACHE | SP_USERFLDR;
#else
    openmode = SP_USERFLDR;
#endif

    dprint(1, (debugfile, "About to open folder \"%s\"    inbox is: \"%s\"\n",
	       newfolder ? newfolder : "?",
	       ps_global->inbox_name ? ps_global->inbox_name : "?"));

    was_dead = sp_a_locked_stream_is_dead();

    /*----- Little to do to if reopening same folder -----*/
    if(new_context == ps_global->context_current && ps_global->mail_stream
       && strcmp(newfolder, ps_global->cur_folder) == 0){
	if(stream){
	    pine_mail_close(stream);	/* don't need it */
	    stream = NULL;
	}

	if(sp_dead_stream(ps_global->mail_stream))
	  do_reopen++;
	
	/*
	 * If it is a stream which could probably discover newmail by
	 * reopening and user has YES set for those streams, or it
	 * is a stream which may discover newmail by reopening and
	 * user has YES set for those stream, then do_reopen.
	 */
	if(!do_reopen
	   &&
	   (((ps_global->mail_stream->dtb
	      && ((ps_global->mail_stream->dtb->flags & DR_NONEWMAIL)
		  || (ps_global->mail_stream->rdonly
		      && ps_global->mail_stream->dtb->flags
					      & DR_NONEWMAILRONLY)))
	     && (ps_global->reopen_rule == REOPEN_YES_YES
	         || ps_global->reopen_rule == REOPEN_YES_ASK_Y
	         || ps_global->reopen_rule == REOPEN_YES_ASK_N
	         || ps_global->reopen_rule == REOPEN_YES_NO))
	    ||
	    ((ps_global->mail_stream->dtb
	      && ps_global->mail_stream->rdonly
	      && !(ps_global->mail_stream->dtb->flags & DR_LOCAL))
	     && (ps_global->reopen_rule == REOPEN_YES_YES))))
	  do_reopen++;

	/*
	 * If it is a stream which could probably discover newmail by
	 * reopening and user has ASK set for those streams, or it
	 * is a stream which may discover newmail by reopening and
	 * user has ASK set for those stream, then ask.
	 */
	if(!do_reopen
	   &&
	   (((ps_global->mail_stream->dtb
	      && ((ps_global->mail_stream->dtb->flags & DR_NONEWMAIL)
		  || (ps_global->mail_stream->rdonly
		      && ps_global->mail_stream->dtb->flags
					      & DR_NONEWMAILRONLY)))
	     && (ps_global->reopen_rule == REOPEN_ASK_ASK_Y
	         || ps_global->reopen_rule == REOPEN_ASK_ASK_N
	         || ps_global->reopen_rule == REOPEN_ASK_NO_Y
	         || ps_global->reopen_rule == REOPEN_ASK_NO_N))
	    ||
	    ((ps_global->mail_stream->dtb
	      && ps_global->mail_stream->rdonly
	      && !(ps_global->mail_stream->dtb->flags & DR_LOCAL))
	     && (ps_global->reopen_rule == REOPEN_YES_ASK_Y
	         || ps_global->reopen_rule == REOPEN_YES_ASK_N
	         || ps_global->reopen_rule == REOPEN_ASK_ASK_Y
	         || ps_global->reopen_rule == REOPEN_ASK_ASK_N)))){
	    int deefault;

	    switch(ps_global->reopen_rule){
	      case REOPEN_YES_ASK_Y:
	      case REOPEN_ASK_ASK_Y:
	      case REOPEN_ASK_NO_Y:
		deefault = 'y';
		break;

	      default:
		deefault = 'n';
		break;
	    }

	    switch(want_to("Re-open folder to check for new messages", deefault,
			   'x', h_reopen_folder, WT_NORM)){
	      case 'y':
	        do_reopen++;
		break;
	    
	      case 'n':
		break;

	      case 'x':
		cmd_cancelled(NULL);
		return(0);
	    }
	}

	if(do_reopen){
	    /*
	     * If it's not healthy or if the user explicitly wants to
	     * do a reopen, we reset things and fall thru
	     * to actually reopen it.
	     */
	    if(sp_dead_stream(ps_global->mail_stream)){
		dprint(2, (debugfile, "Stream was dead, reopening \"%s\"\n",
				      newfolder ? newfolder : "?"));
	    }

	    /* clean up */
	    pine_mail_actually_close(ps_global->mail_stream);
	    ps_global->mangled_header = 1;
	    clear_index_cache();
	}
	else{
	    if(!(flags & DB_NOVISIT))
	      sp_set_recent_since_visited(ps_global->mail_stream, 0L);

	    return(1);			/* successful open of same folder! */
	}
    }

    /*--- Set flag that we're opening the inbox, a special case ---*/
    /*
     * We want to know if inbox is being opened either by name OR
     * fully qualified path...
     *
     * So, IF we're asked to open inbox AND it's already open AND
     * the only stream AND it's healthy, just return ELSE fall thru
     * and close mail_stream returning with inbox_stream as new stream...
     */
    if(open_inbox = (strucmp(newfolder, ps_global->inbox_name) == 0
		     || strcmp(newfolder, ps_global->VAR_INBOX_PATH) == 0
		     || same_remote_mailboxes(newfolder,
					      ps_global->VAR_INBOX_PATH)
		     || (!IS_REMOTE(newfolder)
			 && (lname=mailboxfile(tmp1,newfolder))
			 && (mname=mailboxfile(tmp2,ps_global->VAR_INBOX_PATH))
			 && !strcmp(lname,mname)))){
	if(sp_flagged(ps_global->mail_stream, SP_INBOX)){   /* already open */
	    if(stream)
	      pine_mail_close(stream);

	    if(!(flags & DB_NOVISIT))
	      sp_set_recent_since_visited(ps_global->mail_stream, 0L);

	    return(1);
	}

	new_context = ps_global->context_list; /* restore first context */
    }

    /*
     * If ambiguous foldername (not fully qualified), make sure it's
     * not a nickname for a folder in the given context...
     */

    /* might get reset below */
    strncpy(expanded_file, newfolder, sizeof(expanded_file));
    expanded_file[sizeof(expanded_file)-1] = '\0';

    if(!open_inbox && new_context && context_isambig(newfolder)){
	if (p = folder_is_nick(newfolder, FOLDERS(new_context), FN_WHOLE_NAME)){
	    strncpy(expanded_file, p, sizeof(expanded_file));
	    expanded_file[sizeof(expanded_file)-1] = '\0';
	    dprint(2, (debugfile, "broach_folder: nickname for %s is %s\n",
		       expanded_file ? expanded_file : "?",
		       newfolder ? newfolder : "?"));
	    /*
	     * It would be a dumb config to have an incoming folder other
	     * than INBOX also point to INBOX, but let's check for it
	     * anyway.
	     */
	    if(open_inbox =
		    (strucmp(expanded_file, ps_global->inbox_name) == 0
		     || strcmp(expanded_file, ps_global->VAR_INBOX_PATH) == 0
		     || same_remote_mailboxes(expanded_file,
					      ps_global->VAR_INBOX_PATH)
		     || (!IS_REMOTE(expanded_file)
			 && (lname=mailboxfile(tmp1,expanded_file))
			 && (mname=mailboxfile(tmp2,ps_global->VAR_INBOX_PATH))
			 && !strcmp(lname,mname)))){
		if(sp_flagged(ps_global->mail_stream, SP_INBOX)){
		    if(stream)
		      pine_mail_close(stream);

		    if(!(flags & DB_NOVISIT))
		      sp_set_recent_since_visited(ps_global->mail_stream, 0L);

		    q_status_message(SM_ORDER, 0, 3,
			    "Folder is same as INBOX. Remaining in INBOX");
		    dprint(2, (debugfile,
	"This was another name for INBOX, which is already open. return\n"));

		    return(1);
		}

		new_context = ps_global->context_list; /* restore 1st  cntxt */
	    }
	}
	else if ((new_context->use & CNTXT_INCMNG)
		 && (folder_index(newfolder, new_context, FI_FOLDER) < 0)
		 && !is_absolute_path(newfolder)){
	    q_status_message1(SM_ORDER, 3, 4,
			    "Can't find Incoming Folder %.200s.", newfolder);
	    if(stream)
	      pine_mail_close(stream);

	    return(0);
	}
	else{
	    char tmp3[MAILTMPLEN];

	    /*
	     * Check to see if we are opening INBOX using the folder name
	     * and a context. We won't have recognized this is the
	     * same as INBOX without applying the context first.
	     */
	    context_apply(tmp3, new_context, expanded_file, sizeof(tmp3));
	    if(open_inbox =
		    (strucmp(tmp3, ps_global->inbox_name) == 0
		     || strcmp(tmp3, ps_global->VAR_INBOX_PATH) == 0
		     || same_remote_mailboxes(tmp3,
					      ps_global->VAR_INBOX_PATH)
		     || (!IS_REMOTE(tmp3)
			 && (lname=mailboxfile(tmp1,tmp3))
			 && (mname=mailboxfile(tmp2,ps_global->VAR_INBOX_PATH))
			 && !strcmp(lname,mname)))){
		if(sp_flagged(ps_global->mail_stream, SP_INBOX)){
		    if(stream)
		      pine_mail_close(stream);

		    if(!(flags & DB_NOVISIT))
		      sp_set_recent_since_visited(ps_global->mail_stream, 0L);

		    q_status_message(SM_ORDER, 0, 3,
			    "Folder is same as INBOX. Remaining in INBOX");
		    dprint(2, (debugfile,
	"This was an alt name for INBOX, which is already open. returning\n"));

		    return(1);
		}

		new_context = ps_global->context_list; /* restore 1st  cntxt */
	    }
	}
    }

    /*--- Opening inbox, inbox has been already opened, the easy case ---*/
    /*
     * [ It is probably true that we could eliminate most of this special ]
     * [ inbox stuff and just get the inbox stream back when we do the    ]
     * [ context_open below, but figuring that out hasn't been done.      ]
     */
    if(open_inbox && (strm=sp_inbox_stream()) && !sp_dead_stream(strm)){
	if(ps_global->mail_stream
	   && (!sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
	       || (sp_flagged(ps_global->mail_stream, SP_INBOX)
	           && F_ON(F_EXPUNGE_INBOX, ps_global))
	       || (!sp_flagged(ps_global->mail_stream, SP_INBOX)
		   && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
	           && F_ON(F_EXPUNGE_STAYOPENS, ps_global))))
          expunge_and_close(ps_global->mail_stream, NULL,
			    sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
			      ? EC_NO_CLOSE : EC_NONE);
	else if(!sp_flagged(ps_global->mail_stream, SP_INBOX)
	        && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)){
	    /*
	     * We want to save our position in the folder so that when we
	     * come back to this folder again, we can place the cursor on
	     * a reasonable message number.
	     */
	    
	    sp_set_saved_cur_msg_id(ps_global->mail_stream, NULL);

	    if(ps_global->mail_stream->nmsgs > 0L){
		cur = mn_get_cur(sp_msgmap(ps_global->mail_stream));
		raw = mn_m2raw(sp_msgmap(ps_global->mail_stream), cur);
		if(raw > 0L && raw <= ps_global->mail_stream->nmsgs)
		  env = pine_mail_fetchstructure(ps_global->mail_stream,
						 raw, NULL);
		
		if(env && env->message_id && env->message_id[0])
		  sp_set_saved_cur_msg_id(ps_global->mail_stream,
					  env->message_id);
	    }
	}

	ps_global->mail_stream = strm;
	ps_global->msgmap      = sp_msgmap(strm);

	if(was_dead)
	  icon_text(NULL, IT_MCLOSED);

	dprint(7, (debugfile, "%ld %ld %x\n",
		   mn_get_cur(ps_global->msgmap),
                   mn_get_total(ps_global->msgmap),
		   ps_global->mail_stream));
	/*
	 * remember last context and folder
	 */
	if(context_isambig(ps_global->cur_folder)){
	    ps_global->context_last = ps_global->context_current;
	    strncpy(ps_global->context_current->last_folder,
		    ps_global->cur_folder,
		    sizeof(ps_global->context_current->last_folder)-1);
	    ps_global->context_current->last_folder[sizeof(ps_global->context_current->last_folder)-1] = '\0';
	    ps_global->last_unambig_folder[0] = '\0';
	}
	else{
	    ps_global->context_last = NULL;
	    strncpy(ps_global->last_unambig_folder, ps_global->cur_folder,
		    sizeof(ps_global->last_unambig_folder)-1);
	    ps_global->last_unambig_folder[sizeof(ps_global->last_unambig_folder)-1] = '\0';
	}

	strncpy(ps_global->cur_folder, sp_fldr(ps_global->mail_stream)
					 ? sp_fldr(ps_global->mail_stream)
					 : ps_global->inbox_name,
		sizeof(ps_global->cur_folder)-1);
	ps_global->cur_folder[sizeof(ps_global->cur_folder)-1] = '\0';
	ps_global->context_current = ps_global->context_list;
	reset_index_format();
	clear_index_cache();
        /* MUST sort before restoring msgno! */
	refresh_sort(ps_global->mail_stream, ps_global->msgmap, SRT_NON);
        q_status_message3(SM_ORDER, 0, 3,
			  "Opened folder \"%.200s\" with %.200s message%.200s",
			  ps_global->inbox_name, 
                          long2string(mn_get_total(ps_global->msgmap)),
			  plural(mn_get_total(ps_global->msgmap)));
#ifdef	_WINDOWS
	mswin_settitle(ps_global->inbox_name);
#endif
	if(stream)
	  pine_mail_close(stream);

	if(!(flags & DB_NOVISIT))
	  sp_set_recent_since_visited(ps_global->mail_stream, 0L);

	return(1);
    }
    else if(open_inbox && (strm=sp_inbox_stream()) && sp_dead_stream(strm)){
	/* 
	 * if dead INBOX, just close it and let it be reopened.
	 * This is different from the do_reopen case above,
	 * because we're going from another open mail folder to the
	 * dead INBOX.
	 */
	dprint(2, (debugfile, "INBOX was dead, closing before reopening\n"));
	pine_mail_actually_close(strm);
    }

    if(!new_context && !expand_foldername(expanded_file,sizeof(expanded_file))){
	if(stream)
	  pine_mail_close(stream);

	return(0);
    }

    /*
     * This is a safe time to clean up dead streams because nothing should
     * be referencing them right now.
     */
    sp_cleanup_dead_streams();

    old_folder = NULL;
    old_path   = NULL;
    old_sort   = SortArrival;			/* old sort */
    old_tros   = 0;				/* old reverse sort ? */
    /*---- now close the old one we had open if there was one ----*/
    if(ps_global->mail_stream != NULL){
        old_folder   = cpystr(ps_global->cur_folder);
        old_path     = cpystr(ps_global->mail_stream->original_mailbox
	                        ? ps_global->mail_stream->original_mailbox
				: ps_global->mail_stream->mailbox);
	old_sort     = mn_get_sort(ps_global->msgmap);
	old_tros     = mn_get_revsort(ps_global->msgmap);
	if(!sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
	    || (sp_flagged(ps_global->mail_stream, SP_INBOX)
	        && F_ON(F_EXPUNGE_INBOX, ps_global))
	    || (!sp_flagged(ps_global->mail_stream, SP_INBOX)
	        && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
	        && F_ON(F_EXPUNGE_STAYOPENS, ps_global)))
          expunge_and_close(ps_global->mail_stream, NULL,
			    sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
			      ? EC_NO_CLOSE : EC_NONE);
	else if(!sp_flagged(ps_global->mail_stream, SP_INBOX)
	        && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)){
	    /*
	     * We want to save our position in the folder so that when we
	     * come back to this folder again, we can place the cursor on
	     * a reasonable message number.
	     */
	    
	    sp_set_saved_cur_msg_id(ps_global->mail_stream, NULL);

	    if(ps_global->mail_stream->nmsgs > 0L){
		cur = mn_get_cur(sp_msgmap(ps_global->mail_stream));
		raw = mn_m2raw(sp_msgmap(ps_global->mail_stream), cur);
		if(raw > 0L && raw <= ps_global->mail_stream->nmsgs)
		  env = pine_mail_fetchstructure(ps_global->mail_stream,
						 raw, NULL);
		
		if(env && env->message_id && env->message_id[0])
		  sp_set_saved_cur_msg_id(ps_global->mail_stream,
					  env->message_id);
	    }
	}

	ps_global->mail_stream = NULL;
    }

    sprintf(status_msg, "%.3sOpening \"", do_reopen ? "Re-" : "");
    strncat(status_msg, pretty_fn(newfolder),
	    sizeof(status_msg)-strlen(status_msg) - 2);
    status_msg[sizeof(status_msg)-2] = '\0';
    strncat(status_msg, "\"", 1);
    status_msg[sizeof(status_msg)-1] = '\0';
    we_cancel = busy_alarm(1, status_msg, NULL, 1);

    /* 
     * if requested, make access to folder readonly (only once)
     */
    if(ps_global->open_readonly_on_startup){
	openmode |= OP_READONLY;
	ps_global->open_readonly_on_startup = 0;
    }

    openmode |= SP_USEPOOL;

    if(stream)
      sp_set_first_unseen(stream, 0L);

    m = context_open((new_context && !open_inbox) ? new_context : NULL,
		     stream, 
		     open_inbox ? ps_global->VAR_INBOX_PATH : expanded_file,
		     openmode | (open_inbox ? SP_INBOX : 0),
		     &rflags);

    if(streamp)
      *streamp = m;


    dprint(8, (debugfile, "Opened folder %p \"%s\" (context: \"%s\")\n",
               m, (m && m->mailbox) ? m->mailbox : "nil",
	       (new_context && new_context->context)
	         ? new_context->context : "nil"));


    /* Can get m != NULL if correct passwd for remote, but wrong name */
    if(m == NULL || m->halfopen){
	/*-- non-existent local mailbox, or wrong passwd for remote mailbox--*/
        /* fall back to currently open mailbox */
	if(we_cancel)
	  cancel_busy_alarm(-1);

	ps_global->mail_stream = NULL;
	if(m)
	  pine_mail_actually_close(m);

        rv = 0;
        dprint(8, (debugfile, "Old folder: \"%s\"\n",
               old_folder == NULL ? "" : old_folder));
        if(old_folder != NULL){
            if(strcmp(old_folder, ps_global->inbox_name) == 0){
                ps_global->mail_stream = sp_inbox_stream();
		ps_global->msgmap      = sp_msgmap(ps_global->mail_stream);

                dprint(8, (debugfile, "Reactivate inbox %ld %ld %p\n",
                           mn_get_cur(ps_global->msgmap),
                           mn_get_total(ps_global->msgmap),
                           ps_global->mail_stream));
		strncpy(ps_global->cur_folder,
			sp_fldr(ps_global->mail_stream)
			  ? sp_fldr(ps_global->mail_stream)
			  : ps_global->inbox_name,
			sizeof(ps_global->cur_folder)-1);
		ps_global->cur_folder[sizeof(ps_global->cur_folder)-1] = '\0';
            }
	    else{
                ps_global->mail_stream = pine_mail_open(NULL, old_path,
							openmode, &rflags);
                /* mm_log will take care of error message here */
                if(ps_global->mail_stream == NULL){
                    rv = -1;
                }
		else{
		    ps_global->msgmap = sp_msgmap(ps_global->mail_stream);
		    mn_set_sort(ps_global->msgmap, old_sort);
		    mn_set_revsort(ps_global->msgmap, old_tros);
		    ps_global->mangled_header = 1;
		    reset_index_format();
		    clear_index_cache();

		    if(!(rflags & SP_MATCH)){
			sp_set_expunge_count(ps_global->mail_stream, 0L);
			sp_set_new_mail_count(ps_global->mail_stream, 0L);
			sp_set_dead_stream(ps_global->mail_stream, 0);
			sp_set_noticed_dead_stream(ps_global->mail_stream, 0);

			reset_check_point(ps_global->mail_stream);
			if(IS_NEWS(ps_global->mail_stream)
			   && ps_global->mail_stream->rdonly)
			  msgno_exclude_deleted(ps_global->mail_stream,
					    sp_msgmap(ps_global->mail_stream));

			if(mn_get_total(ps_global->msgmap) > 0)
			  mn_set_cur(ps_global->msgmap,
				     first_sorted_flagged(F_NONE,
						      ps_global->mail_stream,
						      0L,
						      THREADING()
							  ? 0 : FSF_SKIP_CHID));

			if(!(mn_get_sort(ps_global->msgmap) == SortArrival
			     && !mn_get_revsort(ps_global->msgmap)))
			  refresh_sort(ps_global->mail_stream,
				       ps_global->msgmap, SRT_NON);
		    }

                    q_status_message1(SM_ORDER, 0, 3,
				      "Folder \"%.200s\" reopened", old_folder);
                }
            }

	    if(rv == 0)
	      mn_set_cur(ps_global->msgmap,
			 min(mn_get_cur(ps_global->msgmap), 
			     mn_get_total(ps_global->msgmap)));

            fs_give((void **)&old_folder);
            fs_give((void **)&old_path);
        }
	else
	  rv = -1;

        if(rv == -1){
            q_status_message(SM_ORDER | SM_DING, 0, 4, "No folder opened");
	    mn_set_total(ps_global->msgmap, 0L);
	    mn_set_nmsgs(ps_global->msgmap, 0L);
	    mn_set_cur(ps_global->msgmap, -1L);
	    ps_global->cur_folder[0] = '\0';
        }

	if(was_dead && !sp_a_locked_stream_is_dead())
	  icon_text(NULL, IT_MCLOSED);

	if(ps_global->mail_stream && !(flags & DB_NOVISIT))
	  sp_set_recent_since_visited(ps_global->mail_stream, 0L);

	if(ps_global->mail_stream)
	  sp_set_first_unseen(ps_global->mail_stream, 0L);

        return(rv);
    }
    else{
        if(old_folder != NULL){
            fs_give((void **)&old_folder);
            fs_give((void **)&old_path);
        }
    }

    /*----- success in opening the new folder ----*/
    dprint(2, (debugfile, "Opened folder \"%s\" with %ld messages\n",
	       m->mailbox ? m->mailbox : "?", m->nmsgs));


    /*--- A Little house keeping ---*/

    ps_global->mail_stream = m;
    if(!(flags & DB_NOVISIT))
      sp_set_recent_since_visited(ps_global->mail_stream, 0L);

    ps_global->msgmap = sp_msgmap(m);
    if(!(rflags & SP_MATCH)){
	sp_set_expunge_count(m, 0L);
	sp_set_new_mail_count(m, 0L);
	sp_set_dead_stream(m, 0);
	sp_set_noticed_dead_stream(m, 0);
	sp_set_mail_box_changed(m, 0);
	reset_check_point(m);
    }

    if(was_dead && !sp_a_locked_stream_is_dead())
      icon_text(NULL, IT_MCLOSED);

    ps_global->last_unambig_folder[0] = '\0';

    /*
     * remember old folder and context...
     */
    if(context_isambig(ps_global->cur_folder)
       || strucmp(ps_global->cur_folder, ps_global->inbox_name) == 0){
	strncpy(ps_global->context_current->last_folder,
		ps_global->cur_folder,
		sizeof(ps_global->context_current->last_folder)-1);
	ps_global->context_current->last_folder[sizeof(ps_global->context_current->last_folder)-1] = '\0';
    }
    else{
	strncpy(ps_global->last_unambig_folder, ps_global->cur_folder,
		sizeof(ps_global->last_unambig_folder)-1);
	ps_global->last_unambig_folder[sizeof(ps_global->last_unambig_folder)-1] = '\0';
    }

    /* folder in a subdir of context? */
    if(ps_global->context_current->dir->prev)
      sprintf(ps_global->cur_folder, "%.*s%.*s",
		(sizeof(ps_global->cur_folder)-1)/2,
		ps_global->context_current->dir->ref,
		(sizeof(ps_global->cur_folder)-1)/2,
		newfolder);
    else{
	strncpy(ps_global->cur_folder,
		(open_inbox) ? ps_global->inbox_name : newfolder,
		sizeof(ps_global->cur_folder)-1);
	ps_global->cur_folder[sizeof(ps_global->cur_folder)-1] = '\0';
    }

    sp_set_fldr(ps_global->mail_stream, ps_global->cur_folder);

    if(new_context){
	ps_global->context_last    = ps_global->context_current;
	ps_global->context_current = new_context;

	if(!open_inbox)
	  sp_set_context(ps_global->mail_stream, ps_global->context_current);
    }

    clear_index_cache();
    reset_index_format();

    /*
     * Start news reading with messages the user's marked deleted
     * hidden from view...
     */
    if(IS_NEWS(ps_global->mail_stream) && ps_global->mail_stream->rdonly)
      msgno_exclude_deleted(ps_global->mail_stream, ps_global->msgmap);

    if(we_cancel)
      cancel_busy_alarm(0);

    /*
     * If the stream we got from the open above was already opened earlier
     * for some temporary use, then it wouldn't have been filtered. That's
     * why we need this flag, so that we will filter if needed.
     */
    if(!sp_flagged(ps_global->mail_stream, SP_FILTERED))
      process_filter_patterns(ps_global->mail_stream, ps_global->msgmap, 0L);

    q_status_message6(SM_ORDER, 0, 4,
		    "%.20s \"%.200s\" opened with %.20s message%.20s%.20s%.20s",
			IS_NEWS(ps_global->mail_stream)
			  ? "News group" : "Folder",
			pretty_fn(newfolder),
			comatose(mn_get_total(ps_global->msgmap)),
			plural(mn_get_total(ps_global->msgmap)),
			(!open_inbox
			 && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED))
			    ? " (StayOpen)" : "",
			READONLY_FOLDER(ps_global->mail_stream)
						? " READONLY" : "");

#ifdef	_WINDOWS
    mswin_settitle(pretty_fn(newfolder));
#endif

    if(!(rflags & SP_MATCH) || !(rflags & SP_LOCKED))
      reset_sort_order(SRT_VRB);
    else if(sp_new_mail_count(ps_global->mail_stream) > 0L
	    || sp_unsorted_newmail(ps_global->mail_stream)
	    || sp_need_to_rethread(ps_global->mail_stream))
      refresh_sort(ps_global->mail_stream, ps_global->msgmap, SRT_NON);


    /*
     * Set current message number when re-opening Stay-Open or
     * cached folders.
     */
    if(rflags & SP_MATCH){
	if(rflags & SP_LOCKED){
	    if(F_OFF(F_STARTUP_STAYOPEN, ps_global)
	       && (cur = get_msgno_by_msg_id(ps_global->mail_stream,
			     sp_saved_cur_msg_id(ps_global->mail_stream),
			     ps_global->msgmap)) >= 1L
	       && cur <= mn_get_total(ps_global->msgmap)){
	      cur_already_set++;
	      mn_set_cur(ps_global->msgmap, (MsgNo) cur);
	      if(flags & DB_FROMTAB){
		/*
		 * When we TAB to a folder that is a StayOpen folder we try
		 * to increment the current message # by one instead of doing
		 * some search again. Some people probably won't like this
		 * behavior, especially if the new message that has arrived
		 * comes before where we are in the index. That's why we have
		 * the F_STARTUP_STAYOPEN feature above.
		 */
		mn_inc_cur(m, ps_global->msgmap, MH_NONE);
	      }
	      /* else leave it where it is */

	      adjust_cur_to_visible(ps_global->mail_stream, ps_global->msgmap);
	    }
	}
	else{
	    /*
	     * If we're reopening a cached open stream that wasn't explicitly
	     * kept open by the user, then the user expects it to act pretty
	     * much like we are re-opening the stream. A problem is that the
	     * recent messages are still recent because we haven't closed the
	     * stream, so we fake a quasi-recentness by remembering the last
	     * uid assigned on the stream when we pine_mail_close. Then when
	     * we come back messages with uids higher than that are recent.
	     *
	     * If uid_validity has changed, then we don't use any special
	     * treatment, but just do the regular search.
	     */
	    if(m->uid_validity == sp_saved_uid_validity(m)){
		long i;

		/*
		 * Because first_sorted_flagged uses sequence numbers, find the
		 * sequence number of the first message after the old last
		 * uid assigned. I.e., the first recent message.
		 */
		for(i = m->nmsgs; i > 0L; i--)
		  if(mail_uid(m, i) <= sp_saved_uid_last(m))
		    break;
		
		if(i > 0L && i < m->nmsgs)
		  pc = i+1L;
	    }
	}
    }


    if(!cur_already_set && mn_get_total(ps_global->msgmap) > 0L){

	perfolder_startup_rule = reset_startup_rule(ps_global->mail_stream);

	if(ps_global->start_entry > 0){
	    mn_set_cur(ps_global->msgmap, mn_get_revsort(ps_global->msgmap)
		       ? first_sorted_flagged(F_NONE, m,
					      ps_global->start_entry,
					      THREADING() ? 0 : FSF_SKIP_CHID)
		       : first_sorted_flagged(F_SRCHBACK, m,
					      ps_global->start_entry,
					      THREADING() ? 0 : FSF_SKIP_CHID));
	    ps_global->start_entry = 0;
        }
	else if(perfolder_startup_rule != IS_NOTSET ||
	        open_inbox ||
		ps_global->context_current->use & CNTXT_INCMNG){
	    unsigned use_this_startup_rule;

	    if(perfolder_startup_rule != IS_NOTSET)
	      use_this_startup_rule = perfolder_startup_rule;
	    else
	      use_this_startup_rule = ps_global->inc_startup_rule;

	    switch(use_this_startup_rule){
	      /*
	       * For news in incoming collection we're doing the same thing
	       * for first-unseen and first-recent. In both those cases you
	       * get first-unseen if FAKE_NEW is off and first-recent if
	       * FAKE_NEW is on. If FAKE_NEW is on, first unseen is the
	       * same as first recent because all recent msgs are unseen
	       * and all unrecent msgs are seen (see pine_mail_open).
	       */
	      case IS_FIRST_UNSEEN:
first_unseen:
		mn_set_cur(ps_global->msgmap,
			(sp_first_unseen(m)
			 && mn_get_sort(ps_global->msgmap) == SortArrival
			 && !mn_get_revsort(ps_global->msgmap)
			 && !get_lflag(ps_global->mail_stream, NULL,
				       sp_first_unseen(m), MN_EXLD)
			 && (n = mn_raw2m(ps_global->msgmap, 
					  sp_first_unseen(m))))
			   ? n
			   : first_sorted_flagged(F_UNSEEN | F_UNDEL, m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
		break;

	      case IS_FIRST_RECENT:
first_recent:
		/*
		 * We could really use recent for news but this is the way
		 * it has always worked, so we'll leave it. That is, if
		 * the FAKE_NEW feature is on, recent and unseen are
		 * equivalent, so it doesn't matter. If the feature isn't
		 * on, all the undeleted messages are unseen and we start
		 * at the first one. User controls with the FAKE_NEW feature.
		 */
		if(IS_NEWS(ps_global->mail_stream)){
		    mn_set_cur(ps_global->msgmap,
			       first_sorted_flagged(F_UNSEEN|F_UNDEL, m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID));
		}
		else{
		    mn_set_cur(ps_global->msgmap,
			       first_sorted_flagged(F_RECENT | F_UNSEEN
						    | F_UNDEL,
						    m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
		}
		break;

	      case IS_FIRST_IMPORTANT:
		mn_set_cur(ps_global->msgmap,
			   first_sorted_flagged(F_FLAG|F_UNDEL, m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
		break;

	      case IS_FIRST_IMPORTANT_OR_UNSEEN:

		if(IS_NEWS(ps_global->mail_stream))
		  goto first_unseen;

		{
		    MsgNo flagged, first_unseen;

		    flagged = first_sorted_flagged(F_FLAG|F_UNDEL, m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID);
		    first_unseen = (sp_first_unseen(m)
			     && mn_get_sort(ps_global->msgmap) == SortArrival
			     && !mn_get_revsort(ps_global->msgmap)
			     && !get_lflag(ps_global->mail_stream, NULL,
					   sp_first_unseen(m), MN_EXLD)
			     && (n = mn_raw2m(ps_global->msgmap, 
					      sp_first_unseen(m))))
				? n
				: first_sorted_flagged(F_UNSEEN|F_UNDEL, m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID);
		    mn_set_cur(ps_global->msgmap,
			  (MsgNo) min((int) flagged, (int) first_unseen));

		}

		break;

	      case IS_FIRST_IMPORTANT_OR_RECENT:

		if(IS_NEWS(ps_global->mail_stream))
		  goto first_recent;

		{
		    MsgNo flagged, first_recent;

		    flagged = first_sorted_flagged(F_FLAG|F_UNDEL, m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID);
		    first_recent = first_sorted_flagged(F_RECENT | F_UNSEEN
							| F_UNDEL,
							m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID);
		    mn_set_cur(ps_global->msgmap,
			      (MsgNo) min((int) flagged, (int) first_recent));
		}

		break;

	      case IS_FIRST:
		mn_set_cur(ps_global->msgmap,
			   first_sorted_flagged(F_UNDEL, m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
		break;

	      case IS_LAST:
		mn_set_cur(ps_global->msgmap,
			   first_sorted_flagged(F_UNDEL, m, pc,
			         FSF_LAST | (THREADING() ? 0 : FSF_SKIP_CHID)));
		break;

	      default:
		panic("Unexpected incoming startup case");
		break;

	    }
	}
	else if(IS_NEWS(ps_global->mail_stream)){
	    /*
	     * This will go to two different places depending on the FAKE_NEW
	     * feature (see pine_mail_open).
	     */
	    mn_set_cur(ps_global->msgmap,
		       first_sorted_flagged(F_UNSEEN|F_UNDEL, m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
	}
        else{
	    mn_set_cur(ps_global->msgmap,
		       mn_get_revsort(ps_global->msgmap)
		         ? 1L
			 : mn_get_total(ps_global->msgmap));
	}

	adjust_cur_to_visible(ps_global->mail_stream, ps_global->msgmap);
    }
    else if(!(rflags & SP_MATCH)){
	mn_set_cur(ps_global->msgmap, -1L);
    }

    if(ps_global->mail_stream)
      sp_set_first_unseen(ps_global->mail_stream, 0L);

    return(1);
}


void
reset_index_format()
{
    long rflags = ROLE_DO_OTHER;
    PAT_STATE     pstate;
    PAT_S        *pat;
    int           we_set_it = 0;

    if(ps_global->mail_stream && nonempty_patterns(rflags, &pstate)){
	for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate)){
	    if(match_pattern(pat->patgrp, ps_global->mail_stream, NULL,
			     NULL, NULL, SO_NOSERVER|SE_NOPREFETCH))
	      break;
	}

	if(pat && pat->action && !pat->action->bogus
	   && pat->action->index_format){
	    we_set_it++;
	    init_index_format(pat->action->index_format,
			      &ps_global->index_disp_format);
	}
    }

    if(!we_set_it)
      init_index_format(ps_global->VAR_INDEX_FORMAT,
		        &ps_global->index_disp_format);
}


void
reset_sort_order(flags)
    unsigned flags;
{
    long rflags = ROLE_DO_OTHER;
    PAT_STATE     pstate;
    PAT_S        *pat;
    SortOrder	  the_sort_order;
    int           sort_is_rev;

    /* set default order */
    the_sort_order = ps_global->def_sort;
    sort_is_rev    = ps_global->def_sort_rev;

    if(ps_global->mail_stream && nonempty_patterns(rflags, &pstate)){
	for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate)){
	    if(match_pattern(pat->patgrp, ps_global->mail_stream, NULL,
			     NULL, NULL, SO_NOSERVER|SE_NOPREFETCH))
	      break;
	}

	if(pat && pat->action && !pat->action->bogus
	   && pat->action->sort_is_set){
	    the_sort_order = pat->action->sortorder;
	    sort_is_rev    = pat->action->revsort;
	}
    }

    sort_folder(ps_global->mail_stream, ps_global->msgmap,
		the_sort_order, sort_is_rev, flags);
}


unsigned
reset_startup_rule(stream)
    MAILSTREAM *stream;
{
    long rflags = ROLE_DO_OTHER;
    PAT_STATE     pstate;
    PAT_S        *pat;
    unsigned      startup_rule;

    startup_rule = IS_NOTSET;

    if(stream && nonempty_patterns(rflags, &pstate)){
	for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate)){
	    if(match_pattern(pat->patgrp, stream, NULL, NULL, NULL,
			     SO_NOSERVER|SE_NOPREFETCH))
	      break;
	}

	if(pat && pat->action && !pat->action->bogus)
	  startup_rule = pat->action->startup_rule;
    }

    return(startup_rule);
}


/*----------------------------------------------------------------------
    Open the requested folder in the requested context

    Args: state -- usual pine state struct
	  newfolder -- folder to open
	  new_context -- folder context might live in
	  stream -- candidate for recycling

   Result: New folder open or not (if error), and we're set to
	   enter the index screen.
 ----*/
void
visit_folder(state, newfolder, new_context, stream, flags)
    struct pine *state;
    char	*newfolder;
    CONTEXT_S	*new_context;
    MAILSTREAM  *stream;
    unsigned long flags;
{
    dprint(9, (debugfile, "visit_folder(%s, %s)\n",
	       newfolder ? newfolder : "?",
	       (new_context && new_context->context)
	         ? new_context->context : "(NULL)"));

    if(do_broach_folder(newfolder, new_context, stream ? &stream : NULL,
			flags) >= 0
       || !sp_flagged(state->mail_stream, SP_LOCKED))
      state->next_screen = mail_index_screen;
    else
      state->next_screen = folder_screen;
}



/*----------------------------------------------------------------------
      Expunge (if confirmed) and close a mail stream

    Args: stream   -- The MAILSTREAM * to close
        final_msg  -- If non-null, this should be set to point to a
		      message to print out in the caller, it is allocated
		      here and freed by the caller.

   Result:  Mail box is expunged and closed. A message is displayed to
             say what happened
 ----*/
void
expunge_and_close(stream, final_msg, flags)
    MAILSTREAM   *stream;
    char        **final_msg;
    unsigned long flags;
{
    long  i, delete_count, max_folder, seen_not_del;
    char  prompt_b[MAX_SCREEN_COLS+1], *short_folder_name,
          temp[MAILTMPLEN+1], buff1[MAX_SCREEN_COLS+1], *moved_msg = NULL,
	  buff2[MAX_SCREEN_COLS+1], *folder;
    CONTEXT_S *context;
    struct variable *vars = ps_global->vars;
    int ret, expunge = FALSE, no_close = 0;
    char ing[4];

    no_close = (flags & EC_NO_CLOSE);

    if(!(stream && sp_flagged(stream, SP_LOCKED)))
      stream = NULL;

    /* check for dead stream */
    if(stream && sp_dead_stream(stream)){
	pine_mail_actually_close(stream);
	stream = NULL;
    }

    if(stream != NULL){
	context = sp_context(stream);
	folder  = STREAMNAME(stream);

        dprint(2, (debugfile, "expunge_and_close: \"%s\"%s\n",
                   folder, no_close ? " (NO_CLOSE bit set)" : ""));
	if(final_msg)
	  strcpy(ing, "ed");
	else
	  strcpy(ing, "ing");

	buff1[0] = '\0';
	buff2[0] = '\0';

        if(!stream->rdonly){

	    if(!no_close){
		q_status_message1(SM_INFO, 0, 1, "Closing \"%.200s\"...",
				  folder);

		flush_status_messages(1);
	    }

	    mail_expunge_prefilter(stream, MI_CLOSING);

	    /*
	     * Be sure to expunge any excluded (filtered) msgs
	     * Do it here so they're not copied into read/archived
	     * folders *AND* to be sure we don't refilter them
	     * next time the folder's opened.
	     */
	    for(i = 1L; i <= stream->nmsgs; i++)
	      if(get_lflag(stream, NULL, i, MN_EXLD)){	/* if there are any */
		  delete_filtered_msgs(stream);		/* delete them all */
		  expunge = TRUE;
		  break;
	      }

	    /* Save read messages? */
	    if(VAR_READ_MESSAGE_FOLDER && VAR_READ_MESSAGE_FOLDER[0]
	       && sp_flagged(stream, SP_INBOX)
	       && (seen_not_del = count_flagged(stream, F_SEEN | F_UNDEL))){

		if(F_ON(F_AUTO_READ_MSGS,ps_global)
		   || read_msg_prompt(seen_not_del, VAR_READ_MESSAGE_FOLDER))
		  /* move inbox's read messages */
		  moved_msg = move_read_msgs(stream, VAR_READ_MESSAGE_FOLDER,
					     buff1, -1L);
	    }
	    else if(VAR_ARCHIVED_FOLDERS)
	      moved_msg = move_read_incoming(stream, context, folder,
					     VAR_ARCHIVED_FOLDERS,
					     buff1);

	    /*
	     * We need the count_flagged to be executed not only to set
	     * delete_count, but also to set the searched bits in all of
	     * the deleted messages. The searched bit is used in the monkey
	     * business section below which undeletes deleted messages
	     * before expunging. It determines which messages are deleted
	     * by examining the searched bit, which had better be set or not
	     * based on this count_flagged call rather than some random
	     * search that happened earlier.
	     */
            delete_count = count_flagged(stream, F_DEL);
	    if(F_ON(F_EXPUNGE_MANUALLY,ps_global))
              delete_count = 0L;

	    ret = 'n';
	    if(delete_count){
		int charcnt = 0;

		if(delete_count == 1)
		  charcnt = 1;
		else{
		    sprintf(temp, "%ld", delete_count);
		    charcnt = strlen(temp)+1;
		}

		max_folder = max(1,MAXPROMPT - (36+charcnt));
		strncpy(temp, pretty_fn(folder), sizeof(temp));
		temp[sizeof(temp)-1] = '\0';
		short_folder_name = short_str(temp,buff2,max_folder,FrontDots);

		if(F_ON(F_FULL_AUTO_EXPUNGE,ps_global)
		   || (F_ON(F_AUTO_EXPUNGE, ps_global)
		       && ((!strucmp(folder,ps_global->inbox_name))
			   || (context && (context->use & CNTXT_INCMNG)))
		       && context_isambig(folder))){
		    ret = 'y';
		}
		else{
		    sprintf(prompt_b,
			    "Expunge the %ld deleted message%s from \"%s\"",
			    delete_count,
			    delete_count == 1 ? "" : "s",
			    short_folder_name);
		    ret = want_to(prompt_b, 'y', 0, NO_HELP, WT_NORM);
		}

		/* get this message back in queue */
		if(moved_msg)
		  q_status_message(SM_ORDER,
		      F_ON(F_AUTO_READ_MSGS,ps_global) ? 0 : 3, 5, moved_msg);

		if(ret == 'y'){
		    long filtered;

		    filtered = any_lflagged(sp_msgmap(stream), MN_EXLD);

		    sprintf(buff2,
		      "%s%s%s%.30s%s%s %s message%s and remov%s %s.",
			no_close ? "" : "Clos",
			no_close ? "" : ing,
			no_close ? "" : " \"",
	 		no_close ? "" : pretty_fn(folder),
			no_close ? "" : "\". ",
			final_msg ? "Kept" : "Keeping",
			comatose(stream->nmsgs - filtered - delete_count),
			plural(stream->nmsgs - filtered - delete_count),
			ing,
			long2string(delete_count));
		    if(final_msg)
		      *final_msg = cpystr(buff2);
		    else
		      q_status_message(SM_ORDER,
				       no_close ? 1 :
				        (F_ON(F_AUTO_EXPUNGE,ps_global)
					 || F_ON(F_FULL_AUTO_EXPUNGE,ps_global))
					         ? 0 : 3,
				       5, buff2);
		      
		    flush_status_messages(1);
		    ps_global->mm_log_error = 0;
		    ps_global->expunge_in_progress = 1;
		    mail_expunge(stream);
		    ps_global->expunge_in_progress = 0;
		    if(ps_global->mm_log_error && final_msg && *final_msg){
			fs_give((void **)final_msg);
			*final_msg = NULL;
		    }
		}
	    }

	    if(ret != 'y'){
		if(expunge){
		    MESSAGECACHE *mc;
		    char	 *seq;
		    int		  expbits;

		    /*
		     * filtered message monkey business.
		     * The Plan:
		     *   1) light sequence bits for legit deleted msgs
		     *      and store marker in local extension
		     *   2) clear their deleted flag
		     *   3) perform expunge to removed filtered msgs
		     *   4) restore deleted flags for legit msgs
		     *      based on local extension bit
		     */
		    for(i = 1L; i <= stream->nmsgs; i++)
		      if(!get_lflag(stream, NULL, i, MN_EXLD)
			 && (((mc = mail_elt(stream, i)) && mc->valid && mc->deleted)
			     || (mc && !mc->valid && mc->searched))){
			  mc->sequence = 1;
			  expbits      = MSG_EX_DELETE;
			  msgno_exceptions(stream, i, "0", &expbits, TRUE);
		      }
		      else if((mc = mail_elt(stream, i)) != NULL)
			mc->sequence = 0;

		    if(seq = build_sequence(stream, NULL, NULL)){
			mail_flag(stream, seq, "\\DELETED", ST_SILENT);
			fs_give((void **) &seq);
		    }

		    ps_global->mm_log_error = 0;
		    ps_global->expunge_in_progress = 1;
		    mail_expunge(stream);
		    ps_global->expunge_in_progress = 0;

		    for(i = 1L; i <= stream->nmsgs; i++)
		      if((mc = mail_elt(stream, i)) != NULL)
		        mc->sequence 
			   = (msgno_exceptions(stream, i, "0", &expbits, FALSE)
			      && (expbits & MSG_EX_DELETE));

		    if(seq = build_sequence(stream, NULL, NULL)){
			mail_flag(stream, seq, "\\DELETED", ST_SET|ST_SILENT);
			fs_give((void **) &seq);
		    }
		}

		if(!no_close){
		    if(stream->nmsgs){
			sprintf(buff2,
			    "Clos%s folder \"%.*s\". %s%s%s message%s.",
			    ing,
			    sizeof(buff2)-50, pretty_fn(folder), 
			    final_msg ? "Kept" : "Keeping",
			    (stream->nmsgs == 1L) ? " single" : " all ",
			    (stream->nmsgs > 1L)
			      ? comatose(stream->nmsgs) : "",
			    plural(stream->nmsgs));
		    }
		    else{
			sprintf(buff2, "Clos%s empty folder \"%.*s\"",
			    ing, sizeof(buff2)-50, pretty_fn(folder));
		    }

		    if(final_msg)
		      *final_msg = cpystr(buff2);
		    else
		      q_status_message(SM_ORDER, 0, 3, buff2);
		}
	    }
        }
	else{
            if(IS_NEWS(stream)){
		/*
		 * Mark the filtered messages deleted so they aren't
		 * filtered next time.
		 */
	        for(i = 1L; i <= stream->nmsgs; i++){
		  int exbits;
		  if(msgno_exceptions(stream, i, "0" , &exbits, FALSE)
		     && (exbits & MSG_EX_FILTERED)){
		    delete_filtered_msgs(stream);
		    break;
		  }
		}
		/* first, look to archive read messages */
		if(moved_msg = move_read_incoming(stream, context, folder,
						  VAR_ARCHIVED_FOLDERS,
						  buff1))
		  q_status_message(SM_ORDER,
		      F_ON(F_AUTO_READ_MSGS,ps_global) ? 0 : 3, 5, moved_msg);

		sprintf(buff2, "Clos%s news group \"%.*s\"",
			ing, sizeof(buff2)-50, pretty_fn(folder));

		if(F_ON(F_NEWS_CATCHUP, ps_global)){
		    MESSAGECACHE *mc;

		    /* count visible messages */
		    (void) count_flagged(stream, F_DEL);
		    for(i = 1L, delete_count = 0L; i <= stream->nmsgs; i++)
		      if(!(get_lflag(stream, NULL, i, MN_EXLD)
			   || ((mc = mail_elt(stream, i)) && mc->valid
				&& mc->deleted)
			   || (mc && !mc->valid && mc->searched)))
			delete_count++;

		    if(delete_count){
			sprintf(prompt_b,
				"Delete %s%ld message%s from \"%s\"",
				(delete_count > 1L) ? "all " : "",
				delete_count, plural(delete_count),
				pretty_fn(folder));
			if(want_to(prompt_b, 'y', 0,
				   NO_HELP, WT_NORM) == 'y'){
			    char seq[64];

			    sprintf(seq, "1:%ld", stream->nmsgs);
			    mail_flag(stream, seq,
				      "\\DELETED", ST_SET|ST_SILENT);
			}
		    }
		}

		if(F_ON(F_NEWS_CROSS_DELETE, ps_global))
		  cross_delete_crossposts(stream);
	    }
            else
	      sprintf(buff2,
			"Clos%s read-only folder \"%.*s\". No changes to save",
			ing, sizeof(buff2)-60, pretty_fn(folder));

	    if(final_msg)
	      *final_msg = cpystr(buff2);
	    else
	      q_status_message(SM_ORDER, 0, 2, buff2);
        }

	/*
	 * Make darn sure any mm_log fallout caused above get's seen...
	 */
	if(!no_close){
	    flush_status_messages(1);
	    pine_mail_close(stream);
	}
    }
}



/*----------------------------------------------------------------------
  Dispatch messages matching FILTER patterns.

  Args:
	stream -- mail stream serving messages
	msgmap -- sequence to msgno mapping table
	recent -- number of recent messages to check (but really only its
	               nonzeroness is used)

 When we're done, any filtered messages are filtered and the message
 mapping table has any filtered messages removed.
  ---*/
void
process_filter_patterns(stream, msgmap, recent)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    long	recent;
{
    long	  i, n, raw, orig_nmsgs;
    unsigned long uid;
    int           we_cancel = 0, any_msgs = 0, any_to_filter = 0;
    int		  exbits, nt = 0, pending_actions = 0, for_debugging = 0;
    int           cleared_index_cache = 0;
    long          rflags = ROLE_DO_FILTER;
    char	 *charset = NULL, *nick = NULL;
    char          busymsg[80];
    MSGNO_S      *tmpmap = NULL;
    MESSAGECACHE *mc;
    PAT_S        *pat, *nextpat = NULL;
    SEARCHPGM	 *pgm = NULL;
    SEARCHSET	 *srchset = NULL;
    long          flags = (SE_NOPREFETCH|SE_FREE);
    PAT_STATE     pstate;

    dprint(5, (debugfile, "process_filter_patterns(stream=%s, recent=%ld)\n",
	    !stream                             ? "<null>"                   :
	     sp_flagged(stream, SP_INBOX)        ? "inbox"                   :
	      stream->original_mailbox            ? stream->original_mailbox :
	       stream->mailbox                     ? stream->mailbox         :
				                      "?",
	    recent));

    if(!msgmap || !stream)
      return;

    if(!recent)
      sp_set_flags(stream, sp_flags(stream) | SP_FILTERED);

    while(stream && stream->nmsgs && nonempty_patterns(rflags, &pstate)){

	for_debugging++;
	pending_actions = 0;
	nextpat = NULL;

	uid = mail_uid(stream, stream->nmsgs);

	/*
	 * Some of the search stuff won't work on old servers so we
	 * get the data and search locally. Big performance hit.
	 */
	if(is_imap_stream(stream) && !modern_imap_stream(stream))
	  flags |= SO_NOSERVER;
	else if(stream->dtb && stream->dtb->name
	        && !strcmp(stream->dtb->name, "nntp"))
	  flags |= SO_OVERVIEW;

	/*
	 * ignore all previously filtered messages
	 * and, if requested, anything not a recent
	 * arrival...
	 *
	 * Here we're using spare6 (MN_STMP), meaning we'll only
	 * search the ones with spare6 marked, new messages coming 
	 * in will not be considered.  There used to be orig_nmsgs,
	 * which kept track of this, but if a message gets expunged,
	 * then a new message could be lower than orig_nmsgs.
	 */
	for(i = 1; i <= stream->nmsgs; i++)
	  if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
	      if(exbits & MSG_EX_FILTERED){
		  if((mc = mail_elt(stream, i)) != NULL)
		    mc->spare6 = 0;
	      }
	      else if(!recent || !(exbits & MSG_EX_TESTED)){
		  if((mc = mail_elt(stream, i)) != NULL)
		    mc->spare6 = 1;

		  any_to_filter++;
	      }
	      else if((mc = mail_elt(stream, i)) != NULL)
		mc->spare6 = 0;
	  }
	  else{
	      if((mc = mail_elt(stream, i)) != NULL)
	        mc->spare6 = !recent;

	      any_to_filter += !recent;
	  }
	
	if(!any_to_filter){
	  dprint(5, (debugfile, "No messages need filtering\n"));
	}

	/* Then start searching */
	for(pat = first_pattern(&pstate); any_to_filter && pat; pat = nextpat){
	    nextpat = next_pattern(&pstate);
	    dprint(5, (debugfile,
		"Trying filter \"%s\"\n",
		(pat->patgrp && pat->patgrp->nick)
		    ? pat->patgrp->nick : "?"));
	    if(pat->patgrp && !pat->patgrp->bogus
	       && pat->action && !pat->action->bogus
	       && !trivial_patgrp(pat->patgrp)
	       && match_pattern_folder(pat->patgrp, stream)
	       && !match_pattern_folder_specific(pat->action->folder,
						 stream, FOR_FILT)){

		/*
		 * We could just keep track of spare6 accurately when
		 * we change the msgno_exceptions flags, but...
		 */
		for(i = 1; i <= stream->nmsgs; i++){
		    if((mc=mail_elt(stream, i)) && mc->spare6){
			if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
			    if(exbits & MSG_EX_FILTERED)
			      mc->sequence = 0;
			    else if(!recent || !(exbits & MSG_EX_TESTED))
			      mc->sequence = 1;
			    else
			      mc->sequence = 0;
			}
			else 
			  mc->sequence = !recent;
		    }
		    else
		      mc->sequence = 0;
		}

		if(!(srchset = build_searchset(stream))){
		    dprint(5, (debugfile, "Empty searchset\n"));
		    continue;		/* nothing to search, move on */
		}

#ifdef DEBUG
		{SEARCHSET *s;
		  dprint(5, (debugfile, "searchset="));
		  for(s = srchset; s; s = s->next){
		    if(s->first == s->last || s->last == 0L){
		      dprint(5, (debugfile, " %ld", s->first));
		    }
		    else{
		      dprint(5, (debugfile, " %ld-%ld", s->first, s->last));
		    }
		  }
		  dprint(5, (debugfile, "\n"));
		}
#endif
		nick = (pat && pat->patgrp && pat->patgrp->nick
			&& pat->patgrp->nick[0]) ? pat->patgrp->nick : NULL;
		sprintf(busymsg, "Processing filter \"%.50s\"",
			nick ? nick : "?");

		/*
		 * The strange last argument is so that the busy message
		 * won't come out until after a second if the user sets
		 * the feature to quell "filtering done". That's because
		 * they are presumably interested in the filtering actions
		 * themselves more than what is happening, so they'd
		 * rather see the action messages instead of the processing
		 * message. That's my theory anyway.
		 */
		if(F_OFF(F_QUELL_FILTER_MSGS, ps_global))
		  any_msgs = we_cancel = busy_alarm(1, busymsg, NULL,
				   F_ON(F_QUELL_FILTER_DONE_MSG, ps_global)
					? 0 : 1);

		if(pat->patgrp->stat_bom != PAT_STAT_EITHER){
		    if(pat->patgrp->stat_bom == PAT_STAT_YES){
			if(!ps_global->beginning_of_month){
			    dprint(5, (debugfile,
		    "Filter %s wants beginning of month and it isn't bom\n",
				   nick ? nick : "?"));
			    continue;
			}
		    }
		    else if(pat->patgrp->stat_bom == PAT_STAT_NO){
			if(ps_global->beginning_of_month){
			    dprint(5, (debugfile,
		   "Filter %s does not want beginning of month and it is bom\n",
				   nick ? nick : "?"));
			    continue;
			}
		    }
		}

		if(pat->patgrp->stat_boy != PAT_STAT_EITHER){
		    if(pat->patgrp->stat_boy == PAT_STAT_YES){
			if(!ps_global->beginning_of_year){
			    dprint(5, (debugfile,
		    "Filter %s wants beginning of year and it isn't boy\n",
				   nick ? nick : "?"));
			    continue;
			}
		    }
		    else if(pat->patgrp->stat_boy == PAT_STAT_NO){
			if(ps_global->beginning_of_year){
			    dprint(5, (debugfile,
		   "Filter %s does not want beginning of year and it is boy\n",
				   nick ? nick : "?"));
			    continue;
			}
		    }
		}
			
		charset = NULL;
		pgm = match_pattern_srchpgm(pat->patgrp, stream,
					    &charset, srchset);

		pine_mail_search_full(stream, charset, pgm, flags);


		/* check scores */
		if(scores_are_used(SCOREUSE_GET) & SCOREUSE_FILTERS &&
		   pat->patgrp->do_score){
		    SEARCHSET *s, *ss;

		    /*
		     * Build a searchset so we can get all the scores we
		     * need and only the scores we need efficiently.
		     */

		    for(i = 1; i <= stream->nmsgs; i++)
		      if((mc = mail_elt(stream, i)) != NULL)
		        mc->sequence = 0;

		    for(s = srchset; s; s = s->next)
		      for(i = s->first; i <= s->last; i++)
			if(i > 0L && stream && i <= stream->nmsgs
			   && (mc=mail_elt(stream, i)) && mc->searched &&
			   get_msg_score(stream, i) == SCORE_UNDEF)
			  mc->sequence = 1;

		    if((ss = build_searchset(stream)) != NULL){
			(void)calculate_some_scores(stream, ss, 0);
			mail_free_searchset(&ss);
		    }

		    /*
		     * Now check the patterns which have matched so far
		     * to see if their score is in the score interval.
		     */
		    for(s = srchset; s; s = s->next)
		      for(i = s->first; i <= s->last; i++)
			if(i > 0L && stream && i <= stream->nmsgs
			   && (mc=mail_elt(stream, i)) && mc->searched){
			    long score;

			    score = get_msg_score(stream, i);

			    /*
			     * If the score is outside all of the intervals,
			     * turn off the searched bit.
			     * So that means we check each interval and if
			     * it is inside any interval we stop and leave
			     * the bit set. If it is outside we keep checking.
			     */
			    if(score != SCORE_UNDEF){
				INTVL_S *iv;

				for(iv = pat->patgrp->score; iv; iv = iv->next)
				  if(score >= iv->imin && score <= iv->imax)
				    break;
				
				if(!iv)
				  mc->searched = NIL;
			    }
			}
		}

		/* check for 8bit subject match or not */
		if(pat->patgrp->stat_8bitsubj != PAT_STAT_EITHER)
		  find_8bitsubj_in_messages(stream, srchset,
					    pat->patgrp->stat_8bitsubj, 0);

		/* if there are still matches, check for charset matches */
		if(pat->patgrp->charsets)
		  find_charsets_in_messages(stream, srchset, pat->patgrp, 0);

		if(pat->patgrp->abookfrom != AFRM_EITHER)
		  from_or_replyto_in_abook(stream, srchset,
					   pat->patgrp->abookfrom,
					   pat->patgrp->abooks);

		/* Still matches? Run the categorization command on each msg. */
		if(pat->patgrp->category_cmd && pat->patgrp->category_cmd[0]){
		    char **l;
		    int exitval;
		    SEARCHSET *s, *ss = NULL;
		    char *cmd = NULL;
		    char *just_arg0 = NULL;
		    char *cmd_start, *cmd_end;
		    int just_one = !(pat->patgrp->category_cmd[1]);

		    /* find the first command that exists on this host */
		    for(l = pat->patgrp->category_cmd; l && *l; l++){
			cmd = cpystr(*l);
			removing_quotes(cmd);
			if(cmd){
			    for(cmd_start = cmd;
				*cmd_start && isspace(*cmd_start); cmd_start++)
			      ;
			    
			    for(cmd_end = cmd_start+1;
				*cmd_end && !isspace(*cmd_end); cmd_end++)
			      ;
			    
			    just_arg0 = (char *) fs_get((cmd_end-cmd_start+1)
								* sizeof(char));
			    strncpy(just_arg0, cmd_start, cmd_end - cmd_start);
			    just_arg0[cmd_end - cmd_start] = '\0';
			}

			if(valid_filter_command(&just_arg0))
			  break;
			else{
			    if(just_one){
				if(can_access(just_arg0, ACCESS_EXISTS) != 0)
				  q_status_message1(SM_ORDER, 0, 3,
						    "\"%s\" does not exist",
						    just_arg0);
				else
				  q_status_message1(SM_ORDER, 0, 3,
						    "\"%s\" is not executable",
						    just_arg0);
			    }

			    if(just_arg0)
			      fs_give((void **) &just_arg0);
			    if(cmd)
			      fs_give((void **) &cmd);
			}
		    }

		    if(!just_arg0 && !just_one)
		      q_status_message(SM_ORDER, 0, 3,
			"None of the category cmds exists and is executable");

		    /*
		     * If category_cmd isn't executable, it isn't a match.
		     */
		    if(!just_arg0 || !cmd){
			/*
			 * If we couldn't run the pipe command,
			 * we declare no match
			 */
			for(s = srchset; s; s = s->next)
			  for(i = s->first; i <= s->last; i++)
			    if(i > 0L && stream && i <= stream->nmsgs
			       && (mc=mail_elt(stream, i)) && mc->searched)
			      mc->searched = NIL;
		    }
		    else
		      for(s = srchset; s; s = s->next)
			for(i = s->first; i <= s->last; i++)
			  if(i > 0L && stream && i <= stream->nmsgs
			     && (mc=mail_elt(stream, i)) && mc->searched){

			    /*
			     * If there was an error, or the exitval is out of
			     * range, then it is not a match.
			     * Default range is (0,0),
			     * which is right for matching
			     * bogofilter spam.
			     */
			    if(test_message_with_cmd(stream, i, cmd,
						     pat->patgrp->cat_lim,
						     &exitval) != 0)
			      mc->searched = NIL;

			    /* test exitval */
			    if(mc->searched){
			      INTVL_S *iv;

			      if(pat->patgrp->cat){
				for(iv = pat->patgrp->cat; iv; iv = iv->next)
				  if((long) exitval >= iv->imin
				     && (long) exitval <= iv->imax)
				    break;
				    
				if(!iv)
				  mc->searched = NIL;  /* not in any interval */
			      }
			      else{
				/* default to interval containing only zero */
				if(exitval != 0)
				  mc->searched = NIL;
			      }
			    }
			  }
		    
		    if(just_arg0)
		      fs_give((void **) &just_arg0);
		    if(cmd)
		      fs_give((void **) &cmd);
		}

		if(we_cancel){
		    cancel_busy_alarm(-1);
		    we_cancel = 0;
		}

		nt = pat->action->non_terminating;
		pending_actions = max(nt, pending_actions);

		/*
		 * Change some state bits.
		 * This used to only happen if kill was not set, but
		 * it can be useful to Delete a message even if killing.
		 * That way, it will show up in another pine that isn't
		 * running the same filter as Deleted, so the user won't
		 * bother looking at it.  Hubert 2004-11-16
		 */
		if(pat->action->state_setting_bits
		   || pat->action->keyword_set
		   || pat->action->keyword_clr){
		    tmpmap = NULL;
		    mn_init(&tmpmap, stream->nmsgs);

		    for(i = 1L, n = 0L; i <= stream->nmsgs; i++)
		      if((mc = mail_elt(stream, i)) && mc->searched
			 && !(msgno_exceptions(stream, i, "0", &exbits, FALSE)
			      && (exbits & MSG_EX_FILTERED)))
			if(!n++){
			    mn_set_cur(tmpmap, i);
			}
			else{
			    mn_add_cur(tmpmap, i);
			}

		    if(n){
			long flagbits;
			char     **keywords_to_set = NULL,
			         **keywords_to_clr = NULL;
			PATTERN_S *pp;
			int        cnt;

			flagbits = pat->action->state_setting_bits;

			if(pat->action->keyword_set){
			    for(cnt = 0, pp = pat->action->keyword_set;
				pp; pp = pp->next)
			      cnt++;

			    keywords_to_set = (char **) fs_get((cnt+1) *
						    sizeof(*keywords_to_set));
			    memset(keywords_to_set, 0,
				   (cnt+1) * sizeof(*keywords_to_set));
			    for(cnt = 0, pp = pat->action->keyword_set;
				pp; pp = pp->next){
				char *q;

				q = nick_to_keyword(pp->substring);
				if(q && q[0])
				  keywords_to_set[cnt++] = cpystr(q);
			    }
				
			    flagbits |= F_KEYWORD;
			}

			if(pat->action->keyword_clr){
			    for(cnt = 0, pp = pat->action->keyword_clr;
				pp; pp = pp->next)
			      cnt++;

			    keywords_to_clr = (char **) fs_get((cnt+1) *
						    sizeof(*keywords_to_clr));
			    memset(keywords_to_clr, 0,
				   (cnt+1) * sizeof(*keywords_to_clr));
			    for(cnt = 0, pp = pat->action->keyword_clr;
				pp; pp = pp->next){
				char *q;

				q = nick_to_keyword(pp->substring);
				if(q && q[0])
				  keywords_to_clr[cnt++] = cpystr(q);
			    }
				
			    flagbits |= F_UNKEYWORD;
			}

			set_some_flags(stream, tmpmap, flagbits,
				       keywords_to_set, keywords_to_clr, 1,
				       nick);
		    }

		    mn_give(&tmpmap);
		}

		/*
		 * The two halves of the if-else are almost the same and
		 * could probably be combined cleverly. The if clause
		 * is simply setting the MSG_EX_FILTERED bit, and leaving
		 * n set to zero. The msgno_exclude is not done in this case.
		 * The else clause excludes each message (because it is
		 * either filtered into nothing or moved to folder). The
		 * exclude messes with the msgmap and that changes max_msgno,
		 * so the loop control is a little tricky.
		 */
		if(!(pat->action->kill || pat->action->folder)){
		  n = 0L;
		  for(i = 1L; i <= mn_get_total(msgmap); i++)
		    if((raw = mn_m2raw(msgmap, i)) > 0L
		       && stream && raw <= stream->nmsgs
		       && (mc = mail_elt(stream, raw)) && mc->searched){
		        dprint(5, (debugfile,
			    "FILTER matching \"%s\": msg %ld%s\n",
			    nick ? nick : "unnamed",
			    raw, nt ? " (dont stop)" : ""));
		        if(msgno_exceptions(stream, raw, "0", &exbits, FALSE))
			  exbits |= (nt ? MSG_EX_FILTONCE : MSG_EX_FILTERED);
		        else
			  exbits = (nt ? MSG_EX_FILTONCE : MSG_EX_FILTERED);
			
			/*
			 * If this matched an earlier non-terminating rule
			 * we've been keeping track of that so that we can
			 * turn it into a permanent match at the end.
			 * However, now we've matched another rule that is
			 * terminating so we don't have to worry about it
			 * anymore. Turn off the flag.
			 */
			if(!nt && exbits & MSG_EX_FILTONCE)
			  exbits ^= MSG_EX_FILTONCE;
			
			exbits &= ~MSG_EX_STATECHG;

		        msgno_exceptions(stream, raw, "0", &exbits, TRUE);
		    }
		}
		else{
		  for(i = 1L, n = 0L; i <= mn_get_total(msgmap); )
		    if((raw = mn_m2raw(msgmap, i))
		       && raw > 0L && stream && raw <= stream->nmsgs
		       && (mc = mail_elt(stream, raw)) && mc->searched){
		        dprint(5, (debugfile,
			      "FILTER matching \"%s\": msg %ld %s%s\n",
			      nick ? nick : "unnamed",
			      raw, pat->action->folder ? "filed" : "killed",
			      nt ? " (dont stop)" : ""));
			if(nt)
			  i++;
			else{
			    if(!cleared_index_cache
			       && stream == ps_global->mail_stream){
				cleared_index_cache = 1;
				clear_index_cache();
			    }

			    msgno_exclude(stream, msgmap, i, 1);
			    /* 
			     * If this message is new, decrement
			     * new_mail_count. Previously, the caller would
			     * do this by counting MN_EXCLUDE before and after,
			     * but the results weren't accurate in the case
			     * where new messages arrived while filtering,
			     * or the filtered message could have gotten
			     * expunged.
			     */
			    if(msgno_exceptions(stream, raw, "0", &exbits,
						FALSE)
			       && (exbits & MSG_EX_RECENT)){
				long l, ll;

			        l = sp_new_mail_count(stream);
			        ll = sp_recent_since_visited(stream);
				dprint(5, (debugfile, "New message being filtered, decrement new_mail_count: %ld -> %ld\n", l, l-1L));
			        if(l > 0L)
				  sp_set_new_mail_count(stream, l-1L);
			        if(ll > 0L)
				  sp_set_recent_since_visited(stream, ll-1L);
			    }
			}

		        if(msgno_exceptions(stream, raw, "0", &exbits, FALSE))
			  exbits |= (nt ? MSG_EX_FILTONCE : MSG_EX_FILTERED);
		        else
			  exbits = (nt ? MSG_EX_FILTONCE : MSG_EX_FILTERED);
			
			/* set pending exclusion  for later */
			if(nt)
			  exbits |= MSG_EX_PEND_EXLD;

			/*
			 * If this matched an earlier non-terminating rule
			 * we've been keeping track of that so that we can
			 * turn it into a permanent match at the end.
			 * However, now we've matched another rule that is
			 * terminating so we don't have to worry about it
			 * anymore. Turn off the flags.
			 */
			if(!nt && exbits & MSG_EX_FILTONCE){
			    exbits ^= MSG_EX_FILTONCE;

			    /* we've already excluded it, too */
			    if(exbits & MSG_EX_PEND_EXLD)
			      exbits ^= MSG_EX_PEND_EXLD;
			}

			exbits &= ~MSG_EX_STATECHG;

			msgno_exceptions(stream, raw, "0", &exbits, TRUE);
			n++;
		    }
		    else
		      i++;
		}

		if(n && pat->action->folder){
		    PATTERN_S *p;
		    int	       err = 0;

		    tmpmap = NULL;
		    mn_init(&tmpmap, stream->nmsgs);

		    /*
		     * For everything matching msg that hasn't
		     * already been saved somewhere, do it...
		     */
		    for(i = 1L, n = 0L; i <= stream->nmsgs; i++)
		      if((mc = mail_elt(stream, i)) && mc->searched
			 && !(msgno_exceptions(stream, i, "0", &exbits, FALSE)
			      && (exbits & MSG_EX_FILED)))
			if(!n++){
			    mn_set_cur(tmpmap, i);
			}
			else{
			    mn_add_cur(tmpmap, i);
			}

		    /*
		     * Remove already deleted messages from the tmp
		     * message map.
		     */
		    if(n && pat->action->move_only_if_not_deleted){
			char         *seq;
			MSGNO_S      *tmpmap2 = NULL;
			long          nn = 0L;
			MESSAGECACHE *mc;

			mn_init(&tmpmap2, stream->nmsgs);

			/*
			 * First, make sure elts are valid for all the
			 * interesting messages.
			 */
			if(seq = invalid_elt_sequence(stream, tmpmap)){
			    pine_mail_fetch_flags(stream, seq, NIL);
			    fs_give((void **) &seq);
			}

			for(i = mn_first_cur(tmpmap); i > 0L;
			    i = mn_next_cur(tmpmap)){
			    mc = ((raw = mn_m2raw(tmpmap, i)) > 0L
			          && stream && raw <= stream->nmsgs)
				    ? mail_elt(stream, raw) : NULL;
			    if(mc && !mc->deleted){
				if(!nn++){
				    mn_set_cur(tmpmap2, i);
				}
				else{
				    mn_add_cur(tmpmap2, i);
				}
			    }
			}

			mn_give(&tmpmap);
			tmpmap = tmpmap2;
			n = nn;
		    }

		    if(n){
			for(p = pat->action->folder; p; p = p->next){
			  int dval;
			  int flags_for_save;
			  char *processed_name;

			  /* does this filter set delete bit? ... */
			  convert_statebits_to_vals(pat->action->state_setting_bits, &dval, NULL, NULL, NULL);
			  /* ... if so, tell save not to fix it before copy */
			  flags_for_save = SV_FOR_FILT |
				  (nt ? 0 : SV_DELETE) |
				  ((dval != ACT_STAT_SET) ? SV_FIX_DELS : 0);
			  processed_name = detoken_src(p->substring,
						       FOR_FILT, NULL,
						       NULL, NULL, NULL);
			  if(move_filtered_msgs(stream, tmpmap,
					    (processed_name && *processed_name)
					      ? processed_name : p->substring,
					    flags_for_save, nick)){
			      /*
			       * If we filtered into the current
			       * folder, chuck a ping down the
			       * stream so the user can notice it
			       * before the next new mail check...
			       */
			      if(ps_global->mail_stream
				 && ps_global->mail_stream != stream
				 && match_pattern_folder_specific(
						 pat->action->folder,
						 ps_global->mail_stream,
						 FOR_FILT)){
				  (void) pine_mail_ping(ps_global->mail_stream);
			      }				
			  }
			  else{
			      err = 1;
			      break;
			  }

			  if(processed_name)
			    fs_give((void **) &processed_name);
			}

			if(!err)
			  for(n = mn_first_cur(tmpmap);
			      n > 0L;
			      n = mn_next_cur(tmpmap)){

			      if(msgno_exceptions(stream, mn_m2raw(tmpmap, n),
						  "0", &exbits, FALSE))
				exbits |= (nt ? MSG_EX_FILEONCE : MSG_EX_FILED);
			      else
				exbits = (nt ? MSG_EX_FILEONCE : MSG_EX_FILED);

			      exbits &= ~MSG_EX_STATECHG;

			      msgno_exceptions(stream, mn_m2raw(tmpmap, n),
					       "0", &exbits, TRUE);
			  }
		    }

		    mn_give(&tmpmap);
		}

		mail_free_searchset(&srchset);
		if(charset)
		  fs_give((void **) &charset);
	    }

	    /*
	     * If this is the last rule,
	     * we make sure we delete messages that we delayed deleting
	     * in the save. We delayed so that the deletion wouldn't have
	     * an effect on later rules. We convert any temporary
	     * FILED (FILEONCE) and FILTERED (FILTONCE) flags
	     * (which were set by an earlier non-terminating rule)
	     * to permanent. We also exclude some messages from the view.
	     */
	    if(pending_actions && !nextpat){

		pending_actions = 0;
		tmpmap = NULL;
		mn_init(&tmpmap, stream->nmsgs);

		for(i = 1L, n = 0L; i <= mn_get_total(msgmap); i++){

		    raw = mn_m2raw(msgmap, i);
		    if(msgno_exceptions(stream, raw, "0", &exbits, FALSE)){
			if(exbits & MSG_EX_FILEONCE){
			    if(!n++){
				mn_set_cur(tmpmap, raw);
			    }
			    else{
				mn_add_cur(tmpmap, raw);
			    }
			}
		    }
		}

		if(n)
		  set_some_flags(stream, tmpmap, F_DEL, NULL, NULL, 0, NULL);

		mn_give(&tmpmap);

		for(i = 1L; i <= mn_get_total(msgmap); i++){
		    raw = mn_m2raw(msgmap, i);
		    if(msgno_exceptions(stream, raw, "0", &exbits, FALSE)){
			if(exbits & MSG_EX_PEND_EXLD){
			    if(!cleared_index_cache
			       && stream == ps_global->mail_stream){
				cleared_index_cache = 1;
				clear_index_cache();
			    }

			    msgno_exclude(stream, msgmap, i, 1);
			    if(msgno_exceptions(stream, raw, "0",
						&exbits, FALSE)
			       && (exbits & MSG_EX_RECENT)){
				long l, ll;

				/* 
				 * If this message is new, decrement
				 * new_mail_count.  See the above
				 * call to msgno_exclude.
				 */
				l = sp_new_mail_count(stream);
				ll = sp_recent_since_visited(stream);
				dprint(5, (debugfile, "New message being filtered. Decrement new_mail_count: %ld -> %ld\n", l, l-1L));
				if(l > 0L)
				  sp_set_new_mail_count(stream, l - 1L);
				if(ll > 0L)
				  sp_set_recent_since_visited(stream, ll - 1L);
			    }

			    i--;   /* to compensate for loop's i++ */
			}

			/* get rid of temporary flags */
			if(exbits & (MSG_EX_FILTONCE | MSG_EX_FILEONCE |
			             MSG_EX_PEND_EXLD)){
			    if(exbits & MSG_EX_FILTONCE){
				/* convert to permament */
				exbits ^= MSG_EX_FILTONCE;
				exbits |= MSG_EX_FILTERED;
			    }

			    /* convert to permament */
			    if(exbits & MSG_EX_FILEONCE){
				exbits ^= MSG_EX_FILEONCE;
				exbits |= MSG_EX_FILED;
			    }

			    if(exbits & MSG_EX_PEND_EXLD)
			      exbits ^= MSG_EX_PEND_EXLD;

			    exbits &= ~MSG_EX_STATECHG;

			    msgno_exceptions(stream, raw, "0", &exbits,TRUE);
			}
		    }
		}
	    }
	}

	/* New mail arrival means start over */
	if(mail_uid(stream, stream->nmsgs) == uid)
	  break;
	/* else, go again */

	recent = 1; /* only check recent ones now */
    }

    if(!recent){
	/* clear status change flags */
	for(i = 1; i <= stream->nmsgs; i++){
	    if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
		if(exbits & MSG_EX_STATECHG){
		    exbits &= ~MSG_EX_STATECHG;
		    msgno_exceptions(stream, i, "0", &exbits, TRUE);
		}
	    }
	}
    }

    /* clear any private "recent" flags and add TESTED flag */
    for(i = 1; i <= stream->nmsgs; i++){
	if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
	    if(exbits & MSG_EX_RECENT
	       || !(exbits & MSG_EX_TESTED)
	       || (!recent && exbits & MSG_EX_STATECHG)){
		exbits &= ~MSG_EX_RECENT;
		exbits |= MSG_EX_TESTED;
		if(!recent)
		  exbits &= ~MSG_EX_STATECHG;

		msgno_exceptions(stream, i, "0", &exbits, TRUE);
	    }
	}
	else{
	    exbits = MSG_EX_TESTED;
	    msgno_exceptions(stream, i, "0", &exbits, TRUE);
	}

	/* clear any stmp flags just in case */
	if((mc = mail_elt(stream, i)) != NULL)
	  mc->spare6 = 0;
    }

    msgmap->flagged_stmp = 0L;

    if(any_msgs && F_OFF(F_QUELL_FILTER_MSGS, ps_global)
       && F_OFF(F_QUELL_FILTER_DONE_MSG, ps_global)){
	q_status_message(SM_ORDER, 0, 1, "filtering done");
	display_message('x');
    }
}


/*
 * Re-check the filters for matches because a change of message state may
 * have changed the results.
 */
void
reprocess_filter_patterns(stream, msgmap, flags)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    int         flags;
{
    if(stream){
	long i;
	int  exbits;

	if(msgno_include(stream, msgmap, flags)
	   && stream == ps_global->mail_stream
	   && !(flags & MI_CLOSING)){
	    clear_index_cache();
	    refresh_sort(stream, msgmap, SRT_NON);
	    ps_global->mangled_header = 1;
	}

	/*
	 * Passing 1 in the last argument causes it to only look at the
	 * messages we included above, which should be only the ones we
	 * need to look at.
	 */
	process_filter_patterns(stream, msgmap,
				(flags & MI_STATECHGONLY) ? 1L : 0);

	/* clear status change flags */
	for(i = 1; i <= stream->nmsgs; i++){
	    if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
		if(exbits & MSG_EX_STATECHG){
		    exbits &= ~MSG_EX_STATECHG;
		    msgno_exceptions(stream, i, "0", &exbits, TRUE);
		}
	    }
	}
    }
}


/*
 * When killing or filtering we don't want to match by mistake. So if
 * a pattern has nothing set except the Current Folder Type (which is always
 * set to something) we'll consider it to be trivial and not a match.
 * match_pattern uses this to determine if there is a match, where it is
 * just triggered on the Current Folder Type.
 */
int
trivial_patgrp(patgrp)
    PATGRP_S *patgrp;
{
    int ret = 1;

    if(patgrp){
	if(patgrp->subj || patgrp->cc || patgrp->from || patgrp->to ||
	   patgrp->sender || patgrp->news || patgrp->recip || patgrp->partic ||
	   patgrp->alltext || patgrp->bodytext)
	  ret = 0;

	if(ret && patgrp->do_age)
	  ret = 0;

	if(ret && patgrp->do_size)
	  ret = 0;

	if(ret && patgrp->do_score)
	  ret = 0;

	if(ret && patgrp->category_cmd && patgrp->category_cmd[0])
	  ret = 0;

	if(ret && patgrp_depends_on_state(patgrp))
	  ret = 0;

	if(ret && patgrp->stat_8bitsubj != PAT_STAT_EITHER)
	  ret = 0;

	if(ret && patgrp->charsets)
	  ret = 0;

	if(ret && patgrp->stat_bom != PAT_STAT_EITHER)
	  ret = 0;

	if(ret && patgrp->stat_boy != PAT_STAT_EITHER)
	  ret = 0;

	if(ret && patgrp->abookfrom != AFRM_EITHER)
	  ret = 0;

	if(ret && patgrp->arbhdr){
	    ARBHDR_S *a;

	    for(a = patgrp->arbhdr; a && ret; a = a->next)
	      if(a->field && a->field[0] && a->p)
		ret = 0;
	}
    }

    return(ret);
}


int
some_filter_depends_on_active_state()
{
    long          rflags = ROLE_DO_FILTER;
    PAT_S        *pat;
    PAT_STATE     pstate;
    int           ret = 0;

    if(nonempty_patterns(rflags, &pstate)){

	for(pat = first_pattern(&pstate);
	    pat && !ret;
	    pat = next_pattern(&pstate))
	  if(patgrp_depends_on_active_state(pat->patgrp))
	    ret++;
    }

    return(ret);
}


/*----------------------------------------------------------------------
  Move all messages with sequence bit lit to dstfldr
 
  Args: stream -- stream to use
	msgmap -- map of messages to be moved
	dstfldr -- folder to receive moved messages
	flags_for_save

  Returns: nonzero on success or on readonly stream
  ----*/
int
move_filtered_msgs(stream, msgmap, dstfldr, flags_for_save, nick)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    char       *dstfldr;
    int         flags_for_save;
    char       *nick;
{
    long	  n;
    int           we_cancel = 0, width;
    CONTEXT_S	 *save_context = NULL;
    char	  buf[MAX_SCREEN_COLS+1], sbuf[MAX_SCREEN_COLS+1];
    char         *save_ref = NULL;
#define	FILTMSG_MAX	30

    if(!stream)
      return 0;
    
    if(READONLY_FOLDER(stream)){
	dprint(1, (debugfile,
		"Can't delete messages in readonly folder \"%s\"\n",
		STREAMNAME(stream)));
	q_status_message1(SM_ORDER, 1, 3,
			 "Can't delete messages in readonly folder \"%s\"",
			 STREAMNAME(stream));
	return 1;
    }

    buf[0] = '\0';

    width = max(10, ps_global->ttyo ? ps_global->ttyo->screen_cols : 80);
    sprintf(buf, "%.30s%.2sMoving %.10s filtered message%.2s to \"\"",
	    nick ? nick : "", nick ? ": " : "",
	    comatose(mn_total_cur(msgmap)), plural(mn_total_cur(msgmap)));
    /* 2 is for brackets, 5 is for " DONE" in busy alarm */
    width -= (strlen(buf) + 2 + 5);
    sprintf(buf, "%.30s%.2sMoving %.10s filtered message%.2s to \"%s\"",
	    nick ? nick : "", nick ? ": " : "",
	    comatose(mn_total_cur(msgmap)), plural(mn_total_cur(msgmap)),
	    short_str(dstfldr, sbuf, width, FrontDots));

    dprint(5, (debugfile, "%s\n", buf));

    if(F_OFF(F_QUELL_FILTER_MSGS, ps_global))
      we_cancel = busy_alarm(1, buf, NULL, 1);

    if(!is_absolute_path(dstfldr)
       && !(save_context = default_save_context(ps_global->context_list)))
      save_context = ps_global->context_list;

    /*
     * Because this save is happening independent of where the user is
     * in the folder hierarchy and has nothing to do with that, we want
     * to ignore the reference field built into the context. Zero it out
     * temporarily here so it won't affect the results of context_apply
     * in save.
     *
     * This might be a problem elsewhere, as well. The same thing as this
     * is also done in match_pattern_folder_specific, which is also only
     * called from within process_filter_patterns. But there could be
     * others. We could have a separate function, something like
     * copy_default_save_context(), that automatically zeroes out the
     * reference field in the copy. However, some of the uses of
     * default_save_context() require that a pointer into the actual
     * context list is returned, so this would have to be done carefully.
     * Besides, we don't know of any other problems so we'll just change
     * these known cases for now.
     */
    if(save_context && save_context->dir){
	save_ref = save_context->dir->ref;
	save_context->dir->ref = NULL;
    }

    n = save(ps_global, stream, save_context, dstfldr, msgmap, flags_for_save);

    if(save_ref)
      save_context->dir->ref = save_ref;

    if(n != mn_total_cur(msgmap)){
	int   exbits;
	long  x;

	buf[0] = '\0';

	/* Clear "filtered" flags for failed messages */
	for(x = mn_first_cur(msgmap); x > 0L; x = mn_next_cur(msgmap))
	  if(n-- <= 0 && msgno_exceptions(stream, mn_m2raw(msgmap, x),
					  "0", &exbits, FALSE)){
	      exbits &= ~(MSG_EX_FILTONCE | MSG_EX_FILEONCE |
			  MSG_EX_FILTERED | MSG_EX_FILED);
	      msgno_exceptions(stream, mn_m2raw(msgmap, x),
			       "0", &exbits, TRUE);
	  }

	/* then re-incorporate them into folder they belong */
	(void) msgno_include(stream, sp_msgmap(stream), MI_NONE);
	clear_index_cache();
	refresh_sort(stream, sp_msgmap(stream), SRT_NON);
	ps_global->mangled_header = 1;
    }
    else{
	sprintf(buf, "Filtered all %s message%s to \"%.45s\"",
		comatose(n), plural(n), dstfldr);
	dprint(5, (debugfile, "%s\n", buf));
    }

    if(we_cancel)
      cancel_busy_alarm(buf[0] ? 0 : -1);

    return(buf[0] != '\0');
}


/*----------------------------------------------------------------------
  Move all messages with sequence bit lit to dstfldr
 
  Args: stream -- stream to use
	msgmap -- which messages to set
	flagbits -- which flags to set or clear
	kw_on  -- keywords to set
	kw_off -- keywords to clear
	verbose -- 1 => busy alarm after 1 second
	           2 => forced busy alarm
  ----*/
void
set_some_flags(stream, msgmap, flagbits, kw_on, kw_off, verbose, nick)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    long        flagbits;
    char      **kw_on;
    char      **kw_off;
    int         verbose;
    char       *nick;
{
    long	  count = 0L, flipped_flags;
    int           we_cancel = 0;
    char          buf[150], *seq;

    if(!stream)
      return;
    
    if(READONLY_FOLDER(stream)){
	dprint(1, (debugfile, "Can't set flags in readonly folder \"%s\"\n",
		STREAMNAME(stream)));
	q_status_message1(SM_ORDER, 1, 3,
			 "Can't set flags in readonly folder \"%s\"",
			 STREAMNAME(stream));
	return;
    }

    /* use this to determine if anything needs to be done */
    flipped_flags = ((flagbits & F_ANS)    ? F_UNANS : 0)       |
		    ((flagbits & F_UNANS)  ? F_ANS : 0)         |
		    ((flagbits & F_FLAG)   ? F_UNFLAG : 0)      |
		    ((flagbits & F_UNFLAG) ? F_FLAG : 0)        |
		    ((flagbits & F_DEL)    ? F_UNDEL : 0)       |
		    ((flagbits & F_UNDEL)  ? F_DEL : 0)         |
		    ((flagbits & F_SEEN)   ? F_UNSEEN : 0)      |
		    ((flagbits & F_UNSEEN) ? F_SEEN : 0)        |
		    ((flagbits & F_KEYWORD) ? F_UNKEYWORD : 0)  |
		    ((flagbits & F_UNKEYWORD) ? F_KEYWORD : 0);
    if(seq = currentf_sequence(stream, msgmap, flipped_flags, &count, 0,
			       kw_off, kw_on)){
	char *sets = NULL, *clears = NULL;
	char *ps, *pc, **t;
	size_t len;

	/* allocate enough space for mail_flags arguments */
	for(len=100, t = kw_on; t && *t; t++)
	  len += (strlen(*t) + 1);
	
	sets = (char *) fs_get(len * sizeof(*sets));

	for(len=100, t = kw_off; t && *t; t++)
	  len += (strlen(*t) + 1);
	
	clears = (char *) fs_get(len * sizeof(*clears));

	sets[0] = clears[0] = '\0';
	ps = sets;
	pc = clears;

	sprintf(buf, "%.30s%.2sSetting flags in %.10s message%.10s",
		nick ? nick : "", nick ? ": " : "",
		comatose(count), plural(count));

	if(F_OFF(F_QUELL_FILTER_MSGS, ps_global))
	  we_cancel = busy_alarm(1, buf, NULL, verbose ? 1 : 0);

	/*
	 * What's going on here? If we want to set more than one flag
	 * we can do it with a single roundtrip by combining the arguments
	 * into a single call and separating them with spaces.
	 */
	if(flagbits & F_ANS)
	  sstrcpy(&ps, "\\ANSWERED");
	if(flagbits & F_FLAG){
	    if(ps > sets)
	      sstrcpy(&ps, " ");

	    sstrcpy(&ps, "\\FLAGGED");
	}
	if(flagbits & F_DEL){
	    if(ps > sets)
	      sstrcpy(&ps, " ");

	    sstrcpy(&ps, "\\DELETED");
	}
	if(flagbits & F_SEEN){
	    if(ps > sets)
	      sstrcpy(&ps, " ");

	    sstrcpy(&ps, "\\SEEN");
	}
	if(flagbits & F_KEYWORD){
	    for(t = kw_on; t && *t; t++){
		int i;

		/*
		 * We may be able to tell that this will fail before
		 * we actually try it.
		 */
		if(stream->kwd_create ||
		   (((i=user_flag_index(stream, *t)) >= 0) && i < NUSERFLAGS)){
		    if(ps > sets)
		      sstrcpy(&ps, " ");
		    
		    sstrcpy(&ps, *t);
		}
		else{
		  int some_defined = 0;
		  static int msg_delivered = 0;

		  for(i = 0; !some_defined && i < NUSERFLAGS; i++)
		    if(stream->user_flags[i])
		      some_defined++;
		
		  if(msg_delivered++ < 2){
		    char b[200], c[200], *p;
		    int w;

		    if(some_defined){
		      sprintf(b, "Can't set \"%.30s\". No more keywords in ", keyword_to_nick(*t));
		      w = min((ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - strlen(b) - 1 - 2, sizeof(c)-1);
		      p = short_str(STREAMNAME(stream), c, w, FrontDots);
		      q_status_message2(SM_ORDER, 3, 3, "%s%s!", b, p);
		    }
		    else{
		      sprintf(b, "Can't set \"%.30s\". Can't add keywords in ", keyword_to_nick(*t));
		      w = min((ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - strlen(b) - 1 - 2, sizeof(c)-1);
		      p = short_str(STREAMNAME(stream), c, w, FrontDots);
		      q_status_message2(SM_ORDER, 3, 3, "%s%s!", b, p);
		    }
		  }

		  if(some_defined){
		    dprint(1, (debugfile, "Can't set keyword \"%s\". No more keywords allowed in %s\n", *t, stream->mailbox ? stream->mailbox : "target folder"));
		  }
		  else{
		    dprint(1, (debugfile, "Can't set keyword \"%s\". Can't add keywords in %s\n", *t, stream->mailbox ? stream->mailbox : "target folder"));
		  }
		}
	    }
	}

	/* need a separate call for the clears */
	if(flagbits & F_UNANS)
	  sstrcpy(&pc, "\\ANSWERED");
	if(flagbits & F_UNFLAG){
	    if(pc > clears)
	      sstrcpy(&pc, " ");

	    sstrcpy(&pc, "\\FLAGGED");
	}
	if(flagbits & F_UNDEL){
	    if(pc > clears)
	      sstrcpy(&pc, " ");

	    sstrcpy(&pc, "\\DELETED");
	}
	if(flagbits & F_UNSEEN){
	    if(pc > clears)
	      sstrcpy(&pc, " ");

	    sstrcpy(&pc, "\\SEEN");
	}
	if(flagbits & F_UNKEYWORD){
	    for(t = kw_off; t && *t; t++){
		if(pc > clears)
		  sstrcpy(&pc, " ");
		
		sstrcpy(&pc, *t);
	    }
	}


	if(sets[0])
	  mail_flag(stream, seq, sets, ST_SET);

	if(clears[0])
	  mail_flag(stream, seq, clears, 0L);

	fs_give((void **) &sets);
	fs_give((void **) &clears);
	fs_give((void **) &seq);

	if(we_cancel)
	  cancel_busy_alarm(buf[0] ? 0 : -1);
    }
}



/*
 * Delete messages which are marked FILTERED and excluded.
 * Messages which are FILTERED but not excluded are those that have had
 * their state set by a filter pattern, but are to remain in the same
 * folder.
 */
void
delete_filtered_msgs(stream)
    MAILSTREAM *stream;
{
    int	  exbits;
    long  i;
    char *seq;
    MESSAGECACHE *mc;

    for(i = 1L; i <= stream->nmsgs; i++)
      if(msgno_exceptions(stream, i, "0", &exbits, FALSE)
	 && (exbits & MSG_EX_FILTERED)
	 && get_lflag(stream, NULL, i, MN_EXLD)){
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->sequence = 1;
      }
      else if((mc = mail_elt(stream, i)) != NULL)
	mc->sequence = 0;

    if(seq = build_sequence(stream, NULL, NULL)){
	mail_flag(stream, seq, "\\DELETED", ST_SET | ST_SILENT);
	fs_give((void **) &seq);
    }
}



/*----------------------------------------------------------------------
  Move all read messages from srcfldr to dstfldr
 
  Args: stream -- stream to usr
	dstfldr -- folder to receive moved messages
	buf -- place to write success message

  Returns: success message or NULL for failure
  ----*/
char *
move_read_msgs(stream, dstfldr, buf, searched)
    MAILSTREAM *stream;
    char       *dstfldr, *buf;
    long	searched;
{
    long	  i, raw;
    int           we_cancel = 0;
    MSGNO_S	 *msgmap = NULL;
    CONTEXT_S	 *save_context = NULL;
    char	  *bufp = NULL;
    MESSAGECACHE *mc;

    if(!is_absolute_path(dstfldr)
       && !(save_context = default_save_context(ps_global->context_list)))
      save_context = ps_global->context_list;

    /*
     * Use the "sequence" bit to select the set of messages
     * we want to save.  If searched is non-neg, the message
     * cache already has the necessary "sequence" bits set.
     */
    if(searched < 0L)
      searched = count_flagged(stream, F_SEEN | F_UNDEL);

    if(searched){
	mn_init(&msgmap, stream->nmsgs);
	for(i = 1L; i <= mn_get_total(msgmap); i++)
	  set_lflag(stream, msgmap, i, MN_SLCT, 0);

	/*
	 * re-init msgmap to fix the MN_SLCT count, "flagged_tmp", in
	 * case there were any flagged such before we got here.
	 *
	 * BUG: this means the count of MN_SLCT'd msgs in the
	 * folder's real msgmap is instantly bogus.  Until Cancel
	 * after "Really quit?" is allowed, this isn't a problem since
	 * that mapping table is either gone or about to get nuked...
	 */
	mn_init(&msgmap, stream->nmsgs);

	/* select search results */
	for(i = 1L; i <= mn_get_total(msgmap); i++)
	  if((raw = mn_m2raw(msgmap, i)) > 0L && stream
	     && raw <= stream->nmsgs
	     && (mc = mail_elt(stream,raw))
	     && ((mc->valid && mc->seen && !mc->deleted)
	         || (!mc->valid && mc->searched)))
	    set_lflag(stream, msgmap, i, MN_SLCT, 1);

	pseudo_selected(msgmap);
	sprintf(buf, "Moving %s read message%s to \"%.45s\"",
		comatose(searched), plural(searched), dstfldr);
	we_cancel = busy_alarm(1, buf, NULL, 1);
	if(save(ps_global, stream, save_context, dstfldr, msgmap,
		SV_DELETE | SV_FIX_DELS) == searched)
	  strncpy(bufp = buf + 1, "Moved", 5); /* change Moving to Moved */

	mn_give(&msgmap);
	if(we_cancel)
	  cancel_busy_alarm(bufp ? 0 : -1);
    }

    return(bufp);
}



/*----------------------------------------------------------------------
  Move read messages from folder if listed in archive
 
  Args: 

  ----*/
int
read_msg_prompt(n, f)
    long  n;
    char *f;
{
    char buf[MAX_SCREEN_COLS+1];

    sprintf(buf, "Save the %ld read message%s in \"%.*s\"", n, plural(n),
	    sizeof(buf)-50, f);
    return(want_to(buf, 'y', 0, NO_HELP, WT_NORM) == 'y');
}



/*----------------------------------------------------------------------
  Move read messages from folder if listed in archive
 
  Args: 

  ----*/
char *
move_read_incoming(stream, context, folder, archive, buf)
    MAILSTREAM *stream;
    CONTEXT_S  *context;
    char       *folder;
    char      **archive;
    char       *buf;
{
    char *s, *d, *f = folder;
    long  seen_undel;

    buf[0] = '\0';

    if(archive && !sp_flagged(stream, SP_INBOX)
       && context && (context->use & CNTXT_INCMNG)
       && ((context_isambig(folder)
	    && folder_is_nick(folder, FOLDERS(context), 0))
	   || folder_index(folder, context, FI_FOLDER) > 0)
       && (seen_undel = count_flagged(stream, F_SEEN | F_UNDEL))){

	for(; f && *archive; archive++){
	    char *p;

	    get_pair(*archive, &s, &d, 1, 0);
	    if(s && d
	       && (!strcmp(s, folder)
		   || (context_isambig(folder)
		       && (p = folder_is_nick(folder, FOLDERS(context), 0))
		       && !strcmp(s, p)))){
		if(F_ON(F_AUTO_READ_MSGS,ps_global)
		   || read_msg_prompt(seen_undel, d))
		  buf = move_read_msgs(stream, d, buf, seen_undel);

		f = NULL;		/* bust out after cleaning up */
	    }

	    fs_give((void **)&s);
	    fs_give((void **)&d);
	}
    }

    return((buf && *buf) ? buf : NULL);
}


/*----------------------------------------------------------------------
    Delete all references to a deleted news posting

 
  ---*/
void
cross_delete_crossposts(stream)
    MAILSTREAM *stream;
{
    if(count_flagged(stream, F_DEL)){
	static char *fields[] = {"Xref", NULL};
	MAILSTREAM  *tstream;
	CONTEXT_S   *fake_context;
	char	    *xref, *p, *group, *uidp,
		    *newgrp, newfolder[MAILTMPLEN];
	long	     i, uid, hostlatch = 0L;
	int	     we_cancel = 0;
	MESSAGECACHE *mc;

	strncpy(newfolder, stream->mailbox, sizeof(newfolder));
	newfolder[sizeof(newfolder)-1] = '\0';
	if(!(newgrp = strstr(newfolder, "#news.")))
	  return;				/* weird mailbox */

	newgrp += 6;

	we_cancel = busy_alarm(1, "Busy deleting crosspostings", NULL, 0);

	/* build subscribed list */
	strcpy(newgrp, "[]");
	fake_context = new_context(newfolder, 0);
	build_folder_list(NULL, fake_context, "*", NULL, BFL_LSUB);

	for(i = 1L; i <= stream->nmsgs; i++)
	  if(!get_lflag(stream, NULL, i, MN_EXLD)
	     && (mc = mail_elt(stream, i)) && mc->deleted){

	      if(xref = pine_fetchheader_lines(stream, i, NULL, fields)){
		  if(p = strstr(xref, ": ")){
		      p	     += 2;
		      hostlatch = 0L;
		      while(*p){
			  group = p;
			  uidp  = NULL;

			  /* get server */
			  while(*++p && !isspace((unsigned char) *p))
			    if(*p == ':'){
				*p   = '\0';
				uidp = p + 1;
			    }

			  /* tie off uid/host */
			  if(*p)
			    *p++ = '\0';

			  if(uidp){
			      /*
			       * For the nonce, we're only deleting valid
			       * uid's from outside the current newsgroup
			       * and inside only subscribed newsgroups
			       */
			      if(strcmp(group, stream->mailbox
							+ (newgrp - newfolder))
				 && folder_index(group, fake_context,
						 FI_FOLDER) >= 0){
				  if(uid = atol(uidp)){
				      strcpy(newgrp, group);
				      if(tstream = pine_mail_open(NULL,
								  newfolder,
								  SP_USEPOOL,
								  NULL)){
					  mail_flag(tstream, long2string(uid),
						    "\\DELETED",
						    ST_SET | ST_UID);
					  pine_mail_close(tstream);
				      }
				  }
				  else
				    break;		/* bogus uid */
			      }
			  }
			  else if(!hostlatch++){
			      char *p, *q;

			      if(stream->mailbox[0] == '{'
				 && !((p = strpbrk(stream->mailbox+1, "}:/"))
				      && !struncmp(stream->mailbox + 1,
						   q = canonical_name(group),
						   p - (stream->mailbox + 1))
				      && q[p - (stream->mailbox + 1)] == '\0'))
				break;		/* different server? */
			  }
			  else
			    break;		/* bogus field! */
		      }
		  }
		  
		  fs_give((void **) &xref);
	      }
	  }

	free_context(&fake_context);

	if(we_cancel)
	  cancel_busy_alarm(0);
    }
}



/*----------------------------------------------------------------------
    Print current message[s] or folder index

    Args: state -- pointer to struct holding a bunch of pine state
	 msgmap -- table mapping msg nums to c-client sequence nums
	    agg -- boolean indicating we're to operate on aggregate set
       in_index -- boolean indicating we're called from Index Screen

 Filters the original header and sends stuff to printer
  ---*/
void
cmd_print(state, msgmap, agg, in_index)
     struct pine *state;
     MSGNO_S     *msgmap;
     int	  agg;
     CmdWhere	  in_index;
{
    char      prompt[250];
    long      i, msgs, rawno;
    int	      next = 0, do_index = 0;
    ENVELOPE *e;
    BODY     *b;
    MESSAGECACHE *mc;

    if(agg && !pseudo_selected(msgmap))
      return;

    msgs = mn_total_cur(msgmap);

    if((in_index != View) && F_ON(F_PRINT_INDEX, state)){
	char m[10];
	int  ans;
	static ESCKEY_S prt_opts[] = {
	    {'i', 'i', "I", "Index"},
	    {'m', 'm', "M", NULL},
	    {-1, 0, NULL, NULL}};

	if(in_index == ThrdIndx){
	    if(want_to("Print Index", 'y', 'x', NO_HELP, WT_NORM) == 'y')
	      ans = 'i';
	    else
	      ans = 'x';
	}
	else{
	    sprintf(m, "Message%s", (msgs>1L) ? "s" : "");
	    prt_opts[1].label = m;
	    sprintf(prompt, "Print %sFolder Index or %s %s? ",
		(agg==2) ? "thread " : agg ? "selected " : "",
		(agg==2) ? "thread" : agg ? "selected" : "current", m);

	    ans = radio_buttons(prompt, -FOOTER_ROWS(state), prt_opts, 'm', 'x',
				NO_HELP, RB_NORM|RB_SEQ_SENSITIVE, NULL);
	}

	switch(ans){
	  case 'x' :
	    cmd_cancelled("Print");
	    if(agg)
	      restore_selected(msgmap);

	    return;

	  case 'i':
	    do_index = 1;
	    break;

	  default :
	  case 'm':
	    break;
	}
    }

    if(do_index)
      sprintf(prompt, "%sFolder Index ",
	      (agg==2) ? "Thread " : agg ? "Selected " : "");
    else if(msgs > 1L)
      sprintf(prompt, "%s messages ", long2string(msgs));
    else
      sprintf(prompt, "Message %s ", long2string(mn_get_cur(msgmap)));

    if(open_printer(prompt) < 0){
	if(agg)
	  restore_selected(msgmap);

	return;
    }
    
    if(do_index){
	TITLE_S *tc;

	tc = format_titlebar();

	/* Print titlebar... */
	print_text1("%s\n\n", tc ? tc->titlebar_line : "");
	/* then all the index members... */
	if(!print_index(state, msgmap, agg))
	  q_status_message(SM_ORDER | SM_DING, 3, 3,
			   "Error printing folder index");
    }
    else{
        for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap), next++){
	    if(next && F_ON(F_AGG_PRINT_FF, state))
	      if(!print_char(FORMFEED))
	        break;

	    if(!(state->mail_stream
	       && (rawno = mn_m2raw(msgmap, i)) > 0L
	       && rawno <= state->mail_stream->nmsgs
	       && (mc = mail_elt(state->mail_stream, rawno))
	       && mc->valid))
	      mc = NULL;

	    if(!(e=pine_mail_fetchstructure(state->mail_stream,
					    mn_m2raw(msgmap,i),
					    &b))
	       || (F_ON(F_FROM_DELIM_IN_PRINT, ps_global)
		   && !bezerk_delimiter(e, mc, print_char, next))
	       || !format_message(mn_m2raw(msgmap, mn_get_cur(msgmap)),
				  e, b, NULL, FM_NEW_MESS | FM_NOINDENT,
				  print_char)){
	        q_status_message(SM_ORDER | SM_DING, 3, 3,
			       "Error printing message");
	        break;
	    }
        }
    }

    close_printer();

    if(agg)
      restore_selected(msgmap);
}



/*
 * Support structure and functions to support piping raw message texts...
 */
static struct raw_pipe_data {
    MAILSTREAM   *stream;
    unsigned long msgno, len;
    long	  char_limit, flags;
    char         *cur, *body;
} raw_pipe;


int
raw_pipe_getc(c)
    unsigned char *c;
{
    static char *free_this = NULL;

    /*
     * What is this if doing?
     *
     * If((just_starting && unsuccessful_fetch_header)
     *    || (no_chars_available && haven't_fetched_body
     *        && (not_supposed_to_fetch
     *            || (supposed_to_fetch_all && unsuccessful_fetch_text)
     *            || (supposed_to_partial_fetch && unsuccessful_partial_fetch))
     *    || (no_chars_available))
     *   return(0);
     *
     * otherwise, fall through and return next character
     */
    if((!raw_pipe.cur
        && !(raw_pipe.cur = mail_fetch_header(raw_pipe.stream, raw_pipe.msgno,
					      NULL, NULL,
					      &raw_pipe.len,
					      raw_pipe.flags)))
       || ((raw_pipe.len <= 0L) && !raw_pipe.body
           && (raw_pipe.char_limit == 0L
	       || (raw_pipe.char_limit < 0L
	           && !(raw_pipe.cur = raw_pipe.body =
				   pine_mail_fetch_text(raw_pipe.stream,
							raw_pipe.msgno,
							NULL,
							&raw_pipe.len,
							raw_pipe.flags)))
	       || (raw_pipe.char_limit > 0L
	           && !(raw_pipe.cur = raw_pipe.body =
		        pine_mail_partial_fetch_wrapper(raw_pipe.stream,
					   raw_pipe.msgno,
					   NULL,
					   &raw_pipe.len,
					   raw_pipe.flags,
					   (unsigned long) raw_pipe.char_limit,
					   &free_this, 1)))))
       || (raw_pipe.len <= 0L)){

	if(free_this)
	  fs_give((void **) &free_this);

	return(0);
    }

    if(raw_pipe.char_limit > 0L
       && raw_pipe.body
       && raw_pipe.len > raw_pipe.char_limit)
      raw_pipe.len = raw_pipe.char_limit;

    if(raw_pipe.len > 0L){
	*c = (unsigned char) *raw_pipe.cur++;
	raw_pipe.len--;
	return(1);
    }
    else
      return(0);

}


/*
 * Set up for using raw_pipe_getc
 *
 * Args: stream
 *       msgno
 *       char_limit  Set to -1 means whole thing
 *                           0 means headers only
 *                         > 0 means headers plus char_limit body chars
 *       flags -- passed to fetch functions
 */
void
prime_raw_pipe_getc(stream, msgno, char_limit, flags)
    MAILSTREAM *stream;
    long	msgno;
    long        char_limit;
    long        flags;
{
    raw_pipe.stream     = stream;
    raw_pipe.msgno      = (unsigned long) msgno;
    raw_pipe.char_limit = char_limit;
    raw_pipe.len        = 0L;
    raw_pipe.flags      = flags;
    raw_pipe.cur        = NULL;
    raw_pipe.body       = NULL;
}



/*----------------------------------------------------------------------
    Pipe message text

   Args: state -- various pine state bits
	 msgmap -- Message number mapping table
	 agg -- whether or not to aggregate the command on selected msgs

   Filters the original header and sends stuff to specified command
  ---*/
void
cmd_pipe(state, msgmap, agg)
     struct pine *state;
     MSGNO_S *msgmap;
     int	  agg;
{
    ENVELOPE      *e;
    MESSAGECACHE  *mc;
    BODY	  *b;
    PIPE_S	  *syspipe;
    char          *resultfilename = NULL, prompt[80];
    int            done = 0;
    gf_io_t	   pc;
    int		   next = 0;
    int            pipe_rv; /* rv of proc to separate from close_system_pipe rv */
    long           i, rawno;
    static int	   capture = 1, raw = 0, delimit = 0, newpipe = 0;
    static char    pipe_command[MAXPATH];
    static ESCKEY_S pipe_opt[] = {
	{0, 0, "", ""},
	{ctrl('W'), 10, "^W", NULL},
	{ctrl('Y'), 11, "^Y", NULL},
	{ctrl('R'), 12, "^R", NULL},
	{0, 13, "^T", NULL},
	{-1, 0, NULL, NULL}
    };

    if(ps_global->restricted){
	q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Pine demo can't pipe messages");
	return;
    }
    else if(!any_messages(msgmap, NULL, "to Pipe"))
      return;

    if(agg){
	if(!pseudo_selected(msgmap))
	  return;
	else
	  pipe_opt[4].ch = ctrl('T');
    }
    else
      pipe_opt[4].ch = -1;

    while (!done) {
	int flags;

	sprintf(prompt, "Pipe %smessage%s%s to %s%s%s%s%s%s%s: ",
		raw ? "RAW " : "",
		agg ? "s" : " ",
		agg ? "" : comatose(mn_get_cur(msgmap)),
		(!capture || delimit || (newpipe && agg)) ? "(" : "",
		capture ? "" : "uncaptured",
		(!capture && delimit) ? "," : "",
		delimit ? "delimited" : "",
		((!capture || delimit) && newpipe && agg) ? "," : "",
		(newpipe && agg) ? "new pipe" : "",
		(!capture || delimit || (newpipe && agg)) ? ") " : "");
	pipe_opt[1].label = raw ? "Shown Text" : "Raw Text";
	pipe_opt[2].label = capture ? "Free Output" : "Capture Output";
	pipe_opt[3].label = delimit ? "No Delimiter" : "With Delimiter";
	pipe_opt[4].label = newpipe ? "To Same Pipe" : "To Individual Pipes";
	flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE;
	switch(optionally_enter(pipe_command, -FOOTER_ROWS(state), 0,
				sizeof(pipe_command), prompt,
				pipe_opt, NO_HELP, &flags)){
	  case -1 :
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Internal problem encountered");
	    done++;
	    break;
      
	  case 10 :			/* flip raw bit */
	    raw = !raw;
	    break;

	  case 11 :			/* flip capture bit */
	    capture = !capture;
	    break;

	  case 12 :			/* flip delimit bit */
	    delimit = !delimit;
	    break;

	  case 13 :			/* flip newpipe bit */
	    newpipe = !newpipe;
	    break;

	  case 0 :
	    if(pipe_command[0]){
		flags = PIPE_USER | PIPE_WRITE | PIPE_STDERR;
		if(!capture){
#ifndef	_WINDOWS
		    ClearScreen();
		    fflush(stdout);
		    clear_cursor_pos();
		    ps_global->mangled_screen = 1;
		    ps_global->in_init_seq = 1;
#endif
		    flags |= PIPE_RESET;
		}

		if(!newpipe && !(syspipe = cmd_pipe_open(pipe_command,
							 (flags & PIPE_RESET)
							   ? NULL
							   : &resultfilename,
							 flags, &pc)))
		  done++;

		for(i = mn_first_cur(msgmap);
		    i > 0L && !done;
		    i = mn_next_cur(msgmap)){
		    e = pine_mail_fetchstructure(ps_global->mail_stream,
						 mn_m2raw(msgmap, i), &b);
		    if(!(state->mail_stream
		       && (rawno = mn_m2raw(msgmap, i)) > 0L
		       && rawno <= state->mail_stream->nmsgs
		       && (mc = mail_elt(state->mail_stream, rawno))
		       && mc->valid))
		      mc = NULL;

		    if((newpipe
			&& !(syspipe = cmd_pipe_open(pipe_command,
						     (flags & PIPE_RESET)
						       ? NULL
						       : &resultfilename,
						     flags, &pc)))
		       || (delimit && !bezerk_delimiter(e, mc, pc, next++)))
		      done++;

		    if(!done){
			if(raw){
			    char    *pipe_err;

			    prime_raw_pipe_getc(ps_global->mail_stream,
						mn_m2raw(msgmap, i), -1L, 0L);
			    gf_filter_init();
			    gf_link_filter(gf_nvtnl_local, NULL);
			    if(pipe_err = gf_pipe(raw_pipe_getc, pc)){
				q_status_message1(SM_ORDER|SM_DING,
						  3, 3,
						  "Internal Error: %.200s",
						  pipe_err);
				done++;
			    }
			}
			else if(!format_message(mn_m2raw(msgmap, i), e, b,
						NULL,
						FM_NEW_MESS | FM_NOWRAP, pc))
			  done++;
		    }

		    if(newpipe)
		      if(close_system_pipe(&syspipe, &pipe_rv, 0) == -1)
			done++;
		}

		if(!capture)
		  ps_global->in_init_seq = 0;

		if(!newpipe)
		  if(close_system_pipe(&syspipe, &pipe_rv, 0) == -1)
		    done++;
		if(done)		/* say we had a problem */
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
				   "Error piping message");
		else if(resultfilename){
		    /* only display if no error */
		    display_output_file(resultfilename, "PIPE MESSAGE",
					NULL, DOF_EMPTY);
		    fs_give((void **)&resultfilename);
		}
		else
		  q_status_message(SM_ORDER, 0, 2, "Pipe command completed");

		done++;
		break;
	    }
	    /* else fall thru as if cancelled */

	  case 1 :
	    cmd_cancelled("Pipe command");
	    done++;
	    break;

	  case 3 :
	    helper(h_common_pipe, "HELP FOR PIPE COMMAND", HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	    break;

	  case 2 :                              /* no place to escape to */
	  case 4 :                              /* can't suspend */
	  default :
	    break;   
	}
    }

    ps_global->mangled_footer = 1;
    if(agg)
      restore_selected(msgmap);
}



/*----------------------------------------------------------------------
  Actually open the pipe used to write piped data down

   Args: 
   Returns: TRUE if success, otherwise FALSE

  ----*/
PIPE_S *
cmd_pipe_open(cmd, result, flags, pc)
    char     *cmd;
    char    **result;
    int       flags;
    gf_io_t  *pc;
{
    char    err[200];
    PIPE_S *pipe;

    if(pipe = open_system_pipe(cmd, result, NULL, flags, 0))
      gf_set_writec(pc, pipe, 0L, PipeStar);
    else{
	sprintf(err, "Error opening pipe: %.100s", cmd ? cmd : "?");
	q_status_message(SM_ORDER | SM_DING, 3, 3, err) ;
    }

    return(pipe);
}



/*----------------------------------------------------------------------
  Screen to offer list management commands contained in message

    Args: state -- pointer to struct holding a bunch of pine state
	 msgmap -- table mapping msg nums to c-client sequence nums
	    agg -- boolean indicating we're to operate on aggregate set

 Result: 

   NOTE: Inspired by contrib from Jeremy Blackman <loki@maison-otaku.net>
 ----*/
void
rfc2369_display(stream, msgmap, msgno)
     MAILSTREAM *stream;
     MSGNO_S	*msgmap;
     long	 msgno;
{
    int	       winner = 0;
    char      *h, *hdrs[MLCMD_COUNT + 1];
    long       index_no = mn_raw2m(msgmap, msgno);
    RFC2369_S  data[MLCMD_COUNT];

    /* for each header field */
    if(h = pine_fetchheader_lines(stream, msgno, NULL, rfc2369_hdrs(hdrs))){
	memset(&data[0], 0, sizeof(RFC2369_S) * MLCMD_COUNT);
	if(rfc2369_parse_fields(h, &data[0])){
	    STORE_S *explain;

	    if(explain = list_mgmt_text(data, index_no)){
		list_mgmt_screen(explain);
		ps_global->mangled_screen = 1;
		so_give(&explain);
		winner++;
	    }
	}

	fs_give((void **) &h);
    }

    if(!winner)
      q_status_message1(SM_ORDER, 0, 3,
		    "Message %.200s contains no list management information",
			comatose(index_no));
}


STORE_S *
list_mgmt_text(data, msgno)
    RFC2369_S *data;
    long       msgno;
{
    STORE_S	  *store;
    int		   i, j, n, fields = 0;
    static  char  *rfc2369_intro1 =
      "<HTML><HEAD></HEAD><BODY><H1>Mail List Commands</H1>Message ";
    static char *rfc2369_intro2[] = {
	" has information associated with it ",
	"that explains how to participate in an email list.  An ",
	"email list is represented by a single email address that ",
	"users sharing a common interest can send messages to (known ",
	"as posting) which are then redistributed to all members ",
	"of the list (sometimes after review by a moderator).",
	"<P>List participation commands in this message include:",
	NULL
    };

    if(store = so_get(CharStar, NULL, EDIT_ACCESS)){

	/* Insert introductory text */
	so_puts(store, rfc2369_intro1);

	so_puts(store, comatose(msgno));

	for(i = 0; rfc2369_intro2[i]; i++)
	  so_puts(store, rfc2369_intro2[i]);

	so_puts(store, "<P>");
	for(i = 0; i < MLCMD_COUNT; i++)
	  if(data[i].data[0].value
	     || data[i].data[0].comment
	     || data[i].data[0].error){
	      if(!fields++)
		so_puts(store, "<UL>");

	      so_puts(store, "<LI>");
	      so_puts(store,
		      (n = (data[i].data[1].value || data[i].data[1].comment))
			? "Methods to "
			: "A method to ");

	      so_puts(store, data[i].field.description);
	      so_puts(store, ". ");

	      if(n)
		so_puts(store, "<OL>");

	      for(j = 0;
		  j < MLCMD_MAXDATA
		  && (data[i].data[j].comment
		      || data[i].data[j].value
		      || data[i].data[j].error);
		  j++){

		  so_puts(store, n ? "<P><LI>" : "<P>");

		  if(data[i].data[j].comment){
		      so_puts(store,
			      "With the provided comment:<P><BLOCKQUOTE>");
		      so_puts(store, data[i].data[j].comment);
		      so_puts(store, "</BLOCKQUOTE><P>");
		  }

		  if(data[i].data[j].value){
		      if(i == MLCMD_POST
			 && !strucmp(data[i].data[j].value, "NO")){
			  so_puts(store,
			   "Posting is <EM>not</EM> allowed on this list");
		      }
		      else{
			  so_puts(store, "Select <A HREF=\"");
			  so_puts(store, data[i].data[j].value);
			  so_puts(store, "\">HERE</A> to ");
			  so_puts(store, (data[i].field.action)
					   ? data[i].field.action
					   : "try it");
		      }

		      so_puts(store, ".");
		  }

		  if(data[i].data[j].error){
		      so_puts(store, "<P>Unfortunately, Pine can not offer");
		      so_puts(store, " to take direct action based upon it");
		      so_puts(store, " because it was improperly formatted.");
		      so_puts(store, " The unrecognized data associated with");
		      so_puts(store, " the \"");
		      so_puts(store, data[i].field.name);
		      so_puts(store, "\" header field was:<P><BLOCKQUOTE>");
		      so_puts(store, data[i].data[j].error);
		      so_puts(store, "</BLOCKQUOTE>");
		  }

		  so_puts(store, "<P>");
	      }

	      if(n)
		so_puts(store, "</OL>");
	  }

	if(fields)
	  so_puts(store, "</UL>");

	so_puts(store, "</BODY></HTML>");
    }

    return(store);
}



static struct key listmgr_keys[] =
       {HELP_MENU,
	NULL_MENU,
	{"E","Exit CmdList",{MC_EXIT,1,{'e'}},KS_EXITMODE},
	{"Ret","[Try Command]",{MC_VIEW_HANDLE,3,
				{ctrl('m'),ctrl('j'),KEY_RIGHT}},KS_NONE},
	{"^B","Prev Cmd",{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F","Next Cmd",{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(listmgr_keymenu, listmgr_keys);
#define	LM_TRY_KEY	3
#define	LM_PREV_KEY	4
#define	LM_NEXT_KEY	5


void
list_mgmt_screen(html)
    STORE_S *html;
{
    int		    cmd = MC_NONE;
    long	    offset = 0L;
    char	   *error = NULL;
    STORE_S	   *store;
    HANDLE_S	   *handles = NULL;
    gf_io_t	    gc, pc;

    do{
	so_seek(html, 0L, 0);
	gf_set_so_readc(&gc, html);

	init_handles(&handles);

	if(store = so_get(CharStar, NULL, EDIT_ACCESS)){
	    gf_set_so_writec(&pc, store);
	    gf_filter_init();

	    gf_link_filter(gf_html2plain,
			   gf_html2plain_opt(NULL, ps_global->ttyo->screen_cols,
					     NULL, &handles, 0));

	    error = gf_pipe(gc, pc);

	    gf_clear_so_writec(store);

	    if(!error){
		SCROLL_S	sargs;

		memset(&sargs, 0, sizeof(SCROLL_S));
		sargs.text.text	   = so_text(store);
		sargs.text.src	   = CharStar;
		sargs.text.desc	   = "list commands";
		sargs.text.handles = handles;
		if(offset){
		    sargs.start.on	   = Offset;
		    sargs.start.loc.offset = offset;
		}

		sargs.bar.title	   = "MAIL LIST COMMANDS";
		sargs.bar.style	   = MessageNumber;
		sargs.resize_exit  = 1;
		sargs.help.text	   = h_special_list_commands;
		sargs.help.title   = "HELP FOR LIST COMMANDS";
		sargs.keys.menu	   = &listmgr_keymenu;
		setbitmap(sargs.keys.bitmap);
		if(!handles){
		    clrbitn(LM_TRY_KEY, sargs.keys.bitmap);
		    clrbitn(LM_PREV_KEY, sargs.keys.bitmap);
		    clrbitn(LM_NEXT_KEY, sargs.keys.bitmap);
		}

		cmd = scrolltool(&sargs);
		offset = sargs.start.loc.offset;
	    }

	    so_give(&store);
	}

	free_handles(&handles);
	gf_clear_so_readc(html);
    }
    while(cmd == MC_RESIZE);
}



/*----------------------------------------------------------------------
 Prompt the user for the type of select desired

   NOTE: any and all functions that successfully exit the second
	 switch() statement below (currently "select_*() functions"),
	 *MUST* update the folder's MESSAGECACHE element's "searched"
	 bits to reflect the search result.  Functions using
	 mail_search() get this for free, the others must update 'em
	 by hand.
  ----*/
void
aggregate_select(state, msgmap, q_line, in_index, thrdindx)
    struct pine *state;
    MSGNO_S     *msgmap;
    int	         q_line;
    CmdWhere	 in_index;
    int          thrdindx;
{
    long          i, diff, old_tot, msgno, raw;
    int           q = 0, rv = 0, narrow = 0, hidden;
    HelpType      help = NO_HELP;
    ESCKEY_S     *sel_opts;
    MESSAGECACHE *mc;
    SEARCHSET    *limitsrch = NULL;
    PINETHRD_S   *thrd;
    extern     MAILSTREAM *mm_search_stream;
    extern     long	   mm_search_count;

    hidden           = any_lflagged(msgmap, MN_HIDE) > 0L;
    mm_search_stream = state->mail_stream;
    mm_search_count  = 0L;

    sel_opts = thrdindx ? sel_opts4 : sel_opts2;
    if(old_tot = any_lflagged(msgmap, MN_SLCT)){
	if(thrdindx){
	    i = 0;
	    thrd = fetch_thread(state->mail_stream,
				mn_m2raw(msgmap, mn_get_cur(msgmap)));
	    /* check if whole thread is selected or not */
	    if(thrd &&
	       count_lflags_in_thread(state->mail_stream,thrd,msgmap,MN_SLCT)
		   ==
	       count_lflags_in_thread(state->mail_stream,thrd,msgmap,MN_NONE))
	      i = 1;

	    sel_opts1[1].label = "unselect Curthrd" + (i ? 0 : 2);
	}
	else{
	    i = get_lflag(state->mail_stream, msgmap, mn_get_cur(msgmap),
			  MN_SLCT);
	    sel_opts1[1].label = "unselect Cur" + (i ? 0 : 2);
	}

	sel_opts += 2;			/* disable extra options */
	switch(q = radio_buttons(sel_pmt1, q_line, sel_opts1, 'c', 'x', NO_HELP,
				 RB_NORM, NULL)){
	  case 'f' :			/* flip selection */
	    msgno = 0L;
	    for(i = 1L; i <= mn_get_total(msgmap); i++){
		q = !get_lflag(state->mail_stream, msgmap, i, MN_SLCT);
		set_lflag(state->mail_stream, msgmap, i, MN_SLCT, q);
		if(hidden){
		    set_lflag(state->mail_stream, msgmap, i, MN_HIDE, !q);
		    if(!msgno && q)
		      mn_reset_cur(msgmap, msgno = i);
		}
	    }

	    return;

	  case 'n' :			/* narrow selection */
	    narrow++;
	  case 'b' :			/* broaden selection */
	    q = 0;			/* offer criteria prompt */
	    break;

	  case 'c' :			/* Un/Select Current */
	  case 'a' :			/* Unselect All */
	  case 'x' :			/* cancel */
	    break;

	  default :
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Unsupported Select option");
	    return;
	}
    }

    if(!q){
	while(1){
	    q = radio_buttons(sel_pmt2, q_line, sel_opts, 'c', 'x',
			      NO_HELP, RB_NORM|RB_RET_HELP, NULL);

	    if(q == 3){
		helper(h_index_cmd_select, "HELP FOR SELECT", HLPD_SIMPLE);
		ps_global->mangled_screen = 1;
	    }
	    else
	      break;
	}
    }

    /*
     * The purpose of this is to add the appropriate searchset to the
     * search so that the search can be limited to only looking at what
     * it needs to look at. That is, if we are narrowing then we only need
     * to look at messages which are already selected, and if we are
     * broadening, then we only need to look at messages which are not
     * yet selected. This routine will work whether or not
     * limiting_searchset properly limits the search set. In particular,
     * the searchset returned by limiting_searchset may include messages
     * which really shouldn't be included. We do that because a too-large
     * searchset will break some IMAP servers. It is even possible that it
     * becomes inefficient to send the whole set. If the select function
     * frees limitsrch, it should be sure to set it to NULL so we won't
     * try freeing it again here.
     */
    limitsrch = limiting_searchset(state->mail_stream, narrow);

    /*
     * NOTE: See note about MESSAGECACHE "searched" bits above!
     */
    switch(q){
      case 'x':				/* cancel */
	cmd_cancelled("Select command");
	return;

      case 'c' :			/* select/unselect current */
	(void) individual_select(state, msgmap, in_index);
	return;

      case 'a' :			/* select/unselect all */
	msgno = any_lflagged(msgmap, MN_SLCT);
	diff    = (!msgno) ? mn_get_total(msgmap) : 0L;

	for(i = 1L; i <= mn_get_total(msgmap); i++){
	    if(msgno){		/* unmark 'em all */
		if(get_lflag(state->mail_stream, msgmap, i, MN_SLCT)){
		    diff++;
		    set_lflag(state->mail_stream, msgmap, i, MN_SLCT, 0);
		}
		else if(hidden)
		  set_lflag(state->mail_stream, msgmap, i, MN_HIDE, 0);
	    }
	    else			/* mark 'em all */
	      set_lflag(state->mail_stream, msgmap, i, MN_SLCT, 1);
	}

	q_status_message4(SM_ORDER,0,2,
			  "%.200s%.200s message%.200s %.200sselected",
			  msgno ? "" : "All ", comatose(diff), 
			  plural(diff), msgno ? "UN" : "");
	return;

      case 'n' :			/* Select by Number */
	if(thrdindx)
	  rv = select_thrd_number(state->mail_stream, msgmap, &limitsrch);
	else
	  rv = select_number(state->mail_stream, msgmap, &limitsrch);

	break;

      case 'd' :			/* Select by Date */
	rv = select_date(state->mail_stream, msgmap, mn_get_cur(msgmap),
			 &limitsrch);
	break;

      case 't' :			/* Text */
	rv = select_text(state->mail_stream, msgmap, mn_get_cur(msgmap),
			 &limitsrch);
	break;

      case 'z' :			/* Size */
	rv = select_size(state->mail_stream, &limitsrch);
	break;

      case 's' :			/* Status */
	rv = select_flagged(state->mail_stream, &limitsrch);
	break;

      case 'k' :			/* Keyword */
	rv = select_by_keyword(state->mail_stream, &limitsrch);
	break;

      case 'r' :			/* Rule */
	rv = select_by_rule(state->mail_stream, &limitsrch);
	break;

      default :
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Unsupported Select option");
	return;
    }

    if(limitsrch)
      mail_free_searchset(&limitsrch);

    if(rv)				/* bad return value.. */
      return;				/* error already displayed */

    if(narrow)				/* make sure something was selected */
      for(i = 1L; i <= mn_get_total(msgmap); i++)
	if((raw = mn_m2raw(msgmap, i)) > 0L && state->mail_stream
	   && raw <= state->mail_stream->nmsgs
	   && (mc = mail_elt(state->mail_stream, raw)) && mc->searched){
	    if(get_lflag(state->mail_stream, msgmap, i, MN_SLCT))
	      break;
	    else
	      mm_search_count--;
	}

    diff = 0L;
    if(mm_search_count){
	/*
	 * loop thru all the messages, adjusting local flag bits
	 * based on their "searched" bit...
	 */
	for(i = 1L, msgno = 0L; i <= mn_get_total(msgmap); i++)
	  if(narrow){
	      /* turning OFF selectedness if the "searched" bit isn't lit. */
	      if(get_lflag(state->mail_stream, msgmap, i, MN_SLCT)){
		  if((raw = mn_m2raw(msgmap, i)) > 0L && state->mail_stream
		     && raw <= state->mail_stream->nmsgs
		     && (mc = mail_elt(state->mail_stream, raw))
		     && !mc->searched){
		      diff--;
		      set_lflag(state->mail_stream, msgmap, i, MN_SLCT, 0);
		      if(hidden)
			set_lflag(state->mail_stream, msgmap, i, MN_HIDE, 1);
		  }
		  /* adjust current message in case we unselect and hide it */
		  else if(msgno < mn_get_cur(msgmap)
			  && (!thrdindx
			      || !get_lflag(state->mail_stream, msgmap,
					    i, MN_CHID)))
		    msgno = i;
	      }
	  }
	  else if((raw = mn_m2raw(msgmap, i)) > 0L && state->mail_stream
	          && raw <= state->mail_stream->nmsgs
	          && (mc = mail_elt(state->mail_stream, raw)) && mc->searched){
	      /* turn ON selectedness if "searched" bit is lit. */
	      if(!get_lflag(state->mail_stream, msgmap, i, MN_SLCT)){
		  diff++;
		  set_lflag(state->mail_stream, msgmap, i, MN_SLCT, 1);
		  if(hidden)
		    set_lflag(state->mail_stream, msgmap, i, MN_HIDE, 0);
	      }
	  }

	/* if we're zoomed and the current message was unselected */
	if(narrow && msgno
	   && get_lflag(state->mail_stream,msgmap,mn_get_cur(msgmap),MN_HIDE))
	  mn_reset_cur(msgmap, msgno);
    }

    if(!diff){
	if(narrow)
	  q_status_message4(SM_ORDER, 0, 2,
			"%.200s.  %.200s message%.200s remain%.200s selected.",
			mm_search_count
					? "No change resulted"
					: "No messages in intersection",
			comatose(old_tot), plural(old_tot),
			(old_tot == 1L) ? "s" : "");
	else if(old_tot)
	  q_status_message(SM_ORDER, 0, 2,
		   "No change resulted.  Matching messages already selected.");
	else
	  q_status_message1(SM_ORDER | SM_DING, 0, 2,
			    "Select failed.  No %.200smessages selected.",
			    old_tot ? "additional " : "");
    }
    else if(old_tot){
	sprintf(tmp_20k_buf,
		"Select matched %ld message%s.  %s %smessage%s %sselected.",
		(diff > 0) ? diff : old_tot + diff,
		plural((diff > 0) ? diff : old_tot + diff),
		comatose((diff > 0) ? any_lflagged(msgmap, MN_SLCT) : -diff),
		(diff > 0) ? "total " : "",
		plural((diff > 0) ? any_lflagged(msgmap, MN_SLCT) : -diff),
		(diff > 0) ? "" : "UN");
	q_status_message(SM_ORDER, 0, 2, tmp_20k_buf);
    }
    else
      q_status_message2(SM_ORDER, 0, 2, "Select matched %.200s message%.200s!",
			comatose(diff), plural(diff));
}


/*----------------------------------------------------------------------
 Toggle the state of the current message

   Args: state -- pointer pine's state variables
	 msgmap -- message collection to operate on
	 in_index -- in the message index view
   Returns: TRUE if current marked selected, FALSE otw
  ----*/
int
individual_select(state, msgmap, in_index)
     struct pine *state;
     MSGNO_S     *msgmap;
     CmdWhere	  in_index;
{
    long cur;
    int  all_selected = 0;
    unsigned long was, is, tot;

    cur = mn_get_cur(msgmap);

    if(THRD_INDX()){
	PINETHRD_S *thrd;

	thrd = fetch_thread(state->mail_stream, mn_m2raw(msgmap, cur));
	if(!thrd)
	  return 0;

	was = count_lflags_in_thread(state->mail_stream, thrd, msgmap, MN_SLCT);
	tot = count_lflags_in_thread(state->mail_stream, thrd, msgmap, MN_NONE);
	if(was == tot)
	  all_selected++;

	if(all_selected){
	    set_thread_lflags(state->mail_stream, thrd, msgmap, MN_SLCT, 0);
	    if(any_lflagged(msgmap, MN_HIDE) > 0L){
		set_thread_lflags(state->mail_stream, thrd, msgmap, MN_HIDE, 1);
		/*
		 * See if there's anything left to zoom on.  If so, 
		 * pick an adjacent one for highlighting, else make
		 * sure nothing is left hidden...
		 */
		if(any_lflagged(msgmap, MN_SLCT)){
		    mn_inc_cur(state->mail_stream, msgmap,
			       (in_index == View && THREADING()
				&& sp_viewing_a_thread(state->mail_stream))
				 ? MH_THISTHD
				 : (in_index == View)
				   ? MH_ANYTHD : MH_NONE);
		    if(mn_get_cur(msgmap) == cur)
		      mn_dec_cur(state->mail_stream, msgmap,
			         (in_index == View && THREADING()
				  && sp_viewing_a_thread(state->mail_stream))
				   ? MH_THISTHD
				   : (in_index == View)
				     ? MH_ANYTHD : MH_NONE);
		}
		else			/* clear all hidden flags */
		  (void) unzoom_index(state, state->mail_stream, msgmap);
	    }
	}
	else
	  set_thread_lflags(state->mail_stream, thrd, msgmap, MN_SLCT, 1);

	q_status_message3(SM_ORDER, 0, 2, "%.200s message%.200s %.200sselected",
			  comatose(all_selected ? was : tot-was),
			  plural(all_selected ? was : tot-was),
			  all_selected ? "UN" : "");
    }
    else{
	if(all_selected =
	   get_lflag(state->mail_stream, msgmap, cur, MN_SLCT)){ /* set? */
	    set_lflag(state->mail_stream, msgmap, cur, MN_SLCT, 0);
	    if(any_lflagged(msgmap, MN_HIDE) > 0L){
		set_lflag(state->mail_stream, msgmap, cur, MN_HIDE, 1);
		/*
		 * See if there's anything left to zoom on.  If so, 
		 * pick an adjacent one for highlighting, else make
		 * sure nothing is left hidden...
		 */
		if(any_lflagged(msgmap, MN_SLCT)){
		    mn_inc_cur(state->mail_stream, msgmap,
			       (in_index == View && THREADING()
				&& sp_viewing_a_thread(state->mail_stream))
				 ? MH_THISTHD
				 : (in_index == View)
				   ? MH_ANYTHD : MH_NONE);
		    if(mn_get_cur(msgmap) == cur)
		      mn_dec_cur(state->mail_stream, msgmap,
			         (in_index == View && THREADING()
				  && sp_viewing_a_thread(state->mail_stream))
				   ? MH_THISTHD
				   : (in_index == View)
				     ? MH_ANYTHD : MH_NONE);
		}
		else			/* clear all hidden flags */
		  (void) unzoom_index(state, state->mail_stream, msgmap);
	    }
	}
	else
	  set_lflag(state->mail_stream, msgmap, cur, MN_SLCT, 1);

	q_status_message2(SM_ORDER, 0, 2, "Message %.200s %.200sselected",
			  long2string(cur), all_selected ? "UN" : "");
    }


    return(!all_selected);
}



/*----------------------------------------------------------------------
 Prompt the user for the command to perform on selected messages

   Args: state -- pointer pine's state variables
	 msgmap -- message collection to operate on
	 q_line -- line on display to write prompts
   Returns: 1 if the selected messages are suitably commanded,
	    0 if the choice to pick the command was declined

  ----*/
int
apply_command(state, stream, msgmap, preloadkeystroke, flags, q_line)
     struct pine *state;
     MAILSTREAM	 *stream;
     MSGNO_S     *msgmap;
     int	  preloadkeystroke;
     int	  flags;
     int	  q_line;
{
    int i = 8,			/* number of static entries in sel_opts3 */
        rv = 1,
	cmd,
        we_cancel = 0,
	agg = (flags & AC_FROM_THREAD) ? 2 : 1;
    char prompt[80];

    if(!preloadkeystroke){
	if(F_ON(F_ENABLE_FLAG,state)){ /* flag? */
	    sel_opts3[i].ch      = '*';
	    sel_opts3[i].rval    = '*';
	    sel_opts3[i].name    = "*";
	    sel_opts3[i++].label = "Flag";
	}

	if(F_ON(F_ENABLE_PIPE,state)){ /* pipe? */
	    sel_opts3[i].ch      = '|';
	    sel_opts3[i].rval    = '|';
	    sel_opts3[i].name    = "|";
	    sel_opts3[i++].label = "Pipe";
	}

	if(F_ON(F_ENABLE_BOUNCE,state)){ /* bounce? */
	    sel_opts3[i].ch      = 'b';
	    sel_opts3[i].rval    = 'b';
	    sel_opts3[i].name    = "B";
	    sel_opts3[i++].label = "Bounce";
	}

	if(flags & AC_FROM_THREAD){
	    if(flags & (AC_COLL | AC_EXPN)){
		sel_opts3[i].ch      = '/';
		sel_opts3[i].rval    = '/';
		sel_opts3[i].name    = "/";
		sel_opts3[i++].label = (flags & AC_COLL) ? "Collapse"
							 : "Expand";
	    }

	    sel_opts3[i].ch      = ';';
	    sel_opts3[i].rval    = ';';
	    sel_opts3[i].name    = ";";
	    if(flags & AC_UNSEL)
	      sel_opts3[i++].label = "UnSelect";
	    else
	      sel_opts3[i++].label = "Select";
	}

	if(F_ON(F_ENABLE_PRYNT, state)){	/* this one is invisible */
	    sel_opts3[i].ch      = 'y';
	    sel_opts3[i].rval    = '%';
	    sel_opts3[i].name    = "";
	    sel_opts3[i++].label = "";
	}

	sel_opts3[i].ch = -1;

	sprintf(prompt, "%.20s command : ",
		(flags & AC_FROM_THREAD) ? "THREAD" : "APPLY");
	cmd = double_radio_buttons(prompt, q_line, sel_opts3, 'z', 'x', NO_HELP,
				   RB_SEQ_SENSITIVE);
    }
    else
      cmd = preloadkeystroke;
    
    if(isupper(cmd))
      cmd = tolower(cmd);

    switch(cmd){
      case 'd' :			/* delete */
	we_cancel = busy_alarm(1, NULL, NULL, 0);
	cmd_delete(state, msgmap, agg, MsgIndx);
	if(we_cancel)
	  cancel_busy_alarm(0);
	break;

      case 'u' :			/* undelete */
	we_cancel = busy_alarm(1, NULL, NULL, 0);
	cmd_undelete(state, msgmap, agg);
	if(we_cancel)
	  cancel_busy_alarm(0);
	break;

      case 'r' :			/* reply */
	cmd_reply(state, msgmap, agg);
	break;

      case 'f' :			/* Forward */
	cmd_forward(state, msgmap, agg);
	break;

      case '%' :			/* print */
	cmd_print(state, msgmap, agg, MsgIndx);
	break;

      case 't' :			/* take address */
	cmd_take_addr(state, msgmap, agg);
	break;

      case 's' :			/* save */
	cmd_save(state, stream, msgmap, agg, MsgIndx);
	break;

      case 'e' :			/* export */
	cmd_export(state, msgmap, q_line, agg);
	break;

      case '|' :			/* pipe */
	cmd_pipe(state, msgmap, agg);
	break;

      case '*' :			/* flag */
	we_cancel = busy_alarm(1, NULL, NULL, 0);
	cmd_flag(state, msgmap, agg);
	if(we_cancel)
	  cancel_busy_alarm(0);
	break;

      case 'b' :			/* bounce */
	cmd_bounce(state, msgmap, agg);
	break;

      case '/' :
	collapse_or_expand(state, stream, msgmap,
			   F_ON(F_SLASH_COLL_ENTIRE, ps_global)
			     ? 0L
			     : mn_get_cur(msgmap));
	break;

      case ':' :
	select_thread_stmp(state, stream, msgmap);
	break;

      case 'x' :			/* cancel */
	cmd_cancelled((flags & AC_FROM_THREAD) ? "Thread command"
					       : "Apply command");
	rv = 0;
	break;

      case 'z' :			/* default */
        q_status_message(SM_INFO, 0, 2,
			 "Cancelled, there is no default command");
	rv = 0;
	break;

      default:
	break;
    }

    return(rv);
}


/*----------------------------------------------------------------------
  ZOOM the message index (set any and all necessary hidden flag bits)

   Args: state -- usual pine state
	 msgmap -- usual message mapping
   Returns: number of messages zoomed in on

  ----*/
long
zoom_index(state, stream, msgmap)
    struct pine *state;
    MAILSTREAM  *stream;
    MSGNO_S	*msgmap;
{
    long        i, count = 0L, first = 0L, msgno;
    PINETHRD_S *thrd = NULL, *topthrd = NULL, *nthrd;

    if(any_lflagged(msgmap, MN_SLCT)){

	if(THREADING() && sp_viewing_a_thread(stream)){
	    /* get top of current thread */
	    thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
	    if(thrd && thrd->top)
	      topthrd = fetch_thread(stream, thrd->top);
	}

	for(i = 1L; i <= mn_get_total(msgmap); i++){
	    if(!get_lflag(stream, msgmap, i, MN_SLCT)){
		set_lflag(stream, msgmap, i, MN_HIDE, 1);
	    }
	    else{

		/*
		 * Because subject lines depend on whether or not
		 * other parts of the thread above us are visible or not.
		 */
		if(THREADING() && !THRD_INDX()
		   && ps_global->thread_disp_style == THREAD_MUTTLIKE)
		  clear_index_cache_ent(i);

		/*
		 * If a selected message is hidden beneath a collapsed
		 * thread (not beneath a thread index line, but a collapsed
		 * thread or subthread) then we make it visible. The user
		 * should be able to see the selected messages when they
		 * Zoom. We could get a bit fancier and re-collapse the
		 * thread when the user unzooms, but we don't do that
		 * for now.
		 */
		if(THREADING() && !THRD_INDX()
		   && get_lflag(stream, msgmap, i, MN_CHID)){

		    /*
		     * What we need to do is to unhide this message and
		     * uncollapse any parent above us.
		     * Also, when we uncollapse a parent, we need to
		     * trace back down the tree and unhide until we get
		     * to a collapse point or the end. That's what
		     * set_thread_subtree does.
		     */

		    thrd = fetch_thread(stream, mn_m2raw(msgmap, i));

		    if(thrd && thrd->parent)
		      thrd = fetch_thread(stream, thrd->parent);
		    else
		      thrd = NULL;

		    /* unhide and uncollapse its parents */
		    while(thrd){
			/* if this parent is collapsed */
			if(get_lflag(stream, NULL, thrd->rawno, MN_COLL)){
			    /* uncollapse this parent and unhide its subtree */
			    msgno = mn_raw2m(msgmap, thrd->rawno);
			    if(msgno > 0L && msgno <= mn_get_total(msgmap)){
				set_lflag(stream, msgmap, msgno,
					  MN_COLL | MN_CHID, 0);
				if(thrd->next &&
				   (nthrd = fetch_thread(stream, thrd->next)))
				  set_thread_subtree(stream, nthrd, msgmap,
						     0, MN_CHID);
			    }

			    /* collapse symbol will be wrong */
			    clear_index_cache_ent(msgno);
			}

			/*
			 * Continue up tree to next parent looking for
			 * more collapse points.
			 */
			if(thrd->parent)
			  thrd = fetch_thread(stream, thrd->parent);
			else
			  thrd = NULL;
		    }
		}

		count++;
		if(!first){
		    if(THRD_INDX()){
			/* find msgno of top of thread for msg i */
			if((thrd=fetch_thread(stream, mn_m2raw(msgmap, i)))
			    && thrd->top)
			  first = mn_raw2m(msgmap, thrd->top);
		    }
		    else if(THREADING() && sp_viewing_a_thread(stream)){
			/* want first selected message in this thread */
			if(topthrd
			   && (thrd=fetch_thread(stream, mn_m2raw(msgmap, i)))
			   && thrd->top
			   && topthrd->rawno == thrd->top)
			  first = i;
		    }
		    else
		      first = i;
		}
	    }
	}

	if(THRD_INDX()){
	    thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
	    if(count_lflags_in_thread(stream, thrd, msgmap, MN_SLCT) == 0)
	      mn_set_cur(msgmap, first);
	}
	else if((THREADING() && sp_viewing_a_thread(stream))
	        || !get_lflag(stream, msgmap, mn_get_cur(msgmap), MN_SLCT)){
	    if(!first){
		int flags = 0;

		/*
		 * Nothing was selected in the thread we were in, so
		 * drop back to the Thread Index instead. Set the current
		 * thread to the first one that has a selection in it.
		 */

		unview_thread(state, stream, msgmap);

		i = next_sorted_flagged(F_UNDEL, stream, 1L, &flags);
		
		if(flags & NSF_FLAG_MATCH
		   && (thrd=fetch_thread(stream, mn_m2raw(msgmap, i)))
		    && thrd->top)
		  first = mn_raw2m(msgmap, thrd->top);
		else
		  first = 1L;	/* can't happen */

		mn_set_cur(msgmap, first);
	    }
	    else{
		if(msgline_hidden(stream, msgmap, mn_get_cur(msgmap), 0))
		  mn_set_cur(msgmap, first);
	    }
	}
    }

    return(count);
}



/*----------------------------------------------------------------------
  UnZOOM the message index (clear any and all hidden flag bits)

   Args: state -- usual pine state
	 msgmap -- usual message mapping
   Returns: 1 if hidden bits to clear and they were, 0 if none to clear

  ----*/
int
unzoom_index(state, stream, msgmap)
    struct pine *state;
    MAILSTREAM  *stream;
    MSGNO_S	*msgmap;
{
    register long i;

    if(!any_lflagged(msgmap, MN_HIDE))
      return(0);

    for(i = 1L; i <= mn_get_total(msgmap); i++)
      set_lflag(stream, msgmap, i, MN_HIDE, 0);

    return(1);
}


/*
 * Select by message number ranges.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_number(stream, msgmap, limitsrch)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    SEARCHSET **limitsrch;
{
    int r;
    long n1, n2, raw;
    char number1[16], number2[16], numbers[80], *p, *t;
    HelpType help;
    MESSAGECACHE *mc;

    numbers[0] = '\0';
    ps_global->mangled_footer = 1;
    help = NO_HELP;
    while(1){
	int flags = OE_APPEND_CURRENT;

        r = optionally_enter(numbers, -FOOTER_ROWS(ps_global), 0,
			     sizeof(numbers), select_num, NULL, help, &flags);
        if(r == 4)
	  continue;

        if(r == 3){
            help = (help == NO_HELP) ? h_select_by_num : NO_HELP;
	    continue;
	}

	for(t = p = numbers; *p ; p++)	/* strip whitespace */
	  if(!isspace((unsigned char)*p))
	    *t++ = *p;

	*t = '\0';

        if(r == 1 || numbers[0] == '\0'){
	    cmd_cancelled("Selection by number");
	    return(1);
        }
	else
	  break;
    }

    for(n1 = 1; n1 <= stream->nmsgs; n1++)
      if((mc = mail_elt(stream, n1)) != NULL)
        mc->searched = 0;			/* clear searched bits */

    for(p = numbers; *p ; p++){
	t = number1;
	while(*p && isdigit((unsigned char)*p))
	  *t++ = *p++;

	*t = '\0';

	if(number1[0] == '\0'){
	    if(*p == '-')
	      q_status_message1(SM_ORDER | SM_DING, 0, 2,
	   "Invalid message number range, missing number before \"-\": %.200s",
	       numbers);
	    else
	      q_status_message1(SM_ORDER | SM_DING, 0, 2,
			        "Invalid message number: %.200s", numbers);
	    return(1);
	}

	if((n1 = atol(number1)) < 1L || n1 > mn_get_total(msgmap)){
	    q_status_message1(SM_ORDER | SM_DING, 0, 2,
			      "\"%.200s\" out of message number range",
			      long2string(n1));
	    return(1);
	}

	t = number2;
	if(*p == '-'){
	    while(*++p && isdigit((unsigned char)*p))
	      *t++ = *p;

	    *t = '\0';

	    if(number2[0] == '\0'){
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
	     "Invalid message number range, missing number after \"-\": %.200s",
		 numbers);
		return(1);
	    }

	    if((n2 = atol(number2)) < 1L 
	       || n2 > mn_get_total(msgmap)){
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
				  "\"%.200s\" out of message number range",
				  long2string(n2));
		return(1);
	    }

	    if(n2 <= n1){
		char t[20];

		strcpy(t, long2string(n1));
		q_status_message2(SM_ORDER | SM_DING, 0, 2,
			  "Invalid reverse message number range: %.200s-%.200s",
				  t, long2string(n2));
		return(1);
	    }

	    for(;n1 <= n2; n1++){
		raw = mn_m2raw(msgmap, n1);
		if(raw > 0L
		   && (!(limitsrch && *limitsrch)
		       || in_searchset(*limitsrch, (unsigned long) raw)))
		  mm_searched(stream, raw);
	    }
	}
	else{
	    raw = mn_m2raw(msgmap, n1);
	    if(raw > 0L
	       && (!(limitsrch && *limitsrch)
		   || in_searchset(*limitsrch, (unsigned long) raw)))
	      mm_searched(stream, raw);
	}

	if(*p == '\0')
	  break;
    }

    return(0);
}


int
in_searchset(srch, num)
    SEARCHSET *srch;
    unsigned long num;
{
    SEARCHSET *s;
    unsigned long i;

    if(srch)
      for(s = srch; s; s = s->next)
	for(i = s->first; i <= s->last; i++){
	    if(i == num)
	      return 1;
	}

    return 0;
}
    

/*
 * Select by thread number ranges.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_thrd_number(stream, msgmap, msgset)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    SEARCHSET **msgset;
{
    int r;
    long n1, n2;
    char number1[16], number2[16], numbers[80], *p, *t;
    HelpType help;
    PINETHRD_S   *thrd = NULL;
    MESSAGECACHE *mc;

    numbers[0] = '\0';
    ps_global->mangled_footer = 1;
    help = NO_HELP;
    while(1){
	int flags = OE_APPEND_CURRENT;

        r = optionally_enter(numbers, -FOOTER_ROWS(ps_global), 0,
			     sizeof(numbers), select_num, NULL, help, &flags);
        if(r == 4)
	  continue;

        if(r == 3){
            help = (help == NO_HELP) ? h_select_by_thrdnum : NO_HELP;
	    continue;
	}

	for(t = p = numbers; *p ; p++)	/* strip whitespace */
	  if(!isspace((unsigned char)*p))
	    *t++ = *p;

	*t = '\0';

        if(r == 1 || numbers[0] == '\0'){
	    cmd_cancelled("Selection by number");
	    return(1);
        }
	else
	  break;
    }

    for(n1 = 1; n1 <= stream->nmsgs; n1++)
      if((mc = mail_elt(stream, n1)) != NULL)
        mc->searched = 0;			/* clear searched bits */

    for(p = numbers; *p ; p++){
	t = number1;
	while(*p && isdigit((unsigned char)*p))
	  *t++ = *p++;

	*t = '\0';

	if(number1[0] == '\0'){
	    if(*p == '-')
	      q_status_message1(SM_ORDER | SM_DING, 0, 2,
	       "Invalid number range, missing number before \"-\": %.200s",
	       numbers);
	    else
	      q_status_message1(SM_ORDER | SM_DING, 0, 2,
			        "Invalid thread number: %.200s", numbers);
	    return(1);
	}

	if((n1 = atol(number1)) < 1L || n1 > msgmap->max_thrdno){
	    q_status_message1(SM_ORDER | SM_DING, 0, 2,
			      "\"%.200s\" out of thread number range",
			      long2string(n1));
	    return(1);
	}

	t = number2;
	if(*p == '-'){
	    while(*++p && isdigit((unsigned char)*p))
	      *t++ = *p;

	    *t = '\0';

	    if(number2[0] == '\0'){
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
		 "Invalid number range, missing number after \"-\": %.200s",
		 numbers);
		return(1);
	    }

	    if((n2 = atol(number2)) < 1L 
	       || n2 > mn_get_total(msgmap)){
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
				  "\"%.200s\" out of thread number range",
				  long2string(n2));
		return(1);
	    }

	    if(n2 <= n1){
		char t[20];

		strcpy(t, long2string(n1));
		q_status_message2(SM_ORDER | SM_DING, 0, 2,
			  "Invalid reverse message number range: %.200s-%.200s",
				  t, long2string(n2));
		return(1);
	    }

	    for(;n1 <= n2; n1++){
		thrd = find_thread_by_number(stream, msgmap, n1, thrd);

		if(thrd)
		  set_search_bit_for_thread(stream, thrd, msgset);
	    }
	}
	else{
	    thrd = find_thread_by_number(stream, msgmap, n1, NULL);

	    if(thrd)
	      set_search_bit_for_thread(stream, thrd, msgset);
	}

	if(*p == '\0')
	  break;
    }
    
    return(0);
}


/*
 * Set search bit for every message in a thread.
 *
 * Watch out when calling this. The thrd->branch is not part of thrd.
 * Branch is a sibling to thrd, not a child. Zero out branch before calling
 * or call on thrd->next and worry about thrd separately. Top-level threads
 * already have a branch equal to zero.
 *
 *  If msgset is non-NULL, then only set the search bit for a message if that
 *  message is included in the msgset.
 */
void
set_search_bit_for_thread(stream, thrd, msgset)
    MAILSTREAM  *stream;
    PINETHRD_S  *thrd;
    SEARCHSET  **msgset;
{
    PINETHRD_S *nthrd, *bthrd;

    if(!(stream && thrd))
      return;

    if(thrd->rawno > 0L && thrd->rawno <= stream->nmsgs
       && (!(msgset && *msgset) || in_searchset(*msgset, thrd->rawno)))
      mm_searched(stream, thrd->rawno);

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  set_search_bit_for_thread(stream, nthrd, msgset);
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  set_search_bit_for_thread(stream, bthrd, msgset);
    }
}


/*
 * Select by message dates.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_date(stream, msgmap, msgno, limitsrch)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    long	 msgno;
    SEARCHSET **limitsrch;
{
    int	       r, we_cancel = 0, when = 0;
    char       date[100], defdate[100], prompt[128];
    time_t     seldate = time(0);
    struct tm *seldate_tm;
    SEARCHPGM *pgm;
    HelpType   help;
    static struct _tense {
	char *preamble,
	     *range,
	     *scope;
    } tense[] = {
	{"were ", "SENT SINCE",     " (inclusive)"},
	{"were ", "SENT BEFORE",    " (exclusive)"},
	{"were ", "SENT ON",        ""            },
	{"",      "ARRIVED SINCE",  " (inclusive)"},
	{"",      "ARRIVED BEFORE", " (exclusive)"},
	{"",      "ARRIVED ON",     ""            }
    };

    date[0]		      = '\0';
    ps_global->mangled_footer = 1;
    help		      = NO_HELP;

    /*
     * If talking to an old server, default to SINCE instead of
     * SENTSINCE, which was added later.
     */
    if(is_imap_stream(stream) && !modern_imap_stream(stream))
      when = 3;

    while(1){
	int flags = OE_APPEND_CURRENT;

	seldate_tm = localtime(&seldate);
	sprintf(defdate, "%.2d-%.4s-%.4d", seldate_tm->tm_mday,
		month_abbrev(seldate_tm->tm_mon + 1),
		seldate_tm->tm_year + 1900);
	sprintf(prompt,"Select messages which %s%s%s [%s]: ",
		tense[when].preamble, tense[when].range,
		tense[when].scope, defdate);
	r = optionally_enter(date,-FOOTER_ROWS(ps_global), 0, sizeof(date),
			     prompt, sel_date_opt, help, &flags);
	switch (r){
	  case 1 :
	    cmd_cancelled("Selection by date");
	    return(1);

	  case 3 :
	    help = (help == NO_HELP) ? h_select_date : NO_HELP;
	    continue;

	  case 4 :
	    continue;

	  case 11 :
	    {
		MESSAGECACHE *mc;
		long rawno;

		if(stream && (rawno = mn_m2raw(msgmap, msgno)) > 0L
		   && rawno <= stream->nmsgs
		   && (mc = mail_elt(stream, rawno))){

		    /* cache not filled in yet? */
		    if(mc->day == 0){
			char seq[20];

			if(stream->dtb && stream->dtb->flags & DR_NEWS){
			    strncpy(seq,
				    long2string(mail_uid(stream, rawno)),
				    sizeof(seq));
			    seq[sizeof(seq)-1] = '\0';
			    mail_fetch_overview(stream, seq, NULL);
			}
			else{
			    strncpy(seq, long2string(rawno),
				    sizeof(seq));
			    seq[sizeof(seq)-1] = '\0';
			    mail_fetch_fast(stream, seq, 0L);
			}
		    }

		    /* mail_date returns fixed field width date */
		    mail_date(date, mc);
		    date[11] = '\0';
		}
	    }

	    continue;

	  case 12 :			/* set default to PREVIOUS day */
	    seldate -= 86400;
	    continue;

	  case 13 :			/* set default to NEXT day */
	    seldate += 86400;
	    continue;

	  case 14 :
	    when = (when+1) % (sizeof(tense) / sizeof(struct _tense));
	    continue;

	  default:
	    break;
	}

	removing_leading_white_space(date);
	removing_trailing_white_space(date);
	if(!*date){
	    strncpy(date, defdate, sizeof(date));
	    date[sizeof(date)-1] = '\0';
	}

	break;
    }

    if((pgm = mail_newsearchpgm()) != NULL){
	MESSAGECACHE elt;
	short          converted_date;

	if(mail_parse_date(&elt, (unsigned char *) date)){
	    converted_date = mail_shortdate(elt.year, elt.month, elt.day);

	    switch(when){
	      case 0:
		pgm->sentsince = converted_date;
		break;
	      case 1:
		pgm->sentbefore = converted_date;
		break;
	      case 2:
		pgm->senton = converted_date;
		break;
	      case 3:
		pgm->since = converted_date;
		break;
	      case 4:
		pgm->before = converted_date;
		break;
	      case 5:
		pgm->on = converted_date;
		break;
	    }

	    pgm->msgno = (limitsrch ? *limitsrch : NULL);

	    we_cancel = busy_alarm(1, "Busy Selecting", NULL, 0);

	    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);

	    if(we_cancel)
	      cancel_busy_alarm(0);

	    /* we know this was freed in mail_search, let caller know */
	    if(limitsrch)
	      *limitsrch = NULL;
	}
	else{
	    mail_free_searchpgm(&pgm);
	    q_status_message1(SM_ORDER, 3, 3,
			     "Invalid date entered: %.200s", date);
	    return(1);
	}
    }

    return(0);
}


/*
 * Select by searching in message headers or body.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_text(stream, msgmap, msgno, limitsrch)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    long	 msgno;
    SEARCHSET **limitsrch;
{
    int          r, type, we_cancel = 0, not = 0, flags, old_imap;
    char         sstring[80], savedsstring[80], origcharset[16], tmp[128];
    char        *sval = NULL, *cset = NULL, *charset = NULL;
    char         buftmp[MAILTMPLEN];
    ESCKEY_S     ekey[4];
    ENVELOPE    *env = NULL;
    HelpType     help;
    static char *recip = "RECIPIENTS";
    static char *partic = "PARTICIPANTS";
    long         searchflags;
    SEARCHPGM   *srchpgm, *pgm, *secondpgm = NULL, *thirdpgm = NULL;

    ps_global->mangled_footer = 1;
    origcharset[0] = '\0';
    savedsstring[0] = '\0';
    ekey[0].ch = ekey[1].ch = ekey[2].ch = ekey[3].ch = -1;

    while(1){
	type = radio_buttons(not ? sel_not_text : sel_text,
			     -FOOTER_ROWS(ps_global), sel_text_opt,
			     's', 'x', NO_HELP, RB_NORM|RB_RET_HELP, NULL);
	
	if(type == '!')
	  not = !not;
	else if(type == 3){
	    helper(h_select_text, "HELP FOR SELECT BASED ON CONTENTS",
		   HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	}
	else
	  break;
    }

    /*
     * prepare some friendly defaults...
     */
    switch(type){
      case 't' :			/* address fields, offer To or From */
      case 'f' :
      case 'c' :
      case 'r' :
      case 'p' :
	sval          = (type == 't') ? "TO" :
			  (type == 'f') ? "FROM" :
			    (type == 'c') ? "CC" :
			      (type == 'r') ? recip : partic;
	ekey[0].ch    = ctrl('T');
	ekey[0].name  = "^T";
	ekey[0].rval  = 10;
	ekey[0].label = "Cur To";
	ekey[1].ch    = ctrl('R');
	ekey[1].name  = "^R";
	ekey[1].rval  = 11;
	ekey[1].label = "Cur From";
	ekey[2].ch    = ctrl('W');
	ekey[2].name  = "^W";
	ekey[2].rval  = 12;
	ekey[2].label = "Cur Cc";
	break;

      case 's' :
	sval          = "SUBJECT";
	ekey[0].ch    = ctrl('X');
	ekey[0].name  = "^X";
	ekey[0].rval  = 13;
	ekey[0].label = "Cur Subject";
	break;

      case 'a' :
	sval = "TEXT";
	break;

      case 'b' :
	sval = "BODYTEXT";
	break;

      case 'x':
	break;

      default:
	dprint(1, (debugfile,"\n - BOTCH: select_text unrecognized option\n"));
	return(1);
    }

    if(type != 'x'){
	if(ekey[0].ch > -1 && msgno > 0L
	   && !(env=pine_mail_fetchstructure(stream,mn_m2raw(msgmap,msgno),
					     NULL)))
	  ekey[0].ch = -1;

	sstring[0] = '\0';
	help = NO_HELP;
	r = type;
	while(r != 'x'){
	    sprintf(tmp, "String in message %s to %smatch : ", sval,
		    not ? "NOT " : "");
	    flags = OE_APPEND_CURRENT | OE_KEEP_TRAILING_SPACE;
	    r = optionally_enter(sstring, -FOOTER_ROWS(ps_global), 0,
				 sizeof(sstring), tmp, ekey, help, &flags);

	    switch(r){
	      case 3 :
		help = (help == NO_HELP)
			? (not
			    ? ((type == 'f') ? h_select_txt_not_from
			      : (type == 't') ? h_select_txt_not_to
			       : (type == 'c') ? h_select_txt_not_cc
				: (type == 's') ? h_select_txt_not_subj
				 : (type == 'a') ? h_select_txt_not_all
				  : (type == 'r') ? h_select_txt_not_recip
				   : (type == 'p') ? h_select_txt_not_partic
				    : (type == 'b') ? h_select_txt_not_body
				     :                 NO_HELP)
			    : ((type == 'f') ? h_select_txt_from
			      : (type == 't') ? h_select_txt_to
			       : (type == 'c') ? h_select_txt_cc
				: (type == 's') ? h_select_txt_subj
				 : (type == 'a') ? h_select_txt_all
				  : (type == 'r') ? h_select_txt_recip
				   : (type == 'p') ? h_select_txt_partic
				    : (type == 'b') ? h_select_txt_body
				     :                 NO_HELP))
			: NO_HELP;

	      case 4 :
		continue;

	      case 10 :			/* To: default */
		if(env && env->to && env->to->mailbox)
		  sprintf(sstring, "%.30s%s%.40s", env->to->mailbox,
			  env->to->host ? "@" : "",
			  env->to->host ? env->to->host : "");
		continue;

	      case 11 :			/* From: default */
		if(env && env->from && env->from->mailbox)
		  sprintf(sstring, "%.30s%s%.40s", env->from->mailbox,
			  env->from->host ? "@" : "",
			  env->from->host ? env->from->host : "");
		continue;

	      case 12 :			/* Cc: default */
		if(env && env->cc && env->cc->mailbox)
		  sprintf(sstring, "%.30s%s%.40s", env->cc->mailbox,
			  env->cc->host ? "@" : "",
			  env->cc->host ? env->cc->host : "");
		continue;

	      case 13 :			/* Subject: default */
		if(env && env->subject && env->subject[0]){
		    char *q = NULL;
		    if(cset)
		      fs_give((void **) &cset);

		    sprintf(buftmp, "%.75s", env->subject);
		    q = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					       SIZEOF_20KBUF, buftmp, &cset);
		    /*
		     * If decoding was done, and the charset of the decoded
		     * subject is different from ours (cset != NULL) then
		     * we save that charset information for the search.
		     */
		    if(q != env->subject && cset && cset[0]){
			charset = cset;
			sprintf(savedsstring, "%.70s", q);
		    }

		    sprintf(sstring, "%.70s", q);
		}

		continue;

	      default :
		break;
	    }

	    if(r == 1 || sstring[0] == '\0')
	      r = 'x';

	    break;
	}
    }

    if(type == 'x' || r == 'x'){
	cmd_cancelled("Selection by text");
	return(1);
    }

    old_imap = (is_imap_stream(stream) && !modern_imap_stream(stream));

    /* create a search program and fill it in */
    srchpgm = pgm = mail_newsearchpgm();
    if(not && !old_imap){
	srchpgm->not = mail_newsearchpgmlist();
	srchpgm->not->pgm = mail_newsearchpgm();
	pgm = srchpgm->not->pgm;
    }

    switch(type){
      case 'r' :				/* TO or CC */
	if(old_imap){
	    /* No OR on old servers */
	    pgm->to = mail_newstringlist();
	    pgm->to->text.data = (unsigned char *) cpystr(sstring);
	    pgm->to->text.size = strlen(sstring);
	    secondpgm = mail_newsearchpgm();
	    secondpgm->cc = mail_newstringlist();
	    secondpgm->cc->text.data = (unsigned char *) cpystr(sstring);
	    secondpgm->cc->text.size = strlen(sstring);
	}
	else{
	    pgm->or = mail_newsearchor();
	    pgm->or->first->to = mail_newstringlist();
	    pgm->or->first->to->text.data = (unsigned char *) cpystr(sstring);
	    pgm->or->first->to->text.size = strlen(sstring);
	    pgm->or->second->cc = mail_newstringlist();
	    pgm->or->second->cc->text.data = (unsigned char *) cpystr(sstring);
	    pgm->or->second->cc->text.size = strlen(sstring);
	}

	break;

      case 'p' :				/* TO or CC or FROM */
	if(old_imap){
	    /* No OR on old servers */
	    pgm->to = mail_newstringlist();
	    pgm->to->text.data = (unsigned char *) cpystr(sstring);
	    pgm->to->text.size = strlen(sstring);
	    secondpgm = mail_newsearchpgm();
	    secondpgm->cc = mail_newstringlist();
	    secondpgm->cc->text.data = (unsigned char *) cpystr(sstring);
	    secondpgm->cc->text.size = strlen(sstring);
	    thirdpgm = mail_newsearchpgm();
	    thirdpgm->from = mail_newstringlist();
	    thirdpgm->from->text.data = (unsigned char *) cpystr(sstring);
	    thirdpgm->from->text.size = strlen(sstring);
	}
	else{
	    pgm->or = mail_newsearchor();
	    pgm->or->first->to = mail_newstringlist();
	    pgm->or->first->to->text.data = (unsigned char *) cpystr(sstring);
	    pgm->or->first->to->text.size = strlen(sstring);

	    pgm->or->second->or = mail_newsearchor();
	    pgm->or->second->or->first->cc = mail_newstringlist();
	    pgm->or->second->or->first->cc->text.data =
					    (unsigned char *) cpystr(sstring);
	    pgm->or->second->or->first->cc->text.size = strlen(sstring);
	    pgm->or->second->or->second->from = mail_newstringlist();
	    pgm->or->second->or->second->from->text.data =
					    (unsigned char *) cpystr(sstring);
	    pgm->or->second->or->second->from->text.size = strlen(sstring);
	}

	break;

      case 'f' :				/* FROM */
	pgm->from = mail_newstringlist();
	pgm->from->text.data = (unsigned char *) cpystr(sstring);
	pgm->from->text.size = strlen(sstring);
	break;

      case 'c' :				/* CC */
	pgm->cc = mail_newstringlist();
	pgm->cc->text.data = (unsigned char *) cpystr(sstring);
	pgm->cc->text.size = strlen(sstring);
	break;

      case 't' :				/* TO */
	pgm->to = mail_newstringlist();
	pgm->to->text.data = (unsigned char *) cpystr(sstring);
	pgm->to->text.size = strlen(sstring);
	break;

      case 's' :				/* SUBJECT */
	pgm->subject = mail_newstringlist();
	pgm->subject->text.data = (unsigned char *) cpystr(sstring);
	pgm->subject->text.size = strlen(sstring);
	break;

      case 'a' :				/* ALL TEXT */
	pgm->text = mail_newstringlist();
	pgm->text->text.data = (unsigned char *) cpystr(sstring);
	pgm->text->text.size = strlen(sstring);
	break;

      case 'b' :				/* ALL BODY TEXT */
	pgm->body = mail_newstringlist();
	pgm->body->text.data = (unsigned char *) cpystr(sstring);
	pgm->body->text.size = strlen(sstring);
	break;

      default :
	dprint(1, (debugfile,"\n - BOTCH: select_text unrecognized type\n"));
	return(1);
    }

    /*
     * If the user gets the current subject with the ^X command, and
     * that subject has a different charset than what the user uses, and
     * what is left after editing by the user is still a substring of
     * the original subject, and it still has non-ascii characters in it;
     * then use that charset from the original subject in the search.
     */
    if(charset && strstr(savedsstring, sstring) == NULL){
	strncpy(origcharset, charset, sizeof(origcharset));
	origcharset[sizeof(origcharset)-1] = '\0';
	charset = NULL;
    }

    /* set the charset */
    if(!charset){
	for(sval = sstring; *sval && isascii(*sval); sval++)
	  ;

	/* if it's ascii, don't warn user about charset change */
	if(!*sval)
	  origcharset[0] = '\0';

	/* if it isn't ascii, use user's charset */
	charset = (*sval &&
		   ps_global->VAR_CHAR_SET &&
		   ps_global->VAR_CHAR_SET[0])
		     ? ps_global->VAR_CHAR_SET
		     : "US-ASCII";
    }

    if(*origcharset)
      q_status_message2(SM_ORDER, 5, 5,
	    "Warning: character set used for search changed (%.200s -> %.200s)",
		    origcharset, charset);

    /*
     * If we happen to have any messages excluded, make sure we
     * don't waste time searching their text...
     */
    srchpgm->msgno = (limitsrch ? *limitsrch : NULL);

    we_cancel = busy_alarm(1, "Busy Selecting", NULL, 0);

    searchflags = SE_NOPREFETCH | (secondpgm ? 0 : SE_FREE);

    pine_mail_search_full(stream, !old_imap ? charset : NULL, srchpgm,
			  searchflags);
    
    /* search for To or Cc; or To or Cc or From on old imap server */
    if(secondpgm){
	if(srchpgm){
	    srchpgm->msgno = NULL;
	    mail_free_searchpgm(&srchpgm);
	}

	secondpgm->msgno = (limitsrch ? *limitsrch : NULL);
	searchflags |= (SE_RETAIN | (thirdpgm ? 0 : SE_FREE));

	pine_mail_search_full(stream, NULL, secondpgm, searchflags);

	if(thirdpgm){
	    if(secondpgm){
		secondpgm->msgno = NULL;
		mail_free_searchpgm(&secondpgm);
	    }

	    thirdpgm->msgno = (limitsrch ? *limitsrch : NULL);
	    searchflags |= SE_FREE;
	    pine_mail_search_full(stream, NULL, thirdpgm, searchflags);
	}
    }

    /* we know this was freed in mail_search, let caller know */
    if(limitsrch)
      *limitsrch = NULL;

    if(old_imap && not){
	MESSAGECACHE *mc;

	/* 
	 * Old imap server doesn't have a NOT, so we actually searched for
	 * the subject (or whatever) instead of !subject. Flip the searched
	 * bits.
	 */
	for(msgno = 1L; msgno <= mn_get_total(msgmap); msgno++)
	    if(stream && msgno <= stream->nmsgs
	       && (mc=mail_elt(stream, msgno)) && mc->searched)
	      mc->searched = NIL;
	    else
	      mc->searched = T;
    }

    if(we_cancel)
      cancel_busy_alarm(0);

    if(cset)
      fs_give((void **)&cset);

    return(0);
}


/*
 * Select by message size.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_size(stream, limitsrch)
    MAILSTREAM *stream;
    SEARCHSET **limitsrch;
{
    int        r, large = 1;
    unsigned long n, mult = 1L, numerator = 0L, divisor = 1L;
    char       size[16], numbers[80], *p, *t;
    HelpType   help;
    SEARCHPGM *pgm;
    long       flags = (SE_NOPREFETCH | SE_FREE);

    numbers[0] = '\0';
    ps_global->mangled_footer = 1;

    help = NO_HELP;
    while(1){
	int flgs = OE_APPEND_CURRENT;

	sel_size_opt[1].label = large ? sel_size_smaller : sel_size_larger;

        r = optionally_enter(numbers, -FOOTER_ROWS(ps_global), 0,
			     sizeof(numbers), large ? select_size_larger_msg
						    : select_size_smaller_msg,
			     sel_size_opt, help, &flgs);
        if(r == 4)
	  continue;

        if(r == 14){
	    large = 1 - large;
	    continue;
	}

        if(r == 3){
            help = (help == NO_HELP) ? (large ? h_select_by_larger_size
					      : h_select_by_smaller_size)
				     : NO_HELP;
	    continue;
	}

	for(t = p = numbers; *p ; p++)	/* strip whitespace */
	  if(!isspace((unsigned char)*p))
	    *t++ = *p;

	*t = '\0';

        if(r == 1 || numbers[0] == '\0'){
	    cmd_cancelled("Selection by size");
	    return(1);
        }
	else
	  break;
    }

    if(numbers[0] == '-'){
	q_status_message1(SM_ORDER | SM_DING, 0, 2,
			  "Invalid size entered: %.200s", numbers);
	return(1);
    }

    t = size;
    p = numbers;

    while(*p && isdigit((unsigned char)*p))
      *t++ = *p++;

    *t = '\0';

    if(size[0] == '\0' && *p == '.' && isdigit(*(p+1))){
	size[0] = '0';
	size[1] = '\0';
    }

    if(size[0] == '\0'){
	q_status_message1(SM_ORDER | SM_DING, 0, 2,
			  "Invalid size entered: %.200s", numbers);
	return(1);
    }

    n = strtoul(size, (char **)NULL, 10); 

    size[0] = '\0';
    if(*p == '.'){
	/*
	 * We probably ought to just use atof() to convert 1.1 into a
	 * double, but since we haven't used atof() anywhere else I'm
	 * reluctant to use it because of portability concerns.
	 */
	p++;
	t = size;
	while(*p && isdigit((unsigned char)*p)){
	    *t++ = *p++;
	    divisor *= 10;
	}

	*t = '\0';

	if(size[0])
	  numerator = strtoul(size, (char **)NULL, 10); 
    }

    switch(*p){
      case 'g':
      case 'G':
        mult *= 1000;
	/* fall through */

      case 'm':
      case 'M':
        mult *= 1000;
	/* fall through */

      case 'k':
      case 'K':
        mult *= 1000;
	break;
    }

    n = n * mult + (numerator * mult) / divisor;

    pgm = mail_newsearchpgm();
    if(large)
	pgm->larger = n;
    else
	pgm->smaller = n;

    if(is_imap_stream(stream) && !modern_imap_stream(stream))
      flags |= SO_NOSERVER;

    pgm->msgno = (limitsrch ? *limitsrch : NULL);
    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);
    /* we know this was freed in mail_search, let caller know */
    if(limitsrch)
      *limitsrch = NULL;

    return(0);
}


/*
 * visible_searchset -- return c-client search set unEXLDed
 *			sequence numbers
 */
SEARCHSET *
visible_searchset(stream, msgmap)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
{
    long       n, run;
    SEARCHSET *full_set = NULL, **set;

    /*
     * If we're talking to anything other than a server older than
     * imap 4rev1, build a searchset otherwise it'll choke.
     */
    if(!(is_imap_stream(stream) && !modern_imap_stream(stream))){
	if(any_lflagged(msgmap, MN_EXLD)){
	    for(n = 1L, set = &full_set, run = 0L; n <= stream->nmsgs; n++)
	      if(get_lflag(stream, NULL, n, MN_EXLD)){
		  if(run){		/* previous NOT excluded? */
		      if(run > 1L)
			(*set)->last = n - 1L;

		      set = &(*set)->next;
		      run = 0L;
		  }
	      }
	      else if(run++){		/* next in run */
		  (*set)->last = n;
	      }
	      else{				/* start of run */
		  *set = mail_newsearchset();
		  (*set)->first = n;
	      }
	}
	else{
	    full_set = mail_newsearchset();
	    full_set->first = 1L;
	    full_set->last  = stream->nmsgs;
	}
    }

    return(full_set);
}


/*
 * Return a search set which can be used to limit the search to a smaller set,
 * for performance reasons. The caller must still work correctly if we return
 * the whole set (or NULL) here, because we may not be able to send the full
 * search set over IMAP. In cases where the search set description is getting
 * too large we send a search set which contains all of the relevant messages.
 * It may contain more.
 *
 * Args    stream
 *         narrow  -- If set, we are narrowing our selection (restrict to
 *                      messages with MN_SLCT already set) or if not set,
 *                      we are broadening (so we may look only at messages
 *                      with MN_SLCT not set)
 *
 * Returns - allocated search set or NULL. Caller is responsible for freeing it.
 */
SEARCHSET *
limiting_searchset(stream, narrow)
    MAILSTREAM *stream;
    int         narrow;
{
    long       n, run;
    int        cnt = 0;
    SEARCHSET *full_set = NULL, **set, *s;

    /*
     * If we're talking to anything other than a server older than
     * imap 4rev1, build a searchset otherwise it'll choke.
     */
    if(!(is_imap_stream(stream) && !modern_imap_stream(stream))){
	for(n = 1L, set = &full_set, run = 0L; n <= stream->nmsgs; n++)
	  /* test for end of run */
	  if(get_lflag(stream, NULL, n, MN_EXLD)
	     || (narrow && !get_lflag(stream, NULL, n, MN_SLCT))
	     || (!narrow && get_lflag(stream, NULL, n, MN_SLCT))){
	      if(run){		/* previous selected? */
		  set = &(*set)->next;
		  run = 0L;
	      }
	  }
	  else if(run++){		/* next in run */
	      (*set)->last = n;
	  }
	  else{				/* start of run */
	      *set = mail_newsearchset();
	      (*set)->first = (*set)->last = n;

	      /*
	       * Make this last set cover the rest of the messages.
	       * We could be fancier about this but it probably isn't
	       * worth the trouble.
	       */
	      if(++cnt > 100){
		  (*set)->last = stream->nmsgs;
		  break;
	      }
	  }
    }

    return(full_set);
}


/*
 * Select by message status bits.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_flagged(stream, limitsrch)
    MAILSTREAM *stream;
    SEARCHSET **limitsrch;
{
    int	       s, not = 0, we_cancel = 0;
    SEARCHPGM *pgm;

    while(1){
	s = radio_buttons((not) ? sel_flag_not : sel_flag,
			  -FOOTER_ROWS(ps_global), sel_flag_opt, '*', 'x',
			  NO_HELP, RB_NORM|RB_RET_HELP, NULL);
			  
	if(s == 'x'){
	    cmd_cancelled("Selection by status");
	    return(1);
	}
	else if(s == 3){
	    helper(h_select_status, "HELP FOR SELECT BASED ON STATUS",
		   HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	}
	else if(s == '!')
	  not = !not;
	else
	  break;
    }

    pgm = mail_newsearchpgm();
    switch(s){
      case 'n' :
	if(not){
	    SEARCHPGM *notpgm;

	    /* this is the same as seen or deleted or answered */
	    pgm->not = mail_newsearchpgmlist();
	    notpgm = pgm->not->pgm = mail_newsearchpgm();
	    notpgm->unseen = notpgm->undeleted = notpgm->unanswered = 1;
	}
	else
	  pgm->unseen = pgm->undeleted = pgm->unanswered = 1;
	  
	break;

      case 'd' :
	if(not)
	  pgm->undeleted = 1;
	else
	  pgm->deleted = 1;

	break;

      case 'r' :
	if(not)
	  pgm->old = 1;
	else
	  pgm->recent = 1;

	break;

      case 'u' :
	if(not)
	  pgm->seen = 1;
	else
	  pgm->unseen = 1;

	break;

      case 'a':
	/*
	 * Not a true "not", we are implicitly only interested in undeleted.
	 */
	if(not)
	  pgm->unanswered = pgm->undeleted = 1;
	else
	  pgm->answered = pgm->undeleted = 1;
	break;

      default :
	if(not)
	  pgm->unflagged = 1;
	else
	  pgm->flagged = 1;
	  
	break;
    }

    we_cancel = busy_alarm(1, "Busy Selecting", NULL, 0);
    pgm->msgno = (limitsrch ? *limitsrch : NULL);
    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);
    /* we know this was freed in mail_search, let caller know */
    if(limitsrch)
      *limitsrch = NULL;

    if(we_cancel)
      cancel_busy_alarm(0);

    return(0);
}


/*
 * Select by message keywords.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_keyword(stream, limitsrch)
    MAILSTREAM *stream;
    SEARCHSET **limitsrch;
{
    int        r, not = 0;
    char       keyword[MAXUSERFLAG+1], *kword;
    char      *error = NULL, *p;
    KEYWORD_S *kw;
    HelpType   help;
    SEARCHPGM *pgm;

    keyword[0] = '\0';
    ps_global->mangled_footer = 1;

    help = NO_HELP;
    do{
	int oe_flags;

	if(error){
	    q_status_message(SM_ORDER, 3, 4, error);
	    fs_give((void **) &error);
	}

	oe_flags = OE_APPEND_CURRENT;
        r = optionally_enter(keyword, -FOOTER_ROWS(ps_global), 0,
			     sizeof(keyword),
			     not ? "Keyword to NOT match: "
			         : "Keyword to match: ",
			     sel_key_opt, help, &oe_flags);

	if(r == 14){
	    /* select keyword from a list */
	    if((kword=choose_a_keyword()) != NULL){
		strncpy(keyword, kword, sizeof(keyword)-1);
		keyword[sizeof(keyword)-1] = '\0';
		fs_give((void **) &kword);
	    }
	    else
	      r = 4;
	}
	else if(r == '!')
	  not = !not;

	if(r == 3)
	  help = help == NO_HELP ? h_select_keyword : NO_HELP;
	else if(r == 1){
	    cmd_cancelled("Selection by keyword");
	    return(1);
	}

	removing_leading_and_trailing_white_space(keyword);

    }while(r == 3 || r == 4 || r == '!' || keyword_check(keyword, &error));


    /*
     * We want to check the keyword, not the nickname of the keyword,
     * so convert it to the keyword if necessary.
     */
    p = nick_to_keyword(keyword);
    if(p != keyword){
	strncpy(keyword, p, sizeof(keyword)-1);
	keyword[sizeof(keyword)-1] = '\0';
    }

    pgm = mail_newsearchpgm();
    if(not){
	pgm->unkeyword = mail_newstringlist();
	pgm->unkeyword->text.data = (unsigned char *) cpystr(keyword);
	pgm->unkeyword->text.size = strlen(keyword);
    }
    else{
	pgm->keyword = mail_newstringlist();
	pgm->keyword->text.data = (unsigned char *) cpystr(keyword);
	pgm->keyword->text.size = strlen(keyword);
    }

    pgm->msgno = (limitsrch ? *limitsrch : NULL);
    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);
    /* we know this was freed in mail_search, let caller know */
    if(limitsrch)
      *limitsrch = NULL;

    return(0);
}


/*
 * Select by rule. Usually indexcolor and roles would be most useful.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_rule(stream, limitsrch)
    MAILSTREAM *stream;
    SEARCHSET **limitsrch;
{
    char       rulenick[1000], *nick;
    PATGRP_S  *patgrp;
    int        r, not = 0, rflags = ROLE_DO_INCOLS
				    | ROLE_DO_ROLES
				    | ROLE_DO_SCORES
				    | ROLE_DO_OTHER
				    | ROLE_DO_FILTER;

    rulenick[0] = '\0';
    ps_global->mangled_footer = 1;

    do{
	int oe_flags;

	oe_flags = OE_APPEND_CURRENT;
        r = optionally_enter(rulenick, -FOOTER_ROWS(ps_global), 0,
			     sizeof(rulenick),
			     not ? "Rule to NOT match: "
			         : "Rule to match: ",
			     sel_key_opt, NO_HELP, &oe_flags);

	if(r == 14){
	    /* select rulenick from a list */
	    if((nick=choose_a_rule(rflags)) != NULL){
		strncpy(rulenick, nick, sizeof(rulenick)-1);
		rulenick[sizeof(rulenick)-1] = '\0';
		fs_give((void **) &nick);
	    }
	    else
	      r = 4;
	}
	else if(r == '!')
	  not = !not;

	if(r == 3){
	    helper(h_select_rule, "HELP FOR SELECT BY RULE", HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	}
	else if(r == 1){
	    cmd_cancelled("Selection by Rule");
	    return(1);
	}

	removing_leading_and_trailing_white_space(rulenick);

    }while(r == 3 || r == 4 || r == '!');


    /*
     * The approach of requiring a nickname instead of just allowing the
     * user to select from the list of rules has the drawback that a rule
     * may not have a nickname, or there may be more than one rule with
     * the same nickname. However, it has the benefit of allowing the user
     * to type in the nickname and, most importantly, allows us to set
     * up the ! (not). We could incorporate the ! into the selection
     * screen, but this is easier and also allows the typing of nicks.
     * User can just set up nicknames if they want to use this feature.
     */
    patgrp = nick_to_patgrp(rulenick, rflags);

    if(patgrp){
	match_pattern(patgrp, stream, limitsrch ? *limitsrch : 0, NULL,
		      get_msg_score,
		      (not ? MP_NOT : 0) | SE_NOPREFETCH);
	free_patgrp(&patgrp);
    }

    if(limitsrch && *limitsrch){
	mail_free_searchset(limitsrch);
	*limitsrch = NULL;
    }

    return(0);
}


/*
 * These chars are not allowed in keywords.
 *
 * Returns 0 if ok, 1 if not.
 * Returns an allocated error message on error.
 */
int
keyword_check(kw, error)
    char  *kw;
    char **error;
{
    register char *t;
    char buf[100];

    if(!kw || !kw[0])
      return 1;

    kw = nick_to_keyword(kw);

    if((t = strindex(kw, SPACE)) ||
       (t = strindex(kw, '{'))   ||
       (t = strindex(kw, '('))   ||
       (t = strindex(kw, ')'))   ||
       (t = strindex(kw, ']'))   ||
       (t = strindex(kw, '%'))   ||
       (t = strindex(kw, '"'))   ||
       (t = strindex(kw, '\\'))  ||
       (t = strindex(kw, '*'))){
	char s[4];
	s[0] = '"';
	s[1] = *t;
	s[2] = '"';
	s[3] = '\0';
	if(error){
	    sprintf(buf, "%.20s not allowed in keywords",
		*t == SPACE ?
		    "Spaces" :
		    *t == '"' ?
			"Quotes" :
			*t == '%' ?
			    "Percents" :
			    s);
	    *error = cpystr(buf);
	}

	return 1;
    }

    return 0;
}


/*
 * Allow user to choose a keyword from their list of keywords.
 *
 * Returns an allocated keyword on success, NULL otherwise.
 */
char *
choose_a_keyword()
{
    char      *choice = NULL, *q;
    char     **keyword_list, **lp, **t;
    int        cnt;
    KEYWORD_S *kw;

    /*
     * Build a list of keywords to choose from.
     */

    for(cnt = 0, kw = ps_global->keywords; kw; kw = kw->next)
      cnt++;

    if(cnt <= 0){
	q_status_message(SM_ORDER, 3, 4,
	    "No keywords defined, use \"keywords\" option in Setup/Config");
	return(choice);
    }

    lp = keyword_list = (char **) fs_get((cnt + 1) * sizeof(*keyword_list));
    memset(keyword_list, 0, (cnt+1) * sizeof(*keyword_list));

    for(kw = ps_global->keywords; kw; kw = kw->next)
      *lp++ = cpystr(kw->nick ? kw->nick : kw->kw ? kw->kw : "");

    choice = choose_item_from_list(keyword_list, "SELECT A KEYWORD",
				   "keywords ", h_select_keyword_screen,
				   "HELP FOR SELECTING A KEYWORD");

    if(!choice)
      q_status_message(SM_ORDER, 1, 4, "No choice");

    free_list_array(&keyword_list);

    return(choice);
}


/*
 * Allow user to choose a list of keywords from their list of keywords.
 *
 * Returns allocated list.
 */
char **
choose_list_of_keywords()
{
    LIST_SEL_S *listhead, *ls, *p;
    char      **ret = NULL, **t;
    int         cnt, i;
    size_t      len;
    KEYWORD_S  *kw;

    /*
     * Build a list of keywords to choose from.
     */

    p = listhead = NULL;
    for(kw = ps_global->keywords; kw; kw = kw->next){

	ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
	memset(ls, 0, sizeof(*ls));
	ls->item = cpystr(kw->nick ? kw->nick : kw->kw ? kw->kw : "");

	if(p){
	    p->next = ls;
	    p = p->next;
	}
	else
	  listhead = p = ls;
    }
    
    if(!listhead)
      return(ret);
    
    if(!select_from_list_screen(listhead, SFL_ALLOW_LISTMODE,
				"SELECT KEYWORDS", "keywords ",
				h_select_multkeyword_screen,
			        "HELP FOR SELECTING KEYWORDS")){
	for(cnt = 0, p = listhead; p; p = p->next)
	  if(p->selected)
	    cnt++;

	ret = (char **) fs_get((cnt+1) * sizeof(*ret));
	memset(ret, 0, (cnt+1) * sizeof(*ret));
	for(i = 0, p = listhead; p; p = p->next)
	  if(p->selected)
	    ret[i++] = cpystr(p->item ? p->item : "");
    }

    free_list_sel(&listhead);

    return(ret);
}


/*
 * Allow user to choose a list of character sets and/or scripts
 *
 * Returns allocated list.
 */
char **
choose_list_of_charsets()
{
    LIST_SEL_S *listhead, *ls, *p;
    char      **ret = NULL;
    int         cnt, i, got_one;
    CHARSET    *cs;
    SCRIPT     *s;
    char       *q, *t;
    long 	width, limit;
    char        buf[1024], *folded;

    /*
     * Build a list of charsets to choose from.
     */

    p = listhead = NULL;

    /* this width is determined by select_from_list_screen() */
    width = ps_global->ttyo->screen_cols - 4;

    /* first comes a list of scripts (sets of character sets) */
    for(s = utf8_script(NIL); s && s->name; s++){

	limit = sizeof(buf)-1;
	q = buf;
	memset(q, 0, limit+1);

	if(s->name)
	  sstrncpy(&q, s->name, limit);

	if(s->description){
	    sstrncpy(&q, " (", limit-(q-buf));
	    sstrncpy(&q, s->description, limit-(q-buf));
	    sstrncpy(&q, ")", limit-(q-buf));
	}

	/* add the list of charsets that are in this script */
	got_one = 0;
	for(cs = utf8_charset(NIL);
	    cs && cs->name && (q-buf) < limit; cs++){
	    if(cs->script & s->script){
		/*
		 * Filter out some un-useful members of the list.
		 * UTF-7 and UTF-8 weren't actually in the list at the
		 * time this was written. Just making sure.
		 */
		if(!strucmp(cs->name, "ISO-2022-JP-2")
		   || !strucmp(cs->name, "UTF-7")
		   || !strucmp(cs->name, "UTF-8"))
		  continue;

		if(got_one)
		  sstrncpy(&q, " ", limit-(q-buf));
		else{
		    got_one = 1;
		    sstrncpy(&q, " {", limit-(q-buf));
		}

		sstrncpy(&q, cs->name, limit-(q-buf));
	    }
	}

	if(got_one)
	  sstrncpy(&q, "}", limit-(q-buf));

	/* fold this line so that it can all be seen on the screen */
	folded = fold(buf, width, width, 0, 0, "", "    ");
	if(folded){
	    t = folded;
	    while(t && *t && (q = strindex(t, '\n')) != NULL){
		*q = '\0';

		ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
		memset(ls, 0, sizeof(*ls));
		if(t == folded)
		  ls->item = cpystr(s->name);
		else
		  ls->flags = SFL_NOSELECT;

		ls->display_item = cpystr(t);

		t = q+1;

		if(p){
		    p->next = ls;
		    p = p->next;
		}
		else{
		    /* add a heading */
		    listhead = (LIST_SEL_S *) fs_get(sizeof(*ls));
		    memset(listhead, 0, sizeof(*listhead));
		    listhead->flags = SFL_NOSELECT;
		    listhead->display_item =
	   cpystr("Scripts representing groups of related character sets");
		    listhead->next = (LIST_SEL_S *) fs_get(sizeof(*ls));
		    memset(listhead->next, 0, sizeof(*listhead));
		    listhead->next->flags = SFL_NOSELECT;
		    listhead->next->display_item =
	   cpystr(repeat_char(width, '-'));

		    listhead->next->next = ls;
		    p = ls;
		}
	    }

	    fs_give((void **) &folded);
	}
    }

    ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
    memset(ls, 0, sizeof(*ls));
    ls->flags = SFL_NOSELECT;
    if(p){
	p->next = ls;
	p = p->next;
    }
    else
      listhead = p = ls;

    ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
    memset(ls, 0, sizeof(*ls));
    ls->flags = SFL_NOSELECT;
    ls->display_item =
               cpystr("Individual character sets, may be mixed with scripts");
    p->next = ls;
    p = p->next;

    ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
    memset(ls, 0, sizeof(*ls));
    ls->flags = SFL_NOSELECT;
    ls->display_item =
	       cpystr(repeat_char(width, '-'));
    p->next = ls;
    p = p->next;

    /* then comes a list of individual character sets */
    for(cs = utf8_charset(NIL); cs && cs->name; cs++){
	/*
	 * Filter out some un-useful members of the list.
	 */
	if(!strucmp(cs->name, "ISO-2022-JP-2")
	   || !strucmp(cs->name, "UTF-7")
	   || !strucmp(cs->name, "UTF-8"))
	  continue;

	ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
	memset(ls, 0, sizeof(*ls));
	ls->item = cpystr(cs->name);

	if(p){
	    p->next = ls;
	    p = p->next;
	}
	else
	  listhead = p = ls;
    }
    
    if(!listhead)
      return(ret);
    
    if(!select_from_list_screen(listhead, SFL_ALLOW_LISTMODE,
				"SELECT CHARACTER SETS", "character sets ",
				h_select_multcharsets_screen,
			        "HELP FOR SELECTING CHARACTER SETS")){
	for(cnt = 0, p = listhead; p; p = p->next)
	  if(p->selected)
	    cnt++;

	ret = (char **) fs_get((cnt+1) * sizeof(*ret));
	memset(ret, 0, (cnt+1) * sizeof(*ret));
	for(i = 0, p = listhead; p; p = p->next)
	  if(p->selected)
	    ret[i++] = cpystr(p->item ? p->item : "");
    }

    free_list_sel(&listhead);

    return(ret);
}


/*
 * Allow user to choose a rule from their list of rules.
 *
 * Returns an allocated rule nickname on success, NULL otherwise.
 */
char *
choose_a_rule(rflags)
    int rflags;
{
    char      *choice = NULL;
    char     **rule_list, **lp;
    int        cnt = 0;
    PAT_S     *pat;
    PAT_STATE  pstate;

    if(!(nonempty_patterns(rflags, &pstate) && first_pattern(&pstate))){
	q_status_message(SM_ORDER, 3, 3,
			 "No rules available. Use Setup/Rules to add some.");
	return(choice);
    }

    /*
     * Build a list of rules to choose from.
     */

    for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate))
      cnt++;

    if(cnt <= 0){
	q_status_message(SM_ORDER, 3, 4, "No rules defined, use Setup/Rules");
	return(choice);
    }

    lp = rule_list = (char **) fs_get((cnt + 1) * sizeof(*rule_list));
    memset(rule_list, 0, (cnt+1) * sizeof(*rule_list));

    for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate))
      *lp++ = cpystr((pat->patgrp && pat->patgrp->nick)
			  ? pat->patgrp->nick : "?");

    choice = choose_item_from_list(rule_list, "SELECT A RULE",
				   "rules ", h_select_rule_screen,
				   "HELP FOR SELECTING A RULE NICKNAME");

    if(!choice)
      q_status_message(SM_ORDER, 1, 4, "No choice");

    free_list_array(&rule_list);

    return(choice);
}


PATGRP_S *
nick_to_patgrp(nick, rflags)
    char *nick;
    int   rflags;
{
    PAT_S     *pat;
    PAT_STATE  pstate;
    PATGRP_S  *patgrp = NULL;

    if(!(nick && *nick
	 && nonempty_patterns(rflags, &pstate) && first_pattern(&pstate)))
      return(patgrp);

    for(pat = first_pattern(&pstate);
	!patgrp && pat;
	pat = next_pattern(&pstate))
      if(pat->patgrp && pat->patgrp->nick && !strcmp(pat->patgrp->nick, nick))
	patgrp = copy_patgrp(pat->patgrp);

    return(patgrp);
}


/*----------------------------------------------------------------------
   Prompt the user for the type of sort he desires

Args: state -- pine state pointer
      q1 -- Line to prompt on

      Returns 0 if it was cancelled, 1 otherwise.
  ----*/
int
select_sort(state, ql, sort, rev)
    struct pine *state;
    int	  ql;
    SortOrder	 *sort;
    int	 *rev;
{
    char      prompt[200], tmp[3], *p;
    int       s, i;
    int       deefault = 'a', retval = 1;
    HelpType  help;
    ESCKEY_S  sorts[14];

#ifdef _WINDOWS
    DLG_SORTPARAM	sortsel;

    if (mswin_usedialog ()) {

	sortsel.reverse = mn_get_revsort (state->msgmap);
	sortsel.cursort = mn_get_sort (state->msgmap);
	sortsel.helptext = get_help_text (h_select_sort);
	sortsel.rval = 0;

	if ((retval = os_sortdialog (&sortsel))) {
	    *sort = sortsel.cursort;
	    *rev  = sortsel.reverse;
        }

	free_list_array(&sortsel.helptext);

	return (retval);
    }
#endif

    /*----- String together the prompt ------*/
    tmp[1] = '\0';
    if(F_ON(F_USE_FK,ps_global))
      strncpy(prompt, "Choose type of sort : ", sizeof(prompt));
    else
      strncpy(prompt, "Choose type of sort, or 'R' to reverse current sort : ",
	      sizeof(prompt));

    for(i = 0; state->sort_types[i] != EndofList; i++) {
	sorts[i].rval	   = i;
	p = sorts[i].label = sort_name(state->sort_types[i]);
	while(*(p+1) && islower((unsigned char)*p))
	  p++;

	sorts[i].ch   = tolower((unsigned char)(tmp[0] = *p));
	sorts[i].name = cpystr(tmp);

        if(mn_get_sort(state->msgmap) == state->sort_types[i])
	  deefault = sorts[i].rval;
    }

    sorts[i].ch     = 'r';
    sorts[i].rval   = 'r';
    sorts[i].name   = cpystr("R");
    if(F_ON(F_USE_FK,ps_global))
      sorts[i].label  = "Reverse";
    else
      sorts[i].label  = "";

    sorts[++i].ch   = -1;
    help = h_select_sort;

    if((F_ON(F_USE_FK,ps_global)
        && ((s = double_radio_buttons(prompt,ql,sorts,deefault,'x',
				      help,RB_NORM)) != 'x'))
       ||
       (F_OFF(F_USE_FK,ps_global)
        && ((s = radio_buttons(prompt,ql,sorts,deefault,'x',
			       help,RB_NORM, NULL)) != 'x'))){
	state->mangled_body = 1;		/* signal screen's changed */
	if(s == 'r')
	  *rev = !mn_get_revsort(state->msgmap);
	else
	  *sort = state->sort_types[s];

	if(F_ON(F_SHOW_SORT, ps_global))
	  ps_global->mangled_header = 1;
    }
    else{
	retval = 0;
	cmd_cancelled("Sort");
    }

    while(--i >= 0)
      fs_give((void **)&sorts[i].name);

    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
    return(retval);
}


/*---------------------------------------------------------------------
  Build list of folders in the given context for user selection

  Args: c -- pointer to pointer to folder's context context 
	f -- folder prefix to display
	sublist -- whether or not to use 'f's contents as prefix
	lister -- function used to do the actual display

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with local "selected" flag.
  ----*/
int
display_folder_list(c, f, sublist, lister)
    CONTEXT_S **c;
    char       *f;
    int	        sublist;
    int	      (*lister) PROTO((struct pine *, CONTEXT_S **, char *, int));
{
    int	       rc;
    CONTEXT_S *tc;
    TITLEBAR_STATE_S *tbstate = NULL;
    void (*redraw)() = ps_global->redrawer;

    tbstate = save_titlebar_state();
    tc = *c;
    if(rc = (*lister)(ps_global, &tc, f, sublist))
      *c = tc;

    ClearScreen();
    restore_titlebar_state(tbstate);
    free_titlebar_state(&tbstate);
    redraw_titlebar();
    if(ps_global->redrawer = redraw) /* reset old value, and test */
      (*ps_global->redrawer)();

    if(rc == 1 && F_ON(F_SELECT_WO_CONFIRM, ps_global))
      return(1);

    return(0);
}



/*----------------------------------------------------------------------
  Build comma delimited list of selected messages

  Args: stream -- mail stream to use for flag testing
	msgmap -- message number struct of to build selected messages in
	count -- pointer to place to write number of comma delimited
	mark -- mark manually undeleted so we don't refilter right away

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with local "selected" flag.
  ----*/
char *
selected_sequence(stream, msgmap, count, mark)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    long       *count;
    int         mark;
{
    long  i;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    /*
     * The plan here is to use the c-client elt's "sequence" bit
     * to work around any orderings or exclusions in pine's internal
     * mapping that might cause the sequence to be artificially
     * lengthy.  It's probably cheaper to run down the elt list
     * twice rather than call nm_raw2m() for each message as
     * we run down the elt list once...
     */
    for(i = 1L; i <= stream->nmsgs; i++)
      if((mc = mail_elt(stream, i)) != NULL)
        mc->sequence = 0;

    for(i = 1L; i <= mn_get_total(msgmap); i++)
      if(get_lflag(stream, msgmap, i, MN_SLCT)){
	  long rawno;
	  int  exbits = 0;

	  /*
	   * Forget we knew about it, and set "add to sequence"
	   * bit...
	   */
	  clear_index_cache_ent(i);
	  rawno = mn_m2raw(msgmap, i);
	  if(rawno > 0L && rawno <= stream->nmsgs
	     && (mc = mail_elt(stream, rawno)))
	    mc->sequence = 1;

	  /*
	   * Mark this message manually flagged so we don't re-filter it
	   * with a filter which only sets flags.
	   */
	  if(mark){
	      if(msgno_exceptions(stream, rawno, "0", &exbits, FALSE))
		exbits |= MSG_EX_MANUNDEL;
	      else
		exbits = MSG_EX_MANUNDEL;

	      msgno_exceptions(stream, rawno, "0", &exbits, TRUE);
	  }
      }

    return(build_sequence(stream, NULL, count));
}


/*----------------------------------------------------------------------
  Build comma delimited list of messages current in msgmap which have all
  flags matching the arguments

  Args: stream -- mail stream to use for flag testing
	msgmap -- only consider messages selected in this msgmap
	  flag -- flags to match against
	 count -- pointer to place to return number of comma delimited
	  mark -- mark index cache entry changed, and count state change
	 kw_on -- if flag contains F_KEYWORD, this is
	            the array of keywords to be checked
	kw_off -- if flag contains F_UNKEYWORD, this is
	            the array of keywords to be checked

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with local "selected" flag (a flag
	   of zero means all current msgs).
  ----*/
char *
currentf_sequence(stream, msgmap, flag, count, mark, kw_on, kw_off)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    long	flag;
    long       *count;
    int		mark;
    char      **kw_on;
    char      **kw_off;
{
    char	 *seq, *q, **t;
    long	  i, rawno;
    int           exbits, j, is_set;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    /* First, make sure elts are valid for all the interesting messages */
    if(seq = invalid_elt_sequence(stream, msgmap)){
	pine_mail_fetch_flags(stream, seq, NIL);
	fs_give((void **) &seq);
    }

    for(i = 1L; i <= stream->nmsgs; i++)
      if((mc = mail_elt(stream, i)) != NULL)
        mc->sequence = 0;			/* clear "sequence" bits */

    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){
	/* if not already set, go on... */
	rawno = mn_m2raw(msgmap, i);
	mc = (rawno > 0L && rawno <= stream->nmsgs)
		? mail_elt(stream, rawno) : NULL;
	if(!mc)
	  continue;

	if((flag == 0)
	   || ((flag & F_DEL) && mc->deleted)
	   || ((flag & F_UNDEL) && !mc->deleted)
	   || ((flag & F_SEEN) && mc->seen)
	   || ((flag & F_UNSEEN) && !mc->seen)
	   || ((flag & F_ANS) && mc->answered)
	   || ((flag & F_UNANS) && !mc->answered)
	   || ((flag & F_FLAG) && mc->flagged)
	   || ((flag & F_UNFLAG) && !mc->flagged)){
	    mc->sequence = 1;			/* set "sequence" flag */
	}

	/* check for user keywords or not */
	if(!mc->sequence && flag & F_KEYWORD && kw_on){
	    for(t = kw_on; !mc->sequence && *t; t++)
	      if(user_flag_is_set(stream, rawno, *t))
		mc->sequence = 1;
	}
	else if(!mc->sequence && flag & F_UNKEYWORD && kw_off){
	    for(t = kw_off; !mc->sequence && *t; t++)
	      if(!user_flag_is_set(stream, rawno, *t))
		mc->sequence = 1;
	}
	
	if(mc->sequence){
	    if(mark){
		if(THRD_INDX()){
		    PINETHRD_S *thrd;
		    long        t;

		    /* clear thread index line instead of index index line */
		    thrd = fetch_thread(stream, mn_m2raw(msgmap, i));
		    if(thrd && thrd->top
		       && (thrd=fetch_thread(stream,thrd->top))
		       && (t = mn_raw2m(msgmap, thrd->rawno)))
		      clear_index_cache_ent(t);
		}
		else
		  clear_index_cache_ent(i);	/* force new index line */

		/*
		 * Mark this message manually flagged so we don't re-filter it
		 * with a filter which only sets flags.
		 */
		exbits = 0;
		if(msgno_exceptions(stream, rawno, "0", &exbits, FALSE))
		  exbits |= MSG_EX_MANUNDEL;
		else
		  exbits = MSG_EX_MANUNDEL;

		msgno_exceptions(stream, rawno, "0", &exbits, TRUE);
	    }
	}
    }

    return(build_sequence(stream, NULL, count));
}


/*----------------------------------------------------------------------
  Return sequence numbers of messages with invalid MESSAGECACHEs

  Args: stream -- mail stream to use for flag testing
	msgmap -- message number struct of to build selected messages in

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with local "selected" flag (a flag
	   of zero means all current msgs).
  ----*/
char *
invalid_elt_sequence(stream, msgmap)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
{
    long	  i, rawno;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    for(i = 1L; i <= stream->nmsgs; i++)
      if((mc = mail_elt(stream, i)) != NULL)
        mc->sequence = 0;			/* clear "sequence" bits */

    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap))
      if((rawno = mn_m2raw(msgmap, i)) > 0L && rawno <= stream->nmsgs
	 && (mc = mail_elt(stream, rawno)) && !mc->valid)
	mc->sequence = 1;

    return(build_sequence(stream, NULL, NULL));
}


/*----------------------------------------------------------------------
  Build comma delimited list of messages with elt "sequence" bit set

  Args: stream -- mail stream to use for flag testing
	msgmap -- struct containing sort to build sequence in
	count -- pointer to place to write number of comma delimited
		 NOTE: if non-zero, it's a clue as to how many messages
		       have the sequence bit lit.

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with elt's "sequence" bit set
  ----*/
char *
build_sequence(stream, msgmap, count)
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;
    long       *count;
{
#define	SEQ_INCREMENT	128
    long    n = 0L, i, x, lastn = 0L, runstart = 0L;
    size_t  size = SEQ_INCREMENT;
    char   *seq = NULL, *p;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    if(count){
	if(*count > 0L)
	  size = max(size, min((*count) * 4, 16384));

	*count = 0L;
    }

    for(x = 1L; x <= stream->nmsgs; x++){
	if(msgmap){
	    if((i = mn_m2raw(msgmap, x)) == 0L)
	      continue;
	}
	else
	  i = x;

	if(i > 0L && i <= stream->nmsgs
	   && (mc = mail_elt(stream, i)) && mc->sequence){
	    n++;
	    if(!seq)				/* initialize if needed */
	      seq = p = fs_get(size);

	    /*
	     * This code will coalesce the ascending runs of
	     * sequence numbers, but fails to break sequences
	     * into a reasonably sensible length for imapd's to
	     * swallow (reasonable addtition to c-client?)...
	     */
	    if(lastn){				/* if may be in a run */
		if(lastn + 1L == i){		/* and its the next raw num */
		    lastn = i;			/* skip writing anything... */
		    continue;
		}
		else if(runstart != lastn){
		    *p++ = (runstart + 1L == lastn) ? ',' : ':';
		    sstrcpy(&p, long2string(lastn));
		}				/* wrote end of run */
	    }

	    runstart = lastn = i;		/* remember last raw num */

	    if(n > 1L)				/* !first num, write delim */
	      *p++ = ',';

	    if(size - (p - seq) < 16){	/* room for two more nums? */
		size_t offset = p - seq;	/* grow the sequence array */
		size += SEQ_INCREMENT;
		fs_resize((void **)&seq, size);
		p = seq + offset;
	    }

	    sstrcpy(&p, long2string(i));	/* write raw number */
	}
    }

    if(lastn && runstart != lastn){		/* were in a run? */
	*p++ = (runstart + 1L == lastn) ? ',' : ':';
	sstrcpy(&p, long2string(lastn));	/* write the trailing num */
    }

    if(seq)					/* if sequence, tie it off */
      *p  = '\0';

    if(count)
      *count = n;

    return(seq);
}



/*----------------------------------------------------------------------
  If any messages flagged "selected", fake the "currently selected" array

  Args: map -- message number struct of to build selected messages in

  OK folks, here's the tradeoff: either all the functions have to
  know if the user want's to deal with the "current" hilited message
  or the list of currently "selected" messages, *or* we just
  wrap the call to these functions with some glue that tweeks
  what these functions see as the "current" message list, and let them
  do their thing.
  ----*/
int
pseudo_selected(map)
    MSGNO_S *map;
{
    long i, later = 0L;

    if(any_lflagged(map, MN_SLCT)){
	map->hilited = mn_m2raw(map, mn_get_cur(map));

	for(i = 1L; i <= mn_get_total(map); i++)
	  /* BUG: using the global mail_stream is kind of bogus since
	   * everybody that calls us get's a pine stuct passed it.
	   * perhaps a stream pointer in the message struct makes 
	   * sense?
	   */
	  if(get_lflag(ps_global->mail_stream, map, i, MN_SLCT)){
	      if(!later++){
		  mn_set_cur(map, i);
	      }
	      else{
		  mn_add_cur(map, i);
	      }
	  }

	return(1);
    }

    return(0);
}


/*----------------------------------------------------------------------
  Antidote for the monkey business committed above

  Args: map -- message number struct of to build selected messages in

  ----*/
void
restore_selected(map)
    MSGNO_S *map;
{
    if(map->hilited){
	mn_reset_cur(map, mn_raw2m(map, map->hilited));
	map->hilited = 0L;
    }
}


/*
 * Get the user name from the mailbox portion of an address.
 *
 * Args: mailbox -- the mailbox portion of an address (lhs of address)
 *       target  -- a buffer to put the result in
 *       len     -- length of the target buffer
 *
 * Returns the left most portion up to the first '%', ':' or '@',
 * and to the right of any '!' (as if c-client would give us such a mailbox).
 * Returns NULL if it can't find a username to point to.
 */
char *
get_uname(mailbox, target, len)
    char  *mailbox,
	  *target;
    int    len;
{
    int i, start, end;

    if(!mailbox || !*mailbox)
      return(NULL);

    end = strlen(mailbox) - 1;
    for(start = end; start > -1 && mailbox[start] != '!'; start--)
        if(strindex("%:@", mailbox[start]))
	    end = start - 1;

    start++;			/* compensate for either case above */

    for(i = start; i <= end && (i-start) < (len-1); i++) /* copy name */
      target[i-start] = isupper((unsigned char)mailbox[i])
					  ? tolower((unsigned char)mailbox[i])
					  : mailbox[i];

    target[i-start] = '\0';	/* tie it off */

    return(*target ? target : NULL);
}


/*
 * file_lister - call pico library's file lister
 */
int
file_lister(title, path, pathlen, file, filelen, newmail, flags)
    char *title, *path, *file;
    int   pathlen, filelen, newmail, flags;
{
    PICO   pbf;
    int	   rv;
    TITLEBAR_STATE_S *tbstate = NULL;
    void (*redraw)() = ps_global->redrawer;

    tbstate = save_titlebar_state();
    standard_picobuf_setup(&pbf);
    if(!newmail)
      pbf.newmail = NULL;

/* BUG: what about help command and text? */
    pbf.pine_anchor   = title;

    rv = pico_file_browse(&pbf, path, pathlen, file, filelen, NULL, flags);
    standard_picobuf_teardown(&pbf);
    fix_windsize(ps_global);
    init_signals();		/* has it's own signal stuff */

    /* Restore display's titlebar and body */
    restore_titlebar_state(tbstate);
    free_titlebar_state(&tbstate);
    redraw_titlebar();
    if(ps_global->redrawer = redraw)
      (*ps_global->redrawer)();

    return(rv);
}


#ifdef	_WINDOWS


/*
 * windows callback to get/set header mode state
 */
int
header_mode_callback(set, args)
    int  set;
    long args;
{
    return(ps_global->full_header);
}


/*
 * windows callback to get/set zoom mode state
 */
int
zoom_mode_callback(set, args)
    int  set;
    long args;
{
    return(any_lflagged(ps_global->msgmap, MN_HIDE) != 0);
}


/*
 * windows callback to get/set zoom mode state
 */
int
any_selected_callback(set, args)
    int  set;
    long args;
{
    return(any_lflagged(ps_global->msgmap, MN_SLCT) != 0);
}


/*
 *
 */
int
flag_callback(set, flags)
    int  set;
    long flags;
{
    MESSAGECACHE *mc;
    int		  newflags = 0;
    long	  msgno;
    int		  permflag = 0;

    switch (set) {
      case 1:			/* Important */
        permflag = ps_global->mail_stream->perm_flagged;
	break;

      case 2:			/* New */
        permflag = ps_global->mail_stream->perm_seen;
	break;

      case 3:			/* Answered */
        permflag = ps_global->mail_stream->perm_answered;
	break;

      case 4:			/* Deleted */
        permflag = ps_global->mail_stream->perm_deleted;
	break;

    }

    if(!(any_messages(ps_global->msgmap, NULL, "to Flag")
	 && can_set_flag(ps_global, "flag", permflag)))
      return(0);

    if(sp_io_error_on_stream(ps_global->mail_stream)){
	sp_set_io_error_on_stream(ps_global->mail_stream, 0);
	pine_mail_check(ps_global->mail_stream);	/* forces write */
	return(0);
    }

    msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));
    if(msgno > 0L && ps_global->mail_stream
       && msgno <= ps_global->mail_stream->nmsgs
       && (mc = mail_elt(ps_global->mail_stream, msgno))
       && mc->valid){
	/*
	 * NOTE: code below is *VERY* sensitive to the order of
	 * the messages defined in resource.h for flag handling.
	 * Don't change it unless you know what you're doing.
	 */
	if(set){
	    char *flagstr;
	    long  ourflag, mflag;

	    switch(set){
	      case 1 :			/* Important */
		flagstr = "\\FLAGGED";
		mflag   = (mc->flagged) ? 0L : ST_SET;
		break;

	      case 2 :			/* New */
		flagstr = "\\SEEN";
		mflag   = (mc->seen) ? 0L : ST_SET;
		break;

	      case 3 :			/* Answered */
		flagstr = "\\ANSWERED";
		mflag   = (mc->answered) ? 0L : ST_SET;
		break;

	      case 4 :		/* Deleted */
		flagstr = "\\DELETED";
		mflag   = (mc->deleted) ? 0L : ST_SET;
		break;

	      default :			/* bogus */
		return(0);
	    }

	    mail_flag(ps_global->mail_stream, long2string(msgno),
		      flagstr, mflag);

	    if(ps_global->redrawer)
	      (*ps_global->redrawer)();
	}
	else{
	    /* Important */
	    if(mc->flagged)
	      newflags |= 0x0001;

	    /* New */
	    if(!mc->seen)
	      newflags |= 0x0002;

	    /* Answered */
	    if(mc->answered)
	      newflags |= 0x0004;

	    /* Deleted */
	    if(mc->deleted)
	      newflags |= 0x0008;
	}
    }

    return(newflags);
}



MPopup *
flag_submenu(mc)
    MESSAGECACHE *mc;
{
    static MPopup flag_submenu[] = {
	{tMessage, {"Important", lNormal}, {IDM_MI_FLAGIMPORTANT}},
	{tMessage, {"New", lNormal}, {IDM_MI_FLAGNEW}},
	{tMessage, {"Answered", lNormal}, {IDM_MI_FLAGANSWERED}},
	{tMessage , {"Deleted", lNormal}, {IDM_MI_FLAGDELETED}},
	{tTail}
    };

    /* Important */
    flag_submenu[0].label.style = (mc && mc->flagged) ? lChecked : lNormal;

    /* New */
    flag_submenu[1].label.style = (mc && mc->seen) ? lNormal : lChecked;

    /* Answered */
    flag_submenu[2].label.style = (mc && mc->answered) ? lChecked : lNormal;

    /* Deleted */
    flag_submenu[3].label.style = (mc && mc->deleted) ? lChecked : lNormal;

    return(flag_submenu);
}
#endif	/* _WINDOWS */
