#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: buffer.c 11688 2001-06-21 17:54:43Z hubert $";
#endif
/*
 * Program:	Buffer management routines
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
 * Buffer management.
 * Some of the functions are internal,
 * and some are actually attached to user
 * keys. Like everyone else, they set hints
 * for the display system.
 */
#include	"headers.h"

#ifdef	ANSI
int sgetline(char **, int *, char *, int);
#else
int sgetline();
#endif


/*
 * Look through the list of
 * buffers. Return TRUE if there
 * are any changed buffers. Buffers
 * that hold magic internal stuff are
 * not considered; who cares if the
 * list of buffer names is hacked.
 * Return FALSE if no buffers
 * have been changed.
 */
anycb()
{
        register BUFFER *bp;

        bp = bheadp;
        while (bp != NULL) {
                if ((bp->b_flag&BFTEMP)==0 && (bp->b_flag&BFCHG)!=0)
                        return (TRUE);
                bp = bp->b_bufp;
        }
        return (FALSE);
}

/*
 * Find a buffer, by name. Return a pointer
 * to the BUFFER structure associated with it. If
 * the named buffer is found, but is a TEMP buffer (like
 * the buffer list) conplain. If the buffer is not found
 * and the "cflag" is TRUE, create it. The "bflag" is
 * the settings for the flags in in buffer.
 */
BUFFER  *
bfind(bname, cflag, bflag)
     register char *bname;
     int  cflag, bflag;
{
        register BUFFER *bp;
	register BUFFER *sb;	/* buffer to insert after */
        register LINE   *lp;

        bp = bheadp;
        while (bp != NULL) {
                if (strcmp(bname, bp->b_bname) == 0) {
                        if ((bp->b_flag&BFTEMP) != 0) {
                                mlwrite("Cannot select builtin buffer", NULL);
                                return (NULL);
                        }
                        return (bp);
                }
                bp = bp->b_bufp;
        }
        if (cflag != FALSE) {
                if ((bp=(BUFFER *)malloc(sizeof(BUFFER))) == NULL)
                        return (NULL);
                if ((lp=lalloc(0)) == NULL) {
                        free((char *) bp);
                        return (NULL);
                }
		/* find the place in the list to insert this buffer */
		if (bheadp == NULL || strcmp(bheadp->b_bname, bname) > 0) {
			/* insert at the begining */
	                bp->b_bufp = bheadp;
        	        bheadp = bp;
        	} else {
			sb = bheadp;
			while (sb->b_bufp != NULL) {
				if (strcmp(sb->b_bufp->b_bname, bname) > 0)
					break;
				sb = sb->b_bufp;
			}

			/* and insert it */
       			bp->b_bufp = sb->b_bufp;
        		sb->b_bufp = bp;
       		}

		/* and set up the other buffer fields */
		bp->b_active = TRUE;
                bp->b_dotp  = lp;
                bp->b_doto  = 0;
                bp->b_markp = NULL;
                bp->b_marko = 0;
                bp->b_flag  = bflag;
		bp->b_mode  = gmode;
                bp->b_nwnd  = 0;
                bp->b_linep = lp;
                strcpy(bp->b_fname, "");
                strcpy(bp->b_bname, bname);
                lp->l_fp = lp;
                lp->l_bp = lp;
        }
        return (bp);
}

/*
 * This routine blows away all of the text
 * in a buffer. If the buffer is marked as changed
 * then we ask if it is ok to blow it away; this is
 * to save the user the grief of losing text. The
 * window chain is nearly always wrong if this gets
 * called; the caller must arrange for the updates
 * that are required. Return TRUE if everything
 * looks good.
 */
bclear(bp)
register BUFFER *bp;
{
        register LINE   *lp;
        register int    s = FALSE;

	if(Pmaster){
	    if ((bp->b_flag&BFTEMP) == 0 	/* Not scratch buffer.  */
		&& (bp->b_flag&BFCHG) != 0){	/* Something changed    */
		emlwrite("buffer lines not freed.", NULL);
		return (s);
	    }
	}
	else{
	    if ((bp->b_flag&BFTEMP) == 0	/* Not scratch buffer.  */
		&& (bp->b_flag&BFCHG) != 0	/* Something changed    */
		&& (s=mlyesno("Discard changes", -1)) != TRUE){
		return (s);
	    }
	}

        bp->b_flag  &= ~BFCHG;                  /* Not changed          */
        while ((lp=lforw(bp->b_linep)) != bp->b_linep)
                lfree(lp);
        bp->b_dotp  = bp->b_linep;              /* Fix "."              */
        bp->b_doto  = 0;
        bp->b_markp = NULL;                     /* Invalidate "mark"    */
        bp->b_marko = 0;
        return (TRUE);
}


/*
 * packbuf - will pack up the main buffer in the buffer provided 
 *           to be returned to the program that called pico.
 *	     if need be, allocate memory for the new message. 
 *           will also free the memory associated with the editor
 *           buffer, by calling zotedit.
 */
packbuf(buf, blen, lcrlf)
char	**buf;
int	*blen;
int	lcrlf;				/* EOLs are local or CRLF */
{
    register int    i = 0;
    register LINE   *lp;
    register int    retval = 0;
    register char   *bufp;
    register char   *eobuf;
 
    if(anycb() != FALSE){

        lp = lforw(curbp->b_linep);
        do{					/* how many chars? */
            i += llength(lp);
	    /*
	     * add extra for new lines to be inserted later
	     */
	    i += 2;
	    lp = lforw(lp);
        }
        while(lp != curbp->b_linep);

        if(i > *blen){				/* new buffer ? */
            /*
             * don't forget to add one for the null terminator!!!
             */
	    if((bufp = (char *)malloc((i+1)*sizeof(char))) == NULL){
                    zotedit();			/* bag it! */
		    return(COMP_FAILED);
	    }
            free(*buf);
	    *buf = bufp;
	    *blen = i;
        }
        else{
            bufp = *buf;
        }
    
        eobuf = bufp + *blen;
        lp = lforw(curbp->b_linep);             /* First line.          */
        do {
	    for (i = 0; i < llength(lp); i++){	/* copy into buffer */
	        if((bufp+1) < eobuf){
		    *bufp++ = (lp->l_text[i].c & 0xFF);
	        }
	        else{
		    /*
		     * the idea is to malloc enough space for the new
		     * buffer...
		     */
		    *bufp = '\0';
                    zotedit();
		    return(BUF_CHANGED|COMP_FAILED);
	        }
	    }
	    if(lcrlf){
		*bufp++ = '\n';			/* EOLs use local convention */
	    }
	    else{
		*bufp++ = 0x0D;			/* EOLs use net standard */
		*bufp++ = 0x0A;
	    }
            lp = lforw(lp);
        }
        while (lp != curbp->b_linep);
	if(lcrlf)
	  *--bufp = '\0';
	else
	  *bufp = '\0';
        retval = BUF_CHANGED;
    }

    zotedit();
    return(retval);
}


/*
 * readbuf - reads in a buffer.
 */
void
readbuf(buf)
char	**buf;
{
        register LINE   *lp1;
        register LINE   *lp2;
        register BUFFER *bp;
        register WINDOW *wp;
        register int    i;
        register int    s;
        char   *sptr;          /* pointer into buffer string */
        int		nbytes;
        char            line[NLINE];
	CELL            ac;
 
	bp = curbp;
        bp->b_flag &= ~(BFTEMP|BFCHG);
	sptr = *buf;
	ac.a  = 0;

        while((s=sgetline(&sptr,&nbytes,line,NLINE)) == FIOSUC || s == FIOLNG){

                if ((lp1=lalloc(nbytes)) == NULL) {
                        s = FIOERR;             /* Keep message on the  */
                        break;                  /* display.             */
                }
                lp2 = lback(curbp->b_linep);
                lp2->l_fp = lp1;
                lp1->l_fp = curbp->b_linep;
                lp1->l_bp = lp2;
                curbp->b_linep->l_bp = lp1;
                for (i=0; i<nbytes; ++i){
		    ac.c = line[i];
		    lputc(lp1, i, ac);
		}
        }

        for (wp=wheadp; wp!=NULL; wp=wp->w_wndp) {
                if (wp->w_bufp == curbp) {
                        wheadp->w_linep = lforw(curbp->b_linep);
                        wheadp->w_dotp  = lback(curbp->b_linep);
                        wheadp->w_doto  = 0;
                        wheadp->w_markp = NULL;
                        wheadp->w_marko = 0;
                        wheadp->w_flag |= WFHARD;
                }
        }

	strcpy(bp->b_bname, "main");
	strcpy(bp->b_fname, "");

	bp->b_dotp = bp->b_linep;
	bp->b_doto = 0;
}


/*
 * sgetline - copy characters from ibuf to obuf, ending at the first 
 *            newline.  return with ibuf pointing to first char after
 *            newline.
 */
sgetline(ibuf, nchars, obuf, blen)
char	**ibuf;
int	*nchars;
char	*obuf;
int	blen;
{
    register char 	*len;
    register char 	*cbuf = *ibuf;
    register char 	*bufp = obuf;
    register int 	retval = FIOSUC;
#define CR '\015'
#define LF '\012'

    *nchars = 0;
    if(*cbuf == '\0'){
	retval = FIOEOF;
    }
    else{
	len = obuf + blen;
	while (*cbuf != CR && *cbuf != LF && *cbuf != '\0'){
	    if(bufp < len){
		*bufp++ = *cbuf++;
		(*nchars)++;
	    }
	    else{
		*bufp = '\0';
		retval = FIOLNG;
		break;
	    }
	}
    }
    *bufp = '\0';			/* end retured line */
    *ibuf = (*cbuf == CR) ? ++cbuf : cbuf;
    *ibuf = (*cbuf == LF) ? ++cbuf : cbuf;
    return(retval);
}
