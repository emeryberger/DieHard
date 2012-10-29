#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: adrbkcmd.c 14092 2005-09-27 21:27:55Z hubert@u.washington.edu $";
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
    adrbkcmds.c
    Commands called from the addrbook screens.
 ====*/


#include "headers.h"
#include "adrbklib.h"

void           ab_compose_internal PROTO((BuildTo, int));
int            ab_export PROTO((struct pine *, long, int, int));
int            ab_modify_abook_list PROTO((int, int, int, char *,
					   char *, char *));
int            any_rule_files_to_warn_about PROTO((struct pine *));
int            cmp_action_list PROTO((const QSType *, const QSType *));
int            do_the_shuffle PROTO((int *, int, int, char **));
int            expand_addrs_for_pico PROTO((struct headerentry *, char ***));
char          *pico_cancel_for_adrbk_edit PROTO((void (*)()));
char          *pico_cancel_for_adrbk_take PROTO((void (*)()));
char          *pico_cancelexit_for_adrbk PROTO((char *, void (*)()));
char          *pico_sendexit_for_adrbk PROTO((struct headerentry *, void(*)(), int));
int            process_abook_view_cmd PROTO((int, MSGNO_S *, SCROLL_S *));
void           set_act_list_member PROTO((ACTION_LIST_S *, a_c_arg_t,
					PerAddrBook *, PerAddrBook *, char *));
char          *view_message_for_pico PROTO((char **));
int            verify_abook_nick PROTO((char *, char **,char **,BUILDER_ARG *,
					int *));
int            verify_addr PROTO((char *, char **, char **, BUILDER_ARG *,
				  int *));
int            verify_folder_name PROTO((char *,char **,char **,BUILDER_ARG *,
					 int *));
int            verify_nick PROTO((char *, char **, char **, BUILDER_ARG *,
				  int *));
int            verify_server_name PROTO((char *,char **,char **,BUILDER_ARG *,
					 int *));
void           write_single_vcard_entry PROTO((struct pine *, gf_io_t,
					       char **, VCARD_INFO_S *));
void           write_single_tab_entry PROTO((gf_io_t, VCARD_INFO_S *));
VCARD_INFO_S  *prepare_abe_for_vcard PROTO((struct pine *, AdrBk_Entry *, int));
void           free_vcard_info PROTO((VCARD_INFO_S **));
int            convert_abook_to_remote PROTO((struct pine *, PerAddrBook *,
					      char *, size_t, int));

#ifdef	ENABLE_LDAP
typedef struct _saved_query {
    char *qq,
	 *cn,
	 *sn,
	 *gn,
	 *mail,
	 *org,
	 *unit,
	 *country,
	 *state,
	 *locality,
	 *custom;
} SAVED_QUERY_S;

char          *ldap_translate PROTO((char *, LDAP_SERV_S *));
int            process_ldap_cmd PROTO((int, MSGNO_S *, SCROLL_S *));
char          *pico_simpleexit PROTO((struct headerentry *, void (*)(), int));
char          *pico_simplecancel PROTO((struct headerentry *, void (*)()));
void           save_query_parameters PROTO((SAVED_QUERY_S *));
SAVED_QUERY_S *copy_query_parameters PROTO((SAVED_QUERY_S *));
void           free_query_parameters PROTO((SAVED_QUERY_S **));
int            restore_query_parameters PROTO((struct headerentry *, char ***));

static char *expander_address;
#endif	/* ENABLE_LDAP */

static char *fakedomain = "@";

#define INDENT 12	/* strlen("Nickname  : ") */

static struct key abook_view_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"<","Abook",{MC_EXIT,2,{'<',','}},KS_NONE},
	{"U","Update",{MC_EDIT,1,{'u'}},KS_NONE},
	{"C","ComposeTo",{MC_COMPOSE,1,{'c'}},KS_COMPOSER},
	RCOMPOSE_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	{"F", "Fwd Email", {MC_FORWARD, 1, {'f'}}, KS_FORWARD},
	SAVE_MENU,

	HELP_MENU,
	OTHER_MENU,
	{"V","ViewLink",{MC_VIEW_HANDLE,3,{'v',ctrl('m'),ctrl('j')}},KS_NONE},
	NULL_MENU,
	{"^B","PrevLink",{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F","NextLink",{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(abook_view_keymenu, abook_view_keys);
#define	ABV_OTHER	1

static struct key abook_text_keys[] =
       {HELP_MENU,
	NULL_MENU,
	{"E","Exit Viewer",{MC_EXIT,1,{'e'}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", "Save", {MC_SAVETEXT,1,{'s'}}, KS_SAVE}};
INST_KEY_MENU(abook_text_km, abook_text_keys);

#define	VIEW_ABOOK_NONE		0
#define	VIEW_ABOOK_EDITED	1
#define	VIEW_ABOOK_WARPED	2

/*
 * View an addrbook entry.
 * Call scrolltool to do the work.
 */
void
view_abook_entry(ps, cur_line)
    struct pine *ps;
    long         cur_line;
{
    AdrBk_Entry *abe;
    STORE_S    *in_store, *out_store;
    char       *string, *errstr;
    SCROLL_S	sargs;
    HANDLE_S   *handles = NULL;
    gf_io_t	pc, gc;
    int         cmd;
    long	offset = 0L;

    dprint(5, (debugfile, "- view_abook_entry -\n"));

repaint_view:
    if(is_addr(cur_line)){
	abe = ae(cur_line);
	if(!abe){
	    q_status_message(SM_ORDER, 0, 3, "Error reading entry");
	    return;
	}
    }
    else{
	q_status_message(SM_ORDER, 0, 3, "Nothing to view");
	return;
    }

    if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3, "Error allocating space.");
	return;
    }

    so_puts(in_store, "Nickname  : ");
    if(abe->nickname)
      so_puts(in_store, (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					       SIZEOF_20KBUF,
					       abe->nickname, NULL));

    so_puts(in_store, "\015\012");

    so_puts(in_store, "Fullname  : ");
    if(abe->fullname)
      so_puts(in_store, (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					       SIZEOF_20KBUF,
					       abe->fullname, NULL));

    so_puts(in_store, "\015\012");

    so_puts(in_store, "Fcc       : ");
    if(abe->fcc)
      so_puts(in_store, (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					       SIZEOF_20KBUF,
					       abe->fcc, NULL));

    so_puts(in_store, "\015\012");

    so_puts(in_store, AB_COMMENT_STR);
    if(abe->extra){
	size_t n, len;
	unsigned char *p, *tmp = NULL;

	if((n = strlen(abe->extra)) > SIZEOF_20KBUF-1){
	    len = n+1;
	    p = tmp = (unsigned char *)fs_get(len * sizeof(char));
	}
        else{
	    len = SIZEOF_20KBUF;
	    p = (unsigned char *)tmp_20k_buf;
	}

        so_puts(in_store, (char *)rfc1522_decode(p, len, abe->extra, NULL));

	if(tmp)
	  fs_give((void **)&tmp);
    }

    so_puts(in_store, "\015\012");

    so_puts(in_store, "Addresses : ");
    if(abe->tag == Single){
	char *tmp = abe->addr.addr ? abe->addr.addr : "";
#ifdef	ENABLE_LDAP
	if(!strncmp(tmp, QRUN_LDAP, LEN_QRL))
	  string = LDAP_DISP;
	else
#endif
	  string = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					  SIZEOF_20KBUF, tmp, NULL);

	so_puts(in_store, string);
    }
    else{
	char **ll;

	for(ll = abe->addr.list; ll && *ll; ll++){
	    if(ll != abe->addr.list){
		so_puts(in_store, "\015\012");
		so_puts(in_store, repeat_char(INDENT, SPACE));
	    }

#ifdef	ENABLE_LDAP
	    if(!strncmp(*ll, QRUN_LDAP, LEN_QRL))
	      string = LDAP_DISP;
	    else
#endif
	      string = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					      SIZEOF_20KBUF, *ll, NULL);
	    so_puts(in_store, string);

	    if(*(ll+1))	/* not the last one */
	      so_puts(in_store, ",");
	}
    }

    so_puts(in_store, "\015\012");

    do{
	if(!(out_store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    so_give(&in_store);
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Error allocating space.");
	    return;
	}

	so_seek(in_store, 0L, 0);

	init_handles(&handles);
	gf_filter_init();

	if(F_ON(F_VIEW_SEL_URL, ps_global)
	   || F_ON(F_VIEW_SEL_URL_HOST, ps_global)
	   || F_ON(F_SCAN_ADDR, ps_global))
	  gf_link_filter(gf_line_test,
			 gf_line_test_opt(url_hilite_abook, &handles));

	gf_link_filter(gf_wrap, gf_wrap_filter_opt(ps->ttyo->screen_cols - 4,
						   ps->ttyo->screen_cols,
						   NULL, INDENT,
						   GFW_HANDLES));
	gf_link_filter(gf_nvtnl_local, NULL);

	gf_set_so_readc(&gc, in_store);
	gf_set_so_writec(&pc, out_store);

	if(errstr = gf_pipe(gc, pc)){
	    so_give(&in_store);
	    so_give(&out_store);
	    free_handles(&handles);
	    q_status_message1(SM_ORDER | SM_DING, 3, 3,
			      "Can't format entry : %.200s", errstr);
	    return;
	}

	gf_clear_so_writec(out_store);
	gf_clear_so_readc(in_store);

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text	   = so_text(out_store);
	sargs.text.src	   = CharStar;
	sargs.text.desc	   = "expanded entry";
	sargs.text.handles = handles;

	if(offset){		/* resize?  preserve paging! */
	    sargs.start.on	   = Offset;
	    sargs.start.loc.offset = offset;
	    offset = 0L;
	}
	  
	sargs.bar.title	  = "ADDRESS BOOK (View)";
	sargs.bar.style	  = TextPercent;
	sargs.proc.tool	  = process_abook_view_cmd;
	sargs.proc.data.i = VIEW_ABOOK_NONE;
	sargs.resize_exit = 1;
	sargs.help.text	  = h_abook_view;
	sargs.help.title  = "HELP FOR ADDRESS BOOK VIEW";
	sargs.keys.menu	  = &abook_view_keymenu;
	setbitmap(sargs.keys.bitmap);

	if(handles)
	  sargs.keys.menu->how_many = 2;
	else{
	    sargs.keys.menu->how_many = 1;
	    clrbitn(ABV_OTHER, sargs.keys.bitmap);
	}

	if((cmd = scrolltool(&sargs)) == MC_RESIZE)
	  offset = sargs.start.loc.offset;

	so_give(&out_store);
	free_handles(&handles);
    }
    while(cmd == MC_RESIZE);

    so_give(&in_store);

    if(sargs.proc.data.i != VIEW_ABOOK_NONE){
	long old_l_p_p, old_top_ent, old_cur_row;

	if(sargs.proc.data.i == VIEW_ABOOK_WARPED){
	    /*
	     * Warped means we got plopped down somewhere in the display
	     * list so that we don't know where we are relative to where
	     * we were before we warped.  The current line number will
	     * be zero, since that is what the warp would have set.
	     */
	    as.top_ent = first_line(0L - as.l_p_page/2L);
	    as.cur_row = 0L - as.top_ent;
	}
	else if(sargs.proc.data.i == VIEW_ABOOK_EDITED){
	    old_l_p_p   = as.l_p_page;
	    old_top_ent = as.top_ent;
	    old_cur_row = as.cur_row;
	}

	/* Window size may have changed while in pico. */
	ab_resize();

	/* fix up what ab_resize messed up */
	if(sargs.proc.data.i != VIEW_ABOOK_WARPED && old_l_p_p == as.l_p_page){
	    as.top_ent     = old_top_ent;
	    as.cur_row     = old_cur_row;
	    as.old_cur_row = old_cur_row;
	}

	cur_line = as.top_ent+as.cur_row;
	/*
	 * We've changed our opinion about how this should work. Now, instead
	 * of updating and then ending back up at the view screen, you update
	 * and end up back out at the index of entries screen.
	 */
    /*****
	goto repaint_view;
      *****/
    }

    ps->mangled_screen = 1;
}


int
process_abook_view_cmd(cmd, msgmap, sparms)
    int	       cmd;
    MSGNO_S   *msgmap;
    SCROLL_S  *sparms;
{
    int rv = 1, i;
    PerAddrBook *pab;
    AddrScrn_Disp *dl;
    static ESCKEY_S text_or_vcard[] = {
	{'t', 't', "T", "Text"},
	{'v', 'v', "V", "VCard"},
	{-1, 0, NULL, NULL}};

    ps_global->next_screen = SCREEN_FUN_NULL;

    switch(cmd){
      case MC_EDIT :
	/*
	 * MC_EDIT works a little differently from the other cmds here.
	 * The others return 0 to scrolltool so that we are still in
	 * the view screen. This one is different because we may have
	 * changed what we're viewing. We handle that by returning 1
	 * to scrolltool and setting the sparms opt union's gint
	 * to the value below.
	 *
	 * (Late breaking news. Now we're going to return 1 from all these
	 * commands and then in the caller we're going to bounce back out
	 * to the index view instead of the view of the individual entry.
	 * So there is some dead code around for now.)
	 * 
	 * Then, in the view_abook_entry function we check the value
	 * of this field on scrolltool's return and if it is one of
	 * the two special values below view_abook_entry resets the
	 * current line if necessary, flushes the display cache (in
	 * ab_resize) and loops back and starts over, effectively
	 * putting us back in the view screen but with updated
	 * contents. A side effect is that the screen above that (the
	 * abook index) will also have been flushed and corrected by
	 * the ab_resize.
	 */
	pab = &as.adrbks[cur_addr_book()];
	if(pab && pab->access == ReadOnly){
	    readonly_warning(NO_DING, NULL);
	    rv = 0;
	    break;
	}

	if(adrbk_check_all_validity_now()){
	    if(resync_screen(pab, AddrBookScreen, 0)){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
		 "Address book changed. Update cancelled. Try again.");
		ps_global->mangled_screen = 1;
		break;
	    }
	}

	if(pab && pab->access == ReadOnly){
	    readonly_warning(NO_DING, NULL);
	    rv = 0;
	    break;
	}

	/*
	 * Arguments all come from globals, not from the arguments to
	 * process_abook_view_cmd. It would be a little cleaner if the
	 * information was contained in att, I suppose.
	 */
	if(is_addr(as.cur_row+as.top_ent)){
	    AdrBk_Entry *abe, *abe_copy;
	    a_c_arg_t    entry;
	    int          warped = 0;

	    dl = dlist(as.top_ent+as.cur_row);
	    entry = dl->elnum;
	    abe = adrbk_get_ae(pab->address_book, entry, Normal);
	    abe_copy = copy_ae(abe);
	    dprint(9, (debugfile,"Calling edit_entry to edit from view\n"));
	    edit_entry(pab->address_book, abe_copy, entry,
		       abe->tag, 0, &warped, "update");
	    /*
	     * The ABOOK_EDITED case doesn't mean that we necessarily
	     * changed something, just that we might have but we know
	     * we didn't change the sort order (causing the warp).
	     */
	    sparms->proc.data.i = warped
				   ? VIEW_ABOOK_WARPED : VIEW_ABOOK_EDITED;

	    free_ae(&abe_copy);
	    rv = 1;			/* causes scrolltool to return */
	}
	else{
	    q_status_message(SM_ORDER, 0, 4,
			     "Something wrong, entry not updateable");
	    rv = 0;
	    break;
	}

	break;

      case MC_SAVE :
	pab = &as.adrbks[cur_addr_book()];
	/*
	 * Arguments all come from globals, not from the arguments to
	 * process_abook_view_cmd. It would be a little cleaner if the
	 * information was contained in att, I suppose.
	 */
	(void)ab_save(ps_global, pab ? pab->address_book : NULL,
		      as.top_ent+as.cur_row, -FOOTER_ROWS(ps_global), 0);
	rv = 1;
	break;

      case MC_COMPOSE :
	(void)ab_compose_to_addr(as.top_ent+as.cur_row, 0, 0);
	rv = 1;
	break;

      case MC_ROLE :
	(void)ab_compose_to_addr(as.top_ent+as.cur_row, 0, 1);
	rv = 1;
	break;

      case MC_FORWARD :
	rv = 1;
	i = radio_buttons("Forward as text or forward as Vcard attachment ? ",
			  -FOOTER_ROWS(ps_global), text_or_vcard, 't', 'x',
			  h_ab_text_or_vcard, RB_NORM, NULL);
	switch(i){
	  case 'x':
	    cancel_warning(NO_DING, "forward");
	    rv = 0;
	    break;

	  case 't':
	    forward_text(ps_global, sparms->text.text, sparms->text.src);
	    break;

	  case 'v':
	    (void)ab_forward(ps_global, as.top_ent+as.cur_row, 0);
	    break;

	  default:
	    q_status_message(SM_ORDER, 3, 3,
			     "can't happen in process_abook_view_cmd");
	    break;
	}

	break;

      default:
	panic("Unexpected command in process_abook_view_cmd");
	break;
    }

    return(rv);
}


/*
 * Give expanded view of this address entry.
 * Call scrolltool to do the work.
 *
 * Args: headents -- The headerentry array from pico.
 *              s -- Unused here.
 *
 * Returns -- Always 0.
 */
int
expand_addrs_for_pico(headents, s)
    struct headerentry *headents;
    char             ***s;
{
    BuildTo      bldto;
    STORE_S     *store;
    char        *error = NULL, *addr = NULL, *fullname = NULL, *address = NULL;
    SAVE_STATE_S state;
    ADDRESS     *adrlist = NULL, *a;
    int          j, address_index = -1, fullname_index = -1, no_a_fld = 0;
    void       (*redraw)() = ps_global->redrawer;
    static char *fakeaddrpmt = "Address   : ";
    char        *tmp, *tmp2, *tmp3;
    SCROLL_S	 sargs;
    AdrBk_Entry  abe;

    dprint(5, (debugfile, "- expand_addrs_for_pico -\n"));

    if(s)
      *s = NULL;

    ps_global->redrawer = NULL;
    fix_windsize(ps_global);

    ab_nesting_level++;
    save_state(&state);

    for(j=0;
	headents[j].name != NULL && (address_index < 0 || fullname_index < 0);
	j++){
	if(strncmp(headents[j].name, "Address", 7) == 0)
	  address_index = j;
	else if(strncmp(headents[j].name, "Fullname", 8) == 0)
	  fullname_index = j;
    }

    if(address_index >= 0)
      address = *headents[address_index].realaddr;
    else{
	address_index = 1000;	/* a big number */
	no_a_fld++;
    }

    if(fullname_index >= 0)
      fullname = adrbk_formatname(*headents[fullname_index].realaddr,
				  NULL, NULL);

    memset(&abe, 0, sizeof(abe));
    if(fullname)
      abe.fullname = cpystr(fullname);
    
    if(address){
	char *tmp_a_string;

	tmp_a_string = cpystr(address);
	rfc822_parse_adrlist(&adrlist, tmp_a_string, fakedomain);
	fs_give((void **)&tmp_a_string);
	if(adrlist && adrlist->next){
	    int cnt = 0;

	    abe.tag = List;
	    for(a = adrlist; a; a = a->next)
	      cnt++;
	    
	    abe.addr.list = (char **)fs_get((cnt+1) * sizeof(char *));
	    cnt = 0;
	    for(a = adrlist; a; a = a->next){
		if(a->host && a->host[0] == '@')
		  abe.addr.list[cnt++] = cpystr(a->mailbox);
		else if(a->host && a->host[0] && a->mailbox && a->mailbox[0])
		  abe.addr.list[cnt++] =
				  cpystr(simple_addr_string(a, tmp_20k_buf,
							    SIZEOF_20KBUF));
	    }

	    abe.addr.list[cnt] = '\0';
	}
	else{
	    abe.tag = Single;
	    abe.addr.addr = address;
	}

	if(adrlist)
	  mail_free_address(&adrlist);

	bldto.type = Abe;
	bldto.arg.abe = &abe;
	our_build_address(bldto, &addr, &error, NULL, 0);
	if(error){
	    q_status_message1(SM_ORDER, 3, 4, "%.200s", error);
	    fs_give((void **)&error);
	}
	
	if(addr){
	    tmp_a_string = cpystr(addr);
	    rfc822_parse_adrlist(&adrlist, tmp_a_string, ps_global->maildomain);
	    fs_give((void **)&tmp_a_string);
	}
    }
#ifdef	ENABLE_LDAP
    else if(no_a_fld && expander_address){
	WP_ERR_S wp_err;

	memset(&wp_err, 0, sizeof(wp_err));
	adrlist = wp_lookups(expander_address, &wp_err, 0);
	if(fullname && *fullname){
	    if(adrlist){
		if(adrlist->personal)
		  fs_give((void **)&adrlist->personal);
		
		adrlist->personal = cpystr(fullname);
	    }
	}

	if(wp_err.error){
	    q_status_message1(SM_ORDER, 3, 4, "%.200s", wp_err.error);
	    fs_give((void **)&wp_err.error);
	}
    }
#endif
    
    if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3, "Error allocating space.");
	restore_state(&state);
	return(0);
    }

    for(j = 0; j < address_index && headents[j].name != NULL; j++){
	if(tmp = fold(*headents[j].realaddr,
		      ps_global->ttyo->screen_cols,
		      ps_global->ttyo->screen_cols,
		      0, 0, headents[j].prompt,
		      repeat_char(headents[j].prlen, SPACE))){
	    so_puts(store, tmp);
	    fs_give((void **)&tmp);
	}
    }

    /*
     * There is an assumption that Addresses is the last field.
     */
    tmp3 = cpystr(repeat_char(headents[0].prlen + 2, SPACE));
    if(adrlist){
	for(a = adrlist; a; a = a->next){
	    ADDRESS *next_addr;
	    char    *bufp;

	    next_addr = a->next;
	    a->next = NULL;
	    bufp = (char *)fs_get((size_t)est_size(a));
	    tmp2 = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf+5000,
					  SIZEOF_20KBUF-5000,
					  addr_string(a, bufp), NULL);
	    a->next = next_addr;

	    /*
	     * Another assumption, all the prlens are the same.
	     */
	    if(tmp = fold(tmp2,
			  ps_global->ttyo->screen_cols,
			  ps_global->ttyo->screen_cols,
			  0, 0,
			  (a == adrlist) ? (no_a_fld
					      ? fakeaddrpmt
					      : headents[address_index].prompt)
					 : tmp3+2,
			  tmp3)){
		so_puts(store, tmp);
		fs_give((void **)&tmp);
	    }

	    fs_give((void **)&bufp);
	}
    }
    else{
	tmp2 = NULL;
	if(tmp = fold(tmp2=cpystr("<none>"),
		      ps_global->ttyo->screen_cols,
		      ps_global->ttyo->screen_cols,
		      0, 0,
		      no_a_fld ? fakeaddrpmt
			       : headents[address_index].prompt,
		      tmp3)){
	    so_puts(store, tmp);
	    fs_give((void **)&tmp);
	}

	if(tmp2)
	  fs_give((void **)&tmp2);
    }

    if(tmp3)
      fs_give((void **)&tmp3);

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text = so_text(store);
    sargs.text.src  = CharStar;
    sargs.text.desc = "expanded entry";
    sargs.bar.title = "ADDRESS BOOK (Rich View)";
    sargs.bar.style = TextPercent;
    sargs.keys.menu = &abook_text_km;
    setbitmap(sargs.keys.bitmap);

    scrolltool(&sargs);
    so_give(&store);

    restore_state(&state);
    ab_nesting_level--;

    if(addr)
      fs_give((void **)&addr);
    
    if(adrlist)
      mail_free_address(&adrlist);

    if(fullname)
      fs_give((void **)&fullname);

    if(abe.fullname)
      fs_give((void **)&abe.fullname);

    if(abe.tag == List && abe.addr.list)
      free_list_array(&(abe.addr.list));

    ps_global->redrawer = redraw;

    return(0);
}


/*
 * Callback from TakeAddr editing screen to see message that was being
 * viewed.  Call scrolltool to do the work.
 */
char *
view_message_for_pico(error)
    char **error;
{
    STORE_S     *store;
    gf_io_t      pc;
    void       (*redraw)() = ps_global->redrawer;
    SourceType   src = CharStar;
    SCROLL_S	 sargs;

    dprint(5, (debugfile, "- view_message_for_pico -\n"));

    ps_global->redrawer = NULL;
    fix_windsize(ps_global);

#ifdef DOS
    src = TmpFileStar;
#endif

    if(!(store = so_get(src, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3, "Error allocating space.");
	return(NULL);
    }

    gf_set_so_writec(&pc, store);

    format_message(msgno_for_pico_callback, env_for_pico_callback,
		   body_for_pico_callback, NULL, FM_NEW_MESS | FM_DISPLAY, pc);

    gf_clear_so_writec(store);

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text = so_text(store);
    sargs.text.src  = src;
    sargs.text.desc = "expanded entry";
    sargs.bar.title = "MESSAGE TEXT";
    sargs.bar.style = TextPercent;
    sargs.keys.menu = &abook_text_km;
    setbitmap(sargs.keys.bitmap);

    scrolltool(&sargs);

    so_give(&store);

    ps_global->redrawer = redraw;

    return(NULL);
}


/*
prompt::name::help::prlen::maxlen::realaddr::
builder::affected_entry::next_affected::selector::key_label::fileedit::
display_it::break_on_comma::is_attach::rich_header::only_file_chars::
single_space::sticky::dirty::start_here::blank::KS_ODATAVAR
*/
static struct headerentry headents_for_edit[]={
  {"Nickname  : ",  "Nickname",  h_composer_abook_nick, 12, 0, NULL,
   verify_nick,   NULL, NULL, addr_book_nick_for_edit, "To AddrBk", NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Fullname  : ",  "Fullname",  h_composer_abook_full, 12, 0, NULL,
   NULL,          NULL, NULL, view_message_for_pico,   "To Message", NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Fcc       : ",  "FileCopy",  h_composer_abook_fcc, 12, 0, NULL,
   NULL,          NULL, NULL, folders_for_fcc,         "To Fldrs", NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Comment   : ",  "Comment",   h_composer_abook_comment, 12, 0, NULL,
   NULL,          NULL, NULL, view_message_for_pico,   "To Message", NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Addresses : ",  "Addresses", h_composer_abook_addrs, 12, 0, NULL,
   verify_addr,   NULL, NULL, addr_book_change_list,   "To AddrBk", NULL,
   1, 1, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {NULL, NULL, NO_HELP, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
};
#define N_NICK    0
#define N_FULL    1
#define N_FCC     2
#define N_COMMENT 3
#define N_ADDR    4
#define N_END     5

static char        *nick_saved_for_pico_check;
static AdrBk       *abook_saved_for_pico_check;

/*
 * Args: abook  -- Address book handle
 *       abe    -- AdrBk_Entry of old entry to work on.  If NULL, this will
 *                 be a new entry.  This has to be a pointer to a copy of
 *                 an abe that won't go away until we finish this function.
 *                 In other words, don't pass in a pointer to an abe in
 *                 the cache, copy it first.  The tag on this abe is only
 *                 used to decide whether to read abe->addr.list or
 *                 abe->addr.addr, not to determine what the final result
 *                 will be.  That's determined solely by how many addresses
 *                 there are after the user edits.
 *       entry  -- The entry number of the old entry that we will be changing.
 *      old_tag -- If we're changing an old entry, then this is the tag of
 *                 that old entry.
 *     readonly -- Call pico with readonly flag
 *       warped -- We warped to a new part of the addrbook
 *                 (We also overload warped in a couple places and use it's
 *                  being set as an indicator of whether we are Taking or
 *                  not.  It will be NULL if we are Taking.)
 */
void
edit_entry(abook, abe, entry, old_tag, readonly, warped, cmd)
    AdrBk       *abook;
    AdrBk_Entry *abe;
    a_c_arg_t    entry;
    Tag          old_tag;
    int          readonly;
    int         *warped;
    char        *cmd;
{
    AdrBk_Entry local_abe;
    struct headerentry *he;
    PICO pbf;
    STORE_S *msgso;
    adrbk_cntr_t old_entry_num, new_entry_num = NO_NEXT;
    size_t n, len;
    unsigned char *ptmp, *tmp = NULL;
    int rc = 0, resort_happened = 0, list_changed = 0, which_addrbook;
    int editor_result, i = 0, add, n_end, ldap = 0;
    char *nick, *full, *fcc, *comment, *dcomment, *fname, *dummy, *pp;
    char **orig_encoded = NULL, **orig_decoded = NULL;
    char **new_encoded = NULL, **new_decoded = NULL;
    char *addr_encoded  = NULL, *addr_decoded = NULL;
    char **p, **q;
    Tag new_tag;
    char titlebar[40];
    long length;
    SAVE_STATE_S   state;  /* For saving state of addrbooks temporarily */

    dprint(2, (debugfile, "- edit_entry -\n"));

    old_entry_num = (adrbk_cntr_t)entry;
    save_state(&state);
    abook_saved_for_pico_check = abook;

    add = (abe == NULL);  /* doing add or change? */
    if(add){
	local_abe.nickname  = "";
	local_abe.fullname  = "";
	local_abe.fcc       = "";
	local_abe.extra     = "";
	local_abe.addr.addr = "";
	local_abe.tag       = NotSet;
	abe = &local_abe;
	old_entry_num = NO_NEXT;
    }

    new_tag = abe->tag;

#ifdef	ENABLE_LDAP
    expander_address = NULL;
    if(abe->tag == Single &&
       abe->addr.addr &&
       !strncmp(abe->addr.addr,QRUN_LDAP,LEN_QRL)){
	ldap = 1;
	expander_address = cpystr(abe->addr.addr);
	(void)removing_double_quotes(expander_address);
    }
    else if(abe->tag == List &&
	    abe->addr.list &&
	    abe->addr.list[0] &&
	    !abe->addr.list[1] &&
            !strncmp(abe->addr.list[0],QRUN_LDAP,LEN_QRL)){
	ldap = 2;
	expander_address = cpystr(abe->addr.list[0]);
	(void)removing_double_quotes(expander_address);
    }
#endif

    standard_picobuf_setup(&pbf);
    pbf.exittest      = pico_sendexit_for_adrbk;
    pbf.canceltest    = warped ? pico_cancel_for_adrbk_edit
				: pico_cancel_for_adrbk_take;
    pbf.expander      = expand_addrs_for_pico;
    pbf.ctrlr_label   = "RichView";
    sprintf(titlebar, "ADDRESS BOOK (%c%.20s)",
	    readonly ? 'V' : islower((unsigned char)(*cmd))
				    ? toupper((unsigned char)*cmd)
				    : *cmd,
	    readonly ? "iew" : (cmd+1));
    pbf.pine_anchor   = set_titlebar(titlebar,
				      ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,ps_global->msgmap, 
				      0, FolderName, 0, 0, NULL);
    pbf.pine_flags   |= P_NOBODY;
    if(readonly)
      pbf.pine_flags |= P_VIEW;

    /* An informational message */
    if(msgso = so_get(PicoText, NULL, EDIT_ACCESS)){
	pbf.msgtext = (void *)so_text(msgso);
	/*
	 * It's nice if we can make it so these lines make sense even if
	 * they don't all make it on the screen, because the user can't
	 * scroll down to see them.  So just make each line a whole sentence
	 * that doesn't need the others below it to make sense.
	 */
	if(add){
	    so_puts(msgso,
"\n Fill in the fields. It is ok to leave fields blank.");
	    so_puts(msgso,
"\n To form a list, just enter multiple comma-separated addresses.");
	}
	else{
	    so_puts(msgso,
"\n Change any of the fields. It is ok to leave fields blank.");
	    if(ldap)
	      so_puts(msgso,
"\n Since this entry does a directory lookup you may not edit the address field.");
	    else
	      so_puts(msgso,
"\n Additional comma-separated addresses may be entered in the address field.");
	}

	so_puts(msgso,
"\n Press \"^X\" to save the entry, \"^C\" to cancel, \"^G\" for help.");
	so_puts(msgso,
"\n If you want to use quotation marks inside the Fullname field, it is best");
	so_puts(msgso,
"\n to use single quotation marks; for example: George 'Husky' Washington.");
    }

    he = (struct headerentry *)fs_get((N_END+1) * sizeof(struct headerentry));
    memset((void *)he, 0, (N_END+1) * sizeof(struct headerentry));
    pbf.headents      = he;

    /* make a copy of each field */
    nick = cpystr(abe->nickname);
    removing_leading_and_trailing_white_space(nick);
    nick_saved_for_pico_check = cpystr(nick);
    he[N_NICK]          = headents_for_edit[N_NICK];
    he[N_NICK].realaddr = &nick;

    dummy = NULL;
    full = cpystr((char *)rfc1522_decode((unsigned char *)tmp_20k_buf+10000,
					 SIZEOF_20KBUF-10000,
				         abe->fullname, &dummy));
    removing_leading_and_trailing_white_space(full);

    if(dummy)
      fs_give((void **)&dummy);

    he[N_FULL]          = headents_for_edit[N_FULL];
    he[N_FULL].realaddr = &full;

    fcc = cpystr(abe->fcc);
    removing_leading_and_trailing_white_space(fcc);
    he[N_FCC]          = headents_for_edit[N_FCC];
    he[N_FCC].realaddr = &fcc;

    if((n = strlen(abe->extra)) > SIZEOF_20KBUF-1){
	len = n+1;
	ptmp = tmp = (unsigned char *)fs_get(len * sizeof(char));
    }
    else{
	len = SIZEOF_20KBUF;
	ptmp = (unsigned char *)tmp_20k_buf;
    }
    
    dummy = NULL;
    dcomment = (char *)rfc1522_decode(ptmp, len, abe->extra, &dummy);
    if(dummy)
      fs_give((void **)&dummy);

    comment = cpystr(dcomment);
    if(tmp)
      fs_give((void **)&tmp);

    removing_leading_and_trailing_white_space(comment);

    he[N_COMMENT]          = headents_for_edit[N_COMMENT];
    he[N_COMMENT].realaddr = &comment;

    n_end = N_END;
    if(ldap)
      n_end--;

    if(!ldap){
	he[N_ADDR]          = headents_for_edit[N_ADDR];
	he[N_ADDR].realaddr = &addr_decoded;
	if(abe->tag == Single){
	    if(abe->addr.addr){
		orig_encoded = (char **)fs_get(2 * sizeof(char *));
		orig_decoded = (char **)fs_get(2 * sizeof(char *));
		orig_encoded[0] = cpystr(abe->addr.addr);
		orig_encoded[1] = NULL;
	    }
	}
	else if(abe->tag == List){
	    if(listmem_count_from_abe(abe) > 0){
		orig_encoded = (char **)fs_get(
				      (size_t)(listmem_count_from_abe(abe) + 1)
					* sizeof(char *));
		orig_decoded = (char **)fs_get(
				      (size_t)(listmem_count_from_abe(abe) + 1)
					* sizeof(char *));
		for(q = orig_encoded, p = abe->addr.list; p && *p; p++, q++)
		  *q = cpystr(*p);
		
		*q = NULL;
	    }
	}

	/*
	 * Orig_encoded is the original list saved in encoded form.
	 * Now save the original list but in decoded form.
	 */
	for(q = orig_decoded, p = orig_encoded; p && *p; p++, q++){
	    /*
	     * Here we have an address string, which we need to parse, then
	     * decode the fullname, possibly quote it, then turn it back into
	     * a string.
	     */
	    *q = decode_fullname_of_addrstring(*p, 0);
	}

	if(q)
	  *q = NULL;

	/* figure out how large a string we need to allocate */
	length = 0L;
	for(p = orig_decoded; p && *p; p++)
	  length += (strlen(*p) + 2);
	
	if(length)
	  length -= 2L;

	pp = addr_decoded = (char *)fs_get((size_t)(length+1L) * sizeof(char));
	*pp = '\0';
	for(p = orig_decoded; p && *p; p++){
	    sstrcpy(&pp, *p);
	    if(*(p+1))
	      sstrcpy(&pp, ", ");
	}

	if(verify_addr(addr_decoded, NULL, NULL, NULL, NULL) < 0)
	  he[N_ADDR].start_here = 1;

	/*
	 * Now we have orig_encoded -- a list of encoded addresses
	 *             orig_decoded -- a list of decoded addresses
	 *             addr_decoded -- orig_decoded put together into a decoded,
	 *				    comma-separated string
	 *             new_decoded will be the edited, decoded list
	 *             new_encoded will be the encoded version of that
	 */
    }

    he[n_end] = headents_for_edit[N_END];
    for(i = 0; i < n_end; i++){
	/* no callbacks in some cases */
	if(readonly || ((i == N_FULL || i == N_COMMENT)
			&& !env_for_pico_callback)){
	    he[i].selector  = NULL;
	    he[i].key_label = NULL;
	}

	/* no builders for readonly */
	if(readonly)
	  he[i].builder = NULL;
    }

    /* pass to pico and let user change them */
    editor_result = pico(&pbf);
    ps_global->mangled_screen = 1;
    standard_picobuf_teardown(&pbf);

    if(editor_result & COMP_GOTHUP)
      hup_signal();
    else{
	fix_windsize(ps_global);
	init_signals();
    }

    if(editor_result & COMP_CANCEL){
	if(!readonly)
	  cancel_warning(NO_DING, cmd);
    }
    else if(editor_result & COMP_EXIT){
	removing_leading_and_trailing_white_space(nick);
	removing_leading_and_trailing_white_space(full);
	removing_leading_and_trailing_white_space(fcc);
	removing_leading_and_trailing_white_space(comment);
	removing_leading_and_trailing_white_space(addr_decoded);

	/* don't allow adding null entry */
	if(add && !*nick && !*full && !*fcc && !*comment && !*addr_decoded)
	  goto outtahere;

	/*
	 * addr_decoded is now the decoded string which has been edited
	 */
	if(addr_decoded)
	  new_decoded = parse_addrlist(addr_decoded);

	if(!ldap && (!new_decoded || !new_decoded[0]))
	  q_status_message(SM_ORDER, 3, 5, "Warning: entry has no addresses");

	if(!new_decoded || !new_decoded[0] || !new_decoded[1])
	  new_tag = Single;  /* one or zero addresses means its a Single */
	else
	  new_tag = List;    /* more than one addresses means its a List */

	if(new_tag == List && old_tag == List){
	    /*
	     * If Taking, make sure we write it even if user didn't edit
	     * it any further.
	     */
	    if(!warped)
	      list_changed++;
	    else if(he[N_ADDR].dirty)
	      for(q = orig_decoded, p = new_decoded; p && *p && q && *q; p++, q++)
	        if(strcmp(*p, *q) != 0){
		    list_changed++;
		    break;
	        }
	    
	    if(!list_changed && he[N_ADDR].dirty
	      && ((!(p && *p) && (q && *q)) || ((p && *p) && !(q && *q))))
	      list_changed++;

	    if(list_changed){
		/*
		 * need to delete old list members and add new members below
		 */
		rc = adrbk_listdel_all(abook, (a_c_arg_t)old_entry_num, 0);
	    }
	    else{
		/* don't need new_decoded */
		free_list_array(&new_decoded);
	    }

	    if(addr_decoded)
	      fs_give((void **)&addr_decoded);
	}
	else if(new_tag == List && old_tag == Single
	       || new_tag == Single && old_tag == List){
	    /* delete old entry */
	    rc = adrbk_delete(abook, (a_c_arg_t)old_entry_num, 0, 0, 0, 0);
	    old_entry_num = NO_NEXT;
	    if(addr_decoded && new_tag == List)
	      fs_give((void **)&addr_decoded);
	}

	if(new_tag == Single && addr_decoded){
	    /*
	     * Compare addr_decoded to each of orig_decoded.
	     * If it matches one, make addr_encoded equal to orig_encoded
	     * for that one, else encode it in our charset.
	     */
	    for(q = orig_decoded, p = orig_encoded; q && *q; q++, p++){
		if(!he[N_ADDR].dirty || strcmp(*q, addr_decoded) == 0)
		  break;
	    }

	    if(q && *q && p && *p)  /* got a match, use what we already had */
	      addr_encoded = cpystr(*p);
	    else  /* encode in our charset */
	      addr_encoded = encode_fullname_of_addrstring(addr_decoded,
						      ps_global->VAR_CHAR_SET);
	}

	/*
	 * This will be an edit in the cases where the tag didn't change
	 * and an add in the cases where it did.
	 */
	if(rc == 0)
	  rc = adrbk_add(abook,
		       (a_c_arg_t)old_entry_num,
		       nick,
		       he[N_FULL].dirty ? rfc1522_encode(tmp_20k_buf,
						       SIZEOF_20KBUF,
						       (unsigned char *)full,
						       ps_global->VAR_CHAR_SET)
					: abe->fullname,
		       new_tag == Single ? (ldap == 1 ? abe->addr.addr :
					      ldap == 2 ? abe->addr.list[0] :
					        addr_encoded)
					 : NULL,
		       fcc,
		       he[N_COMMENT].dirty ? rfc1522_encode(tmp_20k_buf +
							      2*MAX_FULLNAME,
						   SIZEOF_20KBUF-2*MAX_FULLNAME,
						       (unsigned char *)comment,
						       ps_global->VAR_CHAR_SET)
					   : abe->extra,
		       new_tag,
		       &new_entry_num,
		       &resort_happened,
		       1,
		       0,
		       (new_tag != List || !new_decoded));
    }
    
    if(rc == 0 && new_tag == List && new_decoded){
	char **t;

	/*
	 * Build a new list.
	 * For each entry in new_decoded, look through orig_decoded.  If
	 * matched, go with that orig_encoded, else encode that entry.
	 */
	for(p = new_decoded; *p; p++)
	  ;/* just counting for alloc below */
	
	t = new_encoded = (char **)fs_get((size_t)((p - new_decoded) + 1)
							    * sizeof(char *));
	memset((void *)new_encoded, 0, ((p-new_decoded)+1) * sizeof(char *));
	for(p = new_decoded; p && *p; p++){
	    for(q = orig_decoded; q && *q; q++)
	      if(strcmp(*q, *p) == 0)
		break;

	    if(q && *q)  /* got a match, use what we already have */
	      *t++ = cpystr(orig_encoded[q - orig_decoded]);
	    else  /* encode in our charset */
	      *t++ = encode_fullname_of_addrstring(*p, ps_global->VAR_CHAR_SET);
	}

	rc = adrbk_nlistadd(abook, (a_c_arg_t)new_entry_num, new_encoded,
			    1, 0, 1);
    }

    restore_state(&state);

    if(rc == -2 || rc == -3){
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
			"Error updating address book: %.200s",
			rc == -2 ? error_description(errno) : "Pine bug");
    }
    else if(rc == 0
       && strucmp(nick, nick_saved_for_pico_check) != 0
       && (editor_result & COMP_EXIT)){
	int added_to;

	for(added_to = 0; added_to < as.n_addrbk; added_to++)
	  if(abook_saved_for_pico_check == as.adrbks[added_to].address_book)
	    break;
	
	if(added_to >= as.n_addrbk)
	  added_to = -1;

	fname = addr_lookup(nick, &which_addrbook, added_to, NULL);
	if(fname){
	    char *decode, *decodebuf;
	    size_t len;

	    len = strlen(fname)+1;
	    decodebuf = (char *)fs_get(len * sizeof(char));
	    dummy = NULL;
	    decode = (char *)rfc1522_decode((unsigned char *)decodebuf, len,
					    fname, &dummy);
	    if(dummy)
	      fs_give((void **)&dummy);

	    q_status_message4(SM_ORDER, 5, 9,
	       "Warning! Nickname %.200s also exists in \"%.200s\"%.200s%.200s",
		nick, as.adrbks[which_addrbook].nickname,
		(decode && *decode) ? " as " : "",
		(decode && *decode) ? decode : "");
	    fs_give((void **)&fname);
	    fs_give((void **)&decodebuf);
	}
    }

    if(resort_happened || list_changed){
	DL_CACHE_S dlc_restart;

	dlc_restart.adrbk_num = as.cur;
	dlc_restart.dlcelnum  = new_entry_num;
	switch(new_tag){
	  case Single:
	    dlc_restart.type = DlcSimple;
	    break;
	  
	  case List:
	    dlc_restart.type = DlcListHead;
	    break;
	}

	warp_to_dlc(&dlc_restart, 0L);
	if(warped)
	  *warped = 1;
    }

outtahere:
    if(he)
      free_headents(&he);

    if(msgso)
      so_give(&msgso);

    if(nick)
      fs_give((void **)&nick);
    if(full)
      fs_give((void **)&full);
    if(fcc)
      fs_give((void **)&fcc);
    if(comment)
      fs_give((void **)&comment);

    if(addr_decoded)
      fs_give((void **)&addr_decoded);
    if(addr_encoded)
      fs_give((void **)&addr_encoded);
    if(nick_saved_for_pico_check)
      fs_give((void **)&nick_saved_for_pico_check);

    free_list_array(&orig_decoded);
    free_list_array(&orig_encoded);
    free_list_array(&new_decoded);
    free_list_array(&new_encoded);
#ifdef	ENABLE_LDAP
    if(expander_address)
      fs_give((void **)&expander_address);
#endif
}


/*ARGSUSED*/
int
verify_nick(given, expanded, error, fcc, mangled)
    char	 *given,
		**expanded,
		**error;
    BUILDER_ARG  *fcc;
    int          *mangled;
{
    char *tmp;

    dprint(7, (debugfile, "- verify_nick - (%s)\n", given ? given : "nul"));

    tmp = cpystr(given);
    removing_leading_and_trailing_white_space(tmp);

    if(nickname_check(tmp, error)){
	fs_give((void **)&tmp);
	return -2;
    }

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0) && mangled)
      *mangled = 1;

    ab_nesting_level++;
    if(strucmp(tmp, nick_saved_for_pico_check) != 0
       && adrbk_lookup_by_nick(abook_saved_for_pico_check,
			       tmp, (adrbk_cntr_t *)NULL)){
	if(error){
	    char buf[MAX_NICKNAME + 80];

	    sprintf(buf, "\"%.*s\" already in address book.", MAX_NICKNAME,
		    tmp);
	    *error = cpystr(buf);
	}
	
	ab_nesting_level--;
	fs_give((void **)&tmp);
	return -2;
    }

    ab_nesting_level--;
    if(expanded)
      *expanded = tmp;
    else
      fs_give((void **)&tmp);

    /* This is so pico will erase any old message */
    if(error)
      *error = cpystr("");

    return 0;
}


/*
 * Args: to      -- the passed in line to parse
 *       full_to -- Address of a pointer to return the full address in.
 *		    This will be allocated here and freed by the caller.
 *                  However, this function is just going to copy "to".
 *                  (special case for the route-addr-hack in build_address_int)
 *                  We're just looking for the error messages.
 *       error   -- Address of a pointer to return an error message in.
 *		    This will be allocated here and freed by the caller.
 *       fcc     -- This should be passed in NULL.
 *                  This builder doesn't support affected_entry's.
 *
 * Result:  0 is returned if address was OK, 
 *         -2 if address wasn't OK.
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
verify_addr(to, full_to, error, fcc, mangled)
    char	 *to,
		**full_to,
		**error;
    BUILDER_ARG	 *fcc;
    int          *mangled;
{
    register char *p;
    int            ret_val;
    BuildTo        bldto;
    jmp_buf        save_jmp_buf;
    int           *save_nesting_level;

    dprint(7, (debugfile, "- verify_addr - (%s)\n", to ? to : "nul"));

    /* check to see if to string is empty to avoid work */
    for(p = to; p && *p && isspace((unsigned char)(*p)); p++)
      ;/* do nothing */

    if(!p || !*p){
	if(full_to)
	  *full_to = cpystr(to ? to : "");  /* because pico does a strcmp() */

	return 0;
    }

    if(full_to != NULL)
      *full_to = (char *)NULL;

    if(error != NULL)
      *error = (char *)NULL;
    
    /*
     * If we end up jumping back here because somebody else changed one of
     * our addrbooks out from underneath us, we may well leak some memory.
     * That's probably ok since this will be very rare.
     */
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    save_nesting_level = cpyint(ab_nesting_level);
    if(setjmp(addrbook_changed_unexpectedly)){
	if(full_to && *full_to)
	  fs_give((void **)full_to);

	q_status_message(SM_ORDER, 3, 5, "Resetting address book...");
	dprint(1, (debugfile,
	    "RESETTING address book... verify_addr(%s)!\n", to ? to : "?"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    bldto.type    = Str;
    bldto.arg.str = to;

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0) && mangled)
      *mangled = 1;

    ab_nesting_level++;

    ret_val = build_address_internal(bldto, full_to, error, NULL, NULL, NULL,
				     1, 1, NULL);

    ab_nesting_level--;
    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    if(full_to && *full_to && ret_val >= 0)
      removing_leading_and_trailing_white_space(*full_to);

    /* This is so pico will erase the old message */
    if(error != NULL && *error == NULL)
      *error = cpystr("");

    if(ret_val < 0)
      ret_val = -2;  /* cause pico to stay on same header line */

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    return(ret_val);
}


/*
 *  Call back for pico to prompt the user for exit confirmation
 *
 * Returns: either NULL if the user accepts exit, or string containing
 *	 reason why the user declined.
 */      
char *
pico_sendexit_for_adrbk(he, redraw_pico, allow_flowed)
    struct headerentry *he;
    void (*redraw_pico)();
    int allow_flowed;
{
    char *rstr = NULL;
    void (*redraw)() = ps_global->redrawer;

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);
    
    switch(want_to("Exit and save changes ", 'y', 0, NO_HELP, WT_NORM)){
      case 'y':
	break;

      case 'n':
	rstr = "Use ^C to abandon changes you've made";
	break;
    }

    ps_global->redrawer = redraw;
    return(rstr);
}


/*
 *  Call back for pico to prompt the user for exit confirmation
 *
 * Returns: either NULL if the user accepts exit, or string containing
 *	 reason why the user declined.
 */      
char *
pico_cancelexit_for_adrbk(word, redraw_pico)
    char *word;
    void (*redraw_pico)();
{
    char prompt[90];
    char *rstr = NULL;
    void (*redraw)() = ps_global->redrawer;

    strncat(strncat(strncpy(prompt, "Cancel ", 10), word, 20),
	   " (answering \"Yes\" will abandon any changes made) ", 60);
    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);
    
    switch(want_to(prompt, 'y', 'x', NO_HELP, WT_NORM)){
      case 'y':
	rstr = "";
	break;

      case 'n':
      case 'x':
	break;
    }

    ps_global->redrawer = redraw;
    return(rstr);
}


char *
pico_cancel_for_adrbk_take(redraw_pico)
    void (*redraw_pico)();
{
    return(pico_cancelexit_for_adrbk("take", redraw_pico));
}


char *
pico_cancel_for_adrbk_edit(redraw_pico)
    void (*redraw_pico)();
{
    return(pico_cancelexit_for_adrbk("changes", redraw_pico));
}


/*
prompt::name::help::prlen::maxlen::realaddr::
builder::affected_entry::next_affected::selector::key_label::fileedit::
display_it::break_on_comma::is_attach::rich_header::only_file_chars::
single_space::sticky::dirty::start_here::blank::KS_ODATAVAR
*/
static struct headerentry headents_for_add[]={
  {"Server Name : ",  "Server",  h_composer_abook_add_server, 14, 0, NULL,
   verify_server_name, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 1, 0, 0, 0, 0, KS_NONE},
  {"Folder Name : ",  "Folder",  h_composer_abook_add_folder, 14, 0, NULL,
   verify_folder_name, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"NickName    : ",  "Nickname",  h_composer_abook_add_nick, 14, 0, NULL,
   verify_abook_nick,  NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {NULL, NULL, NO_HELP, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
};
#define NN_SERVER 0
#define NN_FOLDER 1
#define NN_NICK   2
#define NN_END    3

/*
 * Args:             global -- Add a global address book, not personal.
 *           add_after_this -- This is the addrbook number which should come
 *                             right before the new addrbook we're adding, if
 *                             that makes sense. If this is -1, append to end
 *                             of the list.
 *
 * Returns:  addrbook number of new addrbook, or
 *           -1, no addrbook added
 */
int
ab_add_abook(global, add_after_this)
    int global;
    int add_after_this;
{
    int ret;

    dprint(2, (debugfile, "- ab_add_abook -\n"));

    ret = ab_modify_abook_list(0, global, add_after_this, NULL, NULL, NULL);

    if(ret >= 0)
      q_status_message(SM_ORDER, 0, 3,
		       "New address book added. Use \"$\" to adjust order");
    
    return(ret);
}


/*
 * Args:          global -- Add a global address book, not personal.
 *             abook_num -- Abook num of the entry we are editing.
 *                  serv -- Default server.
 *                folder -- Default folder.
 *                  nick -- Default nickname.
 *
 * Returns:  abook_num if successful,
 *           -1, if not
 */
int
ab_edit_abook(global, abook_num, serv, folder, nick)
    int   global, abook_num;
    char *serv, *folder, *nick;
{
    dprint(2, (debugfile, "- ab_edit_abook -\n"));

    return(ab_modify_abook_list(1, global, abook_num, serv, folder, nick));
}

static int the_one_were_editing;


/*
 * Args:            edit -- Edit existing entry
 *                global -- Add a global address book, not personal.
 *             abook_num -- This is the addrbook number which should come
 *                            right before the new addrbook we're adding, if
 *                            that makes sense. If this is -1, append to end
 *                            of the list.
 *                            If we are editing instead of adding, this is
 *                            the abook number of the entry we are editing.
 *              def_serv -- Default server.
 *              def_fold -- Default folder.
 *              def_nick -- Default nickname.
 *
 * Returns:  addrbook number of new addrbook, or
 *           -1, no addrbook added
 */
int
ab_modify_abook_list(edit, global, abook_num, def_serv, def_fold, def_nick)
    int   edit, global, abook_num;
    char *def_serv, *def_fold, *def_nick;
{
    struct headerentry *he;
    PICO pbf;
    STORE_S *msgso;
    int editor_result, i, how_many_in_list, new_abook_num, num_in_list;
    int ret = 0;
    char *server, *folder, *nickname;
    char *new_item = NULL;
    EditWhich ew;
    AccessType remember_access_result;
    PerAddrBook *pab;
    char titlebar[30];
    char         **list, **new_list = NULL;
    char         tmp[1000+MAXFOLDER];
    struct variable *vars = ps_global->vars;

    dprint(2, (debugfile, "- ab_modify_abook_list -\n"));

    if(ps_global->readonly_pinerc){
	q_status_message1(SM_ORDER, 0, 3,
			  "%s cancelled: config file not changeable",
			  edit ? "Change" : "Add");
	return -1;
    }

    ew = Main;

    if(global && vars[V_GLOB_ADDRBOOK].is_fixed ||
       !global && vars[V_ADDRESSBOOK].is_fixed){
	q_status_message1(SM_ORDER, 0, 3,
	     "Cancelled: Sys. Mgmt. does not allow changing %saddress books",
	     global ? "global " : "");
	
	return -1;
    }

    init_ab_if_needed();

    if(edit){
	if((!global &&
		(abook_num < 0 || abook_num >= as.how_many_personals)) ||
	   (global &&
	        (abook_num < as.how_many_personals ||
		    abook_num >= as.n_addrbk))){
	    dprint(1, (debugfile, "Programming botch in ab_modify_abook_list: global=%d abook_num=%d n_addrbk=%d\n", global, abook_num, as.n_addrbk));
	    q_status_message(SM_ORDER, 0, 3, "Programming botch, bad abook_num");
	    return -1;
	}

	the_one_were_editing = abook_num;
    }
    else
      the_one_were_editing = -1;

    standard_picobuf_setup(&pbf);
    pbf.exittest      = pico_sendexit_for_adrbk;
    pbf.canceltest    = pico_cancel_for_adrbk_edit;
    sprintf(titlebar, "%s ADDRESS BOOK", edit ? "CHANGE" : "ADD");
    pbf.pine_anchor   = set_titlebar(titlebar,
				      ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,ps_global->msgmap, 
				      0, FolderName, 0, 0, NULL);
    pbf.pine_flags   |= P_NOBODY;

    /* An informational message */
    if(msgso = so_get(PicoText, NULL, EDIT_ACCESS)){
	int lines_avail;
char *t1 =
" To add a local address book that will be accessed *only* by Pine running\n on this machine, leave the server field blank.";
char *t2 =
" To add an address book that will be accessed by IMAP, fill in the\n server name.";
char *t3 =
" (NOTE: An address book cannot be accessed by IMAP from\n one Pine and as a local file from another. It is either always accessed\n by IMAP or it is a local address book which is never accessed by IMAP.)";
char *t4 =
" In the Folder field, type the remote folder name or local file name.";
char *t5 =
" In the Nickname field, give the address book a nickname or leave it blank.";
char *t6 =
" To get help specific to an item, press ^G.";
char *t7 =
" To exit and save the configuration, press ^X. To cancel, press ^C.";

	pbf.msgtext = (void *)so_text(msgso);
	/*
	 * It's nice if we can make it so these lines make sense even if
	 * they don't all make it on the screen, because the user can't
	 * scroll down to see them.
	 *
	 * The 3 is the number of fields to be defined, the 1 is for a
	 * single blank line after the field definitions.
	 */
	lines_avail = ps_global->ttyo->screen_rows - HEADER_ROWS(ps_global) -
			FOOTER_ROWS(ps_global) - 3 - 1;

	if(lines_avail >= 15){		/* extra blank line */
	    so_puts(msgso, "\n");
	    lines_avail--;
	}

	if(lines_avail >= 2){
	    so_puts(msgso, t1);
	    lines_avail -= 2;
	}

	if(lines_avail >= 5){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t2);
	    so_puts(msgso, t3);
	    lines_avail -= 5;
	}
	else if(lines_avail >= 3){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t2);
	    lines_avail -= 3;
	}

	if(lines_avail >= 2){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t4);
	    lines_avail -= 2;
	}

	if(lines_avail >= 2){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t5);
	    lines_avail -= 2;
	}

	if(lines_avail >= 3){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t6);
	    so_puts(msgso, "\n");
	    so_puts(msgso, t7);
	}
	else if(lines_avail >= 2){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t7);
	}
    }

    he = (struct headerentry *)fs_get((NN_END+1) * sizeof(struct headerentry));
    memset((void *)he, 0, (NN_END+1) * sizeof(struct headerentry));
    pbf.headents      = he;

    /* make a copy of each field */
    server = cpystr(def_serv ? def_serv : "");
    he[NN_SERVER]          = headents_for_add[NN_SERVER];
    he[NN_SERVER].realaddr = &server;

    folder = cpystr(def_fold ? def_fold : "");
    he[NN_FOLDER]          = headents_for_add[NN_FOLDER];
    he[NN_FOLDER].realaddr = &folder;

    nickname = cpystr(def_nick ? def_nick : "");
    he[NN_NICK]          = headents_for_add[NN_NICK];
    he[NN_NICK].realaddr = &nickname;

    he[NN_END] = headents_for_add[NN_END];

    /* pass to pico and let user change them */
    editor_result = pico(&pbf);
    standard_picobuf_teardown(&pbf);

    if(editor_result & COMP_GOTHUP){
	ret = -1;
	hup_signal();
    }
    else{
	fix_windsize(ps_global);
	init_signals();
    }

    if(editor_result & COMP_CANCEL){
	ret = -1;
	q_status_message1(SM_ORDER, 0, 3,
			  "Address book %s is cancelled",
			  edit ? "change" : "add");
    }
    else if(editor_result & COMP_EXIT){
	if(edit &&
	   !strcmp(server, def_serv ? def_serv : "") &&
	   !strcmp(folder, def_fold ? def_fold : "") &&
	   !strcmp(nickname, def_nick ? def_nick : "")){
	    ret = -1;
	    q_status_message1(SM_ORDER, 0, 3,
			      "No change: Address book %s is cancelled",
			      edit ? "change" : "add");
	}
	else{
	    if(global){
		list = VAR_GLOB_ADDRBOOK;
		how_many_in_list = as.n_addrbk - as.how_many_personals;
		if(edit)
		  new_abook_num = abook_num;
		else if(abook_num < 0)
		  new_abook_num  = as.n_addrbk;
		else
		  new_abook_num  = max(min(abook_num + 1, as.n_addrbk),
				       as.how_many_personals);

		num_in_list      = new_abook_num - as.how_many_personals;
	    }
	    else{
		list = VAR_ADDRESSBOOK;
		how_many_in_list = as.how_many_personals;
		new_abook_num = abook_num;
		if(edit)
		  new_abook_num = abook_num;
		else if(abook_num < 0)
		  new_abook_num  = as.how_many_personals;
		else
		  new_abook_num  = min(abook_num + 1, as.how_many_personals);

		num_in_list      = new_abook_num;
	    }

	    if(!edit)
	      how_many_in_list++;		/* for new abook */

	    removing_leading_and_trailing_white_space(server);
	    removing_leading_and_trailing_white_space(folder);
	    removing_leading_and_trailing_white_space(nickname);

	    /* eliminate surrounding brackets */
	    if(server[0] == '{' && server[strlen(server)-1] == '}'){
		char *p;

		server[strlen(server)-1] = '\0';
		for(p = server; *p; p++)
		  *p = *(p+1);
	    }

	    sprintf(tmp, "%s%.500s%s%.*s",
		    *server ? "{" : "",
		    *server ? server : "",
		    *server ? "}" : "",
		    MAXFOLDER, folder);
	    
	    new_item = put_pair(nickname, tmp);

	    if(!new_item || *new_item == '\0'){
		q_status_message1(SM_ORDER, 0, 3,
				 "Address book %s is cancelled",
				 edit ? "change" : "add");
		ret = -1;
		goto get_out;
	    }

	    /* allocate for new list */
	    new_list = (char **)fs_get((how_many_in_list + 1) * sizeof(char *));

	    /* copy old list up to where we will insert new entry */
	    for(i = 0; i < num_in_list; i++)
	      new_list[i] = cpystr(list[i]);
	    
	    /* insert the new entry */
	    new_list[i++] = cpystr(new_item);

	    /* copy rest of old list, skip current if editing */
	    for(; i < how_many_in_list; i++)
	      new_list[i] = cpystr(list[edit ? i : (i-1)]);

	    new_list[i] = NULL;

	    /* this frees old variable contents for us */
	    if(set_variable_list(global ? V_GLOB_ADDRBOOK : V_ADDRESSBOOK,
				 new_list, TRUE, ew)){
		q_status_message1(SM_ORDER, 0, 3,
			"%s cancelled: couldn't save pine configuration file",
			edit ? "Change" : "Add");

		set_current_val(&vars[global ? V_GLOB_ADDRBOOK : V_ADDRESSBOOK],
				TRUE, FALSE);
		ret = -1;
		goto get_out;
	    }

	    ret = new_abook_num;
	    set_current_val(&vars[global ? V_GLOB_ADDRBOOK : V_ADDRESSBOOK],
			    TRUE, FALSE);

	    addrbook_reset();
	    init_ab_if_needed();

	    /*
	     * Test to see if this definition is going to work.
	     * Error messages are a good side effect.
	     */
	    pab = &as.adrbks[num_in_list];
	    init_abook(pab, NoDisplay);
	    remember_access_result = pab->access;
	    addrbook_reset();
	    init_ab_if_needed();
	    /* if we had trouble, give a clue to user (other than error msg) */
	    if(remember_access_result == NoAccess){
		pab = &as.adrbks[num_in_list];
		pab->access = remember_access_result;
	    }
	}
    }
    
get_out:

    if(he)
      free_headents(&he);

    if(new_list)
      free_list_array(&new_list);

    if(new_item)
      fs_give((void **)&new_item);

    if(msgso)
      so_give(&msgso);

    if(server)
      fs_give((void **)&server);
    if(folder)
      fs_give((void **)&folder);
    
    return(ret);
}


int
any_addrbooks_to_convert(ps)
    struct pine *ps;
{
    PerAddrBook *pab;
    int          i, count = 0;

    init_ab_if_needed();

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab && !(pab->type & REMOTE_VIA_IMAP) && !(pab->type & GLOBAL))
	  count++;
    }

    return(count);
}


int
convert_addrbooks_to_remote(ps, rem_folder_prefix, len)
    struct pine *ps;
    char        *rem_folder_prefix;
    size_t       len;
{
    PerAddrBook *pab;
    int          i, count = 0, ret = 0;

    init_ab_if_needed();

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab && !(pab->type & REMOTE_VIA_IMAP) && !(pab->type & GLOBAL))
	  count++;
    }

    for(i = 0; ret != -1 && i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab && !(pab->type & REMOTE_VIA_IMAP) && !(pab->type & GLOBAL))
	  ret = convert_abook_to_remote(ps, pab, rem_folder_prefix, len, count);
    }

    return(ret);
}


/*
 * Returns -1 if cancelled, -2 on error, 0 otherwise.
 */
int
convert_abook_to_remote(ps, pab, rem_folder_prefix, len, count)
    struct pine *ps;
    PerAddrBook *pab;
    char        *rem_folder_prefix;
    size_t       len;
    int          count;
{
#define DEF_ABOOK_NAME "remote_addrbook"
    char  local_file[MAILTMPLEN];
    char  rem_abook[MAILTMPLEN+3], prompt[MAILTMPLEN], old_nick[MAILTMPLEN];
    char *p = NULL, *err_msg = NULL, *q;
    char *serv = NULL, *nick = NULL, *file = NULL, *folder = NULL;
    int   ans, rc, offset, i, abook_num = -1, flags = OE_APPEND_CURRENT;
    HelpType help;

    sprintf(old_nick, "%s%.30s%s",
	    count > 1 ? " \"" : "",
	    count > 1 ? pab->nickname : "",
	    count > 1 ? "\"" : "");

    sprintf(prompt, "Convert addressbook%.100s to a remote addrbook ", old_nick);
    if((ans=want_to(prompt, 'y', 'x', h_convert_abook, WT_NORM)) != 'y')
      return(ans == 'n' ? 0 : -1);

    /* make sure the addrbook has been opened before, so that the file exists */
    if(pab->ostatus == Closed || pab->ostatus == HalfOpen){
	(void)init_addrbooks(NoDisplay, 0, 0, 0);
	(void)init_addrbooks(Closed, 0, 0, 0);
    }

    if(pab->filename){
	strncpy(local_file, pab->filename, sizeof(local_file)-1);
	local_file[sizeof(local_file)-1] = '\0';
#if	defined(DOS)
	p = strrindex(pab->filename, '\\');
#else
	p = strrindex(pab->filename, '/');
#endif
    }

    strncpy(rem_abook, rem_folder_prefix, sizeof(rem_abook)-3);
    if(!*rem_abook){
	sprintf(prompt, "Name of server to contain remote addrbook : ");
	help = NO_HELP;
	while(1){
	    rc = optionally_enter(rem_abook, -FOOTER_ROWS(ps), 0,
				  sizeof(rem_abook), prompt, NULL,
				  help, &flags);
	    removing_leading_and_trailing_white_space(rem_abook);
	    if(rc == 3){
		help = help == NO_HELP ? h_convert_pinerc_server : NO_HELP;
	    }
	    else if(rc == 1){
		cmd_cancelled(NULL);
		return(-1);
	    }
	    else if(rc == 0){
		if(*rem_abook){
		    /* add brackets */
		    offset = strlen(rem_abook);
		    for(i = offset; i >= 0; i--)
		      rem_abook[i+1] = rem_abook[i];
		    
		    rem_abook[0] = '{';
		    rem_abook[++offset] = '}';
		    rem_abook[++offset] = '\0';
		    break;
		}
	    }
	}
    }

    if(*rem_abook){
	if(p && count > 1)
	  strncat(rem_abook, p+1,
		  sizeof(rem_abook)-1-strlen(rem_abook));
	else
	  strncat(rem_abook, DEF_ABOOK_NAME,
		  sizeof(rem_abook)-1-strlen(rem_abook));
    }

    if(*rem_abook){
	file = cpystr(rem_abook);
	if(pab->nickname){
	    nick = (char *)fs_get((max(strlen(pab->nickname),strlen("Address Book"))+8) * sizeof(char));
	    sprintf(nick, "Remote %s",
	            (pab->nickname && !strcmp(pab->nickname, DF_ADDRESSBOOK))
			? "Address Book" : pab->nickname);
	}
	else
	  nick = cpystr("Remote Address Book");
	
	if(file && *file == '{'){
	    q = file + 1;
	    if((p = strindex(file, '}'))){
		*p = '\0';
		serv = q;
		folder = p+1;
	    }
	    else if(file)
	      fs_give((void **)&file);
	}
	else
	  folder = file;
    }
	
    q_status_message(SM_ORDER, 3, 5,
       "You now have a chance to change the name of the remote addrbook...");
    abook_num = ab_modify_abook_list(0, 0, -1, serv, folder, nick);

    /* extract folder name of new abook so we can copy to it */
    if(abook_num >= 0){
	char    **lval;
	EditWhich ew = Main;

	lval = LVAL(&ps->vars[V_ADDRESSBOOK], ew);
	get_pair(lval[abook_num], &nick, &file, 0, 0);
	if(nick)
	  fs_give((void **)&nick);
	
	if(file){
	    strncpy(rem_abook, file, sizeof(rem_abook)-1);
	    rem_abook[sizeof(rem_abook)-1] = '\0';
	    fs_give((void **)&file);
	}
    }

    /* copy the abook */
    if(abook_num >= 0 && copy_abook(local_file, rem_abook, &err_msg)){
	if(err_msg){
	    q_status_message(SM_ORDER | SM_DING, 7, 10, err_msg);
	    fs_give((void **)&err_msg);
	}

	return(-2);
    }
    else if(abook_num >= 0){			/* give user some info */
	STORE_S  *store;
	SCROLL_S  sargs;
	char     *beg, *end;

	/*
	 * Save the hostname in rem_folder_prefix so we can use it again
	 * for other conversions if needed.
	 */
	if((beg = rem_abook)
	   && (*beg == '{' || (*beg == '*' && *++beg == '{'))
	   && (end = strindex(rem_abook, '}'))){
	    rem_folder_prefix[0] = '{';
	    strncpy(rem_folder_prefix+1, beg+1, min(end-beg,len-2));
	    rem_folder_prefix[min(end-beg,len-2)] = '}';
	    rem_folder_prefix[min(end-beg+1,len-1)] = '\0';
	}

	if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    q_status_message(SM_ORDER | SM_DING, 7, 10,
			     "Error allocating space for message.");
	    return(-2);
	}

	sprintf(prompt, "\nYour addressbook%.100s has been copied to the", old_nick);
	so_puts(store, prompt);
	so_puts(store, "\nremote folder \"");
	so_puts(store, rem_abook);
	so_puts(store, "\".");
	so_puts(store, "\nA definition for this remote address book has been added to your list");
	so_puts(store, "\nof address books. The definition for the address book it was copied");
	so_puts(store, "\nfrom is also still there. You may want to remove that after you");
	so_puts(store, "\nare confident that the new address book is complete and working.");
	so_puts(store, "\nUse the Setup/AddressBooks command to do that.\n");

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text  = so_text(store);
	sargs.text.src   = CharStar;
	sargs.text.desc  = "Remote Address Book Information";
	sargs.bar.title  = "ABOUT REMOTE ABOOK";
	sargs.help.text  = NO_HELP;
	sargs.help.title = NULL;

	scrolltool(&sargs);

	so_give(&store);	/* free resources associated with store */
	ps->mangled_screen = 1;
    }

    return(0);
}


int
any_sigs_to_convert(ps)
    struct pine *ps;
{
    char       *sigfile, *litsig;
    long        rflags;
    PAT_STATE   pstate;
    PAT_S      *pat;
    PAT_LINE_S *patline;
    extern PAT_HANDLE **cur_pat_h;

    /* first check main signature file */
    sigfile = ps->VAR_SIGNATURE_FILE;
    litsig = ps->VAR_LITERAL_SIG;

    if(sigfile && *sigfile && !litsig && sigfile[strlen(sigfile)-1] != '|' &&
       !IS_REMOTE(sigfile))
      return(1);

    rflags = (ROLE_DO_ROLES | PAT_USE_MAIN);
    if(any_patterns(rflags, &pstate)){
	set_pathandle(rflags);
	for(patline = *cur_pat_h ?  (*cur_pat_h)->patlinehead : NULL;
	    patline; patline = patline->next){
	    for(pat = patline->first; pat; pat = pat->next){

		/*
		 * See detoken() for when a sig file is used with a role.
		 */
		sigfile = pat->action ? pat->action->sig : NULL;
		litsig = pat->action ? pat->action->litsig : NULL;

		if(sigfile && *sigfile && !litsig &&
		   sigfile[strlen(sigfile)-1] != '|' &&
		   !IS_REMOTE(sigfile))
		  return(1);
	    }
	}
    }

    return(0);
}


int
any_rule_files_to_warn_about(ps)
    struct pine *ps;
{
    long        rflags;
    PAT_STATE   pstate;
    PAT_S      *pat;

    rflags = (ROLE_DO_ROLES | ROLE_DO_INCOLS | ROLE_DO_SCORES |
	      ROLE_DO_FILTER | ROLE_DO_OTHER | PAT_USE_MAIN);
    if(any_patterns(rflags, &pstate)){
	for(pat = first_pattern(&pstate);
	    pat;
	    pat = next_pattern(&pstate)){
	    if(pat->patline && pat->patline->type == File)
	      break;
	}

	if(pat)
	  return(1);
    }

    return(0);
}


int
convert_sigs_to_literal(ps, interactive)
    struct pine *ps;
    int          interactive;
{
    EditWhich   ew = Main;
    char       *sigfile, *litsig, *cstring_version, *nick, *src = NULL;
    char        prompt[MAILTMPLEN];
    STORE_S    *store;
    SCROLL_S	sargs;
    long        rflags;
    int         ans;
    PAT_STATE   pstate;
    PAT_S      *pat;
    PAT_LINE_S *patline;
    extern PAT_HANDLE **cur_pat_h;

    /* first check main signature file */
    sigfile = ps->VAR_SIGNATURE_FILE;
    litsig = ps->VAR_LITERAL_SIG;

    if(sigfile && *sigfile && !litsig && sigfile[strlen(sigfile)-1] != '|' &&
       !IS_REMOTE(sigfile)){
	if(interactive){
	    sprintf(prompt,
		    "Convert signature file \"%.30s\" to a literal sig ",
		    sigfile);
	    ClearBody();
	    ps->mangled_body = 1;
	    if((ans=want_to(prompt, 'y', 'x', h_convert_sig, WT_NORM)) == 'x'){
		cmd_cancelled(NULL);
		return(-1);
	    }
	}
	else
	  ans = 'y';

	if(ans == 'y' && (src = get_signature_file(sigfile, 0, 0, 0)) != NULL){
	    cstring_version = string_to_cstring(src);
	    set_variable(V_LITERAL_SIG, cstring_version, 0, 0, ew);

	    if(cstring_version)
	      fs_give((void **)&cstring_version);

	    fs_give((void **)&src);

	    if(interactive){
		if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
		    q_status_message(SM_ORDER | SM_DING, 7, 10,
				     "Error allocating space for message.");
		    return(-1);
		}

		sprintf(prompt,
    "\nYour signature file \"%.30s\" has been converted", sigfile);
		so_puts(store, prompt);
		so_puts(store,
    "\nto a literal signature, which means it is contained in your");
		so_puts(store,
    "\nPine configuration instead of being in a file of its own.");
		so_puts(store,
    "\nIf that configuration is copied to a remote folder then the");
		so_puts(store,
    "\nsignature will be available remotely also.");
		so_puts(store,
    "\nChanges to the signature file itself will no longer have any");
		so_puts(store,
    "\neffect on Pine but you may still edit the signature with the");
		so_puts(store,
    "\nSetup/Signature command.\n");

		memset(&sargs, 0, sizeof(SCROLL_S));
		sargs.text.text  = so_text(store);
		sargs.text.src   = CharStar;
		sargs.text.desc  = "Literal Signature Information";
		sargs.bar.title  = "ABOUT LITERAL SIG";
		sargs.help.text  = NO_HELP;
		sargs.help.title = NULL;

		scrolltool(&sargs);

		so_give(&store);
		ps->mangled_screen = 1;
	    }
	}
    }

    rflags = (ROLE_DO_ROLES | PAT_USE_MAIN);
    if(any_patterns(rflags, &pstate)){
	set_pathandle(rflags);
	for(patline = *cur_pat_h ?  (*cur_pat_h)->patlinehead : NULL;
	    patline; patline = patline->next){
	    for(pat = patline->first; pat; pat = pat->next){

		/*
		 * See detoken() for when a sig file is used with a role.
		 */
		sigfile = pat->action ? pat->action->sig : NULL;
		litsig = pat->action ? pat->action->litsig : NULL;
		nick = (pat->action && pat->action->nick && pat->action->nick[0]) ? pat->action->nick : NULL;

		if(sigfile && *sigfile && !litsig &&
		   sigfile[strlen(sigfile)-1] != '|' &&
		   !IS_REMOTE(sigfile)){
		    if(interactive){
			sprintf(prompt,
		 "Convert signature file \"%.30s\"%s%.50s%s to a literal sig ",
				sigfile,
				nick ? " in role \"" : "",
				nick ? nick : "",
				nick ? "\"" : "");
			ClearBody();
			ps->mangled_body = 1;
			if((ans=want_to(prompt, 'y', 'x',
					h_convert_sig, WT_NORM)) == 'x'){
			    cmd_cancelled(NULL);
			    return(-1);
			}
		    }
		    else
		      ans = 'y';

		    if(ans == 'y' &&
		       (src = get_signature_file(sigfile,0,0,0)) != NULL){

			cstring_version = string_to_cstring(src);

			if(pat->action->litsig)
			  fs_give((void **)&pat->action->litsig);

			pat->action->litsig = cstring_version;
			fs_give((void **)&src);

			set_pathandle(rflags);
			if(patline->type == Literal)
			  (*cur_pat_h)->dirtypinerc = 1;
			else
			  patline->dirty = 1;

			if(write_patterns(rflags) == 0){
			    if(interactive){
				/*
				 * Flush out current_vals of anything we've
				 * possibly changed.
				 */
				close_patterns(ROLE_DO_ROLES | PAT_USE_CURRENT);

				if(!(store=so_get(CharStar,NULL,EDIT_ACCESS))){
				    q_status_message(SM_ORDER | SM_DING, 7, 10,
					 "Error allocating space for message.");
				    return(-1);
				}

				sprintf(prompt,
		    "Your signature file \"%.30s\"%s%.50s%s has been converted",
					sigfile,
					nick ? " in role \"" : "",
					nick ? nick : "",
					nick ? "\"" : "");
				so_puts(store, prompt);
				so_puts(store,
	    "\nto a literal signature, which means it is contained in your");
				so_puts(store,
	    "\nPine configuration instead of being in a file of its own.");
				so_puts(store,
	    "\nIf that configuration is copied to a remote folder then the");
				so_puts(store,
	    "\nsignature will be available remotely also.");
				so_puts(store,
	    "\nChanges to the signature file itself will no longer have any");
				so_puts(store,
	    "\neffect on Pine. You may edit the signature with the");
				so_puts(store,
	    "\nSetup/Rules/Roles command.\n");

				memset(&sargs, 0, sizeof(SCROLL_S));
				sargs.text.text  = so_text(store);
				sargs.text.src   = CharStar;
				sargs.text.desc  =
					    "Literal Signature Information";
				sargs.bar.title  = "ABOUT LITERAL SIG";
				sargs.help.text  = NO_HELP;
				sargs.help.title = NULL;

				scrolltool(&sargs);

				so_give(&store);
				ps->mangled_screen = 1;
			    }
			}
			else if(interactive){
			  q_status_message(SM_ORDER | SM_DING, 7, 10,
					   "Error writing rules config.");
			}
			else{
			    fprintf(stderr, "Error converting role sig\n");
			    return(-1);
			}
		    }
		}
	    }
	}
    }

    return(0);
}


void
warn_about_rule_files(ps)
    struct pine *ps;
{
    STORE_S    *store;
    SCROLL_S	sargs;

    if(any_rule_files_to_warn_about(ps)){
	if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    q_status_message(SM_ORDER | SM_DING, 7, 10,
			     "Error allocating space for message.");
	    return;
	}

	so_puts(store, "\nSome of your Rules are contained in Rule files instead of being directly");
	so_puts(store, "\ncontained in your Pine configuration file. To make those rules");
	so_puts(store, "\navailable remotely you will need to move them out of the files.");
	so_puts(store, "\nThat can be done using the Shuffle command in the appropriate");
	so_puts(store, "\nSetup/Rules subcommands.\n");

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text  = so_text(store);
	sargs.text.src   = CharStar;
	sargs.text.desc  = "Rule Files Information";
	sargs.bar.title  = "ABOUT RULE FILES";
	sargs.help.text  = NO_HELP;
	sargs.help.title = NULL;

	scrolltool(&sargs);

	so_give(&store);
	ps->mangled_screen = 1;
    }
}


int
verify_folder_name(given, expanded, error, fcc, mangled)
    char        *given,
	       **expanded,
	       **error;
    BUILDER_ARG *fcc;
    int         *mangled;
{
    char *tmp;

    tmp = cpystr(given ? given : "");
    removing_leading_and_trailing_white_space(tmp);

    if(expanded)
      *expanded = tmp;
    else
      fs_give((void **)&tmp);
    
    if(error)
      *error = cpystr("");

    return 0;
}


int
verify_server_name(given, expanded, error, fcc, mangled)
    char        *given,
	       **expanded,
	       **error;
    BUILDER_ARG *fcc;
    int         *mangled;
{
    char *tmp;

    tmp = cpystr(given ? given : "");
    removing_leading_and_trailing_white_space(tmp);

    if(*tmp){
	/*
	 * could try to verify the hostname here
	 */
    }

    if(expanded)
      *expanded = tmp;
    else
      fs_give((void **)&tmp);
    
    if(error)
      *error = cpystr("");

    return 0;
}


int
verify_abook_nick(given, expanded, error, fcc, mangled)
    char        *given,
	       **expanded,
	       **error;
    BUILDER_ARG *fcc;
    int         *mangled;
{
    int   i;
    char *tmp;

    tmp = cpystr(given ? given : "");
    removing_leading_and_trailing_white_space(tmp);

    if(strindex(tmp, '"')){
	fs_give((void **)&tmp);
	if(error)
	  *error = cpystr("Double quote not allowed in nickname");
	
	return -2;
    }

    for(i = 0; i < as.n_addrbk; i++)
      if(i != the_one_were_editing && !strcmp(tmp, as.adrbks[i].nickname))
	break;
    
    if(i < as.n_addrbk){
	fs_give((void **)&tmp);

	if(error)
	  *error = cpystr("Nickname is already being used");
	
	return -2;
    }

    if(expanded)
      *expanded = tmp;
    else
      fs_give((void **)&tmp);
    
    if(error)
      *error = cpystr("");

    return 0;
}


/*
 * Delete an addressbook.
 *
 * Args: cur_line     -- The current line position (in global display list)
 *			 of cursor
 *       command_line -- The screen line on which to prompt
 *       err          -- Points to error message
 *
 * Returns -- 0, deleted addrbook
 *            -1, addrbook not deleted
 */
int
ab_del_abook(cur_line, command_line, err)
    long   cur_line;
    int    command_line;
    char **err;
{
    int          abook_num, varnum, delete_data = 0,
		 num_in_list, how_many_in_list, i, cnt, warn_about_revert = 0;
    char       **list, **new_list, **t, **lval;
    char         tmp[200];
    PerAddrBook *pab;
    struct variable *vars = ps_global->vars;
    EditWhich    ew;
    enum {NotSet,
	  Modify,
	  RevertToDefault,
	  OverRideDefault,
	  DontChange} modify_config;

    /* restrict address book config to normal config file */
    ew = Main;

    if(ps_global->readonly_pinerc){
	if(err)
	  *err = "Delete cancelled: config file not changeable";
	
	return -1;
    }

    abook_num = adrbk_num_from_lineno(cur_line);

    pab = &as.adrbks[abook_num];

    dprint(2, (debugfile, "- ab_del_abook(%s) -\n",
	   pab->nickname ? pab->nickname : "?"));

    varnum = (pab->type & GLOBAL) ? V_GLOB_ADDRBOOK : V_ADDRESSBOOK;

    if(vars[varnum].is_fixed){
	if(err){
	    if(pab->type & GLOBAL)
	      *err =
    "Cancelled: Sys. Mgmt. does not allow changing global address book config";
	    else
	      *err =
    "Cancelled: Sys. Mgmt. does not allow changing address book config";
	}

	return -1;
    }

    /*
     * Deal with reverting to default values of the address book
     * variables, or with user deleting a default value.
     */
    modify_config = NotSet;
    
    /* First count how many address books are in the user's config. */
    cnt = 0;
    lval = LVAL(&vars[varnum], ew);
    if(lval && lval[0])
      for(t = lval; *t != NULL; t++)
	cnt++;
    
    /*
     * Easy case, we can just delete one from the user's list.
     */
    if(cnt > 1){
	modify_config = Modify;
    }
    /*
     * Also easy. We'll revert to the default if it exists, and warn
     * the user about that.
     */
    else if(cnt == 1){
	modify_config = RevertToDefault;
	/* see if there's a default to revert to */
	cnt = 0;
	if(vars[varnum].global_val.l && vars[varnum].global_val.l[0])
	  for(t = vars[varnum].global_val.l; *t != NULL; t++)
	    cnt++;
	
	warn_about_revert = cnt;
    }
    /*
     * User is already using the default. Split it into two cases. If there
     * is one address book in default ask user if they want to delete that
     * default from their config. If there is more than one, ask them if
     * they want to ignore all the defaults or just delete this one.
     */
    else{
	/* count how many in default */
	cnt = 0;
	if(vars[varnum].global_val.l && vars[varnum].global_val.l[0])
	  for(t = vars[varnum].global_val.l; *t != NULL; t++)
	    cnt++;
	
	if(cnt > 1){
	    static ESCKEY_S opts[] = {
		{'i', 'i', "I", "Ignore All"},
		{'r', 'r', "R", "Remove One"},
		{-1, 0, NULL, NULL}};

	    sprintf(tmp,
	   "Ignore all default %.10s address books or just remove this one ? ",
	       pab->type & GLOBAL ? "global" : "personal");
	    switch(radio_buttons(tmp, command_line, opts, 'i', 'x',
		   h_ab_del_ignore, RB_NORM, NULL)){
	      case 'i':
		modify_config = OverRideDefault;
		break;
		
	      case 'r':
		modify_config = Modify;
		break;
		
	      case 'x':
		if(err)
		  *err = "Delete cancelled";

		return -1;
	    }
	}
	else{
	    switch(want_to("Delete this default address book from config ",
		 'n', 'x', h_ab_del_default, WT_NORM)){
	      case 'n':
	      case 'x':
	        if(err)
		  *err = "Delete cancelled";

	        return -1;
	    
	      case 'y':
		modify_config = OverRideDefault;
	        break;
	    }
	}
    }

    /*
     * ReadWrite means it exists and MaybeRorW means it is remote and we
     * haven't selected it yet to know our access permissions. The remote
     * folder should have been created, though, unless we didn't even have
     * permissions for that, in which case we got some error messages earlier.
     */
    if(pab->access == ReadWrite || pab->access == MaybeRorW){
	static ESCKEY_S o[] = {
	    {'d', 'd', "D", "Data"},
	    {'c', 'c', "C", "Config"},
	    {'b', 'b', "B", "Both"},
	    {-1, 0, NULL, NULL}};

	switch(radio_buttons("Delete data, config, or both ? ",
			     command_line, o, 'c', 'x',
			     (modify_config == RevertToDefault)
			       ? h_ab_del_data_revert
			       : h_ab_del_data_modify,
			     RB_NORM, NULL)){
	  case 'b':				/* Delete Both */
	    delete_data = 1;
	    break;
	    
	  case 'd':				/* Delete only Data */
	    modify_config = DontChange;
	    delete_data = 1;
	    break;

	  case 'c':				/* Delete only Config */
	    break;
	    
	  case 'x':				/* Cancel */
	  default:
	    if(err)
	      *err = "Delete cancelled";

	    return -1;
	}
    }
    else{
	/*
	 * Deleting config for address book which doesn't yet exist (hasn't
	 * ever been opened).
	 */
	switch(want_to("Delete configuration for highlighted addressbook ",
		       'n', 'x',
		       (modify_config == RevertToDefault)
		         ? h_ab_del_config_revert
			 : h_ab_del_config_modify,
		       WT_NORM)){
          case 'n':
          case 'x':
	  default:
	    if(err)
	      *err = "Delete cancelled";

	    return -1;
	
          case 'y':
	    break;
	}
    }

    if(delete_data){
	char warning[800];

	dprint(5, (debugfile, "deleting addrbook data\n"));
	warning[0] = '\0';

	/*
	 * In order to delete the address book it is easiest if we open
	 * it first. That fills in the filenames we want to delete.
	 */
	if(pab->address_book == NULL){
	    warning[300] = '\0';
	    pab->address_book = adrbk_open(pab, ps_global->home_dir,
					   &warning[300],
					   AB_SORT_RULE_NONE, 0, 0);
	    /*
	     * Couldn't get it open.
	     */
	    if(pab->address_book == NULL){
		if(warning[300]){
		    strncpy(warning, "Can't delete data: ", 100);
		    strncat(warning, &warning[300], 200);
		}
		else
		  strncpy(warning, "Can't delete address book data", 100);
	    }
	}

	/*
	 * If we have it open, set the delete bits and close to get the
	 * local copies. Delete the remote folder by hand.
	 */
	if(pab->address_book){
	    char       *hashfile, *file, *origfile = NULL;
	    int         h=0, f=0, o=0;

	    /*
	     * We're about to destroy addrbook data, better ask again.
	     */
	    if(pab->address_book->count > 0){
		char prompt[100];

		sprintf(prompt,
			"About to delete the contents of address book (%ld entries), really delete ", adrbk_count(pab->address_book));

		switch(want_to(prompt, 'n', 'n', h_ab_really_delete, WT_NORM)){
		  case 'y':
		    break;
		    
		  case 'n':
		  default:
		    if(err)
		      *err = "Delete cancelled";

		    return -1;
		}
	    }

	    pab->address_book->flags |= (DEL_FILE | DEL_HASHFILE);
	    file = cpystr(pab->address_book->filename);
	    hashfile = cpystr(pab->address_book->hashfile);
	    if(pab->type & REMOTE_VIA_IMAP)
	      origfile = cpystr(pab->address_book->orig_filename);

	    /*
	     * In order to avoid locking problems when we delete the
	     * remote folder, we need to actually close the remote stream
	     * instead of just putting it back in the stream pool.
	     * So we will remove this stream from the re-usable portion
	     * of the stream pool by clearing the SP_USEPOOL flag.
	     * Init_abook(pab, TotallyClosed) via rd_close_remdata is
	     * going to pine_mail_close it.
	     */
	    if(pab->type && REMOTE_VIA_IMAP
	       && pab->address_book
	       && pab->address_book->type == Imap
	       && pab->address_book->rd
	       && rd_stream_exists(pab->address_book->rd)){

		sp_unflag(pab->address_book->rd->t.i.stream, SP_USEPOOL);
	    }

	    /* This deletes the files because of DEL_ bits we set above. */
	    init_abook(pab, TotallyClosed);

	    /*
	     * Delete the remote folder.
	     */
	    if(pab->type & REMOTE_VIA_IMAP){
		REMDATA_S  *rd;
		int         exists;

		ps_global->c_client_error[0] = '\0';
		if(!pine_mail_delete(NULL, origfile) &&
		   ps_global->c_client_error[0] != '\0'){
		    dprint(1, (debugfile, "%s: %s\n", origfile ? origfile : "?",
			   ps_global->c_client_error));
		}

		/* delete line from metadata */
		rd = rd_new_remdata(RemImap, origfile, NULL);
		rd_write_metadata(rd, 1);
		rd_close_remdata(&rd);

		/* Check to see if it's still there */
		if((exists=folder_exists(NULL, origfile)) &&
		   (exists != FEX_ERROR)){
		    o++;
		    dprint(1, (debugfile, "Trouble deleting %s\n",
			   origfile ? origfile : "?"));
		}
	    }

	    if(can_access(hashfile, ACCESS_EXISTS) == 0){
		h++;
		dprint(1, (debugfile, "Trouble deleting %s\n",
		       hashfile ? hashfile : "?"));
	    }

	    if(can_access(file, ACCESS_EXISTS) == 0){
		f++;
		dprint(1, (debugfile, "Trouble deleting %s\n",
		       file ? file : "?"));
	    }

	    if(f || o || h)
	      sprintf(warning, "Trouble deleting data %s%s%s%s%s",
		      f ? file : "",
		      (f && (h || o)) ? ((h && o) ? ", " : " and ") : "",
		      h ? hashfile : "",
		      (h && o) ? " and " : "",
		      o ? origfile : "");

	    fs_give((void **)&file);
	    fs_give((void **)&hashfile);
	    if(origfile)
	      fs_give((void **)&origfile);
	}

	if(*warning){
	    q_status_message(SM_ORDER, 3, 3, warning);
	    dprint(1, (debugfile, "%s\n", warning));
	    display_message(NO_OP_COMMAND);
	}
	else if(modify_config == DontChange)
	  q_status_message(SM_ORDER, 0, 1, "Addressbook data deleted");
    }

    if(modify_config == DontChange){
	/*
	 * We return -1 to indicate that the addrbook wasn't deleted (as far
	 * as we're concerned) but we don't fill in err so that no error
	 * message will be printed.
	 * Since the addrbook is still an addrbook we need to reinitialize it.
	 */
	pab->access = adrbk_access(pab);
	if(pab->type & GLOBAL && pab->access != NoAccess)
	  pab->access = ReadOnly;
	
	init_abook(pab, HalfOpen);
	return -1;
    }
    else if(modify_config == Modify){
	list = vars[varnum].current_val.l;
	if(pab->type & GLOBAL){
	    how_many_in_list = as.n_addrbk - as.how_many_personals - 1;
	    num_in_list      = abook_num - as.how_many_personals;
	}
	else{
	    how_many_in_list = as.how_many_personals - 1;
	    num_in_list      = abook_num;
	}
    }
    else if(modify_config == OverRideDefault)
      how_many_in_list = 1;
    else if(modify_config == RevertToDefault)
      how_many_in_list = 0;
    else
      q_status_message(SM_ORDER, 3, 3, "can't happen in ab_del_abook");

    /* allocate for new list */
    if(how_many_in_list)
      new_list = (char **)fs_get((how_many_in_list + 1) * sizeof(char *));
    else
      new_list = NULL;

    /*
     * This case is both for modifying the users user_val and for the
     * case where the user wants to modify the global_val default and
     * use the modified version for his or her new user_val. We just
     * copy from the existing global_val, deleting the one addrbook
     * and put the result in user_val.
     */
    if(modify_config == Modify){
	/* copy old list up to where we will delete entry */
	for(i = 0; i < num_in_list; i++)
	  new_list[i] = cpystr(list[i]);
	
	/* copy rest of old list */
	for(; i < how_many_in_list; i++)
	  new_list[i] = cpystr(list[i+1]);
	
	new_list[i] = NULL;
    }
    else if(modify_config == OverRideDefault){
	new_list[0] = cpystr("");
	new_list[1] = NULL;
    }

    /* this also frees old variable contents for us */
    if(set_variable_list(varnum, new_list, TRUE, ew)){
	if(err)
	  *err = "Delete cancelled: couldn't save pine configuration file";

	set_current_val(&vars[varnum], TRUE, FALSE);
	free_list_array(&new_list);

	return -1;
    }

    set_current_val(&vars[varnum], TRUE, FALSE);

    if(warn_about_revert){
	sprintf(tmp, "Reverting to default %.10saddress %.10s",
		pab->type & GLOBAL ? "global " : "",
		warn_about_revert > 1 ? "books" : "book");
	q_status_message(SM_ORDER, 3, 4, tmp);
    }

    free_list_array(&new_list);
    
    return 0;
}


/*
 * Shuffle addrbooks.
 *
 * Args:    pab -- Pab from current addrbook.
 *        slide -- return value, tells how far to slide the cursor. If slide
 *                   is negative, slide it up, if positive, slide it down.
 * command_line -- The screen line on which to prompt
 *          msg -- Points to returned message, if any. Should be freed by
 *                   caller.
 *
 * Result: Two address books are swapped in the display order. If the shuffle
 *         crosses the Personal/Global boundary, then instead of swapping
 *         two address books the highlighted abook is moved from one section
 *         to the other.
 *          >= 0 on success.
 *          < 0 on failure, no changes.
 *          > 0 If the return value is greater than zero it means that we've
 *                reverted one of the variables to its default value. That
 *                means we've added at least one new addrbook, so the caller
 *                should reset. The value returned is the number of the
 *                moved addrbook + 1 (+1 so it won't be confused with zero).
 *          = 0 If the return value is zero we've just moved addrbooks around.
 *                No reset need be done.
 */
int
ab_shuffle(pab, slide, command_line, msg)
    PerAddrBook *pab;
    int   *slide;
    int    command_line;
    char **msg;
{
    ESCKEY_S opts[3];
    char     tmp[200];
    int      i, deefault, rv, target = 0;
    int      up_into_empty = 0, down_into_empty = 0;
    HelpType help;
    struct variable *vars = ps_global->vars;

    dprint(2, (debugfile, "- ab_shuffle() -\n"));

    *slide = 0;

    if(ps_global->readonly_pinerc){
	if(msg)
	  *msg = cpystr("Shuffle cancelled: config file not changeable");
	
	return -1;
    }

    /* Move it up or down? */
    i = 0;
    opts[i].ch      = 'u';
    opts[i].rval    = 'u';
    opts[i].name    = "U";
    opts[i++].label = "Up";

    opts[i].ch      = 'd';
    opts[i].rval    = 'd';
    opts[i].name    = "D";
    opts[i++].label = "Down";

    opts[i].ch = -1;
    deefault = 'u';

    if(pab->type & GLOBAL){
	if(vars[V_GLOB_ADDRBOOK].is_fixed){
	    if(msg)
	      *msg = cpystr("Cancelled: Sys. Mgmt. does not allow changing global address book config");
	    
	    return -1;
	}

	if(as.cur == 0){
	    if(as.config)
	      up_into_empty++;
	    else{					/* no up */
		opts[0].ch = -2;
		deefault = 'd';
	    }
	}

	if(as.cur == as.n_addrbk - 1)			/* no down */
	  opts[1].ch = -2;
    }
    else{
	if(vars[V_ADDRESSBOOK].is_fixed){
	    if(msg)
	      *msg = cpystr("Cancelled: Sys. Mgmt. does not allow changing address book config");
	    
	    return -1;
	}

	if(as.cur == 0){				/* no up */
	    opts[0].ch = -2;
	    deefault = 'd';
	}

	if(as.cur == as.n_addrbk - 1){
	    if(as.config)
	      down_into_empty++;
	    else
	      opts[1].ch = -2;				/* no down */
	}
    }

    sprintf(tmp, "Shuffle \"%.100s\" %s%s%s ? ",
	    pab->nickname,
	    (opts[0].ch != -2) ? "UP" : "",
	    (opts[0].ch != -2 && opts[1].ch != -2) ? " or " : "",
	    (opts[1].ch != -2) ? "DOWN" : "");
    help = (opts[0].ch == -2) ? h_ab_shuf_down
			      : (opts[1].ch == -2) ? h_ab_shuf_up
						   : h_ab_shuf;

    rv = radio_buttons(tmp, command_line, opts, deefault, 'x',
		       help, RB_NORM, NULL);

    ps_global->mangled_footer = 1;

    if(rv == 'u' && up_into_empty || rv == 'd' && down_into_empty)
      target = -1;
    else
      target = as.cur + (rv == 'u' ? -1 : 1);

    if(rv == 'x'){
	if(msg)
	  *msg = cpystr("Shuffle cancelled");
	
	return -1;
    }
    else
      return(do_the_shuffle(slide, as.cur, target, msg));
}


/*
 * Actually shuffle the config variables and address books structures around.
 *
 * Args:  anum1, anum2 -- The numbers of the address books
 *                 msg -- Points to returned message, if any.
 *
 * Returns: >= 0 on success.
 *          < 0 on failure, no changes.
 *          > 0 If the return value is greater than zero it means that we've
 *                reverted one of the variables to its default value. That
 *                means we've added at least one new addrbook, so the caller
 *                should reset. The value returned is the number of the
 *                moved addrbook + 1 (+1 so it won't be confused with zero).
 *          = 0 If the return value is zero we've just moved addrbooks around.
 *                No reset need be done.
 *
 * Anum1 is the one that we want to move, anum2 is the one that it will be
 * swapped with. When anum1 and anum2 are on the opposite sides of the
 * Personal/Global boundary then instead of swapping we just move anum1 to
 * the other side of the boundary.
 *
 * Anum2 of -1 means it is a swap into the other type of address book, which
 * is currently empty.
 */
int
do_the_shuffle(slide, anum1, anum2, msg)
    int   *slide;
    int    anum1, anum2;
    char **msg;
{
    PerAddrBook *pab;
    enum {NotSet, Pers, Glob, Empty} type1, type2;
    int          i, j, retval = -1;
    struct variable *vars = ps_global->vars;
    char **lval;
    EditWhich ew;
    char *cancel_msg = "Shuffle cancelled: couldn't save configuration file";

    dprint(5, (debugfile, "- do_the_shuffle(%d, %d) -\n", anum1, anum2));

    /* restrict address book config to normal config file */
    ew = Main;

    if(anum1 == -1)
      type1 = Empty;
    else{
	pab = &as.adrbks[anum1];
	type1 = (pab->type & GLOBAL) ? Glob : Pers;
    }

    if(type1 == Empty){
	if(msg)
	  *msg =
	      cpystr("Shuffle cancelled: highlight entry you wish to shuffle");

	return(retval);
    }

    if(anum2 == -1)
      type2 = Empty;
    else{
	pab = &as.adrbks[anum2];
	type2 = (pab->type & GLOBAL) ? Glob : Pers;
    }

    if(type2 == Empty)
      type2 = (type1 == Pers) ? Glob : Pers;

    if((type1 == Pers || type2 == Pers) && vars[V_ADDRESSBOOK].is_fixed){
	if(msg)
	  *msg = cpystr("Cancelled: Sys. Mgmt. does not allow changing address book configuration");

	return(retval);
    }

    if((type1 == Glob || type2 == Glob) && vars[V_GLOB_ADDRBOOK].is_fixed){
	if(msg)
	  *msg = cpystr("Cancelled: Sys. Mgmt. does not allow changing global address book config");

	return(retval);
    }

    /*
     * There are two cases. If the shuffle is two address books within the
     * same variable, then they just swap places. If it is a shuffle of an
     * addrbook from one side of the boundary to the other, just that one
     * is moved.
     */
    if(type1 == Glob && type2 == Glob ||
       type1 == Pers && type2 == Pers){
	int          how_many_in_list, varnum;
	int          anum1_rel, anum2_rel;	/* position in specific list */
	char       **list, **new_list;
	PerAddrBook  tmppab;

	*slide = (anum1 < anum2) ? LINES_PER_ABOOK : -1 * LINES_PER_ABOOK;

	if(type1 == Pers){
	    how_many_in_list = as.how_many_personals;
	    list             = VAR_ADDRESSBOOK;
	    varnum           = V_ADDRESSBOOK;
	    anum1_rel        = anum1;
	    anum2_rel        = anum2;
	}
	else{
	    how_many_in_list = as.n_addrbk - as.how_many_personals;
	    list             = VAR_GLOB_ADDRBOOK;
	    varnum           = V_GLOB_ADDRBOOK;
	    anum1_rel        = anum1 - as.how_many_personals;
	    anum2_rel        = anum2 - as.how_many_personals;
	}

	/* allocate for new list, same size as old list */
	new_list = (char **)fs_get((how_many_in_list + 1) * sizeof(char *));

	/* fill in new_list */
	for(i = 0; i < how_many_in_list; i++){
	    /* swap anum1 and anum2 */
	    if(i == anum1_rel)
	      j = anum2_rel;
	    else if(i == anum2_rel)
	      j = anum1_rel;
	    else
	      j = i;

	    new_list[i] = cpystr(list[j]);
	}

	new_list[i] = NULL;

	if(set_variable_list(varnum, new_list, TRUE, ew)){
	    if(msg)
	      *msg = cpystr(cancel_msg);

	    /* restore old values */
	    set_current_val(&vars[varnum], TRUE, FALSE);
	    free_list_array(&new_list);
	    return(retval);
	}

	retval = 0;
	set_current_val(&vars[varnum], TRUE, FALSE);
	free_list_array(&new_list);

	/* Swap PerAddrBook structs */
	tmppab = as.adrbks[anum1];
	as.adrbks[anum1] = as.adrbks[anum2];
	as.adrbks[anum2] = tmppab;
    }
    else if(type1 == Pers && type2 == Glob ||
            type1 == Glob && type2 == Pers){
	int how_many_in_srclist, how_many_in_dstlist;
	int srcvarnum, dstvarnum, srcanum;
	int cnt, warn_about_revert = 0;
	char **t;
	char **new_src, **new_dst, **srclist, **dstlist;
	char   tmp[200];
	enum {NotSet, Modify, RevertToDefault, OverRideDefault} modify_config;

	/*
	 * how_many_in_srclist = # in orig src list (Pers or Glob list).
	 * how_many_in_dstlist = # in orig dst list
	 * srcanum = # of highlighted addrbook that is being shuffled
	 */
	if(type1 == Pers){
	    how_many_in_srclist = as.how_many_personals;
	    how_many_in_dstlist = as.n_addrbk - as.how_many_personals;
	    srclist             = VAR_ADDRESSBOOK;
	    dstlist             = VAR_GLOB_ADDRBOOK;
	    srcvarnum           = V_ADDRESSBOOK;
	    dstvarnum           = V_GLOB_ADDRBOOK;
	    srcanum             = as.how_many_personals - 1;
	    *slide              = (how_many_in_srclist == 1)
				    ? (LINES_PER_ADD_LINE + XTRA_LINES_BETWEEN)
				    : XTRA_LINES_BETWEEN;
	}
	else{
	    how_many_in_srclist = as.n_addrbk - as.how_many_personals;
	    how_many_in_dstlist = as.how_many_personals;
	    srclist             = VAR_GLOB_ADDRBOOK;
	    dstlist             = VAR_ADDRESSBOOK;
	    srcvarnum           = V_GLOB_ADDRBOOK;
	    dstvarnum           = V_ADDRESSBOOK;
	    srcanum             = as.how_many_personals;
	    *slide              = (how_many_in_dstlist == 0)
				    ? (LINES_PER_ADD_LINE + XTRA_LINES_BETWEEN)
				    : XTRA_LINES_BETWEEN;
	    *slide              = -1 * (*slide);
	}


	modify_config = Modify;
	if(how_many_in_srclist == 1){
	    /*
	     * Deal with reverting to default values of the address book
	     * variables, or with user deleting a default value.
	     */
	    modify_config = NotSet;
	    
	    /*
	     * Count how many address books are in the user's config.
	     * This has to be one or zero, because how_many_in_srclist == 1.
	     */
	    cnt = 0;
	    lval = LVAL(&vars[srcvarnum], ew);
	    if(lval && lval[0])
	      for(t = lval; *t != NULL; t++)
		cnt++;
	    
	    /*
	     * We'll revert to the default if it exists, and warn
	     * the user about that.
	     */
	    if(cnt == 1){
		modify_config = RevertToDefault;
		/* see if there's a default to revert to */
		cnt = 0;
		if(vars[srcvarnum].global_val.l &&
		   vars[srcvarnum].global_val.l[0])
		  for(t = vars[srcvarnum].global_val.l; *t != NULL; t++)
		    cnt++;
		
		warn_about_revert = cnt;
		if(warn_about_revert > 1 && type1 == Pers)
		  *slide = LINES_PER_ABOOK * warn_about_revert +
						      XTRA_LINES_BETWEEN;
	    }
	    /*
	     * User is already using the default.
	     */
	    else if(cnt == 0){
		modify_config = OverRideDefault;
	    }
	}

	/*
	 * We're adding one to the dstlist, so need how_many + 1 + 1.
	 */
	new_dst = (char **)fs_get((how_many_in_dstlist + 2) * sizeof(char *));
	j = 0;

	/*
	 * Because the Personal list comes before the Global list, when
	 * we move to Global we're inserting a new first element into
	 * the global list (the dstlist).
	 *
	 * When we move from Global to Personal, we're appending a new
	 * last element onto the personal list (the dstlist).
	 */
	if(type2 == Glob)
	  new_dst[j++] = cpystr(srclist[how_many_in_srclist-1]);

	for(i = 0; i < how_many_in_dstlist; i++)
	  new_dst[j++] = cpystr(dstlist[i]);

	if(type2 == Pers)
	  new_dst[j++] = cpystr(srclist[0]);

	new_dst[j] = NULL;

	/*
	 * The srclist is complicated by the reverting to default
	 * behaviors.
	 */
	if(modify_config == Modify){
	    /*
	     * In this case we're just removing one from the srclist
	     * so the new_src is of size how_many -1 +1.
	     */
	    new_src = (char **)fs_get((how_many_in_srclist) * sizeof(char *));
	    j = 0;

	    for(i = 0; i < how_many_in_srclist-1; i++)
	      new_src[j++] = cpystr(srclist[i + ((type1 == Glob) ? 1 : 0)]);

	    new_src[j] = NULL;
	}
	else if(modify_config == OverRideDefault){
	    /*
	     * We were using default and will now revert to nothing.
	     */
	    new_src = (char **)fs_get(2 * sizeof(char *));
	    new_src[0] = cpystr("");
	    new_src[1] = NULL;
	}
	else if(modify_config == RevertToDefault){
	    /*
	     * We are moving our last user variable out and reverting
	     * to the default value for this variable.
	     */
	    new_src = NULL;
	}

	if(set_variable_list(dstvarnum, new_dst, TRUE, ew) ||
	   set_variable_list(srcvarnum, new_src, TRUE, ew)){
	    if(msg)
	      *msg = cpystr(cancel_msg);

	    /* restore old values */
	    set_current_val(&vars[dstvarnum], TRUE, FALSE);
	    set_current_val(&vars[srcvarnum], TRUE, FALSE);
	    free_list_array(&new_dst);
	    free_list_array(&new_src);
	    return(retval);
	}
	
	set_current_val(&vars[dstvarnum], TRUE, FALSE);
	set_current_val(&vars[srcvarnum], TRUE, FALSE);
	free_list_array(&new_dst);
	free_list_array(&new_src);

	retval = (type1 == Pers && warn_about_revert)
		    ? (warn_about_revert + 1) : (srcanum + 1);

	/*
	 * This is a tough case. We're adding one or more new address books
	 * in this case so we need to reset the addrbooks and start over.
	 * We return the number of the address book we just moved after the
	 * reset so that the caller can focus attention on the moved one.
	 * Actually, we return 1+the number so that we can tell it apart
	 * from a return of zero, which just means everything is ok.
	 */
	if(warn_about_revert){
	    sprintf(tmp,
     "This address book now %.20s, reverting to default %.20s address %.20s",
		    (type1 == Glob) ? "Personal" : "Global",
		    (type1 == Glob) ? "Global" : "Personal",
		    warn_about_revert > 1 ? "books" : "book");
	    if(msg)
	      *msg = cpystr(tmp);
	}
	else{
	    /*
	     * Modify PerAddrBook struct and adjust boundary.
	     * In this case we aren't swapping two addrbooks, but just modifying
	     * one from being global to personal or the reverse. It will
	     * still be the same element in the as.adrbks array.
	     */
	    pab = &as.adrbks[srcanum];
	    if(type2 == Glob){
		as.how_many_personals--;
		pab->type |= GLOBAL;
		if(pab->access != NoAccess)
		  pab->access = ReadOnly;
	    }
	    else{
		as.how_many_personals++;
		pab->type &= ~GLOBAL;
		if(pab->access != NoAccess && pab->access != MaybeRorW)
		  pab->access = ReadWrite;
	    }

	    sprintf(tmp,
		    "This address book now %.20s",
		    (type1 == Glob) ? "Personal" : "Global");
	    if(msg)
	      *msg = cpystr(tmp);
	}
    }

    return(retval);
}


int
ab_compose_to_addr(cur_line, agg, allow_role)
    long         cur_line;
    int          agg;
    int          allow_role;
{
    AddrScrn_Disp *dl;
    AdrBk_Entry   *abe;
    SAVE_STATE_S   state;
    BuildTo        bldto;

    dprint(2, (debugfile, "- ab_compose_to_addr -\n"));

    save_state(&state);

    bldto.type    = Str;
    bldto.arg.str = NULL;

    if(agg){
	int    i;
	size_t incr = 100, avail, alloced;
	char  *to = NULL;

	to = (char *)fs_get(incr);
	*to = '\0';
	avail = incr;
	alloced = incr;

	/*
	 * Run through all of the selected entries
	 * in all of the address books.
	 * Put the nicknames together into one long
	 * string with comma separators.
	 */
	for(i = 0; i < as.n_addrbk; i++){
	    adrbk_cntr_t num;
	    PerAddrBook *pab;
	    EXPANDED_S  *next_one;

	    pab = &as.adrbks[i];
	    if(pab->address_book)
	      next_one = pab->address_book->selects;
	    else
	      continue;

	    while((num = entry_get_next(&next_one)) != NO_NEXT){
		char          *a_string;
		AddrScrn_Disp  fake_dl;

		abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)num, Normal);

		/*
		 * Since we're picking up address book entries
		 * directly from the address books and have
		 * no knowledge of the display lines they came
		 * from, we don't know the dl's that go with
		 * them.  We need to pass a dl to abe_to_nick
		 * but it really is only going to use the
		 * type in this case.
		 */
		dl = &fake_dl;
		dl->type = (abe->tag == Single) ? Simple : ListHead;
		a_string = abe_to_nick_or_addr_string(abe, dl, i);

		while(abe && avail < (size_t)strlen(a_string)+1){
		    alloced += incr;
		    avail   += incr;
		    fs_resize((void **)&to, alloced);
		}

		if(!*to)
		  strcpy(to, a_string);
		else{
		    strcat(to, ",");
		    strcat(to, a_string);
		}

		avail -= (strlen(a_string) + 1);
		fs_give((void **)&a_string);
	    }
	}

	bldto.type    = Str;
	bldto.arg.str = to;
    }
    else{
	if(is_addr(cur_line)){

	    dl  = dlist(cur_line);
	    abe = ae(cur_line);

	    if(dl->type == ListEnt){
		bldto.type    = Str;
		bldto.arg.str = cpystr(listmem(cur_line));
	    }
	    else{
		bldto.type    = Abe;
		bldto.arg.abe = abe;
	    }
	}
    }

    if(bldto.type == Str && bldto.arg.str == NULL)
      bldto.arg.str = cpystr("");

    ab_compose_internal(bldto, allow_role);

    restore_state(&state);

    if(bldto.type == Str && bldto.arg.str)
      fs_give((void **)&bldto.arg.str);
    
    /*
     * Window size may have changed in composer.
     * Pine_send will have reset the window size correctly,
     * but we still have to reset our address book data structures.
     */
    ab_resize();
    ps_global->mangled_screen = 1;
    return(1);
}


/*
 * Used by the two compose routines.
 */
void
ab_compose_internal(bldto, allow_role)
    BuildTo        bldto;
    int            allow_role;
{
    int		   good_addr;
    char          *addr, *fcc, *error = NULL;
    ACTION_S      *role = NULL;
    void (*prev_screen)() = ps_global->prev_screen,
	 (*redraw)() = ps_global->redrawer;

    if(allow_role)
      ps_global->redrawer = NULL;

    ps_global->next_screen = SCREEN_FUN_NULL;

    fcc  = NULL;
    addr = NULL;

    good_addr = (our_build_address(bldto, &addr, &error, &fcc, 0) >= 0);
	
    if(error){
	q_status_message1(SM_ORDER, 3, 4, "%.200s", error);
	fs_give((void **)&error);
    }

    if(!good_addr && addr && *addr)
      fs_give((void **)&addr); /* relying on fs_give setting addr to NULL */

    if(allow_role){
	/* Setup role */
	if(role_select_screen(ps_global, &role, MC_COMPOSE) < 0){
	    cmd_cancelled("Composition");
	    ps_global->next_screen = prev_screen;
	    ps_global->redrawer = redraw;
	    return;
	}

	/*
	 * If default role was selected (NULL) we need to make up a role which
	 * won't do anything, but will cause compose_mail to think there's
	 * already a role so that it won't try to confirm the default.
	 */
	if(role)
	  role = copy_action(role);
	else{
	    role = (ACTION_S *)fs_get(sizeof(*role));
	    memset((void *)role, 0, sizeof(*role));
	    role->nick = cpystr("Default Role");
	}
    }

    compose_mail(addr, fcc, role, NULL, NULL);

    if(addr)
      fs_give((void **)&addr);

    if(fcc)
      fs_give((void **)&fcc);
}


/*
 * Export addresses into a file.
 *
 * Args: cur_line     -- The current line position (in global display list)
 *			 of cursor
 *       command_line -- The screen line on which to prompt
 *
 * Returns -- 1 if the export is done
 *            0 if not
 */
int
ab_export(ps, cur_line, command_line, agg)
    struct pine *ps;
    long cur_line;
    int  command_line;
    int  agg;
{
    int		   ret = 0, i, retflags = GER_NONE;
    int            r, orig_errno, failure = 0;
    struct variable *vars = ps->vars;
    char     filename[MAXPATH+1], full_filename[MAXPATH+1];
    STORE_S *store;
    gf_io_t  pc;
    long     start_of_append;
    char    *addr = NULL, *error = NULL;
    BuildTo  bldto;
    char    *p;
    int      good_addr, plur, vcard = 0, tab = 0;
    AdrBk_Entry *abe;
    VCARD_INFO_S *vinfo;
    static ESCKEY_S ab_export_opts[] = {
	{ctrl('T'), 10, "^T", "To Files"},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};
    static ESCKEY_S vcard_or_addresses[] = {
	{'a', 'a', "A", "Address List"},
	{'v', 'v', "V", "VCard"},
	{'t', 't', "T", "TabSep"},
	{-1, 0, NULL, NULL}};


    dprint(2, (debugfile, "- ab_export -\n"));

    if(ps->restricted){
	q_status_message(SM_ORDER, 0, 3,
	    "Pine demo can't export addresses to files");
	return(ret);
    }

    while(1){
	i = radio_buttons("Export list of addresses, vCard format, or Tab Separated ? ",
			  command_line, vcard_or_addresses, 'a', 'x',
			  NO_HELP, RB_NORM|RB_RET_HELP, NULL);
	if(i == 3){
	    helper(h_ab_export_vcard, "HELP FOR EXPORT FORMAT",
		   HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	}
	else
	  break;
    }

    switch(i){
      case 'x':
	cancel_warning(NO_DING, "export");
	return(ret);
    
      case 'a':
	break;

      case 'v':
	vcard++;
	break;

      case 't':
	tab++;
	break;

      default:
	q_status_message(SM_ORDER, 3, 3, "can't happen in ab_export");
	return(ret);
    }

    if(agg)
      plur = 1;
    else{
	abe = ae(cur_line);
	plur = (abe && abe->tag == List);
    }

    filename[0] = '\0';
    r = 0;

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	ab_export_opts[++r].ch  =  ctrl('I');
	ab_export_opts[r].rval  = 11;
	ab_export_opts[r].name  = "TAB";
	ab_export_opts[r].label = "Complete";
    }

    ab_export_opts[++r].ch = -1;

    r = get_export_filename(ps, filename, NULL, full_filename, sizeof(filename),
			    plur ? "addresses" : "address",
			    "EXPORT", ab_export_opts,
			    &retflags, command_line, GE_IS_EXPORT);

    if(r < 0){
	switch(r){
	  case -1:
	    cancel_warning(NO_DING, "export");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
		"Can't export to file outside of %.200s", VAR_OPER_DIR);
	    break;
	}

	goto fini;
    }

    dprint(5, (debugfile, "Opening file \"%s\" for export\n",
	   full_filename ? full_filename : "?"));

    if(!(store = so_get(FileStar, full_filename, WRITE_ACCESS))){
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
		  "Error opening file \"%.200s\" for address export: %.200s",
		  full_filename, error_description(errno));
	goto fini;
    }

    /*
     * The write_single_vcard_entry function wants a pc.
     */
    if(vcard || tab)
      gf_set_so_writec(&pc, store);

    start_of_append = so_tell(store);

    if(agg){
	for(i = 0; !failure && i < as.n_addrbk; i++){
	    adrbk_cntr_t num;
	    PerAddrBook *pab;
	    EXPANDED_S  *next_one;

	    pab = &as.adrbks[i];
	    if(pab->address_book)
	      next_one = pab->address_book->selects;
	    else
	      continue;

	    while(!failure && (num = entry_get_next(&next_one)) != NO_NEXT){

		abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)num, Normal);
		if((vcard || tab) && abe){
		    /*
		     * There is no place to store the charset information
		     * so we don't ask for it.
		     */
		    if(!(vinfo=prepare_abe_for_vcard(ps, abe, 1)))
		      failure++;
		    else{
			if(vcard)
			  write_single_vcard_entry(ps, pc, NULL, vinfo);
			else
			  write_single_tab_entry(pc, vinfo);

			free_vcard_info(&vinfo);
		    }
		}
		else if(abe){
		    bldto.type    = Abe;
		    bldto.arg.abe = abe;
		    error         = NULL;
		    addr          = NULL;
		    good_addr = (our_build_address(bldto,&addr,&error,NULL,0)
									  >= 0);
	
		    if(error){
			q_status_message1(SM_ORDER, 0, 4, "%.200s", error);
			fs_give((void **)&error);
		    }

		    /* rfc1522_decode the addr */
		    if(addr){
			size_t len;

			len = strlen(addr)+1;
			p = (char *)fs_get(len * sizeof(char));
			if(rfc1522_decode((unsigned char *)p,len,addr,NULL) ==
							    (unsigned char *)p){
			    fs_give((void **)&addr);
			    addr = p;
			}
			else
			  fs_give((void **)&p);
		    }

		    if(good_addr){
			int   quoted = 0;

			/*
			 * Change the unquoted commas into newlines.
			 * Not worth it to do complicated quoting,
			 * just consider double quotes.
			 */
			for(p = addr; *p; p++){
			    if(*p == '"')
			      quoted = !quoted;
			    else if(!quoted && *p == ','){
				*p++ = '\n';
				removing_leading_white_space(p);
				p--;
			    }
			}

			if(!so_puts(store, addr) || !so_puts(store, NEWLINE)){
			    orig_errno = errno;
			    failure    = 1;
			}
		    }

		    if(addr)
		      fs_give((void **)&addr);
		}
	    }
	}
    }
    else{
	AddrScrn_Disp *dl;

	dl  = dlist(cur_line);
	abe = ae(cur_line);
	if((vcard || tab) && abe){
	    if(!(vinfo=prepare_abe_for_vcard(ps, abe, 1)))
	      failure++;
	    else{
		if(vcard)
		  write_single_vcard_entry(ps, pc, NULL, vinfo);
		else
		  write_single_tab_entry(pc, vinfo);

		free_vcard_info(&vinfo);
	    }
	}
	else{

	    if(dl->type == ListHead && listmem_count_from_abe(abe) == 0){
		error = "List is empty, nothing to export!";
		good_addr = 0;
	    }
	    else if(dl->type == ListEnt){
		bldto.type    = Str;
		bldto.arg.str = listmem(cur_line);
		good_addr = (our_build_address(bldto,&addr,&error,NULL,0) >= 0);
	    }
	    else{
		bldto.type    = Abe;
		bldto.arg.abe = abe;
		good_addr = (our_build_address(bldto,&addr,&error,NULL,0) >= 0);
	    }

	    if(error){
		q_status_message1(SM_ORDER, 3, 4, "%.200s", error);
		fs_give((void **)&error);
	    }

	    /* Have to rfc1522_decode the addr */
	    if(addr){
		size_t len;
		len = strlen(addr)+1;
		p = (char *)fs_get(len * sizeof(char));
		if(rfc1522_decode((unsigned char *)p,len,addr,NULL) ==
							(unsigned char *)p){
		    fs_give((void **)&addr);
		    addr = p;
		}
		else
		  fs_give((void **)&p);
	    }

	    if(good_addr){
		int   quoted = 0;

		/*
		 * Change the unquoted commas into newlines.
		 * Not worth it to do complicated quoting,
		 * just consider double quotes.
		 */
		for(p = addr; *p; p++){
		    if(*p == '"')
		      quoted = !quoted;
		    else if(!quoted && *p == ','){
			*p++ = '\n';
			removing_leading_white_space(p);
			p--;
		    }
		}

		if(!so_puts(store, addr) || !so_puts(store, NEWLINE)){
		    orig_errno = errno;
		    failure    = 1;
		}
	    }

	    if(addr)
	      fs_give((void **)&addr);
	}
    }

    if(vcard || tab)
      gf_clear_so_writec(store);

    if(so_give(&store))				/* release storage */
      failure++;

    if(failure){
#ifndef	DOS
	truncate(full_filename, (off_t)start_of_append);
#endif
	dprint(1, (debugfile, "FAILED Export: file \"%s\" : %s\n",
	       full_filename ? full_filename : "?",
	       error_description(orig_errno)));
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
		  "Error exporting to \"%.200s\" : %.200s",
		  filename, error_description(orig_errno));
    }
    else{
	ret = 1;
	q_status_message3(SM_ORDER,0,3,
			  "%.200s %.200s to file \"%.200s\"",
			  (vcard || tab) ? (agg ? "Entries" : "Entry")
			        : (plur ? "Addresses" : "Address"),
			  retflags & GER_OVER
			      ? "overwritten"
			      : retflags & GER_APPEND ? "appended" : "exported",
			  filename);
    }

fini:
    ps->mangled_footer = 1;
    return(ret);
}


/*
 * Forward an address book entry or entries via email attachment.
 *
 *   We use the vCard standard to send entries. We group multiple entries
 * using the BEGIN/END construct of vCard, not with multiple MIME parts.
 * A limitation of vCard is that there can be only one charset for the
 * whole group we send, so we might lose that information.
 *
 * Args: cur_line     -- The current line position (in global display list)
 *			 of cursor
 *       command_line -- The screen line on which to prompt
 */
int
ab_forward(ps, cur_line, agg)
    struct pine *ps;
    long         cur_line;
    int          agg;
{
    AddrScrn_Disp *dl;
    AdrBk_Entry   *abe;
    ENVELOPE      *outgoing = NULL;
    BODY          *pb, *body = NULL;
    PART         **pp;
    char          *sig;
    char           charset[200+1];
    gf_io_t        pc;
    int            i, ret = 0, multiple_charsets_used = 0;
    VCARD_INFO_S  *vinfo;
    ACTION_S      *role = NULL;

    dprint(2, (debugfile, "- ab_forward -\n"));

    charset[0] = '\0';

    if(!agg){
	dl  = dlist(cur_line);
	if(dl->type != ListHead && dl->type != Simple)
	  return(ret);

	abe = ae(cur_line);
	if(!abe){
	    q_status_message(SM_ORDER, 3, 3, "Trouble accessing current entry");
	    return(ret);
	}
    }

    outgoing             = mail_newenvelope();
    outgoing->message_id = generate_message_id();
    if(agg && as.selections > 1)
      outgoing->subject = cpystr("Forwarded address book entries from Pine");
    else
      outgoing->subject = cpystr("Forwarded address book entry from Pine");

    body                                      = mail_newbody();
    body->type                                = TYPEMULTIPART;
    /*---- The TEXT part/body ----*/
    body->nested.part                       = mail_newbody_part();
    body->nested.part->body.type            = TYPETEXT;
    /*--- Allocate an object for the body ---*/
    if(body->nested.part->body.contents.text.data =
				(void *)so_get(PicoText, NULL, EDIT_ACCESS)){
	int       did_sig = 0;
	long      rflags = ROLE_COMPOSE;
	PAT_STATE dummy;

	pp = &(body->nested.part->next);

	if(nonempty_patterns(rflags, &dummy)){
	    /*
	     * This is really more like Compose, even though it
	     * is called Forward.
	     */
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{			/* cancel */
		role = NULL;
		cmd_cancelled("Composition");
		goto bomb;
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4, "Composing using role \"%.200s\"",
			    role->nick);

	if(sig = detoken(role, NULL, 2, 0, 1, NULL, NULL)){
	    if(*sig){
		so_puts((STORE_S *)body->nested.part->body.contents.text.data,
			sig);
		did_sig++;
	    }

	    fs_give((void **)&sig);
	}

	/* so we don't have an empty part */
	if(!did_sig)
	  so_puts((STORE_S *)body->nested.part->body.contents.text.data, "\n");
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Problem creating space for message text");
	goto bomb;
    }


    /*---- create the attachment, and write abook entry into it ----*/
    *pp             = mail_newbody_part();
    pb              = &((*pp)->body);
    pb->type        = TYPETEXT;
    pb->encoding    = ENCOTHER;  /* let data decide */
    pb->id          = generate_message_id();
    pb->subtype     = cpystr("DIRECTORY");
    if(agg && as.selections > 1)
      pb->description = cpystr("Pine addressbook entries");
    else
      pb->description = cpystr("Pine addressbook entry");

    pb->parameter            = mail_newbody_parameter();
    pb->parameter->attribute = cpystr("profile");
    pb->parameter->value     = cpystr("vCard");

    if(pb->contents.text.data = (void *)so_get(CharStar, NULL, EDIT_ACCESS)){
	int are_some_unqualified = 0, expand_nicks = 0;
	adrbk_cntr_t num;
	PerAddrBook *pab;
	EXPANDED_S  *next_one;
	 
	gf_set_so_writec(&pc, (STORE_S *) pb->contents.text.data);
    
	if(agg){
	    for(i = 0; i < as.n_addrbk && !are_some_unqualified; i++){

		pab = &as.adrbks[i];
		if(pab->address_book)
		  next_one = pab->address_book->selects;
		else
		  continue;
		
		while((num = entry_get_next(&next_one)) != NO_NEXT &&
		      !are_some_unqualified){

		    abe = adrbk_get_ae(pab->address_book,
				       (a_c_arg_t)num, Normal);
		    if(abe->tag == Single){
			if(abe->addr.addr && abe->addr.addr[0]
			   && !strindex(abe->addr.addr, '@'))
			  are_some_unqualified++;
		    }
		    else{
			char **ll;

			for(ll = abe->addr.list; ll && *ll; ll++){
			    if(!strindex(*ll, '@')){
				are_some_unqualified++;
				break;
			    }
			}
		    }
		}
	    }
	}
	else{
	    /*
	     * Search through the addresses to see if there are any
	     * that are unqualified, and so would be different if
	     * expanded.
	     */
	    if(abe->tag == Single){
		if(abe->addr.addr && abe->addr.addr[0]
		   && !strindex(abe->addr.addr, '@'))
		  are_some_unqualified++;
	    }
	    else{
		char **ll;

		for(ll = abe->addr.list; ll && *ll; ll++){
		    if(!strindex(*ll, '@')){
			are_some_unqualified++;
			break;
		    }
		}
	    }
	}

	if(are_some_unqualified){
	    switch(want_to("Expand nicknames", 'y', 'x', h_ab_forward,WT_NORM)){
	      case 'x':
		gf_clear_so_writec((STORE_S *) pb->contents.text.data);
		cancel_warning(NO_DING, "forward");
		goto bomb;
		
	      case 'y':
		expand_nicks = 1;
		break;
		
	      case 'n':
		expand_nicks = 0;
		break;
	    }

	    ps->mangled_footer = 1;
	}

	if(agg){
	    char cs[200+1];
	    char *csp = cs;

	    for(i = 0; i < as.n_addrbk; i++){

		pab = &as.adrbks[i];
		if(pab->address_book)
		  next_one = pab->address_book->selects;
		else
		  continue;
		
		while((num = entry_get_next(&next_one)) != NO_NEXT){

		    abe = adrbk_get_ae(pab->address_book,
				       (a_c_arg_t)num, Normal);
		    cs[0] = '\0';
		    if(!(vinfo=prepare_abe_for_vcard(ps, abe, expand_nicks))){
			gf_clear_so_writec((STORE_S *) pb->contents.text.data);
			goto bomb;
		    }
		    else{
			write_single_vcard_entry(ps, pc, &csp, vinfo);
			free_vcard_info(&vinfo);
		    }
		    
		    if(cs[0] && !multiple_charsets_used){
			if(strcmp(cs, "MULTI") == 0 ||
			   (charset[0] && strucmp(charset, cs) != 0))
			  multiple_charsets_used++;
			else if(charset[0] == '\0')
			  strncpy(charset, cs, 200);
		    }
		}
	    }
	}
	else{
	    char *csp = charset;
	    if(!(vinfo=prepare_abe_for_vcard(ps, abe, expand_nicks))){
		gf_clear_so_writec((STORE_S *) pb->contents.text.data);
		goto bomb;
	    }
	    else{
		write_single_vcard_entry(ps, pc, &csp, vinfo);
		free_vcard_info(&vinfo);
	    }

	    if(strcmp(charset, "MULTI") == 0)
	      multiple_charsets_used++;
	}

	if(multiple_charsets_used)
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
  "Entries use more than one character set, character set information is lost");

	/* This sets parameter charset, if necessary, and encoding */
	set_mime_type_by_grope(pb, (!multiple_charsets_used && charset[0])
							    ? charset : NULL);
	pb->size.bytes =
		strlen((char *)so_text((STORE_S *)pb->contents.text.data));
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Problem creating space for message text");
	goto bomb;
    }

    gf_clear_so_writec((STORE_S *) pb->contents.text.data);

    pine_send(outgoing, &body, "FORWARDING ADDRESS BOOK ENTRY", role, NULL,
 	      NULL, NULL, NULL, NULL, 0);
    
    ps->mangled_screen = 1;
    ret = 1;

bomb:
    if(outgoing)
      mail_free_envelope(&outgoing);

    if(body)
      pine_free_body(&body);
    
    free_action(&role);
    return(ret);
}


/*
 * Given and Adrbk_Entry fill in some of the fields in a VCARD_SUPL_S
 * for use by write_single_vcard_entry. The returned structure is freed
 * by the caller.
 */
VCARD_INFO_S *
prepare_abe_for_vcard(ps, abe, expand_nicks)
    struct pine   *ps;
    AdrBk_Entry   *abe;
    int            expand_nicks;
{
    VCARD_INFO_S *vinfo = NULL;
    char         *init_addr = NULL, *addr = NULL, *astring;
    int           cnt;
    ADDRESS      *adrlist = NULL;

    if(!abe)
      return(vinfo);

    vinfo = (VCARD_INFO_S *)fs_get(sizeof(VCARD_INFO_S));
    memset((void *)vinfo, 0, sizeof(VCARD_INFO_S));

    if(abe->nickname && abe->nickname[0]){
	vinfo->nickname = (char **)fs_get((1+1) * sizeof(char *));
	vinfo->nickname[0] = cpystr(abe->nickname);
	vinfo->nickname[1] = NULL;
    }

    if(abe->fcc && abe->fcc[0]){
	vinfo->fcc = (char **)fs_get((1+1) * sizeof(char *));
	vinfo->fcc[0] = cpystr(abe->fcc);
	vinfo->fcc[1] = NULL;
    }

    if(abe->extra && abe->extra[0]){
	vinfo->note = (char **)fs_get((1+1) * sizeof(char *));
	vinfo->note[0] = cpystr(abe->extra);
	vinfo->note[1] = NULL;
    }
    
    if(abe->fullname && abe->fullname[0]){
	char *fn, *last = NULL, *middle = NULL, *first = NULL;

	fn = adrbk_formatname(abe->fullname, &first, &last);
	if(fn){
	    if(*fn){
		vinfo->fullname = (char **)fs_get((1+1) * sizeof(char *));
		vinfo->fullname[0] = fn;
		vinfo->fullname[1] = NULL;
	    }
	    else
	      fs_give((void **)&fn);
	}

	if(last && *last){
	    if(first && (middle=strindex(first, ' '))){
		*middle++ = '\0';
		middle = skip_white_space(middle);
	    }

	    vinfo->last = last;
	    vinfo->first = first;
	    vinfo->middle = middle ? cpystr(middle) : NULL;
	    first = NULL;
	    last = NULL;
	}

	if(last)
	  fs_give((void **)&last);
	if(first)
	  fs_give((void **)&first);
    }

    /* expand nicknames and fully-qualify unqualified names */
    if(expand_nicks){
	char    *error = NULL;
	BuildTo  bldto;
	ADDRESS *a;

	if(abe->tag == Single)
	  init_addr = cpystr(abe->addr.addr);
	else{
	    char **ll;
	    char  *p;
	    long   length;

	    /* figure out how large a string we need to allocate */
	    length = 0L;
	    for(ll = abe->addr.list; ll && *ll; ll++)
	      length += (strlen(*ll) + 2);

	    if(length)
	      length -= 2L;
	    
	    init_addr = (char *)fs_get((size_t)(length+1L) * sizeof(char));
	    p = init_addr;
	
	    for(ll = abe->addr.list; ll && *ll; ll++){
		sstrcpy(&p, *ll);
		if(*(ll+1))
		  sstrcpy(&p, ", ");
	    }
	}

	bldto.type    = Str;
	bldto.arg.str = init_addr; 
	our_build_address(bldto, &addr, &error, NULL, 0);
	if(error){
	    q_status_message1(SM_ORDER, 3, 4, "%.200s", error);
	    fs_give((void **)&error);
	    free_vcard_info(&vinfo);
	    return(NULL);
	}

	if(addr)
	  rfc822_parse_adrlist(&adrlist, addr, ps->maildomain);
	
	for(cnt = 0, a = adrlist; a; a = a->next)
	  cnt++;

	vinfo->email = (char **)fs_get((cnt+1) * sizeof(char *));

	for(cnt = 0, a = adrlist; a; a = a->next){
	    char    *bufp;
	    ADDRESS *next_addr;

	    next_addr = a->next;
	    a->next = NULL;
	    bufp = (char *)fs_get((size_t)est_size(a));
	    astring = addr_string(a, bufp);
	    a->next = next_addr;
	    vinfo->email[cnt++] = cpystr(astring ? astring : "");
	    fs_give((void **)&bufp);
	}

	vinfo->email[cnt] = '\0';
    }
    else{ /* don't expand or qualify */
	if(abe->tag == Single){
	    astring =
	       (abe->addr.addr && abe->addr.addr[0]) ? abe->addr.addr : "";
	    vinfo->email = (char **)fs_get((1+1) * sizeof(char *));
	    vinfo->email[0] = cpystr(astring);
	    vinfo->email[1] = '\0';
	}
	else{
	    char **ll;

	    for(cnt = 0, ll = abe->addr.list; ll && *ll; ll++)
	      cnt++;

	    vinfo->email = (char **)fs_get((cnt+1) * sizeof(char *));
	    for(cnt = 0, ll = abe->addr.list; ll && *ll; ll++)
	      vinfo->email[cnt++] = cpystr(*ll);

	    vinfo->email[cnt] = '\0';
	}
    }

    return(vinfo);
}


void
free_vcard_info(vinfo)
    VCARD_INFO_S **vinfo;
{
    if(vinfo && *vinfo){
	if((*vinfo)->nickname)
	  free_list_array(&(*vinfo)->nickname);
	if((*vinfo)->fullname)
	  free_list_array(&(*vinfo)->fullname);
	if((*vinfo)->fcc)
	  free_list_array(&(*vinfo)->fcc);
	if((*vinfo)->note)
	  free_list_array(&(*vinfo)->note);
	if((*vinfo)->title)
	  free_list_array(&(*vinfo)->title);
	if((*vinfo)->tel)
	  free_list_array(&(*vinfo)->tel);
	if((*vinfo)->email)
	  free_list_array(&(*vinfo)->email);

	if((*vinfo)->first)
	  fs_give((void **)&(*vinfo)->first);
	if((*vinfo)->middle)
	  fs_give((void **)&(*vinfo)->middle);
	if((*vinfo)->last)
	  fs_give((void **)&(*vinfo)->last);

	fs_give((void **)vinfo);
    }
}


/*
 * Args   cset_return -- an array of size at least 200+1
 */
void
write_single_vcard_entry(ps, pc, cset_return, vinfo)
    struct pine   *ps;
    gf_io_t        pc;
    char         **cset_return;
    VCARD_INFO_S  *vinfo;
{
    char  *decoded, *cset, *charset, *tmp2, *tmp = NULL, *hdr;
    char **ll;
    char   charset_used[200+1];
    int    multiple_charsets_used = 0;
    int    i, did_fn = 0, did_n = 0;
    int    cr;
    char   eol[3];
#define FOLD_BY 75

    if(!vinfo)
      return;

#if defined(DOS) || defined(OS2)
      cr = 1;
#else
      cr = 0;
#endif

    if(cr)
      strcpy(eol, "\r\n");
    else
      strcpy(eol, "\n");

    gf_puts("BEGIN:VCARD", pc);
    gf_puts(eol, pc);
    gf_puts("VERSION:3.0", pc);
    gf_puts(eol, pc);

    charset_used[0] = '\0';
    
    for(i = 0; i < 7; i++){
	switch(i){
	  case 0:
	    ll = vinfo->nickname;
	    hdr = "NICKNAME:";
	    break;

	  case 1:
	    ll = vinfo->fullname;
	    hdr = "FN:";
	    break;

	  case 2:
	    ll = vinfo->email;
	    hdr = "EMAIL:";
	    break;

	  case 3:
	    ll = vinfo->title;
	    hdr = "TITLE:";
	    break;

	  case 4:
	    ll = vinfo->note;
	    hdr = "NOTE:";
	    break;

	  case 5:
	    ll = vinfo->fcc;
	    hdr = "X-FCC:";
	    break;

	  case 6:
	    ll = vinfo->tel;
	    hdr = "TEL:";
	    break;
	  
	  default:
	    panic("can't happen in write_single_vcard_entry");
	}

	for(; ll && *ll; ll++){
	    cset = charset = NULL;
	    decoded = (char *)rfc1522_decode((unsigned char *)(tmp_20k_buf),
					     SIZEOF_20KBUF, *ll, &cset);
	    if(decoded != *ll){  /* decoding was done */
		if(cset && cset[0])
		  charset = cset;
		else if(ps->VAR_CHAR_SET && ps->VAR_CHAR_SET[0])
		  charset = ps->VAR_CHAR_SET;
	    }

	    tmp = vcard_escape(decoded);
	    if(tmp){
		if(tmp2 = fold(tmp, FOLD_BY, FOLD_BY, cr, 1, hdr, " ")){
		    gf_puts(tmp2, pc);
		    fs_give((void **)&tmp2);
		    if(i == 1)
		      did_fn++;
		}

		fs_give((void **)&tmp);
	    }

	    if(charset && !multiple_charsets_used){
		if(charset_used[0] && strucmp(charset_used, charset) != 0)
		  multiple_charsets_used++;
		else if(charset_used[0] == '\0')
		  strncpy(charset_used, charset, 200);
	    }

	    if(cset)
	      fs_give((void **)&cset);
	}
    }

    if(vinfo->last && vinfo->last[0]){
	char *pl, *pf, *pm;

	pl = vcard_escape(vinfo->last);
	pf = (vinfo->first && *vinfo->first) ? vcard_escape(vinfo->first)
					     : NULL;
	pm = (vinfo->middle && *vinfo->middle) ? vcard_escape(vinfo->middle)
					       : NULL;
	sprintf(tmp_20k_buf, "%s%s%s%s%s",
		(pl && *pl) ? pl : "", 
		(pf && *pf || pm && *pm) ? ";" : "",
		(pf && *pf) ? pf : "", 
		(pm && *pm) ? ";" : "", 
		(pm && *pm) ? pm : "");

	if(tmp2 = fold(tmp_20k_buf, FOLD_BY, FOLD_BY, cr, 1, "N:", " ")){
	    gf_puts(tmp2, pc);
	    fs_give((void **)&tmp2);
	    did_n++;
	}

	if(pl)
	  fs_give((void **)&pl);
	if(pf)
	  fs_give((void **)&pf);
	if(pm)
	  fs_give((void **)&pm);
    }

    /*
     * These two types are required in draft-ietf-asid-mime-vcard-06, which
     * is April 98 and is in last call.
     */
    if(!did_fn || !did_n){
	if(did_n){
	    sprintf(tmp_20k_buf, "%s%s%s%s%s",
		    (vinfo->first && *vinfo->first) ? vinfo->first : "",
		    (vinfo->first && *vinfo->first &&
		     vinfo->middle && *vinfo->middle) ? " " : "", 
		    (vinfo->middle && *vinfo->middle) ? vinfo->middle : "",
		    ((vinfo->first && *vinfo->first ||
		      vinfo->middle && *vinfo->middle) &&
		     vinfo->last && *vinfo->last) ? " " : "", 
		    (vinfo->last && *vinfo->last) ? vinfo->last : "");

	    tmp = vcard_escape(tmp_20k_buf);
	    if(tmp){
		if(tmp2 = fold(tmp, FOLD_BY, FOLD_BY, cr, 1, "FN:", " ")){
		    gf_puts(tmp2, pc);
		    fs_give((void **)&tmp2);
		    did_n++;
		}

		fs_give((void **)&tmp);
	    }
	}
	else{
	    if(!did_fn){
		gf_puts("FN:<Unknown>", pc);
		gf_puts(eol, pc);
	    }

	    gf_puts("N:<Unknown>", pc);
	    gf_puts(eol, pc);
	}
    }

    gf_puts("END:VCARD", pc);
    gf_puts(eol, pc);

    if(cset_return && charset_used[0])
      strncpy(*cset_return, multiple_charsets_used ? "MULTI" : charset_used,
	      200);
}


/*
 * 
 */
void
write_single_tab_entry(pc, vinfo)
    gf_io_t        pc;
    VCARD_INFO_S  *vinfo;
{
    char  *decoded, *cset, *tmp = NULL;
    char **ll;
    int    i, first;
    char  *eol;

    if(!vinfo)
      return;

#if defined(DOS) || defined(OS2)
      eol = "\r\n";
#else
      eol = "\n";
#endif

    for(i = 0; i < 4; i++){
	switch(i){
	  case 0:
	    ll = vinfo->nickname;
	    break;

	  case 1:
	    ll = vinfo->fullname;
	    break;

	  case 2:
	    ll = vinfo->email;
	    break;

	  case 3:
	    ll = vinfo->note;
	    break;

	  default:
	    panic("can't happen in write_single_tab_entry");
	}

	if(i)
	  gf_puts("\t", pc);

	for(first = 1; ll && *ll; ll++){

	    cset = NULL;
	    decoded = (char *)rfc1522_decode((unsigned char *)(tmp_20k_buf),
					     SIZEOF_20KBUF, *ll, &cset);
	    tmp = vcard_escape(decoded);
	    if(tmp){
		if(i == 2 && !first)
		  gf_puts(",", pc);
		else
		  first = 0;

		gf_puts(tmp, pc);
		fs_give((void **)&tmp);
	    }

	    if(cset)
	      fs_give((void **)&cset);
	}
    }

    gf_puts(eol, pc);
}


/*
 * for ab_save percent done
 */
static int total_to_copy;
static int copied_so_far;
int
percent_done_copying()
{
    return((copied_so_far * 100) / total_to_copy);
}

int
cmp_action_list(a1, a2)
    const QSType *a1, *a2;
{
    ACTION_LIST_S *x = (ACTION_LIST_S *)a1;
    ACTION_LIST_S *y = (ACTION_LIST_S *)a2;

    if(x->pab != y->pab)
      return((x->pab > y->pab) ? 1 : -1);  /* order doesn't matter */
    
    /*
     * The only one that matters is when both x and y have dup lit.
     * For the others, just need to be consistent so sort will terminate.
     */
    if(x->dup){
	if(y->dup)
	  return((x->num_in_dst > y->num_in_dst) ? -1
		   : (x->num_in_dst == y->num_in_dst) ? 0 : 1);
	else
	  return(-1);
    }
    else if(y->dup)
      return(1);
    else
      return((x->num > y->num) ? -1 : (x->num == y->num) ? 0 : 1);
}


/*
 * Copy a bunch of address book entries to a particular address book.
 *
 * Args      abook -- the current addrbook handle
 *        cur_line -- the current line the cursor is on
 *    command_line -- the line to prompt on
 *             agg -- 1 if this is an aggregate copy
 *
 * Returns  1 if successful, 0 if not
 */
int
ab_save(ps, abook, cur_line, command_line, agg)
    struct pine *ps;
    AdrBk       *abook;
    long         cur_line;
    int          command_line;
    int          agg;
{
    PerAddrBook   *pab_dst, *pab;
    SAVE_STATE_S   state;  /* For saving state of addrbooks temporarily */
    int            rc, i;
    int		   how_many_dups = 0, how_many_to_copy = 0, skip_dups = 0;
    int		   how_many_no_action = 0, ret = 1;
    int		   err = 0, need_write = 0, we_cancel = 0;
    int            act_list_size, special_case = 0;
    adrbk_cntr_t   num, new_entry_num;
    char           warn[2][MAX_NICKNAME+1];
    char           warning[MAX_NICKNAME+1];
    char           tmp[max(200,2*MAX_NICKNAME+80)];
    ACTION_LIST_S *action_list = NULL, *al;
    static ESCKEY_S save_or_export[] = {
	{'s', 's', "S", "Save"},
	{'e', 'e', "E", "Export"},
	{-1, 0, NULL, NULL}};

    sprintf(tmp, "Save%.20s to address book or Export to filesystem ? ",
	    !agg               ? " highlighted entry" :
	      as.selections > 1  ? " selected entries" :
	        as.selections == 1 ? " selected entry" : "");
    i = radio_buttons(tmp, -FOOTER_ROWS(ps), save_or_export, 's', 'x',
		      h_ab_save_exp, RB_NORM, NULL);
    switch(i){
      case 'x':
	cancel_warning(NO_DING, "save");
	return(0);

      case 'e':
	return(ab_export(ps, cur_line, command_line, agg));

      case 's':
	break;

      default:
        q_status_message(SM_ORDER, 3, 3, "can't happen in ab_save");
	return(0);
    }

    pab_dst = setup_for_addrbook_add(&state, command_line, "Save");
    if(!pab_dst)
      goto get_out;

    pab = &as.adrbks[as.cur];

    dprint(2, (debugfile, "- ab_save: %s -> %s (agg=%d)-\n",
	   pab->nickname ? pab->nickname : "?",
	   pab_dst->nickname ? pab_dst->nickname : "?", agg));

    if(agg)
      act_list_size = as.selections;
    else
      act_list_size = 1;

    action_list = (ACTION_LIST_S *)fs_get((act_list_size+1) *
						    sizeof(ACTION_LIST_S));
    memset((void *)action_list, 0, (act_list_size+1) * sizeof(ACTION_LIST_S));
    al = action_list;

    if(agg){

	for(i = 0; i < as.n_addrbk; i++){
	    EXPANDED_S    *next_one;

	    pab = &as.adrbks[i];
	    if(pab->address_book)
	      next_one = pab->address_book->selects;
	    else
	      continue;

	    while((num = entry_get_next(&next_one)) != NO_NEXT){
		if(pab != pab_dst &&
		   pab->ostatus != Open &&
		   pab->ostatus != NoDisplay)
		  init_abook(pab, NoDisplay);

		if(pab->ostatus != Open && pab->ostatus != NoDisplay){
		    q_status_message1(SM_ORDER, 0, 4,
				 "Can't re-open address book %.200s to save from",
				 pab->nickname);
		    err++;
		    goto get_out;
		}

		set_act_list_member(al, (a_c_arg_t)num, pab_dst, pab, warning);
		if(al->skip)
		  how_many_no_action++;
		else{
		    if(al->dup){
			if(how_many_dups < 2 && warning[0]){
			    strncpy(warn[how_many_dups], warning, MAX_NICKNAME);
			    warn[how_many_dups][MAX_NICKNAME] = '\0';
			}
		    
			how_many_dups++;
		    }

		    how_many_to_copy++;
		}

		al++;
	    }
	}
    }
    else{
	if(is_addr(cur_line)){
	    AddrScrn_Disp *dl;

	    dl = dlist(cur_line);

	    if(dl->type == ListEnt)
	      special_case++;

	    if(pab && dl){
		num = dl->elnum;
		set_act_list_member(al, (a_c_arg_t)num, pab_dst, pab, warning);
	    }
	    else
	      al->skip = 1;

	    if(al->skip)
	      how_many_no_action++;
	    else{
		if(al->dup){
		    if(how_many_dups < 2 && warning[0]){
			strncpy(warn[how_many_dups], warning, MAX_NICKNAME);
			warn[how_many_dups][MAX_NICKNAME] = '\0';
		    }
		
		    how_many_dups++;
		}

		how_many_to_copy++;
	    }
	}
	else{
	    q_status_message(SM_ORDER, 0, 4, "No current entry to save");
	    goto get_out;
	}
    }

    if(how_many_to_copy == 0 && how_many_no_action == 1 && act_list_size == 1)
      special_case++;

    if(special_case){
	TA_STATE_S tas, *tasp;

	/* Not going to use the action_list now */
	if(action_list)
	  fs_give((void **)&action_list);

	tasp = &tas;
	tas.state = state;
	tas.pab   = pab_dst;
	take_this_one_entry(ps, &tasp, abook, cur_line);

	/*
	 * If take_this_one_entry or its children didn't do this for
	 * us, we do it here.
	 */
	if(tas.pab)
	  restore_state(&(tas.state));

	/*
	 * We don't have enough information to know what to return.
	 */
	return(0);
    }

    /* nothing to do (except for special Take case below) */
    if(how_many_to_copy == 0){
	if(how_many_no_action == 0){
	    err++;
	    goto get_out;
	}
	else{
	    restore_state(&state);

	    sprintf(tmp, "Saved %d %.10s to %.100s",
		    how_many_no_action,
		    how_many_no_action > 1 ? "entries" : "entry",
		    pab_dst->nickname);
	    q_status_message(SM_ORDER, 0, 4, tmp);
	    if(action_list)
	      fs_give((void **)&action_list);

	    return(ret);
	}
    }

    /*
     * If there are some nicknames which already exist in the selected
     * abook, ask user what to do.
     */
    if(how_many_dups > 0){
	if(how_many_dups == 1)
	  sprintf(tmp, "Entry with nickname \"%.*s\" already exists, replace ",
		  MAX_NICKNAME, warn[0]);
	else if(how_many_dups == 2)
	  sprintf(tmp,
		  "Nicknames \"%.*s\" and \"%.*s\" already exist, replace ",
		  MAX_NICKNAME, warn[0], MAX_NICKNAME, warn[1]);
	else
	  sprintf(tmp, "%d of the nicknames already exist, replace ",
		  how_many_dups);
	
	switch(want_to(tmp, 'n', 'x', h_ab_copy_dups, WT_NORM)){
	  case 'n':
	    skip_dups++;
	    if(how_many_to_copy == how_many_dups){
		restore_state(&state);
		if(action_list)
		  fs_give((void **)&action_list);

		cancel_warning(NO_DING, "save");
		return(ret);
	    }

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
     * the correct entries. In order to do that we'll sort the action_list
     * to give us a safe order.
     */
    if(!skip_dups && how_many_dups > 1)
      qsort((QSType *)action_list, (size_t)as.selections, sizeof(*action_list),
	    cmp_action_list);

    /*
     * Set up the busy alarm percent counters.
     */
    total_to_copy = how_many_to_copy - (skip_dups ? how_many_dups : 0);
    copied_so_far = 0;
    we_cancel = busy_alarm(1, "Saving entries",
			 (total_to_copy > 4) ? percent_done_copying : NULL, 1);

    /*
     * Add the list of entries to the destination abook.
     */
    for(al = action_list; al && al->pab; al++){
	AdrBk_Entry   *abe;
	
	if(al->skip || (skip_dups && al->dup))
	  continue;

	if(!(abe = adrbk_get_ae(al->pab->address_book,
				(a_c_arg_t)al->num, Normal))){
	    q_status_message1(SM_ORDER | SM_DING, 3, 5,
			      "Error saving entry: %.200s",
			      error_description(errno));
	    err++;
	    goto get_out;
	}

	/*
	 * Delete existing dups and replace them.
	 */
	if(al->dup){

	    /* delete the existing entry */
	    rc = 0;
	    if(adrbk_delete(pab_dst->address_book,
			    (a_c_arg_t)al->num_in_dst, 1, 0, 0, 0) == 0){
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
	 * Now we have a clean slate to work with.
	 * Add (sorted in correctly) or append abe to the destination
	 * address book.
	 */
	if(total_to_copy <= 1)
	  rc = adrbk_add(pab_dst->address_book,
		         NO_NEXT,
		         abe->nickname,
		         abe->fullname,
		         abe->tag == Single ? abe->addr.addr : NULL,
		         abe->fcc,
		         abe->extra,
		         abe->tag,
		         &new_entry_num,
		         (int *)NULL,
		         0,
		         0,
		         0);
	else
	  rc = adrbk_append(pab_dst->address_book,
		            abe->nickname,
		            abe->fullname,
		            abe->tag == Single ? abe->addr.addr : NULL,
		            abe->fcc,
		            abe->extra,
		            abe->tag,
			    &new_entry_num);

	if(rc == 0)
	  need_write++;
	
	/*
	 * If the entry we copied is a list, we also have to add
	 * the list members to the copy.
	 */
	if(rc == 0 && abe->tag == List){
	    int save_sort_rule;

	    /*
	     * We want it to copy the list in the exact order
	     * without sorting it.
	     */
	    save_sort_rule = pab_dst->address_book->sort_rule;
	    pab_dst->address_book->sort_rule = AB_SORT_RULE_NONE;

	    rc = adrbk_nlistadd(pab_dst->address_book,
				(a_c_arg_t)new_entry_num,
				abe->addr.list,
				0, 0, 0);

	    pab_dst->address_book->sort_rule = save_sort_rule;
	}

	if(rc != 0){
	    q_status_message2(SM_ORDER | SM_DING, 3, 5,
			     "Error saving %.200s: %.200s",
			     (abe && abe->nickname) ? abe->nickname
						      : "entry",
			     error_description(errno));
	    err++;
	    goto get_out;
	}

	copied_so_far++;
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

    restore_state(&state);
    if(action_list)
      fs_give((void **)&action_list);

    ps_global->mangled_footer = 1;

    if(err){
	ret = 0;
	if(need_write)
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
			   "Save only partially completed");
	else
	  cancel_warning(NO_DING, "save");
    }
    else if (how_many_to_copy + how_many_no_action -
	     (skip_dups ? how_many_dups : 0) > 0){

	ret = 1;
	sprintf(tmp, "Saved %d %.10s to %.100s",
		how_many_to_copy + how_many_no_action -
		(skip_dups ? how_many_dups : 0),
		((how_many_to_copy + how_many_no_action -
		  (skip_dups ? how_many_dups : 0)) > 1) ? "entries" : "entry",
		pab_dst->nickname);
	q_status_message(SM_ORDER, 0, 4, tmp);
    }

    return(ret);
}


/*
 * Warn should point to an array of size MAX_NICKNAME+1.
 */
void
set_act_list_member(al, numarg, pab_dst, pab, warn)
    ACTION_LIST_S *al;
    a_c_arg_t      numarg;
    PerAddrBook   *pab_dst, *pab;
    char          *warn;
{
    AdrBk_Entry   *abe1, *abe2;
    adrbk_cntr_t   num;

    num = (adrbk_cntr_t)numarg;

    al->pab = pab;
    al->num = num;

    /* skip if they're copying from and to same addrbook */
    if(pab == pab_dst)
      al->skip = 1;
    else{
	abe1 = adrbk_get_ae(pab->address_book, numarg, Normal);
	if(abe1 && abe1->nickname && abe1->nickname[0]){
	    adrbk_cntr_t dst_enum;

	    abe2 = adrbk_lookup_by_nick(pab_dst->address_book,
					abe1->nickname, &dst_enum);
	    /*
	     * This nickname already exists in the destn address book.
	     */
	    if(abe2){
		/* If it isn't different, no problem. Check it out. */
		if(abes_are_equal(abe1, abe2))
		  al->skip = 1;
		else{
		    strncpy(warn, abe1->nickname, MAX_NICKNAME);
		    warn[MAX_NICKNAME] = '\0';
		    al->dup = 1;
		    al->num_in_dst = dst_enum;
		}
	    }
	}
    }
}


/*
 * Print out the display list.
 */
int
ab_print(agg)
    int agg;
{
    int do_entry = 0, curopen;
    char *prompt;

    dprint(2, (debugfile, "- ab_print -\n"));

    curopen = cur_is_open();
    if(!agg && curopen){
	static ESCKEY_S prt[] = {
	    {'a', 'a', "A", "AddressBook"},
	    {'e', 'e', "E", "Entry"},
	    {-1, 0, NULL, NULL}};

	prompt = "Print Address Book or just this Entry? ";
	switch(radio_buttons(prompt, -FOOTER_ROWS(ps_global), prt, 'a', 'x',
			     NO_HELP, RB_NORM, NULL)){
	  case 'x' :
	    cancel_warning(NO_DING, "print");
	    ps_global->mangled_footer = 1;
	    return 0;

	  case 'e':
	    do_entry = 1;
	    break;

	  default:
	  case 'a':
	    break;
	}
    }

    prompt = agg ? "selected entries "
		 : !curopen ? "address book list "
			    : do_entry ? "entry "
			               : "address book ";

    if(open_printer(prompt) == 0){
	DL_CACHE_S     dlc_buf, *match_dlc;
	AddrScrn_Disp *dl; 
	AdrBk_Entry   *abe;
	long           save_line;
	char          *addr;
	char           spaces[INDENT + 1];
	char           more_spaces[INDENT + 2 + 1];

	strncpy(spaces, repeat_char(INDENT, SPACE), INDENT+1);
	strncpy(more_spaces, repeat_char(INDENT+2, SPACE), INDENT+3);

	save_line = as.top_ent + as.cur_row;
	match_dlc = get_dlc(save_line);
	dlc_buf   = *match_dlc;
	match_dlc = &dlc_buf;

	if(do_entry){		/* print an individual addrbook entry */

	    dl = dlist(save_line);
	    abe = ae(save_line);

	    if(abe){
		int      are_some_unqualified = 0, expand_nicks = 0;
		char    *string, *tmp;
		ADDRESS *adrlist = NULL;

		/*
		 * Search through the addresses to see if there are any
		 * that are unqualified, and so would be different if
		 * expanded.
		 */
		if(abe->tag == Single){
		    if(abe->addr.addr && abe->addr.addr[0]
		       && !strindex(abe->addr.addr, '@'))
		      are_some_unqualified++;
		}
		else{
		    char **ll;

		    for(ll = abe->addr.list; ll && *ll; ll++){
			if(!strindex(*ll, '@')){
			    are_some_unqualified++;
			    break;
			}
		    }
		}

		if(are_some_unqualified){
		    switch(want_to("Expand nicknames", 'y', 'x', h_ab_forward,
								    WT_NORM)){
		      case 'x':
			cancel_warning(NO_DING, "print");
			ps_global->mangled_footer = 1;
			return 0;
			
		      case 'y':
			expand_nicks = 1;
			break;
			
		      case 'n':
			expand_nicks = 0;
			break;
		    }
		}

		/* expand nicknames and fully-qualify unqualified names */
		if(expand_nicks){
		    char    *error = NULL;
		    BuildTo  bldto;
		    char    *init_addr = NULL;

		    if(abe->tag == Single)
		      init_addr = cpystr(abe->addr.addr);
		    else{
			char **ll;
			char  *p;
			long   length;

			/* figure out how large a string we need to allocate */
			length = 0L;
			for(ll = abe->addr.list; ll && *ll; ll++)
			  length += (strlen(*ll) + 2);

			if(length)
			  length -= 2L;
			
			init_addr = (char *)fs_get((size_t)(length+1L) *
								sizeof(char));
			p = init_addr;
		    
			for(ll = abe->addr.list; ll && *ll; ll++){
			    sstrcpy(&p, *ll);
			    if(*(ll+1))
			      sstrcpy(&p, ", ");
			}
		    }

		    bldto.type    = Str;
		    bldto.arg.str = init_addr; 
		    our_build_address(bldto, &addr, &error, NULL, 0);
		    if(init_addr)
		      fs_give((void **)&init_addr);

		    if(error){
			q_status_message1(SM_ORDER, 0, 4, "%.200s", error);
			fs_give((void **)&error);

			ps_global->mangled_footer = 1;
			return 0;
		    }

		    if(addr){
			rfc822_parse_adrlist(&adrlist, addr,
					     ps_global->maildomain);
			fs_give((void **)&addr);
		    }

		    /* Will use adrlist to do the printing below */
		}

		tmp = abe->nickname ? abe->nickname : "";
		string = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						SIZEOF_20KBUF, tmp, NULL);
		if(tmp = fold(string, 80, 80, 0, 0, "Nickname  : ", spaces)){
		    print_text(tmp);
		    fs_give((void **)&tmp);
		}

		tmp = abe->fullname ? abe->fullname : "";
		string = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						SIZEOF_20KBUF, tmp, NULL);
		if(tmp = fold(string, 80, 80, 0, 0, "Fullname  : ", spaces)){
		    print_text(tmp);
		    fs_give((void **)&tmp);
		}

		tmp = abe->fcc ? abe->fcc : "";
		string = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						SIZEOF_20KBUF, tmp, NULL);
		if(tmp = fold(string, 80, 80, 0, 0, "Fcc       : ", spaces)){
		    print_text(tmp);
		    fs_give((void **)&tmp);
		}

		tmp = abe->extra ? abe->extra : "";
		{unsigned char *p, *b = NULL;
		 size_t n, len;
		 if((n = strlen(tmp)) > SIZEOF_20KBUF-1){
		     len = n+1;
		     p = b = (unsigned char *)fs_get(len * sizeof(char));
		 }
		 else{
		     len = SIZEOF_20KBUF;
		     p = (unsigned char *)tmp_20k_buf;
		 }

		 string = (char *)rfc1522_decode(p, len, tmp, NULL);
		 if(tmp = fold(string, 80, 80, 0, 0, "Comment   : ", spaces)){
		    print_text(tmp);
		    fs_give((void **)&tmp);
		 }

		 if(b)
		   fs_give((void **)&b);
		}

		/*
		 * Print addresses
		 */

		if(expand_nicks){
		    ADDRESS *a;

		    for(a = adrlist; a; a = a->next){
			char    *bufp;
			ADDRESS *next_addr;

			next_addr = a->next;
			a->next = NULL;
			bufp = (char *)fs_get((size_t)est_size(a));
			tmp = addr_string(a, bufp);
			a->next = next_addr;
			string = (char *)rfc1522_decode((unsigned char *)
							tmp_20k_buf+10000,
							SIZEOF_20KBUF-10000,
							tmp, NULL);
			if(tmp = fold(string, 80, 80, 0, 0,
				      (a == adrlist) ? "Addresses : " : spaces,
				      more_spaces)){
			    print_text(tmp);
			    fs_give((void **)&tmp);
			}

			fs_give((void **)&bufp);
		    }

		    if(adrlist)
		      mail_free_address(&adrlist);
		    else
		      print_text("Addresses :\n");
		}
		else{ /* don't expand or qualify */
		    if(abe->tag == Single){
			tmp = abe->addr.addr ? abe->addr.addr : "";
			string = (char *)rfc1522_decode((unsigned char *)
							(tmp_20k_buf+10000),
							SIZEOF_20KBUF-10000,
						        tmp, NULL);
			if(tmp = fold(string, 80, 80, 0, 0, "Addresses : ",
				      more_spaces)){
			    print_text(tmp);
			    fs_give((void **)&tmp);
			}
		    }
		    else{
			char **ll;

			if(!abe->addr.list || !abe->addr.list[0])
			  print_text("Addresses :\n");

			for(ll = abe->addr.list; ll && *ll; ll++){
			    string = (char *)rfc1522_decode((unsigned char *)
							    (tmp_20k_buf+10000),
							    SIZEOF_20KBUF-10000,
						            *ll, NULL);
			    if(tmp = fold(string, 80, 80, 0, 0,
					  (ll == abe->addr.list)
					    ? "Addresses : " :  spaces,
					  more_spaces)){
				print_text(tmp);
				fs_give((void **)&tmp);
			    }
			}
		    }
		}
	    }
	}
	else{
	    long         lineno;
	    char         lbuf[MAX_SCREEN_COLS + 1];
	    char        *p;
	    int          i, savecur, savezoomed;
	    OpenStatus   savestatus;
	    PerAddrBook *pab;

	    if(agg){		/* print all selected entries */
		if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
		    savezoomed = as.zoomed;
		    /*
		     * Fool display code into thinking display is zoomed, so
		     * we'll skip the unselected entries. Since this feature
		     * causes all abooks to be displayed we don't have to
		     * step through each addrbook.
		     */
		    as.zoomed = 1;
		    warp_to_beginning();
		    lineno = 0L;
		    for(dl = dlist(lineno);
			dl->type != End;
			dl = dlist(++lineno)){

			switch(dl->type){
			  case Beginning:
			  case ListClickHere:
			  case ListEmpty:
			  case ClickHereCmb:
			  case Text:
			  case TitleCmb:
			  case Empty:
			  case ZoomEmpty:
			  case AskServer:
			    continue;
			}

			p = get_display_line(lineno,0,NULL,NULL,NULL,lbuf);
			print_text1("%s\n", p);
		    }

		    as.zoomed = savezoomed;
		}
		else{		/* print all selected entries */
		    savecur    = as.cur;
		    savezoomed = as.zoomed;
		    as.zoomed  = 1;

		    for(i = 0; i < as.n_addrbk; i++){
			pab = &as.adrbks[i];
			if(!(pab->address_book &&
			     any_selected(pab->address_book->selects)))
			  continue;
			
			/*
			 * Print selected entries from addrbook i.
			 * We have to put addrbook i into Open state so
			 * that the display code will work right.
			 */
			as.cur = i;
			savestatus = pab->ostatus;
			init_abook(pab, Open);
			init_disp_form(pab, ps_global->VAR_ABOOK_FORMATS, i);
			(void)calculate_field_widths();
			warp_to_beginning();
			lineno = 0L;

			for(dl = dlist(lineno);
			    dl->type != End;
			    dl = dlist(++lineno)){

			    switch(dl->type){
			      case Beginning:
			      case ListClickHere:
			      case ClickHereCmb:
				continue;
			    }

			    p = get_display_line(lineno,0,NULL,NULL,NULL,lbuf);
			    print_text1("%s\n", p);
			}

			init_abook(pab, savestatus);
		    }

		    as.cur     = savecur;
		    as.zoomed  = savezoomed;
		    /* restore the display for the current addrbook  */
		    init_disp_form(&as.adrbks[as.cur],
				   ps_global->VAR_ABOOK_FORMATS, as.cur);
		}
	    }
	    else{	/* print either the abook list or a single abook */
		int anum;
		DL_CACHE_S *dlc;

		savezoomed = as.zoomed;
		as.zoomed = 0;

		if(curopen){	/* print a single address book */
		    anum = adrbk_num_from_lineno(as.top_ent+as.cur_row);
		    warp_to_top_of_abook(anum);
		    if(F_ON(F_CMBND_ABOOK_DISP,ps_global))
		      lineno = 0L - XTRA_TITLE_LINES_IN_OLD_ABOOK_DISP;
		    else{
			print_text1("      ADDRESS BOOK %s\n\n",
				    as.adrbks[as.cur].nickname);
			lineno = 0L;
		    }
		}
		else{		/* print the list of address books */
		    warp_to_beginning();
		    lineno = 0L;
		}

		for(dl = dlist(lineno);
		    dl->type != End && (!curopen ||
					(anum==adrbk_num_from_lineno(lineno) &&
					 (as.n_serv == 0 ||
					  ((dlc=get_dlc(lineno)) &&
					   dlc->type != DlcDirDelim1))));
		    dl = dlist(++lineno)){

		    switch(dl->type){
		      case Beginning:
		      case ListClickHere:
		      case ClickHereCmb:
			continue;
		    }

		    p = get_display_line(lineno, 0, NULL, NULL, NULL, lbuf);
		    print_text1("%s\n", p);
		}

		as.zoomed = savezoomed;
	    }
	}

	close_printer();

	/*
	 * jump cache back to where we started so that the next
	 * request won't cause us to page through the whole thing
	 */
	if(!do_entry)
	  warp_to_dlc(match_dlc, save_line);

	ps_global->mangled_screen = 1;
    }

    ps_global->mangled_footer = 1;
    return 1;
}


/*
 * Delete address book entries.
 */
int
ab_agg_delete(ps, agg)
    struct pine *ps;
    int          agg;
{
    int ret = 0, i, ch, rc = 0;
    PerAddrBook *pab;
    adrbk_cntr_t num, ab_count;
    char prompt[80];

    dprint(2, (debugfile, "- ab_agg_delete -\n"));

    if(agg){
	sprintf(prompt, "Really delete %d selected entries", as.selections);
	ch = want_to(prompt, 'n', 'n', NO_HELP, WT_NORM);
	if(ch == 'y'){
	    adrbk_cntr_t   newelnum, flushelnum = NO_NEXT;
	    DL_CACHE_S     dlc_save, dlc_restart, *dlc;
	    int            we_cancel = 0;
	    int            top_level_display;
	    
	    /*
	     * We want to try to put the cursor in a reasonable position
	     * on the screen when we're done. If we are in the top-level
	     * display, then we can leave it the same. If we have an
	     * addrbook opened, then we want to see if we can get back to
	     * the same entry we are currently on.
	     */
	    if(!(top_level_display = !any_ab_open())){
		dlc = get_dlc(as.top_ent+as.cur_row);
		dlc_save = *dlc;
		newelnum = dlc_save.dlcelnum;
	    }
	    
	    we_cancel = busy_alarm(1, NULL, NULL, 0);

	    for(i = 0; i < as.n_addrbk && rc != -5; i++){
		int orig_selected, selected;

		pab = &as.adrbks[i];
		if(!pab->address_book)
		  continue;

		ab_count = adrbk_count(pab->address_book);
		rc = 0;
		selected = howmany_selected(pab->address_book->selects);
		orig_selected = selected;
		/*
		 * Because deleting an entry causes the addrbook to be
		 * immediately updated, we need to delete from higher entry
		 * numbers to lower numbers. That way, entry number n is still
		 * entry number n in the updated address book because we've
		 * only deleted entries higher than n.
		 */
		for(num = ab_count-1; selected > 0 && rc == 0; num--){
		    if(entry_is_selected(pab->address_book->selects,
				         (a_c_arg_t)num)){
			rc = adrbk_delete(pab->address_book, (a_c_arg_t)num,
					  1, 0, 0, 0);

			selected--;

			/*
			 * This is just here to help us reposition the cursor.
			 */
			if(!top_level_display && as.cur == i && rc == 0){
			    if(num >= newelnum)
			      flushelnum = num;
			    else
			      newelnum--;
			}
		    }
		}

		if(rc == 0 && orig_selected > 0)
		  rc = adrbk_write(pab->address_book, NULL, 1, 0, 1);

		if(rc && rc != -5){
		    q_status_message2(SM_ORDER | SM_DING, 3, 5,
			              "Error updating %.200s: %.200s",
				      (as.n_addrbk > 1) ? pab->nickname
						        : "address book",
				      error_description(errno));
		    dprint(1, (debugfile, "Error updating %s: %s\n",
			   pab->filename ? pab->filename : "?",
			   error_description(errno)));
		}
	    }

	    if(we_cancel)
	      cancel_busy_alarm(-1);
	    
	    if(rc == 0){
		q_status_message(SM_ORDER, 0, 2, "Deletions completed");
		ret = 1;
		erase_selections();
	    }

	    if(!top_level_display){
		int lost = 0;
		long new_ent;

		/*
		 * We ought to be able to optimize some of this by flushing
		 * instead of warping. We can figure out where the first
		 * good cache entry is. If we didn't delete any before the
		 * current entry {(newelnum == dlc_save.dlcelnum)} then we
		 * know that we can flush back to flushelnum and the cache
		 * should be ok. We could also optimize the screen redraw
		 * by figuring out where we have to start redrawing. We
		 * don't do any of this yet, we just do a warp.
		 */

		if(flushelnum != dlc_save.dlcelnum){
		    /*
		     * We didn't delete current so restart there. The elnum
		     * may have changed if we deleted entries above it.
		     */
		    dlc_restart = dlc_save;
		    dlc_restart.dlcelnum  = newelnum;
		}
		else{
		    /*
		     * Current was deleted.
		     */
		    dlc_restart.adrbk_num = as.cur;
		    pab = &as.adrbks[as.cur];
		    ab_count = adrbk_count(pab->address_book);
		    if(ab_count == 0)
		      dlc_restart.type = DlcEmpty;
		    else{
			AdrBk_Entry     *abe;

			dlc_restart.dlcelnum  = min(newelnum, ab_count-1);
			abe = adrbk_get_ae(pab->address_book,
					   (a_c_arg_t)dlc_restart.dlcelnum,
					   Normal);
			if(abe && abe->tag == Single)
			  dlc_restart.type = DlcSimple;
			else if(abe && abe->tag == List)
			  dlc_restart.type = DlcListHead;
			else
			  lost++;
		    }
		}

		if(lost){
		    warp_to_top_of_abook(as.cur);
		    as.top_ent = 0L;
		    new_ent    = first_selectable_line(0L);
		    if(new_ent == NO_LINE)
		      as.cur_row = 0L;
		    else
		      as.cur_row = new_ent;
		    
		    /* if it is off screen */
		    if(as.cur_row >= as.l_p_page){
			as.top_ent += (as.cur_row - as.l_p_page + 1);
			as.cur_row  = (as.l_p_page - 1);
		    }
		}
		else if(dlc_restart.type != DlcEmpty &&
			dlc_restart.dlcelnum == dlc_save.dlcelnum &&
			(F_OFF(F_CMBND_ABOOK_DISP,ps_global) || as.cur == 0)){
		    /*
		     * Didn't delete any before this line.
		     * Leave screen about the same. (May have deleted current.)
		     */
		    warp_to_dlc(&dlc_restart, as.cur_row+as.top_ent);
		}
		else{
		    warp_to_dlc(&dlc_restart, 0L);
		    /* put in middle of screen */
		    as.top_ent = first_line(0L - (long)as.l_p_page/2L);
		    as.cur_row = 0L - as.top_ent;
		}

		ps->mangled_body = 1;
	    }
	}
	else
	  cmd_cancelled("Apply Delete command");
    }

    return(ret);
}


/*
 * Delete an entry from the address book
 *
 * Args: abook        -- The addrbook handle into access library
 *       command_line -- The screen line on which to prompt
 *       cur_line     -- The entry number in the display list
 *       warped       -- We warped to a new part of the addrbook
 *
 * Result: returns 1 if an entry was deleted, 0 if not.
 *
 * The main routine above knows what to repaint because it's always the
 * current entry that's deleted.  Here confirmation is asked of the user
 * and the appropriate adrbklib functions are called.
 */
int
single_entry_delete(abook, cur_line, warped)
    AdrBk *abook;
    long   cur_line;
    int   *warped;
{
    char   ch, *cmd, *dname;
    char   prompt[200];
    int    rc;
    register AddrScrn_Disp *dl;
    AdrBk_Entry     *abe;
    DL_CACHE_S      *dlc_to_flush;

    dprint(2, (debugfile, "- single_entry_delete -\n"));

    if(warped)
      *warped = 0;

    dl  = dlist(cur_line);
    abe = adrbk_get_ae(abook, (a_c_arg_t)dl->elnum, Normal);

    switch(dl->type){
      case Simple:
	dname =	(abe->fullname && abe->fullname[0])
			? (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						 SIZEOF_20KBUF,
						 abe->fullname, NULL)
			: abe->nickname ? abe->nickname : "";
        cmd   = "Really delete \"%.100s\"";
        break;

      case ListHead:
	dname =	(abe->fullname && abe->fullname[0])
			? (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						  SIZEOF_20KBUF,
						  abe->fullname, NULL)
			: abe->nickname ? abe->nickname : "";
	cmd   = "Really delete ENTIRE list \"%.100s\"";
        break;

      case ListEnt:
        dname = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				       SIZEOF_20KBUF,
				       listmem_from_dl(abook, dl), NULL);
	cmd   = "Really delete \"%.100s\" from list";
        break;
    } 

    dname = dname ? dname : "";
    cmd   = cmd   ? cmd   : "";

    sprintf(prompt, cmd, dname);
    ch = want_to(prompt, 'n', 'n', NO_HELP, WT_NORM);
    if(ch == 'y'){
	dlc_to_flush = get_dlc(cur_line);
	if(dl->type == Simple || dl->type == ListHead){
	    /*--- Kill a single entry or an entire list ---*/
            rc = adrbk_delete(abook, (a_c_arg_t)dl->elnum, 1, 1, 0, 1);
	}
	else if(listmem_count_from_abe(abe) > 2){
            /*---- Kill an entry out of a list ----*/
            rc = adrbk_listdel(abook, (a_c_arg_t)dl->elnum,
		    listmem_from_dl(abook, dl));
	}
	else{
	    char *nick, *full, *addr, *fcc, *comment;
	    adrbk_cntr_t new_entry_num = NO_NEXT;

	    /*---- Convert a List to a Single entry ----*/

	    /* Save old info to be transferred */
	    nick    = cpystr(abe->nickname);
	    full    = cpystr(abe->fullname);
	    fcc     = cpystr(abe->fcc);
	    comment = cpystr(abe->extra);
	    if(listmem_count_from_abe(abe) == 2)
	      addr = cpystr(abe->addr.list[1 - dl->l_offset]);
	    else
	      addr = cpystr("");

            rc = adrbk_delete(abook, (a_c_arg_t)dl->elnum, 0, 1, 0, 0);
	    if(rc == 0)
	      adrbk_add(abook,
			NO_NEXT,
			nick,
			full,
			addr,
			fcc,
			comment,
			Single,
			&new_entry_num,
			(int *)NULL,
			1,
			0,
			1);

	    fs_give((void **)&nick);
	    fs_give((void **)&full);
	    fs_give((void **)&fcc);
	    fs_give((void **)&comment);
	    fs_give((void **)&addr);

	    if(rc == 0){
		DL_CACHE_S dlc_restart;

		dlc_restart.adrbk_num = as.cur;
		dlc_restart.dlcelnum  = new_entry_num;
		dlc_restart.type = DlcSimple;
		warp_to_dlc(&dlc_restart, 0L);
		*warped = 1;
		return 1;
	    }
	}

	if(rc == 0){
	    q_status_message(SM_ORDER, 0, 3,
		"Entry deleted, address book updated");
            dprint(5, (debugfile, "abook: Entry %s\n",
		(dl->type == Simple || dl->type == ListHead) ? "deleted"
							     : "modified"));
	    /*
	     * Remove deleted line and everything after it from
	     * the dlc cache.  Next time we try to access those lines they
	     * will get filled in with the right info.
	     */
	    flush_dlc_from_cache(dlc_to_flush);
            return 1;
        }
	else{
	    PerAddrBook     *pab;

	    if(rc != -5)
              q_status_message1(SM_ORDER | SM_DING, 3, 5,
			      "Error updating address book: %.200s",
		    error_description(errno));
	    pab = &as.adrbks[as.cur];
            dprint(1, (debugfile, "Error deleting entry from %s (%s): %s\n",
		   pab->nickname ? pab->nickname : "?",
		   pab->filename ? pab->filename : "?",
		   error_description(errno)));
        }

	return 0;
    }
    else{
	q_status_message(SM_INFO, 0, 2, "Entry not deleted");
	return 0;
    }
}


#ifdef	ENABLE_LDAP
/*
prompt::name::help::prlen::maxlen::realaddr::
builder::affected_entry::next_affected::selector::key_label::fileedit::
display_it::break_on_comma::is_attach::rich_header::only_file_chars::
single_space::sticky::dirty::start_here::blank::KS_ODATAVAR
*/
static struct headerentry headents_for_query[]={
  {"Normal Search: ",  "NormalSearch",  h_composer_qserv_qq, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Name         : ",  "Name",  h_composer_qserv_cn, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Surname      : ",  "SurName",  h_composer_qserv_sn, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Given Name   : ",  "GivenName",  h_composer_qserv_gn, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Email Address: ",  "EmailAddress",  h_composer_qserv_mail, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Organization : ",  "Organization",  h_composer_qserv_org, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Org Unit     : ",  "OrganizationalUnit", h_composer_qserv_unit, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Country      : ",  "Country",  h_composer_qserv_country, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"State        : ",  "State",  h_composer_qserv_state, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Locality     : ",  "Locality",  h_composer_qserv_locality, 15, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {" ",  "BlankLine",  NO_HELP, 1, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 1, KS_NONE},
  {"Custom Filter: ",  "CustomFilter",  h_composer_qserv_custom, 15, 0, NULL,
   NULL, NULL, NULL, NULL,  NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {NULL, NULL, NO_HELP, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
};
#define QQ_QQ		0
#define QQ_CN		1
#define QQ_SN		2
#define QQ_GN		3
#define QQ_MAIL		4
#define QQ_ORG		5
#define QQ_UNIT		6
#define QQ_COUNTRY	7
#define QQ_STATE	8
#define QQ_LOCALITY	9
#define QQ_BLANK	10
#define QQ_CUSTOM	11
#define QQ_END		12

static SAVED_QUERY_S *saved_params;


/*
 * Formulate a query for LDAP servers and send it and view it.
 * This is called from the Address Book screen, either from ^T from the
 * composer (selecting) or just when browsing in the address book screen
 * (selecting == 0).
 *
 * Args    ps -- Pine struct
 *  selecting -- This is set if we're just selecting an address, as opposed
 *               to browsing on an LDAP server
 *        who -- Tells us which server to query
 *      error -- An error message allocated here and freed by caller.
 *
 * Returns -- Null if not selecting, possibly an address if selecting.
 *            The address is 1522 decoded and should be freed by the caller.
 */
char *
query_server(ps, selecting, exit, who, error)
    struct pine *ps;
    int          selecting;
    int         *exit;
    int          who;
    char       **error;
{
    struct headerentry *he = NULL;
    PICO pbf;
    STORE_S *msgso = NULL, *store = NULL;
    int i, lret, editor_result, mangled = 0;
    int r = 4, flags;
    HelpType help = NO_HELP;
#define FILTSIZE 1000
    char fbuf[FILTSIZE+1];
    char *ret = NULL;
    LDAP_SERV_RES_S *winning_e = NULL;
    LDAP_SERV_RES_S *free_when_done = NULL;
    SAVED_QUERY_S *sq = NULL;
    static ESCKEY_S ekey[] = {
	{ctrl('T'), 10, "^T", "To complex search"},
	{-1, 0, NULL, NULL}
    };

    dprint(2, (debugfile, "- query_server(%s) -\n", selecting?"Selecting":""));

    if(!(ps->VAR_LDAP_SERVERS && ps->VAR_LDAP_SERVERS[0] &&
         ps->VAR_LDAP_SERVERS[0][0])){
	if(error)
	  *error = cpystr("No LDAP server available for lookup");

	return(ret);
    }

    fbuf[0] = '\0';
    flags = OE_APPEND_CURRENT | OE_KEEP_TRAILING_SPACE;
    while(r == 4 || r == 3){
	r = optionally_enter(fbuf, -FOOTER_ROWS(ps), 0, sizeof(fbuf),
			     "String to search for : ",
			     ekey, help, &flags);
	if(r == 3)
	  help = help == NO_HELP ? h_dir_comp_search : NO_HELP;
    }

    /* strip quotes that user typed by mistake */
    (void)removing_double_quotes(fbuf);

    if(r == 1 || r != 10 && fbuf[0] == '\0'){
	ps->mangled_footer = 1;
	if(error)
	  *error = cpystr("Cancelled");

	return(ret);
    }

    editor_result = COMP_EXIT;  /* just to get right logic below */

    memset((void *)&pbf, 0, sizeof(pbf));

    if(r == 10){
	standard_picobuf_setup(&pbf);
	pbf.exittest      = pico_simpleexit;
	pbf.exit_label    = "Search";
	pbf.canceltest    = pico_simplecancel;
	if(saved_params){
	    pbf.expander      = restore_query_parameters;
	    pbf.ctrlr_label   = "Restore";
	}

	pbf.pine_anchor   = set_titlebar("SEARCH DIRECTORY SERVER",
					  ps_global->mail_stream,
					  ps_global->context_current,
					  ps_global->cur_folder,
					  ps_global->msgmap, 
					  0, FolderName, 0, 0, NULL);
	pbf.pine_flags   |= P_NOBODY;

	/* An informational message */
	if(msgso = so_get(PicoText, NULL, EDIT_ACCESS)){
	    pbf.msgtext = (void *)so_text(msgso);
	    /*
	     * It's nice if we can make it so these lines make sense even if
	     * they don't all make it on the screen, because the user can't
	     * scroll down to see them.  So just make each line a whole sentence
	     * that doesn't need the others below it to make sense.
	     */
	    so_puts(msgso,
"\n Fill in some of the fields above to create a query.");
	    so_puts(msgso,
"\n The match will be for the exact string unless you include wildcards (*).");
	    so_puts(msgso,
"\n All filled-in fields must match in order to be counted as a match.");
	    so_puts(msgso,
"\n Press \"^R\" to restore previous query values (if you've queried previously).");
	    so_puts(msgso,
"\n \"^G\" for help specific to each item. \"^X\" to make the query, or \"^C\" to cancel.");
	}

	he = (struct headerentry *)fs_get((QQ_END+1) *
						sizeof(struct headerentry));
	memset((void *)he, 0, (QQ_END+1) * sizeof(struct headerentry));
	for(i = QQ_QQ; i <= QQ_END; i++)
	  he[i] = headents_for_query[i];

	pbf.headents = he;

	sq = copy_query_parameters(NULL);
	he[QQ_QQ].realaddr		= &sq->qq;
	he[QQ_CN].realaddr		= &sq->cn;
	he[QQ_SN].realaddr		= &sq->sn;
	he[QQ_GN].realaddr		= &sq->gn;
	he[QQ_MAIL].realaddr		= &sq->mail;
	he[QQ_ORG].realaddr		= &sq->org;
	he[QQ_UNIT].realaddr		= &sq->unit;
	he[QQ_COUNTRY].realaddr		= &sq->country;
	he[QQ_STATE].realaddr		= &sq->state;
	he[QQ_LOCALITY].realaddr	= &sq->locality;
	he[QQ_CUSTOM].realaddr		= &sq->custom;

	/* pass to pico and let user set them */
	editor_result = pico(&pbf);
	ps->mangled_screen = 1;
	standard_picobuf_teardown(&pbf);

	if(editor_result & COMP_GOTHUP)
	  hup_signal();
	else{
	    fix_windsize(ps_global);
	    init_signals();
	}
    }

    if(editor_result & COMP_EXIT &&
       ((r == 0 && *fbuf) ||
        (r == 10 && sq &&
	 (*sq->qq || *sq->cn || *sq->sn || *sq->gn || *sq->mail ||
	  *sq->org || *sq->unit || *sq->country || *sq->state ||
	  *sq->locality || *sq->custom)))){
	LDAPLookupStyle  style;
	WP_ERR_S         wp_err;
	int              need_and, mangled;
	char            *string;
	CUSTOM_FILT_S   *filter;
	SAVED_QUERY_S   *s;

	s = copy_query_parameters(sq);
	save_query_parameters(s);

	if(r == 0){
	    string = fbuf;
	    filter = NULL;
	}
	else{
	    int categories = 0;

	    categories = ((*sq->cn != '\0') ? 1 : 0) +
			 ((*sq->sn != '\0') ? 1 : 0) +
			 ((*sq->gn != '\0') ? 1 : 0) +
			 ((*sq->mail != '\0') ? 1 : 0) +
			 ((*sq->org != '\0') ? 1 : 0) +
			 ((*sq->unit != '\0') ? 1 : 0) +
			 ((*sq->country != '\0') ? 1 : 0) +
			 ((*sq->state != '\0') ? 1 : 0) +
			 ((*sq->locality != '\0') ? 1 : 0);
	    need_and = (categories > 1);

	    if((sq->cn ? strlen(sq->cn) : 0 +
	        sq->sn ? strlen(sq->sn) : 0 +
	        sq->gn ? strlen(sq->gn) : 0 +
	        sq->mail ? strlen(sq->mail) : 0 +
	        sq->org ? strlen(sq->org) : 0 +
	        sq->unit ? strlen(sq->unit) : 0 +
	        sq->country ? strlen(sq->country) : 0 +
	        sq->state ? strlen(sq->state) : 0 +
	        sq->locality ? strlen(sq->locality) : 0) > FILTSIZE - 100){
		if(error)
		  *error = cpystr("Search strings too long");
		
		goto all_done;
	    }

	    if(categories > 0){

		sprintf(fbuf,
		   "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			need_and ? "(&" : "",
			*sq->cn ? "(cn=" : "",
			*sq->cn ? sq->cn : "",
			*sq->cn ? ")" : "",
			*sq->sn ? "(sn=" : "",
			*sq->sn ? sq->sn : "",
			*sq->sn ? ")" : "",
			*sq->gn ? "(givenname=" : "",
			*sq->gn ? sq->gn : "",
			*sq->gn ? ")" : "",
			*sq->mail ? "(mail=" : "",
			*sq->mail ? sq->mail : "",
			*sq->mail ? ")" : "",
			*sq->org ? "(o=" : "",
			*sq->org ? sq->org : "",
			*sq->org ? ")" : "",
			*sq->unit ? "(ou=" : "",
			*sq->unit ? sq->unit : "",
			*sq->unit ? ")" : "",
			*sq->country ? "(c=" : "",
			*sq->country ? sq->country : "",
			*sq->country ? ")" : "",
			*sq->state ? "(st=" : "",
			*sq->state ? sq->state : "",
			*sq->state ? ")" : "",
			*sq->locality ? "(l=" : "",
			*sq->locality ? sq->locality : "",
			*sq->locality ? ")" : "",
			need_and ? ")" : "");
	    }

	    if(categories > 0 || *sq->custom)
	      filter = (CUSTOM_FILT_S *)fs_get(sizeof(CUSTOM_FILT_S));

	    /* combine the configed filters with this filter */
	    if(*sq->custom){
		string = "";
		filter->filt = sq->custom;
		filter->combine = 0;
	    }
	    else if(*sq->qq && categories > 0){
		string = sq->qq;
		filter->filt = fbuf;
		filter->combine = 1;
	    }
	    else if(categories > 0){
		string = "";
		filter->filt = fbuf;
		filter->combine = 0;
	    }
	    else{
		string = sq->qq;
		filter = NULL;
	    }
	}
	
	mangled = 0;
	memset(&wp_err, 0, sizeof(wp_err));
	wp_err.mangled = &mangled;
	style = selecting ? AlwaysDisplayAndMailRequired : AlwaysDisplay;

	lret = ldap_lookup_all(string, who, 0, style, filter, &winning_e,
			       &wp_err, &free_when_done);
	
	if(filter)
	  fs_give((void **)&filter);

	if(wp_err.mangled)
	  ps->mangled_screen = 1;

	if(wp_err.error){
	    if(status_message_remaining() && error)
	      *error = wp_err.error;
	    else
	      fs_give((void **)&wp_err.error);
	}

	if(lret == 0 && winning_e && selecting){
	    ADDRESS *addr;

	    addr = address_from_ldap(winning_e);
	    if(addr){
		if(!addr->host){
		    addr->host = cpystr("missing-hostname");
		    if(error){
			if(*error)
			  fs_give((void **)error);

			*error = cpystr("Missing hostname in LDAP address");
		    }
		}

		ret = addr_list_string(addr, NULL, 0, 1);
		if(!ret || !ret[0]){
		    if(ret)
		      fs_give((void **)&ret);
		    
		    if(exit)
		      *exit = 1;
		    
		    if(error && !*error){
			char buf[200];

			sprintf(buf, "No email address available for \"%.60s\"",
				(addr->personal && *addr->personal)
				  ? addr->personal
				  : "selected entry");
			*error = cpystr(buf);
		    }
		}

		mail_free_address(&addr);
	    }
	}
	else if(lret == -1 && exit)
	  *exit = 1;
    }
    
all_done:
    if(he)
      free_headents(&he);

    if(free_when_done)
      free_ldap_result_list(free_when_done);

    if(winning_e)
      fs_give((void **)&winning_e);

    if(msgso)
      so_give(&msgso);

    if(sq)
      free_query_parameters(&sq);

    return(ret);
}


static struct key ldap_view_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"<","Results Index",{MC_EXIT,2,{'<',','}},KS_NONE},
	NULL_MENU,
	{"C", "ComposeTo", {MC_COMPOSE,1,{'c'}}, KS_COMPOSER},
	RCOMPOSE_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	SAVE_MENU,

	HELP_MENU,
	OTHER_MENU,
	{"V","ViewLink",{MC_VIEW_HANDLE,3,{'v',ctrl('m'),ctrl('j')}},KS_NONE},
	NULL_MENU,
	{"^B","PrevLink",{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F","NextLink",{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(ldap_view_keymenu, ldap_view_keys);
#define LDV_OTHER 1


/*
 * View all fields of an LDAP entry while browsing.
 *
 * Args    ps -- Pine struct
 *  winning_e -- The struct containing the information about the entry
 *               to be viewed.
 */
void
view_ldap_entry(ps, winning_e)
    struct pine          *ps;
    LDAP_SERV_RES_S      *winning_e;
{
    STORE_S    *srcstore = NULL;
    SourceType  srctype = CharStar;
    SCROLL_S	sargs;
    HANDLE_S *handles = NULL;

    dprint(9, (debugfile, "- view_ldap_entry -\n"));

    if((srcstore=prep_ldap_for_viewing(ps,winning_e,srctype,&handles)) != NULL){
	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text	  = so_text(srcstore);
	sargs.text.src    = srctype;
	sargs.text.desc   = "expanded entry";
	sargs.text.handles= handles;
	sargs.bar.title   = "DIRECTORY ENTRY";
	sargs.proc.tool   = process_ldap_cmd;
	sargs.proc.data.p = (void *) winning_e;
	sargs.help.text   = h_ldap_view;
	sargs.help.title  = "HELP FOR DIRECTORY VIEW";
	sargs.keys.menu   = &ldap_view_keymenu;
	setbitmap(sargs.keys.bitmap);

	if(handles)
	  sargs.keys.menu->how_many = 2;
	else{
	    sargs.keys.menu->how_many = 1;
	    clrbitn(LDV_OTHER, sargs.keys.bitmap);
	}

	scrolltool(&sargs);

	ps->mangled_screen = 1;
	so_give(&srcstore);
	free_handles(&handles);
    }
    else
      q_status_message(SM_ORDER, 0, 2, "Error allocating space");
}


/*
 * Compose a message to the email addresses contained in an LDAP entry.
 *
 * Args    ps -- Pine struct
 *  winning_e -- The struct containing the information about the entry
 *               to be viewed.
 */
void
compose_to_ldap_entry(ps, e, allow_role)
    struct pine          *ps;
    LDAP_SERV_RES_S      *e;
    int                   allow_role;
{
    char  **elecmail = NULL,
	  **mail = NULL,
	  **cn = NULL,
	  **sn = NULL,
	  **givenname = NULL,
	  **ll;
    size_t  len = 0;

    dprint(9, (debugfile, "- compose_to_ldap_entry -\n"));

    if(e){
	char       *a;
	BerElement *ber;

	for(a = ldap_first_attribute(e->ld, e->res, &ber);
	    a != NULL;
	    a = ldap_next_attribute(e->ld, e->res, ber)){

	    if(strcmp(a, e->info_used->mailattr) == 0){
		if(!mail)
		  mail = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, "electronicmail") == 0){
		if(!elecmail)
		  elecmail = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, e->info_used->cnattr) == 0){
		if(!cn)
		  cn = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, e->info_used->gnattr) == 0){
		if(!givenname)
		  givenname = ldap_get_values(e->ld, e->res, a);
	    }
	    else if(strcmp(a, e->info_used->snattr) == 0){
		if(!sn)
		  sn = ldap_get_values(e->ld, e->res, a);
	    }

	    our_ldap_memfree(a);
	}
    }

    if(elecmail){
	if(elecmail[0] && elecmail[0][0] && !mail)
	  mail = elecmail;
	else
	  ldap_value_free(elecmail);

	elecmail = NULL;
    }

    for(ll = mail; ll && *ll; ll++)
      len += strlen(*ll) + 1;
    
    if(len){
	char        *p, *address, *fn = NULL;
	BuildTo      bldto;
	SAVE_STATE_S state;

	address = (char *)fs_get(len * sizeof(char));
	p = address;
	ll = mail;
	while(*ll){
	    sstrcpy(&p, *ll);
	    ll++;
	    if(*ll)
	      sstrcpy(&p, ",");
	}

	/*
	 * If we have a fullname and there is only a single address and
	 * the address doesn't seem to have a fullname with it, add it.
	 */
	if(mail && mail[0] && mail[0][0] && !mail[1]){
	    if(cn && cn[0] && cn[0][0])
	      fn = cpystr(cn[0]);
	    else if(sn && sn[0] && sn[0][0] &&
	            givenname && givenname[0] && givenname[0][0]){
		fn = (char *)fs_get((strlen(givenname[0])+strlen(sn[0])+2)*
								sizeof(char));
		sprintf(fn, "%s %s", givenname[0], sn[0]);
	    }
	}

	if(mail && mail[0] && mail[0][0] && !mail[1] && fn){
	    ADDRESS *adrlist = NULL;
	    char    *tmp_a_string;

	    tmp_a_string = cpystr(address);
	    rfc822_parse_adrlist(&adrlist, tmp_a_string, fakedomain);
	    fs_give((void **)&tmp_a_string);
	    if(adrlist && !adrlist->next && !adrlist->personal){
		char *new_address;

		adrlist->personal = cpystr(fn);
		new_address = (char *)fs_get(est_size(adrlist) * sizeof(char));
		new_address[0] ='\0';
		/* this will quote it if it needs quoting */
		rfc822_write_address(new_address, adrlist);
		fs_give((void **)&address);
		address = new_address;
	    }

	    if(adrlist)
	      mail_free_address(&adrlist);
	}

	bldto.type    = Str;
	bldto.arg.str = address;

	save_state(&state);
	ab_compose_internal(bldto, allow_role);
	restore_state(&state);
	fs_give((void **)&address);
	if(fn)
	  fs_give((void **)&fn);

	/*
	 * Window size may have changed in composer.
         * Pine_send will have reset the window size correctly,
         * but we still have to reset our address book data structures.
	 */
	if(as.initialized)
	  ab_resize();

	ps_global->mangled_screen = 1;
    }
    else
      q_status_message(SM_ORDER, 0, 4, "No address to compose to");

    if(mail)
      ldap_value_free(mail);
    if(cn)
      ldap_value_free(cn);
    if(sn)
      ldap_value_free(sn);
    if(givenname)
      ldap_value_free(givenname);
}


/*
 * Forward the text of an LDAP entry via email (not a vcard attachment)
 *
 * Args    ps -- Pine struct
 *  winning_e -- The struct containing the information about the entry
 *               to be viewed.
 */
void
forward_ldap_entry(ps, winning_e)
    struct pine          *ps;
    LDAP_SERV_RES_S      *winning_e;
{
    STORE_S    *srcstore = NULL;
    SourceType  srctype = CharStar;

    dprint(9, (debugfile, "- forward_ldap_entry -\n"));

    if((srcstore = prep_ldap_for_viewing(ps,winning_e,srctype,NULL)) != NULL){
	forward_text(ps, so_text(srcstore), srctype);
	ps->mangled_screen = 1;
	so_give(&srcstore);
    }
    else
      q_status_message(SM_ORDER, 0, 2, "Error allocating space");
}


STORE_S *
prep_ldap_for_viewing(ps, winning_e, srctype, handlesp)
    struct pine     *ps;
    LDAP_SERV_RES_S *winning_e;
    SourceType       srctype;
    HANDLE_S       **handlesp;
{
    STORE_S    *store = NULL;
    char       *a, *tmp;
    BerElement *ber;
    int         i, width;
#define W (1000)
#define INDENTHERE (22)
    char        obuf[W+10];
    char        hdr[INDENTHERE+1], hdr2[INDENTHERE+1];
    char      **cn = NULL;

    if(!(store = so_get(srctype, NULL, EDIT_ACCESS)))
      return(store);

    /* for mailto handles so user can select individual email addrs */
    if(handlesp)
      init_handles(handlesp);

    width = max(ps->ttyo->screen_cols, 25);

    memset((void *)obuf, '-', ps->ttyo->screen_cols * sizeof(char));
    obuf[ps->ttyo->screen_cols] = '\n';
    obuf[ps->ttyo->screen_cols+1] = '\0';
    a = ldap_get_dn(winning_e->ld, winning_e->res);

    so_puts(store, obuf);
    if(tmp = fold(a, width, width, 0, 0, "", "   ")){
	so_puts(store, tmp);
	fs_give((void **)&tmp);
    }

    so_puts(store, obuf);
    so_puts(store, "\n");

    our_ldap_dn_memfree(a);

    for(a = ldap_first_attribute(winning_e->ld, winning_e->res, &ber);
	a != NULL;
	a = ldap_next_attribute(winning_e->ld, winning_e->res, ber)){

	if(a && *a){
	    char **vals;
	    int    indent = INDENTHERE, wid;
	    char  *fn = NULL;

	    vals = ldap_get_values(winning_e->ld, winning_e->res, a);

	    /* save this for mailto */
	    if(handlesp && !cn && !strcmp(a, winning_e->info_used->cnattr))
	      cn = ldap_get_values(winning_e->ld, winning_e->res, a);

	    if(vals){
		int do_mailto;

		do_mailto = (handlesp &&
			     !strcmp(a, winning_e->info_used->mailattr));

		wid = W - indent;
		sprintf(hdr, "%-*.*s: ", indent-2,indent-2,
			ldap_translate(a, winning_e->info_used));
		sprintf(hdr2, "%-*.*s: ", indent-2,indent-2, "");
		for(i = 0; vals[i] != NULL; i++){
		    if(do_mailto){
			ADDRESS  *ad = NULL;
			HANDLE_S *h;
			char      buf[20];
			char     *tmp_a_string;
			char     *addr, *new_addr, *enc_addr;
			char     *path = NULL;

			addr = cpystr(vals[i]);
			if(cn && cn[0] && cn[0][0])
			  fn = cpystr(cn[0]);

			if(fn){
			    tmp_a_string = cpystr(addr);
			    rfc822_parse_adrlist(&ad, tmp_a_string, "@");
			    fs_give((void **)&tmp_a_string);
			    if(ad && !ad->next && !ad->personal){
				ad->personal = cpystr(fn);
				new_addr = (char *)fs_get(est_size(ad) *
								sizeof(char));
				new_addr[0] = '\0';
				/* this will quote it if it needs quoting */
				rfc822_write_address(new_addr, ad);
				fs_give((void **)&addr);
				addr = new_addr;
			    }

			    if(ad)
			      mail_free_address(&ad);

			    fs_give((void **)&fn);
			}

			if((enc_addr = rfc1738_encode_mailto(addr)) != NULL){
			    path = (char *)fs_get((strlen(enc_addr) + 8) *
								sizeof(char));
			    sprintf(path, "mailto:%s", enc_addr);
			    fs_give((void **)&enc_addr);
			}
			
			fs_give((void **)&addr);

			if(path){
			    h = new_handle(handlesp);
			    h->type = URL;
			    h->h.url.path = path;
			    sprintf(buf, "%d", h->key);

			    sprintf(obuf, "%c%c%c%s%.*s%c%c%c%c",
				    TAG_EMBED, TAG_HANDLE,
				    strlen(buf), buf, wid, vals[i],
				    TAG_EMBED, TAG_BOLDOFF,
				    TAG_EMBED, TAG_INVOFF);
			}
			else
			  sprintf(obuf, "%.*s", wid, vals[i]);
		    }
		    else
		      sprintf(obuf, "%.*s", wid, vals[i]);

		    if(tmp = fold(obuf, width, width, 0, 0,
				  (i==0) ? hdr : hdr2,
				  repeat_char(indent+2, SPACE))){
			so_puts(store, tmp);
			fs_give((void **)&tmp);
		    }
		}
		
		ldap_value_free(vals);
	    }
	    else{
		sprintf(obuf, "%-*.*s\n", indent-1,indent-1,
			ldap_translate(a, winning_e->info_used));
		so_puts(store, obuf);
	    }
	}

	our_ldap_memfree(a);
    }

    if(cn)
      ldap_value_free(cn);

    return(store);
}


int
process_ldap_cmd(cmd, msgmap, sparms)
    int	       cmd;
    MSGNO_S   *msgmap;
    SCROLL_S  *sparms;
{
    int rv = 1;

    ps_global->next_screen = SCREEN_FUN_NULL;

    switch(cmd){
      case MC_SAVE :
	save_ldap_entry(ps_global, sparms->proc.data.p, 0);
	rv = 0;
	break;

      case MC_COMPOSE :
	compose_to_ldap_entry(ps_global, sparms->proc.data.p, 0);
	rv = 0;
	break;

      case MC_ROLE :
	compose_to_ldap_entry(ps_global, sparms->proc.data.p, 1);
	rv = 0;
	break;

      default:
	panic("Unexpected command in process_ldap_cmd");
	break;
    }

    return(rv);
}


struct tl_table {
    char *ldap_ese;
    char *translated;
};

static struct tl_table ldap_trans_table[]={
    {"mail",				"Email Address"},
#define LDAP_MAIL_ATTR 0
    {"sn",				"Surname"},
#define LDAP_SN_ATTR 1
    {"givenName",			"Given Name"},
#define LDAP_GN_ATTR 2
    {"cn",				"Name"},
#define LDAP_CN_ATTR 3
    {"electronicmail",			"Email Address"},
#define LDAP_EMAIL_ATTR 4
    {"o",				"Organization"},
    {"ou",				"Unit"},
    {"c",				"Country"},
    {"st",				"State or Province"},
    {"l",				"Locality"},
    {"objectClass",			"Object Class"},
    {"title",				"Title"},
    {"departmentNumber",		"Department"},
    {"postalAddress",			"Postal Address"},
    {"homePostalAddress",		"Home Address"},
    {"mailStop",			"Mail Stop"},
    {"telephoneNumber",			"Voice Telephone"},
    {"homePhone",			"Home Telephone"},
    {"officePhone",			"Office Telephone"},
    {"facsimileTelephoneNumber",	"FAX Telephone"},
    {"mobile",				"Mobile Telephone"},
    {"pager",				"Pager"},
    {"roomNumber",			"Room Number"},
    {"uid",				"User ID"},
    {NULL,				NULL}
};

char *
ldap_translate(a, info_used)
    char *a;
    LDAP_SERV_S *info_used;
{
    int i;

    if(info_used){
	if(info_used->mailattr && strucmp(info_used->mailattr, a) == 0)
	  return(ldap_trans_table[LDAP_MAIL_ATTR].translated);
	else if(info_used->snattr && strucmp(info_used->snattr, a) == 0)
	  return(ldap_trans_table[LDAP_SN_ATTR].translated);
	else if(info_used->gnattr && strucmp(info_used->gnattr, a) == 0)
	  return(ldap_trans_table[LDAP_GN_ATTR].translated);
	else if(info_used->cnattr && strucmp(info_used->cnattr, a) == 0)
	  return(ldap_trans_table[LDAP_CN_ATTR].translated);
    }

    for(i = 0; ldap_trans_table[i].ldap_ese; i++){
	if(info_used)
	  switch(i){
	    case LDAP_MAIL_ATTR:
	    case LDAP_SN_ATTR:
	    case LDAP_GN_ATTR:
	    case LDAP_CN_ATTR:
	    case LDAP_EMAIL_ATTR:
	      continue;
	  }

	if(strucmp(ldap_trans_table[i].ldap_ese, a) == 0)
	  return(ldap_trans_table[i].translated);
    }
    
    return(a);
}


char *
pico_simpleexit(he, redraw_pico, allow_flowed)
    struct headerentry *he;
    void (*redraw_pico)();
    int allow_flowed;
{
    return(NULL);
}

char *
pico_simplecancel(he, redraw_pico)
    struct headerentry *he;
    void (*redraw_pico)();
{
    return("Cancelled");
}


/*
 * Store query parameters so that they can be recalled by user later with ^R.
 */
void
save_query_parameters(params)
    SAVED_QUERY_S *params;
{
    free_saved_query_parameters();
    saved_params = params;
}


SAVED_QUERY_S *
copy_query_parameters(params)
    SAVED_QUERY_S *params;
{
    SAVED_QUERY_S *sq;

    sq = (SAVED_QUERY_S *)fs_get(sizeof(SAVED_QUERY_S));
    memset((void *)sq, 0, sizeof(SAVED_QUERY_S));

    if(params && params->qq)
      sq->qq = cpystr(params->qq);
    else
      sq->qq = cpystr("");

    if(params && params->cn)
      sq->cn = cpystr(params->cn);
    else
      sq->cn = cpystr("");

    if(params && params->sn)
      sq->sn = cpystr(params->sn);
    else
      sq->sn = cpystr("");

    if(params && params->gn)
      sq->gn = cpystr(params->gn);
    else
      sq->gn = cpystr("");

    if(params && params->mail)
      sq->mail = cpystr(params->mail);
    else
      sq->mail = cpystr("");

    if(params && params->org)
      sq->org = cpystr(params->org);
    else
      sq->org = cpystr("");

    if(params && params->unit)
      sq->unit = cpystr(params->unit);
    else
      sq->unit = cpystr("");

    if(params && params->country)
      sq->country = cpystr(params->country);
    else
      sq->country = cpystr("");

    if(params && params->state)
      sq->state = cpystr(params->state);
    else
      sq->state = cpystr("");

    if(params && params->locality)
      sq->locality = cpystr(params->locality);
    else
      sq->locality = cpystr("");
    
    if(params && params->custom)
      sq->custom = cpystr(params->custom);
    else
      sq->custom = cpystr("");
    
    return(sq);
}


void
free_saved_query_parameters()
{
    if(saved_params)
      free_query_parameters(&saved_params);
}


void
free_query_parameters(parm)
    SAVED_QUERY_S **parm;
{
    if(parm){
	if(*parm){
	    if((*parm)->qq)
	      fs_give((void **)&(*parm)->qq);
	    if((*parm)->cn)
	      fs_give((void **)&(*parm)->cn);
	    if((*parm)->sn)
	      fs_give((void **)&(*parm)->sn);
	    if((*parm)->gn)
	      fs_give((void **)&(*parm)->gn);
	    if((*parm)->mail)
	      fs_give((void **)&(*parm)->mail);
	    if((*parm)->org)
	      fs_give((void **)&(*parm)->org);
	    if((*parm)->unit)
	      fs_give((void **)&(*parm)->unit);
	    if((*parm)->country)
	      fs_give((void **)&(*parm)->country);
	    if((*parm)->state)
	      fs_give((void **)&(*parm)->state);
	    if((*parm)->locality)
	      fs_give((void **)&(*parm)->locality);
	    if((*parm)->custom)
	      fs_give((void **)&(*parm)->custom);
	    
	    fs_give((void **)parm);
	}
    }
}


/*
 * A callback from pico to restore the saved query parameters.
 *
 * Args  he -- Unused.
 *        s -- The place to return the allocated array of values.
 *
 * Returns -- 1 if there are parameters to return, 0 otherwise.
 */
int
restore_query_parameters(he, s)
    struct headerentry *he;
    char             ***s;
{
    int retval = 0, i = 0;

    if(s)
      *s = NULL;

    if(saved_params && s){
	*s = (char **)fs_get((QQ_END + 1) * sizeof(char *));
	(*s)[i++] = cpystr(saved_params->qq ? saved_params->qq : "");
	(*s)[i++] = cpystr(saved_params->cn ? saved_params->cn : "");
	(*s)[i++] = cpystr(saved_params->sn ? saved_params->sn : "");
	(*s)[i++] = cpystr(saved_params->gn ? saved_params->gn : "");
	(*s)[i++] = cpystr(saved_params->mail ? saved_params->mail : "");
	(*s)[i++] = cpystr(saved_params->org ? saved_params->org : "");
	(*s)[i++] = cpystr(saved_params->unit ? saved_params->unit : "");
	(*s)[i++] = cpystr(saved_params->country ? saved_params->country : "");
	(*s)[i++] = cpystr(saved_params->state ? saved_params->state : "");
	(*s)[i++] = cpystr(saved_params->locality ? saved_params->locality :"");
	(*s)[i++] = cpystr(saved_params->custom ? saved_params->custom :"");
	(*s)[i]   = 0;
	retval = 1;
    }

    return(retval);
}


/*
 * Internal handler for viewing an LDAP url.
 */
int
url_local_ldap(url)
    char *url;
{
    LDAP            *ld;
    struct timeval   t;
    int              ld_err, mangled = 0, we_cancel, retval = 0, proto = 3;
    char             ebuf[300];
    LDAPMessage     *result;
    LDAP_SERV_S     *info;
    LDAP_SERV_RES_S *serv_res = NULL;
    LDAPURLDesc     *ldapurl = NULL;
    WP_ERR_S wp_err;

    dprint(2, (debugfile, "url_local_ldap(%s)\n", url ? url : "?"));

    ld_err = ldap_url_parse(url, &ldapurl);
    if(ld_err || !ldapurl){
      sprintf(ebuf, "URL parse failed for %.200s", url);
      q_status_message(SM_ORDER, 3, 5, ebuf);
      return(retval);
    }

    if(!ldapurl->lud_host){
      sprintf(ebuf, "No host in %.200s", url);
      q_status_message(SM_ORDER, 3, 5, ebuf);
      ldap_free_urldesc(ldapurl);
      return(retval);
    }
    
    intr_handling_on();
    we_cancel = busy_alarm(1, "Searching for LDAP url", NULL, 1);
    ps_global->mangled_footer = 1;

#if (LDAPAPI >= 11)
    if((ld = ldap_init(ldapurl->lud_host, ldapurl->lud_port)) == NULL)
#else
    if((ld = ldap_open(ldapurl->lud_host, ldapurl->lud_port)) == NULL)
#endif
    {
	if(we_cancel){
	    cancel_busy_alarm(-1);
	    we_cancel = 0;
	}

	q_status_message(SM_ORDER,3,5, "LDAP search failed: can't initialize");
    }
    else if(!ps_global->intr_pending){
      if(ldap_v3_is_supported(ld, NULL) &&
	 our_ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &proto) == 0){
	dprint(5,(debugfile, "ldap: using version 3 protocol\n"));
      }

      /*
       * If we don't set RESTART then the select() waiting for the answer
       * in libldap will be interrupted and stopped by our busy_alarm.
       */
      our_ldap_set_option(ld, LDAP_OPT_RESTART, LDAP_OPT_ON);

      t.tv_sec = 30; t.tv_usec = 0;
      ld_err = ldap_search_st(ld, ldapurl->lud_dn, ldapurl->lud_scope,
			      ldapurl->lud_filter, ldapurl->lud_attrs,
			      0, &t, &result);
      if(ld_err != LDAP_SUCCESS){
	  if(we_cancel){
	      cancel_busy_alarm(-1);
	      we_cancel = 0;
	  }

	  sprintf(ebuf, "LDAP search failed: %.200s", ldap_err2string(ld_err));
	  q_status_message(SM_ORDER, 3, 5, ebuf);
	  ldap_unbind(ld);
      }
      else if(!ps_global->intr_pending){
	if(we_cancel){
	    cancel_busy_alarm(-1);
	    we_cancel = 0;
	}

	intr_handling_off();
	if(ldap_count_entries(ld, result) == 0){
	  q_status_message(SM_ORDER, 3, 5, "No matches found for url");
	  ldap_unbind(ld);
	  if(result)
	    ldap_msgfree(result);
	}
        else{
	  serv_res = (LDAP_SERV_RES_S *)fs_get(sizeof(LDAP_SERV_RES_S));
	  memset((void *)serv_res, 0, sizeof(*serv_res));
	  serv_res->ld  = ld;
	  serv_res->res = result;
	  info = (LDAP_SERV_S *)fs_get(sizeof(LDAP_SERV_S));
	  memset((void *)info, 0, sizeof(*info));
	  info->mailattr = cpystr(DEF_LDAP_MAILATTR);
	  info->snattr   = cpystr(DEF_LDAP_SNATTR);
	  info->gnattr   = cpystr(DEF_LDAP_GNATTR);
	  info->cnattr   = cpystr(DEF_LDAP_CNATTR);
	  serv_res->info_used = info;
	  memset(&wp_err, 0, sizeof(wp_err));
	  wp_err.mangled = &mangled;

	  ask_user_which_entry(serv_res, NULL, &wp_err, NULL, DisplayForURL);
	  if(wp_err.error){
	    q_status_message(SM_ORDER, 3, 5, wp_err.error);
	    fs_give((void **)&wp_err.error);
	  }

	  if(mangled)
	    ps_global->mangled_screen = 1;

	  free_ldap_result_list(serv_res);
	  retval = 1;
        }
      }
    }

    if(we_cancel)
      cancel_busy_alarm(-1);

    intr_handling_off();

    if(ldapurl)
      ldap_free_urldesc(ldapurl);

    return(retval);
}
#endif	/* ENABLE_LDAP */
