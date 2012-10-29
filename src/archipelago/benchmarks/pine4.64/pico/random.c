#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: random.c 13654 2004-05-07 21:43:40Z jpf $";
#endif
/*
 * Program:	Random routines
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
 * This file contains the command processing functions for a number of random
 * commands. There is no functional grouping here, for sure.
 */

#include	"headers.h"

#ifdef	ANSI
    int getccol(int);
#else
    int getccol();
#endif

int     tabsize;                        /* Tab size (0: use real tabs)  */


/*
 * Display the current position of the cursor, in origin 1 X-Y coordinates,
 * the character that is under the cursor (in octal), and the fraction of the
 * text that is before the cursor. The displayed column is not the current
 * column, but the column that would be used on an infinite width display.
 * Normally this is bound to "C-X =".
 */
showcpos(f, n)
int f, n;
{
    register LINE   *clp;
    register long   nch;
    register int    cbo;
    register long   nbc;
    register int    lines;
    register int    thisline;
    char     buffer[80];

    clp = lforw(curbp->b_linep);            /* Grovel the data.     */
    cbo = 0;
    nch = 0L;
    lines = 0;
    for (;;) {
	if (clp==curwp->w_dotp && cbo==curwp->w_doto) {
	    thisline = lines;
	    nbc = nch;
	}
	if (cbo == llength(clp)) {
	    if (clp == curbp->b_linep)
	      break;
	    clp = lforw(clp);
	    cbo = 0;
	    lines++;
	} else
	  ++cbo;
	++nch;
    }

    sprintf(buffer,"line %d of %d (%d%%%%), character %ld of %ld (%d%%%%)",
	    thisline+1, lines+1, (int)((100L*(thisline+1))/(lines+1)),
	    nbc, nch, (nch) ? (int)((100L*nbc)/nch) : 0);

    emlwrite(buffer, NULL);
    return (TRUE);
}


/*
 * Return current column.  Stop at first non-blank given TRUE argument.
 */
getccol(bflg)
int bflg;
{
    register int c, i, col;

    col = 0;
    for (i=0; i<curwp->w_doto; ++i) {
	c = lgetc(curwp->w_dotp, i).c;
	if (c!=' ' && c!='\t' && bflg)
	  break;

	if (c == '\t')
	  col |= 0x07;
	else if (c<0x20 || c==0x7F)
	  ++col;

	++col;
    }

    return(col);
}



/*
 * Set tab size if given non-default argument (n <> 1).  Otherwise, insert a
 * tab into file.  If given argument, n, of zero, change to true tabs.
 * If n > 1, simulate tab stop every n-characters using spaces. This has to be
 * done in this slightly funny way because the tab (in ASCII) has been turned
 * into "C-I" (in 10 bit code) already. Bound to "C-I".
 */
tab(f, n)
    int f, n;
{
    if (n < 0)
      return (FALSE);

    if (n == 0 || n > 1) {
	tabsize = n;
	return(TRUE);
    }

    if (! tabsize)
      return(linsert(1, '\t'));

    return(linsert(tabsize - (getccol(FALSE) % tabsize), ' '));
}


/*
 * Insert a newline. Bound to "C-M".
 */
newline(f, n)
    int f, n;
{
    register int    s;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (FALSE);

    if(optimize && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l)){
	    if(curwp->w_doto != 0)
	      l++;
	    scrolldown(curwp, l, n);
	}
    }

    /* if we are in C mode and this is a default <NL> */
    /* pico's never in C mode */

    if(Pmaster && Pmaster->allow_flowed_text && curwp->w_doto
       && isspace(lgetc(curwp->w_dotp, curwp->w_doto - 1).c)
       && !(curwp->w_doto == 3
	    && lgetc(curwp->w_dotp, 0).c == '-'
	    && lgetc(curwp->w_dotp, 1).c == '-'
	    && lgetc(curwp->w_dotp, 2).c == ' ')){
	/*
	 * flowed mode, make the newline a hard one by
	 * stripping trailing space.
	 */
	int i, dellen;
	for(i = curwp->w_doto - 1;
	    i && isspace(lgetc(curwp->w_dotp, i - 1).c);
	    i--);
	dellen = curwp->w_doto - i;
	curwp->w_doto = i;
	ldelete(dellen, NULL);
    }
    /* insert some lines */
    while (n--) {
	if ((s=lnewline()) != TRUE)
	  return (s);
    }
    return (TRUE);
}



/*
 * Delete forward. This is real easy, because the basic delete routine does
 * all of the work. Watches for negative arguments, and does the right thing.
 * If any argument is present, it kills rather than deletes, to prevent loss
 * of text if typed with a big argument. Normally bound to "C-D".
 */
forwdel(f, n)
    int f, n;
{
    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (backdel(f, -n));

    if(optimize && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l) && curwp->w_doto == llength(curwp->w_dotp))
	  scrollup(curwp, l+1, 1);
    }

    if (f != FALSE) {                       /* Really a kill.       */
	if ((lastflag&CFKILL) == 0)
	  kdelete();
	thisflag |= CFKILL;
    }

    return (ldelete((long) n, f ? kinsert : NULL));
}



/*
 * Delete backwards. This is quite easy too, because it's all done with other
 * functions. Just move the cursor back, and delete forwards. Like delete
 * forward, this actually does a kill if presented with an argument. Bound to
 * both "RUBOUT" and "C-H".
 */
backdel(f, n)
    int f, n;
{
    register int    s;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (forwdel(f, -n));

    if(optimize && curwp->w_dotp != curwp->w_bufp->b_linep){
	int l;
	
	if(worthit(&l) && curwp->w_doto == 0 &&
	   lback(curwp->w_dotp) != curwp->w_bufp->b_linep){
	    if(l == curwp->w_toprow)
	      scrollup(curwp, l+1, 1);
	    else if(llength(lback(curwp->w_dotp)) == 0)
	      scrollup(curwp, l-1, 1);
	    else
	      scrollup(curwp, l, 1);
	}
    }

    if (f != FALSE) {                       /* Really a kill.       */
	if ((lastflag&CFKILL) == 0)
	  kdelete();

	thisflag |= CFKILL;
    }

    if ((s=backchar(f, n)) == TRUE)
      s = ldelete((long) n, f ? kinsert : NULL);

    return (s);
}



/*
 * killtext - delete the line that the cursor is currently in.
 *	      a greatly pared down version of its former self.
 */
killtext(f, n)
int f, n;
{
    register int chunk;
    int		 opt_scroll = 0;

    if (curbp->b_mode&MDVIEW)		/* don't allow this command if  */
      return(rdonly());			/* we are in read only mode     */

    if ((lastflag&CFKILL) == 0)		/* Clear kill buffer if */
      kdelete();			/* last wasn't a kill.  */

    if(gmode & MDDTKILL){		/*  */
	if((chunk = llength(curwp->w_dotp) - curwp->w_doto) == 0){
	    chunk = 1;
	    if(optimize)
	      opt_scroll = 1;
	}
    }
    else{
	gotobol(FALSE, 1);		/* wack from bol past newline */
	chunk = llength(curwp->w_dotp) + 1;
	if(optimize)
	  opt_scroll = 1;
    }

    /* optimize what motion we can */
    if(opt_scroll && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l))
	  scrollup(curwp, l, 1);
    }

    thisflag |= CFKILL;
    return(ldelete((long) chunk, kinsert));
}


/*
 * Yank text back from the kill buffer. This is really easy. All of the work
 * is done by the standard insert routines. All you do is run the loop, and
 * check for errors. Bound to "C-Y".
 */
yank(f, n)
int f, n;
{
    register int    c;
    register int    i;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (FALSE);

    if(optimize && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l) && !(lastflag&CFFILL)){
	    register int  t = 0; 
	    register int  i = 0;
	    register int  ch;

	    while((ch=fremove(i++)) >= 0)
	      if(ch == '\n')
		t++;
	    if(t+l < curwp->w_toprow+curwp->w_ntrows)
	      scrolldown(curwp, l, t);
	}
    }

    if(lastflag & CFFILL){		/* if last command was fillpara() */
	REGION region;
	LINE *dotp;

	if(lastflag & CFFLBF){
	    gotoeob(FALSE, 1);
	    dotp = curwp->w_dotp;
	    gotobob(FALSE, 1);
	}
	else{
	    backchar(FALSE, 1);
	    dotp = curwp->w_dotp;
	    gotobop(FALSE, 1);		/* then go to the top of the para */
	}

	curwp->w_doto = 0;
	getregion(&region, dotp, llength(dotp));
	if(!ldelete(region.r_size, NULL))
	  return(FALSE);
    }					/* then splat out the saved buffer */

    while (n--) {
	i = 0;
	while ((c = ((lastflag&CFFILL)
		     ? ((lastflag & CFFLBF) ? kremove(i) : fremove(i))
		     : kremove(i))) >= 0) {
	    if (c == '\n') {
		if (lnewline() == FALSE)
		  return (FALSE);
	    } else {
		if (linsert(1, c) == FALSE)
		  return (FALSE);
	    }

	    ++i;
	}
    }

    if(lastflag&CFFILL){            /* if last command was fillpara() */
	curwp->w_dotp = lforw(curwp->w_dotp);
	curwp->w_doto = 0;

	curwp->w_flag |= WFMODE;
	
	if(!Pmaster){
	    sgarbk = TRUE;
	    emlwrite("", NULL);
	}
    }

    return (TRUE);
}


/*
 * Original idea from Stephen Casner <casner@acm.org>.
 *
 * Apply the appropriate reverse color transformation to the given
 * color pair and return a new color pair. The caller should free the
 * color pair.
 *
 */
COLOR_PAIR *
pico_apply_rev_color(cp, style)
    COLOR_PAIR *cp;
    int         style;
{
    COLOR_PAIR *rc = pico_get_rev_color();

    if(rc){
	if(style == IND_COL_REV){
	    /* just use Reverse color regardless */
	    return(new_color_pair(rc->fg, rc->bg));
	}
	else if(style == IND_COL_FG){
	    /*
	     * If changing to Rev fg is readable and different
	     * from what it already is, do it.
	     */
	    if(strcmp(rc->fg, cp->bg) && strcmp(rc->fg, cp->fg))
	      return(new_color_pair(rc->fg, cp->bg));
	}
	else if(style == IND_COL_BG){
	    /*
	     * If changing to Rev bg is readable and different
	     * from what it already is, do it.
	     */
	    if(strcmp(rc->bg, cp->fg) && strcmp(rc->bg, cp->bg))
	      return(new_color_pair(cp->fg, rc->bg));
	}
	else if(style == IND_COL_FG_NOAMBIG){
	    /*
	     * If changing to Rev fg is readable, different
	     * from what it already is, and not the same as
	     * the Rev color, do it.
	     */
	    if(strcmp(rc->fg, cp->bg) && strcmp(rc->fg, cp->fg) &&
	       strcmp(rc->bg, cp->bg))
	      return(new_color_pair(rc->fg, cp->bg));
	}
	else if(style == IND_COL_BG_NOAMBIG){
	    /*
	     * If changing to Rev bg is readable, different
	     * from what it already is, and not the same as
	     * the Rev color, do it.
	     */
	    if(strcmp(rc->bg, cp->fg) && strcmp(rc->bg, cp->bg) &&
	       strcmp(rc->fg, cp->fg))
	      return(new_color_pair(cp->fg, rc->bg));
	}
    }

    /* come here for IND_COL_FLIP and for the cases which fail the tests  */
    return(new_color_pair(cp->bg, cp->fg));	/* flip the colors */
}
