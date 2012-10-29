/*
 * $Id: efunc.h 13711 2004-06-15 22:22:35Z hubert $
 *
 * Program:	Pine's composer and pico's function declarations
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
 *
 */
/*	EFUNC.H:	MicroEMACS function declarations and names

		This file list all the C code functions used by MicroEMACS
	and the names to use to bind keys to them. To add functions,
	declare it here in both the extern function list and the name
	binding table.

	Update History:

	Daniel Lawrence
*/

#ifndef	EFUNC_H
#define	EFUNC_H


/*	External function declarations		*/
/* attach.c */
extern	int AskAttach PROTO((char *, LMLIST **));
extern	int SyncAttach PROTO((void));
extern	int intag PROTO((char *, int));
extern	char *prettysz PROTO((off_t));
extern  int AttachError PROTO((void));
extern	char *QuoteAttach PROTO((char *));

/* basic.c */
extern	int gotobol PROTO((int, int));
extern	int backchar PROTO((int, int));
extern	int gotoeol PROTO((int, int));
extern	int forwchar PROTO((int, int));
extern	int gotoline PROTO((int, int));
extern	int gotobob PROTO((int, int));
extern	int gotoeob PROTO((int, int));
extern	int forwline PROTO((int, int));
extern	int backline PROTO((int, int));
extern	int gotobop PROTO((int, int));
extern	int gotoeop PROTO((int, int));
extern	int forwpage PROTO((int, int));
extern	int backpage PROTO((int, int));
extern  int scrollupline PROTO((int, int));
extern  int scrolldownline PROTO((int, int));
extern  int scrollto PROTO((int, int));
extern	int setmark PROTO((int, int));
extern	int swapmark PROTO((int, int));
extern	int setimark PROTO((int, int));
extern	int swapimark PROTO((int, int));
extern	int mousepress PROTO((int, int));

/* bind.c */
extern	int whelp PROTO((int, int));
extern	int wscrollw PROTO((int, int, char **, int));
extern	int normal PROTO((int, int (*)[2], int));
extern	void rebindfunc PROTO((int (*)(int, int),int (*)(int, int)));
extern	int bindtokey PROTO((int c, int (*) PROTO((int, int))));

/* browse.c */
extern	int FileBrowse PROTO((char *, int, char *, int, char *, int,
			      LMLIST **));
extern	int ResizeBrowser PROTO((void));
extern	void set_browser_title PROTO((char *));
extern  void zotlmlist PROTO((LMLIST *));

/* buffer.c */
extern	int anycb PROTO((void));
extern	struct BUFFER *bfind PROTO((char *, int, int));
extern	int bclear PROTO((struct BUFFER *));
extern	int packbuf PROTO((char **, int *, int));
extern	void readbuf PROTO((char **));

/* composer.c */
extern	int InitMailHeader PROTO((struct pico_struct *));
extern	int ResizeHeader PROTO((void));
extern	int HeaderEditor PROTO((int, int));
extern	void PaintHeader PROTO((int, int));
extern	void ArrangeHeader PROTO((void));
extern	int ToggleHeader PROTO((int));
extern	int HeaderLen PROTO((void));
extern	int UpdateHeader PROTO((int));
extern	int entry_line PROTO((int, int));
extern	int call_builder PROTO((struct headerentry *, int *, char **));
extern	void call_expander PROTO((void));
extern	void ShowPrompt PROTO((void));
extern	int packheader PROTO((void));
extern	void zotheader PROTO((void));
extern	void display_for_send PROTO((void));
extern	VARS_TO_SAVE *save_pico_state PROTO((void));
extern	void restore_pico_state PROTO((VARS_TO_SAVE *));
extern	void free_pico_state PROTO((VARS_TO_SAVE *));
extern	void HeaderPaintCursor PROTO((void));
extern	void PaintBody PROTO((int));

/* display.c */
extern	int vtinit PROTO((void));
extern	int vtterminalinfo PROTO((int));
extern	void vttidy PROTO((void));
extern	void update PROTO((void));
extern	void modeline PROTO((struct WINDOW *));
extern	void movecursor PROTO((int, int));
extern	void clearcursor PROTO((void));
extern	void mlerase PROTO((void));
extern	int mlyesno PROTO((char *, int));
extern	int mlreply PROTO((char *, char *, int, int, EXTRAKEYS *));
extern	int mlreplyd PROTO((char *, char *, int, int, EXTRAKEYS *));
extern	void emlwrite PROTO((char *, void *));
extern	int mlwrite PROTO((char *, void *));
extern	void scrolldown PROTO((struct WINDOW *, int, int));
extern	void scrollup PROTO((struct WINDOW *, int, int));
extern	int doton PROTO((int *, unsigned int *));
extern	int resize_pico PROTO((int, int));
extern	void zotdisplay PROTO((void));
extern	void pputc PROTO((int, int));
extern	void pputs PROTO((char *, int));
extern	void peeol PROTO((void));
extern	CELL *pscr PROTO((int, int));
extern	void pclear PROTO((int, int));
extern	int pinsert PROTO((CELL));
extern	int pdel PROTO((void));
extern	void wstripe PROTO((int, int, char *, int));
extern	void wkeyhelp PROTO((KEYMENU *));
extern	void get_cursor PROTO((int *, int *));

/* file.c */
extern	int fileread PROTO((int, int));
extern	int insfile PROTO((int, int));
extern	int readin PROTO((char *, int, int));
extern	int filewrite PROTO((int, int));
extern	int filesave PROTO((int, int));
extern	int writeout PROTO((char *, int));
extern	char *writetmp PROTO((int, char *));
extern	int filename PROTO((int, int));
extern	int in_oper_tree PROTO((char *));

/* fileio.c */
extern	int ffropen PROTO((char *));
extern	int ffputline PROTO((CELL *, int));
extern	int ffgetline PROTO((char *, int, int *, int));

/* line.c */
extern	struct LINE *lalloc PROTO((int));
extern	void lfree PROTO((struct LINE *));
extern	void lchange PROTO((int));
extern	int linsert PROTO((int, int));
extern	int geninsert PROTO((LINE **, int *, LINE *, int, int, int, long *));
extern	int lnewline PROTO((void));
extern	int ldelete PROTO((long, int (*) PROTO((int))));
extern	int lisblank PROTO((struct LINE *));
extern	void kdelete PROTO((void));
extern	int kinsert PROTO((int));
extern	int kremove PROTO((int));
extern	int ksize PROTO((void));
extern	void fdelete PROTO((void));
extern	int finsert PROTO((int));
extern	int fremove PROTO((int));

/* os.c */
extern	int Raw PROTO((int));
extern	void StartInverse PROTO((void));
extern	void EndInverse PROTO((void));
extern	int InverseState PROTO((void));
extern	int StartBold PROTO((void));
extern	void EndBold PROTO((void));
extern	void StartUnderline PROTO((void));
extern	void EndUnderline PROTO((void));
extern	void xonxoff_proc PROTO((int));
extern	void crlf_proc PROTO((int));
extern	void intr_proc PROTO((int));
extern	void flush_input PROTO((void));
extern	void bit_strip_off PROTO((void));
extern	void quit_char_off PROTO((void));
extern	int ttisslow PROTO((void));
extern	int input_ready PROTO((int));
extern	int read_one_char PROTO((void));
extern	SigType (*posix_signal PROTO((int, SigType (*)())))();
extern	int posix_sigunblock PROTO((int));
extern	int ttopen PROTO((void));
extern	void ttresize PROTO((void));
extern	int ttclose PROTO((void));
extern	int ttputc PROTO((int));
extern	int ttflush PROTO((void));
extern	int ttgetc PROTO((int, int (*)(), void (*)()));
extern	int simple_ttgetc PROTO((int (*)(), void (*)()));
extern	void ttgetwinsz PROTO((int *, int *));
extern	int GetKey PROTO((void));
extern	int alt_editor PROTO((int, int));
extern	void picosigs PROTO((void));
#ifdef	JOB_CONTROL
extern	int bktoshell PROTO((void));
#endif
extern	int fallowc PROTO((int));
extern	int fexist PROTO((char *, char *, off_t *));
extern	int isdir PROTO((char *, long *, time_t *));
extern	int can_access PROTO((char *, int));
extern	char *gethomedir PROTO((int *));
extern	int homeless PROTO((char *));
extern	char *errstr PROTO((int));
extern	char *getfnames PROTO((char *, char *, int *, char *));
extern	void fioperr PROTO((int, char *));
extern	void fixpath PROTO((char *, size_t));
extern	char *pfnexpand PROTO((char *, size_t));
extern	int compresspath PROTO((char *, char *, int));
extern	void tmpname PROTO((char *, char *));
extern  char *temp_nam PROTO((char *, char *));
extern  char *temp_nam_ext PROTO((char *, char *, char *));
extern	void makename PROTO((char *, char *));
extern	int copy PROTO((char *, char *));
extern	int ffwopen PROTO((char *, int));
extern	int ffclose PROTO((void));
extern	int ffelbowroom PROTO((void));
extern	FILE *P_open PROTO((char *));
extern	void P_close PROTO((FILE *));
extern	int worthit PROTO((int *));
extern	int o_insert PROTO((int));
extern	int o_delete PROTO((void));
extern	int pico_new_mail PROTO((void));
extern	int time_to_check PROTO((void));
extern	int sstrcasecmp PROTO((const QSType *, const QSType *));
extern	int strucmp PROTO((char *, char *));
extern	int struncmp PROTO((char *, char *, int));
extern	void chkptinit PROTO((char *, int));
extern	void set_collation PROTO((int, int));
extern	int (*pcollator)();
extern  COLOR_PAIR *new_color_pair PROTO((char *, char *));
extern	COLOR_PAIR *pico_get_cur_color PROTO((void));
extern	COLOR_PAIR *pico_get_rev_color PROTO((void));
extern	COLOR_PAIR *pico_get_normal_color PROTO((void));
extern	void free_color_pair PROTO((COLOR_PAIR **));
extern	void pico_nfcolor PROTO((char *));
extern	void pico_nbcolor PROTO((char *));
extern	void pico_rfcolor PROTO((char *));
extern	void pico_rbcolor PROTO((char *));
extern	int pico_set_fg_color PROTO((char *));
extern	int pico_set_bg_color PROTO((char *));
extern	COLOR_PAIR *pico_set_colorp PROTO((COLOR_PAIR *, int));
extern	COLOR_PAIR *pico_set_colors PROTO((char *, char *, int));
extern	int pico_is_good_color PROTO((char *));
extern	int pico_is_good_colorpair PROTO((COLOR_PAIR *));
extern	void pico_set_nfg_color PROTO((void));
extern	void pico_set_nbg_color PROTO((void));
extern	void pico_set_normal_color PROTO((void));
extern	int pico_usingcolor PROTO((void));
extern	char *color_to_asciirgb PROTO((char *));
extern	char *colorx PROTO((int));
extern	char *color_to_canonical_name PROTO((char *));
extern	int pico_count_in_color_table PROTO((void));
/* these color functions aren't needed for Windows */
extern	int pico_hascolor PROTO((void));
extern	void pico_toggle_color PROTO((int));
extern	unsigned pico_get_color_options PROTO((void));
extern	void pico_set_color_options PROTO((unsigned));
extern	void pico_endcolor PROTO((void));
extern	char *pico_get_last_fg_color PROTO((void));
extern	char *pico_get_last_bg_color PROTO((void));
#ifdef	MOUSE
extern	unsigned long pico_mouse PROTO((unsigned, int, int));
#endif

/* pico.c */
extern	int pico PROTO((struct pico_struct *));
extern	void edinit PROTO((char *));
extern	int execute PROTO((int, int, int));
extern	int quickexit PROTO((int, int));
extern	int abort_composer PROTO((int, int));
extern	int suspend_composer PROTO((int, int));
extern	int wquit PROTO((int, int));
extern	int ctrlg PROTO((int, int));
extern	int rdonly PROTO((void));
extern	int pico_help PROTO((char **, char *, int));
extern	void zotedit PROTO((void));
#ifdef	_WINDOWS
int	composer_file_drop PROTO((int, int, char *));
int	pico_cursor PROTO((int, long));
#endif

/* random.c */
extern	int showcpos PROTO((int, int));
extern	int tab PROTO((int, int));
extern	int newline PROTO((int, int));
extern	int forwdel PROTO((int, int));
extern	int backdel PROTO((int, int));
extern	int killtext PROTO((int, int));
extern	int yank PROTO((int, int));
extern	COLOR_PAIR *pico_apply_rev_color PROTO((COLOR_PAIR *, int));

/* region.c */
extern	int killregion PROTO((int, int));
extern	int deleteregion PROTO((int, int));
extern	int markregion PROTO((int));
extern	void unmarkbuffer PROTO((void));

/* search.c */
extern	int forwsearch PROTO((int, int));
extern	int readpattern PROTO((char *, int));
extern	int forscan PROTO((int *, char *, LINE *, int, int));
extern	void chword PROTO((char *, char *));

/* spell.c */
#ifdef	SPELLER
extern	int spell PROTO((int, int));
#endif

/* window.c */
extern	int  pico_refresh PROTO((int, int));
extern	void redraw_pico_for_callback();

/* word.c */
extern	int wrapword PROTO((void));
extern	int backword PROTO((int, int));
extern	int forwword PROTO((int, int));
extern	int fillpara PROTO((int, int));
extern	int fillbuf PROTO((int, int));
extern	int inword PROTO((void));
extern	int quote_match PROTO((char *, LINE *, char *, int));

#endif	/* EFUNC_H */
