#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: rpdump.c 14042 2005-04-19 21:37:30Z hubert $";
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
   1989-2005 by the University of Washington.

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

#include "headers.h"

typedef enum {Pinerc, Abook, Sig, NotSet} RemoteType;

int        parse_args PROTO((int, char **, int *, char **, char **));
RemoteType check_for_header_msg PROTO((MAILSTREAM *));
char      *ptype PROTO((RemoteType));
char      *spechdr PROTO((RemoteType));
char      *err_desc PROTO((int));
int        opt_enter PROTO((char *, int, char *, int *));
char      *last_cmpnt PROTO ((char *));
int        wantto PROTO((char *, int, int));

char *ustr = "usage: %s [-f] -l Local_file  -r Remote_folder\n";
int   noshow_error = 0;

/* look for my_timer_period in pico directory for an explanation */
int my_timer_period = ((IDLE_TIMEOUT + 1)*1000);

#ifdef _WINDOWS

#undef main

app_main (argc, argv)
    int argc;
    char argv[];
{
}

#endif /* _WINDOWS */

/*
 *  rpdump [-f] -l Local_file -r Remote_folder
 *
 *             -f          (skip check for special header)
 *
 *   Note: We're not worrying about memory leaks.
 */
main(argc, argv)
    int   argc;
    char *argv[];
{
    MAILSTREAM *stream = NULL;
    FILE       *fp;
    int         usage = 0;
    char        buf[10000];
    char       *local = NULL, *remote = NULL;
    int         force = 0;
    BODY       *body = NULL;
    char       *data, *p;
    RemoteType  rtype;
    unsigned long i;
    struct stat sbuf;

#include "../c-client/linkage.c"

    if(parse_args(argc, argv, &force, &local, &remote)){
	fprintf(stderr, ustr, argv[0]);
	exit(-1);
    }

    if(!local || !*local){
	fprintf(stderr, "No local file specified\n");
	usage++;
    }

    if(!remote || !*remote){
	fprintf(stderr, "No remote folder specified\n");
	usage++;
    }

    if(usage){
	fprintf(stderr, ustr, argv[0]);
	exit(-1);
    }

    if(!IS_REMOTE(remote)){
	fprintf(stderr,
		"Remote folder name \"%s\" %s\n", remote,
		(*remote != '{') ? "must begin with \"{\"" : "not valid");
	exit(-1);
    }

    if(IS_REMOTE(local)){
	fprintf(stderr, "Argument to -l (%s) must be a local filename", local);
	exit(-1);
    }

#ifdef	_WINDOWS
    if(stat(local, &sbuf))
#else
    if(lstat(local, &sbuf))
#endif
    {
	if(errno == ENOENT){		/* File did not exist */
	    int fd;

	    /* create it */
	    if(((fd = open(local, O_CREAT|O_EXCL|O_WRONLY,0600)) < 0)
	       || (close(fd) != 0)){
		fprintf(stderr, "%s: %s\n", local, err_desc(errno));
		exit(-1);
	    }

	    /* now it exists! */
	}
	else{				/* unknown error */
	    fprintf(stderr, "%s: %s\n", local, err_desc(errno));
	    exit(-1);
	}
    }
    else{				/* file exists */

	/* is it a regular file? */
#ifdef	S_ISREG
	if(!S_ISREG(sbuf.st_rdev))
#else
	if(!(S_IFREG & sbuf.st_mode))
#endif
	{
	    fprintf(stderr, "Only allowed to write to regular local files. Try another filename.\n");
	    exit(-1);
	}

	if(access(local, WRITE_ACCESS) == 0){

	    sprintf(buf, "Local file \"%.20s\" exists, overwrite it",
		    (p = last_cmpnt(local)) ? p : local);
	    if(wantto(buf, 'n', 'n') != 'y'){
		fprintf(stderr, "Dump cancelled\n");
		exit(-1);
	    }
	}
	else{
	    fprintf(stderr, "Local file \"%s\" is not writable\n", local);
	    exit(-1);
	}
    }

    /*
     * Try opening the local file.
     */
    if((fp = fopen(local, "w")) == NULL){
	fprintf(stderr, "Can't open \"%s\": %s\n", local, err_desc(errno));
	mail_close(stream);
	exit(-1);
    }

    /*
     * Try opening the remote folder.
     */
    noshow_error = 1;
    stream = mail_open(NULL, remote, OP_READONLY);
    if(!stream || stream->halfopen){
	fprintf(stderr, "Remote folder \"%s\" is not readable\n", remote);
	if(stream)
	  mail_close(stream);

	exit(-1);
    }

    noshow_error = 0;

    if(stream->nmsgs >= 2){
	/*
	 * There is a first message already. Check to see if it is one of
	 * our special header messages.
	 */
	rtype = check_for_header_msg(stream);
	if(!force && rtype == NotSet){
	    fprintf(stderr, "Folder \"%s\"\ndoes not appear to be a Pine remote data folder.\nUse -f to force.\n", remote);
	    mail_close(stream);
	    exit(-1);
	}
    }
    else if(stream->nmsgs == 1){
	fprintf(stderr, "No data in remote folder to copy (only 1 message)\n");
	mail_close(stream);
	exit(-1);
    }
    else{
	fprintf(stderr, "No data in remote folder to copy\n");
	mail_close(stream);
	exit(-1);
    }

    if(!mail_fetchstructure(stream, stream->nmsgs, &body)){
	fprintf(stderr, "Can't access remote IMAP data\n");
	mail_close(stream);
	exit(-1);
    }

    if(!body ||
       body->type != REMOTE_DATA_TYPE ||
       !body->subtype ||
       (!force && strucmp(body->subtype, spechdr(rtype)))){
	fprintf(stderr, "Remote IMAP folder has wrong contents\n");
	mail_close(stream);
	exit(-1);
    }
    
    if(!mail_fetchenvelope(stream, stream->nmsgs)){
	fprintf(stderr, "Can't access envelope in remote data\n");
	mail_close(stream);
	exit(-1);
    }

    data = mail_fetch_body(stream, stream->nmsgs, "1", &i, FT_PEEK);

    p = data;
    for(p = data; p < data+i; p++){

	/* convert to unix newlines */
	if(*p == '\r' && *(p+1) == '\n')
	  p++;

	if(putc(*p, fp) == EOF){
	    fprintf(stderr,
		    "Error writing \"%s\": %s\n", local, err_desc(errno));
	    fclose(fp);
	    mail_close(stream);
	    exit(-1);
	}
    }

    mail_close(stream);
    fclose(fp);

    fprintf(stderr,
	    "Remote folder is of type \"%s\", contents saved to \"%s\"\n",
	    ptype(rtype) ? ptype(rtype) : "unknown", local);
    exit(0);
}


RemoteType
check_for_header_msg(stream)
    MAILSTREAM *stream;
{
    STRINGLIST *sl;
    int         ret = NotSet;
    char       *h, *try;
    size_t      len;
    char       *pinerc, *abook, *sig;

    pinerc = spechdr(Pinerc);
    abook  = spechdr(Abook);
    sig    = spechdr(Sig);
    
    len = max(max(strlen(pinerc), strlen(abook)), strlen(sig));

    sl = mail_newstringlist();
    sl->text.data = (unsigned char *)fs_get((len+1) * sizeof(unsigned char));
    try = pinerc;
    strcpy((char *)sl->text.data, try);
    sl->text.size = strlen(try);

    if(stream && (h=mail_fetch_header(stream,1L,NULL,sl,NULL,FT_PEEK))){

	if(strlen(h) >= sl->text.size && !struncmp(h, try, sl->text.size))
	  ret = Pinerc;
    }

    if(ret == NotSet){
	try = abook;
	strcpy((char *)sl->text.data, try);
	sl->text.size = strlen(try);
	if(stream && (h=mail_fetch_header(stream,1L,NULL,sl,NULL,FT_PEEK))){

	    if(strlen(h) >= sl->text.size && !struncmp(h, try, sl->text.size))
	      ret = Abook;
	}
    }

    if(ret == NotSet){
	try = sig;
	strcpy((char *)sl->text.data, try);
	sl->text.size = strlen(try);
	if(stream && (h=mail_fetch_header(stream,1L,NULL,sl,NULL,FT_PEEK))){

	    if(strlen(h) >= sl->text.size && !struncmp(h, try, sl->text.size))
	      ret = Sig;
	}
    }

    if(sl)
      mail_free_stringlist(&sl);

    if(pinerc)
      fs_give((void **)&pinerc);
    if(abook)
      fs_give((void **)&abook);
    if(sig)
      fs_give((void **)&sig);

    return(ret);
}


char *
ptype(rtype)
    RemoteType rtype;
{
    char *ret = NULL;

    switch(rtype){
      case Pinerc:
        ret = cpystr("pinerc");
	break;
      case Abook:
        ret = cpystr("abook");
	break;
      case Sig:
        ret = cpystr("sig");
	break;
    }

    return(ret);
}


char *
spechdr(rtype)
    RemoteType rtype;
{
    char *ret = NULL;

    switch(rtype){
      case Pinerc:
        ret = cpystr(REMOTE_PINERC_SUBTYPE);
	break;
      case Abook:
        ret = cpystr(REMOTE_ABOOK_SUBTYPE);
	break;
      case Sig:
        ret = cpystr(REMOTE_SIG_SUBTYPE);
	break;
    }

    return(ret);
}


int
parse_args(argc, argv, force, local, remote)
    int          argc;
    char       **argv;
    int         *force;
    char       **local, **remote;
{
    int    ac;
    char **av;
    int    c;
    char  *str;
    int    usage = 0;

    ac = argc;
    av = argv;

      /* while more arguments with leading - */
Loop: while(--ac > 0 && **++av == '-'){
	/* while more chars in this argument */
	while(*++*av){
	    switch(c = **av){
	      case 'h':
		usage++;
		break;
	      case 'f':
		(*force)++;
		break;

	      case 'r': case 'l':	/* string args */
		if(*++*av)
		  str = *av;
		else if(--ac)
		  str = *++av;
		else{
		    fprintf(stderr, "missing argument for flag \"%c\"\n", c);
		    ++usage;
		    goto Loop;
		}

		switch(c){
		  case 'l':
		    if(str)
		      *local = str;

		    break;
		  case 'r':
		    if(str)
		      *remote = str;

		    break;
		}

		goto Loop;

		default:
		  fprintf(stderr, "unknown flag \"%c\"\n", c);
		  ++usage;
		  break;
	    }
	 }
      }

    if(ac != 0)
      usage++;

    return(usage);
}


char *
err_desc(err)
    int err;
{
    return((char *) strerror(err));
}


void mm_exists(stream, number)
    MAILSTREAM *stream;
    unsigned long number;
{
}


void mm_expunged(stream, number)
    MAILSTREAM *stream;
    unsigned long number;
{
}


void mm_flags(stream, number)
    MAILSTREAM *stream;
    unsigned long number;
{
}


void mm_list(stream, delim, name, attrib)
    MAILSTREAM *stream;
    int   delim;
    char *name;
    long attrib;
{
}


void mm_lsub(stream, delimiter, name, attributes)
    MAILSTREAM *stream;
    int delimiter;
    char *name;
    long attributes;
{
}


void mm_notify(stream, string, errflg)
    MAILSTREAM *stream;
    char *string;
    long errflg;
{
    mm_log(string, errflg);
}


void mm_log(string, errflg)
    char *string;
    long errflg;
{
    if(noshow_error)
      return;

    switch(errflg){  
      case BYE:
      case NIL:
	break;

      case PARSE:
	fprintf(stderr, "PARSE: %s\n", string);
	break;

      case WARN:
	fprintf(stderr, "WARN: %s\n", string);
	break;

      case ERROR:
	fprintf(stderr, "ERROR: %s\n", string);
	break;

      default:
	fprintf(stderr, "%s\n", string);
	break;
    }
}


void mm_login(mb, user, pwd, trial)
    NETMBX *mb;
    char   *user;
    char   *pwd;
    long    trial;
{
    char prompt[100], *last;
    int  i, j, goal, ugoal, len, rc, flags = 0;
#define NETMAXPASSWD 100

    user[NETMAXUSER-1] = '\0';

    if(trial == 0L){
	if(mb->user && *mb->user){
	    strncpy(user, mb->user, NETMAXUSER);
	    user[NETMAXUSER-1] = '\0';
	}
    }

    if(!*mb->user){
	/* Dress up long hostnames */
	sprintf(prompt, "%sHOST: ",
		(mb->sslflag||mb->tlsflag) ? "+ " : "");
	len = strlen(prompt);
	/* leave space for "HOST", "ENTER NAME", and 15 chars for input name */
	goal = 80 - (len + 20 + min(15, 80/5));
	last = "  ENTER LOGIN NAME: ";
	if(goal < 9){
	    last = " LOGIN: ";
	    if((goal += 13) < 9){
		last += 1;
		goal = 0;
	    }
	}

	if(goal){
	    for(i = len, j = 0;
		i < sizeof(prompt) && (prompt[i] = mb->host[j]); i++, j++)
	      if(i == goal && mb->host[goal+1] && i < sizeof(prompt)){
		  strcpy(&prompt[i-3], "...");
		  break;
	      }
	}
	else
	  i = 0;

	strncpy(&prompt[i], last, sizeof(prompt)-strlen(prompt));
	prompt[sizeof(prompt)-1] = '\0';

	while(1) {
	    rc = opt_enter(user, NETMAXUSER, prompt, &flags);
	    if(rc != 4)
	      break;
	}

	if(rc == 1 || !user[0]) {
	    user[0]   = '\0';
	    pwd[0] = '\0';
	}
    }
    else
      strncpy(user, mb->user, NETMAXUSER);

    user[NETMAXUSER-1] = '\0';
    pwd[NETMAXPASSWD-1] = '\0';

    if(!user[0])
      return;


    /* Dress up long host/user names */
    /* leave space for "HOST", "USER" "ENTER PWD", 12 for user 6 for pwd */
    sprintf(prompt, "%sHOST: ", (mb->sslflag||mb->tlsflag) ? "+ " : "");
    len = strlen(prompt);
    goal  = strlen(mb->host);
    ugoal = strlen(user);
    if((i = 80 - (len + 8 + 18 + 6)) < 14){
	goal = 0;		/* no host! */
	if((i = 80 - (6 + 18 + 6)) <= 6){
	    ugoal = 0;		/* no user! */
	    if((i = 80 - (18 + 6)) <= 0)
	      i = 0;
	}
	else{
	    ugoal = i;		/* whatever's left */
	    i     = 0;
	}
    }
    else
      while(goal + ugoal > i)
	if(goal > ugoal)
	  goal--;
	else
	  ugoal--;

    if(goal){
	sprintf(prompt, "%sHOST: ",
		(mb->sslflag||mb->tlsflag) ? "+ " : "");
	for(i = len, j = 0;
	    i < sizeof(prompt) && (prompt[i] = mb->host[j]); i++, j++)
	  if(i == goal && mb->host[goal+1] && i < sizeof(prompt)){
	      strcpy(&prompt[i-3], "...");
	      break;
	  }
    }
    else
      i = 0;

    if(ugoal){
	strncpy(&prompt[i], &"  USER: "[i ? 0 : 2], sizeof(prompt)-i);
	for(i += strlen(&prompt[i]), j = 0;
	    i < sizeof(prompt) && (prompt[i] = user[j]); i++, j++)
	  if(j == ugoal && user[ugoal+1] && i < sizeof(prompt)){
	      strcpy(&prompt[i-3], "...");
	      break;
	  }
    }

    strncpy(&prompt[i], &"  ENTER PASSWORD: "[i ? 0 : 8], sizeof(prompt)-i);

    *pwd = '\0';
    while(1) {
	flags = OE_PASSWD;
        rc = opt_enter(pwd, NETMAXPASSWD, prompt, &flags);
	if(rc != 4)
          break;
    }

    if(rc == 1 || !pwd[0]) {
        user[0] = pwd[0] = '\0';
        return;
    }
}


void mm_critical(stream)
    MAILSTREAM *stream;
{
}


void mm_nocritical(stream)
    MAILSTREAM *stream;
{
}


long mm_diskerror(stream, errcode, serious)
    MAILSTREAM *stream;
    long errcode;
    long serious;
{
    return T;
}


void mm_fatal(string)
    char *string;
{
    fprintf(stderr, "%s\n", string);
}


void mm_searched(stream, msgno)
    MAILSTREAM *stream;
    unsigned long msgno;
{
}


void mm_status(stream, mailbox, status)
    MAILSTREAM *stream;
    char *mailbox;
    MAILSTATUS *status;
{
}

void mm_dlog(string)
    char *string;
{
    fprintf(stderr, "%s\n", string);
}


int
opt_enter(string, field_len, prompt, flags)
     char       *string, *prompt;
     int         field_len;
     int	*flags;
{
    char *pw;
    int   return_v = -10;

    while(return_v == -10){

	if(flags && *flags & OE_PASSWD){
	    if((pw = getpass(prompt)) != NULL){
		if(strlen(pw) < field_len){
		    strcpy(string, pw);
		    return_v = 0;
		}
		else{
		    fputs("Password too long\n", stderr);
		    return_v = -1;
		}
	    }
	    else
	      return_v = 1;	/* cancel? */
	}
	else{
	    char *p;

	    fputs(prompt, stdout);
	    fgets(string, field_len, stdin);
	    string[field_len-1] = '\0';
	    if((p = strpbrk(string, "\r\n")) != NULL)
	      *p = '\0';

	    return_v = 0;
	}
    }

    return(return_v);
}

char *
last_cmpnt(filename)
    char *filename;
{
    register char *p = NULL, *q = filename;

    if(!q)
      return(q);

    while(q = strchr(q, '/'))
      if(*++q)
	p = q;

    return(p);
}

int
wantto(question, dflt, on_ctrl_C)
     char    *question;
     int      dflt, on_ctrl_C;
{
    int ret = 0;
    char rep[1000], *p;

    while(!ret){
      fprintf(stdout, "%s? [%c]:", question, dflt);
      fgets(rep, 1000, stdin);
      if((p = strpbrk(rep, "\r\n")) != NULL)
	*p = '\0';
      switch(*rep){
        case 'Y':
        case 'y':
	  ret = (int)'y';
	  break;
        case 'N':
        case 'n':
	  ret = (int)'n';
	  break;
        case '\0':
	  ret = dflt;
	  break;
        default:
	  break;
      }
    }

    return ret;
}
