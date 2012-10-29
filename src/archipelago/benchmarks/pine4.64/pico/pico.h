/*
 * $Id: pico.h 14019 2005-03-30 22:44:40Z jpf $
 *
 * Program:	pico.h - definitions for Pine's composer library
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 *
 * Pine and Pico are registered trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior written
 * permission of the University of Washington.
 * 
 * Pine, Pico, and Pilot software and its included text are Copyright
 * 1989-2004 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 */

#ifndef	PICO_H
#define	PICO_H
/*
 * Defined for attachment support
 */
#define	ATTACHMENTS	1


/*
 * defs of return codes from pine mailer composer.
 */
#define	BUF_CHANGED	0x01
#define	COMP_CANCEL	0x02
#define	COMP_EXIT	0x04
#define	COMP_FAILED	0x08
#define	COMP_SUSPEND	0x10
#define	COMP_GOTHUP	0x20


/*
 * top line from the top of the screen for the editor to do 
 * its stuff
 */
#define	COMPOSER_TOP_LINE	2
#define	COMPOSER_TITLE_LINE	0



/*
 * definitions of Mail header structures 
 */
struct hdr_line {
        char text[256];
        struct  hdr_line        *next;
        struct  hdr_line        *prev;
};
 
#if defined(DOS) || defined(HELPFILE)
#define	HELP_T	short
#else
#define	HELP_T	char **
#endif

 
/* 
 *  This structure controls the header line items on the screen.  An
 * instance of this should be created and passed in as an argument when
 * pico is called. The list is terminated by an entry with the name
 * element NULL.
 */
 
struct headerentry {
        char     *prompt;
	char	 *name;
	HELP_T	  help;
        int	  prlen;
        int	  maxlen;
        char    **realaddr;
        int     (*builder)();        /* Function to verify/canonicalize val */
	struct headerentry        *affected_entry, *next_affected;
				     /* entry builder's 4th arg affects     */
        char   *(*selector)();       /* Browser for possible values         */
        char     *key_label;         /* Key label for key to call browser   */
        char   *(*fileedit)();       /* Editor for file named in header     */
        unsigned  display_it:1;	     /* field is to be displayed by default */
        unsigned  break_on_comma:1;  /* Field breaks on commas              */
        unsigned  is_attach:1;       /* Special case field for attachments  */
        unsigned  rich_header:1;     /* Field is part of rich header        */
        unsigned  only_file_chars:1; /* Field is a file name                */
        unsigned  single_space:1;    /* Crush multiple spaces into one      */
        unsigned  sticky:1;          /* Can't change this via affected_entry*/
        unsigned  dirty:1;           /* We've changed this entry            */
	unsigned  start_here:1;	     /* begin composer on first on lit      */
	unsigned  blank:1;	     /* blank line separator                */
#ifdef	KS_OSDATAVAR
	KS_OSDATAVAR		     /* Port-Specific keymenu data */
#endif
	void     *bldr_private;      /* Data managed by builders            */
        struct    hdr_line        *hd_text;
};


/*
 * Structure to pass as arg to builders.
 *
 *    me -- A pointer to the bldr_private data for the entry currently
 *            being edited.
 *  tptr -- A malloc'd copy of the displayed text for the affected_entry
 *            pointed to by the  aff argument.
 *   aff -- A pointer to the bldr_private data for the affected_entry (the
 *            entry that this entry affects).
 *  next -- The next affected_entry in the list. For example, the Lcc entry
 *            affects the To entry which affects the Fcc entry.
 */
typedef struct bld_arg {
    void          **me;
    char           *tptr;
    void          **aff;
    struct bld_arg *next;
} BUILDER_ARG;

#define BUILDER_SCREEN_MANGLED 0x1
#define BUILDER_MESSAGE_DISPLAYED 0x2


/*
 * structure to keep track of header display
 */
struct on_display {
    int			 p_off;			/* offset into line */
    int			 p_len;			/* length of line   */
    int			 p_line;		/* physical line on screen */
    int			 top_e;			/* topline's header entry */
    struct hdr_line	*top_l;			/* top line on display */
    int			 cur_e;			/* current header entry */
    struct hdr_line	*cur_l;			/* current hd_line */
};						/* global on_display struct */


/*
 * Structure to handle attachments
 */
typedef struct pico_atmt {
    char *description;			/* attachment description */
    char *filename;			/* file/pseudonym for attachment */
    char *size;				/* size of attachment */
    char *id;				/* attachment id */
    unsigned short flags;
    struct pico_atmt *next;
} PATMT;

/*
 * Structure to contain color options
 */
typedef struct pico_colors {
    COLOR_PAIR *tbcp;                   /* title bar color pair */
    COLOR_PAIR *klcp;                   /* key label color pair */
    COLOR_PAIR *kncp;                   /* key name color pair  */
    COLOR_PAIR *stcp;                   /* status color pair    */
} PCOLORS;

/*
 * Flags for attachment handling
 */
#define	A_FLIT	0x0001			/* Accept literal file and size      */
#define	A_ERR	0x0002			/* Problem with specified attachment */
#define	A_TMP	0x0004			/* filename is temporary, delete it  */


/*
 * Master pine composer structure.  Right now there's not much checking
 * that any of these are pointing to something, so pine must have them pointing
 * somewhere.
 */
typedef struct pico_struct {
    void  *msgtext;			/* ptrs to malloc'd arrays of char */
    char  *pine_anchor;			/* ptr to pine anchor line */
    char  *pine_version;		/* string containing Pine's version */
    char  *oper_dir;			/* Operating dir (confine to tree) */
    char  *home_dir;                    /* Home directory that should be used (WINDOWS) */
    char  *quote_str;			/* prepended to lines of quoted text */
    char  *exit_label;			/* Label for ^X in keymenu */
    char  *ctrlr_label;			/* Label for ^R in keymenu */
    char  *alt_spell;			/* Checker to use other than "spell" */
    char **alt_ed;			/* name of alternate editor or NULL */
    int    fillcolumn;			/* where to wrap */
    int    menu_rows;			/* number of rows in menu (0 or 2) */
    long   edit_offset;			/* offset into hdr line or body */
    int   *hibit_entered;		/* hibit input came from user */
    PATMT *attachments;			/* linked list of attachments */
    PCOLORS *colors;                    /* colors for titlebar and keymenu */
    long   pine_flags;			/* entry mode flags */
    /* The next few bits are features that don't fit in pine_flags      */
    /* If we had this to do over, it would probably be one giant bitmap */
    unsigned always_spell_check:1;      /* always spell-checking upon quit */
    unsigned strip_ws_before_send:1;    /* don't default strip bc of flowed */
    unsigned allow_flowed_text:1;    /* clean text when done to keep flowed */
    int   (*helper)();			/* Pine's help function  */
    int   (*showmsg)();			/* Pine's display_message */
    int   (*suspend)();			/* Pine's suspend */
    void  (*keybinput)();		/* Pine's keyboard input indicator */
    int   (*tty_fix)();			/* Let Pine fix tty state */
    long  (*newmail)();			/* Pine's report_new_mail */
    long  (*msgntext)();		/* callback to get msg n's text */
    int   (*upload)();			/* callback to rcv uplaoded text */
    char *(*ckptdir)();			/* callback for checkpoint file dir */
    char *(*exittest)();		/* callback to verify exit request */
    char *(*canceltest)();		/* callback to verify cancel request */
    int   (*mimetype)();		/* callback to display mime type */
    int   (*expander)();		/* callback to expand address lists */
    void  (*resize)();			/* callback handling screen resize */
    void  (*winch_cleanup)();		/* callback handling screen resize */
    int    arm_winch_cleanup;		/* do the winch_cleanup if resized */
    HELP_T search_help;
    HELP_T ins_help;
    HELP_T ins_m_help;
    HELP_T composer_help;
    HELP_T browse_help;
    HELP_T attach_help;
    struct headerentry *headents;
} PICO;


/*
 * Used to save and restore global pico variables that are destroyed by
 * calling pico a second time. This happens when pico calls a selector
 * in the HeaderEditor and that selector calls pico again.
 */
typedef struct _save_stuff {
    int                 vtrow,
	                vtcol,
	                lbound;
    VIDEO             **vscreen,
	              **pscreen;	/* save pointers */
    struct on_display   ods;		/* save whole struct */
    short               delim_ps,
	                invert_ps;
    int                 pico_all_done;
    jmp_buf             finstate;
    char               *pico_anchor;	/* save pointer */
    PICO               *Pmaster;	/* save pointer */
    int                 fillcol;
    char               *pat;		/* save array */
    int                 ComposerTopLine,
                        ComposerEditing;
    long                gmode;
    char               *alt_speller;	/* save pointer */
    char               *quote_str;      /* save pointer */
    int                 currow,
	                curcol,
	                thisflag,
	                lastflag,
	                curgoal;
    char               *opertree;	/* save array */
    WINDOW             *curwp;		/* save pointer */
    WINDOW             *wheadp;		/* save pointer */
    BUFFER             *curbp;		/* save pointer */
    BUFFER             *bheadp;		/* save pointer */
    int                 km_popped;
    int                 mrow;
} VARS_TO_SAVE;


#ifdef	MOUSE
/*
 * Mouse buttons.
 */
#define M_BUTTON_LEFT		0
#define M_BUTTON_MIDDLE		1
#define M_BUTTON_RIGHT		2


/*
 * Flags.  (modifier keys)
 */
#define M_KEY_CONTROL		0x01	/* Control key was down. */
#define M_KEY_SHIFT		0x02	/* Shift key was down. */


/*
 * Mouse Events
 */
#define M_EVENT_DOWN		0x01	/* Mouse went down. */
#define M_EVENT_UP		0x02	/* Mouse went up. */
#define M_EVENT_TRACK		0x04	/* Mouse tracking */

/*
 * Mouse event information.
 */
typedef struct mouse_struct {
	char	mevent;		/* Indicates type of event:  Down, Up or 
				   Track */
	char	down;		/* TRUE when mouse down event */
	char	doubleclick;	/* TRUE when double click. */
	int	button;		/* button pressed. */
	int	flags;		/* What other keys pressed. */
	int	row;
	int	col;
} MOUSEPRESS;



typedef	unsigned long (*mousehandler_t) PROTO((int, int, int, int, int));

typedef struct point {
    unsigned	r:8;		/* row value				*/
    unsigned	c:8;		/* column value				*/
} MPOINT;


typedef struct menuitem {
    unsigned	     val;	/* return value				*/
    mousehandler_t    action;	/* action to perform			*/
    MPOINT	     tl;	/* top-left corner of active area	*/
    MPOINT	     br;	/* bottom-right corner of active area	*/
    MPOINT	     lbl;	/* where the label starts		*/
    char	    *label;
    void            (*label_hiliter)();
    COLOR_PAIR      *kncp;      /* key name color pair                  */
    COLOR_PAIR      *klcp;      /* key label color pair                 */ 
    struct menuitem *next;
} MENUITEM;
#endif


/*
 * Structure used to manage keyboard input that comes as escape
 * sequences (arrow keys, function keys, etc.)
 */
typedef struct  KBSTREE {
	char	value;
        int     func;              /* Routine to handle it         */
	struct	KBSTREE *down; 
	struct	KBSTREE	*left;
} KBESC_T;

/*
 * Protos for functions used to manage keyboard escape sequences
 * NOTE: these may ot actually get defined under some OS's (ie, DOS, WIN)
 */
extern	int	kbseq PROTO((int (*)(), int (*)(), void (*)(), int *));
extern	void	kbdestroy PROTO((KBESC_T *));


/*
 * various flags that they may passed to PICO
 */
#define P_HICTRL	0x80000000	/* overwrite mode		*/
#define	P_CHKPTNOW	0x40000000	/* do the checkpoint on entry      */
#define	P_DELRUBS	0x20000000	/* map ^H to forwdel		   */
#define	P_LOCALLF	0x10000000	/* use local vs. NVT EOL	   */
#define	P_BODY		0x08000000	/* start composer in body	   */
#define	P_HEADEND	0x04000000	/* start composer at end of header */
#define	P_VIEW		MDVIEW		/* read-only			   */
#define	P_FKEYS		MDFKEY		/* run in function key mode 	   */
#define	P_SECURE	MDSCUR		/* run in restricted (demo) mode   */
#define	P_TREE		MDTREE		/* restrict to a subtree	   */
#define	P_SUSPEND	MDSSPD		/* allow ^Z suspension		   */
#define	P_ADVANCED	MDADVN		/* enable advanced features	   */
#define	P_CURDIR	MDCURDIR	/* use current dir for lookups	   */
#define	P_ALTNOW	MDALTNOW	/* enter alt ed sans hesitation	   */
#define	P_SUBSHELL	MDSPWN		/* spawn subshell for suspend	   */
#define	P_COMPLETE	MDCMPLT		/* enable file name completion     */
#define	P_DOTKILL	MDDTKILL	/* kill from dot to eol		   */
#define	P_SHOCUR	MDSHOCUR	/* cursor follows hilite in browser*/
#define	P_HIBITIGN	MDHBTIGN	/* ignore chars with hi bit set    */
#define	P_DOTFILES	MDDOTSOK	/* browser displays dot files	   */
#define	P_NOBODY	MDHDRONLY	/* Operate only on given headers   */
#define	P_ALLOW_GOTO	MDGOTO		/* support "Goto" in file browser  */
#define P_REPLACE       MDREPLACE       /* allow "Replace" in "Where is"   */

/*
 * definitions for various PICO modes 
 */
#define	MDWRAP		0x00000001	/* word wrap			*/
#define	MDSPELL		0x00000002	/* spell error parcing		*/
#define	MDEXACT		0x00000004	/* Exact matching for searches	*/
#define	MDVIEW		0x00000008	/* read-only buffer		*/
#define MDFKEY		0x00000010	/* function key  mode		*/
#define MDSCUR		0x00000020	/* secure (for demo) mode	*/
#define MDSSPD		0x00000040	/* suspendable mode		*/
#define MDADVN		0x00000080	/* Pico's advanced mode		*/
#define MDTOOL		0x00000100	/* "tool" mode (quick exit)	*/
#define MDBRONLY	0x00000200	/* indicates standalone browser	*/
#define MDCURDIR	0x00000400	/* use current dir for lookups	*/
#define MDALTNOW	0x00000800	/* enter alt ed sans hesitation */
#define	MDSPWN		0x00001000	/* spawn subshell for suspend	*/
#define	MDCMPLT		0x00002000	/* enable file name completion  */
#define	MDDTKILL	0x00004000	/* kill from dot to eol		*/
#define	MDSHOCUR	0x00008000	/* cursor follows hilite in browser */
#define	MDHBTIGN	0x00010000	/* ignore chars with hi bit set */
#define	MDDOTSOK	0x00020000	/* browser displays dot files   */
#define	MDEXTFB		0x00040000	/* stand alone File browser     */
#define	MDTREE		0x00080000	/* confine to a subtree         */
#define	MDMOUSE		0x00100000	/* allow mouse (part. in xterm) */
#define	MDONECOL	0x00200000	/* single column browser        */
#define	MDHDRONLY	0x00400000	/* header editing exclusively   */
#define	MDGOTO		0x00800000	/* support "Goto" in file browser */
#define MDREPLACE       0x01000000      /* allow "Replace" in "Where is"*/
#define MDTCAPWINS      0x02000000      /* Termcap overrides defaults   */

/*
 * Main defs 
 */
#ifdef	maindef
PICO	*Pmaster = NULL;		/* composer specific stuff */
char	*version = "4.10";		/* PICO version number */

#else
extern	PICO *Pmaster;			/* composer specific stuff */
extern	char *version;			/* pico version! */

#endif	/* maindef */


/*
 * Flags for FileBrowser call
 */
#define FB_READ		0x0001		/* Looking for a file to read.  */
#define FB_SAVE		0x0002		/* Looking for a file to save.  */
#define	FB_ATTACH	0x0004		/* Looking for a file to attach */
#define	FB_LMODEPOS	0x0008		/* ListMode is a possibility    */
#define	FB_LMODE	0x0010		/* Using ListMode now           */


/*
 * number of keystrokes to delay removing an error message, or new mail
 * notification, or checkpointing
 */
#define	MESSDELAY	25
#define	NMMESSDELAY	60
#ifndef CHKPTDELAY
#define	CHKPTDELAY	100
#endif


/*
 * defs for keypad and function keys...
 */
#define KEY_UP		0x0811
#define KEY_DOWN	0x0812
#define KEY_RIGHT	0x0813
#define KEY_LEFT	0x0814
#define KEY_PGUP	0x0815
#define KEY_PGDN	0x0816
#define KEY_HOME	0x0817
#define KEY_END		0x0818
#define KEY_DEL		0x0819
#define BADESC		0x0820
#define KEY_MOUSE	0x0821	/* Fake key to indicate mouse event. */
#define KEY_SCRLUPL	0x0822
#define KEY_SCRLDNL	0x0823
#define KEY_SCRLTO	0x0824
#define KEY_XTERM_MOUSE	0x0825
#define KEY_DOUBLE_ESC	0x0826
#define KEY_SWALLOW_Z	0x0827
#define KEY_SWAL_UP	0x0828	/* These four have to be in the same order */
#define KEY_SWAL_DOWN	0x0829	/* as KEY_UP, KEY_DOWN, ...                */
#define KEY_SWAL_RIGHT	0x0830
#define KEY_SWAL_LEFT	0x0831
#define KEY_KERMIT	0x0832
#define KEY_JUNK	0x0840
#define KEY_RESIZE	0x0841  /* Fake key to cause resize. */
#define KEY_MENU_FLAG	0x1000
#define KEY_MASK	0x13FF

/*
 * Don't think we are using the fact that this is zero anywhere,
 * but just in case we'll leave it.
 */
#define NO_OP_COMMAND	0x0	/* no-op for short timeouts      */
#define NO_OP_IDLE	0x0842	/* no-op for >25 second timeouts */
#define READY_TO_READ	0x0843
#define BAIL_OUT	0x0844
#define PANIC_NOW	0x0845
#define READ_INTR	0x0846
#define NODATA		0x08FF

#define IDLE_TIMEOUT	(8)
#define FUDGE		(30)	/* better be at least 20 */
 
/*
 * defines for function keys
 */
#define F1      0x1001                  /* Functin key one              */
#define F2      0x1002                  /* Functin key two              */
#define F3      0x1003                  /* Functin key three            */
#define F4      0x1004                  /* Functin key four             */
#define F5      0x1005                  /* Functin key five             */
#define F6      0x1006                  /* Functin key six              */
#define F7      0x1007                  /* Functin key seven            */
#define F8      0x1008                  /* Functin key eight            */
#define F9      0x1009                  /* Functin key nine             */
#define F10     0x100A                  /* Functin key ten              */
#define F11     0x100B                  /* Functin key eleven           */
#define F12     0x100C                  /* Functin key twelve           */

/* 1st tier pine function keys */
#define PF1	F1
#define PF2	F2
#define PF3	F3
#define PF4	F4
#define PF5	F5
#define PF6	F6
#define PF7	F7
#define PF8	F8
#define PF9	F9
#define PF10	F10
#define PF11	F11
#define PF12	F12

#define PF2OPF(x)	(x + 0x10)
#define PF2OOPF(x)	(x + 0x20)
#define PF2OOOPF(x)	(x + 0x30)

/* 2nd tier pine function keys */
#define OPF1	PF2OPF(PF1)
#define OPF2	PF2OPF(PF2)
#define OPF3	PF2OPF(PF3)
#define OPF4	PF2OPF(PF4)
#define OPF5	PF2OPF(PF5)
#define OPF6	PF2OPF(PF6)
#define OPF7	PF2OPF(PF7)
#define OPF8	PF2OPF(PF8)
#define OPF9	PF2OPF(PF9)
#define OPF10	PF2OPF(PF10)
#define OPF11	PF2OPF(PF11)
#define OPF12	PF2OPF(PF12)

/* 3rd tier pine function keys */
#define OOPF1	PF2OOPF(PF1)
#define OOPF2	PF2OOPF(PF2)
#define OOPF3	PF2OOPF(PF3)
#define OOPF4	PF2OOPF(PF4)
#define OOPF5	PF2OOPF(PF5)
#define OOPF6	PF2OOPF(PF6)
#define OOPF7	PF2OOPF(PF7)
#define OOPF8	PF2OOPF(PF8)
#define OOPF9	PF2OOPF(PF9)
#define OOPF10	PF2OOPF(PF10)
#define OOPF11	PF2OOPF(PF11)
#define OOPF12	PF2OOPF(PF12)

/* 4th tier pine function keys */
#define OOOPF1	PF2OOOPF(PF1)
#define OOOPF2	PF2OOOPF(PF2)
#define OOOPF3	PF2OOOPF(PF3)
#define OOOPF4	PF2OOOPF(PF4)
#define OOOPF5	PF2OOOPF(PF5)
#define OOOPF6	PF2OOOPF(PF6)
#define OOOPF7	PF2OOOPF(PF7)
#define OOOPF8	PF2OOOPF(PF8)
#define OOOPF9	PF2OOOPF(PF9)
#define OOOPF10	PF2OOOPF(PF10)
#define OOOPF11	PF2OOOPF(PF11)
#define OOOPF12	PF2OOOPF(PF12)


/*
 * useful function definitions
 */
int   pico PROTO((PICO *));
int   pico_file_browse PROTO((PICO *, char *, int, char *, int, char *, int));
void *pico_get PROTO((void));
void  pico_give PROTO((void *));
int   pico_readc PROTO((void *, unsigned char *));
int   pico_writec PROTO((void *, int));
int   pico_puts PROTO((void *, char *));
int   pico_seek PROTO((void *, long, int));
int   pico_replace PROTO((void *, char *));
int   pico_fncomplete PROTO((char *, char *, int));
#if defined(DOS) || defined(OS2)
int   pico_nfsetcolor PROTO((char *));
int   pico_nbsetcolor PROTO((char *));
int   pico_rfsetcolor PROTO((char *));
int   pico_rbsetcolor PROTO((char *));
#endif
#ifdef	MOUSE
int   init_mouse PROTO((void));
void  end_mouse PROTO((void));
int   mouseexist PROTO((void));
int   register_mfunc PROTO((mousehandler_t, int, int, int, int));
void  clear_mfunc PROTO((mousehandler_t));
unsigned long mouse_in_content PROTO((int, int, int, int, int));
unsigned long mouse_in_pico PROTO((int, int, int, int, int));
void  mouse_get_last PROTO((mousehandler_t *, MOUSEPRESS *));
void  register_key PROTO((int, unsigned, char *, void (*)(),
			  int, int, int, COLOR_PAIR *, COLOR_PAIR *));
int   mouse_on_key PROTO((int, int));
int   checkmouse PROTO((unsigned *, int, int, int));
void  invert_label PROTO((int, MENUITEM *));
void  mouseon PROTO((void));
void  mouseoff PROTO((void));
#endif	/* MOUSE */


#endif	/* PICO_H */
