/*----------------------------------------------------------------------
  $Id: pine.h 14089 2005-09-16 00:39:42Z jpf@u.washington.edu $

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

     pine.h 

     Definitions here are fundamental to pine. 

    No changes should need to be made here to configure pine for one
  site or another.  That is, no changes for local preferences such as
  default directories and other parameters.  Changes might be needed here
  for porting pine to a new machine, but we hope not.

   Includes
     - Various convenience definitions and macros
     - macros for debug printfs
     - data structures used by Pine
     - declarations of all Pine functions

  ====*/


#ifndef _PINE_INCLUDED
#define _PINE_INCLUDED

#define PINE_VERSION		"4.64"
#define	PHONE_HOME_VERSION	"-count"
#define	PHONE_HOME_HOST		"docserver.cac.washington.edu"

#define UNKNOWN_CHARSET		"X-UNKNOWN"

#define OUR_HDRS_LIST		"X-Our-Headers"

#define HIDDEN_PREF		"Normally Hidden Preferences"

#define WYSE_PRINTER		"attached-to-wyse"

#define MAX_BM			80  /* max length of busy message */


#if defined(DOS) || defined(OS2)
#define	NEWLINE		"\r\n"		/* local EOL convention...  */
#else
#define	NEWLINE		"\n"		/* necessary for gf_* funcs */
#endif


#define	FEX_NOENT	0x0000		/* file_exists: doesn't exist	    */
#define	FEX_ISFILE	0x0001		/* file_exists: name is a file	    */
#define	FEX_ISDIR	0x0002		/* file_exists: name is a dir	    */
#define	FEX_ISMARKED	0x0004		/* file_exists: is interesting	    */
#define	FEX_UNMARKED	0x0008		/* file_exists: known UNinteresting */
#define	FEX_ERROR	0x1000		/* file_exists: error occured	    */

#define	BFL_NONE	0x00		/* build_folder_list: no flag */
#define	BFL_FLDRONLY	0x01		/* ignore directories	      */
#define	BFL_LSUB	0x02		/* use mail_lsub vs mail_list */
#define	BFL_SCAN	0x04		/* use mail_scan vs mail_list */

#define	FM_DISPLAY	0x01		/* flag for format_message */
#define	FM_NEW_MESS	0x02		/* ditto		   */
#define	FM_NOWRAP	0x04		/* ditto		   */
#define	FM_WRAPFLOWED	0x08		/* ditto		   */
#define	FM_NOCOLOR	0x10		/* ditto		   */
#define	FM_NOINDENT	0x20		/* no effect if NOWRAP set */

#define	AOS_NONE	0x00		/* alredy_open_stream: no flag  */
#define	AOS_RW_ONLY	0x01		/* don't match readonly streams */

#define	RS_NONE		0x00		/* no options set		*/
#define	RS_RULES	0x01		/* include Rules as option	*/
#define	RS_INCADDR	0x02		/* include Addrbook as option	*/
#define	RS_INCEXP	0x04		/* include Export as option	*/
#define	RS_INCFILTNOW	0x08		/* filter now			*/

#define SEQ_EXCEPTION   (-3)		/* returned when seq # change and no
					   on_ctrl_C available.          */
#define	RB_NORM		0x00		/* flags modifying radio_buttons */
#define	RB_ONE_TRY	0x01		/* one shot answer, else default */
#define	RB_FLUSH_IN	0x02		/* discard pending input	 */
#define	RB_NO_NEWMAIL	0x04		/* Quell new mail check		 */
#define	RB_SEQ_SENSITIVE 0x08		/* Sensitive to seq # changes    */
#define	RB_RET_HELP	0x10		/* Return when help key pressed  */

#define	WT_NORM		RB_NORM		/* flags modifying want_to       */
#define	WT_FLUSH_IN	RB_FLUSH_IN	/* discard pending input         */
#define	WT_SEQ_SENSITIVE RB_SEQ_SENSITIVE /* Sensitive to seq # changes  */

#define	EC_NONE		0x00		/* flags modifying expunge_and_close */
#define	EC_NO_CLOSE	0x01		/* don't close at end                */

#define	PL_NONE		0x00		/* flags modifying parse_list    */
#define	PL_REMSURRQUOT	0x01		/* rm surrounding quotes         */
#define	PL_COMMAQUOTE	0x02		/* backslash quotes comma        */

#define	AC_NONE		0x00		/* flags modifying apply_command */
#define	AC_FROM_THREAD	0x01		/* called from thread_command    */
#define	AC_COLL		0x02		/* offer collapse command        */
#define	AC_EXPN		0x04		/* offer expand command          */
#define	AC_UNSEL	0x08		/* all selected, offer UnSelect  */

#define	FN_NONE		0x00		/* flags modifying folder_is_nick */
#define	FN_WHOLE_NAME	0x01		/* return long name if #move      */

#define	GF_NOOP		0x01		/* flags used by generalized */
#define GF_EOD		0x02		/* filters                   */
#define GF_DATA		0x04		/* See filter.c for more     */
#define GF_ERROR	0x08		/* details                   */
#define GF_RESET	0x10

#define	DA_SAVE		0x01		/* flags used by display_attachment */
#define	DA_FROM_VIEW	0x02		/* see mailpart.c		    */
#define	DA_RESIZE	0x04
#define DA_DIDPROMPT    0x08            /* Already prompted to view att     */

#define OE_NONE			0x00	/* optionally_enter flags	*/
#define OE_DISALLOW_CANCEL	0x01	/* Turn off Cancel menu item	*/
#define	OE_DISALLOW_HELP	0x02	/* Turn off Help menu item	*/
#define OE_USER_MODIFIED	0x08	/* set on return if user input	*/
#define OE_KEEP_TRAILING_SPACE  0x10	/* Allow trailing white-space	*/
#define OE_SEQ_SENSITIVE  	0x20	/* Sensitive to seq # changes   */
#define OE_APPEND_CURRENT  	0x40	/* append, don't truncate       */
#define OE_PASSWD	  	0x80	/* Don't echo user input        */

#define SS_PROMPTFORTO		0x01	/* Simple_send: prompt for To	*/
#define	SS_NULLRP		0x02	/* Null Return-path		*/

#define GE_NONE			0x00	/* get_export_filename flags    */
#define GE_IS_EXPORT		0x01	/* include EXPORT: in prompt    */
#define GE_SEQ_SENSITIVE	0x02	/* Sensitive to seq # changes   */
#define GE_NO_APPEND		0x04	/* No appending to file allowed */
#define GE_IS_IMPORT		0x08	/* No writing of file           */
#define GE_ALLPARTS		0x10	/* Add AllParts toggle to options */

#define GER_NONE		0x00	/* get_export_filename return flags */
#define GER_OVER		0x01	/* overwrite of existing file       */
#define GER_APPEND		0x02	/* append of existing file          */
#define GER_ALLPARTS		0x04	/* AllParts toggle is on            */

#define GFHP_STRIPPED		0x01
#define GFHP_HANDLES		0x02
#define GFHP_LOCAL_HANDLES	0x04

#define	GFW_HANDLES		0x01
#define	GFW_ONCOMMA		0x02
#define	GFW_FLOWED		0x04
#define	GFW_DELSP		0x08
#define GFW_FLOW_RESULT         0x10
#define GFW_USECOLOR            0x20

#define	PML_IS_MOVE_MBOX	0x01

/* next_sorted_folder options */
#define	NSF_TRUST_FLAGS	0x01	/* input flag, don't need to ping           */
#define	NSF_SKIP_CHID	0x02	/* input flag, skip MN_CHID messages        */
#define	NSF_SEARCH_BACK	0x04	/* input flag, search backward if none forw */
#define	NSF_FLAG_MATCH	0x08	/* return flag, actually got a match        */

/* first_sorted_folder options */
#define	FSF_LAST	0x01	/* last instead of first  */
#define	FSF_SKIP_CHID	0x02	/* skip MN_CHID messages  */

#define	FI_FOLDER		0x01
#define	FI_DIR			0x02
#define	FI_RENAME		0x04
#define	FI_ANY			(FI_FOLDER | FI_DIR)

#define FOR_PATTERN	0x01

#define REMOTE_DATA_TYPE	TYPETEXT
#define REMOTE_DATA_VERS_NUM	1
#define REMOTE_ABOOK_SUBTYPE	"x-pine-addrbook"
#define REMOTE_PINERC_SUBTYPE	"x-pine-pinerc"
#define REMOTE_SIG_SUBTYPE	"x-pine-sig"

#define PMAGIC                   "P#*E@"
#define METAFILE_VERSION_NUM     "01"
#define SIZEOF_PMAGIC       (5)
#define SIZEOF_SPACE        (1)
#define SIZEOF_VERSION_NUM  (2)
#define TO_FIND_HDR_PMAGIC  (0)
#define TO_FIND_VERSION_NUM (TO_FIND_HDR_PMAGIC + SIZEOF_PMAGIC + SIZEOF_SPACE)

typedef enum {ReadOnly, ReadWrite, MaybeRorW, NoAccess, NoExists} AccessType;

#ifndef DF_REMOTE_ABOOK_VALIDITY
#define DF_REMOTE_ABOOK_VALIDITY "5"
#endif
#ifndef DF_GOTO_DEFAULT_RULE
#define DF_GOTO_DEFAULT_RULE	"inbox-or-folder-in-recent-collection"
#endif
#ifndef DF_INCOMING_STARTUP
#define DF_INCOMING_STARTUP	"first-unseen"
#endif
#ifndef DF_PRUNING_RULE
#define DF_PRUNING_RULE		"ask-ask"
#endif
#ifndef DF_REOPEN_RULE
#define DF_REOPEN_RULE		"ask-no-n"
#endif
#ifndef DF_THREAD_DISP_STYLE
#define DF_THREAD_DISP_STYLE	"struct"
#endif
#ifndef DF_THREAD_INDEX_STYLE
#define DF_THREAD_INDEX_STYLE	"exp"
#endif
#ifndef DF_THREAD_MORE_CHAR
#define DF_THREAD_MORE_CHAR	">"
#endif
#ifndef DF_THREAD_EXP_CHAR
#define DF_THREAD_EXP_CHAR	"."
#endif
#ifndef DF_THREAD_LASTREPLY_CHAR
#define DF_THREAD_LASTREPLY_CHAR "|"
#endif
#ifndef DF_MAILDROPCHECK
#define DF_MAILDROPCHECK 	"60"
#endif
#ifndef DF_MAXREMSTREAM
#define DF_MAXREMSTREAM 	"2"
#endif
#ifndef DF_VIEW_MARGIN_RIGHT
#define DF_VIEW_MARGIN_RIGHT 	"4"
#endif
#ifndef DF_QUOTE_SUPPRESSION
#define DF_QUOTE_SUPPRESSION 	"0"
#endif
#ifndef DF_DEADLETS
#define DF_DEADLETS 		"1"
#endif
#ifndef DF_NMW_WIDTH
#define DF_NMW_WIDTH 		"80"
#endif

#define SIGDASHES	"-- "
#define START_SIG_BLOCK	2
#define IN_SIG_BLOCK	1
#define OUT_SIG_BLOCK	0


/*
 * This is a little dangerous. We're passing flags to match_pattern and
 * peeling some of them off for our own use while passing the rest on
 * to mail_search_full. So we need to define ours so they don't overlap
 * with the c-client flags that can be passed to mail_search_full.
 * We could formalize it with mrc.
 */
#define MP_IN_CCLIENT_CB	0x10000   /* we're in a c-client callback! */
#define MP_NOT			0x20000   /* use ! of patgrp for search    */


/*
 * Line_hash can't return LINE_HASH_N, so we use it in a couple places as
 * a special value that we can assign knowing it can't be a real hash value.
 */
#define LINE_HASH_N	0xffffffff


/*
 * Size of generic filter's input/output queue
 */
#define	GF_MAXBUF		256


#define KEY_HELP_LEN   (2 * MAX_SCREEN_COLS)
                          /*When saving escape sequences, etc length of string
                                for the usual key menu at the bottom. used with
                                begin_output_store(), end_output_store() */


#undef  min
#define min(a,b)	((a) < (b) ? (a) : (b))
#undef  max
#define max(a,b)	((a) > (b) ? (a) : (b))
#define plural(n)	((n) == 1 ? "" : "s")

/* max length prompt for optionally_enter and want_to */
#define MAXPROMPT	(ps_global->ttyo->screen_cols - 6)
typedef enum {FrontDots, MidDots, EndDots} WhereDots;

typedef enum {ListMode, SingleMode} ScreenMode;

typedef enum {DontAsk, NoDel, Del, RetNoDel, RetDel} SaveDel;

typedef enum {NoKW, KW, KWInit} SubjKW;

typedef struct offsets {
    int        offset;		/* the offset into the line */
    int        len;		/* the length of the colored section */
    COLOR_PAIR color;		/* color for this offset    */
} OFFCOLOR_S;

/*
 * Macro to simplify clearing body portion of pine's display
 */
#define ClearBody()	ClearLines(1, ps_global->ttyo->screen_rows 	      \
						  - FOOTER_ROWS(ps_global) - 1)

/*
 * Macro to reveal horizontal scroll margin.  It can be no greater than
 * half the number of lines on the display...
 */
#define	HS_MAX_MARGIN(p)	(((p)->ttyo->screen_rows-FOOTER_ROWS(p)-3)/2)
#define	HS_MARGIN(p)		min((p)->scroll_margin, HS_MAX_MARGIN(p))

/*
 * Helpful for finding the beginning or ending escape sequence for ISO-2022-JP.
 */
#define JP_START(line,length)	(((length) > 1) && (line) &&		\
				 ((line)[0] == '$') &&			\
				 (((line)[1] == 'B') ||			\
				  ((line)[1] == '@'))) 
#define JP_END(line,length)	(((length) > 1) && (line) &&		\
				 ((line)[0] == '(') &&			\
				 (((line)[1] == 'B') ||			\
				  ((line)[1] == 'H') ||			\
				  ((line)[1] == 'J'))) 


/*
 * Macros to support anything you'd ever want to do with a message
 * number...
 */
#define	mn_init(P, m)		msgno_init((P), (m))

#define	mn_get_cur(p)		(((p) && (p)->select) 			      \
				  ? (p)->select[(p)->sel_cur] : -1)

#define	mn_set_cur(p, m)	do{					      \
				  if(p){				      \
				    (p)->select[(p)->sel_cur] = (m);	      \
				  }					      \
				}while(0)

#define	mn_inc_cur(s, p, f)	msgno_inc(s, p, f)

#define	mn_dec_cur(s, p, f)	msgno_dec(s, p, f)

#define	mn_add_cur(p, m)	do{					      \
				  if(p){				      \
				      if((p)->sel_cnt+1L > (p)->sel_size){    \
					  (p)->sel_size += 10L;		      \
					  fs_resize((void **)&((p)->select),  \
						    (size_t)(p)->sel_size     \
						             * sizeof(long)); \
				      }					      \
				      (p)->select[((p)->sel_cnt)++] = (m);    \
				  }					      \
				}while(0)

#define	mn_total_cur(p)		((p) ? (p)->sel_cnt : 0L)

#define	mn_first_cur(p)		(((p) && (p)->sel_cnt > 0L)		      \
				  ? (p)->select[(p)->sel_cur = 0] : 0L)

#define	mn_next_cur(p)		(((p) && ((p)->sel_cur + 1) < (p)->sel_cnt)   \
				  ? (p)->select[++((p)->sel_cur)] : -1L)

#define	mn_is_cur(p, m)		msgno_in_select((p), (m))

#define	mn_reset_cur(p, m)	do{					      \
				  if(p){				      \
				      (p)->sel_cur  = 0L;		      \
				      (p)->sel_cnt  = 1L;		      \
				      (p)->sel_size = 8L;		      \
				      fs_resize((void **)&((p)->select),      \
					(size_t)(p)->sel_size * sizeof(long));\
				      (p)->select[0] = (m);		      \
				  }					      \
			        }while(0)

#define	mn_m2raw(p, m)		(((p) && (p)->sort && (m) > 0 		      \
				  && (m) <= mn_get_total(p)) 		      \
				   ? (p)->sort[m] : 0L)

#define	mn_raw2m(p, m)		(((p) && (p)->isort && (m) > 0 		      \
				  && (m) <= mn_get_nmsgs(p)) 		      \
				   ? (p)->isort[m] : 0L)

#define	mn_get_total(p)		((p) ? (p)->max_msgno : 0L)

#define	mn_set_total(p, m)	do{ if(p) (p)->max_msgno = (m); }while(0)

#define	mn_get_nmsgs(p)		((p) ? (p)->nmsgs : 0L)

#define	mn_set_nmsgs(p, m)	do{ if(p) (p)->nmsgs = (m); }while(0)

#define	mn_add_raw(p, m)	msgno_add_raw((p), (m))

#define	mn_flush_raw(p, m)	msgno_flush_raw((p), (m))

#define	mn_get_sort(p)		((p) ? (p)->sort_order : SortArrival)

#define	mn_set_sort(p, t)	msgno_set_sort((p), (t))

#define	mn_get_revsort(p)	((p) ? (p)->reverse_sort : 0)

#define	mn_set_revsort(p, t)	do{					      \
				  if(p)					      \
				    (p)->reverse_sort = (t);		      \
				}while(0)

#define	mn_get_mansort(p)	((p) ? (p)->manual_sort : 0)

#define	mn_set_mansort(p, t)	do{					      \
				  if(p)					      \
				    (p)->manual_sort = (t);		      \
				}while(0)

#define	mn_give(P)		msgno_give(P)


/*
 * This searchs for lines beginning with From<space> so that we can QP-encode
 * them.  It also searches for lines consisting of only a dot.  Some mailers
 * will mangle these lines.  The reason it is ifdef'd is because most people
 * seem to prefer the >From style escape provided by a lot of mail software
 * to the QP-encoding.
 * Froms, dots, bmap, and dmap may be any integer type.  C is the next char.
 * bmap and dmap should be initialized to 1.
 * froms is incremented by 1 whenever a line beginning From_ is found.
 * dots is incremented by 1 whenever a line with only a dot is found.
 */
#define Find_Froms(froms,dots,bmap,dmap,c) { int x,y;			\
				switch (c) {				\
				  case '\n': case '\r':			\
				    x = 0x1;				\
				    y = 0x7;				\
				    bmap = 0;				\
				    break;				\
				  case 'F':				\
				    x = 0x3;				\
				    y = 0;				\
				    break;				\
				  case 'r':				\
				    x = 0x7;				\
				    y = 0;				\
				    break;				\
				  case 'o':				\
				    x = 0xf;				\
				    y = 0;				\
				    break;				\
				  case 'm':				\
				    x = 0x1f;				\
				    y = 0;				\
				    break;				\
				  case ' ':				\
				    x = 0x3f;				\
				    y = 0;				\
				    break;				\
				  case '.':				\
				    x = 0;				\
				    y = 0x3;				\
				    break;				\
				  default:				\
				    x = 0;				\
				    y = 0;				\
				    break;				\
				}					\
				bmap = ((x >> 1) == bmap) ? x : 0;	\
				froms += (bmap == 0x3f ? 1 : 0);	\
				if(y == 0x7 && dmap != 0x3){		\
				    y = 0x1;				\
				    dmap = 0;				\
				}					\
				dmap = ((y >> 1) == dmap) ? y : 0;	\
				dots += (dmap == 0x7 ? 1 : 0);		\
			     }



/*
 * Useful macro to test if current folder is a bboard type (meaning
 * netnews for now) collection...
 */
#define	IS_NEWS(S)	((S) ? ns_test((S)->mailbox, "news") : 0)

#define	READONLY_FOLDER(S)  ((S) && (S)->rdonly && !IS_NEWS(S))

#define STREAMNAME(S)	(((S) && sp_fldr(S) && sp_fldr(S)[0])        \
			  ? sp_fldr(S)                               \
			  : ((S) && (S)->mailbox && (S)->mailbox[0]) \
			    ? (S)->mailbox                           \
			    : "?")


/*
 * Simple, handy macro to determine if folder name is remote 
 * (on an imap server)
 */
#define	IS_REMOTE(X)	(*(X) == '{' && *((X) + 1) && *((X) + 1) != '}' \
			 && strchr(((X) + 2), '}'))


#define	MIME_MSG(t,s)	((t) == TYPEMESSAGE && (s) && !strucmp((s),"rfc822"))
#define	MIME_DGST(t,s)	((t) == TYPEMULTIPART && (s) && !strucmp((s),"digest"))
/* Is this a message attachment? */
#define	MIME_MSG_A(a)	MIME_MSG((a)->body->type, (a)->body->subtype)
/* Is this a digest attachment? */
#define	MIME_DGST_A(a)	MIME_DGST((a)->body->type, (a)->body->subtype)
#define	MIME_VCARD(t,s)	((((t) == TYPETEXT || (t) == TYPEAPPLICATION) \
	                   && (s) && !strucmp((s),"DIRECTORY"))       \
			 || ((t) == TYPETEXT                          \
			   && (s) && !strucmp((s),"X-VCARD")))
/* Is this a vCard attachment? */
#define	MIME_VCARD_A(a)	MIME_VCARD((a)->body->type, (a)->body->subtype)

#define STYLE_NAME(a)   ((a)->text.desc ? (a)->text.desc : "text")


/*
 * Macros to help fetch specific fields
 */
#define	pine_fetchheader_lines(S,N,M,F)	     pine_fetch_header(S,N,M,F,0L)
#define	pine_fetchheader_lines_not(S,N,M,F)  pine_fetch_header(S,N,M,F,FT_NOT)


/*
 * Macro to help compare two chars without regard to case
 */
#define	CMPNOCASE(x, y)	((isupper((unsigned char) (x))			     \
			    ? tolower((unsigned char) (x))		     \
			    : (unsigned char) (x))			     \
					    == (isupper((unsigned char) (y)) \
				? tolower((unsigned char) (y))		     \
				: (unsigned char) (y)))


/*======================================================================
       Macros for debug printfs 
   n is debugging level:
       1 logs only highest level events and errors
       2 logs events like file writes
       3
       4 logs each command
       5
       6 
       7 logs details of command execution (7 is highest to run any production)
         allows core dumps without cleaning up terminal modes
       8
       9 logs gross details of command execution
    
    This is sort of dumb.  The first argument in x is always debugfile, and
    we're sort of assuming that to be the case below.  However, we don't
    want to go back and change all of the dprint calls now.
  ====*/

#ifdef DEBUG
#ifdef DEBUGJOURNAL
void debugjournal(FILE *where, char *fmt, ...);
#define   dprint(n,x) {							\
		ps_global->dlevel = n;	/* ugly! */			\
		debugjournal x ;					\
	      }
#else	/* !DEBUGJOURNAL */
#define   dprint(n,x) {							\
	       if(debugfile && debug >= (n) && do_debug(debugfile))	\
		  fprintf x ;						\
	      }
#endif	/* !DEBUGJOURNAL */
#else	/* !DEBUG */
#define   dprint(n,x)
#ifdef DEBUGJOURNAL
Cannot have DEBUGJOURNAL without DEBUG.
#endif	/* DEBUGJOURNAL */
#endif	/* !DEBUG */



/*
 * The array is initialized in init.c so the order of that initialization
 * must correspond to the order of the values here.  The order is
 * significant in that it determines the order that the variables
 * are written into the pinerc file and the order they show up in in the
 * config screen.
 */
typedef	enum {    V_PERSONAL_NAME = 0
		, V_USER_ID
		, V_USER_DOMAIN
		, V_SMTP_SERVER
		, V_NNTP_SERVER
		, V_INBOX_PATH
		, V_ARCHIVED_FOLDERS
		, V_PRUNED_FOLDERS
		, V_DEFAULT_FCC
		, V_DEFAULT_SAVE_FOLDER
		, V_POSTPONED_FOLDER
		, V_READ_MESSAGE_FOLDER
		, V_FORM_FOLDER
		, V_LITERAL_SIG
		, V_SIGNATURE_FILE
		, V_FEATURE_LIST
		, V_INIT_CMD_LIST
		, V_COMP_HDRS
		, V_CUSTOM_HDRS
		, V_VIEW_HEADERS
		, V_VIEW_MARGIN_LEFT
		, V_VIEW_MARGIN_RIGHT
		, V_QUOTE_SUPPRESSION
		, V_SAVED_MSG_NAME_RULE
		, V_FCC_RULE
		, V_SORT_KEY
		, V_AB_SORT_RULE
		, V_FLD_SORT_RULE
		, V_GOTO_DEFAULT_RULE
		, V_INCOMING_STARTUP
		, V_PRUNING_RULE
		, V_REOPEN_RULE
		, V_THREAD_DISP_STYLE
		, V_THREAD_INDEX_STYLE
		, V_THREAD_MORE_CHAR
		, V_THREAD_EXP_CHAR
		, V_THREAD_LASTREPLY_CHAR
		, V_CHAR_SET
		, V_EDITOR
		, V_SPELLER
		, V_FILLCOL
		, V_REPLY_STRING
		, V_REPLY_INTRO
		, V_QUOTE_REPLACE_STRING
		, V_EMPTY_HDR_MSG
		, V_IMAGE_VIEWER
		, V_USE_ONLY_DOMAIN_NAME
		, V_BUGS_FULLNAME
		, V_BUGS_ADDRESS
		, V_BUGS_EXTRAS
		, V_SUGGEST_FULLNAME
		, V_SUGGEST_ADDRESS
		, V_LOCAL_FULLNAME
		, V_LOCAL_ADDRESS
		, V_FORCED_ABOOK_ENTRY
		, V_KBLOCK_PASSWD_COUNT
		, V_DISPLAY_FILTERS
		, V_SEND_FILTER
		, V_ALT_ADDRS
		, V_KEYWORDS
		, V_KW_BRACES
		, V_ABOOK_FORMATS
		, V_INDEX_FORMAT
		, V_OVERLAP
		, V_MARGIN
		, V_STATUS_MSG_DELAY
		, V_MAILCHECK
		, V_MAILCHECKNONCURR
		, V_MAILDROPCHECK
		, V_NNTPRANGE
		, V_NEWSRC_PATH
		, V_NEWS_ACTIVE_PATH
		, V_NEWS_SPOOL_DIR
		, V_UPLOAD_CMD
		, V_UPLOAD_CMD_PREFIX
		, V_DOWNLOAD_CMD
		, V_DOWNLOAD_CMD_PREFIX
		, V_MAILCAP_PATH
		, V_MIMETYPE_PATH
		, V_BROWSER
		, V_MAXREMSTREAM
		, V_PERMLOCKED
		, V_DEADLETS
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
		, V_FIFOPATH
#endif
		, V_NMW_WIDTH
    /*
     * Starting here, the rest of the variables are hidden by default in config
     * screen. They are exposed with expose-hidden-config feature.
     */
		, V_INCOMING_FOLDERS
		, V_MAIL_DIRECTORY
		, V_FOLDER_SPEC
		, V_NEWS_SPEC
		, V_ADDRESSBOOK
		, V_GLOB_ADDRBOOK
		, V_STANDARD_PRINTER
		, V_LAST_TIME_PRUNE_QUESTION
		, V_LAST_VERS_USED
		, V_SENDMAIL_PATH
		, V_OPER_DIR
		, V_USERINPUTTIMEO
#ifdef DEBUGJOURNAL
		, V_DEBUGMEM		/* obsolete */
#endif
		, V_TCPOPENTIMEO
		, V_TCPREADWARNTIMEO
		, V_TCPWRITEWARNTIMEO
		, V_TCPQUERYTIMEO
		, V_RSHCMD
		, V_RSHPATH
		, V_RSHOPENTIMEO
		, V_SSHCMD
		, V_SSHPATH
		, V_SSHOPENTIMEO
		, V_NEW_VER_QUELL
		, V_DISABLE_DRIVERS
		, V_DISABLE_AUTHS
		, V_REMOTE_ABOOK_METADATA
		, V_REMOTE_ABOOK_HISTORY
		, V_REMOTE_ABOOK_VALIDITY
		, V_PRINTER
		, V_PERSONAL_PRINT_COMMAND
		, V_PERSONAL_PRINT_CATEGORY
		, V_PATTERNS		/* obsolete */
		, V_PAT_ROLES
		, V_PAT_FILTS
		, V_PAT_FILTS_OLD	/* obsolete */
		, V_PAT_SCORES
		, V_PAT_SCORES_OLD	/* obsolete */
		, V_PAT_INCOLS
		, V_PAT_OTHER
		, V_ELM_STYLE_SAVE	/* obsolete */
		, V_HEADER_IN_REPLY	/* obsolete */
		, V_FEATURE_LEVEL	/* obsolete */
		, V_OLD_STYLE_REPLY	/* obsolete */
		, V_COMPOSE_MIME	/* obsolete */
		, V_SHOW_ALL_CHARACTERS	/* obsolete */
		, V_SAVE_BY_SENDER	/* obsolete */
#if defined(DOS) || defined(OS2)
		, V_FILE_DIR
		, V_FOLDER_EXTENSION
#endif
#ifndef	_WINDOWS
		, V_COLOR_STYLE
#endif
		, V_INDEX_COLOR_STYLE
		, V_TITLEBAR_COLOR_STYLE
		, V_NORM_FORE_COLOR
		, V_NORM_BACK_COLOR
		, V_REV_FORE_COLOR
		, V_REV_BACK_COLOR
		, V_TITLE_FORE_COLOR
		, V_TITLE_BACK_COLOR
		, V_STATUS_FORE_COLOR
		, V_STATUS_BACK_COLOR
		, V_KEYLABEL_FORE_COLOR
		, V_KEYLABEL_BACK_COLOR
		, V_KEYNAME_FORE_COLOR
		, V_KEYNAME_BACK_COLOR
		, V_SLCTBL_FORE_COLOR
		, V_SLCTBL_BACK_COLOR
		, V_QUOTE1_FORE_COLOR
		, V_QUOTE1_BACK_COLOR
		, V_QUOTE2_FORE_COLOR
		, V_QUOTE2_BACK_COLOR
		, V_QUOTE3_FORE_COLOR
		, V_QUOTE3_BACK_COLOR
		, V_SIGNATURE_FORE_COLOR
		, V_SIGNATURE_BACK_COLOR
		, V_PROMPT_FORE_COLOR
		, V_PROMPT_BACK_COLOR
		, V_IND_PLUS_FORE_COLOR
		, V_IND_PLUS_BACK_COLOR
		, V_IND_IMP_FORE_COLOR
		, V_IND_IMP_BACK_COLOR
		, V_IND_DEL_FORE_COLOR
		, V_IND_DEL_BACK_COLOR
		, V_IND_ANS_FORE_COLOR
		, V_IND_ANS_BACK_COLOR
		, V_IND_NEW_FORE_COLOR
		, V_IND_NEW_BACK_COLOR
		, V_IND_REC_FORE_COLOR
		, V_IND_REC_BACK_COLOR
		, V_IND_UNS_FORE_COLOR
		, V_IND_UNS_BACK_COLOR
		, V_IND_ARR_FORE_COLOR
		, V_IND_ARR_BACK_COLOR
		, V_VIEW_HDR_COLORS
		, V_KW_COLORS
#if defined(DOS) || defined(OS2)
#ifdef	_WINDOWS
		, V_FONT_NAME
		, V_FONT_SIZE
		, V_FONT_STYLE
		, V_FONT_CHAR_SET
		, V_PRINT_FONT_NAME
		, V_PRINT_FONT_SIZE
		, V_PRINT_FONT_STYLE
		, V_PRINT_FONT_CHAR_SET
		, V_WINDOW_POSITION
		, V_CURSOR_STYLE
#endif
#endif
#ifdef	ENABLE_LDAP
		, V_LDAP_SERVERS  /* should be last so make will work right */
#endif
		, V_DUMMY
} VariableIndex;

#define	V_LAST_VAR	(V_DUMMY - 1)


#define VAR_PERSONAL_NAME	     vars[V_PERSONAL_NAME].current_val.p
#define FIX_PERSONAL_NAME	     vars[V_PERSONAL_NAME].fixed_val.p
#define COM_PERSONAL_NAME	     vars[V_PERSONAL_NAME].cmdline_val.p
#define VAR_USER_ID		     vars[V_USER_ID].current_val.p
#define COM_USER_ID		     vars[V_USER_ID].cmdline_val.p
#define VAR_USER_DOMAIN		     vars[V_USER_DOMAIN].current_val.p
#define VAR_SMTP_SERVER		     vars[V_SMTP_SERVER].current_val.l
#define FIX_SMTP_SERVER		     vars[V_SMTP_SERVER].fixed_val.l
#define COM_SMTP_SERVER		     vars[V_SMTP_SERVER].cmdline_val.l
#define GLO_SMTP_SERVER		     vars[V_SMTP_SERVER].global_val.l
#define VAR_INBOX_PATH		     vars[V_INBOX_PATH].current_val.p
#define GLO_INBOX_PATH		     vars[V_INBOX_PATH].global_val.p
#define VAR_INCOMING_FOLDERS	     vars[V_INCOMING_FOLDERS].current_val.l
#define VAR_FOLDER_SPEC		     vars[V_FOLDER_SPEC].current_val.l
#define GLO_FOLDER_SPEC		     vars[V_FOLDER_SPEC].global_val.l
#define VAR_NEWS_SPEC		     vars[V_NEWS_SPEC].current_val.l
#define VAR_ARCHIVED_FOLDERS	     vars[V_ARCHIVED_FOLDERS].current_val.l
#define VAR_PRUNED_FOLDERS	     vars[V_PRUNED_FOLDERS].current_val.l
#define GLO_PRUNED_FOLDERS	     vars[V_PRUNED_FOLDERS].global_val.l
#define VAR_DEFAULT_FCC		     vars[V_DEFAULT_FCC].current_val.p
#define GLO_DEFAULT_FCC		     vars[V_DEFAULT_FCC].global_val.p
#define VAR_DEFAULT_SAVE_FOLDER	     vars[V_DEFAULT_SAVE_FOLDER].current_val.p
#define GLO_DEFAULT_SAVE_FOLDER	     vars[V_DEFAULT_SAVE_FOLDER].global_val.p
#define VAR_POSTPONED_FOLDER	     vars[V_POSTPONED_FOLDER].current_val.p
#define GLO_POSTPONED_FOLDER	     vars[V_POSTPONED_FOLDER].global_val.p
#define VAR_MAIL_DIRECTORY	     vars[V_MAIL_DIRECTORY].current_val.p
#define GLO_MAIL_DIRECTORY	     vars[V_MAIL_DIRECTORY].global_val.p
#define VAR_READ_MESSAGE_FOLDER	     vars[V_READ_MESSAGE_FOLDER].current_val.p
#define GLO_READ_MESSAGE_FOLDER	     vars[V_READ_MESSAGE_FOLDER].global_val.p
#define VAR_FORM_FOLDER		     vars[V_FORM_FOLDER].current_val.p
#define GLO_FORM_FOLDER		     vars[V_FORM_FOLDER].global_val.p
#define VAR_SIGNATURE_FILE	     vars[V_SIGNATURE_FILE].current_val.p
#define GLO_SIGNATURE_FILE	     vars[V_SIGNATURE_FILE].global_val.p
#define VAR_LITERAL_SIG		     vars[V_LITERAL_SIG].current_val.p
#define VAR_GLOB_ADDRBOOK	     vars[V_GLOB_ADDRBOOK].current_val.l
#define VAR_ADDRESSBOOK		     vars[V_ADDRESSBOOK].current_val.l
#define GLO_ADDRESSBOOK		     vars[V_ADDRESSBOOK].global_val.l
#define FIX_ADDRESSBOOK		     vars[V_ADDRESSBOOK].fixed_val.l
#define VAR_FEATURE_LIST	     vars[V_FEATURE_LIST].current_val.l
#define VAR_INIT_CMD_LIST	     vars[V_INIT_CMD_LIST].current_val.l
#define GLO_INIT_CMD_LIST	     vars[V_INIT_CMD_LIST].global_val.l
#define COM_INIT_CMD_LIST	     vars[V_INIT_CMD_LIST].cmdline_val.l
#define VAR_COMP_HDRS		     vars[V_COMP_HDRS].current_val.l
#define GLO_COMP_HDRS		     vars[V_COMP_HDRS].global_val.l
#define VAR_CUSTOM_HDRS		     vars[V_CUSTOM_HDRS].current_val.l
#define VAR_VIEW_HEADERS	     vars[V_VIEW_HEADERS].current_val.l
#define VAR_VIEW_MARGIN_LEFT	     vars[V_VIEW_MARGIN_LEFT].current_val.p
#define GLO_VIEW_MARGIN_LEFT	     vars[V_VIEW_MARGIN_LEFT].global_val.p
#define VAR_VIEW_MARGIN_RIGHT	     vars[V_VIEW_MARGIN_RIGHT].current_val.p
#define GLO_VIEW_MARGIN_RIGHT	     vars[V_VIEW_MARGIN_RIGHT].global_val.p
#define VAR_QUOTE_SUPPRESSION	     vars[V_QUOTE_SUPPRESSION].current_val.p
#define GLO_QUOTE_SUPPRESSION	     vars[V_QUOTE_SUPPRESSION].global_val.p
#ifndef	_WINDOWS
#define VAR_COLOR_STYLE		     vars[V_COLOR_STYLE].current_val.p
#define GLO_COLOR_STYLE		     vars[V_COLOR_STYLE].global_val.p
#endif
#define VAR_INDEX_COLOR_STYLE	     vars[V_INDEX_COLOR_STYLE].current_val.p
#define GLO_INDEX_COLOR_STYLE	     vars[V_INDEX_COLOR_STYLE].global_val.p
#define VAR_TITLEBAR_COLOR_STYLE     vars[V_TITLEBAR_COLOR_STYLE].current_val.p
#define GLO_TITLEBAR_COLOR_STYLE     vars[V_TITLEBAR_COLOR_STYLE].global_val.p
#define VAR_SAVED_MSG_NAME_RULE	     vars[V_SAVED_MSG_NAME_RULE].current_val.p
#define GLO_SAVED_MSG_NAME_RULE	     vars[V_SAVED_MSG_NAME_RULE].global_val.p
#define VAR_FCC_RULE		     vars[V_FCC_RULE].current_val.p
#define GLO_FCC_RULE		     vars[V_FCC_RULE].global_val.p
#define VAR_SORT_KEY		     vars[V_SORT_KEY].current_val.p
#define GLO_SORT_KEY		     vars[V_SORT_KEY].global_val.p
#define COM_SORT_KEY		     vars[V_SORT_KEY].cmdline_val.p
#define VAR_AB_SORT_RULE	     vars[V_AB_SORT_RULE].current_val.p
#define GLO_AB_SORT_RULE	     vars[V_AB_SORT_RULE].global_val.p
#define VAR_FLD_SORT_RULE	     vars[V_FLD_SORT_RULE].current_val.p
#define GLO_FLD_SORT_RULE	     vars[V_FLD_SORT_RULE].global_val.p
#define VAR_CHAR_SET		     vars[V_CHAR_SET].current_val.p
#define GLO_CHAR_SET		     vars[V_CHAR_SET].global_val.p
#define VAR_EDITOR		     vars[V_EDITOR].current_val.l
#define GLO_EDITOR		     vars[V_EDITOR].global_val.l
#define VAR_SPELLER		     vars[V_SPELLER].current_val.p
#define GLO_SPELLER		     vars[V_SPELLER].global_val.p
#define VAR_FILLCOL		     vars[V_FILLCOL].current_val.p
#define GLO_FILLCOL		     vars[V_FILLCOL].global_val.p
#define VAR_DEADLETS		     vars[V_DEADLETS].current_val.p
#define GLO_DEADLETS		     vars[V_DEADLETS].global_val.p
#define VAR_REPLY_STRING	     vars[V_REPLY_STRING].current_val.p
#define GLO_REPLY_STRING	     vars[V_REPLY_STRING].global_val.p
#define VAR_QUOTE_REPLACE_STRING     vars[V_QUOTE_REPLACE_STRING].current_val.p
#define GLO_QUOTE_REPLACE_STRING     vars[V_QUOTE_REPLACE_STRING].global_val.p
#define VAR_REPLY_INTRO		     vars[V_REPLY_INTRO].current_val.p
#define GLO_REPLY_INTRO		     vars[V_REPLY_INTRO].global_val.p
#define VAR_EMPTY_HDR_MSG	     vars[V_EMPTY_HDR_MSG].current_val.p
#define GLO_EMPTY_HDR_MSG	     vars[V_EMPTY_HDR_MSG].global_val.p
#define VAR_IMAGE_VIEWER	     vars[V_IMAGE_VIEWER].current_val.p
#define GLO_IMAGE_VIEWER	     vars[V_IMAGE_VIEWER].global_val.p
#define VAR_USE_ONLY_DOMAIN_NAME     vars[V_USE_ONLY_DOMAIN_NAME].current_val.p
#define GLO_USE_ONLY_DOMAIN_NAME     vars[V_USE_ONLY_DOMAIN_NAME].global_val.p
#define VAR_PRINTER		     vars[V_PRINTER].current_val.p
#define GLO_PRINTER		     vars[V_PRINTER].global_val.p
#define VAR_PERSONAL_PRINT_COMMAND   vars[V_PERSONAL_PRINT_COMMAND].current_val.l
#define GLO_PERSONAL_PRINT_COMMAND   vars[V_PERSONAL_PRINT_COMMAND].global_val.l
#define VAR_PERSONAL_PRINT_CATEGORY  vars[V_PERSONAL_PRINT_CATEGORY].current_val.p
#define GLO_PERSONAL_PRINT_CATEGORY  vars[V_PERSONAL_PRINT_CATEGORY].global_val.p
#define VAR_STANDARD_PRINTER	     vars[V_STANDARD_PRINTER].current_val.l
#define GLO_STANDARD_PRINTER	     vars[V_STANDARD_PRINTER].global_val.l
#define FIX_STANDARD_PRINTER	     vars[V_STANDARD_PRINTER].fixed_val.l
#define VAR_LAST_TIME_PRUNE_QUESTION vars[V_LAST_TIME_PRUNE_QUESTION].current_val.p
#define VAR_LAST_VERS_USED	     vars[V_LAST_VERS_USED].current_val.p
#define VAR_BUGS_FULLNAME	     vars[V_BUGS_FULLNAME].current_val.p
#define GLO_BUGS_FULLNAME	     vars[V_BUGS_FULLNAME].global_val.p
#define VAR_BUGS_ADDRESS	     vars[V_BUGS_ADDRESS].current_val.p
#define GLO_BUGS_ADDRESS	     vars[V_BUGS_ADDRESS].global_val.p
#define VAR_BUGS_EXTRAS		     vars[V_BUGS_EXTRAS].current_val.p
#define GLO_BUGS_EXTRAS		     vars[V_BUGS_EXTRAS].global_val.p
#define VAR_SUGGEST_FULLNAME	     vars[V_SUGGEST_FULLNAME].current_val.p
#define GLO_SUGGEST_FULLNAME	     vars[V_SUGGEST_FULLNAME].global_val.p
#define VAR_SUGGEST_ADDRESS	     vars[V_SUGGEST_ADDRESS].current_val.p
#define GLO_SUGGEST_ADDRESS	     vars[V_SUGGEST_ADDRESS].global_val.p
#define VAR_LOCAL_FULLNAME	     vars[V_LOCAL_FULLNAME].current_val.p
#define GLO_LOCAL_FULLNAME	     vars[V_LOCAL_FULLNAME].global_val.p
#define VAR_LOCAL_ADDRESS	     vars[V_LOCAL_ADDRESS].current_val.p
#define GLO_LOCAL_ADDRESS	     vars[V_LOCAL_ADDRESS].global_val.p
#define VAR_FORCED_ABOOK_ENTRY	     vars[V_FORCED_ABOOK_ENTRY].current_val.l
#define VAR_KBLOCK_PASSWD_COUNT	     vars[V_KBLOCK_PASSWD_COUNT].current_val.p
#define GLO_KBLOCK_PASSWD_COUNT	     vars[V_KBLOCK_PASSWD_COUNT].global_val.p
#define VAR_STATUS_MSG_DELAY	     vars[V_STATUS_MSG_DELAY].current_val.p
#define GLO_STATUS_MSG_DELAY	     vars[V_STATUS_MSG_DELAY].global_val.p
#define GLO_SENDMAIL_PATH	     vars[V_SENDMAIL_PATH].global_val.p
#define FIX_SENDMAIL_PATH	     vars[V_SENDMAIL_PATH].fixed_val.p
#define COM_SENDMAIL_PATH	     vars[V_SENDMAIL_PATH].cmdline_val.p
#define VAR_OPER_DIR		     vars[V_OPER_DIR].current_val.p
#define GLO_OPER_DIR		     vars[V_OPER_DIR].global_val.p
#define FIX_OPER_DIR		     vars[V_OPER_DIR].fixed_val.p
#define COM_OPER_DIR		     vars[V_OPER_DIR].cmdline_val.p
#define VAR_DISPLAY_FILTERS	     vars[V_DISPLAY_FILTERS].current_val.l
#define VAR_SEND_FILTER		     vars[V_SEND_FILTER].current_val.l
#define GLO_SEND_FILTER		     vars[V_SEND_FILTER].global_val.l
#define VAR_ALT_ADDRS		     vars[V_ALT_ADDRS].current_val.l
#define VAR_KEYWORDS		     vars[V_KEYWORDS].current_val.l
#define VAR_KW_COLORS		     vars[V_KW_COLORS].current_val.l
#define GLO_KW_BRACES		     vars[V_KW_BRACES].global_val.p
#define VAR_KW_BRACES		     vars[V_KW_BRACES].current_val.p
#define VAR_ABOOK_FORMATS	     vars[V_ABOOK_FORMATS].current_val.l
#define VAR_INDEX_FORMAT	     vars[V_INDEX_FORMAT].current_val.p
#define VAR_OVERLAP		     vars[V_OVERLAP].current_val.p
#define GLO_OVERLAP		     vars[V_OVERLAP].global_val.p
#define VAR_MAXREMSTREAM	     vars[V_MAXREMSTREAM].current_val.p
#define GLO_MAXREMSTREAM	     vars[V_MAXREMSTREAM].global_val.p
#define VAR_PERMLOCKED		     vars[V_PERMLOCKED].current_val.l
#define VAR_MARGIN		     vars[V_MARGIN].current_val.p
#define GLO_MARGIN		     vars[V_MARGIN].global_val.p
#define VAR_MAILCHECK		     vars[V_MAILCHECK].current_val.p
#define GLO_MAILCHECK		     vars[V_MAILCHECK].global_val.p
#define VAR_MAILCHECKNONCURR	     vars[V_MAILCHECKNONCURR].current_val.p
#define GLO_MAILCHECKNONCURR	     vars[V_MAILCHECKNONCURR].global_val.p
#define VAR_MAILDROPCHECK	     vars[V_MAILDROPCHECK].current_val.p
#define GLO_MAILDROPCHECK	     vars[V_MAILDROPCHECK].global_val.p
#define VAR_NNTPRANGE     	     vars[V_NNTPRANGE].current_val.p
#define GLO_NNTPRANGE     	     vars[V_NNTPRANGE].global_val.p
#define VAR_NEWSRC_PATH		     vars[V_NEWSRC_PATH].current_val.p
#define GLO_NEWSRC_PATH		     vars[V_NEWSRC_PATH].global_val.p
#define VAR_NEWS_ACTIVE_PATH	     vars[V_NEWS_ACTIVE_PATH].current_val.p
#define GLO_NEWS_ACTIVE_PATH	     vars[V_NEWS_ACTIVE_PATH].global_val.p
#define VAR_NEWS_SPOOL_DIR	     vars[V_NEWS_SPOOL_DIR].current_val.p
#define GLO_NEWS_SPOOL_DIR	     vars[V_NEWS_SPOOL_DIR].global_val.p
#define VAR_DISABLE_DRIVERS	     vars[V_DISABLE_DRIVERS].current_val.l
#define VAR_DISABLE_AUTHS	     vars[V_DISABLE_AUTHS].current_val.l
#define VAR_REMOTE_ABOOK_METADATA    vars[V_REMOTE_ABOOK_METADATA].current_val.p
#define VAR_REMOTE_ABOOK_HISTORY     vars[V_REMOTE_ABOOK_HISTORY].current_val.p
#define GLO_REMOTE_ABOOK_HISTORY     vars[V_REMOTE_ABOOK_HISTORY].global_val.p
#define VAR_REMOTE_ABOOK_VALIDITY    vars[V_REMOTE_ABOOK_VALIDITY].current_val.p
#define GLO_REMOTE_ABOOK_VALIDITY    vars[V_REMOTE_ABOOK_VALIDITY].global_val.p
  /* Elm style save is obsolete in Pine 3.81 (see saved msg name rule) */
#define VAR_ELM_STYLE_SAVE           vars[V_ELM_STYLE_SAVE].current_val.p
#define GLO_ELM_STYLE_SAVE           vars[V_ELM_STYLE_SAVE].global_val.p
  /* Header in reply is obsolete in Pine 3.83 (see feature list) */
#define VAR_HEADER_IN_REPLY          vars[V_HEADER_IN_REPLY].current_val.p
#define GLO_HEADER_IN_REPLY          vars[V_HEADER_IN_REPLY].global_val.p
  /* Feature level is obsolete in Pine 3.83 (see feature list) */
#define VAR_FEATURE_LEVEL            vars[V_FEATURE_LEVEL].current_val.p
#define GLO_FEATURE_LEVEL            vars[V_FEATURE_LEVEL].global_val.p
  /* Old style reply is obsolete in Pine 3.83 (see feature list) */
#define VAR_OLD_STYLE_REPLY          vars[V_OLD_STYLE_REPLY].current_val.p
#define GLO_OLD_STYLE_REPLY          vars[V_OLD_STYLE_REPLY].global_val.p
  /* Save by sender is obsolete in Pine 3.83 (see saved msg name rule) */
#define VAR_SAVE_BY_SENDER           vars[V_SAVE_BY_SENDER].current_val.p
#define GLO_SAVE_BY_SENDER           vars[V_SAVE_BY_SENDER].global_val.p
#define VAR_NNTP_SERVER              vars[V_NNTP_SERVER].current_val.l
#define FIX_NNTP_SERVER              vars[V_NNTP_SERVER].fixed_val.l
#ifdef	ENABLE_LDAP
#define VAR_LDAP_SERVERS             vars[V_LDAP_SERVERS].current_val.l
#endif
#define VAR_UPLOAD_CMD		     vars[V_UPLOAD_CMD].current_val.p
#define VAR_UPLOAD_CMD_PREFIX	     vars[V_UPLOAD_CMD_PREFIX].current_val.p
#define VAR_DOWNLOAD_CMD	     vars[V_DOWNLOAD_CMD].current_val.p
#define VAR_DOWNLOAD_CMD_PREFIX	     vars[V_DOWNLOAD_CMD_PREFIX].current_val.p
#define VAR_GOTO_DEFAULT_RULE	     vars[V_GOTO_DEFAULT_RULE].current_val.p
#define GLO_GOTO_DEFAULT_RULE	     vars[V_GOTO_DEFAULT_RULE].global_val.p
#define VAR_MAILCAP_PATH	     vars[V_MAILCAP_PATH].current_val.p
#define VAR_MIMETYPE_PATH	     vars[V_MIMETYPE_PATH].current_val.p
#define VAR_FIFOPATH		     vars[V_FIFOPATH].current_val.p
#define VAR_NMW_WIDTH		     vars[V_NMW_WIDTH].current_val.p
#define GLO_NMW_WIDTH		     vars[V_NMW_WIDTH].global_val.p
#define VAR_USERINPUTTIMEO     	     vars[V_USERINPUTTIMEO].current_val.p
#define GLO_USERINPUTTIMEO     	     vars[V_USERINPUTTIMEO].global_val.p
#define VAR_TCPOPENTIMEO	     vars[V_TCPOPENTIMEO].current_val.p
#define VAR_TCPREADWARNTIMEO	     vars[V_TCPREADWARNTIMEO].current_val.p
#define VAR_TCPWRITEWARNTIMEO	     vars[V_TCPWRITEWARNTIMEO].current_val.p
#define VAR_TCPQUERYTIMEO	     vars[V_TCPQUERYTIMEO].current_val.p
#define VAR_RSHOPENTIMEO	     vars[V_RSHOPENTIMEO].current_val.p
#define VAR_RSHPATH		     vars[V_RSHPATH].current_val.p
#define VAR_RSHCMD		     vars[V_RSHCMD].current_val.p
#define VAR_SSHOPENTIMEO	     vars[V_SSHOPENTIMEO].current_val.p
#define VAR_SSHPATH		     vars[V_SSHPATH].current_val.p
#define GLO_SSHPATH		     vars[V_SSHPATH].global_val.p
#define VAR_SSHCMD		     vars[V_SSHCMD].current_val.p
#define GLO_SSHCMD		     vars[V_SSHCMD].global_val.p
#define VAR_NEW_VER_QUELL	     vars[V_NEW_VER_QUELL].current_val.p
#define VAR_BROWSER		     vars[V_BROWSER].current_val.l
#define VAR_INCOMING_STARTUP	     vars[V_INCOMING_STARTUP].current_val.p
#define GLO_INCOMING_STARTUP	     vars[V_INCOMING_STARTUP].global_val.p
#define VAR_PRUNING_RULE	     vars[V_PRUNING_RULE].current_val.p
#define GLO_PRUNING_RULE	     vars[V_PRUNING_RULE].global_val.p
#define VAR_REOPEN_RULE		     vars[V_REOPEN_RULE].current_val.p
#define GLO_REOPEN_RULE		     vars[V_REOPEN_RULE].global_val.p
#define VAR_THREAD_DISP_STYLE	     vars[V_THREAD_DISP_STYLE].current_val.p
#define GLO_THREAD_DISP_STYLE	     vars[V_THREAD_DISP_STYLE].global_val.p
#define VAR_THREAD_INDEX_STYLE	     vars[V_THREAD_INDEX_STYLE].current_val.p
#define GLO_THREAD_INDEX_STYLE	     vars[V_THREAD_INDEX_STYLE].global_val.p
#define VAR_THREAD_MORE_CHAR	     vars[V_THREAD_MORE_CHAR].current_val.p
#define GLO_THREAD_MORE_CHAR	     vars[V_THREAD_MORE_CHAR].global_val.p
#define VAR_THREAD_EXP_CHAR	     vars[V_THREAD_EXP_CHAR].current_val.p
#define GLO_THREAD_EXP_CHAR	     vars[V_THREAD_EXP_CHAR].global_val.p
#define VAR_THREAD_LASTREPLY_CHAR    vars[V_THREAD_LASTREPLY_CHAR].current_val.p
#define GLO_THREAD_LASTREPLY_CHAR    vars[V_THREAD_LASTREPLY_CHAR].global_val.p

#if defined(DOS) || defined(OS2)
#define	VAR_FILE_DIR		     vars[V_FILE_DIR].current_val.p
#define	GLO_FILE_DIR		     vars[V_FILE_DIR].current_val.p
#define VAR_FOLDER_EXTENSION	     vars[V_FOLDER_EXTENSION].current_val.p
#define GLO_FOLDER_EXTENSION	     vars[V_FOLDER_EXTENSION].global_val.p

#ifdef	_WINDOWS
#define VAR_FONT_NAME		     vars[V_FONT_NAME].current_val.p
#define VAR_FONT_SIZE		     vars[V_FONT_SIZE].current_val.p
#define VAR_FONT_STYLE		     vars[V_FONT_STYLE].current_val.p
#define VAR_FONT_CHAR_SET      	     vars[V_FONT_CHAR_SET].current_val.p
#define VAR_PRINT_FONT_NAME	     vars[V_PRINT_FONT_NAME].current_val.p
#define VAR_PRINT_FONT_SIZE	     vars[V_PRINT_FONT_SIZE].current_val.p
#define VAR_PRINT_FONT_STYLE	     vars[V_PRINT_FONT_STYLE].current_val.p
#define VAR_PRINT_FONT_CHAR_SET	     vars[V_PRINT_FONT_CHAR_SET].current_val.p
#define VAR_WINDOW_POSITION	     vars[V_WINDOW_POSITION].current_val.p
#define VAR_CURSOR_STYLE	     vars[V_CURSOR_STYLE].current_val.p
#endif	/* _WINDOWS */
#endif	/* DOS or OS2 */

#define VAR_NORM_FORE_COLOR	     vars[V_NORM_FORE_COLOR].current_val.p
#define GLO_NORM_FORE_COLOR	     vars[V_NORM_FORE_COLOR].global_val.p
#define VAR_NORM_BACK_COLOR	     vars[V_NORM_BACK_COLOR].current_val.p
#define GLO_NORM_BACK_COLOR	     vars[V_NORM_BACK_COLOR].global_val.p
#define VAR_REV_FORE_COLOR	     vars[V_REV_FORE_COLOR].current_val.p
#define GLO_REV_FORE_COLOR	     vars[V_REV_FORE_COLOR].global_val.p
#define VAR_REV_BACK_COLOR	     vars[V_REV_BACK_COLOR].current_val.p
#define GLO_REV_BACK_COLOR	     vars[V_REV_BACK_COLOR].global_val.p
#define VAR_TITLE_FORE_COLOR	     vars[V_TITLE_FORE_COLOR].current_val.p
#define VAR_TITLE_BACK_COLOR	     vars[V_TITLE_BACK_COLOR].current_val.p
#define VAR_STATUS_FORE_COLOR	     vars[V_STATUS_FORE_COLOR].current_val.p
#define VAR_STATUS_BACK_COLOR	     vars[V_STATUS_BACK_COLOR].current_val.p
#define VAR_IND_PLUS_FORE_COLOR	     vars[V_IND_PLUS_FORE_COLOR].current_val.p
#define VAR_IND_PLUS_BACK_COLOR	     vars[V_IND_PLUS_BACK_COLOR].current_val.p
#define VAR_IND_IMP_FORE_COLOR	     vars[V_IND_IMP_FORE_COLOR].current_val.p
#define VAR_IND_IMP_BACK_COLOR	     vars[V_IND_IMP_BACK_COLOR].current_val.p
#define VAR_IND_DEL_FORE_COLOR	     vars[V_IND_DEL_FORE_COLOR].current_val.p
#define VAR_IND_DEL_BACK_COLOR	     vars[V_IND_DEL_BACK_COLOR].current_val.p
#define VAR_IND_ANS_FORE_COLOR	     vars[V_IND_ANS_FORE_COLOR].current_val.p
#define VAR_IND_ANS_BACK_COLOR	     vars[V_IND_ANS_BACK_COLOR].current_val.p
#define VAR_IND_NEW_FORE_COLOR	     vars[V_IND_NEW_FORE_COLOR].current_val.p
#define VAR_IND_NEW_BACK_COLOR	     vars[V_IND_NEW_BACK_COLOR].current_val.p
#define VAR_IND_REC_FORE_COLOR	     vars[V_IND_REC_FORE_COLOR].current_val.p
#define VAR_IND_REC_BACK_COLOR	     vars[V_IND_REC_BACK_COLOR].current_val.p
#define VAR_IND_UNS_FORE_COLOR	     vars[V_IND_UNS_FORE_COLOR].current_val.p
#define VAR_IND_UNS_BACK_COLOR	     vars[V_IND_UNS_BACK_COLOR].current_val.p
#define VAR_IND_ARR_FORE_COLOR	     vars[V_IND_ARR_FORE_COLOR].current_val.p
#define VAR_IND_ARR_BACK_COLOR	     vars[V_IND_ARR_BACK_COLOR].current_val.p
#define VAR_KEYLABEL_FORE_COLOR	     vars[V_KEYLABEL_FORE_COLOR].current_val.p
#define VAR_KEYLABEL_BACK_COLOR	     vars[V_KEYLABEL_BACK_COLOR].current_val.p
#define VAR_KEYNAME_FORE_COLOR	     vars[V_KEYNAME_FORE_COLOR].current_val.p
#define VAR_KEYNAME_BACK_COLOR	     vars[V_KEYNAME_BACK_COLOR].current_val.p
#define VAR_SLCTBL_FORE_COLOR	     vars[V_SLCTBL_FORE_COLOR].current_val.p
#define VAR_SLCTBL_BACK_COLOR	     vars[V_SLCTBL_BACK_COLOR].current_val.p
#define VAR_QUOTE1_FORE_COLOR	     vars[V_QUOTE1_FORE_COLOR].current_val.p
#define VAR_QUOTE1_BACK_COLOR	     vars[V_QUOTE1_BACK_COLOR].current_val.p
#define VAR_QUOTE2_FORE_COLOR	     vars[V_QUOTE2_FORE_COLOR].current_val.p
#define VAR_QUOTE2_BACK_COLOR	     vars[V_QUOTE2_BACK_COLOR].current_val.p
#define VAR_QUOTE3_FORE_COLOR	     vars[V_QUOTE3_FORE_COLOR].current_val.p
#define VAR_QUOTE3_BACK_COLOR	     vars[V_QUOTE3_BACK_COLOR].current_val.p
#define VAR_SIGNATURE_FORE_COLOR     vars[V_SIGNATURE_FORE_COLOR].current_val.p
#define VAR_SIGNATURE_BACK_COLOR     vars[V_SIGNATURE_BACK_COLOR].current_val.p
#define VAR_PROMPT_FORE_COLOR	     vars[V_PROMPT_FORE_COLOR].current_val.p
#define VAR_PROMPT_BACK_COLOR	     vars[V_PROMPT_BACK_COLOR].current_val.p
#define VAR_VIEW_HDR_COLORS	     vars[V_VIEW_HDR_COLORS].current_val.l


/*
 * The list of feature numbers (which bit goes with which feature).
 * The order of the features is not significant.
 */
typedef enum {
	F_OLD_GROWTH = 0,
	F_ENABLE_FULL_HDR,
	F_ENABLE_PIPE,
	F_ENABLE_TAB_COMPLETE,
	F_QUIT_WO_CONFIRM,
	F_ENABLE_JUMP,
	F_ENABLE_ALT_ED,
	F_ENABLE_BOUNCE,
	F_ENABLE_AGG_OPS,
	F_ENABLE_FLAG,
	F_CAN_SUSPEND,
	F_USE_FK,
	F_INCLUDE_HEADER,
	F_SIG_AT_BOTTOM,
	F_DEL_SKIPS_DEL,
	F_AUTO_EXPUNGE,
	F_FULL_AUTO_EXPUNGE,
	F_EXPUNGE_MANUALLY,
	F_AUTO_READ_MSGS,
	F_AUTO_FCC_ONLY,
	F_READ_IN_NEWSRC_ORDER,
	F_SELECT_WO_CONFIRM,
	F_SAVE_PARTIAL_WO_CONFIRM,
	F_NEXT_THRD_WO_CONFIRM,
	F_USE_CURRENT_DIR,
	F_STARTUP_STAYOPEN,
	F_SAVE_WONT_DELETE,
	F_SAVE_ADVANCES,
	F_UNSELECT_WONT_ADVANCE,
	F_FORCE_LOW_SPEED,
	F_FORCE_ARROW,
	F_PRUNE_USES_ISO,
	F_ALT_ED_NOW,
	F_SHOW_DELAY_CUE,
	F_CANCEL_CONFIRM,
	F_AUTO_OPEN_NEXT_UNREAD,
	F_SELECTED_SHOWN_BOLD,
	F_QUOTE_ALL_FROMS,
	F_AUTO_INCLUDE_IN_REPLY,
	F_DISABLE_CONFIG_SCREEN,
	F_DISABLE_PASSWORD_CACHING,
	F_DISABLE_PASSWORD_CMD,
	F_DISABLE_UPDATE_CMD,
	F_DISABLE_KBLOCK_CMD,
	F_DISABLE_SIGEDIT_CMD,
	F_DISABLE_ROLES_SETUP,
	F_DISABLE_ROLES_SIGEDIT,
	F_DISABLE_ROLES_TEMPLEDIT,
	F_DISABLE_PIPES_IN_SIGS,
	F_DISABLE_PIPES_IN_TEMPLATES,
	F_ATTACHMENTS_IN_REPLY,
	F_ENABLE_INCOMING,
	F_NO_NEWS_VALIDATION,
	F_QUELL_EXTRA_POST_PROMPT,
	F_DISABLE_TAKE_LASTFIRST,
	F_DISABLE_TAKE_FULLNAMES,
	F_DISABLE_TERM_RESET_DISP,
	F_DISABLE_SENDER,
	F_ROT13_MESSAGE_ID,
	F_QUELL_LOCAL_LOOKUP,
	F_COMPOSE_TO_NEWSGRP,
	F_PRESERVE_START_STOP,
	F_COMPOSE_REJECTS_UNQUAL,
	F_FAKE_NEW_IN_NEWS,
	F_SUSPEND_SPAWNS,
	F_ENABLE_8BIT,
	F_COMPOSE_MAPS_DEL,
	F_ENABLE_8BIT_NNTP,
	F_ENABLE_MOUSE,
	F_SHOW_CURSOR,
	F_PASS_CONTROL_CHARS,
	F_PASS_C1_CONTROL_CHARS,
	F_SINGLE_FOLDER_LIST,
	F_VERTICAL_FOLDER_LIST,
	F_TAB_CHK_RECENT,
	F_AUTO_REPLY_TO,
	F_VERBOSE_POST,
	F_FCC_ON_BOUNCE,
	F_SEND_WO_CONFIRM,
	F_USE_SENDER_NOT_X,
	F_BLANK_KEYMENU,
	F_CUSTOM_PRINT,
	F_DEL_FROM_DOT,
	F_AUTO_ZOOM,
	F_AUTO_UNZOOM,
	F_PRINT_INDEX,
	F_ALLOW_TALK,
	F_AGG_PRINT_FF,
	F_ENABLE_DOT_FILES,
	F_ENABLE_DOT_FOLDERS,
	F_FIRST_SEND_FILTER_DFLT,
	F_ALWAYS_LAST_FLDR_DFLT,
	F_TAB_TO_NEW,
	F_MARK_FOR_CC,
	F_WARN_ABOUT_NO_SUBJECT,
	F_WARN_ABOUT_NO_TO_OR_CC,
	F_QUELL_DEAD_LETTER,
	F_QUELL_BEEPS,
	F_QUELL_LOCK_FAILURE_MSGS,
	F_ENABLE_SPACE_AS_TAB,
	F_ENABLE_TAB_DELETES,
	F_FLAG_SCREEN_KW_SHORTCUT,
	F_FLAG_SCREEN_DFLT,
	F_ENABLE_XTERM_NEWMAIL,
	F_ENABLE_NEWMAIL_SHORT_TEXT,
	F_EXPANDED_DISTLISTS,
	F_AGG_SEQ_COPY,
	F_DISABLE_ALARM,
	F_DISABLE_SETLOCALE_COLLATE,
	F_ENABLE_SETLOCALE_CTYPE,
	F_FROM_DELIM_IN_PRINT,
	F_BACKGROUND_POST,
	F_ALLOW_GOTO,
	F_DSN,
	F_ENABLE_SEARCH_AND_REPL,
	F_ARROW_NAV,
	F_RELAXED_ARROW_NAV,
	F_TCAP_WINS,
	F_ENABLE_SIGDASHES,
	F_ENABLE_STRIP_SIGDASHES,
	F_QUELL_PARTIAL_FETCH,
	F_QUELL_PERSONAL_NAME_PROMPT,
	F_QUELL_USER_ID_PROMPT,
	F_VIEW_SEL_ATTACH,
	F_VIEW_SEL_URL,
	F_VIEW_SEL_URL_HOST,
	F_SCAN_ADDR,
	F_FORCE_ARROWS,
	F_PREFER_PLAIN_TEXT,
	F_QUELL_CHARSET_WARNING,
	F_ENABLE_EDIT_REPLY_INDENT,
	F_ENABLE_PRYNT,
	F_ALLOW_CHANGING_FROM,
	F_CACHE_REMOTE_PINERC,
	F_ENABLE_SUB_LISTS,
	F_ENABLE_LESSTHAN_EXIT,
	F_ENABLE_FAST_RECENT,
	F_TAB_USES_UNSEEN,
	F_ENABLE_ROLE_TAKE,
	F_ENABLE_TAKE_EXPORT,
	F_QUELL_ATTACH_EXTRA_PROMPT,
	F_QUELL_ATTACH_EXT_WARN,
	F_QUELL_FILTER_MSGS,
	F_QUELL_FILTER_DONE_MSG,
	F_SHOW_SORT,
	F_FIX_BROKEN_LIST,
	F_ENABLE_MULNEWSRCS,
	F_PREDICT_NNTP_SERVER,
	F_NEWS_CROSS_DELETE,
	F_NEWS_CATCHUP,
	F_QUELL_INTERNAL_MSG,
	F_QUELL_IMAP_ENV_CB,
	F_QUELL_NEWS_ENV_CB,
	F_SEPARATE_FLDR_AS_DIR,
	F_CMBND_ABOOK_DISP,
	F_CMBND_FOLDER_DISP,
	F_CMBND_SUBDIR_DISP,
	F_EXPANDED_ADDRBOOKS,
	F_EXPANDED_FOLDERS,
	F_QUELL_EMPTY_DIRS,
	F_SHOW_TEXTPLAIN_INT,
	F_ROLE_CONFIRM_DEFAULT,
	F_TAB_NO_CONFIRM,
	F_RET_INBOX_NO_CONFIRM,
	F_CHECK_MAIL_ONQUIT,
	F_PREOPEN_STAYOPENS,
	F_EXPUNGE_STAYOPENS,
	F_EXPUNGE_INBOX,
	F_NO_FCC_ATTACH,
	F_DO_MAILCAP_PARAM_SUBST,
	F_PREFER_ALT_AUTH,
	F_SLCTBL_ITEM_NOBOLD,
	F_QUELL_PINGS_COMPOSING,
	F_QUELL_PINGS_COMPOSING_INBOX,
	F_QUELL_BEZERK_TIMEZONE,
	F_QUELL_CONTENT_ID,
	F_QUELL_MAILDOMAIN_WARNING,
	F_DISABLE_SHARED_NAMESPACES,
	F_HIDE_NNTP_PATH,
	F_MAILDROPS_PRESERVE_STATE,
	F_EXPOSE_HIDDEN_CONFIG,
	F_ALT_COMPOSE_MENU,
	F_ALT_ROLE_MENU,
	F_ALWAYS_SPELL_CHECK,
	F_QUELL_TIMEZONE,
	F_COLOR_LINE_IMPORTANT,
	F_SLASH_COLL_ENTIRE,
	F_ENABLE_FULL_HDR_AND_TEXT,
	F_QUELL_FULL_HDR_RESET,
	F_DISABLE_2022_JP_CONVERSIONS,
	F_DISABLE_CHARSET_CONVERSIONS,
	F_MARK_FCC_SEEN,
	F_MULNEWSRC_HOSTNAMES_AS_TYPED,
	F_STRIP_WS_BEFORE_SEND,
	F_QUELL_FLOWED_TEXT,
	F_COMPOSE_ALWAYS_DOWNGRADE,
	F_SORT_DEFAULT_FCC_ALPHA,
	F_SORT_DEFAULT_SAVE_ALPHA,
	F_QUOTE_REPLACE_NOFLOW,
#ifdef	_WINDOWS
	F_ENABLE_TRAYICON,
	F_QUELL_SSL_LARGEBLOCKS,
	F_STORE_WINPOS_IN_CONFIG,
#endif
#ifdef	ENABLE_LDAP
	F_ADD_LDAP_TO_ABOOK,
#endif
	F_FEATURE_LIST_COUNT	/* Number of features */
} FeatureList;


/* Feature list support */
/* Is feature "feature" turned on? */
#define F_ON(feature,ps)	(bitnset((feature),(ps)->feature_list))
#define F_OFF(feature,ps)	(!F_ON(feature,ps))
#define F_TURN_ON(feature,ps)   (setbitn((feature),(ps)->feature_list))
#define F_TURN_OFF(feature,ps)  (clrbitn((feature),(ps)->feature_list))
/* turn off or on depending on value */
#define F_SET(feature,ps,value) ((value) ? F_TURN_ON((feature),(ps))       \
					 : F_TURN_OFF((feature),(ps)))


/*
 * Status message types
 */
#define	SM_DING		0x01			/* ring bell when displayed  */
#define	SM_ASYNC	0x02			/* display any time	     */
#define	SM_ORDER	0x04			/* ordered, flush on prompt  */
#define	SM_MODAL	0x08			/* display, wait for user    */
#define	SM_INFO		0x10			/* Handy, but discardable    */

/*
 * Which header fields should format_envelope output?
 */
#define	FE_FROM		0x0001
#define	FE_SENDER	0x0002
#define	FE_DATE		0x0004
#define	FE_TO		0x0008
#define	FE_CC		0x0010
#define	FE_BCC		0x0020
#define	FE_NEWSGROUPS	0x0040
#define	FE_SUBJECT	0x0080
#define	FE_MESSAGEID	0x0100
#define	FE_REPLYTO	0x0200
#define	FE_FOLLOWUPTO	0x0400
#define	FE_INREPLYTO	0x0800
#define	FE_RETURNPATH	0x1000
#define	FE_REFERENCES	0x2000
#define	FE_DEFAULT	(FE_FROM | FE_DATE | FE_TO | FE_CC | FE_BCC \
			 | FE_NEWSGROUPS | FE_SUBJECT | FE_REPLYTO \
			 | FE_FOLLOWUPTO)

#define ALL_EXCEPT	"all-except"
#define INHERIT		"INHERIT"

/*
 * Flags to indicate how much index border to paint
 */
#define	INDX_CLEAR	0x01
#define	INDX_HEADER	0x02
#define	INDX_FOOTER	0x04


/*
 * Flags to indicate context (i.e., folder collection) use
 */
#define	CNTXT_PSEUDO	0x0001			/* fake folder entry exists  */
#define	CNTXT_INCMNG	0x0002			/* inbox collection	     */
#define	CNTXT_SAVEDFLT	0x0004			/* default save collection   */
#define	CNTXT_PARTFIND	0x0008			/* partial find done         */
#define	CNTXT_NOFIND	0x0010			/* no find done in context   */
#define	CNTXT_FINDALL   0x0020			/* Do a find_all on context  */
#define	CNTXT_NEWS	0x0040			/* News namespace collection */
#define	CNTXT_SUBDIR	0x0080			/* subdirectory within col'n */
#define	CNTXT_PRESRV	0x0100			/* preserve/restore selected */
#define	CNTXT_ZOOM	0x0200			/* context display narrowed  */
#define	CNTXT_INHERIT	0x1000

/*
 * Useful def's to specify interesting flags
 */
#define	F_NONE		0x00000000
#define	F_SEEN		0x00000001
#define	F_UNSEEN	0x00000002
#define	F_DEL		0x00000004
#define	F_UNDEL		0x00000008
#define	F_FLAG		0x00000010
#define	F_UNFLAG	0x00000020
#define	F_ANS		0x00000040
#define	F_UNANS		0x00000080
#define	F_RECENT	0x00000100
#define	F_UNRECENT	0x00000200
#define	F_DRAFT		0x00000400
#define	F_UNDRAFT	0x00000800

#define	F_SRCHBACK	0x00001000	/* search backwards instead of forw */

#define	F_OR_SEEN	0x00010000
#define	F_OR_UNSEEN	0x00020000
#define	F_OR_DEL	0x00040000
#define	F_OR_UNDEL	0x00080000
#define	F_OR_FLAG	0x00100000
#define	F_OR_UNFLAG	0x00200000
#define	F_OR_ANS	0x00400000
#define	F_OR_UNANS	0x00800000
#define	F_OR_RECENT	0x01000000
#define	F_OR_UNRECENT	0x02000000

#define	F_KEYWORD	0x04000000
#define	F_UNKEYWORD	0x08000000


/*
 * Useful flag checking macro
 */
#define FLAG_MATCH(F, M)   (((((F)&F_SEEN)   ? (M)->seen		     \
				: ((F)&F_UNSEEN)     ? !(M)->seen : 1)	     \
			  && (((F)&F_DEL)    ? (M)->deleted		     \
				: ((F)&F_UNDEL)      ? !(M)->deleted : 1)    \
			  && (((F)&F_ANS)    ? (M)->answered		     \
				: ((F)&F_UNANS)	     ? !(M)->answered : 1)   \
			  && (((F)&F_FLAG) ? (M)->flagged		     \
				: ((F)&F_UNFLAG)   ? !(M)->flagged : 1)	     \
			  && (((F)&F_RECENT) ? (M)->recent		     \
				: ((F)&F_UNRECENT)   ? !(M)->recent : 1))    \
			  || ((((F)&F_OR_SEEN) ? (M)->seen		     \
				: ((F)&F_OR_UNSEEN)   ? !(M)->seen : 0)      \
			  || (((F)&F_OR_DEL)   ? (M)->deleted		     \
				: ((F)&F_OR_UNDEL)    ? !(M)->deleted : 0)   \
			  || (((F)&F_OR_ANS)   ? (M)->answered		     \
				: ((F)&F_OR_UNANS)    ? !(M)->answered : 0)  \
			  || (((F)&F_OR_FLAG)? (M)->flagged		     \
				: ((F)&F_OR_UNFLAG) ? !(M)->flagged : 0)     \
			  || (((F)&F_OR_RECENT)? (M)->recent		     \
				: ((F)&F_OR_UNRECENT) ? !(M)->recent : 0)))


/*
 * Save message rules.  if these grow, widen pine
 * struct's save_msg_rule...
 */
#define	MSG_RULE_DEFLT			0
#define	MSG_RULE_LAST			1
#define	MSG_RULE_FROM			2
#define	MSG_RULE_NICK_FROM_DEF		3
#define	MSG_RULE_NICK_FROM		4
#define	MSG_RULE_FCC_FROM_DEF		5
#define	MSG_RULE_FCC_FROM		6
#define	MSG_RULE_RN_FROM_DEF		7
#define	MSG_RULE_RN_FROM		8
#define	MSG_RULE_SENDER			9
#define	MSG_RULE_NICK_SENDER_DEF	10
#define	MSG_RULE_NICK_SENDER		11
#define	MSG_RULE_FCC_SENDER_DEF		12
#define	MSG_RULE_FCC_SENDER		13
#define	MSG_RULE_RN_SENDER_DEF		14
#define	MSG_RULE_RN_SENDER		15
#define	MSG_RULE_RECIP			16
#define	MSG_RULE_NICK_RECIP_DEF		17
#define	MSG_RULE_NICK_RECIP		18
#define	MSG_RULE_FCC_RECIP_DEF		19
#define	MSG_RULE_FCC_RECIP		20
#define	MSG_RULE_RN_RECIP_DEF		21
#define	MSG_RULE_RN_RECIP		22
#define	MSG_RULE_REPLYTO		23
#define	MSG_RULE_NICK_REPLYTO_DEF	24
#define	MSG_RULE_NICK_REPLYTO		25
#define	MSG_RULE_FCC_REPLYTO_DEF	26
#define	MSG_RULE_FCC_REPLYTO		27
#define	MSG_RULE_RN_REPLYTO_DEF		28
#define	MSG_RULE_RN_REPLYTO		29

/*
 * Fcc rules.  if these grow, widen pine
 * struct's fcc_rule...
 */
#define	FCC_RULE_DEFLT		0
#define	FCC_RULE_RECIP		1
#define	FCC_RULE_LAST		2
#define	FCC_RULE_NICK		3
#define	FCC_RULE_NICK_RECIP	4
#define	FCC_RULE_CURRENT	5

/*
 * Addrbook sorting rules.  if these grow, widen pine
 * struct's ab_sort_rule...
 */
#define	AB_SORT_RULE_FULL_LISTS		0
#define	AB_SORT_RULE_FULL  		1
#define	AB_SORT_RULE_NICK_LISTS         2
#define	AB_SORT_RULE_NICK 		3
#define	AB_SORT_RULE_NONE		4

#define AB_COMMENT_STR			"Comment   : "

/*
 * Icon text types, to tell which icon to use
 */
#define IT_NEWMAIL 0
#define IT_MCLOSED 1

/*
 * Incoming startup rules.  if these grow, widen pine
 * struct's inc_startup_rule and reset_startup_rule().
 */
#define	IS_FIRST_UNSEEN			0
#define	IS_FIRST_RECENT  		1
#define	IS_FIRST_IMPORTANT  		2
#define	IS_FIRST_IMPORTANT_OR_UNSEEN  	3
#define	IS_FIRST_IMPORTANT_OR_RECENT  	4
#define	IS_FIRST         		5
#define	IS_LAST 			6
#define	IS_NOTSET			7	/* for reset version */

/*
 * Pruning rules. If these grow, widen pruning_rule.
 */
#define	PRUNE_ASK_AND_ASK		0
#define	PRUNE_ASK_AND_NO		1
#define	PRUNE_YES_AND_ASK  		2
#define	PRUNE_YES_AND_NO  		3
#define	PRUNE_NO_AND_ASK  		4
#define	PRUNE_NO_AND_NO  		5

/*
 * Folder reopen rules. If these grow, widen reopen_rule.
 */
#define	REOPEN_YES_YES			0
#define	REOPEN_YES_ASK_Y		1
#define	REOPEN_YES_ASK_N		2
#define	REOPEN_YES_NO			3
#define	REOPEN_ASK_ASK_Y		4
#define	REOPEN_ASK_ASK_N		5
#define	REOPEN_ASK_NO_Y			6
#define	REOPEN_ASK_NO_N			7
#define	REOPEN_NO_NO			8

/*
 * Goto default rules.
 */
#define	GOTO_INBOX_RECENT_CLCTN		0
#define	GOTO_INBOX_FIRST_CLCTN		1
#define	GOTO_LAST_FLDR			2
#define	GOTO_FIRST_CLCTN		3
#define	GOTO_FIRST_CLCTN_DEF_INBOX	4

/*
 * Thread display styles. If these grow, widen thread_disp_rule.
 */
#define	THREAD_NONE			0
#define	THREAD_STRUCT			1
#define	THREAD_MUTTLIKE			2
#define	THREAD_INDENT_SUBJ1		3
#define	THREAD_INDENT_SUBJ2		4
#define	THREAD_INDENT_FROM1		5
#define	THREAD_INDENT_FROM2		6
#define	THREAD_STRUCT_FROM		7

/*
 * Thread index styles. If these grow, widen thread_index_rule.
 */
#define	THRDINDX_EXP			0
#define	THRDINDX_COLL			1
#define	THRDINDX_SEP			2
#define	THRDINDX_SEP_AUTO		3

/*
 * Titlebar color styles. If these grow, widen titlebar_color_style.
 */
#define	TBAR_COLOR_DEFAULT		0
#define	TBAR_COLOR_INDEXLINE		1
#define	TBAR_COLOR_REV_INDEXLINE	2

/*
 * PC-Pine update registry modes
 */
#ifdef _WINDOWS
#define UREG_NORMAL       0
#define UREG_ALWAYS_SET   1
#define UREG_NEVER_SET    2
#endif


/* for select_from_list_screen */
#define	SFL_NONE		0x000
#define	SFL_ALLOW_LISTMODE	0x001
#define	SFL_STARTIN_LISTMODE	0x002
#define	SFL_NOSELECT		0x004


/*
 * Some macros useful for threading
 */

/* Sort is a threaded sort */
#define SORT_IS_THREADED(msgmap)					\
	(mn_get_sort(msgmap) == SortThread				\
	 || mn_get_sort(msgmap) == SortSubject2)

#define SEP_THRDINDX()							\
	(ps_global->thread_index_style == THRDINDX_SEP 			\
	 || ps_global->thread_index_style == THRDINDX_SEP_AUTO)

#define COLL_THRDS()							\
	(ps_global->thread_index_style == THRDINDX_COLL)

#define THRD_AUTO_VIEW()						\
	(ps_global->thread_index_style == THRDINDX_SEP_AUTO)

/* We are threading now, pay attention to all the other variables */
#define THREADING()							\
	(!ps_global->turn_off_threading_temporarily			\
	 && SORT_IS_THREADED(ps_global->msgmap)				\
	 && (SEP_THRDINDX()						\
	     || ps_global->thread_disp_style != THREAD_NONE))

/* If we were to view the folder, we would get a thread index */
#define THRD_INDX_ENABLED()						\
	(SEP_THRDINDX()							\
	 && THREADING())

/* We are in the thread index (or would be if we weren't in an index menu) */
#define THRD_INDX()							\
	(THRD_INDX_ENABLED()						\
	 && !sp_viewing_a_thread(ps_global->mail_stream))

/* The thread command ought to work now */
#define THRD_COLLAPSE_ENABLE()						\
	(THREADING()							\
	 && !THRD_INDX()						\
	 && ps_global->thread_disp_style != THREAD_NONE)



/*
 * Folder list sort rules
 */
#define	FLD_SORT_ALPHA			0
#define	FLD_SORT_ALPHA_DIR_LAST		1
#define	FLD_SORT_ALPHA_DIR_FIRST	2


/*
 * Color styles
 */
#define	COL_NONE	0
#define	COL_TERMDEF	1
#define	COL_ANSI8	2
#define	COL_ANSI16	3


/*
 * Flags for msgline_hidden.
 *
 * MN_NONE means we should only consider messages which are or would be
 *   visible in the index view. That is, messages which are not hidden due
 *   to zooming, not hidden because they are in a collapsed part of a
 *   thread, and not hidden because we are in one thread and they are in
 *   another and the view we are using only shows one thread.
 * MN_THISTHD changes that a little bit. It considers more messages
 *   to be visible. In particular, messages in this thread which are
 *   hidden due to collapsing are considered to be visible instead of hidden.
 *   This is useful if we are viewing a message and hit Next, and want
 *   to see the next message in the thread even if it was in a
 *   collapsed thread. This only makes sense when using the separate
 *   thread index and viewing a thread. "This thread" is the thread that
 *   is being viewed, not the thread that msgno is part of. So it is the
 *   thread that has been marked MN_CHID2.
 * MN_ANYTHD adds more visibility. It doesn't matter if the message is in
 *   the same thread or if it is collapsed or not. If a message is not
 *   hidden due to zooming, then it is not hidden. Notice that ANYTHD
 *   implies THISTHD.
 */
#define MH_NONE		0x0
#define MH_THISTHD	0x1
#define MH_ANYTHD	0x2


/*
 * These are def's to help manage local, private flags pine uses
 * to maintain it's mapping table (see MSGNO_S def).  The local flags
 * are actually stored in spare bits in c-client's per-message
 * MESSAGECACHE struct.  But they're private, you ask.  Since the flags
 * are tied to the actual message (independent of the mapping), storing
 * them in the c-client means we don't have to worry about them during
 * sorting and such.  See {set,get}_lflags for more on local flags.
 *
 * MN_HIDE hides messages which are not visible due to Zooming.
 * MN_EXLD hides messages which have been filtered away.
 * MN_SLCT marks messages which have been Selected.
 * MN_COLL marks a point in the thread tree where the view has been
 *          collapsed, hiding the messages below that point
 * MN_CHID hides messages which are collapsed out of view
 * MN_CHID2 is similar to CHID and is introduced for performance reasons.
 *          When using the separate-thread-index the toplevel messages are
 *          MN_COLL and everything else is MN_CHID. However, if we view a
 *          single thread from there, instead of marking all of those top
 *          level threads MN_HIDE (or something) we change the semantics
 *          of the flags. When viewing a single thread we mark the messages
 *          of the thread with MN_CHID2 for performance reasons.
 */
#define	MN_NONE		0x0000	/* No Pine specific flags  */
#define	MN_HIDE		0x0001	/* Pine specific hidden    */
#define	MN_EXLD		0x0002	/* Pine specific excluded  */
#define	MN_SLCT		0x0004	/* Pine specific selected  */
#define	MN_COLL		0x0008	/* Pine specific collapsed */
#define	MN_CHID		0x0010	/* A parent somewhere above us is collapsed */
#define	MN_CHID2	0x0020	/* performance related */
#define	MN_USOR		0x0040	/* New message which hasn't been sorted yet */
#define	MN_STMP		0x0080	/* Temporary storage for a per-message bit */

/*
 * Macro's to help sort out how we display MIME types
 */
#define	MCD_NONE	0x00
#define	MCD_INTERNAL	0x01
#define	MCD_EXTERNAL	0x02
#define	MCD_EXT_PROMPT	0x04


/*
 * display_output_file mode flags
 */
#define	DOF_NONE	0
#define	DOF_EMPTY	1
#define	DOF_BRIEF	2


/*
 * Macros to aid hack to turn off talk permission.
 * Bit 020 is usually used to turn off talk permission, we turn off
 * 002 also for good measure, since some mesg commands seem to do that.
 */
#define	TALK_BIT		020		/* mode bits */
#define	GM_BIT			002
#define	TMD_CLEAR		0		/* functions */
#define	TMD_SET			1
#define	TMD_RESET		2
#define	allow_talk(p)		tty_chmod((p), TALK_BIT, TMD_SET)
#define	disallow_talk(p)	tty_chmod((p), TALK_BIT|GM_BIT, TMD_CLEAR)


/*
 * Macros to help set numeric pinerc variables.  Defined here are the 
 * allowed min and max values as well as the name of the var as it
 * should be displayed in error messages.
 */
#define Q_SUPP_LIMIT (4)
#define Q_DEL_ALL    (-10)
#define	SVAR_OVERLAP(ps, n, e)	strtoval((ps)->VAR_OVERLAP,		  \
					 &(n), 0, 20, 0, (e),		  \
					 "Viewer-Overlap")
#define	SVAR_MAXREMSTREAM(ps, n, e)	strtoval((ps)->VAR_MAXREMSTREAM,  \
					 &(n), 0, 15, 0, (e),		  \
					 "Max-Remote-Connections")
#define	SVAR_MARGIN(ps, n, e)	strtoval((ps)->VAR_MARGIN,		  \
					 &(n), 0, 20, 0, (e),		  \
					 "Scroll-Margin")
#define	SVAR_FILLCOL(ps, n, e)	strtoval((ps)->VAR_FILLCOL,		  \
					 &(n), 0, MAX_FILLCOL, 0, (e),	  \
					 "Composer-Wrap-Column")
#define	SVAR_QUOTE_SUPPRESSION(ps, n, e) strtoval((ps)->VAR_QUOTE_SUPPRESSION, \
					 &(n), -(Q_SUPP_LIMIT-1),	  \
					 1000, Q_DEL_ALL, (e),		  \
					 "Quote-Suppression-Threshold")
#define	SVAR_DEADLETS(ps, n, e)	strtoval((ps)->VAR_DEADLETS,		  \
					 &(n), 0, 9, 0, (e),		  \
					 "Dead-Letter-Files")
#define	SVAR_MSGDLAY(ps, n, e)	strtoval((ps)->VAR_STATUS_MSG_DELAY,	  \
					 &(n), -10, 30, 0, (e),		  \
					"Status-Message-Delay")
#define	SVAR_MAILCHK(ps, n, e)	strtoval((ps)->VAR_MAILCHECK,		  \
					 &(n), 15, 30000, 0, (e),	  \
					"Mail-Check-Interval")
#define	SVAR_MAILCHKNONCURR(ps, n, e)	strtoval((ps)->VAR_MAILCHECKNONCURR,  \
					 &(n), 15, 30000, 0, (e),	  \
					"Mail-Check-Interval-Noncurrent")
#define	SVAR_AB_HIST(ps, n, e)	strtoval((ps)->VAR_REMOTE_ABOOK_HISTORY,  \
					 &(n), 0, 100, 0, (e),		  \
					"Remote-Abook-History")
#define	SVAR_AB_VALID(ps, n, e)	strtoval((ps)->VAR_REMOTE_ABOOK_VALIDITY, \
					 &(n), -1, 30000, 0, (e),	  \
					"Remote-Abook-Validity")
#define	SVAR_USER_INPUT(ps, n, e) strtoval((ps)->VAR_USERINPUTTIMEO,	  \
					 &(n), 0, 1000, 0, (e),		  \
					"User-Input-Timeout")
#define	SVAR_TCP_OPEN(ps, n, e)	strtoval((ps)->VAR_TCPOPENTIMEO, 	  \
					 &(n), 5, 30000, 5, (e),	  \
					"Tcp-Open-Timeout")
#define	SVAR_TCP_READWARN(ps, n, e) strtoval((ps)->VAR_TCPREADWARNTIMEO,  \
					 &(n), 5, 30000, 5, (e),	  \
					"Tcp-Read-Warning-Timeout")
#define	SVAR_TCP_WRITEWARN(ps, n, e) strtoval((ps)->VAR_TCPWRITEWARNTIMEO, \
					 &(n), 5, 30000, 0, (e),	  \
					"Tcp-Write-Warning-Timeout")
#define	SVAR_TCP_QUERY(ps, n, e) strtoval((ps)->VAR_TCPQUERYTIMEO, 	  \
					 &(n), 5, 30000, 0, (e),	  \
					"Tcp-Query-Timeout")
#define	SVAR_RSH_OPEN(ps, n, e)	strtoval((ps)->VAR_RSHOPENTIMEO, 	  \
					 &(n), 5, 30000, 0, (e),	  \
					"Rsh-Open-Timeout")
#define	SVAR_SSH_OPEN(ps, n, e)	strtoval((ps)->VAR_SSHOPENTIMEO, 	  \
					 &(n), 5, 30000, 0, (e),	  \
					"Ssh-Open-Timeout")

#define	SVAR_NNTPRANGE(ps, n, e) strtolval((ps)->VAR_NNTPRANGE,		  \
					  &(n), 0L, 30000L, 0L, (e),	  \
					  "Nntp-Range")
#define	SVAR_MAILDCHK(ps, n, e)	strtolval((ps)->VAR_MAILDROPCHECK,	  \
					  &(n), 60L, 30000L, 0L, (e),	  \
					  "Maildrop-Check-Minimum")
#define	SVAR_NMW_WIDTH(ps, n, e) strtoval((ps)->VAR_NMW_WIDTH,		  \
					 &(n), 20, 170, 0, (e),		  \
					 "NewMail-Window-Width")


/*======================================================================
            Various structures that Pine uses
 ====*/

/*
 * Keeps track of selected folders between instances of
 * the folder list screen.
 */
typedef	struct name_list {
    char		*name;
    struct name_list	*next;
} STRLIST_S;


typedef struct init_err {
    int   flags, min_time, max_time;
    char *message;
} INIT_ERR_S;


typedef	struct keywords {
    char		*kw;
    char		*nick;
    struct keywords	*next;
} KEYWORD_S;


/*
 * Keeps track of display dimensions
 */
struct ttyo {
    int	screen_rows,
	screen_cols,
	header_rows,	/* number of rows for titlebar and whitespace */
	footer_rows;	/* number of rows for status and keymenu      */
};

/*
 * HEADER_ROWS is always 2.  1 for the titlebar and 1 for the
 * blank line after the titlebar.  We should probably make it go down
 * to 0 when the screen shrinks but instead we're just figuring out
 * if there is enough room by looking at screen_rows.
 * FOOTER_ROWS is either 3 or 1.  Normally it is 3, 2 for the keymenu plus 1
 * for the status line.  If the NoKeyMenu command has been given, then it is 1.
 */
#define HEADER_ROWS(X) ((X)->ttyo->header_rows)
#define FOOTER_ROWS(X) ((X)->ttyo->footer_rows)


/* (0,0) is upper left */
typedef struct screen_position {
    int row;
    int col;
} Pos;

typedef struct screen_position_list {
    Pos				  where;
    struct screen_position_list  *next;
} POSLIST_S;


typedef enum {Main, Post, None} EditWhich;

#define PVAL(v,w) ((v) ? (((w) == Main) ? (v)->main_user_val.p :	\
	                                  (v)->post_user_val.p) : NULL)
#define APVAL(v,w) ((v) ? (((w) == Main) ? &(v)->main_user_val.p :	\
	                                   &(v)->post_user_val.p) : NULL)
#define LVAL(v,w) ((v) ? (((w) == Main) ? (v)->main_user_val.l :	\
	                                  (v)->post_user_val.l) : NULL)
#define ALVAL(v,w) ((v) ? (((w) == Main) ? &(v)->main_user_val.l :	\
	                                   &(v)->post_user_val.l) : NULL)


/*
 * Code exists that is sensitive to this order.  Don't change
 * it unless you know what you're doing.
 */
typedef enum {SortSubject = 0, SortArrival, SortFrom, SortTo, 
              SortCc, SortDate, SortSize,
	      SortSubject2, SortScore, SortThread, EndofList}   SortOrder;

#define	refresh_sort(S,M,F)	sort_folder((S), (M), mn_get_sort(M), \
					    mn_get_revsort(M), (F))

/*
 * The two structs below hold all knowledge regarding
 * messages requiring special handling
 */
typedef	struct part_exception_s {
    char		     *partno;
    int			      handling;
    struct part_exception_s  *next;
} PARTEX_S;

#define	MSG_EX_DELETE	  0x0001	/* part is deleted */
#define	MSG_EX_RECENT	  0x0002
#define	MSG_EX_TESTED	  0x0004	/* filtering has been run on this msg */
#define	MSG_EX_FILTERED	  0x0008	/* msg has actually been filtered away*/
#define	MSG_EX_FILED	  0x0010	/* msg has been filed */
#define	MSG_EX_FILTONCE	  0x0020
#define	MSG_EX_FILEONCE	  0x0040	/* These last two mean that the
					   message has been filtered or filed
					   already but the filter rule was
					   non-terminating so it is still
					   possible it will get filtered
					   again. When we're done, we flip
					   these two to EX_FILTERED and
					   EX_FILED, the permanent versions. */
#define	MSG_EX_PEND_EXLD  0x0080	/* pending exclusion */
#define	MSG_EX_MANUNDEL   0x0100	/* has been manually undeleted */
#define	MSG_EX_STATECHG	  0x0200	/* state change since filtering */

/* msgno_include flags */
#define	MI_NONE		0x00
#define	MI_REFILTERING	0x01
#define	MI_STATECHGONLY	0x02
#define	MI_CLOSING	0x04


#define	THD_TOP		0x0000		/* start of an individual thread */
#define	THD_NEXT	0x0001
#define	THD_BRANCH	0x0004

typedef struct pine_thrd {
    unsigned long rawno;	/* raw msgno of this message		*/
    unsigned long thrdno;	/* thread number			*/
    unsigned long flags;
    unsigned long next;		/* msgno of first reply to us		*/
    unsigned long branch;	/* like THREADNODE branch, next replier	*/
    unsigned long parent;	/* message that this is a reply to	*/
    unsigned long nextthd;	/* next thread, only tops have this	*/
    unsigned long prevthd;	/* previous thread, only tops have this	*/
    unsigned long top;		/* top of this thread			*/
    unsigned long head;		/* head of the whole thread list	*/
} PINETHRD_S;


typedef struct list_selection {
    char                  *display_item;	/* use item if this is NULL */
    char                  *item;		/* selected value for item  */
    int                    selected;		/* is item selected or not */
    int                    flags;
    struct list_selection *next;
} LIST_SEL_S;


/*
 * Pine's private per-message data stored on the c-client's elt
 * spare pointer.
 */
typedef struct pine_elt {
    PINETHRD_S  *pthrd;
    PARTEX_S    *exceptions;
} PINELT_S;


/*
 * This is *the* struct that keeps track of the pine message number to
 * raw c-client sequence number mappings.  The mapping is necessary
 * because pine may re-sort or even hide (exclude) c-client numbers
 * from the displayed list of messages.  See mailindx.c:msgno_* and
 * the mn_* macros above for how this things gets used.  See
 * mailcmd.c:pseudo_selected for an explanation of the funny business
 * going on with the "hilited" field...
 */
typedef struct msg_nos {
    long      *select,				/* selected message array  */
	       sel_cur,				/* current interesting msg */
	       sel_cnt,				/* its size		   */
	       sel_size,			/* its size		   */
              *sort,				/* sorted array of msgno's */
               sort_size,			/* its size		   */
              *isort,				/* inverse of sort array   */
               isort_size,			/* its size		   */
	       max_msgno,			/* total messages in table */
	       nmsgs,				/* total msgs in folder    */
	       hilited,				/* holder for "current" msg*/
	       top,				/* message at top of screen*/
	       max_thrdno,
	       top_after_thrd;			/* top after thrd view     */
    SortOrder  sort_order;			/* list's current sort     */
    unsigned   reverse_sort:1;			/* whether that's reversed */
    unsigned   manual_sort:1;			/* sorted with $ command   */
    long       flagged_hid,			/* hidden count		   */
	       flagged_exld,			/* excluded count	   */
	       flagged_coll,			/* collapsed count	   */
	       flagged_chid,			/* collapsed-hidden count  */
	       flagged_chid2,			/* */
	       flagged_usor,			/* new unsorted mail	   */
	       flagged_tmp,			/* tmp flagged count	   */
	       flagged_stmp,			/* stmp flagged count	   */
	       flagged_invisible,		/* this one's different    */
	       visible_threads;			/* so is this one          */
} MSGNO_S;


#define	SRT_NON	0x0	/* None; no options set		*/
#define	SRT_VRB	0x1	/* Verbose			*/
#define	SRT_MAN	0x2	/* Sorted manually since opened	*/



/*
 * Used by pine_args to tell caller what was found;
 */
typedef struct argdata {
    enum	{aaFolder = 0, aaMore, aaURL, aaMail,
		 aaPrcCopy, aaAbookCopy} action;
    union {
	char	  *folder;
	char	  *file;
	struct {
	    STRLIST_S *addrlist;
	    PATMT     *attachlist;
	} mail;
	struct {
	    char      *local;
	    char      *remote;
	} copy;
    } data;
    char      *url;
} ARGDATA_S;


/*
 * Flags for the pipe command routines...
 */
#define	PIPE_WRITE	0x0001			/* set up pipe for reading */
#define	PIPE_READ	0x0002			/* set up pipe for reading */
#define	PIPE_NOSHELL	0x0004			/* don't exec in shell     */
#define	PIPE_USER	0x0008			/* user mode		   */
#define	PIPE_STDERR	0x0010			/* stderr to child output  */
#define	PIPE_PROT	0x0020			/* protected mode	   */
#define	PIPE_RESET	0x0040			/* reset terminal mode     */
#define	PIPE_DESC	0x0080			/* no stdio desc wrapping  */
#define	PIPE_SILENT	0x0100			/* no screen clear, etc	   */
#define PIPE_RUNNOW     0x0200           /* don't wait for child (PC-Pine) */

/*
 * stucture required for the pipe commands...
 */
typedef struct pipe_s {
    int      pid,				/* child's process id       */
	     mode,				/* mode flags used to open  */
	     timeout,				/* wait this long for child */
	     old_timeo;				/* previous active alarm    */
    SigType  (*hsig)(),				/* previously installed...  */
	     (*isig)(),				/* handlers		    */
	     (*qsig)(),
	     (*alrm)(),
	     (*chld)();
    union {
	FILE *f;
	int   d;
    }	     in;				/* input data handle	    */
    union {
	FILE *f;
	int   d;
    }	     out;				/* output data handle	    */
    char   **argv,				/* any necessary args	    */
	    *args,
	    *tmp;				/* pointer to stuff	    */
#ifdef	_WINDOWS
    char    *infile;                            /* file containing pipe's stdin  */
    char    *outfile;                           /* file containing pipe's stdout */
    char    *command;				/* command to execute */
    int      exit_code;                         /* proc rv if run right away */
    int      deloutfile;                        /* need to rm outfile at close */
#endif
} PIPE_S;


/*
 * digested form of context including pointer to the parent
 * level of hierarchy...
 */
typedef struct folder_dir {
    char	   *ref,			/* collection location     */
		   *desc,			/* Optional description	   */
		    delim,			/* dir/file delimiter      */
		    status;			/* folder data's status    */
    struct {
	char	   *user,
		   *internal;
    } view;					/* file's within dir	   */
    void           *folders;			/* folder data             */
    struct folder_dir *prev;			/* parent directory	   */
} FDIR_S;

typedef struct selected_s {
    char	*reference;	                /* location of selected	   */
    STRLIST_S   *folders;			/* list of selected	   */
    unsigned     zoomed:1;			/* zoomed state		   */
    struct selected_s  *sub;
} SELECTED_S;


/*------------------------------
    Stucture to keep track of the various folder collections being
    dealt with.
  ----*/
typedef struct context {
    FDIR_S	    *dir;			/* directory stack	   */
    char	    *context,			/* raw context string	   */
		    *server,			/* server name/parms	   */
		    *nickname,			/* user provided nickname  */
		    *label,			/* Description		   */
		    *comment,			/* Optional comment	   */
		     last_folder[MAXFOLDER+1];	/* last folder used        */
    struct {
	struct variable *v;			/* variable where defined  */
	short		 i;			/* index into config list  */
    } var;
    unsigned short   use,			/* use flags for context   */
		     d_line;			/* display line for labels */
    SELECTED_S       selected;
    struct context  *next,			/* next context struct	   */
		    *prev;			/* previous context struct */
} CONTEXT_S;

typedef struct mcap_cmd {
    char *command;                              /* command to execute      */
    int   special_handling;                     /* special os handling     */
} MCAP_CMD_S;

/*
 * The stream pool is where we keep pointers to open streams. Some of them are
 * actively being used, some are connected to a folder but aren't actively
 * in use, some are random temporary use streams left open for possible
 * re-use. Each open stream should be in the streams array, which is of
 * size nstream altogether. Streams which are not to be re-used (don't have
 * the flag SP_USEPOOL set) are in the array anyway.
 */

/*
 * Structure holds global information about the stream pool. The per-stream
 * information is stored in a PER_STREAM_S struct attached to each stream.
 */
typedef struct stream_pool {
    int          max_remstream;	/* max implicitly cached remote streams	*/
    int          nstream;	/* size of streams array 		*/
    MAILSTREAM **streams;	/* the array of streams in stream pool	*/
} SP_S;

/*
 * Pine's private per-stream data stored on the c-client's stream
 * spare pointer.
 */
typedef struct pine_per_stream_data {
    MSGNO_S      *msgmap;
    CONTEXT_S    *context;		/* context fldr was interpreted in */
    char         *fldr;			/* folder name, alloced copy   */
    unsigned long flags;
    int           ref_cnt;
    long          new_mail_count;   /* new mail since the last new_mail check */
    long          mail_since_cmd;   /* new mail since last key pressed */
    long          expunge_count;
    long          first_unseen;
    long          recent_since_visited;
    int           check_cnt;
    time_t        first_status_change;
    time_t        last_ping;		/* Keeps track of when the last    */
					/* command was. The command wasn't */
					/* necessarily a ping.             */
    time_t        last_expunged_reaper;	/* Some IMAP commands defer the    */
					/* return of EXPUNGE responses.    */
					/* This is the time of the last    */
					/* command which did not defer.    */
    time_t        last_chkpnt_done;
    time_t        last_use_time;
    time_t        last_activity;
    unsigned long saved_uid_validity;
    unsigned long saved_uid_last;
    char         *saved_cur_msg_id;
    unsigned      unsorted_newmail:1;
    unsigned      need_to_rethread:1;
    unsigned      io_error_on_stream:1;
    unsigned      mail_box_changed:1;
    unsigned      viewing_a_thread:1;
    unsigned      dead_stream:1;
    unsigned      noticed_dead_stream:1;
    unsigned      closing:1;
} PER_STREAM_S;

/*
 * Complicated set of flags for stream pool cache.
 * LOCKED, PERMLOCKED, TEMPUSE, and USEPOOL are flags stored in the stream
 * flags of the PER_STREAM_S structure.
 *
 *     SP_LOCKED -- In use, don't re-use this.
 *                  That isn't a good description of SP_LOCKED. Every time
 *                  we pine_mail_open a stream it is SP_LOCKED and a ref_cnt
 *                  is incremented. Pine_mail_close decrements the ref_cnt
 *                  and unlocks it when we get to zero.
 * SP_PERMLOCKED -- Should always be kept open, like INBOX. Right now the
 *                  only significance of this is that the expunge_and_close
 *                  won't happen if this is set (like the way INBOX works).
 *                  If a stream is PERMLOCKED it should no doubt be LOCKED
 *                  as well (it isn't done implicitly in the tests).
 *      SP_INBOX -- This stream is open on the INBOX.
 *   SP_USERFLDR -- This stream was opened by the user explicitly, not
 *                  implicitly like would happen with a save or a remote
 *                  address book, etc.
 *   SP_FILTERED -- This stream was opened by the user explicitly and
 *                  filtered.
 *    SP_TEMPUSE -- If a stream is not SP_LOCKED, that says we can re-use
 *                  it if need be but we should prefer to use another unused
 *                  slot if there is one. If a stream is marked TEMPUSE we
 *                  should consider re-using it before we consider re-using
 *                  a stream which is not LOCKED but not marked TEMPUSE.
 *                  This flag is not only stored in the PER_STREAM_S flags,
 *                  it is also an input argument to sp_stream_get.
 *                  It may make sense to mark a stream both SP_LOCKED and
 *                  SP_TEMPUSE. That way, when we close the stream it will
 *                  be SP_TEMPUSE and more re-usable than if we didn't.
 *    SP_USEPOOL -- Passed to pine_mail_open, it means to consider the
 *                  stream pool when opening and to put it into the stream
 *                  pool after we open it. If this is not set when we open,
 *                  we do an honest open and an honest close when we close.
 *
 *  These flags are input flags to sp_stream_get.
 *       SP_MATCH -- We're looking for a stream that is already open on
 *                   this mailbox. This is good if we are reopening the
 *                   same mailbox we already had opened.
 *        SP_SAME -- We're looking for a stream that is open to the same
 *                   server. For example, we might want to do a STATUS
 *                   command or a DELETE. We could use any stream that
 *                   is already open for this. Unless SP_MATCH is also
 *                   set, this will not return exact matches. (For example,
 *                   it is a bad idea to do a STATUS command on an already
 *                   selected mailbox. There may be locking problems if you
 *                   try to delete a folder that is selected...)
 *     SP_TEMPUSE -- The checking for SP_SAME streams is controlled by these
 *    SP_UNLOCKED    two flags. If SP_TEMPUSE is set then we will only match
 *                   streams which are marked TEMPUSE and not LOCKED.
 *                   If TEMPUSE is not set but UNLOCKED is, then we will
 *                   match on any same stream that is not locked. We'll choose
 *                   SP_TEMPUSE streams in preference to those that aren't
 *                   SP_TEMPUSE. If neither SP_TEMPUSE or SP_UNLOCKED is set,
 *                   then we'll consider any stream, even if it is locked.
 *                   We'll still prefer TEMPUSE first, then UNLOCKED, then any.
 *
 * Careful with the values of these flags. Some of them should really be
 * in separate name spaces, but we've combined all of them for convenience.
 * In particular, SP_USERFLDR, SP_INBOX, SP_USEPOOL, and SP_TEMPUSE are
 * all passed in the pine_mail_open flags argument, alongside OP_DEBUG and
 * friends from c-client. So they have to have different values than
 * those OP_ flags. SP_PERMLOCKED was passed at one time but isn't anymore.
 * Still, include it in the careful set. C-client reserves the bits
 * 0xff000000 for client flags.
 */

#define SP_USERFLDR	0x01000000
#define SP_INBOX	0x02000000
#define SP_USEPOOL	0x04000000
#define SP_TEMPUSE	0x08000000

#define SP_PERMLOCKED	0x10000000
#define SP_LOCKED	0x20000000

#define SP_MATCH	0x00100000
#define SP_SAME		0x00200000
#define SP_UNLOCKED	0x00400000
#define SP_FILTERED	0x00800000
#define SP_RO_OK	0x01000000	/* Readonly stream ok for SP_MATCH */


/* access value of first_unseen, but don't set it with this */
#define sp_first_unseen(stream)                                              \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->first_unseen : 0L)

/* use this to set it */
#define sp_set_first_unseen(stream,val) do{                                  \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->first_unseen = (val);}while(0)

#define sp_flags(stream)                                                     \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->flags : 0L)

#define sp_set_flags(stream,val) do{                                         \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->flags = (val);}while(0)

#define sp_ref_cnt(stream)                                                   \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->ref_cnt : 0L)

#define sp_set_ref_cnt(stream,val) do{                                       \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->ref_cnt = (val);}while(0)

#define sp_expunge_count(stream)                                             \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->expunge_count : 0L)

#define sp_set_expunge_count(stream,val) do{                                 \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->expunge_count = (val);}while(0)

#define sp_new_mail_count(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->new_mail_count : 0L)

#define sp_set_new_mail_count(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->new_mail_count = (val);}while(0)

#define sp_mail_since_cmd(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->mail_since_cmd : 0L)

#define sp_set_mail_since_cmd(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->mail_since_cmd = (val);}while(0)

#define sp_recent_since_visited(stream)                                      \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->recent_since_visited : 0L)

#define sp_set_recent_since_visited(stream,val) do{                          \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->recent_since_visited = (val);}while(0)

#define sp_check_cnt(stream)                                                 \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->check_cnt : 0L)

#define sp_set_check_cnt(stream,val) do{                                     \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->check_cnt = (val);}while(0)

#define sp_first_status_change(stream)                                      \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->first_status_change : 0L)

#define sp_set_first_status_change(stream,val) do{                          \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->first_status_change = (val);}while(0)

#define sp_last_ping(stream)                                                \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_ping : 0L)

#define sp_set_last_ping(stream,val) do{                                    \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_ping = (val);}while(0)

#define sp_last_expunged_reaper(stream)                                     \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_expunged_reaper : 0L)

#define sp_set_last_expunged_reaper(stream,val) do{                         \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_expunged_reaper = (val);}while(0)

#define sp_last_chkpnt_done(stream)                                         \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_chkpnt_done : 0L)

#define sp_set_last_chkpnt_done(stream,val) do{                             \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_chkpnt_done = (val);}while(0)

#define sp_last_use_time(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_use_time : 0L)

#define sp_set_last_use_time(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_use_time = (val);}while(0)

#define sp_last_activity(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_activity : 0L)

#define sp_set_last_activity(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_activity = (val);}while(0)

#define sp_saved_uid_validity(stream)                                        \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->saved_uid_validity : 0L)

#define sp_set_saved_uid_validity(stream,val) do{                            \
		    if(sp_data(stream) && *sp_data(stream))                  \
		     (*sp_data(stream))->saved_uid_validity = (val);}while(0)

#define sp_saved_uid_last(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->saved_uid_last : 0L)

#define sp_set_saved_uid_last(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                  \
		     (*sp_data(stream))->saved_uid_last = (val);}while(0)

#define sp_mail_box_changed(stream)                                          \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->mail_box_changed : 0L)

#define sp_set_mail_box_changed(stream,val) do{                              \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->mail_box_changed = (val);}while(0)

#define sp_unsorted_newmail(stream)                                          \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->unsorted_newmail : 0L)

#define sp_set_unsorted_newmail(stream,val) do{                              \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->unsorted_newmail = (val);}while(0)

#define sp_need_to_rethread(stream)                                          \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->need_to_rethread : 0L)

#define sp_set_need_to_rethread(stream,val) do{                              \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->need_to_rethread = (val);}while(0)

#define sp_viewing_a_thread(stream)                                          \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->viewing_a_thread : 0L)

#define sp_set_viewing_a_thread(stream,val) do{                              \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->viewing_a_thread = (val);}while(0)

#define sp_dead_stream(stream)                                               \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->dead_stream : 0L)

#define sp_set_dead_stream(stream,val) do{                                   \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->dead_stream = (val);}while(0)

#define sp_noticed_dead_stream(stream)                                       \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->noticed_dead_stream : 0L)

#define sp_set_noticed_dead_stream(stream,val) do{                           \
		if(sp_data(stream) && *sp_data(stream))                      \
		  (*sp_data(stream))->noticed_dead_stream = (val);}while(0)

#define sp_closing(stream)                                                   \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->closing : 0L)

#define sp_set_closing(stream,val) do{                                       \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->closing = (val);}while(0)

#define sp_io_error_on_stream(stream)                                        \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->io_error_on_stream : 0L)

#define sp_set_io_error_on_stream(stream,val) do{                            \
		  if(sp_data(stream) && *sp_data(stream))                    \
		    (*sp_data(stream))->io_error_on_stream = (val);}while(0)

#define sp_fldr(stream)                                                      \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->fldr : (char *) NULL)

#define sp_saved_cur_msg_id(stream)                                          \
		    ((sp_data(stream) && *sp_data(stream))                   \
		      ? (*sp_data(stream))->saved_cur_msg_id : (char *) NULL)

#define sp_context(stream)                                                   \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->context : 0L)

#define sp_set_context(stream,val) do{                                       \
		  if(sp_data(stream) && *sp_data(stream))                    \
		    (*sp_data(stream))->context = (val);}while(0)


/* do_broach_folder flags */
#define DB_NOVISIT	0x01	/* this is a preopen, not a real visit */
#define DB_FROMTAB	0x02	/* opening because of TAB command      */


/*
 * struct to help manage mail_list calls/callbacks
 */
typedef	struct mm_list_s {
    MAILSTREAM	*stream;
    unsigned     options;
    void       (*filter) PROTO((MAILSTREAM *, char *, int, long, void *,
				unsigned));
    void	*data;
} MM_LIST_S;

extern MM_LIST_S *mm_list_info;


/*
 * Macros to help users of above two structures...
 */
#define	NEWS_TEST(c)	((c) && ((c)->use & CNTXT_NEWS))
			 
#define	FOLDERS(c)	((c)->dir->folders)
#define	FLDR_NAME(X)	((X) ? ((X)->nickname ? (X)->nickname : (X)->name) :"")


/*------------------------------
   Used for displaying as well as
   keeping track of folders. 
  ----*/
typedef struct folder {
    unsigned char   name_len;			/* name length		      */
    unsigned	    isfolder:1;			/* is it a folder?	      */
    unsigned	    isdir:1;			/* is it a directory?	      */
		    /* isdual is only set if the user has the separate
		       directory and folder display option set. Otherwise,
		       we can tell it's dual use because both isfolder
		       and isdir will be set. */
    unsigned	    isdual:1;			/* dual use                   */
    unsigned        haschildren:1;              /* dir known to have children */
    unsigned        hasnochildren:1;            /* known not to have children */
    unsigned	    scanned:1;			/* scanned by c-client	      */
    unsigned	    selected:1;			/* selected by user	      */
    unsigned	    subscribed:1;		/* selected by user	      */
    unsigned long   varhash;			/* hash of var for incoming   */
    unsigned long   uidvalidity;		/* only for #move folder      */
    unsigned long   uidnext;			/* only for #move folder      */
    char	   *nickname;			/* folder's short name        */
    char	    name[1];			/* folder's name              */
} FOLDER_S;



/*------------------------------
   Used for index display
   formatting.
  ----*/
typedef enum {iNothing, iStatus, iFStatus, iIStatus,
	      iDate, iLDate, iS1Date, iS2Date, iS3Date, iS4Date, iSDate,
	      iSDateTime,
	      iDateIso, iDateIsoS,
	      iRDate, iTimezone,
	      iTime24, iTime12,
	      iCurDate, iCurDateIso, iCurDateIsoS, iCurTime24, iCurTime12,
	      iCurDay, iCurDay2Digit, iCurDayOfWeek, iCurDayOfWeekAbb,
	      iCurMon, iCurMon2Digit, iCurMonLong, iCurMonAbb,
	      iCurYear, iCurYear2Digit,
	      iLstMon, iLstMon2Digit, iLstMonLong, iLstMonAbb,
	      iLstMonYear, iLstMonYear2Digit,
	      iLstYear, iLstYear2Digit,
	      iMessNo, iAtt, iMsgID, iSubject,
	      iSubjKey, iSubjKeyInit, iKey, iKeyInit,
	      iSize, iSizeComma, iSizeNarrow, iDescripSize,
	      iNewsAndTo, iToAndNews, iNewsAndRecips, iRecipsAndNews,
	      iFromTo, iFromToNotNews, iFrom, iTo, iSender, iCc, iNews, iRecips,
	      iCurNews, iArrow,
	      iMailbox, iAddress, iInit, iCursorPos,
	      iDay2Digit, iMon2Digit, iYear2Digit,
	      iSTime, iKSize,
	      iRoleNick,
	      iScore, iDayOfWeekAbb, iDayOfWeek,
	      iDay, iDayOrdinal, iMonAbb, iMonLong, iMon, iYear} IndexColType;
typedef enum {AllAuto, Fixed, Percent, WeCalculate, Special} WidthType;
typedef enum {Left, Right} ColAdj;

typedef enum {View, MsgIndx, ThrdIndx} CmdWhere;

typedef struct index_parse_tokens {
    char        *name;
    IndexColType ctype;
    int          what_for;
} INDEX_PARSE_T;

/* these are flags for the what_for field in INDEX_PARSE_T */
#define FOR_NOTHING	0x00
#define FOR_INDEX	0x01
#define FOR_REPLY_INTRO	0x02
#define FOR_TEMPLATE	0x04		/* or for signature */
#define FOR_FILT	0x08
#define DELIM_USCORE	0x10
#define DELIM_PAREN	0x20

#define DEFAULT_REPLY_INTRO "default"

typedef struct col_description {
    IndexColType ctype;
    WidthType    wtype;
    int		 req_width;
    int		 width;
    int		 actual_length;
    ColAdj	 adjustment;
} INDEX_COL_S;



typedef long MsgNo;


struct variable {
    char *name;
    unsigned  is_obsolete:1;	/* variable read in, not written unless set */
    unsigned  is_used:1;	/* Some variables are disabled              */
    unsigned  been_written:1;
    unsigned  is_user:1;
    unsigned  is_global:1;
    unsigned  is_list:1;	/* flag indicating variable is a list       */
    unsigned  is_fixed:1;	/* sys mgr has fixed this variable          */
    unsigned  is_onlymain:1;	/* read and written from main_user_val	    */
    unsigned  is_outermost:1;	/* read and written from outermost pinerc   */
    unsigned  del_quotes:1;	/* remove double quotes                     */
    char     *descrip;		/* description                              */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } current_val;
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } main_user_val;		/* from pinerc                              */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } post_user_val;		/* from pinerc                              */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } global_val;		/* from default or pine.conf                */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } fixed_val;		/* fixed value assigned in pine.conf.fixed  */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } cmdline_val;		/* user typed as cmdline arg                */
};



/*
 * Generic name/value pair structure
 */
typedef struct nameval {
    char *name;			/* the name that goes on the screen */
    char *shortname;		/* if non-NULL, name that goes in config file */
    int   value;		/* the internal bit number */
} NAMEVAL_S;

/* use shortname if present, else regular long name */
#define S_OR_L(v)	(((v) && (v)->shortname) ? (v)->shortname : \
			  ((v) ? (v)->name : NULL))

typedef struct feature_entry {
    char       *name;
    int		id;
    HelpType	help;
    int		section;
} FEATURE_S;


typedef struct lines_to_take {
    char *printval;
    char *exportval;
    int   flags;
    struct lines_to_take *next, *prev;
} LINES_TO_TAKE;

#define	LT_NONE		0x00
#define	LT_NOSELECT	0x01


typedef struct attachment {
    char       *description;
    BODY       *body;
    unsigned	test_deferred:1;
    unsigned	can_display:4;
    unsigned	shown:1;
    unsigned	suppress_editorial:1;
    char       *number;
    char	size[25];
} ATTACH_S;


typedef struct titlebarcontainer {
    char       titlebar_line[MAX_SCREEN_COLS+1];
    COLOR_PAIR color;
} TITLE_S;



/*
 * Struct to help manage embedded urls (and anythin' else we might embed)
 */
typedef	struct handle_s {
    int		     key;		/* tag number embedded in text */
    enum	     {URL, Attach, Folder, Function} type;
    unsigned	     force_display:1;	/* Don't ask before launching */
    unsigned	     using_is_used:1;	/* bit below is being used     */
    unsigned	     is_used:1;		/* if not, remove it from list */
    union {
	struct {			/* URL corresponding to this handle */
	    char *path,			/* Actual url string */
		 *tool,			/* displaying application */
		 *name;			/* URL's NAME attribute */
	} url;				/* stuff to describe URL handle */
	ATTACH_S    *attach;		/* Attachment struct for this handle */
	struct {
	    int	       index;		/* folder's place in context's list */
	    CONTEXT_S *context;		/* description of folders */
	} f;				/* stuff to describe Folder handle */
	struct {
	    struct {			/* function and args to pass it */
		MAILSTREAM *stream;
		MSGNO_S	   *msgmap;
		long	    msgno;
	    } args;
	    void	(*f) PROTO((MAILSTREAM *, MSGNO_S *, long));
	} func;
    } h;
    POSLIST_S	    *loc;		/* list of places it exists in text */
    struct handle_s *next, *prev;	/* next and previous in the list */
} HANDLE_S ;



/*
 * Function used to dispatch locally handled URL's
 */
typedef	int	(*url_tool_t) PROTO((char *));



/*------
   A key menu has two ways to turn on and off individual items in the menu.
   If there is a null entry in the key_menu structure for that key, then
   it is off.  Also, if the passed bitmap has a zero in the position for
   that key, then it is off.  This means you can usually set all of the
   bitmaps and only turn them off if you want to kill a key that is normally
   there otherwise.
   Each key_menu is an array of keys with a multiple of 12 number of keys.
  ------*/

/*
 * Max size of a bitmap based on largest customer: feature list count
 * Problems if a screen's keymenu bitmap ever gets wider than feature list.
 */
#define BM_SIZE			((F_FEATURE_LIST_COUNT / 8)		   \
				      + ((F_FEATURE_LIST_COUNT % 8) ? 1 : 0))
#define _BITCHAR(bit)		((bit) / 8)
#define _BITBIT(bit)		(1 << ((bit) % 8))
typedef unsigned char bitmap_t[BM_SIZE];
/* is bit set? */
#define bitnset(bit,map)	(((map)[_BITCHAR(bit)] & _BITBIT(bit)) ? 1 : 0)
/* set bit */
#define setbitn(bit,map)	((map)[_BITCHAR(bit)] |= _BITBIT(bit))
/* clear bit */
#define clrbitn(bit,map)	((map)[_BITCHAR(bit)] &= ~_BITBIT(bit))
/* clear entire bitmap */
#define clrbitmap(map)		memset((void *)(map), 0, (size_t)BM_SIZE)
/* set entire bitmap */
#define setbitmap(map)		memset((void *)(map), 0xff, (size_t)BM_SIZE)
/*------
  Argument to draw_keymenu().  These are to identify which of the possibly
  multiple sets of twelve keys should be shown in the keymenu.  That is,
  a keymenu may have 24 or 36 keys, so that there are 2 or 3 different
  screens of key menus for that keymenu.  FirstMenu means to use the
  first twelve, NextTwelve uses the one after the previous one, SameTwelve
  uses the same one.
  ------*/
typedef enum {MenuNotSet = 0, FirstMenu, NextMenu, SameMenu,
	      SecondMenu, ThirdMenu, FourthMenu} OtherMenu;


struct key {
    char   *name;			/* the short name */
    char   *label;			/* the descriptive label */
    struct {
	short  cmd;
	short  nch;
	int    ch[11];
    } bind;
    KS_OSDATAVAR			/* slot for port-specific data */
    short  column;			/* menu col after formatting */
};


struct key_menu {
    unsigned int  how_many:4;		/* how many separate sets of 12      */
    unsigned int  which:4;		/* which of the sets are we using    */
    unsigned int  width:8;		/* screen width when formatting done */
    struct key	 *keys;			/* array of how_many*12 size structs */
    unsigned int  formatted_hm:4;	/* how_many when formatting done     */
    bitmap_t      bitmap;
};

/*
 * Macro to simplify instantiation of key_menu structs from key structs
 */
#define	INST_KEY_MENU(X, Y)	static struct key_menu X = \
					{sizeof(Y)/(sizeof(Y[0])*12), 0, 0, Y}

/*
 * Definitions for the various Menu Commands...
 */
#define	MC_NONE		0		/* NO command defined */
#define	MC_UNKNOWN	-1

/* Cursor/page Motion */
#define	MC_CHARUP	100
#define	MC_CHARDOWN	101
#define	MC_CHARRIGHT	102
#define	MC_CHARLEFT	103
#define	MC_GOTOBOL	104
#define	MC_GOTOEOL	105
#define	MC_GOTOSOP	106
#define	MC_GOTOEOP	107
#define	MC_PAGEUP	108
#define	MC_PAGEDN	109
#define	MC_MOUSE	110
#define MC_HOMEKEY      111
#define MC_ENDKEY       112

/* New Screen Commands */
#define	MC_HELP		500
#define	MC_QUIT		501
#define	MC_OTHER	502
#define	MC_MAIN		503
#define	MC_INDEX	504
#define	MC_VIEW_TEXT	505
#define	MC_VIEW_ATCH	506
#define	MC_FOLDERS	507
#define	MC_ADDRBOOK	508
#define	MC_RELNOTES	509
#define	MC_KBLOCK	510
#define	MC_JOURNAL	511
#define	MC_SETUP	512
#define	MC_COLLECTIONS	513
#define	MC_PARENT	514
#define	MC_ROLE		515
#define	MC_LISTMGR	516
#define	MC_THRDINDX	517

/* Commands within Screens */
#define	MC_NEXTITEM	700
#define	MC_PREVITEM	701
#define	MC_DELETE	702
#define	MC_UNDELETE	703
#define	MC_COMPOSE	704
#define	MC_REPLY	705
#define	MC_FORWARD	706
#define	MC_GOTO		707
#define	MC_TAB		708
#define	MC_WHEREIS	709
#define	MC_ZOOM		710
#define	MC_PRINTMSG	711
#define	MC_PRINTTXT	712
#define	MC_TAKE		713
#define	MC_SAVE		714
#define	MC_EXPORT	715
#define	MC_IMPORT	716
#define	MC_EXPUNGE	717
#define	MC_UNEXCLUDE	718
#define	MC_CHOICE	719
#define	MC_SELECT	720
#define	MC_SELCUR	721
#define	MC_SELALL	722
#define	MC_UNSELALL	723
#define	MC_APPLY	724
#define	MC_SORT		725
#define	MC_FULLHDR	726
#define	MC_BOUNCE	727
#define	MC_FLAG		728
#define	MC_PIPE		729
#define	MC_EXIT		730
#define	MC_PRINTALL	731
#define	MC_REPAINT	732
#define	MC_JUMP		733
#define	MC_RESIZE	734
#define	MC_FWDTEXT	735
#define	MC_SAVETEXT	736
#define	MC_ABOUTATCH	737
#define	MC_LISTMODE	738
#define	MC_RENAMEFLDR	739
#define	MC_ADDFLDR	740
#define	MC_SUBSCRIBE	741
#define	MC_UNSUBSCRIBE	742
#define	MC_ADD		743
#define	MC_TOGGLE	744
#define	MC_EDIT		745
#define	MC_ADDABOOK	746
#define	MC_DELABOOK	747
#define	MC_VIEW_ENTRY	748
#define	MC_EDITABOOK	750
#define	MC_OPENABOOK	751
#define	MC_POPUP	752
#define	MC_EXPAND	753
#define	MC_UNEXPAND	754
#define	MC_COPY		755
#define	MC_SHUFFLE	756
#define	MC_VIEW_HANDLE	757
#define	MC_NEXT_HANDLE	758
#define	MC_PREV_HANDLE	759
#define	MC_QUERY_SERV	760
#define MC_GRIPE_LOCAL  761
#define MC_GRIPE_PIC    762
#define MC_GRIPE_READ   763
#define MC_GRIPE_POST   764
#define	MC_FINISH	765
#define	MC_PRINTFLDR	766
#define	MC_OPENFLDR	767
#define	MC_EDITFILE	768
#define	MC_ADDFILE	769
#define	MC_DELFILE	770
#define	MC_CHOICEB	771
#define	MC_CHOICEC	772
#define	MC_CHOICED	773
#define	MC_CHOICEE	774
#define	MC_DEFAULT	775
#define	MC_TOGGLEB	776
#define	MC_TOGGLEC	777
#define	MC_TOGGLED	778
#define	MC_RGB1		779
#define	MC_RGB2		780
#define	MC_RGB3		781
#define MC_EXITQUERY    782
#define	MC_ADDHDR	783
#define	MC_DELHDR	784
#define	MC_PRINTER	785
#define	MC_PASSWD	786
#define	MC_CONFIG	787
#define	MC_SIG		788
#define	MC_ABOOKS	789
#define	MC_CLISTS	790
#define	MC_RULES	791
#define	MC_DIRECTORY	792
#define	MC_KOLOR	793
#define	MC_EXCEPT	794
#define	MC_REMOTE	795
#define	MC_NO_EXCEPT	796
#define	MC_YES		797
#define	MC_NO		798
#define	MC_NOT		799
#define	MC_COLLAPSE	800
#define	MC_CHK_RECENT	801


/*
 * Some standard Key/Command Bindings 
 */
#define	NULL_MENU	{NULL, NULL, {MC_NONE}, KS_NONE}
#define	HELP_MENU	{"?", "Help", \
			 {MC_HELP, 2, {'?',ctrl('G')}}, \
			 KS_SCREENHELP}
#define	OTHER_MENU	{"O", "OTHER CMDS", \
			 {MC_OTHER, 1, {'o'}}, \
			 KS_NONE}
#define	WHEREIS_MENU	{"W", "WhereIs", \
			 {MC_WHEREIS, 2, {'w',ctrl('W')}}, \
			 KS_WHEREIS}
#define	MAIN_MENU	{"M", "Main Menu", \
			 {MC_MAIN, 1, {'m'}}, \
			 KS_MAINMENU}
#define	QUIT_MENU	{"Q", "Quit Pine", \
			 {MC_QUIT, 1, {'q'}}, \
			 KS_EXIT}
#define	PREVMSG_MENU	{"P", "PrevMsg", \
			 {MC_PREVITEM, 1, {'p'}}, \
			 KS_PREVMSG}
#define	NEXTMSG_MENU	{"N", "NextMsg", \
			 {MC_NEXTITEM, 1, {'n'}}, \
			 KS_NEXTMSG}
#define	HOMEKEY_MENU	{"Hme", "FirstPage", \
			 {MC_HOMEKEY, 1, {KEY_HOME}}, \
			 KS_NONE}
#define	ENDKEY_MENU	{"End", "LastPage", \
			 {MC_ENDKEY, 1, {KEY_END}}, \
			 KS_NONE}
#define	PREVPAGE_MENU	{"-", "PrevPage", \
			 {MC_PAGEUP, 3, {'-',ctrl('Y'),KEY_PGUP}}, \
			 KS_PREVPAGE}
#define	NEXTPAGE_MENU	{"Spc", "NextPage", \
			 {MC_PAGEDN, 4, {'+',' ',ctrl('V'),KEY_PGDN}}, \
			 KS_NEXTPAGE}
#define	JUMP_MENU	{"J", "Jump", \
			 {MC_JUMP, 1, {'j'}}, \
			 KS_JUMPTOMSG}
#define	FWDEMAIL_MENU	{"F", "Fwd Email", \
			{MC_FWDTEXT,1,{'f'}}, \
			 KS_FORWARD}
#define	PRYNTMSG_MENU	{"%", "Print", \
			 {MC_PRINTMSG,1,{'%'}}, \
			 KS_PRINT}
#define	PRYNTTXT_MENU	{"%", "Print", \
			 {MC_PRINTTXT,1,{'%'}}, \
			 KS_PRINT}
#define	SAVE_MENU	{"S", "Save", \
			 {MC_SAVE,1,{'s'}}, \
			 KS_SAVE}
#define	EXPORT_MENU	{"E", "Export", \
			 {MC_EXPORT, 1, {'e'}}, \
			 KS_EXPORT}
#define	COMPOSE_MENU	{"C", "Compose", \
			 {MC_COMPOSE,1,{'c'}}, \
			 KS_COMPOSER}
#define	RCOMPOSE_MENU	{"#", "Role", \
			 {MC_ROLE,1,{'#'}}, \
			 KS_NONE}
#define	DELETE_MENU	{"D", "Delete", \
			 {MC_DELETE,2,{'d',KEY_DEL}}, \
			 KS_DELETE}
#define	UNDELETE_MENU	{"U", "Undelete", \
			 {MC_UNDELETE,1,{'u'}}, \
			 KS_UNDELETE}
#define	REPLY_MENU	{"R", "Reply", \
			 {MC_REPLY,1,{'r'}}, \
			 KS_REPLY}
#define	FORWARD_MENU	{"F", "Forward", \
			 {MC_FORWARD,1,{'f'}}, \
			 KS_FORWARD}
#define	LISTFLD_MENU	{"L", "ListFldrs", \
			 {MC_COLLECTIONS,1,{'l'}}, \
			 KS_FLDRLIST}
#define	INDEX_MENU	{"I", "Index", \
			 {MC_INDEX,1,{'i'}}, \
			 KS_FLDRINDEX}
#define	GOTO_MENU	{"G", "GotoFldr", \
			 {MC_GOTO,1,{'g'}}, \
			 KS_GOTOFLDR}
#define	TAKE_MENU	{"T", "TakeAddr", \
			 {MC_TAKE,1,{'t'}}, \
			 KS_TAKEADDR}
#define	FLAG_MENU	{"*", "Flag", \
			 {MC_FLAG,1,{'*'}}, \
			 KS_FLAG}
#define	PIPE_MENU	{"|", "Pipe", \
			 {MC_PIPE,1,{'|'}}, \
			 KS_NONE}
#define	BOUNCE_MENU	{"B", "Bounce", \
			 {MC_BOUNCE,1,{'b'}}, \
			 KS_BOUNCE}
#define	HDRMODE_MENU	{"H", "HdrMode", \
			 {MC_FULLHDR,1,{'h'}}, \
			 KS_HDRMODE}
#define	TAB_MENU	{"Tab", "NextNew", \
			 {MC_TAB,1,{TAB}}, \
			 KS_NONE}


#define USER_INPUT_TIMEOUT(ps) ((ps->hours_to_timeout > 0) && \
  ((time(0) - time_of_last_input) > 60*60*(ps->hours_to_timeout)))


#ifdef	ENABLE_LDAP
/*
 * This is used to consolidate related information about a server. This
 * information is all stored in the ldap-servers variable, per server.
 */
typedef struct ldap_serv {
    char	*serv,		/* Server name			*/
		*base,		/* Search base			*/
		*cust,		/* Custom search filter		*/
		*nick,		/* Nickname			*/
		*mail,		/* Backup email address		*/
		*mailattr,	/* "Mail" attribute name	*/
		*snattr,	/* "Surname" attribute name	*/
		*gnattr,	/* "Givenname" attribute name	*/
		*cnattr;	/* "CommonName" attribute name	*/
    int		 port,		/* Port number			*/
		 time,		/* Time limit			*/
		 size,		/* Size limit			*/
		 impl,		/* Use implicitly feature	*/
		 rhs,		/* Lookup contents feature	*/
		 ref,		/* Save by reference feature	*/
		 nosub,		/* Disable space sub feature	*/
		 ldap_v3_ok,	/* Use ldap v3 even though no UTF-8 */
		 type,		/* Search type (surname...)	*/
		 srch,		/* Search rule (contains...)	*/
		 scope;		/* Scope of search (base...)	*/
} LDAP_SERV_S;

/*
 * Structures to control the LDAP address selection screen
 */
typedef struct ldap_serv_results {
    LDAP                      *ld;		/* LDAP handle */
    LDAPMessage               *res;		/* LDAP search result */
    LDAP_SERV_S               *info_used;
    char                      *serv;
    struct ldap_serv_results  *next;
} LDAP_SERV_RES_S;

typedef struct addr_choose {
    LDAP_SERV_RES_S *res_head;
    char            *title;
    LDAP            *selected_ld;	/* from which ld was entry selected */
    LDAPMessage     *selected_entry;	/* which entry was selected */
    LDAP_SERV_S     *info_used;
    char            *selected_serv;
} ADDR_CHOOSE_S;

/*
 * How the LDAP lookup should work.
 */
typedef	enum {AlwaysDisplay,
	      AlwaysDisplayAndMailRequired,
    	      DisplayIfTwo,
	      DisplayIfOne,
	      DisplayForURL
	      } LDAPLookupStyle;

#define	LDAP_TYPE_CN		0
#define	LDAP_TYPE_SUR		1
#define	LDAP_TYPE_GIVEN		2
#define	LDAP_TYPE_EMAIL		3
#define	LDAP_TYPE_CN_EMAIL	4
#define	LDAP_TYPE_SUR_GIVEN	5
#define	LDAP_TYPE_SEVERAL	6


#define	LDAP_SRCH_CONTAINS	0
#define	LDAP_SRCH_EQUALS	1
#define	LDAP_SRCH_BEGINS	2
#define	LDAP_SRCH_ENDS		3

#define	DEF_LDAP_TYPE		6
#define	DEF_LDAP_SRCH		2
#define	DEF_LDAP_TIME		30
#define	DEF_LDAP_SIZE		0
#define	DEF_LDAP_SCOPE		LDAP_SCOPE_SUBTREE
#define	DEF_LDAP_MAILATTR	"mail"
#define	DEF_LDAP_SNATTR		"sn"
#define	DEF_LDAP_GNATTR		"givenname"
#define	DEF_LDAP_CNATTR		"cn"

#endif	/* ENABLE_LDAP */


/*
 * used to store system derived user info
 */
typedef struct user_info {
    char *homedir;
    char *login;
    char *fullname;
} USER_S;

typedef int (*percent_done_t)();    /* returns %done for progress status msg */

/* used to fake alarm syscall on systems without it */
#ifndef	ALARM_BLIP
#define ALARM_BLIP()
#endif


/*
 * Printing control structure
 */
typedef struct print_ctrl {
#ifndef	DOS
    PIPE_S	*pipe;		/* control struct for pipe to write text */
    FILE	*fp;		/* file pointer to write printed text into */
    char	*result;	/* file containing print command's output */
#endif
#ifdef	OS2
    int		ispipe;
#endif
    int		err;		/* bit indicating something went awry */
} PRINT_S;


/*
 * Child posting control structure
 */
typedef struct post_s {
    int		pid;		/* proc id of child doing posting */
    int		status;		/* child's exit status */
    char       *fcc;		/* fcc we may have copied */
} POST_S;


/*
 * This structure is used to contain strings which are matched against
 * header fields. The match is a simple substring match. The match is
 * an OR of all the patterns in the PATTERN_S list. That is,
 * substring1_matches OR substring2_matches OR substring3_matches.
 * If not is set in the _head_ of the PATTERN_S, it is a NOT of the
 * whole pattern, that is,
 * NOT (substring1_matches OR substring2_matches OR substring3_matches).
 * The not variable is not meaningful except in the head member of the
 * PATTERN_S list.
 */
typedef struct pattern_s {
    int               not;		/* NOT of whole pattern */
    char             *substring;
    struct pattern_s *next;
} PATTERN_S;

/*
 * List of these is a list of arbitrary freetext headers and patterns.
 * This may be part of a pattern group.
 * The isemptyval bit is to keep track of the difference between an arb
 * header with no value set and one with the empty value "" set. For the
 * other builtin headers this difference is kept track of by whether or
 * not the header is in the config file at all or not. Here we want to
 * be able to add a header to the config file without necessarily giving
 * it a value.
 */
typedef struct arbhdr_s {
    char            *field;
    PATTERN_S       *p;
    int              isemptyval;
    struct arbhdr_s *next;
} ARBHDR_S;

/*
 * A list of intervals of integers.
 */
typedef struct intvl_s {
    long            imin, imax;
    struct intvl_s *next;
} INTVL_S;

/*
 * A Pattern group gives characteristics of an envelope to match against. Any of
 * the characteristics (to, from, ...) which is non-null must match for the
 * whole thing to be considered a match. That is, it is an AND of all the
 * non-null members.
 */
typedef struct patgrp_s {
    char      *nick;
    char      *comment;		/* for user, not used for anything */
    PATTERN_S *to,
	      *from,
	      *sender,
	      *cc,
	      *recip,
	      *partic,
	      *news,
	      *subj,
	      *alltext,
	      *bodytext,
	      *keyword,
	      *charsets;
    STRLIST_S *charsets_list;	/* used for efficiency, computed from charset */
    ARBHDR_S  *arbhdr;		/* list of arbitrary hdrnames and patterns */
    int        fldr_type;	/* see FLDR_* below			*/
    PATTERN_S *folder;		/* folder if type FLDR_SPECIFIC		*/
    int        abookfrom;	/* see AFRM_* below			*/
    PATTERN_S *abooks;
    int        do_score;
    INTVL_S   *score;
    int        do_age;
    INTVL_S   *age;		/* ages are in days			*/
    int        do_size;
    INTVL_S   *size;
    int        age_uses_sentdate; /* on or off				*/
    int        do_cat;
    char     **category_cmd;
    INTVL_S   *cat;
    long       cat_lim;		/* -1 no limit  0 only headers		*/
    int        bogus;		/* patgrp contains unknown stuff	*/
    int        stat_new,	/* msg status is New (Unseen)		*/
	       stat_rec,	/* msg status is Recent			*/
	       stat_del,	/* msg status is Deleted		*/
	       stat_imp,	/* msg is flagged Important		*/
	       stat_ans,	/* msg is flagged Answered		*/
	       stat_8bitsubj,	/* subject contains 8bit chars		*/
	       stat_bom,	/* this is first pine run of the month	*/
	       stat_boy;	/* this is first pine run of the year	*/
} PATGRP_S;

#define	FLDR_ANY	0
#define	FLDR_NEWS	1
#define	FLDR_EMAIL	2
#define	FLDR_SPECIFIC	3

#define	FLDR_DEFL	FLDR_EMAIL

#define	AFRM_EITHER	0
#define	AFRM_YES	1
#define	AFRM_NO		2
#define	AFRM_SPEC_YES	3
#define	AFRM_SPEC_NO	4

#define	AFRM_DEFL	AFRM_EITHER

#define	FILTER_STATE	0
#define	FILTER_KILL	1
#define	FILTER_FOLDER	2

/*
 * For the Status parts of a PATGRP_S. For example, stat_del is Deleted
 * status. User sets EITHER means they don't care, it always matches.
 * YES means it must be deleted to match. NO means it must not be deleted.
 */
#define	PAT_STAT_EITHER		0  /* we don't care which, yes or no	*/
#define	PAT_STAT_YES		1  /* yes, this status is true		*/
#define	PAT_STAT_NO		2  /* no, this status is not true	*/

/*
 * For the State setting part of a filter action
 */
#define	ACT_STAT_LEAVE		0  /* leave msg state alone		*/
#define	ACT_STAT_SET		1  /* set this part of msg state	*/
#define	ACT_STAT_CLEAR		2  /* clear this part of msg state	*/

typedef struct action_s {
    unsigned	 is_a_role:1;	/* this is a role action		*/
    unsigned	 is_a_incol:1;	/* this is an index color action	*/
    unsigned	 is_a_score:1;	/* this is a score setting action	*/
    unsigned	 is_a_filter:1;	/* this is a filter action		*/
    unsigned	 is_a_other:1;	/* this is a miscellaneous action	*/
    unsigned	 bogus:1;	/* action contains unknown stuff	*/
    unsigned	 been_here_before:1;  /* inheritance loop prevention	*/
	    /* --- These are for roles --- */
    ADDRESS	*from;		/* value to set for From		*/
    ADDRESS	*replyto;	/* value to set for Reply-To		*/
    char       **cstm;		/* custom headers			*/
    char       **smtp;		/* custom SMTP server for this role	*/
    char       **nntp;		/* custom NNTP server for this role	*/
    char	*fcc;		/* value to set for Fcc			*/
    char	*litsig;	/* value to set Literal Signature	*/
    char	*sig;		/* value to set for Sig File		*/
    char	*template;	/* value to set for Template		*/
    char	*nick;		/* value to set for Nickname		*/
    int		 repl_type;	/* see ROLE_REPL_* below		*/
    int		 forw_type;	/* see ROLE_FORW_* below		*/
    int		 comp_type;	/* see ROLE_COMP_* below		*/
    char	*inherit_nick;	/* pattern we inherit actions from	*/
	    /* --- This is for indexcoloring --- */
    COLOR_PAIR  *incol;		/* colors for index line		*/
	    /* --- This is for scoring --- */
    long         scoreval;
	    /* --- These are for filtering --- */
    int	 	 kill;
    long	 state_setting_bits;
    PATTERN_S	*keyword_set;	/* set these keywords			*/
    PATTERN_S	*keyword_clr;	/* clear these keywords			*/
    PATTERN_S	*folder;	/* folders to recv. filtered mail	*/
    int          move_only_if_not_deleted;	/* on or off		*/
    int          non_terminating;		/* on or off		*/
	    /* --- These are for other --- */
	      /* sort order of folder */
    unsigned     sort_is_set:1;
    SortOrder	 sortorder;	/* sorting order			*/
    int          revsort;	/* whether or not to reverse sort	*/
	      /* Index format of folder */
    char	*index_format;
    unsigned     startup_rule;
} ACTION_S;

/* flags for first_pattern..., set_role_from_msg, and confirm_role() */
#define	PAT_CLOSED	0x00000000	/* closed                            */
#define	PAT_OPENED	0x00000001	/* opened successfully		     */
#define	PAT_OPEN_FAILED	0x00000002
#define	PAT_USE_CURRENT	0x00000010	/* use current_val to set up pattern */
#define	PAT_USE_MAIN	0x00000040	/* use main_user_val		     */
#define	PAT_USE_POST	0x00000080	/* use post_user_val		     */
#define ROLE_COMPOSE	0x00000100	/* roles with compose value != NO   */
#define ROLE_REPLY	0x00000200	/* roles with reply value != NO	    */
#define ROLE_FORWARD	0x00000400	/* roles with forward value != NO   */
#define ROLE_INCOL	0x00000800	/* patterns with non-Normal colors  */
#define ROLE_SCORE	0x00001000	/* patterns with non-zero scorevals */
#define ROLE_DEFAULTOK	0x00002000	/* ok to use default role w confirm */
#define ROLE_DO_ROLES	0x00010000	/* role patterns		    */
#define ROLE_DO_INCOLS	0x00020000	/* index line color patterns	    */
#define ROLE_DO_SCORES	0x00040000	/* set score patterns		    */
#define	ROLE_DO_FILTER	0x00080000	/* filter patterns		    */
#define	ROLE_DO_OTHER	0x00100000	/* miscellaneous patterns	    */
#define	ROLE_OLD_PAT	0x00200000	/* old patterns variable            */
#define	ROLE_OLD_FILT	0x00400000	/* old patterns-filters variable    */
#define	ROLE_OLD_SCORE	0x00800000	/* old patterns-scores variable     */
#define ROLE_CHANGES	0x01000000	/* start editing with changes
					   already registered */

#define PAT_OPEN_MASK	0x0000000f
#define PAT_USE_MASK	0x000000f0
#define ROLE_MASK	0x00ffff00

#define	ROLE_REPL_NO		0  /* never use for reply		 */
#define	ROLE_REPL_YES		1  /* use for reply with confirmation	 */
#define	ROLE_REPL_NOCONF	2  /* use for reply without confirmation */
#define	ROLE_FORW_NO		0  /* ... forward ...			 */
#define	ROLE_FORW_YES		1
#define	ROLE_FORW_NOCONF	2
#define	ROLE_COMP_NO		0  /* ... compose ...			 */
#define	ROLE_COMP_YES		1
#define	ROLE_COMP_NOCONF	2

#define	ROLE_REPL_DEFL		ROLE_REPL_YES	/* default reply value */
#define	ROLE_FORW_DEFL		ROLE_FORW_YES	/* default forward value */
#define	ROLE_COMP_DEFL		ROLE_COMP_NO	/* default compose value */
#define	ROLE_NOTAROLE_DEFL	ROLE_COMP_NO

#define INTVL_INF	  (2147483646L)
#define INTVL_UNDEF	  (INTVL_INF + 1L)
#define SCORE_UNDEF	  INTVL_UNDEF
#define SCORE_MIN	  (-100)
#define SCORE_MAX	  (100)
#define SCOREUSE_GET	  0x000
#define SCOREUSE_INVALID  0x001	/* will recalculate scores_in_use next time */
#define SCOREUSE_ROLES    0x010	/* scores are used for roles                */
#define SCOREUSE_INCOLS   0x020	/* scores are used for index line colors    */
#define SCOREUSE_FILTERS  0x040	/* scores are used for filters              */
#define SCOREUSE_OTHER    0x080	/* scores are used for miscellaneous stuff  */
#define SCOREUSE_INDEX    0x100	/* scores are used in index-format          */
#define SCOREUSE_STATEDEP 0x200	/* scores depend on message state           */

/*
 * A message is compared with a pattern group to see if it matches.
 * If it does match, then there are actions which are taken.
 */
typedef struct pat_s {
    PATGRP_S          *patgrp;
    ACTION_S          *action;
    struct pat_line_s *patline;		/* pat_line that goes with this pat */
    char              *raw;
    unsigned           inherit:1;
    struct pat_s      *next;
    struct pat_s      *prev;
} PAT_S;

typedef	enum {TypeNotSet = 0, Literal, File, Inherit} PAT_TYPE;

/*
 * There's one of these for each line in the pinerc variable.
 * Normal type=Literal patterns have a patline with both first and last
 * pointing to the pattern. Type File has one patline for the file and first
 * and last point to the first and last patterns in the file.
 * The patterns aren't linked into one giant list, the patlines are.
 * To traverse all the patterns you have to go through the patline list
 * and then for each patline go from first to last through the patterns.
 * That's what next_pattern and friends do.
 */
typedef struct pat_line_s {
    PAT_TYPE           type;
    PAT_S             *first;	/* 1st pattern in list belonging to this line */
    PAT_S             *last;
    char              *filename; /* If type File, the filename */
    char              *filepath;
    unsigned	       readonly:1;
    unsigned	       dirty:1;	/* needs to be written back to storage */
    struct pat_line_s *next;
    struct pat_line_s *prev;
} PAT_LINE_S;

typedef struct pat_handle {
    PAT_LINE_S *patlinehead;	/* list of in-core, parsed pat lines */
    unsigned    dirtypinerc:1;	/* needs to be written */
} PAT_HANDLE;

typedef struct pat_state {
    long        rflags;
    int         cur_rflag_num;
    PAT_LINE_S *patlinecurrent;
    PAT_S      *patcurrent;	/* current pat within patline */
} PAT_STATE;

#define PATTERN_MAGIC     "P#Pats"
#define PATTERN_FILE_VERS "01"


typedef struct spec_color_s {
    int   inherit;	/* this isn't a color, it is INHERIT */
    char *spec;
    char *fg;
    char *bg;
    PATTERN_S *val;
    struct spec_color_s *next;
} SPEC_COLOR_S;


/*
 * Message Reply control structure
 */
typedef struct reply_s {
    unsigned int   flags:4;	/* how to interpret handle field */
    char	  *mailbox;	/* mailbox handles are valid in */
    char	  *origmbox;	/* above is canonical name, this is orig */
    char	  *prefix;	/* string to prepend reply-to text */
    char	  *orig_charset;
    union {
	long	   pico_flags;	/* Flags to manage pico initialization */
	struct {		/* UID information */
	    unsigned long  validity;	/* validity token */
	    unsigned long *msgs;	/* array of reply'd to msgs */
	} uid;
    } data;
} REPLY_S;

#define	REPLY_PSEUDO	1
#define	REPLY_FORW	2	/* very similar to REPLY_PSEUDO */
#define	REPLY_MSGNO	3
#define	REPLY_UID	4

/*
 * Flag definitions to help control reply header building
 */
#define	RSF_NONE		0x00
#define	RSF_FORCE_REPLY_TO	0x01
#define	RSF_QUERY_REPLY_ALL	0x02
#define	RSF_FORCE_REPLY_ALL	0x04

/*
 * Flag definitions to help build forwarded bodies
 */
#define	FWD_NONE	0
#define	FWD_ANON	1
#define	FWD_NESTED	2

/*
 * Flag definitions to control composition of forwarded subject
 */
#define FS_NONE           0
#define FS_CONVERT_QUOTES 1

/*
 * Cursor position when resuming postponed message.
 */
typedef struct redraft_pos_s {
    char	  *hdrname;	/* header field name, : if in body */
    long	   offset;	/* offset into header or body */
} REDRAFT_POS_S;

typedef enum {CharStarStar, CharStar, FileStar,
		TmpFileStar, PicoText, PipeStar} SourceType;

/*
 * typedef used by storage object routines
 */

typedef struct store_object {
    unsigned char *dp;		/* current position in data		 */
    unsigned char *eod;		/* end of current data			 */
    void	  *txt;		/* container's data			 */
    unsigned char *eot;		/* end of space alloc'd for data	 */
    int  (*writec) PROTO((int, struct store_object *));
    int  (*readc) PROTO((unsigned char *, struct store_object *));
    int  (*puts) PROTO((struct store_object *, char *));
    fpos_t	   used;	/* amount of object in use		 */
    char          *name;	/* optional object name			 */
    SourceType     src;		/* what we're copying into		 */
    short          flags;	/* flags relating to object use		 */
} STORE_S;

#define	so_writec(c, so)	((*(so)->writec)((c), (so)))
#define	so_readc(c, so)		((*(so)->readc)((c), (so)))
#define	so_puts(so, s)		((*(so)->puts)((so), (s)))

typedef enum {Loc, RemImap} RemType;

/*
 * For flags below. Also for address book flags in struct adrbk. Some of
 * these flags are shared so they need to be in the same namespace.
 */
#define DEL_FILE	0x00001	/* remove addrbook file in adrbk_close     */
#define DEL_HASHFILE	0x00002	/* remove abook hashfile in adrbk_close    */
#define DO_REMTRIM	0x00004	/* trim remote data if needed and possible */
#define USE_OLD_CACHE	0x00008	/* using cache copy, couldn't check if old */
#define REM_OUTOFDATE	0x00010	/* remote data changed since cached        */
#define NO_STATUSCMD	0x00020	/* imap server doesn't have status command */
#define NO_META_UPDATE	0x00040	/* don't try to update metafile            */
#define NO_PERM_CACHE	0x00080	/* remove temp cache files when done       */
#define NO_FILE		0x00100	/* use memory (sonofile) instead of a file */
#define DERIVE_CACHE	0x00200	/* derive cache name from remote name      */
#define FILE_OUTOFDATE	0x00400	/* addrbook file discovered out of date    */
#define OUT_OF_SORTS	0x00800	/* addrbook sort order is messed up        */
#define FIRST_CHANGE	0x01000	/* first addrbook change this session      */
#define COOKIE_ONE_OK	0x02000	/* cookie with old value of one is ok      */
#define USER_SAID_NO	0x04000	/* user said no when asked about cookie    */
#define USER_SAID_YES	0x08000	/* user said yes when asked about cookie   */
#define BELIEVE_CACHE	0x10000	/* user said yes when asked about cookie   */
#define REMUPDATE_DONE	0x20000	/* we have written to remote data          */

/* Remote data folder bookkeeping */
typedef struct remote_data {
    RemType      type;
    char        *rn;		/* remote name (name of folder)              */
    char        *lf;		/* name of local file                        */
    STORE_S	*sonofile;	/* storage object which takes place of lf    */
    AccessType   access;	/* of remote folder                          */
    time_t       last_use;	/* when remote was last accessed             */
    time_t       last_valid_chk;/* when remote valid check was done          */
    time_t       last_local_valid_chk;
    STORE_S	*so;		/* storage object to use                     */
    char         read_status;	/* R for readonly                            */
    unsigned long flags;
    unsigned long cookie;
    union type_specific_data {
      struct imap_remote_data {
	char         *special_hdr;	/* header name for this type folder  */
	MAILSTREAM   *stream;		/* stream to use for remote folder   */
	char	     *chk_date;		/* Date of last message              */
	unsigned long chk_nmsgs;	/* Number of messages in folder      */
	unsigned long shouldbe_nmsgs;	/* Number which should be in folder  */
	unsigned long uidvalidity;	/* UIDVALIDITY of folder             */
	unsigned long uidnext;		/* UIDNEXT of folder                 */
	unsigned long uid;		/* UID of last message in folder     */
      }i;
    }t;
} REMDATA_S;

typedef struct pinerc_line {
  char *line;
  struct variable *var;
  unsigned int  is_var:1;
  unsigned int  is_quoted:1;
  unsigned int  obsolete_var:1;
} PINERC_LINE;

/*
 * Each pinerc has one of these.
 */
typedef struct pinerc_s {
    RemType           type;	/* type of pinerc, remote or local	*/
    char	     *name;	/* file name or remote name		*/
    REMDATA_S 	     *rd;	/* remote data structure		*/
    time_t	      pinerc_written;
    unsigned	      readonly:1;
    unsigned	      outstanding_pinerc_changes:1;
    unsigned	      quit_to_edit:1;
    PINERC_LINE	     *pinerc_lines;
} PINERC_S;


typedef enum {ParsePers, ParsePersPost, ParseGlobal, ParseFixed} ParsePinerc;


/*
 * Hex conversion aids
 */
#define HEX_ARRAY	"0123456789ABCDEF"
#define	HEX_CHAR1(C)	HEX_ARRAY[((C) & 0xf0) >> 4]
#define	HEX_CHAR2(C)	HEX_ARRAY[(C) & 0xf]




/*------------------------------
  Structure to pass optionally_enter to tell it what keystrokes
  are special
  ----*/

typedef struct esckey {
    int  ch;
    int  rval;
    char *name;
    char *label;
} ESCKEY_S;


struct date {
    int	 sec, minute, hour, day, month, 
	 year, hours_off_gmt, min_off_gmt, wkday;
};


/*------------------------------
  Structures and enum used to expand the envelope structure to
  support user defined headers. PINEFIELDs are sort of used for two
  different purposes. The main use is to store information about headers
  in pine_send. There is a pf for every header. It is also used for the
  related task of parsing the customized-hdrs and role->cstm headers and
  storing information about those.
  ----*/

typedef enum {FreeText, Address, Fcc,
	      Attachment, Subject, TypeUnknown} FieldType;
typedef enum {NoMatch = 0,		/* no match for this header */
	      UseAsDef=1,		/* use only if no value set yet */
	      Combine=2,		/* combine if News, To, Cc, Bcc, else
					   replace existing value */
	      Replace=3} CustomType;	/* replace existing value */

typedef struct pine_field {
    char     *name;			/* field's literal name		    */
    FieldType type;			/* field's type			    */
    unsigned  canedit:1;		/* allow editing of this field	    */
    unsigned  writehdr:1;		/* write rfc822 header for field    */
    unsigned  localcopy:1;		/* copy to fcc or postponed	    */
    unsigned  rcptto:1;			/* send to this if type Address	    */
    /* the next three fields are used only for customized-hdrs and friends */
    unsigned  standard:1;		/* this hdr already in pf_template  */
    CustomType cstmtype;		/* for customized-hdrs and r->cstm  */
    char     *val;			/* field's config'd value	    */
    ADDRESS **addr;			/* used if type is Address	    */
    char     *scratch;			/* scratch pointer for Address type */
    char    **text;			/* field's free text form	    */
    char     *textbuf;			/* need to free this when done	    */
    struct headerentry *he;		/* he that goes with this field, a  */
					/*   pointer into headerentry array */
    struct pine_field *next;		/* next pine field		    */
} PINEFIELD;


typedef struct {
    ENVELOPE   *env;		/* standard c-client envelope		*/
    PINEFIELD  *local;		/* this is all of the headers		*/
    PINEFIELD  *custom;		/* ptr to start of custom headers	*/
    PINEFIELD **sending_order;	/* array giving writing order of hdrs	*/
} METAENV;

/*
 * Return values for check_address()
 */
#define CA_OK	  0		/* Address is OK                           */
#define CA_EMPTY  1		/* Address is OK, but no deliverable addrs */
#define CA_BAD   -1		/* Address is bogus                        */

/*
 * This is the structure that the builders impose on the private data
 * that is pointed to by the bldr_private pointer in each header entry.
 *
 * The bldr_private pointer points to a PrivateTop which consists of two
 * parts, whose purposes are independent:
 *   encoded -- This is used to preserve the charset information for
 *                addresses in this entry. For example, if the user types
 *                in a nickname which has a charset in it, the encoded
 *                version containing the charset information is saved here
 *                along with a checksum of the text that created it (the
 *                line containing the nickname).
 *         etext -- Pointer to the encoded text.
 *     chksumlen -- Length of string that produced the etext.
 *     chksumval -- Checksum of that string.
 *         The string that produced the etext is just the displayed
 *         value of the entry, not the nickname before it was expanded.
 *         That's so we can check on the next builder call to see if it
 *         was changed or not. Appending or prepending more addresses to
 *         what's there will work, the etext from the old contents will
 *         be combined with the etext from the appended stuff. (The check
 *         is for appending or prepending so appending AND prepending all
 *         at once won't work, charset will be lost.) If the middle of the
 *         text is edited the charset is lost. If text is removed, the
 *         charset is lost.
 *
 *  affector -- This is for entries which affect the contents of other
 *                fields. For example, the Lcc field affects what goes in
 *                the To and Fcc fields, and the To field affects what goes
 *                in the Fcc field.
 *           who -- Who caused this field to be set last.
 *     chksumlen -- Length of string in the "who" field that caused the effect.
 *     chksumval -- Checksum of that string.
 *         The string that is being checksummed is the one that is displayed
 *         in the field doing the affecting. So, for the affector in the
 *         Fcc headerentry, the who might point to either To or Lcc and
 *         then the checksummed string would be either the To or Lcc displayed
 *         string. The purpose of the affector is to remember that the
 *         affected field was set from an address book entry that is no
 *         longer identifiable once it is expanded. For example, if a list
 *         is entered into the Lcc field, then the To field gets the list
 *         fullname and the fcc field gets the fcc entry for the list. If
 *         we move out of the Lcc field and back in and call the builder
 *         again, the list has been expanded and we can't tell (except for
 *         the affector) that the same list is what caused the results.
 *         Same for the To field. A nickname entered there will cause the
 *         fcc of that nickname to go in the Fcc field and the affector
 *         will cause it to stick as long as the To field is only appended to.
 *
 * It may seem a little strange that the PrivateAffector doesn't have a text
 * field. The reason is that the text is actually displayed in that field
 * and so is contained in the entry itself, unlike the PrivateEncoded
 * which has etext which is not displayed and is only used when we go to
 * send the mail.
 */

typedef enum {BP_Unset, BP_To, BP_Lcc} WhoSetUs;

typedef struct private_affector {
    WhoSetUs who;
    int      cksumlen;
    unsigned long cksumval;
} PrivateAffector;

typedef struct private_encoded {
    char    *etext;
    int      cksumlen;
    unsigned long cksumval;
} PrivateEncoded;

typedef struct private_top {
    PrivateEncoded  *encoded;
    PrivateAffector *affector;
} PrivateTop;


typedef enum {OpenFolder, SaveMessage, FolderMaint, GetFcc,
		Subscribe, PostNews} FolderFun;
typedef enum {MsgIndex, MultiMsgIndex, ZoomIndex, ThreadIndex} IndexType;
typedef enum {TitleBarNone = 0, FolderName, MessageNumber, MsgTextPercent,
		TextPercent, FileTextPercent, ThrdIndex,
		ThrdMsgNum, ThrdMsgPercent} TitleBarType;
typedef enum {GoodTime = 0, BadTime, VeryBadTime, DoItNow} CheckPointTime;
typedef enum {InLine, QStatus} DetachErrStyle;

/*
 * Macro to help with new mail check timing...
 */
#define	NM_TIMING(X)	(((X)==NO_OP_IDLE) ? GoodTime : \
                                (((X)==NO_OP_COMMAND) ? BadTime : VeryBadTime))
#define	NM_NONE		0x00
#define	NM_STATUS_MSG	0x01
#define	NM_DEFER_SORT	0x02
#define	NM_FROM_COMPOSER 0x04


typedef struct titlebar_state {
    MAILSTREAM	*stream;
    MSGNO_S	*msgmap;
    char	*title,			/* constant, not allocated */
		*folder_name,		/* allocated memory	   */
		*context_name;		/*     "       "	   */
    long	 current_msg,
		 current_line,
		 current_thrd,
		 total_lines;
    int		 msg_state,
		 cur_mess_col,
		 del_column, 
		 percent_column,
		 page_column,
		 screen_cols;
    enum	 {Nominal, OnlyRead, ClosedUp} stream_status;
    TitleBarType style;
    TITLE_S      titlecontainer;
} TITLEBAR_STATE_S;


/*
 * Flags to help manage help display
 */
#define	HLPD_NONE	   0
#define	HLPD_NEWWIN	0x01
#define	HLPD_SECONDWIN	0x02
#define	HLPD_SIMPLE	0x04
#define	HLPD_FROMHELP	0x08


/*
 * Flags for write_pinerc
 */
#define	WRP_NONE	   0
#define	WRP_NOUSER	0x01



/*
 * Struct defining scrolltool operating parameters.
 * 
 */
typedef	struct scrolltool_s {
    struct {			/* Data and its attributes to scroll	 */
	void	   *text;	/* what to scroll			 */
	SourceType  src;	/* it's form (char **,char *,FILE *)	 */
	char	   *desc;	/* Description of "type" of data shown	 */
	HANDLE_S   *handles;	/* embedded data descriptions		 */
    } text;
    struct {			/* titlebar state			 */
	char	     *title;	/* screen's title			 */
	TitleBarType  style;	/* it's type				 */
	COLOR_PAIR   *color;
    } bar;
    struct {			/* screen's keymenu/command bindings	 */
	struct key_menu	 *menu;
	bitmap_t	  bitmap;
	OtherMenu	  what;
	void		(*each_cmd) PROTO((struct scrolltool_s *, int));
    } keys;
    struct {			/* help for this attachment		 */
	HelpType  text;		/* help text				 */
	char	 *title;	/* title for help screen		 */
    } help;
    struct {
	int (*click) PROTO((struct scrolltool_s *));
	int (*clickclick) PROTO((struct scrolltool_s *));
#ifdef	_WINDOWS
	/*
	 * For systems that support it, allow caller to do popup menu
	 */
	int (*popup) PROTO((struct scrolltool_s *, int));
#endif
    } mouse;
    struct {			/* where to start paging from		 */
	enum {FirstPage = 0, LastPage, Fragment, Offset, Handle} on;
	union {
	    char	*frag;	/* fragment in html text to start on	 */
	    long	 offset;
	} loc;
    } start;
    struct {			/* Non-default Command Processor	 */
	int (*tool) PROTO((int, MSGNO_S *, struct scrolltool_s *));
	/* The union below is opaque as far as scrolltool itself is
	 * concerned, but is provided as a container to pass data
	 * between the scrolltool caller and the given "handler"
	 * callback (or any other callback that takes a scrolltool_s *).
	 */
	union {
	    void *p;
	    int	  i;
	} data;
    } proc;
				/* End of page processing		 */
    int	       (*end_scroll) PROTO((struct scrolltool_s *));
				/* Handler for invalid command input	 */
    int	       (*bogus_input) PROTO((int));
    unsigned	 resize_exit:1;	/* Return from scrolltool if resized	 */
    unsigned	 body_valid:1;	/* Screen's body already displayed	 */
    unsigned	 no_stat_msg:1;	/* Don't display status messages	 */
    unsigned	 vert_handle:1;	/* hunt up and down on arrows/ctrl-[np]  */
    unsigned	 srch_handle:1;	/* search to next handle		 */
    unsigned	 quell_help:1;	/* Don't show handle nav help message    */
    unsigned	 quell_newmail:1;
				/* Don't check for new mail		 */
    unsigned	 jump_is_debug:1;
    unsigned	 use_indexline_color:1;
} SCROLL_S;



/*
 * typedefs of generalized filters used by gf_pipe
 */
typedef struct filter_s {	/* type to hold data for filter function */
    void (*f) PROTO((struct filter_s *, int));
    struct filter_s *next;	/* next filter to call                   */
    long     n;			/* number of chars seen                  */
    short    f1;		/* flags                                 */
    int	     f2;		/* second place for flags                */
    unsigned char t;		/* temporary char                        */
    char     *line;		/* place for temporary storage           */
    char     *linep;		/* pointer into storage space            */
    void     *opt;		/* optional per instance data		 */
    void     *data;		/* misc internal data pointer		 */
    unsigned char queue[1 + GF_MAXBUF];
    short	  queuein, queueout;
} FILTER_S;

typedef struct filter_insert_s {
    char *where;
    char *text;
    int   len;
    struct filter_insert_s *next;
} LT_INS_S;

typedef int  (*gf_io_t)();	/* type of get and put char function     */
typedef void (*filter_t) PROTO((FILTER_S *, int));
typedef	int  (*linetest_t) PROTO((long, char *, LT_INS_S **, void *));

typedef	struct filtlist_s {
    filter_t  filter;
    void     *data;
} FILTLIST_S;


typedef struct conversion_table {
    char          *from_charset;
    char          *to_charset;
    int            quality;
    void          *table;
    filter_t       convert;
} CONV_TABLE;


/* Conversion table quality of tranlation */
#define	CV_NO_TRANSLATE_POSSIBLE	1	/* We don't know how to      */
						/* translate this pair       */
#define	CV_NO_TRANSLATE_NEEDED		2	/* Not necessary, no-op      */
#define	CV_LOSES_SPECIAL_CHARS		3	/* Letters will translate    */
						/* ok but some special chars */
						/* may be lost               */
#define	CV_LOSES_SOME_LETTERS		4	/* Some special chars and    */
						/* some letters may be lost  */


/*
 * Attribute table where information on embedded formatting and such
 * is really stored.
 */

typedef	enum {Link, LinkTarget} EmbedActions;

typedef struct atable_s {	/* a stands for either "anchor" or "action" */
    short	 handle;	/* handle for this action */
    EmbedActions action;	/* type of action indicated */
    short	 len;		/* number of characters in label */
    unsigned     wasuline:1;	/* previously underlined (not standard bold) */
    char	*name;		/* pointer to name of action */
    PARAMETER	*parms;		/* pointer to  necessary data */
    struct atable_s *next;
} ATABLE_S;


#define TAG_EMBED	'\377'	/* Announces embedded data in text string */
#define	TAG_INVON	'\001'	/* Supported character attributes	  */
#define	TAG_INVOFF	'\002'
#define	TAG_BOLDON	'\003'
#define	TAG_BOLDOFF	'\004'
#define	TAG_ULINEON	'\005'
#define	TAG_ULINEOFF	'\006'
#define	TAG_FGCOLOR	'\010'	/* Change to this foreground color	  */
#define	TAG_BGCOLOR	'\011'	/* Change to this background color	  */
#define	TAG_HANDLE	'\020'	/* indicate's a handle to an action	  */
#define	TAG_HANDLEOFF	'\030'	/* indicate's end of handle text	  */


/*
 * This is just like a struct timeval. We need it for portability to systems
 * that don't have a struct timeval.
 */
typedef struct our_time_val {
    long sec;
    long usec;
} TIMEVAL_S;


/*
 * Structures to control flag maintenance screen
 */
struct flag_table {
    char     *name;		/* flag's name string */
    HelpType  help;		/* help text */
    long      flag;		/* flag tag (i.e., F_DEL above) */
    unsigned  set:2;		/* its state (set, unset, unknown) */
    unsigned  ukn:1;		/* allow unknown state */
    char     *keyword;		/* if keyword is different from nickname */
    char     *comment;		/* comment about the name */
};

struct flag_screen {
    char	      **explanation;
    struct flag_table **flag_table;
};

/*
 * Some defs to help keep flag setting straight...
 */
#define	CMD_FLAG_CLEAR	FALSE
#define	CMD_FLAG_SET	TRUE
#define	CMD_FLAG_UNKN	2


/*
 * Error handling argument for white pages lookups.
 */
typedef struct wp_err {
    char	*error;
    int		 wp_err_occurred;
    int		*mangled;
    int		 ldap_errno;
} WP_ERR_S;


/*
 * Structures to control the collection list screen
 */
typedef struct context_screen {
    unsigned	      edit:1;
    char	     *title, *print_string;
    CONTEXT_S	     *start,		/* for context_select_screen */
		     *selected,
		    **contexts;
    struct variable  *starting_var;	/* another type of start for config */
    int               starting_varmem;
    struct {
	HelpType  text;
	char	 *title;
    } help;
    struct key_menu  *keymenu;
} CONT_SCR_S;

 
/*
 * Structure and macros to help control format_header_text
 */
typedef struct header_s {
    unsigned type:4;
    unsigned except:1;
    union {
	char **l;		/* list of char *'s */
	long   b;		/* bit field of header fields (FE_* above) */
    } h;
} HEADER_S;

#define	HD_LIST		1
#define	HD_BFIELD	2
#define	HD_INIT(H, L, E, B)	if((L) && (L)[0]){			\
				    (H)->type = HD_LIST;		\
				    (H)->except = (E);			\
				    (H)->h.l = (L);			\
				}					\
				else{					\
				    (H)->type = HD_BFIELD;		\
				    (H)->h.b = (B);			\
				    (H)->except = 0;			\
				}


/*
 * struct to help peruse a, possibly fragmented ala RFC 2231, parm list
 */
typedef	struct parmlist {
    PARAMETER *list,
	      *seen;
    char       attrib[32],
	      *value;
} PARMLIST_S;


/*
 * Macro to help determine when we need to filter out chars
 * from message index or headers...
 */
#define	FILTER_THIS(c)	((((unsigned char) (c) < 0x20                  \
                           || (unsigned char) (c) == 0x7f)             \
                          && ((c) != 0x09)                             \
                          && !ps_global->pass_ctrl_chars)              \
                         || (((unsigned char) (c) >= 0x80              \
                              && (unsigned char) (c) < 0xA0)           \
                             && !ps_global->pass_ctrl_chars            \
			     && !ps_global->pass_c1_ctrl_chars))

/*
 * return values for IMAP URL parser
 */
#define	URL_IMAP_MASK		0x0007
#define	URL_IMAP_ERROR		0
#define	URL_IMAP_IMAILBOXLIST	0x0001
#define	URL_IMAP_IMESSAGELIST	0x0002
#define	URL_IMAP_IMESSAGEPART	0x0004
#define	URL_IMAP_IMBXLSTLSUB	0x0010
#define	URL_IMAP_ISERVERONLY	0x0020


#define pine_mail_append(stream,mailbox,message) \
   pine_mail_append_full(stream,mailbox,NULL,NULL,message)
#define pine_mail_copy(stream,sequence,mailbox) \
   pine_mail_copy_full(stream,sequence,mailbox,0)


/*
 * Constants and structs to aid RFC 2369 support
 */
#define	MLCMD_HELP	0
#define	MLCMD_UNSUB	1
#define	MLCMD_SUB	2
#define	MLCMD_POST	3
#define	MLCMD_OWNER	4
#define	MLCMD_ARCHIVE	5
#define	MLCMD_COUNT	6
#define	MLCMD_MAXDATA	3
#define	MLCMD_REASON	8192


typedef	struct	rfc2369_field_s {
    char  *name,
	  *description,
	  *action;
} RFC2369FIELD_S;

typedef	struct rfc2369_data_s {
    char *value,
	 *comment,
	 *error;
} RFC2369DATA_S;

typedef struct rfc2369_s {
    RFC2369FIELD_S  field;
    RFC2369DATA_S   data[MLCMD_MAXDATA];
} RFC2369_S;



/*----------------------------------------------------------------------
   This structure sort of takes the place of global variables or perhaps
is the global variable.  (It can be accessed globally as ps_global.  One
advantage to this is that as soon as you see a reference to the structure
you know it is a global variable. 
   In general it is treated as global by the lower level and utility
routines, but it is not treated so by the main screen driving routines.
Each of them receives it as an argument and then sets ps_global to the
argument they received.  This is sort of with the thought that things
might be coupled more loosely one day and that Pine might run where there
is more than one window and more than one instance.  But we haven't kept
up with this convention very well.
 ----*/
  
struct pine {
    void       (*next_screen)();	/* See loop at end of main() for how */
    void       (*prev_screen)();	/* these are used...		     */
    void       (*redrawer)();		/* NULL means stay in current screen */

    CONTEXT_S   *context_list;		/* list of user defined contexts */
    CONTEXT_S   *context_current;	/* default open context          */
    CONTEXT_S   *context_last;		/* most recently open context    */

    SP_S         s_pool;		/* stream pool */

    char         inbox_name[MAXFOLDER+1];
    char         pine_pre_vers[10];	/* highest version previously run */
    
    MAILSTREAM  *mail_stream;		/* ptr to current folder stream */
    MSGNO_S	*msgmap;		/* ptr to current message map   */

    unsigned     read_predicted:1;

    char         cur_folder[MAXPATH+1];
    char         last_unambig_folder[MAXPATH+1];
    ATTACH_S    *atmts;
    int          atmts_allocated;
    int	         remote_abook_validity;	/* minutes, -1=never, 0=only on opens */

    INDEX_COL_S *index_disp_format;

    char        *folders_dir;

    unsigned     mangled_footer:1; 	/* footer needs repainting */
    unsigned     mangled_header:1;	/* header needs repainting */
    unsigned     mangled_body:1;	/* body of screen needs repainting */
    unsigned     mangled_screen:1;	/* whole screen needs repainting */

    unsigned     in_init_seq:1;		/* executing initial cmd list */
    unsigned     save_in_init_seq:1;
    unsigned     dont_use_init_cmds:1;	/* use keyboard input when true */

    unsigned     give_fixed_warning:1;	/* warn user about "fixed" vars */
    unsigned     fix_fixed_warning:1;	/* offer to fix it              */

    unsigned     unseen_in_view:1;
    unsigned     start_in_context:1;	/* start fldr_scrn in current cntxt */
    unsigned     def_sort_rev:1;	/* true if reverse sort is default  */ 
    unsigned     restricted:1;

    unsigned	 save_msg_rule:5;
    unsigned	 fcc_rule:3;
    unsigned	 ab_sort_rule:3;
    unsigned     color_style:3;
    unsigned     index_color_style:3;
    unsigned     titlebar_color_style:3;
    unsigned	 fld_sort_rule:3;
    unsigned	 inc_startup_rule:3;
    unsigned	 pruning_rule:3;
    unsigned	 reopen_rule:4;
    unsigned	 goto_default_rule:3;
    unsigned	 thread_disp_style:3;
    unsigned	 thread_index_style:3;

    unsigned     full_header:2;         /* display full headers		   */
					/* 0 means normal		   */
					/* 1 means display all quoted text */
					/* 2 means full headers		   */
    unsigned     some_quoting_was_suppressed:1;
    unsigned     orig_use_fkeys:1;
    unsigned     try_to_create:1;	/* Save should try mail_create */
    unsigned     low_speed:1;	      /* various opt's 4 low connect speed */
    unsigned     postpone_no_flow:1;  /* don't set flowed when we postpone */
				      /* and don't reflow when we resume.  */
    unsigned     mm_log_error:1;
    unsigned     show_new_version:1;
    unsigned     pre390:1;
    unsigned     pre441:1;
    unsigned     first_time_user:1;
    unsigned	 intr_pending:1;	/* received SIGINT and haven't acted */
    unsigned	 expunge_in_progress:1;	/* don't want to re-enter c-client   */
    unsigned	 never_allow_changing_from:1;	/* not even for roles */

    unsigned	 readonly_pinerc:1;
    unsigned	 cache_remote_pinerc:1;
    unsigned	 view_all_except:1;
    unsigned     start_in_index:1;	/* cmd line flag modified on startup */
    unsigned     noshow_error:1;	/* c-client error callback controls */
    unsigned     noshow_warn:1;
    unsigned	 noshow_timeout:1;
    unsigned	 conceal_sensitive_debugging:1;
    unsigned	 turn_off_threading_temporarily:1;
    unsigned	 view_skipped_index:1;
    unsigned	 a_format_contains_score:1;
    unsigned	 ugly_consider_advancing_bit:1;
    unsigned	 dont_count_flagchanges:1;

    unsigned	 phone_home:1;
    unsigned     painted_body_on_startup:1;
    unsigned     painted_footer_on_startup:1;
    unsigned     open_readonly_on_startup:1;
    unsigned     exit_if_no_pinerc:1;
    unsigned     pass_ctrl_chars:1;
    unsigned     pass_c1_ctrl_chars:1;
    unsigned     display_keywords_in_subject:1;
    unsigned     display_keywordinits_in_subject:1;
    unsigned     beginning_of_month:1;
    unsigned     beginning_of_year:1;

    unsigned 	 viewer_overlap:8;
    unsigned	 scroll_margin:8;
    unsigned 	 remote_abook_history:8;

#if defined(DOS) || defined(OS2)
    unsigned     blank_user_id:1;
    unsigned     blank_personal_name:1;
    unsigned     blank_user_domain:1;
#ifdef	_WINDOWS
    unsigned	 update_registry:2;
    unsigned     install_flag:1;
#endif
#endif

    unsigned 	 debug_malloc:6;
    unsigned 	 debug_timestamp:1;
    unsigned 	 debug_flush:1;
    unsigned 	 debug_tcp:1;
    unsigned 	 debug_imap:3;
    unsigned 	 debug_nfiles:5;
    unsigned     debugmem:1;
    int          dlevel;		/* hack to pass arg to debugjournal */
#ifdef PASSFILE
    unsigned     nowrite_passfile:1;
#endif

    unsigned     convert_sigs:1;
    unsigned     dump_supported_options:1;

    unsigned     start_entry;		/* cmd line arg: msg # to start on */

    bitmap_t     feature_list;		/* a bitmap of all the features */
    char       **feat_list_back_compat;

    SPEC_COLOR_S *hdr_colors;		/* list of configed colors for view */

    short	 init_context;

    int         *initial_cmds;         /* cmds to execute on startup */
    int         *free_initial_cmds;    /* used to free when done */

    char         c_client_error[300];  /* when nowhow_error is set and PARSE */

    struct ttyo *ttyo;

    USER_S	 ui;		/* system derived user info */

    POST_S      *post;

    char	*home_dir,
                *hostname,	/* Fully qualified hostname */
                *localdomain,	/* The (DNS) domain this host resides in */
                *userdomain,	/* The per user domain from .pinerc or */
                *maildomain,	/* Domain name for most uses */
#if defined(DOS) || defined(OS2)
                *pine_dir,	/* argv[0] as provided by DOS */
                *aux_files_dir,	/* User's auxiliary files directory */
#endif
#ifdef PASSFILE
                *passfile,
#endif /* PASSFILE */
                *pinerc,	/* Location of user's pinerc */
                *exceptions,	/* Location of user's exceptions */
		*pine_name;	/* name we were invoked under */
    PINERC_S    *prc,		/* structure for personal pinerc */
		*post_prc,	/* structure for post-loaded pinerc */
		*pconf;		/* structure for global pinerc */
    
    EditWhich	 ew_for_except_vars;
    EditWhich	 ew_for_role_take;
    EditWhich	 ew_for_score_take;
    EditWhich	 ew_for_filter_take;
    EditWhich	 ew_for_incol_take;
    EditWhich	 ew_for_other_take;

    SortOrder    def_sort,	/* Default sort type */
		 sort_types[22];

    int          last_expire_year, last_expire_month;

    int		 printer_category;

    int		 status_msg_delay;

    int		 composer_fillcol;

    int		 nmw_width;

    int          hours_to_timeout;

    int          tcp_query_timeout;

    time_t       check_interval_for_noncurr;

    time_t       last_nextitem_forcechk;

    int		 deadlets;

    int		 quote_suppression_threshold;

    CONV_TABLE  *conv_table;

    KEYWORD_S   *keywords;
    SPEC_COLOR_S *kw_colors;

    char	 last_error[500];
    INIT_ERR_S  *init_errs;

    PRINT_S	*print;

    struct variable *vars;
};




/*======================================================================
    Declarations of all the Pine functions.
 ====*/
  
/*-- addrbook.c --*/
char	   *addr_book_bounce PROTO((void));
char	   *addr_book_compose PROTO((char **));
char	   *addr_book_compose_lcc PROTO((char **));
char	   *addr_book_oneaddr PROTO((void));
char	   *addr_book_oneaddr_nf PROTO((void));
char	   *addr_book_multaddr_nf PROTO((void));
void	    addr_book_screen PROTO((struct pine *));
void	    addr_book_config PROTO((struct pine *, int));

/*-- adrbkcmd.c --*/
int         any_addrbooks_to_convert PROTO((struct pine *));
int         any_sigs_to_convert PROTO((struct pine *));
int         convert_addrbooks_to_remote PROTO((struct pine *, char *, size_t));
int         convert_sigs_to_literal PROTO((struct pine *, int));
#ifdef	ENABLE_LDAP
void        view_ldap_entry PROTO((struct pine *, LDAP_SERV_RES_S *));
void        free_saved_query_parameters PROTO((void));
int         url_local_ldap PROTO((char *));
void        compose_to_ldap_entry PROTO((struct pine *, LDAP_SERV_RES_S *,int));
#endif

/*-- adrbklib.c --*/
char	   *tempfile_in_same_dir PROTO((char *, char *, char **));
MAILSTREAM *adrbk_handy_stream PROTO((char *));
void        note_closed_adrbk_stream PROTO((MAILSTREAM *));

/*-- args.c --*/
void	    pine_args PROTO((struct pine *, int, char **, ARGDATA_S *));
void        display_args_err PROTO((char *, char **, int));
void        args_help PROTO((void));

/*-- bldaddr.c --*/
void	    addrbook_reset PROTO((void));
int	    address_is_us PROTO((ADDRESS *, struct pine *));
int	    address_is_same PROTO((ADDRESS *, ADDRESS *));
char	   *addr_list_string PROTO((ADDRESS *,
				  char *(*f) PROTO((ADDRESS *, char *, size_t)),
				  int, int));
char	   *addr_string PROTO((ADDRESS *, char *));
void	    from_or_replyto_in_abook PROTO((MAILSTREAM *, SEARCHSET *, int,
					    PATTERN_S *));
void	    adrbk_maintenance PROTO((void));
int	    build_address PROTO((char *, char **,char **,BUILDER_ARG *,int *));
int	    build_addr_lcc PROTO((char *, char **,char **,BUILDER_ARG *,int *));
void	    trim_remote_adrbks PROTO((void));
void	    completely_done_with_adrbks PROTO((void));
void        free_privatetop PROTO((PrivateTop **));
void        free_privateencoded PROTO((PrivateEncoded **));
char	   *get_fcc PROTO((char *));
char	   *get_fcc_based_on_to PROTO((ADDRESS *));
char	   *get_fcc_from_addr PROTO((ADDRESS *, char *, size_t));
char	   *get_nickname_from_addr PROTO((ADDRESS *, char *, size_t));
void	    just_update_lookup_file PROTO((char *, char *));
void	    set_last_fcc PROTO((char *));
char	   *simple_addr_string PROTO((ADDRESS *, char *, size_t));
#ifdef	ENABLE_LDAP
LDAP_SERV_S *break_up_ldap_server PROTO((char *));
void	     free_ldap_server_info PROTO((LDAP_SERV_S **));
int          ldap_v3_is_supported PROTO((LDAP *, LDAP_SERV_S *));
void	     our_ldap_memfree PROTO((void *));
void	     our_ldap_dn_memfree PROTO((void *));
int	     our_ldap_set_option PROTO((LDAP *, int, void *));
#endif

/*--- filter.c ---*/
int	    pc_is_picotext PROTO((gf_io_t));
STORE_S	   *so_get PROTO((SourceType, char *, int));
int	    so_give PROTO((STORE_S **));
int	    so_seek PROTO((STORE_S *, long, int));
int	    so_truncate PROTO((STORE_S *, long));
long	    so_tell PROTO((STORE_S *));
int	    so_release PROTO((STORE_S *));
int	    so_nputs PROTO((STORE_S *, char *, long));
void	   *so_text PROTO((STORE_S *));
void	    gf_filter_init PROTO((void));
void	    gf_link_filter PROTO((filter_t, void *));
char	   *gf_pipe PROTO((gf_io_t, gf_io_t));
long	    gf_bytes_piped PROTO(());
char	   *gf_filter PROTO((char *, char *, STORE_S *,
			     gf_io_t, FILTLIST_S *, int));
void	    gf_set_so_readc PROTO((gf_io_t *, STORE_S *));
void	    gf_clear_so_readc PROTO((STORE_S *));
void	    gf_set_so_writec PROTO((gf_io_t *, STORE_S *));
void	    gf_clear_so_writec PROTO((STORE_S *));
void	    gf_set_readc PROTO((gf_io_t *, void *, unsigned long, SourceType));
void	    gf_set_writec PROTO((gf_io_t *, void *, unsigned long,
				 SourceType));
int	    gf_puts PROTO((char *, gf_io_t));
int	    gf_nputs PROTO((char *, long, gf_io_t));
void	    gf_set_terminal PROTO((gf_io_t));
void	    gf_binary_b64 PROTO((FILTER_S *, int));
void	    gf_b64_binary PROTO((FILTER_S *, int));
void	    gf_qp_8bit PROTO((FILTER_S *, int));
void	    gf_8bit_qp PROTO((FILTER_S *, int));
void	    gf_rich2plain PROTO((FILTER_S *, int));
void	   *gf_rich2plain_opt PROTO((int));
void	    gf_enriched2plain PROTO((FILTER_S *, int));
void	   *gf_enriched2plain_opt PROTO((int));
void	    gf_html2plain PROTO((FILTER_S *, int));
void	   *gf_html2plain_opt PROTO((char *, int, int *, HANDLE_S **, int));
void	    gf_2022_jp_to_euc PROTO((FILTER_S *, int));
void	    gf_euc_to_2022_jp PROTO((FILTER_S *, int));
void	    gf_flow_text_post_compose PROTO((FILTER_S *, int));
void	    gf_convert_8bit_charset PROTO((FILTER_S *, int));
void	    gf_convert_utf8_charset PROTO((FILTER_S *, int));
void	    gf_escape_filter PROTO((FILTER_S *, int));
void	    gf_control_filter PROTO((FILTER_S *, int));
void	   *gf_control_filter_opt PROTO((int *));
void	    gf_tag_filter PROTO((FILTER_S *, int));
void	    gf_wrap PROTO((FILTER_S *, int));
void	   *gf_wrap_filter_opt PROTO((int, int, int *, int, int));
void	    gf_preflow PROTO((FILTER_S *, int));
void	    gf_busy PROTO((FILTER_S *, int));
void	    gf_nvtnl_local PROTO((FILTER_S *, int));
void	    gf_local_nvtnl PROTO((FILTER_S *, int));
void	    gf_line_test PROTO((FILTER_S *, int));
void	   *gf_line_test_opt PROTO((linetest_t, void *));
LT_INS_S  **gf_line_test_new_ins PROTO((LT_INS_S **, char *, char *, int));
void	    gf_line_test_free_ins PROTO((LT_INS_S **));
void	    gf_prefix PROTO((FILTER_S *, int));
void	   *gf_prefix_opt PROTO((char *));

/*--- folder.c ---*/
void	    folder_screen PROTO((struct pine *));
void	    folder_config_screen PROTO((struct pine *, int));
char	   *folders_for_fcc PROTO((char **));
int	    folders_for_goto PROTO((struct pine *, CONTEXT_S **, char *, int));
int	    folders_for_save PROTO((struct pine *, CONTEXT_S **, char *, int));
char	   *folders_for_roles PROTO((int));
char	   *pretty_fn PROTO((char *));
int	    folder_exists PROTO((CONTEXT_S *, char *));
int	    folder_name_exists PROTO((CONTEXT_S *, char *, char **));
int	    folder_create PROTO((char *, CONTEXT_S *));
int	    folder_complete PROTO((CONTEXT_S *, char *, int *));
char	   *folder_as_breakout PROTO((CONTEXT_S *, char *));
void	    init_folders PROTO((struct pine *));
int         news_in_folders PROTO((struct variable *));
CONTEXT_S  *new_context PROTO((char *, int *));
void	    free_contexts PROTO((CONTEXT_S **));
void	    free_context PROTO((CONTEXT_S **));
void	    build_folder_list PROTO((MAILSTREAM **, CONTEXT_S *,
				     char *, char *, int));
void	    free_folder_list PROTO((CONTEXT_S *));
CONTEXT_S  *default_save_context PROTO((CONTEXT_S *));
FOLDER_S   *folder_entry PROTO((int, void *));
FOLDER_S   *new_folder PROTO((char *, unsigned long));
int	    folder_insert PROTO((int, FOLDER_S *, void *));
int	    folder_index PROTO((char *, CONTEXT_S *, int));
char	   *folder_is_nick PROTO((char *, void *, int));
char	   *next_folder PROTO((MAILSTREAM **, char *, char *,CONTEXT_S *,
			       long *, int *));
void	    init_inbox_mapping PROTO((char *, CONTEXT_S *));
int	    news_build PROTO((char *, char **, char **, BUILDER_ARG *, int *));
char	   *news_group_selector PROTO((char **));
void	    free_newsgrp_cache PROTO(());
char	   *context_edit_screen PROTO((struct pine *, char *, char *,
				       char *, char *, char *));
SELECTED_S *new_selected PROTO((void));
void	    free_selected PROTO((SELECTED_S **));
int	    add_new_folder PROTO((CONTEXT_S *, EditWhich, int, char *, size_t,
				  MAILSTREAM *, char *));

/*-- help.c --*/
int	    helper PROTO((HelpType, char *, int));
int	    url_local_helper PROTO((char *));
int	    url_local_config PROTO((char *));
void	    review_messages PROTO((void));
void	    add_review_message PROTO((char *, int));
int	    gripe_gripe_to PROTO((char *));
void	    init_helper_getc PROTO((char **));
int	    helper_getc PROTO((char *));
void	    print_help PROTO((char **));
void	    dump_supported_options PROTO((void));
#if defined(DOS) || defined(HELPFILE)
char	  **get_help_text PROTO((HelpType)); 
#endif

/*-- imap.c --*/
char	   *cached_user_name PROTO((char *));
void	    imap_flush_passwd_cache PROTO(());
long	    pine_tcptimeout PROTO((long, long));
long	    pine_sslcertquery PROTO((char *, char *, char *));
void	    pine_sslfailure PROTO((char *, char *, unsigned long));
char       *pine_newsrcquery PROTO((MAILSTREAM *, char *, char *));
char	   *imap_referral PROTO((MAILSTREAM *, char *, long));
long	    imap_proxycopy PROTO((MAILSTREAM *, char *, char *, long));
void	   *pine_block_notify PROTO((int, void *));
void        set_read_predicted PROTO((int));
int	    url_local_certdetails PROTO((char *));
int         check_for_move_mbox PROTO((char *, char *, size_t, char **));
long        pine_mail_status PROTO((MAILSTREAM *, char *, long));
long        pine_mail_status_full PROTO((MAILSTREAM *, char *, long,
					 unsigned long *, unsigned long *));

/*-- init.c --*/
void	    init_error PROTO((struct pine *, int, int, int, char *));
void	    init_pinerc PROTO((struct pine *, char **));
void	    init_vars PROTO((struct pine *));
void	    free_vars PROTO((struct pine *));
void	    free_variable_values PROTO((struct variable *));
void	    set_current_val PROTO((struct variable *, int, int));
char	   *expand_variables PROTO((char *, size_t, char *, int));
void	    set_news_spec_current_val PROTO((int, int));
void        cur_rule_value PROTO((struct variable *, int, int));
void	    quit_to_edit_msg PROTO((PINERC_S *));
int	    init_username PROTO((struct pine *));
int	    init_hostname PROTO((struct pine *));  
int	    write_pinerc PROTO((struct pine *, EditWhich, int));
int	    var_in_pinerc PROTO((char *));
void	    free_pinerc_lines PROTO((PINERC_LINE **));
int	    decode_sort PROTO((char *, SortOrder *, int *));
void	    dump_global_conf PROTO((void));
void	    dump_new_pinerc PROTO((char *));
int	    set_variable PROTO((int, char *, int, int, EditWhich));
int	    set_variable_list PROTO((int, char **, int, EditWhich));
void        set_current_color_vals PROTO((struct pine *));
void        set_custom_spec_colors PROTO((struct pine *));
int	    init_mail_dir PROTO((struct pine *));
void	    init_init_vars PROTO((struct pine *));
void	    init_save_defaults PROTO(());
int	    expire_sent_mail PROTO((void));
int	    first_run_of_month PROTO((void));
int	    first_run_of_year PROTO((void));
char	  **parse_list PROTO((char *, int, int, char **));
char      **copy_list_array PROTO((char **));
void        free_list_array PROTO((char ***));
int         equal_list_arrays PROTO((char **, char **));
FEATURE_S  *feature_list PROTO((int));
int	    feature_list_index PROTO((int));
char	   *feature_list_name PROTO((int));
char	   *feature_list_section PROTO((FEATURE_S *));
HelpType    feature_list_help PROTO((int));
void        set_feature_list_current_val PROTO((struct variable *));
NAMEVAL_S  *save_msg_rules PROTO((int));
NAMEVAL_S  *fcc_rules PROTO((int));
NAMEVAL_S  *ab_sort_rules PROTO((int));
NAMEVAL_S  *col_style PROTO((int));
NAMEVAL_S  *index_col_style PROTO((int));
NAMEVAL_S  *titlebar_col_style PROTO((int));
NAMEVAL_S  *fld_sort_rules PROTO((int));
NAMEVAL_S  *incoming_startup_rules PROTO((int));
NAMEVAL_S  *startup_rules PROTO((int));
NAMEVAL_S  *pruning_rules PROTO((int));
NAMEVAL_S  *reopen_rules PROTO((int));
NAMEVAL_S  *goto_rules PROTO((int));
NAMEVAL_S  *thread_disp_styles PROTO((int));
NAMEVAL_S  *thread_index_styles PROTO((int));
void	    set_old_growth_bits PROTO((struct pine *, int));
int	    test_old_growth_bits PROTO((struct pine *, int));
void	    dump_config PROTO((struct pine *, gf_io_t, int));
void	    dump_pine_struct PROTO((struct pine *, gf_io_t));
REMDATA_S  *rd_create_remote PROTO((RemType, char *, void *, unsigned *,
				    char *,char *));
REMDATA_S  *rd_new_remdata PROTO((RemType, char *, void *));
void	    rd_free_remdata PROTO((REMDATA_S **));
int	    rd_read_metadata PROTO((REMDATA_S *));
void	    rd_update_metadata PROTO((REMDATA_S *, char *));
void	    rd_write_metadata PROTO((REMDATA_S *, int));
void	    rd_check_remvalid PROTO((REMDATA_S *, long));
void	    rd_open_remote PROTO((REMDATA_S *));
void	    rd_close_remote PROTO((REMDATA_S *));
void	    rd_check_readonly_access PROTO((REMDATA_S *));
int	    rd_update_local PROTO((REMDATA_S *));
int	    rd_update_remote PROTO((REMDATA_S *, char *));
void	    rd_trim_remdata PROTO((REMDATA_S **));
void	    rd_close_remdata PROTO((REMDATA_S **));
int	    rd_remote_is_readonly PROTO((REMDATA_S *));
int	    rd_stream_exists PROTO((REMDATA_S *));
int	    rd_ping_stream PROTO((REMDATA_S *));
int	    copy_pinerc PROTO((char *, char *, char **));
int	    copy_abook PROTO((char *, char *, char **));
PINERC_S   *new_pinerc_s PROTO((char *));
void        free_pinerc_s PROTO((PINERC_S **));

/*---- mailcap.c ----*/
MCAP_CMD_S *mailcap_build_command PROTO((int, char *, PARAMETER *,
					 char *, int *, int));
int	    mailcap_can_display PROTO((int, char *, PARAMETER *, int));
void	    mailcap_free PROTO((void));
int	    set_mime_type_by_extension PROTO((BODY *, char *));
int	    set_mime_extension_by_type PROTO((char *, char *));

/*---- mailcmd.c ----*/
int	    process_cmd PROTO((struct pine *, MAILSTREAM *, MSGNO_S *,
			       int, CmdWhere, int *));
int	    apply_command PROTO((struct pine *, MAILSTREAM *, MSGNO_S *, int,
				 int, int));
int	    menu_command PROTO((int, struct key_menu *));
void	    menu_init_binding PROTO((struct key_menu *, int, int,
				     char *, char *, int));
void	    menu_add_binding PROTO((struct key_menu *, int, int));
int	    menu_clear_binding PROTO((struct key_menu *, int));
int	    menu_binding_index PROTO((struct key_menu *, int));
int	    individual_select PROTO((struct pine *, MSGNO_S *, CmdWhere));
void	    reset_index_format PROTO((void));
void	    reset_sort_order PROTO((unsigned));
void	    bogus_command PROTO((int, char *));
void	    cmd_cancelled PROTO((char *));
char	   *broach_folder PROTO((int, int, CONTEXT_S **));
int	    do_broach_folder PROTO((char *, CONTEXT_S *, MAILSTREAM **,
				    unsigned long));
void	    visit_folder PROTO((struct pine *, char *, CONTEXT_S *,
				MAILSTREAM *, unsigned long));
long	    jump_to PROTO((MSGNO_S *, int, int, SCROLL_S *, CmdWhere));
long	    zoom_index PROTO((struct pine *, MAILSTREAM *, MSGNO_S *));
int	    unzoom_index PROTO((struct pine *, MAILSTREAM *, MSGNO_S *));
int	    save_prompt PROTO((struct pine *, CONTEXT_S **, char *, size_t,
			       char *, ENVELOPE *, long, char *, SaveDel *));
int	    save_fetch_append PROTO((MAILSTREAM *, long, char *, MAILSTREAM *,
				     char *, CONTEXT_S *, unsigned long,
				     char *, char *, STORE_S *));
int         raw_pipe_getc PROTO((unsigned char *));
void	    prime_raw_pipe_getc PROTO((MAILSTREAM *, long, long, long));
PIPE_S	   *cmd_pipe_open PROTO((char *, char **, int, gf_io_t *));
void	    flag_string PROTO((MESSAGECACHE *, long, char *));
int         user_flag_is_set PROTO((MAILSTREAM *, unsigned long, char *));
int         user_flag_index PROTO((MAILSTREAM *, char *));
int         keyword_check PROTO((char *, char **));
char       *choose_a_keyword PROTO((void));
char      **choose_list_of_keywords PROTO((void));
char      **choose_list_of_charsets PROTO((void));
void        advance_cur_after_delete PROTO((struct pine *, MAILSTREAM *,
					    MSGNO_S *, CmdWhere));
void	    expunge_and_close PROTO((MAILSTREAM *, char **, unsigned long));
void	    process_filter_patterns PROTO((MAILSTREAM *, MSGNO_S *, long));
void	    reprocess_filter_patterns PROTO((MAILSTREAM *, MSGNO_S *, int));
char	   *get_uname PROTO((char *, char *, int));
char	   *build_updown_cmd PROTO((char *, char *, char *, char*));
int	    file_lister PROTO((char *, char *, int, char *, int, int, int));
int	    display_folder_list PROTO((CONTEXT_S **, char *, int,
				       int (*) PROTO((struct pine *,
						      CONTEXT_S **,
						      char *, int))));
void	    rfc2369_display PROTO((MAILSTREAM *, MSGNO_S *, long));
int	    pseudo_selected PROTO((MSGNO_S *));
void	    restore_selected PROTO((MSGNO_S *));
int         simple_export PROTO((struct pine *, void *, SourceType, char *,
				       char *));
int         get_export_filename PROTO((struct pine *, char *, char *, char *,
				       size_t, char *, char *, ESCKEY_S *,
				       int *, int, int));
char	   *build_sequence PROTO((MAILSTREAM *, MSGNO_S *, long *));
int         trivial_patgrp PROTO((PATGRP_S *));
int	    bezerk_delimiter PROTO((ENVELOPE *, MESSAGECACHE *, gf_io_t, int));
#ifdef	_WINDOWS
int	    header_mode_callback PROTO((int, long));
int	    any_selected_callback PROTO((int, long));
int	    zoom_mode_callback PROTO((int, long));
int	    flag_callback PROTO((int, long));
MPopup	   *flag_submenu PROTO((MESSAGECACHE *));
#endif

/*--- mailindx.c ---*/
void	    mail_index_screen PROTO((struct pine *));
int	    index_lister PROTO((struct pine *, CONTEXT_S *, char *,
				MAILSTREAM *, MSGNO_S *));
int	    print_index PROTO((struct pine *, MSGNO_S *, int));
void	    clear_index_cache PROTO((void));
void	    clear_iindex_cache PROTO((void));
void	    clear_tindex_cache PROTO((void));
void	    clear_index_cache_ent PROTO((long));
int	    build_index_cache PROTO((long));
int         calculate_some_scores PROTO((MAILSTREAM *, SEARCHSET *, int));
long        get_msg_score PROTO((MAILSTREAM *, long));
void        adjust_cur_to_visible PROTO((MAILSTREAM *, MSGNO_S *));
void        clear_folder_scores PROTO((MAILSTREAM *));
void        clear_msg_score PROTO((MAILSTREAM *, long));
#ifdef	DOS
void	    flush_index_cache PROTO((void));
#endif
unsigned long line_hash PROTO((char *));
void	    init_index_format PROTO((char *, INDEX_COL_S **));
void        set_need_format_setup();
void	    pine_imap_envelope PROTO((MAILSTREAM *, unsigned long, ENVELOPE *));
void	    build_header_cache PROTO((void));
void	    redraw_index_body PROTO((void));
INDEX_PARSE_T *
	    itoktype PROTO((char *, int));
struct key_menu *
	    do_index_border PROTO((CONTEXT_S *, char *, MAILSTREAM *,
				   MSGNO_S *, IndexType, int *, int));
char	   *sort_name PROTO((SortOrder));
void	    sort_folder PROTO((MAILSTREAM *, MSGNO_S *,
			       SortOrder, int, unsigned));
int	    percent_sorted PROTO((void));
void	    msgno_init PROTO((MSGNO_S **, long));
void	    msgno_give PROTO((MSGNO_S **));
void	    msgno_add_raw PROTO((MSGNO_S *, long));
void	    msgno_flush_raw PROTO((MSGNO_S *, long));
int	    msgno_in_select PROTO((MSGNO_S *, long));
void	    msgno_set_sort PROTO((MSGNO_S *, SortOrder));
void	    msgno_inc PROTO((MAILSTREAM *, MSGNO_S *, int));
void	    msgno_dec PROTO((MAILSTREAM *, MSGNO_S *, int));
int	    msgno_include PROTO((MAILSTREAM *, MSGNO_S *, int));
void	    msgno_exclude PROTO((MAILSTREAM *, MSGNO_S *, long, int));
void	    msgno_exclude_deleted PROTO((MAILSTREAM *, MSGNO_S *));
int	    msgno_exceptions PROTO((MAILSTREAM *, long, char *, int *, int));
int	    msgno_any_deletedparts PROTO((MAILSTREAM *, MSGNO_S *));
int	    msgno_part_deleted PROTO((MAILSTREAM *, long, char *));
void	    msgno_free_exceptions PROTO((PARTEX_S **));
char       *prepend_keyword_subject PROTO((MAILSTREAM *, long, char *, SubjKW,
					   char *, OFFCOLOR_S *, int *));
int         get_index_line_color PROTO((MAILSTREAM *, SEARCHSET *,
					PAT_STATE **, COLOR_PAIR **));
void	    free_pine_elt PROTO((void **));
SEARCHSET  *build_searchset PROTO((MAILSTREAM *));
void	    collapse_or_expand PROTO((struct pine *, MAILSTREAM *, MSGNO_S *,
				      unsigned long));
PINETHRD_S *fetch_thread PROTO((MAILSTREAM *, unsigned long));
PINETHRD_S *fetch_head_thread PROTO((MAILSTREAM *));
int         view_thread PROTO((struct pine *, MAILSTREAM *, MSGNO_S *, int));
int         unview_thread PROTO((struct pine *, MAILSTREAM *, MSGNO_S *));
void       *stop_threading_temporarily PROTO((void));
void        restore_threading PROTO((void **));
unsigned long count_lflags_in_thread PROTO((MAILSTREAM *, PINETHRD_S *,
					    MSGNO_S *, int));
void        set_thread_lflags PROTO((MAILSTREAM *, PINETHRD_S *,
				     MSGNO_S *, int, int));
PINETHRD_S *find_thread_by_number PROTO((MAILSTREAM *, MSGNO_S *, long,
					 PINETHRD_S *));
void        collapse_threads PROTO((MAILSTREAM *, MSGNO_S *, PINETHRD_S *));
int         thread_has_some_visible PROTO((MAILSTREAM *, PINETHRD_S *));
void        erase_threading_info PROTO((MAILSTREAM *, MSGNO_S *));
void        select_thread_stmp PROTO((struct pine *, MAILSTREAM *, MSGNO_S *));
#ifdef	_WINDOWS
int	    index_sort_callback PROTO((int, long));
#endif

/*---- mailpart.c ----*/
void	    attachment_screen PROTO((struct pine *));
char	   *detach PROTO((MAILSTREAM *, long, char *,
			  long *, gf_io_t, FILTLIST_S *, int));
int	    display_attachment PROTO((long, ATTACH_S *, int));
MAILSTREAM *save_msg_stream PROTO((CONTEXT_S *, char *, int *));
int	    valid_filter_command PROTO((char **));
char	   *expand_filter_tokens PROTO((char *, ENVELOPE *, char **,
					char **, char **, int *, int *));
char	   *filter_session_key PROTO(());
char	   *filter_data_file PROTO((int));
char	   *dfilter PROTO((char *, STORE_S *, gf_io_t, FILTLIST_S *));
char	   *df_static_trigger PROTO((BODY *, char *));
char	   *detach_raw PROTO((MAILSTREAM *, long, char *, gf_io_t, int));
void        date_str PROTO((char *, IndexColType, int, char *));
char       *pine_mail_fetch_text PROTO((MAILSTREAM *, unsigned long,
					char *, unsigned long *, long));
char       *pine_mail_fetch_body PROTO((MAILSTREAM *, unsigned long,
					char *, unsigned long *, long));
char       *pine_mail_partial_fetch_wrapper PROTO((MAILSTREAM *, unsigned long,
						   char *, unsigned long *,
						   long, unsigned long, char **,
						   int));
int         write_attachment_to_file PROTO((MAILSTREAM *, long, ATTACH_S *,
					    int, char *));

/*--- mailview.c ---*/
void	    mail_view_screen PROTO((struct pine *));
int	    scrolltool PROTO((SCROLL_S *));
char	   *body_type_names PROTO((int));
char	   *type_desc PROTO((int, char *, PARAMETER *, int));
char	   *part_desc PROTO((char *, BODY *, int, int, gf_io_t));
int	    format_message PROTO((long, ENVELOPE *, BODY *, HANDLE_S **,
				  int, gf_io_t));
char	   *format_editorial PROTO((char *, int, gf_io_t));
COLOR_PAIR *hdr_color PROTO((char *, char *, SPEC_COLOR_S *));
void	    free_spec_colors PROTO((SPEC_COLOR_S **));
int	    match_escapes PROTO((char *));
long        fcc_size_guess PROTO((BODY *));
int	    decode_text PROTO((ATTACH_S *, long, gf_io_t, HANDLE_S **,
			      DetachErrStyle,int));
char	   *display_parameters PROTO((PARAMETER *));
void	    display_output_file PROTO((char *, char *, char *, int));
char	   *pine_fetch_header PROTO((MAILSTREAM *, long, char *,
				     char **, long));
int	    format_header PROTO((MAILSTREAM *, long, char *, ENVELOPE *,
				HEADER_S *, char *, HANDLE_S **, int, gf_io_t));
void	    init_handles PROTO((HANDLE_S **));
void	    free_handles PROTO((HANDLE_S **));
HANDLE_S   *new_handle PROTO((HANDLE_S **));
HANDLE_S   *get_handle PROTO((HANDLE_S *, int));
url_tool_t  url_local_handler PROTO((char *));
int	    url_local_fragment PROTO((char *));
int	    url_external_specific_handler PROTO((char *, int));
int	    url_local_mailto PROTO((char *));
int	    url_local_mailto_and_atts PROTO((char *, PATMT *));
int	    url_imap_folder PROTO((char *, char **, long *,
				   long *, char **, int));
int	    url_hilite PROTO((long, char *, LT_INS_S **, void *));
int         url_hilite_abook PROTO((long, char *, LT_INS_S **, void *));
char       *color_embed PROTO((char *, char *));
int	    colorcmp PROTO((char *, char *));
int         scroll_add_listmode PROTO((CONTEXT_S *, int));
int         ng_scroll_edit PROTO((CONTEXT_S *, int));
int         folder_select_update PROTO((CONTEXT_S *, int));
STRINGLIST *new_strlst PROTO((char **));
void	    free_strlst PROTO((STRINGLIST **));

/*--newmail.c --*/
long	    new_mail PROTO((int, CheckPointTime, int));
void	    check_point_change PROTO((MAILSTREAM *));
void	    reset_check_point PROTO((MAILSTREAM *));
void	    zero_new_mail_count PROTO((void));
int	    changes_to_checkpoint PROTO((MAILSTREAM *));
void	    close_newmailfifo PROTO((void));
void	    init_newmailfifo PROTO((char *));

/*-- os.c --*/
int	    can_access_in_path PROTO((char *, char *, int));
long	    name_file_size PROTO((char *));
long	    fp_file_size PROTO((FILE *));
time_t	    name_file_mtime PROTO((char *));
time_t	    fp_file_mtime PROTO((FILE *));
void	    file_attrib_copy PROTO((char *, char *));
int	    is_writable_dir PROTO((char *));
int	    create_mail_dir PROTO((char *));
int	    rename_file PROTO((char *, char *));
void	    build_path PROTO((char *, char *, char *, size_t));
int	    is_absolute_path PROTO((char *));
char	   *last_cmpnt PROTO((char *));
int	    expand_foldername PROTO((char *, size_t));
char	   *fnexpand PROTO((char *, int));
char	   *filter_filename PROTO((char *, int *));
int	    cntxt_allowed PROTO((char *));
long	    disk_quota PROTO((char *, int *));
char	   *read_file PROTO((char *));
FILE	   *create_tmpfile PROTO((void));
void	    coredump PROTO((void));
void	    getdomainnames PROTO((char *, int, char *, int));
int	    have_job_control PROTO((void));
void 	    stop_process PROTO((void));
char	   *error_description PROTO((int));
void	    get_user_info PROTO((struct user_info *));
char	   *local_name_lookup PROTO((char *));
void	    change_passwd PROTO((void));
int	    mime_can_display PROTO((int, char *, PARAMETER *));
int	    fget_pos PROTO((FILE *, fpos_t *));
char	   *canonical_name PROTO((char *));
PIPE_S	   *open_system_pipe PROTO((char *, char **, char **, int, int));
int	    close_system_pipe PROTO((PIPE_S **, int *, int));
int         pipe_putc PROTO((int, PIPE_S *));
int         pipe_puts PROTO((char *, PIPE_S *));
char       *pipe_gets PROTO((char *, int, PIPE_S *));
int         pipe_readc PROTO((unsigned char *, PIPE_S *));
int         pipe_writec PROTO((int, PIPE_S *));
int         pipe_close_write PROTO((PIPE_S *));
char	   *smtp_command PROTO((char *));
int	    mta_handoff PROTO((METAENV *, BODY *, char *, size_t));
char	   *post_handoff PROTO((METAENV *, BODY *, char *, size_t));
void	    exec_mailcap_cmd PROTO((MCAP_CMD_S *, char *, int));
int	    exec_mailcap_test_cmd PROTO((char *));
#ifdef DEBUG
void	    init_debug PROTO((void));
void	    save_debug_on_crash PROTO((FILE *));
void	    debugjournal_to_file PROTO((FILE *));
int	    do_debug PROTO((FILE *));
char	   *debug_time PROTO((int, int));
#endif
#ifdef	DOS
void	   *dos_cache PROTO((MAILSTREAM *, long, long));
char	   *dos_gets PROTO((readfn_t, void *, unsigned long));
#endif
#ifdef	_WINDOWS
void	    scroll_setrange PROTO((long page, long max));
void	    scroll_setpos PROTO((long pos));
long	    scroll_getpos PROTO((void));
long	    scroll_getscrollto PROTO((void));
char	   *pcpine_general_help PROTO((char *));
char	   *pcpine_help PROTO((HelpType));
int         init_install_get_vars PROTO((void));
#endif
#ifdef	MOUSE
unsigned long mouse_in_main PROTO((int, int, int));
#endif
int	    open_printer PROTO((char *));
void	    close_printer PROTO((void));
int	    print_char PROTO((int));
void	    print_text PROTO((char *));
void	    print_text1 PROTO((char *, char *));
void	    print_text2 PROTO((char *, char *, char *));
void	    print_text3 PROTO((char *, char *, char *, char *));
int	    get_time PROTO((TIMEVAL_S *));
long        time_diff PROTO((TIMEVAL_S *, TIMEVAL_S *));
int         mime_os_specific_access PROTO((void));
int         mime_get_os_mimetype_command PROTO((char *, char *,
					   char *, int, int, int *));
int         mime_get_os_mimetype_from_ext PROTO((char *, char *, int));
int         mime_get_os_ext_from_mimetype PROTO((char *, char *, int));
char       *url_os_specified_browser PROTO((char *));
char       *execview_pretty_command PROTO((MCAP_CMD_S *, int *));

/*--- other.c ---*/
int         select_from_list_screen PROTO((LIST_SEL_S *, unsigned long,
					   char *, char *,
					   HelpType, char *));
int	    lock_keyboard PROTO((void));
void	    redraw_kl_body PROTO(());
void	    redraw_klocked_body PROTO(());
void	    option_screen PROTO((struct pine *, int));
void	    flag_maintenance_screen PROTO((struct pine *,
					   struct flag_screen *));
CONTEXT_S  *context_select_screen PROTO((struct pine *, CONT_SCR_S *, int));
void        context_config_screen PROTO((struct pine *, CONT_SCR_S *, int));
void	    parse_printer PROTO ((char *, char **, char **, char **,
				  char **, char **, char **));
#ifdef	ENABLE_LDAP
int         ldap_addr_select PROTO((struct pine *, ADDR_CHOOSE_S *,
				    LDAP_SERV_RES_S **, LDAPLookupStyle,
				    WP_ERR_S *));
void	    directory_config PROTO((struct pine *, int));
void        color_config_screen PROTO((struct pine *, int));
void        convert_to_remote_config PROTO((struct pine *, int));
NAMEVAL_S  *ldap_search_rules PROTO((int));
NAMEVAL_S  *ldap_search_types PROTO((int));
NAMEVAL_S  *ldap_search_scope PROTO((int));
#endif
void	    role_take PROTO((struct pine *, MSGNO_S *, int));
void        role_config_screen PROTO((struct pine *, long, int));
int         role_select_screen PROTO((struct pine *, ACTION_S **, int));
void        role_process_filters PROTO(());
NAMEVAL_S  *pat_fldr_types PROTO((int));
NAMEVAL_S  *abookfrom_fldr_types PROTO((int));
NAMEVAL_S  *filter_types PROTO((int));
NAMEVAL_S  *role_repl_types PROTO((int));
NAMEVAL_S  *role_forw_types PROTO((int));
NAMEVAL_S  *role_comp_types PROTO((int));
NAMEVAL_S  *role_status_types PROTO((int));
NAMEVAL_S  *msg_state_types PROTO((int));
void	    set_feature PROTO((char ***, char *, int));
#ifndef DOS
void	    select_printer PROTO((struct pine *, int));
#endif

/*-- pine.c --*/
void	    main_menu_screen PROTO((struct pine *));
void	    show_main_screen PROTO((struct pine *, int, OtherMenu,
				    struct key_menu *, int, Pos *));
void	    news_screen PROTO((struct pine *));
void	    quit_screen PROTO((struct pine *));
int	    rule_setup_type PROTO((struct pine *, int, char *));
long	    count_flagged PROTO((MAILSTREAM *, long));
void	    panic PROTO((char *));
void	    panic1 PROTO((char *, char *));
MAILSTREAM *same_stream PROTO((char *, MAILSTREAM *));
MAILSTREAM *same_stream_and_mailbox PROTO((char *, MAILSTREAM *));
MsgNo	    first_sorted_flagged PROTO((unsigned long, MAILSTREAM *, long,
					int));
MsgNo	    next_sorted_flagged PROTO((unsigned long, MAILSTREAM *,
				       long, int *));
long	    any_lflagged PROTO((MSGNO_S *, int));
int	    get_lflag PROTO((MAILSTREAM *, MSGNO_S *, long, int));
int	    set_lflag PROTO((MAILSTREAM *, MSGNO_S *, long, int, int));
void	    warn_other_cmds PROTO(());
MAILSTREAM *pine_mail_open PROTO((MAILSTREAM *, char *, long, long *));
long        pine_mail_create PROTO((MAILSTREAM *, char *));
long        pine_mail_delete PROTO((MAILSTREAM *, char *));
long        pine_mail_append_full PROTO((MAILSTREAM *, char *, char *, char *,
					 STRING *));
long        pine_mail_append_multiple PROTO((MAILSTREAM *, char *, append_t,
					     void *, MAILSTREAM *));
long        pine_mail_copy_full PROTO((MAILSTREAM *, char *, char *, long));
void	    pine_mail_close PROTO((MAILSTREAM *));
void	    maybe_kill_old_stream PROTO((MAILSTREAM *));
long        pine_mail_rename PROTO((MAILSTREAM *, char *, char *));
long        pine_mail_search_full PROTO((MAILSTREAM *, char *, SEARCHPGM *,
					 long));
void        pine_mail_fetch_flags PROTO((MAILSTREAM *, char *, long));
ENVELOPE   *pine_mail_fetchenvelope PROTO((MAILSTREAM *, unsigned long));
ENVELOPE   *pine_mail_fetch_structure PROTO((MAILSTREAM *, unsigned long,
					     BODY **, long));
ENVELOPE   *pine_mail_fetchstructure PROTO((MAILSTREAM *, unsigned long,
					    BODY **));
long        pine_mail_ping PROTO((MAILSTREAM *));
void        pine_mail_check PROTO((MAILSTREAM *));
MAILSTREAM *already_open_stream PROTO((char *, int));
unsigned long pine_gets_bytes PROTO((int));
int	    is_imap_stream PROTO((MAILSTREAM *));
int	    modern_imap_stream PROTO((MAILSTREAM *));
PER_STREAM_S **sp_data PROTO((MAILSTREAM *));
MSGNO_S    *sp_msgmap PROTO((MAILSTREAM *));
void        sp_set_fldr PROTO((MAILSTREAM *, char *));
void        sp_set_saved_cur_msg_id PROTO((MAILSTREAM *, char *));
void        sp_free_callback PROTO((void **));
void        sp_flag PROTO((MAILSTREAM *, unsigned long));
void        sp_unflag PROTO((MAILSTREAM *, unsigned long));
int         sp_flagged PROTO((MAILSTREAM *, unsigned long));
void        sp_mark_stream_dead PROTO((MAILSTREAM *));
MAILSTREAM *sp_stream_get PROTO((char *, unsigned long));
int         sp_a_locked_stream_is_dead PROTO((void));
int         sp_a_locked_stream_changed PROTO((void));
MAILSTREAM *sp_inbox_stream PROTO((void));
void        sp_cleanup_dead_streams PROTO((void));
int         sp_nremote_permlocked PROTO((void));
void        sp_end PROTO((void));

/*-- reply.c --*/
void	    reply PROTO((struct pine *, ACTION_S *));
void	    reply_seed PROTO((struct pine *, ENVELOPE *, ENVELOPE *,
			      ADDRESS *, ADDRESS *, ADDRESS *, ADDRESS *,
			      BUILDER_ARG *, int));
int	    reply_harvest PROTO((struct pine *, long, char *,
				 ENVELOPE *, ADDRESS **, ADDRESS **,
				 ADDRESS **, ADDRESS **,int *));
int	    reply_news_test PROTO((ENVELOPE *, ENVELOPE *));
int	    reply_text_query PROTO((struct pine *, long, char **));
BODY	   *reply_body PROTO((MAILSTREAM *, ENVELOPE *, BODY *, long,
			      char *, void *, char *, int, ACTION_S *, int,
			      REDRAFT_POS_S **));
char	   *reply_subject PROTO((char *, char *, size_t));
void	    reply_delimiter PROTO((ENVELOPE *, ACTION_S *, gf_io_t));
char	   *reply_in_reply_to PROTO((ENVELOPE *));
char	   *reply_build_refs PROTO((ENVELOPE *));
char	   *reply_quote_str PROTO((ENVELOPE *));
char       *reply_quote_initials PROTO((char *));
ADDRESS    *reply_cp_addr PROTO((struct pine *, long, char *, char *,
				 ADDRESS *, ADDRESS *, ADDRESS *, int));
void	    forward PROTO((struct pine *, ACTION_S *));
char	   *forward_subject PROTO((ENVELOPE *, int));
BODY	   *forward_body PROTO((MAILSTREAM *, ENVELOPE *, BODY *, long,
				char *, void *, int));
int	    forward_mime_msg PROTO((MAILSTREAM *, long, char *,
				    ENVELOPE *, PART **, void *));
int	    fetch_contents PROTO((MAILSTREAM *, long, char *, BODY *));
void	    bounce PROTO((struct pine *, ACTION_S *));
char	   *bounce_msg PROTO((MAILSTREAM *, long, char *, ACTION_S *,
			      char **, char *, char *, char *));
void	    forward_text PROTO((struct pine *, void *, SourceType));
char	   *generate_message_id PROTO((void));
ADDRESS    *first_addr PROTO((ADDRESS *));
char	   *signature_path PROTO((char *, char *, size_t));
char       *read_remote_sigfile PROTO((char *));
char	   *signature_edit PROTO((char *, char *));
char	   *signature_edit_lit PROTO((char *, char **, char *, HelpType));
char       *get_signature_file PROTO((char *, int, int, int));
ENVELOPE   *copy_envelope PROTO((ENVELOPE *));
BODY	   *copy_body PROTO((BODY *, BODY *));
PARAMETER  *copy_parameters PROTO((PARAMETER *));
int	    get_body_part_text PROTO((MAILSTREAM *, BODY *, long, char *,
				      gf_io_t, char *));
char	   *body_partno PROTO((MAILSTREAM *, long, BODY *));
char	   *partno PROTO((BODY *, BODY *));
ACTION_S   *set_role_from_msg PROTO((struct pine *, long, long, char *));
int	    confirm_role PROTO((long, ACTION_S **));
char	   *detoken PROTO((ACTION_S *, ENVELOPE *, int, int, int,
			   REDRAFT_POS_S **, int *));
char       *detoken_src PROTO((char *, int, ENVELOPE *, ACTION_S *,
			      REDRAFT_POS_S **, int *));
void	    free_redraft_pos PROTO((REDRAFT_POS_S **));
void	    standard_picobuf_setup PROTO((PICO *));
void	    standard_picobuf_teardown PROTO((PICO *));

/*-- screen.c --*/
void	    draw_keymenu PROTO((struct key_menu *, bitmap_t, int, int,
				int, OtherMenu));
void	    blank_keymenu PROTO((int, int));
void	    draw_cancel_keymenu PROTO((void));
void	    redraw_keymenu PROTO((void));
void	    mark_keymenu_dirty PROTO((void));
void	    mark_titlebar_dirty PROTO((void));
TITLE_S	   *format_titlebar PROTO((void));
char	   *set_titlebar PROTO((char *, MAILSTREAM *, CONTEXT_S *, char *,
				MSGNO_S *, int, TitleBarType, long, long,
				COLOR_PAIR *));
TITLEBAR_STATE_S *save_titlebar_state PROTO((void));
void	    restore_titlebar_state PROTO((TITLEBAR_STATE_S *));
void	    free_titlebar_state PROTO((TITLEBAR_STATE_S **));
void	    redraw_titlebar PROTO((void));
void	    update_titlebar_message PROTO((void));
int	    update_titlebar_status PROTO((void));
void	    update_titlebar_percent PROTO((long));
void	    update_titlebar_lpercent PROTO((long));
void	    clearfooter PROTO((struct pine *));
void	    end_titlebar PROTO((void));
void	    end_keymenu PROTO((void));
void        check_cue_display PROTO((char *));

/*-- send.c --*/
void	    compose_screen PROTO((struct pine *)); 
void	    alt_compose_screen PROTO((struct pine *)); 
void	    compose_mail PROTO((char *, char *, ACTION_S *, PATMT *, gf_io_t));
void	    pine_send PROTO((ENVELOPE *, BODY **, char *, ACTION_S *,
			     char *, REPLY_S *, REDRAFT_POS_S *, char *,
			     PINEFIELD *, int));
int	    pine_simple_send PROTO((ENVELOPE *, BODY **, ACTION_S *, char *,
				    char *, char **, int));
char	   *pine_send_status PROTO((int, char *, char *, int *));
void	    phone_home PROTO((char *));
void	    pine_free_body PROTO((BODY **));
void	    simple_header_parse PROTO((char *, char **, char **));
int	    valid_subject PROTO((char *, char **, char **,BUILDER_ARG *,int *));
long	    new_mail_for_pico PROTO((int, int));
int	    display_message_for_pico PROTO((int));
void	    cmd_input_for_pico PROTO((void));
int	    upload_msg_to_pico PROTO((char *, long *));
char	   *checkpoint_dir_for_pico PROTO((char *, int));
void	    set_mime_type_by_grope PROTO((BODY *, char *));
void	    resize_for_pico PROTO((void));
STORE_S	   *open_fcc PROTO((char *, CONTEXT_S **, int, char *, char *));
int	    write_fcc PROTO((char *, CONTEXT_S *, STORE_S *, MAILSTREAM *,
			     char *, char *));
void	    free_headents PROTO((struct headerentry **));
void	    free_attachment_list PROTO((PATMT **));
FieldType   pine_header_standard PROTO((char *));
ADDRESS    *generate_from PROTO((void));
PCOLORS    *colors_for_pico PROTO((void));
void        free_pcolors PROTO((PCOLORS **));
void        customized_hdr_setup PROTO((PINEFIELD *, char **, CustomType));

/*-- signals.c --*/
int	    busy_alarm PROTO((unsigned, char *, percent_done_t, int));
void	    suspend_busy_alarm PROTO((void));
void	    resume_busy_alarm PROTO((unsigned));
void	    cancel_busy_alarm PROTO((int)); 
void	    fake_alarm_blip PROTO((void));
void	    init_signals PROTO((void));
void	    init_sighup PROTO((void));
void	    end_sighup PROTO((void));
void	    init_sigwinch PROTO((void));
int	    ttyfix PROTO((int));
int	    do_suspend PROTO((void));
void	    fix_windsize PROTO((struct pine *));
void	    winch_cleanup PROTO((void));
void	    end_signals PROTO((int));
SigType	    hup_signal PROTO(());
SigType	    term_signal PROTO(());
SigType	    child_signal PROTO(());
void	    intr_allow PROTO((void));
void	    intr_disallow PROTO((void));
void	    intr_handling_on PROTO((void));
void	    intr_handling_off PROTO((void));
void	   *alrm_signal_block PROTO((void));
void	    alrm_signal_unblock PROTO((void *));
void	    user_input_timeout_exit PROTO((int));

/*-- status.c --*/
int	    display_message PROTO((int));
void	    d_q_status_message PROTO((void));
void	    q_status_message PROTO(( int, int, int, char *));
void	    q_status_message1 PROTO((int, int, int, char *, void *));
void	    q_status_message2 PROTO((int, int, int, char *, void *, void *));
void	    q_status_message3 PROTO((int, int, int, char *, void *, void *,
				     void *));
void	    q_status_message4 PROTO((int, int, int, char *, void *, void *,
				     void *, void *));
void	    q_status_message5 PROTO((int, int, int, char *, void *, void *,
				     void *, void *, void *));
void	    q_status_message6 PROTO((int, int, int, char *, void *, void *,
				     void *, void *, void *, void *));
void	    q_status_message7 PROTO((int, int, int, char *, void *, void *,
				     void *, void *, void *, void *, void *));
void	    q_status_message8 PROTO((int, int, int, char *, void *, void *,
				     void *, void *, void *, void *, void *,
				     void *));
int	    messages_queued PROTO((long *));
char	   *last_message_queued PROTO((void));
int	    status_message_remaining PROTO((void));
int	    status_message_write PROTO((char *, int));
void	    flush_status_messages PROTO((int));
void	    flush_ordered_messages PROTO((void));
void	    mark_status_dirty PROTO((void));
void	    mark_status_unknown PROTO((void));
int	    want_to PROTO((char *, int, int, HelpType, int));
int	    one_try_want_to PROTO((char *, int, int, HelpType, int));
int	    radio_buttons PROTO((char *, int, ESCKEY_S *, int, int, HelpType,
				 int, int *));
int	    double_radio_buttons PROTO((char *, int, ESCKEY_S *,
					int, int, HelpType, int));

/*-- strings.c --*/
char	   *rplstr PROTO((char *, int, char *));
void	    sqzspaces PROTO((char *));
void	    sqznewlines PROTO((char *));
void	    removing_trailing_white_space PROTO((char *));
void	    removing_leading_white_space PROTO((char *));
void	    removing_leading_and_trailing_white_space PROTO((char *));
int 	    removing_double_quotes PROTO((char *));
char	   *skip_white_space PROTO((char *));
char	   *skip_to_white_space PROTO((char *));
char	   *removing_quotes PROTO((char *));
char	   *strclean PROTO((char *));
char	   *short_str PROTO((char *, char *, int, WhereDots));
int	    in_dir PROTO((char *, char *));
char	   *srchstr PROTO((char *, char *));
char	   *srchrstr PROTO((char *, char *));
char	   *strindex PROTO((char *, int));
char	   *strrindex PROTO((char *, int));
void	    sstrcpy PROTO((char **, char *));
void	    sstrncpy PROTO((char **, char *, int));
char	   *istrncpy PROTO((char *, char *, int));
CONV_TABLE *conversion_table PROTO((char *, char *));
char	   *month_abbrev PROTO((int));
char	   *month_name PROTO((int));
char	   *week_abbrev PROTO((int));
int	    month_num PROTO((char *));
void	    parse_date PROTO((char *, struct date *));
int	    compare_dates PROTO((MESSAGECACHE *, MESSAGECACHE *));
void	    convert_to_gmt PROTO((MESSAGECACHE *));
char	   *pretty_command PROTO((int));
char	   *repeat_char PROTO((int, int));
char	   *comatose PROTO((long));
char	   *byte_string PROTO((long));
char	   *enth_string PROTO((int));
char       *fold PROTO((char *, int, int, int, int, char *, char *));
char	   *strsquish PROTO((char *, char *, int));
char	   *long2string PROTO((long));
char	   *int2string PROTO((int));
char	   *strtoval PROTO((char *, int *, int, int, int, char *, char *));
char	   *strtolval PROTO((char *, long *, long, long, long, char *, char *));
void	    get_pair PROTO((char *, char **, char **, int, int));
char	   *put_pair PROTO((char *, char *));
char       *quote_if_needed PROTO((char *));
int	    read_hex PROTO((char *));
char	   *string_to_cstring PROTO((char *));
char	   *cstring_to_hexstring PROTO((char *));
void	    cstring_to_string PROTO((char *, char *));
char	   *add_backslash_escapes PROTO((char *));
char	   *add_roletake_escapes PROTO((char *));
char	   *add_comma_escapes PROTO((char *));
char	   *add_viewerhdr_escapes PROTO((char *));
char	   *remove_backslash_escapes PROTO((char *));
char	   *vcard_escape PROTO((char *));
char	   *vcard_unescape PROTO((char *));
void	    vcard_unfold PROTO((char *));
int	    isxpair PROTO((char *));
STRLIST_S  *new_strlist PROTO((void));
STRLIST_S  *copy_strlist PROTO((STRLIST_S *));
int         compare_strlists_for_match PROTO((STRLIST_S *, STRLIST_S *));
void	    free_strlist PROTO((STRLIST_S **));
KEYWORD_S  *init_keyword_list PROTO((char **));
void	    free_keyword_list PROTO((KEYWORD_S **));
char       *nick_to_keyword PROTO((char *));
unsigned char *rfc1522_decode PROTO((unsigned char *, size_t, char *, char **));
char	   *rfc1522_encode PROTO((char *, size_t, unsigned char *, char *));
char	   *rfc1738_scan PROTO((char *, int *));
char	   *rfc1738_str PROTO((char *));
long	    rfc1738_num PROTO((char **));
int	    rfc1738_group PROTO((char *));
char	   *rfc1738_encode_mailto PROTO((char *));
int	    rfc1808_tokens PROTO((char *, char **, char **, char **,
				  char **, char **, char **));
char	   *web_host_scan PROTO((char *, int *));
char	   *mail_addr_scan PROTO((char *, int *));
char	   *rfc2231_get_param PROTO((PARAMETER *, char *, char **, char **));
int	    rfc2231_output PROTO((STORE_S *, char *, char *, char *, char *));
PARMLIST_S *rfc2231_newparmlist PROTO((PARAMETER *));
void	    rfc2231_free_parmlist PROTO((PARMLIST_S **));
int	    rfc2231_list_params PROTO((PARMLIST_S *));
void        set_pathandle PROTO((long));
int         nonempty_patterns PROTO((long, PAT_STATE *));
int         any_patterns PROTO((long, PAT_STATE *));
int         write_patterns PROTO((long));
void        close_every_pattern PROTO((void));
void        close_patterns PROTO((long));
int         match_pattern PROTO((PATGRP_S *, MAILSTREAM *,
				 SEARCHSET *,char *,
				 long (*) PROTO((MAILSTREAM *, long)),
				 long));
int	    match_pattern_folder PROTO((PATGRP_S *, MAILSTREAM *));
int	    match_pattern_folder_specific PROTO((PATTERN_S *, MAILSTREAM *,
						 int));
SEARCHPGM  *match_pattern_srchpgm PROTO((PATGRP_S   *, MAILSTREAM *,
					 char **, SEARCHSET *));
PAT_S      *first_pattern PROTO((PAT_STATE *));
PAT_S      *last_pattern PROTO((PAT_STATE *));
PAT_S      *prev_pattern PROTO((PAT_STATE *));
PAT_S      *next_pattern PROTO((PAT_STATE *));
PAT_LINE_S *parse_pat_file PROTO((char *));
PATTERN_S  *parse_pattern PROTO((char *, char *, int));
void        free_pat PROTO((PAT_S **));
void        free_pattern PROTO((PATTERN_S **));
void        free_action PROTO((ACTION_S **));
char	   *pattern_to_string PROTO((PATTERN_S *));
PATTERN_S  *string_to_pattern PROTO((char *));
PATTERN_S  *editlist_to_pattern PROTO((char **));
char      **pattern_to_editlist PROTO((PATTERN_S *));
PAT_S      *copy_pat PROTO((PAT_S *));
PATGRP_S   *copy_patgrp PROTO((PATGRP_S *));
ACTION_S   *copy_action PROTO((ACTION_S *));
ACTION_S   *combine_inherited_role PROTO((ACTION_S *));
char       *stringform_of_intvl PROTO((INTVL_S *));
INTVL_S    *parse_intvl PROTO((char *));
void        convert_statebits_to_vals PROTO((long, int *, int *, int *, int *));
int         scores_are_used PROTO((int));
int         patgrp_depends_on_state PROTO((PATGRP_S *));
int         patgrp_depends_on_active_state PROTO((PATGRP_S *));
void        calc_extra_hdrs PROTO((void));
char       *get_extra_hdrs PROTO((void));
void        free_extra_hdrs PROTO((void));
char       *choose_item_from_list PROTO((char **, char *, char *, HelpType,
					 char *));
void        free_list_sel PROTO((LIST_SEL_S **));
char	  **rfc2369_hdrs PROTO((char **));
int	    rfc2369_parse_fields PROTO((char *, RFC2369_S *));
unsigned char *trans_euc_to_2022_jp PROTO((unsigned char *));
unsigned char *trans_2022_jp_to_euc PROTO((unsigned char *, unsigned int *));
char       *keyword_to_nick PROTO((char *));
void        find_8bitsubj_in_messages PROTO((MAILSTREAM *, SEARCHSET *,
					     int, int));
void        find_charsets_in_messages PROTO((MAILSTREAM *, SEARCHSET *,
					     PATGRP_S *, int));


/*-- takeaddr.c --*/
char	  **detach_vcard_att PROTO ((MAILSTREAM *, long, BODY *, char *));
void	    cmd_take_addr PROTO((struct pine *, MSGNO_S *, int));
void	    save_vcard_att PROTO((struct pine *, int, long, ATTACH_S *));
ADDRESS    *copyaddr PROTO((ADDRESS *));
ADDRESS    *copyaddrlist PROTO((ADDRESS *));
#ifdef	ENABLE_LDAP
void        save_ldap_entry PROTO((struct pine *, LDAP_SERV_RES_S *, int));
#endif

/*-- ttyin.c--*/
int	    read_char PROTO((int));
int	    read_command PROTO(());
int	    optionally_enter PROTO((char *, int, int, int, char *,
				    ESCKEY_S *, HelpType, int *));
int	    pre_screen_config_opt_enter PROTO((char *, int, char *,
					       ESCKEY_S *, HelpType, int *));
#ifdef	_WINDOWS
int	    win_dialog_opt_enter (char *, int, char *, ESCKEY_S *, HelpType,
				  int *);
#endif
int	    init_tty_driver PROTO((struct pine *));
void	    tty_chmod PROTO((struct pine *, int, int));
void	    end_tty_driver PROTO((struct pine *));
int	    PineRaw PROTO((int));
void	    end_keyboard PROTO((int));
void	    init_keyboard PROTO((int));
int	    validatekeys PROTO((int));
int	    key_recorder PROTO((int));
int	    key_playback PROTO((int));

/*-- ttyout.c --*/
int	    get_windsize PROTO((struct ttyo *));
int	    BeginScroll PROTO((int, int));
void	    EndScroll PROTO((void));
int	    ScrollRegion PROTO(( int));
void	    Writechar PROTO((unsigned int, int));
void	    Write_to_screen PROTO((char *));
void	    Write_to_screen_n PROTO((char *, int));
void	    PutLine0 PROTO((int, int, char *));
void	    PutLine0n8b PROTO((int, int, char *, int, HANDLE_S *));
void	    PutLine1 PROTO((int, int, char *, void *));
void	    PutLine2 PROTO((int, int, char *, void *, void *));
void	    PutLine3 PROTO((int, int, char *, void *, void *, void *));
void	    PutLine4 PROTO((int, int, char *, void *, void *, void *, void *));
void	    PutLine5 PROTO((int, int, char *, void *, void *, void *, void *,
			    void *));
void	    CleartoEOLN PROTO((void));
void	    CleartoEOS PROTO((void));
void	    ClearScreen PROTO((void));
void	    ClearLine PROTO((int));
void	    ClearLines PROTO((int, int));
void	    MoveCursor PROTO((int, int));
void	    NewLine PROTO((void));
int	    config_screen PROTO((struct ttyo **));
void	    init_screen PROTO((void));
void	    end_screen PROTO((char *, int));
void	    outchar PROTO((int));
void	    icon_text PROTO((char *, int));
void	    clear_cursor_pos PROTO((void));
int	    InsertChar PROTO((int));
int	    DeleteChar PROTO((int));

#define SCREEN_FUN_NULL ((void (*) PROTO((void *)))NULL)

#endif /* _PINE_INCLUDED */
