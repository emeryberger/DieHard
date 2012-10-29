#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailview.c 14091 2005-09-20 18:26:20Z hubert@u.washington.edu $";
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
     mailview.c
     Implements the mailview screen
     Also includes scrolltool used to display help text
  ====*/


#include "headers.h"


/*----------------------------------------------------------------------
    Saved state for scrolling text 
 ----*/
typedef struct scroll_text {
    SCROLL_S	*parms;		/* Original text (file, char *, char **)     */
    char       **text_lines,	/* Lines to display			     */
		*fname;		/* filename of line offsets in "text"	     */
    FILE	*findex;	/* file pointer to line offsets in "text"    */
    short	*line_lengths;	/* Length of each line in "text_lines"	     */
    long	 top_text_line,	/* index in "text_lines" top displayed line  */
		 num_lines;	/* number of valid pointers in "text_lines"  */
    int		 lines_allocated; /* size of "text_lines" array		     */
    struct {
	int length,		/* count of displayable lines (== PGSIZE)    */
	    width,		/* width of displayable lines		     */
	    start_line,		/* line number to start painting on	     */
	    other_lines;	/* # of lines not for scroll text	     */
    } screen;			/* screen parameters			     */
} SCRLCTRL_S;


typedef struct scroll_file {
    long	offset;
    int		len;
} SCRLFILE_S;


/*
 * Struct to help write lines do display as they're decoded
 */
struct view_write_s {
    char      *line;
    int	       index,
	       screen_line,
	       last_screen_line;
#ifdef	_WINDOWS
    long       lines;
#endif
    HANDLE_S **handles;
    STORE_S   *store;
} *g_view_write;

#define LINEBUFSIZ (4096)

#define MAX_FUDGE (1024*1024)

/*
 * Struct to help with editorial comment insertion
 */
#define	EDITORIAL_MAX	128
typedef struct _editorial_s {
    char prefix[EDITORIAL_MAX];
    int	 prelen;
    char postfix[EDITORIAL_MAX];
    int	 postlen;
} EDITORIAL_S;


/*
 * If the number of lines in the quote is equal to lines or less, then
 * we show the quote. If the number of lines in the quote is more than lines,
 * then we show lines-1 of the quote followed by one line of editorial
 * comment.
 */
typedef struct del_q_s {
    int        lines;		/* show this many lines (counting editorial) */
    int        indent_length;	/* skip over this indent                     */
    int        is_flowed;	/* message is labelled flowed                */
    HANDLE_S **handlesp;
    int        in_quote;	/* dynamic data */
    char     **saved_line;	/*   "          */
} DELQ_S;


/*
 * Def's to help in sorting out multipart/alternative
 */
#define	SHOW_NONE	0
#define	SHOW_PARTS	1
#define	SHOW_ALL_EXT	2
#define	SHOW_ALL	3


#define FBUF_LEN	(50)


/*
 * Definitions to help scrolltool
 */
#define SCROLL_LINES_ABOVE(X)	HEADER_ROWS(X)
#define SCROLL_LINES_BELOW(X)	FOOTER_ROWS(X)
#define	SCROLL_LINES(X)		max(((X)->ttyo->screen_rows               \
			 - SCROLL_LINES_ABOVE(X) - SCROLL_LINES_BELOW(X)), 0)
#define	scroll_text_lines()	(scroll_state(SS_CUR)->num_lines)


/*
 * Definitions for various scroll state manager's functions
 */
#define	SS_NEW	1
#define	SS_CUR	2
#define	SS_FREE	3


/*
 * Def's to help page reframing based on handles
 */
#define	SHR_NONE	0
#define	SHR_CUR		1
#define	SHR_


/*
 * Handle hints.
 */
#define	HANDLE_INIT_MSG		\
  "Selectable items in text -- Use Up/Down Arrows to choose, Return to view"
#define	HANDLE_ABOVE_ERR	\
  "No selected item displayed -- Use PrevPage to bring choice into view"
#define	HANDLE_BELOW_ERR	\
  "No selected item displayed -- Use NextPage to bring choice into view"


#define	PGSIZE(X)      (ps_global->ttyo->screen_rows - (X)->screen.other_lines)
#define	ISRFCEOL(S)    (*(S) == '\015' && *((S)+1) == '\012')

#define TYPICAL_BIG_MESSAGE_LINES	200

#define	CHARSET_DISCLAIMER_1	\
	"The following text is in the \"%.40s\" character set.\015\012Your "
#define	CHARSET_DISCLAIMER_2	"display is set"
#define	CHARSET_DISCLAIMER_3	\
       " for the \"%.40s\" character set. \015\012Some %.40scharacters may be displayed incorrectly."
#define ENCODING_DISCLAIMER      \
        "The following text contains the unknown encoding type \"%.20s\". \015\012Some or all of the text may be displayed incorrectly."


/*
 * Keymenu/Command mappings for various scrolltool modes.
 */
static struct key view_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"<","MsgIndex",{MC_INDEX,3,{'i','<',','}},KS_FLDRINDEX},
	{">","ViewAttch",{MC_VIEW_ATCH,3,{'v','>','.'}},KS_NONE},
	PREVMSG_MENU,
	NEXTMSG_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	REPLY_MENU,
	FORWARD_MENU,

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
	LISTFLD_MENU,
	GOTO_MENU,
	COMPOSE_MENU,
	WHEREIS_MENU,
	PRYNTMSG_MENU,
	TAKE_MENU,
	SAVE_MENU,
	EXPORT_MENU,

	HELP_MENU,
	OTHER_MENU,
	{"Ret","[View Hilite]",{MC_VIEW_HANDLE,3,
				{ctrl('m'),ctrl('j'),KEY_RIGHT}},KS_NONE},
	{":","SelectCur",{MC_SELCUR,1,{':'}},KS_SELECTCUR},
	{"^B","Prev URL",{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F","Next URL",{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	JUMP_MENU,
	TAB_MENU,
	HDRMODE_MENU,
	BOUNCE_MENU,
	FLAG_MENU,
	PIPE_MENU,

	HELP_MENU,
	OTHER_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	RCOMPOSE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(view_keymenu, view_keys);
#define VIEW_ATT_KEY		 3
#define VIEW_FULL_HEADERS_KEY	32
#define	VIEW_VIEW_HANDLE	26
#define VIEW_SELECT_KEY		27
#define VIEW_PREV_HANDLE	28
#define VIEW_NEXT_HANDLE	29
#define BOUNCE_KEY		33
#define FLAG_KEY		34
#define VIEW_PIPE_KEY		35

static struct key simple_text_keys[] =
       {HELP_MENU,
	NULL_MENU,
	{"E","Exit Viewer",{MC_EXIT,1,{'e'}},KS_EXITMODE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", "Save", {MC_SAVETEXT,1,{'s'}}, KS_SAVE}};
INST_KEY_MENU(simple_text_keymenu, simple_text_keys);




/*
 * Internal prototypes
 */
int	    is_an_env_hdr PROTO((char *));
int         is_an_addr_hdr PROTO((char *));
int         any_hdr_color PROTO((char *));
int	    format_blip_seen PROTO((long));
int	    charset_editorial PROTO((char *, long, HANDLE_S **, int, int,
				     int, gf_io_t));
int	    encoding_editorial PROTO((BODY *, int, gf_io_t));
int	    rfc2369_editorial PROTO((long, HANDLE_S **, int, int, gf_io_t));
void	    format_envelope PROTO((MAILSTREAM *, long, char *, ENVELOPE *,
				   gf_io_t, long, int));
void	    format_env_hdr PROTO((MAILSTREAM *, long, char *, ENVELOPE *,
				  gf_io_t, char *, int));
int	    format_raw_header PROTO((MAILSTREAM *, long, char *, gf_io_t));
void	    format_addr_string PROTO((MAILSTREAM *, long, char *, char *,
				      ADDRESS *, int, gf_io_t));
void	    format_newsgroup_string PROTO((char *, char *, int, gf_io_t));
int	    format_raw_hdr_string PROTO((char *, char *, gf_io_t, int));
int	    format_env_puts PROTO((char *, gf_io_t));
long	    format_size_guess PROTO((BODY *));
int	   *format_view_margin PROTO((void));
void        pine_rfc822_write_address_noquote PROTO((ADDRESS *,
						     gf_io_t, int *));
void        pine2_rfc822_write_address_noquote PROTO((char *,ADDRESS *,int *));
void        pine_rfc822_address PROTO((ADDRESS *, gf_io_t));
void        pine_rfc822_cat PROTO((char *, const char *, gf_io_t));
int	    delineate_this_header PROTO ((char *, char *, char **, char **));
void	    view_writec_init PROTO((STORE_S *, HANDLE_S **, int, int));
void	    view_writec_destroy PROTO((void));
void	    view_writec_killbuf PROTO((void));
int	    view_writec PROTO((int));
int	    view_end_scroll PROTO((SCROLL_S *));
int	    quote_editorial PROTO((long, char *, LT_INS_S **, void *));
int	    replace_quotes PROTO((long, char *, LT_INS_S **, void *));
int	    delete_quotes PROTO((long, char *, LT_INS_S **, void *));
void        delete_unused_handles PROTO((HANDLE_S **));
void        mark_handles_in_line PROTO((char *, HANDLE_S **, int));
void	    set_scroll_text PROTO((SCROLL_S *, long, SCRLCTRL_S *));
long	    scroll_scroll_text PROTO((long, HANDLE_S *, int));
static int  print_to_printer PROTO((SCROLL_S *));
int	    search_scroll_text PROTO((long, int, char *, Pos *, int *));
char	   *search_scroll_line PROTO((char *, char *, int, int));
void	    describe_mime PROTO((BODY *, char *, int, int, int));
int         mime_known_text_subtype PROTO((char *));
void	    format_mime_size PROTO((char *, BODY *));
ATTACH_S   *next_attachment PROTO(());
void	    zero_atmts PROTO((ATTACH_S *));
void	    zero_scroll_text PROTO((void));
char	   *body_parameter PROTO((BODY *, char *));
void	    ScrollFile PROTO((long));
long	    make_file_index PROTO(());
char	   *scroll_file_line PROTO((FILE *, char *, SCRLFILE_S *, int *));
char	   *show_multipart PROTO((MESSAGECACHE *, int));
int	    mime_show PROTO((BODY *));
SCRLCTRL_S *scroll_state PROTO((int));
int	    search_text PROTO((int, long, int, char *, Pos *, int *));
void	    format_scroll_text PROTO((void));
void	    redraw_scroll_text PROTO((void));
void	    update_scroll_titlebar PROTO((long, int));
int	    find_field PROTO((char **, char *, size_t));
void	    free_handle PROTO((HANDLE_S **));
void	    free_handle_locations PROTO((POSLIST_S **));
int	    scroll_handle_obscured PROTO((HANDLE_S *));
int	    scroll_handle_selectable PROTO((HANDLE_S *));
HANDLE_S   *scroll_handle_next_sel PROTO((HANDLE_S *));
HANDLE_S   *scroll_handle_prev_sel PROTO((HANDLE_S *));
HANDLE_S   *scroll_handle_next PROTO((HANDLE_S *));
HANDLE_S   *scroll_handle_prev PROTO((HANDLE_S *));
HANDLE_S   *scroll_handle_boundary PROTO((HANDLE_S *,
					  HANDLE_S *(*) PROTO((HANDLE_S *))));
int	    scroll_handle_column PROTO((int, int));
int	    scroll_handle_index PROTO((int, int));
void	    scroll_handle_set_loc PROTO((POSLIST_S **, int, int));
int	    scroll_handle_start_color PROTO((char *, int *));
int	    scroll_handle_end_color PROTO((char *, int *));
int	    scroll_handle_prompt PROTO((HANDLE_S *, int));
int	    scroll_handle_launch PROTO((HANDLE_S *, int));
HANDLE_S   *scroll_handle_in_frame PROTO((long));
long	    scroll_handle_reframe PROTO((int, int));
int         visible_linelen PROTO((int));
int	    handle_on_line PROTO((long, int));
int	    handle_on_page PROTO((HANDLE_S *, long, long));
int	    dot_on_handle PROTO((long, int));
COLOR_PAIR *get_cur_embedded_color PROTO((void));
void        clear_cur_embedded_color PROTO((void));
char	   *url_embed PROTO((int));
void	    embed_color PROTO((COLOR_PAIR *, gf_io_t));
int         color_a_quote PROTO((long, char *, LT_INS_S **, void *));
int         color_signature PROTO((long, char *, LT_INS_S **, void *));
int         color_headers PROTO((long, char *, LT_INS_S **, void *));
int	    url_hilite_hdr PROTO((long, char *, LT_INS_S **, void *));
int	    url_launch PROTO((HANDLE_S *));
int	    url_launch_too_long PROTO((int));
char	   *url_external_handler PROTO((HANDLE_S *, int));
void	    url_mailto_addr PROTO((ADDRESS **, char *));
int	    url_local_imap PROTO((char *));
int	    url_bogus_imap PROTO((char **, char *, char *));
int	    url_local_nntp PROTO((char *));
int	    url_local_news PROTO((char *));
int	    url_local_file PROTO((char *));
int	    url_local_phone_home PROTO((char *));
int	    url_bogus PROTO((char *, char *));
long        doubleclick_handle PROTO((SCROLL_S *, HANDLE_S *, 
				      int *, int *));
#ifdef	_WINDOWS
int	    format_message_popup PROTO((SCROLL_S *, int));
int	    simple_text_popup PROTO((SCROLL_S *, int));
int	    mswin_readscrollbuf PROTO((int));
int	    pcpine_do_scroll PROTO((int, long));
char	   *pcpine_help_scroll PROTO((char *));
int	    pcpine_resize_scroll PROTO((void));
int	    pcpine_view_cursor PROTO((int, long));
#endif



/*----------------------------------------------------------------------
     Format a buffer with the text of the current message for browser

    Args: ps - pine state structure
  
  Result: The scrolltool is called to display the message

  Loop here viewing mail until the folder changed or a command takes
us to another screen. Inside the loop the message text is fetched and
formatted into a buffer allocated for it.  These are passed to the
scrolltool(), that displays the message and executes commands. It
returns when it's time to display a different message, when we
change folders, when it's time for a different screen, or when
there are no more messages available.
  ---*/

void
mail_view_screen(ps)
     struct pine *ps;
{
    char            last_was_full_header = 0;
    long            last_message_viewed = -1L, raw_msgno, offset = 0L;
    OtherMenu       save_what = FirstMenu;
    int             we_cancel = 0, flags, i;
    MESSAGECACHE   *mc;
    ENVELOPE       *env;
    BODY           *body;
    STORE_S        *store;
    HANDLE_S	   *handles = NULL;
    SCROLL_S	    scrollargs;
    SourceType	    src = CharStar;

    dprint(1, (debugfile, "\n\n  -----  MAIL VIEW  -----\n"));

    ps->prev_screen = mail_view_screen;

    if(ps->ttyo->screen_rows - HEADER_ROWS(ps) - FOOTER_ROWS(ps) < 1){
	q_status_message(SM_ORDER | SM_DING, 0, 3,
			 "Screen too small to view message");
	ps->next_screen = mail_index_screen;
	return;
    }

    /*----------------- Loop viewing messages ------------------*/
    do {
	/*
	 * Check total to make sure there's something to view.  Check it
	 * inside the loop to make sure everything wasn't expunged while
	 * we were viewing.  If so, make sure we don't just come back.
	 */
	if(mn_get_total(ps->msgmap) <= 0L || !ps->mail_stream){
	    q_status_message(SM_ORDER, 0, 3, "No messages to read!");
	    ps->next_screen = mail_index_screen;
	    break;
	}

	we_cancel = busy_alarm(1, NULL, NULL, 0);

	if(mn_get_cur(ps->msgmap) <= 0L)
	  mn_set_cur(ps->msgmap,
		     THREADING() ? first_sorted_flagged(F_NONE,
							ps->mail_stream,
							0L, FSF_SKIP_CHID)
				 : 1L);

	raw_msgno = mn_m2raw(ps->msgmap, mn_get_cur(ps->msgmap));
	body      = NULL;
	if(raw_msgno == 0L
	   || !(env = pine_mail_fetchstructure(ps->mail_stream,raw_msgno,&body))
	   || !(raw_msgno > 0L && ps->mail_stream
	        && raw_msgno <= ps->mail_stream->nmsgs
		&& (mc = mail_elt(ps->mail_stream, raw_msgno)))){
	    q_status_message1(SM_ORDER, 3, 3,
			      "Error getting message %.200s data",
			      comatose(mn_get_cur(ps->msgmap)));
	    dprint(1, (debugfile, "!!!! ERROR fetching %s of msg %ld\n",
		       env ? "elt" : "env", mn_get_cur(ps->msgmap)));
		       
	    ps->next_screen = mail_index_screen;
	    break;
	}
	else
	  ps->unseen_in_view = !mc->seen;

#if	defined(DOS) && !defined(WIN32)
	/* 
	 * Handle big text for DOS here.
	 *
	 * judging from 1000+ message folders around here, it looks
	 * like 9X% of messages fall in the < 8k range, so it
	 * seems like this is as good a place to draw the line as any.
	 *
	 * We ALSO need to divert all news articles we read to secondary
	 * storage as its possible we're using c-client's NNTP driver
	 * which returns BOGUS sizes UNTIL the whole thing is fetched.
	 * Note: this is more a deficiency in NNTP than in c-client.
	 */
	src = (mc->rfc822_size > MAX_MSG_INCORE 
	       || (ps->mail_stream
	           && ps->mail_stream->dtb
		   && ps->mail_stream->dtb->name
		   && !strcmp(ps->mail_stream->dtb->name, "nntp")))
		? TmpFileStar : CharStar;
#endif
	init_handles(&handles);

	store = so_get(src, NULL, EDIT_ACCESS);
	so_truncate(store, format_size_guess(body) + 2048);

	view_writec_init(store, &handles, SCROLL_LINES_ABOVE(ps),
			 SCROLL_LINES_ABOVE(ps) + 
			 ps->ttyo->screen_rows - (SCROLL_LINES_ABOVE(ps)
						  + SCROLL_LINES_BELOW(ps)));

	flags = FM_DISPLAY;
	if(last_message_viewed != mn_get_cur(ps->msgmap)
	   || last_was_full_header == 2)
	  flags |= FM_NEW_MESS;

	if(F_OFF(F_QUELL_FULL_HDR_RESET, ps_global)
	   && last_message_viewed != -1L
	   && last_message_viewed != mn_get_cur(ps->msgmap))
	  ps->full_header = 0;

	if(offset)		/* no pre-paint during resize */
	  view_writec_killbuf();

#ifdef _WINDOWS
	mswin_noscrollupdate(1);
#endif
	(void) format_message(raw_msgno, env, body, &handles, flags,
			      view_writec);
#ifdef _WINDOWS
	mswin_noscrollupdate(0);
#endif

	view_writec_destroy();

	last_message_viewed  = mn_get_cur(ps->msgmap);
	last_was_full_header = ps->full_header;

	ps->next_screen = SCREEN_FUN_NULL;

	memset(&scrollargs, 0, sizeof(SCROLL_S));
	scrollargs.text.text	= so_text(store);
	scrollargs.text.src	= src;
	scrollargs.text.desc	= "message";

	/*
	 * make first selectable handle the default
	 */
	if(handles){
	    HANDLE_S *hp;

	    hp = handles;
	    while(!scroll_handle_selectable(hp) && hp != NULL)
	      hp = hp->next;

	    if(scrollargs.text.handles = hp)
	      scrollargs.body_valid = (hp == handles);
	    else
	      free_handles(&handles);
	}
	else
	  scrollargs.body_valid = 1;

	if(offset){		/* resize?  preserve paging! */
	    scrollargs.start.on		= Offset;
	    scrollargs.start.loc.offset = offset;
	    scrollargs.body_valid = 0;
	    offset = 0L;
	}
	  
	scrollargs.use_indexline_color = 1;

	scrollargs.bar.title	= "MESSAGE TEXT";
	scrollargs.end_scroll	= view_end_scroll;
	scrollargs.resize_exit	= 1;
	scrollargs.help.text	= h_mail_view;
	scrollargs.help.title	= "HELP FOR MESSAGE TEXT VIEW";
	scrollargs.keys.menu	= &view_keymenu;
	scrollargs.keys.what    = save_what;
	setbitmap(scrollargs.keys.bitmap);
	if(F_OFF(F_ENABLE_PIPE, ps_global))
	  clrbitn(VIEW_PIPE_KEY, scrollargs.keys.bitmap);

	/* 
	 * turn off attachment viewing for raw msg txt, atts 
	 * haven't been set up at this point
	 */
	if(ps_global->full_header == 2
	   && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
	  clrbitn(VIEW_ATT_KEY, scrollargs.keys.bitmap);

	if(F_OFF(F_ENABLE_BOUNCE, ps_global))
	  clrbitn(BOUNCE_KEY, scrollargs.keys.bitmap);

	if(F_OFF(F_ENABLE_FLAG, ps_global))
	  clrbitn(FLAG_KEY, scrollargs.keys.bitmap);

	if(F_OFF(F_ENABLE_AGG_OPS, ps_global))
	  clrbitn(VIEW_SELECT_KEY, scrollargs.keys.bitmap);

	if(F_OFF(F_ENABLE_FULL_HDR, ps_global))
	  clrbitn(VIEW_FULL_HEADERS_KEY, scrollargs.keys.bitmap);

	if(!handles){
	    /*
	     * NOTE: the comment below only really makes sense if we
	     *	     decide to replace the "Attachment Index" with
	     *	     a model that lets you highlight the attachments
	     *	     in the header.  In a way its consistent, but
	     *	     would mean more keymenu monkeybusiness since not
	     *	     all things would be available all the time. No wait.
	     *	     Then what would "Save" mean; the attachment, url or
	     *	     message you're currently viewing?  Would Save
	     *	     of a message then only be possible from the message
	     *	     index?  Clumsy.  what about arrow-navigation.  isn't
	     *	     that now inconsistent?  Maybe next/prev url shouldn't
	     *	     be bound to the arrow/^N/^P navigation?
	     */
	    clrbitn(VIEW_VIEW_HANDLE, scrollargs.keys.bitmap);
	    clrbitn(VIEW_PREV_HANDLE, scrollargs.keys.bitmap);
	    clrbitn(VIEW_NEXT_HANDLE, scrollargs.keys.bitmap);
	}

#ifdef	_WINDOWS
	scrollargs.mouse.popup = format_message_popup;
#endif

	if(((i = scrolltool(&scrollargs)) == MC_RESIZE
	    || (i == MC_FULLHDR && ps_global->full_header == 1))
	   && scrollargs.start.on == Offset)
	  offset = scrollargs.start.loc.offset;

	save_what = scrollargs.keys.what;
	ps_global->unseen_in_view = 0;
	so_give(&store);	/* free resources associated with store */
	free_handles(&handles);
#ifdef	_WINDOWS
	mswin_destroyicons();
#endif
    }
    while(ps->next_screen == SCREEN_FUN_NULL);

    if(we_cancel)
      cancel_busy_alarm(-1);

    /*
     * Unless we're going into attachment screen,
     * start over with  full_header.
     */
    if(F_OFF(F_QUELL_FULL_HDR_RESET, ps_global)
       && ps->next_screen != attachment_screen)
      ps->full_header = 0;
}



/*
 * view_writec_init - function to create and init struct that manages
 *		      writing to the display what we can as soon
 *		      as we can.
 */
void
view_writec_init(store, handlesp, first_line, last_line)
    STORE_S   *store;
    HANDLE_S **handlesp;
    int	       first_line;
    int	       last_line;
{
    char tmp[MAILTMPLEN];

    g_view_write = (struct view_write_s *)fs_get(sizeof(struct view_write_s));
    memset(g_view_write, 0, sizeof(struct view_write_s));
    g_view_write->store		   = store;
    g_view_write->handles	   = handlesp;
    g_view_write->screen_line	   = first_line;
    g_view_write->last_screen_line = last_line;

    if(!df_static_trigger(NULL, tmp)){
	g_view_write->line = (char *) fs_get(LINEBUFSIZ*sizeof(char));
#ifdef _WINDOWS
	mswin_beginupdate();
	scroll_setrange(0L, 0L);
#endif

	if(ps_global->VAR_DISPLAY_FILTERS)
	  ClearLines(first_line, last_line - 1);
    }
}



void
view_writec_destroy()
{
    if(g_view_write){
	if(g_view_write->line && g_view_write->index)
	  view_writec('\n');		/* flush pending output! */

	while(g_view_write->screen_line <= g_view_write->last_screen_line)
	  ClearLine(g_view_write->screen_line++);

	view_writec_killbuf();

	fs_give((void **) &g_view_write);
    }

#ifdef _WINDOWS
    mswin_endupdate();
#endif
}



void
view_writec_killbuf()
{
    if(g_view_write->line)
      fs_give((void **) &g_view_write->line);
}



/*
 * view_screen_pc - write chars into the final buffer
 */
int
view_writec(c)
    int c;
{
    static int in_color = 0;

    if(g_view_write->line){
	/*
	 * This only works if the 2nd and 3rd parts of the || don't happen.
	 * The only way it breaks is if we get a really, really long line
	 * because there are oodles of tags in it.  In that case we will
	 * wrap incorrectly or split a tag across lines (losing color perhaps)
	 * but we won't crash.
	 */
	if(c == '\n' ||
	   (!in_color &&
	    (char)c != TAG_EMBED &&
	    g_view_write->index >= (LINEBUFSIZ - 20 * (RGBLEN+2))) ||
	   (g_view_write->index >= LINEBUFSIZ - 1)){
	    int rv;

	    in_color = 0;
	    suspend_busy_alarm();
	    ClearLine(g_view_write->screen_line);
	    if(c != '\n')
	      g_view_write->line[g_view_write->index++] = (char) c;

	    PutLine0n8b(g_view_write->screen_line++, 0,
			g_view_write->line, g_view_write->index,
			g_view_write->handles ? *g_view_write->handles : NULL);

	    resume_busy_alarm(0);
	    rv = so_nputs(g_view_write->store,
			  g_view_write->line,
			  g_view_write->index);
	    g_view_write->index = 0;

	    if(g_view_write->screen_line >= g_view_write->last_screen_line){
		fs_give((void **) &g_view_write->line);
		fflush(stdout);
	    }

	    if(!rv)
	      return(0);
	    else if(c != '\n')
	      return(1);
	}
	else{
	    /*
	     * Don't split embedded things over multiple lines. Colors are
	     * the longest tags so use their length.
	     */
	    if((char)c == TAG_EMBED)
	      in_color = RGBLEN+1;
	    else if(in_color)
	      in_color--;

	    g_view_write->line[g_view_write->index++] = (char) c;
	    return(1);
	}
    }
#ifdef	_WINDOWS
    else if(c == '\n' && g_view_write->lines++ > SCROLL_LINES(ps_global))
      scroll_setrange(SCROLL_LINES(ps_global),
		      g_view_write->lines + SCROLL_LINES(ps_global));
#endif

    return(so_writec(c, g_view_write->store));
}


int
view_end_scroll(sparms)
    SCROLL_S *sparms;
{
    int done = 0, result, force;
    
    if(F_ON(F_ENABLE_SPACE_AS_TAB, ps_global)){

	if(F_ON(F_ENABLE_TAB_DELETES, ps_global)){
	    long save_msgno;

	    /* Let the TAB advance cur msgno for us */
	    save_msgno = mn_get_cur(ps_global->msgmap);
	    cmd_delete(ps_global, ps_global->msgmap, 0);
	    mn_set_cur(ps_global->msgmap, save_msgno);
	}

	/* act like the user hit a TAB */
	result = process_cmd(ps_global, ps_global->mail_stream,
			     ps_global->msgmap, MC_TAB, View, &force);

	if(result == 1)
	  done = 1;
    }

    return(done);
}




/*----------------------------------------------------------------------
    Add lines to the attachments structure
    
  Args: body   -- body of the part being described
        prefix -- The prefix for numbering the parts
        num    -- The number of this specific part
        should_show -- Flag indicating which of alternate parts should be shown
	multalt     -- Flag indicating the part is one of the multipart
			alternative parts (so suppress editorial comment)

Result: The ps_global->attachments data structure is filled in. This
is called recursively to descend through all the parts of a message. 
The description strings filled in are malloced and should be freed.

  ----*/
void
describe_mime(body, prefix, num, should_show, multalt)
    BODY *body;
    char *prefix;
    int   num, should_show;
{
    PART      *part;
    char       numx[512], string[200], *description;
    int        n, named = 0, can_display_ext;
    ATTACH_S  *a;

    if(!body)
      return;

    if(body->type == TYPEMULTIPART){
	int alt_to_show = 0;

        if(strucmp(body->subtype, "alternative") == 0){
	    int effort, best_effort = SHOW_NONE;

	    /*---- Figure out which alternative part to display ---*/
	    /*
	     * This is kind of complicated because some TEXT types
	     * are more displayable than others.  We don't want to
	     * commit to displaying a text-part alternative that we
	     * don't directly recognize unless that's all there is.
	     */
	    for(part=body->nested.part, n=1; part; part=part->next, n++)
	      if(F_ON(F_PREFER_PLAIN_TEXT, ps_global) &&
		 part->body.type == TYPETEXT &&
		 (!part->body.subtype || 
		  !strucmp(part->body.subtype, "PLAIN"))){
		if((effort = mime_show(&part->body)) != SHOW_ALL_EXT){
		  best_effort = effort;
		  alt_to_show = n;
		  break;
		}
	      }
	      else if((effort = mime_show(&part->body)) >= best_effort
		      && (part->body.type != TYPETEXT || mime_known_text_subtype(part->body.subtype))
		      && effort != SHOW_ALL_EXT){
		best_effort = effort;
		alt_to_show = n;
	      }
	      else if(part->body.type == TYPETEXT && alt_to_show == 0){
		  best_effort = effort;
		  alt_to_show = n;
	      }
	}
	else if(!strucmp(body->subtype, "digest")){
	    memset(a = next_attachment(), 0, sizeof(ATTACH_S));
	    if(*prefix){
		prefix[n = strlen(prefix) - 1] = '\0';
		a->number		       = cpystr(prefix);
		prefix[n] = '.';
	    }
	    else
	      a->number = cpystr("");

	    a->description     = cpystr("Multipart/Digest");
	    a->body	       = body;
	    a->can_display     = MCD_INTERNAL;
	    (a+1)->description = NULL;
	}
	else if(mailcap_can_display(body->type, body->subtype,
				    body->parameter, 0)
		|| (can_display_ext 
		    = mailcap_can_display(body->type, body->subtype,
					  body->parameter, 1))){
	    memset(a = next_attachment(), 0, sizeof(ATTACH_S));
	    if(*prefix){
		prefix[n = strlen(prefix) - 1] = '\0';
		a->number		       = cpystr(prefix);
		prefix[n] = '.';
	    }
	    else
	      a->number = cpystr("");

	    sprintf(string, "%s/%.*s", body_type_names(body->type),
		    sizeof(string) - 50, body->subtype);
	    a->description	   = cpystr(string);
	    a->body		   = body;
	    a->can_display	   = MCD_EXTERNAL;
	    if(can_display_ext)
	      a->can_display      |= MCD_EXT_PROMPT;
	    (a+1)->description	   = NULL;
	}

	for(part=body->nested.part, n=1; part; part=part->next, n++){
	    sprintf(numx, "%.*s%d.", sizeof(numx)-20, prefix, n);
	    /*
	     * Last arg to describe_mime here. If we have chosen one part
	     * of a multipart/alternative to display then we suppress
	     * the editorial messages on the other parts.
	     */
	    describe_mime(&(part->body),
			  (part->body.type == TYPEMULTIPART) ? numx : prefix,
			  n, (n == alt_to_show || !alt_to_show),
			  alt_to_show != 0);
	}
    }
    else{
	char tmp1[MAILTMPLEN], tmp2[MAILTMPLEN];

	format_mime_size((a = next_attachment())->size, body);

	a->suppress_editorial = (multalt != 0);

	sprintf(tmp1, "%.200s", body->description ? body->description : "");
	sprintf(tmp2, "%.200s", (!body->description && body->type == TYPEMESSAGE && body->encoding <= ENCBINARY && body->subtype && strucmp(body->subtype, "rfc822") == 0 && body->nested.msg->env && body->nested.msg->env->subject) ? body->nested.msg->env->subject : "");

	description = (body->description)
		        ? (char *) rfc1522_decode((unsigned char *)tmp_20k_buf,
						  SIZEOF_20KBUF, tmp1, NULL)
			: (body->type == TYPEMESSAGE
			   && body->encoding <= ENCBINARY
			   && body->subtype
			   && strucmp(body->subtype, "rfc822") == 0
			   && body->nested.msg->env
			   && body->nested.msg->env->subject)
			   ? (char *) rfc1522_decode((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, tmp2, NULL)
			   : (body->type == TYPEMESSAGE
			      && body->subtype
			      && !strucmp(body->subtype, "delivery-status"))
				? "Delivery Status"
				: NULL;

	description = istrncpy((char *)(tmp_20k_buf+1000), description, 1000);
        sprintf(string, "%s%s%s%s",
                strsquish(tmp_20k_buf+3000,
			  type_desc(body->type,body->subtype,body->parameter,0),
			  sizeof(string)/2 - 10),
                description ? ", \"" : "",
                description ? strsquish(tmp_20k_buf+4000, description,
					sizeof(string)/2 - 10)
			    : "",
                description ? "\"": "");

        a->description = cpystr(string);
        a->body        = body;

	if(body->disposition.type){
	    named = strucmp(body->disposition.type, "inline");
	}
	else{
	    char *value;


	    /*
	     * This test remains for backward compatibility
	     */
	    if(value = body_parameter(body, "name")){
		named = strucmp(value, "Message Body");
		fs_give((void **) &value);
	    }
	}

	/*
	 * Make sure we have the tools available to display the
	 * type/subtype, *AND* that we can decode it if needed.
	 * Of course, if it's text, we display it anyway in the
	 * mail_view_screen so put off testing mailcap until we're
	 * explicitly asked to display that segment 'cause it could
	 * be expensive to test...
	 */
	if((body->type == TYPETEXT && !named)
	   || MIME_VCARD(body->type,body->subtype)){
	    a->test_deferred = 1;
	    a->can_display = MCD_INTERNAL;
	}
	else{
	    a->test_deferred = 0;
	    a->can_display = mime_can_display(body->type, body->subtype,
					      body->parameter);
	}

	/*
	 * Deferred means we can display it
	 */
	a->shown = ((a->can_display & MCD_INTERNAL)
		    && !MIME_VCARD(body->type,body->subtype)
		    && (!named || multalt
			|| (body->type == TYPETEXT && num == 1
			    && !(*prefix && strcmp(prefix,"1."))))
		    && (body->type != TYPEMESSAGE
			|| (body->type == TYPEMESSAGE
			    && body->encoding <= ENCBINARY))
		    && should_show);
	a->number = (char *)fs_get((strlen(prefix) + 16)* sizeof(char));
        sprintf(a->number, "%s%d",prefix, num);
        (a+1)->description = NULL;
        if(body->type == TYPEMESSAGE && body->encoding <= ENCBINARY
	   && body->subtype && strucmp(body->subtype, "rfc822") == 0){
	    body = body->nested.msg->body;
	    sprintf(numx, "%.*s%d.", sizeof(numx)-20, prefix, num);
	    describe_mime(body, numx, 1, should_show, 0);
        }
    }
}


int
mime_known_text_subtype(subtype)
    char *subtype;
{
    char **p;
    static char *known_types[] = {
	"plain",
	"html",
	"enriched",
	"richtext",
	NULL
    };

    if(!(subtype && *subtype))
      return(1);

    for(p = known_types; *p; p++)
      if(!strucmp(subtype, *p))
	return(1);
    return(0);
}


char *
body_parameter(body, attribute)
    BODY *body;
    char *attribute;
{
    return(rfc2231_get_param(body->parameter, attribute, NULL, NULL));
}



/*----------------------------------------------------------------------
  Return a pointer to the next attachment struct
    
  Args: none

  ----*/
ATTACH_S *
next_attachment()
{
    ATTACH_S *a;
    int       n;

    for(a = ps_global->atmts; a->description; a++)
      ;

    if((n = a - ps_global->atmts) + 1 >= ps_global->atmts_allocated){
	ps_global->atmts_allocated *= 2;
	fs_resize((void **)&ps_global->atmts,
		  ps_global->atmts_allocated * sizeof(ATTACH_S));
	a = &ps_global->atmts[n];
    }

    return(a);
}



/*----------------------------------------------------------------------
   Zero out the attachments structure and free up storage
  ----*/
void
zero_atmts(atmts)
     ATTACH_S *atmts;
{
    ATTACH_S *a;

    for(a = atmts; a->description != NULL; a++){
	fs_give((void **)&(a->description));
	fs_give((void **)&(a->number));
    }

    atmts->description = NULL;
}


char *
body_type_names(t)
    int t;
{
#define TLEN 31
    static char body_type[TLEN + 1];
    char   *p;

    body_type[0] = '\0';
    strncpy(body_type,				/* copy the given type */
	    (t > -1 && t < TYPEMAX && body_types[t])
	      ? body_types[t] : "Other", TLEN);
    body_type[TLEN] = '\0';

    for(p = body_type + 1; *p; p++)		/* make it presentable */
      if(isupper((unsigned char)*p))
	*p = tolower((unsigned char)(*p));

    return(body_type);				/* present it */
}


/*----------------------------------------------------------------------
  Mapping table use to neatly display charset parameters
 ----*/

static struct set_names {
    char *rfcname,
	 *humanname;
} charset_names[] = {
    {"US-ASCII",		"Plain Text"},
    {"ISO-8859-1",		"Latin 1 (Western Europe)"},
    {"ISO-8859-2",		"Latin 2 (Eastern Europe)"},
    {"ISO-8859-3",		"Latin 3 (Southern Europe)"},
    {"ISO-8859-4",		"Latin 4 (Northern Europe)"},
    {"ISO-8859-5",		"Latin & Cyrillic"},
    {"ISO-8859-6",		"Latin & Arabic"},
    {"ISO-8859-7",		"Latin & Greek"},
    {"ISO-8859-8",		"Latin & Hebrew"},
    {"ISO-8859-9",		"Latin 5 (Turkish)"},
    {"ISO-8859-10",		"Latin 6 (Nordic)"},
    {"ISO-8859-11",		"Latin & Thai"},
    {"ISO-8859-13",		"Latin 7 (Baltic)"},
    {"ISO-8859-14",		"Latin 8 (Celtic)"},
    {"ISO-8859-15",		"Latin 9 (Euro)"},
    {"KOI8-R",			"Latin & Russian"},
    {"KOI8-U",			"Latin & Ukranian"},
    {"VISCII",			"Latin & Vietnamese"},
    {"GB2312",			"Latin & Simplified Chinese"},
    {"BIG5",			"Latin & Traditional Chinese"},
    {"EUC-JP",			"Latin & Japanese"},
    {"Shift-JIS",		"Latin & Japanese"},
    {"Shift_JIS",		"Latin & Japanese"},
    {"EUC-KR",			"Latin & Korean"},
    {"ISO-2022-CN",		"Latin & Chinese"},
    {"ISO-2022-JP",		"Latin & Japanese"},
    {"ISO-2022-KR",		"Latin & Korean"},
    {"UTF-7",			"7-bit encoded Unicode"},
    {"UTF-8",			"Internet-standard Unicode"},
    {"ISO-2022-JP-2",		"Multilingual"},
    {NULL,			NULL}
};


/*----------------------------------------------------------------------
  Return a nicely formatted discription of the type of the part
 ----*/

char *
type_desc(type, subtype, params, full)
     int type, full;
     char *subtype;
     PARAMETER *params;
{
    static char  type_d[200];
    int		 i;
    char	*p, *parmval;

    p = type_d;
    sstrcpy(&p, body_type_names(type));
    if(full && subtype){
	*p++ = '/';
	sstrcpy(&p, strsquish(tmp_20k_buf + 500, subtype, 30));
    }

    switch(type) {
      case TYPETEXT:
	parmval = rfc2231_get_param(params, "charset", NULL, NULL);

        if(parmval){
	    for(i = 0; charset_names[i].rfcname; i++)
	      if(!strucmp(parmval, charset_names[i].rfcname)){
		  if(!strucmp(parmval, ps_global->VAR_CHAR_SET
			      ? ps_global->VAR_CHAR_SET : "us-ascii")
		     || !strucmp(parmval, "us-ascii"))
		    i = -1;

		  break;
	      }

	    if(i >= 0){			/* charset to write */
		if(charset_names[i].rfcname){
		    sstrcpy(&p, " (charset: ");
		    sstrcpy(&p, charset_names[i].rfcname
				  ? charset_names[i].rfcname : "Unknown");
		    if(full){
			sstrcpy(&p, " \"");
			sstrcpy(&p, charset_names[i].humanname
				? charset_names[i].humanname
				: strsquish(tmp_20k_buf + 500, parmval, 40));
			*p++ = '\"';
		    }

		    sstrcpy(&p, ")");
		}
		else{
		    sstrcpy(&p, " (charset: ");
		    sstrcpy(&p, strsquish(tmp_20k_buf + 500, parmval, 40));
		    sstrcpy(&p, ")");
		}
	    }

	    fs_give((void **) &parmval);
        }

	if(full && (parmval = rfc2231_get_param(params, "name", NULL, NULL))){
	    sprintf(p, " (Name: \"%s\")",
		    strsquish(tmp_20k_buf + 500, parmval, 80));
	    fs_give((void **) &parmval);
	}

        break;

      case TYPEAPPLICATION:
        if(full && subtype && strucmp(subtype, "octet-stream") == 0)
	  if(parmval = rfc2231_get_param(params, "name", NULL, NULL)){
	      sprintf(p, " (Name: \"%s\")",
		      strsquish(tmp_20k_buf + 500, parmval, 80));
	      fs_give((void **) &parmval);
	  }

        break;

      case TYPEMESSAGE:
	if(full && subtype && strucmp(subtype, "external-body") == 0)
	  if(parmval = rfc2231_get_param(params, "access-type", NULL, NULL)){
	      sprintf(p, " (%s%s)", full ? "Access: " : "",
		      strsquish(tmp_20k_buf + 500, parmval, 80));
	      fs_give((void **) &parmval);
	  }

	break;

      default:
        break;
    }

    return(type_d);
}
     

void
format_mime_size(string, b)
     char *string;
     BODY *b;
{
    char tmp[10], *p = NULL;

    *string++ = ' ';
    switch(b->encoding){
      case ENCBASE64 :
	if(b->type == TYPETEXT)
	  *(string-1) = '~';

	strcpy(p = string, byte_string((3 * b->size.bytes) / 4));
	break;

      default :
      case ENCQUOTEDPRINTABLE :
	*(string-1) = '~';

      case ENC8BIT :
      case ENC7BIT :
	if(b->type == TYPETEXT)
	  sprintf(string, "%s lines", comatose(b->size.lines));
	else
	  strcpy(p = string, byte_string(b->size.bytes));

	break;
    }


    if(p){
	for(; *p && (isdigit((unsigned char)*p)
		     || ispunct((unsigned char)*p)); p++)
	  ;

	sprintf(tmp, " %-5.5s",p);
	strcpy(p, tmp);
    }
}

        

/*----------------------------------------------------------------------
   Determine if we can show all, some or none of the parts of a body

Args: body --- The message body to check

Returns: SHOW_ALL, SHOW_ALL_EXT, SHOW_PART or SHOW_NONE depending on
	 how much of the body can be shown and who can show it.
 ----*/     
int
mime_show(body)
     BODY *body;
{
    int   effort, best_effort;
    PART *p;

    if(!body)
      return(SHOW_NONE);

    switch(body->type) {
      case TYPEMESSAGE:
	if(!strucmp(body->subtype, "rfc822"))
	  return(mime_show(body->nested.msg->body) == SHOW_ALL
		 ? SHOW_ALL: SHOW_PARTS);
	/* else fall thru to default case... */

      default:
	/*
	 * Since we're testing for internal displayability, give the
	 * internal result over an external viewer
	 */
	effort = mime_can_display(body->type, body->subtype, body->parameter);
	if(effort == MCD_NONE)
	  return(SHOW_NONE);
	else if(effort & MCD_INTERNAL)
	  return(SHOW_ALL);
	else
	  return(SHOW_ALL_EXT);

      case TYPEMULTIPART:
	best_effort = SHOW_NONE;
        for(p = body->nested.part; p; p = p->next)
	  if((effort = mime_show(&p->body)) > best_effort)
	    best_effort = effort;

	return(best_effort);
    }
}



/*
 * format_size_guess -- Run down the given body summing the text/plain
 *			pieces we're likely to display.  It need only
 *			be a guess since this is intended for preallocating
 *			our display buffer...
 */
long
format_size_guess(body)
    BODY *body;
{
    long  size = 0L;
    long  extra = 0L;
    char *free_me = NULL;

    if(body){
	if(body->type == TYPEMULTIPART){
	    PART *part;

	    for(part = body->nested.part; part; part = part->next)
	      size += format_size_guess(&part->body);
	}
	else if(body->type == TYPEMESSAGE
		&& body->subtype && !strucmp(body->subtype, "rfc822"))
	  size = format_size_guess(body->nested.msg->body);
	else if((!body->type || body->type == TYPETEXT)
		&& (!body->subtype || !strucmp(body->subtype, "plain"))
		&& ((body->disposition.type
		     && !strucmp(body->disposition.type, "inline"))
		    || !(free_me = body_parameter(body, "name")))){
	    /*
	     * Handles and colored quotes cause memory overhead. Figure about
	     * 100 bytes per level of quote per line and about 100 bytes per
	     * handle. Make a guess of 1 handle or colored quote per
	     * 10 lines of message. If we guess too low, we'll do a resize in
	     * so_cs_writec. Most of the overhead comes from the colors, so
	     * if we can see we don't do colors or don't have these features
	     * turned on, skip it.
	     */
	    if(pico_usingcolor() &&
	       ((ps_global->VAR_QUOTE1_FORE_COLOR &&
	         ps_global->VAR_QUOTE1_BACK_COLOR) ||
	        F_ON(F_VIEW_SEL_URL, ps_global) ||
	        F_ON(F_VIEW_SEL_URL_HOST, ps_global) ||
	        F_ON(F_SCAN_ADDR, ps_global)))
	      extra = min(100/10 * body->size.lines, MAX_FUDGE);

	    size = body->size.bytes + extra;
	}

	if(free_me)
	  fs_give((void **) &free_me);
    }

    return(size);
}


/*
 * fcc_size_guess
 */
long
fcc_size_guess(body)
    BODY *body;
{
    long  size = 0L;

    if(body){
	if(body->type == TYPEMULTIPART){
	    PART *part;

	    for(part = body->nested.part; part; part = part->next)
	      size += fcc_size_guess(&part->body);
	}
	else{
	    size = body->size.bytes;
	    /*
	     * If it is ENCBINARY we will be base64 encoding it. This
	     * ideally increases the size by a factor of 4/3, but there
	     * is a per-line increase in that because of the CRLFs and
	     * because the number of characters in the line might not
	     * be a factor of 3. So push it up by 3/2 instead. This still
	     * won't catch all the cases. In particular, attachements with
	     * lots of short lines (< 10) will expand by more than that,
	     * but that's ok since this is an optimization. That's why
	     * so_cs_puts uses the 3/2 factor when it does a resize, so
	     * that it won't have to resize linearly until it gets there.
	     */
	    if(body->encoding == ENCBINARY)
	      size = 3*size/2;
	}
    }

    return(size);
}
        


/*----------------------------------------------------------------------
   Format a message message for viewing

 Args: msgno -- The number of the message to view
       env   -- pointer to the message's envelope
       body  -- pointer to the message's body
       flgs  -- possible flags:
		FM_DISPLAY -- Indicates formatted text is bound for
			      the display (as opposed to the printer,
			      for export, etc)
                FM_NEW_MESS -- flag indicating a different message being
			       formatted than was formatted last time 
			       function was called

Result: Returns true if no problems encountered, else false.

First the envelope is formatted; next a list of all attachments is
formatted if there is more than one. Then all the body parts are
formatted, fetching them as needed. This includes headers of included
message. Richtext is also formatted. An entry is made in the text for
parts that are not displayed or can't be displayed.  source indicates 
how and where the caller would like to have the text formatted.

 ----*/    
int
format_message(msgno, env, body, handlesp, flgs, pc)
    long         msgno;
    ENVELOPE    *env;
    BODY        *body;
    HANDLE_S   **handlesp;
    int          flgs;
    gf_io_t      pc;
{
    char     *decode_err = NULL, *tmp1, *description;
    HEADER_S  h;
    ATTACH_S *a;
    int       show_parts, error_found = 0, width;
    int       is_in_sig = OUT_SIG_BLOCK;
    int       filt_only_c0 = 0, wrapflags;
    gf_io_t   gc;

    clear_cur_embedded_color();

    width = (flgs & FM_DISPLAY) ? ps_global->ttyo->screen_cols : 80;

    /*---- format and copy envelope ----*/
    if(ps_global->full_header == 1)
      q_status_message(SM_INFO, 0, 3, "All quoted text being included");
    else if(ps_global->full_header == 2)
      q_status_message(SM_INFO, 0, 3,
		       "Full header mode ON.  All header text being included");

    HD_INIT(&h, ps_global->VAR_VIEW_HEADERS, ps_global->view_all_except,
	    FE_DEFAULT);
    switch(format_header(ps_global->mail_stream, msgno, NULL,
			 env, &h, NULL, handlesp, flgs, pc)){
			      
      case -1 :			/* write error */
	goto write_error;

      case 1 :			/* fetch error */
	if(!(gf_puts("[ Error fetching header ]",  pc)
	     && !gf_puts(NEWLINE, pc)))
	  goto write_error;

	break;
    }

    if(body == NULL 
       || (ps_global->full_header == 2
	   && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))) {
        /*--- Server is not an IMAP2bis, It can't parse MIME
              so we just show the text here. Hopefully the 
              message isn't a MIME message 
          ---*/
	void *text2;
#if	defined(DOS) && !defined(WIN32)
	char *append_file_name;

	/* for now, always fetch to disk.  This could be tuned to
	 * check for message size, then decide to deal with it on disk...
	 */
	mail_parameters(ps_global->mail_stream, SET_GETS, (void *)dos_gets);
	if(!(append_file_name = temp_nam(NULL, "pv"))
	   || !(append_file = fopen(append_file_name, "w+b"))){
	    if(append_file_name){
		(void)unlink(append_file_name);
		fs_give((void **)&append_file_name);
	    }

	    q_status_message1(SM_ORDER,3,3,"Can't make temp file: %.200s",
			      error_description(errno));
	    return(0);
	}
#endif

        if(text2 = (void *)pine_mail_fetch_text(ps_global->mail_stream,
						msgno, NULL, NULL, NIL)){

 	    if(!gf_puts(NEWLINE, pc))		/* write delimiter */
	      goto write_error;
#if	defined(DOS) && !defined(WIN32)
	    gf_set_readc(&gc, append_file, 0L, FileStar);
#else
	    gf_set_readc(&gc, text2, (unsigned long)strlen(text2), CharStar);
#endif
	    gf_filter_init();
	    /* link in filters, similar to what is done in decode_text() */
	    if(!ps_global->pass_ctrl_chars){
		/* gf_link_filter(gf_escape_filter, NULL); */
		filt_only_c0 = ps_global->pass_c1_ctrl_chars ? 1 : 0;
		gf_link_filter(gf_control_filter,
			       gf_control_filter_opt(&filt_only_c0));
	    }
	    gf_link_filter(gf_tag_filter, NULL);

	    if((F_ON(F_VIEW_SEL_URL, ps_global)
		|| F_ON(F_VIEW_SEL_URL_HOST, ps_global)
		|| F_ON(F_SCAN_ADDR, ps_global))
	       && handlesp){
		gf_link_filter(gf_line_test, gf_line_test_opt(url_hilite, handlesp));
	    }

	    if((flgs & FM_DISPLAY)
	       && !(flgs & FM_NOCOLOR)
	       && pico_usingcolor()
	       && ps_global->VAR_SIGNATURE_FORE_COLOR
	       && ps_global->VAR_SIGNATURE_BACK_COLOR){
		gf_link_filter(gf_line_test, gf_line_test_opt(color_signature, &is_in_sig));
	    }

	    if((flgs & FM_DISPLAY)
	       && !(flgs & FM_NOCOLOR)
	       && pico_usingcolor()
	       && ps_global->VAR_QUOTE1_FORE_COLOR
	       && ps_global->VAR_QUOTE1_BACK_COLOR){
		gf_link_filter(gf_line_test, gf_line_test_opt(color_a_quote, NULL));
	    }

	    wrapflags = (flgs & FM_DISPLAY) ? GFW_HANDLES : 0;
	    if(flgs & FM_DISPLAY
	       && !(flgs & FM_NOCOLOR)
	       && pico_usingcolor())
	      wrapflags |= GFW_USECOLOR;
	    gf_link_filter(gf_wrap, gf_wrap_filter_opt(ps_global->ttyo->screen_cols,
						       ps_global->ttyo->screen_cols,
						       format_view_margin(),
						       0,
						       wrapflags));
	    gf_link_filter(gf_nvtnl_local, NULL);
	    if(decode_err = gf_pipe(gc, pc)){
                sprintf(tmp_20k_buf, "Formatting error: %s", decode_err);
		if(gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)
		   && !format_editorial(tmp_20k_buf, width, pc)
		   && gf_puts(NEWLINE, pc))
		  decode_err = NULL;
		else
		  goto write_error;
	    }
	}

#if	defined(DOS) && !defined(WIN32)
	fclose(append_file);			/* clean up tmp file */
	append_file = NULL;
	unlink(append_file_name);
	fs_give((void **)&append_file_name);
	mail_gc(ps_global->mail_stream, GC_TEXTS);
	mail_parameters(ps_global->mail_stream, SET_GETS, (void *)NULL);
#endif

	if(!text2){
	    if(!gf_puts(NEWLINE, pc)
	       || !gf_puts("    [ERROR fetching text of message]", pc)
	       || !gf_puts(NEWLINE, pc)
	       || !gf_puts(NEWLINE, pc))
	      goto write_error;
	}

	return(1);
    }

    if(flgs & FM_NEW_MESS) {
	zero_atmts(ps_global->atmts);
	describe_mime(body, "", 1, 1, 0);
    }

    /*=========== Format the header into the buffer =========*/
    /*----- First do the list of parts/attachments if needed ----*/
    if((flgs & FM_DISPLAY) && ps_global->atmts[1].description){
	char tmp[MAX_SCREEN_COLS + 1], margin_str[MAX_SCREEN_COLS + 1], *tmpp;
	int  n, max_num_l = 0, size_offset = 0, *margin;

	/*
	 * can't pop margin_str since it's max_screen and view_margin()
	 * can be only a fraction of screen_cols we've already checked
	 * is less than #define MAX_SCREEN_COLS, right?
	 */
	margin = format_view_margin();
	strncpy(margin_str, repeat_char(margin[0], ' '), sizeof(margin_str)-1);
	margin_str[sizeof(margin_str)-1] = '\0';

	if(!(gf_puts(margin_str, pc) && gf_puts("Parts/Attachments:", pc) && gf_puts(NEWLINE, pc)))
	  goto write_error;

	/*----- Figure max display lengths -----*/
        for(a = ps_global->atmts; a->description != NULL; a++) {
	    if((n = strlen(a->number)) > max_num_l)
	      max_num_l = n;

	    if((n = strlen(a->size)) > size_offset)
	      size_offset = n;
	}

	/*----- adjust max lengths for nice display -----*/
	max_num_l = min(max_num_l, (ps_global->ttyo->screen_cols/3));
	size_offset   += max_num_l + 12;

	/*----- Format the list of attachments -----*/
	for(a = ps_global->atmts; a->description != NULL; a++) {
	    COLOR_PAIR *lastc = NULL;

	    if(!gf_puts(margin_str, pc))
	      goto write_error;

	    memset(tmp, ' ', sizeof(tmp)-1);
	    tmp[sizeof(tmp)-1] = '\0';

	    if(msgno_part_deleted(ps_global->mail_stream, msgno, a->number))
	       tmp[1] = 'D';

	    if((n = strlen(a->number)) > max_num_l){
		strcpy(tmp + 3, "...");
		strncpy(tmp + 6, &a->number[n - (max_num_l - 3)],
			max_num_l - 3);
	    }
	    else
	      strncpy(tmp + 3, a->number, n);

	    if(a->shown)
	      strncpy(tmp + max_num_l + 4, "Shown", 5);
	    else if(a->can_display != MCD_NONE
		    && !(a->can_display & MCD_EXT_PROMPT))
	      strncpy(tmp + max_num_l + 4, "  OK ", 5);

	    if(n = strlen(a->size))
	      strncpy(tmp + size_offset - n, a->size, n);

	    if((n = ps_global->ttyo->screen_cols - (size_offset + 4)) > 0)
	      strncpy(tmp + size_offset + 2, a->description, n);

	    tmp[ps_global->ttyo->screen_cols - (margin[0] + margin[1])] = '\0';

	    if(F_ON(F_VIEW_SEL_ATTACH, ps_global) && handlesp){
		char      buf[16], color[64];
		int	  l;
		HANDLE_S *h;

		for(tmpp = tmp; *tmpp && *tmpp == ' '; tmpp++)
		  if(!(*pc)(' '))
		    goto write_error;

		h	    = new_handle(handlesp);
		h->type	    = Attach;
		h->h.attach = a;

		sprintf(buf, "%d", h->key);

		if(!(flgs & FM_NOCOLOR)
		   && scroll_handle_start_color(color, &l)){
		    lastc = get_cur_embedded_color();
		    if(!gf_nputs(color, (long) l, pc))
		       goto write_error;
		}
		else if(!((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDON)))
		  goto write_error;

		if(!((*pc)(TAG_EMBED) && (*pc)(TAG_HANDLE)
		     && (*pc)(strlen(buf)) && gf_puts(buf, pc)))
		  goto write_error;
	    }
	    else
	      tmpp = tmp;

	    if(!format_env_puts(tmpp, pc))
	      goto write_error;

	    if(F_ON(F_VIEW_SEL_ATTACH, ps_global) && handlesp){
		if(lastc){
		    if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
			if(!((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDOFF)))
			  goto write_error;
		    }

		    embed_color(lastc, pc);
		    free_color_pair(&lastc);
		}
		else if(!((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDOFF)))
		  goto write_error;

		if(!((*pc)(TAG_EMBED) && (*pc)(TAG_INVOFF)))
		  goto write_error;
	    }

	    if(!gf_puts(NEWLINE, pc))
	      goto write_error;
        }

	if(!(gf_puts(margin_str, pc)
	     && gf_puts(repeat_char(min((ps_global->ttyo->screen_cols - (margin[0] + margin[1])), 40),'-'), pc)
	     && gf_puts(NEWLINE, pc)))
	  goto write_error;
    }

    if(!gf_puts(NEWLINE, pc))		/* write delimiter */
      goto write_error;

    show_parts = 0;

    /*======== Now loop through formatting all the parts =======*/
    for(a = ps_global->atmts; a->description != NULL; a++) {

	if(a->body->type == TYPEMULTIPART)
	  continue;

        if(!a->shown) {
	    if(a->suppress_editorial)
	      continue;

	    if((decode_err = part_desc(a->number, a->body,
				       (flgs & FM_DISPLAY)
				         ? (a->can_display != MCD_NONE)
					     ? 1 : 2
				         : 3, width, pc))
	       || !gf_puts(NEWLINE, pc))
	      goto write_error;

            continue;
        } 

        switch(a->body->type){

          case TYPETEXT:
	    /*
	     * If a message is multipart *and* the first part of it
	     * is text *and that text is empty, there is a good chance that
	     * there was actually something there that c-client was
	     * unable to parse.  Here we report the empty message body
	     * and insert the raw RFC822.TEXT (if full-headers are
	     * on).
	     */
	    if(body->type == TYPEMULTIPART
	       && a == ps_global->atmts
	       && a->body->size.bytes == 0
	       && F_ON(F_ENABLE_FULL_HDR, ps_global)){
		char *err = NULL;

		sprintf(tmp_20k_buf, 
			"Empty or malformed message%s.",
			ps_global->full_header == 2 
			    ? ". Displaying raw text"
			    : ". Use \"H\" to see raw text");

		if(!(gf_puts(NEWLINE, pc)
		     && !format_editorial(tmp_20k_buf, width, pc)
		     && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
		  goto write_error;

		if(ps_global->full_header == 2
		   && (err = detach_raw(ps_global->mail_stream, msgno,
					a->number, pc, flgs))){
		    sprintf(tmp_20k_buf, 
			    "%s  [ Formatting error: %s ]%s%s",
			    NEWLINE, err, NEWLINE, NEWLINE);
		    if(!gf_puts(tmp_20k_buf, pc))
		      goto write_error;
		}

		break;
	    }

	    /*
	     * Don't write our delimiter if this text part is
	     * the first part of a message/rfc822 segment...
	     */
	    if(show_parts && a != ps_global->atmts 
	       && a[-1].body && a[-1].body->type != TYPEMESSAGE){
		tmp1 = a->body->description ? a->body->description
					      : "Attached Text";
	    	description = istrncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode((unsigned char *)(tmp_20k_buf+15000), 5000, tmp1, NULL), 5000);
		
		sprintf(tmp_20k_buf, "Part %s: \"%.1024s\"", a->number,
			description);
		if(!(gf_puts(NEWLINE, pc)
		     && !format_editorial(tmp_20k_buf, width, pc)
		     && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
		  goto write_error;
	    }

	    error_found += decode_text(a, msgno, pc, handlesp,
				       (flgs & FM_DISPLAY) ? InLine : QStatus,
				       flgs);
            break;

          case TYPEMESSAGE:
	    tmp1 = a->body->description ? a->body->description
		      : (strucmp(a->body->subtype, "delivery-status") == 0)
		          ? "Delivery Status"
			  : "Included Message";
	    description = istrncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode((unsigned char *)(tmp_20k_buf+15000), 5000, tmp1, NULL), 5000);
	    
            sprintf(tmp_20k_buf, "Part %s: \"%.1024s\"", a->number,
		    description);

	    if(!(gf_puts(NEWLINE, pc)
		 && !format_editorial(tmp_20k_buf,
				      ps_global->ttyo->screen_cols, pc)
		 && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
	      goto write_error;

	    if(a->body->subtype && strucmp(a->body->subtype, "rfc822") == 0){
		/* imapenvonly, we may not have all the headers we need */
		if(a->body->nested.msg->env->imapenvonly)
		  mail_fetch_header(ps_global->mail_stream, msgno,
				    a->number, NULL, NULL, FT_PEEK);
		switch(format_header(ps_global->mail_stream, msgno, a->number,
				     a->body->nested.msg->env, &h,
				     NULL, handlesp, flgs, pc)){
		  case -1 :			/* write error */
		    goto write_error;

		  case 1 :			/* fetch error */
		    if(!(gf_puts("[ Error fetching header ]",  pc)
			 && !gf_puts(NEWLINE, pc)))
		      goto write_error;

		    break;
		}
	    }
            else if(a->body->subtype 
		    && strucmp(a->body->subtype, "external-body") == 0) {
		char es[512];
		int *m = format_view_margin();
		int c = ps_global->ttyo->screen_cols;

		c -= (m != NULL) ? m[0] : 0;
		c -= (m != NULL) ? m[1] : 0;
		if(format_editorial("This part is not included and can be fetched as follows:", c, pc)
		   || !gf_puts(NEWLINE, pc)
		   || format_editorial(display_parameters(a->body->parameter), c, pc))
		  goto write_error;
            }
	    else
	      error_found += decode_text(a, msgno, pc, handlesp,
					 (flgs&FM_DISPLAY) ? InLine : QStatus,
					 flgs);

	    if(!gf_puts(NEWLINE, pc))
	      goto write_error;

            break;

          default:
	    if(decode_err = part_desc(a->number, a->body,
				      (flgs & FM_DISPLAY) ? 1 : 3,
				      width, pc))
	      goto write_error;
        }

	show_parts++;
    }

    return(!error_found
	   && rfc2369_editorial(msgno, handlesp, flgs, width, pc)
	   && format_blip_seen(msgno));


  write_error:

    if(!(flgs & FM_DISPLAY))
      q_status_message1(SM_ORDER, 3, 4, "Error writing message: %.200s",
			decode_err ? decode_err : error_description(errno));

    return(0);
}


char *
format_editorial(s, width, pc)
    char    *s;
    int	     width;
    gf_io_t  pc;
{
    gf_io_t   gc;
    char     *t;
    size_t    n, len;
    int      *margin;
    unsigned char *p, *tmp = NULL;
    EDITORIAL_S es;
 
    /*
     * Warning. Some callers of this routine use the first half
     * of tmp_20k_buf to store the passed in source string, out to as
     * many as 10000 characters. However, we don't need to preserve the
     * source string after we have copied it.
     */
    if((n = strlen(s)) > SIZEOF_20KBUF-10000-1){
	len = n+1;
	p = tmp = (unsigned char *)fs_get(len * sizeof(unsigned char));
    }
    else{
	len = SIZEOF_20KBUF-10000;
	p = (unsigned char *)(tmp_20k_buf+10000);
    }

    t = (char *)rfc1522_decode(p, len, s, NULL);

    gf_set_readc(&gc, t, strlen(t), CharStar);

    if(tmp)
      fs_give((void **)&tmp);

    margin = format_view_margin();
    width -= (margin[0] + margin[1]);

    if(width > 40){
	width -= 12;

	es.prelen = max(2, min(margin[0] + 6, EDITORIAL_MAX - 3));
	sprintf(es.prefix, "%s[ ", repeat_char(es.prelen - 2, ' '));
	es.postlen = 2;
	strcpy(es.postfix, " ]");
    }
    else if(width > 20){
	width -= 6;

	es.prelen = max(2, min(margin[0] + 3, EDITORIAL_MAX - 3));
	sprintf(es.prefix, "%s[ ", repeat_char(es.prelen - 2, ' '));
	es.postlen = 2;
	strcpy(es.postfix, " ]");
    }
    else{
	width -= 2;
	strcpy(es.prefix, "[");
	strcpy(es.postfix, "]");
	es.prelen = 1;
	es.postlen = 1;
    }

    gf_filter_init();
    gf_link_filter(gf_wrap, gf_wrap_filter_opt(width, width,
					       NULL, 0, GFW_HANDLES));
    gf_link_filter(gf_line_test, gf_line_test_opt(quote_editorial, &es));
    return(gf_pipe(gc, pc));
}


int
quote_editorial(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    ins = gf_line_test_new_ins(ins, line,
			       ((EDITORIAL_S *)local)->prefix,
			       ((EDITORIAL_S *)local)->prelen);
    ins = gf_line_test_new_ins(ins, line + strlen(line),
			       ((EDITORIAL_S *)local)->postfix,
			       ((EDITORIAL_S *)local)->postlen);
    return(0);
}



/*
 * Format editorial comment about charset mismatch
 */
int
charset_editorial(charset, msgno, handlesp, flags, quality, width, pc)
    char      *charset;
    long       msgno;
    HANDLE_S **handlesp;
    int	       flags;
    int        quality;
    int	       width;
    gf_io_t    pc;
{
    char     *p, color[64], buf[2048];
    int	      i, n;
    HANDLE_S *h = NULL;

    sprintf(buf, CHARSET_DISCLAIMER_1, charset ? charset : "US-ASCII");
    p = &buf[strlen(buf)];

    if(!(ps_global->VAR_CHAR_SET
	 && strucmp(ps_global->VAR_CHAR_SET, "US-ASCII"))
       && handlesp
       && (flags & FM_DISPLAY) == FM_DISPLAY){
	h		      = new_handle(handlesp);
	h->type		      = URL;
	h->h.url.path	      = cpystr("x-pine-help:h_config_char_set");

	/*
	 * Because this filter comes after the delete_quotes filter, we need
	 * to tell delete_quotes that this handle is used, so it won't
	 * delete it. This is a dangerous thing.
	 */
	h->is_used = 1; 

	if(!(flags & FM_NOCOLOR)
	   && scroll_handle_start_color(color, &n)){
	    for(i = 0; i < n; i++)
	      *p++ = color[i];
	}
	else{
	    *p++ = TAG_EMBED;
	    *p++ = TAG_BOLDON;
	}

	*p++ = TAG_EMBED;
	*p++ = TAG_HANDLE;
	sprintf(p + 1, "%d", h->key);
	*p  = strlen(p + 1);
	p  += (*p + 1);
    }

    sstrcpy(&p, CHARSET_DISCLAIMER_2);

    if(h){
	/* in case it was the current selection */
	*p++ = TAG_EMBED;
	*p++ = TAG_INVOFF;

	if(scroll_handle_end_color(color, &n)){
	    for(i = 0; i < n; i++)
	      *p++ = color[i];
	}
	else{
	    *p++ = TAG_EMBED;
	    *p++ = TAG_BOLDOFF;
	}

	*p = '\0';
    }

    sprintf(p, CHARSET_DISCLAIMER_3,
	    ps_global->VAR_CHAR_SET ? ps_global->VAR_CHAR_SET : "US-ASCII",
	    (quality == CV_LOSES_SPECIAL_CHARS) ? "special " : "");

    return(format_editorial(buf, width, pc) == NULL
	   && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc));
}


/*
 * Format editorial comment about unknown encoding
 */
int
encoding_editorial(body, width, pc)
    BODY      *body;
    int	       width;
    gf_io_t    pc;
{
    char     buf[2048];

    sprintf(buf, ENCODING_DISCLAIMER, body_encodings[body->encoding]);

    return(format_editorial(buf, width, pc) == NULL
	   && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc));
}



/*
 * Format editorial comment referencing screen offering
 * List-* header supplied commands
 */
int
rfc2369_editorial(msgno, handlesp, flags, width, pc)
    long       msgno;
    HANDLE_S **handlesp;
    int	       flags;
    int	       width;
    gf_io_t    pc;
{
    char     *p, *hdrp, *hdrs[MLCMD_COUNT + 1],
	      color[64], buf[2048];
    int	      i, n, rv = TRUE;
    HANDLE_S *h = NULL;

    if((flags & FM_DISPLAY)
       && (hdrp = pine_fetchheader_lines(ps_global->mail_stream, msgno,
					 NULL, rfc2369_hdrs(hdrs)))){
	if(*hdrp){
	    sprintf(buf, "Note: This message contains ");
	    p = buf + strlen(buf);

	    if(handlesp){
		h		      = new_handle(handlesp);
		h->type		      = Function;
		h->h.func.f	      = rfc2369_display;
		h->h.func.args.stream = ps_global->mail_stream;
		h->h.func.args.msgmap = ps_global->msgmap;
		h->h.func.args.msgno  = msgno;

		if(!(flags & FM_NOCOLOR)
		   && scroll_handle_start_color(color, &n)){
		    for(i = 0; i < n; i++)
		      *p++ = color[i];
		}
		else{
		    *p++ = TAG_EMBED;
		    *p++ = TAG_BOLDON;
		}

		*p++ = TAG_EMBED;
		*p++ = TAG_HANDLE;
		sprintf(p + 1, "%d", h->key);
		*p  = strlen(p + 1);
		p  += (*p + 1);
	    }

	    sstrcpy(&p, "email list management information");

	    if(h){
		/* in case it was the current selection */
		*p++ = TAG_EMBED;
		*p++ = TAG_INVOFF;

		if(scroll_handle_end_color(color, &n)){
		    for(i = 0; i < n; i++)
		      *p++ = color[i];
		}
		else{
		    *p++ = TAG_EMBED;
		    *p++ = TAG_BOLDOFF;
		}

		*p = '\0';
	    }

	    rv = (gf_puts(NEWLINE, pc)
		  && format_editorial(buf, width, pc) == NULL
		  && gf_puts(NEWLINE, pc));
	}
       
	fs_give((void **) &hdrp);
    }

    return(rv);
}



/*
 * format_blip_seen - if seen bit (which is usually cleared as a side-effect
 *		      of body part fetches as we're formatting) for the
 *		      given message isn't set (likely because there
 *		      weren't any parts suitable for display), then make
 *		      sure to set it here.
 */
int
format_blip_seen(msgno)
    long msgno;
{
    MESSAGECACHE *mc;

    if(msgno > 0L && ps_global->mail_stream
       && msgno <= ps_global->mail_stream->nmsgs
       && (mc = mail_elt(ps_global->mail_stream, msgno))
       && !mc->seen
       && !ps_global->mail_stream->rdonly)
      mail_flag(ps_global->mail_stream, long2string(msgno), "\\SEEN", ST_SET);

    return(1);
}


/*
 * format_view_margin - return sane value for vertical margins
 */
int *
format_view_margin()
{
    static int margin[2];
    char tmp[100], e[200], *err, lastchar = 0;
    int left = 0, right = 0, leftm = 0, rightm = 0;
    size_t l;

    memset(margin, 0, sizeof(margin));

    /*
     * We initially tried to make sure that the user didn't shoot themselves
     * in the foot by setting this too small. People seem to want to do some
     * strange stuff, so we're going to relax the wild shot detection and
     * let people set what they want, until we get to the point of totally
     * absurd. We've also added the possibility of appending the letter c
     * onto the width. That means treat the value as an absolute column
     * instead of a width. That is, a right margin of 76c means wrap at
     * column 76, whereas right margin of 4 means to wrap at column
     * screen width - 4.
     */
    if(ps_global->VAR_VIEW_MARGIN_LEFT){
	strncpy(tmp, ps_global->VAR_VIEW_MARGIN_LEFT, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';
	removing_leading_and_trailing_white_space(tmp);
	if(tmp[0]){
	    l = strlen(tmp);
	    if(l > 0)
	      lastchar = tmp[l-1];

	    if(lastchar == 'c')
	      tmp[l-1] = '\0';
	    
	    if(err = strtoval(tmp, &left, 0, 0, 0, e, "Viewer-margin-left")){
		leftm = 0;
		dprint(2, (debugfile, "%s\n", err));
	    }
	    else{
		if(lastchar == 'c')
		  leftm = left-1;
		else
		  leftm = left;
		
		leftm = min(max(0, leftm), ps_global->ttyo->screen_cols);
	    }
	}
    }

    if(ps_global->VAR_VIEW_MARGIN_RIGHT){
	strncpy(tmp, ps_global->VAR_VIEW_MARGIN_RIGHT, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';
	removing_leading_and_trailing_white_space(tmp);
	if(tmp[0]){
	    l = strlen(tmp);
	    if(l > 0)
	      lastchar = tmp[l-1];

	    if(lastchar == 'c')
	      tmp[l-1] = '\0';
	    
	    if(err = strtoval(tmp, &right, 0, 0, 0, e, "Viewer-margin-right")){
		rightm = 0;
		dprint(2, (debugfile, "%s\n", err));
	    }
	    else{
		if(lastchar == 'c')
		  rightm = ps_global->ttyo->screen_cols - right;
		else
		  rightm = right;
		
		rightm = min(max(0, rightm), ps_global->ttyo->screen_cols);
	    }
	}
    }

    if((rightm > 0 || leftm > 0) && rightm >= 0 && leftm >= 0
       && ps_global->ttyo->screen_cols - rightm - leftm >= 8){
	margin[0] = leftm;
	margin[1] = rightm;
    }

    return(margin);
}



/*
 *  This is a list of header fields that are represented canonically
 *  by the c-client's ENVELOPE structure.  The list is used by the
 *  two functions below to decide if a given field is included in this
 *  set.
 */
static struct envelope_s {
    char *name;
    long val;
} envelope_hdrs[] = {
    {"from",		FE_FROM},
    {"sender",		FE_SENDER},
    {"date",		FE_DATE},
    {"to",		FE_TO},
    {"cc",		FE_CC},
    {"bcc",		FE_BCC},
    {"newsgroups",	FE_NEWSGROUPS},
    {"subject",		FE_SUBJECT},
    {"message-id",	FE_MESSAGEID},
    {"reply-to",	FE_REPLYTO},
    {"followup-to",	FE_FOLLOWUPTO},
    {"in-reply-to",	FE_INREPLYTO},
/*  {"return-path",	FE_RETURNPATH},      not usually filled in */
    {"references",	FE_REFERENCES},
    {NULL,		0}
};


/*
 * is_an_env_hdr - is this name a header in the envelope structure?
 *
 *             name - the header name to check
 */
int
is_an_env_hdr(name)
    char *name;
{
    register int i;

    for(i = 0; envelope_hdrs[i].name; i++)
      if(!strucmp(name, envelope_hdrs[i].name))
	return(1);

    return(0);
}


/*
 * is_an_addr_hdr - is this an address header?
 *
 *             name - the header name to check
 */
int
is_an_addr_hdr(fieldname)
    char *fieldname;
{
    char   fbuf[FBUF_LEN+1];
    char  *colon, *fname;
    static char *addr_headers[] = {
	"from",
	"reply-to",
	"to",
	"cc",
	"bcc",
	"return-path",
	"sender",
	"x-sender",
	"x-x-sender",
	"resent-from",
	"resent-to",
	"resent-cc",
	NULL
    };

    /* so it is pointing to NULL */
    char **p = addr_headers + sizeof(addr_headers)/sizeof(*addr_headers) - 1;

    if((colon = strindex(fieldname, ':')) != NULL){
	strncpy(fbuf, fieldname, min(colon-fieldname,sizeof(fbuf)));
	fbuf[min(colon-fieldname,sizeof(fbuf)-1)] = '\0';
	fname = fbuf;
    }
    else
      fname = fieldname;

    if(fname && *fname){
	for(p = addr_headers; *p; p++)
	  if(!strucmp(fname, *p))
	    break;
    }

    return((*p) ? 1 : 0);
}


/*
 * Format a single field from the envelope
 */
void
format_env_hdr(stream, msgno, section, env, pc, field, flags)
    MAILSTREAM *stream;
    long	msgno;
    char       *section;
    ENVELOPE   *env;
    gf_io_t	pc;
    char       *field;
    int         flags;
{
    register int i;

    for(i = 0; envelope_hdrs[i].name; i++)
      if(!strucmp(field, envelope_hdrs[i].name)){
	  format_envelope(stream, msgno, section, env, pc,
			  envelope_hdrs[i].val, flags);
	  return;
      }
}


/*
 * Look through header string beginning with "begin", for the next
 * occurrence of header "field".  Set "start" to that.  Set "end" to point one
 * position past all of the continuation lines that go with "field".
 * That is, if "end" is converted to a null
 * character then the string "start" will be the next occurence of header
 * "field" including all of its continuation lines. Assume we
 * have CRLF's as end of lines.
 *
 * If "field" is NULL, then we just leave "start" pointing to "begin" and
 * make "end" the end of that header.
 *
 * Returns 1 if found, 0 if not.
 */
int
delineate_this_header(field, begin, start, end)
    char  *field, *begin;
    char **start, **end;
{
    char tmpfield[MAILTMPLEN+2]; /* copy of field with colon appended */
    char *p;
    char *begin_srch;

    if(field == NULL){
	if(!begin || !*begin || isspace((unsigned char)*begin))
	  return 0;
	else
	  *start = begin;
    }
    else{
	strncpy(tmpfield, field, sizeof(tmpfield)-2);
	tmpfield[sizeof(tmpfield)-2] = '\0';
	strcat(tmpfield, ":");

	/*
	 * We require that start is at the beginning of a line, so
	 * either it equals begin (which we assume is the beginning of a
	 * line) or it is preceded by a CRLF.
	 */
	begin_srch = begin;
	*start = srchstr(begin_srch, tmpfield);
	while(*start && *start != begin
	             && !(*start - 2 >= begin && ISRFCEOL(*start - 2))){
	    begin_srch = *start + 1;
	    *start = srchstr(begin_srch, tmpfield);
	}

	if(!*start)
	  return 0;
    }

    for(p = *start; *p; p++){
	if(ISRFCEOL(p)
	   && (!isspace((unsigned char)*(p+2)) || *(p+2) == '\015')){
	    /*
	     * The final 015 in the test above is to test for the end
	     * of the headers.
	     */
	    *end = p+2;
	    break;
	}
    }

    if(!*p)
      *end = p;
    
    return 1;
}



HANDLE_S *
get_handle(handles, key)
    HANDLE_S *handles;
    int key;
{
    HANDLE_S *h;

    if(h = handles){
	for( ; h ; h = h->next)
	  if(h->key == key)
	    return(h);

	for(h = handles->prev ; h ; h = h->prev)
	  if(h->key == key)
	    return(h);
    }

    return(NULL);
}


void
init_handles(handlesp)
    HANDLE_S **handlesp;
{
    if(handlesp)
      *handlesp = NULL;
      
    (void) url_external_specific_handler(NULL, 0);
}



HANDLE_S *
new_handle(handlesp)
    HANDLE_S **handlesp;
{
    HANDLE_S *hp, *h = NULL;

    if(handlesp){
	h = (HANDLE_S *) fs_get(sizeof(HANDLE_S));
	memset(h, 0, sizeof(HANDLE_S));

	/* Put it in the list */
	if((hp = *handlesp) != NULL){
	    while(hp->next)
	      hp = hp->next;

	    h->key = hp->key + 1;
	    hp->next = h;
	    h->prev = hp;
	}
	else{
	    /* Assumption #2,340: There are NO ZERO KEY HANDLES */
	    h->key = 1;
	    *handlesp = h;
	}
    }

    return(h);
}


/*
 * Normally we ignore the is_used bit in HANDLE_S. However, if we are
 * using the delete_quotes filter, we pay attention to it. All of the is_used
 * bits are off by default, and the delete_quotes filter turns them on
 * if it is including lines with those handles.
 *
 * This is a bit of a crock, since it depends heavily on the order of the
 * filters. Notice that the charset_editorial filter, which comes after
 * delete_quotes and adds a handle, has to explicitly set the is_used bit!
 */
void
delete_unused_handles(handlesp)
    HANDLE_S **handlesp;
{
    HANDLE_S *h, *nexth;

    if(handlesp && *handlesp && (*handlesp)->using_is_used){
	for(h = *handlesp; h && h->prev; h = h->prev)
	  ;
	
	for(; h; h = nexth){
	    nexth = h->next;
	    if(h->is_used == 0){
		if(h == *handlesp)
		  *handlesp = nexth;

		free_handle(&h);
	    }
	}
    }
}


void
free_handle(h)
    HANDLE_S **h;
{
    if(h){
	if((*h)->next)				/* clip from list */
	  (*h)->next->prev = (*h)->prev;

	if((*h)->prev)
	  (*h)->prev->next = (*h)->next;

	if((*h)->type == URL){			/* destroy malloc'd data */
	    if((*h)->h.url.path)
	      fs_give((void **) &(*h)->h.url.path);

	   if((*h)->h.url.tool)
	     fs_give((void **) &(*h)->h.url.tool);

	    if((*h)->h.url.name)
	      fs_give((void **) &(*h)->h.url.name);
	}

	free_handle_locations(&(*h)->loc);

	fs_give((void **) h);
    }
}


void
free_handles(handlesp)
    HANDLE_S **handlesp;
{
    HANDLE_S *h;

    if(handlesp && *handlesp){
	while((h = (*handlesp)->next) != NULL)
	  free_handle(&h);

	while((h = (*handlesp)->prev) != NULL)
	  free_handle(&h);

	free_handle(handlesp);
    }
}


void
free_handle_locations(l)
    POSLIST_S **l;
{
    if(*l){
	free_handle_locations(&(*l)->next);
	fs_give((void **) l);
    }
}


int
scroll_handle_prompt(handle, force)
    HANDLE_S *handle;
    int	      force;
{
    char prompt[256], tmp[MAILTMPLEN];
    int  rc, flags, local_h;
    static ESCKEY_S launch_opts[] = {
	{'y', 'y', "Y", "Yes"},
	{'n', 'n', "N", "No"},
	{-2, 0, NULL, NULL},
	{-2, 0, NULL, NULL},
	{0, 'u', "U", "editURL"},
	{0, 'a', "A", "editApp"},
	{-1, 0, NULL, NULL}};

    if(handle->type == URL){
	launch_opts[4].ch = 'u';

	if(!(local_h = !struncmp(handle->h.url.path, "x-pine-", 7))
	   && (handle->h.url.tool
	       || ((local_h = url_local_handler(handle->h.url.path) != NULL)
		   && (handle->h.url.tool = url_external_handler(handle,1)))
	       || (!local_h
		   && (handle->h.url.tool = url_external_handler(handle,0))))){
#ifdef	_WINDOWS
	    /* if NOT special DDE hack */
	    if(handle->h.url.tool[0] != '*')
#endif
	    if(ps_global->vars[V_BROWSER].is_fixed)
	      launch_opts[5].ch = -1;
	    else
	      launch_opts[5].ch = 'a';
	}
	else{
	    launch_opts[5].ch = -1;
	    if(!local_h){
	      if(ps_global->vars[V_BROWSER].is_fixed){
		  q_status_message(SM_ORDER, 3, 4,
				   "URL-Viewer is disabled by sys-admin");
		  return(0);
	      }
	      else{
		if(want_to("No URL-Viewer application defined.  Define now",
			   'y', 0, NO_HELP, WT_SEQ_SENSITIVE) == 'y'){
		    /* Prompt for the displayer? */
		    tmp[0] = '\0';
		    while(1){
			flags = OE_APPEND_CURRENT |
				OE_SEQ_SENSITIVE |
				OE_KEEP_TRAILING_SPACE;

			rc = optionally_enter(tmp, -FOOTER_ROWS(ps_global), 0,
					      sizeof(tmp),
					      "Web Browser: ",
					      NULL, NO_HELP, &flags);
			if(rc == 0){
			    if((flags & OE_USER_MODIFIED) && *tmp){
				if(can_access(tmp, EXECUTE_ACCESS) == 0){
				    int	   n;
				    char **l;

				    /*
				     * Save it for next time...
				     */
				    for(l = ps_global->VAR_BROWSER, n = 0;
					l && *l;
					l++)
				      n++; /* count */

				    l = (char **) fs_get((n+2)*sizeof(char *));
				    for(n = 0;
					ps_global->VAR_BROWSER
					  && ps_global->VAR_BROWSER[n];
					n++)
				      l[n] = cpystr(ps_global->VAR_BROWSER[n]);

				    l[n++] = cpystr(tmp);
				    l[n]   = NULL;

				    set_variable_list(V_BROWSER, l, TRUE, Main);

				    handle->h.url.tool = cpystr(tmp);
				    break;
				}
				else{
				    q_status_message1(SM_ORDER | SM_DING, 2, 2,
						    "Browser not found: %.200s",
						     error_description(errno));
				    continue;
				}
			    }
			    else
			      return(0);
			}
			else if(rc == 1 || rc == -1){
			    return(0);
			}
			else if(rc == 4){
			    if(ps_global->redrawer)
			      (*ps_global->redrawer)();
			}
		    }
		}
		else
		  return(0);
	      }
	    }
	}
    }
    else
      launch_opts[4].ch = -1;

    if(force
       || (handle->type == URL
	   && !struncmp(handle->h.url.path, "x-pine-", 7)))
      return(1);

    while(1){
	int sc = ps_global->ttyo->screen_cols;

	/*
	 * Customize the prompt for mailto, all the other schemes make
	 * sense if you just say View selected URL ...
	 */
	if(handle->type == URL &&
	   !struncmp(handle->h.url.path, "mailto:", 7))
	  sprintf(prompt, "Compose mail to \"%.*s%s\" ? ",
		  min(max(0,sc - 25), sizeof(prompt)-50), handle->h.url.path+7,
		  (strlen(handle->h.url.path+7) > max(0,sc-25)) ? "..." : "");
	else
	  sprintf(prompt, "View selected %s %s%.*s%s ? ",
		  (handle->type == URL) ? "URL" : "Attachment",
		  (handle->type == URL) ? "<" : "",
		  min(max(0,sc-27), sizeof(prompt)-50),
		  (handle->type == URL) ? handle->h.url.path : "",
		  (handle->type == URL)
		    ? ((strlen(handle->h.url.path) > max(0,sc-27))
			    ? "...>" : ">") : "");

	switch(radio_buttons(prompt, -FOOTER_ROWS(ps_global),
			     launch_opts, 'y', 'n', NO_HELP, RB_SEQ_SENSITIVE,
			     NULL)){
	  case 'y' :
	    return(1);

	  case 'u' :
	    strncpy(tmp, handle->h.url.path, sizeof(tmp)-1);
	    tmp[sizeof(tmp)-1] = '\0';
	    while(1){
		flags = OE_APPEND_CURRENT |
			OE_SEQ_SENSITIVE |
			OE_KEEP_TRAILING_SPACE;

		rc = optionally_enter(tmp, -FOOTER_ROWS(ps_global), 0,
				      sizeof(tmp), "Edit URL: ",
				      NULL, NO_HELP, &flags);
		if(rc == 0){
		    if(flags & OE_USER_MODIFIED){
			if(handle->h.url.path)
			  fs_give((void **) &handle->h.url.path);

			handle->h.url.path = cpystr(tmp);
		    }

		    break;
		}
		else if(rc == 1 || rc == -1){
		    return(0);
		}
		else if(rc == 4){
		    if(ps_global->redrawer)
		      (*ps_global->redrawer)();
		}
	    }

	    continue;

	  case 'a' :
	    if(handle->h.url.tool){
		strncpy(tmp, handle->h.url.tool, sizeof(tmp)-1);
		tmp[sizeof(tmp)-1] = '\0';
	    }
	    else
	      tmp[0] = '\0';

	    while(1){
		flags = OE_APPEND_CURRENT |
			OE_SEQ_SENSITIVE |
			OE_KEEP_TRAILING_SPACE |
			OE_DISALLOW_HELP;

		sprintf(prompt, "Viewer command: ");

		rc = optionally_enter(tmp, -FOOTER_ROWS(ps_global), 0,
				      sizeof(tmp), "Viewer Command: ",
				      NULL, NO_HELP, &flags);
		if(rc == 0){
		    if(flags & OE_USER_MODIFIED){
			if(handle->h.url.tool)
			  fs_give((void **) &handle->h.url.tool);

			handle->h.url.tool = cpystr(tmp);
		    }

		    break;
		}
		else if(rc == 1 || rc == -1){
		    return(0);
		}
		else if(rc == 4){
		    if(ps_global->redrawer)
		      (*ps_global->redrawer)();
		}
	    }

	    continue;

	  case 'n' :
	  default :
	    return(0);
	}
    }
}



int
scroll_handle_launch(handle, force)
    HANDLE_S *handle;
    int	      force;
{
    switch(handle->type){
      case URL :
	if(handle->h.url.path){
	    if(scroll_handle_prompt(handle, force)){
		if(url_launch(handle)
		   || ps_global->next_screen != SCREEN_FUN_NULL)
		  return(1);	/* done with this screen */
	    }
	    else
	      return(-1);
	}

      break;

      case Attach :
	if(scroll_handle_prompt(handle, force))
	  display_attachment(mn_m2raw(ps_global->msgmap,
				      mn_get_cur(ps_global->msgmap)),
			     handle->h.attach, DA_FROM_VIEW | DA_DIDPROMPT);
	else
	  return(-1);

	break;

      case Folder :
	break;

      case Function :
	(*handle->h.func.f)(handle->h.func.args.stream,
			    handle->h.func.args.msgmap,
			    handle->h.func.args.msgno);
	break;


      default :
	panic("Unexpected HANDLE type");
    }

    return(0);
}



int
scroll_handle_obscured(handle)
    HANDLE_S *handle;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    return(handle_on_page(handle, st->top_text_line,
			  st->top_text_line + st->screen.length));
}



/*
 * scroll_handle_in_frame -- return handle pointer to visible handle.
 */
HANDLE_S *
scroll_handle_in_frame(top_line)
    long top_line;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    HANDLE_S   *hp;

    switch(handle_on_page(hp = st->parms->text.handles, top_line,
			  top_line + st->screen.length)){
      case -1 :			/* handle above page */
	/* Find first handle from top of page */
	for(hp = st->parms->text.handles->next; hp; hp = hp->next)
	  if(scroll_handle_selectable(hp))
	    switch(handle_on_page(hp, top_line, top_line + st->screen.length)){
	      case  0 : return(hp);
	      case  1 : return(NULL);
	      case -1 : default : break;
	    }

	break;

      case 1 :			/* handle below page */
	/* Find first handle from top of page */
	for(hp = st->parms->text.handles->prev; hp; hp = hp->prev)
	  if(scroll_handle_selectable(hp))
	    switch(handle_on_page(hp, top_line, top_line + st->screen.length)){
	      case  0 : return(hp);
	      case -1 : return(NULL);
	      case  1 : default : break;
	    }

	break;

      case  0 :
      default :
	break;
    }

    return(hp);
}



/*
 * scroll_handle_reframe -- adjust display params to display given handle
 */
long
scroll_handle_reframe(key, center)
    int key, center;
{
    long	l, offset, dlines, start_line;
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    dlines     = PGSIZE(st);
    offset     = (st->parms->text.src == FileStar) ? st->top_text_line : 0;
    start_line = st->top_text_line;

    if(key < 0)
      key = st->parms->text.handles->key;

    /* Searc down from the top line */
    for(l = start_line; l < st->num_lines; l++) {
	if(st->parms->text.src == FileStar && l > offset + dlines)
	  ScrollFile(offset += dlines);

	if(handle_on_line(l - offset, key))
	  break;
    }

    if(l < st->num_lines){
	if(l >= dlines + start_line)			/* bingo! */
	  start_line = l - ((center ? (dlines / 2) : dlines) - 1);
    }
    else{
	if(st->parms->text.src == FileStar)		/* wrap offset */
	  ScrollFile(offset = 0);

	for(l = 0; l < start_line; l++) {
	    if(st->parms->text.src == FileStar && l > offset + dlines)
	      ScrollFile(offset += dlines);

	    if(handle_on_line(l - offset, key))
	      break;
	}

	if(l == start_line)
	  panic("Internal Error: no handle found");
	else
	  start_line = l;
    }

    return(start_line);
}



int
handle_on_line(line, goal)
    long  line;
    int	  goal;
{
    int		i, n, key;
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    for(i = 0; i < st->line_lengths[line]; i++)
      if(st->text_lines[line][i] == TAG_EMBED
	 && st->text_lines[line][++i] == TAG_HANDLE){
	  for(key = 0, n = st->text_lines[line][++i]; n; n--)
	    key = (key * 10) + (st->text_lines[line][++i] - '0');

	  if(key == goal)
	    return(1);
      }

    return(0);
}



int
handle_on_page(handle, first_line, last_line)
    HANDLE_S *handle;
    long      first_line, last_line;
{
    POSLIST_S *l;
    int	       rv = 0;

    if(handle && (l = handle->loc)){
	for( ; l; l = l->next)
	  if(l->where.row < first_line){
	      if(!rv)
		rv = -1;
	  }
	  else if(l->where.row >= last_line){
	      if(!rv)
		rv = 1;
	  }
	  else
	    return(0);				/* found! */
    }

    return(rv);
}



int
scroll_handle_selectable(handle)
    HANDLE_S *handle;
{
    return(handle && (handle->type != URL
		      || (handle->h.url.path && *handle->h.url.path)));
}    



HANDLE_S *
scroll_handle_next_sel(handles)
    HANDLE_S *handles;
{
    while(handles && !scroll_handle_selectable(handles = handles->next))
      ;

    return(handles);
}


HANDLE_S *
scroll_handle_prev_sel(handles)
    HANDLE_S *handles;
{
    while(handles && !scroll_handle_selectable(handles = handles->prev))
      ;

    return(handles);
}



HANDLE_S *
scroll_handle_next(handles)
    HANDLE_S *handles;
{
    HANDLE_S *next = NULL;

    if(scroll_handle_obscured(handles) <= 0){
	next = handles;
	while((next = scroll_handle_next_sel(next))
	      && scroll_handle_obscured(next))
	  ;
    }

    return(next);
}



HANDLE_S *
scroll_handle_prev(handles)
    HANDLE_S *handles;
{
    HANDLE_S *prev = NULL;

    if(scroll_handle_obscured(handles) >= 0){
	prev = handles;
	while((prev = scroll_handle_prev_sel(prev))
	      && scroll_handle_obscured(prev))
	  ;
    }

    return(prev);
}



HANDLE_S *
scroll_handle_boundary(handle, f)
    HANDLE_S *handle;
    HANDLE_S *(*f) PROTO((HANDLE_S *));
{
    HANDLE_S  *hp, *whp = NULL;

    /* Multi-line handle? Punt! */
    if(handle && (!(hp = handle)->loc->next))
      while((hp = (*f)(hp))
	    && hp->loc->where.row == handle->loc->where.row)
	whp = hp;

    return(whp);
}



int
scroll_handle_column(row, offset)
    int	  row, offset;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int		i = 0, n, column = 0;

    while(i < ((offset > -1) ? offset : st->line_lengths[row]))
      switch(st->text_lines[row][i++]){
	case TAG_EMBED :
	  if(st->text_lines[row][i++] == TAG_HANDLE)
	    i += st->text_lines[row][i] + 1;

	  break;

	case ESCAPE :		/* Don't count escape in len */
	  if(n = match_escapes(&st->text_lines[row][i]))
	    i += (n - 1);

	  break;

	case TAB :
	  while(((++column) &  0x07) != 0) /* add tab's spaces */
	    ;

	  break;

	default :
	  column++;
	  break;
      }

    return(column);
}



int
scroll_handle_index(row, column)
    int row, column;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int index = 0, n;

    for(index = 0; column > 0;)
      switch(st->text_lines[row][index++]){
	case TAG_EMBED :
	  switch(st->text_lines[row][index++]){
	    case TAG_HANDLE:
	      index += st->text_lines[row][index] + 1;
	      break;

	    case TAG_FGCOLOR :
	    case TAG_BGCOLOR :
	      index += RGBLEN;
	      break;

	    default :
	      break;
	  }

	  break;

	case ESCAPE :
	  if(n = match_escapes(&st->text_lines[row][index]))
	    index += (n - 1);

	  break;

	case TAB : /* add tab's spaces */
	  while(((--column) &  0x07) != 0)
	    ;

	  break;

	default :
	  column--;
	  break;
      }

    return(index);
}



void
scroll_handle_set_loc(lpp, line, column)
    POSLIST_S **lpp;
    int		line, column;
{
    POSLIST_S *lp;

    lp = (POSLIST_S *) fs_get(sizeof(POSLIST_S));
    lp->where.row = line;
    lp->where.col = column;
    lp->next	= NULL;
    for(; *lpp; lpp = &(*lpp)->next)
      ;

    *lpp = lp;
}



int
scroll_handle_start_color(colorstring, len)
    char *colorstring;
    int  *len;
{
    *len = 0;

    if(pico_usingcolor()){
	char *fg = NULL, *bg = NULL, *s;

	if(ps_global->VAR_SLCTBL_FORE_COLOR
	   && colorcmp(ps_global->VAR_SLCTBL_FORE_COLOR,
		       ps_global->VAR_NORM_FORE_COLOR))
	  fg = ps_global->VAR_SLCTBL_FORE_COLOR;

	if(ps_global->VAR_SLCTBL_BACK_COLOR
	   && colorcmp(ps_global->VAR_SLCTBL_BACK_COLOR,
		       ps_global->VAR_NORM_BACK_COLOR))
	  bg = ps_global->VAR_SLCTBL_BACK_COLOR;

	if(bg || fg){
	    COLOR_PAIR *tmp;

	    /*
	     * The blacks are just known good colors for
	     * testing whether the other color is good.
	     */
	    if(tmp = new_color_pair(fg ? fg : colorx(COL_BLACK),
				    bg ? bg : colorx(COL_BLACK))){
		if(pico_is_good_colorpair(tmp))
		  for(s = color_embed(fg, bg);
		      colorstring[*len] = *s;
		      s++, (*len)++)
		    ;

		free_color_pair(&tmp);
	    }
	}

	if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
	    strncpy(colorstring + *len, url_embed(TAG_BOLDON), 2);
	    *len += 2;
	}
    }

    return(*len != 0);
}



int
scroll_handle_end_color(colorstring, len)
    char *colorstring;
    int  *len;
{
    *len = 0;
    if(pico_usingcolor()){
	char *fg = NULL, *bg = NULL, *s;

	/*
	 * We need to change the fg and bg colors back even if they
	 * are the same as the Normal Colors so that color_a_quote
	 * will have a chance to pick up those colors as the ones to
	 * switch to. We don't do this before the handle above so that
	 * the quote color will flow into the selectable item when
	 * the selectable item color is partly the same as the
	 * normal color. That is, suppose the normal color was black on
	 * cyan and the selectable color was blue on cyan, only a fg color
	 * change. We preserve the only-a-fg-color-change in a quote by
	 * letting the quote background color flow into the selectable text.
	 */
	if(ps_global->VAR_SLCTBL_FORE_COLOR)
	  fg = ps_global->VAR_NORM_FORE_COLOR;

	if(ps_global->VAR_SLCTBL_BACK_COLOR)
	  bg = ps_global->VAR_NORM_BACK_COLOR;

	if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
	    strncpy(colorstring, url_embed(TAG_BOLDOFF), 2);
	    *len = 2;
	}

	if(fg || bg)
	  for(s = color_embed(fg, bg); colorstring[*len] = *s; s++, (*len)++)
	    ;
    }

    return(*len != 0);
}




int
dot_on_handle(line, goal)
    long line;
    int  goal;
{
    int		i = 0, n, key = 0, column = -1;
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    do{
	if(i >= st->line_lengths[line])
	  return(0);

	switch(st->text_lines[line][i++]){
	  case TAG_EMBED :
	    switch(st->text_lines[line][i++]){
	      case TAG_HANDLE :
		for(key = 0, n = st->text_lines[line][i++]; n; n--)
		  key = (key * 10) + (st->text_lines[line][i++] - '0');

		break;

	      case TAG_FGCOLOR :
	      case TAG_BGCOLOR :
		i += RGBLEN;	/* advance past color setting */
		break;

	      case TAG_BOLDOFF :
		key = 0;
		break;
	    }

	    break;

	  case ESCAPE :		/* Don't count escape in len */
	    i += (n = match_escapes(&st->text_lines[line][i])) ? n : 1;
	    break;

	  case TAB :
	    while(((++column) &  0x07) != 0) /* add tab's spaces */
	      ;

	    break;

	  default :
	    column++;
	}
    }
    while(column < goal);

    return(key);
}



char *
url_embed(embed)
    int embed;
{
    static char buf[2] = {TAG_EMBED};
    buf[1] = embed;
    return(buf);
}



char *
color_embed(fg, bg)
    char *fg, *bg;
{
    static char buf[(2 * RGBLEN) + 5], *p;

    p = buf;
    if(fg){
	*p++ = TAG_EMBED;
	*p++ = TAG_FGCOLOR;
	sstrcpy(&p, color_to_asciirgb(fg));
    }

    if(bg){
	*p++ = TAG_EMBED;
	*p++ = TAG_BGCOLOR;
	sstrcpy(&p, color_to_asciirgb(bg));
    }

    return(buf);
}


int
colorcmp(color1, color2)
    char *color1, *color2;
{
    char buf[RGBLEN+1];

    if(color1 && color2){
	strcpy(buf, color_to_asciirgb(color1));
	return(strcmp(buf, color_to_asciirgb(color2)));
    }

    /* if both NULL they're the same? */
    return(!(color1 || color2));
}


struct quote_colors {
    COLOR_PAIR          *color;
    struct quote_colors *next;
};

int
color_a_quote(linenum, line, ins, is_flowed_msg)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *is_flowed_msg;
{
    int countem = 0;
    struct variable *vars = ps_global->vars;
    char *p;
    struct quote_colors *colors = NULL, *cp, *next;
    COLOR_PAIR *col = NULL;
    int is_flowed = is_flowed_msg ? *((int *)is_flowed_msg) : 0;

    p = line;
    if(!is_flowed)
      while(isspace((unsigned char)*p))
	p++;

    if(p[0] == '>'){
	struct quote_colors *c;

	/*
	 * We have a fixed number of quote level colors (3). If there are
	 * more levels of quoting than are defined, they repeat.
	 */
	if(VAR_QUOTE1_FORE_COLOR && VAR_QUOTE1_BACK_COLOR &&
	   (col = new_color_pair(VAR_QUOTE1_FORE_COLOR,
				 VAR_QUOTE1_BACK_COLOR)) &&
	   pico_is_good_colorpair(col)){
	    c = (struct quote_colors *)fs_get(sizeof(*c));
	    memset(c, 0, sizeof(*c));
	    c->color = col;
	    col = NULL;
	    colors = c;
	    cp = c;
	    countem++;
	    if(VAR_QUOTE2_FORE_COLOR && VAR_QUOTE2_BACK_COLOR &&
	       (col = new_color_pair(VAR_QUOTE2_FORE_COLOR,
				     VAR_QUOTE2_BACK_COLOR)) &&
	       pico_is_good_colorpair(col)){
		c = (struct quote_colors *)fs_get(sizeof(*c));
		memset(c, 0, sizeof(*c));
		c->color = col;
		col = NULL;
		cp->next = c;
		cp = c;
		countem++;
		if(VAR_QUOTE3_FORE_COLOR && VAR_QUOTE3_BACK_COLOR &&
		   (col = new_color_pair(VAR_QUOTE3_FORE_COLOR,
					 VAR_QUOTE3_BACK_COLOR)) &&
		   pico_is_good_colorpair(col)){
		    c = (struct quote_colors *)fs_get(sizeof(*cp));
		    memset(c, 0, sizeof(*c));
		    c->color = col;
		    col = NULL;
		    cp->next = c;
		    cp = c;
		    countem++;
		}
	    }
	}
    }

    if(col)
      free_color_pair(&col);

    cp = NULL;
    while(*p == '>'){
	cp = (cp && cp->next) ? cp->next : colors;

	if(countem > 0)
	  ins = gf_line_test_new_ins(ins, p,
				     color_embed(cp->color->fg, cp->color->bg),
				     (2 * RGBLEN) + 4);

	countem = (countem == 1) ? 0 : countem;

	p++;
	if(!is_flowed)
	  for(; isspace((unsigned char)*p); p++)
	    ;
    }

    if(colors){
	char fg[RGBLEN + 1], bg[RGBLEN + 1], rgbbuf[RGBLEN + 1];

	strcpy(fg, color_to_asciirgb(VAR_NORM_FORE_COLOR));
	strcpy(bg, color_to_asciirgb(VAR_NORM_BACK_COLOR));

	/*
	 * Loop watching colors, and override with most recent
	 * quote color whenever the normal foreground and background
	 * colors are in force.
	 */
	while(*p)
	  if(*p++ == TAG_EMBED){

	      switch(*p++){
		case TAG_HANDLE :
		  p += *p + 1;	/* skip handle key */
		  break;

		case TAG_FGCOLOR :
		  sprintf(rgbbuf, "%.*s", RGBLEN, p);
		  p += RGBLEN;	/* advance past color value */
		  
		  if(!colorcmp(rgbbuf, VAR_NORM_FORE_COLOR)
		     && !colorcmp(bg, VAR_NORM_BACK_COLOR))
		    ins = gf_line_test_new_ins(ins, p,
					       color_embed(cp->color->fg,NULL),
					       RGBLEN + 2);
		  break;

		case TAG_BGCOLOR :
		  sprintf(rgbbuf, "%.*s", RGBLEN, p);
		  p += RGBLEN;	/* advance past color value */
		  
		  if(!colorcmp(rgbbuf, VAR_NORM_BACK_COLOR)
		     && !colorcmp(fg, VAR_NORM_FORE_COLOR))
		    ins = gf_line_test_new_ins(ins, p,
					       color_embed(NULL,cp->color->bg),
					       RGBLEN + 2);

		  break;

		default :
		  break;
	      }
	  }

	ins = gf_line_test_new_ins(ins, line + strlen(line),
				   color_embed(VAR_NORM_FORE_COLOR,
					       VAR_NORM_BACK_COLOR),
				   (2 * RGBLEN) + 4);
	for(cp = colors; cp && cp->color; cp = next){
	    free_color_pair(&cp->color);
	    next = cp->next;
	    fs_give((void **)&cp);
	}
    }

    return(0);
}


/*
 * Paint the signature.
 */
int
color_signature(linenum, line, ins, is_in_sig)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *is_in_sig;
{
    struct variable *vars = ps_global->vars;
    int             *in_sig_block;
    COLOR_PAIR      *col = NULL;

    if(is_in_sig == NULL)
      return 0;

    in_sig_block = (int *) is_in_sig;
    
    if(!strcmp(line, SIGDASHES))
      *in_sig_block = START_SIG_BLOCK; 
    else if(*line == '\0')
      /* 
       * Suggested by Eduardo: allow for a blank line right after 
       * the sigdashes. 
       */
      *in_sig_block = (*in_sig_block == START_SIG_BLOCK)
			  ? IN_SIG_BLOCK : OUT_SIG_BLOCK;
    else
      *in_sig_block = (*in_sig_block != OUT_SIG_BLOCK)
			  ? IN_SIG_BLOCK : OUT_SIG_BLOCK;

    if(*in_sig_block != OUT_SIG_BLOCK
       && VAR_SIGNATURE_FORE_COLOR && VAR_SIGNATURE_BACK_COLOR
       && (col = new_color_pair(VAR_SIGNATURE_FORE_COLOR,
				VAR_SIGNATURE_BACK_COLOR))){
	if(!pico_is_good_colorpair(col))
	  free_color_pair(&col);
    }
    
    if(col){
	char *p, fg[RGBLEN + 1], bg[RGBLEN + 1], rgbbuf[RGBLEN + 1];

	ins = gf_line_test_new_ins(ins, line,
				   color_embed(col->fg, col->bg),
				   (2 * RGBLEN) + 4);

        strcpy(fg, color_to_asciirgb(VAR_NORM_FORE_COLOR));
	strcpy(bg, color_to_asciirgb(VAR_NORM_BACK_COLOR));

	/*
	 * Loop watching colors, and override with 
	 * signature color whenever the normal foreground and background
	 * colors are in force.
	 */

	for(p = line; *p; )
	  if(*p++ == TAG_EMBED){

	      switch(*p++){
		case TAG_HANDLE :
		  p += *p + 1;	/* skip handle key */
		  break;

		case TAG_FGCOLOR :
		  sprintf(rgbbuf, "%.*s", RGBLEN, p);
		  p += RGBLEN;	/* advance past color value */
		  
		  if(!colorcmp(rgbbuf, VAR_NORM_FORE_COLOR)
		     && !colorcmp(bg, VAR_NORM_BACK_COLOR))
		    ins = gf_line_test_new_ins(ins, p,
					       color_embed(col->fg,NULL),
					       RGBLEN + 2);
		  break;

		case TAG_BGCOLOR :
		  sprintf(rgbbuf, "%.*s", RGBLEN, p);
		  p += RGBLEN;	/* advance past color value */
		  
		  if(!colorcmp(rgbbuf, VAR_NORM_BACK_COLOR)
		     && !colorcmp(fg, VAR_NORM_FORE_COLOR))
		    ins = gf_line_test_new_ins(ins, p,
					       color_embed(NULL,col->bg),
					       RGBLEN + 2);

		  break;

		default :
		  break;
	      }
	  }
 
	ins = gf_line_test_new_ins(ins, line + strlen(line),
				   color_embed(VAR_NORM_FORE_COLOR,
					       VAR_NORM_BACK_COLOR),
				   (2 * RGBLEN) + 4);
        free_color_pair(&col);
    }

    return 0;
}


/*
 * Replace quotes of nonflowed messages.  This needs to happen
 * towards the end of filtering because a lot of prior filters
 * depend on "> ", such as quote coloring and suppression.
 * Quotes are already colored here, so we have to account for
 * that formatting.
 */
int
replace_quotes(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    char *lp = line, *prefix = NULL, *last_prefix = NULL;
    int no_more_quotes = 0, len, saw_quote = 0;

    if(ps_global->VAR_QUOTE_REPLACE_STRING){
	get_pair(ps_global->VAR_QUOTE_REPLACE_STRING, &prefix, &last_prefix, 0, 0);
	if(!prefix && last_prefix){
	    prefix = last_prefix;
	    last_prefix = NULL;
	}
    }
    else
      return(0);

    while(isspace((unsigned char)(*lp)))
      lp++;
    while(*lp && !no_more_quotes){
	switch(*lp++){
	  case '>':
	    if(*lp == ' '){
		ins = gf_line_test_new_ins(ins, lp - 1, "", -2);
		*lp++;
	    }
	    else
	      ins = gf_line_test_new_ins(ins, lp - 1, "", -1);
	    if(strlen(prefix))
	      ins = gf_line_test_new_ins(ins, lp, prefix, strlen(prefix));
	    saw_quote = 1;
	    break;
	  case TAG_EMBED:
	    switch(*lp++){
	      case TAG_FGCOLOR:
	      case TAG_BGCOLOR:
		len = RGBLEN;
		break;
	      case TAG_HANDLE:
		len = *lp++ + 1;
		break;
	    }
	    if(strlen(lp) < len)
	      no_more_quotes = 1;
	    else
	      lp += len;
	    break;
	  case ' ':
	  case '\t':
	    break;
	  default:
	    if(saw_quote && last_prefix)
	      ins = gf_line_test_new_ins(ins, lp - 1, last_prefix, strlen(last_prefix));
	    no_more_quotes = 1;
	    break;
	}
    }
    if(prefix)
      fs_give((void **)&prefix);
    if(last_prefix)
      fs_give((void **)&last_prefix);
    return(0);
}

/*
 * This one is a little more complicated because it comes late in the
 * filtering sequence after colors have been added and url's hilited and
 * so on. So if it wants to look at the beginning of the line it has to
 * wade through some stuff done by the other filters first. This one is
 * skipping the leading indent added by the viewer-margin stuff and leading
 * tags.
 */
int
delete_quotes(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    DELQ_S *dq;
    char   *lp;
    int     i, lines, not_a_quote = 0;
    size_t  len;

    dq = (DELQ_S *) local;
    if(!dq)
      return(0);

    if(dq->lines > 0 || dq->lines == Q_DEL_ALL){

	lines = (dq->lines == Q_DEL_ALL) ? 0 : dq->lines;

	/*
	 * First determine if this line is part of a quote.
	 */

	/* skip over indent added by viewer-margin-left */
	lp = line;
	for(i = dq->indent_length; i > 0 && !not_a_quote && *lp; i--)
	  if(*lp++ != SPACE)
	    not_a_quote++;
	
	/* skip over leading tags */
	while(!not_a_quote
	      && ((unsigned char) (*lp) == (unsigned char) TAG_EMBED)){
	    switch(*++lp){
	      case '\0':
		not_a_quote++;
	        break;

	      default:
		++lp;
		break;

	      case TAG_FGCOLOR:
	      case TAG_BGCOLOR:
	      case TAG_HANDLE:
		switch(*lp){
		  case TAG_FGCOLOR:
		  case TAG_BGCOLOR:
		    len = RGBLEN + 1;
		    break;

		  case TAG_HANDLE:
		    len = *++lp + 1;
		    break;
		}

		if(strlen(lp) < len)
		  not_a_quote++;
		else
		  lp += len;

		break;

	      case TAG_EMBED:
		break;
	    }
	}

	/* skip over whitespace */
	if(!dq->is_flowed)
	  while(isspace((unsigned char) *lp))
	    lp++;

	/* check first character to see if it is a quote */
	if(!not_a_quote && *lp != '>')
	  not_a_quote++;

	if(not_a_quote){
	    if(dq->in_quote > lines+1){
	      char tmp[500];

	      /*
	       * Display how many lines were hidden.
	       */
	      sprintf(tmp,
		      "%.200s[ %d lines of quoted text hidden from view ]\r\n",
		      repeat_char(dq->indent_length, SPACE),
		      dq->in_quote - lines);
	      if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){

		sprintf(tmp, "%.200s[ %d lines of quoted text hidden ]\r\n",
			repeat_char(dq->indent_length, SPACE),
			dq->in_quote - lines);

		if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		  sprintf(tmp, "%.200s[ %d lines hidden ]\r\n",
			  repeat_char(dq->indent_length, SPACE),
			  dq->in_quote - lines);

		  if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		    sprintf(tmp, "%.200s[ %d hidden ]\r\n",
			    repeat_char(dq->indent_length, SPACE),
			    dq->in_quote - lines);
		  
		    if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		      sprintf(tmp, "%.200s[...]\r\n",
			      repeat_char(dq->indent_length, SPACE));

		      if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		        sprintf(tmp, "%.200s...\r\n",
			        repeat_char(dq->indent_length, SPACE));

			if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		          sprintf(tmp, "%.200s\r\n",
			      repeat_char(min(ps_global->ttyo->screen_cols,3),
					  '.'));
			}
		      }
		    }
		  }
		}
	      }

	      ins = gf_line_test_new_ins(ins, line, tmp, strlen(tmp));
	      mark_handles_in_line(line, dq->handlesp, 1);
	      ps_global->some_quoting_was_suppressed = 1;
	    }
	    else if(dq->in_quote == lines+1
		    && dq->saved_line && *dq->saved_line){
		/*
		 * We would have only had to delete a single line. Instead
		 * of deleting it, just show it.
		 */
		ins = gf_line_test_new_ins(ins, line,
					   *dq->saved_line,
					   strlen(*dq->saved_line));
		mark_handles_in_line(*dq->saved_line, dq->handlesp, 1);
	    }
	    else
	      mark_handles_in_line(line, dq->handlesp, 1);

	    if(dq->saved_line && *dq->saved_line)
	      fs_give((void **) dq->saved_line);

	    dq->in_quote = 0;
	}
	else{
	    dq->in_quote++;				/* count it */
	    if(dq->in_quote > lines){
		/*
		 * If the hidden part is a single line we'll show it instead.
		 */
		if(dq->in_quote == lines+1){
		    if(dq->saved_line && *dq->saved_line)
		      fs_give((void **) dq->saved_line);
		    
		    *dq->saved_line = fs_get(strlen(line) + 3);
		    sprintf(*dq->saved_line, "%s\r\n", line);
		}

		mark_handles_in_line(line, dq->handlesp, 0);
		/* skip it, at least for now */
		return(2);
	    }

	    mark_handles_in_line(line, dq->handlesp, 1);
	}
    }

    return(0);
}


/*
 * We need to delete handles that we are removing from the displayed
 * text. But there is a complication. Some handles appear at more than
 * one location in the text. So we keep track of which handles we're
 * actually using as we go, then at the end we delete the ones we
 * didn't use.
 * 
 * Args    line    -- the text line, which includes tags
 *       handlesp  -- handles pointer
 *         used    -- If 1, set handles in this line to used
 *                    if 0, leave handles in this line set as they already are
 */
void
mark_handles_in_line(line, handlesp, used)
    char      *line;
    HANDLE_S **handlesp;
    int        used;
{
    unsigned char c;
    size_t        length;
    int           key, n;
    HANDLE_S     *h;

    if(!(line && handlesp && *handlesp))
      return;

    length = strlen(line);

    /* mimic code in PutLine0n8b */
    while(length--){

	c = (unsigned char) *line++;

	if(c == (unsigned char) TAG_EMBED && length){

	    length--;

	    switch(*line++){
	      case TAG_HANDLE :
		length -= *line + 1;	/* key length plus length tag */

		for(key = 0, n = *line++; n; n--)
		  key = (key * 10) + (*line++ - '0');

		h = get_handle(*handlesp, key);
		if(h){
		    h->using_is_used = 1;
		    /* 
		     * If it's already marked is_used, leave it marked
		     * that way. We only call this function with used = 0
		     * in order to set using_is_used.
		     */
		    h->is_used = (h->is_used || used) ? 1 : 0;
		}

		break;

	      case TAG_FGCOLOR :
		if(length < RGBLEN){
		    length = 0;
		    break;
		}

		length -= RGBLEN;
		line += RGBLEN;
		break;

	      case TAG_BGCOLOR :
		if(length < RGBLEN){
		    length = 0;
		    break;
		}

		length -= RGBLEN;
		line += RGBLEN;
		break;

	      default:
		break;
	    }
	}	
    }
}


/*
 * Line filter to add color to displayed headers.
 */
int
color_headers(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    static char field[FBUF_LEN + 1];
    char        fg[RGBLEN + 1], bg[RGBLEN + 1], rgbbuf[RGBLEN + 1];
    char       *p, *q, *value, *beg;
    COLOR_PAIR *color;
    int         in_quote = 0, in_comment = 0, did_color = 0;
    struct variable *vars = ps_global->vars;

    field[FBUF_LEN] = '\0';

    if(isspace((unsigned char)*line))		/* continuation line */
      value = line;
    else{
	if(!(value = strindex(line, ':')))
	  return(0);
	
	memset(field, 0, sizeof(field));
	strncpy(field, line, min(value-line, sizeof(field)-1));
    }

    for(value++; isspace((unsigned char)*value); value++)
      ;

    strcpy(fg, color_to_asciirgb(VAR_NORM_FORE_COLOR));
    strcpy(bg, color_to_asciirgb(VAR_NORM_BACK_COLOR));

    /*
     * Split into two cases depending on whether this is a header which
     * contains addresses or not. We may color addresses separately.
     */
    if(is_an_addr_hdr(field)){

	/*
	 * If none of the patterns are for this header, don't bother parsing
	 * and checking each address.
	 */
	if(!any_hdr_color(field))
	  return(0);

	/*
	 * First check for patternless patterns which color whole line.
	 */
	if((color = hdr_color(field, NULL, ps_global->hdr_colors)) != NULL){
	    if(pico_is_good_colorpair(color)){
		ins = gf_line_test_new_ins(ins, value,
					   color_embed(color->fg, color->bg),
					   (2 * RGBLEN) + 4);
		strcpy(fg, color_to_asciirgb(color->fg));
		strcpy(bg, color_to_asciirgb(color->bg));
		did_color++;
	    }
	    else
	      free_color_pair(&color);
	}

	/*
	 * Then go through checking address by address.
	 * Keep track of quotes and watch for color changes, and override
	 * with most recent header color whenever the normal foreground
	 * and background colors are in force.
	 */
	beg = p = value;
	while(*p){
	    switch(*p){
	      case '\\':
		/* skip next character */
		if(*(p+1) && (in_comment || in_quote))
		  p += 2;
		else
		  p++;

		break;

	      case '"':
		if(!in_comment)
		  in_quote = 1 - in_quote;

		p++;
		break;

	      case '(':
		in_comment++;
		p++;
		break;

	      case ')':
		if(in_comment > 0)
		  in_comment--;

		p++;
		break;

	      case ',':
		if(!(in_quote || in_comment)){
		    /* we reached the end of this address */   
		    *p = '\0';
		    if(color)
		      free_color_pair(&color);

		    if((color = hdr_color(field, beg,
					  ps_global->hdr_colors)) != NULL){
			if(pico_is_good_colorpair(color)){
			    did_color++;
			    ins = gf_line_test_new_ins(ins, beg,
						       color_embed(color->fg,
								   color->bg),
						       (2 * RGBLEN) + 4);
			    *p = ',';
			    for(q = p; q > beg && 
				  isspace((unsigned char)*(q-1)); q--)
			      ;

			    ins = gf_line_test_new_ins(ins, q,
						       color_embed(fg, bg),
						       (2 * RGBLEN) + 4);
			}
			else
			  free_color_pair(&color);
		    }
		    else
		      *p = ',';
		    
		    for(p++; isspace((unsigned char)*p); p++)
		      ;
		    
		    beg = p;
		}
		else
		  p++;

		break;
	    
	      case TAG_EMBED:
		switch(*(++p)){
		  case TAG_HANDLE:
		    p++;
		    p += *p + 1;	/* skip handle key */
		    break;

		  case TAG_FGCOLOR:
		    p++;
		    sprintf(rgbbuf, "%.*s", RGBLEN, p);
		    p += RGBLEN;	/* advance past color value */
		  
		    if(!colorcmp(rgbbuf, VAR_NORM_FORE_COLOR))
		      ins = gf_line_test_new_ins(ins, p,
					         color_embed(color->fg,NULL),
					         RGBLEN + 2);
		    break;

		  case TAG_BGCOLOR:
		    p++;
		    sprintf(rgbbuf, "%.*s", RGBLEN, p);
		    p += RGBLEN;	/* advance past color value */
		  
		    if(!colorcmp(rgbbuf, VAR_NORM_BACK_COLOR))
		      ins = gf_line_test_new_ins(ins, p,
					         color_embed(NULL,color->bg),
					         RGBLEN + 2);

		    break;

		  default:
		    break;
	        }

		break;

	      default:
		p++;
		break;
	    }
	}

	for(q = beg; *q && isspace((unsigned char)*q); q++)
	  ;

	if(*q && !(in_quote || in_comment)){
	    /* we reached the end of this address */   
	    if(color)
	      free_color_pair(&color);

	    if((color = hdr_color(field, beg, ps_global->hdr_colors)) != NULL){
		if(pico_is_good_colorpair(color)){
		    did_color++;
		    ins = gf_line_test_new_ins(ins, beg,
					       color_embed(color->fg,
							   color->bg),
					       (2 * RGBLEN) + 4);
		    for(q = p; q > beg && isspace((unsigned char)*(q-1)); q--)
		      ;

		    ins = gf_line_test_new_ins(ins, q,
					       color_embed(fg, bg),
					       (2 * RGBLEN) + 4);
		}
		else
		  free_color_pair(&color);
	    }
	}

	if(color)
	  free_color_pair(&color);

	if(did_color)
	  ins = gf_line_test_new_ins(ins, line + strlen(line),
				     color_embed(VAR_NORM_FORE_COLOR,
					         VAR_NORM_BACK_COLOR),
				     (2 * RGBLEN) + 4);
    }
    else{

	color = hdr_color(field, value, ps_global->hdr_colors);

	if(color){
	    if(pico_is_good_colorpair(color)){
		ins = gf_line_test_new_ins(ins, value,
					   color_embed(color->fg, color->bg),
					   (2 * RGBLEN) + 4);

		/*
		 * Loop watching colors, and override with header
		 * color whenever the normal foreground and background
		 * colors are in force.
		 */
		p = value;
		while(*p)
		  if(*p++ == TAG_EMBED){

		      switch(*p++){
			case TAG_HANDLE:
			  p += *p + 1;	/* skip handle key */
			  break;

			case TAG_FGCOLOR:
			  sprintf(rgbbuf, "%.*s", RGBLEN, p);
			  p += RGBLEN;	/* advance past color value */
			  
			  if(!colorcmp(rgbbuf, VAR_NORM_FORE_COLOR)
			     && !colorcmp(bg, VAR_NORM_BACK_COLOR))
			    ins = gf_line_test_new_ins(ins, p,
						       color_embed(color->fg,NULL),
						       RGBLEN + 2);
			  break;

			case TAG_BGCOLOR:
			  sprintf(rgbbuf, "%.*s", RGBLEN, p);
			  p += RGBLEN;	/* advance past color value */
			  
			  if(!colorcmp(rgbbuf, VAR_NORM_BACK_COLOR)
			     && !colorcmp(fg, VAR_NORM_FORE_COLOR))
			    ins = gf_line_test_new_ins(ins, p,
						       color_embed(NULL,color->bg),
						       RGBLEN + 2);

			  break;

			default:
			  break;
		      }
		  }

		ins = gf_line_test_new_ins(ins, line + strlen(line),
					   color_embed(VAR_NORM_FORE_COLOR,
						       VAR_NORM_BACK_COLOR),
					   (2 * RGBLEN) + 4);
	    }

	    free_color_pair(&color);
	}
    }


    return(0);
}


int
url_hilite(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    register char *lp, *up = NULL, *urlp = NULL,
		  *weburlp = NULL, *mailurlp = NULL;
    int		   n, n1, n2, n3, l;
    char	   buf[256], color[256];
    HANDLE_S	  *h;

    for(lp = line; ; lp = up + n){
	/* scan for all of them so we can choose the first */
	if(F_ON(F_VIEW_SEL_URL,ps_global))
	  urlp = rfc1738_scan(lp, &n1);
	if(F_ON(F_VIEW_SEL_URL_HOST,ps_global))
	  weburlp = web_host_scan(lp, &n2);
	if(F_ON(F_SCAN_ADDR,ps_global))
	  mailurlp = mail_addr_scan(lp, &n3);
	
	if(urlp || weburlp || mailurlp){
	    up = urlp ? urlp : 
		  weburlp ? weburlp : mailurlp;
	    if(up == urlp && weburlp && weburlp < up)
	      up = weburlp;
	    if(mailurlp && mailurlp < up)
	      up = mailurlp;

	    if(up == urlp){
		n = n1;
		weburlp = mailurlp = NULL;
	    }
	    else if(up == weburlp){
		n = n2;
		mailurlp = NULL;
	    }
	    else{
		n = n3;
		weburlp = NULL;
	    }
	}
	else
	  break;

	h	      = new_handle((HANDLE_S **)local);
	h->type	      = URL;
	h->h.url.path = (char *) fs_get((n + 10) * sizeof(char));
	sprintf(h->h.url.path, "%s%.*s",
		weburlp ? "http://" : (mailurlp ? "mailto:" : ""), n, up);

	if(scroll_handle_start_color(color, &l))
	  ins = gf_line_test_new_ins(ins, up, color, l);
	else
	  ins = gf_line_test_new_ins(ins, up, url_embed(TAG_BOLDON), 2);

	buf[0] = TAG_EMBED;
	buf[1] = TAG_HANDLE;
	sprintf(&buf[3], "%d", h->key);
	buf[2] = strlen(&buf[3]);
	ins = gf_line_test_new_ins(ins, up, buf, (int) buf[2] + 3);

	/* in case it was the current selection */
	ins = gf_line_test_new_ins(ins, up + n, url_embed(TAG_INVOFF), 2);

	if(scroll_handle_end_color(color, &l))
	  ins = gf_line_test_new_ins(ins, up + n, color, l);
	else
	  ins = gf_line_test_new_ins(ins, up + n, url_embed(TAG_BOLDOFF), 2);

	urlp = weburlp = mailurlp = NULL;
    }

    return(0);
}


int
url_hilite_hdr(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    static int     check_for_urls = 0;
    register char *lp;

    if(isspace((unsigned char)*line))	/* continuation, check or not 
					   depending on last line */
      lp = line;
    else{
	check_for_urls = 0;
	if(lp = strchr(line, ':')){	/* there ought to always be a colon */
	    FieldType ft;

	    *lp = '\0';

	    if(((ft = pine_header_standard(line)) == FreeText
		|| ft == Subject
		|| ft == TypeUnknown)
	       && strucmp(line, "message-id")
	       && strucmp(line, "newsgroups")
	       && strucmp(line, "references")
	       && strucmp(line, "in-reply-to")
	       && strucmp(line, "received")
	       && strucmp(line, "date")){
		check_for_urls = 1;
	    }

	    *lp = ':';
	}
    }

    if(check_for_urls)
      (void) url_hilite(linenum, lp + 1, ins, local);

    return(0);
}



int
url_hilite_abook(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    register char *lp;

    if((lp = strchr(line, ':')) &&
       !strncmp(line, AB_COMMENT_STR, strlen(AB_COMMENT_STR)))
      (void) url_hilite(linenum, lp + 1, ins, local);

    return(0);
}



/*
 * url_launch - Sniff the given url, see if we can do anything with
 *		it.  If not, hand off to user-defined app.
 *
 */
int
url_launch(handle)
    HANDLE_S *handle;
{
    int	       rv = 0;
    url_tool_t f;
#define	URL_MAX_LAUNCH	(2 * MAILTMPLEN)

    if(handle->h.url.tool){
	char	*toolp, *cmdp, *p, *q, cmd[URL_MAX_LAUNCH + 4];
	char    *left_double_quote, *right_double_quote;
	int	 mode, quotable = 0, copied = 0, double_quoted = 0;
	int      escape_single_quotes = 0;
	PIPE_S  *syspipe;

	toolp = handle->h.url.tool;

	/*
	 * Figure out if we need to quote the URL. If there are shell
	 * metacharacters in it we want to quote it, because we don't want
	 * the shell to interpret them. However, if the user has already
	 * quoted the URL in the command definition we don't want to quote
	 * again. So, we try to see if there are a pair of unescaped
	 * quotes surrounding _URL_ in the cmd.
	 * If we quote when we shouldn't have, it'll cause it not to work.
	 * If we don't quote when we should have, it's a possible security
	 * problem (and it still won't work).
	 *
	 * In bash and ksh $( executes a command, so we use single quotes
	 * instead of double quotes to do our quoting. If configured command
	 * is double-quoted we change that to single quotes.
	 */
#ifdef	_WINDOWS
	/*
	 * It should be safe to not quote any of the characters from the
	 * string below.  It was quoting with '?' and '&' in a URL, which is
	 * unnecessary.  Also the quoting code below only quotes with
	 * ' (single quote), so we'd want it to at least do double quotes
	 * instead, for Windows.
	 */
	if(toolp)
	  quotable = 0;		/* always never quote */
	else
#endif
	/* quote shell specials */
	if(strpbrk(handle->h.url.path, "&*;<>?[]|~$(){}") != NULL){
	    escape_single_quotes++;
	    if((p = strstr(toolp, "_URL_")) != NULL){  /* explicit arg? */
		int in_quote = 0;

		/* see whether or not it is already quoted */

	        quotable = 1;

		for(q = toolp; q < p; q++)
		  if(*q == '\'' && (q == toolp || q[-1] != '\\'))
		    in_quote = 1 - in_quote;
		
		if(in_quote){
		    for(q = p+5; *q; q++)
		      if(*q == '\'' && q[-1] != '\\'){
			  /* already single quoted, leave it alone */
			  quotable = 0;
			  break;
		      }
		}

		if(quotable){
		    in_quote = 0;
		    for(q = toolp; q < p; q++)
		      if(*q == '\"' && (q == toolp || q[-1] != '\\')){
			  in_quote = 1 - in_quote;
			  if(in_quote)
			    left_double_quote = q;
		      }
		    
		    if(in_quote){
			for(q = p+5; *q; q++)
			  if(*q == '\"' && q[-1] != '\\'){
			      /* we'll replace double quotes with singles */
			      double_quoted = 1;
			      right_double_quote = q;
			      break;
			  }
		    }
		}
	    }
	    else
	      quotable = 1;
	}
	else
	  quotable = 0;

	/* Build the command */
	cmdp = cmd;
	while(cmdp-cmd < URL_MAX_LAUNCH)
	  if((!*toolp && !copied)
	     || (*toolp == '_' && !strncmp(toolp + 1, "URL_", 4))){

	      /* implicit _URL_ at end */
	      if(!*toolp)
		*cmdp++ = ' ';

	      /* add single quotes */
	      if(quotable && !double_quoted && cmdp-cmd < URL_MAX_LAUNCH)
		*cmdp++ = '\'';

	      copied = 1;
	      /*
	       * If the url.path contains a single quote we should escape
	       * that single quote to remove its special meaning.
	       * Since backslash-quote is ignored inside single quotes we
	       * close the quotes, backslash escape the quote, then open
	       * the quotes again. So
	       *         'fred's car'
	       * becomes 'fred'\''s car'
	       */
	      for(p = handle->h.url.path;
		  p && *p && cmdp-cmd < URL_MAX_LAUNCH; p++){
		  if(escape_single_quotes && *p == '\''){
		      *cmdp++ = '\'';	/* closing quote */
		      *cmdp++ = '\\';
		      *cmdp++ = '\'';	/* opening quote comes from p below */
		  }

		  *cmdp++ = *p;
	      }

	      if(quotable && !double_quoted && cmdp-cmd < URL_MAX_LAUNCH){
		  *cmdp++ = '\'';
		  *cmdp = '\0';
	      }

	      if(*toolp)
		toolp += 5;		/* length of "_URL_" */
	  }
	  else{
	      /* replace double quotes with single quotes */
	      if(double_quoted &&
		 (toolp == left_double_quote || toolp == right_double_quote)){
		  *cmdp++ = '\'';
		  toolp++;
	      }
	      else if(!(*cmdp++ = *toolp++))
		break;
	  }


	if(cmdp-cmd >= URL_MAX_LAUNCH)
	  return(url_launch_too_long(rv));
	
	mode = PIPE_RESET | PIPE_USER | PIPE_RUNNOW ;
	if(syspipe = open_system_pipe(cmd, NULL, NULL, mode, 0)){
	    close_system_pipe(&syspipe, NULL, 0);
	    q_status_message(SM_ORDER, 0, 4, "VIEWER command completed");
	}
	else
	  q_status_message1(SM_ORDER, 3, 4,
			    "Cannot spawn command : %.200s", cmd);
    }
    else if(f = url_local_handler(handle->h.url.path)){
	if((*f)(handle->h.url.path) > 1)
	  rv = 1;		/* done! */
    }
    else
      q_status_message1(SM_ORDER, 2, 2,
			"\"URL-Viewer\" not defined: Can't open %.200s",
			handle->h.url.path);
    
    return(rv);
}


int
url_launch_too_long(return_value)
    int return_value;
{
    q_status_message(SM_ORDER | SM_DING, 3, 3,
		     "Can't spawn.  Command too long.");
    return(return_value);
}


char *
url_external_handler(handle, specific)
    HANDLE_S *handle;
    int	      specific;
{
    char **l, *test, *cmd, *p, *q, *ep;
    int	   i, specific_match;

    for(l = ps_global->VAR_BROWSER ; l && *l; l++){
	get_pair(*l, &test, &cmd, 0, 1);
	dprint(5, (debugfile, "TEST: \"%s\"  CMD: \"%s\"\n",
		   test ? test : "<NULL>", cmd ? cmd : "<NULL>"));
	removing_quotes(cmd);
	if(valid_filter_command(&cmd)){
	    specific_match = 0;

	    if(p = test){
		while(*p && cmd)
		  if(*p == '_'){
		      if(!strncmp(p+1, "TEST(", 5)
			 && (ep = strstr(p+6, ")_"))){
			  *ep = '\0';

			  if(exec_mailcap_test_cmd(p+6) == 0){
			      p = ep + 2;
			  }
			  else{
			      dprint(5, (debugfile,"failed handler TEST\n"));
			      fs_give((void **) &cmd);
			  }
		      }
		      else if(!strncmp(p+1, "SCHEME(", 7)
			      && (ep = strstr(p+8, ")_"))){
			  *ep = '\0';

			  p += 8;
			  do
			    if(q = strchr(p, ','))
			      *q++ = '\0';
			    else
			      q = ep;
			  while(!((i = strlen(p))
				  && ((p[i-1] == ':'
				       && handle->h.url.path[i - 1] == ':')
				      || (p[i-1] != ':'
					  && handle->h.url.path[i] == ':'))
				  && !struncmp(handle->h.url.path, p, i))
				&& *(p = q));

			  if(*p){
			      specific_match = 1;
			      p = ep + 2;
			  }
			  else{
			      dprint(5, (debugfile,"failed handler SCHEME\n"));
			      fs_give((void **) &cmd);
			  }
		      }
		      else{
			  dprint(5, (debugfile, "UNKNOWN underscore test\n"));
			  fs_give((void **) &cmd);
		      }
		  }
		  else if(isspace((unsigned char) *p)){
		      p++;
		  }
		  else{
		      dprint(1, (debugfile,"bogus handler test: \"%s\"",
			     test ? test : "?"));
		      fs_give((void **) &cmd);
		  }

		fs_give((void **) &test);
	    }

	    if(cmd && (!specific || specific_match))
	      return(cmd);
	}

	if(test)
	  fs_give((void **) &test);

	if(cmd)
	  fs_give((void **) &cmd);
    }

    cmd = NULL;

    if(!specific){
	cmd = url_os_specified_browser(handle->h.url.path);
	/*
	 * Last chance, anything handling "text/html" in mailcap...
	 */
	if(!cmd && mailcap_can_display(TYPETEXT, "html", NULL, 0)){
	    MCAP_CMD_S *mc_cmd;

	    mc_cmd = mailcap_build_command(TYPETEXT, "html",
					   NULL, "_URL_", NULL, 0);
	    /* 
	     * right now URL viewing won't return anything requiring
	     * special handling
	     */
	    if(mc_cmd){
		cmd = mc_cmd->command;
		fs_give((void **)&mc_cmd);
	    }
	}
    }

    return(cmd);
}



#define	UES_LEN	12
#define	UES_MAX	32
int
url_external_specific_handler(url, len)
    char *url;
    int   len;
{
    static char list[UES_LEN * UES_MAX];

    if(url){
	char *p;
	int   i;

	for(i = 0; i < UES_MAX && *(p = &list[i * UES_LEN]); i++)
	  if(strlen(p) == len && !struncmp(p, url, len))
	    return(1);
    }
    else{					/* initialize! */
	char **l, *test, *cmd, *p, *p2;
	int    i = 0, n;

	memset(list, 0, UES_LEN * UES_MAX * sizeof(char));
	for(l = ps_global->VAR_BROWSER ; l && *l; l++){
	    get_pair(*l, &test, &cmd, 1, 1);

	    if((p = srchstr(test, "_scheme(")) && (p2 = strstr(p+8, ")_"))){
		*p2 = '\0';

		for(p += 8; *p && i < UES_MAX; p += n)
		  if(p2 = strchr(p, ',')){
		      if((n = p2 - p) < UES_LEN)
			strncpy(&list[i++ * UES_LEN], p, n);
		      else
			dprint(1, (debugfile,
				   "* * * HANLDER TOO LONG: %.*s\n", n,
				   p ? p : "?"));

		      n++;
		  }
		  else{
		      if(strlen(p) <= UES_LEN)
			strcpy(&list[i++ * UES_LEN], p);

		      break;
		  }
	    }

	    if(test)
	      fs_give((void **) &test);

	    if(cmd)
	      fs_give((void **) &cmd);
	}
    }

    return(0);
}



url_tool_t
url_local_handler(s)
    char *s;
{
    int i;
    static struct url_t {
	char       *url;
	short	    len;
	url_tool_t  f;
    } handlers[] = {
	{"mailto:", 7, url_local_mailto},	/* see url_tool_t def's */
	{"imap://", 7, url_local_imap},		/* for explanations */
	{"nntp://", 7, url_local_nntp},
	{"file://", 7, url_local_file},
#ifdef	ENABLE_LDAP
	{"ldap://", 7, url_local_ldap},
#endif
	{"news:", 5, url_local_news},
	{"x-pine-gripe:", 13, gripe_gripe_to},
	{"x-pine-help:", 11, url_local_helper},
	{"x-pine-phone-home:", 18, url_local_phone_home},
	{"x-pine-config:", 14, url_local_config},
	{"x-pine-cert:", 12, url_local_certdetails},
	{"#", 1, url_local_fragment},
	{NULL, 0, NULL}
    };

    for(i = 0; handlers[i].url ; i++)
      if(!struncmp(s, handlers[i].url, handlers[i].len))
	return(handlers[i].f);

    return(NULL);
}



/*
 * mailto URL digester ala draft-hoffman-mailto-url-02.txt
 */
int
url_local_mailto(url)
  char *url;
{
    return(url_local_mailto_and_atts(url, NULL));
}

int
url_local_mailto_and_atts(url, attachlist)
  char *url;
  PATMT *attachlist;
{
    ENVELOPE *outgoing;
    BODY     *body = NULL;
    REPLY_S   fake_reply;
    char     *sig, *urlp, *p, *hname, *hvalue;
    int	      rv = 0;
    long      rflags;
    int       was_a_body = 0, impl, template_len = 0;
    char     *fcc = NULL;
    PAT_STATE dummy;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S  *role = NULL;

    outgoing		 = mail_newenvelope();
    outgoing->message_id = generate_message_id();
    body		 = mail_newbody();
    body->type		 = TYPETEXT;
    if(body->contents.text.data = (void *) so_get(PicoText,NULL,EDIT_ACCESS)){
	/*
	 * URL format is:
	 *
	 *	mailtoURL  =  "mailto:" [ to ] [ headers ]
	 *	to         =  #mailbox
	 *	headers    =  "?" header *( "&" header )
	 *	header     =  hname "=" hvalue
	 *	hname      =  *urlc
	 *	hvalue     =  *urlc
	 *
	 * NOTE2: "from" and "bcc" are intentionally excluded from
	 *	  the list of understood "header" fields
	 */
	if(p = strchr(urlp = cpystr(url+7), '?'))
	  *p++ = '\0';			/* headers?  Tie off mailbox */

	/* grok mailbox as first "to", then roll thru specific headers */
	if(*urlp)
	  rfc822_parse_adrlist(&outgoing->to,
			       rfc1738_str(urlp),
			       ps_global->maildomain);

	while(p){
	    /* Find next "header" */
	    if(p = strchr(hname = p, '&'))
	      *p++ = '\0';		/* tie off "header" */

	    if(hvalue = strchr(hname, '='))
	      *hvalue++ = '\0';		/* tie off hname */

	    if(!hvalue || !strucmp(hname, "subject")){
		char *sub = rfc1738_str(hvalue ? hvalue : hname);

		if(outgoing->subject){
		    int len = strlen(outgoing->subject);

		    fs_resize((void **) &outgoing->subject,
			      (len + strlen(sub) + 2) * sizeof(char));
		    sprintf(outgoing->subject + len, " %s", sub);
		}
		else
		  outgoing->subject = cpystr(sub);
	    }
	    else if(!strucmp(hname, "to")){
		url_mailto_addr(&outgoing->to, hvalue);
	    }
	    else if(!strucmp(hname, "cc")){
		url_mailto_addr(&outgoing->cc, hvalue);
	    }
	    else if(!strucmp(hname, "bcc")){
		q_status_message(SM_ORDER, 3, 4,
				 "\"Bcc\" header in mailto url ignored");
	    }
	    else if(!strucmp(hname, "from")){
		q_status_message(SM_ORDER, 3, 4,
				 "\"From\" header in mailto url ignored");
	    }
	    else if(!strucmp(hname, "body")){
		char *sub = rfc1738_str(hvalue ? hvalue : "");

		so_puts((STORE_S *)body->contents.text.data, sub);
		so_puts((STORE_S *)body->contents.text.data, NEWLINE);
		was_a_body++;
	    }
	}

	fs_give((void **) &urlp);

	rflags = ROLE_COMPOSE;
	if(nonempty_patterns(rflags, &dummy)){
	    role = set_role_from_msg(ps_global, rflags, -1L, NULL);
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{			/* cancel */
		role = NULL;
		cmd_cancelled("Composition");
		goto outta_here;
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4, "Composing using role \"%.200s\"",
			    role->nick);

	if(!was_a_body && role && role->template){
	    char *filtered;

	    impl = 1;	/* leave cursor in header if not explicit */
	    filtered = detoken(role, NULL, 0, 0, 0, &redraft_pos, &impl);
	    if(filtered){
		if(*filtered){
		    so_puts((STORE_S *)body->contents.text.data, filtered);
		    if(impl == 1)
		      template_len = strlen(filtered);
		}
		
		fs_give((void **)&filtered);
	    }
	}
	else
	  impl = 1;

	if(!was_a_body && (sig = detoken(role, NULL, 2, 0, 1, &redraft_pos,
					 &impl))){
	    if(impl == 2)
	      redraft_pos->offset += template_len;

	    if(*sig)
	      so_puts((STORE_S *)body->contents.text.data, sig);

	    fs_give((void **)&sig);
	}

	memset((void *)&fake_reply, 0, sizeof(fake_reply));
	fake_reply.flags = REPLY_PSEUDO;
	fake_reply.data.pico_flags = (outgoing->subject) ? P_BODY : P_HEADEND;


	if(!(role && role->fcc))
	  fcc = get_fcc_based_on_to(outgoing->to);

	if(attachlist)
	  create_message_body(&body, attachlist, NULL, 0);

	pine_send(outgoing, &body, "\"MAILTO\" COMPOSE",
		  role, fcc, &fake_reply, redraft_pos, NULL, NULL, 0);
	rv++;
	ps_global->mangled_screen = 1;
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 4,
		       "Can't create space for composer");

outta_here:
    if(outgoing)
      mail_free_envelope(&outgoing);

    if(body)
      pine_free_body(&body);
    
    if(fcc)
      fs_give((void **)&fcc);
    
    free_redraft_pos(&redraft_pos);
    free_action(&role);

    return(rv);
}



void
url_mailto_addr(a, a_raw)
    ADDRESS	**a;
    char	 *a_raw;
{
    char *p = cpystr(rfc1738_str(a_raw));

    while(*a)				/* append to address list */
      a = &(*a)->next;

    rfc822_parse_adrlist(a, p, ps_global->maildomain);
    fs_give((void **) &p);
}


/*
 * imap URL digester ala RFC 2192
 */
int
url_local_imap(url)
    char *url;
{
    char      *folder, *mailbox = NULL, *errstr = NULL, *search = NULL,
	       newfolder[MAILTMPLEN];
    int	       rv;
    long       uid = 0L, uid_val = 0L, i;
    CONTEXT_S *fake_context;
    MESSAGECACHE *mc;

    rv = url_imap_folder(url, &folder, &uid, &uid_val, &search, 0);
    switch(rv & URL_IMAP_MASK){
      case URL_IMAP_IMAILBOXLIST :
/* BUG: deal with lsub tag */
	if(fake_context = new_context(folder, NULL)){
	    newfolder[0] = '\0';
	    if(display_folder_list(&fake_context, newfolder,
				   0, folders_for_goto))
	      if(strlen(newfolder) + 1 < MAILTMPLEN)
		mailbox = newfolder;
	}
	else
	  errstr = "Problem building URL's folder list";

	fs_give((void **) &folder);
	free_context(&fake_context);

	if(mailbox){
	    return(1);
	}
	else if(errstr)
	  q_status_message(SM_ORDER|SM_DING, 3, 3, errstr);
	else
	  cmd_cancelled("URL Launch");

	break;

      case URL_IMAP_IMESSAGEPART :
      case URL_IMAP_IMESSAGELIST :
	rv = do_broach_folder(folder, NULL, NULL, 0L);
	fs_give((void **) &folder);
	switch(rv){
	  case -1 :				/* utter failure */
	    ps_global->next_screen = main_menu_screen;
	    break;

	  case 0 :				/* same folder reopened */
	    ps_global->next_screen = mail_index_screen;
	    break;

	  case 1 :				/* requested folder open */
	    ps_global->next_screen = mail_index_screen;

	    if(uid_val != ps_global->mail_stream->uid_validity){
		/* Complain! */
		q_status_message(SM_ORDER|SM_DING, 3, 3,
		     "Warning!  Referenced folder changed since URL recorded");
	    }

	    if(uid){
		/*
		 * Make specified message the currently selected..
		 */
		for(i = 1L; i <= mn_get_total(ps_global->msgmap); i++)
		  if(mail_uid(ps_global->mail_stream, i) == uid){
		      ps_global->next_screen = mail_view_screen;
		      mn_set_cur(ps_global->msgmap, i);
		      break;
		  }

		if(i > mn_get_total(ps_global->msgmap))
		  q_status_message(SM_ORDER, 2, 3,
				   "Couldn't find specified article number");
	    }
	    else if(search){
		/*
		 * Select the specified messages
		 * and present a zoom'd index...
		 */
/* BUG: not dealing with CHARSET yet */

/* ANOTHER BUG: mail_criteria is a compatibility routine for IMAP2BIS
 * so it doesn't know about IMAP4 search criteria, like SENTSINCE.
 * It also doesn't handle literals. */

		pine_mail_search_full(ps_global->mail_stream, NULL,
				      mail_criteria(search),
				      SE_NOPREFETCH | SE_FREE);

		for(i = 1L; i <= mn_get_total(ps_global->msgmap); i++)
		  if(ps_global->mail_stream
		     && i <= ps_global->mail_stream->nmsgs
		     && (mc = mail_elt(ps_global->mail_stream, i))
		     && mc->searched)
		    set_lflag(ps_global->mail_stream,
			      ps_global->msgmap, i, MN_SLCT, 1);

		if(i = any_lflagged(ps_global->msgmap, MN_SLCT)){

		    q_status_message2(SM_ORDER, 0, 3,
				      "%.200s message%.200s selected",
				      long2string(i), plural(i));
		    /* Zoom the index! */
		    zoom_index(ps_global, ps_global->mail_stream,
			       ps_global->msgmap);
		}
	    }
	}

	return(1);

      default:
      case URL_IMAP_ERROR :
	break;
    }

    return(0);
}



int
url_imap_folder(true_url, folder, uid_val, uid, search, silent)
  char  *true_url, **folder;
  long  *uid_val, *uid;
  char **search;
  int	 silent;
{
    char *url, *scheme, *p, *cmd, *server = NULL,
	 *user = NULL, *auth = NULL, *mailbox = NULL,
	 *section = NULL;
    int   rv = URL_IMAP_ERROR;

    /*
     * Since we're planting nulls, operate on a temporary copy...
     */
    scheme = silent ? NULL : "IMAP";
    url = cpystr(true_url + 7);

    /* Try to pick apart the "iserver" portion */
    if(cmd = strchr(url, '/')){			/* iserver "/" [mailbox] ? */
	*cmd++ = '\0';
    }
    else{
	dprint(2, (debugfile, "-- URL IMAP FOLDER: missing: %s\n",
	       url ? url : "?"));
	cmd = &url[strlen(url)-1];	/* assume only iserver */
    }

    if(p = strchr(url, '@')){		/* user | auth | pass? */
	*p++ = '\0';
	server = rfc1738_str(p);
	
	/* only ";auth=*" supported (and also ";auth=anonymous") */
	if(p = srchstr(url, ";auth=")){
	    *p = '\0';
	    auth = rfc1738_str(p + 6);
	}

	if(*url)
	  user = rfc1738_str(url);
    }
    else
      server = rfc1738_str(url);

    if(!*server)
      return(url_bogus_imap(&url, scheme, "No server specified"));

    /*
     * "iserver" in hand, pick apart the "icommand"...
     */
    p = NULL;
    if(!*cmd || (p = srchstr(cmd, ";type="))){
	char *criteria;

	/*
	 * No "icommand" (all top-level folders) or "imailboxlist"...
	 */
	if(p){
	    *p	 = '\0';		/* tie off criteria */
	    criteria = rfc1738_str(cmd);	/* get "enc_list_mailbox" */
	    if(!strucmp(p = rfc1738_str(p+6), "lsub"))
	      rv |= URL_IMAP_IMBXLSTLSUB;
	    else if(strucmp(p, "list"))
	      return(url_bogus_imap(&url, scheme,
				    "Invalid list type specified"));
	}
	else{
	    rv |= URL_IMAP_ISERVERONLY;
	    criteria = "";
	}
		
	/* build folder list from specified server/criteria/list-method */
	*folder = (char *) fs_get((strlen(server) + strlen(criteria) + 11
			     + (user ? (strlen(user)+2) : 9)) * sizeof(char));
	sprintf(*folder, "{%s/%s%s%s}%s%s%s", server,
		user ? "user=\"" : "Anonymous",
		user ? user : "",
		user ? "\"" : "",
		*criteria ? "[" : "", criteria, *criteria ? "[" : "");
	rv |= URL_IMAP_IMAILBOXLIST;
    }
    else{
	if(p = srchstr(cmd, "/;uid=")){ /* "imessagepart" */
	    *p = '\0';		/* tie off mailbox [uidvalidity] */
	    if(section = srchstr(p += 6, "/;section=")){
		*section = '\0';	/* tie off UID */
		section  = rfc1738_str(section + 10);
/* BUG: verify valid section spec ala rfc 2060 */
		dprint(2, (debugfile,
			   "-- URL IMAP FOLDER: section not used: %s\n",
			   section ? section : "?"));
	    }

	    if(!(*uid = rfc1738_num(&p)) || *p) /* decode UID */
	      return(url_bogus_imap(&url, scheme, "Invalid data in UID"));

	    /* optional "uidvalidity"? */
	    if(p = srchstr(cmd, ";uidvalidity=")){
		*p  = '\0';
		p  += 13;
		if(!(*uid_val = rfc1738_num(&p)) || *p)
		  return(url_bogus_imap(&url, scheme,
					"Invalid UIDVALIDITY"));
	    }

	    mailbox = rfc1738_str(cmd);
	    rv = URL_IMAP_IMESSAGEPART;
	}
	else{			/* "imessagelist" */
	    /* optional "uidvalidity"? */
	    if(p = srchstr(cmd, ";uidvalidity=")){
		*p  = '\0';
		p  += 13;
		if(!(*uid_val = rfc1738_num(&p)) || *p)
		  return(url_bogus_imap(&url, scheme,
					"Invalid UIDVALIDITY"));
	    }

	    /* optional "enc_search"? */
	    if(p = strchr(cmd, '?')){
		*p = '\0';
		if(search)
		  *search = cpystr(rfc1738_str(p + 1));
/* BUG: verify valid search spec ala rfc 2060 */
	    }

	    mailbox = rfc1738_str(cmd);
	    rv = URL_IMAP_IMESSAGELIST;
	}

	if(auth && *auth != '*' && strucmp(auth, "anonymous"))
	  q_status_message(SM_ORDER, 3, 3,
	     "Unsupported authentication method.  Using standard login.");

	/*
	 * At this point our structure should contain the
	 * digested url.  Now put it together for c-client...
	 */
	*folder = (char *) fs_get((strlen(server) + 9
				   + (mailbox ? strlen(mailbox) : 0)
				   + (user ? (strlen(user)+2) : 9))
				  * sizeof(char));
	sprintf(*folder, "{%s%s%s%s%s}%s%s", server,
		(user || !(auth && strucmp(auth, "anonymous"))) ? "/" : "",
		user ? "user=\"" : ((auth && strucmp(auth, "anonymous")) ? "" : "Anonymous"),
		user ? user : "",
		user ? "\"" : "",
		user ? "" : (strchr(mailbox, '/') ? "/" : ""), mailbox);
    }

    fs_give((void **) &url);
    return(rv);
}


url_bogus_imap(freeme, url, problem)
    char **freeme, *url, *problem;
{
    fs_give((void **) freeme);
    (void) url_bogus(url, problem);
    return(URL_IMAP_ERROR);
}



int
url_local_nntp(url)
    char *url;
{
    char folder[2*MAILTMPLEN], *group;
    int  group_len;
    long i, article_num;

	/* no hostport, no url, end of story */
	if(group = strchr(url + 7, '/')){
	    group++;
	    for(group_len = 0; group[group_len] && group[group_len] != '/';
		group_len++)
	      if(!rfc1738_group(&group[group_len]))
		return(url_bogus(url, "Invalid newsgroup specified"));
	      
	    if(group_len)
	      sprintf(folder, "{%.*s/nntp}#news.%.*s",
		      min((group - 1) - (url + 7), MAILTMPLEN-20), url + 7,
		      min(group_len, MAILTMPLEN-20), group);
	    else
	      return(url_bogus(url, "No newsgroup specified"));
	}
	else
	  return(url_bogus(url, "No server specified"));

	switch(do_broach_folder(rfc1738_str(folder), NULL, NULL, 0L)){
	  case -1 :				/* utter failure */
	    ps_global->next_screen = main_menu_screen;
	    break;

	  case 0 :				/* same folder reopened */
	    ps_global->next_screen = mail_index_screen;
	    break;

	  case 1 :				/* requested folder open */
	    ps_global->next_screen = mail_index_screen;

	    /* grok article number --> c-client UID */
	    if(group[group_len++] == '/'
	       && (article_num = atol(&group[group_len]))){
		/*
		 * Make the requested article our current message
		 */
		for(i = 1; i <= mn_get_nmsgs(ps_global->msgmap); i++)
		  if(mail_uid(ps_global->mail_stream, i) == article_num){
		      ps_global->next_screen = mail_view_screen;
		      if(i = mn_raw2m(ps_global->msgmap, i))
			mn_set_cur(ps_global->msgmap, i);
		      break;
		  }

		if(i == 0 || i > mn_get_total(ps_global->msgmap))
		  q_status_message(SM_ORDER, 2, 3,
				   "Couldn't find specified article number");
	    }

	    break;
	}

	ps_global->redrawer = (void(*)())NULL;
	return(1);
}



int
url_local_news(url)
    char *url;
{
    char       folder[MAILTMPLEN], *p;
    CONTEXT_S *cntxt = NULL;

    /*
     * NOTE: NO SUPPORT for '*' or message-id
     */
    if(*(url+5)){
	if(*(url+5) == '/' && *(url+6) == '/')
	  return(url_local_nntp(url)); /* really meant "nntp://"? */

	if(ps_global->VAR_NNTP_SERVER && ps_global->VAR_NNTP_SERVER[0])
	  /*
	   * BUG: Only the first NNTP server is tried.
	   */
	  sprintf(folder, "{%s/nntp}#news.", ps_global->VAR_NNTP_SERVER[0]);
	else
	  strcpy(folder, "#news.");

	folder[sizeof(folder)-1] = '\0';
	for(p = strncpy(folder + strlen(folder), url + 5, sizeof(folder)-strlen(folder)-1); 
	    *p && rfc1738_group(p);
	    p++)
	  ;

	if(*p)
	  return(url_bogus(url, "Invalid newsgroup specified"));
    }
    else{			/* fish first group from newsrc */
	FOLDER_S  *f;
	int	   alphaorder;

	folder[0] = '\0';

	/* Find first news context */
	for(cntxt = ps_global->context_list;
	    cntxt && !(cntxt->use & CNTXT_NEWS);
	    cntxt = cntxt->next)
	  ;

	if(cntxt){
	    if(alphaorder = F_OFF(F_READ_IN_NEWSRC_ORDER, ps_global))
	      F_SET(F_READ_IN_NEWSRC_ORDER, ps_global, 1);

	    build_folder_list(NULL, cntxt, NULL, NULL, BFL_LSUB);
	    if(f = folder_entry(0, FOLDERS(cntxt))){
		strncpy(folder, f->name, sizeof(folder));
		folder[sizeof(folder)-1] = '\0';
	    }

	    free_folder_list(cntxt);

	    if(alphaorder)
	      F_SET(F_READ_IN_NEWSRC_ORDER, ps_global, 0);
	}

	if(folder[0] == '\0'){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "No default newsgroup");
	    return(0);
	}
    }

    if(do_broach_folder(rfc1738_str(folder), cntxt, NULL, 0L) < 0)
      ps_global->next_screen = main_menu_screen;
    else
      ps_global->next_screen = mail_index_screen;

    ps_global->redrawer = (void(*)())NULL;

    return(1);
}

int
url_local_file(file_url)
    char *file_url;
{
    if(want_to(
 "\"file\" URL may cause programs to be run on your system. Run anyway",
               'n', 0, NO_HELP, WT_NORM) == 'y'){
	HANDLE_S handle;

	/* fake a handle */
	handle.h.url.path = file_url;
	if((handle.h.url.tool = url_external_handler(&handle, 1))
	   || (handle.h.url.tool = url_external_handler(&handle, 0))){
	    url_launch(&handle);
	    return 1;
	}
	else{
	    q_status_message(SM_ORDER, 0, 4,
		  "No viewer for \"file\" URL.  VIEWER command cancelled");
	    return 0;
	}
    }
    q_status_message(SM_ORDER, 0, 4, "VIEWER command cancelled");
    return 0;
}


int
url_local_fragment(fragment)
    char *fragment;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    HANDLE_S   *hp;

    /*
     * find a handle with the fragment's name
     */
    for(hp = st->parms->text.handles; hp; hp = hp->next)
      if(hp->type == URL && hp->h.url.name
	 && !strcmp(hp->h.url.name, fragment + 1))
	break;

    if(!hp)
      for(hp = st->parms->text.handles->prev; hp; hp = hp->prev)
	if(hp->type == URL && hp->h.url.name
	   && !strcmp(hp->h.url.name, fragment + 1))
	  break;

    /*
     * set the top line of the display to contain this line
     */
    if(hp && hp->loc){
	st->top_text_line = hp->loc->where.row;
	ps_global->mangled_body = 1;
    }
    else
      q_status_message1(SM_ORDER | SM_DING, 0, 3,
			"Can't find fragment: %.200s", fragment);

    return(1);
}



int
url_local_phone_home(url)
    char *url;
{
    phone_home(url + 18);	/* 18 == length of "x-pine-phone-home:" */
    return(2);
}



/*
 * url_bogus - report url syntax errors and such
 */
int
url_bogus(url, reason)
    char *url, *reason;
{
    dprint(2, (debugfile, "-- bogus url \"%s\": %s\n",
	       url ? url : "<NULL URL>", reason ? reason : "?"));
    if(url)
      q_status_message3(SM_ORDER|SM_DING, 2, 3,
		        "Malformed \"%.*s\" URL: %.200s",
			(void *) (strchr(url, ':') - url), url, reason);

    return(0);
}



/*----------------------------------------------------------------------
   Handle fetching and filtering a text message segment to be displayed
   by scrolltool or printed.

Args: att   -- segment to fetch
      msgno -- message number segment is a part of
      pc    -- function to write characters from segment with
      style -- Indicates special handling for error messages
      flags -- Indicates special necessary handling

Returns: 1 if errors encountered, 0 if everything went A-OK

 ----*/     
int
decode_text(att, msgno, pc, handlesp, style, flags)
    ATTACH_S       *att;
    long            msgno;
    gf_io_t         pc;
    HANDLE_S      **handlesp;
    DetachErrStyle  style;
    int		    flags;
{
    FILTLIST_S	filters[14];
    char       *err, *charset;
    int		filtcnt = 0, error_found = 0, column, wrapit;
    int         is_in_sig = OUT_SIG_BLOCK;
    int         is_flowed_msg = 0;
    int         is_delsp_yes = 0;
    int         filt_only_c0 = 0;
    char       *parmval;
    CONV_TABLE *ct = NULL;
    char       *free_this = NULL;
    DELQ_S      dq;

    column = (flags & FM_DISPLAY) ? ps_global->ttyo->screen_cols : 80;
    wrapit = column;
    ps_global->some_quoting_was_suppressed = 0;

    memset(filters, 0, sizeof(filters));

    charset = rfc2231_get_param(att->body->parameter, "charset", NULL, NULL);

    /* determined if it's flowed, affects wrapping and quote coloring */
    if(att->body->type == TYPETEXT
       && !strucmp(att->body->subtype, "plain")
       && (parmval = rfc2231_get_param(att->body->parameter,
				       "format", NULL, NULL))){
	if(!strucmp(parmval, "flowed"))
	  is_flowed_msg = 1;
	fs_give((void **) &parmval);

	if(is_flowed_msg){
	    if(parmval = rfc2231_get_param(att->body->parameter,
					   "delsp", NULL, NULL)){
		if(!strucmp(parmval, "yes"))
		  is_delsp_yes = 1;

		fs_give((void **) &parmval);
	    }
	}
    }

    /*
     * This filter changes Japanese characters encoded in 2022-JP to the
     * appropriate encoding for local display. We don't need to do the
     * filtering if the charset isn't 2022-JP, however some mail is sent
     * with improperly labeled 2022-JP so we also filter if it is labeled
     * as ascii or nothing.
     */
    if(F_OFF(F_DISABLE_2022_JP_CONVERSIONS, ps_global)
       && (!charset
	   || !strucmp(charset, "US-ASCII")
	   || !strucmp(charset, "ISO-2022-JP")))
      filters[filtcnt++].filter = gf_2022_jp_to_euc;

    if(charset){
	if(F_OFF(F_DISABLE_CHARSET_CONVERSIONS, ps_global)){

	    ct = conversion_table(charset, ps_global->VAR_CHAR_SET);
	    if(ct && ct->convert && ct->table){
		filters[filtcnt].filter = ct->convert;
		/* we could call an _opt routine here, but why bother? */
		filters[filtcnt++].data = ct->table;
	    }
	}

	fs_give((void **) &charset);
    }

    if(!ps_global->pass_ctrl_chars){
	/*
	 * Escapes are no longer special. The only escape sequence we
	 * recognize is for ISO-2022-JP and it is handled before this.
	 * Escape is just treated like any other control character at
	 * this point.
	 */
	/* filters[filtcnt++].filter = gf_escape_filter; */
	filters[filtcnt].filter = gf_control_filter;

	filt_only_c0 = ps_global->pass_c1_ctrl_chars ? 1 : 0;
	filters[filtcnt++].data = gf_control_filter_opt(&filt_only_c0);
    }

    if(flags & FM_DISPLAY)
      filters[filtcnt++].filter = gf_tag_filter;

    /*
     * if it's just plain old text, look for url's
     */
    if(!(att->body->subtype && strucmp(att->body->subtype, "plain"))){
	struct variable *vars = ps_global->vars;

	if((F_ON(F_VIEW_SEL_URL, ps_global)
	    || F_ON(F_VIEW_SEL_URL_HOST, ps_global)
	    || F_ON(F_SCAN_ADDR, ps_global))
	   && handlesp){

	    /*
	     * The url_hilite filter really ought to come
	     * after flowing, because flowing with the DelSp=yes parameter
	     * can reassemble broken urls back into identifiable urls.
	     * We add the preflow filter to do only the reassembly part
	     * of the flowing so that we can spot the urls.
	     * At this time (2005-03-29) we know that Apple Mail does
	     * send mail like this sometimes. This filter removes the
	     * sequence  SP CRLF  if that seems safe.
	     */
	    if(ps_global->full_header != 2 && is_delsp_yes)
              filters[filtcnt++].filter = gf_preflow;

	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(url_hilite, handlesp);
	}

	/*
	 * First, paint the signature.
	 * Disclaimers noted below for coloring quotes apply here as well.
	 */
	if((flags & FM_DISPLAY)
	   && !(flags & FM_NOCOLOR)
	   && pico_usingcolor()
	   && VAR_SIGNATURE_FORE_COLOR
	   && VAR_SIGNATURE_BACK_COLOR){
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(color_signature,
						       &is_in_sig);
	}

	/*
	 * Gotta be careful with this. The color_a_quote filter adds color
	 * to the beginning and end of the line. This will break some
	 * line_test-style filters which come after it. For example, if they
	 * are looking for something at the start of a line (like color_a_quote
	 * itself). I guess we could fix that by ignoring tags at the
	 * beginning of the line when doing the search.
	 */
	if((flags & FM_DISPLAY)
	   && !(flags & FM_NOCOLOR)
	   && pico_usingcolor()
	   && VAR_QUOTE1_FORE_COLOR
	   && VAR_QUOTE1_BACK_COLOR){
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(color_a_quote,
						       &is_flowed_msg);
	}
    }
    else if(!strucmp(att->body->subtype, "richtext")){
	/* maybe strip everything! */
	filters[filtcnt].filter = gf_rich2plain;
	filters[filtcnt++].data = gf_rich2plain_opt(!(flags&FM_DISPLAY));
	/* width to use for file or printer */
	if(wrapit - 5 > 0)
	  wrapit -= 5;
    }
    else if(!strucmp(att->body->subtype, "enriched")){
	filters[filtcnt].filter = gf_enriched2plain;
	filters[filtcnt++].data = gf_enriched2plain_opt(!(flags&FM_DISPLAY));
	/* width to use for file or printer */
	if(wrapit - 5 > 0)
	  wrapit -= 5;
    }
    else if(!strucmp(att->body->subtype, "html")
	    && ps_global->full_header < 2){
/*BUG:	    sniff the params for "version=2.0" ala draft-ietf-html-spec-01 */
	int opts = 0;

	if(flags & FM_DISPLAY){
	    if(handlesp)		/* pass on handles awareness */
	      opts |= GFHP_HANDLES;
	}
	else
	  opts |= GFHP_STRIPPED;	/* don't embed anything! */

	wrapit = 0;		/* wrap already handled! */
	filters[filtcnt].filter = gf_html2plain;
	filters[filtcnt++].data = gf_html2plain_opt(NULL, column,
						    format_view_margin(),
						    handlesp, opts);
    }

    /*
     * If the message is not flowed, we do the quote suppression before
     * the wrapping, because the wrapping does not preserve the quote
     * characters at the beginnings of the lines in that case.
     * Otherwise, we defer until after the wrapping.
     *
     * Also, this is a good place to do quote-replacement on nonflowed
     * messages because no other filters depend on the "> ".
     * Quote-replacement is easier in the flowed case and occurs
     * automatically in the flowed wrapping filter.
     */
    if(!is_flowed_msg
       && ps_global->full_header == 0
       && !(att->body->subtype && strucmp(att->body->subtype, "plain"))
       && (flags & FM_DISPLAY)){
	if(ps_global->quote_suppression_threshold != 0){
	    memset(&dq, 0, sizeof(dq));
	    dq.lines = ps_global->quote_suppression_threshold;
	    dq.is_flowed = is_flowed_msg;
	    dq.indent_length = 0;		/* indent didn't happen yet */
	    dq.saved_line = &free_this;
	    dq.handlesp   = handlesp;

	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(delete_quotes, &dq);
	}
	if(ps_global->VAR_QUOTE_REPLACE_STRING
	    && F_ON(F_QUOTE_REPLACE_NOFLOW, ps_global)){
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(replace_quotes, NULL);
	}
    }

    if(wrapit && (!(flags & FM_NOWRAP) || ((flags & FM_WRAPFLOWED) && is_flowed_msg))){
	int   margin = 0, wrapflags = (flags & FM_DISPLAY) ? GFW_HANDLES : 0;

	if(flags & FM_DISPLAY
	   && !(flags & FM_NOCOLOR)
	   && pico_usingcolor())
	  wrapflags |= GFW_USECOLOR;

	/* text/flowed (RFC 2646 + draft)? */
	if(ps_global->full_header != 2 && is_flowed_msg){
	    wrapflags |= GFW_FLOWED;

	    if(is_delsp_yes)
	      wrapflags |= GFW_DELSP;
	}

	filters[filtcnt].filter = gf_wrap;
	filters[filtcnt++].data = gf_wrap_filter_opt(wrapit, column,
						     (flags & FM_NOINDENT)
						       ? 0
						       : format_view_margin(),
						     0, wrapflags);
    }

    /*
     * This has to come after wrapping has happened because the user tells
     * us how many quoted lines to display, and we want that number to be
     * the wrapped lines, not the pre-wrapped lines. We do it before the
     * wrapping if the message is not flowed, because the wrapping does not
     * preserve the quote characters at the beginnings of the lines in that
     * case.
     */
    if(is_flowed_msg
       && ps_global->full_header == 0
       && !(att->body->subtype && strucmp(att->body->subtype, "plain"))
       && (flags & FM_DISPLAY)
       && ps_global->quote_suppression_threshold != 0){
	memset(&dq, 0, sizeof(dq));
	dq.lines = ps_global->quote_suppression_threshold;
	dq.is_flowed = is_flowed_msg;
	dq.indent_length = (wrapit && !(flags & FM_NOWRAP)
			    && !(flags & FM_NOINDENT))
			       ? format_view_margin()[0]
			       : 0;
	dq.saved_line = &free_this;
	dq.handlesp   = handlesp;

	filters[filtcnt].filter = gf_line_test;
	filters[filtcnt++].data = gf_line_test_opt(delete_quotes, &dq);
    }

    /*
     * If there's a charset specified and it's not US-ASCII, and our
     * local charset's undefined or it's not the same as the specified
     * charset, put up a warning...
     */
    if(F_OFF(F_QUELL_CHARSET_WARNING, ps_global) &&
       (!ct || ct->quality != CV_NO_TRANSLATE_NEEDED) &&
       (charset = rfc2231_get_param(att->body->parameter,"charset",NULL,NULL))){
	int rv = TRUE;

	if(strucmp(charset, "us-ascii")
	   && (!ps_global->VAR_CHAR_SET
	       || strucmp(charset, ps_global->VAR_CHAR_SET)))
	  rv = charset_editorial(charset, msgno, handlesp, flags,
				 ct ? ct->quality : CV_NO_TRANSLATE_POSSIBLE,
				 column, pc);

	fs_give((void **) &charset);
	if(!rv)
	  goto write_error;
    }
    if(att->body->encoding > ENCQUOTEDPRINTABLE){
	int rv = TRUE;

	rv = encoding_editorial(att->body, column, pc);

	if(!rv)
	  goto write_error;
    }

    err = detach(ps_global->mail_stream, msgno, att->number,
		 NULL, pc, filtcnt ? filters : NULL, 0);
    
    delete_unused_handles(handlesp);
    
    if(free_this)
      fs_give((void **) &free_this);
    
    if(err) {
	error_found++;
	if(style == QStatus) {
	    q_status_message1(SM_ORDER, 3, 4, "%.200s", err);
	} else if(style == InLine) {
	    char buftmp[MAILTMPLEN];

	    sprintf(buftmp, "%.200s",
		    att->body->description ? att->body->description : "");
	    sprintf(tmp_20k_buf, "%s   [Error: %s]  %c%s%c%s%s",
		    NEWLINE, err,
		    att->body->description ? '\"' : ' ',
		    att->body->description ? buftmp : "",
		    att->body->description ? '\"' : ' ',
		    NEWLINE, NEWLINE);
	    if(!gf_puts(tmp_20k_buf, pc))
	      goto write_error;
	}
    }

    if(att->body->subtype
       && (!strucmp(att->body->subtype, "richtext")
	   || !strucmp(att->body->subtype, "enriched"))
       && !(flags & FM_DISPLAY)){
	if(!gf_puts(NEWLINE, pc) || !gf_puts(NEWLINE, pc))
	  goto write_error;
    }

    return(error_found);

  write_error:
    if(style == QStatus)
      q_status_message1(SM_ORDER, 3, 4, "Error writing message: %.200s", 
			error_description(errno));

    return(1);
}




 
/*------------------------------------------------------------------
   This list of known escape sequences is taken from RFC's 1486 and 1554
   and draft-apng-cc-encoding, and the X11R5 source with only a remote
   understanding of what this all means...

   NOTE: if the length of these should extend beyond 4 chars, fix
	 MAX_ESC_LEN in filter.c
  ----*/
static char *known_escapes[] = {
    "(B",  "(J",  "$@",  "$B",			/* RFC 1468 */
    "(H",
    NULL};

int
match_escapes(esc_seq)
    char *esc_seq;
{
    char **p = NULL;
    int    n;

    if(F_OFF(F_DISABLE_2022_JP_CONVERSIONS, ps_global))
      for(p = known_escapes; *p && strncmp(esc_seq, *p, n = strlen(*p)); p++)
	;

    return((p && *p) ? n + 1 : 0);
}



/*----------------------------------------------------------------------
  Format header text suitable for display

  Args: stream -- mail stream for various header text fetches
	msgno -- sequence number in stream of message we're interested in
	env -- pointer to msg's envelope
	hdrs -- struct containing what's to get formatted
	pc -- function to write header text with
	prefix -- prefix to append to each output line

  Result: 0 if all's well, -1 if write error, 1 if fetch error

  NOTE: Blank-line delimiter is NOT written here.  Newlines are written
	in the local convention.

 ----*/
#define	FHT_OK		0
#define	FHT_WRTERR	-1
#define	FHT_FTCHERR	1
int
format_header(stream, msgno, section, env, hdrs, prefix,handlesp,flags,final_pc)
    MAILSTREAM *stream;
    long	msgno;
    char       *section;
    ENVELOPE   *env;
    HEADER_S   *hdrs;
    char       *prefix;
    HANDLE_S  **handlesp;
    int		flags;
    gf_io_t	final_pc;
{
    int	     rv = FHT_OK;
    int	     nfields, i;
    char    *h = NULL, **fields = NULL, **v, *q, *start,
	    *finish, *current;
    STORE_S *tmp_store;
    gf_io_t  tmp_pc, tmp_gc;

    if(tmp_store = so_get(CharStar, NULL, EDIT_ACCESS))
      gf_set_so_writec(&tmp_pc, tmp_store);
    else
      return(FHT_WRTERR);

    if(ps_global->full_header == 2){
	rv = format_raw_header(stream, msgno, section, tmp_pc);
    }
    else{
	/*
	 * First, calculate how big a fields array we need.
	 */

	/* Custom header viewing list specified */
	if(hdrs->type == HD_LIST){
	    /* view all these headers */
	    if(!hdrs->except){
		for(nfields = 0, v = hdrs->h.l; (q = *v) != NULL; v++)
		  if(!is_an_env_hdr(q))
		    nfields++;
	    }
	    /* view all except these headers */
	    else{
		for(nfields = 0, v = hdrs->h.l; *v != NULL; v++)
		  nfields++;

		if(nfields > 1)
		  nfields--; /* subtract one for ALL_EXCEPT field */
	    }
	}
	else
	  nfields = 6;				/* default view */

	/* allocate pointer space */
	if(nfields){
	    fields = (char **)fs_get((size_t)(nfields+1) * sizeof(char *));
	    memset(fields, 0, (size_t)(nfields+1) * sizeof(char *));
	}

	if(hdrs->type == HD_LIST){
	    /* view all these headers */
	    if(!hdrs->except){
		/* put the non-envelope headers in fields */
		if(nfields)
		  for(i = 0, v = hdrs->h.l; (q = *v) != NULL; v++)
		    if(!is_an_env_hdr(q))
		      fields[i++] = q;
	    }
	    /* view all except these headers */
	    else{
		/* put the list of headers not to view in fields */
		if(nfields)
		  for(i = 0, v = hdrs->h.l; (q = *v) != NULL; v++)
		    if(strucmp(ALL_EXCEPT, q))
		      fields[i++] = q;
	    }

	    v = hdrs->h.l;
	}
	else{
	    if(nfields){
		fields[i = 0] = "Resent-Date";
		fields[++i]   = "Resent-From";
		fields[++i]   = "Resent-To";
		fields[++i]   = "Resent-cc";
		fields[++i]   = "Resent-Subject";
	    }

	    v = fields;
	}

	/* custom view with exception list */
	if(hdrs->type == HD_LIST && hdrs->except){
	    /*
	     * Go through each header in h and print it.
	     * First we check to see if it is an envelope header so we
	     * can print our envelope version of it instead of the raw version.
	     */

	    /* fetch all the other headers */
	    if(nfields)
	      h = pine_fetchheader_lines_not(stream, msgno, section, fields);

	    for(current = h;
		h && delineate_this_header(NULL, current, &start, &finish);
		current = finish){
		char tmp[MAILTMPLEN+1];
		char *colon_loc;

		colon_loc = strindex(start, ':');
		if(colon_loc && colon_loc < finish){
		    strncpy(tmp, start, min(colon_loc-start, sizeof(tmp)-1));
		    tmp[min(colon_loc-start, sizeof(tmp)-1)] = '\0';
		}
		else
		  colon_loc = NULL;

		if(colon_loc && is_an_env_hdr(tmp)){
		    char *dummystart, *dummyfinish;

		    /*
		     * Pretty format for env hdrs.
		     * If the same header appears more than once, only
		     * print the last to avoid duplicates.
		     * They should have been combined in the env when parsed.
		     */
		    if(!delineate_this_header(tmp, current+1, &dummystart,
					      &dummyfinish))
		      format_env_hdr(stream, msgno, section,
				     env, tmp_pc, tmp, flags);
		}
		else{
		    if(rv = format_raw_hdr_string(start, finish, tmp_pc, flags))
		      goto write_error;
		    else
		      start = finish;
		}
	    }
	}
	/* custom view or default */
	else{
	    /* fetch the non-envelope headers */
	    if(nfields)
	      h = pine_fetchheader_lines(stream, msgno, section, fields);

	    /* default envelope for default view */
	    if(hdrs->type == HD_BFIELD)
	      format_envelope(stream, msgno, section, env,
			      tmp_pc, hdrs->h.b, flags);

	    /* go through each header in list, v initialized above */
	    for(; q = *v; v++){
		if(is_an_env_hdr(q)){
		    /* pretty format for env hdrs */
		    format_env_hdr(stream, msgno, section,
				   env, tmp_pc, q, flags);
		}
		else{
		    /*
		     * Go through h finding all occurences of this header
		     * and all continuation lines, and output.
		     */
		    for(current = h;
			h && delineate_this_header(q,current,&start,&finish);
			current = finish){
			if(rv = format_raw_hdr_string(start, finish,
						      tmp_pc, flags))
			  goto write_error;
			else
			  start = finish;
		    }
		}
	    }
	}
    }


  write_error:

    gf_clear_so_writec(tmp_store);

    if(!rv){			/* valid data?  Do wrapping and filtering... */
	int	 column;
	char	*errstr, *display_filter = NULL, trigger[MAILTMPLEN];
	STORE_S *df_store = NULL;

	so_seek(tmp_store, 0L, 0);

	column = (flags & FM_DISPLAY) ? ps_global->ttyo->screen_cols : 80;

	/*
	 * Test for and act on any display filter
	 */
	if(display_filter = df_static_trigger(NULL, trigger)){
	    if(df_store = so_get(CharStar, NULL, EDIT_ACCESS)){

		gf_set_so_writec(&tmp_pc, df_store);
		gf_set_so_readc(&tmp_gc, df_store);
		if(errstr = dfilter(display_filter, tmp_store, tmp_pc, NULL)){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      "Formatting error: %.200s", errstr);
		    rv = FHT_WRTERR;
		}
		else
		  so_seek(df_store, 0L, 0);

		gf_clear_so_writec(df_store);
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 3,
				  "No space for filtered text.");
		rv = FHT_WRTERR;
	    }
	}
	else{
	    so_seek(tmp_store, 0L, 0);
	    gf_set_so_readc(&tmp_gc, tmp_store);
	}

	if(!rv){
	    int margin = 0;

	    gf_filter_init();
	    gf_link_filter(gf_local_nvtnl, NULL);
	    if((F_ON(F_VIEW_SEL_URL, ps_global)
		|| F_ON(F_VIEW_SEL_URL_HOST, ps_global)
		|| F_ON(F_SCAN_ADDR, ps_global))
	       && handlesp)
	      gf_link_filter(gf_line_test,
			     gf_line_test_opt(url_hilite_hdr, handlesp));

	    if((flags & FM_DISPLAY)
	       && !(flags & FM_NOCOLOR)
	       && pico_usingcolor()
	       && ps_global->hdr_colors)
	      gf_link_filter(gf_line_test,
			     gf_line_test_opt(color_headers, NULL));

	    if(prefix && *prefix)
	      column = max(column-strlen(prefix), 50);

	    if(!(flags & FM_NOWRAP))
	      gf_link_filter(gf_wrap,
			     gf_wrap_filter_opt(column, column,
					        (flags & FM_NOINDENT)
						  ? 0 : format_view_margin(), 4,
				 GFW_ONCOMMA | (handlesp ? GFW_HANDLES : 0)));

	    if(prefix && *prefix)
	      gf_link_filter(gf_prefix, gf_prefix_opt(prefix));

	    gf_link_filter(gf_nvtnl_local, NULL);

	    if(errstr = gf_pipe(tmp_gc, final_pc)){
		rv = FHT_WRTERR;
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  "Can't build header : %.200s", errstr);
	    }
	}

	if(df_store){
	    gf_clear_so_readc(df_store);
	    so_give(&df_store);
	}
	else
	  gf_clear_so_readc(tmp_store);
    }

    so_give(&tmp_store);

    if(h)
      fs_give((void **)&h);

    if(fields)
      fs_give((void **)&fields);

    return(rv);
}



/*----------------------------------------------------------------------
  Format RAW header text for display

  Args: stream --
	msgno --
	section --
	pc --

  Result: 0 if all's well, -1 if write error, 1 if fetch error

  NOTE: Blank-line delimiter is NOT written here.  Newlines are written
	in the local convention.

 ----*/
int
format_raw_header(stream, msgno, section, pc)
    MAILSTREAM *stream;
    long	msgno;
    char       *section;
    gf_io_t	pc;
{
    char *h = mail_fetch_header(stream, msgno, section, NULL, NULL, FT_PEEK);
    unsigned char c;

    if(h){
	while(*h){
	    if(ISRFCEOL(h)){
		h += 2;
		if(!gf_puts(NEWLINE, pc))
		  return(FHT_WRTERR);

		if(ISRFCEOL(h))		/* all done! */
		  return(FHT_OK);
	    }
	    else if(FILTER_THIS(*h)){
		c = (unsigned char) *h++;
		if(!((*pc)(c >= 0x80 ? '~' : '^')
		     && (*pc)((c == 0x7f) ? '?' : (c & 0x1f) + '@')))
		  return(FHT_WRTERR);
	    }
	    else if(!(*pc)(*h++))
	      return(FHT_WRTERR);
	}
    }
    else
      return(FHT_FTCHERR);

    return(FHT_OK);
}



/*----------------------------------------------------------------------
  Format c-client envelope data suitable for display

  Args: stream -- stream associated with this envelope
	msgno -- message number associated with this envelope
	e -- envelope
	pc -- place to write result
	which -- which header lines to write

  Result: 0 if all's well, -1 if write error, 1 if fetch error

  NOTE: Blank-line delimiter is NOT written here.  Newlines are written
	in the local convention.

 ----*/
void
format_envelope(s, n, sect, e, pc, which, flags)
    MAILSTREAM *s;
    long	n;
    char       *sect;
    ENVELOPE   *e;
    gf_io_t     pc;
    long	which;
    int         flags;
{
    char       *q;
    char       *p2;
    char        buftmp[MAILTMPLEN];
    
    if(!e)
      return;

    if((which & FE_DATE) && e->date) {
	q = "Date: ";
	sprintf(buftmp, "%.200s", e->date);
	p2 = (char *)rfc1522_decode((unsigned char *) tmp_20k_buf,
				    SIZEOF_20KBUF, buftmp, NULL);
	gf_puts(q, pc);
	format_env_puts(p2, pc);
	gf_puts(NEWLINE, pc);
    }

    if((which & FE_FROM) && e->from)
      format_addr_string(s, n, sect, "From: ", e->from, flags, pc);

    if((which & FE_REPLYTO) && e->reply_to
       && (!e->from || !address_is_same(e->reply_to, e->from)))
      format_addr_string(s, n, sect, "Reply-To: ", e->reply_to, flags, pc);

    if((which & FE_TO) && e->to)
      format_addr_string(s, n, sect, "To: ", e->to, flags, pc);

    if((which & FE_CC) && e->cc)
      format_addr_string(s, n, sect, "Cc: ", e->cc, flags, pc);

    if((which & FE_BCC) && e->bcc)
      format_addr_string(s, n, sect, "Bcc: ", e->bcc, flags, pc);

    if((which & FE_RETURNPATH) && e->return_path)
      format_addr_string(s, n, sect, "Return-Path: ", e->return_path,
			 flags, pc);

    if((which & FE_NEWSGROUPS) && e->newsgroups)
      format_newsgroup_string("Newsgroups: ", e->newsgroups, flags, pc);

    if((which & FE_FOLLOWUPTO) && e->followup_to)
      format_newsgroup_string("Followup-To: ", e->followup_to, flags, pc);

    if((which & FE_SUBJECT) && e->subject && e->subject[0]){
	char *freeme = NULL;

	q = "Subject: ";
	gf_puts(q, pc);

	p2 = istrncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode((unsigned char *)tmp_20k_buf, 10000, e->subject, NULL), SIZEOF_20KBUF-10000);

	if(flags & FM_DISPLAY
	   && (ps_global->display_keywords_in_subject
	       || ps_global->display_keywordinits_in_subject)){
	    int i, some_defined = 0;;

	    /* don't bother if no keywords are defined */
	    for(i = 0; !some_defined && i < NUSERFLAGS; i++)
	      if(s->user_flags[i])
		some_defined++;

	    if(some_defined)
	      p2 = freeme = prepend_keyword_subject(s, n, p2,
			  ps_global->display_keywords_in_subject ? KW : KWInit,
			  ps_global->VAR_KW_BRACES,
			  NULL, NULL);
	}

	format_env_puts(p2, pc);

	if(freeme)
	  fs_give((void **) &freeme);

	gf_puts(NEWLINE, pc);
    }

    if((which & FE_SENDER) && e->sender
       && (!e->from || !address_is_same(e->sender, e->from)))
      format_addr_string(s, n, sect, "Sender: ", e->sender, flags, pc);

    if((which & FE_MESSAGEID) && e->message_id){
	q = "Message-ID: ";
	gf_puts(q, pc);
	p2 = istrncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode((unsigned char *)tmp_20k_buf, 10000, e->message_id, NULL), SIZEOF_20KBUF-10000);
	format_env_puts(p2, pc);
	gf_puts(NEWLINE, pc);
    }

    if((which & FE_INREPLYTO) && e->in_reply_to){
	q = "In-Reply-To: ";
	gf_puts(q, pc);
	p2 = istrncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode((unsigned char *)tmp_20k_buf, 10000, e->in_reply_to, NULL), SIZEOF_20KBUF-10000);
	format_env_puts(p2, pc);
	gf_puts(NEWLINE, pc);
    }

    if((which & FE_REFERENCES) && e->references) {
	q = "References: ";
	gf_puts(q, pc);
	p2 = istrncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode((unsigned char *)tmp_20k_buf, 10000, e->references, NULL), SIZEOF_20KBUF-10000);
	format_env_puts(p2, pc);
	gf_puts(NEWLINE, pc);
    }
}


/*
 * The argument fieldname is something like "Subject:..." or "Subject".
 * Look through the specs in speccolor for a match of the fieldname,
 * and return the color that goes with any match, or NULL.
 * Caller should free the color.
 */
COLOR_PAIR *
hdr_color(fieldname, value, speccolor)
    char         *fieldname,
                 *value;
    SPEC_COLOR_S *speccolor;
{
    SPEC_COLOR_S *hc = NULL;
    COLOR_PAIR   *color_pair = NULL;
    char         *colon, *fname;
    char          fbuf[FBUF_LEN+1];
    int           gotit;
    PATTERN_S    *pat;

    colon = strindex(fieldname, ':');
    if(colon){
	strncpy(fbuf, fieldname, min(colon-fieldname,FBUF_LEN));
	fbuf[min(colon-fieldname,FBUF_LEN)] = '\0';
	fname = fbuf;
    }
    else
      fname = fieldname;

    if(fname && *fname)
      for(hc = speccolor; hc; hc = hc->next)
	if(hc->spec && !strucmp(fname, hc->spec)){
	    if(!hc->val)
	      break;
	    
	    gotit = 0;
	    for(pat = hc->val; !gotit && pat; pat = pat->next)
	      if(srchstr(value, pat->substring))
		gotit++;
	    
	    if(gotit)
	      break;
	}

    if(hc && hc->fg && hc->fg[0] && hc->bg && hc->bg[0])
      color_pair = new_color_pair(hc->fg, hc->bg);
    
    return(color_pair);
}


/*
 * The argument fieldname is something like "Subject:..." or "Subject".
 * Look through the specs in hdr_colors for a match of the fieldname,
 * and return 1 if that fieldname is in one of the patterns, 0 otherwise.
 */
int
any_hdr_color(fieldname)
    char *fieldname;
{
    SPEC_COLOR_S *hc = NULL;
    char         *colon, *fname;
    char          fbuf[FBUF_LEN+1];

    colon = strindex(fieldname, ':');
    if(colon){
	strncpy(fbuf, fieldname, min(colon-fieldname,FBUF_LEN));
	fbuf[min(colon-fieldname,FBUF_LEN)] = '\0';
	fname = fbuf;
    }
    else
      fname = fieldname;

    if(fname && *fname)
      for(hc = ps_global->hdr_colors; hc; hc = hc->next)
	if(hc->spec && !strucmp(fname, hc->spec))
	  break;

    return(hc ? 1 : 0);
}


void
free_spec_colors(colors)
    SPEC_COLOR_S **colors;
{
    if(colors && *colors){
	free_spec_colors(&(*colors)->next);
	if((*colors)->spec)
	  fs_give((void **)&(*colors)->spec);
	if((*colors)->fg)
	  fs_give((void **)&(*colors)->fg);
	if((*colors)->bg)
	  fs_give((void **)&(*colors)->bg);
	if((*colors)->val)
	  free_pattern(&(*colors)->val);
	
	fs_give((void **)colors);
    }
}


/*----------------------------------------------------------------------
     Format an address field, wrapping lines nicely at commas

  Args: field_name  -- The name of the field we're formatting ("TO: ", ...)
        addr        -- ADDRESS structure to format

 Result: A formatted, malloced string is returned.

The resulting lines formatted are 80 columns wide.
  ----------------------------------------------------------------------*/
void
format_addr_string(stream, msgno, section, field_name, addr, flags, pc)
    MAILSTREAM *stream;
    long	msgno;
    char       *section;
    ADDRESS    *addr;
    int         flags;
    char       *field_name;
    gf_io_t	pc;
{
    char    *ptmp, *mtmp;
    int	     trailing = 0, group = 0;
    ADDRESS *atmp;

    if(!addr)
      return;

    /*
     * quickly run down address list to make sure none are patently bogus.
     * If so, just blat raw field out.
     */
    for(atmp = addr; stream && atmp; atmp = atmp->next)
      if(atmp->host && atmp->host[0] == '.'){
	  char *field, *fields[2];

	  fields[1] = NULL;
	  fields[0] = cpystr(field_name);
	  if(ptmp = strchr(fields[0], ':'))
	    *ptmp = '\0';

	  if(field = pine_fetchheader_lines(stream, msgno, section, fields)){
	      char *h, *t;

	      for(t = h = field; *h ; t++)
		if(*t == '\015' && *(t+1) == '\012'){
		    *t = '\0';			/* tie off line */
		    format_env_puts(h, pc);
		    if(*(h = (++t) + 1))	/* set new h and skip CRLF */
		      gf_puts(NEWLINE, pc);	/* more to write */
		    else
		      break;
		}
		else if(!*t){			/* shouldn't happen much */
		    if(h != t)
		      format_env_puts(h, pc);

		    break;
		}

	      fs_give((void **)&field);
	  }

	  fs_give((void **)&fields[0]);
	  gf_puts(NEWLINE, pc);
	  dprint(2, (debugfile, "Error in \"%s\" field address\n",
	         field_name ? field_name : "?"));
	  return;
      }

    gf_puts(field_name, pc);

    while(addr){
	atmp	       = addr->next;		/* remember what's next */
	addr->next     = NULL;
	if(!addr->host && addr->mailbox){
	    mtmp = addr->mailbox;
	    addr->mailbox = cpystr((char *)rfc1522_decode(
			   (unsigned char *)tmp_20k_buf,
			   SIZEOF_20KBUF, addr->mailbox, NULL));
	}

	ptmp	       = addr->personal;	/* RFC 1522 personal name? */
	addr->personal = istrncpy((char *)tmp_20k_buf, (char *)rfc1522_decode((unsigned char *)(tmp_20k_buf+10000), SIZEOF_20KBUF-10000, addr->personal, NULL), 10000);
	tmp_20k_buf[10000-1] = '\0';

	if(!trailing)				/* 1st pass, just address */
	  trailing++;
	else{					/* else comma, unless */
	    if(!((group == 1 && addr->host)	/* 1st addr in group, */
	       || (!addr->host && !addr->mailbox))){ /* or end of group */
		gf_puts(",", pc);
#if	0
		gf_puts(NEWLINE, pc);		/* ONE address/line please */
		gf_puts("   ", pc);
#endif
	    }

	    gf_puts(" ", pc);
	}

	pine_rfc822_write_address_noquote(addr, pc, &group);

	addr->personal = ptmp;			/* restore old personal ptr */
	if(!addr->host && addr->mailbox){
	    fs_give((void **)&addr->mailbox);
	    addr->mailbox = mtmp;
	}

	addr->next = atmp;
	addr       = atmp;
    }

    gf_puts(NEWLINE, pc);
}


static char *rspecials_minus_quote_and_dot = "()<>@,;:\\[]";
				/* RFC822 continuation, must start with CRLF */
#define RFC822CONT "\015\012    "

/* Write RFC822 address with some quoting turned off.
 * Accepts: 
 *	    address to interpret
 *
 * (This is a copy of c-client's rfc822_write_address except
 *  we don't quote double quote and dot in personal names. It writes
 *  to a gf_io_t instead of to a buffer so that we don't have to worry
 *  about fixed sized buffer overflowing. It's also special cased to deal
 *  with only a single address.)
 *
 * The idea is that there are some places where we'd just like to display
 * the personal name as is before applying confusing quoting. However,
 * we do want to be careful not to break things that should be quoted so
 * we'll only use this where we are sure. Quoting may look ugly but it
 * doesn't usually break anything.
 */
void
pine_rfc822_write_address_noquote(adr, pc, group)
    ADDRESS *adr;
    gf_io_t  pc;
    int	    *group;
{
  extern const char *rspecials;

    if (adr->host) {		/* ordinary address? */
      if (!(adr->personal || adr->adl)) pine_rfc822_address (adr, pc);
      else {			/* no, must use phrase <route-addr> form */
        if (adr->personal)
	  pine_rfc822_cat (adr->personal, rspecials_minus_quote_and_dot, pc);

        gf_puts(" <", pc);	/* write address delimiter */
        pine_rfc822_address(adr, pc);
        gf_puts (">", pc);	/* closing delimiter */
      }

      if(*group)
	(*group)++;
    }
    else if (adr->mailbox) {	/* start of group? */
				/* yes, write group name */
      pine_rfc822_cat (adr->mailbox, rspecials, pc);

      gf_puts (": ", pc);	/* write group identifier */
      *group = 1;		/* in a group */
    }
    else if (*group) {		/* must be end of group (but be paranoid) */
      gf_puts (";", pc);
      *group = 0;		/* no longer in that group */
    }
}


void
pine2_rfc822_write_address_noquote(buf, adr, group)
    char    *buf;
    ADDRESS *adr;
    int	    *group;
{
  extern const char *rspecials;

    if (adr->host) {		/* ordinary address? */
      if (!(adr->personal || adr->adl)) rfc822_address (buf,adr);
      else {			/* no, must use phrase <route-addr> form */
        if (adr->personal)
	  rfc822_cat (buf, adr->personal, rspecials_minus_quote_and_dot);

	strcat(buf, " <");
        rfc822_address(buf, adr);
        strcat(buf, ">");	/* closing delimiter */
      }

      if(*group)
	(*group)++;
    }
    else if (adr->mailbox) {	/* start of group? */
				/* yes, write group name */
      rfc822_cat(buf, adr->mailbox, rspecials);

      strcat(buf, ": ");
      *group = 1;		/* in a group */
    }
    else if (*group) {		/* must be end of group (but be paranoid) */
      strcat(buf, ";");
      *group = 0;		/* no longer in that group */
    }
}

/* Write RFC822 route-address to string
 * Accepts:
 *	    address to interpret
 */

void
pine_rfc822_address (adr, pc)
    ADDRESS *adr;
    gf_io_t  pc;
{
  extern char *wspecials;

  if (adr && adr->host) {	/* no-op if no address */
    if (adr->adl) {		/* have an A-D-L? */
      gf_puts (adr->adl, pc);
      gf_puts (":", pc);
    }
				/* write mailbox name */
    pine_rfc822_cat (adr->mailbox, wspecials, pc);
    if (*adr->host != '@') {	/* unless null host (HIGHLY discouraged!) */
      gf_puts ("@", pc);	/* host delimiter */
      gf_puts (adr->host, pc);	/* write host name */
    }
  }
}


/* Concatenate RFC822 string
 * Accepts: 
 *	    pointer to string to concatenate
 *	    list of special characters
 */

void
pine_rfc822_cat (src, specials, pc)
    char *src;
    const char *specials;
    gf_io_t  pc;
{
  char *s;

  if (strpbrk (src,specials)) {	/* any specials present? */
    gf_puts ("\"", pc);		/* opening quote */
				/* truly bizarre characters in there? */
    while (s = strpbrk (src,"\\\"")) {
      char save[2];

      /* turn it into a null-terminated piece */
      save[0] = *s;
      save[1] = '\0';
      *s = '\0';
      gf_puts (src, pc);	/* yes, output leader */
      *s = save[0];
      gf_puts ("\\", pc);	/* quoting */
      gf_puts (save, pc);	/* output the bizarre character */
      src = ++s;		/* continue after the bizarre character */
    }
    if (*src) gf_puts (src, pc);/* output non-bizarre string */
    gf_puts ("\"", pc);		/* closing quote */
  }
  else gf_puts (src, pc);	/* otherwise it's the easy case */
}



/*----------------------------------------------------------------------
  Format an address field, wrapping lines nicely at commas

  Args: field_name  -- The name of the field we're formatting ("TO:", Cc:...)
        newsgrps    -- ADDRESS structure to format

  Result: A formatted, malloced string is returned.

The resuling lines formatted are 80 columns wide.
  ----------------------------------------------------------------------*/
void
format_newsgroup_string(field_name, newsgrps, flags, pc)
    char    *newsgrps;
    int      flags;
    char    *field_name;
    gf_io_t  pc;
{
    char     buf[MAILTMPLEN];
    int	     trailing = 0, llen, alen;
    char    *next_ng;
    
    if(!newsgrps || !*newsgrps)
      return;
    
    gf_puts(field_name, pc);

    llen = strlen(field_name);
    while(*newsgrps){
        for(next_ng = newsgrps; *next_ng && *next_ng != ','; next_ng++);
        strncpy(buf, newsgrps, min(next_ng - newsgrps, sizeof(buf)-1));
        buf[min(next_ng - newsgrps, sizeof(buf)-1)] = '\0';
        newsgrps = next_ng;
        if(*newsgrps)
          newsgrps++;
	alen = strlen(buf);
	if(!trailing){			/* first time thru, just address */
	    llen += alen;
	    trailing++;
	}
	else{				/* else preceding comma */
	    gf_puts(",", pc);
	    llen++;

	    if(alen + llen + 1 > 76){
		gf_puts(NEWLINE, pc);
		gf_puts("    ", pc);
		llen = alen + 5;
	    }
	    else{
		gf_puts(" ", pc);
		llen += alen + 1;
	    }
	}

	if(alen && llen > 76){		/* handle long addresses */
	    register char *q, *p = &buf[alen-1];

	    while(p > buf){
		if(isspace((unsigned char)*p)
		   && (llen - (alen - (int)(p - buf))) < 76){
		    for(q = buf; q < p; q++)
		      (*pc)(*q);	/* write character */

		    gf_puts(NEWLINE, pc);
		    gf_puts("    ", pc);
		    gf_puts(p, pc);
		    break;
		}
		else
		  p--;
	    }

	    if(p == buf)		/* no reasonable break point */
	      gf_puts(buf, pc);
	}
	else
	  gf_puts(buf, pc);
    }

    gf_puts(NEWLINE, pc);
}



/*----------------------------------------------------------------------
  Format a text field that's part of some raw (non-envelope) message header
  This is a bad name for the function. It isn't really raw, it is just not
  an envelope header that we know about. It shouldn't allow any control
  characters through.

  Args: start --
        finish --
	pc -- 

  Result: Semi-digested text (RFC 1522 decoded, anyway) written with "pc"

  ----------------------------------------------------------------------*/
int
format_raw_hdr_string(start, finish, pc, flags)
    char    *start;
    char    *finish;
    gf_io_t  pc;
    int      flags;
{
    register char *current;
    unsigned char *p, *tmp = NULL, c;
    char          *q, *s, *tb = NULL;
    size_t	   n, len;
    char	   ch, b[1000];
    int		   rv = FHT_OK, rfceol = 0;

    ch = *finish;
    *finish = '\0';

    if((n = finish - start) > SIZEOF_20KBUF-1){
	len = n+1;
	p = tmp = (unsigned char *) fs_get(len * sizeof(unsigned char));
    }
    else{
	len = SIZEOF_20KBUF;
	p = (unsigned char *) tmp_20k_buf;
    }

    if(islower((unsigned char)(*start)))
      *start = toupper((unsigned char)(*start));

    current = (char *) rfc1522_decode(p, len, start, NULL);

    /* output from start to finish */
    while(*current && rv == FHT_OK)
      if(ISRFCEOL(current)){
	  if(!gf_puts(NEWLINE, pc))
	    rv = FHT_WRTERR;

	  current += 2;
      }
      else{
	  /* find next EOL or end */
	  for(rfceol = 0, q = current; *q; q++)
	    if(ISRFCEOL(q))
	      break;

	  if(ISRFCEOL(q)){
	    rfceol = 1;
	    *q = '\0';
	  }

	  tb = NULL;
	  /* room to escape control chars */
	  if(2 * (q - current) + 1 > sizeof(b)){
	      len = 2 * (q - current) + 1;
	      s = tb = (char *) fs_get(len * sizeof(char));
	  }
	  else{
	      len = sizeof(b);
	      s = b;
	  }

	  istrncpy(s, current, len);
	  s[len-1] = '\0';		/* making sure */

	  if(!gf_puts(s, pc))
	    rv = FHT_WRTERR;

	  if(tb)
	    fs_give((void **) &tb);

	  if(rfceol)
	    *q = '\015';

	  current = q;
      }


    if(tmp)
      fs_give((void **) &tmp);

    *finish = ch;

    return(rv);
}




/*----------------------------------------------------------------------
  Format a text field that's part of some raw (non-envelope) message header

  Args: s --
	pc -- 

  Result: Output

  ----------------------------------------------------------------------*/
int
format_env_puts(s, pc)
    char    *s;
    gf_io_t  pc;
{
    if(ps_global->pass_ctrl_chars)
      return(gf_puts(s, pc));

    for(; *s; s++)
      if(FILTER_THIS(*s)){
	  if(!((*pc)((unsigned char) (*s) >= 0x80 ? '~' : '^')
	       && (*pc)((*s == 0x7f) ? '?' : (*s & 0x1f) + '@')))
	    return(0);
      }
      else if(!(*pc)(*s))
	return(0);

    return(1);
}




/*----------------------------------------------------------------------
    Format a strings describing one unshown part of a Mime message

Args: number -- A string with the part number i.e. "3.2.1"
      body   -- The body part
      type   -- 1 - Not shown, but can be
                2 - Not shown, cannot be shown
                3 - Can't print
      width  -- allowed width per line of editorial comment
      pc     -- function used to write the description comment

Result: formatted description written to object ref'd by "pc"

Note that size of the strings are carefully calculated never to overflow 
the static buffer:
    number  < 20,  description limited to 100, type_desc < 200,
    size    < 20,  second line < 100           other stuff < 60
 ----*/
char *
part_desc(number, body, type, width, pc)
     char    *number;
     BODY    *body;
     int      type, width;
     gf_io_t  pc;
{
    char *t;
    char buftmp[MAILTMPLEN];

    if(!gf_puts(NEWLINE, pc))
      return("No space for description");

    sprintf(buftmp, "%.200s", body->description ? body->description : "");
    sprintf(tmp_20k_buf+10000, "Part %s, %s%.2048s%s%s  %s%s.",
            number,
            body->description == NULL ? "" : "\"",
            body->description == NULL ? ""
	      : (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				       10000, buftmp, NULL),
            body->description == NULL ? "" : "\"  ",
            type_desc(body->type, body->subtype, body->parameter, 1),
            body->type == TYPETEXT ? comatose(body->size.lines) :
                                     byte_string(body->size.bytes),
            body->type == TYPETEXT ? " lines" : "");

    istrncpy((char *)tmp_20k_buf, (char *)(tmp_20k_buf+10000), 10000);
    tmp_20k_buf[10000] = '\0';

    t = &tmp_20k_buf[strlen(tmp_20k_buf)];

    if(type){
	sstrcpy(&t, "\015\012");
	switch(type) {
	  case 1:
	    if(MIME_VCARD(body->type,body->subtype))
	      sstrcpy(&t,
	   "Not Shown. Use the \"V\" command to view or save to address book.");
	    else
	      sstrcpy(&t,
	       "Not Shown. Use the \"V\" command to view or save this part.");

	    break;

	  case 2:
	    sstrcpy(&t, "Cannot ");
	    if(body->type != TYPEAUDIO && body->type != TYPEVIDEO)
	      sstrcpy(&t, "dis");

	    sstrcpy(&t, 
	       "play this part. Press \"V\" then \"S\" to save in a file.");
	    break;

	  case 3:
	    sstrcpy(&t, "Unable to print this part.");
	    break;
	}
    }

    if(!(t = format_editorial(tmp_20k_buf, width, pc))){
	if(!gf_puts(NEWLINE, pc))
	  t = "No space for description";
    }

    return(t);
}



/*----------------------------------------------------------------------
   routine for displaying text on the screen.

  Args: sparms -- structure of args controlling what happens outside
		  just the business of managing text scrolling
 
   This displays in three different kinds of text. One is an array of
lines passed in in text_array. The other is a simple long string of
characters passed in in text.

  The style determines what some of the error messages will be, and
what commands are available as different things are appropriate for
help text than for message text etc.

 ---*/

int
scrolltool(sparms)
    SCROLL_S *sparms;
{
    register long    cur_top_line,  num_display_lines;
    int              result, done, ch, cmd, found_on, found_on_col,
		     first_view, force, scroll_lines, km_size,
		     cursor_row, cursor_col, km_popped;
    long             jn;
    struct key_menu *km;
    HANDLE_S	    *next_handle;
    bitmap_t         bitmap;
    OtherMenu        what;
    Pos		     whereis_pos;

    num_display_lines	      = SCROLL_LINES(ps_global);
    km_popped		      = 0;
    ps_global->mangled_header = 1;
    ps_global->mangled_footer = 1;
    ps_global->mangled_body   = !sparms->body_valid;
      

    what	    = sparms->keys.what;	/* which key menu to display */
    cur_top_line    = 0;
    done	    = 0;
    found_on	    = -1;
    found_on_col    = -1;
    first_view	    = 1;
    force	    = 0;
    ch		    = 'x';			/* for first time through */
    whereis_pos.row = 0;
    whereis_pos.col = 0;
    next_handle	    = sparms->text.handles;

    set_scroll_text(sparms, cur_top_line, scroll_state(SS_NEW));
    format_scroll_text();

    if(km = sparms->keys.menu){
	memcpy(bitmap, sparms->keys.bitmap, sizeof(bitmap_t));
    }
    else{
	setbitmap(bitmap);
	km = &simple_text_keymenu;
#ifdef	_WINDOWS
	sparms->mouse.popup = simple_text_popup;
#endif
    }

    if(!sparms->bar.title)
      sparms->bar.title = "Text";

    if(sparms->bar.style == TitleBarNone){
	if(THREADING() && sp_viewing_a_thread(ps_global->mail_stream))
	  sparms->bar.style = ThrdMsgPercent;
	else
	  sparms->bar.style = MsgTextPercent;
    }

    switch(sparms->start.on){
      case LastPage :
	cur_top_line = max(0, scroll_text_lines() - (num_display_lines-2));
	if(F_ON(F_SHOW_CURSOR, ps_global)){
	    whereis_pos.row = scroll_text_lines() - cur_top_line;
	    found_on	    = scroll_text_lines() - 1;
	}

	break;

      case Fragment :
	if(sparms->start.loc.frag){
	    (void) url_local_fragment(sparms->start.loc.frag);

	    cur_top_line = scroll_state(SS_CUR)->top_text_line;

	    if(F_ON(F_SHOW_CURSOR, ps_global)){
		whereis_pos.row = scroll_text_lines() - cur_top_line;
		found_on	= scroll_text_lines() - 1;
	    }
	}

	break;

      case Offset :
	if(sparms->start.loc.offset){
	    SCRLCTRL_S *st = scroll_state(SS_CUR);

	    for(cur_top_line = 0L;
		cur_top_line + 1 < scroll_text_lines()
		  && (sparms->start.loc.offset 
				      -= scroll_handle_column(cur_top_line,
								    -1)) >= 0;
		cur_top_line++)
	      ;
	}

	break;

      case Handle :
	if(scroll_handle_obscured(sparms->text.handles))
	  cur_top_line = scroll_handle_reframe(-1, TRUE);

	break;

      default :			/* no-op */
	break;
    }

    /* prepare for calls below to tell us where to go */
    ps_global->next_screen = SCREEN_FUN_NULL;

    cancel_busy_alarm(-1);

    while(!done) {
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(ps_global);
		ps_global->mangled_body = 1;
	    }
	}

	if(ps_global->mangled_screen) {
	    ps_global->mangled_header = 1;
	    ps_global->mangled_footer = 1;
            ps_global->mangled_body   = 1;
	}

        if(!sparms->quell_newmail && streams_died())
          ps_global->mangled_header = 1;

        dprint(9, (debugfile, "@@@@ current:%ld\n",
		   mn_get_cur(ps_global->msgmap)));


        /*==================== All Screen painting ====================*/
        /*-------------- The title bar ---------------*/
	update_scroll_titlebar(cur_top_line, ps_global->mangled_header);

	if(ps_global->mangled_screen){
	    /* this is the only line not cleared by header, body or footer
	     * repaint calls....
	     */
	    ClearLine(1);
            ps_global->mangled_screen = 0;
	}

        /*---- Scroll or update the body of the text on the screen -------*/
        cur_top_line		= scroll_scroll_text(cur_top_line, next_handle,
						     ps_global->mangled_body);
	ps_global->redrawer	= redraw_scroll_text;
        ps_global->mangled_body = 0;

	/*--- Check to see if keymenu might change based on next_handle --*/
	if(sparms->text.handles != next_handle)
	  ps_global->mangled_footer = 1;
	
	if(next_handle)
	  sparms->text.handles = next_handle;

        /*------------- The key menu footer --------------------*/
	if(ps_global->mangled_footer || sparms->keys.each_cmd){
	    if(km_popped){
		FOOTER_ROWS(ps_global) = 3;
		clearfooter(ps_global);
	    }

	    if(F_ON(F_ARROW_NAV, ps_global)){
		menu_clear_binding(km, KEY_LEFT);
		if((cmd = menu_clear_binding(km, '<')) != MC_UNKNOWN){
		    menu_add_binding(km, '<', cmd);
		    menu_add_binding(km, KEY_LEFT, cmd);
		}
	    }

	    if(F_ON(F_ARROW_NAV, ps_global)){
		menu_clear_binding(km, KEY_RIGHT);
		if((cmd = menu_clear_binding(km, '>')) != MC_UNKNOWN){
		    menu_add_binding(km, '>', cmd);
		    menu_add_binding(km, KEY_RIGHT, cmd);
		}
	    }

	    if(sparms->keys.each_cmd){
		(*sparms->keys.each_cmd)(sparms,
				 scroll_handle_obscured(sparms->text.handles));
		memcpy(bitmap, sparms->keys.bitmap, sizeof(bitmap_t));
	    }

	    if(menu_binding_index(km, MC_JUMP) >= 0){
	      for(cmd = 0; cmd < 10; cmd++)
		if(F_ON(F_ENABLE_JUMP, ps_global))
		  (void) menu_add_binding(km, '0' + cmd, MC_JUMP);
		else
		  (void) menu_clear_binding(km, '0' + cmd);
	    }

            draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
			 1-FOOTER_ROWS(ps_global), 0, what);
	    what = SameMenu;
	    ps_global->mangled_footer = 0;
	    if(km_popped){
		FOOTER_ROWS(ps_global) = 1;
		mark_keymenu_dirty();
	    }
	}

	if((ps_global->first_time_user || ps_global->show_new_version)
	   && first_view && sparms->text.handles
	   && (sparms->text.handles->next || sparms->text.handles->prev)
	   && !sparms->quell_help)
	  q_status_message(SM_ORDER, 0, 3, HANDLE_INIT_MSG);

	/*============ Check for New Mail and CheckPoint ============*/
        if(!sparms->quell_newmail &&
	   new_mail(force, NM_TIMING(ch), NM_STATUS_MSG) >= 0){
	    update_scroll_titlebar(cur_top_line, 1);
	    if(ps_global->mangled_footer)
              draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
			   1-FOOTER_ROWS(ps_global), 0, what);

	    ps_global->mangled_footer = 0;
	}

	/*
	 * If an expunge of the current message happened during the
	 * new mail check we want to bail out of here. See mm_expunged.
	 */
	if(ps_global->next_screen != SCREEN_FUN_NULL){
	    done = 1;
	    continue;
	}

	if(first_view && num_display_lines >= scroll_text_lines())
	  q_status_message1(SM_INFO, 0, 1, "ALL of %.200s", STYLE_NAME(sparms));
			    

	force      = 0;		/* may not need to next time around */
	first_view = 0;		/* check_point a priority any more? */

	/*==================== Output the status message ==============*/
	if(!sparms->no_stat_msg){
	    if(km_popped){
		FOOTER_ROWS(ps_global) = 3;
		mark_status_unknown();
	    }

	    display_message(ch);
	    if(km_popped){
		FOOTER_ROWS(ps_global) = 1;
		mark_status_unknown();
	    }
	}

	if(F_ON(F_SHOW_CURSOR, ps_global)){
#ifdef	WINDOWS
	    if(cur_top_line != scroll_state(SS_CUR)->top_text_line)
	      whereis_pos.row = 0;
#endif
	
	    if(whereis_pos.row > 0){
		cursor_row  = SCROLL_LINES_ABOVE(ps_global)
				+ whereis_pos.row - 1;
		cursor_col  = whereis_pos.col;
	    }
	    else{
		POSLIST_S  *lp = NULL;

		if(sparms->text.handles &&
		   !scroll_handle_obscured(sparms->text.handles)){
		    SCRLCTRL_S *st = scroll_state(SS_CUR);

		    for(lp = sparms->text.handles->loc; lp; lp = lp->next)
		      if(lp->where.row >= st->top_text_line
			 && lp->where.row < st->top_text_line
							  + st->screen.length){
			  cursor_row = lp->where.row - cur_top_line
					      + SCROLL_LINES_ABOVE(ps_global);
			  cursor_col = lp->where.col;
			  break;
		      }
		}

		if(!lp){
		    cursor_col = 0;
		    /* first new line of text */
		    cursor_row  = SCROLL_LINES_ABOVE(ps_global) +
			((cur_top_line == 0) ? 0 : ps_global->viewer_overlap);
		}
	    }
	}
	else{
	    cursor_col = 0;
	    cursor_row = ps_global->ttyo->screen_rows
			    - SCROLL_LINES_BELOW(ps_global);
	}

	MoveCursor(cursor_row, cursor_col);

	/*================ Get command and validate =====================*/
#ifdef	MOUSE
#ifndef	WIN32
	if(sparms->text.handles)
#endif
	{
	    mouse_in_content(KEY_MOUSE, -1, -1, 0x5, 0);
	    register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
			   ps_global->ttyo->screen_rows
						- (FOOTER_ROWS(ps_global) + 1),
			   ps_global->ttyo->screen_cols);
	}
#endif
#ifdef	_WINDOWS
	mswin_allowcopy(mswin_readscrollbuf);
	mswin_setscrollcallback(pcpine_do_scroll);

	if(sparms->help.text != NO_HELP)
	  mswin_sethelptextcallback(pcpine_help_scroll);

	mswin_setresizecallback(pcpine_resize_scroll);

	if(sparms->text.handles
	   && sparms->text.handles->type != Folder)
	  mswin_mousetrackcallback(pcpine_view_cursor);
#endif
        ch = read_command();
#ifdef	MOUSE
#ifndef	WIN32
	if(sparms->text.handles)
#endif
	  clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_allowcopy(NULL);
	mswin_setscrollcallback(NULL);
	mswin_sethelptextcallback(NULL);
	mswin_clearresizecallback(pcpine_resize_scroll);
	mswin_mousetrackcallback(NULL);
	cur_top_line = scroll_state(SS_CUR)->top_text_line;
#endif

	cmd = menu_command(ch, km);

	if(km_popped)
	  switch(cmd){
	    case MC_NONE :
	    case MC_OTHER :
	    case MC_RESIZE:
	    case MC_REPAINT :
	      km_popped++;
	      break;
	    
	    default:
	      clearfooter(ps_global);
	      break;
	  }


	/*============= Execute command =======================*/
	switch(cmd){

            /* ------ Help -------*/
	  case MC_HELP :
	    if(FOOTER_ROWS(ps_global) == 1 && km_popped == 0){
		km_popped = 2;
		ps_global->mangled_footer = 1;
		break;
	    }

	    whereis_pos.row = 0;
            if(sparms->help.text == NO_HELP){
                q_status_message(SM_ORDER, 0, 5,
				 "No help text currently available");
                break;
            }

	    km_size = FOOTER_ROWS(ps_global);

	    helper(sparms->help.text, sparms->help.title, 0);

	    if(ps_global->next_screen != main_menu_screen
	       && km_size == FOOTER_ROWS(ps_global)) {
		/* Have to reset because helper uses scroll_text */
		num_display_lines	  = SCROLL_LINES(ps_global);
		ps_global->mangled_screen = 1;
	    }
	    else
	      done = 1;

            break; 


            /*---------- Roll keymenu ------*/
	  case MC_OTHER :
	    if(F_OFF(F_USE_FK, ps_global))
	      warn_other_cmds();

	    what = NextMenu;
	    ps_global->mangled_footer = 1;
	    break;
            

            /* -------- Scroll back one page -----------*/
	  case MC_PAGEUP :
	    whereis_pos.row = 0;
	    if(cur_top_line) {
		scroll_lines = min(max(num_display_lines -
			ps_global->viewer_overlap, 1), num_display_lines);
		cur_top_line -= scroll_lines;
		if(cur_top_line <= 0){
		    cur_top_line = 0;
		    q_status_message1(SM_INFO, 0, 1, "START of %.200s",
				      STYLE_NAME(sparms));
		}
	    }
	    else{
		/* hilite last available handle */
		next_handle = NULL;
		if(sparms->text.handles){
		    HANDLE_S *h = sparms->text.handles;

		    while((h = scroll_handle_prev_sel(h))
			  && !scroll_handle_obscured(h))
		      next_handle = h;
		}

		if(!next_handle)
		  q_status_message1(SM_ORDER, 0, 1, "Already at start of %.200s",
				    STYLE_NAME(sparms));

	    }


            break;


            /*---- Scroll down one page -------*/
	  case MC_PAGEDN :
	    if(cur_top_line + num_display_lines < scroll_text_lines()){
		whereis_pos.row = 0;
		scroll_lines = min(max(num_display_lines -
			ps_global->viewer_overlap, 1), num_display_lines);
		cur_top_line += scroll_lines;

		if(cur_top_line + num_display_lines >= scroll_text_lines())
		  q_status_message1(SM_INFO, 0, 1, "END of %.200s",
				    STYLE_NAME(sparms));
            }
	    else if(!sparms->end_scroll
		    || !(done = (*sparms->end_scroll)(sparms))){
		q_status_message1(SM_ORDER, 0, 1, "Already at end of %.200s",
				  STYLE_NAME(sparms));
		/* hilite last available handle */
		if(sparms->text.handles){
		    HANDLE_S *h = sparms->text.handles;

		    while(h = scroll_handle_next_sel(h))
		      next_handle = h;
		}
	    }

            break;

	    /* scroll to the top page */
	  case MC_HOMEKEY:
	    if(cur_top_line){
		cur_top_line = 0;
		q_status_message1(SM_INFO, 0, 1, "START of %.200s",
				  STYLE_NAME(sparms));
	    }
	    else{
		next_handle = NULL;
		if(sparms->text.handles){
		    HANDLE_S *h = sparms->text.handles;

		    while(h = scroll_handle_prev_sel(h))
		      next_handle = h;
		}
	    }
	    break;

	    /* scroll to the bottom page */
	  case MC_ENDKEY:
	    if(cur_top_line + num_display_lines < scroll_text_lines()){
		cur_top_line = scroll_text_lines() - min(5, num_display_lines);
		q_status_message1(SM_INFO, 0, 1, "END of %.200s",
				  STYLE_NAME(sparms));
	    }
	    else {
		if(sparms->text.handles){
		    HANDLE_S *h = sparms->text.handles;

		    while(h = scroll_handle_next_sel(h))
		      next_handle = h;
		}
	    }
	    break;

            /*------ Scroll down one line -----*/
	  case MC_CHARDOWN :
	    next_handle = NULL;
	    if(sparms->text.handles){
		if(sparms->vert_handle){
		    HANDLE_S *h, *h2;
		    int	      i, j, k;

		    for(h = NULL,
			  i = (h2 = sparms->text.handles)->loc->where.row + 1,
			  j = h2->loc->where.col,
			  k = h2->key;
			h2 && (!h
			       || (h->loc->where.row == h2->loc->where.row));
			h2 = h2->next)
		      /* must be different key */
		      /* ... below current line */
		      /* ... pref'bly to left */
		      if(h2->key != k
			 && h2->loc->where.row >= i){
			  if(h2->loc->where.col > j){
			      if(!h)
				h = h2;

			      break;
			  }
			  else
			    h = h2;
		      }

		    if(h){
			whereis_pos.row = 0;
			next_handle = h;
			if(result = scroll_handle_obscured(next_handle)){
			    long new_top;

			    if(scroll_handle_obscured(sparms->text.handles)
			       && result > 0)
			      next_handle = sparms->text.handles;

			    ps_global->mangled_body++;
			    new_top = scroll_handle_reframe(next_handle->key,0);
			    if(new_top >= 0)
			      cur_top_line = new_top;
			}
		    }
		}
		else if(!(ch == ctrl('N') || F_ON(F_FORCE_ARROWS, ps_global)))
		  next_handle = scroll_handle_next(sparms->text.handles);
	    }

	    if(!next_handle){
		if(cur_top_line + num_display_lines < scroll_text_lines()){
		    whereis_pos.row = 0;
		    cur_top_line++;
		    if(cur_top_line + num_display_lines >= scroll_text_lines())
		      q_status_message1(SM_INFO, 0, 1, "END of %.200s",
					STYLE_NAME(sparms));
		}
		else
		  q_status_message1(SM_ORDER, 0, 1, "Already at end of %.200s",
				    STYLE_NAME(sparms));
	    }

	    break;


            /* ------ Scroll back up one line -------*/
	  case MC_CHARUP :
	    next_handle = NULL;
	    if(sparms->text.handles){
		if(sparms->vert_handle){
		    HANDLE_S *h, *h2;
		    int	  i, j, k;

		    for(h = NULL,
			  i = (h2 = sparms->text.handles)->loc->where.row - 1,
			  j = h2->loc->where.col,
			  k = h2->key;
			h2 && (!h
			       || (h->loc->where.row == h2->loc->where.row));
			h2 = h2->prev)
		      /* must be new key, above current
		       * line and pref'bly to right
		       */
		      if(h2->key != k
			 && h2->loc->where.row <= i){
			  if(h2->loc->where.col < j){
			      if(!h)
				h = h2;

			      break;
			  }
			  else
			    h = h2;
		      }

		    if(h){
			whereis_pos.row = 0;
			next_handle = h;
			if(result = scroll_handle_obscured(next_handle)){
			    long new_top;

			    if(scroll_handle_obscured(sparms->text.handles)
			       && result < 0)
			      next_handle = sparms->text.handles;

			    ps_global->mangled_body++;
			    new_top = scroll_handle_reframe(next_handle->key,0);
			    if(new_top >= 0)
			      cur_top_line = new_top;
			}
		    }
		}
		else if(!(ch == ctrl('P') || F_ON(F_FORCE_ARROWS, ps_global)))
		  next_handle = scroll_handle_prev(sparms->text.handles);
	    }

	    if(!next_handle){
		whereis_pos.row = 0;
		if(cur_top_line){
		    cur_top_line--;
		    if(cur_top_line == 0)
		      q_status_message1(SM_INFO, 0, 1, "START of %.200s",
					STYLE_NAME(sparms));
		}
		else
		  q_status_message1(SM_ORDER, 0, 1,
				    "Already at start of %.200s",
				    STYLE_NAME(sparms));
	    }

	    break;


	  case MC_NEXT_HANDLE :
	    if(next_handle = scroll_handle_next_sel(sparms->text.handles)){
		whereis_pos.row = 0;
		if(result = scroll_handle_obscured(next_handle)){
		    long new_top;

		    if(scroll_handle_obscured(sparms->text.handles)
		       && result > 0)
		      next_handle = sparms->text.handles;

		    ps_global->mangled_body++;
		    new_top = scroll_handle_reframe(next_handle->key, 0);
		    if(new_top >= 0)
		      cur_top_line = new_top;
		}
	    }
	    else{
		if(scroll_handle_obscured(sparms->text.handles)){
		    long new_top;

		    ps_global->mangled_body++;
		    if((new_top = scroll_handle_reframe(-1, 0)) >= 0){
			whereis_pos.row = 0;
			cur_top_line = new_top;
		    }
		}

		q_status_message1(SM_ORDER, 0, 1,
				  "Already on last item in %.200s",
				  STYLE_NAME(sparms));
	    }

	    break;


	  case MC_PREV_HANDLE :
	    if(next_handle = scroll_handle_prev_sel(sparms->text.handles)){
		whereis_pos.row = 0;
		if(result = scroll_handle_obscured(next_handle)){
		    long new_top;

		    if(scroll_handle_obscured(sparms->text.handles)
		       && result < 0)
		      next_handle = sparms->text.handles;

		    ps_global->mangled_body++;
		    new_top = scroll_handle_reframe(next_handle->key, 0);
		    if(new_top >= 0)
		      cur_top_line = new_top;
		}
	    }
	    else{
		if(scroll_handle_obscured(sparms->text.handles)){
		    long new_top;

		    ps_global->mangled_body++;
		    if((new_top = scroll_handle_reframe(-1, 0)) >= 0){
			whereis_pos.row = 0;
			cur_top_line = new_top;
		    }
		}

		q_status_message1(SM_ORDER, 0, 1,
				  "Already on first item in %.200s",
				  STYLE_NAME(sparms));
	    }

	    break;


	    /*------  View the current handle ------*/
	  case MC_VIEW_HANDLE :
	    switch(scroll_handle_obscured(sparms->text.handles)){
	      default :
	      case 0 :
		switch(scroll_handle_launch(sparms->text.handles,
					sparms->text.handles->force_display)){
		  case 1 :
		    cmd = MC_EXIT;		/* propagate */
		    done = 1;
		    break;

		  case -1 :
		    cmd_cancelled(NULL);
		    break;

		  default :
		    break;
		}

		cur_top_line = scroll_state(SS_CUR)->top_text_line;
		break;

	      case 1 :
		q_status_message(SM_ORDER, 0, 2, HANDLE_BELOW_ERR);
		break;

	      case -1 :
		q_status_message(SM_ORDER, 0, 2, HANDLE_ABOVE_ERR);
		break;
	    }

	    break;

            /*---------- Search text (where is) ----------*/
	  case MC_WHEREIS :
            ps_global->mangled_footer = 1;
	    {long start_row;
	     int  start_col, key = 0;

	     if(F_ON(F_SHOW_CURSOR,ps_global)){
		 if(found_on < 0
		    || found_on >= scroll_text_lines()
		    || found_on < cur_top_line
		    || found_on >= cur_top_line + num_display_lines){
		     start_row = cur_top_line;
		     start_col = 0;
		 }
		 else{
		     if(found_on_col < 0){
			 start_row = found_on + 1;
			 start_col = 0;
		     }
		     else{
			 start_row = found_on;
			 start_col = found_on_col+1;
		     }
		 }
	     }
	     else if(sparms->srch_handle){
		 HANDLE_S   *h;

		 if(h = scroll_handle_next_sel(sparms->text.handles)){
		     /*
		      * Translate the screen's column into the
		      * line offset to start on...
		      *
		      * This makes it so search_text never returns -3
		      * so we don't know it is the same match. That's
		      * because we start well after the current handle
		      * (at the next handle) and that causes us to
		      * think the one we just matched on is a different
		      * one from before. Can't think of an easy way to
		      * fix it, though, and it isn't a big deal. We still
		      * match, we just don't say current line contains
		      * the only match.
		      */
		     start_row = h->loc->where.row;
		     start_col = scroll_handle_index(start_row,
						     h->loc->where.col);
		 }
	     }
	     else{
		 start_row = (found_on < 0
			      || found_on >= scroll_text_lines()
			      || found_on < cur_top_line
			      || found_on >= cur_top_line + num_display_lines)
				? cur_top_line : found_on + 1,
		 start_col = 0;
	     }

             found_on = search_text(-FOOTER_ROWS(ps_global), start_row,
				     start_col, tmp_20k_buf, &whereis_pos,
				     &found_on_col);

	     if(found_on == -4){	/* search to top of text */
		 whereis_pos.row = 0;
		 whereis_pos.col = 0;
		 found_on	 = 0;
		 if(sparms->text.handles && sparms->srch_handle)
		   key = 1;
	     }
	     else if(found_on == -5){	/* search to bottom of text */
		 HANDLE_S *h;

		 whereis_pos.row = max(scroll_text_lines() - 1, 0);
		 whereis_pos.col = 0;
		 found_on	 = whereis_pos.row;
		 if((h = sparms->text.handles) && sparms->srch_handle)
		   do
		     key = h->key;
		   while(h = h->next);
	     }
	     else if(found_on == -3){
		 whereis_pos.row = found_on = start_row;
		 found_on_col = start_col - 1;
		 q_status_message(SM_ORDER, 1, 3,
				  "Current line contains the only match");
	     }

	     if(found_on >= 0){
		result = found_on < cur_top_line;
		if(!key)
		  key = (sparms->text.handles)
			  ? dot_on_handle(found_on, whereis_pos.col) : 0;

		if(F_ON(F_FORCE_LOW_SPEED,ps_global)
		   || ps_global->low_speed
		   || F_ON(F_SHOW_CURSOR,ps_global)
		   || key){
		    if((found_on >= cur_top_line + num_display_lines ||
		       found_on < cur_top_line) &&
		       num_display_lines > ps_global->viewer_overlap){
			cur_top_line = found_on - ((found_on > 0) ? 1 : 0);
			if(scroll_text_lines()-cur_top_line < 5)
			  cur_top_line = max(0,
			      scroll_text_lines()-min(5,num_display_lines));
		    }
		    /* else leave cur_top_line alone */
		}
		else{
		    cur_top_line = found_on - ((found_on > 0) ? 1 : 0);
		    if(scroll_text_lines()-cur_top_line < 5)
		      cur_top_line = max(0,
			  scroll_text_lines()-min(5,num_display_lines));
		}

		whereis_pos.row = whereis_pos.row - cur_top_line + 1;
		if(tmp_20k_buf[0])
		  q_status_message(SM_ORDER, 0, 3, tmp_20k_buf);
		else
		  q_status_message2(SM_ORDER, 0, 3,
				    "%.200sFound on line %.200s on screen",
				    result ? "Search wrapped to start. " : "",
				    int2string(whereis_pos.row));

		if(key){
		    if(sparms->text.handles->key < key)
		      for(next_handle = sparms->text.handles->next;
			  next_handle->key != key;
			  next_handle = next_handle->next)
			;
		    else
		      for(next_handle = sparms->text.handles;
			  next_handle->key != key;
			  next_handle = next_handle->prev)
			;
		}
            }
	    else if(found_on == -1)
	      cmd_cancelled("Search");
            else
	      q_status_message(SM_ORDER, 0, 3, "Word not found");
	    }

            break; 


            /*-------------- jump command -------------*/
	    /* NOTE: preempt the process_cmd() version because
	     *	     we need to get at the number..
	     */
	  case MC_JUMP :
	    jn = jump_to(ps_global->msgmap, -FOOTER_ROWS(ps_global), ch,
			 sparms, View);
	    if(sparms && sparms->jump_is_debug)
	      done = 1;
	    else if(jn > 0 && jn != mn_get_cur(ps_global->msgmap)){

		if(mn_total_cur(ps_global->msgmap) > 1L)
		  mn_reset_cur(ps_global->msgmap, jn);
		else
		  mn_set_cur(ps_global->msgmap, jn);
		
		done = 1;
	    }
	    else
	      ps_global->mangled_footer = 1;

	    break;


#ifdef MOUSE	    
            /*-------------- Mouse Event -------------*/
	  case MC_MOUSE:
	    {
	      MOUSEPRESS mp;
	      long	line;
	      int	key;

	      mouse_get_last (NULL, &mp);
	      mp.row -= 2;

	      /* The clicked line have anything special on it? */
	      if((line = cur_top_line + mp.row) < scroll_text_lines()
		 && (key = dot_on_handle(line, mp.col))){
		  switch(mp.button){
		    case M_BUTTON_RIGHT :
#ifdef	_WINDOWS
		      if(sparms->mouse.popup){
			  if(sparms->text.handles->key < key)
			    for(next_handle = sparms->text.handles->next;
				next_handle->key != key;
				next_handle = next_handle->next)
			      ;
			  else
			    for(next_handle = sparms->text.handles;
				next_handle->key != key;
				next_handle = next_handle->prev)
			      ;

			  if(sparms->mouse.popup){
			      cur_top_line = scroll_scroll_text(cur_top_line,
						     next_handle,
						     ps_global->mangled_body);
			      fflush(stdout);
			      switch((*sparms->mouse.popup)(sparms, key)){
				case 1 :
				  cur_top_line = doubleclick_handle(sparms, next_handle, &cmd, &done);
				  break;

				case 2 :
				  done++;
				  break;
			      }
			  }
		      }

#endif
		      break;

		    case M_BUTTON_LEFT :
		      if(sparms->text.handles->key < key)
			for(next_handle = sparms->text.handles->next;
			    next_handle->key != key;
			    next_handle = next_handle->next)
			  ;
		      else
			for(next_handle = sparms->text.handles;
			    next_handle->key != key;
			    next_handle = next_handle->prev)
			  ;

		      if(mp.doubleclick)		/* launch url */
			cur_top_line = doubleclick_handle(sparms, next_handle, &cmd, &done);
		      else if(sparms->mouse.click)
			(*sparms->mouse.click)(sparms);

		      break;

		    case M_BUTTON_MIDDLE :		/* NO-OP for now */
		      break;

		    default:				/* just ignore */
		      break;
		  }
	      }
#ifdef	_WINDOWS
	      else if(mp.button == M_BUTTON_RIGHT){
		  /*
		   * Toss generic popup on to the screen
		   */
		  if(sparms->mouse.popup)
		    if((*sparms->mouse.popup)(sparms, 0) == 2){
			done++;
		    }
	      }
#endif
	    }

	    break;
#endif	/* MOUSE */


            /*-------------- Display Resize -------------*/
          case MC_RESIZE :
	    if(sparms->resize_exit){
		SCRLCTRL_S *st = scroll_state(SS_CUR);
		long	    line;
		
		/*
		 * Figure out char offset of the char in the top left
		 * corner of the display.  Pass it back to the
		 * fetcher/formatter and have it pass the offset
		 * back to us...
		 */
		sparms->start.on = Offset;
		for(sparms->start.loc.offset = line = 0L;
		    line < cur_top_line;
		    line++)
		  sparms->start.loc.offset += scroll_handle_column(line, -1);

		done = 1;
		ClearLine(1);
		break;
	    }
	    /* else no reformatting neccessary, fall thru to repaint */


            /*-------------- refresh -------------*/
          case MC_REPAINT :
            num_display_lines = SCROLL_LINES(ps_global);
	    mark_status_dirty();
	    mark_keymenu_dirty();
	    mark_titlebar_dirty();
            ps_global->mangled_screen = 1;
	    force                     = 1;
            break;


            /*------- no op timeout to check for new mail ------*/
          case MC_NONE :
            break;


	    /*------- Forward displayed text ------*/
	  case MC_FWDTEXT :
	    forward_text(ps_global, sparms->text.text, sparms->text.src);
	    break;


	    /*----------- Save the displayed text ------------*/
	  case MC_SAVETEXT :
	    (void)simple_export(ps_global, sparms->text.text,
				sparms->text.src, "text", NULL);
	    break;


	    /*----------- Exit this screen ------------*/
	  case MC_EXIT :
	    done = 1;
	    break;


	    /*----------- Pop back to the Main Menu ------------*/
	  case MC_MAIN :
	    ps_global->next_screen = main_menu_screen;
	    done = 1;
	    break;


	    /*----------- Print ------------*/
	  case MC_PRINTTXT :
	    print_to_printer(sparms);
	    break;


	    /* ------- First handle on Line ------ */
	  case MC_GOTOBOL :
	    if(sparms->text.handles){
		next_handle = scroll_handle_boundary(sparms->text.handles, 
						     scroll_handle_prev_sel);

		break;
	    }
	    /* fall thru as bogus */

	    /* ------- Last handle on Line ------ */
	  case MC_GOTOEOL :
	    if(sparms->text.handles){
		next_handle = scroll_handle_boundary(sparms->text.handles, 
						     scroll_handle_next_sel);

		break;
	    }
	    /* fall thru as bogus */

            /*------- BOGUS INPUT ------*/
	  case MC_CHARRIGHT :
	  case MC_CHARLEFT :
          case MC_UNKNOWN :
	    if(sparms->bogus_input)
	      done = (*sparms->bogus_input)(ch);
	    else
	      bogus_command(ch, F_ON(F_USE_FK,ps_global) ? "F1" : "?");

            break;


	    /*------- Standard commands ------*/
          default:
	    whereis_pos.row = 0;
	    if(sparms->proc.tool)
	      result = (*sparms->proc.tool)(cmd, ps_global->msgmap, sparms);
	    else
	      result = process_cmd(ps_global, ps_global->mail_stream,
				   ps_global->msgmap, cmd, View, &force);

	    dprint(7, (debugfile, "PROCESS_CMD return: %d\n", result));

	    if(ps_global->next_screen != SCREEN_FUN_NULL || result == 1){
		done = 1;
		if(cmd == MC_FULLHDR){
		  if(ps_global->full_header == 1){
		      SCRLCTRL_S *st = scroll_state(SS_CUR);
		      long	    line;
		    
		      /*
		       * Figure out char offset of the char in the top left
		       * corner of the display.  Pass it back to the
		       * fetcher/formatter and have it pass the offset
		       * back to us...
		       */
		      sparms->start.on = Offset;
		      for(sparms->start.loc.offset = line = 0L;
			  line < cur_top_line;
			  line++)
			sparms->start.loc.offset +=
					scroll_handle_column(line, -1);
		  }
		  else
		    sparms->start.on = 0;

		  switch(km->which){
		    case 0:
		      sparms->keys.what = FirstMenu;
		      break;
		    case 1:
		      sparms->keys.what = SecondMenu;
		      break;
		    case 2:
		      sparms->keys.what = ThirdMenu;
		      break;
		    case 3:
		      sparms->keys.what = FourthMenu;
		      break;
		  }
		}
	    }
	    else if(!scroll_state(SS_CUR)){
		num_display_lines	  = SCROLL_LINES(ps_global);
		ps_global->mangled_screen = 1;
	    }

	    break;

	} /* End of switch() */

	/* Need to frame some handles? */
	if(sparms->text.handles
	   && ((!next_handle
		&& handle_on_page(sparms->text.handles, cur_top_line,
				  cur_top_line + num_display_lines))
	       || (next_handle
		   && handle_on_page(next_handle, cur_top_line,
				     cur_top_line + num_display_lines))))
	  next_handle = scroll_handle_in_frame(cur_top_line);

    } /* End of while() -- loop executing commands */

    ps_global->redrawer	= NULL;	/* next statement makes this invalid! */
    zero_scroll_text();		/* very important to zero out on return!!! */
    scroll_state(SS_FREE);
    if(sparms->bar.color)
      free_color_pair(&sparms->bar.color);

#ifdef	_WINDOWS
    scroll_setrange(0L, 0L);
#endif
    return(cmd);
}



/*----------------------------------------------------------------------
      Print text on paper

    Args:  text -- The text to print out
	   source -- What type of source text is
	   message -- Message for open_printer()
    Handling of error conditions is very poor.

  ----*/
static int
print_to_printer(sparms)
    SCROLL_S *sparms;
{
    char message[64];

    sprintf(message, "%s ", STYLE_NAME(sparms));

    if(open_printer(message) != 0)
      return(-1);

    switch(sparms->text.src){
      case CharStar :
	if(sparms->text.text != (char *)NULL)
	  print_text((char *)sparms->text.text);

	break;

      case CharStarStar :
	if(sparms->text.text != (char **)NULL){
	    register char **t;

	    for(t = sparms->text.text; *t != NULL; t++){
		print_text(*t);
		print_text(NEWLINE);
	    }
	}

	break;

      case FileStar :
	if(sparms->text.text != (FILE *)NULL) {
	    size_t n;
	    int i;

	    fseek((FILE *)sparms->text.text, 0L, 0);
	    n = SIZEOF_20KBUF - 1;
	    while(i = fread((void *)tmp_20k_buf, sizeof(char),
			    n, (FILE *)sparms->text.text)) {
		tmp_20k_buf[i] = '\0';
		print_text(tmp_20k_buf);
	    }
	}

      default :
	break;
    }

    close_printer();
    return(0);
}


/*----------------------------------------------------------------------
   Search text being viewed (help or message)

      Args: q_line      -- The screen line to prompt for search string on
            start_line  -- Line number in text to begin search on
            start_col   -- Column to begin search at in first line of text
            cursor_pos  -- position of cursor is returned to caller here
			   (Actually, this isn't really the position of the
			    cursor because we don't know where we are on the
			    screen.  So row is set to the line number and col
			    is set to the right column.)
            offset_in_line -- Offset where match was found.

    Result: returns line number string was found on
            -1 for cancel
            -2 if not found
            -3 if only match is at start_col - 1
	    -4 if search to first line
	    -5 if search to last line
 ---*/
int
search_text(q_line, start_line, start_col, report, cursor_pos, offset_in_line)
    int   q_line;
    long  start_line;
    int   start_col;
    char *report;
    Pos  *cursor_pos;
    int  *offset_in_line;
{
    char        prompt[MAX_SEARCH+50], nsearch_string[MAX_SEARCH+1];
    HelpType	help;
    int         rc, flags;
    static char search_string[MAX_SEARCH+1] = { '\0' };
    static ESCKEY_S word_search_key[] = { { 0, 0, "", "" },
					 {ctrl('Y'), 10, "^Y", "First Line"},
					 {ctrl('V'), 11, "^V", "Last Line"},
					 {-1, 0, NULL, NULL}
					};

    report[0] = '\0';
    sprintf(prompt, "Word to search for [%.*s] : ",
	    sizeof(prompt)-50, search_string);
    help = NO_HELP;
    nsearch_string[0] = '\0';

    while(1) {
	flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE | OE_KEEP_TRAILING_SPACE;
	
        rc = optionally_enter(nsearch_string, q_line, 0, sizeof(nsearch_string),
                              prompt, word_search_key, help, &flags);

        if(rc == 3) {
            help = help == NO_HELP ? h_oe_searchview : NO_HELP;
            continue;
        }
	else if(rc == 10){
	    strcpy(report, "Searched to First Line.");
	    return(-4);
	}
	else if(rc == 11){
	    strcpy(report, "Searched to Last Line."); 
	    return(-5);
	}

        if(rc != 4)
          break;
    }

    if(rc == 1 || (search_string[0] == '\0' && nsearch_string[0] == '\0'))
      return(-1);

    if(nsearch_string[0] != '\0'){
	strncpy(search_string, nsearch_string, sizeof(search_string)-1);
	search_string[sizeof(search_string)-1] = '\0';
    }

    rc = search_scroll_text(start_line, start_col, search_string, cursor_pos,
			    offset_in_line);
    return(rc);
}



/*----------------------------------------------------------------------
  Update the scroll tool's titlebar

    Args:  cur_top_line --
	   redraw -- flag to force updating

  ----*/
void
update_scroll_titlebar(cur_top_line, redraw)
    long      cur_top_line;
    int	      redraw;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int		num_display_lines = SCROLL_LINES(ps_global);
    long	new_line = (cur_top_line + num_display_lines > st->num_lines)
			     ? st->num_lines
			     : cur_top_line + num_display_lines;
    long        raw_msgno;
    COLOR_PAIR *returned_color = NULL;
    COLOR_PAIR *titlecolor = NULL;
    int         colormatch;
    SEARCHSET  *ss = NULL;

    if(st->parms->use_indexline_color
       && ps_global->titlebar_color_style != TBAR_COLOR_DEFAULT){
	raw_msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));
	if(raw_msgno > 0L && ps_global->mail_stream
	   && raw_msgno <= ps_global->mail_stream->nmsgs){
	    ss = mail_newsearchset();
	    ss->first = ss->last = (unsigned long) raw_msgno;
	}

	if(ss){
	    PAT_STATE  *pstate = NULL;

	    colormatch = get_index_line_color(ps_global->mail_stream,
					      ss, &pstate, &returned_color);
	    mail_free_searchset(&ss);

	    /*
	     * This is a bit tricky. If there is a colormatch but returned_color
	     * is NULL, that means that the user explicitly wanted the
	     * Normal color used in this index line, so that is what we
	     * use. If no colormatch then we will use the TITLE color
	     * instead of Normal.
	     */
	    if(colormatch){
		if(returned_color)
		  titlecolor = returned_color;
		else
		  titlecolor = new_color_pair(ps_global->VAR_NORM_FORE_COLOR,
					      ps_global->VAR_NORM_BACK_COLOR);
	    }

	    if(titlecolor
	       && ps_global->titlebar_color_style == TBAR_COLOR_REV_INDEXLINE){
		char cbuf[MAXCOLORLEN+1];

		strncpy(cbuf, titlecolor->fg, MAXCOLORLEN);
		strncpy(titlecolor->fg, titlecolor->bg, MAXCOLORLEN);
		strncpy(titlecolor->bg, cbuf, MAXCOLORLEN);
	    }
	}
	
	/* Did the color change? */
	if((!titlecolor && st->parms->bar.color)
	   ||
	   (titlecolor && !st->parms->bar.color)
	   ||
	   (titlecolor && st->parms->bar.color
	    && (strcmp(titlecolor->fg, st->parms->bar.color->fg)
		|| strcmp(titlecolor->bg, st->parms->bar.color->bg)))){

	    redraw++;
	    if(st->parms->bar.color)
	      free_color_pair(&st->parms->bar.color);
	    
	    st->parms->bar.color = titlecolor;
	    titlecolor = NULL;
	}

	if(titlecolor)
	  free_color_pair(&titlecolor);
    }


    if(redraw){
	set_titlebar(st->parms->bar.title, ps_global->mail_stream,
		     ps_global->context_current, ps_global->cur_folder,
		     ps_global->msgmap, 1, st->parms->bar.style,
		     new_line, st->num_lines, st->parms->bar.color);
	ps_global->mangled_header = 0;
    }
    else if(st->parms->bar.style == TextPercent)
      update_titlebar_lpercent(new_line);
    else
      update_titlebar_percent(new_line);
}



/*----------------------------------------------------------------------
  manager of global (to this module, anyway) scroll state structures


  ----*/
SCRLCTRL_S *
scroll_state(func)
    int func;
{
    struct scrollstack {
	SCRLCTRL_S	    s;
	struct scrollstack *prev;
    } *s;
    static struct scrollstack *stack = NULL;

    switch(func){
      case SS_CUR:			/* no op */
	break;
      case SS_NEW:
	s = (struct scrollstack *)fs_get(sizeof(struct scrollstack));
	memset((void *)s, 0, sizeof(struct scrollstack));
	s->prev = stack;
	stack  = s;
	break;
      case SS_FREE:
	if(stack){
	    s = stack->prev;
	    fs_give((void **)&stack);
	    stack = s;
	}
	break;
      default:				/* BUG: should complain */
	break;
    }

    return(stack ? &stack->s : NULL);
}



/*----------------------------------------------------------------------
      Save all the data for scrolling text and paint the screen


  ----*/
void
set_scroll_text(sparms, current_line, st)
    SCROLL_S	*sparms;
    long	 current_line;
    SCRLCTRL_S	*st;
{
    /* save all the stuff for possible asynchronous redraws */
    st->parms		   = sparms;
    st->top_text_line      = current_line;
    st->screen.start_line  = SCROLL_LINES_ABOVE(ps_global);
    st->screen.other_lines = SCROLL_LINES_ABOVE(ps_global)
				+ SCROLL_LINES_BELOW(ps_global);
    st->screen.width	   = -1;	/* Force text formatting calculation */
}



/*----------------------------------------------------------------------
     Redraw the text on the screen, possibly reformatting if necessary

   Args None

 ----*/
void
redraw_scroll_text()
{
    int		i, offset;
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    format_scroll_text();

    offset = (st->parms->text.src == FileStar) ? 0 : st->top_text_line;

#ifdef _WINDOWS
    mswin_beginupdate();
#endif
    /*---- Actually display the text on the screen ------*/
    for(i = 0; i < st->screen.length; i++){
	ClearLine(i + st->screen.start_line);
	if((offset + i) < st->num_lines) 
	  PutLine0n8b(i + st->screen.start_line, 0, st->text_lines[offset + i],
		      st->line_lengths[offset + i], st->parms->text.handles);
    }


    fflush(stdout);
#ifdef _WINDOWS
    mswin_endupdate();
#endif
}



/*----------------------------------------------------------------------
  Free memory used as scrolling buffers for text on disk.  Also mark
  text_lines as available
  ----*/
void
zero_scroll_text()
{
    SCRLCTRL_S	 *st = scroll_state(SS_CUR);
    register int  i;

    for(i = 0; i < st->lines_allocated; i++)
      if(st->parms->text.src == FileStar && st->text_lines[i])
	fs_give((void **)&st->text_lines[i]);
      else
	st->text_lines[i] = NULL;

    if(st->parms->text.src == FileStar && st->findex != NULL){
	fclose(st->findex);
	st->findex = NULL;
	if(st->fname){
	    unlink(st->fname);
	    fs_give((void **)&st->fname);
	}
    }

    if(st->text_lines)
      fs_give((void **)&st->text_lines);

    if(st->line_lengths)
      fs_give((void **) &st->line_lengths);
}



/*----------------------------------------------------------------------

Always format at least 20 chars wide. Wrapping lines would be crazy for
screen widths of 1-20 characters 
  ----*/
void
format_scroll_text()
{
    int		     i;
    char	    *p, **pp;
    SCRLCTRL_S	    *st = scroll_state(SS_CUR);
    register short  *ll;
    register char  **tl, **tl_end;

    if(!st || (st->screen.width == (i = ps_global->ttyo->screen_cols)
	       && st->screen.length == PGSIZE(st)))
        return;

    st->screen.width = max(20, i);
    st->screen.length = PGSIZE(st);

    if(st->lines_allocated == 0) {
        st->lines_allocated = TYPICAL_BIG_MESSAGE_LINES;
        st->text_lines = (char **)fs_get(st->lines_allocated *sizeof(char *));
	memset(st->text_lines, 0, st->lines_allocated * sizeof(char *));
        st->line_lengths = (short *)fs_get(st->lines_allocated *sizeof(short));
    }

    tl     = st->text_lines;
    ll     = st->line_lengths;
    tl_end = &st->text_lines[st->lines_allocated];

    if(st->parms->text.src == CharStarStar) {
        /*---- original text is already list of lines -----*/
        /*   The text could be wrapped nicely for narrow screens; for now
             it will get truncated as it is displayed */
        for(pp = (char **)st->parms->text.text; *pp != NULL;) {
            *tl++ = *pp++;
            *ll++ = st->screen.width;
            if(tl >= tl_end) {
		i = tl - st->text_lines;
                st->lines_allocated *= 2;
                fs_resize((void **)&st->text_lines,
                          st->lines_allocated * sizeof(char *));
                fs_resize((void **)&st->line_lengths,
                          st->lines_allocated*sizeof(short));
                tl     = &st->text_lines[i];
                ll     = &st->line_lengths[i];
                tl_end = &st->text_lines[st->lines_allocated];
            }
        }

	st->num_lines = tl - st->text_lines;
    }
    else if (st->parms->text.src == CharStar) {
	/*------ Format the plain text ------*/
	for(p = (char *)st->parms->text.text; *p; ) {
            *tl = p;

	    for(; *p && !(*p == RETURN || *p == LINE_FEED); p++)
	      ;

	    *ll = p - *tl;
	    ll++; tl++;
	    if(tl >= tl_end) {
		i = tl - st->text_lines;
		st->lines_allocated *= 2;
		fs_resize((void **)&st->text_lines,
			  st->lines_allocated * sizeof(char *));
		fs_resize((void **)&st->line_lengths,
			  st->lines_allocated*sizeof(short));
		tl     = &st->text_lines[i];
		ll     = &st->line_lengths[i];
		tl_end = &st->text_lines[st->lines_allocated];
	    }

	    if(*p == '\r' && *(p+1) == '\n') 
	      p += 2;
	    else if(*p == '\n' || *p == '\r')
	      p++;
	}

	st->num_lines = tl - st->text_lines;
    }
    else {
	/*------ Display text is in a file --------*/

	/*
	 * This is pretty much only useful under DOS where we can't fit
	 * all of big messages in core at once.  This scheme makes
	 * some simplifying assumptions:
	 *  1. Lines are on disk just the way we'll display them.  That
	 *     is, line breaks and such are left to the function that
	 *     writes the disk file to catch and fix.
	 *  2. We get away with this mainly because the DOS display isn't
	 *     going to be resized out from under us.
	 *
	 * The idea is to use the already alloc'd array of char * as a 
	 * buffer for sections of what's on disk.  We'll set up the first
	 * few lines here, and read new ones in as needed in 
	 * scroll_scroll_text().
	 *  
	 * but first, make sure there are enough buffer lines allocated
	 * to serve as a place to hold lines from the file.
	 *
	 *   Actually, this is also used under windows so the display will
	 *   be resized out from under us.  So I changed the following
	 *   to always
	 *	1.  free old text_lines, which may have been allocated
	 *	    for a narrow screen.
	 *	2.  insure we have enough text_lines
	 *	3.  reallocate all text_lines that are needed.
	 *   (tom unger  10/26/94)
	 */

	/* free old text lines, which may be too short. */
	for(i = 0; i < st->lines_allocated; i++)
	  if(st->text_lines[i]) 		/* clear alloc'd lines */
	    fs_give((void **)&st->text_lines[i]);

        /* Insure we have enough text lines. */
	if(st->lines_allocated < (2 * PGSIZE(st)) + 1){
	    st->lines_allocated = (2 * PGSIZE(st)) + 1; /* resize */

	    fs_resize((void **)&st->text_lines,
		      st->lines_allocated * sizeof(char *));
	    memset(st->text_lines, 0, st->lines_allocated * sizeof(char *));
	    fs_resize((void **)&st->line_lengths,
		      st->lines_allocated*sizeof(short));
	}

	/* reallocate all text lines that are needed. */
	for(i = 0; i <= PGSIZE(st); i++)
	  if(st->text_lines[i] == NULL)
	    st->text_lines[i] = (char *)fs_get((st->screen.width + 1)
							       * sizeof(char));

	tl = &st->text_lines[i];

	st->num_lines = make_file_index();

	ScrollFile(st->top_text_line);		/* then load them up */
    }

    /*
     * Efficiency hack.  If there are handles, fill in their
     * line number field for later...
     */
    if(st->parms->text.handles){
	long	  line;
	int	  i, col, n, key;
	HANDLE_S *h;

	for(line = 0; line < st->num_lines; line++)
	  for(i = 0, col = 0; i < st->line_lengths[line];)
	    switch(st->text_lines[line][i]){
	      case TAG_EMBED:
		i++;
		switch((i < st->line_lengths[line]) ? st->text_lines[line][i]
						    : 0){
		  case TAG_HANDLE:
		    for(key = 0, n = st->text_lines[line][++i]; n > 0; n--)
		      key = (key * 10) + (st->text_lines[line][++i] - '0');

		    i++;
		    for(h = st->parms->text.handles; h; h = h->next)
		      if(h->key == key){
			  scroll_handle_set_loc(&h->loc, line, col);
			  break;
		      }

		    if(!h)	/* anything behind us? */
		      for(h = st->parms->text.handles->prev; h; h = h->prev)
			if(h->key == key){
			    scroll_handle_set_loc(&h->loc, line, col);
			    break;
			}
		    
		    break;

		  case TAG_FGCOLOR :
		  case TAG_BGCOLOR :
		    i += (RGBLEN + 1);	/* 1 for TAG, RGBLEN for color */
		    break;

		  case TAG_INVON:
		  case TAG_INVOFF:
		  case TAG_BOLDON:
		  case TAG_BOLDOFF:
		  case TAG_ULINEON:
		  case TAG_ULINEOFF:
		    i++;
		    break;
		
		  default:	/* literal embed char */
		    break;
		}

		break;

	      case ESCAPE:
		if(n = match_escapes(&st->text_lines[line][++i]))
		  i += (n-1);	/* Don't count escape for column */

		break;

	      case TAB:
		i++;
		while(((++col) &  0x07) != 0) /* add tab's spaces */
		  ;

		break;

	      default:
		i++, col++;
		break;
	    }
    }

#ifdef	_WINDOWS
    scroll_setrange (st->screen.length, st->num_lines);
#endif

    *tl = NULL;
}




/*
 * ScrollFile - scroll text into the st struct file making sure 'line'
 *              of the file is the one first in the text_lines buffer.
 *
 *   NOTE: talk about massive potential for tuning...
 *         Goes without saying this is still under constuction
 */
void
ScrollFile(line)
    long line;
{
    SCRLCTRL_S	 *st = scroll_state(SS_CUR);
    SCRLFILE_S	  sf;
    register int  i;

    if(line <= 0){		/* reset and load first couple of pages */
	fseek((FILE *) st->parms->text.text, 0L, 0);
	line = 0L;
    }

    if(!st->text_lines)
      return;

    for(i = 0; i < PGSIZE(st); i++){
	/*** do stuff to get the file pointer into the right place ***/
	/*
	 * BOGUS: this is painfully crude right now, but I just want to get
	 * it going. 
	 *
	 * possibly in the near furture, an array of indexes into the 
	 * file that are the offset for the beginning of each line will
	 * speed things up.  Of course, this
	 * will have limits, so maybe a disk file that is an array
	 * of indexes is the answer.
	 */
	if(fseek(st->findex, (size_t)(line++) * sizeof(SCRLFILE_S), 0) < 0
	   || fread(&sf, sizeof(SCRLFILE_S), (size_t)1, st->findex) != 1
	   || fseek((FILE *) st->parms->text.text, sf.offset, 0) < 0
	   || !st->text_lines[i]
	   || (sf.len && !fgets(st->text_lines[i], sf.len + 1,
				(FILE *) st->parms->text.text)))
	  break;

	st->line_lengths[i] = sf.len;
    }

    for(; i < PGSIZE(st); i++)
      if(st->text_lines[i]){		/* blank out any unused lines */
	  *st->text_lines[i]  = '\0';
	  st->line_lengths[i] = 0;
      }
}


/*
 * make_file_index - do a single pass over the file containing the text
 *                   to display, recording line lengths and offsets.
 *    NOTE: This is never really to be used on a real OS with virtual
 *          memory.  This is the whole reason st->findex exists.  Don't
 *          want to waste precious memory on a stupid array that could 
 *          be very large.
 */
long
make_file_index()
{
    SCRLCTRL_S	  *st = scroll_state(SS_CUR);
    SCRLFILE_S	   sf;
    long	   l = 0L;
    int		   state = 0;

    if(!st->findex){
	if(!st->fname)
	  st->fname = temp_nam(NULL, "pi");

	if(!st->fname || (st->findex = fopen(st->fname,"w+b")) == NULL){
	    if(st->fname){
		(void)unlink(st->fname);
		fs_give((void **)&st->fname);
	    }

	    return(0);
	}
    }
    else
      fseek(st->findex, 0L, 0);

    fseek((FILE *)st->parms->text.text, 0L, 0);

    while(1){
	sf.len = st->screen.width + 1;
	if(scroll_file_line((FILE *) st->parms->text.text,
			    tmp_20k_buf, &sf, &state)){
	    fwrite((void *) &sf, sizeof(SCRLFILE_S), (size_t)1, st->findex);
	    l++;
	}
	else
	  break;
    }

    fseek((FILE *)st->parms->text.text, 0L, 0);

    return(l);
}



/*----------------------------------------------------------------------
     Get the next line to scroll from the given file

 ----*/
char *
scroll_file_line(fp, buf, sfp, wrapt)
    FILE       *fp;
    char       *buf;
    SCRLFILE_S *sfp;
    int	       *wrapt;
{
    register char *s = NULL;

    while(1){
	if(!s){
	    sfp->offset = ftell(fp);
	    if(!(s = fgets(buf, sfp->len, fp)))
	      return(NULL);			/* can't grab a line? */
	}

	if(!*s){
	    *wrapt = 1;			/* remember; that we wrapped */
	    break;
	}
	else if(*s == NEWLINE[0] && (!NEWLINE[1] || *(s+1) == NEWLINE[1])){
	    int empty = (*wrapt && s == buf);

	    *wrapt = 0;			/* turn off wrapped state */
	    if(empty)
	      s = NULL;			/* get a new line */
	    else
	      break;			/* done! */
	}
	else 
	  s++;
    }

    sfp->len = s - buf;
    return(buf);
}



/*----------------------------------------------------------------------
     Scroll the text on the screen

   Args:  new_top_line -- The line to be displayed on top of the screen
          redraw -- Flag to force a redraw even in nothing changed 

   Returns: resulting top line
   Note: the returned line number may be less than new_top_line if
	 reformatting caused the total line count to change.

 ----*/
long
scroll_scroll_text(new_top_line, handle, redraw)
    long      new_top_line;
    HANDLE_S *handle;
    int	      redraw;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int		num_display_lines, l, top;
    POSLIST_S  *lp, *lp2;

    /* When this is true, we're still on the same page of the display. */
    if(st->top_text_line == new_top_line && !redraw){
	/* handle changed, so hilite the new handle and unhilite the old */
	if(handle && handle != st->parms->text.handles){
	    top = st->screen.start_line - new_top_line;
	    /* hilite the new one */
	    if(!scroll_handle_obscured(handle))
	      for(lp = handle->loc; lp; lp = lp->next)
		if((l = lp->where.row) >= st->top_text_line
		   && l < st->top_text_line + st->screen.length){
		    if(st->parms->text.src == FileStar)
		      l -= new_top_line;

		    PutLine0n8b(top + lp->where.row, 0, st->text_lines[l],
				st->line_lengths[l], handle);
		}

	    /* unhilite the old one */
	    if(!scroll_handle_obscured(st->parms->text.handles))
	      for(lp = st->parms->text.handles->loc; lp; lp = lp->next)
	       if((l = lp->where.row) >= st->top_text_line
		  && l < st->top_text_line + st->screen.length){
		   for(lp2 = handle->loc; lp2; lp2 = lp2->next)
		     if(l == lp2->where.row)
		       break;

		   if(!lp2){
		       if(st->parms->text.src == FileStar)
			 l -= new_top_line;

		       PutLine0n8b(top + lp->where.row, 0, st->text_lines[l],
				   st->line_lengths[l], handle);
		   }
	       }

	    st->parms->text.handles = handle;	/* update current */
	}

	return(new_top_line);
    }

    num_display_lines = PGSIZE(st);

    format_scroll_text();

    if(st->top_text_line >= st->num_lines)	/* don't pop line count */
      new_top_line = st->top_text_line = max(st->num_lines - 1, 0);

    if(st->parms->text.src == FileStar)
      ScrollFile(new_top_line);		/* set up new st->text_lines */

#ifdef	_WINDOWS
    scroll_setrange (st->screen.length, st->num_lines);
    scroll_setpos (new_top_line);
#endif

    /* --- 
       Check out the scrolling situation. If we want to scroll, but BeginScroll
       says we can't then repaint,  + 10 is so we repaint most of the time.
      ----*/
    if(redraw ||
       (st->top_text_line - new_top_line + 10 >= num_display_lines ||
        new_top_line - st->top_text_line + 10 >= num_display_lines) ||
	BeginScroll(st->screen.start_line,
                    st->screen.start_line + num_display_lines - 1) != 0) {
        /* Too much text to scroll, or can't scroll -- just repaint */

	if(handle)
	  st->parms->text.handles = handle;

        st->top_text_line = new_top_line;
        redraw_scroll_text();
    }
    else{
	/*
	 * We're going to scroll the screen, but first we have to make sure
	 * the old hilited handles are unhilited if they are going to remain
	 * on the screen.
	 */
	top = st->screen.start_line - st->top_text_line;
	if(handle && handle != st->parms->text.handles
	   && st->parms->text.handles
	   && !scroll_handle_obscured(st->parms->text.handles))
	  for(lp = st->parms->text.handles->loc; lp; lp = lp->next)
	   if((l = lp->where.row) >= max(st->top_text_line,new_top_line)
	      && l < min(st->top_text_line,new_top_line) + st->screen.length){
	       if(st->parms->text.src == FileStar)
		 l -= new_top_line;

	       PutLine0n8b(top + lp->where.row, 0, st->text_lines[l],
			   st->line_lengths[l], handle);
	   }

	if(new_top_line > st->top_text_line){
	    /*------ scroll down ------*/
	    while(new_top_line > st->top_text_line) {
		ScrollRegion(1);

		l = (st->parms->text.src == FileStar)
		      ? num_display_lines - (new_top_line - st->top_text_line)
		      : st->top_text_line + num_display_lines;

		if(l < st->num_lines){
		  PutLine0n8b(st->screen.start_line + num_display_lines - 1,
			      0, st->text_lines[l], st->line_lengths[l],
			      handle ? handle : st->parms->text.handles);
		  /*
		   * We clear to the end of line in the right background
		   * color. If the line was exactly the width of the screen
		   * then PutLine0n8b will have left _col and _row moved to
		   * the start of the next row. We don't need or want to clear
		   * that next row.
		   */
		  if(pico_usingcolor()
		     && (st->line_lengths[l] < ps_global->ttyo->screen_cols
		         || visible_linelen(l) < ps_global->ttyo->screen_cols))
		    CleartoEOLN();
		}

		st->top_text_line++;
	    }
	}
	else{
	    /*------ scroll up -----*/
	    while(new_top_line < st->top_text_line) {
		ScrollRegion(-1);

		st->top_text_line--;
		l = (st->parms->text.src == FileStar)
		      ? st->top_text_line - new_top_line
		      : st->top_text_line;
		PutLine0n8b(st->screen.start_line, 0, st->text_lines[l],
			    st->line_lengths[l],
			    handle ? handle : st->parms->text.handles);
		/*
		 * We clear to the end of line in the right background
		 * color. If the line was exactly the width of the screen
		 * then PutLine0n8b will have left _col and _row moved to
		 * the start of the next row. We don't need or want to clear
		 * that next row.
		 */
		if(pico_usingcolor()
		   && (st->line_lengths[l] < ps_global->ttyo->screen_cols
		       || visible_linelen(l) < ps_global->ttyo->screen_cols))
		  CleartoEOLN();
	    }
	}

	EndScroll();

	if(handle && handle != st->parms->text.handles){
	    POSLIST_S *lp;

	    for(lp = handle->loc; lp; lp = lp->next)
	      if(lp->where.row >= st->top_text_line
		 && lp->where.row < st->top_text_line + st->screen.length){
		  PutLine0n8b(st->screen.start_line
					 + (lp->where.row - st->top_text_line),
			      0, st->text_lines[lp->where.row],
			      st->line_lengths[lp->where.row],
			      handle);
		  
	      }

	    st->parms->text.handles = handle;
	}

	fflush(stdout);
    }

    return(new_top_line);
}


/*---------------------------------------------------------------------
  Edit individual char in text so that the entire text doesn't need
  to be completely reformatted. 

  Returns 0 if there were no errors, 1 if we would like the entire
  text to be reformatted.
----*/
int
ng_scroll_edit(context, index)
     CONTEXT_S *context;
     int   index;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    char *ngp, tmp[MAILTMPLEN+10];
    int   len;
    FOLDER_S *f;

    if (!(f = folder_entry(index, FOLDERS(context))))
      return 1;
    if(f->subscribed)
      return 0;  /* nothing in scroll needs to be changed */
    tmp[0] = TAG_HANDLE;
    sprintf(tmp+2, "%d", st->parms->text.handles->key);
    tmp[1] = len = strlen(tmp+2);
    sprintf(tmp+len+2, "%s ", f->selected ? "[ ]" : "[X]");
    sprintf(tmp+len+6, "%.*s", MAILTMPLEN, f->name);
    
    ngp = *(st->text_lines);

    ngp = strstr(ngp, tmp);

    if(!ngp) return 1;
    ngp += 3+len;

    /* assumption that text is of form "[ ] xxx.xxx" */

    if(ngp){
      if(*ngp == 'X'){
	*ngp = ' ';
	return 0;
      }
      else if (*ngp == ' '){
	*ngp = 'X';
	return 0;
      }
    }
    return 1;
}


/*---------------------------------------------------------------------
  Similar to ng_scroll_edit, but this is the more general case of 
  selecting a folder, as opposed to selecting a newsgroup for 
  subscription while in listmode.

  Returns 0 if there were no errors, 1 if we would like the entire
  text to be reformatted.
----*/
int
folder_select_update(context, index)
     CONTEXT_S *context;
     int   index;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    FOLDER_S *f;
    char *ngp, tmp[MAILTMPLEN+10];
    int len, total, fnum, num_sel = 0;

    if (!(f = folder_entry(index, FOLDERS(context))))
      return 1;
    ngp = *(st->text_lines);

    total = folder_total(FOLDERS(context));
      
    for (fnum = 0; num_sel < 2 && fnum < total; fnum++)
      if(folder_entry(fnum, FOLDERS(context))->selected)
	num_sel++;
    if(!num_sel || (f->selected && num_sel == 1))
      return 1;   /* need to reformat the whole thing */

    tmp[0] = TAG_HANDLE;
    sprintf(tmp+2, "%d", st->parms->text.handles->key);  
    tmp[1] = len = strlen(tmp+2);

    ngp = strstr(ngp, tmp);
    if(!ngp) return 1;

    if(F_ON(F_SELECTED_SHOWN_BOLD, ps_global)){
      ngp += 2 + len;
      while(*ngp && ngp[0] != TAG_EMBED 
	    && ngp[1] != (f->selected ? TAG_BOLDOFF : TAG_BOLDON)
	    && *ngp != *(f->name))
	ngp++;

      if (!(*ngp) || (*ngp == *(f->name)))
	return 1;
      else {
	ngp++;
	*ngp = (f->selected ? TAG_BOLDON : TAG_BOLDOFF);
	return 0;
      }
    }
    else{
      while(*ngp != ' ' && *ngp != *(f->name) && *ngp)
	ngp++;
      if(!(*ngp) || (*ngp == *(f->name)))
	return 1;
      else {
	ngp++;
	*ngp = f->selected ? 'X' : ' ';
	return 0;
      }
    }
}

/*---------------------------------------------------------------------
  We gotta go through all of the formatted text and add "[ ] " in the right
  place.  If we don't do this, we must completely reformat the whole text,
  which could take a very long time.

  Return 1 if we encountered some sort of error and we want to reformat the
  whole text, return 0 if everything went as planned.

  ASSUMPTION: for this to work, we assume that there are only total
  number of handles, numbered 1 through total.
----*/
int
scroll_add_listmode(context, total)
     CONTEXT_S *context;
     int total;
{
    SCRLCTRL_S	    *st = scroll_state(SS_CUR);
    long             i;
    char            *ngp, *ngname, handle_str[MAILTMPLEN];
    HANDLE_S        *h;


    ngp = *(st->text_lines);
    h = st->parms->text.handles;

    while(h && h->key != 1 && h->prev)
      h = h->prev;
    if (!h) return 1;
    handle_str[0] = TAG_EMBED;
    handle_str[1] = TAG_HANDLE;
    for(i = 1; i <= total && h; i++, h = h->next){
      sprintf(handle_str+3, "%d", h->key);
      handle_str[2] = strlen(handle_str+3);
      ngp = strstr(ngp, handle_str);
      if(!ngp){
	ngp = *(st->text_lines);
	if (!ngp)
	  return 1;
      }
      ngname = ngp + strlen(handle_str);
      while (strncmp(ngp, "    ", 4) && !(*ngp == '\n')
	     && !(ngp == *(st->text_lines)))
	ngp--;
      if (strncmp(ngp, "    ", 4)) 
	return 1;
      while(ngp+4 != ngname && *ngp){
	ngp[0] = ngp[4];
	ngp++;
      }

      if(folder_entry(h->h.f.index, FOLDERS(context))->subscribed){
	ngp[0] = 'S';
	ngp[1] = 'U';
	ngp[2] = 'B';
      }
      else{
	ngp[0] = '[';
	ngp[1] = ' ';
	ngp[2] = ']';
      }
      ngp[3] = ' ';
    }

    return 0;
}

/*----------------------------------------------------------------------
      Search the set scrolling text

   Args:   start_line -- line to start searching on
	   start_col  -- column to start searching at in first line
           word       -- string to search for
           cursor_pos -- position of cursor is returned to caller here
			 (Actually, this isn't really the position of the
			  cursor because we don't know where we are on the
			  screen.  So row is set to the line number and col
			  is set to the right column.)
           offset_in_line -- Offset where match was found.

   Returns: the line the word was found on, or -2 if it wasn't found, or
	    -3 if the only match is at column start_col - 1.

 ----*/
int
search_scroll_text(start_line, start_col, word, cursor_pos, offset_in_line)
    long  start_line;
    int   start_col;
    char *word;
    Pos  *cursor_pos;
    int  *offset_in_line;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    char       *wh, loc_word[MAX_SEARCH + 1];
    long	l, offset, dlines;
#define	SROW(N)		((N) - offset)
#define	SLINE(N)	st->text_lines[SROW(N)]
#define	SLEN(N)		st->line_lengths[SROW(N)]

    dlines = PGSIZE(st);
    offset = (st->parms->text.src == FileStar) ? st->top_text_line : 0;

    /* lower the case once rather than in each search_scroll_line */
    strncpy(loc_word, word, sizeof(loc_word)-1);
    loc_word[sizeof(loc_word)-1] = '\0';
    lcase((unsigned char *) loc_word);

    if(start_line < st->num_lines){
	/* search first line starting at position start_col in */
	if((wh = search_scroll_line(SLINE(start_line) + start_col,
				    loc_word,
				    SLEN(start_line) - start_col,
				    st->parms->text.handles != NULL)) != NULL){
	    cursor_pos->row = start_line;
	    cursor_pos->col = scroll_handle_column(SROW(start_line),
				     *offset_in_line = wh - SLINE(start_line));
	    return(start_line);
	}

	if(st->parms->text.src == FileStar)
	  offset++;

	for(l = start_line + 1; l < st->num_lines; l++) {
	    if(st->parms->text.src == FileStar && l > offset + dlines)
	      ScrollFile(offset += dlines);

	    if((wh = search_scroll_line(SLINE(l), loc_word, SLEN(l),
				    st->parms->text.handles != NULL)) != NULL){
		cursor_pos->row = l;
		cursor_pos->col = scroll_handle_column(SROW(l), 
					      *offset_in_line = wh - SLINE(l));
		return(l);
	    }
	}
    }
    else
      start_line = st->num_lines;

    if(st->parms->text.src == FileStar)		/* wrap offset */
      ScrollFile(offset = 0);

    for(l = 0; l < start_line; l++) {
	if(st->parms->text.src == FileStar && l > offset + dlines)
	  ScrollFile(offset += dlines);

	if((wh = search_scroll_line(SLINE(l), loc_word, SLEN(l),
				    st->parms->text.handles != NULL)) != NULL){
	    cursor_pos->row = l;
	    cursor_pos->col = scroll_handle_column(SROW(l),
					      *offset_in_line = wh - SLINE(l));
	    return(l);
	}
    }

    /* search in current line */
    if((wh = search_scroll_line(SLINE(start_line), loc_word,
				start_col + strlen(loc_word) - 2,
				st->parms->text.handles != NULL)) != NULL){
	cursor_pos->row = start_line;
	cursor_pos->col = scroll_handle_column(SROW(start_line),
				     *offset_in_line = wh - SLINE(start_line));

	return(start_line);
    }

    /* see if the only match is a repeat */
    if(start_col > 0 && (wh = search_scroll_line(
				SLINE(start_line) + start_col - 1,
				loc_word, strlen(loc_word),
				st->parms->text.handles != NULL)) != NULL){
	cursor_pos->row = start_line;
	cursor_pos->col = scroll_handle_column(SROW(start_line), 
				     *offset_in_line = wh - SLINE(start_line));
	return(-3);
    }

    return(-2);
}




/*----------------------------------------------------------------------
   Search one line of scroll text for given string

   Args:  is -- The string to search in, the larger string
          ss -- The string to search for, the smaller string
	  n  -- The max number of chars in is to search

   Search for first occurrence of ss in the is, and return a pointer
   into the string is when it is found. The search is case indepedent.
  ----*/
char *	    
search_scroll_line(is, ss, n, handles)
    char *is, *ss;
    int   n, handles;
{
    register char *p, *q;
    int		  state = 0;

    if(ss && is)
      for(; n-- > 0 && *is; is++){
	  /* excluding tag! */

	  if(handles)
	    switch(state){
	      case 0 :
		if(*is == TAG_EMBED){
		    state = -1;
		    continue;
		}

		break;

	      case -1 :
		state = (*is == TAG_HANDLE)
			  ? -2
			  : (*is == TAG_FGCOLOR
			     || *is == TAG_BGCOLOR)
				? RGBLEN : 0;
		continue;

	      case -2 :
		state = *is;	/* length of handle's key */
		continue;

	      default :
		state--;
		continue;
	    }

	  for(p = ss, q = is; ; p++, q++){
	      if(!*p)
		return(is);
	      else if(n < p - ss || !*q)	/* len(ss) > len(is) */
		return(NULL);
	      else if(*p != *q && !CMPNOCASE(*p, *q))
		break;
	  }
      }

    return(NULL);
}



/*
 * Returns the number of columns taken up by the visible part of the line.
 * That is, account for handles and color changes and so forth.
 */
int
visible_linelen(line)
    int line;
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int i, n, len = 0;

    if(line < 0 || line >= st->num_lines)
      return(len);

    for(i = 0, len = 0; i < st->line_lengths[line];)
      switch(st->text_lines[line][i]){

        case TAG_EMBED:
	  i++;
	  switch((i < st->line_lengths[line]) ? st->text_lines[line][i] : 0){
	    case TAG_HANDLE:
	      i++;
	      /* skip the length byte plus <length> more bytes */
	      if(i < st->line_lengths[line]){
		  n = st->text_lines[line][i];
		  i++;
	      }

	      if(i < st->line_lengths[line] && n > 0)
		i += n;

		break;

	    case TAG_FGCOLOR :
	    case TAG_BGCOLOR :
	      i += (RGBLEN + 1);	/* 1 for TAG, RGBLEN for color */
	      break;

	    case TAG_INVON:
	    case TAG_INVOFF:
	    case TAG_BOLDON:
	    case TAG_BOLDOFF:
	    case TAG_ULINEON:
	    case TAG_ULINEOFF:
	      i++;
	      break;

	    case TAG_EMBED:		/* escaped embed character */
	      i++;
	      len++;
	      break;
	    
	    default:			/* the embed char was literal */
	      i++;
	      len += 2;
	      break;
	  }

	  break;

	case ESCAPE:
	  i++;
	  if(i < st->line_lengths[line]
	     && (n = match_escapes(&st->text_lines[line][i])))
	    i += (n-1);			/* Don't count escape in length */

	  break;

	case TAB:
	  i++;
	  while(((++len) &  0x07) != 0)		/* add tab's spaces */
	    ;

	  break;

	default:
	  i++;
	  len++;
	  break;
      }

    return(len);
}



char *    
display_parameters(params)
     PARAMETER *params;
{
    int		n, longest = 0;
    char       *d;
    PARAMETER  *p;
    PARMLIST_S *parmlist;

    for(p = params; p; p = p->next)	/* ok if we include *'s */
      if(p->attribute && (n = strlen(p->attribute)) > longest)
	longest = min(32, n);   /* shouldn't be any bigger than 32 */

    d = tmp_20k_buf;
    if(parmlist = rfc2231_newparmlist(params)){
	n = 0;			/* n overloaded */
	while(rfc2231_list_params(parmlist) && d < tmp_20k_buf + 10000){
	    if(n++){
		sprintf(d,"\n");
		d += strlen(d);
	    }

            sprintf(d, "%-*s: %s", longest, parmlist->attrib,
		    parmlist->value ? strsquish(tmp_20k_buf + 11000,
						parmlist->value, 100)
				    : "");
            d += strlen(d);
	}

	rfc2231_free_parmlist(&parmlist);
    }
    else
      tmp_20k_buf[0] = '\0';

    return(tmp_20k_buf);
}



/*----------------------------------------------------------------------
    Display the contents of the given file (likely output from some command)

  Args: filename -- name of file containing output
	title -- title to be used for screen displaying output
	alt_msg -- if no output, Q this message instead of the default
	mode -- non-zero to display short files in status line
  Returns: none
 ----*/
void
display_output_file(filename, title, alt_msg, mode)
    char *filename, *title, *alt_msg;
    int mode;
{
    FILE *f;

#if defined(DOS) || defined(OS2)
#define OUTPUT_FILE_READ_MODE "rb"
#else
#define OUTPUT_FILE_READ_MODE "r"
#endif

    if(f = fopen(filename, OUTPUT_FILE_READ_MODE)){
	if(mode == DOF_BRIEF){
	    int  msg_q = 0, i = 0;
	    char buf[512], *msg_p[4];
#define	MAX_SINGLE_MSG_LEN	60

	    buf[0]   = '\0';
	    msg_p[0] = buf;
	    while(fgets(msg_p[msg_q], sizeof(buf) - (msg_p[msg_q] - buf), f) 
		  && msg_q < 3
		  && (i = strlen(msg_p[msg_q])) < MAX_SINGLE_MSG_LEN){
		msg_p[msg_q+1] = msg_p[msg_q]+strlen(msg_p[msg_q]);
		if (*(msg_p[++msg_q] - 1) == '\n')
		  *(msg_p[msg_q] - 1) = '\0';
	    }

	    if(msg_q < 3 && i < MAX_SINGLE_MSG_LEN){
		if(*msg_p[0])
		  for(i = 0; i < msg_q; i++)
		    q_status_message2(SM_ORDER, 3, 4,
				      "%.200s Result: %.200s", title, msg_p[i]);
		else
		  q_status_message2(SM_ORDER, 0, 4, "%.200s%.200s", title,
				    alt_msg
				      ? alt_msg
				      : " command completed with no output");

		fclose(f);
		f = NULL;
	    }
	}
	else if(mode == DOF_EMPTY){
	    char c;

	    if(fread(&c, sizeof(char), (size_t) 1, f) < 1){
		q_status_message2(SM_ORDER, 0, 4, "%.200s%.200s", title,
				  alt_msg
				    ? alt_msg
				    : " command completed with no output");
		fclose(f);
		f = NULL;
	    }
	}

	if(f){
	    SCROLL_S sargs;
	    char     title_buf[64];

	    sprintf(title_buf, "HELP FOR %s VIEW", title);

	    memset(&sargs, 0, sizeof(SCROLL_S));
	    sargs.text.text  = f;
	    sargs.text.src   = FileStar;
	    sargs.text.desc  = "output";
	    sargs.bar.title  = title;
	    sargs.bar.style  = TextPercent;
	    sargs.help.text  = h_simple_text_view;
	    sargs.help.title = title_buf;
	    scrolltool(&sargs);
	    ps_global->mangled_screen = 1;
	    fclose(f);
	}

	unlink(filename);
    }
    else
      dprint(2, (debugfile, "Error reopening %s to get results: %s\n",
		 filename ? filename : "?", error_description(errno)));
}


/*----------------------------------------------------------------------
      Fetch the requested header fields from the msgno specified

   Args: stream -- mail stream of open folder
         msgno -- number of message to get header lines from
         fields -- array of pointers to desired fields

   Returns: allocated string containing matched header lines,
	    NULL on error.
 ----*/
char *
pine_fetch_header(stream, msgno, section, fields, flags)
     MAILSTREAM  *stream;
     long         msgno;
     char	 *section;
     char       **fields;
     long	  flags;
{
    STRINGLIST *sl;
    char       *p, *m, *h = NULL, *match = NULL, *free_this, tmp[MAILTMPLEN];
    char      **pflds = NULL, **pp = NULL, **qq;
    int         cnt = 0;

    /*
     * If the user misconfigures it is possible to have one of the fields
     * set to the empty string instead of a header name. We want to catch
     * that here instead of asking the server the nonsensical question.
     */
    for(pp = fields ? &fields[0] : NULL; pp && *pp; pp++)
      if(!**pp)
	break;

    if(pp && *pp){		/* found an empty header field, fix it */
	pflds = copy_list_array(fields);
	for(pp = pflds; pp && *pp; pp++){
	    if(!**pp){			/* scoot rest of the lines up */
		free_this = *pp;
		for(qq = pp; *qq; qq++)
		  *qq = *(qq+1);

		if(free_this)
		  fs_give((void **) &free_this);
	    }
	}

	/* no headers to look for, return NULL */
	if(pflds && !*pflds && !(flags & FT_NOT)){
	    free_list_array(&pflds);
	    return(NULL);
	}
    }
    else
      pflds = fields;

    sl = (pflds && *pflds) ? new_strlst(pflds) : NULL;  /* package up fields */
    h  = mail_fetch_header(stream, msgno, section, sl, NULL, flags | FT_PEEK);
    if (sl) 
      free_strlst(&sl);

    if(!h){
	if(pflds && pflds != fields)
	  free_list_array(&pflds);
	  
	return(NULL);
    }

    while(find_field(&h, tmp, sizeof(tmp))){
	for(pp = &pflds[0]; *pp && strucmp(tmp, *pp); pp++)
	  ;

	/* interesting field? */
	if(p = (flags & FT_NOT) ? ((*pp) ? NULL : tmp) : *pp){
	    /*
	     * Hold off allocating space for matching fields until
	     * we at least find one to copy...
	     */
	    if(!match)
	      match = m = fs_get(strlen(h) + strlen(p) + 1);

	    while(*p)				/* copy field name */
	      *m++ = *p++;

	    while(*h && (*m++ = *h++))		/* header includes colon */
	      if(*(m-1) == '\n' && (*h == '\r' || !isspace((unsigned char)*h)))
		break;

	    *m = '\0';				/* tie off match string */
	}
	else{					/* no match, pass this field */
	    while(*h && !(*h++ == '\n'
	          && (*h == '\r' || !isspace((unsigned char)*h))))
	      ;
	}
    }

    if(pflds && pflds != fields)
      free_list_array(&pflds);

    return(match ? match : cpystr(""));
}


int
find_field(h, tmp, ntmp)
     char **h;
     char  *tmp;
     size_t ntmp;
{
    char *otmp = tmp;

    if(!h || !*h || !**h || isspace((unsigned char)**h))
      return(0);

    while(tmp-otmp<ntmp-1 && **h && **h != ':' && !isspace((unsigned char)**h))
      *tmp++ = *(*h)++;

    *tmp = '\0';
    return(1);
}


static char *_last_embedded_fg_color, *_last_embedded_bg_color;


void
embed_color(cp, pc)
    COLOR_PAIR *cp;
    gf_io_t     pc;
{
    if(cp && cp->fg){
	if(_last_embedded_fg_color)
	  fs_give((void **)&_last_embedded_fg_color);

	_last_embedded_fg_color = cpystr(cp->fg);

	if(!(pc && (*pc)(TAG_EMBED) && (*pc)(TAG_FGCOLOR) &&
	     gf_puts(color_to_asciirgb(cp->fg), pc)))
	  return;
    }
    
    if(cp && cp->bg){
	if(_last_embedded_bg_color)
	  fs_give((void **)&_last_embedded_bg_color);

	_last_embedded_bg_color = cpystr(cp->bg);

	if(!(pc && (*pc)(TAG_EMBED) && (*pc)(TAG_BGCOLOR) &&
	     gf_puts(color_to_asciirgb(cp->bg), pc)))
	  return;
    }
}


COLOR_PAIR *
get_cur_embedded_color()
{
    COLOR_PAIR *ret;

    if(_last_embedded_fg_color && _last_embedded_bg_color)
      ret = new_color_pair(_last_embedded_fg_color, _last_embedded_bg_color);
    else
      ret = pico_get_cur_color();
    
    return(ret);
}


void
clear_cur_embedded_color()
{
    if(_last_embedded_fg_color)
      fs_give((void **)&_last_embedded_fg_color);

    if(_last_embedded_bg_color)
      fs_give((void **)&_last_embedded_bg_color);
}


STRINGLIST *
new_strlst(l)
    char **l;
{
    STRINGLIST *sl = mail_newstringlist();

    sl->text.data = (unsigned char *) (*l);
    sl->text.size = strlen(*l);
    sl->next = (*++l) ? new_strlst(l) : NULL;
    return(sl);
}


void
free_strlst(sl)
    STRINGLIST **sl;
{
    if(*sl){
	if((*sl)->next)
	  free_strlst(&(*sl)->next);

	fs_give((void **) sl);
    }
}

/*--------------------------------------------------------------------
  Call the function that will perform the double click operation

  Returns: the current top line
--------*/
long
doubleclick_handle(sparms, next_handle, cmd, done)
     SCROLL_S *sparms;
     HANDLE_S *next_handle;
     int *cmd, *done;
{
  if(sparms->mouse.clickclick){
    if((*sparms->mouse.clickclick)(sparms))
      *done = 1;
  }
  else
    switch(scroll_handle_launch(next_handle, TRUE)){
    case 1 :
      *cmd = MC_EXIT;		/* propagate */
      *done = 1;
      break;
      
    case -1 :
      cmd_cancelled("View");
      break;
      
    default :
      break;
    }
  
  return(scroll_state(SS_CUR)->top_text_line);
}

#ifdef	_WINDOWS
/*
 * Just a little something to simplify assignments
 */
#define	VIEWPOPUP(p, c, s)	{ \
				    (p)->type	      = tQueue; \
				    (p)->data.val     = c; \
				    (p)->label.style  = lNormal; \
				    (p)->label.string = s; \
				}


/*
 * 
 */
int
format_message_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup	  fmp_menu[32];
    HANDLE_S	 *h = NULL;
    int		  i = -1, n;
    long          rawno;
    MESSAGECACHE *mc;

    /* Reason to offer per message ops? */
    if(mn_get_total(ps_global->msgmap) > 0L){
	if(in_handle){
	    SCRLCTRL_S *st = scroll_state(SS_CUR);

	    switch((h = get_handle(st->parms->text.handles, in_handle))->type){
	      case Attach :
		fmp_menu[++i].type	 = tIndex;
		fmp_menu[i].label.string = "View Attachment";
		fmp_menu[i].label.style  = lNormal;
		fmp_menu[i].data.val     = 'X'; /* for local use */

		if(h->h.attach
		   && dispatch_attachment(h->h.attach) != MCD_NONE
		   && !(h->h.attach->can_display & MCD_EXTERNAL)
		   && h->h.attach->body
		   && (h->h.attach->body->type == TYPETEXT
		       || (h->h.attach->body->type == TYPEMESSAGE
			   && h->h.attach->body->subtype
			   && !strucmp(h->h.attach->body->subtype,"rfc822")))){
		    fmp_menu[++i].type	     = tIndex;
		    fmp_menu[i].label.string = "View Attachment in New Window";
		    fmp_menu[i].label.style  = lNormal;
		    fmp_menu[i].data.val     = 'Y';	/* for local use */
		}

		fmp_menu[++i].type	    = tIndex;
		fmp_menu[i].label.style = lNormal;
		fmp_menu[i].data.val    = 'Z';	/* for local use */
		msgno_exceptions(ps_global->mail_stream,
				 mn_m2raw(ps_global->msgmap,
					  mn_get_cur(ps_global->msgmap)),
				 h->h.attach->number, &n, FALSE);
		fmp_menu[i].label.string = (n & MSG_EX_DELETE)
					      ? "Undelete Attachment"
					      : "Delete Attachment";
		break;

	      case URL :
	      default :
		fmp_menu[++i].type	 = tIndex;
		fmp_menu[i].label.string = "View Link";
		fmp_menu[i].label.style  = lNormal;
		fmp_menu[i].data.val     = 'X';	/* for local use */

		fmp_menu[++i].type	 = tIndex;
		fmp_menu[i].label.string = "Copy Link";
		fmp_menu[i].label.style  = lNormal;
		fmp_menu[i].data.val     = 'W';	/* for local use */
		break;
	    }

	    fmp_menu[++i].type = tSeparator;
	}

	/* Delete or Undelete?  That is the question. */
	fmp_menu[++i].type	= tQueue;
	fmp_menu[i].label.style = lNormal;
	mc = ((rawno = mn_m2raw(ps_global->msgmap,
				mn_get_cur(ps_global->msgmap))) > 0L
	      && ps_global->mail_stream
	      && rawno <= ps_global->mail_stream->nmsgs)
	      ? mail_elt(ps_global->mail_stream, rawno) : NULL;
	if(mc && mc->deleted){
	    fmp_menu[i].data.val     = 'U';
	    fmp_menu[i].label.string = in_handle
					 ? "&Undelete Message" : "&Undelete";
	}
	else{
	    fmp_menu[i].data.val     = 'D';
	    fmp_menu[i].label.string = in_handle
					 ? "&Delete Message" : "&Delete";
	}

	if(F_ON(F_ENABLE_FLAG, ps_global)){
	    fmp_menu[++i].type	     = tSubMenu;
	    fmp_menu[i].label.string = "Flag";
	    fmp_menu[i].data.submenu = flag_submenu(mc);
	}

	i++;
	VIEWPOPUP(&fmp_menu[i], 'S', in_handle ? "&Save Message" : "&Save");

	i++;
	VIEWPOPUP(&fmp_menu[i], '%', in_handle ? "Print Message" : "Print");

	i++;
	VIEWPOPUP(&fmp_menu[i], 'R',
		  in_handle ? "&Reply to Message" : "&Reply");

	i++;
	VIEWPOPUP(&fmp_menu[i], 'F',
		  in_handle ? "&Forward Message" : "&Forward");

	fmp_menu[++i].type = tSeparator;

	if(mn_get_cur(ps_global->msgmap) < mn_get_total(ps_global->msgmap)){
	    i++;
	    VIEWPOPUP(&fmp_menu[i], 'N', "View &Next Message");

	    fmp_menu[++i].type = tSeparator;
	}

	/* Offer the attachment screen? */
	for(n = 0; ps_global->atmts && ps_global->atmts[n].description; n++)
	  ;

	if(n > 1){
	    i++;
	    VIEWPOPUP(&fmp_menu[i], 'V', "&View Attachment Index");
	}
    }

    i++;
    VIEWPOPUP(&fmp_menu[i], 'I', "Message &Index");

    i++;
    VIEWPOPUP(&fmp_menu[i], 'M', "&Main Menu");

    fmp_menu[++i].type = tTail;

    if((i = mswin_popup(fmp_menu)) >= 0 && in_handle)
      switch(fmp_menu[i].data.val){
	case 'W' :		/* Copy URL to clipboard */
	  mswin_addclipboard(h->h.url.path);
	  break;

	case 'X' :
	  return(1);		/* return like the user double-clicked */

	break;

	case 'Y' :		/* popup the thing in another window */
	  display_att_window(h->h.attach);
	  break;

	case 'Z' :
	  if(h && h->type == Attach){
	      msgno_exceptions(ps_global->mail_stream,
			       mn_m2raw(ps_global->msgmap,
					mn_get_cur(ps_global->msgmap)),
			       h->h.attach->number, &n, FALSE);
	      n ^= MSG_EX_DELETE;
	      msgno_exceptions(ps_global->mail_stream,
			       mn_m2raw(ps_global->msgmap,
					mn_get_cur(ps_global->msgmap)),
			       h->h.attach->number, &n, TRUE);
	      q_status_message2(SM_ORDER, 0, 3, "Attachment %.200s %.200s!",
				h->h.attach->number,
				(n & MSG_EX_DELETE) ? "deleted" : "undeleted");

	      return(2);
	  }

	  break;

	default :
	  break;
      }

    return(0);
}



/*
 * 
 */
int
simple_text_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup    simple_menu[12];
    int	      n = 0;

    VIEWPOPUP(&simple_menu[n], '%', "Print");
    n++;

    VIEWPOPUP(&simple_menu[n], 'S', "Save");
    n++;

    VIEWPOPUP(&simple_menu[n], 'F', "Forward in Email");
    n++;

    simple_menu[n++].type = tSeparator;

    VIEWPOPUP(&simple_menu[n], 'E', "Exit Viewer");
    n++;

    simple_menu[n].type = tTail;

    (void) mswin_popup(simple_menu);
    return(0);
}



/*----------------------------------------------------------------------
    Return characters in scroll tool buffer serially

   Args: n -- index of char to return

   Returns: returns the character at index 'n', or -1 on error or
	    end of buffer.

 ----*/
int
mswin_readscrollbuf(n)
    int n;
{
    SCRLCTRL_S	 *st = scroll_state(SS_CUR);
    int		  c;
    static char **orig = NULL, **l, *p;
    static int    lastn;

    if(!st)
      return(-1);

    /*
     * All of these are mind-numbingly slow at the moment...
     */
    switch(st->parms->text.src){
      case CharStar :
	return((n >= strlen((char *)st->parms->text.text))
	         ? -1 : ((char *)st->parms->text.text)[n]);

      case CharStarStar :
	/* BUG? is this test rigorous enough? */
	if(orig != (char **)st->parms->text.text || n < lastn){
	    lastn = n;
	    if(orig = l = (char **)st->parms->text.text) /* reset l and p */
	      p = *l;
	}
	else{				/* use cached l and p */
	    c = n;			/* and adjust n */
	    n -= lastn;
	    lastn = c;
	}

	while(l){			/* look for 'n' on each line  */
	    for(; n && *p; n--, p++)
	      ;

	    if(n--)			/* 'n' found ? */
	      p = *++l;
	    else
	      break;
	}

	return((l && *l) ? *p ? *p : '\n' : -1);

      case FileStar :
	return((fseek((FILE *)st->parms->text.text, (long) n, 0) < 0
		|| (c = fgetc((FILE *)st->parms->text.text)) == EOF) ? -1 : c);

      default:
	return(-1);
    }
}



/*----------------------------------------------------------------------
     MSWin scroll callback.  Called during scroll message processing.
	     


  Args: cmd - what type of scroll operation.
	scroll_pos - paramter for operation.  
			used as position for SCROLL_TO operation.

  Returns: TRUE - did the scroll operation.
	   FALSE - was not able to do the scroll operation.
 ----*/
int
pcpine_do_scroll (cmd, scroll_pos)
int	cmd;
long	scroll_pos;
{
    SCRLCTRL_S   *st = scroll_state(SS_CUR);
    HANDLE_S	 *next_handle;
    int		  paint = FALSE;
    int		  num_display_lines;
    int		  scroll_lines;
    int		  num_text_lines;
    char	  message[64];
    long	  maxscroll;
    
	
    message[0] = '\0';
    maxscroll = st->num_lines;
    switch (cmd) {
    case MSWIN_KEY_SCROLLUPLINE:
	if(st->top_text_line > 0) {
	    st->top_text_line -= (int) scroll_pos;
	    paint = TRUE;
	    if (st->top_text_line <= 0){
		sprintf(message, "START of %.*s",
			32, STYLE_NAME(st->parms));
		st->top_text_line = 0;
	    }
        }
	break;

    case MSWIN_KEY_SCROLLDOWNLINE:
        if(st->top_text_line < maxscroll) {
	    st->top_text_line += (int) scroll_pos;
	    paint = TRUE;
	    if (st->top_text_line >= maxscroll){
		sprintf(message, "END of %.*s", 32, STYLE_NAME(st->parms));
		st->top_text_line = maxscroll;
	    }
        }
	break;
		
    case MSWIN_KEY_SCROLLUPPAGE:
	if(st->top_text_line > 0) {
	    num_display_lines = SCROLL_LINES(ps_global);
	    scroll_lines = min(max(num_display_lines -
			ps_global->viewer_overlap, 1), num_display_lines);
	    if (st->top_text_line > scroll_lines)
		st->top_text_line -= scroll_lines;
	    else {
		st->top_text_line = 0;
		sprintf(message, "START of %.*s", 32, STYLE_NAME(st->parms));
	    }
	    paint = TRUE;
        }
	break;
	    
    case MSWIN_KEY_SCROLLDOWNPAGE:
	num_display_lines = SCROLL_LINES(ps_global);
	if(st->top_text_line  < maxscroll) {
	    scroll_lines = min(max(num_display_lines -
			ps_global->viewer_overlap, 1), num_display_lines);
	    st->top_text_line += scroll_lines;
	    if (st->top_text_line >= maxscroll) {
		st->top_text_line = maxscroll;
		sprintf(message, "END of %.*s", 32, STYLE_NAME(st->parms));
	    }
	    paint = TRUE;
	}
	break;
	    
    case MSWIN_KEY_SCROLLTO:
	if (st->top_text_line != scroll_pos) {
	    st->top_text_line = scroll_pos;
	    if (st->top_text_line == 0)
		sprintf(message, "START of %.*s", 32, STYLE_NAME(st->parms));
	    else if(st->top_text_line >= maxscroll) 
		sprintf(message, "END of %.*s", 32, STYLE_NAME(st->parms));
	    paint = TRUE;
        }
	break;
    }

    /* Need to frame some handles? */
    if(st->parms->text.handles
       && (next_handle = scroll_handle_in_frame(st->top_text_line)))
      st->parms->text.handles = next_handle;

    if (paint) {
	mswin_beginupdate();
	update_scroll_titlebar(st->top_text_line, 0);
	(void) scroll_scroll_text(st->top_text_line,
				  st->parms->text.handles, 1);
	if (message[0])
	  q_status_message(SM_INFO, 0, 1, message);

	/* Display is always called so that the "START(END) of message" 
	 * message gets removed when no longer at the start(end). */
	display_message (KEY_PGDN);
	mswin_endupdate();
    }

    return (TRUE);
}


char *
pcpine_help_scroll(title)
    char *title;
{
    SCRLCTRL_S   *st = scroll_state(SS_CUR);

    if(title)
      strcpy(title, (st->parms->help.title)
		      ? st->parms->help.title : "PC-Pine Help");

    return(pcpine_help(st->parms->help.text));
}


int
pcpine_resize_scroll()
{
    int newRows, newCols;

    mswin_getscreensize(&newRows, &newCols);
    if(newCols == ps_global->ttyo->screen_cols){
	SCRLCTRL_S   *st = scroll_state(SS_CUR);

	(void) get_windsize (ps_global->ttyo);
	mswin_beginupdate();
	update_scroll_titlebar(st->top_text_line, 1);
	ClearLine(1);
	(void) scroll_scroll_text(st->top_text_line,
				  st->parms->text.handles, 1);
	mswin_endupdate();
    }

    return(0);
}


int
pcpine_view_cursor(col, row)
    int  col;
    long row;
{
    SCRLCTRL_S   *st = scroll_state(SS_CUR);
    int		  key;
    long	  line;

    return((row >= HEADER_ROWS(ps_global)
	    && row < HEADER_ROWS(ps_global) + SCROLL_LINES(ps_global)
	    && (line = (row - 2) + st->top_text_line) < st->num_lines
	    && (key = dot_on_handle(line, col))
	    && scroll_handle_selectable(get_handle(st->parms->text.handles,key)))
	     ? MSWIN_CURSOR_HAND
	     : MSWIN_CURSOR_ARROW);
}
#endif /* _WINDOWS */
