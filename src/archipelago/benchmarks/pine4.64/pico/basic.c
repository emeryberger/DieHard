#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: basic.c 13654 2004-05-07 21:43:40Z jpf $";
#endif
/*
 * Program:	Cursor manipulation functions
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
 * The routines in this file move the cursor around on the screen. They
 * compute a new value for the cursor, then adjust ".". The display code
 * always updates the cursor location, so only moves between lines, or
 * functions that adjust the top line in the window and invalidate the
 * framing, are hard.
 */
#include        "headers.h"

#ifdef	ANSI
    int getgoal(struct LINE *);
#else
    int getgoal();
#endif


/*
 * Move the cursor to the
 * beginning of the current line.
 * Trivial.
 */
gotobol(f, n)
int f, n;
{
    curwp->w_doto  = 0;
    return (TRUE);
}

/*
 * Move the cursor backwards by "n" characters. If "n" is less than zero call
 * "forwchar" to actually do the move. Otherwise compute the new cursor
 * location. Error if you try and move out of the buffer. Set the flag if the
 * line pointer for dot changes.
 */
backchar(f, n)
int             f;
register int    n;
{
    register LINE   *lp;

    if (n < 0)
      return (forwchar(f, -n));

    while (n--) {
	if (curwp->w_doto == 0) {
	    if ((lp=lback(curwp->w_dotp)) == curbp->b_linep){
		if(Pmaster && Pmaster->headents)
		    /*
		     * go up into editing the mail header if on 
		     * the top line and the user hits the left arrow!!!
		     *
		     * if the editor returns anything except -1, the 
		     * user requested something special, so let 
		     * pico know...
		     */
		  return(HeaderEditor(2, 1));
		else
		  return (FALSE);
	    }

	    curwp->w_dotp  = lp;
	    curwp->w_doto  = llength(lp);
	    curwp->w_flag |= WFMOVE;
	} else
	  curwp->w_doto--;
    }

    return (TRUE);
}


/*
 * Move the cursor to the end of the current line. Trivial. No errors.
 */
gotoeol(f, n)
int f, n;
{
    curwp->w_doto  = llength(curwp->w_dotp);
    return (TRUE);
}


/*
 * Move the cursor forwwards by "n" characters. If "n" is less than zero call
 * "backchar" to actually do the move. Otherwise compute the new cursor
 * location, and move ".". Error if you try and move off the end of the
 * buffer. Set the flag if the line pointer for dot changes.
 */
forwchar(f, n)
int             f;
register int    n;
{
    if (n < 0)
      return (backchar(f, -n));

    while (n--) {
	if (curwp->w_doto == llength(curwp->w_dotp)) {
	    if (curwp->w_dotp == curbp->b_linep)
	      return (FALSE);

	    curwp->w_dotp  = lforw(curwp->w_dotp);
	    curwp->w_doto  = 0;
	    curwp->w_flag |= WFMOVE;
	}
	else
	  curwp->w_doto++;
    }
    
    return (TRUE);
}


/*
 * move to a particular line.
 * argument (n) must be a positive integer for
 * this to actually do anything
 */
gotoline(f, n)
int f, n;
{
    if (n < 1)		/* if a bogus argument...then leave */
      return(FALSE);

    /* first, we go to the start of the buffer */
    curwp->w_dotp  = lforw(curbp->b_linep);
    curwp->w_doto  = 0;
    return(forwline(f, n-1));
}


/*
 * Goto the beginning of the buffer. Massive adjustment of dot. This is
 * considered to be hard motion; it really isn't if the original value of dot
 * is the same as the new value of dot. Normally bound to "M-<".
 */
gotobob(f, n)
int f, n;
{
    curwp->w_dotp  = lforw(curbp->b_linep);
    curwp->w_doto  = 0;
    curwp->w_flag |= WFHARD;
    return (TRUE);
}


/*
 * Move to the end of the buffer. Dot is always put at the end of the file
 * (ZJ). The standard screen code does most of the hard parts of update.
 * Bound to "M->".
 */
gotoeob(f, n)
int f, n;
{
    curwp->w_dotp  = curbp->b_linep;
    curwp->w_doto  = 0;
    curwp->w_flag |= WFHARD;
    return (TRUE);
}


/*
 * Move forward by full lines. If the number of lines to move is less than
 * zero, call the backward line function to actually do it. The last command
 * controls how the goal column is set. Bound to "C-N". No errors are
 * possible.
 */
forwline(f, n)
int f, n;
{
    register LINE   *dlp;

    if (n < 0)
      return (backline(f, -n));

    if ((lastflag&CFCPCN) == 0)             /* Reset goal if last   */
      curgoal = getccol(FALSE);       /* not C-P or C-N       */

    thisflag |= CFCPCN;
    dlp = curwp->w_dotp;
    while (n-- && dlp!=curbp->b_linep)
      dlp = lforw(dlp);

    curwp->w_dotp  = dlp;
    curwp->w_doto  = getgoal(dlp);
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}


/*
 * This function is like "forwline", but goes backwards. The scheme is exactly
 * the same. Check for arguments that are less than zero and call your
 * alternate. Figure out the new line and call "movedot" to perform the
 * motion. No errors are possible. Bound to "C-P".
 */
backline(f, n)
int f, n;
{
    register LINE   *dlp;

    if (n < 0)
      return (forwline(f, -n));

    if(Pmaster && Pmaster->headents){
	/*
	 * go up into editing the mail header if on the top line
	 * and the user hits the up arrow!!!
	 */
	if (lback(curwp->w_dotp) == curbp->b_linep)
	  /*
	   * if the editor returns anything except -1 then the user
	   * has requested something special, so let pico know...
	   */
	  return(HeaderEditor(1, 1));
    }

    if ((lastflag&CFCPCN) == 0)             /* Reset goal if the    */
      curgoal = getccol(FALSE);       /* last isn't C-P, C-N  */

    thisflag |= CFCPCN;
    dlp = curwp->w_dotp;
    while (n-- && lback(dlp)!=curbp->b_linep)
      dlp = lback(dlp);

    curwp->w_dotp  = dlp;
    curwp->w_doto  = getgoal(dlp);
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}


/*
 * go back to the begining of the current paragraph
 * here we look for a <NL><NL> or <NL><TAB> or <NL><SPACE>
 * combination to delimit the begining of a paragraph	
 */
gotobop(f, n)
int f, n;	/* default Flag & Numeric argument */
{
    int quoted, qlen;
    char qstr[NLINE], qstr2[NLINE];

    if (n < 0)	/* the other way...*/
      return(gotoeop(f, -n));

    while (n-- > 0) {	/* for each one asked for */

	while(lisblank(curwp->w_dotp)
	      && lback(curwp->w_dotp) != curbp->b_linep){
	    curwp->w_dotp = lback(curwp->w_dotp);
	    curwp->w_doto = 0;
	}
	
	/* scan line by line until we come to a line ending with
	 * a <NL><NL> or <NL><TAB> or <NL><SPACE>
	 *
	 * PLUS: if there's a quote string, a quoted-to-non-quoted
	 *	 line transition.
	 */
	quoted = (glo_quote_str || (Pmaster && Pmaster->quote_str))
	  ? quote_match(glo_quote_str ? glo_quote_str : Pmaster->quote_str,
			curwp->w_dotp, qstr, NLINE) : 0;
	qlen   = quoted ? strlen(qstr) : 0;
	while(lback(curwp->w_dotp) != curbp->b_linep
	      && llength(lback(curwp->w_dotp)) > qlen
	      && ((glo_quote_str || (Pmaster && Pmaster->quote_str))
		  ? (quoted == quote_match(glo_quote_str ? glo_quote_str
					   : Pmaster->quote_str, 
					   lback(curwp->w_dotp),
					   qstr2, NLINE)
		     && !strcmp(qstr, qstr2))
		  : 1)
	      && lgetc(curwp->w_dotp, qlen).c != TAB
	      && lgetc(curwp->w_dotp, qlen).c != ' ')
	  curwp->w_dotp = lback(curwp->w_dotp);

	if(n){
	    /* keep looking */
	    if(lback(curwp->w_dotp) == curbp->b_linep)
	      break;
	    else
	      curwp->w_dotp = lback(curwp->w_dotp);

	    curwp->w_doto = 0;
	}
	else{
	  /* leave cursor on first word in para */
	    curwp->w_doto = 0;
	    while(isspace((unsigned char)lgetc(curwp->w_dotp, curwp->w_doto).c))
	      if(++curwp->w_doto >= llength(curwp->w_dotp)){
		  curwp->w_doto = 0;
		  curwp->w_dotp = lforw(curwp->w_dotp);
		  if(curwp->w_dotp == curbp->b_linep)
		    break;
	      }
	}
    }

    curwp->w_flag |= WFMOVE;	/* force screen update */
    return(TRUE);
}


/* 
 * go forword to the end of the current paragraph
 * here we look for a <NL><NL> or <NL><TAB> or <NL><SPACE>
 * combination to delimit the begining of a paragraph
 */
gotoeop(f, n)
int f, n;	/* default Flag & Numeric argument */

{
    int quoted, qlen;
    char qstr[NLINE], qstr2[NLINE];

    if (n < 0)	/* the other way...*/
      return(gotobop(f, -n));

    while (n-- > 0) {	/* for each one asked for */

	while(lisblank(curwp->w_dotp)){
	    curwp->w_doto = 0;
	    if((curwp->w_dotp = lforw(curwp->w_dotp)) == curbp->b_linep)
	      break;
	}

	/* scan line by line until we come to a line ending with
	 * a <NL><NL> or <NL><TAB> or <NL><SPACE>
	 *
	 * PLUS: if there's a quote string, a quoted-to-non-quoted
	 *	 line transition.
	 */
	quoted = ((glo_quote_str || (Pmaster && Pmaster->quote_str))
	  ? quote_match(glo_quote_str ? glo_quote_str : Pmaster->quote_str,
			curwp->w_dotp, qstr, NLINE) : 0);
	qlen   = quoted ? strlen(qstr) : 0;
	
	while(curwp->w_dotp != curbp->b_linep
	      && llength(lforw(curwp->w_dotp)) > qlen
	      && ((glo_quote_str || (Pmaster && Pmaster->quote_str))
		  ? (quoted == quote_match(glo_quote_str ? glo_quote_str
					   : Pmaster->quote_str,
					   lforw(curwp->w_dotp),
					   qstr2, NLINE)
		     && !strcmp(qstr, qstr2))
		  : 1)
	      && lgetc(lforw(curwp->w_dotp), qlen).c != TAB
	      && lgetc(lforw(curwp->w_dotp), qlen).c != ' ')
	  curwp->w_dotp = lforw(curwp->w_dotp);

	curwp->w_doto = llength(curwp->w_dotp);

	/* still looking? */
	if(n){
	    if(curwp->w_dotp == curbp->b_linep)
	      break;
	    else
	      curwp->w_dotp = lforw(curwp->w_dotp);

	    curwp->w_doto = 0;
	}
    }

    curwp->w_flag |= WFMOVE;	/* force screen update */
    return(curwp->w_dotp != curbp->b_linep);
}

/*
 * This routine, given a pointer to a LINE, and the current cursor goal
 * column, return the best choice for the offset. The offset is returned.
 * Used by "C-N" and "C-P".
 */
getgoal(dlp)
register LINE   *dlp;
{
    register int    c;
    register int    col;
    register int    newcol;
    register int    dbo;

    col = 0;
    dbo = 0;
    while (dbo != llength(dlp)) {
	c = lgetc(dlp, dbo).c;
	newcol = col;
	if (c == '\t')
	  newcol |= 0x07;
	else if (c<0x20 || c==0x7F)
	  ++newcol;

	++newcol;
	if (newcol > curgoal)
	  break;

	col = newcol;
	++dbo;
    }

    return (dbo);
}



/*
 * Scroll the display forward (up) n lines.
 */
scrollforw (n, movedot)
register int	n;
int		movedot;
{
    register LINE   *lp;
    LINE	    *lp2;
    register int    nl;
    int		    i;

    nl = n;
    lp = curwp->w_linep;
    while (n-- && lp!=curbp->b_linep)
      lp = lforw(lp);

    if (movedot) {			/* Move dot to top of page. */
	curwp->w_dotp  = lp;
	curwp->w_doto  = 0;
    }

    curwp->w_flag |= WFHARD;
    if(lp == curbp->b_linep)
      return(TRUE);
    else
      curwp->w_linep = lp;

    /*
     * if the header is open, close it ...
     */
    if(Pmaster && Pmaster->headents && ComposerTopLine != COMPOSER_TOP_LINE){
	n -= ComposerTopLine - COMPOSER_TOP_LINE;
	ToggleHeader(0);
    }

    /*
     * scroll down from the top the same number of lines we've moved 
     * forward
     */
    if(optimize)
      scrollup(curwp, -1, nl-n-1);

    if(!movedot){
	/* Requested to not move the dot.  Look for the dot in the current
	 * window.  loop through all lines, stop when at end of window
	 * or endof buffer.  If the dot is found, it can stay where it
	 * is, otherwise we do need to move it.
	 */
	movedot = TRUE;
	for (	lp2 = lp, i = 0; 
		lp2 != curbp->b_linep && i < curwp->w_ntrows;  
		lp2 = lforw(lp2), ++i) {
	    if (curwp->w_dotp == lp2) {
		 movedot = FALSE;
		 break;
	    }
        }
	if (movedot) {
	    /* Dot not found in window.  Move to first line of window. */
	    curwp->w_dotp  = lp;
	    curwp->w_doto  = 0;
        }
    }

    return (TRUE);
}


/*
 * Scroll forward by a specified number of lines, or by a full page if no
 * argument. Bound to "C-V". The "2" in the arithmetic on the window size is
 * the overlap; this value is the default overlap value in ITS EMACS. Because
 * this zaps the top line in the display window, we have to do a hard update.
 */
forwpage(f, n)
int             f;
register int    n;
{

    if (f == FALSE) {
	n = curwp->w_ntrows - 2;        /* Default scroll.      */
	if (n <= 0)                     /* Forget the overlap   */
	  n = 1;                  /* if tiny window.      */
    } else if (n < 0)
      return (backpage(f, -n));
#if     CVMVAS
    else                                    /* Convert from pages   */
      n *= curwp->w_ntrows;           /* to lines.            */
#endif
    return (scrollforw (n, TRUE));
}


/*
 * Scroll back (down) number of lines.  
 */
scrollback (n, movedot)
register int	n;
int		movedot;
{
    register LINE   *lp, *tp;
    register int    nl;
    int		    i;

    if(Pmaster && Pmaster->headents){
	/*
	 * go up into editing the mail header if on the top line
	 * and the user hits the up arrow!!!
	 */
	if (lback(curwp->w_dotp) == curbp->b_linep){
	    /*
	     * if the editor returns anything except -1 then the user
	     * has requested something special, so let pico know...
	     */
	    return(HeaderEditor(1, 1));
	}
    }

    /*
     * Count back the number of lines requested.
     */
    nl = n;
    lp = curwp->w_linep;
    while (n-- && lback(lp)!=curbp->b_linep)
      lp = lback(lp);

    curwp->w_linep = lp;
    curwp->w_flag |= WFHARD;

    /*
     * scroll down from the top the same number of lines we've moved 
     * forward
     *
     * This isn't too cool, but it has to be this way so we can 
     * gracefully scroll in the message header
     */
    if(Pmaster && Pmaster->headents){
	if((lback(lp)==curbp->b_linep) && (ComposerTopLine==COMPOSER_TOP_LINE))
	  n -= entry_line(1000, TRUE); /* never more than 1000 headers */
	if(nl-n-1 < curwp->w_ntrows)
	  if(optimize)
	    scrolldown(curwp, -1, nl-n-1);
    }
    else
      if(optimize)
	scrolldown(curwp, -1, nl-n-1);

    if(Pmaster && Pmaster->headents){
	/*
	 * if we're at the top of the page, and the header is closed, 
	 * open it ...
	 */
	if((lback(lp) == curbp->b_linep) 
	   && (ComposerTopLine == COMPOSER_TOP_LINE)){
	    ToggleHeader(1);
	    movecursor(ComposerTopLine, 0);
	}
    }
    
    /*
     * Decide if we move the dot or not.  Calculation done AFTER deciding
     * if we display the header because that will change the number of
     * lines on the screen.
     */
    if (movedot) {
	/* Dot gets put at top of window. */
	curwp->w_dotp  = curwp->w_linep;
	curwp->w_doto  = 0;
    }
    else {
	/* Requested not to move dot, but we do need to keep in on
	 * the screen.  Verify that it is still in the range of lines
	 * visable in the window.  Loop from the first line to the
	 * last line, until we reach the end of the buffer or the end
	 * of the window.  If we find the dot, then we don't need
	 * to move it. */
	movedot = TRUE;
	for (	tp = curwp->w_linep, i = 0; 
		tp != curbp->b_linep && i < curwp->w_ntrows;  
		tp = lforw(tp), ++i) {
	    if (curwp->w_dotp == tp) {
		 movedot = FALSE;
		 break;
	    }
        }
	if (movedot) {
	    /* Dot not found in window.  Move to last line of window. */
	    curwp->w_dotp  = lback (tp);
	    curwp->w_doto  = 0;
        }
    }

    return (TRUE);
}




/*
 * This command is like "forwpage", but it goes backwards. The "2", like
 * above, is the overlap between the two windows. The value is from the ITS
 * EMACS manual. Bound to "M-V". We do a hard update for exactly the same
 * reason.
 */
backpage(f, n)
int             f;
register int    n;
{

    if (f == FALSE) {
	n = curwp->w_ntrows - 2;        /* Default scroll.      */
	if (n <= 0)                     /* Don't blow up if the */
	  n = 1;                  /* window is tiny.      */
    } else if (n < 0)
      return (forwpage(f, -n));
#if     CVMVAS
    else                                    /* Convert from pages   */
      n *= curwp->w_ntrows;           /* to lines.            */
#endif
    return (scrollback (n, TRUE));
}



scrollupline (f, n)
int f, n;
{
    return (scrollback (1, FALSE));
}


scrolldownline (f, n)
int f, n;
{
    return (scrollforw (1, FALSE));
}



/*
 * Scroll to a position.
 */
scrollto (f, n)
int f, n;
{
#ifdef _WINDOWS
    long	scrollLine;
    LINE	*lp;
    int		i;
    
    scrollLine = mswin_getscrollto ();
    
    /*
     * Starting at the first data line in the buffer, step forward
     * 'scrollLine' lines to find the new top line.  It is a circular
     * list of buffers, so watch for the first line to reappear.  if
     * it does, we have some sort of internal error, abort scroll
     * operation.  Also watch for NULL, just in case.
     */
    lp = lforw (curbp->b_linep);
    for (i = 0; i < scrollLine && lp != curbp->b_linep && lp != NULL; ++i)
	lp = lforw(lp);

    if (lp == curbp->b_linep || lp == NULL)
	return (FALSE);					/* Whoops! */
    

    /* Set the new top line for the window and flag a redraw. */
    curwp->w_linep = lp;
    curwp->w_dotp  = lp;
    curwp->w_doto  = 0;
    curwp->w_flag |= WFHARD;
    
    if(Pmaster && Pmaster->headents){
	/*
	 * If we are at the top of the page and header not open, open it.
	 * If we are not at the top of the page and the header is open,
	 * close it.
	 */
	if((lback(lp) == curbp->b_linep) 
	   && (ComposerTopLine == COMPOSER_TOP_LINE)){
	    ToggleHeader(1);
	    movecursor(ComposerTopLine, 0);
	}
	else if((lback(lp) != curbp->b_linep) 
	   && (ComposerTopLine != COMPOSER_TOP_LINE)){
	   ToggleHeader (0);
        }
    }
#endif

    return (TRUE);
}



/*
 * Set the mark in the current window to the value of "." in the window. No
 * errors are possible. Bound to "M-.".  If told to set an already set mark
 * unset it.
 */
setmark(f, n)
int f, n;
{
    if(!curwp->w_markp){
        curwp->w_markp = curwp->w_dotp;
        curwp->w_marko = curwp->w_doto;
	emlwrite("Mark Set", NULL);
    }
    else{
	/* clear inverse chars between here and dot */
	markregion(0);
	curwp->w_markp = NULL;
	emlwrite("Mark UNset", NULL);
    }

#ifdef	_WINDOWS
    mswin_allowcopycut(curwp->w_markp ? kremove : NULL);
#endif
    return (TRUE);
}


/*
 * Swap the values of "." and "mark" in the current window. This is pretty
 * easy, bacause all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible error is "no mark". Bound to
 * "C-X C-X".
 */
swapmark(f, n)
int f, n;
{
    register LINE   *odotp;
    register int    odoto;

    if (curwp->w_markp == NULL) {
	if(Pmaster == NULL)
	  emlwrite("No mark in this window", NULL);
	return (FALSE);
    }

    odotp = curwp->w_dotp;
    odoto = curwp->w_doto;
    curwp->w_dotp  = curwp->w_markp;
    curwp->w_doto  = curwp->w_marko;
    curwp->w_markp = odotp;
    curwp->w_marko = odoto;
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}


/*
 * Set the mark in the current window to the value of "." in the window. No
 * errors are possible. Bound to "M-.".  If told to set an already set mark
 * unset it.
 */
setimark(f, n)
int f, n;
{
    curwp->w_imarkp = curwp->w_dotp;
    curwp->w_imarko = curwp->w_doto;
    return(TRUE);
}


/*
 * Swap the values of "." and "mark" in the current window. This is pretty
 * easy, bacause all of the hard work gets done by the standard routine
 * that moves the mark about. The only possible error is "no mark". Bound to
 * "C-X C-X".
 */
swapimark(f, n)
int f, n;
{
    register LINE   *odotp;
    register int    odoto;

    if (curwp->w_imarkp == NULL) {
	if(Pmaster == NULL)
	  emlwrite("Programmer botch! No mark in this window", NULL);
	return (FALSE);
    }

    odotp = curwp->w_dotp;
    odoto = curwp->w_doto;
    curwp->w_dotp  = curwp->w_imarkp;
    curwp->w_doto  = curwp->w_imarko;
    curwp->w_imarkp = odotp;
    curwp->w_imarko = odoto;
    curwp->w_flag |= WFMOVE;
    return (TRUE);
}



#ifdef MOUSE

/*
 * Handle a mouse down.
 */
mousepress (f, n)
int f, n;
{
    MOUSEPRESS	mp;
    LINE	*lp;
    int    i;


    mouse_get_last (NULL, &mp);


    lp = curwp->w_linep;
    i = mp.row - ((Pmaster && Pmaster->headents) ? ComposerTopLine : 2);
    if (i < 0) {
	if (Pmaster) {
	    /* Clear existing region. */
	    if (curwp->w_markp)
		setmark(0,1);	

	    /* Move to top of document before editing header. */
	    curwp->w_dotp = curwp->w_linep;
	    curwp->w_doto = 0;
	    curwp->w_flag |= WFMOVE;
	    update ();				/* And update. */

	    return (HeaderEditor (1, 1));
        }
    }
    else {
	while(i-- && lp != curbp->b_linep)
	  lp = lforw(lp);

	curgoal = mp.col;
	curwp->w_dotp = lp;
	curwp->w_doto = getgoal(lp);
	curwp->w_flag |= WFMOVE;

	if(mp.doubleclick)
	    setmark(0, 1);
    }

    return(FALSE);
}
#endif
