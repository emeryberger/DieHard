#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: folder.c 14081 2005-09-12 22:04:25Z hubert@u.washington.edu $";
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
     folder.c

  Screen to display and manage all the users folders

This puts up a list of all the folders in the users mail directory on
the screen spacing it nicely. The arrow keys move from one to another
and the user can delete the folder or select it to change to or copy a
message to. The dispay lets the user scroll up or down a screen full,
or search for a folder name.
 ====*/


#include "headers.h"


#define	CLICKHERE	"[ Select Here to See Expanded List ]"
#define	CLICKHERETOO	"[ ** Empty List **  Select Here to Try Re-Expanding ]"
#define	CLICKHERETOONEWS \
	"[ ** Empty List **  Use \"A Subscribe\" to subscribe to a newsgroup ]"
#define	ALL_FOUND(X)	(((X)->dir->status & CNTXT_NOFIND) == 0 && \
			  ((X)->dir->status & CNTXT_PARTFIND) == 0)
#define	FLDR_ENT(S)	(((S) && (S)->text.handles) \
			 ? folder_entry((S)->text.handles->h.f.index, \
				FOLDERS((S)->text.handles->h.f.context)) \
			 : NULL)
#define	SUBSCRIBE_PMT	\
		       "Enter newsgroup name (or partial name to get a list): "
#define	LISTMODE_GRIPE	"Use \"X\" to mark selections in list mode"
#define	SEL_ALTER_PMT	"ALTER folder selection : "
#define	SEL_TEXT_PMT	"Select by folder Name or Contents ? "
#define	SEL_PROP_PMT	"Select by which folder property ? "
#define DIR_FOLD_PMT \
	    "Folder and directory of the same name will be deleted.  Continue"

#define	mail_list(S, R, N)	mail_list_internal(S, R, N)

/*
 * folder name completion routine flags
 */
#define	FC_NONE		0
#define	FC_FORCE_LIST	1


/*
 * folder_list_write
 */
#define	FLW_NONE	0x00
#define	FLW_LUNK	0x01
#define	FLW_SLCT	0x02
#define	FLW_LIST	0x04




/*----------------------------------------------------------------------
   The data needed to redraw the folders screen, including the case where the 
screen changes size in which case it may recalculate the folder_display.
  ----*/


/*
 * Struct managing folder_lister arguments and holding state
 * for various internal methods
 */
typedef struct _folder_screen {
    CONTEXT_S	    *context;		/* current collection		  */
    CONTEXT_S	    *list_cntxt;	/* list mode collection		  */
    MAILSTREAM     **cache_streamp;     /* cached mailstream              */
    char	     first_folder[MAXFOLDER];
    unsigned	     first_dir:1;	/* first_folder is a dir	  */
    unsigned	     combined_view:1;	/* display flat folder list	  */
    unsigned	     no_dirs:1;		/* no dirs in this screen	  */
    unsigned	     no_empty_dirs:1;	/* no empty dirs on this screen	  */
    unsigned	     relative_path:1;	/* return fully-qual'd specs	  */
    unsigned	     save_sel:1;
    unsigned	     force_intro:1;
    unsigned	     agg_ops:1;
    struct key_menu *km;		/* key label/command bindings	  */
    struct _func_dispatch {
	int	 (*valid) PROTO((FOLDER_S *, struct _folder_screen *));
	struct {
	    HelpType  text;
	    char     *title;
	} help;
	struct {
	    char	 *bar;
	    TitleBarType  style;
	} title;
    } f;
} FSTATE_S;


/*
 * Struct mananging folder_lister metadata as it get's passed
 * in and back up thru scrolltool
 */
typedef struct _folder_proc {
    FSTATE_S   *fs;
    STRLIST_S  *rv;
    unsigned	done:1;			/* done listing folders?... */
    unsigned	all_done:1;		/* ...and will list no more forever */
} FPROC_S;

#define	FPROC(X)	((FPROC_S *)(X)->proc.data.p)


/*
 * Structs to ease c-client LIST/LSUB interaction
 */
typedef struct _listargs {
    char *reference,		/* IMAP LIST "reference" arg	    */
	 *name,			/* IMAP LIST "name" arg		    */
	 *tail;			/* Pine "context" after "name" part */
} LISTARGS_S;


typedef struct _listresponse {
    long     count;
    int	     delim;
    unsigned isfile:1,
	     isdir:1,
	     ismarked:1,
	     unmarked:1,
	     haschildren:1,
	     hasnochildren:1;
} LISTRES_S;


typedef struct _build_folder_list_data {
    long	 mask;			/* bitmap of responses to ignore    */
    LISTARGS_S	 args;
    LISTRES_S	 response;
    int          is_move_folder;
    void	*list;
} BFL_DATA_S;


typedef struct _existdata {
    LISTARGS_S	 args;
    LISTRES_S	 response;
    char       **fullname;
    int          is_move_folder;
} EXISTDATA_S;


typedef struct _scanarg {
    MAILSTREAM  *stream;	
    int		 newstream;
    CONTEXT_S	*context;
    char	*pattern;
    char	 type;
} SCANARG_S;


typedef struct _statarg {
    MAILSTREAM  *stream;	
    int		 newstream;
    CONTEXT_S	*context;
    long	 flags;
    long	 nmsgs;
    int		 cmp;
} STATARG_S;



typedef enum {NotChecked, NotInCache, Found, Missing, End} NgCacheReturns;


/* short definition to keep compilers happy */
typedef    int (*QSFunc) PROTO((const QSType *, const QSType *));


/*
 * Internal prototypes
 */
CONTEXT_S *context_screen PROTO((CONTEXT_S *, struct key_menu *, int));
STRLIST_S *folder_lister PROTO((struct pine *, FSTATE_S *));
int	   folder_list_text PROTO((struct pine *, FPROC_S *,
				   gf_io_t, HANDLE_S **, int));
int	   folder_list_write_folder PROTO((gf_io_t, CONTEXT_S *,
					   int, char *, int));
int	   folder_list_write_prefix PROTO((FOLDER_S *, int, gf_io_t));
int	   folder_list_ith PROTO((int, CONTEXT_S *));
char	  *folder_list_center_space PROTO((char *, int));
HANDLE_S  *folder_list_handle PROTO((FSTATE_S *, HANDLE_S *));
int	   folder_processor PROTO((int, MSGNO_S *, SCROLL_S *));
int	   folder_lister_choice PROTO((SCROLL_S *));
int        folder_lister_clickclick PROTO((SCROLL_S *));
int	   folder_lister_finish PROTO((SCROLL_S *, CONTEXT_S *, int));
int	   folder_lister_addmanually PROTO((SCROLL_S *));
void	   folder_lister_km_manager PROTO((SCROLL_S *, int));
void	   folder_lister_km_sel_manager PROTO((SCROLL_S *, int));
void	   folder_lister_km_sub_manager PROTO((SCROLL_S *, int));
int	   folder_lister_select PROTO((FSTATE_S *, CONTEXT_S *, int));
int	   folder_lister_parent PROTO((FSTATE_S *, CONTEXT_S *, int, int));
char	  *folder_lister_desc PROTO((CONTEXT_S *, FDIR_S *));
char	  *folder_lister_fullname PROTO((FSTATE_S *, char *));
void       folder_export PROTO((SCROLL_S *));
int        folder_import PROTO((SCROLL_S *, char *, size_t));
void	   folder_sublist_context PROTO((char *, CONTEXT_S *, CONTEXT_S *,
					 FDIR_S **, int));
int	   folder_selector PROTO((struct pine *, FSTATE_S *,
				  char *, CONTEXT_S **));
char	  *exit_collection_add PROTO((struct headerentry *, void (*)(), int));
char	  *cancel_collection_add PROTO((char *, void (*)()));
int	   build_namespace PROTO((char *, char **, char **,
				  BUILDER_ARG *, int *));
int	   fl_val_gen PROTO((FOLDER_S *, FSTATE_S *));
int	   fl_val_writable PROTO((FOLDER_S *, FSTATE_S *));
int	   fl_val_subscribe PROTO((FOLDER_S *, FSTATE_S *));
void       redraw_folder_lister PROTO((void));
void       display_folder PROTO((FSTATE_S *, int));
int	   folder_select PROTO((struct pine *, CONTEXT_S *, int));
int	   folder_select_restore PROTO((CONTEXT_S *));
void	   folder_select_preserve PROTO((CONTEXT_S *));
int	   selected_folders PROTO((CONTEXT_S *));
int	   folder_insert_sorted PROTO((int, int, int, FOLDER_S *, void *,
				       int (*) PROTO((FOLDER_S *,
						      FOLDER_S *))));
void       folder_insert_index PROTO((FOLDER_S *, int, void *));
int        swap_incoming_folders PROTO((int, int, void *));
int        folder_total PROTO((void *));
int	   shuffle_incoming_folders PROTO((CONTEXT_S *, int));
int	   group_subscription PROTO((char *, size_t, CONTEXT_S *));
STRLIST_S *folders_for_subscribe PROTO((struct pine *, CONTEXT_S *, char *));
int	   rename_folder PROTO((CONTEXT_S *, int, char *, size_t, MAILSTREAM *));
int        delete_folder PROTO((CONTEXT_S *, int, char *, size_t, MAILSTREAM **));
EditWhich  config_containing_inc_fldr PROTO((FOLDER_S *));
void       init_incoming_folder_list PROTO((struct pine *, CONTEXT_S *));
void       reinit_incoming_folder_list PROTO((struct pine *, CONTEXT_S *));
void       print_folders PROTO((FPROC_S *));
int        search_folders PROTO((FSTATE_S *, int));
int        compare_names PROTO((const QSType *, const QSType *));
int	   compare_folders_alpha PROTO((FOLDER_S *, FOLDER_S *));
int	   compare_folders_dir_alpha PROTO((FOLDER_S *, FOLDER_S *));
int	   compare_folders_alpha_dir PROTO((FOLDER_S *, FOLDER_S *));
void      *init_folder_entries PROTO(());
void	   refresh_folder_list PROTO((CONTEXT_S *, int, int, MAILSTREAM **));
void       free_folder_entries PROTO((void **));
FDIR_S	  *new_fdir PROTO((char *, char *, int));
void	   free_fdir PROTO((FDIR_S **, int));
int	   update_bboard_spec PROTO((char *, char *));
void       folder_delete PROTO((int, void *));
int	   folder_complete_internal PROTO((CONTEXT_S *, char *, int *, int));
char      *get_post_list PROTO((char **));
NgCacheReturns chk_newsgrp_cache PROTO((char *));
void       add_newsgrp_cache PROTO((char *, NgCacheReturns));
char	  *folder_last_cmpnt PROTO((char *, int));
int	   folder_select_toggle PROTO((CONTEXT_S *, int,
				       int (*) PROTO((CONTEXT_S *, int))));
MAILSTREAM *mail_cmd_stream PROTO((CONTEXT_S *, int *));
FDIR_S	  *next_folder_dir PROTO((CONTEXT_S *, char *, int, MAILSTREAM **));
void	   reset_context_folders PROTO((CONTEXT_S *));
#ifdef	_WINDOWS
int	   folder_scroll_callback PROTO((int,long));
int	   folder_list_popup PROTO((SCROLL_S *, int));
int	   folder_list_select_popup PROTO((SCROLL_S *, int));
void	   folder_popup_config PROTO((FSTATE_S *, struct key_menu *,MPopup *));
#endif
int	   mail_list_in_collection PROTO((char **, char *, char *, char *));
void	   mail_list_filter PROTO((MAILSTREAM *, char *, int, long, void *,
				   unsigned));
void	   mail_lsub_filter PROTO((MAILSTREAM *, char *, int, long, void *,
				   unsigned));
void	   mail_list_exists PROTO((MAILSTREAM *, char *, int, long, void *,
				   unsigned));
void	   mail_list_response PROTO((MAILSTREAM *, char *, int, long, void *,
				     unsigned));
int	   folder_delimiter PROTO((char *));
int	   folder_select_props PROTO((struct pine *, CONTEXT_S *, int));
int	   folder_select_count PROTO((long *, int *));
int	   folder_select_text PROTO((struct pine *, CONTEXT_S *, int));
int	   foreach_folder PROTO((CONTEXT_S *, int,
				 int (*) PROTO((FOLDER_S *, void *)), void *));
int	   foreach_do_scan PROTO((FOLDER_S *, void *));
int	   scan_get_pattern PROTO((char *, char *, int));
int	   scan_scan_folder PROTO((MAILSTREAM *, CONTEXT_S *,
				   FOLDER_S *, char *));
int	   foreach_do_stat PROTO((FOLDER_S *, void *));
int        pine_mail_list PROTO((MAILSTREAM *, char *, char *, unsigned *));
void	   mail_list_internal PROTO((MAILSTREAM *, char *, char *));
int        percent_formatted PROTO((void));
char      *end_bracket_no_nest PROTO((char *));
#ifdef	DEBUG
void	   dump_contexts PROTO((FILE *fp));
#endif


/*
 * Various screen keymenu/command binding s.
 */
#define PREVC_MENU {"P", "PrevCltn",   {MC_PREVITEM, 1, {'p'}}, KS_NONE}
#define NEXTC_MENU {"N", "NextCltn",   {MC_NEXTITEM, 2, {'n',TAB}}, KS_NONE}
#define	DELC_MENU  {"D", "Del Cltn",   {MC_DELETE,2,{'d',KEY_DEL}}, KS_NONE}
#define PREVF_MENU {"P", "PrevFldr",   {MC_PREV_HANDLE, 3, \
					  {'p', ctrl('B'), KEY_LEFT}}, KS_NONE}
#define NEXTF_MENU {"N", "NextFldr",   {MC_NEXT_HANDLE, 4, \
					  {'n', ctrl('F'), TAB, KEY_RIGHT}}, \
					  KS_NONE}
#define	CIND_MENU  {"I", "CurIndex",   {MC_INDEX,1,{'i'}}, KS_FLDRINDEX}

static struct {
       int num_done;
       int total;
} percent;
  
static struct key context_mgr_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"<", "Main Menu", {MC_MAIN,3,{'m','<',','}}, KS_EXITMODE},
        {">", "[View Cltn]",
	 {MC_CHOICE,5,{'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVC_MENU,
	NEXTC_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	QUIT_MENU,
	NULL_MENU,
	NULL_MENU,
	GOTO_MENU,
	CIND_MENU,
	COMPOSE_MENU,
	PRYNTTXT_MENU,
	RCOMPOSE_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(c_mgr_km, context_mgr_keys);


static struct key context_cfg_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"E", "Exit Setup", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"C", "[Change]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVC_MENU,
	NEXTC_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", "Add Cltn", {MC_ADD,1,{'a'}}, KS_NONE},
	DELC_MENU,
	{"$", "Shuffle", {MC_SHUFFLE,1,{'$'}},KS_NONE},
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(c_cfg_km, context_cfg_keys);

static struct key context_select_keys[] = 
       {HELP_MENU,
	{"E", "ExitSelect", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	{">", "[View Cltn]",
	 {MC_CHOICE, 5, {'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVC_MENU,
	NEXTC_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(c_sel_km, context_select_keys);

static struct key context_fcc_keys[] = 
       {HELP_MENU,
	{"E", "ExitSelect", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	{">", "[View Cltn]",
	 {MC_CHOICE, 5, {'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVC_MENU,
	NEXTC_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(c_fcc_km, context_fcc_keys);


static struct key folder_keys[] =
       {HELP_MENU,
  	OTHER_MENU,
	{"<", NULL, {MC_EXIT,3,{0, '<',','}}, KS_NONE},
        {">", NULL, {MC_CHOICE,2,{'>','.'}}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A","Add",{MC_ADDFLDR,1,{'a'}},KS_NONE},
	DELETE_MENU,
	{"R","Rename",{MC_RENAMEFLDR,1,{'r'}}, KS_NONE},
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	QUIT_MENU,
	MAIN_MENU,
	{"V", "[View Fldr]", {MC_OPENFLDR}, KS_NONE},
	GOTO_MENU,
	CIND_MENU,
	COMPOSE_MENU,
	{"%", "Print", {MC_PRINTFLDR,1,{'%'}}, KS_PRINT},
	{"Z", "ZoomMode", {MC_ZOOM,1,{'z'}}, KS_NONE},
	{";","Select",{MC_SELECT,1,{';'}},KS_SELECT},
	{":","SelectCur",{MC_SELCUR,1,{':'}},KS_SELECT},

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"$", "Shuffle", {MC_SHUFFLE,1,{'$'}},KS_NONE},
	RCOMPOSE_MENU,
	EXPORT_MENU,
	{"U", "Import", {MC_IMPORT,1,{'u'}},KS_NONE},
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(folder_km, folder_keys);
#define	KM_COL_KEY	2
#define	KM_SEL_KEY	3
#define	KM_MAIN_KEY	15
#define	KM_ALTVIEW_KEY	16
#define	KM_ZOOM_KEY	21
#define	KM_SELECT_KEY	22
#define	KM_SELCUR_KEY	23
#define	KM_RECENT_KEY	28
#define	KM_SHUFFLE_KEY	30
#define	KM_EXPORT_KEY	32
#define	KM_IMPORT_KEY	33


static struct key folder_sel_keys[] =
       {HELP_MENU,
	{"E", "ExitSelect", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	{NULL, NULL, {MC_CHOICE,0}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"S", "Select", {MC_OPENFLDR,1,{'s'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(folder_sel_km, folder_sel_keys);

/* add the AddNew command to folder_sel_km */
static struct key folder_sela_keys[] =
       {HELP_MENU,
	{"E", "ExitSelect", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	{NULL, NULL, {MC_CHOICE,0}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"S", "Select", {MC_OPENFLDR,1,{'s'}}, KS_NONE},
	NULL_MENU,
	{"A", "AddNew", {MC_ADD,1,{'a'}}, KS_NONE},
	WHEREIS_MENU};
INST_KEY_MENU(folder_sela_km, folder_sela_keys);
#define	FC_EXIT_KEY	1
#define	FC_COL_KEY	2
#define	FC_SEL_KEY	3
#define	FC_ALTSEL_KEY	8

static struct key folder_sub_keys[] =
       {HELP_MENU,
	{"S", "Subscribe", {MC_CHOICE,1,{'s'}}, KS_NONE},
  	{"E", "ExitSubscb", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {NULL, "[Select]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"L", "List Mode", {MC_LISTMODE, 1, {'l'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(folder_sub_km, folder_sub_keys);
#define	SB_SUB_KEY	1
#define SB_EXIT_KEY     2
#define	SB_SEL_KEY	3
#define	SB_LIST_KEY	8

static struct key folder_post_keys[] =
       {HELP_MENU,
 	NULL_MENU,
	{"E", "ExitSelect", {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"S", "[Select]", {MC_CHOICE, 3, {'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(folder_post_km, folder_post_keys);





/*----------------------------------------------------------------------
      Front end to folder lister when it's called from the main menu

 Args: ps -- The general pine_state data structure

 Result: runs context and folder listers

  ----*/
void
folder_screen(ps)
    struct pine *ps;
{
    int		n = 1;
    CONTEXT_S  *cntxt = ps->context_current;
    STRLIST_S  *folders;
    FSTATE_S    fs;
    MAILSTREAM *cache_stream = NULL;

    dprint(1, (debugfile, "=== folder_screen called ====\n"));
    mailcap_free(); /* free resources we won't be using for a while */
    ps->next_screen = SCREEN_FUN_NULL;

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context		= cntxt;
    fs.cache_streamp    = &cache_stream;
    fs.combined_view	= F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.agg_ops		= F_ON(F_ENABLE_AGG_OPS, ps_global) != 0;
    fs.relative_path	= 1;
    fs.f.valid		= fl_val_gen;
    fs.f.title.bar	= "FOLDER LIST";
    fs.f.title.style    = FolderName;
    fs.f.help.text	= h_folder_maint;
    fs.f.help.title	= "HELP FOR FOLDERS";
    fs.km		= &folder_km;

    if(context_isambig(ps->cur_folder)
       && (IS_REMOTE(ps->cur_folder) || !is_absolute_path(ps->cur_folder)
	   || (cntxt && cntxt->context && cntxt->context[0] == '{'))){
	if(strlen(ps_global->cur_folder) < MAXFOLDER - 1)
	  strcpy(fs.first_folder, ps_global->cur_folder);

	/*
	 * If we're asked to start in the folder list of the current
	 * folder and it looks like the current folder is part of the
	 * current context, try to start in the list of folders in the
	 * current context.
	 */
	if(ps->start_in_context || fs.combined_view){
	    char	tmp[MAILTMPLEN], *p, *q;
	    FDIR_S *fp;

	    ps->start_in_context = 0;
	    n = 0;

	    if(!(NEWS_TEST(cntxt) || (cntxt->use & CNTXT_INCMNG))
	       && cntxt->dir->delim
	       && strchr(ps->cur_folder, cntxt->dir->delim)){
		for(p = strchr(q = ps->cur_folder, cntxt->dir->delim);
		    p;
		    p = strchr(q = ++p, cntxt->dir->delim)){
		    strncpy(tmp, q, min(p - q, sizeof(tmp)-1));
		    tmp[min(p - q, sizeof(tmp)-1)] = '\0';

		    fp = next_folder_dir(cntxt, tmp, FALSE, fs.cache_streamp);

		    fp->desc    = folder_lister_desc(cntxt, fp);

		    /* Insert new directory into list */
		    fp->delim   = cntxt->dir->delim;
		    fp->prev    = cntxt->dir;
		    fp->status |= CNTXT_SUBDIR;
		    cntxt->dir  = fp;
		}
	    }
	}
    }

    while(ps->next_screen == SCREEN_FUN_NULL
	  && ((n++) ? (cntxt = context_screen(cntxt,&c_mgr_km,1)) != NULL :1)){

	fs.context = cntxt;
	if(folders = folder_lister(ps, &fs)){

	    if(do_broach_folder((char *) folders->name, 
				fs.context, fs.cache_streamp 
				&& *fs.cache_streamp ? fs.cache_streamp
				: NULL, 0L) == 1){
		reset_context_folders(ps->context_list);
		ps->next_screen = mail_index_screen;
	    }

	    if(fs.cache_streamp)
	      *fs.cache_streamp = NULL;
	    free_strlist(&folders);
	}
    }

    if(fs.cache_streamp && *fs.cache_streamp)
      pine_mail_close(*fs.cache_streamp);
    ps->prev_screen = folder_screen;
}



/*----------------------------------------------------------------------
      Front end to folder lister when it's called from the main menu

 Args: ps -- The general pine_state data structure

 Result: runs context and folder listers

  ----*/
void
folder_config_screen(ps, edit_exceptions)
    struct pine *ps;
    int          edit_exceptions;
{
    CONT_SCR_S css;
    char title[50], htitle[50];

    dprint(1, (debugfile, "=== folder_config_screen called ====\n"));
    mailcap_free(); /* free resources we won't be using for a while */

    sprintf(title, "SETUP %.15sCOLLECTION LIST",
	    edit_exceptions ? "EXCEPTIONS " : "");
    sprintf(htitle, "HELP FOR SETUP %.15sCOLLECTIONS",
	    edit_exceptions ? "EXCEPTIONS " : "");

    memset(&css, 0, sizeof(CONT_SCR_S));
    css.title	     = title;
    css.print_string = "contexts ";
    css.contexts     = &ps_global->context_list;
    css.help.text    = h_collection_maint;
    css.help.title   = htitle;
    css.keymenu	     = &c_cfg_km;
    css.edit	     = 1;

    /*
     * Use conf_scroll_screen to manage display/selection
     * of contexts
     */
    context_config_screen(ps_global, &css, edit_exceptions);
}



/*----------------------------------------------------------------------
 Browse folders for ^T selection from the Goto Prompt
  
 Args:  ps --
	cntxtp -- pointer to addr of context to start in, list, and return
	folder -- pointer to buffer inwhich to return selected folder

 Returns: 1 if we have something valid in cntxtp and folder
	  0 if problem or user cancelled

  ----*/     
int
folders_for_goto(ps, cntxtp, folder, sublist)
    struct pine	 *ps;
    CONTEXT_S	**cntxtp;
    char	 *folder;
    int		  sublist;
{
    int	       rv;
    CONTEXT_S  fake_context;
    FDIR_S    *fake_dir = NULL;
    FSTATE_S   fs;

    dprint(1, (debugfile, "=== folders_for_goto called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context	     = *cntxtp;
    fs.combined_view = !sublist && F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.f.valid	     = fl_val_gen;
    fs.f.title.bar   = "GOTO: SELECT FOLDER";
    fs.f.title.style = FolderName;
    fs.f.help.text   = h_folder_open;
    fs.f.help.title  = "HELP FOR OPENING FOLDERS";
    fs.km	     = &folder_sel_km;

    /* If we were provided a string,
     * dummy up a context for a substring match
     */
    if(sublist && *folder && context_isambig(folder)){
	if((*cntxtp)->use & CNTXT_INCMNG){
	    q_status_message(SM_ORDER, 0, 3,
			     "All folders displayed for Incoming Collection");
	}
	else{
	    folder_sublist_context(folder, *cntxtp, &fake_context,
				   &fake_dir, sublist);
	    fs.context	     = &fake_context;
	    fs.relative_path = 1;
	    fs.force_intro   = 1;
	    cntxtp	     = &fs.context;
	}
    }

    rv = folder_selector(ps, &fs, folder, cntxtp);

    if(fake_dir)
      free_fdir(&fake_dir, TRUE);

    return(rv);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection from the Save Prompt
  
 Args:  ps --
	cntxtp -- pointer to addr of context to start in, list, and return
	folder -- pointer to buffer inwhich to return selected folder

 Returns: 1 if we have something valid in cntxtp and folder
	  0 if problem or user cancelled

  ----*/
int
folders_for_save(ps, cntxtp, folder, sublist)
    struct pine	 *ps;
    CONTEXT_S	**cntxtp;
    char	 *folder;
    int		  sublist;
{
    int	       rv;
    CONTEXT_S  fake_context;
    FDIR_S    *fake_dir = NULL;
    FSTATE_S   fs;

    dprint(1, (debugfile, "=== folders_for_save called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context	     = *cntxtp;
    fs.combined_view = F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.f.valid	     = fl_val_gen;
    fs.f.title.bar   = "SAVE: SELECT FOLDER";
    fs.f.title.style = MessageNumber;
    fs.f.help.text   = h_folder_save;
    fs.f.help.title  = "HELP FOR SAVING MESSAGES TO FOLDERS";
    fs.km	     = &folder_sela_km;

    /* If we were provided a string,
     * dummy up a context for a substring match
     */
    if(sublist && *folder && context_isambig(folder)){
	if((*cntxtp)->use & CNTXT_INCMNG){
	    q_status_message(SM_ORDER, 0, 3,
			     "All folders displayed for Incoming Collection");
	}
	else{
	    folder_sublist_context(folder, *cntxtp, &fake_context,
				   &fake_dir, sublist);
	    fs.context	     = &fake_context;
	    fs.relative_path = 1;
	    fs.force_intro   = 1;
	    cntxtp	     = &fs.context;
	}
    }

    rv = folder_selector(ps, &fs, folder, cntxtp);

    if(fake_dir)
      free_fdir(&fake_dir, TRUE);

    return(rv);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection from the Subscribe Prompt
  
 Args:  ps --
	cntxtp -- pointer to addr of context to start in, list, and return
	folder -- pointer to buffer inwhich to return selected folder

 Returns: 1 if we have something valid in cntxtp and folder
	  0 if problem or user cancelled

  ----*/
STRLIST_S *
folders_for_subscribe(ps, cntxt, folder)
    struct pine	 *ps;
    CONTEXT_S	 *cntxt;
    char	 *folder;
{
    STRLIST_S	*folders = NULL;
    FSTATE_S	 fs;
    void       (*redraw)();

    dprint(1, (debugfile, "=== folders_for_sub called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context	     = cntxt;
    fs.f.valid	     = fl_val_subscribe;
    fs.f.title.bar   = "SUBSCRIBE: SELECT FOLDER";
    fs.f.title.style = FolderName;
    fs.f.help.text   = h_folder_subscribe;
    fs.f.help.title  = "HELP SELECTING NEWSGROUP TO SUBSCRIBE TO";
    fs.km	     = &folder_sub_km;
    fs.force_intro   = 1;

    fs.context		= cntxt;
    redraw		= ps_global->redrawer;
    folders		= folder_lister(ps, &fs);
    ps_global->redrawer = redraw;
    if(ps_global->redrawer)
      (*ps_global->redrawer)();

    return(folders);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection for posting
  
 Args:  ps --
	cntxtp -- pointer to addr of context to start in, list, and return
	folder -- pointer to buffer inwhich to return selected folder

 Returns: 1 if we have something valid in cntxtp and folder
	  0 if problem or user cancelled

  ----*/
int
folders_for_post(ps, cntxtp, folder)
    struct pine	 *ps;
    CONTEXT_S	**cntxtp;
    char	 *folder;
{
    FSTATE_S fs;

    dprint(1, (debugfile, "=== folders_for_post called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context	     = *cntxtp;
    fs.f.valid	     = fl_val_subscribe;
    fs.f.title.bar   = "NEWS: SELECT GROUP";
    fs.f.title.style = FolderName;
    fs.f.help.text   = h_folder_postnews;
    fs.f.help.title  = "HELP FOR SELECTING NEWSGROUP TO POST TO";
    fs.km	     = &folder_post_km;

    return(folder_selector(ps, &fs, folder, cntxtp));
}


int
folder_selector(ps, fs, folder, cntxtp)
    struct pine  *ps;
    FSTATE_S	 *fs;
    char         *folder;
    CONTEXT_S   **cntxtp;
{
    int	       rv = 0;
    STRLIST_S *folders;

    do{
	fs->context = *cntxtp;
	if(folders = folder_lister(ps, fs)){
	    strncpy(folder, (char *) folders->name, MAILTMPLEN-1);
	    folder[MAILTMPLEN-1] = '\0';
	    free_strlist(&folders);
	    *cntxtp = fs->context;
	    rv++;
	    break;
	}
	else if(!(fs->context
		  && (fs->context->next || fs->context->prev))
		|| fs->combined_view)
	  break;
    }
    while(*cntxtp = context_screen(*cntxtp, &c_sel_km, 0));

    return(rv);
}


void
folder_sublist_context(folder, cntxt, new_cntxt, new_dir, lev)
    char       *folder;
    CONTEXT_S  *cntxt, *new_cntxt;
    FDIR_S    **new_dir;
    int	        lev;
{
    char *p, *q, *ref, *wildcard;

    *new_cntxt		   = *cntxt;
    new_cntxt->next = new_cntxt->prev = NULL;
    new_cntxt->dir  = *new_dir = (FDIR_S *) fs_get(sizeof(FDIR_S));
    memset(*new_dir, 0, sizeof(FDIR_S));
    (*new_dir)->status |= CNTXT_NOFIND;
    (*new_dir)->folders	= init_folder_entries();
    if(!((*new_dir)->delim = cntxt->dir->delim)){
	/* simple LIST to acquire delimiter, doh */
	build_folder_list(NULL, new_cntxt, "", NULL,
			  NEWS_TEST(new_cntxt) ? BFL_LSUB : BFL_NONE);
	new_cntxt->dir->status = CNTXT_NOFIND;
    }

    wildcard = NEWS_TEST(new_cntxt) ? "*" : "%";

    if(p = strrindex(folder, (*new_dir)->delim)){
	sprintf(tmp_20k_buf, "%s%s", p + 1, wildcard);
	(*new_dir)->view.internal = cpystr(tmp_20k_buf);
	for(ref = tmp_20k_buf, q = new_cntxt->context;
	    (*ref = *q) != '\0' && !(*q == '%' && *(q+1) == 's');
	    ref++, q++)
	  ;

	for(q = folder; q <= p; q++, ref++)
	  *ref = *q;

	*ref = '\0';
	(*new_dir)->ref = cpystr(tmp_20k_buf);

	(*new_dir)->status |= CNTXT_SUBDIR;
    }
    else{
	sprintf(tmp_20k_buf, "%s%s%s",
		(lev > 1) ? wildcard : "", folder, wildcard);
	(*new_dir)->view.internal = cpystr(tmp_20k_buf);
	/* leave (*new_dir)->ref == NULL */
    }

    sprintf(tmp_20k_buf, "List of folders matching \"%s*\"", folder);
    (*new_dir)->desc = cpystr(tmp_20k_buf);
}



/*----------------------------------------------------------------------
 Browse folders for ^T selection from the composer
  
 Args: error_mess -- pointer to place to return an error message
  
 Returns: result if folder selected, NULL if not
	  Composer expects the result to be alloc'd here 
 
  ----*/     
char *
folders_for_fcc(errmsg)
    char **errmsg;
{
    char      *rs = NULL;
    STRLIST_S *folders;
    FSTATE_S   fs;

    dprint(1, (debugfile, "=== folders_for_fcc called ====\n"));

    /* Coming back from composer */
    fix_windsize(ps_global);
    init_sigwinch();

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.combined_view = F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.f.valid	     = fl_val_gen;
    fs.f.title.bar   = "FCC: SELECT FOLDER";
    fs.f.title.style = FolderName;
    fs.f.help.text   = h_folder_fcc;
    fs.f.help.title  = "HELP FOR SELECTING THE FCC";
    fs.km	     = &folder_sela_km;

    /* start in the default save context */
    fs.context = default_save_context(ps_global->context_list);

    do{
	if(folders = folder_lister(ps_global, &fs)){
	    char *name;

	    /* replace nickname with full name */
	    if(!(name = folder_is_nick((char *) folders->name,
				       FOLDERS(fs.context), 0)))
	      name = (char *) folders->name;

	    if(context_isambig(name) && !((fs.context->use) & CNTXT_SAVEDFLT)){
		char path_in_context[MAILTMPLEN];

		context_apply(path_in_context, fs.context, name,
			      sizeof(path_in_context));
		if(!(IS_REMOTE(path_in_context)
		     || is_absolute_path(path_in_context))){
		    /*
		     * Name is relative to the home directory,
		     * so have to add that. Otherwise, the sender will
		     * assume it is in the primary collection since it
		     * will still be ambiguous.
		     */
		    build_path(tmp_20k_buf, ps_global->ui.homedir,
			       path_in_context, SIZEOF_20KBUF);
		    rs = cpystr(tmp_20k_buf);
		}
		else
		  rs = cpystr(path_in_context);
	    }
	    else
	      rs = cpystr(name);

	    free_strlist(&folders);
	    break;
	}
	else if(!(fs.context && (fs.context->next || fs.context->prev))
		|| fs.combined_view)
	  break;
    }
    while(fs.context = context_screen(fs.context, &c_fcc_km, 0));

    return(rs);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection from the role editor

 Returns: result if folder selected, NULL if not
	  Tesult is alloc'd here 
 
  ----*/     
char *
folders_for_roles(flags)
    int flags;
{
    char      *rs = NULL;
    STRLIST_S *folders;
    FSTATE_S   fs;

    dprint(1, (debugfile, "=== folders_for_roles called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.combined_view = F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.f.valid	     = fl_val_gen;
    fs.f.title.bar   = "SELECT FOLDER";
    fs.f.title.style = FolderName;
    fs.km	     = &folder_sela_km;
    if(flags & FOR_PATTERN){
	fs.f.help.text   = h_folder_pattern_roles;
	fs.f.help.title  = "HELP FOR SELECTING CURRENT FOLDER";
    }
    else{
	fs.f.help.text   = h_folder_action_roles;
	fs.f.help.title  = "HELP FOR SELECTING FOLDER";
    }

    /* start in the current context */
    fs.context = ps_global->context_current;

    do{
	if(folders = folder_lister(ps_global, &fs)){
	    char *name = NULL;

	    /* replace nickname with full name */
	    if(!(flags & FOR_PATTERN))
	      name = folder_is_nick((char *) folders->name,
				    FOLDERS(fs.context), 0);

	    if(!name)
	      name = (char *) folders->name;

	    if(context_isambig(name) &&
	       !(flags & FOR_PATTERN &&
	         folder_is_nick(name, FOLDERS(fs.context), 0))){
		char path_in_context[MAILTMPLEN];

		context_apply(path_in_context, fs.context, name,
			      sizeof(path_in_context));

		/*
		 * We may still have a non-fully-qualified name. In the
		 * action case, that will be interpreted in the primary
		 * collection instead of as a local name.
		 * Qualify that name the same way we
		 * qualify it in match_pattern_folder_specific.
		 */
		if(!(IS_REMOTE(path_in_context) ||
		     path_in_context[0] == '#')){
		    if(strlen(path_in_context) < (MAILTMPLEN/2)){
			char  tmp[max(MAILTMPLEN,NETMAXMBX)];
			char *t;

			t = mailboxfile(tmp, path_in_context);
			rs = cpystr(t);
		    }
		    else{	/* the else part should never happen */
			build_path(tmp_20k_buf, ps_global->ui.homedir,
				   path_in_context, SIZEOF_20KBUF);
			rs = cpystr(tmp_20k_buf);
		    }
		}
		else
		  rs = cpystr(path_in_context);
	    }
	    else
	      rs = cpystr(name);

	    free_strlist(&folders);
	    break;
	}
	else if(!(fs.context && (fs.context->next || fs.context->prev))
		|| fs.combined_view)
	  break;
    }
    while(fs.context = context_screen(fs.context, &c_fcc_km, 0));

    return(rs);
}



/*
 * offer screen with list of contexts to select and some sort
 * of descriptions
 */
CONTEXT_S *
context_screen(start, km, edit_config)
    CONTEXT_S	     *start;
    struct key_menu  *km;
    int		      edit_config;
{
    /* If a list, let the user tell us what to do */
    if(F_OFF(F_CMBND_FOLDER_DISP, ps_global)
       && ps_global->context_list
       && ps_global->context_list->next){
	CONT_SCR_S css;

	memset(&css, 0, sizeof(CONT_SCR_S));
	css.title	 = "COLLECTION LIST";
	css.print_string = "contexts ";
	css.start        = start;
	css.contexts	 = &ps_global->context_list;
	css.help.text	 = h_collection_screen;
	css.help.title   = "HELP FOR COLLECTION LIST";
	css.keymenu	 = km;
	css.edit	 = edit_config;

	/*
	 * Use conf_scroll_screen to manage display/selection
	 * of contexts
	 */
	return(context_select_screen(ps_global, &css, 0));
    }

    return(ps_global->context_list);
}




static struct headerentry headents_templ[]={
  {"Nickname  : ",  "Nickname",  h_composer_cntxt_nick, 12, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Server    : ",  "Server",  h_composer_cntxt_server, 12, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Path      : ",  "Path",  h_composer_cntxt_path, 12, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"View      : ",  "View",  h_composer_cntxt_view, 12, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {NULL, NULL, NO_HELP, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
};
#define	AC_NICK	0
#define	AC_SERV	1
#define	AC_PATH	2
#define	AC_VIEW	3


char *
context_edit_screen(ps, func, def_nick, def_serv, def_path, def_view)
    struct pine  *ps;
    char	 *func;
    char	 *def_nick, *def_serv, *def_path, *def_view;
{
    int	       editor_result, i, j;
    char       tmp[MAILTMPLEN], stmp[MAILTMPLEN], *nick, *serv, *path, *view,
	      *new_cntxt = NULL, *val, *p, btmp[MAILTMPLEN];
    PICO       pbf;
    STORE_S   *msgso;
    NETMBX     mb;

    standard_picobuf_setup(&pbf);
    pbf.pine_flags   |= P_NOBODY;
    pbf.exittest      = exit_collection_add;
    pbf.canceltest    = cancel_collection_add;
    sprintf(tmp, "FOLDER COLLECTION %.100s", func);
    pbf.pine_anchor   = set_titlebar(tmp, ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,ps_global->msgmap, 
				      0, FolderName, 0, 0, NULL);

    /* An informational message */
    if(msgso = so_get(PicoText, NULL, EDIT_ACCESS)){
	pbf.msgtext = (void *) so_text(msgso);
	so_puts(msgso,
       "\n   Fill in the fields above to add a Folder Collection to your");
	so_puts(msgso,
       "\n   COLLECTION LIST screen.");
	so_puts(msgso,
       "\n   Use the \"^G\" command to get help specific to each item, and");
	so_puts(msgso,
       "\n   use \"^X\" when finished.");
    }


    pbf.headents = (struct headerentry *)fs_get((sizeof(headents_templ)
						  /sizeof(struct headerentry))
						 * sizeof(struct headerentry));
    memset((void *) pbf.headents, 0,
	   (sizeof(headents_templ)/sizeof(struct headerentry))
	   * sizeof(struct headerentry));

    for(i = 0; headents_templ[i].prompt; i++)
      pbf.headents[i] = headents_templ[i];

    nick = cpystr(def_nick ? def_nick : "");
    pbf.headents[AC_NICK].realaddr = &nick;

    serv = cpystr(def_serv ? def_serv : "");
    pbf.headents[AC_SERV].realaddr = &serv;

    path = cpystr(def_path ? def_path : "");
    pbf.headents[AC_PATH].realaddr = &path;
    pbf.headents[AC_PATH].bldr_private = (void *) 0;

    view = cpystr(def_view ? def_view : "");
    pbf.headents[AC_VIEW].realaddr = &view;

    /*
     * If this is new context, setup to query IMAP server
     * for location of personal namespace.
     */
    if(!(def_nick || def_serv || def_path || def_view)){
	pbf.headents[AC_SERV].builder	      = build_namespace;
	pbf.headents[AC_SERV].affected_entry = &pbf.headents[AC_PATH];
	pbf.headents[AC_SERV].bldr_private   = (void *) 0;
    }

    /* pass to pico and let user change them */
    editor_result = pico(&pbf);

    if(editor_result & COMP_GOTHUP){
	hup_signal();
    }
    else{
	fix_windsize(ps_global);
	init_signals();
    }

    if(editor_result & COMP_CANCEL){
	cmd_cancelled(func);
    }
    else if(editor_result & COMP_EXIT){
	if(serv && *serv){
	    if(serv[0] == '{'  && serv[strlen(serv)-1] == '}'){
		strncpy(stmp, serv, sizeof(stmp)-1);
		stmp[sizeof(stmp)-1] = '\0';
	    }
	    else
	      sprintf(stmp, "{%.*s}", sizeof(stmp)-5, serv);

	    if(mail_valid_net_parse(stmp, &mb)){
		if(!struncmp(mb.service, "nntp", 4)){
		    if(!path || strncmp(path, "#news.", 6)){
			strncat(stmp, "#news.", sizeof(stmp)-1-strlen(stmp));
		    }
		}
	    }
	    else
	      panic("Unexpected invalid server");
	}
	else
	  stmp[0] = '\0';

	tmp[0] = '\0';
	if(nick && *nick){
	    val = quote_if_needed(nick);
	    if(val){
		strncpy(tmp, val, sizeof(tmp)-2);
		tmp[sizeof(tmp)-2] = '\0';
		if(val != nick)
		  fs_give((void **)&val);
	    
		strncat(tmp, " ", 1);
		tmp[sizeof(tmp)-1] = '\0';
	    }
	}

	p = btmp;
	sstrncpy(&p, stmp, sizeof(btmp)-1-(p-btmp));
	btmp[sizeof(btmp)-1] = '\0';
	sstrncpy(&p, path, sizeof(btmp)-1-(p-btmp));
	btmp[sizeof(btmp)-1] = '\0';
	if(pbf.headents[AC_PATH].bldr_private != (void *) 0)
	sstrncpy(&p, (char *)pbf.headents[AC_PATH].bldr_private,
		 sizeof(btmp)-1-(p-btmp));
	btmp[sizeof(btmp)-1] = '\0';
	if(view[0] != '[' && sizeof(btmp)-1-(p-btmp) > 0){
	    *p++ = '[';
	    *p = '\0';
	}

	sstrncpy(&p, view, sizeof(btmp)-1-(p-btmp));
	btmp[sizeof(btmp)-1] = '\0';
	if((j=strlen(view)) < 2 || view[j-1] != ']' &&
	   sizeof(btmp)-1-(p-btmp) > 0){
	    *p++ = ']';
	    *p = '\0';
	}

	val = quote_if_needed(btmp);
	if(val){
	    strncat(tmp, val, sizeof(tmp)-1-strlen(tmp));
	    tmp[sizeof(tmp)-1] = '\0';

	    if(val != btmp)
	      fs_give((void **)&val);
	}

	new_cntxt = cpystr(tmp);
    }

    for(i = 0; headents_templ[i].prompt; i++)
      fs_give((void **) pbf.headents[i].realaddr);

    if(pbf.headents[AC_PATH].bldr_private != (void *) 0)
      fs_give(&pbf.headents[AC_PATH].bldr_private);

    fs_give((void **) &pbf.headents);

    standard_picobuf_teardown(&pbf);

    if(msgso)
      so_give(&msgso);

    return(new_cntxt);
}


/*
 *  Call back for pico to prompt the user for exit confirmation
 *
 * Returns: either NULL if the user accepts exit, or string containing
 *	 reason why the user declined.
 */      
char *
exit_collection_add(he, redraw_pico, allow_flowed)
    struct headerentry *he;
    void (*redraw_pico)();
    int allow_flowed;
{
    char     prompt[256], tmp[MAILTMPLEN], tmpnodel[MAILTMPLEN], *server, *path,
	     delim = '\0', *rstr = NULL, *p;
    int	     exists = 0, i;
    void   (*redraw)() = ps_global->redrawer;
    NETMBX   mb;

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);

    server = *he[AC_SERV].realaddr;
    removing_trailing_white_space(server);
    removing_leading_white_space(server);

    path = *he[AC_PATH].realaddr;

    if(*server){
	/* No brackets? */
	if(server[0] == '{'  && server[strlen(server)-1] == '}'){
	    strncpy(tmp, server, sizeof(tmp)-1);
	    tmp[sizeof(tmp)-1] = '\0';
	}
	else				/* add them */
	  sprintf(tmp, "{%.*s}", sizeof(tmp)-3, server);

	if(mail_valid_net_parse(tmp, &mb)){ /* news? verify namespace */
	    if(!struncmp(mb.service, "nntp", 4)){
		if(strncmp(path, "#news.", 6)){
		    strncat(tmp, "#news.", sizeof(tmp)-1-strlen(tmp));
		}
	    }
	}
	else
	  rstr = "Invalid Server entry";
    }
    else
      tmp[0] = '\0';

    if(!rstr){
	/*
	 * Delimiter acquisition below serves dual purposes.  One's to
	 * get the delimiter so we can make sure it's hanging off the end
	 * of the path so we can test for the directory existance.  The
	 * second's to make sure the server (and any requested service)
	 * name we were given exists.  It should be handled by the folder
	 * existance test futher below, but it doesn't work with news...
	 *
	 * Update. Now we are stripping the delimiter in the tmpnodel version
	 * so that we can pass that to folder_exists. Cyrus does not answer
	 * that the folder exists if we leave the trailing delimiter.
	 * Hubert 2004-12-17
	 */
	strncat(tmp, path, sizeof(tmp)-1-strlen(tmp));
	tmp[sizeof(tmp)-1] = '\0';
	strncpy(tmpnodel, tmp, sizeof(tmpnodel)-1);
	tmpnodel[sizeof(tmpnodel)-1] = '\0';

	if(he[AC_PATH].bldr_private != (void *) 0)
	  fs_give(&he[AC_PATH].bldr_private);

	ps_global->mm_log_error = 0;
	if(delim = folder_delimiter(tmp)){
	    if(*path){
		if(tmp[(i = strlen(tmp)) - 1] == delim)
		  tmpnodel[i-1] = '\0';
		else{
		    tmp[i]   = delim;
		    tmp[i+1] = '\0';
		    he[AC_PATH].bldr_private = (void *) cpystr(&tmp[i]);
		}
	    }
	}
	else if(ps_global->mm_log_error && ps_global->last_error)
	  /* We used to bail, but this was changed with 4.10
	   * as some users wanted to add clctn before the server
	   * was actually around and built.
	   */
	  flush_status_messages(0);	/* mail_create gripes */
	else
	  dprint(1, (debugfile, "exit_col_test: No Server Hierarchy!\n"));
    }

    if(!rstr){
	if(!*tmp
	   || !delim
	   || ((*(p = tmp) == '#'
		|| (*tmp == '{' && (p = strchr(tmp, '}')) && *++p))
	       && !struncmp(p, "#news.", 6))
	   || (*tmp == '{' && (p = strchr(tmp, '}')) && !*++p)){
	    exists = 1;
	}
	else if((i = folder_exists(NULL, tmpnodel)) & FEX_ERROR){
	    if(!(rstr = ps_global->last_error))
	      rstr = "Problem testing for directory existance";
	}
	else
	  exists = (i & FEX_ISDIR);

	sprintf(prompt, "Exit%.50s" ,
		exists
		  ? " and save changes"
		  : ", saving changes and creating Path");
	if(want_to(prompt, 'y', 0, NO_HELP, WT_NORM) == 'y'){
	    if(!exists && !pine_mail_create(NULL, tmp)){
		flush_status_messages(1);	/* mail_create gripes */
		if(!(rstr = ps_global->last_error))
		  rstr = "";
	    }
	}
	else
	  rstr = "Use ^C to abandon changes you've made";
    }

    ps_global->redrawer = redraw;
    return(rstr);
}



/*
 *  Call back for pico to prompt the user for exit confirmation
 *
 * Returns: either NULL if the user accepts exit, or string containing
 *	 reason why the user declined.
 */      
char *
cancel_collection_add(word, redraw_pico)
    char *word;
    void (*redraw_pico)();
{
    char *rstr = NULL;
    void (*redraw)() = ps_global->redrawer;
#define	CCA_PROMPT	\
		"Cancel Add (answering \"Yes\" will abandon any changes made) "

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);
    switch(want_to(CCA_PROMPT, 'y', 'x', NO_HELP, WT_NORM)){
      case 'y':
	rstr = "Add Cancelled";
	break;

      case 'n':
      case 'x':
	break;
    }

    ps_global->redrawer = redraw;
    return(rstr);
}


int
build_namespace(server, server_too, error, barg, mangled)
    char	 *server,
		**server_too,
		**error;
    BUILDER_ARG	 *barg;
    int          *mangled;
{
    char	 *p, *name;
    int		  we_cancel = 0;
    MAILSTREAM	 *stream;
    NAMESPACE  ***namespace;

    dprint(5, (debugfile, "- build_namespace - (%s)\n",
	       server ? server : "nul"));

    if(*barg->me){		/* only call this once! */
	if(server_too)
	  *server_too = cpystr(server ? server : "");

	return(0);
    }
    else
      *barg->me = (void *) 1;

    if(p = server)		/* empty string? */
      while(*p && isspace((unsigned char) *p))
	p++;

    if(p && *p){
	if(server_too)
	  *server_too = cpystr(p);

	name = (char *) fs_get((strlen(p) + 3) * sizeof(char));
	sprintf(name, "{%s}", p);
    }
    else{
	if(server_too)
	  *server_too = cpystr("");

	return(0);
    }

    *mangled = 1;
    fix_windsize(ps_global);
    init_sigwinch();
    clear_cursor_pos();

    we_cancel = busy_alarm(1, "Fetching default directory", NULL, 0);

    if(stream = pine_mail_open(NULL, name,
			   OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE,
			       NULL)){
	if((namespace = mail_parameters(stream, GET_NAMESPACE, NULL))
	   && *namespace && (*namespace)[0]
	   && (*namespace)[0]->name && (*namespace)[0]->name[0]){
	    if(barg->tptr)
	      fs_give((void **)&barg->tptr);

	    barg->tptr = cpystr((*namespace)[0]->name);
	}

	pine_mail_close(stream);
    }

    if(we_cancel)
      cancel_busy_alarm(-1);

    fs_give((void **) &name);

    return(1);
}


int
fl_val_gen (f, fs)
    FOLDER_S *f;
    FSTATE_S *fs;
{
    return(f && FLDR_NAME(f));
}


int
fl_val_writable (f, fs)
    FOLDER_S *f;
    FSTATE_S *fs;
{
    return(1);
}


int
fl_val_subscribe (f, fs)
    FOLDER_S *f;
    FSTATE_S *fs;
{
    if(f->subscribed){
	q_status_message1(SM_ORDER, 0, 4, "Already subscribed to \"%.200s\"",
			  FLDR_NAME(f));
	return(0);
    }

    return(1);
}




/*----------------------------------------------------------------------
  Business end of displaying and operating on a collection of folders

  Args:  ps           -- The pine_state data structure
	 fs	      -- Folder screen state structure

  Result: A string list containing folders selected,
	  NULL on Cancel, Error or other problem

  ----*/
STRLIST_S *
folder_lister(ps, fs)
    struct pine	*ps;
    FSTATE_S	*fs;
{
    int		fltrv, we_cancel = 0;
    HANDLE_S   *handles = NULL;
    STORE_S    *screen_text = NULL;
    FPROC_S	folder_proc_data;
    gf_io_t	pc;

    dprint(1, (debugfile, "\n\n    ---- FOLDER LISTER ----\n"));

    memset(&folder_proc_data, 0, sizeof(FPROC_S));
    folder_proc_data.fs = fs;

    while(!folder_proc_data.done){
	if(screen_text = so_get(CharStar, NULL, EDIT_ACCESS)){
	    gf_set_so_writec(&pc, screen_text);
	}
	else{
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Formatting Error: Can't create space for list");
	    return(NULL);
	}

	we_cancel = busy_alarm(1, "Formatting scroll text", percent_formatted, 0);
	fltrv = folder_list_text(ps, &folder_proc_data, pc, &handles, 
			    ps_global->ttyo->screen_cols);
	if(we_cancel){
	  cancel_busy_alarm(-1);
	  percent.num_done = 0;
	}

	if(fltrv){

	    SCROLL_S	    sargs;
	    struct key_menu km;
	    struct key	    keys[36];

	    memset(&sargs, 0, sizeof(SCROLL_S));
	    sargs.text.text = so_text(screen_text);
	    sargs.text.src  = CharStar;
	    sargs.text.desc = "folder list";
	    if(sargs.text.handles = folder_list_handle(fs, handles))
	      sargs.start.on = Handle;

	    sargs.bar.title    = fs->f.title.bar;
	    sargs.bar.style    = fs->f.title.style;

	    sargs.proc.tool	   = folder_processor;
	    sargs.proc.data.p	   = (void *) &folder_proc_data;

	    sargs.resize_exit  = 1;
	    sargs.vert_handle  = 1;
	    sargs.srch_handle  = 1;

	    sargs.help.text    = fs->f.help.text;
	    sargs.help.title   = fs->f.help.title;

	    sargs.keys.menu    = &km;
	    km		       = *fs->km;
	    km.keys	       = keys;
	    memcpy(&keys[0], fs->km->keys,
		   (km.how_many * 12) * sizeof(struct key));
	    setbitmap(sargs.keys.bitmap);

	    if(fs->km == &folder_km){
		sargs.keys.each_cmd = folder_lister_km_manager;
#ifdef	_WINDOWS
		sargs.mouse.popup      = folder_list_popup;
#endif
	    }
	    else{
#ifdef	_WINDOWS
		sargs.mouse.popup = folder_list_select_popup;
#endif
		if(fs->km == &folder_sel_km || fs->km == &folder_sela_km)
		  sargs.keys.each_cmd = folder_lister_km_sel_manager;
		else if(fs->km == &folder_sub_km)
		  sargs.keys.each_cmd = folder_lister_km_sub_manager;
	    }

	    sargs.mouse.clickclick = folder_lister_clickclick;

	    switch(scrolltool(&sargs)){
	      case MC_MAIN :		/* just leave */
		folder_proc_data.done = 1;
		break;

	      case MC_RESIZE :		/* loop around rebuilding screen */
		if(sargs.text.handles){
		    FOLDER_S *fp;

		    fp = folder_entry(sargs.text.handles->h.f.index,
				    FOLDERS(sargs.text.handles->h.f.context));
		    if(fp && strlen(fp->name) < MAXFOLDER -1)
		      strcpy(fs->first_folder, fp->name);

		    fs->context = sargs.text.handles->h.f.context;
		}

		break;

		/*--------- EXIT menu -----------*/
	      case MC_EXIT :
	      case MC_EXITQUERY :
		fs->list_cntxt = NULL;
		folder_proc_data.done = folder_proc_data.all_done = 1;
		break;

	      default :
		break;
	    }


	    if(F_ON(F_BLANK_KEYMENU,ps_global))
	      FOOTER_ROWS(ps_global) = 1;

	    gf_clear_so_writec(screen_text);
	    so_give(&screen_text);
	    free_handles(&handles);
	}
	else
	  folder_proc_data.done = 1;
    }

    reset_context_folders(fs->context);

    if(folder_proc_data.all_done)
      fs->context = NULL;

    if(fs->cache_streamp && *fs->cache_streamp){
	int i;

	/* 
	 * check stream pool to see if currently cached
	 * stream went away
	 */
	for(i = 0; i < ps_global->s_pool.nstream; i++)
	  if(ps_global->s_pool.streams[i] == *fs->cache_streamp)
	    break;
	if(i == ps_global->s_pool.nstream)
	  *fs->cache_streamp = NULL;
    }

    return(folder_proc_data.rv);
}



/*
 * folder_list_text - format collection's contents for display
 */
int
folder_list_text(ps, fp, pc, handlesp, cols)
    struct pine	 *ps;
    FPROC_S	 *fp;
    gf_io_t	  pc;
    HANDLE_S	**handlesp;
    int		  cols;
{
    int	       rv = 1, i, j, ftotal, fcount, slot_size, slot_rows,
	       slot_cols, index, findex, len, shown, selected;
    CONTEXT_S *c_list;

    if(handlesp)
      init_handles(handlesp);

    c_list = fp->fs->context;
    if(fp->fs->combined_view
       && (F_ON(F_CMBND_SUBDIR_DISP, ps_global) || !c_list->dir->prev))
      while(c_list->prev)		/* rewind to start */
	c_list = c_list->prev;

    do{
	/* If we're displaying folders, fetch the list */
	if(shown = (c_list == fp->fs->context
		    || (c_list->dir->status & CNTXT_NOFIND) == 0
		    || F_ON(F_EXPANDED_FOLDERS, ps_global))){
	    /*
	     * if select is allowed, flag context so any that are
	     * are remembered even after the list is destroyed
	     */
	    if(fp->fs->agg_ops)
	      c_list->use |= CNTXT_PRESRV;

	    /* Make sure folder list filled in */
	    refresh_folder_list(c_list, fp->fs->no_dirs, FALSE, fp->fs->cache_streamp);
	}

	/* Insert any introductory text here */
	if(c_list->next
	   || c_list->prev
	   || c_list->dir->prev
	   || fp->fs->force_intro){

	    /* Leading vert line? */
	    if(fp->fs->combined_view
	       && (F_ON(F_CMBND_SUBDIR_DISP,ps_global)
		   || !c_list->dir->prev)){
		if(c_list->prev)
		  gf_puts("\n", pc);		/* blank line */

		gf_puts(repeat_char(cols, '-'), pc);
		gf_puts("\n", pc);
	    }

	    /* nickname or description */
	    if(F_ON(F_CMBND_FOLDER_DISP, ps_global)
	       && (!c_list->dir->prev
		   || F_ON(F_CMBND_SUBDIR_DISP, ps_global))){
		char buf[MAX_SCREEN_COLS + 1];

		sprintf(buf, "%.10s-Collection <%.*s>",
			NEWS_TEST(c_list) ? "News" : "Folder",
			cols - 22,
			strsquish(tmp_20k_buf,
				  (c_list->nickname)
				    ? c_list->nickname
				    : (c_list->label ? c_list->label : ""),
				  cols - 22));
		gf_puts(buf, pc);
		gf_puts("\n", pc);
	    }
	    else if(c_list->label){
		gf_puts(folder_list_center_space(c_list->label, cols), pc);
		gf_puts(c_list->label, pc);
		gf_puts("\n", pc);
	    }

	    if(c_list->comment){
		gf_puts(folder_list_center_space(c_list->comment,cols), pc);
		gf_puts(c_list->comment, pc);
		gf_puts("\n", pc);
	    }

	    if(c_list->dir->desc){
		char buf[MAX_SCREEN_COLS + 1];

		strncpy(buf, strsquish(tmp_20k_buf,c_list->dir->desc,cols-10),
			sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
		gf_puts(folder_list_center_space(buf, cols), pc);
		gf_puts(buf, pc);
		gf_puts("\n", pc);
	    }

	    if(c_list->use & CNTXT_ZOOM){
		sprintf(tmp_20k_buf, "[ ZOOMED on %d (of %d) %ss ]",
			selected_folders(c_list),
			folder_total(FOLDERS(c_list)),
			(c_list->use & CNTXT_NEWS) ? "Newsgroup" : "Folder");

		gf_puts(folder_list_center_space(tmp_20k_buf, cols), pc);
		gf_puts(tmp_20k_buf, pc);
		gf_puts("\n", pc);
	    }

	    gf_puts(repeat_char(cols, '-'), pc);
	    gf_puts("\n\n", pc);
	}

	if(shown){
	    /* Run thru list formatting as necessary */
	    if(ftotal = folder_total(FOLDERS(c_list))){
		/* If previously selected, mark members of new list */
		selected = selected_folders(c_list);

		/* How many chars per cell for each folder name? */
		slot_size = 1;
		for(fcount = i = 0; i < ftotal; i++){
		    FOLDER_S *f = folder_entry(i, FOLDERS(c_list));

		    if((c_list->use & CNTXT_ZOOM) && !f->selected)
		      continue;

		    fcount++;

		    /* length of folder name plus blank */
		    len = strlen(FLDR_NAME(f)) + 1;
		    if(f->isdir)
		      len += (f->isfolder) ? 3 : 1;

		    if(NEWS_TEST(c_list) 
		       && (c_list->use & CNTXT_FINDALL))
		      /* assume listmode so we don't have to reformat */
		      /* if listmode is actually entered */
		      len += 4;    
		    if(selected){
			if(F_OFF(F_SELECTED_SHOWN_BOLD, ps_global))
			  len += 4;		/* " X  " */
		    }
		    else if(c_list == fp->fs->list_cntxt)
		      len += 4;			/* "[X] " */

		    if(slot_size < len)
		      slot_size = len;
		}

		if(F_ON(F_SINGLE_FOLDER_LIST, ps_global)){
		    slot_cols = 1;
		    slot_rows = fcount;
		}
		else{
		    switch(slot_cols = (cols / slot_size)){
		      case 0 :
			slot_cols = 1;
		      case 1 :
			slot_rows = fcount;
			break;

		      default :
			while(slot_cols * (slot_size + 1) < cols)
			  slot_size++;

			slot_rows = (fcount / slot_cols)
					      + ((fcount % slot_cols) ? 1 : 0);
			break;
		    }
		}

		percent.total = slot_rows;
		for(i = index = 0; i < slot_rows; i++){
		    percent.num_done = i;
		    if(i)
		      gf_puts("\n", pc);

		    for(j = len = 0; j < slot_cols; j++, index++){
			if(len){
			    gf_puts(repeat_char(slot_size - len, ' '), pc);
			    len = 0;
			}

			if(F_ON(F_VERTICAL_FOLDER_LIST, ps_global))
			  index = i + (j * slot_rows);

			findex = index;

			if(c_list->use & CNTXT_ZOOM)
			  findex = folder_list_ith(index, c_list);

			if(findex < ftotal){
			    int flags = (handlesp) ? FLW_LUNK : FLW_NONE;

			    if(c_list == fp->fs->list_cntxt)
			      flags |= FLW_LIST;
			    else if(selected)
			      flags |= FLW_SLCT;
			    else if(F_ON(F_SINGLE_FOLDER_LIST, ps_global)
				    || ((c_list->use & CNTXT_FINDALL)
					&& NEWS_TEST(c_list)))
			      gf_puts("    ", pc);

			    len = folder_list_write(pc, handlesp, c_list,
						    findex, NULL, flags);
			}
		    }
		}
	    }
	    else if(fp->fs->combined_view
		    && (F_ON(F_CMBND_SUBDIR_DISP, ps_global)
			|| !c_list->dir->prev)){
		static char *emptiness = "[No Folders in Collection]";

		gf_puts(folder_list_center_space(emptiness, cols), pc);
		len = folder_list_write(pc, handlesp, c_list, -1, emptiness,
					(handlesp) ? FLW_LUNK : FLW_NONE);
	    }
	}
	else if(fp->fs->combined_view
		&& (F_ON(F_CMBND_SUBDIR_DISP, ps_global)
		    || !c_list->dir->prev)){
	    static char *unexpanded = "[Select Here to See Expanded List]";

	    gf_puts(folder_list_center_space(unexpanded, cols), pc);
	    len = folder_list_write(pc, handlesp, c_list, -1, unexpanded,
				    (handlesp) ? FLW_LUNK : FLW_NONE);
	}

	gf_puts("\n", pc);			/* blank line */

    }
    while(fp->fs->combined_view
	  && (F_ON(F_CMBND_SUBDIR_DISP, ps_global) || !c_list->dir->prev)
	  && (c_list = c_list->next));

    return(rv);
}



int
folder_list_write(pc, handlesp, ctxt, fnum, alt_name, flags)
    gf_io_t    pc;
    HANDLE_S **handlesp;
    CONTEXT_S *ctxt;
    int	       fnum;
    char      *alt_name;
    int	       flags;
{
    char      buf[256];
    int	      l = 0;
    FOLDER_S *fp;
    HANDLE_S *h;

    if(flags & FLW_LUNK){
	h		 = new_handle(handlesp);
	h->type		 = Folder;
	h->h.f.index	 = fnum;
	h->h.f.context	 = ctxt;
	h->force_display = 1;

	sprintf(buf, "%d", h->key);
    }
    else
      h = NULL;

    fp = (fnum < 0) ? NULL : folder_entry(fnum, FOLDERS(ctxt));

    /* embed handle pointer */
    if((h ? ((*pc)(TAG_EMBED) && (*pc)(TAG_HANDLE)
	     && (*pc)(strlen(buf)) && gf_puts(buf, pc)) : 1)
       && (fp ? ((l = folder_list_write_prefix(fp, flags, pc)) >= 0
		 && gf_puts(FLDR_NAME(fp), pc)
		 && ((fp->isdir && fp->isfolder) ? (*pc)('[') : 1)
		 && ((fp->isdir) ? (*pc)(ctxt->dir->delim) : 1)
		 && ((fp->isdir && fp->isfolder) ? (*pc)(']') : 1))
	      : (alt_name ? gf_puts(alt_name, pc) : 0))
       && (h ? ((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDOFF)
		&& (*pc)(TAG_EMBED) && (*pc)(TAG_INVOFF)) : 1)){
	if(fp){
	    l += strlen(FLDR_NAME(fp));
	    if(fp->isdir)
	      l += (fp->isfolder) ? 3 : 1;
	}
	else if(alt_name)
	  l = strlen(alt_name);
    }

    return(l);
}


int
folder_list_write_prefix(f, flags, pc)
    FOLDER_S *f;
    int	      flags;
    gf_io_t   pc;
{
    int rv = 0;

    if(flags & FLW_SLCT){
	if(F_OFF(F_SELECTED_SHOWN_BOLD, ps_global) || !(flags & FLW_LUNK)){
	    rv = 4;
	    if(f->selected){
		gf_puts(" X  ", pc);
	    }
	    else{
		gf_puts("    ", pc);
	    }
	}
	else
	    rv = ((*pc)(TAG_EMBED) 
		  && (*pc)((f->selected) ? TAG_BOLDON : TAG_BOLDOFF)) ? 0 : -1;
    }
    else if(flags & FLW_LIST){
	rv = 4;
	gf_puts(f->subscribed ? "SUB " : (f->selected ? "[X] " : "[ ] "), pc);
    }

    return(rv);
}



int
folder_list_ith(n, cntxt)
    int	       n;
    CONTEXT_S *cntxt;
{
    int	      index, ftotal;
    FOLDER_S *f;

    for(index = 0, ftotal = folder_total(FOLDERS(cntxt));
	index < ftotal
	&& (f = folder_entry(index, FOLDERS(cntxt)))
	&& !(f->selected && !n--);
	index++)
      ;
      
    return(index);
}


char *
folder_list_center_space(s, width)
    char *s;
    int   width;
{
    int l;

    return(((l = strlen(s)) < width) ? repeat_char((width - l)/2, ' ') : "");
}



/*
 * folder_list_handle - return pointer in handle list
 *			corresponding to "start"
 */
HANDLE_S *
folder_list_handle(fs, handles)
    FSTATE_S *fs;
    HANDLE_S *handles;
{
    char     *p, *name = NULL;
    HANDLE_S *h, *h_found = NULL;
    FOLDER_S *fp;

    if(handles && fs->context){
	if(!(NEWS_TEST(fs->context) || (fs->context->use & CNTXT_INCMNG))
	   && fs->context->dir->delim)
	  for(p = strchr(fs->first_folder, fs->context->dir->delim);
	      p;
	      p = strchr(p, fs->context->dir->delim))
	    name = ++p;

	for(h = handles; h; h = h->next)
	  if(h->h.f.context == fs->context){
	      if(!h_found)		/* match at least given context */
		h_found = h;

	      if(!fs->first_folder[0]
		 || ((fp = folder_entry(h->h.f.index, FOLDERS(h->h.f.context)))
		     && ((fs->first_dir && fp->isdir)
			 || (!fs->first_dir && fp->isfolder))
		     && !strcmp(name ? name : fs->first_folder,
				FLDR_NAME(fp)))){
		  h_found = h;
		  break;
	      }
	  }

	fs->first_folder[0] = '\0';
    }

    return(h_found ? h_found : handles);
}



int
folder_processor(cmd, msgmap, sparms)
    int	      cmd;
    MSGNO_S  *msgmap;
    SCROLL_S *sparms;
{
    int	      rv = 0;
    char      tmp_output[MAILTMPLEN];

    switch(cmd){
      case MC_FINISH :
	FPROC(sparms)->done = rv = 1;;
	break;

            /*---------- Select or enter a View ----------*/
      case MC_CHOICE :
	rv = folder_lister_choice(sparms);
	break;

            /*--------- Hidden "To Fldrs" command -----------*/
      case MC_LISTMODE :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    if(!FPROC(sparms)->fs->list_cntxt){
		FPROC(sparms)->fs->list_cntxt
					   = sparms->text.handles->h.f.context;
		rv = scroll_add_listmode(sparms->text.handles->h.f.context,
				 folder_total(FOLDERS(sparms->text.handles->h.f.context))); 
		if(!rv){
		  /* need to set the subscribe key ourselves */
		  sparms->keys.menu->keys[SB_SUB_KEY].name = "S";
		  sparms->keys.menu->keys[SB_SUB_KEY].label = "Subscribe";
		  sparms->keys.menu->keys[SB_SUB_KEY].bind.cmd = MC_CHOICE;
		  sparms->keys.menu->keys[SB_SUB_KEY].bind.ch[0] = 's';
		  setbitn(SB_SUB_KEY, sparms->keys.bitmap);
		  ps_global->mangled_screen = 1;
		  
		  sparms->keys.menu->keys[SB_EXIT_KEY].bind.cmd = MC_EXITQUERY;
		}
		q_status_message(SM_ORDER, 0, 1, LISTMODE_GRIPE);
	    }
	    else
	      q_status_message(SM_ORDER, 0, 4, "Already in List Mode");
	}
	else
	  q_status_message(SM_ORDER, 0, 4,
			   "No Folders!  Can't enter List Mode");

	break;

    
	/*--------- Visit parent directory -----------*/
      case MC_PARENT :
	if(folder_lister_parent(FPROC(sparms)->fs,
				(sparms->text.handles)
				 ? sparms->text.handles->h.f.context
				 : FPROC(sparms)->fs->context,
				(sparms->text.handles)
				 ? sparms->text.handles->h.f.index : -1, 0))
	  rv = 1;		/* leave scrolltool to rebuild screen */
	    
	break;


	/*--------- Open the selected folder -----------*/
      case MC_OPENFLDR :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context)))
	  rv = folder_lister_finish(sparms, sparms->text.handles->h.f.context,
				    sparms->text.handles->h.f.index);
	else
	  q_status_message(SM_ORDER, 0, 4,
			   "No Folders!  Nothing to View");

	break;


	/*--------- Export the selected folder -----------*/
      case MC_EXPORT :
	folder_export(sparms);
	break;


	/*--------- Export the selected folder -----------*/
      case MC_IMPORT :
        {
	    CONTEXT_S *cntxt = (sparms->text.handles)
				 ? sparms->text.handles->h.f.context
				 : FPROC(sparms)->fs->context;
	    char       new_file[2*MAXFOLDER+10];
	    int	       r;

	    new_file[0] = '\0';

	    r = folder_import(sparms, new_file, sizeof(new_file));

	    if(r && (cntxt->use & CNTXT_INCMNG || context_isambig(new_file))){
		rv = 1;			/* rebuild display! */
		FPROC(sparms)->fs->context = cntxt;
		if(strlen(new_file) < MAXFOLDER - 1)
		  strcpy(FPROC(sparms)->fs->first_folder, new_file);
	    }
	    else
	      ps_global->mangled_footer++;
	}

	break;


	/*--------- Return to the Collections Screen -----------*/
      case MC_COLLECTIONS :
	FPROC(sparms)->done = rv = 1;
	break;


	/*--------- QUIT pine -----------*/
      case MC_QUIT :
	ps_global->next_screen = quit_screen;
	FPROC(sparms)->done = rv = 1;
	break;
	    

            /*--------- Compose -----------*/
      case MC_COMPOSE :
	ps_global->next_screen = compose_screen;
	FPROC(sparms)->done = rv = 1;
	break;
	    

            /*--------- Alt Compose -----------*/
      case MC_ROLE :
	ps_global->next_screen = alt_compose_screen;
	FPROC(sparms)->done = rv = 1;
	break;
	    

            /*--------- Message Index -----------*/
      case MC_INDEX :
	if(THREADING() && sp_viewing_a_thread(ps_global->mail_stream))
	  unview_thread(ps_global, ps_global->mail_stream, ps_global->msgmap);

	ps_global->next_screen = mail_index_screen;
	FPROC(sparms)->done = rv = 1;
	break;


            /*----------------- Add a new folder name -----------*/
      case MC_ADDFLDR :
        {
	    CONTEXT_S *cntxt = (sparms->text.handles)
				 ? sparms->text.handles->h.f.context
				 : FPROC(sparms)->fs->context;
	    char       new_file[2*MAXFOLDER+10];
	    int	       r;

	    if(NEWS_TEST(cntxt))
	      r = group_subscription(new_file, sizeof(new_file), cntxt);
	    else
	      r = add_new_folder(cntxt, Main, V_INCOMING_FOLDERS, new_file,
				 sizeof(new_file),
				 FPROC(sparms)->fs->cache_streamp
				  ? *FPROC(sparms)->fs->cache_streamp : NULL,
				 NULL);

	    if(r && (cntxt->use & CNTXT_INCMNG || context_isambig(new_file))){
		rv = 1;			/* rebuild display! */
		FPROC(sparms)->fs->context = cntxt;
		if(strlen(new_file) < MAXFOLDER - 1)
		  strcpy(FPROC(sparms)->fs->first_folder, new_file);
	    }
	    else
	      ps_global->mangled_footer++;
	}

        break;


            /*------ Type in new folder name, e.g., for save ----*/
      case MC_ADD :
	rv = folder_lister_addmanually(sparms);
        break;


            /*--------------- Rename folder ----------------*/
      case MC_RENAMEFLDR :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    char new_file[MAXFOLDER+1];
	    int  r;

	    r = rename_folder(sparms->text.handles->h.f.context,
			      sparms->text.handles->h.f.index, new_file,
			      sizeof(new_file),
			      FPROC(sparms)->fs->cache_streamp
			      ? *FPROC(sparms)->fs->cache_streamp : NULL);

	    if(r){
		/* repaint, placing cursor on new folder! */
		rv = 1;
		if(context_isambig(new_file)){
		    FPROC(sparms)->fs->context
					   = sparms->text.handles->h.f.context;
		    if(strlen(new_file) < MAXFOLDER - 1)
		      strcpy(FPROC(sparms)->fs->first_folder, new_file);
		}
	    }

	    ps_global->mangled_footer++;
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 0, 4,
			   "Empty folder collection.  No folder to rename!");

	break;
		     

            /*-------------- Delete --------------------*/
      case MC_DELETE :
	if(!(sparms->text.handles
		 && folder_total(FOLDERS(sparms->text.handles->h.f.context)))){
	    q_status_message(SM_ORDER | SM_DING, 0, 4,
			     "Empty folder collection.  No folder to delete!");
	}
	else{
	    char next_folder[MAILTMPLEN+1];

	    next_folder[0] = '\0';
	    if(delete_folder(sparms->text.handles->h.f.context,
			     sparms->text.handles->h.f.index,
			     next_folder, sizeof(next_folder),
			     FPROC(sparms)->fs->cache_streamp
			     && *FPROC(sparms)->fs->cache_streamp
			     ? FPROC(sparms)->fs->cache_streamp : NULL)){

		/* repaint, placing cursor on adjacent folder! */
		rv = 1;
		if(next_folder[0] && strlen(next_folder) < MAXFOLDER - 1){
		  strcpy(FPROC(sparms)->fs->first_folder, next_folder);
		}
		else
		  sparms->text.handles->h.f.context->use &= ~CNTXT_ZOOM;
	    }
	    else
	      ps_global->mangled_footer++;
	}

	break;


            /*----------------- Shuffle incoming folder list -----------*/
      case MC_SHUFFLE :
        {
	    CONTEXT_S *cntxt = (sparms->text.handles)
				 ? sparms->text.handles->h.f.context : NULL;

	    if(!(cntxt && cntxt->use & CNTXT_INCMNG))
	      q_status_message(SM_ORDER, 0, 4,
			       "May only shuffle incoming-folders.");
	    else if(folder_total(FOLDERS(cntxt)) == 0)
	      q_status_message(SM_ORDER, 0, 4,
			       "No folders to shuffle.");
	    else if(folder_total(FOLDERS(cntxt)) < 2)
	      q_status_message(SM_ORDER, 0, 4,
		       "Shuffle only makes sense with more than one folder.");
	    else{
		if(FPROC(sparms) && FPROC(sparms)->fs &&
		   FPROC(sparms)->fs && sparms->text.handles &&
		   sparms->text.handles->h.f.index >= 0 &&
		   sparms->text.handles->h.f.index <
					       folder_total(FOLDERS(cntxt)))
		  strcpy(FPROC(sparms)->fs->first_folder,
		         FLDR_NAME(folder_entry(sparms->text.handles->h.f.index,
					        FOLDERS(cntxt))));

		rv = shuffle_incoming_folders(cntxt,
					      sparms->text.handles->h.f.index);
	    }
	}

      break;


	/*-------------- Goto Folder Prompt --------------------*/
      case MC_GOTO :
      {
	  CONTEXT_S *c = (sparms->text.handles)
			   ? sparms->text.handles->h.f.context
			   : FPROC(sparms)->fs->context;
	  char *new_fold = broach_folder(-FOOTER_ROWS(ps_global), 0, &c);

	  if(new_fold && do_broach_folder(new_fold, c, NULL, 0L) > 0){
	      ps_global->next_screen = mail_index_screen;
	      FPROC(sparms)->done = rv = 1;
	  }
	  else
	    ps_global->mangled_footer = 1;

	  if((c = ((sparms->text.handles)
		    ? sparms->text.handles->h.f.context
		    : FPROC(sparms)->fs->context))->dir->status & CNTXT_NOFIND)
	    refresh_folder_list(c, FPROC(sparms)->fs->no_dirs, TRUE, FPROC(sparms)->fs->cache_streamp);
      }

      break;


      /*------------- Print list of folders ---------*/
      case MC_PRINTFLDR :
	print_folders(FPROC(sparms));
	ps_global->mangled_footer++;
	break;


	/*----- Select the current folder, or toggle checkbox -----*/
      case MC_SELCUR :
	/*---------- Select set of folders ----------*/
      case MC_SELECT :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    if(cmd == MC_SELCUR){
		rv = folder_select_toggle(sparms->text.handles->h.f.context,
				     sparms->text.handles->h.f.index,
				    (NEWS_TEST(sparms->text.handles->h.f.context) 
				     && FPROC(sparms)->fs->list_cntxt) 
				       ? ng_scroll_edit : folder_select_update);
		   if(!rv) ps_global->mangled_screen = 1;
	    }
	    else
	      switch(folder_select(ps_global,
				   sparms->text.handles->h.f.context,
				   sparms->text.handles->h.f.index)){
		case 1 :
		  rv = 1;		/* rebuild screen */

		case 0 :
		default :
		  ps_global->mangled_screen++;
		  break;
	      }

	    if((sparms->text.handles->h.f.context->use & CNTXT_ZOOM)
	       && !selected_folders(sparms->text.handles->h.f.context)){
		sparms->text.handles->h.f.context->use &= ~CNTXT_ZOOM;
		rv = 1;			/* make sure to redraw */
	    }

	    if(rv){			/* remember where to start */
		FOLDER_S *fp;

		FPROC(sparms)->fs->context = sparms->text.handles->h.f.context;
		if(fp = folder_entry(sparms->text.handles->h.f.index,
				 FOLDERS(sparms->text.handles->h.f.context))){
		    if(strlen(FLDR_NAME(fp)) < MAXFOLDER - 1){
			strcpy(FPROC(sparms)->fs->first_folder, FLDR_NAME(fp));
			FPROC(sparms)->fs->first_dir = fp->isdir;
		    }
		}
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 0, 4,
			   "Empty folder collection.  No folder to select!");

	break;


	/*---------- Display  folders ----------*/
      case MC_ZOOM :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    FOLDER_S *fp;
	    int	      n;

	    if(n = selected_folders(sparms->text.handles->h.f.context)){
		if(sparms->text.handles->h.f.context->use & CNTXT_ZOOM){
		    sparms->text.handles->h.f.context->use &= ~CNTXT_ZOOM;
		    q_status_message(SM_ORDER, 0, 3,
				     "Folder List Zoom mode is now off");
		}
		else{
		    q_status_message1(SM_ORDER, 0, 3,
	     "In Zoomed list of %.200s folders. Use \"Z\" to restore regular list",
				      int2string(n));
		    sparms->text.handles->h.f.context->use |= CNTXT_ZOOM;
		}

		/* exit scrolltool to rebuild screen */
		rv = 1;

		/* Set where to start after it's rebuilt */
		FPROC(sparms)->fs->context = sparms->text.handles->h.f.context;
		FPROC(sparms)->fs->first_folder[0] = '\0';
		if((fp = folder_entry(sparms->text.handles->h.f.index,
				  FOLDERS(sparms->text.handles->h.f.context)))
		   && !((sparms->text.handles->h.f.context->use & CNTXT_ZOOM)
			&& !fp->selected)
		   && strlen(FLDR_NAME(fp)) < MAXFOLDER - 1)
		  strcpy(FPROC(sparms)->fs->first_folder, FLDR_NAME(fp));
	    }
	    else
	      q_status_message(SM_ORDER, 0, 3,
			       "No selected folders to Zoom on");
	}
	else
	  q_status_message(SM_ORDER, 0, 4, "No Folders to Zoom on!");

	break;

	/*----- Ask user to abandon selection before exiting -----*/
      case MC_EXITQUERY :
	if(sparms->text.handles
	   && FOLDERS(sparms->text.handles->h.f.context)){
	  int	   i, folder_n;
	  FOLDER_S *fp;
	  
	  folder_n = folder_total(FOLDERS(sparms->text.handles->h.f.context));
	  /* any selected? */
	  for(i = 0; i < folder_n; i++){
	    fp = folder_entry(i, FOLDERS(sparms->text.handles->h.f.context));
	    if(fp->selected)
	      break;
	  }

	  if(i < folder_n	/* some selections have been made */
	     && want_to("Really abandon your selections ",
			'y', 'x', NO_HELP, WT_NORM) != 'y'){
	    break;
	  }
	}
	rv = 1;
	break;

	/*------------New Msg command --------------*/
      case MC_CHK_RECENT:
	/*
	 * Derived from code provided by
	 * Rostislav Neplokh neplokh@andrew.cmu.edu and
	 * Robert Siemborski (rjs3@andrew).
	 */
        if(sparms->text.handles
           && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    FOLDER_S *folder;

            folder = folder_entry(sparms->text.handles->h.f.index,
				  FOLDERS(sparms->text.handles->h.f.context));
	    
	    if(!folder)
	      strncpy(tmp_output, "Invalid Folder Name", sizeof(tmp_output)-1);
	    else if(folder->isdir)
	      sprintf(tmp_output, "\"%.100s\" is a directory", folder->name);
	    else{
		MAILSTREAM   *strm = NIL;
		char          mailbox_name[MAXPATH+1];
		unsigned long tot, rec;
		int           gotit = 0, we_cancel;

		we_cancel = busy_alarm(1, NULL, NULL, 0);
		context_apply(mailbox_name,
			      sparms->text.handles->h.f.context,
			      folder->name, MAXPATH+1);

		/* do we already have it selected? */
		if((strm = sp_stream_get(mailbox_name, SP_MATCH | SP_RO_OK))){
		    MSGNO_S *msgmap;
		    long     excluded;

		    gotit++;
		    if(strm == ps_global->mail_stream){
			/*
			 * Unfortunately, we have to worry about excluded
			 * messages now. The user doesn't want to have
			 * excluded messages count in the totals, especially
			 * recent excluded messages.
			 */

			msgmap = sp_msgmap(strm);

			if((excluded = any_lflagged(msgmap, MN_EXLD))){

			    tot = strm->nmsgs - excluded;
			    if(tot)
			      rec = count_flagged(strm, F_RECENT);
			    else
			      rec = 0;
			}
			else{
			    tot = strm->nmsgs;
			    rec = strm->recent;
			}
		    }
		    else{
			tot = strm->nmsgs;
			rec = sp_recent_since_visited(strm);
		    }
		}
		/*
		 * No, but how about another stream to same server which
		 * could be used for a STATUS command?
		 */
		else if((strm = sp_stream_get(mailbox_name, SP_SAME))
			&& modern_imap_stream(strm)){

		    extern MAILSTATUS mm_status_result;

		    mm_status_result.flags = 0L;
		    pine_mail_status(strm, mailbox_name,
				     SA_MESSAGES | SA_RECENT);
		    if(mm_status_result.flags & SA_MESSAGES
		       && mm_status_result.flags & SA_RECENT){
			tot = mm_status_result.messages;
			rec = mm_status_result.recent;
			gotit++;
		    }
		}

		/* Let's just Select it. */
		if(!gotit){
		    strm = pine_mail_open(NULL, mailbox_name,
					  OP_READONLY | SP_USEPOOL | SP_TEMPUSE,
					  NULL);
		    if(strm){
			tot = strm->nmsgs;
			rec = strm->recent;
			gotit++;
			pine_mail_close(strm);
		    }
		    else
		      sprintf(tmp_output,
			      "%.100s: Trouble checking for recent mail",
			      folder->name);
		}

		if(we_cancel)
		  cancel_busy_alarm(-1);

		if(gotit)
		  sprintf(tmp_output,
			  "%lu total message%.2s, %lu of them recent",
			  tot, plural(tot), rec);
	    }
        }else
	  strncpy(tmp_output, "No folder to check! Can't get recent info",
		  sizeof(tmp_output)-1);

	q_status_message(SM_ORDER, 0, 3, tmp_output);
        break;

  
            /*--------------- Invalid Command --------------*/
      default: 
	q_status_message1(SM_ORDER, 0, 2, "MIKE:  fix cmd = %x", (void *) cmd);
	break;
    }

    return(rv);
}


int
folder_lister_clickclick(sparms)
     SCROLL_S *sparms;
{
  if(!FPROC(sparms)->fs->list_cntxt)
    return(folder_lister_choice(sparms));
  else
    return(folder_processor(MC_SELCUR, ps_global->msgmap, sparms));
}

int
folder_lister_choice(sparms)
    SCROLL_S *sparms;
{
    int	       rv = 0, empty = 0;
    int	       index = (sparms->text.handles)
			 ? sparms->text.handles->h.f.index : 0;
    CONTEXT_S *cntxt = (sparms->text.handles)
			 ? sparms->text.handles->h.f.context : NULL;

    if(cntxt){
	
	FPROC(sparms)->fs->context = cntxt;

	if(cntxt->dir->status & CNTXT_NOFIND){
	    rv = 1;		/* leave scrolltool to rebuild screen */
	    FPROC(sparms)->fs->context = cntxt;
	    FPROC(sparms)->fs->first_folder[0] = '\0';
	}
	else if(folder_total(FOLDERS(cntxt))){
	    if(folder_lister_select(FPROC(sparms)->fs, cntxt, index)){
		rv = 1;		/* leave scrolltool to rebuild screen */
	    }
	    else if(FPROC(sparms)->fs->list_cntxt == cntxt){
		int	   n = 0, i, folder_n;
		FOLDER_S  *fp;
		STRLIST_S *sl = NULL, **slp;

		/* Scan folder list for selected items */
		folder_n = folder_total(FOLDERS(cntxt));
		slp = &sl;
		for(i = 0; i < folder_n; i++){
		    fp = folder_entry(i, FOLDERS(cntxt));
		    if(fp->selected){
			n++;
			if((*FPROC(sparms)->fs->f.valid)(fp,
							 FPROC(sparms)->fs)){
			    *slp = new_strlist();
			    (*slp)->name = folder_lister_fullname(
				FPROC(sparms)->fs, FLDR_NAME(fp));

			    slp = &(*slp)->next;
			}
			else{
			    free_strlist(&sl);
			    break;
			}
		    }
		}

		if(FPROC(sparms)->rv = sl)
		  FPROC(sparms)->done = rv = 1;
		else if(!n)
		  q_status_message(SM_ORDER, 0, 1, LISTMODE_GRIPE);
	    }
	    else
	      rv = folder_lister_finish(sparms, cntxt, index);
	}
	else
	  empty++;
    }
    else
      empty++;

    if(empty)
      q_status_message(SM_ORDER | SM_DING, 3, 3, "Empty folder list!");

    return(rv);
}


int
folder_lister_finish(sparms, cntxt, index)
    SCROLL_S  *sparms;
    CONTEXT_S *cntxt;
    int	       index;
{
    FOLDER_S *f = folder_entry(index, FOLDERS(cntxt));
    int	      rv = 0;

    if((*FPROC(sparms)->fs->f.valid)(f, FPROC(sparms)->fs)){
	/*
	 * Package up the selected folder names and return...
	 */
	FPROC(sparms)->fs->context = cntxt;
	FPROC(sparms)->rv = new_strlist();
	FPROC(sparms)->rv->name = folder_lister_fullname(FPROC(sparms)->fs,
							 FLDR_NAME(f));
	FPROC(sparms)->done = rv = 1;
    }

    return(rv);
}


/*
 * This is so that when you Save and use ^T to go to the folder list, and
 * you're in a directory with no folders, you have a way to add a new
 * folder there. The add actually gets done by the caller. This is just a
 * way to let the user type in the name.
 */
int
folder_lister_addmanually(sparms)
    SCROLL_S  *sparms;
{
    int        rc, flags = OE_APPEND_CURRENT, cnt = 0, rv = 0;
    char       addname[MAXFOLDER+1];
    HelpType   help;
    CONTEXT_S *cntxt = (sparms->text.handles)
			 ? sparms->text.handles->h.f.context
			 : FPROC(sparms)->fs->context;

    /*
     * Get the foldername from the user.
     */
    addname[0] = '\0';
    help = NO_HELP;
    while(1){
	rc = optionally_enter(addname, -FOOTER_ROWS(ps_global), 0,
			      sizeof(addname), "Name of new folder : ",
			      NULL, help, &flags);
	removing_leading_and_trailing_white_space(addname);

	if(rc == 3)
	  help = (help == NO_HELP) ? h_save_addman : NO_HELP;
	else if(rc == 1)
	  return(rv);
	else if(rc == 0){
	    if(F_OFF(F_ENABLE_DOT_FOLDERS,ps_global) && *addname == '.'){
		if(cnt++ <= 0)
                  q_status_message(SM_ORDER,3,3,
		    "Folder name can't begin with dot");
		else
		  q_status_message1(SM_ORDER,3,3,
		   "Config feature \"%.200s\" enables names beginning with dot",
		   feature_list_name(F_ENABLE_DOT_FOLDERS));

                display_message(NO_OP_COMMAND);
                continue;
	    }
	    else if(!strucmp(addname, ps_global->inbox_name)){
		q_status_message1(SM_ORDER, 3, 3,
				  "Can't add folder named %.200s",
				  ps_global->inbox_name);
		continue;
	    }

	    break;
	}
    }

    if(*addname){
	FPROC(sparms)->fs->context = cntxt;
	FPROC(sparms)->rv = new_strlist();
	FPROC(sparms)->rv->name = folder_lister_fullname(FPROC(sparms)->fs,
							 addname);
	FPROC(sparms)->done = rv = 1;
    }

    return(rv);
}


void
folder_lister_km_manager(sparms, handle_hidden)
    SCROLL_S *sparms;
    int	      handle_hidden;
{
    FOLDER_S *fp;

    /* if we're "in" a sub-directory, offer way out */
    if((sparms->text.handles)
	 ? sparms->text.handles->h.f.context->dir->prev
	 : FPROC(sparms)->fs->context->dir->prev){
	sparms->keys.menu->keys[KM_COL_KEY].bind.ch[0] = '<';
	sparms->keys.menu->keys[KM_COL_KEY].label      = "ParentDir";
	sparms->keys.menu->keys[KM_COL_KEY].bind.cmd   = MC_PARENT;
    }
    else if((FPROC(sparms)->fs->context->next
	     || FPROC(sparms)->fs->context->prev)
	    && !FPROC(sparms)->fs->combined_view){
	sparms->keys.menu->keys[KM_COL_KEY].bind.ch[0] = 'l';
	/* ch[0] was set to zero but that messes up mouse in xterm */
	/* set to 'l' instead, but could also be set to '<' if we */
	/* want to play it safe */
	sparms->keys.menu->keys[KM_COL_KEY].label      = "ClctnList";
	sparms->keys.menu->keys[KM_COL_KEY].bind.cmd   = MC_EXIT;
    }
    else{
	/*
	 * turn off "Main Menu" in keymenu and protect
	 * from rebinding below
	 */
	clrbitn(KM_MAIN_KEY, sparms->keys.bitmap);
	sparms->keys.menu->keys[KM_MAIN_KEY].bind.cmd = MC_NONE;
	sparms->keys.menu->keys[KM_MAIN_KEY].bind.nch = 0;

	sparms->keys.menu->keys[KM_COL_KEY].label      = "Main Menu";
	sparms->keys.menu->keys[KM_COL_KEY].bind.cmd   = MC_MAIN;
	sparms->keys.menu->keys[KM_COL_KEY].bind.ch[0] = 'm';
    }

    if(F_OFF(F_ENABLE_AGG_OPS,ps_global)){
	clrbitn(KM_ZOOM_KEY, sparms->keys.bitmap);
	clrbitn(KM_SELECT_KEY, sparms->keys.bitmap);
	clrbitn(KM_SELCUR_KEY, sparms->keys.bitmap);
    }

    if(sparms->text.handles
       && (fp = folder_entry(sparms->text.handles->h.f.index,
			     FOLDERS(sparms->text.handles->h.f.context)))){
	if(fp->isdir){
	    if(fp->isfolder){
		sparms->keys.menu->keys[KM_SEL_KEY].label = "View Dir";
		menu_clear_binding(sparms->keys.menu, 'v');
		menu_clear_binding(sparms->keys.menu, ctrl('M'));
		menu_clear_binding(sparms->keys.menu, ctrl('J'));
		menu_add_binding(sparms->keys.menu, 'v', MC_OPENFLDR);
		menu_add_binding(sparms->keys.menu, ctrl('M'), MC_OPENFLDR);
		menu_add_binding(sparms->keys.menu, ctrl('J'), MC_OPENFLDR);
		setbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
		setbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
		setbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
	    }
	    else{
		sparms->keys.menu->keys[KM_SEL_KEY].label = "[View Dir]";
		menu_add_binding(sparms->keys.menu, 'v', MC_CHOICE);
		menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
		menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
		clrbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
		clrbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
		clrbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
	    }
	}
	else{
	    sparms->keys.menu->keys[KM_SEL_KEY].label = "[View Fldr]";
	    menu_add_binding(sparms->keys.menu, 'v', MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	    clrbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
	    setbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
	    setbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
	}
    }
    else if(FPROC(sparms)->fs->combined_view
	    && sparms->text.handles && sparms->text.handles->h.f.context
	    && !sparms->text.handles->h.f.context->dir->prev){
	sparms->keys.menu->keys[KM_SEL_KEY].label = "[View Cltn]";
	menu_add_binding(sparms->keys.menu, 'v', MC_CHOICE);
	menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	clrbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
	clrbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
	clrbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
    }
    else{
	clrbitn(KM_SEL_KEY, sparms->keys.bitmap);
	clrbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
	clrbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
	clrbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
    }
      
    if((sparms->text.handles &&
        sparms->text.handles->h.f.context &&
        sparms->text.handles->h.f.context->use & CNTXT_INCMNG) ||
       (FPROC(sparms) && FPROC(sparms)->fs &&
        FPROC(sparms)->fs->context &&
        FPROC(sparms)->fs->context->use & CNTXT_INCMNG))
      setbitn(KM_SHUFFLE_KEY, sparms->keys.bitmap);
    else
      clrbitn(KM_SHUFFLE_KEY, sparms->keys.bitmap);

    if(F_ON(F_TAB_CHK_RECENT, ps_global)){
	menu_clear_binding(sparms->keys.menu, TAB);
	menu_init_binding(sparms->keys.menu, TAB, MC_CHK_RECENT, "Tab",
			  "NewMsgs", KM_RECENT_KEY);
	setbitn(KM_RECENT_KEY, sparms->keys.bitmap);
    }
    else{
	menu_clear_binding(sparms->keys.menu, TAB);
	menu_add_binding(sparms->keys.menu, TAB, MC_NEXT_HANDLE);
	clrbitn(KM_RECENT_KEY, sparms->keys.bitmap);
    }

    /* May have to "undo" what scrolltool "did" */
    if(F_ON(F_ARROW_NAV, ps_global)){
	if(F_ON(F_RELAXED_ARROW_NAV, ps_global)){
	    menu_clear_binding(sparms->keys.menu, KEY_LEFT);
	    menu_add_binding(sparms->keys.menu, KEY_LEFT, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_RIGHT);
	    menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_NEXT_HANDLE);
	}
	else{
	    menu_clear_binding(sparms->keys.menu, KEY_UP);
	    menu_add_binding(sparms->keys.menu, KEY_UP, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_DOWN);
	    menu_add_binding(sparms->keys.menu, KEY_DOWN, MC_NEXT_HANDLE);
	}
    }
}


void
folder_lister_km_sel_manager(sparms, handle_hidden)
    SCROLL_S *sparms;
    int	      handle_hidden;
{
    FOLDER_S *fp;

    /* if we're "in" a sub-directory, offer way out */
    if((sparms->text.handles)
	 ? sparms->text.handles->h.f.context->dir->prev
	 : FPROC(sparms)->fs->context->dir->prev){
	sparms->keys.menu->keys[FC_COL_KEY].name       = "<";
	sparms->keys.menu->keys[FC_COL_KEY].label      = "ParentDir";
	sparms->keys.menu->keys[FC_COL_KEY].bind.cmd   = MC_PARENT;
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[0] = '<';
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[1] = ',';
	if(F_ON(F_ARROW_NAV,ps_global)){
	  sparms->keys.menu->keys[FC_COL_KEY].bind.nch = 3;
	  sparms->keys.menu->keys[FC_COL_KEY].bind.ch[2] = KEY_LEFT;
	}
	else
	  sparms->keys.menu->keys[FC_COL_KEY].bind.nch   = 2;
	setbitn(FC_EXIT_KEY, sparms->keys.bitmap);
    }
    else if((FPROC(sparms)->fs->context->next
	     || FPROC(sparms)->fs->context->prev)
	    && !FPROC(sparms)->fs->combined_view){
	sparms->keys.menu->keys[FC_COL_KEY].name       = "<";
	sparms->keys.menu->keys[FC_COL_KEY].label      = "ClctnList";
	sparms->keys.menu->keys[FC_COL_KEY].bind.cmd   = MC_COLLECTIONS;
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[0] = '<';
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[1] = ',';
	if(F_ON(F_ARROW_NAV,ps_global)){
	  sparms->keys.menu->keys[FC_COL_KEY].bind.nch = 3;
	  sparms->keys.menu->keys[FC_COL_KEY].bind.ch[2] = KEY_LEFT;
	}
	else
	  sparms->keys.menu->keys[FC_COL_KEY].bind.nch   = 2;
	setbitn(FC_EXIT_KEY, sparms->keys.bitmap);
    }
    else if(FPROC(sparms)->fs->combined_view){
	/*
	 * turn off "ExitSelect" in first slot
	 */
	sparms->keys.menu->keys[FC_COL_KEY].name       = "E";
	sparms->keys.menu->keys[FC_COL_KEY].label      = "ExitSelect";
	sparms->keys.menu->keys[FC_COL_KEY].bind.cmd   = MC_EXIT;
	sparms->keys.menu->keys[FC_COL_KEY].bind.nch   = 1;
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[0] = 'e';
	clrbitn(FC_EXIT_KEY, sparms->keys.bitmap);
    }

    /* clean up per-entry bindings */
    clrbitn(FC_SEL_KEY, sparms->keys.bitmap);
    clrbitn(FC_ALTSEL_KEY, sparms->keys.bitmap);
    menu_clear_binding(sparms->keys.menu, ctrl('M'));
    menu_clear_binding(sparms->keys.menu, ctrl('J'));
    menu_clear_binding(sparms->keys.menu, '>');
    menu_clear_binding(sparms->keys.menu, '.');
    menu_clear_binding(sparms->keys.menu, 's');
    if(F_ON(F_ARROW_NAV,ps_global))
      menu_clear_binding(sparms->keys.menu, KEY_RIGHT);

    /* and then re-assign them as needed */
    if(sparms->text.handles
       && (fp = folder_entry(sparms->text.handles->h.f.index,
			     FOLDERS(sparms->text.handles->h.f.context)))){
	setbitn(FC_SEL_KEY, sparms->keys.bitmap);
	if(fp->isdir){
	    sparms->keys.menu->keys[FC_SEL_KEY].name = ">";
	    menu_add_binding(sparms->keys.menu, '>', MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, '.', MC_CHOICE);
	    if(F_ON(F_ARROW_NAV,ps_global))
	      menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_CHOICE);

	    if(fp->isfolder){
		sparms->keys.menu->keys[FC_SEL_KEY].label = "View Dir";
		setbitn(FC_ALTSEL_KEY, sparms->keys.bitmap);
		menu_add_binding(sparms->keys.menu, 's', MC_OPENFLDR);
		menu_add_binding(sparms->keys.menu, ctrl('M'), MC_OPENFLDR);
		menu_add_binding(sparms->keys.menu, ctrl('J'), MC_OPENFLDR);
	    }
	    else{
		sparms->keys.menu->keys[FC_SEL_KEY].label = "[View Dir]";
		menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
		menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	    }
	}
	else{
	    sparms->keys.menu->keys[FC_SEL_KEY].name  = "S";
	    sparms->keys.menu->keys[FC_SEL_KEY].label = "[Select]";

	    menu_add_binding(sparms->keys.menu, 's', MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	}

	if(F_ON(F_ARROW_NAV,ps_global))
	  menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_CHOICE);
    }
    else if(FPROC(sparms)->fs->combined_view
	    && sparms->text.handles && sparms->text.handles->h.f.context
	    && !sparms->text.handles->h.f.context->dir->prev){
	setbitn(FC_SEL_KEY, sparms->keys.bitmap);
	sparms->keys.menu->keys[FC_SEL_KEY].name = ">";
	sparms->keys.menu->keys[FC_SEL_KEY].label = "[View Cltn]";
	menu_add_binding(sparms->keys.menu, '>', MC_CHOICE);
	menu_add_binding(sparms->keys.menu, '.', MC_CHOICE);
	if(F_ON(F_ARROW_NAV,ps_global))
	  menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_CHOICE);

	menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
    }

    /* May have to "undo" what scrolltool "did" */
    if(F_ON(F_ARROW_NAV, ps_global)){
	if(F_ON(F_RELAXED_ARROW_NAV, ps_global)){
	    menu_clear_binding(sparms->keys.menu, KEY_LEFT);
	    menu_add_binding(sparms->keys.menu, KEY_LEFT, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_RIGHT);
	    menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_NEXT_HANDLE);
	}
	else{
	    menu_clear_binding(sparms->keys.menu, KEY_UP);
	    menu_add_binding(sparms->keys.menu, KEY_UP, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_DOWN);
	    menu_add_binding(sparms->keys.menu, KEY_DOWN, MC_NEXT_HANDLE);
	}
    }
}



void
folder_lister_km_sub_manager(sparms, handle_hidden)
    SCROLL_S *sparms;
    int	      handle_hidden;
{
    if(FPROC(sparms)->fs->list_cntxt){
	clrbitn(SB_LIST_KEY, sparms->keys.bitmap);
	sparms->keys.menu->keys[SB_SEL_KEY].name = "X";
	sparms->keys.menu->keys[SB_SEL_KEY].label = "[Set/Unset]";
	sparms->keys.menu->keys[SB_SEL_KEY].bind.cmd = MC_SELCUR;
	sparms->keys.menu->keys[SB_SEL_KEY].bind.ch[0] = 'x';
    }
    else{
	clrbitn(SB_SUB_KEY, sparms->keys.bitmap);
	sparms->keys.menu->keys[SB_SEL_KEY].name = "S";
	sparms->keys.menu->keys[SB_SEL_KEY].label = "[Subscribe]";
	sparms->keys.menu->keys[SB_SEL_KEY].bind.cmd = MC_CHOICE;
	sparms->keys.menu->keys[SB_SEL_KEY].bind.ch[0] = 's';
    }

    /* May have to "undo" what scrolltool "did" */
    if(F_ON(F_ARROW_NAV, ps_global)){
	if(F_ON(F_RELAXED_ARROW_NAV, ps_global)){
	    menu_clear_binding(sparms->keys.menu, KEY_LEFT);
	    menu_add_binding(sparms->keys.menu, KEY_LEFT, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_RIGHT);
	    menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_NEXT_HANDLE);
	}
	else{
	    menu_clear_binding(sparms->keys.menu, KEY_UP);
	    menu_add_binding(sparms->keys.menu, KEY_UP, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_DOWN);
	    menu_add_binding(sparms->keys.menu, KEY_DOWN, MC_NEXT_HANDLE);
	}
    }
}



int
folder_select(ps, context, cur_index)
    struct pine *ps;
    CONTEXT_S	*context;
    int		 cur_index;
{
    int        i, j, n, total, old_tot, diff, 
	       q = 0, rv = 0, narrow = 0;
    HelpType   help = NO_HELP;
    ESCKEY_S  *sel_opts;
    FOLDER_S  *f;
    static ESCKEY_S self_opts2[] = {
	{'a', 'a', "A", "select All"},
	{'c', 'c', "C", "select Cur"},
	{'p', 'p', "P", "Properties"},
	{'t', 't', "T", "Text"},
	{-1, 0, NULL, NULL}
    };
    extern     ESCKEY_S sel_opts1[];
    extern     char *sel_pmt2;

    f = folder_entry(cur_index, FOLDERS(context));

    sel_opts = self_opts2;
    if(old_tot = selected_folders(context)){
	sel_opts1[1].label = "unselect Cur" + (f->selected ? 0 : 2);
	sel_opts += 2;			/* disable extra options */
	switch(q = radio_buttons(SEL_ALTER_PMT, -FOOTER_ROWS(ps_global),
				 sel_opts1, 'c', 'x', help, RB_NORM, NULL)){
	  case 'f' :			/* flip selection */
	    n = folder_total(FOLDERS(context));
	    for(total = i = 0; i < n; i++)
	      if(f = folder_entry(i, FOLDERS(context)))
		f->selected = !f->selected;

	    return(1);			/* repaint */

	  case 'n' :			/* narrow selection */
	    narrow++;
	  case 'b' :			/* broaden selection */
	    q = 0;			/* but don't offer criteria prompt */
	    if(context->use & CNTXT_INCMNG){
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
			 "Select \"%s\" not supported in Incoming Folders",
			 narrow ? "Narrow" : "Broaden");
		return(0);
	    }

	    break;

	  case 'c' :			/* Un/Select Current */
	  case 'a' :			/* Unselect All */
	  case 'x' :			/* cancel */
	    break;

	  default :
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Unsupported Select option");
	    return(0);
	}
    }

    if(!q)
      q = radio_buttons(sel_pmt2, -FOOTER_ROWS(ps_global),
			sel_opts, 'c', 'x', help, RB_NORM, NULL);

    /*
     * NOTE: See note about MESSAGECACHE "searched" bits above!
     */
    switch(q){
      case 'x':				/* cancel */
	cmd_cancelled("Select command");
	return(0);

      case 'c' :			/* toggle current's selected state */
	return(folder_select_toggle(context, cur_index, folder_select_update));

      case 'a' :			/* select/unselect all */
	n = folder_total(FOLDERS(context));
	for(total = i = 0; i < n; i++)
	  folder_entry(i, FOLDERS(context))->selected = old_tot == 0;

	q_status_message4(SM_ORDER, 0, 2,
			  "%.200s%.200s folder%.200s %.200sselected",
			  old_tot ? "" : "All ",
			  comatose(old_tot ? old_tot : n),
			  plural(old_tot ? old_tot : n), old_tot ? "UN" : "");
	return(1);

      case 't' :			/* Text */
	if(!folder_select_text(ps, context, narrow))
	  rv++;

	break;

      case 'p' :			/* Properties */
	if(!folder_select_props(ps, context, narrow))
	  rv++;

	break;

      default :
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Unsupported Select option");
	return(0);
    }

    if(rv)
      return(0);

    /* rectify the scanned vs. selected folders */
    n = folder_total(FOLDERS(context));
    for(total = i = j = 0; i < n; i++)
      if(folder_entry(i, FOLDERS(context))->scanned)
	break;
    
    /*
     * Any matches at all? If not, the selected set remains the same.
     * Note that when Narrowing, only matches in the intersection will
     * show up as scanned. We need to reset i to 0 to erase any already
     * selected messages which aren't in the intersection.
     */
    if(i < n)
      for(i = 0; i < n; i++)
	if(f = folder_entry(i, FOLDERS(context))){
	    if(narrow){
		if(f->selected){
		    f->selected = f->scanned;
		    j++;
		}
	    }
	    else if(f->scanned)
	      f->selected = 1;
	}

    if(!(diff = (total = selected_folders(context)) - old_tot)){
	if(narrow)
	  q_status_message4(SM_ORDER, 0, 2,
			  "%.200s.  %.200s folder%.200s remain%.200s selected.",
			    j ? "No change resulted"
			      : "No messages in intersection",
			    comatose(old_tot), plural(old_tot),
			    (old_tot == 1L) ? "s" : "");
	else if(old_tot && j)
	  q_status_message(SM_ORDER, 0, 2,
		   "No change resulted.  Matching folders already selected.");
	else
	  q_status_message1(SM_ORDER | SM_DING, 0, 2,
			    "Select failed!  No %sfolders selected.",
			    old_tot ? "additional " : "");
    }
    else if(old_tot){
	sprintf(tmp_20k_buf,
		"Select matched %ld folder%s.  %s %sfolder%s %sselected.",
		(diff > 0) ? diff : old_tot + diff,
		plural((diff > 0) ? diff : old_tot + diff),
		comatose((diff > 0) ? total : -diff),
		(diff > 0) ? "total " : "",
		plural((diff > 0) ? total : -diff),
		(diff > 0) ? "" : "UN");
	q_status_message(SM_ORDER, 0, 2, tmp_20k_buf);
    }
    else
      q_status_message2(SM_ORDER, 0, 2, "Select matched %.200s folder%.200s.",
			comatose(diff), plural(diff));

    return(1);
}



int
selected_folders(context)
    CONTEXT_S *context;
{
    int	      i, n, total;

    n = folder_total(FOLDERS(context));
    for(total = i = 0; i < n; i++)
      if(folder_entry(i, FOLDERS(context))->selected)
	total++;

    return(total);
}


SELECTED_S *
new_selected()
{
  SELECTED_S *selp;
  
  selp = (SELECTED_S *)fs_get(sizeof(SELECTED_S));
  selp->sub = NULL;
  selp->reference = NULL;
  selp->folders = NULL;
  selp->zoomed = 0;

  return selp;
}

/*
 *  Free the current selected struct and all of the 
 *  following structs in the list
 */
void
free_selected(selp)
    SELECTED_S **selp;
{
    if(!selp || !(*selp))
      return;
    if((*selp)->sub)
      free_selected(&((*selp)->sub));

    free_strlist(&(*selp)->folders);
    if((*selp)->reference)
      fs_give((void **) &(*selp)->reference);

    fs_give((void **) selp);
}

void
folder_select_preserve(context)
    CONTEXT_S *context;
{
    if(context
       && !(context->use & CNTXT_PARTFIND)){
	FOLDER_S   *fp;
	STRLIST_S **slpp;
	SELECTED_S *selp = &context->selected;
	int	    i, folder_n;

	if(!context->dir->ref){
	  if(!context->selected.folders)
	    slpp     = &context->selected.folders;
	  else
	    return;
	}
	else{
	  if(!selected_folders(context))
	    return;
	  else{
	    while(selp->sub){
	      selp = selp->sub;
	      if(!strcmp(selp->reference, context->dir->ref))
		return;
	    }
	    selp->sub = new_selected();
	    selp = selp->sub;
	    slpp = &(selp->folders);
	  }
	}
	folder_n = folder_total(FOLDERS(context));

	for(i = 0; i < folder_n; i++)
	  if((fp = folder_entry(i, FOLDERS(context)))->selected){
	      *slpp = new_strlist();
	      (*slpp)->name = cpystr(fp->name);
	      slpp = &(*slpp)->next;
	  }

	/* Only remember "ref" if any folders were selected */
	if(selp->folders && context->dir->ref)
	  selp->reference = cpystr(context->dir->ref);

	selp->zoomed = (context->use & CNTXT_ZOOM) != 0;
    }
}



int
folder_select_restore(context)
    CONTEXT_S *context;
{
    int rv = 0;

    if(context
       && !(context->use & CNTXT_PARTFIND)){
	STRLIST_S *slp;
	SELECTED_S *selp, *pselp = NULL;
	int	   i, found = 0;

	selp = &(context->selected);

	if(context->dir->ref){
	  pselp = selp;
	  selp = selp->sub;
	  while(selp && strcmp(selp->reference, context->dir->ref)){
	    pselp = selp;
	    selp = selp->sub;
	  }
	  if (selp)
	    found = 1;
	}
	else 
	  found = selp->folders != 0;
	if(found){
	  for(slp = selp->folders; slp; slp = slp->next)
	    if(slp->name
	       && (i = folder_index(slp->name, context, FI_FOLDER)) >= 0){
		folder_entry(i, FOLDERS(context))->selected = 1;
		rv++;
	    }

	  /* Used, always clean them up */
	  free_strlist(&selp->folders);
	  if(selp->reference)
	    fs_give((void **) &selp->reference);

	  if(selp->zoomed){
	    context->use |= CNTXT_ZOOM;
	    selp->zoomed = 0;
	  }
	  if(!(selp == &context->selected)){
	    if(pselp){
	      pselp->sub = selp->sub;
	      fs_give((void **) &selp);
	    }
	  }
	}
    }

    return(rv);
}



int
folder_lister_select(fs, context, index)
    FSTATE_S  *fs;
    CONTEXT_S *context;
    int	       index;
{
    int       rv = 0;
    FDIR_S   *fp;
    FOLDER_S  *f = folder_entry(index, FOLDERS(context));

    /*--- Entering a directory?  ---*/
    if(f->isdir){
	fp = next_folder_dir(context, f->name, TRUE, fs->cache_streamp);

	/* Provide context in new collection header */
	fp->desc = folder_lister_desc(context, fp);

	/* Insert new directory into list */
	free_folder_list(context);
	fp->prev	  = context->dir;
	fp->status	 |= CNTXT_SUBDIR;
	context->dir  = fp;
	q_status_message2(SM_ORDER, 0, 3, "Now in %.200sdirectory: %.200s",
			  folder_total(FOLDERS(context))
			  ? "" : "EMPTY ",  fp->ref);
	rv++;
    }
    else
      rv = folder_lister_parent(fs, context, index, 1);

    return(rv);
}



char *
folder_lister_desc(cntxt, fdp)
    CONTEXT_S *cntxt;
    FDIR_S    *fdp;
{
    char *p;

    /* Provide context in new collection header */
    sprintf(tmp_20k_buf, "Dir: %s",
	    ((p = strstr(cntxt->context, "%s")) && !*(p+2)
	     && !strncmp(fdp->ref, cntxt->context, p - cntxt->context))
	      ? fdp->ref + (p - cntxt->context) : fdp->ref);
    return(cpystr(tmp_20k_buf));
}



int
folder_lister_parent(fs, context, index, force_parent)
    FSTATE_S  *fs;
    CONTEXT_S *context;
    int	       index;
    int        force_parent;
{
    int       rv = 0;
    FDIR_S   *fp;
    FOLDER_S *f = context ? folder_entry(index, FOLDERS(context)) : NULL;

    if(!force_parent && (fp = context->dir->prev)){
	char *s, oldir[MAILTMPLEN];

	folder_select_preserve(context);
	oldir[0] = '\0';
	if(s = strrindex(context->dir->ref, context->dir->delim)){
	    *s = '\0';
	    if(s = strrindex(context->dir->ref, context->dir->delim)){
		strncpy(oldir, s+1, sizeof(oldir)-1);
		oldir[sizeof(oldir)-1] = '\0';
	    }
	}

	if(*oldir){
	    /* remember current directory for hiliting in new list */
	    fs->context = context;
	    if(strlen(oldir) < MAXFOLDER - 1){
		strcpy(fs->first_folder, oldir);
		fs->first_dir = 1;
	    }
	}

	free_fdir(&context->dir, 0);
	fp->status |= CNTXT_NOFIND;

	context->dir = fp;

	if(fp->status & CNTXT_SUBDIR)
	  q_status_message1(SM_ORDER, 0, 3, "Now in directory: %.200s",
			    strsquish(tmp_20k_buf + 500, fp->ref,
				      ps_global->ttyo->screen_cols - 22));
	else
	  q_status_message(SM_ORDER, 0, 3,
			   "Returned to collection's top directory");

	rv++;
    }

    return(rv);
}


char *
folder_lister_fullname(fs, name)
    FSTATE_S *fs;
    char     *name;
{
    if(fs->context->dir->status & CNTXT_SUBDIR){
	char tmp[2*MAILTMPLEN], tmp2[2*MAILTMPLEN], *p;

	if(fs->context->dir->ref)
	  sprintf(tmp, "%.*s%.*s",
		  sizeof(tmp)/2,
		  ((fs->relative_path || (fs->context->use & CNTXT_SAVEDFLT))
		   && (p = strstr(fs->context->context, "%s")) && !*(p+2)
		   && !strncmp(fs->context->dir->ref, fs->context->context,
			       p - fs->context->context))
		    ? fs->context->dir->ref + (p - fs->context->context)
		    : fs->context->dir->ref,
		  sizeof(tmp)/2, name);

	/*
	 * If the applied name is still ambiguous (defined
	 * that way by the user (i.e., "mail/[]"), then
	 * we better fix it up...
	 */
	if(context_isambig(tmp)
	   && !fs->relative_path
	   && !(fs->context->use & CNTXT_SAVEDFLT)){
	    /* if it's in the primary collection, the names relative */
	    if(fs->context->dir->ref){
		if(IS_REMOTE(fs->context->context)
		   && (p = strrindex(fs->context->context, '}')))
		  sprintf(tmp2, "%.*s%.*s",
			  min(p - fs->context->context + 1, sizeof(tmp2)/2),
			  fs->context->context,
			  sizeof(tmp2)/2, tmp);
		else
		  build_path(tmp2, ps_global->ui.homedir, tmp, sizeof(tmp2));
	    }
	    else
	      (void) context_apply(tmp2, fs->context, tmp, sizeof(tmp2));

	    return(cpystr(tmp2));
	}

	return(cpystr(tmp));
    }

    return(cpystr(name));
}


/*
 * Export a folder from pine space to user's space. It will still be a regular
 * mail folder but it will be in the user's home directory or something like
 * that.
 */
void
folder_export(sparms)
    SCROLL_S *sparms;
{
    FOLDER_S   *f;
    MAILSTREAM *stream, *ourstream = NULL;
    char        expanded_file[MAILTMPLEN], *p, cut[50],
		tmp[MAILTMPLEN], *fname, *fullname = NULL,
		filename[MAXPATH+1], full_filename[MAXPATH+1],
		deefault[MAXPATH+1];
    int	        open_inbox = 0, we_cancel = 0, width,
		index = (sparms && sparms->text.handles)
			 ? sparms->text.handles->h.f.index : 0;
    CONTEXT_S  *savecntxt,
	       *cntxt = (sparms && sparms->text.handles)
			 ? sparms->text.handles->h.f.context : NULL;

    dprint(4, (debugfile, "\n - folder export -\n"));

    if(cntxt){
	if(folder_total(FOLDERS(cntxt))){
	    f = folder_entry(index, FOLDERS(cntxt));
	    if((*FPROC(sparms)->fs->f.valid)(f, FPROC(sparms)->fs)){
		savecntxt = FPROC(sparms)->fs->context;   /* necessary? */
		FPROC(sparms)->fs->context = cntxt;
		strncpy(deefault, FLDR_NAME(f), sizeof(deefault)-1);
		deefault[sizeof(deefault)-1] = '\0';
		fname = folder_lister_fullname(FPROC(sparms)->fs, FLDR_NAME(f));
		FPROC(sparms)->fs->context = savecntxt;

		/*
		 * We have to allow for INBOX and nicknames in
		 * the incoming collection. Mimic what happens in
		 * do_broach_folder.
		 */
		strncpy(expanded_file, fname, sizeof(expanded_file));
		expanded_file[sizeof(expanded_file)-1] = '\0';

		if(strucmp(fname, ps_global->inbox_name) == 0
		   || strucmp(fname, ps_global->VAR_INBOX_PATH) == 0)
		  open_inbox++;

		if(!open_inbox && cntxt && context_isambig(fname)){
		    if(p=folder_is_nick(fname, FOLDERS(cntxt), 0)){
			strncpy(expanded_file, p, sizeof(expanded_file));
			expanded_file[sizeof(expanded_file)-1] = '\0';
		    }
		    else if ((cntxt->use & CNTXT_INCMNG)
			     && (folder_index(fname, cntxt, FI_FOLDER) < 0)
			     && !is_absolute_path(fname)){
			q_status_message1(SM_ORDER, 3, 4,
			    "Can't find Incoming Folder %.200s.", fname);
			return;
		    }
		}

		if(open_inbox)
		  fullname = ps_global->VAR_INBOX_PATH;
		else{
		    /*
		     * We don't want to interpret this name in the context
		     * of the reference string, that was already done
		     * above in folder_lister_fullname(), just in the
		     * regular context. We also don't want to lose the
		     * reference string because we will still be in the
		     * subdirectory after this operation completes. So
		     * temporarily zero out the reference.
		     */
		    FDIR_S *tmpdir;

		    tmpdir = (cntxt ? cntxt->dir : NULL);
		    cntxt->dir = NULL;
		    fullname = context_apply(tmp, cntxt, expanded_file,
					     sizeof(tmp));
		    cntxt->dir = tmpdir;
		}

		width = max(20,
			   ps_global->ttyo ? ps_global->ttyo->screen_cols : 80);
		stream = sp_stream_get(fullname, SP_MATCH | SP_RO_OK);
		if(!stream && fullname){
		    /*
		     * Just using filename and full_filename as convenient
		     * temporary buffers here.
		     */
		    sprintf(filename, "Opening \"%.80s\"",
			    short_str(fullname, full_filename,
				      max(10, width-17),
				      MidDots));
		    we_cancel = busy_alarm(1, filename, NULL, 1);
		    stream = pine_mail_open(NULL, fullname,
					    OP_READONLY|SP_USEPOOL|SP_TEMPUSE,
					    NULL);
		    if(we_cancel)
		      cancel_busy_alarm(0);

		    ourstream = stream;
		}

		/*
		 * We have a stream for the folder we want to
		 * export.
		 */
		if(stream && stream->nmsgs > 0L){
		    int r = 1;
		    static ESCKEY_S eopts[] = {
			{ctrl('T'), 10, "^T", "To Files"},
			{-1, 0, NULL, NULL},
			{-1, 0, NULL, NULL}};

		    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
			eopts[r].ch    =  ctrl('I');
			eopts[r].rval  = 11;
			eopts[r].name  = "TAB";
			eopts[r].label = "Complete";
		    }

		    eopts[++r].ch = -1;

		    filename[0] = '\0';
		    full_filename[0] = '\0';

		    r = get_export_filename(ps_global, filename, deefault,
					    full_filename,
					    sizeof(filename)-20, fname, NULL,
					    eopts, NULL,
					    -FOOTER_ROWS(ps_global),
					    GE_IS_EXPORT | GE_NO_APPEND);
		    if(r < 0){
			switch(r){
			  default:
			  case -1:
			    cmd_cancelled("Export folder");
			    break;

			  case -2:
			    q_status_message1(SM_ORDER, 0, 2,
				      "Can't export to file outside of %.200s",
					      ps_global->VAR_OPER_DIR);
			    break;
			}
		    }
		    else{
			int ok;
			char *ff;

			ps_global->mm_log_error = 0;

			/*
			 * Do the creation in standard unix format, so it
			 * is readable by all.
			 */
			rplstr(full_filename, 0, "#driver.unix/");
			ok = pine_mail_create(NULL, full_filename) != 0L;
			/*
			 * ff points to the original filename, without the
			 * driver prefix. Only mail_create knows how to
			 * handle driver prefixes.
			 */
			ff = full_filename + strlen("#driver.unix/");

			if(!ok){
			    if(!ps_global->mm_log_error)
			      q_status_message(SM_ORDER | SM_DING, 3, 3,
					       "Error creating file");
			}
			else{
			    long     l, snmsgs;
			    MSGNO_S *tmpmap = NULL;

			    snmsgs = stream->nmsgs;
			    mn_init(&tmpmap, snmsgs);
			    for(l = 1L; l <= snmsgs; l++)
			      if(l == 1L)
				mn_set_cur(tmpmap, l);
			      else
				mn_add_cur(tmpmap, l);
			    
			    blank_keymenu(ps_global->ttyo->screen_rows-2, 0);
			    we_cancel = busy_alarm(1, "Copying folder",
						   NULL, 1);
			    l = save(ps_global, stream, NULL, ff, tmpmap, 0);
			    if(we_cancel)
			      cancel_busy_alarm(0);

			    mn_give(&tmpmap);

			    if(l == snmsgs)
			      q_status_message2(SM_ORDER, 0, 3,
					    "Folder %.200s exported to %.200s",
					        fname, filename);
			    else{
				q_status_message1(SM_ORDER | SM_DING, 3, 3,
						  "Error exporting to %.200s",
						  filename);
				dprint(2, (debugfile,
		    "Error exporting to %s: expected %ld msgs, saved %ld\n",
		    filename, snmsgs, l));
			    }
			}
		    }
		}
		else if(stream)
		  q_status_message1(SM_ORDER|SM_DING, 3, 3,
				    "No messages in %.200s to export", fname);
		else
		  q_status_message(SM_ORDER|SM_DING, 3, 3,
				   "Can't open folder for exporting");

		if(fname)
		  fs_give((void **) &fname);

		if(ourstream)
		  pine_mail_close(ourstream);
	    }
	}
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3, "Empty folder list!");
}


/*
 * Import a folder from user's space back to pine space.
 * We're just importing a regular mail folder, and saving all the messages
 * to another folder. It may seem more magical to the user but it isn't.
 * The import folder is a local folder, the new one can be remote or whatever.
 * Args  sparms
 *       add_folder -- return new folder name here
 *       len        -- length of add_folder
 *
 * Returns 1 if we may have to redraw screen, 0 otherwise
 */
int
folder_import(sparms, add_folder, len)
    SCROLL_S *sparms;
    char     *add_folder;
    size_t    len;
{
    MAILSTREAM *istream = NULL;
    int         r = 1, rv = 0;
    char        filename[MAXPATH+1], full_filename[MAXPATH+1];
    static ESCKEY_S eopts[] = {
	{ctrl('T'), 10, "^T", "To Files"},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	eopts[r].ch    =  ctrl('I');
	eopts[r].rval  = 11;
	eopts[r].name  = "TAB";
	eopts[r].label = "Complete";
    }

    eopts[++r].ch = -1;

    filename[0] = '\0';
    full_filename[0] = '\0';

    /* get a folder to import */
    r = get_export_filename(ps_global, filename, NULL, full_filename,
			    sizeof(filename)-20, "messages", "IMPORT",
			    eopts, NULL,
			    -FOOTER_ROWS(ps_global), GE_IS_IMPORT);
    if(r < 0){
	switch(r){
	  default:
	  case -1:
	    cmd_cancelled("Import folder");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
		      "Can't import file outside of %.200s",
			      ps_global->VAR_OPER_DIR);
	    break;
	}
    }
    else{
	ps_global->mm_log_error = 0;
	if(full_filename && full_filename[0])
	  istream = pine_mail_open(NULL, full_filename,
				   OP_READONLY | SP_TEMPUSE, NULL);

	if(istream && istream->nmsgs > 0L){
	    long       l;
	    int        we_cancel = 0;
	    char       newfolder[MAILTMPLEN], nmsgs[32];
	    MSGNO_S   *tmpmap = NULL;
	    CONTEXT_S *cntxt, *ourcntxt;

	    cntxt = (sparms && sparms->text.handles)
			 ? sparms->text.handles->h.f.context : NULL;
	    ourcntxt = cntxt;
	    newfolder[0] = '\0';
	    sprintf(nmsgs, "%.10s msgs ", comatose(istream->nmsgs));

	    /*
	     * Select a folder to save the messages to.
	     */
	    if(save_prompt(ps_global, &cntxt, newfolder, sizeof(newfolder),
			   nmsgs, NULL, 0L, NULL, NULL)){

		if((cntxt == ourcntxt) && newfolder[0]){
		    rv = 1;
		    strncpy(add_folder, newfolder, len-1);
		    add_folder[len-1] = '\0';
		    free_folder_list(cntxt);
		}

		mn_init(&tmpmap, istream->nmsgs);
		for(l = 1; l <= istream->nmsgs; l++)
		  if(l == 1L)
		    mn_set_cur(tmpmap, l);
		  else
		    mn_add_cur(tmpmap, l);
		
		blank_keymenu(ps_global->ttyo->screen_rows-2, 0);
		we_cancel = busy_alarm(1, "Importing messages",
				       NULL, 1);
		l = save(ps_global, istream, cntxt, newfolder, tmpmap, 0);
		if(we_cancel)
		  cancel_busy_alarm(0);

		mn_give(&tmpmap);

		if(l == istream->nmsgs)
		  q_status_message2(SM_ORDER, 0, 3,
				    "Folder %.200s imported to %.200s",
				    full_filename, newfolder);
		else
		  q_status_message1(SM_ORDER | SM_DING, 3, 3,
				    "Error importing to %.200s",
				    newfolder);
	    }
	}
	else if(istream)
	  q_status_message1(SM_ORDER | SM_DING, 3, 3,
			    "No messages in file %.200s",
			    full_filename);
	else if(!ps_global->mm_log_error)
	  q_status_message1(SM_ORDER | SM_DING, 3, 3,
			    "Can't open file %.200s for import", full_filename);
    }

    if(istream)
      pine_mail_close(istream);
    
    return(rv);
}


/*
 *
 */
void
reset_context_folders(cntxt)
    CONTEXT_S *cntxt;
{
    CONTEXT_S *tc;

    for(tc = cntxt; tc && tc->prev ; tc = tc->prev)
      ;				/* start at beginning */

    for( ; tc ; tc = tc->next){
	free_folder_list(tc);

	while(tc->dir->prev){
	    FDIR_S *tp = tc->dir->prev;
	    free_fdir(&tc->dir, 0);
	    tc->dir = tp;
	}
    }
}



/*
 * next_folder_dir - return a directory structure with the folders it
 *		     contains
 */
FDIR_S *
next_folder_dir(context, new_dir, build_list, streamp)
    CONTEXT_S   *context;
    char        *new_dir;
    int	         build_list;
    MAILSTREAM **streamp;
{
    char      tmp[MAILTMPLEN], dir[3];
    FDIR_S   *tmp_fp, *fp;

    fp = (FDIR_S *) fs_get(sizeof(FDIR_S));
    memset(fp, 0, sizeof(FDIR_S));
    (void) context_apply(tmp, context, new_dir, MAILTMPLEN);
    dir[0] = context->dir->delim;
    dir[1] = '\0';
    strncat(tmp, dir, sizeof(tmp)-1-strlen(tmp));
    fp->ref	      = cpystr(tmp);
    fp->delim	      = context->dir->delim;
    fp->view.internal = cpystr(NEWS_TEST(context) ? "*" : "%");
    fp->folders	      = init_folder_entries();
    fp->status	      = CNTXT_NOFIND;
    tmp_fp	      = context->dir;	/* temporarily rebind */
    context->dir      = fp;

    if(build_list)
      build_folder_list(streamp, context, NULL, NULL,
			NEWS_TEST(context) ? BFL_LSUB : BFL_NONE);

    context->dir = tmp_fp;
    return(fp);
}



int
folder_select_toggle(context, index, func)
    CONTEXT_S *context;
    int	       index;
    int      (*func) PROTO((CONTEXT_S *, int));
{
    FOLDER_S *f;

    if(f = folder_entry(index, FOLDERS(context))){
      f->selected = !f->selected;
      return((*func)(context, index));
    }
    return 1;
}



/*
 * mail_cmd_stream - return a stream suitable for mail_lsub,
 *		      mail_subscribe, and mail_unsubscribe
 *
 */
MAILSTREAM *
mail_cmd_stream(context, closeit)
    CONTEXT_S *context;
    int	      *closeit;
{
    char	tmp[MAILTMPLEN];

    *closeit = 1;
    (void) context_apply(tmp, context, "x", sizeof(tmp));

    return(pine_mail_open(NULL, tmp,
			  OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE,
			  NULL));
}

/*
 *  Find the next '}' that isn't part of a "${"
 *  that appear for environment variables.  We need
 *  this for configuration, because we want to edit
 *  the original pinerc entry, and not the digested
 *  value.
 */
char *
end_bracket_no_nest(str)
    char *str;
{
    char *p;

    for(p = str; *p; p++){
	if(*p == '$' && *(p+1) == '{'){
	    while(*p && *p != '}')
	      p++;
	    if(!*p)
	      return NULL;
	}
	else if(*p == '}')
	  return p;
    }
    /* if we get here then there was no trailing '}' */
    return NULL;
}

/*----------------------------------------------------------------------
      Create a new folder

   Args: context    -- context we're adding in
	 which      -- which config file to operate on
	 varnum     -- which varnum to operate on
         add_folder -- return new folder here
	 len        -- length of add_folder
	 possible_stream -- possible stream for re-use
	 def        -- default value to start out with for add_folder
			(for now, only for inbox-path editing)

 Result: returns nonzero on successful add, 0 otherwise
  ----*/
int
add_new_folder(context, which, varnum, add_folder, len, possible_stream, def)
    CONTEXT_S  *context;
    EditWhich   which;
    int         varnum;
    char       *add_folder;
    size_t      len;
    MAILSTREAM *possible_stream;
    char       *def;
{
    char	 tmp[max(MAXFOLDER,MAX_SCREEN_COLS)+1], nickname[32], 
		*p = NULL, *return_val = NULL, buf[MAILTMPLEN],
		buf2[MAILTMPLEN], def_in_prompt[MAILTMPLEN];
    HelpType     help;
    int          i, rc, offset, exists, cnt = 0, isdir = 0;
    int          maildrop = 0, flags = 0, inbox = 0;
    char        *maildropfolder = NULL, *maildroplongname = NULL;
    char        *default_mail_drop_host = NULL,
		*default_mail_drop_folder = NULL,
		*default_dstn_host = NULL,
		*default_dstn_folder = NULL,
		*copydef = NULL,
	        *dstnmbox = NULL;
    char         mdmbox[MAILTMPLEN], ctmp[MAILTMPLEN];
    MAILSTREAM  *create_stream = NULL;
    FOLDER_S    *f;
    static ESCKEY_S add_key[] = {{ctrl('X'),12,"^X", NULL},
				 {-1, 0, NULL, NULL}};

    dprint(4, (debugfile, "\n - add_new_folder - \n"));

    add_folder[0] = '\0';
    nickname[0]   = '\0';
    inbox = (varnum == V_INBOX_PATH);

    if(inbox || context->use & CNTXT_INCMNG){
	char inbox_host[MAXPATH], *beg, *end = NULL;
	int readonly = 0;
	PINERC_S *prc = NULL;
	static ESCKEY_S host_key[4];

	if(ps_global->restricted)
	  readonly = 1;
	else{
	    switch(which){
	      case Main:
		prc = ps_global->prc;
		break;
	      case Post:
		prc = ps_global->post_prc;
		break;
	      case None:
		break;
	    }

	    readonly = prc ? prc->readonly : 1;
	}

	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return(FALSE);
	}

	if(readonly){
	    q_status_message(SM_ORDER,3,5,
		"Cancelled: config file not editable");
	    return(FALSE);
	}

	/*
	 * When adding an Incoming folder we can't supply the inbox host
	 * as a default, because then the user has no way to type just
	 * a plain RETURN to mean "no host, I want a local folder".
	 * So we supply it as a ^X command instead. We could supply it
	 * as the initial value of the string...
	 *
	 * When editing inbox-path we will supply the default value as an
	 * initial value of the string which can be edited. They can edit it
	 * or erase it entirely to mean no host.
	 */

	if(inbox && def){

	    copydef = cpystr(def);
	    (void) removing_double_quotes(copydef);

	    if(check_for_move_mbox(copydef, mdmbox, sizeof(mdmbox), &dstnmbox)){

		/*
		 * Current inbox is using a mail drop. Get the default
		 * host value for the maildrop.
		 */

		if(mdmbox
		   && (mdmbox[0] == '{'
		       || (mdmbox[0] == '*' && mdmbox[1] == '{'))
		   && (end = end_bracket_no_nest(mdmbox+1))
		   && end-mdmbox < len){
		    *end = '\0';
		    if(mdmbox[0] == '{')
		      default_mail_drop_host = cpystr(mdmbox+1);
		    else
		      default_mail_drop_host = cpystr(mdmbox+2);
		}

		if(!default_mail_drop_host)
		  default_mail_drop_folder = cpystr(mdmbox);
		else if(end && *(end+1))
		  default_mail_drop_folder = cpystr(end+1);

		end = NULL;
		if(dstnmbox
		   && (*dstnmbox == '{'
		       || (*dstnmbox == '*' && *++dstnmbox == '{'))
		   && (end = end_bracket_no_nest(dstnmbox+1))
		   && end-dstnmbox < len){
		    *end = '\0';
		    default_dstn_host = cpystr(dstnmbox+1);
		}

		if(!default_dstn_host)
		  default_dstn_folder = cpystr(dstnmbox);
		else if(end && *(end+1))
		  default_dstn_folder = cpystr(end+1);

		maildrop++;
	    }
	    else{
		end = NULL;
		dstnmbox = copydef;
		if(dstnmbox
		   && (*dstnmbox == '{'
		       || (*dstnmbox == '*' && *++dstnmbox == '{'))
		   && (end = end_bracket_no_nest(dstnmbox+1))
		   && end-dstnmbox < len){
		    *end = '\0';
		    default_dstn_host = cpystr(dstnmbox+1);
		}

		if(!default_dstn_host)
		  default_dstn_folder = cpystr(dstnmbox);
		else if(end && *(end+1))
		  default_dstn_folder = cpystr(end+1);
	    }

	    if(copydef)
	      fs_give((void **) &copydef);
	}

get_folder_name:

	i = 0;
	host_key[i].ch      = 0;
	host_key[i].rval    = 0;
	host_key[i].name    = "";
	host_key[i++].label = "";

	inbox_host[0] = '\0';
	if(!inbox && (beg = ps_global->VAR_INBOX_PATH)
	   && (*beg == '{' || (*beg == '*' && *++beg == '{'))
	   && (end = strindex(ps_global->VAR_INBOX_PATH, '}'))){
	    strncpy(inbox_host, beg+1, end - beg);
	    inbox_host[end - beg - 1] = '\0';
	    host_key[i].ch      = ctrl('X');
	    host_key[i].rval    = 12;
	    host_key[i].name    = "^X";
	    host_key[i++].label = "Use Inbox Host";
	}
	else{
	    host_key[i].ch      = 0;
	    host_key[i].rval    = 0;
	    host_key[i].name    = "";
	    host_key[i++].label = "";
	}

	if(!maildrop && !maildropfolder){
	    host_key[i].ch      = ctrl('W');
	    host_key[i].rval    = 13;
	    host_key[i].name    = "^W";
	    host_key[i++].label = "Use a Mail Drop";
	}
	else if(maildrop){
	    host_key[i].ch      = ctrl('W');
	    host_key[i].rval    = 13;
	    host_key[i].name    = "^W";
	    host_key[i++].label = "Do Not use a Mail Drop";
	}

	host_key[i].ch      = -1;
	host_key[i].rval    = 0;
	host_key[i].name    = NULL;
	host_key[i].label = NULL;

	if(maildrop)
	  sprintf(tmp, "Name of Mail Drop server : ");
	else if(maildropfolder)
	  sprintf(tmp, "Name of server to contain destination folder : ");
	else if(inbox)
	  sprintf(tmp, "Name of Inbox server : ");
	else
	  sprintf(tmp, "Name of server to contain added folder : ");

	help = NO_HELP;

	/* set up defaults */
	if(inbox && def){
	    flags = OE_APPEND_CURRENT;
	    if(maildrop && default_mail_drop_host){
		strncpy(add_folder, default_mail_drop_host, len);
		add_folder[len-1] = '\0';
	    }
	    else if(!maildrop && default_dstn_host){
		strncpy(add_folder, default_dstn_host, len);
		add_folder[len-1] = '\0';
	    }
	    else
	      add_folder[0] = '\0';
	}
	else{
	    flags = 0;
	    add_folder[0] = '\0';
	}

	while(1){
	    rc = optionally_enter(add_folder, -FOOTER_ROWS(ps_global), 0,
				  len, tmp, host_key, help, &flags);
	    removing_leading_and_trailing_white_space(add_folder);

	    /*
	     * User went for the whole enchilada and entered a maildrop
	     * completely without going through the steps.
	     * Split it up as if they did and then skip over
	     * some of the code.
	     */
	    if(check_for_move_mbox(add_folder, mdmbox, sizeof(mdmbox),
				   &dstnmbox)){
		maildrop = 1;
		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		maildropfolder = cpystr(mdmbox);

		strncpy(add_folder, dstnmbox, len);
		add_folder[len-1] = '\0';
		offset = 0;
		goto skip_over_folder_input;
	    }

	    /*
	     * Now check to see if they entered a whole c-client
	     * spec that isn't a mail drop.
	     */
	    if(add_folder[0] == '{'
	       && add_folder[1] != '\0'
	       && (p = srchstr(add_folder, "}"))
	       && *(p+1) != '\0'){
		offset = p+1 - add_folder;
		goto skip_over_folder_input;
	    }

	    /*
	     * And check to see if they entered "INBOX".
	     */
	    if(!strucmp(add_folder, ps_global->inbox_name)){
		offset = 0;
		goto skip_over_folder_input;
	    }

	    /* remove surrounding braces */
	    if(add_folder[0] == '{' && add_folder[1] != '\0'){
		char *q;

		q = add_folder + strlen(add_folder) - 1;
		if(*q == '}'){
		    *q = '\0';
		    for(q = add_folder; *q; q++)
		      *q = *(q+1);
		}
	    }

	    if(rc == 3){
		if(maildropfolder && inbox)
		  helper(h_inbox_add_maildrop_destn,
			 "HELP FOR DESTINATION SERVER ", HLPD_SIMPLE);
		else if(maildropfolder && !inbox)
		  helper(h_incoming_add_maildrop_destn,
			 "HELP FOR DESTINATION SERVER ", HLPD_SIMPLE);
		else if(maildrop && inbox)
		  helper(h_inbox_add_maildrop, "HELP FOR MAILDROP NAME ",
		         HLPD_SIMPLE);
		else if(maildrop && !inbox)
		  helper(h_incoming_add_maildrop, "HELP FOR MAILDROP NAME ",
		         HLPD_SIMPLE);
		else if(inbox)
		  helper(h_incoming_add_inbox, "HELP FOR INBOX SERVER ",
		         HLPD_SIMPLE);
		else
		  helper(h_incoming_add_folder_host, "HELP FOR SERVER NAME ",
		         HLPD_SIMPLE);

		ps_global->mangled_screen = 1;
	    }
	    else if(rc == 12){
		strncpy(add_folder, inbox_host, len);
		flags |= OE_APPEND_CURRENT;
	    }
	    else if(rc == 13){
		if(maildrop){
		    maildrop = 0;
		    if(maildropfolder)
		      fs_give((void **) &maildropfolder);
		}
		else{
		    maildrop++;
		    if(inbox && def){
			default_mail_drop_host = default_dstn_host;
			default_dstn_host = NULL;
			default_mail_drop_folder = default_dstn_folder;
			default_dstn_folder = NULL;
		    }
		}

		goto get_folder_name;
	    }
	    else if(rc == 1){
		q_status_message(SM_ORDER,0,2,
				 inbox ? "INBOX change cancelled"
				       : "Addition of new folder cancelled");
		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		if(inbox && def){
		    if(default_mail_drop_host)
		      fs_give((void **) &default_mail_drop_host);

		    if(default_mail_drop_folder)
		      fs_give((void **) &default_mail_drop_folder);

		    if(default_dstn_host)
		      fs_give((void **) &default_dstn_host);

		    if(default_dstn_folder)
		      fs_give((void **) &default_dstn_folder);
		}

		return(FALSE);
	    }
	    else if(rc == 0)
	      break;
	}
    }

    /* set up default folder, if any */
    def_in_prompt[0] = '\0';
    if(inbox && def){
	if(maildrop && default_mail_drop_folder){
	    strncpy(def_in_prompt, default_mail_drop_folder,
		    sizeof(def_in_prompt));
	    def_in_prompt[sizeof(def_in_prompt)-1] = '\0';
	}
	else if(!maildrop && default_dstn_folder){
	    strncpy(def_in_prompt, default_dstn_folder,
		    sizeof(def_in_prompt));
	    def_in_prompt[sizeof(def_in_prompt)-1] = '\0';
	}
    }

    if(offset = strlen(add_folder)){		/* must be host for incoming */
	int i;
	if(maildrop)
	  sprintf(tmp,
	    "Maildrop folder on \"%.100s\" to copy mail from%.5s%.100s%.5s : ",
	    short_str(add_folder, buf, 15, EndDots),
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else if(maildropfolder)
	  sprintf(tmp,
	    "Folder on \"%.100s\" to copy mail to%.5s%.100s%.5s : ",
	    short_str(add_folder, buf, 20, EndDots),
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else if(inbox)
	  sprintf(tmp,
	    "Folder on \"%.100s\" to use for INBOX%.5s%.100s%.5s : ",
	    short_str(add_folder, buf, 20, EndDots),
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else
	  sprintf(tmp,
	    "Folder on \"%.100s\" to add%.5s%.100s%.5s : ",
	    short_str(add_folder, buf, 25, EndDots),
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, 20, MidDots),
	    def_in_prompt[0] ? "]" : "");

	for(i = offset;i >= 0; i--)
	  add_folder[i+1] = add_folder[i];

	add_folder[0] = '{';
	add_folder[++offset] = '}';
	add_folder[++offset] = '\0';		/* +2, total */
    }
    else{
	if(maildrop)
	  sprintf(tmp,
	    "Folder to copy mail from%.5s%.100s%.5s : ",
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else if(maildropfolder)
	  sprintf(tmp,
	    "Folder to copy mail to%.5s%.100s%.5s : ",
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else if(inbox)
	  sprintf(tmp,
	    "Folder name to use for INBOX%.5s%.100s%.5s : ",
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else
	  sprintf(tmp,
	    "Folder name to add%.5s%.100s%.5s : ",
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
    }

    help = NO_HELP;
    while(1){

	p = NULL;
	if(isdir){
	    add_key[0].label = "Create Folder";
	    if(tmp[0] == 'F')
	      rplstr(tmp, 6, "Directory");
	}
	else{
	    add_key[0].label = "Create Directory";
	    if(tmp[0] == 'D')
	      rplstr(tmp, 9, "Folder");
	}

	flags = OE_APPEND_CURRENT;
        rc = optionally_enter(&add_folder[offset], -FOOTER_ROWS(ps_global), 0, 
			      len - offset, tmp,
			      ((context->dir->delim) && !maildrop)
				  ? add_key : NULL,
			      help, &flags);
	/* use default */
	if(rc == 0 && def_in_prompt[0] && !add_folder[offset]){
	    strncpy(&add_folder[offset], def_in_prompt, len-offset);
	    add_folder[len-1] = '\0';
	}

	removing_leading_and_trailing_white_space(&add_folder[offset]);

	if(rc == 0 && !(inbox || context->use & CNTXT_INCMNG)
	   && check_for_move_mbox(add_folder, NULL, 0L, NULL)){
	    q_status_message(SM_ORDER, 6, 6,
	  "#move folders may only be the INBOX or in the Incoming Collection");
	    display_message(NO_OP_COMMAND);
	    continue;
	}

        if(rc == 0 && add_folder[offset]){
	    if(F_OFF(F_ENABLE_DOT_FOLDERS,ps_global)
	       && add_folder[offset] == '.'){
		if(cnt++ <= 0)
		  q_status_message(SM_ORDER,3,3,
				   "Folder name can't begin with dot");
		else
		  q_status_message1(SM_ORDER,3,3,
		   "Config feature \"%.200s\" enables names beginning with dot",
		      feature_list_name(F_ENABLE_DOT_FOLDERS));

                display_message(NO_OP_COMMAND);
                continue;
	    }

	    /* if last char is delim, blast from new folder */
	    for(p = &add_folder[offset]; *p && *(p + 1); p++)
	      ;				/* find last char in folder */

	    if(isdir){
		if(*p && *p != context->dir->delim){
		    *++p   = context->dir->delim;
		    *(p+1) = '\0';
		}
	    }
	    else if(*p == context->dir->delim){
		q_status_message(SM_ORDER|SM_DING, 3, 3,
				 "Can't have trailing directory delimiters!");
		display_message('X');
		continue;
	    }

	    break;
	}

	if(rc == 12){
	    isdir = !isdir;		/* toggle directory creation */
	}
        else if(rc == 3){
	    helper(h_incoming_add_folder_name, "HELP FOR FOLDER NAME ",
		   HLPD_SIMPLE);
	}
	else if(rc == 1 || add_folder[0] == '\0') {
	    q_status_message(SM_ORDER,0,2,
			     inbox ? "INBOX change cancelled"
				   : "Addition of new folder cancelled");
	    if(maildropfolder)
	      fs_give((void **) &maildropfolder);

	    if(inbox && def){
		if(default_mail_drop_host)
		  fs_give((void **) &default_mail_drop_host);

		if(default_mail_drop_folder)
		  fs_give((void **) &default_mail_drop_folder);

		if(default_dstn_host)
		  fs_give((void **) &default_dstn_host);

		if(default_dstn_folder)
		  fs_give((void **) &default_dstn_folder);
	    }

	    return(FALSE);
	}
    }

    if(maildrop && !maildropfolder){
	maildropfolder = cpystr(add_folder);
	maildrop = 0;
	goto get_folder_name;
    }

skip_over_folder_input:

    create_stream = sp_stream_get(context_apply(ctmp, context, add_folder,
						sizeof(ctmp)),
				  SP_SAME);

    if(!create_stream && possible_stream)
      create_stream = context_same_stream(context, add_folder, possible_stream);

    help = NO_HELP;
    if(!inbox && context->use & CNTXT_INCMNG){
	sprintf(tmp, "Nickname for folder \"%.100s\" : ", &add_folder[offset]);
	while(1){
	    int flags = OE_APPEND_CURRENT;

	    rc = optionally_enter(nickname, -FOOTER_ROWS(ps_global), 0,
				  sizeof(nickname), tmp, NULL, help, &flags);
	    removing_leading_and_trailing_white_space(nickname);
	    if(rc == 0){
		if(strucmp(ps_global->inbox_name, nickname))
		  break;
		else{
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				    "Nickname cannot be \"%.200s\"",
				    nickname);
		}
	    }

	    if(rc == 3){
		help = help == NO_HELP
			? h_incoming_add_folder_nickname : NO_HELP;
	    }
	    else if(rc == 1 || (rc != 3 && !*nickname)){
		q_status_message(SM_ORDER,0,2,
				 inbox ? "INBOX change cancelled"
				       : "Addition of new folder cancelled");
		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		if(inbox && def){
		    if(default_mail_drop_host)
		      fs_give((void **) &default_mail_drop_host);

		    if(default_mail_drop_folder)
		      fs_give((void **) &default_mail_drop_folder);

		    if(default_dstn_host)
		      fs_give((void **) &default_dstn_host);

		    if(default_dstn_folder)
		      fs_give((void **) &default_dstn_folder);
		}

		return(FALSE);
	    }
	}

	/*
	 * Already exist?  First, make sure this name won't collide with
	 * anything else in the list.  Next, quickly test to see if it
	 * the actual mailbox exists so we know any errors from 
	 * context_create() are really bad...
	 */
	for(offset = 0; offset < folder_total(FOLDERS(context)); offset++){
	    f = folder_entry(offset, FOLDERS(context));
	    if(!strucmp(FLDR_NAME(f), nickname[0] ? nickname : add_folder)){
		q_status_message1(SM_ORDER | SM_DING, 0, 3,
				  "Incoming folder \"%.200s\" already exists",
				  nickname[0] ? nickname : add_folder);
		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		if(inbox && def){
		    if(default_mail_drop_host)
		      fs_give((void **) &default_mail_drop_host);

		    if(default_mail_drop_folder)
		      fs_give((void **) &default_mail_drop_folder);

		    if(default_dstn_host)
		      fs_give((void **) &default_dstn_host);

		    if(default_dstn_folder)
		      fs_give((void **) &default_dstn_folder);
		}

		return(FALSE);
	    }
	}

	ps_global->c_client_error[0] = '\0';
	exists = folder_exists(context, add_folder);
	if(exists == FEX_ERROR){
	    if(ps_global->c_client_error[0] != '\0')
	      q_status_message1(SM_ORDER, 3, 3, "%.200s",
			        ps_global->c_client_error);
	    else
	      q_status_message1(SM_ORDER, 3, 3, "Error checking for %.200s",
			        add_folder);
	}
    }
    else if(!inbox)
      exists = FEX_NOENT;
    else{
	exists = FEX_ISFILE;
	/*
	 * If inbox is a maildropfolder, try to create the destination
	 * folder. But it shouldn't cause a fatal error.
	 */
	if(maildropfolder && (folder_exists(NULL, add_folder) == FEX_NOENT))
	  context_create(NULL, NULL, add_folder);
    }

    if(exists == FEX_ERROR
       || (exists == FEX_NOENT
	   && !context_create(context, create_stream, add_folder)
	   && !((context->use & CNTXT_INCMNG)
		&& !context_isambig(add_folder)))){
	if(maildropfolder)
	  fs_give((void **) &maildropfolder);

	if(inbox && def){
	    if(default_mail_drop_host)
	      fs_give((void **) &default_mail_drop_host);

	    if(default_mail_drop_folder)
	      fs_give((void **) &default_mail_drop_folder);

	    if(default_dstn_host)
	      fs_give((void **) &default_dstn_host);

	    if(default_dstn_folder)
	      fs_give((void **) &default_dstn_folder);
	}

	return(FALSE);		/* c-client should've reported error */
    }

    if(isdir && p)				/* whack off trailing delim */
      *p = '\0';

    if(inbox || context->use & CNTXT_INCMNG){
	char  **apval;
	char ***alval;

	if(inbox){
	    apval = APVAL(&ps_global->vars[varnum], which);
	    if(apval && *apval)
	      fs_give((void **) apval);
	}
	else{
	    alval = ALVAL(&ps_global->vars[varnum], which);
	    if(!*alval){
		offset = 0;
		*alval = (char **) fs_get(2*sizeof(char *));
	    }
	    else{
		for(offset=0;  (*alval)[offset]; offset++)
		  ;

		fs_resize((void **) alval, (offset + 2) * sizeof(char *));
	    }
	}

	/*
	 * If we're using a Mail Drop we have to assemble the correct
	 * c-client string to do that. Mail drop syntax looks like
	 *
	 *   #move <DELIM> <FromMailbox> <DELIM> <ToMailbox>
	 *
	 * DELIM is any character which does not appear in either of
	 * the mailbox names.
	 *
	 * And then the nickname is still in front of that mess.
	 */
	if(maildropfolder){
	    char *delims = " +-_:!|ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	    char *c;

	    maildroplongname = (char *) fs_get((5+2+1+strlen(maildropfolder)+
					        strlen(add_folder)) *
					       sizeof(char));
	    for(c = delims; *c; c++){
		if(!strindex(maildropfolder, *c) &&
		   !strindex(add_folder, *c))
		  break;
	    }

	    if(*c){
		sprintf(maildroplongname, "#move%c%.*s%c%.*s",
			*c, strlen(maildropfolder), maildropfolder,
			*c, strlen(add_folder), add_folder);
		if(strlen(maildroplongname) < len)
		  strcpy(add_folder, maildroplongname);

		fs_give((void **) &maildroplongname);
	    }
	    else{
		q_status_message2(SM_ORDER,0,2,
		    "Can't find delimiter for \"#move %.100s %.100s\"",
		    maildropfolder, add_folder);
		dprint(2, (debugfile,
		    "Can't find delimiter for \"#move %.100s %.100s\"",
		    maildropfolder ? maildropfolder : "?",
		    add_folder ? add_folder : "?"));

		if(maildroplongname)
		  fs_give((void **) &maildroplongname);

		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		if(inbox && def){
		    if(default_mail_drop_host)
		      fs_give((void **) &default_mail_drop_host);

		    if(default_mail_drop_folder)
		      fs_give((void **) &default_mail_drop_folder);

		    if(default_dstn_host)
		      fs_give((void **) &default_dstn_host);

		    if(default_dstn_folder)
		      fs_give((void **) &default_dstn_folder);
		}

		return(FALSE);
	    }
	}

	if(inbox)
	  *apval = cpystr(add_folder);
	else{
	    (*alval)[offset]   = put_pair(nickname, add_folder);
	    (*alval)[offset+1] = NULL;
	}

	set_current_val(&ps_global->vars[varnum], TRUE, FALSE);
	write_pinerc(ps_global, which, WRP_NONE);

	/*
	 * Instead of inserting the new folder in the list of folders,
	 * and risking bugs associated with that, 
	 * we re-initialize from the config variable.
	 */
	if(context->use & CNTXT_INCMNG)
	  reinit_incoming_folder_list(ps_global, context);

	if(nickname[0]){
	    strncpy(add_folder, nickname, len-1);  /* known by new name */
	    add_folder[len-1] = '\0';
	}

	if(!inbox)
	  q_status_message1(SM_ORDER, 0, 3, "Folder \"%.200s\" created",
			    add_folder);
	return_val = add_folder;
    }
    else if(context_isambig(add_folder)){
	free_folder_list(context);
	q_status_message2(SM_ORDER, 0, 3, "%.200s \"%.200s\" created",
			  isdir ? "Directory" : "Folder", add_folder);
	return_val = add_folder;
    }
    else
      q_status_message1(SM_ORDER, 0, 3,
			"Folder \"%.200s\" created outside current collection",
			add_folder);

    if(maildropfolder)
      fs_give((void **) &maildropfolder);

    if(inbox && def){
	if(default_mail_drop_host)
	  fs_give((void **) &default_mail_drop_host);

	if(default_mail_drop_folder)
	  fs_give((void **) &default_mail_drop_folder);

	if(default_dstn_host)
	  fs_give((void **) &default_dstn_host);

	if(default_dstn_folder)
	  fs_give((void **) &default_dstn_folder);
    }

    return(return_val != NULL);
}



/*----------------------------------------------------------------------
    Subscribe to a news group

   Args: folder -- last folder added
         cntxt  -- The context the subscription is for

 Result: returns the name of the folder subscribed too


This builds a complete context for the entire list of possible news groups. 
It also build a context to find the newly created news groups as 
determined by data kept in .pinerc.  When the find of these new groups is
done the subscribed context is searched and the items marked as new. 
A list of new board is never actually created.

  ----*/
int
group_subscription(folder, len, cntxt)
     char      *folder;
     size_t     len;
     CONTEXT_S *cntxt;
{
    STRLIST_S  *folders = NULL;
    int		rc = 0, last_rc, i, n, flags,
		last_find_partial = 0, we_cancel = 0;
    CONTEXT_S	subscribe_cntxt;
    HelpType	help;
    ESCKEY_S	subscribe_keys[3];

    subscribe_keys[i = 0].ch  = ctrl('T');
    subscribe_keys[i].rval    = 12;
    subscribe_keys[i].name    = "^T";
    subscribe_keys[i++].label = "To All Grps";

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	subscribe_keys[i].ch	= ctrl('I');
	subscribe_keys[i].rval  = 11;
	subscribe_keys[i].name  = "TAB";
	subscribe_keys[i++].label = "Complete";
    }

    subscribe_keys[i].ch = -1;

    /*---- Build a context to find all news groups -----*/
    
    subscribe_cntxt	      = *cntxt;
    subscribe_cntxt.use      |= CNTXT_FINDALL;
    subscribe_cntxt.use      &= ~(CNTXT_PSEUDO | CNTXT_ZOOM | CNTXT_PRESRV);
    subscribe_cntxt.next      = NULL;
    subscribe_cntxt.prev      = NULL;
    subscribe_cntxt.dir       = new_fdir(cntxt->dir->ref,
					 cntxt->dir->view.internal, '*');
    FOLDERS(&subscribe_cntxt) = init_folder_entries();
    /*
     * Prompt for group name.
     */
    folder[0] = '\0';
    help      = NO_HELP;
    while(1){
	flags = OE_APPEND_CURRENT;
	last_rc = rc;
        rc = optionally_enter(folder, -FOOTER_ROWS(ps_global), 0, len,
			      SUBSCRIBE_PMT, subscribe_keys, help, &flags);
	removing_trailing_white_space(folder);
	removing_leading_white_space(folder);
        if((rc == 0 && folder[0]) || rc == 11 || rc == 12){
	    we_cancel = busy_alarm(1, "Fetching newsgroup list", NULL, 0);

	    if(last_find_partial){
		/* clean up any previous find results */
		free_folder_list(&subscribe_cntxt);
		last_find_partial = 0;
	    }

	    if(rc == 11){			/* Tab completion! */
		if(folder_complete_internal(&subscribe_cntxt,
					    folder, &n, FC_FORCE_LIST)){
		    continue;
		}
		else{
		    if(!(n && last_rc == 11 && !(flags & OE_USER_MODIFIED))){
			Writechar(BELL, 0);
			continue;
		    }
		}
	    }

	    if(rc == 12){			/* list the whole enchilada */
		build_folder_list(NULL, &subscribe_cntxt, NULL, NULL,BFL_NONE);
	    }
	    else if(i = strlen(folder)){
		char tmp[MAILTMPLEN];

		sprintf(tmp, "%s%.*s*", (rc == 11) ? "" : "*",
			sizeof(tmp)-3, folder);
		build_folder_list(NULL, &subscribe_cntxt, tmp, NULL, BFL_NONE);
		subscribe_cntxt.dir->status &= ~(CNTXT_PARTFIND|CNTXT_NOFIND);
	    }
	    else{
		q_status_message(SM_ORDER, 0, 2,
	       "No group substring to match! Use ^T to list all news groups.");
		continue;
	    }

	    /*
	     * If we did a partial find on matches, then we faked a full
	     * find which will cause this to just return.
	     */
	    if(i = folder_total(FOLDERS(&subscribe_cntxt))){
		char *f;

		/*
		 * fake that we've found everything there is to find...
		 */
		subscribe_cntxt.use &= ~(CNTXT_NOFIND|CNTXT_PARTFIND);
		last_find_partial = 1;

		if(i == 1){
		    f = folder_entry(0, FOLDERS(&subscribe_cntxt))->name;
		    if(!strcmp(f, folder)){
			rc = 1;			/* success! */
			break;
		    }
		    else{			/* else complete the group */
			strncpy(folder, f, len-1);
			folder[len-1] = '\0';
			continue;
		    }
		}
		else if(!(flags & OE_USER_MODIFIED)){
		    /*
		     * See if there wasn't an exact match in the lot.
		     */
		    while(i-- > 0){
			f = folder_entry(i,FOLDERS(&subscribe_cntxt))->name;
			if(!strcmp(f, folder))
			  break;
			else
			  f = NULL;
		    }

		    /* if so, then the user picked it from the list the
		     * last time and didn't change it at the prompt.
		     * Must mean they're accepting it...
		     */
		    if(f){
			rc = 1;			/* success! */
			break;
		    }
		}
	    }
	    else{
		if(rc == 12)
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
				   "No groups to select from!");
		else
		  q_status_message1(SM_ORDER, 3, 3,
			  "News group \"%.200s\" didn't match any existing groups",
			  folder);
		free_folder_list(&subscribe_cntxt);

		continue;
	    }

	    /*----- Mark groups that are currently subscribed too ------*/
	    /* but first make sure they're found */
	    build_folder_list(NULL, cntxt, "*", NULL, BFL_LSUB);
	    for(i = 0 ; i < folder_total(FOLDERS(&subscribe_cntxt)); i++) {
		FOLDER_S *f = folder_entry(i, FOLDERS(&subscribe_cntxt));

		f->subscribed = search_folder_list(FOLDERS(cntxt),
						   f->name) != 0;
	    }

	    if(we_cancel)
	      cancel_busy_alarm(-1);

	    /*----- Call the folder lister to do all the work -----*/
	    folders = folders_for_subscribe(ps_global,
					    &subscribe_cntxt, folder);

	    if(folders){
		/* Multiple newsgroups OR Auto-select */
		if(folders->next || F_ON(F_SELECT_WO_CONFIRM,ps_global))
		  break;

		strncpy(folder, (char *) folders->name, len-1);
		folder[len-1] = '\0';
		free_strlist(&folders);
	    }
	}
        else if(rc == 3){
            help = help == NO_HELP ? h_news_subscribe : NO_HELP;
	}
	else if(rc == 1 || folder[0] == '\0'){
	    rc = -1;
	    break;
	}
    }

    if(rc < 0){
	folder[0] = '\0';		/* make sure not to return partials */
	if(rc == -1)
	  q_status_message(SM_ORDER, 0, 3, "Subscribe cancelled");
    }
    else{
	MAILSTREAM *sub_stream;
	int sclose = 0, errors = 0;

	if(folders){		/*------ Actually do the subscription -----*/
	    STRLIST_S *flp;
	    int	       n = 0;

	    /* subscribe one at a time */
	    folder[0] = '\0';
	    /*
	     * Open stream before subscribing so c-client knows what newsrc
	     * to use, along with other side-effects.
	     */
	    if(sub_stream = mail_cmd_stream(&subscribe_cntxt, &sclose)){
		for(flp = folders; flp; flp = flp->next){
		    (void) context_apply(tmp_20k_buf, &subscribe_cntxt,
					 (char *) flp->name, SIZEOF_20KBUF);
		    if(mail_subscribe(sub_stream, tmp_20k_buf) == 0L){
			/*
			 * This message may not make it to the screen,
			 * because a c-client message about the failure
			 * will be there.  Probably best not to string
			 * together a whole bunch of errors if there is
			 * something wrong.
			 */
			q_status_message1(errors ?SM_INFO : SM_ORDER,
					  errors ? 0 : 3, 3,
					  "Error subscribing to \"%.200s\"",
					  (char *) flp->name);
			errors++;
		    }
		    else{
			n++;
			if(!folder[0]){
			    strncpy(folder, (char *) flp->name, len-1);
			    folder[len-1] = '\0';
			}

			/*---- Update the screen display data structures -----*/
			if(ALL_FOUND(cntxt)){
			    if(cntxt->use & CNTXT_PSEUDO){
				folder_delete(0, FOLDERS(cntxt));
				cntxt->use &= ~CNTXT_PSEUDO;
			    }

			    folder_insert(-1, new_folder((char *) flp->name, 0),
					  FOLDERS(cntxt));
			}
		    }
		}
		if(sclose)
		  pine_mail_close(sub_stream);
	    }
	    else
	      errors++;

	    if(n == 0)
	      q_status_message(SM_ORDER | SM_DING, 3, 5,
			  "Subscriptions failed, subscribed to no new groups");
	    else
	      q_status_message3(SM_ORDER | (errors ? SM_DING : 0),
				errors ? 3 : 0,3,
				"Subscribed to %.200s new groups%.200s%.200s",
				comatose((long)n),
				errors ? ", failed on " : "",
				errors ? comatose((long)errors) : "");

	    free_strlist(&folders);
	}
	else{
	    if(sub_stream = mail_cmd_stream(&subscribe_cntxt, &sclose)){
		(void) context_apply(tmp_20k_buf, &subscribe_cntxt, folder,
				     SIZEOF_20KBUF);
		if(mail_subscribe(sub_stream, tmp_20k_buf) == 0L){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      "Error subscribing to \"%.200s\"", folder);
		    errors++;
		}
		else if(ALL_FOUND(cntxt)){
		    /*---- Update the screen display data structures -----*/
		    if(cntxt->use & CNTXT_PSEUDO){
			folder_delete(0, FOLDERS(cntxt));
			cntxt->use &= ~CNTXT_PSEUDO;
		    }

		    folder_insert(-1, new_folder(folder, 0), FOLDERS(cntxt));
		}
		if(sclose)
		  pine_mail_close(sub_stream);
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  "Error subscribing to \"%.200s\"", folder);
		errors++;
	    }
	}

	if(!errors && folder[0])
	  q_status_message1(SM_ORDER, 0, 3, "Subscribed to \"%.200s\"", folder);
    }

    free_fdir(&subscribe_cntxt.dir, 1);
    return(folder[0]);
}



/*----------------------------------------------------------------------
      Rename folder
  
   Args: new_file   -- buffer to contain new file name
         index      -- index of folder in folder list to rename
         cntxt      -- collection of folders making up folder list

 Result: returns the new name of the folder, or NULL if nothing happened.

 When either the sent-mail or saved-message folders are renamed, immediately 
create a new one in their place so they always exist. The main loop above also
detects this and makes the rename look like an add of the sent-mail or
saved-messages folder. (This behavior may not be optimal, but it keeps things
consistent.

  ----*/
int
rename_folder(context, index, new_name, len, possible_stream)
    CONTEXT_S  *context;
    int	        index;
    char       *new_name;
    size_t      len;
    MAILSTREAM *possible_stream;
{
    char        *folder, prompt[64], *name_p = NULL, tmp[MAILTMPLEN];
    HelpType     help;
    FOLDER_S	*new_f;
    PINERC_S    *prc = NULL;
    int          rc, ren_cur, cnt = 0, isdir = 0, readonly = 0;
    EditWhich    ew;
    MAILSTREAM  *strm;

    dprint(4, (debugfile, "\n - rename folder -\n"));

    if(NEWS_TEST(context)){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Can't rename news groups!");
	return(0);
    }
    else if(!folder_total(FOLDERS(context))){
	q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Empty folder collection.  No folder to rename!");
	return(0);
    }
    else if((new_f = folder_entry(index, FOLDERS(context)))
	    && !strucmp(FLDR_NAME(new_f), ps_global->inbox_name)) {
        q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  "Can't change special folder name \"%.200s\"",
			  ps_global->inbox_name);
        return(0);
    }

    ew = config_containing_inc_fldr(new_f);
    if(ps_global->restricted)
      readonly = 1;
    else{
	switch(ew){
	  case Main:
	    prc = ps_global->prc;
	    break;
	  case Post:
	    prc = ps_global->post_prc;
	    break;
	  case None:
	    break;
	}

	readonly = prc ? prc->readonly : 1;
    }

    if(prc && prc->quit_to_edit && (context->use & CNTXT_INCMNG)){
	quit_to_edit_msg(prc);
	return(0);
    }

    if(readonly && (context->use & CNTXT_INCMNG)){
	q_status_message(SM_ORDER,3,5,
		     "Rename cancelled: folder not in editable config file");
	return(0);
    }

    if(context->use & CNTXT_INCMNG){
	if(!(folder = new_f->nickname))
	  folder = "";		/* blank nickname */
    }
    else
      folder = FLDR_NAME(new_f);

    ren_cur = strcmp(folder, ps_global->cur_folder) == 0;

    sprintf(prompt, "Rename %.20s to : ",
	    (context->use & CNTXT_INCMNG)
	      ? "nickname"
	      : (isdir = new_f->isdir)
		  ? "directory" : "folder");
    help   = NO_HELP;
    strncpy(new_name, folder, len-1);
    new_name[len-1] = '\0';
    while(1) {
	int flags = OE_APPEND_CURRENT;

        rc = optionally_enter(new_name, -FOOTER_ROWS(ps_global), 0,
			      len, prompt, NULL, help, &flags);
        if(rc == 3) {
            help = help == NO_HELP ? h_oe_foldrename : NO_HELP;
            continue;
        }

	removing_leading_and_trailing_white_space(new_name);

        if(rc == 0 && (*new_name || (context->use & CNTXT_INCMNG))) {
	    /* verify characters */
	    if(F_OFF(F_ENABLE_DOT_FOLDERS,ps_global) && *new_name == '.'){
		if(cnt++ <= 0)
                  q_status_message(SM_ORDER,3,3,
		    "Folder name can't begin with dot");
		else
		  q_status_message1(SM_ORDER,3,3,
		      "Config feature \"%.200s\" enables names beginning with dot",
		      feature_list_name(F_ENABLE_DOT_FOLDERS));

                display_message(NO_OP_COMMAND);
                continue;
	    }

	    if(folder_index(new_name, context, FI_ANY|FI_RENAME) >= 0){
                q_status_message1(SM_ORDER, 3, 3,
				  "Folder \"%.200s\" already exists",
                                  pretty_fn(new_name));
                display_message(NO_OP_COMMAND);
                continue;
            }
	    else if(!strucmp(new_name, ps_global->inbox_name)){
                q_status_message1(SM_ORDER, 3, 3, "Can't rename folder to %.200s",
				  ps_global->inbox_name);
                display_message(NO_OP_COMMAND);
                continue;
	    }
        }

        if(rc != 4) /* redraw */
	  break;  /* no redraw */

    }

    if(rc != 1 && isdir){		/* add trailing delim? */
	for(name_p = new_name; *name_p && *(name_p+1) ; name_p++)
	  ;

	if(*name_p == context->dir->delim) /* lop off delim */
	  *name_p = '\0';
    }

    if(rc == 1
       || !(*new_name || (context->use & CNTXT_INCMNG))
       || !strcmp(new_name, folder)){
        q_status_message(SM_ORDER, 0, 2, "Folder rename cancelled");
        return(0);
    }

    if(context->use & CNTXT_INCMNG){
	char **new_list, **lp, ***alval;
	int   i;

	alval = ALVAL(&ps_global->vars[V_INCOMING_FOLDERS], ew);
	for(i = 0; (*alval)[i]; i++)
	  ;

	new_list = (char **) fs_get((i + 1) * sizeof(char *));

	for(lp = new_list, i = 0; (*alval)[i]; i++){
	  /* figure out if this is the one we're renaming */
	  expand_variables(tmp_20k_buf, SIZEOF_20KBUF, (*alval)[i], 0);
	  if(new_f->varhash == line_hash(tmp_20k_buf)){
	      char *folder_string = NULL, *nickname = NULL;

	      if(new_f->nickname)
		fs_give((void **) &new_f->nickname);

	      if(*new_name)
		new_f->nickname = cpystr(new_name);

	      /*
	       * Parse folder line for nickname and folder name.
	       * No nickname on line is OK.
	       */
	      get_pair(tmp_20k_buf, &nickname, &folder_string, 0, 0);

	      if(nickname)
		fs_give((void **)&nickname);

	      *lp = put_pair(new_name, folder_string);

	      new_f->varhash = line_hash(*lp++);
	  }
	  else
	    *lp++ = cpystr((*alval)[i]);
	}

	*lp = NULL;

	set_variable_list(V_INCOMING_FOLDERS, new_list, TRUE, ew);

	return(1);
    }

    /* Can't rename open streams */
    if((strm = context_already_open_stream(context, folder, AOS_NONE))
       || (ren_cur && (strm=ps_global->mail_stream))){
	if(possible_stream == strm)
	  possible_stream = NULL;

	pine_mail_actually_close(strm);
    }

    if(possible_stream
       && !context_same_stream(context, new_name, possible_stream))
      possible_stream = NULL;
      
    if(rc = context_rename(context, possible_stream, folder, new_name)){
	if(name_p && *name_p == context->dir->delim)
	  *name_p = '\0';		/* blat trailing delim */

	/* insert new name? */
	if(!strindex(new_name, context->dir->delim)){
	    new_f	     = new_folder(new_name, 0);
	    new_f->isdir     = isdir;
	    folder_insert(-1, new_f, FOLDERS(context));
	}

	if(strcmp(ps_global->VAR_DEFAULT_FCC, folder) == 0
	   || strcmp(ps_global->VAR_DEFAULT_SAVE_FOLDER, folder) == 0) {
	    /* renaming sent-mail or saved-messages */
	    if(context_create(context, NULL, folder)){
		q_status_message3(SM_ORDER,0,3,
	      "Folder \"%.200s\" renamed to \"%.200s\". New \"%.200s\" created",
				  folder, new_name,
				  pretty_fn(
				    (strcmp(ps_global->VAR_DEFAULT_SAVE_FOLDER,
					    folder) == 0)
				    ? ps_global->VAR_DEFAULT_SAVE_FOLDER
				    : ps_global->VAR_DEFAULT_FCC));

	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  "Error creating new \"%.200s\"", folder);

		dprint(1, (debugfile, "Error creating \"%s\" in %s context\n",
			   folder ? folder : "?",
			   context->context ? context->context : "?"));
	    }
	}
	else
	  q_status_message2(SM_ORDER, 0, 3,
			    "Folder \"%.200s\" renamed to \"%.200s\"",
			    pretty_fn(folder), pretty_fn(new_name));

	free_folder_list(context);
    }

    if(ren_cur) {
        /* No reopen the folder we just had open */
        do_broach_folder(new_name, context, NULL, 0L);
    }

    return(rc);
}



/*----------------------------------------------------------------------
   Confirm and delete a folder

   Args: fs -- folder screen state

 Result: return 0 if not delete, 1 if deleted.

 NOTE: Currently disallows deleting open folder...
  ----*/
int
delete_folder(context, index, next_folder, len, possible_streamp)
    CONTEXT_S   *context;
    int	         index;
    char        *next_folder;
    size_t       len;
    MAILSTREAM **possible_streamp;
{
    char       *folder, ques_buf[MAX_SCREEN_COLS+1], *target = NULL,
	       *fnamep, buf[1000];
    MAILSTREAM *del_stream = NULL, *sub_stream, *strm = NULL;
    FOLDER_S   *fp;
    EditWhich   ew;
    PINERC_S   *prc = NULL;
    int         ret, unsub_opened = 0, close_opened = 0, blast_folder = 1,
		readonly;

    if(!context){
	cmd_cancelled("Missing context in Delete");
	return(0);
    }

    if(NEWS_TEST(context)){
	static char fmt[] = "Really unsubscribe from \"%.*s\"";
         
        folder = folder_entry(index, FOLDERS(context))->name;
	/* 4 is strlen("%.*s") */
        sprintf(ques_buf, fmt, sizeof(ques_buf) - (sizeof(fmt)-4), folder);
    
        ret = want_to(ques_buf, 'n', 'x', NO_HELP, WT_NORM);
        switch(ret) {
          /* ^C */
          case 'x':
            Writechar(BELL, 0);
            /* fall through */
          case 'n':
            return(0);
        }
    
        dprint(2, (debugfile, "deleting folder \"%s\" in context \"%s\"\n",
	       folder ? folder : "?",
	       context->context ? context->context : "?"));

	if(sub_stream = mail_cmd_stream(context, &unsub_opened)){
	    (void) context_apply(tmp_20k_buf, context, folder, SIZEOF_20KBUF);
	    if(!mail_unsubscribe(sub_stream, tmp_20k_buf)){
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  "Error unsubscribing from \"%.200s\"",folder);
		if(unsub_opened)
		  pine_mail_close(sub_stream);
		return(0);
	    }
	    if(unsub_opened)
	      pine_mail_close(sub_stream);
	}

	/*
	 * Fix  up the displayed list
	 */
	folder_delete(index, FOLDERS(context));
	return(1);
    }

    fp = folder_entry(index, FOLDERS(context));
    if(!fp){
	cmd_cancelled("Can't find folder to Delete!");
	return(0);
    }

    if(!((context->use & CNTXT_INCMNG) && fp->name
         && check_for_move_mbox(fp->name, NULL, 0, &target))){
	target = NULL;
    }

    folder = FLDR_NAME(fp);
    dprint(4, (debugfile, "=== delete_folder(%s) ===\n",
	   folder ? folder : "?"));

    ew = config_containing_inc_fldr(fp);
    if(ps_global->restricted)
      readonly = 1;
    else{
	switch(ew){
	  case Main:
	    prc = ps_global->prc;
	    break;
	  case Post:
	    prc = ps_global->post_prc;
	    break;
	  case None:
	    break;
	}

	readonly = prc ? prc->readonly : 1;
    }

    if(prc && prc->quit_to_edit && (context->use & CNTXT_INCMNG)){
	quit_to_edit_msg(prc);
	return(0);
    }

    if(strucmp(folder, ps_global->inbox_name) == 0){
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
		 "Can't delete special folder \"%.200s\".",
		 ps_global->inbox_name);
	return(0);
    }
    else if(readonly && (context->use & CNTXT_INCMNG)){
	q_status_message(SM_ORDER,3,5,
		     "Deletion cancelled: folder not in editable config file");
	return(0);
    }
    else if((fp->name
	     && (strm=context_already_open_stream(context,fp->name,AOS_NONE)))
	    ||
	    (target
	     && (strm=context_already_open_stream(NULL,target,AOS_NONE)))){
	if(strm == ps_global->mail_stream)
	  close_opened++;
    }
    else if(fp->isdir || fp->isdual){	/* NO DELETE if directory isn't EMPTY */
	FDIR_S *fdirp = next_folder_dir(context,folder,TRUE,possible_streamp);

	if(fp->haschildren)
	  ret = 1;
	else if(fp->hasnochildren)
	  ret = 0;
	else{
	    ret = folder_total(fdirp->folders) > 0;
	    free_fdir(&fdirp, 1);
	}

	if(ret){
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Can't delete non-empty directory \"%.200s\"%.200s.",
			  folder, (fp->isfolder && fp->isdual) ? " (or folder of same name)" : "");
	    return(0);
	}

	/*
	 * Folder by the same name exist?  If so, server's probably going
	 * to delete it as well.  Punt?
	 */
	if(fp->isdual
	   && (ret = want_to(DIR_FOLD_PMT,'n','x',NO_HELP,WT_NORM)) != 'y'){
	    q_status_message(SM_ORDER,0,3, (ret == 'x') ? "Delete cancelled" 
			     : "No folder deleted");
	    return(0);
	}
    }

    if(context->use & CNTXT_INCMNG){
	static ESCKEY_S delf_opts[] = {
	    {'n', 'n', "N", "Nickname only"},
	    {'b', 'b', "B", "Both Folder and Nickname"},
	    {-1, 0, NULL, NULL}
	};
#define	DELF_PROMPT	"DELETE only Nickname or Both nickname and folder? "

	switch(radio_buttons(DELF_PROMPT, -FOOTER_ROWS(ps_global),
			     delf_opts,'n','x',NO_HELP,RB_NORM, NULL)){
	  case 'n' :
	    blast_folder = 0;
	    break;

	  case 'x' :
	    cmd_cancelled("Delete");
	    return(0);

	  default :
	    break;
	}
    }
    else{
	sprintf(ques_buf, "DELETE \"%.100s\"%s", folder, 
		close_opened ? " (the currently open folder)" :
	  (fp->isdir && !(fp->isdual || fp->isfolder
			  || (folder_index(folder, context, FI_FOLDER) >= 0)))
			      ? " (a directory)" : "");

	if((ret = want_to(ques_buf, 'n', 'x', NO_HELP, WT_NORM)) != 'y'){
	    q_status_message(SM_ORDER,0,3, (ret == 'x') ? "Delete cancelled" 
			     : "Nothing deleted");
	    return(0);
	}
    }

    if(blast_folder){
	dprint(2, (debugfile,"deleting \"%s\" (%s) in context \"%s\"\n",
		   fp->name ? fp->name : "?",
		   fp->nickname ? fp->nickname : "",
		   context->context ? context->context : "?"));
	if(strm){
	    /*
	     * Close it, NULL the pointer, and let do_broach_folder fixup
	     * the rest...
	     */
	    pine_mail_actually_close(strm);
	    if(close_opened){
		ps_global->mangled_header = 1;
		do_broach_folder(ps_global->inbox_name,
				 ps_global->context_list, NULL, 0L);
	    }
	}

	/*
	 * Use fp->name since "folder" may be a nickname...
	 */
	if(possible_streamp && *possible_streamp
	   && context_same_stream(context, fp->name, *possible_streamp))
	  del_stream = *possible_streamp;

	fnamep = fp->name;

	if(!context_delete(context, del_stream, fnamep)){
/*
 * BUG: what if sent-mail or saved-messages????
 */
	    q_status_message1(SM_ORDER,3,3,"Delete of \"%.200s\" Failed!", folder);
	    return(0);
	}
    }

    sprintf(buf, "%.50s\"%.200s\" deleted.",
	    !blast_folder               ? "Nickname "          :
	     fp->isdual                  ? "Folder/Directory " :
	      (fp->isdir && fp->isfolder) ? "Folder "          :
	       fp->isdir                   ? "Directory "      :
					      "Folder ",
	    folder);

    q_status_message(SM_ORDER, 0, 3, buf);

    if(context->use & CNTXT_INCMNG){
	char **new_list, **lp, ***alval;
	int   i;

	alval = ALVAL(&ps_global->vars[V_INCOMING_FOLDERS], ew);
	for(i = 0; (*alval)[i]; i++)
	  ;

	/*
	 * Make it one too big in case we don't find the match for
	 * some unknown reason.
	 */
	new_list = (char **) fs_get((i + 1) * sizeof(char *));

	/*
	 * Copy while figuring out which one to skip.
	 */
	for(lp = new_list, i = 0; (*alval)[i]; i++){
	    expand_variables(tmp_20k_buf, SIZEOF_20KBUF, (*alval)[i], 0);
	    if(fp->varhash != line_hash(tmp_20k_buf))
	      *lp++ = cpystr((*alval)[i]);
	}

	*lp = NULL;

	set_variable_list(V_INCOMING_FOLDERS, new_list, TRUE, ew);
	free_list_array(&new_list);
    }

    /*
     * Fix up the displayed list.
     */
    folder_delete(index, FOLDERS(context));

    /*
     * Take a guess at what should get hilited next.
     */
    if(index < (ret = folder_total(FOLDERS(context)))
       || ((index = ret - 1) >= 0)){
	if((fp = folder_entry(index, FOLDERS(context)))
	   && strlen(FLDR_NAME(fp)) < len - 1)
	  strncpy(next_folder, FLDR_NAME(fp), len-1);
	  next_folder[len-1] = '\0';
    }

    if(!(context->use & CNTXT_INCMNG)){
	/*
	 * Then cause the list to get rebuild 'cause we may've blasted
	 * some folder that's also a directory or vice versa...
	 */
	free_folder_list(context);
    }

    return(1);
}


/*
 * Return which pinerc incoming folder #index is in.
 */
EditWhich
config_containing_inc_fldr(folder)
    FOLDER_S *folder;
{
    char    **t;
    int       i, keep_going = 1, inheriting = 0;
    struct variable *v = &ps_global->vars[V_INCOMING_FOLDERS];

    if(v->is_fixed)
      return(None);

    /* is it in exceptions config? */
    if(v->post_user_val.l){
	for(i = 0, t=v->post_user_val.l; t[i]; i++){
	    if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF, t[i], 0)){
		keep_going = 0;

		if(!strcmp(tmp_20k_buf, INHERIT))
		  inheriting = 1;
		else if(folder->varhash == line_hash(tmp_20k_buf))
		  return(Post);
	    }
	}
    }

    if(inheriting)
      keep_going = 1;

    /* is it in main config? */
    if(keep_going && v->main_user_val.l){
	for(i = 0, t=v->main_user_val.l; t[i]; i++){
	    if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF, t[i], 0) &&
	       folder->varhash == line_hash(tmp_20k_buf))
	      return(Main);
	}
    }

    return(None);
}



/*----------------------------------------------------------------------
      Print the list of folders on paper

   Args: list    --  The current list of folders
         lens    --  The list of lengths of the current folders
         display --  The current folder display structure

 Result: list printed on paper

If the display list was created for 80 columns it is used, otherwise
a new list is created for 80 columns

  ----*/

void
print_folders(fp)
    FPROC_S  *fp;
{
    if(open_printer("folder list ") == 0){
	(void) folder_list_text(ps_global, fp, print_char, NULL, 80);

	close_printer();
    }
}
                     

int
scan_get_pattern(kind, pat, len)
    char *kind, *pat;
    int   len;
{
    char prompt[256];
    int  flags;

    pat[0] = '\0';
    sprintf(prompt, "String in folder %.100s to match : ", kind);

    while(1){
	flags = OE_APPEND_CURRENT | OE_DISALLOW_HELP;
	switch(optionally_enter(pat, -FOOTER_ROWS(ps_global), 0, len,
				prompt, NULL, NO_HELP, &flags)){

	  case 4 :
	    if(ps_global->redrawer)
	      (*ps_global->redrawer)();

	    break;

	  case 0 :
	    if(*pat)
	      return(1);

	  case 1 :
	    cmd_cancelled("Select");

	  default :
	    return(0);
	}
    }
}


int
folder_select_text(ps, context, selected)
    struct pine *ps;
    CONTEXT_S	*context;
    int		 selected;
{
    char	     pattern[MAILTMPLEN], type = '\0';
    static ESCKEY_S  scan_opts[] = {
	{'n', 'n', "N", "Name Select"},
	{'c', 'c', "C", "Content Select"},
	{-1, 0, NULL, NULL}
    };

    if(context->use & CNTXT_INCMNG){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
		     "Select \"Text\" not supported in Incoming Folders");
	return(0);
    }

    switch(radio_buttons(SEL_TEXT_PMT, -FOOTER_ROWS(ps_global),
			 scan_opts, 'n', 'x', NO_HELP, RB_NORM, NULL)){
      case 'n' :				/* Name search */
	if(scan_get_pattern("NAME", pattern, sizeof(pattern)))
	  type = 'n';

	break;

      case 'c' :				/* content search */
	if(scan_get_pattern("CONTENTS", pattern, sizeof(pattern)))
	  type = 'c';

	break;

      case 'x' :
      default :
	break;
    }

    if(type){
	int	  rv;
	char	  tmp[MAILTMPLEN];
	SCANARG_S args;

	memset(&args, 0, sizeof(SCANARG_S));
	args.pattern  = pattern;
	args.type     = type;
	args.context  = context;

	if(type == 'c'){
	    args.stream = sp_stream_get(context_apply(tmp, context,
						      "xxx", sizeof(tmp)),
					SP_SAME);
	    if(!args.stream){
		args.stream = pine_mail_open(NULL, tmp,
				 OP_SILENT|OP_HALFOPEN|SP_USEPOOL|SP_TEMPUSE,
					     NULL);
		args.newstream = (args.stream != NULL);
	    }
	}

	rv = foreach_folder(context, selected,
			    foreach_do_scan, (void *) &args);

	if(args.newstream)
	  pine_mail_close(args.stream);

	if(rv)
	  return(1);
    }

    cmd_cancelled("Select");
    return(0);
}


int
foreach_do_scan(f, d)
    FOLDER_S *f;
    void     *d;
{
    SCANARG_S *sa = (SCANARG_S *) d;

    return((sa->type == 'n' && srchstr(FLDR_NAME(f), sa->pattern))
	   || (sa->type == 'c'
	       && scan_scan_folder(sa->stream, sa->context, f, sa->pattern)));
}


int
scan_scan_folder(stream, context, f, pattern)
    MAILSTREAM *stream;
    CONTEXT_S  *context;
    FOLDER_S   *f;
    char       *pattern;
{
    MM_LIST_S  ldata;
    LISTRES_S  response;
    int        we_cancel = 0;
    char      *folder, *ref = NULL, tmp[MAILTMPLEN];

    folder = f->name;
    sprintf(tmp, "Scanning \"%.40s\"", FLDR_NAME(f));
    we_cancel = busy_alarm(1, tmp, NULL, 0);

    mm_list_info	      = &ldata;		/* tie down global reference */
    memset(&ldata, 0, sizeof(MM_LIST_S));
    ldata.filter = mail_list_response;
    memset(ldata.data = &response, 0, sizeof(LISTRES_S));

    /*
     * If no preset reference string, must be at top of context
     */
    if(context && context_isambig(folder) && !(ref = context->dir->ref)){
	char *p;

	if(p = strstr(context->context, "%s")){
	    if(!*(p+2)){
		sprintf(tmp, "%.*s", min(p - context->context, sizeof(tmp)-1),
			context->context);
		ref = tmp;
	    }
	    else{
		sprintf(tmp, context->context, folder);
		folder = tmp;
		ref    = "";
	    }
	}
	else
	  ref = context->context;
    }

    mail_scan(stream, ref, folder, pattern);

    if(context && context->dir && response.delim)
      context->dir->delim = response.delim;

    if(we_cancel)
      cancel_busy_alarm(-1);

    return(((response.isfile) ? FEX_ISFILE : 0)
	   | ((response.isdir) ? FEX_ISDIR : 0));
}


/*----------------------------------------------------------------------
  

    ----*/
void
mail_list_response(stream, mailbox, delim, attribs, data, options)
    MAILSTREAM *stream;
    char       *mailbox;
    int		delim;
    long	attribs;
    void       *data;
    unsigned    options;	/* unused */
{
    if(delim)
      ((LISTRES_S *) data)->delim = delim;

    if(mailbox && *mailbox){
	if(!(attribs & LATT_NOSELECT)){
	    ((LISTRES_S *) data)->isfile  = 1;
	    ((LISTRES_S *) data)->count	 += 1;
	}

	if(!(attribs & LATT_NOINFERIORS)){
	    ((LISTRES_S *) data)->isdir  = 1;
	    ((LISTRES_S *) data)->count += 1;
	}

	if(attribs & LATT_HASCHILDREN)
	  ((LISTRES_S *) data)->haschildren = 1;

	if(attribs & LATT_HASNOCHILDREN)
	  ((LISTRES_S *) data)->hasnochildren = 1;
    }
}



int
folder_select_props(ps, context, selected)
    struct pine *ps;
    CONTEXT_S	*context;
    int		 selected;
{
    int		       cmp = 0;
    long	       flags = 0L, count;
    static ESCKEY_S    prop_opts[] = {
	{'u', 'u', "U", "Unseen msgs"},
	{'n', 'n', "N", "New msgs"},
	{'c', 'c', "C", "msg Count"},
	{-1, 0, NULL, NULL}
    };

    if(context->use & CNTXT_INCMNG){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
		     "Select \"Properties\" not supported in Incoming Folders");
	return(0);
    }

    switch(radio_buttons(SEL_PROP_PMT, -FOOTER_ROWS(ps_global),
			 prop_opts, 'n', 'x', h_folder_prop, RB_NORM, NULL)){
      case 'c' :				/* message count */
	if(folder_select_count(&count, &cmp))
	  flags = SA_MESSAGES;

	break;

      case 'n' :				/* folders with new */
	flags = SA_RECENT;
	break;

      case 'u' :				/* folders with unseen */
	flags = SA_UNSEEN;
	break;

      case 'x' :
      default :
	break;
    }

    if(flags){
	int	  rv;
	char	  tmp[MAILTMPLEN];
	STATARG_S args;

	memset(&args, 0, sizeof(STATARG_S));
	args.flags    = flags;
	args.context  = context;
	args.nmsgs    = count;
	args.cmp      = cmp;
 
	args.stream = sp_stream_get(context_apply(tmp, context,
						  "xxx", sizeof(tmp)),
				    SP_SAME);
	if(!args.stream){
	    args.stream = pine_mail_open(NULL, tmp,
				 OP_SILENT|OP_HALFOPEN|SP_USEPOOL|SP_TEMPUSE,
					 NULL);
	    args.newstream = (args.stream != NULL);
	}

	rv = foreach_folder(context, selected,
			    foreach_do_stat, (void *) &args);

	if(args.newstream)
	  pine_mail_close(args.stream);

	if(rv)
	  return(1);
    }

    cmd_cancelled("Select");
    return(0);
}


int
folder_select_count(count, cmp)
    long *count;
    int	 *cmp;
{
    int		r, flags;
    char	number[32], prompt[128];
    static char *tense[] = {"EQUAL TO", "LESS THAN", "GREATER THAN"};
    static ESCKEY_S sel_num_opt[] = {
	{ctrl('W'), 14, "^W", "Toggle Comparison"},
	{-1, 0, NULL, NULL}
    };

    *count = 0L;
    while(1){
	flags = OE_APPEND_CURRENT | OE_DISALLOW_HELP;
	sprintf(number, "%ld", *count);
	sprintf(prompt, "Select folders with messages %.20s : ", tense[*cmp]);
	r = optionally_enter(number, -FOOTER_ROWS(ps_global), 0, sizeof(number),
			     prompt, sel_num_opt, NO_HELP, &flags);
	switch (r){
	  case 0 :
	    if(!*number)
	      break;
	    else if((*count = atol(number)) < 0L)
	      q_status_message(SM_ORDER, 3, 3,
			       "Can't have NEGATIVE message count!");
	    else
	      return(1);	/* success */

	  case 3 :		/* help */
	  case 4 :		/* redraw */
	    continue;

	  case 14 :		/* toggle comparison */
	    *cmp = ++(*cmp) % 3;
	    continue;

	  case -1 :		/* cancel */
	  default:
	    break;
	}

	break;
    }

    return(0);			/* return failure */
}


int
foreach_do_stat(f, d)
    FOLDER_S *f;
    void     *d;
{
    STATARG_S *sa = (STATARG_S *) d;
    int	       rv = 0;

    if(ps_global->context_current == sa->context
       && !strcmp(ps_global->cur_folder, FLDR_NAME(f))){
	switch(sa->flags){
	  case SA_MESSAGES :
	    switch(sa->cmp){
	      case 0 :				/* equals */
		if(ps_global->mail_stream->nmsgs == sa->nmsgs)
		  rv = 1;

		break;

	      case 1 :				/* less than */
		if(ps_global->mail_stream->nmsgs < sa->nmsgs)
		  rv = 1;

		break;

	      case 2 :
		if(ps_global->mail_stream->nmsgs > sa->nmsgs)
		  rv = 1;

		break;

	      default :
		break;
	    }

	    break;

	  case SA_RECENT :
	    if(count_flagged(ps_global->mail_stream, F_RECENT))
	      rv = 1;

	    break;

	  case SA_UNSEEN :
	    if(count_flagged(ps_global->mail_stream, F_UNSEEN))
	      rv = 1;

	    break;

	  default :
	    break;
	}
    }
    else{
	int	    we_cancel = 0, mlen;
	char	    msg_buf[MAX_BM+1], mbuf[MAX_BM+1];
	extern MAILSTATUS mm_status_result;

	/* 29 is max length of constant strings and 7 is for busy_alarm */
	mlen = min(MAX_BM, ps_global->ttyo->screen_cols) - (29+7);
	sprintf(msg_buf, "Checking %.*s for %s",
		sizeof(msg_buf)-29,
		(mlen > 5) ? short_str(FLDR_NAME(f), mbuf, mlen, FrontDots)
			   : FLDR_NAME(f),
		(sa->flags == SA_UNSEEN)
		    ? "unseen messages"
		    : (sa->flags == SA_MESSAGES) ? "message count"
						 : "recent messages");
	we_cancel = busy_alarm(1, msg_buf, NULL, 1);

	if(!context_status(sa->context, sa->stream, f->name, sa->flags))
	  mm_status_result.flags = 0L;

	if(we_cancel)
	  cancel_busy_alarm(0);

	if(sa->flags & mm_status_result.flags)
	  switch(sa->flags){
	    case SA_MESSAGES :
	      switch(sa->cmp){
		case 0 :			/* equals */
		  if(mm_status_result.messages == sa->nmsgs)
		    rv = 1;

		  break;

		case 1 :				/* less than */
		  if(mm_status_result.messages < sa->nmsgs)
		    rv = 1;

		  break;

		case 2 :
		  if(mm_status_result.messages > sa->nmsgs)
		    rv = 1;

		  break;

		default :
		  break;
	      }

	      break;

	    case SA_RECENT :
	      if(mm_status_result.recent)
		rv = 1;

	      break;

	    case SA_UNSEEN :
	      if(mm_status_result.unseen)
		rv = 1;

	      break;

	    default :
	      break;
	  }
    }

    return(rv);
}


int
foreach_folder(context, selected, test, args)
    CONTEXT_S  *context;
    int		selected;
    int	      (*test) PROTO((FOLDER_S *, void *));
    void       *args;
{
    int		     i, n, rv = 1;
    FOLDER_S	    *fp;

#ifndef	DOS
    intr_handling_on();
#endif

    for(i = 0, n = folder_total(FOLDERS(context)); i < n; i++){
	if(ps_global->intr_pending){
	    for(; i >= 0; i--)
	      folder_entry(i, FOLDERS(context))->scanned = 0;

	    cmd_cancelled("Select");
	    rv = 0;
	    break;
	}

	fp = folder_entry(i, FOLDERS(context));
	fp->scanned = 0;
	if((!selected || fp->selected) && fp->isfolder && (*test)(fp, args))
	  fp->scanned = 1;
    }

#ifndef	DOS
    intr_handling_off();
#endif

    return(rv);
}



/*----------------------------------------------------------------------
      compare two names for qsort, case independent

   Args: pointers to strings to compare

 Result: integer result of strcmp of the names.  Uses simple 
         efficiency hack to speed the string comparisons up a bit.

  ----------------------------------------------------------------------*/
int
compare_names(x, y)
    const QSType *x, *y;
{
    char *a = *(char **)x, *b = *(char **)y;
    int r;
#define	CMPI(X,Y)	((X)[0] - (Y)[0])
#define	UCMPI(X,Y)	((isupper((unsigned char)((X)[0]))	\
				? (X)[0] - 'A' + 'a' : (X)[0])	\
			  - (isupper((unsigned char)((Y)[0]))	\
				? (Y)[0] - 'A' + 'a' : (Y)[0]))

    /*---- Inbox always sorts to the top ----*/
    if(UCMPI(a, ps_global->inbox_name) == 0
       && strucmp(a, ps_global->inbox_name) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : -1);
    else if((UCMPI(b, ps_global->inbox_name)) == 0
       && strucmp(b, ps_global->inbox_name) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : 1);

    /*----- The sent-mail folder, is always next unless... ---*/
    else if(F_OFF(F_SORT_DEFAULT_FCC_ALPHA, ps_global)
	    && CMPI(a, ps_global->VAR_DEFAULT_FCC) == 0
	    && strcmp(a, ps_global->VAR_DEFAULT_FCC) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : -1);
    else if(F_OFF(F_SORT_DEFAULT_FCC_ALPHA, ps_global)
	    && CMPI(b, ps_global->VAR_DEFAULT_FCC) == 0
	    && strcmp(b, ps_global->VAR_DEFAULT_FCC) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : 1);

    /*----- The saved-messages folder, is always next unless... ---*/
    else if(F_OFF(F_SORT_DEFAULT_SAVE_ALPHA, ps_global)
	    && CMPI(a, ps_global->VAR_DEFAULT_SAVE_FOLDER) == 0
	    && strcmp(a, ps_global->VAR_DEFAULT_SAVE_FOLDER) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : -1);
    else if(F_OFF(F_SORT_DEFAULT_SAVE_ALPHA, ps_global)
	    && CMPI(b, ps_global->VAR_DEFAULT_SAVE_FOLDER) == 0
	    && strcmp(b, ps_global->VAR_DEFAULT_SAVE_FOLDER) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : 1);

    else
      return((r = CMPI(a, b)) ? r : strcmp(a, b));
}



/*----------------------------------------------------------------------
      compare two folder structs for ordering alphabetically

   Args: pointers to folder structs to compare

 Result: integer result of dir-bit and strcmp of the folders.
  ----------------------------------------------------------------------*/
int
compare_folders_alpha(f1, f2)
    FOLDER_S *f1, *f2;
{
    int	  i;
    char *f1name = FLDR_NAME(f1),
	 *f2name = FLDR_NAME(f2);

    return(((i = compare_names(&f1name, &f2name)) != 0)
	       ? i : (f2->isdir - f1->isdir));
}


/*----------------------------------------------------------------------
      compare two folder structs alphabetically with dirs first

   Args: pointers to folder structs to compare

 Result: integer result of dir-bit and strcmp of the folders.
  ----------------------------------------------------------------------*/
int
compare_folders_dir_alpha(f1, f2)
    FOLDER_S *f1, *f2;
{
    int	  i;

    if((i = (f2->isdir - f1->isdir)) == 0){
	char *f1name = FLDR_NAME(f1),
	     *f2name = FLDR_NAME(f2);

	return(compare_names(&f1name, &f2name));
    }

    return(i);
}


/*----------------------------------------------------------------------
      compare two folder structs alphabetically with dirs last

   Args: pointers to folder structs to compare

 Result: integer result of dir-bit and strcmp of the folders.
  ----------------------------------------------------------------------*/
int
compare_folders_alpha_dir(f1, f2)
    FOLDER_S *f1, *f2;
{
    int	  i;

    if((i = (f1->isdir - f2->isdir)) == 0){
	char *f1name = FLDR_NAME(f1),
	     *f2name = FLDR_NAME(f2);

	return(compare_names(&f1name, &f2name));
    }

    return(i);
}




/*----------------------------------------------------------------------
      Format the given folder name for display for the user

   Args: folder -- The folder name to fix up

Not sure this always makes it prettier. It could do nice truncation if we
passed in a length. Right now it adds the path name of the mail 
subdirectory if appropriate.
 ----*/
      
char *
pretty_fn(folder)
     char *folder;
{
    if(!strucmp(folder, ps_global->inbox_name))
      return(ps_global->inbox_name);
    else
      return(folder);
}


/*----------------------------------------------------------------------
       Check to see if folder exists in given context

  Args: cntxt -- context inwhich to interpret "file" arg
        file  -- name of folder to check
 
 Result: returns FEX_ISFILE if the folder exists and is a folder
		 FEX_ISDIR  if the folder exists and is a directory
                 FEX_NOENT  if it doesn't exist
		 FEX_ERROR  on error

  The two existance return values above may be logically OR'd

  Uses mail_list to sniff out the existance of the requested folder.
  The context string is just here for convenience.  Checking for
  folder's existance within a given context is probably more efficiently
  handled outside this function for now using build_folder_list().

    ----*/
int
folder_exists(cntxt, file)
    CONTEXT_S *cntxt;
    char      *file;
{
    return(folder_name_exists(cntxt, file, NULL));
}


/*----------------------------------------------------------------------
       Check to see if folder exists in given context

  Args: cntxt -- context inwhich to interpret "file" arg
        file  -- name of folder to check
	name  -- name of folder folder with context applied
 
 Result: returns FEX_ISFILE if the folder exists and is a folder
		 FEX_ISDIR  if the folder exists and is a directory
                 FEX_NOENT  if it doesn't exist
		 FEX_ERROR  on error

  The two existance return values above may be logically OR'd

  Uses mail_list to sniff out the existance of the requested folder.
  The context string is just here for convenience.  Checking for
  folder's existance within a given context is probably more efficiently
  handled outside this function for now using build_folder_list().

    ----*/
int
folder_name_exists(cntxt, file, fullpath)
    CONTEXT_S *cntxt;
    char      *file, **fullpath;
{
    MM_LIST_S    ldata;
    EXISTDATA_S	 parms;
    int          we_cancel = 0, res;
    char	*p, reference[MAILTMPLEN], tmp[MAILTMPLEN], *tfolder = NULL;

    /*
     * No folder means "inbox", and it has to exist, also
     * look explicitly for inbox...
     */
    if(ps_global->VAR_INBOX_PATH &&
       (!*file
        || !strucmp(file, ps_global->inbox_name))){
	file  = ps_global->VAR_INBOX_PATH;
	cntxt = NULL;
    }
    else if(*file == '{' && (p = strchr(file, '}')) && (!*(p+1))){
	tfolder = (char *)fs_get((strlen(file)+strlen("inbox")+1) 
				 * sizeof(char));
	sprintf(tfolder, "%s%s", file, "inbox");
	file = tfolder;
	cntxt = NULL;
    }

    mm_list_info = &ldata;		/* tie down global reference */
    memset(&ldata, 0, sizeof(ldata));
    ldata.filter = mail_list_exists;

    ldata.stream = sp_stream_get(tmp, SP_SAME);

    memset(ldata.data = &parms, 0, sizeof(EXISTDATA_S));

    /*
     * If no preset reference string, must be at top of context
     */
    if(cntxt && context_isambig(file)){
	if(!(parms.args.reference = cntxt->dir->ref)){
	    char *p;

	    if(p = strstr(cntxt->context, "%s")){
		strncpy(parms.args.reference = reference,
			cntxt->context,
			min(p - cntxt->context, sizeof(reference)-1));
		reference[min(p - cntxt->context, sizeof(reference)-1)] = '\0';
		if(*(p += 2))
		  parms.args.tail = p;
	    }
	    else
	      parms.args.reference = cntxt->context;
	}

	parms.fullname = fullpath;
    }

    ps_global->mm_log_error = 0;
    ps_global->noshow_error = 1;

    we_cancel = busy_alarm(1, NULL, NULL, 0);

    parms.args.name = file;

    res = pine_mail_list(ldata.stream, parms.args.reference, parms.args.name,
			 &ldata.options);

    if(we_cancel)
      cancel_busy_alarm(-1);

    ps_global->noshow_error = 0;

    if(cntxt && cntxt->dir && parms.response.delim)
      cntxt->dir->delim = parms.response.delim;

    if(tfolder)
      fs_give((void **)&tfolder);
    return(((res == FALSE) || ps_global->mm_log_error)
	    ? FEX_ERROR
	    : (((parms.response.isfile)
		  ? FEX_ISFILE : 0 )
	       | ((parms.response.isdir)
		  ? FEX_ISDIR : 0)
	       | ((parms.response.ismarked)
		  ? FEX_ISMARKED : 0)
	       | ((parms.response.unmarked)
		  ? FEX_UNMARKED : 0)));
}



/*----------------------------------------------------------------------
  

    ----*/
void
mail_list_exists(stream, mailbox, delim, attribs, data, options)
    MAILSTREAM *stream;
    char       *mailbox;
    int		delim;
    long	attribs;
    void       *data;
    unsigned    options;
{
    if(delim)
      ((EXISTDATA_S *) data)->response.delim = delim;

    if(mailbox && *mailbox){
	if(!(attribs & LATT_NOSELECT)){
	    ((EXISTDATA_S *) data)->response.isfile  = 1;
	    ((EXISTDATA_S *) data)->response.count  += 1;
	}

	if(!(attribs & LATT_NOINFERIORS)){
	    ((EXISTDATA_S *) data)->response.isdir  = 1;
	    ((EXISTDATA_S *) data)->response.count  += 1;
	}
    
	if(attribs & LATT_MARKED)
	  ((EXISTDATA_S *) data)->response.ismarked = 1;

	/* don't mark #move folders unmarked */
	if(attribs & LATT_UNMARKED && !(options & PML_IS_MOVE_MBOX))
	  ((EXISTDATA_S *) data)->response.unmarked = 1;

	if(attribs & LATT_HASCHILDREN)
	  ((EXISTDATA_S *) data)->response.haschildren = 1;

	if(attribs & LATT_HASNOCHILDREN)
	  ((EXISTDATA_S *) data)->response.hasnochildren = 1;

	if(((EXISTDATA_S *) data)->fullname
	   && ((((EXISTDATA_S *) data)->args.reference[0] != '\0')
	         ? struncmp(((EXISTDATA_S *) data)->args.reference, mailbox,
			    strlen(((EXISTDATA_S *)data)->args.reference))
		 : struncmp(((EXISTDATA_S *) data)->args.name, mailbox,
			    strlen(((EXISTDATA_S *) data)->args.name)))){
	    char   *p;
	    size_t  len = (((stream && stream->mailbox)
			      ? strlen(stream->mailbox) : 0)
			   + strlen(((EXISTDATA_S *) data)->args.reference)
			   + strlen(((EXISTDATA_S *) data)->args.name)
			   + strlen(mailbox)) * sizeof(char);

	    /*
	     * Fully qualify (in the c-client name structure sense)
	     * anything that's not in the context of the "reference"...
	     */
	    if(*((EXISTDATA_S *) data)->fullname)
	      fs_give((void **) ((EXISTDATA_S *) data)->fullname);

	    *((EXISTDATA_S *) data)->fullname = (char *) fs_get(len);
	    if(*mailbox != '{'
	       && stream && stream->mailbox && *stream->mailbox == '{'
	       && (p = strindex(stream->mailbox, '}'))){
		strncpy(*((EXISTDATA_S *) data)->fullname,
			stream->mailbox, len = (p - stream->mailbox) + 1);
		p = *((EXISTDATA_S *) data)->fullname + len;
	    }
	    else
	      p = *((EXISTDATA_S *) data)->fullname;

	    strcpy(p, mailbox);
	}
    }
}



/*----------------------------------------------------------------------
  Return the path delimiter for the given folder on the given server

  Args: folder -- folder type for delimiter

    ----*/
int
folder_delimiter(folder)
    char *folder;
{
    MM_LIST_S    ldata;
    LISTRES_S	 response;
    int          ourstream = 0, we_cancel = 0;

    memset(mm_list_info = &ldata, 0, sizeof(MM_LIST_S));
    ldata.filter = mail_list_response;
    memset(ldata.data = &response, 0, sizeof(LISTRES_S));

    we_cancel = busy_alarm(1, NULL, NULL, 0);

    if(*folder == '{'
       && !(ldata.stream = sp_stream_get(folder, SP_MATCH))
       && !(ldata.stream = sp_stream_get(folder, SP_SAME))){
	if(ldata.stream = pine_mail_open(NULL,folder,
				 OP_HALFOPEN|OP_SILENT|SP_USEPOOL|SP_TEMPUSE,
					 NULL)){
	    ourstream++;
	}
	else{
	    if(we_cancel)
	      cancel_busy_alarm(-1);

	    return(FEX_ERROR);
	}
    }

    pine_mail_list(ldata.stream, folder, "", NULL);

    if(ourstream)
      pine_mail_close(ldata.stream);

    if(we_cancel)
      cancel_busy_alarm(-1);

    return(response.delim);
}


/*
 *
 */
char *
folder_as_breakout(cntxt, name)
    CONTEXT_S *cntxt;
    char      *name;
{
    if(context_isambig(name)){		/* if simple check doesn't pan out */
	char tmp[2*MAILTMPLEN], *p, *f;	/* look harder */

	if(!cntxt->dir->delim){
	    (void) context_apply(tmp, cntxt, "", sizeof(tmp)/2);
	    cntxt->dir->delim = folder_delimiter(tmp);
	}

	if(p = strindex(name, cntxt->dir->delim)){
	    if(p == name){		/* assumption 6,321: delim is root */
		if(cntxt->context[0] == '{'
		   && (p = strindex(cntxt->context, '}'))){
		    strncpy(tmp, cntxt->context,
			    min((p - cntxt->context) + 1, sizeof(tmp)/2));
		    tmp[min((p - cntxt->context) + 1, sizeof(tmp)/2)] = '\0';
		    strncpy(&tmp[min((p - cntxt->context) + 1, sizeof(tmp)/2)],
			    name, sizeof(tmp)/2-strlen(tmp));
		    tmp[sizeof(tmp)-1] = '\0';
		    return(cpystr(tmp));
		}
		else
		  return(cpystr(name));
	    }
	    else{			/* assumption 6,322: no create ~foo */
		strncpy(tmp, name, min(p - name, MAILTMPLEN));
		/* lop off trailingpath */
		tmp[min(p - name, sizeof(tmp)/2)] = '\0';
		f	      = NULL;
		(void)folder_name_exists(cntxt, tmp, &f);
		if(f){
		    sprintf(tmp, "%.*s%.*s",sizeof(tmp)/2,f,sizeof(tmp)/2,p);
		    tmp[sizeof(tmp)-1] = '\0';
		    fs_give((void **) &f);
		    return(cpystr(tmp));
		}
	    }
	}
    }

    return(NULL);
}



/*----------------------------------------------------------------------
  

    ----*/
void
mail_list_delim(stream, mailbox, delim, attribs, data)
    MAILSTREAM *stream; 
    char       *mailbox;
    int		delim;
    long	attribs;
    void       *data;
{
    if(delim)
      ((LISTRES_S *) data)->delim = delim;
}



/*----------------------------------------------------------------------
 Initialize global list of contexts for folder collections.

 Interprets collections defined in the pinerc and orders them for
 pine's use.  Parses user-provided context labels and sets appropriate 
 use flags and the default prototype for that collection. 
 (See find_folders for how the actual folder list is found).

  ----*/
void
init_folders(ps)
    struct pine *ps;
{
    CONTEXT_S  *tc, *top = NULL, **clist;
    int         i, prime = 0;

    clist = &top;

    /*
     * If no incoming folders are config'd, but the user asked for
     * them via feature, make sure at least "inbox" ends up there...
     */
    if(F_ON(F_ENABLE_INCOMING, ps) && !ps->VAR_INCOMING_FOLDERS){
	ps->VAR_INCOMING_FOLDERS    = (char **)fs_get(2 * sizeof(char *));
	ps->VAR_INCOMING_FOLDERS[0] = cpystr(ps->inbox_name);
	ps->VAR_INCOMING_FOLDERS[1] = NULL;
    }

    /*
     * Build context that's a list of folders the user's defined
     * as receiveing new messages.  At some point, this should
     * probably include adding a prefix with the new message count.
     * fake new context...
     */
    if(ps->VAR_INCOMING_FOLDERS && ps->VAR_INCOMING_FOLDERS[0]
       && (tc = new_context("Incoming-Folders []", NULL))){
	tc->dir->status &= ~CNTXT_NOFIND;
	tc->use    |= CNTXT_INCMNG;	/* mark this as incoming collection */
	if(tc->label)
	  fs_give((void **) &tc->label);

	tc->label = cpystr("Incoming Message Folders");

	*clist = tc;
	clist  = &tc->next;

	init_incoming_folder_list(ps, tc);
    }

    /*
     * Build list of folder collections.  Because of the way init.c
     * works, we're guaranteed at least a default.  Also write any
     * "bogus format" messages...
     */
    for(i = 0; ps->VAR_FOLDER_SPEC && ps->VAR_FOLDER_SPEC[i] ; i++)
      if(tc = new_context(ps->VAR_FOLDER_SPEC[i], &prime)){
	  *clist    = tc;			/* add it to list   */
	  clist	    = &tc->next;		/* prepare for next */
	  tc->var.v = &ps->vars[V_FOLDER_SPEC];
	  tc->var.i = i;
      }


    /*
     * Whoah cowboy!!!  Guess we couldn't find a valid folder
     * collection???
     */
    if(!prime)
      panic("No folder collections defined");

    /*
     * At this point, insert the INBOX mapping as the leading
     * folder entry of the first collection...
     */
    init_inbox_mapping(ps->VAR_INBOX_PATH, top);

    set_news_spec_current_val(TRUE, TRUE);

    /*
     * If news groups, loop thru list adding to collection list
     */
    for(i = 0; ps->VAR_NEWS_SPEC && ps->VAR_NEWS_SPEC[i] ; i++)
      if(ps->VAR_NEWS_SPEC[i][0]
	 && (tc = new_context(ps->VAR_NEWS_SPEC[i], NULL))){
	  *clist      = tc;			/* add it to list   */
	  clist       = &tc->next;		/* prepare for next */
	  tc->var.v   = &ps->vars[V_NEWS_SPEC];
	  tc->var.i   = i;
      }

    ps->context_list    = top;	/* init pointers */
    ps->context_current = (top->use & CNTXT_INCMNG) ? top->next : top;
    ps->context_last    = NULL;
    /* Tie up all the previous pointers */
    for(; top; top = top->next)
      if(top->next)
	top->next->prev = top;

#ifdef	DEBUG
    if(debugfile && debug > 7 && do_debug(debugfile))
      dump_contexts(debugfile);
#endif
}


/*
 * Add incoming list of folders to context.
 */
void
init_incoming_folder_list(ps, cntxt)
    struct pine *ps;
    CONTEXT_S   *cntxt;
{
    int       i;
    char     *folder_string, *nickname;
    FOLDER_S *f;

    for(i = 0; ps->VAR_INCOMING_FOLDERS[i] ; i++){
	/*
	 * Parse folder line for nickname and folder name.
	 * No nickname on line is OK.
	 */
	get_pair(ps->VAR_INCOMING_FOLDERS[i], &nickname, &folder_string,0,0);

	/*
	 * Allow for inbox to be specified in the incoming list, but
	 * don't let it show up along side the one magically inserted
	 * above!
	 */
	if(!folder_string || !strucmp(ps->inbox_name, folder_string)){
	    if(folder_string)
	      fs_give((void **)&folder_string);

	    if(nickname)
	      fs_give((void **)&nickname);

	    continue;
	}
	else if(update_bboard_spec(folder_string, tmp_20k_buf)){
	    fs_give((void **) &folder_string);
	    folder_string = cpystr(tmp_20k_buf);
	}

	f = new_folder(folder_string,
		       line_hash(ps->VAR_INCOMING_FOLDERS[i]));
	f->isfolder = 1;
	fs_give((void **)&folder_string);
	if(nickname){
	    if(strucmp(ps->inbox_name, nickname)){
		f->nickname = nickname;
		f->name_len = strlen(f->nickname);
	    }
	    else
	      fs_give((void **)&nickname);
	}

	folder_insert(f->nickname 
		      && (strucmp(f->nickname, ps->inbox_name) == 0)
			    ? -1 : folder_total(FOLDERS(cntxt)),
		      f, FOLDERS(cntxt));
    }
}


void
reinit_incoming_folder_list(ps, context)
    struct pine *ps;
    CONTEXT_S   *context;
{
    free_folder_entries(&(FOLDERS(context)));
    FOLDERS(context) = init_folder_entries();
    init_incoming_folder_list(ps_global, context);
    init_inbox_mapping(ps_global->VAR_INBOX_PATH, context);
}


int
news_in_folders(var)
    struct variable *var;
{
    int        i, found_news = 0;
    CONTEXT_S *tc;

    if(!(var && var->current_val.l))
      return(found_news);

    for(i=0; !found_news && var->current_val.l[i]; i++){
	if(tc = new_context(var->current_val.l[i], NULL)){
	    if(tc->use & CNTXT_NEWS)
	      found_news++;
	    
	    free_context(&tc);
	}
    }

    return(found_news);
}



#ifdef	DEBUG
void
dump_contexts(fp)
    FILE *fp;
{
    int i = 0;
    CONTEXT_S *c = ps_global->context_list;

    while(fp && c != NULL){
	fprintf(fp, "\n***** context %s\n", c->context);
	if(c->label)
	  fprintf(fp,"LABEL: %s\n", c->label);

	if(c->comment)
	  fprintf(fp,"COMMENT: %s\n", c->comment);
	
	for(i = 0; i < folder_total(FOLDERS(c)); i++)
	  fprintf(fp, "  %d) %s\n", i + 1, folder_entry(i,FOLDERS(c))->name);
	
	c = c->next;
    }
}
#endif


/*
 *  Release resources associated with global context list
 */
void
free_contexts(ctxt)
    CONTEXT_S **ctxt;
{
    if(ctxt && *ctxt){
	free_contexts(&(*ctxt)->next);
	free_context(ctxt);
    }
}


/*
 *  Release resources associated with the given context structure
 */
void
free_context(cntxt)
    CONTEXT_S **cntxt;
{
  if(cntxt && *cntxt){
    if((*cntxt)->context)
      fs_give((void **) &(*cntxt)->context);

    if((*cntxt)->server)
      fs_give((void **) &(*cntxt)->server);

    if((*cntxt)->nickname)
      fs_give((void **)&(*cntxt)->nickname);

    if((*cntxt)->label)
      fs_give((void **) &(*cntxt)->label);

    if((*cntxt)->comment)
      fs_give((void **) &(*cntxt)->comment);

    if((*cntxt)->selected.reference)
      fs_give((void **) &(*cntxt)->selected.reference);

    if((*cntxt)->selected.sub)
      free_selected(&(*cntxt)->selected.sub);

    free_strlist(&(*cntxt)->selected.folders);

    free_fdir(&(*cntxt)->dir, 1);

    fs_give((void **)cntxt);
  }
}


/*
 *
 */
void
init_inbox_mapping(path, cntxt)
     char      *path;
     CONTEXT_S *cntxt;
{
    FOLDER_S *f;

    /*
     * If mapping already exists, blast it and replace it below...
     */
    if((f = folder_entry(0, FOLDERS(cntxt)))
       && f->nickname && !strcmp(f->nickname, ps_global->inbox_name))
      folder_delete(0, FOLDERS(cntxt));

    if(path){
	f = new_folder(path, 0);
	f->nickname = cpystr(ps_global->inbox_name);
	f->name_len = strlen(f->nickname);
    }
    else
      f = new_folder(ps_global->inbox_name, 0);

    f->isfolder = 1;
    folder_insert(0, f, FOLDERS(cntxt));
}


/*
 *
 */
FDIR_S *
new_fdir(ref, view, wildcard)
    char *ref, *view;
    int   wildcard;
{
    FDIR_S *rv = (FDIR_S *) fs_get(sizeof(FDIR_S));

    memset((void *) rv, 0, sizeof(FDIR_S));

    /* Monkey with the view to make sure it has wildcard? */
    if(view && *view){
	rv->view.user = cpystr(view);

	/*
	 * This is sorta hairy since, for simplicity we allow
	 * users to use '*' in the view, but for mail
	 * we really mean '%' as def'd in 2060...
	 */
	if(wildcard == '*'){	/* must be #news. */
	    if(strindex(view, wildcard))
	      rv->view.internal = cpystr(view);
	}
	else{			/* must be mail */
	    char *p;

	    if(p = strpbrk(view, "*%")){
		rv->view.internal = p = cpystr(view);
		while(p = strpbrk(p, "*%"))
		  *p++ = '%';	/* convert everything to '%' */
	    }
	}

	if(!rv->view.internal){
	    rv->view.internal = (char *) fs_get((strlen(view)+3)*sizeof(char));
	    sprintf(rv->view.internal, "%c%s%c", wildcard, view, wildcard);
	}
    }
    else{
	rv->view.internal = (char *) fs_get(2 * sizeof(char));
	sprintf(rv->view.internal, "%c", wildcard);
    }

    if(ref)
      rv->ref = ref;

    rv->folders = init_folder_entries();
    rv->status = CNTXT_NOFIND;
    return(rv);
}


/*
 *
 */
void
free_fdir(f, recur)
    FDIR_S **f;
    int    recur;
{
    if(f && *f){
	if((*f)->prev && recur)
	  free_fdir(&(*f)->prev, 1);

	if((*f)->ref)
	  fs_give((void **)&(*f)->ref);

	if((*f)->view.user)
	  fs_give((void **)&(*f)->view.user);

	if((*f)->view.internal)
	  fs_give((void **)&(*f)->view.internal);

	if((*f)->desc)
	  fs_give((void **)&(*f)->desc);

	free_folder_entries(&(*f)->folders);
	fs_give((void **) f);
    }
}


/*
 *
 */
int
update_bboard_spec(bboard, buf)
      char *bboard, *buf;
{
    char *p = NULL;
    int	  nntp = 0, bracket = 0;

    if(*bboard == '*'
       || (*bboard == '{' && (p = strindex(bboard, '}')) && *(p+1) == '*')){
	/* server name ? */
	if(p || (*(bboard+1) == '{' && (p = strindex(++bboard, '}'))))
	  while(bboard <= p)		/* copy it */
	    if((*buf++ = *bboard++) == '/' && !strncmp(bboard, "nntp", 4))
	      nntp++;

	if(*bboard == '*')
	  bboard++;

	if(!nntp)
	  /*
	   * See if path portion looks newsgroup-ish while being aware
	   * of the "view" portion of the spec...
	   */
	  for(p = bboard; *p; p++)
	    if(*p == '['){
		if(bracket)		/* only one set allowed! */
		  break;
		else
		  bracket++;
	    }
	    else if(*p == ']'){
		if(bracket != 1)	/* must be closing bracket */
		  break;
		else
		  bracket++;
	    }
	    else if(!(isalnum((unsigned char) *p) || strindex(".-", *p)))
	      break;

	sprintf(buf, "%s%s%s",
		(!nntp && *p) ? "#public" : "#news.",
		(!nntp && *p && *bboard != '/') ? "/" : "",
		bboard);

	return(1);
    }

    return(0);
}


/*
 * new_context - creates and fills in a new context structure, leaving
 *               blank the actual folder list (to be filled in later as
 *               needed).  Also, parses the context string provided 
 *               picking out any user defined label.  Context lines are
 *               of the form:
 *
 *               [ ["] <string> ["] <white-space>] <context>
 *
 */
CONTEXT_S *
new_context(cntxt_string, prime)
    char *cntxt_string;
    int  *prime;
{
    CONTEXT_S  *c;
    char        host[MAXPATH], rcontext[MAXPATH],
		view[MAXPATH], dcontext[MAXPATH],
	       *nickname = NULL, *c_string = NULL, *p;

    /*
     * do any context string parsing (like splitting user-supplied 
     * label from actual context)...
     */
    get_pair(cntxt_string, &nickname, &c_string, 0, 0);

    if(update_bboard_spec(c_string, tmp_20k_buf)){
	fs_give((void **) &c_string);
	c_string = cpystr(tmp_20k_buf);
    }

    if(c_string && *c_string == '\"')
      (void)removing_double_quotes(c_string);

    if(p = context_digest(c_string, dcontext, host, rcontext, view, MAXPATH)){
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Bad context, %.200s : %.200s", p, c_string);
	fs_give((void **) &c_string);
	if(nickname)
	  fs_give((void **)&nickname);

	return(NULL);
    }
    else
      fs_give((void **) &c_string);

    c = (CONTEXT_S *) fs_get(sizeof(CONTEXT_S)); /* get new context   */
    memset((void *) c, 0, sizeof(CONTEXT_S));	/* and initialize it */
    if(*host)
      c->server  = cpystr(host);		/* server + params */

    c->context = cpystr(dcontext);

    if(strstr(c->context, "#news."))
      c->use |= CNTXT_NEWS;

    c->dir = new_fdir(NULL, view, (c->use & CNTXT_NEWS) ? '*' : '%');

    /* fix up nickname */
    if(!(c->nickname = nickname)){			/* make one up! */
	sprintf(tmp_20k_buf, "%s%s%.100s",
		(c->use & CNTXT_NEWS)
		  ? "News"
		  : (c->dir->ref)
		      ? (c->dir->ref) :"Mail",
		(c->server) ? " on " : "",
		(c->server) ? c->server : "");
	c->nickname = cpystr(tmp_20k_buf);
    }

    if(prime && !*prime){
	*prime = 1;
	c->use |= CNTXT_SAVEDFLT;
    }

    /* fix up label */
    if(NEWS_TEST(c)){
	sprintf(tmp_20k_buf, "%sews groups%s%.100s",
		(*host) ? "N" : "Local n", (*host) ? " on " : "",
		(*host) ? host : "");
    }
    else{
	p = srchstr(rcontext, "[]");
	sprintf(tmp_20k_buf, "%solders%s%.100s in %.*s%s",
		(*host) ? "F" : "Local f", (*host) ? " on " : "",
		(*host) ? host : "",
		p ? min(p - rcontext, 100) : 0,
		rcontext, (p && (p - rcontext) > 0) ? "" : "home directory");
    }

    c->label = cpystr(tmp_20k_buf);

    dprint(5, (debugfile, "Context: %s serv:%s ref: %s view: %s\n",
	       c->context ? c->context : "?",
	       (c->server) ? c->server : "\"\"",
	       (c->dir->ref) ? c->dir->ref : "\"\"",
	       (c->dir->view.user) ? c->dir->view.user : "\"\""));

    return(c);
}



/*
 * build_folder_list - call mail_list to fetch us a list of folders
 *		       from the given context.
 */
void
build_folder_list(stream, context, pat, content, flags)
    MAILSTREAM **stream;
    CONTEXT_S   *context;
    char        *pat, *content;
    int		 flags;
{
    MM_LIST_S  ldata;
    BFL_DATA_S response;
    int	       local_open = 0, we_cancel = 0;
    char       reference[2*MAILTMPLEN], *p;

    if(!(context->dir->status & CNTXT_NOFIND)
       || (context->dir->status & CNTXT_PARTFIND))
      return;				/* find already done! */

    dprint(7, (debugfile, "build_folder_list: %s %s\n",
	       context ? context->context : "NULL",
	       pat ? pat : "NULL"));

    we_cancel = busy_alarm(1, NULL, NULL, 0);

    /*
     * Set up the pattern of folder name's to match within the
     * given context.
     */
    if(!pat || ((*pat == '*' || *pat == '%') && *(pat+1) == '\0')){
	context->dir->status &= ~CNTXT_NOFIND;	/* let'em know we tried */
	pat = context->dir->view.internal;
    }
    else
      context->use |= CNTXT_PARTFIND;	/* or are in a partial find */

    memset(mm_list_info = &ldata, 0, sizeof(MM_LIST_S));
    ldata.filter = (NEWS_TEST(context)) ? mail_lsub_filter : mail_list_filter;
    memset(ldata.data = &response, 0, sizeof(BFL_DATA_S));
    response.list = FOLDERS(context);

    if(flags & BFL_FLDRONLY)
      response.mask = LATT_NOSELECT;

    /*
     * if context is associated with a server, prepare a stream for
     * sending our request.
     */
    if(*context->context == '{'){
	/*
	 * Try using a stream we've already got open...
	 */
	if(stream && *stream
	   && !(ldata.stream = same_stream(context->context, *stream))){
	    pine_mail_close(*stream);
	    *stream = NULL;
	}

	if(!ldata.stream)
	  ldata.stream = sp_stream_get(context->context, SP_MATCH);

	if(!ldata.stream)
	  ldata.stream = sp_stream_get(context->context, SP_SAME);

	/* gotta open a new one? */
	if(!ldata.stream){
	    ldata.stream = mail_cmd_stream(context, &local_open);
	    if(stream)
	      *stream = ldata.stream;
	}

	dprint(ldata.stream ? 7 : 1,
	       (debugfile, "build_folder_list: mail_open(%s) %s.\n",
		context->server ? context->server : "?",
		ldata.stream ? "OK" : "FAILED"));

	if(!ldata.stream){
	    context->use &= ~CNTXT_PARTFIND;	/* unset partial find bit */
	    if(we_cancel)
	      cancel_busy_alarm(-1);

	    return;
	}
    }
    else if(stream && *stream){			/* no server, simple case */
	if(!sp_flagged(*stream, SP_LOCKED))
	  pine_mail_close(*stream);

	*stream = NULL;
    }

    /*
     * If preset reference string, we're somewhere in the hierarchy.
     * ELSE we must be at top of context...
     */
    response.args.name = pat;
    if(!(response.args.reference = context->dir->ref)){
	if(p = strstr(context->context, "%s")){
	    strncpy(response.args.reference = reference,
		    context->context,
		    min(p - context->context, sizeof(reference)/2));
	    reference[min(p - context->context, sizeof(reference)/2)] = '\0';
	    if(*(p += 2))
	      response.args.tail = p;
	}
	else
	  response.args.reference = context->context;
    }

    if(flags & BFL_SCAN)
      mail_scan(ldata.stream, response.args.reference,
		response.args.name, content);
    else if(flags & BFL_LSUB)
      mail_lsub(ldata.stream, response.args.reference, response.args.name);
    else{
      set_read_predicted(1);
      pine_mail_list(ldata.stream, response.args.reference, response.args.name,
		     &ldata.options);
      set_read_predicted(0);
    }

    if(context->dir && response.response.delim)
      context->dir->delim = response.response.delim;

    if(!(flags & (BFL_LSUB|BFL_SCAN)) && F_ON(F_QUELL_EMPTY_DIRS, ps_global)){
	LISTRES_S  listres;
	FOLDER_S  *f;
	int	   i;

	for(i = 0; i < folder_total(response.list); i++)
	  if((f = folder_entry(i, FOLDERS(context)))->isdir){
	      /* 
	       * we don't need to do a list if we know already
	       * whether there are children or not.
	       */
	      if(!f->haschildren && !f->hasnochildren){
		  memset(ldata.data = &listres, 0, sizeof(LISTRES_S));
		  ldata.filter = mail_list_response;

		  if(context->dir->ref)
		    sprintf(reference, "%.*s%.*s",
			    sizeof(reference)/2-1, context->dir->ref,
			    sizeof(reference)/2-1, f->name);
		  else
		    context_apply(reference, context, f->name, sizeof(reference)/2);

		  /* append the delimiter to the reference */
		  for(p = reference; *p; p++)
		    ;

		  *p++ = context->dir->delim;
		  *p   = '\0';

		  pine_mail_list(ldata.stream, reference, "%", NULL);
	      }

	      /* anything interesting inside? */
	      if(f->hasnochildren ||
		 (!f->haschildren && listres.count <= 1L)){
		  if(f->isfolder){
		      f->isdir = 0;
		  }
		  else{
		      folder_delete(i, FOLDERS(context));
		      i--;	/* offset inc'ing */
		  }
	      }
	  }
    }

    if(local_open && !stream)
      pine_mail_close(ldata.stream);

    if(context->use & CNTXT_PRESRV)
      folder_select_restore(context);

    context->use &= ~CNTXT_PARTFIND;	/* unset partial list bit */
    if(we_cancel)
      cancel_busy_alarm(-1);
}


/*
 * Validate LIST response to command issued from build_folder_list
 *
 */
void
mail_list_filter(stream, mailbox, delim, attribs, data, options)
    MAILSTREAM *stream;
    char       *mailbox;
    int		delim;
    long	attribs;
    void       *data;
    unsigned    options;
{
    BFL_DATA_S *ld = (BFL_DATA_S *) data;
    FOLDER_S   *new_f = NULL, *dual_f = NULL;

    if(delim)
      ld->response.delim = delim;

    /* test against mask of DIS-allowed attributes */
    if((ld->mask & attribs)){
	dprint(3, (debugfile, "mail_list_filter: failed attribute test"));
	return;
    }

    /*
     * First, make sure response fits our "reference" arg 
     * NOTE: build_folder_list can't supply breakout?
     */
    if(!mail_list_in_collection(&mailbox, ld->args.reference,
				ld->args.name, ld->args.tail))
      return;

    /* Does result  */
    if(F_OFF(F_ENABLE_DOT_FOLDERS, ps_global) && *mailbox == '.'){
	dprint(3, (debugfile, "mail_list_filter: dotfolder disallowed"));
	return;
    }

    /*
     * "Inbox" is filtered out here, since pine only supports one true
     * inbox...
     */
    if(!(attribs & LATT_NOSELECT) && strucmp(mailbox, "inbox")){
	ld->response.count++;
	ld->response.isfile = 1;
	new_f = new_folder(mailbox, 0);
	new_f->isfolder = 1;

	if(F_ON(F_SEPARATE_FLDR_AS_DIR, ps_global)){
	    folder_insert(-1, new_f, ld->list);
	    dual_f = new_f;
	    new_f = NULL;
	}
    }

    /* directory? */
    if(delim && !(attribs & LATT_NOINFERIORS)){
	ld->response.count++;
	ld->response.isdir = 1;

	if(F_OFF(F_SEPARATE_FLDR_AS_DIR, ps_global)
	   && !strucmp(mailbox, "inbox")){
	    FOLDER_S *f;
	    int	      i;

	    for(i = 0; i < folder_total(ld->list); i++)
	      if(!strucmp((f = folder_entry(i, ld->list))->name, "inbox")){
		  f->isdir = 1;
		  if(attribs & LATT_HASCHILDREN)
		    f->haschildren = 1;
		  if(attribs & LATT_HASNOCHILDREN)
		    f->hasnochildren = 1;
		  break;
	      }
	}
	else{
	    int dual;

	    if(!new_f)
	      new_f = new_folder(mailbox, 0);

	    new_f->isdir = 1;
	    if(attribs & LATT_HASCHILDREN)
	      new_f->haschildren = 1;
	    if(attribs & LATT_HASNOCHILDREN)
	      new_f->hasnochildren = 1;

	    /*
	     * When we have F_SEPARATE_FLDR_AS_DIR we still want to know
	     * whether the name really represents both so that we don't
	     * inadvertently delete both when the user meant one or the
	     * other.
	     */
	    if(dual_f){
		if(attribs & LATT_HASCHILDREN)
		  dual_f->haschildren = 1;
		if(attribs & LATT_HASNOCHILDREN)
		  dual_f->hasnochildren = 1;

		dual_f->isdual = dual = 1;
	    }
	    else
	      dual = 0;

	    new_f->isdual = dual;
	}
    }

    if(new_f)
      folder_insert(-1, new_f, ld->list);

    if(attribs & LATT_MARKED)
      ld->response.ismarked = 1;

    /* don't mark #move folders unmarked */
    if(attribs & LATT_UNMARKED && !(options & PML_IS_MOVE_MBOX))
      ld->response.unmarked = 1;

    if(attribs & LATT_HASCHILDREN)
      ld->response.haschildren = 1;

    if(attribs & LATT_HASNOCHILDREN)
      ld->response.hasnochildren = 1;
}


/*
 * Validate LSUB response to command issued from build_folder_list
 *
 */
void
mail_lsub_filter(stream, mailbox, delim, attribs, data, options)
    MAILSTREAM *stream;
    char       *mailbox;
    int		delim;
    long	attribs;
    void       *data;
    unsigned    options;	/* unused */
{
    BFL_DATA_S *ld = (BFL_DATA_S *) data;
    FOLDER_S   *new_f = NULL;
    char       *ref;

    if(delim)
      ld->response.delim = delim;

    /* test against mask of DIS-allowed attributes */
    if((ld->mask & attribs)){
	dprint(3, (debugfile, "mail_lsub_filter: failed attribute test"));
	return;
    }

    /* Normalize mailbox and reference strings re: namespace */
    if(!strncmp(mailbox, "#news.", 6))
      mailbox += 6;

    if(!strncmp(ref = ld->args.reference, "#news.", 6))
      ref += 6;

    if(!mail_list_in_collection(&mailbox, ref, ld->args.name, ld->args.tail))
      return;

    if(!(attribs & LATT_NOSELECT)){
	ld->response.count++;
	ld->response.isfile = 1;
	new_f = new_folder(mailbox, 0);
	new_f->isfolder = 1;
	folder_insert(F_ON(F_READ_IN_NEWSRC_ORDER, ps_global)
			? folder_total(ld->list) : -1,
		      new_f, ld->list);
    }

    /* We don't support directories in #news */
}


/*
 * 
 */
int
mail_list_in_collection(mailbox, ref, name, tail)
    char **mailbox, *ref, *name, *tail;
{
    int	  boxlen, reflen, taillen;
    char *p;

    boxlen  = strlen(*mailbox);
    reflen  = ref ? strlen(ref) : 0;
    taillen = tail ? strlen(tail) : 0;

    if(boxlen
       && (reflen ? !struncmp(*mailbox, ref, reflen)
		  : (p = strpbrk(name, "%*"))
		       ? !struncmp(*mailbox, name, reflen = p - name)
		       : !strucmp(*mailbox,name))
       && (!taillen
	   || (taillen < boxlen - reflen
	       && !strucmp(&(*mailbox)[boxlen - taillen], tail)))){
	if(taillen)
	  (*mailbox)[boxlen - taillen] = '\0';

	if(*(*mailbox += reflen))
	  return(TRUE);
    }
    /*
     * else don't worry about context "breakouts" since
     * build_folder_list doesn't let the user introduce
     * one...
     */

    return(FALSE);
}


/*
 * rebuild_folder_list -- free up old list and re-issue commands to build
 *			  a new list.
 */
void
refresh_folder_list(context, nodirs, startover, streamp)
    CONTEXT_S   *context;
    int	         nodirs, startover;
    MAILSTREAM **streamp;
{
    if(startover)
      free_folder_list(context);

    build_folder_list(streamp, context, NULL, NULL,
		      (NEWS_TEST(context) ? BFL_LSUB : BFL_NONE)
		        | ((nodirs) ? BFL_FLDRONLY : BFL_NONE));
}


/*
 * free_folder_list - loop thru the context's lists of folders
 *                     clearing all entries without nicknames
 *                     (as those were user provided) AND reset the 
 *                     context's find flag.
 *
 * NOTE: if fetched() information (e.g., like message counts come back
 *       in bboard collections), we may want to have a check before
 *       executing the loop and setting the FIND flag.
 */
void
free_folder_list(cntxt)
    CONTEXT_S *cntxt;
{
    int n, i;

    /*
     * In this case, don't blast the list as it was given to us by the
     * user and not the result of a mail_list call...
     */
    if(cntxt->use & CNTXT_INCMNG)
      return;

    if(cntxt->use & CNTXT_PRESRV)
      folder_select_preserve(cntxt);

    for(n = folder_total(FOLDERS(cntxt)), i = 0; n > 0; n--)
      if(folder_entry(i, FOLDERS(cntxt))->nickname)
	i++;					/* entry wasn't from LIST */
      else
	folder_delete(i, FOLDERS(cntxt));

    cntxt->dir->status |= CNTXT_NOFIND;		/* do find next time...  */
						/* or add the fake entry */
    cntxt->use &= ~(CNTXT_PSEUDO | CNTXT_PRESRV | CNTXT_ZOOM);
}


/*
 * default_save_context - return the default context for saved messages
 */
CONTEXT_S *
default_save_context(cntxt)
    CONTEXT_S *cntxt;
{
    while(cntxt)
      if((cntxt->use) & CNTXT_SAVEDFLT)
	return(cntxt);
      else
	cntxt = cntxt->next;

    return(NULL);
}



/*
 * folder_complete - foldername completion routine
 *
 *   Result: returns 0 if the folder doesn't have a any completetion
 *		     1 if the folder has a completion (*AND* "name" is
 *		       replaced with the completion)
 *
 */
folder_complete(context, name, completions)
    CONTEXT_S *context;
    char      *name;
    int	      *completions;
{
    return(folder_complete_internal(context, name, completions, FC_NONE));
}


/*
 * 
 */
int
folder_complete_internal(context, name, completions, flags)
    CONTEXT_S *context;
    char      *name;
    int	      *completions;
    int	       flags;
{
    int	      i, match = -1, ftotal;
    char      tmp[MAXFOLDER+2], *a, *b, *fn, *pat;
    FOLDER_S *f;

    if(completions)
      *completions = 0;

    if(*name == '\0' || !context_isambig(name))
      return(0);

    if(!((context->use & CNTXT_INCMNG) || ALL_FOUND(context))){
	/*
	 * Build the folder list from scratch since we may need to
	 * traverse hierarchy...
	 */
	
	free_folder_list(context);
	sprintf(tmp, "%.*s%c", sizeof(tmp)-2, name,
		NEWS_TEST(context) ? '*' : '%');
	build_folder_list(NULL, context, tmp, NULL,
			  (NEWS_TEST(context) & !(flags & FC_FORCE_LIST))
			    ? BFL_LSUB : BFL_NONE);
    }

    *tmp = '\0';			/* find uniq substring */
    ftotal = folder_total(FOLDERS(context));
    for(i = 0; i < ftotal; i++){
	f   = folder_entry(i, FOLDERS(context));
	fn  = FLDR_NAME(f);
	pat = name;
	if(!(NEWS_TEST(context) || (context->use & CNTXT_INCMNG))){
	    fn  = folder_last_cmpnt(fn, context->dir->delim);
	    pat = folder_last_cmpnt(pat, context->dir->delim);
	}

	if(!strncmp(fn, pat, strlen(pat))){
	    if(match != -1){		/* oh well, do best we can... */
		a = fn;
		if(match >= 0){
		    f  = folder_entry(match, FOLDERS(context));
		    fn = FLDR_NAME(f);
		    if(!NEWS_TEST(context))
		      fn = folder_last_cmpnt(fn, context->dir->delim);

		    strncpy(tmp, fn, sizeof(tmp)-1);
		    tmp[sizeof(tmp)-1] = '\0';
		}

		match = -2;
		b     = tmp;		/* remember largest common text */
		while(*a && *b && *a == *b)
		  *b++ = *a++;

		*b = '\0';
	    }
	    else		
	      match = i;		/* bingo?? */
	}
    }

    if(match >= 0){			/* found! */
	f  = folder_entry(match, FOLDERS(context));
	fn = FLDR_NAME(f);
	if(!(NEWS_TEST(context) || (context->use & CNTXT_INCMNG)))
	  fn = folder_last_cmpnt(fn, context->dir->delim);

	strcpy(pat, fn);
	if(f->isdir){
	    name[i = strlen(name)] = context->dir->delim;
	    name[i+1] = '\0';
	}
    }
    else if(match == -2)		/* closest we could find */
      strcpy(pat, tmp);

    if(completions)
      *completions = ftotal;

    if(!((context->use & CNTXT_INCMNG) || ALL_FOUND(context)))
      free_folder_list(context);

    return((match >= 0) ? ftotal : 0);
}


/*
 *
 */
char *
folder_last_cmpnt(s, d)
    char *s;
    int   d;
{
    register char *p;
    
    if(d)
      for(p = s; p = strindex(p, d); s = ++p)
	;

    return(s);
}


/*
 *           FOLDER MANAGEMENT ROUTINES
 */


/*
 * Folder List Structure - provides for two ways to access and manage
 *                         folder list data.  One as an array of pointers
 *                         to folder structs or
 */
typedef struct folder_list {
    unsigned   used;
    unsigned   allocated;
#ifdef	DOSXXX
    FILE      *folders;		/* tmpfile of binary */
#else
    FOLDER_S **folders;		/* array of pointers to folder structs */
#endif
} FLIST;

#define FCHUNK  64


/*
 * init_folder_entries - return a piece of memory suitable for attaching 
 *                   a list of folders...
 *
 */
void *
init_folder_entries()
{
    FLIST *flist = (FLIST *) fs_get(sizeof(FLIST));
    flist->folders = (FOLDER_S **) fs_get(FCHUNK * sizeof(FOLDER_S *));
    memset((void *)flist->folders, 0, (FCHUNK * sizeof(FOLDER_S *)));
    flist->allocated = FCHUNK;
    flist->used      = 0;
    return((void *)flist);
}



/*
 * new_folder - return a brand new folder entry, with the given name
 *              filled in.
 *
 * NOTE: THIS IS THE ONLY WAY TO PUT A NAME INTO A FOLDER ENTRY!!!
 * STRCPY WILL NOT WORK!!!
 */
FOLDER_S *
new_folder(name, hash)
    char *name;
    unsigned long hash;
{
#ifdef	DOSXXX
#else
    FOLDER_S *tmp;
    size_t    l = strlen(name);

    tmp = (FOLDER_S *)fs_get(sizeof(FOLDER_S) + (l * sizeof(char)));
    memset((void *)tmp, 0, sizeof(FOLDER_S));
    strcpy(tmp->name, name);
    tmp->name_len = (unsigned char) l;
    tmp->varhash = hash;
    return(tmp);
#endif
}



/*
 * folder_entry - folder struct access routine.  Permit reference to
 *                folder structs via index number. Serves two purposes:
 *            1) easy way for callers to access folder data 
 *               conveniently
 *            2) allows for a custom manager to limit memory use
 *               under certain rather limited "operating systems"
 *               who shall renameless, but whose initials are DOS
 *
 *
 */
FOLDER_S *
folder_entry(i, flist)
    int   i;
    void *flist;
{
#ifdef	DOSXXX
    /*
     * Manage entries within a limited amount of core (what a drag).
     */

    fseek((FLIST *)flist->folders, i * sizeof(FOLDER_S) + MAXPATH, 0);
    fread(&folder, sizeof(FOLDER_S) + MAXPATH, (FLIST *)flist->folders);
#else
    return((i >= ((FLIST *)flist)->used) ? NULL:((FLIST *)flist)->folders[i]);
#endif
}



/*
 * free_folder_entries - release all resources associated with the given 
 *			 list of folder entries
 */
void
free_folder_entries(flist)
    void **flist;
{
#ifdef	DOSXXX
    fclose((*((FLIST **)flist))->folders); 	/* close folder tmpfile */
#else
    register int i;

    if(!(flist && *flist))
      return;

    i = (*((FLIST **)flist))->used;
    while(i--){
	if((*((FLIST **)flist))->folders[i]->nickname)
	  fs_give((void **)&(*((FLIST **)flist))->folders[i]->nickname);

	fs_give((void **)&((*((FLIST **)flist))->folders[i]));
    }

    fs_give((void **)&((*((FLIST **)flist))->folders));
#endif
    fs_give(flist);
}



/*
 * return the number of folders associated with the given folder list
 */
int
folder_total(flist)
    void *flist;
{
    return((int)((FLIST *)flist)->used);
}


/*
 * return the index number of the given name in the given folder list
 */
int
folder_index(name, cntxt, flags)
    char      *name;
    CONTEXT_S *cntxt;
    int	       flags;
{
    register  int i = 0;
    FOLDER_S *f;
    char     *fname;

    for(i = 0; f = folder_entry(i, FOLDERS(cntxt)); i++)
      if(((flags & FI_FOLDER) && (f->isfolder || (cntxt->use & CNTXT_INCMNG)))
	 || ((flags & FI_DIR) && f->isdir)){
	  fname = FLDR_NAME(f);
#if defined(DOS) || defined(OS2)
	  if(flags & FI_RENAME){	/* case-dependent for rename */
	    if(*name == *fname && strcmp(name, fname) == 0)
	      return(i);
	  }
	  else{
	    if(toupper((unsigned char)(*name))
	       == toupper((unsigned char)(*fname)) && strucmp(name, fname) == 0)
	      return(i);
	  }
#else
	  if(*name == *fname && strcmp(name, fname) == 0)
	    return(i);
#endif
      }

    return(-1);
}



/*
 * next_folder - given a current folder in a context, return the next in
 *               the list, or NULL if no more or there's a problem.
 *
 *  Args   streamp -- If set, try to re-use this stream for checking.
 *            next -- Put return value here, return points to this
 *         current -- Current folder, so we know where to start looking
 *           cntxt --
 *     find_recent -- Returns the number of recent here. The presence of
 *                     this arg also indicates that we should do the calls
 *                     to figure out whether there is a next interesting folder
 *                     or not.
 *      did_cancel -- Tell caller if user canceled. Only used if find_recent
 *                     is also set. Also, user will only be given the
 *                     opportunity to cancel if this is set. If it isn't
 *                     set, we just plow ahead when there is an error or
 *                     when the folder does not exist.
 */
char *
next_folder(streamp, next, current, cntxt, find_recent, did_cancel)
    MAILSTREAM **streamp;
    char	*current, *next;
    CONTEXT_S	*cntxt;
    long	*find_recent;
    int         *did_cancel;
{
    int       index, recent = 0, failed_status = 0, try_fast;
    char      prompt[128];
    FOLDER_S *f = NULL;
    char      tmp[MAILTMPLEN];


    /* note: find_folders may assign "stream" */
    build_folder_list(streamp, cntxt, NULL, NULL,
		      NEWS_TEST(cntxt) ? BFL_LSUB : BFL_NONE);

    try_fast = (F_ON(F_ENABLE_FAST_RECENT, ps_global) &&
	        F_OFF(F_TAB_USES_UNSEEN, ps_global));
    if(find_recent)
      *find_recent = 0L;

    for(index = folder_index(current, cntxt, FI_FOLDER) + 1;
	index > 0
	&& index < folder_total(FOLDERS(cntxt))
	&& (f = folder_entry(index, FOLDERS(cntxt)))
	&& !f->isdir;
	index++)
      if(find_recent){
	  MAILSTREAM *stream = NULL;
	  int         rv, we_cancel = 0, mlen, match;
	  char        msg_buf[MAX_BM+1], mbuf[MAX_BM+1];

	  /* must be a folder and it can't be the current one */
	  if(ps_global->context_current == ps_global->context_list
	     && !strcmp(ps_global->cur_folder, FLDR_NAME(f)))
	    continue;

	  /*
	   * If we already have the folder open, short circuit all this
	   * stuff.
	   */
	  match = 0;
	  if((stream = sp_stream_get(context_apply(tmp, cntxt, f->name,
						   sizeof(tmp)),
				     SP_MATCH)) != NULL){
	      (void) pine_mail_ping(stream);

	      if(F_ON(F_TAB_USES_UNSEEN, ps_global)){
		  /*
		   * Just fall through and let the status call below handle
		   * the already open stream. If we were doing this the
		   * same as the else case, we would figure out how many
		   * unseen are in this open stream by doing a search.
		   * Instead of repeating that code that is already in
		   * pine_mail_status_full, fall through and note the
		   * special case by lighting the match variable.
		   */
		  match++;
	      }
	      else{
		  *find_recent = sp_recent_since_visited(stream);
		  if(*find_recent){
		      recent++;
		      break;
		  }

		  continue;
	      }
	  }

	  /* 29 is length of constant strings and 7 is for busy_alarm */
	  mlen = min(MAX_BM, ps_global->ttyo->screen_cols) - (29+7);
	  sprintf(msg_buf, "Checking %.*s for recent messages",
		  sizeof(msg_buf)-29,
		  (mlen > 5) ? short_str(FLDR_NAME(f), mbuf, mlen, FrontDots)
			     : FLDR_NAME(f));
	  we_cancel = busy_alarm(1, msg_buf, NULL, 1);

	  /* First, get a stream for the test */
	  if(!stream && streamp && *streamp){
	      if(context_same_stream(cntxt, f->name, *streamp)){
		  stream = *streamp;
	      }
	      else{
		  pine_mail_close(*streamp);
		  *streamp = NULL;
	      }
	  }

	  if(!stream){
	      stream = sp_stream_get(context_apply(tmp, cntxt, f->name,
						   sizeof(tmp)),
				     SP_SAME);
	  }

	  /*
	   * If interestingness is indeterminate or we're
	   * told to explicitly, look harder...
	   */

	  /*
	   * We could make this more efficient in the cases where we're
	   * opening a new stream or using streamp by having folder_exists
	   * cache the stream. The change would require a folder_exists()
	   * that caches streams, but most of the time folder_exists just
	   * uses the inbox stream or ps->mail_stream.
	   *
	   * Another thing to consider is that maybe there should be an
	   * option to try to LIST a folder before doing a STATUS (or SELECT).
	   * This isn't done by default for the case where a folder is
	   * SELECTable but not LISTable, but on some servers doing an
	   * RLIST first tells the server that we support mailbox referrals.
	   */
	  if(!try_fast
	     || !((rv = folder_exists(cntxt,f->name))
					     & (FEX_ISMARKED | FEX_UNMARKED))){
	      extern MAILSTATUS mm_status_result;

	      if(try_fast && (rv == 0 || rv & FEX_ERROR)){
		  failed_status = 1;
		  mm_status_result.flags = 0L;
	      }
	      else{
		  if(stream){
		      if(!context_status_full(cntxt, match ? NULL : stream,
					      f->name,
					      F_ON(F_TAB_USES_UNSEEN, ps_global)
					        ? SA_UNSEEN : SA_RECENT,
					      &f->uidvalidity,
					      &f->uidnext)){
			  failed_status = 1;
			  mm_status_result.flags = 0L;
		      }
		  }
		  else{
		      /* so we can re-use the stream */
		      if(!context_status_streamp_full(cntxt, streamp, f->name,
					      F_ON(F_TAB_USES_UNSEEN, ps_global)
					        ? SA_UNSEEN : SA_RECENT,
						      &f->uidvalidity,
						      &f->uidnext)){
			  failed_status = 1;
			  mm_status_result.flags = 0L;
		      }
		  }
	      }

	      if(F_ON(F_TAB_USES_UNSEEN, ps_global)){
		  rv = ((mm_status_result.flags & SA_UNSEEN)
			&& (*find_recent = mm_status_result.unseen))
			 ? FEX_ISMARKED : 0;
	      }
	      else{
		  rv = ((mm_status_result.flags & SA_RECENT)
			&& (*find_recent = mm_status_result.recent))
			 ? FEX_ISMARKED : 0;
	      }

	      /* we don't know how many in this case */
	      if(try_fast)
		*find_recent = 0L;	/* consistency, boy! */
	  }

	  if(we_cancel)
	    cancel_busy_alarm(0);

	  if(failed_status && did_cancel){
	    sprintf(prompt, "Checking of folder %.30s%s failed. Continue ",
		    FLDR_NAME(f), strlen(FLDR_NAME(f)) > 30 ? "..." : "" );
	    if(want_to(prompt, 'y', 0, NO_HELP, WT_NORM) == 'n'){
	      *did_cancel = 1;
	      break;
	    }
	  }

	  failed_status = 0;

	  if(rv & FEX_ISMARKED){
	      recent++;
	      break;
	  }
      }

    if(f && (!find_recent || recent))
      strcpy(next, FLDR_NAME(f));
    else
      *next = '\0';

    /* BUG: how can this be made smarter so we cache the list? */
    free_folder_list(cntxt);
    return((*next) ? next : NULL);
}



/*
 * folder_is_nick - check to see if the given name is a nickname
 *                  for some folder in the given context...
 *
 *  NOTE: no need to check if mm_list_names has been done as 
 *        nicknames can only be set by configuration...
 */
char *
folder_is_nick(nickname, flist, flags)
    char *nickname;
    void *flist;
    int   flags;
{
    register  int  i = 0;
    FOLDER_S *f;

    while(f = folder_entry(i, flist)){
	if(f->nickname && strcmp(nickname, f->nickname) == 0){
	    char source[MAILTMPLEN], *target = NULL;

	    /*
	     * If f is a maildrop, then we want to return the
	     * destination folder, not the whole #move thing.
	     */
	    if(!(flags & FN_WHOLE_NAME)
	       && check_for_move_mbox(f->name, source, sizeof(source), &target))
	      return(target);
	    else
	      return(f->name);
	}
	else
	  i++;
    }

    return(NULL);
}



/*----------------------------------------------------------------------
  Insert the given folder name into the sorted folder list
  associated with the given context.  Only allow ambiguous folder
  names IF associated with a nickname.

   Args: index  -- Index to insert at, OR insert in sorted order if -1
         folder -- folder structure to insert into list
	 flist  -- folder list to insert folder into

  **** WARNING ****
  DON'T count on the folder pointer being valid after this returns
  *** ALL FOLDER ELEMENT READS SHOULD BE THRU folder_entry() ***

  ----*/
int
folder_insert(index, folder, flist)
    int       index;
    FOLDER_S *folder;
    void     *flist;
{
    /* requested index < 0 means add to sorted list */
    if(index < 0 && (index = folder_total(flist)) > 0)
      index = folder_insert_sorted(index / 2, 0, index, folder, flist,
		     (ps_global->fld_sort_rule == FLD_SORT_ALPHA_DIR_FIRST)
			? compare_folders_dir_alpha
			: (ps_global->fld_sort_rule == FLD_SORT_ALPHA_DIR_LAST)
			     ? compare_folders_alpha_dir
			     : compare_folders_alpha);

    folder_insert_index(folder, index, flist);
    return(index);
}


/*
 * folder_insert_sorted - Insert given folder struct into given list
 *			  observing sorting specified by given
 *			  comparison function
 */
int
folder_insert_sorted(index, min_index, max_index, folder, flist, compf)
    int	       index, min_index, max_index;
    FOLDER_S  *folder;
    void      *flist;
    int	     (*compf) PROTO((FOLDER_S *, FOLDER_S *));
{
    int	      i;

    return(((i = (*compf)(folder_entry(index, flist), folder)) == 0)
	     ? index
	     : (i < 0)
		? ((++index >= max_index)
		    ? max_index
		    : ((*compf)(folder_entry(index, flist), folder) > 0)
		       ? index
		       : folder_insert_sorted((index + max_index) / 2, index,
					      max_index, folder, flist, compf))
		: ((index <= min_index)
		    ? min_index
		    : folder_insert_sorted((min_index + index) / 2, min_index, index,
					   folder, flist, compf)));
}


/* 
 * folder_insert_index - Insert the given folder struct into the global list
 *                 at the given index.
 */
void
folder_insert_index(folder, index, flist)
    FOLDER_S *folder;
    int       index;
    void     *flist;
{
#ifdef	DOSXXX
    FOLDER *tmp;

    tmp = (FOLDER_S *)fs_get(sizeof(FOLDER_S) + (MAXFOLDER * sizeof(char)));


#else
    register FOLDER_S **flp, **iflp;

    /* if index is beyond size, place at end of list */
    index = min(index, ((FLIST *)flist)->used);

    /* grow array ? */
    if(((FLIST *)flist)->used + 1 > ((FLIST *)flist)->allocated){
	((FLIST *)flist)->allocated += FCHUNK;
	fs_resize((void **)&(((FLIST *)flist)->folders),
		  (((FLIST *)flist)->allocated) * sizeof(FOLDER_S *));
    }

    /* shift array left */
    iflp = &(((FOLDER_S **)((FLIST *)flist)->folders)[index]);
    for(flp = &(((FOLDER_S **)((FLIST *)flist)->folders)[((FLIST *)flist)->used]); 
	flp > iflp; flp--)
      flp[0] = flp[-1];

    ((FLIST *)flist)->folders[index] = folder;
    ((FLIST *)flist)->used          += 1;
#endif
}


/*----------------------------------------------------------------------
    Removes a folder at the given index in the given context's
    list.

Args: index -- Index in folder list of folder to be removed
      flist -- folder list
 ----*/
void
folder_delete(index, flist)
    int   index;
    void *flist;
{
    register int  i;
    FOLDER_S     *f;

    if(((FLIST *)flist)->used 
       && (index < 0 || index >= ((FLIST *)flist)->used))
      return;				/* bogus call! */

    if((f = folder_entry(index, flist))->nickname)
      fs_give((void **)&(f->nickname));
      
#ifdef	DOSXXX
    /* shift all entries after index up one.... */
#else
    fs_give((void **)&(((FLIST *)flist)->folders[index]));
    for(i = index; i < ((FLIST *)flist)->used - 1; i++)
      ((FLIST *)flist)->folders[i] = ((FLIST *)flist)->folders[i+1];


    ((FLIST *)flist)->used -= 1;
#endif
}



/*----------------------------------------------------------------------
      Shuffle order of incoming folders
  ----*/
int
shuffle_incoming_folders(context, index)
    CONTEXT_S *context;
    int index;
{
    int       tot, i, deefault, rv, inheriting = 0;
    int       new_index, index_within_var, new_index_within_var;
    int       readonly = 0;
    char      tmp[200];
    HelpType  help;
    ESCKEY_S  opts[3];
    char   ***alval;
    EditWhich ew;
    FOLDER_S *fp;
    PINERC_S *prc = NULL;

    dprint(4, (debugfile, "shuffle_incoming_folders\n"));

    if(!(context->use & CNTXT_INCMNG) ||
       (tot = folder_total(FOLDERS(context))) < 2 ||
       index < 0 || index >= tot)
      return(0);
    
    if(index == 0){
	q_status_message(SM_ORDER,0,3, "Cannot shuffle INBOX");
	return(0);
    }

    fp = folder_entry(index, FOLDERS(context));
    ew = config_containing_inc_fldr(fp);

    if(ps_global->restricted)
      readonly = 1;
    else{
	switch(ew){
	  case Main:
	    prc = ps_global->prc;
	    break;
	  case Post:
	    prc = ps_global->post_prc;
	    break;
	  case None:
	    break;
	}

	readonly = prc ? prc->readonly : 1;
    }

    if(prc && prc->quit_to_edit){
	quit_to_edit_msg(prc);
	return(0);
    }

    if(readonly){
	q_status_message(SM_ORDER,3,5,
	    "Shuffle cancelled: config file not editable");
	return(0);
    }

    alval = ALVAL(&ps_global->vars[V_INCOMING_FOLDERS], ew);

    if(!(alval && *alval))
      return(0);
    
    i = 0;
    opts[i].ch      = 'b';
    opts[i].rval    = 'b';
    opts[i].name    = "B";
    opts[i++].label = "Back";

    opts[i].ch      = 'f';
    opts[i].rval    = 'f';
    opts[i].name    = "F";
    opts[i++].label = "Forward";

    opts[i].ch = -1;
    deefault = 'b';

    /* find where this entry is in the particular config list */
    index_within_var = -1;
    for(i = 0; (*alval)[i]; i++){
	expand_variables(tmp_20k_buf, SIZEOF_20KBUF, (*alval)[i], 0);
	if(i == 0 && !strcmp(tmp_20k_buf, INHERIT))
	  inheriting = 1;
	else if(fp->varhash == line_hash(tmp_20k_buf)){
	    index_within_var = i;
	    break;
	}
    }

    if(index_within_var == -1){			/* didn't find it */
	q_status_message(SM_ORDER,3,5,
	    "Shuffle cancelled: unexpected trouble shuffling");
	return(0);
    }

    if(index_within_var == 0 || inheriting && index_within_var == 1){
	opts[0].ch = -2;			/* no back */
	deefault = 'f';
    }

    if(!(*alval)[i+1])				/* no forward */
      opts[1].ch = -2;
    
    if(opts[0].ch == -2 && opts[1].ch == -2){
	q_status_message(SM_ORDER, 0, 4,
			 "Cannot shuffle from one config file to another.");
	return(0);
    }

    sprintf(tmp, "Shuffle \"%.100s\" %s%s%s ? ",
	    FLDR_NAME(folder_entry(index, FOLDERS(context))),
	    (opts[0].ch != -2) ? "BACK" : "",
	    (opts[0].ch != -2 && opts[1].ch != -2) ? " or " : "",
	    (opts[1].ch != -2) ? "FORWARD" : "");
    help = (opts[0].ch == -2) ? h_incoming_shuf_down
			      : (opts[1].ch == -2) ? h_incoming_shuf_up
						   : h_incoming_shuf;

    rv = radio_buttons(tmp, -FOOTER_ROWS(ps_global), opts, deefault, 'x',
		       help, RB_NORM, NULL);

    new_index = index;
    new_index_within_var = index_within_var;

    switch(rv){
      case 'x':
	cmd_cancelled("Shuffle");
	return(0);

      case 'b':
	new_index_within_var--;
	new_index--;
	break;

      case 'f':
	new_index_within_var++;
	new_index++;
	break;
    }

    if(swap_incoming_folders(index, new_index, FOLDERS(context))){
	char    *stmp;

	/* swap them in the config variable, too  */
	stmp = (*alval)[index_within_var];
	(*alval)[index_within_var] = (*alval)[new_index_within_var];
	(*alval)[new_index_within_var] = stmp;

	set_current_val(&ps_global->vars[V_INCOMING_FOLDERS], TRUE, FALSE);
	write_pinerc(ps_global, ew, WRP_NONE);

	return(1);
    }
    else
      return(0);
}


int
swap_incoming_folders(index1, index2, flist)
    int    index1, index2;
    void  *flist;
{
    FOLDER_S *ftmp;

    if(!flist)
      return(0);

    if(index1 == index2)
      return(1);
    
    if(index1 < 0 || index1 >= ((FLIST *)flist)->used){
	dprint(1, (debugfile, "Error in swap_incoming_folders: index1=%d, used=%d\n", index1, ((FLIST *)flist)->used));
	return(0);
    }

    if(index2 < 0 || index2 >= ((FLIST *)flist)->used){
	dprint(1, (debugfile, "Error in swap_incoming_folders: index2=%d, used=%d\n", index2, ((FLIST *)flist)->used));
	return(0);
    }

    ftmp = ((FLIST *)flist)->folders[index1];
    ((FLIST *)flist)->folders[index1] = ((FLIST *)flist)->folders[index2];
    ((FLIST *)flist)->folders[index2] = ftmp;

    return(1);
}



/*----------------------------------------------------------------------
    Find an entry in the folder list by matching names
  ----*/
search_folder_list(list, name)
     void *list;
     char *name;
{
    int i;
    char *n;

    for(i = 0; i < folder_total(list); i++) {
        n = folder_entry(i, list)->name;
        if(strucmp(name, n) == 0)
          return(1); /* Found it */
    }
    return(0);
}



static CONTEXT_S *post_cntxt = NULL;

/*----------------------------------------------------------------------
    Verify and canonicalize news groups names. 
    Called from the message composer

Args:  given_group    -- List of groups typed by user
       expanded_group -- pointer to point to expanded list, which will be
			 allocated here and freed in caller.  If this is
			 NULL, don't attempt to validate.
       error          -- pointer to store error message
       fcc            -- pointer to point to fcc, which will be
			 allocated here and freed in caller

Returns:  0 if all is OK
         -1 if addresses weren't valid

Test the given list of newstroups against those recognized by our nntp
servers.  Testing by actually trying to open the list is much cheaper, both
in bandwidth and memory, than yanking the whole list across the wire.
  ----*/
int
news_build(given_group, expanded_group, error, fcc, mangled)
    char	 *given_group,
		**expanded_group,
		**error;
    BUILDER_ARG	 *fcc;
    int		 *mangled;
{
    char	 ng_error[90], *p1, *p2, *name, *end, *ep, **server,
		 ng_ref[MAILTMPLEN];
    int          expanded_len = 0, num_in_error = 0, cnt_errs, we_cancel = 0;

    MAILSTREAM  *stream = NULL;
    struct ng_list {
	char  *groupname;
	NgCacheReturns  found;
	struct ng_list *next;
    }*nglist = NULL, **ntmpp, *ntmp;
#ifdef SENDNEWS
    static int no_servers = 0;
#endif

    clear_cursor_pos();

    dprint(5, (debugfile,
	"- news_build - (%s)\n", given_group ? given_group : "nul"));

    if(error)
      *error = NULL;

    ng_ref[0] = '\0';

    /*------ parse given entries into a list ----*/
    ntmpp = &nglist;
    for(name = given_group; *name; name = end){

	/* find start of next group name */
        while(*name && (isspace((unsigned char)*name) || *name == ','))
	  name++;

	/* find end of group name */
	end = name;
	while(*end && !isspace((unsigned char)*end) && *end != ',')
	  end++;

        if(end != name){
	    *ntmpp = (struct ng_list *)fs_get(sizeof(struct ng_list));
	    (*ntmpp)->next      = NULL;
	    (*ntmpp)->found     = NotChecked;
            (*ntmpp)->groupname = fs_get(end - name + 1);
            strncpy((*ntmpp)->groupname, name, end - name);
            (*ntmpp)->groupname[end - name] = '\0';
	    ntmpp = &(*ntmpp)->next;
	    if(!expanded_group)
	      break;  /* no need to continue if just doing fcc */
        }
    }

    /*
     * If fcc is not set or is set to default, then replace it if
     * one of the recipient rules is in effect.
     */
    if(fcc){
	if((ps_global->fcc_rule == FCC_RULE_RECIP ||
	    ps_global->fcc_rule == FCC_RULE_NICK_RECIP) &&
	       (nglist && nglist->groupname)){
	  if(fcc->tptr)
	    fs_give((void **)&fcc->tptr);

	  fcc->tptr = cpystr(nglist->groupname);
	}
	else if(!fcc->tptr) /* already default otherwise */
	  fcc->tptr = cpystr(ps_global->VAR_DEFAULT_FCC);
    }

    if(!nglist){
	if(expanded_group)
	  *expanded_group = cpystr("");
        return 0;
    }

    if(!expanded_group)
      return 0;

#ifdef	DEBUG
    for(ntmp = nglist; debug >= 9 && ntmp; ntmp = ntmp->next)
      dprint(9, (debugfile, "Parsed group: --[%s]--\n",
	     ntmp->groupname ? ntmp->groupname : "?"));
#endif

    /* If we are doing validation */
    if(F_OFF(F_NO_NEWS_VALIDATION, ps_global)){
	int need_to_talk_to_server = 0;

	/*
	 * First check our cache of validated newsgroups to see if we even
	 * have to open a stream.
	 */
	for(ntmp = nglist; ntmp; ntmp = ntmp->next){
	    ntmp->found = chk_newsgrp_cache(ntmp->groupname);
	    if(ntmp->found == NotInCache)
	      need_to_talk_to_server++;
	}

	if(need_to_talk_to_server){

#ifdef SENDNEWS
	  if(no_servers == 0)
#endif
	    we_cancel = busy_alarm(1, "Validating newsgroup(s)", NULL, 1);
	  
	    /*
	     * Build a stream to the first server that'll talk to us...
	     */
	    for(server = ps_global->VAR_NNTP_SERVER;
		server && *server && **server;
		server++){
		sprintf(ng_ref, "{%.*s/nntp}#news.",
			sizeof(ng_ref)-30, *server);
		if(stream = pine_mail_open(stream, ng_ref,
					   OP_HALFOPEN|SP_USEPOOL|SP_TEMPUSE,
					   NULL))
		  break;
	    }
	    if(!server || !stream){
		if(error)
#ifdef SENDNEWS
		{
		 /* don't say this over and over */
		 if(no_servers == 0){
		    if(!server || !*server || !**server)
		      no_servers++;

		    *error = cpystr(no_servers
			    ? "Can't validate groups.  No servers defined"
			    : "Can't validate groups.  No servers responding");
		 }
		}
#else
		  *error = cpystr((!server || !*server || !**server)
			    ? "No servers defined for posting to newsgroups"
			    : "Can't validate groups.  No servers responding");
#endif
		*expanded_group = cpystr(given_group);
		goto done;
	    }
	}

	/*
	 * Now, go thru the list, making sure we can at least open each one...
	 */
	for(server = ps_global->VAR_NNTP_SERVER;
	    server && *server && **server; server++){
	    /*
	     * It's faster and easier right now just to open the stream and
	     * do our own finds than to use the current folder_exists()
	     * interface...
	     */
	    for(ntmp = nglist; ntmp; ntmp = ntmp->next){
	        if(ntmp->found == NotInCache){
		  sprintf(ng_ref, "{%.*s/nntp}#news.%.*s", 
			  sizeof(ng_ref)/2 - 10, *server,
			  sizeof(ng_ref)/2 - 10, ntmp->groupname);
		  ps_global->noshow_error = 1;
		  stream = pine_mail_open(stream, ng_ref,
					  OP_SILENT|SP_USEPOOL|SP_TEMPUSE,
					  NULL);
		  ps_global->noshow_error = 0;
		  if(stream)
		    add_newsgrp_cache(ntmp->groupname, ntmp->found = Found);
		}

	    }

	    if(stream){
		pine_mail_close(stream);
		stream = NULL;
	    }

	}

    }

    /* figure length of string for matching groups */
    for(ntmp = nglist; ntmp; ntmp = ntmp->next){
      if(ntmp->found == Found || F_ON(F_NO_NEWS_VALIDATION, ps_global))
	expanded_len += strlen(ntmp->groupname) + 2;
      else{
	num_in_error++;
	if(ntmp->found == NotInCache)
	  add_newsgrp_cache(ntmp->groupname, ntmp->found = Missing);
      }
    }

    /*
     * allocate and write the allowed, and error lists...
     */
    p1 = *expanded_group = fs_get((expanded_len + 1) * sizeof(char));
    if(error && num_in_error){
	cnt_errs = num_in_error;
	memset((void *)ng_error, 0, sizeof(ng_error));
	sprintf(ng_error, "Unknown news group%s: ", plural(num_in_error));
	ep = ng_error + strlen(ng_error);
    }
    for(ntmp = nglist; ntmp; ntmp = ntmp->next){
	p2 = ntmp->groupname;
	if(ntmp->found == Found || F_ON(F_NO_NEWS_VALIDATION, ps_global)){
	    while(*p2)
	      *p1++ = *p2++;

	    if(ntmp->next){
		*p1++ = ',';
		*p1++ = ' ';
	    }
	}
	else if (error){
	    while(*p2 && (ep - ng_error < sizeof(ng_error)-1))
	      *ep++ = *p2++;

	    if(--cnt_errs > 0 && (ep - ng_error < sizeof(ng_error)-3)){
		strcpy(ep, ", ");
		ep += 2;
	    }
	}
    }

    *p1 = '\0';

    if(error && num_in_error)
      *error = cpystr(ng_error);

done:
    while(ntmp = nglist){
	nglist = nglist->next;
	fs_give((void **)&ntmp->groupname);
	fs_give((void **)&ntmp);
    }

    if(we_cancel){
	cancel_busy_alarm(0);
	mark_status_dirty();
	display_message('x');
	if(mangled)
	  *mangled |= BUILDER_MESSAGE_DISPLAYED;
    }

    return(num_in_error ? -1 : 0);
}


typedef struct ng_cache {
    char          *name;
    NgCacheReturns val;
}NgCache;
static NgCache *ng_cache_ptr;
#if defined(DOS) && !defined(_WINDOWS)
#define MAX_NGCACHE_ENTRIES 15
#else
#define MAX_NGCACHE_ENTRIES 40
#endif
/*
 * Simple newsgroup validity cache.  Opening a newsgroup to see if it
 * exists can be very slow on a heavily loaded NNTP server, so we cache
 * the results.
 */
NgCacheReturns
chk_newsgrp_cache(group)
char *group;
{
    register NgCache *ngp;
    
    for(ngp = ng_cache_ptr; ngp && ngp->name; ngp++){
	if(strcmp(group, ngp->name) == 0)
	  return(ngp->val);
    }

    return NotInCache;
}


/*
 * Add an entry to the newsgroup validity cache.
 *
 * LRU entry is the one on the bottom, oldest on the top.
 * A slot has an entry in it if name is not NULL.
 */
void
add_newsgrp_cache(group, result)
char *group;
NgCacheReturns result;
{
    register NgCache *ngp;
    NgCache save_ngp;

    /* first call, initialize cache */
    if(!ng_cache_ptr){
	int i;

	ng_cache_ptr =
	    (NgCache *)fs_get((MAX_NGCACHE_ENTRIES+1)*sizeof(NgCache));
	for(i = 0; i <= MAX_NGCACHE_ENTRIES; i++){
	    ng_cache_ptr[i].name = NULL;
	    ng_cache_ptr[i].val  = NotInCache;
	}
	ng_cache_ptr[MAX_NGCACHE_ENTRIES].val  = End;
    }

    if(chk_newsgrp_cache(group) == NotInCache){
	/* find first empty slot or End */
	for(ngp = ng_cache_ptr; ngp->name; ngp++)
	  ;/* do nothing */
	if(ngp->val == End){
	    /*
	     * Cache is full, throw away top entry, move everything up,
	     * and put new entry on the bottom.
	     */
	    ngp = ng_cache_ptr;
	    if(ngp->name) /* just making sure */
	      fs_give((void **)&ngp->name);

	    for(; (ngp+1)->name; ngp++){
		ngp->name = (ngp+1)->name;
		ngp->val  = (ngp+1)->val;
	    }
	}
	ngp->name = cpystr(group);
	ngp->val  = result;
    }
    else{
	/*
	 * Move this entry from current location to last to preserve
	 * LRU order.
	 */
	for(ngp = ng_cache_ptr; ngp && ngp->name; ngp++){
	    if(strcmp(group, ngp->name) == 0) /* found it */
	      break;
	}
	save_ngp.name = ngp->name;
	save_ngp.val  = ngp->val;
	for(; (ngp+1)->name; ngp++){
	    ngp->name = (ngp+1)->name;
	    ngp->val  = (ngp+1)->val;
	}
	ngp->name = save_ngp.name;
	ngp->val  = save_ngp.val;
    }
}


void
free_newsgrp_cache()
{
    register NgCache *ngp;

    for(ngp = ng_cache_ptr; ngp && ngp->name; ngp++)
      fs_give((void **)&ngp->name);
    if(ng_cache_ptr)
      fs_give((void **)&ng_cache_ptr);
}


/*----------------------------------------------------------------------
    Browse list of newsgroups available for posting

  Called from composer when ^T is typed in newsgroups field

Args:    none

Returns: pointer to selected newsgroup, or NULL.
         Selector call in composer expects this to be alloc'd here.

  ----*/
char *
news_group_selector(error_mess)
    char **error_mess;
{
    CONTEXT_S *tc;
    char      *post_folder;
    int        rc;
    char      *em;

    /* Coming back from composer */
    fix_windsize(ps_global);
    init_sigwinch();

    post_folder = fs_get((size_t)MAILTMPLEN);

    /*--- build the post_cntxt -----*/
    em = get_post_list(ps_global->VAR_NNTP_SERVER);
    if(em != NULL){
        if(error_mess != NULL)
          *error_mess = cpystr(em);

	cancel_busy_alarm(-1);
        return(NULL);
    }

    /*----- Call the browser -------*/
    tc = post_cntxt;
    if(rc = folders_for_post(ps_global, &tc, post_folder))
      post_cntxt = tc;

    cancel_busy_alarm(-1);

    if(rc <= 0)
      return(NULL);

    return(post_folder);
}



/*----------------------------------------------------------------------
    Get the list of news groups that are possible for posting

Args: post_host -- host name for posting

Returns NULL if list is retrieved, pointer to error message if failed

This is kept in a standards "CONTEXT" for a acouple of reasons. First
it makes it very easy to use the folder browser to display the
newsgroup for selection on ^T from the composer. Second it will allow
the same mechanism to be used for all folder lists on memory tight
systems like DOS. The list is kept for the life of the session because
fetching it is a expensive. 

 ----*/
char *
get_post_list(post_host)
     char **post_host;
{
    char *post_context_string;

    if(!post_host || !post_host[0]) {
        /* BUG should assume inews and get this from active file */
        return("Can't post messages, NNTP server needs to be configured");
    }

    if(!post_cntxt){
	int we_cancel;

	we_cancel = busy_alarm(1, "Getting full list of groups for posting",
			       NULL, 0);

        post_context_string = fs_get(strlen(post_host[0]) + 20);
        sprintf(post_context_string, "{%s/nntp}#news.[]", post_host[0]);
        
        post_cntxt          = new_context(post_context_string, NULL);
        post_cntxt->use    |= CNTXT_FINDALL;
        post_cntxt->dir->status |= CNTXT_NOFIND; 
        post_cntxt->next    = NULL;

        build_folder_list(NULL, post_cntxt, NULL, NULL,
			  NEWS_TEST(post_cntxt) ? BFL_LSUB : BFL_NONE);
	if(we_cancel)
	  cancel_busy_alarm(-1);
    }
    return(NULL);
}


/*
 * Unlike mail_list, this version returns a value. The returned value is
 * TRUE if the stream is opened ok, and FALSE if we failed opening the
 * stream. This allows us to differentiate between a LIST which returns
 * no matches and a failure opening the stream. We do this by pre-opening
 * the stream ourselves.
 *
 * We should probably incorporate the FIX_BROKEN_LIST code into this function
 * instead of leaving it in mail_list_internal, but leave well enough alone.
 */
int
pine_mail_list(stream, ref, pat, options)
    MAILSTREAM *stream;
    char       *ref, *pat;
    unsigned   *options;
{
    int   we_open = 0;
    char *halfopen_target;
    char  source[MAILTMPLEN], *target = NULL;
    MAILSTREAM *ourstream = NULL;

    dprint(7, (debugfile, "pine_mail_list: ref=%s pat=%s%s\n", 
	       ref ? ref : "(NULL)",
	       pat ? pat : "(NULL)",
	       stream ? "" : " (stream was NULL)"));

    if(!ref && check_for_move_mbox(pat, source, sizeof(source), &target)
       ||
       check_for_move_mbox(ref, source, sizeof(source), &target)){
	ref = NIL;
	pat = target;
	if(options)
	  *options |= PML_IS_MOVE_MBOX;

	dprint(7, (debugfile,
		   "pine_mail_list: #move special case, listing \"%s\"%s\n", 
		   pat ? pat : "(NULL)",
		   stream ? "" : " (stream was NULL)"));
    }

    if(!stream && ((pat && *pat == '{') || (ref && *ref == '{'))){
	we_open++;
	if(pat && *pat == '{'){
	    ref = NIL;
	    halfopen_target = pat;
	}
	else
	  halfopen_target = ref;
    }

    if(we_open){
	long flags = (OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE);

	stream = sp_stream_get(halfopen_target, SP_MATCH);
	if(!stream)
	  stream = sp_stream_get(halfopen_target, SP_SAME);

	if(!stream){
	    DRIVER *d;

	    /*
	     * POP is a special case. We don't need to have a stream
	     * to call mail_list for a POP name. The else part is the
	     * POP part.
	     */
	    if((d = mail_valid(NIL, halfopen_target, (char *) NIL))
	       && (d->flags & DR_HALFOPEN)){
		stream = pine_mail_open(NIL, halfopen_target, flags, NULL);
		ourstream = stream;
		if(!stream)
		  return(FALSE);
	    }
	    else
	      stream = NULL;
	}

	mail_list(stream, ref, pat);
    }
    else
      mail_list(stream, ref, pat);
    
    if(ourstream)
      pine_mail_close(ourstream);

    return(TRUE);
}


/*
 * mail_list_internal -- A monument to software religion and those who
 *			 would force it upon us.
 */
#undef	mail_list
void
mail_list_internal(s, r, p)
    MAILSTREAM *s;
    char       *r, *p;
{
    if(F_ON(F_FIX_BROKEN_LIST, ps_global)
       && ((s && s->mailbox && *s->mailbox == '{')
	   || (!s && ((r && *r == '{') || (p && *p == '{'))))){
	char tmp[2*MAILTMPLEN];

	sprintf(tmp, "%.*s%.*s", sizeof(tmp)/2-1, r ? r : "",
		sizeof(tmp)/2-1, p);
	mail_list(s, "", tmp);
    }
    else
      mail_list(s, r, p);
}

int
percent_formatted()
{
  if (percent.total == 0)
    return percent.total;
  else
    return ((int)(100*percent.num_done/percent.total));
}

#ifdef	_WINDOWS
int
folder_list_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup fldr_popup[20];

    memset(fldr_popup, 0, 20 * sizeof(MPopup));
    fldr_popup[0].type = tTail;
    if(in_handle){
	int	  i, n = 0;
	HANDLE_S *h = get_handle(sparms->text.handles, in_handle);
	FOLDER_S *fp = (h) ? folder_entry(h->h.f.index,
					  FOLDERS(h->h.f.context))
			   : NULL;

	if((i = menu_binding_index(sparms->keys.menu, MC_CHOICE)) >= 0){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = (fp && fp->isdir)
					     ? "&View Directory"
					     : "&View Folder";
	}

	if((i = menu_binding_index(sparms->keys.menu, MC_SELCUR)) >= 0
	   && bitnset(i, sparms->keys.bitmap)){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = (fp && fp->isdir)
					     ? "&Select Directory"
					     : "&Select Folder";
	}

	if((i = menu_binding_index(sparms->keys.menu, MC_DELETE)) >= 0
	   && bitnset(i, sparms->keys.bitmap)){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = "Delete Folder";
	}

	if((i = menu_binding_index(sparms->keys.menu, MC_EXPORT)) >= 0
	   && bitnset(i, sparms->keys.bitmap)){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = "Export Folder";
	}

	if((i = menu_binding_index(sparms->keys.menu, MC_CHK_RECENT)) >= 0
	   && bitnset(i, sparms->keys.bitmap)){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = "Check New Messages";
	}

	if(n)
	  fldr_popup[n++].type = tSeparator;

	folder_popup_config(((FPROC_S *)sparms->proc.data.p)->fs,
			    sparms->keys.menu, &fldr_popup[n]);
    }
    else
      folder_popup_config(((FPROC_S *)sparms->proc.data.p)->fs,
			  sparms->keys.menu, fldr_popup);

    if(fldr_popup[0].type != tTail)
      mswin_popup(fldr_popup);

    return(0);
}



int
folder_list_select_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup fldr_popup[20];

    memset(fldr_popup, 0, 20 * sizeof(MPopup));
    fldr_popup[0].type = tTail;
    if(in_handle){
	int	  i, n = 0;
	HANDLE_S *h = get_handle(sparms->text.handles, in_handle);
	FOLDER_S *fp = (h) ? folder_entry(h->h.f.index,FOLDERS(h->h.f.context))
			   : NULL;

	if((i = menu_binding_index(sparms->keys.menu, MC_CHOICE)) >= 0){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = (fp && fp->isdir)
					     ? "&View Directory"
					     : "&Select";

	    fldr_popup[n++].type = tSeparator;
	}

	folder_popup_config(((FPROC_S *)sparms->proc.data.p)->fs,
			    sparms->keys.menu, &fldr_popup[n]);
    }
    else
      folder_popup_config(((FPROC_S *)sparms->proc.data.p)->fs,
			  sparms->keys.menu, fldr_popup);

    if(fldr_popup[0].type != tTail)
      mswin_popup(fldr_popup);

    return(0);
}



/*
 * Just a little something to simplify assignments
 */
#define	FLDRPOPUP(p, c, s)	{ \
				    (p)->type	      = tQueue; \
				    (p)->data.val     = c; \
				    (p)->label.style  = lNormal; \
				    (p)->label.string = s; \
				}


/*----------------------------------------------------------------------
  Popup Menu configurator
	     
 ----*/
void
folder_popup_config(fs, km, popup)
    FSTATE_S	    *fs;
    struct key_menu *km;
    MPopup	    *popup;
{
    int i;

    if((i = menu_binding_index(km, MC_PARENT)) >= 0){
	FLDRPOPUP(popup, km->keys[i].bind.ch[0], "Parent Directory");
	popup++;
    }

    if(fs->km == &folder_km){
	if((fs->context->next || fs->context->prev) && !fs->combined_view){
	    FLDRPOPUP(popup, '<', "Collection List");
	    popup++;
	}
    }
    else if((i = menu_binding_index(km, MC_COLLECTIONS)) >= 0){
	FLDRPOPUP(popup, km->keys[i].bind.ch[0], "Collection List");
	popup++;
    }

    if((i = menu_binding_index(km, MC_INDEX)) >= 0){
	FLDRPOPUP(popup, km->keys[i].bind.ch[0], "Current Folder Index");
	popup++;
    }

    if((i = menu_binding_index(km, MC_MAIN)) >= 0){
	FLDRPOPUP(popup, km->keys[i].bind.ch[0], "Main Menu");
	popup++;
    }
    
    popup->type = tTail;		/* tie off the array */
}
#endif	/* _WINDOWS */
