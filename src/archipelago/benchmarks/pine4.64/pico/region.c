#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: region.c 11688 2001-06-21 17:54:43Z hubert $";
#endif
/*
 * Program:	Region management routines
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
 * 1989-2001 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 *
 */
/*
 * The routines in this file
 * deal with the region, that magic space
 * between "." and mark. Some functions are
 * commands. Some functions are just for
 * internal use.
 */
#include	"headers.h"

/*
 * Kill the region. Ask "getregion"
 * to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 * Bound to "C-W".
 */
killregion(f, n)
    int f, n;
{
    REGION          region;

    if (curbp->b_mode&MDVIEW)		/* don't allow this command if	*/
      return(rdonly());			/* we are in read only mode	*/

    if (getregion(&region, curwp->w_markp, curwp->w_marko) != TRUE){
	return (killtext(f, n));
    }else {
	mlerase();
    }

    if ((lastflag&CFKILL) == 0)		/* This is a kill type  */
      kdelete();			/* command, so do magic */

    thisflag |= CFKILL;			/* kill buffer stuff.   */
    curwp->w_dotp = region.r_linep;
    curwp->w_doto = region.r_offset;
    curwp->w_markp = NULL;
#ifdef	_WINDOWS
    mswin_allowcopycut(NULL);
#endif

    if(ldelete(region.r_size, kinsert)){
	if(curwp->w_dotp == curwp->w_linep && curwp->w_dotp == curbp->b_linep){
	    curwp->w_force = 0;		/* Center dot. */
	    curwp->w_flag |= WFFORCE;
	}

	return(TRUE);
    }

    return (FALSE);
}


/*
 * Blast the region without saving . Ask "getregion"
 * to figure out the bounds of the region.
 * Move "." to the start, and kill the characters.
 * Bound to "C-W".
 */
deleteregion(f, n)
    int f, n;
{
    REGION          region;

    if (curbp->b_mode&MDVIEW)		/* don't allow this command if	*/
      return(rdonly());			/* we are in read only mode	*/

    if (getregion(&region, curwp->w_markp, curwp->w_marko) == TRUE){
	curwp->w_dotp = region.r_linep;
	curwp->w_doto = region.r_offset;
	curwp->w_markp = NULL;
#ifdef	_WINDOWS
	mswin_allowcopycut(NULL);
#endif
	if(ldelete(region.r_size, NULL)){
	    if(curwp->w_dotp == curwp->w_linep
	       && curwp->w_dotp == curbp->b_linep){
		curwp->w_force = 0;		/* Center dot. */
		curwp->w_flag |= WFFORCE;
	    }

	    return(TRUE);
	}
    }

    return (FALSE);
}


/*
 * Copy all of the characters in the
 * region to the kill buffer. Don't move dot
 * at all. This is a bit like a kill region followed
 * by a yank. Bound to "M-W".
 */
copyregion(f, n)
{
    register LINE   *linep;
    register int    loffs;
    register int    s;
    REGION          region;

    if ((s=getregion(&region, curwp->w_markp, curwp->w_marko)) != TRUE)
      return (s);

    if ((lastflag&CFKILL) == 0)		/* Kill type command.   */
      kdelete();

    thisflag |= CFKILL;
    linep = region.r_linep;		/* Current line.        */
    loffs = region.r_offset;		/* Current offset.      */
    while (region.r_size--) {
	if (loffs == llength(linep)) {  /* End of line.         */
	    if ((s=kinsert('\n')) != TRUE)
	      return (s);
	    linep = lforw(linep);
	    loffs = 0;
	} else {                        /* Middle of line.      */
	    if ((s=kinsert(lgetc(linep, loffs).c)) != TRUE)
	      return (s);
	    ++loffs;
	}
    }

    return (TRUE);
}


/*
 * Lower case region. Zap all of the upper
 * case characters in the region to lower case. Use
 * the region code to set the limits. Scan the buffer,
 * doing the changes. Call "lchange" to ensure that
 * redisplay is done in all buffers. Bound to
 * "C-X C-L".
 */
lowerregion(f, n)
{
    register LINE   *linep;
    register int    loffs;
    register int    c;
    register int    s;
    REGION          region;
    CELL            ac;

    ac.a = 0;
    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if ((s=getregion(&region, curwp->w_markp, curwp->w_marko)) != TRUE)
      return (s);

    lchange(WFHARD);
    linep = region.r_linep;
    loffs = region.r_offset;
    while (region.r_size--) {
	if (loffs == llength(linep)) {
	    linep = lforw(linep);
	    loffs = 0;
	} else {
	    c = lgetc(linep, loffs).c;
	    if (c>='A' && c<='Z'){
		ac.c = c+'a'-'A';
		lputc(linep, loffs, ac);
	    }
	    ++loffs;
	}
    }

    return (TRUE);
}

/*
 * Upper case region. Zap all of the lower
 * case characters in the region to upper case. Use
 * the region code to set the limits. Scan the buffer,
 * doing the changes. Call "lchange" to ensure that
 * redisplay is done in all buffers. Bound to
 * "C-X C-L".
 */
upperregion(f, n)
{
    register LINE   *linep;
    register int    loffs;
    register int    c;
    register int    s;
    REGION          region;
    CELL            ac;

    ac.a = 0;
    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if ((s=getregion(&region, curwp->w_markp, curwp->w_marko)) != TRUE)
      return (s);

    lchange(WFHARD);
    linep = region.r_linep;
    loffs = region.r_offset;
    while (region.r_size--) {
	if (loffs == llength(linep)) {
	    linep = lforw(linep);
	    loffs = 0;
	} else {
	    c = lgetc(linep, loffs).c;
	    if (c>='a' && c<='z'){
		ac.c = c - 'a' + 'A';
		lputc(linep, loffs, ac);
	    }
	    ++loffs;
	}
    }

    return (TRUE);
}

/*
 * This routine figures out the
 * bounds of the region in the current window, and
 * fills in the fields of the "REGION" structure pointed
 * to by "rp". Because the dot and mark are usually very
 * close together, we scan outward from dot looking for
 * mark. This should save time. Return a standard code.
 * Callers of this routine should be prepared to get
 * an "ABORT" status; we might make this have the
 * conform thing later.
 */
getregion(rp, markp, marko)
register REGION *rp;
register LINE	*markp;
register int	 marko;
{
    register LINE   *flp;
    register LINE   *blp;
    long	    fsize;
    register long   bsize;

    if (markp == NULL) {
	return (FALSE);
    }

    if (curwp->w_dotp == markp) {
	rp->r_linep = curwp->w_dotp;
	if (curwp->w_doto < marko) {
	    rp->r_offset = curwp->w_doto;
	    rp->r_size = marko - curwp->w_doto;
	} else {
	    rp->r_offset = marko;
	    rp->r_size = curwp->w_doto - marko;
	}
	return (TRUE);
    }

    blp = curwp->w_dotp;
    bsize = curwp->w_doto;
    flp = curwp->w_dotp;
    fsize = llength(flp)-curwp->w_doto+1;
    while (flp!=curbp->b_linep || lback(blp)!=curbp->b_linep) {
	if (flp != curbp->b_linep) {
	    flp = lforw(flp);
	    if (flp == markp) {
		rp->r_linep = curwp->w_dotp;
		rp->r_offset = curwp->w_doto;
		rp->r_size = fsize + marko;
		return (TRUE);
	    }

	    fsize += llength(flp) + 1;
	}

	if (lback(blp) != curbp->b_linep) {
	    blp = lback(blp);
	    bsize += llength(blp)+1;
	    if (blp == markp) {
		rp->r_linep = blp;
		rp->r_offset = marko;
		rp->r_size = bsize - marko;
		return (TRUE);
	    }
	}
    }

    emlwrite("Bug: lost mark", NULL);
    return (FALSE);
}


/*
 * set the highlight attribute accordingly on all characters in region
 */
markregion(attr)
    int attr;
{
    register LINE   *linep;
    register int    loffs;
    register int    s;
    REGION          region;
    CELL            ac;

    if ((s=getregion(&region, curwp->w_markp, curwp->w_marko)) != TRUE)
      return (s);

    lchange(WFHARD);
    linep = region.r_linep;
    loffs = region.r_offset;
    while (region.r_size--) {
	if (loffs == llength(linep)) {
	    linep = lforw(linep);
	    loffs = 0;
	} else {
	    ac = lgetc(linep, loffs);
	    ac.a = attr;
	    lputc(linep, loffs, ac);
	    ++loffs;
	}
    }

    return (TRUE);
}


/*
 * clear all the attributes of all the characters in the buffer?
 * this is real dumb.  Movement with mark set needs to be smarter!
 */
void
unmarkbuffer()
{
    register LINE   *linep;
    register int     n;
    CELL c;

    linep = curwp->w_linep;
    while(lforw(linep) != curwp->w_linep){
	n = llength(linep);
	for(n=0; n < llength(linep); n++){
	    c = lgetc(linep, n);
	    c.a = 0;
	    lputc(linep, n, c);
	}

	linep = lforw(linep);
    }
}
