#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: other.c 14081 2005-09-12 22:04:25Z hubert@u.washington.edu $";
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
      other.c

      This implements the "setup" screen of miscellaneous commands such
  as keyboard lock, and disk usage

  ====*/

#include "headers.h"
#include "adrbklib.h"

extern PAT_HANDLE **cur_pat_h;

#define	BODY_LINES(X)	((X)->ttyo->screen_rows -HEADER_ROWS(X)-FOOTER_ROWS(X))

#define	CONFIG_SCREEN_TITLE		"SETUP CONFIGURATION"
#define	CONFIG_SCREEN_TITLE_EXC		"SETUP CONFIGURATION EXCEPTIONS"
#define	CONFIG_SCREEN_HELP_TITLE	"HELP FOR SETUP CONFIGURATION"
#define	R_SELD				'*'
#define	EXIT_PMT "Commit changes (\"Yes\" replaces settings, \"No\" abandons changes)"
static char *empty_val  = "Empty Value";
static char *empty_val2 = "<Empty Value>";
#define EMPTY_VAL_LEN     11
static char *no_val     = "No Value Set";
#define NO_VAL_LEN        12
static char *fixed_val  = "Value is Fixed";
static char yesstr[] = "Yes";
static char nostr[]  = "No";
#define NOT		"! "
#define NOTLEN		2

#define ARB_HELP "HELP FOR ARBITRARY HEADER PATTERNS"
#define ADDXHDRS "Add Extra Headers"

/* another in a long line of hacks in this file */
#define DSTRING "default ("
#define VSTRING "value from fcc-name-rule"

static EditWhich ew = Main;
static int treat_color_vars_as_text;

typedef struct edit_arb {
    struct variable *v;
    ARBHDR_S        *a;
    struct edit_arb *next;
} EARB_S;

typedef struct conf_line {
    char	     *varname,			/* alloc'd var name string   */
		     *value;			/* alloc'd var value string  */
    short	      varoffset;		/* offset from screen left   */
    short	      valoffset;		/* offset from screen left   */
    short	      val2offset;		/* offset from screen left   */
    struct variable  *var;			/* pointer to pinerc var     */
    long	      varmem;			/* value's index, if list    */
    int		      (*tool)();		/* tool to manipulate values */
    struct key_menu  *keymenu;			/* tool-specific  keymenu    */
    HelpType	      help;			/* variable's help text      */
    char	     *help_title;
    unsigned          flags;
    struct conf_line *varnamep;		/* pointer to varname        */
    struct conf_line *headingp;		/* pointer to heading        */
    struct conf_line *next, *prev;
    union flag_or_context_data {
	struct flag_conf {
	    struct flag_table **ftbl;	/* address of start of table */
	    struct flag_table  *fp;	/* pointer into table for each row */
	} f;
	struct context_and_screen {
	    CONTEXT_S  *ct;
	    CONT_SCR_S *cs;
	} c;
	struct role_conf {
	    PAT_LINE_S *patline;
	    PAT_S      *pat;
	    PAT_S     **selected;
	} r;
	struct abook_conf {
	    char **selected;
	    char  *abookname;
	} b;
	EARB_S **earb;
	struct list_select {
	    LIST_SEL_S *lsel;
	    ScreenMode *listmode;
	} l;
#ifdef	ENABLE_LDAP
	struct entry_and_screen {
	    LDAP          *ld;
	    LDAPMessage   *res;
	    LDAP_SERV_S   *info_used;
	    char          *serv;
	    ADDR_CHOOSE_S *ac;
	} a;
#endif
	struct take_export_val {
	    int         selected;
	    char       *exportval;
	    ScreenMode *listmode;
	} t;
    } d;
} CONF_S;

/*
 * Valid for flags argument of config screen tools or flags field in CONF_S
 */
#define	CF_CHANGES	0x0001		/* Have been earlier changes */
#define	CF_NOSELECT	0x0002		/* This line is unselectable */
#define	CF_NOHILITE	0x0004		/* Don't highlight varname   */
#define	CF_NUMBER	0x0008		/* Input should be numeric   */
#define	CF_INVISIBLEVAR	0x0010		/* Don't show the varname    */
#define CF_PRINTER      0x0020		/* Printer config line       */
#define	CF_H_LINE	0x0040		/* Horizontal line	     */
#define	CF_B_LINE	0x0080		/* Blank line		     */
#define	CF_CENTERED	0x0100		/* Centered text	     */
#define	CF_STARTITEM	0x0200		/* Start of an "item"        */
#define	CF_PRIVATE	0x0400		/* Private flag for tool     */
#define	CF_DOUBLEVAR	0x0800		/* Line has 2 settable vars  */
#define	CF_VAR2		0x1000		/* Cursor on 2nd of 2 vars   */
#define	CF_COLORSAMPLE	0x2000		/* Show color sample here    */
#define	CF_POT_SLCTBL	0x4000		/* Potentially selectable    */
#define	CF_INHERIT	0x8000		/* Inherit Defaults line     */


#define SPACE_BETWEEN_DOUBLEVARS 3
#define SAMPLE_LEADER "---------------------------"
#define SAMP1 "[Sample ]"
#define SAMP2 "[Default]"
#define SAMPLE_LEN 9
#define SAMPEXC "[Exception]"
#define SAMPEXC_LEN 11
#define SBS 1	/* space between samples */
#define HEADER_WORD "Header "
#define KW_COLORS_HDR "KEYWORD COLORS"
#define ADDHEADER_COMMENT "[ Use the AddHeader command to add colored headers in MESSAGE VIEW ]"
#define COLOR_BLOB "<    >"
#define COLOR_BLOB_LEN 6
#define EQ_COL 37
#define COLOR_INDENT 3
#define COLORNOSET "  [ Colors below may not be set until color is turned on above ]"

typedef struct save_config {
    union {
	char  *p;
	char **l;
    } saved_user_val;
} SAVED_CONFIG_S;

/*
 *
 */
typedef struct conf_screen {
    CONF_S  *current,
	    *prev,
	    *top_line;
    int      ro_warning,
	     deferred_ro_warning;
} OPT_SCREEN_S;


static OPT_SCREEN_S *opt_screen;
static char **def_printer_line;
static char no_ff[] = "-no-formfeed";

static long role_global_flags;
static PAT_STATE *role_global_pstate;

/*
 * This is pretty ugly. Some of the routines operate differently depending
 * on which variable they are operating on. Sometimes those variables are
 * global (real pine.h V_ style variables) and sometimes they are just
 * local variables (as in role_config_edit_screen). These pointers are here
 * so that the routines can figure out which variable they are operating
 * on and do the right thing.
 */
static struct variable	*score_act_global_ptr,
			*scorei_pat_global_ptr,
			*age_pat_global_ptr,
			*size_pat_global_ptr,
			*cati_global_ptr,
			*cat_cmd_global_ptr,
			*cat_lim_global_ptr,
			*startup_ptr,
			*role_comment_ptr,
			*role_forw_ptr,
			*role_repl_ptr,
			*role_fldr_ptr,
			*role_afrom_ptr,
			*role_filt_ptr,
			*role_status1_ptr,
			*role_status2_ptr,
			*role_status3_ptr,
			*role_status4_ptr,
			*role_status5_ptr,
			*role_status6_ptr,
			*role_status7_ptr,
			*role_status8_ptr,
			*msg_state1_ptr,
			*msg_state2_ptr,
			*msg_state3_ptr,
			*msg_state4_ptr;

#define next_confline(p)  ((p) ? (p)->next : NULL)
#define prev_confline(p)  ((p) ? (p)->prev : NULL)

/*
 * Macro's to help with color config support.
 */

/*
 * The CONF_S's varmem field serves dual purposes.  The low two bytes
 * are reserved for the pre-defined color index (though only 8 are 
 * defined for the nonce, and the high order bits are for the index
 * of the particular SPEC_COLOR_S this CONF_S is associated with.
 * Capiche?
 */
#define	CFC_ICOLOR(V)		((V)->varmem & 0xff)
#define	CFC_ICUST(V)		((V)->varmem >> 16)
#define	CFC_SET_COLOR(I, C)	(((I) << 16) | (C))
#define	CFC_ICUST_INC(V)	CFC_SET_COLOR(CFC_ICUST(V) + 1, CFC_ICOLOR(V))
#define	CFC_ICUST_DEC(V)	CFC_SET_COLOR(CFC_ICUST(V) - 1, CFC_ICOLOR(V))


typedef NAMEVAL_S *(*PTR_TO_RULEFUNC) PROTO((int));


/*
 * Internal prototypes
 */
void	 draw_klocked_body PROTO((char *, char *));
void	 update_option_screen PROTO((struct pine *, OPT_SCREEN_S *, Pos *));
void	 print_option_screen PROTO((OPT_SCREEN_S *, char *));
void	 option_screen_redrawer PROTO(());
int	 conf_scroll_screen PROTO((struct pine *, OPT_SCREEN_S *, CONF_S *,
				   char *, char *, int));
HelpType config_help PROTO((int, int));
int      text_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      text_toolit PROTO((struct pine *, int, CONF_S **, unsigned, int));
int      litsig_text_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      inbox_path_text_tool PROTO((struct pine *, int, CONF_S **, unsigned));
void     exception_override_warning PROTO((struct variable *));
int	 checkbox_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 flag_checkbox_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 radiobutton_tool PROTO((struct pine *, int, CONF_S **, unsigned));
void     standard_radio_setup PROTO((struct pine *, CONF_S **,
				     struct variable *, CONF_S **));
PTR_TO_RULEFUNC rulefunc_from_var PROTO((struct pine *, struct variable *));
int      feature_gets_an_x PROTO((struct pine *, CONF_S *, char **));
int      feature_in_list PROTO((char **, char *));
int      take_export_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      select_from_list_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 color_setting_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      color_edit_screen PROTO((struct pine *, CONF_S **));
void	 color_update_selected PROTO((struct pine *, CONF_S *, char *,
				      char *, int));
void     color_config_init_display PROTO((struct pine *, CONF_S **, CONF_S **));
char    *abook_select_screen PROTO((struct pine *));
void     add_header_color_line PROTO((struct pine *, CONF_S **, char *, int));
void     add_color_setting_disp PROTO((struct pine *,
				       CONF_S **, struct variable *, CONF_S *,
				       struct key_menu *, struct key_menu *,
				       HelpType, int, int,
				       char *, char *, int));
SPEC_COLOR_S *spec_color_from_var PROTO((char *, int));
SPEC_COLOR_S *spec_colors_from_varlist PROTO((char **, int));
char    *var_from_spec_color PROTO((SPEC_COLOR_S *));
char   **varlist_from_spec_colors PROTO((SPEC_COLOR_S *));
char	*new_color_line PROTO((char *, int, int, int));
int	 is_rgb_color PROTO((char *));
void     set_color_val PROTO((struct variable *, int));
char    *color_parenthetical PROTO((struct variable *));
int      var_defaults_to_rev PROTO((struct variable *));
void     write_custom_hdr_colors PROTO((struct pine *));
int	 yesno_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 print_select_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 print_edit_tool PROTO((struct pine *, int, CONF_S **, unsigned));
void	 set_def_printer_value PROTO((char *));
int      context_config_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      context_config_delete PROTO((struct pine *, CONF_S **));
int      context_config_add PROTO((struct pine *, CONF_S **));
int      context_config_edit PROTO((struct pine *, CONF_S **));
int      context_config_shuffle PROTO((struct pine *, CONF_S **));
int      ccs_var_delete PROTO((struct pine *, CONTEXT_S *));
int      ccs_var_insert PROTO((struct pine *, char *, struct variable *,
			       char **, int));
int	 context_select_tool PROTO((struct pine *, int, CONF_S **, unsigned));
#ifdef	ENABLE_LDAP
int	 addr_select_tool PROTO((struct pine *, int, CONF_S **, unsigned));
void     dir_init_display PROTO((struct pine *, CONF_S **, char **,
				 struct variable *, CONF_S **));
int	 dir_config_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      dir_edit_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      dir_edit_screen PROTO((struct pine *, LDAP_SERV_S *, char *,
				char **));
void	 dir_config_edit PROTO((struct pine *, CONF_S **));
void	 dir_config_add PROTO((struct pine *, CONF_S **));
void	 dir_config_del PROTO((struct pine *, CONF_S **));
void	 dir_config_shuffle PROTO((struct pine *, CONF_S **));
int	 ldap_checkbox_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 ldap_radiobutton_tool PROTO((struct pine *, int, CONF_S **, unsigned));
void	 toggle_ldap_option_bit PROTO((struct pine *, int, struct variable *,
				    char *));
void     add_ldap_server_to_display PROTO((struct pine *, CONF_S **, char *,
					   char *, struct variable *, int,
					   struct key_menu *, HelpType,
					   int (*)(), int, CONF_S **));
void     add_ldap_fake_first_server PROTO((struct pine *, CONF_S **,
					   struct variable *,
					   struct key_menu *, HelpType,
					   int (*)()));
NAMEVAL_S *ldap_feature_list PROTO((int));
#endif	/* ENABLE_LDAP */
void	 toggle_feature_bit PROTO((struct pine *, int, struct variable *,
				   CONF_S *, int));
void	 config_add_list PROTO((struct pine *, CONF_S **, char **,
				char ***, int));
void	 config_del_list_item PROTO((CONF_S **, char ***));
int      standard_radio_var PROTO((struct pine *, struct variable *));
char    *pretty_value PROTO((struct pine *, CONF_S *));
char    *text_pretty_value PROTO((struct pine *, CONF_S *));
char    *checkbox_pretty_value PROTO((struct pine *, CONF_S *));
char    *color_pretty_value PROTO((struct pine *, CONF_S *));
char    *radio_pretty_value PROTO((struct pine *, CONF_S *));
char    *sort_pretty_value PROTO((struct pine *, CONF_S *));
char    *generalized_sort_pretty_value PROTO((struct pine *, CONF_S *, int));
char    *yesno_pretty_value PROTO((struct pine *, CONF_S *));
char    *sigfile_pretty_value PROTO((struct pine *, CONF_S *));
void     set_radio_pretty_vals PROTO((struct pine *, CONF_S **));
char    *sample_text PROTO((struct pine *, struct variable *));
char    *sampleexc_text PROTO((struct pine *, struct variable *));
COLOR_PAIR *sample_color PROTO((struct pine *, struct variable *));
COLOR_PAIR *sampleexc_color PROTO((struct pine *, struct variable *));
char    *color_setting_text_line PROTO((struct pine *, struct variable *));
void     offer_to_fix_pinerc PROTO((struct pine *));
CONF_S	*new_confline PROTO((CONF_S **));
void	 snip_confline PROTO((CONF_S **));
void	 free_conflines PROTO((CONF_S **));
CONF_S	*first_confline PROTO((CONF_S *));
CONF_S  *first_sel_confline PROTO((CONF_S *));
CONF_S	*last_confline PROTO((CONF_S *));
void     maybe_add_to_incoming PROTO((CONTEXT_S *, char *));
int	 fixed_var PROTO((struct variable *, char *, char *));
int	 simple_exit_cmd PROTO((unsigned));
int      config_exit_cmd PROTO((unsigned));
int	 screen_exit_cmd PROTO((unsigned, char *));
void	 config_scroll_up PROTO((long));
void	 config_scroll_down PROTO((long));
void	 config_scroll_to_pos PROTO((long));
CONF_S  *config_top_scroll PROTO((struct pine *, CONF_S *));
char	*printer_name PROTO ((char *));
#ifdef	_WINDOWS
int      unix_color_style_in_pinerc PROTO((PINERC_S *));
char    *transformed_color PROTO((char *));
int      convert_pc_gray_names PROTO((struct pine *, PINERC_S *, EditWhich));
int	 config_scroll_callback PROTO((int, long));
#endif
void     fix_side_effects PROTO ((struct pine *, struct variable *, int));
SAVED_CONFIG_S *save_config_vars PROTO((struct pine *, int));
SAVED_CONFIG_S *save_color_config_vars PROTO((struct pine *));
void            revert_to_saved_config PROTO((struct pine *, SAVED_CONFIG_S *,
					      int));
void            revert_to_saved_color_config PROTO((struct pine *,
						    SAVED_CONFIG_S *));
void            free_saved_config PROTO((struct pine *, SAVED_CONFIG_S **,
					 int));
void            free_saved_color_config PROTO((struct pine *,
					       SAVED_CONFIG_S **));
int	 exclude_config_var PROTO((struct pine *, struct variable *, int));
int	 color_related_var PROTO((struct pine *, struct variable *));
int	 color_holding_var PROTO((struct pine *, struct variable *));
int      color_text_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      save_include PROTO((struct pine *, struct variable *, int));
char    *role_type_print PROTO((char *, char *, long));
void     role_config_init_disp PROTO((struct pine *, CONF_S **,
				      long, PAT_STATE *));
void     add_patline_to_display PROTO((struct pine *, CONF_S **, int, CONF_S **,
				       CONF_S **, PAT_LINE_S *, long));
void     free_earb PROTO((EARB_S **));
void     add_role_to_display PROTO((CONF_S **, PAT_LINE_S *, PAT_S *, int,
				    CONF_S **, int, long));
void     add_fake_first_role PROTO((CONF_S **, int, long));
int      abook_select_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_select_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_config_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_config_add PROTO((struct pine *, CONF_S **, long));
int      role_config_edit PROTO((struct pine *, CONF_S **, long));
int      role_config_shuffle PROTO((struct pine *, CONF_S **));
int      role_config_del PROTO((struct pine *, CONF_S **, long));
int      role_config_addfile PROTO((struct pine *, CONF_S **, long));
int      role_config_delfile PROTO((struct pine *, CONF_S **, long));
int      role_config_replicate PROTO((struct pine *, CONF_S **, long));
void     swap_literal_roles PROTO((CONF_S *, CONF_S *));
void     swap_file_roles PROTO((CONF_S *, CONF_S *));
void     move_role_around_file PROTO((CONF_S **, int));
void     move_role_into_file PROTO((CONF_S **, int));
void     move_role_outof_file PROTO((CONF_S **, int));
void     delete_a_role PROTO((CONF_S **, long));
PATTERN_S *addrlst_to_pattern PROTO((ADDRESS *));
int      role_config_edit_screen PROTO((struct pine *, PAT_S *,
					char *, long, PAT_S **));
int      role_text_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_cstm_text_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_litsig_text_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 role_filt_text_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_text_tool_inick PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_text_tool_afrom PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_text_tool_kword PROTO((struct pine *, int, CONF_S **, unsigned));
int      role_text_tool_charset PROTO((struct pine *, int, CONF_S **, unsigned));
void     calculate_inick_stuff PROTO((struct pine *));
int	 role_radiobutton_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 role_addhdr_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 role_filt_addhdr_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 role_sort_tool PROTO((struct pine *, int, CONF_S **, unsigned));
int	 role_filt_radiobutton_tool PROTO((struct pine *, int, CONF_S **,
					   unsigned));
int	 role_filt_exitcheck PROTO((CONF_S **, unsigned));
int      check_role_folders PROTO((char **, unsigned));
char   **get_role_specific_folder PROTO((CONF_S **));
int      delete_user_vals PROTO((struct variable *));
int	 feat_checkbox_tool PROTO((struct pine *, int, CONF_S **, unsigned));
void	 toggle_feat_option_bit PROTO((struct pine *, int, struct variable *,
				    char *));
NAMEVAL_S *feat_feature_list PROTO((int));
void     setup_dummy_pattern_var PROTO((struct variable *, char *,
					PATTERN_S *));
void     setup_role_pat PROTO((struct pine *, CONF_S **, struct variable *,
			       HelpType, char *, struct key_menu *,
			       int (*tool)(), EARB_S **, int));
void     setup_role_pat_alt PROTO((struct pine *, CONF_S **, struct variable *,
				   HelpType, char *, struct key_menu *,
				   int (*tool)(), int, int));


static char *klockin, *klockame;

void
redraw_kl_body()
{
#ifndef NO_KEYBOARD_LOCK
    ClearScreen();

    set_titlebar("KEYBOARD LOCK", ps_global->mail_stream,
		 ps_global->context_current, ps_global->cur_folder, NULL,
		 1, FolderName, 0, 0, NULL);

    PutLine0(6,3 ,
       "You may lock this keyboard so that no one else can access your mail");
    PutLine0(8, 3 ,
       "while you are away.  The screen will be locked after entering the ");
    PutLine0(10, 3 ,
       "password to be used for unlocking the keyboard when you return.");
    fflush(stdout);
#endif
}


void
redraw_klocked_body()
{
#ifndef NO_KEYBOARD_LOCK
    ClearScreen();

    set_titlebar("KEYBOARD LOCK", ps_global->mail_stream,
		 ps_global->context_current, ps_global->cur_folder, NULL,
		 1, FolderName, 0, 0, NULL);

    PutLine2(6, 3, "This keyboard is locked by %s <%s>.",klockame, klockin);
    PutLine0(8, 3, "To unlock, enter password used to lock the keyboard.");
    fflush(stdout);
#endif
}


#ifndef NO_KEYBOARD_LOCK
/*----------------------------------------------------------------------
          Execute the lock keyboard command

    Args: None

  Result: keyboard is locked until user gives password
  ---*/

lock_keyboard()
{
    struct pine *ps = ps_global;
    char inpasswd[80], passwd[80], pw[80];
    HelpType help = NO_HELP;
    int i, times, old_suspend, flags;

    passwd[0] = '\0';
    redraw_kl_body();
    ps->redrawer = redraw_kl_body;

    times = atoi(ps->VAR_KBLOCK_PASSWD_COUNT);
    if(times < 1 || times > 5){
	dprint(2, (debugfile,
	"Kblock-passwd-count var out of range (1 to 5) [%d]\n", times));
	times = 1;
    }

    inpasswd[0] = '\0';

    for(i = 0; i < times; i++){
	pw[0] = '\0';
	while(1){			/* input pasword to use for locking */
	    int rc;
	    char prompt[50];

	    sprintf(prompt,
		"%s password to LOCK keyboard %s: ",
		i ? "Retype" : "Enter",
		i > 1 ? "(Yes, again) " : "");

	    flags = OE_PASSWD;
	    rc =  optionally_enter(pw, -FOOTER_ROWS(ps), 0, sizeof(pw),
				    prompt, NULL, help, &flags);

	    if(rc == 3)
	      help = help == NO_HELP ? h_kb_lock : NO_HELP;
	    else if(rc == 1 || pw[0] == '\0'){
		q_status_message(SM_ORDER, 0, 2, "Keyboard lock cancelled");
		return(-1);
	    }
	    else if(rc != 4)
	      break;
	}

	if(!inpasswd[0])
	  strcpy(inpasswd, pw);
	else if(strcmp(inpasswd, pw)){
	    q_status_message(SM_ORDER, 0, 2,
		"Mismatch with initial password: keyboard lock cancelled");
	    return(-1);
	}
    }

    if(want_to("Really lock keyboard with entered password", 'y', 'n',
	       NO_HELP, WT_NORM) != 'y'){
	q_status_message(SM_ORDER, 0, 2, "Keyboard lock cancelled");
	return(-1);
    }

    draw_klocked_body(ps->VAR_USER_ID ? ps->VAR_USER_ID : "<no-user>",
		  ps->VAR_PERSONAL_NAME ? ps->VAR_PERSONAL_NAME : "<no-name>");

    ps->redrawer = redraw_klocked_body;
    if(old_suspend = F_ON(F_CAN_SUSPEND, ps_global))
      F_TURN_OFF(F_CAN_SUSPEND, ps_global);

    while(strcmp(inpasswd, passwd)){
	if(passwd[0])
	  q_status_message(SM_ORDER | SM_DING, 3, 3,
		     "Password to UNLOCK doesn't match password used to LOCK");
        
        help = NO_HELP;
        while(1){
	    int rc;

	    flags = OE_PASSWD | OE_DISALLOW_CANCEL;
	    rc =  optionally_enter(passwd, -FOOTER_ROWS(ps), 0, sizeof(passwd),
				   "Enter password to UNLOCK keyboard : ",NULL,
				   help, &flags);
	    if(rc == 3) {
		help = help == NO_HELP ? h_oe_keylock : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
        }
    }

    if(old_suspend)
      F_TURN_ON(F_CAN_SUSPEND, ps_global);

    q_status_message(SM_ORDER, 0, 3, "Keyboard Unlocked");
    return(0);
}


void
draw_klocked_body(login, username)
    char *login, *username;
{
    klockin = login;
    klockame = username;
    redraw_klocked_body();
}
#endif /* !NO_KEYBOARD_LOCK */



/*
 *  * * * * *    Start of Config Screen Support Code   * * * * * 
 */

#define PREV_MENU {"P", "Prev", {MC_PREVITEM, 1, {'p'}}, KS_NONE}
#define NEXT_MENU {"N", "Next", {MC_NEXTITEM, 2, {'n','\t'}}, KS_NONE}
#define EXIT_SETUP_MENU \
	{"E", "Exit Setup", {MC_EXIT,1,{'e'}}, KS_EXITMODE}
#define TOGGLE_MENU \
	{"X", "[Set/Unset]", {MC_TOGGLE,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE}
#define TOGGLEB_MENU \
	{"X", "[Set/Unset]", {MC_TOGGLEB,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE}
#define TOGGLEC_MENU \
	{"X", "[Set/Unset]", {MC_TOGGLEC,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE}
#define TOGGLED_MENU \
	{"X", "[Set/Unset]", {MC_TOGGLED,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE}

static struct key config_text_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_text_keymenu, config_text_keys);

static struct key config_text_wshuf_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"$", "Shuffle", {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_text_wshuf_keymenu, config_text_wshuf_keys);

static struct key color_pattern_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(color_pattern_keymenu, color_pattern_keys);

static struct key take_export_keys_sm[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	{"<","ExitTake", {MC_EXIT,4,{'e',ctrl('C'),'<',','}}, KS_EXITMODE},
	{"T","[Take]",{MC_TAKE,3,{'t',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"L","ListMode",{MC_LISTMODE,1,{'l'}},KS_NONE}};
INST_KEY_MENU(take_export_keymenu_sm, take_export_keys_sm);

static struct key take_export_keys_lm[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	{"<","ExitTake", {MC_EXIT,4,{'e',ctrl('C'),'<',','}}, KS_EXITMODE},
	{"T","Take", {MC_TAKE,1,{'t'}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X","[Set/Unset]", {MC_CHOICE,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE},
	{"A", "SetAll",{MC_SELALL,1,{'a'}},KS_NONE},
	{"U","UnSetAll",{MC_UNSELALL,1,{'u'}},KS_NONE},
	{"S","SinglMode",{MC_LISTMODE,1,{'s'}},KS_NONE}};
INST_KEY_MENU(take_export_keymenu_lm, take_export_keys_lm);

static struct key config_role_file_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToFiles", {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	{"F", "editFile", {MC_EDITFILE, 1, {'f'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_file_keymenu, config_role_file_keys);

static struct key config_role_file_res_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToFiles", {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_file_res_keymenu, config_role_file_res_keys);

static struct key config_role_keyword_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToKeywords", {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_keyword_keymenu, config_role_keyword_keys);

static struct key config_role_keyword_keys_not[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToKeywords", {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	{"!", "toggle NOT", {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_keyword_keymenu_not, config_role_keyword_keys_not);

static struct key config_role_charset_keys_not[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToCharSets", {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	{"!", "toggle NOT", {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_charset_keymenu_not, config_role_charset_keys_not);

static struct key config_role_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_role_keymenu, config_role_keys);

static struct key config_role_keys_not[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"X", "eXtraHdr", {MC_ADDHDR, 1, {'x'}}, KS_NONE},
	{"!", "toggle NOT", {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_keymenu_not, config_role_keys_not);

static struct key config_role_keys_extra[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"X", "[eXtraHdr]", {MC_ADDHDR, 3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_role_keymenu_extra, config_role_keys_extra);

static struct key config_role_addr_pat_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToAddrBk", {MC_CHOICEB, 2, {'t', ctrl('T')}}, KS_NONE},
	{"X", "eXtraHdr", {MC_ADDHDR, 1, {'x'}}, KS_NONE},
	{"!", "toggle NOT", {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_addr_pat_keymenu, config_role_addr_pat_keys);

static struct key config_role_xtrahdr_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"X", "eXtraHdr", {MC_ADDHDR, 1, {'x'}}, KS_NONE},
	{"!", "toggle NOT", {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	{"R", "RemoveHdr", {MC_DELHDR, 1, {'r'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_xtrahdr_keymenu, config_role_xtrahdr_keys);

static struct key config_role_addr_act_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToAddrBk", {MC_CHOICEC, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_addr_act_keymenu, config_role_addr_act_keys);

static struct key config_role_patfolder_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToFldrs", {MC_CHOICED, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_patfolder_keymenu, config_role_patfolder_keys);

static struct key config_role_actionfolder_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToFldrs", {MC_CHOICEE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_actionfolder_keymenu, config_role_actionfolder_keys);

static struct key config_role_inick_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToNicks", {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_inick_keymenu, config_role_inick_keys);

static struct key config_role_afrom_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change Val]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Value", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete Val", {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", "ToAbookList", {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(config_role_afrom_keymenu, config_role_afrom_keys);

static struct key config_checkbox_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	TOGGLE_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_checkbox_keymenu, config_checkbox_keys);

static struct key hdr_color_checkbox_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	TOGGLEB_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(hdr_color_checkbox_keymenu, hdr_color_checkbox_keys);

static struct key kw_color_checkbox_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	TOGGLEC_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(kw_color_checkbox_keymenu, kw_color_checkbox_keys);

static struct key selectable_bold_checkbox_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	TOGGLED_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(selectable_bold_checkbox_keymenu, selectable_bold_checkbox_keys);

static struct key config_radiobutton_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"*", "[Select]", {MC_CHOICE,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_radiobutton_keymenu, config_radiobutton_keys);

static struct key config_yesno_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change]", {MC_TOGGLE,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_yesno_keymenu, config_yesno_keys);


static struct key color_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	{"E", "To Colors", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[Select]", {MC_CHOICE,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(color_changing_keymenu, color_changing_keys);


static struct key custom_color_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	{"E", "To Colors", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[Select]", {MC_CHOICEB,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(custom_color_changing_keymenu, custom_color_changing_keys);

static struct key kw_color_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	{"E", "To Colors", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[Select]", {MC_CHOICEC,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(kw_color_changing_keymenu, kw_color_changing_keys);


#ifdef	_WINDOWS

static struct key color_rgb_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	{"E", "To Colors", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[Select]", {MC_CHOICE,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"C", "Customize", {MC_RGB1,1,{'c'}},KS_NONE},
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(color_rgb_keymenu, color_rgb_changing_keys);


static struct key custom_rgb_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	{"E", "To Colors", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[Select]", {MC_CHOICEB,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"C", "Customize", {MC_RGB2,1,{'c'}},KS_NONE},
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(custom_rgb_keymenu, custom_rgb_changing_keys);

static struct key kw_rgb_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	{"E", "To Colors", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[Select]", {MC_CHOICEC,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"C", "Customize", {MC_RGB3,1,{'c'}},KS_NONE},
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(kw_rgb_keymenu, kw_rgb_changing_keys);

#endif


static struct key color_setting_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "AddHeader", {MC_ADD,1,{'a'}}, KS_NONE},
	{"R", "RestoreDefs", {MC_DEFAULT,1,{'r'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(color_setting_keymenu, color_setting_keys);

static struct key custom_color_setting_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "AddHeader", {MC_ADD,1,{'a'}}, KS_NONE},
	{"R", "RestoreDefs", {MC_DEFAULT,1,{'r'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"D", "DeleteHdr", {MC_DELETE,1,{'d'}}, KS_NONE},
	{"$", "ShuffleHdr", {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(custom_color_setting_keymenu, custom_color_setting_keys);

static struct key role_color_setting_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"*", "[Select]", {MC_CHOICE,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(role_color_setting_keymenu, role_color_setting_keys);

static struct key kw_color_setting_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"C", "[Change]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "AddHeader", {MC_ADD,1,{'a'}}, KS_NONE},
	{"R", "RestoreDefs", {MC_DEFAULT,1,{'r'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(kw_color_setting_keymenu, kw_color_setting_keys);






/*----------------------------------------------------------------------
    Present pinerc data for manipulation

    Args: None

  Result: help edit certain pinerc fields.
  ---*/
void
option_screen(ps, edit_exceptions)
    struct pine *ps;
    int          edit_exceptions;
{
    char	    tmp[MAXPATH+1], *pval, **lval;
    int		    i, j, ln = 0, lv, readonly_warning = 0;
    struct	    variable  *vtmp;
    CONF_S	   *ctmpa = NULL, *ctmpb, *first_line = NULL;
    FEATURE_S	   *feature;
    SAVED_CONFIG_S *vsave;
    OPT_SCREEN_S    screen;
    int             expose_hidden_config, add_hidden_vars_title = 0;

    dprint(3,(debugfile, "-- option_screen --\n"));

    expose_hidden_config = F_ON(F_EXPOSE_HIDDEN_CONFIG, ps_global);
    treat_color_vars_as_text = expose_hidden_config;

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    treat_color_vars_as_text = 0;
	    return;
	}
    }

    ps->next_screen = SCREEN_FUN_NULL;

    mailcap_free(); /* free resources we won't be using for a while */

    if(ps->fix_fixed_warning)
      offer_to_fix_pinerc(ps);

    /*
     * First, find longest variable name
     */
    for(vtmp = ps->vars; vtmp->name; vtmp++){
	if(exclude_config_var(ps, vtmp, expose_hidden_config))
	  continue;

	if((i = strlen(vtmp->name)) > ln)
	  ln = i;
    }

    dprint(9,(debugfile, "initialize config list\n"));

    /*
     * Next, allocate and initialize config line list...
     */
    for(vtmp = ps->vars; vtmp->name; vtmp++){
	/*
	 * INCOMING_FOLDERS is currently the first of the normally
	 * hidden variables. Should probably invent a more robust way
	 * to keep this up to date.
	 */
	if(expose_hidden_config && vtmp == &ps->vars[V_INCOMING_FOLDERS])
	  add_hidden_vars_title = 1;

	if(exclude_config_var(ps, vtmp, expose_hidden_config))
	  continue;

	if(add_hidden_vars_title){

	    add_hidden_vars_title = 0;

	    new_confline(&ctmpa);		/* Blank line */
	    ctmpa->flags	       |= CF_NOSELECT | CF_B_LINE;

	    new_confline(&ctmpa)->var	= NULL;
	    ctmpa->help			= NO_HELP;
	    ctmpa->valoffset		= 2;
	    ctmpa->flags	       |= CF_NOSELECT;
	    ctmpa->value = cpystr("--- [ Normally hidden configuration options ] ---");

	    new_confline(&ctmpa);		/* Blank line */
	    ctmpa->flags	       |= CF_NOSELECT | CF_B_LINE;
	}

	if(vtmp->is_list)
	  lval  = LVAL(vtmp, ew);
	else
	  pval  = PVAL(vtmp, ew);

	new_confline(&ctmpa)->var = vtmp;
	if(!first_line)
	  first_line = ctmpa;

	ctmpa->valoffset = ln + 3;
	if(vtmp->is_list)
	  ctmpa->keymenu	 = &config_text_wshuf_keymenu;
	else
	  ctmpa->keymenu	 = &config_text_keymenu;
	  
	ctmpa->help	 = config_help(vtmp - ps->vars, 0);
	ctmpa->tool	 = text_tool;

	sprintf(tmp, "%-*.100s =", ln, vtmp->name);
	ctmpa->varname  = cpystr(tmp);
	ctmpa->varnamep = ctmpb = ctmpa;
	ctmpa->flags   |= CF_STARTITEM;
	if(vtmp == &ps->vars[V_FEATURE_LIST]){	/* special checkbox case */
	    char *this_sect, *new_sect;

	    ctmpa->flags		 |= CF_NOSELECT;
	    ctmpa->keymenu		  = &config_checkbox_keymenu;
	    ctmpa->tool			  = NULL;

	    /* put a nice delimiter before list */
	    new_confline(&ctmpa)->var = NULL;
	    ctmpa->varnamep		  = ctmpb;
	    ctmpa->keymenu		  = &config_checkbox_keymenu;
	    ctmpa->help			  = NO_HELP;
	    ctmpa->tool			  = checkbox_tool;
	    ctmpa->valoffset		  = 12;
	    ctmpa->flags		 |= CF_NOSELECT;
	    ctmpa->value = cpystr("Set    Feature Name");

	    new_confline(&ctmpa)->var = NULL;
	    ctmpa->varnamep		  = ctmpb;
	    ctmpa->keymenu		  = &config_checkbox_keymenu;
	    ctmpa->help			  = NO_HELP;
	    ctmpa->tool			  = checkbox_tool;
	    ctmpa->valoffset		  = 12;
	    ctmpa->flags		 |= CF_NOSELECT;
	    ctmpa->value = cpystr("---  ----------------------");

	    for(i = 0, this_sect = NULL; feature = feature_list(i); i++)
	      if((new_sect = feature_list_section(feature)) &&
		 (strcmp(new_sect, HIDDEN_PREF) != 0)){
		  if(this_sect != new_sect){
		      new_confline(&ctmpa)->var = NULL;
		      ctmpa->varnamep		= ctmpb;
		      ctmpa->keymenu		= &config_checkbox_keymenu;
		      ctmpa->help		= NO_HELP;
		      ctmpa->tool		= checkbox_tool;
		      ctmpa->valoffset		= 2;
		      ctmpa->flags	       |= (CF_NOSELECT | CF_STARTITEM);
		      sprintf(tmp, "[ %.100s ]", this_sect = new_sect);
		      ctmpa->value = cpystr(tmp);
		  }

		  new_confline(&ctmpa)->var = vtmp;
		  ctmpa->varnamep	    = ctmpb;
		  ctmpa->keymenu	    = &config_checkbox_keymenu;
		  ctmpa->help		    = config_help(vtmp-ps->vars,
							  feature->id);
		  ctmpa->tool		    = checkbox_tool;
		  ctmpa->valoffset	    = 12;
		  ctmpa->varmem		    = i;
		  ctmpa->value		    = pretty_value(ps, ctmpa);
	      }
	}
	else if(standard_radio_var(ps, vtmp)){
	    standard_radio_setup(ps, &ctmpa, vtmp, NULL);
	}
	else if(vtmp == &ps->vars[V_SORT_KEY]){ /* radio case */
	    SortOrder def_sort;
	    int       def_sort_rev;

	    ctmpa->flags       |= CF_NOSELECT;
	    ctmpa->keymenu      = &config_radiobutton_keymenu;
	    ctmpa->tool		= NULL;

	    /* put a nice delimiter before list */
	    new_confline(&ctmpa)->var = NULL;
	    ctmpa->varnamep		  = ctmpb;
	    ctmpa->keymenu		  = &config_radiobutton_keymenu;
	    ctmpa->help			  = NO_HELP;
	    ctmpa->tool			  = radiobutton_tool;
	    ctmpa->valoffset		  = 12;
	    ctmpa->flags		 |= CF_NOSELECT;
	    ctmpa->value = cpystr("Set    Sort Options");

	    new_confline(&ctmpa)->var = NULL;
	    ctmpa->varnamep	      = ctmpb;
	    ctmpa->keymenu	      = &config_radiobutton_keymenu;
	    ctmpa->help		      = NO_HELP;
	    ctmpa->tool		      = radiobutton_tool;
	    ctmpa->valoffset	      = 12;
	    ctmpa->flags             |= CF_NOSELECT;
	    ctmpa->value = cpystr("---  ----------------------");

	    /* find longest value's name */
	    for(lv = 0, i = 0; ps->sort_types[i] != EndofList; i++)
	      if(lv < (j = strlen(sort_name(ps->sort_types[i]))))
		lv = j;
	    
	    decode_sort(pval, &def_sort, &def_sort_rev);

	    for(j = 0; j < 2; j++){
		for(i = 0; ps->sort_types[i] != EndofList; i++){
		    new_confline(&ctmpa)->var = vtmp;
		    ctmpa->varnamep	      = ctmpb;
		    ctmpa->keymenu	      = &config_radiobutton_keymenu;
		    ctmpa->help		      = config_help(vtmp - ps->vars, 0);
		    ctmpa->tool		      = radiobutton_tool;
		    ctmpa->valoffset	      = 12;
		    ctmpa->varmem	      = i + (j * EndofList);
		    ctmpa->value	      = pretty_value(ps, ctmpa);
		}
	    }
	}
	else if(vtmp == &ps->vars[V_USE_ONLY_DOMAIN_NAME]){ /* yesno case */
	    ctmpa->keymenu = &config_yesno_keymenu;
	    ctmpa->tool	   = yesno_tool;
	    ctmpa->value   = pretty_value(ps, ctmpa);
	}
	else if(vtmp == &ps->vars[V_LITERAL_SIG]){
	    ctmpa->tool    = litsig_text_tool;
	    ctmpa->value   = pretty_value(ps, ctmpa);
	}
	else if(vtmp == &ps->vars[V_INBOX_PATH]){
	    ctmpa->tool    = inbox_path_text_tool;
	    ctmpa->value   = pretty_value(ps, ctmpa);
	}
	else if(vtmp->is_list){
	    if(lval){
		for(i = 0; lval[i]; i++){
		    if(i)
		      (void)new_confline(&ctmpa);

		    ctmpa->var       = vtmp;
		    ctmpa->varmem    = i;
		    ctmpa->valoffset = ln + 3;
		    ctmpa->value     = pretty_value(ps, ctmpa);
		    ctmpa->keymenu   = &config_text_wshuf_keymenu;
		    ctmpa->help      = config_help(vtmp - ps->vars, 0);
		    ctmpa->tool      = text_tool;
		    ctmpa->varnamep  = ctmpb;
		}
	    }
	    else{
		ctmpa->varmem = 0;
		ctmpa->value  = pretty_value(ps, ctmpa);
	    }
	}
	else{
	    if(vtmp == &ps->vars[V_FILLCOL]
	       || vtmp == &ps->vars[V_QUOTE_SUPPRESSION]
	       || vtmp == &ps->vars[V_OVERLAP]
	       || vtmp == &ps->vars[V_MAXREMSTREAM]
	       || vtmp == &ps->vars[V_MARGIN]
	       || vtmp == &ps->vars[V_DEADLETS]
	       || vtmp == &ps->vars[V_NMW_WIDTH]
	       || vtmp == &ps->vars[V_STATUS_MSG_DELAY]
	       || vtmp == &ps->vars[V_MAILCHECK]
	       || vtmp == &ps->vars[V_MAILCHECKNONCURR]
	       || vtmp == &ps->vars[V_MAILDROPCHECK]
	       || vtmp == &ps->vars[V_NNTPRANGE]
	       || vtmp == &ps->vars[V_TCPOPENTIMEO]
	       || vtmp == &ps->vars[V_TCPREADWARNTIMEO]
	       || vtmp == &ps->vars[V_TCPWRITEWARNTIMEO]
	       || vtmp == &ps->vars[V_TCPQUERYTIMEO]
	       || vtmp == &ps->vars[V_RSHOPENTIMEO]
	       || vtmp == &ps->vars[V_SSHOPENTIMEO]
	       || vtmp == &ps->vars[V_USERINPUTTIMEO]
	       || vtmp == &ps->vars[V_REMOTE_ABOOK_VALIDITY]
	       || vtmp == &ps->vars[V_REMOTE_ABOOK_HISTORY])
	      ctmpa->flags |= CF_NUMBER;

	    ctmpa->value = pretty_value(ps, ctmpa);
	}
    }

    dprint(9,(debugfile, "add hidden features\n"));

    /* add the hidden features */
    if(expose_hidden_config){
	char *new_sect;

	new_confline(&ctmpa);		/* Blank line */
	ctmpa->flags	       |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmpa)->var	= NULL;
	ctmpa->help			= NO_HELP;
	ctmpa->valoffset		= 2;
	ctmpa->flags	       |= CF_NOSELECT;
	ctmpa->value = cpystr("--- [ Normally hidden configuration features ] ---");

	new_confline(&ctmpa);		/* Blank line */
	ctmpa->flags	       |= CF_NOSELECT | CF_B_LINE;

	vtmp = &ps->vars[V_FEATURE_LIST];

	ctmpa->flags		 |= CF_NOSELECT;
	ctmpa->keymenu		  = &config_checkbox_keymenu;
	ctmpa->tool			  = NULL;

	/* put a nice delimiter before list */
	new_confline(&ctmpa)->var = NULL;
	ctmpa->varnamep		  = ctmpb;
	ctmpa->keymenu		  = &config_checkbox_keymenu;
	ctmpa->help			  = NO_HELP;
	ctmpa->tool			  = checkbox_tool;
	ctmpa->valoffset		  = 12;
	ctmpa->flags		 |= CF_NOSELECT;
	ctmpa->value = cpystr("Set    Feature Name");

	new_confline(&ctmpa)->var = NULL;
	ctmpa->varnamep		  = ctmpb;
	ctmpa->keymenu		  = &config_checkbox_keymenu;
	ctmpa->help			  = NO_HELP;
	ctmpa->tool			  = checkbox_tool;
	ctmpa->valoffset		  = 12;
	ctmpa->flags		 |= CF_NOSELECT;
	ctmpa->value = cpystr("---  ----------------------");

	for(i = 0; feature = feature_list(i); i++)
	  if((new_sect = feature_list_section(feature)) &&
	     (strcmp(new_sect, HIDDEN_PREF) == 0)){

	      new_confline(&ctmpa)->var	= vtmp;
	      ctmpa->varnamep		= ctmpb;
	      ctmpa->keymenu		= &config_checkbox_keymenu;
	      ctmpa->help		= config_help(vtmp-ps->vars,
						      feature->id);
	      ctmpa->tool		= checkbox_tool;
	      ctmpa->valoffset		= 12;
	      ctmpa->varmem		= i;
	      ctmpa->value		= pretty_value(ps, ctmpa);
	  }
    }

    vsave = save_config_vars(ps, expose_hidden_config);
    first_line = first_sel_confline(first_line);

    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = readonly_warning;
    switch(conf_scroll_screen(ps, &screen, first_line,
			      edit_exceptions ? CONFIG_SCREEN_TITLE_EXC
					      : CONFIG_SCREEN_TITLE,
			      "configuration ", 0)){
      case 0:
	break;

      case 1:
	write_pinerc(ps, ew, WRP_NONE);
	break;
    
      case 10:
	revert_to_saved_config(ps, vsave, expose_hidden_config);
	break;
      
      default:
	q_status_message(SM_ORDER,7,10,
	    "conf_scroll_screen bad ret, not supposed to happen");
	break;
    }

    pval = PVAL(&ps->vars[V_SORT_KEY], ew);
    if(vsave[V_SORT_KEY].saved_user_val.p && pval
       && strcmp(vsave[V_SORT_KEY].saved_user_val.p, pval)){
	if(!mn_get_mansort(ps_global->msgmap)){
	    clear_index_cache();
	    reset_sort_order(SRT_VRB);
	}
    }

    treat_color_vars_as_text = 0;
    free_saved_config(ps, &vsave, expose_hidden_config);
#ifdef _WINDOWS
    mswin_set_quit_confirm (F_OFF(F_QUIT_WO_CONFIRM, ps_global));
#endif
}


/*
 * We test for this same set of vars in a few places.
 */
int
standard_radio_var(ps, v)
    struct pine     *ps;
    struct variable *v;
{
    return(v == &ps->vars[V_SAVED_MSG_NAME_RULE] ||
	   v == &ps->vars[V_FCC_RULE] ||
	   v == &ps->vars[V_GOTO_DEFAULT_RULE] ||
	   v == &ps->vars[V_INCOMING_STARTUP] ||
	   v == &ps->vars[V_PRUNING_RULE] ||
	   v == &ps->vars[V_REOPEN_RULE] ||
	   v == &ps->vars[V_THREAD_DISP_STYLE] ||
	   v == &ps->vars[V_THREAD_INDEX_STYLE] ||
	   v == &ps->vars[V_FLD_SORT_RULE] ||
#ifndef	_WINDOWS
	   v == &ps->vars[V_COLOR_STYLE] ||
#endif
	   v == &ps->vars[V_INDEX_COLOR_STYLE] ||
	   v == &ps->vars[V_TITLEBAR_COLOR_STYLE] ||
	   v == &ps->vars[V_AB_SORT_RULE]);
}


PTR_TO_RULEFUNC
rulefunc_from_var(ps, v)
    struct pine     *ps;
    struct variable *v;
{
    PTR_TO_RULEFUNC rulefunc = NULL;

    if(v == &ps->vars[V_SAVED_MSG_NAME_RULE])
      rulefunc = save_msg_rules;
    else if(v == &ps->vars[V_FCC_RULE])
      rulefunc = fcc_rules;
    else if(v == &ps->vars[V_GOTO_DEFAULT_RULE])
      rulefunc = goto_rules;
    else if(v == &ps->vars[V_INCOMING_STARTUP])
      rulefunc = incoming_startup_rules;
    else if(v == startup_ptr)
      rulefunc = startup_rules;
    else if(v == &ps->vars[V_PRUNING_RULE])
      rulefunc = pruning_rules;
    else if(v == &ps->vars[V_REOPEN_RULE])
      rulefunc = reopen_rules;
    else if(v == &ps->vars[V_THREAD_DISP_STYLE])
      rulefunc = thread_disp_styles;
    else if(v == &ps->vars[V_THREAD_INDEX_STYLE])
      rulefunc = thread_index_styles;
    else if(v == &ps->vars[V_FLD_SORT_RULE])
      rulefunc = fld_sort_rules;
    else if(v == &ps->vars[V_AB_SORT_RULE])
      rulefunc = ab_sort_rules;
    else if(v == &ps->vars[V_INDEX_COLOR_STYLE])
      rulefunc = index_col_style;
    else if(v == &ps->vars[V_TITLEBAR_COLOR_STYLE])
      rulefunc = titlebar_col_style;
#ifndef	_WINDOWS
    else if(v == &ps->vars[V_COLOR_STYLE])
      rulefunc = col_style;
#endif
    
    return(rulefunc);
}


void
standard_radio_setup(ps, cl, v, first_line)
    struct pine     *ps;
    CONF_S         **cl;
    struct variable *v;
    CONF_S         **first_line;
{
    int     i, j, lv;
    CONF_S *ctmpb;
    PTR_TO_RULEFUNC rulefunc;
    NAMEVAL_S *f;

    if(!(cl && *cl))
      return;

    rulefunc = rulefunc_from_var(ps, v);
    ctmpb = (*cl);

    (*cl)->flags		|= CF_NOSELECT;
    (*cl)->keymenu      	 = &config_radiobutton_keymenu;
    (*cl)->tool			 = NULL;

    /* put a nice delimiter before list */
    new_confline(cl)->var	 = NULL;
    (*cl)->varnamep		 = ctmpb;
    (*cl)->keymenu		 = &config_radiobutton_keymenu;
    (*cl)->help			 = NO_HELP;
    (*cl)->tool			 = radiobutton_tool;
    (*cl)->valoffset		 = 12;
    (*cl)->flags		|= CF_NOSELECT;
    (*cl)->value		 = cpystr("Set    Rule Values");

    new_confline(cl)->var	 = NULL;
    (*cl)->varnamep		 = ctmpb;
    (*cl)->keymenu		 = &config_radiobutton_keymenu;
    (*cl)->help			 = NO_HELP;
    (*cl)->tool			 = radiobutton_tool;
    (*cl)->valoffset		 = 12;
    (*cl)->flags		|= CF_NOSELECT;
    (*cl)->value		 = cpystr("---  ----------------------");

    /* find longest name */
    if(rulefunc)
      for(lv = 0, i = 0; f = (*rulefunc)(i); i++)
        if(lv < (j = strlen(f->name)))
	  lv = j;
    
    if(rulefunc)
      for(i = 0; f = (*rulefunc)(i); i++){
	new_confline(cl)->var	= v;
	if(first_line && !*first_line && !pico_usingcolor())
	  *first_line = (*cl);

	(*cl)->varnamep		= ctmpb;
	(*cl)->keymenu		= &config_radiobutton_keymenu;
	(*cl)->help		= (v == startup_ptr)
				    ? h_config_other_startup
				    : config_help(v - ps->vars,0);
	(*cl)->tool		= radiobutton_tool;
	(*cl)->valoffset	= 12;
	(*cl)->varmem		= i;
	(*cl)->value		= pretty_value(ps, *cl);
      }
}


/*
 * Reset the displayed values for all of the lines for this
 * variable because others besides this line may change.
 */
void
set_radio_pretty_vals(ps, cl)
    struct pine *ps;
    CONF_S     **cl;
{
    CONF_S *ctmp;

    if(!(cl && *cl &&
       ((*cl)->var == &ps->vars[V_SORT_KEY] ||
        standard_radio_var(ps, (*cl)->var) ||
	(*cl)->var == startup_ptr)))
      return;

    /* hunt backwards */
    for(ctmp = *cl;
	ctmp && !(ctmp->flags & CF_NOSELECT) && !ctmp->varname;
	ctmp = prev_confline(ctmp)){
	if(ctmp->value)
	  fs_give((void **)&ctmp->value);
	
	ctmp->value = pretty_value(ps, ctmp);
    }

    /* hunt forwards */
    for(ctmp = *cl;
	ctmp && !ctmp->varname && !(ctmp->flags & CF_NOSELECT);
	ctmp = next_confline(ctmp)){
	if(ctmp->value)
	  fs_give((void **)&ctmp->value);
	
	ctmp->value = pretty_value(ps, ctmp);
    }
}


/*
 * test whether or not a var is 
 *
 * returns:  1 if it should be excluded, 0 otw
 */
int
exclude_config_var(ps, var, allow_hard_to_config_remotely)
    struct pine *ps;
    struct variable *var;
    int allow_hard_to_config_remotely;
{
    if((ew != Main && (var->is_onlymain)) ||
       (ew != ps_global->ew_for_except_vars && var->is_outermost))
      return(1);

    if(allow_hard_to_config_remotely)
      return(!(var->is_user && var->is_used && !var->is_obsolete));

    switch(var - ps->vars){
      case V_MAIL_DIRECTORY :
      case V_INCOMING_FOLDERS :
      case V_FOLDER_SPEC :
      case V_NEWS_SPEC :
      case V_STANDARD_PRINTER :
      case V_LAST_TIME_PRUNE_QUESTION :
      case V_LAST_VERS_USED :
      case V_ADDRESSBOOK :
      case V_GLOB_ADDRBOOK :
      case V_DISABLE_DRIVERS :
      case V_DISABLE_AUTHS :
      case V_REMOTE_ABOOK_METADATA :
      case V_REMOTE_ABOOK_HISTORY :
      case V_REMOTE_ABOOK_VALIDITY :
      case V_OPER_DIR :
      case V_USERINPUTTIMEO :
      case V_TCPOPENTIMEO :
      case V_TCPREADWARNTIMEO :
      case V_TCPWRITEWARNTIMEO :
      case V_TCPQUERYTIMEO :
      case V_RSHCMD :
      case V_RSHPATH :
      case V_RSHOPENTIMEO :
      case V_SSHCMD :
      case V_SSHPATH :
      case V_SSHOPENTIMEO :
      case V_SENDMAIL_PATH :
      case V_NEW_VER_QUELL :
      case V_PATTERNS :
      case V_PAT_ROLES :
      case V_PAT_FILTS :
      case V_PAT_SCORES :
      case V_PAT_INCOLS :
      case V_PAT_OTHER :
      case V_PRINTER :
      case V_PERSONAL_PRINT_COMMAND :
      case V_PERSONAL_PRINT_CATEGORY :
#if defined(DOS) || defined(OS2)
      case V_UPLOAD_CMD :
      case V_UPLOAD_CMD_PREFIX :
      case V_DOWNLOAD_CMD :
      case V_DOWNLOAD_CMD_PREFIX :
#ifdef	_WINDOWS
      case V_FONT_NAME :
      case V_FONT_SIZE :
      case V_FONT_STYLE :
      case V_FONT_CHAR_SET :
      case V_PRINT_FONT_NAME :
      case V_PRINT_FONT_SIZE :
      case V_PRINT_FONT_STYLE :
      case V_PRINT_FONT_CHAR_SET :
      case V_WINDOW_POSITION :
      case V_CURSOR_STYLE :
#endif	/* _WINDOWS */
#endif	/* DOS */
#ifdef	ENABLE_LDAP
      case V_LDAP_SERVERS :
#endif	/* ENABLE_LDAP */
	return(1);

      default:
	break;
    }

    return(!(var->is_user && var->is_used && !var->is_obsolete &&
	     !color_related_var(ps, var)));
}


/*
 * Test to indicate what should be saved in case user wants to abandon
 * changes.
 */
int
save_include(ps, v, allow_hard_to_config_remotely)
    struct pine     *ps;
    struct variable *v;
    int              allow_hard_to_config_remotely;
{
    return(!exclude_config_var(ps, v, allow_hard_to_config_remotely)
	   || (v->is_user
	    && v->is_used
	    && !v->is_obsolete
	    && (v == &ps->vars[V_PERSONAL_PRINT_COMMAND]
#ifdef	ENABLE_LDAP
	     || v == &ps->vars[V_LDAP_SERVERS]
#endif
		)));
}


#ifndef	DOS
static struct key printer_edit_keys[] = 
       {HELP_MENU,
	PRYNTTXT_MENU,
	EXIT_SETUP_MENU,
	{"S", "[Select]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Printer", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "DeletePrint", {MC_DELETE,1,{'d'}}, KS_NONE},
	{"C", "Change", {MC_EDIT,1,{'c'}}, KS_NONE},
	WHEREIS_MENU};
INST_KEY_MENU(printer_edit_keymenu, printer_edit_keys);

static struct key printer_select_keys[] = 
       {HELP_MENU,
	PRYNTTXT_MENU,
	EXIT_SETUP_MENU,
	{"S", "[Select]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(printer_select_keymenu, printer_select_keys);

/*
 * Information used to paint and maintain a line on the configuration screen
 */
/*----------------------------------------------------------------------
    The printer selection screen

   Draws the screen and prompts for the printer number and the custom
   command if so selected.

 ----*/
void
select_printer(ps, edit_exceptions) 
    struct pine *ps;
    int          edit_exceptions;
{
    struct        variable  *vtmp;
    CONF_S       *ctmpa = NULL, *ctmpb = NULL, *heading = NULL,
		 *start_line = NULL;
    int i, saved_printer_cat, readonly_warning = 0, no_ex;
    SAVED_CONFIG_S *vsave;
    char *saved_printer, **lval;
    OPT_SCREEN_S screen;

    if(edit_exceptions){
	q_status_message(SM_ORDER, 3, 7,
			 "Exception Setup not implemented for printer");
	return;
    }

    if(fixed_var(&ps_global->vars[V_PRINTER], "change", "printer"))
      return;

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;
    
    no_ex = (ps_global->ew_for_except_vars == Main);

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return;
	}
    }

    saved_printer = cpystr(ps->VAR_PRINTER);
    saved_printer_cat = ps->printer_category;

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("\"Select\" a port or |pipe-command as your default printer.");
#else
      = cpystr("You may \"Select\" a print command as your default printer.");
#endif

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("You may also add alternative ports or !pipes to the list in the");
#else
      = cpystr("You may also add custom print commands to the list in the");
#endif

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("\"Personally selected port or |pipe\" section below.");
#else
      = cpystr("\"Personally selected print command\" section below.");
#endif

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmpa);
    ctmpa->valoffset = 4;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    def_printer_line = &ctmpa->value;
    set_def_printer_value(ps->VAR_PRINTER);

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT | CF_B_LINE;

#ifndef OS2
    new_confline(&ctmpa);
    heading = ctmpa;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->varname
	= cpystr(" Printer attached to IBM PC or compatible, Macintosh");
    ctmpa->flags    |= (CF_NOSELECT | CF_STARTITEM);
    ctmpa->value     = cpystr("");
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
      = cpystr("This may not work with all attached printers, and will depend on the");
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
      = cpystr("terminal emulation/communications software in use. It is known to work");
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
      = cpystr("with Kermit and the latest UW version of NCSA telnet on Macs and PCs,");
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
      = cpystr("Versaterm Pro on Macs, and WRQ Reflections on PCs.");
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    start_line = ctmpb = ctmpa; /* default start line */
    ctmpa->valoffset = 17;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = h_config_set_att_ansi;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOHILITE;
    ctmpa->varoffset = 8;
    ctmpa->varname   = cpystr("Printer:");
    ctmpa->value     = cpystr(ANSI_PRINTER);
    ctmpa->varnamep  = ctmpb;
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 17;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = h_config_set_att_ansi2;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOHILITE;
    ctmpa->varoffset = 8;
    ctmpa->value     = (char *)fs_get(strlen(ANSI_PRINTER)+strlen(no_ff)+1);
    ctmpa->varnamep  = ctmpb;
    ctmpa->headingp  = heading;
    strcat(strcpy(ctmpa->value, ANSI_PRINTER), no_ff);

    new_confline(&ctmpa);
    ctmpa->valoffset = 17;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = h_config_set_att_wyse;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOHILITE;
    ctmpa->varoffset = 8;
    ctmpa->value     = cpystr(WYSE_PRINTER);
    ctmpa->varnamep  = ctmpb;
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 17;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = h_config_set_att_wyse2;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOHILITE;
    ctmpa->varoffset = 8;
    ctmpa->value     = (char *)fs_get(strlen(WYSE_PRINTER)+strlen(no_ff)+1);
    ctmpa->varnamep  = ctmpb;
    ctmpa->headingp  = heading;
    strcat(strcpy(ctmpa->value, WYSE_PRINTER), no_ff);
#endif

    new_confline(&ctmpa);
    ctmpa->valoffset = 0;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT | CF_B_LINE;
    ctmpa->var = &ps->vars[V_STANDARD_PRINTER];


    new_confline(&ctmpa);
    heading = ctmpa;
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->varname
#ifdef OS2
        = cpystr(" Standard OS/2 printer port");
#else
	= cpystr(" Standard UNIX print command");
#endif
    ctmpa->value = cpystr("");
    ctmpa->flags    |= (CF_NOSELECT | CF_STARTITEM);
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_STANDARD_PRINTER];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("Using this option may require you to use the OS/2 \"MODE\" command to");
#else
      = cpystr("Using this option may require setting your \"PRINTER\" or \"LPDEST\"");
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_STANDARD_PRINTER];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("direct printer output to the correct port.");
#else
      = cpystr("environment variable using the standard UNIX utilities.");
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_STANDARD_PRINTER];

    vtmp = &ps->vars[V_STANDARD_PRINTER];
    for(i = 0; vtmp->current_val.l[i]; i++){
	new_confline(&ctmpa);
	ctmpa->valoffset = 22;
	ctmpa->keymenu   = &printer_select_keymenu;
	ctmpa->help      = NO_HELP;
	ctmpa->help      = h_config_set_stand_print;
	ctmpa->tool      = print_select_tool;
	if(i == 0){
	    ctmpa->varoffset = 8;
	    ctmpa->varname   = cpystr("Printer List:");
	    ctmpa->flags    |= CF_NOHILITE|CF_PRINTER;
#ifdef OS2
	    start_line = ctmpb = ctmpa; /* default start line */
#else
	    ctmpb = ctmpa;
#endif
	}

	ctmpa->varnamep  = ctmpb;
	ctmpa->headingp  = heading;
	ctmpa->varmem = i;
	ctmpa->var = vtmp;
	ctmpa->value = printer_name(vtmp->current_val.l[i]);
    }

    new_confline(&ctmpa);
    ctmpa->valoffset = 0;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmpa);
    heading = ctmpa;
    ctmpa->valoffset = 0;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->varname
#ifdef OS2
        = cpystr(" Personally selected port or |command");
#else
	= cpystr(" Personally selected print command");
#endif
    ctmpa->flags    |= (CF_NOSELECT | CF_STARTITEM);
    ctmpa->value = cpystr("");
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];


    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("The text to be printed will be sent to the printer or command given here.");
#else
      = cpystr("The text to be printed will be piped into the command given here. The");
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("The printer port or |pipe is in the 2nd column, the printer name is in");
#else
      = cpystr("command is in the 2nd column, the printer name is in the first column. Some");
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("the first column. Examples: \"LPT1\", \"COM2\", \"|enscript\". A command may");
#else
      = cpystr("examples are: \"prt\", \"lpr\", \"lp\", or \"enscript\". The command may be given");
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("be given options, for example \"|ascii2ps -p LPT1\" or \"|txt2hplc -2\". Use");
#else
      = cpystr("with options, for example \"enscript -2 -r\" or \"lpr -Plpacc170\". The");
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("the |command method for printers that require conversion from ASCII.");
#else
      = cpystr("commands and options on your system may be different from these examples.");
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    vtmp = &ps->vars[V_PERSONAL_PRINT_COMMAND];
    lval = no_ex ? vtmp->current_val.l : LVAL(vtmp, ew);
    if(lval){
	for(i = 0; lval[i]; i++){
	    new_confline(&ctmpa);
	    ctmpa->valoffset = 22;
	    ctmpa->keymenu   = &printer_edit_keymenu;
	    ctmpa->help      = h_config_set_custom_print;
	    ctmpa->tool      = print_edit_tool;
	    if(i == 0){
		ctmpa->varoffset = 8;
		ctmpa->varname   = cpystr("Printer List:");
		ctmpa->flags    |= CF_NOHILITE|CF_PRINTER;
		ctmpb = ctmpa;
	    }

	    ctmpa->varnamep  = ctmpb;
	    ctmpa->headingp  = heading;
	    ctmpa->varmem = i;
	    ctmpa->var = vtmp;
	    ctmpa->value = printer_name(lval[i]);
	}
    }
    else{
	new_confline(&ctmpa);
	ctmpa->valoffset = 22;
	ctmpa->keymenu   = &printer_edit_keymenu;
	ctmpa->help      = h_config_set_custom_print;
	ctmpa->tool      = print_edit_tool;
	ctmpa->flags    |= CF_NOHILITE;
	ctmpa->varoffset = 8;
	ctmpa->varname   = cpystr("Printer List:");
	ctmpa->varnamep  = ctmpa;
	ctmpa->headingp  = heading;
	ctmpa->varmem    = 0;
	ctmpa->var       = vtmp;
	ctmpa->value     = cpystr("");
    }

    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = readonly_warning;
    vsave = save_config_vars(ps, 0);
    switch(conf_scroll_screen(ps, &screen, start_line,
			      edit_exceptions ? "SETUP PRINTER EXCEPTIONS"
					      : "SETUP PRINTER",
			      "printer config ", 0)){
      case 0:
	break;
    
      case 1:
	write_pinerc(ps, ew, WRP_NONE);
	break;
    
      case 10:
	revert_to_saved_config(ps, vsave, 0);
	ps->printer_category = saved_printer_cat;
	set_variable(V_PRINTER, saved_printer, 1, 0, ew);
	set_variable(V_PERSONAL_PRINT_CATEGORY, comatose(ps->printer_category),
		     1, 0, ew);
	break;
    }

    def_printer_line = NULL;
    free_saved_config(ps, &vsave, 0);
    fs_give((void **)&saved_printer);
}
#endif	/* !DOS */


void
set_def_printer_value(printer)
    char *printer;
{
    char *p, *nick, *cmd;
    int set, editing_norm_except_exists;

    if(!def_printer_line)
      return;
    
    editing_norm_except_exists = ((ps_global->ew_for_except_vars != Main) &&
				  (ew == Main));

    parse_printer(printer, &nick, &cmd, NULL, NULL, NULL, NULL);
    p = *nick ? nick : cmd;
    set = *p;
    if(*def_printer_line)
      fs_give((void **)def_printer_line);

    *def_printer_line = fs_get(60 + strlen(p));
    sprintf(*def_printer_line, "Default printer %s%s%s%s%s",
	set ? "set to \"" : "unset", set ? p : "", set ? "\"" : "",
	(set && editing_norm_except_exists) ? " (in exception config)" : "",
	set ? "." : ""); 

    fs_give((void **)&nick);
    fs_give((void **)&cmd);
}


static struct key flag_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", "Exit Flags", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        TOGGLE_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(flag_keymenu, flag_keys);


/*----------------------------------------------------------------------
   Function to control flag set/clearing

   Basically, turn the flags into a fake list of features...

 ----*/
void
flag_maintenance_screen(ps, flags)
    struct pine	       *ps;
    struct flag_screen *flags;
{
    int		  i, lv, lc, maxwidth, offset, need;
    char	  tmp[256], **p, *spacer;
    CONF_S	 *ctmpa = NULL, *first_line = NULL;
    struct	  flag_table  *fp;
    OPT_SCREEN_S  screen;

    maxwidth = max(min((ps->ttyo ? ps->ttyo->screen_cols : 80), 150), 30);

    for(p = flags->explanation; p && *p; p++){
	new_confline(&ctmpa);
	ctmpa->keymenu   = &flag_keymenu;
	ctmpa->help      = NO_HELP;
	ctmpa->tool      = flag_checkbox_tool;
	ctmpa->flags    |= CF_NOSELECT;
	ctmpa->valoffset = 0;
	ctmpa->value     = cpystr(*p);
    }

    /* Now wire flags checkboxes together */
    for(lv = 0, lc = 0, fp = (flags->flag_table ? *flags->flag_table : NULL);
	fp && fp->name; fp++){	/* longest name */
	if(lv < (i = strlen(fp->name)))
	  lv = i;
	if(fp->comment && lc < (i = strlen(fp->comment)))
	  lc = i;
    }
    
    lv = min(lv,100);
    lc = min(lc,100);
    if(lc > 0)
      spacer = "    ";
    else
      spacer = "";

    offset = 12;
    if((need = offset + 5 + lv + strlen(spacer) + lc) > maxwidth){
	offset -= (need - maxwidth);
	offset = max(0,offset);
	if((need = offset + 5 + lv + strlen(spacer) + lc) > maxwidth){
	    spacer = " ";
	    if((need = offset + 5 + lv + strlen(spacer) + lc) > maxwidth){
		lc -= (need - maxwidth);
		lc = max(0,lc);
		if(lc == 0)
		  spacer = "";
	    }
	}
    }

    new_confline(&ctmpa);
    ctmpa->keymenu   = &flag_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = flag_checkbox_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->valoffset = 0;
    ctmpa->value     = cpystr("");

    new_confline(&ctmpa);
    ctmpa->keymenu   = &flag_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = flag_checkbox_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->valoffset = 0;
    sprintf(tmp, "%*.*sSet  Flag Name", offset, offset, "");
    ctmpa->value = cpystr(tmp);

    new_confline(&ctmpa);
    ctmpa->keymenu   = &flag_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = flag_checkbox_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->valoffset = 0;
    sprintf(tmp, "%*.*s---  %.*s",
	    offset, offset, "",
	    lv+lc+strlen(spacer), repeat_char(lv+lc+strlen(spacer), '-'));
    ctmpa->value = cpystr(tmp);

    for(fp = (flags->flag_table ? *flags->flag_table : NULL);
	fp && fp->name; fp++){	/* build the list */
	new_confline(&ctmpa);
	if(!first_line)
	  first_line = ctmpa;

	ctmpa->keymenu		  = &flag_keymenu;
	ctmpa->help		  = fp->help;
	ctmpa->tool		  = flag_checkbox_tool;
	ctmpa->d.f.ftbl		  = flags->flag_table;
	ctmpa->d.f.fp		  = fp;
	ctmpa->valoffset	  = offset;

	sprintf(tmp, "[%c]  %-*.*s%.10s%-*.*s",
		(fp->set == 0) ? ' ' : (fp->set == 1) ? 'X' : '?',
		lv, lv, fp->name,
		spacer, lc, lc, fp->comment ? fp->comment : "");
	ctmpa->value = cpystr(tmp);
    }
      
    memset(&screen, 0, sizeof(screen));
    (void) conf_scroll_screen(ps, &screen, first_line,
			      "FLAG MAINTENANCE", "configuration ", 0);
    ps->mangled_screen = 1;
}



/*
 * Setup CollectionLists. Build a context list on the fly from the config
 * variable and edit it. This won't include Incoming-Folders because that
 * is a pseudo collection, but that's ok since we can't do the operations
 * on it, anyway. Reset real config list at the end.
 */
void
context_config_screen(ps, cs, edit_exceptions)
    struct pine *ps;
    CONT_SCR_S  *cs;
    int          edit_exceptions;
{
    CONTEXT_S    *top, **clist, *cp;
    CONF_S	 *ctmpa, *first_line, *heading;
    OPT_SCREEN_S  screen;
    int           i, readonly_warning, some_defined, ret;
    int           reinit_contexts = 0, prime;
    char        **lval, **lval1, **lval2, ***alval;
    struct variable fake_fspec_var, fake_nspec_var;
    struct variable *fake_fspec, *fake_nspec;

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return;
	}
    }

go_again:
    top = NULL; ctmpa = NULL; first_line = NULL;
    some_defined = 0, prime = 0;
    fake_fspec = &fake_fspec_var;
    fake_nspec = &fake_nspec_var;
    memset((void *)fake_fspec, 0, sizeof(*fake_fspec));
    memset((void *)fake_nspec, 0, sizeof(*fake_nspec));

    /* so fixed_var() will work right */
    fake_fspec->is_list = 1;
    fake_nspec->is_list = 1;
    if((ps->vars[V_FOLDER_SPEC]).is_fixed){
	fake_fspec->is_fixed = 1;
	if((ps->vars[V_FOLDER_SPEC]).fixed_val.l
	   && (ps->vars[V_FOLDER_SPEC]).fixed_val.l[0]){
	    fake_fspec->fixed_val.l = (char **) fs_get(2 * sizeof(char *));
	    fake_fspec->fixed_val.l[0]
			    = cpystr((ps->vars[V_FOLDER_SPEC]).fixed_val.l[0]);
	    fake_fspec->fixed_val.l[1] = NULL;
	}
    }

    if((ps->vars[V_NEWS_SPEC]).is_fixed){
	fake_nspec->is_fixed = 1;
	if((ps->vars[V_NEWS_SPEC]).fixed_val.l
	   && (ps->vars[V_NEWS_SPEC]).fixed_val.l[0]){
	    fake_nspec->fixed_val.l = (char **) fs_get(2 * sizeof(char *));
	    fake_nspec->fixed_val.l[0] = cpystr(INHERIT);
	    fake_nspec->fixed_val.l[0]
			    = cpystr((ps->vars[V_NEWS_SPEC]).fixed_val.l[0]);
	    fake_nspec->fixed_val.l[1] = NULL;
	}
    }

    clist = &top;
    lval1 = LVAL(&ps->vars[V_FOLDER_SPEC], ew);
    lval2 = LVAL(&ps->vars[V_NEWS_SPEC], ew);

    alval = ALVAL(fake_fspec, ew);
    if(lval1)
      *alval = copy_list_array(lval1);
    else if(!edit_exceptions && ps->VAR_FOLDER_SPEC && ps->VAR_FOLDER_SPEC[0] &&
	    ps->VAR_FOLDER_SPEC[0][0])
      *alval = copy_list_array(ps->VAR_FOLDER_SPEC);
    else
      fake_fspec = NULL;

    if(fake_fspec){
	lval = LVAL(fake_fspec, ew);
	for(i = 0; lval && lval[i]; i++){
	    cp = NULL;
	    if(i == 0 && !strcmp(lval[i], INHERIT)){
		cp = (CONTEXT_S *)fs_get(sizeof(*cp));
		memset((void *)cp, 0, sizeof(*cp));
		cp->use = CNTXT_INHERIT;
		cp->label = cpystr("Default collections are inherited");
	    }
	    else if(cp = new_context(lval[i], &prime)){
		cp->var.v = fake_fspec;
		cp->var.i = i;
	    }
	    
	    if(cp){
		*clist    = cp;			/* add it to list   */
		clist     = &cp->next;		/* prepare for next */
	    }
	}

	set_current_val(fake_fspec, FALSE, FALSE);
    }

    alval = ALVAL(fake_nspec, ew);
    if(lval2)
      *alval = copy_list_array(lval2);
    else if(!edit_exceptions && ps->VAR_NEWS_SPEC && ps->VAR_NEWS_SPEC[0] &&
	    ps->VAR_NEWS_SPEC[0][0])
      *alval = copy_list_array(ps->VAR_NEWS_SPEC);
    else
      fake_nspec = NULL;

    if(fake_nspec){
	lval = LVAL(fake_nspec, ew);
	for(i = 0; lval && lval[i]; i++){
	    cp = NULL;
	    if(i == 0 && !strcmp(lval[i], INHERIT)){
		cp = (CONTEXT_S *)fs_get(sizeof(*cp));
		memset((void *)cp, 0, sizeof(*cp));
		cp->use = CNTXT_INHERIT;
		cp->label = cpystr("Default collections are inherited");
	    }
	    else if(cp = new_context(lval[i], &prime)){
		cp->var.v = fake_nspec;
		cp->var.i = i;
	    }
	    
	    if(cp){
		*clist    = cp;			/* add it to list   */
		clist     = &cp->next;		/* prepare for next */
	    }
	}

	set_current_val(fake_nspec, FALSE, FALSE);
    }
    
    for(cp = top; cp; cp = cp->next)
      if(!(cp->use & CNTXT_INHERIT)){
	  some_defined++;
	  break;
      }

    if(edit_exceptions && !some_defined){
	q_status_message(SM_ORDER, 3, 7,
 "No exceptions to edit. First collection exception must be set by editing file");
	free_contexts(&top);
	if(reinit_contexts){
	    free_contexts(&ps_global->context_list);
	    init_folders(ps_global);
	}

	return;
    }


    /* fix up prev pointers */
    for(cp = top; cp; cp = cp->next)
      if(cp->next)
	cp->next->prev = cp;

    new_confline(&ctmpa);		/* blank line */
    ctmpa->keymenu    = cs->keymenu;
    ctmpa->help       = cs->help.text;
    ctmpa->help_title = cs->help.title;
    ctmpa->tool       = context_config_tool;
    ctmpa->flags     |= (CF_NOSELECT | CF_B_LINE);

    for(cp = top; cp; cp = cp->next){
	new_confline(&ctmpa);
	heading		  = ctmpa;
	if(!(cp->use & CNTXT_INHERIT))
	  ctmpa->value	  = cpystr(cp->nickname ? cp->nickname : cp->context);

	ctmpa->var	  = cp->var.v;
	ctmpa->keymenu    = cs->keymenu;
	ctmpa->help       = cs->help.text;
	ctmpa->help_title = cs->help.title;
	ctmpa->tool       = context_config_tool;
	ctmpa->flags	 |= CF_STARTITEM;
	ctmpa->valoffset  = 4;
	ctmpa->d.c.ct     = cp;
	ctmpa->d.c.cs	  = cs;
	if(cp->use & CNTXT_INHERIT)
	  ctmpa->flags |= CF_INHERIT | CF_NOSELECT;

	if((!first_line && !(cp->use & CNTXT_INHERIT)) ||
	   (!(cp->use & CNTXT_INHERIT) &&
	    cp->var.v &&
	    (cs->starting_var == cp->var.v) &&
	    (cs->starting_varmem == cp->var.i)))
	  first_line = ctmpa;

	/* Add explanatory text */
	new_confline(&ctmpa);
	ctmpa->value	  = cpystr(cp->label ? cp->label : "* * *");
	ctmpa->keymenu	  = cs->keymenu;
	ctmpa->help	  = cs->help.text;
	ctmpa->help_title = cs->help.title;
	ctmpa->tool	  = context_config_tool;
	ctmpa->flags	 |= CF_NOSELECT;
	ctmpa->valoffset  = 8;

	/* Always add blank line, make's shuffling a little easier */
	new_confline(&ctmpa);
	heading->headingp  = ctmpa;		/* use headingp to mark end */
	ctmpa->keymenu	   = cs->keymenu;
	ctmpa->help	   = cs->help.text;
	ctmpa->help_title  = cs->help.title;
	ctmpa->tool	   = context_config_tool;
	ctmpa->flags	  |= CF_NOSELECT | CF_B_LINE;
	ctmpa->valoffset   = 0;
    }

    cs->starting_var = NULL;


    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = readonly_warning;
    ret = conf_scroll_screen(ps, &screen, first_line, cs->title,
			     cs->print_string, 0);

    free_contexts(&top);
    
    if(ret)
      reinit_contexts++;

    /*
     * 15 means the tool wants us to reset and go again. The config var
     * has been changed. The contexts will be built again from the
     * config variable and the real contexts will be rebuilt below.
     * This is easier and safer than having the tools rebuild the context
     * list and the display correct. It's difficult to do because of all
     * the inheriting and defaulting going on.
     */
    if(ret == 15){
	if(edit_exceptions && !LVAL(fake_fspec, ew) && !LVAL(fake_nspec, ew)){
	    if(want_to("Really delete last exceptional collection",
		       'n', 'n', h_config_context_del_except,
		       WT_FLUSH_IN) != 'y'){
		free_variable_values(fake_fspec);
		free_variable_values(fake_nspec);
		goto go_again;
	    }
	}

	/* resolve variable changes */
	if(lval1 && !equal_list_arrays(lval1, LVAL(fake_fspec, ew)) ||
	   fake_fspec && !equal_list_arrays(ps->VAR_FOLDER_SPEC,
					    LVAL(fake_fspec, ew))){
	    i = set_variable_list(V_FOLDER_SPEC,
				  LVAL(fake_fspec, ew), TRUE, ew);
	    set_current_val(&ps->vars[V_FOLDER_SPEC], TRUE, FALSE);

	    if(i)
	      q_status_message(SM_ORDER, 3, 3,
			       "Trouble saving change, cancelled");
	    else if(!edit_exceptions && lval1 && !LVAL(fake_fspec, ew)){
		cs->starting_var = fake_fspec;
		cs->starting_varmem = 0;
		q_status_message(SM_ORDER, 3, 3,
		       "Deleted last folder-collection, reverting to default");
	    }
	    else if(!edit_exceptions && !lval1 && !LVAL(fake_fspec, ew)){
		cs->starting_var = fake_fspec;
		cs->starting_varmem = 0;
		q_status_message(SM_ORDER, 3, 3,
	       "Deleted default folder-collection, reverting back to default");
	    }
	}

	if(lval2 && !equal_list_arrays(lval2, LVAL(fake_nspec, ew)) ||
	   fake_nspec && !equal_list_arrays(ps->VAR_NEWS_SPEC,
					    LVAL(fake_nspec, ew))){
	    i = set_variable_list(V_NEWS_SPEC,
				  LVAL(fake_nspec, ew), TRUE, ew);
	    set_news_spec_current_val(TRUE, FALSE);

	    if(i)
	      q_status_message(SM_ORDER, 3, 3,
			       "Trouble saving change, cancelled");
	    else if(!edit_exceptions && lval2 && !LVAL(fake_nspec, ew) &&
		    ps->VAR_NEWS_SPEC && ps->VAR_NEWS_SPEC[0] &&
		    ps->VAR_NEWS_SPEC[0][0]){
		cs->starting_var = fake_nspec;
		cs->starting_varmem = 0;
		q_status_message(SM_ORDER, 3, 3,
		       "Deleted last news-collection, reverting to default");
	    }
	    else if(!edit_exceptions && !lval2 && !LVAL(fake_nspec, ew) &&
		    ps->VAR_NEWS_SPEC && ps->VAR_NEWS_SPEC[0] &&
		    ps->VAR_NEWS_SPEC[0][0]){
		cs->starting_var = fake_nspec;
		cs->starting_varmem = 0;
	        q_status_message(SM_ORDER, 3, 3,
	         "Deleted default news-collection, reverting back to default");
	    }
	}

	free_variable_values(fake_fspec);
	free_variable_values(fake_nspec);
	goto go_again;
    }

    ps->mangled_screen = 1;

    /* make the real context list match the changed config variables */
    if(reinit_contexts){
	free_contexts(&ps_global->context_list);
	init_folders(ps_global);
    }
}


int
context_config_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    int retval = 0;

    switch(cmd){
      case MC_DELETE :
	if(!fixed_var((*cl)->var, "delete", "collection"))
	  retval = context_config_delete(ps, cl);

	break;

      case MC_EDIT :
	if(!fixed_var((*cl)->var, "change", "collection"))
	  retval = context_config_edit(ps, cl);

	break;

      case MC_ADD :
	if(!fixed_var((*cl)->var, "add to", "collection"))
	  retval = context_config_add(ps, cl);

	break;

      case MC_SHUFFLE :
	if(!fixed_var((*cl)->var, "shuffle", "collection"))
	  retval = context_config_shuffle(ps, cl);

	break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      default:
	retval = -1;
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}


int
context_config_add(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    char            *raw_ctxt;
    struct variable *var;
    char           **lval;
    int              count;

    if(raw_ctxt = context_edit_screen(ps, "ADD", NULL, NULL, NULL, NULL)){

	/*
	 * If var is non-NULL we add to the end of that var.
	 * If it is NULL, that means we're adding to the current_val, so
	 * we'll have to soak up the default values from there into our
	 * own variable.
	 */
	if((*cl)->var){
	    var = (*cl)->var;
	    lval = LVAL((*cl)->var, ew);
	}
	else{
	    q_status_message(SM_ORDER|SM_DING, 3, 3,
			     "Programmer botch in context_config_add");
	    return(0);
	}

	for(count = 0; lval && lval[count]; count++)
	  ;

	if(!ccs_var_insert(ps, raw_ctxt, var, lval, count)){
	    fs_give((void **)&raw_ctxt);
	    q_status_message(SM_ORDER|SM_DING, 3, 3,
			     "Error adding new collection");
	    return(0);
	}

	fs_give((void **)&raw_ctxt);

	(*cl)->d.c.cs->starting_var = var;
	(*cl)->d.c.cs->starting_varmem = count;
	q_status_message(SM_ORDER, 0, 3,
			 "New collection added.  Use \"$\" to adjust order.");
	return(15);
    }

    ps->mangled_screen = 1;
    return(0);
}


int
context_config_shuffle(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    char      prompt[256];
    int	      n = 0, cmd, i1, i2, count = 0, insert_num, starting_varmem;
    int       news_problem = 0, deefault = 0;
    ESCKEY_S  ekey[3];
    CONTEXT_S *cur_ctxt, *other_ctxt;
    char     *tmp, **lval, **lval1, **lval2;
    struct variable *cur_var, *other_var;

    if(!((*cl)->d.c.ct && (*cl)->var))
      return(0);

    /* enable UP? */
    if((*cl)->d.c.ct->prev && !((*cl)->d.c.ct->prev->use & CNTXT_INHERIT)){
	/*
	 * Don't allow shuffling news collection up to become
	 * the primary collection. That would happen if prev is primary
	 * and this one is news.
	 */
	if((*cl)->d.c.ct->prev->use & CNTXT_SAVEDFLT &&
	   (*cl)->d.c.ct->use & CNTXT_NEWS)
	  news_problem++;
	else{
	    ekey[n].ch      = 'u';
	    ekey[n].rval    = 'u';
	    ekey[n].name    = "U";
	    ekey[n++].label = "Up";
	    deefault        = 'u';
	}
    }

    /* enable DOWN? */
    if((*cl)->d.c.ct->next && !((*cl)->d.c.ct->next->use & CNTXT_INHERIT)){
	/*
	 * Don't allow shuffling down past news collection if this
	 * is primary collection.
	 */
	if((*cl)->d.c.ct->use & CNTXT_SAVEDFLT &&
	   (*cl)->d.c.ct->next->use & CNTXT_NEWS)
	  news_problem++;
	else{
	    ekey[n].ch      = 'd';
	    ekey[n].rval    = 'd';
	    ekey[n].name    = "D";
	    ekey[n++].label = "Down";
	    if(!deefault)
	      deefault = 'd';
	}
    }

    if(n){
	ekey[n].ch = -1;
	sprintf(prompt, "Shuffle selected context %s%s%s? ",
		(ekey[0].ch == 'u') ?  "UP" : "",
		(n > 1) ? " or " : "",
		(ekey[0].ch == 'd' || n > 1) ? "DOWN" : "");

	cmd = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey,
			    deefault, 'x', NO_HELP, RB_NORM, NULL);
	switch(cmd){
	  case 'x':
	  default:
	    cmd_cancelled("Shuffle");
	    return(0);
	  
	  case 'u':
	  case 'd':
	    break;
	}

	/*
	 * This is complicated by the fact that there are two
	 * vars involved, the folder-collections and the news-collections.
	 * We may have to shuffle across collections.
	 */
	cur_ctxt = (*cl)->d.c.ct;
	if(cmd == 'd')
	  other_ctxt = (*cl)->d.c.ct->next;
	else if(cmd == 'u')
	  other_ctxt = (*cl)->d.c.ct->prev;
	
	cur_var = cur_ctxt->var.v;
	other_var = other_ctxt->var.v;

	/* swap elements of config var */
	if(cur_var == other_var){
	    i1 = cur_ctxt->var.i;
	    i2 = other_ctxt->var.i;
	    lval = LVAL(cur_var, ew);
	    if(lval){
		tmp = lval[i1];
		lval[i1] = lval[i2];
		lval[i2] = tmp;
	    }

	    starting_varmem = i2;
	}
	else{
	    /* swap into the other_var */
	    i1 = cur_ctxt->var.i;
	    i2 = other_ctxt->var.i;
	    lval1 = LVAL(cur_var, ew);
	    lval2 = LVAL(other_var, ew);
	    /* count */
	    for(count = 0; lval2 && lval2[count]; count++)
	      ;
	    if(cmd == 'd')
	      insert_num = count ? 1 : 0;
	    else{
		insert_num = count ? count - 1 : count;
	    }

	    starting_varmem = insert_num;
	    if(ccs_var_insert(ps,lval1[i1],other_var,lval2,insert_num)){
		if(!ccs_var_delete(ps, cur_ctxt)){
		    q_status_message(SM_ORDER|SM_DING, 3, 3,
				     "Error deleting shuffled context");
		    return(0);
		}
	    }
	    else{
		q_status_message(SM_ORDER, 3, 3,
				 "Trouble shuffling, cancelled");
		return(0);
	    }
	}

	(*cl)->d.c.cs->starting_var = other_var;
	(*cl)->d.c.cs->starting_varmem = starting_varmem;

	q_status_message(SM_ORDER, 0, 3, "Collections shuffled");
	return(15);
    }

    if(news_problem)
      q_status_message(SM_ORDER, 0, 3, "Sorry, cannot Shuffle news to top");
    else
      q_status_message(SM_ORDER, 0, 3, "Sorry, nothing to Shuffle");

    return(0);
}


int
context_config_edit(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    char            *raw_ctxt, tpath[MAILTMPLEN], *p, **lval;
    struct variable *var;
    int              i;

    if(!(*cl)->d.c.ct)
      return(0);

    /* Undigest the context */
    strncpy(tpath, ((*cl)->d.c.ct->context[0] == '{'
		    && (p = strchr((*cl)->d.c.ct->context, '}')))
		      ? ++p
		      : (*cl)->d.c.ct->context, sizeof(tpath)-1);
    tpath[sizeof(tpath)-1] = '\0';

    if(p = strstr(tpath, "%s"))
      *p = '\0';

    if(raw_ctxt = context_edit_screen(ps, "EDIT", (*cl)->d.c.ct->nickname,
				      (*cl)->d.c.ct->server, tpath,
				      (*cl)->d.c.ct->dir ?
				             (*cl)->d.c.ct->dir->view.user
					     : NULL)){

	if((*cl)->var){
	    var = (*cl)->var;
	    lval = LVAL(var, ew);
	    i = (*cl)->d.c.ct->var.i;
	    if(lval && lval[i])
	      fs_give((void **)&lval[i]);
	    
	    if(lval)
	      lval[i] = raw_ctxt;

	    (*cl)->d.c.cs->starting_var = var;
	    (*cl)->d.c.cs->starting_varmem = i;
	}
	else{
	    q_status_message(SM_ORDER|SM_DING, 3, 3,
			     "Programmer botch in context_config_edit");
	    return(0);
	}

	q_status_message(SM_ORDER, 0, 3, "Collection list entry updated");

	return(15);
    }

    ps->mangled_screen = 1;
    return(0);
}


int
context_config_delete(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    char       tmp[MAILTMPLEN];

    if(!(*cl)->var){
	q_status_message(SM_ORDER|SM_DING, 3, 3,
			 "Programmer botch in context_config_delete");
	return(0);
    }

    if((*cl)->d.c.ct->use & CNTXT_SAVEDFLT &&
       (*cl)->d.c.ct->next &&
       (*cl)->d.c.ct->next->use & CNTXT_NEWS &&
       (*cl)->d.c.ct->var.v == (*cl)->d.c.ct->next->var.v){
	q_status_message(SM_ORDER|SM_DING, 3, 3,
			 "Sorry, cannot Delete causing news to move to top");
	return(0);
    }

    sprintf(tmp, "Delete the collection definition for \"%.40s\"",
	    (*cl)->value);
    if(want_to(tmp, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){

	(*cl)->d.c.cs->starting_var = (*cl)->var;
	(*cl)->d.c.cs->starting_varmem = (*cl)->d.c.ct->var.i;
	if((*cl)->d.c.ct->next){
	    if((*cl)->d.c.ct->next->var.v != (*cl)->var){
		(*cl)->d.c.cs->starting_var = (*cl)->d.c.ct->next->var.v;
		(*cl)->d.c.cs->starting_varmem = 0;
	    }
	    else{
		(*cl)->d.c.cs->starting_var = (*cl)->var;
		(*cl)->d.c.cs->starting_varmem = (*cl)->d.c.ct->var.i;
	    }
	}
	else{
	    if((*cl)->d.c.ct->var.i > 0){
		(*cl)->d.c.cs->starting_var = (*cl)->var;
		(*cl)->d.c.cs->starting_varmem = (*cl)->d.c.ct->var.i - 1;
	    }
	    else{
		if((*cl)->d.c.ct->prev){
		    (*cl)->d.c.cs->starting_var = (*cl)->d.c.ct->prev->var.v;
		    (*cl)->d.c.cs->starting_varmem = (*cl)->d.c.ct->prev->var.i;
		}
	    }
	}
	  
	/* Remove from var list */
	if(!ccs_var_delete(ps, (*cl)->d.c.ct)){
	    q_status_message(SM_ORDER|SM_DING, 3, 3,
			     "Error deleting renamed context");
	    return(0);
	}

	q_status_message(SM_ORDER, 0, 3, "Collection deleted");

	return(15);
    }

    q_status_message(SM_ORDER, 0, 3, "No collections deleted");
    return(0);
}


int
ccs_var_delete(ps, ctxt)
    struct pine	 *ps;
    CONTEXT_S	 *ctxt;
{
    int		count, i;
    char      **newl = NULL, **lval, **lp, ***alval;

    if(ctxt)
      lval = LVAL(ctxt->var.v, ew);
    else
      lval = NULL;

    for(count = 0; lval && lval[count]; count++)
      ;				/* sum the list */

    if(count > 1){
	newl = (char **) fs_get(count * sizeof(char *));
	for(i = 0, lp = newl; lval[i]; i++)
	  if(i != ctxt->var.i)
	    *lp++ = cpystr(lval[i]);

	*lp = NULL;
    }
    
    alval = ALVAL(ctxt->var.v, ew);
    if(alval){
	free_list_array(alval);
	if(newl){
	    for(i = 0; newl[i] ; i++)	/* count elements */
	      ;

	    *alval = (char **) fs_get((i+1) * sizeof(char *));

	    for(i = 0; newl[i] ; i++)
	      (*alval)[i] = cpystr(newl[i]);

	    (*alval)[i]         = NULL;
	}
    }

    free_list_array(&newl);
    return(1);
}


/*
 * Insert into "var", which currently has values "oldvarval", the "newline"
 * at position "insert".
 */
int
ccs_var_insert(ps, newline, var, oldvarval, insert)
    struct pine     *ps;
    char            *newline;
    struct variable *var;
    char           **oldvarval;
    int              insert;
{
    int    count, i, offset;
    char **newl, ***alval;

    for(count = 0; oldvarval && oldvarval[count]; count++)
      ;

    if(insert < 0 || insert > count){
	q_status_message(SM_ORDER,3,5, "unexpected problem inserting folder");
	return(0);
    }

    newl = (char **)fs_get((count + 2) * sizeof(char *));
    newl[insert] = cpystr(newline);
    newl[count + 1]   = NULL;
    for(i = offset = 0; oldvarval && oldvarval[i]; i++){
	if(i == insert)
	  offset = 1;

	newl[i + offset] = cpystr(oldvarval[i]);
    }

    alval = ALVAL(var, ew);
    if(alval){
	free_list_array(alval);
	if(newl){
	    for(i = 0; newl[i] ; i++)	/* count elements */
	      ;

	    *alval = (char **) fs_get((i+1) * sizeof(char *));

	    for(i = 0; newl[i] ; i++)
	      (*alval)[i] = cpystr(newl[i]);

	    (*alval)[i]         = NULL;
	}
    }

    free_list_array(&newl);
    return(1);
}


/*----------------------------------------------------------------------
   Function to display/manage collections

 ----*/
CONTEXT_S *
context_select_screen(ps, cs, ro_warn)
    struct pine *ps;
    CONT_SCR_S  *cs;
    int          ro_warn;
{
    CONTEXT_S	 *cp;
    CONF_S	 *ctmpa = NULL, *first_line = NULL, *heading;
    OPT_SCREEN_S  screen;
    int           readonly_warning = 0;

    /* restrict to normal config */
    ew = Main;

    if(!cs->edit)
      readonly_warning = 0;
    else if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(ro_warn && prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return(NULL);
	}
    }

    readonly_warning *= ro_warn;

    /*
     * Loop thru available contexts, setting up for display
     * (note: if no "cp" we're hosed.  should never happen ;)
     */
    for(cp = *cs->contexts; cp->prev; cp = cp->prev)
      ;
    
    /* delimiter for Mail Collections */
    new_confline(&ctmpa);		/* blank line */
    ctmpa->keymenu    = cs->keymenu;
    ctmpa->help       = cs->help.text;
    ctmpa->help_title = cs->help.title;
    ctmpa->tool       = context_select_tool;
    ctmpa->flags     |= CF_NOSELECT | CF_B_LINE;

    do{
	new_confline(&ctmpa);
	heading		  = ctmpa;
	ctmpa->value	  = cpystr(cp->nickname ? cp->nickname : cp->context);
	ctmpa->var	  = cp->var.v;
	ctmpa->keymenu    = cs->keymenu;
	ctmpa->help       = cs->help.text;
	ctmpa->help_title = cs->help.title;
	ctmpa->tool       = context_select_tool;
	ctmpa->flags	 |= CF_STARTITEM;
	ctmpa->valoffset  = 4;
	ctmpa->d.c.ct     = cp;
	ctmpa->d.c.cs	  = cs;

	if(!first_line || cp == cs->start)
	  first_line = ctmpa;

	/* Add explanatory text */
	new_confline(&ctmpa);
	ctmpa->value	  = cpystr(cp->label ? cp->label : "* * *");
	ctmpa->keymenu	  = cs->keymenu;
	ctmpa->help	  = cs->help.text;
	ctmpa->help_title = cs->help.title;
	ctmpa->tool	  = context_select_tool;
	ctmpa->flags	 |= CF_NOSELECT;
	ctmpa->valoffset  = 8;

	/* Always add blank line, make's shuffling a little easier */
	new_confline(&ctmpa);
	heading->headingp  = ctmpa;
	ctmpa->keymenu	   = cs->keymenu;
	ctmpa->help	   = cs->help.text;
	ctmpa->help_title  = cs->help.title;
	ctmpa->tool	   = context_select_tool;
	ctmpa->flags	  |= CF_NOSELECT | CF_B_LINE;
	ctmpa->valoffset   = 0;
    }
    while(cp = cp->next);


    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = readonly_warning;
    (void) conf_scroll_screen(ps, &screen, first_line, cs->title,
			      cs->print_string, 0);
    ps->mangled_screen = 1;
    return(cs->selected);
}


int
context_select_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    int retval = 0;

    switch(cmd){
      case MC_CHOICE :
	(*cl)->d.c.cs->selected = (*cl)->d.c.ct;
	retval = simple_exit_cmd(flags);
	break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      case MC_MAIN :
        retval = simple_exit_cmd(flags);
	ps_global->next_screen = main_menu_screen;
	break;

      case MC_INDEX :
	if(THREADING() && sp_viewing_a_thread(ps_global->mail_stream))
	  unview_thread(ps_global, ps_global->mail_stream, ps_global->msgmap);

	retval = simple_exit_cmd(flags);
	ps_global->next_screen = mail_index_screen;
	break;

      case MC_COMPOSE :
	retval = simple_exit_cmd(flags);
	ps_global->next_screen = compose_screen;
	break;

      case MC_ROLE :
	retval = simple_exit_cmd(flags);
	ps_global->next_screen = alt_compose_screen;
	break;

      case MC_GOTO :
        {
	    CONTEXT_S *c = (*cl)->d.c.ct;
	    char *new_fold = broach_folder(-FOOTER_ROWS(ps), 0, &c);

	    if(new_fold && do_broach_folder(new_fold, c, NULL, 0L) > 0){
		ps_global->next_screen = mail_index_screen;
		retval = simple_exit_cmd(flags);
	    }
	    else
	      ps->mangled_footer = 1;
        }

	break;

      case MC_QUIT :
	retval = simple_exit_cmd(flags);
	ps_global->next_screen = quit_screen;
	break;

      default:
	retval = -1;
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}



#ifdef	ENABLE_LDAP

static char *srch_res_help_title = "HELP FOR SEARCH RESULTS INDEX";
#define ADD_FIRST_LDAP_SERVER "Use Add to add a directory server"
#define ADDR_SELECT_EXIT_VAL 5
#define ADDR_SELECT_GOBACK_VAL 6
#define ADDR_SELECT_FORCED_EXIT_VAL 7

static struct key addr_select_keys[] = 
       {HELP_MENU,
        {"E", "ExitSelect", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
        {"S", "[Select]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(addr_s_km, addr_select_keys);

static struct key addr_select_with_goback_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"<", "AddressBkList", {MC_ADDRBOOK,2,{'<',','}}, KS_NONE},
        {"S", "[Select]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
        {"E", "ExitSelect", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	WHEREIS_MENU};
INST_KEY_MENU(addr_s_km_with_goback, addr_select_with_goback_keys);

static struct key addr_select_with_view_keys[] = 
       {HELP_MENU,
	RCOMPOSE_MENU,
        {"<", "AddressBkList", {MC_ADDRBOOK,2,{'<',','}}, KS_NONE},
        {">", "[View]",
	   {MC_VIEW_TEXT,5,{'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
        {"C", "ComposeTo", {MC_COMPOSE,1,{'c'}}, KS_COMPOSER},
	FWDEMAIL_MENU,
	SAVE_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(addr_s_km_with_view, addr_select_with_view_keys);

static struct key addr_select_for_url_keys[] = 
       {HELP_MENU,
	RCOMPOSE_MENU,
        {"<", "Exit Viewer", {MC_ADDRBOOK,3,{'<',',','e'}}, KS_NONE},
        {">", "[View]",
	   {MC_VIEW_TEXT,5,{'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
        {"C", "ComposeTo", {MC_COMPOSE,1,{'c'}}, KS_COMPOSER},
	FWDEMAIL_MENU,
	SAVE_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(addr_s_km_for_url, addr_select_for_url_keys);

static struct key addr_select_exit_keys[] = 
       {NULL_MENU,
	NULL_MENU,
        {"E", "[Exit]", {MC_EXIT,3,{'e',ctrl('M'),ctrl('J')}},
	   KS_EXITMODE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(addr_s_km_exit, addr_select_exit_keys);

static struct key addr_select_goback_keys[] = 
       {NULL_MENU,
	NULL_MENU,
        {"E", "[Exit]", {MC_ADDRBOOK,3,{'e',ctrl('M'),ctrl('J')}},
	   KS_EXITMODE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(addr_s_km_goback, addr_select_goback_keys);


static int some_selectable;
static char *dserv = "Directory Server on ";

/*
 * Let user choose an ldap entry (or return an entry if user doesn't need
 * to be consulted).
 *
 * Returns  0 if ok,
 *         -1 if Exit was chosen
 *         -2 if none were selectable
 *         -3 if no entries existed at all
 *         -4 go back to Abook List was chosen
 *
 *      When 0 is returned the winner is pointed to by result.
 *         Result is an allocated LDAP_SEARCH_WINNER_S which has pointers
 *         to the ld and entry that were chosen. Those are pointers into
 *         the initial data, not copies. The two-pointer structure is
 *         allocated here and freed by the caller.
 */
int
ldap_addr_select(ps, ac, result, style, wp_err)
    struct pine           *ps;
    ADDR_CHOOSE_S         *ac;
    LDAP_SERV_RES_S      **result;
    LDAPLookupStyle        style;
    WP_ERR_S              *wp_err;
{
    LDAPMessage     *e;
    LDAP_SERV_RES_S *res_list;
    CONF_S          *ctmpa = NULL, *first_line = NULL, *alt_first_line = NULL;
    int              i, retval = 0, got_n_mail = 0, got_n_entries = 0;
    int              need_mail;
    OPT_SCREEN_S     screen;
    struct key_menu *km;
    char             ee[200];
    HelpType         help;
    void           (*prev_redrawer) ();

    dprint(4,(debugfile, "ldap_addr_select()\n"));

    need_mail = (style == AlwaysDisplay || style == DisplayForURL) ? 0 : 1;
    if(style == AlwaysDisplay){
	km = &addr_s_km_with_view;
	help = h_address_display;
    }
    else if(style == AlwaysDisplayAndMailRequired){
	km = &addr_s_km_with_goback;
	help = h_address_select;
    }
    else if(style == DisplayForURL){
	km = &addr_s_km_for_url;
	help = h_address_display;
    }
    else{
	km = &addr_s_km;
	help = h_address_select;
    }

    if(result)
      *result = NULL;

    some_selectable = 0;

    for(res_list = ac->res_head; res_list; res_list = res_list->next){
	for(e = ldap_first_entry(res_list->ld, res_list->res);
	    e != NULL;
	    e = ldap_next_entry(res_list->ld, e)){
	    char       *dn, *a;
	    char      **cn, **org, **unit, **title, **mail, **sn;
	    BerElement *ber;
	    static char no_email[] = "<No Email Address Available>";
	    int         indent, have_mail;
	    
	    dn = NULL;
	    cn = org = title = unit = mail = sn = NULL;
	    for(a = ldap_first_attribute(res_list->ld, e, &ber);
		a != NULL;
		a = ldap_next_attribute(res_list->ld, e, ber)){

		dprint(9, (debugfile, " %s", a ? a : "?"));
		if(strcmp(a, res_list->info_used->cnattr) == 0){
		    if(!cn)
		      cn = ldap_get_values(res_list->ld, e, a);

		    if(cn && !(cn[0] && cn[0][0])){
			ldap_value_free(cn);
			cn = NULL;
		    }
		}
		else if(strcmp(a, res_list->info_used->mailattr) == 0){
		    if(!mail)
		      mail = ldap_get_values(res_list->ld, e, a);
		}
		else if(strcmp(a, "o") == 0){
		    if(!org)
		      org = ldap_get_values(res_list->ld, e, a);
		}
		else if(strcmp(a, "ou") == 0){
		    if(!unit)
		      unit = ldap_get_values(res_list->ld, e, a);
		}
		else if(strcmp(a, "title") == 0){
		    if(!title)
		      title = ldap_get_values(res_list->ld, e, a);
		}

		our_ldap_memfree(a);
	    }

	    dprint(9, (debugfile, "\n"));

	    if(!cn){
		for(a = ldap_first_attribute(res_list->ld, e, &ber);
		    a != NULL;
		    a = ldap_next_attribute(res_list->ld, e, ber)){

		    if(strcmp(a,  res_list->info_used->snattr) == 0){
			if(!sn)
			  sn = ldap_get_values(res_list->ld, e, a);

			if(sn && !(sn[0] && sn[0][0])){
			    ldap_value_free(sn);
			    sn = NULL;
			}
		    }

		    our_ldap_memfree(a);
		}
	    }

	    if(mail && mail[0] && mail[0][0])
	      have_mail = 1;
	    else
	      have_mail = 0;

	    got_n_mail += have_mail;
	    got_n_entries++;
	    indent = 2;

	    /*
	     * First line is either cn, sn, or dn.
	     */
	    if(cn){
		new_confline(&ctmpa);
		if(!alt_first_line)
		  alt_first_line = ctmpa;

		ctmpa->flags     |= CF_STARTITEM;
		if(need_mail && !have_mail)
		  ctmpa->flags     |= CF_PRIVATE;

		ctmpa->value      = cpystr(cn[0]);
		ldap_value_free(cn);
		ctmpa->valoffset  = indent;
		ctmpa->keymenu    = km;
		ctmpa->help       = help;
		ctmpa->help_title = srch_res_help_title;
		ctmpa->tool       = addr_select_tool;
		ctmpa->d.a.ld     = res_list->ld;
		ctmpa->d.a.res    = e;
		ctmpa->d.a.info_used = res_list->info_used;
		ctmpa->d.a.serv   = res_list->serv;
		ctmpa->d.a.ac     = ac;
		if(!first_line && (have_mail || !need_mail))
		  first_line = ctmpa;
	    }
	    
	    /* only happens if no cn */
	    if(sn){
		new_confline(&ctmpa);
		if(!alt_first_line)
		  alt_first_line = ctmpa;

		ctmpa->flags     |= CF_STARTITEM;
		if(need_mail && !have_mail)
		  ctmpa->flags     |= CF_PRIVATE;

		ctmpa->value      = cpystr(sn[0]);
		ldap_value_free(sn);
		ctmpa->valoffset  = indent;
		ctmpa->keymenu    = km;
		ctmpa->help       = help;
		ctmpa->help_title = srch_res_help_title;
		ctmpa->tool       = addr_select_tool;
		ctmpa->d.a.ld     = res_list->ld;
		ctmpa->d.a.res    = e;
		ctmpa->d.a.info_used = res_list->info_used;
		ctmpa->d.a.serv   = res_list->serv;
		ctmpa->d.a.ac     = ac;
		if(!first_line && (have_mail || !need_mail))
		  first_line = ctmpa;
	    }

	    if(!sn && !cn){
		new_confline(&ctmpa);
		if(!alt_first_line)
		  alt_first_line = ctmpa;

		ctmpa->flags     |= CF_STARTITEM;
		if(need_mail && !have_mail)
		  ctmpa->flags     |= CF_PRIVATE;

		dn = ldap_get_dn(res_list->ld, e);

		if(dn && !dn[0]){
		    our_ldap_dn_memfree(dn);
		    dn = NULL;
		}

		ctmpa->value      = cpystr(dn ? dn : "?");
		if(dn)
		  our_ldap_dn_memfree(dn);

		ctmpa->valoffset  = indent;
		ctmpa->keymenu    = km;
		ctmpa->help       = help;
		ctmpa->help_title = srch_res_help_title;
		ctmpa->tool       = addr_select_tool;
		ctmpa->d.a.ld     = res_list->ld;
		ctmpa->d.a.res    = e;
		ctmpa->d.a.info_used = res_list->info_used;
		ctmpa->d.a.serv   = res_list->serv;
		ctmpa->d.a.ac     = ac;
		if(!first_line && (have_mail || !need_mail))
		  first_line = ctmpa;
	    }

	    if(title){
		for(i = 0; title[i] && title[i][0]; i++){
		    new_confline(&ctmpa);
		    ctmpa->flags     |= CF_NOSELECT;
		    ctmpa->valoffset  = indent + 2;
		    ctmpa->value      = cpystr(title[i]);
		    ctmpa->keymenu    = km;
		    ctmpa->help       = help;
		    ctmpa->help_title = srch_res_help_title;
		    ctmpa->tool       = addr_select_tool;
		    ctmpa->d.a.ld     = res_list->ld;
		    ctmpa->d.a.res    = e;
		    ctmpa->d.a.info_used = res_list->info_used;
		    ctmpa->d.a.serv   = res_list->serv;
		    ctmpa->d.a.ac     = ac;
		}

		ldap_value_free(title);
	    }

	    if(unit){
		for(i = 0; unit[i] && unit[i][0]; i++){
		    new_confline(&ctmpa);
		    ctmpa->flags     |= CF_NOSELECT;
		    ctmpa->valoffset  = indent + 2;
		    ctmpa->value      = cpystr(unit[i]);
		    ctmpa->keymenu    = km;
		    ctmpa->help       = help;
		    ctmpa->help_title = srch_res_help_title;
		    ctmpa->tool       = addr_select_tool;
		    ctmpa->d.a.ld     = res_list->ld;
		    ctmpa->d.a.res    = e;
		    ctmpa->d.a.info_used = res_list->info_used;
		    ctmpa->d.a.serv   = res_list->serv;
		    ctmpa->d.a.ac     = ac;
		}

		ldap_value_free(unit);
	    }

	    if(org){
		for(i = 0; org[i] && org[i][0]; i++){
		    new_confline(&ctmpa);
		    ctmpa->flags     |= CF_NOSELECT;
		    ctmpa->valoffset  = indent + 2;
		    ctmpa->value      = cpystr(org[i]);
		    ctmpa->keymenu    = km;
		    ctmpa->help       = help;
		    ctmpa->help_title = srch_res_help_title;
		    ctmpa->tool       = addr_select_tool;
		    ctmpa->d.a.ld     = res_list->ld;
		    ctmpa->d.a.res    = e;
		    ctmpa->d.a.info_used = res_list->info_used;
		    ctmpa->d.a.serv   = res_list->serv;
		    ctmpa->d.a.ac     = ac;
		}

		ldap_value_free(org);
	    }

	    if(have_mail){
		/* Don't show long list of email addresses. */
		if(!(mail[0] && mail[0][0]) ||
		   !(mail[1] && mail[1][0]) ||
		   !(mail[2] && mail[2][0]) ||
		   !(mail[3] && mail[3][0])){
		    for(i = 0; mail[i] && mail[i][0]; i++){
			new_confline(&ctmpa);
			ctmpa->flags     |= CF_NOSELECT;
			ctmpa->valoffset  = indent + 2;
			ctmpa->value      = cpystr(mail[i]);
			ctmpa->keymenu    = km;
			ctmpa->help       = help;
			ctmpa->help_title = srch_res_help_title;
			ctmpa->tool       = addr_select_tool;
			ctmpa->d.a.ld     = res_list->ld;
			ctmpa->d.a.res    = e;
			ctmpa->d.a.info_used = res_list->info_used;
			ctmpa->d.a.serv   = res_list->serv;
			ctmpa->d.a.ac     = ac;
		    }
		}
		else{
		    char tmp[200];

		    for(i = 4; mail[i] && mail[i][0]; i++)
		      ;
		    
		    new_confline(&ctmpa);
		    ctmpa->flags     |= CF_NOSELECT;
		    ctmpa->valoffset  = indent + 2;
		    sprintf(tmp, "(%d email addresses)", i);
		    ctmpa->value      = cpystr(tmp);
		    ctmpa->keymenu    = km;
		    ctmpa->help       = help;
		    ctmpa->help_title = srch_res_help_title;
		    ctmpa->tool       = addr_select_tool;
		    ctmpa->d.a.ld     = res_list->ld;
		    ctmpa->d.a.res    = e;
		    ctmpa->d.a.info_used = res_list->info_used;
		    ctmpa->d.a.serv   = res_list->serv;
		    ctmpa->d.a.ac     = ac;
		}
	    }
	    else{
		new_confline(&ctmpa);
		ctmpa->flags     |= CF_NOSELECT;
		ctmpa->valoffset  = indent + 2;
		ctmpa->value      = cpystr(no_email);
		ctmpa->keymenu    = km;
		ctmpa->help       = help;
		ctmpa->help_title = srch_res_help_title;
		ctmpa->tool       = addr_select_tool;
		ctmpa->d.a.ld     = res_list->ld;
		ctmpa->d.a.res    = e;
		ctmpa->d.a.info_used = res_list->info_used;
		ctmpa->d.a.serv   = res_list->serv;
		ctmpa->d.a.ac     = ac;
	    }

	    if(mail)
	      ldap_value_free(mail);

	    new_confline(&ctmpa);		/* blank line */
	    ctmpa->keymenu    = km;
	    ctmpa->help       = help;
	    ctmpa->help_title = srch_res_help_title;
	    ctmpa->tool       = addr_select_tool;
	    ctmpa->flags     |= CF_NOSELECT | CF_B_LINE;
	}
    }

    if(first_line)
      some_selectable++;
    else if(alt_first_line)
      first_line = alt_first_line;
    else{
	new_confline(&ctmpa);		/* blank line */
	ctmpa->keymenu    = need_mail ? &addr_s_km_exit : &addr_s_km_goback;
	ctmpa->help       = help;
	ctmpa->help_title = srch_res_help_title;
	ctmpa->tool       = addr_select_tool;
	ctmpa->flags     |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmpa);
	first_line = ctmpa;
	strncpy(ee, "[ ", 3);
	if(wp_err && wp_err->ldap_errno)
	  sprintf(ee+2, "%.100s, No Matches Returned",
		  ldap_err2string(wp_err->ldap_errno));
	else
	    strncat(ee, "No Matches", 100);

	strncat(ee, " -- Choose Exit ]", 50);
	ctmpa->value      = cpystr(ee);
	ctmpa->valoffset  = 10;
	ctmpa->keymenu    = need_mail ? &addr_s_km_exit : &addr_s_km_goback;
	ctmpa->help       = help;
	ctmpa->help_title = srch_res_help_title;
	ctmpa->tool       = addr_select_tool;
	ctmpa->flags     |= CF_NOSELECT;
    }

    if(style == AlwaysDisplay || style == DisplayForURL ||
       style == AlwaysDisplayAndMailRequired ||
       (style == DisplayIfOne && got_n_mail >= 1) ||
       (style == DisplayIfTwo && got_n_mail >= 1 && got_n_entries >= 2)){
	TITLEBAR_STATE_S *tbstate = NULL;

	if(wp_err && wp_err->mangled)
	  *wp_err->mangled = 1;

	prev_redrawer = ps_global->redrawer;
	tbstate = save_titlebar_state();

	memset(&screen, 0, sizeof(screen));
	switch(conf_scroll_screen(ps,&screen,first_line,ac->title,"this ",0)){
	  case ADDR_SELECT_EXIT_VAL:
	    retval = -1;
	    break;

	  case ADDR_SELECT_GOBACK_VAL:
	    retval = -4;
	    break;

	  case ADDR_SELECT_FORCED_EXIT_VAL:
	    if(alt_first_line)	/* some entries, but none suitable */
	      retval = -2;
	    else
	      retval = -3;

	    break;

	  default:
	    retval = 0;
	    break;
	}

	ClearScreen();
	restore_titlebar_state(tbstate);
	free_titlebar_state(&tbstate);
	redraw_titlebar();
	if(ps_global->redrawer = prev_redrawer)
	  (*ps_global->redrawer)();

	if(result && retval == 0 && ac->selected_ld && ac->selected_entry){
	    (*result) = (LDAP_SERV_RES_S *)fs_get(sizeof(LDAP_SERV_RES_S));
	    (*result)->ld    = ac->selected_ld;
	    (*result)->res   = ac->selected_entry;
	    (*result)->info_used = ac->info_used;
	    (*result)->serv  = ac->selected_serv;
	    (*result)->next  = NULL;
	}
    }
    else if(style == DisplayIfOne && got_n_mail < 1){
	if(alt_first_line)	/* some entries, but none suitable */
	  retval = -2;
	else
	  retval = -3;

	first_line = first_confline(ctmpa);
	free_conflines(&first_line);
    }
    else if(style == DisplayIfTwo && (got_n_mail < 1 || got_n_entries < 2)){
	if(got_n_mail < 1){
	    if(alt_first_line)	/* some entries, but none suitable */
	      retval = -2;
	    else
	      retval = -3;
	}
	else{
	    retval = 0;
	    if(result){
		(*result) = (LDAP_SERV_RES_S *)fs_get(sizeof(LDAP_SERV_RES_S));
		(*result)->ld    = first_line->d.a.ld;
		(*result)->res   = first_line->d.a.res;
		(*result)->info_used = first_line->d.a.info_used;
		(*result)->serv  = first_line->d.a.serv;
		(*result)->next  = NULL;
	    }
	}

	first_line = first_confline(ctmpa);
	free_conflines(&first_line);
    }

    return(retval);
}


int
addr_select_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    int retval = 0;

    switch(cmd){
      case MC_CHOICE :
	if(flags & CF_PRIVATE){
	    q_status_message(SM_ORDER | SM_DING, 0, 3,
     "No email address available for this entry; choose another or ExitSelect");
	}
	else if(some_selectable){
	    (*cl)->d.a.ac->selected_ld    = (*cl)->d.a.ld;
	    (*cl)->d.a.ac->selected_entry = (*cl)->d.a.res;
	    (*cl)->d.a.ac->info_used      = (*cl)->d.a.info_used;
	    (*cl)->d.a.ac->selected_serv  = (*cl)->d.a.serv;
	    retval = simple_exit_cmd(flags);
	}
	else
	  retval = ADDR_SELECT_FORCED_EXIT_VAL;

	break;

      case MC_VIEW_TEXT :
      case MC_SAVE :
      case MC_FWDTEXT :
      case MC_COMPOSE :
      case MC_ROLE :
	{LDAP_SERV_RES_S *e;

	  if((*cl)->d.a.ld && (*cl)->d.a.res){
	    e = (LDAP_SERV_RES_S *)fs_get(sizeof(LDAP_SERV_RES_S));
	    e->ld    = (*cl)->d.a.ld;
	    e->res   = (*cl)->d.a.res;
	    e->info_used = (*cl)->d.a.info_used;
	    e->serv  = (*cl)->d.a.serv;
	    e->next  = NULL;
	    if(cmd == MC_VIEW_TEXT)
	      view_ldap_entry(ps, e);
	    else if(cmd == MC_SAVE)
	      save_ldap_entry(ps, e, 0);
	    else if(cmd == MC_COMPOSE)
	      compose_to_ldap_entry(ps, e, 0);
	    else if(cmd == MC_ROLE)
	      compose_to_ldap_entry(ps, e, 1);
	    else
	      forward_ldap_entry(ps, e);

	    fs_give((void **)&e);
	  }
	}

	break;

      case MC_ADDRBOOK :
        retval = ADDR_SELECT_GOBACK_VAL;
	break;

      case MC_EXIT :
        retval = ADDR_SELECT_EXIT_VAL;
	break;

      default:
	retval = -1;
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}


static struct key direct_config_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", "Exit Setup", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"C", "[Change]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	{"P", "PrevDir", {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", "NextDir", {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Dir", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Del Dir", {MC_DELETE,1,{'d'}}, KS_NONE},
	{"$", "Shuffle", {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	WHEREIS_MENU};
INST_KEY_MENU(dir_conf_km, direct_config_keys);


void
dir_init_display(ps, ctmp, servers, var, first_line)
    struct pine     *ps;
    CONF_S         **ctmp;
    char           **servers;
    struct variable *var;
    CONF_S         **first_line;
{
    int   i;
    char *serv;
    char *subtitle;
    LDAP_SERV_S *info;

    if(first_line)
      *first_line = NULL;

    if(servers && servers[0] && servers[0][0]){
	for(i = 0; servers[i]; i++){
	    info = break_up_ldap_server(servers[i]);
	    serv = (info && info->nick && *info->nick) ? cpystr(info->nick) :
		     (info && info->serv && *info->serv) ? cpystr(info->serv) :
		       cpystr("Bad Server Config, Delete this");
	    subtitle = (char *)fs_get((((info && info->serv && *info->serv)
					    ? strlen(info->serv)
					    : 3) +
					       strlen(dserv) + 15) *
					 sizeof(char));
	    if(info && info->port >= 0)
	      sprintf(subtitle, "%s%s:%d",
		      dserv,
		      (info && info->serv && *info->serv) ? info->serv : "<?>",
		      info->port);
	    else
	      sprintf(subtitle, "%s%s",
		      dserv,
		      (info && info->serv && *info->serv) ? info->serv : "<?>");

	    add_ldap_server_to_display(ps, ctmp, serv, subtitle, var,
				       i, &dir_conf_km, h_direct_config,
				       dir_config_tool, 0,
				       (first_line && *first_line == NULL)
					  ? first_line
					  : NULL);

	    free_ldap_server_info(&info);
	}
    }
    else{
	add_ldap_fake_first_server(ps, ctmp, var,
				   &dir_conf_km, h_direct_config,
				   dir_config_tool);
	if(first_line)
	  *first_line = *ctmp;
    }
}


void
directory_config(ps, edit_exceptions)
    struct pine *ps;
    int          edit_exceptions;
{
    CONF_S   *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S  screen;
    int           no_ex, readonly_warning = 0;

    if(edit_exceptions){
	q_status_message(SM_ORDER, 3, 7,
			 "Exception Setup not implemented for directory");
	return;
    }

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;
    
    no_ex = (ps_global->ew_for_except_vars == Main);

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return;
	}
    }

    if(ps->fix_fixed_warning)
      offer_to_fix_pinerc(ps);

    dir_init_display(ps, &ctmp, no_ex ? ps->VAR_LDAP_SERVERS
				      : LVAL(&ps->vars[V_LDAP_SERVERS], ew),
		     &ps->vars[V_LDAP_SERVERS], &first_line);

    memset(&screen, 0, sizeof(screen));
    screen.deferred_ro_warning = readonly_warning;
    (void)conf_scroll_screen(ps, &screen, first_line,
			     "SETUP DIRECTORY SERVERS", "servers ", 0);
    ps->mangled_screen = 1;
}


int
dir_config_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    int first_one, rv = 0;

    first_one = (*cl)->value &&
		(strcmp((*cl)->value, ADD_FIRST_LDAP_SERVER) == 0);
    switch(cmd){
      case MC_DELETE :
	if(first_one)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
			   "Nothing to Delete, use Add");
	else
	  dir_config_del(ps, cl);

	break;

      case MC_ADD :
	if(!fixed_var((*cl)->var, NULL, "directory list"))
	  dir_config_add(ps, cl);

	break;

      case MC_EDIT :
	if(!fixed_var((*cl)->var, NULL, "directory list")){
	    if(first_one)
	      dir_config_add(ps, cl);
	    else
	      dir_config_edit(ps, cl);
	}

	break;

      case MC_SHUFFLE :
	if(!fixed_var((*cl)->var, NULL, "directory list")){
	    if(first_one)
	      q_status_message(SM_ORDER|SM_DING, 0, 3,
			       "Nothing to Shuffle, use Add");
	    else
	      dir_config_shuffle(ps, cl);
	}

	break;

      case MC_EXIT :
	rv = 2;
	break;

      default:
	rv = -1;
	break;
    }

    return(rv);
}


/*
 * Add LDAP directory entry
 */
void
dir_config_add(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    char *raw_server = NULL;
    LDAP_SERV_S *info = NULL;
    char **lval;
    int no_ex;

    no_ex = (ps_global->ew_for_except_vars == Main);

    if(dir_edit_screen(ps, NULL, "ADD A", &raw_server) == 1){

	info = break_up_ldap_server(raw_server);

	if(info && info->serv && *info->serv){
	    char  *subtitle;
	    int    i, cnt = 0;
	    char **new_list;
	    CONF_S *cp;

	    lval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
	    if(lval)
	      while(lval[cnt])
		cnt++;

	    /* catch the special "" case */
	    if(cnt == 0 ||
	       cnt == 1 && lval[0][0] == '\0'){
		new_list = (char **)fs_get((1 + 1) * sizeof(char *));
		new_list[0] = raw_server;
		new_list[1] = NULL;
	    }
	    else{
		/* add one for new value */
		cnt++;
		new_list = (char **)fs_get((cnt + 1) * sizeof(char *));

		for(i = 0; i < (*cl)->varmem; i++)
		  new_list[i] = cpystr(lval[i]);

		new_list[(*cl)->varmem] = raw_server;

		for(i = (*cl)->varmem; i < cnt; i++)
		  new_list[i+1] = cpystr(lval[i]);
	    }

	    raw_server = NULL;
	    set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
	    free_list_array(&new_list);
	    set_current_val((*cl)->var, TRUE, FALSE);
	    subtitle = (char *)fs_get((((info && info->serv && *info->serv)
					    ? strlen(info->serv)
					    : 3) +
					       strlen(dserv) + 15) *
					 sizeof(char));
	    if(info && info->port >= 0)
	      sprintf(subtitle, "%s%s:%d",
		      dserv,
		      (info && info->serv && *info->serv) ? info->serv : "<?>",
		      info->port);
	    else
	      sprintf(subtitle, "%s%s",
		      dserv,
		      (info && info->serv && *info->serv) ? info->serv : "<?>");

	    if(cnt < 2){			/* first one */
		struct variable *var;
		struct key_menu *keymenu;
		HelpType         help;
		int            (*tool)();

		var      = (*cl)->var;
		keymenu  = (*cl)->keymenu;
		help     = (*cl)->help;
		tool     = (*cl)->tool;
		*cl = first_confline(*cl);
		free_conflines(cl);
		add_ldap_server_to_display(ps, cl,
					   (info && info->nick && *info->nick)
					     ? cpystr(info->nick)
					     : cpystr(info->serv),
					   subtitle, var, 0, keymenu, help,
					   tool, 0, NULL);

		opt_screen->top_line = NULL;
	    }
	    else{
		/*
		 * Insert new server.
		 */
		add_ldap_server_to_display(ps, cl,
					   (info && info->nick && *info->nick)
					     ? cpystr(info->nick)
					     : cpystr(info->serv),
					   subtitle,
					   (*cl)->var,
					   (*cl)->varmem,
					   (*cl)->keymenu,
					   (*cl)->help,
					   (*cl)->tool,
					   1,
					   NULL);
		/* adjust the rest of the varmems */
		for(cp = (*cl)->next; cp; cp = cp->next)
		  cp->varmem++;
	    }

	    /* because add_ldap advanced cl to its third line */
	    (*cl) = (*cl)->prev->prev;
	    
	    fix_side_effects(ps, (*cl)->var, 0);
	    write_pinerc(ps, ew, WRP_NONE);
	}
	else
	  q_status_message(SM_ORDER, 0, 3, "Add cancelled, no server name");
    }

    free_ldap_server_info(&info);
    if(raw_server)
      fs_give((void **)&raw_server);
}


/*
 * Shuffle order of LDAP directory entries
 */
void
dir_config_shuffle(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    int      cnt, rv, current_num, new_num, i, j, deefault;
    char   **new_list, **lval;
    char     tmp[200];
    HelpType help;
    ESCKEY_S opts[3];
    CONF_S  *a, *b;
    int no_ex;

    no_ex = (ps_global->ew_for_except_vars == Main);

    /* how many are in our current list? */
    lval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
    for(cnt = 0; lval && lval[cnt]; cnt++)
      ;
    
    if(cnt < 2){
	q_status_message(SM_ORDER, 0, 3,
	 "Shuffle only makes sense when there is more than one server in list");
	return;
    }

    current_num = (*cl)->varmem;  /* variable number of highlighted directory */

    /* Move it up or down? */
    i = 0;
    opts[i].ch      = 'u';
    opts[i].rval    = 'u';
    opts[i].name    = "U";
    opts[i++].label = "Up";

    opts[i].ch      = 'd';
    opts[i].rval    = 'd';
    opts[i].name    = "D";
    opts[i++].label = "Down";

    opts[i].ch = -1;
    deefault = 'u';

    if(current_num == 0){			/* no up */
	opts[0].ch = -2;
	deefault = 'd';
    }
    else if(current_num == cnt - 1)		/* no down */
      opts[1].ch = -2;

    sprintf(tmp, "Shuffle \"%.100s\" %s%s%s ? ",
	    (*cl)->value,
	    (opts[0].ch != -2) ? "UP" : "",
	    (opts[0].ch != -2 && opts[1].ch != -2) ? " or " : "",
	    (opts[1].ch != -2) ? "DOWN" : "");
    help = (opts[0].ch == -2) ? h_dir_shuf_down
			      : (opts[1].ch == -2) ? h_dir_shuf_up
						   : h_dir_shuf;

    rv = radio_buttons(tmp, -FOOTER_ROWS(ps), opts, deefault, 'x',
		       help, RB_NORM, NULL);

    switch(rv){
      case 'x':
	cmd_cancelled("Shuffle");
	return;

      case 'u':
	new_num = current_num - 1;
	a = (*cl)->prev->prev->prev;
	b = *cl;
	break;

      case 'd':
	new_num = current_num + 1;
	a = *cl;
	b = (*cl)->next->next->next;
	break;
    }

    /* allocate space for new list */
    new_list = (char **)fs_get((cnt + 1) * sizeof(char *));

    /* fill in new_list */
    for(i = 0; i < cnt; i++){
	if(i == current_num)
	  j = new_num;
	else if (i == new_num)
	  j = current_num;
	else
	  j = i;

	/* notice this works even if we were using default */
	new_list[i] = cpystr(lval[j]);
    }

    new_list[i] = NULL;

    j = set_variable_list((*cl)->var - ps->vars, new_list, TRUE, ew);
    free_list_array(&new_list);
    if(j){
	q_status_message(SM_ORDER, 0, 3,
			 "Shuffle cancelled: couldn't save configuration file");
	set_current_val((*cl)->var, TRUE, FALSE);
	return;
    }

    set_current_val((*cl)->var, TRUE, FALSE);

    if(a == opt_screen->top_line)
      opt_screen->top_line = b;
    
    j = a->varmem;
    a->varmem = b->varmem;
    b->varmem = j;

    /*
     * Swap display lines. To start with, a is lower in list, b is higher.
     * The fact that there are 3 lines per entry is totally entangled in
     * the code.
     */
    a->next->next->next = b->next->next->next;
    if(b->next->next->next)
      b->next->next->next->prev = a->next->next;
    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    b->next->next->next = a;
    a->prev = b->next->next;

    ps->mangled_body = 1;
    write_pinerc(ps, ew, WRP_NONE);
}


/*
 * Edit LDAP directory entry
 */
void
dir_config_edit(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    char        *raw_server = NULL, **lval;
    LDAP_SERV_S *info;
    int no_ex;

    no_ex = (ps_global->ew_for_except_vars == Main);

    lval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
    info = break_up_ldap_server((lval && lval[(*cl)->varmem])
				    ? lval[(*cl)->varmem] : NULL);
    
    if(dir_edit_screen(ps, info, "CHANGE THIS", &raw_server) == 1){

	free_ldap_server_info(&info);
	info = break_up_ldap_server(raw_server);

	if(lval && lval[(*cl)->varmem] &&
	   strcmp(lval[(*cl)->varmem], raw_server) == 0)
	  q_status_message(SM_ORDER, 0, 3, "No change, cancelled");
	else if(!(info && info->serv && *info->serv))
	  q_status_message(SM_ORDER, 0, 3,
	      "Change cancelled, use Delete if you want to remove this server");
	else{
	    char  *subtitle;
	    int    i, cnt;
	    char **new_list;

	    for(cnt = 0; lval && lval[cnt]; cnt++)
	      ;

	    new_list = (char **)fs_get((cnt + 1) * sizeof(char *));

	    for(i = 0; i < (*cl)->varmem; i++)
	      new_list[i] = cpystr(lval[i]);

	    new_list[(*cl)->varmem] = raw_server;
	    raw_server = NULL;

	    for(i = (*cl)->varmem + 1; i < cnt; i++)
	      new_list[i] = cpystr(lval[i]);
	    
	    new_list[cnt] = NULL;
	    set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
	    free_list_array(&new_list);
	    set_current_val((*cl)->var, TRUE, FALSE);

	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    (*cl)->value = cpystr((info->nick && *info->nick) ? info->nick
							      : info->serv);

	    if((*cl)->next->value)
	      fs_give((void **)&(*cl)->next->value);

	    subtitle = (char *)fs_get((((info && info->serv && *info->serv)
					    ? strlen(info->serv)
					    : 3) +
					       strlen(dserv) + 15) *
					 sizeof(char));
	    if(info && info->port >= 0)
	      sprintf(subtitle, "%s%s:%d",
		      dserv,
		      (info && info->serv && *info->serv) ? info->serv : "<?>",
		      info->port);
	    else
	      sprintf(subtitle, "%s%s",
		      dserv,
		      (info && info->serv && *info->serv) ? info->serv : "<?>");
	    (*cl)->next->value = subtitle;

	    fix_side_effects(ps, (*cl)->var, 0);
	    write_pinerc(ps, ew, WRP_NONE);
	}
    }

    free_ldap_server_info(&info);
    if(raw_server)
      fs_give((void **)&raw_server);
}


#define   LDAP_F_IMPL  0
#define   LDAP_F_RHS   1
#define   LDAP_F_REF   2
#define   LDAP_F_NOSUB 3
#define   LDAP_F_LDAPV3OK 4
bitmap_t  ldap_option_list;
struct variable *ldap_srch_rule_ptr;

/*
 * Gives user screen to edit config values for ldap server.
 *
 * Args    ps  -- pine struct
 *         def -- default values to start with
 *       title -- part of title at top of screen
 *  raw_server -- This is the returned item, allocated here and freed by caller.
 *
 * Returns:  0 if no change
 *           1 if user requested a change
 *               (change is stored in raw_server and hasn't been acted upon yet)
 *          10 user says abort
 */
int
dir_edit_screen(ps, def, title, raw_server)
    struct pine  *ps;
    LDAP_SERV_S  *def;
    char         *title;
    char        **raw_server;
{
    OPT_SCREEN_S    screen, *saved_screen;
    CONF_S         *ctmp = NULL, *ctmpb, *first_line = NULL;
    char            tmp[MAXPATH+1], custom_scope[MAXPATH], **apval;
    int             rv, i, j, lv;
    NAMEVAL_S      *f;
    struct variable server_var, base_var, port_var, nick_var,
		    srch_type_var, srch_rule_var, time_var,
		    size_var, mailattr_var, cnattr_var,
		    snattr_var, gnattr_var, cust_var,
		    opt_var, *v, *varlist[20];
    char           *server = NULL, *base = NULL, *port = NULL, *nick = NULL,
		   *srch_type = NULL, *srch_rule = NULL, *ttime = NULL,
		   *ssize = NULL, *mailattr = NULL, *cnattr = NULL,
		   *snattr = NULL, *gnattr = NULL, *cust = NULL;

    /*
     * We edit by making a nested call to conf_scroll_screen.
     * We use some fake struct variables to get back the results in, and
     * so we can use the existing tools from the config screen.
     */

    custom_scope[0] = '\0';

    varlist[j = 0] = &server_var;
    varlist[++j] = &base_var;
    varlist[++j] = &port_var;
    varlist[++j] = &nick_var;
    varlist[++j] = &srch_type_var;
    varlist[++j] = &srch_rule_var;
    varlist[++j] = &time_var;
    varlist[++j] = &size_var;
    varlist[++j] = &mailattr_var;
    varlist[++j] = &cnattr_var;
    varlist[++j] = &snattr_var;
    varlist[++j] = &gnattr_var;
    varlist[++j] = &cust_var;
    varlist[++j] = &opt_var;
    varlist[++j] = NULL;
    for(j = 0; varlist[j]; j++)
      memset(varlist[j], 0, sizeof(struct variable));

    server_var.name       = cpystr("ldap-server");
    server_var.is_used    = 1;
    server_var.is_user    = 1;
    apval = APVAL(&server_var, ew);
    *apval = (def && def->serv && def->serv[0]) ? cpystr(def->serv) : NULL;
    set_current_val(&server_var, FALSE, FALSE);

    base_var.name       = cpystr("search-base");
    base_var.is_used    = 1;
    base_var.is_user    = 1;
    apval = APVAL(&base_var, ew);
    *apval = (def && def->base && def->base[0]) ? cpystr(def->base) : NULL;
    set_current_val(&base_var, FALSE, FALSE);

    port_var.name       = cpystr("port");
    port_var.is_used    = 1;
    port_var.is_user    = 1;
    if(def && def->port >= 0){
	apval = APVAL(&port_var, ew);
	*apval = cpystr(int2string(def->port));
    }

    port_var.global_val.p = cpystr(int2string(LDAP_PORT));
    set_current_val(&port_var, FALSE, FALSE);

    nick_var.name       = cpystr("nickname");
    nick_var.is_used    = 1;
    nick_var.is_user    = 1;
    apval = APVAL(&nick_var, ew);
    *apval = (def && def->nick && def->nick[0]) ? cpystr(def->nick) : NULL;
    set_current_val(&nick_var, FALSE, FALSE);

    srch_type_var.name       = cpystr("search-type");
    srch_type_var.is_used    = 1;
    srch_type_var.is_user    = 1;
    apval = APVAL(&srch_type_var, ew);
    *apval = (f=ldap_search_types(def ? def->type : -1))
			? cpystr(f->name) : NULL;
    srch_type_var.global_val.p =
	(f=ldap_search_types(DEF_LDAP_TYPE)) ? cpystr(f->name) : NULL;
    set_current_val(&srch_type_var, FALSE, FALSE);

    ldap_srch_rule_ptr = &srch_rule_var;	/* so radiobuttons can tell */
    srch_rule_var.name       = cpystr("search-rule");
    srch_rule_var.is_used    = 1;
    srch_rule_var.is_user    = 1;
    apval = APVAL(&srch_rule_var, ew);
    *apval = (f=ldap_search_rules(def ? def->srch : -1))
			? cpystr(f->name) : NULL;
    srch_rule_var.global_val.p =
	(f=ldap_search_rules(DEF_LDAP_SRCH)) ? cpystr(f->name) : NULL;
    set_current_val(&srch_rule_var, FALSE, FALSE);

    time_var.name       = cpystr("timelimit");
    time_var.is_used    = 1;
    time_var.is_user    = 1;
    if(def && def->time >= 0){
	apval = APVAL(&time_var, ew);
	*apval = cpystr(int2string(def->time));
    }

    time_var.global_val.p = cpystr(int2string(DEF_LDAP_TIME));
    set_current_val(&time_var, FALSE, FALSE);

    size_var.name       = cpystr("sizelimit");
    size_var.is_used    = 1;
    size_var.is_user    = 1;
    if(def && def->size >= 0){
	apval = APVAL(&size_var, ew);
	*apval = cpystr(int2string(def->size));
    }

    size_var.global_val.p = cpystr(int2string(DEF_LDAP_SIZE));
    set_current_val(&size_var, FALSE, FALSE);

    mailattr_var.name       = cpystr("email-attribute");
    mailattr_var.is_used    = 1;
    mailattr_var.is_user    = 1;
    apval = APVAL(&mailattr_var, ew);
    *apval = (def && def->mailattr && def->mailattr[0])
		    ? cpystr(def->mailattr) : NULL;
    mailattr_var.global_val.p = cpystr(DEF_LDAP_MAILATTR);
    set_current_val(&mailattr_var, FALSE, FALSE);

    cnattr_var.name       = cpystr("name-attribute");
    cnattr_var.is_used    = 1;
    cnattr_var.is_user    = 1;
    apval = APVAL(&cnattr_var, ew);
    *apval = (def && def->cnattr && def->cnattr[0])
		    ? cpystr(def->cnattr) : NULL;
    cnattr_var.global_val.p = cpystr(DEF_LDAP_CNATTR);
    set_current_val(&cnattr_var, FALSE, FALSE);

    snattr_var.name       = cpystr("surname-attribute");
    snattr_var.is_used    = 1;
    snattr_var.is_user    = 1;
    apval = APVAL(&snattr_var, ew);
    *apval = (def && def->snattr && def->snattr[0])
		    ? cpystr(def->snattr) : NULL;
    snattr_var.global_val.p = cpystr(DEF_LDAP_SNATTR);
    set_current_val(&snattr_var, FALSE, FALSE);

    gnattr_var.name       = cpystr("givenname-attribute");
    gnattr_var.is_used    = 1;
    gnattr_var.is_user    = 1;
    apval = APVAL(&gnattr_var, ew);
    *apval = (def && def->gnattr && def->gnattr[0])
		    ? cpystr(def->gnattr) : NULL;
    gnattr_var.global_val.p = cpystr(DEF_LDAP_GNATTR);
    set_current_val(&gnattr_var, FALSE, FALSE);

    cust_var.name       = cpystr("custom-search-filter");
    cust_var.is_used    = 1;
    cust_var.is_user    = 1;
    apval = APVAL(&cust_var, ew);
    *apval = (def && def->cust && def->cust[0]) ? cpystr(def->cust) : NULL;
    set_current_val(&cust_var, FALSE, FALSE);

    opt_var.name          = cpystr("Features");
    opt_var.is_used       = 1;
    opt_var.is_user       = 1;
    opt_var.is_list       = 1;
    clrbitmap(ldap_option_list);
    if(def && def->impl)
      setbitn(LDAP_F_IMPL, ldap_option_list);
    if(def && def->rhs)
      setbitn(LDAP_F_RHS, ldap_option_list);
    if(def && def->ref)
      setbitn(LDAP_F_REF, ldap_option_list);
    if(def && def->nosub)
      setbitn(LDAP_F_NOSUB, ldap_option_list);
    if(def && def->ldap_v3_ok)
      setbitn(LDAP_F_LDAPV3OK, ldap_option_list);

    /* save the old opt_screen before calling scroll screen again */
    saved_screen = opt_screen;

    /* Server */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR LDAP SERVER";
    ctmp->var       = &server_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_server;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", server_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    first_line = ctmp;

    /* Search Base */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR SERVER SEARCH BASE";
    ctmp->var       = &base_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_base;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", base_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Port */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR PORT NUMBER";
    ctmp->var       = &port_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_port;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", port_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->flags    |= CF_NUMBER;

    /* Nickname */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR SERVER NICKNAME";
    ctmp->var       = &nick_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_nick;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", nick_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* Options */
    new_confline(&ctmp);
    ctmp->var       = &opt_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    sprintf(tmp, "%-20.100s =", opt_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = ldap_checkbox_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Feature Name");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = ldap_checkbox_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("---  ----------------------");

    /*  find longest value's name */
    for(lv = 0, i = 0; f = ldap_feature_list(i); i++)
      if(lv < (j = strlen(f->name)))
	lv = j;
    
    lv = min(lv, 100);

    for(i = 0; f = ldap_feature_list(i); i++){
	new_confline(&ctmp);
	ctmp->var       = &opt_var;
	ctmp->help_title= "HELP FOR LDAP FEATURES";
	ctmp->varnamep  = ctmpb;
	ctmp->keymenu   = &config_checkbox_keymenu;
	switch(i){
	  case LDAP_F_IMPL:
	    ctmp->help      = h_config_ldap_opts_impl;
	    break;
	  case LDAP_F_RHS:
	    ctmp->help      = h_config_ldap_opts_rhs;
	    break;
	  case LDAP_F_REF:
	    ctmp->help      = h_config_ldap_opts_ref;
	    break;
	  case LDAP_F_NOSUB:
	    ctmp->help      = h_config_ldap_opts_nosub;
	    break;
	  case LDAP_F_LDAPV3OK:
	    ctmp->help      = h_config_ldap_opts_ldap_v3_ok;
	    break;
	}
	ctmp->tool      = ldap_checkbox_tool;
	ctmp->valoffset = 12;
	ctmp->varmem    = i;
	sprintf(tmp, "[%c]  %-*.*s", 
		bitnset(f->value, ldap_option_list) ? 'X' : ' ',
		lv, lv, f->name);
	ctmp->value     = cpystr(tmp);
    }

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* Search Type */
    new_confline(&ctmp);
    ctmp->var       = &srch_type_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    sprintf(tmp, "%-20.100s =", srch_type_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Rule Values");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = ldap_radiobutton_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("---  ----------------------");

    /* find longest value's name */
    for(lv = 0, i = 0; f = ldap_search_types(i); i++)
      if(lv < (j = strlen(f->name)))
	lv = j;
    
    lv = min(lv, 100);
    
    for(i = 0; f = ldap_search_types(i); i++){
	new_confline(&ctmp);
	ctmp->help_title= "HELP FOR SEARCH TYPE";
	ctmp->var       = &srch_type_var;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = h_config_ldap_searchtypes;
	ctmp->varmem    = i;
	ctmp->tool      = ldap_radiobutton_tool;
	ctmp->varnamep  = ctmpb;
	sprintf(tmp, "(%c)  %-*.*s", (((!def || def->type == -1) &&
				        f->value == DEF_LDAP_TYPE) ||
				      (def && f->value == def->type))
				         ? R_SELD : ' ',
		lv, lv, f->name);
	ctmp->value     = cpystr(tmp);
    }

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;
    ctmp->varname   = cpystr("");

    /* Search Rule */
    new_confline(&ctmp);
    ctmp->var       = &srch_rule_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    sprintf(tmp, "%-20.100s =", srch_rule_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    /* Search Rule */
    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Rule Values");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = ldap_radiobutton_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("---  ----------------------");

    /* find longest value's name */
    for(lv = 0, i = 0; f = ldap_search_rules(i); i++)
      if(lv < (j = strlen(f->name)))
	lv = j;
    
    lv = min(lv, 100);
    
    for(i = 0; f = ldap_search_rules(i); i++){
	new_confline(&ctmp);
	ctmp->help_title= "HELP FOR SEARCH RULE";
	ctmp->var       = &srch_rule_var;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = h_config_ldap_searchrules;
	ctmp->varmem    = i;
	ctmp->tool      = ldap_radiobutton_tool;
	ctmp->varnamep  = ctmpb;
	sprintf(tmp, "(%c)  %-*.*s", (((!def || def->srch == -1) &&
				        f->value == DEF_LDAP_SRCH) ||
				      (def && f->value == def->srch))
				         ? R_SELD : ' ',
		lv, lv, f->name);
	ctmp->value     = cpystr(tmp);
    }

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;
    ctmp->varname   = cpystr("");

    /* Email attribute name */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR EMAIL ATTRIBUTE NAME";
    ctmp->var       = &mailattr_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_email_attr;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", mailattr_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Name attribute name */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR NAME ATTRIBUTE NAME";
    ctmp->var       = &cnattr_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_cn_attr;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", cnattr_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Surname attribute name */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR SURNAME ATTRIBUTE NAME";
    ctmp->var       = &snattr_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_sn_attr;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", snattr_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Givenname attribute name */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR GIVEN NAME ATTRIBUTE NAME";
    ctmp->var       = &gnattr_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_gn_attr;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", gnattr_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;
    ctmp->varname   = cpystr("");

    /* Time limit */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR SERVER TIMELIMIT";
    ctmp->var       = &time_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_time;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", time_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->flags    |= CF_NUMBER;

    /* Size limit */
    new_confline(&ctmp);
    ctmp->var       = &size_var;
    ctmp->help_title= "HELP FOR SERVER SIZELIMIT";
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_size;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", size_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->flags    |= CF_NUMBER;

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* Custom Search Filter */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR CUSTOM SEARCH FILTER";
    ctmp->var       = &cust_var;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_cust;
    ctmp->tool      = dir_edit_tool;
    sprintf(tmp, "%-20.100s =", cust_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);


    sprintf(tmp, "%s DIRECTORY SERVER", title);
    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = saved_screen ? saved_screen->deferred_ro_warning : 0;
    rv = conf_scroll_screen(ps, &screen, first_line, tmp, "servers ", 0);

    /*
     * Now look at the fake variables and extract the information we
     * want from them.
     */

    if(rv == 1 && raw_server){
	char dir_tmp[2000], *p;
	int portval = -1, timeval = -1, sizeval = -1;

	apval = APVAL(&server_var, ew);
	server = *apval;
	*apval = NULL;

	apval = APVAL(&base_var, ew);
	base = *apval;
	*apval = NULL;

	apval = APVAL(&port_var, ew);
	port = *apval;
	*apval = NULL;

	apval = APVAL(&nick_var, ew);
	nick = *apval;
	*apval = NULL;

	apval = APVAL(&srch_type_var, ew);
	srch_type = *apval;
	*apval = NULL;

	apval = APVAL(&srch_rule_var, ew);
	srch_rule = *apval;
	*apval = NULL;

	apval = APVAL(&time_var, ew);
	ttime = *apval;
	*apval = NULL;

	apval = APVAL(&size_var, ew);
	ssize = *apval;
	*apval = NULL;

	apval = APVAL(&cust_var, ew);
	cust = *apval;
	*apval = NULL;

	apval = APVAL(&mailattr_var, ew);
	mailattr = *apval;
	*apval = NULL;

	apval = APVAL(&snattr_var, ew);
	snattr = *apval;
	*apval = NULL;

	apval = APVAL(&gnattr_var, ew);
	gnattr = *apval;
	*apval = NULL;

	apval = APVAL(&cnattr_var, ew);
	cnattr = *apval;
	*apval = NULL;

	if(server)
	  removing_leading_and_trailing_white_space(server);

	if(base){
	    removing_leading_and_trailing_white_space(base);
	    (void)removing_double_quotes(base);
	    p = add_backslash_escapes(base);
	    fs_give((void **)&base);
	    base = p;
	}

	if(port){
	    removing_leading_and_trailing_white_space(port);
	    if(*port)
	      portval = atoi(port);
	}
	
	if(nick){
	    removing_leading_and_trailing_white_space(nick);
	    (void)removing_double_quotes(nick);
	    p = add_backslash_escapes(nick);
	    fs_give((void **)&nick);
	    nick = p;
	}

	if(ttime){
	    removing_leading_and_trailing_white_space(ttime);
	    if(*ttime)
	      timeval = atoi(ttime);
	}
	
	if(ssize){
	    removing_leading_and_trailing_white_space(ssize);
	    if(*ssize)
	      sizeval = atoi(ssize);
	}
	
	if(cust){
	    removing_leading_and_trailing_white_space(cust);
	    p = add_backslash_escapes(cust);
	    fs_give((void **)&cust);
	    cust = p;
	}

	if(mailattr){
	    removing_leading_and_trailing_white_space(mailattr);
	    p = add_backslash_escapes(mailattr);
	    fs_give((void **)&mailattr);
	    mailattr = p;
	}

	if(snattr){
	    removing_leading_and_trailing_white_space(snattr);
	    p = add_backslash_escapes(snattr);
	    fs_give((void **)&snattr);
	    snattr = p;
	}

	if(gnattr){
	    removing_leading_and_trailing_white_space(gnattr);
	    p = add_backslash_escapes(gnattr);
	    fs_give((void **)&gnattr);
	    gnattr = p;
	}

	if(cnattr){
	    removing_leading_and_trailing_white_space(cnattr);
	    p = add_backslash_escapes(cnattr);
	    fs_give((void **)&cnattr);
	    cnattr = p;
	}

	/*
	 * Don't allow user to edit scope but if one is present then we
	 * leave it (so they could edit it by hand).
	 */
	if(def && def->scope != -1 && def->scope != DEF_LDAP_SCOPE){
	    NAMEVAL_S *v;

	    v = ldap_search_scope(def->scope);
	    if(v)
	      sprintf(custom_scope, "/scope=%.50s", v->name);
	}

	sprintf(dir_tmp, "%.100s%s%.100s \"/base=%.100s/impl=%d/rhs=%d/ref=%d/nosub=%d/ldap_v3_ok=%d/type=%.50s/srch=%.50s%.100s/time=%.50s/size=%.50s/cust=%.100s/nick=%.100s/matr=%.50s/catr=%.50s/satr=%.50s/gatr=%.50s\"",
		server ? server : "",
		(portval >= 0 && port && *port) ? ":" : "",
		(portval >= 0 && port && *port) ? port : "",
		base ? base : "",
		bitnset(LDAP_F_IMPL, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_RHS, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_REF, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_NOSUB, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_LDAPV3OK, ldap_option_list) ? 1 : 0,
		srch_type ? srch_type : "",
		srch_rule ? srch_rule : "",
		custom_scope,
		(timeval >= 0 && ttime && *ttime) ? ttime : "",
		(sizeval >= 0 && ssize && *ssize) ? ssize : "",
		cust ? cust : "",
		nick ? nick : "",
		mailattr ? mailattr : "",
		cnattr ? cnattr : "",
		snattr ? snattr : "",
		gnattr ? gnattr : "");
	
	*raw_server = cpystr(dir_tmp);
    }

    for(j = 0; varlist[j]; j++){
	v = varlist[j];
	if(v->current_val.p)
	  fs_give((void **)&v->current_val.p);
	if(v->global_val.p)
	  fs_give((void **)&v->global_val.p);
	if(v->main_user_val.p)
	  fs_give((void **)&v->main_user_val.p);
	if(v->post_user_val.p)
	  fs_give((void **)&v->post_user_val.p);
	if(v->name)
	  fs_give((void **)&v->name);
    }

    if(server)
      fs_give((void **)&server);
    if(base)
      fs_give((void **)&base);
    if(port)
      fs_give((void **)&port);
    if(nick)
      fs_give((void **)&nick);
    if(srch_type)
      fs_give((void **)&srch_type);
    if(srch_rule)
      fs_give((void **)&srch_rule);
    if(ttime)
      fs_give((void **)&ttime);
    if(ssize)
      fs_give((void **)&ssize);
    if(mailattr)
      fs_give((void **)&mailattr);
    if(cnattr)
      fs_give((void **)&cnattr);
    if(snattr)
      fs_give((void **)&snattr);
    if(gnattr)
      fs_give((void **)&gnattr);
    if(cust)
      fs_give((void **)&cust);

    opt_screen = saved_screen;
    ps->mangled_screen = 1;
    return(rv);
}


/*
 * Just calls text_tool except for intercepting MC_EXIT.
 */
int
dir_edit_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    if(cmd == MC_EXIT)
      return(config_exit_cmd(flags));
    else
      return(text_tool(ps, cmd, cl, flags));
}


/*
 * Delete LDAP directory entry
 */
void
dir_config_del(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    char    prompt[81];
    int     rv = 0, i;

    if(fixed_var((*cl)->var, NULL, NULL)){
	if((*cl)->var->post_user_val.l || (*cl)->var->main_user_val.l){
	    if(want_to("Delete (unused) directory servers ",
		       'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		rv = 1;
		delete_user_vals((*cl)->var);
	    }
	}
	else
	  q_status_message(SM_ORDER, 3, 3,
			   "Can't delete sys-admin defined value");
    }
    else{
	int cnt, ans = 0, no_ex;
	char  **new_list, **lval, **nelval;

	no_ex = (ps_global->ew_for_except_vars == Main);

	/* This can't happen, intercepted at caller by first_one case */
	nelval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
	lval = LVAL((*cl)->var, ew);
	if(lval && lval[0] && lval[0][0] == '\0')
	  ans = 'r';

	/* how many servers defined? */
	for(cnt = 0; nelval[cnt]; cnt++)
	  ;

	/*
	 * If using default and there is more than one in list, ask if user
	 * wants to ignore them all or delete just this one. If just this
	 * one, copy rest to user_val. If ignore all, copy "" to user_val
	 * to override.
	 */
	if(!lval && cnt > 1){
	    static ESCKEY_S opts[] = {
		{'i', 'i', "I", "Ignore All"},
		{'r', 'r', "R", "Remove One"},
		{-1, 0, NULL, NULL}};
	    ans = radio_buttons(
	"Ignore all default directory servers or just remove this one ? ",
				-FOOTER_ROWS(ps), opts, 'i', 'x',
				h_ab_del_dir_ignore, RB_NORM, NULL);
	}

	if(ans == 0)
	  sprintf(prompt, "Really delete %.10s \"%.30s\" from directory servers ",
		  ((*cl)->value && *(*cl)->value)
		      ? "server"
		      : "item",
		  ((*cl)->value && *(*cl)->value)
		      ? (*cl)->value
		      : int2string((*cl)->varmem + 1));
	

	ps->mangled_footer = 1;
	if(ans == 'i'){
	    rv = ps->mangled_body = 1;

	    /*
	     * Ignore all of default by adding an empty string. Make it
	     * look just like there are no servers defined.
	     */

	    new_list = (char **)fs_get((1 + 1) * sizeof(char *));
	    new_list[0] = cpystr("");
	    new_list[1] = NULL;
	    set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
	    free_list_array(&new_list);
	    *cl = first_confline(*cl);
	    free_conflines(cl);
	    opt_screen->top_line = NULL;

	    add_ldap_fake_first_server(ps, cl, &ps->vars[V_LDAP_SERVERS],
				       &dir_conf_km, h_direct_config,
				       dir_config_tool);
	}
	else if(ans == 'r' ||
	       (ans != 'x' &&
	        want_to(prompt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y')){
	    CONF_S *cp;
	    char    **servers;
	    int       move_top = 0, this_one, revert_to_default,
		      default_there_to_revert_to;

	    /*
	     * Remove one from current list.
	     */

	    rv = ps->mangled_body = 1;

	    this_one = (*cl)->varmem;

	    /* might have to re-adjust screen to see new current */
	    move_top = (this_one > 0) &&
		       (this_one == cnt - 1) &&
		       (((*cl)    == opt_screen->top_line) ||
		        ((*cl)->prev == opt_screen->top_line) ||
		        ((*cl)->prev->prev == opt_screen->top_line));

	    /*
	     * If this is last one and there is a default available, revert
	     * to it.
	     */
	    revert_to_default = ((cnt == 1) && lval);
	    if(cnt > 1){
		new_list = (char **)fs_get((cnt + 1) * sizeof(char *));
		for(i = 0; i < this_one; i++)
		  new_list[i] = cpystr(nelval[i]);

		for(i = this_one; i < cnt; i++)
		  new_list[i] = cpystr(nelval[i+1]);

		set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
		free_list_array(&new_list);
	    }
	    else if(revert_to_default){
		char ***alval;

		alval = ALVAL((*cl)->var, ew);
		if(alval && *alval)
		  free_list_array(alval);
	    }
	    else{
		/* cnt is one and we want to hide default */
		new_list = (char **)fs_get((1 + 1) * sizeof(char *));
		new_list[0] = cpystr("");
		new_list[1] = NULL;
		set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
		free_list_array(&new_list);
	    }
		
	    if(cnt == 1){	/* delete display line for this_one */
		if(revert_to_default){
		    servers = (*cl)->var->global_val.l;
		    default_there_to_revert_to = (servers != NULL);
		}

		*cl = first_confline(*cl);
		free_conflines(cl);
		opt_screen->top_line = NULL;
		if(revert_to_default && default_there_to_revert_to){
		    CONF_S   *first_line = NULL;

		    q_status_message(SM_ORDER, 0, 3,
				     "Reverting to default directory server");
		    dir_init_display(ps, cl, servers,
				     &ps->vars[V_LDAP_SERVERS], &first_line);
		    *cl = first_line;
		}
		else{
		    add_ldap_fake_first_server(ps, cl,
					       &ps->vars[V_LDAP_SERVERS],
					       &dir_conf_km, h_direct_config,
					       dir_config_tool);
		}
	    }
	    else if(this_one == cnt - 1){	/* deleted last one */
		/* back up and delete it */
		*cl = (*cl)->prev;
		free_conflines(&(*cl)->next);
		/* now back up to first line of this server */
		*cl = (*cl)->prev->prev;
		if(move_top)
		  opt_screen->top_line = *cl;
	    }
	    else{			/* deleted one out of the middle */
		if(*cl == opt_screen->top_line)
		  opt_screen->top_line = (*cl)->next->next->next;

		cp = *cl;
		*cl = (*cl)->next;	/* move to next line, then */
		snip_confline(&cp);	/* snip 1st deleted line   */
		cp = *cl;
		*cl = (*cl)->next;	/* move to next line, then */
		snip_confline(&cp);	/* snip 2nd deleted line   */
		cp = *cl;
		*cl = (*cl)->next;	/* move to next line, then */
		snip_confline(&cp);	/* snip 3rd deleted line   */
		/* adjust varmems */
		for(cp = *cl; cp; cp = cp->next)
		  cp->varmem--;
	    }
	}
	else
	  q_status_message(SM_ORDER, 0, 3, "Server not deleted");
    }

    if(rv == 1){
	set_current_val((*cl)->var, TRUE, FALSE);
	fix_side_effects(ps, (*cl)->var, 0);
	write_pinerc(ps, ew, WRP_NONE);
    }
}


/*
 * Utility routine to help set up display
 */
void
add_ldap_fake_first_server(ps, ctmp, var, km, help, tool)
    struct pine     *ps;
    CONF_S         **ctmp;
    struct variable *var;
    struct key_menu *km;
    HelpType	     help;
    int		   (*tool)();
{
    new_confline(ctmp);
    (*ctmp)->help_title= "HELP FOR DIRECTORY SERVER CONFIGURATION";
    (*ctmp)->value     = cpystr(ADD_FIRST_LDAP_SERVER);
    (*ctmp)->var       = var;
    (*ctmp)->varmem    = 0;
    (*ctmp)->keymenu   = km;
    (*ctmp)->help      = help;
    (*ctmp)->tool      = tool;
    (*ctmp)->valoffset = 2;
}


/*
 * Add an ldap server to the display list.
 *
 * Args  before -- Insert it before current, else append it after.
 */
void
add_ldap_server_to_display(ps, ctmp, serv, subtitle, var, member, km, help,
			   tool, before, first_line)
    struct pine     *ps;
    CONF_S         **ctmp;
    char            *serv;
    char            *subtitle;
    struct variable *var;
    int              member;
    struct key_menu *km;
    HelpType	     help;
    int		   (*tool)();
    int              before;
    CONF_S         **first_line;
{
    new_confline(ctmp);
    if(first_line)
      *first_line = *ctmp;

    if(before){
	/*
	 * New_confline appends ctmp after old current instead of inserting
	 * it, so we have to adjust. We have
	 *  <- a <-> b <-> p <-> c -> and want <- a <-> p <-> b <-> c ->
	 */

	CONF_S *a, *b, *c, *p;

	p = *ctmp;
	b = (*ctmp)->prev;
	c = (*ctmp)->next;
	a = b ? b->prev : NULL;
	if(a)
	  a->next = p;

	if(b){
	    b->prev = p;
	    b->next = c;
	}

	if(c)
	  c->prev = b;

	p->prev = a;
	p->next = b;
    }

    (*ctmp)->help_title= "HELP FOR DIRECTORY SERVER CONFIGURATION";
    (*ctmp)->value     = serv;
    (*ctmp)->var       = var;
    (*ctmp)->varmem    = member;
    (*ctmp)->keymenu   = km;
    (*ctmp)->help      = help;
    (*ctmp)->tool      = tool;
    (*ctmp)->flags    |= CF_STARTITEM;
    (*ctmp)->valoffset = 4;

    new_confline(ctmp);
    (*ctmp)->value     = subtitle;
    (*ctmp)->keymenu   = km;
    (*ctmp)->help      = help;
    (*ctmp)->tool      = tool;
    (*ctmp)->flags    |= CF_NOSELECT;
    (*ctmp)->valoffset = 8;

    new_confline(ctmp);
    (*ctmp)->keymenu   = km;
    (*ctmp)->help      = help;
    (*ctmp)->tool      = tool;
    (*ctmp)->flags    |= CF_NOSELECT | CF_B_LINE;
    (*ctmp)->valoffset = 0;
}


/*
 * ldap option list manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
ldap_checkbox_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S	**cl;
    unsigned      flags;
{
    int  rv = 0;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark option */
	rv = 1;
	toggle_ldap_option_bit(ps, (*cl)->varmem, (*cl)->var, (*cl)->value);
	break;

      case MC_EXIT:				 /* exit */
        rv = config_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


void
toggle_ldap_option_bit(ps, index, var, value)
    struct pine     *ps;
    int		     index;
    struct variable *var;
    char            *value;
{
    NAMEVAL_S  *f;
    char      **vp, *p;

    f  = ldap_feature_list(index);

    /* flip the bit */
    if(bitnset(f->value, ldap_option_list))
      clrbitn(f->value, ldap_option_list);
    else
      setbitn(f->value, ldap_option_list);

    if(value)
      value[1] = bitnset(f->value, ldap_option_list) ? 'X' : ' ';
}


NAMEVAL_S *
ldap_feature_list(index)
    int index;
{
    static NAMEVAL_S ldap_feat_list[] = {
	{"use-implicitly-from-composer",      NULL, LDAP_F_IMPL},
	{"lookup-addrbook-contents",          NULL, LDAP_F_RHS},
	{"save-search-criteria-not-result",   NULL, LDAP_F_REF},
	{"disable-ad-hoc-space-substitution", NULL, LDAP_F_NOSUB},
	{"allow-ldap-v3",                     NULL, LDAP_F_LDAPV3OK}
    };

    return((index >= 0 &&
	    index < (sizeof(ldap_feat_list)/sizeof(ldap_feat_list[0])))
		   ? &ldap_feat_list[index] : NULL);
}


NAMEVAL_S *
ldap_search_rules(index)
    int index;
{
    static NAMEVAL_S ldap_search_list[] = {
	{"contains",		NULL, LDAP_SRCH_CONTAINS},
	{"equals",		NULL, LDAP_SRCH_EQUALS},
	{"begins-with",		NULL, LDAP_SRCH_BEGINS},
	{"ends-with",		NULL, LDAP_SRCH_ENDS}
    };

    return((index >= 0 &&
	    index < (sizeof(ldap_search_list)/sizeof(ldap_search_list[0])))
		   ? &ldap_search_list[index] : NULL);
}


NAMEVAL_S *
ldap_search_types(index)
    int index;
{
    static NAMEVAL_S ldap_types_list[] = {
	{"name",				NULL, LDAP_TYPE_CN},
	{"surname",				NULL, LDAP_TYPE_SUR},
	{"givenname",				NULL, LDAP_TYPE_GIVEN},
	{"email",				NULL, LDAP_TYPE_EMAIL},
	{"name-or-email",			NULL, LDAP_TYPE_CN_EMAIL},
	{"surname-or-givenname",		NULL, LDAP_TYPE_SUR_GIVEN},
	{"sur-or-given-or-name-or-email",	NULL, LDAP_TYPE_SEVERAL}
    };

    return((index >= 0 &&
	    index < (sizeof(ldap_types_list)/sizeof(ldap_types_list[0])))
		   ? &ldap_types_list[index] : NULL);
}


NAMEVAL_S *
ldap_search_scope(index)
    int index;
{
    static NAMEVAL_S ldap_scope_list[] = {
	{"base",		NULL, LDAP_SCOPE_BASE},
	{"onelevel",		NULL, LDAP_SCOPE_ONELEVEL},
	{"subtree",		NULL, LDAP_SCOPE_SUBTREE}
    };

    return((index >= 0 &&
	    index < (sizeof(ldap_scope_list)/sizeof(ldap_scope_list[0])))
		   ? &ldap_scope_list[index] : NULL);
}


/*
 * simple radio-button style variable handler
 */
int
ldap_radiobutton_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int	       rv = 0;
    CONF_S    *ctmp;
    NAMEVAL_S *rule;
    char     **apval;

    switch(cmd){
      case MC_CHOICE :				/* set/unset feature */

	/* hunt backwards, turning off old values */
	for(ctmp = *cl; ctmp && !(ctmp->flags & CF_NOSELECT) && !ctmp->varname;
	    ctmp = prev_confline(ctmp))
	  ctmp->value[1] = ' ';

	/* hunt forwards, turning off old values */
	for(ctmp = *cl; ctmp && !(ctmp->flags & CF_NOSELECT) && !ctmp->varname;
	    ctmp = next_confline(ctmp))
	  ctmp->value[1] = ' ';

	/* turn on current value */
	(*cl)->value[1] = R_SELD;

	if((*cl)->var == ldap_srch_rule_ptr)
	  rule = ldap_search_rules((*cl)->varmem);
	else
	  rule = ldap_search_types((*cl)->varmem);

	apval = APVAL((*cl)->var, ew);
	if(apval && *apval)
	  fs_give((void **)apval);

	if(apval)
	  *apval = cpystr(rule->name);

	ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	rv = 1;

	break;

      case MC_EXIT:				/* exit */
	rv = config_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}
#endif	/* ENABLE_LDAP */


void
take_to_export(ps, lines_to_take)
    struct pine   *ps;
    LINES_TO_TAKE *lines_to_take;
{
    CONF_S        *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S   screen;
    LINES_TO_TAKE *li;
    char          *help_title = "HELP FOR TAKE EXPORT SCREEN";
    char          *p;
    ScreenMode     listmode = SingleMode;

    for(li = lines_to_take; li; li = li->next){

	new_confline(&ctmp);
	ctmp->flags        |= CF_STARTITEM;
	if(li->flags & LT_NOSELECT)
	  ctmp->flags      |= CF_NOSELECT;
	else if(!first_line)
	  first_line = ctmp;

	p = li->printval ? li->printval : "";

	if(ctmp->flags & CF_NOSELECT)
	  ctmp->value = cpystr(p);
	else{
	    /* 5 is for "[X]  " */
	    ctmp->value         = (char *)fs_get((strlen(p)+5+1)*sizeof(char));
	    sprintf(ctmp->value, "    %s", p);
	}

	/* this points to data, it doesn't have its own copy */
	ctmp->d.t.exportval = li->exportval ? li->exportval : NULL;
	ctmp->d.t.selected  = 0;
	ctmp->d.t.listmode  = &listmode;

	ctmp->tool          = take_export_tool;
	ctmp->help_title    = help_title;
	ctmp->help          = h_takeexport_screen;
	ctmp->keymenu       = &take_export_keymenu_sm;
    }

    if(!first_line)
      q_status_message(SM_ORDER, 3, 3, "No lines to export");
    else{
	memset(&screen, 0, sizeof(screen));
	conf_scroll_screen(ps, &screen, first_line, "Take Export", NULL, 0);
    }
}


int
take_export_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    CONF_S    *ctmp;
    int        retval = 0;
    int        some_selected = 0, something_to_export = 0;
    SourceType srctype;
    STORE_S   *srcstore = NULL;
    char      *prompt_msg;

    switch(cmd){
      case MC_TAKE :
	srctype = CharStar;
	if((srcstore = so_get(srctype, NULL, EDIT_ACCESS)) != NULL){
	    if(*(*cl)->d.t.listmode == SingleMode){
		some_selected++;
		if((*cl)->d.t.exportval && (*cl)->d.t.exportval[0]){
		    so_puts(srcstore, (*cl)->d.t.exportval);
		    so_puts(srcstore, "\n");
		    something_to_export++;
		    prompt_msg = "selection";
		}
	    }
	    else{
		/* go to first line */
		for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
		  ;
		
		for(; ctmp; ctmp = next_confline(ctmp))
		  if(!(ctmp->flags & CF_NOSELECT) && ctmp->d.t.selected){
		      some_selected++;
		      if(ctmp->d.t.exportval && ctmp->d.t.exportval[0]){
			  so_puts(srcstore, ctmp->d.t.exportval);
			  so_puts(srcstore, "\n");
			  something_to_export++;
			  prompt_msg = "selections";
		      }
		  }
	    }
	}
	  
	if(!srcstore)
	  q_status_message(SM_ORDER, 0, 3, "Error allocating space");
	else if(something_to_export)
	  simple_export(ps, so_text(srcstore), srctype, prompt_msg, NULL);
	else if(!some_selected && *(*cl)->d.t.listmode == ListMode)
	  q_status_message(SM_ORDER, 0, 3, "Use \"X\" to mark selections");
	else
	  q_status_message(SM_ORDER, 0, 3, "Nothing to export");

	if(srcstore)
	  so_give(&srcstore);

	break;

      case MC_LISTMODE :
        if(*(*cl)->d.t.listmode == SingleMode){
	    /*
	     * UnHide the checkboxes
	     */

	    *(*cl)->d.t.listmode = ListMode;

	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		  ctmp->value[0] = '[';
		  ctmp->value[1] = ctmp->d.t.selected ? 'X' : SPACE;
		  ctmp->value[2] = ']';
		  ctmp->keymenu  = &take_export_keymenu_lm;
	      }
	}
	else{
	    /*
	     * Hide the checkboxes
	     */

	    *(*cl)->d.t.listmode = SingleMode;

	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		  ctmp->value[0] = ctmp->value[1] = ctmp->value[2] = SPACE;
		  ctmp->keymenu  = &take_export_keymenu_sm;
	      }
	}

	ps->mangled_body = ps->mangled_footer = 1;
	break;

      case MC_CHOICE :
	if((*cl)->value[1] == 'X'){
	    (*cl)->d.t.selected = 0;
	    (*cl)->value[1] = SPACE;
	}
	else{
	    (*cl)->d.t.selected = 1;
	    (*cl)->value[1] = 'X';
	}

	ps->mangled_body = 1;
        break;

      case MC_SELALL :
	/* go to first line */
	for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	  ;
	
	for(; ctmp; ctmp = next_confline(ctmp)){
	    if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		ctmp->d.t.selected = 1;
		if(ctmp->value)
		  ctmp->value[1] = 'X';
	    }
	}

	ps->mangled_body = 1;
        break;

      case MC_UNSELALL :
	/* go to first line */
	for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	  ;
	
	for(; ctmp; ctmp = next_confline(ctmp)){
	    if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		ctmp->d.t.selected = 0;
		if(ctmp->value)
		  ctmp->value[1] = SPACE;
	    }
	}

	ps->mangled_body = 1;
        break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      default:
	retval = -1;
	break;
    }

    return(retval);
}


/*
 * Handles screen painting and motion.  Passes other commands to
 * custom tools.
 *
 * Tool return values:  Tools should return the following:
 *     0 nothing changed
 *    -1 unrecognized command
 *     1 something changed, conf_scroll_screen should remember that
 *     2 tells conf_scroll_screen to return with value 1 or 0 depending
 *       on whether or not it has previously gotten a 1 from some tool.
 *     3 tells conf_scroll_screen to return 1 (like 1 and 2 combined)
 *     ? Other tool-specific values can be used.  They will cause
 *       conf_scroll_screen to return that value.
 *
 * Return values:
 *     0 if nothing happened.  That is, a tool returned 2 and we hadn't
 *       previously noted a return of 1
 *     1 if something happened.  That is, a tool returned 2 and we had
 *       previously noted a return of 1
 *     ? Tool-returned value different from -1, 0, 1, or 2.  This is it.
 *
 * Special proviso: If first_line->flags has CF_CHANGES set on entry, then
 * that will cause things to behave like a change was made since entering
 * this function.
 */
int
conf_scroll_screen(ps, screen, start_line, title, pdesc, multicol)
    struct pine  *ps;
    OPT_SCREEN_S *screen;
    CONF_S       *start_line;
    char         *title;
    char	 *pdesc;
    int		  multicol;
{
    char	  tmp[MAXPATH+1];
    int		  cmd, i, j, ch = 'x', done = 0, changes = 0;
    int		  retval = 0;
    int		  km_popped = 0, stay_in_col = 0;
    struct	  key_menu  *km = NULL;
    CONF_S	 *ctmpa = NULL, *ctmpb = NULL;
    Pos           cursor_pos;
    OtherMenu	  what_keymenu = FirstMenu;
    void        (*prev_redrawer) ();

    dprint(7,(debugfile, "conf_scroll_screen()\n"));

    if(BODY_LINES(ps) < 1){
	q_status_message(SM_ORDER | SM_DING, 3, 3, "Screen too small");
	return(0);
    }

    if(screen && screen->ro_warning)
      q_status_message1(SM_ORDER, 1, 3,
			"%.200s can't change options or settings",
			ps_global->restricted ? "Pine demo"
					      : "Config file not changeable,");

    screen->current    = start_line;
    if(start_line && start_line->flags & CF_CHANGES)
      changes++;

    opt_screen	       = screen;
    ps->mangled_screen = 1;
    ps->redrawer       = option_screen_redrawer;

    while(!done){
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(ps);
		ps->mangled_body = 1;
	    }
	}

	if(ps->mangled_screen){
	    ps->mangled_header = 1;
	    ps->mangled_footer = 1;
	    ps->mangled_body   = 1;
	    ps->mangled_screen = 0;
	}

	/*----------- Check for new mail -----------*/
        if(new_mail(0, NM_TIMING(ch), NM_STATUS_MSG | NM_DEFER_SORT) >= 0)
          ps->mangled_header = 1;

	if(ps->mangled_header){
	    set_titlebar(title, ps->mail_stream,
			 ps->context_current,
			 ps->cur_folder, ps->msgmap, 1, FolderName, 0, 0, NULL);
	    ps->mangled_header = 0;
	}

	update_option_screen(ps, screen, &cursor_pos);

	if(F_OFF(F_SHOW_CURSOR, ps)){
	    cursor_pos.row = ps->ttyo->screen_rows - FOOTER_ROWS(ps);
	    cursor_pos.col = 0;
	}

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

	if(ps->mangled_footer || km != screen->current->keymenu){
	    bitmap_t	 bitmap;

	    setbitmap(bitmap);

	    ps->mangled_footer = 0;
	    km                 = screen->current->keymenu;

	    if(multicol &&
	       (F_OFF(F_ARROW_NAV, ps_global)) ||
	        F_ON(F_RELAXED_ARROW_NAV, ps_global)){
		menu_clear_binding(km, KEY_LEFT);
		menu_clear_binding(km, KEY_RIGHT);
		menu_clear_binding(km, KEY_UP);
		menu_clear_binding(km, KEY_DOWN);
		menu_add_binding(km, KEY_UP, MC_CHARUP);
		menu_add_binding(km, KEY_DOWN, MC_CHARDOWN);	
		menu_add_binding(km, KEY_LEFT, MC_PREVITEM);
		menu_add_binding(km, ctrl('B'), MC_PREVITEM);
		menu_add_binding(km, KEY_RIGHT, MC_NEXTITEM);
		menu_add_binding(km, ctrl('F'), MC_NEXTITEM);
	    }
	    else{
		menu_clear_binding(km, KEY_LEFT);
		menu_clear_binding(km, KEY_RIGHT);
		menu_clear_binding(km, KEY_UP);
		menu_clear_binding(km, KEY_DOWN);

		/*
		 * Fix up arrow nav mode if necessary...
		 */
		if(F_ON(F_ARROW_NAV, ps_global)){
		    int cmd;

		    if((cmd = menu_clear_binding(km, '<')) != MC_UNKNOWN){
			menu_add_binding(km, '<', cmd);
			menu_add_binding(km, KEY_LEFT, cmd);
		    }

		    if((cmd = menu_clear_binding(km, '>')) != MC_UNKNOWN){
			menu_add_binding(km, '>', cmd);
			menu_add_binding(km, KEY_RIGHT, cmd);
		    }

		    if((cmd = menu_clear_binding(km, 'p')) != MC_UNKNOWN){
			menu_add_binding(km, 'p', cmd);
			menu_add_binding(km, KEY_UP, cmd);
		    }

		    if((cmd = menu_clear_binding(km, 'n')) != MC_UNKNOWN){
			menu_add_binding(km, 'n', cmd);
			menu_add_binding(km, KEY_DOWN, cmd);
		    }
		}
	    }

	    if(km_popped){
		FOOTER_ROWS(ps) = 3;
		clearfooter(ps);
	    }

	    draw_keymenu(km, bitmap, ps->ttyo->screen_cols,
			 1-FOOTER_ROWS(ps), 0, what_keymenu);
	    what_keymenu = SameMenu;

	    if(km_popped){
		FOOTER_ROWS(ps) = 1;
		mark_keymenu_dirty();
	    }
	}

	MoveCursor(cursor_pos.row, cursor_pos.col);
#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);	/* prime the handler */
	register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
		       ps_global->ttyo->screen_rows -(FOOTER_ROWS(ps_global)+1),
		       ps_global->ttyo->screen_cols);
#endif
#ifdef	_WINDOWS
	mswin_setscrollcallback(config_scroll_callback);
#endif
        /*------ Read the command from the keyboard ----*/
	ch = read_command();
#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_setscrollcallback(NULL);
#endif

	cmd = menu_command(ch, km);

	if(km_popped)
	  switch(cmd){
	    case MC_NONE:
	    case MC_OTHER: 
	    case MC_RESIZE: 
	    case MC_REPAINT:
	      km_popped++;
	      break;
	    
	    default:
	      clearfooter(ps);
	      break;
	  }

	switch(cmd){
	  case MC_OTHER :
	    what_keymenu = NextMenu;
	    ps->mangled_footer = 1;
	    break;

	  case MC_HELP:					/* help! */
	    if(FOOTER_ROWS(ps) == 1 && km_popped == 0){
		km_popped = 2;
		ps->mangled_footer = 1;
		break;
	    }

	    if(screen->current->help != NO_HELP){
	        prev_redrawer = ps_global->redrawer;
		helper(screen->current->help,
		       (screen->current->help_title)
		         ? screen->current->help_title
		         : CONFIG_SCREEN_HELP_TITLE,
		       HLPD_SIMPLE);
		ps_global->redrawer = prev_redrawer;
		ps->mangled_screen = 1;
	    }
	    else
	      q_status_message(SM_ORDER,0,3,"No help yet.");

	    break;


	  case MC_NEXTITEM:			/* next list element */
	  case MC_CHARDOWN:
	    stay_in_col = 0;
	    if(screen->current->flags & CF_DOUBLEVAR){
		/* if going from col1 to col2, it's simple */
		if(!(screen->current->flags & CF_VAR2) && cmd == MC_NEXTITEM){
		    screen->current->flags |= CF_VAR2;
		    break;
		}

		/* otherwise we fall through to normal next */
		stay_in_col = (screen->current->flags & CF_VAR2 &&
			       cmd == MC_CHARDOWN);
		screen->current->flags &= ~CF_VAR2;
	    }

	    for(ctmpa = next_confline(screen->current), i = 1;
		ctmpa && (ctmpa->flags & CF_NOSELECT);
		ctmpa = next_confline(ctmpa), i++)
	      ;

	    if(ctmpa){
		screen->current = ctmpa;
		if(screen->current->flags & CF_DOUBLEVAR && stay_in_col)
		  screen->current->flags |= CF_VAR2;

		if(cmd == MC_CHARDOWN){
		    for(ctmpa = screen->top_line,
			j = BODY_LINES(ps) - 1 - HS_MARGIN(ps);
			j > 0 && ctmpa && ctmpa != screen->current;
			ctmpa = next_confline(ctmpa), j--)
		      ;

		    if(!j && ctmpa){
			for(i = 0;
			    ctmpa && ctmpa != screen->current;
			    ctmpa = next_confline(ctmpa), i++)
			  ;

			if(i)
			  config_scroll_up(i);
		    }
		}
	    }
	    else{
		/*
		 * Scroll screen a bit so we show the non-selectable
		 * lines at the bottom.
		 */

		/* set ctmpa to the bottom line on the screen */
		for(ctmpa = screen->top_line, j = BODY_LINES(ps) - 1;
		    j > 0 && ctmpa;
		    ctmpa = next_confline(ctmpa), j--)
		  ;

		i = 0;
		if(ctmpa){
		    for(ctmpa = next_confline(ctmpa);
			ctmpa &&
			(ctmpa->flags & (CF_NOSELECT | CF_B_LINE)) ==
								CF_NOSELECT;
			ctmpa = next_confline(ctmpa), i++)
		      ;
		}

		if(i)
		  config_scroll_up(i);
		else
		  q_status_message(SM_ORDER,0,1, "Already at end of screen");
	    }

	    break;

	  case MC_PREVITEM:			/* prev list element */
	  case MC_CHARUP:
	    stay_in_col = 0;
	    if(screen->current->flags & CF_DOUBLEVAR){
		if(screen->current->flags & CF_VAR2 && cmd == MC_PREVITEM){
		    screen->current->flags &= ~CF_VAR2;
		    break;
		}

		/* otherwise we fall through to normal prev */
		stay_in_col = (!(screen->current->flags & CF_VAR2) &&
			       cmd == MC_CHARUP);
		screen->current->flags &= ~CF_VAR2;
	    }
	    else if(cmd == MC_CHARUP)
	      stay_in_col = 1;

	    ctmpa = screen->current;
	    i = 0;
	    do
	      if(ctmpa == config_top_scroll(ps, screen->top_line))
		i = 1;
	      else if(i)
		i++;
	    while((ctmpa = prev_confline(ctmpa))
		  && (ctmpa->flags&CF_NOSELECT));

	    if(ctmpa){
		screen->current = ctmpa;
		if(screen->current->flags & CF_DOUBLEVAR && !stay_in_col)
		  screen->current->flags |= CF_VAR2;

		if((cmd == MC_CHARUP) && i)
		  config_scroll_down(i);
	    }
	    else
	      q_status_message(SM_ORDER, 0, 1,
			       "Already at start of screen");

	    break;

	  case MC_PAGEDN:			/* page forward */
	    screen->current->flags &= ~CF_VAR2;
	    for(ctmpa = screen->top_line, i = BODY_LINES(ps);
		i > 0 && ctmpa;
		ctmpb = ctmpa, ctmpa = next_confline(ctmpa), i--)
	      ;

	    if(ctmpa){			/* first line off bottom of screen */
		ctmpb = ctmpa;
		ps->mangled_body = 1;
		/* find first selectable line on next page */
		for(screen->top_line = ctmpa;
		    ctmpa && (ctmpa->flags & CF_NOSELECT);
		    ctmpa = next_confline(ctmpa))
		  ;
		
		/*
		 * No selectable lines on next page. Slide up to first
		 * selectable.
		 */
		if(!ctmpa){
		    for(ctmpa = prev_confline(ctmpb);
			ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = prev_confline(ctmpa))
		      ;
		    
		    if(ctmpa)
		      screen->top_line = ctmpa;
		}
	    }
	    else{  /* on last screen */
		/* just move current down to last entry on screen */
		if(ctmpb){		/* last line of data */
		    for(ctmpa = ctmpb, i = BODY_LINES(ps); 
			i > 0 && ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = prev_confline(ctmpa), i--)
		      ;

		    if(ctmpa == screen->current){
			q_status_message(SM_ORDER,0,1,
					 "Already at end of screen");
			goto no_down;
		    }

		    ps->mangled_body = 1;
		}
	    }

	    if(ctmpa)
	      screen->current = ctmpa;
no_down:
	    break;

	  case MC_PAGEUP:			/* page backward */
	    ps->mangled_body = 1;
	    screen->current->flags &= ~CF_VAR2;
	    if(!(ctmpa=prev_confline(screen->top_line)))
	      ctmpa = screen->current;

	    for(i = BODY_LINES(ps) - 1;
		i > 0 && prev_confline(ctmpa);
		i--, ctmpa = prev_confline(ctmpa))
	      ;

	    for(screen->top_line = ctmpa;
	        ctmpa && (ctmpa->flags & CF_NOSELECT);
		ctmpa = next_confline(ctmpa))
	      ;

	    if(ctmpa){
		if(ctmpa == screen->current){
		    /*
		     * We get to here if there was nothing selectable on
		     * the previous page. There still may be something
		     * selectable further back than the previous page,
		     * so look for that.
		     */
		    for(ctmpa = prev_confline(screen->top_line);
			ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = prev_confline(ctmpa))
		      ;

		    if(!ctmpa){
			ctmpa = screen->current;
			q_status_message(SM_ORDER, 0, 1,
					 "Already at start of screen");
		    }
		}

		screen->current = ctmpa;
	    }

	    break;

#ifdef MOUSE	    
	  case MC_MOUSE:
	    {   
		MOUSEPRESS mp;

		mouse_get_last (NULL, &mp);
		mp.row -= HEADER_ROWS(ps);
		ctmpa = screen->top_line;

		while (mp.row && ctmpa != NULL) {
		    --mp.row;
		    ctmpa = ctmpa->next;
		}

		if (ctmpa != NULL && !(ctmpa->flags & CF_NOSELECT)){
		    if(screen->current->flags & CF_DOUBLEVAR)
		      screen->current->flags &= ~CF_VAR2;

		    screen->current = ctmpa;

		    if(screen->current->flags & CF_DOUBLEVAR &&
		       mp.col >= screen->current->val2offset)
		      screen->current->flags |= CF_VAR2;

		    update_option_screen(ps, screen, &cursor_pos);

		    if(mp.button == M_BUTTON_LEFT && mp.doubleclick){
		       
			if(screen->current->tool){
			    unsigned flags;
			    int default_cmd;

			    flags  = screen->current->flags;
			    flags |= (changes ? CF_CHANGES : 0);

			    default_cmd = menu_command(ctrl('M'), km);
			    switch(i=(*screen->current->tool)(ps, default_cmd,
						     &screen->current, flags)){
			      case -1:
			      case 0:
				break;

			      case 1:
				changes = 1;
				break;

			      case 2:
				retval = changes;
				done++;
				break;

			      case 3:
				retval = 1;
				done++;
				break;

			      default:
				retval = i;
				done++;
				break;
			    }
			}
		    }
#ifdef	_WINDOWS
		    else if(mp.button == M_BUTTON_RIGHT) {
			MPopup other_popup[20];
			int    n = -1, cmd, i;
			struct key_menu *sckm = screen->current->keymenu; /* only for popup */

			if((cmd = menu_command(ctrl('M'), sckm)) != MC_UNKNOWN){
			    i = menu_binding_index(sckm, cmd);
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val   = ctrl('M');
			}
			else if((cmd = menu_command('>', sckm)) != MC_UNKNOWN){
			    i = menu_binding_index(sckm, cmd);
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val	= '>';
			}

			if(((i = menu_binding_index(sckm, MC_RGB1)) >= 0) ||
			   ((i = menu_binding_index(sckm, MC_RGB2)) >= 0)){
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val	=
							sckm->keys[i].bind.ch[0];
			}

			if((cmd = menu_command('<', sckm)) != MC_UNKNOWN){
			    i = menu_binding_index(sckm, cmd);
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val	= '<';
			}
			else if((i = menu_binding_index(sckm, MC_EXIT)) >= 0){
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val	=
							sckm->keys[i].bind.ch[0];
			}

			if((i = menu_binding_index(sckm, MC_HELP)) >= 0){
			    if(n > 0)
			      other_popup[++n].type = tSeparator;

			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val = sckm->keys[i].bind.ch[0];
			}

			if(n > 0){
			    other_popup[++n].type = tTail;
			    mswin_popup(other_popup);
			}
		    }
		}
		else if(mp.button == M_BUTTON_RIGHT) {
		    MPopup other_popup[20];
		    int    n = -1, cmd, i;
		    struct key_menu *sckm = screen->current->keymenu; /* only for popup */

		    if((cmd = menu_command('<', sckm)) != MC_UNKNOWN){
			i = menu_binding_index(sckm, cmd);
			other_popup[++n].type	    = tQueue;
			other_popup[n].label.style  = lNormal;
			other_popup[n].label.string = sckm->keys[i].label;
			other_popup[n].data.val	    = '<';
		    }
		    else if((i = menu_binding_index(sckm, MC_EXIT)) >= 0){
			other_popup[++n].type	    = tQueue;
			other_popup[n].label.style  = lNormal;
			other_popup[n].label.string = sckm->keys[i].label;
			other_popup[n].data.val	    = sckm->keys[i].bind.ch[0];
		    }

		    other_popup[++n].type = tTail;

		    if(n > 0)
		      mswin_popup(other_popup);
#endif
		}
	    }
	    break;
#endif

	  case MC_PRINTTXT:			/* print screen */
	    print_option_screen(screen, pdesc ? pdesc : "");
	    break;

	  case MC_WHEREIS:			/* whereis */
	    /*--- get string  ---*/
	    {int   rc, found = 0;
#define FOUND_IT       0x01
#define FOUND_CURRENT  0x02
#define FOUND_WRAPPED  0x04
#define FOUND_NOSELECT 0x08
#define FOUND_ABOVE    0x10
	     char *result = NULL, buf[64];
	     static char last[64];
	     HelpType help;
	     static ESCKEY_S ekey[] = {
		{0, 0, "", ""},
		{ctrl('Y'), 10, "^Y", "Top"},
		{ctrl('V'), 11, "^V", "Bottom"},
		{-1, 0, NULL, NULL}};

	     ps->mangled_footer = 1;
	     buf[0] = '\0';
	     sprintf(tmp, "Word to find %s%.64s%s: ",
		     (last[0]) ? "[" : "",
		     (last[0]) ? last : "",
		     (last[0]) ? "]" : "");
	     help = NO_HELP;
	     while(1){
		 int flags = OE_APPEND_CURRENT;

		 rc = optionally_enter(buf,-FOOTER_ROWS(ps),0,sizeof(buf),
					 tmp,ekey,help,&flags);
		 if(rc == 3)
		   help = help == NO_HELP ? h_config_whereis : NO_HELP;
		 else if(rc == 0 || rc == 1 || rc == 10 || rc == 11 || !buf[0]){
		     if(rc == 0 && !buf[0] && last[0])
		       strncpy(buf, last, 64);

		     break;
		 }
	     }

	     screen->current->flags &= ~CF_VAR2;
	     if(rc == 0 && buf[0]){
		 CONF_S *started_here;

		 ch   = KEY_DOWN;
		 ctmpa = screen->current;
		 /*
		  * Skip over the unselectable lines of this "item"
		  * before starting search so that we don't find the
		  * same one again.
		  */
		 while((ctmpb = next_confline(ctmpa)) &&
		       (ctmpb->flags & CF_NOSELECT) &&
		       !(ctmpb->flags & CF_STARTITEM))
		   ctmpa = ctmpb;

		 started_here = next_confline(ctmpa);
		 while(ctmpa = next_confline(ctmpa))
		   if(srchstr(ctmpa->varname, buf)
		      || srchstr(ctmpa->value, buf)){

		       found = FOUND_IT;
		       /*
			* If this line is not selectable, back up to the
			* previous selectable line, but not past the
			* start of this "entry".
			*/
		       if(ctmpa->flags & CF_NOSELECT)
			 found |= FOUND_NOSELECT;

		       while((ctmpa->flags & CF_NOSELECT) &&
			     !(ctmpa->flags & CF_STARTITEM) &&
			     (ctmpb = prev_confline(ctmpa)))
			 ctmpa = ctmpb;
		       
		       /*
			* If that isn't selectable, better search forward
			* for something that is.
			*/
		       while((ctmpa->flags & CF_NOSELECT) &&
			     (ctmpb = next_confline(ctmpa))){
			   ctmpa = ctmpb;
			   found |= FOUND_ABOVE;
		       }

		       /*
			* If that still isn't selectable, better search
			* backwards for something that is.
			*/
		       while((ctmpa->flags & CF_NOSELECT) &&
			     (ctmpb = prev_confline(ctmpa))){
			   ctmpa = ctmpb;
			   found &= ~FOUND_ABOVE;
		       }

		       break;
		   }

		 if(!found){
		     found = FOUND_WRAPPED;
		     ctmpa = first_confline(screen->current);

		     while(ctmpa != started_here)
		       if(srchstr(ctmpa->varname, buf)
			  || srchstr(ctmpa->value, buf)){

			   found |= FOUND_IT;
			   if(ctmpa->flags & CF_NOSELECT)
			       found |= FOUND_NOSELECT;

			   while((ctmpa->flags & CF_NOSELECT) &&
				 !(ctmpa->flags & CF_STARTITEM) &&
				 (ctmpb = prev_confline(ctmpa)))
			     ctmpa = ctmpb;

			   while((ctmpa->flags & CF_NOSELECT) &&
				 (ctmpb = next_confline(ctmpa))){
			       ctmpa = ctmpb;
			       found |= FOUND_ABOVE;
			   }

			   if(ctmpa == screen->current)
			     found |= FOUND_CURRENT;

			   break;
		       }
		       else
			 ctmpa = next_confline(ctmpa);
		 }
	     }
	     else if(rc == 10){
		 screen->current = first_confline(screen->current);
		 if(screen->current && screen->current->flags & CF_NOSELECT){
		    for(ctmpa = next_confline(screen->current);
			ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = next_confline(ctmpa))
		      ;
		    
		    if(ctmpa)
		      screen->current = ctmpa;
		 }

		 result = "Searched to top";
	     }
	     else if(rc == 11){
		 screen->current = last_confline(screen->current);
		 if(screen->current && screen->current->flags & CF_NOSELECT){
		    for(ctmpa = prev_confline(screen->current);
			ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = prev_confline(ctmpa))
		      ;
		    
		    if(ctmpa)
		      screen->current = ctmpa;
		 }
		 
		 result = "Searched to bottom";
	     }
	     else
	       result = "WhereIs cancelled";

	     if((found & FOUND_IT) && ctmpa){
		 strncpy(last, buf, 64);
		 result =
    (found & FOUND_CURRENT && found & FOUND_WRAPPED && found & FOUND_NOSELECT)
      ? "Current item contains the only match"
      : (found & FOUND_CURRENT && found & FOUND_WRAPPED)
	? "Current line contains the only match"
	: (found & FOUND_NOSELECT && found & FOUND_WRAPPED)
	  ? ((found & FOUND_ABOVE)
	       ? "Search wrapped: word found in text above current line"
	       : "Search wrapped: word found in text below current line")
	  : (found & FOUND_WRAPPED)
	    ? "Search wrapped to beginning: word found"
	    : (found & FOUND_NOSELECT)
	      ? ((found & FOUND_ABOVE)
		   ? "Word found in text above current line"
		   : "Word found in text below current line")
	      : "Word found";
		 screen->current = ctmpa;
	     }

	     q_status_message(SM_ORDER,0,3,result ? result : "Word not found");
	    }

	    break;

	  case MC_REPAINT:			/* redraw the display */
	  case MC_RESIZE:
	    ClearScreen();
	    ps->mangled_screen = 1;
	    break;

	  default:
	    if(screen && screen->ro_warning){
		if(cmd == MC_EXIT){
		    retval = 0;
		    done++;
		}
		else
		  q_status_message1(SM_ORDER|SM_DING, 1, 3,
		     "%.200s can't change options or settings",
		     ps_global->restricted ? "Pine demo"
					   : "Config file not changeable,");
	    }
	    else if(screen->current->tool){
		unsigned flags;

		flags  = screen->current->flags;
		flags |= (changes ? CF_CHANGES : 0);

		switch(i=(*screen->current->tool)(ps, cmd,
		    &screen->current, flags)){
		  case -1:
		    q_status_message2(SM_ORDER, 0, 2,
		      "Command \"%.200s\" not defined here.%.200s",
		      pretty_command(ch),
		      F_ON(F_BLANK_KEYMENU,ps) ? "" : "  See key menu below.");
		    break;

		  case 0:
		    break;

		  case 1:
		    changes = 1;
		    break;

		  case 2:
		    retval = changes;
		    done++;
		    break;

		  case 3:
		    retval = 1;
		    done++;
		    break;

		  default:
		    retval = i;
		    done++;
		    break;
		}
	    }

	    break;

	  case MC_NONE:				/* simple timeout */
	    break;
	}
    }

    screen->current = first_confline(screen->current);
    free_conflines(&screen->current);
    return(retval);
}


/*
 *
 */
void
config_scroll_up(n)
    long n;
{
    CONF_S *ctmp = opt_screen->top_line;
    int     cur_found = 0;

    if(n < 0)
      config_scroll_down(-n);
    else if(n){
      for(; n>0 && ctmp->next; n--){
	ctmp = next_confline(ctmp);
	if(prev_confline(ctmp) == opt_screen->current)
	  cur_found++;
      }

      opt_screen->top_line = ctmp;
      ps_global->mangled_body = 1;
      if(cur_found){
	for(ctmp = opt_screen->top_line;
	    ctmp && (ctmp->flags & CF_NOSELECT);
	    ctmp = next_confline(ctmp))
	  ;

	if(ctmp)
	  opt_screen->current = opt_screen->prev = ctmp;
	else {
	  while(opt_screen->top_line->flags & CF_NOSELECT)
	    opt_screen->top_line = prev_confline(opt_screen->top_line);
	  opt_screen->current = opt_screen->prev = opt_screen->top_line;
	}
      }
    }
}


/*
 * config_scroll_down -
 */
void
config_scroll_down(n)
    long n;
{
    CONF_S *ctmp = opt_screen->top_line, *last_sel = NULL;
    int     i;

    if(n < 0)
      config_scroll_up(-n);
    else if(n){
	for(; n>0 && ctmp->prev; n--)
	  ctmp = prev_confline(ctmp);

	opt_screen->top_line = ctmp;
	ps_global->mangled_body = 1;
	for(ctmp = opt_screen->top_line, i = BODY_LINES(ps_global);
	    i > 0 && ctmp && ctmp != opt_screen->current;
	    ctmp = next_confline(ctmp), i--)
	  if(!(ctmp->flags & CF_NOSELECT))
	    last_sel = ctmp;

	if(!i && last_sel)
	  opt_screen->current = opt_screen->prev = last_sel;
    }
}


/*
 * config_scroll_to_pos -
 */
void
config_scroll_to_pos(n)
    long n;
{
    CONF_S *ctmp;

    for(ctmp = first_confline(opt_screen->current);
	n && ctmp && ctmp != opt_screen->top_line;
	ctmp = next_confline(ctmp), n--)
      ;

    if(n == 0)
      while(ctmp && ctmp != opt_screen->top_line)
	if(ctmp = next_confline(ctmp))
	  n--;

    config_scroll_up(n);
}


/*
 * config_top_scroll - return pointer to the 
 */
CONF_S *
config_top_scroll(ps, topline)
    struct pine *ps;
    CONF_S *topline;
{
    int     i;
    CONF_S *ctmp;

    for(ctmp = topline, i = HS_MARGIN(ps);
	ctmp && i;
	ctmp = next_confline(ctmp), i--)
      ;

    return(ctmp ? ctmp : topline);
}


/*
 *
 */
HelpType
config_help(var, feature)
    int var, feature;
{
    switch(var){
      case V_FEATURE_LIST :
	return(feature_list_help(feature));
	break;

      case V_PERSONAL_NAME :
	return(h_config_pers_name);
      case V_USER_ID :
	return(h_config_user_id);
      case V_USER_DOMAIN :
	return(h_config_user_dom);
      case V_SMTP_SERVER :
	return(h_config_smtp_server);
      case V_NNTP_SERVER :
	return(h_config_nntp_server);
      case V_INBOX_PATH :
	return(h_config_inbox_path);
      case V_PRUNED_FOLDERS :
	return(h_config_pruned_folders);
      case V_DEFAULT_FCC :
	return(h_config_default_fcc);
      case V_DEFAULT_SAVE_FOLDER :
	return(h_config_def_save_folder);
      case V_POSTPONED_FOLDER :
	return(h_config_postponed_folder);
      case V_READ_MESSAGE_FOLDER :
	return(h_config_read_message_folder);
      case V_FORM_FOLDER :
	return(h_config_form_folder);
      case V_ARCHIVED_FOLDERS :
	return(h_config_archived_folders);
      case V_SIGNATURE_FILE :
	return(h_config_signature_file);
      case V_LITERAL_SIG :
	return(h_config_literal_sig);
      case V_INIT_CMD_LIST :
	return(h_config_init_cmd_list);
      case V_COMP_HDRS :
	return(h_config_comp_hdrs);
      case V_CUSTOM_HDRS :
	return(h_config_custom_hdrs);
      case V_VIEW_HEADERS :
	return(h_config_viewer_headers);
      case V_VIEW_MARGIN_LEFT :
	return(h_config_viewer_margin_left);
      case V_VIEW_MARGIN_RIGHT :
	return(h_config_viewer_margin_right);
      case V_QUOTE_SUPPRESSION :
	return(h_config_quote_suppression);
      case V_SAVED_MSG_NAME_RULE :
	return(h_config_saved_msg_name_rule);
      case V_FCC_RULE :
	return(h_config_fcc_rule);
      case V_SORT_KEY :
	return(h_config_sort_key);
      case V_AB_SORT_RULE :
	return(h_config_ab_sort_rule);
      case V_FLD_SORT_RULE :
	return(h_config_fld_sort_rule);
      case V_CHAR_SET :
	return(h_config_char_set);
      case V_EDITOR :
	return(h_config_editor);
      case V_SPELLER :
	return(h_config_speller);
      case V_DISPLAY_FILTERS :
	return(h_config_display_filters);
      case V_SEND_FILTER :
	return(h_config_sending_filter);
      case V_ALT_ADDRS :
	return(h_config_alt_addresses);
      case V_KEYWORDS :
	return(h_config_keywords);
      case V_KW_BRACES :
	return(h_config_kw_braces);
      case V_KW_COLORS :
	return(h_config_kw_color);
      case V_ABOOK_FORMATS :
	return(h_config_abook_formats);
      case V_INDEX_FORMAT :
	return(h_config_index_format);
      case V_OVERLAP :
	return(h_config_viewer_overlap);
      case V_MAXREMSTREAM :
	return(h_config_maxremstream);
      case V_PERMLOCKED :
	return(h_config_permlocked);
      case V_MARGIN :
	return(h_config_scroll_margin);
      case V_DEADLETS :
	return(h_config_deadlets);
      case V_FILLCOL :
	return(h_config_composer_wrap_column);
      case V_TCPOPENTIMEO :
	return(h_config_tcp_open_timeo);
      case V_TCPREADWARNTIMEO :
	return(h_config_tcp_readwarn_timeo);
      case V_TCPWRITEWARNTIMEO :
	return(h_config_tcp_writewarn_timeo);
      case V_TCPQUERYTIMEO :
	return(h_config_tcp_query_timeo);
      case V_RSHOPENTIMEO :
	return(h_config_rsh_open_timeo);
      case V_SSHOPENTIMEO :
	return(h_config_ssh_open_timeo);
      case V_USERINPUTTIMEO :
	return(h_config_user_input_timeo);
      case V_REMOTE_ABOOK_VALIDITY :
	return(h_config_remote_abook_validity);
      case V_REMOTE_ABOOK_HISTORY :
	return(h_config_remote_abook_history);
      case V_INCOMING_FOLDERS :
	return(h_config_incoming_folders);
      case V_FOLDER_SPEC :
	return(h_config_folder_spec);
      case V_NEWS_SPEC :
	return(h_config_news_spec);
      case V_ADDRESSBOOK :
	return(h_config_address_book);
      case V_GLOB_ADDRBOOK :
	return(h_config_glob_addrbook);
      case V_LAST_VERS_USED :
	return(h_config_last_vers);
      case V_SENDMAIL_PATH :
	return(h_config_sendmail_path);
      case V_OPER_DIR :
	return(h_config_oper_dir);
      case V_RSHPATH :
	return(h_config_rshpath);
      case V_RSHCMD :
	return(h_config_rshcmd);
      case V_SSHPATH :
	return(h_config_sshpath);
      case V_SSHCMD :
	return(h_config_sshcmd);
      case V_NEW_VER_QUELL :
	return(h_config_new_ver_quell);
      case V_DISABLE_DRIVERS :
	return(h_config_disable_drivers);
      case V_DISABLE_AUTHS :
	return(h_config_disable_auths);
      case V_REMOTE_ABOOK_METADATA :
	return(h_config_abook_metafile);
      case V_REPLY_STRING :
	return(h_config_reply_indent_string);
      case V_QUOTE_REPLACE_STRING :
	return(h_config_quote_replace_string);
      case V_REPLY_INTRO :
	return(h_config_reply_intro);
      case V_EMPTY_HDR_MSG :
	return(h_config_empty_hdr_msg);
      case V_STATUS_MSG_DELAY :
	return(h_config_status_msg_delay);
      case V_MAILCHECK :
	return(h_config_mailcheck);
      case V_MAILCHECKNONCURR :
	return(h_config_mailchecknoncurr);
      case V_MAILDROPCHECK :
	return(h_config_maildropcheck);
      case V_NNTPRANGE :
	return(h_config_nntprange);
      case V_NEWS_ACTIVE_PATH :
	return(h_config_news_active);
      case V_NEWS_SPOOL_DIR :
	return(h_config_news_spool);
      case V_IMAGE_VIEWER :
	return(h_config_image_viewer);
      case V_USE_ONLY_DOMAIN_NAME :
	return(h_config_domain_name);
      case V_LAST_TIME_PRUNE_QUESTION :
	return(h_config_prune_date);
      case V_UPLOAD_CMD:
	return(h_config_upload_cmd);
      case V_UPLOAD_CMD_PREFIX:
	return(h_config_upload_prefix);
      case V_DOWNLOAD_CMD:
	return(h_config_download_cmd);
      case V_DOWNLOAD_CMD_PREFIX:
	return(h_config_download_prefix);
      case V_GOTO_DEFAULT_RULE:
	return(h_config_goto_default);
      case V_INCOMING_STARTUP:
	return(h_config_inc_startup);
      case V_PRUNING_RULE:
	return(h_config_pruning_rule);
      case V_REOPEN_RULE:
	return(h_config_reopen_rule);
      case V_THREAD_DISP_STYLE:
	return(h_config_thread_disp_style);
      case V_THREAD_INDEX_STYLE:
	return(h_config_thread_index_style);
      case V_THREAD_MORE_CHAR:
	return(h_config_thread_indicator_char);
      case V_THREAD_EXP_CHAR:
	return(h_config_thread_exp_char);
      case V_THREAD_LASTREPLY_CHAR:
	return(h_config_thread_lastreply_char);
      case V_MAILCAP_PATH :
	return(h_config_mailcap_path);
      case V_MIMETYPE_PATH :
	return(h_config_mimetype_path);
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
      case V_FIFOPATH :
	return(h_config_fifopath);
#endif
      case V_NMW_WIDTH :
	return(h_config_newmailwidth);
      case V_NEWSRC_PATH :
	return(h_config_newsrc_path);
      case V_BROWSER :
	return(h_config_browser);
#if defined(DOS) || defined(OS2)
      case V_FILE_DIR :
	return(h_config_file_dir);
#endif
      case V_NORM_FORE_COLOR :
      case V_NORM_BACK_COLOR :
	return(h_config_normal_color);
      case V_REV_FORE_COLOR :
      case V_REV_BACK_COLOR :
	return(h_config_reverse_color);
      case V_TITLE_FORE_COLOR :
      case V_TITLE_BACK_COLOR :
	return(h_config_title_color);
      case V_STATUS_FORE_COLOR :
      case V_STATUS_BACK_COLOR :
	return(h_config_status_color);
      case V_SLCTBL_FORE_COLOR :
      case V_SLCTBL_BACK_COLOR :
	return(h_config_slctbl_color);
      case V_QUOTE1_FORE_COLOR :
      case V_QUOTE2_FORE_COLOR :
      case V_QUOTE3_FORE_COLOR :
      case V_QUOTE1_BACK_COLOR :
      case V_QUOTE2_BACK_COLOR :
      case V_QUOTE3_BACK_COLOR :
	return(h_config_quote_color);
      case V_SIGNATURE_FORE_COLOR :
      case V_SIGNATURE_BACK_COLOR :
	return(h_config_signature_color);
      case V_PROMPT_FORE_COLOR :
      case V_PROMPT_BACK_COLOR :
	return(h_config_prompt_color);
      case V_IND_PLUS_FORE_COLOR :
      case V_IND_IMP_FORE_COLOR :
      case V_IND_DEL_FORE_COLOR :
      case V_IND_ANS_FORE_COLOR :
      case V_IND_NEW_FORE_COLOR :
      case V_IND_UNS_FORE_COLOR :
      case V_IND_REC_FORE_COLOR :
      case V_IND_PLUS_BACK_COLOR :
      case V_IND_IMP_BACK_COLOR :
      case V_IND_DEL_BACK_COLOR :
      case V_IND_ANS_BACK_COLOR :
      case V_IND_NEW_BACK_COLOR :
      case V_IND_UNS_BACK_COLOR :
      case V_IND_REC_BACK_COLOR :
	return(h_config_index_color);
      case V_IND_ARR_FORE_COLOR :
      case V_IND_ARR_BACK_COLOR :
	return(h_config_arrow_color);
      case V_KEYLABEL_FORE_COLOR :
      case V_KEYLABEL_BACK_COLOR :
	return(h_config_keylabel_color);
      case V_KEYNAME_FORE_COLOR :
      case V_KEYNAME_BACK_COLOR :
	return(h_config_keyname_color);
      case V_VIEW_HDR_COLORS :
	return(h_config_customhdr_color);
      case V_PRINTER :
	return(h_config_printer);
      case V_PERSONAL_PRINT_CATEGORY :
	return(h_config_print_cat);
      case V_PERSONAL_PRINT_COMMAND :
	return(h_config_print_command);
      case V_PAT_ROLES :
	return(h_config_pat_roles);
      case V_PAT_FILTS :
	return(h_config_pat_filts);
      case V_PAT_SCORES :
	return(h_config_pat_scores);
      case V_PAT_INCOLS :
	return(h_config_pat_incols);
      case V_PAT_OTHER :
	return(h_config_pat_other);
      case V_INDEX_COLOR_STYLE :
	return(h_config_index_color_style);
      case V_TITLEBAR_COLOR_STYLE :
	return(h_config_titlebar_color_style);
#ifdef	_WINDOWS
      case V_FONT_NAME :
	return(h_config_font_name);
      case V_FONT_SIZE :
	return(h_config_font_size);
      case V_FONT_STYLE :
	return(h_config_font_style);
      case V_FONT_CHAR_SET :
	return(h_config_font_char_set);
      case V_PRINT_FONT_NAME :
	return(h_config_print_font_name);
      case V_PRINT_FONT_SIZE :
	return(h_config_print_font_size);
      case V_PRINT_FONT_STYLE :
	return(h_config_print_font_style);
      case V_PRINT_FONT_CHAR_SET :
	return(h_config_print_font_char_set);
      case V_WINDOW_POSITION :
	return(h_config_window_position);
      case V_CURSOR_STYLE :
	return(h_config_cursor_style);
#else
      case V_COLOR_STYLE :
	return(h_config_color_style);
#endif
#ifdef	ENABLE_LDAP
      case V_LDAP_SERVERS :
	return(h_config_ldap_servers);
#endif
      default :
	return(NO_HELP);
    }
}


int
litsig_text_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    char           **apval;
    int		     rv = 0;

    if(cmd != MC_EXIT && fixed_var((*cl)->var, NULL, NULL))
      return(rv);

    apval = APVAL((*cl)->var, ew);

    switch(cmd){
      case MC_ADD:
      case MC_EDIT :
	if(apval){
	    char *input = NULL, *result = NULL, *err = NULL, *cstring_version;
	    char *olddefval = NULL, *start_with;
	    size_t len;

	    if(!*apval && (*cl)->var->current_val.p &&
	       (*cl)->var->current_val.p[0]){
		if(!strncmp((*cl)->var->current_val.p,
			    DSTRING,
			    (len=strlen(DSTRING)))){
		    /* strip DSTRING and trailing paren */
		    olddefval = (char *)fs_get(strlen((*cl)->var->current_val.p)+1);
		    strncpy(olddefval, (*cl)->var->current_val.p+len,
			    strlen((*cl)->var->current_val.p)-len-1);
		    olddefval[strlen((*cl)->var->current_val.p)-len-1] = '\0';
		    start_with = olddefval;
		}
		else{
		    olddefval = cpystr((*cl)->var->current_val.p);
		    start_with = olddefval;
		}
	    }
	    else
	      start_with = (*apval) ? *apval : "";

	    input = (char *)fs_get((strlen(start_with)+1) * sizeof(char));
	    input[0] = '\0';
	    cstring_to_string(start_with, input);
	    err = signature_edit_lit(input, &result,
				     ((*cl)->var == role_comment_ptr)
					 ? "COMMENT EDITOR"
					 : "SIGNATURE EDITOR",
				     ((*cl)->var == role_comment_ptr)
					 ? h_composer_commentedit
					 : h_composer_sigedit);

	    if(!err){
		if(olddefval && !strcmp(input, result) &&
		   want_to("Leave unset and use default ", 'y',
			   'y', NO_HELP, WT_FLUSH_IN) == 'y'){
		    rv = 0;
		}
		else{
		    cstring_version = string_to_cstring(result);

		    if(apval && *apval)
		      fs_give((void **)apval);
		    
		    if(apval){
			*apval = cstring_version;
			cstring_version = NULL;
		    }

		    if(cstring_version)
		      fs_give((void **)&cstring_version);

		    rv = 1;
		}
	    }
	    else
	      rv = 0;

	    if(err){
		q_status_message1(SM_ORDER, 3, 5, "%.200s", err);
		fs_give((void **)&err);
	    }

	    if(result)
	      fs_give((void **)&result);
	    if(olddefval)
	      fs_give((void **)&olddefval);
	    if(input)
	      fs_give((void **)&input);
	}

	ps->mangled_screen = 1;
	break;
	
      default:
	rv = text_tool(ps, cmd, cl, flags);
	break;
    }

    /*
     * At this point, if changes occurred, var->user_val.X is set.
     * So, fix the current_val, and handle special cases...
     *
     * NOTE: we don't worry about the "fixed variable" case here, because
     *       editing such vars should have been prevented above...
     */
    if(rv == 1){
	/*
	 * Now go and set the current_val based on user_val changes
	 * above.  Turn off command line settings...
	 */
	set_current_val((*cl)->var, TRUE, FALSE);

	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	(*cl)->value = pretty_value(ps, *cl);

	exception_override_warning((*cl)->var);

	/*
	 * The value of literal sig can affect whether signature file is
	 * used or not. So it affects what we display for sig file variable.
	 */
	if((*cl)->next && (*cl)->next->var == &ps->vars[V_SIGNATURE_FILE]){
	    if((*cl)->next->value)
	      fs_give((void **)&(*cl)->next->value);
	    
	    (*cl)->next->value = pretty_value(ps, (*cl)->next);
	}
    }

    return(rv);
}


int
inbox_path_text_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    char           **apval;
    int		     rv = 0;
    char             new_inbox_path[2*MAXFOLDER+1];
    char            *def = NULL;
    CONTEXT_S       *cntxt;

    if(cmd != MC_EXIT && fixed_var((*cl)->var, NULL, NULL))
      return(rv);

    apval = APVAL((*cl)->var, ew);

    switch(cmd){
      case MC_ADD:
      case MC_EDIT:
	cntxt = ps->context_list;
	if(cmd == MC_EDIT && (*cl)->var){
	    if(ew == Post && (*cl)->var->post_user_val.p)
	      def = (*cl)->var->post_user_val.p;
	    else if(ew == Main && (*cl)->var->main_user_val.p)
	      def = (*cl)->var->main_user_val.p;
	    else if((*cl)->var->current_val.p)
	      def = (*cl)->var->current_val.p;
	}

	rv = add_new_folder(cntxt, ew, V_INBOX_PATH, new_inbox_path,
			    sizeof(new_inbox_path), NULL, def);
	rv = rv ? 1 : 0;

	ps->mangled_screen = 1;
        break;

      default:
	rv = text_tool(ps, cmd, cl, flags);
	break;
    }

    /*
     * This is just like the end of text_tool.
     */
    if(rv == 1){
	/*
	 * Now go and set the current_val based on user_val changes
	 * above.  Turn off command line settings...
	 */
	set_current_val((*cl)->var, TRUE, FALSE);
	fix_side_effects(ps, (*cl)->var, 0);

	if((*cl)->value)
	  fs_give((void **) &(*cl)->value);

	(*cl)->value = pretty_value(ps, *cl);

	exception_override_warning((*cl)->var);
    }

    return(rv);
}


int
text_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    return(text_toolit(ps, cmd, cl, flags, 0));
}


/*
 * simple text variable handler
 *
 * note, things get a little involved due to the
 *	 screen struct <--> variable mapping. (but, once its
 *       running it shouldn't need changing ;).
 *
 *    look_for_backslash == 1 means that backslash is an escape character.
 *                In particular, \, can be used to put a literal comma
 *                into a value. The value will still have the backslash
 *                in it, but the comma after the backslash won't be treated
 *                as an item separator.
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 *           returns what conf_exit_cmd returns for exit command.
 */
int
text_toolit(ps, cmd, cl, flags, look_for_backslash)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
    int		  look_for_backslash;
{
    char	     prompt[81], *sval, *tmp, *swap_val, **newval = NULL;
    char            *pval, **apval, **lval, ***alval;
    char            *olddefval = NULL;
    int		     rv = 0, skip_to_next = 0, after = 0, i = 4, j, k;
    int		     lowrange, hirange, incr, oeflags, oebufsize;
    int		     numval, repeat_key = 0;
    int              curindex, previndex, nextindex, deefault;
    HelpType         help;
    ESCKEY_S         ekey[6];

    if((*cl)->var->is_list){
	lval  = LVAL((*cl)->var, ew);
	alval = ALVAL((*cl)->var, ew);
    }
    else{
	pval  = PVAL((*cl)->var, ew);
	apval = APVAL((*cl)->var, ew);
    }

    oebufsize = MAXPATH;
    sval = (char *)fs_get(oebufsize*sizeof(char));
    sval[0] = '\0';

    if(flags&CF_NUMBER){ /* only happens if !is_list */
	incr = 1;
	if((*cl)->var == &ps->vars[V_FILLCOL]){
	    lowrange = 1;
	    hirange  = MAX_FILLCOL;
	}
	else if((*cl)->var == &ps->vars[V_OVERLAP]
		|| (*cl)->var == &ps->vars[V_MARGIN]){
	    lowrange = 0;
	    hirange  = 20;
	}
	else if((*cl)->var == &ps->vars[V_QUOTE_SUPPRESSION]){
	    lowrange = -(Q_SUPP_LIMIT-1);
	    hirange  = 1000;
	}
	else if((*cl)->var == &ps->vars[V_MAXREMSTREAM]){
	    lowrange = 0;
	    hirange  = 15;
	}
	else if((*cl)->var == &ps->vars[V_STATUS_MSG_DELAY]){
	    lowrange = -10;
	    hirange  = 30;
	}
	else if((*cl)->var == &ps->vars[V_MAILCHECK]
		|| (*cl)->var == &ps->vars[V_MAILCHECKNONCURR]){
	    lowrange = 0;
	    hirange  = 25000;
	    incr     = 15;
	}
	else if((*cl)->var == &ps->vars[V_DEADLETS]){
	    lowrange = 0;
	    hirange  = 9;
	}
	else if((*cl)->var == &ps->vars[V_NMW_WIDTH]){
	    lowrange = 20;
	    hirange  = MAX_SCREEN_COLS;
	}
	else if((*cl)->var == score_act_global_ptr){
	    lowrange = -100;
	    hirange  = 100;
	}
	else if((*cl)->var == &ps->vars[V_TCPOPENTIMEO] ||
	        (*cl)->var == &ps->vars[V_TCPREADWARNTIMEO] ||
	        (*cl)->var == &ps->vars[V_TCPQUERYTIMEO]){
	    lowrange = 5;
	    hirange  = 1000;
	}
	else if((*cl)->var == &ps->vars[V_TCPWRITEWARNTIMEO] ||
	        (*cl)->var == &ps->vars[V_RSHOPENTIMEO] ||
	        (*cl)->var == &ps->vars[V_SSHOPENTIMEO] ||
	        (*cl)->var == &ps->vars[V_USERINPUTTIMEO]){
	    lowrange = 0;
	    hirange  = 1000;
	}
	else if((*cl)->var == &ps->vars[V_MAILDROPCHECK]){
	    lowrange = 0;
	    hirange  = 1000000;
	    incr     = 60;
	}
	else if((*cl)->var == &ps->vars[V_NNTPRANGE]){
	    lowrange = 0;
	    hirange  = 1000000;
	    incr     = 100;
	}
	else if((*cl)->var == &ps->vars[V_REMOTE_ABOOK_VALIDITY]){
	    lowrange = -1;
	    hirange  = 25000;
	}
	else if((*cl)->var == &ps->vars[V_REMOTE_ABOOK_HISTORY]){
	    lowrange = 0;
	    hirange  = 100;
	}
	else if((*cl)->var == cat_lim_global_ptr){
	    lowrange = -1;
	    hirange  = 10000000;
	}
	else{
	    lowrange = 0;
	    hirange  = 25000;
	}

	ekey[0].ch    = -2;
	ekey[0].rval  = 'x';
	ekey[0].name  = "";
	ekey[0].label = "";
	ekey[1].ch    = ctrl('P');
	ekey[1].rval  = ctrl('P');
	ekey[1].name  = "^P";
	ekey[1].label = "Decrease";
	ekey[2].ch    = ctrl('N');
	ekey[2].rval  = ctrl('N');
	ekey[2].name  = "^N";
	ekey[2].label = "Increase";
	ekey[3].ch    = KEY_DOWN;
	ekey[3].rval  = ctrl('P');
	ekey[3].name  = "";
	ekey[3].label = "";
	ekey[4].ch    = KEY_UP;
	ekey[4].rval  = ctrl('N');
	ekey[4].name  = "";
	ekey[4].label = "";
	ekey[5].ch    = -1;
    }

    switch(cmd){
      case MC_ADD:				/* add to list */
	if(fixed_var((*cl)->var, "add to", NULL)){
	    break;
	}
	else if(!(*cl)->var->is_list && pval){
	    q_status_message(SM_ORDER, 3, 3,
			    "Only single value allowed.  Use \"Change\".");
	}
	else{
	    int maxwidth;
	    char *p;

	    if((*cl)->var->is_list
	       && lval && lval[0] && lval[0][0]
	       && (*cl)->value){
		char tmpval[101];
		/* regular add to an existing list */

		strncpy(tmpval, (*cl)->value, 100);
		tmpval[100] = '\0';
		removing_trailing_white_space(tmpval);
		/* 33 is the number of chars other than the value */
		maxwidth = min(80, ps->ttyo->screen_cols) - 15;
		k = min(18, max(maxwidth-33,0));
		if(strlen(tmpval) > k && k >= 3){
		    tmpval[k-1] = tmpval[k-2] = tmpval[k-3] = '.';
		    tmpval[k] = '\0';
		}

		sprintf(prompt,
		    "Enter text to insert before \"%.*s\": ",k,tmpval);
	    }
	    else if((*cl)->var->is_list
		    && !lval
		    && (*cl)->var->current_val.l){
		/* Add to list which doesn't exist, but default does exist */
		ekey[0].ch    = 'r';
		ekey[0].rval  = 'r';
		ekey[0].name  = "R";
		ekey[0].label = "Replace";
		ekey[1].ch    = 'a';
		ekey[1].rval  = 'a';
		ekey[1].name  = "A";
		ekey[1].label = "Add To";
		ekey[2].ch    = -1;
		strcpy(prompt, "Replace or Add To default value ? ");
		switch(radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, 'a', 'x',
				     h_config_replace_add, RB_NORM, NULL)){
		  case 'a':
		    p = sval;
		    for(j = 0; (*cl)->var->current_val.l[j]; j++){
			strcpy(p, (*cl)->var->current_val.l[j]);
			p += strlen(p);
			*p++ = ',';
			*p++ = ' ';
			*p = '\0';
		    }

add_text:
		    sprintf(prompt, "Enter the %stext to be added : ",
			flags&CF_NUMBER ? "numeric " : "");
		    break;
		    
		  case 'r':
replace_text:
		    if(olddefval){
			strncpy(sval, olddefval, oebufsize-1);
			sval[oebufsize-1] = '\0';
		    }

		    sprintf(prompt, "Enter the %sreplacement text : ",
			flags&CF_NUMBER ? "numeric " : "");
		    break;
		    
		  case 'x':
		    i = 1;
		    cmd_cancelled("Add");
		    break;
		}
	    }
	    else
	      sprintf(prompt, "Enter the %stext to be added : ",
		    flags&CF_NUMBER ? "numeric " : "");

	    ps->mangled_footer = 1;

	    if(i == 1)
	      break;

	    help = NO_HELP;
	    while(1){
		if((*cl)->var->is_list
		    && lval && lval[0] && lval[0][0]
		    && (*cl)->value){
		    ekey[0].ch    = ctrl('W');
		    ekey[0].rval  = 5;
		    ekey[0].name  = "^W";
		    ekey[0].label = after ? "InsertBefore" : "InsertAfter";
		    ekey[1].ch    = -1;
		}
		else if(!(flags&CF_NUMBER))
		  ekey[0].ch    = -1;

		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(sval, -FOOTER_ROWS(ps), 0, oebufsize,
				     prompt,
				     (ekey[0].ch != -1) ? ekey : NULL,
				     help, &oeflags);
		if(i == 0){
		    rv = 1;
		    if((*cl)->var->is_list)
		      ps->mangled_body = 1;
		    else
		      ps->mangled_footer = 1;

		    removing_leading_and_trailing_white_space(sval);
		    /*
		     * Coerce "" and <Empty Value> to empty string input.
		     * Catch <No Value Set> as a substitute for deleting.
		     */
		    if((*sval == '\"' && *(sval+1) == '\"' && *(sval+2) == '\0')
		        || !struncmp(sval, empty_val, EMPTY_VAL_LEN) 
			|| (*sval == '<'
			    && !struncmp(sval+1, empty_val, EMPTY_VAL_LEN)))
		      *sval = '\0';
		    else if(!struncmp(sval, no_val, NO_VAL_LEN)
		        || (*sval == '<'
			    && !struncmp(sval+1, no_val, NO_VAL_LEN)))
		      goto delete;

		    if((*cl)->var->is_list){
			if(*sval || !lval){
			    char **ltmp;
			    int    i;

			    i = 0;
			    for(tmp = sval; *tmp; tmp++)
			      if(*tmp == ',')
				i++;	/* conservative count of ,'s */

			    if(!i){
				ltmp    = (char **)fs_get(2 * sizeof(char *));
				ltmp[0] = cpystr(sval);
				ltmp[1] = NULL;
			    }
			    else
			      ltmp = parse_list(sval, i + 1,
					        look_for_backslash
						  ? PL_COMMAQUOTE : 0,
						NULL);

			    if(ltmp[0]){
				config_add_list(ps, cl, ltmp, &newval, after);
				if(after)
				  skip_to_next = 1;
			    }
			    else{
				q_status_message1(SM_ORDER, 0, 3,
					 "Can't add %.200s to list", empty_val);
				rv = ps->mangled_body = 0;
			    }

			    fs_give((void **)&ltmp);
			}
			else{
			    q_status_message1(SM_ORDER, 0, 3,
					 "Can't add %.200s to list", empty_val);
			}
		    }
		    else{
			if(flags&CF_NUMBER && sval[0]
			  && !(isdigit((unsigned char)sval[0])
			       || sval[0] == '-' || sval[0] == '+')){
			    q_status_message(SM_ORDER,3,3,
				  "Entry must be numeric");
			    i = 3; /* to keep loop going */
			    continue;
			}

			if(apval && *apval)
			  fs_give((void **)apval);

			if(!(olddefval && !strcmp(sval, olddefval))
			   || want_to("Leave unset and use default ",
				      'y', 'y', NO_HELP, WT_FLUSH_IN) == 'n')
			  *apval = cpystr(sval);

			newval = &(*cl)->value;
		    }
		}
		else if(i == 1){
		    cmd_cancelled("Add");
		}
		else if(i == 3){
		    help = help == NO_HELP ? h_config_add : NO_HELP;
		    continue;
		}
		else if(i == 4){		/* no redraw, yet */
		    continue;
		}
		else if(i == 5){ /* change from/to prepend to/from append */
		    char tmpval[101];

		    after = after ? 0 : 1;
		    strncpy(tmpval, (*cl)->value, 100);
		    tmpval[100] = '\0';
		    removing_trailing_white_space(tmpval);
		    /* 33 is the number of chars other than the value */
		    maxwidth = min(80, ps->ttyo->screen_cols) - 15;
		    k = min(18, max(maxwidth-33,0));
		    if(strlen(tmpval) > k && k >= 3){
			tmpval[k-1] = tmpval[k-2] = tmpval[k-3] = '.';
			tmpval[k] = '\0';
		    }

		    sprintf(prompt,
			"Enter text to insert %s \"%.*s\": ",
			after ? "after" : "before", k, tmpval);
		    continue;
		}
		else if(i == ctrl('P')){
		    if(sval[0])
		      numval = atoi(sval);
		    else{
		      if(pval)
			numval = atoi(pval);
		      else
			numval = lowrange + 1;
		    }

		    if(numval == lowrange){
			/*
			 * Protect user from repeating arrow key that
			 * causes message to appear over and over.
			 */
			if(++repeat_key > 0){
			    q_status_message1(SM_ORDER,3,3,
				"Minimum value is %.200s", comatose(lowrange));
			    repeat_key = -5;
			}
		    }
		    else
		      repeat_key = 0;

		    numval = max(numval - incr, lowrange);
		    sprintf(sval, "%d", numval);
		    continue;
		}
		else if(i == ctrl('N')){
		    if(sval[0])
		      numval = atoi(sval);
		    else{
		      if(pval)
			numval = atoi(pval);
		      else
			numval = lowrange + 1;
		    }

		    if(numval == hirange){
			if(++repeat_key > 0){
			    q_status_message1(SM_ORDER,3,3,
				"Maximum value is %.200s", comatose(hirange));
			    repeat_key = -5;
			}
		    }
		    else
		      repeat_key = 0;

		    numval = min(numval + incr, hirange);
		    sprintf(sval, "%d", numval);
		    continue;
		}

		break;
	    }
	}

	break;

      case MC_DELETE:				/* delete */
delete:
	if(!(*cl)->var->is_list
	    && apval && !*apval
	    && (*cl)->var->current_val.p){
	    char pmt[80];

	    sprintf(pmt, "Override default with %.20s", empty_val2);
	    if(want_to(pmt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		sval[0] = '\0';
		*apval = cpystr(sval);
		newval = &(*cl)->value;
		rv = ps->mangled_footer = 1;
	    }
	}
	else if((*cl)->var->is_list
		&& alval && !lval
		&& (*cl)->var->current_val.l){
	    char pmt[80];

	    sprintf(pmt, "Override default with %.20s", empty_val2);
	    if(want_to(pmt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		char **ltmp;

		sval[0] = '\0';
		ltmp    = (char **)fs_get(2 * sizeof(char *));
		ltmp[0] = cpystr(sval);
		ltmp[1] = NULL;
		config_add_list(ps, cl, ltmp, &newval, 0);
		fs_give((void **)&ltmp);
		rv = ps->mangled_body = 1;
	    }
	}
	else if(((*cl)->var->is_list && !lval)
		|| (!(*cl)->var->is_list && !pval)){
	    q_status_message(SM_ORDER, 0, 3, "No set value to delete");
	}
	else{
	    if((*cl)->var->is_fixed)
	        sprintf(prompt, "Delete (unused) %.30s from %.20s ",
		    (*cl)->var->is_list
		      ? (!*lval[(*cl)->varmem])
			  ? empty_val2
			  : lval[(*cl)->varmem]
		      : (pval)
			  ? (!*pval)
			      ? empty_val2
			      : pval
		 	  : "<NULL VALUE>",
		    (*cl)->var->name);
	    else
	        sprintf(prompt, "Really delete %s%.20s from %.30s ",
		    (*cl)->var->is_list ? "item " : "", 
		    (*cl)->var->is_list
		      ? int2string((*cl)->varmem + 1)
		      : (pval)
			  ? (!*pval)
			      ? empty_val2
			      : pval
		 	  : "<NULL VALUE>",
		    (*cl)->var->name);


	    ps->mangled_footer = 1;
	    if(want_to(prompt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		rv = 1;
		if((*cl)->var->is_list)
		  ps->mangled_body = 1;
		else
		  ps->mangled_footer = 1;

		if((*cl)->var->is_list){
		    if(lval[(*cl)->varmem])
		      fs_give((void **)&lval[(*cl)->varmem]);

		    config_del_list_item(cl, &newval);
		}
		else{
		    if(apval && *apval)
		      fs_give((void **)apval);

		    newval = &(*cl)->value;
		}
	    }
	    else
	      q_status_message(SM_ORDER, 0, 3, "Value not deleted");
	}

	break;

      case MC_EDIT:				/* edit/change list option */
	if(fixed_var((*cl)->var, NULL, NULL)){
	    break;
	}
	else if(((*cl)->var->is_list
		    && !lval
		    && (*cl)->var->current_val.l)
		||
		(!(*cl)->var->is_list
		    && !pval
		    && (*cl)->var->current_val.p)){

	    /*
	     * In non-list case, offer default value for editing.
	     */
	    if(!(*cl)->var->is_list
	       && (*cl)->var != &ps->vars[V_REPLY_INTRO]
	       && (*cl)->var->current_val.p[0]
	       && strcmp(VSTRING,(*cl)->var->current_val.p)){
		int quote_it;
		size_t len;

		olddefval = (char *)fs_get(strlen((*cl)->var->current_val.p)+3);

		if(!strncmp((*cl)->var->current_val.p,
			    DSTRING,
			    (len=strlen(DSTRING)))){
		    /* strip DSTRING and trailing paren */
		    strncpy(olddefval, (*cl)->var->current_val.p+len,
			    strlen((*cl)->var->current_val.p)-len-1);
		    olddefval[strlen((*cl)->var->current_val.p)-len-1] = '\0';
		}
		else{
		    /* quote it if there are trailing spaces */
		    quote_it = ((*cl)->var->current_val.p[strlen((*cl)->var->current_val.p)-1] == SPACE);
		    sprintf(olddefval, "%s%s%s", quote_it ? "\"" : "",
						 (*cl)->var->current_val.p,
						 quote_it ? "\"" : "");
		}
	    }

	    goto replace_text;
	}
	else if(((*cl)->var->is_list
		    && !lval
		    && !(*cl)->var->current_val.l)
		||
		(!(*cl)->var->is_list
		    && !pval
		    && !(*cl)->var->current_val.p)){
	    goto add_text;
	}
	else{
	    HelpType help;
	    char *clptr;

	    if(sval)
	      fs_give((void **)&sval);
	    if((*cl)->var->is_list){
		sprintf(prompt, "Change field %.30s list entry : ",
			(*cl)->var->name);
 		clptr = lval[(*cl)->varmem] ? lval[(*cl)->varmem] : NULL;
	    }
	    else{
		sprintf(prompt, "Change %sfield %.35s value : ",
			flags&CF_NUMBER ? "numeric " : "",
			(*cl)->var->name);
 		clptr = pval ? pval : NULL;
	    }

 	    oebufsize = clptr ? (int)max(MAXPATH, 50+strlen(clptr)) : MAXPATH;
 	    sval = (char *)fs_get(oebufsize * sizeof(char));
 	    sprintf(sval, "%s", clptr ? clptr : "");

	    ps->mangled_footer = 1;
	    help = NO_HELP;
	    while(1){
		if(!(flags&CF_NUMBER))
		  ekey[0].ch = -1;

		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(sval, -FOOTER_ROWS(ps), 0, oebufsize,
				     prompt,
				     (ekey[0].ch != -1) ? ekey : NULL,
				     help, &oeflags);
		if(i == 0){
		    removing_leading_and_trailing_white_space(sval);
		    /*
		     * Coerce "" and <Empty Value> to empty string input.
		     * Catch <No Value Set> as a substitute for deleting.
		     */
		    if((*sval == '\"' && *(sval+1) == '\"' && *(sval+2) == '\0')
		        || !struncmp(sval, empty_val, EMPTY_VAL_LEN) 
			|| (*sval == '<'
			    && !struncmp(sval+1, empty_val, EMPTY_VAL_LEN)))
		      *sval = '\0';
		    else if(!struncmp(sval, no_val, NO_VAL_LEN)
			|| (*sval == '<'
			    && !struncmp(sval+1, no_val, NO_VAL_LEN)))
		      goto delete;

		    rv = 1;
		    if((*cl)->var->is_list)
		      ps->mangled_body = 1;
		    else
		      ps->mangled_footer = 1;

		    if((*cl)->var->is_list){
			char **ltmp = NULL;
			int    i;

			if(lval[(*cl)->varmem])
			  fs_give((void **)&lval[(*cl)->varmem]);

			i = 0;
			for(tmp = sval; *tmp; tmp++)
			  if(*tmp == ',')
			    i++;	/* conservative count of ,'s */

			if(i)
			  ltmp = parse_list(sval, i + 1,
					    look_for_backslash
					      ? PL_COMMAQUOTE : 0,
					    NULL);

			if(ltmp && !ltmp[0])		/* only commas */
			  goto delete;
			else if(!i || (ltmp && !ltmp[1])){  /* only one item */
			    lval[(*cl)->varmem] = cpystr(sval);
			    newval = &(*cl)->value;

			    if(ltmp && ltmp[0])
			      fs_give((void **)&ltmp[0]);
			}
			else if(ltmp){
			    /*
			     * Looks like the value was changed to a 
			     * list, so delete old value, and insert
			     * new list...
			     *
			     * If more than one item in existing list and
			     * current is end of existing list, then we
			     * have to delete and append instead of
			     * deleting and prepending.
			     */
			    if(((*cl)->varmem > 0 || lval[1])
			       && !(lval[(*cl)->varmem+1])){
				after = 1;
				skip_to_next = 1;
			    }

			    config_del_list_item(cl, &newval);
			    config_add_list(ps, cl, ltmp, &newval, after);
			}

			if(ltmp)
			  fs_give((void **)&ltmp);
		    }
		    else{
			if(flags&CF_NUMBER && sval[0]
			  && !(isdigit((unsigned char)sval[0])
			       || sval[0] == '-' || sval[0] == '+')){
			    q_status_message(SM_ORDER,3,3,
				  "Entry must be numeric");
			    continue;
			}

			if(apval && *apval)
			  fs_give((void **)apval);

			if(sval[0] && apval)
			  *apval = cpystr(sval);

			newval = &(*cl)->value;
		    }
		}
		else if(i == 1){
		    cmd_cancelled("Change");
		}
		else if(i == 3){
		    help = help == NO_HELP ? h_config_change : NO_HELP;
		    continue;
		}
		else if(i == 4){		/* no redraw, yet */
		    continue;
		}
		else if(i == ctrl('P')){
		    numval = atoi(sval);
		    if(numval == lowrange){
			/*
			 * Protect user from repeating arrow key that
			 * causes message to appear over and over.
			 */
			if(++repeat_key > 0){
			    q_status_message1(SM_ORDER,3,3,
				"Minimum value is %.200s", comatose(lowrange));
			    repeat_key = -5;
			}
		    }
		    else
		      repeat_key = 0;

		    numval = max(numval - incr, lowrange);
		    sprintf(sval, "%d", numval);
		    continue;
		}
		else if(i == ctrl('N')){
		    numval = atoi(sval);
		    if(numval == hirange){
			if(++repeat_key > 0){
			    q_status_message1(SM_ORDER,3,3,
				"Maximum value is %.200s", comatose(hirange));
			    repeat_key = -5;
			}
		    }
		    else
		      repeat_key = 0;

		    numval = min(numval + incr, hirange);
		    sprintf(sval, "%d", numval);
		    continue;
		}

		break;
	    }
	}

	break;

      case MC_SHUFFLE:
	if(!((*cl)->var && (*cl)->var->is_list)){
	    q_status_message(SM_ORDER, 0, 2,
			     "Can't shuffle single-valued setting");
	    break;
	}

	if(!alval)
	  break;

	curindex = (*cl)->varmem;
	previndex = curindex-1;
	nextindex = curindex+1;
	if(!*alval || !(*alval)[nextindex])
	  nextindex = -1;

	if((previndex < 0 && nextindex < 0) || !*alval){
	    q_status_message(SM_ORDER, 0, 3,
   "Shuffle only makes sense when there is more than one value defined");
	    break;
	}

	/* Move it up or down? */
	i = 0;
	ekey[i].ch      = 'u';
	ekey[i].rval    = 'u';
	ekey[i].name    = "U";
	ekey[i++].label = "Up";

	ekey[i].ch      = 'd';
	ekey[i].rval    = 'd';
	ekey[i].name    = "D";
	ekey[i++].label = "Down";

	ekey[i].ch = -1;
	deefault = 'u';

	if(previndex < 0){		/* no up */
	    ekey[0].ch = -2;
	    deefault = 'd';
	}
	else if(nextindex < 0)
	  ekey[1].ch = -2;	/* no down */

	sprintf(prompt, "Shuffle %s%s%s ? ",
		(ekey[0].ch != -2) ? "UP" : "",
		(ekey[0].ch != -2 && ekey[1].ch != -2) ? " or " : "",
		(ekey[1].ch != -2) ? "DOWN" : "");
	help = (ekey[0].ch == -2) ? h_hdrcolor_shuf_down
				  : (ekey[1].ch == -2) ? h_hdrcolor_shuf_up
						       : h_hdrcolor_shuf;

	i = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, deefault, 'x',
			   help, RB_NORM, NULL);

	switch(i){
	  case 'x':
	    cmd_cancelled("Shuffle");
	    return(rv);

	  case 'u':
	  case 'd':
	    break;
	}
		
	/* swap order */
	if(i == 'd'){
	    swap_val = (*alval)[curindex];
	    (*alval)[curindex] = (*alval)[nextindex];
	    (*alval)[nextindex] = swap_val;
	}
	else if(i == 'u'){
	    swap_val = (*alval)[curindex];
	    (*alval)[curindex] = (*alval)[previndex];
	    (*alval)[previndex] = swap_val;
	}
	else		/* can't happen */
	  break;

	/*
	 * Fix the conf line values.
	 */

	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	(*cl)->value = pretty_value(ps, *cl);

	if(i == 'd'){
	    if((*cl)->next->value)
	      fs_give((void **)&(*cl)->next->value);

	    (*cl)->next->value = pretty_value(ps, (*cl)->next);
	    *cl = next_confline(*cl);
	}
	else{
	    if((*cl)->prev->value)
	      fs_give((void **)&(*cl)->prev->value);

	    (*cl)->prev->value = pretty_value(ps, (*cl)->prev);
	    *cl = prev_confline(*cl);
	}

	rv = ps->mangled_body = 1;
	break;

      case MC_EXIT:				/* exit */
	rv = config_exit_cmd(flags);
	break;

      default:
	rv = -1;
	break;
    }

    if(skip_to_next)
      *cl = next_confline(*cl);

    /*
     * At this point, if changes occurred, var->user_val.X is set.
     * So, fix the current_val, and handle special cases...
     *
     * NOTE: we don't worry about the "fixed variable" case here, because
     *       editing such vars should have been prevented above...
     */
    if(rv == 1){
	/*
	 * Now go and set the current_val based on user_val changes
	 * above.  Turn off command line settings...
	 */
	set_current_val((*cl)->var, TRUE, FALSE);
	fix_side_effects(ps, (*cl)->var, 0);

	/*
	 * Delay setting the displayed value until "var.current_val" is set
	 * in case current val get's changed due to a special case above.
	 */
	if(newval){
	    if(*newval)
	      fs_give((void **)newval);

	    *newval = pretty_value(ps, *cl);
	}

	exception_override_warning((*cl)->var);
    }

    if(sval)
      fs_give((void **)&sval);
 
    if(olddefval)
      fs_give((void **)&olddefval);
      
    return(rv);
}


int
config_exit_cmd(flags)
    unsigned flags;
{
    return(screen_exit_cmd(flags, "Configuration"));
}


simple_exit_cmd(flags)
    unsigned flags;
{
    return(2);
}


/*
 * screen_exit_cmd - basic config/flag screen exit logic
 */
int
screen_exit_cmd(flags, cmd)
    unsigned  flags;
    char     *cmd;
{
    if(flags & CF_CHANGES){
      switch(want_to(EXIT_PMT, 'y', 'x', h_config_undo, WT_FLUSH_IN)){
	case 'y':
	  q_status_message1(SM_ORDER,0,3,"%.200s changes saved", cmd);
	  return(2);

	case 'n':
	  q_status_message1(SM_ORDER,3,5,"No %.200s changes saved", cmd);
	  return(10);

	case 'x':  /* ^C */
	default :
	  q_status_message(SM_ORDER,3,5,"Changes not yet saved");
	  return(0);
      }
    }
    else
      return(2);
}


/*
 *
 */
void
config_add_list(ps, cl, ltmp, newval, after)
    struct pine *ps;
    CONF_S     **cl;
    char       **ltmp, ***newval;
    int		 after;
{
    int	    items, i;
    char   *tmp, ***alval;
    CONF_S *ctmp;

    for(items = 0, i = 0; ltmp[i]; i++)		/* count list items */
      items++;

    alval = ALVAL((*cl)->var, ew);

    if(alval && (*alval)){
	if((*alval)[0] && (*alval)[0][0]){
	    /*
	     * Since we were already a list, make room
	     * for the new member[s] and fall thru to
	     * actually fill them in below...
	     */
	    for(i = 0; (*alval)[i]; i++)
	      ;

	    fs_resize((void **)alval, (i + items + 1) * sizeof(char *));

	    /*
	     * move the ones that will be bumped down to the bottom of the list
	     */
	    for(; i >= (*cl)->varmem + (after?1:0); i--)
	      (*alval)[i+items] = (*alval)[i];

	    i = 0;
	}
	else if(alval){
	    (*cl)->varmem = 0;
	    if(*alval)
	      free_list_array(alval);

	    *alval = (char **)fs_get((items+1)*sizeof(char *));
	    memset((void *)(*alval), 0, (items+1)*sizeof(char *));
	    (*alval)[0] = ltmp[0];
	    if(newval)
	      *newval = &(*cl)->value;

	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    i = 1;
	}
    }
    else if(alval){
	/*
	 * since we were previously empty, we want
	 * to replace the first CONF_S's value with
	 * the first new value, and fill the other
	 * in below if there's a list...
	 *
	 * first, make sure we're at the beginning of this config
	 * section and dump the config lines for the default list,
	 * except for the first one, which we will over-write.
	 */
	*cl = (*cl)->varnamep; 
	while((*cl)->next && (*cl)->next->varnamep == (*cl)->varnamep)
	  snip_confline(&(*cl)->next);

	/*
	 * now allocate the new user_val array and fill in the first entry.
	 */
	*alval = (char **)fs_get((items+1)*sizeof(char *));
	memset((void *)(*alval), 0, (items+1) * sizeof(char *));
	(*alval)[(*cl)->varmem=0] = ltmp[0];
	if(newval)
	  *newval = &(*cl)->value;

	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	i = 1;
    }

    /*
     * Make new cl's to fit in the new space.  Move the value from the current
     * line if inserting before it, else leave it where it is.
     */
    for(; i < items ; i++){
	(*alval)[i+(*cl)->varmem + (after?1:0)] = ltmp[i];
	tmp = (*cl)->value;
	new_confline(cl);
	if(after)
	  (*cl)->value   = NULL;
	else
	  (*cl)->value   = tmp;

	(*cl)->var       = (*cl)->prev->var;
	(*cl)->valoffset = (*cl)->prev->valoffset;
	(*cl)->varoffset = (*cl)->prev->varoffset;
	(*cl)->headingp  = (*cl)->prev->headingp;
	(*cl)->keymenu   = (*cl)->prev->keymenu;
	(*cl)->help      = (*cl)->prev->help;
	(*cl)->tool      = (*cl)->prev->tool;
	(*cl)->varnamep  = (*cl)->prev->varnamep;
	*cl		 = (*cl)->prev;
	if(!after)
	  (*cl)->value   = NULL;

	if(newval){
	    if(after)
	      *newval	 = &(*cl)->next->value;
	    else
	      *newval	 = &(*cl)->value;
	}
    }

    /*
     * now fix up varmem values and fill in new values that have been
     * left NULL
     */
    for(ctmp = (*cl)->varnamep, i = 0;
	(*alval)[i];
	ctmp = ctmp->next, i++){
	ctmp->varmem = i;
	if(!ctmp->value){
	    /* BUG:  We should be able to do this without the temp
	     * copy...  
	     */
	    char *ptmp = pretty_value(ps, ctmp);
	    ctmp->value = (ctmp->varnamep->flags & CF_PRINTER) ? printer_name(ptmp) : cpystr(ptmp);
	    fs_give((void **)&ptmp);
	}
    }
}


/*
 *
 */
void
config_del_list_item(cl, newval)
    CONF_S  **cl;
    char   ***newval;
{
    char   **bufp, ***alval;
    int	     i;
    CONF_S  *ctmp;

    alval = ALVAL((*cl)->var, ew);

    if((*alval)[(*cl)->varmem + 1]){
	for(bufp = &(*alval)[(*cl)->varmem];
	    *bufp = *(bufp+1); bufp++)
	  ;

	if(*cl == (*cl)->varnamep){		/* leading value */
	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    ctmp = (*cl)->next;
	    (*cl)->value = ctmp->value;
	    ctmp->value  = NULL;
	}
	else{
	    ctmp = *cl;			/* blast the confline */
	    *cl = (*cl)->next;
	    if(ctmp == opt_screen->top_line)
	      opt_screen->top_line = *cl;
	}

	snip_confline(&ctmp);

	for(ctmp = (*cl)->varnamep, i = 0;	/* now fix up varmem values */
	    (*alval)[i];
	    ctmp = ctmp->next, i++)
	  ctmp->varmem = i;
    }
    else if((*cl)->varmem){			/* blasted last in list */
	ctmp = *cl;
	*cl = (*cl)->prev;
	if(ctmp == opt_screen->top_line)
	  opt_screen->top_line = *cl;

	snip_confline(&ctmp);
    }
    else{					/* blasted last remaining */
	if(alval && *alval)
	  fs_give((void **)alval);

	*newval = &(*cl)->value;
    }
}


/*
 * feature list manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
checkbox_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S	**cl;
    unsigned      flags;
{
    int  rv = 0;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark feature */
	if((*cl)->var == &ps->vars[V_FEATURE_LIST]){
	    rv = 1;
	    toggle_feature_bit(ps, (*cl)->varmem, (*cl)->var, *cl, 0);
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 6,
			   "Programmer botch!  Unknown checkbox type.");

	break;

      case MC_EXIT:				 /* exit */
	rv = config_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


/*
 * Message flag manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
flag_checkbox_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S	**cl;
    unsigned      flags;
{
    int rv = 0, state;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark feature */
	state = (*cl)->d.f.fp->set;
	state = (state == 1) ? 0 : (!state && ((*cl)->d.f.fp->ukn)) ? 2 : 1;
	(*cl)->value[1] = (state == 0) ? ' ' : ((state == 1) ? 'X': '?');
	(*cl)->d.f.fp->set = state;
	rv = 1;
	break;

      case MC_EXIT:				/* exit */
	rv = simple_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


/*
 * simple radio-button style variable handler
 */
int
radiobutton_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    char     **apval;
    int	       rv = 0;
    int        old_uc, old_cs;
    CONF_S    *ctmp;
    NAMEVAL_S *rule = NULL;

    apval = APVAL((*cl)->var, ew);

    switch(cmd){
      case MC_CHOICE :				/* set/unset feature */

	if(fixed_var((*cl)->var, NULL, NULL)){
	    if(((*cl)->var->post_user_val.p || (*cl)->var->main_user_val.p)
	       && want_to("Delete old unused personal option setting",
			  'y', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		delete_user_vals((*cl)->var);
		q_status_message(SM_ORDER, 0, 3, "Deleted");
		rv = 1;
	    }

	    return(rv);
	}

	if(standard_radio_var(ps, (*cl)->var) || (*cl)->var == startup_ptr){
	    PTR_TO_RULEFUNC rulefunc;

#ifndef	_WINDOWS
	    if((*cl)->var == &ps->vars[V_COLOR_STYLE]){
		old_uc = pico_usingcolor();
		old_cs = ps->color_style;
	    }
#endif

	    if((*cl)->var->cmdline_val.p)
	      fs_give((void **)&(*cl)->var->cmdline_val.p);

	    if(apval && *apval)
	      fs_give((void **)apval);

	    rulefunc = rulefunc_from_var(ps, (*cl)->var);
	    if(rulefunc)
	      rule = (*rulefunc)((*cl)->varmem);

	    if(apval && rule)
	      *apval = cpystr(S_OR_L(rule));

	    cur_rule_value((*cl)->var, TRUE, TRUE);
	    set_radio_pretty_vals(ps, cl);

	    if((*cl)->var == &ps->vars[V_AB_SORT_RULE])
	      addrbook_reset();
	    else if((*cl)->var == &ps->vars[V_THREAD_DISP_STYLE]){
		clear_index_cache();
	    }
	    else if((*cl)->var == &ps->vars[V_THREAD_INDEX_STYLE]){
		MAILSTREAM *m;
		int         i;

		clear_index_cache();
		/* clear all hidden and collapsed flags */
		set_lflags(ps->mail_stream, ps->msgmap, MN_COLL | MN_CHID, 0);

		if(SEP_THRDINDX() && SORT_IS_THREADED(ps->msgmap))
		  unview_thread(ps, ps->mail_stream, ps->msgmap);

		if(SORT_IS_THREADED(ps->msgmap)
		   && (SEP_THRDINDX() || COLL_THRDS()))
		  collapse_threads(ps->mail_stream, ps->msgmap, NULL);

		for(i = 0; i < ps_global->s_pool.nstream; i++){
		    m = ps_global->s_pool.streams[i];
		    if(m)
		      sp_set_viewing_a_thread(m, 0);
		}

		adjust_cur_to_visible(ps->mail_stream, ps->msgmap);
	    }
#ifndef	_WINDOWS
	    else if((*cl)->var == &ps->vars[V_COLOR_STYLE]){
		if(old_cs != ps->color_style){
		    pico_toggle_color(0);
		    switch(ps->color_style){
		      case COL_NONE:
		      case COL_TERMDEF:
			pico_set_color_options(0);
			break;
		      case COL_ANSI8:
			pico_set_color_options(COLOR_ANSI8_OPT);
			break;
		      case COL_ANSI16:
			pico_set_color_options(COLOR_ANSI16_OPT);
			break;
		    }

		    if(ps->color_style != COL_NONE)
		      pico_toggle_color(1);
		}
	    
		if(pico_usingcolor())
		  pico_set_normal_color();

		if(!old_uc && pico_usingcolor()){

		    /*
		     * remove the explanatory warning line and a blank line
		     */

		    /* first find the first blank line */
		    for(ctmp = *cl; ctmp; ctmp = next_confline(ctmp))
		      if(ctmp->flags & CF_NOSELECT)
			break;

		    if(ctmp && ctmp->flags & CF_NOSELECT &&
		       ctmp->prev && !(ctmp->prev->flags & CF_NOSELECT) &&
		       ctmp->next && ctmp->next->flags & CF_NOSELECT &&
		       ctmp->next->next &&
		       ctmp->next->next->flags & CF_NOSELECT){
			ctmp->prev->next = ctmp->next->next;
			ctmp->next->next->prev = ctmp->prev;
			ctmp->next->next = NULL;
			free_conflines(&ctmp);
		    }

		    /* make all the colors selectable */
		    for(ctmp = *cl; ctmp; ctmp = next_confline(ctmp))
		      if(ctmp->flags & CF_POT_SLCTBL)
			ctmp->flags &= ~CF_NOSELECT;
		}
		else if(old_uc && !pico_usingcolor()){

		    /*
		     * add the explanatory warning line and a blank line
		     */

		    /* first find the existing blank line */
		    for(ctmp = *cl; ctmp; ctmp = next_confline(ctmp))
		      if(ctmp->flags & CF_NOSELECT)
			break;

		    /* add the explanatory warning line */
		    new_confline(&ctmp);
		    ctmp->help   = NO_HELP;
		    ctmp->flags |= CF_NOSELECT;
		    ctmp->value  = cpystr(COLORNOSET);

		    /* and add another blank line */
		    new_confline(&ctmp);
		    ctmp->flags |= (CF_NOSELECT | CF_B_LINE);

		    /* make all the colors non-selectable */
		    for(ctmp = *cl; ctmp; ctmp = next_confline(ctmp))
		      if(ctmp->flags & CF_POT_SLCTBL)
			ctmp->flags |= CF_NOSELECT;
		}

		clear_index_cache();
		ClearScreen();
		ps->mangled_screen = 1;
	    }
#endif

	    ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	    rv = 1;
	}
	else if((*cl)->var == &ps->vars[V_SORT_KEY]){
	    SortOrder def_sort;
	    int       def_sort_rev;

	    def_sort_rev  = (*cl)->varmem >= (short) EndofList;
	    def_sort      = (SortOrder) ((*cl)->varmem - (def_sort_rev
								 * EndofList));
	    sprintf(tmp_20k_buf, "%s%s", sort_name(def_sort),
		    (def_sort_rev) ? "/Reverse" : "");

	    if((*cl)->var->cmdline_val.p)
	      fs_give((void **)&(*cl)->var->cmdline_val.p);

	    if(apval){
		if(*apval)
		  fs_give((void **)apval);

		*apval = cpystr(tmp_20k_buf);
	    }

	    set_current_val((*cl)->var, TRUE, TRUE);
	    if(decode_sort(ps->VAR_SORT_KEY, &def_sort, &def_sort_rev) != -1){
		ps->def_sort     = def_sort;
		ps->def_sort_rev = def_sort_rev;
	    }

	    set_radio_pretty_vals(ps, cl);
	    ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	    rv = 1;
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 6,
			   "Programmer botch!  Unknown radiobutton type.");

	break;

      case MC_EXIT:				/* exit */
	rv = config_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    if(rv == 1)
      exception_override_warning((*cl)->var);
      
    return(rv);
}



/*
 * simple yes/no style variable handler
 */
int
yesno_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int   rv = 0, yes = 0;
    char *pval, **apval;

    pval =  PVAL((*cl)->var, ew);
    apval = APVAL((*cl)->var, ew);

    switch(cmd){
      case MC_TOGGLE:				/* toggle yes to no and back */
	if(fixed_var((*cl)->var, NULL, NULL)){
	    if(((*cl)->var->post_user_val.p || (*cl)->var->main_user_val.p)
	       && want_to("Delete old unused personal option setting",
			  'y', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		delete_user_vals((*cl)->var);
		q_status_message(SM_ORDER, 0, 3, "Deleted");
		rv = 1;
	    }

	    return(rv);
	}

	rv = 1;
	yes = ((pval && !strucmp(pval, yesstr)) ||
	       (!pval && (*cl)->var->current_val.p &&
	        !strucmp((*cl)->var->current_val.p, yesstr)));
	fs_give((void **)&(*cl)->value);

	if(apval){
	    if(*apval)
	      fs_give((void **)apval);

	    if(yes)
	      *apval = cpystr(nostr);
	    else
	      *apval = cpystr(yesstr);
	}

	set_current_val((*cl)->var, FALSE, FALSE);
	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	(*cl)->value = pretty_value(ps, *cl);
	fix_side_effects(ps, (*cl)->var, 0);

	break;

      case MC_EXIT:				/* exit */
	rv = config_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


int
print_select_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    int rc, retval, no_ex, printer_msg = 0;
    char *p, **lval, *printer_was;
    struct variable *vtmp;

    no_ex = (ps_global->ew_for_except_vars == Main);

    printer_was = ps->VAR_PRINTER ? cpystr(ps->VAR_PRINTER) : NULL;

    switch(cmd){
      case MC_EXIT:
        retval = config_exit_cmd(flags);
	break;

      case MC_CHOICE :
	if(cl && *cl){
	    char aname[100], wname[100];

	    strncat(strncpy(aname, ANSI_PRINTER, 50), no_ff, 30);
	    strncat(strncpy(wname, WYSE_PRINTER, 50), no_ff, 30);
	    if((*cl)->var){
		vtmp = (*cl)->var;
		lval = (no_ex || !vtmp->is_user) ? vtmp->current_val.l
						 : LVAL(vtmp, ew);
		rc = set_variable(V_PRINTER, lval ? lval[(*cl)->varmem] : NULL,
				  1, 1, ew);
		if(rc == 0){
		    if(vtmp == &ps->vars[V_STANDARD_PRINTER])
		      ps->printer_category = 2;
		    else if(vtmp == &ps->vars[V_PERSONAL_PRINT_COMMAND])
		      ps->printer_category = 3;

		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);

		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5,
			"Trouble setting default printer");

		retval = 1;
	    }
	    else if(!strcmp((*cl)->value,ANSI_PRINTER)){
		rc = set_variable(V_PRINTER, ANSI_PRINTER, 1, 1, ew);
		if(rc == 0){
		    ps->printer_category = 1;
		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);
		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5,
			"Trouble setting default printer");

		retval = 1;
	    }
	    else if(!strcmp((*cl)->value,aname)){
		rc = set_variable(V_PRINTER, aname, 1, 1, ew);
		if(rc == 0){
		    ps->printer_category = 1;
		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);
		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5,
			"Trouble setting default printer");

		retval = 1;
	    }
	    else if(!strcmp((*cl)->value,WYSE_PRINTER)){
		rc = set_variable(V_PRINTER, WYSE_PRINTER, 1, 1, ew);
		if(rc == 0){
		    ps->printer_category = 1;
		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);
		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5,
			"Trouble setting default printer");

		retval = 1;
	    }
	    else if(!strcmp((*cl)->value,wname)){
		rc = set_variable(V_PRINTER, wname, 1, 1, ew);
		if(rc == 0){
		    ps->printer_category = 1;
		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);
		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5,
			"Trouble setting default printer");

		retval = 1;
	    }
	    else
	      retval = 0;
	}
	else
	  retval = 0;

	if(retval){
	    ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	    set_def_printer_value(ps->VAR_PRINTER);
	}

	break;

      default:
	retval = -1;
	break;
    }

    if(printer_msg){
	p = NULL;
	if(ps->VAR_PRINTER){
	    char *nick, *q;

	    parse_printer(ps->VAR_PRINTER, &nick, &q,
			  NULL, NULL, NULL, NULL);
	    p = cpystr(*nick ? nick : q);
	    fs_give((void **)&nick);
	    fs_give((void **)&q);
	}

	q_status_message4(SM_ORDER, 0, 3,
			  "Default printer%.200s %.200s%.200s%.200s",
			  ((!printer_was && !ps->VAR_PRINTER) ||
			   (printer_was && ps->VAR_PRINTER &&
			    !strcmp(printer_was,ps->VAR_PRINTER)))
			      ? " still" : "",
			  p ? "set to \"" : "unset",
			  p ? p : "", p ? "\"" : ""); 

	if(p)
	  fs_give((void **)&p);
    }

    if(printer_was)
      fs_give((void **)&printer_was);

    return(retval);
}


int
print_edit_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    char	     prompt[81], sval[MAXPATH+1], name[MAXPATH+1];
    char            *nick, *p, *tmp, **newval = NULL, **ltmp = NULL;
    char           **lval, **nelval;
    int		     rv = 0, skip_to_next = 0, after = 0, i = 4, j, k = 0;
    int		     oeflags, changing_selected = 0, no_ex;
    HelpType         help;
    ESCKEY_S         ekey[6];

    /* need this to preserve old behavior when no exception config file */
    no_ex = (ps_global->ew_for_except_vars == Main);

    if(cmd == MC_CHOICE)
      return(print_select_tool(ps, cmd, cl, flags));

    if(!(cl && *cl && (*cl)->var))
      return(0);

    nelval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
    lval = LVAL((*cl)->var, ew);

    switch(cmd){
      case MC_ADD:				/* add to list */
	sval[0] = '\0';
	if(!fixed_var((*cl)->var, "add to", NULL)){

	    if(lval && (*cl)->value){
		strcpy(prompt, "Enter printer name : ");
	    }
	    else if(!lval && nelval){
		/* Add to list which doesn't exist, but default does exist */
		ekey[0].ch    = 'r';
		ekey[0].rval  = 'r';
		ekey[0].name  = "R";
		ekey[0].label = "Replace";
		ekey[1].ch    = 'a';
		ekey[1].rval  = 'a';
		ekey[1].name  = "A";
		ekey[1].label = "Add To";
		ekey[2].ch    = -1;
		strcpy(prompt, "Replace or Add To default value ? ");
		switch(i = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, 'a',
					 'x', h_config_replace_add, RB_NORM,
					 NULL)){
		  case 'a':
		    /* Make a list of the default commands, leaving room for 
		       the command we are about to add below. */
		    for(k = 0; nelval[k]; k++)
		      ;

		    ltmp = (char **)fs_get((k+2) * sizeof(char *));
		    
		    for(j = 0; j < k; j++)
		      ltmp[j] = cpystr(nelval[j]);

		    ltmp[k + 1] = ltmp[k] = NULL;

add_text:
		    strcpy(prompt, "Enter name of printer to be added : ");
		    break;
		    
		  case 'r':
replace_text:
		    strcpy(prompt,
			"Enter the name for replacement printer : ");
		    break;
		    
		  case 'x':
		    cmd_cancelled("Add");
		    break;
		}

		if(i == 'x')
		  break;
	    }
	    else
	      strcpy(prompt, "Enter name of printer to be added : ");

	    ps->mangled_footer = 1;
	    help = NO_HELP;

	    name[0] = '\0';
	    i = 2;
	    while(i != 0 && i != 1){
		if(lval && (*cl)->value){
		    ekey[0].ch    = ctrl('W');
		    ekey[0].rval  = 5;
		    ekey[0].name  = "^W";
		    ekey[0].label = after ? "InsertBefore" : "InsertAfter";
		    ekey[1].ch    = -1;
		}
		else
		  ekey[0].ch    = -1;

		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(name, -FOOTER_ROWS(ps), 0, sizeof(name),
				     prompt,
				     (ekey[0].ch != -1) ? ekey : NULL,
				     help, &oeflags);
		if(i == 0){
		    rv = ps->mangled_body = 1;
		    removing_leading_and_trailing_white_space(name);
		}
		else if(i == 1){
		    cmd_cancelled("Add");
		}
		else if(i == 3){
		    help = (help == NO_HELP) ? h_config_insert_after : NO_HELP;
		}
		else if(i == 4){		/* no redraw, yet */
		}
		else if(i == 5){ /* change from/to prepend to/from append */
		    after = after ? 0 : 1;
		}
	    }

	    if(i == 0)
	      i = 2;

#ifdef OS2
	    strcpy(prompt, "Enter port or |command : ");
#else
	    strcpy(prompt, "Enter command for printer : ");
#endif
	    while(i != 0 && i != 1){
		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(sval, -FOOTER_ROWS(ps), 0, sizeof(sval),
				     prompt,
				     (ekey[0].ch != -1) ? ekey : NULL,
				     help, &oeflags);
		if(i == 0){
		    rv = ps->mangled_body = 1;
		    removing_leading_and_trailing_white_space(sval);
		    if(*sval || !lval){

			for(tmp = sval; *tmp; tmp++)
			  if(*tmp == ',')
			  i++;	/* conservative count of ,'s */

			if(!i){	/* only one item */
			  if (!ltmp){
			    ltmp = (char **)fs_get(2 * sizeof(char *));
			    ltmp[1] = NULL;
			    k = 0;
			  }
			  if(*name){
			    ltmp[k] = (char *)fs_get(strlen(name) + 4 + strlen(sval) + 1);
			    sprintf(ltmp[k], "%s [] %s", name, sval);
			  }
			  else
			    ltmp[k] = cpystr(sval);
			}
			else{
			    /*
			     * Don't allow input of multiple entries at once.
			     */
			    q_status_message(SM_ORDER,3,5,
				"No commas allowed in command");
			    i = 2;
			    continue;
			}

			config_add_list(ps, cl, ltmp, &newval, after);

			if(after)
			  skip_to_next = 1;

			fs_give((void **)&ltmp);
			k = 0;
		    }
		    else
		      q_status_message1(SM_ORDER, 0, 3,
					 "Can't add %.200s to list", empty_val);
		}
		else if(i == 1){
		    cmd_cancelled("Add");
		}
		else if(i == 3){
		    help = help == NO_HELP ? h_config_print_cmd : NO_HELP;
		}
		else if(i == 4){		/* no redraw, yet */
		}
		else if(i == 5){ /* change from/to prepend to/from append */
		    after = after ? 0 : 1;
		}
	    }
	}

	break;

      case MC_DELETE:					/* delete */
	if((*cl)->var->current_val.l
	  && (*cl)->var->current_val.l[(*cl)->varmem]
	  && !strucmp(ps->VAR_PRINTER,(*cl)->var->current_val.l[(*cl)->varmem]))
	    changing_selected = 1;

	if(!lval && nelval){
	    char pmt[80];

	    sprintf(pmt, "Override default with %.20s", empty_val2);
	    if(want_to(pmt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		char **ltmp;

		sval[0] = '\0';
		ltmp    = (char **)fs_get(2 * sizeof(char *));
		ltmp[0] = cpystr(sval);
		ltmp[1] = NULL;
		config_add_list(ps, cl, ltmp, &newval, 0);
		fs_give((void **)&ltmp);
		rv = ps->mangled_body = 1;
	    }
	}
	else if(!lval){
	    q_status_message(SM_ORDER, 0, 3, "No set value to delete");
	}
	else{
	    if((*cl)->var->is_fixed){
		parse_printer(lval[(*cl)->varmem],
		    &nick, &p, NULL, NULL, NULL, NULL);
	        sprintf(prompt, "Delete (unused) printer %.30s ",
		    *nick ? nick : (!*p) ? empty_val2 : p);
		fs_give((void **)&nick);
		fs_give((void **)&p);
	    }
	    else
	      sprintf(prompt, "Really delete item %.20s from printer list ",
		    int2string((*cl)->varmem + 1));

	    ps->mangled_footer = 1;
	    if(want_to(prompt,'n','n',h_config_print_del, WT_FLUSH_IN) == 'y'){
		rv = ps->mangled_body = 1;
		fs_give((void **)&lval[(*cl)->varmem]);
		config_del_list_item(cl, &newval);
	    }
	    else
	      q_status_message(SM_ORDER, 0, 3, "Printer not deleted");
	}

	break;

      case MC_EDIT:				/* edit/change list option */
	if((*cl)->var->current_val.l
	  && (*cl)->var->current_val.l[(*cl)->varmem]
	  && !strucmp(ps->VAR_PRINTER,(*cl)->var->current_val.l[(*cl)->varmem]))
	    changing_selected = 1;

	if(fixed_var((*cl)->var, NULL, "printer"))
	  break;
	else if(!lval &&  nelval)
	  goto replace_text;
	else if(!lval && !nelval)
	  goto add_text;
	else{
	    HelpType help;

	    ekey[0].ch    = 'n';
	    ekey[0].rval  = 'n';
	    ekey[0].name  = "N";
	    ekey[0].label = "Name";
	    ekey[1].ch    = 'c';
	    ekey[1].rval  = 'c';
	    ekey[1].name  = "C";
	    ekey[1].label = "Command";
	    ekey[2].ch    = 'o';
	    ekey[2].rval  = 'o';
	    ekey[2].name  = "O";
	    ekey[2].label = "Options";
	    ekey[3].ch    = -1;
	    strcpy(prompt, "Change Name or Command or Options ? ");
	    i = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, 'c', 'x',
			      h_config_print_name_cmd, RB_NORM, NULL);

	    if(i == 'x'){
		cmd_cancelled("Change");
		break;
	    } 
	    else if(i == 'c'){
		char *all_but_cmd;

		parse_printer(lval[(*cl)->varmem],
			      NULL, &p, NULL, NULL, NULL, &all_but_cmd);
		
		strcpy(prompt, "Change command : ");
		strncpy(sval, p ? p : "", sizeof(sval)-1);
		sval[sizeof(sval)-1] = '\0';
		fs_give((void **)&p);

		ps->mangled_footer = 1;
		help = NO_HELP;
		while(1){
		    oeflags = OE_APPEND_CURRENT;
		    i = optionally_enter(sval, -FOOTER_ROWS(ps), 0,
				         sizeof(sval), prompt, NULL,
					 help, &oeflags);
		    if(i == 0){
			removing_leading_and_trailing_white_space(sval);
			rv = ps->mangled_body = 1;
			if(lval[(*cl)->varmem])
			  fs_give((void **)&lval[(*cl)->varmem]);

			i = 0;
			for(tmp = sval; *tmp; tmp++)
			  if(*tmp == ',')
			    i++;	/* count of ,'s */

			if(!i){	/* only one item */
			    lval[(*cl)->varmem]
			      = (char *)fs_get(strlen(all_but_cmd) +
						strlen(sval) + 1);
			    strcpy(lval[(*cl)->varmem], all_but_cmd);
			    strcat(lval[(*cl)->varmem], sval);

			    newval = &(*cl)->value;
			}
			else{
			    /*
			     * Don't allow input of multiple entries at once.
			     */
			    q_status_message(SM_ORDER,3,5,
				"No commas allowed in command");
			    continue;
			}
		    }
		    else if(i == 1){
			cmd_cancelled("Change");
		    }
		    else if(i == 3){
			help = help == NO_HELP ? h_config_change : NO_HELP;
			continue;
		    }
		    else if(i == 4){		/* no redraw, yet */
			continue;
		    }

		    break;
		}
	    }
	    else if(i == 'n'){
		char *all_but_nick;

		parse_printer(lval[(*cl)->varmem],
			      &p, NULL, NULL, NULL, &all_but_nick, NULL);
		
		strcpy(prompt, "Change name : ");
		strncpy(name, p ? p : "", MAXPATH);
		fs_give((void **)&p);

		ps->mangled_footer = 1;
		help = NO_HELP;
		while(1){
		    oeflags = OE_APPEND_CURRENT;
		    i = optionally_enter(name, -FOOTER_ROWS(ps), 0,
					 sizeof(name), prompt, NULL,
					 help, &oeflags);
		    if(i == 0){
			rv = ps->mangled_body = 1;
			removing_leading_and_trailing_white_space(name);
			if(lval[(*cl)->varmem])
			  fs_give((void **)&lval[(*cl)->varmem]);

			lval[(*cl)->varmem] = (char *)fs_get(strlen(name) + 1
					+ ((*all_but_nick == '[') ? 0 : 3)
					+ strlen(all_but_nick) + 1);
			sprintf(lval[(*cl)->varmem],
			    "%s %s%s", name,
			    (*all_but_nick == '[') ? "" : "[] ",
			    all_but_nick);
			
			newval = &(*cl)->value;
		    }
		    else if(i == 1){
			cmd_cancelled("Change");
		    }
		    else if(i == 3){
			help = help == NO_HELP ? h_config_change : NO_HELP;
			continue;
		    }
		    else if(i == 4){		/* no redraw, yet */
			continue;
		    }

		    break;
		}
		
		fs_give((void **)&all_but_nick);
	    }
	    else if(i == 'o'){
		HelpType help;

		ekey[0].ch    = 'i';
		ekey[0].rval  = 'i';
		ekey[0].name  = "I";
		ekey[0].label = "Init";
		ekey[1].ch    = 't';
		ekey[1].rval  = 't';
		ekey[1].name  = "T";
		ekey[1].label = "Trailer";
		ekey[2].ch    = -1;
		strcpy(prompt, "Change Init string or Trailer string ? ");
		j = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, 'i', 'x',
				  h_config_print_opt_choice, RB_NORM, NULL);

		if(j == 'x'){
		    cmd_cancelled("Change");
		    break;
		} 
		else{
		    char *init, *trailer;

		    parse_printer(lval[(*cl)->varmem],
				  &nick, &p, &init, &trailer, NULL, NULL);
		    
		    sprintf(prompt, "Change %s string : ",
			(j == 'i') ? "INIT" : "TRAILER");
		    strncpy(sval, (j == 'i') ? init : trailer, sizeof(sval)-1);
		    sval[sizeof(sval)-1] = '\0';

		    tmp = string_to_cstring(sval);
		    strncpy(sval, tmp, sizeof(sval)-1);
		    sval[sizeof(sval)-1] = '\0';
		    fs_give((void **)&tmp);
		    
		    ps->mangled_footer = 1;
		    help = NO_HELP;
		    while(1){
			oeflags = OE_APPEND_CURRENT;
			i = optionally_enter(sval, -FOOTER_ROWS(ps), 0,
			    sizeof(sval), prompt, NULL, help, &oeflags);
			if(i == 0){
			    removing_leading_and_trailing_white_space(sval);
			    rv = 1;
			    if(lval[(*cl)->varmem])
			      fs_give((void **)&lval[(*cl)->varmem]);
			    if(j == 'i'){
				init = cstring_to_hexstring(sval);
				tmp = cstring_to_hexstring(trailer);
				fs_give((void **)&trailer);
				trailer = tmp;
			    }
			    else{
				trailer = cstring_to_hexstring(sval);
				tmp = cstring_to_hexstring(init);
				fs_give((void **)&init);
				init = tmp;
			    }

			    lval[(*cl)->varmem] = (char *)fs_get(strlen(nick) + 1
				  + 2 + strlen("INIT=") + strlen(init)
				  + 1 + strlen("TRAILER=") + strlen(trailer)
				  + 1 + strlen(p) + 1);
			    sprintf(lval[(*cl)->varmem],
				"%s%s%s%s%s%s%s%s%s%s%s",
	    /* nick */	    nick,
	    /* space */	    *nick ? " " : "",
	    /* [ */		    (*nick || *init || *trailer) ? "[" : "",
	    /* INIT= */	    *init ? "INIT=" : "",
	    /* init */	    init,
	    /* space */	    (*init && *trailer) ? " " : "",
	    /* TRAILER= */	    *trailer ? "TRAILER=" : "",
	    /* trailer */	    trailer,
	    /* ] */		    (*nick || *init || *trailer) ? "]" : "",
	    /* space */	    (*nick || *init || *trailer) ? " " : "",
	    /* command */	    p);
	    
			    newval = &(*cl)->value;
			}
			else if(i == 1){
			    cmd_cancelled("Change");
			}
			else if(i == 3){
			    help=(help == NO_HELP)?h_config_print_init:NO_HELP;
			    continue;
			}
			else if(i == 4){		/* no redraw, yet */
			    continue;
			}

			break;
		    }

		    fs_give((void **)&nick);
		    fs_give((void **)&p);
		    fs_give((void **)&init);
		    fs_give((void **)&trailer);
		}
	    }
	}

	break;

      case MC_EXIT:				/* exit */
	rv = config_exit_cmd(flags);
	break;

      default:
	rv = -1;
	break;
    }

    if(skip_to_next)
      *cl = next_confline(*cl);

    /*
     * At this point, if changes occurred, var->user_val.X is set.
     * So, fix the current_val, and handle special cases...
     */
    if(rv == 1){
	set_current_val((*cl)->var, TRUE, FALSE);
	fix_side_effects(ps, (*cl)->var, 0);

	if(newval){
	    if(*newval)
	      fs_give((void **)newval);
	    
	    if((*cl)->var->current_val.l)
	      *newval = printer_name((*cl)->var->current_val.l[(*cl)->varmem]);
	    else
	      *newval = cpystr("");
	}

	if(changing_selected)
	  print_select_tool(ps, MC_CHOICE, cl, flags);
    }

    return(rv);
}


/*
 * Manage display of the config/options menu body.
 */
void
update_option_screen(ps, screen, cursor_pos)
    struct pine  *ps;
    OPT_SCREEN_S *screen;
    Pos          *cursor_pos;
{
    int		   dline, save;
    CONF_S	  *top_line, *ctmp;
    char          *value;

#ifdef _WINDOWS
    int		   last_selectable;
    mswin_beginupdate();
#endif
    if(cursor_pos){
	cursor_pos->col = 0;
	cursor_pos->row = -1;		/* to tell us if we've set it yet */
    }

    /*
     * calculate top line of display for reframing if the current field
     * is off the display defined by screen->top_line...
     */
    if(ctmp = screen->top_line)
      for(dline = BODY_LINES(ps);
	  dline && ctmp && ctmp != screen->current;
	  ctmp = next_confline(ctmp), dline--)
	;

    if(!ctmp || !dline){		/* force reframing */
	dline = 0;
	ctmp = top_line = first_confline(screen->current);
	do
	  if(((dline++)%BODY_LINES(ps)) == 0)
	    top_line = ctmp;
	while(ctmp != screen->current && (ctmp = next_confline(ctmp)));
    }
    else
      top_line = screen->top_line;

#ifdef _WINDOWS
    /*
     * Figure out how far down the top line is from the top and how many
     * total lines there are.  Dumb to loop every time thru, but
     * there aren't that many lines, and it's cheaper than rewriting things
     * to maintain a line count in each structure...
     */
    for(dline = 0, ctmp = prev_confline(top_line); ctmp; ctmp = prev_confline(ctmp))
      dline++;

    scroll_setpos(dline);
    last_selectable = dline;
    for(ctmp = next_confline(top_line); ctmp ; ctmp = next_confline(ctmp)){
      dline++;
      if (!(ctmp->flags & CF_NOSELECT))
	last_selectable = dline;
    }
    dline = last_selectable;
    scroll_setrange(BODY_LINES(ps), dline);
#endif

    /* mangled body or new page, force redraw */
    if(ps->mangled_body || screen->top_line != top_line)
      screen->prev = NULL;

    /* loop thru painting what's needed */
    for(dline = 0, ctmp = top_line;
	dline < BODY_LINES(ps);
	dline++, ctmp = next_confline(ctmp)){

	/*
	 * only fall thru painting if something needs painting...
	 */
	if(!(!screen->prev || ctmp == screen->prev || ctmp == screen->current
	     || ctmp == screen->prev->varnamep
	     || ctmp == screen->current->varnamep
	     || ctmp == screen->prev->headingp
	     || ctmp == screen->current->headingp))
	  continue;

	ClearLine(dline + HEADER_ROWS(ps));

	if(ctmp){
	    if(ctmp->flags & CF_B_LINE)
	      continue;

	    if(ctmp->varname && !(ctmp->flags & CF_INVISIBLEVAR)){
		if(ctmp == screen->current && cursor_pos)
		  cursor_pos->row  = dline + HEADER_ROWS(ps);

		if((ctmp == screen->current
		    || ctmp == screen->current->varnamep
		    || ctmp == screen->current->headingp)
		   && !(ctmp->flags & CF_NOHILITE))
		  StartInverse();

		if(ctmp->flags & CF_H_LINE){
		    MoveCursor(dline + HEADER_ROWS(ps), 0);
		    Write_to_screen(repeat_char(ps->ttyo->screen_cols, '-'));
		}

		if(ctmp->flags & CF_CENTERED){
		    int offset = ps->ttyo->screen_cols/2
				  - (strlen(ctmp->varname)/2);
		    MoveCursor(dline + HEADER_ROWS(ps),
			       (offset > 0) ? offset : 0);
		}
		else if(ctmp->varoffset)
		  MoveCursor(dline+HEADER_ROWS(ps), ctmp->varoffset);

		Write_to_screen(ctmp->varname);
		if((ctmp == screen->current
		    || ctmp == screen->current->varnamep
		    || ctmp == screen->current->headingp)
		   && !(ctmp->flags & CF_NOHILITE))
		  EndInverse();
	    }

	    value = (ctmp->flags & CF_INHERIT) ? INHERIT : ctmp->value;
	    if(value){
		char *p;
		int   i, j;

		memset(tmp_20k_buf, '\0',
		       (ps->ttyo->screen_cols + 1) * sizeof(char));
		if(ctmp == screen->current){
		    if(!(ctmp->flags & CF_DOUBLEVAR && ctmp->flags & CF_VAR2))
		      StartInverse();

		    if(cursor_pos)
		      cursor_pos->row  = dline + HEADER_ROWS(ps);
		}

		if(ctmp->flags & CF_H_LINE)
		  memset(tmp_20k_buf, '-',
			 ps->ttyo->screen_cols * sizeof(char));

		if(ctmp->flags & CF_CENTERED){
		    int offset = ps->ttyo->screen_cols/2
				  - (strlen(value)/2);
		    /* BUG: tabs screw us figuring length above */
		    if(offset > 0){
			char *q;

			p = tmp_20k_buf + offset;
			if(!*(q = tmp_20k_buf))
			  while(q < p)
			    *q++ = ' ';
		    }
		}
		else
		  p = tmp_20k_buf;

		/*
		 * Copy the value to a temp buffer expanding tabs, and
		 * making sure not to write beyond screen right...
		 */
		for(i = 0, j = ctmp->valoffset;
		    value[i] && j < ps->ttyo->screen_cols;
		    i++){
		    if(value[i] == ctrl('I')){
			do
			  *p++ = ' ';
			while(j < ps_global->ttyo->screen_cols
			      && ((++j) & 0x07));
		    }
		    else{
			*p++ = value[i];
			j++;
		    }
		}

		if(ctmp == screen->current && cursor_pos){
		    if(ctmp->flags & CF_DOUBLEVAR && ctmp->flags & CF_VAR2)
		      cursor_pos->col = ctmp->val2offset;
		    else
		      cursor_pos->col = ctmp->valoffset;

		    if(ctmp->tool == radiobutton_tool
#ifdef	ENABLE_LDAP
		       || ctmp->tool==ldap_radiobutton_tool
#endif
		       || ctmp->tool==role_radiobutton_tool
		       || ctmp->tool==checkbox_tool
		       || (ctmp->tool==color_setting_tool &&
			   ctmp->valoffset != COLOR_INDENT))
		      cursor_pos->col++;
		}

		if(ctmp->flags & CF_DOUBLEVAR){
		    long l;

		    p = tmp_20k_buf;
		    if((l=strlen(p)) > ctmp->val2offset - ctmp->valoffset
				       - SPACE_BETWEEN_DOUBLEVARS &&
		       ctmp->val2offset - ctmp->valoffset
			- SPACE_BETWEEN_DOUBLEVARS >= 0){
			save = p[ctmp->val2offset - ctmp->valoffset
			         - SPACE_BETWEEN_DOUBLEVARS];
			p[ctmp->val2offset - ctmp->valoffset
			  - SPACE_BETWEEN_DOUBLEVARS] = '\0';
		    }
		    else
		      save = '\0';

		    /*
		     * If this is a COLOR_BLOB line we do special coloring.
		     * The current object inverse hilite is only on the
		     * checkbox part, the exact format comes from the
		     * new_color_line function. If we change that we'll have
		     * to change this to get the coloring right.
		     */
		    if(p[0] == '(' && p[2] == ')' &&
		       p[3] == ' ' && p[4] == ' ' &&
		       !strncmp(p+5, COLOR_BLOB, COLOR_BLOB_LEN)){
			COLOR_PAIR  *lastc = NULL, *newc = NULL;

			MoveCursor(dline+HEADER_ROWS(ps), ctmp->valoffset);
			Write_to_screen_n(p, 3);
			if(!(ctmp->flags & CF_VAR2) && ctmp == screen->current)
			  EndInverse();

			Write_to_screen_n(p+3, 3);
			newc = new_color_pair(colorx(CFC_ICOLOR(ctmp)),
					      colorx(CFC_ICOLOR(ctmp)));
			if(newc){
			    lastc = pico_get_cur_color();
			    (void)pico_set_colorp(newc, PSC_NONE);
			    free_color_pair(&newc);
			}

			Write_to_screen_n(p+6, COLOR_BLOB_LEN-2);

			if(lastc){
			    (void)pico_set_colorp(lastc, PSC_NONE);
			    free_color_pair(&lastc);
			}

			Write_to_screen(p+6+COLOR_BLOB_LEN-2);
		    }
		    else{
			PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset, p);
			if(!(ctmp->flags & CF_VAR2) && ctmp == screen->current)
			  EndInverse();
		    }

		    if(save)
		      p[ctmp->val2offset - ctmp->valoffset
			- SPACE_BETWEEN_DOUBLEVARS] = save;

		    PutLine0(dline+HEADER_ROWS(ps),
			     ctmp->val2offset - SPACE_BETWEEN_DOUBLEVARS,
			     repeat_char(SPACE_BETWEEN_DOUBLEVARS, SPACE));

		    if(l > ctmp->val2offset - ctmp->valoffset &&
		           ctmp->val2offset - ctmp->valoffset >= 0)
		      p += (ctmp->val2offset - ctmp->valoffset);

		    if(p > tmp_20k_buf){
			if(ctmp->flags & CF_VAR2 && ctmp == screen->current)
			  StartInverse();

			if(p[0] == '(' && p[2] == ')' &&
			   p[3] == ' ' && p[4] == ' ' &&
			   !strncmp(p+5, COLOR_BLOB, COLOR_BLOB_LEN)){
			    COLOR_PAIR  *lastc = NULL, *newc = NULL;

			    MoveCursor(dline+HEADER_ROWS(ps), ctmp->val2offset);
			    Write_to_screen_n(p, 3);
			    if(ctmp->flags & CF_VAR2 && ctmp == screen->current)
			      EndInverse();

			    Write_to_screen_n(p+3, 3);
			    newc = new_color_pair(colorx(CFC_ICOLOR(ctmp)),
						  colorx(CFC_ICOLOR(ctmp)));
			    if(newc){
				lastc = pico_get_cur_color();
				(void)pico_set_colorp(newc, PSC_NONE);
				free_color_pair(&newc);
			    }

			    Write_to_screen_n(p+6, COLOR_BLOB_LEN-2);

			    if(lastc){
				(void)pico_set_colorp(lastc, PSC_NONE);
				free_color_pair(&lastc);
			    }

			    Write_to_screen(p+6+COLOR_BLOB_LEN-2);
			}
			else{
			    PutLine0(dline+HEADER_ROWS(ps),ctmp->val2offset,p);
			    if(ctmp->flags & CF_VAR2 && ctmp == screen->current)
			      EndInverse();
			}
		    }
		}
		else{
		    char       *q, *first_space, *sample, *ptr;
		    COLOR_PAIR *lastc, *newc;
		    int         invert;


		    if(ctmp->flags & CF_COLORSAMPLE &&
		       pico_usingcolor() &&
		       ((q = strstr(tmp_20k_buf, SAMPLE_LEADER)) ||
			(q = strstr(tmp_20k_buf, "Color"))) &&
		       (first_space = strindex(q, SPACE)) &&
		       (strstr(value, SAMP1) ||
		        strstr(value, SAMP2))){

			ptr = tmp_20k_buf;

			/* write out first part */
			*first_space = '\0';
			PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset,
				 ptr);
			*first_space = SPACE;
			ptr = first_space;

			if(ctmp == screen->current)
			  EndInverse();

			sample = skip_white_space(ptr);
			/* if there's enough room to put some sample up */
			if(ctmp->valoffset + sample - tmp_20k_buf <
							ps->ttyo->screen_cols){
			    sample++;	/* for `[' at edge of sample */
			    save = *sample;
			    *sample = '\0';
			    /* spaces and bracket before sample1 */
			    PutLine0(dline+HEADER_ROWS(ps),
				     ctmp->valoffset+ptr-tmp_20k_buf,
				     ptr);
			    *sample = save;
			    ptr = sample;

			    /* then the color sample */
			    if(ctmp->var == &ps->vars[V_VIEW_HDR_COLORS]){
				SPEC_COLOR_S *hc, *hcolors;

				lastc = newc = NULL;

				hcolors =
				  spec_colors_from_varlist(LVAL(ctmp->var, ew),
							   0);
				for(hc = hcolors, i=0; hc; hc = hc->next, i++)
				  if(CFC_ICUST(ctmp) == i)
				    break;

				if(hc && hc->fg && hc->fg[0] && hc->bg &&
				   hc->bg[0])
				  newc = new_color_pair(hc->fg, hc->bg);

				if(newc){
				    lastc = pico_get_cur_color();
				    (void)pico_set_colorp(newc, PSC_NONE);
				    free_color_pair(&newc);
				}

				if(hcolors)
				  free_spec_colors(&hcolors);

				/* print out sample1 */
				save = tmp_20k_buf[min(ps->ttyo->screen_cols -
						         ctmp->valoffset,
						       ptr - tmp_20k_buf + 
							 (SAMPLE_LEN-2))];
				tmp_20k_buf[min(ps->ttyo->screen_cols -
						  ctmp->valoffset,
					        ptr - tmp_20k_buf + 
						  (SAMPLE_LEN-2))] = '\0';
				PutLine0(dline+HEADER_ROWS(ps),
					 ctmp->valoffset + ptr - tmp_20k_buf,
					 ptr);
				tmp_20k_buf[min(ps->ttyo->screen_cols -
						  ctmp->valoffset,
					        ptr - tmp_20k_buf + 
						  (SAMPLE_LEN-2))] = save;
				ptr += (SAMPLE_LEN-2);
				if(lastc){
				    (void)pico_set_colorp(lastc, PSC_NONE);
				    free_color_pair(&lastc);
				}
			    }
			    else if(ctmp->var == &ps->vars[V_KW_COLORS]){
				KEYWORD_S    *kw;
				SPEC_COLOR_S *kw_col = NULL;

				lastc = newc = NULL;

				/* find keyword associated with this line */
				for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
				  if(CFC_ICUST(ctmp) == i)
				    break;

				if(kw)
				  kw_col =
				   spec_colors_from_varlist(LVAL(ctmp->var,ew),
							    0);

				/* color for this keyword */
				if(kw && kw_col
				   && ((kw->nick && kw->nick[0]
				        && (newc=hdr_color(kw->nick, NULL,
							   kw_col)))
				       ||
				       (kw->kw && kw->kw[0]
				        && (newc=hdr_color(kw->kw, NULL,
							   kw_col))))){
				    lastc = pico_get_cur_color();
				    (void)pico_set_colorp(newc, PSC_NONE);
				    free_color_pair(&newc);
				}

				if(kw_col)
				  free_spec_colors(&kw_col);

				/* print out sample1 */
				save = tmp_20k_buf[min(ps->ttyo->screen_cols -
						         ctmp->valoffset,
						       ptr - tmp_20k_buf + 
							 (SAMPLE_LEN-2))];
				tmp_20k_buf[min(ps->ttyo->screen_cols -
						  ctmp->valoffset,
					        ptr - tmp_20k_buf + 
						  (SAMPLE_LEN-2))] = '\0';
				PutLine0(dline+HEADER_ROWS(ps),
					 ctmp->valoffset + ptr - tmp_20k_buf,
					 ptr);
				tmp_20k_buf[min(ps->ttyo->screen_cols -
						  ctmp->valoffset,
					        ptr - tmp_20k_buf + 
						  (SAMPLE_LEN-2))] = save;
				ptr += (SAMPLE_LEN-2);
				if(lastc){
				    (void)pico_set_colorp(lastc, PSC_NONE);
				    free_color_pair(&lastc);
				}
			    }
			    else{
				lastc = NULL;
				invert = 0;
				newc = sample_color(ps, ctmp->var);
				if(newc){
				    if(lastc = pico_get_cur_color())
				      (void)pico_set_colorp(newc, PSC_NONE);

				    free_color_pair(&newc);
				}
				else if(var_defaults_to_rev(ctmp->var)){
				    if(newc = pico_get_rev_color()){
					/*
					 * Note, don't have to free newc.
					 */
					if(lastc = pico_get_cur_color())
					  (void)pico_set_colorp(newc, PSC_NONE);
				    }
				    else{
					StartInverse();
					invert = 1;
				    }
				}

				if(ctmp->var==&ps->vars[V_SLCTBL_FORE_COLOR] &&

				   (F_OFF(F_SLCTBL_ITEM_NOBOLD, ps) ||
				    !(PVAL(ctmp->var,ew) &&
				      PVAL(ctmp->var+1,ew))))
				  StartBold();

				/* print out sample1 */
				save = tmp_20k_buf[min(ps->ttyo->screen_cols -
						         ctmp->valoffset,
						       ptr - tmp_20k_buf + 
							 (SAMPLE_LEN-2))];
				tmp_20k_buf[min(ps->ttyo->screen_cols -
						  ctmp->valoffset,
					        ptr - tmp_20k_buf + 
						  (SAMPLE_LEN-2))] = '\0';
				PutLine0(dline+HEADER_ROWS(ps),
					 ctmp->valoffset + ptr - tmp_20k_buf,
					 ptr);
				tmp_20k_buf[min(ps->ttyo->screen_cols -
						  ctmp->valoffset,
					        ptr - tmp_20k_buf + 
						  (SAMPLE_LEN-2))] = save;
				ptr += (SAMPLE_LEN-2);

				if(ctmp->var==&ps->vars[V_SLCTBL_FORE_COLOR] &&

				   (F_OFF(F_SLCTBL_ITEM_NOBOLD, ps) ||
				    !(PVAL(ctmp->var,ew) &&
				      PVAL(ctmp->var+1,ew))))
				  EndBold();

				if(lastc){
				    (void)pico_set_colorp(lastc, PSC_NONE);
				    free_color_pair(&lastc);
				}
				else if(invert)
				  EndInverse();
			    }

			    /*
			     * Finish sample1 with the right bracket.
			     */
			    if(ctmp->valoffset + ptr - tmp_20k_buf <
			       ps->ttyo->screen_cols){
				save = tmp_20k_buf[ptr - tmp_20k_buf + 1];
				tmp_20k_buf[ptr - tmp_20k_buf + 1] = '\0';
				PutLine0(dline+HEADER_ROWS(ps),
					 ctmp->valoffset + ptr - tmp_20k_buf,
					 ptr);
				tmp_20k_buf[ptr - tmp_20k_buf + 1] = save;
				ptr++;
			    }

			    /*
			     * Now check for an exception sample and paint it.
			     */
			    if(ctmp->valoffset + ptr - tmp_20k_buf + SBS + 1 <
			       ps->ttyo->screen_cols &&
			       (q = strstr(ptr, SAMPEXC))){
				/* spaces + `[' */
				save = ptr[SBS+1];
				ptr[SBS+1] = '\0';
				PutLine0(dline+HEADER_ROWS(ps),
					 ctmp->valoffset + ptr - tmp_20k_buf,
					 ptr);
				ptr[SBS+1] = save;
				ptr += (SBS+1);

				/*
				 * Figure out what color to paint it.
				 * This only happens with normal variables,
				 * not with V_VIEW_HDR_COLORS.
				 */
				lastc = NULL;
				invert = 0;
				newc = sampleexc_color(ps, ctmp->var);
				if(newc){
				    if(lastc = pico_get_cur_color())
				      (void)pico_set_colorp(newc, PSC_NONE);

				    free_color_pair(&newc);
				}
				else if(var_defaults_to_rev(ctmp->var)){
				    if(newc = pico_get_rev_color()){
					/*
					 * Note, don't have to free newc.
					 */
					if(lastc = pico_get_cur_color())
					  (void)pico_set_colorp(newc, PSC_NONE);
				    }
				    else{
					StartInverse();
					invert = 1;
				    }
				}

				if(ctmp->var==&ps->vars[V_SLCTBL_FORE_COLOR] &&

				   (F_OFF(F_SLCTBL_ITEM_NOBOLD, ps) ||
				    !(PVAL(ctmp->var,Post) &&
				      PVAL(ctmp->var+1,Post))))
				  StartBold();

				/* sample2 */
				save = tmp_20k_buf[min(ps->ttyo->screen_cols -
						         ctmp->valoffset,
						       ptr - tmp_20k_buf + 
							 (SAMPEXC_LEN-2))];
				tmp_20k_buf[min(ps->ttyo->screen_cols -
						  ctmp->valoffset,
					        ptr - tmp_20k_buf + 
						  (SAMPEXC_LEN-2))] = '\0';
				PutLine0(dline+HEADER_ROWS(ps),
					 ctmp->valoffset + ptr - tmp_20k_buf,
					 ptr);
				tmp_20k_buf[min(ps->ttyo->screen_cols -
						  ctmp->valoffset,
					        ptr - tmp_20k_buf + 
						  (SAMPEXC_LEN-2))] = save;
				ptr += (SAMPEXC_LEN-2);

				/* turn off bold and color */
				if(ctmp->var==&ps->vars[V_SLCTBL_FORE_COLOR] &&

				   (F_OFF(F_SLCTBL_ITEM_NOBOLD, ps) ||
				    !(PVAL(ctmp->var,Post) &&
				      PVAL(ctmp->var+1,Post))))
				  EndBold();

				if(lastc){
				    (void)pico_set_colorp(lastc, PSC_NONE);
				    free_color_pair(&lastc);
				}
				else if(invert)
				  EndInverse();

				/*
				 * Finish sample2 with the right bracket.
				 */
				if(ctmp->valoffset + ptr - tmp_20k_buf <
				   ps->ttyo->screen_cols){
				    save = tmp_20k_buf[ptr - tmp_20k_buf + 1];
				    tmp_20k_buf[ptr - tmp_20k_buf + 1] = '\0';
				    PutLine0(dline+HEADER_ROWS(ps),
					     ctmp->valoffset + ptr -
					       tmp_20k_buf,
					     ptr);
				    tmp_20k_buf[ptr - tmp_20k_buf + 1] = save;
				    ptr++;
				}
			    }

			    /* paint rest of the line if there is any left */
			    if(ctmp->valoffset + ptr - tmp_20k_buf <
			       ps->ttyo->screen_cols &&
			       *ptr){
				tmp_20k_buf[ps->ttyo->screen_cols -
					      ctmp->valoffset] = '\0';
				PutLine0(dline+HEADER_ROWS(ps),
				         ctmp->valoffset + ptr - tmp_20k_buf,
				         ptr);
			    }
			}
		    }
		    else{
			tmp_20k_buf[ps->ttyo->screen_cols -
				      ctmp->valoffset] = '\0';
			PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset,
				 tmp_20k_buf);
			if(ctmp == screen->current)
			  EndInverse();
		    }
		}
	    }
	}
    }

    ps->mangled_body = 0;
    screen->top_line = top_line;
    screen->prev     = screen->current;
#ifdef _WINDOWS
    mswin_endupdate();
#endif
}



/*
 * 
 */
void
print_option_screen(screen, prompt)
    OPT_SCREEN_S *screen;
    char *prompt;
{
    CONF_S *ctmp;
    int     so_far;
    char    line[500];

    if(open_printer(prompt) == 0){
	for(ctmp = first_confline(screen->current);
	    ctmp;
	    ctmp = next_confline(ctmp)){

	    so_far = 0;
	    if(ctmp->varname && !(ctmp->flags & CF_INVISIBLEVAR)){

		sprintf(line, "%*.200s%.200s", ctmp->varoffset, "",
			ctmp->varname);
		print_text(line);
		so_far = ctmp->varoffset + strlen(ctmp->varname);
	    }

	    if(ctmp && ctmp->value){
		char *p = tmp_20k_buf;
		int   i, j, spaces;

		/* Copy the value to a temp buffer expanding tabs. */
		for(i = 0, j = ctmp->valoffset; ctmp->value[i]; i++){
		    if(ctmp->value[i] == ctrl('I')){
			do
			  *p++ = ' ';
			while((++j) & 0x07);
			      
		    }
		    else{
			*p++ = ctmp->value[i];
			j++;
		    }
		}

		*p = '\0';
		removing_trailing_white_space(tmp_20k_buf);

		spaces = max(ctmp->valoffset - so_far, 0);
		sprintf(line, "%*.200s%.200s\n", spaces, "", tmp_20k_buf);
		print_text(line);
	    }
	}

	close_printer();
    }
}



/*
 *
 */
void
option_screen_redrawer()
{
    ps_global->mangled_body = 1;
    update_option_screen(ps_global, opt_screen, (Pos *)NULL);
}



/*
 * pretty_value - given the line, return an
 *                alloc'd string for line's value...
 */
char *
pretty_value(ps, cl)
    struct pine *ps;
    CONF_S      *cl;
{
    struct variable *v;

    v = cl->var;

    if(v == &ps->vars[V_FEATURE_LIST])
      return(checkbox_pretty_value(ps, cl));
    else if(standard_radio_var(ps, v) || v == startup_ptr)
      return(radio_pretty_value(ps, cl));
    else if(v == &ps->vars[V_SORT_KEY])
      return(sort_pretty_value(ps, cl));
    else if(v == &ps->vars[V_SIGNATURE_FILE])
      return(sigfile_pretty_value(ps, cl));
    else if(v == &ps->vars[V_USE_ONLY_DOMAIN_NAME])
      return(yesno_pretty_value(ps, cl));
    else if(color_holding_var(ps, v))
      return(color_pretty_value(ps, cl));
    else
      return(text_pretty_value(ps, cl));
}


char *
text_pretty_value(ps, cl)
    struct pine *ps;
    CONF_S      *cl;
{
    char  tmp[MAX_SCREEN_COLS+20], *pvalnorm, **lvalnorm, *pvalexc, **lvalexc;
    char *p, *pval, **lval, lastchar = '\0';
    char *left_paren = NULL, *left_quote = NULL;
    int   editing_except, fixed, uvalset, uvalposlen;
    int   comments, except_set, avail_width;
    int   norm_with_except = 0, norm_with_except_inherit = 0;
    int   inherit_line = 0;

    editing_except = (ew == ps_global->ew_for_except_vars);
    fixed = cl->var->is_fixed;
    if((ps_global->ew_for_except_vars != Main) && (ew == Main))
      norm_with_except++;	/* editing normal and except config exists */

    if(cl->var->is_list){
	lvalnorm   = LVAL(cl->var, Main);
	lvalexc    = LVAL(cl->var, ps_global->ew_for_except_vars);
	if(editing_except){
	    uvalset    = lvalexc != NULL;
	    uvalposlen = uvalset && lvalexc[0] && lvalexc[0][0];
	    lval       = lvalexc;
	}
	else{
	    uvalset    = lvalnorm != NULL;
	    uvalposlen = uvalset && lvalnorm[0] && lvalnorm[0][0];
	    lval       = lvalnorm;
	}

	except_set = lvalexc != NULL;
	comments = cl->var->current_val.l != NULL;
	if(norm_with_except && except_set && lvalexc[0] &&
	   !strcmp(lvalexc[0],INHERIT))
	  norm_with_except_inherit++;

	if(uvalset && !strcmp(lval[0], INHERIT)){
	    if(cl->varmem == 0){
		inherit_line++;
		comments = 0;
	    }
	}

	/* only add extra comments on last member of list */
	if(uvalset && !inherit_line && lval && lval[cl->varmem] &&
	   lval[cl->varmem + 1])
	  comments = 0;
    }
    else{
	pvalnorm   = PVAL(cl->var, Main);
	pvalexc    = PVAL(cl->var, ps_global->ew_for_except_vars);
	if(editing_except){
	    uvalset    = pvalexc != NULL;
	    uvalposlen = uvalset && *pvalexc;
	    pval       = pvalexc;
	}
	else{
	    uvalset    = pvalnorm != NULL;
	    uvalposlen = uvalset && *pvalnorm;
	    pval       = pvalnorm;
	}

	except_set = pvalexc != NULL;
	comments   = cl->var->current_val.p != NULL;
    }

    p = tmp;
    *p = '\0';

    avail_width = ps->ttyo->screen_cols - cl->valoffset;

    if(fixed || !uvalset || !uvalposlen)
      sstrncpy(&p, "<", avail_width-(p-tmp));

    if(fixed)
      sstrncpy(&p, fixed_val, avail_width-(p-tmp));
    else if(!uvalset)
      sstrncpy(&p, no_val, avail_width-(p-tmp));
    else if(!uvalposlen)
      sstrncpy(&p, empty_val, avail_width-(p-tmp));
    else if(inherit_line)
      sstrncpy(&p, INHERIT, avail_width-(p-tmp));
    else{
	if(cl->var->is_list)
	  sstrncpy(&p, lval[cl->varmem], avail_width+1-(p-tmp));
	else
	  sstrncpy(&p, pval, avail_width+1-(p-tmp));
    }

    if(comments && (fixed || !uvalset ||
       (norm_with_except && except_set))){
	if(fixed || !uvalset)
	  sstrncpy(&p, ": using ", avail_width-(p-tmp));

	if(norm_with_except && except_set){
	    if(!uvalset)
	      sstrncpy(&p, "exception ", avail_width-(p-tmp));
	    else if(!fixed){
		if(!uvalposlen)
		  sstrncpy(&p, ": ", avail_width-(p-tmp));
		else{
		    left_paren = p+1;
		    sstrncpy(&p, " (", avail_width-(p-tmp));
		}

		if(norm_with_except_inherit)
		  sstrncpy(&p, "added to by exception ", avail_width-(p-tmp));
		else
		  sstrncpy(&p, "overridden by exception ", avail_width-(p-tmp));
	    }
	}

	if(avail_width-(p-tmp) >= 7){
	    left_quote = p;
	    sstrcpy(&p, "\"");
	    if(cl->var->is_list){
		char **the_list;

		the_list = cl->var->current_val.l;

		if(norm_with_except && except_set)
		  the_list = lvalexc;
		
		if(the_list && the_list[0] && !strcmp(the_list[0], INHERIT))
		  the_list++;

		for(lval = the_list; avail_width-(p-tmp) > 0 && *lval; lval++){
		    if(lval != the_list)
		      sstrncpy(&p, ",", avail_width-(p-tmp));

		    sstrncpy(&p, *lval, max(avail_width+1-(p-tmp),1));
		}
	    }
	    else
	      sstrncpy(&p, cl->var->current_val.p, avail_width+1-(p-tmp));

	    *p++ = '\"';
	    *p = '\0';
	}
	else if(*(p-1) == SPACE)
	  *--p = '\0';
    }

    if(fixed || !uvalset || !uvalposlen)
      lastchar = '>';
    else if(comments && norm_with_except && except_set)
      lastchar = ')';

    if(lastchar){
	*p++ = lastchar;
	*p = '\0';
    }

    tmp[avail_width+1] = '\0';

    if((int)strlen(tmp) > avail_width){
	if(left_quote){
	    if(left_quote - tmp < avail_width - 5)
	      p = tmp + avail_width;
	    else
	      p = left_quote + 2;

	    *p-- = '\0';
	    *p-- = lastchar;
	    if(left_quote - tmp < avail_width - 5)
	      *p-- = '"';

	    *p-- = '.';
	    *p-- = '.';
	    *p   = '.';
	}
    }

    if(left_paren && (left_paren - tmp) > avail_width - 6){
	lastchar = *(left_paren - 1) = '\0';
    }

    if((int)strlen(tmp) > avail_width){
	p = tmp + avail_width;
	*p-- = '\0';

	if(lastchar)
	  *p-- = lastchar;
	
	*p-- = '.';
	*p-- = '.';
	*p   = '.';
    }

    if((int)strlen(tmp) < avail_width)
      sprintf(tmp+strlen(tmp), "%*s", avail_width-strlen(tmp), "");

    return(cpystr(tmp));
}


char *
sigfile_pretty_value(ps, cl)
    struct pine *ps;
    CONF_S      *cl;
{
    if(cl && cl->var == &ps->vars[V_SIGNATURE_FILE] &&
       cl->prev && cl->prev->var == &ps->vars[V_LITERAL_SIG]){
	if(cl->prev->var->current_val.p){
	    cl->flags |= CF_NOSELECT;		/* side effect */
	    return(cpystr("<Ignored: using literal-signature instead>"));
	}
	else{
	    cl->flags &= ~CF_NOSELECT;
	    return(text_pretty_value(ps, cl));
	}
    }
    else
      return(cpystr(""));
}


int
feature_gets_an_x(ps, cl, comment)
    struct pine     *ps;
    CONF_S          *cl;
    char           **comment;
{
    char           **lval, **lvalexc, **lvalnorm;
    char            *def = "  (default)";
    int              j, done = 0;
    FEATURE_S	    *feature;
    int              feature_fixed_on = 0, feature_fixed_off = 0;

    if(comment)
      *comment = NULL;

    lval  = LVAL(cl->var, ew);
    lvalexc  = LVAL(cl->var, ps_global->ew_for_except_vars);
    lvalnorm = LVAL(cl->var, Main);
    feature = feature_list(cl->varmem);

    /* feature value is administratively fixed */
    if(j = feature_in_list(cl->var->fixed_val.l, feature->name)){
	if(j == 1)
	  feature_fixed_on++;
	else if(j == -1)
	  feature_fixed_off++;

	done++;
	if(comment)
	  *comment = "  (fixed)";
    }

    /*
     * We have an exceptions config setting which overrides anything
     * we do here, in the normal config.
     */
    if(!done &&
       ps_global->ew_for_except_vars != Main && ew == Main &&
       feature_in_list(lvalexc, feature->name)){
	done++;
	if(comment)
	  *comment = "  (overridden)";
    }

    /*
     * Feature is set On in default but not set here.
     */
    if(!done &&
       !feature_in_list(lval, feature->name) &&
       ((feature_in_list(cl->var->global_val.l, feature->name) == 1) ||
        ((ps_global->ew_for_except_vars != Main &&
          ew == ps_global->ew_for_except_vars &&
          feature_in_list(lvalnorm, feature->name) == 1)))){
	done = 17;
	if(comment)
	  *comment = def;
    }

    /*
     * Feature allow-changing-from is on by default.
     * Tests say it is not in the list we're editing, and,
     * is not in the global_val list, and,
     * if we're editing an except which is not the normal then it is also
     * not in the normal list.
     * So it isn't set anywhere which means it is on because of the special
     * default value for this feature.
     */
    if(!done &&
       feature->id == F_ALLOW_CHANGING_FROM &&
       !feature_in_list(lval, feature->name) &&
       !feature_in_list(cl->var->global_val.l, feature->name) &&
       (ps_global->ew_for_except_vars == Main ||
        ew != ps_global->ew_for_except_vars ||
        !feature_in_list(lvalnorm, feature->name))){
	done = 17;
	if(comment)
	  *comment = def;
    }

    return(feature_fixed_on ||
	   (!feature_fixed_off &&
	    (done == 17 ||
	     test_feature(lval, feature->name,
			  test_old_growth_bits(ps, feature->id)))));
}


char *
checkbox_pretty_value(ps, cl)
    struct pine     *ps;
    CONF_S          *cl;
{
    char             tmp[MAXPATH];
    char            *comment = NULL;
    int              i, j, lv, x;
    FEATURE_S	    *feature;

    tmp[0] = '\0';

    /* find longest value's name */
    for(lv = 0, i = 0; feature = feature_list(i); i++)
      if(feature_list_section(feature)
	 && lv < (j = strlen(feature->name)))
	lv = j;

    lv = min(lv, 100);

    feature = feature_list(cl->varmem);

    x = feature_gets_an_x(ps, cl, &comment);

    sprintf(tmp, "[%c]  %-*.*s%.50s", x ? 'X' : ' ',
	    lv, lv, feature->name, comment ? comment : "");

    return(cpystr(tmp));
}


char *
yesno_pretty_value(ps, cl)
    struct pine *ps;
    CONF_S      *cl;
{
    char  tmp[MAXPATH], *pvalnorm, *pvalexc;
    char *p, *pval, lastchar = '\0';
    int   editing_except, fixed, norm_with_except, uvalset;
    int   curval, except_set;

    editing_except = (ew == ps_global->ew_for_except_vars);
    fixed = cl->var->is_fixed;
    if((ps_global->ew_for_except_vars == Main) ||
       (ew == ps_global->ew_for_except_vars))
      norm_with_except = 0;
    else
      norm_with_except = 1;	/* editing normal and except config exists */

    pvalnorm   = PVAL(cl->var, Main);
    pvalexc    = PVAL(cl->var, ps_global->ew_for_except_vars);
    if(editing_except){
	uvalset    = (pvalexc != NULL &&
		      (!strucmp(pvalexc,yesstr) || !strucmp(pvalexc,nostr)));
	pval       = pvalexc;
    }
    else{
	uvalset    = (pvalnorm != NULL &&
		      (!strucmp(pvalnorm,yesstr) || !strucmp(pvalnorm,nostr)));
	pval       = pvalnorm;
    }

    except_set = (pvalexc != NULL &&
		  (!strucmp(pvalexc,yesstr) || !strucmp(pvalexc,nostr)));
    curval  = (cl->var->current_val.p != NULL &&
	       (!strucmp(cl->var->current_val.p,yesstr) ||
	        !strucmp(cl->var->current_val.p,nostr)));

    p = tmp;
    *p = '\0';

    if(fixed || !uvalset)
      sstrcpy(&p, "<");

    if(fixed)
      sstrcpy(&p, fixed_val);
    else if(!uvalset)
      sstrcpy(&p, no_val);
    else if(!strucmp(pval, yesstr))
      sstrcpy(&p, yesstr);
    else
      sstrcpy(&p, nostr);

    if(curval && (fixed || !uvalset || (norm_with_except && except_set))){
	if(fixed || !uvalset)
	  sstrcpy(&p, ": using ");

	if(norm_with_except && except_set){
	    if(!uvalset)
	      sstrcpy(&p, "exception ");
	    else if(!fixed){
		sstrcpy(&p, " (");
		sstrcpy(&p, "overridden by exception ");
	    }
	}

	sstrcpy(&p, "\"");
	sstrcpy(&p, !strucmp(cl->var->current_val.p,yesstr) ? yesstr : nostr);
	sstrcpy(&p, "\"");
    }

    if(fixed || !uvalset)
      lastchar = '>';
    else if(curval && norm_with_except && except_set)
      lastchar = ')';

    if(lastchar){
	*p++ = lastchar;
	*p = '\0';
    }

    if((int)strlen(tmp) < ps->ttyo->screen_cols - cl->valoffset)
      sprintf(tmp+strlen(tmp),
	      "%*s", ps->ttyo->screen_cols - cl->valoffset - strlen(tmp), "");

    return(cpystr(tmp));
}


char *
color_pretty_value(ps, cl)
    struct pine *ps;
    CONF_S      *cl;
{
    char             tmp[MAXPATH];
    char            *p, *q;
    struct variable *v;
    int              is_index;

    tmp[0] = '\0';
    v = cl->var;

    if(v && color_holding_var(ps, v) &&
       (p=srchstr(v->name, "-foreground-color"))){

	is_index = !struncmp(v->name, "index-", 6);

	q = sampleexc_text(ps, v);
	sprintf(tmp, "%c%.*s %sColor%*.50s %.20s%*s%.20s%.20s",
		islower((unsigned char)v->name[0])
					? toupper((unsigned char)v->name[0])
					: v->name[0],
		min(p-v->name-1,30),
		v->name+1,
		is_index ? "Symbol " : "",
		max(EQ_COL - COLOR_INDENT -1 - min(p-v->name-1,30)
			    - 6 - (is_index ? 7 : 0) - 1,0), "",
		sample_text(ps,v), *q ? SBS : 0, "", q,
		color_parenthetical(v));
    }

    return(cpystr(tmp));
}


char *
sample_text(ps, v)
    struct pine     *ps;
    struct variable *v;
{
    char *ret = SAMP2;
    char *pvalfg, *pvalbg;

    pvalfg = PVAL(v, ew);
    pvalbg = PVAL(v+1, ew);

    if(v && v->name &&
       (srchstr(v->name, "-foreground-color") &&
	(pvalfg && pvalfg[0] && pvalbg && pvalbg[0])) ||
       (v == &ps->vars[V_VIEW_HDR_COLORS] || v == &ps->vars[V_KW_COLORS]))
      ret = SAMP1;

    return(ret);
}


COLOR_PAIR *
sample_color(ps, v)
    struct pine     *ps;
    struct variable *v;
{
    COLOR_PAIR *cp = NULL;
    char *pvalefg, *pvalebg;
    char *pvalmfg, *pvalmbg;

    pvalefg = PVAL(v, ew);
    pvalebg = PVAL(v+1, ew);
    pvalmfg = PVAL(v, Main);
    pvalmbg = PVAL(v+1, Main);
    if(v && color_holding_var(ps, v) &&
       srchstr(v->name, "-foreground-color")){
	if(pvalefg && pvalefg[0] && pvalebg && pvalebg[0])
	  cp = new_color_pair(pvalefg, pvalebg);
	else if(ew == Post && pvalmfg && pvalmfg[0] && pvalmbg && pvalmbg[0])
	  cp = new_color_pair(pvalmfg, pvalmbg);
	else if(v->global_val.p && v->global_val.p[0] &&
	        (v+1)->global_val.p && (v+1)->global_val.p[0])
	  cp = new_color_pair(v->global_val.p, (v+1)->global_val.p);
    }

    return(cp);
}


COLOR_PAIR *
sampleexc_color(ps, v)
    struct pine     *ps;
    struct variable *v;
{
    COLOR_PAIR *cp = NULL;
    char *pvalfg, *pvalbg;

    pvalfg = PVAL(v, Post);
    pvalbg = PVAL(v+1, Post);
    if(v && color_holding_var(ps, v) &&
       srchstr(v->name, "-foreground-color") &&
       pvalfg && pvalfg[0] && pvalbg && pvalbg[0])
	cp = new_color_pair(pvalfg, pvalbg);

    return(cp);
}


char *
sampleexc_text(ps, v)
    struct pine     *ps;
    struct variable *v;
{
    char *ret = "";
    char *pvalfg, *pvalbg;

    pvalfg = PVAL(v, Post);
    pvalbg = PVAL(v+1, Post);
    if(v && color_holding_var(ps, v) &&
       srchstr(v->name, "-foreground-color")){
	if(ew == Main && pvalfg && pvalfg[0] && pvalbg && pvalbg[0])
	  ret = SAMPEXC;
    }

    return(ret);
}


char *
color_setting_text_line(ps, v)
    struct pine     *ps;
    struct variable *v;
{
    char *p;
    char  tmp[30+5+SAMPLE_LEN+SBS+SAMPEXC_LEN+30+1];

    p = sampleexc_text(ps, v);
    sprintf(tmp, "%.30s     %.*s%*s%.*s%.30s", SAMPLE_LEADER,
	    SAMPLE_LEN, sample_text(ps,v),
	    *p ? SBS : 0, "",
	    SAMPEXC_LEN, p,
	    color_parenthetical(v));
    return(cpystr(tmp));
}


char *
radio_pretty_value(ps, cl)
    struct pine *ps;
    CONF_S      *cl;
{
    char  tmp[MAXPATH];
    char *pvalnorm, *pvalexc, *pval;
    int   editing_except_which_isnt_normal, editing_normal_which_isnt_except;
    int   fixed, is_set_for_this_level = 0, is_the_one, the_exc_one;
    int   i, j, lv = 0;
    NAMEVAL_S *rule = NULL, *f;
    PTR_TO_RULEFUNC rulefunc;
    struct variable *v;

    tmp[0] = '\0';
    v = cl->var;

    editing_except_which_isnt_normal = (ew == ps_global->ew_for_except_vars &&
					ew != Main);
    editing_normal_which_isnt_except = (ew == Main &&
					ew != ps_global->ew_for_except_vars);
    fixed = cl->var->is_fixed;
    pvalnorm = PVAL(v, Main);
    pvalexc  = PVAL(v, ps_global->ew_for_except_vars);

    rulefunc = rulefunc_from_var(ps, v);
    rule = rulefunc ? (*rulefunc)(cl->varmem) : NULL;

    /* find longest name */
    if(rulefunc)
      for(lv = 0, i = 0; f = (*rulefunc)(i); i++)
        if(lv < (j = strlen(f->name)))
	  lv = j;

    lv = min(lv, 100);

    if(editing_except_which_isnt_normal)
      pval = pvalexc;
    else
      pval = pvalnorm;

    if(pval)
      is_set_for_this_level++;

    if(fixed){
	pval = v->fixed_val.p;
	is_the_one = (pval && !strucmp(pval, S_OR_L(rule)));

	sprintf(tmp, "(%c)  %-*.*s%s",
		is_the_one ? R_SELD : ' ',
		lv, lv, rule->name, is_the_one ? "   (value is fixed)" : "");
    }
    else if(is_set_for_this_level){
	is_the_one = (pval && !strucmp(pval, S_OR_L(rule)));
	the_exc_one = (editing_normal_which_isnt_except && pvalexc &&
		       !strucmp(pvalexc, S_OR_L(rule)));
	sprintf(tmp, "(%c)  %-*.*s%s",
		is_the_one ? R_SELD : ' ',
		lv, lv, rule->name,
		(!is_the_one && the_exc_one) ? "   (value set in exceptions)" :
		 (is_the_one && the_exc_one) ? "   (also set in exceptions)" :
		  (is_the_one &&
		   editing_normal_which_isnt_except &&
		   pvalexc &&
		   !the_exc_one)             ? "   (overridden by exceptions)" :
					       "");
    }
    else{
	if(pvalexc){
	    is_the_one = !strucmp(pvalexc, S_OR_L(rule));
	    sprintf(tmp, "(%c)  %-*.*s%s",
		    is_the_one ? R_SELD : ' ',
		    lv, lv, rule->name,
		    is_the_one ? "   (value set in exceptions)" : "");
	}
	else{
	    pval = v->current_val.p;
	    is_the_one = (pval && !strucmp(pval, S_OR_L(rule)));
	    sprintf(tmp, "(%c)  %-*.*s%s",
		    is_the_one ? R_SELD : ' ',
		    lv, lv, rule->name,
		    is_the_one ? ((editing_except_which_isnt_normal && pvalnorm) ? "   (default from regular config)" : "   (default)") : "");
	}
    }

    return(cpystr(tmp));
}


char *
sort_pretty_value(ps, cl)
    struct pine *ps;
    CONF_S      *cl;
{
    return(generalized_sort_pretty_value(ps, cl, 1));
}


char *
generalized_sort_pretty_value(ps, cl, default_ok)
    struct pine *ps;
    CONF_S      *cl;
    int          default_ok;
{
    char  tmp[MAXPATH];
    char *pvalnorm, *pvalexc, *pval;
    int   editing_except_which_isnt_normal, editing_normal_which_isnt_except;
    int   fixed, is_set_for_this_level = 0, is_the_one, the_exc_one;
    int   i, j, lv = 0;
    struct variable *v;
    SortOrder line_sort, var_sort, exc_sort;
    int       line_sort_rev, var_sort_rev, exc_sort_rev;

    tmp[0] = '\0';
    v = cl->var;

    editing_except_which_isnt_normal = (ew == ps_global->ew_for_except_vars &&
					ew != Main);
    editing_normal_which_isnt_except = (ew == Main &&
					ew != ps_global->ew_for_except_vars);
    fixed = cl->var->is_fixed;
    pvalnorm = PVAL(v, Main);
    pvalexc  = PVAL(v, ps_global->ew_for_except_vars);

    /* find longest value's name */
    for(lv = 0, i = 0; ps->sort_types[i] != EndofList; i++)
      if(lv < (j = strlen(sort_name(ps->sort_types[i]))))
	lv = j;
    
    lv = min(lv, 100);

    if(editing_except_which_isnt_normal)
      pval = pvalexc;
    else
      pval = pvalnorm;

    if(pval)
      is_set_for_this_level++;

    /* the config line we're talking about */
    if(cl->varmem >= 0){
	line_sort_rev = cl->varmem >= (short)EndofList;
	line_sort     = (SortOrder)(cl->varmem - (line_sort_rev * EndofList));
    }

    if(cl->varmem < 0){
	sprintf(tmp, "(%c)  %-*s",
		(pval == NULL) ? R_SELD : ' ',
		lv, "Default");
    }
    else if(fixed){
	pval = v->fixed_val.p;
	decode_sort(pval, &var_sort, &var_sort_rev);
	is_the_one = (var_sort_rev == line_sort_rev && var_sort == line_sort);

	sprintf(tmp, "(%c)  %s%-*s%*s%s",
		is_the_one ? R_SELD : ' ',
		line_sort_rev ? "Reverse " : "",
		lv, sort_name(line_sort),
		line_sort_rev ? 0 : 8, "",
		is_the_one ? "   (value is fixed)" : "");
    }
    else if(is_set_for_this_level){
	decode_sort(pval, &var_sort, &var_sort_rev);
	is_the_one = (var_sort_rev == line_sort_rev && var_sort == line_sort);
	decode_sort(pvalexc, &exc_sort, &exc_sort_rev);
	the_exc_one = (editing_normal_which_isnt_except && pvalexc &&
		       exc_sort_rev == line_sort_rev && exc_sort == line_sort);
	sprintf(tmp, "(%c)  %s%-*s%*s%s",
		is_the_one ? R_SELD : ' ',
		line_sort_rev ? "Reverse " : "",
		lv, sort_name(line_sort),
		line_sort_rev ? 0 : 8, "",
		(!is_the_one && the_exc_one) ? "   (value set in exceptions)" :
		 (is_the_one && the_exc_one) ? "   (also set in exceptions)" :
		  (is_the_one &&
		   editing_normal_which_isnt_except &&
		   pvalexc &&
		   !the_exc_one)             ? "   (overridden by exceptions)" :
					       "");
    }
    else{
	if(pvalexc){
	    decode_sort(pvalexc, &exc_sort, &exc_sort_rev);
	    is_the_one = (exc_sort_rev == line_sort_rev &&
			  exc_sort == line_sort);
	    sprintf(tmp, "( )  %s%-*s%*s%s",
		    line_sort_rev ? "Reverse " : "",
		    lv, sort_name(line_sort),
		    line_sort_rev ? 0 : 8, "",
		    is_the_one ? "   (value set in exceptions)" : "");
	}
	else{
	    pval = v->current_val.p;
	    decode_sort(pval, &var_sort, &var_sort_rev);
	    is_the_one = ((pval || default_ok) &&
			  var_sort_rev == line_sort_rev &&
			  var_sort == line_sort);
	    sprintf(tmp, "(%c)  %s%-*s%*s%s",
		    is_the_one ? R_SELD : ' ',
		    line_sort_rev ? "Reverse " : "",
		    lv, sort_name(line_sort),
		    line_sort_rev ? 0 : 8, "",
		    is_the_one ? ((editing_except_which_isnt_normal && pvalnorm) ? "   (default from regular config)" : "   (default)") : "");
	}
    }

    return(cpystr(tmp));
}


/*
 * test_feature - runs thru a feature list, and returns:
 *                 1 if feature explicitly set and matches 'v'
 *                 0 if feature not explicitly set *or* doesn't match 'v'
 */
int
test_feature(l, f, g)
    char **l;
    char  *f;
    int    g;
{
    char *p;
    int   rv = 0, forced_off;

    for(; l && *l; l++){
	p = (forced_off = !struncmp(*l, "no-", 3)) ? *l + 3 : *l;
	if(!strucmp(p, f))
	  rv = !forced_off;
	else if(g && !strucmp(p, "old-growth"))
	  rv = !forced_off;
    }

    return(rv);
}


/*
 * Returns 1 -- Feature is in the list and positive
 *         0 -- Feature is not in the list at all
 *        -1 -- Feature is in the list and negative (no-)
 */
int
feature_in_list(l, f)
    char **l;
    char  *f;
{
    char *p;
    int   rv = 0, forced_off;

    for(; l && *l; l++){
	p = (forced_off = !struncmp(*l, "no-", 3)) ? *l + 3 : *l;
	if(!strucmp(p, f))
	  rv = forced_off ? -1 : 1;
    }

    return(rv);
}


void
clear_feature(l, f)
    char ***l;
    char   *f;
{
    char **list = l ? *l : NULL;
    int    count = 0;

    for(; list && *list; list++, count++){
	if(f && !strucmp(((!struncmp(*list,"no-",3)) ? *list + 3 : *list), f)){
	    fs_give((void **)list);
	    f = NULL;
	}

	if(!f)					/* shift  */
	  *list = *(list + 1);
    }

    /*
     * this is helpful to keep the array from growing if a feature
     * get's set and unset repeatedly
     */
    if(!f)
      fs_resize((void **)l, count * sizeof(char *));
}


void
set_feature(l, f, v)
    char ***l;
    char   *f;
    int     v;
{
    char **list = l ? *l : NULL, newval[256];
    int    count = 0;

    sprintf(newval, "%s%.200s", v ? "" : "no-", f);
    for(; list && *list; list++, count++)
      if((**list == '\0')                       /* anything can replace an empty value */
	 || !strucmp(((!struncmp(*list, "no-", 3)) ? *list + 3 : *list), f)){
	  fs_give((void **)list);		/* replace with new value */
	  *list = cpystr(newval);
	  return;
      }

    /*
     * if we got here, we didn't find it in the list, so grow the list
     * and add it..
     */
    if(!*l)
      *l = (char **)fs_get((count + 2) * sizeof(char *));
    else
      fs_resize((void **)l, (count + 2) * sizeof(char *));

    (*l)[count]     = cpystr(newval);
    (*l)[count + 1] = NULL;
}


/*
 * feature_replaces_obsolete - Check to see if this feature replaces
 *			       an obsolete variable.
 */
int
feature_replaces_obsolete(v)
    int v;
{
    return((v == F_INCLUDE_HEADER) || (v == F_SIG_AT_BOTTOM));
}


/*
 *
 */
void
toggle_feature_bit(ps, index, var, cl, just_flip_value)
    struct pine     *ps;
    int		     index;
    struct variable *var;
    CONF_S          *cl;
    int              just_flip_value;
{
    FEATURE_S  *f;
    char      **vp, *p, **lval, ***alval;
    int		og, on_before, was_set;
    long        l;

    f  = feature_list(index);
    og = test_old_growth_bits(ps, f->id);

    /*
     * if this feature is in the fixed set, or old-growth is in the fixed
     * set and this feature is in the old-growth set, don't alter it...
     */
    for(vp = var->fixed_val.l; vp && *vp; vp++){
	p = (struncmp(*vp, "no-", 3)) ? *vp : *vp + 3;
	if(!strucmp(p, f->name) || (og && !strucmp(p, "old-growth"))){
	    q_status_message(SM_ORDER, 3, 3,
			     "Can't change value fixed by sys-admin.");
	    return;
	}
    }

    on_before = F_ON(f->id, ps);

    lval  = LVAL(var, ew);
    alval = ALVAL(var, ew);
    if(just_flip_value)
      was_set = test_feature(lval, f->name, og);
    else
      was_set = feature_gets_an_x(ps, cl, NULL);

    if(alval)
      set_feature(alval, f->name, !was_set);

    set_feature_list_current_val(var);
    process_feature_list(ps, var->current_val.l, 0, 0, 0);
    
    if(just_flip_value){
	if(cl->value && cl->value[0])
	  cl->value[1] = (cl->value[1] == ' ') ? 'X' : ' ';
    }
    else{
	/*
	 * This fork is only called from the checkbox_tool, which has
	 * varmem set to index correctly and cl->var set correctly.
	 */
	if(cl->value)
	  fs_give((void **)&cl->value);

	cl->value = pretty_value(ps, cl);
    }

    /*
     * Handle any features that need special attention here...
     */
    if(on_before != F_ON(f->id, ps))
     switch(f->id){
      case F_QUOTE_ALL_FROMS :
	mail_parameters(NULL,SET_FROMWIDGET,(void *)(F_ON(f->id ,ps) ? 1 : 0));
	break;

      case F_CMBND_ABOOK_DISP :
	addrbook_reset();
	break;

      case F_FAKE_NEW_IN_NEWS :
        if(IS_NEWS(ps->mail_stream))
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "news-approximates-new-status won't affect current newsgroup until next open");

	break;

      case F_COLOR_LINE_IMPORTANT :
	clear_tindex_cache();
	break;

      case F_MARK_FOR_CC :
	clear_index_cache();
	if(THREADING() && sp_viewing_a_thread(ps->mail_stream))
	  unview_thread(ps, ps->mail_stream, ps->msgmap);

	break;

      case F_HIDE_NNTP_PATH :
	mail_parameters(NULL, SET_NNTPHIDEPATH,
			(void *)(F_ON(f->id, ps) ? 1 : 0));
	break;

      case F_MAILDROPS_PRESERVE_STATE :
	mail_parameters(NULL, SET_SNARFPRESERVE,
			(void *)(F_ON(f->id, ps) ? 1 : 0));
	break;

      case F_DISABLE_SHARED_NAMESPACES :
	mail_parameters(NULL, SET_DISABLEAUTOSHAREDNS,
			(void *)(F_ON(f->id, ps) ? 1 : 0));
	break;

      case F_QUELL_LOCK_FAILURE_MSGS :
	mail_parameters(NULL, SET_LOCKEACCESERROR,
			(void *)(F_ON(f->id, ps) ? 1 : 0));
	break;

      case F_MULNEWSRC_HOSTNAMES_AS_TYPED :
	l = F_ON(f->id, ps) ? 0L : 1L;
	mail_parameters(NULL, SET_NEWSRCCANONHOST, (void *) l);
	break;

      case F_QUELL_INTERNAL_MSG :
	mail_parameters(NULL, SET_USERHASNOLIFE,
			(void *)(F_ON(f->id, ps) ? 1 : 0));
	break;

      case F_ENABLE_INCOMING :
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "Folder List changes will take effect your next pine session.");

	break;

      case F_PRESERVE_START_STOP :
	/* toggle raw mode settings to make tty driver aware of new setting */
	PineRaw(0);
	PineRaw(1);
	break;

      case F_SHOW_SORT :
	ps->mangled_header = 1;
	break;

      case F_USE_FK :
	ps->orig_use_fkeys = F_ON(F_USE_FK, ps);
	ps->mangled_footer = 1;
	mark_keymenu_dirty();
	break;

      case F_BLANK_KEYMENU :
	clearfooter(ps);
	if(F_ON(f->id, ps)){
	    FOOTER_ROWS(ps) = 1;
	    ps->mangled_body = 1;
	}
	else{
	    FOOTER_ROWS(ps) = 3;
	    ps->mangled_footer = 1;
	}

	break;

      case F_DISABLE_SETLOCALE_COLLATE :
      case F_ENABLE_SETLOCALE_CTYPE :
	set_collation(F_OFF(F_DISABLE_SETLOCALE_COLLATE, ps),
		      F_ON(F_ENABLE_SETLOCALE_CTYPE, ps));
	break;

#ifdef	_WINDOWS
      case F_SHOW_CURSOR :
	mswin_showcaret(F_ON(f->id,ps));
	break;

      case F_ENABLE_TRAYICON :
	mswin_trayicon(F_ON(f->id,ps));
	break;

#endif
#if !defined(DOS) && !defined(OS2)
      case F_ALLOW_TALK :
	if(F_ON(f->id, ps))
	  allow_talk(ps);
	else
	  disallow_talk(ps);

	break;

      case F_PASS_CONTROL_CHARS :
	ps->pass_ctrl_chars = F_ON(F_PASS_CONTROL_CHARS,ps_global) ? 1 : 0;
	break;

      case F_PASS_C1_CONTROL_CHARS :
	ps->pass_c1_ctrl_chars = F_ON(F_PASS_C1_CONTROL_CHARS,ps_global)
								    ? 1 : 0;
	break;
#endif
#ifdef	MOUSE
      case F_ENABLE_MOUSE :
	if(F_ON(f->id, ps))
	  init_mouse();
	else
	  end_mouse();

	break;
#endif

      default :
	break;
     }
}


/*
 * new_confline - create new CONF_S zero it out, and insert it after current.
 *                NOTE current gets set to the new CONF_S too!
 */
CONF_S *
new_confline(current)
    CONF_S **current;
{
    CONF_S *p;

    p = (CONF_S *)fs_get(sizeof(CONF_S));
    memset((void *)p, 0, sizeof(CONF_S));
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


/*
 *
 */
void
snip_confline(p)
    CONF_S **p;
{
    if(*p){
	/* Yank it from the linked list */
	if((*p)->prev)
	  (*p)->prev->next = (*p)->next;

	if((*p)->next)
	  (*p)->next->prev = (*p)->prev;

	/* Then free up it's memory */
	(*p)->prev = (*p)->next = NULL;
	free_conflines(p);
    }
}


/*
 *
 */
void
free_conflines(p)
    CONF_S **p;
{
    if(*p){
	free_conflines(&(*p)->next);

	if((*p)->varname)
	  fs_give((void **) &(*p)->varname);

	if((*p)->value)
	  fs_give((void **) &(*p)->value);

	fs_give((void **) p);
    }
}


/*
 *
 */
CONF_S *
first_confline(p)
    CONF_S *p;
{
    while(p && p->prev)
      p = p->prev;

    return(p);
}


/*
 * First selectable confline.
 */
CONF_S *
first_sel_confline(p)
    CONF_S *p;
{
    for(p = first_confline(p); p && (p->flags&CF_NOSELECT); p=next_confline(p))
      ;/* do nothing */

    return(p);
}


/*
 *
 */
CONF_S *
last_confline(p)
    CONF_S *p;
{
    while(p && p->next)
      p = p->next;

    return(p);
}


/*
 *
 */
fixed_var(v, action, name)
    struct variable *v;
    char	    *action, *name;
{
    char **lval;

    if(v && v->is_fixed
       && (!v->is_list
           || ((lval=v->fixed_val.l) && lval[0]
	       && strcmp(INHERIT, lval[0]) != 0))){
	q_status_message2(SM_ORDER, 3, 3,
			  "Can't %.200s sys-admin defined %.200s.",
			  action ? action : "change", name ? name : "value");
	return(1);
    }

    return(0);
}


void
exception_override_warning(v)
    struct variable *v;
{
    char **lval;

    /* if exceptions config file exists and we're not editing it */
    if(v && (ps_global->ew_for_except_vars != Main) && (ew == Main)){
	if((!v->is_list && PVAL(v, ps_global->ew_for_except_vars)) ||
	   (v->is_list && (lval=LVAL(v, ps_global->ew_for_except_vars)) &&
	    lval[0] && strcmp(INHERIT, lval[0]) != 0))
	  q_status_message1(SM_ORDER, 3, 3,
	   "Warning: \"%.200s\" is overridden in your exceptions configuration",
	      v->name);
    }
}


void
offer_to_fix_pinerc(ps)
    struct pine *ps;
{
    struct variable *v;
    char             prompt[300];
    char            *p, *q;
    char           **list;
    char           **list_fixed;
    int              rv = 0, write_main = 0, write_post = 0;
    int              i, k, j, need, exc;
    char            *clear = ": delete it";
    char          ***plist;

    dprint(4,(debugfile, "offer_to_fix_pinerc()\n"));

    ps->fix_fixed_warning = 0;  /* so we only ask first time */

    if(ps->readonly_pinerc)
      return;

    set_titlebar("FIXING PINERC", ps->mail_stream,
		 ps->context_current,
		 ps->cur_folder, ps->msgmap, 1, FolderName, 0, 0, NULL);

    if(want_to("Some of your options conflict with site policy.  Investigate",
	'y', 'n', NO_HELP, WT_FLUSH_IN) != 'y')
      return;
    
/* space want_to requires in addition to the string you pass in */
#define WANTTO_SPACE 6
    need = WANTTO_SPACE + strlen(clear);

    for(v = ps->vars; v->name; v++){
	if(!v->is_fixed ||
	   !v->is_user ||
	    v->is_obsolete ||
	    v == &ps->vars[V_FEATURE_LIST]) /* handle feature-list below */
	  continue;
	
	prompt[0] = '\0';
	
	if(v->is_list &&
	   (v->post_user_val.l || v->main_user_val.l)){
	    char **active_list;

	    active_list = v->post_user_val.l ? v->post_user_val.l
					     : v->main_user_val.l;
	    if(*active_list){
		sprintf(prompt, "Your setting for %.50s is ", v->name);
		p = prompt + strlen(prompt);
		for(i = 0; active_list[i]; i++){
		    if(p - prompt > ps->ttyo->screen_cols - need)
		      break;
		    if(i)
		      *p++ = ',';
		    sstrcpy(&p, active_list[i]);
		}
		*p = '\0';
	    }
	    else
	      sprintf(prompt, "Your setting for %.50s is %.50s", v->name, empty_val2);
	}
	else{
	    if(v->post_user_val.p || v->main_user_val.p){
		char *active_var;

		active_var = v->post_user_val.p ? v->post_user_val.p
						: v->main_user_val.p;
		if(*active_var){
		    sprintf(prompt, "Your setting for %.50s is %.100s",
			v->name, active_var);
		}
		else{
		    sprintf(prompt, "Your setting for %.50s is %.100s",
			v->name, empty_val2);
		}
	    }
	}

	if(*prompt){
	    if(strlen(prompt) > ps->ttyo->screen_cols - need)
	      (void)strcpy(prompt + max(ps->ttyo->screen_cols - need - 3, 0),
			  "...");

	    (void)strcat(prompt, clear);
	    if(want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
		if(v->is_list){
		    if(v->main_user_val.l)
		      write_main++;
		    if(v->post_user_val.l)
		      write_post++;
		}
		else{
		    if(v->main_user_val.p)
		      write_main++;
		    if(v->post_user_val.p)
		      write_post++;
		}

		if(delete_user_vals(v))
		  rv++;
	    }
	}
    }


    /*
     * As always, feature-list has to be handled separately.
     */
    exc = (ps->ew_for_except_vars != Main);
    v = &ps->vars[V_FEATURE_LIST];
    list_fixed = v->fixed_val.l;

    for(j = 0; j < 2; j++){
      plist = (j==0) ? &v->main_user_val.l : &v->post_user_val.l;
      list = *plist;
      if(list){
        for(i = 0; list[i]; i++){
	  p = list[i];
	  if(!struncmp(p, "no-", 3))
	    p += 3;
	  for(k = 0; list_fixed && list_fixed[k]; k++){
	    q = list_fixed[k];
	    if(!struncmp(q, "no-", 3))
	      q += 3;
	    if(!strucmp(q, p) && strucmp(list[i], list_fixed[k])){
	      sprintf(prompt, "Your %.50s is %s%s, fixed value is %s",
		  p, p == list[i] ? "ON" : "OFF",
		  exc ? ((plist == &v->main_user_val.l) ? ""
						        : " in postload-config")
		      : "",
		  q == list_fixed[k] ? "ON" : "OFF");

	      if(strlen(prompt) > ps->ttyo->screen_cols - need)
	        (void)strcpy(prompt + max(ps->ttyo->screen_cols - need - 3, 0),
			     "...");

	      (void)strcat(prompt, clear);
	      if(want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
		  rv++;

		  if(plist == &v->main_user_val.l)
		    write_main++;
		  else
		    write_post++;

		  /*
		   * Clear the feature from the user's pinerc
		   * so that we'll stop bothering them when they
		   * start up Pine.
		   */
		  clear_feature(plist, p);

		  /*
		   * clear_feature scoots the list up, so if list[i] was
		   * the last one going in, now it is the end marker.  We
		   * just decrement i so that it will get incremented and
		   * then test == 0 in the for loop.  We could just goto
		   * outta_here to accomplish the same thing.
		   */
		  if(!list[i])
		    i--;
	      }
	    }
	  }
        }
      }
    }


    if(write_main)
      write_pinerc(ps, Main, WRP_NONE);
    if(write_post)
      write_pinerc(ps, Post, WRP_NONE);

    return;
}


/*
 * Compare saved user_val with current user_val to see if it changed.
 * If any have changed, change it back and take the appropriate action.
 */
void
revert_to_saved_config(ps, vsave, allow_hard_to_config_remotely)
    struct pine *ps;
    SAVED_CONFIG_S *vsave;
    int allow_hard_to_config_remotely;
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;
    int i, n;
    int changed = 0;
    char *pval, **apval, **lval, ***alval;

    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!save_include(ps, vreal, allow_hard_to_config_remotely))
	  continue;

	changed = 0;
	if(vreal->is_list){
	    lval  = LVAL(vreal, ew);
	    alval = ALVAL(vreal, ew);

	    if((v->saved_user_val.l && !lval)
	       || (!v->saved_user_val.l && lval))
	      changed++;
	    else if(!v->saved_user_val.l && !lval)
	      ;/* no change, nothing to do */
	    else
	      for(i = 0; v->saved_user_val.l[i] || lval[i]; i++)
		if((v->saved_user_val.l[i]
		      && (!lval[i]
			 || strcmp(v->saved_user_val.l[i], lval[i])))
		   ||
		     (!v->saved_user_val.l[i] && lval[i])){
		    changed++;
		    break;
		}
	    
	    if(changed){
		char  **list;

		if(alval){
		    if(*alval)
		      free_list_array(alval);
		
		    /* copy back the original one */
		    if(v->saved_user_val.l){
			list = v->saved_user_val.l;
			n = 0;
			/* count how many */
			while(list[n])
			  n++;

			*alval = (char **)fs_get((n+1) * sizeof(char *));

			for(i = 0; i < n; i++)
			  (*alval)[i] = cpystr(v->saved_user_val.l[i]);

			(*alval)[n] = NULL;
		    }
		}
	    }
	}
	else{
	    pval  = PVAL(vreal, ew);
	    apval = APVAL(vreal, ew);

	    if((v->saved_user_val.p &&
	        (!pval || strcmp(v->saved_user_val.p, pval))) ||
	       (!v->saved_user_val.p && pval)){
		/* It changed, fix it */
		changed++;
		if(apval){
		    /* free the changed value */
		    if(*apval)
		      fs_give((void **)apval);

		    if(v->saved_user_val.p)
		      *apval = cpystr(v->saved_user_val.p);
		}
	    }
	}

	if(changed){
	    if(vreal == &ps->vars[V_FEATURE_LIST])
	      set_feature_list_current_val(vreal);
	    else
	      set_current_val(vreal, TRUE, FALSE);

	    fix_side_effects(ps, vreal, 1);
	}
    }
}


/*
 * Compare saved user_val with current user_val to see if it changed.
 * If any have changed, change it back and take the appropriate action.
 */
void
revert_to_saved_color_config(ps, vsave)
struct pine *ps;
SAVED_CONFIG_S *vsave;
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;
    int i, n;
    int changed = 0;
    char *pval, **apval, **lval, ***alval;

    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!(color_related_var(ps, vreal) || vreal==&ps->vars[V_FEATURE_LIST]))
	  continue;

	if(vreal->is_list){
	    lval  = LVAL(vreal, ew);
	    alval = ALVAL(vreal, ew);

	    if((v->saved_user_val.l && !lval)
	       || (!v->saved_user_val.l && lval))
	      changed++;
	    else if(!v->saved_user_val.l && !lval)
	      ;/* no change, nothing to do */
	    else
	      for(i = 0; v->saved_user_val.l[i] || lval[i]; i++)
		if((v->saved_user_val.l[i]
		      && (!lval[i]
			 || strcmp(v->saved_user_val.l[i], lval[i])))
		   ||
		     (!v->saved_user_val.l[i] && lval[i])){
		    changed++;
		    break;
		}
	    
	    if(changed){
		char  **list;

		if(alval){
		    if(*alval)
		      free_list_array(alval);
		
		    /* copy back the original one */
		    if(v->saved_user_val.l){
			list = v->saved_user_val.l;
			n = 0;
			/* count how many */
			while(list[n])
			  n++;

			*alval = (char **)fs_get((n+1) * sizeof(char *));

			for(i = 0; i < n; i++)
			  (*alval)[i] = cpystr(v->saved_user_val.l[i]);

			(*alval)[n] = NULL;
		    }
		}
	    }
	}
	else{
	    pval  = PVAL(vreal, ew);
	    apval = APVAL(vreal, ew);

	    if((v->saved_user_val.p &&
	        (!pval || strcmp(v->saved_user_val.p, pval))) ||
	       (!v->saved_user_val.p && pval)){
		/* It changed, fix it */
		changed++;
		if(apval){
		    /* free the changed value */
		    if(*apval)
		      fs_give((void **)apval);

		    if(v->saved_user_val.p)
		      *apval = cpystr(v->saved_user_val.p);
		}
	    }
	}

	if(changed){
	    if(vreal == &ps->vars[V_FEATURE_LIST])
	      set_feature_list_current_val(vreal);
	    else
	      set_current_val(vreal, TRUE, FALSE);

	    fix_side_effects(ps, vreal, 1);
	}
    }

    if(changed){
	set_current_color_vals(ps);
	ClearScreen();
	ps->mangled_screen = 1;
    }
}


/*
 * Adjust side effects that happen because variable changes values.
 *
 * Var->user_val should be set to the new value before calling this.
 */
void
fix_side_effects(ps, var, revert)
struct pine     *ps;
struct variable *var;
int              revert;
{
    int    val = 0;
    char **v, *q, **apval;
    struct variable *vars = ps->vars;

    /* move this up here so we get the Using default message */
    if(var == &ps->vars[V_PERSONAL_NAME]){
	if(!(var->main_user_val.p ||
	     var->post_user_val.p) && ps->ui.fullname){
	    if(var->current_val.p)
	      fs_give((void **)&var->current_val.p);

	    var->current_val.p = cpystr(ps->ui.fullname);
	}
    }

    if(!revert
      && ((!var->is_fixed
	    && !var->is_list
	    && !(var->main_user_val.p ||
		 var->post_user_val.p)
	    && var->current_val.p)
	 ||
	 (!var->is_fixed
	    && var->is_list
	    && !(var->main_user_val.l ||
		 var->post_user_val.l)
	    && var->current_val.l)))
      q_status_message(SM_ORDER,0,3,"Using default value");

    if(var == &ps->vars[V_USER_DOMAIN]){
	char *p, *q;

	if(ps->VAR_USER_DOMAIN
	   && ps->VAR_USER_DOMAIN[0]
	   && (p = strrindex(ps->VAR_USER_DOMAIN, '@'))){
	    if(*(++p)){
		if(!revert)
		  q_status_message2(SM_ORDER, 3, 5,
		    "User-domain (%.200s) cannot contain \"@\"; using %.200s",
		    ps->VAR_USER_DOMAIN, p);
		q = ps->VAR_USER_DOMAIN;
		while((*q++ = *p++) != '\0')
		  ;/* do nothing */
	    }
	    else{
		if(!revert)
		  q_status_message1(SM_ORDER, 3, 5,
		    "User-domain (%.200s) cannot contain \"@\"; deleting",
		    ps->VAR_USER_DOMAIN);

		if(ps->vars[V_USER_DOMAIN].post_user_val.p){
		    fs_give((void **)&ps->vars[V_USER_DOMAIN].post_user_val.p);
		    set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
		}

		if(ps->VAR_USER_DOMAIN
		   && ps->VAR_USER_DOMAIN[0]
		   && (p = strrindex(ps->VAR_USER_DOMAIN, '@'))){
		    if(ps->vars[V_USER_DOMAIN].main_user_val.p){
			fs_give((void **)&ps->vars[V_USER_DOMAIN].main_user_val.p);
			set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
		    }
		}
	    }
	}

	/*
	 * Reset various pointers pertaining to domain name and such...
	 */
	init_hostname(ps);
    }
    else if(var == &ps->vars[V_INBOX_PATH]){
	/*
	 * fixup the inbox path based on global/default values...
	 */
	init_inbox_mapping(ps->VAR_INBOX_PATH, ps->context_list);

	if(!strucmp(ps->cur_folder, ps->inbox_name) && ps->mail_stream
	   && strcmp(ps->VAR_INBOX_PATH, ps->mail_stream->mailbox)){
	    /*
	     * If we currently have "inbox" open and the mailbox name
	     * doesn't match, reset the current folder's name and
	     * remove the SP_INBOX flag.
	     */
	    strncpy(ps->cur_folder, ps->mail_stream->mailbox,
		    sizeof(ps->cur_folder)-1);
	    ps->cur_folder[sizeof(ps->cur_folder)-1] = '\0';
	    sp_set_fldr(ps->mail_stream, ps->cur_folder);
	    sp_unflag(ps->mail_stream, SP_INBOX);
	    ps->mangled_header = 1;
	}
	else if(sp_inbox_stream()
	    && strcmp(ps->VAR_INBOX_PATH, sp_inbox_stream()->original_mailbox)){
	    MAILSTREAM *m = sp_inbox_stream();

	    /*
	     * if we don't have inbox directly open, but have it
	     * open for new mail notification, close the stream like
	     * any other ordinary folder, and clean up...
	     */
	    if(m){
		sp_unflag(m, SP_PERMLOCKED | SP_INBOX);
		sp_set_fldr(m, m->mailbox);
		expunge_and_close(m, NULL, EC_NONE);
	    }
	}
    }
    else if(var == &ps->vars[V_ADDRESSBOOK] ||
	    var == &ps->vars[V_GLOB_ADDRBOOK] ||
#ifdef	ENABLE_LDAP
	    var == &ps->vars[V_LDAP_SERVERS] ||
#endif
	    var == &ps->vars[V_ABOOK_FORMATS]){
	addrbook_reset();
    }
    else if(var == &ps->vars[V_INDEX_FORMAT]){
	reset_index_format();
	clear_iindex_cache();
    }
    else if(var == &ps->vars[V_DEFAULT_FCC] ||
	    var == &ps->vars[V_DEFAULT_SAVE_FOLDER]){
	init_save_defaults();
    }
    else if(var == &ps->vars[V_KW_BRACES]){
	clear_iindex_cache();
    }
    else if(var == &ps->vars[V_KEYWORDS]){
	if(ps_global->keywords)
	  free_keyword_list(&ps_global->keywords);
	
	if(var->current_val.l && var->current_val.l[0])
	  ps_global->keywords = init_keyword_list(var->current_val.l);

	clear_iindex_cache();
    }
    else if(var == &ps->vars[V_INIT_CMD_LIST]){
	if(!revert)
	  q_status_message(SM_ASYNC, 0, 3,
	    "Initial command changes will affect your next pine session.");
    }
    else if(var == &ps->vars[V_VIEW_HEADERS]){
	ps->view_all_except = 0;
	if(ps->VAR_VIEW_HEADERS)
	  for(v = ps->VAR_VIEW_HEADERS; (q = *v) != NULL; v++)
	    if(q[0]){
		char *p;

		removing_leading_white_space(q);
		/* look for colon or space or end */
		for(p = q; *p && !isspace((unsigned char)*p) && *p != ':'; p++)
		  ;/* do nothing */
		
		*p = '\0';
		if(strucmp(q, ALL_EXCEPT) == 0)
		  ps->view_all_except = 1;
	    }
    }
    else if(var == &ps->vars[V_OVERLAP]){
	int old_value = ps->viewer_overlap;

	if(SVAR_OVERLAP(ps, old_value, tmp_20k_buf)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->viewer_overlap = old_value;
    }
    else if(var == &ps->vars[V_MAXREMSTREAM]){
	int old_value = ps->s_pool.max_remstream;

	if(SVAR_MAXREMSTREAM(ps, old_value, tmp_20k_buf)){
	    if(!revert )
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->s_pool.max_remstream = old_value;

	dprint(9, (debugfile, "max_remstream goes to %d\n",
		ps->s_pool.max_remstream));
    }
    else if(var == &ps->vars[V_MARGIN]){
	int old_value = ps->scroll_margin;

	if(SVAR_MARGIN(ps, old_value, tmp_20k_buf)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->scroll_margin = old_value;
    }
    else if(var == &ps->vars[V_DEADLETS]){
	int old_value = ps->deadlets;

	if(SVAR_DEADLETS(ps, old_value, tmp_20k_buf)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->deadlets = old_value;
    }
    else if(var == &ps->vars[V_FILLCOL]){
	if(SVAR_FILLCOL(ps, ps->composer_fillcol, tmp_20k_buf)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
    }
    else if(var == &ps->vars[V_QUOTE_SUPPRESSION]){
	val = ps->quote_suppression_threshold;
	if(val < Q_SUPP_LIMIT && val > 0)
	  val = -val;

	if(ps->VAR_QUOTE_SUPPRESSION
	   && SVAR_QUOTE_SUPPRESSION(ps, val, tmp_20k_buf)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else{
	    if(val > 0 && val < Q_SUPP_LIMIT){
		if(!revert){
		    sprintf(tmp_20k_buf, "Ignoring Quote-Suppression-Threshold value of %.50s, see help", ps->VAR_QUOTE_SUPPRESSION);
		    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
		}
	    }
	    else{
		if(val < 0 && val != Q_DEL_ALL)
		  ps->quote_suppression_threshold = -val;
		else
		  ps->quote_suppression_threshold = val;
	    }
	}
    }
    else if(var == &ps->vars[V_STATUS_MSG_DELAY]){
	if(SVAR_MSGDLAY(ps, ps->status_msg_delay, tmp_20k_buf)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
    }
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
    else if(var == &ps->vars[V_FIFOPATH]){
	init_newmailfifo(ps->VAR_FIFOPATH);
    }
#endif
    else if(var == &ps->vars[V_NMW_WIDTH]){
	int old_value = ps->nmw_width;

	if(SVAR_NMW_WIDTH(ps, old_value, tmp_20k_buf)){
	    if(!revert )
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else{
#ifdef _WINDOWS
	    if(old_value != ps->nmw_width)
	      mswin_setnewmailwidth(old_value);	/* actually the new value */
#endif
	    ps->nmw_width = old_value;
	}
    }
    else if(var == &ps->vars[V_TCPOPENTIMEO]){
	val = 30;
	if(!revert)
	  if(ps->VAR_TCPOPENTIMEO && SVAR_TCP_OPEN(ps, val, tmp_20k_buf))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_TCPREADWARNTIMEO]){
	val = 15;
	if(!revert)
	  if(ps->VAR_TCPREADWARNTIMEO && SVAR_TCP_READWARN(ps,val,tmp_20k_buf))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_TCPWRITEWARNTIMEO]){
	val = 0;
	if(!revert)
	 if(ps->VAR_TCPWRITEWARNTIMEO && SVAR_TCP_WRITEWARN(ps,val,tmp_20k_buf))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_TCPQUERYTIMEO]){
	val = 60;
	if(!revert)
	  if(ps->VAR_TCPQUERYTIMEO && SVAR_TCP_QUERY(ps, val, tmp_20k_buf))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_RSHOPENTIMEO]){
	val = 15;
	if(!revert)
	  if(ps->VAR_RSHOPENTIMEO && SVAR_RSH_OPEN(ps, val, tmp_20k_buf))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_SSHOPENTIMEO]){
	val = 15;
	if(!revert)
	  if(ps->VAR_SSHOPENTIMEO && SVAR_SSH_OPEN(ps, val, tmp_20k_buf))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_SIGNATURE_FILE]){
	if(ps->VAR_OPER_DIR && ps->VAR_SIGNATURE_FILE &&
	   is_absolute_path(ps->VAR_SIGNATURE_FILE) &&
	   !in_dir(ps->VAR_OPER_DIR, ps->VAR_SIGNATURE_FILE)){
	    char *e;

	    e = (char *)fs_get((strlen(ps->VAR_OPER_DIR) + 100) * sizeof(char));
	    sprintf(e, "Warning: Sig file can't be outside of %s",
		    ps->VAR_OPER_DIR);
	    q_status_message(SM_ORDER, 3, 6, e);
	    fs_give((void **)&e);
	}
    }
    else if(var == &ps->vars[V_OPER_DIR]){
	if(ps->VAR_OPER_DIR && !ps->VAR_OPER_DIR[0]){
	    q_status_message(SM_ORDER, 3, 5, "Operating-dir is turned off.");
	    fs_give((void **)&ps->vars[V_OPER_DIR].current_val.p);
	    if(ps->vars[V_OPER_DIR].fixed_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].fixed_val.p);
	    if(ps->vars[V_OPER_DIR].global_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].global_val.p);
	    if(ps->vars[V_OPER_DIR].cmdline_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].cmdline_val.p);
	    if(ps->vars[V_OPER_DIR].post_user_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].post_user_val.p);
	    if(ps->vars[V_OPER_DIR].main_user_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].main_user_val.p);
	}
    }
    else if(var == &ps->vars[V_MAILCHECK]){
	timeo = 15;
	if(SVAR_MAILCHK(ps, timeo, tmp_20k_buf)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else if(timeo == 0 && !revert){
	    q_status_message(SM_ORDER, 4, 6,
"Warning: automatic new mail checking and mailbox checkpointing is disabled");
	    if(ps->VAR_INBOX_PATH && ps->VAR_INBOX_PATH[0] == '{')
	      q_status_message(SM_ASYNC, 3, 6,
"Warning: Mail-Check-Interval=0 may cause IMAP server connection to time out");
	}
    }
    else if(var == &ps->vars[V_MAILCHECKNONCURR]){
	val = (int) ps->check_interval_for_noncurr;
	if(ps->VAR_MAILCHECKNONCURR
	   && SVAR_MAILCHKNONCURR(ps, val, tmp_20k_buf)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->check_interval_for_noncurr = (time_t) val;
    }
    else if(var == &ps->vars[V_MAILDROPCHECK]){
	long rvl;

	rvl = 60L;
	if(ps->VAR_MAILDROPCHECK && SVAR_MAILDCHK(ps, rvl, tmp_20k_buf))
	  q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	else{
	    if(rvl == 0L)
	      rvl = (60L * 60L * 24L * 100L);	/* 100 days */

	    if(rvl >= 60L)
	      mail_parameters(NULL, SET_SNARFINTERVAL, (void *) rvl);
	}
    }
    else if(var == &ps->vars[V_NNTPRANGE]){
	long rvl;

	rvl = 0L;
	if(ps->VAR_NNTPRANGE && SVAR_NNTPRANGE(ps, rvl, tmp_20k_buf))
	  q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	else{
	    if(rvl >= 0L)
	      mail_parameters(NULL, SET_NNTPRANGE, (void *) rvl);
	}
    }
    else if(var == &ps->vars[V_CUSTOM_HDRS] || var == &ps->vars[V_COMP_HDRS]){
	/* this will give warnings about headers that can't be changed */
	if(!revert && var->current_val.l && var->current_val.l[0])
	  customized_hdr_setup(NULL, var->current_val.l, UseAsDef);
    }
#if defined(DOS) || defined(OS2)
    else if(var == &ps->vars[V_FOLDER_EXTENSION]){
	mail_parameters(NULL, SET_EXTENSION,
			(void *)var->current_val.p);
    }
    else if(var == &ps->vars[V_NEWSRC_PATH]){
	if(var->current_val.p && var->current_val.p[0])
	  mail_parameters(NULL, SET_NEWSRC,
			  (void *)var->current_val.p);
    }
#endif
    else if(revert && standard_radio_var(ps, var)){

	cur_rule_value(var, TRUE, FALSE);
	if(var == &ps_global->vars[V_AB_SORT_RULE])
	  addrbook_reset();
	else if(var == &ps_global->vars[V_THREAD_INDEX_STYLE]){
	    clear_index_cache();
	    set_lflags(ps_global->mail_stream, ps_global->msgmap,
		       MN_COLL | MN_CHID, 0);
	    if(SORT_IS_THREADED(ps_global->msgmap)
	       && (SEP_THRDINDX() || COLL_THRDS()))
	      collapse_threads(ps_global->mail_stream, ps_global->msgmap, NULL);

	    adjust_cur_to_visible(ps_global->mail_stream, ps_global->msgmap);
	}
#ifndef	_WINDOWS
	else if(var == &ps->vars[V_COLOR_STYLE]){
	    pico_toggle_color(0);
	    switch(ps->color_style){
	      case COL_NONE:
	      case COL_TERMDEF:
		pico_set_color_options(0);
		break;
	      case COL_ANSI8:
		pico_set_color_options(COLOR_ANSI8_OPT);
		break;
	      case COL_ANSI16:
		pico_set_color_options(COLOR_ANSI16_OPT);
		break;
	    }

	    if(ps->color_style != COL_NONE)
	      pico_toggle_color(1);
	
	    if(pico_usingcolor())
	      pico_set_normal_color();

	    clear_index_cache();
	    ClearScreen();
	    ps->mangled_screen = 1;
	}
#endif
    }
    else if(revert && var == &ps->vars[V_SORT_KEY]){
	int def_sort_rev;

	decode_sort(VAR_SORT_KEY, &ps->def_sort, &def_sort_rev);
	ps->def_sort_rev = def_sort_rev;
    }
    else if(var == &ps->vars[V_THREAD_MORE_CHAR] ||
            var == &ps->vars[V_THREAD_EXP_CHAR] ||
            var == &ps->vars[V_THREAD_LASTREPLY_CHAR]){

	if(var == &ps->vars[V_THREAD_LASTREPLY_CHAR] &&
	   !(var->current_val.p && var->current_val.p[0])){
	    if(var->current_val.p)
	      fs_give((void **) &var->current_val.p);
	    
	    q_status_message1(SM_ORDER, 3, 5,
		      "\"%.200s\" can't be Empty, using default", var->name);

	    apval = APVAL(var, ew);
	    if(*apval)
	      fs_give((void **)apval);

	    set_current_val(var, FALSE, FALSE);

	    if(!(var->current_val.p && var->current_val.p[0]
		 && !var->current_val.p[1])){
		if(var->current_val.p)
		  fs_give((void **) &var->current_val.p);
		
		var->current_val.p = cpystr(DF_THREAD_LASTREPLY_CHAR);
	    }
	}

	if(var == &ps->vars[V_THREAD_MORE_CHAR] ||
	   var == &ps->vars[V_THREAD_EXP_CHAR] ||
	   var == &ps->vars[V_THREAD_LASTREPLY_CHAR]){
	    if(var->current_val.p && var->current_val.p[0] &&
	       var->current_val.p[1]){
		q_status_message1(SM_ORDER, 3, 5,
			  "Only first character of \"%.200s\" is used",
			  var->name);
		var->current_val.p[1] = '\0';
	    }

	    if(var->main_user_val.p && var->main_user_val.p[0] &&
	       var->main_user_val.p[1])
	      var->main_user_val.p[1] = '\0';

	    if(var->post_user_val.p && var->post_user_val.p[0] &&
	       var->post_user_val.p[1])
	      var->post_user_val.p[1] = '\0';
	}

	clear_index_cache();
	set_need_format_setup();
    }
    else if(var == &ps->vars[V_NNTP_SERVER]){
	    free_contexts(&ps_global->context_list);
	    init_folders(ps_global);
    }
    else if(var == &ps->vars[V_USE_ONLY_DOMAIN_NAME]){
	init_hostname(ps);
    }
    else if(var == &ps->vars[V_KW_COLORS] ||
	    var == &ps->vars[V_IND_PLUS_FORE_COLOR] ||
	    var == &ps->vars[V_IND_IMP_FORE_COLOR]  ||
            var == &ps->vars[V_IND_DEL_FORE_COLOR]  ||
            var == &ps->vars[V_IND_ANS_FORE_COLOR]  ||
            var == &ps->vars[V_IND_NEW_FORE_COLOR]  ||
            var == &ps->vars[V_IND_UNS_FORE_COLOR]  ||
            var == &ps->vars[V_IND_ARR_FORE_COLOR]  ||
            var == &ps->vars[V_IND_REC_FORE_COLOR]  ||
            var == &ps->vars[V_IND_PLUS_BACK_COLOR] ||
            var == &ps->vars[V_IND_IMP_BACK_COLOR]  ||
            var == &ps->vars[V_IND_DEL_BACK_COLOR]  ||
            var == &ps->vars[V_IND_ANS_BACK_COLOR]  ||
            var == &ps->vars[V_IND_NEW_BACK_COLOR]  ||
            var == &ps->vars[V_IND_UNS_BACK_COLOR]  ||
            var == &ps->vars[V_IND_ARR_BACK_COLOR]  ||
            var == &ps->vars[V_IND_REC_BACK_COLOR]){
	clear_index_cache();
    }
    else if(var == score_act_global_ptr){
	int score;

	score = atoi(var->current_val.p);
	if(score < SCORE_MIN || score > SCORE_MAX){
	    q_status_message2(SM_ORDER, 3, 5,
			      "Score Value must be in range %.200s to %.200s",
			      comatose(SCORE_MIN), comatose(SCORE_MAX));
	    apval = APVAL(var, ew);
	    if(*apval)
	      fs_give((void **)apval);
	    
	    set_current_val(var, FALSE, FALSE);
	}
    }
    else if(var == scorei_pat_global_ptr || var == age_pat_global_ptr
	    || var == size_pat_global_ptr || var == cati_global_ptr){
	apval = APVAL(var, ew);
	if(*apval){
	    INTVL_S *iv;
	    iv = parse_intvl(*apval);
	    if(iv){
		fs_give((void **) apval);
		*apval = stringform_of_intvl(iv);
		free_intvl(&iv);
	    }
	    else
	      fs_give((void **) apval);
	}

	set_current_val(var, FALSE, FALSE);
    }
    else if(var == &ps->vars[V_FEATURE_LIST]){
	process_feature_list(ps, var->current_val.l, 0, 0, 0);
    }
    else if(!revert && (var == &ps->vars[V_LAST_TIME_PRUNE_QUESTION] ||
		        var == &ps->vars[V_REMOTE_ABOOK_HISTORY] ||
		        var == &ps->vars[V_REMOTE_ABOOK_VALIDITY] ||
		        var == &ps->vars[V_USERINPUTTIMEO] ||
		        var == &ps->vars[V_NEWS_ACTIVE_PATH] ||
		        var == &ps->vars[V_NEWS_SPOOL_DIR] ||
		        var == &ps->vars[V_INCOMING_FOLDERS] ||
		        var == &ps->vars[V_FOLDER_SPEC] ||
		        var == &ps->vars[V_NEWS_SPEC] ||
		        var == &ps->vars[V_DISABLE_DRIVERS] ||
		        var == &ps->vars[V_DISABLE_AUTHS] ||
		        var == &ps->vars[V_RSHPATH] ||
		        var == &ps->vars[V_RSHCMD] ||
		        var == &ps->vars[V_SSHCMD] ||
		        var == &ps->vars[V_SSHPATH])){
	q_status_message2(SM_ASYNC, 0, 3,
		      "Changes%.200s%.200s will affect your next pine session.",
			  var->name ? " to " : "", var->name ? var->name : "");
    }

    if(!revert && (var == &ps->vars[V_TCPOPENTIMEO] ||
		   var == &ps->vars[V_TCPREADWARNTIMEO] ||
		   var == &ps->vars[V_TCPWRITEWARNTIMEO] ||
		   var == &ps->vars[V_TCPQUERYTIMEO] ||
		   var == &ps->vars[V_RSHOPENTIMEO] ||
		   var == &ps->vars[V_SSHOPENTIMEO]))
      q_status_message(SM_ASYNC, 0, 3,
		       "Timeout changes will affect your next pine session.");
}


SAVED_CONFIG_S *
save_config_vars(ps, allow_hard_to_config_remotely)
    struct pine *ps;
    int allow_hard_to_config_remotely;
{
    struct variable *vreal;
    SAVED_CONFIG_S *vsave, *v;

    vsave = (SAVED_CONFIG_S *)fs_get((V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    memset((void *)vsave, 0, (V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!save_include(ps, vreal, allow_hard_to_config_remotely))
	  continue;
	
	if(vreal->is_list){
	    int n, i;
	    char **list;

	    if(LVAL(vreal, ew)){
		/* count how many */
		n = 0;
		list = LVAL(vreal, ew);
		while(list[n])
		  n++;

		v->saved_user_val.l = (char **)fs_get((n+1) * sizeof(char *));
		memset((void *)v->saved_user_val.l, 0, (n+1)*sizeof(char *));
		for(i = 0; i < n; i++)
		  v->saved_user_val.l[i] = cpystr(list[i]);

		v->saved_user_val.l[n] = NULL;
	    }
	}
	else{
	    if(PVAL(vreal, ew))
	      v->saved_user_val.p = cpystr(PVAL(vreal, ew));
	}
    }

    return(vsave);
}


SAVED_CONFIG_S *
save_color_config_vars(ps)
struct pine *ps;
{
    struct variable *vreal;
    SAVED_CONFIG_S *vsave, *v;

    vsave = (SAVED_CONFIG_S *)fs_get((V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    memset((void *)vsave, 0, (V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!(color_related_var(ps, vreal) || vreal==&ps->vars[V_FEATURE_LIST]))
	  continue;
	
	if(vreal->is_list){
	    int n, i;
	    char **list;

	    if(LVAL(vreal, ew)){
		/* count how many */
		n = 0;
		list = LVAL(vreal, ew);
		while(list[n])
		  n++;

		v->saved_user_val.l = (char **)fs_get((n+1) * sizeof(char *));
		memset((void *)v->saved_user_val.l, 0, (n+1)*sizeof(char *));
		for(i = 0; i < n; i++)
		  v->saved_user_val.l[i] = cpystr(list[i]);

		v->saved_user_val.l[n] = NULL;
	    }
	}
	else{
	    if(PVAL(vreal, ew))
	      v->saved_user_val.p = cpystr(PVAL(vreal, ew));
	}
    }

    return(vsave);
}


void
free_saved_config(ps, vsavep, allow_hard_to_config_remotely)
    struct pine *ps;
    SAVED_CONFIG_S **vsavep;
    int allow_hard_to_config_remotely;
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;

    if(vsavep && *vsavep){
	for(v = *vsavep, vreal = ps->vars; vreal->name; vreal++,v++){
	    if(!save_include(ps, vreal, allow_hard_to_config_remotely))
	      continue;
	    
	    if(vreal->is_list){  /* free saved_user_val.l */
		if(v && v->saved_user_val.l)
		  free_list_array(&v->saved_user_val.l);
	    }
	    else if(v && v->saved_user_val.p)
	      fs_give((void **)&v->saved_user_val.p);
	}

	fs_give((void **)vsavep);
    }
}


void
free_saved_color_config(ps, vsavep)
struct pine *ps;
SAVED_CONFIG_S **vsavep;
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;

    if(vsavep && *vsavep){
	for(v = *vsavep, vreal = ps->vars; vreal->name; vreal++,v++){
	    if(!(color_related_var(ps, vreal) ||
	       (vreal == &ps->vars[V_FEATURE_LIST])))
	      continue;
	    
	    if(vreal->is_list){  /* free saved_user_val.l */
		if(v && v->saved_user_val.l)
		  free_list_array(&v->saved_user_val.l);
	    }
	    else if(v && v->saved_user_val.p)
	      fs_give((void **)&v->saved_user_val.p);
	}

	fs_give((void **)vsavep);
    }
}


/*
 * Given a single printer string from the config file, returns pointers
 * to alloc'd strings containing the printer nickname, the command,
 * the init string, the trailer string, everything but the nickname string,
 * and everything but the command string.  All_but_cmd includes the trailing
 * space at the end (the one before the command) but all_but_nick does not
 * include the leading space (the one before the [).
 * If you pass in a pointer it is guaranteed to come back pointing to an
 * allocated string, even if it is just an empty string.  It is ok to pass
 * NULL for any of the six return strings.
 */
void
parse_printer(input, nick, cmd, init, trailer, all_but_nick, all_but_cmd)
    char  *input;
    char **nick,
	 **cmd,
	 **init,
	 **trailer,
	 **all_but_nick,
	 **all_but_cmd;
{
    char *p, *q, *start, *saved_options = NULL;
    int tmpsave, cnt;

    if(!input)
      input = "";

    if(nick || all_but_nick){
	if(p = srchstr(input, " [")){
	    if(all_but_nick)
	      *all_but_nick = cpystr(p+1);

	    if(nick){
		while(p-1 > input && isspace((unsigned char)*(p-1)))
		  p--;

		tmpsave = *p;
		*p = '\0';
		*nick = cpystr(input);
		*p = tmpsave;
	    }
	}
	else{
	    if(nick)
	      *nick = cpystr("");

	    if(all_but_nick)
	      *all_but_nick = cpystr(input);
	}
    }

    if(p = srchstr(input, "] ")){
	do{
	    ++p;
	}while(isspace((unsigned char)*p));

	tmpsave = *p;
	*p = '\0';
	saved_options = cpystr(input);
	*p = tmpsave;
    }
    else
      p = input;
    
    if(cmd)
      *cmd = cpystr(p);

    if(init){
	if(saved_options && (p = srchstr(saved_options, "INIT="))){
	    start = p + strlen("INIT=");
	    for(cnt=0, p = start; *p && *(p+1) && isxpair(p); p += 2)
	      cnt++;
	    
	    q = *init = (char *)fs_get((cnt + 1) * sizeof(char));
	    for(p = start; *p && *(p+1) && isxpair(p); p += 2)
	      *q++ = read_hex(p);
	    
	    *q = '\0';
	}
	else
	  *init = cpystr("");
    }

    if(trailer){
	if(saved_options && (p = srchstr(saved_options, "TRAILER="))){
	    start = p + strlen("TRAILER=");
	    for(cnt=0, p = start; *p && *(p+1) && isxpair(p); p += 2)
	      cnt++;
	    
	    q = *trailer = (char *)fs_get((cnt + 1) * sizeof(char));
	    for(p = start; *p && *(p+1) && isxpair(p); p += 2)
	      *q++ = read_hex(p);
	    
	    *q = '\0';
	}
	else
	  *trailer = cpystr("");
    }

    if(all_but_cmd){
	if(saved_options)
	  *all_but_cmd = saved_options;
	else
	  *all_but_cmd = cpystr("");
    }
    else if(saved_options)
      fs_give((void **)&saved_options);
}


/*
 * Given a single printer string from the config file, returns an allocated
 * copy of the friendly printer name, which is
 *      "Nickname"  command
 */
char *
printer_name(input)
    char *input;
{
    char *nick, *cmd;
    char *ret;

    parse_printer(input, &nick, &cmd, NULL, NULL, NULL, NULL);
    ret = (char *)fs_get((2+22+1+strlen(cmd)) * sizeof(char));
    sprintf(ret, "\"%.21s\"%*s%s",
	*nick ? nick : "",
	22 - min(strlen(nick), 21),
	"",
	cmd);
    fs_give((void **)&nick);
    fs_give((void **)&cmd);

    return(ret);
}



static struct key sel_from_list_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", "Exit",     {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {"S", "[Select]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", "Prev", {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", "Next", {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(sel_from_list, sel_from_list_keys);

static struct key sel_from_list_keys_sm[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", "Exit",     {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {"S", "[Select]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", "Prev", {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", "Next", {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	{"L","ListMode",{MC_LISTMODE,1,{'l'}},KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(sel_from_list_sm, sel_from_list_keys_sm);

static struct key sel_from_list_keys_lm[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", "Exit",     {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {"S", "Select", {MC_SELECT,1,{'s'}}, KS_NONE},
	{"P", "Prev", {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", "Next", {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X","[Set/Unset]", {MC_TOGGLE,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE},
	{"1","SinglMode",{MC_LISTMODE,1,{'1'}},KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(sel_from_list_lm, sel_from_list_keys_lm);

/*
 * This is intended to be a generic tool to select strings from a list
 * of strings.
 *
 * Args     lsel -- the items as well as the answer are contained in this list
 *         flags -- There is some inconsistent flags usage. Notice that the
 *                  flag SFL_ALLOW_LISTMODE is a flag passed in the flags
 *                  argument whereas the flag SFL_NOSELECT is a per item
 *                  (that is, per LIST_SEL_S) flag.
 *         title -- passed to conf_scroll_screen
 *         pdesc -- passed to conf_scroll_screen
 *          help -- passed to conf_scroll_screen
 *     helptitle -- passed to conf_scroll_screen
 *
 * You have screen width - 4 columns to work with. If you want to overflow to
 * a second (or third or fourth) line for an item just send another item
 * in the list but with the SFL_NOSELECT flag set. Only the selectable lines
 * will be highlighted, which is kind of a crock, but it looked like a lot
 * of work to fix that.  Too long lines will be truncated and replaced
 * with ending "...".
 *
 * Returns 0 on successful choice
 *        -1 if cancelled
 */
int
select_from_list_screen(lsel, flags, title, pdesc, help, htitle)
    LIST_SEL_S   *lsel;
    unsigned long flags;
    char         *title;
    char         *pdesc;
    HelpType      help;
    char         *htitle;
{
    CONF_S      *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S screen;
    int          j, lv, ret = -1;
    LIST_SEL_S  *p;
    char        *display;
    ScreenMode   listmode = SingleMode;

    if(!lsel)
      return(ret);
    
    /* find longest value's length */
    for(lv = 0, p = lsel; p; p = p->next){
	if(!(p->flags & SFL_NOSELECT)){
	    display = p->display_item ? p->display_item :
			p->item ? p->item : "";
	    if(lv < (j = strlen(display)))
	      lv = j;
	}
    }

    lv = min(lv, ps_global->ttyo->screen_cols - 4);


    /*
     * Convert the passed in list to conf_scroll lines.
     */

    if(flags & SFL_ALLOW_LISTMODE){

	for(p = lsel; p; p = p->next){

	    display = p->display_item ? p->display_item :
			p->item ? p->item : "";
	    new_confline(&ctmp);
	    if(!first_line && !(p->flags & SFL_NOSELECT))
	      first_line = ctmp;

	    ctmp->value        = (char *) fs_get((lv + 4 + 1) * sizeof(char));
	    sprintf(ctmp->value, "    %-*.*s", lv, lv, display);
	    if(strlen(ctmp->value) > ps_global->ttyo->screen_cols){
		ctmp->value[ps_global->ttyo->screen_cols-1] = '.';
		ctmp->value[ps_global->ttyo->screen_cols-2] = '.';
		ctmp->value[ps_global->ttyo->screen_cols-3] = '.';
	    }

	    ctmp->d.l.lsel     = p;
	    ctmp->d.l.listmode = &listmode;
	    ctmp->keymenu      = &sel_from_list_sm;
	    ctmp->help         = help;
	    ctmp->help_title   = htitle;
	    ctmp->tool         = select_from_list_tool;
	    ctmp->flags        = CF_STARTITEM |
				 ((p->flags & SFL_NOSELECT) ? CF_NOSELECT : 0);
	}
    }
    else{

	for(p = lsel; p; p = p->next){

	    display = p->display_item ? p->display_item :
			p->item ? p->item : "";
	    new_confline(&ctmp);
	    if(!first_line && !(p->flags & SFL_NOSELECT))
	      first_line = ctmp;

	    ctmp->value        = (char *) fs_get((lv + 1) * sizeof(char));
	    sprintf(ctmp->value, "%-*.*s", lv, lv, display);
	    if(strlen(ctmp->value) > ps_global->ttyo->screen_cols){
		ctmp->value[ps_global->ttyo->screen_cols-1] = '.';
		ctmp->value[ps_global->ttyo->screen_cols-2] = '.';
		ctmp->value[ps_global->ttyo->screen_cols-3] = '.';
	    }

	    ctmp->d.l.lsel     = p;
	    ctmp->d.l.listmode = &listmode;
	    ctmp->keymenu      = &sel_from_list;
	    ctmp->help         = help;
	    ctmp->help_title   = htitle;
	    ctmp->tool         = select_from_list_tool;
	    ctmp->flags        = CF_STARTITEM |
				 ((p->flags & SFL_NOSELECT) ? CF_NOSELECT : 0);
	    ctmp->valoffset    = 4;
	}
    }

    /* just convert to start in listmode after the fact, easier that way */
    if(flags & SFL_STARTIN_LISTMODE){
	listmode = ListMode;

	for(ctmp = first_line; ctmp; ctmp = next_confline(ctmp))
	  if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
	      ctmp->value[0] = '[';
	      ctmp->value[1] = ctmp->d.l.lsel->selected ? 'X' : SPACE;
	      ctmp->value[2] = ']';
	      ctmp->keymenu  = &sel_from_list_lm;
	  }
    }

    memset(&screen, 0, sizeof(screen));
    switch(conf_scroll_screen(ps_global, &screen, first_line, title, pdesc, 0)){
      case 1:
        ret = 0;
	break;
      
      default:
	break;
    }

    ps_global->mangled_screen = 1;
    return(ret);
}


int
select_from_list_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    CONF_S *ctmp;
    int     retval = 0;

    switch(cmd){
      case MC_SELECT :
	if(*(*cl)->d.l.listmode == SingleMode){
	    (*cl)->d.l.lsel->selected = 1;
	    retval = 3;
	}
	else{
	    /* check if anything is selected */
	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->d.l.lsel->selected){
		  retval = 3;
		  break;
	      }
	    
	    if(retval == 0){
		q_status_message(SM_ORDER, 0, 3,
		     "Nothing selected, use Exit to exit without a selection.");
	    }
	}

	break;

      case MC_LISTMODE :
        if(*(*cl)->d.l.listmode == SingleMode){
	    /*
	     * UnHide the checkboxes
	     */

	    *(*cl)->d.l.listmode = ListMode;

	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		  ctmp->value[0] = '[';
		  ctmp->value[1] = ctmp->d.l.lsel->selected ? 'X' : SPACE;
		  ctmp->value[2] = ']';
		  ctmp->keymenu  = &sel_from_list_lm;
	      }
	}
	else{
	    /*
	     * Hide the checkboxes
	     */

	    *(*cl)->d.l.listmode = SingleMode;

	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		  ctmp->value[0] = ctmp->value[1] = ctmp->value[2] = SPACE;
		  ctmp->keymenu  = &sel_from_list_sm;
	      }
	}

	ps->mangled_body = ps->mangled_footer = 1;
	break;

      case MC_TOGGLE :
	if((*cl)->value[1] == 'X'){
	    (*cl)->d.l.lsel->selected = 0;
	    (*cl)->value[1] = SPACE;
	}
	else{
	    (*cl)->d.l.lsel->selected = 1;
	    (*cl)->value[1] = 'X';
	}

	ps->mangled_body = 1;
	break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      default:
	retval = -1;
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}



static struct key role_select_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", "Exit", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	{"P", "PrevRole", {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", "NextRole", {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(role_select_km, role_select_keys);
#define DEFAULT_KEY 3

int
role_select_screen(ps, role, alt_compose)
    struct pine    *ps;
    ACTION_S      **role;
    int            alt_compose;
{
    CONF_S        *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S   screen;
    PAT_S         *pat, *sel_pat = NULL;
    int            ret = -1;
    long           rflags = ROLE_DO_ROLES;
    char          *helptitle;
    HelpType       help;
    PAT_STATE      pstate;

    if(!role)
      return(ret);

    *role = NULL;

    if(!(nonempty_patterns(rflags, &pstate) &&
         first_pattern(&pstate))){
	q_status_message(SM_ORDER, 0, 3,
			 "No roles available. Use Setup/Rules to add roles.");
	return(ret);
    }


    if(alt_compose){
	menu_init_binding(&role_select_km,
	  alt_compose == MC_FORWARD ? 'F' : alt_compose == MC_REPLY ? 'R' : 'C',
	  MC_CHOICE,
	  alt_compose == MC_FORWARD ? "F" : alt_compose == MC_REPLY ? "R" : "C",
	  alt_compose == MC_FORWARD ? "[ForwardAs]"
	    : alt_compose == MC_REPLY ? "[ReplyAs]" : "[ComposeAs]",
	  DEFAULT_KEY);
	menu_add_binding(&role_select_km, ctrl('J'), MC_CHOICE);
	menu_add_binding(&role_select_km, ctrl('M'), MC_CHOICE);
    }
    else{
	menu_init_binding(&role_select_km, 'S', MC_CHOICE, "S", "[Select]",
			  DEFAULT_KEY);
	menu_add_binding(&role_select_km, ctrl('J'), MC_CHOICE);
	menu_add_binding(&role_select_km, ctrl('M'), MC_CHOICE);
    }

    if(alt_compose){
	helptitle = "HELP FOR SELECTING A ROLE TO COMPOSE AS";
	help      = h_role_select;
    }
    else{
	helptitle = "HELP FOR SELECTING A ROLE NICKNAME";
	help      = h_role_nick_select;
    }

    for(pat = first_pattern(&pstate);
	pat;
	pat = next_pattern(&pstate)){
	new_confline(&ctmp);
	if(!first_line)
	  first_line = ctmp;

	ctmp->value        = cpystr((pat->patgrp && pat->patgrp->nick)
					? pat->patgrp->nick : "?");
	ctmp->d.r.selected = &sel_pat;
	ctmp->d.r.pat      = pat;
	ctmp->keymenu      = &role_select_km;
	ctmp->help         = help;
	ctmp->help_title   = helptitle;
	ctmp->tool         = role_select_tool;
	ctmp->flags        = CF_STARTITEM;
	ctmp->valoffset    = 4;
    }

    memset(&screen, 0, sizeof(screen));
    (void)conf_scroll_screen(ps, &screen, first_line, "SELECT ROLE",
			     "roles ", 0);

    if(sel_pat){
	*role = sel_pat->action;
	ret = 0;
    }

    ps->mangled_screen = 1;
    return(ret);
}


int
abook_select_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    int retval;

    switch(cmd){
      case MC_CHOICE :
	*((*cl)->d.b.selected) = cpystr((*cl)->d.b.abookname);
	retval = simple_exit_cmd(flags);
	break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      default:
	retval = -1;
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}


int
role_select_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    int retval;

    switch(cmd){
      case MC_CHOICE :
	*((*cl)->d.r.selected) = (*cl)->d.r.pat;
	retval = simple_exit_cmd(flags);
	break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      default:
	retval = -1;
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}


static struct key abook_select_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", "Exit", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"S", "[Select]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	{"P", "Prev", {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", "Next", {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(abook_select_km, abook_select_keys);

char *
abook_select_screen(ps)
    struct pine *ps;
{
    CONF_S        *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S   screen;
    int            adrbknum;
    char          *helptitle;
    HelpType       help;
    PerAddrBook   *pab;
    char          *abook = NULL;

    helptitle = "HELP FOR SELECTING AN ADDRESS BOOK";
    help      = h_role_abook_select;

    init_ab_if_needed();

    for(adrbknum = 0; adrbknum < as.n_addrbk; adrbknum++){
	new_confline(&ctmp);
	if(!first_line)
	  first_line = ctmp;

	pab = &as.adrbks[adrbknum];

	ctmp->value        = cpystr((pab && pab->nickname)
				      ? pab->nickname
				      : (pab && pab->filename)
				        ? pab->filename
					: "?");

	ctmp->d.b.selected  = &abook;
	ctmp->d.b.abookname = ctmp->value;
	ctmp->keymenu       = &abook_select_km;
	ctmp->help          = help;
	ctmp->help_title    = helptitle;
	ctmp->tool          = abook_select_tool;
	ctmp->flags         = CF_STARTITEM;
	ctmp->valoffset     = 4;
    }

    if(first_line){
	memset(&screen, 0, sizeof(screen));
	(void) conf_scroll_screen(ps, &screen, first_line,
				  "SELECT ADDRESS BOOK", "abooks ", 0);
    }
    else
      q_status_message(SM_ORDER|SM_DING, 3, 3, "No address books defined!");

    ps->mangled_screen = 1;
    return(abook);
}


void
role_config_screen(ps, rflags, edit_exceptions)
    struct pine *ps;
    long         rflags;
    int          edit_exceptions;
{
    CONF_S      *first_line;
    OPT_SCREEN_S screen;
    char         title[100];
    int          readonly_warning = 0;
    PAT_STATE    pstate;
    struct variable *v = NULL;

    dprint(4,(debugfile, "role_config_screen()\n"));

    if(ps->fix_fixed_warning)
      offer_to_fix_pinerc(ps);

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    rflags |= PAT_USE_MAIN;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    rflags |= PAT_USE_POST;
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return;
	}
    }

    if(!any_patterns(rflags, &pstate))
      return;
    
    if(rflags & ROLE_DO_ROLES)
      v = &ps_global->vars[V_PAT_ROLES];
    else if(rflags & ROLE_DO_INCOLS)
      v = &ps_global->vars[V_PAT_INCOLS];
    else if(rflags & ROLE_DO_OTHER)
      v = &ps_global->vars[V_PAT_OTHER];
    else if(rflags & ROLE_DO_SCORES)
      v = &ps_global->vars[V_PAT_SCORES];
    else if(rflags & ROLE_DO_FILTER)
      v = &ps_global->vars[V_PAT_FILTS];

    if((ps_global->ew_for_except_vars != Main) && (ew == Main)){
	char           **lval;

	if((lval=LVAL(v, ps_global->ew_for_except_vars)) &&
	   lval[0] && strcmp(INHERIT, lval[0]) != 0){
	    role_type_print(title, "Warning: \"%sRules\" are overridden in your exceptions configuration", rflags);
	    q_status_message(SM_ORDER, 7, 7, title);
	}
    }

    role_type_print(title, "%sRules", rflags);
    if(fixed_var(v, "change", title))
      return;

uh_oh:
    first_line = NULL;

    sprintf(title, "SETUP%s ", edit_exceptions ? " EXCEPTIONAL" : "");
    role_type_print(title+strlen(title), "%sRULES", rflags);
    role_global_flags = rflags;
    role_global_pstate = &pstate;
    role_config_init_disp(ps, &first_line, rflags, &pstate);
    
    if(!first_line){
	role_global_flags = 0;
	ps->mangled_screen = 1;
	q_status_message(SM_ORDER,5,5,
		    "Unexpected problem: config file modified externally?");
	q_status_message1(SM_ORDER,5,5,
	 "Perhaps a newer version of pine was used to set variable \"%.200s\"?",
	    v ? v->name : "?");
	dprint(1,(debugfile, "Unexpected problem: config file modified externally?\nPerhaps by a newer pine? Variable \"%s\" has unexpected contents.\n",
	(v && v->name) ? v->name : "?"));
	return;
    }

    memset(&screen, 0, sizeof(screen));
    screen.deferred_ro_warning = readonly_warning;
    switch(conf_scroll_screen(ps, &screen, first_line, title, "rules ", 0)){
	case 0:
	  break;

	case 10:
	  /* flush changes and re-read orig */
	  close_patterns(rflags);
	  break;
	
	case 1:
	  if(write_patterns(rflags))
	    goto uh_oh;

	  /*
	   * Flush out current_vals of anything we've possibly changed.
	   */
	  close_patterns((rflags & ROLE_MASK) | PAT_USE_CURRENT);

	  /* scores may have changed */
	  if(rflags & ROLE_DO_SCORES){
	      int         i;
	      MAILSTREAM *m;

	      clear_iindex_cache();

	      for(i = 0; i < ps_global->s_pool.nstream; i++){
		  m = ps_global->s_pool.streams[i];
		  if(m)
		    clear_folder_scores(m);
	      }
	      
	      if(mn_get_sort(sp_msgmap(ps_global->mail_stream)) == SortScore)
	        refresh_sort(ps_global->mail_stream,
			     sp_msgmap(ps_global->mail_stream), SRT_VRB);
	  }

	  /* recalculate need for scores */
	  scores_are_used(SCOREUSE_INVALID);

	  /* we may want to fetch more or fewer headers each fetch */ 
	  calc_extra_hdrs();
	  if(get_extra_hdrs())
	    (void) mail_parameters(NULL, SET_IMAPEXTRAHEADERS,
				   (void *) get_extra_hdrs());

	  if(rflags & ROLE_DO_INCOLS && pico_usingcolor())
	    clear_iindex_cache();

	  if(rflags & ROLE_DO_FILTER)
	    role_process_filters();

	  /*
	   * ROLE_DO_OTHER is made up of a bunch of different variables
	   * that may have changed. Assume they all changed and fix them.
	   */
	  if(rflags & ROLE_DO_OTHER){
	      reset_index_format();
	      clear_iindex_cache();
	      if(!mn_get_mansort(ps_global->msgmap))
		reset_sort_order(SRT_VRB);
	  }

	  break;
	
	default:
	  q_status_message(SM_ORDER,7,10, "conf_scroll_screen unexpected ret");
	  break;
    }

    role_global_flags = 0;
    ps->mangled_screen = 1;
}


/*
 * This is called from process_cmd to add a new pattern to the end of the
 * list of patterns. The pattern is seeded with values from the current
 * message.
 */
void
role_take(ps, msgmap, rtype)
    struct pine *ps;
    MSGNO_S     *msgmap;
    int          rtype;
{
    PAT_S       *defpat, *newpat = NULL;
    PAT_LINE_S  *new_patline, *patline;
    ENVELOPE    *env = NULL;
    long         rflags;
    char        *s, title[100], specific_fldr[MAXPATH+1];
    PAT_STATE    pstate;
    EditWhich    ew;

    dprint(4,(debugfile, "role_take()\n"));

    if(mn_get_cur(msgmap) > 0){
	env = pine_mail_fetchstructure(ps->mail_stream,
				       mn_m2raw(msgmap, mn_get_cur(msgmap)),
				       NULL);
    
	if(!env){
	    q_status_message(SM_ORDER, 3, 7,
			     "problem getting addresses from message");
	    return;
	}
    }

    switch(rtype){
      case 'r':
	rflags = ROLE_DO_ROLES;
	ew = ps_global->ew_for_role_take;
	break;
      case 's':
	rflags = ROLE_DO_SCORES;
	ew = ps_global->ew_for_score_take;
	break;
      case 'i':
	rflags = ROLE_DO_INCOLS;
	ew = ps_global->ew_for_incol_take;
	break;
      case 'f':
	rflags = ROLE_DO_FILTER;
	ew = ps_global->ew_for_filter_take;
	break;
      case 'o':
	rflags = ROLE_DO_OTHER;
	ew = ps_global->ew_for_other_take;
	break;

      default:
	cmd_cancelled(NULL);
	return;
    }

    switch(ew){
      case Main:
	rflags |= PAT_USE_MAIN;
	break;
      case Post:
	rflags |= PAT_USE_POST;
	break;
    }

    if(!any_patterns(rflags, &pstate)){
	q_status_message(SM_ORDER, 3, 7, "problem accessing rules");
	return;
    }

    /* set this so that even if we don't edit at all, we'll be asked */
    rflags |= ROLE_CHANGES;

    /*
     * Make a pattern out of the information in the envelope and
     * use that as the default pattern we give to the role editor.
     * It will have a pattern but no actions set.
     */
    defpat = (PAT_S *)fs_get(sizeof(*defpat));
    memset((void *)defpat, 0, sizeof(*defpat));

    defpat->patgrp = (PATGRP_S *)fs_get(sizeof(*defpat->patgrp));
    memset((void *)defpat->patgrp, 0, sizeof(*defpat->patgrp));

    if(env){
	if(env->to)
	  defpat->patgrp->to = addrlst_to_pattern(env->to);

	if(env->from)
	  defpat->patgrp->from = addrlst_to_pattern(env->from);

	if(env->cc)
	  defpat->patgrp->cc = addrlst_to_pattern(env->cc);

	if(env->sender &&
	   (!env->from || !address_is_same(env->sender, env->from)))
	  defpat->patgrp->sender = addrlst_to_pattern(env->sender);

	/*
	 * Env->newsgroups is already comma-separated and there shouldn't be
	 * any commas or backslashes in newsgroup names, so we don't add the
	 * roletake escapes.
	 */
	if(env->newsgroups)
	  defpat->patgrp->news = string_to_pattern(env->newsgroups);

	/*
	 * Subject may have commas or backslashes, so we add escapes.
	 * We lose track of the charset when we go into the pattern editor.
	 */
	if(env->subject){
	    char *p, *q, *t = NULL, *charset;

	    p = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				       SIZEOF_20KBUF, env->subject, &charset);

	    /* toss the charset for now */
	    if(charset)
	      fs_give((void **)&charset);

	    /*
	     * We do the mime decode above before passing it to strip_subject
	     * because otherwise strip_subject wants to give us utf8 back, and
	     * we can't handle it.
	     */
	    mail_strip_subject(p, &q);
	    if(q != NULL){
		t = add_roletake_escapes(q);
		fs_give((void **)&q);
	    }

	    if(t){
		defpat->patgrp->subj = string_to_pattern(t);
		fs_give((void **)&t);
	    }
	}
    }
    
    if(IS_NEWS(ps->mail_stream))
      defpat->patgrp->fldr_type = FLDR_NEWS;
    else
      defpat->patgrp->fldr_type = FLDR_EMAIL;

    specific_fldr[0] = specific_fldr[sizeof(specific_fldr)-1] = '\0';
    if(sp_flagged(ps->mail_stream, SP_INBOX))
      strncpy(specific_fldr, ps_global->inbox_name, sizeof(specific_fldr)-1);
    else if(ps->context_current
	    && ps->context_current->use & CNTXT_INCMNG &&
	    folder_is_nick(ps->cur_folder, FOLDERS(ps->context_current), 0))
      strncpy(specific_fldr, ps->cur_folder, sizeof(specific_fldr)-1);
    else
      context_apply(specific_fldr, ps->context_current, ps->cur_folder,
		    sizeof(specific_fldr));
    
    if(specific_fldr[0]){
	s = add_comma_escapes(specific_fldr);
	if(s){
	    if(rtype == 'f')
	      defpat->patgrp->fldr_type = FLDR_SPECIFIC;

	    defpat->patgrp->folder = string_to_pattern(s);
	    fs_give((void **)&s);
	}
    }

    role_type_print(title, "ADD NEW %sRULE", rflags);

    /*
     * Role_config_edit_screen is sometimes called as a tool or a sub
     * routine called from a tool within conf_scroll_screen, but here it
     * is going to be at the top-level (we're not inside conf_scrooll_screen
     * right now). It uses opt_screen to set the ro_warning bit. We need
     * to let it know that we're at the top, which we do by setting
     * opt_screen to NULL. Otherwise, the thing that opt_screen is pointing
     * to is just random stack stuff from some previous conf_scroll_screen
     * call which has already exited.
     */
    opt_screen = NULL;

    if(role_config_edit_screen(ps, defpat, title, rflags,
			       &newpat) == 1 && newpat){

	if(ps->never_allow_changing_from && newpat->action &&
	   newpat->action->from)
	    q_status_message(SM_ORDER|SM_DING, 3, 7,
      "Site policy doesn't allow changing From address so From is ignored");

	if(rflags & ROLE_DO_ROLES && newpat->patgrp && newpat->patgrp->nick){
	    PAT_S *pat;
	    
	    for(pat = first_pattern(&pstate);
		pat;
		pat = next_pattern(&pstate)){
		if(pat->patgrp && pat->patgrp->nick &&
		   !strucmp(pat->patgrp->nick, newpat->patgrp->nick)){
		    q_status_message(SM_ORDER|SM_DING, 3, 7, "Warning: The nickname of the new role is already in use.");
		    break;
		}
	    }
	}


	set_pathandle(rflags);

	/* need a new patline */
	new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
	memset((void *)new_patline, 0, sizeof(*new_patline));
	new_patline->type = Literal;
	(*cur_pat_h)->dirtypinerc = 1;

	/* tie together with new pattern */
	new_patline->first = new_patline->last = newpat;
	newpat->patline = new_patline;

	/* find last current patline */
	for(patline = (*cur_pat_h)->patlinehead;
	    patline && patline->next;
	    patline = patline->next)
	  ;
	
	/* add new patline to end of list */
	if(patline){
	    patline->next = new_patline;
	    new_patline->prev = patline;
	}
	else
	  (*cur_pat_h)->patlinehead = new_patline;

	if(write_patterns(rflags) == 0){
	    char msg[60];

	    /*
	     * Flush out current_vals of anything we've possibly changed.
	     */
	    close_patterns(rflags | PAT_USE_CURRENT);

	    role_type_print(msg, "New %srule saved", rflags);
	    q_status_message(SM_ORDER, 0, 3, msg);

	    /* scores may have changed */
	    if(rflags & ROLE_DO_SCORES){
		int         i;
		MAILSTREAM *m;

		clear_iindex_cache();

		for(i = 0; i < ps_global->s_pool.nstream; i++){
		    m = ps_global->s_pool.streams[i];
		    if(m)
		      clear_folder_scores(m);
		}
	      
		/* We've already bound msgmap to global mail_stream
		 * at the start of this function, but if we wanted to
		 * we could clean this up.
		 */
		if(mn_get_sort(msgmap) == SortScore)
	          refresh_sort(ps_global->mail_stream, msgmap, SRT_VRB);
	    }

	    if(rflags & ROLE_DO_FILTER)
	      role_process_filters();

	    /* recalculate need for scores */
	    scores_are_used(SCOREUSE_INVALID);

	    /* we may want to fetch more or fewer headers each fetch */ 
	    calc_extra_hdrs();
	    if(get_extra_hdrs())
	      (void) mail_parameters(NULL, SET_IMAPEXTRAHEADERS,
				     (void *) get_extra_hdrs());

	    if(rflags & ROLE_DO_INCOLS && pico_usingcolor())
	      clear_iindex_cache();

	    /*
	     * ROLE_DO_OTHER is made up of a bunch of different variables
	     * that may have changed. Assume they all changed and fix them.
	     */
	    if(rflags & ROLE_DO_OTHER){
	        reset_index_format();
	        clear_iindex_cache();
		if(!mn_get_mansort(msgmap))
	          reset_sort_order(SRT_VRB);
	    }
	}
    }
    else
      cmd_cancelled(NULL);

    free_pat(&defpat);
    ps->mangled_screen = 1;
}


PATTERN_S *
addrlst_to_pattern(addr)
    ADDRESS *addr;
{
    char      *s, *t, *u, *v;
    PATTERN_S *p = NULL;

    if(addr){
	t = s = (char *)fs_get((est_size(addr) + 1) * sizeof(char));
	s[0] = '\0';
	while(addr){
	    u = simple_addr_string(addr, tmp_20k_buf, SIZEOF_20KBUF);
	    v = add_roletake_escapes(u);
	    if(v){
		if(*v && t != s)
		  sstrcpy(&t, ",");

		sstrcpy(&t, v);
		fs_give((void **)&v);
	    }

	    addr = addr->next;
	}

	if(*s)
	  p = string_to_pattern(s);
	
	fs_give((void **)&s);
    }

    return(p);
}


static struct key role_config_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"E", "Exit Setup", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"C", "[Change]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	{"P", "PrevRule", {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", "NextRule", {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add", {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", "Delete", {MC_DELETE,1,{'d'}}, KS_NONE},
	{"$", "Shuffle", {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
        NULL_MENU,
        NULL_MENU,
	{"I", "IncludeFile", {MC_ADDFILE,1,{'i'}}, KS_NONE},
	{"X", "eXcludeFile", {MC_DELFILE,1,{'x'}}, KS_NONE},
        NULL_MENU,
        NULL_MENU,
	{"R", "Replicate", {MC_COPY,1,{'r'}}, KS_NONE},
        NULL_MENU,
        NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(role_conf_km, role_config_keys);


void
role_config_init_disp(ps, first_line, rflags, pstate)
    struct pine     *ps;
    CONF_S         **first_line;
    long             rflags;
    PAT_STATE       *pstate;
{
    PAT_LINE_S    *patline;
    CONF_S        *ctmp = NULL;
    int            inherit = 0, added_fake = 0;

    if(first_line)
      *first_line = NULL;

    /*
     * Set cur_pat_h and manipulate directly.
     */
    set_pathandle(rflags);
    patline = *cur_pat_h ? (*cur_pat_h)->patlinehead : NULL;
    if(patline && patline->type == Inherit){
	add_patline_to_display(ps, &ctmp, 0, first_line, NULL, patline, rflags);
	patline = patline->next;
    }

    if(!patline){
	add_fake_first_role(&ctmp, 0, rflags);
	added_fake++;
	if(first_line && !*first_line)
	  (*first_line) = ctmp;
    }

    for(; patline; patline = patline->next)
      add_patline_to_display(ps, &ctmp, 0, first_line, NULL, patline, rflags);
    
    /*
     * If there are no actual patterns so far, we need to have an Add line
     * for the cursor to be on. This would happen if all of the patlines
     * were File includes and none of the files contained patterns.
     */
    if(!first_pattern(role_global_pstate) ||
       ((inherit=first_pattern(role_global_pstate)->inherit) &&
	 !next_pattern(role_global_pstate))){

	/*
	 * Find the start and prepend the fake first role.
	 */
	while(ctmp && ctmp->prev)
	  ctmp = ctmp->prev;

	if(!added_fake){
	    add_fake_first_role(&ctmp, inherit ? 0 : 1, rflags);
	    if(first_line && !*first_line)
	      (*first_line) = ctmp;
	}
    }
}


void
add_patline_to_display(ps, ctmp, before, first_line, top_line, patline, rflags)
    struct pine *ps;
    CONF_S     **ctmp;
    int          before;
    CONF_S     **first_line;
    CONF_S     **top_line;
    PAT_LINE_S  *patline;
    long         rflags;
{
    PAT_S *pat;
    int    len, firstitem;
    char  *q;

    /* put dashed line around file contents */
    if(patline->type == File){
	new_confline(ctmp);
	if(before){
	    /*
	     * New_confline appends ctmp after old current instead of inserting
	     * it, so we have to adjust. We have
	     *  <- a <-> b <-> p <-> c -> and want <- a <-> p <-> b <-> c ->
	     */

	    CONF_S *a, *b, *c, *p;

	    p = *ctmp;
	    b = (*ctmp)->prev;
	    c = (*ctmp)->next;
	    a = b ? b->prev : NULL;
	    if(a)
	      a->next = p;

	    if(b){
		b->prev = p;
		b->next = c;
	    }

	    if(c)
	      c->prev = b;

	    p->prev = a;
	    p->next = b;
	}

	if(top_line && *top_line == NULL)
	  *top_line = (*ctmp);

	(*ctmp)->value = cpystr(repeat_char(ps->ttyo->screen_cols, '-'));
	len = strlen(patline->filename);
	q = (char *)fs_get((len + 100) * sizeof(char));
	sprintf(q, "From file %s%s", patline->filename,
		patline->readonly ? " (ReadOnly)" : ""); 
	len = min(strlen(q), ps->ttyo->screen_cols -2);
	strncpy((*ctmp)->value + 2, q, len);
	fs_give((void **)&q);
	(*ctmp)->flags     |= (CF_NOSELECT | CF_STARTITEM);
	(*ctmp)->d.r.patline = patline;
	firstitem = 0;
    }
    else
      firstitem = 1;

    for(pat = patline->first; pat; pat = pat->next){
	
	/* Check that pattern has a role and is of right type */
	if(pat->inherit ||
	   (pat->action &&
	    ((rflags & ROLE_DO_ROLES)  && pat->action->is_a_role  ||
	     (rflags & ROLE_DO_INCOLS) && pat->action->is_a_incol ||
	     (rflags & ROLE_DO_OTHER)  && pat->action->is_a_other ||
	     (rflags & ROLE_DO_SCORES) && pat->action->is_a_score ||
	     (rflags & ROLE_DO_FILTER) && pat->action->is_a_filter))){
	    add_role_to_display(ctmp, patline, pat, 0,
				(first_line && *first_line == NULL)
				  ? first_line :
				    (top_line && *top_line == NULL)
				      ? top_line : NULL,
				firstitem, rflags);
	    firstitem = 1;
	    if(top_line && *top_line == NULL && first_line)
	      *top_line = *first_line;
	}

    }

    if(patline->type == File){
	new_confline(ctmp);
	(*ctmp)->value = cpystr(repeat_char(ps->ttyo->screen_cols, '-'));
	len = strlen(patline->filename);
	q = (char *)fs_get((len + 100) * sizeof(char));
	sprintf(q, "End of rules from %s", patline->filename);
	len = min(strlen(q), ps->ttyo->screen_cols -2);
	strncpy((*ctmp)->value + 2, q, len);
	fs_give((void **)&q);
	(*ctmp)->flags     |= CF_NOSELECT;
	(*ctmp)->d.r.patline = patline;
    }
}


void
add_role_to_display(ctmp, patline, pat, before, first_line, firstitem, rflags)
    CONF_S     **ctmp;
    PAT_LINE_S  *patline;
    PAT_S       *pat;
    int          before;
    CONF_S     **first_line;
    int          firstitem;
    long         rflags;
{
    char      title[80];

    if(!(pat && (pat->action || pat->inherit)))
      return;

    new_confline(ctmp);
    if(first_line && !pat->inherit)
      *first_line = *ctmp;

    if(before){
	/*
	 * New_confline appends ctmp after old current instead of inserting
	 * it, so we have to adjust. We have
	 *  <- a <-> b <-> p <-> c -> and want <- a <-> p <-> b <-> c ->
	 */

	CONF_S *a, *b, *c, *p;

	p = *ctmp;
	b = (*ctmp)->prev;
	c = (*ctmp)->next;
	a = b ? b->prev : NULL;
	if(a)
	  a->next = p;

	if(b){
	    b->prev = p;
	    b->next = c;
	}

	if(c)
	  c->prev = b;

	p->prev = a;
	p->next = b;
    }

    role_type_print(title, "HELP FOR %sRULE CONFIGURATION", rflags);

    if(pat->inherit){
	(*ctmp)->flags    |= ((firstitem ? CF_STARTITEM : 0) |
			      CF_NOSELECT | CF_INHERIT);
    }
    else{
	(*ctmp)->flags    |= (firstitem ? CF_STARTITEM : 0);
	(*ctmp)->value     = cpystr((pat && pat->patgrp && pat->patgrp->nick)
				    ? pat->patgrp->nick : "?");
    }

    (*ctmp)->d.r.patline = patline;
    (*ctmp)->d.r.pat     = pat;
    (*ctmp)->keymenu     = &role_conf_km;
    (*ctmp)->help        = (rflags & ROLE_DO_INCOLS) ? h_rules_incols :
			    (rflags & ROLE_DO_OTHER) ? h_rules_other :
			     (rflags & ROLE_DO_FILTER) ? h_rules_filter :
			      (rflags & ROLE_DO_SCORES) ? h_rules_score :
			       (rflags & ROLE_DO_ROLES)  ? h_rules_roles :
			       NO_HELP;
    (*ctmp)->help_title  = title;
    (*ctmp)->tool        = role_config_tool;
    (*ctmp)->valoffset   = 4;
}


void
add_fake_first_role(ctmp, before, rflags)
    CONF_S **ctmp;
    int      before;
    long     rflags;
{
    char title[80];
    char add[80];

    new_confline(ctmp);

    if(before){
	/*
	 * New_confline appends ctmp after old current instead of inserting
	 * it, so we have to adjust. We have
	 *  <- a <-> b <-> p <-> c -> and want <- a <-> p <-> b <-> c ->
	 */

	CONF_S *a, *b, *c, *p;

	p = *ctmp;
	b = (*ctmp)->prev;
	c = (*ctmp)->next;
	a = b ? b->prev : NULL;
	if(a)
	  a->next = p;

	if(b){
	    b->prev = p;
	    b->next = c;
	}

	if(c)
	  c->prev = b;

	p->prev = a;
	p->next = b;
    }

    role_type_print(title, "HELP FOR %sRULE CONFIGURATION", rflags);
    role_type_print(add, "Use Add to add a %sRule", rflags);

    (*ctmp)->value      = cpystr(add);
    (*ctmp)->keymenu    = &role_conf_km;
    (*ctmp)->help        = (rflags & ROLE_DO_INCOLS) ? h_rules_incols :
			    (rflags & ROLE_DO_OTHER) ? h_rules_other :
			     (rflags & ROLE_DO_FILTER) ? h_rules_filter :
			      (rflags & ROLE_DO_SCORES) ? h_rules_score :
			       (rflags & ROLE_DO_ROLES)  ? h_rules_roles :
			       NO_HELP;
    (*ctmp)->help_title = title;
    (*ctmp)->tool       = role_config_tool;
    (*ctmp)->flags     |= CF_STARTITEM;
    (*ctmp)->valoffset  = 4;
}


int
role_config_tool(ps, cmd, cl, flags)
    struct pine *ps;
    int          cmd;
    CONF_S     **cl;
    unsigned     flags;
{
    int       first_one = 0, rv = 0;
    char      exitpmt[80];
    PAT_S    *pat;

    if(!(pat = first_pattern(role_global_pstate)) ||
       (pat->inherit && !next_pattern(role_global_pstate)))
      first_one++;

    switch(cmd){
      case MC_DELETE :
	if(first_one)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
			   "Nothing to Delete, use Add");
	else
	  rv = role_config_del(ps, cl, role_global_flags);

	break;

      case MC_ADD :
	rv = role_config_add(ps, cl, role_global_flags);
	break;

      case MC_EDIT :
	if(first_one)
	  rv = role_config_add(ps, cl, role_global_flags);
	else
	  rv = role_config_edit(ps, cl, role_global_flags);

	break;

      case MC_SHUFFLE :
	if(first_one)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
			   "Nothing to Shuffle, use Add");
	else
	  rv = role_config_shuffle(ps, cl);

	break;

      case MC_EXIT :
	role_type_print(exitpmt, "%sRule Setup", role_global_flags);
	rv = screen_exit_cmd(flags, exitpmt);
	break;

      case MC_ADDFILE :
	rv = role_config_addfile(ps, cl, role_global_flags);
	break;

      case MC_DELFILE :
	rv = role_config_delfile(ps, cl, role_global_flags);
	break;

      case MC_COPY :
	if(first_one)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
			   "Nothing to Replicate, use Add");
	else
	  rv = role_config_replicate(ps, cl, role_global_flags);

	break;

      default:
	rv = -1;
	break;
    }

    return(rv);
}


/*
 * Add a new role.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_add(ps, cl, rflags)
    struct pine  *ps;
    CONF_S      **cl;
    long          rflags;
{
    int         rv = 0, first_pat = 0;
    PAT_S      *new_pat = NULL, *cur_pat;
    PAT_LINE_S *new_patline, *cur_patline;
    PAT_STATE   pstate;
    char        title[80];

    if((*cl)->d.r.patline &&
       (*cl)->d.r.patline->readonly
       && (*cl)->d.r.patline->type == File){
	q_status_message(SM_ORDER, 0, 3, "Can't add rule to ReadOnly file");
	return(rv);
    }

    role_type_print(title, "ADD A %sRULE", rflags);

    if(role_config_edit_screen(ps, NULL, title, rflags,
			       &new_pat) == 1 && new_pat){
	if(ps->never_allow_changing_from &&
	   new_pat->action &&
	   new_pat->action->from)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
      "Site policy doesn't allow changing From address so From is ignored");

	if(rflags & ROLE_DO_ROLES &&
	   new_pat->patgrp &&
	   new_pat->patgrp->nick &&
	   nonempty_patterns(ROLE_DO_ROLES, &pstate)){
	    PAT_S *pat;
	    
	    for(pat = first_pattern(&pstate);
		pat;
		pat = next_pattern(&pstate)){
		if(pat->patgrp && pat->patgrp->nick &&
		   !strucmp(pat->patgrp->nick, new_pat->patgrp->nick)){
		    q_status_message(SM_ORDER|SM_DING, 3, 7, "Warning: The nickname of the new role is already in use.");
		    break;
		}
	    }
	}
	
	rv = 1;
	cur_pat = (*cl)->d.r.pat;
	if(!cur_pat)
	  first_pat++;

	set_pathandle(rflags);
	cur_patline = first_pat ? (*cur_pat_h)->patlinehead : cur_pat->patline;

	/* need a new pat_line */
	if(first_pat || cur_patline && cur_patline->type == Literal){
	    new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
	    memset((void *)new_patline, 0, sizeof(*new_patline));
	    new_patline->type = Literal;
	    (*cur_pat_h)->dirtypinerc = 1;
	}

	if(cur_patline){
	    if(first_pat || cur_patline->type == Literal){
		new_patline->prev = cur_patline;
		new_patline->next = cur_patline->next;
		if(cur_patline->next)
		  cur_patline->next->prev = new_patline;

		cur_patline->next = new_patline;

		/* tie new patline and new pat together */
		new_pat->patline   = new_patline;
		new_patline->first = new_patline->last = new_pat;
	    }
	    else if(cur_patline->type == File){ /* don't need a new pat_line */
		/* tie together */
		new_pat->patline = cur_patline;
		cur_patline->dirty = 1;

		/* Splice new_pat after cur_pat */
		new_pat->prev = cur_pat;
		new_pat->next = cur_pat->next;
		if(cur_pat->next)
		  cur_pat->next->prev = new_pat;
		else
		  cur_patline->last = new_pat;

		cur_pat->next = new_pat;
	    }
	}
	else{
	    /* tie new first patline and pat together */
	    new_pat->patline   = new_patline;
	    new_patline->first = new_patline->last = new_pat;

	    /* set head of list */
	    (*cur_pat_h)->patlinehead = new_patline;
	}

	/*
	 * If this is the first role, we replace the "Use Add" fake role
	 * with this real one.
	 */
	if(first_pat){
	    /* Adjust conf_scroll_screen variables */
	    (*cl)->d.r.pat = new_pat;
	    (*cl)->d.r.patline = new_pat->patline;
	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    (*cl)->value = cpystr((new_pat && new_pat->patgrp &&
				   new_pat->patgrp->nick)
					 ? new_pat->patgrp->nick : "?");
	}
	/* Else we are inserting a new role after the cur role */
	else
	  add_role_to_display(cl, new_pat->patline, new_pat, 0, NULL,
			      1, rflags);
    }

    return(rv);
}


/*
 * Replicate a role.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_replicate(ps, cl, rflags)
    struct pine  *ps;
    CONF_S      **cl;
    long          rflags;
{
    int         rv = 0, first_pat = 0;
    PAT_S      *new_pat = NULL, *cur_pat, *defpat = NULL;
    PAT_LINE_S *new_patline, *cur_patline;
    PAT_STATE   pstate;
    char        title[80];

    if((*cl)->d.r.patline &&
       (*cl)->d.r.patline->readonly
       && (*cl)->d.r.patline->type == File){
	q_status_message(SM_ORDER, 0, 3, "Can't add rule to ReadOnly file");
	return(rv);
    }

    if((*cl)->d.r.pat && (defpat = copy_pat((*cl)->d.r.pat))){
	/* change nickname */
	if(defpat->patgrp && defpat->patgrp->nick){
#define CLONEWORD " Copy"
	    char *oldnick = defpat->patgrp->nick;
	    size_t len;

	    len = strlen(oldnick)+strlen(CLONEWORD)+1;
	    defpat->patgrp->nick = (char *)fs_get(len * sizeof(char));
	    strncpy(defpat->patgrp->nick, oldnick, len-1);
	    defpat->patgrp->nick[len-1] = '\0';
	    strncat(defpat->patgrp->nick, CLONEWORD,
		    len-1-strlen(defpat->patgrp->nick));
	    fs_give((void **)&oldnick);
	    if(defpat->action){
		if(defpat->action->nick)
		  fs_give((void **)&defpat->action->nick);
		
		defpat->action->nick = cpystr(defpat->patgrp->nick);
	    }
	}

	/* set this so that even if we don't edit at all, we'll be asked */
	rflags |= ROLE_CHANGES;

	role_type_print(title, "CHANGE THIS %sRULE", rflags);

	if(role_config_edit_screen(ps, defpat, title, rflags,
				   &new_pat) == 1 && new_pat){

	if(ps->never_allow_changing_from &&
	   new_pat->action &&
	   new_pat->action->from)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
      "Site policy doesn't allow changing From address so From is ignored");

	if(rflags & ROLE_DO_ROLES &&
	   new_pat->patgrp &&
	   new_pat->patgrp->nick &&
	   nonempty_patterns(ROLE_DO_ROLES, &pstate)){
	    PAT_S *pat;
	    
	    for(pat = first_pattern(&pstate);
		pat;
		pat = next_pattern(&pstate)){
		if(pat->patgrp && pat->patgrp->nick &&
		   !strucmp(pat->patgrp->nick, new_pat->patgrp->nick)){
		    q_status_message(SM_ORDER|SM_DING, 3, 7, "Warning: The nickname of the new role is already in use.");
		    break;
		}
	    }
	}
	
	rv = 1;
	cur_pat = (*cl)->d.r.pat;
	if(!cur_pat)
	  first_pat++;

	set_pathandle(rflags);
	cur_patline = first_pat ? (*cur_pat_h)->patlinehead : cur_pat->patline;

	/* need a new pat_line */
	if(first_pat || cur_patline && cur_patline->type == Literal){
	    new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
	    memset((void *)new_patline, 0, sizeof(*new_patline));
	    new_patline->type = Literal;
	    (*cur_pat_h)->dirtypinerc = 1;
	}

	if(cur_patline){
	    if(first_pat || cur_patline->type == Literal){
		new_patline->prev = cur_patline;
		new_patline->next = cur_patline->next;
		if(cur_patline->next)
		  cur_patline->next->prev = new_patline;

		cur_patline->next = new_patline;

		/* tie new patline and new pat together */
		new_pat->patline   = new_patline;
		new_patline->first = new_patline->last = new_pat;
	    }
	    else if(cur_patline->type == File){ /* don't need a new pat_line */
		/* tie together */
		new_pat->patline = cur_patline;
		cur_patline->dirty = 1;

		/* Splice new_pat after cur_pat */
		new_pat->prev = cur_pat;
		new_pat->next = cur_pat->next;
		if(cur_pat->next)
		  cur_pat->next->prev = new_pat;
		else
		  cur_patline->last = new_pat;

		cur_pat->next = new_pat;
	    }
	}
	else{
	    /* tie new first patline and pat together */
	    new_pat->patline   = new_patline;
	    new_patline->first = new_patline->last = new_pat;

	    /* set head of list */
	    (*cur_pat_h)->patlinehead = new_patline;
	}

	/*
	 * If this is the first role, we replace the "Use Add" fake role
	 * with this real one.
	 */
	if(first_pat){
	    /* Adjust conf_scroll_screen variables */
	    (*cl)->d.r.pat = new_pat;
	    (*cl)->d.r.patline = new_pat->patline;
	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    (*cl)->value = cpystr((new_pat && new_pat->patgrp &&
				   new_pat->patgrp->nick)
					 ? new_pat->patgrp->nick : "?");
	}
	/* Else we are inserting a new role after the cur role */
	else
	  add_role_to_display(cl, new_pat->patline, new_pat, 0, NULL,
			      1, rflags);
	}
    }

    if(defpat)
      free_pat(&defpat);

    return(rv);
}


/* 
 * Change the current role.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_edit(ps, cl, rflags)
    struct pine  *ps;
    CONF_S      **cl;
    long          rflags;
{
    int         rv = 0;
    PAT_S      *new_pat = NULL, *cur_pat;
    char        title[80];

    if((*cl)->d.r.patline->readonly){
	q_status_message(SM_ORDER, 0, 3, "Can't change ReadOnly rule");
	return(rv);
    }

    cur_pat = (*cl)->d.r.pat;

    role_type_print(title, "CHANGE THIS %sRULE", rflags);

    if(role_config_edit_screen(ps, cur_pat, title,
			       rflags, &new_pat) == 1 && new_pat){

	if(ps->never_allow_changing_from &&
	   new_pat->action &&
	   new_pat->action->from)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
      "Site policy doesn't allow changing From address so From is ignored");

	if(rflags & ROLE_DO_ROLES && new_pat->patgrp && new_pat->patgrp->nick){
	    PAT_S *pat;
	    
	    for(pat = first_pattern(role_global_pstate);
		pat;
		pat = next_pattern(role_global_pstate)){
		if(pat->patgrp && pat->patgrp->nick && pat != cur_pat &&
		   !strucmp(pat->patgrp->nick, new_pat->patgrp->nick)){
		    q_status_message(SM_ORDER|SM_DING, 3, 7, "Warning: The nickname of this role is also used for another role.");
		    break;
		}
	    }
	}
	
	rv = 1;

	/*
	 * Splice in new_pat in place of cur_pat
	 */

	if(cur_pat->prev)
	  cur_pat->prev->next = new_pat;

	if(cur_pat->next)
	  cur_pat->next->prev = new_pat;

	new_pat->prev = cur_pat->prev;
	new_pat->next = cur_pat->next;

	/* tie together patline and pat (new_pat gets patline in editor) */
	if(new_pat->patline->first == cur_pat)
	  new_pat->patline->first = new_pat;

	if(new_pat->patline->last == cur_pat)
	  new_pat->patline->last = new_pat;
	
	if(new_pat->patline->type == Literal){
	    set_pathandle(rflags);
	    if(*cur_pat_h)
	      (*cur_pat_h)->dirtypinerc = 1;
	}
	else
	  new_pat->patline->dirty = 1;

	cur_pat->next = NULL;
	free_pat(&cur_pat);

	/* Adjust conf_scroll_screen variables */
	(*cl)->d.r.pat = new_pat;
	(*cl)->d.r.patline = new_pat->patline;
	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	(*cl)->value = cpystr((new_pat->patgrp && new_pat->patgrp->nick)
				? new_pat->patgrp->nick : "?");
    }

    return(rv);
}


/*
 * Delete a role.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_del(ps, cl, rflags)
    struct pine  *ps;
    CONF_S      **cl;
    long          rflags;
{
    int  rv = 0;
    char msg[80];
    char prompt[100];

    if((*cl)->d.r.patline->readonly){
	q_status_message(SM_ORDER, 0, 3, "Can't delete ReadOnly rule");
	return(rv);
    }

    role_type_print(msg, "Really delete %srule", rflags);
    sprintf(prompt, "%.50s \"%.20s\" ", msg, (*cl)->value);

    ps->mangled_footer = 1;
    if(want_to(prompt,'n','n',h_config_role_del, WT_FLUSH_IN) == 'y'){
	rv = ps->mangled_body = 1;
	delete_a_role(cl, rflags);
    }
    else
      q_status_message(SM_ORDER, 0, 3, "Rule not deleted");
    
    return(rv);
}


void
delete_a_role(cl, rflags)
    CONF_S **cl;
    long     rflags;
{
    PAT_S      *cur_pat;
    CONF_S     *cp, *cq;
    PAT_LINE_S *cur_patline;
    int         inherit = 0;

    cur_pat     = (*cl)->d.r.pat;
    cur_patline = (*cl)->d.r.patline;

    if(cur_patline->type == Literal){	/* delete patline */
	set_pathandle(rflags);
	if(cur_patline->prev)
	  cur_patline->prev->next = cur_patline->next;
	else{
	    if(*cur_pat_h)		/* this has to be true */
	      (*cur_pat_h)->patlinehead = cur_patline->next;
	}

	if(cur_patline->next)
	  cur_patline->next->prev = cur_patline->prev;
	
	if(*cur_pat_h)		/* this has to be true */
	  (*cur_pat_h)->dirtypinerc = 1;

	cur_patline->next = NULL;
	free_patline(&cur_patline);
    }
    else if(cur_patline->type == File){	/* or delete pat */
	if(cur_pat->prev)
	  cur_pat->prev->next = cur_pat->next;
	else
	  cur_patline->first = cur_pat->next;

	if(cur_pat->next)
	  cur_pat->next->prev = cur_pat->prev;
	else
	  cur_patline->last = cur_pat->prev;

	cur_patline->dirty = 1;

	cur_pat->next = NULL;
	free_pat(&cur_pat);
    }

    /* delete the conf line */

    /* deleting last real rule */
    if(!first_pattern(role_global_pstate) ||
       ((inherit=first_pattern(role_global_pstate)->inherit) &&
	 !next_pattern(role_global_pstate))){

	cq = *cl;

	/*
	 * Find the start and prepend the fake first role.
	 */
	while(*cl && (*cl)->prev)
	  *cl = (*cl)->prev;

	add_fake_first_role(cl, inherit ? 0 : 1, rflags);
	snip_confline(&cq);
	opt_screen->top_line = (*cl);
	opt_screen->current = (*cl);
    }
    else{
	/* find next selectable line */
	for(cp = (*cl)->next;
	    cp && (cp->flags & CF_NOSELECT);
	    cp = cp->next)
	  ;
	
	if(!cp){	/* no next selectable, find previous selectable */
	    if(*cl == opt_screen->top_line)
	      opt_screen->top_line = (*cl)->prev;

	    for(cp = (*cl)->prev;
		cp && (cp->flags & CF_NOSELECT);
		cp = cp->prev)
	      ;
	}
	else if(*cl == opt_screen->top_line)
	  opt_screen->top_line = (*cl)->next;

	cq = *cl;
	*cl = cp;
	snip_confline(&cq);
    }
}


/*
 * Shuffle the current role up or down.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_shuffle(ps, cl)
    struct pine  *ps;
    CONF_S      **cl;
{
    int      rv = 0, deefault, i;
    int      readonlyabove = 0, readonlybelow = 0;
    ESCKEY_S opts[5];
    HelpType help;
    char     tmp[200];
    CONF_S  *a, *b;
    PAT_TYPE curtype, prevtype, nexttype;

    if(!((*cl)->prev || (*cl)->next)){
	q_status_message(SM_ORDER, 0, 3,
	   "Shuffle only makes sense when there is more than one rule defined");
	return(rv);
    }

    /* Move it up or down? */
    i = 0;
    opts[i].ch      = 'u';
    opts[i].rval    = 'u';
    opts[i].name    = "U";
    opts[i++].label = "Up";

    opts[i].ch      = 'd';
    opts[i].rval    = 'd';
    opts[i].name    = "D";
    opts[i++].label = "Down";

    opts[i].ch      = 'b';
    opts[i].rval    = 'b';
    opts[i].name    = "B";
    opts[i++].label = "Before File";

    opts[i].ch      = 'a';
    opts[i].rval    = 'a';
    opts[i].name    = "A";
    opts[i++].label = "After File";

    opts[i].ch = -1;
    deefault = 'u';

    curtype = ((*cl)->d.r.patline) ? (*cl)->d.r.patline->type : TypeNotSet;

    prevtype = ((*cl)->prev && (*cl)->prev->d.r.patline)
		? (*cl)->prev->d.r.patline->type : TypeNotSet;
    if(curtype == File && prevtype == File && (*cl)->prev->d.r.pat == NULL)
      prevtype = TypeNotSet;

    nexttype = ((*cl)->next && (*cl)->next->d.r.patline)
		? (*cl)->next->d.r.patline->type : TypeNotSet;
    if(curtype == File && nexttype == File && (*cl)->next->d.r.pat == NULL)
      nexttype = TypeNotSet;


    if(curtype == Literal){
	if(prevtype == TypeNotSet ||
	   prevtype == Inherit){	/* no up, at top	*/
	    opts[0].ch = -2;
	    opts[2].ch = -2;
	    deefault = 'd';
	}
	else if(prevtype == Literal){	/* regular up		*/
	    opts[2].ch = -2;
	}
	else if(prevtype == File){	/* file above us	*/
	    if((*cl)->prev->d.r.patline->readonly)
	      readonlyabove++;
	}

	if(nexttype == TypeNotSet){	/* no down, at bottom	*/
	    opts[1].ch = -2;
	    opts[3].ch = -2;
	}
	else if(nexttype == Literal){	/* regular down		*/
	    opts[3].ch = -2;
	}
	else if(nexttype == File){	/* file below us	*/
	    if((*cl)->next->d.r.patline->readonly)
	      readonlybelow++;
	}
    }
    else if(curtype == File){
	if((*cl)->d.r.patline && (*cl)->d.r.patline->readonly){
	    q_status_message(SM_ORDER, 0, 3, "Can't change ReadOnly file");
	    return(0);
	}

	opts[2].ch = -2;
	opts[3].ch = -2;
    }
    else{
	q_status_message(SM_ORDER, 0, 3,
	"Programming Error: unknown line type in role_shuffle");
	return(rv);
    }

    sprintf(tmp, "Shuffle \"%.50s\" %s%s%s%s%s%s%s ? ",
	    (*cl)->value,
	    (opts[0].ch != -2) ? "UP" : "",
	    (opts[0].ch != -2  && opts[1].ch != -2) ? " or " : "",
	    (opts[1].ch != -2) ? "DOWN" : "",
	    ((opts[0].ch != -2 ||
	      opts[1].ch != -2) && opts[2].ch != -2) ? " or " : "",
	    (opts[2].ch != -2) ? "BEFORE" : "",
	    ((opts[0].ch != -2 ||
	      opts[1].ch != -2 ||
	      opts[2].ch != -2) && opts[3].ch != -2) ? " or " : "",
	    (opts[3].ch != -2) ? "AFTER" : "");

    help = (opts[0].ch == -2) ? h_role_shuf_down
			      : (opts[1].ch == -2) ? h_role_shuf_up
						   : h_role_shuf;

    rv = radio_buttons(tmp, -FOOTER_ROWS(ps), opts, deefault, 'x',
		       help, RB_NORM, NULL);

    if(rv == 'x'){
	cmd_cancelled("Shuffle");
	return(0);
    }

    if((readonlyabove && rv == 'u' && curtype != prevtype) ||
       (readonlybelow && rv == 'd' && curtype != nexttype)){
	q_status_message(SM_ORDER, 0, 3, "Can't shuffle into ReadOnly file");
	return(0);
    }

    if(rv == 'u' && curtype == Literal && prevtype == Literal){
	rv = 1;
	a = (*cl)->prev;
	b = (*cl);
	if(a == opt_screen->top_line)
	  opt_screen->top_line = b;

	swap_literal_roles(a, b);
	ps->mangled_body = 1;
    }
    else if(rv == 'd' && curtype == Literal && nexttype == Literal){
	rv = 1;
	a = (*cl);
	b = (*cl)->next;
	if(a == opt_screen->top_line)
	  opt_screen->top_line = b;

	swap_literal_roles(a, b);
	ps->mangled_body = 1;
    }
    else if(rv == 'u' && curtype == File && prevtype == File){
	rv = 1;
	a = (*cl)->prev;
	b = (*cl);
	if(a == opt_screen->top_line)
	  opt_screen->top_line = b;

	swap_file_roles(a, b);
	ps->mangled_body = 1;
    }
    else if(rv == 'u' && curtype == File){
	rv = 1;
	move_role_outof_file(cl, 1);
	ps->mangled_body = 1;
    }
    else if(rv == 'd' && curtype == File && nexttype == File){
	rv = 1;
	a = (*cl);
	b = (*cl)->next;
	if(a == opt_screen->top_line)
	  opt_screen->top_line = b;

	swap_file_roles(a, b);
	ps->mangled_body = 1;
    }
    else if(rv == 'd' && curtype == File){
	rv = 1;
	if(*cl == opt_screen->top_line)
	  opt_screen->top_line = (*cl)->next;

	move_role_outof_file(cl, 0);
	ps->mangled_body = 1;
    }
    else if(rv == 'u' && curtype == Literal && prevtype == File){
	rv = 1;
	move_role_into_file(cl, 1);
	ps->mangled_body = 1;
    }
    else if(rv == 'd' && curtype == Literal && nexttype == File){
	rv = 1;
	if(*cl == opt_screen->top_line)
	  opt_screen->top_line = (*cl)->next;

	move_role_into_file(cl, 0);
	ps->mangled_body = 1;
    }
    else if(rv == 'b'){
	rv = 1;
	move_role_around_file(cl, 1);
	ps->mangled_body = 1;
    }
    else if(rv == 'a'){
	rv = 1;
	if(*cl == opt_screen->top_line)
	  opt_screen->top_line = (*cl)->next;

	move_role_around_file(cl, 0);
	ps->mangled_body = 1;
    }

    return(rv);
}


int
role_config_addfile(ps, cl, rflags)
    struct pine  *ps;
    CONF_S      **cl;
    long          rflags;
{
    char        filename[MAXPATH+1], full_filename[MAXPATH+1];
    char        dir2[MAXPATH+1], pdir[MAXPATH+1];
    char       *lc, *newfile = NULL;
    PAT_LINE_S *file_patline;
    int         rv = 0, len;
    int         r = 1, flags;
    HelpType    help = NO_HELP;
    PAT_TYPE    curtype;
    CONF_S     *first_line = NULL, *add_line, *save_current;
    struct variable *vars = ps->vars;

    if(ps->restricted){
	q_status_message(SM_ORDER, 0, 3, "Pine demo can't read files");
	return(rv);
    }

    curtype = ((*cl)->d.r.patline && (*cl)->d.r.patline)
	        ? (*cl)->d.r.patline->type : TypeNotSet;

    if(curtype == File){
	q_status_message(SM_ORDER, 0, 3, "Current rule is already part of a file. Move outside any files first.");
	return(rv);
    }

    /*
     * Parse_pattern_file uses signature_path to figure out where to look
     * for the file. In signature_path we read signature files relative
     * to the pinerc dir, so if user selects one that is in there we'll
     * use relative instead of absolute, so it looks nicer.
     */
    pdir[0] = '\0';
    if(VAR_OPER_DIR){
	strncpy(pdir, VAR_OPER_DIR, sizeof(pdir)-1);
	pdir[sizeof(pdir)-1] = '\0';
	len = strlen(pdir) + 1;
    }
    else if((lc = last_cmpnt(ps->pinerc)) != NULL){
	strncpy(pdir, ps->pinerc, min(sizeof(pdir)-1,lc-ps->pinerc));
	pdir[min(sizeof(pdir)-1, lc-ps->pinerc)] = '\0';
	len = strlen(pdir);
    }

    strncpy(dir2, pdir, sizeof(dir2)-1);
    dir2[sizeof(dir2)-1] = '\0';
    filename[0] = '\0';
    full_filename[0] = '\0';
    ps->mangled_footer = 1;

    while(1){
	flags = OE_APPEND_CURRENT;
	r = optionally_enter(filename, -FOOTER_ROWS(ps), 0, sizeof(filename),
			     "Name of file to be added to rules: ",
			     NULL, help, &flags);
	
	if(r == 3){
	    help = (help == NO_HELP) ? h_config_role_addfile : NO_HELP;
	    continue;
	}
	else if(r == 10 || r == 11){    /* Browser or File Completion */
	    continue;
	}
	else if(r == 1 || (r == 0 && filename[0] == '\0')){
	    cmd_cancelled("IncludeFile");
	    return(rv);
	}
	else if(r == 4){
	    continue;
	}
	else if(r != 0){
	    Writechar(BELL, 0);
	    continue;
	}

	removing_leading_and_trailing_white_space(filename);
	if(is_absolute_path(filename))
	  newfile = cpystr(filename);
	else{
	    build_path(full_filename, dir2, filename, sizeof(full_filename));
	    removing_leading_and_trailing_white_space(full_filename);
	    if(!strncmp(full_filename, pdir, strlen(pdir)))
	      newfile = cpystr(full_filename + len); 
	    else
	      newfile = cpystr(full_filename);
	}
	
	if(newfile && *newfile)
	  break;
    }

    if(!newfile)
      return(rv);
    
    set_pathandle(rflags);

    if((file_patline = parse_pat_file(newfile)) != NULL){
	/*
	 * Insert the file after the current line.
	 */
	PAT_LINE_S *cur_patline;
	int         first_pat;

	rv = ps->mangled_screen = 1;
	first_pat = !(*cl)->d.r.pat;
	cur_patline = (*cl)->d.r.patline ? (*cl)->d.r.patline :
		       (*cur_pat_h) ? (*cur_pat_h)->patlinehead : NULL;

	if(*cur_pat_h)
	  (*cur_pat_h)->dirtypinerc = 1;

	file_patline->dirty = 1;

	if(cur_patline){
	    file_patline->prev = cur_patline;
	    file_patline->next = cur_patline->next;
	    if(cur_patline->next)
	      cur_patline->next->prev = file_patline;

	    cur_patline->next = file_patline;
	}
	else{
	    /* set head of list */
	    if(*cur_pat_h)
	      (*cur_pat_h)->patlinehead = file_patline;
	}

	if(first_pat){
	    if(file_patline->first){
		/* get rid of Fake Add line */
		add_line = *cl;
		opt_screen->top_line = NULL;
		add_patline_to_display(ps, cl, 0, &first_line,
				       &opt_screen->top_line, file_patline,
				       rflags);
		opt_screen->current = first_line;
		snip_confline(&add_line);
	    }
	    else{
		/* we're _appending_ to the Fake Add line */
		save_current = opt_screen->current;
		add_patline_to_display(ps, cl, 0, NULL, NULL, file_patline,
				       rflags);
		opt_screen->current = save_current;
	    }
	}
	else{
	    opt_screen->top_line = NULL;
	    save_current = opt_screen->current;
	    add_patline_to_display(ps, cl, 0, &first_line,
				   &opt_screen->top_line, file_patline,
				   rflags);
	    if(first_line)
	      opt_screen->current = first_line;
	    else
	      opt_screen->current = save_current;
	}
    }

    if(newfile)
      fs_give((void **)&newfile);
    
    return(rv);
}


int
role_config_delfile(ps, cl, rflags)
    struct pine  *ps;
    CONF_S      **cl;
    long          rflags;
{
    int         rv = 0;
    PAT_LINE_S *cur_patline;
    char        prompt[100];

    if(!(cur_patline = (*cl)->d.r.patline)){
	q_status_message(SM_ORDER, 0, 3,
			 "Unknown problem in role_config_delfile");
	return(rv);
    }

    if(cur_patline->type != File){
	q_status_message(SM_ORDER, 0, 3, "Current rule is not part of a file. Use Delete to remove current rule");
	return(rv);
    }

    sprintf(prompt, "Really remove rule file \"%.20s\" from rules config ",
	    cur_patline->filename);

    ps->mangled_footer = 1;
    if(want_to(prompt,'n','n',h_config_role_delfile, WT_FLUSH_IN) == 'y'){
	CONF_S *ctmp, *cp;

	set_pathandle(rflags);
	rv = ps->mangled_screen = 1;
	if(*cur_pat_h)
	  (*cur_pat_h)->dirtypinerc = 1;

	if(cur_patline->prev)
	  cur_patline->prev->next = cur_patline->next;
	else{
	    if(*cur_pat_h)
	      (*cur_pat_h)->patlinehead = cur_patline->next;
	}

	if(cur_patline->next)
	  cur_patline->next->prev = cur_patline->prev;
	
	/* delete the conf lines */

	/* find the first one associated with this file */
	for(ctmp = *cl;
	    ctmp && ctmp->prev && ctmp->prev->d.r.patline == cur_patline;
	    ctmp = ctmp->prev)
	  ;
	
	if(ctmp->prev)	/* this file wasn't the first thing in config */
	  *cl = ctmp->prev;
	else{		/* this file was first in config */
	    for(cp = ctmp; cp && cp->next; cp = cp->next)
	      ;

	    if(cp->d.r.patline == cur_patline)
	      *cl = NULL;
	    else
	      *cl = cp;
	}
	
	/* delete lines from the file */
	while(ctmp && ctmp->d.r.patline == cur_patline){
	    cp = ctmp;
	    ctmp = ctmp->next;
	    snip_confline(&cp);
	}

	/* deleting last real rule */
	if(!first_pattern(role_global_pstate)){
	    /*
	     * Find the start and prepend the fake first role
	     * in there.
	     */
	    while(*cl && (*cl)->prev)
	      *cl = (*cl)->prev;

	    add_fake_first_role(cl, 1, rflags);
	}
	else if(first_pattern(role_global_pstate)->inherit &&
	       !next_pattern(role_global_pstate)){
	    while(*cl && (*cl)->prev)
	      *cl = (*cl)->prev;
	    
	    /* append fake first after inherit */
	    add_fake_first_role(cl, 0, rflags);
	}

	opt_screen->top_line = first_confline(*cl);
	opt_screen->current  = first_sel_confline(opt_screen->top_line);

	cur_patline->next = NULL;
	free_patline(&cur_patline);
    }
    else
      q_status_message(SM_ORDER, 0, 3, "Rule file not removed");
    
    return(rv);
}


/*
 * Swap from a, b to b, a.
 */
void
swap_literal_roles(a, b)
    CONF_S *a, *b;
{
    PAT_LINE_S *patline_a, *patline_b;

    patline_a = a->d.r.patline;
    patline_b = b->d.r.patline;

    set_pathandle(role_global_flags);
    if(*cur_pat_h)
      (*cur_pat_h)->dirtypinerc = 1;

    /* first swap the patlines */
    if(patline_a->next == patline_b){
	patline_b->prev = patline_a->prev;
	if(patline_a->prev)
	  patline_a->prev->next = patline_b;

	patline_a->next = patline_b->next;
	if(patline_b->next)
	  patline_b->next->prev = patline_a;

	patline_b->next = patline_a;
	patline_a->prev = patline_b;
    }
    else{
	PAT_LINE_S *new_a_prev, *new_a_next;

	new_a_prev = patline_b->prev;
	new_a_next = patline_b->next;

	patline_b->prev = patline_a->prev;
	patline_b->next = patline_a->next;
	if(patline_b->prev)
	  patline_b->prev->next = patline_b;
	if(patline_b->next)
	  patline_b->next->prev = patline_b;

	patline_a->prev = new_a_prev;
	patline_a->next = new_a_next;
	if(patline_a->prev)
	  patline_a->prev->next = patline_a;
	if(patline_a->next)
	  patline_a->next->prev = patline_a;
    }

    /*
     * If patline_b is now the first one in the list, we need to fix the
     * head of the list to point to this new role.
     */
    if(patline_b->prev == NULL && *cur_pat_h)
      (*cur_pat_h)->patlinehead = patline_b;


    /* and then swap the conf lines */

    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    
    a->next = b->next;
    if(b->next)
      b->next->prev = a;
    
    b->next = a;
    a->prev = b;
}


/*
 * Swap from a, b to b, a.
 */
void
swap_file_roles(a, b)
    CONF_S *a, *b;
{
    PAT_S      *pat_a, *pat_b;
    PAT_LINE_S *patline;

    pat_a = a->d.r.pat;
    pat_b = b->d.r.pat;
    patline = pat_a->patline;

    patline->dirty = 1;

    /* first swap the pats */
    if(pat_a->next == pat_b){
	pat_b->prev = pat_a->prev;
	if(pat_a->prev)
	  pat_a->prev->next = pat_b;
	
	pat_a->next = pat_b->next;
	if(pat_b->next)
	  pat_b->next->prev = pat_a;
	
	pat_b->next = pat_a;
	pat_a->prev = pat_b;
    }
    else{
	PAT_S *new_a_prev, *new_a_next;

	new_a_prev = pat_b->prev;
	new_a_next = pat_b->next;

	pat_b->prev = pat_a->prev;
	pat_b->next = pat_a->next;
	if(pat_b->prev)
	  pat_b->prev->next = pat_b;
	if(pat_b->next)
	  pat_b->next->prev = pat_b;

	pat_a->prev = new_a_prev;
	pat_a->next = new_a_next;
	if(pat_a->prev)
	  pat_a->prev->next = pat_a;
	if(pat_a->next)
	  pat_a->next->prev = pat_a;
    }

    /*
     * Fix the first and last pointers.
     */
    if(patline->first == pat_a)
      patline->first = pat_b;
    if(patline->last == pat_b)
      patline->last = pat_a;

    /* and then swap the conf lines */

    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    
    a->next = b->next;
    if(b->next)
      b->next->prev = a;
    
    b->next = a;
    a->prev = b;
}


/*
 */
void
move_role_into_file(cl, up)
    CONF_S **cl;
    int      up;
{
    PAT_LINE_S *cur_patline, *file_patline;
    PAT_S      *pat;
    CONF_S     *a, *b;

    cur_patline = (*cl)->d.r.patline;

    if(up){
	file_patline = (*cl)->prev->d.r.patline;
	a = (*cl)->prev;
	b = (*cl);
	b->d.r.patline = file_patline;
    }
    else{
	file_patline = (*cl)->next->d.r.patline;
	a = (*cl);
	b = (*cl)->next;
	a->d.r.patline = file_patline;
    }

    set_pathandle(role_global_flags);
    if(*cur_pat_h)
      (*cur_pat_h)->dirtypinerc = 1;

    file_patline->dirty = 1;

    pat = cur_patline->first;

    if(!up && *cur_pat_h && cur_patline == (*cur_pat_h)->patlinehead)
      (*cur_pat_h)->patlinehead = (*cur_pat_h)->patlinehead->next;

    if(file_patline->first){
	if(up){
	    file_patline->last->next = pat;
	    pat->prev = file_patline->last;
	    file_patline->last = pat;
	}
	else{
	    file_patline->first->prev = pat;
	    pat->next = file_patline->first;
	    file_patline->first = pat;
	}
    }
    else		/* will be only role in file */
      file_patline->first = file_patline->last = pat;

    pat->patline = file_patline;

    /* delete the now unused cur_patline */
    cur_patline->first = cur_patline->last = NULL;
    if(cur_patline->prev)
      cur_patline->prev->next = cur_patline->next;
    if(cur_patline->next)
      cur_patline->next->prev = cur_patline->prev;
    
    cur_patline->next = NULL;
    free_patline(&cur_patline);

    /* and then swap the conf lines */

    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    
    a->next = b->next;
    if(b->next)
      b->next->prev = a;
    
    b->next = a;
    a->prev = b;
}


/*
 */
void
move_role_outof_file(cl, up)
    CONF_S **cl;
    int      up;
{
    PAT_LINE_S *file_patline, *new_patline;
    PAT_S      *pat;
    CONF_S     *a, *b;

    new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
    memset((void *)new_patline, 0, sizeof(*new_patline));
    new_patline->type = Literal;

    file_patline = (*cl)->d.r.patline;
    pat = (*cl)->d.r.pat;

    if(up){
	a = (*cl)->prev;
	b = (*cl);

	if(pat->prev)
	  pat->prev->next = pat->next;
	else
	  file_patline->first = pat->next;

	if(pat->next)
	  pat->next->prev = pat->prev;
	else
	  file_patline->last = pat->prev;

	if(file_patline->first)
	  file_patline->first->prev = NULL;

	if(file_patline->last)
	  file_patline->last->next = NULL;
	
	if(file_patline->prev)
	  file_patline->prev->next = new_patline;
	
	new_patline->prev = file_patline->prev;
	new_patline->next = file_patline;
	file_patline->prev = new_patline;
	b->d.r.patline = new_patline;
    }
    else{
	a = (*cl);
	b = (*cl)->next;

	if(pat->prev)
	  pat->prev->next = pat->next;
	else
	  file_patline->first = pat->next;

	if(pat->next)
	  pat->next->prev = pat->prev;
	else
	  file_patline->last = pat->prev;

	if(file_patline->first)
	  file_patline->first->prev = NULL;

	if(file_patline->last)
	  file_patline->last->next = NULL;

	if(file_patline->next)
	  file_patline->next->prev = new_patline;
	
	new_patline->next = file_patline->next;
	new_patline->prev = file_patline;
	file_patline->next = new_patline;
	a->d.r.patline = new_patline;
    }

    set_pathandle(role_global_flags);
    if(*cur_pat_h)
      (*cur_pat_h)->dirtypinerc = 1;

    file_patline->dirty = 1;

    new_patline->first = new_patline->last = pat;
    pat->patline = new_patline;
    pat->prev = pat->next = NULL;

    if(up && *cur_pat_h && file_patline == (*cur_pat_h)->patlinehead)
      (*cur_pat_h)->patlinehead = new_patline;

    /* and then swap the conf lines */

    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    
    a->next = b->next;
    if(b->next)
      b->next->prev = a;
    
    b->next = a;
    a->prev = b;
}


/*
 * This is a move of a literal role from before a file to after a file,
 * or vice versa.
 */
void
move_role_around_file(cl, up)
    CONF_S **cl;
    int      up;
{
    PAT_LINE_S *file_patline, *lit_patline;
    CONF_S     *cp;

    set_pathandle(role_global_flags);
    lit_patline = (*cl)->d.r.patline;
    if(up)
      file_patline = (*cl)->prev->d.r.patline;
    else{
	if(*cur_pat_h && lit_patline == (*cur_pat_h)->patlinehead)
	  (*cur_pat_h)->patlinehead = (*cur_pat_h)->patlinehead->next;

	file_patline = (*cl)->next->d.r.patline;
    }

    if(*cur_pat_h)
      (*cur_pat_h)->dirtypinerc = 1;

    /* remove the lit_patline from the list */
    if(lit_patline->prev)
      lit_patline->prev->next = lit_patline->next;
    if(lit_patline->next)
      lit_patline->next->prev = lit_patline->prev;

    /* and reinsert it on the other side of the file */
    if(up){
	if(*cur_pat_h && file_patline == (*cur_pat_h)->patlinehead)
	  (*cur_pat_h)->patlinehead = lit_patline;

	lit_patline->prev = file_patline->prev;
	lit_patline->next = file_patline;

	if(file_patline->prev)
	  file_patline->prev->next = lit_patline;
	
	file_patline->prev = lit_patline;
    }
    else{
	lit_patline->next = file_patline->next;
	lit_patline->prev = file_patline;

	if(file_patline->next)
	  file_patline->next->prev = lit_patline;
	
	file_patline->next = lit_patline;
    }

    /*
     * And then move the conf line around the file conf lines.
     */

    /* find it's new home */
    if(up)
      for(cp = (*cl);
	  cp && cp->prev && cp->prev->d.r.patline == file_patline;
	  cp = prev_confline(cp))
	;
    else
      for(cp = (*cl);
	  cp && cp->next && cp->next->d.r.patline == file_patline;
	  cp = next_confline(cp))
	;

    /* remove it from where it is */
    if((*cl)->prev)
      (*cl)->prev->next = (*cl)->next;
    if((*cl)->next)
      (*cl)->next->prev = (*cl)->prev;
    
    /* cp points to top or bottom of the file lines */
    if(up){
	(*cl)->prev = cp->prev;
	if(cp->prev)
	  cp->prev->next = (*cl);
	
	cp->prev = (*cl);
	(*cl)->next = cp;
    }
    else{
	(*cl)->next = cp->next;
	if(cp->next)
	  cp->next->prev = (*cl);
	
	cp->next = (*cl);
	(*cl)->prev = cp;
    }
}


#define SETUP_PAT_STATUS(ctmp,svar,val,htitle,hval)			\
   {char tmp[MAXPATH+1];						\
    int i, j, lv;							\
    NAMEVAL_S *f;							\
									\
    /* Blank line */							\
    new_confline(&ctmp);						\
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;				\
									\
    new_confline(&ctmp);						\
    ctmp->var       = &svar;						\
    ctmp->valoffset = indent;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = NULL;						\
    sprintf(tmp, "%-*.100s =", indent-3, svar.name);			\
    ctmp->varname   = cpystr(tmp);					\
    ctmp->varnamep  = ctmpb = ctmp;					\
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);			\
									\
    new_confline(&ctmp);						\
    ctmp->var       = NULL;						\
    ctmp->valoffset = 12;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = NULL;						\
    ctmp->varnamep  = ctmpb;						\
    ctmp->flags    |= CF_NOSELECT;					\
    ctmp->value     = cpystr("Set    Choose One");			\
									\
    new_confline(&ctmp);						\
    ctmp->var       = NULL;						\
    ctmp->valoffset = 12;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = radio_tool;					\
    ctmp->varnamep  = ctmpb;						\
    ctmp->flags    |= CF_NOSELECT;					\
    ctmp->value     = cpystr("---  --------------------");		\
									\
    /* find longest value's name */					\
    for(lv = 0, i = 0; f = role_status_types(i); i++)			\
      if(lv < (j = strlen(f->name)))					\
	lv = j;								\
    									\
    lv = min(lv, 100);							\
    									\
    for(i = 0; f = role_status_types(i); i++){				\
	new_confline(&ctmp);						\
	ctmp->help_title= htitle;					\
	ctmp->var       = &svar;					\
	ctmp->valoffset = 12;						\
	ctmp->keymenu   = &config_radiobutton_keymenu;			\
	ctmp->help      = hval;						\
	ctmp->varmem    = i;						\
	ctmp->tool      = radio_tool;					\
	ctmp->varnamep  = ctmpb;					\
	sprintf(tmp, "(%c)  %-*.*s", (((!(def && def->patgrp) ||	\
					 val == -1) &&			\
					f->value == PAT_STAT_EITHER) ||	\
				      (def && def->patgrp &&		\
				      f->value == val))			\
					 ? R_SELD : ' ',		\
		lv, lv, f->name);					\
	ctmp->value     = cpystr(tmp);					\
    }									\
   }

#define SETUP_MSG_STATE(ctmp,svar,val,htitle,hval)			\
   {char tmp[MAXPATH+1];						\
    int i, j, lv;							\
    NAMEVAL_S *f;							\
									\
    /* Blank line */							\
    new_confline(&ctmp);						\
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;				\
									\
    new_confline(&ctmp);						\
    ctmp->var       = &svar;						\
    ctmp->valoffset = indent;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = NULL;						\
    sprintf(tmp, "%-*.100s =", indent-3, svar.name);			\
    ctmp->varname   = cpystr(tmp);					\
    ctmp->varnamep  = ctmpb = ctmp;					\
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);			\
									\
    new_confline(&ctmp);						\
    ctmp->var       = NULL;						\
    ctmp->valoffset = 12;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = NULL;						\
    ctmp->varnamep  = ctmpb;						\
    ctmp->flags    |= CF_NOSELECT;					\
    ctmp->value     = cpystr("Set    Choose One");			\
									\
    new_confline(&ctmp);						\
    ctmp->var       = NULL;						\
    ctmp->valoffset = 12;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = radio_tool;					\
    ctmp->varnamep  = ctmpb;						\
    ctmp->flags    |= CF_NOSELECT;					\
    ctmp->value     = cpystr("---  --------------------");		\
									\
    /* find longest value's name */					\
    for(lv = 0, i = 0; f = msg_state_types(i); i++)			\
      if(lv < (j = strlen(f->name)))					\
	lv = j;								\
    									\
    lv = min(lv, 100);							\
    									\
    for(i = 0; f = msg_state_types(i); i++){				\
	new_confline(&ctmp);						\
	ctmp->help_title= htitle;					\
	ctmp->var       = &svar;					\
	ctmp->valoffset = 12;						\
	ctmp->keymenu   = &config_radiobutton_keymenu;			\
	ctmp->help      = hval;						\
	ctmp->varmem    = i;						\
	ctmp->tool      = radio_tool;					\
	ctmp->varnamep  = ctmpb;					\
	sprintf(tmp, "(%c)  %-*.*s", (f->value == val)			\
					 ? R_SELD : ' ',		\
		lv, lv, f->name);					\
	ctmp->value     = cpystr(tmp);					\
    }									\
   }


#define  FEAT_SENTDATE   0
#define  FEAT_IFNOTDEL   1
#define  FEAT_NONTERM    2
bitmap_t feat_option_list;

#define INICK_INICK_CONF    0
#define INICK_FROM_CONF     1
#define INICK_REPLYTO_CONF  2
#define INICK_FCC_CONF      3
#define INICK_LITSIG_CONF   4	/* this needs to come before SIG_CONF */
#define INICK_SIG_CONF      5
#define INICK_TEMPL_CONF    6
#define INICK_CSTM_CONF     7
#define INICK_SMTP_CONF     8
#define INICK_NNTP_CONF     9
CONF_S *inick_confs[INICK_NNTP_CONF+1];


/*
 * Screen for editing configuration of a role.
 *
 * Args     ps -- pine struct
 *         def -- default role values to start with
 *       title -- part of title at top of screen
 *      rflags -- which parts of role to edit
 *      result -- This is the returned PAT_S, freed by caller.
 *
 * Returns:  0 if no change
 *           1 if user requested a change
 *               (change is stored in raw_server and hasn't been acted upon yet)
 *          10 user says abort
 */
int
role_config_edit_screen(ps, def, title, rflags, result)
    struct pine *ps;
    PAT_S       *def;
    char        *title;
    long         rflags;
    PAT_S      **result;
{
    OPT_SCREEN_S     screen, *saved_screen;
    CONF_S          *ctmp = NULL, *ctmpb, *first_line = NULL;
    struct variable  nick_var, to_pat_var, from_pat_var,
		     comment_var,
		     sender_pat_var, cc_pat_var, recip_pat_var, news_pat_var,
		     subj_pat_var, inick_var, fldr_type_var, folder_pat_var,
		     abook_type_var, abook_pat_var,
		     alltext_pat_var, scorei_pat_var, partic_pat_var,
		     bodytext_pat_var, age_pat_var, size_pat_var,
		     keyword_pat_var, charset_pat_var,
		     stat_new_var, stat_del_var, stat_imp_var, stat_ans_var,
		     stat_rec_var, stat_8bit_var,
		     stat_bom_var, stat_boy_var,
		     cat_cmd_var, cati_var, cat_lim_var,
		     from_act_var, replyto_act_var, fcc_act_var,
		     sig_act_var, litsig_act_var, templ_act_var,
                     cstm_act_var, smtp_act_var, nntp_act_var,
		     sort_act_var, iform_act_var, startup_var,
		     repl_type_var, forw_type_var, comp_type_var, score_act_var,
		     rolecolor_vars[2], filter_type_var, folder_act_var,
		     keyword_set_var, keyword_clr_var,
		     filt_new_var, filt_del_var, filt_imp_var, filt_ans_var;
    struct variable *v, *varlist[63], opt_var;
    char            *nick = NULL, *inick = NULL, *fldr_type_pat = NULL,
		    *comment = NULL,
		    *scorei_pat = NULL, *age_pat = NULL, *size_pat = NULL,
		    *abook_type_pat = NULL,
		    *stat_new = NULL, *stat_del = NULL, *stat_imp = NULL,
		    *stat_rec = NULL, *stat_ans = NULL, *stat_8bit = NULL,
		    *stat_bom = NULL, *stat_boy = NULL,
		    *filt_new = NULL, *filt_del = NULL, *filt_imp = NULL,
		    *filt_ans = NULL, *cati = NULL, *cat_lim = NULL,
		    *from_act = NULL, *replyto_act = NULL, *fcc_act = NULL,
		    *sig_act = NULL, *litsig_act = NULL, *sort_act = NULL,
		    *templ_act = NULL, *repl_type = NULL, *forw_type = NULL,
		    *comp_type = NULL, *rc_fg = NULL, *rc_bg = NULL,
		    *score_act = NULL, *filter_type = NULL,
		    *iform_act = NULL, *startup_act = NULL,
		    *old_fg = NULL, *old_bg = NULL;
    char           **to_pat = NULL, **from_pat = NULL, **sender_pat = NULL,
		   **cc_pat = NULL, **news_pat = NULL, **recip_pat = NULL,
		   **partic_pat = NULL, **subj_pat = NULL,
		   **alltext_pat = NULL, **bodytext_pat = NULL,
		   **keyword_pat = NULL, **folder_pat = NULL,
		   **charset_pat = NULL,
		   **abook_pat = NULL, **folder_act = NULL,
		   **keyword_set = NULL, **keyword_clr = NULL,
		   **cat_cmd = NULL, **cstm_act = NULL, **smtp_act = NULL,
		   **nntp_act = NULL, **spat;
    char             tmp[MAXPATH+1], **apval, **lval, ***alval, *p;
    char            *fstr = " CURRENT FOLDER CONDITIONS BEGIN HERE ";
    char             mstr[50];
    char            *astr = " ACTIONS BEGIN HERE ";
    char            *ustr = " USES BEGIN HERE ";
    char            *ostr = " OPTIONS BEGIN HERE ";
    SortOrder        def_sort;
    int              def_sort_rev;
    ARBHDR_S        *aa, *a;
    EARB_S          *earb = NULL, *ea;
    int              rv, i, j, lv, indent = 18, pindent,
		     scoreval = 0, edit_role,
		     edit_incol, edit_score, edit_filter, edit_other,
		     dval, ival, nval, aval, fval, noselect,
		     per_folder_only, need_uses, need_options;
    int	        (*radio_tool) PROTO((struct pine *, int, CONF_S **, unsigned));
    int	        (*addhdr_tool) PROTO((struct pine *, int, CONF_S **, unsigned));
    int	        (*t_tool) PROTO((struct pine *, int, CONF_S **, unsigned));
    NAMEVAL_S       *f;

    dprint(4,(debugfile, "role_config_edit_screen()\n"));
    edit_role	= rflags & ROLE_DO_ROLES;
    edit_incol	= rflags & ROLE_DO_INCOLS;
    edit_score	= rflags & ROLE_DO_SCORES;
    edit_filter	= rflags & ROLE_DO_FILTER;
    edit_other	= rflags & ROLE_DO_OTHER;

    per_folder_only = (edit_other &&
		       !(edit_role || edit_incol || edit_score || edit_filter));
    need_uses       = edit_role;
    need_options    = !per_folder_only;

    radio_tool = edit_filter ? role_filt_radiobutton_tool
			     : role_radiobutton_tool;
    t_tool = edit_filter ? role_filt_text_tool : role_text_tool;
    addhdr_tool = edit_filter ? role_filt_addhdr_tool : role_addhdr_tool;

    /*
     * We edit by making a nested call to conf_scroll_screen.
     * We use some fake struct variables to get back the results in, and
     * so we can use the existing tools from the config screen.
     */
    varlist[j = 0] = &nick_var;
    varlist[++j] = &comment_var;
    varlist[++j] = &to_pat_var;
    varlist[++j] = &from_pat_var;
    varlist[++j] = &sender_pat_var;
    varlist[++j] = &cc_pat_var;
    varlist[++j] = &recip_pat_var;
    varlist[++j] = &partic_pat_var;
    varlist[++j] = &news_pat_var;
    varlist[++j] = &subj_pat_var;
    varlist[++j] = &alltext_pat_var;
    varlist[++j] = &bodytext_pat_var;
    varlist[++j] = &keyword_pat_var;
    varlist[++j] = &charset_pat_var;
    varlist[++j] = &age_pat_var;
    varlist[++j] = &size_pat_var;
    varlist[++j] = &scorei_pat_var;
    varlist[++j] = &stat_new_var;
    varlist[++j] = &stat_rec_var;
    varlist[++j] = &stat_del_var;
    varlist[++j] = &stat_imp_var;
    varlist[++j] = &stat_ans_var;
    varlist[++j] = &stat_8bit_var;
    varlist[++j] = &stat_bom_var;
    varlist[++j] = &stat_boy_var;
    varlist[++j] = &cat_cmd_var;
    varlist[++j] = &cati_var;
    varlist[++j] = &cat_lim_var;
    varlist[++j] = &inick_var;
    varlist[++j] = &fldr_type_var;
    varlist[++j] = &folder_pat_var;
    varlist[++j] = &abook_type_var;
    varlist[++j] = &abook_pat_var;
    varlist[++j] = &from_act_var;
    varlist[++j] = &replyto_act_var;
    varlist[++j] = &fcc_act_var;
    varlist[++j] = &sig_act_var;
    varlist[++j] = &litsig_act_var;
    varlist[++j] = &sort_act_var;
    varlist[++j] = &iform_act_var;
    varlist[++j] = &startup_var;
    varlist[++j] = &templ_act_var;
    varlist[++j] = &cstm_act_var;
    varlist[++j] = &smtp_act_var;
    varlist[++j] = &nntp_act_var;
    varlist[++j] = &score_act_var;
    varlist[++j] = &repl_type_var;
    varlist[++j] = &forw_type_var;
    varlist[++j] = &comp_type_var;
    varlist[++j] = &rolecolor_vars[0];
    varlist[++j] = &rolecolor_vars[1];
    varlist[++j] = &filter_type_var;
    varlist[++j] = &folder_act_var;
    varlist[++j] = &keyword_set_var;
    varlist[++j] = &keyword_clr_var;
    varlist[++j] = &filt_new_var;
    varlist[++j] = &filt_del_var;
    varlist[++j] = &filt_imp_var;
    varlist[++j] = &filt_ans_var;
    varlist[++j] = &opt_var;
    varlist[++j] = NULL;
    for(j = 0; varlist[j]; j++)
      memset(varlist[j], 0, sizeof(struct variable));

    if(def && ((def->patgrp && def->patgrp->bogus) || (def->action && def->action->bogus))){
	char msg[MAX_SCREEN_COLS+1];

	sprintf(msg,
		"Rule contains unknown %s element, possibly from newer Pine",
		(def->patgrp && def->patgrp->bogus) ? "pattern" : "action");
	q_status_message(SM_ORDER | SM_DING, 7, 7, msg);
	q_status_message(SM_ORDER | SM_DING, 7, 7,
		"Editing with this version of Pine will destroy information");
        flush_status_messages(0);
    }

    role_forw_ptr = role_repl_ptr = role_fldr_ptr = role_filt_ptr = NULL;
    role_status1_ptr = role_status2_ptr = role_status3_ptr = NULL;
    role_status4_ptr = role_status5_ptr = role_status6_ptr = NULL;
    role_status7_ptr = NULL; role_status8_ptr = NULL;
    msg_state1_ptr = msg_state2_ptr = NULL;
    msg_state3_ptr = msg_state4_ptr = NULL;
    role_afrom_ptr = startup_ptr = NULL;
    role_comment_ptr = NULL;

    nick_var.name       = cpystr("Nickname");
    nick_var.is_used    = 1;
    nick_var.is_user    = 1;
    apval = APVAL(&nick_var, ew);
    *apval = (def && def->patgrp && def->patgrp->nick)
				? cpystr(def->patgrp->nick) : NULL;

    nick_var.global_val.p = cpystr(edit_role
				    ? "Alternate Role"
				    : (edit_other
				       ? "Other Rule"
				       : (edit_incol
					  ? "Index Color Rule"
					  : (edit_score
					     ? "Score Rule"
					     : "Filter Rule"))));
    set_current_val(&nick_var, FALSE, FALSE);

    role_comment_ptr    = &comment_var;		/* so radiobuttons can tell */
    comment_var.name    = cpystr("Comment");
    comment_var.is_used = 1;
    comment_var.is_user = 1;
    apval = APVAL(&comment_var, ew);
    *apval = (def && def->patgrp && def->patgrp->comment)
				? cpystr(def->patgrp->comment) : NULL;
    set_current_val(&comment_var, FALSE, FALSE);

    setup_dummy_pattern_var(&to_pat_var, "To pattern",
			   (def && def->patgrp) ? def->patgrp->to : NULL);
    setup_dummy_pattern_var(&from_pat_var, "From pattern",
			   (def && def->patgrp) ? def->patgrp->from : NULL);
    setup_dummy_pattern_var(&sender_pat_var, "Sender pattern",
			   (def && def->patgrp) ? def->patgrp->sender : NULL);
    setup_dummy_pattern_var(&cc_pat_var, "Cc pattern",
			   (def && def->patgrp) ? def->patgrp->cc : NULL);
    setup_dummy_pattern_var(&news_pat_var, "News pattern",
			   (def && def->patgrp) ? def->patgrp->news : NULL);
    setup_dummy_pattern_var(&subj_pat_var, "Subject pattern",
			   (def && def->patgrp) ? def->patgrp->subj : NULL);
    setup_dummy_pattern_var(&recip_pat_var, "Recip pattern",
			   (def && def->patgrp) ? def->patgrp->recip : NULL);
    setup_dummy_pattern_var(&partic_pat_var, "Partic pattern",
			   (def && def->patgrp) ? def->patgrp->partic : NULL);
    setup_dummy_pattern_var(&alltext_pat_var, "AllText pattern",
			   (def && def->patgrp) ? def->patgrp->alltext : NULL);
    setup_dummy_pattern_var(&bodytext_pat_var, "BdyText pattern",
			   (def && def->patgrp) ? def->patgrp->bodytext : NULL);

    setup_dummy_pattern_var(&keyword_pat_var, "Keyword pattern",
			   (def && def->patgrp) ? def->patgrp->keyword : NULL);
    setup_dummy_pattern_var(&charset_pat_var, "Charset pattern",
			   (def && def->patgrp) ? def->patgrp->charsets : NULL);

    age_pat_global_ptr     = &age_pat_var;
    age_pat_var.name       = cpystr("Age interval");
    age_pat_var.is_used    = 1;
    age_pat_var.is_user    = 1;
    if(def && def->patgrp && def->patgrp->do_age){
	apval = APVAL(&age_pat_var, ew);
	*apval = stringform_of_intvl(def->patgrp->age);
    }

    set_current_val(&age_pat_var, FALSE, FALSE);

    size_pat_global_ptr     = &size_pat_var;
    size_pat_var.name       = cpystr("Size interval");
    size_pat_var.is_used    = 1;
    size_pat_var.is_user    = 1;
    if(def && def->patgrp && def->patgrp->do_size){
	apval = APVAL(&size_pat_var, ew);
	*apval = stringform_of_intvl(def->patgrp->size);
    }

    set_current_val(&size_pat_var, FALSE, FALSE);

    scorei_pat_global_ptr     = &scorei_pat_var;
    scorei_pat_var.name       = cpystr("Score interval");
    scorei_pat_var.is_used    = 1;
    scorei_pat_var.is_user    = 1;
    if(def && def->patgrp && def->patgrp->do_score){
	apval = APVAL(&scorei_pat_var, ew);
	*apval = stringform_of_intvl(def->patgrp->score);
    }

    set_current_val(&scorei_pat_var, FALSE, FALSE);

    pindent = strlen(subj_pat_var.name);	/* the longest one */
    for(a = (def && def->patgrp) ? def->patgrp->arbhdr : NULL; a; a = a->next)
      if((lv=strlen(a->field ? a->field : "")+8) > pindent)
	pindent = lv;
    
    pindent += NOTLEN;

    role_status1_ptr = &stat_del_var;		/* so radiobuttons can tell */
    stat_del_var.name       = cpystr("Message is Deleted?");
    stat_del_var.is_used    = 1;
    stat_del_var.is_user    = 1;
    apval = APVAL(&stat_del_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_del : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_del_var, FALSE, FALSE);

    role_status2_ptr = &stat_new_var;		/* so radiobuttons can tell */
    stat_new_var.name       = cpystr("Message is New (Unseen)?");
    stat_new_var.is_used    = 1;
    stat_new_var.is_user    = 1;
    apval = APVAL(&stat_new_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_new : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_new_var, FALSE, FALSE);

    role_status3_ptr = &stat_imp_var;		/* so radiobuttons can tell */
    stat_imp_var.name       = cpystr("Message is Important?");
    stat_imp_var.is_used    = 1;
    stat_imp_var.is_user    = 1;
    apval = APVAL(&stat_imp_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_imp : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_imp_var, FALSE, FALSE);

    role_status4_ptr = &stat_ans_var;		/* so radiobuttons can tell */
    stat_ans_var.name       = cpystr("Message is Answered?");
    stat_ans_var.is_used    = 1;
    stat_ans_var.is_user    = 1;
    apval = APVAL(&stat_ans_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_ans : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_ans_var, FALSE, FALSE);

    role_status5_ptr = &stat_8bit_var;		/* so radiobuttons can tell */
    stat_8bit_var.name       = cpystr("Subject contains raw 8-bit?");
    stat_8bit_var.is_used    = 1;
    stat_8bit_var.is_user    = 1;
    apval = APVAL(&stat_8bit_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_8bitsubj : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_8bit_var, FALSE, FALSE);

    role_status6_ptr = &stat_rec_var;		/* so radiobuttons can tell */
    stat_rec_var.name       = cpystr("Message is Recent?");
    stat_rec_var.is_used    = 1;
    stat_rec_var.is_user    = 1;
    apval = APVAL(&stat_rec_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_rec : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_rec_var, FALSE, FALSE);

    role_status7_ptr = &stat_bom_var;		/* so radiobuttons can tell */
    stat_bom_var.name       = cpystr("Beginning of Month?");
    stat_bom_var.is_used    = 1;
    stat_bom_var.is_user    = 1;
    apval = APVAL(&stat_bom_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_bom : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_bom_var, FALSE, FALSE);

    role_status8_ptr = &stat_boy_var;		/* so radiobuttons can tell */
    stat_boy_var.name       = cpystr("Beginning of Year?");
    stat_boy_var.is_used    = 1;
    stat_boy_var.is_user    = 1;
    apval = APVAL(&stat_boy_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_boy : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_boy_var, FALSE, FALSE);



    convert_statebits_to_vals((def && def->action) ? def->action->state_setting_bits : 0L, &dval, &aval, &ival, &nval);
    msg_state1_ptr = &filt_del_var;		/* so radiobuttons can tell */
    filt_del_var.name       = cpystr("Set Deleted Status");
    filt_del_var.is_used    = 1;
    filt_del_var.is_user    = 1;
    apval = APVAL(&filt_del_var, ew);
    *apval = (f=msg_state_types(dval)) ? cpystr(f->name) : NULL;
    set_current_val(&filt_del_var, FALSE, FALSE);

    msg_state2_ptr = &filt_new_var;		/* so radiobuttons can tell */
    filt_new_var.name       = cpystr("Set New Status");
    filt_new_var.is_used    = 1;
    filt_new_var.is_user    = 1;
    apval = APVAL(&filt_new_var, ew);
    *apval = (f=msg_state_types(nval)) ? cpystr(f->name) : NULL;
    set_current_val(&filt_new_var, FALSE, FALSE);

    msg_state3_ptr = &filt_imp_var;		/* so radiobuttons can tell */
    filt_imp_var.name       = cpystr("Set Important Status");
    filt_imp_var.is_used    = 1;
    filt_imp_var.is_user    = 1;
    apval = APVAL(&filt_imp_var, ew);
    *apval = (f=msg_state_types(ival)) ? cpystr(f->name) : NULL;
    set_current_val(&filt_imp_var, FALSE, FALSE);

    msg_state4_ptr = &filt_ans_var;		/* so radiobuttons can tell */
    filt_ans_var.name       = cpystr("Set Answered Status");
    filt_ans_var.is_used    = 1;
    filt_ans_var.is_user    = 1;
    apval = APVAL(&filt_ans_var, ew);
    *apval = (f=msg_state_types(aval)) ? cpystr(f->name) : NULL;
    set_current_val(&filt_ans_var, FALSE, FALSE);

    
    pindent += 3;

    inick_var.name       = cpystr("Initialize settings using role");
    inick_var.is_used    = 1;
    inick_var.is_user    = 1;
    apval = APVAL(&inick_var, ew);
    *apval = (def && def->action && def->action->inherit_nick &&
	      def->action->inherit_nick[0])
	       ? cpystr(def->action->inherit_nick) : NULL;

    role_fldr_ptr = &fldr_type_var;		/* so radiobuttons can tell */
    fldr_type_var.name       = cpystr("Current Folder Type");
    fldr_type_var.is_used    = 1;
    fldr_type_var.is_user    = 1;
    apval = APVAL(&fldr_type_var, ew);
    *apval = (f=pat_fldr_types((def && def->patgrp) ? def->patgrp->fldr_type : (!def && edit_filter) ? FLDR_SPECIFIC : FLDR_DEFL)) ? cpystr(f->name) : NULL;
    set_current_val(&fldr_type_var, FALSE, FALSE);

    setup_dummy_pattern_var(&folder_pat_var, "Folder List",
			   (def && def->patgrp) ? def->patgrp->folder : NULL);
    /* special default for folder_pat */
    alval = ALVAL(&folder_pat_var, ew);
    if(alval && !*alval && !def && edit_filter){
	char **ltmp;

	ltmp    = (char **) fs_get(2 * sizeof(*ltmp));
	ltmp[0] = cpystr(ps_global->inbox_name);
	ltmp[1] = NULL;
	*alval  = ltmp;
	set_current_val(&folder_pat_var, FALSE, FALSE);
    }

    role_afrom_ptr = &abook_type_var;		/* so radiobuttons can tell */
    abook_type_var.name       = cpystr("From or ReplyTo is in address book?");
    abook_type_var.is_used    = 1;
    abook_type_var.is_user    = 1;
    apval = APVAL(&abook_type_var, ew);
    *apval = (f=abookfrom_fldr_types((def && def->patgrp) ? def->patgrp->abookfrom : AFRM_EITHER)) ? cpystr(f->name) : NULL;
    set_current_val(&abook_type_var, FALSE, FALSE);

    setup_dummy_pattern_var(&abook_pat_var, "Abook List",
			   (def && def->patgrp) ? def->patgrp->abooks : NULL);

    /*
     * This is a little different from some of the other patterns. Tt is
     * actually a char ** in the struct instead of a PATTERN_S.
     */
    cat_cmd_global_ptr     = &cat_cmd_var;
    cat_cmd_var.name       = cpystr("External Categorizer Commands");
    cat_cmd_var.is_used    = 1;
    cat_cmd_var.is_user    = 1;
    cat_cmd_var.is_list    = 1;
    alval = ALVAL(&cat_cmd_var, ew);
    *alval = (def && def->patgrp && def->patgrp->category_cmd &&
	      def->patgrp->category_cmd[0])
	       ? copy_list_array(def->patgrp->category_cmd) : NULL;
    set_current_val(&cat_cmd_var, FALSE, FALSE);

    cati_global_ptr     = &cati_var;
    cati_var.name       = cpystr("Exit Status Interval");
    cati_var.is_used    = 1;
    cati_var.is_user    = 1;
    if(def && def->patgrp && def->patgrp->do_cat && def->patgrp->category_cmd &&
       def->patgrp->category_cmd[0]){
	apval = APVAL(&cati_var, ew);
	*apval = stringform_of_intvl(def->patgrp->cat);
    }

    set_current_val(&cati_var, FALSE, FALSE);

    cat_lim_global_ptr     = &cat_lim_var;
    cat_lim_var.name       = cpystr("Character Limit");
    cat_lim_var.is_used    = 1;
    cat_lim_var.is_user    = 1;
    cat_lim_var.global_val.p = cpystr("-1");
    apval = APVAL(&cat_lim_var, ew);
    if(def && def->patgrp && def->patgrp->category_cmd &&
       def->patgrp->category_cmd[0] && def->patgrp->cat_lim != -1){
	*apval = (char *) fs_get(20 * sizeof(char));
	sprintf(*apval, "%d", def->patgrp->cat_lim);
    }

    set_current_val(&cat_lim_var, FALSE, FALSE);

    from_act_var.name       = cpystr("Set From");
    from_act_var.is_used    = 1;
    from_act_var.is_user    = 1;
    if(def && def->action && def->action->from){
	char *bufp;

	bufp = (char *)fs_get((size_t)est_size(def->action->from));
	apval = APVAL(&from_act_var, ew);
	bufp[0] = '\0';
	rfc822_write_address(bufp, def->action->from);
	*apval = bufp;
    }
    else{
	apval = APVAL(&from_act_var, ew);
	*apval = NULL;
    }

    replyto_act_var.name       = cpystr("Set Reply-To");
    replyto_act_var.is_used    = 1;
    replyto_act_var.is_user    = 1;
    if(def && def->action && def->action->replyto){
	char *bufp;

	bufp = (char *)fs_get((size_t)est_size(def->action->replyto));
	apval = APVAL(&replyto_act_var, ew);
	bufp[0] = '\0';
	rfc822_write_address(bufp, def->action->replyto);
	*apval = bufp;
    }
    else{
	apval = APVAL(&replyto_act_var, ew);
	*apval = NULL;
    }

    fcc_act_var.name       = cpystr("Set Fcc");
    fcc_act_var.is_used    = 1;
    fcc_act_var.is_user    = 1;
    apval = APVAL(&fcc_act_var, ew);
    *apval = (def && def->action && def->action->fcc)
	       ? cpystr(def->action->fcc) : NULL;

    sort_act_var.name       = cpystr("Set Sort Order");
    sort_act_var.is_used    = 1;
    sort_act_var.is_user    = 1;
    apval = APVAL(&sort_act_var, ew);
    if(def && def->action && def->action->is_a_other &&
       def->action->sort_is_set){
	sprintf(tmp_20k_buf, "%s%s", sort_name(def->action->sortorder),
		(def->action->revsort) ? "/Reverse" : "");
	*apval = cpystr(tmp_20k_buf);
    }
    else
      *apval = NULL;

    iform_act_var.name       = cpystr("Set Index Format");
    iform_act_var.is_used    = 1;
    iform_act_var.is_user    = 1;
    apval = APVAL(&iform_act_var, ew);
    *apval = (def && def->action && def->action->is_a_other &&
	      def->action->index_format)
	       ? cpystr(def->action->index_format) : NULL;
    if(ps_global->VAR_INDEX_FORMAT){
	iform_act_var.global_val.p = cpystr(ps_global->VAR_INDEX_FORMAT);
	set_current_val(&iform_act_var, FALSE, FALSE);
    }

    startup_ptr            = &startup_var;
    startup_var.name       = cpystr("Set Startup Rule");
    startup_var.is_used    = 1;
    startup_var.is_user    = 1;
    apval = APVAL(&startup_var, ew);
    *apval = NULL;
    if(def && def->action && def->action->is_a_other){
	*apval = (f=startup_rules(def->action->startup_rule))
					? cpystr(f->name) : NULL;
	set_current_val(&startup_var, FALSE, FALSE);
    }
    if(!*apval){
	*apval = (f=startup_rules(IS_NOTSET)) ? cpystr(f->name) : NULL;
	set_current_val(&startup_var, FALSE, FALSE);
    }

    litsig_act_var.name       = cpystr("Set LiteralSig");
    litsig_act_var.is_used    = 1;
    litsig_act_var.is_user    = 1;
    apval = APVAL(&litsig_act_var, ew);
    *apval = (def && def->action && def->action->litsig)
	       ? cpystr(def->action->litsig) : NULL;

    sig_act_var.name       = cpystr("Set Signature");
    sig_act_var.is_used    = 1;
    sig_act_var.is_user    = 1;
    apval = APVAL(&sig_act_var, ew);
    *apval = (def && def->action && def->action->sig)
	       ? cpystr(def->action->sig) : NULL;

    templ_act_var.name       = cpystr("Set Template");
    templ_act_var.is_used    = 1;
    templ_act_var.is_user    = 1;
    apval = APVAL(&templ_act_var, ew);
    *apval = (def && def->action && def->action->template)
		 ? cpystr(def->action->template) : NULL;

    cstm_act_var.name       = cpystr("Set Other Hdrs");
    cstm_act_var.is_used    = 1;
    cstm_act_var.is_user    = 1;
    cstm_act_var.is_list    = 1;
    alval = ALVAL(&cstm_act_var, ew);
    *alval = (def && def->action && def->action->cstm)
		 ? copy_list_array(def->action->cstm) : NULL;

    smtp_act_var.name       = cpystr("Use SMTP Server");
    smtp_act_var.is_used    = 1;
    smtp_act_var.is_user    = 1;
    smtp_act_var.is_list    = 1;
    alval = ALVAL(&smtp_act_var, ew);
    *alval = (def && def->action && def->action->smtp)
		 ? copy_list_array(def->action->smtp) : NULL;

    nntp_act_var.name       = cpystr("Use NNTP Server");
    nntp_act_var.is_used    = 1;
    nntp_act_var.is_user    = 1;
    nntp_act_var.is_list    = 1;
    alval = ALVAL(&nntp_act_var, ew);
    *alval = (def && def->action && def->action->nntp)
		 ? copy_list_array(def->action->nntp) : NULL;

    score_act_global_ptr     = &score_act_var;
    score_act_var.name       = cpystr("Score Value");
    score_act_var.is_used    = 1;
    score_act_var.is_user    = 1;
    if(def && def->action && def->action->scoreval >= SCORE_MIN &&
       def->action->scoreval <= SCORE_MAX)
      scoreval = def->action->scoreval;

    score_act_var.global_val.p = cpystr("0");
    if(scoreval != 0){
	apval = APVAL(&score_act_var, ew);
	*apval = (char *)fs_get(20 * sizeof(char));
	sprintf(*apval, "%d", scoreval);
    }

    set_current_val(&score_act_var, FALSE, FALSE);

    role_repl_ptr = &repl_type_var;		/* so radiobuttons can tell */
    repl_type_var.name       = cpystr("Reply Use");
    repl_type_var.is_used    = 1;
    repl_type_var.is_user    = 1;
    apval = APVAL(&repl_type_var, ew);
    *apval = (f=role_repl_types((def && def->action) ? def->action->repl_type : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&repl_type_var, FALSE, FALSE);

    role_forw_ptr = &forw_type_var;		/* so radiobuttons can tell */
    forw_type_var.name       = cpystr("Forward Use");
    forw_type_var.is_used    = 1;
    forw_type_var.is_user    = 1;
    apval = APVAL(&forw_type_var, ew);
    *apval = (f=role_forw_types((def && def->action) ? def->action->forw_type : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&forw_type_var, FALSE, FALSE);

    comp_type_var.name       = cpystr("Compose Use");
    comp_type_var.is_used    = 1;
    comp_type_var.is_user    = 1;
    apval = APVAL(&comp_type_var, ew);
    *apval = (f=role_comp_types((def && def->action) ? def->action->comp_type : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&comp_type_var, FALSE, FALSE);

    rolecolor_vars[0].is_used    = 1;
    rolecolor_vars[0].is_user    = 1;
    apval = APVAL(&rolecolor_vars[0], ew);
    *apval = (def && def->action && def->action->incol &&
	      def->action->incol->fg[0])
	         ? cpystr(def->action->incol->fg) : NULL;
    rolecolor_vars[1].is_used    = 1;
    rolecolor_vars[1].is_user    = 1;
    rolecolor_vars[0].name = cpystr("ic-foreground-color");
    rolecolor_vars[1].name = cpystr(rolecolor_vars[0].name);
    strncpy(rolecolor_vars[1].name + 3, "back", 4);
    apval = APVAL(&rolecolor_vars[1], ew);
    *apval = (def && def->action && def->action->incol &&
	      def->action->incol->bg[0])
	         ? cpystr(def->action->incol->bg) : NULL;
    set_current_val(&rolecolor_vars[0], FALSE, FALSE);
    set_current_val(&rolecolor_vars[1], FALSE, FALSE);
    old_fg = PVAL(&rolecolor_vars[0], ew) ? cpystr(PVAL(&rolecolor_vars[0], ew))
					  : NULL;
    old_bg = PVAL(&rolecolor_vars[1], ew) ? cpystr(PVAL(&rolecolor_vars[1], ew))
					  : NULL;


    /* save the old opt_screen before calling scroll screen again */
    saved_screen = opt_screen;

    /* Nickname */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR NICKNAME";
    ctmp->var       = &nick_var;
    ctmp->valoffset = pindent;
    ctmp->keymenu   = &config_role_keymenu;
    ctmp->help      = edit_role ? h_config_role_nick :
		       edit_incol ? h_config_incol_nick :
			edit_score ? h_config_score_nick :
			 edit_other ? h_config_other_nick
			            : h_config_filt_nick;
    ctmp->tool      = t_tool;
    sprintf(tmp, "%-*.100s =", pindent-3, nick_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->varmem    = -1;

    first_line = ctmp;
    if(rflags & ROLE_CHANGES)
      first_line->flags |= CF_CHANGES;

    /* Comment */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR COMMENT";
    ctmp->var       = &comment_var;
    ctmp->valoffset = pindent;
    ctmp->keymenu   = &config_role_keymenu;
    ctmp->help      = h_config_role_comment;
    ctmp->tool      = role_litsig_text_tool;
    sprintf(tmp, "%-*.100s =", pindent-3, comment_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->varmem    = -1;

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(repeat_char(ps->ttyo->screen_cols, '='));
    if(ps->ttyo->screen_cols >= strlen(fstr) + 2)
      strncpy(ctmp->value + (ps->ttyo->screen_cols - strlen(fstr))/2,
	      fstr, strlen(fstr));

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* Folder Type */
    new_confline(&ctmp);
    ctmp->var       = &fldr_type_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    sprintf(tmp, "%-*.100s =", indent-3, fldr_type_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Choose One");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = radio_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("---  --------------------");

    /* find longest value's name */
    for(lv = 0, i = 0; f = pat_fldr_types(i); i++)
      if(lv < (j = strlen(f->name)))
	lv = j;
    
    lv = min(lv, 100);

    fval = -1;
    for(i = 0; f = pat_fldr_types(i); i++){
	new_confline(&ctmp);
	ctmp->help_title= "HELP FOR CURRENT FOLDER TYPE";
	ctmp->var       = &fldr_type_var;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = edit_role ? h_config_role_fldr_type :
			   edit_incol ? h_config_incol_fldr_type :
			    edit_score ? h_config_score_fldr_type :
			     edit_other ? h_config_other_fldr_type
				        : h_config_filt_fldr_type;
	ctmp->varmem    = i;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;

	if((PVAL(&fldr_type_var, ew) &&
	    !strucmp(PVAL(&fldr_type_var, ew), f->name))
	   || (!PVAL(&fldr_type_var, ew) && f->value == FLDR_DEFL))
	  fval = f->value;

	sprintf(tmp, "(%c)  %-*.*s",
		(fval == f->value) ? R_SELD : ' ',
		lv, lv, f->name);
	ctmp->value     = cpystr(tmp);
    }

    /* Folder */
    setup_role_pat_alt(ps, &ctmp, &folder_pat_var,
		       edit_role ? h_config_role_fldr_type :
			edit_incol ? h_config_incol_fldr_type :
			 edit_score ? h_config_score_fldr_type :
			  edit_other ? h_config_other_fldr_type
				     : h_config_filt_fldr_type,
		       "HELP FOR FOLDER LIST",
		       &config_role_patfolder_keymenu, t_tool, 12+5,
		       !(fval == FLDR_SPECIFIC));

  if(!per_folder_only){		/* sorry about that indent */
    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(repeat_char(ps->ttyo->screen_cols, '='));
    sprintf(mstr, " %.10s MESSAGE CONDITIONS BEGIN HERE ",
	    edit_role ? "CURRENT" :
	     edit_score ? "SCORED" :
	      edit_incol ? "COLORED" :
	       edit_filter ? "FILTERED" : "CURRENT");
    if(ps->ttyo->screen_cols >= strlen(mstr) + 2)
      strncpy(ctmp->value + (ps->ttyo->screen_cols - strlen(mstr))/2,
	      mstr, strlen(mstr));

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    setup_role_pat(ps, &ctmp, &to_pat_var,
		   edit_role ? h_config_role_topat :
		    edit_incol ? h_config_incol_topat :
		     edit_score ? h_config_score_topat :
		      edit_other ? h_config_other_topat
			         : h_config_filt_topat,
		   "HELP FOR TO PATTERN",
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &from_pat_var, h_config_role_frompat,
		   "HELP FOR FROM PATTERN",
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &sender_pat_var, h_config_role_senderpat,
		   "HELP FOR SENDER PATTERN",
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &cc_pat_var, h_config_role_ccpat,
		   "HELP FOR CC PATTERN",
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &news_pat_var, h_config_role_newspat,
		   "HELP FOR NEWS PATTERN",
		   &config_role_keymenu_not, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &subj_pat_var, h_config_role_subjpat,
		   "HELP FOR SUBJECT PATTERN",
		   &config_role_keymenu_not, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &recip_pat_var, h_config_role_recippat,
		   "HELP FOR RECIPIENT PATTERN",
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &partic_pat_var, h_config_role_particpat,
		   "HELP FOR PARTICIPANT PATTERN",
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    /* Arbitrary Patterns */
    ea = NULL;
    for(j = 0, a = (def && def->patgrp) ? def->patgrp->arbhdr : NULL;
	a;
	j++, a = a->next){
	char *fn = (a->field) ? a->field : "";

	if(ea){
	    ea->next = (EARB_S *)fs_get(sizeof(*ea));
	    ea = ea->next;
	}
	else{
	    earb = (EARB_S *)fs_get(sizeof(*ea));
	    ea = earb;
	}

	memset((void *)ea, 0, sizeof(*ea));
	ea->v = (struct variable *)fs_get(sizeof(struct variable));
	memset((void *)ea->v, 0, sizeof(struct variable));
	ea->a = (ARBHDR_S *)fs_get(sizeof(ARBHDR_S));
	memset((void *)ea->a, 0, sizeof(ARBHDR_S));

	ea->a->field = cpystr(fn);

	p = (char *) fs_get(strlen(fn) + strlen(" pattern") + 1);
	sprintf(p, "%s pattern", fn);
	setup_dummy_pattern_var(ea->v, p, a->p);
	fs_give((void **) &p);
	setup_role_pat(ps, &ctmp, ea->v, h_config_role_arbpat,
		       ARB_HELP, &config_role_xtrahdr_keymenu,
		       t_tool, &earb, pindent);
    }

    new_confline(&ctmp);
    ctmp->help_title = "HELP FOR EXTRA HEADERS";
    ctmp->value = cpystr(ADDXHDRS);
    ctmp->keymenu = &config_role_keymenu_extra;
    ctmp->help      = h_config_role_arbpat;
    ctmp->tool      = addhdr_tool;
    ctmp->d.earb    = &earb;
    ctmp->varmem    = -1;

    setup_role_pat(ps, &ctmp, &alltext_pat_var, h_config_role_alltextpat,
		   "HELP FOR ALL TEXT PATTERN",
		   &config_role_keymenu_not, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &bodytext_pat_var, h_config_role_bodytextpat,
		   "HELP FOR BODY TEXT PATTERN",
		   &config_role_keymenu_not, t_tool, &earb, pindent);

    /* Age Interval */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR AGE INTERVAL";
    ctmp->var       = &age_pat_var;
    ctmp->valoffset = pindent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_role_age;
    ctmp->tool      = t_tool;
    sprintf(tmp, "%-*.100s =", pindent-3, age_pat_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Size Interval */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR SIZE INTERVAL";
    ctmp->var       = &size_pat_var;
    ctmp->valoffset = pindent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_role_size;
    ctmp->tool      = t_tool;
    sprintf(tmp, "%-*.100s =", pindent-3, size_pat_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    if(!edit_score){
	/* Score Interval */
	new_confline(&ctmp);
	ctmp->help_title= "HELP FOR SCORE INTERVAL";
	ctmp->var       = &scorei_pat_var;
	ctmp->valoffset = pindent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_role_scorei;
	ctmp->tool      = t_tool;
	sprintf(tmp, "%-*.100s =", pindent-3, scorei_pat_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;
	ctmp->value     = pretty_value(ps, ctmp);
    }

    /* Keyword Pattern */
    setup_role_pat(ps, &ctmp, &keyword_pat_var, h_config_role_keywordpat,
		   "HELP FOR KEYWORD PATTERN",
		   &config_role_keyword_keymenu_not, role_text_tool_kword,
		   NULL, pindent);

    /* Charset Pattern */
    setup_role_pat(ps, &ctmp, &charset_pat_var, h_config_role_charsetpat,
		   "HELP FOR CHARACTER SET PATTERN",
		   &config_role_charset_keymenu_not, role_text_tool_charset,
		   NULL, pindent);

    /* Important Status */
    SETUP_PAT_STATUS(ctmp, stat_imp_var, def->patgrp->stat_imp,
		     "HELP FOR IMPORTANT STATUS", h_config_role_stat_imp);
    /* New Status */
    SETUP_PAT_STATUS(ctmp, stat_new_var, def->patgrp->stat_new,
		     "HELP FOR NEW STATUS", h_config_role_stat_new);
    /* Recent Status */
    SETUP_PAT_STATUS(ctmp, stat_rec_var, def->patgrp->stat_rec,
		     "HELP FOR RECENT STATUS", h_config_role_stat_recent);
    /* Deleted Status */
    SETUP_PAT_STATUS(ctmp, stat_del_var, def->patgrp->stat_del,
		     "HELP FOR DELETED STATUS", h_config_role_stat_del);
    /* Answered Status */
    SETUP_PAT_STATUS(ctmp, stat_ans_var, def->patgrp->stat_ans,
		     "HELP FOR ANSWERED STATUS", h_config_role_stat_ans);
    /* 8-bit Subject */
    SETUP_PAT_STATUS(ctmp, stat_8bit_var, def->patgrp->stat_8bitsubj,
		     "HELP FOR 8-BIT SUBJECT", h_config_role_stat_8bitsubj);
    /* Beginning of month */
    SETUP_PAT_STATUS(ctmp, stat_bom_var, def->patgrp->stat_bom,
		     "HELP FOR BEGINNING OF MONTH", h_config_role_bom);
    /* Beginning of year */
    SETUP_PAT_STATUS(ctmp, stat_boy_var, def->patgrp->stat_boy,
		     "HELP FOR BEGINNING OF YEAR", h_config_role_boy);

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* From in Addrbook */
    new_confline(&ctmp);
    ctmp->var       = &abook_type_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    sprintf(tmp, "%-*.100s =", indent-3, abook_type_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Choose One");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = 12;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = radio_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("---  --------------------");

    /* find longest value's name */
    for(lv = 0, i = 0; f = abookfrom_fldr_types(i); i++)
      if(lv < (j = strlen(f->name)))
	lv = j;
    
    lv = min(lv, 100);

    fval = -1;
    for(i = 0; f = abookfrom_fldr_types(i); i++){
	new_confline(&ctmp);
	ctmp->help_title= "HELP FOR FROM IN ADDRESS BOOK";
	ctmp->var       = &abook_type_var;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = h_config_role_abookfrom;
	ctmp->varmem    = i;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;

	if((PVAL(&abook_type_var, ew) &&
	    !strucmp(PVAL(&abook_type_var, ew), f->name))
	   || (!PVAL(&abook_type_var, ew) && f->value == AFRM_DEFL))
	  fval = f->value;

	sprintf(tmp, "(%c)  %-*.*s",
		(fval == f->value) ? R_SELD : ' ',
		lv, lv, f->name);
	ctmp->value     = cpystr(tmp);
    }

    /* Specific list of abooks */
    setup_role_pat_alt(ps, &ctmp, &abook_pat_var, h_config_role_abookfrom,
		       "HELP FOR ABOOK LIST",
		       &config_role_afrom_keymenu, role_text_tool_afrom, 12+5,
		       !(fval == AFRM_SPEC_YES || fval == AFRM_SPEC_NO));

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    i = 4+strlen(cati_var.name)+3;

    /* External Command Categorizer */
    new_confline(&ctmp);
    ctmp->var       = &cat_cmd_var;
    ctmp->valoffset = i;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    sprintf(tmp, "%-*.100s =", pindent-3, cat_cmd_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    /* Commands */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR CATEGORIZER COMMAND";
    ctmp->var       = &cat_cmd_var;
    ctmp->varoffset = 4;
    ctmp->valoffset = i;
    ctmp->keymenu   = &config_text_wshuf_keymenu;
    ctmp->help      = h_config_role_cat_cmd;
    ctmp->tool      = t_tool;
    sprintf(tmp, "%-*.100s =", i-4-3, "Command");
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags     = CF_STARTITEM;

    if((lval = LVAL(&cat_cmd_var, ew)) != NULL && lval[0]){
	for(j = 0; lval[j]; j++){
	    if(j)
	      (void) new_confline(&ctmp);
	    
	    ctmp->var       = &cat_cmd_var;
	    ctmp->varmem    = j;
	    ctmp->valoffset = i;
	    ctmp->keymenu   = &config_text_wshuf_keymenu;
	    ctmp->help      = h_config_role_cat_cmd;
	    ctmp->tool      = t_tool;
	    ctmp->varnamep  = ctmp;
	    ctmp->value     = pretty_value(ps, ctmp);
	}
    }
    else
      ctmp->value = pretty_value(ps, ctmp);

    /* Exit status interval */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR CATEGORIZER EXIT STATUS";
    ctmp->var       = &cati_var;
    ctmp->varoffset = 4;
    ctmp->valoffset = i;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_role_cat_status;
    ctmp->tool      = t_tool;
    sprintf(tmp, "%-*.100s =", i-4-3, cati_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Character Limit */
    new_confline(&ctmp);
    ctmp->help_title= "HELP FOR CHARACTER LIMIT";
    ctmp->var       = &cat_lim_var;
    ctmp->varoffset = 4;
    ctmp->valoffset = i;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_role_cat_limit;
    ctmp->tool      = t_tool;
    sprintf(tmp, "%-*.100s =", i-4-3, cat_lim_var.name);
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->flags    |= CF_NUMBER;
  }

    /* Actions */

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(repeat_char(ps->ttyo->screen_cols, '='));
    if(ps->ttyo->screen_cols >= strlen(astr) + 2)
      strncpy(ctmp->value + (ps->ttyo->screen_cols - strlen(astr))/2,
	      astr, strlen(astr));

    if(edit_role){
	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Inherit Nickname */
	new_confline(&ctmp);
	inick_confs[INICK_INICK_CONF] = ctmp;
	ctmp->help_title= "HELP FOR INITIAL SET NICKNAME";
	ctmp->var       = &inick_var;
	ctmp->valoffset = 33;
	ctmp->keymenu   = &config_role_inick_keymenu;
	ctmp->help      = h_config_role_inick;
	ctmp->tool      = role_text_tool_inick;
	sprintf(tmp, "%-30.100s :", inick_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* From Action */
	new_confline(&ctmp);
	inick_confs[INICK_FROM_CONF] = ctmp;
	ctmp->help_title= "HELP FOR SET FROM ACTION";
	ctmp->var       = &from_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_role_addr_act_keymenu;
	ctmp->help      = h_config_role_setfrom;
	ctmp->tool      = role_text_tool;
	sprintf(tmp, "%-*.100s =", indent-3, from_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Reply-To Action */
	new_confline(&ctmp);
	inick_confs[INICK_REPLYTO_CONF] = ctmp;
	ctmp->help_title= "HELP FOR SET REPLY-TO ACTION";
	ctmp->var       = &replyto_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_role_addr_act_keymenu;
	ctmp->help      = h_config_role_setreplyto;
	ctmp->tool      = role_text_tool;
	sprintf(tmp, "%-*.100s =", indent-3, replyto_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Fcc Action */
	new_confline(&ctmp);
	inick_confs[INICK_FCC_CONF] = ctmp;
	ctmp->help_title= "HELP FOR SET FCC ACTION";
	ctmp->var       = &fcc_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_role_actionfolder_keymenu;
	ctmp->help      = h_config_role_setfcc;
	ctmp->tool      = role_text_tool;
	sprintf(tmp, "%-*.100s =", indent-3, fcc_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* LitSig Action */
	new_confline(&ctmp);
	inick_confs[INICK_LITSIG_CONF] = ctmp;
	ctmp->help_title= "HELP FOR SET LITERAL SIGNATURE ACTION";
	ctmp->var       = &litsig_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_role_setlitsig;
	ctmp->tool      = role_litsig_text_tool;
	sprintf(tmp, "%-*.100s =", indent-3, litsig_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Sig Action */
	new_confline(&ctmp);
	inick_confs[INICK_SIG_CONF] = ctmp;
	ctmp->help_title= "HELP FOR SET SIGNATURE ACTION";
	ctmp->var       = &sig_act_var;
	ctmp->valoffset = indent;
	if(F_ON(F_DISABLE_ROLES_SIGEDIT, ps_global))
	  ctmp->keymenu   = &config_role_file_res_keymenu;
	else
	  ctmp->keymenu   = &config_role_file_keymenu;

	ctmp->help      = h_config_role_setsig;
	ctmp->tool      = role_text_tool;
	sprintf(tmp, "%-*.100s =", indent-3, sig_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Template Action */
	new_confline(&ctmp);
	inick_confs[INICK_TEMPL_CONF] = ctmp;
	ctmp->help_title= "HELP FOR SET TEMPLATE ACTION";
	ctmp->var       = &templ_act_var;
	ctmp->valoffset = indent;
	if(F_ON(F_DISABLE_ROLES_TEMPLEDIT, ps_global))
	  ctmp->keymenu   = &config_role_file_res_keymenu;
	else
	  ctmp->keymenu   = &config_role_file_keymenu;

	ctmp->help      = h_config_role_settempl;
	ctmp->tool      = role_text_tool;
	sprintf(tmp, "%-*.100s =", indent-3, templ_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Other Headers Action */
	new_confline(&ctmp);
	inick_confs[INICK_CSTM_CONF] = ctmp;
	ctmp->help_title= "HELP FOR SET OTHER HEADERS ACTION";
	ctmp->var       = &cstm_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_text_wshuf_keymenu;
	ctmp->help      = h_config_role_setotherhdr;
	ctmp->tool      = role_cstm_text_tool;
	sprintf(tmp, "%-*.100s =", indent-3, cstm_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags     = CF_STARTITEM;

	if((lval = LVAL(&cstm_act_var, ew)) != NULL){
	    for(i = 0; lval[i]; i++){
		if(i)
		  (void)new_confline(&ctmp);

		ctmp->var       = &cstm_act_var;
		ctmp->varmem    = i;
		ctmp->valoffset = indent;
		ctmp->keymenu   = &config_text_wshuf_keymenu;
		ctmp->help      = h_config_role_setotherhdr;
		ctmp->tool      = role_cstm_text_tool;
		ctmp->varnamep  = ctmpb;
	    }
	}
	else
	  ctmp->varmem = 0;

	/* SMTP Server Action */
	new_confline(&ctmp);
	inick_confs[INICK_SMTP_CONF] = ctmp;
	ctmp->help_title= "HELP FOR SMTP SERVER ACTION";
	ctmp->var       = &smtp_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_text_wshuf_keymenu;
	ctmp->help      = h_config_role_usesmtp;
	ctmp->tool      = t_tool;
	sprintf(tmp, "%-*.100s =", indent-3, smtp_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags     = CF_STARTITEM;

	if((lval = LVAL(&smtp_act_var, ew)) != NULL){
	    for(i = 0; lval[i]; i++){
		if(i)
		  (void)new_confline(&ctmp);

		ctmp->var       = &smtp_act_var;
		ctmp->varmem    = i;
		ctmp->valoffset = indent;
		ctmp->keymenu   = &config_text_wshuf_keymenu;
		ctmp->help      = h_config_role_usesmtp;
		ctmp->tool      = t_tool;
		ctmp->varnamep  = ctmpb;
	    }
	}
	else
	  ctmp->varmem = 0;

	/* NNTP Server Action */
	new_confline(&ctmp);
	inick_confs[INICK_NNTP_CONF] = ctmp;
	ctmp->help_title= "HELP FOR NNTP SERVER ACTION";
	ctmp->var       = &nntp_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_text_wshuf_keymenu;
	ctmp->help      = h_config_role_usenntp;
	ctmp->tool      = t_tool;
	sprintf(tmp, "%-*.100s =", indent-3, nntp_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags     = CF_STARTITEM;

	if((lval = LVAL(&nntp_act_var, ew)) != NULL){
	    for(i = 0; lval[i]; i++){
		if(i)
		  (void)new_confline(&ctmp);

		ctmp->var       = &nntp_act_var;
		ctmp->varmem    = i;
		ctmp->valoffset = indent;
		ctmp->keymenu   = &config_text_wshuf_keymenu;
		ctmp->help      = h_config_role_usenntp;
		ctmp->tool      = t_tool;
		ctmp->varnamep  = ctmpb;
	    }
	}
	else
	  ctmp->varmem = 0;

	calculate_inick_stuff(ps);
    }
    else
      inick_confs[INICK_INICK_CONF] = NULL;

    if(edit_score){
	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Score Value -- This doesn't inherit from inick */
	new_confline(&ctmp);
	ctmp->help_title= "HELP FOR SCORE VALUE ACTION";
	ctmp->var       = &score_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_role_scoreval;
	ctmp->tool      = text_tool;
	ctmp->flags    |= CF_NUMBER;
	sprintf(tmp, "%-*.100s =", indent-3, score_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;
	ctmp->value     = pretty_value(ps, ctmp);
    }

    if(edit_filter){
	/*
	 * Filtering got added in stages, so instead of simply having a
	 * variable in action which is set to one of the three possible
	 * values (FILTER_KILL, FILTER_STATE, FILTER_FOLDER) we infer
	 * the value from other variables. (Perhaps it would still make
	 * sense to change this.)
	 * Action->kill is set iff the user checks Delete.
	 * If the user checks the box that says Just Set State, then kill
	 * is not set and action->folder is not set (and vice versa).
	 * And finally, FILTER_FOLDER is set if !kill and action->folder is set.
	 * (And it is set here as the default if there is no default
	 * action and the user is required to fill in the Folder.)
	 */
	if(def && def->action && def->action->kill)
	  fval = FILTER_KILL;
	else if(def && def->action && !def->action->kill &&
		!def->action->folder)
	  fval = FILTER_STATE;
	else
	  fval = FILTER_FOLDER;

	role_filt_ptr = &filter_type_var;	/* so radiobuttons can tell */
	filter_type_var.name       = cpystr("Filter Action");
	filter_type_var.is_used    = 1;
	filter_type_var.is_user    = 1;
	apval = APVAL(&filter_type_var, ew);
	*apval = (f=filter_types(fval)) ? cpystr(f->name) : NULL;
	set_current_val(&filter_type_var, FALSE, FALSE);

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Filter Type */
	new_confline(&ctmp);
	ctmp->var       = &filter_type_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	sprintf(tmp, "%-*.100s =", indent-3, filter_type_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Choose One");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("---  --------------------");

	/* find longest value's name */
	for(lv = 0, i = 0; f = filter_types(i); i++)
	  if(lv < (j = strlen(f->name)))
	    lv = j;
	
	lv = min(lv, 100);
	
	for(i = 0; f = filter_types(i); i++){
	    new_confline(&ctmp);
	    ctmp->help_title= "HELP FOR FILTER ACTION";
	    ctmp->var       = &filter_type_var;
	    ctmp->valoffset = 12;
	    ctmp->keymenu   = &config_radiobutton_keymenu;
	    ctmp->help      = h_config_filt_rule_type;
	    ctmp->varmem    = i;
	    ctmp->tool      = radio_tool;
	    ctmp->varnamep  = ctmpb;
	    sprintf(tmp, "(%c)  %-*.*s", (f->value == fval) ? R_SELD : ' ',
		    lv, lv, f->name);
	    ctmp->value     = cpystr(tmp);
	}

	/* Specific list of folders to copy to */
	setup_dummy_pattern_var(&folder_act_var, "Folder List",
			        (def && def->action)
				   ? def->action->folder : NULL);

	setup_role_pat_alt(ps, &ctmp, &folder_act_var, h_config_filter_folder,
			   "HELP FOR FILTER FOLDER NAME",
			   &config_role_actionfolder_keymenu, t_tool, 12+5,
			   !(fval == FILTER_FOLDER));

	SETUP_MSG_STATE(ctmp, filt_imp_var, ival,
		      "HELP FOR SET IMPORTANT STATUS", h_config_filt_stat_imp);
	SETUP_MSG_STATE(ctmp, filt_new_var, nval,
			"HELP FOR SET NEW STATUS", h_config_filt_stat_new);
	SETUP_MSG_STATE(ctmp, filt_del_var, dval,
			"HELP FOR SET DELETED STATUS", h_config_filt_stat_del);
	SETUP_MSG_STATE(ctmp, filt_ans_var, aval,
			"HELP FOR SET ANSWERED STATUS", h_config_filt_stat_ans);

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Keywords to be Set */
	setup_dummy_pattern_var(&keyword_set_var, "Set These Keywords",
			        (def && def->action)
				  ? def->action->keyword_set : NULL);
	setup_role_pat(ps, &ctmp, &keyword_set_var, h_config_filter_kw_set,
		       "HELP FOR KEYWORDS TO BE SET",
		       &config_role_keyword_keymenu, role_text_tool_kword,
		       NULL, 23);

	/* Keywords to be Cleared */
	setup_dummy_pattern_var(&keyword_clr_var, "Clear These Keywords",
			        (def && def->action)
				  ? def->action->keyword_clr : NULL);
	setup_role_pat(ps, &ctmp, &keyword_clr_var, h_config_filter_kw_clr,
		       "HELP FOR KEYWORDS TO BE CLEARED",
		       &config_role_keyword_keymenu, role_text_tool_kword,
		       NULL, 23);
    }

    if(edit_other){
	char     *pval;

	indent = 19;

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags		|= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp)->var  = NULL;
	sprintf(tmp, "%-*.100s =", indent-3, sort_act_var.name);
	ctmp->varname		  = cpystr(tmp);
	ctmp->varnamep		  = ctmpb = ctmp;
	ctmp->keymenu		  = &config_radiobutton_keymenu;
	ctmp->help		  = NO_HELP;
	ctmp->tool		  = role_sort_tool;
	ctmp->valoffset		  = 12;
	ctmp->flags		 |= CF_NOSELECT;

	new_confline(&ctmp)->var  = NULL;
	ctmp->varnamep		  = ctmpb;
	ctmp->keymenu		  = &config_radiobutton_keymenu;
	ctmp->help		  = NO_HELP;
	ctmp->tool		  = role_sort_tool;
	ctmp->valoffset		  = 12;
	ctmp->flags		 |= CF_NOSELECT;
	ctmp->value = cpystr("Set    Sort Options");

	new_confline(&ctmp)->var = NULL;
	ctmp->varnamep		  = ctmpb;
	ctmp->keymenu		  = &config_radiobutton_keymenu;
	ctmp->help		  = NO_HELP;
	ctmp->tool		  = role_sort_tool;
	ctmp->valoffset	    	  = 12;
	ctmp->flags              |= CF_NOSELECT;
	ctmp->value = cpystr("---  ----------------------");

	/* find longest value's name */
	for(lv = 0, i = 0; ps->sort_types[i] != EndofList; i++)
	  if(lv < (j = strlen(sort_name(ps->sort_types[i]))))
	    lv = j;
	
	pval = PVAL(&sort_act_var, ew);
	if(pval)
	  decode_sort(pval, &def_sort, &def_sort_rev);

	/* allow user to set their default sort order */
	new_confline(&ctmp)->var = &sort_act_var;
	ctmp->varnamep	      = ctmpb;
	ctmp->keymenu	      = &config_radiobutton_keymenu;
	ctmp->help	      = h_config_perfolder_sort;
	ctmp->tool	      = role_sort_tool;
	ctmp->valoffset	      = 12;
	ctmp->varmem	      = -1;
	ctmp->value	      = generalized_sort_pretty_value(ps, ctmp, 0);

	for(j = 0; j < 2; j++){
	    for(i = 0; ps->sort_types[i] != EndofList; i++){
		new_confline(&ctmp)->var = &sort_act_var;
		ctmp->varnamep	      = ctmpb;
		ctmp->keymenu	      = &config_radiobutton_keymenu;
		ctmp->help	      = h_config_perfolder_sort;
		ctmp->tool	      = role_sort_tool;
		ctmp->valoffset	      = 12;
		ctmp->varmem	      = i + (j * EndofList);
		ctmp->value	      = generalized_sort_pretty_value(ps, ctmp,
								      0);
	    }
	}


	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags		|= CF_NOSELECT | CF_B_LINE;

	/* Index Format Action */
	new_confline(&ctmp);
	ctmp->help_title= "HELP FOR SET INDEX FORMAT ACTION";
	ctmp->var       = &iform_act_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_set_index_format;
	ctmp->tool      = text_tool;
	sprintf(tmp, "%-*.100s =", indent-3, iform_act_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;
	ctmp->value     = pretty_value(ps, ctmp);

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp);
	ctmp->flags    |= CF_STARTITEM;
	sprintf(tmp, "%-*.100s =", indent-3, startup_var.name);
	ctmp->varname		  = cpystr(tmp);
	standard_radio_setup(ps, &ctmp, &startup_var, NULL);
    }

    if(edit_incol && pico_usingcolor()){
	char *pval0, *pval1;
	int def;

	indent = 12;

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp);
	ctmp->var		 = &rolecolor_vars[0];	/* foreground */
	ctmp->varname		 = cpystr("Index Line Color    =");
	ctmp->varnamep		 = ctmpb = ctmp;
	ctmp->flags		|= (CF_STARTITEM | CF_NOSELECT);
	ctmp->keymenu		 = &role_color_setting_keymenu;

	pval0 = PVAL(&rolecolor_vars[0], ew);
	pval1 = PVAL(&rolecolor_vars[1], ew);
	if(pval0 && pval1)
	  def = !(pval0[0] && pval1[1]);
	else
	  def = 1;

	add_color_setting_disp(ps, &ctmp, &rolecolor_vars[0], ctmpb,
			       &role_color_setting_keymenu,
			       &config_checkbox_keymenu,
			       h_config_incol,
			       indent, 0,
			       def ? ps->VAR_NORM_FORE_COLOR
				   : PVAL(&rolecolor_vars[0], ew),
			       def ? ps->VAR_NORM_BACK_COLOR
				   : PVAL(&rolecolor_vars[1], ew),
			       def);
    }

    if(need_options){
	/* Options */

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr(repeat_char(ps->ttyo->screen_cols, '='));
	if(ps->ttyo->screen_cols >= strlen(ostr) + 2)
	  strncpy(ctmp->value + (ps->ttyo->screen_cols - strlen(ostr))/2,
		  ostr, strlen(ostr));

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	opt_var.name      = cpystr("Features");
	opt_var.is_used   = 1;
	opt_var.is_user   = 1;
	opt_var.is_list   = 1;
	clrbitmap(feat_option_list);
	if(def && def->patgrp && def->patgrp->age_uses_sentdate)
	  setbitn(FEAT_SENTDATE, feat_option_list);

	if(edit_filter){
	    if(def && def->action && def->action->move_only_if_not_deleted)
	      setbitn(FEAT_IFNOTDEL, feat_option_list);
	    if(def && def->action && def->action->non_terminating)
	      setbitn(FEAT_NONTERM, feat_option_list);
	}

	/* Options */
	new_confline(&ctmp);
	ctmp->var       = &opt_var;
	ctmp->valoffset = 23;
	ctmp->keymenu   = &config_checkbox_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	sprintf(tmp, "%-20.100s =", opt_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_checkbox_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = feat_checkbox_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Feature Name");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_checkbox_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = feat_checkbox_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("---  ----------------------");

	/*  find longest value's name */
	for(lv = 0, i = 0; f = feat_feature_list(i); i++){
	    if(!edit_filter && (i == FEAT_IFNOTDEL || i == FEAT_NONTERM))
	      continue;

	    if(lv < (j = strlen(f->name)))
	      lv = j;
	}
	
	lv = min(lv, 100);

	for(i = 0; f = feat_feature_list(i); i++){
	    if(!edit_filter && (i == FEAT_IFNOTDEL || i == FEAT_NONTERM))
	      continue;

	    new_confline(&ctmp);
	    ctmp->var       = &opt_var;
	    ctmp->help_title= "HELP FOR FILTER FEATURES";
	    ctmp->varnamep  = ctmpb;
	    ctmp->keymenu   = &config_checkbox_keymenu;
	    switch(i){
	      case FEAT_SENTDATE:
		ctmp->help      = h_config_filt_opts_sentdate;
		break;
	      case FEAT_IFNOTDEL:
		ctmp->help      = h_config_filt_opts_notdel;
		break;
	      case FEAT_NONTERM:
		ctmp->help      = h_config_filt_opts_nonterm;
		break;
	    }

	    ctmp->tool      = feat_checkbox_tool;
	    ctmp->valoffset = 12;
	    ctmp->varmem    = i;
	    sprintf(tmp, "[%c]  %-*.*s", 
		    bitnset(f->value, feat_option_list) ? 'X' : ' ',
		    lv, lv, f->name);
	    ctmp->value     = cpystr(tmp);
	}
    }

    if(need_uses){
	/* Uses */

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr(repeat_char(ps->ttyo->screen_cols, '='));
	if(ps->ttyo->screen_cols >= strlen(ustr) + 2)
	  strncpy(ctmp->value + (ps->ttyo->screen_cols - strlen(ustr))/2,
		  ustr, strlen(ustr));

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Reply Type */
	new_confline(&ctmp);
	ctmp->var       = &repl_type_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	sprintf(tmp, "%-*.100s =", indent-3, repl_type_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Choose One");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("---  --------------------");

	/* find longest value's name */
	for(lv = 0, i = 0; f = role_repl_types(i); i++)
	  if(lv < (j = strlen(f->name)))
	    lv = j;
	
	lv = min(lv, 100);

	for(i = 0; f = role_repl_types(i); i++){
	    new_confline(&ctmp);
	    ctmp->help_title= "HELP FOR ROLE REPLY USE";
	    ctmp->var       = &repl_type_var;
	    ctmp->valoffset = 12;
	    ctmp->keymenu   = &config_radiobutton_keymenu;
	    ctmp->help      = h_config_role_replyuse;
	    ctmp->varmem    = i;
	    ctmp->tool      = radio_tool;
	    ctmp->varnamep  = ctmpb;
	    sprintf(tmp, "(%c)  %-*.*s", (((!(def && def->action) ||
					     def->action->repl_type == -1) &&
					    f->value == ROLE_REPL_DEFL) ||
					  (def && def->action &&
					  f->value == def->action->repl_type))
					     ? R_SELD : ' ',
		    lv, lv, f->name);
	    ctmp->value     = cpystr(tmp);
	}

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Forward Type */
	new_confline(&ctmp);
	ctmp->var       = &forw_type_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	sprintf(tmp, "%-*.100s =", indent-3, forw_type_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Choose One");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("---  --------------------");

	/* find longest value's name */
	for(lv = 0, i = 0; f = role_forw_types(i); i++)
	  if(lv < (j = strlen(f->name)))
	    lv = j;
	
	lv = min(lv, 100);

	for(i = 0; f = role_forw_types(i); i++){
	    new_confline(&ctmp);
	    ctmp->help_title= "HELP FOR ROLE FORWARD USE";
	    ctmp->var       = &forw_type_var;
	    ctmp->valoffset = 12;
	    ctmp->keymenu   = &config_radiobutton_keymenu;
	    ctmp->help      = h_config_role_forwarduse;
	    ctmp->varmem    = i;
	    ctmp->tool      = radio_tool;
	    ctmp->varnamep  = ctmpb;
	    sprintf(tmp, "(%c)  %-*.*s", (((!(def && def->action) ||
					     def->action->forw_type == -1) &&
					    f->value == ROLE_FORW_DEFL) ||
					  (def && def->action &&
					  f->value == def->action->forw_type))
					     ? R_SELD : ' ',
		    lv, lv, f->name);
	    ctmp->value     = cpystr(tmp);
	}

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Compose Type */
	new_confline(&ctmp);
	ctmp->var       = &comp_type_var;
	ctmp->valoffset = indent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	sprintf(tmp, "%-*.100s =", indent-3, comp_type_var.name);
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Choose One");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = 12;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("---  --------------------");

	/* find longest value's name */
	for(lv = 0, i = 0; f = role_comp_types(i); i++)
	  if(lv < (j = strlen(f->name)))
	    lv = j;
	
	lv = min(lv, 100);
	
	for(i = 0; f = role_comp_types(i); i++){
	    new_confline(&ctmp);
	    ctmp->help_title= "HELP FOR ROLE COMPOSE USE";
	    ctmp->var       = &comp_type_var;
	    ctmp->valoffset = 12;
	    ctmp->keymenu   = &config_radiobutton_keymenu;
	    ctmp->help      = h_config_role_composeuse;
	    ctmp->varmem    = i;
	    ctmp->tool      = radio_tool;
	    ctmp->varnamep  = ctmpb;
	    sprintf(tmp, "(%c)  %-*.*s", (((!(def && def->action) ||
					     def->action->comp_type == -1) &&
					    f->value == ROLE_COMP_DEFL) ||
					  (def && def->action &&
					  f->value == def->action->comp_type))
					     ? R_SELD : ' ',
		    lv, lv, f->name);
	    ctmp->value     = cpystr(tmp);
	}
    }

    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = saved_screen ? saved_screen->deferred_ro_warning : 0;
    rv = conf_scroll_screen(ps, &screen, first_line, title, "roles ",
			    (edit_incol && pico_usingcolor()) ? 1 : 0);

    /*
     * Now look at the fake variables and extract the information we
     * want from them.
     */

    if(rv == 1 && result){
	/*
	 * We know these variables exist, so we don't have to check that
	 * apval is nonnull before evaluating *apval.
	 */
	apval = APVAL(&nick_var, ew);
	nick = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(nick);

	apval = APVAL(&comment_var, ew);
	comment = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(comment);

	alval = ALVAL(&to_pat_var, ew);
	to_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&from_pat_var, ew);
	from_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&sender_pat_var, ew);
	sender_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&cc_pat_var, ew);
	cc_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&recip_pat_var, ew);
	recip_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&partic_pat_var, ew);
	partic_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&news_pat_var, ew);
	news_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&subj_pat_var, ew);
	subj_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&alltext_pat_var, ew);
	alltext_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&bodytext_pat_var, ew);
	bodytext_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&keyword_pat_var, ew);
	keyword_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&charset_pat_var, ew);
	charset_pat = *alval;
	*alval = NULL;

	apval = APVAL(&age_pat_var, ew);
	age_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(age_pat);

	apval = APVAL(&size_pat_var, ew);
	size_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(size_pat);

	apval = APVAL(&scorei_pat_var, ew);
	scorei_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(scorei_pat);

	apval = APVAL(&stat_del_var, ew);
	stat_del = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_del);

	apval = APVAL(&stat_new_var, ew);
	stat_new = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_new);

	apval = APVAL(&stat_rec_var, ew);
	stat_rec = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_rec);

	apval = APVAL(&stat_imp_var, ew);
	stat_imp = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_imp);

	apval = APVAL(&stat_ans_var, ew);
	stat_ans = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_ans);

	apval = APVAL(&stat_8bit_var, ew);
	stat_8bit = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_8bit);

	apval = APVAL(&stat_bom_var, ew);
	stat_bom = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_bom);

	apval = APVAL(&stat_boy_var, ew);
	stat_boy = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_boy);

	apval = APVAL(&fldr_type_var, ew);
	fldr_type_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(fldr_type_pat);

	alval = ALVAL(&folder_pat_var, ew);
	folder_pat = *alval;
	*alval = NULL;

	apval = APVAL(&abook_type_var, ew);
	abook_type_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(abook_type_pat);

	alval = ALVAL(&abook_pat_var, ew);
	abook_pat = *alval;
	*alval = NULL;

	apval = APVAL(&cati_var, ew);
	cati = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(cati);

	apval = APVAL(&cat_lim_var, ew);
	cat_lim = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(cat_lim);

	apval = APVAL(&inick_var, ew);
	inick = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(inick);

	apval = APVAL(&from_act_var, ew);
	from_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(from_act);

	apval = APVAL(&replyto_act_var, ew);
	replyto_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(replyto_act);

	apval = APVAL(&fcc_act_var, ew);
	fcc_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(fcc_act);

	apval = APVAL(&litsig_act_var, ew);
	litsig_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(litsig_act);

	apval = APVAL(&sort_act_var, ew);
	sort_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(sort_act);

	apval = APVAL(&iform_act_var, ew);
	iform_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(iform_act);

	apval = APVAL(&startup_var, ew);
	startup_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(startup_act);

	apval = APVAL(&sig_act_var, ew);
	sig_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(sig_act);

	apval = APVAL(&templ_act_var, ew);
	templ_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(templ_act);

	apval = APVAL(&score_act_var, ew);
	score_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(score_act);

	apval = APVAL(&repl_type_var, ew);
	repl_type = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(repl_type);

	apval = APVAL(&forw_type_var, ew);
	forw_type = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(forw_type);

	apval = APVAL(&comp_type_var, ew);
	comp_type = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(comp_type);

	apval = APVAL(&rolecolor_vars[0], ew);
	rc_fg = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(rc_fg);

	apval = APVAL(&rolecolor_vars[1], ew);
	rc_bg = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(rc_bg);

	apval = APVAL(&filter_type_var, ew);
	filter_type = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filter_type);

	alval = ALVAL(&folder_act_var, ew);
	folder_act = *alval;
	*alval = NULL;

	alval = ALVAL(&keyword_set_var, ew);
	keyword_set = *alval;
	*alval = NULL;

	alval = ALVAL(&keyword_clr_var, ew);
	keyword_clr = *alval;
	*alval = NULL;

	apval = APVAL(&filt_imp_var, ew);
	filt_imp = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filt_imp);

	apval = APVAL(&filt_del_var, ew);
	filt_del = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filt_del);

	apval = APVAL(&filt_new_var, ew);
	filt_new = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filt_new);

	apval = APVAL(&filt_ans_var, ew);
	filt_ans = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filt_ans);


	alval = ALVAL(&cat_cmd_var, ew);
	cat_cmd = *alval;
	*alval = NULL;

	alval = ALVAL(&cstm_act_var, ew);
	cstm_act = *alval;
	*alval = NULL;

	alval = ALVAL(&smtp_act_var, ew);
	smtp_act = *alval;
	*alval = NULL;

	alval = ALVAL(&nntp_act_var, ew);
	nntp_act = *alval;
	*alval = NULL;

	if(ps->VAR_OPER_DIR && sig_act &&
	   is_absolute_path(sig_act) &&
	   !in_dir(ps->VAR_OPER_DIR, sig_act)){
	    q_status_message1(SM_ORDER | SM_DING, 3, 4,
			      "Warning: Sig file can't be outside of %.200s",
			      ps->VAR_OPER_DIR);
	}

	if(ps->VAR_OPER_DIR && templ_act &&
	   is_absolute_path(templ_act) &&
	   !in_dir(ps->VAR_OPER_DIR, templ_act)){
	    q_status_message1(SM_ORDER | SM_DING, 3, 4,
			    "Warning: Template file can't be outside of %.200s",
			    ps->VAR_OPER_DIR);
	}

	if(ps->VAR_OPER_DIR)
	  for(i = 0; folder_act[i]; i++){
	    if(folder_act[i][0] && is_absolute_path(folder_act[i]) &&
	       !in_dir(ps->VAR_OPER_DIR, folder_act[i])){
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  "Warning: Folder can't be outside of %.200s",
				  ps->VAR_OPER_DIR);
	    }
	  }

	*result = (PAT_S *)fs_get(sizeof(**result));
	memset((void *)(*result), 0, sizeof(**result));

	(*result)->patgrp = (PATGRP_S *)fs_get(sizeof(*(*result)->patgrp));
	memset((void *)(*result)->patgrp, 0, sizeof(*(*result)->patgrp));

	(*result)->action = (ACTION_S *)fs_get(sizeof(*(*result)->action));
	memset((void *)(*result)->action, 0, sizeof(*(*result)->action));

	(*result)->patline = def ? def->patline : NULL;
	
	if(nick && *nick){
	    (*result)->patgrp->nick = nick;
	    nick = NULL;
	}
	else
	  (*result)->patgrp->nick = cpystr(nick_var.global_val.p);

	if(comment && *comment){
	    (*result)->patgrp->comment = comment;
	    comment = NULL;
	}

	(*result)->action->nick = cpystr((*result)->patgrp->nick);

	(*result)->action->is_a_role   = edit_role  ? 1 : 0;
	(*result)->action->is_a_incol  = edit_incol ? 1 : 0;
	(*result)->action->is_a_score  = edit_score ? 1 : 0;
	(*result)->action->is_a_filter = edit_filter ? 1 : 0;
	(*result)->action->is_a_other  = edit_other ? 1 : 0;

	(*result)->patgrp->to      = editlist_to_pattern(to_pat);
	if((*result)->patgrp->to && !strncmp(to_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->to->not = 1;

	(*result)->patgrp->from    = editlist_to_pattern(from_pat);
	if((*result)->patgrp->from && !strncmp(from_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->from->not = 1;

	(*result)->patgrp->sender  = editlist_to_pattern(sender_pat);
	if((*result)->patgrp->sender &&
	   !strncmp(sender_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->sender->not = 1;

	(*result)->patgrp->cc      = editlist_to_pattern(cc_pat);
	if((*result)->patgrp->cc && !strncmp(cc_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->cc->not = 1;

	(*result)->patgrp->recip   = editlist_to_pattern(recip_pat);
	if((*result)->patgrp->recip &&
	   !strncmp(recip_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->recip->not = 1;

	(*result)->patgrp->partic  = editlist_to_pattern(partic_pat);
	if((*result)->patgrp->partic &&
	   !strncmp(partic_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->partic->not = 1;

	(*result)->patgrp->news    = editlist_to_pattern(news_pat);
	if((*result)->patgrp->news && !strncmp(news_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->news->not = 1;

	(*result)->patgrp->subj    = editlist_to_pattern(subj_pat);
	if((*result)->patgrp->subj && !strncmp(subj_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->subj->not = 1;

	(*result)->patgrp->alltext = editlist_to_pattern(alltext_pat);
	if((*result)->patgrp->alltext &&
	   !strncmp(alltext_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->alltext->not = 1;

	(*result)->patgrp->bodytext = editlist_to_pattern(bodytext_pat);
	if((*result)->patgrp->bodytext &&
	   !strncmp(bodytext_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->bodytext->not = 1;

	(*result)->patgrp->keyword = editlist_to_pattern(keyword_pat);
	if((*result)->patgrp->keyword &&
	   !strncmp(keyword_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->keyword->not = 1;

	(*result)->patgrp->charsets = editlist_to_pattern(charset_pat);
	if((*result)->patgrp->charsets &&
	   !strncmp(charset_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->charsets->not = 1;

	(*result)->patgrp->age_uses_sentdate =
		bitnset(FEAT_SENTDATE, feat_option_list) ? 1 : 0;

	if(age_pat){
	    if(((*result)->patgrp->age  = parse_intvl(age_pat)) != NULL)
	      (*result)->patgrp->do_age = 1;
	}

	if(size_pat){
	    if(((*result)->patgrp->size  = parse_intvl(size_pat)) != NULL)
	      (*result)->patgrp->do_size = 1;
	}

	if(scorei_pat){
	    if(((*result)->patgrp->score = parse_intvl(scorei_pat)) != NULL)
	      (*result)->patgrp->do_score  = 1;
	}

	(*result)->patgrp->cat_lim = -1L; /* default */
	if(cat_cmd){
	    if(!cat_cmd[0])
	      fs_give((void **) &cat_cmd);

	    /* quick check for absolute paths */
	    if(cat_cmd)
	      for(j = 0; cat_cmd[j]; j++)
		if(!is_absolute_path(cat_cmd[j]))
		  q_status_message1(SM_ORDER | SM_DING, 3, 4,
			"Warning: command must be absolute path: \"%.200s\"",
			cat_cmd[j]);

	    (*result)->patgrp->category_cmd = cat_cmd;
	    cat_cmd = NULL;

	    if(cati){
		if(((*result)->patgrp->cat = parse_intvl(cati)) != NULL)
		  (*result)->patgrp->do_cat  = 1;
	    }
	    if(cat_lim && *cat_lim)
	      (*result)->patgrp->cat_lim = atol(cat_lim);
	}

	if(stat_del && *stat_del){
	    for(j = 0; f = role_status_types(j); j++)
	      if(!strucmp(stat_del, f->name)){
		  (*result)->patgrp->stat_del = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_del = PAT_STAT_EITHER;

	if(stat_new && *stat_new){
	    for(j = 0; f = role_status_types(j); j++)
	      if(!strucmp(stat_new, f->name)){
		  (*result)->patgrp->stat_new = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_new = PAT_STAT_EITHER;

	if(stat_rec && *stat_rec){
	    for(j = 0; f = role_status_types(j); j++)
	      if(!strucmp(stat_rec, f->name)){
		  (*result)->patgrp->stat_rec = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_rec = PAT_STAT_EITHER;

	if(stat_imp && *stat_imp){
	    for(j = 0; f = role_status_types(j); j++)
	      if(!strucmp(stat_imp, f->name)){
		  (*result)->patgrp->stat_imp = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_imp = PAT_STAT_EITHER;

	if(stat_ans && *stat_ans){
	    for(j = 0; f = role_status_types(j); j++)
	      if(!strucmp(stat_ans, f->name)){
		  (*result)->patgrp->stat_ans = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_ans = PAT_STAT_EITHER;

	if(stat_8bit && *stat_8bit){
	    for(j = 0; f = role_status_types(j); j++)
	      if(!strucmp(stat_8bit, f->name)){
		  (*result)->patgrp->stat_8bitsubj = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_8bitsubj = PAT_STAT_EITHER;

	if(stat_bom && *stat_bom){
	    for(j = 0; f = role_status_types(j); j++)
	      if(!strucmp(stat_bom, f->name)){
		  (*result)->patgrp->stat_bom = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_bom = PAT_STAT_EITHER;

	if(stat_boy && *stat_boy){
	    for(j = 0; f = role_status_types(j); j++)
	      if(!strucmp(stat_boy, f->name)){
		  (*result)->patgrp->stat_boy = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_boy = PAT_STAT_EITHER;

	if(sort_act){
	    decode_sort(sort_act, &def_sort, &def_sort_rev);
	    (*result)->action->sort_is_set = 1;
	    (*result)->action->sortorder = def_sort;
	    (*result)->action->revsort = (def_sort_rev ? 1 : 0);
	    /*
	     * Don't try to re-sort until next open of folder. If user
	     * $-sorted then it probably shouldn't change anyway. Why
	     * bother keeping track of that?
	     */
	}

	(*result)->action->index_format = iform_act;
	iform_act = NULL;

	if(startup_act && *startup_act){
	    for(j = 0; f = startup_rules(j); j++)
	      if(!strucmp(startup_act, f->name)){
		  (*result)->action->startup_rule = f->value;
		  break;
	      }
	}
	else
	  (*result)->action->startup_rule = IS_NOTSET;

	aa = NULL;
	for(ea = earb; ea; ea = ea->next){
	    char *xyz;

	    if(aa){
		aa->next = (ARBHDR_S *)fs_get(sizeof(*aa));
		aa = aa->next;
	    }
	    else{
		(*result)->patgrp->arbhdr =
				      (ARBHDR_S *)fs_get(sizeof(ARBHDR_S));
		aa = (*result)->patgrp->arbhdr;
	    }

	    memset(aa, 0, sizeof(*aa));

	    aa->field = cpystr((ea->a && ea->a->field) ? ea->a->field : "");

	    alval = ALVAL(ea->v, ew);
	    spat = *alval;
	    *alval = NULL;
	    aa->p = editlist_to_pattern(spat);
	    if(aa->p && ea->v && ea->v->name &&
	       !strncmp(ea->v->name, NOT, NOTLEN))
	      aa->p->not = 1;

	    if((xyz = pattern_to_string(aa->p)) != NULL){
		if(!*xyz)
		  aa->isemptyval = 1;
		
		fs_give((void **)&xyz);
	    }
	}

	if(fldr_type_pat && *fldr_type_pat){
	    for(j = 0; f = pat_fldr_types(j); j++)
	      if(!strucmp(fldr_type_pat, f->name)){
		  (*result)->patgrp->fldr_type = f->value;
		  break;
	      }
	}
	else{
	    f = pat_fldr_types(FLDR_DEFL);
	    if(f)
	      (*result)->patgrp->fldr_type = f->value;
	}

	(*result)->patgrp->folder = editlist_to_pattern(folder_pat);

	if(abook_type_pat && *abook_type_pat){
	    for(j = 0; f = abookfrom_fldr_types(j); j++)
	      if(!strucmp(abook_type_pat, f->name)){
		  (*result)->patgrp->abookfrom = f->value;
		  break;
	      }
	}
	else{
	    f = abookfrom_fldr_types(AFRM_DEFL);
	    if(f)
	      (*result)->patgrp->abookfrom = f->value;
	}

	(*result)->patgrp->abooks = editlist_to_pattern(abook_pat);


	(*result)->action->inherit_nick = inick;
	inick = NULL;
	(*result)->action->fcc = fcc_act;
	fcc_act = NULL;
	(*result)->action->litsig = litsig_act;
	litsig_act = NULL;
	(*result)->action->sig = sig_act;
	sig_act = NULL;
	(*result)->action->template = templ_act;
	templ_act = NULL;

	if(cstm_act){
	    /*
	     * Check for From or Reply-To and eliminate them.
	     */
	    for(i = 0; cstm_act[i]; i++){
		char *free_this;

		if((!struncmp(cstm_act[i],"from",4) &&
		    (cstm_act[i][4] == ':' ||
		     cstm_act[i][4] == '\0')) ||
		   (!struncmp(cstm_act[i],"reply-to",8) &&
		    (cstm_act[i][8] == ':' ||
		     cstm_act[i][8] == '\0'))){
		    free_this = cstm_act[i];
		    /* slide the rest up */
		    for(j = i; cstm_act[j]; j++)
		      cstm_act[j] = cstm_act[j+1];

		    fs_give((void **)&free_this);
		    i--;	/* recheck now that we've slid them up */
		}
	    }

	    /* nothing left */
	    if(!cstm_act[0])
	      fs_give((void **)&cstm_act);

	    (*result)->action->cstm = cstm_act;
	    cstm_act = NULL;
	}

	if(smtp_act){
	    if(!smtp_act[0])
	      fs_give((void **)&smtp_act);

	    (*result)->action->smtp = smtp_act;
	    smtp_act = NULL;
	}

	if(nntp_act){
	    if(!nntp_act[0])
	      fs_give((void **)&nntp_act);

	    (*result)->action->nntp = nntp_act;
	    nntp_act = NULL;
	}

	if(filter_type && *filter_type){
	  (*result)->action->non_terminating =
			bitnset(FEAT_NONTERM, feat_option_list) ? 1 : 0;
	  for(i = 0; f = filter_types(i); i++){
	    if(!strucmp(filter_type, f->name)){
	      if(f->value == FILTER_FOLDER){
		(*result)->action->folder = editlist_to_pattern(folder_act);
		(*result)->action->move_only_if_not_deleted =
			bitnset(FEAT_IFNOTDEL, feat_option_list) ? 1 : 0;
	      }
	      else if(f->value == FILTER_STATE){
		(*result)->action->kill = 0;
	      }
	      else if(f->value == FILTER_KILL){
	        (*result)->action->kill = 1;
	      }

	      /*
	       * This is indented an extra indent because we used to condition
	       * this on !kill. We changed it so that you can set state bits
	       * even if you're killing. This is marginally helpful if you
	       * have another client running that doesn't know about this
	       * filter, but you want to, for example, have the messages show
	       * up now as deleted instead of having that deferred until we
	       * exit. It is controlled by the user by setting the status
	       * action bits along with the Delete.
	       */
		if(filt_imp && *filt_imp){
		  for(j = 0; f = msg_state_types(j); j++){
		    if(!strucmp(filt_imp, f->name)){
		      switch(f->value){
			case ACT_STAT_LEAVE:
			  break;
			case ACT_STAT_SET:
			  (*result)->action->state_setting_bits |= F_FLAG;
			  break;
			case ACT_STAT_CLEAR:
			  (*result)->action->state_setting_bits |= F_UNFLAG;
			  break;
		      }
		      break;
		    }
		  }
		}

		if(filt_del && *filt_del){
		  for(j = 0; f = msg_state_types(j); j++){
		    if(!strucmp(filt_del, f->name)){
		      switch(f->value){
			case ACT_STAT_LEAVE:
			  break;
			case ACT_STAT_SET:
			  (*result)->action->state_setting_bits |= F_DEL;
			  break;
			case ACT_STAT_CLEAR:
			  (*result)->action->state_setting_bits |= F_UNDEL;
			  break;
		      }
		      break;
		    }
		  }
		}

		if(filt_ans && *filt_ans){
		  for(j = 0; f = msg_state_types(j); j++){
		    if(!strucmp(filt_ans, f->name)){
		      switch(f->value){
			case ACT_STAT_LEAVE:
			  break;
			case ACT_STAT_SET:
			  (*result)->action->state_setting_bits |= F_ANS;
			  break;
			case ACT_STAT_CLEAR:
			  (*result)->action->state_setting_bits |= F_UNANS;
			  break;
		      }
		      break;
		    }
		  }
		}

		if(filt_new && *filt_new){
		  for(j = 0; f = msg_state_types(j); j++){
		    if(!strucmp(filt_new, f->name)){
		      switch(f->value){
			case ACT_STAT_LEAVE:
			  break;
			case ACT_STAT_SET:
			  (*result)->action->state_setting_bits |= F_UNSEEN;
			  break;
			case ACT_STAT_CLEAR:
			  (*result)->action->state_setting_bits |= F_SEEN;
			  break;
		      }
		      break;
		    }
		  }
		}

		(*result)->action->keyword_set =
					editlist_to_pattern(keyword_set);
		(*result)->action->keyword_clr =
					editlist_to_pattern(keyword_clr);

	      break;
	    }
	  }
	}

	if(from_act && *from_act)
	  rfc822_parse_adrlist(&(*result)->action->from, from_act,
			       ps->maildomain);

	if(replyto_act && *replyto_act)
	  rfc822_parse_adrlist(&(*result)->action->replyto, replyto_act,
			       ps->maildomain);

	if(score_act && (j = atoi(score_act)) >= SCORE_MIN && j <= SCORE_MAX)
	  (*result)->action->scoreval = (long) j;

	if(repl_type && *repl_type){
	    for(j = 0; f = role_repl_types(j); j++)
	      if(!strucmp(repl_type, f->name)){
		  (*result)->action->repl_type = f->value;
		  break;
	      }
	}
	else{
	    f = role_repl_types(ROLE_REPL_DEFL);
	    if(f)
	      (*result)->action->repl_type = f->value;
	}

	if(forw_type && *forw_type){
	    for(j = 0; f = role_forw_types(j); j++)
	      if(!strucmp(forw_type, f->name)){
		  (*result)->action->forw_type = f->value;
		  break;
	      }
	}
	else{
	    f = role_forw_types(ROLE_FORW_DEFL);
	    if(f)
	      (*result)->action->forw_type = f->value;
	}

	if(comp_type && *comp_type){
	    for(j = 0; f = role_comp_types(j); j++)
	      if(!strucmp(comp_type, f->name)){
		  (*result)->action->comp_type = f->value;
		  break;
	      }
	}
	else{
	    f = role_comp_types(ROLE_COMP_DEFL);
	    if(f)
	      (*result)->action->comp_type = f->value;
	}

	if(rc_fg && *rc_fg && rc_bg && *rc_bg){
	    if(!old_fg || !old_bg || strucmp(old_fg, rc_fg) ||
	       strucmp(old_bg, rc_bg))
	      clear_index_cache();

	    /*
	     * If same as normal color, don't set it. This may or may
	     * not surprise the user when they change the normal color.
	     * This color will track the normal color instead of staying
	     * the same as the old normal color, which is probably
	     * what they want.
	     */
	    if(!ps_global->VAR_NORM_FORE_COLOR ||
	       !ps_global->VAR_NORM_BACK_COLOR ||
	       strucmp(ps_global->VAR_NORM_FORE_COLOR, rc_fg) ||
	       strucmp(ps_global->VAR_NORM_BACK_COLOR, rc_bg))
	      (*result)->action->incol = new_color_pair(rc_fg, rc_bg);
	}
    }

    for(j = 0; varlist[j]; j++){
	v = varlist[j];
	free_variable_values(v);
	if(v->name)
	  fs_give((void **)&v->name);
    }

    if(earb)
      free_earb(&earb);
    if(nick)
      fs_give((void **)&nick);
    if(comment)
      fs_give((void **)&comment);
    if(to_pat)
      free_list_array(&to_pat);
    if(from_pat)
      free_list_array(&from_pat);
    if(sender_pat)
      free_list_array(&sender_pat);
    if(cc_pat)
      free_list_array(&cc_pat);
    if(recip_pat)
      free_list_array(&recip_pat);
    if(partic_pat)
      free_list_array(&partic_pat);
    if(news_pat)
      free_list_array(&news_pat);
    if(subj_pat)
      free_list_array(&subj_pat);
    if(alltext_pat)
      free_list_array(&alltext_pat);
    if(bodytext_pat)
      free_list_array(&bodytext_pat);
    if(keyword_pat)
      free_list_array(&keyword_pat);
    if(charset_pat)
      free_list_array(&charset_pat);
    if(age_pat)
      fs_give((void **)&age_pat);
    if(size_pat)
      fs_give((void **)&size_pat);
    if(scorei_pat)
      fs_give((void **)&scorei_pat);
    if(cati)
      fs_give((void **)&cati);
    if(cat_lim)
      fs_give((void **)&cat_lim);
    if(stat_del)
      fs_give((void **)&stat_del);
    if(stat_new)
      fs_give((void **)&stat_new);
    if(stat_rec)
      fs_give((void **)&stat_rec);
    if(stat_imp)
      fs_give((void **)&stat_imp);
    if(stat_ans)
      fs_give((void **)&stat_ans);
    if(stat_8bit)
      fs_give((void **)&stat_8bit);
    if(stat_bom)
      fs_give((void **)&stat_bom);
    if(stat_boy)
      fs_give((void **)&stat_boy);
    if(fldr_type_pat)
      fs_give((void **)&fldr_type_pat);
    if(folder_pat)
      free_list_array(&folder_pat);
    if(abook_type_pat)
      fs_give((void **)&abook_type_pat);
    if(abook_pat)
      free_list_array(&abook_pat);
    if(inick)
      fs_give((void **)&inick);
    if(from_act)
      fs_give((void **)&from_act);
    if(replyto_act)
      fs_give((void **)&replyto_act);
    if(fcc_act)
      fs_give((void **)&fcc_act);
    if(litsig_act)
      fs_give((void **)&litsig_act);
    if(sort_act)
      fs_give((void **)&sort_act);
    if(iform_act)
      fs_give((void **)&iform_act);
    if(keyword_set)
      free_list_array(&keyword_set);
    if(keyword_clr)
      free_list_array(&keyword_clr);
    if(startup_act)
      fs_give((void **)&startup_act);
    if(sig_act)
      fs_give((void **)&sig_act);
    if(templ_act)
      fs_give((void **)&templ_act);
    if(score_act)
      fs_give((void **)&score_act);
    if(repl_type)
      fs_give((void **)&repl_type);
    if(forw_type)
      fs_give((void **)&forw_type);
    if(comp_type)
      fs_give((void **)&comp_type);
    if(rc_fg)
      fs_give((void **)&rc_fg);
    if(rc_bg)
      fs_give((void **)&rc_bg);
    if(old_fg)
      fs_give((void **)&old_fg);
    if(old_bg)
      fs_give((void **)&old_bg);
    if(filt_del)
      fs_give((void **)&filt_del);
    if(filt_new)
      fs_give((void **)&filt_new);
    if(filt_ans)
      fs_give((void **)&filt_ans);
    if(filt_imp)
      fs_give((void **)&filt_imp);
    if(folder_act)
      free_list_array(&folder_act);
    if(filter_type)
      fs_give((void **)&filter_type);

    if(cat_cmd)
      free_list_array(&cat_cmd);

    if(cstm_act)
      free_list_array(&cstm_act);

    if(smtp_act)
      free_list_array(&smtp_act);

    if(nntp_act)
      free_list_array(&nntp_act);

    opt_screen = saved_screen;
    ps->mangled_screen = 1;
    return(rv);
}


void
setup_dummy_pattern_var(v, name, defpat)
    struct variable *v;
    char            *name;
    PATTERN_S       *defpat;
{
    char ***alval;
    char   *s, *p;
    int     cnt = 0;

    if(!(v && name))
      panic("setup_dummy_pattern_var");

    v->name = (char *) fs_get(strlen(name)+NOTLEN+1);
    sprintf(v->name, "%s%s", (defpat && defpat->not) ? NOT : "", name);
    v->is_used    = 1;
    v->is_user    = 1;
    v->is_list    = 1;
    alval = ALVAL(v, ew);
    *alval = pattern_to_editlist(defpat);
    set_current_val(v, FALSE, FALSE);
}


void
setup_role_pat(ps, ctmp, v, help, help_title, keymenu, tool, earb, indent)
    struct pine     *ps;
    CONF_S         **ctmp;
    struct variable *v;
    HelpType         help;
    char            *help_title;
    struct key_menu *keymenu;
    int            (*tool)();
    EARB_S         **earb;
    int              indent;
{
    char    tmp[MAXPATH+1];
    char  **lval;
    int     i;
    CONF_S *ctmpb;

    new_confline(ctmp);
    (*ctmp)->help_title = help_title;
    (*ctmp)->var        = v;
    (*ctmp)->valoffset  = indent;
    (*ctmp)->keymenu    = keymenu;
    (*ctmp)->help       = help;
    (*ctmp)->tool       = tool;
    sprintf(tmp, "%-*.*s =", indent-3, indent-3, (v && v->name) ? v->name : "");
    (*ctmp)->varname    = cpystr(tmp);
    (*ctmp)->varnamep   = *ctmp;
    (*ctmp)->value      = pretty_value(ps, *ctmp);
    (*ctmp)->d.earb     = earb;
    (*ctmp)->varmem     = 0;
    (*ctmp)->flags      = CF_STARTITEM;

    ctmpb = (*ctmp);

    lval = LVAL(v, ew);
    if(lval){
	for(i = 0; lval[i]; i++){
	    if(i)
	      new_confline(ctmp);
	    
	    (*ctmp)->var       = v;
	    (*ctmp)->varmem    = i;
	    (*ctmp)->valoffset = indent;
	    (*ctmp)->value     = pretty_value(ps, *ctmp);
	    (*ctmp)->d.earb    = earb;
	    (*ctmp)->keymenu   = keymenu;
	    (*ctmp)->help      = help;
	    (*ctmp)->tool      = tool;
	    (*ctmp)->varnamep  = ctmpb;
	}
    }
}


/*
 * Watch out for polarity of nosel flag. Setting it means to turn on
 * the NOSELECT flag, which means to make that line unselectable.
 */
void
setup_role_pat_alt(ps, ctmp, v, help, help_title, keymenu, tool, voff, nosel)
    struct pine     *ps;
    CONF_S         **ctmp;
    struct variable *v;
    HelpType         help;
    char            *help_title;
    struct key_menu *keymenu;
    int            (*tool)();
    int              voff;
    int              nosel;
{
    char    tmp[MAXPATH+1];
    char  **lval;
    int     i, j;
    CONF_S *ctmpb;

    new_confline(ctmp);
    (*ctmp)->help_title = help_title;
    (*ctmp)->var        = v;

    (*ctmp)->varoffset  = voff;
    j                   = voff+strlen(v->name)+3;
    (*ctmp)->valoffset  = j;

    (*ctmp)->keymenu    = keymenu;
    (*ctmp)->help       = help;
    (*ctmp)->tool       = tool;

    sprintf(tmp, "%.100s =", v->name);
    (*ctmp)->varname    = cpystr(tmp);

    (*ctmp)->varnamep   = *ctmp;
    (*ctmp)->value      = pretty_value(ps, *ctmp);
    (*ctmp)->varmem     = 0;

    (*ctmp)->flags      = (nosel ? CF_NOSELECT : 0);

    ctmpb = (*ctmp);

    lval = LVAL(v, ew);
    if(lval){
	for(i = 0; lval[i]; i++){
	    if(i)
	      new_confline(ctmp);
	    
	    (*ctmp)->var       = v;
	    (*ctmp)->varmem    = i;
	    (*ctmp)->varoffset = voff;
	    (*ctmp)->valoffset = j;
	    (*ctmp)->value     = pretty_value(ps, *ctmp);
	    (*ctmp)->keymenu   = keymenu;
	    (*ctmp)->help      = help;
	    (*ctmp)->tool      = tool;
	    (*ctmp)->varnamep  = ctmpb;
	    (*ctmp)->flags     = (nosel ? CF_NOSELECT : 0);
	}
    }
}


void
free_earb(ea)
    EARB_S **ea;
{
    if(ea && *ea){
	free_earb(&(*ea)->next);
	if((*ea)->v){
	    free_variable_values((*ea)->v);
	    if((*ea)->v->name)
	      fs_give((void **) &(*ea)->v->name);

	    fs_give((void **) &(*ea)->v);;
	}

	free_arbhdr(&(*ea)->a);
	fs_give((void **) ea);
    }
}


void
calculate_inick_stuff(ps)
    struct pine *ps;
{
    ACTION_S        *role, *irole;
    CONF_S          *ctmp, *ctmpa;
    struct variable *v;
    int              i;
    char            *nick;

    if(inick_confs[INICK_INICK_CONF] == NULL)
      return;

    for(i = INICK_FROM_CONF; i <= INICK_NNTP_CONF; i++){
	v = inick_confs[i] ? inick_confs[i]->var : NULL;
	if(v)
	  if(v->is_list){
	      if(v->global_val.l)
		free_list_array(&v->global_val.l);
	  }
	  else{
	      if(v->global_val.p)
		fs_give((void **)&v->global_val.p);
	  }
    }

    nick = PVAL(inick_confs[INICK_INICK_CONF]->var, ew);

    if(nick){
	/*
	 * Use an empty role with inherit_nick set to nick and then use the
	 * combine function to find the action values.
	 */
	role = (ACTION_S *)fs_get(sizeof(*role));
	memset((void *)role, 0, sizeof(*role));
	role->is_a_role = 1;
	role->inherit_nick = cpystr(nick);
	irole = combine_inherited_role(role);

	ctmp = inick_confs[INICK_FROM_CONF];
	v = ctmp ? ctmp->var : NULL;

	if(irole && irole->from){
	    char *bufp;

	    bufp = (char *)fs_get((size_t)est_size(irole->from));
	    bufp[0] = '\0';
	    rfc822_write_address(bufp, irole->from);
	    v->global_val.p = bufp;
	}

	ctmp = inick_confs[INICK_REPLYTO_CONF];
	v = ctmp ? ctmp->var : NULL;

	if(irole && irole->replyto){
	    char *bufp;

	    bufp = (char *)fs_get((size_t)est_size(irole->replyto));
	    bufp[0] = '\0';
	    rfc822_write_address(bufp, irole->replyto);
	    v->global_val.p = bufp;
	}

	ctmp = inick_confs[INICK_FCC_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.p = (irole && irole->fcc) ? cpystr(irole->fcc) : NULL;
	
	ctmp = inick_confs[INICK_LITSIG_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.p = (irole && irole->litsig) ? cpystr(irole->litsig)
						   : NULL;

	ctmp = inick_confs[INICK_SIG_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.p = (irole && irole->sig) ? cpystr(irole->sig) : NULL;

	ctmp = inick_confs[INICK_TEMPL_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.p = (irole && irole->template)
					? cpystr(irole->template) : NULL;

	ctmp = inick_confs[INICK_CSTM_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.l = (irole && irole->cstm) ? copy_list_array(irole->cstm)
						 : NULL;

	ctmp = inick_confs[INICK_SMTP_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.l = (irole && irole->smtp) ? copy_list_array(irole->smtp)
						 : NULL;

	ctmp = inick_confs[INICK_NNTP_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.l = (irole && irole->nntp) ? copy_list_array(irole->nntp)
						 : NULL;

	free_action(&role);
	free_action(&irole);
    }

    for(i = INICK_INICK_CONF; i <= INICK_NNTP_CONF; i++){
	ctmp = inick_confs[i];
	v = ctmp ? ctmp->var : NULL;
	/*
	 * If we didn't set a global_val using the nick above, then
	 * set one here for each variable that uses one.
	 */
	if(v && !v->global_val.p){
	    char    *str, *astr, *lc, pdir[MAXPATH+1];
	    ADDRESS *addr;
	    int      len;

	    switch(i){
	      case INICK_FROM_CONF:
		addr = generate_from();
		astr = addr_list_string(addr, NULL, 0, 1);
		str = (astr && astr[0]) ? astr : "?";
		v->global_val.p = (char *)fs_get((strlen(str) + 20) *
							    sizeof(char));
		sprintf(v->global_val.p, "%s%s)", DSTRING, str);
		if(astr)
		  fs_give((void **)&astr);

		if(addr)
		  mail_free_address(&addr);

		break;

	      case INICK_FCC_CONF:
		v->global_val.p = cpystr(VSTRING);
		break;

	      case INICK_LITSIG_CONF:
		/*
		 * This default works this way because of the ordering
		 * of the choices in the detoken routine.
		 */
		if(ps->VAR_LITERAL_SIG){
		    str = ps->VAR_LITERAL_SIG;
		    v->global_val.p = (char *)fs_get((strlen(str) + 20) *
								sizeof(char));
		    sprintf(v->global_val.p,
			    "%s%s)", DSTRING, str);
		}

		break;

	      case INICK_SIG_CONF:
		pdir[0] = '\0';
		if(ps_global->VAR_OPER_DIR){
		    strncpy(pdir, ps_global->VAR_OPER_DIR, MAXPATH);
		    pdir[MAXPATH] = '\0';
		    len = strlen(pdir) + 1;
		}
		else if((lc = last_cmpnt(ps_global->pinerc)) != NULL){
		    strncpy(pdir, ps_global->pinerc,
			    min(MAXPATH,lc-ps_global->pinerc));
		    pdir[min(MAXPATH, lc-ps_global->pinerc)] = '\0';
		    len = strlen(pdir);
		}

		if(pdir[0] && ps->VAR_SIGNATURE_FILE &&
		   ps->VAR_SIGNATURE_FILE[0] &&
		   is_absolute_path(ps->VAR_SIGNATURE_FILE) &&
		   !strncmp(ps->VAR_SIGNATURE_FILE, pdir, len)){
		    str = ps->VAR_SIGNATURE_FILE + len;
		}
		else
		  str = (ps->VAR_SIGNATURE_FILE && ps->VAR_SIGNATURE_FILE[0])
			  ? ps->VAR_SIGNATURE_FILE : NULL;
		if(str){
		    v->global_val.p = (char *)fs_get((strlen(str) + 20) *
								sizeof(char));
		    sprintf(v->global_val.p, "%s%s)", DSTRING, str);
		}

		break;

	      case INICK_INICK_CONF:
	      case INICK_REPLYTO_CONF:
	      case INICK_TEMPL_CONF:
	      case INICK_CSTM_CONF:
	      case INICK_SMTP_CONF:
	      case INICK_NNTP_CONF:
		break;
	    }
	}

	if(v)
	  set_current_val(v, FALSE, FALSE);

	if(ctmp){
	    CONF_S          *ctmpsig = NULL;
	    struct variable *vlsig;

	    for(ctmpa = ctmp;
		ctmpa && ctmpa->varnamep == ctmp;
		ctmpa = ctmpa->next){
		if(ctmpa->value)
		  fs_give((void **)&ctmpa->value);

		ctmpa->value = pretty_value(ps, ctmpa);
	    }

	    if(i == INICK_SIG_CONF){
		/*
		 * Turn off NOSELECT, but possibly turn it on again
		 * in next line.
		 */
		if(ctmpsig = inick_confs[INICK_SIG_CONF])
		  ctmpsig->flags &= ~CF_NOSELECT;

		if(inick_confs[INICK_LITSIG_CONF] && 
		   (vlsig = inick_confs[INICK_LITSIG_CONF]->var) &&
		   vlsig->current_val.p &&
		   vlsig->current_val.p[0]){
		    if(ctmp->value)
		      fs_give((void **)&ctmp->value);

		    ctmp->value =
				cpystr("<Ignored: using LiteralSig instead>");
		    
		    ctmp->flags |= CF_NOSELECT;
		}
	    }
	}
    }
}


NAMEVAL_S *
pat_fldr_types(index)
    int index;
{
    static NAMEVAL_S pat_fldr_list[] = {
	{"Any",			"ANY",		FLDR_ANY},
	{"News",		"NEWS",		FLDR_NEWS},
	{"Email",		"EMAIL",	FLDR_EMAIL},
	{"Specific (Enter Incoming Nicknames or use ^T)", "SPEC", FLDR_SPECIFIC}
    };

    return((index >= 0 &&
	    index < (sizeof(pat_fldr_list)/sizeof(pat_fldr_list[0])))
		   ? &pat_fldr_list[index] : NULL);
}


NAMEVAL_S *
abookfrom_fldr_types(index)
    int index;
{
    static NAMEVAL_S abookfrom_fldr_list[] = {
	{"Don't care, always matches",			"E",	AFRM_EITHER},
	{"Yes, in any address book",			"YES",	AFRM_YES},
	{"No, not in any address book",			"NO",	AFRM_NO},
	{"Yes, in specific address books",		"SYES",	AFRM_SPEC_YES},
	{"No, not in any of specific address books",	"SNO",	AFRM_SPEC_NO}
    };

    return((index >= 0 &&
	   index < (sizeof(abookfrom_fldr_list)/sizeof(abookfrom_fldr_list[0])))
		   ? &abookfrom_fldr_list[index] : NULL);
}


NAMEVAL_S *
filter_types(index)
    int index;
{
    static NAMEVAL_S filter_type_list[] = {
	{"Just Set Message Status",	"NONE",		FILTER_STATE},
	{"Delete",			"DEL",		FILTER_KILL},
	{"Move (Enter folder name(s) in primary collection, or use ^T)",
						    "FLDR", FILTER_FOLDER}
    };

    return((index >= 0 &&
	    index < (sizeof(filter_type_list)/sizeof(filter_type_list[0])))
		   ? &filter_type_list[index] : NULL);
}


NAMEVAL_S *
role_repl_types(index)
    int index;
{
    static NAMEVAL_S role_repl_list[] = {
	{"Never",			"NO",	ROLE_REPL_NO},
	{"With confirmation",		"YES",	ROLE_REPL_YES},
	{"Without confirmation",	"NC",	ROLE_REPL_NOCONF}
    };

    return((index >= 0 &&
	    index < (sizeof(role_repl_list)/sizeof(role_repl_list[0])))
		   ? &role_repl_list[index] : NULL);
}


NAMEVAL_S *
role_forw_types(index)
    int index;
{
    static NAMEVAL_S role_forw_list[] = {
	{"Never",		  	"NO",	ROLE_FORW_NO},
	{"With confirmation",		"YES",	ROLE_FORW_YES},
	{"Without confirmation",	"NC",	ROLE_FORW_NOCONF}
    };

    return((index >= 0 &&
	    index < (sizeof(role_forw_list)/sizeof(role_forw_list[0])))
		   ? &role_forw_list[index] : NULL);
}


NAMEVAL_S *
role_comp_types(index)
    int index;
{
    static NAMEVAL_S role_comp_list[] = {
	{"Never",		  	"NO",	ROLE_COMP_NO},
	{"With confirmation",		"YES",	ROLE_COMP_YES},
	{"Without confirmation",	"NC",	ROLE_COMP_NOCONF}
    };

    return((index >= 0 &&
	    index < (sizeof(role_comp_list)/sizeof(role_comp_list[0])))
		   ? &role_comp_list[index] : NULL);
}


NAMEVAL_S *
role_status_types(index)
    int index;
{
    static NAMEVAL_S role_status_list[] = {
	{"Don't care, always matches",	"E",	PAT_STAT_EITHER},
	{"Yes",				"YES",	PAT_STAT_YES},
	{"No",				"NO",	PAT_STAT_NO}
    };

    return((index >= 0 &&
	    index < (sizeof(role_status_list)/sizeof(role_status_list[0])))
		   ? &role_status_list[index] : NULL);
}


NAMEVAL_S *
msg_state_types(index)
    int index;
{
    static NAMEVAL_S msg_state_list[] = {
	{"Don't change it",		"LV",	ACT_STAT_LEAVE},
	{"Set this state",		"SET",	ACT_STAT_SET},
	{"Clear this state",		"CLR",	ACT_STAT_CLEAR}
    };

    return((index >= 0 &&
	    index < (sizeof(msg_state_list)/sizeof(msg_state_list[0])))
		   ? &msg_state_list[index] : NULL);
}


/* Arguments:
 *      lst:  a list of folders
 *      action: a 1 or 0 value which basically says that str is associated with
 *   the filter action if true or the Current Folder type if false.
 * Return:
 *   Returns 2 on success (user wants to exit) and 0 on failure.
 *
 * This function cycles through a list of folders and checks whether or not each
 * folder exists.  If they exist, return 2, if they don't exist, notify the user
 * or offer to create depending on whether or not it is a filter action or not. 
 * With each of these prompts, the user can abort their desire to exit.
 */
int
check_role_folders(lst, action)
    char     **lst;
    unsigned   action;
{
    char       *cur_fn, wt_res, prompt[MAX_SCREEN_COLS];
    int         i, rv = 2;
    CONTEXT_S  *cntxt = NULL;
    char        nbuf1[MAX_SCREEN_COLS], nbuf2[MAX_SCREEN_COLS];
    int         space, w1, w2, exists;

    if(!(lst && *lst)){
      if(action)
	q_status_message(SM_ORDER, 3, 5,
			 "Set a valid Filter Action before Exiting");
      else
	q_status_message(SM_ORDER, 3, 5,
			 "Set a valid Specific Folder before Exiting");
      rv = 0;
      return rv;
    }

    for(i = 0; lst[i]; i++){
	if(action)
	  cur_fn = detoken_src(lst[i], FOR_FILT, NULL, NULL, NULL, NULL);
	else
	  cur_fn = lst[i];

	removing_leading_and_trailing_white_space(cur_fn);
	if(*cur_fn != '\0'){
	  space = MAXPROMPT;
	  if(is_absolute_path(cur_fn) || !context_isambig(cur_fn))
	    cntxt = NULL;
	  else
	    cntxt = default_save_context(ps_global->context_list);

	  if(!(exists=folder_exists(cntxt, cur_fn)) &&
    (action || !folder_is_nick(cur_fn,FOLDERS(ps_global->context_current), 0))){
	    if(cntxt && (action == 1)){
	      space -= 37;		/* for fixed part of prompt below */
	      w1 = max(1,min(strlen(cur_fn),space/2));
	      w2 = min(max(1,space-w1),strlen(cntxt->nickname));
	      w1 += max(0,space-w1-w2);
	      sprintf(prompt,
		      "Folder \"%.100s\" in <%.100s> doesn't exist. Create",
		      short_str(cur_fn,nbuf1,w1,MidDots),
		      short_str(cntxt->nickname,nbuf2,w2,MidDots));
	    }
	    else if(cntxt && (action == 0)){
	      space -= 51;		/* for fixed part of prompt below */
	      w1 = max(1,min(strlen(cur_fn),space/2));
	      w2 = min(max(1,space-w1),strlen(cntxt->nickname));
	      w1 += max(0,space-w1-w2);
	      sprintf(prompt,
		      "Folder \"%.100s\" in <%.100s> doesn't exist. Exit and save anyway",
		      short_str(cur_fn,nbuf1,w1,MidDots),
		      short_str(cntxt->nickname,nbuf2,w2,MidDots));
	    }
	    else if(!cntxt && (action == 1)){
	      space -= 31;		/* for fixed part of prompt below */
	      w1 = max(1,space);
	      sprintf(prompt,
		      "Folder \"%.100s\" doesn't exist. Create",
		      short_str(cur_fn,nbuf1,w1,MidDots));
	    }
	    else{ /*!cntxt && (action == 0) */
	      space -= 45;		/* for fixed part of prompt below */
	      w1 = max(1,space);
	      sprintf(prompt,
		      "Folder \"%.100s\" doesn't exist. Exit and save anyway",
		      short_str(cur_fn,nbuf1,w1,MidDots));
	    }

	    wt_res = want_to(prompt, 'n', 'x', NO_HELP, WT_NORM);
	    if(wt_res == 'y'){
	      if(action){
		if(context_create(cntxt, NULL, cur_fn)){
		  q_status_message(SM_ORDER,3,5,"Folder created");
		  maybe_add_to_incoming(cntxt, cur_fn);
		}
	      }
	      /* No message to notify of changes being saved, we can't */
	      /* assume that the role screen isn't exited yet          */
	      rv = 2;
	    }
	    else if(wt_res == 'n' && action){
	      rv = 2;
	      q_status_message(SM_ORDER,3,5,"Folder not created");
	    }
	    else{
	      q_status_message(SM_ORDER,3,5,"Exit cancelled");
	      rv = 0;
	      break;
	    }
	  }
	  else{
	      if(exists & FEX_ERROR){
	        if(ps_global->mm_log_error && ps_global->c_client_error)
		  q_status_message(SM_ORDER,3,5,ps_global->c_client_error);
		else
		  q_status_message1(SM_ORDER,3,5,"\"%.200s\": Trouble checking for folder existence", cur_fn);
	      }

	      rv = 2;
	  }
	}
	else{ /* blank item in list of folders */
	  if(action && lst[i+1] == '\0')
	    q_status_message(SM_ORDER,3,5,"Set a valid Filter Action before Exiting");
	  else /* !action && lst[i+1] == '\0' */
	    q_status_message(SM_ORDER,3,5,"Set a valid Specific Folder before Exiting");
	  rv = 0;
	  break;
	}

	if(cur_fn && cur_fn != lst[i])
	  fs_give((void **) &cur_fn);
    }

    return(rv);
}


void
maybe_add_to_incoming(cntxt, cur_fn)
    CONTEXT_S *cntxt;
    char      *cur_fn;
{
    char      name[MAILTMPLEN], nname[32];
    char      nbuf1[MAX_SCREEN_COLS], nbuf2[MAX_SCREEN_COLS];
    char      prompt[MAX_SCREEN_COLS];
    char   ***alval;
    int       i, found, space, w1, w2;
    FOLDER_S *f;

    if(ps_global->context_list->use & CNTXT_INCMNG &&
       ((alval = ALVAL(&ps_global->vars[V_INCOMING_FOLDERS], Main)) != NULL)){
	(void)context_apply(name, cntxt, cur_fn, sizeof(name));
	/*
	 * Since the folder didn't exist it is very unlikely that it is
	 * in the incoming-folders list already, but we're just checking
	 * to be sure. We should really be canonicalizing both names
	 * before comparing, but...
	 */
	for(found = 0, i = 0; *alval && (*alval)[i] && !found; i++){
	    char *nickname, *folder;

	    get_pair((*alval)[i], &nickname, &folder, 0, 0);
	    if(folder && !strucmp((*alval)[i], folder))
	      found++;
	    
	    if(nickname)
	      fs_give((void **)&nickname);
	    if(folder)
	      fs_give((void **)&folder);
	}

	if(found)
	  return;
	
	space = MAXPROMPT;
	space -= 15;		/* for fixed part of prompt below */
	w2 = max(1,
		min(space/2,min(strlen(ps_global->context_list->nickname),20)));
	w1 = max(1,space - w2);
	sprintf(prompt,
		"Add \"%.100s\" to %.100s list",
		short_str(name,nbuf1,w1,MidDots),
		short_str(ps_global->context_list->nickname,nbuf2,w2,MidDots));
	if(want_to(prompt, 'n', 'x', NO_HELP, WT_NORM) == 'y'){
	    char *pp;

	    nname[0] = '\0';
	    space = MAXPROMPT;
	    space -= 25;
	    w1 = max(1, space);
	    sprintf(prompt, "Nickname for folder \"%.100s\" : ",
		    short_str(name,nbuf1,w1,MidDots));
	    while(1){
		int rc;
		int flags = OE_APPEND_CURRENT;

		rc = optionally_enter(nname, -FOOTER_ROWS(ps_global), 0,
				      sizeof(nname), prompt, NULL,
				      NO_HELP, &flags);
		removing_leading_and_trailing_white_space(nname);
		if(rc == 0 && *nname){
		    /* see if nickname already exists */
		    found = 0;
		    if(!strucmp(ps_global->inbox_name, nname))
		      found++;

		    for(i = 0;
			!found &&
			  i < folder_total(FOLDERS(ps_global->context_list));
			i++){
			FOLDER_S *f;

			f = folder_entry(i, FOLDERS(ps_global->context_list));
			if(!strucmp(FLDR_NAME(f), nname))
			  found++;
		    }

		    if(found){
			q_status_message1(SM_ORDER | SM_DING, 3, 5,
				      "Nickname \"%.200s\" is already in use",
					  nname);
			continue;
		    }

		    break;
		}
		else if(rc == 3)
		  q_status_message(SM_ORDER, 0, 3, "No help yet.");
		else if(rc == 1){
		    q_status_message1(SM_ORDER, 0, 3,
		    "Not adding nickname to %.200s list",
		    ps_global->context_list->nickname);
		    return;
		}
	    }

	    pp = put_pair(nname, name);
	    f = new_folder(name, line_hash(pp));
	    f->nickname = cpystr(nname);
	    f->name_len = strlen(nname);
	    folder_insert(folder_total(FOLDERS(ps_global->context_list)), f,
			  FOLDERS(ps_global->context_list));
	    
	    if(!*alval){
		i = 0;
		*alval = (char **)fs_get(2 * sizeof(char *));
	    }
	    else{
		for(i = 0; (*alval)[i]; i++)
		  ;
		
		fs_resize((void **)alval, (i + 2) * sizeof(char *));
	    }

	    (*alval)[i]   = pp;
	    (*alval)[i+1] = NULL;
	    set_current_val(&ps_global->vars[V_INCOMING_FOLDERS], TRUE, TRUE);
	    write_pinerc(ps_global, ew, WRP_NONE);
	}
    }
}


int
role_filt_exitcheck(cl, flags)
    CONF_S      **cl;
    unsigned      flags;
{
    int             rv, j, action;
    char          **to_folder = NULL, **spec_fldr = NULL;
    CONF_S         *ctmp;
    NAMEVAL_S      *f;
#define ACT_UNKNOWN		0
#define ACT_KILL		1
#define ACT_MOVE		2
#define ACT_MOVE_NOFOLDER	3
#define ACT_STATE		4

    /*
     * We have to locate the lines which define the Filter Action and
     * then check to see that it is set to something before allowing
     * user to Exit.
     */
    action = ACT_UNKNOWN;
    if(flags & CF_CHANGES && role_filt_ptr && PVAL(role_filt_ptr,ew)){
	for(j = 0; f = filter_types(j); j++)
	  if(!strucmp(PVAL(role_filt_ptr,ew), f->name))
	    break;
	
	switch(f ? f->value : -1){
	  case FILTER_KILL:
	    action = ACT_KILL;
	    break;

	  case FILTER_STATE:
	    action = ACT_STATE;
	    break;

	  case FILTER_FOLDER:
	    /*
	     * Check that the folder is set to something.
	     */

	    action = ACT_MOVE_NOFOLDER;
	    /* go to end of screen */
	    for(ctmp = (*cl);
		ctmp && ctmp->next;
		ctmp = next_confline(ctmp))
	      ;

	    /* back up to start of Filter Action */
	    for(;
		ctmp &&
		!(ctmp->flags & CF_STARTITEM && ctmp->var == role_filt_ptr);
		ctmp = prev_confline(ctmp))
	      ;
	    
	    /* skip back past NOSELECTs */
	    for(;
		ctmp && (ctmp->flags & CF_NOSELECT);
		ctmp = next_confline(ctmp))
	      ;

	    /* find line with new var (the Folder line) */
	    for(;
		ctmp && (ctmp->var == role_filt_ptr);
		ctmp = next_confline(ctmp))
	      ;
	    
	    /* ok, we're finally at the Folder line */
	    if(ctmp && ctmp->var && LVAL(ctmp->var,ew)){
		to_folder = copy_list_array(LVAL(ctmp->var,ew));
		if(to_folder && to_folder[0])
		  action = ACT_MOVE;
	    }

	    break;

	  default:
	    dprint(1,(debugfile,
		    "Can't happen, role_filt_ptr set to %s\n",
		    PVAL(role_filt_ptr,ew) ? PVAL(role_filt_ptr,ew) : "?"));
	    break;
	}
    }

    if(flags & CF_CHANGES){
	switch(want_to((action == ACT_KILL)
	   ? "Commit changes (\"Yes\" means matching messages will be deleted)"
	   : EXIT_PMT, 'y', 'x', h_config_undo, WT_FLUSH_IN)){
	  case 'y':
	    switch(action){
	      case ACT_KILL:
		if(spec_fldr = get_role_specific_folder(cl)){
		  rv = check_role_folders(spec_fldr, 0);
		  free_list_array(&spec_fldr);
		  if(rv == 2)
		    q_status_message(SM_ORDER,0,3,"Ok, messages matching that Pattern will be deleted");
		}
		else{
		  q_status_message(SM_ORDER, 0, 3,
				   "Ok, messages matching that Pattern will be deleted");
		  rv = 2;
		}
		break;

	      case ACT_MOVE:
		if(spec_fldr = get_role_specific_folder(cl)){
		  rv = check_role_folders(spec_fldr, 0);
		  free_list_array(&spec_fldr);
		  if(to_folder && rv == 2)
		    rv = check_role_folders(to_folder, 1);  
		}
		else
		  rv = check_role_folders(to_folder, 1);  

		break;

	      case ACT_MOVE_NOFOLDER:
		rv = 0;
		q_status_message(SM_ORDER, 3, 5,
				 "Set a valid Filter Action before Exiting");
		break;

	      case ACT_STATE:
		if(spec_fldr = get_role_specific_folder(cl)){
		  rv = check_role_folders(spec_fldr, 0);
		  free_list_array(&spec_fldr);
		}
		else
		  rv = 2;

		break;

	      default:
		rv = 2;
		dprint(1,(debugfile,
		    "This can't happen, role_filt_ptr or to_folder not set\n"));
		break;
	    }

	    break;

	  case 'n':
	    q_status_message(SM_ORDER,3,5,"No filter changes saved");
	    rv = 10;
	    break;

	  case 'x':  /* ^C */
	  default :
	    q_status_message(SM_ORDER,3,5,"Changes not yet saved");
	    rv = 0;
	    break;
	}
    }
    else
      rv = 2;

    if(to_folder)
      free_list_array(&to_folder);

    return(rv);
}


/*
 * Don't allow exit unless user has set the action to something.
 */
int
role_filt_text_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int             rv;

    switch(cmd){
      case MC_EXIT:
	rv = role_filt_exitcheck(cl, flags);
	break;

      default:
	rv = role_text_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


int
role_filt_addhdr_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int rv;

    switch(cmd){
      case MC_EXIT:
	rv = role_filt_exitcheck(cl, flags);
	break;

      default:
	rv = role_addhdr_tool(ps, cmd, cl, flags);
	break;
    }

    return rv;
}

int
role_addhdr_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int rv;

    switch(cmd){
      case MC_ADDHDR:
      case MC_EXIT:
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      default:
	rv = -1;
	break;
    }

    return rv;
}

/*
 * Don't allow exit unless user has set the action to something.
 */
int
role_filt_radiobutton_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int             rv;

    switch(cmd){
      case MC_EXIT:
	rv = role_filt_exitcheck(cl, flags);
	break;

      default:
	rv = role_radiobutton_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


/*
 * simple radio-button style variable handler
 */
int
role_radiobutton_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int	       rv = 0, i;
    CONF_S    *ctmp, *spec_ctmp = NULL;
    NAMEVAL_S *rule;
    char     **apval;

    switch(cmd){
      case MC_CHOICE :				/* set/unset feature */

	/* back up to first line */
	for(ctmp = (*cl);
	    ctmp && ctmp->prev && !(ctmp->prev->flags & CF_NOSELECT);
	    ctmp = prev_confline(ctmp))
	  ;
	
	for(i = 0; ctmp && (!(ctmp->flags & CF_NOSELECT)
			    || (*cl)->var == role_fldr_ptr 
			    || (*cl)->var == role_afrom_ptr
			    || (*cl)->var == role_filt_ptr);
	    ctmp = next_confline(ctmp), i++){
	    if(((*cl)->var == role_fldr_ptr) ||
	       ((*cl)->var == role_afrom_ptr) ||
	       ((*cl)->var == role_filt_ptr)){
		if((((*cl)->var == role_fldr_ptr) && !pat_fldr_types(i))
		   || (((*cl)->var == role_afrom_ptr)
		       && !abookfrom_fldr_types(i))
		   || (((*cl)->var == role_filt_ptr) && !filter_types(i))){
		    spec_ctmp = ctmp;
		    break;
		}
	    }

	    ctmp->value[1] = ' ';
	}

	/* turn on current value */
	(*cl)->value[1] = R_SELD;

	if((*cl)->var == role_fldr_ptr){
	    for(ctmp = spec_ctmp;
		ctmp && ctmp->varnamep == spec_ctmp;
		ctmp = next_confline(ctmp))
	      if((*cl)->varmem == FLDR_SPECIFIC)
		ctmp->flags &= ~CF_NOSELECT;
	      else
		ctmp->flags |= CF_NOSELECT;

	    rule = pat_fldr_types((*cl)->varmem);
	}
	else if((*cl)->var == role_afrom_ptr){
	    for(ctmp = spec_ctmp;
		ctmp && ctmp->varnamep == spec_ctmp;
		ctmp = next_confline(ctmp))
	      if(((*cl)->varmem == AFRM_SPEC_YES)
	         || ((*cl)->varmem == AFRM_SPEC_NO))
	        ctmp->flags &= ~CF_NOSELECT;
	      else
	        ctmp->flags |= CF_NOSELECT;

	    rule = abookfrom_fldr_types((*cl)->varmem);
	}
	else if((*cl)->var == role_filt_ptr){
	    for(ctmp = spec_ctmp;
		ctmp && ctmp->varnamep == spec_ctmp;
		ctmp = next_confline(ctmp))
	      if((*cl)->varmem == FILTER_FOLDER)
		ctmp->flags &= ~CF_NOSELECT;
	      else
		ctmp->flags |= CF_NOSELECT;

	    rule = filter_types((*cl)->varmem);
	}
	else if((*cl)->var == role_forw_ptr)
	  rule = role_forw_types((*cl)->varmem);
	else if((*cl)->var == role_repl_ptr)
	  rule = role_repl_types((*cl)->varmem);
	else if((*cl)->var == role_status1_ptr ||
	        (*cl)->var == role_status2_ptr ||
	        (*cl)->var == role_status3_ptr ||
	        (*cl)->var == role_status4_ptr ||
	        (*cl)->var == role_status5_ptr ||
	        (*cl)->var == role_status6_ptr ||
	        (*cl)->var == role_status7_ptr ||
	        (*cl)->var == role_status8_ptr)
	  rule = role_status_types((*cl)->varmem);
	else if((*cl)->var == msg_state1_ptr ||
	        (*cl)->var == msg_state2_ptr ||
	        (*cl)->var == msg_state3_ptr ||
	        (*cl)->var == msg_state4_ptr)
	  rule = msg_state_types((*cl)->varmem);
	else
	  rule = role_comp_types((*cl)->varmem);

	apval = APVAL((*cl)->var, ew);
	if(apval && *apval)
	  fs_give((void **)apval);

	if(apval)
	  *apval = cpystr(rule->name);

	ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	rv = 1;

	break;

      case MC_EXIT:				/* exit */
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


int
role_sort_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int	       rv = 0, i;
    CONF_S    *ctmp;
    char     **apval;
    SortOrder  def_sort;
    int        def_sort_rev;

    apval = APVAL((*cl)->var, ew);

    switch(cmd){
      case MC_CHOICE :				/* set/unset feature */

	if((*cl)->varmem >= 0){
	    def_sort_rev = (*cl)->varmem >= (short) EndofList;
	    def_sort = (SortOrder)((*cl)->varmem - (def_sort_rev * EndofList));

	    sprintf(tmp_20k_buf, "%s%s", sort_name(def_sort),
		    (def_sort_rev) ? "/Reverse" : "");
	}

	if(apval){
	    if(*apval)
	      fs_give((void **)apval);
	    
	    if((*cl)->varmem >= 0)
	      *apval = cpystr(tmp_20k_buf);
	}

	/* back up to first line */
	for(ctmp = (*cl);
	    ctmp && ctmp->prev && !(ctmp->prev->flags & CF_NOSELECT);
	    ctmp = prev_confline(ctmp))
	  ;
	
	/* turn off all values */
	for(i = 0;
	    ctmp && !(ctmp->flags & CF_NOSELECT);
	    ctmp = next_confline(ctmp), i++)
	  ctmp->value[1] = ' ';

	/* turn on current value */
	(*cl)->value[1] = R_SELD;

	ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	rv = 1;

	break;

      case MC_EXIT:				/* exit */
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}

/*
 * Return an allocated list of the Specific Folder list for 
 * roles, or NULL if Current Folder type is not set to 
 * to Specific Folder
 *
 * WARNING, the method used in obtaining the specific folder is
 * VERY dependent on the order in which it is presented on the 
 * screen.  If the Current Folder radio buttons were changed,
 * this function would probably need to be fixed accordingly.
 */
char **
get_role_specific_folder(cl)
    CONF_S     **cl;
{
    CONF_S   *ctmp;

    /* go to the first line */
    for(ctmp = *cl;
	ctmp && ctmp->prev;
	ctmp = prev_confline(ctmp))
      ;
  
    /* go to the current folder radio button list */
    while(ctmp && ctmp->var != role_fldr_ptr)
      ctmp = next_confline(ctmp);

    /* go to the specific folder button (caution) */
    while(ctmp && ctmp->varmem != FLDR_SPECIFIC)
      ctmp = next_confline(ctmp);

    /* check if selected (assumption of format "(*)" */
    if(ctmp && ctmp->value[1] == R_SELD){
	/* go to next line, the start of the list */
	ctmp = next_confline(ctmp);
	if(LVAL(ctmp->var, ew))
	  return copy_list_array(LVAL(ctmp->var, ew));
	else{
	    char **ltmp;

	    /*
	     * Need to allocate empty string so as not to confuse it
	     * with the possibility that Specific Folder is not selected.
	     */
	    ltmp    = (char **) fs_get(2 * sizeof(char *));
	    ltmp[0] = cpystr("");
	    ltmp[1] = NULL;
	    return(ltmp);
	}
    }
    else
      return NULL;
}

/*
 */
int
role_litsig_text_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int   rv;

    switch(cmd){
      case MC_ADD :
      case MC_EDIT :
	rv = litsig_text_tool(ps, cmd, cl, flags);
	if(rv)
	  calculate_inick_stuff(ps);

	break;

      default :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


/*
 */
int
role_cstm_text_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int   rv;

    switch(cmd){
      case MC_EXIT :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      default :
	rv = text_tool(ps, cmd, cl, flags);
	if(rv == 1 && (*cl)->var){
	    char **lval;

	    lval = LVAL((*cl)->var, ew);
	    if(lval && lval[(*cl)->varmem] &&
	       ((!struncmp(lval[(*cl)->varmem],"from",4) &&
		 (lval[(*cl)->varmem][4] == ':' ||
		  lval[(*cl)->varmem][4] == '\0')) ||
	        (!struncmp(lval[(*cl)->varmem],"reply-to",8) &&
		 (lval[(*cl)->varmem][8] == ':' ||
		  lval[(*cl)->varmem][8] == '\0'))))
	      q_status_message1(SM_ORDER|SM_DING, 5, 7,
				"Use \"Set %.200s\" instead, Change ignored",
				!struncmp(lval[(*cl)->varmem],"from",4)
				    ? "From" : "Reply-To");
	}

	break;
    }

    return(rv);
}


/*
 */
int
role_text_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    OPT_SCREEN_S *saved_screen;
    int   rv = -1, oeflags, len, sig, r, i, cancel = 0;
    char *file, *err, title[20], *newfile, *lc, *addr, *fldr = NULL, *tmpfldr;
    char  dir2[MAXPATH+1], pdir[MAXPATH+1], *p;
    char  full_filename[MAXPATH+1], filename[MAXPATH+1];
    char  tmp[MAXPATH+1], **spec_fldr, **apval;
    EARB_S *earb, *ea, *eaprev;
    CONF_S *ctmp, *ctmpb, *newcp, *ctend;
    HelpType help;

    switch(cmd){
      case MC_EXIT :
	if(flags & CF_CHANGES){
	  switch(want_to(EXIT_PMT, 'y', 'x', h_config_role_undo, WT_FLUSH_IN)){
	    case 'y':
	      if(spec_fldr = get_role_specific_folder(cl)){
		rv = check_role_folders(spec_fldr, 0);
		free_list_array(&spec_fldr);
	      }
	      else 
		rv = 2;
	      break;

	    case 'n':
	      q_status_message(SM_ORDER,3,5,"No changes saved");
	      rv = 10;
	      break;

	    case 'x':  /* ^C */
	      q_status_message(SM_ORDER,3,5,"Changes not yet saved");
	      rv = 0;
	      break;
	  }
	}
	else
	  rv = 2;

	break;

      case MC_NOT :		/* toggle between !matching and matching */
	ctmp = (*cl)->varnamep;
	if(ctmp->varname && ctmp->var && ctmp->var->name){
	    if(!strncmp(ctmp->varname, NOT, NOTLEN) &&
	       !strncmp(ctmp->var->name, NOT, NOTLEN)){
		char *pp;

		rplstr(ctmp->var->name, NOTLEN, "");
		rplstr(ctmp->varname, NOTLEN, "");
		strncpy(ctmp->varname+strlen(ctmp->varname)-1,
		       repeat_char(NOTLEN, ' '), NOTLEN+1);
		strncat(ctmp->varname, "=", 2);
	    }
	    else{
		rplstr(ctmp->var->name, 0, NOT);
		strncpy(ctmp->varname+strlen(ctmp->varname)-1-NOTLEN, "=", 2);
		rplstr(ctmp->varname, 0, NOT);
	    }

	    rv = 1;
	}

	break;

      case MC_CHOICE :				/* Choose a file */
	/*
	 * In signature_path we read signature files relative to the pinerc
	 * dir, so if user selects one that is in there we'll make it
	 * relative instead of absolute, so it looks nicer.
	 */
	pdir[0] = '\0';
	if(ps_global->VAR_OPER_DIR){
	    strncpy(pdir, ps_global->VAR_OPER_DIR, MAXPATH);
	    pdir[MAXPATH] = '\0';
	    len = strlen(pdir) + 1;
	}
	else if((lc = last_cmpnt(ps_global->pinerc)) != NULL){
	    strncpy(pdir, ps_global->pinerc, min(MAXPATH,lc-ps_global->pinerc));
	    pdir[min(MAXPATH, lc-ps_global->pinerc)] = '\0';
	    len = strlen(pdir);
	}

	strncpy(title, "CHOOSE A", 15);
	strncpy(dir2, pdir, MAXPATH);

	filename[0] = '\0';
	build_path(full_filename, dir2, filename, sizeof(full_filename));

	r = file_lister(title, dir2, MAXPATH+1,
			filename, MAXPATH+1, 
			TRUE, FB_READ);
	ps->mangled_screen = 1;

	if(r == 1){
	    build_path(full_filename, dir2, filename, sizeof(full_filename));
	    removing_leading_and_trailing_white_space(full_filename);
	    if(!strncmp(full_filename, pdir, strlen(pdir)))
	      newfile = cpystr(full_filename + len); 
	    else
	      newfile = cpystr(full_filename);

	    apval = APVAL((*cl)->var, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);
	    
	    if(apval)
	      *apval = newfile;

	    if((*cl)->value)
	      fs_give((void **)&((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	break;

      case MC_CHOICEB :		/* Choose Addresses, no full names */
	addr = addr_book_multaddr_nf();
	ps->mangled_screen = 1;
	if(addr && (*cl)->var && (*cl)->var->is_list){
	    char **ltmp, *tmp;
	    int    i;

	    i = 0;
	    for(tmp = addr; *tmp; tmp++)
	      if(*tmp == ',')
		i++;	/* conservative count of ,'s */

	    ltmp = parse_list(addr, i + 1, PL_COMMAQUOTE, NULL);
	    fs_give((void **) &addr);

	    if(ltmp && ltmp[0])
	      config_add_list(ps, cl, ltmp, NULL, 0);

	    if(ltmp)
	      fs_give((void **) &ltmp);
	    
	    if((*cl)->value)
	      fs_give((void **)&((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	break;

      case MC_CHOICEC :		/* Choose an Address, no full name */
	addr = addr_book_oneaddr();
	ps->mangled_screen = 1;
	if(addr){
	    apval = APVAL((*cl)->var, ew);
	    if(apval && *apval)			/* replace current value */
	      fs_give((void **)apval);
	    
	    if(apval)
	      *apval = addr;

	    if((*cl)->value)
	      fs_give((void **)&((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	break;

      case MC_CHOICED :				/* Choose a Folder */
      case MC_CHOICEE :
	saved_screen = opt_screen;
	if(cmd == MC_CHOICED)
	  tmpfldr = folders_for_roles(FOR_PATTERN);
	else
	  tmpfldr = folders_for_roles(0);
	
	if(tmpfldr){
	    fldr = add_comma_escapes(tmpfldr);
	    fs_give((void**) &tmpfldr);
	}

	opt_screen = saved_screen;

	ps->mangled_screen = 1;
	if(fldr && *fldr && (*cl)->var && (*cl)->var->is_list){
	    char **ltmp;

	    ltmp    = (char **) fs_get(2 * sizeof(char *));
	    ltmp[0] = fldr;
	    ltmp[1] = NULL;
	    fldr    = NULL;

	    if(ltmp && ltmp[0])
	      config_add_list(ps, cl, ltmp, NULL, 0);

	    if(ltmp)
	      fs_give((void **) &ltmp);
	    
	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else if(fldr && *fldr && (*cl)->var && !(*cl)->var->is_list){
	    apval = APVAL((*cl)->var, ew);
	    if(apval && *apval)			/* replace current value */
	      fs_give((void **)apval);

	    if(apval){
		*apval = fldr;
		fldr   = NULL;
	    }

	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	if(fldr)
	  fs_give((void **) &fldr);

	break;

      case MC_EDITFILE :
	file = ((*cl)->var && PVAL((*cl)->var, ew))
		    ? cpystr(PVAL((*cl)->var, ew)) : NULL;
	if(file)
	  removing_leading_and_trailing_white_space(file);

	sig = (srchstr((*cl)->varname, "signature") != NULL);
	if(!file || !*file){
	    err = (char *)fs_get(100 * sizeof(char));
	    sprintf(err, "No %s file defined. First define a file name.",
		    sig ? "signature" : "template");
	}
	else{
	    if(file[len=(strlen(file)-1)] == '|')
	      file[len] = '\0';

	    sprintf(title, "%s EDITOR", sig ? "SIGNATURE" : "TEMPLATE");
	    err = signature_edit(file, title);
	}

	fs_give((void **)&file);
	if(err){
	    q_status_message1(SM_ORDER, 3, 5, "%.200s", err);
	    fs_give((void **)&err);
	}

	rv = 0;
	ps->mangled_screen = 1;
	break;

      /* Add an arbitrary header to this role */
      case MC_ADDHDR :
	rv = 0;
	/* make earb point to last one */
	for(earb = *(*cl)->d.earb; earb && earb->next; earb = earb->next)
	  ;

	/* Add new one to end of list */
	ea = (EARB_S *)fs_get(sizeof(*ea));
	memset((void *)ea, 0, sizeof(*ea));
	ea->v = (struct variable *)fs_get(sizeof(struct variable));
	memset((void *)ea->v, 0, sizeof(struct variable));
	ea->a = (ARBHDR_S *)fs_get(sizeof(ARBHDR_S));
	memset((void *)ea->a, 0, sizeof(ARBHDR_S));

	/* get new header field name */
	help = NO_HELP;
	tmp[0] = '\0';
	while(1){
	    i = optionally_enter(tmp, -FOOTER_ROWS(ps), 0, sizeof(tmp),
			     "Enter the name of the header field to be added: ",
				 NULL, help, NULL);
	    if(i == 0)
	      break;
	    else if(i == 1){
		cmd_cancelled("eXtraHdr");
		cancel = 1;
		break;
	    }
	    else if(i == 3){
		help = help == NO_HELP ? h_config_add_pat_hdr : NO_HELP;
		continue;
	    }
	    else
	      break;
	}

	ps->mangled_footer = 1;

	removing_leading_and_trailing_white_space(tmp);
	if(tmp[strlen(tmp)-1] == ':')  /* remove trailing colon */
	  tmp[strlen(tmp)-1] = '\0';

	removing_trailing_white_space(tmp);

	if(cancel || !tmp[0])
	  break;

	tmp[0] = islower((unsigned char)tmp[0]) ? toupper((unsigned char)tmp[0])
						: tmp[0];
	ea->a->field = cpystr(tmp);

	if(earb)
	  earb->next = ea;
	else
	  *((*cl)->d.earb) = ea;

	/* go to first line */
	for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	  ;

	/*
	 * Go to the Add Extra Headers line. We will put the
	 * new header before this line.
	 */
	for(; ctmp; ctmp = next_confline(ctmp))
	  if(ctmp->value && !strcmp(ctmp->value, ADDXHDRS))
	    break;

	/* move back one */
	if(ctmp)
	  ctmp = prev_confline(ctmp);
	
	/*
	 * Add a new line after this point, which is after the last
	 * extra header (if any) or after the Participant pattern, and
	 * before the Add Extra Headers placeholder line.
	 */
	p = (char *) fs_get(strlen(tmp) + strlen(" pattern") + 1);
	sprintf(p, "%s pattern", tmp);
	setup_dummy_pattern_var(ea->v, p, NULL);
	fs_give((void **) &p);

	/* find what indent should be */
	if(ctmp && ctmp->varnamep && ctmp->varnamep->varname)
	  i = min(max(strlen(ctmp->varnamep->varname) + 1, 3), 200);
	else
	  i = 20;
	
	setup_role_pat(ps, &ctmp, ea->v, h_config_role_arbpat,
		       ARB_HELP, &config_role_xtrahdr_keymenu,
		       ctmp->prev->tool, ctmp->prev->d.earb, i);

	/*
	 * move current line to new line
	 */

	newcp = ctmp;

	/* check if new line comes before the top of the screen */
	ctmpb = (opt_screen && opt_screen->top_line)
		    ? opt_screen->top_line->prev : NULL;
	for(; ctmpb; ctmpb = prev_confline(ctmpb))
	  if(ctmpb == newcp)
	    break;
	/*
	 * Keep the right lines visible.
	 * The if triggers if the new line is off the top of the screen, and
	 * it makes the new line be the top line.
	 * The else counts how many lines down the screen the new line is.
	 */

	i = 0;
	if(ctmpb == newcp)
	  opt_screen->top_line = newcp;
	else{
	    for(ctmp = opt_screen->top_line; ctmp && ctmp != newcp;
		i++, ctmp = next_confline(ctmp))
	      ;
	}

	if(i >= BODY_LINES(ps)){	/* new line is off screen */
	    /* move top line down this far */
	    i = i + 1 - BODY_LINES(ps);
	    for(ctmp = opt_screen->top_line;
		i > 0;
		i--, ctmp = next_confline(ctmp))
	      ;

	    opt_screen->top_line = ctmp;
	}

	*cl = newcp;

	ps->mangled_screen = 1;
	rv = 1;
	break;

      /* Delete an arbitrary header from this role */
      case MC_DELHDR :
	/*
	 * Find this one in earb list. We don't have a good way to locate
	 * it so we match the ea->v->name with ctmp->varname.
	 */
	rv = 0;
	eaprev = NULL;
	for(ea = *(*cl)->d.earb; ea; ea = ea->next){
	    if((*cl)->varnamep && (*cl)->varnamep->varname
	       && ea->v && ea->v->name
	       && !strncmp((*cl)->varnamep->varname,
			   ea->v->name, strlen(ea->v->name)))
	      break;
	    
	    eaprev = ea;
	}
	
	sprintf(tmp, "Really remove \"%.100s\" pattern from this rule",
		(ea && ea->a && ea->a->field) ? ea->a->field : "this");
	if(want_to(tmp, 'y', 'n', NO_HELP, WT_NORM) != 'y'){
	    cmd_cancelled("RemoveHdr");
	    return(rv);
	}

	/* delete the earb element from the list */
	if(ea){
	    if(eaprev)
	      eaprev->next = ea->next;
	    else
	      *(*cl)->d.earb = ea->next;
	    
	    ea->next = NULL;
	    free_earb(&ea);
	}

	/* remember start of deleted header */
	ctmp = (*cl && (*cl)->varnamep) ? (*cl)->varnamep : NULL;

	/* and end of deleted header */
	for(ctend = *cl; ctend; ctend = next_confline(ctend))
	  if(!ctend->next || ctend->next->varnamep != ctmp)
	    break;

	/* check if top line is one we're deleting */
	for(ctmpb = ctmp; ctmpb; ctmpb = next_confline(ctmpb)){
	    if(ctmpb == opt_screen->top_line)
	      break;
	    
	    if(ctmpb == (*cl))
	      break;
	}

	if(ctmpb == opt_screen->top_line)
	  opt_screen->top_line = ctend ? ctend->next : NULL;

	/* move current line after this header */
	*cl = ctend ? ctend->next : NULL;

	/* remove deleted header lines */
	if(ctmp && ctend){
	    /* remove from linked list */
	    if(ctmp->prev)
	      ctmp->prev->next = ctend->next;
	    
	    if(ctend->next)
	      ctend->next->prev = ctmp->prev;
	    
	    /* free memory */
	    ctmp->prev = ctend->next = NULL;
	    free_conflines(&ctmp);
	}
	
	ps->mangled_body = 1;
	rv = 1;
	break;

      default :
	if(((*cl)->var == scorei_pat_global_ptr
	    || (*cl)->var == age_pat_global_ptr
	    || (*cl)->var == size_pat_global_ptr
	    || (*cl)->var == cati_global_ptr)
	   &&
	   (cmd == MC_EDIT || (cmd == MC_ADD && !PVAL((*cl)->var, ew)))){
	    char prompt[60];

	    rv = 0;
	    sprintf(prompt, "%s the interval : ",
		    PVAL((*cl)->var, ew) ? "Change" : "Enter");

	    ps->mangled_footer = 1;
	    help = NO_HELP;
	    tmp[0] = '\0';
	    sprintf(tmp,
		    "%.200s", PVAL((*cl)->var, ew) ? PVAL((*cl)->var, ew) : "");
	    while(1){
		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(tmp, -FOOTER_ROWS(ps), 0, sizeof(tmp),
				     prompt, NULL, help, &oeflags);
		if(i == 0){
		    rv = ps->mangled_body = 1;
		    apval = APVAL((*cl)->var, ew);
		    if(apval && *apval)
		      fs_give((void **)apval);
		    
		    if(apval && tmp[0])
		      *apval = cpystr(tmp);

		    fix_side_effects(ps, (*cl)->var, 0);
		    if((*cl)->value)
		      fs_give((void **)&(*cl)->value);
		    
		    (*cl)->value = pretty_value(ps, *cl);
		}
		else if(i == 1)
		  cmd_cancelled(cmd == MC_ADD ? "Add" : "Change");
		else if(i == 3){
		    help = help == NO_HELP ? h_config_edit_scorei : NO_HELP;
		    continue;
		}
		else if(i == 4)
		  continue;

		break;
	    }
	}
	else{
	    if(cmd == MC_ADD && (*cl)->var && !(*cl)->var->is_list)
	      cmd = MC_EDIT;

	    rv = text_toolit(ps, cmd, cl, flags, 1);

	    /* make sure the earb pointers are set */
	    for(ctmp = (*cl)->varnamep;
		ctmp->next && ctmp->next->var == ctmp->var;
		ctmp = next_confline(ctmp))
	      ctmp->next->d.earb = ctmp->d.earb;
	}

	break;
    }

    /*
     * If the inherit nickname changed, we have to re-calculate the
     * global_vals and values for the action variables.
     * We may have to do the same if literal sig changed, too.
     */
    if(rv)
      calculate_inick_stuff(ps);

    return(rv);
}


/*
 */
int
role_text_tool_inick(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int   rv = -1;
    char **apval;

    switch(cmd){
      case MC_EXIT :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      case MC_CHOICE :		/* Choose a role nickname */
	{void (*prev_screen)() = ps->prev_screen,
	      (*redraw)() = ps->redrawer;
	 OPT_SCREEN_S *saved_screen;
	 ACTION_S     *role;

	ps->redrawer = NULL;
	ps->next_screen = SCREEN_FUN_NULL;

	saved_screen = opt_screen;
	if(role_select_screen(ps, &role, 0) == 0){
	    apval = APVAL((*cl)->var, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);
	    
	    if(apval)
	      *apval = (role && role->nick) ? cpystr(role->nick) : NULL;

	    if((*cl)->value)
	      fs_give((void **)&((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else{
	    ps->next_screen = prev_screen;
	    ps->redrawer = redraw;
	    rv = 0;
	}

	opt_screen = saved_screen;
	}
	/* fall through */

      case MC_EDIT :
      case MC_ADD :
      case MC_DELETE :
	if(cmd != MC_CHOICE)
	  rv = text_tool(ps, cmd, cl, flags);

	ps->mangled_screen = 1;
	break;

      default :
	rv = text_tool(ps, cmd, cl, flags);
	break;
    }

    /*
     * If the inherit nickname changed, we have to re-calculate the
     * global_vals and values for the action variables.
     * We may have to do the same if literal sig changed, too.
     */
    if(rv)
      calculate_inick_stuff(ps);

    return(rv);
}


/*
 */
int
role_text_tool_kword(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int    i, j, rv = -1;
    char **lval;

    switch(cmd){
      case MC_CHOICE :		/* Choose keywords from list and add them */
	{void (*prev_screen)() = ps->prev_screen,
	      (*redraw)() = ps->redrawer;
	 OPT_SCREEN_S *saved_screen;
	 char         *esc;
	 char        **kw;

	ps->redrawer = NULL;
	ps->next_screen = SCREEN_FUN_NULL;

	saved_screen = opt_screen;

	if(kw=choose_list_of_keywords()){
	    for(i = 0; kw[i]; i++){
		esc = add_roletake_escapes(kw[i]);
		fs_give((void **) &kw[i]);
		kw[i] = esc;
	    }

	    /* eliminate duplicates before the add */
	    lval = LVAL((*cl)->var, ew);
	    if(lval && *lval){
		for(i = 0; kw[i]; ){
		    /* if kw[i] is a dup, eliminate it */
		    for(j = 0; lval[j]; j++)
		      if(!strcmp(kw[i], lval[j]))
			break;

		    if(lval[j]){		/* it is a dup */
			for(j = i; kw[j]; j++)
			  kw[j] = kw[j+1];
		    }
		    else
		      i++;
		}
	    }

	    if(kw[0])
	      config_add_list(ps, cl, kw, NULL, 0);
	    
	    fs_give((void **) &kw);

	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else{
	    ps->next_screen = prev_screen;
	    ps->redrawer = redraw;
	    rv = 0;
	}

	opt_screen = saved_screen;
	}

	ps->mangled_screen = 1;
	break;

      case MC_EDIT :
      case MC_ADD :
      case MC_DELETE :
      case MC_NOT :
	rv = role_text_tool(ps, cmd, cl, flags);
	ps->mangled_screen = 1;
	break;

      case MC_EXIT :
      default :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


/*
 */
int
role_text_tool_charset(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int    i, j, rv = -1;
    char **lval;

    switch(cmd){
      case MC_CHOICE :		/* Choose charsets from list and add them */
	{void (*prev_screen)() = ps->prev_screen,
	      (*redraw)() = ps->redrawer;
	 OPT_SCREEN_S *saved_screen;
	 char         *esc;
	 char        **kw;

	ps->redrawer = NULL;
	ps->next_screen = SCREEN_FUN_NULL;

	saved_screen = opt_screen;

	if(kw=choose_list_of_charsets()){
	    for(i = 0; kw[i]; i++){
		esc = add_roletake_escapes(kw[i]);
		fs_give((void **) &kw[i]);
		kw[i] = esc;
	    }

	    /* eliminate duplicates before the add */
	    lval = LVAL((*cl)->var, ew);
	    if(lval && *lval){
		for(i = 0; kw[i]; ){
		    /* if kw[i] is a dup, eliminate it */
		    for(j = 0; lval[j]; j++)
		      if(!strcmp(kw[i], lval[j]))
			break;

		    if(lval[j]){		/* it is a dup */
			for(j = i; kw[j]; j++)
			  kw[j] = kw[j+1];
		    }
		    else
		      i++;
		}
	    }

	    if(kw[0])
	      config_add_list(ps, cl, kw, NULL, 0);
	    
	    fs_give((void **) &kw);

	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else{
	    ps->next_screen = prev_screen;
	    ps->redrawer = redraw;
	    rv = 0;
	}

	opt_screen = saved_screen;
	}

	ps->mangled_screen = 1;
	break;

      case MC_EDIT :
      case MC_ADD :
      case MC_DELETE :
      case MC_NOT :
	rv = role_text_tool(ps, cmd, cl, flags);
	ps->mangled_screen = 1;
	break;

      case MC_EXIT :
      default :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


/*
 * Choose an address book nickname
 */
int
role_text_tool_afrom(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int   rv = -1;
    char **apval;
    char  *abook;

    switch(cmd){
      case MC_EXIT :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      case MC_CHOICE :		/* Choose an addressbook */
	{void (*prev_screen)() = ps->prev_screen,
	      (*redraw)() = ps->redrawer;
	 OPT_SCREEN_S *saved_screen;
	 char *abook = NULL, *abookesc = NULL;

	ps->redrawer = NULL;
	ps->next_screen = SCREEN_FUN_NULL;
	saved_screen = opt_screen;

	abook = abook_select_screen(ps);
	if(abook){
	    abookesc = add_comma_escapes(abook);
	    fs_give((void**) &abook);
	}

	ps->mangled_screen = 1;
	if(abookesc && *abookesc && (*cl)->var && (*cl)->var->is_list){
	    char **ltmp;

	    ltmp     = (char **) fs_get(2 * sizeof(char *));
	    ltmp[0]  = abookesc;
	    ltmp[1]  = NULL;
	    abookesc = NULL;

	    if(ltmp && ltmp[0])
	      config_add_list(ps, cl, ltmp, NULL, 0);

	    if(ltmp)
	      fs_give((void **) &ltmp);
	    
	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	if(abookesc)
	  fs_give((void **) &abookesc);

	opt_screen = saved_screen;
	}

	ps->mangled_screen = 1;
	break;

      default :
	rv = text_tool(ps, cmd, cl, flags);
	ps->mangled_screen = 1;
	break;
    }

    return(rv);
}


/*
 * Args fmt -- a printf style fmt string with a single %s
 *      buf -- place to put result, assumed large enough (strlen(fmt)+11)
 *   rflags -- controls what goes in buf
 *
 * Returns -- pointer to buf
 */
char *
role_type_print(buf, fmt, rflags)
    char *buf;
    char *fmt;
    long  rflags;
{
#define CASE_MIXED	1
#define CASE_UPPER	2
#define CASE_LOWER	3
    int   cas = CASE_UPPER;
    int   prev_word_is_a = 0;
    char *q, *p;

    /* find %sRule to see what case */
    if((p = srchstr(fmt, "%srule")) != NULL){
	if(p[2] == 'R'){
	    if(p[3] == 'U')
	      cas = CASE_UPPER;
	    else
	      cas = CASE_MIXED;
	}
	else
	  cas = CASE_LOWER;
	
	if(p-3 >= fmt &&
	   p[-1] == SPACE &&
	   (p[-2] == 'a' || p[-2] == 'A')
	   && p[-3] == SPACE)
	  prev_word_is_a++;
    }

    if(cas == CASE_UPPER)
      q = (rflags & ROLE_DO_INCOLS) ? "INDEX COLOR " :
	   (rflags & ROLE_DO_FILTER) ? "FILTERING " :
	    (rflags & ROLE_DO_SCORES) ? "SCORING " :
	     (rflags & ROLE_DO_OTHER)  ? "OTHER " :
	      (rflags & ROLE_DO_ROLES)  ? "ROLE " : "";
    else if(cas == CASE_LOWER)
      q = (rflags & ROLE_DO_INCOLS) ? "index color " :
	   (rflags & ROLE_DO_FILTER) ? "filtering " :
	    (rflags & ROLE_DO_SCORES) ? "scoring " :
	     (rflags & ROLE_DO_OTHER) ? "other " :
	      (rflags & ROLE_DO_ROLES)  ? "role " : "";
    else
      q = (rflags & ROLE_DO_INCOLS) ? "Index Color " :
	   (rflags & ROLE_DO_FILTER) ? "Filtering " :
	    (rflags & ROLE_DO_SCORES) ? "Scoring " :
	     (rflags & ROLE_DO_OTHER) ? "Other " :
	      (rflags & ROLE_DO_ROLES)  ? "Role " : "";
    
    /* it ain't right to say "a index" */
    if(prev_word_is_a && !struncmp(q, "index", 5))
      q += 6;
      
    sprintf(buf, fmt, q);
    return(buf);
}



void
role_process_filters()
{
    int         i;
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	stream = ps_global->s_pool.streams[i];
	if(stream && pine_mail_ping(stream)){
	    msgmap = sp_msgmap(stream);
	    if(msgmap)
	      reprocess_filter_patterns(stream, msgmap, MI_REFILTERING);
	}
    }
}


/*
 * filter option list manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
feat_checkbox_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S	**cl;
    unsigned      flags;
{
    int  rv = 0;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark option */
	rv = 1;
	toggle_feat_option_bit(ps, (*cl)->varmem, (*cl)->var, (*cl)->value);
	break;

      case MC_EXIT:				 /* exit */
        rv = role_filt_exitcheck(cl, flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


void
toggle_feat_option_bit(ps, index, var, value)
    struct pine     *ps;
    int		     index;
    struct variable *var;
    char            *value;
{
    NAMEVAL_S  *f;

    f  = feat_feature_list(index);

    /* flip the bit */
    if(bitnset(f->value, feat_option_list))
      clrbitn(f->value, feat_option_list);
    else
      setbitn(f->value, feat_option_list);

    if(value)
      value[1] = bitnset(f->value, feat_option_list) ? 'X' : ' ';
}


NAMEVAL_S *
feat_feature_list(index)
    int index;
{
    static NAMEVAL_S opt_feat_list[] = {
	{"use-date-header-for-age",        NULL, FEAT_SENTDATE},
	{"move-only-if-not-deleted",       NULL, FEAT_IFNOTDEL},
	{"dont-stop-even-if-rule-matches", NULL, FEAT_NONTERM}
    };

    return((index >= 0 &&
	    index < (sizeof(opt_feat_list)/sizeof(opt_feat_list[0])))
		   ? &opt_feat_list[index] : NULL);
}


void
color_config_screen(ps, edit_exceptions)
    struct pine *ps;
    int          edit_exceptions;
{
    CONF_S	   *ctmp = NULL, *first_line = NULL;
    SAVED_CONFIG_S *vsave;
    OPT_SCREEN_S    screen;
    int             readonly_warning = 0;

    ps->next_screen = SCREEN_FUN_NULL;

    mailcap_free(); /* free resources we won't be using for a while */

    if(ps->fix_fixed_warning)
      offer_to_fix_pinerc(ps);

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return;
	}
    }

    color_config_init_display(ps, &ctmp, &first_line);

    vsave = save_color_config_vars(ps);

    memset(&screen, 0, sizeof(screen));
    screen.deferred_ro_warning = readonly_warning;
    switch(conf_scroll_screen(ps, &screen, first_line,
			      edit_exceptions ? "SETUP COLOR EXCEPTIONS"
					      : "SETUP COLOR",
			      "configuration ", 0)){
      case 0:
	break;

      case 1:
	write_pinerc(ps, ew, WRP_NONE);
	break;
    
      case 10:
	revert_to_saved_color_config(ps, vsave);
	break;
      
      default:
	q_status_message(SM_ORDER, 7, 10,
			 "conf_scroll_screen bad ret in color_config");
	break;
    }

    free_saved_color_config(ps, &vsave);

#ifdef _WINDOWS
    mswin_set_quit_confirm (F_OFF(F_QUIT_WO_CONFIRM, ps_global));
#endif
}


void
color_config_init_display(ps, ctmp, first_line)
    struct pine *ps;
    CONF_S     **ctmp,
	       **first_line;
{
    char	  **lval;
    int		    i, saw_first_index = 0;
    struct	    variable  *vtmp;

#ifndef	_WINDOWS
    vtmp = &ps->vars[V_COLOR_STYLE];

    new_confline(ctmp);
    (*ctmp)->flags       |= (CF_NOSELECT | CF_STARTITEM);
    (*ctmp)->keymenu      = &config_radiobutton_keymenu;
    (*ctmp)->tool	  = NULL;
    (*ctmp)->varname	  = cpystr("Color Style");
    (*ctmp)->varnamep	  = *ctmp;

    standard_radio_setup(ps, ctmp, vtmp, first_line);

    new_confline(ctmp);
    /* Blank line */
    (*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);

    if(!pico_usingcolor()){
	/* add a line explaining that color is not turned on */
	new_confline(ctmp);
	(*ctmp)->help			 = NO_HELP;
	(*ctmp)->flags			|= CF_NOSELECT;
	(*ctmp)->value = cpystr(COLORNOSET);

	new_confline(ctmp);
	/* Blank line */
	(*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);
    }

#endif

    vtmp = &ps->vars[V_INDEX_COLOR_STYLE];

    new_confline(ctmp);
    (*ctmp)->flags       |= (CF_NOSELECT | CF_STARTITEM);
    (*ctmp)->keymenu      = &config_radiobutton_keymenu;
    (*ctmp)->tool	  = NULL;
    (*ctmp)->varname	  = cpystr("Current Indexline Style");
    (*ctmp)->varnamep	  = *ctmp;

    standard_radio_setup(ps, ctmp, vtmp, NULL);

    vtmp = &ps->vars[V_TITLEBAR_COLOR_STYLE];

    new_confline(ctmp);
    (*ctmp)->flags       |= (CF_NOSELECT | CF_STARTITEM);
    (*ctmp)->keymenu      = &config_radiobutton_keymenu;
    (*ctmp)->tool	  = NULL;
    (*ctmp)->varname	  = cpystr("Titlebar Color Style");
    (*ctmp)->varnamep	  = *ctmp;

    standard_radio_setup(ps, ctmp, vtmp, NULL);

    new_confline(ctmp);
    /* Blank line */
    (*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);

    new_confline(ctmp);
    /* title before general colors */
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr("----------------");
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr("GENERAL COLORS");
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr("----------------");

    for(vtmp = ps->vars; vtmp->name; vtmp++){
	if(!color_holding_var(ps, vtmp))
	  continue;

	/* If not foreground, skip it */
	if(!srchstr(vtmp->name, "-foreground-color"))
	  continue;

	if(!saw_first_index && !struncmp(vtmp->name, "index-", 6)){
	    saw_first_index++;
	    new_confline(ctmp);		/* Blank line */
	    (*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);
	    new_confline(ctmp);
	    (*ctmp)->help			 = NO_HELP;
	    (*ctmp)->flags			|= CF_NOSELECT;
	    (*ctmp)->value = cpystr("--------------");
	    new_confline(ctmp);
	    (*ctmp)->help			 = NO_HELP;
	    (*ctmp)->flags			|= CF_NOSELECT;
	    (*ctmp)->value = cpystr("INDEX COLORS");
	    new_confline(ctmp);
	    (*ctmp)->help			 = NO_HELP;
	    (*ctmp)->flags			|= CF_NOSELECT;
	    (*ctmp)->value = cpystr("--------------");
	}

	new_confline(ctmp);
	/* Blank line */
	(*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;

	new_confline(ctmp)->var = vtmp;
	if(first_line && !*first_line)
	  *first_line = *ctmp;

	(*ctmp)->varnamep		 = *ctmp;
	(*ctmp)->keymenu		 = &color_setting_keymenu;
	(*ctmp)->help		 	 = config_help(vtmp - ps->vars, 0);
	(*ctmp)->tool			 = color_setting_tool;
	(*ctmp)->flags |= (CF_STARTITEM | CF_COLORSAMPLE | CF_POT_SLCTBL);
	if(!pico_usingcolor())
	  (*ctmp)->flags |= CF_NOSELECT;

	(*ctmp)->value			 = pretty_value(ps, *ctmp);
	(*ctmp)->valoffset		 = COLOR_INDENT;
    }

    /*
     * custom header colors
     */
    new_confline(ctmp);		/* Blank line */
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr("----------------");
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr("HEADER COLORS");
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr("----------------");

    vtmp = &ps->vars[V_VIEW_HDR_COLORS];
    lval = LVAL(vtmp, ew);

    if(lval && lval[0] && lval[0][0]){
	for(i = 0; lval && lval[i]; i++)
	  add_header_color_line(ps, ctmp, lval[i], i);
    }
    else{
	new_confline(ctmp);		/* Blank line */
	(*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;
	new_confline(ctmp);
	(*ctmp)->help			 = NO_HELP;
	(*ctmp)->flags			|= CF_NOSELECT;
	(*ctmp)->value = cpystr(ADDHEADER_COMMENT);
	(*ctmp)->valoffset		 = COLOR_INDENT;
    }


    /*
     * custom keyword colors
     */
    new_confline(ctmp);		/* Blank line */
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr("----------------");
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr(KW_COLORS_HDR);
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr("----------------");


    if(ps->keywords){
	KEYWORD_S *kw;
	char      *name, *comment;
	int        j, lv = 0, lc = 0, ltot = 0, eq_col = EQ_COL;

	vtmp = &ps->vars[V_KW_COLORS];

	/* first figure out widths for display */
	for(kw = ps->keywords; kw; kw = kw->next){
	    i = 0;
	    if(lv < (i = strlen(kw->nick ? kw->nick : kw->kw ? kw->kw : "")))
	      lv = i;

	    j = 0;
	    if(kw->nick && kw->kw && kw->kw[0]){
		if(lc < (j = strlen(kw->kw) + 2))
		  lc = j;
	    }

	    if(ltot < (i + (j > 2 ? j : 0)))
	      ltot = (i + (j > 2 ? j : 0));
	}

	lv = min(lv, 100);
	lc = min(lc, 100);
	ltot = min(ltot, 100);

	/*
	 * SC is length of " Color"
	 * SS is space between nickname and keyword value
	 * SW is space between words and Sample
	 */
#define SC 6
#define SS 1
#define SW 3
	if(COLOR_INDENT + SC + ltot + (lc>0?SS:0) > eq_col - SW){
	  eq_col = min(max(ps->ttyo->screen_cols - 10, 20),
		       COLOR_INDENT + SC + ltot + (lc>0?SS:0) + SW);
	  if(COLOR_INDENT + SC + ltot + (lc>0?SS:0) > eq_col - SW){
	    eq_col = min(max(ps->ttyo->screen_cols - 10, 20),
		         COLOR_INDENT + SC + lv + (lc>0?SS:0) + lc + SW);
	    if(COLOR_INDENT + SC + lv + (lc>0?SS:0) + lc > eq_col - SW){
	      lc = min(lc, max(eq_col - SW - COLOR_INDENT - SC - lv - SS, 7));
	      if(COLOR_INDENT + SC + lv + (lc>0?SS:0) + lc > eq_col - SW){
	        lc = 0;
	        if(COLOR_INDENT + SC + lv > eq_col - SW){
		  lv = max(eq_col - SW - COLOR_INDENT - SC, 7);
		}
	      }
	    }
	  }
	}

	lval = LVAL(vtmp, ew);
	if(lval && lval[0] && !strcmp(lval[0], INHERIT)){
	    new_confline(ctmp);
	    (*ctmp)->flags		|= CF_NOSELECT | CF_B_LINE;

	    new_confline(ctmp)->var	 = vtmp;
	    (*ctmp)->varnamep		 = *ctmp;
	    (*ctmp)->keymenu		 = &kw_color_setting_keymenu;
	    (*ctmp)->help		 = config_help(vtmp - ps->vars, 0);
	    (*ctmp)->tool		 = color_setting_tool;
	    (*ctmp)->varmem		 = CFC_SET_COLOR(i, 0);
	    (*ctmp)->valoffset		 = COLOR_INDENT;

	    (*ctmp)->flags = (CF_NOSELECT | CF_INHERIT);
	}

	/* now create the config lines */
	for(kw = ps->keywords, i = 0; kw; kw = kw->next, i++){
	    char tmp[300];

	    /* Blank line */
	    new_confline(ctmp);
	    (*ctmp)->flags		|= CF_NOSELECT | CF_B_LINE;

	    new_confline(ctmp)->var	 = vtmp;
	    (*ctmp)->varnamep		 = *ctmp;
	    (*ctmp)->keymenu		 = &kw_color_setting_keymenu;
	    (*ctmp)->help		 = config_help(vtmp - ps->vars, 0);
	    (*ctmp)->tool		 = color_setting_tool;
	    (*ctmp)->flags |= (CF_STARTITEM | CF_COLORSAMPLE | CF_POT_SLCTBL);
	    if(!pico_usingcolor())
	      (*ctmp)->flags |= CF_NOSELECT;

	    /*
	     * Not actually a color in this case, it is an index into
	     * the keywords list.
	     */
	    (*ctmp)->varmem		 = CFC_SET_COLOR(i, 0);

	    name = short_str(kw->nick ? kw->nick : kw->kw ? kw->kw : "",
			     tmp_20k_buf+10000, lv, EndDots);
	    if(lc > 0 && kw->nick && kw->kw && kw->kw[0])
	      comment = short_str(kw->kw, tmp_20k_buf+11000, lc, EndDots);
	    else
	      comment = NULL;

	    sprintf(tmp, "%.*s%*s%.1s%.*s%.1s Color%*s %.30s%.30s",
		    lv, name,
		    (lc > 0 && comment) ? SS : 0,
		    "",
		    (lc > 0 && comment) ? "(" : "",
		    (lc > 0 && comment) ? lc : 0,
		    (lc > 0 && comment) ? comment : "",
		    (lc > 0 && comment) ? ")" : "",
		    max(min(eq_col - COLOR_INDENT - min(lv,strlen(name))
			- SC - 1
			- ((lc > 0 && comment)
			    ? (SS+2+min(lc,strlen(comment))) : 0), 100), 0),
		    "",
		    sample_text(ps,vtmp),
		    color_parenthetical(vtmp));

	    (*ctmp)->value		 = cpystr(tmp);
	    (*ctmp)->valoffset		 = COLOR_INDENT;
	}
    }
    else{
	new_confline(ctmp);		/* Blank line */
	(*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;
	new_confline(ctmp);
	(*ctmp)->help			 = NO_HELP;
	(*ctmp)->flags			|= CF_NOSELECT;
	(*ctmp)->value = cpystr("[ Use Setup/Config command to add Keywords ]");
	(*ctmp)->valoffset		 = COLOR_INDENT;
    }
}


char *
color_parenthetical(var)
    struct variable *var;
{
    int    norm, exc, exc_inh;
    char **lval, *ret = "";

    if(var == &ps_global->vars[V_VIEW_HDR_COLORS]
       || var == &ps_global->vars[V_KW_COLORS]){
	norm    = (LVAL(var, Main) != NULL);
	exc     = (LVAL(var, ps_global->ew_for_except_vars) != NULL);
	exc_inh = ((lval=LVAL(var, ps_global->ew_for_except_vars)) &&
		   lval[0] && !strcmp(INHERIT, lval[0]));

	/* editing normal but there is an exception config */
	if((ps_global->ew_for_except_vars != Main) && (ew == Main)){
	    if((exc && !exc_inh))
	      ret = " (overridden by exceptions)";
	    else if(exc && exc_inh)
	      ret = " (more in exceptions)";
	}
	/* editing exception config */
	else if((ps_global->ew_for_except_vars != Main) &&
		(ew == ps_global->ew_for_except_vars)){
	    if(exc && exc_inh && norm)
	      ret = " (more in main config)";
	}
    }

    return(ret);
}


void
add_header_color_line(ps, ctmp, val, which)
    struct pine *ps;
    CONF_S     **ctmp;
    char        *val;
    int          which;
{
    struct variable *vtmp;
    SPEC_COLOR_S     *hc;
    char	     tmp[100+1];
    int              l;

    vtmp = &ps->vars[V_VIEW_HDR_COLORS];
    l = strlen(HEADER_WORD);

    /* Blank line */
    new_confline(ctmp);
    (*ctmp)->flags		|= CF_NOSELECT | CF_B_LINE;

    new_confline(ctmp)->var	 = vtmp;
    (*ctmp)->varnamep		 = *ctmp;
    (*ctmp)->keymenu		 = &custom_color_setting_keymenu;
    (*ctmp)->help		 = config_help(vtmp - ps->vars, 0);
    (*ctmp)->tool		 = color_setting_tool;
    (*ctmp)->flags |= (CF_STARTITEM | CF_COLORSAMPLE | CF_POT_SLCTBL);
    if(!pico_usingcolor())
      (*ctmp)->flags |= CF_NOSELECT;

    /* which is an index into the variable list */
    (*ctmp)->varmem		 = CFC_SET_COLOR(which, 0);

    hc = spec_color_from_var(val, 0);
    if(hc && hc->inherit)
      (*ctmp)->flags = (CF_NOSELECT | CF_INHERIT);
    else{
	sprintf(tmp, "%s%c%.*s Color%*s %.30s%.30s",
		HEADER_WORD,
		(hc && hc->spec) ? (islower((unsigned char)hc->spec[0])
					    ? toupper((unsigned char)hc->spec[0])
					    : hc->spec[0]) : '?',
		min(strlen((hc && hc->spec && hc->spec[0]) ? hc->spec+1 : ""),30-l),
		(hc && hc->spec && hc->spec[0]) ? hc->spec+1 : "",
		max(EQ_COL - COLOR_INDENT -1 -
		   min(strlen((hc && hc->spec && hc->spec[0]) ? hc->spec+1 : ""),30-l)
			    - l - 6 - 1, 0), "",
		sample_text(ps,vtmp),
		color_parenthetical(vtmp));
	(*ctmp)->value		 = cpystr(tmp);
    }

    (*ctmp)->valoffset		 = COLOR_INDENT;

    if(hc)
      free_spec_colors(&hc);
}


/*
 * Set up the standard color setting display for one color.
 *
 * Args   fg -- the current foreground color
 *        bg -- the current background color
 *       def -- default box should be checked
 */
void
add_color_setting_disp(ps, ctmp, var, varnamep, km, cb_km, help, indent, which,
		       fg, bg, def)
    struct pine     *ps;
    CONF_S         **ctmp;
    struct variable *var;
    CONF_S	    *varnamep;
    struct key_menu *km;
    struct key_menu *cb_km;
    HelpType         help;
    int              indent;
    int              which;
    char            *fg;
    char            *bg;
    int              def;
{
    int             i, lv, count;
    char	    tmp[100+1];
    char           *title   = "HELP FOR SETTING UP COLOR";
    char           *pvalfg, *pvalbg;


    /* find longest value's name */
    count = pico_count_in_color_table();
    lv = COLOR_BLOB_LEN;

    /* put a title before list */
    new_confline(ctmp);
    (*ctmp)->varnamep		 = varnamep;
    (*ctmp)->keymenu		 = km;
    (*ctmp)->help		 = NO_HELP;
    (*ctmp)->tool		 = color_setting_tool;
    (*ctmp)->valoffset		 = indent;
    (*ctmp)->flags		|= CF_NOSELECT;
    (*ctmp)->varmem		 = 0;
    (*ctmp)->value = cpystr("Foreground     Background");

    new_confline(ctmp)->var	 = var;
    (*ctmp)->varnamep		 = varnamep;
    (*ctmp)->keymenu		 = km;
    (*ctmp)->help		 = NO_HELP;
    (*ctmp)->tool		 = color_setting_tool;
    (*ctmp)->valoffset		 = indent;
    (*ctmp)->flags		|= (CF_COLORSAMPLE | CF_NOSELECT);
    (*ctmp)->varmem		 = CFC_SET_COLOR(which, 0);
    (*ctmp)->value		 = color_setting_text_line(ps, var);

    for(i = 0; i < count; i++){
	new_confline(ctmp)->var	 = var;
	(*ctmp)->varnamep	 = varnamep;
	(*ctmp)->keymenu	 = km;
	(*ctmp)->help		 = help;
	(*ctmp)->help_title	 = title;
	(*ctmp)->tool		 = color_setting_tool;
	(*ctmp)->valoffset	 = indent;
	/* 5 is length of "( )  " */
	(*ctmp)->val2offset	 = indent + lv + 5 + SPACE_BETWEEN_DOUBLEVARS;
	(*ctmp)->flags		|= CF_DOUBLEVAR;
	(*ctmp)->varmem		 = CFC_SET_COLOR(which, i);
	/*
	 * Special case: The 2nd and 3rd arguments here have the count == 8
	 * special case in them. See pico/osdep/unix init_color_table().
	 */
	(*ctmp)->value		 = new_color_line(COLOR_BLOB,
						  fg &&
			   !strucmp(color_to_canonical_name(fg), colorx(i)),
						  bg &&
			   !strucmp(color_to_canonical_name(bg), colorx(i)),
						  lv);
    }

#ifdef	_WINDOWS
    new_confline(ctmp)->var  = var;
    (*ctmp)->varnamep	     = varnamep;
    (*ctmp)->keymenu	     = (km == &custom_color_changing_keymenu)
				 ? &custom_rgb_keymenu :
				 (km == &kw_color_changing_keymenu)
				   ? &kw_rgb_keymenu : &color_rgb_keymenu;
    (*ctmp)->help	     = help;
    (*ctmp)->help_title	     = title;
    (*ctmp)->tool	     = color_setting_tool;
    (*ctmp)->valoffset	     = indent;
    /* 5 is length of "( )  " */
    (*ctmp)->val2offset	     = indent + lv + 5 + SPACE_BETWEEN_DOUBLEVARS;
    (*ctmp)->flags	    |= CF_DOUBLEVAR;
    (*ctmp)->varmem	     = CFC_SET_COLOR(which, i);
    (*ctmp)->value	     = new_color_line("Custom",
					      (fg && is_rgb_color(fg)),
					      (bg && is_rgb_color(bg)),
					      lv);
#endif

    new_confline(ctmp)->var	= var;
    (*ctmp)->varnamep		= varnamep;
    (*ctmp)->keymenu		= cb_km;
    (*ctmp)->help		= h_config_dflt_color;
    (*ctmp)->help_title		= title;
    (*ctmp)->tool		= color_setting_tool;
    (*ctmp)->valoffset		= indent;
    (*ctmp)->varmem		= CFC_SET_COLOR(which, 0);
#ifdef	_WINDOWS
    sprintf(tmp, "[%c]  %s", def ? 'X' : ' ', "Default");
#else
    pvalfg = PVAL(var,Main);
    pvalbg = PVAL(var+1,Main);
    if(!var->is_list &&
       ((var == &ps->vars[V_NORM_FORE_COLOR]) ||
        (ew == Post && pvalfg && pvalfg[0] && pvalbg && pvalbg[0]) ||
        (var->global_val.p && var->global_val.p[0] &&
         (var+1)->global_val.p && (var+1)->global_val.p[0])))
      sprintf(tmp, "[%c]  %s", def ? 'X' : ' ', "Default");
    else if(var == &ps->vars[V_REV_FORE_COLOR])
      sprintf(tmp, "[%c]  %s", def ? 'X' : ' ',
	  "Default (terminal's standout mode, usually reverse video)");
    else if(var == &ps->vars[V_SLCTBL_FORE_COLOR])
      sprintf(tmp, "[%c]  %s", def ? 'X' : ' ',
	  "Default (Bold Normal Color)");
    else if(var_defaults_to_rev(var))
      sprintf(tmp, "[%c]  %s", def ? 'X' : ' ',
	  "Default (same as Reverse Color)");
    else if(km == &kw_color_changing_keymenu)
      sprintf(tmp, "[%c]  %s", def ? 'X' : ' ',
	  "Default (same as Indexline Color)");
    else
      sprintf(tmp, "[%c]  %s", def ? 'X' : ' ',
	  "Default (same as Normal Color)");
#endif
    (*ctmp)->value		= cpystr(tmp);

    /*
     * Add a checkbox to turn bold on or off for selectable-item color.
     */
    if(var == &ps->vars[V_SLCTBL_FORE_COLOR]){
	char     **lval;

	new_confline(ctmp)->var	= var;
	(*ctmp)->varnamep	= varnamep;
	(*ctmp)->keymenu	= &selectable_bold_checkbox_keymenu;
	(*ctmp)->help		= h_config_bold_slctbl;
	(*ctmp)->help_title	= title;
	(*ctmp)->tool		= color_setting_tool;
	(*ctmp)->valoffset	= indent;
	(*ctmp)->varmem		= feature_list_index(F_SLCTBL_ITEM_NOBOLD);

	lval = LVAL(&ps->vars[V_FEATURE_LIST], ew);
	/*
	 * We don't use checkbox_pretty_value here because we just want
	 * the word Bold instead of the name of the variable and because
	 * we are actually using the negative of the feature. That is,
	 * the feature is really NOBOLD and we are using Bold.
	 */
	sprintf(tmp, "[%c]  %s",
		test_feature(lval, feature_list_name(F_SLCTBL_ITEM_NOBOLD), 0)
		    ? ' ' : 'X', "Bold");

	(*ctmp)->value		= cpystr(tmp);
    }
}


int
is_rgb_color(color)
    char *color;
{
    int i, j;

    for(i = 0; i < 3; i++){
	if(i && *color++ != ',')
	  return(FALSE);

	for(j = 0; j < 3; j++, color++)
	  if(!isdigit((unsigned char) *color))
	    return(FALSE);
    }

    return(TRUE);
}


char *
new_color_line(color, fg, bg, len)
    char *color;
    int	  fg, bg, len;
{
    char tmp[256];

    sprintf(tmp, "(%c)  %-*.*s%*s(%c)  %-*.*s",
	    fg ? R_SELD : ' ', len, len, color, SPACE_BETWEEN_DOUBLEVARS, "",
	    bg ? R_SELD : ' ', len, len, color);
    return(cpystr(tmp));
}


int
color_text_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int		  cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int             rv = 0, i;
    struct variable v, *save_var;
    SPEC_COLOR_S    *hc, *hcolors;
    char           *starting_val, *val, tmp[100], ***alval, **apval;

    if(cmd == MC_EXIT)
      return(simple_exit_cmd(flags));

    alval = ALVAL((*cl)->var, ew);
    if(!alval || !*alval)
      return(rv);

    hcolors = spec_colors_from_varlist(*alval, 0);

    for(hc = hcolors, i=0; hc; hc = hc->next, i++)
      if(CFC_ICUST(*cl) == i)
	break;

    starting_val = (hc && hc->val) ? pattern_to_string(hc->val) : NULL;

    memset(&v, 0, sizeof(v));
    v.is_used    = 1;
    v.is_user    = 1;
    sprintf(tmp, "\"%c%.30s Pattern\"",
	    islower((unsigned char)hc->spec[0])
					? toupper((unsigned char)hc->spec[0])
					: hc->spec[0],
	    hc->spec[1] ? hc->spec + 1 : "");
    v.name       = tmp;
    /* have to use right part of v so text_tool will work right */
    apval = APVAL(&v, ew);
    *apval = starting_val ? cpystr(starting_val) : NULL;
    set_current_val(&v, FALSE, FALSE);

    save_var = (*cl)->var;
    (*cl)->var = &v;
    rv = text_tool(ps, cmd, cl, flags);

    if(rv == 1){
	val = *apval;
	*apval = NULL;
	if(val)
	  removing_leading_and_trailing_white_space(val);
	
	if(hc->val)
	  fs_give((void **)&hc->val);
	
	hc->val = string_to_pattern(val);

	(*cl)->var = save_var;

	if((*alval)[CFC_ICUST(*cl)])
	  fs_give((void **)&(*alval)[CFC_ICUST(*cl)]);

	(*alval)[CFC_ICUST(*cl)] = var_from_spec_color(hc);
	set_current_color_vals(ps);
	ps->mangled_screen = 1;
    }
    else
      (*cl)->var = save_var;
    
    if(hcolors)
      free_spec_colors(&hcolors);
    if(*apval)
      fs_give((void **)apval);
    if(v.current_val.p)
      fs_give((void **)&v.current_val.p);
    if(starting_val)
      fs_give((void **)&starting_val);

    return(rv);
}


/*
 * Test whether or not a var is one of the vars which might have a
 * color value stored in it.
 *
 * returns:  1 if it is a color var, 0 otherwise
 */
int
color_holding_var(ps, var)
    struct pine     *ps;
    struct variable *var;
{
    if(treat_color_vars_as_text)
      return(0);
    else
      return(var && var->name &&
	     (srchstr(var->name, "-foreground-color") ||
	      srchstr(var->name, "-background-color") ||
	      var == &ps->vars[V_VIEW_HDR_COLORS] ||
	      var == &ps->vars[V_KW_COLORS]));
}


/*
 * test whether or not a var is one of the vars having to do with color
 *
 * returns:  1 if it is a color var, 0 otherwise
 */
int
color_related_var(ps, var)
    struct variable *var;
    struct pine     *ps;
{
    return(
#ifndef	_WINDOWS
	   var == &ps->vars[V_COLOR_STYLE] ||
#endif
           var == &ps->vars[V_INDEX_COLOR_STYLE] ||
           var == &ps->vars[V_TITLEBAR_COLOR_STYLE] ||
	   color_holding_var(ps, var));
}


int
color_setting_tool(ps, cmd, cl, flags)
    struct pine  *ps;
    int	          cmd;
    CONF_S      **cl;
    unsigned      flags;
{
    int	             rv = 0, i, cancel = 0, deefault;
    int              curcolor, prevcolor, nextcolor, another;
    CONF_S          *ctmp, *first_line, *beg = NULL, *end = NULL,
		    *cur_beg, *cur_end, *prev_beg, *prev_end,
		    *next_beg, *next_end;
    struct variable *v, *fgv, *bgv, *setv = NULL, *otherv;
    SPEC_COLOR_S    *hc = NULL, *new_hcolor;
    SPEC_COLOR_S    *hcolors = NULL;
    KEYWORD_S       *kw;
    char            *old_val, *confline = NULL;
    char             prompt[100], sval[MAXPATH+1];
    char           **lval, **apval, ***alval, **t;
    char           **apval1, **apval2;
    HelpType         help;
    ESCKEY_S         opts[3];
#ifdef	_WINDOWS
    char            *pval;
#endif

    switch(cmd){
      case MC_CHOICE :				/* set a color */

	if(((*cl)->flags & CF_VAR2 && fixed_var((*cl)->var+1, NULL, NULL)) ||
	   (!((*cl)->flags & CF_VAR2) && fixed_var((*cl)->var, NULL, NULL))){
	    if(((*cl)->var->post_user_val.p ||
	        ((*cl)->var+1)->post_user_val.p ||
	        (*cl)->var->main_user_val.p ||
		((*cl)->var+1)->main_user_val.p)
	       && want_to("Delete old unused personal option setting",
			  'y', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		delete_user_vals((*cl)->var);
		delete_user_vals((*cl)->var+1);
		q_status_message(SM_ORDER, 0, 3, "Deleted");
		rv = 1;
	    }

	    return(rv);
	}

	fgv = (*cl)->var;				/* foreground color */
	bgv = (*cl)->var+1;				/* background color */
	v = ((*cl)->flags & CF_VAR2) ? bgv : fgv;	/* var being changed */

	apval = APVAL(v, ew);
	old_val = apval ? *apval : NULL;

	if(apval){
	    if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
	      *apval = cpystr(colorx(CFC_ICOLOR(*cl)));
	    else if(old_val)
	      *apval = cpystr(is_rgb_color(old_val)
			       ? old_val : color_to_asciirgb(old_val));
	    else if(v->current_val.p)
	      *apval = cpystr(is_rgb_color(v->current_val.p)
				       ? v->current_val.p
				       : color_to_asciirgb(v->current_val.p));
	    else if(v == fgv)
	      *apval = cpystr(color_to_asciirgb(colorx(COL_BLACK)));
	    else
	      *apval = cpystr(color_to_asciirgb(colorx(COL_WHITE)));
	}

	if(old_val)
	  fs_give((void **)&old_val);

	set_current_val(v, TRUE, FALSE);

	/*
	 * If the user sets one of foreground/background and the
	 * other is not yet set, set the other.
	 */
	if(PVAL(v, ew)){
	    if(v == fgv && !PVAL(bgv, ew)){
		setv   = bgv;
		otherv = fgv;
	    }
	    else if(v == bgv && !PVAL(fgv, ew)){
		setv   = fgv;
		otherv = bgv;
	    }
	}
	
	if(setv){
	    if(apval = APVAL(setv, ew)){
		if(setv->current_val.p)
		  *apval = cpystr(setv->current_val.p);
		else if (setv == fgv && ps_global->VAR_NORM_FORE_COLOR)
		  *apval = cpystr(ps_global->VAR_NORM_FORE_COLOR);
		else if (setv == bgv && ps_global->VAR_NORM_BACK_COLOR)
		  *apval = cpystr(ps_global->VAR_NORM_BACK_COLOR);
		else if(!strucmp(color_to_canonical_name(otherv->current_val.p),
				 colorx(COL_WHITE)))
		  *apval = cpystr(colorx(COL_BLACK));
		else
		  *apval = cpystr(colorx(COL_WHITE));
	    }

	    set_current_val(setv, TRUE, FALSE);
	}

	fix_side_effects(ps, v, 0);
	set_current_color_vals(ps);

	/*
	 * Turn on selected *'s for default selections, if any, and
	 * for ones we forced on.
	 */
	color_update_selected(ps, *cl, PVAL(fgv, ew), PVAL(bgv, ew), TRUE);

	ClearScreen();
	rv = ps->mangled_screen = 1;
	break;

      case MC_CHOICEB :				/* set a custom hdr color */
	/*
	 * Find the SPEC_COLOR_S for header.
	 */
	lval = LVAL((*cl)->var, ew);
	hcolors = spec_colors_from_varlist(lval, 0);
	for(hc = hcolors, i=0; hc; hc = hc->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(hc){
	    if((*cl)->flags & CF_VAR2){
		old_val = hc->bg;
		if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
		  hc->bg = cpystr(colorx(CFC_ICOLOR(*cl)));
		else if(old_val)
		  hc->bg = cpystr(is_rgb_color(old_val)
				    ? old_val
				    : color_to_asciirgb(old_val));
		else
		  hc->bg = cpystr(color_to_asciirgb(colorx(COL_WHITE)));

		if(old_val)
		  fs_give((void **) &old_val);

		/*
		 * If the user sets one of foreground/background and the
		 * other is not yet set, set it.
		 */
		if(!(hc->fg && hc->fg[0])){
		    if(hc->fg)
		      fs_give((void **)&hc->fg);

		    hc->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
		}
	    }
	    else{
		old_val = hc->fg;

		if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
		  hc->fg = cpystr(colorx(CFC_ICOLOR(*cl)));
		else if(old_val)
		  hc->fg = cpystr(is_rgb_color(old_val)
				    ? old_val
				    : color_to_asciirgb(old_val));
		else
		  hc->fg = cpystr(color_to_asciirgb(colorx(COL_BLACK)));

		if(old_val)
		  fs_give((void **) &old_val);

		if(!(hc->bg && hc->bg[0])){
		    if(hc->bg)
		      fs_give((void **)&hc->bg);

		    hc->bg = cpystr(ps->VAR_NORM_BACK_COLOR);
		}
	    }
	}

	/*
	 * Turn on selected *'s for default selections, if any, and
	 * for ones we forced on.
	 */
	color_update_selected(ps, *cl, 
			      (hc && hc->fg && hc->fg[0]
			       && hc->bg && hc->bg[0])
				  ? hc->fg : ps->VAR_NORM_FORE_COLOR,
			      (hc && hc->fg && hc->fg[0]
			       && hc->bg && hc->bg[0])
				  ? hc->bg : ps->VAR_NORM_BACK_COLOR,
			      TRUE);

	if(hc && lval && lval[i]){
	    fs_give((void **)&lval[i]);
	    lval[i] = var_from_spec_color(hc);
	}
	
	if(hcolors)
	  free_spec_colors(&hcolors);

	set_current_color_vals(ps);
	ClearScreen();
	rv = ps->mangled_screen = 1;
	break;


      case MC_CHOICEC :			/* set a custom keyword color */
	/* find keyword associated with color we are editing */
	for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(!kw){					/* can't happen */
	    dprint(1,(debugfile,
		"This can't happen, kw not set when setting keyword color\n"));
	    break;
	}

	hcolors = spec_colors_from_varlist(LVAL((*cl)->var, ew), 0);

	/*
	 * Look through hcolors, derived from lval, to find this keyword
	 * and its current color.
	 */
	for(hc = hcolors; hc; hc = hc->next)
	  if(hc->spec && ((kw->nick && !strucmp(kw->nick, hc->spec))
			  || (kw->kw && !strucmp(kw->kw, hc->spec))))
	    break;

	if(!hc){	/* this keyword didn't have a color set, add to list */
	    SPEC_COLOR_S *new;

	    new = (SPEC_COLOR_S *) fs_get(sizeof(*hc));
	    memset((void *) new, 0, sizeof(*new));
	    new->spec = cpystr(kw->kw);
	    new->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
	    new->bg = cpystr(ps->VAR_NORM_BACK_COLOR);

	    if(hcolors){
		for(hc = hcolors; hc->next; hc = hc->next)
		  ;

		hc->next = new;
	    }
	    else
	      hcolors = new;

	    hc = new;
	}

	if(hc){
	    if((*cl)->flags & CF_VAR2){
		old_val = hc->bg;
		if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
		  hc->bg = cpystr(colorx(CFC_ICOLOR(*cl)));
		else if(old_val)
		  hc->bg = cpystr(is_rgb_color(old_val)
				    ? old_val
				    : color_to_asciirgb(old_val));
		else
		  hc->bg = cpystr(color_to_asciirgb(colorx(COL_WHITE)));

		if(old_val)
		  fs_give((void **) &old_val);

		/*
		 * If the user sets one of foreground/background and the
		 * other is not yet set, set it.
		 */
		if(!(hc->fg && hc->fg[0])){
		    if(hc->fg)
		      fs_give((void **)&hc->fg);

		    hc->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
		}
	    }
	    else{
		old_val = hc->fg;

		if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
		  hc->fg = cpystr(colorx(CFC_ICOLOR(*cl)));
		else if(old_val)
		  hc->fg = cpystr(is_rgb_color(old_val)
				    ? old_val
				    : color_to_asciirgb(old_val));
		else
		  hc->fg = cpystr(color_to_asciirgb(colorx(COL_BLACK)));

		if(old_val)
		  fs_give((void **) &old_val);

		if(!(hc->bg && hc->bg[0])){
		    if(hc->bg)
		      fs_give((void **)&hc->bg);

		    hc->bg = cpystr(ps->VAR_NORM_BACK_COLOR);
		}
	    }
	}

	/*
	 * Turn on selected *'s for default selections, if any, and
	 * for ones we forced on.
	 */
	color_update_selected(ps, *cl, 
			      (hc && hc->fg && hc->fg[0]
			       && hc->bg && hc->bg[0])
				  ? hc->fg : ps->VAR_NORM_FORE_COLOR,
			      (hc && hc->fg && hc->fg[0]
			       && hc->bg && hc->bg[0])
				  ? hc->bg : ps->VAR_NORM_BACK_COLOR,
			      TRUE);

	alval = ALVAL((*cl)->var, ew);
	free_list_array(alval);
	*alval = varlist_from_spec_colors(hcolors);

	if(hcolors)
	  free_spec_colors(&hcolors);

	fix_side_effects(ps, (*cl)->var, 0);
	set_current_color_vals(ps);
	ClearScreen();
	rv = ps->mangled_screen = 1;
	break;

      case MC_TOGGLE :		/* toggle default on or off */
	fgv = (*cl)->var;				/* foreground color */
	bgv = (*cl)->var+1;				/* background color */

	if((*cl)->value[1] == 'X'){		/* turning default off */
	    (*cl)->value[1] = ' ';
	    /*
	     * Take whatever color is the current_val and suck it
	     * into the user_val. Same colors remain checked.
	     */
	    apval1 = APVAL(fgv, ew);
	    if(apval1){
		if(*apval1)
		  fs_give((void **)apval1);
	    }

	    apval2 = APVAL(bgv, ew);
	    if(apval2){
		if(*apval2)
		  fs_give((void **)apval2);
	    }

	    /* editing normal but there is an exception config */
	    if((ps->ew_for_except_vars != Main) && (ew == Main)){
		COLOR_PAIR *newc;

		/* use global_val if it is set */
		if(fgv && fgv->global_val.p && fgv->global_val.p[0] &&
		   bgv && bgv->global_val.p && bgv->global_val.p[0]){
		    *apval1 = cpystr(fgv->global_val.p);
		    *apval2 = cpystr(bgv->global_val.p);
		}
		else if(var_defaults_to_rev(fgv) &&
		        (newc = pico_get_rev_color())){
		    *apval1 = cpystr(newc->fg);
		    *apval2 = cpystr(newc->bg);
		}
		else{
		    *apval1 = cpystr(ps->VAR_NORM_FORE_COLOR);
		    *apval2 = cpystr(ps->VAR_NORM_BACK_COLOR);
		}
	    }
	    else{				/* editing outermost config */
		/* just use current_vals */
		if(fgv->current_val.p)
		  *apval1 = cpystr(fgv->current_val.p);
		if(bgv->current_val.p)
		  *apval2 = cpystr(bgv->current_val.p);
	    }
	}
	else{					/* turning default on */
	    char *starred_fg = NULL, *starred_bg = NULL;

	    (*cl)->value[1] = 'X';
	    apval = APVAL(fgv, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);

	    apval = APVAL(bgv, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);

	    if(fgv->cmdline_val.p)
	      fs_give((void **)&fgv->cmdline_val.p);

	    if(bgv->cmdline_val.p)
	      fs_give((void **)&bgv->cmdline_val.p);

	    set_current_color_vals(ps);

	    if(fgv == &ps->vars[V_SLCTBL_FORE_COLOR]){
		F_TURN_OFF(F_SLCTBL_ITEM_NOBOLD, ps);
		(*cl)->next->value[1] = 'X';
	    }

	    /* editing normal but there is an exception config */
	    if((ps->ew_for_except_vars != Main) && (ew == Main)){
		COLOR_PAIR *newc;

		/* use global_val if it is set */
		if(fgv && fgv->global_val.p && fgv->global_val.p[0] &&
		   bgv && bgv->global_val.p && bgv->global_val.p[0]){
		    starred_fg = fgv->global_val.p;
		    starred_bg = bgv->global_val.p;
		}
		else if(var_defaults_to_rev(fgv) &&
		        (newc = pico_get_rev_color())){
		    starred_fg = newc->fg;
		    starred_bg = newc->bg;
		}
		else{
		    starred_fg = ps->VAR_NORM_FORE_COLOR;
		    starred_bg = ps->VAR_NORM_BACK_COLOR;
		}
	    }
	    else{				/* editing outermost config */
		starred_fg = fgv->current_val.p;
		starred_bg = bgv->current_val.p;
	    }

	    /*
	     * Turn on selected *'s for default selections.
	     */
	    color_update_selected(ps, prev_confline(*cl),
				  starred_fg, starred_bg, FALSE);

	    ps->mangled_body = 1;
	}

	fix_side_effects(ps, fgv, 0);
	rv = 1;
	break;

      case MC_TOGGLEB :		/* toggle default on or off, hdr color */
	/*
	 * Find the SPEC_COLOR_S for header.
	 */
	rv = 1;
	lval = LVAL((*cl)->var, ew);
	hcolors = spec_colors_from_varlist(lval, 0);
	for(hc = hcolors, i=0; hc; hc = hc->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if((*cl)->value[1] == 'X'){		/* turning default off */
	    (*cl)->value[1] = ' ';
	    /*
	     * Take whatever color is the default value and suck it
	     * into the hc structure.
	     */
	    if(hc){
		if(hc->bg)
		  fs_give((void **)&hc->bg);
		if(hc->fg)
		  fs_give((void **)&hc->fg);
		
		if(ps->VAR_NORM_FORE_COLOR &&
		   ps->VAR_NORM_FORE_COLOR[0] &&
		   ps->VAR_NORM_BACK_COLOR &&
		   ps->VAR_NORM_BACK_COLOR[0]){
		    hc->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
		    hc->bg = cpystr(ps->VAR_NORM_BACK_COLOR);
		}

		if(lval && lval[i]){
		    fs_give((void **)&lval[i]);
		    lval[i] = var_from_spec_color(hc);
		}
	    }
	}
	else{					/* turning default on */
	    (*cl)->value[1] = 'X';
	    /* Remove current colors, leaving val */
	    if(hc){
		if(hc->bg)
		  fs_give((void **)&hc->bg);
		if(hc->fg)
		  fs_give((void **)&hc->fg);

		if(lval && lval[i]){
		    fs_give((void **)&lval[i]);
		    lval[i] = var_from_spec_color(hc);
		}
	    }

	    set_current_color_vals(ps);
	    ClearScreen();
	    ps->mangled_screen = 1;

	}

	if(hcolors)
	  free_spec_colors(&hcolors);

	/*
	 * Turn on selected *'s for default selections.
	 */
	color_update_selected(ps, prev_confline(*cl),
			      ps->VAR_NORM_FORE_COLOR,
			      ps->VAR_NORM_BACK_COLOR,
			      FALSE);

	break;

      case MC_TOGGLEC :		/* toggle default on or off, keyword color */
	for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(!kw){					/* can't happen */
	    dprint(1,(debugfile,
		"This can't happen, kw not set when togglec keyword color\n"));
	    break;
	}

	hcolors = spec_colors_from_varlist(LVAL((*cl)->var, ew), 0);

	/*
	 * Look through hcolors, derived from lval, to find this keyword
	 * and its current color.
	 */
	for(hc = hcolors; hc; hc = hc->next)
	  if(hc->spec && ((kw->nick && !strucmp(kw->nick, hc->spec))
			  || (kw->kw && !strucmp(kw->kw, hc->spec))))
	    break;

	/* Remove this color from list */
	if(hc){
	    SPEC_COLOR_S *tmp;

	    if(hc == hcolors){
		hcolors = hc->next;
		hc->next = NULL;
		free_spec_colors(&hc);
	    }
	    else{
		for(tmp = hcolors; tmp->next; tmp = tmp->next)
		  if(tmp->next == hc)
		    break;

		if(tmp->next){
		    tmp->next = hc->next;
		    hc->next = NULL;
		    free_spec_colors(&hc);
		}
	    }
	}

	if((*cl)->value[1] == 'X')
	  (*cl)->value[1] = ' ';
	else
	  (*cl)->value[1] = 'X';

	/*
	 * Turn on selected *'s for default selections, if any, and
	 * for ones we forced on.
	 */
	color_update_selected(ps, prev_confline(*cl), 
			      ps->VAR_NORM_FORE_COLOR,
			      ps->VAR_NORM_BACK_COLOR,
			      FALSE);

	alval = ALVAL((*cl)->var, ew);
	free_list_array(alval);
	*alval = varlist_from_spec_colors(hcolors);

	if(hcolors)
	  free_spec_colors(&hcolors);

	fix_side_effects(ps, (*cl)->var, 0);
	set_current_color_vals(ps);
	ClearScreen();
	rv = ps->mangled_screen = 1;
	break;

      case MC_TOGGLED :		/* toggle selectable item bold on or off */
	toggle_feature_bit(ps, feature_list_index(F_SLCTBL_ITEM_NOBOLD),
			   &ps->vars[V_FEATURE_LIST], *cl, 1);
	ps->mangled_body = 1;		/* to fix Sample Text */
	rv = 1;
	break;

      case MC_DEFAULT :				/* restore default values */

	/* First, confirm that user wants to restore all default colors */
	if(want_to("Really restore all colors to default values",
		   'y', 'n', NO_HELP, WT_NORM) != 'y'){
	    cmd_cancelled("RestoreDefs");
	    return(rv);
	}

	/* get rid of all user set colors */
	for(v = ps->vars; v->name; v++){
	    if(!color_holding_var(ps, v)
	       || v == &ps->vars[V_VIEW_HDR_COLORS]
	       || v == &ps->vars[V_KW_COLORS])
	      continue;

	    apval = APVAL(v, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);

	    if(v->cmdline_val.p)
	      fs_give((void **)&v->cmdline_val.p);
	}

	/*
	 * For custom header colors, we want to remove the color values
	 * but leave the spec value so that it is easy to reset.
	 */
	alval = ALVAL(&ps->vars[V_VIEW_HDR_COLORS], ew);
	if(alval && *alval){
	    hcolors = spec_colors_from_varlist(*alval, 0);
	    for(hc = hcolors; hc; hc = hc->next){
		if(hc->fg)
		  fs_give((void **)&hc->fg);
		if(hc->bg)
		  fs_give((void **)&hc->bg);
	    }

	    free_list_array(alval);
	    *alval = varlist_from_spec_colors(hcolors);

	    if(hcolors)
	      free_spec_colors(&hcolors);
	}

	/*
	 * Same for keyword colors.
	 */
	alval = ALVAL(&ps->vars[V_KW_COLORS], ew);
	if(alval && *alval){
	    hcolors = spec_colors_from_varlist(*alval, 0);
	    for(hc = hcolors; hc; hc = hc->next){
		if(hc->fg)
		  fs_give((void **)&hc->fg);
		if(hc->bg)
		  fs_give((void **)&hc->bg);
	    }

	    free_list_array(alval);
	    *alval = varlist_from_spec_colors(hcolors);

	    if(hcolors)
	      free_spec_colors(&hcolors);
	}

	/* set bold for selectable items */
	F_TURN_OFF(F_SLCTBL_ITEM_NOBOLD, ps);
	lval = LVAL(&ps->vars[V_FEATURE_LIST], ew);
	if(test_feature(lval, feature_list_name(F_SLCTBL_ITEM_NOBOLD), 0))
	  toggle_feature_bit(ps, feature_list_index(F_SLCTBL_ITEM_NOBOLD),
			     &ps->vars[V_FEATURE_LIST], *cl, 1);

	set_current_color_vals(ps);
	clear_index_cache();

	/* redo config display */
	*cl = first_confline(*cl);
	free_conflines(cl);
	opt_screen->top_line = NULL;
	first_line = NULL;
	color_config_init_display(ps, cl, &first_line);
	*cl = first_line;
	ClearScreen();
	ps->mangled_screen = 1;
	rv = 1;
	break;

      case MC_ADD :				/* add custom header color */
	/* get header field name */
	help = NO_HELP;
	while(1){
	    i = optionally_enter(sval, -FOOTER_ROWS(ps), 0, sizeof(sval),
			     "Enter the name of the header field to be added: ",
				 NULL, help, NULL);
	    if(i == 0)
	      break;
	    else if(i == 1){
		cmd_cancelled("Add");
		cancel = 1;
		break;
	    }
	    else if(i == 3){
		help = help == NO_HELP ? h_config_add_custom_color : NO_HELP;
		continue;
	    }
	    else
	      break;
	}

	ps->mangled_footer = 1;

	removing_leading_and_trailing_white_space(sval);
	if(sval[strlen(sval)-1] == ':')  /* remove trailing colon */
	  sval[strlen(sval)-1] = '\0';

	removing_trailing_white_space(sval);

	if(cancel || !sval[0])
	  break;

	new_hcolor = (SPEC_COLOR_S *)fs_get(sizeof(*new_hcolor));
	memset((void *)new_hcolor, 0, sizeof(*new_hcolor));
	new_hcolor->spec = cpystr(sval);
	confline = var_from_spec_color(new_hcolor);

	/* add it to end of list */
	alval = ALVAL(&ps->vars[V_VIEW_HDR_COLORS], ew);
	if(alval){
	    for(t = *alval, i=0; t && t[0]; t++)
	      i++;
	    
	    if(i)
	      fs_resize((void **)alval, sizeof(char *) * (i+1+1));
	    else
	      *alval = (char **)fs_get(sizeof(char *) * (i+1+1));
	    
	    (*alval)[i] = confline;
	    (*alval)[i+1] = NULL;
	}

	set_current_color_vals(ps);

	/* go to end of display */
	for(ctmp = *cl; ctmp && ctmp->next; ctmp = next_confline(ctmp))
	  ;

	/* back up to the KEYWORD COLORS title line */
	for(; ctmp && (!ctmp->value || strcmp(ctmp->value, KW_COLORS_HDR))
	      && ctmp->prev;
	    ctmp = prev_confline(ctmp))
	  ;

	/*
	 * Back up to last header line, or comment line if no header lines.
	 * One of many in a long line of dangerous moves in the config
	 * screens.
	 */
	ctmp = prev_confline(ctmp);		/* ------- line */
	ctmp = prev_confline(ctmp);		/* blank line */
	ctmp = prev_confline(ctmp);		/* the line */
	
	*cl = ctmp;
	
	/* delete the comment line if there were no headers before this */
	if(i == 0){
	    beg = ctmp->prev;
	    end = ctmp;

	    *cl = beg ? beg->prev : NULL;

	    if(beg && beg->prev)		/* this will always be true */
	      beg->prev->next = end ? end->next : NULL;
	    
	    if(end && end->next)
	      end->next->prev = beg ? beg->prev : NULL;
	    
	    if(end)
	      end->next = NULL;
	    
	    if(beg == opt_screen->top_line || end == opt_screen->top_line)
	      opt_screen->top_line = NULL;

	    free_conflines(&beg);
	}

	add_header_color_line(ps, cl, confline, i);

	/* be sure current is on selectable line */
	for(; *cl && ((*cl)->flags & CF_NOSELECT); *cl = next_confline(*cl))
	  ;
	for(; *cl && ((*cl)->flags & CF_NOSELECT); *cl = prev_confline(*cl))
	  ;

	rv = ps->mangled_body = 1;
	break;

      case MC_DELETE :				/* delete custom header color */
	if((*cl)->var != &ps->vars[V_VIEW_HDR_COLORS]){
	    q_status_message(SM_ORDER, 0, 2,
			     "Can't delete this color setting");
	    break;
	}

	if(want_to("Really delete header color from config",
		   'y', 'n', NO_HELP, WT_NORM) != 'y'){
	    cmd_cancelled("Delete");
	    return(rv);
	}

	alval = ALVAL((*cl)->var, ew);
	if(alval){
	    int n, j;

	    for(t = *alval, n=0; t && t[0]; t++)
	      n++;
	    
	    j = CFC_ICUST(*cl);

	    if(n > j){		/* it better be */
		if((*alval)[j])
		  fs_give((void **)&(*alval)[j]);

		for(i = j; i < n; i++)
		  (*alval)[i] = (*alval)[i+1];
	    }
	}

	set_current_color_vals(ps);

	/*
	 * Note the conf lines that go with this header. That's the
	 * blank line before and the current line.
	 */
	beg = (*cl)->prev;
	end = *cl;

	another = 0;
	/* reset current line */
	if(end && end->next && end->next->next && 
	   end->next->next->var == &ps->vars[V_VIEW_HDR_COLORS]){
	    *cl = end->next->next;		/* next Header Color */
	    another++;
	}
	else if(beg && beg->prev &&
	   beg->prev->var == &ps->vars[V_VIEW_HDR_COLORS]){
	    *cl = beg->prev;			/* prev Header Color */
	    another++;
	}

	/* adjust SPEC_COLOR_S index (varmem) values */
	for(ctmp = end; ctmp; ctmp = next_confline(ctmp))
	  if(ctmp->var == &ps->vars[V_VIEW_HDR_COLORS])
	    ctmp->varmem = CFC_ICUST_DEC(ctmp);
	
	/*
	 * If that was the last header color line, add in the comment
	 * line placeholder. If there is another, just delete the
	 * old conf lines.
	 */
	if(another){
	    if(beg && beg->prev)		/* this will always be true */
	      beg->prev->next = end ? end->next : NULL;
	    
	    if(end && end->next)
	      end->next->prev = beg ? beg->prev : NULL;
	    
	    if(end)
	      end->next = NULL;
	    
	    if(beg == opt_screen->top_line || end == opt_screen->top_line)
	      opt_screen->top_line = NULL;

	    free_conflines(&beg);
	}
	else if(end){
	    if(end->varname)
	      fs_give((void **) &end->varname);

	    if(end->value)
	      fs_give((void **) &end->value);

	    end->flags     = CF_NOSELECT;
	    end->help      = NO_HELP;
	    end->value     = cpystr(ADDHEADER_COMMENT);
	    end->valoffset = COLOR_INDENT;
	    end->varnamep  = NULL;
	    end->varmem    = 0;
	    end->keymenu   = NULL;
	    end->tool      = NULL;
	}

	/* at least put current on some selectable line */
	for(; *cl && ((*cl)->flags & CF_NOSELECT) && next_confline(*cl);
	    *cl = next_confline(*cl))
	  ;
	for(; *cl && ((*cl)->flags & CF_NOSELECT) && prev_confline(*cl);
	    *cl = prev_confline(*cl))
	  ;

	rv = ps->mangled_body = 1;
	q_status_message(SM_ORDER, 0, 3, "header color deleted");
	break;

      case MC_SHUFFLE :  /* shuffle order of custom headers */
	if((*cl)->var != &ps->vars[V_VIEW_HDR_COLORS]){
	    q_status_message(SM_ORDER, 0, 2,
			     "Can't shuffle this color setting");
	    break;
	}

	alval = ALVAL((*cl)->var, ew);
	if(!alval)
	  return(rv);

	curcolor = CFC_ICUST(*cl);
	prevcolor = curcolor-1;
	nextcolor = curcolor+1;
	if(!*alval || !(*alval)[nextcolor])
	  nextcolor = -1;

	if((prevcolor < 0 && nextcolor < 0) || !*alval){
	    q_status_message(SM_ORDER, 0, 3,
   "Shuffle only makes sense when there is more than one Header Color defined");
	    return(rv);
	}

	/* Move it up or down? */
	i = 0;
	opts[i].ch      = 'u';
	opts[i].rval    = 'u';
	opts[i].name    = "U";
	opts[i++].label = "Up";

	opts[i].ch      = 'd';
	opts[i].rval    = 'd';
	opts[i].name    = "D";
	opts[i++].label = "Down";

	opts[i].ch = -1;
	deefault = 'u';

	if(prevcolor < 0){		/* no up */
	    opts[0].ch = -2;
	    deefault = 'd';
	}
	else if(nextcolor < 0)
	  opts[1].ch = -2;	/* no down */

	sprintf(prompt, "Shuffle %s%s%s ? ",
		(opts[0].ch != -2) ? "UP" : "",
		(opts[0].ch != -2 && opts[1].ch != -2) ? " or " : "",
		(opts[1].ch != -2) ? "DOWN" : "");
	help = (opts[0].ch == -2) ? h_hdrcolor_shuf_down
				  : (opts[1].ch == -2) ? h_hdrcolor_shuf_up
						       : h_hdrcolor_shuf;

	i = radio_buttons(prompt, -FOOTER_ROWS(ps), opts, deefault, 'x',
			   help, RB_NORM, NULL);

	switch(i){
	  case 'x':
	    cmd_cancelled("Shuffle");
	    return(rv);

	  case 'u':
	  case 'd':
	    break;
	}
		
	/* swap order */
	if(i == 'd'){
	    old_val = (*alval)[curcolor];
	    (*alval)[curcolor] = (*alval)[nextcolor];
	    (*alval)[nextcolor] = old_val;
	}
	else if(i == 'u'){
	    old_val = (*alval)[curcolor];
	    (*alval)[curcolor] = (*alval)[prevcolor];
	    (*alval)[prevcolor] = old_val;
	}
	else		/* can't happen */
	  return(rv);

	set_current_color_vals(ps);

	/*
	 * Swap the conf lines.
	 */

	cur_beg = (*cl)->prev;
	cur_end = *cl;

	if(i == 'd'){
	    next_beg = cur_end->next;
	    next_end = next_beg ? next_beg->next : NULL;

	    if(next_end->next)
	      next_end->next->prev = cur_end;
	    cur_end->next = next_end->next;
	    next_end->next = cur_beg;
	    if(cur_beg->prev)
	      cur_beg->prev->next = next_beg;
	    next_beg->prev = cur_beg->prev;
	    cur_beg->prev = next_end;

	    /* adjust SPEC_COLOR_S index values */
	    cur_beg->varmem	  = CFC_ICUST_INC(cur_beg);
	    cur_beg->next->varmem = CFC_ICUST_INC(cur_beg->next);

	    next_beg->varmem	   = CFC_ICUST_DEC(next_beg);
	    next_beg->next->varmem = CFC_ICUST_DEC(next_beg->next);

	    if(opt_screen->top_line == cur_end)
	      opt_screen->top_line = next_end;
	    else if(opt_screen->top_line == cur_beg)
	      opt_screen->top_line = next_beg;
	}
	else{
	    prev_end = cur_beg->prev;
	    prev_beg = prev_end ? prev_end->prev : NULL;

	    if(prev_beg && prev_beg->prev)
	      prev_beg->prev->next = cur_beg;
	    cur_beg->prev = prev_beg->prev;
	    prev_beg->prev = cur_end;
	    if(cur_end->next)
	      cur_end->next->prev = prev_end;
	    prev_end->next = cur_end->next;
	    cur_end->next = prev_beg;

	    /* adjust SPEC_COLOR_S index values */
	    cur_beg->varmem	  = CFC_ICUST_DEC(cur_beg);
	    cur_beg->next->varmem = CFC_ICUST_DEC(cur_beg->next);

	    prev_beg->varmem	   = CFC_ICUST_INC(prev_beg);
	    prev_beg->next->varmem = CFC_ICUST_INC(prev_beg->next);

	    if(opt_screen->top_line == prev_end)
	      opt_screen->top_line = cur_end;
	    else if(opt_screen->top_line == prev_beg)
	      opt_screen->top_line = cur_beg;
	}

	rv = ps->mangled_body = 1;
	q_status_message(SM_ORDER, 0, 3, "Header Colors shuffled");
	break;

      case MC_EDIT:
	rv = color_edit_screen(ps, cl);
	if((*cl)->value && (*cl)->var &&
	   srchstr((*cl)->var->name, "-foreground-color")){
	    fs_give((void **)&(*cl)->value);
	    (*cl)->value = pretty_value(ps, *cl);
	}

	break;

      case MC_EXIT:				/* exit */
	if((*cl)->keymenu == &color_changing_keymenu ||
	   (*cl)->keymenu == &kw_color_changing_keymenu ||
	   (*cl)->keymenu == &custom_color_changing_keymenu ||
	   ((*cl)->prev &&
	    ((*cl)->prev->keymenu == &color_changing_keymenu ||
	     (*cl)->prev->keymenu == &kw_color_changing_keymenu ||
	     (*cl)->prev->keymenu == &custom_color_changing_keymenu)) ||
	   ((*cl)->prev->prev &&
	    ((*cl)->prev->prev->keymenu == &color_changing_keymenu ||
	     (*cl)->prev->prev->keymenu == &kw_color_changing_keymenu ||
	     (*cl)->prev->prev->keymenu == &custom_color_changing_keymenu)))
	  rv = simple_exit_cmd(flags);
	else
	  rv = config_exit_cmd(flags);

	break;

#ifdef	_WINDOWS
      case MC_RGB1 :
	fgv = (*cl)->var;
	bgv = (*cl)->var+1;
	v = (*cl)->var;
	if((*cl)->flags & CF_VAR2)
	  v += 1;

	pval = PVAL(v, ew);
	apval = APVAL(v, ew);
	if(old_val = mswin_rgbchoice(pval ? pval : v->current_val.p)){
	    if(*apval)
	      fs_give((void **)apval);

	    *apval = old_val;
	    set_current_val(v, TRUE, FALSE);
	    fix_side_effects(ps, v, 0);
	    set_current_color_vals(ps);
	    color_update_selected(ps, *cl, PVAL(fgv, ew), PVAL(bgv, ew), TRUE);
	    rv = ps->mangled_screen = 1;
	}

	break;

      case MC_RGB2 :
	/*
	 * Find the SPEC_COLOR_S for header.
	 */
	alval = ALVAL((*cl)->var, ew);
	hcolors = spec_colors_from_varlist(*alval, 0);

	for(hc = hcolors, i = 0; hc; hc = hc->next, i++)
	  if(CFC_ICUST(*cl) == i){
	      char **pc = ((*cl)->flags & CF_VAR2) ? &hc->bg : &hc->fg;

	      if(old_val = mswin_rgbchoice(*pc)){
		  fs_give((void **) pc);
		  *pc = old_val;
		  color_update_selected(ps, *cl,
					(hc->fg && hc->fg[0]
					 && hc->bg && hc->bg[0])
					  ? hc->fg : ps->VAR_NORM_FORE_COLOR,
					(hc->fg && hc->fg[0]
					 && hc->bg && hc->bg[0])
					  ? hc->bg : ps->VAR_NORM_BACK_COLOR,
					TRUE);

		  if(hc && *alval && (*alval)[i]){
		      fs_give((void **)&(*alval)[i]);
		      (*alval)[i] = var_from_spec_color(hc);
		  }
	
		  if(hcolors)
		    free_spec_colors(&hcolors);

		  set_current_color_vals(ps);
		  ClearScreen();
		  rv = ps->mangled_screen = 1;
	      }

	      break;
	  }

	break;

      case MC_RGB3 :
	/*
	 * Custom colored keywords.
	 */
	for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(!kw){					/* can't happen */
	    dprint(1,(debugfile,
		"This can't happen, kw not set in MC_RGB3\n"));
	    break;
	}

	hcolors = spec_colors_from_varlist(LVAL((*cl)->var, ew), 0);

	/*
	 * Look through hcolors, derived from lval, to find this keyword
	 * and its current color.
	 */
	for(hc = hcolors; hc; hc = hc->next)
	  if(hc->spec && ((kw->nick && !strucmp(kw->nick, hc->spec))
			  || (kw->kw && !strucmp(kw->kw, hc->spec))))
	    break;

	if(!hc){	/* this keyword didn't have a color set, add to list */
	    SPEC_COLOR_S *new;

	    new = (SPEC_COLOR_S *) fs_get(sizeof(*hc));
	    memset((void *) new, 0, sizeof(*new));
	    new->spec = cpystr(kw->kw);
	    new->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
	    new->bg = cpystr(ps->VAR_NORM_BACK_COLOR);

	    if(hcolors){
		for(hc = hcolors; hc->next; hc = hc->next)
		  ;

		hc->next = new;
	    }
	    else
	      hcolors = new;

	    hc = new;
	}

	if(hc){
	    char **pc = ((*cl)->flags & CF_VAR2) ? &hc->bg : &hc->fg;

	    if(old_val = mswin_rgbchoice(*pc)){
		fs_give((void **) pc);
		*pc = old_val;

		/*
		 * Turn on selected *'s for default selections, if any, and
		 * for ones we forced on.
		 */
		color_update_selected(ps, *cl, 
				      (hc && hc->fg && hc->fg[0]
				       && hc->bg && hc->bg[0])
					  ? hc->fg : ps->VAR_NORM_FORE_COLOR,
				      (hc && hc->fg && hc->fg[0]
				       && hc->bg && hc->bg[0])
					  ? hc->bg : ps->VAR_NORM_BACK_COLOR,
				      TRUE);

		alval = ALVAL((*cl)->var, ew);
		free_list_array(alval);
		*alval = varlist_from_spec_colors(hcolors);
		fix_side_effects(ps, (*cl)->var, 0);
		set_current_color_vals(ps);
		ClearScreen();
		rv = 1;
	    }
	}

	if(hcolors)
	  free_spec_colors(&hcolors);

	ps->mangled_screen = 1;
	break;
#endif

      default :
	rv = -1;
	break;
    }


    if(rv == 1)
      exception_override_warning((*cl)->var);

    return(rv);
}


/*
 * Turn on selected *'s for default selections, if any, and
 * for ones we forced on.
 * Adjust the Sample line right above the color selection lines.
 */
void
color_update_selected(ps, cl, fg, bg, cleardef)
    struct pine *ps;
    CONF_S      *cl;
    char        *fg, *bg;
    int	         cleardef;
{
    int i;

    /* back up to header line */
    for(; cl && (cl->flags & CF_DOUBLEVAR); cl = prev_confline(cl))
      ;
    
    /* adjust sample line */
    if(cl && cl->var && cl->flags & CF_COLORSAMPLE){
	if(cl->value)
	  fs_give((void **)&cl->value);
	
	cl->value = color_setting_text_line(ps, cl->var);
    }

    for(i = 0, cl = next_confline(cl);
	i < pico_count_in_color_table() && cl;
	i++, cl = next_confline(cl)){
	if(fg && !strucmp(color_to_canonical_name(fg), colorx(i)))
	  cl->value[1] = R_SELD;
	else
	  cl->value[1] = ' ';

	if(bg && !strucmp(color_to_canonical_name(bg), colorx(i)))
	  cl->value[cl->val2offset - cl->valoffset + 1] = R_SELD;
	else
	  cl->value[cl->val2offset - cl->valoffset + 1] = ' ';
    }

#ifdef	_WINDOWS
    /* check for rgb color indicating a custom setting */
    cl->value[1] = (fg && is_rgb_color(fg)) ? R_SELD : ' ';
    cl->value[cl->val2offset - cl->valoffset + 1] =  (bg && is_rgb_color(bg))
							? R_SELD : ' ';
    cl = next_confline(cl); /* advance to Default checkbox */
#endif

    /* Turn off Default X */
    if(cleardef)
      cl->value[1] = ' ';
}


int
color_edit_screen(ps, cl)
    struct pine *ps;
    CONF_S     **cl;
{
    OPT_SCREEN_S     screen, *saved_screen;
    CONF_S          *ctmp = NULL, *first_line = NULL, *ctmpb;
    int              rv, is_index = 0, is_custom = 0, indent = 12;
    int              is_normal = 0, is_keywordcol = 0;
    char             tmp[200+1], name[200], *p;
    struct variable *vtmp, v;
    int              i, def;
    COLOR_PAIR      *color = NULL;
    SPEC_COLOR_S    *hc = NULL, *hcolors = NULL;
    KEYWORD_S       *kw;

    vtmp = (*cl)->var;
    if(vtmp == &ps->vars[V_VIEW_HDR_COLORS])
      is_custom++;
    else if(vtmp == &ps->vars[V_KW_COLORS])
      is_keywordcol++;
    else if(color_holding_var(ps, vtmp)){
	if(!struncmp(vtmp->name, "index-", 6))
	  is_index++;
	else
	  is_normal++;
    }

    new_confline(&ctmp);
    /* Blank line */
    ctmp->flags |= CF_NOSELECT | CF_B_LINE;

    first_line = ctmp;

    new_confline(&ctmp)->var = vtmp;

    if(is_normal){
	p = srchstr(vtmp->name, "-foreground-color");
	sprintf(name, "%.*s", p ? min(p - vtmp->name, 30) : 30, vtmp->name);
	if(islower((unsigned char)name[0]))
	  name[0] = toupper((unsigned char)name[0]);
    }
    else if(is_index){
	p = srchstr(vtmp->name, "-foreground-color");
	sprintf(name, "%.*s Symbol",
		p ? min(p - vtmp->name, 30) : 30, vtmp->name);
	if(islower((unsigned char)name[0]))
	  name[0] = toupper((unsigned char)name[0]);
    }
    else if(is_custom){
	char **lval;
	
	lval = LVAL(vtmp, ew);
	hcolors = spec_colors_from_varlist(lval, 0);

	for(hc = hcolors, i = 0; hc; hc = hc->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;
	
	if(hc){
	    sprintf(name, "%s%.50s", HEADER_WORD, hc->spec);
	    i = sizeof(HEADER_WORD) - 1;
	    if(islower((unsigned char) name[i]))
	      name[i] = toupper((unsigned char) name[i]);
	}
    }
    else if(is_keywordcol){
	char **lval;

	for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(kw){
	    char *nm, *comment = NULL;

	    nm = kw->nick ? kw->nick : kw->kw ? kw->kw : "";
	    if(kw->nick && kw->kw && kw->kw[0])
	      comment = kw->kw;

	    if(strlen(nm) + (comment ? strlen(comment) : 0) < 60)
	      sprintf(name, "%.50s%.2s%.50s%.1s",
		      nm,
		      comment ? " (" : "",
		      comment ? comment : "",
		      comment ? ")" : "");
	    else
	      sprintf(name, "%.60s", nm);

	    lval = LVAL(vtmp, ew);
	    hcolors = spec_colors_from_varlist(lval, 0);
	    if(kw && hcolors)
	      if(!(kw->nick && kw->nick[0]
		   && (color=hdr_color(kw->nick, NULL, hcolors))))
		if(kw->kw && kw->kw[0])
		  color = hdr_color(kw->kw, NULL, hcolors);
	}
    }
    else{
	name[0] = '\0';
    }

    sprintf(tmp, "%.80s Color =", name);
    ctmp->varname		 = cpystr(tmp);
    ctmp->varnamep		 = ctmpb = ctmp;
    ctmp->flags			|= (CF_STARTITEM | CF_NOSELECT);
    ctmp->keymenu		 = &color_changing_keymenu;

    if(is_custom){
	char **apval;

	def = !(hc && hc->fg && hc->fg[0] && hc->bg && hc->bg[0]);
	
	add_color_setting_disp(ps, &ctmp, vtmp, ctmpb,
			       &custom_color_changing_keymenu,
			       &hdr_color_checkbox_keymenu,
			       config_help(vtmp - ps->vars, 0),
			       indent, CFC_ICUST(*cl),
			       def ? ps_global->VAR_NORM_FORE_COLOR
				   : hc->fg,
			       def ? ps_global->VAR_NORM_BACK_COLOR
				   : hc->bg,
			       def);

	/* optional string to match in header value */
	new_confline(&ctmp);
	ctmp->varnamep		 = ctmpb;
	ctmp->keymenu		 = &color_pattern_keymenu;
	ctmp->help		 = h_config_customhdr_pattern;
	ctmp->tool		 = color_text_tool;
	ctmp->varoffset		 = indent-5;
	ctmp->varname		 = cpystr("Pattern to match =");
	ctmp->valoffset		 = indent-5 + strlen(ctmp->varname) + 1;
	ctmp->varmem		 = (*cl)->varmem;

	/*
	 * This is really ugly. This is just to get the value correct.
	 */
	memset(&v, 0, sizeof(v));
	v.is_used    = 1;
	v.is_user    = 1;
	apval = APVAL(&v, ew);
	if(hc && hc->val && apval)
	  *apval = pattern_to_string(hc->val);

	set_current_val(&v, FALSE, FALSE);
	ctmp->var = &v;
	ctmp->value = pretty_value(ps, ctmp);
	ctmp->var = vtmp;
	if(apval && *apval)
	  fs_give((void **)apval);

	if(v.current_val.p)
	  fs_give((void **)&v.current_val.p);

	if(hcolors)
	  free_spec_colors(&hcolors);
    }
    else if(is_keywordcol){

	def = !(color && color->fg && color->fg[0]
		&& color->bg && color->bg[0]);
	
	add_color_setting_disp(ps, &ctmp, vtmp, ctmpb,
			       &kw_color_changing_keymenu,
			       &kw_color_checkbox_keymenu,
			       config_help(vtmp - ps->vars, 0),
			       indent, CFC_ICUST(*cl),
			       def ? ps_global->VAR_NORM_FORE_COLOR
				   : color->fg,
			       def ? ps_global->VAR_NORM_BACK_COLOR
				   : color->bg,
			       def);

	if(hcolors)
	  free_spec_colors(&hcolors);
    }
    else{
	char       *pvalfg, *pvalbg;
	int         def;
	COLOR_PAIR *newc;

	pvalfg = PVAL(vtmp, ew);
	pvalbg = PVAL(vtmp+1, ew);
	def = !(pvalfg && pvalfg[0] && pvalbg && pvalbg[0]);
	if(def){
	    /* display default val, if there is one */
	    pvalfg = PVAL(vtmp, Main);
	    pvalbg = PVAL(vtmp+1, Main);
	    if(ew == Post && pvalfg && pvalfg[0] && pvalbg && pvalbg[0]){
		;
	    }
	    else if(vtmp && vtmp->global_val.p && vtmp->global_val.p[0] &&
	       (vtmp+1)->global_val.p && (vtmp+1)->global_val.p[0]){
		pvalfg = vtmp->global_val.p;
		pvalbg = (vtmp+1)->global_val.p;
	    }
	    else{
		if(var_defaults_to_rev(vtmp) && (newc = pico_get_rev_color())){
		    pvalfg = newc->fg;
		    pvalbg = newc->bg;
		}
		else{
		    pvalfg = NULL;
		    pvalbg = NULL;
		}
	    }
	}

	add_color_setting_disp(ps, &ctmp, vtmp, ctmpb,
			       &color_changing_keymenu,
			       &config_checkbox_keymenu,
			       config_help(vtmp - ps->vars, 0),
			       indent, 0, pvalfg, pvalbg, def);
    }

    first_line = first_sel_confline(first_line);

    saved_screen = opt_screen;
    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = saved_screen ? saved_screen->deferred_ro_warning : 0;
    rv = conf_scroll_screen(ps, &screen, first_line,
			    ew == Post ? "SETUP COLOR EXCEPTIONS"
				       : "SETUP COLOR",
			    "configuration ", 1);

    opt_screen = saved_screen;
    ps->mangled_screen = 1;
    return(rv);
}


void
set_current_color_vals(ps)
    struct pine *ps;
{
    struct variable *vars = ps->vars;
    int later_color_is_set = 0;

    set_current_val(&vars[V_NORM_FORE_COLOR], TRUE, TRUE);
    set_current_val(&vars[V_NORM_BACK_COLOR], TRUE, TRUE);
    pico_nfcolor(VAR_NORM_FORE_COLOR);
    pico_nbcolor(VAR_NORM_BACK_COLOR);

    set_current_val(&vars[V_REV_FORE_COLOR], TRUE, TRUE);
    set_current_val(&vars[V_REV_BACK_COLOR], TRUE, TRUE);
    pico_rfcolor(VAR_REV_FORE_COLOR);
    pico_rbcolor(VAR_REV_BACK_COLOR);

    set_color_val(&vars[V_TITLE_FORE_COLOR], 1);
    set_color_val(&vars[V_STATUS_FORE_COLOR], 1);
    set_color_val(&vars[V_KEYLABEL_FORE_COLOR], 1);
    set_color_val(&vars[V_KEYNAME_FORE_COLOR], 1);
    set_color_val(&vars[V_SLCTBL_FORE_COLOR], 1);
    set_color_val(&vars[V_PROMPT_FORE_COLOR], 1);
    set_color_val(&vars[V_IND_PLUS_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_IMP_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_DEL_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_ANS_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_NEW_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_REC_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_UNS_FORE_COLOR], 0);
    set_color_val(&vars[V_IND_ARR_FORE_COLOR], 0);
    set_color_val(&vars[V_SIGNATURE_FORE_COLOR], 0);

    set_current_val(&ps->vars[V_VIEW_HDR_COLORS], TRUE, TRUE);
    set_current_val(&ps->vars[V_KW_COLORS], TRUE, TRUE);
    set_custom_spec_colors(ps);

    /*
     * Set up the quoting colors. If a later color is set but not an earlier
     * color we set the earlier color to Normal to make it easier when
     * we go to use the colors. However, if the only quote colors set are
     * Normal that is the same as no settings, so delete them.
     */
    set_color_val(&vars[V_QUOTE1_FORE_COLOR], 0);
    set_color_val(&vars[V_QUOTE2_FORE_COLOR], 0);
    set_color_val(&vars[V_QUOTE3_FORE_COLOR], 0);

    if((!(VAR_QUOTE3_FORE_COLOR && VAR_QUOTE3_BACK_COLOR) ||
	(!strucmp(VAR_QUOTE3_FORE_COLOR, VAR_NORM_FORE_COLOR) &&
	 !strucmp(VAR_QUOTE3_BACK_COLOR, VAR_NORM_BACK_COLOR)))   &&
       (!(VAR_QUOTE2_FORE_COLOR && VAR_QUOTE2_BACK_COLOR) ||
	(!strucmp(VAR_QUOTE2_FORE_COLOR, VAR_NORM_FORE_COLOR) &&
	 !strucmp(VAR_QUOTE2_BACK_COLOR, VAR_NORM_BACK_COLOR)))   &&
       (!(VAR_QUOTE1_FORE_COLOR && VAR_QUOTE1_BACK_COLOR) ||
	(!strucmp(VAR_QUOTE1_FORE_COLOR, VAR_NORM_FORE_COLOR) &&
	 !strucmp(VAR_QUOTE1_BACK_COLOR, VAR_NORM_BACK_COLOR)))){
	/*
	 * They are all either Normal or not set. Delete them all.
	 */
	if(VAR_QUOTE3_FORE_COLOR)
	  fs_give((void **)&VAR_QUOTE3_FORE_COLOR);
	if(VAR_QUOTE3_BACK_COLOR)
	  fs_give((void **)&VAR_QUOTE3_BACK_COLOR);
	if(VAR_QUOTE2_FORE_COLOR)
	  fs_give((void **)&VAR_QUOTE2_FORE_COLOR);
	if(VAR_QUOTE2_BACK_COLOR)
	  fs_give((void **)&VAR_QUOTE2_BACK_COLOR);
	if(VAR_QUOTE1_FORE_COLOR)
	  fs_give((void **)&VAR_QUOTE1_FORE_COLOR);
	if(VAR_QUOTE1_BACK_COLOR)
	  fs_give((void **)&VAR_QUOTE1_BACK_COLOR);
    }
    else{			/* something is non-Normal */
	if(VAR_QUOTE3_FORE_COLOR && VAR_QUOTE3_BACK_COLOR)
	  later_color_is_set++;

	/* if 3 is set but not 2, set 2 to Normal */
	if(VAR_QUOTE2_FORE_COLOR && VAR_QUOTE2_BACK_COLOR)
	  later_color_is_set++;
	else if(later_color_is_set)
	  set_color_val(&vars[V_QUOTE2_FORE_COLOR], 1);

	/* if 3 or 2 is set but not 1, set 1 to Normal */
	if(VAR_QUOTE1_FORE_COLOR && VAR_QUOTE1_BACK_COLOR)
	  later_color_is_set++;
	else if(later_color_is_set)
	  set_color_val(&vars[V_QUOTE1_FORE_COLOR], 1);
    }

#ifdef	_WINDOWS
    if(ps->pre441){
	int conv_main = 0, conv_post = 0;

	ps->pre441 = 0;
	if(ps->prc && !unix_color_style_in_pinerc(ps->prc)){
	    conv_main = convert_pc_gray_names(ps, ps->prc, Main);
	    if(conv_main)
	      ps->prc->outstanding_pinerc_changes = 1;
	}
	

	if(ps->post_prc && !unix_color_style_in_pinerc(ps->post_prc)){
	    conv_post = convert_pc_gray_names(ps, ps->post_prc, Post);
	    if(conv_post)
	      ps->post_prc->outstanding_pinerc_changes = 1;
	}
	
	if(conv_main || conv_post){
	    if(conv_main)
	      write_pinerc(ps, Main, WRP_NONE);

	    if(conv_post)
	      write_pinerc(ps, Post, WRP_NONE);

	    set_current_color_vals(ps);
	}
    }
#endif	/* _WINDOWS */

    pico_set_normal_color();
}


/*
 * Set current_val for the foreground and background color vars, which
 * are assumed to be in order. If a set_current_val on them doesn't
 * produce current_vals, then use the colors from defvar to set those
 * current_vals.
 */
void
set_color_val(v, use_default)
    struct variable *v;
    int              use_default;
{
    set_current_val(v, TRUE, TRUE);
    set_current_val(v+1, TRUE, TRUE);

    if(!(v->current_val.p && v->current_val.p[0] &&
         (v+1)->current_val.p && (v+1)->current_val.p[0])){
	struct variable *defvar;

	if(v->current_val.p)
	  fs_give((void **)&v->current_val.p);
	if((v+1)->current_val.p)
	  fs_give((void **)&(v+1)->current_val.p);

	if(!use_default)
	  return;

	if(var_defaults_to_rev(v))
	  defvar = &ps_global->vars[V_REV_FORE_COLOR];
	else
	  defvar = &ps_global->vars[V_NORM_FORE_COLOR];

	/* use default vars values instead */
	if(defvar && defvar->current_val.p && defvar->current_val.p[0] &&
           (defvar+1)->current_val.p && (defvar+1)->current_val.p[0]){
	    v->current_val.p = cpystr(defvar->current_val.p);
	    (v+1)->current_val.p = cpystr((defvar+1)->current_val.p);
	}
    }
}


int
var_defaults_to_rev(v)
    struct variable *v;
{
    return(v == &ps_global->vars[V_REV_FORE_COLOR] ||
	   v == &ps_global->vars[V_TITLE_FORE_COLOR] ||
	   v == &ps_global->vars[V_STATUS_FORE_COLOR] ||
	   v == &ps_global->vars[V_KEYNAME_FORE_COLOR] ||
	   v == &ps_global->vars[V_PROMPT_FORE_COLOR]);
}


/*
 * Each item in the list looks like:
 *
 *  /HDR=<header>/FG=<foreground color>/BG=<background color>
 *
 * We separate the three pieces into an array of structures to make
 * it easier to deal with later.
 */
void
set_custom_spec_colors(ps)
    struct pine *ps;
{
    if(ps->hdr_colors)
      free_spec_colors(&ps->hdr_colors);

    ps->hdr_colors = spec_colors_from_varlist(ps->VAR_VIEW_HDR_COLORS, 1);

    /* fit keyword colors into the same structures for code re-use */
    if(ps->kw_colors)
      free_spec_colors(&ps->kw_colors);

    ps->kw_colors = spec_colors_from_varlist(ps->VAR_KW_COLORS, 1);
}


/*
 * Input is one item from config variable.
 *
 * Return value must be freed by caller. The return is a single SPEC_COLOR_S,
 * not a list.
 */
SPEC_COLOR_S *
spec_color_from_var(t, already_expanded)
    char *t;
    int   already_expanded;
{
    char        *p, *spec, *fg, *bg;
    PATTERN_S   *val;
    SPEC_COLOR_S *new_hcolor = NULL;

    if(t && t[0] && !strcmp(t, INHERIT)){
	new_hcolor = (SPEC_COLOR_S *)fs_get(sizeof(*new_hcolor));
	memset((void *)new_hcolor, 0, sizeof(*new_hcolor));
	new_hcolor->inherit = 1;
    }
    else if(t && t[0]){
	char tbuf[10000];

	if(!already_expanded){
	    tbuf[0] = '\0';
	    if(expand_variables(tbuf, sizeof(tbuf), t, 0))
	      t = tbuf;
	}

	spec = fg = bg = NULL;
	val = NULL;
	if((p = srchstr(t, "/HDR=")) != NULL)
	  spec = remove_backslash_escapes(p+5);
	if((p = srchstr(t, "/FG=")) != NULL)
	  fg = remove_backslash_escapes(p+4);
	if((p = srchstr(t, "/BG=")) != NULL)
	  bg = remove_backslash_escapes(p+4);
	val = parse_pattern("VAL", t, 0);
	
	if(spec && *spec){
	    /* remove colons */
	    if((p = strindex(spec, ':')) != NULL)
	      *p = '\0';

	    new_hcolor = (SPEC_COLOR_S *)fs_get(sizeof(*new_hcolor));
	    memset((void *)new_hcolor, 0, sizeof(*new_hcolor));
	    new_hcolor->spec = spec;
	    new_hcolor->fg   = fg;
	    new_hcolor->bg   = bg;
	    new_hcolor->val  = val;
	}
	else{
	    if(spec)
	      fs_give((void **)&spec);
	    if(fg)
	      fs_give((void **)&fg);
	    if(bg)
	      fs_give((void **)&bg);
	    if(val)
	      free_pattern(&val);
	}
    }

    return(new_hcolor);
}


/*
 * Input is a list from config file.
 *
 * Return value may be a list of SPEC_COLOR_S and must be freed by caller.
 */
SPEC_COLOR_S *
spec_colors_from_varlist(varlist, already_expanded)
    char **varlist;
    int    already_expanded;
{
    char        **s, *t;
    SPEC_COLOR_S *new_hc = NULL;
    SPEC_COLOR_S *new_hcolor, **nexthc;

    nexthc = &new_hc;
    if(varlist){
	for(s = varlist; (t = *s) != NULL; s++){
	    if(t[0]){
		new_hcolor = spec_color_from_var(t, already_expanded);
		if(new_hcolor){
		    *nexthc = new_hcolor;
		    nexthc = &new_hcolor->next;
		}
	    }
	}
    }

    return(new_hc);
}


/*
 * Returns allocated charstar suitable for config var for a single
 * SPEC_COLOR_S.
 */
char *
var_from_spec_color(hc)
    SPEC_COLOR_S *hc;
{
    char *ret_val = NULL;
    char *p, *spec = NULL, *fg = NULL, *bg = NULL, *val = NULL;
    size_t len;

    if(hc && hc->inherit)
      ret_val = cpystr(INHERIT);
    else if(hc){
	if(hc->spec)
	  spec = add_viewerhdr_escapes(hc->spec);
	if(hc->fg)
	  fg = add_viewerhdr_escapes(hc->fg);
	if(hc->bg)
	  bg = add_viewerhdr_escapes(hc->bg);
	if(hc->val){
	    p = pattern_to_string(hc->val);
	    if(p){
		val = add_viewerhdr_escapes(p);
		fs_give((void **)&p);
	    }
	}

	len = strlen("/HDR=/FG=/BG=") + strlen(spec ? spec : "") +
	      strlen(fg ? fg : "") + strlen(bg ? bg : "") +
	      strlen(val ? "/VAL=" : "") + strlen(val ? val : "");
	ret_val = (char *) fs_get(len + 1);
	sprintf(ret_val, "/HDR=%s/FG=%s/BG=%s%s%s",
		spec ? spec : "", fg ? fg : "", bg ? bg : "",
		val ? "/VAL=" : "", val ? val : "");

	if(spec)
	  fs_give((void **)&spec);
	if(fg)
	  fs_give((void **)&fg);
	if(bg)
	  fs_give((void **)&bg);
	if(val)
	  fs_give((void **)&val);
    }

    return(ret_val);
}


/*
 * Returns allocated charstar suitable for config var for a single
 * SPEC_COLOR_S.
 */
char **
varlist_from_spec_colors(hcolors)
    SPEC_COLOR_S *hcolors;
{
    SPEC_COLOR_S *hc;
    char       **ret_val = NULL;
    int          i;

    /* count how many */
    for(hc = hcolors, i = 0; hc; hc = hc->next, i++)
      ;
    
    ret_val = (char **)fs_get((i+1) * sizeof(*ret_val));
    memset((void *)ret_val, 0, (i+1) * sizeof(*ret_val));
    for(hc = hcolors, i = 0; hc; hc = hc->next, i++)
      ret_val[i] = var_from_spec_color(hc);
    
    return(ret_val);
}


/*
 * Returns positive if any thing was actually deleted.
 */
int
delete_user_vals(v)
    struct variable *v;
{
    int rv = 0;

    if(v){
	if(v->is_list){
	    if(v->post_user_val.l){
		rv++;
		free_list_array(&v->post_user_val.l);
	    }
	    if(v->main_user_val.l){
		rv++;
		free_list_array(&v->main_user_val.l);
	    }
	}
	else{
	    if(v->post_user_val.p){
		rv++;
		fs_give((void **)&v->post_user_val.p);
	    }
	    if(v->main_user_val.p){
		rv++;
		fs_give((void **)&v->main_user_val.p);
	    }
	}
    }

    return(rv);
}


#ifdef	_WINDOWS

char *
transformed_color(old)
    char *old;
{
    if(!old)
      return("");

    if(!struncmp(old, "color008", 8))
      return("colorlgr");
    else if(!struncmp(old, "color009", 8))
      return("colormgr");
    else if(!struncmp(old, "color010", 8))
      return("colordgr");

    return("");
}


/*
 * If this is the first time we've run a version > 4.40, and there
 * is evidence that the config file has not been used by unix pine,
 * then we convert color008 to colorlgr, color009 to colormgr, and
 * color010 to colordgr. If the config file is being used by
 * unix pine then color008 may really supposed to be color008, color009
 * may really supposed to be red, and color010 may really supposed to be
 * green. Same if we've already run 4.41 or higher previously.
 *
 * Returns 0 if no changes, > 0 if something was changed.
 */
int
convert_pc_gray_names(ps, prc, which)
    struct pine *ps;
    PINERC_S    *prc;
    EditWhich    which;
{
    struct variable *v;
    int              ret = 0, ic = 0;
    char           **s, *t, *p, *pstr, *new, *pval, **apval, **lval;

    for(v = ps->vars; v->name; v++){
	if(!color_holding_var(ps, v) || v == &ps->vars[V_KW_COLORS])
	  continue;
	
	if(v == &ps->vars[V_VIEW_HDR_COLORS]){

	    if((lval = LVAL(v,which)) != NULL){
		/* fix these in place */
		for(s = lval; (t = *s) != NULL; s++){
		    if((p = srchstr(t, "FG=color008")) ||
		       (p = srchstr(t, "FG=color009")) ||
		       (p = srchstr(t, "FG=color010"))){
			strncpy(p+3, transformed_color(p+3), 8);
			ret++;
		    }

		    if((p = srchstr(t, "BG=color008")) ||
		       (p = srchstr(t, "BG=color009")) ||
		       (p = srchstr(t, "BG=color010"))){
			strncpy(p+3, transformed_color(p+3), 8);
			ret++;
		    }
		}
	    }
	}
	else{
	    if((pval = PVAL(v,which)) != NULL){
		apval = APVAL(v,which);
		if(apval && (!strucmp(pval, "color008") ||
		             !strucmp(pval, "color009") ||
		             !strucmp(pval, "color010"))){
		    new = transformed_color(pval);
		    if(*apval)
		      fs_give((void **)apval);

		    *apval = cpystr(new);
		    ret++;
		}
	    }
	}
    }

    v = &ps->vars[V_PAT_INCOLS];
    if((lval = LVAL(v,which)) != NULL){
	for(s = lval; (t = *s) != NULL; s++){
	    if((pstr = srchstr(t, "action=")) != NULL){
		if((p = srchstr(pstr, "FG=color008")) ||
		   (p = srchstr(pstr, "FG=color009")) ||
		   (p = srchstr(pstr, "FG=color010"))){
		    strncpy(p+3, transformed_color(p+3), 8);
		    ic++;
		}

		if((p = srchstr(pstr, "BG=color008")) ||
		   (p = srchstr(pstr, "BG=color009")) ||
		   (p = srchstr(pstr, "BG=color010"))){
		    strncpy(p+3, transformed_color(p+3), 8);
		    ic++;
		}
	    }
	}
    }

    if(ic)
      set_current_val(&ps->vars[V_PAT_INCOLS], TRUE, TRUE);

    return(ret+ic);
}


int
unix_color_style_in_pinerc(prc)
    PINERC_S *prc;
{
    PINERC_LINE *pline;

    for(pline = prc ? prc->pinerc_lines : NULL;
	pline && (pline->var || pline->line); pline++)
      if(pline->line && !struncmp("color-style=", pline->line, 12))
	return(1);
    
    return(0);
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
config_scroll_callback (cmd, scroll_pos)
int	cmd;
long	scroll_pos;
{   
    switch (cmd) {
      case MSWIN_KEY_SCROLLUPLINE:
	config_scroll_down (scroll_pos);
	break;

      case MSWIN_KEY_SCROLLDOWNLINE:
	config_scroll_up (scroll_pos);
	break;

      case MSWIN_KEY_SCROLLUPPAGE:
	config_scroll_down (BODY_LINES(ps_global));
	break;

      case MSWIN_KEY_SCROLLDOWNPAGE:
	config_scroll_up (BODY_LINES(ps_global));
	break;

      case MSWIN_KEY_SCROLLTO:
	config_scroll_to_pos (scroll_pos);
	break;
    }

    option_screen_redrawer();
    fflush(stdout);

    return(TRUE);
}
#endif	/* _WINDOWS */
