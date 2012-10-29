#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailcap.c 13858 2004-11-03 20:11:00Z hubert $";
#endif
/*----------------------------------------------------------------------

           T H E    P I N E    M A I L   S Y S T E M

   Laurence Lundblade, Mike Seibel and David Miller
   Networks and Distributed Computing
   Computing and Communications
   University of Washington
   4545 Builiding, JE-20
   Seattle, Washington, 98195, USA
   Internet: mikes@cac.washington.edu
             dlm@cac.wasington.edu

   Please address all bugs and comments to "pine-bugs@cac.washington.edu"


   Pine and Pico are registered trademarks of the University of Washington.
   No commercial use of these trademarks may be made without prior written
   permission of the University of Washington.

   Pine, Pico, and Pilot software and its included text are Copyright
   1989-2004 by the University of Washington.

   The full text of our legal notices is contained in the file called
   CPYRIGHT, included with this distribution.


   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    *                   Copyright (c) 1986, 1987 Dave Taylor              *
    *                   Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************


   ******************************************************* 
   Mailcap support is based in part on Metamail version 2.7:
      Author:  Nathaniel S. Borenstein, Bellcore

   Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)
 
   Permission to use, copy, modify, and distribute this material
   for any purpose and without fee is hereby granted, provided
   that the above copyright notice and this permission notice
   appear in all copies, and that the name of Bellcore not be
   used in advertising or publicity pertaining to this
   material without the specific, prior written permission
   of an authorized representative of Bellcore.  BELLCORE
   MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY
   OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS",
   WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.
   ******************************************************* 

   ******************************************************* 
   MIME-TYPE support based on code contributed
   by Hans Drexler <drexler@mpi.nl>.  His comment:

   Mime types makes mime assign attachment types according
   to file name extensions found in a system wide file
   ``/usr/local/lib/mime.types'' and a user specific file
   ``~/.mime.types'' . These files specify file extensions
   that will be connected to a mime type.
   ******************************************************* 


  ----------------------------------------------------------------------*/

/*======================================================================
       mailcap.c

       Functions to support "mailcap" (RFC1524) to help us read MIME
       segments of various types, and ".mime.types" ala NCSA Mosaic to
       help us attach various types to outbound MIME segments.

 ====*/

#include "headers.h"

/*
 * We've decided not to implement the RFC1524 standard minimum path, because
 * some of us think it is harder to debug a problem when you may be misled
 * into looking at the wrong mailcap entry.  Likewise for MIME.Types files.
 */
#if defined(DOS) || defined(OS2)
#define MC_PATH_SEPARATOR ';'
#define	MC_USER_FILE	  "MAILCAP"
#define MC_STDPATH        NULL
#define MT_PATH_SEPARATOR ';'
#define	MT_USER_FILE	  "MIMETYPE"
#define MT_STDPATH        NULL
#else /* !DOS */
#define MC_PATH_SEPARATOR ':'
#define	MC_USER_FILE	  NULL
#define MC_STDPATH         \
		".mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap"
#define MT_PATH_SEPARATOR ':'
#define	MT_USER_FILE	  NULL
#define MT_STDPATH         \
		".mime.types:/etc/mime.types:/usr/local/lib/mime.types"
#endif /* !DOS */

#ifdef	_WINDOWS
#define	MC_ADD_TMP	" %s"
#else
#define	MC_ADD_TMP	" < %s"
#endif

#define LINE_BUF_SIZE      2000

typedef struct mcap_entry {
    struct mcap_entry *next;
    int                needsterminal;
    char              *contenttype;
    char              *command;
    char              *testcommand;
    char              *label;           /* unused */
    char              *printcommand;    /* unused */
} MailcapEntry;

struct	mailcap_data {
    MailcapEntry *head, **tail;
    STRINGLIST	 *raw;
} MailcapData;

#define	MC_TOKEN_MAX	64


/*
 * Types used to pass parameters and operator functions to the
 * mime.types searching routines.
 */
#define MT_MAX_FILE_EXTENSION 3

/*
 * Struct passed mime.types search functions
 */
typedef struct {
    union {
	char	 *ext;
	char	 *mime_type;
    } from;
    union {
	struct {
	    int	  type;
	    char *subtype;
	} mime;
	char	 *ext;
    } to;
} MT_MAP_T;

typedef int (* MT_OPERATORPROC) PROTO((MT_MAP_T *, FILE *));


/*
 * Internal prototypes
 */
MailcapEntry *mc_new_entry PROTO((void));
void	  mc_free_entry PROTO((MailcapEntry **));
void      mc_process_file PROTO((char *));
void	  mc_parse_file PROTO((char *));
int	  mc_parse_line PROTO((char **, char **));
int	  mc_comment PROTO((char **));
int	  mc_token PROTO((char **, char **));
void	  mc_build_entry PROTO((char **));
char     *mc_bld_test_cmd PROTO((char *, int, char *, PARAMETER *));
char     *mc_cmd_bldr PROTO((char *, int, char *, PARAMETER *, char *,
			     char **));
int       mc_ctype_match PROTO((int, char *, char *));
int	  mc_sane_command PROTO((char *));
MailcapEntry *mc_get_command PROTO((int, char *, PARAMETER *, int, int *));
void      mc_init PROTO((void));
int       mc_passes_test PROTO((MailcapEntry *, int, char *, PARAMETER *));
int	  mt_get_file_ext PROTO((char *, char **));
int	  mt_srch_mime_type PROTO((MT_OPERATORPROC, MT_MAP_T *));
int	  mt_browse_types_file PROTO((MT_OPERATORPROC, MT_MAP_T *, char *));
int	  mt_translate_type PROTO((char *));
int	  mt_srch_by_ext PROTO((MT_MAP_T *, FILE *));
int	  mt_srch_by_type PROTO((MT_MAP_T *, FILE *));


char *
mc_conf_path(def_path, env_path, user_file, separator, stdpath)
    char *def_path, *env_path, *user_file, *stdpath;
    int   separator;
{
    char *path;

    /* We specify MIMETYPES as a path override */
    if(def_path)
      /* there may need to be an override specific to pine */
      path = cpystr(def_path);
    else if(env_path)
      path = cpystr(env_path);
    else{
#if defined(DOS) || defined(OS2)
	char *s;

        /*
	 * This gets interesting.  Since we don't have any standard location
	 * for config/data files, look in the same directory as the PINERC
	 * and the same dir as PINE.EXE.  This is similar to the UNIX
	 * situation with personal config info coming before 
	 * potentially shared config data...
	 */
	if(s = last_cmpnt(ps_global->pinerc)){
	    strncpy(tmp_20k_buf, ps_global->pinerc, s - ps_global->pinerc);
	    tmp_20k_buf[s - ps_global->pinerc] = '\0';
	}
	else
	  strcpy(tmp_20k_buf, ".\\");

	sprintf(tmp_20k_buf + strlen(tmp_20k_buf), "%s%c%s\\%s",
		user_file, separator, ps_global->pine_dir, user_file);
#else	/* !DOS */
	build_path(tmp_20k_buf, ps_global->home_dir, stdpath, SIZEOF_20KBUF);
#endif	/* !DOS */
	path = cpystr(tmp_20k_buf);
    }

    return(path);
}


/*
 * mc_init - Run down the path gathering all the mailcap entries.
 *           Returns with the Mailcap list built.
 */
void
mc_init()
{
    char *s,
	 *pathcopy,
	 *path,
	  image_viewer[MAILTMPLEN];
  
    if(MailcapData.raw)		/* already have the file? */
      return;
    else
      MailcapData.tail = &MailcapData.head;

    dprint(5, (debugfile, "- mc_init -\n"));

    pathcopy = mc_conf_path(ps_global->VAR_MAILCAP_PATH, getenv("MAILCAPS"),
			    MC_USER_FILE, MC_PATH_SEPARATOR, MC_STDPATH);

    path = pathcopy;			/* overloaded "path" */

    /*
     * Insert an entry for the image-viewer variable from .pinerc, if present.
     */
    if(ps_global->VAR_IMAGE_VIEWER && *ps_global->VAR_IMAGE_VIEWER){
	MailcapEntry *mc = mc_new_entry();

	sprintf(image_viewer, "%.*s %%s", sizeof(image_viewer)-30,
		ps_global->VAR_IMAGE_VIEWER);

	MailcapData.raw		   = mail_newstringlist();
	MailcapData.raw->text.data = (unsigned char *) cpystr(image_viewer);
	mc->command		   = (char *) MailcapData.raw->text.data;
	mc->contenttype		   = "image/*";
	mc->label		   = "Pine Image Viewer";
	dprint(5, (debugfile, "mailcap: using image-viewer=%s\n", 
		   ps_global->VAR_IMAGE_VIEWER
		     ? ps_global->VAR_IMAGE_VIEWER : "?"));
    }

    dprint(7, (debugfile, "mailcap: path: %s\n", path ? path : "?"));
    while(path){
	s = strindex(path, MC_PATH_SEPARATOR);
	if(s)
	  *s++ = '\0';
	mc_process_file(path);
	path = s;
    }
  
    if(pathcopy)
      fs_give((void **)&pathcopy);

#ifdef DEBUG
    if(debug >= 11){
	MailcapEntry *mc;
	int i = 0;

	dprint(11, (debugfile, "Collected mailcap entries\n"));
	for(mc = MailcapData.head; mc; mc = mc->next){

	    dprint(11, (debugfile, "%d: ", i++));
	    if(mc->label)
	      dprint(11, (debugfile, "%s\n", mc->label ? mc->label : "?"));
	    if(mc->contenttype)
	      dprint(11, (debugfile, "   %s",
		     mc->contenttype ? mc->contenttype : "?"));
	    if(mc->command)
	      dprint(11, (debugfile, "   command: %s\n",
		     mc->command ? mc->command : "?"));
	    if(mc->testcommand)
	      dprint(11, (debugfile, "   testcommand: %s",
		     mc->testcommand ? mc->testcommand : "?"));
	    if(mc->printcommand)
	      dprint(11, (debugfile, "   printcommand: %s",
		     mc->printcommand ? mc->printcommand : "?"));
	    dprint(11, (debugfile, "   needsterminal %d\n", mc->needsterminal));
	}
    }
#endif /* DEBUG */
}


/*
 * Add all the entries from this file onto the Mailcap list.
 */
void
mc_process_file(file)
char *file;
{
    char filebuf[MAXPATH+1], *file_data;

    dprint(5, (debugfile, "mailcap: process_file: %s\n", file ? file : "?"));

    (void)strncpy(filebuf, file, MAXPATH);
    filebuf[MAXPATH] = '\0';
    file = fnexpand(filebuf, sizeof(filebuf));
    dprint(7, (debugfile, "mailcap: processing file: %s\n", file ? file : "?"));
    switch(is_writable_dir(file)){
      case 0: case 1: /* is a directory */
	dprint(1, (debugfile, "mailcap: %s is a directory, should be a file\n",
	    file ? file : "?"));
	return;

      case 2: /* ok */
	break;

      case 3: /* doesn't exist */
	dprint(5, (debugfile, "mailcap: %s doesn't exist\n", file ? file : "?"));
	return;

      default:
	panic("Programmer botch in mc_process_file");
	/*NOTREACHED*/
    }

    if(file_data = read_file(file)){
	STRINGLIST *newsl, **sl;

	/* Create a new container */
	newsl		 = mail_newstringlist();
	newsl->text.data = (unsigned char *) file_data;

	/* figure out where in the list it should go */
	for(sl = &MailcapData.raw; *sl; sl = &((*sl)->next))
	  ;

	*sl = newsl;			/* Add it to the list */

	mc_parse_file(file_data);	/* the process mailcap data */
    }
    else
      dprint(5, (debugfile, "mailcap: %s can't be read\n", file ? file : "?"));
}


void
mc_parse_file(file)
    char *file;
{
    char *tokens[MC_TOKEN_MAX];

    while(mc_parse_line(&file, tokens))
      mc_build_entry(tokens);
}


int
mc_parse_line(line, tokens)
    char **line, **tokens;
{
    char **tokenp = tokens;

    while(mc_comment(line))		/* skip comment lines */
      ;

    while(mc_token(tokenp, line))	/* collect ';' delim'd tokens */
      if(++tokenp - tokens >= MC_TOKEN_MAX)
	fatal("Ran out of tokens parsing mailcap file");	/* outch! */

    *++tokenp = NULL;			/* tie off list */
    return(*tokens != NULL);
}


/*
 * Retuns 1 if line is a comment, 0 otherwise
 */
int
mc_comment(line)
    char **line;
{
    if(**line == '\n'){		/* blank line is a comment, too */
	(*line)++;
	return(1);
    }

    if(**line == '#'){
	while(**line)			/* !EOF */
	  if(*++(*line) == '\n'){	/* EOL? */
	      (*line)++;
	      break;
	  }

	return(1);
    }

    return(0);
}


/*
 * Retuns 0 if EOL, 1 otherwise
 */
int
mc_token(token, line)
    char **token;
    char **line;
{
    int   rv = 0;
    char *start, *wsp = NULL;

    *token = NULL;			/* init the slot for this token */

    /* skip leading white space */
    while(**line && isspace((unsigned char) **line))
      (*line)++;

    start = *line;

    /* Then see what's left */
    while(1)
      switch(**line){
	case ';' :			/* End-Of-Token */
	  rv = 1;			/* let caller know more follows */
	case '\n' :			/* EOL */
	  if(wsp)
	    *wsp = '\0';		/* truncate white space? */
	  else
	    *start = '\0';		/* if we have a token, tie it off  */

	  (*line)++;			/* and get ready to parse next one */

	  if(rv == 1){			/* ignore trailing semicolon */
	      while(**line){
		  if(**line == '\n')
		    rv = 0;
		  
		  if(isspace((unsigned char) **line))
		    (*line)++;
		  else
		    break;
	      }
	  }

	case '\0' :			/* EOF */
	  return(rv);

	case '\\' :			/* Quoted char */
	  (*line)++;
#if	defined(DOS) || defined(OS2)
	  /*
	   * RFC 1524 says that backslash is used to quote
	   * the next character, but since backslash is part of pathnames
	   * on DOS we're afraid people will not put double backslashes
	   * in their mailcap files.  Therefore, we violate the RFC by
	   * looking ahead to the next character.  If it looks like it
	   * is just part of a pathname, then we consider a single
	   * backslash to *not* be a quoting character, but a literal
	   * backslash instead.
	   *
	   * SO: 
	   * If next char is any of these, treat the backslash
	   * that preceded it like a regular character.
	   */
	  if(**line && isascii(**line)
	     && (isalnum((unsigned char) **line) || strchr("_+-=~" , **line))){
	      *start++ = '\\';
	      wsp = NULL;
	      break;
	  }
	  else
#endif  /* !DOS */

	   if(**line == '\n'){		/* quoted line break */
	       *start = ' ';
	       (*line)++;		/* just move on */
	       while(isspace((unsigned char) **line))
		 (*line)++;

	       break;
	   }
	   else if(**line == '%')	/* quoted '%' becomes "%%" */
	     *--(*line) = '%';		/* overwrite '\' !! */

	   /* Fall thru and copy/advance pointers*/

	default :
	  if(!*token)
	    *token = start;

	  *start = *(*line)++;
	  wsp    = (isspace((unsigned char) *start) && !wsp) ? start : NULL;
	  start++;
	  break;
      }
}


void
mc_build_entry(tokens)
    char **tokens;
{
    MailcapEntry *mc;

    if(!tokens[0]){
	dprint(5, (debugfile, "mailcap: missing content type!\n"));
	return;
    }
    else if(!tokens[1] || !mc_sane_command(tokens[1])){
	dprint(5, (debugfile, "mailcap: missing/bogus command!\n"));
	return;
    }

    mc		    = mc_new_entry();
    mc->contenttype = *tokens++;
    mc->command     = *tokens++;

    dprint(9, (debugfile, "mailcap: content type: %s\n   command: %s\n",
	       mc->contenttype ? mc->contenttype : "?",
	       mc->command ? mc->command : "?"));

    /* grok options  */
    for( ; *tokens; tokens++){
	char *arg;

	/* legit value? */
	if(!isalnum((unsigned char) **tokens)){
	    dprint(5, (debugfile, "Unknown parameter = \"%s\"", *tokens));
	    continue;
	}

	if(arg = strindex(*tokens, '=')){
	    *arg = ' ';
	    while(arg > *tokens && isspace((unsigned char) arg[-1]))
	      arg--;

	    *arg++ = '\0';		/* tie off parm arg */
	    while(*arg && isspace((unsigned char) *arg))
	      arg++;

	    if(!*arg)
	      arg = NULL;
	}

	if(!strucmp(*tokens, "needsterminal")){
	    mc->needsterminal = 1;
	    dprint(9, (debugfile, "mailcap: set needsterminal\n"));
	}
	else if(!strucmp(*tokens, "copiousoutput")){
	    mc->needsterminal = 2;
	    dprint(9, (debugfile, "mailcap: set copiousoutput\n"));
	}
	else if(arg && !strucmp(*tokens, "test")){
	    mc->testcommand = arg;
	    dprint(9, (debugfile, "mailcap: testcommand=%s\n",
		       mc->testcommand ? mc->testcommand : "?"));
	}
	else if(arg && !strucmp(*tokens, "description")){
	    mc->label = arg;
	    dprint(9, (debugfile, "mailcap: label=%s\n",
		   mc->label ? mc->label : "?"));
	}
	else if(arg && !strucmp(*tokens, "print")){
	    mc->printcommand = arg;
	    dprint(9, (debugfile, "mailcap: printcommand=%s\n",
		       mc->printcommand ? mc->printcommand : "?"));
	}
	else if(arg && !strucmp(*tokens, "compose")){
	    /* not used */
	    dprint(9, (debugfile, "mailcap: not using compose=%s\n",
		   arg ? arg : "?"));
	}
	else if(arg && !strucmp(arg, "composetyped")){
	    /* not used */
	    dprint(9, (debugfile, "mailcap: not using composetyped=%s\n",
		   arg ? arg : "?"));
	}
	else if(arg && !strucmp(arg, "textualnewlines")){
	    /* not used */
	    dprint(9, (debugfile,
		       "mailcap: not using texttualnewlines=%s\n",
		       arg ? arg : "?"));
	}
	else if(arg && !strucmp(arg, "edit")){
	    /* not used */
	    dprint(9, (debugfile, "mailcap: not using edit=%s\n",
		   arg ? arg : "?"));
	}
	else if(arg && !strucmp(arg, "x11-bitmap")){
	    /* not used */
	    dprint(9, (debugfile, "mailcap: not using x11-bitmap=%s\n",
		   arg ? arg : "?"));
	}
	else
	  dprint(9, (debugfile, "mailcap: ignoring unknown flag: %s\n",
		     arg ? arg : "?"));
    }
}


/*
 * Tests for mailcap defined command's sanity
 */
mc_sane_command(command)
    char *command;
{
    /* First, test that a command string actually exists */
    if(command && *command){
#ifdef	LATER
	/*
	 * NOTE: Maybe we'll do this later.  The problem is when the
	 * mailcap's been misconfigured.  We then end up supressing
	 * valuable output when the user actually tries to launch the
	 * spec'd viewer.
	 */

	/* Second, Make sure we can get at it */
	if(can_access_in_path(getenv("PATH"), command,  EXECUTE_ACCESS) >= 0)
#endif
	return(1);
    }

    return(0);			/* failed! */
}


/*
 * Returns the mailcap entry for type/subtype from the successfull
 * mailcap entry, or NULL if none.  Command string still contains % stuff.
 */
MailcapEntry *
mc_get_command(type, subtype, params, check_extension, sp_handlingp)
    int        type;
    char      *subtype;
    PARAMETER *params;
    int        check_extension, *sp_handlingp;
{
    MailcapEntry *mc;
    char	  tmp_subtype[256], tmp_ext[16], *ext = NULL;

    dprint(5, (debugfile, "- mc_get_command(%s/%s) -\n",
	       body_type_names(type),
	       subtype ? subtype : "?"));

    if(type == TYPETEXT
       && (!subtype || !strucmp(subtype, "plain"))
       && F_ON(F_SHOW_TEXTPLAIN_INT, ps_global))
      return(NULL);

    mc_init();

    if(check_extension){
	char	 *namep, *dec_namebuf = NULL, *dec_namep = NULL;
	MT_MAP_T  e2b;

	/*
	 * Special handling for when we're looking at what's likely
	 * binary application data. Look for a file name extension
	 * that we might use to hook a helper app to.
	 *
	 * NOTE: This used to preclude an "app/o-s" mailcap entry
	 *       since this took precedence.  Now that there are
	 *       typically two scans through the check_extension
	 *       mechanism, the mailcap entry now takes precedence.
	 */
	if(namep = rfc2231_get_param(params, "name", NULL, NULL)){
	    if(namep[0] == '=' && namep[1] == '?'){
	    /*
	     * Here we have a client that wrongly sent us rfc2047 encoded
	     * parameter instead of what rfc2231 suggests.  Grudgingly,
	     * we try to fix what we didn't break.
	     */
		dec_namebuf = (char *)fs_get((strlen(namep)+1)*sizeof(char));
		dec_namep =
		  (char *)rfc1522_decode((unsigned char *)dec_namebuf,
					 strlen(namep)+1, namep, NULL);
	    }
	    if(mt_get_file_ext((char *) dec_namep ? dec_namep : namep,
			       &e2b.from.ext)){
		if(strlen(e2b.from.ext) < sizeof(tmp_ext) - 2){
		    strcpy(ext = tmp_ext, e2b.from.ext - 1); /* remember it */
		    if(mt_srch_mime_type(mt_srch_by_ext, &e2b)){
			type = e2b.to.mime.type;		/* mapped type */
			strncpy(subtype = tmp_subtype, e2b.to.mime.subtype,
				sizeof(tmp_subtype)-1);
			tmp_subtype[sizeof(tmp_subtype)-1] = '\0';
			fs_give((void **) &e2b.to.mime.subtype);
			params = NULL;		/* they no longer apply */
		    }
		}
	    }

	    fs_give((void **) &namep);
	    if(dec_namebuf)
	      fs_give((void **) &dec_namebuf);
	}
	else
	  return(NULL);
    }

    for(mc = MailcapData.head; mc; mc = mc->next)
      if(mc_ctype_match(type, subtype, mc->contenttype)
	 && mc_passes_test(mc, type, subtype, params)){
	  dprint(9, (debugfile, 
		     "mc_get_command: type=%s/%s, command=%s\n", 
		     body_type_names(type),
		     subtype ? subtype : "?",
		     mc->command ? mc->command : "?"));
	  return(mc);
      }

    if(mime_os_specific_access()){
	static MailcapEntry  fake_mc;
	static char	     fake_cmd[1024];
	char		     tmp_mime_type[256];

	memset(&fake_mc, 0, sizeof(MailcapEntry));
	fake_mc.command = fake_cmd;

	sprintf(tmp_mime_type, "%.127s/%.127s", body_types[type], subtype);
	if(mime_get_os_mimetype_command(tmp_mime_type, ext, fake_cmd,
					1024, check_extension, sp_handlingp))
	  return(&fake_mc);
    }

    return(NULL);
}


/*
 * Check whether the pattern "pat" matches this type/subtype.
 * Returns 1 if it does, 0 if not.
 */
int
mc_ctype_match(type, subtype, pat)
int   type;
char *subtype;
char *pat;
{
    char *type_name = body_type_names(type);
    int   len = strlen(type_name);

    dprint(5, (debugfile, "mc_ctype_match: %s == %s / %s ?\n",
	       pat ? pat : "?",
	       type_name ? type_name : "?",
	       subtype ? subtype : "?"));

    return(!struncmp(type_name, pat, len)
	   && ((pat[len] == '/'
		&& (!pat[len+1] || pat[len+1] == '*'
		    || !strucmp(subtype, &pat[len+1])))
	       || !pat[len]));
}


/*
 * Run the test command for entry mc to see if this entry currently applies to
 * applies to this type/subtype.
 *
 * Returns 1 if it does pass test (exits with status 0), 0 otherwise.
 */
int
mc_passes_test(mc, type, subtype, params)
MailcapEntry *mc;
int           type;
char         *subtype;
PARAMETER    *params;
{
    char *cmd = NULL;
    int   rv;

    dprint(5, (debugfile, "- mc_passes_test -\n"));

    if(mc->testcommand
       && *mc->testcommand
       && !(cmd = mc_bld_test_cmd(mc->testcommand, type, subtype, params)))
      return(FALSE);	/* couldn't be built */
    
    if(!mc->testcommand || !cmd || !*cmd){
	if(cmd)
	  fs_give((void **)&cmd);

	dprint(7, (debugfile, "no test command, so Pass\n"));
	return 1;
    }

    rv = exec_mailcap_test_cmd(cmd);
    dprint(7, (debugfile, "mc_passes_test: \"%s\" %s (rv=%d)\n",
       cmd ? cmd : "?", rv ? "Failed" : "Passed", rv)) ;

    fs_give((void **)&cmd);

    return(!rv);
}


int
mailcap_can_display(type, subtype, params, check_extension)
int       type;
char     *subtype;
PARAMETER *params;
{
    dprint(5, (debugfile, "- mailcap_can_display -\n"));

    return(mc_get_command(type, subtype, params,
			  check_extension, NULL) != NULL);
}


MCAP_CMD_S *
mailcap_build_command(type, subtype, params, tmp_file, needsterm, chk_extension)
    int	       type;
    char      *subtype, *tmp_file;
    PARAMETER *params;
    int	      *needsterm;
    int        chk_extension;
{
    MailcapEntry *mc;
    char         *command, *err = NULL;
    MCAP_CMD_S   *mc_cmd = NULL;
    int           sp_handling = 0;

    dprint(5, (debugfile, "- mailcap_build_command -\n"));

    mc = mc_get_command(type, subtype, params, chk_extension, &sp_handling);
    if(!mc){
	q_status_message(SM_ORDER, 3, 4, "Error constructing viewer command");
	dprint(1, (debugfile,
		   "mailcap_build_command: no command string for %s/%s\n",
		   body_type_names(type), subtype ? subtype : "?"));
	return((MCAP_CMD_S *)NULL);
    }

    if(needsterm)
      *needsterm = mc->needsterminal;

    if(sp_handling)
      command = cpystr(mc->command);
    else if(!(command = mc_cmd_bldr(mc->command, type, subtype, params, tmp_file, &err)) && err && *err)
      q_status_message(SM_ORDER, 5, 5, err);

    dprint(5, (debugfile, "built command: %s\n", command ? command : "?"));

    if(command){
	mc_cmd = (MCAP_CMD_S *)fs_get(sizeof(MCAP_CMD_S));
	mc_cmd->command = command;
	mc_cmd->special_handling = sp_handling;
    }
    return(mc_cmd);
}


/*
 * mc_bld_test_cmd - build the command to test if the given type flies
 *
 *    mc_cmd_bldr's tmp_file argument is NULL as we're not going to
 *    decode and write each and every MIME segment's data to a temp file
 *    when no test's going to use the data anyway.
 */
char *
mc_bld_test_cmd(controlstring, type, subtype, params)
    char      *controlstring;
    int        type;
    char      *subtype;
    PARAMETER *params;
{
    return(mc_cmd_bldr(controlstring, type, subtype, params, NULL, NULL));
}


/*
 * mc_cmd_bldr - construct a command string to execute
 *
 *    If tmp_file is null, then the contents of the given MIME segment
 *    is not provided.  This is useful for building the "test=" string
 *    as it doesn't operate on the segment's data.
 *
 *    The return value is an alloc'd copy of the command to be executed.
 */
char *
mc_cmd_bldr(controlstring, type, subtype, parameter, tmp_file, err)
    char *controlstring;
    int   type;
    char *subtype;
    PARAMETER *parameter;
    char *tmp_file;
    char **err;
{
    char        *from, *to, *s, *parm;
    int	         prefixed = 0, used_tmp_file = 0;

    dprint(8, (debugfile, "- mc_cmd_bldr -\n"));

    for(from = controlstring, to = tmp_20k_buf; *from; ++from){
	if(prefixed){			/* previous char was % */
	    prefixed = 0;
	    switch(*from){
	      case '%':			/* turned \% into this earlier */
		*to++ = '%';
		break;

	      case 's':			/* insert tmp_file name in cmd */
		if(tmp_file){
		    used_tmp_file = 1;
		    sstrcpy(&to, tmp_file);
		}
		else
		  dprint(1, (debugfile,
			     "mc_cmd_bldr: %%s in cmd but not supplied!\n"));

		break;

	      case 't':			/* insert MIME type/subtype */
		/* quote to prevent funny business */
		*to++ = '\'';
		sstrcpy(&to, body_type_names(type));
		*to++ = '/';
		sstrcpy(&to, subtype);
		*to++ = '\'';
		break;

	      case '{':			/* insert requested MIME param */
		if(F_OFF(F_DO_MAILCAP_PARAM_SUBST, ps_global)){
		    dprint(2,
			   (debugfile, "mc_cmd_bldr: param subs %s\n",
			   from ? from : "?"));
		    if(err){
			if(s = strindex(from, '}'))
			  *++s = '\0';

			sprintf(tmp_20k_buf,
			    "Mailcap: see hidden feature %.200s (%%%.200s)",
			    feature_list_name(F_DO_MAILCAP_PARAM_SUBST), from);
			*err = tmp_20k_buf;
			if(s)
			  *s = '}';
		    }

		    return(NULL);
		}

		s = strindex(from, '}');
		if(!s){
		    q_status_message1(SM_ORDER, 0, 4,
    "Ignoring ill-formed parameter reference in mailcap file: %.200s", from);
		    break;
		}

		*s = '\0';
		++from;    /* from is the part inside the brackets now */

		parm = rfc2231_get_param(parameter, from, NULL, NULL);

		dprint(9, (debugfile,
			   "mc_cmd_bldr: parameter %s = %s\n", 
			   from ? from : "?", parm ? parm : "(not found)"));

		/*
		 * Quote parameter values for /bin/sh.
		 * Put single quotes around the whole thing but every time
		 * there is an actual single quote put it outside of the
		 * single quotes with a backslash in front of it.  So the
		 * parameter value  fred's car
		 * turns into       'fred'\''s car'
		 */
		*to++ = '\'';  /* opening quote */
		if(parm){
		    char *p;

		    /*
		     * Copy value, but quote single quotes for /bin/sh
		     * Backslash quote is ignored inside single quotes so
		     * have to put those outside of the single quotes.
		     * (The parm+1000 nonsense is to protect against
		     * malicious mail trying to overlow our buffer.)
		     */
		    for(p = parm; *p && p < parm+1000; p++){
			if(*p == '\''){
			    *to++ = '\'';  /* closing quote */
			    *to++ = '\\';
			    *to++ = '\'';  /* below will be opening quote */
			}
			*to++ = *p;
		    }

		    fs_give((void **) &parm);
		}

		*to++ = '\'';  /* closing quote for /bin/sh */

		*s = '}'; /* restore */
		from = s;
		break;

		/*
		 * %n and %F are used by metamail to support otherwise
		 * unrecognized multipart Content-Types.  Pine does
		 * not use these since we're only dealing with the individual
		 * parts at this point.
		 */
	      case 'n':
	      case 'F':  
	      default:  
		dprint(9, (debugfile, 
		   "Ignoring %s format code in mailcap file: %%%c\n",
		   (*from == 'n' || *from == 'F') ? "unimplemented"
						  : "unrecognized",
		   *from));
		break;
	    }
	}
	else if(*from == '%')		/* next char is special */
	  prefixed = 1;
	else				/* regular character, just copy */
	  *to++ = *from;
    }

    *to = '\0';

    /*
     * file not specified, redirect to stdin
     */
    if(!used_tmp_file && tmp_file)
      sprintf(to, MC_ADD_TMP, tmp_file);

    return(cpystr(tmp_20k_buf));
}

/*
 *
 */
MailcapEntry *
mc_new_entry()
{
    MailcapEntry *mc = (MailcapEntry *) fs_get(sizeof(MailcapEntry));
    memset(mc, 0, sizeof(MailcapEntry));
    *MailcapData.tail = mc;
    MailcapData.tail  = &mc->next;
    return(mc);
}


/*
 * Free a list of mailcap entries
 */
void
mc_free_entry(mc)
    MailcapEntry **mc;
{
    if(mc && *mc){
	mc_free_entry(&(*mc)->next);
	fs_give((void **) mc);
    }
}


void
mailcap_free()
{
    mail_free_stringlist(&MailcapData.raw);
    mc_free_entry(&MailcapData.head);
}



/*
 * END OF MAILCAP ROUTINES, START OF MIME.TYPES ROUTINES
 */


/*
 * Exported function that does the work of sniffing the mime.types
 * files and filling in the body pointer if found.  Returns 1 (TRUE) if
 * extension found, and body pointer filled in, 0 (FALSE) otherwise.
 */
int
set_mime_type_by_extension(body, filename)
    BODY *body;
    char *filename;
{
    MT_MAP_T  e2b;

    if(mt_get_file_ext(filename, &e2b.from.ext)
       && mt_srch_mime_type(mt_srch_by_ext, &e2b)){
	body->type    = e2b.to.mime.type;
	body->subtype = e2b.to.mime.subtype; /* NOTE: subtype was malloc'd */
	return(1);
    }

    return(0);
}


/*
 * Exported function that maps from mime types to file extensions.
 */
int
set_mime_extension_by_type (ext, mtype)
    char *ext;
    char *mtype;
{
    MT_MAP_T t2e;

    t2e.from.mime_type = mtype;
    t2e.to.ext	       = ext;
    return (mt_srch_mime_type (mt_srch_by_type, &t2e));
}
    



/* 
 * Separate and return a pointer to the first character in the 'filename'
 * character buffer that comes after the rightmost '.' character in the
 * filename. (What I mean is a pointer to the filename - extension).
 */
int
mt_get_file_ext(filename, extension)
    char *filename;
    char **extension;
{
    dprint(5, (debugfile, "mt_get_file_ext : filename=\"%s\", ",
	   filename ? filename : "?"));

    for(*extension = NULL; filename && *filename; filename++)
      if(*filename == '.')
	*extension = filename + 1;

    dprint(5, (debugfile, "extension=\"%s\"\n",
	   (extension && *extension) ? *extension : "?"));

    return((int) *extension);
}


/*
 * Build a list of possible mime.type files.  For each one that exists
 * call the mt_operator function.
 * Loop terminates when mt_operator returns non-zero.  
 */
int
mt_srch_mime_type(mt_operator, mt_map)
    MT_OPERATORPROC	mt_operator;
    MT_MAP_T	       *mt_map;
{
    char *s, *pathcopy, *path;
    int   rv = 0;
  
    dprint(5, (debugfile, "- mt_srch_mime_type -\n"));

    pathcopy = mc_conf_path(ps_global->VAR_MIMETYPE_PATH, getenv("MIMETYPES"),
			    MT_USER_FILE, MT_PATH_SEPARATOR, MT_STDPATH);

    path = pathcopy;			/* overloaded "path" */

    dprint(7, (debugfile, "mime_types: path: %s\n", path ? path : "?"));
    while(path){
	if(s = strindex(path, MT_PATH_SEPARATOR))
	  *s++ = '\0';

	if(rv = mt_browse_types_file(mt_operator, mt_map, path))
	  break;

	path = s;
    }
  
    if(pathcopy)
      fs_give((void **)&pathcopy);

    if(!rv && mime_os_specific_access()){
	if(mt_operator == mt_srch_by_ext){
	    char buf[256];

	    if(mime_get_os_mimetype_from_ext(mt_map->from.ext, buf, 256)){
		if(s = strindex(buf, '/')){
		    *s++ = '\0';
		    mt_map->to.mime.type = mt_translate_type(buf);
		    mt_map->to.mime.subtype = cpystr(s);
		    rv = 1;
		}
	    }
	}
	else if(mt_operator == mt_srch_by_type){
	    if(mime_get_os_ext_from_mimetype(mt_map->from.mime_type,
				  mt_map->to.ext, 32)){
		/* the 32 comes from var ext[] in display_attachment() */
		if(*(s = mt_map->to.ext) == '.')
		  while(*s = *(s+1))
		    s++;

		rv = 1;
	    }
	}
	else
	  panic("Unhandled mime type search");
    }


    return(rv);
}


/*
 * Try to match a file extension against extensions found in the file
 * ``filename'' if that file exists. return 1 if a match
 * was found and 0 in all other cases.
 */
int
mt_browse_types_file(mt_operator, mt_map, filename)
    MT_OPERATORPROC	mt_operator;
    MT_MAP_T	       *mt_map;
    char *filename;
{
    int   rv = 0;
    FILE *file;

    if(file = fopen(filename, "r")){
	rv = (*mt_operator)(mt_map, file);
	fclose(file);
    }
    else
      dprint(1, (debugfile, "mt_browse: FAILED open(%s) : %s.\n",
		 filename ? filename : "?", error_description(errno)));

    return(rv);
}


/*
 * scan each line of the file. Treat each line as a mime type definition.
 * The first word is a type/subtype specification. All following words
 * are file extensions belonging to that type/subtype. Words are separated
 * bij whitespace characters.
 * If a file extension occurs more than once, then the first definition
 * determines the file type and subtype.
 */
int
mt_srch_by_ext(e2b, file)
    MT_MAP_T *e2b;
    FILE     *file;
{
    char buffer[LINE_BUF_SIZE];
#if	defined(DOS) || defined(OS2)
#define	STRCMP	strucmp
#else
#define	STRCMP	strcmp
#endif

    /* construct a loop reading the file line by line. Then check each
     * line for a matching definition.
     */
    while(fgets(buffer,LINE_BUF_SIZE,file) != NULL){
	char *typespec;
	char *try_extension;

	if(buffer[0] == '#')
	  continue;		/* comment */

	/* remove last character from the buffer */
	buffer[strlen(buffer)-1] = '\0';
	/* divide the input buffer into words separated by whitespace.
	 * The first words is the type and subtype. All following words
	 * are file extensions.
	 */
	dprint(5, (debugfile, "traverse: buffer=\"%s\"\n", buffer));
	typespec = strtok(buffer," \t");	/* extract type,subtype  */
	if(!typespec)
	  continue;

	dprint(5, (debugfile, "typespec=\"%s\"\n", typespec ? typespec : "?"));
	while((try_extension = strtok(NULL, " \t")) != NULL){
	    /* compare the extensions, and assign the type if a match
	     * is found.
	     */
	    dprint(5,(debugfile,"traverse: trying ext \"%s\"\n",try_extension));
	    if(STRCMP(try_extension, e2b->from.ext) == 0){
		/* split the 'type/subtype' specification */
		char *type, *subtype = NULL;

		type = strtok(typespec,"/");
		if(type)
		  subtype = strtok(NULL,"/");

		dprint(5, (debugfile, "traverse: type=%s, subtype=%s.\n",
			   type ? type : "<null>",
			   subtype ? subtype : "<null>"));
		/* The type is encoded as a small integer. we have to
		 * translate the character string naming the type into
		 * the corresponding number.
		 */
		e2b->to.mime.type    = mt_translate_type(type);
		e2b->to.mime.subtype = cpystr(subtype ? subtype : "x-unknown");
		return 1; /* a match has been found */
	    }
	}
    }

    dprint(5, (debugfile, "traverse: search failed.\n"));
    return 0;
}




/*
 * scan each line of the file. Treat each line as a mime type definition.
 * Here we are looking for a matching type.  When that is found return the
 * first extension that is three chars or less.
 */
int
mt_srch_by_type(t2e, file)
    MT_MAP_T *t2e;
    FILE     *file;
{
    char buffer[LINE_BUF_SIZE];

    /* construct a loop reading the file line by line. Then check each
     * line for a matching definition.
     */
    while(fgets(buffer,LINE_BUF_SIZE,file) != NULL){
	char *typespec;
	char *try_extension;

	if(buffer[0] == '#')
	  continue;		/* comment */

	/* remove last character from the buffer */
	buffer[strlen(buffer)-1] = '\0';
	/* divide the input buffer into words separated by whitespace.
	 * The first words is the type and subtype. All following words
	 * are file extensions.
	 */
	dprint(5, (debugfile, "traverse: buffer=%s.\n", buffer));
	typespec = strtok(buffer," \t");	/* extract type,subtype  */
	dprint(5, (debugfile, "typespec=%s.\n", typespec ? typespec : "?"));
	if (strucmp (typespec, t2e->from.mime_type) == 0) {
	    while((try_extension = strtok(NULL, " \t")) != NULL) {
		if (strlen (try_extension) <= MT_MAX_FILE_EXTENSION) {
		    strcpy (t2e->to.ext, try_extension);
		    return (1);
	        }
	    }
        }
    }

    dprint(5, (debugfile, "traverse: search failed.\n"));
    return 0;
}


/*
 * Translate a character string representing a content type into a short 
 * integer number, according to the coding described in c-client/mail.h
 * List of content types taken from rfc1521, September 1993.
 */
int
mt_translate_type(type)
    char *type;
{
    int i;

    for (i=0;(i<=TYPEMAX) && body_types[i] && strucmp(type,body_types[i]);i++)
      ;

    if (i > TYPEMAX)
      i = TYPEOTHER;
    else if (!body_types[i])	/* if empty slot, assign it to this type */
      body_types[i] = cpystr (type);

    return(i);
}
