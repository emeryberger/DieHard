#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: search.c 13728 2004-07-01 21:33:30Z jpf $";
#endif
/*
 * Program:	Searching routines
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
 * The functions in this file implement commands that search in the forward
 * and backward directions. There are no special characters in the search
 * strings. Probably should have a regular expression search, or something
 * like that.
 *
 */

#include	"headers.h"

int	eq PROTO((int, int));
int	expandp PROTO((char *, char *, int));
int	readnumpat PROTO((char *));
void	get_pat_cases PROTO((char *, char *));


#define	FWS_RETURN(RV)	{				\
			    thisflag |= CFSRCH;		\
			    curwp->w_flag |= WFMODE;	\
			    sgarbk = TRUE;		\
			    return(RV);			\
			}


/*
 * Search forward. Get a search string from the user, and search, beginning at
 * ".", for the string. If found, reset the "." to be just after the match
 * string, and [perhaps] repaint the display. Bound to "C-S".
 */

/*	string search input parameters	*/

#define	PTBEG	1	/* leave the point at the begining on search */
#define	PTEND	2	/* leave the point at the end on search */



static char *SearchHelpText[] = {
"Help for Search Command",
" ",
"\tEnter the words or characters you would like to search",
"~\tfor, then press ~R~e~t~u~r~n.  The search then takes place.",
"\tWhen the characters or words that you entered ",
"\tare found, the buffer will be redisplayed with the cursor ",
"\tat the beginning of the selected text.",
" ",
"\tThe most recent string for which a search was made is",
"\tdisplayed in the \"Search\" prompt between the square",
"\tbrackets.  This string is the default search prompt.",
"~        Hitting only ~R~e~t~u~r~n or at the prompt will cause the",
"\tsearch to be made with the default value.",
"  ",
"\tThe text search is not case sensitive, and will examine the",
"\tentire message.",
"  ",
"\tShould the search fail, a message will be displayed.",
"  ",
"End of Search Help.",
"  ",
NULL
};


/*
 * Compare two characters. The "bc" comes from the buffer. It has it's case
 * folded out. The "pc" is from the pattern.
 */
eq(bc, pc)
int bc;
int pc;
{
    if ((curwp->w_bufp->b_mode & MDEXACT) == 0){
	if (bc>='a' && bc<='z')
	  bc -= 0x20;

	if (pc>='a' && pc<='z')
	  pc -= 0x20;
    }

    return(bc == pc);
}


forwsearch(f, n)
    int f, n;
{
  register         int status;
  int              wrapt = FALSE, wrapt2 = FALSE;
  int              repl_mode = FALSE;
  char             defpat[NPAT];
  int              search = FALSE;

    /* resolve the repeat count */
    if (n == 0)
      n = 1;

    if (n < 1)			/* search backwards */
      FWS_RETURN(0);

    defpat[0] = '\0';

    /* ask the user for the text of a pattern */
    while(1){

	if (gmode & MDREPLACE)
	  status = srpat("Search", defpat, repl_mode);
	else
	  status = readpattern("Search", TRUE);

	switch(status){
	  case TRUE:                         /* user typed something */
	    search = TRUE;
	    break;

	  case HELPCH:			/* help requested */
	    if(Pmaster){
		VARS_TO_SAVE *saved_state;

		saved_state = save_pico_state();
		(*Pmaster->helper)(Pmaster->search_help,
				   "Help for Searching", 1);
		if(saved_state){
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		}
	    }
	    else
	      pico_help(SearchHelpText, "Help for Searching", 1);

	  case (CTRL|'L'):			/* redraw requested */
	    pico_refresh(FALSE, 1);
	    update();
	    break;

	  case  (CTRL|'V'):
	    gotoeob(0, 1);
	    mlerase();
	    FWS_RETURN(TRUE);

	  case (CTRL|'Y'):
	    gotobob(0, 1);
	    mlerase();
	    FWS_RETURN(TRUE); 

	  case (CTRL|'T') :
	    switch(status = readnumpat("Search to Line Number : ")){
	      case -1 :
		emlwrite("Search to Line Number Cancelled", NULL);
		FWS_RETURN(FALSE);

	      case  0 :
		emlwrite("Line number must be greater than zero", NULL);
		FWS_RETURN(FALSE);

	      case -2 :
		emlwrite("Line number must contain only digits", NULL);
		FWS_RETURN(FALSE);
		
	      case -3 :
		continue;

	      default :
		gotoline(0, status);
		mlerase();
		FWS_RETURN(TRUE);
	    }

	    break;

	  case  (CTRL|'W'):
	    {
		LINE *linep = curwp->w_dotp;
		int   offset = curwp->w_doto;

		gotobop(0, 1);
		gotobol(0, 1);

		/*
		 * if we're asked to backup and we're already
		 *
		 */
		if((lastflag & CFSRCH)
		   && linep == curwp->w_dotp
		   && offset == curwp->w_doto
		   && !(offset == 0 && lback(linep) == curbp->b_linep)){
		    backchar(0, 1);
		    gotobop(0, 1);
		    gotobol(0, 1);
		}
	    }

	    mlerase();
	    FWS_RETURN(TRUE);

	  case  (CTRL|'O'):
	    if(curwp->w_dotp != curbp->b_linep){
		gotoeop(0, 1);
		forwchar(0, 1);
	    }

	    mlerase();
	    FWS_RETURN(TRUE);

	  case (CTRL|'U'):
	    fillbuf(0, 1);
	    mlerase();
	    FWS_RETURN(TRUE);

	  case  (CTRL|'R'):        /* toggle replacement option */
	    repl_mode = !repl_mode;
	    break;

	  default:
	    if(status == ABORT)
	      emlwrite("Search Cancelled", NULL);
	    else
	      mlerase();

	    FWS_RETURN(FALSE);
	}

	/* replace option is disabled */
	if (!(gmode & MDREPLACE)){
	    strcpy(defpat, pat);
	    break;
	}
	else if (search){  /* search now */
	    strcpy(pat, defpat);  /* remember this search for the future */
	    break;
	}
    }

    /*
     * This code is kind of dumb.  What I want is successive C-W 's to 
     * move dot to successive occurences of the pattern.  So, if dot is
     * already sitting at the beginning of the pattern, then we'll move
     * forward a char before beginning the search.  We'll let the
     * automatic wrapping handle putting the dot back in the right 
     * place...
     */
    status = 0;		/* using "status" as int temporarily! */
    while(1){
	if(defpat[status] == '\0'){
	    forwchar(0, 1);
	    break;		/* find next occurence! */
	}

	if(status + curwp->w_doto >= llength(curwp->w_dotp) ||
	   !eq(defpat[status],lgetc(curwp->w_dotp, curwp->w_doto + status).c))
	  break;		/* do nothing! */
	status++;
    }

    /* search for the pattern */
    
    while (n-- > 0) {
	if((status = forscan(&wrapt,&defpat[0],NULL,0,PTBEG)) == FALSE)
	  break;
    }

    /* and complain if not there */
    if (status == FALSE){
      emlwrite("\"%s\" not found", defpat);
    }
    else if((gmode & MDREPLACE) && repl_mode == TRUE){
        status = replace_pat(defpat, &wrapt2);    /* replace pattern */
	if (wrapt == TRUE || wrapt2 == TRUE)
	  emlwrite("Replacement %srapped",
		   (status == ABORT) ? "cancelled but w" : "W");
    }
    else if(wrapt == TRUE){
	emlwrite("Search Wrapped", NULL);
    }
    else if(status == TRUE){
	emlwrite("", NULL);
    }

    FWS_RETURN(status);
}



/* Replace a pattern with the pattern the user types in one or more times. */
int replace_pat(defpat, wrapt)
char *defpat;
int  *wrapt;
{
  register         int status;
  char             lpat[NPAT], origpat[NPAT];     /* case sensitive pattern */
  EXTRAKEYS        menu_pat[2];
  int              repl_all = FALSE;
  char             prompt[2*NLINE+32];

    forscan(wrapt, defpat, NULL, 0, PTBEG);    /* go to word to be replaced */

    lpat[0] = '\0';

    /* additional 'replace all' menu option */
    menu_pat[0].name  = "^X";
    menu_pat[0].key   = (CTRL|'X');
    menu_pat[0].label = "Repl All";
    KS_OSDATASET(&menu_pat[0], KS_NONE);
    menu_pat[1].name  = NULL;

    while(1) {

	update();
	(*term.t_rev)(1);
	get_pat_cases(origpat, defpat);
	pputs(origpat, 1);                       /* highlight word */
	(*term.t_rev)(0);

	sprintf(prompt, "Replace%s \"", repl_all ? " every" : "");

	expandp(&defpat[0], &prompt[strlen(prompt)], NPAT/2);
	strcat(prompt, "\" with");
	if(rpat[0] != 0){
	    strcat(prompt, " [");
	    expandp(rpat, &prompt[strlen(prompt)], NPAT/2);
	    strcat(prompt, "]");
	}

	strcat(prompt, " : ");

	status = mlreplyd(prompt, lpat, NPAT, QDEFLT, menu_pat);

	curwp->w_flag |= WFMOVE;

	switch(status){

	  case TRUE :
	  case FALSE :
	    if(lpat[0])
	      strcpy(rpat, lpat); /* remember default */
	    else
	      strcpy(lpat, rpat); /* use default */

	    if (repl_all){
		status = replace_all(defpat, lpat);
	    }
	    else{
		chword(defpat, lpat);	/* replace word    */
		update();
		status = TRUE;
	    }

	    if(status == TRUE)
	      emlwrite("", NULL);

	    return(status);

	  case HELPCH:                      /* help requested */
	    if(Pmaster){
		VARS_TO_SAVE *saved_state;

		saved_state = save_pico_state();
		(*Pmaster->helper)(Pmaster->search_help,
				   "Help for Searching", 1);
		if(saved_state){
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		}
	    }
	    else
	      pico_help(SearchHelpText, "Help for Searching", 1);

	  case (CTRL|'L'):			/* redraw requested */
	    pico_refresh(FALSE, 1);
	    update();
	    break;

	  case (CTRL|'X'):        /* toggle replace all option */
	    if (repl_all){
		repl_all = FALSE;
		menu_pat[0].label = "Repl All";
	    }
	    else{
		repl_all = TRUE;
		menu_pat[0].label = "Repl One";
	    }

	    break;

	  default:
	    if(status == ABORT)
	      emlwrite("Replacement Cancelled", NULL);
	    else 
	      mlerase();
	    chword(defpat, origpat);
	    update();
	    return(FALSE);
	}
    }
}

/* Since the search is not case sensitive, we must obtain the actual pattern 
   that appears in the text, so that we can highlight (and unhighlight) it
   without using the wrong cases  */
void
get_pat_cases(realpat, searchpat)
char *searchpat, *realpat;
{
  int i, searchpatlen, curoff;

  curoff = curwp->w_doto;
  searchpatlen = strlen(searchpat);

  for (i = 0; i < searchpatlen; i++)
    realpat[i] = lgetc(curwp->w_dotp, curoff++).c;
  realpat[searchpatlen] = '\0';
}
    
/* Ask the user about every occurence of orig pattern and replace it with a 
   repl pattern if the response is affirmative. */   
int replace_all(orig, repl)
char *orig;
char *repl;
{
  register         int status = 0;
  char             prompt[NLINE], realpat[NPAT];
  int              wrapt, n = 0;
  LINE		  *stop_line   = curwp->w_dotp;
  int		   stop_offset = curwp->w_doto;

  while (1)
    if (forscan(&wrapt, orig, stop_line, stop_offset, PTBEG)){
        curwp->w_flag |= WFMOVE;            /* put cursor back */

        update();
	(*term.t_rev)(1);
	get_pat_cases(realpat, orig);
	pputs(realpat, 1);                       /* highlight word */
	(*term.t_rev)(0);
	fflush(stdout);

	strcpy(prompt, "Replace \"");
	expandp(&orig[0], &prompt[strlen(prompt)], NPAT/2);
	strcat(prompt, "\" with \"");
	expandp(&repl[0], &prompt[strlen(prompt)], NPAT/2);
	strcat(prompt, "\"");

	status = mlyesno(prompt, TRUE);		/* ask user */

	if (status == TRUE){
	    n++;
	    chword(realpat, repl);		     /* replace word    */
	    update();
	}else{
	    chword(realpat, realpat);	       /* replace word by itself */
	    update();
	    if(status == ABORT){		/* if cancelled return */
		emlwrite("Replace All cancelled after %d changes", (char *) n);
		return (ABORT);			/* ... else keep looking */
	    }
	}
    }
    else{
	emlwrite("No more matches for \"%s\"", orig);
	return (FALSE);
    }
}


/* Read a replacement pattern.  Modeled after readpattern(). */
srpat(prompt, defpat, repl_mode)
char *prompt;
char *defpat;
int   repl_mode;
{
	register int s;
	int	     i = 0;
	char	     tpat[NPAT+20];
	EXTRAKEYS    menu_pat[8];

	menu_pat[i = 0].name = "^Y";
	menu_pat[i].label    = "FirstLine";
	menu_pat[i].key	     = (CTRL|'Y');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	menu_pat[++i].name = "^V";
	menu_pat[i].label  = "LastLine";
	menu_pat[i].key	   = (CTRL|'V');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	menu_pat[++i].name = "^R";
	menu_pat[i].label  = repl_mode ? "No Replace" : "Replace";
	menu_pat[i].key	   = (CTRL|'R');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	if(!repl_mode){
	    menu_pat[++i].name = "^T";
	    menu_pat[i].label  = "LineNumber";
	    menu_pat[i].key    = (CTRL|'T');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^W";
	    menu_pat[i].label  = "Start of Para";
	    menu_pat[i].key    = (CTRL|'W');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^O";
	    menu_pat[i].label  = "End of Para";
	    menu_pat[i].key    = (CTRL|'O');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^U";
	    menu_pat[i].label  = "FullJustify";
	    menu_pat[i].key    = (CTRL|'U');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);
	}

	menu_pat[++i].name = NULL;

	strcpy(tpat, prompt);	/* copy prompt to output string */
        if (repl_mode){
	  strcat(tpat, " (to replace)");
	}
        if(pat[0] != '\0'){
	    strcat(tpat, " [");	/* build new prompt string */
	    expandp(&pat[0], &tpat[strlen(tpat)], NPAT/2);  /*add old pattern*/
	    strcat(tpat, "]");
        }

	strcat(tpat, ": ");

	s = mlreplyd(tpat, defpat, NLINE, QDEFLT, menu_pat);

	if (s == TRUE || s == FALSE){	/* changed or not, they're done */
	    if(!defpat[0]){		/* use default */
		strcpy(defpat, pat);
	    }
	    else if(strcmp(pat, defpat)){   	      /* Specified */
		strcpy(pat, defpat);
		rpat[0] = '\0';
	    }

	    s = TRUE;			/* let caller know to proceed */
	}

	return(s);
}



/*
 * Read a pattern. Stash it in the external variable "pat". The "pat" is not
 * updated if the user types in an empty line. If the user typed an empty line,
 * and there is no old pattern, it is an error. Display the old pattern, in the
 * style of Jeff Lomicka. There is some do-it-yourself control expansion.
 * change to using <ESC> to delemit the end-of-pattern to allow <NL>s in
 * the search string.
 */

readnumpat(prompt)
char *prompt;
{
    int		 i, n;
    char	 tpat[NPAT+20];
    EXTRAKEYS    menu_pat[2];

    menu_pat[i = 0].name  = "^T";
    menu_pat[i].label	  = "No Line Number";
    menu_pat[i].key	  = (CTRL|'T');
    KS_OSDATASET(&menu_pat[i++], KS_NONE);

    menu_pat[i].name  = NULL;

    tpat[0] = '\0';
    while(1)
      switch(mlreplyd(prompt, tpat, NPAT, QNORML, menu_pat)){
	case TRUE :
	  if(*tpat){
	      for(i = n = 0; tpat[i]; i++)
		if(strchr("0123456789", tpat[i])){
		    n = (n * 10) + (tpat[i] - '0');
		}
		else
		  return(-2);

	      return(n);
	  }

	case FALSE :
	default :
	  return(-1);

	case (CTRL|'T') :
	  return(-3);

	case (CTRL|'L') :
	case HELPCH :
	  break;
      }
}	    


readpattern(prompt, text_mode)
char *prompt;
int   text_mode;
{
	register int s;
	int	     i;
	char	     tpat[NPAT+20];
	EXTRAKEYS    menu_pat[7];

	menu_pat[i = 0].name = "^Y";
	menu_pat[i].label    = "FirstLine";
	menu_pat[i].key	     = (CTRL|'Y');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	menu_pat[++i].name = "^V";
	menu_pat[i].label  = "LastLine";
	menu_pat[i].key	   = (CTRL|'V');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	if(text_mode){
	    menu_pat[++i].name = "^T";
	    menu_pat[i].label  = "LineNumber";
	    menu_pat[i].key    = (CTRL|'T');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^W";
	    menu_pat[i].label  = "Start of Para";
	    menu_pat[i].key    = (CTRL|'W');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^O";
	    menu_pat[i].label  = "End of Para";
	    menu_pat[i].key    = (CTRL|'O');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^U";
	    menu_pat[i].label  = "FullJustify";
	    menu_pat[i].key    = (CTRL|'U');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);
	}

	menu_pat[++i].name = NULL;

	strcpy(tpat, prompt);	/* copy prompt to output string */
        if(pat[0] != '\0'){
	    strcat(tpat, " [");	/* build new prompt string */
	    expandp(&pat[0], &tpat[strlen(tpat)], NPAT/2);	/* add old pattern */
	    strcat(tpat, "]");
        }
	strcat(tpat, " : ");

	s = mlreplyd(tpat, tpat, NPAT, QNORML, menu_pat);

	if ((s == TRUE) && strcmp(pat,tpat)){			/* Specified */
	  strcpy(pat, tpat);
	  rpat[0] = '\0';
	}
	else if (s == FALSE && pat[0] != 0)	/* CR, but old one */
		s = TRUE;

	return(s);
}


/* search forward for a <patrn>	*/
forscan(wrapt,patrn,limitp,limito,leavep)
int	*wrapt;		/* boolean indicating search wrapped */
char *patrn;		/* string to scan for */
LINE *limitp;		/* stop searching if reached */
int limito;		/* stop searching if reached */
int leavep;		/* place to leave point
				PTBEG = begining of match
				PTEND = at end of match		*/

{
    register LINE *curline;		/* current line during scan */
    register int curoff;		/* position within current line */
    register LINE *lastline;	/* last line position during scan */
    register int lastoff;		/* position within last line */
    register int c;			/* character at current position */
    register LINE *matchline;	/* current line during matching */
    register int matchoff;		/* position in matching line */
    register char *patptr;		/* pointer into pattern */
    register int stopoff;		/* offset to stop search */
    register LINE *stopline;	/* line to stop search */

    *wrapt = FALSE;

    /*
     * the idea is to set the character to end the search at the 
     * next character in the buffer.  thus, let the search wrap
     * completely around the buffer.
     * 
     * first, test to see if we are at the end of the line, 
     * otherwise start searching on the next character. 
     */
    if(curwp->w_doto == llength(curwp->w_dotp)){
	/*
	 * dot is not on end of a line
	 * start at 0 offset of the next line
	 */
	stopoff = curoff  = 0;
	stopline = curline = lforw(curwp->w_dotp);
	if (curwp->w_dotp == curbp->b_linep)
	  *wrapt = TRUE;
    }
    else{
	stopoff = curoff  = curwp->w_doto;
	stopline = curline = curwp->w_dotp;
    }

    /* scan each character until we hit the head link record */

    /*
     * maybe wrapping is a good idea
     */
    while (curline){

	if (curline == curbp->b_linep)
	  *wrapt = TRUE;

	/* save the current position in case we need to
	   restore it on a match			*/

	lastline = curline;
	lastoff = curoff;

	/* get the current character resolving EOLs */
	if (curoff == llength(curline)) {	/* if at EOL */
	    curline = lforw(curline);	/* skip to next line */
	    curoff = 0;
	    c = '\n';			/* and return a <NL> */
	}
	else
	  c = lgetc(curline, curoff++).c;	/* get the char */

	/* test it against first char in pattern */
	if (eq(c, patrn[0]) != FALSE) {	/* if we find it..*/
	    /* setup match pointers */
	    matchline = curline;
	    matchoff = curoff;
	    patptr = &patrn[0];

	    /* scan through patrn for a match */
	    while (*++patptr != 0) {
		/* advance all the pointers */
		if (matchoff == llength(matchline)) {
		    /* advance past EOL */
		    matchline = lforw(matchline);
		    matchoff = 0;
		    c = '\n';
		} else
		  c = lgetc(matchline, matchoff++).c;

		if(matchline == limitp && matchoff == limito)
		  return(FALSE);

		/* and test it against the pattern */
		if (eq(*patptr, c) == FALSE)
		  goto fail;
	    }

	    /* A SUCCESSFULL MATCH!!! */
	    /* reset the global "." pointers */
	    if (leavep == PTEND) {	/* at end of string */
		curwp->w_dotp = matchline;
		curwp->w_doto = matchoff;
	    }
	    else {		/* at begining of string */
		curwp->w_dotp = lastline;
		curwp->w_doto = lastoff;
	    }

	    curwp->w_flag |= WFMOVE; /* flag that we have moved */
	    return(TRUE);

	}

fail:;			/* continue to search */
	if(((curline == stopline) && (curoff == stopoff))
	   || (curline == limitp && curoff == limito))
	  break;			/* searched everywhere... */
    }
    /* we could not find a match */

    return(FALSE);
}

/* 	expandp:	expand control key sequences for output		*/

expandp(srcstr, deststr, maxlength)

char *srcstr;	/* string to expand */
char *deststr;	/* destination of expanded string */
int maxlength;	/* maximum chars in destination */

{
	char c;		/* current char to translate */

	/* scan through the string */
	while ((c = *srcstr++) != 0) {
		if (c == '\n') {		/* its an EOL */
			*deststr++ = '<';
			*deststr++ = 'N';
			*deststr++ = 'L';
			*deststr++ = '>';
			maxlength -= 4;
		} else if (c < 0x20 || c == 0x7f) {	/* control character */
			*deststr++ = '^';
			*deststr++ = c ^ 0x40;
			maxlength -= 2;
		} else if (c == '%') {
			*deststr++ = '%';
			*deststr++ = '%';
			maxlength -= 2;

		} else {			/* any other character */
			*deststr++ = c;
			maxlength--;
		}

		/* check for maxlength */
		if (maxlength < 4) {
			*deststr++ = '$';
			*deststr = '\0';
			return(FALSE);
		}
	}
	*deststr = '\0';
	return(TRUE);
}



/* 
 * chword() - change the given word, wp, pointed to by the curwp->w_dot 
 *            pointers to the word in cb
 */
void
chword(wb, cb)
  char *wb;					/* word buffer */
  char *cb;					/* changed buffer */
{
    ldelete((long) strlen(wb), NULL);		/* not saved in kill buffer */
    while(*cb != '\0')
      linsert(1, *cb++);

    curwp->w_flag |= WFEDIT;
}
