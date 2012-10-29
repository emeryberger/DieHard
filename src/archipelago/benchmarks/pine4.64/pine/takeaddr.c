#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: takeaddr.c 14081 2005-09-12 22:04:25Z hubert@u.washington.edu $";
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

/*====================================================================== 
    takeaddr.c
    Mostly support for Take Address command.
 ====*/


#include "headers.h"
#include "adrbklib.h"

/*
 * Information used to paint and maintain a line on the TakeAddr screen
 */
typedef struct takeaddr_line {
    unsigned int	  checked:1;/* addr is selected                     */
    unsigned int	  skip_it:1;/* skip this one                        */
    unsigned int	  print:1;  /* for printing only                    */
    unsigned int	  frwrded:1;/* forwarded from another pine          */
    char                 *strvalue; /* alloc'd value string                 */
    ADDRESS              *addr;     /* original ADDRESS this line came from */
    char                 *nickname; /* The first TA may carry this extra    */
    char                 *fullname; /*   information, or,                   */
    char                 *fcc;      /* all vcard ta's have it.              */
    char                 *comment;
    struct takeaddr_line *next, *prev;
} TA_S;

typedef struct takeaddress_screen {
    ScreenMode mode;
    TA_S      *current,
              *top_line;
} TA_SCREEN_S;

static TA_SCREEN_S *ta_screen;
static char        *fakedomain = "@";

static ESCKEY_S save_or_export[] = {
    {'s', 's', "S", "Save"},
    {'e', 'e', "E", "Export"},
    {-1, 0, NULL, NULL}};


void           add_abook_entry PROTO((TA_S *, char *, char *, char *,
					char *, int, TA_STATE_S **, char *));
int            add_addresses_to_talist PROTO((struct pine *, long, char *,
						    TA_S **, ADDRESS *, int));
PerAddrBook   *check_for_addrbook PROTO((char *));
int            dup_addrs PROTO((ADDRESS *, ADDRESS *));
int            edit_nickname PROTO((AdrBk *, AddrScrn_Disp *, int, char *,
						char *, HelpType, int, int));
int            eliminate_dups_and_us PROTO((TA_S *));
int            eliminate_dups_but_not_us PROTO((TA_S *));
int            eliminate_dups_and_maybe_us PROTO((TA_S *, int));
void           export_vcard_att PROTO((struct pine *, int, long, ATTACH_S *));
TA_S          *fill_in_ta PROTO((TA_S **, ADDRESS *, int, char *));
TA_S          *first_checked PROTO((TA_S *));
TA_S          *first_sel_taline PROTO((TA_S *));
TA_S          *last_sel_taline PROTO((TA_S *));
TA_S          *first_taline PROTO((TA_S *));
TA_S          *whereis_taline PROTO((TA_S *));
void           free_taline PROTO((TA_S **));
int            get_line_of_message PROTO((STORE_S *, char *, int));
char          *getaltcharset PROTO ((char *, char **, char **, int *));
int	       grab_addrs_from_body PROTO((MAILSTREAM *,long,BODY *,TA_S **));
int            is_talist_of_one PROTO((TA_S *));
char         **list_of_checked PROTO((TA_S *));
TA_S          *new_taline PROTO((TA_S **));
TA_S          *next_sel_taline PROTO((TA_S *));
TA_S          *next_taline PROTO((TA_S *));
TA_S          *pre_sel_taline PROTO((TA_S *));
TA_S          *pre_taline PROTO((TA_S *));
LINES_TO_TAKE *new_ltline PROTO((LINES_TO_TAKE **));
void           free_ltline PROTO((LINES_TO_TAKE **));
int            convert_ta_to_lines PROTO((TA_S *, LINES_TO_TAKE **));
int            process_vcard_atts PROTO((MAILSTREAM *, long, BODY *, BODY *,
					 char *, TA_S **));
void           switch_to_last_comma_first PROTO((char *, char *, size_t));
int	       ta_do_take PROTO((TA_S *, int, int, TA_STATE_S **, char *));
int            ta_take_marked_addrs PROTO((int, TA_S *, int, TA_STATE_S **,
								    char *));
int            ta_take_single_addr PROTO((TA_S *, int, TA_STATE_S **, char *));
int            ta_mark_all PROTO((TA_S *));
int            ta_unmark_all PROTO((TA_S *));
void           take_to_addrbooks PROTO((char **, char *, char *, char *, char *,
					char *, int, TA_STATE_S **, char *));
void           take_to_addrbooks_frontend PROTO((char **, char *, char *,
			 char *, char *, char *, int, TA_STATE_S **, char *));
int            take_without_edit PROTO((TA_S *, int, int,
					    TA_STATE_S **, char *));
void           takeaddr_bypass PROTO((struct pine *, TA_S *, TA_STATE_S **));
int            takeaddr_screen PROTO((struct pine *, TA_S *, int,
				      ScreenMode, TA_STATE_S **, char *));
void           takeaddr_screen_redrawer_list PROTO((void));
void           takeaddr_screen_redrawer_single PROTO((void));
int            update_takeaddr_screen PROTO((struct pine *, TA_S *,
							TA_SCREEN_S *, Pos *));
PerAddrBook   *use_this_addrbook PROTO((int, char *));
int            vcard_to_ta PROTO((char *, char *, char *, char *, char *,
				  char *, TA_S **));
#ifdef  _WINDOWS
int            ta_scroll_up PROTO((long));
int            ta_scroll_down PROTO((long));
int            ta_scroll_to_pos PROTO((long));
int            ta_scroll_callback PROTO((int, long));
#endif



/*
 * Edit a nickname field.
 *
 * Args: abook     -- the addressbook handle
 *       dl        -- display list line (NULL if new entry)
 *    command_line -- line to prompt on
 *       orig      -- nickname to edit
 *       prompt    -- prompt
 *      this_help  -- help
 * return_existing -- changes the behavior when a user types in a nickname
 *                    which already exists in this abook.  If not set, it
 *                    will just keep looping until the user changes; if set,
 *                    it will return -8 to the caller and orig will be set
 *                    to the matching nickname.
 *
 * Returns: -10 to cancel
 *          -9  no change
 *          -7  only case of nickname changed (only happens if dl set)
 *          -8  existing nickname chosen (only happens if return_existing set)
 *           0  new value copied into orig
 */
int
edit_nickname(abook, dl, command_line, orig, prompt, this_help,
		return_existing, takeaddr)
    AdrBk         *abook;
    AddrScrn_Disp *dl;
    int            command_line;
    char          *orig,
		  *prompt;
    HelpType       this_help;
    int            return_existing,
		   takeaddr;
{
    char         edit_buf[MAX_NICKNAME + 1];
    HelpType     help;
    int          flags, rc;
    AdrBk_Entry *check, *passed_in_ae;
    ESCKEY_S     ekey[2];
    char        *error = NULL;

    ekey[0].ch    = ctrl('T');
    ekey[0].rval  = 2;
    ekey[0].name  = "^T";
    ekey[0].label = "To AddrBk";

    ekey[1].ch    = -1;

    strncpy(edit_buf, orig, sizeof(edit_buf)-1);
    edit_buf[sizeof(edit_buf)-1] = '\0';
    if(dl)
      passed_in_ae = adrbk_get_ae(abook, (a_c_arg_t)dl->elnum, Lock);
    else
      passed_in_ae = (AdrBk_Entry *)NULL;

    help  = NO_HELP;
    rc    = 0;
    check = NULL;
    do{
	if(error){
	    q_status_message(SM_ORDER, 3, 4, error);
	    fs_give((void **)&error);
	}

	/* display a message because adrbk_lookup_by_nick returned positive */
	if(check){
	    if(return_existing){
		strncpy(orig, edit_buf, sizeof(edit_buf)-1);
		orig[sizeof(edit_buf)-1] = '\0';
		if(passed_in_ae)
		  (void)adrbk_get_ae(abook, (a_c_arg_t)dl->elnum, Unlock);
		return -8;
	    }

            q_status_message1(SM_ORDER, 0, 4,
		    "Already an entry with nickname \"%.200s\"", edit_buf);
	}

	if(rc == 3)
          help = (help == NO_HELP ? this_help : NO_HELP);

	flags = OE_APPEND_CURRENT;
	rc = optionally_enter(edit_buf, command_line, 0, sizeof(edit_buf),
			      prompt, ekey, help, &flags);

	if(rc == 1)  /* ^C */
	  break;

	if(rc == 2){ /* ^T */
	    void (*redraw) () = ps_global->redrawer;
	    char *returned_nickname;
	    SAVE_STATE_S state;  /* For saving state of addrbooks temporarily */
	    TITLEBAR_STATE_S *tbstate = NULL;

	    tbstate = save_titlebar_state();
	    save_state(&state);
	    if(takeaddr)
	      returned_nickname = addr_book_takeaddr();
	    else
	      returned_nickname = addr_book_selnick();

	    restore_state(&state);
	    if(returned_nickname){
		strncpy(edit_buf, returned_nickname, sizeof(edit_buf)-1);
		edit_buf[sizeof(edit_buf)-1] = '\0';
		fs_give((void **)&returned_nickname);
	    }

	    ClearScreen();
	    restore_titlebar_state(tbstate);
	    free_titlebar_state(&tbstate);
	    redraw_titlebar();
	    if(ps_global->redrawer = redraw) /* reset old value, and test */
	      (*ps_global->redrawer)();
	}
            
    }while(rc == 2 ||
	   rc == 3 ||
	   rc == 4 ||
	   nickname_check(edit_buf, &error) ||
           ((check =
	       adrbk_lookup_by_nick(abook, edit_buf, (adrbk_cntr_t *)NULL)) &&
	     check != passed_in_ae));

    if(rc != 0){
	if(passed_in_ae)
	  (void)adrbk_get_ae(abook, (a_c_arg_t)dl->elnum, Unlock);

	return -10;
    }

    /* only the case of nickname changed */
    if(passed_in_ae && check == passed_in_ae && strcmp(edit_buf, orig)){
	(void)adrbk_get_ae(abook, (a_c_arg_t)dl->elnum, Unlock);
	strncpy(orig, edit_buf, sizeof(edit_buf)-1);
	orig[sizeof(edit_buf)-1] = '\0';
	return -7;
    }

    if(passed_in_ae)
      (void)adrbk_get_ae(abook, (a_c_arg_t)dl->elnum, Unlock);

    if(strcmp(edit_buf, orig) == 0) /* no change */
      return -9;
    
    strncpy(orig, edit_buf, sizeof(edit_buf)-1);
    orig[sizeof(edit_buf)-1] = '\0';
    return 0;
}


/*
 * Add an entry to address book.
 * It is for capturing addresses off incoming mail.
 * This is a front end for take_to_addrbooks.
 * It is also used for replacing an existing entry and for adding a single
 * new address to an existing list.
 *
 * The reason this is here is so that when Taking a single address, we can
 * rearrange the fullname to be Last, First instead of First Last.
 *
 * Args: ta_entry -- the entry from the take screen
 *   command_line -- line to prompt on
 *
 * Result: item is added to one of the address books,
 *       an error message is queued if appropriate.
 */
void
add_abook_entry(ta_entry, nick, fullname, fcc, comment, command_line, tas, cmd)
    TA_S *ta_entry;
    char *nick;
    char *fullname;
    char *fcc;
    char *comment;
    int   command_line;
    TA_STATE_S **tas;
    char *cmd;
{
    ADDRESS *addr;
    char     new_fullname[MAX_FULLNAME + 1],
	     new_address[MAX_ADDRESS + 1];
    char   **new_list, *charset = NULL;
    char    *old_fullname;
    int      need_to_encode = 0;

    dprint(5, (debugfile, "-- add_abook_entry --\n"));

    /*-- rearrange full name (Last, First) ---*/
    new_fullname[0]              = '\0';
    new_fullname[sizeof(new_fullname)-1] = '\0';
    new_fullname[sizeof(new_fullname)-2] = '\0';
    addr = ta_entry->addr;
    if(!fullname && addr->personal != NULL){
	if(F_ON(F_DISABLE_TAKE_LASTFIRST, ps_global))
	  strncpy(new_fullname, addr->personal, sizeof(new_fullname)-1);
	else{
	    char buftmp[MAILTMPLEN];

	    /*
	     * In order to swap First Last to Last, First we have to decode
	     * and re-encode.
	     */
	    sprintf(buftmp, "%.500s", addr->personal);
	    old_fullname = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						  SIZEOF_20KBUF,
						  buftmp, &charset);
	    if(old_fullname == buftmp){
		if(charset)
		  fs_give((void **)&charset);

		charset = NULL;
	    }
	    else
	      need_to_encode++;

	    switch_to_last_comma_first(old_fullname, new_fullname,
				       sizeof(new_fullname));
	}
    }

    if(need_to_encode){
	char *s;
	char  buf[MAX_FULLNAME + 10];

	/* re-encode in original charset */
	s = rfc1522_encode(buf, sizeof(buf), (unsigned char *)new_fullname,
	    charset ? charset : ps_global->VAR_CHAR_SET);
	if(s != new_fullname){
	    strncpy(new_fullname, s, sizeof(new_fullname)-1);
	    new_fullname[sizeof(new_fullname)-1] = '\0';
	}
	
	new_fullname[sizeof(new_fullname)-1] = '\0';
	if(charset)
	  fs_give((void **)&charset);
    }

    /* initial value for new address */
    new_address[0] = '\0';
    new_address[sizeof(new_address)-1] = '\0';
    if(addr->mailbox && addr->mailbox[0]){
	char *scratch, *p, *t, *u;
	unsigned long l;

	scratch = (char *)fs_get((size_t)est_size(addr));
	scratch[0] = '\0';
	rfc822_write_address(scratch, addr);
	if(p = srchstr(scratch, "@.RAW-FIELD.")){
	  for(t = p; ; t--)
	    if(*t == '&'){		/* find "leading" token */
		*t++ = ' ';		/* replace token */
		*p = '\0';		/* tie off string */
		u = (char *)rfc822_base64((unsigned char *)t,
					  (unsigned long)strlen(t), &l);
		*p = '@';		/* restore 'p' */
		rplstr(p, 12, "");	/* clear special token */
		rplstr(t, strlen(t), u);  /* Null u is handled */
		if(u)
		  fs_give((void **)&u);
	    }
	    else if(t == scratch)
	      break;
	}

	strncpy(new_address, scratch, sizeof(new_address)-1);
	new_address[sizeof(new_address)-1] = '\0';

	if(scratch)
	  fs_give((void **)&scratch);
    }

    if(ta_entry->frwrded){
	ADDRESS *a;
	int i, j;

	for(i = 0, a = addr; a; i++, a = a->next)
	  ;/* just counting for alloc below */

	/* catch special case where empty addr was set in vcard_to_ta */
	if(i == 1 && !addr->host && !addr->mailbox && !addr->personal)
	  i = 0;

	new_list = (char **)fs_get((i+1) * sizeof(char *));
	for(j = 0, a = addr; i && a; j++, a = a->next){
	    ADDRESS *next_addr;
	    char    *bufp;

	    next_addr = a->next;
	    a->next = NULL;
	    bufp = (char *)fs_get((size_t)est_size(a));
	    new_list[j] = cpystr(addr_string(a, bufp));
	    a->next = next_addr;
	    fs_give((void **)&bufp);
	}

	new_list[j] = NULL;
    }
    else{
	int i = 0, j;

	j = (ta_entry->strvalue && ta_entry->strvalue[0]) ? 2 : 1;

	new_list = (char **)fs_get(j * sizeof(char *));

	if(j == 2)
	  new_list[i++] = cpystr(ta_entry->strvalue);

	new_list[i] = NULL;
    }

    take_to_addrbooks_frontend(new_list, nick,
			       fullname ? fullname : new_fullname,
			       new_address, fcc, comment, command_line,
			       tas, cmd);
    free_list_array(&new_list);
}


void
take_to_addrbooks_frontend(new_entries, nick, fullname, addr, fcc,
			    comment, cmdline, tas, cmd)
    char **new_entries;
    char  *nick;
    char  *fullname;
    char  *addr;
    char  *fcc;
    char  *comment;
    int    cmdline;
    TA_STATE_S **tas;
    char  *cmd;
{
    jmp_buf save_jmp_buf;
    int    *save_nesting_level;

    dprint(5, (debugfile, "-- take_to_addrbooks_frontend --\n"));

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0))
      ps_global->mangled_footer = 1;

    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	q_status_message(SM_ORDER, 5, 10, "Resetting address book...");
	dprint(1, (debugfile,
	    "RESETTING address book... take_to_addrbooks_frontend!\n"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    take_to_addrbooks(new_entries, nick, fullname, addr, fcc, comment, cmdline,
		      tas, cmd);
    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    ab_nesting_level--;
    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);
}


/*
 * Add to address book, called from take screen.
 * It is also used for adding to an existing list or replacing an existing
 * entry.
 *
 * Args: new_entries -- a list of addresses to add to a list or to form
 *                      a new list with
 *              nick -- if adding new entry, suggest this for nickname
 *          fullname -- if adding new entry, use this for fullname
 *              addr -- if only one new_entry, this is its addr
 *               fcc -- if adding new entry, use this for fcc
 *           comment -- if adding new entry, use this for comment
 *      command_line -- line to prompt on
 *
 * Result: item is added to one of the address books,
 *       an error message is queued if appropriate.
 */
void
take_to_addrbooks(new_entries, nick, fullname, addr, fcc, comment, command_line,
		  tas, cmd)
    char **new_entries;
    char  *nick;
    char  *fullname;
    char  *addr;
    char  *fcc;
    char  *comment;
    int    command_line;
    TA_STATE_S **tas;
    char  *cmd;
{
    char          new_nickname[MAX_NICKNAME + 1], exist_nick[MAX_NICKNAME + 1];
    char          prompt[200], **p;
    int           rc, listadd = 0, ans, i;
    AdrBk        *abook;
    SAVE_STATE_S  state;
    PerAddrBook  *pab;
    AdrBk_Entry  *abe = (AdrBk_Entry *)NULL, *abe_copy;
    adrbk_cntr_t  entry_num = NO_NEXT;
    size_t        tot_size, new_size, old_size;
    Tag           old_tag;
    char         *tmp_a_string;
    char         *simple_a = NULL;
    ADDRESS      *a = NULL;


    dprint(5, (debugfile, "-- take_to_addrbooks --\n"));

    if(tas && *tas)
      pab = (*tas)->pab;
    else
      pab = setup_for_addrbook_add(&state, command_line, cmd);

    /* check we got it opened ok */
    if(pab == NULL || pab->address_book == NULL)
      goto take_to_addrbooks_cancel;

    adrbk_check_validity(pab->address_book, 1L);
    if(pab->address_book->flags & FILE_OUTOFDATE ||
       (pab->address_book->rd &&
	pab->address_book->rd->flags & REM_OUTOFDATE)){
	q_status_message3(SM_ORDER, 0, 4,
      "Address book%.200s%.200s has changed: %.200stry again",
      (as.n_addrbk > 1 && pab->nickname) ? " " : "",
      (as.n_addrbk > 1 && pab->nickname) ? pab->nickname : "",
      (ps_global->remote_abook_validity == -1) ? "resynchronize and " : "");
	if(tas && *tas){
	    restore_state(&((*tas)->state));
	    (*tas)->pab   = NULL;
	}
	else
	  restore_state(&state);

	return;
    }

    abook = pab->address_book;
    new_nickname[0] = '\0';
    new_nickname[MAX_NICKNAME] = '\0';
    exist_nick[0]   = '\0';

    if(addr){
	simple_a = NULL;
	a = NULL;
	/* rfc822_parse_adrlist feels free to destroy input so send copy */
	tmp_a_string = cpystr(addr);
	rfc822_parse_adrlist(&a, tmp_a_string, fakedomain);
	if(tmp_a_string)
	  fs_give((void **)&tmp_a_string);
	
	if(a){
	    simple_a = simple_addr_string(a, tmp_20k_buf, SIZEOF_20KBUF);
	    mail_free_address(&a);
	}

	if(simple_a && *simple_a)
	  abe = adrbk_lookup_by_addr(abook, simple_a, NULL);
	
	if(abe){
	    sprintf(prompt, "Warning: address exists with %s%.50s, continue ",
		    (abe->nickname && abe->nickname[0]) ? "nickname "
		     : (abe->fullname && abe->fullname[0]) ? "fullname "
							   : "no nickname",
		    (abe->nickname && abe->nickname[0]) ? abe->nickname
		     : (abe->fullname && abe->fullname[0]) ? abe->fullname
							   : "");
	    switch(want_to(prompt, 'y', 'x', NO_HELP, WT_NORM)){
	      case 'y':
		if(abe->nickname && abe->nickname[0]){
		    strncpy(new_nickname, abe->nickname, MAX_NICKNAME);
		    strncpy(exist_nick, new_nickname, MAX_NICKNAME);
		    exist_nick[MAX_NICKNAME] = '\0';
		}

		break;
	      
	      default:
		goto take_to_addrbooks_cancel;
	    }
	}
    }

get_nick:
    abe = NULL;
    old_tag = NotSet;
    entry_num = NO_NEXT;

    /*----- nickname ------*/
    sprintf(prompt,
      "Enter new or existing nickname (one word and easy to remember): ");
    if(!new_nickname[0] && nick)
      strncpy(new_nickname, nick, MAX_NICKNAME);

    rc = edit_nickname(abook, (AddrScrn_Disp *)NULL, command_line,
		new_nickname, prompt, h_oe_takenick, 1, 1);
    if(rc == -8){  /* this means an existing nickname was entered */
	abe = adrbk_lookup_by_nick(abook, new_nickname, &entry_num);
	if(!abe){  /* this shouldn't happen */
	    q_status_message1(SM_ORDER, 0, 4,
		"Already an entry %.200s in address book!",
		new_nickname);
	    goto take_to_addrbooks_cancel;
	}

	old_tag = abe->tag;

	if(abe->tag == Single && !strcmp(new_nickname, exist_nick)){
	    static ESCKEY_S choices[] = {
		{'r', 'r', "R", "Replace"},
		{'n', 'n', "N", "No"},
		{-1, 0, NULL, NULL}};

	    sprintf(prompt, "Entry %.50s (%.50s) exists, replace ? ",
		  new_nickname,
		  (abe->fullname && abe->fullname[0])
			? (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						 SIZEOF_20KBUF,
						 abe->fullname, NULL)
			: "<no long name>");
	    ans = radio_buttons(prompt,
				command_line,
				choices,
				'r',
				'x',
				h_oe_take_replace,
				RB_NORM, NULL);
	}
	else{
	    static ESCKEY_S choices[] = {
		{'r', 'r', "R", "Replace"},
		{'a', 'a', "A", "Add"},
		{'n', 'n', "N", "No"},
		{-1, 0, NULL, NULL}};

	    sprintf(prompt,
		  "%s %.50s (%.50s) exists, replace or add addresses to it ? ",
		  abe->tag == List ? "List" : "Entry",
		  new_nickname,
		  (abe->fullname && abe->fullname[0])
			? (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						 SIZEOF_20KBUF,
						 abe->fullname, NULL)
			: "<no long name>");

	    ans = radio_buttons(prompt,
				command_line,
				choices,
				'a',
				'x',
				h_oe_take_replace_or_add,
				RB_NORM, NULL);
	}

	switch(ans){
	  case 'y':
	  case 'r':
	    break;
	  
	  case 'n':
	    goto get_nick;
	    break;
	  
	  case 'a':
	    listadd++;
	    break;

	  default:
	    goto take_to_addrbooks_cancel;
	}
    }
    else if(rc != 0 && rc != -9)  /* -9 means a null nickname */
      goto take_to_addrbooks_cancel;

    if((long)abook->count > MAX_ADRBK_SIZE ||
       (old_tag == NotSet && (long)abook->count >= MAX_ADRBK_SIZE)){
	q_status_message(SM_ORDER, 3, 5,
	    "Address book is at maximum size, cancelled.");
	dprint(2, (debugfile, "Addrbook at Max size, TakeAddr cancelled\n"));
	goto take_to_addrbooks_cancel;
    }

    if(listadd){
	/* count up size of existing list */
	if(abe->tag == List){
	    for(p = abe->addr.list; p != NULL && *p != NULL; p++)
	      ;/* do nothing */
	
	    old_size = p - abe->addr.list;
	}
	/* or size of existing single address */
	else if(abe->addr.addr && abe->addr.addr[0])
	  old_size = 1;
	else
	  old_size = 0;
    }
    else /* don't care about old size, they will be tossed in edit_entry */
      old_size = 0;

    /* make up an abe to pass to edit_entry */
    abe_copy = adrbk_newentry();
    abe_copy->nickname = cpystr(new_nickname);
    abe_copy->tag = List;
    abe_copy->addr.list = NULL;

    if(listadd){
	abe_copy->fullname = cpystr((abe->fullname && abe->fullname[0])
							? abe->fullname : "");
	abe_copy->fcc   = cpystr((abe->fcc && abe->fcc[0]) ? abe->fcc : "");
	abe_copy->extra = cpystr((abe->extra&&abe->extra[0]) ? abe->extra : "");
    }
    else{
	/*
	 * use passed in info if available
	 */
	abe_copy->fullname = cpystr((fullname && fullname[0])
				      ? fullname
				      : (abe && abe->fullname)
				        ? abe->fullname
				        : "");
	abe_copy->fcc      = cpystr((fcc && fcc[0])
				      ? fcc
				      : (abe && abe->fcc)
				        ? abe->fcc
				        : "");
	abe_copy->extra    = cpystr((comment && comment[0])
				      ? comment
				      : (abe && abe->extra)
				        ? abe->extra
				        : "");
    }

    /* get rid of duplicates */
    if(listadd){
	if(abe->tag == List){
	    int      elim_dup;
	    char   **q, **r;
	    ADDRESS *newadr, *oldadr;

	    for(q = new_entries; q != NULL && *q != NULL;){

		tmp_a_string = cpystr(*q);
		newadr = NULL;
		rfc822_parse_adrlist(&newadr, tmp_a_string, fakedomain);
		fs_give((void **)&tmp_a_string);

		elim_dup = (newadr == NULL);
		for(p = abe->addr.list;
		    !elim_dup && p != NULL && *p != NULL;
		    p++){
		    tmp_a_string = cpystr(*p);
		    oldadr = NULL;
		    rfc822_parse_adrlist(&oldadr, tmp_a_string, fakedomain);
		    fs_give((void **)&tmp_a_string);

		    if(address_is_same(newadr, oldadr))
		      elim_dup++;
		    
		    if(oldadr)
		      mail_free_address(&oldadr);
		}

		/* slide the addresses down one to eliminate newadr */
		if(elim_dup){
		    char *f;

		    f = *q;
		    for(r = q; r != NULL && *r != NULL; r++)
		      *r = *(r+1);
		      
		    if(f)
		      fs_give((void **)&f);
		}
		else
		  q++;

		if(newadr)
		  mail_free_address(&newadr);
	    }
	}
	else{
	    char   **q, **r;
	    ADDRESS *newadr, *oldadr;

	    tmp_a_string = cpystr(abe->addr.addr ? abe->addr.addr : "");
	    oldadr = NULL;
	    rfc822_parse_adrlist(&oldadr, tmp_a_string, fakedomain);
	    fs_give((void **)&tmp_a_string);

	    for(q = new_entries; q != NULL && *q != NULL;){

		tmp_a_string = cpystr(*q);
		newadr = NULL;
		rfc822_parse_adrlist(&newadr, tmp_a_string, fakedomain);
		fs_give((void **)&tmp_a_string);

		/* slide the addresses down one to eliminate newadr */
		if(address_is_same(newadr, oldadr)){
		    char *f;

		    f = *q;
		    for(r = q; r != NULL && *r != NULL; r++)
		      *r = *(r+1);
		      
		    if(f)
		      fs_give((void **)&f);
		}
		else
		  q++;

		if(newadr)
		  mail_free_address(&newadr);
	    }

	    if(oldadr)
	      mail_free_address(&oldadr);
	}

	if(!new_entries || !*new_entries){
	    q_status_message1(SM_ORDER, 0, 4,
		     "All of the addresses are already included in \"%.200s\"",
		     new_nickname);
	    free_ae(&abe_copy);
	    if(tas && *tas){
		restore_state(&((*tas)->state));
		(*tas)->pab   = NULL;
	    }
	    else
	      restore_state(&state);

	    return;
	}
    }

    /* count up size of new list */
    for(p = new_entries; p != NULL && *p != NULL; p++)
      ;/* do nothing */
    
    new_size = p - new_entries;
    tot_size = old_size + new_size;
    abe_copy->addr.list = (char **)fs_get((tot_size+1) * sizeof(char *));
    memset((void *)abe_copy->addr.list, 0, (tot_size+1) * sizeof(char *));
    if(old_size > 0){
	if(abe->tag == List){
	    for(i = 0; i < old_size; i++)
	      abe_copy->addr.list[i] = cpystr(abe->addr.list[i]);
	}
	else
	  abe_copy->addr.list[0] = cpystr(abe->addr.addr);
    }

    /* add new addresses to list */
    if(tot_size == 1 && addr)
      abe_copy->addr.list[0] = cpystr(addr);
    else
      for(i = 0; i < new_size; i++)
	abe_copy->addr.list[old_size + i] = cpystr(new_entries[i]);

    abe_copy->addr.list[tot_size] = NULL;

    if(F_ON(F_DISABLE_TAKE_FULLNAMES, ps_global)){
	for(i = 0; abe_copy->addr.list[i]; i++){
	    simple_a = NULL;
	    a = NULL;
	    tmp_a_string = cpystr(abe_copy->addr.list[i]);
	    rfc822_parse_adrlist(&a, tmp_a_string, fakedomain);
	    if(tmp_a_string)
	      fs_give((void **) &tmp_a_string);

	    if(a){
		simple_a = simple_addr_string(a, tmp_20k_buf, SIZEOF_20KBUF);
		mail_free_address(&a);
	    }

	    /* replace the old addr string with one with no full name */
	    if(simple_a && *simple_a){
		if(abe_copy->addr.list[i])
		  fs_give((void **) &abe_copy->addr.list[i]);

		abe_copy->addr.list[i] = cpystr(simple_a);
	    }
	}
    }

    edit_entry(abook, abe_copy, (a_c_arg_t)entry_num, old_tag, 0, NULL, cmd);

    /* free copy */
    free_ae(&abe_copy);
    if(tas && *tas){
	restore_state(&((*tas)->state));
	(*tas)->pab   = NULL;
    }
    else
      restore_state(&state);

    return;

take_to_addrbooks_cancel:
    cancel_warning(NO_DING, "addition");
    if(tas && *tas){
	restore_state(&((*tas)->state));
	(*tas)->pab   = NULL;
    }
    else
      restore_state(&state);
}


/*
 * Prep addrbook for TakeAddr add operation.
 *
 * Arg: savep -- Address of a pointer to save addrbook state in.
 *      stp   -- Address of a pointer to save addrbook state in.
 *
 * Returns: a PerAddrBook pointer, or NULL.
 */
PerAddrBook *
setup_for_addrbook_add(state, command_line, cmd)
    SAVE_STATE_S *state;
    int	          command_line;
    char         *cmd;
{
    PerAddrBook *pab;
    int          save_rem_abook_valid = 0;

    init_ab_if_needed();
    save_state(state);

    if(as.n_addrbk == 0){
        q_status_message(SM_ORDER, 3, 4, "No address book configured!");
        return NULL;
    }
    else
      pab = use_this_addrbook(command_line, cmd);
    
    if(!pab)
      return NULL;

    if((pab->type & REMOTE_VIA_IMAP) && ps_global->remote_abook_validity == -1){
	save_rem_abook_valid = -1;
	ps_global->remote_abook_validity = 0;
    }

    /* initialize addrbook so we can add to it */
    init_abook(pab, Open);

    if(save_rem_abook_valid)
	ps_global->remote_abook_validity = save_rem_abook_valid;

    if(pab->ostatus != Open){
        q_status_message(SM_ORDER, 3, 4, "Can't open address book!");
        return NULL;
    }

    if(pab->access != ReadWrite){
	if(pab->access == ReadOnly)
	  readonly_warning(NO_DING, NULL);
	else if(pab->access == NoAccess)
	  q_status_message(SM_ORDER, 3, 4,
		"AddressBook not accessible, permission denied");

        return NULL;
    }

    return(pab);
}


/*
 * Interact with user to figure out which address book they want to add a
 * new entry (TakeAddr) to.
 *
 * Args: command_line -- just the line to prompt on
 *
 * Results: returns a pab pointing to the selected addrbook, or NULL.
 */
PerAddrBook *
use_this_addrbook(command_line, cmd)
    int   command_line;
    char *cmd;
{
    HelpType   help;
    int        rc = 0;
    PerAddrBook  *pab, *the_only_pab;
#define MAX_ABOOK 1000
    int        i, abook_num, count_read_write;
    char       addrbook[MAX_ABOOK + 1],
               prompt[MAX_ABOOK + 81];
    static ESCKEY_S ekey[] = {
	{-2,   0,   NULL, NULL},
	{ctrl('P'), 10, "^P", "Prev AddrBook"},
	{ctrl('N'), 11, "^N", "Next AddrBook"},
	{KEY_UP,    10, "", ""},
	{KEY_DOWN,  11, "", ""},
	{-1, 0, NULL, NULL}};

    dprint(9, (debugfile, "- use_this_addrbook -\n"));

    /* check for only one ReadWrite addrbook */
    count_read_write = 0;
    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	/*
	 * NoExists is counted, too, so the user can add to an empty
	 * addrbook the first time.
	 */
	if(pab->access == ReadWrite ||
	   pab->access == NoExists ||
	   pab->access == MaybeRorW){
	    count_read_write++;
	    the_only_pab = &as.adrbks[i];
	}
    }

    /* only one usable addrbook, use it */
    if(count_read_write == 1)
      return(the_only_pab);

    /* no addrbook to write to */
    if(count_read_write == 0){
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "No %sAddressbook to %s to!",
			  (as.n_addrbk > 0) ? "writable " : "", cmd);
	return NULL;
    }

    /* start with the first addrbook */
    abook_num = 0;
    pab       = &as.adrbks[abook_num];
    strncpy(addrbook, pab->nickname, sizeof(addrbook)-1);
    addrbook[sizeof(addrbook)-1] = '\0';
    sprintf(prompt, "%c%s to which addrbook : %s",
	islower((unsigned char)(*cmd)) ? toupper((unsigned char)*cmd) : *cmd,
	cmd+1,
	(pab->access == ReadOnly || pab->access == NoAccess) ?
	    "[ReadOnly] " : "");
    help = NO_HELP;
    ps_global->mangled_footer = 1;
    do{
	int flags;

	if(!pab)
          q_status_message1(SM_ORDER, 3, 4, "No addressbook \"%.200s\"",
			    addrbook);

	if(rc == 3)
          help = (help == NO_HELP ? h_oe_chooseabook : NO_HELP);

	flags = OE_APPEND_CURRENT;
	rc = optionally_enter(addrbook, command_line, 0, sizeof(addrbook),
			      prompt, ekey, help, &flags);

	if(rc == 1){ /* ^C */
	    char capcmd[50];

	    sprintf(capcmd,
		"%c%s",
		islower((unsigned char)(*cmd)) ? toupper((unsigned char)*cmd)
					       : *cmd,
		cmd+1);
	    cmd_cancelled(capcmd);
	    break;
	}

	if(rc == 10){			/* Previous addrbook */
	    if(--abook_num < 0)
	      abook_num = as.n_addrbk - 1;

	    pab = &as.adrbks[abook_num];
	    strncpy(addrbook, pab->nickname, sizeof(addrbook)-1);
	    addrbook[sizeof(addrbook)-1] = '\0';
	    sprintf(prompt, "%s to which addrbook : %s", cmd,
		(pab->access == ReadOnly || pab->access == NoAccess) ?
		    "[ReadOnly] " : "");
	}
	else if(rc == 11){			/* Next addrbook */
	    if(++abook_num > as.n_addrbk - 1)
	      abook_num = 0;

	    pab = &as.adrbks[abook_num];
	    strncpy(addrbook, pab->nickname, sizeof(addrbook)-1);
	    addrbook[sizeof(addrbook)-1] = '\0';
	    sprintf(prompt, "%s to which addrbook : %s", cmd,
		(pab->access == ReadOnly || pab->access == NoAccess) ?
		    "[ReadOnly] " : "");
	}

    }while(rc == 2 || rc == 3 || rc == 4 || rc == 10 || rc == 11 || rc == 12 ||
           !(pab = check_for_addrbook(addrbook)));
            
    ps_global->mangled_footer = 1;

    if(rc != 0)
      return NULL;

    return(pab);
}


/*
 * Return a pab pointer to the addrbook which corresponds to the argument.
 * 
 * Args: addrbook -- the string representing the addrbook.
 *
 * Results: returns a PerAddrBook pointer for the referenced addrbook, NULL
 *          if none.  First the nicknames are checked and then the filenames.
 *          This must be one of the existing addrbooks.
 */
PerAddrBook *
check_for_addrbook(addrbook)
    char *addrbook;
{
    register int i;
    register PerAddrBook *pab;

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(strcmp(pab->nickname, addrbook) == 0)
	  break;
    }

    if(i < as.n_addrbk)
      return(pab);

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(strcmp(pab->filename, addrbook) == 0)
	  break;
    }

    if(i < as.n_addrbk)
      return(pab);
    
    return NULL;
}

/*
 * Some common key menu/bindings
 */
#define	TA_EXIT_MENU	{"<","ExitTake", \
			 {MC_EXIT,4,{'e',ctrl('C'),'<',','}}, \
			 KS_EXITMODE}
#define	TA_NEXT_MENU	{"N","Next", \
			 {MC_CHARDOWN,4,{'n','\t',ctrl('N'),KEY_DOWN}}, \
			 KS_NONE}
#define	TA_PREV_MENU	{"P","Prev", \
			 {MC_CHARUP, 3, {'p',ctrl('P'),KEY_UP}}, \
			 KS_NONE}

static struct key ta_keys_lm[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	TA_EXIT_MENU,
	{"T", "Take", {MC_TAKE,1,{'t'}}, KS_NONE},
	TA_PREV_MENU,
	TA_NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X","[Set/Unset]", {MC_CHOICE,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE},
	{"A", "SetAll",{MC_SELALL,1,{'a'}},KS_NONE},
	{"U","UnSetAll",{MC_UNSELALL,1,{'u'}},KS_NONE},
	{"S","SinglMode",{MC_LISTMODE,1,{'s'}},KS_NONE}};
INST_KEY_MENU(ta_keymenu_lm, ta_keys_lm);

static struct key ta_keys_sm[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	TA_EXIT_MENU,
	{"T","[Take]",{MC_TAKE,3,{'t',ctrl('M'),ctrl('J')}}, KS_NONE},
	TA_PREV_MENU,
	TA_NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"L","ListMode",{MC_LISTMODE,1,{'l'}},KS_NONE}};
INST_KEY_MENU(ta_keymenu_sm, ta_keys_sm);


/*
 * Screen for selecting which addresses to Take to address book.
 *
 * Args:      ps -- Pine state
 *       ta_list -- Screen is formed from this list of addresses
 *  how_many_selected -- how many checked initially in ListMode
 *          mode -- which mode to start in
 *
 * Result: an address book may be updated
 * Returns  -- 0 normally
 *             1 if it returns before redrawing screen
 */
int
takeaddr_screen(ps, ta_list, how_many_selected, mode, tas, command)
    struct pine       *ps;
    TA_S              *ta_list;
    int                how_many_selected;
    ScreenMode         mode;
    TA_STATE_S       **tas;
    char              *command;
{
    int		  cmd, dline, give_warn_message, command_line;
    int		  ch = 'x',
		  km_popped = 0,
		  directly_to_take = 0,
		  ret = 0,
		  done = 0;
    TA_S	 *current = NULL,
		 *ctmp = NULL;
    TA_SCREEN_S   screen;
    Pos           cursor_pos;
    struct key_menu *km;

    dprint(2, (debugfile, "- takeaddr_screen -\n"));

    command_line = -FOOTER_ROWS(ps);  /* third line from the bottom */

    screen.current = screen.top_line = NULL;
    screen.mode    = mode;

    if(ta_list == NULL){
	q_status_message1(SM_INFO, 0, 2, "No addresses to %.200s, cancelled",
			  command);
	return 1;
    }

    current	       = first_sel_taline(ta_list);
    ps->mangled_screen = 1;
    ta_screen	       = &screen;

    if(is_talist_of_one(current)){
	directly_to_take++;
	screen.mode = SingleMode; 
    }
    else if(screen.mode == ListMode)
      q_status_message(SM_INFO, 0, 1,
	    "List mode: Use \"X\" to mark addresses to be included in list");
    else
      q_status_message(SM_INFO, 0, 1,
	    "Single mode: Use \"P\" or \"N\" to select desired address");

    while(!done){
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(ps);
		ps->mangled_body = 1;
	    }
	}

	if(screen.mode == ListMode)
	  ps->redrawer = takeaddr_screen_redrawer_list;
	else
	  ps->redrawer = takeaddr_screen_redrawer_single;

	if(ps->mangled_screen){
	    ps->mangled_header = 1;
	    ps->mangled_footer = 1;
	    ps->mangled_body   = 1;
	    ps->mangled_screen = 0;
	}

	/*----------- Check for new mail -----------*/
	if(new_mail(0, NM_TIMING(ch), NM_STATUS_MSG | NM_DEFER_SORT) >= 0)
	  ps->mangled_header = 1;

#ifdef _WINDOWS
	mswin_beginupdate();
#endif
	if(ps->mangled_header){
	    char tbuf[40];

	    sprintf(tbuf, "TAKE ADDRESS SCREEN (%s Mode)",
				    (screen.mode == ListMode) ? "List"
							      : "Single");
            set_titlebar(tbuf, ps->mail_stream, ps->context_current,
                             ps->cur_folder, ps->msgmap, 1, FolderName, 0, 0,
			     NULL);
	    ps->mangled_header = 0;
	}

	dline = update_takeaddr_screen(ps, current, &screen, &cursor_pos);
	if(F_OFF(F_SHOW_CURSOR, ps)){
	    cursor_pos.row = ps->ttyo->screen_rows - FOOTER_ROWS(ps);
	    cursor_pos.col = 0;
	}

	/*---- This displays new mail notification, or errors ---*/
	if(km_popped){
	    FOOTER_ROWS(ps_global) = 3;
	    mark_status_unknown();
	}

        display_message(ch);
	if(km_popped){
	    FOOTER_ROWS(ps_global) = 1;
	    mark_status_unknown();
	}

	/*---- Redraw footer ----*/
	if(ps->mangled_footer){
	    bitmap_t bitmap;

	    if(km_popped){
		FOOTER_ROWS(ps) = 3;
		clearfooter(ps);
	    }

	    setbitmap(bitmap);
	    ps->mangled_footer = 0;

	    km = (screen.mode == ListMode) ? &ta_keymenu_lm : &ta_keymenu_sm;

	    menu_clear_binding(km, KEY_LEFT);
	    menu_clear_binding(km, KEY_RIGHT);
	    if(F_ON(F_ARROW_NAV, ps_global)){
		int cmd;

		if((cmd = menu_clear_binding(km, '<')) != MC_UNKNOWN){
		    menu_add_binding(km, '<', cmd);
		    menu_add_binding(km, KEY_LEFT, cmd);
		}

		if((cmd = menu_clear_binding(km, '>')) != MC_UNKNOWN){
		    menu_add_binding(km, '>', cmd);
		    menu_add_binding(km, KEY_RIGHT, cmd);
		}
	    }

	    draw_keymenu(km, bitmap, ps->ttyo->screen_cols,
			 1 - FOOTER_ROWS(ps_global), 0, FirstMenu);

	    if(km_popped){
		FOOTER_ROWS(ps) = 1;
		mark_keymenu_dirty();
	    }
	}

#ifdef _WINDOWS
	mswin_endupdate();
#endif
        /*------ Read the command from the keyboard ----*/
	MoveCursor(cursor_pos.row, cursor_pos.col);

	if(directly_to_take){  /* bypass this screen */
	    cmd = MC_TAKE;
	    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	}
	else {
#ifdef	MOUSE
	    mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);
	    register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
			   ps_global->ttyo->screen_rows - (FOOTER_ROWS(ps)+1),
			   ps_global->ttyo->screen_cols);
#endif

#ifdef  _WINDOWS
        mswin_setscrollcallback(ta_scroll_callback);
#endif
	    ch = read_command();

#ifdef	MOUSE
	    clear_mfunc(mouse_in_content);
#endif

#ifdef  _WINDOWS
        mswin_setscrollcallback(NULL);
#endif
	    cmd = menu_command(ch, km);
	    if (ta_screen->current)
	      current = ta_screen->current;

	    if(km_popped)
	      switch(cmd){
		case MC_NONE :
		case MC_OTHER :
		case MC_RESIZE :
		case MC_REPAINT :
		  km_popped++;
		  break;

		default:
		  clearfooter(ps);
		  break;
	      }
	}

	switch(cmd){
	  case MC_HELP :			/* help! */
	    if(FOOTER_ROWS(ps_global) == 1 && km_popped == 0){
		km_popped = 2;
		ps_global->mangled_footer = 1;
		break;
	    }

	    helper(h_takeaddr_screen, "HELP FOR TAKE ADDRESS SCREEN",
		   HLPD_SIMPLE);
	    ps->mangled_screen = 1;
	    break;

	  case MC_EXIT:				/* exit takeaddr screen */
	    cancel_warning(NO_DING, "addition");
	    ret = 1;
	    done++;
	    break;

	  case MC_TAKE:
	    if(ta_do_take(current, how_many_selected, command_line, tas,
	       command))
	      done++;
	    else
	      directly_to_take = 0;

	    break;

	  case MC_CHARDOWN :			/* next list element */
	    if(ctmp = next_sel_taline(current))
	      current = ctmp;
	    else
	      q_status_message(SM_INFO, 0, 1, "Already on last line.");

	    break;

	  case MC_CHARUP:			/* previous list element */
	    if(ctmp = pre_sel_taline(current))
	      current = ctmp;
	    else
	      q_status_message(SM_INFO, 0, 1, "Already on first line.");

	    break;

	  case MC_PAGEDN :			/* page forward */
	    give_warn_message = 1;
	    while(dline++ < ps->ttyo->screen_rows - FOOTER_ROWS(ps)){
	        if(ctmp = next_sel_taline(current)){
		    current = ctmp;
		    give_warn_message = 0;
		}
	        else
		    break;
	    }

	    if(give_warn_message)
	      q_status_message(SM_INFO, 0, 1, "Already on last page.");

	    break;

	  case MC_PAGEUP :			/* page backward */
	    /* move to top of screen */
	    give_warn_message = 1;
	    while(dline-- > HEADER_ROWS(ps_global)){
	        if(ctmp = pre_sel_taline(current)){
		    current = ctmp;
		    give_warn_message = 0;
		}
	        else
		    break;
	    }

	    /* page back one screenful */
	    while(++dline < ps->ttyo->screen_rows - FOOTER_ROWS(ps)){
	        if(ctmp = pre_sel_taline(current)){
		    current = ctmp;
		    give_warn_message = 0;
		}
	        else
		    break;
	    }

	    if(give_warn_message)
	      q_status_message(SM_INFO, 0, 1, "Already on first page.");

	    break;

	  case MC_WHEREIS :			/* whereis */
	    if(ctmp = whereis_taline(current))
 	      current = ctmp;
	    
	    ps->mangled_footer = 1;
	    break;

	  case KEY_SCRLTO:
	    /* no op for now */
	    break;

#ifdef MOUSE	    
	  case MC_MOUSE:
	    {
		MOUSEPRESS mp;

		mouse_get_last(NULL, &mp);
		mp.row -= HEADER_ROWS(ps_global);
		ctmp = screen.top_line;
		if(mp.doubleclick){
		    if(screen.mode == SingleMode){
			if(ta_do_take(current, how_many_selected, command_line,
				      tas, command))
			  done++;
			else
			  directly_to_take = 0;
		    }
		    else{
			current->checked = !current->checked;  /* flip it */
			how_many_selected += (current->checked ? 1 : -1);
		    }
		}
		else{
		    while(mp.row && ctmp != NULL){
			--mp.row;
			do  ctmp = ctmp->next;
			while(ctmp != NULL && ctmp->skip_it && !ctmp->print);
		    }

		    if(ctmp != NULL && !ctmp->skip_it)
		      current = ctmp;
		}
	    }
	    break;
#endif

	  case MC_REPAINT :
          case MC_RESIZE :
	    ClearScreen();
	    ps->mangled_screen = 1;
	    break;

	  case MC_CHOICE :			/* [UN]select this addr */
	    current->checked = !current->checked;  /* flip it */
	    how_many_selected += (current->checked ? 1 : -1);
	    break;

	  case MC_SELALL :			/* select all */
	    how_many_selected = ta_mark_all(first_sel_taline(current));
	    ps->mangled_body = 1;
	    break;

	  case MC_UNSELALL:			/* unselect all */
	    how_many_selected = ta_unmark_all(first_sel_taline(current));
	    ps->mangled_body = 1;
	    break;

	  case MC_LISTMODE:			/* switch to SingleMode */
	    if(screen.mode == ListMode){
		screen.mode = SingleMode;
		q_status_message(SM_INFO, 0, 1,
		  "Single mode: Use \"P\" or \"N\" to select desired address");
	    }
	    else{
		screen.mode = ListMode;
		q_status_message(SM_INFO, 0, 1,
	    "List mode: Use \"X\" to mark addresses to be included in list");

		if(how_many_selected <= 1){
		    how_many_selected =
				    ta_unmark_all(first_sel_taline(current));
		    current->checked = 1;
		    how_many_selected++;
		}
	    }

	    ps->mangled_screen = 1;
	    break;


	  case MC_NONE :			/* simple timeout */
	    break;


	    /* Unbound (or not dealt with) keystroke */
	  case MC_CHARRIGHT :
	  case MC_CHARLEFT :
	  case MC_GOTOBOL :
	  case MC_GOTOEOL :
	  case MC_UNKNOWN :
	  default:
	    bogus_command(ch, F_ON(F_USE_FK, ps) ? "F1" : "?");
	}
    }

    /* clean up */
    if(current)
      while(current->prev)
	current = current->prev;

    while(current){
	ctmp = current->next;
	free_taline(&current);
	current = ctmp;
    }

    ps->mangled_screen = 1;

    return(ret);
}


/*
 * Do what takeaddr_screen does except bypass the takeaddr_screen and
 * go directly to do_take.
 */
void
takeaddr_bypass(ps, current, tasp)
    struct pine *ps;
    TA_S        *current;
    TA_STATE_S **tasp;
{
    TA_S       *ctmp;
    TA_SCREEN_S screen;	 /* We have to fake out ta_do_take because */
			 /* we're bypassing takeaddr_screen.       */
    ta_screen = &screen;
    ta_screen->mode = SingleMode;
    current = first_sel_taline(current);
    (void)ta_do_take(current, 1, -FOOTER_ROWS(ps_global), tasp, "save");
    ps->mangled_screen = 1;

    while(current->prev)
      current = current->prev;
    
    while(current){
	ctmp = current->next;
	free_taline(&current);
	current = ctmp;
    }
}


/*
 *
 */
int
ta_do_take(current, how_many_selected, command_line, tas, cmd)
    TA_S	*current;
    int		 how_many_selected;
    int		 command_line;
    TA_STATE_S **tas;
    char        *cmd;
{
    return((ta_screen->mode == ListMode)
	      ? ta_take_marked_addrs(how_many_selected,
				     first_sel_taline(current),
				     command_line, tas, cmd)
	      : ta_take_single_addr(current, command_line, tas, cmd));
}


/*
 * Previous selectable TA line.
 * skips over the elements with skip_it set
 */
TA_S *
pre_sel_taline(current)
    TA_S *current;
{
    TA_S *p;

    if(!current)
      return NULL;

    p = current->prev;
    while(p && p->skip_it)
      p = p->prev;
    
    return(p);
}


/*
 * Previous TA line, selectable or just printable.
 * skips over the elements with skip_it set
 */
TA_S *
pre_taline(current)
    TA_S *current;
{
    TA_S *p;

    if(!current)
      return NULL;

    p = current->prev;
    while(p && (p->skip_it && !p->print))
      p = p->prev;
    
    return(p);
}


/*
 * Next selectable TA line.
 * skips over the elements with skip_it set
 */
TA_S *
next_sel_taline(current)
    TA_S *current;
{
    TA_S *p;

    if(!current)
      return NULL;

    p = current->next;
    while(p && p->skip_it)
      p = p->next;
    
    return(p);
}


/*
 * Next TA line, including print only lines.
 * skips over the elements with skip_it set unless they are print lines
 */
TA_S *
next_taline(current)
    TA_S *current;
{
    TA_S *p;

    if(!current)
      return NULL;

    p = current->next;
    while(p && (p->skip_it && !p->print))
      p = p->next;
    
    return(p);
}


/*
 * WhereIs for TakeAddr screen.
 *
 * Returns the line match is found in or NULL.
 */
TA_S *
whereis_taline(current)
    TA_S *current;
{
    TA_S *p;
    int   rc, found = 0, wrapped = 0, flags;
    char *result = NULL, buf[MAX_SEARCH+1], tmp[MAX_SEARCH+20];
    static char last[MAX_SEARCH+1];
    HelpType help;
    static ESCKEY_S ekey[] = {
	{0, 0, "", ""},
	{ctrl('Y'), 10, "^Y", "Top"},
	{ctrl('V'), 11, "^V", "Bottom"},
	{-1, 0, NULL, NULL}};

    if(!current)
      return NULL;

    /*--- get string  ---*/
    buf[0] = '\0';
    sprintf(tmp, "Word to find %s%.*s%s: ",
	     (last[0]) ? "[" : "",
	     sizeof(tmp)-20, (last[0]) ? last : "",
	     (last[0]) ? "]" : "");
    help = NO_HELP;
    flags = OE_APPEND_CURRENT | OE_KEEP_TRAILING_SPACE;
    while(1){
	rc = optionally_enter(buf,-FOOTER_ROWS(ps_global),0,sizeof(buf),
				 tmp,ekey,help,&flags);
	if(rc == 3)
	  help = help == NO_HELP ? h_config_whereis : NO_HELP;
	else if(rc == 0 || rc == 1 || rc == 10 || rc == 11 || !buf[0]){
	    if(rc == 0 && !buf[0] && last[0]){
		strncpy(buf, last, sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
	    }

	    break;
	}
    }

    if(rc == 0 && buf[0]){
	p = current;
	while(p = next_taline(p))
	  if(srchstr((char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					    SIZEOF_20KBUF,
					    p->strvalue, NULL),
		     buf)){
	      found++;
	      break;
	  }

	if(!found){
	    p = first_taline(current);

	    while(p != current)
	      if(srchstr((char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						SIZEOF_20KBUF,
						p->strvalue, NULL),
			 buf)){
		  found++;
		  wrapped++;
		  break;
	      }
	      else
		p = next_taline(p);
	}
    }
    else if(rc == 10){
	current = first_sel_taline(current);
	result = "Searched to top";
    }
    else if(rc == 11){
	current = last_sel_taline(current);
	result = "Searched to bottom";
    }
    else{
	current = NULL;
	result = "WhereIs cancelled";
    }

    if(found){
	current = p;
	result  = wrapped ? "Search wrapped to beginning" : "Word found";
	strncpy(last, buf, sizeof(last)-1);
	last[sizeof(last)-1] = '\0';
    }

    q_status_message(SM_ORDER,0,3,result ? result : "Word not found");
    return(current);
}


/*
 * Call the addrbook functions which add the checked addresses.
 *
 * Args: how_many_selected -- how many addresses are checked
 *                  f_line -- the first ta line
 *
 * Returns: 1 -- we're done, caller should return
 *          0 -- we're not done
 */
int
ta_take_marked_addrs(how_many_selected, f_line, command_line, tas, cmd)
    int   how_many_selected;
    TA_S *f_line;
    int   command_line;
    TA_STATE_S **tas;
    char *cmd;
{
    char **new_list;
    TA_S  *p;

    if(how_many_selected == 0){
	q_status_message(SM_ORDER, 0, 4,
  "No addresses marked for taking. Use ExitTake to leave TakeAddr screen");
	return 0;
    }

    if(how_many_selected == 1){
	for(p = f_line; p; p = next_sel_taline(p))
	  if(p->checked && !p->skip_it)
	    break;

	if(p)
	  add_abook_entry(p,
			  (p->nickname && p->nickname[0]) ? p->nickname : NULL,
			  (p->fullname && p->fullname[0]) ? p->fullname : NULL,
			  (p->fcc && p->fcc[0]) ? p->fcc : NULL,
			  (p->comment && p->comment[0]) ? p->comment : NULL,
			  command_line, tas, cmd);
    }
    else{
	new_list = list_of_checked(f_line);
	for(p = f_line; p; p = next_sel_taline(p))
	  if(p->checked && !p->skip_it)
	    break;

	take_to_addrbooks_frontend(new_list, p ? p->nickname : NULL,
		p ? p->fullname : NULL, NULL, p ? p->fcc : NULL,
		p ? p->comment : NULL, command_line, tas, cmd);
	free_list_array(&new_list);
    }

    return 1;
}


int
ta_take_single_addr(cur, command_line, tas, cmd)
    TA_S *cur;
    int   command_line;
    TA_STATE_S **tas;
    char *cmd;
{
    add_abook_entry(cur,
		    (cur->nickname && cur->nickname[0]) ? cur->nickname : NULL,
		    (cur->fullname && cur->fullname[0]) ? cur->fullname : NULL,
		    (cur->fcc && cur->fcc[0]) ? cur->fcc : NULL,
		    (cur->comment && cur->comment[0]) ? cur->comment : NULL,
		    command_line, tas, cmd);

    return 1;
}


/*
 * Mark all of the addresses with a check.
 *
 * Args: f_line -- the first ta line
 *
 * Returns the number of lines checked.
 */
int
ta_mark_all(f_line)
    TA_S *f_line;
{
    TA_S *ctmp;
    int   how_many_selected = 0;

    for(ctmp = f_line; ctmp; ctmp = next_sel_taline(ctmp)){
	ctmp->checked = 1;
	how_many_selected++;
    }

    return(how_many_selected);
}


/*
 * Does the takeaddr list consist of a single address?
 *
 * Args: f_line -- the first ta line
 *
 * Returns 1 if only one address and 0 otherwise.
 */
int
is_talist_of_one(f_line)
    TA_S *f_line;
{
    if(f_line == NULL)
      return 0;

    /* there is at least one, see if there are two */
    if(next_sel_taline(f_line) != NULL)
      return 0;
    
    return 1;
}


/*
 * Turn off all of the check marks.
 *
 * Args: f_line -- the first ta line
 *
 * Returns the number of lines checked (0).
 */
int
ta_unmark_all(f_line)
    TA_S *f_line;
{
    TA_S *ctmp;

    for(ctmp = f_line; ctmp; ctmp = ctmp->next)
      ctmp->checked = 0;

    return 0;
}


/*
 * Manage display of the Take Address screen.
 *
 * Args:     ps -- pine state
 *      current -- the current TA line
 *       screen -- the TA screen
 *   cursor_pos -- return good cursor position here
 */
int
update_takeaddr_screen(ps, current, screen, cursor_pos)
    struct pine  *ps;
    TA_S	 *current;
    TA_SCREEN_S  *screen;
    Pos          *cursor_pos;
{
    int	   dline;
    TA_S  *top_line,
          *ctmp;
    int    longest, i, j;
    char   buf1[MAX_SCREEN_COLS + 30];
    char   buf2[MAX_SCREEN_COLS + 30];
    char  *p, *q;
    int screen_width = ps->ttyo->screen_cols;
    Pos    cpos;

    cpos.row = HEADER_ROWS(ps); /* default return value */

    /* calculate top line of display */
    dline = 0;
    top_line = 0;

    if (ta_screen->top_line){
      for(dline = 0, ctmp = ta_screen->top_line; 
	 ctmp && ctmp != current; ctmp = next_taline(ctmp))
	dline++;

      if (ctmp && (dline < ps->ttyo->screen_rows - HEADER_ROWS(ps)
		   - FOOTER_ROWS(ps)))
	top_line = ta_screen->top_line;
    }

    if (!top_line){
      dline = 0;
      ctmp = top_line = first_taline(current);
      do
	if(((dline++) % (ps->ttyo->screen_rows - HEADER_ROWS(ps)
			 - FOOTER_ROWS(ps))) == 0)
	  top_line = ctmp;
      while(ctmp != current && (ctmp = next_taline(ctmp)));
    }

#ifdef _WINDOWS
    /*
     * Figure out how far down the top line is from the top and how many
     * total lines there are.  Dumb to loop every time thru, but
     * there aren't that many lines, and it's cheaper than rewriting things
     * to maintain a line count in each structure...
     */
    for(dline = 0, ctmp = pre_taline(top_line); ctmp; ctmp = pre_taline(ctmp))
      dline++;

    scroll_setpos(dline);

    for(ctmp = next_taline(top_line); ctmp ; ctmp = next_taline(ctmp))
      dline++;

    scroll_setrange(ps->ttyo->screen_rows - FOOTER_ROWS(ps) - HEADER_ROWS(ps),
		   dline);
#endif


    /* mangled body or new page, force redraw */
    if(ps->mangled_body || screen->top_line != top_line)
      screen->current = NULL;
    
    /* find length of longest line for nicer formatting */
    longest = 0;
    for(ctmp = first_taline(top_line); ctmp; ctmp = next_taline(ctmp)){
	int len;

	if(ctmp
	   && !ctmp->print
	   && ctmp->strvalue
	   && longest < (len
		   =strlen((char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						  SIZEOF_20KBUF,
						  ctmp->strvalue, NULL))))
	  longest = len;
    }

#define LENGTH_OF_THAT_STRING 5   /* "[X]  " */
    longest = min(longest, ps->ttyo->screen_cols);

    /* loop thru painting what's needed */
    for(dline = 0, ctmp = top_line;
	dline < ps->ttyo->screen_rows - FOOTER_ROWS(ps) - HEADER_ROWS(ps);
	dline++, ctmp = next_taline(ctmp)){

	/*
	 * only fall thru painting if something needs painting...
	 */
	if(!ctmp || !screen->current || ctmp == screen->current ||
				   ctmp == top_line || ctmp == current){
	    ClearLine(dline + HEADER_ROWS(ps));
	    if(!ctmp || !ctmp->strvalue)
	      continue;
	}

	p = buf1;
	if(ctmp == current){
	    cpos.row = dline + HEADER_ROWS(ps);  /* col set below */
	    StartInverse();
	}

	if(ctmp->print)
	  j = 0;
	else
	  j = LENGTH_OF_THAT_STRING;

	/*
	 * Copy the value to a temp buffer expanding tabs, and
	 * making sure not to write beyond screen right...
	 */
	q = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				   SIZEOF_20KBUF, ctmp->strvalue, NULL);

	for(i = 0; q[i] && j < ps->ttyo->screen_cols; i++){
	    if(q[i] == ctrl('I')){
		do
		  *p++ = SPACE;
		while(j < ps->ttyo->screen_cols && ((++j)&0x07));
		      
	    }
	    else{
		*p++ = q[i];
		j++;
	    }
	}
	for(; i < longest; i++)
	  *p++ = ' ';

	*p = '\0';

	/* mark lines which have check marks */
	if(ctmp == current){
	    if(screen->mode == ListMode){
	        sprintf(buf2, "[%c]  %-*.*s",
		    ctmp->checked ? 'X' : SPACE,
		    longest, longest, buf1);
		cpos.col = 1; /* position on the X */
	    }
	    else{
		sprintf(buf2, "     %-*.*s",
		    longest, longest, buf1);
		cpos.col = 5; /* 5 spaces before text */
	    }
	}
	else{
	    if(ctmp->print){
		int len;
		char *start_printing;

		memset((void *)buf2, '-', screen_width * sizeof(char));
		buf2[screen_width] = '\0';
		len = strlen(buf1);
		if(len > screen_width){
		    len = screen_width;
		    buf1[len] = '\0';
		}

		start_printing = buf2 + (screen_width - len) / 2;
		sprintf(start_printing, "%.*s",
			sizeof(buf2)-(start_printing-buf2)-1, buf1);
		if(len < screen_width)
		  start_printing[strlen(start_printing)] = '-';
	    }
	    else{
		if(screen->mode == ListMode)
	          sprintf(buf2, "[%c]  %.*s", ctmp->checked ? 'X' : SPACE,
			  sizeof(buf2)-6, buf1);
		else
	          sprintf(buf2, "     %.*s", sizeof(buf2)-6, buf1);
	    }
	}

	PutLine0(dline + HEADER_ROWS(ps), 0, buf2);

	if(ctmp == current)
	  EndInverse();
    }

    ps->mangled_body = 0;
    screen->top_line = top_line;
    screen->current  = current;
    if(cursor_pos)
      *cursor_pos = cpos;

    return(cpos.row);
}


void
takeaddr_screen_redrawer_list()
{
    ps_global->mangled_body = 1;
    (void)update_takeaddr_screen(ps_global, ta_screen->current, ta_screen,
	(Pos *)NULL);
}


void
takeaddr_screen_redrawer_single()
{
    ps_global->mangled_body = 1;
    (void)update_takeaddr_screen(ps_global, ta_screen->current, ta_screen,
	(Pos *)NULL);
}


/*
 * new_taline - create new TA_S, zero it out, and insert it after current.
 *                NOTE current gets set to the new TA_S, too!
 */
TA_S *
new_taline(current)
    TA_S **current;
{
    TA_S *p;

    p = (TA_S *)fs_get(sizeof(TA_S));
    memset((void *)p, 0, sizeof(TA_S));
    if(current){
	if(*current){
	    p->next	     = (*current)->next;
	    (*current)->next = p;
	    p->prev	     = *current;
	    if(p->next)
	      p->next->prev = p;
	}

	*current = p;
    }

    return(p);
}


void
free_taline(p)
    TA_S **p;
{
    if(p){
	if((*p)->addr)
	  mail_free_address(&(*p)->addr);

	if((*p)->strvalue)
	  fs_give((void **)&(*p)->strvalue);

	if((*p)->nickname)
	  fs_give((void **)&(*p)->nickname);

	if((*p)->fullname)
	  fs_give((void **)&(*p)->fullname);

	if((*p)->fcc)
	  fs_give((void **)&(*p)->fcc);

	if((*p)->comment)
	  fs_give((void **)&(*p)->comment);

	if((*p)->prev)
	  (*p)->prev->next = (*p)->next;

	if((*p)->next)
	  (*p)->next->prev = (*p)->prev;

	fs_give((void **)p);
    }
}


void
free_ltline(p)
    LINES_TO_TAKE **p;
{
    if(p){
	if((*p)->printval)
	  fs_give((void **)&(*p)->printval);

	if((*p)->exportval)
	  fs_give((void **)&(*p)->exportval);

	if((*p)->prev)
	  (*p)->prev->next = (*p)->next;

	if((*p)->next)
	  (*p)->next->prev = (*p)->prev;

	fs_give((void **)p);
    }
}


/*
 * Return the first selectable TakeAddr line.
 *
 * Args: q -- any line in the list
 */
TA_S *
first_sel_taline(q)
    TA_S *q;
{
    TA_S *first;

    if(q == NULL)
      return NULL;

    first = NULL;
    /* back up to the head of the list */
    while(q){
	first = q;
	q = pre_sel_taline(q);
    }

    /*
     * If first->skip_it, that means we were already past the first
     * legitimate line, so we have to look in the other direction.
     */
    if(first->skip_it)
      first = next_sel_taline(first);
    
    return(first);
}


/*
 * Return the last selectable TakeAddr line.
 *
 * Args: q -- any line in the list
 */
TA_S *
last_sel_taline(q)
    TA_S *q;
{
    TA_S *last;

    if(q == NULL)
      return NULL;

    last = NULL;
    /* go to the end of the list */
    while(q){
	last = q;
	q = next_sel_taline(q);
    }

    /*
     * If last->skip_it, that means we were already past the last
     * legitimate line, so we have to look in the other direction.
     */
    if(last->skip_it)
      last = pre_sel_taline(last);
    
    return(last);
}


/*
 * Return the first TakeAddr line, selectable or just printable.
 *
 * Args: q -- any line in the list
 */
TA_S *
first_taline(q)
    TA_S *q;
{
    TA_S *first;

    if(q == NULL)
      return NULL;

    first = NULL;
    /* back up to the head of the list */
    while(q){
	first = q;
	q = pre_taline(q);
    }

    /*
     * If first->skip_it, that means we were already past the first
     * legitimate line, so we have to look in the other direction.
     */
    if(first->skip_it && !first->print)
      first = next_taline(first);
    
    return(first);
}


/*
 * Find the first TakeAddr line which is checked, beginning with the
 * passed in line.
 *
 * Args: head -- A passed in TakeAddr line, usually will be the first
 *
 * Result: returns a pointer to the first checked line.
 *         NULL if no such line
 */
TA_S *
first_checked(head)
    TA_S *head;
{
    TA_S *p;

    p = head;

    for(p = head; p; p = next_sel_taline(p))
      if(p->checked && !p->skip_it)
	break;

    return(p);
}


/*
 * Form a list of strings which are addresses to go in a list.
 * These are entries in a list, so can be full rfc822 addresses.
 * The strings are allocated here.
 *
 * Args: head -- A passed in TakeAddr line, usually will be the first
 *
 * Result: returns an allocated array of pointers to allocated strings
 */
char **
list_of_checked(head)
    TA_S *head;
{
    TA_S  *p;
    int    count;
    char **list, **cur;
    ADDRESS *a;

    /* first count them */
    for(p = head, count = 0; p; p = next_sel_taline(p)){
	if(p->checked && !p->skip_it){
	    if(p->frwrded){
		/*
		 * Remove fullname, fcc, comment, and nickname since not
		 * appropriate for list values.
		 */
		if(p->fullname)
		  fs_give((void **)&p->fullname);
		if(p->fcc)
		  fs_give((void **)&p->fcc);
		if(p->comment)
		  fs_give((void **)&p->comment);
		if(p->nickname)
		  fs_give((void **)&p->nickname);
	      
		for(a = p->addr; a; a = a->next)
		  count++;
	    }
	    else{
		/*
		 * Don't even attempt to include bogus addresses in
		 * the list.  If the user wants to get at those, they
		 * have to try taking only that single address.
		 */
		if(p->addr && p->addr->host && p->addr->host[0] == '.')
		  p->skip_it = 1;
		else
		  count++;
	    }
	}
    }
    
    /* allocate pointers */
    list = (char **)fs_get((count + 1) * sizeof(char *));
    memset((void *)list, 0, (count + 1) * sizeof(char *));

    cur = list;

    /* allocate and point to address strings */
    for(p = head; p; p = next_sel_taline(p)){
	if(p->checked && !p->skip_it){
	    if(p->frwrded)
	      for(a = p->addr; a; a = a->next){
		  ADDRESS *next_addr;
		  char    *bufp;

		  next_addr = a->next;
		  a->next = NULL;
		  bufp = (char *)fs_get((size_t)est_size(a));
		  *cur++ = cpystr(addr_string(a, bufp));
		  a->next = next_addr;
		  fs_give((void **)&bufp);
	      }
	    else
	      *cur++ = cpystr(p->strvalue);
	}
    }

    return(list);
}


/*
 * Execute command to take addresses out of message and put in the address book
 *
 * Args: ps     -- pine state
 *       msgmap -- the MessageMap
 *       agg    -- this is aggregate operation if set
 *
 * Result: The entry is added to an address book.
 */     
void
cmd_take_addr(ps, msgmap, agg)
    struct pine *ps;
    MSGNO_S     *msgmap;
    int          agg;
{
    long      i;
    ENVELOPE *env;
    int       how_many_selected,
	      added, rtype,
	      we_cancel,
	      special_processing = 0,
	      free_talines = 1,
	      select_froms = 0;
    TA_S     *current,
	     *prev_comment_line,
	     *ctmp,
	     *ta;
    BODY     **body_h,
	      *special_body = NULL,
	      *body = NULL;

    dprint(2, (debugfile, "\n - taking address into address book - \n"));

    if(agg && !pseudo_selected(msgmap))
      return;

    if(mn_get_total(msgmap) > 0 && mn_total_cur(msgmap) == 1)
      special_processing = 1;

    if(agg)
      select_froms = 1;
    
    /* Ask user what kind of Take they want to do */
    if(!agg && F_ON(F_ENABLE_ROLE_TAKE, ps)){

	rtype = rule_setup_type(ps,
				RS_RULES |
				  ((mn_get_total(msgmap) > 0)
				    ? (F_ON(F_ENABLE_TAKE_EXPORT, ps)
				       ? (RS_INCADDR | RS_INCEXP)
				       : RS_INCADDR)
				    : RS_NONE),
				"Take to : ");

	switch(rtype){
	  case 'a':
	  case 'e':
	  case 'Z':
	    break;

	  default:
	    role_take(ps, msgmap, rtype);
	    return;
	}
    }
    else if(F_ON(F_ENABLE_TAKE_EXPORT, ps) && mn_get_total(msgmap) > 0)
      rtype = rule_setup_type(ps, RS_INCADDR | RS_INCEXP, "Take to : ");
    else
      rtype = 'a';

    if(rtype == 'x' || rtype == 'Z'){
	if(rtype == 'x')
	  cmd_cancelled(NULL);
	else if(rtype == 'Z')
	  q_status_message(SM_ORDER | SM_DING, 3, 5,
			  "Try turning on color with the Setup/Kolor command.");
	if(agg)
	  restore_selected(msgmap);

	return;
    }

    ps->mangled_footer = 1;
    how_many_selected = 0;
    current = NULL;

    /* this is a non-selectable label */
    current = fill_in_ta(&current, (ADDRESS *)NULL, 0,
	       " These entries are taken from the attachments ");
    prev_comment_line = current;

    /*
     * Add addresses from special attachments for each message.
     */
    added = 0;
    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){
	env = pine_mail_fetchstructure(ps->mail_stream, mn_m2raw(msgmap, i),
				       &body);
	if(!env){
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "Can't take address into address book. Error accessing folder");
	    goto doneskee;
	}

	added += process_vcard_atts(ps->mail_stream, mn_m2raw(msgmap, i),
				    body, body, NULL, &current);
    }

    if(!agg && added > 1 && rtype == 'a'){
	char prompt[200];
	int            command_line = -FOOTER_ROWS(ps);

	sprintf(prompt,
		"Take %d entries from attachment to addrbook all at once ",
		added);
	switch(want_to(prompt, 'n', 'x', NO_HELP, WT_NORM)){
	  case 'y':
	    (void)take_without_edit(current, added, command_line, NULL, "take");
	    goto doneskee;
	  
	  case 'x':
	    cmd_cancelled("Take");
	    goto doneskee;
	  
	  default:
	    break;
	}
    }

    we_cancel = busy_alarm(1, NULL, NULL, 0);

    /*
     * add a comment line to separate attachment address lines
     * from header address lines
     */
    if(added > 0){
	current = fill_in_ta(&current, (ADDRESS *)NULL, 0,
	       " These entries are taken from the msg headers ");
	prev_comment_line = current;
	how_many_selected += added;
	select_froms = 0;
    }
    else{  /* turn off header comment, and no separator comment */
	prev_comment_line->print = 0;
	prev_comment_line        = NULL;
    }

    /*
     * Add addresses from headers of messages.
     */
    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){

	if(special_processing)
	  body_h = &special_body;
	else
	  body_h = NULL;

	env = pine_mail_fetchstructure(ps->mail_stream, mn_m2raw(msgmap, i),
				       body_h);
	if(!env){
	    if(we_cancel)
	      cancel_busy_alarm(-1);

	    q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "Can't take address into address book. Error accessing folder");
	    goto doneskee;
	}

	added = add_addresses_to_talist(ps, i, "from", &current,
					env->from, select_froms);
	if(select_froms)
	  how_many_selected += added;

	if(!address_is_same(env->from, env->reply_to))
	  (void)add_addresses_to_talist(ps, i, "reply-to", &current,
					env->reply_to, 0);

	if(!address_is_same(env->from, env->sender))
	  (void)add_addresses_to_talist(ps, i, "sender", &current,
					env->sender, 0);

	(void)add_addresses_to_talist(ps, i, "to", &current, env->to, 0);
	(void)add_addresses_to_talist(ps, i, "cc", &current, env->cc, 0);
	(void)add_addresses_to_talist(ps, i, "bcc", &current, env->bcc, 0);
    }
	
    /*
     * Check to see if we added an explanatory line about the
     * header addresses but no header addresses.  If so, remove the
     * explanatory line.
     */
    if(prev_comment_line){
	how_many_selected -=
	    eliminate_dups_and_us(first_sel_taline(current));
	for(ta = prev_comment_line->next; ta; ta = ta->next)
	  if(!ta->skip_it)
	    break;

	/* all entries were skip_it entries, turn off print */
	if(!ta){
	    prev_comment_line->print = 0;
	    prev_comment_line        = NULL;
	}
    }

    /*
     * If there's only one message we let the user view it using ^T
     * from the header editor.
     */
    if(special_processing && env && special_body){
	msgno_for_pico_callback = mn_m2raw(msgmap, mn_first_cur(msgmap));
	body_for_pico_callback  = special_body;
	env_for_pico_callback   = env;
    }
    else{
	env_for_pico_callback   = NULL;
	body_for_pico_callback  = NULL;
    }

    current = fill_in_ta(&current, (ADDRESS *)NULL, 0,
     " Below this line are some possibilities taken from the text of the msg ");
    prev_comment_line = current;

    /*
     * Add addresses from the text of the body.
     */
    added = 0;
    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){

	body = NULL;
	env = pine_mail_fetchstructure(ps->mail_stream, mn_m2raw(msgmap, i),
				       &body);
	if(env && body)
	  added += grab_addrs_from_body(ps->mail_stream,
				        mn_m2raw(msgmap, i),
				        body, &current);
    }

    if(added == 0){
	prev_comment_line->print = 0;
	prev_comment_line        = NULL;
    }

    /*
     * Check to see if all we added are dups.
     * If so, remove the comment line.
     */
    if(prev_comment_line){
	how_many_selected -= eliminate_dups_and_us(first_sel_taline(current));
	for(ta = prev_comment_line->next; ta; ta = ta->next)
	  if(!ta->skip_it)
	    break;

	/* all entries were skip_it entries, turn off print */
	if(!ta)
	  prev_comment_line->print = 0;
    }

    if(we_cancel)
      cancel_busy_alarm(-1);

    if(rtype == 'e'){
	LINES_TO_TAKE *lines_to_take = NULL, *lt;

	added = convert_ta_to_lines(current, &lines_to_take);

	if(added){
	    while(lines_to_take && lines_to_take->prev)
	      lines_to_take = lines_to_take->prev;

	    take_to_export(ps, lines_to_take);
	}
	else
	  q_status_message(SM_ORDER, 3, 4, "Can't find anything to export");

	while(lines_to_take){
	    lt = lines_to_take->next;
	    free_ltline(&lines_to_take);
	    lines_to_take = lt;
	}
    }
    else{
	(void)takeaddr_screen(ps, current, how_many_selected,
			      agg ? ListMode : SingleMode, NULL, "take");
	
	free_talines = 0;
    }

doneskee:
    env_for_pico_callback   = NULL;
    body_for_pico_callback  = NULL;
    
    if(free_talines){
	/* clean up */
	if(current)
	  while(current->prev)
	    current = current->prev;

	while(current){
	    ctmp = current->next;
	    free_taline(&current);
	    current = ctmp;
	}
    }

    ps->mangled_screen = 1;

    if(agg)
      restore_selected(msgmap);
}


/*
 * Special case interface to allow a more interactive Save in the case where
 * the user seems to be wanting to save an exact copy of an existing entry.
 * For example, they might be trying to save a copy of a list with the intention
 * of changing it a little bit. The regular save doesn't allow this, since no
 * editing takes place, but this version plops them into the address book
 * editor.
 */
void
take_this_one_entry(ps, tasp, abook, cur_line)
    struct pine *ps;
    TA_STATE_S **tasp;
    AdrBk       *abook;
    long         cur_line;
{
    AdrBk_Entry *abe;
    AddrScrn_Disp *dl;
    char        *fcc = NULL, *comment = NULL, *fullname = NULL,
		*nickname = NULL;
    ADDRESS     *addr;
    int          how_many_selected;
    TA_S        *current = NULL;

    dl = dlist(cur_line);
    abe = ae(cur_line);
    if(!abe){
	q_status_message(SM_ORDER, 0, 4, "Nothing to save, cancelled");
	return;
    }

    if(dl->type == ListHead || dl->type == Simple){
	fcc      = (abe->fcc && abe->fcc[0]) ? abe->fcc : NULL;
	comment  = (abe->extra && abe->extra[0]) ? abe->extra : NULL;
	fullname = (abe->fullname && abe->fullname[0]) ? abe->fullname : NULL;
	nickname = (abe->nickname && abe->nickname[0]) ? abe->nickname : NULL;
    }

    addr = abe_to_address(abe, dl, abook, &how_many_selected);
    if(!addr){
	addr = mail_newaddr();
	addr->host = cpystr("");
	addr->mailbox = cpystr("");
    }

    switch(abe->tag){
      case Single:
#ifdef	ENABLE_LDAP
	/*
	 * Special case. When user is saving an entry with a runtime
	 * ldap lookup address, they may be doing it because the lookup
	 * has become stale. Give them a way to get the old address out
	 * of the lookup entry so they can save that, instead.
	 */
	if(!addr->personal && !strncmp(addr->mailbox, RUN_LDAP, LEN_RL)){
	    LDAP_SERV_S *info = NULL;
	    int i = 'l';
	    static ESCKEY_S backup_or_ldap[] = {
		{'b', 'b', "B", "Backup"},
		{'l', 'l', "L", "LDAP"},
		{-1, 0, NULL, NULL}};

	    info = break_up_ldap_server(addr->mailbox + LEN_RL);
	    if(info && info->mail && *info->mail)
	      i = radio_buttons("Copy backup address or retain LDAP search criteria ? ",
				-FOOTER_ROWS(ps_global), backup_or_ldap,
				'b', 'x',
				h_ab_backup_or_ldap, RB_NORM, NULL);

	    if(i == 'b'){
		ADDRESS *a = NULL;

		rfc822_parse_adrlist(&a, info->mail, fakedomain);

		if(a){
		    if(addr->mailbox)
		      fs_give((void **)&addr->mailbox);
		    if(addr->host)
		      fs_give((void **)&addr->host);

		    addr->mailbox = a->mailbox;
		    a->mailbox = NULL;
		    addr->host = a->host;
		    a->host = NULL;
		    mail_free_address(&a);
		}
	    }

	    if(info)
	      free_ldap_server_info(&info);
	}
#endif	/* ENABLE_LDAP */
	current = fill_in_ta(&current, addr, 0, (char *)NULL);
	break;

      case List:
	/*
	 * The empty string for the last argument is significant. Fill_in_ta
	 * copies the whole adrlist into a single TA if there is a print
	 * string.
	 */
	if(dl->type == ListHead)
	  current = fill_in_ta(&current, addr, 1, "");
	else
	  current = fill_in_ta(&current, addr, 0, (char *)NULL);

	break;
      
      default:
	dprint(1, (debugfile,
		"default case in take_this_one_entry, shouldn't happen\n"));
	return;
    } 

    if(current->strvalue && !strcmp(current->strvalue, "@")){
	fs_give((void **)&current->strvalue);
	if(fullname && fullname[0])
	  current->strvalue = cpystr(fullname);
	else if(nickname && nickname[0])
	  current->strvalue = cpystr(nickname);
	else
	  current->strvalue = cpystr("?");
    }

    if(addr)
      mail_free_address(&addr);

    if(current){
	current = first_sel_taline(current);
	if(fullname && *fullname)
	  current->fullname = cpystr(fullname);

	if(fcc && *fcc)
	  current->fcc = cpystr(fcc);

	if(comment && *comment)
	  current->comment = cpystr(comment);

	if(nickname && *nickname)
	  current->nickname = cpystr(nickname);

	takeaddr_bypass(ps, current, tasp);
    }
    else
      q_status_message(SM_ORDER, 0, 4, "Nothing to save, cancelled");
}


int
convert_ta_to_lines(ta_list, old_current)
    TA_S           *ta_list;
    LINES_TO_TAKE **old_current;
{
    ADDRESS       *a;
    TA_S          *ta;
    LINES_TO_TAKE *new_current;
    char          *exportval, *printval;
    int            ret = 0;

    for(ta = first_sel_taline(ta_list);
	ta;
	ta = next_sel_taline(ta)){
	if(ta->skip_it)
	  continue;
	
	if(ta->frwrded){
	    if(ta->fullname)
	      fs_give((void **)&ta->fullname);
	    if(ta->fcc)
	      fs_give((void **)&ta->fcc);
	    if(ta->comment)
	      fs_give((void **)&ta->comment);
	    if(ta->nickname)
	      fs_give((void **)&ta->nickname);
	}
	else if(ta->addr && ta->addr->host && ta->addr->host[0] == '.')
	  ta->skip_it = 1;

	if(ta->frwrded){
	    for(a = ta->addr; a; a = a->next){
		ADDRESS *next_addr;

		if(a->host && a->host[0] == '.')
		  continue;

		next_addr = a->next;
		a->next = NULL;

		exportval = cpystr(simple_addr_string(a, tmp_20k_buf,
						      SIZEOF_20KBUF));
		if(!exportval || !exportval[0]){
		    if(exportval)
		      fs_give((void **)&exportval);
		    
		    a->next = next_addr;
		    continue;
		}

		printval = addr_list_string(a, NULL, 0, 0);
		if(!printval || !printval[0]){
		    if(printval)
		      fs_give((void **)&printval);
		    
		    printval = cpystr(exportval);
		}
	    
		new_current = new_ltline(old_current);
		new_current->flags = LT_NONE;

		new_current->printval = printval;
		new_current->exportval = exportval;
		a->next = next_addr;
		ret++;
	    }
	}
	else{
	    if(ta->addr){
		exportval = cpystr(simple_addr_string(ta->addr, tmp_20k_buf,
						      SIZEOF_20KBUF));
		if(exportval && exportval[0]){
		    new_current = new_ltline(old_current);
		    new_current->flags = LT_NONE;

		    new_current->exportval = exportval;

		    if(ta->strvalue && ta->strvalue[0])
		      new_current->printval = cpystr(ta->strvalue);
		    else
		      new_current->printval = cpystr(new_current->exportval);

		    ret++;
		}
		else if(exportval)
		  fs_give((void **)&exportval);
	    }
	}
    }

    return(ret);
}


/*
 * new_ltline - create new LINES_TO_TAKE, zero it out,
 *                and insert it after current.
 *                NOTE current gets set to the new current.
 */
LINES_TO_TAKE *
new_ltline(current)
    LINES_TO_TAKE **current;
{
    LINES_TO_TAKE *p;

    p = (LINES_TO_TAKE *)fs_get(sizeof(LINES_TO_TAKE));
    memset((void *)p, 0, sizeof(LINES_TO_TAKE));
    if(current){
	if(*current){
	    p->next	     = (*current)->next;
	    (*current)->next = p;
	    p->prev	     = *current;
	    if(p->next)
	      p->next->prev = p;
	}

	*current = p;
    }

    return(p);
}


/*
 * Execute command to save addresses out of vcard attachment.
 */
void
save_vcard_att(ps, qline, msgno, a)
    struct pine *ps;
    int          qline;
    long         msgno;
    ATTACH_S    *a;
{
    int       how_many_selected,
	      j,
	      free_talines = 1;
    TA_S     *current,
	     *ctmp;
    TA_STATE_S tas, *tasp;


    dprint(2, (debugfile, "\n - saving vcard attachment - \n"));

    j = radio_buttons("Save to address book or Export to filesystem ? ",
		      qline, save_or_export, 's', 'x',
		      h_ab_save_exp, RB_NORM|RB_SEQ_SENSITIVE, NULL);
    
    switch(j){
      case 'x':
	cancel_warning(NO_DING, "save");
	return;

      case 'e':
	export_vcard_att(ps, qline, msgno, a);
	return;

      case 's':
	break;

      default:
        q_status_message(SM_ORDER, 3, 3, "can't happen in save_vcard_att");
	return;
    }

    dprint(2, (debugfile, "\n - saving attachment into address book - \n"));
    ps->mangled_footer = 1;
    current = NULL;
    how_many_selected = process_vcard_atts(ps->mail_stream, msgno, NULL,
					   a->body, a->number, &current);
    if(how_many_selected > 0){
	tas.pab = NULL;
	tasp    = &tas;
	if(how_many_selected == 1){
	    takeaddr_bypass(ps, current, NULL);
	    free_talines = 0;
	}
	else if(take_without_edit(current, how_many_selected, qline,
			     &tasp, "save") == 'T'){
	    /*
	     * Eliminate dups.
	     */
	    how_many_selected -=
			eliminate_dups_but_not_us(first_sel_taline(current));

	    (void)takeaddr_screen(ps, current, how_many_selected, SingleMode,
				  &tasp, "save");
	    
	    /*
	     * If takeaddr_screen or its children didn't do this for us,
	     * we do it here.
	     */
	    if(tas.pab)
	      restore_state(&(tas.state));

	    free_talines = 0;
	}
    }
    else if(how_many_selected == 0)
      q_status_message(SM_ORDER, 0, 3,
		       "Save cancelled: no entries in attachment");

    if(free_talines){
	/* clean up */
	if(current)
	  while(current->prev)
	    current = current->prev;

	while(current){
	    ctmp = current->next;
	    free_taline(&current);
	    current = ctmp;
	}
    }
}


/*
 * Execute command to export vcard attachment.
 */
void
export_vcard_att(ps, qline, msgno, a)
    struct pine *ps;
    int          qline;
    long         msgno;
    ATTACH_S    *a;
{
    int       how_many_selected, i;
    TA_S     *current;
    STORE_S  *srcstore = NULL;
    SourceType srctype;
    static ESCKEY_S vcard_or_addresses[] = {
	{'a', 'a', "A", "Address List"},
	{'v', 'v', "V", "VCard"},
	{-1, 0, NULL, NULL}};

    if(ps->restricted){
	q_status_message(SM_ORDER, 0, 3,
	    "Pine demo can't export addresses to files");
	return;
    }

    dprint(2, (debugfile, "\n - exporting vcard attachment - \n"));

    i = radio_buttons("Export list of addresses or vCard text ? ",
		      qline, vcard_or_addresses, 'a', 'x',
		      h_ab_export_vcard, RB_NORM|RB_SEQ_SENSITIVE, NULL);

    switch(i){
      case 'x':
	cancel_warning(NO_DING, "export");
	return;
    
      case 'a':
	break;

      case 'v':
	write_attachment(qline, msgno, a, "EXPORT");
	return;

      default:
	q_status_message(SM_ORDER, 3, 3, "can't happen in export_vcard_att");
	return;
    }

    ps->mangled_footer = 1;
    current = NULL;
    how_many_selected = process_vcard_atts(ps->mail_stream, msgno, NULL,
					   a->body, a->number, &current);
    /*
     * Run through all of the list and run through
     * the addresses in each ta->addr, writing them into a storage object.
     * Then export to filesystem.
     */
    srctype = CharStar;
    if(how_many_selected > 0 &&
       (srcstore = so_get(srctype, NULL, EDIT_ACCESS)) != NULL){
	ADDRESS *aa, *bb;
	int are_some = 0;

	for(current = first_taline(current);
	    current;
	    current = next_taline(current)){

	    for(aa = current->addr; aa; aa = aa->next){
		bb = aa->next;
		aa->next = NULL;
		so_puts(srcstore, addr_list_string(aa, NULL, 0, 0));
		are_some++;
		so_puts(srcstore, "\n");
		aa->next = bb;
	    }
	}

	if(are_some)
	  simple_export(ps, so_text(srcstore), srctype, "addresses", NULL);
	else
	  q_status_message(SM_ORDER, 0, 3, "No addresses to export");
	  
	so_give(&srcstore);
    }
    else{
	if(how_many_selected == 0)
	  q_status_message(SM_ORDER, 0, 3, "Nothing to export");
	else
	  q_status_message(SM_ORDER,0,2, "Error allocating space");
    }
}


int
add_addresses_to_talist(ps, msgno, field, old_current, adrlist, checked)
    struct pine *ps;
    long         msgno;
    char        *field;
    TA_S       **old_current;
    ADDRESS     *adrlist;
    int          checked;
{
    ADDRESS *addr;
    int      added = 0;

    for(addr = adrlist; addr; addr = addr->next){
	if(addr->host && addr->host[0] == '.'){
	    char *h, *fields[2];

	    fields[0] = field;
	    fields[1] = NULL;
	    if(h = pine_fetchheader_lines(ps->mail_stream, msgno,NULL,fields)){
		char *p, fname[32];

		sprintf(fname, "%s:", field);
		for(p = h; p = strstr(p, fname); )
		  rplstr(p, strlen(fname), "");   /* strip field strings */
		
		sqznewlines(h);                   /* blat out CR's & LF's */
		for(p = h; p = strchr(p, TAB); )
		  *p++ = ' ';                     /* turn TABs to space */
		
		if(*h){
		    unsigned long l;
		    ADDRESS *new_addr;

		    new_addr = (ADDRESS *)fs_get(sizeof(ADDRESS));
		    memset(new_addr, 0, sizeof(ADDRESS));

		    /*
		     * Base64 armor plate the gunk to protect against
		     * c-client quoting in output.
		     */
		    p = (char *)rfc822_binary((void *)h,
					      (unsigned long)strlen(h), &l);
		    sqznewlines(p);
		    new_addr->mailbox = (char *)fs_get(strlen(p) + 4);
		    sprintf(new_addr->mailbox, "&%s", p);
		    fs_give((void **)&p);
		    new_addr->host = cpystr(".RAW-FIELD.");
		    fill_in_ta(old_current, new_addr, 0, (char *)NULL);
		    mail_free_address(&new_addr);
		}

		fs_give((void **)&h);
	    }

	    return(0);
	}
    }

    /* no syntax errors in list, add them all */
    for(addr = adrlist; addr; addr = addr->next){
	fill_in_ta(old_current, addr, checked, (char *)NULL);
	added++;
    }

    return(added);
}


/*
 * Look for Text/directory attachments and process them.
 * Return number of lines added to the ta_list.
 */
int
process_vcard_atts(stream, msgno, root, body, partnum, ta_list)
    MAILSTREAM *stream;
    long        msgno;
    BODY       *root;
    BODY       *body;
    char       *partnum;
    TA_S      **ta_list;
{
    char      *addrs,        /* comma-separated list of addresses */
	      *value,
	      *encoded,
	      *escval,
	      *nickname,
	      *fullname,
	      *struct_name,
	      *fcc,
	      *tag,
	      *decoded,
	      *num = NULL,
	      *defaulttype = NULL,
	      *charset = NULL,
	      *altcharset,
	      *comma,
	      *p,
	      *comment,
	      *title,
	     **lines = NULL,
	     **ll;
    size_t     space;
    int        used = 0,
	       is_encoded = 0,
	       vcard_nesting = 0,
	       selected = 0;
    PART      *part;
    PARAMETER *parm;
    static char a_comma = ',';

    /*
     * Look through all the subparts searching for our special type.
     */
    if(body && body->type == TYPEMULTIPART){
	for(part = body->nested.part; part; part = part->next)
	  selected += process_vcard_atts(stream, msgno, root, &part->body,
					 partnum, ta_list);
    }
    /* only look in rfc822 subtype of type message */
    else if(body && body->type == TYPEMESSAGE
	    && body->subtype && !strucmp(body->subtype, "rfc822")){
	selected += process_vcard_atts(stream, msgno, root,
				       body->nested.msg->body,
				       partnum, ta_list);
    }
    /* found one (TYPEAPPLICATION for back compat.) */
    else if(body && MIME_VCARD(body->type,body->subtype)){

	dprint(4, (debugfile, "\n - found abook attachment to process - \n"));

	/*
	 * The Text/directory type has a possible parameter called
	 * "defaulttype" that we need to look for (this is from an old draft
	 * and is supported only for backwards compatibility). There is
	 * also a possible default "charset". (Default charset is also from
	 * an old draft and is no longer allowed.) We don't care about any of
	 * the other parameters.
	 */
	for(parm = body->parameter; parm; parm = parm->next)
	  if(!strucmp("defaulttype", parm->attribute))
	    break;
	
	if(parm)
	  defaulttype = parm->value;

	/* ...and look for possible charset parameter */
	for(parm = body->parameter; parm; parm = parm->next)
	  if(!strucmp("charset", parm->attribute))
	    break;
	
	if(parm)
	  charset = parm->value;

	num = partnum ? cpystr(partnum) : partno(root, body);
	lines = detach_vcard_att(stream, msgno, body, num);
	if(num)
	  fs_give((void **)&num);

	nickname = fullname = comment = title = fcc = struct_name = NULL;
#define CHUNK (size_t)500
	space = CHUNK;
	/* make comma-separated list of email addresses in addrs */
	addrs = (char *)fs_get((space+1) * sizeof(char));
	*addrs = '\0';
	for(ll = lines; ll && *ll; ll++){
	    altcharset = NULL;
	    decoded = NULL;
	    escval = NULL;
	    is_encoded = 0;
	    value = getaltcharset(*ll, &tag, &altcharset, &is_encoded);

	    if(!value || !tag){
		if(tag)
		  fs_give((void **)&tag);

		if(altcharset)
		  fs_give((void **)&altcharset);

		continue;
	    }

	    if(is_encoded){
		unsigned long length;

		decoded = (char *)rfc822_base64((unsigned char *)value,
						(unsigned long)strlen(value),
						&length);
		if(decoded){
		    decoded[length] = '\0';
		    value = decoded;
		}
		else
		  value = "<malformed B64 data>";
	    }

	    /* support default tag (back compat.) */
	    if(*tag == '\0' && defaulttype){
		fs_give((void **)&tag);
		tag = cpystr(defaulttype);
	    }

	    if(!strucmp(tag, "begin")){  /* vCard BEGIN */
		vcard_nesting++;
	    }
	    else if(!strucmp(tag, "end")){  /* vCard END */
		if(--vcard_nesting == 0){
		    if(title){
			if(!comment)
			  comment = title;
			else
			  fs_give((void **)&title);
		    }

		    /* add this entry */
		    selected += vcard_to_ta(addrs, fullname, struct_name,
					    nickname, comment, fcc, ta_list);
		    if(addrs)
		      *addrs = '\0';
		    
		    used = 0;
		    nickname = fullname = comment = title = fcc = NULL;
		    struct_name = NULL;
		}
	    }
	    /* add another address to addrs */
	    else if(!strucmp(tag, "email")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = encode_fullname_of_addrstring(escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->VAR_CHAR_SET);
		    if(encoded){
			/* allocate more space */
			if((used + strlen(encoded) + 1) > space){
			    space += CHUNK;
			    fs_resize((void **)&addrs, (space+1)*sizeof(char));
			}

			if(*addrs)
			  strcat(addrs, ",");

			strcat(addrs, encoded);
			used += (strlen(encoded) + 1);
			fs_give((void **)&encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "note") || !strucmp(tag, "misc")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					     (unsigned char *)escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->VAR_CHAR_SET);
		    if(encoded){
			/* Last one wins */
			if(comment)
			  fs_give((void **)&comment);

			comment = cpystr(encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "title")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					     (unsigned char *)escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->VAR_CHAR_SET);
		    if(encoded){
			/* Last one wins */
			if(title)
			  fs_give((void **)&title);

			title = cpystr(encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "x-fcc")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					     (unsigned char *)escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->VAR_CHAR_SET);
		    if(encoded){
			/* Last one wins */
			if(fcc)
			  fs_give((void **)&fcc);

			fcc = cpystr(encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "fn") || !strucmp(tag, "cn")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					     (unsigned char *)escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->VAR_CHAR_SET);
		    if(encoded){
			/* Last one wins */
			if(fullname)
			  fs_give((void **)&fullname);

			fullname = cpystr(encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "n")){
		/*
		 * This is a structured name field. It has up to five
		 * fields separated by ';'. The fields are Family Name (which
		 * we'll take to be Last name for our purposes), Given Name
		 * (First name for us), additional names, prefixes, and
		 * suffixes. We'll ignore prefixes and suffixes.
		 *
		 * If we find one of these records we'll use it in preference
		 * to the formatted name field (fn).
		 */
		if(*value && *value != ';'){
		    char *last, *first, *middle = NULL, *rest = NULL;
		    char *esclast, *escfirst, *escmiddle;
		    static char a_semi = ';';

		    last = value;

		    first = NULL;
		    p = last;
		    while(p && (first = strindex(p, a_semi)) != NULL){
			if(first > last && first[-1] != '\\')
			  break;
			else
			  p = first + 1;
		    }

		    if(first)
		      *first = '\0';

		    if(first && *(first+1) && *(first+1) != a_semi){
			first++;

			middle = NULL;
			p = first;
			while(p && (middle = strindex(p, a_semi)) != NULL){
			    if(middle > first && middle[-1] != '\\')
			      break;
			    else
			      p = middle + 1;
			}

			if(middle)
			  *middle = '\0';

			if(middle && *(middle+1) && *(middle+1) != a_semi){
			    middle++;

			    rest = NULL;
			    p = middle;
			    while(p && (rest = strindex(p, a_semi)) != NULL){
				if(rest > middle && rest[-1] != '\\')
				  break;
				else
				  p = rest + 1;
			    }

			    /* we don't care about the rest */
			    if(rest)
			      *rest = '\0';
			}
			else
			  middle = NULL;
		    }
		    else
		      first = NULL;

		    /*
		     * Structured name pieces can have multiple values.
		     * We're just going to take the first value in each part.
		     * Skip excaped commas, though.
		     */
		    p = last;
		    while(p && (comma = strindex(p, a_comma)) != NULL){
			if(comma > last && comma[-1] != '\\'){
			    *comma = '\0';
			    break;
			}
			else
			  p = comma + 1;
		    }

		    p = first;
		    while(p && (comma = strindex(p, a_comma)) != NULL){
			if(comma > first && comma[-1] != '\\'){
			    *comma = '\0';
			    break;
			}
			else
			  p = comma + 1;
		    }

		    p = middle;
		    while(p && (comma = strindex(p, a_comma)) != NULL){
			if(comma > middle && comma[-1] != '\\'){
			    *comma = '\0';
			    break;
			}
			else
			  p = comma + 1;
		    }

		    esclast = vcard_unescape(last);
		    escfirst = vcard_unescape(first);
		    escmiddle = vcard_unescape(middle);
		    sprintf(tmp_20k_buf+10000, "%s%s%s%s%s",
			    esclast ? esclast : "",
			    escfirst ? ", " : "",
			    escfirst ? escfirst : "",
			    escmiddle ? " " : "",
			    escmiddle ? escmiddle : "");

		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF-10000,
					     (unsigned char *)tmp_20k_buf+10000,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->VAR_CHAR_SET);

		    if(esclast && *esclast && escfirst && *escfirst){
			if(struct_name)
			  fs_give((void **)&struct_name);

			struct_name = cpystr(encoded);
		    }
		    else{
			/* in case we don't get a fullname better than this */
			if(!fullname)
			  fullname = cpystr(encoded);
		    }

		    if(esclast)
		      fs_give((void **)&esclast);
		    if(escfirst)
		      fs_give((void **)&escfirst);
		    if(escmiddle)
		      fs_give((void **)&escmiddle);
		}
	    }
	    /* suggested nickname */
	    else if(!strucmp(tag, "nickname") || !strucmp(tag, "x-nickname")){
		if(*value){
		    /* Last one wins */
		    if(nickname)
		      fs_give((void **)&nickname);

		    /*
		     * Nickname can have multiple values. We're just
		     * going to take the first. Skip escaped commas, though.
		     */
		    p = value;
		    while(p && (comma = strindex(p, a_comma)) != NULL){
			if(comma > value && comma[-1] != '\\'){
			    *comma = '\0';
			    break;
			}
			else
			  p = comma + 1;
		    }

		    nickname = vcard_unescape(value);
		}
	    }

	    if(tag)
	      fs_give((void **)&tag);

	    if(altcharset)
	      fs_give((void **)&altcharset);

	    if(decoded)
	      fs_give((void **)&decoded);

	    if(escval)
	      fs_give((void **)&escval);
	}

	if(title){
	    if(!comment)
	      comment = title;
	    else
	      fs_give((void **)&title);
	}

	if(fullname || struct_name || nickname || fcc
	   || comment || (addrs && *addrs))
	  selected += vcard_to_ta(addrs, fullname, struct_name, nickname,
				  comment, fcc, ta_list);

	if(addrs)
	  fs_give((void **)&addrs);

	free_list_array(&lines);
    }

    return(selected);
}


typedef struct a_list {
    int          dup;
    adrbk_cntr_t dst_enum;
    TA_S        *ta;
} SWOOP_S;

int
cmp_swoop_list(a1, a2)
    const QSType *a1, *a2;
{
    SWOOP_S *x = (SWOOP_S *)a1;
    SWOOP_S *y = (SWOOP_S *)a2;

    if(x->dup){
	if(y->dup)
	  return((x->dst_enum > y->dst_enum) ? -1
		   : (x->dst_enum == y->dst_enum) ? 0 : 1);
	else
	  return(-1);
    }
    else if(y->dup)
      return(1);
    else
      return((x > y) ? -1 : (x == y) ? 0 : 1);  /* doesn't matter */
}

int
take_without_edit(ta_list, num_in_list, command_line, tas, cmd)
    TA_S        *ta_list;
    int          num_in_list;
    int          command_line;
    TA_STATE_S **tas;
    char        *cmd;
{
    PerAddrBook   *pab_dst;
    SAVE_STATE_S   state;  /* For saving state of addrbooks temporarily */
    int            rc, total_to_copy;
    int		   how_many_dups = 0, how_many_to_copy = 0, skip_dups = 0;
    int		   ret = 0;
    int		   err = 0, need_write = 0, we_cancel = 0;
    adrbk_cntr_t   new_entry_num;
    char           warn[2][MAX_NICKNAME+1];
    char           tmp[200];
    TA_S          *current;
    SWOOP_S       *swoop_list = NULL, *sw;

    dprint(2, (debugfile, "\n - take_without_edit(%d) - \n",
	       num_in_list));

    /* move to beginning of the list */
    if(ta_list)
      while(ta_list->prev)
	ta_list = ta_list->prev;

    pab_dst = setup_for_addrbook_add(&state, command_line, cmd);
    if(!pab_dst)
      goto get_out;
    
    swoop_list = (SWOOP_S *)fs_get((num_in_list+1) * sizeof(SWOOP_S));
    memset((void *)swoop_list, 0, (num_in_list+1) * sizeof(SWOOP_S));
    sw = swoop_list;

    /*
     * Look through all the vcards for those with nicknames already
     * existing in the destination abook (dups) and build a list of
     * entries to be acted on.
     */
    for(current = ta_list; current; current = current->next){
	adrbk_cntr_t dst_enum;

	if(current->skip_it)
	  continue;
	
	/* check to see if this nickname already exists in the dest abook */
	if(current->nickname && current->nickname[0]){
	    AdrBk_Entry *abe;

	    current->checked = 0;
	    abe = adrbk_lookup_by_nick(pab_dst->address_book,
				       current->nickname, &dst_enum);
	    /*
	     * This nickname already exists.
	     */
	    if(abe){
		sw->dup = 1;
		sw->dst_enum = dst_enum;
		if(how_many_dups < 2){
		  strncpy(warn[how_many_dups], current->nickname, MAX_NICKNAME);
		  warn[how_many_dups][MAX_NICKNAME] = '\0';
		}
		
		how_many_dups++;
	    }
	}

	sw->ta = current;
	sw++;
	how_many_to_copy++;
    }

    /*
     * If there are some nicknames which already exist in the selected
     * abook, ask user what to do.
     */
    if(how_many_dups > 0){
	if(how_many_dups == 1){
	    if(how_many_to_copy == 1 && num_in_list == 1){
		ret = 'T';  /* use Take */
		if(tas && *tas){
		    (*tas)->state = state;
		    (*tas)->pab   = pab_dst;
		}

		goto get_out;
	    }
	    else{
		sprintf(tmp,
		        "Entry with nickname \"%.*s\" already exists, replace ",
		        sizeof(tmp)-50, warn[0]);
	    }
	}
	else if(how_many_dups == 2)
	  sprintf(tmp,
		  "Nicknames \"%.*s\" and \"%.*s\" already exist, replace ",
		  (sizeof(tmp)-50)/2, warn[0], (sizeof(tmp)-50)/2, warn[1]);
	else
	  sprintf(tmp, "%d of the nicknames already exist, replace ",
		  how_many_dups);
	
	switch(want_to(tmp, 'n', 'x', h_ab_copy_dups, WT_NORM)){
	  case 'n':
	    skip_dups++;
	    break;

	  case 'y':
	    break;

	  case 'x':
	    err++;
	    goto get_out;
	}
    }

    /*
     * Because the deletes happen immediately we have to delete from high
     * entry number towards lower entry numbers so that we are deleting
     * the correct entries. In order to do that we'll sort the swoop_list
     * to give us a safe order.
     */
    if(!skip_dups && how_many_dups > 1)
      qsort((QSType *)swoop_list, (size_t)num_in_list, sizeof(*swoop_list),
	    cmp_swoop_list);

    we_cancel = busy_alarm(1, "Saving addrbook entries", NULL, 1);
    total_to_copy = how_many_to_copy - (skip_dups ? how_many_dups : 0);

    /*
     * Add the list of entries to the destination abook.
     */
    for(sw = swoop_list; sw && sw->ta; sw++){
	Tag  tag;
	char abuf[MAX_ADDRESS + 1];
	int  count_of_addrs;
	
	if(skip_dups && sw->dup)
	  continue;

	/*
	 * Delete existing dups and replace them.
	 */
	if(sw->dup){

	    /* delete the existing entry */
	    rc = 0;
	    if(adrbk_delete(pab_dst->address_book,
			    (a_c_arg_t)sw->dst_enum, 1, 0, 0, 0) == 0){
		need_write++;
	    }
	    else{
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				 "Error replacing entry in %.200s: %.200s",
				 pab_dst->nickname,
				 error_description(errno));
		err++;
		goto get_out;
	    }
	}

	/*
	 * We need to count the number of addresses in this entry in order
	 * to tell the adrbk routines if it is a List or a Single, and in
	 * order to pass the right stuff to be added.
	 */
	count_of_addrs = count_addrs(sw->ta->addr);
	tag = (count_of_addrs > 1) ? List : Single;
	if(tag == Single){
	    if(sw->ta->addr->mailbox && sw->ta->addr->mailbox[0]){
		char *scratch, *p, *t, *u;
		unsigned long l;

		scratch = (char *)fs_get((size_t)est_size(sw->ta->addr));
		scratch[0] = '\0';
		rfc822_write_address(scratch, sw->ta->addr);
		if(p = srchstr(scratch, "@.RAW-FIELD.")){
		  for(t = p; ; t--)
		    if(*t == '&'){		/* find "leading" token */
			*t++ = ' ';		/* replace token */
			*p = '\0';		/* tie off string */
			u = (char *)rfc822_base64((unsigned char *)t,
						  (unsigned long)strlen(t), &l);
			*p = '@';		/* restore 'p' */
			rplstr(p, 12, "");	/* clear special token */
			rplstr(t, strlen(t), u);  /* Null u is handled */
			if(u)
			  fs_give((void **)&u);
		    }
		    else if(t == scratch)
		      break;
		}

		strncpy(abuf, scratch, sizeof(abuf)-1);
		abuf[sizeof(abuf)-1] = '\0';

		if(scratch)
		  fs_give((void **)&scratch);
	    }
	    else
	      abuf[0] = '\0';
	}
	  

	/*
	 * Now we have a clean slate to work with.
	 */
	if(total_to_copy <= 1)
	  rc = adrbk_add(pab_dst->address_book,
		         NO_NEXT,
		         sw->ta->nickname,
		         sw->ta->fullname,
		         tag == Single ? abuf : NULL,
		         sw->ta->fcc,
		         sw->ta->comment,
		         tag,
		         &new_entry_num,
		         (int *)NULL,
		         0,
		         0,
		         0);
	else
	  rc = adrbk_append(pab_dst->address_book,
		            sw->ta->nickname,
		            sw->ta->fullname,
		            tag == Single ? abuf : NULL,
		            sw->ta->fcc,
		            sw->ta->comment,
		            tag,
			    &new_entry_num);

	if(rc == 0)
	  need_write++;
	
	/*
	 * If the entry we copied is a list, we also have to add
	 * the list members to the copy.
	 */
	if(rc == 0 && tag == List){
	    int i, save_sort_rule;
	    ADDRESS *a, *save_next;
	    char **list;

	    list = (char **)fs_get((count_of_addrs + 1) * sizeof(char *));
	    memset((void *)list, 0, (count_of_addrs+1) * sizeof(char *));
	    i = 0;
	    for(a = sw->ta->addr; a; a = a->next){
		save_next = a->next;
		a->next = NULL;

		if(a->mailbox && a->mailbox[0]){
		    char *scratch, *p, *t, *u;
		    unsigned long l;

		    scratch = (char *)fs_get((size_t)est_size(a));
		    scratch[0] = '\0';
		    rfc822_write_address(scratch, a);
		    if(p = srchstr(scratch, "@.RAW-FIELD.")){
		      for(t = p; ; t--)
			if(*t == '&'){		/* find "leading" token */
			    *t++ = ' ';		/* replace token */
			    *p = '\0';		/* tie off string */
			    u = (char *)rfc822_base64((unsigned char *)t,
						      (unsigned long)strlen(t), &l);
			    *p = '@';		/* restore 'p' */
			    rplstr(p, 12, "");	/* clear special token */
			    rplstr(t, strlen(t), u);  /* Null u is handled */
			    if(u)
			      fs_give((void **)&u);
			}
			else if(t == scratch)
			  break;
		    }

		    strncpy(abuf, scratch, sizeof(abuf)-1);
		    abuf[sizeof(abuf)-1] = '\0';

		    if(scratch)
		      fs_give((void **)&scratch);
		}
		else
		  abuf[0] = '\0';

		list[i++] = cpystr(abuf);
		a->next = save_next;
	    }

	    /*
	     * We want it to copy the list in the exact order
	     * without sorting it.
	     */
	    save_sort_rule = pab_dst->address_book->sort_rule;
	    pab_dst->address_book->sort_rule = AB_SORT_RULE_NONE;

	    rc = adrbk_nlistadd(pab_dst->address_book,
				(a_c_arg_t)new_entry_num,
				list, 0, 0, 0);

	    pab_dst->address_book->sort_rule = save_sort_rule;
	    free_list_array(&list);
	}

	if(rc != 0){
	    q_status_message1(SM_ORDER | SM_DING, 3, 5,
			      "Error saving: %.200s",
			      error_description(errno));
	    err++;
	    goto get_out;
	}
    }

    if(need_write){
	long old_size;

	if(adrbk_write(pab_dst->address_book, NULL, 0, 1, total_to_copy <= 1)){
	    err++;
	    goto get_out;
	}

	if(total_to_copy > 1){
	    exp_free(pab_dst->address_book->exp);

	    /*
	     * Sort the results since they were appended.
	     */
#if !(defined(DOS) && !defined(_WINDOWS))
	    old_size = adrbk_set_nominal_cachesize(pab_dst->address_book,
				    (long)adrbk_count(pab_dst->address_book));
#endif /* !DOS */
	    (void)adrbk_sort(pab_dst->address_book,
			     (a_c_arg_t)0, (adrbk_cntr_t *)NULL, 0);
#if !(defined(DOS) && !defined(_WINDOWS))
	    (void)adrbk_set_nominal_cachesize(pab_dst->address_book, old_size);
#endif /* !DOS */
	}
    }

get_out:
    if(we_cancel)
      cancel_busy_alarm(1);

    if(!ret)
      restore_state(&state);

    if(swoop_list)
      fs_give((void **)&swoop_list);

    ps_global->mangled_footer = 1;

    if(err){
	char capcmd[50];

	ret = -1;
	sprintf(capcmd,
		"%c%.*s",
		islower((unsigned char)(*cmd)) ? toupper((unsigned char)*cmd)
					       : *cmd,
		sizeof(capcmd)-2, cmd+1);
	if(need_write)
	  q_status_message1(SM_ORDER | SM_DING, 3, 4,
			   "%.200s only partially completed", capcmd);
	else
	  cmd_cancelled(capcmd);
    }
    else if(ret != 'T' && total_to_copy > 0){

	ret = 1;
	sprintf(tmp, "Saved %d %s to \"%.*s\"",
		total_to_copy,
		(total_to_copy > 1) ? "entries" : "entry",
		sizeof(tmp)-30, pab_dst->nickname);
	q_status_message(SM_ORDER, 0, 4, tmp);
    }

    return(ret);
}


/*
 * Take the split out contents of a vCard entry and turn them into a TA.
 *
 * Always returns 1, the number of TAs added to ta_list.
 */
int
vcard_to_ta(addrs, fullname, better_fullname, nickname, comment, fcc, ta_list)
    char  *addrs, *fullname, *better_fullname, *nickname, *comment, *fcc;
    TA_S **ta_list;
{
    ADDRESS   *addrlist = NULL;

    /*
     * Parse it into an addrlist, which is what fill_in_ta wants
     * to see.
     */
    rfc822_parse_adrlist(&addrlist, addrs, fakedomain);
    if(!addrlist)
      addrlist = mail_newaddr();  /* empty addr, to make right thing happen */

    *ta_list = fill_in_ta(ta_list, addrlist, 1,
			  fullname ? fullname
				   : better_fullname ? better_fullname
				   		     : "Forwarded Entry");
    (*ta_list)->nickname = nickname ? nickname : cpystr("");
    (*ta_list)->comment  = comment;
    (*ta_list)->fcc      = fcc;

    /*
     * We are tempted to want to call switch_to_last_comma_first() here when
     * we don't have a better_fullname (which is already last, first).
     * But, since this is the way it was sent to us we should probably leave
     * it alone. That means that if we use a fullname culled from the
     * body of a message, or from a header, or from an "N" vcard record,
     * we will make it be Last, First; but if we use one from an "FN" record
     * we won't.
     */
    (*ta_list)->fullname = better_fullname ? better_fullname
					   : fullname;
    
    /*
     * The TA list will free its members but we have to take care of this
     * extra one that isn't assigned to a TA member.
     */
    if(better_fullname && fullname)
      fs_give((void **)&fullname);

    if(addrlist)
      mail_free_address(&addrlist);

    return(1);
}


/*
 * Look through line which is supposed to look like
 *
 *  type;charset=iso-8859-1;encoding=b: value
 *
 * Type might be email, or nickname, ...  It is optional because of the
 * defaulttype parameter (there isn't such a thing as defaulttype parameters
 * as of Nov. 96 draft). The semicolon and colon are special chars.  Each
 * parameter in this line is a semicolon followed by the parameter type "="
 * the parameter value.  Parameters are optional, too.  There is always a colon,
 * followed by value.  Whitespace can be everywhere up to where value starts.
 * There is also an optional <group> "." preceding the type, which we will
 * ignore. It's for grouping related lines.
 * (As of July, 97 draft, it is no longer permissible to include a charset
 * parameter in each line. Only one is permitted in the content-type mime hdr.)
 * (Also as of July, 97 draft, all white space is supposed to be significant.
 * We will continue to skip white space surrounding '=' and so forth for
 * backwards compatibility and because it does no harm in almost all cases.)
 * (As of Nov, 97 draft, some white space is not significant. That is, it is
 * allowed in some places.)
 *
 *
 * Args: line     -- the line to look at
 *       type     -- this is a return value, and is an allocated copy of
 *                    the type, freed by the caller. For example, "email".
 *       alt      -- this is a return value, and is an allocated copy of
 *                    the value of the alternate charset, to be freed by
 *                    the caller.  For example, this might be "iso-8859-2".
 *     encoded    -- this is a return value, and is set to 1 if the value
 *                    is b-encoded
 *
 * Return value: a pointer to the start of value, or NULL if the syntax
 * isn't right.  It's possible for value to be equal to "".
 */
char *
getaltcharset(line, type, alt, encoded)
    char  *line;
    char **type;
    char **alt;
    int   *encoded;
{
    char        *p, *q, *left_semi, *group_dot, *colon;
    char        *start_of_enc, *start_of_cset, tmpsave;
    static char *cset = "charset";
    static char *enc = "encoding";

    if(type)
      *type = NULL;

    if(alt)
      *alt = NULL;

    if(encoded)
      *encoded = 0;

    colon = strindex(line, ':');
    if(!colon)
      return NULL;

    left_semi = strindex(line, ';');
    if(left_semi && left_semi > colon)
      left_semi = NULL;
    
    group_dot = strindex(line, '.');
    if(group_dot && (group_dot > colon || left_semi && group_dot > left_semi))
      group_dot = NULL;

    /*
     * Type is everything up to the semicolon, or the colon if no semicolon.
     * However, we want to skip optional <group> ".".
     */
    if(type){
	q = left_semi ? left_semi : colon;
	tmpsave = *q;
	*q = '\0';
	*type = cpystr(group_dot ? group_dot+1 : line);
	*q = tmpsave;
	sqzspaces(*type);
    }

    if(left_semi && alt
       && (p = srchstr(left_semi+1, cset))
       && p < colon){
	p += strlen(cset);
	p = skip_white_space(p);
	if(*p++ == '='){
	    p = skip_white_space(p);
	    if(p < colon){
		start_of_cset = p;
		p++;
		while(p < colon && !isspace((unsigned char)*p) && *p != ';')
		  p++;
		
		tmpsave = *p;
		*p = '\0';
		*alt = cpystr(start_of_cset);
		*p = tmpsave;
	    }
	}
    }

    if(encoded && left_semi
       && (p = srchstr(left_semi+1, enc))
       && p < colon){
	p += strlen(enc);
	p = skip_white_space(p);
	if(*p++ == '='){
	    p = skip_white_space(p);
	    if(p < colon){
		start_of_enc = p;
		p++;
		while(p < colon && !isspace((unsigned char)*p) && *p != ';')
		  p++;
		
		if(*start_of_enc == 'b' && start_of_enc + 1 == p)
		  *encoded = 1;
	    }
	}
    }

    p = colon + 1;

    return(skip_white_space(p));
}


/*
 * Return incoming_fullname in Last, First form.
 * The passed in fullname should already be rfc1522_decoded.
 * If there is already a comma, we add quotes around the fullname so we can
 *   tell it isn't a Last, First comma but a real comma in the Fullname.
 *
 * Args: incoming_fullname -- 
 *                new_full -- passed in pointer to place to put new fullname
 *                    nbuf -- size of new_full
 * Returns: new_full
 */
void
switch_to_last_comma_first(incoming_fullname, new_full, nbuf)
    char  *incoming_fullname;
    char  *new_full;
    size_t nbuf;
{
    char  save_value;
    char *save_end, *nf, *inc, *p = NULL;

    if(incoming_fullname == NULL){
	new_full[0] = '\0';
	return;
    }

    /* get rid of leading white space */
    for(inc = incoming_fullname; *inc && isspace((unsigned char)*inc); inc++)
      ;/* do nothing */

    /* get rid of trailing white space */
    for(p = inc + strlen(inc) - 1; *p && p >= inc && 
	  isspace((unsigned char)*p); p--)
      ;/* do nothing */
    
    save_end = ++p;
    if(save_end == inc){
	new_full[0] = '\0';
	return;
    }

    save_value = *save_end;  /* so we don't alter incoming_fullname */
    *save_end = '\0';
    nf = new_full;
    memset(new_full, 0, nbuf);

    if(strindex(inc, ',') != NULL){
	int add_quotes = 0;

	/*
	 * Quote already existing comma.
	 *
	 * We'll get this wrong if it is already quoted but the quote
	 * doesn't start right at the beginning.
	 */
	if(inc[0] != '"'){
	    add_quotes++;
	    *nf++ = '"';
	}

	strncpy(nf, inc, nbuf - (add_quotes ? 3 : 1));
	if(add_quotes)
	  strcat(nf, "\"");
    }
    else if(strindex(inc, SPACE)){
	char *last, *end;

	end = new_full + nbuf - 1;

	/*
	 * Switch First Middle Last to Last, First Middle
	 */

	/* last points to last word, which does exist */
	last = skip_white_space(strrindex(inc, SPACE));

	/* copy Last */
	for(p = last; *p && nf < end-2; *nf++ = *p++)
	  ;/* do nothing */

	*nf++ = ',';
	*nf++ = SPACE;

	/* copy First Middle */
	for(p = inc; p < last && nf < end; *nf++ = *p++)
	  ;/* do nothing */

	removing_trailing_white_space(new_full);
    }
    else
      strncpy(new_full, inc, nbuf-1);

    *save_end = save_value;
}


/*
 * Fetch this body part, content-decode it if necessary, split it into
 * lines, and return the lines in an allocated array, which is freed
 * by the caller.  Folded lines are replaced by single long lines.
 */
char **
detach_vcard_att(stream, msgno, body, partnum)
    MAILSTREAM *stream;
    long        msgno;
    BODY       *body;
    char       *partnum;
{
    unsigned long length;
    char    *text  = NULL, /* raw text */
	    *dtext = NULL, /* content-decoded text */
	   **res   = NULL, /* array of decoded lines */
	    *lptr,         /* line pointer */
	    *next_line;
    int      i, count;

    /* make our own copy of text so we can change it */
    text = cpystr(mail_fetchbody_full(stream, msgno, partnum, &length,FT_PEEK));
    text[length] = '\0';

    /* decode the text */
    switch(body->encoding){
      default:
      case ENC7BIT:
      case ENC8BIT:
      case ENCBINARY:
	dtext = text;
	break;

      case ENCBASE64:
	dtext = (char *)rfc822_base64((unsigned char *)text,
				      (unsigned long)strlen(text),
				      &length);
	if(dtext)
	  dtext[length] = '\0';
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "Malformed B64 data in address book attachment.");

	if(text)
	  fs_give((void **)&text);

	break;

      case ENCQUOTEDPRINTABLE:
	dtext = (char *)rfc822_qprint((unsigned char *)text,
				      (unsigned long)strlen(text),
				      &length);
	if(dtext)
	  dtext[length] = '\0';
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "Malformed QP data in address book attachment.");

	if(text)
	  fs_give((void **)&text);

	break;
    }

    /* count the lines */
    next_line = dtext;
    count = 0;
    for(lptr = next_line; lptr && *lptr; lptr = next_line){
	for(next_line = lptr; *next_line; next_line++){
	    /*
	     * Find end of current line.
	     */
	    if(*next_line == '\r' && *(next_line+1) == '\n'){
		next_line += 2;
		/* not a folded line, count it */
		if(!*next_line || (*next_line != SPACE && *next_line != TAB))
		  break;
	    }
	}

	count++;
    }

    /* allocate space for resulting array of lines */
    res = (char **)fs_get((count + 1) * sizeof(char *));
    memset((void *)res, 0, (count + 1) * sizeof(char *));
    next_line = dtext;
    for(i=0, lptr=next_line; lptr && *lptr && i < count; lptr=next_line, i++){
	/*
	 * Move next_line to start of next line and null terminate
	 * current line.
	 */
	for(next_line = lptr; *next_line; next_line++){
	    /*
	     * Find end of current line.
	     */
	    if(*next_line == '\r' && *(next_line+1) == '\n'){
		next_line += 2;
		/* not a folded line, terminate it */
		if(!*next_line || (*next_line != SPACE && *next_line != TAB)){
		    *(next_line-2) = '\0';
		    break;
		}
	    }
	}

	/* turn folded lines into long lines in place */
	vcard_unfold(lptr);
	res[i] = cpystr(lptr);
    }

    if(dtext)
      fs_give((void **)&dtext);

    res[count] = '\0';
    return(res);
}


/* from eduardo chappa */
#define ALLOWED_TYPE(t) (!strucmp((t), "plain") || !strucmp((t), "html") || \
			 !strucmp((t), "enriched") || !strucmp((t), "richtext"))

/*
 * Look for possible addresses in the first text part of a message for
 * use by TakeAddr command.
 * Returns the number of TA lines added.
 */
int
grab_addrs_from_body(stream, msgno, body, ta_list)
    MAILSTREAM *stream;
    long        msgno;
    BODY       *body;
    TA_S      **ta_list;
{
#define MAXLINESZ 2000
    char       line[MAXLINESZ + 1];
    STORE_S   *so;
    gf_io_t    pc;
    SourceType src = CharStar;
    int        added = 0, failure;

    dprint(9, (debugfile, "\n - grab_addrs_from_body - \n"));

    /*
     * If it is text/plain or it is multipart with a first part of text/plain,
     * we want to continue, else forget it.
     */
    if(!((body->type == TYPETEXT && body->subtype
	  && ALLOWED_TYPE(body->subtype))
			      ||
         (body->type == TYPEMULTIPART && body->nested.part
	  && body->nested.part->body.type == TYPETEXT
	  && ALLOWED_TYPE(body->nested.part->body.subtype))))
      return 0;

#ifdef DOS
    src = TmpFileStar;
#endif

    if((so = so_get(src, NULL, EDIT_ACCESS)) == NULL)
      return 0;

    gf_set_so_writec(&pc, so);

    failure = !get_body_part_text(stream, body, msgno, "1", pc, NULL);

    gf_clear_so_writec(so);

    if(failure){
	so_give(&so);
	return 0;
    }

    so_seek(so, 0L, 0);

    while(get_line_of_message(so, line, sizeof(line))){
	ADDRESS *addr;
	char     save_end, *start, *tmp_a_string, *tmp_personal, *p;
	int      n;

	/* process each @ in the line */
	for(p = (char *) line; start = mail_addr_scan(p, &n); p = start + n){

	    tmp_personal = NULL;

	    if(start > line && *(start-1) == '<' && *(start+n) == '>'){
		int   words, in_quote;
		char *fn_start;

		/*
		 * Take a shot at looking for full name
		 * If we can find a colon maybe we've got a header line
		 * embedded in the body.
		 * This is real ad hoc.
		 */

		/*
		 * Go back until we run into a character that probably
		 * isn't a fullname character.
		 */
		fn_start = start-1;
		in_quote = words = 0;
		while(fn_start > p && (in_quote || !(*(fn_start-1) == ':'
		      || *(fn_start-1) == ';' || *(fn_start-1) == ','))){
		    fn_start--;
		    if(!in_quote && isspace((unsigned char)*fn_start))
		      words++;
		    else if(*fn_start == '"' &&
			    (fn_start == p || *(fn_start-1) != '\\')){
			if(in_quote){
			    in_quote = 0;
			    break;
			}
			else
			  in_quote = 1;
		    }

		    if(words == 5)
		      break;
		}

		/* wasn't a real quote, forget about fullname */
		if(in_quote)
		  fn_start = start-1;

		/* Skip forward over the white space. */
		while(isspace((unsigned char)*fn_start) || *fn_start == '(')
		  fn_start++;

		/*
		 * Make sure the first word is capitalized.
		 * (just so it is more likely it is a name)
		 */
		while(fn_start < start-1 &&
		      !(isupper(*fn_start) || *fn_start == '"')){
		    if(*fn_start == '('){
			fn_start++;
			continue;
		    }

		    /* skip forward over this word */
		    while(fn_start < start-1 && 
			  !isspace((unsigned char)*fn_start))
		      fn_start++;

		    while(fn_start < start-1 && 
			  isspace((unsigned char)*fn_start))
		      fn_start++;
		}

		if(fn_start < start-1){
		    char *fn_end, save_fn_end;
		    
		    /* remove white space between fullname and start */
		    fn_end = start-1;
		    while(fn_end > fn_start
			  && isspace((unsigned char)*(fn_end - 1)))
		      fn_end--;

		    save_fn_end = *fn_end;
		    *fn_end = '\0';

		    /* remove matching quotes */
		    if(*fn_start == '"' && *(fn_end-1) == '"'){
			fn_start++;
			*fn_end = save_fn_end;
			fn_end--;
			save_fn_end = *fn_end;
			*fn_end = '\0';
		    }

		    /* copy fullname */
		    if(*fn_start)
		      tmp_personal = cpystr(fn_start);

		    *fn_end = save_fn_end;
		}
	    }


	    save_end = *(start+n);
	    *(start+n) = '\0';
	    /* rfc822_parse_adrlist feels free to destroy input so send copy */
	    tmp_a_string = cpystr(start);
	    *(start+n) = save_end;
	    addr = NULL;
	    ps_global->c_client_error[0] = '\0';
	    rfc822_parse_adrlist(&addr, tmp_a_string, fakedomain);
	    if(tmp_a_string)
	      fs_give((void **)&tmp_a_string);

	    if(tmp_personal){
		if(addr){
		    if(addr->personal)
		      fs_give((void **)&addr->personal);

		    addr->personal = tmp_personal;
		}
		else
		  fs_give((void **)&tmp_personal);
	    }

	    if((addr && addr->host && addr->host[0] == '@') ||
	       ps_global->c_client_error[0]){
		mail_free_address(&addr);
		continue;
	    }

	    if(addr && addr->mailbox && addr->host){
		added++;
		*ta_list = fill_in_ta(ta_list, addr, 0, (char *)NULL);
	    }

	    if(addr)
	      mail_free_address(&addr);
	}
    }

    so_give(&so);
    return(added);
}


/*
 * Get the next line of the object pointed to by source.
 * Skips empty lines.
 * Linebuf is a passed in buffer to put the line in.
 * Linebuf is null terminated and \r and \n chars are removed.
 * 0 is returned when there is no more input.
 */
int
get_line_of_message(source, linebuf, linebuflen)
    STORE_S *source;
    char    *linebuf;
    int      linebuflen;
{
    int pos = 0;
    unsigned char c;

    if(linebuflen < 2)
      return 0;

    while(so_readc(&c, source)){
	if(c == '\n' || c == '\r'){
	  if(pos > 0)
	    break;
	}
	else{
	    linebuf[pos++] = c;
	    if(pos >= linebuflen - 2)
	      break;
	}
    }

    linebuf[pos] = '\0';

    return(pos);
}


/*
 * Copy the first address in list a and return it in allocated memory.
 */
ADDRESS *
copyaddr(a)
    ADDRESS *a;
{
    ADDRESS *new = NULL;

    if(a){
	new = mail_newaddr();
	if(a->personal)
	  new->personal = cpystr(a->personal);

	if(a->adl)
	  new->adl      = cpystr(a->adl);

	if(a->mailbox)
	  new->mailbox  = cpystr(a->mailbox);

	if(a->host)
	  new->host     = cpystr(a->host);

	new->next = NULL;
    }

    return(new);
}


/*
 * Copy the whole list a.
 */
ADDRESS *
copyaddrlist(a)
    ADDRESS *a;
{
    ADDRESS *new = NULL, *head = NULL, *current;

    for(; a; a = a->next){
	new = mail_newaddr();
	if(!head)
	  head = current = new;
	else{
	    current->next = new;
	    current = new;
	}

	if(a->personal)
	  new->personal = cpystr(a->personal);

	if(a->adl)
	  new->adl      = cpystr(a->adl);

	if(a->mailbox)
	  new->mailbox  = cpystr(a->mailbox);

	if(a->host)
	  new->host     = cpystr(a->host);
    }

    if(new)
      new->next = NULL;

    return(head);
}


/*
 * Inserts a new entry based on addr in the TA list.
 *
 * Args: old_current -- the TA list
 *              addr -- the address for this line
 *           checked -- start this line out checked
 *      print_string -- if non-null, this line is informational and is just
 *                       printed out, it can't be selected
 */
TA_S *
fill_in_ta(old_current, addr, checked, print_string)
    TA_S    **old_current;
    ADDRESS  *addr;
    int       checked;
    char     *print_string;
{
    TA_S *new_current;

    /* c-client convention for group syntax, which we don't want to deal with */
    if(!print_string && (!addr || !addr->mailbox || !addr->host))
      new_current = *old_current;
    else{

	new_current           = new_taline(old_current);
	if(print_string && addr){
	    new_current->frwrded  = 1;
	    new_current->skip_it  = 0;
	    new_current->print    = 0;
	    new_current->checked  = checked != 0;
	    new_current->addr     = copyaddrlist(addr);
	    new_current->strvalue = cpystr(print_string);
	}
	else if(print_string){
	    new_current->frwrded  = 0;
	    new_current->skip_it  = 1;
	    new_current->print    = 1;
	    new_current->checked  = 0;
	    new_current->addr     = 0;
	    new_current->strvalue = cpystr(print_string);
	}
	else{
	    new_current->frwrded  = 0;
	    new_current->skip_it  = 0;
	    new_current->print    = 0;
	    new_current->checked  = checked != 0;
	    new_current->addr     = copyaddr(addr);
	    if(addr->host[0] == '.')
	      new_current->strvalue
		  = cpystr("Error in address (ok to try Take anyway)");
	    else{
		char *bufp;

		bufp = (char *)fs_get((size_t)est_size(new_current->addr));
		new_current->strvalue
			      = cpystr(addr_string(new_current->addr,bufp));
		fs_give((void **)&bufp);
	    }
	}
    }

    return(new_current);
}


int
eliminate_dups_and_us(list)
    TA_S *list;
{
    return(eliminate_dups_and_maybe_us(list, 1));
}


int
eliminate_dups_but_not_us(list)
    TA_S *list;
{
    return(eliminate_dups_and_maybe_us(list, 0));
}


/*
 * Look for dups in list and mark them so we'll skip them.
 *
 * We also throw out any addresses that are us (if us_too is set), since
 * we won't usually want to take ourselves to the addrbook.
 * On the otherhand, if there is nothing but us, we leave it.
 *
 * Don't toss out forwarded entries.
 *
 * Returns the number of dups eliminated that were also selected.
 */
int
eliminate_dups_and_maybe_us(list, us_too)
    TA_S *list;
    int   us_too;
{
    ADDRESS *a, *b;
    TA_S    *ta, *tb;
    int eliminated = 0;

    /* toss dupes */
    for(ta = list; ta; ta = ta->next){

	if(ta->skip_it || ta->frwrded) /* already tossed or forwarded */
	  continue;

	a = ta->addr;

	/* Check addr "a" versus tail of the list */
	for(tb = ta->next; tb; tb = tb->next){
	    if(tb->skip_it || tb->frwrded) /* already tossed or forwarded */
	      continue;

	    b = tb->addr;
	    if(dup_addrs(a, b)){
		if(ta->checked || !(tb->checked)){
		    /* throw out b */
		    if(tb->checked)
		      eliminated++;

		    tb->skip_it = 1;
		}
		else{ /* tb->checked */
		    /* throw out a */
		    ta->skip_it = 1;
		    break;
		}
	    }
	}
    }

    if(us_too){
	/* check whether all remaining addrs are us */
	for(ta = list; ta; ta = ta->next){

	    if(ta->skip_it) /* already tossed */
	      continue;

	    if(ta->frwrded) /* forwarded entry, so not us */
	      break;

	    a = ta->addr;

	    if(!address_is_us(a, ps_global))
	      break;
	}

	/*
	 * if at least one address that isn't us, remove all of us from
	 * the list
	 */
	if(ta){
	    for(ta = list; ta; ta = ta->next){

		if(ta->skip_it || ta->frwrded) /* already tossed or forwarded */
		  continue;

		a = ta->addr;

		if(address_is_us(a, ps_global)){
		    if(ta->checked)
		      eliminated++;

		    /* throw out a */
		    ta->skip_it = 1;
		}
	    }
	}
    }

    return(eliminated);
}


/*
 * Returns 1 if x and y match, 0 if not.
 */
int
dup_addrs(x, y)
    ADDRESS *x, *y;
{
    return(x && y && strucmp(x->mailbox, y->mailbox) == 0
           &&  strucmp(x->host, y->host) == 0);
}



#ifdef	ENABLE_LDAP
/*
 * Save an LDAP directory entry somewhere
 *
 * Args: ps        -- pine struct
 *       e         -- the entry to save
 *       save      -- If this is set, then bypass the question about whether
 *                    to save or export and just do the save.
 */
void
save_ldap_entry(ps, e, save)
    struct pine          *ps;
    LDAP_SERV_RES_S      *e;
    int                   save;
{
    char  *fullname = NULL,
	  *address = NULL,
	  *first = NULL,
	  *last = NULL,
	  *comment = NULL;
    char **cn = NULL,
	 **mail = NULL,
	 **sn = NULL,
	 **givenname = NULL,
	 **title = NULL,
	 **telephone = NULL,
	 **elecmail = NULL,
	 **note = NULL,
	 **ll;
    int    j,
	   export = 0;


    dprint(2, (debugfile, "\n - save_ldap_entry - \n"));

    if(e){
	char       *a;
	BerElement *ber;

	for(a = ldap_first_attribute(e->ld, e->res, &ber);
	    a != NULL;
	    a = ldap_next_attribute(e->ld, e->res, ber)){

	    dprint(9, (debugfile, " %s", a ? a : "?"));
	    if(strcmp(a, e->info_used->cnattr) == 0){
		if(!cn)
		  cn = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, e->info_used->mailattr) == 0){
		if(!mail)
		  mail = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, "electronicmail") == 0){
		if(!elecmail)
		  elecmail = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, "comment") == 0){
		if(!note)
		  note = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, e->info_used->snattr) == 0){
		if(!sn)
		  sn = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, e->info_used->gnattr) == 0){
		if(!givenname)
		  givenname = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, "telephonenumber") == 0){
		if(!telephone)
		  telephone = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, "title") == 0){
		if(!title)
		  title = ldap_get_values(e->ld, e->res, a);
	    }
	}

	if(!save){
	    j = radio_buttons("Save to address book or Export to filesystem ? ",
			      -FOOTER_ROWS(ps), save_or_export, 's', 'x',
			      h_ab_save_exp, RB_NORM, NULL);
	    
	    switch(j){
	      case 'x':
		cancel_warning(NO_DING, "Save");
		break;

	      case 'e':
		export++;
		break;

	      case 's':
		save++;
		break;

	      default:
		q_status_message(SM_ORDER, 3, 3,
				 "can't happen in save_ldap_ent");
		break;
	    }
	}
    }

    if(elecmail){
	if(elecmail[0] && elecmail[0][0] && !mail)
	  mail = elecmail;
	else
	  ldap_value_free(elecmail);

	elecmail = NULL;
    }

    if(save){				/* save into the address book */
	ADDRESS *addr, *a, *a2;
	char    *tmp_a_string,
		*d,
		*fakedomain = "@";
	size_t   len;
	char   **cm;
	int      how_many_selected = 0;

	
	if(cn && cn[0] && cn[0][0])
	  fullname = cpystr(cn[0]);
	if(sn && sn[0] && sn[0][0])
	  last = cpystr(sn[0]);
	if(givenname && givenname[0] && givenname[0][0])
	  first = cpystr(givenname[0]);

	if(note && note[0] && note[0][0])
	  cm = note;
	else
	  cm = title;

	for(ll = cm, len = 0; ll && *ll; ll++)
	  len += strlen(*ll) + 2;

	if(len){
	    comment = (char *)fs_get(len * sizeof(char));
	    d = comment;
	    ll = cm;
	    while(*ll){
		sstrcpy(&d, *ll);
		ll++;
		if(*ll)
		  sstrcpy(&d, "; ");
	    }
	}

	for(ll = mail, len = 0; ll && *ll; ll++)
	  len += strlen(*ll) + 1;
	
	/* paste the email addresses together */
	if(len){
	    address = (char *)fs_get(len * sizeof(char));
	    d = address;
	    ll = mail;
	    while(*ll){
		sstrcpy(&d, *ll);
		ll++;
		if(*ll)
		  sstrcpy(&d, ",");
	    }
	}

	addr = NULL;
	if(address)
	  rfc822_parse_adrlist(&addr, address, fakedomain);

	if(addr && fullname && !(first && last)){
	    if(addr->personal)
	      fs_give((void **)&addr->personal);
	    
	    addr->personal = cpystr(fullname);
	}

	if(addr && e->serv && *e->serv){	/* save by reference */
	    char *dn, *edn = NULL;

	    dn = ldap_get_dn(e->ld, e->res);
	    if(dn){
		edn = add_backslash_escapes(dn);
		our_ldap_dn_memfree(dn);
	    }

	    if(e->serv && *e->serv && edn && *edn){
		char  buf[MAILTMPLEN+1];
		char *backup_mail = NULL;

		how_many_selected++;

		if(addr && addr->mailbox && addr->host){
		    strncpy(buf, addr->mailbox, sizeof(buf)-2),
		    buf[sizeof(buf)-2] = '\0';
		    strcat(buf, "@");
		    strncat(buf, addr->host, sizeof(buf)-1-strlen(buf));
		    buf[sizeof(buf)-1] = '\0';
		    backup_mail = cpystr(buf);
		}

		/*
		 * We only need one addr which we will use to hold the
		 * pointer to the query.
		 */
		if(addr->next)
		  mail_free_address(&addr->next);

		if(addr->mailbox)
		  fs_give((void **)&addr->mailbox);
		if(addr->host)
		  fs_give((void **)&addr->host);
		if(addr->personal)
		  fs_give((void **)&addr->personal);

		sprintf(buf,
	       "%s%.100s /base=%.500s/scope=base/cust=(objectclass=*)%s%.100s",
			RUN_LDAP,
			e->serv,
			edn,
			backup_mail ? "/mail=" : "",
			backup_mail ? backup_mail : "");

		if(backup_mail)
		  fs_give((void **)&backup_mail);

		/*
		 * Put the search parameters in mailbox and put @ in
		 * host so that expand_address accepts it as an unqualified
		 * address and doesn't try to add localdomain.
		 */
		addr->mailbox = cpystr(buf);
		addr->host    = cpystr("@");
	    }

	    if(edn)
	      fs_give((void **)&edn);
	}
	else{					/* save by value */
	    how_many_selected++;
	    if(!addr)
	      addr = mail_newaddr();
	}

	if(how_many_selected > 0){
	    TA_S  *current = NULL;

	    /*
	     * The empty string for the last argument is significant.
	     * Fill_in_ta copies the whole adrlist into a single TA if
	     * there is a print string.
	     */
	    current = fill_in_ta(&current, addr, 1, "");
	    current = first_sel_taline(current);

	    if(first && last && current){
		char *p;

		p = (char *)fs_get(strlen(last) + 2 + strlen(first) +1);
		sprintf(p, "%s, %s", last, first);
		current->fullname = p;
	    }

	    if(comment && current)
	      current->comment = cpystr(comment);
	    
	    /*
	     * We don't want the personal name to make it into the address
	     * field in an LDAP: query sort of address, so move it
	     * out of the addr.
	     */
	    if(e->serv && *e->serv && current && fullname){
		if(current->fullname)
		  fs_give((void **)&current->fullname);

		current->fullname = fullname;
		fullname = NULL;
	    }

	    mail_free_address(&addr);

	    if(current)
	      takeaddr_bypass(ps, current, NULL);
	    else
	      q_status_message(SM_ORDER, 0, 4, "Nothing to save");
	}
	else
	  q_status_message(SM_ORDER, 0, 4, "Nothing to save");

    }
    else if(export){				/* export to filesystem */
	STORE_S *store = NULL, *srcstore = NULL;
	SourceType srctype;
	static ESCKEY_S text_or_vcard[] = {
	    {'t', 't', "T", "Text"},
	    {'a', 'a', "A", "Addresses"},
	    {'v', 'v', "V", "VCard"},
	    {-1, 0, NULL, NULL}};

	j = radio_buttons(
		"Export text of entry, address, or VCard format ? ",
		-FOOTER_ROWS(ps), text_or_vcard, 't', 'x',
		h_ldap_text_or_vcard, RB_NORM, NULL);
	
	switch(j){
	  case 'x':
	    cancel_warning(NO_DING, "Export");
	    break;
	
	  case 't':
	    srctype = CharStar;
	    if(!(srcstore = prep_ldap_for_viewing(ps, e, srctype, NULL)))
	      q_status_message(SM_ORDER, 0, 2, "Error allocating space");
	    else{
		(void)simple_export(ps_global, so_text(srcstore), srctype,
				    "text", NULL);
		so_give(&srcstore);
	    }

	    break;
	
	  case 'a':
	    if(mail && mail[0] && mail[0][0]){

		srctype = CharStar;
		if(!(srcstore = so_get(srctype, NULL, EDIT_ACCESS)))
		  q_status_message(SM_ORDER,0,2, "Error allocating space");
		else{
		    ADDRESS *a, *b;

		    for(ll = mail; ll && *ll; ll++){
			so_puts(srcstore, *ll);
			so_puts(srcstore, "\n");
		    }

		    (void)simple_export(ps_global, so_text(srcstore),
					srctype, "addresses", NULL);
		    so_give(&srcstore);
		}
	    }

	    break;
	
	  case 'v':
	    srctype = CharStar;
	    if(!(srcstore = so_get(srctype, NULL, EDIT_ACCESS)))
	      q_status_message(SM_ORDER,0,2, "Error allocating space");
	    else{
		gf_io_t       pc;
		VCARD_INFO_S *vinfo;

		vinfo = (VCARD_INFO_S *)fs_get(sizeof(VCARD_INFO_S));
		memset((void *)vinfo, 0, sizeof(VCARD_INFO_S));

		if(cn && cn[0] && cn[0][0])
		  vinfo->fullname = copy_list_array(cn);

		if(note && note[0] && note[0][0])
		  vinfo->note = copy_list_array(note);

		if(title && title[0] && title[0][0])
		  vinfo->title = copy_list_array(title);

		if(telephone && telephone[0] && telephone[0][0])
		  vinfo->tel = copy_list_array(telephone);

		if(mail && mail[0] && mail[0][0])
		  vinfo->email = copy_list_array(mail);

		if(sn && sn[0] && sn[0][0])
		  vinfo->last = cpystr(sn[0]);

		if(givenname && givenname[0] && givenname[0][0])
		  vinfo->first = cpystr(givenname[0]);

		gf_set_so_writec(&pc, srcstore);

		write_single_vcard_entry(ps_global, pc, 0, NULL, vinfo);

		free_vcard_info(&vinfo);

		(void)simple_export(ps_global, so_text(srcstore),
				    srctype, "vcard text", NULL);
		so_give(&srcstore);
	    }

	    break;
	
	  default:
	    q_status_message(SM_ORDER, 3, 3, "can't happen in text_or_vcard");
	    break;
	}
    }

    if(cn)
      ldap_value_free(cn);
    if(mail)
      ldap_value_free(mail);
    if(elecmail)
      ldap_value_free(elecmail);
    if(note)
      ldap_value_free(note);
    if(sn)
      ldap_value_free(sn);
    if(givenname)
      ldap_value_free(givenname);
    if(telephone)
      ldap_value_free(telephone);
    if(title)
      ldap_value_free(title);
    if(fullname)
      fs_give((void **)&fullname);
    if(address)
      fs_give((void **)&address);
    if(first)
      fs_give((void **)&first);
    if(last)
      fs_give((void **)&last);
    if(comment)
      fs_give((void **)&comment);
}
#endif	/* ENABLE_LDAP */

#ifdef _WINDOWS

int
ta_scroll_up(count)
     long count;
{
  if(count<0)
    return(ta_scroll_down(-count));
  else if (count){
    long i=count;
    TA_S *next_sel;

    while(i && ta_screen->top_line->next){
      if(ta_screen->top_line ==  ta_screen->current){
	if(next_sel = next_sel_taline(ta_screen->current)){
	  ta_screen->current = next_sel;
	  ta_screen->top_line = next_taline(ta_screen->top_line);
	  i--;
	}
	else i = 0;
      }
      else{
	ta_screen->top_line = next_taline(ta_screen->top_line);
	i--;
      }
    }
  }
  return(TRUE);
}

int
ta_scroll_down(count)
     long count;
{
  if(count < 0)
    return(ta_scroll_up(-count));
  else if (count){
    long i,dline;
    long page_size = ps_global->ttyo->screen_rows - 
                      FOOTER_ROWS(ps_global) - HEADER_ROWS(ps_global);
    TA_S *prev_sel, *ctmp;
    TA_S *first = first_taline(ta_screen->top_line);

    i=count;
    dline=0;
    for(ctmp = ta_screen->top_line; 
	ctmp != ta_screen->current; ctmp = next_taline(ctmp))
      dline++;
    while(i && ta_screen->top_line != first){    
      ta_screen->top_line = pre_taline(ta_screen->top_line);
      i--;
      dline++;
      if(dline >= page_size){
	ctmp = pre_sel_taline(ta_screen->current);
	if(ctmp == NULL){
	  i = 0;
	  ta_screen->top_line = next_taline(ta_screen->top_line);
	}
	else {
	  for(; ctmp != ta_screen->current; 
	      ta_screen->current = pre_taline(ta_screen->current)) 
	    dline--;
	}
      }
    }
  }
  return(TRUE);
}

int ta_scroll_to_pos(line)
     long line;
{
  TA_S *ctmp;
  int dline;

  for(dline = 0, ctmp = first_taline(ta_screen->top_line); 
      ctmp != ta_screen->top_line; ctmp = next_taline(ctmp))
    dline++;

  if (!ctmp)
    dline = 1;

  return(ta_scroll_up(line - dline));
}

int
ta_scroll_callback (cmd, scroll_pos)
     int cmd;
    long scroll_pos;
{
    int paint = TRUE;
    long page_size = ps_global->ttyo->screen_rows - HEADER_ROWS(ps_global)
                       - FOOTER_ROWS(ps_global);

    switch (cmd) {
    case MSWIN_KEY_SCROLLUPLINE:
      paint = ta_scroll_down (scroll_pos);
      break;

    case MSWIN_KEY_SCROLLDOWNLINE:
      paint = ta_scroll_up (scroll_pos);
      break;

    case MSWIN_KEY_SCROLLUPPAGE:
      paint = ta_scroll_down (page_size);
      break;

    case MSWIN_KEY_SCROLLDOWNPAGE:
      paint = ta_scroll_up (page_size);
      break;

    case MSWIN_KEY_SCROLLTO:
      paint = ta_scroll_to_pos (scroll_pos);
      break;
    }

    if(paint)
      update_takeaddr_screen(ps_global, ta_screen->current, ta_screen, (Pos *)NULL);

    return(paint);
}

#endif /* _WINDOWS */








