#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: args.c 13984 2005-03-10 01:10:08Z jpf $";
#endif
/*----------------------------------------------------------------------

            T H E    P I N E    M A I L   S Y S T E M

   Laurence Lundblade and Mike Seibel
   Networks and Distributed Computing
   Computing and Communications
   University of Washington
   Administration Builiding, AG-44
   Seattle, Washington, 98195, USA
   Internet: lgl@CAC.Washington.EDU
             mikes@CAC.Washington.EDU

   Please address all bugs and comments to "pine-bugs@cac.washington.edu"


   Pine and Pico are registered trademarks of the University of Washington.
   No commercial use of these trademarks may be made without prior written
   permission of the University of Washington.

   Pine, Pico, and Pilot software and its included text are Copyright
   1989-2003 by the University of Washington.

   The full text of our legal notices is contained in the file called
   CPYRIGHT, included with this distribution.


   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    * 			Copyright (c) 1986, 1987 Dave Taylor              *
    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************
 

  ----------------------------------------------------------------------*/

/*======================================================================
      args.c
      Command line argument parsing functions 

  ====*/

#ifdef	OS2
#define	INCL_DOS
#define	INCL_NOPM
#define INCL_VIO
#include <os2.h>
#undef ADDRESS
#endif
#include "headers.h"

int  process_debug_str PROTO((char *));
void args_add_attach PROTO((PATMT **, char *, int));
#ifdef	_WINDOWS
#endif


/*
 * Name started as to invoke function key mode
 */
#define	PINE_FKEY_NAME	"pinef"

/*
 * Various error and informational strings..
 */
char args_err_missing_pinerc[] =	"missing argument for option \"-pinerc\" (use - for standard out)";
char args_err_missing_aux[] =		"missing argument for option \"-aux\"";
char args_err_missing_passfile[] =	"missing argument for option \"-passfile\"";
char args_err_non_abs_passfile[] =	"argument to \"-passfile\" should be fully-qualified";
char args_err_missing_lu[] =		"missing argument for option \"-create_lu\"\nUsage: pine -create_lu <addrbook_file> <addrbook_sort_type>";
char args_err_missing_sort[] =		"missing argument for option \"-sort\"";
char args_err_missing_flag_arg[] =	"missing argument for flag \"%c\"";
char args_err_missing_flag_num[] =	"Non numeric argument for flag \"%c\"";
char args_err_missing_debug_num[] =	"Non numeric argument for \"%s\"";
char args_err_missing_url[] =		"missing URL for \"-url\"";
char args_err_missing_attachment[] =	"missing attachment for \"%s\"";
char args_err_conflict[] =		"conflicting action: \"%s\"";
char args_err_unknown[] =		"unknown flag \"%c\"";
char args_err_I_error[] =		"-I argument \"%s\": %s";
char args_err_d_error[] =		"-d argument \"%s\": %s";
char args_err_internal[] =		"%s";
char args_err_missing_copyprc[] =	"missing argument for option \"-copy_pinerc\"\nUsage: pine -copy_pinerc <local_pinerc> <remote_pinerc>";
char args_err_missing_copyabook[] =	"missing argument for option \"-copy_abook\"\nUsage: pine -copy_abook <local_abook> <remote_abook>";


char *args_pine_args[] = {
"Possible Starting Arguments for Pine program:",
"",
" Argument\tMeaning",
" <addrs>...\tGo directly into composer sending to given address",
"\t\tList multiple addresses with a single space between them.",
"\t\tStandard input redirection is allowed with addresses.",
"\t\tNote: Places addresses in the \"To\" field only.",
" -attach <file>\tGo directly into composer with given file",
" -attachlist <file-list>",
" -attach_and_delete <file>",
"\t\tGo to composer, attach file, delete when finished",
"\t\tNote: Attach options can't be used if -f, -F",
"\t\tadded to Attachment list.  Attachlist must be the last",
"\t\toption on the command line",
" -bail\t\tExit if pinerc file doesn't already exist",
#ifdef	DEBUG
" -d n\t\tDebug - set debug level to 'n', or use the following:",
" -d keywords...\tflush,timestamp,imap=0..4,tcp,numfiles=0..31,verbose=0..9",
#endif
" -f <folder>\tFolder - give folder name to open",
" -c <number>\tContext - which context to apply to -f arg",
" -F <file>\tFile - give file name to open and page through and",
"\t\tforward as email.",
" -h \t\tHelp - give this list of options",
" -k \t\tKeys - Force use of function keys",
" -z \t\tSuspend - allow use of ^Z suspension",
" -r \t\tRestricted - can only send mail to oneself",
" -sort <sort>\tSort - Specify sort order of folder:",
"\t\t       subject, arrival, date, from, size, /reverse",
" -i\t\tIndex - Go directly to index, bypassing main menu",
" -I <keystroke_list>   Initial keystrokes to be executed",
" -n <number>\tEntry in index to begin on",
" -o \t\tReadOnly - Open first folder read-only",
" -conf\t\tConfiguration - Print out fresh global configuration. The",
"\t\tvalues of your global configuration affect all Pine users",
"\t\ton your system unless they have overridden the values in their",
"\t\tpinerc files.",
" -pinerc <file>\tConfiguration - Put fresh pinerc configuration in <file>",
" -p <pinerc>\tUse alternate .pinerc file",
#if	!defined(DOS) && !defined(OS2)
" -P <pine.conf>\tUse alternate pine.conf file",
#else
" -aux <aux_files_dir>\tUse this with remote pinerc",
" -P <pine.conf>\tUse pine.conf file for default settings",
" -nosplash \tDisable the PC-Pine splash screen",
#endif
#ifdef PASSFILE
" -passfile <fully_qualified_filename>\tSet the password file to something other",
"\t\tthan the default",
" -nowrite_passfile\tRead from a passfile if there is one, but never offer to write a password to the passfile",
#endif /* PASSFILE */
" -x <config>\tUse configuration exceptions in <config>.",
"\t\tExceptions are used to override your default pinerc",
"\t\tsettings for a particular platform, can be a local file or",
"\t\ta remote folder.",
" -v \t\tVersion - show version information",
" -version\tVersion - show version information",
" -supported\tList supported options",
#if defined(OS2)
" -w <rows>\tSet window size in rows on startup",
#endif
" -url <url>\tOpen the given URL",
"\t\tNote: Can't be used if -f, -F",
"\t\tStandard input redirection is not allowed with URLs.",
"\t\tFor mailto URLs, 'body='text should be used in place of",
"\t\tinput redirection.",
" -create_lu <abook_file> <ab_sort_type>       create .lu from script",
" -copy_pinerc <local_pinerc> <remote_pinerc>  copy local pinerc to remote",
" -copy_abook <local_abook> <remote_abook>     copy local addressbook to remote",
" -convert_sigs -p <pinerc>   convert signatures to literal signatures",
#if	defined(_WINDOWS)
" -install \tPrompt for some basic setup information",
" -registry <cmd>\tWhere cmd is set,noset,clear,clearsilent,dump",
#endif
" -<option>=<value>   Assign <value> to the pinerc option <option>",
"\t\t     e.g. -signature-file=sig1",
"\t\t     e.g. -color-style=no-color",
"\t\t     e.g. -feature-list=enable-sigdashes",
"\t\t     Note: feature-list is additive.",
"\t\t     You may leave off the \"feature-list=\" part of that,",
"\t\t     e.g. -enable-sigdashes",
NULL
};



/*
 *  Parse the command line args.
 *
 *  Args: pine_state -- The pine_state structure to put results in
 *        argc, argv -- The obvious
 *        addrs      -- Pointer to address list that we set for caller
 *
 * Result: command arguments parsed
 *       possible printing of help for command line
 *       various flags in pine_state set
 *       returns the string name of the first folder to open
 *       addrs is set
 */
void
pine_args(pine_state, argc, argv, args)
    struct pine *pine_state;
    int          argc;
    char       **argv;
    ARGDATA_S	*args;
{
    register int    ac;
    register char **av;
    int   c;
    char *str;
    char *cmd_list            = NULL;
    char *debug_str           = NULL;
    char *sort                = NULL;
    char *pinerc_file         = NULL;
    char *addrbook_file       = NULL;
    char *ab_sort_descrip     = NULL;
    char *lc		      = NULL;
    int   do_help             = 0;
    int   do_conf             = 0;
    int   usage               = 0;
    int   do_use_fk           = 0;
    int   do_can_suspend      = 0;
    int   do_version          = 0;
    struct variable *vars     = pine_state->vars;

    ac = argc;
    av = argv;
    memset(args, 0, sizeof(ARGDATA_S));
    args->action = aaFolder;

    pine_state->pine_name = (lc = last_cmpnt(argv[0])) ? lc : (lc = argv[0]);
#ifdef	OS2
    /*
     * argv0 is not reliable under OS2, so we have to get
     * the executable path from the environment block
     */
    {
      PTIB ptib;
      PPIB ppib;
      char * p;
      DosGetInfoBlocks(&ptib, &ppib);
      p = ppib->pib_pchcmd - 1;
      while (*--p)
	;
      ++p;
      strcpy(tmp_20k_buf, p);
      if((p = strrchr(tmp_20k_buf, '\\'))!=NULL)
	*++p = '\0';
      pine_state->pine_dir = cpystr(tmp_20k_buf);
    }
#endif
#ifdef	DOS
    sprintf(tmp_20k_buf, "%.*s", pine_state->pine_name - argv[0], argv[0]);
    pine_state->pine_dir = cpystr(tmp_20k_buf);
#endif

      /* while more arguments with leading - */
Loop: while(--ac > 0)
        if(**++av == '-'){
	  /* while more chars in this argument */
	  while(*++*av){
	      /* check for pinerc options */
	      if(pinerc_cmdline_opt(*av)){
		  goto Loop;  /* done with this arg, go to next */
	      }
	      /* then other multi-char options */
	      else if(strcmp(*av, "conf") == 0){
		  do_conf = 1;
		  goto Loop;  /* done with this arg, go to next */
	      }
	      else if(strcmp(*av, "pinerc") == 0){
		  if(--ac)
		    pinerc_file = *++av;
		  else{
		      display_args_err(args_err_missing_pinerc, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
#if	defined(DOS) || defined(OS2)
	      else if(strcmp(*av, "aux") == 0){
		  if(--ac){
		      if((str = *++av) != NULL)
			pine_state->aux_files_dir = cpystr(str);
		  }
		  else{
		      display_args_err(args_err_missing_aux, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "nosplash") == 0)
		goto Loop;   /* already taken care of in WinMain */
#endif
#ifdef PASSFILE
	      else if(strcmp(*av, "passfile") == 0){
		  if(--ac){
		      if((str = *++av) != NULL){
			  if(!is_absolute_path(str)){
			      display_args_err(args_err_non_abs_passfile,
					       NULL, 1);
			      ++usage;
			  }
			  else{
			      if(pine_state->passfile)
				fs_give((void **)&pine_state->passfile);

			      pine_state->passfile = cpystr(str);
			  }
		      }
		  }
		  else{
		      display_args_err(args_err_missing_passfile, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "nowrite_passfile") == 0){
		  ps_global->nowrite_passfile = 1;
		  goto Loop;
	      }
#endif  /* PASSFILE */
	      else if(strcmp(*av, "create_lu") == 0){
		  if(ac > 2){
		      ac -= 2;
		      addrbook_file   = *++av;
		      ab_sort_descrip = *++av;
		  }
		  else{
		      display_args_err(args_err_missing_lu, NULL, 1);
		      exit(-1);
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "convert_sigs") == 0){
		  ps_global->convert_sigs = 1;
		  goto Loop;
	      }
	      else if(strcmp(*av, "supported") == 0){
		  ps_global->dump_supported_options = 1;
		  goto Loop;
	      }
	      else if(strcmp(*av, "copy_pinerc") == 0){
		  if(args->action == aaFolder && !args->data.folder){
		      args->action = aaPrcCopy;
		      if(ac > 2){
			ac -= 2;
			args->data.copy.local  = *++av;
			args->data.copy.remote = *++av;
		      }
		      else{
			  display_args_err(args_err_missing_copyprc, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      sprintf(tmp_20k_buf, args_err_conflict, "-copy_pinerc");
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "copy_abook") == 0){
		  if(args->action == aaFolder && !args->data.folder){
		      args->action = aaAbookCopy;
		      if(ac > 2){
			ac -= 2;
			args->data.copy.local  = *++av;
			args->data.copy.remote = *++av;
		      }
		      else{
			  display_args_err(args_err_missing_copyabook, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      sprintf(tmp_20k_buf, args_err_conflict, "-copy_abook");
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "sort") == 0){
		  if(--ac){
		      sort = *++av;
		      COM_SORT_KEY = cpystr(sort);
		  }
		  else{
		      display_args_err(args_err_missing_sort, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "url") == 0){
		  if(args->action == aaFolder && !args->data.folder){
		      args->action = aaURL;
		      if(--ac){
			  args->url = *++av;
		      }
		      else{
			  display_args_err(args_err_missing_url, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      sprintf(tmp_20k_buf, args_err_conflict, "-url");
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "attach") == 0){
		  if((args->action == aaFolder && !args->data.folder)
		     || args->action == aaMail
		     || args->action == aaURL){
		      if(args->action != aaURL)
			args->action = aaMail;
		      if(--ac){
			  args_add_attach(&args->data.mail.attachlist,
					  *++av, FALSE);
		      }
		      else{
			  sprintf(tmp_20k_buf, args_err_missing_attachment, "-attach");
			  display_args_err(tmp_20k_buf, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      sprintf(tmp_20k_buf, args_err_conflict, "-attach");
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "attachlist") == 0){
		  if((args->action == aaFolder && !args->data.folder)
		     || args->action == aaMail
		     || args->action == aaURL){
		      if(args->action != aaURL)
			args->action = aaMail;
		      if(ac - 1){
			  do{
			      if(can_access(*(av+1), READ_ACCESS) == 0){
				  ac--;
				  args_add_attach(&args->data.mail.attachlist,
						  *++av, FALSE);
			      }
			      else
				break;
			  }
			  while(ac);
		      }
		      else{
			  sprintf(tmp_20k_buf, args_err_missing_attachment, "-attachList");
			  display_args_err(tmp_20k_buf, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      sprintf(tmp_20k_buf, args_err_conflict, "-attachList");
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "attach_and_delete") == 0){
		  if((args->action == aaFolder && !args->data.folder)
		     || args->action == aaMail
		     || args->action == aaURL){
		      if(args->action != aaURL)
			args->action = aaMail;
		      if(--ac){
			  args_add_attach(&args->data.mail.attachlist,
					  *++av, TRUE);
		      }
		      else{
			  sprintf(tmp_20k_buf, args_err_missing_attachment, "-attach_and_delete");
			  display_args_err(tmp_20k_buf, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      sprintf(tmp_20k_buf, args_err_conflict, "-attach_and_delete");
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "bail") == 0){
		  pine_state->exit_if_no_pinerc = 1;
		  goto Loop;
	      }
	      else if(strcmp(*av, "version") == 0){
		  do_version = 1;
		  goto Loop;
	      }
#ifdef	_WINDOWS
	      else if(strcmp(*av, "install") == 0){
		  ps_global->install_flag = 1;
		  goto Loop;
	      }
	      else if(strcmp(*av, "registry") == 0){
		  if(--ac){
		      if(!strucmp(*++av, "set")){
			  pine_state->update_registry = UREG_ALWAYS_SET;
		      }
		      else if(!strucmp(*av, "noset")){
			  pine_state->update_registry = UREG_NEVER_SET;
		      }
		      else if(!strucmp(*av, "clear")){
			  if(!mswin_reg(MSWR_OP_BLAST, MSWR_NULL, NULL, 0))
			    display_args_err(
				       "Pine related Registry values removed.",
				       NULL, 0);
			  else
			    display_args_err(
		      "Not all Pine related Registry values could be removed",
				       NULL, 0);
			  exit(0);
		      }
		      else if(!strucmp(*av, "clearsilent")){
			  mswin_reg(MSWR_OP_BLAST, MSWR_NULL, NULL, 0);
			  exit(0);
		      }
		      else if(!strucmp(*av, "dump")){
			  char **pRegistry = mswin_reg_dump();

			  if(pRegistry){
			      display_args_err(NULL, pRegistry, 0);
			      free_list_array(&pRegistry);
			  }
			  exit(0);
		      }
		      else{
			  display_args_err("unknown registry command",
					   NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      sprintf(tmp_20k_buf, args_err_missing_flag_arg, c);
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
#endif
	      /* single char flags */
	      else{
		  switch(c = **av){
		    case 'h':
		      do_help = 1;
		      break;			/* break back to inner-while */
		    case 'k':
		      do_use_fk = 1;
		      break;
		    case 'z':
		      do_can_suspend = 1;
		      break;
		    case 'r':
		      pine_state->restricted = 1;
		      break;
		    case 'o':
		      pine_state->open_readonly_on_startup = 1;
		      break;
		    case 'i':
		      pine_state->start_in_index = 1;
		      break;
		    case 'v':
		      do_version = 1;
		      break;			/* break back to inner-while */
		      /* these take arguments */
		    case 'f': case 'F': case 'p': case 'I':
		    case 'c': case 'd': case 'P': case 'x':  /* string args */
		    case 'n':				     /* integer args */
#ifdef	OS2
		    case 'w':
#endif
		      if(*++*av)
			str = *av;
		      else if(--ac)
			str = *++av;
		      else if(c == 'f' || c == 'F')
			str = "";
		      else{
			  sprintf(tmp_20k_buf, args_err_missing_flag_arg, c);
			  display_args_err(tmp_20k_buf, NULL, 1);
			  ++usage;
			  goto Loop;
		      }

		      switch(c){
			case 'f':
			  if(args->action == aaFolder && !args->data.folder){
			      args->data.folder = str;
			  }
			  else{
			      sprintf(tmp_20k_buf, args_err_conflict, "-f");
			      display_args_err(tmp_20k_buf, NULL, 1);
			      usage++;
			  }
			  
			  break;
			case 'F':
			  if(args->action == aaFolder && !args->data.folder){
			      args->action    = aaMore;
			      args->data.file = str;
			  }
			  else{
			      sprintf(tmp_20k_buf, args_err_conflict, "-F");
			      display_args_err(tmp_20k_buf, NULL, 1);
			      usage++;
			  }

			  break;
			case 'd':
			  debug_str = str;
			  break;
			case 'I':
			  cmd_list = str;
			  break;
			case 'p':
			  if(str){
			      char path[MAXPATH], dir[MAXPATH];
				
			      if(IS_REMOTE(str) || is_absolute_path(str)){
				  strncpy(path, str, sizeof(path)-1);
				  path[sizeof(path)-1] = '\0';
			      }
			      else{
				  getcwd(dir, sizeof(path));
				  build_path(path, dir, str, sizeof(path));
			      }

			      /*
			       * Pinerc used to be the name of the pinerc
			       * file. Now, since the pinerc can be remote,
			       * we've replaced the variable pinerc with the
			       * structure prc. Unfortunately, other parts of
			       * pine rely on the fact that pinerc is the
			       * name of the pinerc _file_, and use the
			       * directory that the pinerc file is located
			       * in for their own purposes. We keep that so
			       * things will keep working.
			       */

			      if(!IS_REMOTE(path)){
				  if(pine_state->pinerc)
				    fs_give((void **)&pine_state->pinerc);

				  pine_state->pinerc = cpystr(path);
			      }

			      /*
			       * Last one wins. This would be the place where
			       * we put multiple pinercs in a list if we
			       * were to allow that.
			       */
			      if(pine_state->prc)
				free_pinerc_s(&pine_state->prc);

			      pine_state->prc = new_pinerc_s(path);
			  }

			  break;
			case 'P':
			  if(str){
			      char path[MAXPATH], dir[MAXPATH];

			      if(IS_REMOTE(str) || is_absolute_path(str)){
				  strncpy(path, str, sizeof(path)-1);
				  path[sizeof(path)-1] = '\0';
			      }
			      else{
				  getcwd(dir, sizeof(path));
				  build_path(path, dir, str, sizeof(path));
			      }

			      if(pine_state->pconf)
				free_pinerc_s(&pine_state->pconf);

			      pine_state->pconf = new_pinerc_s(path);
			  }

			  break;
			case 'x':
			  if(str)
			    pine_state->exceptions = cpystr(str);

			  break;
			case 'c':
			  if(!isdigit((unsigned char)str[0])){
			      sprintf(tmp_20k_buf,
				      args_err_missing_flag_num, c);
			      display_args_err(tmp_20k_buf, NULL, 1);
			      ++usage;
			      break;
			  }

			  pine_state->init_context = (short) atoi(str);
			  break;

			case 'n':
			  if(!isdigit((unsigned char)str[0])){
			      sprintf(tmp_20k_buf,
				      args_err_missing_flag_num, c);
			      display_args_err(tmp_20k_buf, NULL, 1);
			      ++usage;
			      break;
			  }

			  pine_state->start_entry = atoi(str);
			  if(pine_state->start_entry < 1)
			    pine_state->start_entry = 1;

			  break;
#ifdef	OS2
			case 'w':
		        {
			    USHORT nrows = (USHORT)atoi(str);
			    if(nrows > 10 && nrows < 255) {
				VIOMODEINFO mi;
				mi.cb = sizeof(mi);
				if(VioGetMode(&mi, 0)==0){
				    mi.row = nrows;
				    VioSetMode(&mi, 0);
				}
			    }
		        }
#endif
		      }

		      goto Loop;

		    default:
		      sprintf(tmp_20k_buf, args_err_unknown, c);
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		      break;
		  }
	      }
	  }
      }
      else if(args->action == aaMail
	      || (args->action == aaFolder && !args->data.folder)){
	  STRLIST_S *stp, **slp;

	  args->action = aaMail;

	  stp	    = new_strlist();
	  stp->name = cpystr(*av);

	  for(slp = &args->data.mail.addrlist; *slp; slp = &(*slp)->next)
	    ;

	  *slp = stp;
      }
      else{
	  sprintf(tmp_20k_buf, args_err_conflict, *av);
	  display_args_err(tmp_20k_buf, NULL, 1);
	  usage++;
      }

    if(cmd_list){
	int    commas         = 0;
	char  *p              = cmd_list;
	char  *error          = NULL;

	while(*p++)
	  if(*p == ',')
	    ++commas;

	COM_INIT_CMD_LIST = parse_list(cmd_list, commas+1, 0, &error);
	if(error){
	    sprintf(tmp_20k_buf, args_err_I_error, cmd_list, error);
	    display_args_err(tmp_20k_buf, NULL, 1);
	    exit(-1);
	}
    }

#ifdef DEBUG
    pine_state->debug_nfiles = NUMDEBUGFILES;
#endif
    if(debug_str && process_debug_str(debug_str))
      usage++;

    if(lc && strncmp(lc, PINE_FKEY_NAME, sizeof(PINE_FKEY_NAME) - 1) == 0)
      do_use_fk = 1;

    if(do_use_fk || do_can_suspend){
	char   list[500];
	int    commas         = 0;
	char  *p              = list;
	char  *error          = NULL;

        list[0] = '\0';

        if(do_use_fk){
	    if(list[0])
	      (void)strcat(list, ",");

	    (void)strcat(list, "use-function-keys");
	}

	if(do_can_suspend){
	    if(list[0])
	      (void)strcat(list, ",");

	    (void)strcat(list, "enable-suspend");
	}

	while(*p++)
	  if(*p == ',')
	    ++commas;

	pine_state->feat_list_back_compat = parse_list(list,commas+1,0,&error);
	if(error){
	    sprintf(tmp_20k_buf, args_err_internal, error);
	    display_args_err(tmp_20k_buf, NULL, 1);
	    exit(-1);
	}
    }

    if(((do_conf ? 1 : 0)+(pinerc_file ? 1 : 0)+(addrbook_file ? 1 : 0)) > 1){
	display_args_err("May only have one of -conf, -pinerc, and -create_lu",
			 NULL, 1);
	exit(-1);
    }

    if(do_help || usage)
      args_help(); 

    if(usage)
      exit(-1);

    if(do_version){
	extern char datestamp[], hoststamp[];

	sprintf(tmp_20k_buf, "Pine %s built %s on %s",
		pine_version, datestamp, hoststamp);
	display_args_err(tmp_20k_buf, NULL, 0);
	exit(0);
    }

    if(do_conf)
      dump_global_conf();

    if(pinerc_file)
      dump_new_pinerc(pinerc_file);

    if(addrbook_file)
      just_update_lookup_file(addrbook_file, ab_sort_descrip);

    if(ac <= 0)
      *av = NULL;
}


/*
 * Returns 0 if ok, -1 if error.
 */
int
process_debug_str(debug_str)
    char *debug_str;
{
    int    i, usage            = 0;
    int    commas              = 0;
    int    new_style_debug_arg = 0;
    char  *q                   = debug_str;
    char  *error               = NULL;
    char **list, **p;

#ifdef DEBUG
    if(debug_str){
	if(!isdigit((unsigned char)debug_str[0]))
	  new_style_debug_arg++;

	if(new_style_debug_arg){
	    while(*q++)
	      if(*q == ',')
		++commas;

	    list = parse_list(debug_str, commas+1, 0, &error);
	    if(error){
		sprintf(tmp_20k_buf, args_err_d_error, debug_str, error);
		display_args_err(tmp_20k_buf, NULL, 1);
		return(-1);
	    }

	    if(list){
	      for(p = list; *p; p++){
		if(struncmp(*p, "timestamp", 9) == 0){
		    ps_global->debug_timestamp = 1;
		}
		else if(struncmp(*p, "imap", 4) == 0){
		    q = *p + 4;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			sprintf(tmp_20k_buf, args_err_missing_debug_num, *p);
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else{
			i = atoi(q+1);
			ps_global->debug_imap = min(5,max(0,i));
		    }
		}
		else if(struncmp(*p, "flush", 5) == 0){
		    ps_global->debug_flush = 1;
		}
		else if(struncmp(*p, "tcp", 3) == 0){
		    ps_global->debug_tcp = 1;
		}
		else if(struncmp(*p, "verbose", 7) == 0){
		    q = *p + 7;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			sprintf(tmp_20k_buf, args_err_missing_debug_num, *p);
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else
		      debug = atoi(q+1);
		}
		else if(struncmp(*p, "numfiles", 8) == 0){
		    q = *p + 8;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			sprintf(tmp_20k_buf, args_err_missing_debug_num, *p);
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else{
			i = atoi(q+1);
			ps_global->debug_nfiles = min(31,max(0,i));
		    }
		}
		else if(struncmp(*p, "malloc", 6) == 0){
		    q = *p + 6;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			sprintf(tmp_20k_buf, args_err_missing_debug_num, *p);
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else{
			i = atoi(q+1);
			ps_global->debug_malloc = min(63,max(0,i));
		    }
		}
#if defined(ENABLE_LDAP) && defined(LDAP_DEBUG)
		else if(struncmp(*p, "ldap", 4) == 0){
		    q = *p + 4;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			sprintf(tmp_20k_buf, args_err_missing_debug_num, *p);
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else{
			i = atoi(q+1);
			ldap_debug = i;
		    }
		}
#endif	/* LDAP */
		else{
		    sprintf(tmp_20k_buf, "unknown debug keyword \"%s\"", *p);
		    display_args_err(tmp_20k_buf, NULL, 1);
		    usage = -1;
		}
	      }

	      free_list_array(&list);
	    }
	}
	else{
	    debug = atoi(debug_str);
	    if(debug > 9)
	      ps_global->debug_imap = 5;
	    else if(debug > 7)
	      ps_global->debug_imap = 4;
	    else if(debug > 6)
	      ps_global->debug_imap = 3;
	    else if(debug > 4)
	      ps_global->debug_imap = 2;
	    else if(debug > 2)
	      ps_global->debug_imap = 1;

	    if(debug > 7)
	      ps_global->debug_timestamp = 1;

	    if(debug > 8)
	      ps_global->debug_flush = 1;
	}
    }

    if(!new_style_debug_arg){
#ifdef CSRIMALLOC
	ps_global->debug_malloc =
			(debug <= DEFAULT_DEBUG) ? 1 : (debug < 9) ? 2 : 3;
#else /* !CSRIMALLOC */
#ifdef HAVE_SMALLOC
	if(debug > 8)
	  ps_global->debug_malloc = 2;
#else /* !HAVE_SMALLOC */
#ifdef NXT
	if(debug > 8)
	  ps_global->debug_malloc = 32;
#endif /* NXT */
#endif /* HAVE_SMALLOC */
#endif /* CSRIMALLOC */
    }

#else  /* !DEBUG */

    sprintf(tmp_20k_buf, "unknown flag \"d\", debugging not compiled in");
    display_args_err(tmp_20k_buf, NULL, 1);
    usage = -1;

#endif /* !DEBUG */

    return(usage);
}


void
args_add_attach(alpp, s, deleted)
    PATMT  **alpp;
    char    *s;
    int	     deleted;
{
    PATMT *atmp, **atmpp;

    atmp = (PATMT *) fs_get(sizeof(PATMT));
    memset(atmp, 0, sizeof(PATMT));
    atmp->filename = cpystr(s);

#if	defined(DOS) || defined(OS2)
    (void) removing_quotes(atmp->filename);
#endif

    if(deleted)
      atmp->flags |= A_TMP;

    for(atmpp = alpp; *atmpp; atmpp = &(*atmpp)->next)
      ;

    *atmpp = atmp;
}



/*----------------------------------------------------------------------
    print a few lines of help for command line arguments

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
args_help()
{
    /**  print out possible starting arguments... **/
    display_args_err(NULL, args_pine_args, 0);
    exit(1);
}


/*----------------------------------------------------------------------
   write argument error to the display...

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
display_args_err(s, a, err)
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

#ifdef	_WINDOWS
    if(errp)
      mswin_messagebox(errp, err);

    if(a && *a){
	os_argsdialog(a);
    }
#else
    if(errp)
      fprintf(fp, "%s\n", errp);

    while(a && *a)
      fprintf(fp, "%s\n", *a++);
#endif
}
