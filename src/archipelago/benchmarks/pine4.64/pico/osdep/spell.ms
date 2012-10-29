#line 2 "osdep/spell.ms"
/*
 * Program:	msspell.c
 *
 *
 * Thomas Unger and Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: pine@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 *
 * Pine and Pico are registered trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior written
 * permission of the University of Washington.
 * 
 * Pine, Pico, and Pilot software and its included text are Copyright
 * 1989-2002 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 */

#include "headers.h"

#ifdef	SPELLER
#include "../spell/spell.h"


extern FILE		*gfDebugFile = NULL;



int  movetoword PROTO((char *));


#define  NSHLINES  12

static char *spellhelp[] = {
  "Spell Check Help",
  " ",
  "\tThe spell checker examines all words in the text.  It then",
  "\toffers each misspelled word for correction while simultaneously",
  "\thighlighting it in the text.  To leave a word unchanged simply",
  "~\thit ~R~e~t~u~r~n at the edit prompt.  If a word has been corrected,",
  "\teach occurrence of the incorrect word is offered for replacement.",
  " ",
  "~\tSpell checking can be cancelled at any time by typing ~^~C (~F~3)",
  "\tafter exiting help.",
  " ",
  "End of Spell Check Help",
  " ",
  NULL
};


static char *pinespellhelp[] = {
  "Spell Check Help",
  " ",
  "\tThe spell checker examines all words in the text.  It then",
  "\toffers each misspelled word for correction while simultaneously",
  "\thighlighting it in the text.  To leave a word unchanged simply",
  "\thit Return at the edit prompt.  If a word has been corrected,",
  "\teach occurrence of the incorrect word is offered for replacement.",
  " ",
  "\tSpell checking can be cancelled at any time by typing ^C (F3)",
  "\tafter exiting help.",
  " ",
  "End of Spell Check Help",
  " ",
  NULL
};


extern HWND		ghTTYWnd;



/*
 * spell() - check for potentially missspelled words and offer them for
 *           correction
 */
spell(f, n)
    int f, n;
{
    int    wlen, bsize, ret, begi, endi, offset, i, qlen;
    char   wbuf[NLINE], *lbuf;
    FILE   *p;
    LINE   *lbp;  /* pointer to line before current line */
    struct StrStResult result;

    setimark(0, 1);
    gotobob(0, 1);
    emlwrite("Checking spelling...",NULL);		/* greetings */

    ret = 1;

    StResetUserDict((char *) getenv("USER_DICTIONARY"), NULL);

    qlen = (Pmaster && Pmaster->quote_str) ? strlen(Pmaster->quote_str) : 0;
    lbuf = (char *) malloc(sizeof(char));
    bsize = 0;
    for(lbp = curbp->b_linep; lbp != curbp->b_linep->l_bp; lbp = lforw(lbp)){
	/* 
	 * before, we were accessing the current line, but since that can
	 * potentially be free()'d (like in chword()), we should only access
	 * the line from the line before (hence all the lforw(lbp)'s).
	 */
	if(qlen){				/* quoted line? */
	    i = 0;
	    if(qlen <= llength(lforw(lbp)))
	      while(i < qlen && Pmaster->quote_str[i] == lgetc(lforw(lbp), i).c)
		i++;

	    if(i == qlen)
	      continue;
	}

	if(bsize < llength(lforw(lbp)) + 2){	/* resize line buffer? */
	    bsize = llength(lforw(lbp));
	    if(!(lbuf = (char *)realloc(lbuf, (bsize + 1) * sizeof(char)))){
		emlwrite("Can't reallocate internal buffer!", NULL);
		ret = 0;			/* FAILURE ! */
		break;
	    }
	}
	  
	for(i = 0; i < llength(lforw(lbp)); i++)  /* copy line to buffer */
	  lbuf[i] = lgetc(lforw(lbp), i).c;

	lbuf[i] = '\0';				/* initialize */
	begi = endi = offset = 0;
	while(wlen = StParseLine(lbuf, wbuf, &begi, &endi, i))
	  if(!SpellWord(wbuf, 0, ghTTYWnd, &result)){

	      if(movetoword(wbuf) != TRUE)
		break;

	      update();
	      (*term.t_rev)(1);
	      pputs(wbuf, 1);			/* and highlight it */
	      (*term.t_rev)(0);

	      SpellWord(wbuf, ST_INTERACTIVE, ghTTYWnd, &result);

	      curwp->w_flag |= WFMOVE;		/* put cursor back */
	      sgarbk = 0;			/* fake no-keymenu-change! */
	      update();
	      pputs(wbuf, 0);			/* un-highlight */

	      if(result.code & (ST_EXIT | ST_ERROR)){
		  if(result.code & ST_EXIT)
		    emlwrite("Spell Checking Cancelled", NULL);

		  lbp = NULL;		        /* stop looking */
		  ret  = 0;
		  break;
	      }
	      else if(result.replace[0]){
		  chword(wbuf, result.replace);	/* correct word */
		  offset += strlen(result.replace) - wlen;
		  update();			/* place cursor */
	      }
	  }

	if(!lbp)				/* more to come? */
	  break;
    }

    StResetUserDict(NULL, NULL);
    StClearHist();
    swapimark(0, 1);
    curwp->w_flag |= WFHARD|WFMODE;
    sgarbk = TRUE;

    if(ret)
      emlwrite("Done checking spelling", NULL);

    return(ret);
}


/* 
 * movetoword() - move to the first occurance of the word w
 *
 *	returns:
 *		TRUE upon success
 *		FALSE otherwise
 *
 * BUG - since it only moves to the first occurence, there is
 *       no possibility to ignore the first misspelling and
 *       change the second.
 */
movetoword(w)
  char *w;
{
    int      i;
    int      ret  = FALSE;
    int	     olddoto;
    LINE     *olddotp;
    register int   off;				/* curwp offset */
    register LINE *lp;				/* curwp line   */

    olddoto = curwp->w_doto;			/* save where we are */
    olddotp = curwp->w_dotp;

    curwp->w_bufp->b_mode |= MDEXACT;		/* case sensitive */
    while(forscan(&i, w, NULL, 0, 1) == TRUE){
	if(i)
	  break;				/* wrap NOT allowed! */

	lp  = curwp->w_dotp;			/* for convenience */
	off = curwp->w_doto;

	/*
	 * We want to minimize the number of substrings that we report
	 * as matching a misspelled word...
	 */
	if(off == 0 || !isalpha((unsigned char)lgetc(lp, off - 1).c)){
	    off += strlen(w);
	    if((!isalpha((unsigned char)lgetc(lp, off).c) || off == llength(lp))
	       && lgetc(lp, 0).c != '>'){
		ret = TRUE;
		break;
	    }
	}

	forwchar(0, 1);				/* move on... */

    }
    curwp->w_bufp->b_mode ^= MDEXACT;		/* case insensitive */

    if(ret == FALSE){
	curwp->w_dotp = olddotp;
	curwp->w_doto = olddoto;
    }
    else
      curwp->w_flag |= WFHARD;

    return(ret);
}
#endif	/* SPELLER */
