#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: main.c 13547 2004-03-26 22:36:45Z hubert $";
#endif
/*
 * Program:	Main stand-alone Pine Composer routines
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
 *
 * WEEMACS/PICO NOTES:
 *
 * 08 Jan 92 - removed PINE defines to simplify compiling
 *
 * 08 Apr 92 - removed PINE stub calls
 *
 */

#include	"headers.h"


/*
 * Useful internal prototypes
 */
#ifdef	_WINDOWS
int	pico_file_drop PROTO((int, int, char *));
#endif

/*
 * this isn't defined in the library, because it's a pine global
 * which we use for GetKey's timeout
 */
int	timeoutset = 0;


int	 my_timer_period = (300 * 1000);

/*
 * function key mappings
 */
static int fkm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'X')},
    { F3,  (CTRL|'O')},
    { F4,  (CTRL|'J')},
    { F5,  (CTRL|'R')},
    { F6,  (CTRL|'W')},
    { F7,  (CTRL|'Y')},
    { F8,  (CTRL|'V')},
    { F9,  (CTRL|'K')},
    { F10, (CTRL|'U')},
    { F11, (CTRL|'C')},
#ifdef	SPELLER
    { F12, (CTRL|'T')}
#else
    { F12, (CTRL|'D')}
#endif
};

char *pico_args PROTO((int, char **, int *, int *, int *, int *));
void  pico_args_help PROTO((void));
void  pico_vers_help PROTO((void));
void  pico_display_args_err PROTO((char *, char **, int));

char  args_pico_missing_flag[]  = "unknown flag \"%c\"";
char  args_pico_missing_arg[]   = "missing or empty argument to \"%c\" flag";
char  args_pico_missing_num[]   = "non numeric argument for \"%c\" flag";
char  args_pico_missing_color[] = "missing color for \"%s\" flag";

char *args_pico_args[] = {
"Possible Starting Arguments for Pico editor:",
"",
"\tArgument\t\tMeaning",
"\t -e \t\tComplete - allow file name completion",
"\t -k \t\tCut - let ^K cut from cursor position to end of line",
"\t -a \t\tShowDot - show dot files in file browser",
"\t -j \t\tGoto - allow 'Goto' command in file browser",
"\t -g \t\tShow - show cursor in file browser",
"\t -m \t\tMouse - turn on mouse support",
"\t -x \t\tNoKeyhelp - suppress keyhelp",
"\t -p \t\tPreserveStartStop - preserve \"start\"(^Q) and \"stop\"(^S) characters",
"\t -q \t\tTermdefWins - termcap or terminfo takes precedence over defaults",
"\t -Q <quotestr> \tSet quote string (eg. \"> \") esp. for composing email",
"\t -d \t\tRebind - let delete key delete current character",
"\t -f \t\tKeys - force use of function keys",
"\t -b \t\tReplace - allow search and replace",
"\t -h \t\tHelp - give this list of options",
"\t -r[#cols] \tFill - set fill column to #cols columns, default=72",
"\t -n[#s] \tMail - notify about new mail every #s seconds, default=180",
"\t -s <speller> \tSpeller - specify alternative speller",
"\t -t \t\tShutdown - enable special shutdown mode",
"\t -o <dir>\tOperation - specify the operating directory",
"\t -z \t\tSuspend - allow use of ^Z suspension",
"\t -w \t\tNoWrap - turn off word wrap",
#if     defined(DOS) || defined(OS2)
"\t -cnf color \tforeground color",
"\t -cnb color \tbackground color",
"\t -crf color \treverse foreground color",
"\t -crb color \treverse background color",
#endif
"\t +[line#] \tLine - start on line# line, default=1",
"\t -v \t\tView - view file",
"\t -setlocale_ctype\tdo setlocale(LC_CTYPE) if available",
"\t -no_setlocale_collate\tdo not do setlocale(LC_COLLATE)",
"\t -version\tPico version number",
"", 
"\t All arguments may be followed by a file name to display.",
"",
NULL
};


/*
 * main standalone pico routine
 */
#ifdef _WINDOWS
app_main (argc, argv)
#else
main(argc, argv)
#endif
char    *argv[];
{
    register int    c;
    register int    f;
    register int    n;
    register BUFFER *bp;
    int	     viewflag = FALSE;		/* are we starting in view mode?*/
    int	     starton = 0;		/* where's dot to begin with?	*/
    int      setlocale_collate = 1;
    int      setlocale_ctype = 0;
    char     bname[NBUFN];		/* buffer name of file to read	*/
    char    *file_to_edit = NULL;

    timeo = 600;
    Pmaster = NULL;     		/* turn OFF composer functionality */
    km_popped = 0;
    opertree[0] = opertree[NLINE] = '\0';
    browse_dir[0] = browse_dir[NLINE] = '\0';

    /*
     * Read command line flags before initializing, otherwise, we never
     * know to init for f_keys...
     */
    file_to_edit = pico_args(argc, argv, &starton, &viewflag,
			     &setlocale_collate, &setlocale_ctype);

    set_collation(setlocale_collate, setlocale_ctype);

#if	defined(DOS) || defined(OS2)
    if(file_to_edit){			/* strip quotes? */
	int   l;

	if(strchr("'\"", file_to_edit[0])
	   && (l = strlen(file_to_edit)) > 1
	   && file_to_edit[l-1] == file_to_edit[0]){
	    file_to_edit[l-1] = '\0';	/* blat trailing quote */
	    file_to_edit++;		/* advance past leading quote */
	}
    }
#endif

    if(!vtinit())			/* Displays.            */
	exit(1);

    strcpy(bname, "main");		/* default buffer name */
    edinit(bname);			/* Buffers, windows.   */

    update();				/* let the user know we are here */

#ifdef	_WINDOWS
    mswin_setwindow(NULL, NULL, NULL, NULL, NULL, NULL);
    mswin_showwindow();
    mswin_showcaret(1);			/* turn on for main window */
    mswin_allowpaste(MSWIN_PASTE_FULL);
    mswin_setclosetext("Use the ^X command to exit Pico.");
    mswin_setscrollcallback (pico_scroll_callback);
#endif

#if	defined(USE_TERMCAP) || defined(USE_TERMINFO) || defined(VMS)
    if(kbesc == NULL){			/* will arrow keys work ? */
	(*term.t_putchar)('\007');
	emlwrite("Warning: keypad keys may non-functional", NULL);
    }
#endif	/* USE_TERMCAP/USE_TERMINFO/VMS */

    if(file_to_edit){			/* Any file to edit? */

	makename(bname, file_to_edit);	/* set up a buffer for this file */

	bp = curbp;			/* read in first file */
	makename(bname, file_to_edit);
	strcpy(bp->b_bname, bname);

	if(strlen(file_to_edit) >= NFILEN){
	    char buf[128];

	    sprintf(buf, "Filename \"%.10s...\" too long", file_to_edit);
	    emlwrite(buf, NULL);
	    file_to_edit = NULL;
	}
	else{
	    strcpy(bp->b_fname, file_to_edit);
	    if ((gmode&MDTREE) && !in_oper_tree(file_to_edit) ||
		readin(file_to_edit, (viewflag==FALSE), TRUE) == ABORT) {
		if ((gmode&MDTREE) && !in_oper_tree(file_to_edit))
		  emlwrite("Can't read file from outside of %s", opertree);

		file_to_edit = NULL;
	    }
	}

	if(!file_to_edit){
	    strcpy(bp->b_bname, "main");
	    strcpy(bp->b_fname, "");
	}

	bp->b_dotp = bp->b_linep;
	bp->b_doto = 0;

	if (viewflag)			/* set the view mode */
	  bp->b_mode |= MDVIEW;
    }

    /* setup to process commands */
    lastflag = 0;			/* Fake last flags.     */
    curbp->b_mode |= gmode;		/* and set default modes*/

    curwp->w_flag |= WFMODE;		/* and force an update	*/

    if(timeoutset)
      emlwrite("Checking for new mail every %D seconds", (void *)timeo);

    forwline(0, starton - 1);		/* move dot to specified line */

    while(1){

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
	register_mfunc(mouse_in_pico, 2, 0, term.t_nrow - (term.t_mrow + 1),
		       term.t_ncol);
#else
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);
	register_mfunc(mouse_in_content, 2, 0, term.t_nrow - (term.t_mrow + 1),
		       term.t_ncol);
#endif
#endif
#ifdef	_WINDOWS
	mswin_setdndcallback (pico_file_drop);
	mswin_mousetrackcallback(pico_cursor);
#endif
	c = GetKey();
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

	if(timeoutset && (c == NODATA || time_to_check())){
	    if(pico_new_mail())
	      emlwrite("You may possibly have new mail.", NULL);
	}

	if(km_popped)
	  switch(c){
	    case NODATA:
	    case (CTRL|'L'):
	      km_popped++;
	      break;
	    
	    default:
	      /* clear bottom three lines */
	      mlerase();
	      break;
	  }

	if(c == NODATA)
	  continue;

	if(mpresf){			/* erase message line? */
	    if(mpresf++ > MESSDELAY)
	      mlerase();
	}

	f = FALSE;
	n = 1;

#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
					/* Do it.               */
	execute(normalize_cmd(c, fkm, 1), f, n);
    }
}


/*
 *  Parse the command line args.
 *
 * Args      ac
 *           av
 *      starton -- place to return starton value
 *     viewflag -- place to return viewflag value
 *
 * Result: command arguments parsed
 *       possible printing of help for command line
 *       various global flags set
 *       returns the name of any file to open, else NULL
 */
char *
pico_args(ac, av, starton, viewflag, setlocale_collate, setlocale_ctype)
    int    ac;
    char **av;
    int   *starton;
    int   *viewflag;
    int   *setlocale_collate;
    int   *setlocale_ctype;
{
    int   c, usage = 0;
    char *str;
    char  tmp_1k_buf[1000];     /* tmp buf to contain err msgs  */ 

Loop:
    /* while more arguments with leading - or + */
    while(--ac > 0 && (**++av == '-' || **av == '+')){
      if(**av == '+'){
	if(*++*av)
	  str = *av;
	else if(--ac)
	  str = *++av;
	else{
	  sprintf(tmp_1k_buf, args_pico_missing_arg, '+');
	  pico_display_args_err(tmp_1k_buf, NULL, 1);
	  usage++;
	  goto Loop;
	}

	if(!isdigit((unsigned char)str[0])){
	  sprintf(tmp_1k_buf, args_pico_missing_num, '+');
	  pico_display_args_err(tmp_1k_buf, NULL, 1);
	  usage++;
	}

	if(starton)
	  *starton = atoi(str);

	goto Loop;
      }
      
      /* while more chars in this argument */
      else while(*++*av){

	if(strcmp(*av, "version") == 0){
	    pico_vers_help();
	}
	else if(strcmp(*av, "setlocale_ctype") == 0){
	    *setlocale_ctype = 1;
	    goto Loop;
	}
	else if(strcmp(*av, "no_setlocale_collate") == 0){
	    *setlocale_collate = 0;
	    goto Loop;
	}
#if	defined(DOS) || defined(OS2)
	else if(strcmp(*av, "cnf") == 0
	   || strcmp(*av, "cnb") == 0
	   || strcmp(*av, "crf") == 0
	   || strcmp(*av, "crb") == 0){

	    char *cmd = *av; /* save it to use below */

	    if(--ac){
		str = *++av;
		if(cmd[1] == 'n'){
		    if(cmd[2] == 'f')
		      pico_nfcolor(str);
		    else if(cmd[2] == 'b')
		      pico_nbcolor(str);
		}
		else if(cmd[1] == 'r'){
		    if(cmd[2] == 'f')
		      pico_rfcolor(str);
		    else if(cmd[2] == 'b')
		      pico_rbcolor(str);
		}
	    }
	    else{
		sprintf(tmp_1k_buf, args_pico_missing_color, cmd);
		pico_display_args_err(tmp_1k_buf, NULL, 1);
	        usage++;
	    }

	    goto Loop;
	}
#endif

	/*
	 * Single char options.
	 */
	switch(c = **av){
	  /*
	   * These don't take arguments.
	   */
          case 'a':
            gmode ^= MDDOTSOK;          /* show dot files */
            break;
	  case 'b':
	    gmode ^= MDREPLACE;  /* -b for replace string in where is command */
	    break;
	  case 'd':			/* -d for rebind delete key */
	    bindtokey(0x7f, forwdel);
	    break;
	  case 'e':			/* file name completion */
	    gmode ^= MDCMPLT;
	    break;
	  case 'f':			/* -f for function key use */
	    gmode ^= MDFKEY;
	    break;
	  case 'g':			/* show-cursor in file browser */
	    gmode ^= MDSHOCUR;
	    break;
	  case 'h':
	    usage++;
	    break;
	  case 'j':			/* allow "Goto" in file browser */
	    gmode ^= MDGOTO;
	    break;
	  case 'k':			/* kill from dot */
	    gmode ^= MDDTKILL;
	    break;
	  case 'm':			/* turn on mouse support */
	    gmode ^= MDMOUSE;
	    break;
	  case 'p':
	    preserve_start_stop = 1;
	    break;
	  case 'q':			/* -q for termcap takes precedence */
	    gmode ^= MDTCAPWINS;
	    break;
	  case 't':			/* special shutdown mode */
	    gmode ^= MDTOOL;
	    rebindfunc(wquit, quickexit);
	    break;
	  case 'v':			/* -v for View File */
	  case 'V':
	    *viewflag = !*viewflag;
	    break;			/* break back to inner-while */
	  case 'w':			/* -w turn off word wrap */
	    gmode ^= MDWRAP;
	    break;
	  case 'x':			/* suppress keyhelp */
	    sup_keyhelp = !sup_keyhelp;
	    break;
	  case 'z':			/* -z to suspend */
	    gmode ^= MDSSPD;
	    break;

	  /*
	   * These do take arguments.
	   */
	  case 'r':			/* set fill column */
	  case 'n':			/* -n for new mail notification */
	  case 's' :			/* speller */
	  case 'o' :			/* operating tree */
	  case 'Q' :                    /* Quote string */
	    if(*++*av)
	      str = *av;
	    else if(--ac)
	      str = *++av;
	    else{
	      if(c == 'r')
		str= "72";
	      else if(c == 'n')
		str = "180";
	      else{
		  sprintf(tmp_1k_buf, args_pico_missing_arg, c);
		  pico_display_args_err(tmp_1k_buf, NULL, 1);
		  usage++;
		  goto Loop;
	      }
	    }

	    switch(c){
	      case 's':
	        alt_speller = str;
		break;
	      case 'o':
		strncpy(opertree, str, NLINE);
		gmode ^= MDTREE;
		break;
	      case 'Q':
		strncpy(glo_quote_str_buf, str, NLINE);
		glo_quote_str = glo_quote_str_buf;
		break;

	/* numeric args */
	      case 'r':
	      case 'n':
		if(!isdigit((unsigned char)str[0])){
		  sprintf(tmp_1k_buf, args_pico_missing_num, c);
		  pico_display_args_err(tmp_1k_buf, NULL, 1);
		  usage++;
		}

		if(c == 'r'){
		    if((userfillcol = atoi(str)) < 1)
		      userfillcol = 72;
		}
		else{
		    timeoutset = 1;
		    timeo = 180;
		    if((timeo = atoi(str)) < 30)
		      timeo = 180;
		}
		
		break;
	    }

	    goto Loop;

	  default:			/* huh? */
	    sprintf(tmp_1k_buf, args_pico_missing_flag, c);
	    pico_display_args_err(tmp_1k_buf, NULL, 1);
	    usage++;
	    break;
	}
      }
    }

    if(usage)
      pico_args_help();

    /* return the first filename for editing */
    if(ac > 0)
      return(*av);
    else
      return(NULL);
}


#ifdef	_WINDOWS
/*
 *
 */
int
pico_file_drop(x, y, filename)
    int   x, y;
    char *filename;
{
    /*
     * if current buffer is unchanged
     * *or* "new buffer" and no current text
     */
    if(((curwp->w_bufp->b_flag & BFCHG) == 0)
       || (curwp->w_bufp->b_fname[0] == '\0'
	   && curwp->w_bufp->b_linep == lforw(curwp->w_bufp->b_linep)
	   && curwp->w_doto == 0)){
	register BUFFER *bp = curwp->w_bufp;
	char     bname[NBUFN];

	makename(bname, filename);
	strcpy(bp->b_bname, bname);
	strcpy(bp->b_fname, filename);
	bp->b_flag &= ~BFCHG;	/* turn off change bit */
	if (readin(filename, 1, 1) == ABORT) {
	    strcpy(bp->b_bname, "");
	    strcpy(bp->b_fname, "");
	}
	bp->b_dotp = bp->b_linep;
	bp->b_doto = 0;
    }
    else{
	ifile(filename);
	curwp->w_flag |= WFHARD;
	update();
	emlwrite("Inserted dropped file \"%s\"", filename);
    }

    curwp->w_flag |= WFHARD;
    update();			/* restore cursor */
    return(1);
}
#endif


/*----------------------------------------------------------------------
    print a few lines of help for command line arguments

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
pico_args_help()
{
    /**  print out possible starting arguments... **/
    pico_display_args_err(NULL, args_pico_args, 0);
    exit(1);
}


void
pico_vers_help()
{
    char v0[100];
    char *v[2];

    sprintf(v0, "Pico %.50s", version);
    v[0] = v0;
    v[1] = NULL;

    pico_display_args_err(NULL, v, 0);
    exit(1);
}


/*----------------------------------------------------------------------
   write argument error to the display...

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
pico_display_args_err(s, a, err)
    char  *s;
    char **a;
    int    err;
{
    char  errstr[256], *errp;
    FILE *fp = err ? stderr : stdout;
#ifdef _WINDOWS
    char         tmp_20k_buf[20480];
#endif


    if(err && s)
      sprintf(errp = errstr, "Argument Error: %.200s", s);
    else
      errp = s;

#ifdef  _WINDOWS
    if(errp)
      mswin_messagebox(errp, err);

    if(a && *a){
        strcpy(tmp_20k_buf, *a++);
        while(a && *a){
            strcat(tmp_20k_buf, "\n");
            strcat(tmp_20k_buf, *a++);
        }

        mswin_messagebox(tmp_20k_buf, err);
    }
#else
    if(errp)
      fprintf(fp, "%s\n", errp);

    while(a && *a)
      fprintf(fp, "%s\n", *a++);
#endif
}
