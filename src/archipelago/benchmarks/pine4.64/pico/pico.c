#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pico.c 14020 2005-03-31 17:08:28Z hubert $";
#endif
/*
 * Program:	Main Pine Composer routines
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
 * 1989-2005 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 *
 *
 * WEEMACS/PICO NOTES:
 *
 * 01 Nov 89 - MicroEmacs 3.6 vastly pared down and becomes a function call 
 * 	       weemacs() and plugged into the Pine mailer.  Lots of unused
 *	       MicroEmacs code laying around.
 *
 * 17 Jan 90 - weemacs() became weemacs() the composer.  Header editing
 *	       functions added.
 *
 * 09 Sep 91 - weemacs() renamed pico() for the PIne COmposer.
 *
 */


/*
 * This program is in public domain; written by Dave G. Conroy.
 * This file contains the main driving routine, and some keyboard processing
 * code, for the MicroEMACS screen editor.
 *
 * REVISION HISTORY:
 *
 * 1.0  Steve Wilhite, 30-Nov-85
 *      - Removed the old LK201 and VT100 logic. Added code to support the
 *        DEC Rainbow keyboard (which is a LK201 layout) using the the Level
 *        1 Console In ROM INT. See "rainbow.h" for the function key defs
 *      Steve Wilhite, 1-Dec-85
 *      - massive cleanup on code in display.c and search.c
 *
 * 2.0  George Jones, 12-Dec-85
 *      - Ported to Amiga.
 *
 * 3.0  Daniel Lawrence, 29-Dec-85
 *	16-apr-86
 *	- updated documentation and froze development for 3.6 net release
 */

/* make global definitions not external */
#define	maindef

#include	"headers.h"
#include	"ebind.h"	/* default key bindings */


void	func_init PROTO((void));
void	breplace PROTO((void *));
int	any_header_changes PROTO((void));
int     cleanwhitespace PROTO((void));
int     isquotedspace PROTO((LINE *));
#ifdef	_WINDOWS
int	composer_file_drop PROTO((int, int, char *));
#endif


/*
 * function key mappings
 */
static int pfkm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'C')},
    { F3,  (CTRL|'X')},
    { F4,  (CTRL|'J')},
    { F5,  (CTRL|'R')},
    { F6,  (CTRL|'W')},
    { F7,  (CTRL|'Y')},
    { F8,  (CTRL|'V')},
    { F9,  (CTRL|'K')},
    { F10, (CTRL|'U')},
    { F11, (CTRL|'O')},
#ifdef	SPELLER
    { F12, (CTRL|'T')}
#else
    { F12, (CTRL|'D')}
#endif
};


/*
 * flag for the various functions in pico() to set when ready
 * for pico() to return...
 */
int      pico_all_done = 0;
jmp_buf  finstate;
char    *pico_anchor = NULL;
extern struct headerentry *headents;

/*
 * pico - the main routine for Pine's composer.
 *
 */
pico(pm)
PICO *pm;
{
    register int    c;
    register int    f;
    register int    n;
    char     bname[NBUFN];		/* buffer name of file to read */
    extern   struct on_display ods;
    int      checkpointcnt = 0, input = 0;
    char     chkptfile[NLINE];
#ifdef	_WINDOWS
    int      cursor_shown;
#endif

    Pmaster       = pm;
    gmode	  = MDWRAP;
    gmode        |= pm->pine_flags;	/* high 4 bits rsv'd for pine */

    alt_speller   = pm->alt_spell;
    pico_all_done = 0;
    km_popped     = 0;

    if(!vtinit())			/* Init Displays.      */
      return(COMP_CANCEL);

    strcpy(bname, "main");		/* default buffer name */
    edinit(bname);			/* Buffers, windows.   */

    if(InitMailHeader(pm))		/* init mail header structure */
      gmode &= ~(P_BODY | P_HEADEND);	/* flip off special header stuff */

    /* setup to process commands */
    lastflag = 0;			/* Fake last flags.     */
    curbp->b_mode |= gmode;		/* and set default modes*/

    pico_anchor = (char *)malloc((strlen(Pmaster->pine_anchor) + 1)
				 * sizeof(char));
    if(pico_anchor)
      strcpy(pico_anchor, Pmaster->pine_anchor);

    bindtokey(DEL, (gmode & P_DELRUBS) ? forwdel : backdel);

    if(pm->msgtext)
      breplace(pm->msgtext);

#ifdef	_WINDOWS
    cursor_shown = mswin_showcaret(1);	/* turn on for main window */
    mswin_allowpaste(MSWIN_PASTE_FULL);
    mswin_setscrollcallback (pico_scroll_callback);
#endif

    /* prepare for checkpointing */
    chkptfile[0] = '\0';
    chkptinit((*Pmaster->ckptdir)(chkptfile, NLINE), NLINE);
    if(gmode & P_CHKPTNOW)
      writeout(chkptfile, TRUE);

    pico_all_done = setjmp(finstate);	/* jump out of HUP handler ? */

    if(gmode & MDALTNOW){
	while(!pico_all_done){
	    if(((gmode & P_BODY) || !Pmaster->headents)
	       && alt_editor(0, 1) < 0)
	      break;			/* if problem, drop into pico */

	    if(Pmaster->headents){
		update();		/* paint screen, n' start editing... */
		HeaderEditor((gmode & (P_HEADEND | P_BODY)) ? 2 : 0, 0);
		gmode |= P_BODY;	/* make sure we enter alt ed next */
	    }
	    else
	      pico_all_done = COMP_EXIT;
	}
    }
    else if(!pico_all_done){
	if(gmode & P_BODY){		/* begin editing the header? */
	    ArrangeHeader();		/* line up pointers */
	    /*
	     * Move to the offset pine asked us to move to.
	     * Perhaps we should be checking to see if this is
	     * a reasonable number before moving.
	     */
	    if(Pmaster && Pmaster->edit_offset)
	      forwchar(FALSE, Pmaster->edit_offset);
	}
	else{
	    update();			/* paint screen, */
	    HeaderEditor((gmode & P_HEADEND) ? 2 : 0, 0);
	}
    }

    while(1){
	if(pico_all_done){
#ifdef	_WINDOWS
	    if(!cursor_shown)
	      mswin_showcaret(0);

	    mswin_allowpaste(MSWIN_PASTE_DISABLE);
	    mswin_setscrollcallback (NULL);
#endif
	    c = anycb() ? BUF_CHANGED : 0;
	    switch(pico_all_done){	/* prepare for/handle final events */
	      case COMP_EXIT :		/* already confirmed */
		packheader();
		if(Pmaster 
		   && (Pmaster->strip_ws_before_send
		       || Pmaster->allow_flowed_text))
		  cleanwhitespace();
		c |= COMP_EXIT;
		break;

	      case COMP_CANCEL :	/* also already confirmed */
		packheader();
		c = COMP_CANCEL;
		break;

	      case COMP_GOTHUP:
		/* 
		 * pack up and let caller know that we've received a SIGHUP
		 */
		if(ComposerEditing)		/* expand addr if needed */
		  call_builder(&headents[ods.cur_e], NULL, NULL);

		packheader();
		c |= COMP_GOTHUP;
		break;

	      case COMP_SUSPEND :
	      default:			/* safest if internal error */
		/*
		 * If we're in the headers mark the current header line
		 * with start_here bit so caller knows where to reset.
		 * Also set the edit_offset, which is either the offset
		 * into this header line or the offset into the body.
		 * Packheader will adjust edit_offset for multi-line
		 * headers.
		 */
		if(ComposerEditing){		/* in the headers */
		    headents[ods.cur_e].start_here = 1;
		    Pmaster->edit_offset = ods.p_off;
		}
		else{
		    register LINE *clp;
		    register long  offset;

		    for(clp = lforw(curbp->b_linep), offset = 0L;
			clp != curwp->w_dotp;
			clp = lforw(clp))
		      offset += (llength(clp) + 1);

		    Pmaster->edit_offset = offset + curwp->w_doto;
		}

		packheader();
		c |= COMP_SUSPEND;
		break;
	    }

	    free(pico_anchor);
	    vttidy();			/* clean up tty modes */
	    zotdisplay();		/* blast display buffers */
	    zotedit();
	    unlink(chkptfile);
	    Pmaster = NULL;		/* blat global */

	    return(c);
	}

	if(km_popped){
	    km_popped--;
	    if(km_popped == 0) /* cause bottom three lines to be repainted */
	      curwp->w_flag |= WFHARD;
	}

	if(km_popped){  /* temporarily change to cause menu to be painted */
	    term.t_mrow = 2;
	    curwp->w_ntrows -= 2;
	    curwp->w_flag |= WFMODE;
	    movecursor(term.t_nrow-2, 0); /* clear status line, too */
	    peeol();
	}

	update();			/* Fix up the screen    */
	if(km_popped){
	    term.t_mrow = 0;
	    curwp->w_ntrows += 2;
	}

#ifdef	MOUSE
#ifdef  EX_MOUSE
	/* New mouse function for real mouse text seletion. */
	register_mfunc(mouse_in_pico, 2, 0, term.t_nrow - (term.t_mrow+1),
		       term.t_ncol);
#else
	mouse_in_content(KEY_MOUSE, -1, -1, -1, 0);
	register_mfunc(mouse_in_content, 2, 0, term.t_nrow - (term.t_mrow + 1),
		       term.t_ncol);
#endif
#endif
#ifdef	_WINDOWS
	mswin_setdndcallback (composer_file_drop);
	mswin_mousetrackcallback(pico_cursor);
#endif
	c = GetKey();
        if (term.t_nrow < 6 && c != NODATA){
            (*term.t_beep)();
            emlwrite("Please make the screen bigger.", NULL);
            continue;
        }

#ifdef	MOUSE
#ifdef  EX_MOUSE
	clear_mfunc(mouse_in_pico);
#else
	clear_mfunc(mouse_in_content);
#endif
#endif
#ifdef	_WINDOWS
	mswin_cleardndcallback ();
	mswin_mousetrackcallback(NULL);
#endif
	if(c == NODATA || time_to_check()){	/* new mail ? */
	    if((*Pmaster->newmail)(c == NODATA ? 0 : 2, 1) >= 0){
		int rv;

		if(km_popped){
		    term.t_mrow = 2;
		    curwp->w_ntrows -= 2;
		    curwp->w_flag |= WFHARD;
		    km_popped = 0;
		}

		clearcursor();
		mlerase();
		rv = (*Pmaster->showmsg)(c);
		ttresize();
		picosigs();	/* restore altered handlers */
		if(rv)		/* Did showmsg corrupt the display? */
		  PaintBody(0);	/* Yes, repaint */

		mpresf = 1;
		input = 0;
	    }

	    clearcursor();
	    movecursor(0, 0);
	}

	if(km_popped)
	  switch(c){
	    case NODATA:
	    case (CTRL|'L'):
	      km_popped++;
	      break;
	    
	    default:
	      mlerase();
	      break;
	  }

	if(c == NODATA)		/* no op, getkey timed out */
	  continue;
	else if(!input++)
	  (*Pmaster->keybinput)();

	if (mpresf != FALSE) {		/* message stay around only  */
	    if (mpresf++ > NMMESSDELAY)	/* so long! */
	      mlerase();
	}

	f = FALSE;			/* vestigial */
	n = 1;
					/* Do it.               */
	execute(normalize_cmd(c, pfkm, 2), f, n);
	if(++checkpointcnt >= CHKPTDELAY){
	    checkpointcnt = 0;
	    writeout(chkptfile, TRUE);
	}
    }
}

/*
 * Initialize all of the buffers and windows. The buffer name is passed down
 * as an argument, because the main routine may have been told to read in a
 * file by default, and we want the buffer name to be right.
 */

/*
 * For the pine composer, we don't want to take over the whole screen
 * for editing.  the first some odd lines are to be used for message 
 * header information editing.
 */
void
edinit(bname)
char    bname[];
{
    register BUFFER *bp;
    register WINDOW *wp;

    if(Pmaster)
      func_init();

    bp = bfind(bname, TRUE, BFWRAPOPEN);    /* First buffer         */
    wp = (WINDOW *) malloc(sizeof(WINDOW)); /* First window         */

    if (bp==NULL || wp==NULL){
	if(Pmaster)
	  return;
	else
	  exit(1);
    }

    curbp  = bp;                            /* Make this current    */
    wheadp = wp;
    curwp  = wp;
    wp->w_wndp  = NULL;                     /* Initialize window    */
    wp->w_bufp  = bp;
    bp->b_nwnd  = 1;                        /* Displayed.           */
    wp->w_linep = bp->b_linep;
    wp->w_dotp  = bp->b_linep;
    wp->w_doto  = 0;
    wp->w_markp = wp->w_imarkp = NULL;
    wp->w_marko = wp->w_imarko = 0;
    bp->b_linecnt = -1;

    if(Pmaster){
	term.t_mrow = Pmaster->menu_rows;
	wp->w_toprow = ComposerTopLine = COMPOSER_TOP_LINE;
	wp->w_ntrows = term.t_nrow - COMPOSER_TOP_LINE - term.t_mrow;
	fillcol = Pmaster->fillcolumn;
	strcpy(opertree,
	       (Pmaster->oper_dir && strlen(Pmaster->oper_dir) < NLINE)
	         ? Pmaster->oper_dir : "");
    }
    else{
	if(sup_keyhelp)
	  term.t_mrow = 0;
	else
	  term.t_mrow = 2;

        wp->w_toprow = 2;
        wp->w_ntrows = term.t_nrow - 2 - term.t_mrow;
	if(userfillcol > 0)			/* set fill column */
	  fillcol = userfillcol;
	else
	  fillcol = term.t_ncol - 6;
    }

    /*
     * MDSCUR mode implies MDTREE mode with a opertree of home directory,
     * unless opertree has been set differently.
     */
    if((gmode & MDSCUR) && !opertree[0])
      strncpy(opertree, gethomedir(NULL), NLINE);

    if(*opertree)
      fixpath(opertree, NLINE);

    wp->w_force = 0;
    wp->w_flag  = WFMODE|WFHARD;            /* Full.                */
}


/*
 * This is the general command execution routine. It handles the fake binding
 * of all the keys to "self-insert". It also clears out the "thisflag" word,
 * and arranges to move it to the "lastflag", so that the next command can
 * look at it. Return the status of command.
 */
execute(c, f, n)
int c, f, n;
{
    register KEYTAB *ktp;
    register int    status;

    ktp = (Pmaster) ? &keytab[0] : &pkeytab[0];

    while (ktp->k_fp != NULL) {
	if (ktp->k_code == c) {

	    if(lastflag&CFFILL){
		curwp->w_flag |= WFMODE;
		if(Pmaster == NULL)
		  sgarbk = TRUE;
	    }

	    thisflag = 0;
	    status   = (*ktp->k_fp)(f, n);
	    if((lastflag & CFFILL) && !(thisflag & CFFILL))
	      fdelete();
	    if((lastflag & CFFLBF) && !(thisflag & CFFLBF))
	      kdelete();

	    lastflag = thisflag;

	    /*
	     * Reset flag saying wrap should open a new line whenever
	     * we execute a command (as opposed to just typing in text).
	     * However, if that command leaves us in the same line on the
	     * screen, then don't reset.
	     */
	    if(curwp->w_flag & (WFMOVE | WFHARD))
	      curbp->b_flag |= BFWRAPOPEN;    /* wrap should open new line */

	    return (status);
	}
	++ktp;
    }

    if(lastflag & CFFILL)		/* blat unusable fill data */
      fdelete();
    if(lastflag & CFFLBF)
      kdelete();

    if (VALID_KEY(c)) {			/* Self inserting.      */

	if (n <= 0) {                   /* Fenceposts.          */
	    lastflag = 0;
	    return (n<0 ? FALSE : TRUE);
	}
	thisflag = 0;                   /* For the future.      */

	/* do the appropriate insertion */
	/* pico never does C mode, this is simple */
	status = linsert(n, c);

	/*
	 * Check to make sure we didn't go off of the screen
	 * with that character.  Take into account tab expansion.
	 * If so wrap the line...
	 */
	if(curwp->w_bufp->b_mode & MDWRAP){
	    register int j;
	    register int k;

	    for(j = k = 0; j < llength(curwp->w_dotp); j++, k++)
	      if(isspace((unsigned char)lgetc(curwp->w_dotp, j).c)){
		  if(lgetc(curwp->w_dotp, j).c == TAB)
		    while(k+1 & 0x07)
		      k++;
	      }
	      else if(k >= fillcol){
		  wrapword();
		  break;
	      }
	}

	lastflag = thisflag;
	return (status);
    }
    
    if(c&CTRL)
      emlwrite("\007Unknown Command: ^%c", (void *)(c&0xff));
    else
      emlwrite("\007Unknown Command", NULL);

    lastflag = 0;                           /* Fake last flags.     */
    return (FALSE);
}



/*
 * Fancy quit command, as implemented by Norm. If the any buffer has
 * changed do a write on that buffer and exit emacs, otherwise simply exit.
 */
int
quickexit(f, n)
int f, n;
{
    register BUFFER *bp;	/* scanning pointer to buffers */

    bp = bheadp;
    while (bp != NULL) {
	if ((bp->b_flag&BFCHG) != 0	/* Changed.             */
	    && (bp->b_flag&BFTEMP) == 0) {	/* Real.                */
	    curbp = bp;		/* make that buffer cur	*/
	    filesave(f, n);
	}
	bp = bp->b_bufp;			/* on to the next buffer */
    }
    return(wquit(f, n));                             /* conditionally quit   */
}



/* 
 * abort_composer - ask the question here, then go quit or 
 *                  return FALSE
 */
abort_composer(f, n)
int f, n;
{
    char *result;

    result = "";

    Pmaster->arm_winch_cleanup++;
    if(Pmaster->canceltest){
        if(((Pmaster->pine_flags & MDHDRONLY) && !any_header_changes())
	  || (result = (*Pmaster->canceltest)(redraw_pico_for_callback))){
	    pico_all_done = COMP_CANCEL;
	    emlwrite(result, NULL);
	    Pmaster->arm_winch_cleanup--;
	    return(TRUE);
	}
	else{
	    emlwrite("Cancel Cancelled", NULL);
	    curwp->w_flag |= WFMODE;		/* and modeline so we  */
	    sgarbk = TRUE;			/* redraw the keymenu  */
	    pclear(term.t_nrow - 1, term.t_nrow + 1);
	    Pmaster->arm_winch_cleanup--;
	    return(FALSE);
	}
    }
    else switch(mlyesno(Pmaster->headents
	 ? "Cancel message (answering \"Yes\" will abandon your mail message)"
	 : (anycb() == FALSE)
	     ? "Cancel Edit (and abandon changes)"
	     : "Cancel Edit",
	 FALSE)){
      case TRUE:
	pico_all_done = COMP_CANCEL;
	return(TRUE);

      case ABORT:
	emlwrite("\007Cancel Cancelled", NULL);
	break;

      default:
	mlerase();
    }
    return(FALSE);
}


/*
 * suspend_composer - return to pine with what's been edited so far
 */
suspend_composer(f, n)
int f, n;
{
    if(Pmaster && Pmaster->headents)
      pico_all_done = COMP_SUSPEND;
    else
      (*term.t_beep)();
    
    return(TRUE);
}



/*
 * Quit command. If an argument, always quit. Otherwise confirm if a buffer
 * has been changed and not written out. Normally bound to "C-X C-C".
 */
wquit(f, n)
int f, n;
{
    register int    s;

    if(Pmaster){
	char *result;

	/* First, make sure there are no outstanding problems */ 
	if(AttachError()){
	    emlwrite("\007Problem with attachments!  Fix errors or delete attachments.", NULL);
	    return(FALSE);
	}

#ifdef	SPELLER
	if(Pmaster->always_spell_check)
	  if(spell(0, 0) == -1)
	    sleep(3);    /* problem, show error */
#endif
	/*
	 * if we're not in header, show some of it as we verify sending...
	 */
	display_for_send();
	packheader();
	Pmaster->arm_winch_cleanup++;
	if((!(Pmaster->pine_flags & MDHDRONLY) || any_header_changes())
	   && (result = (*Pmaster->exittest)(Pmaster->headents,
					     redraw_pico_for_callback,
					     Pmaster->allow_flowed_text))){
	    Pmaster->arm_winch_cleanup--;
	    if(sgarbf)
	      update();

	    lchange(WFHARD);			/* set update flags... */
	    curwp->w_flag |= WFMODE;		/* and modeline so we  */
	    sgarbk = TRUE;			/* redraw the keymenu  */
	    pclear(term.t_nrow - 2, term.t_nrow + 1);
	    if(*result)
	      emlwrite(result, NULL);
	}
	else{
	    Pmaster->arm_winch_cleanup--;
	    pico_all_done = COMP_EXIT;
	    return(TRUE);
	}
    }
    else{
        if (f != FALSE                          /* Argument forces it.  */
        || anycb() == FALSE                     /* All buffers clean.   */
						/* User says it's OK.   */
        || (s=mlyesno("Save modified buffer (ANSWERING \"No\" WILL DESTROY CHANGES)", -1)) == FALSE) {
                vttidy();
#if     defined(USE_TERMCAP) || defined(USE_TERMINFO) || defined(VMS)
		kbdestroy(kbesc);
#endif
                exit(0);
        }

	if(s == TRUE){
	    if(filewrite(0,1) == TRUE)
	      wquit(1, 0);
	}
	else if(s == ABORT){
	    emlwrite("Exit cancelled", NULL);
	    if(term.t_mrow == 0)
	      curwp->w_flag |= WFHARD;	/* cause bottom 3 lines to paint */
	}
        return(s);
    }

    return(FALSE);
}


/*
 * Has any editing been done to headers?
 */
any_header_changes()
{
    struct headerentry *he;

    for(he = Pmaster->headents; he->name != NULL; he++)
      if(he->dirty)
	break;
    
    return(he->name && he->dirty);
}

int
isquotedspace(line)
    LINE *line;
{
    int i, was_quote = 0;
    for(i = 0; i < llength(line); i++){
	if(lgetc(line, i).c == '>')
	  was_quote = 1;
	else if(was_quote && lgetc(line, i).c == ' '){
	    if(i+1 < llength(line) && isspace(lgetc(line,i+1).c))
	      return 1;
	    else
	      return 0;
	}
	else
	  return 0;
    }
    return 0;
}

/*
 * This function serves two purposes, 1) to strip white space when
 * Pmaster asks that the composition have its trailing white space
 * stripped, or 2) to prepare the text as flowed text, as Pmaster
 * is telling us that we're working with flowed text.
 *
 * What flowed currently means to us is stripping all trailing white
 * space, except for one space if the following line is a continuation
 * of the paragraph.  Also, we space-stuff all lines beginning
 * with white-space, and leave siglines alone.
 */
int
cleanwhitespace()
{
    LINE *cur_line = NULL, *cursor_dotp = NULL, **lp = NULL;
    int i = 0, cursor_doto = 0, is_cursor_line = 0;

    cursor_dotp = curwp->w_dotp;
    cursor_doto = curwp->w_doto;
    gotobob(FALSE, 1);

    for(lp = &curwp->w_dotp; (*lp) != curbp->b_linep; (*lp) = lforw(*lp)){
	if(!(llength(*lp) == 3
	     && lgetc(*lp, 0).c == '-'
	     && lgetc(*lp, 1).c == '-'
	     && lgetc(*lp, 2).c == ' ')
	   && llength(*lp)){
	    is_cursor_line = (cursor_dotp == (*lp));
	    /* trim trailing whitespace, to be added back if flowing */
	    for(i = llength(*lp); i; i--)
	      if(!isspace(lgetc(*lp, i - 1).c))
		break;
	    if(i != llength(*lp)){
		int flow_line = 0;

		if(Pmaster && !Pmaster->strip_ws_before_send
		   && lforw(*lp) != curbp->b_linep
		   && llength(lforw(*lp))
		   && !(isspace(lgetc(lforw(*lp), 0).c)
			|| isquotedspace(lforw(*lp)))
		   && !(llength(lforw(*lp)) == 3
			&& lgetc(lforw(*lp), 0).c == '-'
			&& lgetc(lforw(*lp), 1).c == '-'
			&& lgetc(lforw(*lp), 2).c == ' '))
		  flow_line = 1;
		if(flow_line && i && lgetc(*lp, i).c == ' '){
		    /* flowed line ending with space */
		    i++;
		    if(i != llength(*lp)){
			curwp->w_doto = i;
			ldelete(llength(*lp) - i, NULL);
		    }
		}
		else if(flow_line && i && isspace(lgetc(*lp, i).c)){
		    /* flowed line ending with whitespace other than space*/
		    curwp->w_doto = i;
		    ldelete(llength(*lp) - i, NULL);
		    linsert(1, ' ');
		}
		else{
		    curwp->w_doto = i;
		    ldelete(llength(*lp) - i, NULL);
		}
	    }
	    if(Pmaster && Pmaster->allow_flowed_text
	       && llength(*lp) && isspace(lgetc(*lp, 0).c)){
		/* space-stuff only if flowed */
		curwp->w_doto = 0;
		if(is_cursor_line && cursor_doto)
		  cursor_doto++;
		linsert(1, ' ');
	    }
	    if(is_cursor_line)
	      cursor_dotp = (*lp);
	}
    }

    /* put the cursor back where we found it */
    gotobob(FALSE, 1);
    curwp->w_dotp = cursor_dotp;
    curwp->w_doto = (cursor_doto < llength(curwp->w_dotp))
      ? cursor_doto : llength(curwp->w_dotp) - 1;

    return(0);
}

/*
 * Remove all trailing white space from the text
 */
int
stripwhitespace()
{
    int i;
    LINE *cur_line = lforw(curbp->b_linep);

    do{
      /* we gotta test for the sigdash case here */
      if(!(cur_line->l_used == 3 && 
	   lgetc(cur_line, 0).c == '-' &&
	   lgetc(cur_line, 1).c == '-' &&
	   lgetc(cur_line, 2).c == ' '))
	for(i = cur_line->l_used - 1; i >= 0; i--)
	  if(isspace(lgetc(cur_line, i).c))
	    cur_line->l_used--;
	  else
	    break;
    }while((cur_line = lforw(cur_line)) != curbp->b_linep);
    return 0;
}

/*
 * Abort.
 * Beep the beeper. Kill off any keyboard macro, etc., that is in progress.
 * Sometimes called as a routine, to do general aborting of stuff.
 */
ctrlg(f, n)
int f, n;
{
    emlwrite("Cancelled", NULL);
    return (ABORT);
}


/* tell the user that this command is illegal while we are in
 *  VIEW (read-only) mode
 */
rdonly()
{
    (*term.t_beep)();
    emlwrite("Key illegal in VIEW mode", NULL);
    return(FALSE);
}



/*
 * reset all globals to their initial values
 */
void
func_init()
{
    extern int    vtrow;
    extern int    vtcol;
    extern int    lbound;

    /*
     * re-initialize global buffer type variables ....
     */
    fillcol = (term.t_ncol > 80) ? 77 : term.t_ncol - 6;
    eolexist = TRUE;
    revexist = FALSE;
    sgarbf = TRUE;
    mpresf = FALSE;
    mline_open = FALSE;
    ComposerEditing = FALSE;

    /*
     * re-initialize hardware display variables ....
     */
    vtrow = vtcol = lbound = 0;
    clearcursor();

    pat[0] = rpat[0] = '\0';
    browse_dir[0] = '\0';
}


/*
 * pico_help - help function for standalone composer
 */
pico_help(text, title, i)
char *text[], *title;
int i;
{
    register    int numline = 0;
    char        **p;
 
    p = text;
    while(*p++ != NULL) 
      numline++;
    return(wscrollw(COMPOSER_TOP_LINE, term.t_nrow-1, text, numline));
}



/*
 * zotedit() - kills the buffer and frees all lines associated with it!!!
 */
void
zotedit()
{
    wheadp->w_linep = wheadp->w_dotp = wheadp->w_markp = wheadp->w_imarkp = NULL;
    bheadp->b_linep = bheadp->b_dotp = bheadp->b_markp = NULL;

    free((char *) wheadp);			/* clean up window */
    wheadp = NULL;
    curwp  = NULL;

    free((char *) bheadp);			/* clean up buffers */
    bheadp = NULL;
    curbp  = NULL;

    zotheader();				/* blast header lines */

    kdelete();					/* blast kill buffer */

}


#ifdef	MOUSE
/*
 * Generic mouse handling functions
 */
MENUITEM  menuitems[12];		/* key labels and functions */
MENUITEM *mfunc = NULL;			/* list of regional functions  */
mousehandler_t	mtrack;			/* mouse tracking handler */

/* last mouse position */
static  int levent = 0, lrow = 0, lcol = 0, doubleclick, lbutton, lflags;
#ifdef	DOS
static  clock_t lastcalled = 0;
#else
static	time_t	lastcalled = 0;
#endif
static	mousehandler_t lastf;


/*
 * register_mfunc - register the given function to get called
 * 		    on mouse events in the given display region
 */
register_mfunc(f, tlr, tlc, brr, brc)
mousehandler_t	f;
int		tlr, tlc, brr, brc;
{
    MENUITEM **mp;

    if(!mouseexist())
      return(FALSE);

    for(mp = &mfunc; *mp; mp = &(*mp)->next)
      ;

    *mp = (MENUITEM *)malloc(sizeof(MENUITEM));
    memset(*mp, 0, sizeof(MENUITEM));

    (*mp)->action = f;
    (*mp)->tl.r   = tlr;
    (*mp)->br.r   = brr;
    (*mp)->tl.c   = tlc;
    (*mp)->br.c   = brc;
    (*mp)->lbl.c  = (*mp)->lbl.r = 0;
    (*mp)->label  = "";
    return(TRUE);
}


/*
 * clear_mfunc - clear any previously set mouse function
 */
void
clear_mfunc(f)
mousehandler_t f;
{
    MENUITEM *mp, *tp;

    if(mp = mfunc){
	if(mp->action == f)
	  mfunc = mp->next;
	else
	  for(tp = mp; tp->next; tp = tp->next)
	    if(tp->next->action == f){
		mp = tp->next;
		tp->next = tp->next->next;
		break;
	    }

	if(mp){
	    mp->action = NULL;
	    free(mp);
	}
    }
}



#ifdef EX_MOUSE

void
clear_mtrack ()
{
    mtrack = NULL;
    mswin_allowmousetrack (FALSE);
}


void
register_mtrack (f)
mousehandler_t	f;
{
    if (f) {
	mtrack = f;
	mswin_allowmousetrack (TRUE);
    }
    else
	clear_mtrack ();
}


static void
move_dot_to (row, col)
    int row, col;
{
    LINE *lp;
    int	i;
    
    lp = curwp->w_linep;
    i = row - ((Pmaster) ? ComposerTopLine : 2);
    while(i-- && lp != curbp->b_linep)	/* count from top */
      lp = lforw(lp);
    curgoal = col;
    curwp->w_dotp = lp;			/* to new dot. */
    curwp->w_doto = getgoal(lp);
    curwp->w_flag |= WFMOVE;
}


/*
 * mouse_in_pico
 *
 * When the mouse goes down in the body we set the mark and start 
 * tracking.
 *
 * As the mouse moves we update the dot and redraw the screen.
 *
 * If the mouse moves above or below the pico body region of the 
 * screen we scroll the text and update the dot position.
 *
 * When the mouse comes up we clean up.  If the mouse did not
 * move, then we clear the mark and turn off the selection.
 *
 * Most of the mouse processing is handled here.  The exception is
 * mouse down in the header.  Can't call HeaderEditor() from here so
 * we send up the KEY_MOUSE character, which gets dispatched to
 * mousepress(), which _can_ call HeaderEditor().
 */
unsigned long
mouse_in_pico(mevent, row, col, button, flags)
    int	     mevent;
    int      row, col, button, flags;
{
    unsigned long	rv = 0;		/* Our return value. */
    int			trow, tcol;	/* translated row and col. */

    static int lheader = FALSE;		/* Mouse down was in header. */

  
    /*
     * What's up.
     */
    switch (mevent) {
    case M_EVENT_DOWN:
      if(button != M_BUTTON_LEFT)
	break;

	/* Ignore mouse down if not in pico body region. */
	if (row < 2 || row > term.t_nrow - (term.t_mrow+1)) {
	    clear_mtrack ();
	    break;
        }
	
	/* Detect double clicks.  Not that we do anything with em, just
	 * detect them. */
#ifdef	DOS
#ifdef	CLOCKS_PER_SEC
	doubleclick = (lrow == row && lcol == col
		       && clock() < (lastcalled + CLOCKS_PER_SEC/2));
#else
#ifdef	CLK_TCK
doubleclick = (lrow == row && lcol == col
		       && clock() < (lastcalled + CLK_TCK/2));
#else
	doubleclick = FALSE;
#endif
#endif
	lastcalled  = clock();
#else
	doubleclick = (lrow == row && lcol == col
		       && time(0) < (lastcalled + 2));
	lastcalled  = time(0);
#endif
	lheader	    = FALSE;	/* Rember mouse down position. */
	levent	    = mevent;
	lrow	    = row;
	lcol	    = col;
	lbutton     = button;
	lflags      = flags;
	
	/* Mouse down in body? */
	if (row < (Pmaster ? ComposerTopLine : 2)) {
	    /* Mouse down in message header -> no tracking, just remember
	     * where */
	    lheader = TRUE;
        }
	else {
	    /* Mouse down in message.
	     * If no shift key and an existing mark -> clear the mark.
	     * If shift key and no existing mark -> set mark before moving */
	    if (!(flags & M_KEY_SHIFT) && curwp->w_markp) 
		setmark (0,1);		/* this clears the mark. */
	    else if (flags & M_KEY_SHIFT && !curwp->w_markp)
		setmark (0,1);		/* while this sets the mark. */
	    
	    /* Reposition dot to mouse down. */
	    move_dot_to (row, col);
	    
	    /* Set the mark to dot if no existing mark. */
	    if (curwp->w_markp == NULL) 
		setmark (0,1);

	    /* Track mouse movement. */
	    register_mtrack (mouse_in_pico);
	    update ();
	    lheader = FALSE;		/* Just to be sure. */
        }
	break;
	
	
    case M_EVENT_TRACK:
	/* Mouse tracking. */
	if (lheader)			/* Ignore mouse movement in header. */
	    break;
    
	/* If above or below body, scroll body and adjust the row and col. */
	if (row < (Pmaster ? ComposerTopLine : 2)) {
	    /* Scroll text down screen and move dot to top left corner. */
	    scrollupline (0,1);
	    trow = (Pmaster) ? ComposerTopLine : 2;
	    tcol = 0;
        }
	else if (row > term.t_nrow - (term.t_mrow + 1)) {
	    /* Scroll text up screen and move dot to bottom right corner. */
	    scrolldownline (0,1);
	    trow = term.t_nrow - (term.t_mrow + 1);
	    tcol = term.t_ncol;
        }
	else {
	    trow = row;
	    tcol = col;
        }

	/* Move dot to target column. */
	move_dot_to (trow, tcol);
	
	/* Update screen. */
	update ();
	break;

	
    case M_EVENT_UP:
      if(button == M_BUTTON_RIGHT){
#ifdef	_WINDOWS
	  pico_popup();
#endif
	  break;
      }
      else if(button != M_BUTTON_LEFT)
	break;

	if (lheader) {
	    lheader = FALSE;
	    /* Last down in header. */
	    if (row == lrow && col == lcol) {
		/* Mouse up and down in same place in header.  Means the
		 * user want to edit the header.  Return KEY_MOUSE which
		 * will cause mousepress to be called, which will
		 * call HeaderEditor.  Can't call HeaderEditor from here
		 * because that would mess up layering. */
		if (curwp->w_marko)
		    setmark (0,1);
		rv = ((unsigned long)KEY_MOUSE << 16) | TRUE;
	    }
        }
	else {
	    /* If up at same place, clear mark */
	    if (curwp->w_markp == curwp->w_dotp && 
		    curwp->w_marko == curwp->w_doto) {
		setmark (0,1);
		curwp->w_flag |= WFMOVE;
	    }
	    clear_mtrack ();
	    update ();
        }
	break;    
    }
    
    return(rv);
}
#endif



/*
 * mouse_in_content - general mechanism used to pass recognized mouse
 *		      events in predefined region back thru the usual
 *		      keyboard input stream.  The actual return value
 *		      passed back from this function is set dynamically
 *		      via the "down" argument which is read when both the
 *		      "row" and "col" arguments are negative.
 */
unsigned long
mouse_in_content(mevent, row, col, button, flags)
    int      mevent;
    int      row, col, button, flags;
{
    unsigned long   rv = 0;
    static unsigned mouse_val = KEY_MOUSE;

    if(row == -1 && col == -1){
	mouse_val = mevent;			/* setting return value */
    }
    else {
	/* A real event. */
	levent = mevent;
	switch (mevent) {
	case M_EVENT_DOWN:
	    /* Mouse down does not mean anything, just keep track of 
	     * where it went down and if this is a double click. */
#ifdef	DOS
#ifdef	CLOCKS_PER_SEC
	    doubleclick = (lrow == row && lcol == col
			   && clock() < (lastcalled + CLOCKS_PER_SEC/2));
#else
#ifdef	CLK_TCK
	    doubleclick = (lrow == row && lcol == col
			   && clock() < (lastcalled + CLK_TCK/2));
#else
	    doubleclick = FALSE;
#endif
#endif
	    lastcalled	= clock();
#else
	    doubleclick = (lrow == row && lcol == col
			   && time(0) < (lastcalled + 2));
	    lastcalled  = time(0);
#endif
	    lrow	= row;
	    lcol	= col;
	    lbutton	= button;
	    lflags	= flags;
	    break;

	case M_EVENT_UP:
	    /* Mouse up.  If in the same position as it went down
	     * then we return the value set above, which goes into
	     * the character input stream, which gets processed as
	     * a mouse event by some upper layer, which calls to 
	     * mouse_get_last(). */
	    if (lrow == row && lcol == col) {
		rv = mouse_val;
		rv = (rv << 16) | TRUE;
	    }
	    break;
	    
	case M_EVENT_TRACK:
	    break;
	}
    }

    return(rv);
}


/*
 * mouse_get_last - Get last mouse event.
 *
 */
void
mouse_get_last(f, mp)
    mousehandler_t *f;
    MOUSEPRESS *mp;
{
    if (f != NULL)
        *f  = lastf;
    if (mp != NULL) {
	mp->mevent	= levent;
	mp->row		= lrow;
	mp->col		= lcol;
	mp->doubleclick = doubleclick;
	mp->button	= lbutton;
	mp->flags	= lflags;
    }
}



/*
 * register_key - register the given keystroke to accept mouse events
 */
void
register_key(i, rval, label, label_printer, row, col, len, kn, kl)
int       i;
unsigned  rval;
char     *label;
void      (*label_printer)();
int       row, col, len;
COLOR_PAIR *kn, *kl;
{
    if(i > 11)
      return;

    menuitems[i].val   = rval;
    menuitems[i].tl.r  = menuitems[i].br.r = row;
    menuitems[i].tl.c  = col;
    menuitems[i].br.c  = col + len;
    menuitems[i].lbl.r = menuitems[i].tl.r;
    menuitems[i].lbl.c = menuitems[i].tl.c;
    menuitems[i].label_hiliter = label_printer;
    if(menuitems[i].label){
	free(menuitems[i].label);
	menuitems[i].label = NULL;
    }
    if(menuitems[i].kncp)
        free_color_pair(&menuitems[i].kncp);
    if(menuitems[i].klcp)
        free_color_pair(&menuitems[i].klcp);
    if(kn)
      menuitems[i].kncp = new_color_pair(kn->fg, kn->bg);
    else 
      menuitems[i].kncp = NULL;
    if(kl)
      menuitems[i].klcp = new_color_pair(kl->fg, kl->bg);
    else 
      menuitems[i].klcp = NULL;    

    if(label
       && (menuitems[i].label =(char *)malloc((strlen(label)+1)*sizeof(char))))
      strcpy(menuitems[i].label, label);
}


int
mouse_on_key(row, col)
    int row, col;
{
    int i;

    for(i = 0; i < 12; i++)
      if(M_ACTIVE(row, col, &menuitems[i]))
	return(TRUE);
	
    return(FALSE);
}
#endif	/* MOUSE */


/* 
 * Below are functions for use outside pico to manipulate text
 * in a pico's native format (circular linked list of lines).
 *
 * The idea is to streamline pico use by making it fairly easy
 * for outside programs to prepare text intended for pico's use.
 * The simple char * alternative is messy as it requires two copies
 * of the same text, and isn't very economic in limited memory
 * situations (THANKS BELLEVUE-BILLY.).
 */
typedef struct picotext {
    LINE *linep;
    LINE *dotp;
    int doto;
    short crinread;
} PICOTEXT;

#define PT(X)	((PICOTEXT *)(X))

/*
 * pico_get - return window struct pointer used as a handle
 *            to the other pico_xxx routines.
 */
void *
pico_get()
{
   PICOTEXT *wp = NULL;
   LINE     *lp = NULL;

   if(wp = (PICOTEXT *)malloc(sizeof(PICOTEXT))){
       wp->crinread = 0;
       if((lp = lalloc(0)) == NULL){
	   free(wp);
	   return(NULL);
       }

       wp->dotp = wp->linep = lp->l_fp = lp->l_bp = lp;
       wp->doto = 0;
   }
   else
     emlwrite("Can't allocate space for text", NULL);

   return((void *)wp);
}

/*
 * pico_give - free resources and give up picotext struct
 */
void
pico_give(w)
void *w;
{
    register LINE *lp;
    register LINE *fp;

    fp = lforw(PT(w)->linep);
    while((lp = fp) != PT(w)->linep){
        fp = lforw(lp);
	free(lp);
    }
    free(PT(w)->linep);
    free((PICOTEXT *)w);
}

/*
 * pico_readc - return char at current point.  Up to calling routines
 *              to keep cumulative count of chars.
 */
int
pico_readc(w, c)
void          *w;
unsigned char *c;
{
    int rv     = 0;

    if(PT(w)->crinread){
	*c = '\012';				/* return LF */
	PT(w)->crinread = 0;
	rv++;
    }
    else if(PT(w)->doto < llength(PT(w)->dotp)){ /* normal char to return */
        *c = (unsigned char) lgetc(PT(w)->dotp, (PT(w)->doto)++).c;
	rv++;
    }
    else if(PT(w)->dotp != PT(w)->linep){ /* return line break */
	PT(w)->dotp = lforw(PT(w)->dotp);
	PT(w)->doto = 0;
#if	defined(DOS) || defined(OS2)
	*c = '\015';
	PT(w)->crinread++;
#else
	*c = '\012';				/* return local eol! */
#endif
	rv++;
    }						/* else no chars to return */

    return(rv);
}


/*
 * pico_writec - write a char into picotext and advance pointers.
 *               Up to calling routines to keep track of total chars
 *               written
 */
int
pico_writec(w, c)
void *w;
int   c;
{
    int   rv = 0;

    if(c == '\r')				/* ignore CR's */
      rv++;					/* so fake it */
    else if(c == '\n'){				/* insert newlines on LF */
	/*
	 * OK, if there are characters on the current line or 
	 * dotp is pointing to the delimiter line, insert a newline
	 * No here's the tricky bit; preserve the implicit EOF newline.
	 */
	if(lforw(PT(w)->dotp) == PT(w)->linep && PT(w)->dotp != PT(w)->linep){
	    PT(w)->dotp = PT(w)->linep;
	    PT(w)->doto = 0;
	}
	else{
	    register LINE *lp;

	    if((lp = lalloc(0)) == NULL){
		emlwrite("Can't allocate space for more characters",NULL);
		return(0);
	    }

	    if(PT(w)->dotp == PT(w)->linep){
		lforw(lp) = PT(w)->linep;
		lback(lp) = lback(PT(w)->linep);
		lforw(lback(lp)) = lback(PT(w)->linep) = lp;
	    }
	    else{
		lforw(lp) = lforw(PT(w)->dotp);
		lback(lp) = PT(w)->dotp;
		lback(lforw(lp)) = lforw(PT(w)->dotp) = lp;
		PT(w)->dotp = lp;
		PT(w)->doto = 0;
	    }
	}

	rv++;
    }
    else
      rv = geninsert(&PT(w)->dotp, &PT(w)->doto, PT(w)->linep, c, 0, 1, NULL);

    return((rv) ? 1 : 0);			/* return number written */
}


/*
 * pico_puts - just write the given string into the text
 */
int
pico_puts(w, s)
void *w;
char *s;
{
    int rv = 0;

    while(*s != '\0')
      rv += pico_writec(w, (int)*s++);

    return((rv) ? 1 : 0);
}


/*
 * pico_seek - position dotp and dot at requested location
 */
int
pico_seek(w, offset, orig)
void *w;
long  offset;
int   orig;
{
    register LINE *lp;

    PT(w)->crinread = 0;
    switch(orig){
      case 0 :				/* SEEK_SET */
	PT(w)->dotp = lforw(PT(w)->linep);
	PT(w)->doto = 0;
      case 1 :				/* SEEK_CUR */
	lp = PT(w)->dotp;
	while(lp != PT(w)->linep){
	    if(offset <= llength(lp)){
		PT(w)->doto = (int)offset;
		PT(w)->dotp = lp;
		break;
	    }

	    offset -= ((long)llength(lp)
#if defined(DOS) || defined(OS2)
		       + 2L);
#else
		       + 1L);
#endif
	    lp = lforw(lp);
	}
        break;

      case 2 :				/* SEEK_END */
	PT(w)->dotp = lback(PT(w)->linep);
	PT(w)->doto = llength(PT(w)->dotp);
	break;
      default :
        return(-1);
    }

    return(0);
}


/*
 * breplace - replace the current window's text with the given 
 *            LINEs
 */
void
breplace(w)
void *w;
{
    register LINE *lp;
    register LINE *fp;

    fp = lforw(curbp->b_linep);
    while((lp = fp) != curbp->b_linep){		/* blast old lines */
        fp = lforw(lp);
	free(lp);
    }
    free(curbp->b_linep);

    curbp->b_linep   = PT(w)->linep;			/* arrange pointers */

    curwp->w_linep   = lforw(curbp->b_linep);
    curwp->w_dotp    = lforw(curbp->b_linep);
    curwp->w_doto    = 0;
    curwp->w_markp   = curwp->w_imarkp = NULL;
    curwp->w_marko   = curwp->w_imarko = 0;

    curbp->b_dotp    = curwp->w_dotp;
    curbp->b_doto    = curbp->b_marko  = 0;
    curbp->b_markp   = NULL;
    curbp->b_linecnt = -1;

    curwp->w_flag |= WFHARD;
}


#ifdef	_WINDOWS
/*
 *
 */
int
composer_file_drop(x, y, filename)
    int   x, y;
    char *filename;
{
    int attached = 0;
    if((ComposerTopLine > 2 && x <= ComposerTopLine)
       || !LikelyASCII(filename)){
	AppendAttachment(filename, NULL, NULL);
	attached++;
    }
    else{
	setimark(FALSE, 1);
	ifile(filename);
	swapimark(FALSE, 1);
    }

    if(ComposerEditing){		/* update display */
	PaintBody(0);
    }
    else{
	pico_refresh(0, 1);
	update();
    }

    if(attached)
      emlwrite("Attached dropped file \"%s\"", filename);
    else
      emlwrite("Inserted dropped file \"%s\"", filename);

    if(ComposerEditing){		/* restore cursor */
	HeaderPaintCursor();
    }
    else{
	curwp->w_flag |= WFHARD;
	update();
    }

    return(1);
}



int
pico_cursor(col, row)
    int  col;
    long row;
{
    return((row > 1 && row <= term.t_nrow - (term.t_mrow + 1))
	     ? MSWIN_CURSOR_IBEAM
	     : MSWIN_CURSOR_ARROW);
}
#endif /* _WINDOWS */
