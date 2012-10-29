#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: composer.c 14001 2005-03-17 19:07:47Z hubert $";
#endif
/*
 * Program:	Pine composer routines
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
 * NOTES:
 *
 *  - composer.c is the composer for the PINE mail system
 *
 *  - tabled 01/19/90
 *
 *  Notes: These routines aren't incorporated yet, because the composer as
 *         a whole still needs development.  These are ideas that should
 *         be implemented in later releases of PINE.  See the notes in 
 *         pico.c concerning what needs to be done ....
 *
 *  - untabled 01/30/90
 *
 *  Notes: Installed header line editing, with wrapping and unwrapping, 
 *         context sensitive help, and other mail header editing features.
 *
 *  - finalish code cleanup 07/15/91
 * 
 *  Notes: Killed/yanked header lines use emacs kill buffer.
 *         Arbitrarily large headers are handled gracefully.
 *         All formatting handled by FormatLines.
 *
 *  - Work done to optimize display painting 06/26/92
 *         Not as clean as it should be, needs more thought 
 *
 */
#include "headers.h"


int              InitEntryText PROTO((char *, struct headerentry *));
int              HeaderOffset PROTO((int));
int              HeaderFocus PROTO((int, int));
int              LineEdit PROTO((int));
int              header_downline PROTO((int, int));
int              header_upline PROTO((int));
int              FormatLines PROTO((struct hdr_line *, char *, int, int, int));
int              FormatSyncAttach PROTO((void));
char            *strqchr PROTO((char *, int, int *, int));
int              ComposerHelp PROTO((int));
void             NewTop PROTO((int));
void             display_delimiter PROTO((int));
void             zotentry PROTO((struct hdr_line *));
int              InvertPrompt PROTO((int, int));
int              partial_entries PROTO((void));
int              physical_line PROTO((struct hdr_line *));
int              strend PROTO((char *, int));
int              KillHeaderLine PROTO((struct hdr_line *, int));
int              SaveHeaderLines PROTO((void));
char            *break_point PROTO((char *, int, int, int *));
int              hldelete PROTO((struct hdr_line *));
int              is_blank PROTO((int, int, int));
int              zotcomma PROTO((char *));
struct hdr_line *first_hline PROTO((int *));
struct hdr_line *first_sel_hline PROTO((int *));
struct hdr_line *next_hline PROTO((int *, struct hdr_line *));
struct hdr_line *next_sel_hline PROTO((int *, struct hdr_line *));
struct hdr_line *prev_hline PROTO((int *, struct hdr_line *));
struct hdr_line *prev_sel_hline PROTO((int *, struct hdr_line *));
struct hdr_line *first_requested_hline PROTO((int *));
void             fix_mangle_and_err PROTO((int *, char **, char *));


/*
 * definition header field array, structures defined in pico.h
 */
struct headerentry *headents;


/*
 * structure that keeps track of the range of header lines that are
 * to be displayed and other fun stuff about the header
 */
struct on_display ods;				/* global on_display struct */


/*
 * useful macros
 */
#define	HALLOC()	(struct hdr_line *)malloc(sizeof(struct hdr_line))
#define	LINELEN()	(term.t_ncol - headents[ods.cur_e].prlen)
#define	BOTTOM()	(term.t_nrow - term.t_mrow)
#define	FULL_SCR()	(BOTTOM() - 3)
#define	HALF_SCR()	(FULL_SCR()/2)

#ifdef	MOUSE
/*
 * Redefine HeaderEditor to install wrapper required for mouse even
 * handling...
 */
#define	HeaderEditor	HeaderEditorWork
#endif /* MOUSE */

#if	(defined(DOS) && !defined(_WINDOWS)) || defined(OS2)
#define	HDR_DELIM	"\xCD\xCD\xCD\xCD\xCD Message Text \xCD\xCD\xCD\xCD\xCD"
#else
#define	HDR_DELIM	"----- Message Text -----"
#endif

/*
 * useful declarations
 */
static short delim_ps  = 0;		/* previous state */
static short invert_ps = 0;		/* previous state */


static KEYMENU menu_header[] = {
    {"^G", "Get Help", KS_SCREENHELP},	{"^X", "Send", KS_SEND},
    {"^R", "Rich Hdr", KS_RICHHDR},	{"^Y", "PrvPg/Top", KS_PREVPAGE},
    {"^K", "Cut Line", KS_CURPOSITION},	{"^O", "Postpone", KS_POSTPONE},
    {"^C", "Cancel", KS_CANCEL},	{"^D", "Del Char", KS_NONE},
    {"^J", "Attach", KS_ATTACH},	{"^V", "NxtPg/End", KS_NEXTPAGE},
    {"^U", "UnDel Line", KS_NONE},	{NULL, NULL}
};
#define	SEND_KEY	1
#define	RICH_KEY	2
#define	CUT_KEY		4
#define	PONE_KEY	5
#define	DEL_KEY		7
#define	ATT_KEY		8
#define	UDEL_KEY	10
#define	TO_KEY		11


/*
 * function key mappings for header editor
 */
static int ckm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'C')},
    { F3,  (CTRL|'X')},
    { F4,  (CTRL|'D')},
    { F5,  (CTRL|'R')},
    { F6,  (CTRL|'J')},
    { F7,  0 },
    { F8,  0 },
    { F9,  (CTRL|'K')},
    { F10, (CTRL|'U')},
    { F11, (CTRL|'O')},
    { F12, (CTRL|'T')}
};


/*
 * InitMailHeader - initialize header array, and set beginning editor row 
 *                  range.  The header entry structure should look just like 
 *                  what is written on the screen, the vector 
 *                  (entry, line, offset) will describe the current cursor 
 *                  position in the header.
 *
 *	   Returns: TRUE if special header handling was requested,
 *		    FALSE under standard default behavior.
 */
InitMailHeader(mp)
PICO  *mp;
{
    char	       *addrbuf;
    struct headerentry *he;
    int			rv;

    if(!mp->headents){
	headents = NULL;
	return(FALSE);
    }

    /*
     * initialize some of on_display structure, others below...
     */
    ods.p_off  = 0;
    ods.p_line = COMPOSER_TOP_LINE;
    ods.top_l = ods.cur_l = NULL;

    headents = mp->headents;
    /*--- initialize the fields in the headerent structure ----*/
    for(he = headents; he->name != NULL; he++){
	he->hd_text    = NULL;
	he->display_it = he->display_it ? he->display_it : !he->rich_header;
        if(he->is_attach) {
            /*--- A lot of work to do since attachments are special ---*/
            he->maxlen = 0;
	    if(mp->attachments != NULL){
		char   buf[NLINE];
                int    attno = 0;
		int    l1, ofp, ofp1, ofp2;  /* OverFlowProtection */
		size_t addrbuflen = 4 * NLINE; /* malloc()ed size of addrbuf */
                PATMT *ap = mp->attachments;
 
		ofp = NLINE - 35;

                addrbuf = (char *)malloc(addrbuflen);
                addrbuf[0] = '\0';
                buf[0] = '\0';
                while(ap){
		  if((l1 = strlen(ap->filename)) <= ofp){
		      ofp1 = l1;
		      ofp2 = ofp - l1;
		  }
		  else{
		      ofp1 = ofp;
		      ofp2 = 0;
		  }

		  if(ap->filename){
                     sprintf(buf, "%d. %.*s%s %s%s%s\"",
                             ++attno,
			     ofp1,
                             ap->filename,
			     (l1 > ofp) ? "...]" : "",
                             ap->size ? "(" : "",
                             ap->size ? ap->size : "",
                             ap->size ? ") " : "");

		     /* append description, escaping quotes */
		     if(ap->description){
			 char *dp = ap->description, *bufp = &buf[strlen(buf)];
			 int   escaped = 0;

			 do
			   if(*dp == '\"' && !escaped){
			       *bufp++ = '\\';
			       ofp2--;
			   }
			   else if(escaped){
			       *bufp++ = '\\';
			       escaped = 0;
			   }
			 while(--ofp2 > 0 && (*bufp++ = *dp++));

			 *bufp = '\0';
		     }

		     sprintf(buf + strlen(buf), "\"%s", ap->next ? "," : "");

		     if(strlen(addrbuf) + strlen(buf) >= addrbuflen){
			 addrbuflen += NLINE * 4;
			 if(!(addrbuf = (char *)realloc(addrbuf, addrbuflen))){
			     emlwrite("\007Can't realloc addrbuf to %d bytes",
				      (void *) addrbuflen);
			     return(ABORT);
			 }
		     }

                     strcat(addrbuf, buf);
                 }
                 ap = ap->next;
                }
                InitEntryText(addrbuf, he);
                free((char *)addrbuf);
            } else {
                InitEntryText("", he);
            }
            he->realaddr = NULL;
        } else {
	    if(!he->blank)
              addrbuf = *(he->realaddr);
	    else
              addrbuf = "";

            InitEntryText(addrbuf, he);
	}
    }

    /*
     * finish initialization and then figure out display layout.
     * first, look for any field the caller requested we start in,
     * and set the offset into that field.
     */
    if(ods.cur_l = first_requested_hline(&ods.cur_e)){
	ods.top_e = 0;				/* init top_e */
	ods.top_l = first_hline(&ods.top_e);
	if(!HeaderFocus(ods.cur_e, Pmaster ? Pmaster->edit_offset : 0))
	  HeaderFocus(ods.cur_e, 0);

	rv = TRUE;
    }
    else{
	ods.top_l = first_hline(&ods.cur_e);
	ods.cur_l = first_sel_hline(&ods.cur_e);
	ods.top_e = ods.cur_e;
	rv = 0;
    }

    UpdateHeader(0);
    return(rv);
}



/*
 * InitEntryText - Add the given header text into the header entry 
 *		   line structure.
 */
InitEntryText(address, e)
char	*address;
struct headerentry *e;
{
    struct  hdr_line	*curline;
    register  int	longest;

    /*
     * get first chunk of memory, and tie it to structure...
     */
    if((curline = HALLOC()) == NULL){
        emlwrite("Unable to make room for full Header.", NULL);
        return(FALSE);
    }
    longest = term.t_ncol - e->prlen - 1;
    curline->text[0] = '\0';
    curline->next = NULL;
    curline->prev = NULL;
    e->hd_text = curline;		/* tie it into the list */

    if(FormatLines(curline, address, longest, e->break_on_comma, 0) == -1)
      return(FALSE);
    else
      return(TRUE);
}



/*
 *  ResizeHeader - Handle resizing display when SIGWINCH received.
 *
 *	notes:
 *		works OK, but needs thorough testing
 *		  
 */
ResizeHeader()
{
    register struct headerentry *i;
    register int offset;

    if(!headents)
      return(TRUE);

    offset = (ComposerEditing) ? HeaderOffset(ods.cur_e) : 0;

    for(i=headents; i->name; i++){		/* format each entry */
	if(FormatLines(i->hd_text, "", (term.t_ncol - i->prlen),
		       i->break_on_comma, 0) == -1){
	    return(-1);
	}
    }

    if(ComposerEditing)				/* restart at the top */
      HeaderFocus(ods.cur_e, offset);		/* fix cur_l and p_off */
    else {
      struct hdr_line *l;
      int              e;

      for(i = headents; i->name != NULL; i++);	/* Find last line */
      i--;
      e = i - headents;
      l = headents[e].hd_text;
      if(!headents[e].display_it || headents[e].blank)
        l = prev_sel_hline(&e, l);		/* last selectable line */

      if(!l){
	  e = i - headents;
	  l = headents[e].hd_text;
      }

      HeaderFocus(e, -1);		/* put focus on last line */
    }

    if(ComposerTopLine != COMPOSER_TOP_LINE)
      UpdateHeader(0);

    PaintBody(0);

    if(ComposerEditing)
      movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);

    (*term.t_flush)();
    return(TRUE);
}



/*
 * HeaderOffset - return the character offset into the given header
 */
HeaderOffset(h)
int	h;
{
    register struct hdr_line *l;
    int	     i = 0;

    l = headents[h].hd_text;

    while(l != ods.cur_l){
	i += strlen(l->text);
	l = l->next;
    }
    return(i+ods.p_off);
}



/*
 * HeaderFocus - put the dot at the given offset into the given header
 */
HeaderFocus(h, offset)
int	h, offset;
{
    register struct hdr_line *l;
    register int    i;
    int	     last = 0;

    if(offset == -1)				/* focus on last line */
      last = 1;

    l = headents[h].hd_text;
    while(1){
	if(last && l->next == NULL){
	    break;
	}
	else{
	    if((i=strlen(l->text)) >= offset)
	      break;
	    else
	      offset -= i;
	}
	if((l = l->next) == NULL)
	  return(FALSE);
    }

    ods.cur_l = l;
    ods.p_len = strlen(l->text);
    ods.p_off = (last) ? 0 : offset;

    return(TRUE);
}



/*
 * HeaderEditor() - edit the mail header field by field, trapping 
 *                  important key sequences, hand the hard work off
 *                  to LineEdit().  
 *	returns:
 *              -3    if we drop out bottom *and* want to process mouse event
 *		-1    if we drop out the bottom 
 *		FALSE if editing is cancelled
 *		TRUE  if editing is finished
 */
HeaderEditor(f, n)
int f, n;
{
    register  int	i;
    register  int	ch;
    register  char	*bufp;
    struct headerentry *h;
    int                 cur_e, mangled, retval = -1,
		        hdr_only = (gmode & MDHDRONLY) ? 1 : 0;
    char               *errmss, *err;
#ifdef MOUSE
    MOUSEPRESS		mp;
#endif

    if(!headents)
      return(TRUE);				/* nothing to edit! */

    ComposerEditing = TRUE;
    display_delimiter(0);			/* provide feedback */

#ifdef	_WINDOWS
    mswin_setscrollrange (0, 0);
#endif /* _WINDOWS */

    /* 
     * Decide where to begin editing.  if f == TRUE begin editing
     * at the bottom.  this case results from the cursor backing
     * into the editor from the bottom.  otherwise, the user explicitly
     * requested editing the header, and we begin at the top.
     * 
     * further, if f == 1, we moved into the header by hitting the up arrow
     * in the message text, else if f == 2, we moved into the header by
     * moving past the left edge of the top line in the message.  so, make 
     * the end of the last line of the last entry the current cursor position
     * lastly, if f == 3, we moved into the header by hitting backpage() on
     * the top line of the message, so scroll a page back.  
     */
    if(f){
	if(f == 2){				/* 2 leaves cursor at end  */
	    struct hdr_line *l = ods.cur_l;
	    int              e = ods.cur_e;

	    /*--- make sure on last field ---*/
	    while(l = next_sel_hline(&e, l))
	      if(headents[ods.cur_e].display_it){
		  ods.cur_l = l;
		  ods.cur_e = e;
	      }

	    ods.p_off = 1000;			/* and make sure at EOL    */
	}
	else{
	    /*
	     * note: assumes that ods.cur_e and ods.cur_l haven't changed
	     *       since we left...
	     */

	    /* fix postition */
	    if(curwp->w_doto < headents[ods.cur_e].prlen)
	      ods.p_off = 0;
	    else if(curwp->w_doto < ods.p_off + headents[ods.cur_e].prlen)
	      ods.p_off = curwp->w_doto - headents[ods.cur_e].prlen;
	    else
	      ods.p_off = 1000;

	    /* and scroll back if needed */
	    if(f == 3)
	      for(i = 0; header_upline(0) && i <= FULL_SCR(); i++)
		;
	}

	ods.p_line = ComposerTopLine - 2;
    }
    /* else just trust what ods contains */

    InvertPrompt(ods.cur_e, TRUE);		/* highlight header field */
    sgarbk = 1;

    do{
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0)
	      sgarbk = 1;
	}

	if(sgarbk){
	    if(km_popped){  /* temporarily change to cause menu to paint */
		term.t_mrow = 2;
		curwp->w_ntrows -= 2;
		movecursor(term.t_nrow-2, 0); /* clear status line, too */
		peeol();
	    }
	    else if(term.t_mrow == 0)
	      PaintBody(1);

	    ShowPrompt();			/* display correct options */
	    sgarbk = 0;
	    if(km_popped){
		term.t_mrow = 0;
		curwp->w_ntrows += 2;
	    }
	}

	ch = LineEdit(!(gmode&MDVIEW));		/* work on the current line */

	if(km_popped)
	  switch(ch){
	    case NODATA:
	    case (CTRL|'L'):
	      km_popped++;
	      break;
	    
	    default:
	      movecursor(term.t_nrow-2, 0);
	      peeol();
	      movecursor(term.t_nrow-1, 0);
	      peeol();
	      movecursor(term.t_nrow, 0);
	      peeol();
	      break;
	  }

        switch (ch){
	  case (CTRL|'R') :			/* Toggle header display */
	    if(Pmaster->pine_flags & MDHDRONLY){
		if(Pmaster->expander){
		    packheader();
		    call_expander();
		    PaintBody(0);
		    break;
		}
		else
		  goto bleep;
	    }

            /*---- Are there any headers to expand above us? ---*/
            for(h = headents; h != &headents[ods.cur_e]; h++)
              if(h->rich_header)
                break;
            if(h->rich_header)
	      InvertPrompt(ods.cur_e, FALSE);	/* Yes, don't leave inverted */

	    mangled = 0;
	    err = NULL;
	    if(partial_entries()){
                /*--- Just turned off all rich headers --*/
		if(headents[ods.cur_e].rich_header){
                    /*-- current header got turned off too --*/
#ifdef	ATTACHMENTS
		    if(headents[ods.cur_e].is_attach){
			/* verify the attachments */
			if((i = FormatSyncAttach()) != 0){
			    FormatLines(headents[ods.cur_e].hd_text, "",
					term.t_ncol - headents[ods.cur_e].prlen,
					headents[ods.cur_e].break_on_comma, 0);
			}
		    }
#endif
		    if(headents[ods.cur_e].builder)	/* verify text */
		      i = call_builder(&headents[ods.cur_e], &mangled, &err)>0;
                    /* Check below */
                    for(cur_e =ods.cur_e; headents[cur_e].name!=NULL; cur_e++)
                      if(!headents[cur_e].rich_header)
                        break;
                    if(headents[cur_e].name == NULL) {
                        /* didn't find one, check above */
                        for(cur_e =ods.cur_e; headents[cur_e].name!=NULL;
                            cur_e--)
                          if(!headents[cur_e].rich_header)
                            break;

                    }
		    ods.p_off = 0;
		    ods.cur_e = cur_e;
		    ods.cur_l = headents[ods.cur_e].hd_text;
		}
	    }

	    ods.p_line = 0;			/* force update */
	    UpdateHeader(0);
	    PaintHeader(COMPOSER_TOP_LINE, FALSE);
	    PaintBody(1);
	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
	    break;

	  case (CTRL|'C') :			/* bag whole thing ?*/
	    if(abort_composer(1, 0) == TRUE)
	      return(FALSE);

	    break;

	  case (CTRL|'X') :			/* Done. Send it. */
	    i = 0;
#ifdef	ATTACHMENTS
	    if(headents[ods.cur_e].is_attach){
		/* verify the attachments, and pretty things up in case
		 * we come back to the composer due to error...
		 */
		if((i = FormatSyncAttach()) != 0){
		    sleep(2);		/* give time for error to absorb */
		    FormatLines(headents[ods.cur_e].hd_text, "",
				term.t_ncol - headents[ods.cur_e].prlen,
				headents[ods.cur_e].break_on_comma, 0);
		}
	    }
	    else
#endif
	    mangled = 0;
	    err = NULL;
	    if(headents[ods.cur_e].builder)	/* verify text? */
	      i = call_builder(&headents[ods.cur_e], &mangled, &err);

	    if(i < 0){			/* don't leave without a valid addr */
		fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
		break;
	    }
	    else if(i > 0){
		ods.cur_l = headents[ods.cur_e].hd_text; /* attach cur_l */
		ods.p_off = 0;
		ods.p_line = 0;			/* force realignment */
	        fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
		NewTop(0);
	    }

	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);

	    if(wquit(1,0) == TRUE)
	      return(TRUE);

	    if(i > 0){
		/*
		 * need to be careful here because pointers might be messed up.
		 * also, could do a better job of finding the right place to
		 * put the dot back (i.e., the addr/list that was expanded).
		 */
		UpdateHeader(0);
		PaintHeader(COMPOSER_TOP_LINE, FALSE);
		PaintBody(1);
	    }
	    break;

	  case (CTRL|'Z') :			/* Suspend compose */
	    if(gmode&MDSSPD){			/* is it allowed? */
		bktoshell();
		PaintBody(0);
	    }
	    else{
		(*term.t_beep)();
		emlwrite("Unknown Command: ^Z", NULL);
	    }
	    break;

	  case (CTRL|'O') :			/* Suspend message */
	    if(Pmaster->pine_flags & MDHDRONLY)
	      goto bleep;

	    i = 0;
	    mangled = 0;
	    err = NULL;
	    if(headents[ods.cur_e].is_attach){
		if(FormatSyncAttach() < 0){
		    if(mlyesno("Problem with attachments. Postpone anyway?",
			       FALSE) != TRUE){
			if(FormatLines(headents[ods.cur_e].hd_text, "",
				       term.t_ncol - headents[ods.cur_e].prlen,
				       headents[ods.cur_e].break_on_comma, 0) == -1)
			  emlwrite("\007Format lines failed!", NULL);
			UpdateHeader(0);
			PaintHeader(COMPOSER_TOP_LINE, FALSE);
			PaintBody(1);
			continue;
		    }
		}
	    }
	    else if(headents[ods.cur_e].builder)
	      i = call_builder(&headents[ods.cur_e], &mangled, &err);

	    ods.cur_l = headents[ods.cur_e].hd_text;
	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);

	    if(i < 0)			/* don't leave without a valid addr */
	      break;

	    suspend_composer(1, 0);
	    return(TRUE);

#ifdef	ATTACHMENTS
	  case (CTRL|'J') :			/* handle attachments */
	    if(Pmaster->pine_flags & MDHDRONLY)
	      goto bleep;

	    { char    cmt[NLINE];
	      LMLIST *lm = NULL, *lmp;
	      char buf[NLINE], *bfp;
	      int saved_km_popped;
	      size_t len;

	      /*
	       * Attachment questions mess with km_popped and assume
	       * it is zero to start with.  If we don't set it to zero
	       * on entry, the message about mime type will be erased
	       * by PaintBody.  If we don't reset it when we come back,
	       * the bottom three lines may be messed up.
	       */
	      saved_km_popped = km_popped;
	      km_popped = 0;

	      if(AskAttach(cmt, &lm)){

		for(lmp = lm; lmp; lmp = lmp->next){
		    len = lmp->dir ? strlen(lmp->dir)+1 : 0;
		    len += lmp->fname ? strlen(lmp->fname) : 0;

		    if(len+3 > sizeof(buf)){
			bfp = malloc(len+3);
			if((bfp=malloc(len+3)) == NULL){
			    emlwrite("\007Can't malloc space for filename",
				     NULL);
			    continue;
			}
		    }
		    else
		      bfp = buf;

		    if(lmp->dir && lmp->dir[0])
		      sprintf(bfp, "%s%c%s", lmp->dir, C_FILESEP,
			      lmp->fname ? lmp->fname : "");
		    else
		      sprintf(bfp, "%s", lmp->fname ? lmp->fname : "");

		    (void) QuoteAttach(bfp);

		    (void) AppendAttachment(bfp, lm->size, cmt);

		    if(bfp != buf)
		      free(bfp);
		}

		zotlmlist(lm);
	      }

	      km_popped = saved_km_popped;
	      sgarbk = 1;			/* clean up prompt */
	    }
	    break;
#endif

	  case (CTRL|'I') :			/* tab */
	    ods.p_off = 0;			/* fall through... */

	  case (CTRL|'N') :
	  case KEY_DOWN :
	    header_downline(!hdr_only, hdr_only);
	    break;

	  case (CTRL|'P') :
	  case KEY_UP :
	    header_upline(1);
	    break;

	  case (CTRL|'V') :			/* down a page */
	  case KEY_PGDN :
	    cur_e = ods.cur_e;
	    if(!next_sel_hline(&cur_e, ods.cur_l)){
		header_downline(!hdr_only, hdr_only);
		if(!(gmode & MDHDRONLY))
		  retval = -1;			/* tell caller we fell out */
	    }
	    else{
		int move_down, bot_pline;
		struct hdr_line *new_cur_l, *line, *next_line, *prev_line;

		move_down = BOTTOM() - 2 - ods.p_line;
		if(move_down < 0)
		  move_down = 0;

		/*
		 * Count down move_down lines to find the pointer to the line
		 * that we want to become the current line.
		 */
		new_cur_l = ods.cur_l;
		cur_e = ods.cur_e;
		for(i = 0; i < move_down; i++){
		    next_line = next_hline(&cur_e, new_cur_l);
		    if(!next_line)
		      break;

		    new_cur_l = next_line;
		}

		if(headents[cur_e].blank){
		    next_line = next_sel_hline(&cur_e, new_cur_l);
		    if(!next_line)
		      break;

		    new_cur_l = next_line;
		}

		/*
		 * Now call header_downline until we get down to the
		 * new current line, so that the builders all get called.
		 * New_cur_l will remain valid since we won't be calling
		 * a builder for it during this loop.
		 */
		while(ods.cur_l != new_cur_l && header_downline(0, 0))
		  ;
		
		/*
		 * Count back up, if we're at the bottom, to find the new
		 * top line.
		 */
		cur_e = ods.cur_e;
		if(next_hline(&cur_e, ods.cur_l) == NULL){
		    /*
		     * Cursor stops at bottom of headers, which is where
		     * we are right now.  Put as much of headers on
		     * screen as will fit.  Count up to figure
		     * out which line is top_l and which p_line cursor is on.
		     */
		    cur_e = ods.cur_e;
		    line = ods.cur_l;
		    /* leave delimiter on screen, too */
		    bot_pline = BOTTOM() - 1 - ((gmode & MDHDRONLY) ? 0 : 1);
		    for(i = COMPOSER_TOP_LINE; i < bot_pline; i++){
			prev_line = prev_hline(&cur_e, line);
			if(!prev_line)
			  break;
			
			line = prev_line;
		    }

		    ods.top_l = line;
		    ods.top_e = cur_e;
		    ods.p_line = i;
		      
		}
		else{
		    ods.top_l = ods.cur_l;
		    ods.top_e = ods.cur_e;
		    /*
		     * We don't want to scroll down further than the
		     * delimiter, so check to see if that is the case.
		     * If it is, we move the p_line down the screen
		     * until the bottom line is where we want it.
		     */
		    bot_pline = BOTTOM() - 1 - ((gmode & MDHDRONLY) ? 0 : 1);
		    cur_e = ods.cur_e;
		    line = ods.cur_l;
		    for(i = bot_pline; i > COMPOSER_TOP_LINE; i--){
			next_line = next_hline(&cur_e, line);
			if(!next_line)
			  break;

			line = next_line;
		    }

		    /*
		     * i is the desired value of p_line.
		     * If it is greater than COMPOSER_TOP_LINE, then
		     * we need to adjust top_l.
		     */
		    ods.p_line = i;
		    line = ods.top_l;
		    cur_e = ods.top_e;
		    for(; i > COMPOSER_TOP_LINE; i--){
			prev_line = prev_hline(&cur_e, line);
			if(!prev_line)
			  break;
			
			line = prev_line;
		    }

		    ods.top_l = line;
		    ods.top_e = cur_e;

		    /*
		     * Special case.  If p_line is within one of the bottom,
		     * move it to the bottom.
		     */
		    if(ods.p_line == bot_pline - 1){
			header_downline(0, 0);
			/* but put these back where we want them */
			ods.p_line = bot_pline;
			ods.top_l = line;
			ods.top_e = cur_e;
		    }
		}

		UpdateHeader(0);
		PaintHeader(COMPOSER_TOP_LINE, FALSE);
		PaintBody(1);
	    }

	    break;

	  case (CTRL|'Y') :			/* up a page */
	  case KEY_PGUP :
	    for(i = 0; header_upline(0) && i <= FULL_SCR(); i++)
	      if(i < 0)
		break;

	    break;

#ifdef	MOUSE
	  case KEY_MOUSE:
	    mouse_get_last (NULL, &mp);
	    switch(mp.button){
	      case M_BUTTON_LEFT :
		if (!mp.doubleclick) {
		    if (mp.row < ods.p_line) {
			for (i = ods.p_line - mp.row;
			     i > 0 && header_upline(0); 
			     --i)
			  ;
		    }
		    else {
			for (i = mp.row-ods.p_line;
			     i > 0 && header_downline(!hdr_only, 0);
			     --i)
			  ;
		    }

		    if((ods.p_off = mp.col - headents[ods.cur_e].prlen) <= 0)
		      ods.p_off = 0;

		    /* -3 is returned  if we drop out bottom
		     * *and* want to process a mousepress.  The Headereditor
		     * wrapper should make sense of this return code.
		     */
		    if (ods.p_line >= ComposerTopLine)
		      retval = -3;
		}

		break;

	      case M_BUTTON_RIGHT :
#ifdef	_WINDOWS
		pico_popup();
#endif
	      case M_BUTTON_MIDDLE :
	      default :	/* NOOP */
		break;
	    }

	    break;
#endif /* MOUSE */

	  case (CTRL|'T') :			/* Call field selector */
	    errmss = NULL;
            if(headents[ods.cur_e].is_attach) {
                /*--- selector for attachments ----*/
		char dir[NLINE], fn[NLINE], sz[NLINE];
		LMLIST *lm = NULL, *lmp;

		strcpy(dir, (gmode & MDCURDIR)
		       ? (browse_dir[0] ? browse_dir : ".")
		       : ((gmode & MDTREE) || opertree[0])
		       ? opertree
		       : (browse_dir[0] ? browse_dir
			  : gethomedir(NULL)));
		fn[0] = '\0';
		if(FileBrowse(dir, NLINE, fn, NLINE, sz,
			      FB_READ | FB_ATTACH | FB_LMODEPOS, &lm) == 1){
		    char buf[NLINE], *bfp;
		    size_t len;

		    for(lmp = lm; lmp; lmp = lmp->next){
			len = lmp->dir ? strlen(lmp->dir)+1 : 0;
			len += lmp->fname ? strlen(lmp->fname) : 0;
			len += 7;
			len += strlen(lmp->size);

			if(len+3 > sizeof(buf)){
			    bfp = malloc(len+3);
			    if((bfp=malloc(len+3)) == NULL){
				emlwrite("\007Can't malloc space for filename",
					 NULL);
				continue;
			    }
			}
			else
			  bfp = buf;

			if(lmp->dir && lmp->dir[0])
			  sprintf(bfp, "%s%c%s", lmp->dir, C_FILESEP,
				  lmp->fname ? lmp->fname : "");
			else
			  sprintf(bfp, "%s", lmp->fname ? lmp->fname : "");

			(void) QuoteAttach(bfp);

			sprintf(bfp + strlen(bfp), " (%s) \"\"%s", lmp->size,
			    (!headents[ods.cur_e].hd_text->text[0]) ? "":",");

			if(FormatLines(headents[ods.cur_e].hd_text, bfp,
				     term.t_ncol - headents[ods.cur_e].prlen,
				     headents[ods.cur_e].break_on_comma,0)==-1){
			    emlwrite("\007Format lines failed!", NULL);
			}

			if(bfp != buf)
			  free(bfp);

			UpdateHeader(0);
		    }

		    zotlmlist(lm);
		}				/* else, nothing of interest */
            } else if (headents[ods.cur_e].selector != NULL) {
		VARS_TO_SAVE *saved_state;

                /*---- General selector for non-attachments -----*/

		/*
		 * Since the selector may make a new call back to pico()
		 * we need to save and restore the pico state here.
		 */
		if((saved_state = save_pico_state()) != NULL){
		    bufp = (*(headents[ods.cur_e].selector))(&errmss);
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		    ttresize();			/* fixup screen bufs */
		    picosigs();			/* restore altered signals */
		}
		else{
		    char *s = "Not enough memory";

		    errmss = (char *)malloc((strlen(s)+1) * sizeof(char));
		    strcpy(errmss, s);
		    bufp = NULL;
		}

                if(bufp != NULL) {
		    mangled = 0;
		    err = NULL;
                    if(headents[ods.cur_e].break_on_comma) {
                        /*--- Must be an address ---*/
                        if(ods.cur_l->text[0] != '\0'){
			    struct hdr_line *h, *hh;
			    int             q = 0;
			    
			    /*--- Check for continuation of previous line ---*/
			    for(hh = h = headents[ods.cur_e].hd_text; 
				h && h->next != ods.cur_l; h = h->next){
				if(strqchr(h->text, ',', &q, -1)){
				    hh = h->next;
				}
			    }
			    if(hh && hh != ods.cur_l){
				/*--- Looks like a continuation ---*/
				ods.cur_l = hh;
				ods.p_len = strlen(hh->text);
			    }
			    for(i = ++ods.p_len; i; i--)
			      ods.cur_l->text[i] = ods.cur_l->text[i-1];

			    ods.cur_l->text[0] = ',';
			}

                        if(FormatLines(ods.cur_l, bufp,
				      (term.t_ncol-headents[ods.cur_e].prlen), 
                                      headents[ods.cur_e].break_on_comma, 0) == -1){
                            emlwrite("Problem adding address to header !",
                                     NULL);
                            (*term.t_beep)();
                            break;
                        }

			/*
			 * If the "selector" has a "builder" as well, pass
			 * what was just selected thru the builder...
			 */
			if(headents[ods.cur_e].builder){
			    struct hdr_line *l;
			    int		     cur_row, top_too = 0;

			    for(l = headents[ods.cur_e].hd_text, cur_row = 0;
				l && l != ods.cur_l;
				l = l->next, cur_row++)
			      ;

			    top_too = headents[ods.cur_e].hd_text == ods.top_l;

			    if(call_builder(&headents[ods.cur_e], &mangled,
					    &err) < 0){
			        fix_mangle_and_err(&mangled, &err,
						   headents[ods.cur_e].name);
			    }

			    for(ods.cur_l = headents[ods.cur_e].hd_text;
				ods.cur_l->next && cur_row;
				ods.cur_l = ods.cur_l->next, cur_row--)
			      ;

			    if(top_too)
			      ods.top_l = headents[ods.cur_e].hd_text;
			}

    		        UpdateHeader(0);
                    } else {
                        strcpy(headents[ods.cur_e].hd_text->text, bufp);
                    }

		    free(bufp);
		    /* mark this entry dirty */
		    headents[ods.cur_e].sticky = 1;
		    headents[ods.cur_e].dirty  = 1;
		    fix_mangle_and_err(&mangled,&err,headents[ods.cur_e].name);
		}
	    } else {
                /*----- No selector -----*/
		(*term.t_beep)();
		continue;
	    }

	    PaintBody(0);
	    if(errmss != NULL) {
		(*term.t_beep)();
		emlwrite(errmss, NULL);
		free(errmss);
		errmss = NULL;
	    }
	    continue;

	  case (CTRL|'G'):			/* HELP */
	    if(term.t_mrow == 0){
		if(km_popped == 0){
		    km_popped = 2;
		    sgarbk = 1;			/* bring up menu */
		    break;
		}
	    }

	    if(!ComposerHelp(ods.cur_e))
	      break;				/* else, fall through... */

	  case (CTRL|'L'):			/* redraw requested */
	    PaintBody(0);
	    break;

	  case (CTRL|'_'):			/* file editor */
            if(headents[ods.cur_e].fileedit != NULL){
		struct headerentry *e;
		struct hdr_line    *line;
		int                 sz = 0;
		char               *filename;
		VARS_TO_SAVE       *saved_state;

		/*
		 * Since the fileedit will make a new call back to pico()
		 * we need to save and restore the pico state here.
		 */
		if((saved_state = save_pico_state()) != NULL){
		    e = &headents[ods.cur_e];

		    for(line = e->hd_text; line != NULL; line = line->next)
		      sz += strlen(line->text);
		    
		    filename = (char *)malloc(sz + 1);
		    filename[0] = '\0';

		    for(line = e->hd_text; line != NULL; line = line->next)
		      strcat(filename, line->text);

		    errmss = (*(headents[ods.cur_e].fileedit))(filename);

		    free(filename);
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		    ttresize();			/* fixup screen bufs */
		    picosigs();			/* restore altered signals */
		}
		else{
		    char *s = "Not enough memory";

		    errmss = (char *)malloc((strlen(s)+1) * sizeof(char));
		    strcpy(errmss, s);
		}

		PaintBody(0);

		if(errmss != NULL) {
		    (*term.t_beep)();
		    emlwrite(errmss, NULL);
		    free(errmss);
		    errmss = NULL;
		}

		continue;
	    }
	    else
	      goto bleep;

	    break;

	  default :				/* huh? */
bleep:
	    if(ch&CTRL)
	      emlwrite("\007Unknown command: ^%c", (void *)(ch&0xff));
	    else
	  case BADESC:
	      emlwrite("\007Unknown command", NULL);

	  case NODATA:
	    break;
	}
    }
    while (ods.p_line < ComposerTopLine);

    display_delimiter(1);
    curwp->w_flag |= WFMODE;
    movecursor(currow, curcol);
    ComposerEditing = FALSE;
    if (ComposerTopLine == BOTTOM()){
	UpdateHeader(0);
	PaintHeader(COMPOSER_TOP_LINE, FALSE);
	PaintBody(1);
    }
    
    return(retval);
}


/*
 *
 */
int
header_downline(beyond, gripe)
    int beyond, gripe;
{
    struct hdr_line *new_l, *l;
    int    new_e, status, fullpaint, len, e, incr = 0;

    /* calculate the next line: physical *and* logical */
    status    = 0;
    new_e     = ods.cur_e;
    if((new_l = next_sel_hline(&new_e, ods.cur_l)) == NULL && !beyond){

	if(gripe){
	    char xx[81];

	    strcpy(xx, "Can't move down. Use ^X to ");
	    strcat(xx, (Pmaster && Pmaster->exit_label)
			    ? Pmaster->exit_label
			    : (gmode & MDHDRONLY)
			      ? "eXit/Save"
			      : (gmode & MDVIEW)
				? "eXit"
				: "Send");
	    strcat(xx, ".");
	    emlwrite(xx, NULL);
	}

        return(0);
    }

    /*
     * Because of blank header lines the cursor may need to move down
     * more than one line. Figure out how far.
     */
    e = ods.cur_e;
    l = ods.cur_l;
    while(l != new_l){
	if((l = next_hline(&e, l)) != NULL)
	  incr++;
	else
	  break;  /* can't happen */
    }

    ods.p_line += incr;
    fullpaint = ods.p_line >= BOTTOM();	/* force full redraw?       */

    /* expand what needs expanding */
    if(new_e != ods.cur_e || !new_l){		/* new (or last) field !    */
	if(new_l)
	  InvertPrompt(ods.cur_e, FALSE);	/* turn off current entry   */

	if(headents[ods.cur_e].is_attach) {	/* verify data ?	    */
	    if(status = FormatSyncAttach()){	/* fixup if 1 or -1	    */
		headents[ods.cur_e].rich_header = 0;
		if(FormatLines(headents[ods.cur_e].hd_text, "",
			       term.t_ncol-headents[new_e].prlen,
			       headents[ods.cur_e].break_on_comma, 0) == -1)
		  emlwrite("\007Format lines failed!", NULL);
	    }
	} else if(headents[ods.cur_e].builder) { /* expand addresses	    */
	    int mangled = 0;
	    char *err = NULL;

	    if((status = call_builder(&headents[ods.cur_e], &mangled, &err))>0){
		struct hdr_line *l;		/* fixup ods.cur_l */
		ods.p_line = 0;			/* force top line recalc */
		for(l = headents[ods.cur_e].hd_text; l; l = l->next)
		  ods.cur_l = l;

		if(new_l)			/* if new_l, force validity */
		  new_l = headents[new_e].hd_text;

		NewTop(0);			/* get new top_l */
	    }
	    else if(status < 0){		/* bad addr? no leave! */
		--ods.p_line;
		fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
		InvertPrompt(ods.cur_e, TRUE);
		return(0);
	    }

	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
	}

	if(new_l){				/* if one below, turn it on */
	    InvertPrompt(new_e, TRUE);
	    sgarbk = 1;				/* paint keymenu too	    */
	}
    }

    if(new_l){					/* fixup new pointers	    */
	ods.cur_l = (ods.cur_e != new_e) ? headents[new_e].hd_text : new_l;
	ods.cur_e = new_e;
	if(ods.p_off > (len = strlen(ods.cur_l->text)))
	  ods.p_off = len;
    }

    if(!new_l || status || fullpaint){		/* handle big screen paint  */
	UpdateHeader(0);
	PaintHeader(COMPOSER_TOP_LINE, FALSE);
	PaintBody(1);

	if(!new_l){				/* make sure we're done     */
	    ods.p_line = ComposerTopLine;
	    InvertPrompt(ods.cur_e, FALSE);	/* turn off current entry   */
	}
    }

    return(new_l ? 1 : 0);
}


/*
 *
 */
int
header_upline(gripe)
    int gripe;
{
    struct hdr_line *new_l, *l;
    int    new_e, status, fullpaint, len, e, incr = 0;

    /* calculate the next line: physical *and* logical */
    status    = 0;
    new_e     = ods.cur_e;
    if(!(new_l = prev_sel_hline(&new_e, ods.cur_l))){	/* all the way up! */
	ods.p_line = COMPOSER_TOP_LINE;
	if(gripe)
	  emlwrite("Can't move beyond top of %s",
	      (Pmaster->pine_flags & MDHDRONLY) ? "entry" : "header");

	return(0);
    }
    
    /*
     * Because of blank header lines the cursor may need to move up
     * more than one line. Figure out how far.
     */
    e = ods.cur_e;
    l = ods.cur_l;
    while(l != new_l){
	if((l = prev_hline(&e, l)) != NULL)
	  incr++;
	else
	  break;  /* can't happen */
    }

    ods.p_line -= incr;
    fullpaint = ods.p_line <= COMPOSER_TOP_LINE;

    if(new_e != ods.cur_e){			/* new field ! */
	InvertPrompt(ods.cur_e, FALSE);
	if(headents[ods.cur_e].is_attach){
	    if(status = FormatSyncAttach()){   /* non-zero ? reformat field */
		headents[ods.cur_e].rich_header = 0;
		if(FormatLines(headents[ods.cur_e].hd_text, "",
			       term.t_ncol - headents[ods.cur_e].prlen,
			       headents[ods.cur_e].break_on_comma,0) == -1)
		  emlwrite("\007Format lines failed!", NULL);
	    }
	}
	else if(headents[ods.cur_e].builder){
	    int mangled = 0;
	    char *err = NULL;

	    if((status = call_builder(&headents[ods.cur_e], &mangled,
				      &err)) >= 0){
		/* repair new_l */
		for(new_l = headents[new_e].hd_text;
		    new_l->next;
		    new_l=new_l->next)
		  ;

		/* and cur_l (required in fix_and... */
		ods.cur_l = new_l;
	    }
	    else{
		++ods.p_line;
		fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
		InvertPrompt(ods.cur_e, TRUE);
		return(0);
	    }

	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
	}

	InvertPrompt(new_e, TRUE);
	sgarbk = 1;
    }

    ods.cur_e = new_e;				/* update pointers */
    ods.cur_l = new_l;
    if(ods.p_off > (len = strlen(ods.cur_l->text)))
      ods.p_off = len;

    if(status > 0 || fullpaint){
	UpdateHeader(0);
	PaintHeader(COMPOSER_TOP_LINE, FALSE);
	PaintBody(1);
    }

    return(1);
}


/*
 * 
 */
int
AppendAttachment(fn, sz, cmt)
    char *fn, *sz, *cmt;
{
    int	 a_e, status, spaces;
    struct hdr_line *lp;

    /*--- Find headerentry that is attachments (only first) --*/
    for(a_e = 0; headents[a_e].name != NULL; a_e++ )
      if(headents[a_e].is_attach){
	  /* make sure field stays displayed */
	  headents[a_e].rich_header = 0;
	  headents[a_e].display_it = 1;
	  break;
      }

    /* append new attachment line */
    for(lp = headents[a_e].hd_text; lp->next; lp=lp->next)
      ;

    /* build new attachment line */
    if(lp->text[0]){		/* adding a line? */
	strcat(lp->text, ",");	/* append delimiter */
	if(lp->next = HALLOC()){	/* allocate new line */
	    lp->next->prev = lp;
	    lp->next->next = NULL;
	    lp = lp->next;
	}
	else{
	    emlwrite("\007Can't allocate line for new attachment!", NULL);
	    return(0);
	}
    }

    
    spaces = (*fn == '\"') ? 0 : (strpbrk(fn, "(), \t") != NULL);
    sprintf(lp->text, "%s%s%s (%s) \"%.*s\"",
	    spaces ? "\"" : "", fn, spaces ? "\"" : "",
	    sz ? sz : "", 80, cmt ? cmt : "");

    /* validate the new attachment, and reformat if needed */
    if(status = SyncAttach()){
	if(status < 0)
	  emlwrite("\007Problem attaching: %s", fn);

	if(FormatLines(headents[a_e].hd_text, "",
		       term.t_ncol - headents[a_e].prlen,
		       headents[a_e].break_on_comma, 0) == -1){
	    emlwrite("\007Format lines failed!", NULL);
	    return(0);
	}
    }

    UpdateHeader(0);
    PaintHeader(COMPOSER_TOP_LINE, status != 0);
    PaintBody(1);
    return(status != 0);
}




/*
 * LineEdit - the idea is to manage 7 bit ascii character only input.
 *            Always use insert mode and handle line wrapping
 *
 *	returns:
 *		Any characters typed in that aren't printable 
 *		(i.e. commands)
 *
 *	notes: 
 *		Assume we are guaranteed that there is sufficiently 
 *		more buffer space in a line than screen width (just one 
 *		less thing to worry about).  If you want to change this,
 *		then pputc will have to be taught to check the line buffer
 *		length, and HALLOC() will probably have to become a func.
 */
LineEdit(allowedit)
int	allowedit;
{
    register struct	hdr_line   *lp;		/* temporary line pointer    */
    register int	i;
    register int	ch = 0;
    register int	status;			/* various func's return val */
    register char	*tbufp;			/* temporary buffer pointers */
	     int	skipmove = 0;
             char	*strng;
    int      last_key;				/* last keystroke  */

    strng   = ods.cur_l->text;			/* initialize offsets */
    ods.p_len = strlen(strng);
    if(ods.p_off < 0)				/* offset within range? */
      ods.p_off = 0;
    else if(ods.p_off > ods.p_len)
      ods.p_off = ods.p_len;
    else if(ods.p_off > LINELEN())		/* shouldn't happen, but */
        ods.p_off = LINELEN();			/* you never know...     */

    while(1){					/* edit the line... */

	if(skipmove)
	  skipmove = 0;
	else
	  HeaderPaintCursor();

	last_key = ch;

	(*term.t_flush)();			/* get everything out */

#ifdef MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);
	register_mfunc(mouse_in_content,2,0,term.t_nrow-(term.t_mrow+1),
		       term.t_ncol);
#endif
#ifdef	_WINDOWS
	mswin_setdndcallback (composer_file_drop);
	mswin_mousetrackcallback(pico_cursor);
#endif

	ch = GetKey();
	
	if (term.t_nrow < 6 && ch != NODATA){
	    (*term.t_beep)();
	    emlwrite("Please make the screen bigger.", NULL);
	    continue;
	}

#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_cleardndcallback ();
	mswin_mousetrackcallback(NULL);
#endif

	switch(ch){
	  case DEL :
	    if(gmode & P_DELRUBS)
	      ch = KEY_DEL;

	  default :
	    (*Pmaster->keybinput)();
	    if(!time_to_check())
	      break;

	  case NODATA :			/* new mail ? */
	    if((*Pmaster->newmail)(ch == NODATA ? 0 : 2, 1) >= 0){
		int rv;
		
		if(km_popped){
		    term.t_mrow = 2;
		    curwp->w_ntrows -= 2;
		}

		clearcursor();
		mlerase();
		rv = (*Pmaster->showmsg)(ch);
		ttresize();
		picosigs();
		if(rv)		/* Did showmsg corrupt the display? */
		  PaintBody(0);	/* Yes, repaint */

		mpresf = 1;
		if(km_popped){
		    term.t_mrow = 0;
		    curwp->w_ntrows += 2;
		}
	    }

	    clearcursor();
	    movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);
	    if(ch == NODATA)			/* GetKey timed out */
	      continue;

	    break;
	}

        if(mpresf){				/* blast old messages */
	    if(mpresf++ > NMMESSDELAY){		/* every few keystrokes */
		mlerase();
		movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);
	    }
        }

	if(VALID_KEY(ch)){			/* char input */
            /*
             * if we are allowing editing, insert the new char
             * end up leaving tbufp pointing to newly
             * inserted character in string, and offset to the
             * index of the character after the inserted ch ...
             */
            if(allowedit){
		if(headents[ods.cur_e].only_file_chars
		   && !fallowc((unsigned char) ch)){
		    /* no garbage in filenames */
		    emlwrite("\007Can't have a '%c' in folder name",
			     (void *) ch);
		    continue;
		}
		else if(headents[ods.cur_e].is_attach
			&& intag(strng,ods.p_off)){
		    emlwrite("\007Can't edit attachment number!", NULL);
		    continue;
		}

		if(headents[ods.cur_e].single_space){
		    if(ch == ' ' 
		       && (strng[ods.p_off]==' ' || strng[ods.p_off-1]==' '))
		      continue;
		}

		/*
		 * go ahead and add the character...
		 */
		tbufp = &strng[++ods.p_len];	/* find the end */
		do{
		    *tbufp = tbufp[-1];
		} while(--tbufp > &strng[ods.p_off]);	/* shift right */
		strng[ods.p_off++] = ch;	/* add char to str */

		/* mark this entry dirty */
		headents[ods.cur_e].sticky = 1;
		headents[ods.cur_e].dirty  = 1;

		/*
		 * then find out where things fit...
		 */
		if(ods.p_len < LINELEN()){
		    CELL c;

		    c.c = ch;
		    c.a = 0;
		    if(pinsert(c)){		/* add char to str */
			skipmove++;		/* must'a been optimal */
			continue; 		/* on to the next! */
		    }
		}
		else{
                    if((status = FormatLines(ods.cur_l, "", LINELEN(), 
    			        headents[ods.cur_e].break_on_comma,0)) == -1){
                        (*term.t_beep)();
                        continue;
                    }
                    else{
			/*
			 * during the format, the dot may have moved
			 * down to the next line...
			 */
			if(ods.p_off >= strlen(strng)){
			    ods.p_line++;
			    ods.p_off -= strlen(strng);
			    ods.cur_l = ods.cur_l->next;
			    strng = ods.cur_l->text;
			}
			ods.p_len = strlen(strng);
		    }
		    UpdateHeader(0);
		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    PaintBody(1);
                    continue;
		}
            }
            else{  
                rdonly();
                continue;
            } 
        }
        else {					/* interpret ch as a command */
            switch (ch = normalize_cmd(ch, ckm, 2)) {
	      case (CTRL|'@') :		/* word skip */
		while(strng[ods.p_off]
		      && isalnum((unsigned char)strng[ods.p_off]))
		  ods.p_off++;		/* skip any text we're in */

		while(strng[ods.p_off]
		      && !isalnum((unsigned char)strng[ods.p_off]))
		  ods.p_off++;		/* skip any whitespace after it */

		if(strng[ods.p_off] == '\0'){
		    ods.p_off = 0;	/* end of line, let caller handle it */
		    return(KEY_DOWN);
		}

		continue;

	      case (CTRL|'K') :			/* kill line cursor's on */
		if(!allowedit){
		    rdonly();
		    continue;
		}

		lp = ods.cur_l;
		if (!(gmode & MDDTKILL))
		  ods.p_off = 0;

		if(KillHeaderLine(lp, (last_key == (CTRL|'K')))){
		    if(optimize && 
		       !(ods.cur_l->prev==NULL && ods.cur_l->next==NULL)) 
		      scrollup(wheadp, ods.p_line, 1);

		    if(ods.cur_l->next == NULL)
		      if(zotcomma(ods.cur_l->text)){
			  if(ods.p_off > 0)
			    ods.p_off = strlen(ods.cur_l->text);
		      }
		    
		    i = (ods.p_line == COMPOSER_TOP_LINE);
		    UpdateHeader(0);
		    PaintHeader(COMPOSER_TOP_LINE, TRUE);

		    if(km_popped){
			km_popped--;
			movecursor(term.t_nrow, 0);
			peeol();
		    }

		    PaintBody(1);

		}
		strng = ods.cur_l->text;
		ods.p_len = strlen(strng);
		headents[ods.cur_e].sticky = 0;
		headents[ods.cur_e].dirty  = 1;
		continue;

	      case (CTRL|'U') :			/* un-delete deleted lines */
		if(!allowedit){
		    rdonly();
		    continue;
		}

		if(SaveHeaderLines()){
		    UpdateHeader(0);
		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    if(km_popped){
			km_popped--;
			movecursor(term.t_nrow, 0);
			peeol();
		    }

		    PaintBody(1);
		    strng = ods.cur_l->text;
		    ods.p_len = strlen(strng);
		    headents[ods.cur_e].sticky = 1;
		    headents[ods.cur_e].dirty  = 1;
		}
		else
		  emlwrite("Problem Unkilling text", NULL);
		continue;

	      case (CTRL|'F') :
	      case KEY_RIGHT:			/* move character right */
		if(ods.p_off < ods.p_len){
		    pputc(pscr(ods.p_line, 
			       (ods.p_off++)+headents[ods.cur_e].prlen)->c,0);
		    skipmove++;
		    continue;
		}
		else if(gmode & MDHDRONLY)
		  continue;

		ods.p_off = 0;
		return(KEY_DOWN);

	      case (CTRL|'B') :
	      case KEY_LEFT	:		/* move character left */
		if(ods.p_off > 0){
		    ods.p_off--;
		    continue;
		}
		if(ods.p_line != COMPOSER_TOP_LINE)
		  ods.p_off = 1000;		/* put cursor at end of line */
		return(KEY_UP);

	      case (CTRL|'M') :			/* goto next field */
		ods.p_off = 0;
		return(KEY_DOWN);

	      case KEY_HOME :
	      case (CTRL|'A') :			/* goto beginning of line */
		ods.p_off = 0;
		continue;

	      case KEY_END  :
	      case (CTRL|'E') :			/* goto end of line */
		ods.p_off = ods.p_len;
		continue;

	      case (CTRL|'D')   :		/* blast this char */
	      case KEY_DEL :
		if(!allowedit){
		    rdonly();
		    continue;
		}
		else if(ods.p_off >= strlen(strng))
		  continue;

		if(headents[ods.cur_e].is_attach && intag(strng, ods.p_off)){
		    emlwrite("\007Can't edit attachment number!", NULL);
		    continue;
		}

		pputc(strng[ods.p_off++], 0); 	/* drop through and rubout */

	      case DEL        :			/* blast previous char */
	      case (CTRL|'H') :
		if(!allowedit){
		    rdonly();
		    continue;
		}

		if(headents[ods.cur_e].is_attach && intag(strng, ods.p_off-1)){
		    emlwrite("\007Can't edit attachment number!", NULL);
		    continue;
		}

		if(ods.p_off > 0){		/* just shift left one char */
		    ods.p_len--;
		    headents[ods.cur_e].dirty  = 1;
		    if(ods.p_len == 0)
		      headents[ods.cur_e].sticky = 0;
		    else
		      headents[ods.cur_e].sticky = 1;

		    tbufp = &strng[--ods.p_off];
		    while(*tbufp++ != '\0')
		      tbufp[-1] = *tbufp;
		    tbufp = &strng[ods.p_off];
		    if(pdel())			/* physical screen delete */
		      skipmove++;		/* must'a been optimal */
		}
		else{				/* may have work to do */
		    if(ods.cur_l->prev == NULL){  
			(*term.t_beep)();	/* no erase into next field */
			continue;
		    }

		    ods.p_line--;
		    ods.cur_l = ods.cur_l->prev;
		    strng = ods.cur_l->text;
		    if((i=strlen(strng)) > 0){
			strng[i-1] = '\0';	/* erase the character */
			ods.p_off = i-1;
		    }
		    else{
			headents[ods.cur_e].sticky = 0;
			ods.p_off = 0;
		    }
		    
		    tbufp = &strng[ods.p_off];
		}

		if((status = FormatLines(ods.cur_l, "", LINELEN(), 
				   headents[ods.cur_e].break_on_comma,0))==-1){
		    (*term.t_beep)();
		    continue;
		}
		else{
		    /*
		     * beware, the dot may have moved...
		     */
		    while((ods.p_len=strlen(strng)) < ods.p_off){
			ods.p_line++;
			ods.p_off -= strlen(strng);
			ods.cur_l = ods.cur_l->next;
			strng = ods.cur_l->text;
			ods.p_len = strlen(strng);
			tbufp = &strng[ods.p_off];
			status = TRUE;
		    }
		    UpdateHeader(0);
		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    if(status == TRUE)
		      PaintBody(1);
		}

		movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);

		if(skipmove)
		  continue;

		break;

              default   :
		return(ch);
            }
        }

	while (*tbufp != '\0')		/* synchronizing loop */
	  pputc(*tbufp++, 0);

	if(ods.p_len < LINELEN())
	  peeol();

    }
}


void
HeaderPaintCursor()
{
    movecursor(ods.p_line, ods.p_off+headents[ods.cur_e].prlen);
}



/*
 * FormatLines - Place the given text at the front of the given line->text
 *               making sure to properly format the line, then check
 *               all lines below for proper format.
 *
 *	notes:
 *		Not much optimization at all.  Right now, it recursively
 *		fixes all remaining lines in the entry.  Some speed might
 *		gained if this was built to iteratively scan the lines.
 *
 *	returns:
 *		-1 on error
 *		FALSE if only this line is changed
 *		TRUE  if text below the first line is changed
 */
FormatLines(h, instr, maxlen, break_on_comma, quoted)
struct  hdr_line  *h;				/* where to begin formatting */
char	*instr;					/* input string */
int	maxlen;					/* max chars on a line */
int	break_on_comma;				/* break lines on commas */
int	quoted;					/* this line inside quotes */
{
    int		retval = FALSE;
    register	int	i, l;
    char	*ostr;				/* pointer to output string */
    register	char	*breakp;		/* pointer to line break */
    register	char	*bp, *tp;		/* temporary pointers */
    char	*buf;				/* string to add later */
    struct hdr_line	*nlp, *lp;

    ostr = h->text;
    nlp = h->next;
    l = strlen(instr) + strlen(ostr);
    if((buf = (char *)malloc(l+10)) == NULL)
      return(-1);

    if(l >= maxlen){				/* break then fixup below */

	if((l=strlen(instr)) < maxlen){		/* room for more */

	    if(break_on_comma && (bp = (char *)strqchr(instr, ',', &quoted, -1))){
		bp += (bp[1] == ' ') ? 2 : 1;
		for(tp = bp; *tp && *tp == ' '; tp++)
		  ;

		strcpy(buf, tp);
		strcat(buf, ostr);
		for(i = 0; &instr[i] < bp; i++)
		  ostr[i] = instr[i];
		ostr[i] = '\0';
		retval = TRUE;
	    }
	    else{
		breakp = break_point(ostr, maxlen-strlen(instr),
				     break_on_comma ? ',' : ' ',
				     break_on_comma ? &quoted : NULL);

		if(breakp == ostr){	/* no good breakpoint */
		    if(break_on_comma && *breakp == ','){
			breakp = ostr + 1;
			retval = TRUE;
		    }
		    else if(strchr(instr,(break_on_comma && !quoted)?',':' ')){
			strcpy(buf, ostr);
			strcpy(ostr, instr);
		    }
		    else{		/* instr's broken as we can get it */
			breakp = &ostr[maxlen-strlen(instr)-1];
			retval = TRUE;
		    }
		}
		else
		  retval = TRUE;
	    
		if(retval){
		    strcpy(buf, breakp);	/* save broken line  */
		    if(breakp == ostr){
			strcpy(ostr, instr);	/* simple if no break */
		    }
		    else{
			*breakp = '\0';		/* more work to break it */
			i = strlen(instr);
			/*
			 * shift ostr i chars
			 */
			for(bp=breakp; bp >= ostr && i; bp--)
			  *(bp+i) = *bp;
			for(tp=ostr, bp=instr; *bp != '\0'; tp++, bp++)
			  *tp = *bp;		/* then add instr */
		    }
		}
	    }
	}
	/*
	 * Short-circuit the recursion in this case.
	 * No time right now to figure out how to do it better.
	 */
	else if(l > 2*maxlen){
	    char *instrp, *saveostr = NULL;

	    retval = TRUE;
	    if(ostr && *ostr){
		saveostr = (char *)malloc(strlen(ostr) + 1);
		strcpy(saveostr, ostr);
		ostr[0] = '\0';
	    }

	    instrp = instr;
	    while(l > 2*maxlen){
		if(break_on_comma || maxlen == 1){
		    breakp = (!(bp = strqchr(instrp, ',', &quoted, maxlen))
			      || bp - instrp >= maxlen || maxlen == 1)
			       ? &instrp[maxlen]
			       : bp + ((bp[1] == ' ') ? 2 : 1);
		}
		else{
		    breakp = break_point(instrp, maxlen, ' ', NULL);

		    if(breakp == instrp)	/* no good break point */
		      breakp = &instrp[maxlen - 1];
		}
		
		for(tp=ostr,bp=instrp; bp < breakp; tp++, bp++)
		  *tp = *bp;

		*tp = '\0';
		l -= (breakp - instrp);
		instrp = breakp;

		if((lp = HALLOC()) == NULL){
		    emlwrite("Can't allocate any more lines for header!", NULL);
		    free(buf);
		    return(-1);
		}

		lp->next = h->next;
		if(h->next)
		  h->next->prev = lp;

		h->next = lp;
		lp->prev = h;
		lp->text[0] = '\0';
		h = h->next;
		ostr = h->text;
	    }

	    /*
	     * Now l is still > maxlen. Do it the recursive way,
	     * like the else clause below. Surely we could fix up the
	     * flow control some here, but this works for now.
	     */

	    nlp = h->next;
	    instr = instrp;
	    if(saveostr){
		strcpy(ostr, saveostr);
		free(saveostr);
	    }

	    if(break_on_comma || maxlen == 1){
		breakp = (!(bp = strqchr(instrp, ',', &quoted, maxlen))
			  || bp - instrp >= maxlen || maxlen == 1)
			   ? &instrp[maxlen]
			   : bp + ((bp[1] == ' ') ? 2 : 1);
	    }
	    else{
		breakp = break_point(instrp, maxlen, ' ', NULL);

		if(breakp == instrp)		/* no good break point */
		  breakp = &instrp[maxlen - 1];
	    }
	    
	    strcpy(buf, breakp);		/* save broken line */
	    strcat(buf, ostr);			/* add line that was there */
	    for(tp=ostr,bp=instr; bp < breakp; tp++, bp++)
	      *tp = *bp;

	    *tp = '\0';
	}
	else{					/* instr > maxlen ! */
	    if(break_on_comma || maxlen == 1){
		breakp = (!(bp = strqchr(instr, ',', &quoted, maxlen))
			  || bp - instr >= maxlen || maxlen == 1)
			   ? &instr[maxlen]
			   : bp + ((bp[1] == ' ') ? 2 : 1);
	    }
	    else{
		breakp = break_point(instr, maxlen, ' ', NULL);

		if(breakp == instr)		/* no good break point */
		  breakp = &instr[maxlen - 1];
	    }
	    
	    strcpy(buf, breakp);		/* save broken line */
	    strcat(buf, ostr);			/* add line that was there */
	    for(tp=ostr,bp=instr; bp < breakp; tp++, bp++)
	      *tp = *bp;

	    *tp = '\0';
	}

	if(nlp == NULL){			/* no place to add below? */
	    if((lp = HALLOC()) == NULL){
		emlwrite("Can't allocate any more lines for header!", NULL);
		free(buf);
		return(-1);
	    }

	    if(optimize && (i = physical_line(h)) != -1)
	      scrolldown(wheadp, i - 1, 1);

	    h->next = lp;			/* fix up links */
	    lp->prev = h;
	    lp->next = NULL;
	    lp->text[0] = '\0';
	    nlp = lp;
	    retval = TRUE;
	}
    }
    else{					/* combined length < max */
	if(*instr){
	     strcpy(buf, instr);		/* insert instr before ostr */
	     strcat(buf, ostr);
	     strcpy(ostr, buf);
	}

	*buf = '\0';
	breakp = NULL;

	if(break_on_comma && (breakp = strqchr(ostr, ',', &quoted, -1))){
	    breakp += (breakp[1] == ' ') ? 2 : 1;
	    strcpy(buf, breakp);
	    *breakp = '\0';

	    if(strlen(buf) && !nlp){
		if((lp = HALLOC()) == NULL){
		    emlwrite("Can't allocate any more lines for header!",NULL);
		    free(buf);
		    return(-1);
		}

		if(optimize && (i = physical_line(h)) != -1)
		  scrolldown(wheadp, i - 1, 1);

		h->next = lp;		/* fix up links */
		lp->prev = h;
		lp->next = NULL;
		lp->text[0] = '\0';
		nlp = lp;
		retval = TRUE;
	    }
	}

	if(nlp){
	    if(!strlen(buf) && !breakp){
		if(strlen(ostr) + strlen(nlp->text) >= maxlen){
		    breakp = break_point(nlp->text, maxlen-strlen(ostr), 
					 break_on_comma ? ',' : ' ',
					 break_on_comma ? &quoted : NULL);
		    
		    if(breakp == nlp->text){	/* commas this line? */
			for(tp=ostr; *tp  && *tp != ' '; tp++)
			  ;

			if(!*tp){		/* no commas, get next best */
			    breakp += maxlen - strlen(ostr) - 1;
			    retval = TRUE;
			}
			else
			  retval = FALSE;
		    }
		    else
		      retval = TRUE;

		    if(retval){			/* only if something to do */
			for(tp = &ostr[strlen(ostr)],bp=nlp->text; bp<breakp; 
			tp++, bp++)
			  *tp = *bp;		/* add breakp to this line */
			*tp = '\0';
			for(tp=nlp->text, bp=breakp; *bp != '\0'; tp++, bp++)
			  *tp = *bp;		/* shift next line to left */
			*tp = '\0';
		    }
		}
		else{
		    strcat(ostr, nlp->text);

		    if(optimize && (i = physical_line(nlp)) != -1)
		      scrollup(wheadp, i, 1);

		    hldelete(nlp);

		    if(!(nlp = h->next)){
			free(buf);
			return(TRUE);		/* can't go further */
		    }
		    else
		      retval = TRUE;		/* more work to do? */
		}
	    }
	}
	else{
	    free(buf);
	    return(FALSE);
	}

    }

    i = FormatLines(nlp, buf, maxlen, break_on_comma, quoted);
    free(buf);
    switch(i){
      case -1:					/* bubble up worst case */
	return(-1);
      case FALSE:
	if(retval == FALSE)
	  return(FALSE);
      default:
	return(TRUE);
    }
}

/*
 * Format the lines before parsing attachments so we
 * don't expand a bunch of attachments that we don't
 * have the buffer space for.
 */
int
FormatSyncAttach ()
{
    FormatLines(headents[ods.cur_e].hd_text, "",
		term.t_ncol - headents[ods.cur_e].prlen,
		headents[ods.cur_e].break_on_comma, 0);
    return(SyncAttach());
}


/*
 * PaintHeader - do the work of displaying the header from the given 
 *               physical screen line the end of the header.
 *
 *       17 July 91 - fixed reshow to deal with arbitrarily large headers.
 */
void
PaintHeader(line, clear)
    int	line;					/* physical line on screen   */
    int	clear;					/* clear before painting */
{
    register struct hdr_line	*lp;
    register char	*bufp;
    register int	curline;
    register int	curoffset;
    char     buf[NLINE];
    int      e;

    if(clear)
      pclear(COMPOSER_TOP_LINE, ComposerTopLine);

    curline   = COMPOSER_TOP_LINE;
    curoffset = 0;

    for(lp = ods.top_l, e = ods.top_e; ; curline++){
	if((curline == line) || ((lp = next_hline(&e, lp)) == NULL))
	  break;
    }

    while(headents[e].name != NULL){			/* begin to redraw */
	while(lp != NULL){
	    buf[0] = '\0';
            if((!lp->prev || curline == COMPOSER_TOP_LINE) && !curoffset){
	        if(InvertPrompt(e, (e == ods.cur_e && ComposerEditing)) == -1
		   && !is_blank(curline, 0, headents[e].prlen))
		   sprintf(buf, "%*s", headents[e].prlen, " ");
	    }
	    else if(!is_blank(curline, 0, headents[e].prlen))
	      sprintf(buf, "%*s", headents[e].prlen, " ");

	    if(*(bufp = buf) != '\0'){		/* need to paint? */
		movecursor(curline, 0);		/* paint the line... */
		while(*bufp != '\0')
		  pputc(*bufp++, 0);
	    }

	    bufp = &(lp->text[curoffset]);	/* skip chars already there */
	    curoffset += headents[e].prlen;
	    while(pscr(curline, curoffset) != NULL &&
		  *bufp == pscr(curline, curoffset)->c && *bufp != '\0'){
		bufp++;
		if(++curoffset >= term.t_ncol)
		  break;
	    }

	    if(*bufp != '\0'){			/* need to move? */
		movecursor(curline, curoffset);
		while(*bufp != '\0'){		/* display what's not there */
		    pputc(*bufp++, 0);
		    curoffset++;
		}
	    }

	    if(curoffset < term.t_ncol 
	       && !is_blank(curline, curoffset, term.t_ncol - curoffset)){
		     movecursor(curline, curoffset);
		     peeol();
	    }
	    curline++;

            curoffset = 0;
	    if(curline >= BOTTOM())
	      break;

	    lp = lp->next;
        }

	if(curline >= BOTTOM())
	  return;				/* don't paint delimiter */

	while(headents[++e].name != NULL)
	  if(headents[e].display_it){
	      lp = headents[e].hd_text;
	      break;
	  }
    }

    display_delimiter(ComposerEditing ? 0 : 1);
}




/*
 * PaintBody() - generic call to handle repainting everything BUT the 
 *		 header
 *
 *	notes:
 *		The header redrawing in a level 0 body paint gets done
 *		in update()
 */
void
PaintBody(level)
int	level;
{
    curwp->w_flag |= WFHARD;			/* make sure framing's right */
    if(level == 0)				/* specify what to update */
        sgarbf = TRUE;

    update();					/* display message body */

    if(level == 0 && ComposerEditing){
	mlerase();				/* clear the error line */
	ShowPrompt();
    }
}


/*
 * display_for_send - paint the composer from the top line and return.
 */
void
display_for_send()
{
    int		     i = 0;
    struct hdr_line *l;

    /* if first header line isn't displayed, there's work to do */
    if(headents && ((l = first_hline(&i)) != ods.top_l
		    || ComposerTopLine == COMPOSER_TOP_LINE
		    || !ods.p_line)){
	struct on_display orig_ods;
	int		  orig_edit    = ComposerEditing,
			  orig_ct_line = ComposerTopLine;

	/*
	 * fake that the cursor's in the first header line
	 * and force repaint...
	 */
	orig_ods	= ods;
	ods.cur_e	= i;
	ods.top_l	= ods.cur_l = l;
	ods.top_e	= ods.cur_e;
	ods.p_line	= COMPOSER_TOP_LINE;
	ComposerEditing = TRUE;			/* to fool update() */
	setimark(FALSE, 1);			/* remember where we were */
	gotobob(FALSE, 1);

	UpdateHeader(0);			/* redraw whole enchilada */
	PaintHeader(COMPOSER_TOP_LINE, TRUE);
	PaintBody(0);

	ods = orig_ods;				/* restore original state */
	ComposerEditing = orig_edit;
	ComposerTopLine = curwp->w_toprow = orig_ct_line;
        curwp->w_ntrows = BOTTOM() - ComposerTopLine;
	swapimark(FALSE, 1);

	/* in case we don't exit, set up restoring the screen */
	sgarbf = TRUE;				/* force redraw */
    }
}


/*
 * ArrangeHeader - set up display parm such that header is reasonably 
 *                 displayed
 */
void
ArrangeHeader()
{
    int      e;
    register struct hdr_line *l;

    ods.p_line = ods.p_off = 0;
    e = ods.top_e = 0;
    l = ods.top_l = headents[e].hd_text;
    while(headents[e+1].name || (l && l->next))
      if(l = next_sel_hline(&e, l)){
	  ods.cur_l = l;
	  ods.cur_e = e;
      }

    UpdateHeader(1);
}


/*
 * ComposerHelp() - display mail help in a context sensitive way
 *                  based on the level passed ...
 */
ComposerHelp(level)
int	level;
{
    char buf[80];
    VARS_TO_SAVE *saved_state;

    curwp->w_flag |= WFMODE;
    sgarbf = TRUE;

    if(level < 0 || !headents[level].name){
	(*term.t_beep)();
	emlwrite("Sorry, I can't help you with that.", NULL);
	sleep(2);
	return(FALSE);
    }

    sprintf(buf, "Help for %s %.40s Field",
		 (Pmaster->pine_flags & MDHDRONLY) ? "Address Book"
						 : "Composer",
		 headents[level].name);
    saved_state = save_pico_state();
    (*Pmaster->helper)(headents[level].help, buf, 1);
    if(saved_state){
	restore_pico_state(saved_state);
	free_pico_state(saved_state);
    }

    ttresize();
    picosigs();					/* restore altered handlers */
    return(TRUE);
}



/*
 * ToggleHeader() - set or unset pico values to the full screen size
 *                  painting header if need be.
 */
ToggleHeader(show)
int show;
{
    /*
     * check to see if we need to display the header... 
     */
    if(show){
	UpdateHeader(0);				/* figure bounds  */
	PaintHeader(COMPOSER_TOP_LINE, FALSE);	/* draw it */
    }
    else{
        /*
         * set bounds for no header display
         */
        curwp->w_toprow = ComposerTopLine = COMPOSER_TOP_LINE;
        curwp->w_ntrows = BOTTOM() - ComposerTopLine;
    }
    return(TRUE);
}



/*
 * HeaderLen() - return the length in lines of the exposed portion of the
 *               header
 */
HeaderLen()
{
    register struct hdr_line *lp;
    int      e;
    int      i;
    
    i = 1;
    lp = ods.top_l;
    e  = ods.top_e;
    while(lp != NULL){
	lp = next_hline(&e, lp);
	i++;
    }
    return(i);
}



/*
 * first_hline() - return a pointer to the first displayable header line
 * 
 *	returns:
 *		1) pointer to first displayable line in header and header
 *                 entry, via side effect, that the first line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
first_hline(entry)
    int *entry;
{
    /* init *entry so we're sure to start from the top */
    for(*entry = 0; headents[*entry].name; (*entry)++)
      if(headents[*entry].display_it)
	return(headents[*entry].hd_text);

    *entry = 0;
    return(NULL);		/* this shouldn't happen */
}


/*
 * first_sel_hline() - return a pointer to the first selectable header line
 * 
 *	returns:
 *		1) pointer to first selectable line in header and header
 *                 entry, via side effect, that the first line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
first_sel_hline(entry)
    int *entry;
{
    /* init *entry so we're sure to start from the top */
    for(*entry = 0; headents[*entry].name; (*entry)++)
      if(headents[*entry].display_it && !headents[*entry].blank)
	return(headents[*entry].hd_text);

    *entry = 0;
    return(NULL);		/* this shouldn't happen */
}



/*
 * next_hline() - return a pointer to the next line structure
 * 
 *	returns:
 *		1) pointer to next displayable line in header and header
 *                 entry, via side effect, that the next line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
next_hline(entry, line)
    int *entry;
    struct hdr_line *line;
{
    if(line == NULL)
      return(NULL);

    if(line->next == NULL){
	while(headents[++(*entry)].name != NULL){
	    if(headents[*entry].display_it)
	      return(headents[*entry].hd_text);
	}
	--(*entry);
	return(NULL);
    }
    else
      return(line->next);
}


/*
 * next_sel_hline() - return a pointer to the next selectable line structure
 * 
 *	returns:
 *		1) pointer to next selectable line in header and header
 *                 entry, via side effect, that the next line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
next_sel_hline(entry, line)
    int *entry;
    struct hdr_line *line;
{
    if(line == NULL)
      return(NULL);

    if(line->next == NULL){
	while(headents[++(*entry)].name != NULL){
	    if(headents[*entry].display_it && !headents[*entry].blank)
	      return(headents[*entry].hd_text);
	}
	--(*entry);
	return(NULL);
    }
    else
      return(line->next);
}



/*
 * prev_hline() - return a pointer to the next line structure back
 * 
 *	returns:
 *              1) pointer to previous displayable line in header and 
 *                 the header entry that the next line is a part of 
 *                 via side effect
 *              2) NULL if no next line, leaving entry unchanged from
 *                 the value it had on entry.
 */
struct hdr_line *
prev_hline(entry, line)
    int *entry;
    struct hdr_line *line;
{
    if(line == NULL)
      return(NULL);

    if(line->prev == NULL){
	int orig_entry;

	orig_entry = *entry;
	while(--(*entry) >= 0){
	    if(headents[*entry].display_it){
		line = headents[*entry].hd_text;
		while(line->next != NULL)
		  line = line->next;
		return(line);
	    }
	}

	*entry = orig_entry;
	return(NULL);
    }
    else
      return(line->prev);
}


/*
 * prev_sel_hline() - return a pointer to the previous selectable line
 * 
 *	returns:
 *              1) pointer to previous selectable line in header and 
 *                 the header entry that the next line is a part of 
 *                 via side effect
 *              2) NULL if no next line, leaving entry unchanged from
 *                 the value it had on entry.
 */
struct hdr_line *
prev_sel_hline(entry, line)
    int *entry;
    struct hdr_line *line;
{
    if(line == NULL)
      return(NULL);

    if(line->prev == NULL){
	int orig_entry;

	orig_entry = *entry;
	while(--(*entry) >= 0){
	    if(headents[*entry].display_it && !headents[*entry].blank){
		line = headents[*entry].hd_text;
		while(line->next != NULL)
		  line = line->next;
		return(line);
	    }
	}

	*entry = orig_entry;
	return(NULL);
    }
    else
      return(line->prev);
}



/*
 * first_requested_hline() - return pointer to first line that pico's caller
 *			     asked that we start on.
 */
struct hdr_line *
first_requested_hline(ent)
    int *ent;
{
    int		     i, reqfield;
    struct hdr_line *rv = NULL;

    for(reqfield = -1, i = 0; headents[i].name;  i++)
      if(headents[i].start_here){
	  headents[i].start_here = 0;		/* clear old setting */
	  if(reqfield < 0){			/* if not already, set up */
	      headents[i].display_it = 1;	/* make sure it's shown */
	      *ent = reqfield = i;
	      rv = headents[i].hd_text;
	  }
      }

    return(rv);
}



/*
 * UpdateHeader() - determines the best range of lines to be displayed 
 *                  using the global ods value for the current line and the
 *		    top line, also sets ComposerTopLine and pico limits
 *
 *	showtop -- Attempt to show all header lines if they'll fit.
 *                    
 *      notes:
 *	        This is pretty ugly because it has to keep the current line
 *		on the screen in a reasonable location no matter what.
 *		There are also a couple of rules to follow:
 *                 1) follow paging conventions of pico (ie, half page 
 *		      scroll)
 *                 2) if more than one page, always display last half when 
 *                    pline is toward the end of the header
 * 
 *      returns:
 *             TRUE  if anything changed (side effects: new p_line, top_l
 *		     top_e, and pico parms)
 *             FALSE if nothing changed 
 *             
 */
UpdateHeader(showtop)
    int showtop;
{
    register struct	hdr_line	*lp;
    int	     i, le;
    int      ret = FALSE;
    int      old_top = ComposerTopLine;
    int      old_p = ods.p_line;

    if(ods.p_line < COMPOSER_TOP_LINE ||
       ((ods.p_line == ComposerTopLine-2) ? 2: 0) + ods.p_line >= BOTTOM()){
        /* NewTop if cur header line is at bottom of screen or two from */
        /* the bottom of the screen if cur line is bottom header line */
	NewTop(showtop);			/* get new top_l */
	ret = TRUE;
    }
    else{					/* make sure p_line's OK */
	i = COMPOSER_TOP_LINE;
	lp = ods.top_l;
	le = ods.top_e;
	while(lp != ods.cur_l){
	    /*
	     * this checks to make sure cur_l is below top_l and that
	     * cur_l is on the screen...
	     */
	    if((lp = next_hline(&le, lp)) == NULL || ++i >= BOTTOM()){
		NewTop(0);
		ret = TRUE;
		break;
	    }
	}
    }

    ods.p_line = COMPOSER_TOP_LINE;		/* find  p_line... */
    lp = ods.top_l;
    le = ods.top_e;
    while(lp && lp != ods.cur_l){
	lp = next_hline(&le, lp);
	ods.p_line++;
    }

    if(!ret)
      ret = !(ods.p_line == old_p);

    ComposerTopLine = ods.p_line;		/* figure top composer line */
    while(lp && ComposerTopLine <= BOTTOM()){
	lp = next_hline(&le, lp);
	ComposerTopLine += (lp) ? 1 : 2;	/* allow for delim at end   */
    }

    if(!ret)
      ret = !(ComposerTopLine == old_top);

    if(wheadp->w_toprow != ComposerTopLine){	/* update pico params... */
        wheadp->w_toprow = ComposerTopLine;
        wheadp->w_ntrows = ((i = BOTTOM() - ComposerTopLine) > 0) ? i : 0;
	ret = TRUE;
    }
    return(ret);
}



/*
 * NewTop() - calculate a new top_l based on the cur_l
 *
 *	showtop -- Attempt to show all the header lines if they'll fit
 *
 *	returns:
 *		with ods.top_l and top_e pointing at a reasonable line
 *		entry
 */
void
NewTop(showtop)
    int showtop;
{
    register struct hdr_line *lp;
    register int i;
    int      e;

    lp = ods.cur_l;
    e  = ods.cur_e;
    i  = showtop ? FULL_SCR() : HALF_SCR();

    while(lp != NULL && --i){
	ods.top_l = lp;
	ods.top_e = e;
	lp = prev_hline(&e, lp);
    }
}



/*
 * display_delimiter() - just paint the header/message body delimiter with
 *                       inverse value specified by state.
 */
void
display_delimiter(state)
int	state;
{
    register char    *bufp;

    if(ComposerTopLine - 1 >= BOTTOM())		/* silently forget it */
      return;

    bufp = (gmode & MDHDRONLY) ? "" : HDR_DELIM;

    if(state == delim_ps){			/* optimize ? */
	for(delim_ps = 0; bufp[delim_ps] && pscr(ComposerTopLine-1,delim_ps) != NULL && pscr(ComposerTopLine-1,delim_ps)->c == bufp[delim_ps];delim_ps++)
	  ;

	if(bufp[delim_ps] == '\0' && !(gmode & MDHDRONLY)){
	    delim_ps = state;
	    return;				/* already displayed! */
	}
    }

    delim_ps = state;

    movecursor(ComposerTopLine - 1, 0);
    if(state)
      (*term.t_rev)(1);

    while(*bufp != '\0')
      pputc(*bufp++, state ? 1 : 0);

    if(state)
      (*term.t_rev)(0);

    peeol();
}



/*
 * InvertPrompt() - invert the prompt associated with header entry to state
 *                  state (true if invert, false otherwise).
 *	returns:
 *		non-zero if nothing done
 *		0 if prompt inverted successfully
 *
 *	notes:
 *		come to think of it, this func and the one above could
 *		easily be combined
 */
InvertPrompt(entry, state)
int	entry, state;
{
    register char   *bufp;
    register int    i;

    bufp = headents[entry].prompt;		/* fresh prompt paint */
    if((i = entry_line(entry, FALSE)) == -1)
      return(-1);				/* silently forget it */

    if(entry < 16 && (invert_ps&(1<<entry)) 
       == (state ? 1<<entry : 0)){	/* optimize ? */
	int j;

	for(j = 0; bufp[j] && pscr(i, j)->c == bufp[j]; j++)
	  ;

	if(bufp[j] == '\0'){
	    if(state)
	      invert_ps |= 1<<entry;
	    else
	      invert_ps &= ~(1<<entry);
	    return(0);				/* already displayed! */
	}
    }

    if(entry < 16){  /* if > 16, cannot be stored in invert_ps */
      if(state)
	invert_ps |= 1<<entry;
      else
	invert_ps &= ~(1<<entry);
    }

    movecursor(i, 0);
    if(state)
      (*term.t_rev)(1);

    while(*bufp && *(bufp + 1))
      pputc(*bufp++, 1);			/* putc upto last char */

    if(state)
      (*term.t_rev)(0);

    pputc(*bufp, 0);				/* last char not inverted */
    return(TRUE);
}




/*
 * partial_entries() - toggle display of the bcc and fcc fields.
 *
 *	returns:
 *		TRUE if there are partial entries on the display
 *		FALSE otherwise.
 */
partial_entries()
{
    register struct headerentry *h;
    int                          is_on;
  
    /*---- find out status of first rich header ---*/
    for(h = headents; !h->rich_header && h->name != NULL; h++)
      ;

    is_on = h->display_it;
    for(h = headents; h->name != NULL; h++) 
      if(h->rich_header) 
        h->display_it = ! is_on;

    return(is_on);
}



/*
 * entry_line() - return the physical line on the screen associated
 *                with the given header entry field.  Note: the field
 *                may span lines, so if the last char is set, return
 *                the appropriate value.
 *
 *	returns:
 *             1) physical line number of entry
 *             2) -1 if entry currently not on display
 */
entry_line(entry, lastchar)
int	entry, lastchar;
{
    register int    p_line = COMPOSER_TOP_LINE;
    int    i;
    register struct hdr_line    *line;

    for(line = ods.top_l, i = ods.top_e;
	headents && headents[i].name && i <= entry;
	p_line++){
	if(p_line >= BOTTOM())
	  break;
	if(i == entry){
	    if(lastchar){
		if(line->next == NULL)
		  return(p_line);
	    }
	    else if(line->prev == NULL)
	      return(p_line);
	    else
	      return(-1);
	}
	line = next_hline(&i, line);
    }
    return(-1);
}



/*
 * physical_line() - return the physical line on the screen associated
 *                   with the given header line pointer.
 *
 *	returns:
 *             1) physical line number of entry
 *             2) -1 if entry currently not on display
 */
physical_line(l)
struct hdr_line *l;
{
    register int    p_line = COMPOSER_TOP_LINE;
    register struct hdr_line    *lp;
    int    i;

    for(lp=ods.top_l, i=ods.top_e; headents[i].name && lp != NULL; p_line++){
	if(p_line >= BOTTOM())
	  break;

	if(lp == l)
	  return(p_line);

	lp = next_hline(&i, lp);
    }
    return(-1);
}



/*
 * call_builder() - resolve any nicknames in the address book associated
 *                  with the given entry...
 *
 *    NOTES:
 * 
 *      BEWARE: this function can cause cur_l and top_l to get lost so BE 
 *              CAREFUL before and after you call this function!!!
 * 
 *      There could to be something here to resolve cur_l and top_l
 *      reasonably into the new linked list for this entry.  
 *
 *      The reason this would mostly work without it is resolve_niks gets
 *      called for the most part in between fields.  Since we're moving
 *      to the beginning or end (i.e. the next/prev pointer in the old 
 *      freed cur_l is NULL) of the next entry, we get a new cur_l
 *      pointing at a good line.  Then since top_l is based on cur_l in
 *      NewTop() we have pretty much lucked out.
 * 
 *      Where we could get burned is in a canceled exit (ctrl|x).  Here
 *      nicknames get resolved into addresses, which invalidates cur_l
 *      and top_l.  Since we don't actually leave, we could begin editing
 *      again with bad pointers.  This would usually results in a nice 
 *      core dump.
 *
 *      NOTE: The mangled argument is a little strange. It's used on both
 *      input and output. On input, if it is not set, then that tells the
 *      builder not to do anything that might take a long time, like a
 *      white pages lookup. On return, it tells the caller that the screen
 *      and signals may have been mangled so signals should be reset, window
 *      resized, and screen redrawn.
 *
 *	RETURNS:
 *              > 0 if any names where resolved, otherwise
 *                0 if not, or
 *		< 0 on error
 *                -1: move to next line
 *                -2: don't move off this line
 */
call_builder(entry, mangled, err)
struct headerentry *entry;
int                *mangled;
char              **err;
{
    register    int     retval = 0;
    register	int	i;
    register    struct  hdr_line  *line;
    int          quoted = 0;
    char	*sbuf;
    char	*s = NULL;
    struct headerentry *e;
    BUILDER_ARG *nextarg, *arg = NULL, *headarg = NULL;
    VARS_TO_SAVE *saved_state;

    if(!entry->builder)
      return(0);

    line = entry->hd_text;
    i = 0;
    while(line != NULL){
	i += term.t_ncol;
        line = line->next;
    }
    
    if((sbuf=(char *)malloc((unsigned) i)) == NULL){
	emlwrite("Can't malloc space to expand address", NULL);
	return(-1);
    }
    
    *sbuf = '\0';
    /*
     * cat the whole entry into one string...
     */
    line = entry->hd_text;
    while(line != NULL){
	i = strlen(line->text);
	/*
	 * To keep pine address builder happy, addresses should be separated
	 * by ", ".  Add this space if needed, otherwise...
	 * (This is some ancient requirement that is no longer needed.)
	 *
	 * If this line is NOT a continuation of the previous line, add
	 * white space for pine's address builder if its not already there...
	 * (This is some ancient requirement that is no longer needed.)
	 *
	 * Also if it's not a continuation (i.e., there's already and addr on 
	 * the line), and there's another line below, treat the new line as
	 * an implied comma.
	 * (This should only be done for address-type lines, not for regular
	 * text lines like subjects. Key off of the break_on_comma bit which
	 * should only be set on those that won't mind a comma being added.)
	 */
	if(entry->break_on_comma){
	    if(i && line->text[i-1] == ',')
	      strcat(line->text, " ");		/* help address builder */
	    else if(line->next != NULL && !strend(line->text, ',')){
		if(strqchr(line->text, ',', &quoted, -1))
		  strcat(line->text, ", ");		/* implied comma */
	    }
	    else if(line->prev != NULL && line->next != NULL){
		if(strchr(line->prev->text, ' ') != NULL 
		   && line->text[i-1] != ' ')
		  strcat(line->text, " ");
	    }
	}

	strcat(sbuf, line->text);
        line = line->next;
    }

    if(entry->affected_entry){
	/* check if any non-sticky affected entries */
	for(e = entry->affected_entry; e; e = e->next_affected)
	  if(!e->sticky)
	    break;

	/* there is at least one non-sticky so make a list to pass */
	if(e){
	    for(e = entry->affected_entry; e; e = e->next_affected){
		if(!arg){
		    headarg = arg = (BUILDER_ARG *)malloc(sizeof(BUILDER_ARG));
		    if(!arg){
			emlwrite("Can't malloc space for fcc", NULL);
			return(-1);
		    }
		    else{
			arg->next = NULL;
			arg->tptr = NULL;
			arg->aff  = &(e->bldr_private);
			arg->me   = &(entry->bldr_private);
		    }
		}
		else{
		    nextarg = (BUILDER_ARG *)malloc(sizeof(BUILDER_ARG));
		    if(!nextarg){
			emlwrite("Can't malloc space for fcc", NULL);
			return(-1);
		    }
		    else{
			nextarg->next = NULL;
			nextarg->tptr = NULL;
			nextarg->aff  = &(e->bldr_private);
			nextarg->me   = &(entry->bldr_private);
			arg->next     = nextarg;
			arg           = arg->next;
		    }
		}

		if(!e->sticky){
		    line = e->hd_text;
		    if(!(arg->tptr=(char *)malloc(strlen(line->text) + 1))){
			emlwrite("Can't malloc space for fcc", NULL);
			return(-1);
		    }
		    else
		      strcpy(arg->tptr, line->text);
		}
	    }
	}
    }

    /*
     * Even if there are no affected entries, we still need the arg
     * to pass the "me" pointer.
     */
    if(!headarg){
	headarg = (BUILDER_ARG *)malloc(sizeof(BUILDER_ARG));
	if(!headarg){
	    emlwrite("Can't malloc space", NULL);
	    return(-1);
	}
	else{
	    headarg->next = NULL;
	    headarg->tptr = NULL;
	    headarg->aff  = NULL;
	    headarg->me   = &(entry->bldr_private);
	}
    }

    /*
     * The builder may make a new call back to pico() so we save and
     * restore the pico state.
     */
    saved_state = save_pico_state();
    retval = (*entry->builder)(sbuf, &s, err, headarg, mangled);
    if(saved_state){
	restore_pico_state(saved_state);
	free_pico_state(saved_state);
    }

    if(mangled && *mangled & BUILDER_MESSAGE_DISPLAYED){
	*mangled &= ~ BUILDER_MESSAGE_DISPLAYED;
	if(mpresf == FALSE)
	  mpresf = TRUE;
    }

    if(retval >= 0){
	if(strcmp(sbuf, s)){
	    line = entry->hd_text;
	    InitEntryText(s, entry);		/* arrange new one */
	    zotentry(line); 			/* blast old list o'entries */
	    entry->dirty = 1;			/* mark it dirty */
	    retval = 1;
	}

	for(e = entry->affected_entry, arg = headarg;
	    e;
	    e = e->next_affected, arg = arg ? arg->next : NULL){
	    if(!e->sticky){
		line = e->hd_text;
		if(strcmp(line->text, arg ? arg->tptr : "")){ /* it changed */
		    /* make sure they see it if changed */
		    e->display_it = 1;
		    InitEntryText(arg ? arg->tptr : "", e);
		    if(line == ods.top_l)
		      ods.top_l = e->hd_text;

		    zotentry(line);	/* blast old list o'entries */
		    e->dirty = 1;	/* mark it dirty */
		    retval = 1;
		}
	    }
	}
    }

    if(s)
      free(s);

    for(arg = headarg; arg; arg = nextarg){
	/* Don't free xtra or me, they just point to headerentry data */
	nextarg = arg->next;
	if(arg->tptr)
	  free(arg->tptr);
	
	free(arg);
    }

    free(sbuf);
    return(retval);
}


void
call_expander()
{
    char        **s = NULL;
    VARS_TO_SAVE *saved_state;
    int           expret;

    if(!Pmaster->expander)
      return;

    /*
     * Since expander may make a call back to pico() we need to
     * save and restore pico state.
     */
    if((saved_state = save_pico_state()) != NULL){

	expret = (*Pmaster->expander)(headents, &s);

	restore_pico_state(saved_state);
	free_pico_state(saved_state);
	ttresize();
	picosigs();

	if(expret > 0 && s){
	    char               *tbuf;
	    int                 i, biggest = 100;
	    struct headerentry *e;

	    /*
	     * Use tbuf to cat together multiple line entries before comparing.
	     */
	    tbuf = (char *)malloc(biggest + 1);
	    for(e = headents, i=0; e->name != NULL; e++,i++){
		int sz = 0;
		struct hdr_line *line;

		while(e->name && e->blank)
		  e++;
		
		if(e->name == NULL)
		  continue;

		for(line = e->hd_text; line != NULL; line = line->next)
		  sz += strlen(line->text);
		
		if(sz > biggest){
		    biggest = sz;
		    free(tbuf);
		    tbuf = (char *)malloc(biggest + 1);
		}

		tbuf[0] = '\0';
		for(line = e->hd_text; line != NULL; line = line->next)
		  strcat(tbuf, line->text);
		  
		if(strcmp(tbuf, s[i])){ /* it changed */
		    struct hdr_line *zline;

		    line = zline = e->hd_text;
		    InitEntryText(s[i], e);
		    /*
		     * If any of the lines for this entry are current or
		     * top, fix that.
		     */
		    for(; line != NULL; line = line->next){
			if(line == ods.top_l)
			  ods.top_l = e->hd_text;

			if(line == ods.cur_l)
			  ods.cur_l = e->hd_text;
		    }

		    zotentry(zline);	/* blast old list o'entries */
		}
	    }

	    free(tbuf);
	}

	if(s){
	    char **p;

	    for(p = s; *p; p++)
	      free(*p);

	    free(s);
	}
    }

    return;
}


/*
 * strend - neglecting white space, returns TRUE if c is at the
 *          end of the given line.  otherwise FALSE.
 */
strend(s, ch)
char *s;
int   ch;
{
    register char *b;
    register char  c;

    c = (char)ch;

    if(s == NULL)
      return(FALSE);

    if(*s == '\0')
      return(FALSE);

    b = &s[strlen(s)];
    while(isspace((unsigned char)(*--b))){
	if(b == s)
	  return(FALSE);
    }

    return(*b == c);
}


/*
 * strqchr - returns pointer to first non-quote-enclosed occurance of ch in 
 *           the given string.  otherwise NULL.
 *      s -- the string
 *     ch -- the character we're looking for
 *      q -- q tells us if we start out inside quotes on entry and is set
 *           correctly on exit.
 *      m -- max characters we'll check for ch (set to -1 for no check)
 */
char *
strqchr(s, ch, q, m)
    char *s;
    int   ch;
    int  *q;
    int   m;
{
    int	 quoted = (q) ? *q : 0;

    for(; s && *s && m != 0; s++, m--){
	if(*s == '"'){
	    quoted = !quoted;
	    if(q)
	      *q = quoted;
	}

	if(!quoted && *s == ch)
	  return(s);
    }

    return(NULL);
}


/*
 * KillHeaderLine() - kill a line in the header
 *
 *	notes:
 *		This is pretty simple.  Just using the emacs kill buffer
 *		and its accompanying functions to cut the text from lines.
 *
 *	returns:
 *		TRUE if hldelete worked
 *		FALSE otherwise
 */
KillHeaderLine(l, append)
struct	hdr_line    *l;
int     append;
{
    register char	*c;
    int i = ods.p_off;
    int nl = TRUE;

    if(!append)
	kdelete();

    c = l->text;
    if (gmode & MDDTKILL){
	if (c[i] == '\0')  /* don't insert a new line after this line*/
	  nl = FALSE;
        /*put to be deleted part into kill buffer */
	for (i=ods.p_off; c[i] != '\0'; i++) 
	  kinsert(c[i]);                       
    }else{
	while(*c != '\0')				/* splat out the line */
	  kinsert(*c++);
    }

    if (nl)
        kinsert('\n');				/* helpful to yank in body */

#ifdef _WINDOWS
    mswin_killbuftoclip (kremove);
#endif

    if (gmode & MDDTKILL){
	if (l->text[0]=='\0'){

	   if(l->next && l->prev)
	      ods.cur_l = next_hline(&ods.cur_e, l);
	   else if(l->prev)
	      ods.cur_l = prev_hline(&ods.cur_e, l);

	   if(l == ods.top_l)
	      ods.top_l = ods.cur_l;

	   return(hldelete(l));
	}
	else {
	  l->text[ods.p_off]='\0';    /* delete part of the line from the cursor */
	  return(TRUE);
	}
    }else{
	if(l->next && l->prev)
	   ods.cur_l = next_hline(&ods.cur_e, l);
	else if(l->prev)
	   ods.cur_l = prev_hline(&ods.cur_e, l);

	if(l == ods.top_l)
	   ods.top_l = ods.cur_l;

        return(hldelete(l));			/* blast it  */
    }
}



/*
 * SaveHeaderLines() - insert the saved lines in the list before the 
 *                     current line in the header
 *
 *	notes:
 *		Once again, just using emacs' kill buffer and its 
 *              functions.
 *
 *	returns:
 *		TRUE if something good happend
 *		FALSE otherwise
 */
SaveHeaderLines()
{
    char     *buf;				/* malloc'd copy of buffer */
    register char       *bp;			/* pointer to above buffer */
    register unsigned	i;			/* index */
    char     *work_buf, *work_buf_begin;
    char     empty[1];
    int      len, buf_len, work_buf_len, tentative_p_off = 0;
    struct hdr_line *travel, *tentative_cur_l = NULL;

    if(ksize()){
	if((bp = buf = (char *)malloc(ksize()+5)) == NULL){
	    emlwrite("Can't malloc space for saved text", NULL);
	    return(FALSE);
	}
    }
    else
      return(FALSE);

    for(i=0; i < ksize(); i++)
      if(kremove(i) != '\n')			/* filter out newlines */
	*bp++ = kremove(i);
    *bp = '\0';

    while(--bp >= buf)				/* kill trailing white space */
      if(*bp != ' '){
	  if(ods.cur_l->text[0] != '\0'){
	      if(*bp == '>'){			/* inserting an address */
		  *++bp = ',';			/* so add separator */
		  *++bp = '\0';
	      }
	  }
	  else{					/* nothing in field yet */
	      if(*bp == ','){			/* so blast any extra */
		  *bp = '\0';			/* separators */
	      }
	  }
	  break;
      }

    /* insert new text at the dot position */
    buf_len      = strlen(buf);
    tentative_p_off = ods.p_off + buf_len;
    work_buf_len = strlen(ods.cur_l->text) + buf_len;
    work_buf = (char *) malloc((work_buf_len + 1) * sizeof(char));
    if (work_buf == NULL) {
	emlwrite("Can't malloc space for saved text", NULL);
	return(FALSE);
    }

    sprintf(work_buf_begin = work_buf, "%.*s%s%s", ods.p_off,
	    ods.cur_l->text, buf, &ods.cur_l->text[ods.p_off]);
    empty[0]='\0';
    ods.p_off = 0;

    /* insert text in 256-byte chuks */
    while(work_buf_len + ods.p_off > 256) {
	strncpy(&ods.cur_l->text[ods.p_off], work_buf, 256-ods.p_off);
	work_buf += (256 - ods.p_off);
	work_buf_len -= (256 - ods.p_off);

	if(FormatLines(ods.cur_l, empty, LINELEN(),
		       headents[ods.cur_e].break_on_comma, 0) == -1) {
	   i = FALSE;
	   break;
	} else {
	   i = TRUE;
	  len = 0;
	  travel = ods.cur_l;
	  while (len < 256){
	      len += strlen(travel->text);
	      if (len >= 256)
		break;

	      /*
	       * This comes after the break above because it will
	       * be accounted for in the while loop below.
	       */
	      if(!tentative_cur_l){
		  if(tentative_p_off <= strlen(travel->text))
		    tentative_cur_l = travel;
		  else
		    tentative_p_off -= strlen(travel->text);
	      }

	      travel = travel->next;
	  }

	  ods.cur_l = travel;
	  ods.p_off = strlen(travel->text) - len + 256;
	}
    }

    /* insert the remainder of text */
    if (i != FALSE && work_buf_len > 0) {
	strcpy(&ods.cur_l->text[ods.p_off], work_buf);
	work_buf = work_buf_begin;
	free(work_buf);

	if(FormatLines(ods.cur_l, empty, LINELEN(),
		       headents[ods.cur_e].break_on_comma, 0) == -1) {
	   i = FALSE;
	} else {  
	  len = 0;
	  travel = ods.cur_l;
	  while (len < work_buf_len + ods.p_off){
	      if(!tentative_cur_l){
		  if(tentative_p_off <= strlen(travel->text))
		    tentative_cur_l = travel;
		  else
		    tentative_p_off -= strlen(travel->text);
	      }

	      len += strlen(travel->text);
	      if (len >= work_buf_len + ods.p_off)
		break;

	      travel = travel->next;
	  }

	  ods.cur_l = travel;
	  ods.p_off = strlen(travel->text) - len + work_buf_len + ods.p_off;
	  if(tentative_cur_l
	     && tentative_p_off >= 0
	     && tentative_p_off <= strlen(tentative_cur_l->text)){
	      ods.cur_l = tentative_cur_l;
	      ods.p_off = tentative_p_off;
	  }
	}
    }

    free(buf);
    return(i);
}




/*
 * break_point - Break the given line s at the most reasonable character c
 *               within l max characters.
 *
 *	returns:
 *		Pointer to the best break point in s, or
 *		Pointer to the beginning of s if no break point found
 */
char *
break_point(s, l, ch, q)
    char *s;
    int   l, ch, *q;
{
    register char *b = s + l;
    int            quoted = (q) ? *q : 0;

    while(b != s){
	if(ch == ',' && *b == '"')		/* don't break on quoted ',' */
	  quoted = !quoted;			/* toggle quoted state */

	if(*b == ch){
	    if(ch == ' '){
		if(b + 1 < s + l){
		    b++;			/* leave the ' ' */
		    break;
		}
	    }
	    else{
		/*
		 * if break char isn't a space, leave a space after
		 * the break char.
		 */
		if(!(b+1 >= s+l || (b[1] == ' ' && b+2 == s+l))){
		    b += (b[1] == ' ') ? 2 : 1;
		    break;
		}
	    }
	}
	b--;
    }

    if(q)
      *q = quoted;

    return((quoted) ? s : b);
}




/*
 * hldelete() - remove the header line pointed to by l from the linked list
 *              of lines.
 *
 *	notes:
 *		the case of first line in field is kind of bogus.  since
 *              the array of headers has a pointer to the first line, and 
 *		i don't want to worry about this too much, i just copied 
 *		the line below and removed it rather than the first one
 *		from the list.
 *
 *	returns:
 *		TRUE if it worked 
 *		FALSE otherwise
 */
hldelete(l)
struct hdr_line  *l;
{
    register struct hdr_line *lp;

    if(l == NULL)
      return(FALSE);

    if(l->next == NULL && l->prev == NULL){	/* only one line in field */
	l->text[0] = '\0';
	return(TRUE);				/* no free only line in list */
    }
    else if(l->next == NULL){			/* last line in field */
	l->prev->next = NULL;
    }
    else if(l->prev == NULL){			/* first line in field */
	strcpy(l->text, l->next->text);
	lp = l->next;
	if((l->next = lp->next) != NULL)
	  l->next->prev = l;
	l = lp;
    }
    else{					/* some where in field */
	l->prev->next = l->next;
	l->next->prev = l->prev;
    }

    l->next = NULL;
    l->prev = NULL;
    free((char *)l);
    return(TRUE);
}



/*
 * is_blank - returns true if the next n chars from coordinates row, col
 *           on display are spaces
 */
is_blank(row, col, n)
int row, col, n;
{
    n += col;
    for( ;col < n; col++){
        if(pscr(row, col) == NULL || pscr(row, col)->c != ' ')
	  return(0);
    }
    return(1);
}


/*
 * ShowPrompt - display key help corresponding to the current header entry
 */
void
ShowPrompt()
{
    if(headents[ods.cur_e].key_label){
	menu_header[TO_KEY].name  = "^T";
	menu_header[TO_KEY].label = headents[ods.cur_e].key_label;
	KS_OSDATASET(&menu_header[TO_KEY], KS_OSDATAGET(&headents[ods.cur_e]));
    }
    else
      menu_header[TO_KEY].name  = NULL;
    
    if(Pmaster && Pmaster->exit_label)
      menu_header[SEND_KEY].label = Pmaster->exit_label;
    else if(gmode & (MDVIEW | MDHDRONLY))
      menu_header[SEND_KEY].label =  (gmode & MDHDRONLY) ? "eXit/Save" : "eXit";
    else
      menu_header[SEND_KEY].label = "Send";

    if(gmode & MDVIEW){
	menu_header[CUT_KEY].name  = NULL;
	menu_header[DEL_KEY].name  = NULL;
	menu_header[UDEL_KEY].name = NULL;
    }
    else{
	menu_header[CUT_KEY].name  = "^K";
	menu_header[DEL_KEY].name  = "^D";
	menu_header[UDEL_KEY].name = "^U";
    }

    if(Pmaster->ctrlr_label){
	menu_header[RICH_KEY].label = Pmaster->ctrlr_label;
	menu_header[RICH_KEY].name = "^R";
    }
    else if(gmode & MDHDRONLY){
	menu_header[RICH_KEY].name  = NULL;
    }
    else{
	menu_header[RICH_KEY].label = "Rich Hdr";
	menu_header[RICH_KEY].name  = "^R";
    }

    if(gmode & MDHDRONLY){
	if(headents[ods.cur_e].fileedit){
	    menu_header[PONE_KEY].name  = "^_";
	    menu_header[PONE_KEY].label   = "Edit File";
	}
	else
	  menu_header[PONE_KEY].name  = NULL;

	menu_header[ATT_KEY].name   = NULL;
    }
    else{
	menu_header[PONE_KEY].name  = "^O";
	menu_header[PONE_KEY].label = "Postpone";

	menu_header[ATT_KEY].name   = "^J";
    }

    wkeyhelp(menu_header);
}


/*
 * packheader - packup all of the header fields for return to caller. 
 *              NOTE: all of the header info passed in, including address
 *                    of the pointer to each string is contained in the
 *                    header entry array "headents".
 */
packheader()
{
    register int	i = 0;		/* array index */
    register int	count;		/* count of chars in a field */
    register int	retval = TRUE;	/* count of chars in a field */
    register char	*bufp;		/* */
    register struct	hdr_line *line;

    if(!headents)
      return(TRUE);

    while(headents[i].name != NULL){
#ifdef	ATTACHMENTS
	/*
	 * attachments are special case, already in struct we pass back
	 */
	if(headents[i].is_attach){
	    i++;
	    continue;
	}
#endif

	if(headents[i].blank){
	    i++;
	    continue;
	}

        /*
         * count chars to see if we need a new malloc'd space for our
         * array.
         */
        line = headents[i].hd_text;
        count = 0;
        while(line != NULL){
            /*
             * add one for possible concatination of a ' ' character ...
             */
            count += (strlen(line->text) + 1);
            line = line->next;
        }

        line = headents[i].hd_text;
        if(count < headents[i].maxlen){		
            *headents[i].realaddr[0] = '\0';
        }
        else{
            /*
             * don't forget to include space for the null terminator!!!!
             */
            if((bufp = (char *)malloc((count+1) * sizeof(char))) != NULL){
                *bufp = '\0';

                free(*headents[i].realaddr);
                *headents[i].realaddr = bufp;
            }
            else{
                emlwrite("Can't make room to pack header field.", NULL);
                retval = FALSE;
            }
        }

        if(retval != FALSE){
	    while(line != NULL){
		/* pass the cursor offset back in Pmaster struct */
		if(headents[i].start_here && ods.cur_l == line && Pmaster)
		  Pmaster->edit_offset += strlen(*headents[i].realaddr);

                strcat(*headents[i].realaddr, line->text);
		if(line->text[0] && line->text[strlen(line->text)-1] == ',')
		  strcat(*headents[i].realaddr, " ");

                line = line->next;
            }
        }

        i++;
    }
    return(retval);    
}



/*
 * zotheader - free all malloc'd lines associated with the header structs
 */
void
zotheader()
{
    register struct headerentry *i;
  
    for(i = headents; headents && i->name; i++)
      zotentry(i->hd_text);
}


/*
 * zotentry - free malloc'd space associated with the given linked list
 */
void
zotentry(l)
register struct hdr_line *l;
{
    register struct hdr_line *ld, *lf = l;

    while((ld = lf) != NULL){
	lf = ld->next;
	ld->next = ld->prev = NULL;
	free((char *) ld);
    }
}



/*
 * zotcomma - blast any trailing commas and white space from the end 
 *	      of the given line
 */
int
zotcomma(s)
char *s;
{
    register char *p;
    int retval = FALSE;

    p = &s[strlen(s)];
    while(--p >= s){
	if(*p != ' '){
	    if(*p == ','){
		*p = '\0';
		retval = TRUE;
	    }

	    return(retval);
	}
    }

    return(retval);
}


/*
 * Save the current state of global variables so that we can restore
 * them later. This is so we can call pico again.
 * Also have to initialize some variables that normally would be set to
 * zero on startup.
 */
VARS_TO_SAVE *
save_pico_state()
{
    VARS_TO_SAVE  *ret;
    extern int     vtrow;
    extern int     vtcol;
    extern int     lbound;
    extern VIDEO **vscreen;
    extern VIDEO **pscreen;
    extern int     pico_all_done;
    extern jmp_buf finstate;
    extern char   *pico_anchor;

    if((ret = (VARS_TO_SAVE *)malloc(sizeof(VARS_TO_SAVE))) == NULL)
      return(ret);

    ret->vtrow = vtrow;
    ret->vtcol = vtcol;
    ret->lbound = lbound;
    ret->vscreen = vscreen;
    ret->pscreen = pscreen;
    ret->ods = ods;
    ret->delim_ps = delim_ps;
    ret->invert_ps = invert_ps;
    ret->pico_all_done = pico_all_done;
    memcpy(ret->finstate, finstate, sizeof(jmp_buf));
    ret->pico_anchor = pico_anchor;
    ret->Pmaster = Pmaster;
    ret->fillcol = fillcol;
    if((ret->pat = (char *)malloc(sizeof(char) * (strlen(pat)+1))) != NULL)
      strcpy(ret->pat, pat);

    ret->ComposerTopLine = ComposerTopLine;
    ret->ComposerEditing = ComposerEditing;
    ret->gmode = gmode;
    ret->alt_speller = alt_speller;
    ret->quote_str = glo_quote_str;
    ret->currow = currow;
    ret->curcol = curcol;
    ret->thisflag = thisflag;
    ret->lastflag = lastflag;
    ret->curgoal = curgoal;
    ret->opertree = (char *) malloc(sizeof(char) * (strlen(opertree) + 1));
    if(ret->opertree != NULL)
      strcpy(ret->opertree, opertree);

    ret->curwp = curwp;
    ret->wheadp = wheadp;
    ret->curbp = curbp;
    ret->bheadp = bheadp;
    ret->km_popped = km_popped;
    ret->mrow = term.t_mrow;

    /* Initialize for next pico call */
    wheadp = NULL;
    curwp = NULL;
    bheadp = NULL;
    curbp = NULL;

    return(ret);
}


void
restore_pico_state(state)
VARS_TO_SAVE *state;
{
    extern int     vtrow;
    extern int     vtcol;
    extern int     lbound;
    extern VIDEO **vscreen;
    extern VIDEO **pscreen;
    extern int     pico_all_done;
    extern jmp_buf finstate;
    extern char   *pico_anchor;

    clearcursor();
    vtrow = state->vtrow;
    vtcol = state->vtcol;
    lbound = state->lbound;
    vscreen = state->vscreen;
    pscreen = state->pscreen;
    ods = state->ods;
    delim_ps = state->delim_ps;
    invert_ps = state->invert_ps;
    pico_all_done = state->pico_all_done;
    memcpy(finstate, state->finstate, sizeof(jmp_buf));
    pico_anchor = state->pico_anchor;
    Pmaster = state->Pmaster;
    if(Pmaster)
      headents = Pmaster->headents;

    fillcol = state->fillcol;
    if(state->pat)
      strcpy(pat, state->pat);

    ComposerTopLine = state->ComposerTopLine;
    ComposerEditing = state->ComposerEditing;
    gmode = state->gmode;
    alt_speller = state->alt_speller;
    glo_quote_str = state->quote_str;
    currow = state->currow;
    curcol = state->curcol;
    thisflag = state->thisflag;
    lastflag = state->lastflag;
    curgoal = state->curgoal;
    if(state->opertree)
      strcpy(opertree, state->opertree);

    curwp = state->curwp;
    wheadp = state->wheadp;
    curbp = state->curbp;
    bheadp = state->bheadp;
    km_popped = state->km_popped;
    term.t_mrow = state->mrow;
}


void
free_pico_state(state)
VARS_TO_SAVE *state;
{
    if(state->pat)
      free(state->pat);

    if(state->opertree)
      free(state->opertree);

    free(state);
}


/*
 * Ok to call this twice in a row because it won't do anything the second
 * time.
 */
void
fix_mangle_and_err(mangled, errmsg, name)
    int   *mangled;
    char **errmsg;
    char  *name;
{
    if(mangled && *mangled){
	ttresize();
	picosigs();
	PaintBody(0);
	*mangled = 0;
    }

    if(errmsg && *errmsg){
	if(**errmsg){
	    char err[500];

	    sprintf(err, "%s field: %s", name, *errmsg);
	    (*term.t_beep)();
	    emlwrite(err, NULL);
	}
	else
	    mlerase();

	free(*errmsg);
	*errmsg = NULL;
    }
}


#ifdef	MOUSE
#undef	HeaderEditor

/*
 * Wraper function for the real header editor. 
 * Does the important tasks of:
 *	1) verifying that we _can_ edit the headers.
 *	2) acting on the result code from the header editor.
 */
int
HeaderEditor(f, n)
     int f, n;
{
    int  retval;
    
    
#ifdef _WINDOWS
    /* Sometimes we get here from a scroll callback, which
     * is no good at all because mswin is not ready to process input and
     * this _headeredit() will never do anything.
     * Putting this test here was the most general solution I could think
     * of. */
    if (!mswin_caninput()) 
	return (-1);
#endif

    retval = HeaderEditorWork(f, n);
    if (retval == -3) {
	retval = mousepress(0,0);
    }
    return (retval);
}
#endif
