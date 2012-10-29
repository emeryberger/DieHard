#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailpart.c 14047 2005-04-27 18:53:45Z hubert $";
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
     mailpart.c
     The meat and pototoes of attachment processing here.

  ====*/

#include "headers.h"


/*
 * Information used to paint and maintain a line on the attachment
 * screen.
 */
typedef struct atdisp_line {
    char	     *dstring;			/* alloc'd var value string  */
    ATTACH_S	     *attp;			/* actual attachment pointer */
    struct atdisp_line *next, *prev;
} ATDISP_S;


/*
 * struct defining attachment screen's current state
 */
typedef struct att_screen {
    ATDISP_S   *current,
	       *top_line;
    COLOR_PAIR *titlecolor;
} ATT_SCREEN_S;
static ATT_SCREEN_S *att_screen;


#define	next_attline(p)	((p) ? (p)->next : NULL)
#define	prev_attline(p)	((p) ? (p)->prev : NULL)


/*
 * We need to defined simple functions here for the piping and 
 * temporary storage object below.  We can use the filter.c functions
 * because they're already in use for the "putchar" function passed to
 * detach.
 */
static STORE_S *detach_so = NULL;


#ifdef	_WINDOWS
/*
 * local global pointer to scroll attachment popup menu
 */
static ATTACH_S *scrat_attachp;
#endif


/*
 * The display filter is locally global because it's set in df_trigger_cmp
 * which sniffs at lines of the unencoded segment...
 */
typedef struct _trigger {
    int		   (*cmp) PROTO((char *, char *));
    char	    *text;
    char	    *cmd;
    struct _trigger *next;
} TRGR_S;

static char	*display_filter;
static TRGR_S	*df_trigger_list;

/*
 * Data used to keep track of partial fetches...
 */
typedef struct _fetch_read {
    unsigned	   free_me:1;
    MAILSTREAM	  *stream;		/* stream of open mailbox */
    unsigned long  msgno;		/* message number within mailbox */
    char	  *section,		/* MIME section within message */
		  *chunk,		/* block of partial fetched data */
		  *chunkp,		/* pointer to next char in block */
		  *endp,		/* cell past last char in block */
		  *error;		/* Error message to report */
    unsigned long  read,		/* bytes read so far */
		   size,		/* total bytes to read */
		   chunksize,		/* size of chunk block */
		   allocsize;		/* allocated size of chunk block */
    long	   flags,		/* flags to use fetching block */
		   fetchtime;		/* usecs avg per chunk fetch */
    gf_io_t	   readc;
    STORE_S	  *cache;
} FETCH_READC_S;

static FETCH_READC_S *g_fr_desc;
static FETCH_READC_S *g_pft_desc;

/*
 * BUG: Might want to be more adaptive based on connect speed
 *	or somesuch
 */
#define	INIT_FETCH_CHUNK	((unsigned long)(8 * 1024L))
#define	MIN_FETCH_CHUNK		((unsigned long)(4 * 1024L))
#define	MAX_FETCH_CHUNK		((unsigned long)(256 * 1024L))
#define	AVOID_MICROSOFT_SSL_CHUNKING_BUG ((unsigned long)(12 * 1024L))
#define	TARGET_INTR_TIME	((unsigned long)2000000L) /* two seconds */
#define	FETCH_READC	g_fr_desc->readc


static struct key att_index_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"<",NULL,{MC_EXIT,2,{'<',','}},KS_EXITMODE},
	{">","[View]",{MC_VIEW_ATCH,5,{'v','>','.',ctrl('M'),ctrl('J')}},
	  KS_VIEW},
	{"P", "PrevAttch",{MC_PREVITEM,4,{'p',ctrl('B'),ctrl('P'),KEY_UP}},
	  KS_PREVMSG},
	{"N", "NextAtch",
	 {MC_NEXTITEM, 5, {'n','\t',ctrl('F'),ctrl('N'), KEY_DOWN}},
	 KS_NEXTMSG},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	{"S", "Save", {MC_SAVETEXT,1,{'s'}}, KS_SAVE},
	{NULL, NULL, {MC_EXPORT, 1, {'e'}}, KS_EXPORT},

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
	PIPE_MENU,
	BOUNCE_MENU,
	{"A","AboutAttch",{MC_ABOUTATCH,1,{'a'}},KS_NONE},
	WHEREIS_MENU,
	{"%", "Print", MC_PRINTMSG,1,{'%'}, KS_PRINT},
	INDEX_MENU,
	REPLY_MENU,
	FORWARD_MENU};
INST_KEY_MENU(att_index_keymenu, att_index_keys);
#define	ATT_PARENT_KEY	 2
#define	ATT_EXPORT_KEY	11
#define	ATT_PIPE_KEY	16
#define	ATT_BOUNCE_KEY	17
#define	ATT_PRINT_KEY	20
#define	ATT_REPLY_KEY	22
#define	ATT_FORWARD_KEY	23


static struct key att_view_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"<",NULL,{MC_EXIT,2,{'<',','}},KS_EXITMODE},
	{"Ret","[View Hilite]",{MC_VIEW_HANDLE,3,
				{ctrl('m'),ctrl('j'),KEY_RIGHT}},KS_NONE},
	{"^B","Prev URL",{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F","Next URL",{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	SAVE_MENU,
	EXPORT_MENU,

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
	PIPE_MENU,
	BOUNCE_MENU,
	NULL_MENU,
	WHEREIS_MENU,
	{"%", "Print", MC_PRINTMSG,1,{'%'}, KS_PRINT},
	NULL_MENU,
	REPLY_MENU,
	FORWARD_MENU};
INST_KEY_MENU(att_view_keymenu, att_view_keys);
#define	ATV_BACK_KEY	 2
#define	ATV_VIEW_HILITE	 3
#define	ATV_PREV_URL	 4
#define	ATV_NEXT_URL	 5
#define	ATV_EXPORT_KEY	11
#define	ATV_PIPE_KEY	16
#define	ATV_BOUNCE_KEY	17
#define	ATV_PRINT_KEY	20
#define	ATV_REPLY_KEY	22
#define	ATV_FORWARD_KEY	23


/* used to keep track of detached MIME segments total length */
static long	save_att_length;

/*
 * Internal Prototypes
 */
void	    save_attachment PROTO((int, long, ATTACH_S *));
void	    export_attachment PROTO((int, long, ATTACH_S *));
void	    print_attachment PROTO((int, long, ATTACH_S *));
void	    pipe_attachment PROTO((long, ATTACH_S *));
int	    delete_attachment PROTO((long, ATTACH_S *));
int	    undelete_attachment PROTO((long, ATTACH_S *, int *));
void	    save_msg_att PROTO((long, ATTACH_S *));
void	    save_digest_att PROTO((long, ATTACH_S *));
void	    export_msg_att PROTO((long, ATTACH_S *));
void	    export_digest_att PROTO((long, ATTACH_S *));
void	    write_attachment PROTO((int, long, ATTACH_S *, char *));
char	   *write_attached_msg PROTO((long, ATTACH_S **, STORE_S *, int));
int	    print_msg_att PROTO((long, ATTACH_S *, int));
void	    print_digest_att PROTO((long, ATTACH_S *));
STORE_S	   *format_text_att PROTO((long, ATTACH_S *, HANDLE_S **));
void	    display_text_att PROTO((long, ATTACH_S *, int));
void	    display_msg_att PROTO((long, ATTACH_S *, int));
void	    display_digest_att PROTO((long, ATTACH_S *, int));
void	    forward_attachment PROTO((MAILSTREAM *, long, ATTACH_S *));
void	    forward_msg_att PROTO((MAILSTREAM *, long, ATTACH_S *));
void	    reply_msg_att PROTO((MAILSTREAM *, long, ATTACH_S *));
void	    bounce_msg_att PROTO((MAILSTREAM *, long, char *, char *));
int	    scroll_attachment PROTO((char *, STORE_S *, SourceType,
				     HANDLE_S *, ATTACH_S *, int));
int	    process_attachment_cmd PROTO((int, MSGNO_S *, SCROLL_S *));
int	    format_msg_att PROTO((long, ATTACH_S **, gf_io_t, int));
void	    display_vcard_att PROTO((long, ATTACH_S *, int));
void	    display_attach_info PROTO((long, ATTACH_S *));
void        update_att_screen_titlebar PROTO(());
void	    run_viewer PROTO((char *, BODY *, int));
void	    fetch_readc_init PROTO((FETCH_READC_S *, MAILSTREAM *, long,
				    char *, unsigned long, long));
int	    fetch_readc_cleanup PROTO(());
char	   *fetch_gets PROTO((readfn_t, void *, unsigned long, GETS_DATA *));
char	   *partial_text_gets PROTO((readfn_t, void *,
				     unsigned long, GETS_DATA *));
int	    fetch_readc PROTO((unsigned char *));
ATDISP_S   *new_attline PROTO((ATDISP_S **));
void	    free_attline PROTO((ATDISP_S **));
ATDISP_S   *first_attline PROTO((ATDISP_S *));
void	    attachment_screen_redrawer PROTO(());
int	    attachment_screen_updater PROTO((struct pine *, ATDISP_S *, \
					     ATT_SCREEN_S *));
int	    init_att_progress PROTO((char *, MAILSTREAM *, BODY *));
long	    save_att_piped PROTO((int));
int	    save_att_percent PROTO(());
int	    detach_writec PROTO((int));
int	    df_trigger_cmp PROTO((long, char *, LT_INS_S **, void *));
int	    df_trigger_cmp_text PROTO((char *, char *));
int	    df_trigger_cmp_lwsp PROTO((char *, char *));
int	    df_trigger_cmp_start PROTO((char *, char *));
TRGR_S	   *build_trigger_list PROTO(());
void	    blast_trigger_list PROTO((TRGR_S **));
#ifdef	_WINDOWS
int	    scroll_att_popup PROTO((SCROLL_S *, int));
void	    display_text_att_window PROTO((ATTACH_S *));
void	    display_msg_att_window PROTO((ATTACH_S *));
void	    display_att_window PROTO((ATTACH_S *));
#endif


#if defined(DOS) || defined(OS2)
#define	READ_MODE	"rb"
#define	WRITE_MODE	"wb"
#else
#define	READ_MODE	"r"
#define	WRITE_MODE	"w"
#endif

#if	defined(DOS) && !defined(WIN32)
#define	SAVE_TMP_TYPE		TmpFileStar
#else
#define	SAVE_TMP_TYPE		CharStar
#endif



/*----------------------------------------------------------------------
   Provide attachments in browser for selected action

  Args: ps -- pointer to pine structure
	msgmap -- struct containing current message to display

  Result: 

  Handle painting and navigation of attachment index.  It would be nice
  to someday handle message/rfc822 segments in a neat way (i.e., enable
  forwarding, take address, etc.).
 ----*/
void
attachment_screen(ps)
    struct pine *ps;
{
    int		  i, ch = 'x', cmd, dline,
		  nl = 0, sl = 0, old_cols = -1, km_popped = 0, expbits,
		  last_type = TYPEOTHER;
    long	  msgno;
    char	 *p, *q, *last_subtype = NULL, backtag[64];
    OtherMenu     what;
    ATTACH_S	 *atmp;
    ATDISP_S	 *current = NULL, *ctmp = NULL;
    ATT_SCREEN_S  screen;

    ps->prev_screen = attachment_screen;
    ps->next_screen = SCREEN_FUN_NULL;

    if(ps->ttyo->screen_rows - HEADER_ROWS(ps) - FOOTER_ROWS(ps) < 1){
	q_status_message(SM_ORDER | SM_DING, 0, 3,
			 "Screen too small to view attachment");
	ps->next_screen = mail_view_screen;
	return;
    }

    if(mn_total_cur(ps->msgmap) > 1L){
	q_status_message(SM_ORDER | SM_DING, 0, 3,
			 "Can only view one message's attachments at a time.");
	return;
    }
    else if(ps->atmts && ps->atmts->description && !(ps->atmts + 1)->description)
      q_status_message1(SM_ASYNC, 0, 3,
    "Message %.200s has only one part (the message body), and no attachments.",
	long2string(mn_get_cur(ps->msgmap)));

    /*
     * find the longest number and size strings
     */
    for(atmp = ps->atmts; atmp && atmp->description; atmp++){
	if((i = strlen(atmp->number)) > nl)
	  nl = i;

	if((i = strlen(atmp->size)) > sl)
	  sl = i;
    }

    /* Only give up a third off the display to part numbers */
    nl = min(nl, (ps->ttyo->screen_cols/3));

    /*
     * Then, allocate and initialize attachment line list...
     */
    for(atmp = ps->atmts; atmp && atmp->description; atmp++)
      new_attline(&current)->attp = atmp;

    memset(&screen, 0, sizeof(screen));
    msgno	       = mn_m2raw(ps->msgmap, mn_get_cur(ps->msgmap));
    ps->mangled_screen = 1;			/* build display */
    ps->redrawer       = attachment_screen_redrawer;
    att_screen	       = &screen;
    current	       = first_attline(current);
    what	       = FirstMenu;

    /*
     * Advance to next attachment if it's likely that we've already
     * shown it to the user...
     */

    if (current == NULL){
      q_status_message1(SM_ORDER | SM_DING, 0, 3,      
                       "Malformed message: %.200s",
			ps->c_client_error ? ps->c_client_error : "?");
      ps->next_screen = mail_view_screen;
    }
    else if(current->attp->shown && (ctmp = next_attline(current)))
      current = ctmp;

    while(ps->next_screen == SCREEN_FUN_NULL){
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(ps);
		ps->mangled_body = 1;
	    }
	}

	if(ps->mangled_screen){
	    /*
	     * build/rebuild display lines
	     */
	    if(old_cols != ps->ttyo->screen_cols){
		old_cols = ps->ttyo->screen_cols;
		for(ctmp = first_attline(current);
		    ctmp && (atmp = ctmp->attp) && atmp->description;
		    ctmp = next_attline(ctmp)){
		    size_t len, dlen;

		    if(ctmp->dstring)
		      fs_give((void **)&ctmp->dstring);

		    len = max(80, ps->ttyo->screen_cols) * sizeof(char);
		    ctmp->dstring = (char *)fs_get(len + 1);
		    ctmp->dstring[len] = '\0';
		    memset(ctmp->dstring, ' ', len);

		    if(msgno_part_deleted(ps->mail_stream,msgno,atmp->number))
		      ctmp->dstring[1] = 'D';

		    p = ctmp->dstring + 3;
		    if((dlen = strlen(atmp->number)) > nl){
			sstrcpy(&p, "...");
			if((i = dlen - nl) < 0)
			  i = -i;		/* flip sign */

			dlen -= i;
		    }
		    else
		      i = 0;

		    strncpy(p, &atmp->number[i], dlen);

		    /* add 3 spaces, plus pad number, and right justify size */
		    p += 3 + nl + (sl - strlen(atmp->size));

		    for(i = 0; atmp->size[i]; i++)
		      *p++ = atmp->size[i];

		    p += 3;

		    /* copy the type description */
		    q = type_desc(atmp->body->type, atmp->body->subtype,
				  atmp->body->parameter, 1);
		    if((dlen = strlen(q)) > (i = len - (p - ctmp->dstring)))
		      dlen = i;

		    strncpy(p, q, dlen);
		    p += dlen;

		    /* provided there's room, copy the user's description */
		    if(len - (p - ctmp->dstring) > 8
		       && ((q = atmp->body->description)
			   || (MIME_MSG_A(atmp)
			       && atmp->body->nested.msg->env
			       && (q=atmp->body->nested.msg->env->subject)))){
			char buftmp[MAILTMPLEN];

			sstrcpy(&p, ", \"");
			sprintf(buftmp, "%.200s", q);
			q = (char *) rfc1522_decode(
						(unsigned char *)tmp_20k_buf,
						SIZEOF_20KBUF, buftmp, NULL);

			if((dlen=strlen(q)) > (i=len - (p - ctmp->dstring)))
			  dlen = i;

			istrncpy(p, q, dlen);
			*(p += dlen) = '\"';
		    }
		}
	    }

	    ps->mangled_header = 1;
	    ps->mangled_footer = 1;
	    ps->mangled_body   = 1;
	}

	/*----------- Check for new mail -----------*/
        if(new_mail(0, NM_TIMING(ch), NM_STATUS_MSG | NM_DEFER_SORT) >= 0)
          ps->mangled_header = 1;

	/*
	 * If an expunge of the current message happened during the
	 * new mail check we want to bail out of here. See mm_expunged.
	 */
	if(ps->next_screen != SCREEN_FUN_NULL)
	  break;

	if(ps->mangled_header){
	    update_att_screen_titlebar();
	    ps->mangled_header = 0;
	}

	if(ps->mangled_screen){
	    ClearLine(1);
	    ps->mangled_screen = 0;
	}

	dline = attachment_screen_updater(ps, current, &screen);

	/*---- This displays new mail notification, or errors ---*/
	if(km_popped){
	    FOOTER_ROWS(ps) = 3;
	    mark_status_unknown();
	}

        display_message(ch);
	if(km_popped){
	    FOOTER_ROWS(ps) = 1;
	    mark_status_unknown();
	}

	if(ps->mangled_footer
	   || current->attp->body->type != last_type
	   || !(last_subtype && current->attp->body->subtype)
	   || strucmp(last_subtype, current->attp->body->subtype)){

	    struct key_menu *km = &att_index_keymenu;
	    bitmap_t	     bitmap;

	    setbitmap(bitmap);
	    ps->mangled_footer = 0;
	    last_type	       = current->attp->body->type;
	    last_subtype       = current->attp->body->subtype;

	    sprintf(backtag, "Msg #%ld", mn_get_cur(ps->msgmap));
	    km->keys[ATT_PARENT_KEY].label = backtag;

	    if(F_OFF(F_ENABLE_PIPE, ps))
	      clrbitn(ATT_PIPE_KEY, bitmap);

	    /*
	     * If message or digest, leave Reply and Save and,
	     * conditionally, Bounce on...
	     */
	    if(MIME_MSG(last_type, last_subtype)
	       || MIME_DGST(last_type, last_subtype)){
		if(F_OFF(F_ENABLE_BOUNCE, ps))
		  clrbitn(ATT_BOUNCE_KEY, bitmap);

		km->keys[ATT_EXPORT_KEY].name  = "";
		km->keys[ATT_EXPORT_KEY].label = "";
	    }
	    else{
		clrbitn(ATT_BOUNCE_KEY, bitmap);
		clrbitn(ATT_REPLY_KEY, bitmap);

		if(last_type != TYPETEXT)
		  clrbitn(ATT_PRINT_KEY, bitmap);

		km->keys[ATT_EXPORT_KEY].name  = "E";
		km->keys[ATT_EXPORT_KEY].label = "Export";
	    }

	    if(km_popped){
		FOOTER_ROWS(ps) = 3;
		clearfooter(ps);
	    }

	    if(F_ON(F_ARROW_NAV, ps)){
		menu_add_binding(km, KEY_LEFT, MC_EXIT);
		menu_add_binding(km, KEY_RIGHT, MC_VIEW_ATCH);
	    }
	    else{
		menu_clear_binding(km, KEY_LEFT);
		menu_clear_binding(km, KEY_RIGHT);
	    }

	    draw_keymenu(km, bitmap, ps->ttyo->screen_cols, 1-FOOTER_ROWS(ps),
			 0, what);
	    what = SameMenu;
	    if(km_popped){
		FOOTER_ROWS(ps) = 1;
		mark_keymenu_dirty();
	    }
	}

	if(F_ON(F_SHOW_CURSOR, ps))
	  MoveCursor(dline, 0);
	else
	  MoveCursor(max(0, ps->ttyo->screen_rows - FOOTER_ROWS(ps)), 0);

        /*------ Prepare to read the command from the keyboard ----*/
#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0); /* prime the handler */
	register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
		       ps_global->ttyo->screen_rows -(FOOTER_ROWS(ps_global)+1),
		       ps_global->ttyo->screen_cols);
#endif
	ch = read_command();
#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif

	cmd = menu_command(ch, &att_index_keymenu);

	if(km_popped)
	  switch(cmd){
	    case MC_NONE :
	    case MC_OTHER :
	    case MC_RESIZE :
	    case MC_REPAINT :
	      km_popped++;
	      break;
	    
	    default:
	      clearfooter(ps);
	      break;
	  }

	switch(cmd){
	  case MC_HELP :
	    if(FOOTER_ROWS(ps) == 1 && km_popped == 0){
		km_popped = 2;
		ps->mangled_footer = 1;
		break;
	    }

	    helper(h_attachment_screen, "HELP FOR ATTACHMENT INDEX", 0);
	    ps->mangled_screen = 1;
	    break;

	  case MC_OTHER :
	    what = NextMenu;
	    ps->mangled_footer = 1;
	    break;

	  case MC_EXIT :			/* exit attachment screen */
	    ps->next_screen = mail_view_screen;
	    break;

	  case MC_QUIT :
	    ps->next_screen = quit_screen;
	    break;

	  case MC_MAIN :
	    ps->next_screen = main_menu_screen;
	    break;

	  case MC_INDEX :
	    ps->next_screen = mail_index_screen;
	    break;

	  case MC_NEXTITEM :
	    if(ctmp = next_attline(current))
	      current = ctmp;
	    else
	      q_status_message(SM_ORDER, 0, 1, "Already on last attachment");

	    break;

	  case MC_PREVITEM :
	    if(ctmp = prev_attline(current))
	      current = ctmp;
	    else
	      q_status_message(SM_ORDER, 0, 1, "Already on first attachment");

	    break;

	  case MC_PAGEDN :				/* page forward */
	    if(next_attline(current)){
		while(dline++ < ps->ttyo->screen_rows - FOOTER_ROWS(ps))
		  if(ctmp = next_attline(current))
		    current = ctmp;
		  else
		    break;
	    }
	    else
	      q_status_message(SM_ORDER, 0, 1,
			       "Already on last page of attachments");
	    

	    break;

	  case MC_PAGEUP :			/* page backward */
	    if(prev_attline(current)){
		while(dline-- > HEADER_ROWS(ps))
		  if(ctmp = prev_attline(current))
		    current = ctmp;
		  else
		    break;

		while(++dline < ps->ttyo->screen_rows - FOOTER_ROWS(ps))
		  if(ctmp = prev_attline(current))
		    current = ctmp;
		  else
		    break;
	    }
	    else
	      q_status_message(SM_ORDER, 0, 1,
			       "Already on first page of attachments");

	    break;

#ifdef MOUSE	    
	  case MC_MOUSE:
	    {
		MOUSEPRESS mp;

		mouse_get_last (NULL, &mp);
		mp.row -= HEADER_ROWS(ps);
		ctmp = screen.top_line;
		while (mp.row && ctmp != NULL) {
		    --mp.row;
		    ctmp = ctmp->next;
		}

		if (ctmp != NULL){
		    current = ctmp;

		    if (mp.doubleclick){
			if(mp.button == M_BUTTON_LEFT)
			  display_attachment(msgno, current->attp, DA_SAVE);
		    }
#ifdef	_WINDOWS
		    else if(mp.button == M_BUTTON_RIGHT){
			MPopup atmt_popup[20];
			int    i = -1, exoffer = 0;

			dline = attachment_screen_updater(ps,current,&screen);

			if(dispatch_attachment(current->attp) != MCD_NONE){
			    atmt_popup[++i].type   = tQueue;
			    atmt_popup[i].data.val   = 'V';
			    atmt_popup[i].label.style    = lNormal;
			    atmt_popup[i].label.string    = "&View";

			    if(!(current->attp->can_display & MCD_EXTERNAL)
				&& (current->attp->body->type == TYPETEXT
				    || MIME_MSG_A(current->attp)
				    || MIME_DGST_A(current->attp))){
				exoffer++;
				atmt_popup[++i].type	   = tIndex;
				atmt_popup[i].label.style  = lNormal;
				atmt_popup[i].label.string =
							  "View in New Window";
			    }
			}

			atmt_popup[++i].type	   = tQueue;
			atmt_popup[i].data.val	   = 'S';
			atmt_popup[i].label.style  = lNormal;
			atmt_popup[i].label.string = "&Save";

			atmt_popup[++i].type	  = tQueue;
			atmt_popup[i].label.style = lNormal;
			if(current->dstring[1] == 'D'){
			    atmt_popup[i].data.val     = 'U';
			    atmt_popup[i].label.string = "&Undelete";
			}
			else{
			    atmt_popup[i].data.val     = 'D';
			    atmt_popup[i].label.string = "&Delete";
			}

			if(MIME_MSG_A(current->attp)){
			    atmt_popup[++i].type       = tQueue;
			    atmt_popup[i].label.style  = lNormal;
			    atmt_popup[i].label.string = "&Reply...";
			    atmt_popup[i].data.val     = 'R';

			    atmt_popup[++i].type       = tQueue;
			    atmt_popup[i].label.style  = lNormal;
			    atmt_popup[i].label.string = "&Forward...";
			    atmt_popup[i].data.val   = 'F';
			}

			atmt_popup[++i].type	   = tQueue;
			atmt_popup[i].label.style  = lNormal;
			atmt_popup[i].label.string = "&About...";
			atmt_popup[i].data.val	   = 'A';

			atmt_popup[++i].type = tSeparator;

			atmt_popup[++i].type	   = tQueue;
			sprintf(backtag, "View Message #%ld",
				mn_get_cur(ps->msgmap));
			atmt_popup[i].label.string = backtag;
			atmt_popup[i].label.style  = lNormal;
			atmt_popup[i].data.val	   = '<';

			atmt_popup[++i].type = tTail;

			if(mswin_popup(atmt_popup) == 1 && exoffer)
			  display_att_window(current->attp);
		    }
		}
		else if(mp.button == M_BUTTON_RIGHT){
		    MPopup atmt_popup[2];

		    atmt_popup[0].type = tQueue;
		    sprintf(backtag, "View Message #%ld",
			    mn_get_cur(ps->msgmap));
		    atmt_popup[0].label.string = backtag;
		    atmt_popup[0].label.style  = lNormal;
		    atmt_popup[0].data.val     = '<';

		    atmt_popup[1].type = tTail;

		    mswin_popup(atmt_popup);
#endif
		}
	    }
	    break;
#endif

	  case MC_WHEREIS :			/* whereis */
	    /*--- get string  ---*/
	    {int   rc, found = 0;
	     char *result = NULL, buf[64];
	     static char last[64], tmp[64];
	     HelpType help;

	     ps->mangled_footer = 1;
	     buf[0] = '\0';
	     sprintf(tmp, "Word to find %s%.40s%s: ",
		     (last[0]) ? "[" : "",
		     (last[0]) ? last : "",
		     (last[0]) ? "]" : "");
	     help = NO_HELP;
	     while(1){
		 int flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE;

		 rc = optionally_enter(buf,-FOOTER_ROWS(ps),0,sizeof(buf),
					 tmp,NULL,help,&flags);
		 if(rc == 3)
		   help = help == NO_HELP ? h_attach_index_whereis : NO_HELP;
		 else if(rc == 0 || rc == 1 || !buf[0]){
		     if(rc == 0 && !buf[0] && last[0])
		       strcpy(buf, last);

		     break;
		 }
	     }

	     if(rc == 0 && buf[0]){
		 ctmp = current;
		 while(ctmp = next_attline(ctmp))
		   if(srchstr(ctmp->dstring, buf)){
		       found++;
		       break;
		   }

		 if(!found){
		     ctmp = first_attline(current);

		     while(ctmp != current)
		       if(srchstr(ctmp->dstring, buf)){
			   found++;
			   break;
		       }
		       else
			 ctmp = next_attline(ctmp);
		 }
	     }
	     else
	       result = "WhereIs cancelled";

	     if(found && ctmp){
		 strcpy(last, buf);
		 result  = "Word found";
		 current = ctmp;
	     }

	     q_status_message(SM_ORDER, 0, 3,
			      result ? result : "Word not found");
	    }

	    break;

	  case MC_DELETE :
	    if(delete_attachment(msgno, current->attp)){
		int l = current ? strlen(current->attp->number) : 0;

		current->dstring[1] = 'D';

		/* Also indicate any children that will be deleted */

		for(ctmp = current; ctmp; ctmp = next_attline(ctmp))
		  if(!strncmp(ctmp->attp->number, current->attp->number, l)
		     && ctmp->attp->number[l] == '.'){
		      ctmp->dstring[1] = 'D';
		      ps->mangled_screen = 1;
		  }
	    }

	    break;

	  case MC_UNDELETE :
	    if(undelete_attachment(msgno, current->attp, &expbits)){
		int l = current ? strlen(current->attp->number) : 0;

		current->dstring[1] = ' ';

		/* And unflag anything implicitly undeleted */
		for(ctmp = current; ctmp; ctmp = next_attline(ctmp))
		  if(!strncmp(ctmp->attp->number, current->attp->number, l)
		     && ctmp->attp->number[l] == '.'
		     && (!msgno_exceptions(ps->mail_stream, msgno,
					   ctmp->attp->number, &expbits, FALSE)
			 || !(expbits & MSG_EX_DELETE))){
		      ctmp->dstring[1] = ' ';
		      ps->mangled_screen = 1;
		  }
	    }

	    break;

	  case MC_REPLY :
	    reply_msg_att(ps->mail_stream, msgno, current->attp);
	    break;

	  case MC_FORWARD :
	    forward_attachment(ps->mail_stream, msgno, current->attp);
	    break;

	  case MC_BOUNCE :
	    bounce_msg_att(ps->mail_stream, msgno, current->attp->number,
			   current->attp->body->nested.msg->env->subject);
	    ps->mangled_footer = 1;
	    break;

	  case MC_ABOUTATCH :
	    display_attach_info(msgno, current->attp);
	    break;

	  case MC_VIEW_ATCH :			/* View command */
	    display_attachment(msgno, current->attp, DA_SAVE);
	    old_cols = -1;
	    /* fall thru to repaint */

	  case MC_REPAINT :			/* redraw */
          case MC_RESIZE :
	    ps->mangled_screen = 1;
	    break;

	  case MC_EXPORT :
	    export_attachment(-FOOTER_ROWS(ps), msgno, current->attp);
	    ps->mangled_footer = 1;
	    break;

	  case MC_SAVETEXT :			/* Save command */
	    save_attachment(-FOOTER_ROWS(ps), msgno, current->attp);
	    ps->mangled_footer = 1;
	    break;

	  case MC_PRINTMSG :			/* Save command */
	    print_attachment(-FOOTER_ROWS(ps), msgno, current->attp);
	    ps->mangled_footer = 1;
	    break;

	  case MC_PIPE :			/* Pipe command */
	    if(F_ON(F_ENABLE_PIPE, ps)){
		pipe_attachment(msgno, current->attp);
		ps->mangled_footer = 1;
		break;
	    }					/* fall thru to complain */

	  case MC_NONE:				/* simple timeout */
	    break;

	  case MC_CHARUP :
	  case MC_CHARDOWN :
	  case MC_CHARRIGHT :
	  case MC_CHARLEFT :
	  case MC_GOTOBOL :
	  case MC_GOTOEOL :
	  case MC_UNKNOWN :
	  default:
	    bogus_command(ch, F_ON(F_USE_FK, ps) ? "F1" : "?");

	}
    }

    for(current = first_attline(current); current;){	/* clean up */
	ctmp = current->next;
	free_attline(&current);
	current = ctmp;
    }

    if(screen.titlecolor)
      free_color_pair(&screen.titlecolor);
}



/*----------------------------------------------------------------------
  allocate and attach a fresh attachment line struct

  Args: current -- display line to attache new struct to

  Returns: newly alloc'd attachment display line
  ----*/
ATDISP_S *
new_attline(current)
    ATDISP_S **current;
{
    ATDISP_S *p;

    p = (ATDISP_S *)fs_get(sizeof(ATDISP_S));
    memset((void *)p, 0, sizeof(ATDISP_S));
    if(current){
	if(*current){
	    p->next	     = (*current)->next;
	    (*current)->next = p;
	    p->prev	     = *current;
	    if(p->next)
	      p->next->prev = p;
	}

	*current = p;
    }

    return(p);
}



/*----------------------------------------------------------------------
  Release system resources associated with attachment display line

  Args: p -- line to free

  Result: 
  ----*/
void
free_attline(p)
    ATDISP_S **p;
{
    if(p){
	if((*p)->dstring)
	  fs_give((void **)&(*p)->dstring);

	fs_give((void **)p);
    }
}



/*----------------------------------------------------------------------
  Manage display of the attachment screen menu body.

  Args: ps -- pine struct pointer
	current -- currently selected display line
	screen -- reference points for display painting

  Result: 
 */
int
attachment_screen_updater(ps, current, screen)
    struct pine  *ps;
    ATDISP_S	 *current;
    ATT_SCREEN_S *screen;
{
    int	      dline, return_line = HEADER_ROWS(ps);
    ATDISP_S *top_line, *ctmp;

    /* calculate top line of display */
    dline = 0;
    ctmp = top_line = first_attline(current);
    do
      if(((dline++)%(ps->ttyo->screen_rows-HEADER_ROWS(ps)-FOOTER_ROWS(ps)))==0)
	top_line = ctmp;
    while(ctmp != current && (ctmp = next_attline(ctmp)));

#ifdef _WINDOWS
    /* Don't know how to manage scroll bar for attachment screen yet. */
    scroll_setrange (0L, 0L);
#endif

    /* mangled body or new page, force redraw */
    if(ps->mangled_body || screen->top_line != top_line)
      screen->current = NULL;

    /* loop thru painting what's needed */
    for(dline = 0, ctmp = top_line;
	dline < ps->ttyo->screen_rows - FOOTER_ROWS(ps) - HEADER_ROWS(ps);
	dline++, ctmp = next_attline(ctmp)){

	/*
	 * only fall thru painting if something needs painting...
	 */
	if(!(!screen->current || ctmp == screen->current || ctmp == current))
	  continue;

	if(ctmp && ctmp->dstring){
	    char *p = tmp_20k_buf;
	    int   i, j, x = 0;
	    if(F_ON(F_FORCE_LOW_SPEED,ps) || ps->low_speed){
		x = 2;
		if(ctmp == current){
		    return_line = dline + HEADER_ROWS(ps);
		    PutLine0(dline + HEADER_ROWS(ps), 0, "->");
		}
		else
		  PutLine0(dline + HEADER_ROWS(ps), 0, "  ");

		/*
		 * Only paint lines if we have to...
		 */
		if(screen->current)
		  continue;
	    }
	    else if(ctmp == current){
		return_line = dline + HEADER_ROWS(ps);
		StartInverse();
	    }

	    /*
	     * Copy the value to a temp buffer expanding tabs, and
	     * making sure not to write beyond screen right...
	     */
	    for(i=0,j=x; ctmp->dstring[i] && j < ps->ttyo->screen_cols; i++){
		if(ctmp->dstring[i] == ctrl('I')){
		    do
		      *p++ = ' ';
		    while(j < ps_global->ttyo->screen_cols && ((++j)&0x07));
			  
		}
		else{
		    *p++ = ctmp->dstring[i];
		    j++;
		}
	    }

	    *p = '\0';
	    PutLine0(dline + HEADER_ROWS(ps), x, tmp_20k_buf + x);

	    if(ctmp == current
	       && !(F_ON(F_FORCE_LOW_SPEED,ps) || ps->low_speed))
	      EndInverse();
	}
	else
	  ClearLine(dline + HEADER_ROWS(ps));
    }

    ps->mangled_body = 0;
    screen->top_line = top_line;
    screen->current  = current;
    return(return_line);
}


/*----------------------------------------------------------------------
  Redraw the attachment screen based on the global "att_screen" struct

  Args: none

  Result: 
  ----*/
void
attachment_screen_redrawer()
{
    bitmap_t	 bitmap;

    update_att_screen_titlebar();
    ps_global->mangled_header = 0;
    ClearLine(1);

    ps_global->mangled_body = 1;
    (void)attachment_screen_updater(ps_global,att_screen->current,att_screen);

    setbitmap(bitmap);
    draw_keymenu(&att_index_keymenu, bitmap, ps_global->ttyo->screen_cols,
		 1-FOOTER_ROWS(ps_global), 0, SameMenu);
}


void
update_att_screen_titlebar()
{
    long        raw_msgno;
    COLOR_PAIR *returned_color = NULL;
    COLOR_PAIR *titlecolor = NULL;
    int         colormatch;
    SEARCHSET  *ss = NULL;
    PAT_STATE  *pstate = NULL;

    if(ps_global->titlebar_color_style != TBAR_COLOR_DEFAULT){
	raw_msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));
	ss = mail_newsearchset();
	ss->first = ss->last = (unsigned long) raw_msgno;

	if(ss){
	    colormatch = get_index_line_color(ps_global->mail_stream,
					      ss, &pstate, &returned_color);
	    mail_free_searchset(&ss);

	    /*
	     * This is a bit tricky. If there is a colormatch but
	     * returned_color
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
	if((!titlecolor && att_screen->titlecolor)
	   ||
	   (titlecolor && !att_screen->titlecolor)
	   ||
	   (titlecolor && att_screen->titlecolor
	    && (strcmp(titlecolor->fg, att_screen->titlecolor->fg)
		|| strcmp(titlecolor->bg, att_screen->titlecolor->bg)))){

	    if(att_screen->titlecolor)
	      free_color_pair(&att_screen->titlecolor);

	    att_screen->titlecolor = titlecolor;
	    titlecolor = NULL;
	}

	if(titlecolor)
	  free_color_pair(&titlecolor);
    }

    set_titlebar("ATTACHMENT INDEX", ps_global->mail_stream,
		 ps_global->context_current, ps_global->cur_folder,
		 ps_global->msgmap, 1, MessageNumber, 0, 0,
		 att_screen->titlecolor);
}



/*----------------------------------------------------------------------
  Seek back from the given display line to the beginning of the list

  Args: p -- display linked list

  Result: 
  ----*/
ATDISP_S *
first_attline(p)
    ATDISP_S *p;
{
    while(p && p->prev)
      p = p->prev;

    return(p);
}


int
init_att_progress(msg, stream, body)
    char       *msg;
    MAILSTREAM *stream;
    BODY	*body;
{
    if(save_att_length = body->size.bytes){
	/* if there are display filters, factor in extra copy */
	if(body->type == TYPETEXT && ps_global->VAR_DISPLAY_FILTERS)
	  save_att_length += body->size.bytes;

	/* if remote folder and segment not cached, factor in IMAP fetch */
	if(stream && stream->mailbox && IS_REMOTE(stream->mailbox)
	   && !((body->type == TYPETEXT && body->contents.text.data)
		|| ((body->type == TYPEMESSAGE)
		    && body->nested.msg && body->nested.msg->text.text.data)
		|| body->contents.text.data))
	  save_att_length += body->size.bytes;

	gf_filter_init();			/* reset counters */
	pine_gets_bytes(1);
	save_att_piped(1);
	return(busy_alarm(1, msg, save_att_percent, 1));
    }

    return(0);
}


long
save_att_piped(reset)
    int reset;
{
    static long x;
    long	y;

    if(reset){
	x = y = 0L;
    }
    else if((y = gf_bytes_piped()) >= x){
	x = y;
	y = 0;
    }

    return(x + y);
}


int
save_att_percent()
{
    int i = (int) (((pine_gets_bytes(0) + save_att_piped(0)) * 100)
							   / save_att_length);
    return(min(i, 100));
}



/*----------------------------------------------------------------------
  Save the given attachment associated with the given message no

  Args: ps

  Result: 
  ----*/
void
save_attachment(qline, msgno, a)
     int       qline;
     long      msgno;
     ATTACH_S *a;
{
    if(ps_global->restricted){
        q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Pine demo can't save attachments");
        return;
    }

    if(MIME_MSG_A(a))
      save_msg_att(msgno, a);
    else if(MIME_DGST_A(a))
      save_digest_att(msgno, a);
    else if(MIME_VCARD_A(a))
      save_vcard_att(ps_global, qline, msgno, a);
    else
      write_attachment(qline, msgno, a, "SAVE");
}



/*----------------------------------------------------------------------
  Save the given attachment associated with the given message no

  Args: ps

  Result: 
  ----*/
void
export_attachment(qline, msgno, a)
     int       qline;
     long      msgno;
     ATTACH_S *a;
{
    if(ps_global->restricted){
        q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Pine demo can't export attachments");
        return;
    }

    if(MIME_MSG_A(a))
      export_msg_att(msgno, a);
    else if(MIME_DGST_A(a))
      export_digest_att(msgno, a);
    else
      q_status_message1(SM_ORDER, 0, 3,
     "Can't Export %.200s. Use \"Save\" to write file, \"<\" to leave index.",
	 body_type_names(a->body->type));
}


void
write_attachment(qline, msgno, a, method)
     int       qline;
     long      msgno;
     ATTACH_S *a;
     char     *method;
{
    char	filename[MAXPATH+1], full_filename[MAXPATH+1], *charset,
	       *l_string, title_buf[64], prompt_buf[256], *att_name, *err,
               *dec_err, *terr;
    int         r, is_text, rflags = GER_NONE, we_cancel = 0;
    long        len, orig_size;
    gf_io_t     pc;
    STORE_S    *store;
    static ESCKEY_S att_save_opts[] = {
	{ctrl('T'), 10, "^T", "To Files"},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    is_text = a->body->type == TYPETEXT;

    /*-------  Figure out suggested file name ----*/
    filename[0] = full_filename[0] = '\0';
    att_name	= "filename";

    /* warning: overload "err" use */
    if((a->body->disposition.type &&
       (err = rfc2231_get_param(a->body->disposition.parameter, att_name,
				NULL, NULL))) ||
       (err = rfc2231_get_param(a->body->parameter, att_name + 4, NULL, NULL))){
	if(err[0] == '=' && err[1] == '?'){
	    if(!(dec_err = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						  SIZEOF_20KBUF, err, NULL)))
	      dec_err = err;
	      
	}
	else
	  dec_err = err;
	terr = last_cmpnt(dec_err);
	if(!terr)
	  terr = dec_err;
	strncpy(filename, terr, sizeof(filename)-1);
	filename[sizeof(filename)-1] = '\0';
	fs_give((void **) &err);
    }

    dprint(9, (debugfile, "export_attachment(name: %s)\n",
	   filename ? filename : "?"));

    r = 0;
#if	!defined(DOS) && !defined(MAC) && !defined(OS2)
    if(ps_global->VAR_DOWNLOAD_CMD && ps_global->VAR_DOWNLOAD_CMD[0]){
	att_save_opts[++r].ch  = ctrl('V');
	att_save_opts[r].rval  = 12;
	att_save_opts[r].name  = "^V";
	att_save_opts[r].label = "Downld Msg";
    }
#endif	/* !(DOS || MAC) */

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	att_save_opts[++r].ch  =  ctrl('I');
	att_save_opts[r].rval  = 11;
	att_save_opts[r].name  = "TAB";
	att_save_opts[r].label = "Complete";
    }

    att_save_opts[++r].ch = -1;

    sprintf(title_buf, "%s ATTACHMENT", method);

    r = get_export_filename(ps_global, filename, NULL, full_filename,
			    sizeof(filename), "attachment", title_buf,
			    att_save_opts, &rflags, qline, GE_SEQ_SENSITIVE);

    if(r < 0){
	switch(r){
	  case -1:
	    cmd_cancelled(lcase((unsigned char *) title_buf + 1) - 1);
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
			      "Can't save to file outside of %.200s",
			      ps_global->VAR_OPER_DIR);
	    break;
	}

	return;
    }
#if	!defined(DOS) && !defined(MAC) && !defined(OS2)
    else if(r == 12){			/* Download */
	char     cmd[MAXPATH], *tfp = NULL;
	PIPE_S  *syspipe;
	gf_io_t  pc;

	if(ps_global->restricted){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Download disallowed in restricted mode");
	    return;
	}

	err = NULL;
	tfp = temp_nam(NULL, "pd");
	dprint(1, (debugfile, "Download attachment called!\n"));
	if(store = so_get(FileStar, tfp, WRITE_ACCESS|OWNER_ONLY)){

	    sprintf(prompt_buf, "Saving to \"%.50s\"", tfp);
	    we_cancel = init_att_progress(prompt_buf,
					  ps_global->mail_stream,
					  a->body);

	    gf_set_so_writec(&pc, store);
	    if(err = detach(ps_global->mail_stream, msgno,
			    a->number, &len, pc, NULL, 0))
	      q_status_message2(SM_ORDER | SM_DING, 3, 5,
			       "%.200s: Error writing attachment to \"%.200s\"",
				err, tfp);

	    /* cancel regardless, so it doesn't get in way of xfer */
	    cancel_busy_alarm(0);

	    gf_clear_so_writec(store);
	    if(so_give(&store))		/* close file */
	      err = "Error writing tempfile for download";

	    if(!err){
		build_updown_cmd(cmd, ps_global->VAR_DOWNLOAD_CMD_PREFIX,
				 ps_global->VAR_DOWNLOAD_CMD, tfp);
		if(syspipe = open_system_pipe(cmd, NULL, NULL,
					      PIPE_USER | PIPE_RESET, 0))
		  (void)close_system_pipe(&syspipe, NULL, 0);
		else
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
				   err = "Error running download command");
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 3,
			   err = "Error building temp file for download");

	if(tfp){
	    (void)unlink(tfp);
	    fs_give((void **)&tfp);
	}

	if(!err)
	  q_status_message1(SM_ORDER, 0, 4, "Part %.200s downloaded",
			    a->number);
			    
	return;
    }
#endif	/* !(DOS || MAC) */

    (void) write_attachment_to_file(ps_global->mail_stream, msgno, a,
				    rflags, full_filename);
}


/*
 * Args  stream --
 *       msgno  -- raw message number
 *       a      -- attachment struct
 *       flags  -- comes from get_export_filename
 *             GER_OVER -- the file was truncated before we wrote
 *           GER_APPEND -- the file was not truncated prior to our writing,
 *                         so we were appending
 *                 else -- the file didn't previously exist
 *       file   -- the full pathname of the file
 *
 * Returns  1 for successful write, 0 for nothing to write, -1 for error
 */
int
write_attachment_to_file(stream, msgno, a, flags, file)
    MAILSTREAM *stream;
    long        msgno;
    ATTACH_S   *a;
    int         flags;
    char       *file;
{
    char       *l_string, title_buf[64], sbuf[256], *err;
    int         is_text, we_cancel = 0;
    long        len, orig_size;
    gf_io_t     pc;
    STORE_S    *store;

    if(!(a && a->body && a->number && a->number[0] && file && file[0]
	 && stream))
      return 0;

    is_text = a->body->type == TYPETEXT;

    if(flags & GER_APPEND)
      orig_size = name_file_size(file);

    if((store = so_get(FileStar, file, WRITE_ACCESS)) == NULL){
	q_status_message2(SM_ORDER | SM_DING, 3, 5,
			  "Error opening destination %.200s: %.200s",
			  file, error_description(errno));
	return -1;
    }

    sprintf(sbuf, "Saving to \"%.50s\"", file);
    we_cancel = init_att_progress(sbuf, stream, a->body);

    gf_set_so_writec(&pc, store);
    err = detach(stream, msgno, a->number, &len, pc, NULL, 0);
    gf_clear_so_writec(store);

    if(we_cancel)
      cancel_busy_alarm(0);

    if(so_give(&store))			/* close file */
      err = error_description(errno);

    if(err){
	if(!(flags & (GER_APPEND | GER_OVER)))
	  unlink(file);
	else
	  truncate(file, (flags & GER_APPEND) ? (off_t) orig_size : (off_t) 0);

	q_status_message2(SM_ORDER | SM_DING, 3, 5,
			  "%.200s: Error writing attachment to \"%.200s\"",
			  err, file);
	return -1;
    }
    else{
        l_string = cpystr(byte_string(len));
        q_status_message8(SM_ORDER, 0, 4,
	     "Part %.200s, %.200s%.200s %.200s to \"%.200s\"%.200s%.200s%.200s",
			  a->number, 
			  is_text
			    ? comatose(a->body->size.lines) : l_string,
			  is_text ? " lines" : "",
			  flags & GER_OVER
			      ? "overwritten"
			      : flags & GER_APPEND ? "appended" : "written",
			  file,
			  (is_text || len == a->body->size.bytes)
			    ? "" : "(decoded from ",
                          (is_text || len == a->body->size.bytes)
			    ? "" : byte_string(a->body->size.bytes),
			  is_text || len == a->body->size.bytes
			    ? "" : ")");
        fs_give((void **)&l_string);
	return 1;
    }
}


char *
write_attached_msg(msgno, ap, store, newfile)
     long	msgno;
     ATTACH_S **ap;
     STORE_S   *store;
     int	newfile;
{
    char      *err = NULL;
    long      start_of_append;
    gf_io_t   pc;
    MESSAGECACHE *mc;

    if((*ap)->body->nested.msg->env){
	start_of_append = so_tell(store);

	gf_set_so_writec(&pc, store);
	if(!(ps_global->mail_stream && msgno > 0L
	   && msgno <= ps_global->mail_stream->nmsgs
	   && (mc = mail_elt(ps_global->mail_stream, msgno)) && mc->valid))
	  mc = NULL;
	
	if(!bezerk_delimiter((*ap)->body->nested.msg->env, mc, pc, newfile)
	   || !format_msg_att(msgno, ap, pc, FM_NOINDENT))
	  err = error_description(errno);

	gf_clear_so_writec(store);

	if(err)
	  ftruncate(fileno((FILE *)store->txt), start_of_append);
    }
    else
      err = "Can't export message. Missing Envelope data";

    return(err);
}


/*----------------------------------------------------------------------
  Save the attachment message/rfc822 to specified folder

  Args: 

  Result: 
  ----*/
void
save_msg_att(msgno, a)
    long      msgno;
    ATTACH_S *a;
{
    char	 newfolder[MAILTMPLEN], *save_folder, flags[64], date[64];
    char         nmsgs[80];
    CONTEXT_S   *cntxt = NULL;
    int		 our_stream = 0, rv;
    MAILSTREAM   *save_stream;
    MESSAGECACHE *mc;
    STORE_S      *so;

    sprintf(nmsgs, "Attached Msg (part %.20s) ", a->number);
    if(save_prompt(ps_global, &cntxt, newfolder, sizeof(newfolder), nmsgs,
		   a->body->nested.msg->env, msgno, a->number, NULL)){
	if(strucmp(newfolder, ps_global->inbox_name) == 0){
	    save_folder = ps_global->VAR_INBOX_PATH;
	    cntxt = NULL;
	}
	else
	  save_folder = newfolder;

	save_stream = save_msg_stream(cntxt, save_folder, &our_stream);

	if(so = so_get(SAVE_TMP_TYPE, NULL, WRITE_ACCESS)){
	    /* store flags before the fetch so UNSEEN bit isn't flipped */
	    mc = (msgno > 0L && ps_global->mail_stream
		  && msgno <= ps_global->mail_stream->nmsgs)
		  ? mail_elt(ps_global->mail_stream, msgno) : NULL;
	    flag_string(mc, F_ANS|F_FLAG|F_SEEN, flags);
	    if(mc && mc->day)
	      mail_date(date, mc);
	    else
	      *date = '\0';

	    rv = save_fetch_append(ps_global->mail_stream, msgno, a->number,
				   save_stream, save_folder, cntxt,
				   a->body->size.bytes, flags, date, so);
	    if(rv == 1)
	      q_status_message2(SM_ORDER, 0, 4,
			   "Attached message (part %.200s) saved to \"%.200s\"",
				a->number, 
				save_folder);
	    else if(rv == -1)
	      cmd_cancelled("Attached message Save");
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
    }
}


/*----------------------------------------------------------------------
  Save the message/rfc822 in the given digest to the specified folder

  Args: 

  Result: 
  ----*/
void
save_digest_att(msgno, a)
    long      msgno;
    ATTACH_S *a;
{
    char	 newfolder[MAILTMPLEN], *save_folder,
		 date[64], nmsgs[80], *p;
    CONTEXT_S   *cntxt = NULL;
    int		 our_stream = 0, rv, cnt = 0;
    MAILSTREAM   *save_stream;
    PART	 *part;
    STORE_S      *so;

    for(part = a->body->nested.part; part; part = part->next)
      if(MIME_MSG(part->body.type, part->body.subtype))
	cnt++;
    
    sprintf(nmsgs, "%d Msg Digest (part %.20s) ", cnt, a->number);

    if(save_prompt(ps_global, &cntxt, newfolder, sizeof(newfolder),
		   nmsgs, NULL, 0, NULL, NULL)){
	save_folder = (strucmp(newfolder, ps_global->inbox_name) == 0)
			? ps_global->VAR_INBOX_PATH : newfolder;

	save_stream = save_msg_stream(cntxt, save_folder, &our_stream);

	for(part = a->body->nested.part; part; part = part->next)
	  if(MIME_MSG(part->body.type, part->body.subtype)){
	      if(so = so_get(SAVE_TMP_TYPE, NULL, WRITE_ACCESS)){
		  *date = '\0';
		  rv = save_fetch_append(ps_global->mail_stream, msgno,
				       p = body_partno(ps_global->mail_stream,
						       msgno, &part->body),
				       save_stream, save_folder, cntxt,
				       part->body.size.bytes,
				       NULL, date, so);
		  fs_give((void **) &p);
	      }
	      else{
		  dprint(1, (debugfile, "Can't allocate store for save: %s\n",
			     error_description(errno)));
		  q_status_message(SM_ORDER | SM_DING, 3, 4,
				   "Problem creating space for message text.");
		  rv = 0;
	      }

	      if(rv != 1)
		break;
	  }

	if(rv == 1)
	  q_status_message2(SM_ORDER, 0, 4,
			    "Attached digest (part %.200s) saved to \"%.200s\"",
			    a->number, 
			    save_folder);
	else if(rv == -1)
	  cmd_cancelled("Attached digest Save");
	/* else whatever broke in save_fetch_append shoulda bitched */

	if(our_stream)
	  mail_close(save_stream);
    }
}


MAILSTREAM *
save_msg_stream(context, folder, we_opened)
    CONTEXT_S *context;
    char      *folder;
    int	      *we_opened;
{
    char	tmp[MAILTMPLEN];
    MAILSTREAM *save_stream = NULL;

    if(IS_REMOTE(context_apply(tmp, context, folder, sizeof(tmp)))
       && !(save_stream = sp_stream_get(tmp, SP_MATCH))
       && !(save_stream = sp_stream_get(tmp, SP_SAME))){
	if(save_stream = context_open(context, NULL, folder,
				      OP_HALFOPEN | SP_USEPOOL | SP_TEMPUSE,
				      NULL))
	  *we_opened = 1;
    }

    return(save_stream);
}


/*----------------------------------------------------------------------
  Export the attachment message/rfc822 to specified file

  Args: 

  Result: 
  ----*/
void
export_msg_att(msgno, a)
    long      msgno;
    ATTACH_S *a;
{
    char      filename[MAXPATH+1], full_filename[MAXPATH+1], *err;
    int	      rv, rflags = GER_NONE, i = 1;
    ATTACH_S *ap = a;
    STORE_S  *store;
    static ESCKEY_S opts[] = {
	{ctrl('T'), 10, "^T", "To Files"},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	opts[i].ch    =  ctrl('I');
	opts[i].rval  = 11;
	opts[i].name  = "TAB";
	opts[i].label = "Complete";
    }

    filename[0] = full_filename[0] = '\0';

    rv = get_export_filename(ps_global, filename, NULL, full_filename,
			     sizeof(filename), "msg attachment",
			     "MSG ATTACHMENT", opts,
			     &rflags, -FOOTER_ROWS(ps_global),
			     GE_IS_EXPORT | GE_SEQ_SENSITIVE);

    if(rv < 0){
	switch(rv){
	  case -1:
	    cmd_cancelled("Export");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
			      "Can't export to file outside of %.200s",
			      ps_global->VAR_OPER_DIR);
	    break;
	}

	return;
    }

    /* With name in hand, allocate storage object and save away... */
    if(store = so_get(FileStar, full_filename, WRITE_ACCESS)){
	if(err = write_attached_msg(msgno, &ap, store, !(rflags & GER_APPEND)))
	  q_status_message(SM_ORDER | SM_DING, 3, 4, err);
	else
          q_status_message3(SM_ORDER, 0, 4,
			  "Attached message (part %.200s) %.200s to \"%.200s\"",
			    a->number, 
			    rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "written",
			    full_filename);

	if(so_give(&store))
	  q_status_message2(SM_ORDER | SM_DING, 3, 4,
			   "Error writing %.200s: %.200s",
			   full_filename, error_description(errno));
    }
    else
      q_status_message2(SM_ORDER | SM_DING, 3, 4,
		    "Error opening file \"%.200s\" to export message: %.200s",
			full_filename, error_description(errno));
}


/*----------------------------------------------------------------------
  Export the message/rfc822 in the given digest to the specified file

  Args: 

  Result: 
  ----*/
void
export_digest_att(msgno, a)
    long      msgno;
    ATTACH_S *a;
{
    char      filename[MAXPATH+1], full_filename[MAXPATH+1], *err = NULL;
    int	      rv, rflags = GER_NONE, i = 1;
    long      count = 0L;
    ATTACH_S *ap;
    STORE_S  *store;
    static ESCKEY_S opts[] = {
	{ctrl('T'), 10, "^T", "To Files"},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	opts[i].ch    =  ctrl('I');
	opts[i].rval  = 11;
	opts[i].name  = "TAB";
	opts[i].label = "Complete";
    }

    filename[0] = full_filename[0] = '\0';

    rv = get_export_filename(ps_global, filename, NULL, full_filename,
			     sizeof(filename), "digest", "DIGEST ATTACHMENT",
			     opts, &rflags, -FOOTER_ROWS(ps_global),
			     GE_IS_EXPORT | GE_SEQ_SENSITIVE);

    if(rv < 0){
	switch(rv){
	  case -1:
	    cmd_cancelled("Export");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
			      "Can't export to file outside of %.200s",
			      ps_global->VAR_OPER_DIR);
	    break;
	}

	return;
    }

    /* With name in hand, allocate storage object and save away... */
    if(store = so_get(FileStar, full_filename, WRITE_ACCESS)){
	count = 0;

	for(ap = a + 1;
	    ap->description
	      && !strncmp(a->number, ap->number, strlen(a->number))
	      && !err;
	    ap++){
	    if(ap->body->type == TYPEMESSAGE){
		if(ap->body->subtype && strucmp(ap->body->subtype, "rfc822")){
		    char buftmp[MAILTMPLEN];

		    sprintf(buftmp, "%.75s", ap->body->subtype);
		    sprintf(tmp_20k_buf, "  [Unknown Message subtype: %s ]\n",
			    buftmp);
		    if(!so_puts(store, tmp_20k_buf))
		      err = "Can't write export file";
		}
		else{
		    count++;
		    err = write_attached_msg(msgno, &ap, store,
					     !count && !(rflags & GER_APPEND));
		}
	    }
	    else if(!so_puts(store, "Unknown type in Digest"))
	      err = "Can't write export file";
	}

	if(so_give(&store))
	  err = error_description(errno);

	if(err){
	    q_status_message1(SM_ORDER | SM_DING, 3, 3,
			      "Error exporting: %.200s", err);
	    q_status_message1(SM_ORDER | SM_DING, 3, 3,
			"%.200s messages exported before error occurred", err);
	}
	else
          q_status_message4(SM_ORDER, 0, 4,
		"%.200s messages in digest (part %.200s) %.200s to \"%.200s\"",
			    long2string(count),
			    a->number, 
			    rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "written",
			    full_filename);
    }
    else
      q_status_message2(SM_ORDER | SM_DING, 3, 4,
		    "Error opening file \"%.200s\" to export digest: %.200s",
			full_filename, error_description(errno));
}



/*----------------------------------------------------------------------
  Print the given attachment associated with the given message no

  Args: ps

  Result: 
  ----*/
void
print_attachment(qline, msgno, a)
     int       qline;
     long      msgno;
     ATTACH_S *a;
{
    char prompt[250];
    int	 next = 0;

    if(ps_global->restricted){
        q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Pine demo can't Print attachments");
        return;
    }

    sprintf(prompt, "attach%s %.100s ",
	    (a->body->type == TYPETEXT) ? "ment" : "ed message",
	    MIME_DGST_A(a) ? "digest" : a->number);
    if(open_printer(prompt) >= 0){
	if(MIME_MSG_A(a))
	  (void) print_msg_att(msgno, a, next);
	else if(MIME_DGST_A(a))
	  print_digest_att(msgno, a);
	else
	  (void) decode_text(a, msgno, print_char, NULL, QStatus, FM_NOINDENT);

	close_printer();
    }
}


/*----------------------------------------------------------------------
  Print the attachment message/rfc822 to specified file

  Args: 

  Result: 
  ----*/
int
print_msg_att(msgno, a, next)
    long      msgno;
    ATTACH_S *a;
    int	      next;
{
    ATTACH_S *ap = a;
    MESSAGECACHE *mc;

    if(!(ps_global->mail_stream && msgno > 0L
       && msgno <= ps_global->mail_stream->nmsgs
       && (mc = mail_elt(ps_global->mail_stream, msgno)) && mc->valid))
      mc = NULL;

    if(((next && F_ON(F_AGG_PRINT_FF, ps_global)) ? print_char(FORMFEED) : 1)
       && pine_mail_fetchstructure(ps_global->mail_stream, msgno, NULL)
       && (F_ON(F_FROM_DELIM_IN_PRINT, ps_global)
	     ? bezerk_delimiter(a->body->nested.msg->env, mc, print_char, next)
	     : 1)
       && format_msg_att(msgno, &ap, print_char, FM_NOINDENT))
      return(1);


    q_status_message2(SM_ORDER | SM_DING, 3, 3,
		      "Error printing message %.200s, part %.200s",
		      long2string(msgno), a->number);
    return(0);
}


/*----------------------------------------------------------------------
  Print the attachment message/rfc822 to specified file

  Args: 

  Result: 
  ----*/
void
print_digest_att(msgno, a)
    long      msgno;
    ATTACH_S *a;
{
    ATTACH_S *ap;
    int	      next = 0;

    for(ap = a + 1;
	ap->description
	  && !strncmp(a->number, ap->number, strlen(a->number));
	ap++, next++){
	if(ap->body->type == TYPEMESSAGE){
	    if(ap->body->subtype && strucmp(ap->body->subtype, "rfc822")){
		char buftmp[MAILTMPLEN];

		sprintf(buftmp, "%.75s", ap->body->subtype);
		sprintf(tmp_20k_buf, "  [Unknown Message subtype: %s ]\n",
			buftmp);
		if(!gf_puts(tmp_20k_buf, print_char))
		  break;
	    }
	    else{
		char *p = part_desc(ap->number, ap->body->nested.msg->body,
				    0, 80, print_char);
		if(p){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      "Can't print digest: %.200s", p);
		    break;
		}
		else if(print_msg_att(msgno, ap, next))
		  break;
	    }
	}
	else if(ap->body->type == TYPETEXT
		&& decode_text(ap, msgno, print_char, NULL, QStatus,
			       FM_NOINDENT))
	  break;
	else if(!gf_puts("Unknown type in Digest", print_char))
	  break;
    }
}


/*----------------------------------------------------------------------
  Unpack and display the given attachment associated with given message no.

  Args: msgno -- message no attachment is part of
	a -- attachment to display

  Returns: 0 on success, non-zero (and error message queued) otherwise
  ----*/        
int
display_attachment(msgno, a, flags)
     long      msgno;
     ATTACH_S *a;
     int       flags;
{
    char    *filename = NULL;
    STORE_S *store;
    gf_io_t  pc;
    char    *err;
    int      we_cancel = 0;
    char     prefix[8];

    /*------- Display the attachment -------*/
    if(dispatch_attachment(a) == MCD_NONE) {
        /*----- Can't display this type ------*/
	if(a->body->encoding < ENCOTHER)
	  q_status_message4(SM_ORDER | SM_DING, 3, 5,
	     "Don't know how to display %.200s%.200s%.200s attachments.%.200s",
			    body_type_names(a->body->type),
			    a->body->subtype ? "/" : "",
			    a->body->subtype ? a->body->subtype :"",
			    (flags & DA_SAVE) ? " Try Save." : "");
	else
	  q_status_message1(SM_ORDER | SM_DING, 3, 5,
			    "Don't know how to unpack \"%.200s\" encoding",
			    body_encodings[(a->body->encoding <= ENCMAX)
					     ? a->body->encoding : ENCOTHER]);

        return(1);
    }
    else if(!(a->can_display & MCD_EXTERNAL)){
	if(a->body->type == TYPEMULTIPART){
	    if(a->body->subtype){
		if(!strucmp(a->body->subtype, "digest"))
		  display_digest_att(msgno, a, flags);
		else
		  q_status_message1(SM_ORDER, 3, 5,
				   "Can't display Multipart/%.200s",
				   a->body->subtype);
	    }
	    else
	      q_status_message(SM_ORDER, 3, 5,
			       "Can't display unknown Multipart Subtype");
	}
	else if(MIME_VCARD_A(a))
	  display_vcard_att(msgno, a, flags);
	else if(a->body->type == TYPETEXT)
	  display_text_att(msgno, a, flags);
	else if(a->body->type == TYPEMESSAGE)
	  display_msg_att(msgno, a, flags);

	ps_global->mangled_screen = 1;
	return(0);
    }

    if(F_OFF(F_QUELL_ATTACH_EXTRA_PROMPT, ps_global)
       && (!(flags & DA_DIDPROMPT)))
      if(want_to("View selected Attachment", 'y',
		 0, NO_HELP, WT_NORM) == 'n'){
	  cmd_cancelled(NULL);
	  return(1);
      }
    if(F_OFF(F_QUELL_ATTACH_EXT_WARN, ps_global)
       && (a->can_display & MCD_EXT_PROMPT)){
	char prompt[256], *namep = NULL, *dec_namep = NULL, *ext = NULL;

	if(namep = rfc2231_get_param(a->body->parameter, "name", NULL, NULL)){
	    if(namep[0] == '=' && namep[1] == '?'){
		if(!(dec_namep = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
							SIZEOF_20KBUF, namep, NULL)))
		  dec_namep = namep;
	    }
	    else
	      dec_namep = namep;

	    mt_get_file_ext((char *) dec_namep, &ext);
	}
	sprintf(prompt, 
		"Attachment %.12s%s unrecognized. %s%.10s%s", 
		a->body->subtype, 
		strlen(a->body->subtype) > 12 ? "..." : "", 
		ext ? "Try open by file extension (." : "Try opening anyway",
		ext ? ext : "",
		ext ? ")" : "");
	if(namep)
	  fs_give((void **)&namep);
	if(want_to(prompt, 'n', 0, NO_HELP, WT_NORM) == 'n'){
	    cmd_cancelled(NULL);
	    return(1);
	}
    }
    /*------ Write the image to a temporary file ------*/
    if(mime_os_specific_access()){
	/*
	 *  Windows applications are particular about the file name extensions
	 *  Try to look up the expected extension from the mime.types file.
	 *
	 *  This is now general, because now unix-based (Mac OS X)
	 *  apps are finnicky about extensions.  It's probably ok
	 *  to do extensions for all platforms, but we'll just do
	 *  Windows and OSX for now.
	 *
	 *  The mime_os_specific_access() call is only to delineate
	 *  Win and Mac from others.
	 */
	char ext[32];
	char mtype[128];

	ext[0] = '\0';
	strcpy (mtype, body_type_names(a->body->type));
	if (a->body->subtype) {
	    strcat (mtype, "/");
	    strcat (mtype, a->body->subtype);
	}

	if(!set_mime_extension_by_type(ext, mtype)){
	    char *namep, *dec_namep, *dotp, *p;

	    if(namep = rfc2231_get_param(a->body->parameter, "name", NULL, NULL)){
		if(namep[0] == '=' && namep[1] == '?'){
		    if(!(dec_namep = 
			 (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						SIZEOF_20KBUF, namep, NULL)))
		      dec_namep = namep;
		}
		else
		  dec_namep = namep;

		for(dotp = NULL, p = dec_namep; *p; p++)
		  if(*p == '.')
		    dotp = p + 1;

		if(dotp && strlen(dotp) < sizeof(ext) - 1)
		  strcpy(ext, dotp);

		if(namep)
		  fs_give((void **) &namep);
	    }
	}

	filename = temp_nam_ext(NULL, "im", ext);
	if(!filename) /* just try opening it without extension */
	  filename = temp_nam(NULL, "im");
    }
    else{
	sprintf(prefix, "img-%.3s", (a->body->subtype)
		? a->body->subtype : "unk");
	filename = temp_nam(NULL, prefix);
    }

    if((store = so_get(FileStar, filename, WRITE_ACCESS|OWNER_ONLY)) == NULL){
        q_status_message2(SM_ORDER | SM_DING, 3, 5,
                          "Error \"%.200s\", Can't write file %.200s",
                          error_description(errno), filename);
	if(filename){
	    (void)unlink(filename);
	    fs_give((void **)&filename);
	}

        return(1);
    }


    if(a->body->size.bytes){
	char msg_buf[128];

	sprintf(msg_buf, "Decoding %s%.50s%.20s%s",
		a->description ? "\"" : "",
		a->description ? a->description : "attachment number ",
		a->description ? "" : a->number,
		a->description ? "\"" : "");
	we_cancel = init_att_progress(msg_buf, ps_global->mail_stream,
				      a->body);
    }

    if(a->body->type == TYPEMULTIPART){
	char *h = mail_fetch_mime(ps_global->mail_stream, msgno, a->number,
				  NULL, 0L);

	/* Write to store while converting newlines */
	while(h && *h)
	  if(*h == '\015' && *(h+1) == '\012'){
	      so_puts(store, NEWLINE);
	      h += 2;
	  }
	  else
	    so_writec(*h++, store);
    }

    gf_set_so_writec(&pc, store);

    err = detach(ps_global->mail_stream, msgno, a->number, NULL, pc, NULL, 0);

    gf_clear_so_writec(store);

    if(we_cancel)
      cancel_busy_alarm(0);

    so_give(&store);

    if(err){
	q_status_message2(SM_ORDER | SM_DING, 3, 5,
		     "%.200s: Error saving image to temp file %.200s",
		     err, filename);
	if(filename){
	    (void)unlink(filename);
	    fs_give((void **)&filename);
	}

	return(1);
    }

    /*----- Run the viewer process ----*/
    run_viewer(filename, a->body, a->can_display & MCD_EXT_PROMPT);
    if(filename)
      fs_give((void **)&filename);

    return(0);
}


/*----------------------------------------------------------------------
   Fish the required command from mailcap and run it

  Args: image_file -- The name of the file to pass viewer
	body -- body struct containing type/subtype of part

A side effect may be that scrolltool is called as well if
exec_mailcap_cmd has any substantial output...
 ----*/
void
run_viewer(image_file, body, chk_extension)
     char *image_file;
     BODY *body;
{
    MCAP_CMD_S *mc_cmd   = NULL;
    int   needs_terminal = 0, we_cancel = 0;

    we_cancel = busy_alarm(1, "Displaying attachment", NULL, 1);

    if(mc_cmd = mailcap_build_command(body->type, body->subtype,
				      body->parameter, image_file,
				      &needs_terminal, chk_extension)){
	if(we_cancel)
	  cancel_busy_alarm(-1);

	exec_mailcap_cmd(mc_cmd, image_file, needs_terminal);
	if(mc_cmd->command)
	  fs_give((void **)&mc_cmd->command);
	fs_give((void **)&mc_cmd);
    }
    else{
	if(we_cancel)
	  cancel_busy_alarm(-1);

	q_status_message1(SM_ORDER, 3, 4, "Cannot display %.200s attachment",
			  type_desc(body->type, body->subtype,
				    body->parameter, 1));
    }
}



/*----------------------------------------------------------------------
  Detach and provide for browsing a text body part

  Args: msgno -- raw message number to get part from
	 a -- attachment struct for the desired part

  Result: 
 ----*/
STORE_S *
format_text_att(msgno, a, handlesp)
    long       msgno;
    ATTACH_S  *a;
    HANDLE_S **handlesp;
{
    STORE_S	*store;
    gf_io_t	 pc;
    int		 flags;

    if(store = so_get(CharStar, NULL, EDIT_ACCESS)){
	flags = FM_DISPLAY;

	if(handlesp)
	  init_handles(handlesp);

	gf_set_so_writec(&pc, store);
	(void) decode_text(a, msgno, pc, handlesp, QStatus, flags);
	gf_clear_so_writec(store);
    }

    return(store);
}



/*----------------------------------------------------------------------
  Detach and provide for browsing a text body part

  Args: msgno -- raw message number to get part from
	 a -- attachment struct for the desired part

  Result: 
 ----*/
void
display_text_att(msgno, a, flags)
    long      msgno;
    ATTACH_S *a;
    int	      flags;
{
    STORE_S  *store;
    HANDLE_S *handles = NULL;

    if(msgno > 0L)
      clear_index_cache_ent(msgno);

    if(store = format_text_att(msgno, a, &handles)){
	scroll_attachment("ATTACHED TEXT", store, CharStar, handles, a, flags);
	free_handles(&handles);
	so_give(&store);	/* free resources associated with store */
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3,
		       "Error allocating space for attachment.");
}


/*----------------------------------------------------------------------
  Detach and provide for browsing a body part of type "MESSAGE"

  Args: msgno -- message number to get partrom
	 a -- attachment struct for the desired part

  Result: 
 ----*/
void
display_msg_att(msgno, a, flags)
    long      msgno;
    ATTACH_S *a;
    int	      flags;
{
    STORE_S	*store;
    gf_io_t	 pc;
    ATTACH_S	*ap = a;

    if(msgno > 0L)
      clear_index_cache_ent(msgno);

    /* BUG, should check this return code */
    (void) pine_mail_fetchstructure(ps_global->mail_stream, msgno, NULL);

    /* initialize a storage object */
    if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Error allocating space for message.");
	return;
    }

    gf_set_so_writec(&pc, store);

    if(format_msg_att(msgno, &ap, pc, FM_DISPLAY)){
	if(ps_global->full_header == 2)
	  q_status_message(SM_INFO, 0, 3,
		      "Full header mode ON.  All header text being included");

	scroll_attachment((a->body->subtype
			   && !strucmp(a->body->subtype, "delivery-status"))
			     ? "DELIVERY STATUS REPORT" : "ATTACHED MESSAGE",
			  store, CharStar, NULL, a, flags);
    }

    gf_clear_so_writec(store);

    so_give(&store);	/* free resources associated with store */
}


/*----------------------------------------------------------------------
  Detach and provide for browsing a multipart body part of type "DIGEST"

  Args: msgno -- message number to get partrom
	 a -- attachment struct for the desired part

  Result: 
 ----*/
void
display_digest_att(msgno, a, flags)
    long      msgno;
    ATTACH_S *a;
    int	      flags;
{
    STORE_S     *store;
    ATTACH_S	*ap;
    gf_io_t      pc;
    SourceType	 src = CharStar;
    int		 bad_news = 0;

    if(msgno > 0L)
      clear_index_cache_ent(msgno);

    /* BUG, should check this return code */
    (void) pine_mail_fetchstructure(ps_global->mail_stream, msgno, NULL);

    /* initialize a storage object */
#if	defined(DOS) && !defined(WIN32)
    if(a->body->size.bytes > MAX_MSG_INCORE
       || (ps_global->mail_stream
           && ps_global->mail_stream->dtb
	   && ps_global->mail_stream->dtb->name
	   && !strcmp(ps_global->mail_stream->dtb->name, "nntp")))
      src = TmpFileStar;
#endif

    if(!(store = so_get(src, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Error allocating space for message.");
	return;
    }

    gf_set_so_writec(&pc, store);

    /*
     * While in a subtype of this message
     */
    for(ap = a + 1;
	ap->description
	  && !strncmp(a->number, ap->number, strlen(a->number))
	  && !bad_news;
	ap++){
	if(ap->body->type == TYPEMESSAGE){
	    char *errstr;

	    if(ap->body->subtype && strucmp(ap->body->subtype, "rfc822")){
		char buftmp[MAILTMPLEN];

		sprintf(buftmp, "%.75s", ap->body->subtype);
		sprintf(tmp_20k_buf, "Unknown Message subtype: %s",
			buftmp);
		if(errstr = format_editorial(tmp_20k_buf,
					     ps_global->ttyo->screen_cols,
					     pc)){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      "Can't format digest: %.200s", errstr);
		    bad_news++;
		}
		else if(!gf_puts(NEWLINE, pc))
		  bad_news++;
	    }
	    else{
		if(errstr = part_desc(ap->number, ap->body->nested.msg->body,
				      0, ps_global->ttyo->screen_cols, pc)){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      "Can't format digest: %.200s", errstr);
		    bad_news++;
		}
		else if(format_msg_att(msgno, &ap, pc, FM_DISPLAY))
		  bad_news++;
	    }
	}
	else if(ap->body->type == TYPETEXT
		&& decode_text(ap, msgno, pc, NULL, QStatus, FM_DISPLAY))
	  bad_news++;
	else if(!gf_puts("Unknown type in Digest", pc))
	  bad_news++;
    }

    if(!bad_news){
	if(ps_global->full_header == 2)
	  q_status_message(SM_INFO, 0, 3,
		      "Full header mode ON.  All header text being included");

	scroll_attachment("ATTACHED MESSAGES", store, src, NULL, a, flags);
    }

    gf_clear_so_writec(store);
    so_give(&store);	/* free resources associated with store */
}


int
scroll_attachment(title, store, src, handles, a, flags)
    char	*title;
    STORE_S	*store;
    SourceType	 src;
    HANDLE_S	*handles;
    ATTACH_S	*a;
    int		 flags;
{
    SCROLL_S  sargs;

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text    = so_text(store);
    sargs.text.src     = src;
    sargs.text.desc    = "attachment";
    sargs.text.handles = handles;
    sargs.bar.title    = title;
    sargs.proc.tool    = process_attachment_cmd;
    sargs.proc.data.p  = (void *) a;
    sargs.help.text    = h_mail_text_att_view;
    sargs.help.title   = "HELP FOR ATTACHED TEXT VIEW";
    sargs.keys.menu    = &att_view_keymenu;
    setbitmap(sargs.keys.bitmap);

    /* First, fix up "back" key */
    if(flags & DA_FROM_VIEW){
	att_view_keymenu.keys[ATV_BACK_KEY].label = "MsgText";
    }
    else{
	att_view_keymenu.keys[ATV_BACK_KEY].label = "AttchIndex";
    }

    if(!handles){
	clrbitn(ATV_VIEW_HILITE, sargs.keys.bitmap);
	clrbitn(ATV_PREV_URL, sargs.keys.bitmap);
	clrbitn(ATV_NEXT_URL, sargs.keys.bitmap);
    }

    if(F_OFF(F_ENABLE_PIPE, ps_global))
      clrbitn(ATV_PIPE_KEY, sargs.keys.bitmap);

    if(!(a->body->type == TYPETEXT || MIME_MSG_A(a) || MIME_DGST_A(a)))
      clrbitn(ATV_PRINT_KEY, sargs.keys.bitmap);

    /*
     * If message or digest, leave Reply and Save and,
     * conditionally, Bounce on...
     */
    if(MIME_MSG_A(a) || MIME_DGST_A(a)){
	if(F_OFF(F_ENABLE_BOUNCE, ps_global))
	  clrbitn(ATV_BOUNCE_KEY, sargs.keys.bitmap);
    }
    else{
	clrbitn(ATV_BOUNCE_KEY, sargs.keys.bitmap);
	clrbitn(ATV_REPLY_KEY, sargs.keys.bitmap);
	clrbitn(ATV_EXPORT_KEY, sargs.keys.bitmap);
    }

    sargs.use_indexline_color = 1;

    if(DA_RESIZE & flags)
      sargs.resize_exit = 1;

#ifdef	_WINDOWS
    scrat_attachp    = a;
    sargs.mouse.popup = scroll_att_popup;
#endif

    return(scrolltool(&sargs));
}



int
process_attachment_cmd(cmd, msgmap, sparms)
    int	      cmd;
    MSGNO_S  *msgmap;
    SCROLL_S *sparms;
{
    int rv = 0, n;
    long rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
#define	AD(X)	((ATTACH_S *) (X)->proc.data.p)

    switch(cmd){
      case MC_EXIT :
	rv = 1;
	break;

      case MC_QUIT :
	ps_global->next_screen = quit_screen;
	break;

      case MC_MAIN :
	ps_global->next_screen = main_menu_screen;
	break;

      case MC_REPLY :
	reply_msg_att(ps_global->mail_stream, rawno, sparms->proc.data.p);
	break;

      case MC_FORWARD :
	forward_attachment(ps_global->mail_stream, rawno, sparms->proc.data.p);
	break;

      case MC_BOUNCE :
	bounce_msg_att(ps_global->mail_stream, rawno, AD(sparms)->number,
		       AD(sparms)->body->nested.msg->env->subject);
	ps_global->mangled_footer = 1;
	break;

      case MC_DELETE :
	delete_attachment(rawno, sparms->proc.data.p);
	break;

      case MC_UNDELETE :
	if(undelete_attachment(rawno, sparms->proc.data.p, &n))
	  q_status_message1(SM_ORDER, 0, 3,
			    "Part %.200s UNdeleted", AD(sparms)->number);

	break;

      case MC_SAVE :
	save_attachment(-FOOTER_ROWS(ps_global), rawno, sparms->proc.data.p);
	ps_global->mangled_footer = 1;
	break;

      case MC_EXPORT :
	export_attachment(-FOOTER_ROWS(ps_global), rawno, sparms->proc.data.p);
	ps_global->mangled_footer = 1;
	break;

      case MC_PRINTMSG :
	print_attachment(-FOOTER_ROWS(ps_global), rawno, sparms->proc.data.p);
	ps_global->mangled_footer = 1;
	break;

      case MC_PIPE :
	pipe_attachment(rawno, sparms->proc.data.p);
	ps_global->mangled_footer = 1;
	break;

      default:
	panic("Unexpected command case");
	break;
    }

    return(rv);
}



int
format_msg_att(msgno, a, pc, flags)
    long       msgno;
    ATTACH_S **a;
    gf_io_t    pc;
    int	       flags;
{
    int rv = 1;

    if((*a)->body->type != TYPEMESSAGE)
      return(gf_puts("[ Undisplayed Attachment of Type ", pc)
	     && gf_puts(body_type_names((*a)->body->type), pc)
	     && gf_puts(" ]", pc) && gf_puts(NEWLINE, pc));

    if((*a)->body->subtype && strucmp((*a)->body->subtype, "rfc822") == 0){
	HEADER_S h;

	HD_INIT(&h, ps_global->VAR_VIEW_HEADERS, ps_global->view_all_except,
		FE_DEFAULT);
	switch(format_header(ps_global->mail_stream, msgno, (*a)->number,
			     (*a)->body->nested.msg->env, &h, NULL, NULL,
			     0, pc)){
	  case -1 :			/* write error */
	    return(0);

	  case 1 :			/* fetch error */
	    if(!(gf_puts("[ Error fetching header ]",  pc)
		 && !gf_puts(NEWLINE, pc)))
	      return(0);

	    break;
	}

	gf_puts(NEWLINE, pc);
	if((++(*a))->description
	   && (*a)->body && (*a)->body->type == TYPETEXT){
	    if(decode_text(*a, msgno, pc, NULL, QStatus, flags))
	      rv = 0;
	}
	else if(!(gf_puts("[Can't display ", pc)
		  && gf_puts(((*a)->description && (*a)->body)
			       ? "first non-" : "missing ", pc)
		  && gf_puts("text segment]", pc)
		  && gf_puts(NEWLINE, pc)))
	  rv = 0;
    }
    else if((*a)->body->subtype 
	    && strucmp((*a)->body->subtype, "external-body") == 0) {
	if(format_editorial("This part is not included and can be fetched as follows:",
			    ps_global->ttyo->screen_cols,
			    pc)
	   || !gf_puts(NEWLINE, pc)
	   || format_editorial(display_parameters((*a)->body->parameter),
			       ps_global->ttyo->screen_cols,
			       pc))
	  rv = 0;
    }
    else if(decode_text(*a, msgno, pc, NULL, QStatus, flags))
      rv = 0;

    return(rv);
}


void
display_vcard_att(msgno, a, flags)
    long      msgno;
    ATTACH_S *a;
    int	      flags;
{
    STORE_S   *in_store, *out_store = NULL;
    HANDLE_S  *handles = NULL;
    gf_io_t    gc, pc;
    char     **lines, **ll, *errstr = NULL, tmp[MAILTMPLEN], *p;
    int	       cmd, indent, begins = 0;

    lines = detach_vcard_att(ps_global->mail_stream,
			     msgno, a->body, a->number);
    if(!lines){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Error accessing attachment."); 
	return;
    }

    if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS))){
	free_list_array(&lines);
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Error allocating space for attachment."); 
	return;
    }

    for(ll = lines, indent = 0; ll && *ll; ll++)
      if((p = strindex(*ll, ':')) && p - *ll > indent)
	indent = p - *ll;

    indent += 5;
    for(ll = lines; ll && *ll; ll++){
	if(p = strindex(*ll, ':')){
	    if(begins < 2 && struncmp(*ll, "begin:", 6) == 0)
	      begins++;

	    sprintf(tmp, "  %-*.*s : ", indent - 5,
		    min(p - *ll, sizeof(tmp)-5), *ll);
	    so_puts(in_store, tmp);
	    p++;
	}
	else{
	    p = *ll;
	    so_puts(in_store, repeat_char(indent, SPACE));
	}

	sprintf(tmp, "%.200s", p);
	so_puts(in_store,
		(char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				       SIZEOF_20KBUF, tmp, NULL));
	so_puts(in_store, "\015\012");
    }

    free_list_array(&lines);

    so_puts(in_store, "\015\012\015\012");

    do{
	if(out_store = so_get(CharStar, NULL, EDIT_ACCESS)){
	    so_seek(in_store, 0L, 0);

	    init_handles(&handles);
	    gf_filter_init();

	    if(F_ON(F_VIEW_SEL_URL, ps_global)
	       || F_ON(F_VIEW_SEL_URL_HOST, ps_global)
	       || F_ON(F_SCAN_ADDR, ps_global))
	      gf_link_filter(gf_line_test, gf_line_test_opt(url_hilite,
							    &handles));

	    gf_link_filter(gf_wrap,
			   gf_wrap_filter_opt(ps_global->ttyo->screen_cols - 4,
					      ps_global->ttyo->screen_cols,
					      NULL, indent, GFW_HANDLES));
	    gf_link_filter(gf_nvtnl_local, NULL);

	    gf_set_so_readc(&gc, in_store);
	    gf_set_so_writec(&pc, out_store);

	    errstr = gf_pipe(gc, pc);

	    gf_clear_so_readc(in_store);

	    if(!errstr){
#define	VCARD_TEXT_ONE	"This is a vCard which has been forwarded to you. You may add parts of it to your address book with the Save command. You will have a chance to edit it before committing it to your address book."
#define	VCARD_TEXT_MORE	"This is a vCard which has been forwarded to you. You may add the entries to your address book with the Save command."
		errstr = format_editorial((begins > 1)
					    ? VCARD_TEXT_MORE : VCARD_TEXT_ONE,
					  ps_global->ttyo->screen_cols, pc);
	    }

	    gf_clear_so_writec(out_store);

	    if(!errstr)
	      cmd = scroll_attachment("ADDRESS BOOK ATTACHMENT", out_store,
				      CharStar, handles, a, flags | DA_RESIZE);

	    free_handles(&handles);
	    so_give(&out_store);
	}
	else
	  errstr = "Error allocating space";
    }
    while(!errstr && cmd == MC_RESIZE);

    if(errstr)
      q_status_message1(SM_ORDER | SM_DING, 3, 3,
			"Can't format entry : %.200s", errstr);

    so_give(&in_store);
}



/*----------------------------------------------------------------------
  Display attachment information

  Args: msgno -- message number to get partrom
	 a -- attachment struct for the desired part

  Result: a screen containing information about attachment:
	  Type		:
	  Subtype	:
	  Parameters	:
	    Comment	:
            FileName	:
	  Encoded size	:
	  Viewer	:
 ----*/
void
display_attach_info(msgno, a)
    long      msgno;
    ATTACH_S *a;
{
    int		i;
    STORE_S    *store;
    PARMLIST_S *plist;
    SCROLL_S	sargs;

    (void) dispatch_attachment(a);

    if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Error allocating space for message.");
	return;
    }

    so_puts(store, "\nDetails about Attachment #");
    so_puts(store, a->number);
    so_puts(store, " :\n\n\n");
    so_puts(store, "\t\tType\t\t: ");
    so_puts(store, body_type_names(a->body->type));
    so_puts(store, "\n");
    so_puts(store, "\t\tSubtype\t\t: ");
    so_puts(store, a->body->subtype ? a->body->subtype : "Unknown");
    so_puts(store, "\n");
    so_puts(store, "\t\tEncoding\t: ");
    so_puts(store, a->body->encoding < ENCMAX
			 ? body_encodings[a->body->encoding]
			 : "Unknown");
    so_puts(store, "\n");
    if(plist = rfc2231_newparmlist(a->body->parameter)){
	so_puts(store, "\t\tParameters\t: ");
	i = 0;
	while(rfc2231_list_params(plist)){
	    if(i++)
	      so_puts(store, "\t\t\t\t  ");

	    so_puts(store, plist->attrib);
	    so_puts(store, " = ");
	    so_puts(store, plist->value ? plist->value : "");
	    so_puts(store, "\n");
	}

	rfc2231_free_parmlist(&plist);
    }

    if(a->body->description){
	char buftmp[MAILTMPLEN];

	sprintf(buftmp, "%.200s", a->body->description);
	so_puts(store, "\t\tDescription\t: \"");
	so_puts(store,(char *) rfc1522_decode((unsigned char *)tmp_20k_buf,
					      SIZEOF_20KBUF, buftmp, NULL));
	so_puts(store, "\"\n");
	/* BUG: Wrap description text if necessary */
    }

    /* BUG: no a->body->language support */

    if(a->body->disposition.type){
	so_puts(store, "\t\tDisposition\t: ");
	so_puts(store, a->body->disposition.type);
	if(plist = rfc2231_newparmlist(a->body->disposition.parameter)){
	    so_puts(store, "; ");
	    i = 0;
	    while(rfc2231_list_params(plist)){
		if(i++)
		  so_puts(store, ";\n\t\t\t\t  ");

		so_puts(store, plist->attrib);
		so_puts(store, " = ");
		so_puts(store, plist->value ? plist->value : "");
		so_puts(store, "\n");
	    }

	    rfc2231_free_parmlist(&plist);
	}
    }

    so_puts(store, "\t\tApprox. Size\t: ");
    so_puts(store, comatose((a->body->encoding == ENCBASE64)
			      ? ((a->body->size.bytes * 3)/4)
			      : a->body->size.bytes));
    so_puts(store, " bytes\n");
    so_puts(store, "\t\tDisplay Method\t: ");
    if(a->can_display == MCD_NONE) {
	so_puts(store, "Can't, ");
	so_puts(store, (a->body->encoding < ENCOTHER)
			 ? "Unknown Attachment Format"
			 : "Unknown Encoding");
    }
    else if(!(a->can_display & MCD_EXTERNAL)){
	so_puts(store, "Pine's Internal Pager");
    }
    else{
	int   nt, free_pretty_cmd;
	MCAP_CMD_S *mc_cmd;
	char *pretty_cmd;

	if(mc_cmd = mailcap_build_command(a->body->type, a->body->subtype,
				       a->body->parameter, "<datafile>", &nt,
				       a->can_display & MCD_EXT_PROMPT)){
	    so_puts(store, "\"");
	    if(pretty_cmd = execview_pretty_command(mc_cmd, &free_pretty_cmd))
	      so_puts(store, pretty_cmd);
	    so_puts(store, "\"");
	    if(free_pretty_cmd && pretty_cmd)
	      fs_give((void **)&pretty_cmd);
	    if(mc_cmd->command)
	      fs_give((void **)&mc_cmd->command);
	    fs_give((void **)&mc_cmd);
	}
    }

    so_puts(store, "\n");

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text  = so_text(store);
    sargs.text.src   = CharStar;
    sargs.text.desc  = "attachment info";
    sargs.bar.title  = "ABOUT ATTACHMENT";
    sargs.help.text  = h_simple_text_view;
    sargs.help.title = "HELP FOR \"ABOUT ATTACHMENT\"";

    sargs.use_indexline_color = 1;

    scrolltool(&sargs);

    so_give(&store);	/* free resources associated with store */
    ps_global->mangled_screen = 1;
}



/*----------------------------------------------------------------------

 ----*/
void
forward_attachment(stream, msgno, a)
    MAILSTREAM *stream;
    long	msgno;
    ATTACH_S   *a;
{
    char     *sig;
    void     *msgtext;
    ENVELOPE *outgoing;
    BODY     *body;

    if(MIME_MSG_A(a)){
	forward_msg_att(stream, msgno, a);
    }
    else{
	ACTION_S      *role = NULL;
	REDRAFT_POS_S *redraft_pos = NULL;
	long           rflags = ROLE_FORWARD;
	PAT_STATE      dummy;

	outgoing              = mail_newenvelope();
	outgoing->message_id  = generate_message_id();
	outgoing->subject     = cpystr("Forwarded attachment...");

	if(nonempty_patterns(rflags, &dummy)){
	    /*
	     * There is no message, but a Current Folder Type might match.
	     *
	     * This has been changed to check against the message 
	     * containing the attachment.
	     */
	    role = set_role_from_msg(ps_global, ROLE_FORWARD, msgno, NULL);
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{
		role = NULL;
		cmd_cancelled("Forward");
		mail_free_envelope(&outgoing);
		return;
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4,
			    "Forwarding using role \"%.200s\"", role->nick);

	/*
	 * as with all text bound for the composer, build it in 
	 * a storage object of the type it understands...
	 */
	if(msgtext = (void *) so_get(PicoText, NULL, EDIT_ACCESS)){
	    int   impl, template_len = 0;

	    if(role && role->template){
		char *filtered;

		impl = 1;
		filtered = detoken(role, NULL, 0, 0, 0, &redraft_pos, &impl);
		if(filtered){
		    if(*filtered){
			so_puts((STORE_S *)msgtext, filtered);
			if(impl == 1)
			  template_len = strlen(filtered);
		    }
		    
		    fs_give((void **)&filtered);
		}
	    }
	    else
	      impl = 1;

	    if(sig = detoken(role, NULL, 2, 0, 1, &redraft_pos, &impl)){
		if(impl == 2)
		  redraft_pos->offset += template_len;

		so_puts((STORE_S *)msgtext, *sig ? sig : NEWLINE);

		fs_give((void **)&sig);
	    }
	    else
	      so_puts((STORE_S *)msgtext, NEWLINE);

	    /*---- New Body to start with ----*/
	    body       = mail_newbody();
	    body->type = TYPEMULTIPART;

	    /*---- The TEXT part/body ----*/
	    body->nested.part			       = mail_newbody_part();
	    body->nested.part->body.type	       = TYPETEXT;
	    body->nested.part->body.contents.text.data = msgtext;

	    /*---- The corresponding things we're attaching ----*/
	    body->nested.part->next  = mail_newbody_part();
	    body->nested.part->next->body.id = generate_message_id();
	    copy_body(&body->nested.part->next->body, a->body);

	    if(fetch_contents(stream, msgno, a->number,
			      &body->nested.part->next->body)){
		pine_send(outgoing, &body, "FORWARD MESSAGE",
			  role, NULL, NULL, redraft_pos, NULL, NULL, FALSE);

		ps_global->mangled_screen = 1;
		pine_free_body(&body);
		free_redraft_pos(&redraft_pos);

#if	defined(DOS) && !defined(WIN32)
		mail_parameters(ps->mail_stream, SET_GETS, (void *)NULL);
		append_file = NULL;
		mail_gc(ps->mail_stream, GC_TEXTS);
#endif
	    }
	    else{
		mail_free_body(&body);
		so_give((STORE_S **) &msgtext);
		free_redraft_pos(&redraft_pos);
		q_status_message(SM_ORDER | SM_DING, 4, 5,
		   "Error fetching message contents.  Can't forward message.");
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
			   "Error allocating message text");

	mail_free_envelope(&outgoing);
	free_action(&role);
    }
}


/*----------------------------------------------------------------------

 ----*/
void
forward_msg_att(stream, msgno, a)
    MAILSTREAM *stream;
    long	msgno;
    ATTACH_S   *a;
{
    char          *p, *sig = NULL;
    int            ret;
    void          *msgtext;
    ENVELOPE      *outgoing;
    BODY          *body;
    ACTION_S      *role = NULL;
    REPLY_S        reply;
    REDRAFT_POS_S *redraft_pos = NULL;

    outgoing             = mail_newenvelope();
    outgoing->message_id = generate_message_id();

    memset((void *)&reply, 0, sizeof(reply));

    if(outgoing->subject = forward_subject(a->body->nested.msg->env, 0)){
	/*
	 * as with all text bound for the composer, build it in 
	 * a storage object of the type it understands...
	 */
	if(msgtext = (void *) so_get(PicoText, NULL, EDIT_ACCESS)){
	    int impl, template_len = 0;
	    long rflags = ROLE_FORWARD;
	    PAT_STATE dummy;

	    ret = 'n';
	    if(ps_global->full_header == 2)
	       ret = want_to("Forward message as an attachment", 'n', 0,
			     NO_HELP, WT_SEQ_SENSITIVE);
	    /* Setup possible role */
	    if(nonempty_patterns(rflags, &dummy)){
		role = set_role_from_msg(ps_global, rflags, msgno, a->number);
		if(confirm_role(rflags, &role))
		  role = combine_inherited_role(role);
		else{				/* cancel reply */
		    role = NULL;
		    cmd_cancelled("Forward");
		    mail_free_envelope(&outgoing);
		    so_give((STORE_S **) &msgtext);
		    return;
		}
	    }

	    if(role)
	      q_status_message1(SM_ORDER, 3, 4,
				"Forwarding using role \"%.200s\"", role->nick);

	    if(role && role->template){
		char *filtered;

		impl = 1;
		filtered = detoken(role, a->body->nested.msg->env,
				   0, 0, 0, &redraft_pos, &impl);
		if(filtered){
		    if(*filtered){
			so_puts((STORE_S *)msgtext, filtered);
			if(impl == 1)
			  template_len = strlen(filtered);
		    }
		    
		    fs_give((void **)&filtered);
		}
	    }
	    else
	      impl = 1;

	    if(sig = detoken(role, a->body->nested.msg->env,
			     2, 0, 1, &redraft_pos, &impl)){
		if(impl == 2)
		  redraft_pos->offset += template_len;

		so_puts((STORE_S *)msgtext, *sig ? sig : NEWLINE);

		fs_give((void **)&sig);
	    }
	    else
	      so_puts((STORE_S *)msgtext, NEWLINE);

	    if(ret == 'y'){
		/*---- New Body to start with ----*/
		body	   = mail_newbody();
		body->type = TYPEMULTIPART;

		/*---- The TEXT part/body ----*/
		body->nested.part = mail_newbody_part();
		body->nested.part->body.type = TYPETEXT;
		body->nested.part->body.contents.text.data = msgtext;

		if(!forward_mime_msg(stream, msgno,
				     p = body_partno(stream, msgno, a->body),
				     a->body->nested.msg->env,
				     &body->nested.part->next, msgtext))
		  mail_free_body(&body);
	    }
	    else{
		reply.flags = REPLY_FORW;
		if(a->body->nested.msg->body){
		    char *charset;

		    charset
		      = rfc2231_get_param(a->body->nested.msg->body->parameter,
				          "charset", NULL, NULL);
		    
		    if(charset && strucmp(charset, "us-ascii") != 0){
			CONV_TABLE *ct;

			/*
			 * There is a non-ascii charset,
			 * is there conversion happening?
			 */
			if(F_ON(F_DISABLE_CHARSET_CONVERSIONS, ps_global)
			   || !(ct=conversion_table(charset,
					            ps_global->VAR_CHAR_SET))
			   || !ct->table){
			    reply.orig_charset = charset;
			    charset = NULL;
			}
		    }

		    if(charset)
		      fs_give((void **) &charset);
		}

		body = forward_body(stream, a->body->nested.msg->env,
				    a->body->nested.msg->body, msgno,
				    p = body_partno(stream, msgno, a->body),
				    msgtext, FWD_NONE);
	    }

	    fs_give((void **) &p);

	    if(body){
		pine_send(outgoing, &body,
			  "FORWARD MESSAGE",
			  role, NULL,
			  reply.flags ? &reply : NULL,
			  redraft_pos, NULL, NULL, FALSE);

		ps_global->mangled_screen = 1;
		pine_free_body(&body);
		free_redraft_pos(&redraft_pos);
		free_action(&role);

#if	defined(DOS) && !defined(WIN32)
		mail_parameters(ps->mail_stream, SET_GETS, (void *)NULL);
		append_file = NULL;
		mail_gc(ps->mail_stream, GC_TEXTS);
#endif
	    }
	    else{
		so_give((STORE_S **) &msgtext);
		q_status_message(SM_ORDER | SM_DING, 4, 5,
		   "Error fetching message contents.  Can't forward message.");
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
			   "Error allocating message text");
    }
    else
      q_status_message1(SM_ORDER,3,4,
			"Error fetching message %.200s. Can't forward it.",
			long2string(msgno));

    if(reply.orig_charset)
      fs_give((void **)&reply.orig_charset);

    mail_free_envelope(&outgoing);
}


/*----------------------------------------------------------------------

 ----*/
void
reply_msg_att(stream, msgno, a)
    MAILSTREAM *stream;
    long	msgno;
    ATTACH_S   *a;
{
    ADDRESS       *saved_from, *saved_to, *saved_cc, *saved_resent;
    ENVELOPE      *outgoing;
    BODY          *body;
    void          *msgtext;
    char          *tp, *prefix = NULL;
    int            include_text = 0, flags = RSF_QUERY_REPLY_ALL;
    long           rflags;
    PAT_STATE      dummy;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S      *role = NULL;
    BUILDER_ARG    fcc;

    memset(&fcc, 0, sizeof(BUILDER_ARG));
    outgoing = mail_newenvelope();

    dprint(4, (debugfile,"\n - attachment reply \n"));

    saved_from		  = (ADDRESS *) NULL;
    saved_to		  = (ADDRESS *) NULL;
    saved_cc		  = (ADDRESS *) NULL;
    saved_resent	  = (ADDRESS *) NULL;
    outgoing->subject	  = NULL;

    prefix = reply_quote_str(a->body->nested.msg->env);
    /*
     * For consistency, the first question is always "include text?"
     */
    if((include_text = reply_text_query(ps_global, 1, &prefix)) >= 0
       && reply_news_test(a->body->nested.msg->env, outgoing) > 0
       && reply_harvest(ps_global, msgno, a->number,
			a->body->nested.msg->env, &saved_from,
			&saved_to, &saved_cc, &saved_resent, &flags)){
	outgoing->subject = reply_subject(a->body->nested.msg->env->subject,
					  NULL, 0);
	reply_seed(ps_global, outgoing, a->body->nested.msg->env,
		   saved_from, saved_to, saved_cc, saved_resent,
		   &fcc, flags & RSF_FORCE_REPLY_ALL);

	if(sp_expunge_count(stream))	/* current msg was expunged */
	  goto seeyalater;

	/* Setup possible role */
	rflags = ROLE_REPLY;
	if(nonempty_patterns(rflags, &dummy)){
	    role = set_role_from_msg(ps_global, rflags, msgno, a->number);
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{				/* cancel reply */
		role = NULL;
		cmd_cancelled("Reply");
		goto seeyalater;
	    }
	}

	if(role){
	    q_status_message1(SM_ORDER, 3, 4,
			      "Replying using role \"%.200s\"", role->nick);

	    /* override fcc gotten in reply_seed */
	    if(role->fcc){
		if(fcc.tptr)
		  fs_give((void **)&fcc.tptr);

		memset((void *)&fcc, 0, sizeof(BUILDER_ARG));
	    }
	}

	outgoing->in_reply_to = reply_in_reply_to(a->body->nested.msg->env);
	outgoing->references = reply_build_refs(a->body->nested.msg->env);
	outgoing->message_id = generate_message_id();

	if(!outgoing->to && !outgoing->cc
	   && !outgoing->bcc && !outgoing->newsgroups)
	  q_status_message(SM_ORDER | SM_DING, 3, 6,
			   "Warning: no valid addresses to reply to!");

	/*
	 * Now fix up the body...
	 */
	if(msgtext = (void *) so_get(PicoText, NULL, EDIT_ACCESS)){
	    REPLY_S reply;

	    memset((void *)&reply, 0, sizeof(reply));
	    reply.flags = REPLY_FORW;
	    if(a->body->nested.msg->body){
		char *charset;

		charset
		  = rfc2231_get_param(a->body->nested.msg->body->parameter,
				      "charset", NULL, NULL);
		
		if(charset && strucmp(charset, "us-ascii") != 0){
		    CONV_TABLE *ct;

		    /*
		     * There is a non-ascii charset,
		     * is there conversion happening?
		     */
		    if(F_ON(F_DISABLE_CHARSET_CONVERSIONS, ps_global)
		       || !(ct=conversion_table(charset,
						ps_global->VAR_CHAR_SET))
		       || !ct->table){
			reply.orig_charset = charset;
			charset = NULL;
		    }
		}

		if(charset)
		  fs_give((void **) &charset);
	    }

	    if(body = reply_body(stream, a->body->nested.msg->env,
				 a->body->nested.msg->body, msgno,
				 tp = body_partno(stream, msgno, a->body),
				 msgtext, prefix, include_text, role,
				 1, &redraft_pos)){
		/* partially formatted outgoing message */
		pine_send(outgoing, &body, "COMPOSE MESSAGE REPLY",
			  role, fcc.tptr, &reply, redraft_pos, NULL, NULL, 0);

		pine_free_body(&body);
		ps_global->mangled_screen = 1;
	    }
	    else
	      q_status_message(SM_ORDER | SM_DING, 3, 4,
			       "Error building message body");

	    fs_give((void **) &tp);
	    if(reply.orig_charset)
	      fs_give((void **)&reply.orig_charset);
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
			   "Error allocating message text");
    }

seeyalater:
    mail_free_envelope(&outgoing);
    mail_free_address(&saved_from);
    mail_free_address(&saved_to);
    mail_free_address(&saved_cc);
    mail_free_address(&saved_resent);

    if(prefix)
      fs_give((void **) &prefix);

    if(fcc.tptr)
      fs_give((void **) &fcc.tptr);
    
    free_redraft_pos(&redraft_pos);
    free_action(&role);
}


/*----------------------------------------------------------------------

 ----*/
void
bounce_msg_att(stream, msgno, part, subject)
    MAILSTREAM *stream;
    long	msgno;
    char       *part, *subject;
{
    char *errstr;

    if(errstr = bounce_msg(stream, msgno, part, NULL, NULL, subject, NULL, NULL))
      q_status_message(SM_ORDER | SM_DING, 3, 3, errstr);
}


/*----------------------------------------------------------------------

  ----*/        
void
pipe_attachment(msgno, a)
     long      msgno;
     ATTACH_S *a;
{
    char    *err, *resultfilename = NULL, prompt[80];
    int      rc, capture = 1, raw = 0, we_cancel = 0;
    PIPE_S  *syspipe;
    HelpType help;
    char     pipe_command[MAXPATH+1];
    static ESCKEY_S pipe_opt[] = {
	{0, 0, "", ""},
	{ctrl('W'), 10, "^W", NULL},
	{ctrl('Y'), 11, "^Y", NULL},
	{-1, 0, NULL, NULL}
    };
    
    if(ps_global->restricted){
	q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Pine demo can't pipe attachments");
	return;
    }

    help = NO_HELP;
    pipe_command[0] = '\0';
    while(1){
	int flags;

	sprintf(prompt, "Pipe %sattachment %.20s to %s: ", raw ? "RAW " : "",
		a->number, capture ? "" : "(Free Output) ");
	pipe_opt[1].label = raw ? "DecodedData" : "Raw Data";
	pipe_opt[2].label = capture ? "Free Output" : "Capture Output";
	flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE;
	rc = optionally_enter(pipe_command, -FOOTER_ROWS(ps_global), 0,
			      sizeof(pipe_command), prompt,
			      pipe_opt, help, &flags);
	if(rc == -1){
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Internal problem encountered");
	    break;
	}
	else if(rc == 10){
	    raw = !raw;			/* flip raw text */
	}
	else if(rc == 11){
	    capture = !capture;		/* flip capture output */
	}
	else if(rc == 0){
	    if(pipe_command[0] == '\0'){
		cmd_cancelled("Pipe command");
		break;
	    }

	    flags = PIPE_USER | PIPE_WRITE | PIPE_STDERR;
	    if(!capture){
#ifndef	_WINDOWS
		ClearScreen();
		fflush(stdout);
		clear_cursor_pos();
		ps_global->mangled_screen = 1;
#endif
		flags |= PIPE_RESET;
	    }

	    if(syspipe = open_system_pipe(pipe_command,
				   (flags&PIPE_RESET) ? NULL : &resultfilename,
				   NULL, flags, 0)){
		gf_io_t  pc;		/* wire up a generic putchar */
		gf_set_writec(&pc, syspipe, 0L, PipeStar);

		/*------ Write the image to a temporary file ------*/
		if(raw){		/* pipe raw text */
		    FETCH_READC_S fetch_part;

		    err = NULL;

		    if(capture)
		      we_cancel = busy_alarm(1, NULL, NULL, 0);
		    else
		      suspend_busy_alarm();

		    gf_filter_init();
		    fetch_readc_init(&fetch_part, ps_global->mail_stream,
				     msgno, a->number, a->body->size.bytes, 0);
		    gf_link_filter(gf_nvtnl_local, NULL);
		    err = gf_pipe(FETCH_READC, pc);

		    if(capture){
			if(we_cancel)
			  cancel_busy_alarm(0);
		    }
		    else
		      resume_busy_alarm(0);
		}
		else{
		    /* BUG: there's got to be a better way */
		    if(!capture)
		      ps_global->print = (PRINT_S *) 1;

		    suspend_busy_alarm();
		    err = detach(ps_global->mail_stream, msgno,
				 a->number, (long *)NULL, pc, NULL, 0);
		    ps_global->print = (PRINT_S *) NULL;
		    resume_busy_alarm(0);
		}

		(void) close_system_pipe(&syspipe, NULL, 0);

		if(err)
		  q_status_message1(SM_ORDER | SM_DING, 3, 4,
				    "Error detaching for pipe: %.200s", err);

		display_output_file(resultfilename,
				    (err)
				      ? "PIPE ATTACHMENT (ERROR)"
				      : "PIPE ATTACHMENT",
				    NULL, DOF_EMPTY);

		fs_give((void **) &resultfilename);
	    }
	    else
	      q_status_message(SM_ORDER | SM_DING, 3, 4,
			       "Error opening pipe");

	    break;
	}
	else if(rc == 1){
	    cmd_cancelled("Pipe");
	    break;
	}
	else if(rc == 3)
	  help = (help == NO_HELP) ? h_pipe_attach : NO_HELP;
    }
}


int
delete_attachment(msgno, a)
    long      msgno;
    ATTACH_S *a;
{
    int expbits, rv = 0;

    if(!msgno_exceptions(ps_global->mail_stream, msgno,
			 a->number, &expbits, FALSE)
       || !(expbits & MSG_EX_DELETE)){
	expbits |= MSG_EX_DELETE;
	msgno_exceptions(ps_global->mail_stream, msgno,
			 a->number, &expbits, TRUE);

	q_status_message1(SM_ORDER, 0, 3,
			"Part %.200s will be omitted only if message is Saved",
			  a->number);
	rv = 1;
    }
    else
      q_status_message1(SM_ORDER, 0, 3, "Part %.200s already deleted",
			a->number);

    return(rv);
}



int
undelete_attachment(msgno, a, expbitsp)
    long      msgno;
    ATTACH_S *a;
    int	     *expbitsp;
{
    int rv = 0;

    if(msgno_exceptions(ps_global->mail_stream, msgno,
			a->number, expbitsp, FALSE)
       && ((*expbitsp) & MSG_EX_DELETE)){
	(*expbitsp) ^= MSG_EX_DELETE;
	msgno_exceptions(ps_global->mail_stream, msgno,
			 a->number, expbitsp, TRUE);
	rv = 1;
    }
    else
      q_status_message1(SM_ORDER, 0, 3, "Part %.200s already UNdeleted",
			a->number);

    return(rv);
}



/*----------------------------------------------------------------------
  Resolve any deferred tests for attachment displayability

  Args: attachment structure

  Returns: undefer's attachment's displayability test
  ----*/
int
dispatch_attachment(a)
    ATTACH_S *a;
{
    if(a->test_deferred){
	a->test_deferred = 0;
	a->can_display = mime_can_display(a->body->type, a->body->subtype,
					  a->body->parameter);
    }

    return(a->can_display);
}


/*----------------------------------------------------------------------
  detach the given raw body part; don't do any decoding

  Args: a bunch

  Returns: NULL on success, error message otherwise
  ----*/
char *
detach_raw(stream, msg_no, part_no, pc, flags)
     MAILSTREAM *stream;		/* c-client stream to use         */
     long        msg_no;		/* message number to deal with	  */
     char       *part_no;		/* part number of message 	  */
     gf_io_t	 pc;			/* where to put it		  */
{
    FETCH_READC_S  *frd = (FETCH_READC_S *)fs_get(sizeof(FETCH_READC_S));
    char *err = NULL;
    int column = (flags & FM_DISPLAY) ? ps_global->ttyo->screen_cols : 80;
    
    memset(g_fr_desc = frd, 0, sizeof(FETCH_READC_S));
    frd->stream  = stream;
    frd->msgno   = msg_no;
    frd->section = part_no;
    frd->size    = 0;		        /* wouldn't be here otherwise     */
    frd->readc	 = fetch_readc;
    frd->chunk   = pine_mail_fetch_text(stream, msg_no, NULL, &frd->read, 0);
    frd->endp    = &frd->chunk[frd->read];
    frd->chunkp  = frd->chunk;

    gf_filter_init();
    if (!(flags & FM_NOWRAP))
      gf_link_filter(gf_wrap, gf_wrap_filter_opt(column, column, NULL, 0,
						 (flags & FM_DISPLAY)
						   ? GFW_HANDLES : 0));
    err = gf_pipe(FETCH_READC, pc);

    return(err);
}


/*----------------------------------------------------------------------
  detach the given body part using the given encoding

  Args: a bunch

  Returns: NULL on success, error message otherwise
  ----*/
char *
detach(stream, msg_no, part_no, len, pc, aux_filters, flags)
     MAILSTREAM *stream;		/* c-client stream to use         */
     long        msg_no;		/* message number to deal with	  */
     char       *part_no;		/* part number of message 	  */
     long       *len;			/* returns bytes read in this arg */
     gf_io_t	 pc;			/* where to put it		  */
     FILTLIST_S *aux_filters;		/* null terminated array of filts */
     int         flags;		/* not used anymore */
{
    unsigned long  rv;
    int            we_cancel = 0;
    char          *status, trigger[MAILTMPLEN];
    BODY	  *body;
    SourceType     src = CharStar;
    static char    err_string[100];
    FETCH_READC_S  fetch_part;

    err_string[0] = '\0';

    if(!ps_global->print && !pc_is_picotext(pc))
      we_cancel = busy_alarm(1, NULL, NULL, 0);

    gf_filter_init();			/* prepare for filtering! */

    if(!(body = mail_body(stream, msg_no, (unsigned char *) part_no)))
      return("Can't find body for requested message");

    fetch_readc_init(&fetch_part, stream, msg_no, part_no, body->size.bytes,0);
    rv = body->size.bytes ? body->size.bytes : 1;

    switch(body->encoding) {		/* handle decoding */
      case ENC7BIT:
      case ENC8BIT:
      case ENCBINARY:
        break;

      case ENCBASE64:
	gf_link_filter(gf_b64_binary, NULL);
        break;

      case ENCQUOTEDPRINTABLE:
	gf_link_filter(gf_qp_8bit, NULL);
        break;

      case ENCOTHER:
      default:
	dprint(1, (debugfile, "detach: unknown CTE: \"%s\" (%d)\n",
		   (body->encoding <= ENCMAX
		    && body_encodings[body->encoding])
		       ? body_encodings[body->encoding]
		       : "BEYOND-KNOWN-TYPES",
		   body->encoding));
	break;
    }

    /*
     * If we're detaching a text segment and there are user-defined
     * filters and there are text triggers to look for, install filter
     * to let us look at each line...
     */
    display_filter = NULL;
    if(body->type == TYPETEXT && ps_global->VAR_DISPLAY_FILTERS){
	/* check for "static" triggers (i.e., none or CHARSET) */
	if(!(display_filter = df_static_trigger(body, trigger))
	   && (df_trigger_list = build_trigger_list())){
	    /* else look for matching text trigger */
	    gf_link_filter(gf_line_test,
			   gf_line_test_opt(df_trigger_cmp, NULL));
	}
    }
    else
      /* add aux filters if we're not going to MIME decode into a temporary
       * storage object, otherwise we pass the aux_filters on to gf_filter
       * below so it can pass what comes out of the external filter command
       * thru the rest of the filters...
       */
      for( ; aux_filters && aux_filters->filter; aux_filters++)
	gf_link_filter(aux_filters->filter, aux_filters->data);

    /*
     * Following canonical model, after decoding convert newlines from
     * crlf to local convention.  ALSO, convert newlines if we're fetching
     * a multipart segment since an external handler's going to have to
     * make sense of it...
     */
    if(body->type == TYPETEXT
       || body->type == TYPEMESSAGE
       || body->type == TYPEMULTIPART){
	gf_link_filter(gf_nvtnl_local, NULL);
    }

    /*
     * If we're detaching a text segment and a user-defined filter may
     * need to be invoked later (see below), decode the segment into
     * a temporary storage object...
     */
    if(body->type == TYPETEXT && ps_global->VAR_DISPLAY_FILTERS
       && !(detach_so = so_get(src, NULL, EDIT_ACCESS)))
      strcpy(err_string,
	   "Formatting error: no space to make copy, no display filters used");

    if(status = gf_pipe(FETCH_READC, detach_so ? detach_writec : pc)) {
	sprintf(err_string, "Formatting error: %s", status);
        rv = 0L;
    }

    /*
     * If we wrote to a temporary area, there MAY be a user-defined
     * filter to invoke.  Filter it it (or not if no trigger match)
     * *AND* send the output thru any auxiliary filters, destroy the
     * temporary object and be done with it...
     */
    if(detach_so){
	if(!err_string[0] && display_filter && *display_filter){
	    FILTLIST_S *p, *aux = NULL;
	    size_t	count;

	    if(aux_filters){
		/* insert NL conversion filters around remaining aux_filters
		 * so they're not tripped up by local NL convention
		 */
		for(p = aux_filters; p->filter; p++)	/* count aux_filters */
		  ;

		count = (p - aux_filters) + 3;
		p = aux = (FILTLIST_S *) fs_get(count * sizeof(FILTLIST_S));
		memset(p, 0, count * sizeof(FILTLIST_S));
		p->filter = gf_local_nvtnl;
		p++;
		for(; aux_filters->filter; p++, aux_filters++)
		  *p = *aux_filters;

		p->filter = gf_nvtnl_local;
	    }

	    if(status = dfilter(display_filter, detach_so, pc, aux)){
		sprintf(err_string, "Formatting error: %s", status);
		rv = 0L;
	    }

	    if(aux)
	      fs_give((void **)&aux);
	}
	else{					/*  just copy it, then */
	    gf_io_t	   gc;

	    gf_set_so_readc(&gc, detach_so);
	    so_seek(detach_so, 0L, 0);
	    gf_filter_init();
	    if(aux_filters){
		/* if other filters are involved, correct for
		 * newlines on either side of the pipe...
		 */
		gf_link_filter(gf_local_nvtnl, NULL);
		for( ; aux_filters->filter ; aux_filters++)
		  gf_link_filter(aux_filters->filter, aux_filters->data);

		gf_link_filter(gf_nvtnl_local, NULL);
	    }

	    if(status = gf_pipe(gc, pc)){	/* Second pass, sheesh */
		sprintf(err_string, "Formatting error: %s", status);
		rv = 0L;
	    }

	    gf_clear_so_readc(detach_so);
	}

	so_give(&detach_so);			/* blast temp copy */
    }

    if(!ps_global->print && we_cancel)
      cancel_busy_alarm(0);

    if (len)
      *len = rv;

    if(0 && display_filter)
      fs_give((void **) &display_filter);

    if(df_trigger_list)
      blast_trigger_list(&df_trigger_list);

    return((err_string[0] == '\0') ? NULL : err_string);
}


int
detach_writec(c)
    int c;
{
    return(so_writec(c, detach_so));
}


/*
 * df_static_trigger - look thru the display filter list and set the
 *		       filter to any triggers that don't require scanning
 *		       the message segment.
 */
char *
df_static_trigger(body, cmdbuf)
    BODY *body;
    char *cmdbuf;
{
    int    passed = 0;
    char **l, *test, *cmd;

    for(l = ps_global->VAR_DISPLAY_FILTERS ; l && *l && !passed; l++){

	get_pair(*l, &test, &cmd, 1, 1);

	dprint(5, (debugfile, "static trigger: \"%s\" --> \"%s\" and \"%s\"",
		   (l && *l) ? *l : "?",
		   test ? test : "<NULL>", cmd ? cmd : "<NULL>"));

	if(passed = (df_valid_test(body, test) && valid_filter_command(&cmd)))
	  strcpy(cmdbuf, cmd);

	fs_give((void **) &test);
	fs_give((void **) &cmd);
    }

    return(passed ? cmdbuf : NULL);
}


df_valid_test(body, test)
    BODY *body;
    char *test;
{
    int passed = 0;

    if(!(passed = !test)){			/* NO test always wins */
	if(!*test){
	    passed++;				/* NULL test also wins! */
	}
	else if(body && !struncmp(test, "_CHARSET(", 9)){
	    char *p = strrindex(test, ')');

	    if(p){
		*p = '\0';			/* tie off user charset */
		if(p = rfc2231_get_param(body->parameter,"charset",NULL,NULL)){
		    passed = !strucmp(test + 9, p);
		    fs_give((void **) &p);
		}
		else
		  passed = !strucmp(test + 9, "us-ascii");
	    }
	    else
	      dprint(1, (debugfile,
			 "filter trigger: malformed test: %s\n",
			 test ? test : "?"));
	}
    }

    return(passed);
}



/*
 * build_trigger_list - return possible triggers in a list of triggers 
 *			structs
 */
TRGR_S *
build_trigger_list()
{
    TRGR_S  *tp = NULL, **trailp;
    char   **l, *test, *str, *ep, *cmd = NULL;
    int	     i;

    trailp = &tp;
    for(l = ps_global->VAR_DISPLAY_FILTERS ; l && *l; l++){
	get_pair(*l, &test, &cmd, 1, 1);
	if(test && valid_filter_command(&cmd)){
	    *trailp	  = (TRGR_S *) fs_get(sizeof(TRGR_S));
	    (*trailp)->cmp = df_trigger_cmp_text;
	    str		   = test;
	    if(*test == '_' && (i = strlen(test)) > 10
	       && *(ep = &test[i-1]) == '_' && *--ep == ')'){
		if(struncmp(test, "_CHARSET(", 9) == 0){
		    fs_give((void **)&test);
		    fs_give((void **)&cmd);
		    fs_give((void **)trailp);
		    continue;
		}

		if(strncmp(test+1, "LEADING(", 8) == 0){
		    (*trailp)->cmp = df_trigger_cmp_lwsp;
		    *ep		   = '\0';
		    str		   = cpystr(test+9);
		    fs_give((void **)&test);
		}
		else if(strncmp(test+1, "BEGINNING(", 10) == 0){
		    (*trailp)->cmp = df_trigger_cmp_start;
		    *ep		   = '\0';
		    str		   = cpystr(test+11);
		    fs_give((void **)&test);
		}
	    }

	    (*trailp)->text = str;
	    (*trailp)->cmd = cmd;
	    *(trailp = &(*trailp)->next) = NULL;
	}
	else{
	    fs_give((void **)&test);
	    fs_give((void **)&cmd);
	}
    }

    return(tp);
}


/*
 * blast_trigger_list - zot any list of triggers we've been using 
 */
void
blast_trigger_list(tlist)
    TRGR_S **tlist;
{
    if((*tlist)->next)
      blast_trigger_list(&(*tlist)->next);

    fs_give((void **)&(*tlist)->text);
    fs_give((void **)&(*tlist)->cmd);
    fs_give((void **)tlist);
}


/*
 * df_trigger_cmp - compare the line passed us with the list of defined
 *		    display filter triggers
 */
int
df_trigger_cmp(n, s, e, l)
    long       n;
    char      *s;
    LT_INS_S **e;
    void      *l;
{
    register TRGR_S *tp;
    int		     result;

    if(!display_filter)				/* already found? */
      for(tp = df_trigger_list; tp; tp = tp->next)
	if(tp->cmp){
	    if((result = (*tp->cmp)(s, tp->text)) < 0)
	      tp->cmp = NULL;
	    else if(result > 0)
	      return(((display_filter = tp->cmd) != NULL) ? 1 : 0);
	}

    return(0);
}


/*
 * df_trigger_cmp_text - return 1 if s1 is in s2
 */
int
df_trigger_cmp_text(s1, s2)
    char *s1, *s2;
{
    return(strstr(s1, s2) != NULL);
}


/*
 * df_trigger_cmp_lwsp - compare the line passed us with the list of defined
 *		         display filter triggers. returns:
 *
 *		    0  if we don't know yet
 *		    1  if we match
 *		   -1  if we clearly don't match
 */
int
df_trigger_cmp_lwsp(s1, s2)
    char *s1, *s2;
{
    while(*s1 && isspace((unsigned char)*s1))
      s1++;

    return((*s1) ? (!strncmp(s1, s2, strlen(s2)) ? 1 : -1) : 0);
}


/*
 * df_trigger_cmp_start - return 1 if first strlen(s2) chars start s1
 */
int
df_trigger_cmp_start(s1, s2)
    char *s1, *s2;
{
    return(!strncmp(s1, s2, strlen(s2)));
}


/*
 * valid_filter_command - make sure argv[0] of command really exists.
 *		      "cmd" is required to be an alloc'd string since
 *		      it will get realloc'd if the command's path is
 *		      expanded.
 */
int
valid_filter_command(cmd)
    char **cmd;
{
    int  i;
    char cpath[MAXPATH+1], *p;

    if(!(cmd && *cmd))
      return(FALSE);

    /*
     * copy cmd to build expanded path if necessary.
     */
    for(i = 0; i < sizeof(cpath) && (cpath[i] = (*cmd)[i]); i++)
      if(isspace((unsigned char)(*cmd)[i])){
	  cpath[i] = '\0';		/* tie off command's path*/
	  break;
      }

#if	defined(DOS) || defined(OS2)
    if(is_absolute_path(cpath)){
	fixpath(cpath, sizeof(cpath));
	p = (char *) fs_get(strlen(cpath) + strlen(&(*cmd)[i]) + 1);
	strcpy(p, cpath);		/* copy new path */
	strcat(p, &(*cmd)[i]);		/* and old args */
	fs_give((void **) cmd);		/* free it */
	*cmd = p;			/* and assign new buf */
    }
#else
    if(cpath[0] == '~'){
	if(fnexpand(cpath, sizeof(cpath))){
	    p = (char *) fs_get(strlen(cpath) + strlen(&(*cmd)[i]) + 1);
	    strcpy(p, cpath);		/* copy new path */
	    strcat(p, &(*cmd)[i]);	/* and old args */
	    fs_give((void **) cmd);	/* free it */
	    *cmd = p;			/* and assign new buf */
	}
	else
	  return(FALSE);
    }
#endif

    return(is_absolute_path(cpath) && can_access(cpath, EXECUTE_ACCESS) == 0);
}



/*
 * dfilter - pipe the data from the given storage object thru the
 *	     global display filter and into whatever the putchar's
 *	     function points to.
 */
char *
dfilter(rawcmd, input_so, output_pc, aux_filters)
    char       *rawcmd;
    STORE_S    *input_so;
    gf_io_t	output_pc;
    FILTLIST_S *aux_filters;
{
    char *status = NULL, *cmd, *resultf = NULL, *tmpfile = NULL;
    int   key = 0;

    if(cmd=expand_filter_tokens(rawcmd,NULL,&tmpfile,&resultf,NULL,&key,NULL)){
	suspend_busy_alarm();
#ifndef	_WINDOWS
	ps_global->mangled_screen = 1;
	ClearScreen();
	fflush(stdout);
#endif

	/*
	 * If it was requested that the interaction take place via
	 * a tmpfile, fill it with text from our input_so, and let
	 * system_pipe handle the rest.  Session key and tmp file
	 * mode support additions based loosely on a patch 
	 * supplied by Thomas Stroesslin <thomas.stroesslin@epfl.ch>
	 */
	if(tmpfile){
	    PIPE_S	  *filter_pipe;
	    FILE          *fp;
	    gf_io_t	   gc, pc;
	    STORE_S       *tmpf_so;

	    /* write the tmp file */
	    so_seek(input_so, 0L, 0);
	    if(tmpf_so = so_get(FileStar, tmpfile, EDIT_ACCESS|OWNER_ONLY)){
	        if(key){
		    so_puts(tmpf_so, filter_session_key());
                    so_puts(tmpf_so, NEWLINE);
		}
	        /* copy input to tmp file */
		gf_set_so_readc(&gc, input_so);
		gf_set_so_writec(&pc, tmpf_so);
		gf_filter_init();
		status = gf_pipe(gc, pc);
		gf_clear_so_readc(input_so);
		gf_clear_so_writec(tmpf_so);
		if(so_give(&tmpf_so) != 0 && status == NULL)
		  status = error_description(errno);

		/* prepare the terminal in case the filter uses it */
		if(status == NULL){
		    if(filter_pipe = open_system_pipe(cmd, NULL, NULL,
						      PIPE_USER | PIPE_RESET,
						      0)){
			if (close_system_pipe(&filter_pipe, NULL, 0) == 0){
			    /* pull result out of tmp file */
			    if(fp = fopen(tmpfile, READ_MODE)){
				gf_set_readc(&gc, fp, 0L, FileStar);
				gf_filter_init();
				if(aux_filters)
				  for( ; aux_filters->filter; aux_filters++)
				    gf_link_filter(aux_filters->filter,
						   aux_filters->data);

				status = gf_pipe(gc, output_pc);
				fclose(fp);
			    }
			    else
			      status = "Can't read result of display filter";
			}
			else
			  status = "Filter command returned error.";
		    }
		    else
		      status = "Can't open pipe for display filter";
		}

		unlink(tmpfile);
	    }
	    else
	      status = "Can't open display filter tmp file";
	}
	else if(status = gf_filter(cmd, key ? filter_session_key() : NULL,
				   input_so, output_pc, aux_filters,
				   F_ON(F_DISABLE_TERM_RESET_DISP, ps_global))){
	    int ch;

	    fprintf(stdout,"\r\n%s  Hit return to continue.", status);
	    fflush(stdout);
	    while((ch = read_char(300)) != ctrl('M') && ch != NO_OP_IDLE)
	      putchar(BELL);
	}

	if(resultf){
	    if(name_file_size(resultf) > 0L)
	      display_output_file(resultf, "Filter", NULL, DOF_BRIEF);

	    fs_give((void **)&resultf);
	}

	resume_busy_alarm(0);
#ifndef	_WINDOWS
	ClearScreen();
#endif
	fs_give((void **)&cmd);
    }

    return(status);
}


/*
 * expand_filter_tokens - return an alloc'd string with any special tokens
 *			  in the given filter expanded, NULL otherwise.
 */
char *
expand_filter_tokens(filter, env, tmpf, resultf, mtypef, key, hdrs)
    char      *filter;
    ENVELOPE  *env;
    char     **tmpf, **resultf, **mtypef;
    int       *key, *hdrs;
{
    char   **array, **q;
    char    *bp, *cmd = NULL, *p = NULL,
	    *tfn = NULL, *rfn = NULL, *dfn = NULL, *mfn = NULL,
	    *freeme_tfn = NULL, *freeme_rfn = NULL, *freeme_mfn = NULL;
    int      n = 0;
    size_t   len;

    /*
     * break filter into words delimited by whitespace so that we can
     * look for tokens. First we count how many words.
     */
    if((bp = cpystr(filter)) != NULL)
      p = strtok(bp, " \t");
    
    if(p){
	n++;
	while(strtok(NULL, " \t") != NULL)
	  n++;
    }

    if(!n){
	dprint(1, (debugfile, "Unexpected failure creating sending_filter\n"));
	if(bp)
	  fs_give((void **)&bp);

	return(cmd);
    }

    q = array = (char **)fs_get((n+1) * sizeof(*array));
    memset(array, 0, (n+1) * sizeof(*array));
    /* restore bp and form the array */
    strcpy(bp, filter);
    if((p = strtok(bp, " \t")) != NULL){
	*q++ = cpystr(p);
	while((p = strtok(NULL, " \t")) != NULL)
	  *q++ = cpystr(p);
    }

    if(bp)
      fs_give((void **)&bp);

    for(q = array; *q != NULL; q++){
	if(!strcmp(*q, "_RECIPIENTS_")){
	    char *rl = NULL;

	    if(env){
		char *to_l = addr_list_string(env->to,
					      simple_addr_string, 0, 0),
		     *cc_l = addr_list_string(env->cc,
					      simple_addr_string, 0, 0),
		     *bcc_l = addr_list_string(env->bcc,
					       simple_addr_string, 0, 0);

		rl = fs_get(strlen(to_l) + strlen(cc_l) + strlen(bcc_l) + 3);
		sprintf(rl, "%s %s %s", to_l, cc_l, bcc_l);
		fs_give((void **)&to_l);
		fs_give((void **)&cc_l);
		fs_give((void **)&bcc_l);
		for(to_l = rl; *to_l; to_l++)	/* to_l overloaded! */
		  if(*to_l == ',')			/* space delim'd list */
		    *to_l = ' ';
	    }

	    fs_give((void **)q);
	    *q = rl ? rl : cpystr("");
	}
	else if(!strcmp(*q, "_TMPFILE_")){
	    if(!tfn){
		tfn = temp_nam(NULL, "sf");		/* send filter file */
		if(!tfn)
		  dprint(1, (debugfile, "FAILED creat of _TMPFILE_\n"));
	    }

	    if(tmpf)
	      *tmpf = tfn;
	    else
	      freeme_tfn = tfn;

	    fs_give((void **)q);
	    *q = cpystr(tfn ? tfn : "");
	}
	else if(!strcmp(*q, "_RESULTFILE_")){
	    if(!rfn){
		rfn = temp_nam(NULL, "rf");
		/*
		 * We don't create the result file, the user does.
		 * That means we have to remove it after temp_nam creates it.
		 */
		if(rfn)
		  (void)unlink(rfn);
		else
		  dprint(1, (debugfile, "FAILED creat of _RESULTFILE_\n"));
	    }

	    if(resultf)
	      *resultf = rfn;
	    else
	      freeme_rfn = rfn;

	    fs_give((void **)q);
	    *q = cpystr(rfn ? rfn : "");
	}
	else if(!strcmp(*q, "_MIMETYPE_")){
	    if(!mfn){
		mfn = temp_nam(NULL, "mt");
		/*
		 * We don't create the mimetype file, the user does.
		 * That means we have to remove it after temp_nam creates it.
		 */
		if(mfn)
		  (void)unlink(mfn);
		else
		  dprint(1, (debugfile, "FAILED creat of _MIMETYPE_\n"));
	    }

	    if(mtypef)
	      *mtypef = mfn;
	    else
	      freeme_mfn = mfn;

	    fs_give((void **)q);
	    *q = cpystr(mfn ? mfn : "");
	}
	else if(!strcmp(*q, "_DATAFILE_")){
	    if((dfn = filter_data_file(1)) == NULL)	/* filter data file */
	      dprint(1, (debugfile, "FAILED creat of _DATAFILE_\n"));

	    fs_give((void **)q);
	    *q = cpystr(dfn ? dfn : "");
	}
	else if(!strcmp(*q, "_PREPENDKEY_")){
	    (*q)[0] = '\0';
	    if(key)
	      *key = 1;
	}
	else if(!strcmp(*q, "_INCLUDEALLHDRS_")){
	    (*q)[0] = '\0';
	    if(hdrs)
	      *hdrs = 1;
	}
    }

    /* count up required length */
    for(len = 0, q = array; *q != NULL; q++)
      len += (strlen(*q)+1);
    
    cmd = fs_get((len+1) * sizeof(char));
    cmd[len] = '\0';

    /* cat together all the args */
    p = cmd;
    for(q = array; *q != NULL; q++){
	sstrcpy(&p, *q);
	sstrcpy(&p, " ");
    }

    if(freeme_rfn)
      fs_give((void **) &freeme_rfn);

    if(freeme_tfn){			/* this shouldn't happen */
	(void)unlink(freeme_tfn);
	fs_give((void **) &freeme_tfn);
    }

    if(freeme_mfn)
      fs_give((void **) &freeme_mfn);
    
    free_list_array(&array);

    return(cmd);
}




/*
 * filter_data_file - function to return filename of scratch file for
 *		      display and sending filters.  This file is created
 *		      the first time it's needed, and persists until pine
 *		      exits.
 */
char *
filter_data_file(create_it)
    int create_it;
{
    static char *fn = NULL;

    if(!fn && create_it)
      fn = temp_nam(NULL, "df");
    
    return(fn);
}


/*
 * filter_session_key - function to return randomly generated number
 *			representing a key for this session.  The idea is
 *			the display/sending filter could use it to muddle
 *			up any pass phrase or such stored in the
 *			"_DATAFILE_".
 */
char *
filter_session_key()
{
    static char *key = NULL;

    if(!key){
	sprintf(tmp_20k_buf, "%ld", random());
	key = cpystr(tmp_20k_buf);
    }
    
    return(key);
}


/* * * * Routines to support partial/interruptable body fetching * * * * */


/*
 *
 */
void
fetch_readc_init(frd, stream, msgno, section, size, flags)
    FETCH_READC_S *frd;
    MAILSTREAM	  *stream;
    long	   msgno;
    char	  *section;
    unsigned long  size;
    long	   flags;
{
    memset(g_fr_desc = frd, 0, sizeof(FETCH_READC_S));
    frd->stream  = stream;
    frd->msgno   = msgno;
    frd->section = section;
    frd->flags   = flags;
    frd->size    = size;
    frd->readc	 = fetch_readc;
    if(modern_imap_stream(stream)
       && !imap_cache(stream, msgno, section, NULL, NULL)
       && size > INIT_FETCH_CHUNK
       && (F_OFF(F_QUELL_PARTIAL_FETCH, ps_global)
           ||
#ifdef	_WINDOWS
	   F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global)
#else
	   0
#endif
	   )){
	frd->allocsize = INIT_FETCH_CHUNK;
	frd->chunk = (char *) fs_get ((frd->allocsize + 1) * sizeof(char));
	frd->chunksize = frd->allocsize/2;  /* this gets doubled 1st time */
	frd->endp  = frd->chunk;
	frd->free_me++;

#if	!defined(DOS) || defined(WIN32)
	intr_handling_on();
	frd->cache = so_get(CharStar, NULL, EDIT_ACCESS);
	so_truncate(frd->cache, size);		/* pre-allocate */
#endif
    }
    else{				/* fetch the whole bloody thing here */
#if	defined(DOS) && !defined(WIN32)
	SourceType  src = CharStar;
	char	   *tmpfile_name = NULL;

	if(size > MAX_MSG_INCORE
	   || (stream && stream->dtb && stream->dtb->name
	       && !strcmp(stream->dtb->name,"nntp"))){
	    src = FileStar;
	    if(!(tmpfile_name = temp_nam(NULL, "dt"))
	       || !(append_file = fopen(tmpfile_name, "w+b"))){
		if(tmpfile_name){
		    (void)unlink(tmpfile_name);
		    fs_give((void **)&tmpfile_name);
		}

		frd->error = "Can't open temporary file";
		ps_global->intr_pending;
	    }

	    mail_parameters(stream, SET_GETS, (void *)dos_gets);
	    frd->readc = fetch_readc_file;
	}
	else
	  mail_parameters(stream, SET_GETS, (void *)NULL);
#endif	/* DOS */

	frd->chunk = mail_fetch_body(stream, msgno, section, &frd->read,flags);

	/* This only happens if the server gave us a bogus size */
	if(size != frd->read){
	    dprint(1, (debugfile,
	  "fetch_readc_init: size mismatch: size=%lu read=%lu, continue...\n",
		   frd->size, frd->read));
	    q_status_message(SM_ORDER | SM_DING, 0, 3,
		 "Message size does not match expected size, continuing...");
	    frd->size = min(size, frd->read);
	}

	frd->endp  = &frd->chunk[frd->read];
    }

    frd->chunkp = frd->chunk;
}


/*
 *
 */
int
fetch_readc_cleanup()
{
    if(g_fr_desc){
	if(g_fr_desc->chunk && g_fr_desc->free_me)
	  fs_give((void **) &g_fr_desc->chunk);

	if(g_fr_desc->cache){
	    SIZEDTEXT text;

	    text.size = g_fr_desc->size;
	    text.data = (unsigned char *) so_text(g_fr_desc->cache);
	    imap_cache(g_fr_desc->stream, g_fr_desc->msgno,
		       g_fr_desc->section, NULL, &text);
	    g_fr_desc->cache->txt = (void *) NULL;
	    so_give(&g_fr_desc->cache);
	}
    }

#if	defined(DOS) && !defined(WIN32)
    /*
     * free up file pointer, and delete tmpfile opened for
     * dos_gets.  Again, we can't use DOS' tmpfile() as it writes root
     * sheesh.
     */
    if(src == FileStar){
	fclose(append_file);
	append_file = NULL;
	unlink(tmpfile_name);
	fs_give((void **)&tmpfile_name);
	mail_parameters(stream, SET_GETS, (void *)NULL);
	mail_gc(stream, GC_TEXTS);
    }
#endif
#ifndef	DOS
    intr_handling_off();
#endif
    return(0);
}

/*
 * Wrapper around mail_fetch_body.
 * Currently only used to turn on partial fetching if trying
 * to work around the microsoft ssl bug.
 */
char *
pine_mail_fetch_body(stream, msgno, section, len, flags)
    MAILSTREAM *stream;
    unsigned long msgno;
    char *section;
    unsigned long *len;
    long flags;
{
#ifdef _WINDOWS
    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global))
      return(pine_mail_partial_fetch_wrapper(stream, msgno, 
			     section, len, flags, 0, NULL, 0));
    else
#endif
    return(mail_fetch_body(stream, msgno, section, len, flags));
}

/*
 * Wrapper around mail_fetch_text.
 * Currently the only purpose this wrapper serves is to turn
 * on partial fetching for quell-ssl-largeblocks.
 */
char *
pine_mail_fetch_text(stream, msgno, section, len, flags)
    MAILSTREAM *stream;
    unsigned long msgno;
    char *section;
    unsigned long *len;
    long flags;
{
#ifdef _WINDOWS
    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global))
      return(pine_mail_partial_fetch_wrapper(stream, msgno,
					     section, len, flags, 
					     0, NULL, 1));
    else
#endif
      return(mail_fetch_text(stream, msgno, section, len, flags));
}


/*
 * Determine whether to do partial-fetching or not, and do it
 *    args - same as c-client functions being wrapped around
 *    get_n_bytes - try to partial fetch for the first n bytes.
 *                  makes no guarantees, might wind up fetching
 *                  the entire text anyway.
 *    str_to_free - pointer to string to free if we only get
 *                  (non-cachable) partial text (required for
 *                  get_n_bytes).
 *    is_text_fetch - 
 *      set, tells us to do mail_fetch_text and mail_partial_text
 *      unset, tells us to do mail_fetch_body and mail_partial_body
 */
char *
pine_mail_partial_fetch_wrapper(stream, msgno, section, len, flags,
				get_n_bytes, str_to_free,
				is_text_fetch)
    MAILSTREAM *stream;
    unsigned long msgno;
    char *section;
    unsigned long *len;
    long flags;
    unsigned long get_n_bytes;
    char **str_to_free;
    int is_text_fetch;
{
    BODY *body;
    unsigned long size, firstbyte, lastbyte;
    void *old_gets;
    FETCH_READC_S *pftc;
    char imap_cache_section[MAILTMPLEN];
    SIZEDTEXT new_text;
    MESSAGECACHE *mc;
    char *(*fetch_full)(MAILSTREAM *, unsigned long, char *,
		       unsigned long *, long);
    long (*fetch_partial)(MAILSTREAM *, unsigned long, char *,
			   unsigned long, unsigned long, long);

    fetch_full = is_text_fetch ? mail_fetch_text : mail_fetch_body;
    fetch_partial = is_text_fetch ? mail_partial_text : mail_partial_body;
    if(str_to_free)
      *str_to_free = NULL;
#ifdef _WINDOWS
    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global) || get_n_bytes){
#else
    if(get_n_bytes){
#endif /* _WINDOWS */
	if(section && *section)
	  body = mail_body(stream, msgno, (unsigned char *) section);
	else
	  pine_mail_fetch_structure(stream, msgno, &body, flags);
	if(!body)
	  return NULL;
	if(body->type != TYPEMULTIPART)
	  size = body->size.bytes;
	else if((!section || !*section) && msgno > 0L
	        && stream && msgno <= stream->nmsgs
		&& (mc = mail_elt(stream, msgno)))
	  size = mc->rfc822_size; /* upper bound */
	else      /* just a guess, can't get actual size */
	  size = fcc_size_guess(body) + 2048;

	/* 
	 * imap_cache, originally intended for c-client internal use,
	 * takes a section argument that is different from one we
	 * would pass to mail_body.  Typically in this function
	 * section is NULL, which translates to "TEXT", but in other
	 * cases we would want to append ".TEXT" to the section
	 */
	if(is_text_fetch)
	  sprintf(imap_cache_section, "%.*s%sTEXT", MAILTMPLEN - 10,
		  section && *section ? section : "",
		  section && *section ? "." : "");
	else
	  sprintf(imap_cache_section, "%.*s", MAILTMPLEN - 10,
		  section && *section ? section : "");

	if(modern_imap_stream(stream)
#ifdef _WINDOWS
	   && ((size > AVOID_MICROSOFT_SSL_CHUNKING_BUG)
	       || (get_n_bytes && size > get_n_bytes))
#else
	   && (get_n_bytes && size > get_n_bytes)
#endif /* _WINDOWS */
	   && !imap_cache(stream, msgno, imap_cache_section,
			  NULL, NULL)){
	    if(get_n_bytes == 0){
	      dprint(8, (debugfile, 
    "fetch_wrapper: doing partial fetching to work around microsoft bug\n"));
	    }
	    else{
	      dprint(8, (debugfile,
    "fetch_wrapper: partial fetching due to %lu get_n_bytes\n", get_n_bytes));
	    }
	    pftc = (FETCH_READC_S *)fs_get(sizeof(FETCH_READC_S));
	    memset(g_pft_desc = pftc, 0, sizeof(FETCH_READC_S));
#ifdef _WINDOWS
	    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global)){
		if(get_n_bytes)
		  pftc->chunksize = min(get_n_bytes,
					AVOID_MICROSOFT_SSL_CHUNKING_BUG);
		else
		  pftc->chunksize = AVOID_MICROSOFT_SSL_CHUNKING_BUG;
	    }
	    else
#endif /* _WINDOWS */
	    pftc->chunksize = get_n_bytes;

	    pftc->chunk = (char *) fs_get((pftc->chunksize+1)
					  * sizeof(char));
	    pftc->cache = so_get(CharStar, NULL, EDIT_ACCESS);
	    pftc->read = 0L;
	    so_truncate(pftc->cache, size + 1);
	    old_gets = mail_parameters(stream, GET_GETS, (void *)NULL);
	    mail_parameters(stream, SET_GETS, (void *) partial_text_gets);
	    /* start fetching */
	    do{
		firstbyte = pftc->read ;
		lastbyte = firstbyte + pftc->chunksize;
		if(get_n_bytes > firstbyte && get_n_bytes < lastbyte){
		    pftc->chunksize = get_n_bytes - firstbyte;
		    lastbyte = get_n_bytes;
		}
		(*fetch_partial)(stream, msgno, section, firstbyte,
				 pftc->chunksize, flags);

		if(pftc->read != lastbyte)
		  break;
	    } while((pftc->read ==  lastbyte)
		    && (!get_n_bytes || (pftc->read < get_n_bytes)));
	    dprint(8, (debugfile,
		       "fetch_wrapper: anticipated size=%lu read=%lu\n",
		       size, pftc->read));
	    mail_parameters(stream, SET_GETS, old_gets);
	    new_text.size = pftc->read;
	    new_text.data = (unsigned char *)so_text(pftc->cache);

	    if(get_n_bytes && pftc->read == get_n_bytes){
		/* 
		 * don't write to cache if we're only dealing with
		 * partial text.
		 */
		if(!str_to_free)
		  panic("Programmer botch: partial fetch attempt w/o string pointer");
		else
		  *str_to_free = (char *) new_text.data;
	    }
	    else {
		/* ugh, rewrite string in case it got stomped on last call */
		if(is_text_fetch)
		  sprintf(imap_cache_section, "%.*s%sTEXT", MAILTMPLEN - 10,
			  section && *section ? section : "",
			  section && *section ? "." : "");
		else
		  sprintf(imap_cache_section, "%.*s", MAILTMPLEN - 10,
			  section && *section ? section : "");

		imap_cache(stream, msgno, imap_cache_section, NULL, &new_text);
	    }

	    pftc->cache->txt = (void *)NULL;
	    so_give(&pftc->cache);
	    fs_give((void **)&pftc->chunk);
	    fs_give((void **)&pftc);
	    g_pft_desc = NULL;
	    if(len)
	      *len = new_text.size;
	    return ((char *)new_text.data);
	}
	else
	  return((*fetch_full)(stream, msgno, section, len, flags));
    }
    else
      return((*fetch_full)(stream, msgno, section, len, flags));
}

/*
 * c-client callback that handles getting the text
 */
char *
partial_text_gets(f, stream, size, md)
    readfn_t f;
    void *stream;
    unsigned long size;
    GETS_DATA *md;
{
    unsigned long n;

    n = min(g_pft_desc->chunksize, size);
    g_pft_desc->read +=n;

    (*f) (stream, n, g_pft_desc->chunk);

    if(g_pft_desc->cache)
      so_nputs(g_pft_desc->cache, g_pft_desc->chunk, (long) n);


    return(NULL);
}


/*
 *
 */
char *
fetch_gets(f, stream, size, md)
    readfn_t f;
    void *stream;
    unsigned long size;
    GETS_DATA *md;
{
    unsigned long n;

    n		     = min(g_fr_desc->chunksize, size);
    g_fr_desc->read += n;
    g_fr_desc->endp  = &g_fr_desc->chunk[n];

    (*f) (stream, n, g_fr_desc->chunkp = g_fr_desc->chunk);

    if(g_fr_desc->cache)
      so_nputs(g_fr_desc->cache, g_fr_desc->chunk, (long) n);

    /* BUG: need to read requested "size" in case it's larger than chunk? */

    return(NULL);
}


/*
 *
 */
int
fetch_readc(c)
    unsigned char *c;
{
    extern void gf_error PROTO((char *));

    if(ps_global->intr_pending){
	(void) fetch_readc_cleanup();
	gf_error(g_fr_desc->error ? g_fr_desc->error :"Transfer interrupted!");
	/* no return */
    }
    else if(g_fr_desc->chunkp == g_fr_desc->endp){

	/* Anything to read, do it */
	if(g_fr_desc->read < g_fr_desc->size){
	    void *old_gets;
	    int	  rv;
	    TIMEVAL_S before, after;
	    long diff, wdiff;
	    unsigned long save_read;

	    old_gets = mail_parameters(g_fr_desc->stream, GET_GETS,
				       (void *)NULL);
	    mail_parameters(g_fr_desc->stream, SET_GETS, (void *) fetch_gets);

	    /*
	     * Adjust chunksize with the goal that it will be about
	     * TARGET_INTR_TIME useconds +- 20%
	     * to finish the partial fetch. We want that time
	     * to be small so that interrupts will happen fast, but we want
	     * the chunksize large so that the whole fetch will happen
	     * fast. So it's a tradeoff between those two things.
	     *
	     * If the estimated fetchtime is getting too large, we
	     * half the chunksize. If it is small, we double
	     * the chunksize. If it is in between, we leave it. There is
	     * some risk of oscillating between two values, but who cares?
	     */
	    if(g_fr_desc->fetchtime <
				    TARGET_INTR_TIME - TARGET_INTR_TIME/5)
	      g_fr_desc->chunksize *= 2;
	    else if(g_fr_desc->fetchtime >
				    TARGET_INTR_TIME + TARGET_INTR_TIME/5)
	      g_fr_desc->chunksize /= 2;

	    g_fr_desc->chunksize = min(MAX_FETCH_CHUNK,
				       max(MIN_FETCH_CHUNK,
					   g_fr_desc->chunksize));
	    
#ifdef	_WINDOWS
	    /*
	     * If this feature is set, limit the max size to less than
	     * 16K - 5, the magic number that avoids Microsoft's bug.
	     * Let's just go with 12K instead of 16K - 5.
	     */
	    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global))
	      g_fr_desc->chunksize =
		  min(AVOID_MICROSOFT_SSL_CHUNKING_BUG, g_fr_desc->chunksize);
#endif

	    /* don't ask for more than there should be left to ask for */
	    g_fr_desc->chunksize =
		min(g_fr_desc->size - g_fr_desc->read, g_fr_desc->chunksize);

	    /*
	     * If chunksize grew, reallocate chunk.
	     */
	    if(g_fr_desc->chunksize > g_fr_desc->allocsize){
		g_fr_desc->allocsize = g_fr_desc->chunksize;
		fs_give((void **) &g_fr_desc->chunk);
		g_fr_desc->chunk = (char *) fs_get ((g_fr_desc->allocsize + 1)
								* sizeof(char));
		g_fr_desc->endp   = g_fr_desc->chunk;
		g_fr_desc->chunkp = g_fr_desc->chunk;
	    }

	    save_read = g_fr_desc->read;
	    (void)get_time(&before);

	    rv = mail_partial_body(g_fr_desc->stream, g_fr_desc->msgno,
				   g_fr_desc->section, g_fr_desc->read,
				   g_fr_desc->chunksize, g_fr_desc->flags);

	    /*
	     * If the amount we actually read is less than the amount we
	     * asked for we assume that is because the server gave us a
	     * bogus size when we originally asked for it.
	     */
	    if(g_fr_desc->chunksize > (g_fr_desc->read - save_read)){
		dprint(1, (debugfile,
  "partial_body returned less than asked for: asked=%lu got=%lu, continue...\n",
		       g_fr_desc->chunksize, g_fr_desc->read - save_read));
		q_status_message(SM_ORDER | SM_DING, 0, 3,
		   "Message size does not match expected size, continuing...");
		g_fr_desc->size = g_fr_desc->read;
	    }

	    if(get_time(&after) == 0){
		diff = time_diff(&after, &before);
		wdiff = min(TARGET_INTR_TIME + TARGET_INTR_TIME/2,
			    max(TARGET_INTR_TIME - TARGET_INTR_TIME/2, diff));
		/*
		 * Fetchtime is an exponentially weighted average of the number
		 * of usecs it takes to do a single fetch of whatever the
		 * current chunksize is. Since the fetch time probably isn't
		 * simply proportional to the chunksize, we don't try to
		 * calculate a chunksize by keeping track of the bytes per
		 * second. Instead, we just double or half the chunksize if
		 * we are too fast or too slow. That happens the next time
		 * through the loop a few lines up.
		 * Too settle it down a bit, Windsorize the mean.
		 */
		g_fr_desc->fetchtime = (g_fr_desc->fetchtime == 0)
					? wdiff
					: g_fr_desc->fetchtime/2 + wdiff/2;
		dprint(8, (debugfile,
	         "fetch: diff=%ld wdiff=%ld fetchave=%ld prev chunksize=%ld\n",
	         diff, wdiff, g_fr_desc->fetchtime, g_fr_desc->chunksize));
	    }
	    else  /* just set it so it won't affect anything */
	      g_fr_desc->fetchtime = TARGET_INTR_TIME;

	    /* UNinstall mailgets */
	    mail_parameters(g_fr_desc->stream, SET_GETS, old_gets);

	    if(!rv){
		(void) fetch_readc_cleanup();
		gf_error("Partial fetch failed!");
		/* no return */
	    }
	}
	else				/* clean up and return done. */
	  return(fetch_readc_cleanup());
    }

    *c = *g_fr_desc->chunkp++;

    return(1);
}


#ifdef	_WINDOWS
int
scroll_att_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup scrat_popup[20];
    int	   i = -1, n;

    if(in_handle){
	scrat_popup[++i].type	    = tIndex;
	scrat_popup[i].label.style  = lNormal;
	scrat_popup[i].label.string = "View Selectable Item";
	scrat_popup[i].data.val	    = ctrl('L');
    }

    scrat_popup[++i].type	= tQueue;
    scrat_popup[i].label.style	= lNormal;
    scrat_popup[i].label.string = "&Save";
    scrat_popup[i].data.val	= 'S';

    scrat_popup[++i].type	= tQueue;
    scrat_popup[i].label.style	= lNormal;
    if(msgno_exceptions(ps_global->mail_stream,
			mn_m2raw(ps_global->msgmap,
				 mn_get_cur(ps_global->msgmap)),
			scrat_attachp->number, &n, FALSE)
       && (n & MSG_EX_DELETE)){
	scrat_popup[i].label.string = "&Undelete";
	scrat_popup[i].data.val	    = 'U';
    }
    else{
	scrat_popup[i].label.string = "&Delete";
	scrat_popup[i].data.val	    = 'D';
    }

    if(MIME_MSG_A(scrat_attachp) || MIME_DGST_A(scrat_attachp)){
	scrat_popup[++i].type	    = tQueue;
	scrat_popup[i].label.style  = lNormal;
	scrat_popup[i].label.string = "&Reply";
	scrat_popup[i].data.val	    = 'R';

	scrat_popup[++i].type	    = tQueue;
	scrat_popup[i].label.style  = lNormal;
	scrat_popup[i].label.string = "&Forward";
	scrat_popup[i].data.val	    = 'f';
    }

    scrat_popup[++i].type = tSeparator;

    scrat_popup[++i].type	    = tQueue;
    scrat_popup[i].label.style  = lNormal;
    scrat_popup[i].label.string = "Attachment Index";
    scrat_popup[i].data.val	    = '<';

    scrat_popup[++i].type = tTail;

    return(mswin_popup(scrat_popup) == 0 && in_handle);
}


void
display_att_window(a)
     ATTACH_S *a;
{
    char    *filename;
    STORE_S *store;
    gf_io_t  pc;
    char    *err;
    int      we_cancel = 0;
#if !defined(DOS) && !defined(OS2)
    char     prefix[8];
#endif

    if(a->body->type == TYPEMULTIPART){
	if(a->body->subtype){
/*	    if(!strucmp(a->body->subtype, "digest"))
	      display_digest_att(msgno, a, flags);
	    else */
	      q_status_message1(SM_ORDER, 3, 5,
				"Can't display Multipart/%.200s",
				a->body->subtype);
	}
	else
	  q_status_message(SM_ORDER, 3, 5,
			   "Can't display unknown Multipart Subtype");
    }
/*    else if(MIME_VCARD_A(a))
      display_vcard_att_window(msgno, a, flags);*/
    else if(a->body->type == TYPETEXT)
      display_text_att_window(a);
    else if(a->body->type == TYPEMESSAGE)
      display_msg_att_window(a);
}



void
display_text_att_window(a)
    ATTACH_S *a;
{
    STORE_S  *store;
    long      msgno;

    msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));

    if(store = format_text_att(msgno, a, NULL)){
	if (mswin_displaytext("ATTACHED TEXT",
			      so_text(store),
			      strlen((char *) so_text(store)),
			      NULL, 0, 0) >= 0)
	  store->txt = (void *) NULL;	/* free'd in mswin_displaytext */

	so_give(&store);	/* free resources associated with store */
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3,
		       "Error allocating space for attachment.");
}



void
display_msg_att_window(a)
    ATTACH_S *a;
{
    STORE_S  *store;
    gf_io_t   pc;
    ATTACH_S *ap = a;
    long      msgno;

    msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));

    /* BUG, should check this return code */
    (void) pine_mail_fetchstructure(ps_global->mail_stream, msgno, NULL);

    /* initialize a storage object */
    if(store = so_get(CharStar, NULL, EDIT_ACCESS)){

	gf_set_so_writec(&pc, store);

	if(format_msg_att(msgno, &ap, pc, FM_DISPLAY)
	   && mswin_displaytext("ATTACHED MESSAGE", so_text(store),
				strlen((char *) so_text(store)),
				NULL, 0, 0) >= 0)
	  /* free'd in mswin_displaytext */
	  store->txt = (void *) NULL;

	gf_clear_so_writec(store);

	so_give(&store);
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3,
		       "Error allocating space for message.");
}
#endif
