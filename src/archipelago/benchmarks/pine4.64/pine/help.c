#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: help.c 13884 2004-11-23 18:29:47Z hubert $";
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
   1989-2004 by the University of Washington.

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
     help.c
     Functions to support the pine help screens
 ====*/


#include "headers.h"


static struct key help_keys[] =
       {MAIN_MENU,
	{NULL,NULL,{MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{NULL,NULL,{MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{NULL,NULL,{MC_VIEW_HANDLE,3,{'v',ctrl('m'),ctrl('j')}},KS_NONE},
	{"^B","PrevLink",{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F","NextLink",{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTMSG_MENU,
	{"Z","Print All",{MC_PRINTALL,1,{'z'}},KS_NONE},
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(help_keymenu, help_keys);
#define	HLP_MAIN_KEY	0
#define	HLP_SUBEXIT_KEY	1
#define	HLP_EXIT_KEY	2
#define	HLP_VIEW_HANDLE	3
#define	HLP_PREV_HANDLE	4
#define	HLP_NEXT_HANDLE	5
#define	HLP_ALL_KEY	9


typedef struct _help_scroll {
    unsigned	keys_formatted:1;	/* Has full keymenu been formatted? */
    char      **help_source;		/* Source of displayed help text */
} HELP_SCROLL_S;


static struct {
    unsigned   crlf:1;
    char     **line,
	      *offset;
} g_h_text;


typedef struct _help_print_state {
    int	  page;
    char *title;
    int   title_len;
} HPRT_S;

static HPRT_S *g_hprt;


static char att_cur_msg[] = "\
         Reporting a bug...\n\
\n\
  If you think that the \"current\" message may be related to the bug you\n\
  are reporting you may include it as an attachment.  If you want to\n\
  include a message but you aren't sure if it is the current message,\n\
  cancel this bug report, go to the folder index, place the cursor on\n\
  the message you wish to include, then return to the main menu and run\n\
  the bug report command again.  Answer \"Y\" when asked the question\n\
  \"Attach current message to report?\"\n\
\n\
  This bug report will also automatically include your pine\n\
  configuration file, which is helpful when investigating the problem.";


/* Helpful def's */
#define	GRIPE_OPT_CONF	0x01
#define	GRIPE_OPT_MSG	0x02
#define	GRIPE_OPT_LOCAL	0x04
#define	GRIPE_OPT_KEYS	0x08



/*
 * Internal prototypes
 */
int	 helper_internal PROTO((HelpType, char **, char *, char *, int));
HelpType help_name2section PROTO((char *, int));
int	 help_processor PROTO((int, MSGNO_S *, SCROLL_S *));
int	 journal_processor PROTO((int, MSGNO_S *, SCROLL_S *));
void     prune_review_messages PROTO((size_t *, size_t));
size_t   trim_review PROTO((unsigned long, int, size_t));
void	 help_keymenu_tweek PROTO((SCROLL_S *, int));
int	 help_bogus_input PROTO((int));
char   **get_supported_options PROTO((void));
void     print_all_help PROTO((void));
void	 print_help_page_title PROTO((char *, HPRT_S *));
int	 print_help_page_break PROTO((long, char *, LT_INS_S **, void *));
int	 gripe_bogus_input PROTO((int));
ADDRESS *gripe_token_addr PROTO((char *));
int	 gripe_newbody PROTO((struct pine *, BODY **, long, int));
char	*gripe_id PROTO((char *));
void	 att_cur_drawer PROTO((void));
#ifdef	_WINDOWS
int	 help_popup PROTO((SCROLL_S *, int));
int	 help_subsection_popup PROTO((SCROLL_S *, int));
#endif



/*----------------------------------------------------------------------
     Get the help text in the proper format and call scroller

    Args: text   -- The help text to display (from pine.help --> helptext.c)
          title  -- The title of the help text 

	  The dumb otext argument is for when we want to pass in a char **
	  data item no matter what the type of HelpType is.
  
  Result: format text and call scroller

  The pages array contains the line number of the start of the pages in
the text. Page 1 is in the 0th element of the array.
The list is ended with a page line number of -1. Line number 0 is also
the first line in the text.
  -----*/
int
helper_internal(text, otext, frag, title, flags)
    HelpType  text;
    char    **otext;
    char     *frag;
    char     *title;
    int	      flags;
{
    char	  **shown_text;
    int		    cmd = MC_NONE;
    long	    offset = 0L;
    char	   *error = NULL, tmp_title[MAX_SCREEN_COLS + 1];
    STORE_S	   *store;
    HANDLE_S	   *handles = NULL, *htmp;
    HELP_SCROLL_S   hscroll;
    gf_io_t	    pc;
#ifdef	HELPFILE
    char	  **dynamic_text = NULL;
#endif /* HELPFILE */

    dprint(1, (debugfile, "\n\n    ---- HELPER ----\n"));

#ifdef	HELPFILE
    if(otext)
      shown_text = otext;
    else if((shown_text = dynamic_text = get_help_text(text)) == NULL)
      return(cmd);
#else
    if(otext)
      shown_text = otext;
    else
      shown_text = text;
#endif /* HELPFILE */

    if(F_ON(F_BLANK_KEYMENU,ps_global)){
	FOOTER_ROWS(ps_global) = 3;
	clearfooter(ps_global);
	ps_global->mangled_screen = 1;
    }

    if(flags & HLPD_NEWWIN){
 	fix_windsize(ps_global);
	init_sigwinch();
    }

    /*
     * At this point, shown_text is a charstarstar with html
     * Turn it into a charstar with digested html
     */
    do{
	init_helper_getc(shown_text);
	init_handles(&handles);

	memset(&hscroll, 0, sizeof(HELP_SCROLL_S));
	hscroll.help_source = shown_text;
	if(store = so_get(CharStar, NULL, EDIT_ACCESS)){
	    gf_set_so_writec(&pc, store);
	    gf_filter_init();

	    if(!struncmp(shown_text[0], "<html>", 6))
	      gf_link_filter(gf_html2plain,
			     gf_html2plain_opt("x-pine-help:",
					       ps_global->ttyo->screen_cols, NULL,
					       &handles, GFHP_LOCAL_HANDLES));
	    else
	      gf_link_filter(gf_wrap, gf_wrap_filter_opt(
						  ps_global->ttyo->screen_cols,
						  ps_global->ttyo->screen_cols,
						  NULL, 0, GFW_HANDLES));

	    error = gf_pipe(helper_getc, pc);

	    gf_clear_so_writec(store);

	    if(!error){
		SCROLL_S	sargs;
		struct key_menu km;
		struct key	keys[24];

		for(htmp = handles; htmp; htmp = htmp->next)
		  if(htmp->type == URL
		     && htmp->h.url.path
		     && (htmp->h.url.path[0] == 'x'
			 || htmp->h.url.path[0] == '#'))
		    htmp->force_display = 1;

		/* This is mostly here to get the curses variables
		 * for line and column in sync with where the
		 * cursor is on the screen. This gets warped when
		 * the composer is called because it does it's own
		 * stuff
		 */
		ClearScreen();

		memset(&sargs, 0, sizeof(SCROLL_S));
		sargs.text.text	   = so_text(store);
		sargs.text.src	   = CharStar;
		sargs.text.desc	   = "help text";
		if(sargs.text.handles = handles)
		  while(sargs.text.handles->type == URL
			&& !sargs.text.handles->h.url.path
			&& sargs.text.handles->next)
		    sargs.text.handles = sargs.text.handles->next;

		if(!(sargs.bar.title = title)){
		    if(!struncmp(shown_text[0], "<html>", 6)){
			char *p;
			int   i;

			/* if we're looking at html, look for a <title>
			 * in the <head>... */
			for(i = 1;
			    shown_text[i]
			      && struncmp(shown_text[i], "</head>", 7);
			    i++)
			  if(!struncmp(shown_text[i], "<title>", 7)){
			      strcpy(tmp_20k_buf, &shown_text[i][7]);
			      if(p = strchr(tmp_20k_buf, '<'))
				*p = '\0';

			      sprintf(sargs.bar.title = tmp_title,
				      "HELP -- %.*s",
				      ps_global->ttyo->screen_cols-10,
				      strsquish(tmp_20k_buf +  500,
					tmp_20k_buf,
					ps_global->ttyo->screen_cols / 3));
			      break;
			  }
		    }

		    if(!sargs.bar.title)
		      sargs.bar.title = "HELP TEXT";
		}

		sargs.bar.style	   = TextPercent;
		sargs.proc.tool	   = help_processor;
		sargs.proc.data.p  = (void *) &hscroll;
		sargs.resize_exit  = 1;
		sargs.help.text	   = h_special_help_nav;
		sargs.help.title   = "HELP FOR HELP TEXT";
		sargs.keys.menu	   = &km;
		km		   = help_keymenu;
		km.keys		   = keys;
		memcpy(&keys[0], help_keymenu.keys,
		       (help_keymenu.how_many * 12) * sizeof(struct key));
		setbitmap(sargs.keys.bitmap);
		if(flags & HLPD_FROMHELP){
		    km.keys[HLP_EXIT_KEY].name	     = "P";
		    km.keys[HLP_EXIT_KEY].label	     = "Prev Help";
		    km.keys[HLP_EXIT_KEY].bind.cmd   = MC_FINISH;
		    km.keys[HLP_EXIT_KEY].bind.ch[0] = 'p';

		    km.keys[HLP_SUBEXIT_KEY].name	= "E";
		    km.keys[HLP_SUBEXIT_KEY].label	= "Exit Help";
		    km.keys[HLP_SUBEXIT_KEY].bind.cmd   = MC_EXIT;
		    km.keys[HLP_SUBEXIT_KEY].bind.ch[0] = 'e';
		}
		else if(text == h_special_help_nav){
		    km.keys[HLP_EXIT_KEY].name	     = "P";
		    km.keys[HLP_EXIT_KEY].label	     = "Prev Help";
		    km.keys[HLP_EXIT_KEY].bind.cmd   = MC_FINISH;
		    km.keys[HLP_EXIT_KEY].bind.ch[0] = 'p';

		    clrbitn(HLP_MAIN_KEY, sargs.keys.bitmap);
		    clrbitn(HLP_SUBEXIT_KEY, sargs.keys.bitmap);
		}
		else{
		    km.keys[HLP_EXIT_KEY].name	     = "E";
		    km.keys[HLP_EXIT_KEY].label	     = "Exit Help";
		    km.keys[HLP_EXIT_KEY].bind.cmd   = MC_EXIT;
		    km.keys[HLP_EXIT_KEY].bind.ch[0] = 'e';

		    km.keys[HLP_SUBEXIT_KEY].name	= "?";
		    km.keys[HLP_SUBEXIT_KEY].label	= "Help Help";
		    km.keys[HLP_SUBEXIT_KEY].bind.cmd   = MC_HELP;
		    km.keys[HLP_SUBEXIT_KEY].bind.ch[0] = '?';
		}

		if(flags & HLPD_SIMPLE){
		    clrbitn(HLP_MAIN_KEY, sargs.keys.bitmap);
		}
		else
		  sargs.bogus_input = help_bogus_input;

		if(handles){
		    sargs.keys.each_cmd = help_keymenu_tweek;
		    hscroll.keys_formatted = 0;
		}
		else{
		    clrbitn(HLP_VIEW_HANDLE, sargs.keys.bitmap);
		    clrbitn(HLP_PREV_HANDLE, sargs.keys.bitmap);
		    clrbitn(HLP_NEXT_HANDLE, sargs.keys.bitmap);
		}

		if(text != main_menu_tx
		   && text != h_mainhelp_pinehelp)
		  clrbitn(HLP_ALL_KEY, sargs.keys.bitmap);

		if(frag){
		    sargs.start.on	 = Fragment;
		    sargs.start.loc.frag = frag;
		    frag		 = NULL; /* ignore next time */
		}
		else if(offset){
		    sargs.start.on	   = Offset;
		    sargs.start.loc.offset = offset;
		}
		else
		  sargs.start.on = FirstPage;

#ifdef	_WINDOWS
		sargs.mouse.popup = (flags & HLPD_FROMHELP)
				      ? help_subsection_popup : help_popup;
#endif

		cmd = scrolltool(&sargs);

		offset = sargs.start.loc.offset;

		if(F_ON(F_BLANK_KEYMENU,ps_global))
		  FOOTER_ROWS(ps_global) = 1;

		ClearScreen();
	    }

	    so_give(&store);
	}

	free_handles(&handles);
    }
    while(cmd == MC_RESIZE);

#ifdef	HELPFILE
    if(dynamic_text)
      free_list_array(&dynamic_text);
#endif

    return(cmd);
}


/*
 * helper -- compatibility function around newer helper_internal
 */
int
helper(text, title, flags)
    HelpType  text;
    char     *title;
    int	      flags;
{
    return(helper_internal(text, NULL, NULL, title, flags));
}



void
init_helper_getc(help_txt)
    char **help_txt;
{
    g_h_text.crlf   = 0;
    g_h_text.line   = help_txt;
    g_h_text.offset = *g_h_text.line;
}



int
helper_getc(c)
    char *c;
{
    if(g_h_text.crlf){
	*c = '\012';
	g_h_text.crlf = 0;
	return(1);
    }
    else if(g_h_text.offset && *g_h_text.line){
	if(!(*c = *g_h_text.offset++)){
	    g_h_text.offset = *++g_h_text.line;
	    *c = '\015';
	    g_h_text.crlf = 1;
	}

	return(1);
    }

    return(0);
}



int
help_processor(cmd, msgmap, sparms)
    int	      cmd;
    MSGNO_S  *msgmap;
    SCROLL_S *sparms;
{
    int rv = 0;
    char message[64];

    switch(cmd){
	/*----------- Print all the help ------------*/
      case MC_PRINTALL :
	print_all_help();
	break;

      case MC_PRINTMSG :
	sprintf(message, "%.60s ", STYLE_NAME(sparms));
	if(open_printer(message) == 0){
	    print_help(((HELP_SCROLL_S *)sparms->proc.data.p)->help_source);
	    close_printer();
	}

	break;

      case MC_FINISH :
	rv = 1;
	break;

      default :
	panic("Unhandled case");
    }

    return(rv);
}


void
help_keymenu_tweek(sparms, handle_hidden)
    SCROLL_S *sparms;
    int	      handle_hidden;
{
    if(handle_hidden){
	sparms->keys.menu->keys[HLP_VIEW_HANDLE].name  = "";
	sparms->keys.menu->keys[HLP_VIEW_HANDLE].label = "";
    }
    else{
	if(!((HELP_SCROLL_S *)sparms->proc.data.p)->keys_formatted){
	    /* If label's always been blank, force reformatting */
	    mark_keymenu_dirty();
	    sparms->keys.menu->width = 0;
	    ((HELP_SCROLL_S *)sparms->proc.data.p)->keys_formatted = 1;
	}

	sparms->keys.menu->keys[HLP_VIEW_HANDLE].name  = "V";
	sparms->keys.menu->keys[HLP_VIEW_HANDLE].label = "[View Link]";
    }
}


/*
 * print_help - send the raw array of lines to printer
 */
void
print_help(text)
    char **text;
{
    char   *error, buf[256];
    HPRT_S  help_data;

    init_helper_getc(text);

    memset(g_hprt = &help_data, 0, sizeof(HPRT_S));

    help_data.page = 1;
    
    gf_filter_init();

    if(!struncmp(text[0], "<html>", 6)){
	int   i;
	char *p;

	gf_link_filter(gf_html2plain,
		       gf_html2plain_opt(NULL,80,NULL,
					 NULL,GFHP_STRIPPED));
	for(i = 1; i <= 5 && text[i]; i++)
	  if(!struncmp(text[i], "<title>", 7)
	     && (p = srchstr(text[i] + 7, "</title>"))
	     && p - text[i] > 7){
	      help_data.title	  = text[i] + 7;
	      help_data.title_len = p - help_data.title;
	      break;
	  }
    }
    else
      gf_link_filter(gf_wrap, gf_wrap_filter_opt(80, 80, NULL, 0, 0));

    gf_link_filter(gf_line_test,
		   gf_line_test_opt(print_help_page_break, NULL));
    gf_link_filter(gf_nvtnl_local, NULL);

    print_help_page_title(buf, &help_data);
    print_text(buf);
    print_text(NEWLINE);		/* terminate it */
    print_text(NEWLINE);		/* and write two blank links */
    print_text(NEWLINE);

    if(error = gf_pipe(helper_getc, print_char))
      q_status_message1(SM_ORDER | SM_DING, 3, 3, "Printing Error: %.200s", error);

    print_char(ctrl('L'));		/* new page. */
}



void
print_all_help()
{
#ifdef	HELPFILE
    short t;
#else
    struct _help_texts *t;
#endif
    char **h;

    if(open_printer("all 150+ pages of help text ") == 0) {
#ifndef	DOS
	intr_handling_on();
#endif
#ifdef	HELPFILE
        for(t = 0; t < LASTHELP; t++) {
	    if(!(h = get_help_text(t)))
	      return;
#else
	for(t = h_texts; (h = t->help_text) != NO_HELP; t++) {
#endif
	    if(ps_global->intr_pending){
		q_status_message(SM_ORDER, 3, 3,
				 "Print of all help cancelled");
		break;
	    }

	    print_help(h);

#ifdef	HELPFILE
	    free_list_array(&h);
#endif
        }

#ifndef	DOS
	intr_handling_off();
#endif
        close_printer();
    }
}


/*
 * print_help_page_title -- 
 */
void
print_help_page_title(buf, hprt)
    char    *buf;
    HPRT_S  *hprt;
{
    sprintf(buf, "  Pine Help%s%.*s%*s%d",
	    hprt->title_len ? ": " : " Text",
	    min(55, hprt->title_len), hprt->title_len ? hprt->title : "",
	    59 - (hprt->title_len ? min(55, hprt->title_len) : 5),
	    "Page ", hprt->page);
}


/*
 * print_help_page_break -- insert page breaks and such for printed
 *			    help text
 */
int
print_help_page_break(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    char buf[256];

    if(((linenum + (g_hprt->page * 3)) % 62) == 0){
	g_hprt->page++;			/* start on new page */
	buf[0] = ctrl('L');
	print_help_page_title(buf + 1, g_hprt);
	strncat(buf, "\015\012\015\012\015\012", sizeof(buf)-strlen(buf));
	ins = gf_line_test_new_ins(ins, line, buf, strlen(buf));
    }

    return(0);
}


/*
 * help_bogus_input - used by scrolltool to complain about
 *		      invalid user input.
 */
int
help_bogus_input(ch)
    int ch;		
{
    bogus_command(ch, NULL);
    return(0);
}


int
url_local_helper(url)
    char *url;
{
    if(!struncmp(url, "x-pine-help:", 12) && *(url += 12)){
	char		   *frag;
	HelpType	    newhelp;

	/* internal fragment reference? */
	if(frag = strchr(url, '#')){
	    size_t len;

	    if(len = frag - url){
		newhelp = help_name2section(url, len);
	    }
	    else{
		url_local_fragment(url);
		return(1);
	    }
	}
	else
	  newhelp = help_name2section(url, strlen(url));


	if(newhelp != NO_HELP){
	    int rv;

	    rv = helper_internal(newhelp, NULL, frag, "HELP SUB-SECTION",
				 HLPD_NEWWIN | HLPD_SIMPLE | HLPD_FROMHELP);
	    ps_global->mangled_screen = 1;
	    return((rv == MC_EXIT) ? 2 : 1);
	}
    }

    q_status_message1(SM_ORDER | SM_DING, 0, 3,
		      "Unrecognized Internal help: \"%.200s\"", url);
    return(0);
}


char **
get_supported_options()
{
    char         **config;
    DRIVER        *d;
    AUTHENTICATOR *a;
    char          *title = "Supported features in this Pine";
    char           sbuf[MAX_SCREEN_COLS+1];
    int            cnt, len, cols, disabled, any_disabled = 0;;

    /*
     * Line count:
     *   Title + blank			= 2
     *   SSL Title + SSL line + blank	= 3
     *   Auth title + blank		= 2
     *   Driver title + blank		= 2
     *   LDAP title + LDAP line 	= 2
     *   Disabled explanation + blank line = 4
     *   end				= 1
     */
    cnt = 16;
    for(a = mail_lookup_auth(1); a; a = a->next)
      cnt++;
    for(d = (DRIVER *)mail_parameters(NIL, GET_DRIVERS, NIL);
	d; d = d->next)
      cnt++;

    config = (char **)fs_get(cnt * sizeof(char *));
    memset(config, 0, cnt * sizeof(char *));

    cols = ps_global->ttyo ? ps_global->ttyo->screen_cols : 0;
    len = strlen(title);
    sprintf(sbuf, "%*s%s", cols > len ? (cols-len)/2 : 0, "", title);

    config[cnt=0] = cpystr(sbuf);
    config[++cnt] = cpystr("");

    config[++cnt] = cpystr("Encryption:");
    if(mail_parameters(NIL, GET_SSLDRIVER, NIL))
      config[++cnt] = cpystr("  TLS and SSL");
    else
      config[++cnt] = cpystr("  None (no TLS or SSL)");

    config[++cnt] = cpystr("");
    config[++cnt] = cpystr("Authenticators:");
    for(a = mail_lookup_auth(1); a; a = a->next){
	disabled = (a->client == NULL && a->server == NULL);
	any_disabled += disabled;
	sprintf(sbuf, "  %s%s", a->name, disabled ? " (disabled)" : "");
	config[++cnt] = cpystr(sbuf);
    }

    config[++cnt] = cpystr("");
    config[++cnt] = cpystr("Mailbox drivers:");
    for(d = (DRIVER *)mail_parameters(NIL, GET_DRIVERS, NIL);
	d; d = d->next){
	disabled = (d->flags & DR_DISABLE);
	any_disabled += disabled;
	sprintf(sbuf, "  %s%s", d->name, disabled ? " (disabled)" : "");
	config[++cnt] = cpystr(sbuf);
    }

    config[++cnt] = cpystr("");
    config[++cnt] = cpystr("Directories:");
#ifdef	ENABLE_LDAP
      config[++cnt] = cpystr("  LDAP");
#else
      config[++cnt] = cpystr("  None (no LDAP)");
#endif

    if(any_disabled){
	config[++cnt] = cpystr("");
	if(ps_global->ttyo)
	  config[++cnt] = cpystr("Authenticators may be disabled because of the \"disable-these-authenticators\" hidden config option. Mailbox drivers may be disabled because of the \"disable-these-drivers\" hidden config option.");
	else{
	    config[++cnt] = cpystr("Authenticators may be disabled because of the \"disable-these-authenticators\"");
	    config[++cnt] = cpystr("hidden config option. Mailbox drivers may be disabled because of the");
	    config[++cnt] = cpystr("\"disable-these-drivers\" hidden config option.");
	}
    }

    config[++cnt] = NULL;

    return(config);
}


int
url_local_config(url)
    char *url;
{
    if(!struncmp(url, "x-pine-config:", 14)){
	char **config;
	int    rv;

	config = get_supported_options();
	if(config){
	    rv = helper_internal(NO_HELP, config, NULL, "HELP CONFIG",
				 HLPD_NEWWIN | HLPD_SIMPLE | HLPD_FROMHELP);
	    free_list_array(&config);
	}

	ps_global->mangled_screen = 1;
	return((rv == MC_EXIT) ? 2 : 1);
    }

    q_status_message1(SM_ORDER | SM_DING, 0, 3,
		      "Unrecognized Internal help: \"%.200s\"", url);
    return(0);
}


void
dump_supported_options()
{
    char **config;
    char **p;
    FILE  *f = stdout;

    config = get_supported_options();
    if(config){
	display_args_err(NULL, config, 0);
	free_list_array(&config);
    }
}


HelpType
help_name2section(url, url_len)
    char *url;
    int   url_len;
{
    char		name[256];
    HelpType		newhelp = NO_HELP;
#ifdef	HELPFILE
    HelpType		t;
#else
    struct _help_texts *t;
#endif

    sprintf(name, "%.*s", min(url_len,sizeof(name)), url);

#ifdef	HELPFILE
    {
	int		  i;
	char	  buf[MAILTMPLEN];
	FILE	 *helpfile;
	struct hindx  hindex_record;

	/*
	 * Make sure index file is available,
	 * and read appropriate record
	 */
	build_path(buf, ps_global->pine_dir, HELPINDEX, sizeof(buf));
	if(helpfile = fopen(buf, "rb")){
	    for(t = 0; t < LASTHELP; t++){
		i = fseek(helpfile, t * sizeof(struct hindx), SEEK_SET) == 0
		     && fread((void *) &hindex_record,
			      sizeof(struct hindx), 1 ,helpfile) == 1;

		if(!i){	/* problem in fseek or read */
		    q_status_message(SM_ORDER | SM_DING, 3, 5,
		   "No Help!  Can't locate proper offset for help text file.");
		    break;
		}

		if(!strucmp(hindex_record.key, name)){
		    newhelp = t;
		    break;
		}
	    }

	    fclose(helpfile);
	}
	else
	  q_status_message1(SM_ORDER | SM_DING, 3, 5,
			    "No Help!  Index \"%.200s\" not found.", buf);

    }
#else
    for(t = h_texts; t->help_text != NO_HELP; t++)
      if(!strucmp(t->tag, name)){
	  newhelp = t->help_text;
	  break;
      }
#endif

    return(newhelp);
}



#ifdef HELPFILE
/*
 * get_help_text - return the help text associated with index
 *                 in an array of pointers to each line of text.
 */
char **
get_help_text(index)
    HelpType index;
{
    int  i, len;
    char buf[1024], **htext, *tmp;
    struct hindx hindex_record;
    FILE *helpfile;

    if(index < 0 || index >= LASTHELP)
	return(NULL);

    /* make sure index file is available, and read appropriate record */
    build_path(buf, ps_global->pine_dir, HELPINDEX, sizeof(buf));
    if(!(helpfile = fopen(buf, "rb"))){
	q_status_message1(SM_ORDER,3,5,
	    "No Help!  Index \"%.200s\" not found.", buf);
	return(NULL);
    }

    i = fseek(helpfile, index * sizeof(struct hindx), SEEK_SET) == 0
	 && fread((void *)&hindex_record,sizeof(struct hindx),1,helpfile) == 1;

    fclose(helpfile);
    if(!i){	/* problem in fseek or read */
        q_status_message(SM_ORDER, 3, 5,
		  "No Help!  Can't locate proper offset for help text file.");
	return(NULL);
    }

    /* make sure help file is open */
    build_path(buf, ps_global->pine_dir, HELPFILE, sizeof(buf));
    if((helpfile = fopen(buf, "rb")) == NULL){
	q_status_message2(SM_ORDER,3,5,"No Help!  \"%.200s\" : %.200s", buf,
			  error_description(errno));
	return(NULL);
    }

    if(fseek(helpfile, hindex_record.offset, SEEK_SET) != 0
       || fgets(buf, sizeof(buf) - 1, helpfile) == NULL
       || strncmp(hindex_record.key, buf, strlen(hindex_record.key))){
	/* problem in fseek, or don't see key */
        q_status_message(SM_ORDER, 3, 5,
		     "No Help!  Can't locate proper entry in help text file.");
	fclose(helpfile);
	return(NULL);
    }

    htext = (char **)fs_get(sizeof(char *) * (hindex_record.lines + 1));

    for(i = 0; i < hindex_record.lines; i++){
	if(fgets(buf, sizeof(buf) - 1, helpfile) == NULL){
	    htext[i] = NULL;
	    free_list_array(&htext);
	    fclose(helpfile);
            q_status_message(SM_ORDER,3,5,"No Help!  Entry not in help text.");
	    return(NULL);
	}

	if(*buf){
	    if((len = strlen(buf)) > 1
	       && (buf[len-2] == '\n' || buf[len-2] == '\r'))
	      buf[len-2] = '\0';
	    else if(buf[len-1] == '\n' || buf[len-1] == '\r')
	      buf[len-1] = '\0';

	    htext[i] = cpystr(buf);
	}
	else
	  htext[i] = cpystr("");
    }

    htext[i] = NULL;

    fclose(helpfile);
    return(htext);
}
#endif	/* HELPFILE */


#define RMMSGLEN 120
#define RMTIMLEN 15
#define RMJLEN   500
#define RMLLEN   2000
#define RMHLEN   2000
typedef struct _rev_msg {
    unsigned long    seq;
    short            level;	/* -1 for journal, debuglevel for dprint */
    unsigned         continuation:1;
    char             message[RMMSGLEN+1];
    char             timestamp[RMTIMLEN+1];
} REV_MSG_S;

typedef enum {No, Jo, Lo, Hi} RMCat;

static REV_MSG_S rmjoarray[RMJLEN];	/* For regular journal */
static REV_MSG_S rmloarray[RMLLEN];	/* debug 0-4 */
static REV_MSG_S rmhiarray[RMHLEN];	/* debug 5-9 */
static int       rmjofirst = -1, rmjolast = -1;
static int       rmlofirst = -1, rmlolast = -1;
static int       rmhifirst = -1, rmhilast = -1;
static unsigned long rmseq = 0L;

static int        not_right_now;

/*----------------------------------------------------------------------
     Review latest status messages
  -----*/
void
review_messages()
{
    SCROLL_S	    sargs;
    STORE_S        *in_store = NULL, *out_store = NULL;
    gf_io_t         gc, pc;
    int             jo, lo, hi, donejo, donelo, donehi;
    RMCat           rmcat;
    int             cmd, timestamps=0, show_level=-1;
    char            debugkeylabel[20];
    char            timestampkeylabel[] = "NoTimestamps";
    static struct key rev_msg_keys[] =
       {HELP_MENU,
	NULL_MENU,
	{"E","Exit Viewer",{MC_EXIT,1,{'e'}},KS_EXITMODE},
	NULL_MENU,
	{"T",NULL,{MC_TOGGLE,1,{'t'}},KS_NONE},
	{"D",NULL,{MC_JUMP,1,{'d'}},KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", "Save", {MC_SAVETEXT,1,{'s'}}, KS_SAVE}};
    INST_KEY_MENU(rev_msg_keymenu, rev_msg_keys);
#define TIMESTAMP_KEY 4
#define DEBUG_KEY     5

    if((rmjofirst < 0 && rmlofirst < 0 && rmhifirst < 0)
       || rmjofirst >= RMJLEN || rmjolast >= RMJLEN
       || rmlofirst >= RMLLEN || rmlolast >= RMLLEN
       || rmhifirst >= RMHLEN || rmhilast >= RMHLEN
       || (rmjofirst >= 0 && rmjolast < 0)
       || (rmlofirst >= 0 && rmlolast < 0)
       || (rmhifirst >= 0 && rmhilast < 0))
      return;
    
    do{

	if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS)) ||
	   !(out_store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    if(in_store)
	      so_give(&in_store);

	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Failed allocating memory");
	    return;
	}
	
	add_review_message("Turning off new messages while reviewing", 0);
	not_right_now = 1;

	donejo = donehi = donelo = 0;
	jo = rmjofirst;
	if(jo < 0)
	  donejo = 1;

	lo = rmlofirst;
	if(lo < 0)
	  donelo = 1;

	hi = rmhifirst;
	if(hi < 0)
	  donehi = 1;

	while(!(donejo && donelo && donehi)){
	    REV_MSG_S *pjo, *plo, *phi, *p;

	    if(!donejo)
	      pjo = &rmjoarray[jo];
	    else
	      pjo = NULL;

	    if(!donelo)
	      plo = &rmloarray[lo];
	    else
	      plo = NULL;

	    if(!donehi)
	      phi = &rmhiarray[hi];
	    else
	      phi = NULL;

	    if(pjo && (!plo || pjo->seq <= plo->seq)
	       && (!phi || pjo->seq <= phi->seq))
	      rmcat = Jo;
	    else if(plo && (!phi || plo->seq <= phi->seq))
	      rmcat = Lo;
	    else if(phi)
	      rmcat = Hi;
	    else
	      rmcat = No;

	    switch(rmcat){
	      case Jo:
		p = pjo;
		if(jo == rmjofirst && (((rmjolast + 1) % RMJLEN) == rmjofirst))
		  so_puts(in_store,
	"**** Journal entries prior to this point have been trimmed. ****\n");
		break;

	      case Lo:
		p = plo;
		if(show_level >= 0 &&
		   lo == rmlofirst && (((rmlolast + 1) % RMLLEN) == rmlofirst))
		  so_puts(in_store,
	"**** Debug 0-4 entries prior to this point have been trimmed. ****\n");
		break;

	      case Hi:
		p = phi;
		if(show_level >= 5 &&
		   hi == rmhifirst && (((rmhilast + 1) % RMHLEN) == rmhifirst))
		  so_puts(in_store,
	"**** Debug 5-9 entries prior to this point have been trimmed. ****\n");
		break;

	      default:
		p = NULL;
		break;
	    }

	    if(p){
		if(p->level <= show_level){
		    if(timestamps && p->timestamp && p->timestamp[0]){
			so_puts(in_store, p->timestamp);
			so_puts(in_store, ": ");
		    }

		    if(p->message && p->message[0]){
			if(p->continuation)
			  so_puts(in_store, ">");

			so_puts(in_store, p->message);
			so_puts(in_store, "\n");
		    }
		}
	    }

	    switch(rmcat){
	      case Jo:
		if(jo == rmjolast)
		  donejo++;
		else
		  jo = (jo + 1) % RMJLEN;

		break;

	      case Lo:
		if(lo == rmlolast)
		  donelo++;
		else
		  lo = (lo + 1) % RMLLEN;

		break;

	      case Hi:
		if(hi == rmhilast)
		  donehi++;
		else
		  hi = (hi + 1) % RMHLEN;

		break;

	      default:
		donejo++;
		donelo++;
		donehi++;
		break;
	    }
	}


	so_seek(in_store, 0L, 0);
	gf_filter_init();
	gf_link_filter(gf_wrap,
		       gf_wrap_filter_opt(ps_global->ttyo->screen_cols - 4,
					  ps_global->ttyo->screen_cols,
					  NULL, show_level < 0 ? 2 : 0, 0));
	gf_set_so_readc(&gc, in_store);
	gf_set_so_writec(&pc, out_store);
	gf_pipe(gc, pc);
	gf_clear_so_writec(out_store);
	gf_clear_so_readc(in_store);

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text     = so_text(out_store);
	sargs.text.src      = CharStar;
	sargs.text.desc     = "journal";
	sargs.keys.menu     = &rev_msg_keymenu;
	sargs.proc.tool     = journal_processor;
	sargs.start.on      = LastPage;
	sargs.resize_exit   = 1;
	sargs.proc.data.p   = (void *)&show_level;
	setbitmap(sargs.keys.bitmap);

#ifdef DEBUG
#ifdef DEBUGJOURNAL
	sargs.jump_is_debug = 1;
	sargs.help.title    = "HELP FOR DEBUG JOURNAL";
	sargs.help.text     = h_debugjournal;
	sargs.bar.title     = "REVIEW DEBUGGING";
#else	/* !DEBUGJOURNAL */
	clrbitn(DEBUG_KEY, sargs.keys.bitmap);
	sargs.help.title    = "HELP FOR JOURNAL";
	sargs.help.text     = h_journal;
	sargs.bar.title     = "REVIEW RECENT MESSAGES";
#endif	/* !DEBUGJOURNAL */
#else	/* !DEBUG */
	clrbitn(DEBUG_KEY, sargs.keys.bitmap);
	clrbitn(TIMESTAMP_KEY, sargs.keys.bitmap);
	sargs.help.title    = "HELP FOR JOURNAL";
	sargs.help.text     = h_journal;
	sargs.bar.title     = "REVIEW RECENT MESSAGES";
#endif	/* !DEBUG */

	if(timestamps)
	  rev_msg_keys[TIMESTAMP_KEY].label = timestampkeylabel;
	else
	  rev_msg_keys[TIMESTAMP_KEY].label = timestampkeylabel + 2;

	if(show_level >= 0)
	  sprintf(debugkeylabel, "Debug (%d)", show_level);
	else
	  strncpy(debugkeylabel, "DebugView", sizeof(debugkeylabel));

	rev_msg_keys[DEBUG_KEY].label = debugkeylabel;
	KS_OSDATASET(&rev_msg_keys[DEBUG_KEY], KS_NONE);

	if((cmd = scrolltool(&sargs)) == MC_TOGGLE)
	  timestamps = !timestamps;

	so_give(&in_store);
	so_give(&out_store);
	
    }while(cmd != MC_EXIT);

    not_right_now = 0;
    add_review_message("Done reviewing", 0);
}


int
journal_processor(cmd, msgmap, sparms)
    int	      cmd;
    MSGNO_S  *msgmap;
    SCROLL_S *sparms;
{
    switch(cmd){
      case MC_TOGGLE:		/* turn timestamps on or off */
        break;

      default:
        panic("Unexpected command in journal_processor");
	break;
    }
    
    return(1);
}


#ifdef DEBUG
void
debugjournal_to_file(dfile)
    FILE *dfile;
{
    int donejo, donelo, donehi, jo, lo, hi;
    RMCat rmcat;

    if(dfile && (rmjofirst >= 0 || rmlofirst >= 0 || rmhifirst >= 0)
       && rmjofirst < RMJLEN && rmjolast < RMJLEN
       && rmlofirst < RMLLEN && rmlolast < RMLLEN
       && rmhifirst < RMHLEN && rmhilast < RMHLEN
       && (rmjofirst < 0 || rmjolast >= 0)
       && (rmlofirst < 0 || rmlolast >= 0)
       && (rmhifirst < 0 || rmhilast >= 0)){

	donejo = donehi = donelo = 0;
	jo = rmjofirst;
	if(jo < 0)
	  donejo = 1;

	lo = rmlofirst;
	if(lo < 0)
	  donelo = 1;

	hi = rmhifirst;
	if(hi < 0)
	  donehi = 1;

	while(!(donejo && donelo && donehi)){
	    REV_MSG_S *pjo, *plo, *phi, *p;

	    if(!donejo)
	      pjo = &rmjoarray[jo];
	    else
	      pjo = NULL;

	    if(!donelo)
	      plo = &rmloarray[lo];
	    else
	      plo = NULL;

	    if(!donehi)
	      phi = &rmhiarray[hi];
	    else
	      phi = NULL;

	    if(pjo && (!plo || pjo->seq <= plo->seq)
	       && (!phi || pjo->seq <= phi->seq))
	      rmcat = Jo;
	    else if(plo && (!phi || plo->seq <= phi->seq))
	      rmcat = Lo;
	    else if(phi)
	      rmcat = Hi;
	    else
	      rmcat = No;

	    if(rmcat == Jo){
		p = pjo;
		if(jo == rmjofirst &&
		   (((rmjolast + 1) % RMJLEN) == rmjofirst) &&
       fputs("*** Level -1 entries prior to this are deleted", dfile) == EOF)
		  break;
	    }
	    else if(rmcat == Lo){
		p = plo;
		if(lo == rmlofirst &&
		   (((rmlolast + 1) % RMLLEN) == rmlofirst) &&
       fputs("*** Level 0-4 entries prior to this are deleted", dfile) == EOF)
		  break;
	    }
	    else if(rmcat == Hi){
		p = phi;
		if(hi == rmhifirst &&
		   (((rmhilast + 1) % RMHLEN) == rmhifirst) &&
       fputs("*** Level 5-9 entries prior to this are deleted", dfile) == EOF)
		  break;
	    }
	    else if(rmcat == No){
		p = NULL;
	    }

	    if(p){
		if(p->timestamp && p->timestamp[0]
		   && (fputs(p->timestamp, dfile) == EOF
		       || fputs(": ", dfile) == EOF))
		  break;

		if(p->message && p->message[0]
		   && (fputs(p->message, dfile) == EOF
		       || fputs("\n", dfile) == EOF))
		  break;
	    }

	    switch(rmcat){
	      case Jo:
		if(jo == rmjolast)
		  donejo++;
		else
		  jo = (jo + 1) % RMJLEN;

		break;

	      case Lo:
		if(lo == rmlolast)
		  donelo++;
		else
		  lo = (lo + 1) % RMLLEN;

		break;

	      case Hi:
		if(hi == rmhilast)
		  donehi++;
		else
		  hi = (hi + 1) % RMHLEN;

		break;

	      default:
		donejo++;
		donelo++;
		donehi++;
		break;
	    }
	}
    }
}
#endif /* DEBUG */


/*----------------------------------------------------------------------
     Add a message to the circular status message review buffer

    Args: message  -- The message to add
  -----*/
void
add_review_message(message, level)
    char *message;
    int   level;
{
    int   next_is_continuation = 0, cur_is_continuation = 0;
    char *p, *q;

    if(not_right_now || !(message && *message))
      return;

    /*
     * Debug output can have newlines in it, so split up each newline piece
     * by hand and make them separate messages.
     */
    not_right_now = 1;
    for(p = message; *p; p = (*q && !next_is_continuation) ? q+1 : q){
	for(q = p; *q && *q != '\n' && (q-p) < RMMSGLEN; q++)
	  ;

	if(p == q)
	  continue;

	cur_is_continuation = next_is_continuation;

	if((q-p) == RMMSGLEN && *q && *q != '\n')
	  next_is_continuation = 1;
	else
	  next_is_continuation = 0;

	if(level < 0){
	    if(rmjofirst < 0){
		rmjofirst = 0;
		rmjolast  = 0;
	    }
	    else{
		rmjolast = (rmjolast + 1) % RMJLEN;
		if(rmjolast == rmjofirst)
		  rmjofirst = (rmjofirst + 1) % RMJLEN;
	    }

	    rmjoarray[rmjolast].level = (short) level;
	    rmjoarray[rmjolast].seq   = rmseq++;
	    rmjoarray[rmjolast].continuation = cur_is_continuation ? 1 : 0;
	    memset(rmjoarray[rmjolast].message, 0, (RMMSGLEN+1)*sizeof(char));
	    strncpy(rmjoarray[rmjolast].message, p, min(q-p,RMMSGLEN));
#ifdef DEBUG
	    memset(rmjoarray[rmjolast].timestamp, 0, (RMTIMLEN+1)*sizeof(char));
	    strncpy(rmjoarray[rmjolast].timestamp, debug_time(0,1), RMTIMLEN);
#endif
	}
	else if(level <= 4){
	    if(rmlofirst < 0){
		rmlofirst = 0;
		rmlolast  = 0;
	    }
	    else{
		rmlolast = (rmlolast + 1) % RMLLEN;
		if(rmlolast == rmlofirst)
		  rmlofirst = (rmlofirst + 1) % RMLLEN;
	    }

	    rmloarray[rmlolast].level = (short) level;
	    rmloarray[rmlolast].seq   = rmseq++;
	    rmloarray[rmlolast].continuation = cur_is_continuation ? 1 : 0;
	    memset(rmloarray[rmlolast].message, 0, (RMMSGLEN+1)*sizeof(char));
	    strncpy(rmloarray[rmlolast].message, p, min(q-p,RMMSGLEN));
#ifdef DEBUG
	    memset(rmloarray[rmlolast].timestamp, 0, (RMTIMLEN+1)*sizeof(char));
	    strncpy(rmloarray[rmlolast].timestamp, debug_time(0,1), RMTIMLEN);
#endif
	}
	else{
	    if(rmhifirst < 0){
		rmhifirst = 0;
		rmhilast  = 0;
	    }
	    else{
		rmhilast = (rmhilast + 1) % RMHLEN;
		if(rmhilast == rmhifirst)
		  rmhifirst = (rmhifirst + 1) % RMHLEN;
	    }

	    rmhiarray[rmhilast].level = (short) level;
	    rmhiarray[rmhilast].seq   = rmseq++;
	    rmhiarray[rmhilast].continuation = cur_is_continuation ? 1 : 0;
	    memset(rmhiarray[rmhilast].message, 0, (RMMSGLEN+1)*sizeof(char));
	    strncpy(rmhiarray[rmhilast].message, p, min(q-p,RMMSGLEN));
#ifdef DEBUG
	    memset(rmhiarray[rmhilast].timestamp, 0, (RMTIMLEN+1)*sizeof(char));
	    strncpy(rmhiarray[rmhilast].timestamp, debug_time(0,1), RMTIMLEN);
#endif
	}
    }

    not_right_now = 0;
}



/*
 *  * * * * * * * *    Bug Report support routines    * * * * * * * *
 */



/*
 * standard type of storage object used for body parts...
 */
#ifdef	DOS
#define		  PART_SO_TYPE	TmpFileStar
#else
#define		  PART_SO_TYPE	CharStar
#endif


int
gripe_gripe_to(url)
    char *url;
{
    char      *composer_title, *url_copy, *optstr, *p;
    int	       opts = 0;
    BODY      *body = NULL;
    ENVELOPE  *outgoing = NULL;
    REPLY_S    fake_reply;
    PINEFIELD *pf = NULL;
    long       msgno = mn_m2raw(ps_global->msgmap, 
				mn_get_cur(ps_global->msgmap));

    url_copy = cpystr(url + 13);
    if(optstr = strchr(url_copy, '?'))
      *optstr++ = '\0';

    outgoing		 = mail_newenvelope();
    outgoing->message_id = generate_message_id();

    if(outgoing->to = gripe_token_addr(url_copy)){
	composer_title = "COMPOSE TO LOCAL SUPPORT";
	dprint(1, (debugfile, 
		   "\n\n   -- Send to local support(%s@%s) --\n",
		   outgoing->to->mailbox ? outgoing->to->mailbox : "NULL",
		   outgoing->to->host ? outgoing->to->host : "NULL"));
    }
    else{			/* must be global */
	composer_title = "REQUEST FOR ASSISTANCE";
	rfc822_parse_adrlist(&outgoing->to, url_copy, ps_global->maildomain);
    }

    /*
     * Sniff thru options
     */
    while(optstr){
	if(p = strchr(optstr, '?'))	/* tie off list item */
	  *p++ = '\0';

	if(!strucmp(optstr, "config"))
	  opts |= GRIPE_OPT_CONF;
	else if(!strucmp(optstr, "curmsg"))
	  opts |= GRIPE_OPT_MSG;
	else if(!strucmp(optstr, "local"))
	  opts |= GRIPE_OPT_LOCAL;
	else if(!strucmp(optstr, "keys"))
	  opts |= GRIPE_OPT_KEYS;

	optstr = p;
    }

    /* build body and hand off to composer... */
    if(gripe_newbody(ps_global, &body, msgno, opts) == 0){
	pf = (PINEFIELD *) fs_get(sizeof(PINEFIELD));
	memset(pf, 0, sizeof(PINEFIELD));
	pf->name		   = cpystr("X-Generated-Via");
	pf->type		   = FreeText;
	pf->textbuf		   = gripe_id("Pine Bug Report screen");
	memset((void *)&fake_reply, 0, sizeof(fake_reply));
	fake_reply.flags	   = REPLY_PSEUDO;
	fake_reply.data.pico_flags = P_HEADEND;
	pine_send(outgoing, &body, composer_title, NULL, NULL,
		  &fake_reply, NULL, NULL, pf, 0);
    }
    
    ps_global->mangled_screen = 1;
    mail_free_envelope(&outgoing);

    if(body)
      pine_free_body(&body);

    fs_give((void **) &url_copy);
    
    return(10);
}


int
gripe_newbody(ps, body, msgno, flags)
    struct pine *ps;
    BODY       **body;
    long         msgno;
    int          flags;
{
    BODY        *pb;
    PART       **pp;
    STORE_S	*store;
    gf_io_t      pc;
    static char *err = "Problem creating space for message text.";
    int          i;
    char         tmp[MAILTMPLEN], *p;

    if(store = so_get(PicoText, NULL, EDIT_ACCESS)){
	*body = mail_newbody();

	if((p = detoken(NULL, NULL, 2, 0, 1, NULL, NULL)) != NULL){
	    if(*p)
	      so_puts(store, p);

	    fs_give((void **) &p);
	}
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4, err);
	return(-1);
    }

    if(flags){
	/*---- Might have multiple parts ----*/
	(*body)->type			= TYPEMULTIPART;
	/*---- The TEXT part/body ----*/
	(*body)->nested.part            = mail_newbody_part();
	(*body)->nested.part->body.type = TYPETEXT;
	(*body)->nested.part->body.contents.text.data = (void *) store;

	/*---- create object, and write current config into it ----*/
	pp = &((*body)->nested.part->next);

	if(flags & GRIPE_OPT_CONF){
	    *pp			     = mail_newbody_part();
	    pb			     = &((*pp)->body);
	    pp			     = &((*pp)->next);
	    pb->type		     = TYPETEXT;
	    pb->id		     = generate_message_id();
	    pb->description	     = cpystr("Pine Configuration Data");
	    pb->parameter	     = mail_newbody_parameter();
	    pb->parameter->attribute = cpystr("name");
	    pb->parameter->value     = cpystr("config.txt");

	    if(store = so_get(CharStar, NULL, EDIT_ACCESS)){
		extern char datestamp[], hoststamp[];

		pb->contents.text.data = (void *) store;
		gf_set_so_writec(&pc, store);
		gf_puts("Pine built ", pc);
		gf_puts(datestamp, pc);
		gf_puts(" on host: ", pc);
		gf_puts(hoststamp, pc);
		gf_puts("\n", pc);

		dump_pine_struct(ps, pc);
		dump_config(ps, pc, 0);

		pb->size.bytes = strlen((char *) so_text(store));
		gf_clear_so_writec(store);
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 4, err);
		return(-1);
	    }
	}

	if(flags & GRIPE_OPT_KEYS){
	    *pp			     = mail_newbody_part();
	    pb			     = &((*pp)->body);
	    pp			     = &((*pp)->next);
	    pb->type		     = TYPETEXT;
	    pb->id		     = generate_message_id();
	    pb->description	     = cpystr("Recent User Input");
	    pb->parameter	      = mail_newbody_parameter();
	    pb->parameter->attribute  = cpystr("name");
	    pb->parameter->value      = cpystr("uinput.txt");

	    if(store = so_get(CharStar, NULL, EDIT_ACCESS)){
		pb->contents.text.data = (void *) store;

		so_puts(store, "User's most recent input:\n");

		/* dump last n keystrokes */
		so_puts(store, "========== Latest keystrokes ==========\n");
		while((i = key_playback(0)) != -1){
		    sprintf(tmp, "\t%.20s\t(0x%04.4x)\n", pretty_command(i), i);
		    so_puts(store, tmp);
		}

		pb->size.bytes = strlen((char *) so_text(store));
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 4, err);
		return(-1);
	    }
	}

	/* check for local debugging info? */
	if((flags & GRIPE_OPT_LOCAL)
	   && ps_global->VAR_BUGS_EXTRAS 
	   && can_access(ps_global->VAR_BUGS_EXTRAS, EXECUTE_ACCESS) == 0){
	    char *error		      = NULL;

	    *pp			      = mail_newbody_part();
	    pb			      = &((*pp)->body);
	    pp			      = &((*pp)->next);
	    pb->type		      = TYPETEXT;
	    pb->id		      = generate_message_id();
	    pb->description	      = cpystr("Local Configuration Data");
	    pb->parameter	      = mail_newbody_parameter();
	    pb->parameter->attribute  = cpystr("name");
	    pb->parameter->value      = cpystr("lconfig.txt");

	    if(store = so_get(CharStar, NULL, EDIT_ACCESS)){
		PIPE_S  *syspipe;		
		gf_io_t  gc;
		
		pb->contents.text.data = (void *) store;
		gf_set_so_writec(&pc, store);
		if(syspipe = open_system_pipe(ps_global->VAR_BUGS_EXTRAS,
					 NULL, NULL,
					 PIPE_READ | PIPE_STDERR | PIPE_USER,
					 0)){
		    gf_set_readc(&gc, (void *)syspipe, 0, PipeStar);
		    gf_filter_init();
		    error = gf_pipe(gc, pc);
		    (void) close_system_pipe(&syspipe, NULL, 0);
		}
		else
		  error = "executing config collector";

		gf_clear_so_writec(store);
	    }
	    
	    if(error){
		q_status_message1(SM_ORDER | SM_DING, 3, 4, 
				  "Problem %.200s", error);
		return(-1);
	    }
	    else			/* fixup attachment's size */
	      pb->size.bytes = strlen((char *) so_text(store));
	}

	if((flags & GRIPE_OPT_MSG) && mn_get_total(ps->msgmap) > 0L){
	    int ch = 0;
	
	    ps->redrawer = att_cur_drawer;
	    att_cur_drawer();

	    if((ch = one_try_want_to("Attach current message to report",
				     'y','x',NO_HELP,
				     WT_FLUSH_IN|WT_SEQ_SENSITIVE)) == 'y'){
		*pp		      = mail_newbody_part();
		pb		      = &((*pp)->body);
		pp		      = &((*pp)->next);
		pb->type	      = TYPEMESSAGE;
		pb->id		      = generate_message_id();
		sprintf(tmp, "Problem Message (%ld of %ld)",
			mn_get_cur(ps->msgmap), mn_get_total(ps->msgmap));
		pb->description	      = cpystr(tmp);

		/*---- Package each message in a storage object ----*/
		if(store = so_get(PART_SO_TYPE, NULL, EDIT_ACCESS)){
		    pb->contents.text.data = (void *) store;
		}
		else{
		    q_status_message(SM_ORDER | SM_DING, 3, 4, err);
		    return(-1);
		}

		/* write the header */
		if((p = mail_fetch_header(ps->mail_stream, msgno, NIL, NIL,
					  NIL, FT_PEEK)) && *p)
		  so_puts(store, p);
		else
		  return(-1);

#if	defined(DOS) && !defined(WIN32)
		/* write fetched text to disk */
		mail_parameters(ps->mail_stream, SET_GETS, (void *)dos_gets);
		append_file = (FILE *) so_text(store);

		/* HACK!  See mailview.c:format_message for details... */
		ps->mail_stream->text = NULL;
		/* write the body */
		if(!pine_mail_fetch_text(ps->mail_stream, msgno,
					 NULL, NULL, NIL))
		  return(-1);

		pb->size.bytes = ftell(append_file);
		/* next time body may stay in core */
		mail_parameters(ps->mail_stream, SET_GETS, (void *)NULL);
		append_file   = NULL;
		mail_gc(ps->mail_stream, GC_TEXTS);
		so_release(store);
#else
		pb->size.bytes = strlen(p);
		so_puts(store, "\015\012");

		if((p = pine_mail_fetch_text(ps->mail_stream, 
					     msgno, NULL, NULL, NIL))
		   &&  *p)
		  so_puts(store, p);
		else
		  return(-1);

		pb->size.bytes += strlen(p);
#endif
	    }
	    else if(ch == 'x'){
		q_status_message(SM_ORDER, 0, 3, "Bug report cancelled.");
		return(-1);
	    }
	}
    }
    else{
	/*---- Only one part! ----*/
	(*body)->type = TYPETEXT;
	(*body)->contents.text.data = (void *) store;
    }

    return(0);
}


ADDRESS *
gripe_token_addr(token)
    char *token;
{
    char    *p;
    ADDRESS *a = NULL;

    if(token && *token++ == '_'){
	if(!strcmp(token, "LOCAL_ADDRESS_")){
	    p = (ps_global->VAR_LOCAL_ADDRESS
		 && ps_global->VAR_LOCAL_ADDRESS[0])
		    ? ps_global->VAR_LOCAL_ADDRESS
		    : "postmaster";
	    a = rfc822_parse_mailbox(&p, ps_global->maildomain);
	    a->personal = cpystr((ps_global->VAR_LOCAL_FULLNAME 
				  && ps_global->VAR_LOCAL_FULLNAME[0])
				    ? ps_global->VAR_LOCAL_FULLNAME 
				    : "Place to report Pine Bugs");
	}
	else if(!strcmp(token, "BUGS_ADDRESS_")){
	    p = (ps_global->VAR_BUGS_ADDRESS
		 && ps_global->VAR_BUGS_ADDRESS[0])
		    ? ps_global->VAR_BUGS_ADDRESS : "postmaster";
	    a = rfc822_parse_mailbox(&p, ps_global->maildomain);
	    a->personal = cpystr((ps_global->VAR_BUGS_FULLNAME 
				  && ps_global->VAR_BUGS_FULLNAME[0])
				    ? ps_global->VAR_BUGS_FULLNAME 
				    : "Place to report Pine Bugs");
	}
    }

    return(a);
}


char *
gripe_id(key)
    char *key;
{
    int i;
    
    /*
     * Build our contribution to the subject; part constant string
     * and random 4 character alpha numeric string.
     */
    tmp_20k_buf[0] = '\0';
    sprintf(tmp_20k_buf, "%s (ID %c%c%d%c%c)", key,
	    ((i = (int)(random() % 36L)) < 10) ? '0' + i : 'A' + (i - 10),
	    ((i = (int)(random() % 36L)) < 10) ? '0' + i : 'A' + (i - 10),
	    (int)(random() % 10L),
	    ((i = (int)(random() % 36L)) < 10) ? '0' + i : 'A' + (i - 10),
	    ((i = (int)(random() % 36L)) < 10) ? '0' + i : 'A' + (i - 10));
    return(cpystr(tmp_20k_buf));
}


int
gripe_bogus_input(ch)
    int ch;
{
    return(1);			/* done! */
}



/*
 * Used by gripe_tool.
 */
void
att_cur_drawer()
{
    int	       i, dline, j;
    char       buf[256+1];

    /* blat helpful message to screen */
    ClearBody();
    j = 0;
    for(dline = 2;
	dline < ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global);
	dline++){
	for(i = 0; i < 256 && att_cur_msg[j] && att_cur_msg[j] != '\n'; i++)
	  buf[i] = att_cur_msg[j++];

	buf[i] = '\0';
	if(att_cur_msg[j])
	  j++;
	else if(!i)
	  break;

        PutLine0(dline, 1, buf);
    }
}


#ifdef	_WINDOWS
/*
 * 
 */
int
help_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup hp_menu[10];
    int	   i = -1;

    if(in_handle){
	hp_menu[++i].type	= tQueue;
	hp_menu[i].label.style	= lNormal;
	hp_menu[i].label.string = "View Help Section";
	hp_menu[i].data.val	= 'V';
    }

    hp_menu[++i].type	    = tQueue;
    hp_menu[i].label.style  = lNormal;
    hp_menu[i].label.string = "Exit Help";
    hp_menu[i].data.val	    = 'E';

    hp_menu[++i].type = tTail;

    mswin_popup(hp_menu);

    return(0);
}


/*
 * 
 */
int
help_subsection_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup hp_menu[10];
    int	   i = -1;

    if(in_handle){
	hp_menu[++i].type	= tQueue;
	hp_menu[i].label.style  = lNormal;
	hp_menu[i].label.string = "View Help Section";
	hp_menu[i].data.val	= 'V';
    }

    hp_menu[++i].type	    = tQueue;
    hp_menu[i].label.style  = lNormal;
    hp_menu[i].label.string = "Previous Help Section";
    hp_menu[i].data.val	    = 'P';

    hp_menu[++i].type	    = tQueue;
    hp_menu[i].label.style  = lNormal;
    hp_menu[i].label.string = "Exit Help";
    hp_menu[i].data.val	    = 'E';

    hp_menu[++i].type = tTail;

    if(mswin_popup(hp_menu) == (in_handle ? 1 : 0))
      /*(void) helper_internal()*/;

    return(0);
}
#endif
