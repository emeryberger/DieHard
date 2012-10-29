#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: send.c 14081 2005-09-12 22:04:25Z hubert@u.washington.edu $";
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
       send.c
       Functions for composing and sending mail

 ====*/

#include "headers.h"
#include "../c-client/smtp.h"
#include "../c-client/nntp.h"
#include "../c-client/imap4r1.h"


#ifndef TCPSTREAM
#define TCPSTREAM void
#endif

#define	MIME_VER	"MIME-Version: 1.0\015\012"

typedef struct body_particulars {
    unsigned short     type, encoding, had_csp;
    char              *subtype, *charset;
    PARAMETER         *parameter;
} BODY_PARTICULARS_S;



/*
 * Internal Prototypes
 */
int	   redraft PROTO((MAILSTREAM **, ENVELOPE **, BODY **,
			  char **, char **, REPLY_S **, REDRAFT_POS_S **,
			  PINEFIELD **, ACTION_S **, int));
int	   redraft_cleanup PROTO((MAILSTREAM **, int, int));
int	   redraft_prompt PROTO((char *, char *, int));
int	   postpone_prompt PROTO(());
REPLY_S   *build_reply_uid PROTO((char *));
void       outgoing2strings PROTO((METAENV *, BODY *, void **, PATMT **));
void       strings2outgoing PROTO((METAENV *, BODY **, PATMT *, char *, int));
void	   resolve_encoded_entries PROTO((ADDRESS *, ADDRESS *));
void       create_message_body PROTO((BODY **, PATMT *, char *, int));
void	   create_message_body_text PROTO((BODY *, char *, int));
int	   check_addresses PROTO((METAENV *));
int	   check_for_subject PROTO((METAENV *));
PINEFIELD *parse_custom_hdrs PROTO((char **, CustomType));
void       add_defaults_from_list PROTO((PINEFIELD *, char **));
void       free_customs PROTO((PINEFIELD *));
int        view_as_rich PROTO((char *, int));
int        count_custom_hdrs_pf PROTO((PINEFIELD *, int));
int        count_custom_hdrs_list PROTO((char **));
PINEFIELD *combine_custom_headers PROTO((PINEFIELD *, PINEFIELD *));
int        pine_header_forbidden PROTO((char *));
int        hdr_is_in_list PROTO((char *, PINEFIELD *));
CustomType set_default_hdrval PROTO((PINEFIELD *, PINEFIELD *));
int	   filter_message_text PROTO((char *, ENVELOPE *, BODY *, STORE_S **,
				      METAENV *));
void	   post_compose_filters PROTO((BODY *));
void	   pine_send_newsgroup_name PROTO((char *, char*, size_t));
long	   message_format_for_pico PROTO((long, int (*)(int)));
char	  *send_exit_for_pico PROTO((struct headerentry *, void (*)(), int));
int	   mime_type_for_pico PROTO((char *));
char      *cancel_for_pico PROTO((void (*)()));
void	   update_answered_flags PROTO((REPLY_S *));
int	   pine_write_body_header PROTO((BODY *, soutr_t, TCPSTREAM *));
int	   pwbh_finish PROTO((int, STORE_S *));
void       pine_encode_body PROTO((BODY *));
long       pine_rfc822_header PROTO((METAENV *, BODY *, soutr_t, TCPSTREAM *));
int        pine_header_line PROTO((char *, METAENV *, char *, soutr_t,
			    TCPSTREAM *, int, int));
int	   pine_address_line PROTO((char *, METAENV *, ADDRESS *,
			     soutr_t, TCPSTREAM *, int, int));
long       post_rfc822_output PROTO ((char *, ENVELOPE *, BODY *, soutr_t,
				      TCPSTREAM *, long));
long       pine_rfc822_output PROTO ((METAENV *, BODY *, soutr_t,TCPSTREAM *));
long       pine_rfc822_output_body PROTO((BODY *,soutr_t,TCPSTREAM *));
void       pine_free_body PROTO((BODY **));
void       pine_free_body_data PROTO((BODY *));
long	   pine_smtp_verbose PROTO((SENDSTREAM *));
void	   pine_smtp_verbose_out PROTO((char *));
int        call_mailer PROTO((METAENV *, BODY *, char **));
int        news_poster PROTO((METAENV *, BODY *, char **));
char      *tidy_smtp_mess PROTO((char *, char *, char *));
void       mime_recur PROTO((BODY *));
long	   piped_soutr PROTO((void *, char *));
long	   piped_sout PROTO((void *, char *, unsigned long));
char	  *piped_getline PROTO((void *));
void	  *piped_smtp_open PROTO((char *, char *, unsigned long));
void	  *piped_aopen PROTO((NETMBX *, char *, char *));
void	   piped_close PROTO((void *));
void	   piped_abort PROTO((void *));
char	  *piped_host PROTO((void *));
unsigned long piped_port PROTO((void *));
char	  *piped_localhost PROTO((void *));
int	   sent_percent PROTO(());
long	   send_body_size PROTO((BODY *));
void	   set_body_size PROTO((BODY *));
BODY	  *first_text_8bit PROTO((BODY *));
void	   set_only_charset_by_grope PROTO((BODY *, char *));
void       set_mime_flowed_text PROTO((BODY *));
void	   set_mime_charset PROTO((PARAMETER *, int, char *));
int	   background_posting PROTO((int));
void       free_body_particulars PROTO((BODY_PARTICULARS_S *));
void       reset_body_particulars PROTO((BODY_PARTICULARS_S *, BODY *));
char      *encode_header_value PROTO((char *, size_t, unsigned char *,
				      char *, int));
int        encode_whole_header PROTO((char *, METAENV *));
BODY_PARTICULARS_S *save_body_particulars PROTO((BODY *));
ADDRESS		   *phone_home_from PROTO(());
unsigned int	    phone_home_hash PROTO((char *));

/*
 * Pointer to buffer to hold pointers into pine data that's needed by pico. 
 */
static	PICO	*pbf;


/* 
 * Storage object where the FCC (or postponed msg) is to be written.
 * This is amazingly bogus.  Much work was done to put messages 
 * together and encode them as they went to the tmp file for sendmail
 * or into the SMTP slot (especially for for DOS, to prevent a temporary
 * file (and needlessly copying the message).
 * 
 * HOWEVER, since there's no piping into c-client routines
 * (particularly mail_append() which copies the fcc), the fcc will have
 * to be copied to disk.  This global tells pine's copy of the rfc822
 * output functions where to also write the message bytes for the fcc.
 * With piping in the c-client we could just have two pipes to shove
 * down rather than messing with damn copies.  FIX THIS!
 *
 * The function open_fcc, locates the actual folder and creates it if
 * requested before any mailing or posting is done.
 */
static struct local_message_copy {
    STORE_S  *so;
    unsigned  text_only:1;
    unsigned  all_written:1;
    unsigned  text_written:1;
} lmc;




static char *g_rolenick = NULL;


/*
 * Locally global pointer to stream used for sending/posting.
 * It's also used to indicate when/if we write the Bcc: field in
 * the header.
 */
static SENDSTREAM *sending_stream = NULL;
static struct hooks {
    void	*rfc822_out;			/* Message outputter */
} sending_hooks;
static FILE	  *verbose_send_output = NULL;
static long	   send_bytes_sent, send_bytes_to_send;
static char	  *sending_filter_requested;
static char	   verbose_requested, background_requested, flowing_requested;
static unsigned char dsn_requested;
static METAENV	  *send_header = NULL;
static NETDRIVER piped_io = {
    piped_smtp_open,		/* open a connection */
    piped_aopen,		/* open an authenticated connection */
    piped_getline,		/* get a line */
    NULL,			/* get a buffer */
    piped_soutr,		/* output pushed data */
    piped_sout,			/* output string */
    piped_close,		/* close connection */
    piped_host,			/* return host name */
    piped_host,			/* remotehost */
    piped_port,			/* return port number */
    piped_host			/* return local host (NOTE: same as host!) */
};


#if	defined(DOS) && !defined(WIN32)
#define	FCC_SOURCE	TmpFileStar
#define	FCC_STREAM_MODE	OP_SHORTCACHE
#else
#define	FCC_SOURCE	CharStar
#define	FCC_STREAM_MODE	0L
#endif



/*
 * Various useful strings
 */
#define	INTRPT_PMT \
	    "Continue INTERRUPTED composition (answering \"n\" won't erase it)"
#define	PSTPND_PMT \
	    "Continue postponed composition (answering \"No\" won't erase it)"
#define	FORM_PMT \
	    "Start composition from Form Letter Folder"
#define	PSTPN_FORM_PMT	\
	    "Save to Postponed or Form letter folder? "
#define	POST_PMT   \
	    "Posted message may go to thousands of readers. Really post"
#define	INTR_DEL_PMT \
	    "Deleted messages will be removed from folder after use.  Proceed"

#if	defined(DOS) || defined(OS2)
#define	POST_PERM_GRIPE	\
	"Send requires open remote folder. Postpone until a folder is opened."
#endif

/*
 * Since c-client preallocates, it's necessary here to define a limit
 * such that we don't blow up in c-client (see rfc822_address_line()).
 */
#define	MAX_SINGLE_ADDR	MAILTMPLEN


/*
 * For check_for_
 */
#define CF_OK		0x1
#define CF_MISSING	0x2

/*
 * For delivery-status notification
 */
#define DSN_NEVER   0x1
#define DSN_DELAY   0x2
#define DSN_SUCCESS 0x4
#define DSN_FULL    0x8
#define DSN_SHOW    0x10


/*
 * Macros to help sort out posting results
 */
#define	P_MAIL_WIN	0x01
#define	P_MAIL_LOSE	0x02
#define	P_MAIL_BITS	0x03
#define	P_NEWS_WIN	0x04
#define	P_NEWS_LOSE	0x08
#define	P_NEWS_BITS	0x0C
#define	P_FCC_WIN	0x10
#define	P_FCC_LOSE	0x20
#define	P_FCC_BITS	0x30


/*
 * Redraft flags...
 */
#define	REDRAFT_NONE	0
#define	REDRAFT_PPND	0x01
#define	REDRAFT_DEL	0x02

#define COMPOSE_MAIL_TITLE "COMPOSE MESSAGE"

/*
 * Phone home hash controls
 */
#define PH_HASHBITS	24
#define PH_MAXHASH	(1<<(PH_HASHBITS))


#ifdef	DEBUG_PLUS
#define	TIME_STAMP(str, l)	{ \
				  struct timeval  tp; \
				  struct timezone tzp; \
				  if(gettimeofday(&tp, &tzp) == 0) \
				    dprint(l, (debugfile, \
					   "\nKACHUNK (%s) : %.8s.%3.3ld\n", \
					   str, ctime(&tp.tv_sec) + 11, \
					   tp.tv_usec));\
				}
#else
#define	TIME_STAMP(str, l)
#endif



/*----------------------------------------------------------------------
    Compose screen (not forward or reply). Set up envelope, call composer
  
   Args: pine_state -- The usual pine structure
 
  Little front end for the compose screen
  ---*/
void
compose_screen(pine_state)
    struct pine *pine_state;
{
    void (*prev_screen)() = pine_state->prev_screen,
	 (*redraw)() = pine_state->redrawer;

    pine_state->redrawer = NULL;
    ps_global->next_screen = SCREEN_FUN_NULL;
    mailcap_free(); /* free resources we won't be using for a while */
    compose_mail(NULL, NULL, NULL, NULL, NULL);
    pine_state->next_screen = prev_screen;
    pine_state->redrawer = redraw;
}



/*----------------------------------------------------------------------
    Alternate compose screen. Set up role and call regular compose.
  
   Args: pine_state -- The usual pine structure
  ---*/
void
alt_compose_screen(pine_state)
    struct pine *pine_state;
{
    ACTION_S *role = NULL;
    void (*prev_screen)() = pine_state->prev_screen,
	 (*redraw)() = pine_state->redrawer;

    pine_state->redrawer = NULL;
    ps_global->next_screen = SCREEN_FUN_NULL;
    mailcap_free(); /* free resources we won't be using for a while */

    /* Setup role */
    if(role_select_screen(pine_state, &role, MC_COMPOSE) < 0){
	cmd_cancelled("Composition");
	pine_state->next_screen = prev_screen;
	pine_state->redrawer = redraw;
	return;
    }

    /*
     * If default role was selected (NULL) we need to make up a role which
     * won't do anything, but will cause compose_mail to think there's
     * already a role so that it won't try to confirm the default.
     */
    if(role)
      role = combine_inherited_role(role);
    else{
	role = (ACTION_S *)fs_get(sizeof(*role));
	memset((void *)role, 0, sizeof(*role));
	role->nick = cpystr("Default Role");
    }

    pine_state->redrawer = NULL;
    compose_mail(NULL, NULL, role, NULL, NULL);
    free_action(&role);
    pine_state->next_screen = prev_screen;
    pine_state->redrawer = redraw;
}



/*----------------------------------------------------------------------
     Format envelope for outgoing message and call editor

 Args:  given_to -- An address to send mail to (usually from command line 
                       invocation)
        fcc_arg  -- The fcc that goes with this address.
 
 If a "To" line is given format that into the envelope and get ready to call
           the composer
 If there's a message postponed, offer to continue it, and set it up,
 otherwise just fill in the outgoing envelope as blank.

 NOTE: we ignore postponed and interrupted messages in nr mode
 ----*/
void 
compose_mail(given_to, fcc_arg, role_arg, attach, inc_text_getc)
    char          *given_to,
	          *fcc_arg;
    ACTION_S      *role_arg;
    PATMT         *attach;
    gf_io_t        inc_text_getc;
{
    BODY	  *body;
    ENVELOPE	  *outgoing = NULL;
    PINEFIELD	  *custom   = NULL;
    REPLY_S	  *reply	= NULL;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S      *role = NULL;
    MAILSTREAM	  *stream;
    char	  *fcc_to_free,
		  *fcc      = NULL,
		  *lcc      = NULL,
		  *sig      = NULL;
    int		   fcc_is_sticky = 0,
                   intrptd = 0,
                   postponed = 0,
                   form = 0;

    dprint(1, (debugfile,
                 "\n\n    ---- COMPOSE SCREEN (not in pico yet) ----\n"));

    /*-- Check for INTERRUPTED mail  --*/
    if(!role_arg && !(given_to || attach)){
	char	     file_path[MAXPATH+1];

	/* build filename and see if it exists.  build_path creates
	 * an explicit local path name, so all c-client access is thru
	 * local drivers.
	 */
	file_path[0] = '\0';
	build_path(file_path,
		   ps_global->VAR_OPER_DIR ? ps_global->VAR_OPER_DIR
					   : ps_global->home_dir,
		   INTERRUPTED_MAIL, sizeof(file_path));

	/* check to see if the folder exists, the user wants to continue
	 * and that we can actually read something in...
	 */
	if(folder_exists(NULL, file_path) & FEX_ISFILE)
	  intrptd = 1;
    }
    /*-- Check for postponed mail --*/
    if(!role_arg
       && !outgoing				/* not replying/forwarding */
       && !(given_to || attach)			/* not command line send */
       && ps_global->VAR_POSTPONED_FOLDER	/* folder to look in */
       && ps_global->VAR_POSTPONED_FOLDER[0])
      postponed = 1;

    /*-- Check for form letter folder --*/
    if(!role_arg
       && !outgoing				/* not replying/forwarding */
       && !(given_to || attach)			/* not command line send */
       && ps_global->VAR_FORM_FOLDER		/* folder to look in */
       && ps_global->VAR_FORM_FOLDER[0])
      form = 1;

    if(!outgoing && !(given_to || attach)
       && !role_arg && F_ON(F_ALT_COMPOSE_MENU, ps_global)){
        char     prompt[80];
 	char     letters[30];
	char     chosen_task;
 	char    *new      = "New";
 	char    *intrpt   = "Interrupted";
 	char    *postpnd  = "Postponed";
 	char    *formltr  = "FormLetter";
 	char    *roles    = "setRole";
	HelpType help     = h_composer_browse;
	ESCKEY_S compose_style[6];
	unsigned which_help;
	int      ekey_num;
	
	ekey_num = 0;
	compose_style[ekey_num].ch      = 'n';
 	compose_style[ekey_num].rval    = 'n';
 	compose_style[ekey_num].name    = "N";
 	compose_style[ekey_num++].label = new;

	if(intrptd){
 	    compose_style[ekey_num].ch      = 'i';
 	    compose_style[ekey_num].rval    = 'i';
 	    compose_style[ekey_num].name    = "I";
 	    compose_style[ekey_num++].label = intrpt;
 	}
 
 	if(postponed){
 	    compose_style[ekey_num].ch      = 'p';
 	    compose_style[ekey_num].rval    = 'p';
 	    compose_style[ekey_num].name    = "P";
 	    compose_style[ekey_num++].label = postpnd;
 	}
 
 	if(form){
 	    compose_style[ekey_num].ch      = 'f';
 	    compose_style[ekey_num].rval    = 'f';
 	    compose_style[ekey_num].name    = "F";
 	    compose_style[ekey_num++].label = formltr;
 	}

	compose_style[ekey_num].ch      = 'r';
	compose_style[ekey_num].rval    = 'r';
	compose_style[ekey_num].name    = "R";
	compose_style[ekey_num++].label = roles;

 	compose_style[ekey_num].ch    = -1;

 	if(F_ON(F_BLANK_KEYMENU,ps_global)){
 	    char *p;
 
 	    p = letters;
 	    *p = '\0';
 	    for(ekey_num = 0; compose_style[ekey_num].ch != -1; ekey_num++){
 		*p++ = compose_style[ekey_num].ch;
 		if(compose_style[ekey_num + 1].ch != -1)
 		  *p++ = ',';
 	    }
 
 	    *p = '\0';
 	}
 
	which_help = intrptd + 2 * postponed + 4 * form;
	switch(which_help){
	  case 1:
	    help = h_compose_intrptd;
	    break;
	  case 2:
	    help = h_compose_postponed;
	    break;
	  case 3:
	    help = h_compose_intrptd_postponed;
	    break;
	  case 4:
	    help = h_compose_form;
	    break;
	  case 5:
	    help = h_compose_intrptd_form;
	    break;
	  case 6:
	    help = h_compose_postponed_form;
	    break;
	  case 7:
	    help = h_compose_intrptd_postponed_form;
	    break;
	  default:
	    help = h_compose_default;
	    break;
	}
 	sprintf(prompt,
 		"Choose a compose method from %s : ",
 		F_ON(F_BLANK_KEYMENU,ps_global) ? letters : "the menu below");

 	chosen_task = radio_buttons(prompt, -FOOTER_ROWS(ps_global),
 				    compose_style, 'n', 'x', help,RB_NORM,NULL);
	intrptd = postponed = form = 0;
	switch(chosen_task){
	  case 'i':
	    intrptd = 1;
	    break;
	  case 'p':
	    postponed = 1;
	    break;
	  case 'r':
	    { 
	      void (*prev_screen)() = ps_global->prev_screen,
		   (*redraw)() = ps_global->redrawer;
	      ps_global->redrawer = NULL;
	      ps_global->next_screen = SCREEN_FUN_NULL;	      
	      if(role_select_screen(ps_global, &role, MC_COMPOSE) < 0){
		cmd_cancelled("Composition");
		ps_global->next_screen = prev_screen;
		ps_global->redrawer = redraw;
		return;
	      }
	      ps_global->next_screen = prev_screen;
	      ps_global->redrawer = redraw;
	      if(role)
		role = combine_inherited_role(role);
	    }
	    break;
	  case 'f':
	    form = 1;
	    break;
	  case 'x':
	    q_status_message(SM_ORDER, 0, 3,
			     "Composition cancelled");
	    return;
	    break;
	  default:
	    break;
	}
    }
    if(intrptd && !outgoing){
	char	     file_path[MAXPATH+1];
	int	     ret = 'n';

	file_path[0] = '\0';
	build_path(file_path,
		   ps_global->VAR_OPER_DIR ? ps_global->VAR_OPER_DIR
					   : ps_global->home_dir,
		   INTERRUPTED_MAIL, sizeof(file_path));
	if(folder_exists(NULL, file_path) & FEX_ISFILE){
	    if((stream = pine_mail_open(NULL, file_path,
					FCC_STREAM_MODE|SP_USEPOOL|SP_TEMPUSE,
					NULL))
	       && !stream->halfopen){

		if(F_ON(F_ALT_COMPOSE_MENU, ps_global) || 
		   (ret = redraft_prompt("Interrupted",INTRPT_PMT,'n')) =='y'){
		    if(!redraft(&stream, &outgoing, &body, &fcc, &lcc, &reply,
				&redraft_pos, &custom, &role, REDRAFT_DEL)){
			if(stream)
			  pine_mail_close(stream);

			return;
		    }

		    /* redraft() may or may not have closed stream */
		    if(stream)
		      pine_mail_close(stream);

		    postponed = form = 0;
		}
		else{
		    pine_mail_close(stream);
		    if(ret == 'x'){
			q_status_message(SM_ORDER, 0, 3,
					 "Composition cancelled");
			return;
		    }
		}
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  "Can't open Interrupted mailbox: %.200s",
				  file_path);
		if(stream)
		  pine_mail_close(stream);
	    }
	}
    }
    if(postponed && !outgoing){
	CONTEXT_S *p_cntxt = NULL;
	char	  *mbox, *p, *q, tmp[MAILTMPLEN], *fullname = NULL;
	int	   ret = 'n', exists, done = 0;

	/*
	 * find default context to look for folder...
	 *
	 * The "mbox" is assumed to be local if we're given what looks
	 * like an absolute path.  This is different from Goto/Save
	 * where we do alot of work to interpret paths relative to the
	 * server.  This reason is to support all the pre-4.00 pinerc'
	 * that specified a path and because there's yet to be a way
	 * in c-client to specify otherwise in the face of a remote
	 * context.
	 */
	if(!is_absolute_path(mbox = ps_global->VAR_POSTPONED_FOLDER)
	   && !(p_cntxt = default_save_context(ps_global->context_list)))
	  p_cntxt = ps_global->context_list;

	/* check to see if the folder exists, the user wants to continue
	 * and that we can actually read something in...
	 */
	exists = folder_name_exists(p_cntxt, mbox, &fullname);
	if(fullname)
	  mbox = fullname;

	if(exists & FEX_ISFILE){
	    context_apply(tmp, p_cntxt, mbox, sizeof(tmp));
	    if(!(IS_REMOTE(tmp) || is_absolute_path(tmp))){
		/*
		 * The mbox is relative to the home directory.
		 * Make it absolute so we can compare it to
		 * stream->mailbox.
		 */
		build_path(tmp_20k_buf, ps_global->ui.homedir, tmp,
			   SIZEOF_20KBUF);
		strncpy(tmp, tmp_20k_buf, sizeof(tmp));
		tmp[sizeof(tmp)-1] = '\0';
	    }

	    if((stream = ps_global->mail_stream)
	       && !(stream->mailbox
		    && ((*tmp != '{' && !strcmp(tmp, stream->mailbox))
			|| (*tmp == '{'
			    && same_stream(tmp, stream)
			    && (p = strchr(tmp, '}'))
			    && (q = strchr(stream->mailbox,'}'))
			    && !strcmp(p + 1, q + 1)))))
	       stream = NULL;

	    if(stream
	       || ((stream = context_open(p_cntxt,NULL,mbox,
					  FCC_STREAM_MODE|SP_USEPOOL|SP_TEMPUSE,
					  NULL))
		   && !stream->halfopen)){
		if(F_ON(F_ALT_COMPOSE_MENU, ps_global) || 
		   (ret = redraft_prompt("Postponed",PSTPND_PMT,'n')) == 'y'){
		    if(!redraft(&stream, &outgoing, &body, &fcc, &lcc, &reply,
				&redraft_pos, &custom, &role,
				REDRAFT_DEL | REDRAFT_PPND))
			done++;

		    /* stream may or may not be closed in redraft() */
		    if(stream && (stream != ps_global->mail_stream))
		      pine_mail_close(stream);

		    intrptd = form = 0;
		}
		else{
		    if(stream != ps_global->mail_stream)
		      pine_mail_close(stream);

		    if(ret == 'x'){
			q_status_message(SM_ORDER, 0, 3,
					 "Composition cancelled");
			done++;
		    }
		}
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  "Can't open Postponed mailbox: %.200s", mbox);
		if(stream)
		  pine_mail_close(stream);
	    }
	}
	else{
	  if(F_ON(F_ALT_COMPOSE_MENU, ps_global)){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			      "Postponed message folder doesn't exist!");
	    return;
	  }
	}
	if(fullname)
	  fs_give((void **) &fullname);

	if(done)
	  return;
    }
    if(form && !outgoing){
	CONTEXT_S *p_cntxt = NULL;
	char	  *mbox, *p, *q, tmp[MAILTMPLEN], *fullname = NULL;
	int	   ret = 'n', exists, done = 0;

	/*
	 * find default context to look for folder...
	 *
	 * The "mbox" is assumed to be local if we're given what looks
	 * like an absolute path.  This is different from Goto/Save
	 * where we do alot of work to interpret paths relative to the
	 * server.  This reason is to support all the pre-4.00 pinerc'
	 * that specified a path and because there's yet to be a way
	 * in c-client to specify otherwise in the face of a remote
	 * context.
	 */
	if(!is_absolute_path(mbox = ps_global->VAR_FORM_FOLDER)
	   && !(p_cntxt = default_save_context(ps_global->context_list)))
	  p_cntxt = ps_global->context_list;

	/* check to see if the folder exists, the user wants to continue
	 * and that we can actually read something in...
	 */
	exists = folder_name_exists(p_cntxt, mbox, &fullname);
	if(fullname)
	  mbox = fullname;

	if(exists & FEX_ISFILE){
	    context_apply(tmp, p_cntxt, mbox, sizeof(tmp));
	    if(!(IS_REMOTE(tmp) || is_absolute_path(tmp))){
		/*
		 * The mbox is relative to the home directory.
		 * Make it absolute so we can compare it to
		 * stream->mailbox.
		 */
		build_path(tmp_20k_buf, ps_global->ui.homedir, tmp,
			   SIZEOF_20KBUF);
		strncpy(tmp, tmp_20k_buf, sizeof(tmp));
		tmp[sizeof(tmp)-1] = '\0';
	    }

	    if((stream = ps_global->mail_stream)
	       && !(stream->mailbox
		    && ((*tmp != '{' && !strcmp(tmp, stream->mailbox))
			|| (*tmp == '{'
			    && same_stream(tmp, stream)
			    && (p = strchr(tmp, '}'))
			    && (q = strchr(stream->mailbox,'}'))
			    && !strcmp(p + 1, q + 1)))))
	       stream = NULL;

	    if(stream
	       || ((stream = context_open(p_cntxt,NULL,mbox,
					  FCC_STREAM_MODE|SP_USEPOOL|SP_TEMPUSE,
					  NULL))
		   && !stream->halfopen)){
		if(stream->nmsgs > 0L){
		    if(F_ON(F_ALT_COMPOSE_MENU, ps_global) ||
		       (ret = want_to(FORM_PMT,'y','x',NO_HELP,WT_NORM))=='y'){
			if(!redraft(&stream, &outgoing, &body, &fcc, &lcc,
				    &reply,&redraft_pos,&custom,NULL,
				    REDRAFT_NONE))
			    done++;

			/* stream may or may not be closed in redraft() */
			if(stream && (stream != ps_global->mail_stream))
			  pine_mail_close(stream);

			intrptd = postponed = 0;
		    }
		    else{
			if(stream != ps_global->mail_stream)
			  pine_mail_close(stream);

			if(ret == 'x'){
			    q_status_message(SM_ORDER, 0, 3,
					     "Composition cancelled");
			    done++;
			}
		    }
		}
		else if(stream != ps_global->mail_stream)
		  pine_mail_close(stream);
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
			      "Can't open form letter folder: %.200s", mbox);
		if(stream)
		  pine_mail_close(stream);
	    }
	}
	else{
	  if(F_ON(F_ALT_COMPOSE_MENU, ps_global)){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			      "Form letter folder doesn't exist!");
	    return;
	  }
	}
	if(fullname)
	  fs_give((void **) &fullname);

	if(done)
	  return;
    }

    /*-- normal composition --*/
    if(!outgoing){
	int impl, template_len = 0;
	long rflags = ROLE_COMPOSE;
	PAT_STATE dummy;

        /*=================  Compose new message ===============*/
        body         = mail_newbody();
        outgoing     = mail_newenvelope();

        if(given_to)
	  rfc822_parse_adrlist(&outgoing->to, given_to, ps_global->maildomain);

        outgoing->message_id = generate_message_id();

	/*
	 * Setup possible role
	 */
	if(role_arg)
	  role = copy_action(role_arg);

	if(!role){
	    /* Setup possible compose role */
	    if(nonempty_patterns(rflags, &dummy)){
		/*
		 * setup default role
		 * Msgno = -1 means there is no msg.
		 * This will match roles which have the Compose Use turned
		 * on, and have no patterns set, and match the Current
		 * Folder Type.
		 */
		role = set_role_from_msg(ps_global, rflags, -1L, NULL);

		if(confirm_role(rflags, &role))
		  role = combine_inherited_role(role);
		else{				/* cancel reply */
		    role = NULL;
		    cmd_cancelled("Composition");
		    return;
		}
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4, "Composing using role \"%.200s\"",
			    role->nick);

	/*
	 * The type of storage object allocated below is vitally
	 * important.  See SIMPLIFYING ASSUMPTION #37
	 */
	if(body->contents.text.data = (void *) so_get(PicoText,
						      NULL, EDIT_ACCESS)){
	    char ch;

	    if(inc_text_getc){
		while((*inc_text_getc)(&ch))
		  if(!so_writec(ch, (STORE_S *)body->contents.text.data)){
		      break;
		  }
	    }
	}
	else{
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Problem creating space for message text.");
	    return;
	}

	if(role && role->template){
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

	if(sig = detoken(role, NULL, 2, 0, 1, &redraft_pos, &impl)){
	    if(impl == 2)
	      redraft_pos->offset += template_len;

	    if(*sig)
	      so_puts((STORE_S *)body->contents.text.data, sig);

	    fs_give((void **)&sig);
	}

	body->type = TYPETEXT;

	if(attach)
	  create_message_body(&body, attach, NULL, 0);
    }

    ps_global->prev_screen = compose_screen;
    if(!(fcc_to_free = fcc) && !(role && role->fcc))
      fcc = fcc_arg;			/* Didn't pick up fcc, use given  */

    /*
     * check whether a build_address-produced fcc is different from
     * fcc.  If same, do nothing, if different, set sticky bit in pine_send.
     */
    if(fcc){
	char *tmp_fcc = NULL;

	if(outgoing->to){
	    tmp_fcc = get_fcc_based_on_to(outgoing->to);
	    if(strcmp(fcc, tmp_fcc ? tmp_fcc : ""))
	      fcc_is_sticky++;  /* cause sticky bit to get set */

	}
	else if((tmp_fcc = get_fcc(NULL)) != NULL &&
	        !strcmp(fcc, tmp_fcc)){
	    /* not sticky */
	}
	else
	  fcc_is_sticky++;
	    
	if(tmp_fcc)
	  fs_give((void **)&tmp_fcc);
    }

    pine_send(outgoing, &body, COMPOSE_MAIL_TITLE, role, fcc,
	      reply, redraft_pos, lcc, custom, fcc_is_sticky);

    if(reply){
	if(reply->mailbox)
	  fs_give((void **) &reply->mailbox);
	if(reply->origmbox)
	  fs_give((void **) &reply->origmbox);
	if(reply->prefix)
	  fs_give((void **) &reply->prefix);
	if(reply->data.uid.msgs)
	  fs_give((void **) &reply->data.uid.msgs);
	fs_give((void **) &reply);
    }

    if(fcc_to_free)
      fs_give((void **)&fcc_to_free);

    if(lcc)
      fs_give((void **)&lcc);

    mail_free_envelope(&outgoing);
    pine_free_body(&body);
    free_redraft_pos(&redraft_pos);
    free_action(&role);
}


/*----------------------------------------------------------------------
 Args:	stream -- This is where we get the postponed messages from
		  We'll expunge and close it here unless it is mail_stream.

    These are all return values:
    ================
	outgoing -- 
	body -- 
	fcc -- 
	lcc -- 
	reply -- 
	redraft_pos -- 
	custom -- 
	role -- 
    ================

	flags --
 
 ----*/
int
redraft(streamp, outgoing, body, fcc, lcc, reply, redraft_pos, custom, role,
	flags)
    MAILSTREAM	  **streamp;
    ENVELOPE	  **outgoing;
    BODY	  **body;
    char	  **fcc;
    char	  **lcc;
    REPLY_S	  **reply;
    REDRAFT_POS_S **redraft_pos;
    PINEFIELD	  **custom;
    ACTION_S      **role;
    int		    flags;
{
    MAILSTREAM  *stream;
    ENVELOPE	*e = NULL;
    BODY	*b;
    PART        *part;
    STORE_S	*so = NULL;
    PINEFIELD   *pf;
    gf_io_t	 pc;
    char	*extras, **fields, **values, *p;
    char        *hdrs[2], *h;
    char       **smtp_servers = NULL, **nntp_servers = NULL;
    long	 cont_msg = 1L;
    int		 i, pine_generated = 0, our_replyto = 0;
    int          added_to_role = 0;
    MESSAGECACHE *mc;


    if(!(streamp && *streamp))
      return(0);
    
    stream = *streamp;

    /*
     * If we're manipulating the current folder, don't bother
     * with index
     */
    if(!stream->nmsgs){
	q_status_message1(SM_ORDER | SM_DING, 3, 5,
			  "Empty folder!  No messages really %.200s!",
			  (REDRAFT_PPND&flags) ? "postponed" : "interrupted");
	return(redraft_cleanup(streamp, FALSE, flags));
    }
    else if(stream == ps_global->mail_stream){
	/*
	 * Since the user's got this folder already opened and they're
	 * on a selected message, pick that one rather than rebuild
	 * another index screen...
	 */
	cont_msg = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));
    }
    else if(stream->nmsgs > 1L){		/* offer browser ? */
	int	 rv;

	mn_set_cur(sp_msgmap(stream),
		   (REDRAFT_PPND&flags) ? stream->nmsgs : 1L);

	clear_index_cache();
	while(1){
	    void *ti;

	    ti = stop_threading_temporarily();
	    rv = index_lister(ps_global, NULL, stream->mailbox,
			      stream, sp_msgmap(stream));
	    restore_threading(&ti);

	    cont_msg = mn_get_cur(sp_msgmap(stream));
	    if(count_flagged(stream, F_DEL)
	       && want_to(INTR_DEL_PMT, 'n', 0, NO_HELP, WT_NORM) == 'n'){
		q_status_message1(SM_ORDER, 3, 3,
				  "Undelete %.200s, and then continue message",
				  (REDRAFT_PPND&flags)
				    ? "messages to remain postponed"
				    : "form letters you want to keep");
		continue;
	    }

	    break;
	}

	clear_index_cache();

	if(rv){
	    q_status_message(SM_ORDER, 0, 3, "Composition cancelled");
	    (void) redraft_cleanup(streamp, FALSE, flags);
	    return(0);				/* special case */
	}
    }

    /* grok any user-defined or non-c-client headers */
    if((e = pine_mail_fetchstructure(stream, cont_msg, &b))
       && (so = (void *)so_get(PicoText, NULL, EDIT_ACCESS))){

	/*
	 * The custom headers to look for in the suspended message should
	 * have been stored in the X-Our-Headers header. So first we get
	 * that list. If we can't find it (version that stored the
	 * message < 4.30) then we use the global custom list.
	 */
	hdrs[0] = OUR_HDRS_LIST;
	hdrs[1] = NULL;
	if((h = pine_fetchheader_lines(stream, cont_msg, NULL, hdrs)) != NULL){
	    int    commas = 0;
	    char **list;
	    char  *hdrval = NULL;

	    if((hdrval = strindex(h, ':')) != NULL){
		for(hdrval++; *hdrval && isspace((unsigned char)*hdrval); 
		    hdrval++)
		  ;
	    }

	    /* count elements in list */
	    for(p = hdrval; p && *p; p++)
	      if(*p == ',')
		commas++;

	    if(hdrval && (list = parse_list(hdrval,commas+1,0,NULL)) != NULL){
		
		*custom = parse_custom_hdrs(list, Replace);
		add_defaults_from_list(*custom,
				       ps_global->VAR_CUSTOM_HDRS);
		free_list_array(&list);
	    }

	    if(*custom && !(*custom)->name){
		free_customs(*custom);
		*custom = NULL;
	    }

	    fs_give((void **)&h);
	}

	if(!*custom)
	  *custom = parse_custom_hdrs(ps_global->VAR_CUSTOM_HDRS, UseAsDef);

#define INDEX_FCC         0
#define INDEX_POSTERR     1
#define INDEX_REPLYUID    2
#define INDEX_REPLYMBOX   3
#define INDEX_SMTP        4
#define INDEX_NNTP        5
#define INDEX_CURSORPOS   6
#define INDEX_OUR_REPLYTO 7
#define INDEX_LCC         8	/* MUST REMAIN LAST FIELD DECLARED */
#define FIELD_COUNT       9

	i = count_custom_hdrs_pf(*custom,1) + FIELD_COUNT + 1;

	/*
	 * Having these two fields separated isn't the slickest, but
	 * converting the pointer array for fetchheader_lines() to
	 * a list of structures or some such for simple_header_parse()
	 * is too goonie.  We could do something like re-use c-client's
	 * PARAMETER struct which is a simple char * pairing, but that
	 * doesn't make sense to pass to fetchheader_lines()...
	 */
	fields = (char **) fs_get((size_t) i * sizeof(char *));
	values = (char **) fs_get((size_t) i * sizeof(char *));
	memset(fields, 0, (size_t) i * sizeof(char *));
	memset(values, 0, (size_t) i * sizeof(char *));

	fields[i = INDEX_FCC] = "Fcc";	/* Fcc: special case */
	fields[++i]   = "X-Post-Error";	/* posting errors too */
	fields[++i]   = "X-Reply-UID";	/* Reply'd to msg's UID */
	fields[++i]   = "X-Reply-Mbox";	/* Reply'd to msg's Mailbox */
	fields[++i]   = "X-SMTP-Server";/* SMTP server to use */
	fields[++i]   = "X-NNTP-Server";/* NNTP server to use */
	fields[++i]   = "X-Cursor-Pos";	/* Cursor position */
	fields[++i]   = "X-Our-ReplyTo";	/* ReplyTo is real */
	fields[++i]   = "Lcc";		/* Lcc: too... */
	if(++i != FIELD_COUNT)
	  panic("Fix FIELD_COUNT");

	for(pf = *custom; pf && pf->name; pf = pf->next)
	  if(!pf->standard)
	    fields[i++] = pf->name;		/* assign custom fields */

	if(extras = pine_fetchheader_lines(stream, cont_msg, NULL,fields)){
	    simple_header_parse(extras, fields, values);
	    fs_give((void **) &extras);

	    /*
	     * translate RFC 1522 strings,
	     * starting with "Lcc" field
	     */
	    for(i = INDEX_LCC; fields[i]; i++)
	      if(values[i]){
		  size_t len;
		  char *charset;
		  char *bufp, *biggerbuf = NULL;

		  if((len=strlen(values[i])) > SIZEOF_20KBUF-1){
		      len++;
		      biggerbuf = (char *)fs_get(len * sizeof(char));
		      bufp = biggerbuf;
		  }
		  else{
		      bufp = tmp_20k_buf;
		      len = SIZEOF_20KBUF;
		  }

		  p = (char *)rfc1522_decode((unsigned char*)bufp,
					     len, values[i], &charset);
		  if(charset)
		    fs_give((void **)&charset);

		  if(p == tmp_20k_buf){
		      fs_give((void **)&values[i]);
		      values[i] = cpystr(p);
		  }

		  if(biggerbuf)
		    fs_give((void **)&biggerbuf);
	      }

	    for(pf = *custom, i = FIELD_COUNT;
		pf && pf->name;
		pf = pf->next){
		if(pf->standard){
		    /*
		     * Because the value is already in the envelope.
		     */
		    pf->cstmtype = NoMatch;
		    continue;
		}

		if(values[i]){	/* use this instead of default */
		    if(pf->textbuf)
		      fs_give((void **)&pf->textbuf);

		    pf->textbuf = values[i]; /* freed in pine_send! */
		}
		else if(pf->textbuf)  /* was erased before postpone */
		  fs_give((void **)&pf->textbuf);
		
		i++;
	    }

	    if(values[INDEX_FCC])		/* If "Fcc:" was there...  */
	      pine_generated = 1;		/* we put it there? */

	    /*
	     * Since c-client fills in the reply_to field in the envelope
	     * even if there isn't a Reply-To header in the message we
	     * have to work around that. When we postpone we add
	     * a second header that has value "Empty" if there really
	     * was a Reply-To and it was empty. It has the
	     * value "Full" if we put the Reply-To contents there
	     * intentionally (and it isn't empty).
	     */
	    if(values[INDEX_OUR_REPLYTO]){
		if(values[INDEX_OUR_REPLYTO][0] == 'E')
		  our_replyto = 'E';  /* we put an empty one there */
		else if(values[INDEX_OUR_REPLYTO][0] == 'F')
		  our_replyto = 'F';  /* we put it there */

		fs_give((void **) &values[INDEX_OUR_REPLYTO]);
	    }

	    if(fcc)				/* fcc: special case... */
	      *fcc = values[INDEX_FCC] ? values[INDEX_FCC] : cpystr("");
	    else if(values[INDEX_FCC])
	      fs_give((void **) &values[INDEX_FCC]);

	    if(values[INDEX_POSTERR]){		/* x-post-error?!?1 */
		q_status_message(SM_ORDER|SM_DING, 4, 4,
				 values[INDEX_POSTERR]);
		fs_give((void **) &values[INDEX_POSTERR]);
	    }

	    if(values[INDEX_REPLYUID]){
		if(reply)
		  *reply = build_reply_uid(values[INDEX_REPLYUID]);

		fs_give((void **) &values[INDEX_REPLYUID]);

		if(values[INDEX_REPLYMBOX] && reply && *reply)
		  (*reply)->origmbox = cpystr(values[INDEX_REPLYMBOX]);
		
		if(reply && *reply && !(*reply)->origmbox && (*reply)->mailbox)
		  (*reply)->origmbox = cpystr((*reply)->mailbox);
	    }

	    if(values[INDEX_REPLYMBOX])
	      fs_give((void **) &values[INDEX_REPLYMBOX]);

	    if(values[INDEX_SMTP]){
		char  *q;
		char **lp;
		size_t cnt = 0;

		/*
		 * Turn the space delimited list of smtp servers into
		 * a char ** list.
		 */
		p = values[INDEX_SMTP];
		do{
		    if(!*p || isspace((unsigned char) *p))
		      cnt++;
		} while(*p++);
		
		smtp_servers = (char **) fs_get((cnt+1) * sizeof(char *));
		memset(smtp_servers, 0, (cnt+1) * sizeof(char *));

		cnt = 0;
		q = p = values[INDEX_SMTP];
		do{
		    if(!*p || isspace((unsigned char) *p)){
			if(*p){
			    *p = '\0';
			    smtp_servers[cnt++] = cpystr(q);
			    *p = ' ';
			    q = p+1;
			}
			else
			  smtp_servers[cnt++] = cpystr(q);
		    }
		} while(*p++);

		fs_give((void **) &values[INDEX_SMTP]);
	    }

	    if(values[INDEX_NNTP]){
		char  *q;
		char **lp;
		size_t cnt = 0;

		/*
		 * Turn the space delimited list of smtp nntp into
		 * a char ** list.
		 */
		p = values[INDEX_NNTP];
		do{
		    if(!*p || isspace((unsigned char) *p))
		      cnt++;
		} while(*p++);
		
		nntp_servers = (char **) fs_get((cnt+1) * sizeof(char *));
		memset(nntp_servers, 0, (cnt+1) * sizeof(char *));

		cnt = 0;
		q = p = values[INDEX_NNTP];
		do{
		    if(!*p || isspace((unsigned char) *p)){
			if(*p){
			    *p = '\0';
			    nntp_servers[cnt++] = cpystr(q);
			    *p = ' ';
			    q = p+1;
			}
			else
			  nntp_servers[cnt++] = cpystr(q);
		    }
		} while(*p++);

		fs_give((void **) &values[INDEX_NNTP]);
	    }

	    if(values[INDEX_CURSORPOS]){
		/*
		 * The redraft cursor position is written as two fields
		 * separated by a space. First comes the name of the
		 * header field we're in, or just a ":" if we're in the
		 * body. Then comes the offset into that header or into
		 * the body.
		 */
		if(redraft_pos){
		    char *q1, *q2;

		    *redraft_pos
			      = (REDRAFT_POS_S *)fs_get(sizeof(REDRAFT_POS_S));
		    (*redraft_pos)->offset = 0L;

		    q1 = skip_white_space(values[INDEX_CURSORPOS]);
		    if(*q1 && (q2 = strindex(q1, SPACE))){
			*q2 = '\0';
			(*redraft_pos)->hdrname = cpystr(q1);
			q1 = skip_white_space(q2+1);
			if(*q1)
			  (*redraft_pos)->offset = atol(q1);
		    }
		    else
		      (*redraft_pos)->hdrname = cpystr(":");
		}

		fs_give((void **) &values[INDEX_CURSORPOS]);
	    }

	    if(lcc)
	      *lcc = values[INDEX_LCC];
	    else
	      fs_give((void **) &values[INDEX_LCC]);
	}

	fs_give((void **)&fields);
	fs_give((void **)&values);

	*outgoing = copy_envelope(e);

	/*
	 * If the postponed message has a From which is different from
	 * the default, it is either because allow-changing-from is on
	 * or because there was a role with a from that allowed it to happen.
	 * If allow-changing-from is not on, put this back in a role
	 * so that it will be allowed again in pine_send.
	 */
	if(role && *role == NULL &&
	   !ps_global->never_allow_changing_from &&
	   *outgoing){
	    /*
	     * Now check to see if the from is different from default from.
	     */
	    ADDRESS *deffrom;

	    deffrom = generate_from();
	    if(!((*outgoing)->from &&
		 address_is_same(deffrom, (*outgoing)->from) &&
	         ((!(deffrom->personal && deffrom->personal[0]) &&
		   !((*outgoing)->from->personal &&
		     (*outgoing)->from->personal[0])) ||
		  (deffrom->personal && (*outgoing)->from->personal &&
		   !strcmp(deffrom->personal, (*outgoing)->from->personal))))){

		*role = (ACTION_S *)fs_get(sizeof(**role));
		memset((void *)*role, 0, sizeof(**role));
		if(!(*outgoing)->from)
		  (*outgoing)->from = mail_newaddr();

		(*role)->from = (*outgoing)->from;
		(*outgoing)->from = NULL;
		added_to_role++;
	    }

	    mail_free_address(&deffrom);
	}

	/*
	 * Look at each empty address and see if the user has specified
	 * a default for that field or not.  If they have, that means
	 * they have erased it before postponing, so they won't want
	 * the default to come back.  If they haven't specified a default,
	 * then the default should be generated in pine_send.  We prevent
	 * the default from being assigned by assigning an empty address
	 * to the variable here.
	 *
	 * BUG: We should do this for custom Address headers, too, but
	 * there isn't such a thing yet.
	 */
	if(!(*outgoing)->to && hdr_is_in_list("to", *custom))
	  (*outgoing)->to = mail_newaddr();
	if(!(*outgoing)->cc && hdr_is_in_list("cc", *custom))
	  (*outgoing)->cc = mail_newaddr();
	if(!(*outgoing)->bcc && hdr_is_in_list("bcc", *custom))
	  (*outgoing)->bcc = mail_newaddr();

	if(our_replyto == 'E'){
	    /* user erased reply-to before postponing */
	    if((*outgoing)->reply_to)
	      mail_free_address(&(*outgoing)->reply_to);

	    /*
	     * If empty is not the normal default, make the outgoing
	     * reply_to be an emtpy address. If it is default, leave it
	     * as NULL and the default will be used.
	     */
	    if(hdr_is_in_list("reply-to", *custom)){
		PINEFIELD pf;

		pf.name = "reply-to";
		set_default_hdrval(&pf, *custom);
		if(pf.textbuf){
		    if(pf.textbuf[0])	/* empty is not default */
		      (*outgoing)->reply_to = mail_newaddr();

		    fs_give((void **)&pf.textbuf);
		}
	    }
	}
	else if(our_replyto == 'F'){
	    int add_to_role = 0;

	    /*
	     * The reply-to is real. If it is different from the default
	     * reply-to, put it in the role so that it will show up when
	     * the user edits.
	     */
	    if(hdr_is_in_list("reply-to", *custom)){
		PINEFIELD pf;
		char *str;

		pf.name = "reply-to";
		set_default_hdrval(&pf, *custom);
		if(pf.textbuf && pf.textbuf[0]){
		    if(str = addr_list_string((*outgoing)->reply_to,NULL,0,1)){
			if(!strcmp(str, pf.textbuf)){
			    /* standard value, leave it alone */
			    ;
			}
			else  /* not standard, put in role */
			  add_to_role++;

			fs_give((void **)&str);
		    }
		}
		else  /* not standard, put in role */
		  add_to_role++;

		if(pf.textbuf)
		  fs_give((void **)&pf.textbuf);
	    }
	    else  /* not standard, put in role */
	      add_to_role++;

	    if(add_to_role && role && (*role == NULL || added_to_role)){
		if(*role == NULL){
		    added_to_role++;
		    *role = (ACTION_S *)fs_get(sizeof(**role));
		    memset((void *)*role, 0, sizeof(**role));
		}

		(*role)->replyto = (*outgoing)->reply_to;
		(*outgoing)->reply_to = NULL;
	    }
	}
	else{
	    /* this is a bogus c-client generated replyto */
	    if((*outgoing)->reply_to)
	      mail_free_address(&(*outgoing)->reply_to);
	}

	if((smtp_servers || nntp_servers)
	   && role && (*role == NULL || added_to_role)){
	    if(*role == NULL){
		*role = (ACTION_S *)fs_get(sizeof(**role));
		memset((void *)*role, 0, sizeof(**role));
	    }

	    if(smtp_servers)
	      (*role)->smtp = smtp_servers;
	    if(nntp_servers)
	      (*role)->nntp = nntp_servers;
	}

	if(!(*outgoing)->subject && hdr_is_in_list("subject", *custom))
	  (*outgoing)->subject = cpystr("");

	if(!pine_generated){
	    /*
	     * Now, this is interesting.  We should have found 
	     * the "fcc:" field if pine wrote the message being
	     * redrafted.  Hence, we probably can't trust the 
	     * "originator" type fields, so we'll blast them and let
	     * them get set later in pine_send.  This should allow
	     * folks with custom or edited From's and such to still
	     * use redraft reasonably, without inadvertently sending
	     * messages that appear to be "From" others...
	     */
	    if((*outgoing)->from)
	      mail_free_address(&(*outgoing)->from);

	    /*
	     * Ditto for Reply-To and Sender...
		 */
	    if((*outgoing)->reply_to)
	      mail_free_address(&(*outgoing)->reply_to);

	    if((*outgoing)->sender)
	      mail_free_address(&(*outgoing)->sender);
	}

	if(!pine_generated || !(flags & REDRAFT_DEL)){

	    /*
	     * Generate a fresh message id for pretty much the same
	     * reason From and such got wacked...
	     * Also, if we're coming from a form letter, we need to
	     * generate a different id each time.
	     */
	    if((*outgoing)->message_id)
	      fs_give((void **)&(*outgoing)->message_id);

	    (*outgoing)->message_id = generate_message_id();
	}

	if(b && b->type != TYPETEXT)
	  if(b->type == TYPEMULTIPART){
	      if(strucmp(b->subtype, "mixed")){
		  q_status_message1(SM_INFO, 3, 4, 
			     "Converting Multipart/%.200s to Multipart/Mixed", 
				 b->subtype);
		  fs_give((void **)&b->subtype);
		  b->subtype = cpystr("mixed");
	      }
	  }
	  else{
	      q_status_message2(SM_ORDER | SM_DING, 3, 4, 
				"Unable to resume type %.200s/%.200s message",
				body_types[b->type], b->subtype);
	      return(redraft_cleanup(streamp, TRUE, flags));
	  }
		
	gf_set_so_writec(&pc, so);

	if(b && b->type != TYPETEXT){	/* already TYPEMULTIPART */
	    *body			   = copy_body(NULL, b);
	    part			   = (*body)->nested.part;
	    part->body.contents.text.data = (void *)so;
	    set_mime_type_by_grope(&part->body, NULL);
	    if(part->body.type != TYPETEXT){
		q_status_message2(SM_ORDER | SM_DING, 3, 4,
		      "Unable to resume; first part is non-text: %.200s/%.200s",
				  body_types[part->body.type], 
				  part->body.subtype);
		return(redraft_cleanup(streamp, TRUE, flags));
	    }
		
	    ps_global->postpone_no_flow = 1;
	    get_body_part_text(stream, &b->nested.part->body,
			       cont_msg, "1", pc, NULL);
	    ps_global->postpone_no_flow = 0;

	    if(!fetch_contents(stream, cont_msg, NULL, *body))
	      q_status_message(SM_ORDER | SM_DING, 3, 4,
			       "Error including all message parts");
	}
	else{
	    *body			 = mail_newbody();
	    (*body)->type		 = TYPETEXT;
	    (*body)->contents.text.data = (void *)so;
	    ps_global->postpone_no_flow = 1;
	    get_body_part_text(stream, b, cont_msg, "1", pc, NULL);
	    ps_global->postpone_no_flow = 0;
	}

	gf_clear_so_writec(so);

	    /* We have what we want, blast this message... */
	if((flags & REDRAFT_DEL)
	   && cont_msg > 0L && stream && cont_msg <= stream->nmsgs
	   && (mc = mail_elt(stream, cont_msg)) && !mc->deleted)
	  mail_flag(stream, long2string(cont_msg), "\\DELETED", ST_SET);
    }
    else
      return(redraft_cleanup(streamp, TRUE, flags));

    return(redraft_cleanup(streamp, FALSE, flags));
}



/*----------------------------------------------------------------------
    Clear deleted messages from given stream and expunge if necessary

Args:	stream --
	problem --

 ----*/
int
redraft_cleanup(streamp, problem, flags)
    MAILSTREAM **streamp;
    int		 problem;
    int          flags;
{
    MAILSTREAM *stream;

    if(!(streamp && *streamp))
      return(0);

    if(!problem && streamp && (stream = *streamp)){
	if(stream->nmsgs){
	    ps_global->expunge_in_progress = 1;
	    mail_expunge(stream);		/* clean out deleted */
	    ps_global->expunge_in_progress = 0;
	}

	if(!stream->nmsgs){			/* close and delete folder */
	    int do_the_broach = 0;
	    char *mbox = cpystr(stream->mailbox);

	    /* if it is current, we have to change folders */
	    if(stream == ps_global->mail_stream)
	      do_the_broach++;

	    /*
	     * This is the stream to the empty postponed-msgs folder.
	     * We are going to delete the folder in a second. It is
	     * probably preferable to unselect the mailbox and leave
	     * this stream open for re-use instead of actually closing it,
	     * so we do that if possible.
	     */
	    if(is_imap_stream(stream) && LEVELUNSELECT(stream)){
		/* this does the UNSELECT on the stream */
		mail_open(stream, stream->mailbox,
			  OP_HALFOPEN | (stream->debug ? OP_DEBUG : NIL));
		/* now close it so it is put into the stream cache */
		sp_set_flags(stream, sp_flags(stream) | SP_TEMPUSE);
		pine_mail_close(stream);
	    }
	    else
	      pine_mail_actually_close(stream);

	    *streamp = NULL;

	    if(do_the_broach){
		q_status_message2(SM_ORDER, 3, 7,
			     "No more %.200s, returning to \"%.200s\"",
			     (REDRAFT_PPND&flags) ? "postponed messages"
						  : "form letters",
			     ps_global->inbox_name);
		ps_global->mail_stream = NULL;	/* already closed above */
		do_broach_folder(ps_global->inbox_name,
				 ps_global->context_list, NULL, 0L);

		ps_global->next_screen = mail_index_screen;
	    }

	    if(!pine_mail_delete(NULL, mbox))
	      q_status_message1(SM_ORDER|SM_DING, 3, 3,
				"Can't delete %.200s", mbox);

	    fs_give((void **) &mbox);
	}
    }

    return(!problem);
}



int
redraft_prompt(type, prompt, failure)
    char *type, *prompt;
    int	failure;
{
    if(background_posting(FALSE)){
	q_status_message1(SM_ORDER, 0, 3,
			  "%.200s folder unavailable while background posting",
			  type);
	return(failure);
    }

    return(want_to(prompt, 'y', 'x', NO_HELP, WT_NORM));
}



/*----------------------------------------------------------------------
    Parse the given header text for any given fields

Args:  text -- Text to parse for fcc and attachments refs
       fields -- array of field names to look for
       values -- array of pointer to save values to, returned NULL if
                 fields isn't in text.

This function simply looks for the given fields in the given header
text string.
NOTE: newlines are expected CRLF, and we'll ignore continuations
 ----*/
void
simple_header_parse(text, fields, values)
    char   *text, **fields, **values;
{
    int   i, n;
    char *p, *t;

    for(i = 0; fields[i]; i++)
      values[i] = NULL;				/* clear values array */

    /*---- Loop until the end of the header ----*/
    for(p = text; *p; ){
	for(i = 0; fields[i]; i++)		/* find matching field? */
	  if(!struncmp(p, fields[i], (n=strlen(fields[i]))) && p[n] == ':'){
	      for(p += n + 1; *p; p++){		/* find start of value */
		  if(*p == '\015' && *(p+1) == '\012'
		     && !isspace((unsigned char) *(p+2)))
		    break;

		  if(!isspace((unsigned char) *p))
		    break;			/* order here is key... */
	      }

	      if(!values[i]){			/* if we haven't already */
		  values[i] = fs_get(strlen(text) + 1);
		  values[i][0] = '\0';		/* alloc space for it */
	      }

	      if(*p && *p != '\015'){		/* non-blank value. */
		  t = values[i] + (values[i][0] ? strlen(values[i]) : 0);
		  while(*p){			/* check for cont'n lines */
		      if(*p == '\015' && *(p+1) == '\012'){
			  if(isspace((unsigned char) *(p+2))){
			      p += 3;
			      continue;
			  }
			  else
			    break;
		      }

		      *t++ = *p++;
		  }

		  *t = '\0';
	      }

	      break;
	  }

        /* Skip to end of line, what ever it was */
        for(; *p ; p++)
	  if(*p == '\015' && *(p+1) == '\012'){
	      p += 2;
	      break;
	  }
    }
}

 
/*----------------------------------------------------------------------
    build a fresh REPLY_S from the given string (see pine_send for format)

Args:	s -- "X-Reply-UID" header value

Returns: filled in REPLY_S or NULL on parse error
 ----*/
REPLY_S *
build_reply_uid(s)
    char *s;
{
    char    *p, *prefix, *val, *seq, *mbox;
    int	     i, nseq;
    REPLY_S *reply = NULL;

    /* FORMAT: (n prefix)(n validity uidlist)mailbox */
    if(*s == '('){
	for(p = s + 1; isdigit(*p); p++)
	  ;

	if(*p == ' '){
	    *p++ = '\0';
	    if((i = atoi(s+1)) && i < strlen(p)){
		prefix = p;
		p += i;
		if(*p == ')'){
		    *p++ = '\0';
		    if(*p++ == '(' && *p){
			for(seq = p; isdigit(*p); p++)
			  ;

			if(*p == ' '){
			    *p++ = '\0';
			    for(val = p; isdigit(*p); p++)
			      ;

			    if(*p == ' '){
				*p++ = '\0';

				if((nseq = atoi(seq)) && isdigit(*(seq = p))
				   && (p = strchr(p, ')')) && *(mbox = ++p)){
				    unsigned long *uidl;

				    uidl = (unsigned long *) fs_get
					      ((nseq+1)*sizeof(unsigned long));
				    for(i = 0; i < nseq; i++)
				      if(p = strchr(seq,',')){
					  *p = '\0';
					  if(uidl[i]= strtoul(seq,NULL,10))
					    seq = ++p;
					  else
					    break;
				      }
				      else if(p = strchr(seq, ')')){
					  if(uidl[i]= strtoul(seq,NULL,10))
					    i++;

					  break;
				      }

				    if(i == nseq){
					reply =
					    (REPLY_S *)fs_get(sizeof(REPLY_S));
					memset(reply, 0, sizeof(REPLY_S));
					reply->flags	     = REPLY_UID;
					reply->data.uid.validity
						      = strtoul(val, NULL, 10);
					reply->prefix	     = cpystr(prefix);
					reply->mailbox	     = cpystr(mbox);
					uidl[nseq]	     = 0;
					reply->data.uid.msgs = uidl;
				    }
				    else
				      fs_give((void **) &uidl);
				}
			    }
			}
		    }
		}
	    }
	}
    }

    return(reply);
}


#if defined(DOS) || defined(OS2)
/*----------------------------------------------------------------------
    Verify that the necessary pieces are around to allow for
    message sending under DOS

Args:  strict -- tells us if a remote stream is required before
		 sending is permitted.

The idea is to make sure pine knows enough to put together a valid 
from line.  The things we MUST know are a user-id, user-domain and
smtp server to dump the message off on.  Typically these are 
provided in pine's configuration file, but if not, the user is
queried here.
 ----*/
int
dos_valid_from()
{
    char        prompt[100], answer[80], *p;
    int         rc, i, flags;
    HelpType    help;

    /*
     * query for user name portion of address, use IMAP login
     * name as default
     */
    if(!ps_global->VAR_USER_ID || ps_global->VAR_USER_ID[0] == '\0'){
	NETMBX mb;
	int no_prompt_user_id = 0;

	if(ps_global->mail_stream && ps_global->mail_stream->mailbox
	   && mail_valid_net_parse(ps_global->mail_stream->mailbox, &mb)
	   && *mb.user){
	    strncpy(answer, mb.user, sizeof(answer)-1);
	    answer[sizeof(answer)-1] = '\0';
	}
	else if(F_ON(F_QUELL_USER_ID_PROMPT, ps_global)){
	    /* no user-id prompting if set */
	    no_prompt_user_id = 1;
	    rc = 0;
	    if(!ps_global->mail_stream)
	      do_broach_folder(ps_global->inbox_name,
			       ps_global->context_list, NULL, 0L);
	    if(ps_global->mail_stream && ps_global->mail_stream->mailbox
	       && mail_valid_net_parse(ps_global->mail_stream->mailbox, &mb)
	       && *mb.user){
		strncpy(answer, mb.user, sizeof(answer)-1);
		answer[sizeof(answer)-1] = '\0';
	    }
	    else
	      answer[0] = '\0';
	}
	else
	  answer[0] = '\0';

	if(F_ON(F_QUELL_USER_ID_PROMPT, ps_global) && answer[0]){
	    /* No prompt, just assume mailbox login is user-id */
	    no_prompt_user_id = 1; 
	    rc = 0;
	}

	sprintf(prompt,"User-id for From address : "); 

	help = NO_HELP;
	while(!no_prompt_user_id) {
	    flags = OE_APPEND_CURRENT;
	    rc = optionally_enter(answer,-FOOTER_ROWS(ps_global),0,
				  sizeof(answer),prompt,NULL,help,&flags);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_user_id : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
	}

	if(rc == 1 || (rc == 0 && !answer[0])) {
	    q_status_message(SM_ORDER, 3, 4,
		   "Send cancelled (User-id must be provided before sending)");
	    return(0);
	}

	/* save the name */
	sprintf(prompt, "Preserve %.*s as \"user-id\" in PINERC",
		sizeof(prompt)-50, answer);
	if(ps_global->blank_user_id
	   && !no_prompt_user_id
	   && want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
	    set_variable(V_USER_ID, answer, 1, 1, Main);
	}
	else{
            fs_give((void **)&(ps_global->VAR_USER_ID));
	    ps_global->VAR_USER_ID = cpystr(answer);
	}
    }

    /* query for personal name */
    if(!ps_global->VAR_PERSONAL_NAME || ps_global->VAR_PERSONAL_NAME[0]=='\0'
	&& F_OFF(F_QUELL_PERSONAL_NAME_PROMPT, ps_global)){
	answer[0] = '\0';
	sprintf(prompt,"Personal name for From address : "); 

	help = NO_HELP;
	while(1) {
	    flags = OE_APPEND_CURRENT;
	    rc = optionally_enter(answer,-FOOTER_ROWS(ps_global),0,
				  sizeof(answer),prompt,NULL,help,&flags);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_personal_name : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
    	}

	if(rc == 0 && answer){		/* save the name */
	    sprintf(prompt, "Preserve %.*s as \"personal-name\" in PINERC",
		    sizeof(prompt)-50, answer);
	    if(ps_global->blank_personal_name
	       && want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
		set_variable(V_PERSONAL_NAME, answer, 1, 1, Main);
	    }
	    else{
        	fs_give((void **)&(ps_global->VAR_PERSONAL_NAME));
		ps_global->VAR_PERSONAL_NAME = cpystr(answer);
	    }
	}
    }

    /* 
     * query for host/domain portion of address, using IMAP
     * host as default
     */
    if(ps_global->blank_user_domain 
       || ps_global->maildomain == ps_global->localdomain
       || ps_global->maildomain == ps_global->hostname){
	if(ps_global->inbox_name[0] == '{'){
	    for(i=0;
		i < sizeof(answer)-1 && ps_global->inbox_name[i+1] != '}'; i++)
		answer[i] = ps_global->inbox_name[i+1];

	    answer[i] = '\0';
	}
	else
	  answer[0] = '\0';

	sprintf(prompt,"Host/domain for From address : "); 

	help = NO_HELP;
	while(1) {
	    flags = OE_APPEND_CURRENT;
	    rc = optionally_enter(answer,-FOOTER_ROWS(ps_global),0,
				  sizeof(answer),prompt,NULL,help,&flags);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_domain : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
	}

	if(rc == 1 || (rc == 0 && !answer[0])) {
	    q_status_message(SM_ORDER, 3, 4,
	  "Send cancelled (Host/domain name must be provided before sending)");
	    return(0);
	}

	/* save the name */
	sprintf(prompt, "Preserve %.*s as \"user-domain\" in PINERC",
		sizeof(prompt)-50, answer);
	if(!ps_global->userdomain && !ps_global->blank_user_domain
	   && want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
	    set_variable(V_USER_DOMAIN, answer, 1, 1, Main);
	    fs_give((void **)&(ps_global->maildomain));	/* blast old val */
	    ps_global->userdomain      = cpystr(answer);
	    ps_global->maildomain      = ps_global->userdomain;
	}
	else{
            fs_give((void **)&(ps_global->maildomain));
            ps_global->userdomain = cpystr(answer);
	    ps_global->maildomain = ps_global->userdomain;
	}
    }

    /* check for smtp server */
    if(!ps_global->VAR_SMTP_SERVER ||
       !ps_global->VAR_SMTP_SERVER[0] ||
       !ps_global->VAR_SMTP_SERVER[0][0]){
	char **list;

	if(ps_global->inbox_name[0] == '{'){
	    for(i=0;
		i < sizeof(answer)-1 && ps_global->inbox_name[i+1] != '}'; i++)
	      answer[i] = ps_global->inbox_name[i+1];

	    answer[i] = '\0';
	}
	else
          answer[0] = '\0';

        sprintf(prompt,"SMTP server to forward message : "); 

	help = NO_HELP;
        while(1) {
	    flags = OE_APPEND_CURRENT;
            rc = optionally_enter(answer,-FOOTER_ROWS(ps_global),0,
				  sizeof(answer),prompt,NULL,help,&flags);
            if(rc == 2)
                  continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_smtp : NO_HELP;
		continue;
	    }

            if(rc != 4)
                  break;
        }

        if(rc == 1 || (rc == 0 && answer[0] == '\0')) {
            q_status_message(SM_ORDER, 3, 4,
	       "Send cancelled (SMTP server must be provided before sending)");
            return(0);
        }

	/* save the name */
        list    = (char **) fs_get(2 * sizeof(char *));
	list[0] = cpystr(answer);
	list[1] = NULL;
	set_variable_list(V_SMTP_SERVER, list, TRUE, Main);
	fs_give((void *)&list[0]);
	fs_give((void *)list);
    }

    return(1);
}
#endif


/* this is for initializing the fixed header elements in pine_send() */
/*
prompt::name::help::prlen::maxlen::realaddr::
builder::affected_entry::next_affected::selector::key_label::fileedit::
display_it::break_on_comma::is_attach::rich_header::only_file_chars::
single_space::sticky::dirty::start_here::blank::KS_ODATAVAR
*/
static struct headerentry he_template[]={
  {"From    : ",  "From",        h_composer_from,       10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL,
   0, 1, 0, 1, 0, 1, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"Reply-To: ",  "Reply To",    h_composer_reply_to,   10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL,
   0, 1, 0, 1, 0, 1, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"To      : ",  "To",          h_composer_to,         10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL,
   0, 1, 0, 0, 0, 1, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"Cc      : ",  "Cc",          h_composer_cc,         10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL,
   0, 1, 0, 0, 0, 1, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"Bcc     : ",  "Bcc",         h_composer_bcc,        10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL,
   0, 1, 0, 1, 0, 1, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"Newsgrps: ",  "Newsgroups",  h_composer_news,        10, 0, NULL,
   news_build,    NULL, NULL, news_group_selector,  "To NwsGrps", NULL,
   0, 1, 0, 1, 0, 1, 0, 0, 0, 0, KS_NONE},
  {"Fcc     : ",  "Fcc",         h_composer_fcc,        10, 0, NULL,
   NULL,          NULL, NULL, folders_for_fcc,      "To Fldrs", NULL,
   0, 0, 0, 1, 1, 1, 0, 0, 0, 0, KS_NONE},
  {"Lcc     : ",  "Lcc",         h_composer_lcc,        10, 0, NULL,
   build_addr_lcc, NULL, NULL, addr_book_compose_lcc,"To AddrBk", NULL,
   0, 1, 0, 1, 0, 1, 0, 0, 0, 0, KS_NONE},
  {"Attchmnt: ",  "Attchmnt",    h_composer_attachment, 10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 "To Files", NULL,
   0, 1, 1, 0, 0, 1, 0, 0, 0, 0, KS_NONE},
  {"Subject : ",  "Subject",     h_composer_subject,    10, 0, NULL,
   valid_subject, NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "References",  NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "Date",        NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "In-Reply-To", NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "Message-ID",  NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "To",          NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Post-Error",NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Reply-UID", NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Reply-Mbox", NO_HELP,              10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-SMTP-Server", NO_HELP,             10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Cursor-Pos", NO_HELP,              10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Our-ReplyTo", NO_HELP,             10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            OUR_HDRS_LIST, NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
  {"",            "Sender",      NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
#endif
};

static struct headerentry he_custom_addr_templ={
   NULL,          NULL,          h_composer_custom_addr,10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL,
   0, 1, 0, 1, 0, 1, 0, 0, 0, 0, KS_TOADDRBOOK};
static struct headerentry he_custom_free_templ={
   NULL,          NULL,          h_composer_custom_free,10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL,
   0, 0, 0, 1, 0, 0, 0, 0, 0, 0, KS_NONE};

/*
 * Note, these are in the same order in the he_template and pf_template arrays.
 */
#define N_FROM    0
#define N_REPLYTO 1
#define N_TO      2
#define N_CC      3
#define N_BCC     4
#define N_NEWS    5
#define N_FCC     6
#define N_LCC     7
#define N_ATTCH   8
#define N_SUBJ    9
#define N_REF     10
#define N_DATE    11
#define N_INREPLY 12
#define N_MSGID   13
#define N_NOBODY  14
#define	N_POSTERR 15
#define	N_RPLUID  16
#define	N_RPLMBOX 17
#define	N_SMTP    18
#define	N_NNTP    19
#define	N_CURPOS  20
#define	N_OURREPLYTO  21
#define	N_OURHDRS 22
#define N_SENDER  23

#define TONAME "To"
#define CCNAME "cc"
#define SUBJNAME "Subject"

/* this is used in pine_send and pine_simple_send */
/* name::type::canedit::writehdr::localcopy::rcptto */
static PINEFIELD pf_template[] = {
  {"From",        Address,	0, 1, 1, 0},
  {"Reply-To",    Address,	0, 1, 1, 0},
  {TONAME,        Address,	1, 1, 1, 1},
  {CCNAME,        Address,	1, 1, 1, 1},
  {"bcc",         Address,	1, 0, 1, 1},
  {"Newsgroups",  FreeText,	1, 1, 1, 0},
  {"Fcc",         Fcc,		1, 0, 0, 0},
  {"Lcc",         Address,	1, 0, 1, 1},
  {"Attchmnt",    Attachment,	1, 1, 1, 0},
  {SUBJNAME,      Subject,	1, 1, 1, 0},
  {"References",  FreeText,	0, 1, 1, 0},
  {"Date",        FreeText,	0, 1, 1, 0},
  {"In-Reply-To", FreeText,	0, 1, 1, 0},
  {"Message-ID",  FreeText,	0, 1, 1, 0},
  {"To",          Address,	0, 0, 0, 0},	/* N_NOBODY */
  {"X-Post-Error",FreeText,	0, 0, 0, 0},	/* N_POSTERR */
  {"X-Reply-UID", FreeText,	0, 0, 0, 0},	/* N_RPLUID */
  {"X-Reply-Mbox",FreeText,	0, 0, 0, 0},	/* N_RPLMBOX */
  {"X-SMTP-Server",FreeText,	0, 0, 0, 0},	/* N_SMTP */
  {"X-NNTP-Server",FreeText,	0, 0, 0, 0},	/* N_NNTP */
  {"X-Cursor-Pos",FreeText,	0, 0, 0, 0},	/* N_CURPOS */
  {"X-Our-ReplyTo",FreeText,	0, 0, 0, 0},	/* N_OURREPLYTO */
  {OUR_HDRS_LIST, FreeText,	0, 0, 0, 0},	/* N_OURHDRS */
#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
  {"X-X-Sender",    Address,	0, 1, 1, 0},
#endif
  {NULL,         FreeText}
};


/*----------------------------------------------------------------------
     Get addressee for message, then post message

  Args:  outgoing -- Partially formatted outgoing ENVELOPE
         body     -- Body of outgoing message
        prmpt_who -- Optional prompt for optionally_enter call
        prmpt_cnf -- Optional prompt for confirmation call
    used_tobufval -- The string that the to was eventually set equal to.
		      This gets passed back if non-NULL on entry.
        flagsarg  --     SS_PROMPTFORTO - Allow user to change recipient
                         SS_NULLRP - Use null return-path so we'll send an
			             SMTP MAIL FROM: <>

  Result: message "To: " field is provided and message is sent or cancelled.

  Fields:
     remail                -
     return_path           -
     date                 added here
     from                 added here
     sender                -
     reply_to              -
     subject              passed in, NOT edited but maybe canonized here
     to                   possibly passed in, edited and canonized here
     cc                    -
     bcc                   -
     in_reply_to           -
     message_id            -
  
Can only deal with message bodies that are either TYPETEXT or TYPEMULTIPART
with the first part TYPETEXT! All newlines in the text here also end with
CRLF.

Returns 0 on success, -1 on failure.
  ----*/
int
pine_simple_send(outgoing, body, role, prmpt_who, prmpt_cnf,
		 used_tobufval, flagsarg)
    ENVELOPE  *outgoing;  /* envelope for outgoing message */
    BODY     **body;   
    ACTION_S  *role;
    char      *prmpt_who, *prmpt_cnf;
    char     **used_tobufval;
    int        flagsarg;
{
    char     **tobufp, *p;
    void      *messagebuf;
    int        done = 0, retval = 0, x;
    int        cnt, i, resize_len, result, fcc_result;
    struct headerentry he_dummy;
    PINEFIELD *pfields, *pf, **sending_order;
    METAENV    header;
    HelpType   help;
    ESCKEY_S   ekey[2];
    BUILDER_ARG ba_fcc;

    dprint(1, (debugfile,"\n === simple send called === \n"));

    memset(&ba_fcc, 0, sizeof(BUILDER_ARG));

    /* how many fields are there? */
    cnt = (sizeof(pf_template)/sizeof(pf_template[0])) - 1;

    /* temporary PINEFIELD array */
    i = (cnt + 1) * sizeof(PINEFIELD);
    pfields = (PINEFIELD *)fs_get((size_t) i);
    memset(pfields, 0, (size_t) i);

    i             = (cnt + 1) * sizeof(PINEFIELD *);
    sending_order = (PINEFIELD **)fs_get((size_t) i);
    memset(sending_order, 0, (size_t) i);

    header.env           = outgoing;
    header.local         = pfields;
    header.custom        = NULL;
    header.sending_order = sending_order;

    /*----- Fill in a few general parts of the envelope ----*/
    if(!outgoing->date){
	if(F_ON(F_QUELL_TIMEZONE, ps_global))
	  mail_parameters(NULL, SET_DISABLE822TZTEXT, (void *) TRUE);

	rfc822_date(tmp_20k_buf);		/* format and copy new date */
	if(F_ON(F_QUELL_TIMEZONE, ps_global))
	  mail_parameters(NULL, SET_DISABLE822TZTEXT, (void *) FALSE);

	outgoing->date = (unsigned char *) cpystr(tmp_20k_buf);
    }

    if(!outgoing->from){
	if(role && role->from){
	    if(ps_global->never_allow_changing_from)
	      q_status_message(SM_ORDER, 3, 3, "Site policy doesn't allow changing From address so role's From has no effect");
	    else
	      outgoing->from = copyaddrlist(role->from);
	}
	else
	  outgoing->from = generate_from();
    }

    if(!(flagsarg & SS_NULLRP))
      outgoing->return_path = rfc822_cpy_adr(outgoing->from);

#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
#define NN 3
#else
#define NN 2
#endif
    /* initialize pfield */
    pf = pfields;
    for(i=0; i < cnt; i++, pf++){

        pf->name        = cpystr(pf_template[i].name);
	if(i == N_SENDER && F_ON(F_USE_SENDER_NOT_X, ps_global))
	  /* slide string over so it is Sender instead of X-X-Sender */
	  if(strlen(pf->name) > 6)
	    strncpy(pf->name, "Sender", 7);

        pf->type        = pf_template[i].type;
	pf->canedit     = pf_template[i].canedit;
	pf->rcptto      = pf_template[i].rcptto;
	pf->writehdr    = pf_template[i].writehdr;
	pf->localcopy   = pf_template[i].localcopy;
        pf->he          = NULL; /* unused */
        pf->next        = pf + 1;

        switch(pf->type){
          case FreeText:
            switch(i){
              case N_NEWS:
		pf->text		= &outgoing->newsgroups;
		sending_order[0]	= pf;
                break;

              case N_DATE:
		pf->text		= (char **) &outgoing->date;
		sending_order[1]	= pf;
                break;

              case N_INREPLY:
		pf->text		= &outgoing->in_reply_to;
		sending_order[NN+9]	= pf;
                break;

              case N_MSGID:
		pf->text		= &outgoing->message_id;
		sending_order[NN+10]	= pf;
                break;

              case N_REF:			/* won't be used here */
		sending_order[NN+11]	= pf;
                break;

              case N_POSTERR:			/* won't be used here */
		sending_order[NN+12]	= pf;
                break;

              case N_RPLUID:			/* won't be used here */
		sending_order[NN+13]	= pf;
                break;

              case N_RPLMBOX:			/* won't be used here */
		sending_order[NN+14]	= pf;
                break;

              case N_SMTP:			/* won't be used here */
		sending_order[NN+15]	= pf;
                break;

              case N_NNTP:			/* won't be used here */
		sending_order[NN+16]	= pf;
                break;

              case N_CURPOS:			/* won't be used here */
		sending_order[NN+17]	= pf;
                break;

              case N_OURREPLYTO:		/* won't be used here */
		sending_order[NN+18]	= pf;
                break;

              case N_OURHDRS:			/* won't be used here */
		sending_order[NN+19]	= pf;
                break;

              default:
                q_status_message1(SM_ORDER,3,3,
		    "Internal error: 1)FreeText header %d", (void *)i);
                break;
            }

            break;

          case Attachment:
            break;

          case Address:
            switch(i){
	      case N_FROM:
		sending_order[2]	= pf;
		pf->addr		= &outgoing->from;
		break;

	      case N_TO:
		sending_order[NN+2]	= pf;
		pf->addr		= &outgoing->to;
	        tobufp			= &pf->scratch;
		memset((void *) &he_dummy, 0, sizeof(he_dummy));
		pf->he			= &he_dummy;
		pf->he->dirty		= 1;	/* for strings2outgoing */
		break;

	      case N_CC:
		sending_order[NN+3]	= pf;
		pf->addr		= &outgoing->cc;
		break;

	      case N_BCC:
		sending_order[NN+4]	= pf;
		pf->addr		= &outgoing->bcc;
		break;

	      case N_REPLYTO:
		sending_order[NN+1]	= pf;
		pf->addr		= &outgoing->reply_to;
		break;

	      case N_LCC:		/* won't be used here */
		sending_order[NN+7]	= pf;
		break;

#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
              case N_SENDER:
		sending_order[3]	= pf;
		pf->addr		= &outgoing->sender;
                break;
#endif

              case N_NOBODY: /* won't be used here */
		sending_order[NN+5]	= pf;
                break;

              default:
                q_status_message1(SM_ORDER,3,3,
		    "Internal error: Address header %d", (void *) i);
                break;
            }
            break;

          case Fcc:
	    sending_order[NN+8] = pf;
	    pf->text		= &ba_fcc.tptr;
            break;

	  case Subject:
	    sending_order[NN+6]	= pf;
	    pf->text		= &outgoing->subject;
	    break;

          default:
            q_status_message1(SM_ORDER,3,3,
		"Unknown header type %d in pine_simple_send", (void *)pf->type);
            break;
        }
    }

    pf->next = NULL;

    ekey[0].ch    = ctrl('T');
    ekey[0].rval  = 2;
    ekey[0].name  = "^T";
    ekey[0].label = "To AddrBk";
    ekey[1].ch    = -1;

    /*----------------------------------------------------------------------
       Loop editing the "To: " field until everything goes well
     ----*/
    help = NO_HELP;

    while(!done){
	int flags;

	outgoing2strings(&header, *body, &messagebuf, NULL);

	resize_len = max(MAXPATH, strlen(*tobufp));
	fs_resize((void **)tobufp, resize_len+1);

	flags = OE_APPEND_CURRENT;
	if(flagsarg & SS_PROMPTFORTO)
	  i = optionally_enter(*tobufp, -FOOTER_ROWS(ps_global),
				0, resize_len,
				prmpt_who
				  ? prmpt_who
				  : outgoing->remail == NULL 
				    ? "FORWARD (as e-mail) to : "
				    : "BOUNCE (redirect) message to : ",
				ekey, help, &flags);
	else
	  i = 0;

	switch(i){
	  case -1:
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Internal problem encountered");
	    retval = -1;
	    done++;
	    break;

	  case 2: /* ^T */
	  case 0:
	   {void (*redraw) () = ps_global->redrawer;
	    char  *returned_addr = NULL;
	    int    len;

	    if(i == 2){
		int got_something = 0;
		TITLEBAR_STATE_S *tbstate = NULL;

		tbstate = save_titlebar_state();
		returned_addr = addr_book_bounce();

		/*
		 * Just make it look like user typed this list in.
		 */
		if(returned_addr){
		    got_something++;
		    if(resize_len < (len = strlen(returned_addr) + 1))
		      fs_resize((void **)tobufp, (size_t)len);

		    strcpy(*tobufp, returned_addr);
		    fs_give((void **)&returned_addr);
		}

		ClearScreen();
		restore_titlebar_state(tbstate);
		free_titlebar_state(&tbstate);
		redraw_titlebar();
		if(ps_global->redrawer = redraw) /* reset old value, and test */
		  (*ps_global->redrawer)();

		if(!got_something)
		  continue;
	    }

	    if(**tobufp != '\0'){
		char *errbuf, *addr;
		int   tolen;

		errbuf = NULL;

		/*
		 * If role has an fcc, use it instead of what build_address
		 * tells us.
		 */
		if(role && role->fcc){
		    if(ba_fcc.tptr)
		      fs_give((void **) &ba_fcc.tptr);
		    
		    ba_fcc.tptr = cpystr(role->fcc);
		}

		if(build_address(*tobufp, &addr, &errbuf,
		    (role && role->fcc) ? NULL : &ba_fcc, NULL) >= 0){
		    int sendit = 0;

		    if(errbuf)
		      fs_give((void **)&errbuf);

		    if(strlen(*tobufp) < (tolen = strlen(addr) + 1))
		      fs_resize((void **)tobufp, (size_t) tolen);

		    strcpy(*tobufp, addr);
		    if(used_tobufval)
		      *used_tobufval = cpystr(addr);

		    strings2outgoing(&header, body, NULL, NULL, 0);

		    if((flagsarg & SS_PROMPTFORTO) 
		       && ((x = check_addresses(&header)) == CA_BAD 
			   || (x == CA_EMPTY && F_OFF(F_FCC_ON_BOUNCE,
						      ps_global))))
		      /*--- Addresses didn't check out---*/
		      continue;
			
		    /* confirm address */
		    if(flagsarg & SS_PROMPTFORTO){
			char dsn_string[30];
			int  dsn_label = 0, dsn_show, i;
			int  verbose_label = 0;
			ESCKEY_S opts[13];

			i = 0;
			opts[i].ch      = 'y';
			opts[i].rval    = 'y';
			opts[i].name    = "Y";
			opts[i++].label = "Yes";

			opts[i].ch      = 'n';
			opts[i].rval    = 'n';
			opts[i].name    = "N";
			opts[i++].label = "No";

			verbose_requested = 0;
			if(F_ON(F_VERBOSE_POST, ps_global)){
			    /* setup keymenu slot to toggle verbose mode */
			    opts[i].ch    = ctrl('W');
			    opts[i].rval  = 12;
			    opts[i].name  = "^W";
			    verbose_label = i++;
			    if(F_ON(F_DSN, ps_global)){
				opts[i].ch      = 0;
				opts[i].rval    = 0;
				opts[i].name    = "";
				opts[i++].label = "";
			    }
			}

			dsn_requested = 0;
			if(F_ON(F_DSN, ps_global)){
			    /* setup keymenu slots to toggle dsn bits */
			    opts[i].ch      = 'd';
			    opts[i].rval    = 'd';
			    opts[i].name    = "D";
			    opts[i].label   = "DSNOpts";
			    dsn_label       = i++;
			    opts[i].ch      = -2;
			    opts[i].rval    = 's';
			    opts[i].name    = "S";
			    opts[i++].label = "";
			    opts[i].ch      = -2;
			    opts[i].rval    = 'x';
			    opts[i].name    = "X";
			    opts[i++].label = "";
			    opts[i].ch      = -2;
			    opts[i].rval    = 'h';
			    opts[i].name    = "H";
			    opts[i++].label = "";
			}

			opts[i].ch = -1;

			while(1){
			    int rv;

			    dsn_show = (dsn_requested & DSN_SHOW);
			    sprintf(tmp_20k_buf,
				    "%s%s%s%s%s%sto \"%s\" ? ",
				    prmpt_cnf ? prmpt_cnf : "Send message ",
				    (verbose_requested || dsn_show)
				      ? "(" : "",
				    (verbose_requested)
				      ? "in verbose mode" : "",
				    (dsn_show && verbose_requested)
				      ? ", " : "",
				    (dsn_show) ? dsn_string : "",
				    (verbose_requested || dsn_show)
				      ? ") " : "",
				    (addr && *addr)
					? addr
					: (F_ON(F_FCC_ON_BOUNCE, ps_global)
					   && ba_fcc.tptr && ba_fcc.tptr[0])
					    ? ba_fcc.tptr
					    : "");

			    if((strlen(tmp_20k_buf) >
					ps_global->ttyo->screen_cols - 2) &&
			       ps_global->ttyo->screen_cols >= 7)
			      strcpy(tmp_20k_buf+ps_global->ttyo->screen_cols-7,
				     "...? ");

			    if(verbose_label)
			      opts[verbose_label].label =
				      verbose_requested ? "Normal" : "Verbose";
			    
			    if(F_ON(F_DSN, ps_global)){
				if(dsn_requested & DSN_SHOW){
				    opts[dsn_label].label   =
						(dsn_requested & DSN_DELAY)
						   ? "NoDelay" : "Delay";
				    opts[dsn_label+1].ch    = 's';
				    opts[dsn_label+1].label =
						(dsn_requested & DSN_SUCCESS)
						   ? "NoSuccess" : "Success";
				    opts[dsn_label+2].ch    = 'x';
				    opts[dsn_label+2].label =
						(dsn_requested & DSN_NEVER)
						   ? "ErrRets" : "NoErrRets";
				    opts[dsn_label+3].ch    = 'h';
				    opts[dsn_label+3].label =
						(dsn_requested & DSN_FULL)
						   ? "RetHdrs" : "RetFull";
				}
			    }

			    rv = radio_buttons(tmp_20k_buf,
					       -FOOTER_ROWS(ps_global), opts,
					       'y', 'z', NO_HELP, RB_NORM,NULL);
			    if(rv == 'y'){		/* user ACCEPTS! */
				sendit = 1;
				break;
			    }
			    else if(rv == 'n'){		/* Declined! */
				break;
			    }
			    else if(rv == 'z'){		/* Cancelled! */
				break;
			    }
			    else if(rv == 12){		/* flip verbose bit */
				verbose_requested = !verbose_requested;
			    }
			    else if(dsn_requested & DSN_SHOW){
				if(rv == 's'){		/* flip success bit */
				    dsn_requested ^= DSN_SUCCESS;
				    /* turn off related bits */
				    if(dsn_requested & DSN_SUCCESS)
				      dsn_requested &= ~(DSN_NEVER);
				}
				else if(rv == 'd'){	/* flip delay bit */
				    dsn_requested ^= DSN_DELAY;
				    /* turn off related bits */
				    if(dsn_requested & DSN_DELAY)
				      dsn_requested &= ~(DSN_NEVER);
				}
				else if(rv == 'x'){	/* flip never bit */
				    dsn_requested ^= DSN_NEVER;
				    /* turn off related bits */
				    if(dsn_requested & DSN_NEVER)
				      dsn_requested &= ~(DSN_SUCCESS | DSN_DELAY);
				}
				else if(rv == 'h'){	/* flip full bit */
				    dsn_requested ^= DSN_FULL;
				}
			    }
			    else if(rv == 'd'){		/* show dsn options */
				dsn_requested = (DSN_SHOW | DSN_SUCCESS |
						 DSN_DELAY | DSN_FULL);
			    }

			    sprintf(dsn_string, "DSN requested[%s%s%s%s]",
				    (dsn_requested & DSN_NEVER)
				      ? "Never" : "F",
				    (dsn_requested & DSN_DELAY)
				      ? "D" : "",
				    (dsn_requested & DSN_SUCCESS)
				      ? "S" : "",
				    (dsn_requested & DSN_NEVER)
				      ? ""
				      : (dsn_requested & DSN_FULL) ? "-Full"
								   : "-Hdrs");
			}
		    }

		    if(addr)
		      fs_give((void **)&addr);

		    if(!(flagsarg & SS_PROMPTFORTO) || sendit){
			char *fcc = NULL;
			CONTEXT_S *fcc_cntxt = NULL;

			if(F_ON(F_FCC_ON_BOUNCE, ps_global)){
			    if(ba_fcc.tptr)
			      fcc = cpystr(ba_fcc.tptr);

			    set_last_fcc(fcc);

			    /*
			     * If special name "inbox" then replace it with the
			     * real inbox path.
			     */
			    if(ps_global->VAR_INBOX_PATH
			       && strucmp(fcc, ps_global->inbox_name) == 0){
				char *replace_fcc;

				replace_fcc = cpystr(ps_global->VAR_INBOX_PATH);
				fs_give((void **)&fcc);
				fcc = replace_fcc;
			    }
			}

			/*---- Check out fcc -----*/
			if(fcc && *fcc){
			    lmc.all_written = lmc.text_written = 0;
			    lmc.text_only = F_ON(F_NO_FCC_ATTACH, ps_global) != 0;
			    if(!(lmc.so = open_fcc(fcc, &fcc_cntxt,
						     0, NULL, NULL))){
				dprint(4,(debugfile,"can't open fcc, cont\n"));
				if(!(flagsarg & SS_PROMPTFORTO)){
				    retval = -1;
				    fs_give((void **)&fcc);
				    fcc = NULL;
				    goto finish;
				}
				else
				  continue;
			    }
			    else
			      so_truncate(lmc.so, fcc_size_guess(*body) + 2048);
			}
			else
			  lmc.so = NULL;

			if(!(outgoing->to || outgoing->cc || outgoing->bcc
			     || lmc.so)){
			    q_status_message(SM_ORDER, 3, 5,
				"No recipients specified!");
			    continue;
			}

			if(outgoing->to || outgoing->cc || outgoing->bcc){
			    char **alt_smtp = NULL;

			    if(role && role->smtp){
				if(ps_global->FIX_SMTP_SERVER
				   && ps_global->FIX_SMTP_SERVER[0])
				  q_status_message(SM_ORDER | SM_DING, 5, 5, "Use of a role-defined smtp-server is administratively prohibited");
				else
				  alt_smtp = role->smtp;
			    }

			    result = call_mailer(&header, *body, alt_smtp);
			}
			else
			  result = 0;

			if(result == 1 && !lmc.so)
			  q_status_message(SM_ORDER, 0, 3, "Message sent");

			/*----- Was there an fcc involved? -----*/
			if(lmc.so){
			    if(result == 1
			       || (result == 0
			   && pine_rfc822_output(&header, *body, NULL, NULL))){
				char label[50];

				strcpy(label, "Fcc");
				if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC))
				  sprintf(label + 3, " to %.40s", fcc);

				/* Now actually copy to fcc folder and close */
				fcc_result = 
				  write_fcc(fcc, fcc_cntxt, lmc.so, NULL,
					    label,
					    F_ON(F_MARK_FCC_SEEN, ps_global)
						? "\\SEEN" : NULL);
			    }
			    else if(result == 0){
				q_status_message(SM_ORDER,3,5,
				    "Fcc Failed!.  No message saved.");
				retval = -1;
				dprint(1,
				  (debugfile, "explicit fcc write failed!\n"));
			    }

			    so_give(&lmc.so);
			}

			if(result < 0){
			    dprint(1, (debugfile, "Bounce failed\n"));
			    if(!(flagsarg & SS_PROMPTFORTO))
			      retval = -1;
			    else
			      continue;
			}
			else if(result == 1){
			  if(!fcc)
			    q_status_message(SM_ORDER, 0, 3, 
					     "Message sent");
			  else{
			      int avail = ps_global->ttyo->screen_cols-2;
			      int need, fcclen;
			      char *part1 = "Message sent and ";
			      char *part2 = fcc_result ? "" : "NOT ";
			      char *part3 = "copied to ";
			      fcclen = strlen(fcc);

			      need = 2 + strlen(part1) + strlen(part2) +
				     strlen(part3) + fcclen;

			      if(need > avail && fcclen > 6)
				fcclen -= min(fcclen-6, need-avail);

			      q_status_message4(SM_ORDER, 0, 3,
						"%.200s%.200s%.200s\"%.200s\"",
						part1, part2, part3,
						short_str(fcc,
							  (char *)tmp_20k_buf,
							  fcclen, FrontDots));
			  }
			} 

			if(fcc)
			  fs_give((void **)&fcc);
		    }
		    else{
			q_status_message(SM_ORDER, 0, 3, "Send cancelled");
			retval = -1;
		    }
		}
		else{
		    q_status_message1(SM_ORDER | SM_DING, 3, 5,
				      "Error in address: %.200s", errbuf);
		    if(errbuf)
		      fs_give((void **)&errbuf);

		    if(!(flagsarg & SS_PROMPTFORTO))
		      retval = -1;
		    else
		      continue;
		}

	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 5,
				 "No addressee!  No e-mail sent.");
		retval = -1;
	    }
	   }

	    done++;
	    break;

	  case 1:
	    q_status_message(SM_ORDER, 0, 3, "Send cancelled");
	    done++;
	    retval = -1;
	    break;

	  case 3:
	    help = (help == NO_HELP)
			    ? (outgoing->remail == NULL
				? h_anon_forward
				: h_bounce)
			    : NO_HELP;
	    break;

	  case 4:				/* can't suspend */
	  default:
	    break;
	}
    }

finish:
    if(ba_fcc.tptr)
      fs_give((void **)&ba_fcc.tptr);

    for(i=0; i < cnt; i++)
      fs_give((void **)&pfields[i].name);

    fs_give((void **)&pfields);
    fs_give((void **)&sending_order);

    return(retval);
}


/*----------------------------------------------------------------------
     Prepare data structures for pico, call pico, then post message

  Args:  outgoing     -- Partially formatted outgoing ENVELOPE
         body         -- Body of outgoing message
         editor_title -- Title for anchor line in composer
         fcc_arg      -- The file carbon copy field
	 reply        -- Struct describing set of msgs being replied-to
	 lcc_arg      --
	 custom       -- custom header list.
	 sticky_fcc   --

  Result: message is edited, then postponed, cancelled or sent.

  Fields:
     remail                -
     return_path           -
     date                 added here
     from                 added here
     sender                -
     reply_to              -
     subject              passed in, edited and cannonized here
     to                   possibly passed in, edited and cannonized here
     cc                   possibly passed in, edited and cannonized here
     bcc                  edited and cannonized here
     in_reply_to          generated in reply() and passed in
     message_id            -
  
 Storage for these fields comes from anywhere outside. It is remalloced
 here so the composer can realloc them if needed. The copies here are also 
 freed here.

Can only deal with message bodies that are either TYPETEXT or TYPEMULTIPART
with the first part TYPETEXT! All newlines in the text here also end with
CRLF.

There's a further assumption that the text in the TYPETEXT part is 
stored in a storage object (see filter.c).
  ----*/
void
pine_send(outgoing, body, editor_title, role, fcc_arg, reply, redraft_pos,
	  lcc_arg, custom, sticky_fcc)
    ENVELOPE  *outgoing;  /* c-client envelope for outgoing message */
    BODY     **body;   
    char      *editor_title;
    ACTION_S  *role;
    char      *fcc_arg;
    REPLY_S   *reply;
    REDRAFT_POS_S *redraft_pos;
    char      *lcc_arg;
    PINEFIELD *custom;
    int        sticky_fcc;
{
    int			i, fixed_cnt, total_cnt, index, hibit_entered,
			editor_result = 0, body_start = 0, use_news_order = 0;
    char	       *p, *addr, *fcc, *fcc_to_free = NULL;
    char	       *start_here_name = NULL;
    char               *suggested_nntp_server = NULL;
    char	       *title = NULL;
    struct headerentry *he, *headents, *he_to, *he_fcc, *he_news, *he_lcc,
		       *he_from = NULL;
    PINEFIELD          *pfields, *pf, *pf_nobody = NULL,
                       *pf_smtp_server, *pf_nntp_server,
		       *pf_fcc = NULL, *pf_err, *pf_uid, *pf_mbox, *pf_curpos,
		       *pf_ourrep, *pf_ourhdrs, **sending_order;
    METAENV             header;
    ADDRESS            *lcc_addr = NULL;
    ADDRESS            *nobody_addr = NULL;
    BODY_PARTICULARS_S *bp;
    STORE_S	       *orig_so = NULL;
    PICO	        pbuf1, *save_previous_pbuf;
    REDRAFT_POS_S      *local_redraft_pos = NULL;
#ifdef	DOS
    char               *reserve;
#endif

    dprint(1, (debugfile,"\n=== send called ===\n"));

    save_previous_pbuf = pbf;
    pbf = &pbuf1;
    standard_picobuf_setup(pbf);

    /*
     * Cancel any pending initial commands since pico uses a different
     * input routine.  If we didn't cancel them, they would happen after
     * we returned from the editor, which would be confusing.
     */
    if(ps_global->in_init_seq){
	ps_global->in_init_seq = 0;
	ps_global->save_in_init_seq = 0;
	clear_cursor_pos();
	if(ps_global->initial_cmds){
	    if(ps_global->free_initial_cmds)
	      fs_give((void **)&(ps_global->free_initial_cmds));

	    ps_global->initial_cmds = 0;
	}

	F_SET(F_USE_FK,ps_global,ps_global->orig_use_fkeys);
    }

#if defined(DOS) || defined(OS2)
    if(!dos_valid_from()){
	pbf = save_previous_pbuf;
	return;
    }

    pbf->upload        = NULL;
#else
    pbf->upload        = (ps_global->VAR_UPLOAD_CMD
			  && ps_global->VAR_UPLOAD_CMD[0])
			  ? upload_msg_to_pico : NULL;
#endif

    pbf->msgntext      = message_format_for_pico;
    pbf->mimetype      = mime_type_for_pico;
    pbf->exittest      = send_exit_for_pico;
    if(F_OFF(F_CANCEL_CONFIRM, ps_global))
      pbf->canceltest    = cancel_for_pico;

    pbf->alt_ed        = (ps_global->VAR_EDITOR && ps_global->VAR_EDITOR[0] &&
			    ps_global->VAR_EDITOR[0][0])
				? ps_global->VAR_EDITOR : NULL;
    pbf->alt_spell     = (ps_global->VAR_SPELLER && ps_global->VAR_SPELLER[0])
				? ps_global->VAR_SPELLER : NULL;
    pbf->always_spell_check = F_ON(F_ALWAYS_SPELL_CHECK, ps_global);
    pbf->quote_str     = reply && reply->prefix ? reply->prefix : "> ";
    /* We actually want to set this only if message we're sending is flowed */
    pbf->strip_ws_before_send = F_ON(F_STRIP_WS_BEFORE_SEND, ps_global);
    pbf->allow_flowed_text = (F_OFF(F_QUELL_FLOWED_TEXT, ps_global)
			      && F_OFF(F_STRIP_WS_BEFORE_SEND, ps_global)
			      && (strcmp(pbf->quote_str, "> ") == 0
				  || strcmp(pbf->quote_str, ">") == 0));
    pbf->edit_offset   = 0;
    title               = cpystr(set_titlebar(editor_title,
				    ps_global->mail_stream,
				    ps_global->context_current,
				    ps_global->cur_folder,ps_global->msgmap, 
				    0, FolderName, 0, 0, NULL));
    pbf->pine_anchor   = title;

#if	defined(DOS) || defined(OS2)
    if(!pbf->oper_dir && ps_global->VAR_FILE_DIR){
	pbf->oper_dir    = ps_global->VAR_FILE_DIR;
    }
#endif

    if(redraft_pos && editor_title && !strcmp(editor_title, COMPOSE_MAIL_TITLE))
      pbf->pine_flags |= P_CHKPTNOW;

    /* NOTE: initial cursor position set below */

    dprint(9, (debugfile, "flags: %x\n", pbf->pine_flags));

    /*
     * When user runs compose and the current folder is a newsgroup,
     * offer to post to the current newsgroup.
     */
    if(!(outgoing->to || (outgoing->newsgroups && *outgoing->newsgroups))
       && IS_NEWS(ps_global->mail_stream)){
	char prompt[200], news_group[MAILTMPLEN];

	pine_send_newsgroup_name(ps_global->mail_stream->mailbox, news_group,
				 sizeof(news_group));

	/*
	 * Replies don't get this far because To or Newsgroups will already
	 * be filled in.  So must be either ordinary compose or forward.
	 * Forward sets subject, so use that to tell the difference.
	 */
	if(news_group[0] && !outgoing->subject){
	    int ch = 'y';
	    int ret_val;
	    char *errmsg = NULL;
	    BUILDER_ARG	 *fcc_build = NULL;

	    if(F_OFF(F_COMPOSE_TO_NEWSGRP,ps_global)){
		sprintf(prompt,
		    "Post to current newsgroup (%.100s)", news_group);
		ch = want_to(prompt, 'y', 'x', NO_HELP, WT_NORM);
	    }

	    switch(ch){
	      case 'y':
		if(outgoing->newsgroups)
		  fs_give((void **)&outgoing->newsgroups); 

		if(!fcc_arg && !(role && role->fcc)){
		    fcc_build = (BUILDER_ARG *)fs_get(sizeof(BUILDER_ARG));
		    memset((void *)fcc_build, 0, sizeof(BUILDER_ARG));
		    fcc_build->tptr = fcc_to_free;
		}

		ret_val = news_build(news_group, &outgoing->newsgroups,
				     &errmsg, fcc_build, NULL);

		if(ret_val == -1){
		    if(outgoing->newsgroups)
		      fs_give((void **)&outgoing->newsgroups); 

		    outgoing->newsgroups = cpystr(news_group);
		}

		if(!fcc_arg && !(role && role->fcc)){
		    fcc_arg = fcc_to_free = fcc_build->tptr;
		    fs_give((void **)&fcc_build);
		}

		if(errmsg){
		    if(*errmsg){
			q_status_message(SM_ORDER, 3, 3, errmsg);
			display_message(NO_OP_COMMAND);
		    }

		    fs_give((void **)&errmsg);
		}

		break;

	      case 'x': /* ^C */
		q_status_message(SM_ORDER, 0, 3, "Message cancelled");
		dprint(4, (debugfile, "=== send: cancelled\n"));
		pbf = save_previous_pbuf;
		return;

	      case 'n':
		break;

	      default:
		break;
	    }
	}
    }
    if(F_ON(F_PREDICT_NNTP_SERVER, ps_global)
       && outgoing->newsgroups && *outgoing->newsgroups
       && IS_NEWS(ps_global->mail_stream)){
	NETMBX news_mb;

	if(mail_valid_net_parse(ps_global->mail_stream->original_mailbox,
				&news_mb))
	  if(!strucmp(news_mb.service, "nntp")){
	      if(*ps_global->mail_stream->original_mailbox == '{'){
		  char *svcp = NULL, *psvcp;

		  suggested_nntp_server =
		    cpystr(ps_global->mail_stream->original_mailbox + 1);
		  if(p = strindex(suggested_nntp_server, '}'))
		    *p = '\0';
		  for(p = strindex(suggested_nntp_server, '/'); p && *p;
		      p = strindex(p, '/')){
		      /* take out /nntp, which gets added in nntp_open */
		      if(!struncmp(p, "/nntp", 5))
			svcp = p + 5;
		      else if(!struncmp(p, "/service=nntp", 13))
			svcp = p + 13;
		      else if(!struncmp(p, "/service=\"nntp\"", 15))
			svcp = p + 15;
		      else
			p++;
		      if(svcp){
			  if(*svcp == '\0')
			    *p = '\0';
			  else if(*svcp == '/' || *svcp == ':'){
			      for(psvcp = p; *svcp; svcp++, psvcp++)
				*psvcp = *svcp;
			      *psvcp = '\0';
			  }
			  svcp = NULL;
		      }
		  }
	      }
	      else
		suggested_nntp_server = cpystr(news_mb.orighost);
	  }
    }

    /*
     * If we don't already have custom headers set and the role has custom
     * headers, then incorporate those custom headers into "custom".
     */
    if(!custom){
	PINEFIELD *dflthdrs = NULL, *rolehdrs = NULL;

	dflthdrs = parse_custom_hdrs(ps_global->VAR_CUSTOM_HDRS, UseAsDef);
/*
 * If we allow the Combine argument here, we're saying that we want to
 * combine the values from the envelope and the role for the fields To,
 * Cc, Bcc, and Newsgroups. For example, if we are replying to a message
 * we'll have a To in the envelope because we're replying. If our role also
 * has a To action, then Combine would combine those two and offer both
 * to the user. We've decided against doing this. Instead, we always use
 * Replace, and the role's header value replaces the value from the
 * envelope. It might also make sense in some cases to do the opposite,
 * which would be treating the role headers as defaults, just like
 * customized-hdrs.
 */
#ifdef WANT_TO_COMBINE_ADDRESSES
	if(role && role->cstm)
	  rolehdrs = parse_custom_hdrs(role->cstm, Combine);
#else
	if(role && role->cstm)
	  rolehdrs = parse_custom_hdrs(role->cstm, Replace);
#endif

	if(rolehdrs){
	    custom = combine_custom_headers(dflthdrs, rolehdrs);
	    if(dflthdrs)
	      free_customs(dflthdrs);
	    if(rolehdrs)
	      free_customs(rolehdrs);
	}
        else
	  custom = dflthdrs;
    }

    g_rolenick = role ? role->nick : NULL;

    /* how many fixed fields are there? */
    fixed_cnt = (sizeof(pf_template)/sizeof(pf_template[0])) - 1;

    total_cnt = fixed_cnt + count_custom_hdrs_pf(custom,1);

    /* the fixed part of the PINEFIELDs */
    i       = fixed_cnt * sizeof(PINEFIELD);
    pfields = (PINEFIELD *)fs_get((size_t) i);
    memset(pfields, 0, (size_t) i);

    /* temporary headerentry array for pico */
    i        = (total_cnt + 1) * sizeof(struct headerentry);
    headents = (struct headerentry *)fs_get((size_t) i);
    memset(headents, 0, (size_t) i);

    i             = total_cnt * sizeof(PINEFIELD *);
    sending_order = (PINEFIELD **)fs_get((size_t) i);
    memset(sending_order, 0, (size_t) i);

    pbf->headents        = headents;
    header.env           = outgoing;
    header.local         = pfields;
    header.sending_order = sending_order;

    /* custom part of PINEFIELDs */
    header.custom = custom;

    he = headents;
    pf = pfields;

    /*
     * For Address types, pf->addr points to an ADDRESS *.
     * If that address is in the "outgoing" envelope, it will
     * be freed by the caller, otherwise, it should be freed here.
     * Pf->textbuf for an Address is used a little to set up a default,
     * but then is freed right away below.  Pf->scratch is used for a
     * pointer to some alloced space for pico to edit in.  Addresses in
     * the custom area are freed by free_customs().
     *
     * For FreeText types, pf->addr is not used.  Pf->text points to a
     * pointer that points to the text.  Pf->textbuf points to a copy of
     * the text that must be freed before we leave, otherwise, it is
     * probably a pointer into the envelope and that gets freed by the
     * caller.
     *
     * He->realaddr is the pointer to the text that pico actually edits.
     */

#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
#define NN 3
#else
#define NN 2
#endif

    if(outgoing->newsgroups && *outgoing->newsgroups)
      use_news_order++;

    /* initialize the fixed header elements of the two temp arrays */
    for(i=0; i < fixed_cnt; i++, pf++){
	static int news_order[] = {
	    N_FROM, N_REPLYTO, N_NEWS, N_TO, N_CC, N_BCC, N_FCC,
	    N_LCC, N_ATTCH, N_SUBJ, N_REF, N_DATE, N_INREPLY,
	    N_MSGID, N_NOBODY, N_POSTERR, N_RPLUID, N_RPLMBOX,
	    N_SMTP, N_NNTP, N_CURPOS, N_OURREPLYTO, N_OURHDRS
#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
	    , N_SENDER
#endif
	};

	index = i;
	/* slightly different editing order if sending to news */
	if(use_news_order &&
	   index >= 0 && index < sizeof(news_order)/sizeof(news_order[0]))
	  index = news_order[i];

	/* copy the templates */
	*he             = he_template[index];

	pf->name        = cpystr(pf_template[index].name);
	if(index == N_SENDER && F_ON(F_USE_SENDER_NOT_X, ps_global))
	  /* slide string over so it is Sender instead of X-X-Sender */
	  for(p=pf->name; *(p+1); p++)
	    *p = *(p+4);

	pf->type        = pf_template[index].type;
	pf->canedit     = pf_template[index].canedit;
	pf->rcptto      = pf_template[index].rcptto;
	pf->writehdr    = pf_template[index].writehdr;
	pf->localcopy   = pf_template[index].localcopy;
	pf->he          = he;
	pf->next        = pf + 1;

	he->rich_header = view_as_rich(pf->name, he->rich_header);

	switch(pf->type){
	  case FreeText:   		/* realaddr points to c-client env */
	    if(index == N_NEWS){
		sending_order[0]	= pf;
		he->realaddr		= &outgoing->newsgroups;
	        he_news			= he;

		switch(set_default_hdrval(pf, custom)){
		  case Replace:
		    if(*he->realaddr)
		      fs_give((void **)he->realaddr);

		    *he->realaddr = pf->textbuf;
		    pf->textbuf = NULL;
		    break;

		  case Combine:
		    if(*he->realaddr){		/* combine values */
			if(pf->textbuf && *pf->textbuf){
			    char *combined_hdr;

			    combined_hdr = (char *)fs_get((strlen(*he->realaddr)
						    + strlen(pf->textbuf) + 2)
						      * sizeof(char));
			    strcat(strcat(strcpy(combined_hdr,
					     *he->realaddr), ","), pf->textbuf);
			    fs_give((void **)he->realaddr);
			    *he->realaddr = combined_hdr;
			    q_status_message(SM_ORDER, 3, 3,
					     "Adding newsgroup from role");
			}
		    }
		    else{
			*he->realaddr = pf->textbuf;
			pf->textbuf   = NULL;
		    }

		    break;

		  case UseAsDef:
		    /* if no value, use default */
		    if(!*he->realaddr){
			*he->realaddr = pf->textbuf;
			pf->textbuf   = NULL;
		    }

		    break;

		  case NoMatch:
		    break;
		}

		/* If there is a newsgroup, we'd better show it */
		if(outgoing->newsgroups && *outgoing->newsgroups)
		  he->rich_header = 0; /* force on by default */

		if(pf->textbuf)
		  fs_give((void **)&pf->textbuf);

		pf->text = he->realaddr;
	    }
	    else if(index == N_DATE){
		sending_order[1]	= pf;
		pf->text		= (char **) &outgoing->date;
		pf->he			= NULL;
	    }
	    else if(index == N_INREPLY){
		sending_order[NN+9]	= pf;
		pf->text		= &outgoing->in_reply_to;
		pf->he			= NULL;
	    }
	    else if(index == N_MSGID){
		sending_order[NN+10]	= pf;
		pf->text		= &outgoing->message_id;
		pf->he			= NULL;
	    }
	    else if(index == N_REF){
		sending_order[NN+11]	= pf;
		pf->text		= &outgoing->references;
		pf->he			= NULL;
	    }
	    else if(index == N_POSTERR){
		sending_order[NN+12]	= pf;
		pf_err			= pf;
		pf->text		= &pf->textbuf;
		pf->he			= NULL;
	    }
	    else if(index == N_RPLUID){
		sending_order[NN+13]	= pf;
		pf_uid			= pf;
		pf->text		= &pf->textbuf;
		pf->he			= NULL;
	    }
	    else if(index == N_RPLMBOX){
		sending_order[NN+14]	= pf;
		pf_mbox			= pf;
		pf->text		= &pf->textbuf;
		pf->he			= NULL;
	    }
	    else if(index == N_SMTP){
		sending_order[NN+15]	= pf;
		pf_smtp_server		= pf;
		pf->text		= &pf->textbuf;
		pf->he			= NULL;
	    }
	    else if(index == N_NNTP){
		sending_order[NN+16]	= pf;
		pf_nntp_server		= pf;
		pf->text		= &pf->textbuf;
		pf->he			= NULL;
	    }
	    else if(index == N_CURPOS){
		sending_order[NN+17]	= pf;
		pf_curpos		= pf;
		pf->text		= &pf->textbuf;
		pf->he			= NULL;
	    }
	    else if(index == N_OURREPLYTO){
		sending_order[NN+18]	= pf;
		pf_ourrep		= pf;
		pf->text		= &pf->textbuf;
		pf->he			= NULL;
	    }
	    else if(index == N_OURHDRS){
		sending_order[NN+19]	= pf;
		pf_ourhdrs		= pf;
		pf->text		= &pf->textbuf;
		pf->he			= NULL;
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 7,
			    "Botched: Unmatched FreeText header in pine_send");
	    }

	    break;

	  /* can't do a default for this one */
	  case Attachment:
	    /* If there is an attachment already, we'd better show them */
            if(body && *body && (*body)->type != TYPETEXT)
	      he->rich_header = 0; /* force on by default */

	    break;

	  case Address:
	    switch(index){
	      case N_FROM:
		sending_order[2]	= pf;
		pf->addr		= &outgoing->from;
		if(role && role->from){
		    if(ps_global->never_allow_changing_from)
		      q_status_message(SM_ORDER, 3, 3, "Site policy doesn't allow changing From address so role's From has no effect");
		    else{
			outgoing->from = copyaddrlist(role->from);
			he->display_it  = 1;  /* show it */
			he->rich_header = 0;
		    }
		}

		he_from			= he;
		break;

	      case N_TO:
		sending_order[NN+2]	= pf;
		pf->addr		= &outgoing->to;
		/* If already set, make it act like we typed it in */
		if(outgoing->to
		   && outgoing->to->mailbox
		   && outgoing->to->mailbox[0])
		  he->sticky = 1;

		he_to			= he;
		break;

	      case N_NOBODY:
		sending_order[NN+5]	= pf;
		pf_nobody		= pf;
		if(ps_global->VAR_EMPTY_HDR_MSG
		   && !ps_global->VAR_EMPTY_HDR_MSG[0]){
		    pf->addr		= NULL;
		}
		else{
		    nobody_addr          = mail_newaddr();
		    nobody_addr->next    = mail_newaddr();
		    nobody_addr->mailbox = cpystr(rfc1522_encode(tmp_20k_buf,
			    SIZEOF_20KBUF,
			    (unsigned char *)(ps_global->VAR_EMPTY_HDR_MSG
						? ps_global->VAR_EMPTY_HDR_MSG
						: "undisclosed-recipients"),
			    ps_global->VAR_CHAR_SET));
		    pf->addr		= &nobody_addr;
		}

		break;

	      case N_CC:
		sending_order[NN+3]	= pf;
		pf->addr		= &outgoing->cc;
		break;

	      case N_BCC:
		sending_order[NN+4]	= pf;
		pf->addr		= &outgoing->bcc;
		/* if bcc exists, make sure it's exposed so nothing's
		 * sent by mistake...
		 */
		if(outgoing->bcc)
		  he->display_it = 1;

		break;

	      case N_REPLYTO:
		sending_order[NN+1]	= pf;
		pf->addr		= &outgoing->reply_to;
		if(role && role->replyto){
		    if(outgoing->reply_to)
		      mail_free_address(&outgoing->reply_to);

		    outgoing->reply_to = copyaddrlist(role->replyto);
		    he->display_it  = 1;  /* show it */
		    he->rich_header = 0;
		}

		break;

	      case N_LCC:
		sending_order[NN+7]	= pf;
		pf->addr		= &lcc_addr;
		he_lcc			= he;
		if(lcc_arg){
		    build_address(lcc_arg, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(&lcc_addr, addr,
					 ps_global->maildomain);
		    fs_give((void **)&addr);
		    he->display_it = 1;
		}

		break;

#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
              case N_SENDER:
		sending_order[3]	= pf;
		pf->addr		= &outgoing->sender;
                break;
#endif

	      default:
		q_status_message1(SM_ORDER,3,7,
		    "Internal error: Address header %d", (void *)index);
		break;
	    }

	    /*
	     * If this is a reply to news, don't show the regular email
	     * recipient headers (unless they are non-empty).
	     */
	    if((outgoing->newsgroups && *outgoing->newsgroups)
	       && (index == N_TO || index == N_CC
		   || index == N_BCC || index == N_LCC)
	       && (pf->addr && !*pf->addr)){
		if(set_default_hdrval(pf, custom) >= UseAsDef &&
		   pf->textbuf && *pf->textbuf){
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(pf->addr, addr,
					 ps_global->maildomain);
		    fs_give((void **)&addr);
		}
		else
		  he->rich_header = 1; /* hide */
	    }

	    /*
	     * If this address doesn't already have a value, then we check
	     * for a default value assigned by the user.
	     */
	    else if(pf->addr && !*pf->addr){
		if(set_default_hdrval(pf, custom) >= UseAsDef &&
		   (index != N_FROM ||
		    (!ps_global->never_allow_changing_from &&
		     F_ON(F_ALLOW_CHANGING_FROM, ps_global))) &&
		   pf->textbuf && *pf->textbuf){
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(pf->addr, addr,
					 ps_global->maildomain);
		    fs_give((void **)&addr);
		}

		/* if we still don't have a from */
		if(index == N_FROM && !*pf->addr)
		  *pf->addr = generate_from();
	    }

	    /*
	     * Addr is already set in the rest of the cases.
	     */
	    else if((index == N_FROM || index == N_REPLYTO) && pf->addr){
		ADDRESS *adr = NULL;

		/*
		 * We get to this case of the ifelse if the from or reply-to
		 * addr was set by a role above.
		 */

		/* figure out the default value */
		(void)set_default_hdrval(pf, custom);
		if(pf->textbuf && *pf->textbuf){
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(&adr, addr,
					 ps_global->maildomain);
		    fs_give((void **)&addr);
		}

		/* if value set by role is different from default, show it */
		if(adr && !address_is_same(*pf->addr, adr))
		  he->display_it = 1;  /* start this off showing */

		/* malformed */
		if(!(*pf->addr)->mailbox){
		    fs_give((void **)pf->addr);
		    he->display_it = 1;
		}

		if(adr)
		  mail_free_address(&adr);
	    }
	    else if((index == N_TO || index == N_CC || index == N_BCC)
		    && pf->addr){
		ADDRESS *a = NULL, **tail;

		/*
		 * These three are different from the others because we
		 * might add the addresses to what is already there instead
		 * of replacing.
		 */

		switch(set_default_hdrval(pf, custom)){
		  case Replace:
		    if(*pf->addr)
		      mail_free_address(pf->addr);
		    
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(pf->addr, addr, ps_global->maildomain);
		    fs_give((void **)&addr);
		    break;

		  case Combine:
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(&a, addr, ps_global->maildomain);
		    fs_give((void **)&addr);
		    if(a){
			for(tail = pf->addr; *tail; tail = &(*tail)->next)
			  ;
			*tail = reply_cp_addr(ps_global, 0, NULL, NULL,
					      *pf->addr, NULL, a, 1);
			q_status_message(SM_ORDER, 3, 3,
					 "Adding addresses from role");
			mail_free_address(&a);
		    }

		    break;

		  case UseAsDef:
		  case NoMatch:
		    break;
		}

		he->display_it = 1;  /* start this off showing */
	    }
	    else if(pf->addr){
		switch(set_default_hdrval(pf, custom)){
		  case Replace:
		  case Combine:
		    if(*pf->addr)
		      mail_free_address(pf->addr);
		    
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(pf->addr, addr, ps_global->maildomain);
		    fs_give((void **)&addr);
		    break;

		  case UseAsDef:
		  case NoMatch:
		    break;
		}

		he->display_it = 1;
	    }

	    if(pf->addr && *pf->addr && !(*pf->addr)->mailbox){
		mail_free_address(pf->addr);
		he->display_it = 1;  /* start this off showing */
	    }

	    if(pf->textbuf)		/* free default value in any case */
	      fs_give((void **)&pf->textbuf);

	    /* outgoing2strings will alloc the string pf->scratch below */
	    he->realaddr = &pf->scratch;
	    break;

	  case Fcc:
	    sending_order[NN+8] = pf;
	    pf_fcc              = pf;
	    if(role && role->fcc)
	      fcc = role->fcc;
	    else
	      fcc = get_fcc(fcc_arg);

	    if(fcc_to_free){
		fs_give((void **)&fcc_to_free);
		fcc_arg = NULL;
	    }

	    if((sticky_fcc && fcc[0]) || (role && role->fcc))
	      he->sticky = 1;

	    if(role)
	      role->fcc = NULL;

	    if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC) != 0)
	      he->display_it = 1;  /* start this off showing */

	    he->realaddr  = &fcc;
	    pf->text      = &fcc;
	    he_fcc        = he;
	    break;

	  case Subject :
	    sending_order[NN+6]	= pf;
	    
	    switch(set_default_hdrval(pf, custom)){
	      case Replace:
	      case Combine:
		pf->scratch = pf->textbuf;
		pf->textbuf = NULL;
		if(outgoing->subject)
		  fs_give((void **)&outgoing->subject);

		break;

	      case UseAsDef:
	      case NoMatch:
		/* if no value, use default */
		if(outgoing->subject){
		    pf->scratch = cpystr(outgoing->subject);
		}
		else{
		    pf->scratch = pf->textbuf;
		    pf->textbuf   = NULL;
		}

		break;
	    }

	    he->realaddr = &pf->scratch;
	    pf->text	 = &outgoing->subject;
	    break;

	  default:
	    q_status_message1(SM_ORDER,3,7,
			      "Unknown header type %d in pine_send",
			      (void *)pf->type);
	    break;
	}

	/*
	 * We may or may not want to give the user the chance to edit
	 * the From and Reply-To lines.  If they are listed in either
	 * Default-composer-hdrs or Customized-hdrs, then they can edit
	 * them, else no.
	 * If canedit is not set, that means that this header is not in
	 * the user's customized-hdrs.  If rich_header is set, that
	 * means that this header is not in the user's
	 * default-composer-hdrs (since From and Reply-To are rich
	 * by default).  So, don't give it an he to edit with in that case.
	 *
	 * For other types, just not setting canedit will cause it to be
	 * uneditable, regardless of what the user does.
	 */
	switch(index){
	  case N_FROM:
    /* to allow it, we let this fall through to the reply-to case below */
	    if(ps_global->never_allow_changing_from ||
	       (F_OFF(F_ALLOW_CHANGING_FROM, ps_global) &&
	        !(role && role->from))){
		if(pf->canedit || !he->rich_header)
		  q_status_message(SM_ORDER, 3, 3,
			"Not allowed to change header \"From\"");

		memset(he, 0, (size_t)sizeof(*he));
		pf->he = NULL;
		break;
	    }

	  case N_REPLYTO:
	    if(!pf->canedit && he->rich_header){
	        memset(he, 0, (size_t)sizeof(*he));
		pf->he = NULL;
	    }
	    else{
		pf->canedit = 1;
		he++;
	    }

	    break;

	  default:
	    if(!pf->canedit){
	        memset(he, 0, (size_t)sizeof(*he));
		pf->he = NULL;
	    }
	    else
	      he++;

	    break;
	}
    }

    /*
     * This is so the builder can tell the composer to fill the affected
     * field based on the value in the field on the left.
     *
     * Note that this mechanism isn't completely general.  Each entry has
     * only a single next_affected, so if some other entry points an
     * affected entry at an entry with a next_affected, they all inherit
     * that next_affected.  Since this isn't used much a careful ordering
     * of the affected fields should make it a sufficient mechanism.
     */
    he_to->affected_entry   = he_fcc;
    he_news->affected_entry = he_fcc;
    he_lcc->affected_entry  = he_to;
    he_to->next_affected    = he_fcc;

    (--pf)->next = (total_cnt != fixed_cnt) ? header.custom : NULL;

    i--;  /* subtract one because N_ATTCH doesn't get a sending_order slot */
    /*
     * Set up headerentries for custom fields.
     * NOTE: "i" is assumed to now index first custom field in sending
     *       order.
     */
    for(pf = pf->next; pf && pf->name; pf = pf->next){
	char *addr;

	if(pf->standard)
	  continue;

	pf->he          = he;
	pf->canedit     = 1;
	pf->rcptto      = 0;
	pf->writehdr    = 1;
	pf->localcopy   = 1;
	
	switch(pf->type){
	  case Address:
	    if(pf->addr){				/* better be set */
		sending_order[i++] = pf;
		*he = he_custom_addr_templ;
		/* change default text into an ADDRESS */
		/* strip quotes around whole default */
		removing_trailing_white_space(pf->textbuf);
		(void)removing_double_quotes(pf->textbuf);
		build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		rfc822_parse_adrlist(pf->addr, addr, ps_global->maildomain);
		fs_give((void **)&addr);
		if(pf->textbuf)
		  fs_give((void **)&pf->textbuf);

		he->realaddr = &pf->scratch;
	    }

	    break;

	  case FreeText:
	    sending_order[i++] = pf;
	    *he                = he_custom_free_templ;
	    he->realaddr       = &pf->textbuf;
	    pf->text           = &pf->textbuf;
	    if(((!pf->val || !pf->val[0]) && pf->textbuf && pf->textbuf[0]) ||
	       (pf->val && (!pf->textbuf || strcmp(pf->textbuf, pf->val))))
	      he->display_it  = 1;  /* show it */

	    break;

	  default:
	    q_status_message1(SM_ORDER,0,7,"Unknown custom header type %d",
							(void *)pf->type);
	    break;
	}

	he->name = pf->name;

	/* use first 8 characters for prompt */
	he->prompt = cpystr("        : ");
	strncpy(he->prompt, he->name, min(strlen(he->name), he->prlen - 2));

	he->rich_header = view_as_rich(he->name, he->rich_header);
	he++;
    }

    /*
     * Make sure at least *one* field is displayable...
     */
    for(index = -1, i=0, pf=header.local; pf && pf->name; pf=pf->next, i++)
      if(pf->he && !pf->he->rich_header){
	  index = i;
	  break;
      }

    /*
     * None displayable!!!  Warn and display defaults.
     */
    if(index == -1){
	q_status_message(SM_ORDER,0,5,
		     "No default-composer-hdrs matched, displaying defaults");
	for(i = 0, pf = header.local; pf; pf = pf->next, i++)
	  if((i == N_TO || i == N_CC || i == N_SUBJ || i == N_ATTCH)
	      && pf->he)
	    pf->he->rich_header = 0;
    }

    /*
     * Save information about body which set_mime_type_by_grope might change.
     * Then, if we get an error sending, we reset these things so that
     * grope can do it's thing again after we edit some more.
     */
    if ((*body)->type == TYPEMULTIPART)
	bp = save_body_particulars(&(*body)->nested.part->body);
    else
        bp = save_body_particulars(*body);


    local_redraft_pos = redraft_pos;

    /*----------------------------------------------------------------------
       Loop calling the editor until everything goes well
     ----*/
    while(1){
	int saved_user_timeout;

	/* Reset body to what it was when we started. */
	if ((*body)->type == TYPEMULTIPART)
	    reset_body_particulars(bp, &(*body)->nested.part->body);
	else
	    reset_body_particulars(bp,*body);
	/*
	 * set initial cursor position based on how many times we've been
	 * thru the loop...
	 */
	if(reply && reply->flags == REPLY_PSEUDO){
	    pbf->pine_flags |= reply->data.pico_flags;
	}
	else if(body_start){
	    pbf->pine_flags |= P_BODY;
	    body_start = 0;		/* maybe not next time */
	}
	else if(local_redraft_pos){
	    pbf->edit_offset = local_redraft_pos->offset;
	    /* set the start_here bit in correct header */
	    for(pf = header.local; pf && pf->name; pf = pf->next)
	      if(strcmp(pf->name, local_redraft_pos->hdrname) == 0
		  && pf->he){
		  pf->he->start_here = 1;
		  break;
	      }
	    
	    /* If didn't find it, we start in body. */
	    if(!pf || !pf->name)
	      pbf->pine_flags |= P_BODY;
	}
	else if(reply && reply->flags != REPLY_FORW){
	    pbf->pine_flags |= P_BODY;
	}

	/* in case these were turned on in previous pass through loop */
	if(pf_nobody){
	    pf_nobody->writehdr  = 0;
	    pf_nobody->localcopy = 0;
	}

	if(pf_fcc)
	  pf_fcc->localcopy = 0;

	/*
	 * If a sending attempt failed after we passed the message text
	 * thru a user-defined filter, "orig_so" points to the original
	 * text.  Replace the body's encoded data with the original...
	 */
	if(orig_so){
	    STORE_S **so = (STORE_S **)(((*body)->type == TYPEMULTIPART)
				? &(*body)->nested.part->body.contents.text.data
				: &(*body)->contents.text.data);
	    so_give(so);
	    *so     = orig_so;
	    orig_so = NULL;
	}

        /*
         * Convert the envelope and body to the string format that
         * pico can edit
         */
        outgoing2strings(&header, *body, &pbf->msgtext, &pbf->attachments);

	for(pf = header.local; pf && pf->name; pf = pf->next){
	    /*
	     * If this isn't the first time through this loop, we may have
	     * freed some of the FreeText headers below so that they wouldn't
	     * show up as empty headers in the finished message.  Need to
	     * alloc them again here so they can be edited.
	     */
	    if(pf->type == FreeText && pf->he && !*pf->he->realaddr)
	      *pf->he->realaddr = cpystr("");

	    if(pf->type != Attachment && pf->he && *pf->he->realaddr)
	      pf->he->maxlen = strlen(*pf->he->realaddr);
	}

	/*
	 * If From is exposed, probably by a role, then start the cursor
	 * on the first line which isn't filled in. If it isn't, then we
	 * don't move the cursor, mostly for back-compat.
	 */
	if((!reply || reply->flags == REPLY_FORW) &&
	   !local_redraft_pos && !(pbf->pine_flags & P_BODY) && he_from &&
	   (he_from->display_it || !he_from->rich_header)){
	    for(pf = header.local; pf && pf->name; pf = pf->next)
	      if(pf->he &&
		 (pf->he->display_it || !pf->he->rich_header) &&
		 pf->he->realaddr &&
		 (!*pf->he->realaddr || !**pf->he->realaddr)){
		  pf->he->start_here = 1;
		  break;
	      }
	}

#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_COMPOSER);
#endif
#if	defined(DOS) && !defined(_WINDOWS)
/*
 * dumb hack to help ensure we've got something left
 * to send with if the user runs out of precious memory
 * in the composer...	(FIX THIS!!!)
 */
	if((reserve = (char *)malloc(16384)) == NULL)
	  q_status_message(SM_ORDER | SM_DING, 0, 5,
			   "LOW MEMORY!  May be unable to complete send!");
#endif

	cancel_busy_alarm(-1);
        flush_status_messages(1);

	pbf->hibit_entered = &hibit_entered;
	*pbf->hibit_entered = 0;
    
	/* turn off user input timeout when in composer */
	saved_user_timeout = ps_global->hours_to_timeout;
	ps_global->hours_to_timeout = 0;
	dprint(1, (debugfile, "\n  ---- COMPOSER ----\n"));
	editor_result = pico(pbf);
	dprint(4, (debugfile, "... composer returns (0x%x)\n", editor_result));
	ps_global->hours_to_timeout = saved_user_timeout;

#if	defined(DOS) && !defined(_WINDOWS)
	free((char *)reserve);
#endif
#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_DEFAULT);
#endif
	fix_windsize(ps_global);
	/*
	 * Only reinitialize signals if we didn't receive an interesting
	 * one while in pico, since pico's return is part of processing that
	 * signal and it should continue to be ignored.
	 */
	if(!(editor_result & COMP_GOTHUP))
	  init_signals();        /* Pico has it's own signal stuff */

	/*
	 * We're going to save in DEADLETTER.  Dump attachments first.
	 */
	if(editor_result & COMP_CANCEL)
	  free_attachment_list(&pbf->attachments);

        /* Turn strings back into structures */
        strings2outgoing(&header, body, pbf->attachments,
			 reply ? reply->orig_charset : NULL,
			 flowing_requested);

        /* Make newsgroups NULL if it is "" (so won't show up in headers) */
	if(outgoing->newsgroups){
	    sqzspaces(outgoing->newsgroups);
	    if(!outgoing->newsgroups[0])
	      fs_give((void **)&(outgoing->newsgroups));
	}

        /* Make subject NULL if it is "" (so won't show up in headers) */
        if(outgoing->subject && !outgoing->subject[0])
          fs_give((void **)&(outgoing->subject)); 
	
	/* remove custom fields that are empty */
	for(pf = header.local; pf && pf->name; pf = pf->next){
	  if(pf->type == FreeText && pf->textbuf){
	    if(pf->textbuf[0] == '\0'){
		fs_give((void **)&pf->textbuf); 
		pf->text = NULL;
	    }
	  }
	}

        removing_trailing_white_space(fcc);

	/*-------- Stamp it with a current date -------*/
	if(outgoing->date)			/* update old date */
	  fs_give((void **)&(outgoing->date));

	if(F_ON(F_QUELL_TIMEZONE, ps_global))
	  mail_parameters(NULL, SET_DISABLE822TZTEXT, (void *) TRUE);

	rfc822_date(tmp_20k_buf);		/* format and copy new date */
	if(F_ON(F_QUELL_TIMEZONE, ps_global))
	  mail_parameters(NULL, SET_DISABLE822TZTEXT, (void *) FALSE);

	outgoing->date = (unsigned char *) cpystr(tmp_20k_buf);

	/* Set return_path based on From which is going to be used */
	if(outgoing->return_path)
	  mail_free_address(&outgoing->return_path);

	outgoing->return_path = rfc822_cpy_adr(outgoing->from);

	/*
	 * Don't ever believe the sender that is there.
	 * If From doesn't look quite right, generate our own sender.
	 */
	if(outgoing->sender)
	  mail_free_address(&outgoing->sender);

	/*
	 * If the LHS of the address doesn't match, or the RHS
	 * doesn't match one of localdomain or hostname,
	 * then add a sender line (really X-X-Sender).
	 *
	 * Don't add a personal_name since the user can change that.
	 */
	if(F_OFF(F_DISABLE_SENDER, ps_global)
	   &&
	   (!outgoing->from
	    || !outgoing->from->mailbox
	    || strucmp(outgoing->from->mailbox, ps_global->VAR_USER_ID) != 0
	    || !outgoing->from->host
	    || !(strucmp(outgoing->from->host, ps_global->localdomain) == 0
	    || strucmp(outgoing->from->host, ps_global->hostname) == 0))){

	    outgoing->sender	      = mail_newaddr();
	    outgoing->sender->mailbox = cpystr(ps_global->VAR_USER_ID);
	    outgoing->sender->host    = cpystr(ps_global->hostname);
	}

        /*----- Message is edited, now decide what to do with it ----*/
	if(editor_result & (COMP_SUSPEND | COMP_GOTHUP | COMP_CANCEL)){
            /*=========== Postpone or Interrupted message ============*/
	    CONTEXT_S *fcc_cntxt = NULL;
	    char       folder[MAXPATH+1];
	    int	       fcc_result = 0;
	    char       label[50];

	    dprint(4, (debugfile, "pine_send:%s handling\n",
		       (editor_result & COMP_SUSPEND)
			   ? "SUSPEND"
			   : (editor_result & COMP_GOTHUP)
			       ? "HUP"
			       : (editor_result & COMP_CANCEL)
				   ? "CANCEL" : "HUH?"));
	    if((editor_result & COMP_CANCEL)
	       && (F_ON(F_QUELL_DEAD_LETTER, ps_global)
	           || ps_global->deadlets == 0)){
		q_status_message(SM_ORDER, 0, 3, "Message cancelled");
		break;
	    }

	    /*
	     * The idea here is to use the Fcc: writing facility
	     * to append to the special postponed message folder...
	     *
	     * NOTE: the strategy now is to write the message and
	     * all attachments as they exist at composition time.
	     * In other words, attachments are postponed by value
	     * and not reference.  This may change later, but we'll
	     * need a local "message/external-body" type that
	     * outgoing2strings knows how to properly set up for
	     * the composer.  Maybe later...
	     */

	    label[0] = '\0';

	    if(F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global)
	       && (editor_result & COMP_SUSPEND)
	       && (check_addresses(&header) == CA_BAD)){
		/*--- Addresses didn't check out---*/
		q_status_message(SM_ORDER, 7, 7,
	      "Not allowed to postpone message until addresses are qualified");
		continue;
            }

	    /*
	     * Build the local message copy so.
	     * 
	     * In the HUP case, we'll write the bezerk delimiter by
	     * hand and output the message directly into the folder.
	     * It's not only faster, we don't have to worry about
	     * c-client reentrance and less hands paw over the data so
	     * there's less chance of a problem.
	     *
	     * In the Postpone case, just create it if the user wants to
	     * and create a temporary storage object to write into.  */
  fake_hup:
	    lmc.all_written = lmc.text_only = lmc.text_written = 0;
	    if(editor_result & (COMP_GOTHUP | COMP_CANCEL)){
		int    newfile = 1;
		time_t now = time((time_t *)0);

#if defined(DOS) || defined(OS2)
		/*
		 * we can't assume anything about root or home dirs, so
		 * just plunk it down in the same place as the pinerc
		 */
		if(!getenv("HOME")){
		    char *lc = last_cmpnt(ps_global->pinerc);
		    folder[0] = '\0';
		    if(lc != NULL){
			strncpy(folder,ps_global->pinerc,
				min(lc-ps_global->pinerc,sizeof(folder)-1));
			folder[min(lc-ps_global->pinerc,sizeof(folder)-1)]='\0';
		    }

		    strncat(folder, (editor_result & COMP_GOTHUP)
				     ? INTERRUPTED_MAIL : DEADLETTER,
			   sizeof(folder)-strlen(folder)-1);
		}
		else
#endif
		build_path(folder,
			   ps_global->VAR_OPER_DIR
			     ? ps_global->VAR_OPER_DIR : ps_global->home_dir,
			   (editor_result & COMP_GOTHUP)
			     ? INTERRUPTED_MAIL : DEADLETTER,
			   sizeof(folder));

		if(editor_result & COMP_CANCEL){
		  char filename[MAXPATH+1], newfname[MAXPATH+1], nbuf[5];

		  if(strlen(folder) + 1 < sizeof(filename))
		    for(i = ps_global->deadlets - 1; i > 0 && i < 9; i--){
			strncpy(filename, folder, sizeof(filename));
			strncpy(newfname, filename, sizeof(newfname));
			if(i > 1){
			    sprintf(nbuf, "%d", i);
			    strncat(filename, nbuf,
				    sizeof(filename)-strlen(filename));
			}

			sprintf(nbuf, "%d", i+1);
			strncat(newfname, nbuf,
				sizeof(newfname)-strlen(newfname));
			filename[sizeof(filename)-1] = '\0';
			newfname[sizeof(newfname)-1] = '\0';
			(void) rename_file(filename, newfname);
		    }

		  unlink(folder);
		}
		else
		  newfile = can_access(folder, ACCESS_EXISTS);

		if(lmc.so = so_get(FCC_SOURCE, NULL, WRITE_ACCESS)){
		  if (outgoing->from){
		    sprintf(tmp_20k_buf, "%sFrom %s@%s %.24s\015\012",
			      newfile ? "" : "\015\012",
			      outgoing->from->mailbox,
			      outgoing->from->host, ctime(&now));
		    if(!so_puts(lmc.so, tmp_20k_buf)){
		      if(editor_result & COMP_CANCEL)
			q_status_message2(SM_ORDER | SM_DING, 3, 3,
					  "Can't write \"%.200s\": %.200s",
					  folder, error_description(errno));
		      else
			dprint(1, (debugfile, "* * * CAN'T WRITE %s: %s\n",
			       folder ? folder : "?",
			       error_description(errno)));
		    }
		  }
		}
	    }
	    else{			/* Must be COMP_SUSPEND */
		if(!ps_global->VAR_POSTPONED_FOLDER
		   || !ps_global->VAR_POSTPONED_FOLDER[0]){
		    q_status_message(SM_ORDER | SM_DING, 3, 3,
				     "No postponed file defined");
		    continue;
		}

		/*
		 * Store the cursor position
		 *
		 * First find the header entry with the start_here
		 * bit set, if any. This means the editor is telling
		 * us to start on this header field next time.
		 */
		start_here_name = NULL;
		for(pf = header.local; pf && pf->name; pf = pf->next)
		  if(pf->he && pf->he->start_here){
		      start_here_name = pf->name;
		      break;
		  }
		
		/* If there wasn't one, ":" means we start in the body. */
		if(!start_here_name || !*start_here_name)
		  start_here_name = ":";

		if(ps_global->VAR_FORM_FOLDER
		   && ps_global->VAR_FORM_FOLDER[0]
		   && postpone_prompt() == 'f'){
		    strncpy(folder, ps_global->VAR_FORM_FOLDER,
			    sizeof(folder)-1);
		    folder[sizeof(folder)-1] = '\0';
		    strcpy(label, "form letter");
		}
		else{
		    strncpy(folder, ps_global->VAR_POSTPONED_FOLDER,
			    sizeof(folder)-1);
		    folder[sizeof(folder)-1] = '\0';
		    strcpy(label, "postponed message");
		}

		lmc.so = open_fcc(folder,&fcc_cntxt, 1, NULL, NULL);
	    }

	    if(lmc.so){
		size_t sz;

		/* copy fcc line to postponed or interrupted folder */
	        if(pf_fcc)
		  pf_fcc->localcopy = 1;

		/* plug error into header for later display to user */
		if((editor_result & ~0xff) && last_message_queued()){
		    pf_err->writehdr  = 1;
		    pf_err->localcopy = 1;
		    pf_err->textbuf   = cpystr(last_message_queued());
		}

		/*
		 * if reply, write (UID)folder header field so we can
		 * later flag the replied-to message \\ANSWERED
		 * DON'T save MSGNO's.
		 */
		if(reply && reply->flags == REPLY_UID){
		    char uidbuf[MAILTMPLEN], *p;
		    long i;

		    for(i = 0L, p = tmp_20k_buf; reply->data.uid.msgs[i]; i++){
			if(i)
			  sstrcpy(&p, ",");

			sstrcpy(&p,long2string(reply->data.uid.msgs[i]));
		    }

		    pf_uid->writehdr  = 1;
		    pf_uid->localcopy = 1;
		    sprintf(uidbuf, "(%d %s)(%d %lu %s)%s",
			    strlen(reply->prefix), reply->prefix,
			    i, reply->data.uid.validity,
			    tmp_20k_buf, reply->mailbox);
		    pf_uid->textbuf   = cpystr(uidbuf);

		    /*
		     * Logically, this ought to be part of pf_uid, but this
		     * was added later and so had to be in a separate header
		     * for backwards compatibility.
		     */
		    pf_mbox->writehdr  = 1;
		    pf_mbox->localcopy = 1;
		    pf_mbox->textbuf   = cpystr(reply->origmbox
						  ? reply->origmbox
						  : reply->mailbox);
		}

		/* Save cursor position */
		if(start_here_name && *start_here_name){
		    char curposbuf[MAILTMPLEN];

		    pf_curpos->writehdr  = 1;
		    pf_curpos->localcopy = 1;
		    sprintf(curposbuf, "%s %ld", start_here_name,
						 pbf->edit_offset);
		    pf_curpos->textbuf   = cpystr(curposbuf);
		}

		/*
		 * Work around c-client reply-to bug. C-client will
		 * return a reply_to in an envelope even if there is
		 * no reply-to header field. We want to note here whether
		 * the reply-to is real or not.
		 */
		if(outgoing->reply_to || hdr_is_in_list("reply-to", custom)){
		    pf_ourrep->writehdr  = 1;
		    pf_ourrep->localcopy = 1;
		    if(outgoing->reply_to)
		      pf_ourrep->textbuf   = cpystr("Full");
		    else
		      pf_ourrep->textbuf   = cpystr("Empty");
		}

		/* Save the role-specific smtp server */
		if(role && role->smtp && role->smtp[0]){
		    char  *q, *smtp = NULL;
		    char **lp;
		    size_t len = 0;

		    /*
		     * Turn the list of smtp servers into a space-
		     * delimited list in a single string.
		     */
		    for(lp = role->smtp; (q = *lp) != NULL; lp++)
		      len += (strlen(q) + 1);

		    if(len){
			smtp = (char *) fs_get(len * sizeof(char));
			smtp[0] = '\0';
			for(lp = role->smtp; (q = *lp) != NULL; lp++){
			    if(lp != role->smtp)
			      strncat(smtp, " ", len-strlen(smtp));

			    strncat(smtp, q, len-strlen(smtp));
			}
			
			smtp[len-1] = '\0';
		    }
		    
		    pf_smtp_server->writehdr  = 1;
		    pf_smtp_server->localcopy = 1;
		    if(smtp)
		      pf_smtp_server->textbuf = smtp;
		    else
		      pf_smtp_server->textbuf = cpystr("");
		}

		/* Save the role-specific nntp server */
		if(suggested_nntp_server || 
		   (role && role->nntp && role->nntp[0])){
		    char  *q, *nntp = NULL;
		    char **lp;
		    size_t len = 0;

		    if(role && role->nntp && role->nntp[0]){
			/*
			 * Turn the list of nntp servers into a space-
			 * delimited list in a single string.
			 */
			for(lp = role->nntp; (q = *lp) != NULL; lp++)
			  len += (strlen(q) + 1);

			if(len){
			    nntp = (char *) fs_get(len * sizeof(char));
			    nntp[0] = '\0';
			    for(lp = role->nntp; (q = *lp) != NULL; lp++){
				if(lp != role->nntp)
				  strncat(nntp, " ", len-strlen(nntp));

				strncat(nntp, q, len-strlen(nntp));
			    }
			
			    nntp[len-1] = '\0';
			}
		    }
		    else
		      nntp = cpystr(suggested_nntp_server);
		    
		    pf_nntp_server->writehdr  = 1;
		    pf_nntp_server->localcopy = 1;
		    if(nntp)
		      pf_nntp_server->textbuf = nntp;
		    else
		      pf_nntp_server->textbuf = cpystr("");
		}

		/*
		 * Write the list of custom headers to the
		 * X-Our-Headers header so that we can recover the
		 * list in redraft.
		 */
		sz = 0;
		for(pf = header.custom; pf && pf->name; pf = pf->next)
		  sz += strlen(pf->name) + 1;

		if(sz){
		    char *q;

		    pf_ourhdrs->writehdr  = 1;
		    pf_ourhdrs->localcopy = 1;
		    pf_ourhdrs->textbuf = (char *)fs_get(sz);
		    memset(pf_ourhdrs->textbuf, 0, sz);
		    q = pf_ourhdrs->textbuf;
		    for(pf = header.custom; pf && pf->name; pf = pf->next){
			if(pf != header.custom)
			  sstrcpy(&q, ",");

			sstrcpy(&q, pf->name);
		    }
		}

		/*
		 * We need to make sure any header values that got cleared
		 * get written to the postponed message (they won't if
		 * pf->text is NULL).  Otherwise, we can't tell previously
		 * non-existent custom headers or default values from 
		 * custom (or other) headers that got blanked in the
		 * composer...
		 */
		for(pf = header.local; pf && pf->name; pf = pf->next)
		  if(pf->type == FreeText && pf->he && !*(pf->he->realaddr))
		    *(pf->he->realaddr) = cpystr("");

		if(pine_rfc822_output(&header,*body,NULL,NULL) >= 0L){
		    if(editor_result & (COMP_GOTHUP | COMP_CANCEL)){
			char	*err;
			STORE_S *hup_so;
			gf_io_t	 gc, pc;
			int      we_cancel = 0;

			if(editor_result & COMP_CANCEL){
			    sprintf(tmp_20k_buf,
				    "Saving to \"%.100s\"", folder);
			    we_cancel = busy_alarm(1, (char *)tmp_20k_buf,
						   NULL, 0);
			}

			if(hup_so =
			    so_get(FileStar, folder, WRITE_ACCESS|OWNER_ONLY)){
			    gf_set_so_readc(&gc, lmc.so);
			    gf_set_so_writec(&pc, hup_so);
			    so_seek(lmc.so, 0L, 0); 	/* read msg copy and */
			    so_seek(hup_so, 0L, 2);	/* append to folder  */
			    gf_filter_init();
			    gf_link_filter(gf_nvtnl_local, NULL);
			    if(!(fcc_result = !(err = gf_pipe(gc, pc))))
			      dprint(1, (debugfile, "*** PIPE FAILED: %s\n",
					 err ? err : "?"));

			    gf_clear_so_readc(lmc.so);
			    gf_clear_so_writec(hup_so);
			    so_give(&hup_so);
			}
			else
			  dprint(1, (debugfile, "*** CAN'T CREATE %s: %s\n",
				     folder ? folder : "?",
				     error_description(errno)));
			
			if(we_cancel)
			  cancel_busy_alarm(-1);
		    }
		    else
		      fcc_result = write_fcc(folder, fcc_cntxt,
					     lmc.so, NULL, label, NULL);
		}

		so_give(&lmc.so);
	    }
	    else
	      dprint(1, (debugfile, "***CAN'T ALLOCATE temp store: %s ",
			 error_description(errno)));

	    if(editor_result & COMP_GOTHUP){
		/*
		 * Special Hack #291: if any hi-byte bits are set in
		 *		      editor's result, we put them there.
		 */
		if(editor_result & 0xff00)
		  exit(editor_result >> 8);

		dprint(1, (debugfile, "Save composition on HUP %sED\n",
			   fcc_result ? "SUCCEED" : "FAIL"));
		hup_signal();		/* Do what we normally do on SIGHUP */
	    }
	    else if((editor_result & COMP_SUSPEND) && fcc_result){
		if(ps_global->VAR_FORM_FOLDER
		   && ps_global->VAR_FORM_FOLDER[0]
		   && !strcmp(folder, ps_global->VAR_FORM_FOLDER))
		  q_status_message(SM_ORDER, 0, 3,
	   "Composition saved to Form Letter Folder. Select Compose to send.");
		else
		  q_status_message(SM_ORDER, 0, 3,
			 "Composition postponed. Select Compose to resume.");

                break; /* postpone went OK, get out of here */
	    }
	    else if(editor_result & COMP_CANCEL){
		char *lc = NULL;

		if(fcc_result && folder)
		  lc = last_cmpnt(folder);

		q_status_message3(SM_ORDER, 0, 3,
				  "Message cancelled%.200s%.200s%.200s",
				  (lc && *lc) ? " and copied to \"" : "",
				  (lc && *lc) ? lc : "",
				  (lc && *lc) ? "\" file" : "");
		break;
            }
	    else{
		q_status_message(SM_ORDER, 0, 4,
		    "Continuing composition.  Message not postponed or sent");
		body_start = 1;
		continue; /* postpone failed, jump back in to composer */
            }
	}
	else{
	    /*------ Must be sending mail or posting ! -----*/
	    int	       result, valid_addr, continue_with_only_fcc = 0;
	    CONTEXT_S *fcc_cntxt = NULL;

	    result = 0;
	    dprint(4, (debugfile, "=== sending: "));

            /* --- If posting, confirm with user ----*/
	    if(outgoing->newsgroups && *outgoing->newsgroups
	       && F_OFF(F_QUELL_EXTRA_POST_PROMPT, ps_global)
	       && want_to(POST_PMT, 'n', 'n', NO_HELP, WT_NORM) == 'n'){
		q_status_message(SM_ORDER, 0, 3, "Message not posted");
		dprint(4, (debugfile, "no post, continuing\n"));
		continue;
	    }

	    if(!(outgoing->to || outgoing->cc || outgoing->bcc || lcc_addr
		 || outgoing->newsgroups)){
		if(fcc && fcc[0]){
		    if(F_OFF(F_AUTO_FCC_ONLY, ps_global) &&
		       want_to("No recipients, really copy only to Fcc ",
			       'n', 'n', h_send_fcc_only, WT_NORM) != 'y')
		      continue;
		    
		    continue_with_only_fcc++;
		}
		else{
		    q_status_message(SM_ORDER, 3, 4,
				     "No recipients specified!");
		    dprint(4, (debugfile, "no recip, continuing\n"));
		    continue;
		}
	    }

	    if((valid_addr = check_addresses(&header)) == CA_BAD){
		/*--- Addresses didn't check out---*/
		dprint(4, (debugfile, "addrs failed, continuing\n"));
		continue;
	    }

	    if(F_ON(F_WARN_ABOUT_NO_TO_OR_CC, ps_global)
	       && !continue_with_only_fcc
	       && !(outgoing->to || outgoing->cc || lcc_addr
		    || outgoing->newsgroups)
	       && (want_to("No To, Cc, or Newsgroup specified, send anyway ",
			   'n', 'n', h_send_check_to_cc, WT_NORM) != 'y')){
		dprint(4, (debugfile, "No To or CC or Newsgroup, continuing\n"));
		if(local_redraft_pos && local_redraft_pos != redraft_pos)
		  free_redraft_pos(&local_redraft_pos);
		
		local_redraft_pos
			= (REDRAFT_POS_S *) fs_get(sizeof(*local_redraft_pos));
		memset((void *) local_redraft_pos,0,sizeof(*local_redraft_pos));
		local_redraft_pos->hdrname = cpystr(TONAME);
		continue;
	    }

	    if(F_ON(F_WARN_ABOUT_NO_SUBJECT, ps_global)
	       && check_for_subject(&header) == CF_MISSING){
		dprint(4, (debugfile, "No subject, continuing\n"));
		if(local_redraft_pos && local_redraft_pos != redraft_pos)
		  free_redraft_pos(&local_redraft_pos);
		
		local_redraft_pos
			= (REDRAFT_POS_S *) fs_get(sizeof(*local_redraft_pos));
		memset((void *) local_redraft_pos,0,sizeof(*local_redraft_pos));
		local_redraft_pos->hdrname = cpystr(SUBJNAME);
		continue;
	    }

	    set_last_fcc(fcc);

            /*---- Check out fcc -----*/
            if(fcc && *fcc){
		/*
		 * If special name "inbox" then replace it with the
		 * real inbox path.
		 */
		if(ps_global->VAR_INBOX_PATH
		   && strucmp(fcc, ps_global->inbox_name) == 0){
		    char *replace_fcc;

		    replace_fcc = cpystr(ps_global->VAR_INBOX_PATH);
		    fs_give((void **)&fcc);
		    fcc = replace_fcc;
		}

		lmc.all_written = lmc.text_written = 0;
/*		lmc.text_only = F_ON(F_NO_FCC_ATTACH, ps_global) != 0;*/
	        if(!(lmc.so = open_fcc(fcc, &fcc_cntxt, 0, NULL, NULL))){
		    /* ---- Open or allocation of fcc failed ----- */
		    dprint(4, (debugfile,"can't open/allocate fcc, cont'g\n"));

		    /*
		     * Find field entry associated with fcc, and start
		     * composer on it...
		     */
		    for(pf = header.local; pf && pf->name; pf = pf->next)
		      if(pf->type == Fcc && pf->he)
			pf->he->start_here = 1;

		    continue;
		}
		else
		  so_truncate(lmc.so, fcc_size_guess(*body) + 2048);
            }
	    else
	      lmc.so = NULL;

            /*---- Take care of any requested prefiltering ----*/
	    if(sending_filter_requested
	       && !filter_message_text(sending_filter_requested, outgoing,
				       *body, &orig_so, &header)){
		q_status_message1(SM_ORDER, 3, 3,
				 "Problem filtering!  Nothing sent%.200s.",
				 fcc ? " or saved to fcc" : "");
		continue;
	    }

            /*------ Actually post  -------*/
            if(outgoing->newsgroups){
		char **alt_nntp = NULL, *alt_nntp_p[2];
		if(((role && role->nntp)
		    || suggested_nntp_server)){
		    if(ps_global->FIX_NNTP_SERVER
		       && ps_global->FIX_NNTP_SERVER[0])
		      q_status_message(SM_ORDER | SM_DING, 5, 5,
				       "Using nntp-server that is administratively fixed");
		    else if(role && role->nntp)
		      alt_nntp = role->nntp;
		    else{
			alt_nntp_p[0] = suggested_nntp_server;
			alt_nntp_p[1] = NULL;
			alt_nntp = alt_nntp_p;
		    }
		}
		if(news_poster(&header, *body, alt_nntp) < 0){
		    dprint(1, (debugfile, "Post failed, continuing\n"));
		    if(outgoing->message_id)
		      fs_give((void **) &outgoing->message_id);

		    outgoing->message_id = generate_message_id();

		    continue;
		}
		else
		  result |= P_NEWS_WIN;
	    }

	    /*
	     * BUG: IF we've posted the message *and* an fcc was specified
	     * then we've already got a neatly formatted message in the
	     * lmc.so.  It'd be nice not to have to re-encode everything
	     * to insert it into the smtp slot...
	     */

	    /*
	     * Turn on "undisclosed recipients" header if no To or cc.
	     */
            if(!(outgoing->to || outgoing->cc || outgoing->newsgroups)
	      && (outgoing->bcc || lcc_addr) && pf_nobody && pf_nobody->addr){
		pf_nobody->writehdr  = 1;
		pf_nobody->localcopy = 1;
	    }

#if	defined(BACKGROUND_POST) && defined(SIGCHLD)
	    /*
	     * If requested, launch backgroud posting...
	     */
	    if(background_requested && !verbose_requested){
		ps_global->post = (POST_S *)fs_get(sizeof(POST_S));
		memset(ps_global->post, 0, sizeof(POST_S));
		if(fcc)
		  ps_global->post->fcc = cpystr(fcc);

		if((ps_global->post->pid = fork()) == 0){
		    /*
		     * Put us in new process group...
		     */
		    setpgrp(0, ps_global->post->pid);

		    /* BUG: should fix argv[0] to indicate what we're up to */

		    /*
		     * If there are any live streams, pretend we never
		     * knew them.  Problem is two processes writing
		     * same server process.
		     * This is not clean but we're just going to exit
		     * right away anyway. We just want to be sure to leave
		     * the stuff that the parent is going to use alone.
		     * The next three lines will disable the re-use of the
		     * existing streams and cause us to open a new one if
		     * needed.
		     */
		    ps_global->mail_stream = NULL;
		    ps_global->s_pool.streams = NULL;
		    ps_global->s_pool.nstream = 0;

		    /* quell any display output */
		    ps_global->in_init_seq = 1;

		    /*------- Actually mail the message ------*/
		    if(valid_addr == CA_OK 
		       && (outgoing->to || outgoing->cc
			   || outgoing->bcc || lcc_addr)){
			char **alt_smtp = NULL;

			if(role && role->smtp){
			    if(ps_global->FIX_SMTP_SERVER
			       && ps_global->FIX_SMTP_SERVER[0])
			      q_status_message(SM_ORDER | SM_DING, 5, 5, "Use of a role-defined smtp-server is administratively prohibited");
			    else
			      alt_smtp = role->smtp;
			}

		        result |= (call_mailer(&header, *body, alt_smtp) > 0)
				    ? P_MAIL_WIN : P_MAIL_LOSE;
		    }

		    /*----- Was there an fcc involved? -----*/
		    if(lmc.so){
			/*------ Write it if at least something worked ------*/
			if((result & (P_MAIL_WIN | P_NEWS_WIN))
			   || (!(result & (P_MAIL_BITS | P_NEWS_BITS))
			       && pine_rfc822_output(&header, *body,
						     NULL, NULL))){
			    char label[50];

			    strcpy(label, "Fcc");
			    if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC))
			      sprintf(label + 3, " to %.40s", fcc);

			    /*-- Now actually copy to fcc folder and close --*/
			    result |= (write_fcc(fcc, fcc_cntxt, lmc.so,
						 NULL, label,
					      F_ON(F_MARK_FCC_SEEN, ps_global)
						  ? "\\SEEN" : NULL))
					? P_FCC_WIN : P_FCC_LOSE;
			}
			else if(!(result & (P_MAIL_BITS | P_NEWS_BITS))){
			    q_status_message(SM_ORDER, 3, 5,
					    "Fcc Failed!.  No message saved.");
			    dprint(1, (debugfile,
				       "explicit fcc write failed!\n"));
			    result |= P_FCC_LOSE;
			}

			so_give(&lmc.so);
		    }

		    if(result & (P_MAIL_LOSE | P_NEWS_LOSE | P_FCC_LOSE)){
			/*
			 * Encode child's result in hi-byte of
			 * editor's result
			 */
			editor_result = ((result << 8) | COMP_GOTHUP);
			goto fake_hup;
		    }

		    exit(result);
		}

		if(ps_global->post->pid > 0){
		    q_status_message(SM_ORDER, 3, 3,
				     "Message handed off for posting");
		    break;		/* up to our child now */
		}
		else{
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      "Can't fork for send: %.200s",
				      error_description(errno));
		    if(ps_global->post->fcc)
		      fs_give((void **) &ps_global->post->fcc);

		    fs_give((void **) &ps_global->post);
		}

		if(lmc.so)	/* throw away unused store obj */
		  so_give(&lmc.so);

		if(outgoing->message_id)
		  fs_give((void **) &outgoing->message_id);

		outgoing->message_id = generate_message_id();

		continue;		/* if we got here, there was a prob */
	    }
#endif /* BACKGROUND_POST */

            /*------- Actually mail the message ------*/
            if(valid_addr == CA_OK
	       && (outgoing->to || outgoing->cc || outgoing->bcc || lcc_addr)){
		char **alt_smtp = NULL;

		if(role && role->smtp){
		    if(ps_global->FIX_SMTP_SERVER
		       && ps_global->FIX_SMTP_SERVER[0])
		      q_status_message(SM_ORDER | SM_DING, 5, 5, "Use of a role-defined smtp-server is administratively prohibited");
		    else
		      alt_smtp = role->smtp;
		}

		result |= (call_mailer(&header, *body, alt_smtp) > 0)
			    ? P_MAIL_WIN : P_MAIL_LOSE;
	    }

	    /*----- Was there an fcc involved? -----*/
            if(lmc.so){
		/*------ Write it if at least something worked ------*/
		if((result & (P_MAIL_WIN | P_NEWS_WIN))
		   || (!(result & (P_MAIL_BITS | P_NEWS_BITS))
		       && pine_rfc822_output(&header, *body, NULL, NULL))){
		    char label[50];

		    strcpy(label, "Fcc");
		    if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC))
		      sprintf(label + 3, " to %.40s", fcc);

		    /*-- Now actually copy to fcc folder and close --*/
		    result |= (write_fcc(fcc, fcc_cntxt, lmc.so, NULL, label,
					 F_ON(F_MARK_FCC_SEEN, ps_global)
					    ? "\\SEEN" : NULL))
				? P_FCC_WIN : P_FCC_LOSE;
		}
		else if(!(result & (P_MAIL_BITS | P_NEWS_BITS))){
		    q_status_message(SM_ORDER,3,5,
			"Fcc Failed!.  No message saved.");
		    dprint(1, (debugfile, "explicit fcc write failed!\n"));
		    result |= P_FCC_LOSE;
		}

		so_give(&lmc.so);
	    }

            /*----- Mail Post FAILED, back to composer -----*/
            if(result & (P_MAIL_LOSE | P_FCC_LOSE)){
		dprint(1, (debugfile, "Send failed, continuing\n"));

		if(result & P_FCC_LOSE){
		    /*
		     * Find field entry associated with fcc, and start
		     * composer on it...
		     */
		    for(pf = header.local; pf && pf->name; pf = pf->next)
		      if(pf->type == Fcc && pf->he)
			pf->he->start_here = 1;

		    q_status_message(SM_ORDER | SM_DING, 3, 3,
				     pine_send_status(result, fcc,
						      tmp_20k_buf, NULL));
		}

		if(outgoing->message_id)
		  fs_give((void **) &outgoing->message_id);

		outgoing->message_id = generate_message_id();

		continue;
	    }

	    /*
	     * If message sent *completely* successfully, there's a
	     * reply struct AND we're allowed to write back state, do it.
	     * But also protect against shifted message numbers due 
	     * to new mail arrival.  Since the number passed is based
	     * on the real imap msg no, AND we're sure no expunge has 
	     * been done, just fix up the sorted number...
	     */
	    update_answered_flags(reply);

            /*----- Signed, sealed, delivered! ------*/
	    q_status_message(SM_ORDER, 0, 3,
			     pine_send_status(result, fcc, tmp_20k_buf, NULL));

            break; /* All's well, pop out of here */
        }
    }

    if(orig_so)
      so_give(&orig_so);

    if(fcc)
      fs_give((void **)&fcc);
    
    free_body_particulars(bp);

    free_attachment_list(&pbf->attachments);

    standard_picobuf_teardown(pbf);

    for(i=0; i < fixed_cnt; i++){
	if(pfields[i].textbuf)
	  fs_give((void **)&pfields[i].textbuf);

	fs_give((void **)&pfields[i].name);
    }

    if(lcc_addr)
      mail_free_address(&lcc_addr);
    
    if(nobody_addr)
      mail_free_address(&nobody_addr);
    
    free_customs(header.custom);
    fs_give((void **)&pfields);
    free_headents(&headents);
    fs_give((void **)&sending_order);
    if(suggested_nntp_server)
      fs_give((void **)&suggested_nntp_server);
    if(title)
      fs_give((void **)&title);
    
    if(local_redraft_pos && local_redraft_pos != redraft_pos)
      free_redraft_pos(&local_redraft_pos);

    pbf = save_previous_pbuf;
    g_rolenick = NULL;

    dprint(4, (debugfile, "=== send returning ===\n"));
}


int
postpone_prompt()
{
    static ESCKEY_S pstpn_form_opt[] = { {'p', 'p', "P", "Postponed Folder"},
					 {'f', 'f', "F", "Form Letter Folder"},
					 {-1, 0, NULL, NULL} };

    return(radio_buttons(PSTPN_FORM_PMT, -FOOTER_ROWS(ps_global),
			 pstpn_form_opt, 'p', 0, NO_HELP, RB_FLUSH_IN, NULL));
}



/*
 * This is specialized routine. It assumes that the only things that we
 * care about restoring are the body type, subtype, encoding and the
 * state of the charset parameter. It also assumes that if the charset
 * parameter exists when we save it, it won't be removed later.
 */
BODY_PARTICULARS_S *
save_body_particulars(body)
    BODY *body;
{
    BODY_PARTICULARS_S *bp;
    PARAMETER *pm;

    bp = (BODY_PARTICULARS_S *)fs_get(sizeof(BODY_PARTICULARS_S));

    bp->type      = body->type;
    bp->encoding  = body->encoding;
    bp->subtype   = body->subtype ? cpystr(body->subtype) : NULL;
    bp->parameter = body->parameter;
    for(pm = bp->parameter;
	pm && strucmp(pm->attribute, "charset") != 0;
	pm = pm->next)
      ;/* searching for possible charset parameter */
    
    if(pm){					/* found one */
	bp->had_csp = 1;		/* saved body had charset parameter */
	bp->charset = pm->value ? cpystr(pm->value) : NULL;
    }
    else{
	bp->had_csp = 0;
	bp->charset = NULL;
    }

    return(bp);
}


void
reset_body_particulars(bp, body)
    BODY_PARTICULARS_S *bp;
    BODY               *body;
{
    body->type     = bp->type;
    body->encoding = bp->encoding;
    if(body->subtype)
      fs_give((void **)&body->subtype);

    body->subtype  = bp->subtype ? cpystr(bp->subtype) : NULL;

    if(bp->parameter){
	PARAMETER *pm, *pm_prev = NULL;

	for(pm = body->parameter;
	    pm && strucmp(pm->attribute, "charset") != 0;
	    pm = pm->next)
	  pm_prev = pm;

	if(pm){				/* body has charset parameter */
	    if(bp->had_csp){		/* reset to what it used to be */
		if(pm->value)
		  fs_give((void **)&pm->value);

		pm->value = bp->charset ? cpystr(bp->charset) : NULL;
	    }
	    else{			/* remove charset parameter */
		if(pm_prev)
		  pm_prev->next = pm->next;
		else
		  body->parameter = pm->next;

		mail_free_body_parameter(&pm);
	    }
	}
	else{
	    if(bp->had_csp){
		/*
		 * This can't happen because grope never removes
		 * the charset parameter.
		 */
		q_status_message(SM_ORDER | SM_DING, 5, 5,
"Programmer error: saved charset but no current charset param in pine_send");
	    }
	    /*
	    else{
		ok, still no parameter 
	    }
	    */
	}
    }
    else{
	if(body->parameter)
	  mail_free_body_parameter(&body->parameter);
	
	body->parameter = NULL;
    }
}


void
free_body_particulars(bp)
    BODY_PARTICULARS_S *bp;
{
    if(bp){
	if(bp->subtype)
	  fs_give((void **)&bp->subtype);

	if(bp->charset)
	  fs_give((void **)&bp->charset);

	fs_give((void **)&bp);
    }
}


void
free_headents(head)
    struct headerentry **head;
{
    struct headerentry  *he;
    PrivateTop          *pt;

    if(head && *head){
	for(he = *head; he->name; he++)
	  if(he->bldr_private){
	      pt = (PrivateTop *)he->bldr_private;
	      free_privatetop(&pt);
	  }

	fs_give((void **)head);
    }
}



/*----------------------------------------------------------------------
   Build a status message suitable for framing

Returns: pointer to resulting buffer
  ---*/
char *
pine_send_status(result, fcc_name, buf, goodorbad)
    int   result;
    char *fcc_name;
    char *buf;
    int  *goodorbad;
{
    int   avail = ps_global->ttyo->screen_cols - 2;
    int   fixedneed, need, lenfcc;
    char *part1, *part2, *part3, *part4, *part5;
    char  fbuf[MAILTMPLEN+1];

    part1 = (result & P_NEWS_WIN)
	     ? "posted"
	     : (result & P_NEWS_LOSE)
	      ? "NOT POSTED"
	      : "";
    part2 = ((result & P_NEWS_BITS) && (result & P_MAIL_BITS)
	     && (result & P_FCC_BITS))
	     ? ", "
	     : ((result & P_NEWS_BITS) && (result & (P_MAIL_BITS | P_FCC_BITS)))
	      ? " and "
	      : "";
    part3 = (result & P_MAIL_WIN)
	     ? "sent"
	     : (result & P_MAIL_LOSE)
	      ? "NOT SENT"
	      : "";
    part4 = ((result & P_MAIL_BITS) && (result & P_FCC_BITS))
	     ? " and "
	     : "";
    part5 = ((result & P_FCC_WIN) && !(result & (P_MAIL_WIN | P_NEWS_WIN)))
	     ? "ONLY copied to " 
	     : (result & P_FCC_WIN)
	      ? "copied to "
	      : (result & P_FCC_LOSE)
	       ? "NOT copied to "
	       : "";
    lenfcc = min(sizeof(fbuf)-1, (result & P_FCC_BITS) ? strlen(fcc_name) : 0);
    
    fixedneed = 9 + strlen(part1) + strlen(part2) + strlen(part3) +
	strlen(part4) + strlen(part5);
    need = fixedneed + ((result & P_FCC_BITS) ? 2 : 0) + lenfcc;

    if(need > avail && fixedneed + 3 >= avail){
	/* dots on end of fixed, no fcc */
	sprintf(fbuf, "Message %.20s%.20s%.20s%.20s%.20s     ",
		part1, part2, part3, part4, part5);
	short_str(fbuf, buf, avail, EndDots);
    }
    else if(need > avail){
	/* include whole fixed part, quotes and dots at end of fcc name */
	if(lenfcc > 0)
	  lenfcc = max(1, lenfcc-(need-avail));

	sprintf(buf, "Message %.20s%.20s%.20s%.20s%.20s%.20s%.200s%.20s.",
		part1, part2, part3, part4, part5,
		(result & P_FCC_BITS) ? "\"" : "",
		short_str((result & P_FCC_BITS) ? fcc_name : "",
			  fbuf, lenfcc, FrontDots), 
		(result & P_FCC_BITS) ? "\"" : "");
    }
    else{
	/* whole thing */
	sprintf(buf, "Message %.20s%.20s%.20s%.20s%.20s%.20s%.200s%.20s.",
		part1, part2, part3, part4, part5,
		(result & P_FCC_BITS) ? "\"" : "",
		(result & P_FCC_BITS) ? fcc_name : "",
		(result & P_FCC_BITS) ? "\"" : "");
    }

    if(goodorbad)
      *goodorbad = (result & (P_MAIL_LOSE | P_NEWS_LOSE | P_FCC_LOSE)) == 0;

    return(buf);
}


/*
 * Check for subject in outgoing message.
 * 
 * Asks user whether to proceed with no subject.
 */
int
check_for_subject(header)
    METAENV *header;
{
    PINEFIELD *pf;
    int        rv = CF_OK;

    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Subject){
	  if(pf->text && *pf->text && **pf->text)
	    rv = CF_OK;
	  else{
	      if(want_to("No Subject, send anyway ",
		         'n', 'n', h_send_check_subj, WT_NORM) == 'y') 
		rv = CF_OK;
	      else
		rv = CF_MISSING;
	  }

	  break;
      }
	      

    return(rv);
}


/*----------------------------------------------------------------------
   Check for addresses the user is not permitted to send to, or probably
   doesn't want to send to
   
Returns:  0 if OK
          1 if there are only empty groups
         -1 if the message shouldn't be sent

Queues a message indicating what happened
  ---*/
int
check_addresses(header)
    METAENV *header;
{
    PINEFIELD *pf;
    ADDRESS *a;
    int	     send_daemon = 0, rv = CA_EMPTY;

    /*---- Is he/she trying to send mail to the mailer-daemon ----*/
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr)
	for(a = *pf->addr; a != NULL; a = a->next){
	    if(a->host && (a->host[0] == '.'
			   || (F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global)
			       && a->host[0] == '@'))){
		q_status_message2(SM_ORDER, 4, 7,
				  "Can't send to address %.200s: %.200s",
				  a->mailbox,
				  (a->host[0] == '.')
				    ? a->host
				    : "not in addressbook");
		return(CA_BAD);
	    }
	    else if(ps_global->restricted
		    && !address_is_us(*pf->addr, ps_global)){
		q_status_message(SM_ORDER, 3, 3,
	"Restricted demo version of Pine. You may only send mail to yourself");
		return(CA_BAD);
	    }
	    else if(a->mailbox && strucmp(a->mailbox, "mailer-daemon") == 0
		    && !send_daemon){
		send_daemon = 1;
		if(want_to("Really send this message to the MAILER-DAEMON",
			   'n', 'n', NO_HELP, WT_NORM) == 'n')
		  return(CA_BAD);
		rv = CA_OK;
	    }
	    else if(a->mailbox && a->host){
		rv = CA_OK;
	    }
	}

    return(rv);
}


/*----------------------------------------------------------------------
    Validate the given subject relative to any news groups.
     
Args: none

Returns: always returns 1, but also returns error if
----*/      
int
valid_subject(given, expanded, error, fcc, mangled)
    char	 *given,
		**expanded,
		**error;
    BUILDER_ARG  *fcc;
    int          *mangled;
{
    struct headerentry *hp;

    if(expanded)
      *expanded = cpystr(given);

    if(error){
	/*
	 * Now look for any header entry we passed to pico that has to do
	 * with news.  If there's no subject, gripe.
	 */
	for(hp = pbf->headents; hp->prompt; hp++)
	  if(hp->help == h_composer_news){
	      if(hp->hd_text->text[0] && !*given)
		*error = cpystr(
			"News postings MUST have a subject!  Please add one!");

	      break;
	  }
    }

    return(0);
}


PCOLORS *
colors_for_pico()
{
    PCOLORS *colors = NULL;
    struct variable *vars = ps_global->vars;

    if (pico_usingcolor()){
      colors = (PCOLORS *)fs_get(sizeof(PCOLORS));
      if (VAR_TITLE_FORE_COLOR && VAR_TITLE_BACK_COLOR){
	colors->tbcp = new_color_pair(VAR_TITLE_FORE_COLOR,
				      VAR_TITLE_BACK_COLOR);
      }
      else colors->tbcp = NULL;

      if (VAR_KEYLABEL_FORE_COLOR && VAR_KEYLABEL_BACK_COLOR){
	colors->klcp = new_color_pair(VAR_KEYLABEL_FORE_COLOR,
				      VAR_KEYLABEL_BACK_COLOR);
	if (!pico_is_good_colorpair(colors->klcp))
	  free_color_pair(&colors->klcp);
      }
      else colors->klcp = NULL;

      if (colors->klcp && VAR_KEYNAME_FORE_COLOR && VAR_KEYNAME_BACK_COLOR){
	colors->kncp = new_color_pair(VAR_KEYNAME_FORE_COLOR, 
				      VAR_KEYNAME_BACK_COLOR);
      }
      else colors->kncp = NULL;
      if (VAR_STATUS_FORE_COLOR && VAR_STATUS_BACK_COLOR){
	colors->stcp = new_color_pair(VAR_STATUS_FORE_COLOR, 
				      VAR_STATUS_BACK_COLOR);
      }
      else colors->stcp = NULL;
    }
    
    return colors;
}

void
free_pcolors(colors)
    PCOLORS  **colors;
{
    if (*colors){
	  if ((*colors)->tbcp)
		free_color_pair(&(*colors)->tbcp);
	  if ((*colors)->kncp)
		free_color_pair(&(*colors)->kncp);
	  if ((*colors)->klcp)
		free_color_pair(&(*colors)->klcp);
	  if ((*colors)->stcp)
		free_color_pair(&(*colors)->stcp);
	  fs_give((void **)colors);
	  *colors = NULL;
	}
}
      
/*----------------------------------------------------------------------
    Call back for pico to use to check for new mail.
     
Args: cursor -- pointer to in to tell caller if cursor location changed
                if NULL, turn off cursor positioning.
      timing -- whether or not it's a good time to check 
 

Returns: returns 1 on success, zero on error.
----*/      
long
new_mail_for_pico(timing, status)
    int  timing, status;
{
    /*
     * If we're not interested in the status, don't display the busy
     * cue either...
     */
    /* don't know where the cursor's been, reset it */
    clear_cursor_pos();
    return(new_mail(0, timing,
		    (status ? NM_STATUS_MSG : NM_NONE) | NM_DEFER_SORT
						       | NM_FROM_COMPOSER));
}


void
cmd_input_for_pico()
{
    zero_new_mail_count();
}




/*----------------------------------------------------------------------
    Call back for pico to insert the specified message's text
     
Args: n -- message number to format
      f -- function to use to output the formatted message
 

Returns: returns msg number formatted on success, zero on error.
----*/      
long
message_format_for_pico(n, f)
    long n;
    int  (*f) PROTO((int));
{
    ENVELOPE *e;
    BODY     *b;
    char     *old_quote = NULL;
    long      rv = n;

    if(!(n > 0L && n <= mn_get_total(ps_global->msgmap)
       && (e = pine_mail_fetchstructure(ps_global->mail_stream,
				        mn_m2raw(ps_global->msgmap, n), &b)))){
	q_status_message(SM_ORDER|SM_DING,3,3,"Error inserting Message");
	flush_status_messages(0);
	return(0L);
    }

    /* temporarily assign a new quote string */
    old_quote = pbf->quote_str;
    pbf->quote_str = reply_quote_str(e);

    /* build separator line */
    reply_delimiter(e, NULL, f);

    /* actually write message text */
    if(!format_message(mn_m2raw(ps_global->msgmap, n), e, b, NULL,
		       FM_NEW_MESS | FM_DISPLAY | FM_NOCOLOR | FM_NOINDENT, f)){
	q_status_message(SM_ORDER|SM_DING,3,3,"Error inserting Message");
	flush_status_messages(0);
	rv = 0L;
    }

    fs_give((void **)&pbf->quote_str);
    pbf->quote_str = old_quote;
    return(rv);
}



/*----------------------------------------------------------------------
    Call back for pico to prompt the user for exit confirmation

Args: dflt -- default answer for confirmation prompt

Returns: either NULL if the user accepts exit, or string containing
	 reason why the user declined.
----*/      
char *
send_exit_for_pico(he, redraw_pico, allow_flowed)
    struct headerentry *he;
    void (*redraw_pico)();
    int    allow_flowed;
{
    int	       i, rv, c, verbose_label = 0, bg_label = 0, old_suspend;
    int        dsn_label = 0, fcc_label = 0, dsn_show, lparen;
    int        flowing_label = 0, double_rad;
    char      *rstr = NULL, *p, *lc, *optp;
    char       dsn_string[30];
    void     (*redraw)() = ps_global->redrawer;
    ESCKEY_S   opts[15];
    struct filters {
	char  *filter;
	int    index;
	struct filters *prev, *next;
    } *filters = NULL, *fp;

    sending_filter_requested = NULL;
    verbose_requested        = 0;
    background_requested     = 0;
    dsn_requested            = 0;
    flowing_requested        = allow_flowed ? 1 : 0;
    lmc.text_only            = F_ON(F_NO_FCC_ATTACH, ps_global) != 0;

    if(background_posting(FALSE))
      return("Can't send while background posting. Use postpone.");

    if(F_ON(F_SEND_WO_CONFIRM, ps_global))
      return(NULL);

    ps_global->redrawer = redraw_pico;

    if(old_suspend = F_ON(F_CAN_SUSPEND, ps_global))
      F_SET(F_CAN_SUSPEND, ps_global, 0);

    /*
     * Build list of available filters...
     */
    for(i=0; ps_global->VAR_SEND_FILTER && ps_global->VAR_SEND_FILTER[i]; i++){
	for(p = ps_global->VAR_SEND_FILTER[i];
	    *p && !isspace((unsigned char)*p); p++)
	  ;

	c  = *p;
	*p = '\0';
	if(!(is_absolute_path(ps_global->VAR_SEND_FILTER[i])
	     && can_access(ps_global->VAR_SEND_FILTER[i],EXECUTE_ACCESS) ==0)){
	    *p = c;
	    continue;
	}

	fp	   = (struct filters *)fs_get(sizeof(struct filters));
	fp->index  = i;
	if(lc = last_cmpnt(ps_global->VAR_SEND_FILTER[i])){
	    fp->filter = cpystr(lc);
	}
	else if((p - ps_global->VAR_SEND_FILTER[i]) > 20){
	    sprintf(tmp_20k_buf, "...%s", p - 17);
	    fp->filter = cpystr(tmp_20k_buf);
	}
	else
	  fp->filter = cpystr(ps_global->VAR_SEND_FILTER[i]);

	*p = c;

	if(filters){
	    fp->next	   = filters;
	    fp->prev	   = filters->prev;
	    fp->prev->next = filters->prev = fp;
	}
	else{
	    filters = (struct filters *)fs_get(sizeof(struct filters));
	    filters->index  = -1;
	    filters->filter = NULL;
	    filters->next = filters->prev = fp;
	    fp->next = fp->prev = filters;
	}
    }

    i = 0;
    opts[i].ch      = 'y';
    opts[i].rval    = 'y';
    opts[i].name    = "Y";
    opts[i++].label = "Yes";

    opts[i].ch      = 'n';
    opts[i].rval    = 'n';
    opts[i].name    = "N";
    opts[i++].label = "No";

    if(filters){
	/* set global_filter_pointer to desired filter or NULL if none */
	/* prepare two keymenu slots for selecting filter */
	opts[i].ch      = ctrl('P');
	opts[i].rval    = 10;
	opts[i].name    = "^P";
	opts[i++].label = "Prev Filter";

	opts[i].ch      = ctrl('N');
	opts[i].rval    = 11;
	opts[i].name    = "^N";
	opts[i++].label = "Next Filter";

	if(F_ON(F_FIRST_SEND_FILTER_DFLT, ps_global))
	  filters = filters->next;
    }

    if(F_ON(F_VERBOSE_POST, ps_global)){
	/* setup keymenu slot to toggle verbose mode */
	opts[i].ch    = ctrl('W');
	opts[i].rval  = 12;
	opts[i].name  = "^W";
	verbose_label = i++;
    }

    if(allow_flowed){
	/* setup keymenu slot to toggle flowed mode */
	opts[i].ch    = ctrl('V');
	opts[i].rval  = 22;
	opts[i].name  = "^V";
	flowing_label = i++;
	flowing_requested = 1;
    }

    if(F_ON(F_NO_FCC_ATTACH, ps_global)){
	/* setup keymenu slot to toggle attacment on fcc */
	opts[i].ch      = ctrl('F');
	opts[i].rval    = 21;
	opts[i].name    = "^F";
	fcc_label       = i++;
    }

#if	defined(BACKGROUND_POST) && defined(SIGCHLD)
    if(F_ON(F_BACKGROUND_POST, ps_global)){
	opts[i].ch    = ctrl('R');
	opts[i].rval  = 15;
	opts[i].name  = "^R";
	bg_label = i++;
    }
#endif

    double_rad = i;

    if(F_ON(F_DSN, ps_global)){
	/* setup keymenu slots to toggle dsn bits */
	opts[i].ch      = 'd';
	opts[i].rval    = 'd';
	opts[i].name    = "D";
	opts[i].label   = "DSNOpts";
	dsn_label       = i++;
	opts[i].ch      = -2;
	opts[i].rval    = 's';
	opts[i].name    = "S";
	opts[i++].label = "";
	opts[i].ch      = -2;
	opts[i].rval    = 'x';
	opts[i].name    = "X";
	opts[i++].label = "";
	opts[i].ch      = -2;
	opts[i].rval    = 'h';
	opts[i].name    = "H";
	opts[i++].label = "";
    }

    if(filters){
	opts[i].ch      = KEY_UP;
	opts[i].rval    = 10;
	opts[i].name    = "";
	opts[i++].label = "";

	opts[i].ch      = KEY_DOWN;
	opts[i].rval    = 11;
	opts[i].name    = "";
	opts[i++].label = "";
    }

    opts[i].ch = -1;

    fix_windsize(ps_global);

    while(1){
	if(filters && filters->filter && (p = strindex(filters->filter, ' ')))
	  *p = '\0';
	else
	  p = NULL;

	lparen = 0;
	dsn_show = (dsn_requested & DSN_SHOW);
	strcpy(tmp_20k_buf, "Send message");
	optp = &tmp_20k_buf[12];

	if(F_ON(F_NO_FCC_ATTACH, ps_global) && !lmc.text_only)
	  sstrcpy(&optp, " and Fcc Atmts");

	if(allow_flowed && !flowing_requested){
	    if(!lparen){
	      *optp++ = ' ';
	      *optp++ = '(';
	      lparen++;
	    }
	    else 
	      *optp++ = ' ';

	    sstrcpy(&optp, "not flowed");
	}

	if(filters){
	    if(!lparen){
	      *optp++ = ' ';
	      *optp++ = '(';
	      lparen++;
	    }
	    else{
		*optp++ = ',';
		*optp++ = ' ';
	    }

	    if(filters->filter){
		sstrcpy(&optp, "filtered thru \"");
		sstrcpy(&optp, filters->filter);
		*optp++ = '\"';
	    }
	    else
	      sstrcpy(&optp, "unfiltered");
	}

	if(verbose_requested || background_requested){
	    if(!lparen){
	      *optp++ = ' ';
	      *optp++ = '(';
	      lparen++;
	    }
	    else 
	      *optp++ = ' ';

	    sstrcpy(&optp, "in ");
	    if(verbose_requested)
	      sstrcpy(&optp, "verbose ");

	    if(background_requested)
	      sstrcpy(&optp, "background ");

	    sstrcpy(&optp, "mode");
	}

	if(g_rolenick && !(he && he[N_FROM].dirty)){
	    if(!lparen){
	      *optp++ = ' ';
	      *optp++ = '(';
	      lparen++;
	    }
	    else 
	      *optp++ = ' ';

	    sstrcpy(&optp, "as \"");
	    sstrcpy(&optp, g_rolenick);
	    sstrcpy(&optp, "\"");
	}

	if(dsn_show){
	    if(!lparen){
	      *optp++ = ' ';
	      *optp++ = '(';
	      lparen++;
	    }
	    else{
		*optp++ = ',';
		*optp++ = ' ';
	    }

	    sstrcpy(&optp, dsn_string);
	}

	if(lparen)
	  *optp++ = ')';

        sstrcpy(&optp, "? ");

	if(p)
	  *p = ' ';

	if(flowing_label)
	  opts[flowing_label].label = flowing_requested ? "NoFlow" : "Flow";

	if(verbose_label)
	  opts[verbose_label].label = verbose_requested ? "Normal" : "Verbose";

	if(bg_label)
	  opts[bg_label].label = background_requested
				   ? "Foreground" : "Background";

	if(fcc_label)
	  opts[fcc_label].label = lmc.text_only ? "Fcc Attchmnts"
						: "No Fcc Atmts ";

	if(F_ON(F_DSN, ps_global)){
	    if(dsn_requested & DSN_SHOW){
		opts[dsn_label].label   = (dsn_requested & DSN_DELAY)
					   ? "NoDelay" : "Delay";
		opts[dsn_label+1].ch    = 's';
		opts[dsn_label+1].label = (dsn_requested & DSN_SUCCESS)
					   ? "NoSuccess" : "Success";
		opts[dsn_label+2].ch    = 'x';
		opts[dsn_label+2].label = (dsn_requested & DSN_NEVER)
					   ? "ErrRets" : "NoErrRets";
		opts[dsn_label+3].ch    = 'h';
		opts[dsn_label+3].label = (dsn_requested & DSN_FULL)
					   ? "RetHdrs" : "RetFull";
	    }
	}

	if(double_rad +
	   (dsn_requested ? 4 : F_ON(F_DSN, ps_global) ? 1 : 0) > 11)
	  rv = double_radio_buttons(tmp_20k_buf, -FOOTER_ROWS(ps_global), opts,
			   'y', 'z',
			   (F_ON(F_DSN, ps_global) && allow_flowed)
					          ? h_send_prompt_dsn_flowed :
			    F_ON(F_DSN, ps_global) ? h_send_prompt_dsn       :
			     allow_flowed           ? h_send_prompt_flowed   :
						       h_send_prompt,
			   RB_NORM);
	else
	  rv = radio_buttons(tmp_20k_buf, -FOOTER_ROWS(ps_global), opts,
			   'y', 'z',
			   (double_rad +
			    (dsn_requested ? 4 : F_ON(F_DSN, ps_global)
			     ? 1 : 0) == 11)
				   ? NO_HELP : 
			   (F_ON(F_DSN, ps_global) && allow_flowed)
					          ? h_send_prompt_dsn_flowed :
			    F_ON(F_DSN, ps_global) ? h_send_prompt_dsn       :
			     allow_flowed           ? h_send_prompt_flowed   :
						       h_send_prompt,
			   RB_NORM, NULL);

	if(rv == 'y'){				/* user ACCEPTS! */
	    break;
	}
	else if(rv == 'n'){			/* Declined! */
	    rstr = "No Message Sent";
	    break;
	}
	else if(rv == 'z'){			/* Cancelled! */
	    rstr = "Send Cancelled";
	    break;
	}
	else if(rv == 10){			/* PREVIOUS filter */
	    filters = filters->prev;
	}
	else if(rv == 11){			/* NEXT filter */
	    filters = filters->next;
	}
	else if(rv == 21){
	    lmc.text_only = !lmc.text_only;
	}
	else if(rv == 12){			/* flip verbose bit */
	    if((verbose_requested = !verbose_requested)
	       && background_requested)
	      background_requested = 0;
	}
	else if(rv == 22){			/* flip flowing bit */
	    flowing_requested = !flowing_requested;
	}
	else if(rv == 15){
	    if((background_requested = !background_requested)
	       && verbose_requested)
	      verbose_requested = 0;
	}
	else if(dsn_requested & DSN_SHOW){
	    if(rv == 's'){			/* flip success bit */
		dsn_requested ^= DSN_SUCCESS;
		/* turn off related bits */
		if(dsn_requested & DSN_SUCCESS)
		  dsn_requested &= ~(DSN_NEVER);
	    }
	    else if(rv == 'd'){			/* flip delay bit */
		dsn_requested ^= DSN_DELAY;
		/* turn off related bits */
		if(dsn_requested & DSN_DELAY)
		  dsn_requested &= ~(DSN_NEVER);
	    }
	    else if(rv == 'x'){			/* flip never bit */
		dsn_requested ^= DSN_NEVER;
		/* turn off related bits */
		if(dsn_requested & DSN_NEVER)
		  dsn_requested &= ~(DSN_SUCCESS | DSN_DELAY);
	    }
	    else if(rv == 'h'){			/* flip full bit */
		dsn_requested ^= DSN_FULL;
	    }
	}
	else if(rv == 'd'){			/* show dsn options */
	    /*
	     * When you turn on DSN, the default is to notify on
	     * failure, success, or delay; and to return the whole
	     * body on failure.
	     */
	    dsn_requested = (DSN_SHOW | DSN_SUCCESS | DSN_DELAY | DSN_FULL);
	}

	sprintf(dsn_string, "DSN requested[%s%s%s%s]",
		(dsn_requested & DSN_NEVER)
		  ? "Never" : "F",
		(dsn_requested & DSN_DELAY)
		  ? "D" : "",
		(dsn_requested & DSN_SUCCESS)
		  ? "S" : "",
		(dsn_requested & DSN_NEVER)
		  ? ""
		  : (dsn_requested & DSN_FULL) ? "-Full"
					       : "-Hdrs");
    }

    /* remember selection */
    if(filters && filters->index > -1)
      sending_filter_requested = ps_global->VAR_SEND_FILTER[filters->index];

    if(filters){
	filters->prev->next = NULL;			/* tie off list */
	while(filters){				/* then free it */
	    fp = filters->next;
	    if(filters->filter)
	      fs_give((void **)&filters->filter);

	    fs_give((void **)&filters);
	    filters = fp;
	}
    }

    if(old_suspend)
      F_SET(F_CAN_SUSPEND, ps_global, 1);

    ps_global->redrawer = redraw;
    return(rstr);
}



/*----------------------------------------------------------------------
    Call back for pico to get newmail status messages displayed

Args: x -- char processed

Returns: 
----*/      
int
display_message_for_pico(x)
    int x;
{
    int rv;
    
    clear_cursor_pos();			/* can't know where cursor is */
    mark_status_dirty();		/* don't count on cached text */
    fix_windsize(ps_global);
    init_sigwinch();
    display_message(x);
    rv = ps_global->mangled_screen;
    ps_global->mangled_screen = 0;
    return(rv);
}



/*----------------------------------------------------------------------
    Call back for pico to get desired directory for its check point file
     
  Args: s -- buffer to write directory name
	n -- length of that buffer

  Returns: pointer to static buffer
----*/      
char *
checkpoint_dir_for_pico(s, n)
    char *s;
    int   n;
{
#if defined(DOS) || defined(OS2)
    /*
     * we can't assume anything about root or home dirs, so
     * just plunk it down in the same place as the pinerc
     */
    if(!getenv("HOME")){
	char *lc = last_cmpnt(ps_global->pinerc);

	if(lc != NULL){
	    strncpy(s, ps_global->pinerc, min(n-1,lc-ps_global->pinerc));
	    s[min(n-1,lc-ps_global->pinerc)] = '\0';
	}
	else{
	    strncpy(s, ".\\", n-1);
	    s[n-1] = '\0';
	}
    }
    else
#endif
    strcpy(s, ps_global->home_dir);

    return(s);
}


/*----------------------------------------------------------------------
    Call back for pico to display mime type of attachment
     
Args: file -- filename being attached

Returns: returns 1 on success (message queued), zero otherwise (don't know
	  type so nothing queued).
----*/      
int
mime_type_for_pico(file)
    char *file;
{
    BODY *body;
    int   rv;
    void *file_contents;

    body           = mail_newbody();
    body->type     = TYPEOTHER;
    body->encoding = ENCOTHER;

    /* don't know where the cursor's been, reset it */
    clear_cursor_pos();
    if(!set_mime_type_by_extension(body, file)){
	if((file_contents=(void *)so_get(FileStar,file,READ_ACCESS)) != NULL){
	    body->contents.text.data = file_contents;
	    set_mime_type_by_grope(body, NULL);
	}
    }

    if(body->type != TYPEOTHER){
	rv = 1;
	q_status_message3(SM_ORDER, 0, 3,
	    "File %.200s attached as type %.200s/%.200s", file,
	    body_types[body->type],
	    body->subtype ? body->subtype : rfc822_default_subtype(body->type));
    }
    else
      rv = 0;

    pine_free_body(&body);
    return(rv);
}



/*----------------------------------------------------------------------
  Call back for pico to receive an uploaded message

  Args: fname -- name for uploaded file (empty if they want us to assign it)
	size -- pointer to long to hold the attachment's size

  Notes: the attachment is uploaded to a temp file, and 

  Returns: TRUE on success, FALSE otherwise
----*/
int
upload_msg_to_pico(fname, size)
    char *fname;
    long *size;
{
    char     cmd[MAXPATH+1], *fnp = NULL;
    long     l;
    PIPE_S  *syspipe;

    dprint(1, (debugfile, "Upload cmd called to xfer \"%s\"\n",
	       fname ? fname : "<NO FILE>"));

    if(!fname)				/* no place for file name */
      return(0);

    if(!*fname){			/* caller wants temp file */
	if((fnp = temp_nam(NULL, "pu")) != NULL){
	    strncpy(fname, fnp, NLINE-1);
	    fname[NLINE-1] = '\0';
	    (void)unlink(fnp);
	    fs_give((void **)&fnp);
	}
    }

    build_updown_cmd(cmd, ps_global->VAR_UPLOAD_CMD_PREFIX,
		     ps_global->VAR_UPLOAD_CMD, fname);
    if(syspipe = open_system_pipe(cmd, NULL, NULL, PIPE_USER | PIPE_RESET, 0)){
	(void) close_system_pipe(&syspipe, NULL, 0);
	if((l = name_file_size(fname)) < 0L){
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			      "Error determining size of %.200s: %.200s", fname,
			      fnp = error_description(errno));
	    dprint(1, (debugfile,
		   "!!! Upload cmd \"%s\" failed for \"%s\": %s\n",
		   cmd ? cmd : "?",
		   fname ? fname : "?",
		   fnp ? fnp : "?"));
	}
	else if(size)
	  *size = l;

	return(l >= 0);
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 4, "Error opening pipe");

    return(0);
}



/*----------------------------------------------------------------------
  Call back for pico to tell us the window size's changed

  Args: none

  Returns: none (but pine's ttyo structure may have been updated)
----*/
void
resize_for_pico()
{
    fix_windsize(ps_global);
}


char *
cancel_for_pico(redraw_pico)
    void (*redraw_pico)();
{
    int	  rv;
    char *rstr = NULL;
    char *prompt =
     "Cancel message (answering \"Confirm\" will abandon your mail message) ? ";
    void (*redraw)() = ps_global->redrawer;
    static ESCKEY_S opts[] = {
	{'c', 'c', "C", "Confirm"},
	{'n', 'n', "N", "No"},
	{'y', 'y', "", ""},
	{-1, 0, NULL, NULL}
    };

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);
    
    while(1){
	rv = radio_buttons(prompt, -FOOTER_ROWS(ps_global), opts,
			   'n', 'x', h_confirm_cancel, RB_NORM, NULL);
	if(rv == 'c'){				/* user ACCEPTS! */
	    rstr = "";
	    break;
	}
	else if(rv == 'y'){
	    q_status_message(SM_INFO, 1, 3, " Type \"C\" to cancel message ");
	    display_message('x');
	}
	else
	  break;
    }

    ps_global->redrawer = redraw;
    return(rstr);
}



/*----------------------------------------------------------------------
    Set answered flags for messages specified by reply structure
     
Args: reply -- 

Returns: with appropriate flags set and index cache entries suitably tweeked
----*/      
void
update_answered_flags(reply)
    REPLY_S *reply;
{
    char       *seq, *p;
    long	i, ourstream = 0, we_cancel = 0;
    MAILSTREAM *stream = NULL;

    /* nothing to flip in a pseudo reply */
    if(reply && (reply->flags == REPLY_MSGNO || reply->flags == REPLY_UID)){
	int         j;
	MAILSTREAM *m;

	/*
	 * If an established stream will do, use it, else
	 * build one unless we have an array of msgno's...
	 *
	 * I was just mimicking what was already here. I don't really
	 * understand why we use strcmp instead of same_stream_and_mailbox().
	 * Or sp_stream_get(reply->mailbox, SP_MATCH).
	 * Hubert 2003-07-09
	 */
	for(j = 0; !stream && j < ps_global->s_pool.nstream; j++){
	    m = ps_global->s_pool.streams[j];
	    if(m && reply->mailbox && m->mailbox
	       && !strcmp(reply->mailbox, m->mailbox))
	      stream = m;
	}

	if(!stream && reply->flags == REPLY_MSGNO)
	  return;

	/*
	 * This is here only for people who ran pine4.42 and are
	 * processing postponed mail from 4.42 now. Pine4.42 saved the
	 * original mailbox name in the canonical name's position in
	 * the postponed-msgs folder so it won't match the canonical
	 * name from the stream.
	 */
	if(!stream && (!reply->origmbox ||
		       (reply->mailbox &&
		        !strcmp(reply->origmbox, reply->mailbox))))
	  stream = sp_stream_get(reply->mailbox, SP_MATCH);

	we_cancel = busy_alarm(1, "Updating \"Answered\" Flags", NULL, 1);
	if(!stream){
	    if(stream = pine_mail_open(NULL,
				       reply->origmbox ? reply->origmbox
						       : reply->mailbox,
				       OP_SILENT | SP_USEPOOL | SP_TEMPUSE,
				       NULL)){
		ourstream++;
	    }
	    else{
		if(we_cancel)
		  cancel_busy_alarm(0);

		return;
	    }
	}

	if(stream->uid_validity == reply->data.uid.validity){
	    for(i = 0L, p = tmp_20k_buf; reply->data.uid.msgs[i]; i++){
		if(i)
		  sstrcpy(&p, ",");

		sstrcpy(&p, long2string(reply->data.uid.msgs[i]));
	    }

	    mail_flag(stream, seq = cpystr(tmp_20k_buf), "\\ANSWERED",
		      ST_SET | ((reply->flags == REPLY_UID) ? ST_UID : 0L));
	    fs_give((void **)&seq);
	}

	if(ourstream)
	  pine_mail_close(stream);	/* clean up dangling stream */

	if(we_cancel)
	  cancel_busy_alarm(0);
    }
}


/*
 * Take the PicoText pointed to and replace it with PicoText which has been
 * filtered to change the EUC Japanese (unix Pine) or the Shift-JIS (PC-Pine)
 * into ISO-2022-JP.
 *
 * Flowed text preparation was done here, but that was put into pico.
 * If any other filters were to be done before sending a message, this
 * would be the place to do it.
 */
void
post_compose_filters(body)
    BODY *body;
{
    STORE_S **so = (STORE_S **)((body->type == TYPEMULTIPART)
				? &body->nested.part->body.contents.text.data
				: &body->contents.text.data);
    STORE_S  *filtered_so = NULL; 
    gf_io_t   pc, gc;
    char     *errstr;
    int       do_jp_cnv = 0;

    /*
     * Change EUC (unix Pine) or Shift-JIS (PC-Pine) into ISO-2022-JP
     * in the message body.
     * We want to do it here before create_message_body because
     * create_message_body decides the encoding level and would encode
     * the 8-bit characters but not the ISO-2022-JP. Only do this translation
     * if user has a character-set of ISO-2022-JP, which will make it worth
     * less for some. But we can't be assuming 8-bit characters are
     * always ISO-2022-JP.
     */
    if(ps_global->VAR_CHAR_SET
       && !strucmp(ps_global->VAR_CHAR_SET, "iso-2022-jp")
       && F_OFF(F_DISABLE_2022_JP_CONVERSIONS, ps_global))
      do_jp_cnv = 1;
    if(!do_jp_cnv)
      return;
    if(filtered_so = so_get(PicoText, NULL, EDIT_ACCESS)){
	so_seek(*so, 0L, 0);
	gf_filter_init();
	if(do_jp_cnv)
	  gf_link_filter(gf_euc_to_2022_jp, NULL);
	gf_set_so_readc(&gc, *so);
	gf_set_so_writec(&pc, filtered_so);
	if(errstr = gf_pipe(gc, pc)){
	    so_give(&filtered_so);
	    dprint(1, (debugfile,
		       "Error with post_compose_filters: %s\n",
		       errstr ? errstr : "?"));
	    return;
	}

	gf_clear_so_readc(*so);
	gf_clear_so_writec(filtered_so);

	so_give(so);
	*so = filtered_so;
    }
}


/*----------------------------------------------------------------------
    Pass the first text segment of the message thru the "send filter"
     
Args: body pointer and address for storage object of old data

Returns: returns 1 on success, zero on error.
----*/      
int
filter_message_text(fcmd, outgoing, body, old, header)
    char      *fcmd;
    ENVELOPE  *outgoing;
    BODY      *body;
    STORE_S  **old;
    METAENV   *header;
{
    char     *cmd, *tmpf = NULL, *resultf = NULL, *errstr = NULL, *mtf = NULL;
    int	      key = 0, include_hdrs = 0;
    gf_io_t   gc, pc;
    STORE_S **so = (STORE_S **)((body->type == TYPEMULTIPART)
				? &body->nested.part->body.contents.text.data
				: &body->contents.text.data),
	     *tmp_so = NULL, *tmpf_so,
	     *save_local_so, *readthis_so, *our_tmpf_so = NULL;
#define DO_HEADERS 1

    if(fcmd
       && (cmd=expand_filter_tokens(fcmd, outgoing, &tmpf, &resultf, &mtf,
				    &key, &include_hdrs))){
	if(tmpf){
	    if(tmpf_so = so_get(FileStar, tmpf, EDIT_ACCESS|OWNER_ONLY)){
		if(key){
		    so_puts(tmpf_so, filter_session_key());
		    so_puts(tmpf_so, NEWLINE);
		}

		/*
		 * If the headers are wanted for filtering, we can just
		 * stick them in the tmpf file that is already there before
		 * putting the body in.
		 */
		if(include_hdrs){
		    save_local_so = lmc.so;
		    lmc.so = tmpf_so;		/* write it to tmpf_so */
		    lmc.all_written = lmc.text_written = lmc.text_only = 0;
		    pine_rfc822_header(header, body, NULL, NULL);
		    lmc.so = save_local_so;
		}

		so_seek(*so, 0L, 0);
		gf_set_so_readc(&gc, *so);
		gf_set_so_writec(&pc, tmpf_so);
		gf_filter_init();
		errstr = gf_pipe(gc, pc);
		gf_clear_so_readc(*so);
		gf_clear_so_writec(tmpf_so);
		so_give(&tmpf_so);
	    }
	    else
	      errstr = "Can't create space for filter temporary file.";
	}
	else if(include_hdrs){
	    /*
	     * Gf_filter wants a single storage object to read from.
	     * If headers are wanted for filtering we'll have to put them
	     * and the body into a temp file first and then use that
	     * as the storage object for gf_filter.
	     */
	    if(our_tmpf_so = so_get(TmpFileStar, NULL, EDIT_ACCESS|OWNER_ONLY)){
		/* put headers in our_tmpf_so */
		save_local_so = lmc.so;
		lmc.so = our_tmpf_so;		/* write it to our_tmpf_so */
		lmc.all_written = lmc.text_written = lmc.text_only = 0;
		pine_rfc822_header(header, body, NULL, NULL);
		lmc.so = save_local_so;

		/* put body in our_tmpf_so */
		so_seek(*so, 0L, 0);
		gf_set_so_readc(&gc, *so);
		gf_set_so_writec(&pc, our_tmpf_so);
		gf_filter_init();
		errstr = gf_pipe(gc, pc);
		gf_clear_so_readc(*so);
		gf_clear_so_writec(our_tmpf_so);

		/* tell gf_filter to read from our_tmpf_so instead of *so */
		readthis_so = our_tmpf_so;
	    }
	    else
	      errstr = "Can't create space for temporary file.";
	}
	else
	  readthis_so = *so;

	if(!errstr){
	    if(tmp_so = so_get(PicoText, NULL, EDIT_ACCESS)){
		gf_set_so_writec(&pc, tmp_so);
		ps_global->mangled_screen = 1;
		suspend_busy_alarm();
		ClearScreen();
		fflush(stdout);
		if(tmpf){
		    PIPE_S *fpipe;

		    if(fpipe = open_system_pipe(cmd, NULL, NULL,
						PIPE_NOSHELL | PIPE_RESET, 0)){
			if(close_system_pipe(&fpipe, NULL, 0) == 0){
			    if(tmpf_so = so_get(FileStar, tmpf, READ_ACCESS)){
				gf_set_so_readc(&gc, tmpf_so);
				gf_filter_init();
				errstr = gf_pipe(gc, pc);
				gf_clear_so_readc(tmpf_so);
				so_give(&tmpf_so);
			    }
			    else
			      errstr = "Can't open temp file filter wrote.";
			}
			else
			  errstr = "Filter command returned error.";
		    }
		    else
		      errstr = "Can't exec filter text.";
		}
		else
		  errstr = gf_filter(cmd, key ? filter_session_key() : NULL,
				     readthis_so, pc, NULL, 0);

		if(our_tmpf_so)
		  so_give(&our_tmpf_so);

		gf_clear_so_writec(tmp_so);

#ifdef _WINDOWS
		if(!errstr){
		    /*
		     * Can't really be using stdout, so don't print message that
		     * gets printed in the else.  Ideally the program being called
		     * will wait after showing the message, we might want to look
		     * into doing the waiting in console based apps... or not.
		     */
#else
		if(errstr){
		    int ch;

		    fprintf(stdout, "\r\n%s  Hit return to continue.", errstr);
		    fflush(stdout);
		    while((ch = read_char(300)) != ctrl('M')
			  && ch != NO_OP_IDLE)
		      putchar(BELL);
		}
		else{
#endif /* _WINDOWS */
		    BODY *b = (body->type == TYPEMULTIPART)
					   ? &body->nested.part->body : body;

		    *old = *so;			/* save old so */
		    *so = tmp_so;		/* return new one */

		    /*
		     * If the command said it would return new MIME
		     * mime type data, check it out...
		     */
		    if(mtf){
			char  buf[MAILTMPLEN], *s;
			FILE *fp;

			if(fp = fopen(mtf, "r")){
			    if(fgets(buf, sizeof(buf), fp)
			       && !struncmp(buf, "content-", 8)
			       && (s = strchr(buf+8, ':'))){
				BODY *nb = mail_newbody();

				for(*s++ = '\0'; *s == ' '; s++)
				  ;

				rfc822_parse_content_header(nb,
				    (char *) ucase((unsigned char *) buf+8),s);
				if(nb->type == TYPETEXT
				   && nb->subtype
				   && (!b->subtype 
				       || strucmp(b->subtype, nb->subtype))){
				    if(b->subtype)
				      fs_give((void **) &b->subtype);

				    b->subtype  = nb->subtype;
				    nb->subtype = NULL;
				      
				    mail_free_body_parameter(&b->parameter);
				    b->parameter = nb->parameter;
				    nb->parameter = NULL;
				    mail_free_body_parameter(&nb->parameter);
				}

				mail_free_body(&nb);
			    }

			    fclose(fp);
			}
		    }

		    /*
		     * Reevaluate the encoding in case form's changed...
		     */
		    b->encoding = ENCOTHER;
		    set_mime_type_by_grope(b, NULL);
		}

		ClearScreen();
		resume_busy_alarm(0);
	    }
	    else
	      errstr = "Can't create space for filtered text.";
	}

	fs_give((void **)&cmd);
    }
    else
      return(0);

    if(tmpf){
	unlink(tmpf);
	fs_give((void **)&tmpf);
    }

    if(mtf){
	unlink(mtf);
	fs_give((void **) &mtf);
    }

    if(resultf){
	if(name_file_size(resultf) > 0L)
	  display_output_file(resultf, "Filter", NULL, DOF_BRIEF);

	fs_give((void **)&resultf);
    }
    else if(errstr){
	if(tmp_so)
	  so_give(&tmp_so);

	q_status_message1(SM_ORDER | SM_DING, 3, 6, "Problem filtering: %.200s",
			  errstr);
	dprint(1, (debugfile, "Filter FAILED: %s\n",
	       errstr ? errstr : "?"));
    }

    return(errstr == NULL);
}



/*----------------------------------------------------------------------
    Copy the newsgroup name of the given mailbox into the given buffer
     
Args: 

Returns: 
----*/      
void
pine_send_newsgroup_name(mailbox, group_name, len)
    char *mailbox, *group_name;
    size_t len;
{
    NETMBX  mb;

    if(*mailbox == '#'){		/* Strip the leading "#news." */
	strncpy(group_name, mailbox + 6, len-1);
	group_name[len-1] = '\0';
    }
    else if(mail_valid_net_parse(mailbox, &mb)){
	pine_send_newsgroup_name(mb.mailbox, group_name, len);
    }
    else
      *group_name = '\0';
}



/*----------------------------------------------------------------------
     Generate and send a message back to the pine development team
     
Args: none

Returns: none
----*/      
void
phone_home(addr)
    char *addr;
{
    char      tmp[MAX_ADDRESS];
    ENVELOPE *outgoing;
    BODY     *body;

#if defined(DOS) || defined(OS2)
    if(!dos_valid_from())
      return;
#endif

    outgoing = mail_newenvelope();
    if(!addr || !strindex(addr, '@'))
      sprintf(addr = tmp, "pine%s@%s", PHONE_HOME_VERSION, PHONE_HOME_HOST);

    rfc822_parse_adrlist(&outgoing->to, addr, ps_global->maildomain);

    outgoing->message_id  = generate_message_id();
    outgoing->subject	  = cpystr("Document Request");
    outgoing->from	  = phone_home_from();

    body       = mail_newbody();
    body->type = TYPETEXT;

    if(body->contents.text.data = (void *)so_get(PicoText,NULL,EDIT_ACCESS)){
	so_puts((STORE_S *)body->contents.text.data, "Document request: ");
	so_puts((STORE_S *)body->contents.text.data, "Pine-");
	so_puts((STORE_S *)body->contents.text.data, pine_version);
	if(ps_global->first_time_user)
	  so_puts((STORE_S *)body->contents.text.data, " for New Users");

	if(ps_global->VAR_INBOX_PATH && ps_global->VAR_INBOX_PATH[0] == '{')
	  so_puts((STORE_S *)body->contents.text.data, " and IMAP");

	if(ps_global->VAR_NNTP_SERVER && ps_global->VAR_NNTP_SERVER[0]
	      && ps_global->VAR_NNTP_SERVER[0][0])
	  so_puts((STORE_S *)body->contents.text.data, " and NNTP");

	(void)pine_simple_send(outgoing, &body, NULL,NULL,NULL,NULL, SS_NULLRP);

	q_status_message(SM_ORDER, 0, 3, "Thanks for being counted!");
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 4,
		       "Problem creating space for message text.");

    mail_free_envelope(&outgoing);
    pine_free_body(&body);

}


/*
 * phone_home_from - make phone home request's from address IMpersonal.
 *		     Doesn't include user's personal name.
 */
ADDRESS *
phone_home_from()
{
    ADDRESS *addr = mail_newaddr();
    char     tmp[32];

    /* garble up mailbox name */
    sprintf(tmp, "hash_%08u", phone_home_hash(ps_global->VAR_USER_ID));
    addr->mailbox = cpystr(tmp);
    addr->host	  = cpystr(ps_global->maildomain);
    return(addr);
}


/*
 * one-way-hash a username into an 8-digit decimal number 
 *
 * Corey Satten, corey@cac.washington.edu, 7/15/98
 */
unsigned int
phone_home_hash(s)
    char *s;
{
    unsigned int h;
    
    for (h=0; *s; ++s) {
        if (h & 1)
	  h = (h>>1) | (PH_MAXHASH/2);
        else 
	  h = (h>>1);

        h = ((h+1) * ((unsigned char) *s)) & (PH_MAXHASH - 1);
    }
    
    return (h);
}


/*----------------------------------------------------------------------
     Call the mailer, SMTP, sendmail or whatever
     
Args: header -- full header (envelope and local parts) of message to send
      body -- The full body of the message including text
      verbose -- flag to indicate verbose transaction mode requested

Returns: -1 if failed, 1 if succeeded
----*/      
int
call_mailer(header, body, alt_smtp_servers)
    METAENV *header;
    BODY    *body;
    char   **alt_smtp_servers;
{
    char         error_buf[200], *error_mess = NULL, *postcmd;
    ADDRESS     *a;
    ENVELOPE	*fake_env = NULL;
    int          addr_error_count, we_cancel = 0;
    long	 smtp_opts = 0L;
    char	*verbose_file = NULL;
    BODY	*bp = NULL;
    PINEFIELD	*pf;

#define MAX_ADDR_ERROR 2  /* Only display 2 address errors */

    dprint(4, (debugfile, "Sending mail...\n"));

    /* Check for any recipients */
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr)
	break;

    if(!pf){
	q_status_message(SM_ORDER,3,3,
	    "Can't send message. No recipients specified!");
	return(0);
    }

    /* set up counts and such to keep track sent percentage */
    send_bytes_sent = 0;
    gf_filter_init();				/* zero piped byte count, 'n */
    send_bytes_to_send = send_body_size(body);	/* count body bytes	     */
    ps_global->c_client_error[0] = error_buf[0] = '\0';
    we_cancel = busy_alarm(1, "Sending mail",
			   send_bytes_to_send ? sent_percent : NULL, 1);

    /* try posting via local "<mta> <-t>" if specified */
    if(mta_handoff(header, body, error_buf, sizeof(error_buf))){
	if(error_buf[0])
	  error_mess = error_buf;

	goto done;
    }

    /*
     * If the user's asked for it, and we find that the first text
     * part (attachments all get b64'd) is non-7bit, ask for 8BITMIME.
     */
    if(F_ON(F_ENABLE_8BIT, ps_global) && (bp = first_text_8bit(body)))
       smtp_opts |= SOP_8BITMIME;

#ifdef	DEBUG
#ifndef DEBUGJOURNAL
    if(debug > 5 || verbose_requested)
#endif
      smtp_opts |= SOP_DEBUG;
#endif

    if(dsn_requested){
	smtp_opts |= SOP_DSN;
	if(!(dsn_requested & DSN_NEVER)){ /* if never, don't turn others on */
	    if(dsn_requested & DSN_DELAY)
	      smtp_opts |= SOP_DSN_NOTIFY_DELAY;
	    if(dsn_requested & DSN_SUCCESS)
	      smtp_opts |= SOP_DSN_NOTIFY_SUCCESS;

	    /*
	     * If it isn't Never, then we're always going to let them
	     * know about failures.  This means we don't allow for the
	     * possibility of setting delay or success without failure.
	     */
	    smtp_opts |= SOP_DSN_NOTIFY_FAILURE;

	    if(dsn_requested & DSN_FULL)
	      smtp_opts |= SOP_DSN_RETURN_FULL;
	}
    }


    /*
     * Set global header pointer so post_rfc822_output can get at it when
     * it's called back from c-client's sending routine...
     */
    send_header = header;

    /*
     * Fabricate a fake ENVELOPE to hand c-client's SMTP engine.
     * The purpose is to give smtp_mail the list for SMTP RCPT when
     * there are recipients in pine's METAENV that are outside c-client's
     * envelope.
     *  
     * NOTE: If there aren't any, don't bother.  Dealt with it below.
     */
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr
	 && !(*pf->addr == header->env->to || *pf->addr == header->env->cc
	      || *pf->addr == header->env->bcc))
	break;

    if(pf && pf->name){
	ADDRESS **tail;

	fake_env = (ENVELOPE *)fs_get(sizeof(ENVELOPE));
	memset(fake_env, 0, sizeof(ENVELOPE));
	fake_env->return_path = rfc822_cpy_adr(header->env->return_path);
	tail = &(fake_env->to);
	for(pf = header->local; pf && pf->name; pf = pf->next)
	  if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr){
	      *tail = rfc822_cpy_adr(*pf->addr);
	      while(*tail)
		tail = &((*tail)->next);
	  }
    }

    /*
     * Install our rfc822 output routine 
     */
    sending_hooks.rfc822_out = mail_parameters(NULL, GET_RFC822OUTPUT, NULL);
    (void)mail_parameters(NULL, SET_RFC822OUTPUT, (void *)post_rfc822_output);

    /*
     * Allow for verbose posting
     */
    (void) mail_parameters(NULL, SET_SMTPVERBOSE,
			   (void *) pine_smtp_verbose_out);

    /*
     * We do this because we want mm_log to put the error message into
     * c_client_error instead of showing it itself.
     */
    ps_global->noshow_error = 1;

    /*
     * OK, who posts what?  We tried an mta_handoff above, but there
     * was either none specified or we decided not to use it.  So,
     * if there's an smtp-server defined anywhere, 
     */
    if(alt_smtp_servers && alt_smtp_servers[0] && alt_smtp_servers[0][0]){
	/*---------- SMTP ----------*/
	dprint(4, (debugfile, "call_mailer: via TCP (%s)\n",
		alt_smtp_servers[0]));
	TIME_STAMP("smtp-open start (tcp)", 1);
	sending_stream = smtp_open(alt_smtp_servers, smtp_opts);
    }
    else if(ps_global->VAR_SMTP_SERVER && ps_global->VAR_SMTP_SERVER[0]
	    && ps_global->VAR_SMTP_SERVER[0][0]){
	/*---------- SMTP ----------*/
	dprint(4, (debugfile, "call_mailer: via TCP\n"));
	TIME_STAMP("smtp-open start (tcp)", 1);
	sending_stream = smtp_open(ps_global->VAR_SMTP_SERVER, smtp_opts);
    }
    else if(postcmd = smtp_command(ps_global->c_client_error)){
	char *cmdlist[2];

	/*----- Send via LOCAL SMTP agent ------*/
	dprint(4, (debugfile, "call_mailer: via \"%s\"\n", postcmd));

	TIME_STAMP("smtp-open start (pipe)", 1);
	fs_give((void **) &postcmd);
	cmdlist[0] = "localhost";
	cmdlist[1] = NULL;
	sending_stream = smtp_open_full(&piped_io, cmdlist, "smtp",
					SMTPTCPPORT, smtp_opts);
/* BUG: should provide separate stderr output! */
    }

    ps_global->noshow_error = 0;

    TIME_STAMP("smtp open", 1);
    if(sending_stream){
	unsigned short save_encoding;

	dprint(1, (debugfile, "Opened SMTP server \"%s\"\n",
	       net_host(sending_stream->netstream)
	         ? net_host(sending_stream->netstream) : "?"));

	if(verbose_requested){
	    TIME_STAMP("verbose start", 1);
	    if(verbose_file = temp_nam(NULL, "sd")){
		if(verbose_send_output = fopen(verbose_file, "w")){
		    if(!pine_smtp_verbose(sending_stream))
		      sprintf(error_mess = error_buf,
			      "Mail not sent.  VERBOSE mode error%s%.50s.",
			      (sending_stream && sending_stream->reply)
			        ? ": ": "",
			      (sending_stream && sending_stream->reply)
			        ? sending_stream->reply : "");
		}
		else{
		    (void)unlink(verbose_file);
		    strcpy(error_mess = error_buf,
			   "Can't open tmp file for VERBOSE mode.");
		}
	    }
	    else
	      strcpy(error_mess = error_buf,
		     "Can't create tmp file name for VERBOSE mode.");

	    TIME_STAMP("verbose end", 1);
	}

	/*
	 * Before we actually send data, see if we have to protect
	 * the first text body part from getting encoded.  We protect
	 * it from getting encoded in "pine_rfc822_output_body" by
	 * temporarily inventing a synonym for ENC8BIT...
	 */
	if(bp && sending_stream->protocol.esmtp.eightbit.ok
	      && sending_stream->protocol.esmtp.eightbit.want){
	    int i;

	    for(i = 0; (i <= ENCMAX) && body_encodings[i]; i++)
	      ;

	    if(i > ENCMAX){		/* no empty encoding slots! */
		bp = NULL;
	    }
	    else {
		body_encodings[i] = body_encodings[ENC8BIT];
		save_encoding = bp->encoding;
		bp->encoding = (unsigned short) i;
	    }
	}

	if(sending_stream->protocol.esmtp.ok
	   && sending_stream->protocol.esmtp.dsn.want
	   && !sending_stream->protocol.esmtp.dsn.ok)
	  q_status_message(SM_ORDER,3,3,
	    "Delivery Status Notification not available from this server.");

	TIME_STAMP("smtp start", 1);
	if(!error_mess && !smtp_mail(sending_stream, "MAIL",
				     fake_env ? fake_env : header->env, body)){
	    struct headerentry *last_he = NULL;

	    sprintf(error_buf,
		    "Mail not sent. Sending error%s%.40s",
		    (sending_stream && sending_stream->reply) ? ": ": ".",
		    (sending_stream && sending_stream->reply)
		      ? sending_stream->reply : "");
	    dprint(1, (debugfile, error_buf));
	    addr_error_count = 0;
	    if(fake_env){
		for(a = fake_env->to; a != NULL; a = a->next)
		  if(a->error != NULL){
		      if(addr_error_count++ < MAX_ADDR_ERROR){

			  /*
			   * Too complicated to figure out which header line
			   * has the error in the fake_env case, so just
			   * leave cursor at default.
			   */


			  if(error_mess)	/* previous error? */
			    q_status_message(SM_ORDER, 4, 7, error_mess);

			  error_mess = tidy_smtp_mess(a->error,
						      "Mail not sent: %.80s",
						      error_buf);
		      }

		      dprint(1, (debugfile, "Send Error: \"%s\"\n",
				 a->error));
		  }
	    }
	    else{
	      for(pf = header->local; pf && pf->name; pf = pf->next)
	        if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr)
		  for(a = *pf->addr; a != NULL; a = a->next)
		    if(a->error != NULL){
		      if(addr_error_count++ < MAX_ADDR_ERROR){
			  if(pf->he){
			      if(last_he)	/* start last reported err */
				last_he->start_here = 0;

			      (last_he = pf->he)->start_here = 1;
			  }

			  if(error_mess)	/* previous error? */
			    q_status_message(SM_ORDER, 4, 7, error_mess);

			  error_mess = tidy_smtp_mess(a->error,
						      "Mail not sent: %.80s",
						      error_buf);
		      }

		      dprint(1, (debugfile, "Send Error: \"%s\"\n",
				 a->error));
		    }
	    }

	    if(!error_mess)
	      error_mess = error_buf;
	}

	/* repair modified "body_encodings" array? */
	if(bp && sending_stream->protocol.esmtp.eightbit.ok
	      && sending_stream->protocol.esmtp.eightbit.want){
	    body_encodings[bp->encoding] = NULL;
	    bp->encoding = save_encoding;
	}

	TIME_STAMP("smtp closing", 1);
	smtp_close(sending_stream);
	sending_stream = NULL;
	TIME_STAMP("smtp done", 1);
    }
    else if(!error_mess){
	sprintf(error_mess = error_buf, "Error sending%.2s%.80s",
	        ps_global->c_client_error[0] ? ": " : "",
		ps_global->c_client_error);
    }

    if(verbose_file){
	if(verbose_send_output){
	    TIME_STAMP("verbose start", 1);
	    fclose(verbose_send_output);
	    verbose_send_output = NULL;
	    q_status_message(SM_ORDER, 0, 3, "Verbose SMTP output received");
	    display_output_file(verbose_file, "Verbose SMTP Interaction",
				NULL, DOF_BRIEF);
	    TIME_STAMP("verbose end", 1);
	}

	fs_give((void **)&verbose_file);
    }

    /*
     * Restore original 822 emitter...
     */
    (void) mail_parameters(NULL, SET_RFC822OUTPUT, sending_hooks.rfc822_out);

    if(fake_env)
      mail_free_envelope(&fake_env);

  done:
    if(we_cancel)
      cancel_busy_alarm(0);

    TIME_STAMP("call_mailer done", 1);
    /*-------- Did message make it ? ----------*/
    if(error_mess){
        /*---- Error sending mail -----*/
	if(lmc.so && !lmc.all_written)
	  so_give(&lmc.so);

        q_status_message(SM_ORDER | SM_DING, 4, 7, error_mess);
	dprint(1, (debugfile, "call_mailer ERROR: %s\n", error_mess));
	return(-1);
    }
    else{
	lmc.all_written = 1;
	return(1);
    }
}


/*----------------------------------------------------------------------
    Checks to make sure the fcc is available and can be opened

Args: fcc -- the name of the fcc to create.  It can't be NULL.
      fcc_cntxt -- Returns the context the fcc is in.
      force -- supress user option prompt

Returns allocated storage object on success, NULL on failure
  ----*/
STORE_S *
open_fcc(fcc, fcc_cntxt, force, err_prefix, err_suffix)
    char       *fcc;
    CONTEXT_S **fcc_cntxt;
    int 	force;
    char       *err_prefix,
	       *err_suffix;
{
    int		exists, ok = 0;

    *fcc_cntxt		    = NULL;
    ps_global->mm_log_error = 0;

    /* 
     * check for fcc's existance...
     */
    TIME_STAMP("open_fcc start", 1);
    if(!is_absolute_path(fcc) && context_isambig(fcc)
       && (strucmp(ps_global->inbox_name, fcc) != 0)){
	int flip_dot = 0;

	/*
	 * Don't want to preclude a user from Fcc'ing a .name'd folder
	 */
	if(F_OFF(F_ENABLE_DOT_FOLDERS, ps_global)){
	    flip_dot = 1;
	    F_TURN_ON(F_ENABLE_DOT_FOLDERS, ps_global);
	}

	/*
	 * We only want to set the "context" if fcc is an ambiguous
	 * name.  Otherwise, our "relativeness" rules for contexts 
	 * (implemented in context.c) might cause the name to be
	 * interpreted in the wrong context...
	 */
	if(!(*fcc_cntxt = default_save_context(ps_global->context_list)))
	  *fcc_cntxt = ps_global->context_list;

        build_folder_list(NULL, *fcc_cntxt, fcc, NULL, BFL_FLDRONLY);
        if(folder_index(fcc, *fcc_cntxt, FI_FOLDER) < 0){
	    if(ps_global->context_list->next)
	      sprintf(tmp_20k_buf,
		      "Folder \"%.20s\" in <%.30s> doesn't exist. Create",
		      strsquish(tmp_20k_buf + 500, fcc, 20),
		      strsquish(tmp_20k_buf + 1000,(*fcc_cntxt)->nickname,30));
	    else
	      sprintf(tmp_20k_buf,
		      "Folder \"%s\" doesn't exist.  Create",
		      strsquish(tmp_20k_buf + 500, fcc, 40));

	    if(force || want_to(tmp_20k_buf,'y','n',NO_HELP,WT_NORM) == 'y'){

		ps_global->noshow_error = 1;

		if(context_create(*fcc_cntxt, NULL, fcc))
		  ok++;

		ps_global->noshow_error = 0;
	    }
	    else
	      ok--;			/* declined! */
        }
	else
	  ok++;				/* found! */

	if(flip_dot)
	  F_TURN_OFF(F_ENABLE_DOT_FOLDERS, ps_global);

        free_folder_list(*fcc_cntxt);
    }
    else if((exists = folder_exists(NULL, fcc)) != FEX_ERROR){
	if(exists & (FEX_ISFILE | FEX_ISDIR)){
	    ok++;
	}
	else{
	    sprintf(tmp_20k_buf,"Folder \"%s\" doesn't exist.  Create",
		    strsquish(tmp_20k_buf + 500, fcc, 40));
	    if(force || want_to(tmp_20k_buf,'y','n',NO_HELP,WT_NORM) == 'y'){
		ps_global->mm_log_error = 0;
		ps_global->noshow_error = 1;

		ok = pine_mail_create(NULL, fcc) != 0L;

		ps_global->noshow_error = 0;
	    }
	    else
	      ok--;			/* declined! */
	}
    }

    TIME_STAMP("open_fcc done.", 1);
    if(ok > 0){
	return(so_get(FCC_SOURCE, NULL, WRITE_ACCESS));
    }
    else{
	int   l1, l2, l3, wid, w;
	char *errstr, tmp[MAILTMPLEN];
	char *s1, *s2;

	if(ok == 0){
	    if(ps_global->mm_log_error){
		s1 = err_prefix ? err_prefix : "Fcc Error: ";
		s2 = err_suffix ? err_suffix : " Message NOT sent or copied.";

		l1 = strlen(s1);
		l2 = strlen(s2);
		l3 = strlen(ps_global->c_client_error);
		wid = (ps_global->ttyo && ps_global->ttyo->screen_cols > 0)
		       ? ps_global->ttyo->screen_cols : 80;
		w = wid - l1 - l2 - 5;

		sprintf(errstr = tmp,
			"%.99s\"%.*s%.99s\".%.99s",
			s1,
			(l3 > w) ? max(w-3,0) : max(w,0),
			ps_global->c_client_error,
			(l3 > w) ? "..." : "",
			s2);

	    }
	    else
	      errstr = "Fcc creation error. Message NOT sent or copied.";
	}
	else
	  errstr = "Fcc creation rejected. Message NOT sent or copied.";

	q_status_message(SM_ORDER | SM_DING, 3, 3, errstr);
    }

    return(NULL);
}


/*----------------------------------------------------------------------
   mail_append() the fcc accumulated in temp_storage to proper destination

Args:  fcc -- name of folder
       fcc_cntxt -- context for folder
       temp_storage -- String of file where Fcc has been accumulated

This copies the string of file to the actual folder, which might be IMAP
or a disk folder.  The temp_storage is freed after it is written.
An error message is produced if this fails.
  ----*/
int
write_fcc(fcc, fcc_cntxt, tmp_storage, stream, label, flags)
     char	*fcc;
     CONTEXT_S	*fcc_cntxt;
     STORE_S	*tmp_storage;
     MAILSTREAM *stream;
     char	*label;
     char       *flags;
{
    STRING      msg;
    CONTEXT_S  *cntxt;
    int         we_cancel = 0;
#if	defined(DOS) && !defined(WIN32)
    struct {			/* hack! stolen from dawz.c */
	int fd;
	unsigned long pos;
    } d;
    extern STRINGDRIVER dawz_string;
#endif

    if(!tmp_storage)
      return(0);

    TIME_STAMP("write_fcc start.", 1);
    dprint(4, (debugfile, "Writing %s\n", (label && *label) ? label : ""));
    if(label && *label){
	char msg_buf[80];

	strncat(strcpy(msg_buf, "Writing "), label, sizeof(msg_buf)-10);
	we_cancel = busy_alarm(1, msg_buf, NULL, 1);
    }
    else
      we_cancel = busy_alarm(1, NULL, NULL, 0);

    so_seek(tmp_storage, 0L, 0);

/*
 * Before changing this note that these lines depend on the
 * definition of FCC_SOURCE.
 */
#if	defined(DOS) && !defined(WIN32)
    d.fd  = fileno((FILE *)so_text(tmp_storage));
    d.pos = 0L;
    INIT(&msg, dawz_string, (void *)&d, filelength(d.fd));
#else
    INIT(&msg, mail_string, (void *)so_text(tmp_storage), 
	     strlen((char *)so_text(tmp_storage)));
#endif

    cntxt      = fcc_cntxt;

    if(!context_append_full(cntxt, stream, fcc, flags, NULL, &msg)){
	cancel_busy_alarm(-1);
	we_cancel = 0;

	q_status_message1(SM_ORDER | SM_DING, 3, 5,
			  "Write to \"%.200s\" FAILED!!!", fcc);
	dprint(1, (debugfile, "ERROR appending %s in \"%s\"",
	       fcc ? fcc : "?",
	       (cntxt && cntxt->context) ? cntxt->context : "NULL"));
	return(0);
    }

    if(we_cancel)
      cancel_busy_alarm(label ? 0 : -1);

    dprint(4, (debugfile, "done.\n"));
    TIME_STAMP("write_fcc done.", 1);
    return(1);
}



/*
 * first_text_8bit - return TRUE if somewhere in the body 8BIT data's
 *		     contained.
 */
BODY *
first_text_8bit(body)
    BODY *body;
{
    if(body->type == TYPEMULTIPART)	/* advance to first contained part */
      body = &body->nested.part->body;

    return((body->type == TYPETEXT && body->encoding != ENC7BIT)
	     ? body : NULL);
}



/*----------------------------------------------------------------------
    Remove the leading digits from SMTP error messages
 -----*/
char *
tidy_smtp_mess(error, printstring, outbuf)
    char *error, *printstring, *outbuf;
{
    while(isdigit((unsigned char)*error) || isspace((unsigned char)*error) ||
	  (*error == '.' && isdigit((unsigned char)*(error+1))))
      error++;

    sprintf(outbuf, printstring, error);
    return(outbuf);
}

        
    
/*----------------------------------------------------------------------
    Set up fields for passing to pico.  Assumes first text part is
    intended to be passed along for editing, and is in the form of
    of a storage object brought into existence sometime before pico_send().
 -----*/
void
outgoing2strings(header, bod, text, pico_a)
    METAENV   *header;
    BODY      *bod;
    void     **text;
    PATMT    **pico_a;
{
    PINEFIELD  *pf;

    /*
     * SIMPLIFYING ASSUMPTION #37: the first TEXT part's storage object
     * is guaranteed to be of type PicoText!
     */
    if(bod->type == TYPETEXT){
	*text = so_text((STORE_S *) bod->contents.text.data);
    } else if(bod->type == TYPEMULTIPART){
	PART       *part;
	PATMT     **ppa;
	char       *type, *name, *p;
	int		name_l;
	/*
	 * We used to jump out the window if the first part wasn't text,
	 * but that may not be the case when bouncing a message with
	 * a leading non-text segment.  So, IT'S UNDERSTOOD that the 
	 * contents of the first part to send is still ALWAYS in a 
	 * PicoText storage object, *AND* if that object doesn't contain
	 * data of type text, then it must contain THE ENCODED NON-TEXT
	 * DATA of the piece being sent.
	 *
	 * It's up to the programmer to make sure that such a message is
	 * sent via pine_simple_send and never get to the composer via
	 * pine_send.
	 *
	 * Make sense?
	 */
	*text = so_text((STORE_S *) bod->nested.part->body.contents.text.data);

	/*
	 * If we already had a list, blast it now, so we can build a new
	 * attachment list that reflects what's really there...
	 */
	if(pico_a)
	  free_attachment_list(pico_a);


        /* Simplifyihg assumption #28e. (see cross reference) 
           All parts in the body passed in here that are not already
           in the attachments list are added to the end of the attachments
           list. Attachment items not in the body list will be taken care
           of in strings2outgoing, but they are unlikey to occur
         */

        for(part = bod->nested.part->next; part != NULL; part = part->next) {
	    /* Already in list? */
            for(ppa = pico_a;
		*ppa && strcmp((*ppa)->id, part->body.id);
		ppa = &(*ppa)->next)
	      ;

	    if(!*ppa){		/* Not in the list! append it... */
		*ppa = (PATMT *)fs_get(sizeof(PATMT));

		if(part->body.description){
		    char *p, *charset = NULL;
		    size_t len;

		    len = strlen(part->body.description)+1;
		    p = (char *)fs_get(len*sizeof(char));
		    if(rfc1522_decode((unsigned char *)p, len,
				      part->body.description, &charset)
		       == (unsigned char *) p)
		      (*ppa)->description = p;
		    else{
			fs_give((void **)&p);
			(*ppa)->description = cpystr(part->body.description);
		    }
		    if(charset)
		      fs_give((void **)&charset);
		}
		else
		  (*ppa)->description = cpystr("");
            
		type = type_desc(part->body.type, part->body.subtype,
				 part->body.parameter, 0);

		/*
		 * If we can find a "name" parm, display that too...
		 */
		if(name = rfc2231_get_param(part->body.parameter,
					    "name", NULL, NULL)){
		    /* Convert any [ or ]'s the name contained */
		    for(p = name; *p ; p++)
		      if(*p == '[')
			*p = '(';
		      else if(*p == ']')
			*p = ')';

		    name_l = p - name;
		}
		else
		  name_l = 0;

		(*ppa)->filename = fs_get(strlen(type) + name_l + 5);

		sprintf((*ppa)->filename, "[%s%s%s]", type,
			name ? ": " : "", name ? name : "");

		if(name)
		  fs_give((void **) &name);

		(*ppa)->flags = A_FLIT;
		(*ppa)->size  = cpystr(byte_string(
						 send_body_size(&part->body)));
		if(!part->body.id)
		  part->body.id = generate_message_id();

		(*ppa)->id   = cpystr(part->body.id);
		(*ppa)->next = NULL;
	    }
	}
    }
        

    /*------------------------------------------------------------------
       Malloc strings to pass to composer editor because it expects
       such strings so it can realloc them
      -----------------------------------------------------------------*/
    /*
     * turn any address fields into text strings
     */
    /*
     * SIMPLIFYING ASSUMPTION #116: all header strings are understood
     * NOT to be RFC1522 decoded.  Said differently, they're understood
     * to be RFC1522 ENCODED as necessary.  The intent is to preserve
     * original charset tagging as far into the compose/send pipe as
     * we can.
     */
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->canedit)
	switch(pf->type){
	  case Address :
	    if(pf->addr){
		char *p, *t, *u;
		long  l;

		pf->scratch = addr_list_string(*pf->addr, NULL, 0, 1);

		/*
		 * Scan for and fix-up patently bogus fields.
		 *
		 * NOTE: collaboration with this code and what's done in
		 * reply.c:reply_cp_addr to package up the bogus stuff
		 * is required.
		 */
		for(p = pf->scratch; p = strstr(p, "@.RAW-FIELD."); )
		  for(t = p; ; t--)
		    if(*t == '&'){		/* find "leading" token */
			int replacelen;

			/*
			 * Rfc822_cat has been changed so that it now quotes
			 * this sometimes. So we have to look out for quotes
			 * which confuse the decoder. It was only quoting
			 * because we were putting \r \n in the input, I think.
			 */
			if(t > pf->scratch && t[-1] == '\"' && p[-1] == '\"')
			  t[-1] = p[-1] = ' ';

			*t++ = ' ';		/* replace token */
			*p = '\0';		/* tie off string */
			u = rfc822_base64((unsigned char *) t,
					  (unsigned long) strlen(t),
					  (unsigned long *) &l);
			replacelen = strlen(t);
			*p = '@';		/* restore 'p' */
			rplstr(p, 12, "");	/* clear special token */
			rplstr(t, replacelen, u);  /* NULL u handled in func */
			if(u)
			  fs_give((void **) &u);

			if(pf->he)
			  pf->he->start_here = 1;

			break;
		    }
		    else if(t == pf->scratch)
		      break;

		removing_leading_white_space(pf->scratch);
		if(pf->scratch){
		    size_t l;

		    /*
		     * Replace control characters with ^C notation, unless
		     * some conditions are met (see istrncpy).
		     * If user doesn't edit, then we switch back to the
		     * original version. If user does edit, then all bets
		     * are off.
		     */
		    istrncpy((char *)tmp_20k_buf, pf->scratch, SIZEOF_20KBUF-1);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    if((l=strlen((char *)tmp_20k_buf)) > strlen(pf->scratch)){
			fs_give((void **)&pf->scratch);
			pf->scratch = (char *)fs_get((l+1) * sizeof(char));
		    }

		    strncpy(pf->scratch, (char *)tmp_20k_buf, l+1);
		}
	    }

	    break;

	  case Subject :
	    if(pf->text){
		char *p, *src, *charset = NULL;
		size_t len;

		src = pf->scratch ? pf->scratch
				  : (*pf->text) ? *pf->text : "";

		len = strlen(src)+1;
		p = (char *)fs_get(len * sizeof(char));
		if(rfc1522_decode((unsigned char *)p, len, src, &charset)
						   == (unsigned char *) p){
		    if(pf->scratch)
		      fs_give((void **)&pf->scratch);

		    pf->scratch = p;
		}
		else{
		    fs_give((void **)&p);
		    if(!pf->scratch)
		      pf->scratch = cpystr(src);
		}

		if(charset)
		  fs_give((void **)&charset);

		if(pf->scratch){
		    size_t l;

		    /*
		     * Replace control characters with ^C notation, unless
		     * some conditions are met (see istrncpy).
		     * If user doesn't edit, then we switch back to the
		     * original version. If user does edit, then all bets
		     * are off.
		     */
		    istrncpy((char *)tmp_20k_buf, pf->scratch, SIZEOF_20KBUF-1);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    if((l=strlen((char *)tmp_20k_buf)) > strlen(pf->scratch)){
			fs_give((void **)&pf->scratch);
			pf->scratch = (char *)fs_get((l+1) * sizeof(char));
		    }

		    strncpy(pf->scratch, (char *)tmp_20k_buf, l+1);
		}
	    }

	    break;
	}
}


/*----------------------------------------------------------------------
    Restore fields returned from pico to form useful to sending
    routines.
 -----*/
void
strings2outgoing(header, bod, attach, charset, flow_it)
    METAENV  *header;
    BODY    **bod;
    PATMT    *attach;
    char     *charset;
    int       flow_it;
{
    PINEFIELD *pf;
    int we_cancel = 0;

    we_cancel = busy_alarm(1, NULL, NULL, 0);

    /*
     * turn any local address strings into address lists
     */
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->scratch){
	  /*
	   * If we edited this header, then we use the edited version.
	   * Otherwise we usually use the original, which may contain charset
	   * information that we preserved.
	   *
	   * If we are using the original because we haven't edited it,
	   * we need to make sure it is valid. Check for .RAW-FIELD..
	   * We need to toss out that pf->addr and use the one in pf->scratch,
	   * which was decoded before we went into pico.
	   *
	   * Special case: For subject, if there was no original subject but
	   * there was a default from customized-hdrs, need to copy that
	   * as if we edited it.
	   */
	  if((pf->canedit && ((pf->he && pf->he->dirty)
			      || (pf->type == Address
				  && pf->addr
				  && *pf->addr
				  && (*pf->addr)->host
				  && !strcmp((*pf->addr)->host,".RAW-FIELD."))))
	     || (pf->type == Subject && *pf->scratch && !*pf->text)){
		char *the_address = NULL;

	      switch(pf->type){
		case Address :
		  removing_trailing_white_space(pf->scratch);
		  /*
		   * If encoded exists, that means that build_address filled
		   * it in when expanding an addrbook nickname so as to
		   * preserve the charset information.
		   * If it wasn't edited at all, we don't go through here
		   * and we just use the original addr, which still has
		   * the charset info in it.
		   */
		  if(pf->he){
		      PrivateTop *pt;

		      pt = (PrivateTop *)pf->he->bldr_private;
		      if(pt && pt->encoded && pt->encoded->etext)
			the_address = pt->encoded->etext;
		  }

		  if((the_address || *pf->scratch) && pf->addr){
		      ADDRESS     *new_addr = NULL;
		      static char *fakedomain = "@";

		      if(!the_address)
			the_address = pf->scratch;

		      rfc822_parse_adrlist(&new_addr, the_address,
				   (F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global))
				     ? fakedomain : ps_global->maildomain);
		      resolve_encoded_entries(new_addr, *pf->addr);
		      mail_free_address(pf->addr);	/* free old addrs */
		      *pf->addr = new_addr;		/* assign new addr */
		  }
		  else if(pf->addr)
		    mail_free_address(pf->addr);	/* free old addrs */

		  break;

		case Subject :
		  if(*pf->text)
		    fs_give((void **)pf->text);

		  if(*pf->scratch){
		      if(ps_global->VAR_CHAR_SET &&
		         !strucmp(ps_global->VAR_CHAR_SET, "iso-2022-jp"))
		        *pf->text =
			 (char *) trans_euc_to_2022_jp((unsigned char *) (pf->scratch));
		      else
		        *pf->text = cpystr(pf->scratch);
		  }

		  break;
	      }
	  }

	  fs_give((void **)&pf->scratch);	/* free now useless text */
      }

    if(bod && *bod)
      post_compose_filters(*bod);

    create_message_body(bod, attach, charset, flow_it);
    pine_encode_body(*bod);

    if(we_cancel)
      cancel_busy_alarm(-1);
}


/*----------------------------------------------------------------------
 -----*/
void
resolve_encoded_entries(new, old)
    ADDRESS *new, *old;
{
    ADDRESS *a;

    /* BUG: deal properly with group syntax? */
    for(; old; old = old->next)
      if(old->personal && old->mailbox && old->host)
	for(a = new; a; a = a->next)
	  if(a->personal && a->mailbox && !strcmp(old->mailbox, a->mailbox)
	     && a->host && !strcmp(old->host, a->host)){
	      char *charset = NULL, *p, *q = NULL, buftmp[MAILTMPLEN];

	      /*
	       * if we actually found 1522 in the personal name, then
	       * make sure the decoded personal name matches the old 
	       * un-encoded name.  If so, replace the new string with
	       * with the old one.  If 1522 isn't involved, just trust
	       * the change.
	       */
	      sprintf(buftmp, "%.200s", old->personal ? old->personal : "");
	      p = (char *) rfc1522_decode((unsigned char *)tmp_20k_buf,
					  SIZEOF_20KBUF, buftmp, &charset);

	      q = (char *) trans_euc_to_2022_jp((unsigned char *)(a->personal));

	      if(p == tmp_20k_buf		/* personal was decoded */
		 && !strcmp(q, p)){		/* still matches what it was */
		  fs_give((void **)&a->personal);
		  a->personal = cpystr(old->personal);
	      }
	      else if(ps_global->VAR_CHAR_SET &&
		     !strucmp(ps_global->VAR_CHAR_SET, "iso-2022-jp")){
		  /*
		   * Convert EUC (unix Pine) or Shift-JIS (PC-Pine) into
		   * ISO-2022-JP.
		   */
		  if(a->personal)
		    fs_give((void **)&a->personal);

		  a->personal = q;
		  q = NULL;			/* so we don't free it below */
	      }

	      if(q)
		fs_give((void **)&q);
	      if(charset)
		fs_give((void **)&charset);

	      break;
	  }
}


/*----------------------------------------------------------------------

 The head of the body list here is always either TEXT or MULTIPART. It may be
changed from TEXT to MULTIPART if there are attachments to be added
and it is not already multipart. 
  ----*/
void
create_message_body(b, attach, charset, flow_it)
    BODY  **b;
    PATMT  *attach;
    char   *charset;
    int     flow_it;
{
    PART       *p, **pp;
    PATMT      *pa;
    BODY       *tmp_body, *text_body = NULL;
    void       *file_contents;
    PARAMETER **parmp;
    char       *lc;

    TIME_STAMP("create_body start.", 1);
    /*
     * if conditions are met short circuit MIME wrapping
     */
    if((*b)->type != TYPEMULTIPART && !attach){

	/* only override assigned encoding if it might need upgrading */
	if((*b)->type == TYPETEXT && (*b)->encoding == ENC7BIT)
	  (*b)->encoding = ENCOTHER;

	create_message_body_text(*b, charset, flow_it);

	if(F_ON(F_COMPOSE_ALWAYS_DOWNGRADE, ps_global)
	   || !((*b)->encoding == ENC8BIT
		|| (*b)->encoding == ENCBINARY)){
	    TIME_STAMP("create_body end.", 1);
	    return;
	}
	else			/* protect 8bit in multipart */
	  text_body = *b;
    }

    if((*b)->type == TYPETEXT) {
        /*-- Current type is text, but there are attachments to add --*/
        /*-- Upgrade to a TYPEMULTIPART --*/
        tmp_body			= (BODY *)mail_newbody();
        tmp_body->type			= TYPEMULTIPART;
        tmp_body->nested.part		= mail_newbody_part();
	if(text_body){
	    (void) copy_body(&(tmp_body->nested.part->body), *b);
	    /* move contents which were NOT copied */
	    tmp_body->nested.part->body.contents.text.data = (*b)->contents.text.data;
	    (*b)->contents.text.data = NULL;
	}
	else{
	    tmp_body->nested.part->body	= **b;

	    (*b)->subtype = (*b)->id = (*b)->description = NULL;
	    (*b)->parameter				 = NULL;
	    (*b)->contents.text.data			 = NULL;
	}

	pine_free_body(b);
        *b = tmp_body;
    }

    if(!text_body){
	/*-- Now type must be MULTIPART with first part text --*/
	(*b)->nested.part->body.encoding = ENCOTHER;
	create_message_body_text(&((*b)->nested.part->body), charset, flow_it);
    }

    /*------ Go through the parts list remove those to be deleted -----*/
    for(pp = &(*b)->nested.part->next; *pp;) {
	for(pa = attach; pa && (*pp)->body.id; pa = pa->next)
	  /* already existed? */
	  if(pa->id && strcmp(pa->id, (*pp)->body.id) == 0){
	      char *orig_descp, *charset = NULL;

	      /* 
	       * decode original to see if it matches what was decoded
	       * when we sent it in.
	       */
	      if((*pp)->body.description)
		orig_descp = (char *) rfc1522_decode(tmp_20k_buf, SIZEOF_20KBUF,
					    (*pp)->body.description, &charset);
	      if(!(*pp)->body.description	/* update description? */
		 || strcmp(pa->description, orig_descp)){
		  if((*pp)->body.description)
		    fs_give((void **) &(*pp)->body.description);

		  (*pp)->body.description = cpystr(
			   rfc1522_encode(tmp_20k_buf,
					  SIZEOF_20KBUF,
					  (unsigned char *) pa->description,
					  ps_global->VAR_CHAR_SET));
	      }
	      if(charset)
		fs_give((void **)&charset);

	      break;
	  }

        if(pa == NULL){
	    p = *pp;				/* prepare to zap *pp */
	    *pp = p->next;			/* pull next one in list up */
	    p->next = NULL;			/* tie off removed node */

	    pine_free_body_data(&p->body);	/* clean up contained data */
	    mail_free_body_part(&p);		/* free up the part */
	}
	else
	  pp = &(*pp)->next;
    }

    /*---------- Now add any new attachments ---------*/
    for(p = (*b)->nested.part ; p->next != NULL; p = p->next);
    for(pa = attach; pa != NULL; pa = pa->next) {
        if(pa->id != NULL)
	  continue;			/* Has an ID, it's old */

	/*
	 * the idea is handle ALL attachments as open FILE *'s.  Actual
         * encoding and such is handled at the time the message
         * is shoved into the mail slot or written to disk...
	 *
         * Also, we never unlink a file, so it's up to whoever opens
         * it to deal with tmpfile issues.
	 */
	if((file_contents = (void *)so_get(FileStar, pa->filename,
					   READ_ACCESS)) == NULL){
            q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Error \"%.200s\", couldn't attach file \"%.200s\"",
                              error_description(errno), pa->filename);
            display_message('x');
            continue;
        }
        
        p->next			   = mail_newbody_part();
        p			   = p->next;
        p->body.id		   = generate_message_id();
        p->body.contents.text.data = file_contents;

	/*
	 * Set type to unknown and let set_mime_type_by_* figure it out.
	 * Always encode attachments we add as BINARY.
	 */
	p->body.type		     = TYPEOTHER;
	p->body.encoding	     = ENCBINARY;
	p->body.size.bytes           = name_file_size(pa->filename);
	if(set_mime_type_by_extension(&p->body, pa->filename))
	  set_only_charset_by_grope(&p->body, NULL);
	else
	  set_mime_type_by_grope(&p->body, NULL);

	so_release((STORE_S *)p->body.contents.text.data);
        p->body.description = cpystr(rfc1522_encode(tmp_20k_buf,
					   SIZEOF_20KBUF,
				           (unsigned char *) pa->description,
					   ps_global->VAR_CHAR_SET));

	/* Add name attribute for backward compatibility */
	for(parmp = &p->body.parameter; *parmp; )
	  if(!struncmp((*parmp)->attribute, "name", 4)
	     && (!*((*parmp)->attribute + 4)
		 || *((*parmp)->attribute + 4) == '*')){
	      PARAMETER *free_me = *parmp;
	      *parmp = (*parmp)->next;
	      free_me->next = NULL;
	      mail_free_body_parameter(&free_me);
	  }
	  else
	    parmp = &(*parmp)->next;

	*parmp		    = mail_newbody_parameter();
	(*parmp)->attribute = cpystr("name");
	(*parmp)->value	    = cpystr((lc = last_cmpnt(pa->filename))
				       ? lc : pa->filename);

	/* Then set the Content-Disposition ala RFC1806 */
	if(!p->body.disposition.type){
	    p->body.disposition.type = cpystr("attachment");
            for(parmp = &p->body.disposition.parameter; *parmp; )
	      if(!struncmp((*parmp)->attribute, "filename", 4)
		 && (!*((*parmp)->attribute + 4)
		     || *((*parmp)->attribute + 4) == '*')){
		  PARAMETER *free_me = *parmp;
		  *parmp = (*parmp)->next;
		  free_me->next = NULL;
		  mail_free_body_parameter(&free_me);
	      }
	      else
		parmp = &(*parmp)->next;

	    *parmp		= mail_newbody_parameter();
	    (*parmp)->attribute = cpystr("filename");
	    (*parmp)->value	= cpystr((lc = last_cmpnt(pa->filename))
					 ? lc : pa->filename);
	}

        p->next = NULL;
        pa->id = cpystr(p->body.id);
    }

    /*
     * Now, if this multipart has but one text piece (that is, no
     * attachments), then downgrade from a composite type to a discrete
     * text/plain message if CTE is not 8bit.
     */
    if(!(*b)->nested.part->next
       && (*b)->nested.part->body.type == TYPETEXT
       && (F_ON(F_COMPOSE_ALWAYS_DOWNGRADE, ps_global)
	   || !((*b)->nested.part->body.encoding == ENC8BIT
		|| (*b)->nested.part->body.encoding == ENCBINARY))){
	/* Clone the interesting body part */
	tmp_body  = mail_newbody();
	*tmp_body = (*b)->nested.part->body;
	/* and rub out what we don't want cleaned up when it's free'd */
	mail_initbody(&(*b)->nested.part->body);
	mail_free_body(b);
	*b = tmp_body;
    }


    TIME_STAMP("create_body end.", 1);
}

/*
 * Fill in text BODY part's structure
 * 
 */
void
create_message_body_text(b, charset, flow_it)
    BODY *b;
    char *charset;
    int   flow_it;
{
    set_mime_type_by_grope(b,
			   (pbf && pbf->hibit_entered && *pbf->hibit_entered)
			   ? NULL : charset);
    if(F_OFF(F_QUELL_FLOWED_TEXT, ps_global)
       && F_OFF(F_STRIP_WS_BEFORE_SEND, ps_global)
       && flow_it)
      set_mime_flowed_text(b);

    set_body_size(b);
}

/*
 * Build and return the "From:" address for outbound messages from
 * global data...
 */
ADDRESS *
generate_from()
{
    ADDRESS *addr = mail_newaddr();
    if(ps_global->VAR_PERSONAL_NAME){
	addr->personal = cpystr(ps_global->VAR_PERSONAL_NAME);
	removing_leading_and_trailing_white_space(addr->personal);
	if(addr->personal[0] == '\0')
	  fs_give((void **)&addr->personal);
    }

    addr->mailbox = cpystr(ps_global->VAR_USER_ID);
    addr->host    = cpystr(ps_global->maildomain);
    removing_leading_and_trailing_white_space(addr->mailbox);
    removing_leading_and_trailing_white_space(addr->host);
    return(addr);
}


/*
 * free_attachment_list - free attachments in given list
 */
void
free_attachment_list(alist)
    PATMT  **alist;
{
    PATMT  *leading;

    while(alist && *alist){		/* pointer pointing to something */
	leading = (*alist)->next;
	if((*alist)->description)
          fs_give((void **)&(*alist)->description);

	if((*alist)->filename){
	    if((*alist)->flags & A_TMP)
	      if(unlink((*alist)->filename) < 0)
		dprint(1, (debugfile, "-- Can't unlink(%s): %s\n",
		       (*alist)->filename ? (*alist)->filename : "?",
		       error_description(errno)));

	    fs_give((void **)&(*alist)->filename);
	}

	if((*alist)->size)
          fs_give((void **)&(*alist)->size);

	if((*alist)->id)
          fs_give((void **)&(*alist)->id);

	fs_give((void **)alist);

	*alist = leading;
    }
}
    

/*
 * set_mime_type_by_grope - sniff the given storage object to determine its 
 *                  type, subtype, encoding, and charset
 *
 *		"Type" and "encoding" must be set before calling this routine.
 *		If "type" is set to something other than TYPEOTHER on entry,
 *		then that is the "type" we wish to use.  Same for "encoding"
 *		using ENCOTHER instead of TYPEOTHER.  Otherwise, we
 *		figure them out here.  If "type" is already set, we also
 *		leave subtype alone.  If not, we figure out subtype here.
 *		There is a chance that we will upgrade encoding to a "higher"
 *		level.  For example, if it comes in as 7BIT we may change
 *		that to 8BIT if we find a From_ we want to escape.
 *		We may also set the charset attribute if the type is TEXT.
 *
 * NOTE: this is rather inefficient if the store object is a CharStar
 *       but the win is all types are handled the same
 */
void
set_mime_type_by_grope(body, charset)
    BODY *body;
    char *charset;
{
#define RBUFSZ	(8193)
    unsigned char   *buf, *p, *bol;
    register size_t  n;
    long             max_line = 0L,
                     eight_bit_chars = 0L,
                     line_so_far = 0L,
                     len = 0L;
    STORE_S         *so = (STORE_S *)body->contents.text.data;
    unsigned short   new_encoding = ENCOTHER;
    int              we_cancel = 0,
		     can_be_ascii = 1,
		     set_charset_separately = 0;
#ifdef ENCODE_FROMS
    short            froms = 0, dots = 0,
                     bmap  = 0x1, dmap = 0x1;
#endif

    we_cancel = busy_alarm(1, NULL, NULL, 0);

#ifndef DOS
    buf = (unsigned char *)fs_get(RBUFSZ);
#else
    buf = (unsigned char *)tmp_20k_buf;
#endif
    so_seek(so, 0L, 0);

    for(n = 0; n < RBUFSZ-1 && so_readc(&buf[n], so) != 0; n++)
      ;

    buf[n] = '\0';

    if(n){    /* check first few bytes to look for magic numbers */
	if(body->type == TYPEOTHER){
	    if(buf[0] == 'G' && buf[1] == 'I' && buf[2] == 'F'){
		body->type    = TYPEIMAGE;
		body->subtype = cpystr("GIF");
	    }
	    else if((n > 9) && buf[0] == 0xFF && buf[1] == 0xD8
		    && buf[2] == 0xFF && buf[3] == 0xE0
		    && !strncmp((char *)&buf[6], "JFIF", 4)){
	        body->type    = TYPEIMAGE;
	        body->subtype = cpystr("JPEG");
	    }
	    else if((buf[0] == 'M' && buf[1] == 'M')
		    || (buf[0] == 'I' && buf[1] == 'I')){
		body->type    = TYPEIMAGE;
		body->subtype = cpystr("TIFF");
	    }
	    else if((buf[0] == '%' && buf[1] == '!')
		     || (buf[0] == '\004' && buf[1] == '%' && buf[2] == '!')){
		body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("PostScript");
	    }
	    else if(buf[0] == '%' && !strncmp((char *)buf+1, "PDF-", 4)){
		body->type = TYPEAPPLICATION;
		body->subtype = cpystr("PDF");
	    }
	    else if(buf[0] == '.' && !strncmp((char *)buf+1, "snd", 3)){
		body->type    = TYPEAUDIO;
		body->subtype = cpystr("Basic");
	    }
	    else if((n > 3) && buf[0] == 0x00 && buf[1] == 0x05
		            && buf[2] == 0x16 && buf[3] == 0x00){
	        body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("APPLEFILE");
	    }
	    else if((n > 3) && buf[0] == 0x50 && buf[1] == 0x4b
		            && buf[2] == 0x03 && buf[3] == 0x04){
	        body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("ZIP");
	    }

	    /*
	     * if type was set above, but no encoding specified, go
	     * ahead and make it BASE64...
	     */
	    if(body->type != TYPEOTHER && body->encoding == ENCOTHER)
	      body->encoding = ENCBINARY;
	}
    }
    else{
	/* PROBLEM !!! */
	if(body->type == TYPEOTHER){
	    body->type = TYPEAPPLICATION;
	    body->subtype = cpystr("octet-stream");
	    if(body->encoding == ENCOTHER)
		body->encoding = ENCBINARY;
	}
    }

    if (body->encoding == ENCOTHER || body->type == TYPEOTHER){
#if defined(DOS) || defined(OS2) /* for binary file detection */
	int lastchar = '\0';
#define BREAKOUT 300   /* a value that a character can't be */
#endif

	p   = bol = buf;
	len = n;
	while (n--){
/* Some people don't like quoted-printable caused by leading Froms */
#ifdef ENCODE_FROMS
	    Find_Froms(froms, dots, bmap, dmap, *p);
#endif
	    if(*p == '\n'){
		max_line    = max(max_line, line_so_far + p - bol);
		bol	    = NULL;		/* clear beginning of line */
		line_so_far = 0L;		/* clear line count */
#if	defined(DOS) || defined(OS2)
		/* LF with no CR!! */
		if(lastchar != '\r')		/* must be non-text data! */
		  lastchar = BREAKOUT;
#endif
	    }
	    else if(*p == ctrl('O') || *p == ctrl('N') || *p == ESCAPE){
		can_be_ascii--;
	    }
	    else if(*p & 0x80){
		eight_bit_chars++;
	    }
	    else if(!*p){
		/* NULL found. Unless we're told otherwise, must be binary */
		if(body->type == TYPEOTHER){
		    body->type    = TYPEAPPLICATION;
		    body->subtype = cpystr("octet-stream");
		}

		/*
		 * The "TYPETEXT" here handles the case that the NULL
		 * comes from imported text generated by some external
		 * editor that permits or inserts NULLS.  Otherwise,
		 * assume it's a binary segment...
		 */
		new_encoding = (body->type==TYPETEXT) ? ENC8BIT : ENCBINARY;

		/*
		 * Since we've already set encoding, count this as a 
		 * hi bit char and continue.  The reason is that if this
		 * is text, there may be a high percentage of encoded 
		 * characters, so base64 may get set below...
		 */
		if(body->type == TYPETEXT)
		  eight_bit_chars++;
		else
		  break;
	    }

#if defined(DOS) || defined(OS2) /* for binary file detection */
	    if(lastchar != BREAKOUT)
	      lastchar = *p;
#endif

	    /* read another buffer in */
	    if(n == 0){
		if(bol)
		  line_so_far += p - bol;

		for (n = 0; n < RBUFSZ-1 && so_readc(&buf[n], so) != 0; n++)
		  ;

		len += n;
		p = buf;
	    }
	    else
	      p++;

	    /*
	     * If there's no beginning-of-line pointer, then we must
	     * have seen an end-of-line.  Set bol to the start of the
	     * new line...
	     */
	    if(!bol)
	      bol = p;

#if defined(DOS) || defined(OS2) /* for binary file detection */
	    /* either a lone \r or lone \n indicate binary file */
	    if(lastchar == '\r' || lastchar == BREAKOUT){
		if(lastchar == BREAKOUT || n == 0 || *p != '\n'){
		    if(body->type == TYPEOTHER){
			body->type    = TYPEAPPLICATION;
			body->subtype = cpystr("octet-stream");
		    }

		    new_encoding = ENCBINARY;
		    break;
		}
	    }
#endif
	}
    }
    else if(body->type == TYPETEXT)
      set_charset_separately++;

    if(body->encoding == ENCOTHER || body->type == TYPEOTHER){
	/*
	 * Since the type or encoding aren't set yet, fall thru a 
	 * series of tests to make sure an adequate type and 
	 * encoding are set...
	 */

	if(max_line >= 1000L){ 		/* 1000 comes from rfc821 */
	    if(body->type == TYPEOTHER){
		/*
		 * Since the types not set, then we didn't find a NULL.
		 * If there's no NULL, then this is likely text.  However,
		 * since we can't be *completely* sure, we set it to
		 * the generic type.
		 */
		body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("octet-stream");
	    }

	    if(new_encoding != ENCBINARY)
	      /*
	       * As with NULL handling, if we're told it's text, 
	       * qp-encode it, else it gets base 64...
	       */
	      new_encoding = (body->type == TYPETEXT) ? ENC8BIT : ENCBINARY;
	}

	if(eight_bit_chars == 0L){
	    if(body->type == TYPEOTHER)
	      body->type = TYPETEXT;

	    if(new_encoding == ENCOTHER)
	      new_encoding = ENC7BIT;  /* short lines, no 8 bit */
	}
	else if(len <= 3000L || (eight_bit_chars * 100L)/len < 30L){
	    /*
	     * The 30% threshold is based on qp encoded readability
	     * on non-MIME UA's.
	     */
	    can_be_ascii--;
	    if(body->type == TYPEOTHER)
	      body->type = TYPETEXT;

	    if(new_encoding != ENCBINARY)
	      new_encoding = ENC8BIT;  /* short lines, < 30% 8 bit chars */
	}
	else{
	    can_be_ascii--;
	    if(body->type == TYPEOTHER){
		body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("octet-stream");
	    }

	    /*
	     * Apply maximal encoding regardless of previous
	     * setting.  This segment's either not text, or is 
	     * unlikely to be readable with > 30% of the
	     * text encoded anyway, so we might as well save space...
	     */
	    new_encoding = ENCBINARY;   /*  > 30% 8 bit chars */
	}
    }

#ifdef ENCODE_FROMS
    /* If there were From_'s at the beginning of a line or standalone dots */
    if((froms || dots) && new_encoding != ENCBINARY)
      new_encoding = ENC8BIT;
#endif

    /* Set the subtype */
    if(body->subtype == NULL)
      body->subtype = cpystr(rfc822_default_subtype(body->type));

    /*
     * For TYPETEXT we set the charset. The charset may already be set.
     * If it is, we still consider downgrading it to US-ASCII if possible.
     */
    if(body->type == TYPETEXT){
        PARAMETER *pm;

	if(body->parameter == NULL){	/* no parameters, add charset */
	    pm = body->parameter = mail_newbody_parameter();
	    pm->attribute = cpystr("charset");
	}
	else{
	    int su;

	    for(pm = body->parameter;
		(su=strucmp(pm->attribute, "charset")) && pm->next != NULL;
		pm = pm->next)
	      ;/* searching for charset parameter */

	    if(su){			/* add charset parameter */
		pm->next = mail_newbody_parameter();
		pm = pm->next;
		pm->attribute = cpystr("charset");
	    }
	    /* else already a charset parameter there */
	}

	/*
	 * If this is set we haven't groped for eight bit chars. This is
	 * an easy way to do it.
	 */
	if(set_charset_separately)
	  set_only_charset_by_grope(body, charset);
	else
	  set_mime_charset(pm,
			   can_be_ascii > 0,
			   charset ? charset : ps_global->VAR_CHAR_SET);
    }

    if(body->encoding == ENCOTHER)
      body->encoding = new_encoding;

#ifndef	DOS
    fs_give((void **)&buf);
#endif

    if(we_cancel)
      cancel_busy_alarm(-1);
}

void
set_mime_flowed_text(body)
    BODY *body;
{
    PARAMETER *pm;

    if(body->parameter == NULL){
	pm = body->parameter = mail_newbody_parameter();
	pm->attribute = cpystr("format");
	pm->value = cpystr("flowed");
    }
    else{
	int su;

	for(pm = body->parameter;
	    (su = strucmp(pm->attribute, "format")) && pm->next != NULL;
	    pm = pm->next)
	  ;
	if(su){
	    pm->next = mail_newbody_parameter();
	    pm = pm->next;
	    pm->attribute = cpystr("format");
	    pm->value = cpystr("flowed");
	}
    }
}

void
set_only_charset_by_grope(body, charset)
    BODY *body;
    char *charset;
{
    unsigned char c;
    int           can_be_ascii = 1;
    STORE_S      *so = (STORE_S *)body->contents.text.data;
    int           we_cancel = 0;
    PARAMETER    *pm;

    if(body->type != TYPETEXT)
      return;

    we_cancel = busy_alarm(1, NULL, NULL, 0);

    so_seek(so, 0L, 0);

    while(can_be_ascii && so_readc(&c, so))
      if(c == ctrl('O') || c == ctrl('N') || c == ESCAPE || c & 0x80 || !c)
	can_be_ascii--;

    if(body->parameter == NULL){	/* no parameters, add charset */
	pm = body->parameter = mail_newbody_parameter();
	pm->attribute = cpystr("charset");
    }
    else{
	int su;

	for(pm = body->parameter;
	    (su=strucmp(pm->attribute, "charset")) && pm->next != NULL;
	    pm = pm->next)
	  ;/* searching for charset parameter */

	if(su){			/* add charset parameter */
	    pm->next = mail_newbody_parameter();
	    pm = pm->next;
	    pm->attribute = cpystr("charset");
	}
	/* else already a charset parameter there */
    }

    set_mime_charset(pm,
		     can_be_ascii > 0,
		     charset ? charset : ps_global->VAR_CHAR_SET);

    if(we_cancel)
      cancel_busy_alarm(-1);
}


/*
 * set_mime_charset - assign charset
 *
 *                    We may leave the charset that is already set alone,
 *                    or we may set it to cs,
 *                    or we may downgrade it to us-ascii.
 */
void
set_mime_charset(pm, ascii_ok, cs)
    PARAMETER *pm;
    int   ascii_ok;
    char *cs;
{
    char	**excl;
    static char  *us_ascii = "US-ASCII";
    static char  *non_ascii[] = {"UTF-7", "UNICODE-1-1-UTF-7", NULL};

    if(!pm || strucmp(pm->attribute, "charset") != 0)
      panic("set_mime_charset: no charset parameter");
    
    /* If it's set to ascii, just unset it and reset it if necessary */
    if(pm->value && (!*pm->value || strucmp(pm->value, us_ascii) == 0))
      fs_give((void **)&pm->value);

    /* see if cs is a special non_ascii charset */
    for(excl = non_ascii; cs && *excl && strucmp(*excl, cs); excl++)
      ;

    /*
     * *excl means it matched one of the non_ascii charsets that is made up
     * of ascii characters.
     */
    if((ascii_ok && cs && *cs && *excl) ||
       (!ascii_ok && cs && *cs && strucmp(cs, us_ascii))){
	/*
	 * If cs is one of the non_ascii charsets and ascii_ok is set, or
	 * !ascii_ok and cs is set but not equal to ascii, then we use
	 * the passed in charset (unless charset is already set to something).
	 */
	if(!pm->value)
	  pm->value = cpystr(cs);
    }
    else if(ascii_ok){
	/*
	 * Else if ascii is ok and it wasn't one of the special non_ascii
	 * charsets, we go with us_ascii. This could be a downgrade from
	 * a non_ascii charset (like some 8859 charset) to ascii because there
	 * are no non ascii characters present.
	 */
	if(pm->value)				/* downgrade to ascii */
	  fs_give((void **)&pm->value);

	pm->value = cpystr(us_ascii);
    }
    else{
	/*
	 * Else we don't know. There are non ascii characters but we either
	 * don't have a charset to set it to or that charset is just us_ascii,
	 * which is impossible. So we label it unknown. An alternative would
	 * have been to strip the high bits instead and label it ascii.
	 * If it is already set, we leave it.
	 */
	if(!pm->value)
	  pm->value = cpystr(UNKNOWN_CHARSET);
    }
}


/*
 *
 */
void
set_body_size(b)
    BODY *b;
{
    unsigned char c;
    int we_cancel = 0;

    we_cancel = busy_alarm(1, NULL, NULL, 0);
    so_seek((STORE_S *)b->contents.text.data, 0L, 0);
    b->size.bytes = 0L;
    while(so_readc(&c, (STORE_S *)b->contents.text.data))
      b->size.bytes++;

    if(we_cancel)
      cancel_busy_alarm(-1);
}



/*
 * pine_header_line - simple wrapper around c-client call to contain
 *                    repeated code, and to write fcc if required.
 */
int
pine_header_line(field, header, text, f, s, writehdr, localcopy)
    char      *field;
    METAENV   *header;
    char      *text;
    soutr_t    f;
    TCPSTREAM *s;
    int        writehdr, localcopy;
{
    int   ret = 1;
    int   big = 10000;
    char *value, *folded = NULL;


    value = encode_header_value(tmp_20k_buf, SIZEOF_20KBUF,
				(unsigned char *) text,
			        ps_global->VAR_CHAR_SET,
				encode_whole_header(field, header));
    
    if(value && value == text){	/* no encoding was done, have to fold */
	int   fold_by, len;
	char *actual_field;

	len = ((header && header->env && header->env->remail)
		    ? strlen("ReSent-") : 0) +
	      (field ? strlen(field) : 0) + 2;
	      
	actual_field = (char *)fs_get((len+1) * sizeof(char));
	sprintf(actual_field, "%s%s: ",
		(header && header->env && header->env->remail) ? "ReSent-" : "",
		field ? field : "");

	/*
	 * We were folding everything except message-id, but that wasn't
	 * sufficient. Since 822 only allows folding where linear-white-space
	 * is allowed we'd need a smarter folder than "fold" to do it. So,
	 * instead of inventing that smarter folder (which would have to
	 * know 822 syntax) we'll fold only the Subject field, which we know
	 * we can fold without harm. It's really the one we care about anyway.
	 *
	 * We could just alloc space and copy the actual_field followed by
	 * the value into it, but since that's what fold does anyway we'll
	 * waste some cpu time and use fold with a big fold parameter.
	 */
	if(field &&
	   (!strucmp("Subject", field) || !strucmp("References", field)))
	  fold_by = 75;
	else
	  fold_by = big;

	folded = fold(value, fold_by, big, 1, 0, actual_field, " ");

	if(actual_field)
	  fs_give((void **)&actual_field);
    }
    else if(value){			/* encoding was done */
	char *p;

	/*
	 * rfc1522_encode already inserted continuation lines and did
	 * the necessary folding so we don't have to do it. Let
	 * rfc822_header_line add the trailing crlf and the resent- if
	 * necessary. The 20 could actually be a 12.
	 */
	folded = (char *)fs_get((strlen(field)+strlen(value)+20)*sizeof(char));
	*(p = folded) = '\0';
	rfc822_header_line(&p, field, header ? header->env : NULL, value);
    }

    if(value){
	if(writehdr && f)
	  ret = (*f)(s, folded);
	
	if(ret && localcopy && lmc.so && !lmc.all_written)
	  ret = so_puts(lmc.so, folded);

	if(folded)
	  fs_give((void **)&folded);
    }
    
    return(ret);
}


/*
 * Do appropriate encoding of text header lines.
 * For some field types (those that consist of 822 *text) we just encode
 * the whole thing. For structured fields we encode only within comments
 * if possible.
 *
 * Args      d -- Destination buffer if needed. (tmp_20k_buf)
 *           s -- Source string.
 *     charset -- Charset to encode with.
 *  encode_all -- If set, encode the whole string. If not, try to encode
 *                only within comments if possible.
 *
 * Returns   S is returned if no encoding is done. D is returned if encoding
 *           was needed.
 */
char *
encode_header_value(d, len, s, charset, encode_all)
    char          *d;
    size_t         len;		/* length of d */
    unsigned char *s;
    char          *charset;
    int            encode_all;
{
    char *p, *q, *r, *start_of_comment = NULL, *value = NULL;
    int   in_comment = 0;

    if(!s)
      return((char *)s);
    
    if(len < SIZEOF_20KBUF)
      panic("bad call to encode_header_value");

    if(!encode_all){
	/*
	 * We don't have to worry about keeping track of quoted-strings because
	 * none of these fields which aren't addresses contain quoted-strings.
	 * We do keep track of escaped parens inside of comments and comment
	 * nesting.
	 */
	p = d+7000;
	for(q = (char *)s; *q; q++){
	    switch(*q){
	      case LPAREN:
		if(in_comment++ == 0)
		  start_of_comment = q;

		break;

	      case RPAREN:
		if(--in_comment == 0){
		    /* encode the comment, excluding the outer parens */
		    if(p-d < len-1)
		      *p++ = LPAREN;

		    *q = '\0';
		    r = rfc1522_encode(d+14000, len-14000,
				       (unsigned char *)start_of_comment+1,
				       charset);
		    if(r != start_of_comment+1)
		      value = d+7000;  /* some encoding was done */

		    start_of_comment = NULL;
		    if(r)
		      sstrncpy(&p, r, len-1-(p-d));

		    *q = RPAREN;
		    if(p-d < len-1)
		      *p++ = *q;
		}
		else if(in_comment < 0){
		    in_comment = 0;
		    if(p-d < len-1)
		      *p++ = *q;
		}

		break;

	      case BSLASH:
		if(!in_comment && *(q+1)){
		    if(p-d < len-2){
			*p++ = *q++;
			*p++ = *q;
		    }
		}

		break;

	      default:
		if(!in_comment && p-d < len-1)
		  *p++ = *q;

		break;
	    }
	}

	if(value){
	    /* Unterminated comment (wasn't really a comment) */
	    if(start_of_comment)
	      sstrncpy(&p, start_of_comment, len-1-(p-d));
	    
	    *p = '\0';
	}
    }

    /*
     * We have to check if there is anything that needs to be encoded that
     * wasn't in a comment. If there is, we'd better just start over and
     * encode the whole thing. So, if no encoding has been done within
     * comments, or if encoding is needed both within and outside of
     * comments, then we encode the whole thing. Otherwise, go with
     * the version that has only comments encoded.
     */
    if(!value || rfc1522_encode(d, len,
				(unsigned char *)value, charset) != value)
      return(rfc1522_encode(d, len, s, charset));
    else{
	strncpy(d, value, len-1);
	d[len-1] = '\0';
	return(d);
    }
}


/*
 * pine_address_line - write a header field containing addresses,
 *                     one by one (so there's no buffer limit), and
 *                     wrapping where necessary.
 * Note: we use c-client functions to properly build the text string,
 *       but have to screw around with pointers to fool c-client functions
 *       into not blatting all the text into a single buffer.  Yeah, I know.
 */
int
pine_address_line(field, header, alist, f, s, writehdr, localcopy)
    char      *field;
    METAENV   *header;
    ADDRESS   *alist;
    soutr_t    f;
    TCPSTREAM *s;
    int        writehdr, localcopy;
{
    char     tmp[MAX_SINGLE_ADDR], *tmpptr = NULL;
    size_t   alloced = 0;
    char    *p, *delim, *ptmp, buftmp[MAILTMPLEN];
    ADDRESS *atmp;
    int      i, count;
    int      in_group = 0, was_start_of_group = 0, fix_lcc = 0, failed = 0;
    static   char comma[]     = ", ";
    static   char end_group[] = ";";
#define	no_comma	(&comma[1])

    if(!alist)				/* nothing in field! */
      return(1);

    if(!alist->host && alist->mailbox){ /* c-client group convention */
        in_group++;
	was_start_of_group++;
    }

    ptmp	    = alist->personal;	/* remember personal name */
    /* make sure personal name is encoded */
    if(ptmp){
	sprintf(buftmp, "%.200s", ptmp);
	alist->personal = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					 (unsigned char *) buftmp,
					 ps_global->VAR_CHAR_SET);
    }

    atmp            = alist->next;
    alist->next	    = NULL;		/* digest only first address! */

    /* use automatic buffer unless it isn't big enough */
    if((alloced = est_size(alist)) > MAX_SINGLE_ADDR)
      tmpptr = (char *)fs_get(alloced);
    else
      tmpptr = tmp;

    *(p = tmpptr)  = '\0';
    rfc822_address_line(&p, field, header ? header->env : NULL, alist);
    alist->next	    = atmp;		/* restore pointer to next addr */
    alist->personal = ptmp;		/* in case it changed, restore name */

    if((count = strlen(tmpptr)) > 2){	/* back over CRLF */
	count -= 2;
	tmpptr[count] = '\0';
    }

    /*
     * If there is no sending_stream and we are writing the Lcc header,
     * then we are piping it to sendmail -t which expects it to be a bcc,
     * not lcc.
     *
     * When we write it to the fcc or postponed (the lmc.so),
     * we want it to be lcc, not bcc, so we put it back.
     */
    if(!sending_stream && writehdr && struncmp("lcc:", tmpptr, 4) == 0)
      fix_lcc = 1;

    if(writehdr && f && *tmpptr){
	if(fix_lcc)
	  tmpptr[0] = 'b';
	
	failed = !(*f)(s, tmpptr);
	if(fix_lcc)
	  tmpptr[0] = 'L';

	if(failed)
	  goto bail_out;
    }

    if(localcopy && lmc.so &&
       !lmc.all_written && *tmpptr && !so_puts(lmc.so, tmpptr))
      goto bail_out;

    for(alist = atmp; alist; alist = alist->next){
	delim = comma;
	/* account for c-client's representation of group names */
	if(in_group){
	    if(!alist->host){  /* end of group */
		in_group = 0;
		was_start_of_group = 0;
		/*
		 * Rfc822_write_address no longer writes out the end of group
		 * unless the whole group address is passed to it, so we do
		 * it ourselves.
		 */
		delim = end_group;
	    }
	    else if(!localcopy || !lmc.so || lmc.all_written)
	      continue;
	}
	/* start of new group, print phrase below */
        else if(!alist->host && alist->mailbox){
	    in_group++;
	    was_start_of_group++;
	}

	/* no comma before first address in group syntax */
	if(was_start_of_group && alist->host){
	    delim = no_comma;
	    was_start_of_group = 0;
	}

	/* write delimiter */
	if(((!in_group||was_start_of_group) && writehdr && f && !(*f)(s, delim))
	   || (localcopy && lmc.so && !lmc.all_written
	       && !so_puts(lmc.so, delim)))
	  goto bail_out;

	ptmp		= alist->personal; /* remember personal name */
	sprintf(buftmp, "%.200s", ptmp ? ptmp : "");
	alist->personal = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					 (unsigned char *) buftmp,
					 ps_global->VAR_CHAR_SET);
	atmp		= alist->next;
	alist->next	= NULL;		/* tie off linked list */
	if((i = est_size(alist)) > max(MAX_SINGLE_ADDR, alloced)){
	    alloced = i;
	    fs_resize((void **)&tmpptr, alloced);
	}

	*(p = tmpptr)  = '\0';
	/* make sure we don't write out group end with rfc822_write_address */
        if(alist->host || alist->mailbox)
	  rfc822_write_address(tmpptr, alist);

	alist->next	= atmp;		/* restore next pointer */
	alist->personal = ptmp;		/* in case it changed, restore name */

	/*
	 * BUG
	 * With group syntax addresses we no longer have two identical
	 * streams of output.  Instead, for the fcc/postpone copy we include
	 * all of the addresses inside the :; of the group, and for the
	 * mail we're sending we don't include them.  That means we aren't
	 * correctly keeping track of the column to wrap in, below.  That is,
	 * we are keeping track of the fcc copy but we aren't keeping track
	 * of the regular copy.  It could result in too long or too short
	 * lines.  Should almost never come up since group addresses are almost
	 * never followed by other addresses in the same header, and even
	 * when they are, you have to go out of your way to get the headers
	 * messed up.
	 */
	if(count + 2 + (i = strlen(tmpptr)) > 78){ /* wrap long lines... */
	    count = i + 4;
	    if((!in_group && writehdr && f && !(*f)(s, "\015\012    "))
	       || (localcopy && lmc.so && !lmc.all_written &&
				       !so_puts(lmc.so, "\015\012    ")))
	      goto bail_out;
	}
	else
	  count += i + 2;

	if(((!in_group || was_start_of_group)
	    && writehdr && *tmpptr && f && !(*f)(s, tmpptr))
	   || (localcopy && lmc.so && !lmc.all_written
	       && *tmpptr && !so_puts(lmc.so, tmpptr)))
	  goto bail_out;
    }

bail_out:
    if(tmpptr && tmpptr != tmp)
      fs_give((void **)&tmpptr);

    if(failed)
      return(0);

    return((writehdr && f ? (*f)(s, "\015\012") : 1)
	   && ((localcopy && lmc.so
	       && !lmc.all_written) ? so_puts(lmc.so, "\015\012") : 1));
}


/*
 * mutated pine version of c-client's rfc822_header() function. 
 * changed to call pine-wrapped header and address functions
 * so we don't have to limit the header size to a fixed buffer.
 * This function also calls pine's body_header write function
 * because encoding is delayed until output_body() is called.
 */
long
pine_rfc822_header(header, body, f, s)
    METAENV   *header;
    BODY      *body;
    soutr_t    f;
    TCPSTREAM *s;
{
    PINEFIELD  *pf;
    int         j;

    if(header->env->remail){			/* if remailing */
	long i = strlen (header->env->remail);
	if(i > 4 && header->env->remail[i-4] == '\015')
	  header->env->remail[i-2] = '\0'; /* flush extra blank line */

	if((f && !(*f)(s, header->env->remail)) 
	  || (lmc.so && !lmc.all_written
	      && !so_puts(lmc.so, header->env->remail)))
	  return(0L);				/* start with remail header */
    }

    j = 0;
    for(pf = header->sending_order[j]; pf; pf = header->sending_order[++j]){
	switch(pf->type){
	  /*
	   * Warning:  This is confusing.  The 2nd to last argument used to
	   * be just pf->writehdr.  We want Bcc lines to be written out
	   * if we are handing off to a sendmail temp file but not if we
	   * are talking smtp, so bcc's writehdr is set to 0 and
	   * pine_address_line was sending if writehdr OR !sending_stream.
	   * That works as long as we want to write everything when
	   * !sending_stream (an mta handoff to sendmail).  But then we
	   * added the undisclosed recipients line which should only get
	   * written if writehdr is set, and not when we pass to a
	   * sendmail temp file.  So pine_address_line has been changed
	   * so it bases its decision solely on the writehdr passed to it,
	   * and the logic that worries about Bcc and sending_stream
	   * was moved up to the caller (here) to decide when to set it.
	   *
	   * So we have:
	   *   undisclosed recipients:;  This will just be written
	   *                             if writehdr was set and not
	   *                             otherwise, nothing magical.
	   *** We may want to change this, because sendmail -t doesn't handle
	   *** the empty group syntax well unless it has been configured to
	   *** do so.  It isn't configured by default, or in any of the
	   *** sendmail v8 configs.  So we may want to not write this line
	   *** if we're doing an mta_handoff (!sending_stream).
	   *
	   *   !sending_stream (which means a handoff to a sendmail -t)
	   *           bcc or lcc both set the arg so they'll get written
	   *             (There is also Lcc hocus pocus in pine_address_line
	   *              which converts the Lcc: to Bcc: for sendmail
	   *              processing.)
	   *   sending_stream (which means an smtp handoff)
	   *           bcc and lcc will never have writehdr set, so
	   *             will never be written (They both do have rcptto set,
	   *             so they both do cause RCPT TO commands.)
	   *
	   *   The localcopy is independent of sending_stream and is just
	   *   written if it is set for all of these.
	   */
	  case Address:
	    if(!pine_address_line(pf->name,
				  header,
				  pf->addr ? *pf->addr : NULL,
				  f,
				  s,
				  (!strucmp("bcc",pf->name ? pf->name : "")
				    || !strucmp("Lcc",pf->name ? pf->name : ""))
					   ? !sending_stream
					   : pf->writehdr,
				  pf->localcopy))
	      return(0L);

	    break;

	  case Fcc:
	  case FreeText:
	  case Subject:
	    if(!pine_header_line(pf->name, header,
				 pf->text ? *pf->text : NULL,
				 f, s, pf->writehdr, pf->localcopy))
	      return(0L);

	    break;

	  default:
	    q_status_message1(SM_ORDER,3,7,"Unknown header type: %.200s",
			      pf->name);
	    break;
	}
    }


#if	(defined(DOS) || defined(OS2)) && !defined(NOAUTH)
    /*
     * Add comforting "X-" header line indicating what sort of 
     * authenticity the receiver can expect...
     */
    if(F_OFF(F_DISABLE_SENDER, ps_global)){
	 NETMBX	     netmbox;
	 char	     sstring[MAILTMPLEN], *label;	/* place to write  */
	 MAILSTREAM *m;
	 int         i, anonymous = 1;

	 for(i = 0; anonymous && i < ps_global->s_pool.nstream; i++){
	     m = ps_global->s_pool.streams[i];
	     if(m && sp_flagged(m, SP_LOCKED)
		&& mail_valid_net_parse(m->mailbox, &netmbox)
		&& !netmbox.anoflag)
	       anonymous = 0;
	 }

	 if(!anonymous){
	     char  last_char = netmbox.host[strlen(netmbox.host) - 1],
		  *user = (*netmbox.user)
			    ? netmbox.user
			    : cached_user_name(netmbox.mailbox);
	     sprintf(sstring, "%.300s@%s%.300s%s", user ? user : "NULL", 
		     isdigit((unsigned char)last_char) ? "[" : "",
		     netmbox.host,
		     isdigit((unsigned char) last_char) ? "]" : "");
	     label = "X-X-Sender";		/* Jeez. */
	     if(F_ON(F_USE_SENDER_NOT_X,ps_global))
	       label += 4;
	 }
	 else{
	     strcpy(sstring,"UNAuthenticated Sender");
	     label = "X-Warning";
	 }

	 if(!pine_header_line(label, header, sstring, f, s, 1, 1))
	   return(0L);
     }
#endif

    if(body && !header->env->remail){	/* not if remail or no body */
	if((f && !(*f)(s, MIME_VER))
	   || (lmc.so && !lmc.all_written && !so_puts(lmc.so, MIME_VER))
	   || !pine_write_body_header(body, f, s))
	  return(0L);
    }
    else{					/* write terminating newline */
	if((f && !(*f)(s, "\015\012"))
	   || (lmc.so && !lmc.all_written && !so_puts(lmc.so, "\015\012")))
	  return(0L);
    }

    return(1L);
}


/*
 * since encoding happens on the way out the door, this is basically
 * just needed to handle TYPEMULTIPART
 */
void
pine_encode_body (body)
    BODY *body;
{
  PART *part;

  dprint(4, (debugfile, "-- pine_encode_body: %d\n", body ? body->type : 0));
  if (body) switch (body->type) {
  case TYPEMULTIPART:		/* multi-part */
    if (!body->parameter) {	/* cookie not set up yet? */
      char tmp[MAILTMPLEN];	/* make cookie not in BASE64 or QUOTEPRINT*/
      sprintf (tmp,"%ld-%ld-%ld=:%ld",gethostid (),random (),(long) time (0),
	       (long) getpid ());
      body->parameter = mail_newbody_parameter ();
      body->parameter->attribute = cpystr ("BOUNDARY");
      body->parameter->value = cpystr (tmp);
    }
    part = body->nested.part;	/* encode body parts */
    do pine_encode_body (&part->body);
    while (part = part->next);	/* until done */
    break;
/* case MESSAGE:	*/	/* here for documentation */
    /* Encapsulated messages are always treated as text objects at this point.
       This means that you must replace body->contents.msg with
       body->contents.text, which probably involves copying
       body->contents.msg.text to body->contents.text */
  default:			/* all else has some encoding */
    /*
     * but we'll delay encoding it until the message is on the way
     * into the mail slot...
     */
    break;
  }
}


/*
 * post_rfc822_output - cloak for pine's 822 output routine.  Since
 *			we can't pass opaque envelope thru c-client posting
 *			logic, we need to wrap the real output inside
 *			something that c-client knows how to call.
 */
long
post_rfc822_output (tmp, env, body, f, s, ok8bit)
    char      *tmp;
    ENVELOPE  *env;
    BODY      *body;
    soutr_t    f;
    TCPSTREAM *s;
    long       ok8bit;
{
    return(pine_rfc822_output(send_header, body, f, s));
}


/*
 * pine_rfc822_output - pine's version of c-client call.  Necessary here
 *			since we're not using its structures as intended!
 */
long
pine_rfc822_output(header, body, f, s)
    METAENV   *header;
    BODY      *body;
    soutr_t    f;
    TCPSTREAM *s;
{
    int we_cancel = 0;
    long retval;

    dprint(4, (debugfile, "-- pine_rfc822_output\n"));

    we_cancel = busy_alarm(1, NULL, NULL, 0);
    pine_encode_body(body);		/* encode body as necessary */
    /* build and output RFC822 header, output body */
    retval = pine_rfc822_header(header, body, f, s)
	   && (body ? pine_rfc822_output_body(body, f, s) : 1L);

    if(we_cancel)
      cancel_busy_alarm(-1);

    return(retval);
}


/*
 * Local globals pine's body output routine needs
 */
static soutr_t    l_f;
static TCPSTREAM *l_stream;
static unsigned   c_in_buf = 0;

/*
 * def to make our pipe write's more friendly
 */
#ifdef	PIPE_MAX
#if	PIPE_MAX > 20000
#undef	PIPE_MAX
#endif
#endif

#ifndef	PIPE_MAX
#define	PIPE_MAX	1024
#endif


/*
 * l_flust_net - empties gf_io terminal function's buffer
 */
int
l_flush_net(force)
    int force;
{
    if(c_in_buf){
	char *p = &tmp_20k_buf[0], *lp = NULL, c = '\0';

	tmp_20k_buf[c_in_buf] = '\0';
	if(!force){
	    /*
	     * The start of each write is expected to be the start of a
	     * "record" (i.e., a CRLF terminated line).  Make sure that is true
	     * else we might screw up SMTP dot quoting...
	     */
	    for(p = tmp_20k_buf, lp = NULL;
		p = strstr(p, "\015\012");
		lp = (p += 2))
	      ;
	      

	    if(!lp && c_in_buf > 2)			/* no CRLF! */
	      for(p = &tmp_20k_buf[c_in_buf] - 2;
		  p > &tmp_20k_buf[0] && *p == '.';
		  p--)					/* find last non-dot */
		;

	    if(lp && *lp){				/* snippet remains */
		c = *lp;
		*lp = '\0';
	    }
	}

	if((l_f && !(*l_f)(l_stream, tmp_20k_buf))
	   || (lmc.so && !lmc.all_written
	       && !(lmc.text_only && lmc.text_written)
	       && !so_puts(lmc.so, tmp_20k_buf)))
	  return(0);

	c_in_buf = 0;
	if(lp && (*lp = c))				/* Shift text left? */
	  while(tmp_20k_buf[c_in_buf] = *lp)
	    c_in_buf++, lp++;
    }

    return(1);
}


/*
 * l_putc - gf_io terminal function that calls smtp's soutr_t function.
 *
 */
int
l_putc(c)
    int c;
{
    tmp_20k_buf[c_in_buf++] = (char) c;
    return((c_in_buf >= PIPE_MAX) ? l_flush_net(FALSE) : TRUE);
}



/*
 * pine_rfc822_output_body - pine's version of c-client call.  Again, 
 *                necessary since c-client doesn't know about how
 *                we're treating attachments
 */
long
pine_rfc822_output_body(body, f, s)
    BODY *body;
    soutr_t f;
    TCPSTREAM *s;
{
    PART *part;
    PARAMETER *param;
    char *t, *cookie = NIL, *encode_error;
    char tmp[MAILTMPLEN];
    int                add_trailing_crlf;
    gf_io_t            gc;

    dprint(4, (debugfile, "-- pine_rfc822_output_body: %d\n",
	       body ? body->type : 0));
    if(body->type == TYPEMULTIPART) {   /* multipart gets special handling */
	part = body->nested.part;	/* first body part */
					/* find cookie */
	for (param = body->parameter; param && !cookie; param = param->next)
	  if (!strucmp (param->attribute,"BOUNDARY")) cookie = param->value;
	if (!cookie) cookie = "-";	/* yucky default */

	/*
	 * Output a bit of text before the first multipart delimiter
	 * to warn unsuspecting users of non-mime-aware ua's that
	 * they should expect weirdness...
	 */
	if(f && !(*f)(s, "  This message is in MIME format.  The first part should be readable text,\015\012  while the remaining parts are likely unreadable without MIME-aware tools.\015\012\015\012"))
	  return(0);

	do {				/* for each part */
					/* build cookie */
	    sprintf (tmp, "--%s\015\012", cookie);
					/* append cookie,mini-hdr,contents */
	    if((f && !(*f)(s, tmp))
	       || (lmc.so && !lmc.all_written && !so_puts(lmc.so, tmp))
	       || !pine_write_body_header(&part->body,f,s)
	       || !pine_rfc822_output_body (&part->body,f,s))
	      return(0);
	} while (part = part->next);	/* until done */
					/* output trailing cookie */
	sprintf (t = tmp,"--%s--",cookie);
	if(lmc.so && !lmc.all_written){
	    so_puts(lmc.so, t);
	    so_puts(lmc.so, "\015\012");
	}

	return(f ? ((*f) (s,t) && (*f) (s,"\015\012")) : 1);
    }

    l_f      = f;			/* set up for writing chars...  */
    l_stream = s;			/* out other end of pipe...     */
    gf_filter_init();
    dprint(4, (debugfile, "-- pine_rfc822_output_body: segment %ld bytes\n",
	       body->size.bytes));

    if(body->contents.text.data)
      gf_set_so_readc(&gc, (STORE_S *) body->contents.text.data);
    else
      return(1);

    /*
     * Don't add trailing line if it is PicoText, which already guarantees
     * a trailing newline.
     */
    add_trailing_crlf = !(body->contents.text.data
		 && ((STORE_S *) body->contents.text.data)->src == PicoText);

    so_seek((STORE_S *) body->contents.text.data, 0L, 0);

    if(body->type != TYPEMESSAGE){ 	/* NOT encapsulated message */
	/*
	 * Convert text pieces to canonical form
	 * BEFORE applying any encoding (rfc1341: appendix G)...
	 */
	if(body->type == TYPETEXT && body->encoding != ENCBASE64){
	    gf_link_filter(gf_local_nvtnl, NULL);
	}

	switch (body->encoding) {	/* all else needs filtering */
	  case ENC8BIT:			/* encode 8BIT into QUOTED-PRINTABLE */
	    gf_link_filter(gf_8bit_qp, NULL);
	    break;

	  case ENCBINARY:		/* encode binary into BASE64 */
	    gf_link_filter(gf_binary_b64, NULL);
	    break;

	  default:			/* otherwise text */
	    break;
	}
    }

    if(encode_error = gf_pipe(gc, l_putc)){ /* shove body part down pipe */
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  "Encoding Error \"%.200s\"", encode_error);
	display_message('x');
    }

    gf_clear_so_readc((STORE_S *) body->contents.text.data);

    if(encode_error || !l_flush_net(TRUE))
      return(0);

    send_bytes_sent += gf_bytes_piped();
    so_release((STORE_S *)body->contents.text.data);

    if(lmc.so && !lmc.all_written && lmc.text_only){
	if(lmc.text_written){	/* we have some splainin' to do */
	    char tmp[MAILTMPLEN];
	    char *name = NULL;

	    if(!(so_puts(lmc.so,"The following attachment was sent,\015\012")
		 && so_puts(lmc.so,"but NOT saved in the Fcc copy:\015\012")))
	      return(0);

	    name = rfc2231_get_param(body->parameter, "name", NULL, NULL);
	    sprintf(tmp,
    "    A %.20s/%.100s%.10s%.100s%.10s segment of about %.20s bytes.\015\012",
		    body_type_names(body->type), 
		    body->subtype ? body->subtype : "Unknown",
		    name ? " (Name=\"" : "",
		    name ? name : "",
		    name ? "\")" : "",
		    comatose(body->size.bytes));
	    if(name)
	      fs_give((void **)&name);

	    if(!so_puts(lmc.so, tmp))
	      return(0);
	}
	else			/* suppress everything after first text part */
	  lmc.text_written = (body->type == TYPETEXT
			      && (!body->subtype
				  || !strucmp(body->subtype, "plain")));
    }

    if(add_trailing_crlf)
      return((f ? (*f)(s, "\015\012") : 1)	/* output final stuff */
	   && ((lmc.so && !lmc.all_written) ? so_puts(lmc.so,"\015\012") : 1));
    else
      return(1);
}


/*
 * pine_write_body_header - another c-client clone.  This time
 *                          so the final encoding labels get set 
 *                          correctly since it hasn't happened yet,
 *			    and to be paranoid about line lengths.
 *
 * Returns: TRUE/nonzero on success, zero on error
 */
int
pine_write_body_header(body, f, s)
    BODY  *body;
    soutr_t    f;
    TCPSTREAM *s;
{
    char tmp[MAILTMPLEN];
    int  i;
    unsigned char c;
    STRINGLIST *stl;
    PARAMETER *param = body->parameter;
    STORE_S   *so;
    extern const char *tspecials;

    if(so = so_get(CharStar, NULL, WRITE_ACCESS)){
	if(!(so_puts(so, "Content-Type: ")
	     && so_puts(so, body_types[body->type])
	     && so_puts(so, "/")
	     && so_puts(so, body->subtype
			      ? body->subtype
			      : rfc822_default_subtype (body->type))))
	  return(pwbh_finish(0, so));
	    
	if (param){
	    do
	      if(!(so_puts(so, "; ")
		   && rfc2231_output(so, param->attribute, param->value,
				     (char *) tspecials,
				     ps_global->VAR_CHAR_SET)))
		return(pwbh_finish(0, so));
	    while (param = param->next);
	}
	else if(!so_puts(so, "; CHARSET=US-ASCII"))
	  return(pwbh_finish(0, so));
	    
	if(!so_puts(so, "\015\012"))
	  return(pwbh_finish(0, so));

	if ((body->encoding		/* note: encoding 7BIT never output! */
	     && !(so_puts(so, "Content-Transfer-Encoding: ")
		  && so_puts(so, body_encodings[(body->encoding==ENCBINARY)
						? ENCBASE64
						: (body->encoding == ENC8BIT)
						  ? ENCQUOTEDPRINTABLE
						  : (body->encoding <= ENCMAX)
						    ? body->encoding
						    : ENCOTHER])
		  && so_puts(so, "\015\012")))
	    /*
	     * If requested, strip Content-ID headers that don't look like they
	     * are needed. Microsoft's Outlook XP has a bug that causes it to
	     * not show that there is an attachment when there is a Content-ID
	     * header present on that attachment.
	     *
	     * If user has quell-content-id turned on, don't output content-id
	     * unless it is of type message/external-body.
	     * Since this code doesn't look inside messages being forwarded
	     * type message content-ids will remain as is and type multipart
	     * alternative will remain as is. We don't create those on our
	     * own. If we did, we'd have to worry about getting this right.
	     */
	    || (body->id && (F_OFF(F_QUELL_CONTENT_ID, ps_global)
	                     || (body->type == TYPEMESSAGE
			         && body->subtype
				 && !strucmp(body->subtype, "external-body")))
		&& !(so_puts(so, "Content-ID: ") && so_puts(so, body->id)
		     && so_puts(so, "\015\012")))
	    || (body->description
		&& strlen(body->description) < 5000	/* arbitrary! */
		&& !(so_puts(so, "Content-Description: ")
		     && so_puts(so, rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					   (unsigned char *) body->description,
					   ps_global->VAR_CHAR_SET))
		     && so_puts(so, "\015\012")))
	    || (body->md5
		&& !(so_puts(so, "Content-MD5: ")
		     && so_puts(so, body->md5)
		     && so_puts(so, "\015\012"))))
	  return(pwbh_finish(0, so));

	if (stl = body->language) {
	    if(!so_puts(so, "Content-Language: "))
	      return(pwbh_finish(0, so));

	    do {
		if(strlen((char *)stl->text.data) > 500) /* arbitrary! */
		  return(pwbh_finish(0, so));

		tmp[0] = '\0';
		rfc822_cat (tmp,(char *)stl->text.data, tspecials);
		if(!so_puts(so, tmp)
		   || ((stl = stl->next) && !so_puts(so, ", ")))
		  return(pwbh_finish(0, so));
	    }
	    while (stl);

	    if(!so_puts(so, "\015\012"))
	      return(pwbh_finish(0, so));
	}

	if (body->disposition.type) {
	    if(!(so_puts(so, "Content-Disposition: ")
		 && so_puts(so, body->disposition.type)))
	      return(pwbh_finish(0, so));

	    if (param = body->disposition.parameter)
	      do
		if(!(so_puts(so, "; ")
		     && rfc2231_output(so, param->attribute,
				       param->value, (char *) tspecials,
				       ps_global->VAR_CHAR_SET)))
		  return(pwbh_finish(0, so));
	      while (param = param->next);

	    if(!so_puts(so, "\015\012"))
	      return(pwbh_finish(0, so));
	}

	/* copy out of so, a line at a time (or less than a K)
	 * and send it down the pike
	 */
	so_seek(so, 0L, 0);
	i = 0;
	while(so_readc(&c, so))
	  if((tmp[i++] = c) == '\012' || i > sizeof(tmp) - 3){
	      tmp[i] = '\0';
	      if((f && !(*f)(s, tmp))
		 || !lmc_body_header_line(tmp, i <= sizeof(tmp) - 3))
		return(pwbh_finish(0, so));

	      i = 0;
	  }

	/* Finally, write blank line */
	if((f && !(*f)(s, "\015\012")) || !lmc_body_header_finish())
	  return(pwbh_finish(0, so));
	
	return(pwbh_finish(i == 0, so)); /* better of ended on LF */
    }

    return(0);
}


int
lmc_body_header_line(line, beginning)
    char *line;
    int   beginning;
{
    if(lmc.so && !lmc.all_written){
	if(beginning && lmc.text_only && lmc.text_written
	   && (!struncmp(line, "content-type:", 13)
	       || !struncmp(line, "content-transfer-encoding:", 26)
	       || !struncmp(line, "content-disposition:", 20))){
	    /*
	     * "comment out" the real values since our comment isn't
	     * likely the same type, disposition nor encoding...
	     */
	    if(!so_puts(lmc.so, "X-"))
	      return(FALSE);
	}

	return(so_puts(lmc.so, line));
    }

    return(TRUE);
}


int
lmc_body_header_finish()
{
    if(lmc.so && !lmc.all_written){
	if(lmc.text_only && lmc.text_written
	   && !so_puts(lmc.so, "Content-Type: TEXT/PLAIN\015\012"))
	  return(FALSE);

	return(so_puts(lmc.so, "\015\012"));
    }

    return(TRUE);
}



int
pwbh_finish(rv, so)
    int	     rv;
    STORE_S *so;
{
    if(so)
      so_give(&so);

    return(rv);
}


/*
 * pine_free_body - c-client call wrapper so the body data pointer we
 *		    we're using in a way c-client doesn't know about
 *		    gets free'd appropriately.
 */
void
pine_free_body(body)
    BODY **body;
{
    /*
     * Preempt c-client's contents.text.data clean up since we've
     * usurped it's meaning for our own purposes...
     */
    pine_free_body_data (*body);

    /* Then let c-client handle the rest... */
    mail_free_body(body);
}


/* 
 * pine_free_body_data - free pine's interpretations of the body part's
 *			 data pointer.
 */
void
pine_free_body_data(body)
    BODY *body;
{
    if(body){
	if(body->type == TYPEMULTIPART){
	    PART *part = body->nested.part;
	    do				/* for each part */
	      pine_free_body_data(&part->body);
	    while (part = part->next);	/* until done */
	}
	else if(body->contents.text.data)
	  so_give((STORE_S **) &body->contents.text.data);
    }
}


/*
 *
 */
int
sent_percent()
{
    int i = (int) (((send_bytes_sent + gf_bytes_piped()) * 100)
							/ send_bytes_to_send);
    return(min(i, 100));
}



/*
 *
 */
long
send_body_size(body)
    BODY *body;
{
    long  l = 0L;
    PART *part;

    if(body->type == TYPEMULTIPART) {   /* multipart gets special handling */
	part = body->nested.part;	/* first body part */
	do				/* for each part */
	  l += send_body_size(&part->body);
	while (part = part->next);	/* until done */
	return(l);
    }

    return(l + body->size.bytes);
}



/*
 * pine_smtp_verbose - pine's contribution to the smtp stream.  Return
 *		       TRUE for any positive reply code to our "VERBose"
 *		       request.
 *
 *	NOTE: at worst, this command may cause the SMTP connection to get
 *	      nuked.  Modern sendmail's recognize it, and possibly other
 *	      SMTP implementations (the "ON" arg is for PMDF).  What's
 *	      more, if it works, what's returned (sendmail uses reply code
 *	      050, but we'll accept anything less than 100) may not get 
 *	      recognized, and if it does the accompanying text will likely
 *	      vary from server to server.
 */
long
pine_smtp_verbose(stream)
    SENDSTREAM *stream;
{
    /* any 2xx reply to this is acceptable */
    return(smtp_send(stream,"VERB","ON")/100L == 2L);
}



/*
 * pine_smtp_verbose_out - write 
 */
void
pine_smtp_verbose_out(s)
    char *s;
{
    if(verbose_send_output && s){
	char *p, last = '\0';

	for(p = s; *p; p++)
	  if(*p == '\015')
	    *p = ' ';
	  else
	    last = *p;

	fputs(s, verbose_send_output);
	if(last != '\012')
	  fputc('\n', verbose_send_output);
    }

}



/*----------------------------------------------------------------------
      Post news via NNTP or inews

Args: env -- envelope of message to post
       body -- body of message to post

Returns: -1 if failed or cancelled, 1 if succeeded

WARNING: This call function has the side effect of writing the message
    to the lmc.so object.   
  ----*/
news_poster(header, body, alt_nntp_servers)
     METAENV *header;
     BODY    *body;
     char   **alt_nntp_servers;
{
    char *error_mess, error_buf[200], **news_servers;
    char **servers_to_use;
    int	  we_cancel = 0, server_no = 0, done_posting = 0;
    void *orig_822_output;
    BODY *bp = NULL;

    error_buf[0] = '\0';
    we_cancel = busy_alarm(1, "Posting news", NULL, 1);

    dprint(4, (debugfile, "Posting: [%s]\n",
	   (header && header->env && header->env->newsgroups)
	     ? header->env->newsgroups : "?"));

    if((alt_nntp_servers && alt_nntp_servers[0] && alt_nntp_servers[0][0])
       || (ps_global->VAR_NNTP_SERVER && ps_global->VAR_NNTP_SERVER[0]
       && ps_global->VAR_NNTP_SERVER[0][0])){
       /*---------- NNTP server defined ----------*/
	error_mess = NULL;
	servers_to_use = (alt_nntp_servers && alt_nntp_servers[0]
			  && alt_nntp_servers[0][0]) 
	  ? alt_nntp_servers : ps_global->VAR_NNTP_SERVER;

	/*
	 * Install our rfc822 output routine 
	 */
	orig_822_output = mail_parameters(NULL, GET_RFC822OUTPUT, NULL);
	(void) mail_parameters(NULL, SET_RFC822OUTPUT,
			       (void *)post_rfc822_output);

	server_no = 0;
	news_servers = (char **)fs_get(2 * sizeof(char *));
	news_servers[1] = NULL;
	while(!done_posting && servers_to_use[server_no] && 
	      servers_to_use[server_no][0]){
	    news_servers[0] = servers_to_use[server_no];
	    ps_global->noshow_error = 1;
#ifdef DEBUG
	    sending_stream = nntp_open(news_servers, debug ? NOP_DEBUG : 0L);
#else
	    sending_stream = nntp_open(news_servers, 0L);
#endif
	    ps_global->noshow_error = 0;
	    
	    if(sending_stream != NULL) {
	        unsigned short save_encoding;

		/*
		 * Fake that we've got clearance from the transport agent
		 * for 8bit transport for the benefit of our output routines...
		 */
		if(F_ON(F_ENABLE_8BIT_NNTP, ps_global)
		   && (bp = first_text_8bit(body))){
		    int i;

		    for(i = 0; (i <= ENCMAX) && body_encodings[i]; i++)
		        ;

		    if(i > ENCMAX){		/* no empty encoding slots! */
		        bp = NULL;
		    }
		    else {
		        body_encodings[i] = body_encodings[ENC8BIT];
			save_encoding = bp->encoding;
			bp->encoding = (unsigned short) i;
		    }
		}

		/*
		 * Set global header pointer so we can get at it later...
		 */
		send_header = header;
		ps_global->noshow_error = 1;
		if(nntp_mail(sending_stream, header->env, body) == 0)
		    sprintf(error_mess = error_buf, 
			    "Error posting message: %.60s",
			    sending_stream->reply);
		else{
		    done_posting = 1;
		    error_buf[0] = '\0';
		    error_mess = NULL;
		}
		smtp_close(sending_stream);
		ps_global->noshow_error = 0;
		sending_stream = NULL;
		if(F_ON(F_ENABLE_8BIT_NNTP, ps_global) && bp){
		    body_encodings[bp->encoding] = NULL;
		    bp->encoding = save_encoding;
		}
		
	    } else {
	        /*---- Open of NNTP connection failed ------ */
	        sprintf(error_mess = error_buf,
			"Error connecting to news server: %.60s",
			ps_global->c_client_error);
		dprint(1, (debugfile, error_buf));
	    }
	    server_no++;
	}
	fs_give((void **)&news_servers);
	(void) mail_parameters (NULL, SET_RFC822OUTPUT, orig_822_output);
    } else {
        /*----- Post via local mechanism -------*/
        error_mess = post_handoff(header, body, error_buf, sizeof(error_buf));
    }

    if(we_cancel)
      cancel_busy_alarm(0);

    if(error_mess){
	if(lmc.so && !lmc.all_written)
	  so_give(&lmc.so);	/* clean up any fcc data */

        q_status_message(SM_ORDER | SM_DING, 4, 5, error_mess);
        return(-1);
    }

    lmc.all_written = 1;
    return(1);
}



/*
 * view_as_rich - set the rich_header flag
 *
 *         name  - name of the header field
 *         deflt - default value to return if user didn't set it
 *
 *         Note: if the user tries to turn them all off with "", then
 *		 we take that to mean default, since otherwise there is no
 *		 way to get to the headers.
 */
int
view_as_rich(name, deflt)
    char *name;
    int deflt;
{
    char **p;
    char *q;

    p = ps_global->VAR_COMP_HDRS;

    if(p && *p && **p){
        for(; (q = *p) != NULL; p++){
	    if(!struncmp(q, name, strlen(name)))
	      return 0; /* 0 means we *do* view it by default */
	}

        return 1; /* 1 means it starts out hidden */
    }
    return(deflt);
}


/*
 * pine_header_forbidden - is this name a "forbidden" header?
 *
 *          name - the header name to check
 *  We don't allow user to change these.
 */
int
pine_header_forbidden(name)
    char *name;
{
    char **p;
    static char *forbidden_headers[] = {
	"sender",
	"x-sender",
	"x-x-sender",
	"date",
	"received",
	"message-id",
	"in-reply-to",
	"path",
	"resent-message-id",
	"resent-date",
	"resent-from",
	"resent-sender",
	"resent-to",
	"resent-cc",
	"resent-reply-to",
	"mime-version",
	"content-type",
	NULL
    };

    for(p = forbidden_headers; *p; p++)
      if(!strucmp(name, *p))
	break;

    return((*p) ? 1 : 0);
}


/*
 * hdr_is_in_list - is there a custom value for this header?
 *
 *          hdr - the header name to check
 *       custom - the list to check in
 *  Returns 1 if there is a custom value, 0 otherwise.
 */
int
hdr_is_in_list(hdr, custom)
    char      *hdr;
    PINEFIELD *custom;
{
    PINEFIELD *pf;

    for(pf = custom; pf && pf->name; pf = pf->next)
      if(strucmp(pf->name, hdr) == 0)
	return 1;
    
    return 0;
}


/*
 * count_custom_hdrs_pf - returns number of custom headers in arg
 *            custom -- the list to be counted
 *  only_nonstandard -- only count headers which aren't standard pine headers
 */
int
count_custom_hdrs_pf(custom, only_nonstandard)
    PINEFIELD *custom;
    int        only_nonstandard;
{
    int ret = 0;

    for(; custom && custom->name; custom = custom->next)
      if(!only_nonstandard || !custom->standard)
        ret++;

    return(ret);
}


/*
 * count_custom_hdrs_list - returns number of custom headers in arg
 */
int
count_custom_hdrs_list(list)
    char **list;
{
    char **p;
    char  *q     = NULL;
    char  *name;
    char  *t;
    char   save;
    int    ret   = 0;

    if(list){
	for(p = list; (q = *p) != NULL; p++){
	    if(q[0]){
		/* remove leading whitespace */
		name = skip_white_space(q);
		
		/* look for colon or space or end */
		for(t = name;
		    *t && !isspace((unsigned char)*t) && *t != ':'; t++)
		  ;/* do nothing */

		save = *t;
		*t = '\0';
		if(!pine_header_forbidden(name))
		  ret++;

		*t = save;
	    }
	}
    }

    return(ret);
}


/*
 * set_default_hdrval - put the user's default value for this header
 *                      into pf->textbuf.
 *             setthis - the pinefield to be set
 *              custom - where to look for the default
 */
CustomType
set_default_hdrval(setthis, custom)
    PINEFIELD *setthis, *custom;
{
    PINEFIELD *pf;
    CustomType ret = NoMatch;

    if(!setthis || !setthis->name){
	q_status_message(SM_ORDER,3,7,"Internal error setting default header");
	return(ret);
    }

    setthis->textbuf = NULL;

    for(pf = custom; pf && pf->name; pf = pf->next){
	if(strucmp(pf->name, setthis->name) != 0)
	  continue;

	ret = pf->cstmtype;

	/* turn on editing */
	if(strucmp(pf->name, "From") == 0 || strucmp(pf->name, "Reply-To") == 0)
	  setthis->canedit = 1;

	if(pf->val)
	  setthis->textbuf = cpystr(pf->val);
    }

    if(!setthis->textbuf)
      setthis->textbuf = cpystr("");
    
    return(ret);
}


/*
 * pine_header_standard - is this name a "standard" header?
 *
 *          name - the header name to check
 */
FieldType
pine_header_standard(name)
    char *name;
{
    int    i;

    /* check to see if this is a standard header */
    for(i = 0; pf_template[i].name; i++)
      if(!strucmp(name, pf_template[i].name))
	return(pf_template[i].type);

    return(TypeUnknown);
}


/*
 * customized_hdr_setup - setup the PINEFIELDS for all the customized headers
 *                    Allocates space for each name and addr ptr.
 *                    Allocates space for default in textbuf, even if empty.
 *
 *              head - the first PINEFIELD to fill in
 *              list - the list to parse
 */
void
customized_hdr_setup(head, list, cstmtype)
    PINEFIELD *head;
    char     **list;
    CustomType cstmtype;
{
    char **p, *q, *t, *name, *value, save;
    PINEFIELD *pf;

    pf = head;

    if(list){
        for(p = list; (q = *p) != NULL; p++){

	    if(q[0]){

		/* anything after leading whitespace? */
		if(!*(name = skip_white_space(q)))
		  continue;

		/* look for colon or space or end */
	        for(t = name;
		    *t && !isspace((unsigned char)*t) && *t != ':'; t++)
	    	  ;/* do nothing */

		/* if there is a space in the field-name, skip it */
		if(isspace((unsigned char)*t)){
		    q_status_message1(SM_ORDER, 3, 3,
				    "Space not allowed in header name (%.200s)",
				      name);
		    continue;
		}

		save = *t;
		*t = '\0';

		/* Don't allow any of the forbidden headers. */
		if(pine_header_forbidden(name)){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      "Not allowed to change header \"%.200s\"",
				      name);

		    *t = save;
		    continue;
		}

		if(pf){
		    if(pine_header_standard(name) != TypeUnknown)
		      pf->standard = 1;

		    pf->name     = cpystr(name);
		    pf->type     = FreeText;
		    pf->cstmtype = cstmtype;
		    pf->next     = pf+1;

#ifdef OLDWAY
		    /*
		     * Some mailers apparently break if we change
		     * user@domain into Fred <user@domain> for
		     * return-receipt-to,
		     * so we'll just call this a FreeText field, too.
		     */
		    /*
		     * For now, all custom headers are FreeText except for
		     * this one that we happen to know about.  We might
		     * have to add some syntax to the config option so that
		     * people can tell us their custom header takes addresses.
		     */
		    if(!strucmp(pf->name, "Return-Receipt-to")){
			pf->type = Address;
			pf->addr = (ADDRESS **)fs_get(sizeof(ADDRESS *));
			*pf->addr = (ADDRESS *)NULL;
		    }
#endif /* OLDWAY */

		    *t = save;

		    /* remove space between name and colon */
		    value = skip_white_space(t);

		    /* give them an alloc'd default, even if empty */
		    pf->textbuf = cpystr((*value == ':')
					   ? skip_white_space(++value) : "");
		    if(pf->textbuf && pf->textbuf[0]){
			removing_double_quotes(pf->textbuf);
			pf->val = cpystr(pf->textbuf);
		    }

		    pf++;
		}
		else
		  *t = save;
	    }
	}
    }

    /* fix last next pointer */
    if(head && pf != head)
      (pf-1)->next = NULL;
}


/*
 * add_defaults_from_list - the PINEFIELDS list given by "head" is already
 *                          setup except that it doesn't have default values.
 *                          Add those defaults if they exist in "list".
 *
 *              head - the first PINEFIELD to add a default to
 *              list - the list to get the defaults from
 */
void
add_defaults_from_list(head, list)
    PINEFIELD *head;
    char     **list;
{
    char **p, *q, *t, *name, *value, save;
    PINEFIELD *pf;

    for(pf = head; pf && list; pf = pf->next){
	if(!pf->name)
	  continue;

        for(p = list; (q = *p) != NULL; p++){

	    if(q[0]){

		/* anything after leading whitespace? */
		if(!*(name = skip_white_space(q)))
		  continue;

		/* look for colon or space or end */
	        for(t = name;
		    *t && !isspace((unsigned char)*t) && *t != ':'; t++)
	    	  ;/* do nothing */

		/* if there is a space in the field-name, skip it */
		if(isspace((unsigned char)*t))
		  continue;

		save = *t;
		*t = '\0';

		if(strucmp(name, pf->name) != 0){
		    *t = save;
		    continue;
		}

		*t = save;

		/*
		 * Found the right header. See if it has a default
		 * value set.
		 */

		/* remove space between name and colon */
		value = skip_white_space(t);

		if(*value == ':'){
		    char *defval;

		    defval = skip_white_space(++value);
		    if(defval && *defval){
			if(pf->val)
			  fs_give((void **)&pf->val);
			
			removing_double_quotes(defval);
			pf->val = cpystr(defval);
		    }
		}

		break;		/* on to next pf */
	    }
	}
    }
}


/*
 * parse_custom_hdrs - allocate PINEFIELDS for custom headers
 *                     fill in the defaults
 * Args -     list -- The list to parse.
 *        cstmtype -- Fill the in cstmtype field with this value
 */
PINEFIELD *
parse_custom_hdrs(list, cstmtype)
    char **list;
    CustomType cstmtype;
{
    PINEFIELD          *pfields;
    int			i;

    /*
     * add one for possible use by fcc
     *   What is this "possible use"? I don't see it. I don't
     *   think it exists anymore. I think +1 would be ok.   hubert 2000-08-02
     */
    i       = (count_custom_hdrs_list(list) + 2) * sizeof(PINEFIELD);
    pfields = (PINEFIELD *)fs_get((size_t) i);
    memset(pfields, 0, (size_t) i);

    /* set up the custom header pfields */
    customized_hdr_setup(pfields, list, cstmtype);

    return(pfields);
}


/*
 * Combine the two lists of headers into one list which is allocated here
 * and freed by the caller. Eliminate duplicates with values from the role
 * taking precedence over values from the default.
 */
PINEFIELD *
combine_custom_headers(dflthdrs, rolehdrs)
    PINEFIELD *dflthdrs, *rolehdrs;
{
    PINEFIELD *pfields, *pf, *pf2;
    int        max_hdrs, i;

    max_hdrs = count_custom_hdrs_pf(rolehdrs,0) +
               count_custom_hdrs_pf(dflthdrs,0);

    pfields = (PINEFIELD *)fs_get((size_t)(max_hdrs+1)*sizeof(PINEFIELD));
    memset(pfields, 0, (size_t)(max_hdrs+1)*sizeof(PINEFIELD));

    pf = pfields;
    for(pf2 = rolehdrs; pf2 && pf2->name; pf2 = pf2->next){
	pf->name     = pf2->name ? cpystr(pf2->name) : NULL;
	pf->type     = pf2->type;
	pf->cstmtype = pf2->cstmtype;
	pf->textbuf  = pf2->textbuf ? cpystr(pf2->textbuf) : NULL;
	pf->val      = pf2->val ? cpystr(pf2->val) : NULL;
	pf->standard = pf2->standard;
	pf->next     = pf+1;
	pf++;
    }

    /* if these aren't already there, add them */
    for(pf2 = dflthdrs; pf2 && pf2->name; pf2 = pf2->next){
	/* check for already there */
	for(i = 0;
	    pfields[i].name && strucmp(pfields[i].name, pf2->name);
	    i++)
	  ;
	
	if(!pfields[i].name){			/* this is a new one */
	    pf->name     = pf2->name ? cpystr(pf2->name) : NULL;
	    pf->type     = pf2->type;
	    pf->cstmtype = pf2->cstmtype;
	    pf->textbuf  = pf2->textbuf ? cpystr(pf2->textbuf) : NULL;
	    pf->val      = pf2->val ? cpystr(pf2->val) : NULL;
	    pf->standard = pf2->standard;
	    pf->next     = pf+1;
	    pf++;
	}
    }

    /* fix last next pointer */
    if(pf != pfields)
      (pf-1)->next = NULL;

    return(pfields);
}


/*
 * free_customs - free misc. resources associated with custom header fields
 *
 *           pf - pointer to first custom field
 */
void
free_customs(head)
    PINEFIELD *head;
{
    PINEFIELD *pf;

    for(pf = head; pf && pf->name; pf = pf->next){

	fs_give((void **)&pf->name);

	if(pf->val)
	  fs_give((void **)&pf->val);

	/* only true for FreeText */
	if(pf->textbuf)
	  fs_give((void **)&pf->textbuf);

	/* only true for Address */
	if(pf->addr){
	    if(*pf->addr)
	      mail_free_address(pf->addr);

	    fs_give((void **)&pf->addr);
	}

	if(pf->he && pf->he->prompt)
	  fs_give((void **)&pf->he->prompt);
    }

    fs_give((void **)&head);
}


/*
 * encode_whole_header -- returns 1 if field is a custom header
 */
int
encode_whole_header(field, header)
    char      *field;
    METAENV   *header;
{
    int        retval = 0;
    PINEFIELD *pf;

    if(field && (!strucmp(field, "Subject") ||
		 !strucmp(field, "Comment") ||
		 !struncmp(field, "X-", 2)))
      retval++;
    else if(field && *field && header && header->custom){
	for(pf = header->custom; pf && pf->name; pf = pf->next){

	    if(!pf->standard && !strucmp(pf->name, field)){
		retval++;
		break;
	    }
	}
    }

    return(retval);
}


/*
 * background_posting - return whether or not we're already in the process
 *			of posting
 */
int
background_posting(gripe)
    int gripe;
{
    if(ps_global->post){
	if(gripe)
	  q_status_message(SM_ORDER|SM_DING, 3, 3,
			   "Can't post while posting!");
	return(1);
    }

    return(0);
}



/**************** "PIPE" READING POSTING I/O ROUTINES ****************/


/*
 * helpful def's
 */
#define	S(X)		((PIPE_S *)(X))
#define	GETBUFLEN	(4 * MAILTMPLEN)


void *
piped_smtp_open (host, service, port)
    char *host, *service;
    unsigned long port;
{
    char *postcmd;
    void *rv = NULL;

    if(strucmp(host, "localhost")){
	char tmp[MAILTMPLEN];

	sprintf(tmp, "Unexpected hostname for piped SMTP: %.*s", 
		sizeof(tmp)-50, host);
	mm_log(tmp, ERROR);
    }
    else if(postcmd = smtp_command(ps_global->c_client_error)){
	rv = open_system_pipe(postcmd, NULL, NULL,
	  PIPE_READ|PIPE_STDERR|PIPE_WRITE|PIPE_PROT|PIPE_NOSHELL|PIPE_DESC, 0);
	fs_give((void **) &postcmd);
    }
    else
      mm_log(ps_global->c_client_error, ERROR);

    return(rv);
}


void *
piped_aopen (mb, service, user)
     NETMBX *mb;
     char *service;
     char *user;
{
    return(NULL);
}


/* 
 * piped_soutr - Replacement for tcp_soutr that writes one of our
 *		     pipes rather than a tcp stream
 */
long
piped_soutr (stream,s)
     void *stream;
     char *s;
{
    return(piped_sout(stream, s, strlen(s)));
}


/* 
 * piped_sout - Replacement for tcp_soutr that writes one of our
 *		     pipes rather than a tcp stream
 */
long
piped_sout (stream,s,size)
     void *stream;
     char *s;
     unsigned long size;
{
    int i, o;

    if(S(stream)->out.d < 0)
      return(0L);

    if(i = (int) size){
	while((o = write(S(stream)->out.d, s, i)) != i)
	  if(o < 0){
	      if(errno != EINTR){
		  piped_abort(stream);
		  return(0L);
	      }
	  }
	  else{
	      s += o;			/* try again, fix up counts */
	      i -= o;
	  }
    }

    return(1L);
}


/* 
 * piped_getline - Replacement for tcp_getline that reads one
 *		       of our pipes rather than a tcp pipe
 *
 *                     C-client expects that the \r\n will be stripped off.
 */
char *
piped_getline (stream)
    void *stream;
{
    static int   cnt;
    static char *ptr;
    int		 n, m;
    char	*ret, *s, *sp, c = '\0', d;

    if(S(stream)->in.d < 0)
      return(NULL);

    if(!S(stream)->tmp){		/* initialize! */
	/* alloc space to collect input (freed in close_system_pipe) */
	S(stream)->tmp = (char *) fs_get(GETBUFLEN);
	memset(S(stream)->tmp, 0, GETBUFLEN);
	cnt = -1;
    }

    while(cnt < 0){
	while((cnt = read(S(stream)->in.d, S(stream)->tmp, GETBUFLEN)) < 0)
	  if(errno != EINTR){
	      piped_abort(stream);
	      return(NULL);
	  }

	if(cnt == 0){
	    piped_abort(stream);
	    return(NULL);
	}

	ptr = S(stream)->tmp;
    }

    s = ptr;
    n = 0;
    while(cnt--){
	d = *ptr++;
	if((c == '\015') && (d == '\012')){
	    ret = (char *)fs_get (n--);
	    memcpy(ret, s, n);
	    ret[n] = '\0';
	    return(ret);
	}

	n++;
	c = d;
    }
					/* copy partial string from buffer */
    memcpy((ret = sp = (char *) fs_get (n)), s, n);
					/* get more data */
    while(cnt < 0){
	memset(S(stream)->tmp, 0, GETBUFLEN);
	while((cnt = read(S(stream)->in.d, S(stream)->tmp, GETBUFLEN)) < 0)
	  if(errno != EINTR){
	      fs_give((void **) &ret);
	      piped_abort(stream);
	      return(NULL);
	  }

	if(cnt == 0){
	    if(n > 0)
	      ret[n-1] = '\0';  /* to try to get error message logged */
	    else{
		piped_abort(stream);
		return(NULL);
	    }
	}

	ptr = S(stream)->tmp;
    }

    if(c == '\015' && *ptr == '\012'){
	ptr++;
	cnt--;
	ret[n - 1] = '\0';		/* tie off string with null */
    }
    else if (s = piped_getline(stream)) {
	ret = (char *) fs_get(n + 1 + (m = strlen (s)));
	memcpy(ret, sp, n);		/* copy first part */
	memcpy(ret + n, s, m);		/* and second part */
	fs_give((void **) &sp);		/* flush first part */
	fs_give((void **) &s);		/* flush second part */
	ret[n + m] = '\0';		/* tie off string with null */
    }

    return(ret);
}


/* 
 * piped_close - Replacement for tcp_close that closes pipes to our
 *		     child rather than a tcp connection
 */
void
piped_close(stream)
    void *stream;
{
    /*
     * Uninstall our hooks into smtp_send since it's being used by
     * the nntp driver as well...
     */
    (void) close_system_pipe((PIPE_S **) &stream, NULL, 0);
}


/*
 * piped_abort - close down the pipe we were using to post
 */
void
piped_abort(stream)
    void *stream;
{
    if(S(stream)->in.d >= 0){
	close(S(stream)->in.d);
	S(stream)->in.d = -1;
    }

    if(S(stream)->out.d){
	close(S(stream)->out.d);
	S(stream)->out.d = -1;
    }
}


char *
piped_host(stream)
    void *stream;
{
    return(ps_global->hostname ? ps_global->hostname : "localhost");
}


unsigned long
piped_port(stream)
    void *stream;
{
    return(0L);
}
