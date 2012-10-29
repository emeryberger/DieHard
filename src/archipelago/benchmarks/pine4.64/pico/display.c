#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: display.c 13654 2004-05-07 21:43:40Z jpf $";
#endif
/*
 * Program:	Display functions
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
/*
 * The functions in this file handle redisplay. There are two halves, the
 * ones that update the virtual display screen, and the ones that make the
 * physical display screen the same as the virtual display screen. These
 * functions use hints that are left in the windows by the commands.
 *
 */

#include	"headers.h"


#ifdef	ANSI
    void vtmove(int, int);
    void vtputc(CELL);
    void vtpute(CELL);
    void vteeol(void);
    void updateline(int, CELL *, CELL *, short *);
    void updext(void);
    void mlputi(int, int);
    void pprints(int, int);
    void mlputli(long, int);
    void showCompTitle(void);
    int nlforw(void);
    int dumbroot(int, int);
    int dumblroot(long, int);
#else
    void vtmove();
    void vtputc();
    void vtpute();
    void vteeol();
    void updateline();
    void updext();
    void mlputi();
    void pprints();
    void mlputli();
    void showCompTitle();
    int nlforw();
    int dumbroot();
    int dumblroot();
#endif


/*
 * Standard pico keymenus...
 */
static KEYMENU menu_pico[] = {
    {"^G", "Get Help", KS_SCREENHELP},	{"^O", "WriteOut", KS_SAVEFILE},
    {"^R", "Read File", KS_READFILE},	{"^Y", "Prev Pg", KS_PREVPAGE},
    {"^K", "Cut Text", KS_NONE},	{"^C", "Cur Pos", KS_CURPOSITION},
    {"^X", "Exit", KS_EXIT},		{"^J", "Justify", KS_JUSTIFY},
    {"^W", "Where is", KS_WHEREIS},	{"^V", "Next Pg", KS_NEXTPAGE},
    {"^U", NULL, KS_NONE},
#ifdef	SPELLER
    {"^T", "To Spell", KS_SPELLCHK}
#else
    {"^D", "Del Char", KS_NONE}
#endif
};
#define	UNCUT_KEY	10


static KEYMENU menu_compose[] = {
    {"^G", "Get Help", KS_SCREENHELP},	{"^X", NULL, KS_SEND},
    {"^R", "Read File", KS_READFILE},	{"^Y", "Prev Pg", KS_PREVPAGE},
    {"^K", "Cut Text", KS_NONE},	{"^O", "Postpone", KS_POSTPONE},
    {"^C", "Cancel", KS_CANCEL},	{"^J", "Justify", KS_JUSTIFY},
    {NULL, NULL, KS_NONE},		{"^V", "Next Pg", KS_NEXTPAGE},
    {"^U", NULL, KS_NONE},
#ifdef	SPELLER
    {"^T", "To Spell", KS_SPELLCHK}
#else
    {"^D", "Del Char", KS_NONE}
#endif
};
#define	EXIT_KEY	1
#define	PSTPN_KEY	5
#define	WHERE_KEY	8


/*
 * Definition's for pico's modeline
 */
#define	PICO_TITLE	"  UW PICO(tm) %s  "
#define	PICO_MOD_MSG	"Modified  "
#define	PICO_NEWBUF_MSG	" New Buffer "

#define WFDEBUG 0                       /* Window flag debug. */

#define VFCHG   0x0001                  /* Changed flag			*/
#define	VFEXT	0x0002			/* extended (beyond column 80)	*/
#define	VFREV	0x0004			/* reverse video status		*/
#define	VFREQ	0x0008			/* reverse video request	*/

int     vtrow   = 0;                    /* Row location of SW cursor */
int     vtcol   = 0;                    /* Column location of SW cursor */
int     ttrow   = FARAWAY;              /* Row location of HW cursor */
int     ttcol   = FARAWAY;              /* Column location of HW cursor */
int	lbound	= 0;			/* leftmost column of current line
					   being displayed */

VIDEO   **vscreen;                      /* Virtual screen. */
VIDEO   **pscreen;                      /* Physical screen. */

#define	ISCONTROL(C)	((C) < 0x20 || (C) == 0x7F \
			 || ((gmode & P_HICTRL) && ((C) > 0x7F && (C) < 0xA0)))


/*
 * Initialize the data structures used by the display code. The edge vectors
 * used to access the screens are set up. The operating system's terminal I/O
 * channel is set up. All the other things get initialized at compile time.
 * The original window has "WFCHG" set, so that it will get completely
 * redrawn on the first call to "update".
 */
vtinit()
{
    register int i;
    register VIDEO *vp;

    if(Pmaster == NULL)
      vtterminalinfo(gmode & MDTCAPWINS);

    (*term.t_open)();

    (*term.t_rev)(FALSE);
    vscreen = (VIDEO **) malloc((term.t_nrow+1)*sizeof(VIDEO *));
    if (vscreen == NULL){
	emlwrite("Allocating memory for virtual display failed.", NULL);
        return(FALSE);
    }


    pscreen = (VIDEO **) malloc((term.t_nrow+1)*sizeof(VIDEO *));
    if (pscreen == NULL){
	free((void *)vscreen);
	emlwrite("Allocating memory for physical display failed.", NULL);
        return(FALSE);
    }


    for (i = 0; i <= term.t_nrow; ++i) {
        vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

        if (vp == NULL){
	    free((void *)vscreen);
	    free((void *)pscreen);
	    emlwrite("Allocating memory for virtual display lines failed.",
		     NULL);
            return(FALSE);
	}
	else
	  memset(vp, ' ', sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

	vp->v_flag = 0;
        vscreen[i] = vp;
        vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

        if (vp == NULL){
            free((void *)vscreen[i]);
	    while(--i >= 0){
		free((void *)vscreen[i]);
		free((void *)pscreen[i]);
	    }

	    free((void *)vscreen);
	    free((void *)pscreen);
	    emlwrite("Allocating memory for physical display lines failed.",
		     NULL);
            return(FALSE);
	}
	else
	  memset(vp, ' ', sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

	vp->v_flag = 0;
        pscreen[i] = vp;
    }

    return(TRUE);
}

vtterminalinfo(termcap_wins)
    int termcap_wins;
{
    return((term.t_terminalinfo) ? (*term.t_terminalinfo)(termcap_wins)
				 : (Pmaster ? 0 : TRUE));
}


/*
 * Clean up the virtual terminal system, in anticipation for a return to the
 * operating system. Move down to the last line and clear it out (the next
 * system prompt will be written in the line). Shut down the channel to the
 * terminal.
 */
void
vttidy()
{
    movecursor(term.t_nrow-1, 0);
    peeol();
    movecursor(term.t_nrow, 0);
    peeol();
    (*term.t_close)();
}


/*
 * Set the virtual cursor to the specified row and column on the virtual
 * screen. There is no checking for nonsense values; this might be a good
 * idea during the early stages.
 */
void
vtmove(row, col)
int row, col;
{
    vtrow = row;
    vtcol = col;
}


/*
 * Write a character to the virtual screen. The virtual row and column are
 * updated. If the line is too long put a "$" in the last column. This routine
 * only puts printing characters into the virtual terminal buffers. Only
 * column overflow is checked.
 */
void
vtputc(c)
CELL c;
{
    register VIDEO      *vp;
    CELL     ac;

    vp = vscreen[vtrow];
    ac.c = ' ';
    ac.a = c.a;

    if (vtcol >= term.t_ncol) {
        vtcol = (vtcol + 0x07) & ~0x07;
	ac.c = '$';
        vp->v_text[term.t_ncol - 1] = ac;
    }
    else if (c.c == '\t') {
        do {
            vtputc(ac);
	}
        while ((vtcol&0x07) != 0);
    }
    else if (ISCONTROL(c.c)){
	ac.c = '^';
        vtputc(ac);
	ac.c = ((c.c & 0x7f) | 0x40);
        vtputc(ac);
    }
    else
      vp->v_text[vtcol++] = c;
}


/* put a character to the virtual screen in an extended line. If we are
 * not yet on left edge, don't print it yet. check for overflow on
 * the right margin.
 */
void
vtpute(c)
CELL c;
{
    register VIDEO      *vp;
    CELL                 ac;

    vp = vscreen[vtrow];
    ac.c = ' ';
    ac.a = c.a;

    if (vtcol >= term.t_ncol) {
        vtcol = (vtcol + 0x07) & ~0x07;
	ac.c = '$';
        vp->v_text[term.t_ncol - 1] = ac;
    }
    else if (c.c == '\t'){
        do {
            vtpute(ac);
        }
        while (((vtcol + lbound)&0x07) != 0 && vtcol < term.t_ncol);
    }
    else if (ISCONTROL(c.c)){
	ac.c = '^';
        vtpute(ac);
	ac.c = ((c.c & 0x7f) | 0x40);
        vtpute(ac);
    }
    else {
	if (vtcol >= 0)
	  vp->v_text[vtcol] = c;
	++vtcol;
    }
}


/*
 * Erase from the end of the software cursor to the end of the line on which
 * the software cursor is located.
 */
void
vteeol()
{
    register VIDEO      *vp;
    CELL     c;

    c.c = ' ';
    c.a = 0;
    vp = vscreen[vtrow];
    while (vtcol < term.t_ncol)
      vp->v_text[vtcol++] = c;
}


/*
 * Make sure that the display is right. This is a three part process. First,
 * scan through all of the windows looking for dirty ones. Check the framing,
 * and refresh the screen. Second, make sure that "currow" and "curcol" are
 * correct for the current window. Third, make the virtual and physical
 * screens the same.
 */
void
update()
{
    register LINE   *lp;
    register WINDOW *wp;
    register VIDEO  *vp1;
    register VIDEO  *vp2;
    register int     i;
    register int     j;
    register int     scroll = 0;
    CELL	     c;

#if	TYPEAH
    if (typahead())
	return;
#endif

#ifdef _WINDOWS
    /* This tells our MS Windows module to not bother updating the
     * cursor position while a massive screen update is in progress.
     */
    mswin_beginupdate ();
#endif

/*
 * BUG: setting and unsetting whole region at a time is dumb.  fix this.
 */
    if(curwp->w_markp){
	unmarkbuffer();
	markregion(1);
    }

    wp = wheadp;

    while (wp != NULL){
        /* Look at any window with update flags set on. */

        if (wp->w_flag != 0){
            /* If not force reframe, check the framing. */

            if ((wp->w_flag & WFFORCE) == 0){
                lp = wp->w_linep;

                for (i = 0; i < wp->w_ntrows; ++i){
                    if (lp == wp->w_dotp)
		      goto out;

                    if (lp == wp->w_bufp->b_linep)
		      break;

                    lp = lforw(lp);
		}
	    }

            /* Not acceptable, better compute a new value for the line at the
             * top of the window. Then set the "WFHARD" flag to force full
             * redraw.
             */
            i = wp->w_force;

            if (i > 0){
                --i;

                if (i >= wp->w_ntrows)
                  i = wp->w_ntrows-1;
	    }
            else if (i < 0){
                i += wp->w_ntrows;

                if (i < 0)
		  i = 0;
	    }
            else if(optimize){
		/* 
		 * find dotp, if its been moved just above or below the 
		 * window, use scrollxxx() to facilitate quick redisplay...
		 */
		lp = lforw(wp->w_dotp);
		if(lp != wp->w_dotp){
		    if(lp == wp->w_linep && lp != wp->w_bufp->b_linep){
			scroll = 1;
		    }
		    else {
			lp = wp->w_linep;
			for(j=0;j < wp->w_ntrows; ++j){
			    if(lp != wp->w_bufp->b_linep)
			      lp = lforw(lp);
			    else
			      break;
			}
			if(lp == wp->w_dotp && j == wp->w_ntrows)
			  scroll = 2;
		    }
		}
		j = i = wp->w_ntrows/2;
	    }
	    else
	      i = wp->w_ntrows/2;

            lp = wp->w_dotp;

            while (i != 0 && lback(lp) != wp->w_bufp->b_linep){
                --i;
                lp = lback(lp);
	    }

	    /*
	     * this is supposed to speed things up by using tcap sequences
	     * to efficiently scroll the terminal screen.  the thinking here
	     * is that its much faster to update pscreen[] than to actually
	     * write the stuff to the screen...
	     */
	    if(optimize){
		switch(scroll){
		  case 1:			/* scroll text down */
		    j = j-i+1;			/* add one for dot line */
			/* 
			 * do we scroll down the header as well?  Well, only 
			 * if we're not editing the header, we've backed up 
			 * to the top, and the composer is not being 
			 * displayed...
			 */
		    if(Pmaster && Pmaster->headents && !ComposerEditing 
		       && (lback(lp) == wp->w_bufp->b_linep)
		       && (ComposerTopLine == COMPOSER_TOP_LINE))
		      j += entry_line(1000, TRUE); /* Never > 1000 headers */

		    scrolldown(wp, -1, j);
		    break;
		  case 2:			/* scroll text up */
		    j = wp->w_ntrows - (j-i);	/* we chose new top line! */
		    if(Pmaster && j){
			/* 
			 * do we scroll down the header as well?  Well, only 
			 * if we're not editing the header, we've backed up 
			 * to the top, and the composer is not being 
			 * displayed...
			 */
			if(!ComposerEditing 
			   && (ComposerTopLine != COMPOSER_TOP_LINE))
			  scrollup(wp, COMPOSER_TOP_LINE, 
				   j+entry_line(1000, TRUE));
			else
			  scrollup(wp, -1, j);
		    }
		    else
		      scrollup(wp, -1, j);
		    break;
		    default :
		      break;
		}
	    }

            wp->w_linep = lp;
            wp->w_flag |= WFHARD;       /* Force full. */
out:
	    /*
	     * if the line at the top of the page is the top line
	     * in the body, show the header...
	     */
	    if(Pmaster && Pmaster->headents && !ComposerEditing){
		if(lback(wp->w_linep) == wp->w_bufp->b_linep){
		    if(ComposerTopLine == COMPOSER_TOP_LINE){
			i = term.t_nrow - 2 - term.t_mrow - HeaderLen();
			if(i > 0 && nlforw() >= i) {	/* room for header ? */
			    if((i = nlforw()/2) == 0 && term.t_nrow&1)
			      i = 1;
			    while(wp->w_linep != wp->w_bufp->b_linep && i--)
			      wp->w_linep = lforw(wp->w_linep);
			    
			}
			else
			  ToggleHeader(1);
		    }
		}
		else{
		    if(ComposerTopLine != COMPOSER_TOP_LINE)
		      ToggleHeader(0);		/* hide it ! */
		}
	    }

            /* Try to use reduced update. Mode line update has its own special
             * flag. The fast update is used if the only thing to do is within
             * the line editing.
             */
            lp = wp->w_linep;
            i = wp->w_toprow;

            if ((wp->w_flag & ~WFMODE) == WFEDIT){
                while (lp != wp->w_dotp){
                    ++i;
                    lp = lforw(lp);
		}

                vscreen[i]->v_flag |= VFCHG;
                vtmove(i, 0);

                for (j = 0; j < llength(lp); ++j)
                    vtputc(lgetc(lp, j));

                vteeol();
	    }
	    else if ((wp->w_flag & (WFEDIT | WFHARD)) != 0){
                while (i < wp->w_toprow+wp->w_ntrows){
                    vscreen[i]->v_flag |= VFCHG;
                    vtmove(i, 0);

		    /* if line has been changed */
                    if (lp != wp->w_bufp->b_linep){
                        for (j = 0; j < llength(lp); ++j)
                            vtputc(lgetc(lp, j));

                        lp = lforw(lp);
		    }

                    vteeol();
                    ++i;
		}
	    }
#if ~WFDEBUG
            if ((wp->w_flag&WFMODE) != 0)
                modeline(wp);

            wp->w_flag  = 0;
            wp->w_force = 0;
#endif
	}
#if WFDEBUG
        modeline(wp);
        wp->w_flag =  0;
        wp->w_force = 0;
#endif

	/* and onward to the next window */
        wp = wp->w_wndp;
    }

    /* Always recompute the row and column number of the hardware cursor. This
     * is the only update for simple moves.
     */
    lp = curwp->w_linep;
    currow = curwp->w_toprow;

    while (lp != curwp->w_dotp){
        ++currow;
        lp = lforw(lp);
    }

    curcol = 0;
    i = 0;

    while (i < curwp->w_doto){
	c = lgetc(lp, i++);

        if (c.c == '\t')
            curcol |= 0x07;
        else if (ISCONTROL(c.c))
            ++curcol;

        ++curcol;
    }

    if (curcol >= term.t_ncol) { 		/* extended line. */
	/* flag we are extended and changed */
	vscreen[currow]->v_flag |= VFEXT | VFCHG;
	updext();				/* and output extended line */
    } else
      lbound = 0;				/* not extended line */

    /* make sure no lines need to be de-extended because the cursor is
     * no longer on them 
     */

    wp = wheadp;

    while (wp != NULL) {
	lp = wp->w_linep;
	i = wp->w_toprow;

	while (i < wp->w_toprow + wp->w_ntrows) {
	    if (vscreen[i]->v_flag & VFEXT) {
		/* always flag extended lines as changed */
		vscreen[i]->v_flag |= VFCHG;
		if ((wp != curwp) || (lp != wp->w_dotp) ||
		    (curcol < term.t_ncol)) {
		    vtmove(i, 0);
		    for (j = 0; j < llength(lp); ++j)
		      vtputc(lgetc(lp, j));
		    vteeol();

		    /* this line no longer is extended */
		    vscreen[i]->v_flag &= ~VFEXT;
		}
	    }
	    lp = lforw(lp);
	    ++i;
	}
	/* and onward to the next window */
        wp = wp->w_wndp;
    }

    /* Special hacking if the screen is garbage. Clear the hardware screen,
     * and update your copy to agree with it. Set all the virtual screen
     * change bits, to force a full update.
     */

    if (sgarbf != FALSE){
	if(Pmaster){
	    int rv;
       
	    showCompTitle();

	    if(ComposerTopLine != COMPOSER_TOP_LINE){
		UpdateHeader(0);		/* arrange things */
		PaintHeader(COMPOSER_TOP_LINE, TRUE);
	    }

	    /*
	     * since we're using only a portion of the screen and only 
	     * one buffer, only clear enough screen for the current window
	     * which is to say the *only* window.
	     */
	    for(i=wheadp->w_toprow;i<=term.t_nrow; i++){
		movecursor(i, 0);
		peeol();
		vscreen[i]->v_flag |= VFCHG;
	    }
	    rv = (*Pmaster->showmsg)('X' & 0x1f);	/* ctrl-L */
	    ttresize();
	    picosigs();		/* restore altered handlers */
	    if(rv)		/* Did showmsg corrupt the display? */
	      PaintBody(0);	/* Yes, repaint */
	    movecursor(wheadp->w_toprow, 0);
	}
	else{
	    for (i = 0; i < term.t_nrow-term.t_mrow; ++i){
		vscreen[i]->v_flag |= VFCHG;
		vp1 = pscreen[i];
		c.c = ' ';
		c.a = 0;
		for (j = 0; j < term.t_ncol; ++j)
		  vp1->v_text[j] = c;
	    }

	    movecursor(0, 0);	               /* Erase the screen. */
	    (*term.t_eeop)();

	}

        sgarbf = FALSE;				/* Erase-page clears */
        mpresf = FALSE;				/* the message area. */

	if(Pmaster)
	  modeline(curwp);
	else
	  sgarbk = TRUE;			/* fix the keyhelp as well...*/
    }

    /* Make sure that the physical and virtual displays agree. Unlike before,
     * the "updateline" code is only called with a line that has been updated
     * for sure.
     */
    if(Pmaster)
      i = curwp->w_toprow;
    else
      i = 0;

    if (term.t_nrow > term.t_mrow)
       c.c = term.t_nrow - term.t_mrow;
    else
       c.c = 0;

    for (; i < (int)c.c; ++i){

        vp1 = vscreen[i];

	/* for each line that needs to be updated, or that needs its
	   reverse video status changed, call the line updater	*/
	j = vp1->v_flag;
        if (j & VFCHG){

#if	TYPEAH
	    if (typahead()){
#ifdef _WINDOWS
		mswin_endupdate ();
#endif
	        return;
	    }
#endif
            vp2 = pscreen[i];

            updateline(i, &vp1->v_text[0], &vp2->v_text[0], &vp1->v_flag);

	}
    }

    if(Pmaster == NULL){

	if(sgarbk != FALSE){
	    if(term.t_mrow > 0){
		movecursor(term.t_nrow-1, 0);
		peeol();
		movecursor(term.t_nrow, 0);
		peeol();
	    }

	    if(lastflag&CFFILL){
		menu_pico[UNCUT_KEY].label = "UnJustify";
		if(!(lastflag&CFFLBF)){
		    emlwrite("Can now UnJustify!", NULL);
		    mpresf = FARAWAY;	/* remove this after next keystroke! */
		}
	    }
	    else
	      menu_pico[UNCUT_KEY].label = "UnCut Text";

	    wkeyhelp(menu_pico);
	    sgarbk = FALSE;
        }
    }
    if(lastflag&CFFLBF){
	emlwrite("Can now UnJustify!", NULL);
	mpresf = FARAWAY;  /* remove this after next keystroke! */
    }

    /* Finally, update the hardware cursor and flush out buffers. */

    movecursor(currow, curcol - lbound);
#ifdef _WINDOWS
    mswin_endupdate ();

    /* 
     * Update the scroll bars.  This function is where curbp->b_linecnt
     * is really managed.  See update_scroll.
     */
    update_scroll ();
#endif
    (*term.t_flush)();
}


/* updext - update the extended line which the cursor is currently
 *	    on at a column greater than the terminal width. The line
 *	    will be scrolled right or left to let the user see where
 *	    the cursor is
 */
void
updext()
{
    register int rcursor;		/* real cursor location */
    register LINE *lp;			/* pointer to current line */
    register int j;			/* index into line */

    /* calculate what column the real cursor will end up in */
    rcursor = ((curcol - term.t_ncol) % term.t_scrsiz) + term.t_margin;
    lbound = curcol - rcursor + 1;

    /* scan through the line outputing characters to the virtual screen
     * once we reach the left edge
     */
    vtmove(currow, -lbound);		/* start scanning offscreen */
    lp = curwp->w_dotp;			/* line to output */
    for (j=0; j<llength(lp); ++j)	/* until the end-of-line */
      vtpute(lgetc(lp, j));

    /* truncate the virtual line */
    vteeol();

    /* and put a '$' in column 1 */
    vscreen[currow]->v_text[0].c = '$';
    vscreen[currow]->v_text[0].a = 0;
}


/*
 * Update a single line. This does not know how to use insert or delete
 * character sequences; we are using VT52 functionality. Update the physical
 * row and column variables. It does try an exploit erase to end of line. The
 * RAINBOW version of this routine uses fast video.
 */
void
updateline(row, vline, pline, flags)
int  row;
CELL vline[];				/* what we want it to end up as */
CELL pline[];				/* what it looks like now       */
short *flags;				/* and how we want it that way  */
{
    register CELL *cp1;
    register CELL *cp2;
    register CELL *cp3;
    register CELL *cp4;
    register CELL *cp5;
    register CELL *cp6;
    register CELL *cp7;
    register int  display = TRUE;
    register int nbflag;		/* non-blanks to the right flag? */


    /* set up pointers to virtual and physical lines */
    cp1 = &vline[0];
    cp2 = &pline[0];
    cp3 = &vline[term.t_ncol];

    /* advance past any common chars at the left */
    while (cp1 != cp3 && cp1[0].c == cp2[0].c && cp1[0].a == cp2[0].a) {
	++cp1;
	++cp2;
    }

/* This can still happen, even though we only call this routine on changed
 * lines. A hard update is always done when a line splits, a massive
 * change is done, or a buffer is displayed twice. This optimizes out most
 * of the excess updating. A lot of computes are used, but these tend to
 * be hard operations that do a lot of update, so I don't really care.
 */
    /* if both lines are the same, no update needs to be done */
    if (cp1 == cp3){
	*flags &= ~VFCHG;			/* mark it clean */
	return;
    }

    /* find out if there is a match on the right */
    nbflag = FALSE;
    cp3 = &vline[term.t_ncol];
    cp4 = &pline[term.t_ncol];

    while (cp3[-1].c == cp4[-1].c && cp3[-1].a == cp4[-1].a) {
	--cp3;
	--cp4;
	if (cp3[0].c != ' ' || cp3[0].a != 0)/* Note if any nonblank */
	  nbflag = TRUE;		/* in right match. */
    }

    cp5 = cp3;

    if (nbflag == FALSE && eolexist == TRUE) {	/* Erase to EOL ? */
	while (cp5 != cp1 && cp5[-1].c == ' ' && cp5[-1].a == 0)
	  --cp5;

	if (cp3-cp5 <= 3)		/* Use only if erase is */
	  cp5 = cp3;			/* fewer characters. */
    }

    movecursor(row, cp1-&vline[0]);		/* Go to start of line. */

    if (!nbflag) {				/* use insert or del char? */
	cp6 = cp3;
	cp7 = cp4;

	if(inschar&&(cp7!=cp2 && cp6[0].c==cp7[-1].c && cp6[0].a==cp7[-1].a)){
	    while (cp7 != cp2 && cp6[0].c==cp7[-1].c && cp6[0].a==cp7[-1].a){
		--cp7;
		--cp6;
	    }

	    if (cp7==cp2 && cp4-cp2 > 3){
		o_insert((int)cp1->c);  /* insert the char */
		display = FALSE;        /* only do it once!! */
	    }
	}
	else if(delchar && cp3 != cp1 && cp7[0].c == cp6[-1].c
		&& cp7[0].a == cp6[-1].a){
	    while (cp6 != cp1 && cp7[0].c==cp6[-1].c && cp7[0].a==cp6[-1].a){
		--cp7;
		--cp6;
	    }

	    if (cp6==cp1 && cp5-cp6 > 3){
		o_delete();		/* insert the char */
		display = FALSE;        /* only do it once!! */
	    }
	}
    }

    while (cp1 != cp5) {		/* Ordinary. */
	if(display){
	    (*term.t_rev)(cp1->a);	/* set inverse for this char */
	    (*term.t_putchar)(cp1->c);
	}

	++ttcol;
	*cp2++ = *cp1++;
    }

    (*term.t_rev)(0);			/* turn off inverse anyway! */

    if (cp5 != cp3) {			/* Erase. */
	if(display)
	  peeol();
	while (cp1 != cp3)
	  *cp2++ = *cp1++;
    }
    *flags &= ~VFCHG;			/* flag this line is changed */
}


/*
 * Redisplay the mode line for the window pointed to by the "wp". This is the
 * only routine that has any idea of how the modeline is formatted. You can
 * change the modeline format by hacking at this routine. Called by "update"
 * any time there is a dirty window.
 */
void
modeline(wp)
WINDOW *wp;
{
    if(Pmaster){
        if(ComposerEditing)
	  ShowPrompt();
	else{
	    menu_compose[EXIT_KEY].label  = (Pmaster->headents)
					      ? "Send" :"Exit";
	    menu_compose[PSTPN_KEY].name  = (Pmaster->headents)
					      ? "^O" : NULL;
	    menu_compose[PSTPN_KEY].label = (Pmaster->headents)
					      ? "Postpone" : NULL;
	    menu_compose[WHERE_KEY].name  = (Pmaster->alt_ed) ? "^_" : "^W";
	    menu_compose[WHERE_KEY].label = (Pmaster->alt_ed) ? "Alt Edit" 
							      : "Where is";
	    KS_OSDATASET(&menu_compose[WHERE_KEY],
			 (Pmaster->alt_ed) ? KS_ALTEDITOR : KS_WHEREIS);
	    menu_compose[UNCUT_KEY].label = (thisflag&CFFILL) ? "UnJustify"
							      : "UnCut Text";
	    wkeyhelp(menu_compose);
#ifdef _WINDOWS
	    /* When alt editor is available "Where is" is not on the menu
	     * but the command is still available.  This call enables any
	     * "Where is" menu items. */
	    if (Pmaster->alt_ed)
		mswin_menuitemadd (MENU|CTRL|'W', "", KS_WHEREIS, 0);
#endif 
	}
    }
    else{
	register char *cp;
	register int n;		/* cursor position count */
	register BUFFER *bp;
	register int i;		/* loop index */
	register int lchar;	/* character to draw line in buffer with */
	char     tline[NLINE];	/* buffer for part of mode line */
	CELL     c;

	n = 0;
	c.a = 1;
	vtmove(1, 0);
	vteeol();
	vscreen[n]->v_flag |= VFCHG; /* Redraw next time. */
	vtmove(n, 0);		/* Seek to right line. */

#if	REVSTA
	if (revexist)
	  lchar = ' ';
	else
#endif
	  lchar = '-';

	c.c = lchar;
	vtputc(c);
	bp = wp->w_bufp;

	n = 1;

	sprintf(cp = tline, PICO_TITLE, version);	/* write version */

	while(c.c = *cp++){
	    vtputc(c);
	    ++n;
	}

	if(bp->b_fname[0]){				/* File name? */
	    char *p, *endp, *prefix;

	    prefix = NULL;				/* for abreviation */
	    endp   = strchr(cp = bp->b_fname, '\0');	/* find eol */
	    i	   = term.t_ncol - n - 22;		/* space available */
	    while(i > 0 && endp - cp > i){
		if(!prefix){
		    prefix = ".../";			/* path won't fit! */
		    i -= 4;
		}

		if(!(p = strchr(cp, '/'))){		/* neat break? */
		    cp = endp - i;			/* do best we can */
		    break;
		}
		else
		  cp = p + 1;
	    }

	    sprintf(tline, "%*.sFile: %s%s",
		    ((i - (endp - cp)) > 0) ? (i - (endp - cp))/2 : 0, " ",
		    prefix ? prefix : "", cp);

	    for(cp = tline; c.c = *cp; cp++, n++)
	      vtputc(c);
        }
        else{
	    cp = PICO_NEWBUF_MSG;
	    if(sizeof(PICO_NEWBUF_MSG) < term.t_ncol){	/* enough room? */
		c.c = lchar;
		for(i = (term.t_ncol-sizeof(PICO_NEWBUF_MSG))/2; n < i; n++)
		  vtputc(c);
	    }

	    while(c.c = *cp++){
		vtputc(c);
		++n;
	    }
	}

#if WFDEBUG
	vtputc(lchar);
	vtputc((wp->w_flag&WFMODE)!=0  ? 'M' : lchar);
	vtputc((wp->w_flag&WFHARD)!=0  ? 'H' : lchar);
	vtputc((wp->w_flag&WFEDIT)!=0  ? 'E' : lchar);
	vtputc((wp->w_flag&WFMOVE)!=0  ? 'V' : lchar);
	vtputc((wp->w_flag&WFFORCE)!=0 ? 'F' : lchar);
	n += 6;
#endif
	i = term.t_ncol - n;				/* space available */
	cp = NULL;
	if(bp->b_flag&BFCHG){				/* "MOD" if changed. */
	    cp = PICO_MOD_MSG;
	    i  = (i > sizeof(PICO_MOD_MSG)) ? i - sizeof(PICO_MOD_MSG) : 0;
	}

	c.c = lchar;
	while(i-- > 0)					/* Pad width */
	  vtputc(c);

	if(cp)
	  while(c.c = *cp++)
	    vtputc(c);
    }
}



/*
 * Send a command to the terminal to move the hardware cursor to row "row"
 * and column "col". The row and column arguments are origin 0. Optimize out
 * random calls. Update "ttrow" and "ttcol".
 */
void
movecursor(row, col)
int row, col;
{
    if (row!=ttrow || col!=ttcol) {
        ttrow = row;
        ttcol = col;
        (*term.t_move)(row, col);
    }
}


/*
 * Erase any sense we have of the cursor's HW location...
 */
void
clearcursor()
{
    ttrow = ttcol = FARAWAY;
}

void
get_cursor(row, col)
    int *row, *col;
{
    if(row)
      *row = ttrow;
    if(col)
      *col = ttcol;
}


/*
 * Erase the message line. This is a special routine because the message line
 * is not considered to be part of the virtual screen. It always works
 * immediately; the terminal buffer is flushed via a call to the flusher.
 */
void
mlerase()
{
    if (term.t_nrow < term.t_mrow)
      return;

    movecursor(term.t_nrow - term.t_mrow, 0);
    (*term.t_rev)(0);
    if (eolexist == TRUE)
      peeol();
    else
      while(++ttcol < term.t_ncol)		/* track's ttcol */
	(*term.t_putchar)(' ');

    (*term.t_flush)();
    mpresf = FALSE;
}


/*
 * Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with a ^G. if d >= 0, d is the default answer returned. Otherwise there
 * is no default.
 */
mlyesno(prompt, dflt)
char  *prompt;
int   dflt;
{
    int     rv;
    char    buf[NLINE];
    KEYMENU menu_yesno[12];

#ifdef _WINDOWS
    if (mswin_usedialog ()) 
      switch (mswin_yesno (prompt)) {
	default:
	case 0:		return (ABORT);
	case 1:		return (TRUE);
	case 2:		return (FALSE);
      }
#endif  

    for(rv = 0; rv < 12; rv++){
	menu_yesno[rv].name = NULL;
	KS_OSDATASET(&menu_yesno[rv], KS_NONE);
    }

    menu_yesno[1].name  = "Y";
    menu_yesno[1].label = (dflt == TRUE) ? "[Yes]" : "Yes";
    menu_yesno[6].name  = "^C";
    menu_yesno[6].label = "Cancel";
    menu_yesno[7].name  = "N";
    menu_yesno[7].label = (dflt == FALSE) ? "[No]" : "No";
    wkeyhelp(menu_yesno);		/* paint generic menu */
    sgarbk = TRUE;			/* mark menu dirty */
    if(Pmaster && curwp)
      curwp->w_flag |= WFMODE;

    sprintf(buf, "%s ? ", prompt);
    mlwrite(buf, NULL);
    (*term.t_rev)(1);
    rv = -1;
    while(1){
	switch(GetKey()){
	  case (CTRL|'M') :		/* default */
	    if(dflt >= 0){
		pputs((dflt) ? "Yes" : "No", 1);
		rv = dflt;
	    }
	    else
	      (*term.t_beep)();

	    break;

	  case (CTRL|'C') :		/* Bail out! */
	  case F2         :
	    pputs("ABORT", 1);
	    rv = ABORT;
	    break;

	  case 'y' :
	  case 'Y' :
	  case F3  :
	    pputs("Yes", 1);
	    rv = TRUE;
	    break;

	  case 'n' :
	  case 'N' :
	  case F4  :
	    pputs("No", 1);
	    rv = FALSE;
	    break;

	  case (CTRL|'G') :
	    if(term.t_mrow == 0 && km_popped == 0){
		movecursor(term.t_nrow-2, 0);
		peeol();
		term.t_mrow = 2;
		(*term.t_rev)(0);
		wkeyhelp(menu_yesno);		/* paint generic menu */
		mlwrite(buf, NULL);
		(*term.t_rev)(1);
		sgarbk = TRUE;			/* mark menu dirty */
		km_popped++;
		break;
	    }
	    /* else fall through */

	  default:
	    (*term.t_beep)();
	  case NODATA :
	    break;
	}

	(*term.t_flush)();
	if(rv != -1){
	    (*term.t_rev)(0);
	    if(km_popped){
		term.t_mrow = 0;
		movecursor(term.t_nrow, 0);
		peeol();
		sgarbf = 1;
		km_popped = 0;
	    }

	    return(rv);
	}
    }
}



/*
 * Write a prompt into the message line, then read back a response. Keep
 * track of the physical position of the cursor. If we are in a keyboard
 * macro throw the prompt away, and return the remembered response. This
 * lets macros run at full speed. The reply is always terminated by a carriage
 * return. Handle erase, kill, and abort keys.
 */
mlreply(prompt, buf, nbuf, flg, extras)
char	  *prompt, *buf;
int	   nbuf, flg;
EXTRAKEYS *extras;
{
    return(mlreplyd(prompt, buf, nbuf, flg|QDEFLT, extras));
}


/*
 * function key mappings
 */
static int rfkm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'C')},
    { F3,  0 },
    { F4,  0 },
    { F5,  0 },
    { F6,  0 },
    { F7,  0 },
    { F8,  0 },
    { F9,  0 },
    { F10, 0 },
    { F11, 0 },
    { F12, 0 }
};


/*
 * mlreplyd - write the prompt to the message line along with an default
 *	      answer already typed in.  Carraige return accepts the
 *	      default.  answer returned in buf which also holds the initial
 *            default, nbuf is it's length, def set means use default value,
 *            and ff means for-file which checks that all chars are allowed
 *            in file names.
 */
mlreplyd(prompt, buf, nbuf, flg, extras)
char	  *prompt;
char	  *buf;
int	   nbuf, flg;
EXTRAKEYS *extras;
{
    register int    c;				/* current char       */
    register char   *b;				/* pointer in buf     */
    register int    i, j;
    register int    maxl;
    register int    plen;
    int      changed = FALSE;
    int      return_val = 0;
    KEYMENU  menu_mlreply[12];
    int	     extra_v[12];

#ifdef _WINDOWS
    if (mswin_usedialog ()) {
	MDlgButton		btn_list[12];
	int			i, j;

	memset (&btn_list, 0, sizeof (MDlgButton) * 12);
	j = 0;
	for (i = 0; extras && extras[i].name != NULL; ++i) {
	    if (extras[i].label[0] != '\0') {
		if ((extras[i].key & CTRL) == CTRL) 
		    btn_list[j].ch = (extras[i].key & ~CTRL) - '@';
		else
		    btn_list[j].ch = extras[i].key;
		btn_list[j].rval = extras[i].key;
		btn_list[j].name = extras[i].name;
		btn_list[j++].label = extras[i].label;
	    }
	}
	btn_list[j].ch = -1;

	return (mswin_dialog (prompt, buf, nbuf, ((flg&QDEFLT) > 0), 
			FALSE, btn_list, NULL, 0));
    }
#endif

    menu_mlreply[0].name = "^G";
    menu_mlreply[0].label = "Get Help";
    KS_OSDATASET(&menu_mlreply[0], KS_SCREENHELP);
    for(j = 0, i = 1; i < 6; i++){	/* insert odd extras */
	menu_mlreply[i].name = NULL;
	KS_OSDATASET(&menu_mlreply[i], KS_NONE);
	rfkm[2*i][1] = 0;
	if(extras){
	    for(; extras[j].name && j != 2*(i-1); j++)
	      ;

	    if(extras[j].name){
		rfkm[2*i][1]	      = extras[j].key;
		menu_mlreply[i].name  = extras[j].name;
		menu_mlreply[i].label = extras[j].label;
		KS_OSDATASET(&menu_mlreply[i], KS_OSDATAGET(&extras[j]));
	    }
	}
    }

    menu_mlreply[6].name = "^C";
    menu_mlreply[6].label = "Cancel";
    KS_OSDATASET(&menu_mlreply[6], KS_NONE);
    for(j = 0, i = 7; i < 12; i++){	/* insert even extras */
	menu_mlreply[i].name = NULL;
	rfkm[2*(i-6)+1][1] = 0;
	if(extras){
	    for(; extras[j].name && j != (2*(i-6)) - 1; j++)
	      ;

	    if(extras[j].name){
		rfkm[2*(i-6)+1][1]    = extras[j].key;
		menu_mlreply[i].name  = extras[j].name;
		menu_mlreply[i].label = extras[j].label;
		KS_OSDATASET(&menu_mlreply[i], KS_OSDATAGET(&extras[j]));
	    }
	}
    }

    /* set up what to watch for and return values */
    memset(extra_v, 0, sizeof(extra_v));
    for(i = 0, j = 0; i < 12 && extras && extras[i].name; i++)
      extra_v[j++] = extras[i].key;

    plen = mlwrite(prompt, NULL);		/* paint prompt */
    if(!(flg&QDEFLT))
      *buf = '\0';

    (*term.t_rev)(1);

    maxl = (nbuf-1 < term.t_ncol - plen - 1) ? nbuf-1 : term.t_ncol - plen - 1;

    pputs(buf, 1);
    b = &buf[(flg & QBOBUF) ? 0 : strlen(buf)];
    
    (*term.t_rev)(0);
    wkeyhelp(menu_mlreply);		/* paint generic menu */
    sgarbk = 1;				/* mark menu dirty */
    (*term.t_rev)(1);

    for (;;) {
	movecursor(term.t_nrow - term.t_mrow, plen + b - buf);
	(*term.t_flush)();

#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0x5, 0);
	register_mfunc(mouse_in_content, 
		       term.t_nrow - term.t_mrow, plen,
		       term.t_nrow - term.t_mrow, plen + maxl);
#endif
#ifdef	_WINDOWS
	mswin_allowpaste(MSWIN_PASTE_LINE);
#endif
	while((c = GetKey()) == NODATA)
	  ;

#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_allowpaste(MSWIN_PASTE_DISABLE);
#endif

	switch(c = normalize_cmd(c, rfkm, 1)){
	  case (CTRL|'A') :			/* CTRL-A beginning     */
	  case KEY_HOME :
	    b = buf;
	    continue;

	  case (CTRL|'B') :			/* CTRL-B back a char   */
	    if(ttcol > plen)
		b--;
	    continue;

	  case (CTRL|'C') :			/* CTRL-C abort		*/
	    pputs("ABORT", 1);
	    ctrlg(FALSE, 0);
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return_val = ABORT;
	    goto ret;

	  case (CTRL|'E') :			/* CTRL-E end of line   */
	  case KEY_END  :
	    b = &buf[strlen(buf)];
	    continue;

	  case (CTRL|'F') :			/* CTRL-F forward a char*/
	    if(*b != '\0')
		b++;
	    continue;

	  case (CTRL|'G') :			/* CTRL-G help		*/
	    if(term.t_mrow == 0 && km_popped == 0){
		movecursor(term.t_nrow-2, 0);
		peeol();
		sgarbk = 1;			/* mark menu dirty */
		km_popped++;
		term.t_mrow = 2;
		(*term.t_rev)(0);
		wkeyhelp(menu_mlreply);		/* paint generic menu */
		plen = mlwrite(prompt, NULL);		/* paint prompt */
		(*term.t_rev)(1);
		pputs(buf, 1);
		break;
	    }

	    pputs("HELP", 1);
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return_val = HELPCH;
	    goto ret;

	  case (CTRL|'H') :			/* CTRL-H backspace	*/
	  case 0x7f :				/*        rubout	*/
	    if (b <= buf)
	      break;
	    b--;
	    ttcol--;				/* cheating!  no pputc */
	    (*term.t_putchar)('\b');

	  case (CTRL|'D') :			/* CTRL-D delete char   */
	  case KEY_DEL :
	    changed=TRUE;
	    i = 0;
	    do					/* blat out left char   */
	      b[i] = b[i+1];
	    while(b[i++] != '\0');
	    break;

	  case (CTRL|'L') :			/* CTRL-L redraw	*/
	    (*term.t_rev)(0);
	    return_val = (CTRL|'L');
	    goto ret;

	  case (CTRL|'K') :			/* CTRL-K kill line	*/
	    changed=TRUE;
	    buf[0] = '\0';
	    b = buf;
	    movecursor(ttrow, plen);
	    break;

	  case KEY_LEFT:
	    if(ttcol > plen)
	      b--;
	    continue;

	  case KEY_RIGHT:
	    if(*b != '\0')
	      b++;
	    continue;

	  case F1 :				/* sort of same thing */
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return_val = HELPCH;
	    goto ret;

	  case (CTRL|'M') :			/*        newline       */
	    (*term.t_rev)(0);
	    (*term.t_flush)();
	    return_val = changed;
	    goto ret;

#ifdef	MOUSE
	  case KEY_MOUSE :
	    {
	      MOUSEPRESS mp;

	      mouse_get_last (NULL, &mp);

	      /* The clicked line have anything special on it? */
	      switch(mp.button){
		case M_BUTTON_LEFT :			/* position cursor */
		  mp.col -= plen;			/* normalize column */
		  if(mp.col >= 0 && mp.col <= strlen(buf))
		    b = buf + mp.col;

		  break;

		case M_BUTTON_RIGHT :
#ifdef	_WINDOWS
		  mswin_allowpaste(MSWIN_PASTE_LINE);
		  mswin_paste_popup();
		  mswin_allowpaste(MSWIN_PASTE_DISABLE);
		  break;
#endif

		case M_BUTTON_MIDDLE :			/* NO-OP for now */
		default:				/* just ignore */
		  break;
	      }
	    }

	    continue;
#endif

	  default : 
	    if(strlen(buf) >= maxl){		/* contain the text      */
		(*term.t_beep)();
		continue;
	    }

	    /* look for match in extra_v */
	    for(i = 0; i < 12; i++)
	      if(c && c == extra_v[i]){
		  (*term.t_rev)(0);
		  return_val = c;
		  goto ret;
	      }

	    changed=TRUE;

	    if(c&(~0xff)){			/* bag ctrl/special chars */
		(*term.t_beep)();
	    }
	    else{
		i = strlen(b);
		if(flg&QFFILE){
		    if(!fallowc(c)){ 		/* c OK in filename? */
			(*term.t_beep)();
			continue;
		    }
		}
		if(flg&QNODQT){	                /* reject double quotes? */
		    if(c == '"'){
			(*term.t_beep)();
			continue;
		    }
		}
		

		do				/* blat out left char   */
		  b[i+1] = b[i];
		while(i-- > 0);

		pputc(*b++ = c, 0);
	    }
	}

	pputs(b, 1);				/* show default */
	i = term.t_ncol-1;
	while(pscreen[ttrow]->v_text[i].c == ' ' 
	      && pscreen[ttrow]->v_text[i].a == 0)
	  i--;

	while(ttcol <= i)
	  pputc(' ', 0);
    }

ret:
    if(km_popped){
	term.t_mrow = 0;
	movecursor(term.t_nrow, 0);
	peeol();
	sgarbf = 1;
	km_popped = 0;
    }

    return(return_val);
}


/*
 * emlwrite() - write the message string to the error half of the screen
 *              center justified.  much like mlwrite (which is still used
 *              to paint the line for prompts and such), except it center
 *              the text.
 */
void
emlwrite(message, arg) 
char	*message;
void	*arg;
{
    register char *bufp = message;
    register char *ap;
    register long l;

    mlerase();

    if((l = strlen(message)) == 0 || term.t_nrow < 2)	
      return;    /* nothing to write or no space to write, bag it */

    /*
     * next, figure out where the to move the cursor so the message 
     * comes out centered
     */
    if((ap=(char *)strchr(message, '%')) != NULL){
	l -= 2;
	switch(ap[1]){
	  case '%':
	  case 'c':
	    l += arg ? 2 : 1;
	    break;
	  case 'd':
	    l += (long)dumbroot((int)arg, 10);
	    break;
	  case 'D':
	    l += (long)dumblroot((long)arg, 10);
	    break;
	  case 'o':
	    l += (long)dumbroot((int)arg, 8);
	    break;
	  case 'x':
	    l += (long)dumbroot((int)arg, 16);
	    break;
	  case 's':
            l += arg ? strlen((char *)arg) : 2;
	    break;
	}
    }

    if(l+4 <= term.t_ncol)
      movecursor(term.t_nrow-term.t_mrow, (term.t_ncol - ((int)l + 4))/2);
    else
      movecursor(term.t_nrow-term.t_mrow, 0);

    (*term.t_rev)(1);
    pputs("[ ", 1);
    while (*bufp != '\0' && ttcol < term.t_ncol-2){
	if(*bufp == '\007')
	  (*term.t_beep)();
	else if(*bufp == '%'){
	    switch(*++bufp){
	      case 'c':
		if(arg)
		  pputc((char)(int)arg, 0);
		else {
		    pputs("%c", 0);
		}
		break;
	      case 'd':
		mlputi((int)arg, 10);
		break;
	      case 'D':
		mlputli((long)arg, 10);
		break;
	      case 'o':
		mlputi((int)arg, 16);
		break;
	      case 'x':
		mlputi((int)arg, 8);
		break;
	      case 's':
		pputs(arg ? (char *)arg : "%s", 0);
		break;
	      case '%':
	      default:
		pputc(*bufp, 0);
		break;
	    }
	}
	else
	  pputc(*bufp, 0);
	bufp++;
    }

    pputs(" ]", 1);
    (*term.t_rev)(0);
    (*term.t_flush)();
    mpresf = TRUE;
}


/*
 * Write a message into the message line. Keep track of the physical cursor
 * position. A small class of printf like format items is handled. Assumes the
 * stack grows down; this assumption is made by the "++" in the argument scan
 * loop. Set the "message line" flag TRUE.
 */
mlwrite(fmt, arg)
char *fmt;
void *arg;
{
    register int c;
    register char *ap;

    /*
     * the idea is to only highlight if there is something to show
     */
    mlerase();

    ttcol = 0;
    (*term.t_rev)(1);
    ap = (char *) &arg;
    while ((c = *fmt++) != 0) {
        if (c != '%') {
            (*term.t_putchar)(c);
            ++ttcol;
	}
        else {
            c = *fmt++;
            switch (c) {
	      case 'd':
		mlputi(*(int *)ap, 10);
		ap += sizeof(int);
		break;

	      case 'o':
		mlputi(*(int *)ap,  8);
		ap += sizeof(int);
		break;

	      case 'x':
		mlputi(*(int *)ap, 16);
		ap += sizeof(int);
		break;

	      case 'D':
		mlputli(*(long *)ap, 10);
		ap += sizeof(long);
		break;

	      case 's':
		pputs(*(char **)ap, 1);
		ap += sizeof(char *);
		break;

              default:
		(*term.t_putchar)(c);
		++ttcol;
	    }
	}
    }

    c = ttcol;
    while(ttcol < term.t_ncol)
      pputc(' ', 0);

    movecursor(term.t_nrow - term.t_mrow, c);
    (*term.t_rev)(0);
    (*term.t_flush)();
    mpresf = TRUE;
    return(c);
}


/*
 * Write out an integer, in the specified radix. Update the physical cursor
 * position. This will not handle any negative numbers; maybe it should.
 */
void
mlputi(i, r)
int i, r;
{
    register int q;
    static char hexdigits[] = "0123456789ABCDEF";

    if (i < 0){
        i = -i;
	pputc('-', 1);
    }

    q = i/r;

    if (q != 0)
      mlputi(q, r);

    pputc(hexdigits[i%r], 1);
}


/*
 * do the same except as a long integer.
 */
void
mlputli(l, r)
long l;
int  r;
{
    register long q;

    if (l < 0){
        l = -l;
        pputc('-', 1);
    }

    q = l/r;

    if (q != 0)
      mlputli(q, r);

    pputc((int)(l%r)+'0', 1);
}


/*
 * scrolldown - use stuff to efficiently move blocks of text on the
 *              display, and update the pscreen array to reflect those
 *              moves...
 *
 *        wp is the window to move in
 *        r  is the row at which to begin scrolling
 *        n  is the number of lines to scrol
 */
void
scrolldown(wp, r, n)
WINDOW *wp;
int     r, n;
{
#ifdef	TERMCAP
    register int i;
    register int l;
    register VIDEO *vp1;
    register VIDEO *vp2;

    if(!n)
      return;

    if(r < 0){
	r = wp->w_toprow;
	l = wp->w_ntrows;
    }
    else{
	if(r > wp->w_toprow)
	    vscreen[r-1]->v_flag |= VFCHG;
	l = wp->w_toprow+wp->w_ntrows-r;
    }

    o_scrolldown(r, n);

    for(i=l-n-1; i >=  0; i--){
	vp1 = pscreen[r+i]; 
	vp2 = pscreen[r+i+n];
	bcopy(vp1, vp2, term.t_ncol * sizeof(CELL));
    }
    pprints(r+n-1, r);
    ttrow = FARAWAY;
    ttcol = FARAWAY;
#endif /* TERMCAP */
}


/*
 * scrollup - use tcap stuff to efficiently move blocks of text on the
 *            display, and update the pscreen array to reflect those
 *            moves...
 */
void
scrollup(wp, r, n)
WINDOW *wp;
int     r, n;
{
#ifdef	TERMCAP
    register int i;
    register VIDEO *vp1;
    register VIDEO *vp2;

    if(!n)
      return;

    if(r < 0)
      r = wp->w_toprow;

    o_scrollup(r, n);

    i = 0;
    while(1){
	if(Pmaster){
	    if(!(r+i+n < wp->w_toprow+wp->w_ntrows))
	      break;
	}
	else{
	    if(!((i < wp->w_ntrows-n)&&(r+i+n < wp->w_toprow+wp->w_ntrows)))
	      break;
	}
	vp1 = pscreen[r+i+n]; 
	vp2 = pscreen[r+i];
	bcopy(vp1, vp2, term.t_ncol * sizeof(CELL));
	i++;
    }
    pprints(wp->w_toprow+wp->w_ntrows-n, wp->w_toprow+wp->w_ntrows-1);
    ttrow = FARAWAY;
    ttcol = FARAWAY;
#endif /* TERMCAP */
}


/*
 * print spaces in the physical screen starting from row abs(n) working in
 * either the positive or negative direction (depending on sign of n).
 */
void
pprints(x, y)
int x, y;
{
    register int i;
    register int j;

    if(x < y){
	for(i = x;i <= y; ++i){
	    for(j = 0; j < term.t_ncol; j++){
		pscreen[i]->v_text[j].c = ' ';
		pscreen[i]->v_text[j].a = 0;
	    }
        }
    }
    else{
	for(i = x;i >= y; --i){
	    for(j = 0; j < term.t_ncol; j++){
		pscreen[i]->v_text[j].c = ' ';
		pscreen[i]->v_text[j].a = 0;
	    }
        }
    }
    ttrow = y;
    ttcol = 0;
}



/*
 * doton - return the physical line number that the dot is on in the
 *         current window, and by side effect the number of lines remaining
 */
doton(r, chs)
int       *r;
unsigned  *chs;
{
    register int  i = 0;
    register LINE *lp = curwp->w_linep;
    int      l = -1;

    *chs = 0;
    while(i++ < curwp->w_ntrows){
	if(lp == curwp->w_dotp)
	  l = i-1;
	lp = lforw(lp);
	if(lp == curwp->w_bufp->b_linep){
	    i++;
	    break;
	}
	if(l >= 0)
	  (*chs) += llength(lp);
    }
    *r = i - l - term.t_mrow;
    return(l+curwp->w_toprow);
}



/*
 * resize_pico - given new window dimensions, allocate new resources
 */
resize_pico(row, col)
int  row, col;
{
    int old_nrow, old_ncol;
    register int i;
    register VIDEO *vp;

    old_nrow = term.t_nrow;
    old_ncol = term.t_ncol;

    term.t_nrow = row;
    term.t_ncol = col;

    if (old_ncol == term.t_ncol && old_nrow == term.t_nrow)
      return(TRUE);

    if(curwp){
	curwp->w_toprow = 2;
	curwp->w_ntrows = term.t_nrow - curwp->w_toprow - term.t_mrow;
    }

    if(Pmaster){
	fillcol = Pmaster->fillcolumn;
	(*Pmaster->resize)();
    }
    else if(userfillcol > 0)
      fillcol = userfillcol;
    else
      fillcol = term.t_ncol - 6;	       /* we control the fill column */

    /* 
     * free unused screen space ...
     */
    for(i=term.t_nrow+1; i <= old_nrow; ++i){
	free((char *) vscreen[i]);
	free((char *) pscreen[i]);
    }

    /* 
     * realloc new space for screen ...
     */
    if((vscreen=(VIDEO **)realloc(vscreen,(term.t_nrow+1)*sizeof(VIDEO *))) == NULL){
	if(Pmaster)
	  return(-1);
	else
	  exit(1);
    }

    if((pscreen=(VIDEO **)realloc(pscreen,(term.t_nrow+1)*sizeof(VIDEO *))) == NULL){
	if(Pmaster)
	  return(-1);
	else
	  exit(1);
    }

    for (i = 0; i <= term.t_nrow; ++i) {
	if(i <= old_nrow)
	  vp = (VIDEO *) realloc(vscreen[i], sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));
	else
	  vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

	if (vp == NULL)
	  exit(1);
	vp->v_flag = VFCHG;
	vscreen[i] = vp;
	if(old_ncol < term.t_ncol){  /* don't let any garbage in */
	    vtrow = i;
	    vtcol = (i < old_nrow) ? old_ncol : 0;
	    vteeol();
	}

	if(i <= old_nrow)
	  vp = (VIDEO *) realloc(pscreen[i], sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));
	else
	  vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

	if (vp == NULL)
	  exit(1);

	vp->v_flag = VFCHG;
	pscreen[i] = vp;
    }

    if(!ResizeBrowser()){
	if(Pmaster && Pmaster->headents){
	    ResizeHeader();
	}
	else{
	    curwp->w_flag |= (WFHARD | WFMODE);
	    pico_refresh(0, 1);                /* redraw whole enchilada. */
	    update();                          /* do it */
	}
    }

    return(TRUE);
}

void
redraw_pico_for_callback()
{
    pico_refresh(0, 1);
    update();
}


/*
 * showCompTitle - display the anchor line passed in from pine
 */
void
showCompTitle()
{
    if(Pmaster){
	register char *bufp;
	extern   char *pico_anchor;
	COLOR_PAIR *lastc = NULL;

	if((bufp = pico_anchor) == NULL)
	  return;
	
	movecursor(COMPOSER_TITLE_LINE, 0);
	if (Pmaster->colors && Pmaster->colors->tbcp &&
	    pico_is_good_colorpair(Pmaster->colors->tbcp)){
	  lastc = pico_get_cur_color();
	  (void)pico_set_colorp(Pmaster->colors->tbcp, PSC_NONE);
	}
	else
	  (*term.t_rev)(1);   
	while (ttcol < term.t_ncol)
	  if(*bufp != '\0')
	    pputc(*bufp++, 1);
          else
	    pputc(' ', 1);

	if (lastc){
	  (void)pico_set_colorp(lastc, PSC_NONE);
	  free_color_pair(&lastc);
	}
	else
	  (*term.t_rev)(0);
	movecursor(COMPOSER_TITLE_LINE + 1, 0);
	peeol();
    }
}



/*
 * zotdisplay - blast malloc'd space created for display maps
 */
void
zotdisplay()
{
    register int i;

    for (i = 0; i <= term.t_nrow; ++i){		/* free screens */
	free((char *) vscreen[i]);
	free((char *) pscreen[i]);
    }

    free((char *) vscreen);
    free((char *) pscreen);
}



/*
 * nlforw() - returns the number of lines from the top to the dot
 */
nlforw()
{
    register int  i = 0;
    register LINE *lp = curwp->w_linep;
    
    while(lp != curwp->w_dotp){
	lp = lforw(lp);
	i++;
    }
    return(i);
}



/*
 * pputc - output the given char, keep track of it on the physical screen
 *	   array, and keep track of the cursor
 */
void
pputc(c, a)
int   c;				/* char to write */
int   a;				/* and its attribute */
{
    if((ttcol >= 0 && ttcol < term.t_ncol) 
       && (ttrow >= 0 && ttrow <= term.t_nrow)){
       
	/*
	 * Some terminals scroll when you write in the lower right corner
	 * of the screen, so don't write there.
	 */
	if(!(ttrow == term.t_nrow && ttcol == term.t_ncol -1)){
	    (*term.t_putchar)(c);			/* write it */
	    pscreen[ttrow]->v_text[ttcol].c = c;	/* keep track of it */
	    pscreen[ttrow]->v_text[ttcol].a = a;	/* keep track of it */
	}

	ttcol++;
    }
}


/*
 * pputs - print a string and keep track of the cursor
 */
void
pputs(s, a)
register char *s;			/* string to write */
register int   a;			/* and its attribute */
{
    while (*s != '\0')
      pputc(*s++, a);
}


/*
 * peeol - physical screen array erase to end of the line.  remember to
 *	   track the cursor.
 */
void
peeol()
{
    register int r = ttrow;
    register int c = ttcol;
    CELL         cl;

    cl.c = ' ';
    cl.a = 0;

    /*
     * Don't clear if we think we are sitting past the last column,
     * that erases the last column if we just wrote it.
     */
    if(c < term.t_ncol)
      (*term.t_eeol)();

    while(c < term.t_ncol && c >= 0 && r <= term.t_nrow && r >= 0)
      pscreen[r]->v_text[c++] = cl;
}


/*
 * pscr - return the character cell on the physical screen map on the 
 *        given line, l, and offset, o.
 */
CELL *
pscr(l, o)
int l, o;
{
    if((l >= 0 && l <= term.t_nrow) && (o >= 0 && o < term.t_ncol))
      return(&(pscreen[l]->v_text[o]));
    else
      return(NULL);
}


/*
 * pclear() - clear the physical screen from row x through row y
 */
void
pclear(x, y)
register int x;
register int y;
{
    register int i;

    for(i=x; i < y; i++){
	movecursor(i, 0);
	peeol();
    }
}


/*
 * dumbroot - just get close 
 */
dumbroot(x, b)
int x, b;
{
    if(x < b)
      return(1);
    else
      return(dumbroot(x/b, b) + 1);
}


/*
 * dumblroot - just get close 
 */
dumblroot(x, b)
long x;
int  b;
{
    if(x < b)
      return(1);
    else
      return(dumblroot(x/b, b) + 1);
}


/*
 * pinsertc - use optimized insert, fixing physical screen map.
 *            returns true if char written, false otherwise
 */
pinsert(c)
CELL c;
{
    register int   i;
    register CELL *p;

    if(o_insert((int) c.c)){		/* if we've got it, use it! */
	p = pscreen[ttrow]->v_text;	/* then clean up physical screen */
	for(i = term.t_ncol-1; i > ttcol; i--)
	  p[i] = p[i-1];		/* shift right */

	p[ttcol++] = c;			/* insert new char */
	
	return(1);
    }

    return(0);
}


/*
 * pdel - use optimized delete to rub out the current char and
 *        fix the physical screen array.
 *        returns true if optimized the delete, false otherwise
 */
pdel()
{
    register int   i;
    register CELL *c;

    if(delchar){			/* if we've got it, use it! */
	(*term.t_putchar)('\b'); 	/* move left a char */
	--ttcol;
	o_delete();			/* and delete it */

	c = pscreen[ttrow]->v_text;	/* then clean up physical screen */
	for(i=ttcol; i < term.t_ncol; i++)
	  c[i] = c[i+1];
	c[i].c = ' ';
	c[i].a = 0;
	
	return(1);
    }

    return(0);
}



/*
 * wstripe - write out the given string at the given location, and reverse
 *           video on flagged characters.  Does the same thing as pine's
 *           stripe.
 */
void
wstripe(line, column, pmt, key)
int	line, column;
char	*pmt;
int      key;
{
    register char *buf;
    register int  i = 0;
    register int  j = 0;
    register int  l;
    COLOR_PAIR *lastc = NULL;
    COLOR_PAIR *kncp = NULL;
    COLOR_PAIR *klcp = NULL;

    if (Pmaster && Pmaster->colors){
      if(pico_is_good_colorpair(Pmaster->colors->klcp))
        klcp = Pmaster->colors->klcp;

      if(klcp && pico_is_good_colorpair(Pmaster->colors->kncp))
        kncp = Pmaster->colors->kncp;

      lastc = pico_get_cur_color();
    }

    l = strlen(pmt);
    while(1){
	if(i >= term.t_ncol || j >= l)
	  return;				/* equal strings */

	if(pmt[j] == key)
	  j++;

	if (pscr(line, i) == NULL)
	  return;
	
	if(pscr(line, i)->c != pmt[j]){
	    if(j >= 1 && pmt[j-1] == key)
 	      j--;
	    break;
	}

	j++;
	i++;
    }

    movecursor(line, column+i);
    if(klcp) (void)pico_set_colorp(klcp, PSC_NONE);
    buf = &pmt[j];
    do{
	if(*buf == key){
	    buf++;
	    if(kncp)
	      (void)pico_set_colorp(kncp, PSC_NONE);
	    else
	      (void)(*term.t_rev)(1);

	    pputc(*buf, 1);
	    if(kncp)
	      (void)pico_set_colorp(klcp, PSC_NONE);
	    else
	      (void)(*term.t_rev)(0);
	}
	else{
	    pputc(*buf, 0);
	}
    }    
    while(*++buf != '\0');

    peeol();
    if (lastc){
      (void)pico_set_colorp(lastc, PSC_NONE);
      free_color_pair(&lastc);
    }
    (*term.t_flush)();
}



/*
 *  wkeyhelp - paint list of possible commands on the bottom
 *             of the display (yet another pine clone)
 *  NOTE: function key mode is handled here since all the labels
 *        are the same...
 */
void
wkeyhelp(keymenu)
KEYMENU *keymenu;
{
    char *obufp, *p, fkey[4];
    char  linebuf[2*NLINE];	/* "2" is for space for invert tokens */
    int   row, slot, tspace, adjusted_tspace, nspace[6], index, n;
#ifdef	MOUSE
    char  nbuf[NLINE];
#endif

#ifdef _WINDOWS
    pico_config_menu_items (keymenu);
#endif

    if(term.t_mrow == 0)
      return;

    /*
     * Calculate amount of space for the names column by column...
     */
    for(index = 0; index < 6; index++)
      if(!(gmode&MDFKEY)){
	  nspace[index] = (keymenu[index].name)
			    ? strlen(keymenu[index].name) : 0;
	  if(keymenu[index+6].name 
	     && (n = strlen(keymenu[index+6].name)) > nspace[index])
	    nspace[index] = n;

	  nspace[index]++;
      }
      else
	nspace[index] = (index < 4) ? 3 : 4;

    tspace = term.t_ncol/6;		/* total space for each item */
    /*
     * Avoid writing in bottom right corner so we won't scroll screens that
     * scroll when you do that. The way this is setup, we won't do that
     * unless the number of columns is evenly divisible by 6.
     */
    adjusted_tspace = (6 * tspace == term.t_ncol) ? tspace - 1 : tspace;

    index  = 0;
    for(row = 0; row <= 1; row++){
	linebuf[0] = '\0';
	obufp = &linebuf[0];
	for(slot = 0; slot < 6; slot++){
	    if(keymenu[index].name && keymenu[index].label){
		if(gmode&MDFKEY){
		    p = fkey;
		    sprintf(fkey, "F%d", (2 * slot) + row + 1);
		}
		else
		  p = keymenu[index].name;
#ifdef	MOUSE
		sprintf(nbuf, "%.*s %s", nspace[slot], p,
			keymenu[index].label);
		register_key(index,
			     (gmode&MDFKEY) ? F1 + (2 * slot) + row:
			     (keymenu[index].name[0] == '^')
			       ? (CTRL | keymenu[index].name[1])
			       : (keymenu[index].name[0] == 'S'
				  && !strcmp(keymenu[index].name, "Spc"))
				   ? ' '
				   : keymenu[index].name[0],
			     nbuf, invert_label,
			     term.t_nrow - 1 + row, (slot * tspace),
			     strlen(nbuf),
			     (Pmaster && Pmaster->colors) 
			       ? Pmaster->colors->kncp: NULL,
			     (Pmaster && Pmaster->colors) 
			       ? Pmaster->colors->klcp: NULL);
#endif

		n = nspace[slot];
		while(p && *p && n--){
		    *obufp++ = '~';	/* insert "invert" token */
		    *obufp++ = *p++;
		}

		while(n-- > 0)
		  *obufp++ = ' ';

		p = keymenu[index].label;
		n = ((slot == 5 && row == 1) ? adjusted_tspace
					     : tspace) - nspace[slot];
		while(p && *p && n-- > 0)
		  *obufp++ = *p++;

		while(n-- > 0)
		  *obufp++ = ' ';
	    }
	    else{
		n = (slot == 5 && row == 1) ? adjusted_tspace : tspace;
		while(n--)
		  *obufp++ = ' ';

#ifdef	MOUSE
		register_key(index, NODATA, "", NULL, 0, 0, 0, NULL, NULL);
#endif
	    }

	    *obufp = '\0';
	    index++;
	}

	wstripe(term.t_nrow - 1 + row, 0, linebuf, '~');
    }
}
