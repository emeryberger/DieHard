#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: file.c 13703 2004-06-11 21:49:40Z hubert $";
#endif
/*
 * Program:	High level file input and output routines
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
 * The routines in this file
 * handle the reading and writing of
 * disk files. All of details about the
 * reading and writing of the disk are
 * in "fileio.c".
 */
#include	"headers.h"


#ifdef	ANSI
    int ifile(char *);
    int insmsgchar(int);
#else
    int ifile();
    int insmsgchar();
#endif
char *file_split(char *, int *, char *, int);

/*
 * Read a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "read a file into the current buffer" code.
 * Bound to "C-X C-R".
 */
fileread(f, n)
int f, n;
{
        register int    s;
        char fname[NFILEN];

        if ((s=mlreply("Read file: ", fname, NFILEN, QNORML, NULL)) != TRUE)
                return(s);

	if(gmode&MDSCUR){
	    emlwrite("File reading disabled in secure mode",NULL);
	    return(0);
	}

	if (strlen(fname) == 0) {
	  emlwrite("No file name entered",NULL);
	  return(0);
	}

	if((gmode & MDTREE) && !in_oper_tree(fname)){
	  emlwrite("Can't read file from outside of %s", opertree);
	  return(0);
	}

        return(readin(fname, TRUE, TRUE));
}




static char *inshelptext[] = {
  "Insert File Help Text",
  " ",
  "\tType in a file name to have it inserted into your editing",
  "\tbuffer between the line that the cursor is currently on",
  "\tand the line directly below it.  You may abort this by ",
  "~\ttyping the ~F~2 (~^~C) key after exiting help.",
  " ",
  "End of Insert File Help",
  " ",
  NULL
};

static char *writehelp[] = {
  "Write File Help Text",
  " ",
  "\tType in a file name to have it written out, thus saving",
  "\tyour buffer, to a file.  You can abort this by typing ",
  "~\tthe ~F~2 (~^~C) key after exiting help.",
  " ",
  "End of Write File Help",
  " ",
  " ",
  NULL
};


/*
 * Insert a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "insert a file into the current buffer" code.
 * Bound to "C-X C-I".
 */
insfile(f, n)
int f, n;
{
    register int s;
    char	 fname[NLINE], dir[NLINE];
    int		 retval, bye = 0, msg = 0;
    char	 prompt[64], *infile;
    EXTRAKEYS    menu_ins[5];
    
    if (curbp->b_mode&MDVIEW) /* don't allow this command if */
      return(rdonly()); /* we are in read only mode */

    fname[0] = '\0';
    while(!bye){
	/* set up keymenu stuff */
	if(!msg){
	    int last_menu = 0;

	    menu_ins[last_menu].name  = "^T";
	    menu_ins[last_menu].key   = (CTRL|'T');
	    menu_ins[last_menu].label = "To Files";
	    KS_OSDATASET(&menu_ins[last_menu], KS_NONE);

	    if(Pmaster && Pmaster->msgntext){
		menu_ins[++last_menu].name  = "^W";
		menu_ins[last_menu].key     = (CTRL|'W');
		menu_ins[last_menu].label   = msg ? "InsertFile" : "InsertMsg";
		KS_OSDATASET(&menu_ins[last_menu], KS_NONE);
	    }

#if	!defined(DOS) && !defined(MAC)
	    if(Pmaster && Pmaster->upload){
		menu_ins[++last_menu].name = "^Y";
		menu_ins[last_menu].key    = (CTRL|'Y');
		menu_ins[last_menu].label  = "RcvUpload";
		KS_OSDATASET(&menu_ins[last_menu], KS_NONE);
	    }
#endif	/* !(DOS || MAC) */

	    if(gmode & MDCMPLT){
		menu_ins[++last_menu].name = msg ? "" : "TAB";
		menu_ins[last_menu].key    = (CTRL|'I');
		menu_ins[last_menu].label  = msg ? "" : "Complete";
		KS_OSDATASET(&menu_ins[last_menu], KS_NONE);
	    }

	    menu_ins[++last_menu].name = NULL;
	}

	sprintf(prompt, "%s to insert from %s %s: ",
		msg ? "Number of message" : "File",
		(msg || (gmode&MDCURDIR))
		  ? "current"
		  : ((gmode & MDTREE) || opertree[0]) ? opertree : "home",
		msg ? "folder" : "directory");
	s = mlreplyd(prompt, fname, NLINE, QDEFLT, msg ? NULL : menu_ins);
	/* something to read and it was edited or the default accepted */
        if(fname[0] && (s == TRUE || s == FALSE)){
	    bye++;
	    if(msg){
		if((*Pmaster->msgntext)(atol(fname), insmsgchar))
		  emlwrite("Message %s included", fname);
	    }
	    else{
		bye++;
		if(gmode&MDSCUR){
		    emlwrite("Can't insert file in restricted mode",NULL);
		}
		else{
		    if((gmode & MDTREE)
		       && !compresspath(opertree, fname, NLINE)){
			emlwrite(
			"Can't insert file from outside of %s: too many ..'s",
			opertree);
		    }
		    else{
			fixpath(fname, NLINE);

			if((gmode & MDTREE) && !in_oper_tree(fname))
			  emlwrite("Can't insert file from outside of %s",
				    opertree);
			else
			  retval = ifile(fname);
		    }
		}
	    }
	}
	else{
	    switch(s){
	      case (CTRL|'I') :
		{
		    char *fn, *p;
		    int   dirlen, l = NLINE;

		    dir[0] = '\0';
		    fn = file_split(dir, &l, fname, 0);

		    if(!pico_fncomplete(dir, fn, l - 1))
		      (*term.t_beep)();
		}

		break;
	      case (CTRL|'W'):
		msg = !msg;			/* toggle what to insert */
		break;
	      case (CTRL|'T'):
		if(msg){
		    emlwrite("Can't select messages yet!", NULL);
		}
		else{
		    char *fn;
		    int len = NLINE;

		    fn = file_split(dir, &len, fname, 1);
		    if(!isdir(dir, NULL, NULL))
		      strcpy(dir, (gmode&MDCURDIR)
			     ? (browse_dir[0] ? browse_dir : ".")
			     : ((gmode & MDTREE) || opertree[0])
			     ? opertree
			     : (browse_dir[0] ? browse_dir
				:gethomedir(NULL)));
		    else{
			if(*fn){
			    int dirlen;

			    dirlen = strlen(dir);
			    if(dirlen && dir[dirlen - 1] != C_FILESEP)
			      strcat(dir, S_FILESEP);
			    strcat(dir, fn);
			    if(!isdir(dir, NULL, NULL))
			      dir[dirlen] = '\0';
			}
		    }

		    fname[0] = '\0';
		    if((s = FileBrowse(dir, NLINE, fname, NLINE, 
				       NULL, FB_READ, NULL)) == 1){
			if(gmode&MDSCUR){
			    emlwrite("Can't insert in restricted mode",
				     NULL);
			    sleep(2);
			}
			else{
			    if (infile = (char *)malloc((strlen(dir)+
			                       strlen(S_FILESEP)+
					       strlen(fname)+1)*sizeof(char))){
			      strcpy(infile, dir);
			      strcat(infile, S_FILESEP);
			      strcat(infile, fname);
			      retval = ifile(infile);
			      free((char *) infile);
			    }
			    else {
			      emlwrite("Trouble allocating space for insert!"
				       ,NULL);
			      sleep(3);
			    }
			}
			bye++;
		    }
		    else
		      fname[0] = '\0';

		    pico_refresh(FALSE, 1);
		    if(s != 1){
			update(); 		/* redraw on return */
			continue;
		    }
		}

		break;

#if	!defined(DOS) && !defined(MAC)
	      case (CTRL|'Y') :
		if(Pmaster && Pmaster->upload){
		    char tfname[NLINE];

		    if(gmode&MDSCUR){
			emlwrite(
			      "\007Restricted mode disallows uploaded command",
			      NULL);
			return(0);
		    }

		    tfname[0] = '\0';
		    retval = (*Pmaster->upload)(tfname, NULL);

		    pico_refresh(FALSE, 1);
		    update();

		    if(retval){
			retval = ifile(tfname);
			bye++;
		    }
		    else
		      sleep(3);			/* problem, show error! */

		    if(tfname[0])		/* clean up temp file */
		      unlink(tfname);
		}
		else
		  (*term.t_beep)();		/* what? */

		break;
#endif	/* !(DOS || MAC) */
	      case HELPCH:
		if(Pmaster){
		    VARS_TO_SAVE *saved_state;

		    saved_state = save_pico_state();
		    (*Pmaster->helper)(msg ? Pmaster->ins_m_help
					   : Pmaster->ins_help,
				       "Help for Insert File", 1);
		    if(saved_state){
			restore_pico_state(saved_state);
			free_pico_state(saved_state);
		    }
		}
		else
		  pico_help(inshelptext, "Help for Insert File", 1);
	      case (CTRL|'L'):
		pico_refresh(FALSE, 1);
		update();
		continue;
	      default:
		ctrlg(FALSE, 0);
                retval = s;
		bye++;
	    }
        }
    }
    curwp->w_flag |= WFMODE|WFHARD;
    
    return(retval);
}

/* 
 * split out the file name from the path.
 * Copy path into dirbuf, return filename,
 * which is a pointer into orig_fname.
 * lenp is the length of dirbuf.
 *   is_for_browse - use browse dir if possible
 *      don't want to use it for TAB-completion
 */
char *
file_split(dirbuf, lenp, orig_fname, is_for_browse)
    char *dirbuf;
    int   *lenp;
    char  *orig_fname;
    int    is_for_browse;
{
    char *p, *fn;
    int dirlen;

    if(*orig_fname && (p = strrchr(orig_fname, C_FILESEP))){
	fn = p + 1;
	(*lenp) -= fn - orig_fname;
	dirlen = p - orig_fname;
	if(p == orig_fname)
	  strcpy(dirbuf, S_FILESEP);
#ifdef	DOS
	else if(orig_fname[0] == C_FILESEP
		|| (isalpha((unsigned char)orig_fname[0])
		    && orig_fname[1] == ':')){
	    if(orig_fname[1] == ':' && p == orig_fname+2)
	      dirlen = fn - orig_fname;
	    strncpy(dirbuf, orig_fname, dirlen);
	    dirbuf[dirlen] = '\0';
	}
#else
	else if (orig_fname[0] == C_FILESEP
		 || orig_fname[0] == '~') {
	    strncpy(dirbuf, orig_fname, dirlen);
	    dirbuf[dirlen] = '\0';
	}
#endif
	else
	  sprintf(dirbuf, "%s%c%.*s", 
		  (gmode & MDCURDIR)
		  ? ((is_for_browse && browse_dir[0]) ? browse_dir : ".")
		  : ((gmode & MDTREE) || opertree[0])
		  ? opertree
		  : ((is_for_browse && browse_dir[0])
		     ? browse_dir : gethomedir(NULL)),
		  C_FILESEP, p - orig_fname, orig_fname);
    }
    else{
	fn = orig_fname;
	strcpy(dirbuf, (gmode & MDCURDIR)
	       ? ((is_for_browse && browse_dir[0]) ? browse_dir : ".")
	       : ((gmode & MDTREE) || opertree[0])
	       ? opertree
	       : ((is_for_browse && browse_dir[0])
		  ? browse_dir : gethomedir(NULL)));
    }
    return fn;
}

insmsgchar(c)
    int c;
{
    if(c == '\n'){
	char *p;

	lnewline();
	for(p = (glo_quote_str ? glo_quote_str
		 : (Pmaster ? Pmaster->quote_str : NULL));
	    p && *p; p++)
	  if(!linsert(1, *p))
	    return(0);
    }
    else if(c != '\r')			/* ignore CR (likely CR of CRLF) */
      return(linsert(1, c));

    return(1);
}



/*
 * Read file "fname" into the current
 * buffer, blowing away any text found there. Called
 * by both the read and find commands. Return the final
 * status of the read. Also called by the mainline,
 * to read in a file specified on the command line as
 * an argument.
 */
readin(fname, lockfl, rename)
char    fname[];	/* name of file to read */
int	lockfl;		/* check for file locks? */
int     rename;         /* don't rename if reading from, say, alt speller */
{
        char line[NLINE], *linep;
	long nline;
	int  s, done, newline;

	curbp->b_linecnt = -1;			/* Must be recalculated */
	if ((s = bclear(curbp)) != TRUE)	/* Might be old.        */
	  return (s);

	if(rename)
	  strcpy(curbp->b_fname, fname);
	if ((s=ffropen(fname)) != FIOSUC){	/* Hard file open.      */
	    if(s == FIOFNF)                     /* File not found.      */
	      emlwrite("New file", NULL);
	    else
	      fioperr(s, fname);
	}
	else{
	    int charsread = 0;

	    emlwrite("Reading file", NULL);
	    nline = 0L;
	    done  = newline = 0;
	    while(!done)
	      if((s = ffgetline(line, NLINE, &charsread, 1)) == FIOEOF){
		  curbp->b_flag &= ~(BFTEMP|BFCHG);
		  gotobob(FALSE, 1);
		  sprintf(line,"Read %d line%s",
			  nline, (nline > 1) ? "s" : "");
		  emlwrite(line, NULL);
		  break;
	      }
	      else{
		  if(newline){
		      lnewline();
		      newline = 0;
		  }

		  switch(s){
		    case FIOSUC :
		      nline++;
		      newline = 1;

		    case FIOLNG :
		      for(linep = line; charsread-- > 0; linep++)
			linsert(1, (unsigned char) *linep);

		      break;

		    default :
		      done++;
		      break;
		  }
	      }

	    ffclose();                              /* Ignore errors.       */
	}

	return(s != FIOERR && s != FIOFNF);		/* true if success */
}


/*
 * Ask for a file name, and write the
 * contents of the current buffer to that file.
 * Update the remembered file name and clear the
 * buffer changed flag. This handling of file names
 * is different from the earlier versions, and
 * is more compatable with Gosling EMACS than
 * with ITS EMACS. Bound to "C-X C-W".
 */
filewrite(f, n)
int f, n;
{
        register WINDOW *wp;
        register int    s;
        char            fname[NFILEN];
	char		shows[NLINE], origshows[NLINE], *bufp;
	EXTRAKEYS	menu_write[3];

	if(curbp->b_fname[0] != 0)
	  strcpy(fname, curbp->b_fname);
	else
	  fname[0] = '\0';

	menu_write[0].name  = "^T";
	menu_write[0].label = "To Files";
	menu_write[0].key   = (CTRL|'T');
	menu_write[1].name  = "TAB";
	menu_write[1].label = "Complete";
	menu_write[1].key   = (CTRL|'I');
	menu_write[2].name  = NULL;
	for(;!(gmode & MDTOOL);){
	    s = mlreplyd("File Name to write : ", fname, NFILEN,
			 QDEFLT|QFFILE, menu_write);

	    switch(s){
	      case FALSE:
		if(!fname[0]){			/* no file name to write to */
		    ctrlg(FALSE, 0);
		    return(s);
		}
	      case TRUE:
		if((gmode & MDTREE) && !compresspath(opertree, fname, NFILEN)){
		    emlwrite("Can't write outside of %s: too many ..'s",
			     opertree);
		    sleep(2);
		    continue;
		}
		else{
		    fixpath(fname, NFILEN);	/*  fixup ~ in file name  */
		    if((gmode & MDTREE) && !in_oper_tree(fname)){
			emlwrite("Can't write outside of %s", opertree);
			sleep(2);
			continue;
		    }
		}

		break;

	      case (CTRL|'I'):
		{
		    char *fn, *p, dir[NFILEN];
		    int   l = NFILEN;

		    dir[0] = '\0';
		    if(*fname && (p = strrchr(fname, C_FILESEP))){
			fn = p + 1;
			l -= fn - fname;
			if(p == fname)
			  strcpy(dir, S_FILESEP);
#ifdef	DOS
			else if(fname[0] == C_FILESEP
				 || (isalpha((unsigned char)fname[0])
				     && fname[1] == ':')){
#else
			else if (fname[0] == C_FILESEP || fname[0] == '~') {
#endif
			    strncpy(dir, fname, p - fname);
			    dir[p-fname] = '\0';
			}
			else
			  sprintf(dir, "%s%c%.*s", 
				  (gmode & MDCURDIR)
				     ? "."
				     : ((gmode & MDTREE) || opertree[0])
					  ? opertree : gethomedir(NULL),
				  C_FILESEP, p - fname, fname);
		    }
		    else{
			fn = fname;
			strcpy(dir, (gmode & MDCURDIR)
				       ? "."
				       : ((gmode & MDTREE) || opertree[0])
					    ? opertree : gethomedir(NULL));
		    }

		    if(!pico_fncomplete(dir, fn, l - 1))
		      (*term.t_beep)();
		}

		continue;
	      case (CTRL|'T'):
		/* If we have a file name, break up into path and file name.*/
		*shows = 0;
		if(*fname) {
		    if (isdir (fname, NULL, NULL)) {
			/* fname is a directory. */
			strcpy (shows, fname);
			*fname = '\0';
		    }
		    else {
			/* Find right most seperator. */
			bufp = strrchr (fname, C_FILESEP);
			if (bufp != NULL) {
			    /* Copy directory part to 'shows', and file
			     * name part to front of 'fname'. */
			    *bufp = '\0';
			    strcpy (shows, fname);
			    memcpy (fname, bufp+1, strlen (bufp+1) + 1);
			}
		    }
		}

		/* If we did not end up with a valid directory, use home. */
		if (!*shows || !isdir (shows, NULL, NULL))
		  strcpy(shows, ((gmode & MDTREE) || opertree[0])
			 ? opertree
			 : (browse_dir[0] ? browse_dir
			    : gethomedir(NULL)));

		strcpy(origshows, shows);
		if ((s = FileBrowse(shows, NLINE, fname, NFILEN, NULL,
				    FB_SAVE, NULL)) == 1) {
		  if (strlen(shows)+strlen(S_FILESEP)+strlen(fname) < NLINE){
		    strcat(shows, S_FILESEP);
		    strcat(shows, fname);
		    strcpy(fname, shows);
		  }
		  else {
		    emlwrite("Cannot write. File name too long!!",NULL);
		    sleep(3);
		  }
		}
		else if (s == 0 && strcmp(shows, origshows)){
		    strcat(shows, S_FILESEP);
		    strcat(shows, fname);
		    strcpy(fname, shows);
		}
		else if (s == -1){
		  emlwrite("Cannot write. File name too long!!",NULL);
		  sleep(3);
		}
		pico_refresh(FALSE, 1);
		update();
		if(s == 1)
		  break;
		else
		  continue;
	      case HELPCH:
		pico_help(writehelp, "", 1);
	      case (CTRL|'L'):
		pico_refresh(FALSE, 1);
		update();
		continue;
	      default:
		return(s);
		break;
	    }

	    if(strcmp(fname, curbp->b_fname) == 0)
		break;

	    if((s=fexist(fname, "w", (off_t *)NULL)) == FIOSUC){
						    /*exists overwrite? */

		sprintf(shows, "File \"%s\" exists, OVERWRITE", fname);
		if((s=mlyesno(shows, FALSE)) == TRUE)
		  break;
	    }
	    else if(s == FIOFNF){
		break;				/* go write it */
	    }
	    else{				/* some error, can't write */
		fioperr(s, fname);
		return(ABORT);
	    }
	}
	emlwrite("Writing...", NULL);

        if ((s=writeout(fname, 0)) != -1) {
	        if(!(gmode&MDTOOL)){
		    strcpy(curbp->b_fname, fname);
		    curbp->b_flag &= ~BFCHG;

		    wp = wheadp;                    /* Update mode lines.   */
		    while (wp != NULL) {
                        if (wp->w_bufp == curbp)
			    if((Pmaster && s == TRUE) || Pmaster == NULL)
                                wp->w_flag |= WFMODE;
                        wp = wp->w_wndp;
		    }
		}

		if(s > 1)
		  emlwrite("Wrote %d lines", (void *)s);
		else
		  emlwrite("Wrote 1 line", NULL);
        }
        return ((s == -1) ? FALSE : TRUE);
}



/*
 * Save the contents of the current
 * buffer in its associatd file. No nothing
 * if nothing has changed (this may be a bug, not a
 * feature). Error if there is no remembered file
 * name for the buffer. Bound to "C-X C-S". May
 * get called by "C-Z".
 */
filesave(f, n)
int f, n;
{
        register WINDOW *wp;
        register int    s;

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if ((curbp->b_flag&BFCHG) == 0)         /* Return, no changes.  */
                return (TRUE);
        if (curbp->b_fname[0] == 0) {           /* Must have a name.    */
                emlwrite("No file name", NULL);
		sleep(2);
                return (FALSE);
        }

	emlwrite("Writing...", NULL);
        if ((s=writeout(curbp->b_fname, 0)) != -1) {
                curbp->b_flag &= ~BFCHG;
                wp = wheadp;                    /* Update mode lines.   */
                while (wp != NULL) {
                        if (wp->w_bufp == curbp)
			  if(Pmaster == NULL)
                                wp->w_flag |= WFMODE;
                        wp = wp->w_wndp;
                }
		if(s > 1){
		    emlwrite("Wrote %d lines", (void *)s);
		}
		else
		  emlwrite("Wrote 1 line", NULL);
        }
        return (s);
}

/*
 * This function performs the details of file
 * writing. Uses the file management routines in the
 * "fileio.c" package. The number of lines written is
 * displayed. Sadly, it looks inside a LINE; provide
 * a macro for this. Most of the grief is error
 * checking of some sort.
 *
 * If the argument readonly is set, the file is created with user read
 * and write permission only if it doesn't already exist. Note that the
 * word readonly is misleading. It is user r/w permission only.
 *
 * CHANGES: 1 Aug 91: returns number of lines written or -1 on error, MSS
 */
writeout(fn, readonly)
char    *fn;
int     readonly;
{
        register int    s;
        register int    t;
        register LINE   *lp;
        register int    nline;

        if (!((s = ffwopen(fn, readonly)) == FIOSUC && ffelbowroom()))
	  return (-1);				/* Open writes message. */

        lp = lforw(curbp->b_linep);             /* First line.          */
        nline = 0;                              /* Number of lines.     */
        while (lp != curbp->b_linep) {
                if ((s=ffputline(&lp->l_text[0], llength(lp))) != FIOSUC)
                        break;
                ++nline;
                lp = lforw(lp);
        }

	t = ffclose();			/* ffclose complains if error */

	if(s == FIOSUC)
	  s = t;				/* report worst case */

	return ((s == FIOSUC) ? nline : -1);
}


/*
 * writetmp - write a temporary file for message text, mindful of 
 *	      access restrictions and included text.  If n is true, include
 *	      lines that indicated included message text, otw forget them
 *            If dir is non-null, put the temp file in that directory.
 */
char *writetmp(n, dir)
int   n;
char *dir;
{
        static   char	fn[NFILEN];
        register int    s;
        register int    t;
        register LINE   *lp;
        register int    nline;

        tmpname(dir, fn);

	/* Open writes message */
        if (!fn[0] || (s=ffwopen(fn, TRUE)) != FIOSUC){
	    if(fn[0])
	      (void)unlink(fn);

	    return(NULL);
	}

        lp = lforw(curbp->b_linep);             /* First line.          */
        nline = 0;                              /* Number of lines.     */
        while (lp != curbp->b_linep) {
	    if(n || (!n && lp->l_text[0].c != '>'))
	      if ((s=ffputline(&lp->l_text[0], llength(lp))) != FIOSUC)
		break;

	    ++nline;
	    lp = lforw(lp);
        }

	t = ffclose();			/* ffclose complains if error */

	if(s == FIOSUC)
	  s = t;				/* remember worst case */

	if (s != FIOSUC){                       /* Some sort of error.  */
	    (void)unlink(fn);
	    return(NULL);
	}

	return(fn);
}


/*
 * Insert file "fname" into the current
 * buffer, Called by insert file command. Return the final
 * status of the read.
 */
ifile(fname)
char    fname[];
{
        char line[NLINE], *linep;
	long nline;
	int  s, done, newline, charsread = 0;

        if ((s=ffropen(fname)) != FIOSUC){      /* Hard file open.      */
	    fioperr(s, fname);
	    return(FALSE);
	}

	gotobol(FALSE, 1);

        curbp->b_flag |= BFCHG;			/* we have changed	*/
	curbp->b_flag &= ~BFTEMP;		/* and are not temporary*/
	curbp->b_linecnt = -1;			/* must be recalculated */

        emlwrite("Inserting %s.", fname);
	done = newline = 0;
	nline = 0L;
	while(!done)
	  if((s = ffgetline(line, NLINE, &charsread, 1)) == FIOEOF){
	      if(llength(curwp->w_dotp) > curwp->w_doto)
		lnewline();
	      else
		forwchar(FALSE, 1);

	      sprintf(line,"Inserted %d line%s", nline, (nline>1) ? "s" : "");
	      emlwrite(line, NULL);
	      break;
	  }
	  else{
	      if(newline){
		  lnewline();
		  newline = 0;
	      }

	      switch(s){
		case FIOSUC :			/* copy line into buf */
		  nline++;
		  newline = 1;

		case FIOLNG :
		  for(linep = line; charsread-- > 0; linep++)
		    linsert(1, (unsigned char) *linep);

		  break;

		default :
		  done++;
	      }
	  }

        ffclose();                              /* Ignore errors.       */

	return(s != FIOERR);
}



/*
 * pico_fncomplete - pico's function to complete the given file name
 */
int pico_fncomplete(dir, fn, len)
char *dir, *fn;
int   len;
{
    char *p, *dlist, tmp[NLINE], dtmp[NLINE];
    int   n, i, match = -1;
#ifdef	DOS
#define	FILECMP(x, y)	(toupper((unsigned char)(x))\
				== toupper((unsigned char)(y)))
#else
#define	FILECMP(x, y)	((x) == (y))
#endif

    strcpy(dtmp, dir);
    pfnexpand(dir = dtmp, NLINE);
    if(*fn && (dlist = p = getfnames(dir, fn, &n, NULL))){
	memset(tmp, 0, sizeof(tmp));
	while(n--){			/* any names in it */
	    for(i = 0; fn[i] && FILECMP(p[i], fn[i]); i++)
	      ;

	    if(!fn[i]){			/* match or more? */
		if(tmp[0]){
		    for(; p[i] && FILECMP(p[i], tmp[i]); i++)
		      ;

		    match = !p[i] && !tmp[i];
		    tmp[i] = '\0';	/* longest common string */
		}
		else{
		    match = 1;		/* may be it!?! */
		    strcpy(tmp,  p);
		}
	    }

	    p += strlen(p) + 1;
	}

	free(dlist);
    }

    if(match >= 0){
	strncpy(fn, tmp, len);
	fn[len] = '\0';
	if(match == 1){
	  if ((strlen(dir)+strlen(S_FILESEP)+strlen(fn)) < len){
	    strcat(dir, S_FILESEP);
	    strcat(dir, fn);
	    if(isdir(dir, NULL, NULL))
	      strcat(fn, S_FILESEP);
	  }
	  else{
	    emlwrite("File name too BIG!!",0);
	    sleep(3);
	    *fn = '\0';
	  }

	}
    }

    return(match == 1);

}


/*
 * in_oper_tree - returns true if file "f" does reside in opertree
 */
in_oper_tree(f)
char *f;
{
    int end = strlen(opertree);

    return(!strncmp(opertree, f, end)
	    && (opertree[end-1] == '/'
		|| opertree[end-1] == '\\'
		|| f[end] == '\0'
		|| f[end] == '/'
		|| f[end] == '\\'));
}
