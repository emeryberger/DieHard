#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pilot.c 13703 2004-06-11 21:49:40Z hubert $";
#endif
/*
 * Program:	Main stand-alone Pine File Browser routines
 *
 * Author:	Michael Seibel
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: mikes@cac.washington.edu
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
 * BROWSER NOTES:
 *
 * 30 Sep 92 - Stand alone PIne's "Lister of Things" came into being.
 *	       It's built against libpico.a from a command line like:
 *
 *		       cc pilot.c libpico.a -ltermcap -lc -o pilot
 *
 *	       should it become a fleshed out tool, we'll move it into
 *	       the normal build process.
 */

#include	"headers.h"


#define		PILOT_VERSION	"UW PILOT 2.0"


extern char *gethomedir();

char *pilot_args PROTO((int, char **, int *, int *));
void  pilot_args_help PROTO((void));
void  pilot_display_args_err PROTO((char *, char **, int));

char  args_pilot_missing_flag[]  = "unknown flag \"%c\"";
char  args_pilot_missing_arg[]   = "missing or empty argument to \"%c\" flag";
char  args_pilot_missing_num[]   = "non numeric argument for \"%c\" flag";
char  args_pilot_missing_color[] = "missing color for \"%s\" flag";

char *args_pilot_args[] = {
"Possible Starting Arguments for Pilot file browser:",
"",
"\tArgument\t\tMeaning",
"\t -a \t\tShowDot - show dot files in file browser",
"\t -j \t\tGoto - allow 'Goto' command in file browser",
"\t -g \t\tShow - show cursor in file browser",
"\t -m \t\tMouse - turn on mouse support",
"\t -v \t\tOneColumn - use single column display",
"\t -x \t\tNoKeyhelp - suppress keyhelp",
"\t -q \t\tTermdefWins - termcap or terminfo takes precedence over defaults",
"\t -f \t\tKeys - force use of function keys",
"\t -h \t\tHelp - give this list of options",
"\t -n[#s] \tMail - notify about new mail every #s seconds, default=180",
"\t -t \t\tShutdown - enable special shutdown mode",
"\t -o <dir>\tOperation - specify the operating directory",
"\t -z \t\tSuspend - allow use of ^Z suspension",
#if     defined(DOS) || defined(OS2)
"\t -cnf color \tforeground color",
"\t -cnb color \tbackground color",
"\t -crf color \treverse foreground color",
"\t -crb color \treverse background color",
#endif
"\t -setlocale_ctype\tdo setlocale(LC_CTYPE) if available",
"\t -no_setlocale_collate\tdo not do setlocale(LC_COLLATE)",
"", 
"\t All arguments may be followed by a directory name to start in.",
"",
NULL
};


/*
 * main standalone browser routine
 */
main(argc, argv)
char    *argv[];
{
    char  bname[NBUFN];		/* buffer name of file to read	*/
    char  filename[NSTRING];
    char  filedir[NSTRING];
    char *dir;
    int   setlocale_collate = 1;
    int   setlocale_ctype = 0;
 
    timeo = 0;
    Pmaster = NULL;			/* turn OFF composer functionality */
    km_popped = 0;
    opertree[0] = '\0'; opertree[NLINE] = '\0';
    filename[0] ='\0';
    gmode |= MDBRONLY;			/* turn on exclusive browser mode */

    /*
     * Read command line flags before initializing, otherwise, we never
     * know to init for f_keys...
     */
    if(dir = pilot_args(argc, argv, &setlocale_collate, &setlocale_ctype)){
	strcpy(filedir, dir);
	fixpath(filedir, NSTRING);
    }
    else
      strcpy(filedir, gethomedir(NULL));

    set_collation(setlocale_collate, setlocale_ctype);

    if(!vtinit())			/* Displays.            */
      exit(1);

    strcpy(bname, "main");		/* default buffer name */
    edinit(bname);			/* Buffers, windows.   */
#if	defined(USE_TERMCAP) || defined(USE_TERMINFO) || defined(VMS)
    if(kbesc == NULL){			/* will arrow keys work ? */
	(*term.t_putchar)('\007');
	emlwrite("Warning: keypad keys may be non-functional", NULL);
    }
#endif	/* USE_TERMCAP/USE_TERMINFO/VMS */

    curbp->b_mode |= gmode;		/* and set default modes*/
    if(timeo)
      emlwrite("Checking for new mail every %D seconds", (void *) timeo);

    set_browser_title(PILOT_VERSION);
    FileBrowse(filedir, NSTRING, filename, NSTRING, NULL, 0, NULL);
    wquit(1, 0);
}


/*
 *  Parse the command line args.
 *
 * Args      ac
 *           av
 *
 * Result: command arguments parsed
 *       possible printing of help for command line
 *       various global flags set
 *       returns the name of directory to start in if specified, else NULL
 */
char *
pilot_args(ac, av, setlocale_collate, setlocale_ctype)
    int    ac;
    char **av;
    int   *setlocale_collate;
    int   *setlocale_ctype;
{
    int   c, usage = 0;
    char *str;
    char  tmp_1k_buf[1000];     /* tmp buf to contain err msgs  */ 

Loop:
    /* while more arguments with leading - */
    while(--ac > 0 && **++av == '-'){
      /* while more chars in this argument */
      while(*++*av){

	if(strcmp(*av, "setlocale_ctype") == 0){
	    *setlocale_ctype = 1;
	    goto Loop;
	}
	else if(strcmp(*av, "no_setlocale_collate") == 0){
	    *setlocale_collate = 0;
	    goto Loop;
	}
#if	defined(DOS) || defined(OS2)
	if(strcmp(*av, "cnf") == 0
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
		sprintf(tmp_1k_buf, args_pilot_missing_color, cmd);
		pilot_display_args_err(tmp_1k_buf, NULL, 1);
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
	    gmode ^= MDDOTSOK;		/* show dot files */
	    break;
	  case 'f':			/* -f for function key use */
	    gmode ^= MDFKEY;
	    break;
	  case 'j':			/* allow "Goto" in file browser */
	    gmode ^= MDGOTO;
	    break;
	  case 'g':			/* show-cursor in file browser */
	    gmode ^= MDSHOCUR;
	    break;
	  case 'm':			/* turn on mouse support */
	    gmode ^= MDMOUSE;
	    break;
	  case 'v':			/* single column display */
	    gmode ^= MDONECOL;
	    break;			/* break back to inner-while */
	  case 'x':			/* suppress keyhelp */
	    sup_keyhelp = !sup_keyhelp;
	    break;
	  case 'q':			/* -q for termcap takes precedence */
	    gmode ^= MDTCAPWINS;
	    break;
	  case 'z':			/* -z to suspend */
	    gmode ^= MDSSPD;
	    break;
	  case 'h':
	    usage++;
	    break;

	  /*
	   * These do take arguments.
	   */
	  case 'n':			/* -n for new mail notification */
	  case 'o' :			/* operating tree */
	    if(*++*av)
	      str = *av;
	    else if(--ac)
	      str = *++av;
	    else{
	      sprintf(tmp_1k_buf, args_pilot_missing_arg, c);
	      pilot_display_args_err(tmp_1k_buf, NULL, 1);
	      usage++;
	      goto Loop;
	    }

	    switch(c){
	      case 'o':
		strncpy(opertree, str, NLINE);
		gmode ^= MDTREE;
		break;

	/* numeric args */
	      case 'n':
		if(!isdigit((unsigned char)str[0])){
		  sprintf(tmp_1k_buf, args_pilot_missing_num, c);
		  pilot_display_args_err(tmp_1k_buf, NULL, 1);
		  usage++;
		}

		timeo = 180;
		if((timeo = atoi(str)) < 30)
		  timeo = 180;
		
		break;
	    }

	    goto Loop;

	  default:			/* huh? */
	    sprintf(tmp_1k_buf, args_pilot_missing_flag, c);
	    pilot_display_args_err(tmp_1k_buf, NULL, 1);
	    usage++;
	    break;
	}
      }
    }

    if(usage)
      pilot_args_help();

    /* return the directory */
    if(ac > 0)
      return(*av);
    else
      return(NULL);
}


/*----------------------------------------------------------------------
    print a few lines of help for command line arguments

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
pilot_args_help()
{
    /**  print out possible starting arguments... **/
    pilot_display_args_err(NULL, args_pilot_args, 0);
    exit(1);
}


/*----------------------------------------------------------------------
   write argument error to the display...

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
pilot_display_args_err(s, a, err)
    char  *s;
    char **a;
    int    err;
{
    char  errstr[256], *errp;
    FILE *fp = err ? stderr : stdout;

    if(err && s)
      sprintf(errp = errstr, "Argument Error: %.200s", s);
    else
      errp = s;

    if(errp)
      fprintf(fp, "%s\n", errp);

    while(a && *a)
      fprintf(fp, "%s\n", *a++);
}
