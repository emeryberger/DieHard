#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: bldaddr.c 14092 2005-09-27 21:27:55Z hubert@u.washington.edu $";
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
    buildaddr.c
    Support for build_address function and low-level display cache.
 ====*/


#include "headers.h"
#include "adrbklib.h"

/*
 * Jump back to this location if we discover that one of the open addrbooks
 * has been changed by some other process.
 *
 * The trouble_filename variable is usually NULL (no trouble) but may be
 * set if adrbklib detected trouble in an addrbook.lu file.  In that case,
 * trouble_filename will be set to the name of the addressbook
 * with the problem.  It is used to force a rebuild of the .lu file.
 */
jmp_buf addrbook_changed_unexpectedly;
char   *trouble_filename;


void           add_forced_entries PROTO((AdrBk *));
AdrBk_Entry   *address_to_abe PROTO((ADDRESS *));
AdrBk_Entry   *adrbk_lookup_with_opens_by_nick PROTO((char *, int, int *, int));
DL_CACHE_S    *dlc_mgr PROTO((long, DlMgrOps, DL_CACHE_S *));
DL_CACHE_S    *dlc_next PROTO((DL_CACHE_S *, DL_CACHE_S *));
DL_CACHE_S    *dlc_prev PROTO((DL_CACHE_S *, DL_CACHE_S *));
int            dlc_siblings PROTO((DL_CACHE_S *, DL_CACHE_S *));
void           done_with_dlc_cache PROTO((void));
ADDRESS       *expand_address PROTO((BuildTo, char *, char *, int *, char **,
				     int *, char **, char **, int, int, int *));
void           free_cache_array PROTO((DL_CACHE_S **, int));
DL_CACHE_S    *get_bottom_dl_of_adrbk PROTO((int, DL_CACHE_S *));
DL_CACHE_S    *get_top_dl_of_adrbk PROTO((int, DL_CACHE_S *));
DL_CACHE_S    *get_global_bottom_dlc PROTO((DL_CACHE_S *));
DL_CACHE_S    *get_global_top_dlc PROTO((DL_CACHE_S *));
void           initialize_dlc_cache PROTO((void));
ADDRESS       *massage_phrase_addr PROTO ((char *, char *, char *));
char          *skip_to_next_addr PROTO((char *));
void           strip_personal_quotes PROTO((ADDRESS *));
int	       addr_is_in_addrbook PROTO((PerAddrBook *, ADDRESS *, ADDRESS *));
#ifdef	ENABLE_LDAP
int              ask_user_which_entry PROTO((LDAP_SERV_RES_S *, char *,
					     LDAP_SERV_RES_S **,
					     WP_ERR_S *, LDAPLookupStyle));
LDAP_SERV_S     *copy_ldap_serv_info PROTO((LDAP_SERV_S *));
LDAP_SERV_RES_S *ldap_lookup PROTO((LDAP_SERV_S *, char *, CUSTOM_FILT_S *,
				    WP_ERR_S *, int));
int              our_ldap_get_lderrno PROTO((LDAP *, char **, char **));
int              our_ldap_set_lderrno PROTO((LDAP *, int, char *, char *));
#endif	/* ENABLE_LDAP */

#define NO_PERMISSION	"[ Permission Denied ]"
#define READONLY	" (ReadOnly)"
#define NOACCESS	" (Un-readable)"
#define OLDSTYLE_DIR_TITLE	"Directories"


/*
 * This returns the actual dlc instead of the dl within the dlc.
 */
DL_CACHE_S *
get_dlc(row)
    long row;
{
    dprint(11, (debugfile, "- get_dlc(%ld) -\n", row));

    return(dlc_mgr(row, Lookup, (DL_CACHE_S *)NULL));
}


void
initialize_dlc_cache()
{
    dprint(11, (debugfile, "- initialize_dlc_cache -\n"));

    (void)dlc_mgr(NO_LINE, Initialize, (DL_CACHE_S *)NULL);
}


void
done_with_dlc_cache()
{
    dprint(11, (debugfile, "- done_with_dlc_cache -\n"));

    (void)dlc_mgr(NO_LINE, DoneWithCache, (DL_CACHE_S *)NULL);
}


/*
 * Move to new_dlc and give it row number row_number_to_assign_it.
 * We copy the passed in dlc in case the caller passed us a pointer into
 * the cache.
 */
void
warp_to_dlc(new_dlc, row_number_to_assign_it)
    DL_CACHE_S *new_dlc;
    long row_number_to_assign_it;
{
    DL_CACHE_S dlc;

    dprint(9, (debugfile, "- warp_to_dlc(%ld) -\n", row_number_to_assign_it));

    dlc = *new_dlc;

    /*
     * Make sure we can move forward and backward from these
     * types that we may wish to warp to. The caller may not have known
     * to set adrbk_num for these types.
     */
    switch(dlc.type){
      case DlcPersAdd:
	dlc.adrbk_num = 0;
	break;
      case DlcGlobAdd:
	dlc.adrbk_num = as.n_addrbk;
	break;
    }

    (void)dlc_mgr(row_number_to_assign_it, ArbitraryStartingPoint, &dlc);
}


/*
 * Move to first dlc and give it row number 0.
 */
void
warp_to_beginning()
{
    dprint(9, (debugfile, "- warp_to_beginning -\n"));

    (void)dlc_mgr(0L, FirstEntry, (DL_CACHE_S *)NULL);
}


/*
 * Move to first dlc in abook_num and give it row number 0.
 */
void
warp_to_top_of_abook(abook_num)
    int abook_num;
{
    DL_CACHE_S dlc;

    dprint(9, (debugfile, "- warp_to_top_of_abook(%d) -\n", abook_num));

    (void)get_top_dl_of_adrbk(abook_num, &dlc);
    warp_to_dlc(&dlc, 0L);
}


/*
 * Move to last dlc and give it row number 0.
 */
void
warp_to_end()
{
    dprint(9, (debugfile, "- warp_to_end -\n"));

    (void)dlc_mgr(0L, LastEntry, (DL_CACHE_S *)NULL);
}


/*
 * This flushes all of the cache that is related to this dlc.
 */
void
flush_dlc_from_cache(dlc_to_flush)
    DL_CACHE_S *dlc_to_flush;
{
    dprint(11, (debugfile, "- flush_dlc_from_cache -\n"));

    (void)dlc_mgr(NO_LINE, FlushDlcFromCache, dlc_to_flush);
}


/*
 * Returns 1 if the dlc's match, 0 otherwise.
 */
int
matching_dlcs(dlc1, dlc2)
    DL_CACHE_S *dlc1, *dlc2;
{
    if(!dlc1 || !dlc2 ||
        dlc1->type != dlc2->type ||
	dlc1->adrbk_num != dlc2->adrbk_num)
	return 0;

    switch(dlc1->type){

      case DlcSimple:
      case DlcListHead:
      case DlcListBlankTop:
      case DlcListBlankBottom:
      case DlcListClickHere:
      case DlcListEmpty:
	return(dlc1->dlcelnum == dlc2->dlcelnum);

      case DlcListEnt:
	return(dlc1->dlcelnum  == dlc2->dlcelnum &&
	       dlc1->dlcoffset == dlc2->dlcoffset);

      case DlcTitle:
      case DlcSubTitle:
      case DlcTitleNoPerm:
      case DlcTitleBlankTop:
      case DlcEmpty:
      case DlcZoomEmpty:
      case DlcNoPermission:
      case DlcPersAdd:
      case DlcGlobAdd:
      case DlcGlobDelim1:
      case DlcGlobDelim2:
      case DlcDirAccess:
      case DlcDirDelim1:
      case DlcDirDelim2:
      case DlcTitleDashTopCmb:
      case DlcTitleCmb:
      case DlcTitleDashBottomCmb:
      case DlcTitleBlankBottomCmb:
      case DlcClickHereCmb:
      case DlcTitleBlankTopCmb:
	return 1;

      case DlcNotSet:
      case DlcBeginning:
      case DlcOneBeforeBeginning:
      case DlcTwoBeforeBeginning:
      case DlcEnd:
      case DlcNoAbooks:
	return 0;
    }
    /*NOTREACHED*/

    return 0;
}


/*
 * Returns 1 if the dlc's are related to each other, 0 otherwise.
 *
 * The idea is that if you are going to flush one of these dlcs from the
 * cache you should also flush its partners. For example, if you flush one
 * Listent from a list you should flush the entire entry including all the
 * Listents and the ListHead. If you flush a DlcTitle, you should also
 * flush the SubTitle and the TitleBlankTop.
 */
int
dlc_siblings(dlc1, dlc2)
    DL_CACHE_S *dlc1, *dlc2;
{
    if(!dlc1 || !dlc2 || dlc1->adrbk_num != dlc2->adrbk_num)
      return 0;

    switch(dlc1->type){

      case DlcSimple:
      case DlcListHead:
      case DlcListBlankTop:
      case DlcListBlankBottom:
      case DlcListEnt:
      case DlcListClickHere:
      case DlcListEmpty:
	switch(dlc2->type){
	  case DlcSimple:
	  case DlcListHead:
	  case DlcListBlankTop:
	  case DlcListBlankBottom:
	  case DlcListEnt:
	  case DlcListClickHere:
	  case DlcListEmpty:
	    return(dlc1->dlcelnum == dlc2->dlcelnum);

	  default:
	    return 0;
	}

	break;

      case DlcDirDelim1:
      case DlcDirDelim2:
	switch(dlc2->type){
	  case DlcDirDelim1:
	  case DlcDirDelim2:
	    return 1;

	  default:
	    return 0;
	}
	break;

      case DlcGlobDelim1:
      case DlcGlobDelim2:
	switch(dlc2->type){
	  case DlcGlobDelim1:
	  case DlcGlobDelim2:
	    return 1;

	  default:
	    return 0;
	}
	break;

      case DlcTitle:
      case DlcTitleNoPerm:
      case DlcSubTitle:
      case DlcTitleBlankTop:
	switch(dlc2->type){
	  case DlcTitle:
	  case DlcTitleNoPerm:
	  case DlcSubTitle:
	  case DlcTitleBlankTop:
	    return 1;

	  default:
	    return 0;
	}
	break;

      case DlcDirAccess:
      case DlcDirSubTitle:
      case DlcDirBlankTop:
	switch(dlc2->type){
	  case DlcDirAccess:
	  case DlcDirSubTitle:
	  case DlcDirBlankTop:
	    return 1;

	  default:
	    return 0;
	}
	break;

      case DlcTitleDashTopCmb:
      case DlcTitleCmb:
      case DlcTitleDashBottomCmb:
      case DlcTitleBlankBottomCmb:
      case DlcClickHereCmb:
      case DlcTitleBlankTopCmb:
	switch(dlc2->type){
	  case DlcTitleDashTopCmb:
	  case DlcTitleCmb:
	  case DlcTitleDashBottomCmb:
	  case DlcTitleBlankBottomCmb:
	  case DlcClickHereCmb:
	  case DlcTitleBlankTopCmb:
	    return 1;

	  default:
	    return 0;
	}
	break;

      default:
	return 0;
    }
    /*NOTREACHED*/
}


/* data for the display list cache */
static DL_CACHE_S *cache_array = (DL_CACHE_S *)NULL;
static long        valid_low,
		   valid_high;
static int         index_of_low,
		   size_of_cache,
		   n_cached;

/*
 * Manage the display list cache.
 *
 * The cache is a circular array of DL_CACHE_S elements.  It always
 * contains a contiguous set of display lines.
 * The lowest numbered line in the cache is
 * valid_low, and the highest is valid_high.  Everything in between is
 * also valid.  Index_of_low is where to look
 * for the valid_low element in the circular array.
 *
 * We make calls to dlc_prev and dlc_next to get new entries for the cache.
 * We need a starting value before we can do that.
 *
 * This returns a pointer to a dlc for the desired row.  If you want the
 * actual display list line you call dlist(row) instead of dlc_mgr.
 */
DL_CACHE_S *
dlc_mgr(row, op, dlc_start)
    long row;
    DlMgrOps op;
    DL_CACHE_S *dlc_start;
{
    int                new_index, known_index, next_index;
    DL_CACHE_S        *dlc = (DL_CACHE_S *)NULL;
    long               next_row;
    long               prev_row;


    if(op == Lookup){

	if(row >= valid_low && row <= valid_high){  /* already cached */

	    new_index = ((row - valid_low) + index_of_low) % size_of_cache;
	    dlc = &cache_array[new_index];

	}
	else if(row > valid_high){  /* row is past where we've looked */

	    known_index =
	      ((valid_high - valid_low) + index_of_low) % size_of_cache;
	    next_row    = valid_high + 1L;

	    /* we'll usually be looking for row = valid_high + 1 */
	    while(next_row <= row){

		new_index = (known_index + 1) % size_of_cache;

		dlc =
		  dlc_next(&cache_array[known_index], &cache_array[new_index]);

		/*
		 * This means somebody changed the file out from underneath
		 * us.  This would happen if dlc_next needed to ask for an
		 * abe to figure out what the type of the next row is, but
		 * adrbk_get_ae returned a NULL.  I don't think that can
		 * happen, but if it does...
		 */
		if(dlc->type == DlcNotSet){
		    dprint(1, (debugfile, "dlc_next returned DlcNotSet\n"));
		    goto panic_abook_corrupt;
		}

		if(n_cached == size_of_cache){ /* replaced low cache entry */
		    valid_low++;
		    index_of_low = (index_of_low + 1) % size_of_cache;
		}
		else
		  n_cached++;

		valid_high++;

		next_row++;
		known_index = new_index; /* for next time through loop */
	    }
	}
	else if(row < valid_low){  /* row is back up the screen */

	    known_index = index_of_low;
	    prev_row = valid_low - 1L;

	    while(prev_row >= row){

		new_index = (known_index - 1 + size_of_cache) % size_of_cache;
		dlc =
		  dlc_prev(&cache_array[known_index], &cache_array[new_index]);

		if(dlc->type == DlcNotSet){
		    dprint(1, (debugfile, "dlc_prev returned DlcNotSet (1)\n"));
		    goto panic_abook_corrupt;
		}

		if(n_cached == size_of_cache) /* replaced high cache entry */
		  valid_high--;
		else
		  n_cached++;

		valid_low--;
		index_of_low =
			    (index_of_low - 1 + size_of_cache) % size_of_cache;

		prev_row--;
		known_index = new_index;
	    }
	}
    }
    else if(op == Initialize){

	n_cached = 0;

	if(!cache_array || size_of_cache != 3 * max(as.l_p_page,1)){
	    if(cache_array)
	      free_cache_array(&cache_array, size_of_cache);

	    size_of_cache = 3 * max(as.l_p_page,1);
	    cache_array =
		(DL_CACHE_S *)fs_get(size_of_cache * sizeof(DL_CACHE_S));
	    memset((void *)cache_array, 0, size_of_cache * sizeof(DL_CACHE_S));
	}

	/* this will return NULL below and the caller should ignore that */
    }
    /*
     * Flush all rows for a particular addrbook entry from the cache, but
     * keep the cache alive and anchored in the same place.  The particular
     * entry is the one that dlc_start is one of the rows of.
     */
    else if(op == FlushDlcFromCache){
	long low_entry;

	next_row = dlc_start->global_row - 1;
	for(; next_row >= valid_low; next_row--){
	    next_index = ((next_row - valid_low) + index_of_low) %
		size_of_cache;
	    if(!dlc_siblings(dlc_start, &cache_array[next_index]))
		break;
	}

	low_entry = next_row + 1L;

	/*
	 * If low_entry now points one past a ListBlankBottom, delete that,
	 * too, since it may not make sense anymore.
	 */
	if(low_entry > valid_low){
	    next_index = ((low_entry -1L - valid_low) + index_of_low) %
		size_of_cache;
	    if(cache_array[next_index].type == DlcListBlankBottom)
		low_entry--;
	}

	if(low_entry > valid_low){ /* invalidate everything >= this */
	    n_cached -= (valid_high - (low_entry - 1L));
	    valid_high = low_entry - 1L;
	}
	else{
	    /*
	     * This is the tough case.  That entry was the first thing cached,
	     * so we need to invalidate the whole cache.  However, we also
	     * need to keep at least one thing cached for an anchor, so
	     * we need to get the dlc before this one and it should be a
	     * dlc not related to this same addrbook entry.
	     */
	    known_index = index_of_low;
	    prev_row = valid_low - 1L;

	    for(;;){

		new_index = (known_index - 1 + size_of_cache) % size_of_cache;
		dlc =
		  dlc_prev(&cache_array[known_index], &cache_array[new_index]);

		if(dlc->type == DlcNotSet){
		    dprint(1, (debugfile, "dlc_prev returned DlcNotSet (2)\n"));
		    goto panic_abook_corrupt;
		}

		valid_low--;
		index_of_low =
			    (index_of_low - 1 + size_of_cache) % size_of_cache;

		if(!dlc_siblings(dlc_start, dlc))
		  break;

		known_index = new_index;
	    }

	    n_cached = 1;
	    valid_high = valid_low;
	}
    }
    /*
     * We have to anchor ourselves at a first element.
     * Here's how we start at the top.
     */
    else if(op == FirstEntry){
	initialize_dlc_cache();
	n_cached++;
	dlc = &cache_array[0];
	dlc = get_global_top_dlc(dlc);
	dlc->global_row = row;
	index_of_low = 0;
	valid_low    = row;
	valid_high   = row;
    }
    /* And here's how we start from the bottom. */
    else if(op == LastEntry){
	initialize_dlc_cache();
	n_cached++;
	dlc = &cache_array[0];
	dlc = get_global_bottom_dlc(dlc);
	dlc->global_row = row;
	index_of_low = 0;
	valid_low    = row;
	valid_high   = row;
    }
    /*
     * And here's how we start from an arbitrary position in the middle.
     * We root the cache at display line row, so it helps if row is close
     * to where we're going to be starting so that things are easy to find.
     * The dl that goes with line row is dl_start from addrbook number
     * adrbk_num_start.
     */
    else if(op == ArbitraryStartingPoint){
	AddrScrn_Disp      dl;

	initialize_dlc_cache();
	n_cached++;
	dlc = &cache_array[0];
	/*
	 * Save this in case fill_in_dl_field needs to free the text
	 * it points to.
	 */
	dl = dlc->dl;
	*dlc = *dlc_start;
	dlc->dl = dl;
	dlc->global_row = row;

	index_of_low = 0;
	valid_low    = row;
	valid_high   = row;
    }
    else if(op == DoneWithCache){

	n_cached = 0;
	if(cache_array)
	  free_cache_array(&cache_array, size_of_cache);
    }

    return(dlc);

panic_abook_corrupt:
    q_status_message(SM_ORDER | SM_DING, 5, 10,
	"Addrbook changed unexpectedly, re-syncing...");
    dprint(1, (debugfile,
	"addrbook changed while we had it open?, re-sync\n"));
    dprint(2, (debugfile,
	"valid_low=%ld valid_high=%ld index_of_low=%d size_of_cache=%d\n",
	valid_low, valid_high, index_of_low, size_of_cache));
    dprint(2, (debugfile,
	"n_cached=%d new_index=%d known_index=%d next_index=%d\n",
	n_cached, new_index, known_index, next_index));
    dprint(2, (debugfile,
	"next_row=%ld prev_row=%ld row=%ld\n", next_row, prev_row, row));
    /* jump back to a safe starting point */
    longjmp(addrbook_changed_unexpectedly, 1);
    /*NOTREACHED*/
}


void
free_cache_array(c_array, size)
    DL_CACHE_S **c_array;
    int size;
{
    DL_CACHE_S *dlc;
    int i;

    for(i = 0; i < size; i++){
	dlc = &(*c_array)[i];
	/* free any allocated space */
	switch(dlc->dl.type){
	  case Text:
	  case Title:
	  case TitleCmb:
	  case AskServer:
	    if(dlc->dl.txt)
	      fs_give((void **)&dlc->dl.txt);

	    break;
	}
    }

    fs_give((void **)c_array);
}


/*
 * Get the dlc element that comes before "old".  The function that calls this
 * function is the one that keeps a cache and checks in the cache before
 * calling here.  New is a passed in pointer to a buffer where we fill in
 * the answer.
 */
DL_CACHE_S *
dlc_prev(old, new)
    DL_CACHE_S *old, *new;
{
    PerAddrBook  *pab;
    AdrBk_Entry  *abe;
    adrbk_cntr_t  list_count;

    new->adrbk_num  = -2;
    new->dlcelnum   = NO_NEXT;
    new->dlcoffset  = NO_NEXT;
    new->type       = DlcNotSet;
    pab = &as.adrbks[old->adrbk_num];

    switch(old->type){
      case DlcTitle:
      case DlcTitleNoPerm:
	if(old->adrbk_num == 0 && as.config && as.how_many_personals == 0)
	  new->type = DlcGlobDelim2;
	else if(old->adrbk_num == 0)
	  new->type = DlcOneBeforeBeginning;
	else if(old->adrbk_num == as.how_many_personals)
	  new->type = DlcGlobDelim2;
	else
	  new->type = DlcTitleBlankTop;

	break;

      case DlcSubTitle:
	if(pab->access == NoAccess)
	  new->type = DlcTitleNoPerm;
	else
	  new->type = DlcTitle;

	break;

      case DlcTitleBlankTop:
	new->adrbk_num = old->adrbk_num - 1;
	new->type = DlcSubTitle;
	break;

      case DlcEmpty:
      case DlcZoomEmpty:
      case DlcNoPermission:
      case DlcNoAbooks:
	if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(as.n_addrbk == 1 && as.n_serv == 0)
	      new->type = DlcOneBeforeBeginning;
	    else
	      new->type = DlcTitleBlankBottomCmb;
	}
	else
	  new->type = DlcOneBeforeBeginning;

	break;

      case DlcSimple:
	{
	adrbk_cntr_t el;
	long i;

	i = old->dlcelnum;
	i--;
	el = old->dlcelnum - 1;
	while(i >= 0L){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el--;
	    i--;
	}

	if(i >= 0){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book,
			       (a_c_arg_t)new->dlcelnum,
			       Normal);
	    if(abe && abe->tag == Single)
	      new->type = DlcSimple;
	    else if(abe && abe->tag == List)
	      new->type = DlcListBlankBottom;
	}
	else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(as.n_addrbk == 1 && as.n_serv == 0)
	      new->type = DlcOneBeforeBeginning;
	    else
	      new->type = DlcTitleBlankBottomCmb;
	}
	else
	  new->type = DlcOneBeforeBeginning;
	}

	break;

      case DlcListHead:
	{
	adrbk_cntr_t el;
	long i;

	i = old->dlcelnum;
	i--;
	el = old->dlcelnum - 1;
	while(i >= 0L){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el--;
	    i--;
	}

	if(i >= 0){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book,
			       (a_c_arg_t)new->dlcelnum,
			       Normal);
	    if(abe && abe->tag == Single){
		new->type  = DlcListBlankTop;
		new->dlcelnum = old->dlcelnum;
	    }
	    else if(abe && abe->tag == List)
	      new->type  = DlcListBlankBottom;
	}
	else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(as.n_addrbk == 1 && as.n_serv == 0)
	      new->type = DlcOneBeforeBeginning;
	    else
	      new->type = DlcTitleBlankBottomCmb;
	}
	else
	  new->type = DlcOneBeforeBeginning;
	}

	break;

      case DlcListEnt:
	if(old->dlcoffset > 0){
	    new->type      = DlcListEnt;
	    new->dlcelnum  = old->dlcelnum;
	    new->dlcoffset = old->dlcoffset - 1;
	}
	else{
	    new->type     = DlcListHead;
	    new->dlcelnum = old->dlcelnum;
	}

	break;

      case DlcListClickHere:
      case DlcListEmpty:
	new->type     = DlcListHead;
	new->dlcelnum = old->dlcelnum;
	break;

      case DlcListBlankTop:  /* can only occur between a Simple and a List */
	new->type   = DlcSimple;
	{
	adrbk_cntr_t el;
	long i;

	i = old->dlcelnum;
	i--;
	el = old->dlcelnum - 1;
	while(i >= 0L){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el--;
	    i--;
	}

	if(i >= 0)
	  new->dlcelnum = el;
	else{
	    dprint(1,
		(debugfile,
		"Bug in addrbook: case ListBlankTop with no selected entry\n"));
	    goto oops;
	}
	}

	break;

      case DlcListBlankBottom:
	new->dlcelnum = old->dlcelnum;
	abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)new->dlcelnum, Normal);
	if(F_ON(F_EXPANDED_DISTLISTS,ps_global)
	  || exp_is_expanded(pab->address_book->exp, (a_c_arg_t)new->dlcelnum)){
	    list_count = listmem_count_from_abe(abe);
	    if(list_count == 0)
	      new->type = DlcListEmpty;
	    else{
		new->type      = DlcListEnt;
		new->dlcoffset = list_count - 1;
	    }
	}
	else
	  new->type = DlcListClickHere;

	break;

      case DlcGlobDelim1:
	if(as.how_many_personals == 0)
	  new->type = DlcPersAdd;
	else{
	    new->adrbk_num = as.how_many_personals - 1;
	    new->type = DlcSubTitle;
	}
	break;

      case DlcGlobDelim2:
	new->type = DlcGlobDelim1;
	break;

      case DlcPersAdd:
	new->type = DlcOneBeforeBeginning;
	break;

      case DlcGlobAdd:
	new->type = DlcGlobDelim2;
	break;

      case DlcDirAccess:
	if(old->adrbk_num == 0 && as.n_addrbk == 0)
	  new->type = DlcOneBeforeBeginning;
	else if(old->adrbk_num == 0)
	  new->type = DlcDirDelim2;
	else
	  new->type = DlcDirBlankTop;

	break;
	
      case DlcDirSubTitle:
	new->type = DlcDirAccess;
	break;

      case DlcDirBlankTop:
	new->adrbk_num = old->adrbk_num -1;
	new->type = DlcDirSubTitle;
	break;

      case DlcDirDelim1:
        new->adrbk_num = as.n_addrbk - 1;
	if(as.n_addrbk == 0)
	  new->type = DlcNoAbooks;
	else
	  new = get_bottom_dl_of_adrbk(new->adrbk_num, new);

	break;

      case DlcDirDelim1a:
        new->type = DlcDirDelim1;
	break;

      case DlcDirDelim1b:
        new->type = DlcDirDelim1a;
	break;

      case DlcDirDelim1c:
        new->type = DlcDirDelim1b;
	break;

      case DlcDirDelim2:
	if(F_ON(F_CMBND_ABOOK_DISP,ps_global))
          new->type = DlcDirDelim1c;
	else
          new->type = DlcDirDelim1;

	break;

      case DlcTitleDashTopCmb:
	if(old->adrbk_num == 0)
	  new->type = DlcOneBeforeBeginning;
	else
	  new->type = DlcTitleBlankTopCmb;

	break;

      case DlcTitleCmb:
	new->type = DlcTitleDashTopCmb;
	break;

      case DlcTitleDashBottomCmb:
	new->type = DlcTitleCmb;
	break;

      case DlcTitleBlankBottomCmb:
	new->type = DlcTitleDashBottomCmb;
	break;

      case DlcClickHereCmb:
	if(as.n_addrbk == 1 && as.n_serv == 0)
	  new->type = DlcOneBeforeBeginning;
	else
	  new->type = DlcTitleBlankBottomCmb;

	break;

      case DlcTitleBlankTopCmb:
	new->adrbk_num = old->adrbk_num - 1;
	new = get_bottom_dl_of_adrbk(new->adrbk_num, new);
	break;

      case DlcBeginning:
      case DlcTwoBeforeBeginning:
	new->type = DlcBeginning;
	break;

      case DlcOneBeforeBeginning:
	new->type = DlcTwoBeforeBeginning;
	break;

oops:
      default:
	q_status_message(SM_ORDER | SM_DING, 5, 10,
	    "Bug in addrbook, not supposed to happen, re-syncing...");
	dprint(1,
	    (debugfile,
	    "Bug in addrbook, impossible case (%d) in dlc_prev, re-sync\n",
	    old->type));
	/* jump back to a safe starting point */
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }

    new->global_row = old->global_row - 1L;
    if(new->adrbk_num == -2)
      new->adrbk_num = old->adrbk_num;

    return(new);
}


/*
 * Get the dlc element that comes after "old".  The function that calls this
 * function is the one that keeps a cache and checks in the cache before
 * calling here.
 */
DL_CACHE_S *
dlc_next(old, new)
    DL_CACHE_S *old, *new;
{
    PerAddrBook  *pab;
    AdrBk_Entry  *abe;
    adrbk_cntr_t  ab_count;
    adrbk_cntr_t  list_count;

    new->adrbk_num  = -2;
    new->dlcelnum   = NO_NEXT;
    new->dlcoffset  = NO_NEXT;
    new->type       = DlcNotSet;
    pab = &as.adrbks[old->adrbk_num];

    switch(old->type){
      case DlcTitle:
      case DlcTitleNoPerm:
	new->type = DlcSubTitle;
	break;

      case DlcSubTitle:
	if((old->adrbk_num == as.how_many_personals - 1) &&
	   (as.config || as.n_addrbk > as.how_many_personals))
	  new->type = DlcGlobDelim1;
	else if(as.n_serv && !as.config &&
		(old->adrbk_num == as.n_addrbk - 1))
	  new->type = DlcDirDelim1;
	else if(old->adrbk_num == as.n_addrbk - 1)
	  new->type = DlcEnd;
	else{
	    new->adrbk_num = old->adrbk_num + 1;
	    new->type = DlcTitleBlankTop;
	}

	break;

      case DlcTitleBlankTop:
	if(pab->access == NoAccess)
	  new->type = DlcTitleNoPerm;
	else
	  new->type = DlcTitle;

	break;

      case DlcEmpty:
      case DlcZoomEmpty:
      case DlcNoPermission:
      case DlcClickHereCmb:
	if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(old->adrbk_num < as.n_addrbk - 1){
		new->adrbk_num = old->adrbk_num + 1;
		new->type = DlcTitleBlankTopCmb;
	    }
	    else if(as.n_serv)
	      new->type = DlcDirDelim1;
	    else
	      new->type = DlcEnd;
	}
	else
          new->type = DlcEnd;

	break;

      case DlcNoAbooks:
      case DlcGlobAdd:
      case DlcEnd:
        new->type = DlcEnd;
	break;

      case DlcSimple:
	{
	adrbk_cntr_t el;
	long i;

	ab_count = adrbk_count(pab->address_book);
	i = old->dlcelnum;
	i++;
	el = old->dlcelnum + 1;
	while(i < ab_count){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el++;
	    i++;
	}

	if(i < ab_count){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)new->dlcelnum,
			       Normal);
	    if(abe && abe->tag == Single)
	      new->type = DlcSimple;
	    else if(abe && abe->tag == List)
	      new->type = DlcListBlankTop;
	}
	else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(old->adrbk_num < as.n_addrbk - 1){
		new->adrbk_num = old->adrbk_num + 1;
		new->type = DlcTitleBlankTopCmb;
	    }
	    else if(as.n_serv)
	      new->type = DlcDirDelim1;
	    else
	      new->type = DlcEnd;
	}
	else
	  new->type = DlcEnd;
	}

	break;

      case DlcListHead:
	new->dlcelnum = old->dlcelnum;
	abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)new->dlcelnum, Normal);
	if(F_ON(F_EXPANDED_DISTLISTS,ps_global)
	  || exp_is_expanded(pab->address_book->exp, (a_c_arg_t)new->dlcelnum)){
	    list_count = listmem_count_from_abe(abe);
	    if(list_count == 0)
	      new->type = DlcListEmpty;
	    else{
		new->type      = DlcListEnt;
		new->dlcoffset = 0;
	    }
	}
	else
	  new->type = DlcListClickHere;

	break;

      case DlcListEnt:
	new->dlcelnum = old->dlcelnum;
	abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)new->dlcelnum, Normal);
	list_count = listmem_count_from_abe(abe);
	if(old->dlcoffset == list_count - 1){  /* last member of list */
	    adrbk_cntr_t el;
	    long i;

	    ab_count = adrbk_count(pab->address_book);
	    i = old->dlcelnum;
	    i++;
	    el = old->dlcelnum + 1;
	    while(i < ab_count){
		if(!as.zoomed || entry_is_selected(pab->address_book->selects,
						   (a_c_arg_t)el))
		  break;
		
		el++;
		i++;
	    }

	    if(i < ab_count)
	      new->type = DlcListBlankBottom;
	    else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
		if(old->adrbk_num < as.n_addrbk - 1){
		    new->adrbk_num = old->adrbk_num + 1;
		    new->type = DlcTitleBlankTopCmb;
		}
		else if(as.n_serv)
		  new->type = DlcDirDelim1;
		else
		  new->type = DlcEnd;
	    }
	    else
	      new->type = DlcEnd;
	}
	else{
	    new->type      = DlcListEnt;
	    new->dlcoffset = old->dlcoffset + 1;
	}

	break;

      case DlcListClickHere:
      case DlcListEmpty:
	{
	adrbk_cntr_t el;
	long i;

	new->dlcelnum = old->dlcelnum;
	ab_count = adrbk_count(pab->address_book);
	i = old->dlcelnum;
	i++;
	el = old->dlcelnum + 1;
	while(i < ab_count){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el++;
	    i++;
	}

	if(i < ab_count)
	  new->type = DlcListBlankBottom;
	else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(old->adrbk_num < as.n_addrbk - 1){
		new->adrbk_num = old->adrbk_num + 1;
		new->type = DlcTitleBlankTopCmb;
	    }
	    else if(as.n_serv)
	      new->type = DlcDirDelim1;
	    else
	      new->type = DlcEnd;
	}
	else
	  new->type = DlcEnd;
	}

	break;

      case DlcListBlankTop:
	new->type   = DlcListHead;
	new->dlcelnum  = old->dlcelnum;
	break;

      case DlcListBlankBottom:
	{
	adrbk_cntr_t el;
	long i;

	ab_count = adrbk_count(pab->address_book);
	i = old->dlcelnum;
	i++;
	el = old->dlcelnum + 1;
	while(i < ab_count){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el++;
	    i++;
	}

	if(i < ab_count){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)new->dlcelnum,
			       Normal);
	    if(abe && abe->tag == Single)
	      new->type = DlcSimple;
	    else if(abe && abe->tag == List)
	      new->type = DlcListHead;
	}
	else
	  /* can't happen */
	  new->type = DlcEnd;
	}

	break;

      case DlcGlobDelim1:
	new->type = DlcGlobDelim2;
	break;

      case DlcGlobDelim2:
	if(as.config && as.how_many_personals == as.n_addrbk)
	  new->type = DlcGlobAdd;
	else{
	    new->adrbk_num = as.how_many_personals;
	    pab = &as.adrbks[new->adrbk_num];
	    if(pab->access == NoAccess)
	      new->type = DlcTitleNoPerm;
	    else
	      new->type = DlcTitle;
	}

	break;

      case DlcDirDelim1:
	if(F_ON(F_CMBND_ABOOK_DISP,ps_global))
	  new->type = DlcDirDelim1a;
	else
	  new->type = DlcDirDelim2;

	new->adrbk_num = 0;
	break;

      case DlcDirDelim1a:
	new->type = DlcDirDelim1b;
	break;

      case DlcDirDelim1b:
	new->type = DlcDirDelim1c;
	break;

      case DlcDirDelim1c:
	new->type = DlcDirDelim2;
	new->adrbk_num = 0;
	break;

      case DlcDirDelim2:
	new->type = DlcDirAccess;
	new->adrbk_num = 0;
	break;

      case DlcPersAdd:
	new->type = DlcGlobDelim1;
	break;

      case DlcDirAccess:
	new->type = DlcDirSubTitle;
	break;

      case DlcDirSubTitle:
	if(old->adrbk_num == as.n_serv - 1)
	  new->type = DlcEnd;
	else{
	    new->type = DlcDirBlankTop;
	    new->adrbk_num = old->adrbk_num + 1;
	}

	break;

      case DlcDirBlankTop:
	new->type = DlcDirAccess;
	break;

      case DlcTitleDashTopCmb:
	new->type = DlcTitleCmb;
	break;

      case DlcTitleCmb:
	new->type = DlcTitleDashBottomCmb;
	break;

      case DlcTitleDashBottomCmb:
        new->type = DlcTitleBlankBottomCmb;
	break;

      case DlcTitleBlankBottomCmb:
	pab = &as.adrbks[old->adrbk_num];
	if(pab->ostatus != Open && pab->access != NoAccess)
	  new->type = DlcClickHereCmb;
	else
	  new = get_top_dl_of_adrbk(old->adrbk_num, new);

	break;

      case DlcTitleBlankTopCmb:
	new->type = DlcTitleDashTopCmb;
	break;

      case DlcOneBeforeBeginning:
	new = get_global_top_dlc(new);
	break;

      case DlcTwoBeforeBeginning:
	new->type = DlcOneBeforeBeginning;
	break;

      default:
	q_status_message(SM_ORDER | SM_DING, 5, 10,
	    "Bug in addrbook, not supposed to happen, re-syncing...");
	dprint(1,
	    (debugfile,
	    "Bug in addrbook, impossible case (%d) in dlc_next, re-sync\n",
	    old->type));
	/* jump back to a safe starting point */
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }

    new->global_row = old->global_row + 1L;
    if(new->adrbk_num == -2)
      new->adrbk_num = old->adrbk_num;

    return(new);
}


/*
 * Get the display line at the very top of whole addrbook screen display.
 */
DL_CACHE_S *
get_global_top_dlc(new)
    DL_CACHE_S *new;  /* fill in answer here */
{
    PerAddrBook  *pab;

    new->dlcelnum   = NO_NEXT;
    new->dlcoffset  = NO_NEXT;
    new->type       = DlcNotSet;

    if(F_ON(F_CMBND_ABOOK_DISP,ps_global) && !as.config){
	new->adrbk_num = 0;
	if(as.n_addrbk > 1 || (as.n_serv && as.n_addrbk == 1))
	  new->type = DlcTitleDashTopCmb;
	else if(as.n_addrbk == 1)
	  new = get_top_dl_of_adrbk(new->adrbk_num, new);
	else if(as.n_serv > 0){				/* 1st directory */
	    new->type = DlcDirAccess;
	    new->adrbk_num = 0;
	}
	else
	  new->type = DlcNoAbooks;
    }
    else{
	new->adrbk_num = 0;

	if(as.config){
	    if(as.how_many_personals == 0)		/* no personals */
	      new->type = DlcPersAdd;
	    else{
		pab = &as.adrbks[new->adrbk_num];	/* 1st personal */
		if(pab->access == NoAccess)
		  new->type = DlcTitleNoPerm;
		else
		  new->type = DlcTitle;
	    }
	}
	else if(any_ab_open()){
	    new->adrbk_num = as.cur;
	    new = get_top_dl_of_adrbk(new->adrbk_num, new);
	}
	else if(as.n_addrbk > 0){			/* 1st addrbook */
	    pab = &as.adrbks[new->adrbk_num];
	    if(pab->access == NoAccess)
	      new->type = DlcTitleNoPerm;
	    else
	      new->type = DlcTitle;
	}
	else if(as.n_serv > 0){				/* 1st directory */
	    new->type = DlcDirAccess;
	    new->adrbk_num = 0;
	}
	else
	  new->type = DlcNoAbooks;
    }

    return(new);
}


/*
 * Get the last display line for the whole address book screen.
 * This gives us a way to start at the end and move back up.
 */
DL_CACHE_S *
get_global_bottom_dlc(new)
    DL_CACHE_S *new;  /* fill in answer here */
{
    new->dlcelnum   = NO_NEXT;
    new->dlcoffset  = NO_NEXT;
    new->type       = DlcNotSet;

    if(F_ON(F_CMBND_ABOOK_DISP,ps_global) && !as.config){
	if(as.n_serv){
	    new->type = DlcDirSubTitle;
	    new->adrbk_num = as.n_serv - 1;
	}
	else if(as.n_addrbk > 0){
	    new->adrbk_num = as.n_addrbk - 1;
	    new = get_bottom_dl_of_adrbk(new->adrbk_num, new);
	}
	else
	  new->type = DlcNoAbooks;
    }
    else{
	new->adrbk_num = max(as.n_addrbk - 1, 0);

	if(as.config){
	    if(as.how_many_personals == as.n_addrbk)	/* no globals */
	      new->type = DlcGlobAdd;
	    else
	      new->type = DlcSubTitle;
	}
	else if(any_ab_open()){
	    new->adrbk_num = as.cur;
	    new = get_bottom_dl_of_adrbk(new->adrbk_num, new);
	}
	else if(as.n_serv){
	    new->type = DlcDirSubTitle;
	    new->adrbk_num = as.n_serv - 1;
	}
	else{				/* !config && !opened && !n_serv */
	    if(as.n_addrbk)
	      new->type = DlcSubTitle;
	    else
	      new->type = DlcNoAbooks;
	}
    }

    return(new);
}


/*
 * First dl in a particular addrbook, not counting title lines.
 * Assumes as.opened.
 */
DL_CACHE_S *
get_top_dl_of_adrbk(adrbk_num, new)
    int         adrbk_num;
    DL_CACHE_S *new;  /* fill in answer here */
{
    PerAddrBook  *pab;
    AdrBk_Entry  *abe;
    adrbk_cntr_t  ab_count;

    pab = &as.adrbks[adrbk_num];
    new->adrbk_num = adrbk_num;

    if(pab->access == NoAccess)
      new->type = DlcNoPermission;
    else{
	adrbk_cntr_t el;
	long i;

	ab_count = adrbk_count(pab->address_book);

	i = 0L;
	el = 0;
	/* find first displayed entry */
	while(i < ab_count){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el++;
	    i++;
	}

	if(i < ab_count){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)new->dlcelnum,
			       Normal);
	    if(abe && abe->tag == Single)
	      new->type = DlcSimple;
	    else if(abe && abe->tag == List)
	      new->type = DlcListHead;
	}
	else if(as.zoomed)
	  new->type = DlcZoomEmpty;
	else
	  new->type = DlcEmpty;
    }

    return(new);
}


/*
 * Find the last display line for addrbook number adrbk_num.
 * Assumes as.opened (unless OLD_ABOOK_DISP).
 */
DL_CACHE_S *
get_bottom_dl_of_adrbk(adrbk_num, new)
    int         adrbk_num;
    DL_CACHE_S *new;  /* fill in answer here */
{
    PerAddrBook  *pab;
    AdrBk_Entry  *abe;
    adrbk_cntr_t  ab_count;
    adrbk_cntr_t  list_count;

    pab = &as.adrbks[adrbk_num];
    new->adrbk_num = adrbk_num;

    if(F_ON(F_CMBND_ABOOK_DISP,ps_global) && pab->ostatus != Open){
	if(pab->access == NoAccess)
	  new->type = DlcNoPermission;
	else
	  new->type = DlcClickHereCmb;
    }
    else if(F_OFF(F_CMBND_ABOOK_DISP,ps_global) && pab->ostatus != Open){
	new->type = DlcSubTitle;
    }
    else{
	if(pab->access == NoAccess)
	  new->type = DlcNoPermission;
	else{
	    adrbk_cntr_t el;
	    long i;

	    ab_count = adrbk_count(pab->address_book);
	    i = ab_count - 1;
	    el = ab_count - 1;
	    /* find last displayed entry */
	    while(i >= 0L){
		if(!as.zoomed || entry_is_selected(pab->address_book->selects,
						   (a_c_arg_t)el))
		  break;
		
		el--;
		i--;
	    }

	    if(i >= 0){
		new->dlcelnum = el;
		abe = adrbk_get_ae(pab->address_book,
				   (a_c_arg_t)new->dlcelnum,
				   Normal);
		if(abe && abe->tag == Single)
		  new->type = DlcSimple;
		else if(abe && abe->tag == List){
		    if(F_ON(F_EXPANDED_DISTLISTS,ps_global)
		       || exp_is_expanded(pab->address_book->exp,
					  (a_c_arg_t)new->dlcelnum)){
			list_count = listmem_count_from_abe(abe);
			if(list_count == 0)
			  new->type = DlcListEmpty;
			else{
			    new->type      = DlcListEnt;
			    new->dlcoffset = list_count - 1;
			}
		    }
		    else
		      new->type = DlcListClickHere;
		}
	    }
	    else if(as.zoomed)
	      new->type = DlcZoomEmpty;
	    else
	      new->type = DlcEmpty;
	}

    }

    return(new);
}


/*
 * Uses information in new to fill in new->dl.
 */
void
fill_in_dl_field(new)
    DL_CACHE_S *new;
{
    AddrScrn_Disp *dl;
    PerAddrBook   *pab;
    char buf[MAX_SCREEN_COLS + 1];
    char buf2[1024];
    char hostbuf[128];
    char *folder;
    char *q;
    int screen_width = ps_global->ttyo->screen_cols;
    int len;
    static char *eight_spaces = "        ";
    char *four_spaces = eight_spaces + 4;

    buf[MAX_SCREEN_COLS] = '\0';
    screen_width = min(MAX_SCREEN_COLS, screen_width);

    dl = &(new->dl);

    /* free any previously allocated space */
    switch(dl->type){
      case Text:
      case Title:
      case TitleCmb:
      case AskServer:
	if(dl->txt)
	  fs_give((void **)&dl->txt);
    }

    /* set up new dl */
    switch(new->type){
      case DlcListBlankTop:
      case DlcListBlankBottom:
      case DlcGlobDelim1:
      case DlcGlobDelim2:
      case DlcDirDelim1:
      case DlcDirDelim2:
      case DlcTitleBlankTop:
      case DlcDirBlankTop:
      case DlcTitleBlankBottomCmb:
      case DlcTitleBlankTopCmb:
	dl->type = Text;
	dl->txt  = cpystr("");
	break;

      case DlcTitleDashTopCmb:
      case DlcTitleDashBottomCmb:
      case DlcDirDelim1a:
      case DlcDirDelim1c:
	/* line of dashes in txt field */
	dl->type = Text;
	memset((void *)buf, '-', screen_width * sizeof(char));
	buf[screen_width] = '\0';
	dl->txt = cpystr(buf);
	break;

      case DlcNoPermission:
	dl->type = Text;
	dl->txt  = cpystr(NO_PERMISSION);
	break;

      case DlcTitle:
      case DlcTitleNoPerm:
	dl->type = Title;
	pab = &as.adrbks[new->adrbk_num];
        /* title for this addrbook */
        memset((void *)buf, SPACE, screen_width * sizeof(char));
	buf[screen_width] = '\0';
        sprintf(buf2, "%.20s%.100s", four_spaces,
		pab->nickname ? pab->nickname : pab->filename);
	len = strlen(buf2);
	len = min(len, screen_width);
        strncpy(buf, buf2, len);	/* leave the spaces */
        if(as.ro_warning && pab->access == ReadOnly){
	    q = buf + screen_width - strlen(READONLY);
	    strcpy(q, READONLY);
        }

	/* put this on top of readonly warning */
	if(pab->access == NoAccess){
	    q = buf + screen_width - strlen(NOACCESS);
	    strcpy(q, NOACCESS);
	}

	dl->txt = cpystr(buf);
	break;

      case DlcSubTitle:
	dl->type = Text;
	pab = &as.adrbks[new->adrbk_num];
        memset((void *)buf, SPACE, screen_width * sizeof(char));
	buf[screen_width] = '\0';
	/* Find a hostname to put in title */
	hostbuf[0] = '\0';
	folder = NULL;
	if(pab->type & REMOTE_VIA_IMAP && pab->filename){
	    char *start, *end;

	    start = strindex(pab->filename, '{');
	    if(start)
	      end = strindex(start+1, '}');

	    if(start && end){
		strncpy(hostbuf, start + 1,
			min(end - start - 1, sizeof(hostbuf)-1));
		hostbuf[min(end - start - 1, sizeof(hostbuf)-1)] = '\0';
		if(*(end+1))
		  folder = end+1;
	    }
	}

	if(!folder)
	  folder = pab->filename;

	/*
	 * Just trying to find the name relative to the home directory
	 * to display in an OS-independent way.
	 */
	if(folder && in_dir(ps_global->home_dir, folder)){
	    char *p, *new_folder = NULL, *savep = NULL;
	    int   l, saveval = 0;

	    l = strlen(ps_global->home_dir);

	    while(!new_folder && (p = last_cmpnt(folder)) != NULL){
		if(savep){
		    *savep = saveval;
		    savep = NULL;
		}

		if(folder + l == p || folder + l + 1 == p)
		  new_folder = p;
		else{
		    savep = --p;
		    saveval = *savep;
		    *savep = '\0';
		}
	    }

	    if(savep)
	      *savep = saveval;

	    if(new_folder)
	      folder = new_folder;
	}

        sprintf(buf2, "%.20s%.20s AddressBook%.20s%.100s in %.100s",
		eight_spaces,
		(pab->type & GLOBAL) ? "Global" : "Personal",
		(pab->type & REMOTE_VIA_IMAP && *hostbuf) ? " on " : "",
		(pab->type & REMOTE_VIA_IMAP && *hostbuf) ? hostbuf : "",
		(folder && *folder) ? folder : "<?>");
	len = strlen(buf2);
	len = min(len, screen_width);
        strncpy(buf, buf2, len);	/* leave the spaces */
	dl->txt = cpystr(buf);
	break;

      case DlcTitleCmb:
	dl->type = TitleCmb;
	pab = &as.adrbks[new->adrbk_num];
        /* title for this addrbook */
        memset((void *)buf, SPACE, screen_width * sizeof(char));
	buf[screen_width] = '\0';
        sprintf(buf2, "%.20s AddressBook <%.100s>",
		    (new->adrbk_num < as.how_many_personals) ?
			"Personal" :
			"Global",
                    pab->nickname ? pab->nickname : pab->filename);
	len = strlen(buf2);
	len = min(len, screen_width);
        strncpy(buf, buf2, len);	/* leave the spaces */
        if(as.ro_warning && pab->access == ReadOnly){
	    q = buf + screen_width - strlen(READONLY);
	    strcpy(q, READONLY);
        }

	/* put this on top of readonly warning */
	if(pab->access == NoAccess){
	    q = buf + screen_width - strlen(NOACCESS);
	    strcpy(q, NOACCESS);
	}

	dl->txt = cpystr(buf);
	break;

      case DlcDirDelim1b:
	dl->type = Text;
        memset((void *)buf, SPACE, screen_width * sizeof(char));
	buf[screen_width] = '\0';
	strncpy(buf2, OLDSTYLE_DIR_TITLE, sizeof(buf2)-1);
	len = strlen(buf2);
	len = min(len, screen_width);
        strncpy(buf, buf2, len);	/* leave the spaces */
	dl->txt = cpystr(buf);
	break;

      case DlcClickHereCmb:
	dl->type  = ClickHereCmb;
	break;

      case DlcListClickHere:
	dl->type  = ListClickHere;
	dl->elnum = new->dlcelnum;
	break;

      case DlcListEmpty:
	dl->type  = ListEmpty;
	dl->elnum = new->dlcelnum;
	break;

      case DlcEmpty:
	dl->type = Empty;
	break;

      case DlcZoomEmpty:
	dl->type = ZoomEmpty;
	break;

      case DlcNoAbooks:
	dl->type = NoAbooks;
	break;

      case DlcPersAdd:
	dl->type = AddFirstPers;
	break;

      case DlcGlobAdd:
	dl->type = AddFirstGlob;
	break;

      case DlcDirAccess:
	dl->type = AskServer;

#ifdef	ENABLE_LDAP
	{
	LDAP_SERV_S *info;

	info =
	     break_up_ldap_server(ps_global->VAR_LDAP_SERVERS[new->adrbk_num]);
	sprintf(buf2, "%.20s%.100s",
		four_spaces,
		(info && info->nick && *info->nick) ? info->nick :
		  (info && info->serv && *info->serv) ? info->serv :
		    comatose(new->adrbk_num + 1));
	if(info)
	  free_ldap_server_info(&info);
	}
#else
	sprintf(buf2, "%.20s%.20s", four_spaces, comatose(new->adrbk_num + 1));
#endif

        memset((void *)buf, SPACE, screen_width * sizeof(char));
	buf[screen_width] = '\0';
	len = strlen(buf2);
	len = min(len, screen_width);
        strncpy(buf, buf2, len);	/* leave the spaces */
	dl->txt = cpystr(buf);
	break;

      case DlcDirSubTitle:
	dl->type = Text;
#ifdef	ENABLE_LDAP
	{
	LDAP_SERV_S *info;

	info =
	     break_up_ldap_server(ps_global->VAR_LDAP_SERVERS[new->adrbk_num]);
	if(info && info->port >= 0)
	  sprintf(buf2, "%.20sDirectory Server on %.100s:%d",
		  eight_spaces,
		  (info && info->serv && *info->serv) ? info->serv : "<?>",
		  info->port);
	else
	  sprintf(buf2, "%.20sDirectory Server on %.100s",
		  eight_spaces,
		  (info && info->serv && *info->serv) ? info->serv : "<?>");

	if(info)
	  free_ldap_server_info(&info);
	}
#else
	sprintf(buf2, "%.20sDirectory Server %.20s", eight_spaces,
		comatose(new->adrbk_num + 1));
#endif

        memset((void *)buf, SPACE, screen_width * sizeof(char));
	buf[screen_width] = '\0';
	len = strlen(buf2);
	len = min(len, screen_width);
        strncpy(buf, buf2, len);	/* leave the spaces */
	dl->txt = cpystr(buf);
	break;

      case DlcSimple:
	dl->type  = Simple;
	dl->elnum = new->dlcelnum;
	break;

      case DlcListHead:
	dl->type  = ListHead;
	dl->elnum = new->dlcelnum;
	break;

      case DlcListEnt:
	dl->type     = ListEnt;
	dl->elnum    = new->dlcelnum;
	dl->l_offset = new->dlcoffset;
	break;

      case DlcBeginning:
      case DlcOneBeforeBeginning:
      case DlcTwoBeforeBeginning:
	dl->type = Beginning;
	break;

      case DlcEnd:
	dl->type = End;
	break;

      default:
	q_status_message(SM_ORDER | SM_DING, 5, 10,
	    "Bug in addrbook, not supposed to happen, re-syncing...");
	dprint(1,
	    (debugfile,
	    "Bug in addrbook, impossible dflt in fill_in_dl (%d)\n",
	    new->type));
	/* jump back to a safe starting point */
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }
}


/*
 * Returns 1 if any addrbooks are in Open state, 0 otherwise.
 *
 * This is a test for ostatus == Open, not for whether or not the address book
 * FILE is opened.
 */
int
any_ab_open()
{
    int i, ret = 0;

    if(as.initialized)
      for(i = 0; ret == 0 && i < as.n_addrbk; i++)
	if(as.adrbks[i].ostatus == Open)
	  ret++;
    
    return(ret);
}


/*
 * Returns a pointer to the member_number'th list member of the list
 * associated with this display line.
 */
char *
listmem(row)
    long row;
{
    PerAddrBook *pab;
    AddrScrn_Disp *dl;

    dl = dlist(row);
    if(dl->type != ListEnt)
      return((char *)NULL);

    pab = &as.adrbks[adrbk_num_from_lineno(row)];

    return(listmem_from_dl(pab->address_book, dl));
}


/*
 * Returns a pointer to the list member
 * associated with this display line.
 */
char *
listmem_from_dl(address_book, dl)
    AdrBk         *address_book;
    AddrScrn_Disp *dl;
{
    AdrBk_Entry *abe;
    char **p = (char **)NULL;

    /* This shouldn't happen */
    if(dl->type != ListEnt)
      return((char *)NULL);

    abe = adrbk_get_ae(address_book, (a_c_arg_t)dl->elnum, Normal);

    /*
     * If we wanted to be more careful, We'd go through the list making sure
     * we don't pass the end.  We'll count on the caller being careful
     * instead.
     */
    if(abe && abe->tag == List){
	p =  abe->addr.list;
	p += dl->l_offset;
    }

    return((p && *p) ? *p : (char *)NULL);
}


/*
 * How many members in list?
 */
adrbk_cntr_t
listmem_count_from_abe(abe)
    AdrBk_Entry *abe;
{
    char **p;

    if(abe->tag != List)
      return 0;

    for(p = abe->addr.list; p != NULL && *p != NULL; p++)
      ;/* do nothing */
    
    return((adrbk_cntr_t)(p - abe->addr.list));
}


/*
 * Make sure addrbooks are minimally initialized.
 */
void
init_ab_if_needed()
{
    dprint(9, (debugfile, "- init_ab_if_needed -\n"));

    if(!as.initialized)
      (void)init_addrbooks(Closed, 0, 0, 1);
}


/*
 * Sets everything up to get started.
 *
 * Args: want_status      -- The desired OpenStatus for all addrbooks.
 *       reset_to_top     -- Forget about the old location and put cursor
 *                           at top.
 *       open_if_only_one -- If want_status is HalfOpen and there is only
 *                           section to look at, then promote want_status
 *                           to Open.
 *       ro_warning       -- Set ReadOnly warning global
 *
 * Return: 1 if ok, 0 if problem
 */
int
init_addrbooks(want_status, reset_to_top, open_if_only_one, ro_warning)
    OpenStatus want_status;
    int        reset_to_top,
	       open_if_only_one,
	       ro_warning;
{
    register PerAddrBook *pab;
    char *q, **t;
    long line;

    dprint(4, (debugfile, "-- init_addrbooks(%s, %d, %d, %d) --\n",
		    want_status==Open ?
				"Open" :
				want_status==HalfOpen ?
					"HalfOpen" :
				   want_status==ThreeQuartOpen ?
					"ThreeQuartOpen" :
					want_status==NoDisplay ?
						"NoDisplay" :
						"Closed",
		    reset_to_top, open_if_only_one, ro_warning));

    as.l_p_page = ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)
					       - HEADER_ROWS(ps_global);
    if(as.l_p_page <= 0)
      as.no_op_possbl++;
    else
      as.no_op_possbl = 0;

    as.ro_warning = ro_warning;

    /* already been initialized */
    if(as.initialized){
	int i;

	/*
	 * Special case. If there is only one addressbook we start the
	 * user out with that open.
	 */
	if(want_status == HalfOpen &&
	   ((open_if_only_one && as.n_addrbk == 1 && as.n_serv == 0) ||
	    (F_ON(F_EXPANDED_ADDRBOOKS, ps_global) &&
	     F_ON(F_CMBND_ABOOK_DISP, ps_global))))
	  want_status = Open;

	/* open to correct state */
	for(i = 0; i < as.n_addrbk; i++)
	  init_abook(&as.adrbks[i], want_status);

	if(reset_to_top){
	    warp_to_beginning();
	    as.top_ent     = 0L;
	    line           = first_selectable_line(0L);
	    if(line == NO_LINE)
	      as.cur_row = 0L;
	    else
	      as.cur_row = line;

	    if(as.cur_row >= as.l_p_page)
	      as.top_ent += (as.cur_row - as.l_p_page + 1);

	    as.old_cur_row = as.cur_row;
	}

	dprint(4, (debugfile, "init_addrbooks: already initialized: %d books\n",
				    as.n_addrbk));
        return((as.n_addrbk + as.n_serv) ? 1 : 0);
    }

    as.initialized = 1;

    /* count directory servers */
    as.n_serv = 0;
    as.n_impl = 0;
#ifdef	ENABLE_LDAP
    if(ps_global->VAR_LDAP_SERVERS &&
       ps_global->VAR_LDAP_SERVERS[0] &&
       ps_global->VAR_LDAP_SERVERS[0][0])
	for(t = ps_global->VAR_LDAP_SERVERS; t[0] && t[0][0]; t++){
	    LDAP_SERV_S *info;

	    as.n_serv++;
	    info = break_up_ldap_server(*t);
	    as.n_impl += (info && info->impl) ? 1 : 0;
	    if(info)
	      free_ldap_server_info(&info);
	}
#endif	/* ENABLE_LDAP */

    /* count addressbooks */
    as.how_many_personals = 0;
    if(ps_global->VAR_ADDRESSBOOK &&
       ps_global->VAR_ADDRESSBOOK[0] &&
       ps_global->VAR_ADDRESSBOOK[0][0])
	for(t = ps_global->VAR_ADDRESSBOOK; t[0] && t[0][0]; t++)
	  as.how_many_personals++;

    as.n_addrbk = as.how_many_personals;
    if(ps_global->VAR_GLOB_ADDRBOOK &&
       ps_global->VAR_GLOB_ADDRBOOK[0] &&
       ps_global->VAR_GLOB_ADDRBOOK[0][0])
	for(t = ps_global->VAR_GLOB_ADDRBOOK; t[0] && t[0][0]; t++)
	  as.n_addrbk++;

    if(want_status == HalfOpen &&
       ((open_if_only_one && as.n_addrbk == 1 && as.n_serv == 0) ||
	(F_ON(F_EXPANDED_ADDRBOOKS, ps_global) &&
	 F_ON(F_CMBND_ABOOK_DISP, ps_global))))
      want_status = Open;


    /*
     * allocate array of PerAddrBooks
     * (we don't give this up until we exit Pine, but it's small)
     */
    if(as.n_addrbk){
	as.adrbks = (PerAddrBook *)fs_get(as.n_addrbk * sizeof(PerAddrBook));
	memset((void *)as.adrbks, 0, as.n_addrbk * sizeof(PerAddrBook));

	/* init PerAddrBook data */
	for(as.cur = 0; as.cur < as.n_addrbk; as.cur++){
	    char *nickname = NULL,
		 *filename = NULL;

	    if(as.cur < as.how_many_personals)
	      q = ps_global->VAR_ADDRESSBOOK[as.cur];
	    else
	      q = ps_global->VAR_GLOB_ADDRBOOK[as.cur - as.how_many_personals];

	    pab = &as.adrbks[as.cur];
	    
	    /* Parse entry for optional nickname and filename */
	    get_pair(q, &nickname, &filename, 0, 0);

	    if(nickname && !*nickname)
	      fs_give((void **)&nickname);
	    
	    strcpy(tmp_20k_buf, filename);
	    fs_give((void **)&filename);

	    filename = tmp_20k_buf;
	    if(nickname == NULL)
	      pab->nickname = cpystr(filename);
	    else
	      pab->nickname = nickname;

	    if(*filename == '~')
	      fnexpand(filename, SIZEOF_20KBUF);

	    if(*filename == '{' || is_absolute_path(filename)){
		pab->filename = cpystr(filename); /* fully qualified */
	    }
	    else{
		char book_path[MAXPATH+1];
		char *lc = last_cmpnt(ps_global->pinerc);

		book_path[0] = '\0';
		if(lc != NULL){
		    strncpy(book_path, ps_global->pinerc,
			    min(lc - ps_global->pinerc, sizeof(book_path)-1));
		    book_path[min(lc - ps_global->pinerc,
				  sizeof(book_path)-1)] = '\0';
		}

		strncat(book_path, filename,
			sizeof(book_path)-1-strlen(book_path));
		pab->filename = cpystr(book_path);
	    }

	    if(*pab->filename == '{')
	      pab->type |= REMOTE_VIA_IMAP;

	    if(as.cur >= as.how_many_personals)
	      pab->type |= GLOBAL;

	    pab->access = adrbk_access(pab);

	    /* global address books are forced readonly */
	    if(pab->type & GLOBAL && pab->access != NoAccess)
	      pab->access = ReadOnly;

	    pab->ostatus  = TotallyClosed;

	    /*
	     * and remember that the memset above initializes everything
	     * else to 0
	     */

	    init_abook(pab, want_status);
	}
    }

    /*
     * Have to reset_to_top in this case since this is the first open,
     * regardless of the value of the argument, since these values haven't been
     * set before here.
     */
    as.cur         = 0;
    as.top_ent     = 0L;
    warp_to_beginning();
    line           = first_selectable_line(0L);

    if(line == NO_LINE)
      as.cur_row = 0L;
    else
      as.cur_row = line;

    if(as.cur_row >= as.l_p_page){
	as.top_ent += (as.cur_row - as.l_p_page + 1);
	as.cur_row = as.l_p_page - 1;
    }

    as.old_cur_row = as.cur_row;

    return((as.n_addrbk + as.n_serv) ? 1 : 0);
}


/*
 * Create addrbook_file.lu lookup file and exit.  This is for
 * use as a stand-alone creator of .lu files.
 */
void
just_update_lookup_file(addrbook_file, sort_rule_descrip)
    char *addrbook_file;
    char *sort_rule_descrip;
{
    int        i;
    int        sort_rule;
    NAMEVAL_S *v;
    AdrBk     *ab;
    char       warning[800];
    PerAddrBook pabb;


    /*
     * This function gets called before returning from pine_args so
     * debugging hasn't been set up yet. Initialize it here.
     */
#ifdef DEBUG
    init_debug();
#endif

    /*
     * It also gets called before collation has been set up so set it
     * up here. The problem with this is that we don't know what collation
     * features the user wants yet since we haven't looked at the pinerc
     * file yet. However, we don't want to wait to go through the screen
     * setup and everything because we might be called from cron without
     * a screen. We'll just do the collation the way it works by default.
     */
    set_collation(1, 0);

    dprint(9, (debugfile, "just_update_lookup_file(%s,%s)\n",
	       addrbook_file ? addrbook_file : "<null>",
	       sort_rule_descrip ? sort_rule_descrip : "<null>"));

    memset((void *)&pabb, 0, sizeof(PerAddrBook));
    pabb.filename = addrbook_file;
    if(addrbook_file && *addrbook_file == '{'){
	fprintf(stderr, "No lu file to create for remote addrbooks\n");
	exit(0);
    }

    sort_rule = -1;
    for(i = 0; v = ab_sort_rules(i); i++){
       if(!strucmp(sort_rule_descrip, v->name)){
	   sort_rule = v->value;
	   break;
       }
    }

    if(sort_rule == -1){
	fprintf(stderr, "Sort rule %s unknown\n", sort_rule_descrip);
	exit(-1);
    }

    dprint(9, (debugfile, "sort_rule = %d\n", sort_rule));

    pabb.access = adrbk_access(&pabb);
    ab_nesting_level++;
    warning[0] = '\0';
    ab = adrbk_open(&pabb, NULL, warning, sort_rule, 1, 1);
    if(ab == NULL){
	if(*warning)
	  fprintf(stderr, "%s: %s\n", addrbook_file, warning);
	else
	  fprintf(stderr, "%s: %s\n",
		addrbook_file, errno ? error_description(errno)
				     : "unknown error");

	dprint(9, (debugfile, "adrbk_open failed\n"));
	exit(-1);
    }

    if(!adrbk_is_in_sort_order(ab, 1, 1)){
	dprint(9, (debugfile, "addrbook was not in sort order, sort it\n"));
	adrbk_set_nominal_cachesize(ab, (long)adrbk_count(ab));
	if(adrbk_sort(ab, (a_c_arg_t)0, (adrbk_cntr_t *)NULL, 1)){
	    fprintf(stderr, "Sort failed (%s)\n",
		    error_description(errno));
	    dprint(1, (debugfile, "Sort failed (%s)",
		   error_description(errno)));
	}
    }
    else
      dprint(9, (debugfile, "addrbook was in sort order, done\n"));

    ab_nesting_level--;

    exit(0);
}


/*
 * Something was changed in options screen, so need to start over.
 */
void
addrbook_reset()
{
    dprint(4, (debugfile, "- addrbook_reset -\n"));
    completely_done_with_adrbks();
}


/*
 * Returns type of access allowed on this addrbook.
 */
AccessType
adrbk_access(pab)
    PerAddrBook *pab;
{
    char       fbuf[MAXPATH+1];
    AccessType access = NoExists;
    CONTEXT_S *dummy_cntxt;

    dprint(9, (debugfile, "- addrbook_access -\n"));

    if(pab && pab->type & REMOTE_VIA_IMAP){
	/*
	 * Open_fcc creates the folder if it didn't already exist.
	 */
	if((pab->so = open_fcc(pab->filename, &dummy_cntxt, 1,
			       "Error: ",
			       " Can't fetch remote addrbook.")) != NULL){
	    /*
	     * We know the folder is there but don't know what access
	     * rights we have until we try to select it, which we don't
	     * want to do unless we have to. So delay evaluating.
	     */
	    access = MaybeRorW;
	}
    }
    else if(pab){	/* local file */
#if defined(NO_LOCAL_ADDRBOOKS)
	/* don't allow any access to local addrbooks */
	access = NoAccess;
#else /* !NO_LOCAL_ADDRBOOKS) */
	build_path(fbuf, is_absolute_path(pab->filename) ? NULL
							 : ps_global->home_dir,
		   pab->filename, sizeof(fbuf));

#if defined(DOS) || defined(OS2)
	/*
	 * Microsoft networking causes some access calls to do a DNS query (!!)
	 * when it is turned on. In particular, if there is a / in the filename
	 * this seems to happen. So, just don't allow it.
	 */
	if(strindex(fbuf, '/') != NULL){
	    dprint(2, (debugfile, "\"/\" not allowed in addrbook name\n"));
	    return NoAccess;
	}
#else /* !DOS */
	/* also prevent backslash in non-DOS addrbook names */
	if(strindex(fbuf, '\\') != NULL){
	    dprint(2, (debugfile, "\"\\\" not allowed in addrbook name\n"));
	    return NoAccess;
	}
#endif /* !DOS */

	if(can_access(fbuf, ACCESS_EXISTS) == 0){
	    if(can_access(fbuf, EDIT_ACCESS) == 0){
		char *dir, *p;

		dir = ".";
		if(p = last_cmpnt(fbuf)){
		    *--p = '\0';
		    dir  = *fbuf ? fbuf : "/";
		}

#if	defined(DOS) || defined(OS2)
		/*
		 * If the dir has become a drive letter and : (e.g. "c:")
		 * then append a "\".  The library function access() in the
		 * win 16 version of MSC seems to require this.
		 */
		if(isalpha((unsigned char) *dir)
		   && *(dir+1) == ':' && *(dir+2) == '\0'){
		    *(dir+2) = '\\';
		    *(dir+3) = '\0';
		}
#endif	/* DOS || OS2 */

		/*
		 * Even if we can edit the address book file itself, we aren't
		 * going to be able to change it unless we can also write in
		 * the directory that contains it (because we write into a
		 * temp file and then rename).
		 */
		if(can_access(dir, EDIT_ACCESS) == 0)
		  access = ReadWrite;
		else{
		    access = ReadOnly;
		    q_status_message1(SM_ORDER, 2, 2,
				      "Address book directory (%.200s) is ReadOnly",
				      dir);
		}
	    }
	    else if(can_access(fbuf, READ_ACCESS) == 0)
	      access = ReadOnly;
	    else
	      access = NoAccess;
	}
#endif /* !NO_LOCAL_ADDRBOOKS) */
    }
    
    return(access);
}


/*
 * Trim back remote address books if necessary.
 */
void
trim_remote_adrbks()
{
    register PerAddrBook *pab;
    int i;

    dprint(2, (debugfile, "- trim_remote_adrbks -\n"));

    if(!as.initialized)
      return;

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->ostatus != TotallyClosed && pab->address_book
	   && pab->address_book->rd)
	  rd_trim_remdata(&pab->address_book->rd);
    }
}


/*
 * Free and close everything.
 */
void
completely_done_with_adrbks()
{
    register PerAddrBook *pab;
    int i;

    dprint(2, (debugfile, "- completely_done_with_adrbks -\n"));

    ab_nesting_level = 0;

    if(!as.initialized)
      return;

    for(i = 0; i < as.n_addrbk; i++)
      init_abook(&as.adrbks[i], TotallyClosed);

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];

	if(pab->filename)
	  fs_give((void **)&pab->filename);

	if(pab->nickname)
	  fs_give((void **)&pab->nickname);
    }

    done_with_dlc_cache();

    if(as.adrbks)
      fs_give((void **)&as.adrbks);

    as.n_addrbk    = 0;
    as.initialized = 0;
}


/*
 * Initialize or re-initialize this address book.
 *
 *  Args: pab        -- the PerAddrBook ptr
 *       want_status -- desired OpenStatus for this address book
 */
void
init_abook(pab, want_status)
    PerAddrBook *pab;
    OpenStatus   want_status;
{
    register OpenStatus new_status;

    dprint(4, (debugfile, "- init_abook -\n"));
    dprint(7, (debugfile, "    addrbook nickname = %s filename = %s",
	   pab->nickname ? pab->nickname : "<null>",
	   pab->filename ? pab->filename : "<null>"));
    dprint(7, (debugfile, "  ostatus was %s, want %s\n",
	   pab->ostatus==Open ? "Open" :
	     pab->ostatus==HalfOpen ? "HalfOpen" :
	       pab->ostatus==ThreeQuartOpen ? "ThreeQuartOpen" :
		 pab->ostatus==NoDisplay ? "NoDisplay" :
		   pab->ostatus==Closed ? "Closed" : "TotallyClosed",
	   want_status==Open ? "Open" :
	     want_status==HalfOpen ? "HalfOpen" :
	       want_status==ThreeQuartOpen ? "ThreeQuartOpen" :
		 want_status==NoDisplay ? "NoDisplay" :
		   want_status==Closed ? "Closed" : "TotallyClosed"));

    new_status = want_status;  /* optimistic default */

    if(want_status == TotallyClosed){
	if(pab->address_book != NULL){
	    adrbk_close(pab->address_book);
	    pab->address_book = NULL;
	}

	if(pab->so != NULL){
	    so_give(&pab->so);
	    pab->so = NULL;
	}
    }
    /*
     * If we don't need it, release some addrbook memory by calling
     * adrbk_partial_close().
     */
    else if((want_status == Closed || want_status == HalfOpen) &&
	pab->address_book != NULL){
	adrbk_partial_close(pab->address_book);
    }
    /* If we want the addrbook read in and it hasn't been, do so */
    else if(want_status == Open || want_status == NoDisplay){
      if(pab->address_book == NULL){  /* abook handle is not currently active */
	if(pab->access != NoAccess){
	    char warning[800]; /* place to put a warning */
	    int sort_rule;
	    int force_not_valid = 0;

	    warning[0] = '\0';
	    if(pab->access == ReadOnly)
	      sort_rule = AB_SORT_RULE_NONE;
	    else
	      sort_rule = ps_global->ab_sort_rule;

	    if(trouble_filename){
		if(pab->filename)
		  force_not_valid = 
			       strcmp(trouble_filename, pab->filename) == 0;
		else
		  force_not_valid = *trouble_filename == '\0';

		if(force_not_valid){
#ifdef notdef
		    /*
		     * I go back and forth on whether or not this should
		     * be here.  If the sys admin screws up and copies the
		     * wrong .lu file into place where a large global
		     * addressbook is, do we want it to rebuild for all
		     * the users or not?  With this commented out we're
		     * rebuilding for everybody, just like we would for
		     * a personal addressbook.  Most likely that will mean
		     * that it gets rebuilt in /tmp for each current user.
		     * That's what I'm going with for now.
		     */
		    if(pab->type & GLOBAL)
		      force_not_valid = 0;
#endif /* notdef */
		    fs_give((void **)&trouble_filename);
		}
	    }

	    pab->address_book = adrbk_open(pab,
		   ps_global->home_dir, warning, sort_rule, 0, force_not_valid);

	    if(pab->address_book == NULL){
		pab->access = NoAccess;
		if(want_status == Open){
		    new_status = HalfOpen;  /* best we can do */
		    q_status_message1(SM_ORDER | SM_DING, *warning?1:3, 4,
				      "Error opening/creating address book %.200s",
				      pab->nickname);
		    if(*warning)
			q_status_message2(SM_ORDER, 3, 4, "%.200s: %.200s",
			    as.n_addrbk > 1 ? pab->nickname : "addressbook",
			    warning);
		}
		else
		    new_status  = Closed;

		dprint(1, (debugfile, "Error opening address book %s: %s\n",
			  pab->nickname ? pab->nickname : "?",
			  error_description(errno)));
	    }
	    else{
		if(pab->access == NoExists)
		  pab->access = ReadWrite;

		/* 200 is out of the blue */
		(void)adrbk_set_nominal_cachesize(pab->address_book,
		    min((long)adrbk_count(pab->address_book), 200L));

		if(pab->access == ReadWrite){
#if !(defined(DOS) && !defined(_WINDOWS))
		    long old_size;
#endif /* !DOS */

		    /*
		     * Add forced entries if there are any.  These are
		     * entries that are always supposed to show up in
		     * personal address books.  They're specified in the
		     * global config file.
		     */
		    add_forced_entries(pab->address_book);

		    /*
		     * For huge addrbooks, it really pays if you can make
		     * them read-only so that you skip adrbk_is_in_sort_order.
		     *
		     * It is possible to get into a sorting contest with
		     * another pine. For example, if the other pine has
		     * a different sort rule defined or if the other pine
		     * is being run in a locale with a different collating
		     * rule, then the two pines will not agree on the
		     * correct sorting order. One will sort the addrbook,
		     * the other will eventually discover that the addrbook
		     * changed so will re-read it and will then discover that
		     * it is sorted incorrect from its point of view, so will
		     * sort it and save it back. Then it happens in the other
		     * direction and so on and so on. We should not usually
		     * have to sort the address book. We do it once when we
		     * start to fix up any problems and then don't do it
		     * anymore after that (unless 3 hours has passed).
		     */
		    if(!(pab->when_we_sorted &&
			 (time((time_t *)0) - pab->when_we_sorted)
						 < SORTING_LOOP_AVOID_TIMEOUT)
			 &&
		       !adrbk_is_in_sort_order(pab->address_book, 0, 0)){
			/* remember when we sorted */
			pab->when_we_sorted = time((time_t *) 0);
/* DOS sorts will be very slow on large addrbooks */
#if !(defined(DOS) && !defined(_WINDOWS))
			old_size =
			    adrbk_set_nominal_cachesize(pab->address_book,
			    (long)adrbk_count(pab->address_book));
#endif /* !DOS */
			(void)adrbk_sort(pab->address_book,
			    (a_c_arg_t)0, (adrbk_cntr_t *)NULL, 0);
#if !(defined(DOS) && !defined(_WINDOWS))
			(void)adrbk_set_nominal_cachesize(pab->address_book,
			    old_size);
#endif /* !DOS */
		    }
		}

		new_status = want_status;
		dprint(2,
		      (debugfile,
		      "Address book %s (%s) opened with %ld items\n",
		       pab->nickname ? pab->nickname : "?",
		       pab->filename ? pab->filename : "?",
		       (long)adrbk_count(pab->address_book)));
		if(*warning){
		    dprint(1, (debugfile,
				 "Addressbook parse error in %s (%s): %s\n",
				 pab->nickname ? pab->nickname : "?",
				 pab->filename ? pab->filename : "?",
				 warning));
		    if(!pab->gave_parse_warnings && want_status == Open){
			pab->gave_parse_warnings++;
			q_status_message2(SM_ORDER, 3, 4, "%.200s: %.200s",
			    as.n_addrbk > 1 ? pab->nickname : "addressbook",
			    warning);
		    }
		}
	    }
	}
	else{
	    if(want_status == Open){
		new_status = HalfOpen;  /* best we can do */
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
		   "Insufficient permissions for opening address book %.200s",
		   pab->nickname);
	    }
	    else
	      new_status = Closed;
	}
      }
      /*
       * File handle was already open but we were in Closed or HalfOpen
       * state. Check to see if someone else has changed the addrbook
       * since we first opened it.
       */
      else if((pab->ostatus == Closed || pab->ostatus == HalfOpen) &&
	      ps_global->remote_abook_validity > 0)
	(void)adrbk_check_and_fix(pab, 1, 0, 0);
    }

    pab->ostatus = new_status;
}


/*
 * Does a validity check on all the open address books.
 *
 * Return -- number of address books with invalid data
 */
int
adrbk_check_all_validity_now()
{
    int          i;
    int          something_out_of_date = 0;
    PerAddrBook *pab;

    dprint(7, (debugfile, "- adrbk_check_all_validity_now -\n"));

    if(as.initialized){
	for(i = 0; i < as.n_addrbk; i++){
	    pab = &as.adrbks[i];
	    if(pab->address_book){
		adrbk_check_validity(pab->address_book, 1L);
		if(pab->address_book->flags & FILE_OUTOFDATE ||
		   (pab->address_book->rd &&
		    pab->address_book->rd->flags & REM_OUTOFDATE))
		  something_out_of_date++;
	    }
	}
    }

    return(something_out_of_date);
}


/*
 * Fix out-of-date address book. This only fixes the AdrBk part of the
 * problem, not the pab and display. It closes and reopens the address_book
 * file, clears the cache, reads new data.
 *
 * Arg        pab -- Pointer to the PerAddrBook data for this addrbook
 *           safe -- It is safe to apply the fix
 *       low_freq -- This is a low frequency check (longer between checks)
 *
 * Returns non-zero if addrbook was fixed
 */
int
adrbk_check_and_fix(pab, safe, low_freq, check_now)
    PerAddrBook *pab;
    int          safe;
    int          low_freq;
    int          check_now;
{
    int  ret = 0;
    long chk_interval;

    if(!as.initialized || !pab)
      return(ret);

    dprint(7, (debugfile, "- adrbk_check_and_fix(%s) -\n",
	   pab->filename ? pab->filename : "?"));

    if(pab->address_book){
	if(check_now)
	  chk_interval = 1L;
	else if(low_freq)
	  chk_interval = -60L * max(LOW_FREQ_CHK_INTERVAL,
				    ps_global->remote_abook_validity + 5);
	else
	  chk_interval = 0L;

	adrbk_check_validity(pab->address_book, chk_interval);

	if(pab->address_book->flags & FILE_OUTOFDATE ||
	   (pab->address_book->rd &&
	    pab->address_book->rd->flags & REM_OUTOFDATE &&
	    !(pab->address_book->rd->flags & USER_SAID_NO))){
	    if(safe){
		OpenStatus save_status;
		int        save_rem_abook_valid = 0;

		dprint(2, (debugfile, "adrbk_check_and_fix %s: fixing %s\n",
		       debug_time(0,0),
		       pab->filename ? pab->filename : "?"));
		if(ab_nesting_level > 0){
		    q_status_message3(SM_ORDER, 0, 2,
				     "Resyncing address book%.200s%.200s%.200s",
				     as.n_addrbk > 1 ? " \"" : "",
				     as.n_addrbk > 1 ? pab->nickname : "",
				     as.n_addrbk > 1 ? "\"" : "");
		    display_message('x');
		    fflush(stdout);
		}

		ret++;
		save_status = pab->ostatus;

		/* don't do the trim right now */
		if(pab->address_book->rd)
		  pab->address_book->rd->flags &= ~DO_REMTRIM;

		/*
		 * Have to change this from -1 or we won't actually do
		 * the resync.
		 */
		if((pab->address_book->rd &&
		    pab->address_book->rd->flags & REM_OUTOFDATE) &&
		   ps_global->remote_abook_validity == -1){
		    save_rem_abook_valid = -1;
		    ps_global->remote_abook_validity = 0;
		}

		init_abook(pab, TotallyClosed);

		pab->so = NULL;
		/* this sets up pab->so, so is important */
		pab->access = adrbk_access(pab);

		/*
		 * If we just re-init to HalfOpen... we won't actually
		 * open the address book, which was open before. That
		 * would be fine but it's a little nicer if we can open
		 * it now so that we don't defer the resync until
		 * the next open, which would be a user action for sure.
		 * Right now we may be here during a newmail check
		 * timeout, so this is a good time to do the resync.
		 */
		if(save_status == HalfOpen ||
		   save_status == ThreeQuartOpen ||
		   save_status == Closed)
		  init_abook(pab, NoDisplay);

		init_abook(pab, save_status);

		if(save_rem_abook_valid)
		  ps_global->remote_abook_validity = save_rem_abook_valid;

		if(ab_nesting_level > 0 && pab->ostatus == save_status)
		    q_status_message3(SM_ORDER, 0, 2,
				     "Resynced address book%.200s%.200s%.200s",
				     as.n_addrbk > 1 ? " \"" : "",
				     as.n_addrbk > 1 ? pab->nickname : "",
				     as.n_addrbk > 1 ? "\"" : "");
	    }
	    else
	      dprint(2, (debugfile,
		     "adrbk_check_and_fix: not safe to fix %s\n",
		     pab->filename ? pab->filename : "?"));
	}
    }

    return(ret);
}


static time_t last_check_and_fix_all;
/*
 * Fix out of date address books. This only fixes the AdrBk part of the
 * problem, not the pab and display. It closes and reopens the address_book
 * files, clears the caches, reads new data.
 *
 * Args      safe -- It is safe to apply the fix
 *       low_freq -- This is a low frequency check (longer between checks)
 *
 * Returns non-zero if an addrbook was fixed
 */
int
adrbk_check_and_fix_all(safe, low_freq, check_now)
    int safe;
    int low_freq;
    int check_now;
{
    int          i, ret = 0;
    PerAddrBook *pab;

    if(!as.initialized)
      return(ret);

    dprint(7, (debugfile, "- adrbk_check_and_fix_all -\n"));

    last_check_and_fix_all = get_adj_time();

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->address_book)
	  ret += adrbk_check_and_fix(pab, safe, low_freq, check_now);
    }

    return(ret);
}


/*
 *
 */
void
adrbk_maintenance()
{
    static time_t last_time_here = 0;
    time_t        now;
    int           i;
    long          low_freq_interval;

    dprint(9, (debugfile, "- adrbk_maintenance -\n"));

    if(!as.initialized)
      return;
    
    now = get_adj_time();

    if(now < last_time_here + 120)
      return;

    last_time_here = now;

    low_freq_interval = max(LOW_FREQ_CHK_INTERVAL,
			    ps_global->remote_abook_validity + 5);

    /* check the time here to make it cheap */
    if(ab_nesting_level == 0 &&
       ps_global->remote_abook_validity > 0 &&
       now > last_check_and_fix_all + low_freq_interval * 60L)
      (void)adrbk_check_and_fix_all(1, 1, 0);

    /* close down idle connections */
    for(i = 0; i < as.n_addrbk; i++){
	PerAddrBook *pab;

	pab = &as.adrbks[i];

	if(pab->address_book &&
	   pab->address_book->type == Imap &&
	   pab->address_book->rd &&
	   rd_stream_exists(pab->address_book->rd)){
	    dprint(7, (debugfile,
		   "adrbk_maint: %s: idle cntr %ld (%ld)\n",
		   pab->address_book->orig_filename
		     ? pab->address_book->orig_filename : "?",
		   (long)(now - pab->address_book->rd->last_use),
		   (long)pab->address_book->rd->last_use));

	    if(now > pab->address_book->rd->last_use + IMAP_IDLE_TIMEOUT){
		dprint(2, (debugfile,
		    "adrbk_maint %s: closing idle (%ld secs) connection: %s\n",
		    debug_time(0,0),
		    (long)(now - pab->address_book->rd->last_use),
		    pab->address_book->orig_filename
		      ? pab->address_book->orig_filename : "?"));
		rd_close_remote(pab->address_book->rd);
	    }
	    else{
		/*
		 * If we aren't going to close it, we ping it instead to
		 * make sure it stays open and doesn't timeout on us.
		 * This shouldn't be necessary unless the server has a
		 * really short timeout. If we got killed for some reason
		 * we set imap.stream to NULL.
		 * Instead of just pinging, we may as well check for
		 * updates, too.
		 */
		if(ab_nesting_level == 0 &&
		   ps_global->remote_abook_validity > 0){
		    time_t save_last_use;

		    /*
		     * We shouldn't count this as a real last_use.
		     */
		    save_last_use = pab->address_book->rd->last_use;
		    (void)adrbk_check_and_fix(pab, 1, 0, 1);
		    pab->address_book->rd->last_use = save_last_use;
		}
		/* just ping it if not safe to fix it */
		else if(!rd_ping_stream(pab->address_book->rd)){
		    dprint(2, (debugfile,
		      "adrbk_maint: %s: abook stream closed unexpectedly: %s\n",
		      debug_time(0,0),
		      pab->address_book->orig_filename
		        ? pab->address_book->orig_filename : "?"));
		}
	    }
	}
    }
}


/*
 * 1522 encode the personal name portion of addr and return an allocated
 * copy of the resulting address string.
 */
char *
encode_fullname_of_addrstring(addr, charset)
    char *addr;
    char *charset;
{
    char    *pers_encoded,
            *tmp_a_string,
            *ret = NULL;
    ADDRESS *adr;
    static char *fakedomain = "@";

    tmp_a_string = cpystr(addr ? addr : "");
    adr = NULL;
    rfc822_parse_adrlist(&adr, tmp_a_string, fakedomain);
    fs_give((void **)&tmp_a_string);

    if(!adr)
      return(cpystr(""));

    if(adr->personal && adr->personal[0]){
	pers_encoded = cpystr(rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
				(unsigned char *)adr->personal,
				charset));
	fs_give((void **)&adr->personal);
	adr->personal = pers_encoded;
    }

    ret = (char *)fs_get((size_t)est_size(adr));
    ret[0] = '\0';
    rfc822_write_address(ret, adr);
    mail_free_address(&adr);
    return(ret);
}


/*
 * 1522 decode the personal name portion of addr and return an allocated
 * copy of the resulting address string.
 */
char *
decode_fullname_of_addrstring(addr, verbose)
    char *addr;
    int   verbose;
{
    char    *pers_decoded,
            *tmp_a_string,
            *ret = NULL,
	    *dummy = NULL;
    ADDRESS *adr;
    static char *fakedomain = "@";

    tmp_a_string = cpystr(addr ? addr : "");
    adr = NULL;
    rfc822_parse_adrlist(&adr, tmp_a_string, fakedomain);
    fs_give((void **)&tmp_a_string);

    if(!adr)
      return(cpystr(""));

    if(adr->personal && adr->personal[0]){
	pers_decoded
	    = cpystr((char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					    SIZEOF_20KBUF,
					    adr->personal,
					    verbose ? NULL : &dummy));
	fs_give((void **)&adr->personal);
	adr->personal = pers_decoded;
	if(dummy)
	  fs_give((void **)&dummy);
    }

    ret = (char *)fs_get((size_t)est_size(adr));
    ret[0] = '\0';
    rfc822_write_address(ret, adr);
    mail_free_address(&adr);
    return(ret);
}


/*
 *  Interface to address book lookups for callers outside or inside this file.
 *
 * Args: nickname       -- The nickname to look up
 *       which_addrbook -- If matched, addrbook number it was found in.
 *       not_here       -- If non-negative, skip looking in this abook.
 *       found_abe      -- If this is non-null, it will be set to point to 
 *                         a copy of the actual abe that was found on return.
 *                         The caller must free the memory.
 *
 * Result: returns NULL or the corresponding fullname.  The fullname is
 * allocated here so the caller must free it.
 *
 * This opens the address books if they haven't been opened and restores
 * them to the state they were in upon entry.
 */
char *
addr_lookup(nickname, which_addrbook, not_here, found_abe)
    char *nickname;
    int  *which_addrbook;
    int   not_here;
    AdrBk_Entry **found_abe;
{
    AdrBk_Entry  *abe;
    SAVE_STATE_S  state;
    char         *fullname;

    dprint(9, (debugfile, "- addr_lookup(%s) -\n",nickname ? nickname : "nil"));

    init_ab_if_needed();
    save_state(&state);

    abe = adrbk_lookup_with_opens_by_nick(nickname,0,which_addrbook,not_here);

    /* copy instead of just point because of the restore_state below */
    if(found_abe)
      *found_abe = copy_ae(abe);

    fullname = (abe && abe->fullname) ? cpystr(abe->fullname) : NULL;

    restore_state(&state);

    return(fullname);
}


/*
 * This is like build_address() only it doesn't close
 * everything down when it is done, and it doesn't open addrbooks that
 * are already open.  Other than that, it has the same functionality.
 * It opens addrbooks that haven't been opened and saves and restores the
 * addrbooks open states (if save_and_restore is set).
 *
 * Args: to                    -- the address to attempt expanding (see the
 *				   description in expand_address)
 *       full_to               -- a pointer to result
 *		    This will be allocated here and freed by the caller.
 *       error                 -- a pointer to an error message, if non-null
 *       fcc                   -- a pointer to returned fcc, if non-null
 *		    This will be allocated here and freed by the caller.
 *		    *fcc should be null on entry.
 *       save_and_restore      -- restore addrbook states when finished
 *
 * Results:    0 -- address is ok
 *            -1 -- address is not ok
 * full_to contains the expanded address on success, or a copy of to
 *         on failure
 * *error  will point to an error message on failure it it is non-null
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
our_build_address(to, full_to, error, fcc, save_and_restore)
    BuildTo to;
    char  **full_to,
	  **error,
	  **fcc;
    int     save_and_restore;
{
    int ret;

    dprint(7, (debugfile, "- our_build_address -  (%s)\n",
	(to.type == Str) ? (to.arg.str ? to.arg.str : "nul")
			 : (to.arg.abe->nickname ? to.arg.abe->nickname
						: "no nick")));

    if(to.type == Str && !to.arg.str || to.type == Abe && !to.arg.abe){
	if(full_to)
	  *full_to = cpystr("");
	ret = 0;
    }
    else
      ret = build_address_internal(to, full_to, error, fcc, NULL, NULL,
			       save_and_restore, 0, NULL);

    dprint(8, (debugfile, "   our_build_address says %s address\n",
	ret ? "BAD" : "GOOD"));

    return(ret);
}


/*
 * This is the builder used by the composer for the Lcc line.
 *
 * Args:     lcc -- the passed in Lcc line to parse
 *      full_lcc -- Address of a pointer to return the full address in.
 *		    This will be allocated here and freed by the caller.
 *         error -- Address of a pointer to return an error message in.
 *                  This is not allocated so should not be freed by the caller.
 *          barg -- This is a pointer to text for affected entries which
 *		    we may be changing.  The first one in the list is the
 *		    To entry.  We may put the name of the list in empty
 *		    group syntax form there (like  List Name: ;).
 *		    The second one in the list is the fcc field.
 *		    The tptr members already point to text allocated in the
 *		    caller. We may free and reallocate here, caller will
 *		    free the result in any case.
 *
 * Result:  0 is returned if address was OK, 
 *         -1 if address wasn't OK.
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
build_addr_lcc(lcc, full_lcc, error, barg, mangled)
    char	 *lcc,
		**full_lcc,
		**error;
    BUILDER_ARG	 *barg;
    int          *mangled;
{
    int		     ret_val,
		     no_repo = 0;	/* fcc or lcc not reproducible */
    int		    *save_nesting_level;
    BuildTo	     bldlcc;
    PrivateTop      *pt = NULL;
    PrivateAffector *af = NULL;
    char	    *p,
		    *fcc_local = NULL,
		    *to = NULL,
		    *dummy;
    jmp_buf          save_jmp_buf;

    dprint(5, (debugfile, "- build_addr_lcc - (%s)\n", lcc ? lcc : "nul"));

    /* check to see if to string is empty to avoid work */
    for(p = lcc; p && *p && isspace((unsigned char)*p); p++)
      ;/* do nothing */

    if(!p || !*p){
	if(full_lcc)
	  *full_lcc = cpystr(lcc ? lcc : ""); /* because pico does a strcmp() */

	return 0;
    }

    if(error != NULL)
      *error = (char *)NULL;

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0) && mangled)
      *mangled = 1;

    /*
     * If we end up jumping back here because somebody else changed one of
     * our addrbooks out from underneath us, we may well leak some memory.
     * That's probably ok since this will be very rare.
     */
    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	no_repo = 0;
	pt = NULL;
	af = NULL;
	fcc_local = NULL;
	to = NULL;
	if(error != NULL)
	  *error = (char *)NULL;

	if(full_lcc && *full_lcc)
	  fs_give((void **)full_lcc);

	q_status_message(SM_ORDER, 3, 5, "Resetting address book...");
	dprint(1, (debugfile,
	    "RESETTING address book... build_address(%s)!\n", lcc ? lcc : "?"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    bldlcc.type    = Str;
    bldlcc.arg.str = lcc;

    /*
     * To is first affected_entry and Fcc is second.
     * The conditional stuff for the fcc argument says to only change the
     * fcc if the fcc pointer is passed in non-null, and the To pointer
     * is also non-null.  If they are null, that means they've already been
     * entered (are sticky).  We don't affect fcc if either fcc or To has
     * been typed in.
     */
    ret_val = build_address_internal(bldlcc,
		full_lcc,
		error,
		(barg && barg->next && barg->next->tptr
		   && barg->tptr) ? &fcc_local : NULL,
		&no_repo,
		(barg && barg->tptr) ? &to : NULL,
		1, 0, mangled);

    ab_nesting_level--;
    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    /* full_lcc is what ends up in the Lcc: line */
    if(full_lcc && *full_lcc){
	size_t len;

	/*
	 * Have to rfc1522_decode the full_lcc string before sending it back.
	 */
	len = strlen(*full_lcc)+1;
	p = (char *)fs_get(len * sizeof(char));
	dummy = NULL;
	if(rfc1522_decode((unsigned char *)p, len, *full_lcc, &dummy)
						== (unsigned char *)p){
	    fs_give((void **)full_lcc);
	    *full_lcc = p;
	}
	else
	  fs_give((void **)&p);

	if(dummy)
	  fs_give((void **)&dummy);
    }

    /* to is what ends up in the To: line */
    if(to && *to){
	unsigned long csum;
	size_t len;

	/*
	 * Have to rfc1522_decode the full_to string before sending it back.
	 */
	len = strlen(to)+1;
	p = (char *)fs_get(len * sizeof(char));
	dummy = NULL;
	if(rfc1522_decode((unsigned char *)p, len, to, &dummy)
						== (unsigned char *)p){
	    /*
	     * If the caller wants us to try to preserve the charset
	     * information (they set aff) we copy it into encoded->etext.
	     * We don't have to worry about pasting together pieces of
	     * etext like we do in build_address because whenever the
	     * Lcc line is setting the To line it will be setting the
	     * whole line, not modifying it.
	     * Pt will point to headents[To].bldr_private.
	     */
	    if(barg && barg->aff){
		pt = (PrivateTop *)(*barg->aff);

		if(pt && pt->encoded && pt->encoded->etext)
		  fs_give((void **)&pt->encoded->etext);

		if(!pt){
		    *barg->aff = (void *)fs_get(sizeof(PrivateTop));
		    pt = (PrivateTop *)(*barg->aff);
		    memset((void *)pt, 0, sizeof(PrivateTop));
		}

		if(!pt->encoded)
		  pt->encoded =
		       (PrivateEncoded *)fs_get(sizeof(PrivateEncoded));
		    
		pt->encoded->etext    = to;
		pt->encoded->cksumlen = strlen(p);
		pt->encoded->cksumval = line_hash(p);
	    }
	    else
	      fs_give((void **)&to);
	    
	    to = p;
	}
	else
	  fs_give((void **)&p);

	if(dummy)
	  fs_give((void **)&dummy);


	/*
	 * This part is recording the fact that the To line was set to
	 * what it is by entering something on the Lcc line. In particular,
	 * if a list alias was entered here then the fullname of the list
	 * goes in the To line. We save this affector information so that
	 * we can tell it shouldn't be modified if we call build_addr_lcc
	 * again unless we actually modified what's in the Lcc line so that
	 * it doesn't start with the same thing. The problem we're solving
	 * is that the contents of the Lcc line no longer look like the
	 * list they were derived from.
	 * Pt will point to headents[To].bldr_private.
	 */
	if(barg && barg->aff)
	  pt = (PrivateTop *)(*barg->aff);

	if(pt && (af=pt->affector) && af->who == BP_Lcc){
	    int len;

	    len = strlen(lcc);
	    if(len >= af->cksumlen){
		int save;

		save = lcc[af->cksumlen];
		lcc[af->cksumlen] = '\0';
		csum = line_hash(lcc);
		lcc[af->cksumlen] = save;
	    }
	    else
	      csum = af->cksumval + 1;		/* so they aren't equal */
	}

	if(!pt ||
	   !pt->affector ||
	   pt->affector->who != BP_Lcc ||
	   (pt->affector->who == BP_Lcc && csum != pt->affector->cksumval)){

	    /* replace to value */
	    if(barg->tptr)
	      fs_give((void **)&barg->tptr);
	    
	    barg->tptr = to;

	    if(barg->aff){
		if(!pt){
		    *barg->aff = (void *)fs_get(sizeof(PrivateTop));
		    pt = (PrivateTop *)(*barg->aff);
		    memset((void *)pt, 0, sizeof(PrivateTop));
		}

		if(no_repo){
		    if(!pt->affector)
		      pt->affector =
			    (PrivateAffector *)fs_get(sizeof(PrivateAffector));
		    
		    af = pt->affector;
		    af->who = BP_Lcc;
		    af->cksumlen = strlen(((full_lcc && *full_lcc)
							    ? *full_lcc : ""));
		    af->cksumval = line_hash(((full_lcc && *full_lcc)
							    ? *full_lcc : ""));
		}
		else{
		    /*
		     * If result is reproducible, we don't keep track here.
		     */
		    if(pt->affector)
		      fs_give((void **)&pt->affector);
		}
	    }
	}
	else
	  fs_give((void **)&to);  /* unused in this case */
    }

    if(fcc_local){
	unsigned long csum;

	/*
	 * If *barg->next->aff is set, that means fcc was set from a list
	 * during some previous builder call. If the current Lcc line
	 * contains the old expansion as a prefix, then we should leave
	 * things as they are. In order to decide that we look at a hash
	 * value computed from the strings.
	 * Pt will point to headents[Fcc].bldr_private
	 */
	pt = NULL;
	if(barg && barg->next && barg->next->aff)
	  pt = (PrivateTop *)(*barg->next->aff);

	if(pt && (af=pt->affector) && af->who == BP_Lcc){
	    int len;

	    len = strlen(lcc);
	    if(len >= af->cksumlen){
		int save;

		save = lcc[af->cksumlen];
		lcc[af->cksumlen] = '\0';
		csum = line_hash(lcc);
		lcc[af->cksumlen] = save;
	    }
	    else
	      csum = af->cksumval + 1;  /* something not equal to cksumval */
	}

	if(!pt ||
	   !pt->affector ||
	   pt->affector->who != BP_Lcc ||
	   (pt->affector->who == BP_Lcc && csum != pt->affector->cksumval)){

	    /* replace fcc value */
	    if(barg->next->tptr)
	      fs_give((void **)&barg->next->tptr);

	    barg->next->tptr = fcc_local;

	    if(barg->next->aff){
		if(!pt){
		    *barg->next->aff = (void *)fs_get(sizeof(PrivateTop));
		    pt = (PrivateTop *)(*barg->next->aff);
		    memset((void *)pt, 0, sizeof(PrivateTop));
		}

		if(no_repo){
		    if(!pt->affector)
		      pt->affector =
			    (PrivateAffector *)fs_get(sizeof(PrivateAffector));
		    
		    af = pt->affector;
		    af->who = BP_Lcc;
		    af->cksumlen = strlen(((full_lcc && *full_lcc)
							    ? *full_lcc : ""));
		    af->cksumval = line_hash(((full_lcc && *full_lcc)
							    ? *full_lcc : ""));
		}
		else{
		    /*
		     * If result is reproducible, we don't keep track here.
		     */
		    if(pt->affector)
		      fs_give((void **)&pt->affector);
		}
	    }
	}
	else
	  fs_give((void **)&fcc_local);  /* unused in this case */
    }


    if(error != NULL && *error == NULL)
      *error = cpystr("");

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    flush_status_messages(0);
    return(ret_val);
}


/*
 * This is the build_address used by the composer to check for an address
 * in the addrbook.
 *
 * Args: to      -- the passed in line to parse
 *       full_to -- Address of a pointer to return the full address in.
 *		    This will be allocated here and freed by the caller.
 *       error   -- Address of a pointer to return an error message in.
 *		    This will be allocated here and freed by the caller.
 *        barg   -- Address of a pointer to return the fcc in is in
 *		    fcc->tptr.  It will have already been allocated by the
 *		    caller but we may free it and reallocate if we wish.
 *		    Caller will free it.
 *
 * Result:  0 is returned if address was OK, 
 *         -1 if address wasn't OK.
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
build_address(to, full_to, error, barg, mangled)
    char	 *to,
		**full_to,
		**error;
    BUILDER_ARG	 *barg;
    int          *mangled;
{
    char   *p;
    int     ret_val, no_repo = 0, *save_nesting_level;
    BuildTo bldto;
    PrivateTop *pt = NULL;
    PrivateAffector *af = NULL;
    char   *fcc_local = NULL, *dummy = NULL;
    jmp_buf save_jmp_buf;

    dprint(5, (debugfile, "- build_address - (%s)\n", to ? to : "nul"));

    /* check to see if to string is empty to avoid work */
    for(p = to; p && *p && isspace((unsigned char)*p); p++)
      ;/* do nothing */

    if(!p || !*p){
	if(full_to)
	  *full_to = cpystr(to ? to : "");  /* because pico does a strcmp() */

	/*
	 * no reason to save anything anymore,
	 * since no decoding happened
	 */
	if(barg && barg->me){
	    pt = (PrivateTop *)(*barg->me);
	    if(pt)
	      free_privateencoded(&pt->encoded);
	}

	return 0;
    }

    if(full_to != NULL)
      *full_to = (char *)NULL;

    if(error != NULL)
      *error = (char *)NULL;

    /* No guarantee cursor or status line is how we saved it */
    clear_cursor_pos();
    mark_status_unknown();

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0) && mangled)
      *mangled = 1;

    /*
     * If we end up jumping back here because somebody else changed one of
     * our addrbooks out from underneath us, we may well leak some memory.
     * That's probably ok since this will be very rare.
     *
     * The reason for the memcpy of the jmp_buf is that we may actually
     * be indirectly calling this function from within the address book.
     * For example, we may be in the address book screen and then run
     * the ComposeTo command which puts us in the composer, then we call
     * build_address from there which resets addrbook_changed_unexpectedly.
     * Once we leave build_address we need to reset addrbook_changed_un...
     * because this position on the stack will no longer be valid.
     * Same is true of the other setjmp's in this file which are wrapped
     * in memcpy calls.
     */
    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	no_repo = 0;
	pt = NULL;
	af = NULL;
	fcc_local = NULL;
	dummy = NULL;
	if(error != NULL)
	  *error = (char *)NULL;

	if(full_to && *full_to)
	  fs_give((void **)full_to);

	q_status_message(SM_ORDER, 3, 5, "Resetting address book...");
	dprint(1, (debugfile,
	    "RESETTING address book... build_address(%s)!\n", to ? to : "?"));
	addrbook_reset();
        ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    bldto.type    = Str;
    bldto.arg.str = to;
    ret_val = build_address_internal(bldto, full_to, error,
				     barg ? &fcc_local : NULL,
				     &no_repo, NULL, 1, 0, mangled);
    ab_nesting_level--;
    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    /*
     * Have to rfc1522_decode the full_to string before sending it back.
     */
    if(full_to && *full_to ){
	unsigned long csum_start, csum_end, csum_mid;
	char   *q;
	size_t  len;

	/* Pt will point to headents[To].bldr_private */
	pt = NULL;
	if(barg && barg->me)
	  pt = (PrivateTop *)(*barg->me);

	len = strlen(*full_to)+1;
	q = (char *)fs_get(len * sizeof(char));
	p = (char *)rfc1522_decode((unsigned char *)q, len, *full_to, &dummy);
	/* p == q means that decoding happened, p is decoded *full_to */
	if(p == q || (pt && pt->encoded && pt->encoded->etext)){
	    char  save;

	    /*
	     * We save a copy of the encoded string to preserve the charset
	     * information in it if possible.
	     */
	    if(pt && pt->encoded){
		int len;

		/* set to something not equal to cksumval */
		csum_start = csum_end = csum_mid = pt->encoded->cksumval + 1;

		/*
		 * Compare hash value computed from prefix and suffix of
		 * new string to see if it matches the hash value of the
		 * old string. If it does, that means we've just appended
		 * or prepended something new, so we can preserve the old
		 * charset info. Also check if it is just the old string
		 * with quotes around it.
		 */
		len = strlen(p);
		if(len >= pt->encoded->cksumlen){
		    char *begin;

		    save = p[pt->encoded->cksumlen];
		    p[pt->encoded->cksumlen] = '\0';
		    csum_start = line_hash(p);
		    p[pt->encoded->cksumlen] = save;

		    if(len > pt->encoded->cksumlen){
			begin = p + (len - pt->encoded->cksumlen);
			csum_end = line_hash(begin);
		    }
		    else
		      csum_end = csum_start;
		    
		    /*
		     * Maybe just added quotes?
		     * This is tough because the quotes are around the phrase
		     * not around the whole thing. So we're just taking a
		     * guess about that second quote. But if we guess right,
		     * we are right, and we can just leave the pt->encoded
		     * stuff as it is now.
		     */
		    if(len == pt->encoded->cksumlen+2 &&
		       p[0] == '"' &&
		       strchr(p+1, '"')){
			char *tmp, *quote, *r;

			quote = strchr(p+1, '"');
			*quote = '\0';
			tmp = (char *)fs_get((len-1) * sizeof(char));
			r = tmp;
			sstrcpy(&r, p+1);
			sstrcpy(&r, quote+1);
			*quote = '"';

			csum_mid = line_hash(tmp);
			fs_give((void **)&tmp);
		    }
		}
	    }

	    /* If the caller wanted us to do this at all */
	    if(barg && barg->me){
		if(!pt || !pt->encoded ||
	           (csum_start != pt->encoded->cksumval &&
		      csum_mid != pt->encoded->cksumval &&
		      csum_end != pt->encoded->cksumval)){

		    /* no match, save whole string */

		    if(pt && pt->encoded && pt->encoded->etext)
		      fs_give((void **)&pt->encoded->etext);

		    if(p == q){		/* decoded != encoded version */
			if(!pt){
			    *barg->me = (void *)fs_get(sizeof(PrivateTop));
			    pt = (PrivateTop *)(*barg->me);
			    memset((void *)pt, 0, sizeof(PrivateTop));
			}

			if(!pt->encoded)
			  pt->encoded =
			       (PrivateEncoded *)fs_get(sizeof(PrivateEncoded));
		    
			pt->encoded->etext    = *full_to;
			pt->encoded->cksumlen = strlen(p);
			pt->encoded->cksumval = line_hash(p);
		    }
		    else{
			/*
			 * no reason to save anything anymore,
			 * since no decoding happened
			 */
			if(pt)
			  free_privateencoded(&pt->encoded);
		    }
		}
		else{	/* got a match */
		  char *etext1, *etext2, *etext3, *r;
		  int   end_of_new;

		  if(csum_mid != pt->encoded->cksumval){ /* else, leave alone */

		    /* matched prefix */
		    if(csum_start == pt->encoded->cksumval){
			etext1 = "";
			etext2 = pt->encoded->etext;
			/*
			 * *full_to is now the unchanged decoded prefix
			 * of length cksumlen, plus the new good part
			 * that we want. We paste the old good etext
			 * together with the new tail end of *full_to.
			 */
			etext3 = *full_to + pt->encoded->cksumlen;
		    }
		    else{				    /* matched suffix */
			etext1 = *full_to;
			end_of_new = strlen(etext1) - pt->encoded->cksumlen;
			save   = etext1[end_of_new];
			etext1[end_of_new] = '\0';
			etext2 = pt->encoded->etext;
			etext3 = "";
		    }

		    pt->encoded->etext = fs_get(strlen(etext1) +
						strlen(etext2) +
						strlen(etext3) + 1);

		    r = pt->encoded->etext;
		    sstrcpy(&r, etext1);
		    sstrcpy(&r, etext2);
		    sstrcpy(&r, etext3);

		    if(etext2)
		      fs_give((void **)&etext2);

		    if(csum_start != pt->encoded->cksumval)
		      etext1[end_of_new] = save;

		    pt->encoded->cksumlen = strlen(p);
		    pt->encoded->cksumval = line_hash(p);
		  }
		}
	    }
	    else
	      fs_give((void **)full_to);
	    
	    *full_to = p;
	}

	if(p != q)
	  fs_give((void **)&q);

	if(dummy)
	  fs_give((void **)&dummy);
    }
    else{
	/*
	 * no reason to save anything anymore,
	 * since no decoding happened
	 */
	if(barg && barg->me){
	    pt = (PrivateTop *)(*barg->me);
	    if(pt)
	      free_privateencoded(&pt->encoded);
	}
    }

    if(fcc_local){
	unsigned long csum;

	/* Pt will point to headents[Fcc].bldr_private */
	pt = NULL;
	if(barg && barg->aff)
	  pt = (PrivateTop *)(*barg->aff);

	/*
	 * If *barg->aff is set, that means fcc was set from a list
	 * during some previous builder call.
	 * If the current To line contains the old expansion as a prefix, then
	 * we should leave things as they are. In order to decide that,
	 * we look at a hash value computed from the strings.
	 */
	if(pt && (af=pt->affector) && af->who == BP_To){
	    int len;

	    len = strlen(to);
	    if(len >= af->cksumlen){
		int save;

		save = to[af->cksumlen];
		to[af->cksumlen] = '\0';
		csum = line_hash(to);
		to[af->cksumlen] = save;
	    }
	    else
	      csum = af->cksumval + 1;  /* something not equal to cksumval */
	}

	if(!pt ||
	   !pt->affector ||
	   (pt->affector->who == BP_To && csum != pt->affector->cksumval)){

	    /* replace fcc value */
	    if(barg->tptr)
	      fs_give((void **)&barg->tptr);

	    barg->tptr = fcc_local;

	    if(barg->aff){
		if(!pt){
		    *barg->aff = (void *)fs_get(sizeof(PrivateTop));
		    pt = (PrivateTop *)(*barg->aff);
		    memset((void *)pt, 0, sizeof(PrivateTop));
		}

		if(no_repo){
		    if(!pt->affector)
		      pt->affector =
			    (PrivateAffector *)fs_get(sizeof(PrivateAffector));
		    
		    af = pt->affector;
		    af->who = BP_To;
		    af->cksumlen = strlen(((full_to && *full_to)
							    ? *full_to : ""));
		    af->cksumval = line_hash(((full_to && *full_to)
							    ? *full_to : ""));
		}
		else{
		    /*
		     * If result is reproducible, we don't keep track here.
		     */
		    if(pt->affector)
		      fs_give((void **)&pt->affector);
		}
	    }
	}
	else
	  fs_give((void **)&fcc_local);  /* unused in this case */
    }

    /* This is so pico will erase the old message */
    if(error != NULL && *error == NULL)
      *error = cpystr("");

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    flush_status_messages(1);
    return(ret_val);
}


/*
 * Until we can think of a better way to do this. If user exits from an
 * ldap address selection screen we want to return -1 so that call_builder
 * will stay on the same line. Might want to use another return value
 * for the builder so that call_builder more fully understands what we're
 * up to.
 */
static int wp_exit;

#define FCC_SET    1	/* Fcc was set */
#define LCC_SET    2	/* Lcc was set */
#define FCC_NOREPO 4	/* Fcc was set in a non-reproducible way */
#define LCC_NOREPO 8	/* Lcc was set in a non-reproducible way */
/*
 * Given an address, expand it based on address books, local domain, etc.
 * This will open addrbooks if needed before checking (actually one of
 * its children will open them).
 *
 * Args: to       -- The given address to expand (see the description
 *			in expand_address)
 *       full_to  -- Returned value after parsing to.
 *       error    -- This gets pointed at error message, if any
 *       fcc      -- Returned value of fcc for first addr in to
 *       no_repo  -- Returned value, set to 1 if the fcc or lcc we're
 *			returning is not reproducible from the expanded
 *			address.  That is, if we were to run
 *			build_address_internal again on the resulting full_to,
 *			we wouldn't get back the fcc again.  For example,
 *			if we expand a list and use the list fcc from the
 *			addrbook, the full_to no longer contains the
 *			information that this was originally list foo.
 *       save_and_restore -- restore addrbook state when done
 *
 * Result:  0 is returned if address was OK, 
 *         -1 if address wasn't OK.
 * The address is expanded, fully-qualified, and personal name added.
 *
 * Input may have more than one address separated by commas.
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
build_address_internal(to, full_to, error, fcc, no_repo, lcc, save_and_restore,
		       simple_verify, mangled)
    BuildTo to;
    char  **full_to,
	  **error,
	  **fcc,
	  **lcc;
    int    *no_repo;
    int     save_and_restore, simple_verify;
    int    *mangled;
{
    ADDRESS *a;
    int      loop, i;
    int      tried_route_addr_hack = 0;
    int      did_set = 0;
    char    *tmp = NULL;
    SAVE_STATE_S  state;
    PerAddrBook  *pab;

    dprint(8, (debugfile, "- build_address_internal -  (%s)\n",
	(to.type == Str) ? (to.arg.str ? to.arg.str : "nul")
			 : (to.arg.abe->nickname ? to.arg.abe->nickname
						: "no nick")));

    init_ab_if_needed();
    if(save_and_restore)
      save_state(&state);

start:
    loop = 0;
    ps_global->c_client_error[0] = '\0';
    wp_exit = 0;

    a = expand_address(to, ps_global->maildomain,
		       F_OFF(F_QUELL_LOCAL_LOOKUP, ps_global)
			 ? ps_global->maildomain : NULL,
		       &loop, fcc, &did_set, lcc, error,
		       0, simple_verify, mangled);

    /*
     * If the address is a route-addr, expand_address() will have rejected
     * it unless it was enclosed in brackets, since route-addrs can't stand
     * alone.  Try it again with brackets.  We should really be checking
     * each address in the list of addresses instead of assuming there is
     * only one address, but we don't want to have this function know
     * all about parsing rfc822 addrs.
     */
    if(!tried_route_addr_hack &&
        ps_global->c_client_error[0] != '\0' &&
	((to.type == Str && to.arg.str && to.arg.str[0] == '@') ||
	 (to.type == Abe && to.arg.abe->tag == Single &&
				 to.arg.abe->addr.addr[0] == '@'))){
	BuildTo      bldto;

	tried_route_addr_hack++;

	tmp = (char *)fs_get((size_t)(MAX_ADDR_FIELD + 3));

	/* add brackets to whole thing */
	if(to.type == Str)
	  strcat(strncat(strcpy(tmp, "<"), to.arg.str, MAX_ADDR_FIELD), ">");
	else
	  strcat(strncat(strcpy(tmp, "<"), to.arg.abe->addr.addr, MAX_ADDR_FIELD), ">");

	loop = 0;
	ps_global->c_client_error[0] = '\0';

	bldto.type    = Str;
	bldto.arg.str = tmp;

	if(a)
	  mail_free_address(&a);

	/* try it */
	a = expand_address(bldto, ps_global->maildomain,
			   F_OFF(F_QUELL_LOCAL_LOOKUP, ps_global)
			     ? ps_global->maildomain : NULL,
		       &loop, fcc, &did_set, lcc, error,
		       0, simple_verify, mangled);

	/* if no error this time, use it */
	if(ps_global->c_client_error[0] == '\0'){
	    if(save_and_restore)
	      restore_state(&state);

	    /*
	     * Clear references so that Addrbook Entry caching in adrbklib.c
	     * is allowed to throw them out of cache.
	     */
	    for(i = 0; i < as.n_addrbk; i++){
		pab = &as.adrbks[i];
		if(pab->ostatus == Open || pab->ostatus == NoDisplay)
		  adrbk_clearrefs(pab->address_book);
	    }

	    goto ok;
	}
	else{  /* go back and use what we had before, so we get the error */
	    if(tmp)
	      fs_give((void **)&tmp);

	    tmp = NULL;
	    goto start;
	}
    }

    if(save_and_restore)
      restore_state(&state);

    /*
     * Clear references so that Addrbook Entry caching in adrbklib.c
     * is allowed to throw them out of cache.
     */
    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->ostatus == Open || pab->ostatus == NoDisplay)
	  adrbk_clearrefs(pab->address_book);
    }

    if(ps_global->c_client_error[0] != '\0'){
        /* Parse error.  Return string as is and error message */
	if(full_to){
	    if(to.type == Str)
	      *full_to = cpystr(to.arg.str);
	    else{
	        if(to.arg.abe->nickname && to.arg.abe->nickname[0])
	          *full_to = cpystr(to.arg.abe->nickname);
	        else if(to.arg.abe->tag == Single)
	          *full_to = cpystr(to.arg.abe->addr.addr);
	        else
	          *full_to = cpystr("");
	    }
	}

	if(error != NULL){
	    /* display previous error and add new one */
	    if(*error){
		q_status_message(SM_ORDER, 3, 5, *error);
		display_message('x');
		fs_give((void **)error);
	    }

            *error = cpystr(ps_global->c_client_error);
	}

        dprint(2, (debugfile,
	    "build_address_internal returning parse error: %s\n",
                  ps_global->c_client_error ? ps_global->c_client_error : "?"));
	if(a)
	  mail_free_address(&a);

	if(tmp)
	  fs_give((void **)&tmp);

        return -1;
    }

    /*
     * If there's a loop in the addressbook, we modify the address and
     * send an error back, but we still return 0.
     */
ok:
    if(loop && error != NULL){
	/* display previous error and add new one */
	if(*error){
	    q_status_message(SM_ORDER, 3, 5, *error);
	    display_message('x');
	    fs_give((void **)error);
	}

	*error = cpystr("Loop or Duplicate detected in addressbook!");
    }


    if(full_to){
	if(simple_verify){
	    if(tmp){
		*full_to = tmp;  /* add the brackets to route addr */
		tmp = NULL;
	    }
	    else{
		/* try to return what they sent us */
		if(to.type == Str)
		  *full_to = cpystr(to.arg.str);
		else{
		    if(to.arg.abe->nickname && to.arg.abe->nickname[0])
		      *full_to = cpystr(to.arg.abe->nickname);
		    else if(to.arg.abe->tag == Single)
		      *full_to = cpystr(to.arg.abe->addr.addr);
		    else
		      *full_to = cpystr("");
		}
	    }
	}
	else{
	    *full_to = (char *)fs_get((size_t)est_size(a));
	    (*full_to)[0] = '\0';
	    /*
	     * Assume that quotes surrounding the whole personal name are
	     * not meant to be literal quotes.  That is, the name
	     * "Joe College, PhD." is quoted so that we won't do the
	     * switcheroo of Last, First, not so that the quotes will be
	     * literal.  Rfc822_write_address will put the quotes back if they
	     * are needed, so Joe College would end up looking like
	     * "Joe College, PhD." <joe@somewhere.edu> but not like
	     * "\"Joe College, PhD.\"" <joe@somewhere.edu>.
	     */
	    strip_personal_quotes(a);
	    rfc822_write_address(*full_to, a);
	}
    }

    if(no_repo && (did_set & FCC_NOREPO || did_set & LCC_NOREPO))
      *no_repo = 1;

    /*
     * The condition in the leading if means that addressbook fcc's
     * override the fcc-rule (because did_set will be set).
     */
    if(fcc && !(did_set & FCC_SET)){
	char *fcc_got = NULL;

	if((ps_global->fcc_rule == FCC_RULE_LAST
	    || ps_global->fcc_rule == FCC_RULE_CURRENT)
	   && strcmp(fcc_got = get_fcc(NULL), ps_global->VAR_DEFAULT_FCC)){
	    if(*fcc)
	      fs_give((void **)fcc);
	    
	    *fcc = cpystr(fcc_got);
	}
	else if(a && a->host){ /* not group syntax */
	    if(*fcc)
	      fs_give((void **)fcc);

	    if(!tmp)
	      tmp = (char *)fs_get((size_t)200);

	    if((ps_global->fcc_rule == FCC_RULE_RECIP ||
		ps_global->fcc_rule == FCC_RULE_NICK_RECIP) &&
		  get_uname(a ? a->mailbox : NULL, tmp, 200))
	      *fcc = cpystr(tmp);
	    else
	      *fcc = cpystr(ps_global->VAR_DEFAULT_FCC);
	}
	else{ /* first addr is group syntax */
	    if(!*fcc)
	      *fcc = cpystr(ps_global->VAR_DEFAULT_FCC);
	    /* else, leave it alone */
	}

	if(fcc_got)
	  fs_give((void **)&fcc_got);
    }

    if(a)
      mail_free_address(&a);

    if(tmp)
      fs_give((void **)&tmp);

    if(wp_exit)
      return -1;

    return 0;
}


/*
 * Expand an address string against the address books, local names, and domain.
 *
 * Args:  to      -- this is either an address string to parse (one or more
 *		      address strings separated by commas) or it is an
 *		      AdrBk_Entry, in which case it refers to a single addrbook
 *		      entry.  If it is an abe, then it is treated the same as
 *		      if the nickname of this entry was passed in and we
 *		      looked it up in the addrbook, except that it doesn't
 *		      actually have to have a non-null nickname.
 *     userdomain -- domain the user is in
 *    localdomain -- domain of the password file (usually same as userdomain)
 *  loop_detected -- pointer to an int we set if we detect a loop in the
 *		     address books (or a duplicate in a list)
 *       fcc      -- Returned value of fcc for first addr in a_string
 *       did_set  -- expand_address set the fcc (need this in case somebody
 *                     sets fcc explicitly to a value equal to default-fcc)
 *  simple_verify -- don't add list full names or expand 2nd level lists
 *
 * Result: An adrlist of expanded addresses is returned
 *
 * If the localdomain is NULL, then no lookup against the password file will
 * be done.
 */
ADDRESS *
expand_address(to, userdomain, localdomain, loop_detected, fcc, did_set,
    lcc, error, recursing, simple_verify, mangled)
    BuildTo  to;
    char    *userdomain,
	    *localdomain;
    int     *loop_detected;
    char   **fcc;
    int     *did_set;
    char   **lcc;
    char   **error;
    int      recursing, simple_verify;
    int     *mangled;
{
    size_t       domain_length, length;
    ADDRESS     *adr, *a, *a_tail, *adr2, *a2, *a_temp, *wp_a;
    AdrBk_Entry *abe, *abe2;
    char        *list, *l1, **l2;
    char        *tmp_a_string, *q;
    BuildTo      bldto;
    static char *fakedomain;

    dprint(9, (debugfile, "- expand_address -  (%s)\n",
	(to.type == Str) ? (to.arg.str ? to.arg.str : "nul")
			 : (to.arg.abe->nickname ? to.arg.abe->nickname
						: "no nick")));

    /*
     * We use the domain "@" to detect an unqualified address.  If it comes
     * back from rfc822_parse_adrlist with the host part set to "@", then
     * we know it must have been unqualified (so we should look it up in the
     * addressbook).  Later, we also use a c-client hack.  If an ADDRESS has
     * a host part that begins with @ then rfc822_write_address()
     * will write only the local part and leave off the @domain part.
     *
     * We also malloc enough space here so that we can strcpy over host below.
     */
    if(!recursing){
	domain_length = max(localdomain!=NULL ? strlen(localdomain) : (size_t)0,
			    userdomain!=NULL ? strlen(userdomain) : (size_t)0);
	fakedomain = (char *)fs_get(domain_length + 1);
	memset((void *)fakedomain, '@', domain_length);
	fakedomain[domain_length] = '\0';
    }

    adr = NULL;

    if(to.type == Str){
	/* rfc822_parse_adrlist feels free to destroy input so send copy */
	tmp_a_string = cpystr(to.arg.str);
	/* remove trailing comma */
	for(q = tmp_a_string + strlen(tmp_a_string) - 1;
	    q >= tmp_a_string && (*q == SPACE || *q == ',');
	    q--)
	  *q = '\0';

	if(as.n_impl)
	  mail_parameters(NIL, SET_PARSEPHRASE, (void *)massage_phrase_addr);

	rfc822_parse_adrlist(&adr, tmp_a_string, fakedomain);

	if(as.n_impl)
	  mail_parameters(NIL, SET_PARSEPHRASE, NULL);

	/*
	 * Short circuit the process if there was a parsing error.
	 */
	if(!recursing && ps_global->c_client_error[0] != '\0')
	  mail_free_address(&adr);

	fs_give((void **)&tmp_a_string);
    }
    else{
	if(!to.arg.abe ||
	   (to.arg.abe->tag == Single &&
	     (!to.arg.abe->addr.addr || to.arg.abe->addr.addr[0] == '\0')) ||
	   (to.arg.abe->tag == List &&
	     (!to.arg.abe->addr.list || !to.arg.abe->addr.list[0] ||
	      to.arg.abe->addr.list[0][0] == '\0'))){
	    adr = NULL;
	}
	else{
	    /* if we've already looked it up, fake an adr */
	    adr = mail_newaddr();
	    adr->mailbox = cpystr(to.arg.abe->nickname);
	    adr->host    = cpystr(fakedomain);
	}
    }

    for(a = adr, a_tail = adr; a;){

	/* start or end of c-client group syntax */
	if(!a->host){
            a_tail = a;
            a      = a->next;
	    continue;
	}
        else if(a->host[0] != '@'){
            /* Already fully qualified hostname */
            a_tail = a;
            a      = a->next;
        }
        else{
            /*
             * Hostname is "@" indicating name wasn't qualified.
             * Need to look up in address book, and the password file.
             * If no match then fill in the local domain for host.
             */
	    if(to.type == Str)
              abe = adrbk_lookup_with_opens_by_nick(a->mailbox,
						    !recursing,
						    (int *)NULL, -1);
	    else
              abe = to.arg.abe;

            if(simple_verify && abe == NULL){
                /*--- Move to next address in list -----*/
                a_tail = a;
                a = a->next;
            }
            else if(abe == NULL){
		WP_ERR_S wp_err;

	        if(F_OFF(F_COMPOSE_REJECTS_UNQUAL, ps_global)){
		    if(localdomain != NULL && a->personal == NULL){
			/* lookup in passwd file for local full name */
			a->personal = local_name_lookup(a->mailbox); 
			if(a->personal)
			  strcpy(a->host, localdomain);
		    }
		}

		/*
		 * Didn't find it in address book or password
		 * file, try white pages.
		 */
		memset(&wp_err, 0, sizeof(wp_err));
		wp_err.mangled = mangled;
		if(!wp_exit && a->personal == NULL &&
		   (wp_a = wp_lookups(a->mailbox, &wp_err, recursing))){
		    if(wp_a->mailbox && wp_a->mailbox[0] &&
		       wp_a->host && wp_a->host[0]){
			a->personal = wp_a->personal;
			if(a->adl)fs_give((void **)&a->adl);
			a->adl = wp_a->adl;
			if(a->mailbox)fs_give((void **)&a->mailbox);
			a->mailbox = wp_a->mailbox;
			if(a->host)fs_give((void **)&a->host);
			a->host = wp_a->host;
		    }

		    fs_give((void **)&wp_a);
		}

		if(wp_err.error){
		    /*
		     * If wp_err has already been displayed long enough
		     * just get rid of it. Otherwise, try to fit it in
		     * with any other error messages we have had.
		     * In that case we may display error messages in a
		     * weird order. We'll have to see if this is a problem
		     * in real life.
		     */
		    if(status_message_remaining() && error && !wp_exit){
			if(*error){
			    q_status_message(SM_ORDER, 3, 5, *error);
			    display_message('x');
			    fs_give((void **)error);
			}

			*error = wp_err.error;
		    }
		    else
		      fs_give((void **)&wp_err.error);
		}
		  
		/* still haven't found it */
		if(a->host[0] == '@'){
		    extern const char *rspecials;
		    int space_phrase;

		    /*
		     * Figure out if there is a space in the mailbox so
		     * that user probably meant it to resolve on the
		     * directory server.
		     */
		    space_phrase = (a->mailbox && strindex(a->mailbox, SPACE));

		    if(F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global) ||
		       (space_phrase && as.n_impl)){
			char ebuf[200];

			sprintf(ebuf, "Address for \"%.100s\" not in %.20s",
				a->mailbox,
				(space_phrase && as.n_impl) ? "directory"
							    : "addressbook");

			if(error){
			    /* display previous error and add new one */
			    if(*error){
				if(!wp_exit){
				    q_status_message(SM_ORDER, 3, 5, *error);
				    display_message('x');
				}

				fs_give((void **)error);
			    }

			    if(!wp_exit)
			      *error = cpystr(ebuf);
			}

			if(!wp_exit)
			  strncpy(ps_global->c_client_error, ebuf, 200);

			if(!recursing)
			  fs_give((void **)&fakedomain);

			if(adr)
			  mail_free_address(&adr);
			    
			return(adr);
		    }
		    else if(wp_err.wp_err_occurred){
			if(!recursing){
			    if(error && *error && !wp_exit)
			      strncpy(ps_global->c_client_error, *error, 200);
			    else
			      strcpy(ps_global->c_client_error, " ");

			    fs_give((void **)&fakedomain);
			    if(adr)
			      mail_free_address(&adr);
			    
			    return(adr);
			}
		    }
		    else
		      strcpy(a->host, userdomain);
		}

                /*--- Move to next address in list -----*/
                a_tail = a;
                a = a->next;

            }
	    /* expand first list, but not others if simple_verify */
	    else if(abe->tag == List && simple_verify && recursing){
                /*--- Move to next address in list -----*/
                a_tail = a;
                a = a->next;
	    }
	    else{
                /*
                 * There was a match in the address book.  We have to do a lot
                 * here because the item from the address book might be a 
                 * distribution list.  Treat the string just like an address
                 * passed in to parse and recurse on it.  Then combine
                 * the personal names from address book.  Lastly splice
                 * result into address list being processed
                 */

		/* first addr in list and fcc needs to be filled in */
		if(!recursing && a == adr && fcc && !(*did_set & FCC_SET)){
		    /*
		     * Easy case for fcc.  This is a nickname that has
		     * an fcc associated with it.
		     */
		    if(abe->fcc && abe->fcc[0]){
			if(*fcc)
			  fs_give((void **)fcc);

			if(!strcmp(abe->fcc, "\"\""))
			  *fcc = cpystr("");
			else
			  *fcc = cpystr(abe->fcc);

			/*
			 * After we expand the list, we no longer remember
			 * that it came from this address book entry, so
			 * we wouldn't be able to set the fcc again based
			 * on the result.  This tells our caller to remember
			 * that for us.
			 */
			*did_set |= (FCC_SET | FCC_NOREPO);
		    }
		    /*
		     * else if fcc-rule=fcc-by-nickname, use that
		     */
		    else if(abe->nickname && abe->nickname[0] &&
			    (ps_global->fcc_rule == FCC_RULE_NICK ||
			     ps_global->fcc_rule == FCC_RULE_NICK_RECIP)){
			if(*fcc)
			  fs_give((void **)fcc);

			*fcc = cpystr(abe->nickname);
			/*
			 * After we expand the list, we no longer remember
			 * that it came from this address book entry, so
			 * we wouldn't be able to set the fcc again based
			 * on the result.  This tells our caller to remember
			 * that for us.
			 */
			*did_set |= (FCC_SET | FCC_NOREPO);
		    }
		}

		/* lcc needs to be filled in */
		if(a == adr  &&
		    lcc      &&
		   (!*lcc || !**lcc)){
		    ADDRESS *atmp;
		    char    *tmp;

		    /* return fullname for To line */
		    if(abe->fullname && *abe->fullname){
			if(*lcc)
			  fs_give((void **)lcc);

			atmp = mail_newaddr();
			atmp->mailbox = cpystr(abe->fullname);
			tmp = (char *)fs_get((size_t)est_size(atmp));
			tmp[0] = '\0';
			/* write the phrase with quoting */
			rfc822_write_address(tmp, atmp);
			*lcc = (char *)fs_get((size_t)(strlen(tmp)+1+1));
			strcpy(*lcc, tmp);
			strcat(*lcc, ";");
			mail_free_address(&atmp);
			fs_give((void **)&tmp);
			*did_set |= (LCC_SET | LCC_NOREPO);
		    }
		}

                if(recursing && abe->referenced){
                     /*---- caught an address loop! ----*/
                    fs_give(((void **)&a->host));
		    (*loop_detected)++;
                    a->host = cpystr("***address-loop-in-addressbooks***");
                    continue;
                }

                abe->referenced++;   /* For address loop detection */
                if(abe->tag == List){
                    length = 0;
                    for(l2 = abe->addr.list; *l2; l2++)
                        length += (strlen(*l2) + 1);

                    list = (char *)fs_get(length + 1);
		    list[0] = '\0';
                    l1 = list;
                    for(l2 = abe->addr.list; *l2; l2++){
                        if(l1 != list)
                          *l1++ = ',';
                        strcpy(l1, *l2);
                        l1 += strlen(l1);
                    }

		    bldto.type    = Str;
		    bldto.arg.str = list;
                    adr2 = expand_address(bldto, userdomain, localdomain,
					  loop_detected, fcc, did_set,
					  lcc, error, 1, simple_verify,
					  mangled);
                    fs_give((void **)&list);
                }
                else if(abe->tag == Single){
                    if(strucmp(abe->addr.addr, a->mailbox)){
			bldto.type    = Str;
			bldto.arg.str = abe->addr.addr;
                        adr2 = expand_address(bldto, userdomain,
					      localdomain, loop_detected,
					      fcc, did_set, lcc,
					      error, 1, simple_verify,
					      mangled);
                    }
		    else{
                        /*
			 * A loop within plain single entry is ignored.
			 * Set up so later code thinks we expanded.
			 */
                        adr2          = mail_newaddr();
                        adr2->mailbox = cpystr(abe->addr.addr);
                        adr2->host    = cpystr(userdomain);
                        adr2->adl     = cpystr(a->adl);
                    }
                }

                abe->referenced--;  /* Janet Jackson <janet@dialix.oz.au> */
                if(adr2 == NULL){
                    /* expanded to nothing, hack out of list */
                    a_temp = a;
                    if(a == adr){
                        adr    = a->next;
                        a      = adr;
                        a_tail = adr;
                    }
		    else{
                        a_tail->next = a->next;
                        a            = a->next;
                    }

		    a_temp->next = NULL;  /* So free won't do whole list */
                    mail_free_address(&a_temp);
                    continue;
                }

                /*
                 * Personal names:  If the expanded address has a personal
                 * name and the address book entry is a list with a fullname,
		 * tack the full name from the address book on in front.
                 * This mainly occurs with a distribution list where the
                 * list has a full name, and the first person in the list also
                 * has a full name.
		 *
		 * This algorithm doesn't work very well if lists are
		 * included within lists, but it's not clear what would
		 * be better.
                 */
		if(abe->fullname && abe->fullname[0]){
		    if(adr2->personal && adr2->personal[0]){
			if(abe->tag == List){
			    /* combine list name and existing name */
			    char *name;

			    if(!simple_verify){
				name = (char *)fs_get(strlen(adr2->personal) +
						  strlen(abe->fullname) + 5);
				sprintf(name, "%s -- %s", abe->fullname,
					adr2->personal);
				fs_give((void **)&adr2->personal);
				adr2->personal = name;
			    }
			}
			else{
			    /* replace with nickname fullname */
			    fs_give((void **)&adr2->personal);
			    adr2->personal = adrbk_formatname(abe->fullname,
							      NULL, NULL);
			}
		    }
		    else{
			if(abe->tag != List || !simple_verify){
			    if(adr2->personal)
			      fs_give((void **)&adr2->personal);

			    adr2->personal = adrbk_formatname(abe->fullname,
							      NULL, NULL);
			}
		    }
		}

                /* splice new list into old list and remove replaced addr */
                for(a2 = adr2; a2->next != NULL; a2 = a2->next)
		  ;/* do nothing */

                a2->next = a->next;
                if(a == adr)
                  adr    = adr2;
		else
                  a_tail->next = adr2;

                /* advance to next item, and free replaced ADDRESS */
                a_tail       = a2;
                a_temp       = a;
                a            = a->next;
                a_temp->next = NULL;  /* So free won't do whole list */
                mail_free_address(&a_temp);
            }
        }

	if((a_tail == adr && fcc && !(*did_set & FCC_SET))
	   || !a_tail->personal)
	  /*
	   * This looks for the addressbook entry that matches the given
	   * address.  It looks through all the addressbooks
	   * looking for an exact match and then returns that entry.
	   */
	  abe2 = address_to_abe(a_tail);
	else
	  abe2 = NULL;

	/*
	 * If there is no personal name yet but we found the address in
	 * an address book, then we take the fullname from that address
	 * book entry and use it.  One consequence of this is that if I
	 * have an address book entry with address hubert@cac.washington.edu
	 * and a fullname of Steve Hubert, then there is no way I can
	 * send mail to hubert@cac.washington.edu without having the
	 * personal name filled in for me.
	 */
	if(!a_tail->personal && abe2 && abe2->fullname && abe2->fullname[0])
	  a_tail->personal = adrbk_formatname(abe2->fullname, NULL, NULL);

	/* if it's first addr in list and fcc hasn't been set yet */
	if(!recursing && a_tail == adr && fcc && !(*did_set & FCC_SET)){
	    if(abe2 && abe2->fcc && abe2->fcc[0]){
		if(*fcc)
		  fs_give((void **)fcc);

		if(!strcmp(abe2->fcc, "\"\""))
		  *fcc = cpystr("");
		else
		  *fcc = cpystr(abe2->fcc);

		*did_set |= FCC_SET;
	    }
	    else if(abe2 && abe2->nickname && abe2->nickname[0] &&
		    (ps_global->fcc_rule == FCC_RULE_NICK ||
		     ps_global->fcc_rule == FCC_RULE_NICK_RECIP)){
		if(*fcc)
		  fs_give((void **)fcc);

		*fcc = cpystr(abe2->nickname);
		*did_set |= FCC_SET;
	    }
	}

	/*
	 * Lcc needs to be filled in.
	 * Bug: if ^T select was used to put the list in the lcc field, then
	 * the list will have been expanded already and the fullname for
	 * the list will be mixed with the initial fullname in the list,
	 * and we don't have anyway to tell them apart.  We could look for
	 * the --.  We could change expand_address so it doesn't combine
	 * those two addresses.
	 */
	if(adr &&
	    a_tail == adr  &&
	    lcc      &&
	   (!*lcc || !**lcc)){
	    if(adr->personal){
		ADDRESS *atmp;
		char    *tmp;

		if(*lcc)
		  fs_give((void **)lcc);

		atmp = mail_newaddr();
		atmp->mailbox = cpystr(adr->personal);
		tmp = (char *)fs_get((size_t)est_size(atmp));
		tmp[0] = '\0';
		/* write the phrase with quoting */
		rfc822_write_address(tmp, atmp);
		*lcc = (char *)fs_get((size_t)(strlen(tmp)+1+1));
		strcpy(*lcc, tmp);
		strcat(*lcc, ";");
		mail_free_address(&atmp);
		fs_give((void **)&tmp);
		*did_set |= (LCC_SET | LCC_NOREPO);
	    }
	}
    }

    if(!recursing)
      fs_give((void **)&fakedomain);

    return(adr);
}


/*
 * This is a call back from rfc822_parse_adrlist. If we return NULL then
 * it does its regular parsing. However, if we return and ADDRESS then it
 * returns that address to us.
 *
 * We want a phrase with no address to be parsed just like a nickname would
 * be in expand_address. That is, the phrase gets put in the mailbox name
 * and the host is set to the fakedomain. Then expand_address will try to
 * look that up using wp_lookup().
 *
 * Args     phrase -- The start of the phrase of the address
 *             end -- The first character after the phrase
 *            host -- The defaulthost that was passed to rfc822_parse_adrlist
 *
 * Returns  An allocated address, or NULL.
 */
ADDRESS *
massage_phrase_addr(phrase, end, host)
    char *phrase;
    char *end;
    char *host;
{
    ADDRESS *adr = NULL;
    size_t   size;

    if((size = end - phrase) > 0){
	char *mycopy;

	mycopy = (char *)fs_get((size+1) * sizeof(char));
	strncpy(mycopy, phrase, size);
	mycopy[size] = '\0';
	removing_trailing_white_space(mycopy);

	/*
	 * If it is quoted we want to leave it alone. It will be treated
	 * like an atom in parse_adrlist anyway, which is what we want to
	 * have happen, and we'd have to remove the quotes ourselves here
	 * if we did it. The problem then is that we don't know if we
	 * removed the quotes here or they just weren't there in the
	 * first place.
	 */
	if(*mycopy == '"' && mycopy[strlen(mycopy)-1] == '"')
	  fs_give((void **)&mycopy);
	else{
	    adr = mail_newaddr();
	    adr->mailbox = mycopy;
	    adr->host    = cpystr(host);
	}
    }

    return(adr);
}


/*
 * This function does white pages lookups.
 *
 * Args  string -- the string to use in the lookup
 *
 * Returns     NULL -- lookup failed
 *          Address -- A single address is returned if lookup was successfull.
 */
ADDRESS *
wp_lookups(string, wp_err, recursing)
    char     *string;
    WP_ERR_S *wp_err;
    int       recursing;
{
    ADDRESS *ret_a = NULL;
    char ebuf[200];
#ifdef	ENABLE_LDAP
    LDAP_SERV_RES_S *free_when_done = NULL;
    LDAPLookupStyle style;
    LDAP_SERV_RES_S *winning_e = NULL;
    LDAP_SERV_S *info = NULL;
    static char *fakedomain = "@";
    char *tmp_a_string;

    /*
     * Runtime ldap lookup of addrbook entry.
     */
    if(!strncmp(string, RUN_LDAP, LEN_RL)){
	LDAP_SERV_RES_S *head_of_result_list;

	info = break_up_ldap_server(string+LEN_RL);
        head_of_result_list = ldap_lookup(info, "", NULL, wp_err, 1);

	if(head_of_result_list){
	    if(!wp_exit)
	      (void)ask_user_which_entry(head_of_result_list, string,
					 &winning_e,
					 wp_err,
					 wp_err->wp_err_occurred
					   ? DisplayIfOne : DisplayIfTwo);

	    free_when_done = head_of_result_list;
	}
	else{
	    wp_err->wp_err_occurred = 1;
	    if(wp_err->error){
		q_status_message(SM_ORDER, 3, 5, wp_err->error);
		display_message('x');
		fs_give((void **)&wp_err->error);
	    }

	    /* try using backup email address */
	    if(info && info->mail && *info->mail){
		tmp_a_string = cpystr(info->mail);
		rfc822_parse_adrlist(&ret_a, tmp_a_string, fakedomain);
		fs_give((void **)&tmp_a_string);

		wp_err->error =
		  cpystr("Directory lookup failed, using backup email address");
	    }
	    else{
		/*
		 * Do this so the awful LDAP: ... string won't show up
		 * in the composer. This shouldn't actually happen in
		 * real life, so we're not too concerned about it. If we
		 * were we'd want to recover the nickname we started with
		 * somehow, or something like that.
		 */
		ret_a = mail_newaddr();
		ret_a->mailbox = cpystr("missing-username");
		wp_err->error = cpystr("Directory lookup failed, no backup email address available");
	    }

	    q_status_message(SM_ORDER, 3, 5, wp_err->error);
	    display_message('x');
	}
    }
    else{
	style = F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global)
			? DisplayIfOne : DisplayIfTwo;
	(void)ldap_lookup_all(string, as.n_serv, recursing, style, NULL,
			      &winning_e, wp_err, &free_when_done);
    }


    if(winning_e){
	ret_a = address_from_ldap(winning_e);
	if(ret_a && F_ON(F_ADD_LDAP_TO_ABOOK, ps_global) && !info)
	  save_ldap_entry(ps_global, winning_e, 1);

	fs_give((void **)&winning_e);
    }

    /* Info's only set in the RUN_LDAP case */
    if(info){
	if(ret_a && ret_a->host){
	    ADDRESS *backup = NULL;

	    if(info->mail && *info->mail)
	      rfc822_parse_adrlist(&backup, info->mail, fakedomain);

	    if(!backup || !address_is_same(ret_a, backup)){
		if(wp_err->error){
		    q_status_message(SM_ORDER, 3, 5, wp_err->error);
		    display_message('x');
		    fs_give((void **)&wp_err->error);
		}

		sprintf(ebuf,
	       "Warning: current address different from saved address (%.60s)",
		   info->mail);
		wp_err->error = cpystr(ebuf);
		q_status_message(SM_ORDER, 3, 5, wp_err->error);
		display_message('x');
	    }

	    if(backup)
	      mail_free_address(&backup);
	}

	free_ldap_server_info(&info);
    }

    if(free_when_done)
      free_ldap_result_list(free_when_done);
#endif	/* ENABLE_LDAP */

    if(ret_a){
	if(ret_a->mailbox){  /* indicates there was a MAIL attribute */
	    if(!ret_a->host || !ret_a->host[0]){
		if(ret_a->host)
		  fs_give((void **)&ret_a->host);

		ret_a->host = cpystr("missing-hostname");
		wp_err->wp_err_occurred = 1;
		if(wp_err->error)
		  fs_give((void **)&wp_err->error);

		wp_err->error = cpystr("Missing hostname in LDAP address");
		q_status_message(SM_ORDER, 3, 5, wp_err->error);
		display_message('x');
	    }

	    if(!ret_a->mailbox[0]){
		if(ret_a->mailbox)
		  fs_give((void **)&ret_a->mailbox);

		ret_a->mailbox = cpystr("missing-username");
		wp_err->wp_err_occurred = 1;
		if(wp_err->error)
		  fs_give((void **)&wp_err->error);

		wp_err->error = cpystr("Missing username in LDAP address");
		q_status_message(SM_ORDER, 3, 5, wp_err->error);
		display_message('x');
	    }
	}
	else{
	    wp_err->wp_err_occurred = 1;

	    if(wp_err->error)
	      fs_give((void **)&wp_err->error);

	    sprintf(ebuf, "No email address available for \"%.60s\"",
		    (ret_a->personal && *ret_a->personal)
			    ? ret_a->personal
			    : "selected entry");
	    wp_err->error = cpystr(ebuf);
	    q_status_message(SM_ORDER, 3, 5, wp_err->error);
	    display_message('x');
	    mail_free_address(&ret_a);
	    ret_a = NULL;
	}
    }

    return(ret_a);
}


#ifdef	ENABLE_LDAP
/*
 * Goes through all servers looking up string.
 *
 * Args  string -- String to search for
 *          who -- Which servers to look on
 *        style -- Sometimes we want to display no matter what, sometimes
 *                  only if more than one entry, sometimes only if email
 *                  addresses, ...
 *
 *     The possible styles are:
 *      AlwaysDisplayAndMailRequired: This happens when user ^T's from
 *                                    composer to address book screen, and
 *                                    then queries directory server.
 *                     AlwaysDisplay: This happens when user is browsing
 *                                    in address book maintenance screen and
 *                                    then queries directory server.
 *                      DisplayIfOne: These two come from implicit lookups
 *                      DisplayIfTwo: from build_address. If the compose rejects
 *                                    unqualified feature is on we get the
 *                                    DisplayIfOne, otherwise IfTwo.
 *
 *         cust -- Use this custom filter instead of configured filters
 *    winning_e -- Return value
 *       wp_err -- Error handling
 * free_when_done -- Caller needs to free this
 *
 * Returns  -- value returned by ask_user_which_entry
 */
int
ldap_lookup_all(string, who, recursing, style, cust, winning_e, wp_err,
		free_when_done)
    char                  *string;
    int                    who;
    int                    recursing;
    LDAPLookupStyle        style;
    CUSTOM_FILT_S         *cust;
    LDAP_SERV_RES_S      **winning_e;
    WP_ERR_S              *wp_err;
    LDAP_SERV_RES_S      **free_when_done;
{
    int      i, retval;
    LDAP_SERV_RES_S *serv_res;
    LDAP_SERV_RES_S *rr, *head_of_result_list = NULL;

    wp_exit = 0;

    /* maybe coming from composer */
    fix_windsize(ps_global);
    init_sigwinch();
    clear_cursor_pos();

    /* If there is at least one server */
    if(ps_global->VAR_LDAP_SERVERS && ps_global->VAR_LDAP_SERVERS[0] &&
       ps_global->VAR_LDAP_SERVERS[0][0]){
	int how_many_servers;

	for(i = 0; ps_global->VAR_LDAP_SERVERS[i] &&
		   ps_global->VAR_LDAP_SERVERS[i][0]; i++)
	  ;

	how_many_servers = i;

	/* For each server in list */
	for(i = 0; !wp_exit && ps_global->VAR_LDAP_SERVERS[i] &&
			       ps_global->VAR_LDAP_SERVERS[i][0]; i++){
	    LDAP_SERV_S *info;

	    info = NULL;
	    if(who == -1 || who == i || who == as.n_serv)
	      info = break_up_ldap_server(ps_global->VAR_LDAP_SERVERS[i]);

	    /*
	     * Who tells us which servers to look on.
	     * Who == -1 means all servers.
	     * Who ==  0 means server[0].
	     * Who ==  1 means server[1].
	     * Who == as.n_serv means query on those with impl set.
	     */
	    if(!(who == -1 || who == i ||
		 (who == as.n_serv && !recursing && info && info->impl) ||
		 (who == as.n_serv && recursing && info && info->rhs))){

		if(info)
		  free_ldap_server_info(&info);
		
		continue;
	    }

	    serv_res = ldap_lookup(info, string, cust,
				   wp_err, how_many_servers > 1);
	    if(serv_res){
		/* Add new one to end of list so they come in the right order */
		for(rr = head_of_result_list; rr && rr->next; rr = rr->next)
		  ;
		
		if(rr)
		  rr->next = serv_res;
		else
		  head_of_result_list = serv_res;
	    }

	    if(info)
	      free_ldap_server_info(&info);
	}
    }

    if(!wp_exit)
      retval = ask_user_which_entry(head_of_result_list, string, winning_e,
				    wp_err,
				    (wp_err->wp_err_occurred &&
				     style == DisplayIfTwo) ? DisplayIfOne
							    : style);

    *free_when_done = head_of_result_list;

    return(retval);
}


/*
 * Do an LDAP lookup to the server described in the info argument.
 *
 * Args      info -- LDAP info for server.
 *         string -- String to lookup.
 *           cust -- Possible custom filter description.
 *         wp_err -- We set this is we get a white pages error.
 *  name_in_error -- Caller sets this if they want us to include the server
 *                   name in error messages.
 *
 * Returns  Results of lookup, NULL if lookup failed.
 */
LDAP_SERV_RES_S *
ldap_lookup(info, string, cust, wp_err, name_in_error)
    LDAP_SERV_S   *info;
    char          *string;
    CUSTOM_FILT_S *cust;
    WP_ERR_S      *wp_err;
    int            name_in_error;
{
    char     ebuf[900];
    char     buf[900];
    char    *serv, *base, *serv_errstr;
    char    *mailattr, *snattr, *gnattr, *cnattr;
    int      we_cancel;
    LDAP_SERV_RES_S *serv_res = NULL;
    LDAP *ld;
    int   ld_errnum;
    char *ld_errstr;


    serv = cpystr((info->serv && *info->serv) ? info->serv : "?");
    if(name_in_error)
      sprintf(ebuf, " (%.100s)",
	      (info->nick && *info->nick) ? info->nick : serv);
    else
      ebuf[0] = '\0';

    serv_errstr = cpystr(ebuf);
    base = cpystr(info->base ? info->base : "");

    if(info->port < 0)
      info->port = LDAP_PORT;

    if(info->type < 0)
      info->type = DEF_LDAP_TYPE;

    if(info->srch < 0)
      info->srch = DEF_LDAP_SRCH;
	
    if(info->time < 0)
      info->time = DEF_LDAP_TIME;

    if(info->size < 0)
      info->size = DEF_LDAP_SIZE;

    if(info->scope < 0)
      info->scope = DEF_LDAP_SCOPE;

    mailattr = (info->mailattr && info->mailattr[0]) ? info->mailattr
						     : DEF_LDAP_MAILATTR;
    snattr = (info->snattr && info->snattr[0]) ? info->snattr
						     : DEF_LDAP_SNATTR;
    gnattr = (info->gnattr && info->gnattr[0]) ? info->gnattr
						     : DEF_LDAP_GNATTR;
    cnattr = (info->cnattr && info->cnattr[0]) ? info->cnattr
						     : DEF_LDAP_CNATTR;

    /*
     * We may want to keep ldap handles open, but at least for
     * now, re-open them every time.
     */

    dprint(2,(debugfile, "ldap_lookup(%s,%d)\n", serv ? serv : "?",
	   info->port));

    sprintf(ebuf, "Searching%.100s%.100s%.100s on %.100s",
	    (string && *string) ? " for \"" : "",
	    (string && *string) ? string : "",
	    (string && *string) ? "\"" : "",
	    serv);
    intr_handling_on();		/* this erases keymenu */
    we_cancel = busy_alarm(1, ebuf, NULL, 1);
    if(wp_err->mangled)
      *(wp_err->mangled) = 1;

#if (LDAPAPI >= 11)
    if((ld = ldap_init(serv, info->port)) == NULL)
#else
    if((ld = ldap_open(serv, info->port)) == NULL)
#endif
    {
      sprintf(ebuf, "Access to LDAP server failed: %.100s%.100s(%.100s)",
	      errno ? error_description(errno) : "",
	      errno ? " " : "",
	      serv);
      wp_err->wp_err_occurred = 1;
      if(wp_err->error)
	fs_give((void **)&wp_err->error);

      wp_err->error = cpystr(ebuf);
      if(we_cancel)
        cancel_busy_alarm(-1);

      q_status_message(SM_ORDER, 3, 5, wp_err->error);
      display_message('x');
      dprint(2, (debugfile, "%s\n", ebuf));
    }
    else if(!ps_global->intr_pending){
      int proto = 3;

      if(ldap_v3_is_supported(ld, info) &&
	 our_ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &proto) == 0){
	dprint(5,(debugfile, "ldap: using version 3 protocol\n"));
      }

      /*
       * If we don't set RESTART then the select() waiting for the answer
       * in libldap will be interrupted and stopped by our busy_alarm.
       */
      our_ldap_set_option(ld, LDAP_OPT_RESTART, LDAP_OPT_ON);

      /*
       * LDAPv2 requires the bind. v3 doesn't require it but we want
       * to tell the server we're v3 if the server supports v3, and if the
       * server doesn't support v3 the bind is required.
       */
      if(ldap_simple_bind_s(ld, NULL, NULL) != LDAP_SUCCESS){
	wp_err->wp_err_occurred = 1;

	ld_errnum = our_ldap_get_lderrno(ld, NULL, &ld_errstr);

	sprintf(ebuf, "LDAP server failed: %.100s%.100s%.100s%.100s",
		ldap_err2string(ld_errnum),
		serv_errstr,
		(ld_errstr && *ld_errstr) ? ": " : "",
		(ld_errstr && *ld_errstr) ? ld_errstr : "");

        if(wp_err->error)
	  fs_give((void **)&wp_err->error);

        if(we_cancel)
          cancel_busy_alarm(-1);

	ldap_unbind(ld);
        wp_err->error = cpystr(ebuf);
        q_status_message(SM_ORDER, 3, 5, wp_err->error);
        display_message('x');
	dprint(2, (debugfile, "%s\n", ebuf));
      }
      else if(!ps_global->intr_pending){
	int          srch_res, args, slen, flen;
#define TEMPLATELEN 512
	char         filt_template[TEMPLATELEN + 1];
	char         filt_format[2*TEMPLATELEN + 1];
	char         filter[2*TEMPLATELEN + 1];
	char         scp[2*TEMPLATELEN + 1];
	char        *p, *q;
	LDAPMessage *res = NULL;
	int intr_happened = 0;
	int tl;

	tl = (info->time == 0) ? info->time : info->time + 10;

	our_ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &tl);
	our_ldap_set_option(ld, LDAP_OPT_SIZELIMIT, &info->size);

	/*
	 * If a custom filter has been passed in and it doesn't include a
	 * request to combine it with the configured filter, then replace
	 * any configured filter with the passed in filter.
	 */
	if(cust && cust->filt && !cust->combine){
	    if(info->cust)
	      fs_give((void **)&info->cust);
	    
	    info->cust = cpystr(cust->filt);
	}

	if(info->cust && *info->cust){	/* use custom filter if present */
	    strncpy(filt_template, info->cust, TEMPLATELEN);
	    filt_template[TEMPLATELEN] = '\0';
	}
	else{				/* else use configured filter */
	    switch(info->type){
	      case LDAP_TYPE_SUR:
		sprintf(filt_template, "(%.100s=%%s)", snattr);
		break;
	      case LDAP_TYPE_GIVEN:
		sprintf(filt_template, "(%.100s=%%s)", gnattr);
		break;
	      case LDAP_TYPE_EMAIL:
		sprintf(filt_template, "(%.100s=%%s)", mailattr);
		break;
	      case LDAP_TYPE_CN_EMAIL:
		sprintf(filt_template, "(|(%.100s=%%s)(%.100s=%%s))", cnattr,
			mailattr);
		break;
	      case LDAP_TYPE_SUR_GIVEN:
		sprintf(filt_template, "(|(%.100s=%%s)(%.100s=%%s))",
			snattr, gnattr);
		break;
	      case LDAP_TYPE_SEVERAL:
		sprintf(filt_template,
			"(|(%.100s=%%s)(%.100s=%%s)(%.100s=%%s)(%.100s=%%s))",
			cnattr, mailattr, snattr, gnattr);
		break;
	      default:
	      case LDAP_TYPE_CN:
		sprintf(filt_template, "(%.100s=%%s)", cnattr);
		break;
	    }
	}

	/* just copy if custom */
	if(info->cust && *info->cust)
	  info->srch = LDAP_SRCH_EQUALS;

	p = filt_template;
	q = filt_format;
	memset((void *)filt_format, 0, sizeof(filt_format));
	args = 0;
	while(*p && (q - filt_format) < (2*TEMPLATELEN - 4)){
	    if(*p == '%' && *(p+1) == 's'){
		args++;
		switch(info->srch){
		  /* Exact match */
		  case LDAP_SRCH_EQUALS:
		    *q++ = *p++;
		    *q++ = *p++;
		    break;

		  /* Append wildcard after %s */
		  case LDAP_SRCH_BEGINS:
		    *q++ = *p++;
		    *q++ = *p++;
		    *q++ = '*';
		    break;

		  /* Insert wildcard before %s */
		  case LDAP_SRCH_ENDS:
		    *q++ = '*';
		    *q++ = *p++;
		    *q++ = *p++;
		    break;

		  /* Put wildcard before and after %s */
		  default:
		  case LDAP_SRCH_CONTAINS:
		    *q++ = '*';
		    *q++ = *p++;
		    *q++ = *p++;
		    *q++ = '*';
		    break;
		}
	    }
	    else
	      *q++ = *p++;
	}

	/*
	 * If combine is lit we put the custom filter and the filt_format
	 * filter and combine them with an &.
	 */
	if(cust && cust->filt && cust->combine){
	    char *combined;

	    combined = (char *)fs_get(strlen(filt_format) +
				      strlen(cust->filt) + 4);
	    sprintf(combined, "(&%s%s)", cust->filt, filt_format);
	    strncpy(filt_format, combined, 2*TEMPLATELEN);
	    fs_give((void **)&combined);
	}

	/*
	 * Ad hoc attempt to make "Steve Hubert" match
	 * Steven Hubert but not Steven Shubert.
	 * We replace a <SPACE> with * <SPACE> (not * <SPACE> *).
	 */
	memset((void *)scp, 0, sizeof(scp));
	if(info->nosub)
	  strncpy(scp, string, 2*TEMPLATELEN);
	else{
	    p = string;
	    q = scp;
	    while(*p && (q - scp) < (2*TEMPLATELEN - 2)){
		if(*p == SPACE && *(p+1) != SPACE){
		    *q++ = '*';
		    *q++ = *p++;
		}
		else
		  *q++ = *p++;
	    }
	}

	slen = strlen(scp);
	flen = strlen(filt_format);
	/* truncate string if it will overflow filter */
	if(args*slen + flen - 2*args > 2*TEMPLATELEN)
	  scp[(2*TEMPLATELEN - flen)/args] = '\0';

	/*
	 * Replace %s's with scp.
	 */
	switch(args){
	  case 0:
	    sprintf(filter, filt_format);
	    break;
	  case 1:
	    sprintf(filter, filt_format, scp);
	    break;
	  case 2:
	    sprintf(filter, filt_format, scp, scp);
	    break;
	  case 3:
	    sprintf(filter, filt_format, scp, scp, scp);
	    break;
	  case 4:
	    sprintf(filter, filt_format, scp, scp, scp, scp);
	    break;
	  case 5:
	    sprintf(filter, filt_format, scp, scp, scp, scp, scp);
	    break;
	  case 6:
	    sprintf(filter, filt_format, scp, scp, scp, scp, scp, scp);
	    break;
	  case 7:
	    sprintf(filter, filt_format, scp, scp, scp, scp, scp, scp, scp);
	    break;
	  case 8:
	    sprintf(filter, filt_format, scp, scp, scp, scp, scp, scp, scp,
		    scp);
	    break;
	  case 9:
	    sprintf(filter, filt_format, scp, scp, scp, scp, scp, scp, scp,
		    scp, scp);
	    break;
	  case 10:
	  default:
	    sprintf(filter, filt_format, scp, scp, scp, scp, scp, scp, scp,
		    scp, scp, scp);
	    break;
	}

	(void)removing_double_quotes(base);
	dprint(5, (debugfile, "about to ldap_search(\"%s\", %s)\n",
	       base ? base : "?", filter ? filter : "?"));
        if(ps_global->intr_pending)
	  srch_res = LDAP_PROTOCOL_ERROR;
	else{
	  int msgid;
	  time_t start_time;

	  start_time = time((time_t *)0);

	  msgid = ldap_search(ld, base, info->scope, filter, NULL, 0);

	  if(msgid == -1)
	    srch_res = our_ldap_get_lderrno(ld, NULL, NULL);
	  else{
	    int lres;
	    /*
	     * Warning: struct timeval is not portable. However, since it is
	     * part of LDAP api it must be portable to all platforms LDAP
	     * has been ported to.
	     */
	    struct timeval t;

	    t.tv_sec  = 1; t.tv_usec = 0;
	      
	    do {
	      if(ps_global->intr_pending)
		intr_happened = 1;

	      dprint(6, (debugfile, "ldap_result(id=%d): ", msgid));
	      if((lres=ldap_result(ld, msgid, LDAP_MSG_ALL, &t, &res)) == -1){
	        /* error */
		srch_res = our_ldap_get_lderrno(ld, NULL, NULL);
	        dprint(6, (debugfile, "error (-1 returned): ld_errno=%d\n",
			   srch_res));
	      }
	      else if(lres == 0){  /* timeout, no results available */
		if(intr_happened){
		  ldap_abandon(ld, msgid);
		  srch_res = LDAP_PROTOCOL_ERROR;
		  if(our_ldap_get_lderrno(ld, NULL, NULL) == LDAP_SUCCESS)
		    our_ldap_set_lderrno(ld, LDAP_PROTOCOL_ERROR, NULL, NULL);

	          dprint(6, (debugfile, "timeout, intr: srch_res=%d\n",
			     srch_res));
		}
		else if(info->time > 0 &&
			((long)time((time_t *)0) - start_time) > info->time){
		  /* try for partial results */
		  t.tv_sec  = 0; t.tv_usec = 0;
		  lres = ldap_result(ld, msgid, LDAP_MSG_RECEIVED, &t, &res);
		  if(lres > 0 && lres != LDAP_RES_SEARCH_RESULT){
		    srch_res = LDAP_SUCCESS;
		    dprint(6, (debugfile, "partial result: lres=0x%x\n", lres));
		  }
		  else{
		    if(lres == 0)
		      ldap_abandon(ld, msgid);

		    srch_res = LDAP_TIMEOUT;
		    if(our_ldap_get_lderrno(ld, NULL, NULL) == LDAP_SUCCESS)
		      our_ldap_set_lderrno(ld, LDAP_TIMEOUT, NULL, NULL);

	            dprint(6, (debugfile,
			       "timeout, total_time (%d), srch_res=%d\n",
			       info->time, srch_res));
		  }
		}
		else{
	          dprint(6, (debugfile, "timeout\n"));
		}
	      }
	      else{
		srch_res = ldap_result2error(ld, res, 0);
	        dprint(6, (debugfile, "lres=0x%x, srch_res=%d\n", lres,
			   srch_res));
	      }
	    }while(lres == 0 &&
		    !(intr_happened ||
		      (info->time > 0 &&
		       ((long)time((time_t *)0) - start_time) > info->time)));
	  }
	}

	if(intr_happened){
	  wp_exit = 1;
          if(we_cancel)
            cancel_busy_alarm(-1);

	  if(wp_err->error)
	    fs_give((void **)&wp_err->error);
	  else{
	    q_status_message(SM_ORDER, 0, 1, "Interrupt");
	    display_message('x');
	    fflush(stdout);
	  }

	  if(res)
	    ldap_msgfree(res);
	  if(ld)
	    ldap_unbind(ld);
	  
	  res = NULL; ld  = NULL;
	}
	else if(srch_res != LDAP_SUCCESS &&
	   srch_res != LDAP_TIMELIMIT_EXCEEDED &&
	   srch_res != LDAP_RESULTS_TOO_LARGE &&
	   srch_res != LDAP_TIMEOUT &&
	   srch_res != LDAP_SIZELIMIT_EXCEEDED){
	  wp_err->wp_err_occurred = 1;

	  ld_errnum = our_ldap_get_lderrno(ld, NULL, &ld_errstr);

	  sprintf(ebuf, "LDAP search failed: %.100s%.100s%.100s%.100s",
		  ldap_err2string(ld_errnum),
		  serv_errstr,
		  (ld_errstr && *ld_errstr) ? ": " : "",
		  (ld_errstr && *ld_errstr) ? ld_errstr : "");

          if(wp_err->error)
	    fs_give((void **)&wp_err->error);

          wp_err->error = cpystr(ebuf);
          if(we_cancel)
            cancel_busy_alarm(-1);

          q_status_message(SM_ORDER, 3, 5, wp_err->error);
          display_message('x');
	  dprint(2, (debugfile, "%s\n", ebuf));
	  if(res)
	    ldap_msgfree(res);
	  if(ld)
	    ldap_unbind(ld);
	  
	  res = NULL; ld  = NULL;
	}
	else{
	  int cnt;

	  cnt = ldap_count_entries(ld, res);

	  if(cnt > 0){

	    if(srch_res == LDAP_TIMELIMIT_EXCEEDED ||
	       srch_res == LDAP_RESULTS_TOO_LARGE ||
	       srch_res == LDAP_TIMEOUT ||
	       srch_res == LDAP_SIZELIMIT_EXCEEDED){
	      wp_err->wp_err_occurred = 1;
	      ld_errnum = our_ldap_get_lderrno(ld, NULL, &ld_errstr);

	      sprintf(ebuf, "LDAP partial results: %.100s%.100s%.100s%.100s",
		      ldap_err2string(ld_errnum),
		      serv_errstr,
		      (ld_errstr && *ld_errstr) ? ": " : "",
		      (ld_errstr && *ld_errstr) ? ld_errstr : "");
	      dprint(2, (debugfile, "%s\n", ebuf));
	      if(wp_err->error)
		fs_give((void **)&wp_err->error);

	      wp_err->error = cpystr(ebuf);
	      if(we_cancel)
		cancel_busy_alarm(-1);

	      q_status_message(SM_ORDER, 3, 5, wp_err->error);
	      display_message('x');
	    }

	    dprint(5, (debugfile, "Matched %d entries on %s\n",
	           cnt, serv ? serv : "?"));

	    serv_res = (LDAP_SERV_RES_S *)fs_get(sizeof(LDAP_SERV_RES_S));
	    memset((void *)serv_res, 0, sizeof(*serv_res));
	    serv_res->ld   = ld;
	    serv_res->res  = res;
	    serv_res->info_used = copy_ldap_serv_info(info);
	    /* Save by reference? */
	    if(info->ref){
		sprintf(buf, "%.100s:%.100s", serv, comatose(info->port));
		serv_res->serv = cpystr(buf);
	    }
	    else
	      serv_res->serv = NULL;

	    serv_res->next = NULL;
	  }
	  else{
	    if(srch_res == LDAP_TIMELIMIT_EXCEEDED ||
	       srch_res == LDAP_RESULTS_TOO_LARGE ||
	       srch_res == LDAP_TIMEOUT ||
	       srch_res == LDAP_SIZELIMIT_EXCEEDED){
	      wp_err->wp_err_occurred = 1;
	      wp_err->ldap_errno      = srch_res;

	      ld_errnum = our_ldap_get_lderrno(ld, NULL, &ld_errstr);

	      sprintf(ebuf, "LDAP search failed: %.100s%.100s%.100s%.100s",
		      ldap_err2string(ld_errnum),
		      serv_errstr,
		      (ld_errstr && *ld_errstr) ? ": " : "",
		      (ld_errstr && *ld_errstr) ? ld_errstr : "");

	      if(wp_err->error)
		fs_give((void **)&wp_err->error);

	      wp_err->error = cpystr(ebuf);
	      if(we_cancel)
		cancel_busy_alarm(-1);

	      q_status_message(SM_ORDER, 3, 5, wp_err->error);
	      display_message('x');
	      dprint(2, (debugfile, "%s\n", ebuf));
	    }

	    dprint(5, (debugfile, "Matched 0 entries on %s\n",
		   serv ? serv : "?"));
	    if(res)
	      ldap_msgfree(res);
	    if(ld)
	      ldap_unbind(ld);

	    res = NULL; ld  = NULL;
	  }
	}
      }
    }

    if(we_cancel)
      cancel_busy_alarm(-1);

    intr_handling_off();

    if(serv)
      fs_give((void **)&serv);
    if(base)
      fs_give((void **)&base);
    if(serv_errstr)
      fs_give((void **)&serv_errstr);

    return(serv_res);
}


/*
 * Given a list of entries, present them to user so user may
 * select one.
 *
 * Args  head -- The head of the list of results
 *       orig -- The string the user was searching for
 *     result -- Returned pointer to chosen LDAP_SEARCH_WINNER or NULL
 *     wp_err -- Error handling
 *      style -- 
 *
 * Returns  0   ok
 *         -1   Exit chosen by user
 *         -2   None of matched entries had an email address
 *         -3   No matched entries
 *         -4   Goback to Abook List chosen by user
 */
int
ask_user_which_entry(head, orig, result, wp_err, style)
    LDAP_SERV_RES_S       *head;
    char                  *orig;
    LDAP_SERV_RES_S      **result;
    WP_ERR_S              *wp_err;
    LDAPLookupStyle        style;
{
    ADDR_CHOOSE_S ac;
    char          t[200];
    int           retval;

    dprint(3, (debugfile, "ask_user_which(style=%s)\n",
	style == AlwaysDisplayAndMailRequired ? "AlwaysDisplayAndMailRequired" :
	  style == AlwaysDisplay ? "AlwaysDisplay" :
	    style == DisplayIfTwo ? "DisplayIfTwo" :
	      style == DisplayForURL ? "DisplayForURL" :
	        style == DisplayIfOne ? "DisplayIfOne" : "?"));

    /*
     * Set up a screen for user to choose one entry.
     */

    if(style == AlwaysDisplay || style == DisplayForURL)
      sprintf(t, "SEARCH RESULTS INDEX");
    else{
	int len;

	len = strlen(orig);
	sprintf(t, "SELECT ONE ADDRESS%.20s%.100s%.20s",
		(orig && *orig && len < 40) ? " FOR \"" : "",
		(orig && *orig && len < 40) ? orig : "",
		(orig && *orig && len < 40) ? "\"" : "");
    }

    memset(&ac, 0, sizeof(ADDR_CHOOSE_S));
    ac.title = cpystr(t);
    ac.res_head = head;

    retval = ldap_addr_select(ps_global, &ac, result, style, wp_err);
    switch(retval){
      case 0:		/* Ok */
	break;

      case -1:		/* Exit chosen by user */
	wp_exit = 1;
	break;

      case -4:		/* GoBack to AbookList chosen by user */
	break;

      case -2:
	if(style != AlwaysDisplay){
	    if(wp_err->error)
	      fs_give((void **)&wp_err->error);

	    wp_err->error =
   cpystr("None of the names matched on directory server has an email address");
	    q_status_message(SM_ORDER, 3, 5, wp_err->error);
	    display_message('x');
	}

	break;

      case -3:
	if(style == AlwaysDisplayAndMailRequired){
	    if(wp_err->error)
	      fs_give((void **)&wp_err->error);

	    wp_err->error = cpystr("No matches on directory server");
	    q_status_message(SM_ORDER, 3, 5, wp_err->error);
	    display_message('x');
	}

	break;
    }

    fs_give((void **)&ac.title);

    return(retval);
}


ADDRESS *
address_from_ldap(winning_e)
    LDAP_SERV_RES_S *winning_e;
{
    ADDRESS *ret_a = NULL;

    if(winning_e){
	char       *a;
	BerElement *ber;

	ret_a = mail_newaddr();
	for(a = ldap_first_attribute(winning_e->ld, winning_e->res, &ber);
	    a != NULL;
	    a = ldap_next_attribute(winning_e->ld, winning_e->res, ber)){
	    int i;
	    char  *p;
	    char **vals;

	    dprint(9, (debugfile, "attribute: %s\n", a ? a : "?"));
	    if(!ret_a->personal &&
	       strcmp(a, winning_e->info_used->cnattr) == 0){
		dprint(9, (debugfile, "Got cnattr:"));
		vals = ldap_get_values(winning_e->ld, winning_e->res, a);
		for(i = 0; vals[i] != NULL; i++)
		  dprint(9, (debugfile, "       %s\n",
		         vals[i] ? vals[i] : "?"));
	    
		if(vals && vals[0])
		  ret_a->personal = cpystr(vals[0]);

		ldap_value_free(vals);
	    }
	    else if(!ret_a->mailbox &&
		    strcmp(a, winning_e->info_used->mailattr) == 0){
		dprint(9, (debugfile, "Got mailattr:"));
		vals = ldap_get_values(winning_e->ld, winning_e->res, a);
		for(i = 0; vals[i] != NULL; i++)
		  dprint(9, (debugfile, "         %s\n",
		         vals[i] ? vals[i] : "?"));
		    
		/* use first one */
		if(vals && vals[0]){
		    if((p = strindex(vals[0], '@')) != NULL){
			ret_a->host = cpystr(p+1);
			*p = '\0';
		    }

		    ret_a->mailbox = cpystr(vals[0]);
		}

		ldap_value_free(vals);
	    }

	    our_ldap_memfree(a);
	}
    }

    return(ret_a);
}


/*
 * Break up the ldap-server string stored in the pinerc into its
 * parts. The structure is allocated here and should be freed by the caller.
 *
 * The original string looks like
 *     <servername>[:port] <SPACE> "/base=<base>/impl=1/..."
 *
 * Args  serv_str -- The original string from the pinerc to parse.
 *
 * Returns A pointer to a structure with filled in answers.
 *
 *  Some of the members have defaults. If port is -1, that means to use
 *  the default LDAP_PORT. If base is NULL, use "". Type and srch have
 *  defaults defined in pine.h. If cust is non-NULL, it overrides type and
 *  srch.
 */
LDAP_SERV_S *
break_up_ldap_server(serv_str)
    char        *serv_str;
{
    char *lserv;
    char *q, *p, *tail;
    int   i, only_one = 1;
    LDAP_SERV_S *info = NULL;

    if(!serv_str)
      return(info);

    info = (LDAP_SERV_S *)fs_get(sizeof(LDAP_SERV_S));

    /*
     * Initialize to defaults.
     */
    memset((void *)info, 0, sizeof(*info));
    info->port  = -1;
    info->srch  = -1;
    info->type  = -1;
    info->time  = -1;
    info->size  = -1;
    info->scope = -1;

    /* copy the whole string to work on */
    lserv = cpystr(serv_str);
    if(lserv)
      removing_trailing_white_space(lserv);

    if(!lserv || !*lserv || *lserv == '"'){
	if(lserv)
	  fs_give((void **)&lserv);

	if(info)
	  free_ldap_server_info(&info);

	return(NULL);
    }

    tail = lserv;
    while((tail = strindex(tail, SPACE)) != NULL){
	tail++;
	if(*tail == '"' || *tail == '/'){
	    *(tail-1) = '\0';
	    break;
	}
	else
	  only_one = 0;
    }

    /* tail is the part after server[:port] <SPACE> */
    if(tail && *tail){
	removing_leading_white_space(tail);
	(void)removing_double_quotes(tail);
    }

    /* get the optional port number */
    if(only_one && (q = strindex(lserv, ':')) != NULL){
	int ldapport = -1;

	*q = '\0';
	if((ldapport = atoi(q+1)) >= 0)
	  info->port = ldapport;
    }

    /* use lserv for serv even though it has a few extra bytes alloced */
    info->serv = lserv;
    
    if(tail && *tail){
	/* get the search base */
	if((q = srchstr(tail, "/base=")) != NULL)
	  info->base = remove_backslash_escapes(q+6);

	/* get the implicit parameter */
	if((q = srchstr(tail, "/impl=1")) != NULL)
	  info->impl = 1;

	/* get the rhs parameter */
	if((q = srchstr(tail, "/rhs=1")) != NULL)
	  info->rhs = 1;

	/* get the ref parameter */
	if((q = srchstr(tail, "/ref=1")) != NULL)
	  info->ref = 1;

	/* get the nosub parameter */
	if((q = srchstr(tail, "/nosub=1")) != NULL)
	  info->nosub = 1;

	/* get the ldap_v3_ok parameter */
	if((q = srchstr(tail, "/ldap_v3_ok=1")) != NULL)
	  info->ldap_v3_ok = 1;

	/* get the search type value */
	if((q = srchstr(tail, "/type=")) != NULL){
	    NAMEVAL_S *v;

	    q += 6;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    for(i = 0; v = ldap_search_types(i); i++)
	      if(!strucmp(q, v->name)){
		  info->type = v->value;
		  break;
	      }
	    
	    if(p)
	      *p = '/';
	}

	/* get the search rule value */
	if((q = srchstr(tail, "/srch=")) != NULL){
	    NAMEVAL_S *v;

	    q += 6;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    for(i = 0; v = ldap_search_rules(i); i++)
	      if(!strucmp(q, v->name)){
		  info->srch = v->value;
		  break;
	      }
	    
	    if(p)
	      *p = '/';
	}

	/* get the scope */
	if((q = srchstr(tail, "/scope=")) != NULL){
	    NAMEVAL_S *v;

	    q += 7;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    for(i = 0; v = ldap_search_scope(i); i++)
	      if(!strucmp(q, v->name)){
		  info->scope = v->value;
		  break;
	      }
	    
	    if(p)
	      *p = '/';
	}

	/* get the time limit */
	if((q = srchstr(tail, "/time=")) != NULL){
	    q += 6;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    /* This one's a number */
	    if(*q){
		char *err;

		err = strtoval(q, &i, 0, 500, 0, tmp_20k_buf, "ldap timelimit");
		if(err){
		  dprint(1, (debugfile, "%s\n", err ? err : "?"));
		}
		else
		  info->time = i;
	    }

	    if(p)
	      *p = '/';
	}

	/* get the size limit */
	if((q = srchstr(tail, "/size=")) != NULL){
	    q += 6;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    /* This one's a number */
	    if(*q){
		char *err;

		err = strtoval(q, &i, 0, 500, 0, tmp_20k_buf, "ldap sizelimit");
		if(err){
		  dprint(1, (debugfile, "%s\n", err ? err : "?"));
		}
		else
		  info->size = i;
	    }

	    if(p)
	      *p = '/';
	}

	/* get the custom search filter */
	if((q = srchstr(tail, "/cust=")) != NULL)
	  info->cust = remove_backslash_escapes(q+6);

	/* get the nickname */
	if((q = srchstr(tail, "/nick=")) != NULL)
	  info->nick = remove_backslash_escapes(q+6);

	/* get the mail attribute name */
	if((q = srchstr(tail, "/matr=")) != NULL)
	  info->mailattr = remove_backslash_escapes(q+6);

	/* get the sn attribute name */
	if((q = srchstr(tail, "/satr=")) != NULL)
	  info->snattr = remove_backslash_escapes(q+6);

	/* get the gn attribute name */
	if((q = srchstr(tail, "/gatr=")) != NULL)
	  info->gnattr = remove_backslash_escapes(q+6);

	/* get the cn attribute name */
	if((q = srchstr(tail, "/catr=")) != NULL)
	  info->cnattr = remove_backslash_escapes(q+6);

	/* get the backup mail address */
	if((q = srchstr(tail, "/mail=")) != NULL)
	  info->mail = remove_backslash_escapes(q+6);
    }

    return(info);
}


void
free_ldap_server_info(info)
LDAP_SERV_S **info;
{
    if(info && *info){
	if((*info)->serv)
	  fs_give((void **)&(*info)->serv);

	if((*info)->base)
	  fs_give((void **)&(*info)->base);

	if((*info)->cust)
	  fs_give((void **)&(*info)->cust);

	if((*info)->nick)
	  fs_give((void **)&(*info)->nick);

	if((*info)->mail)
	  fs_give((void **)&(*info)->mail);

	if((*info)->mailattr)
	  fs_give((void **)&(*info)->mailattr);

	if((*info)->snattr)
	  fs_give((void **)&(*info)->snattr);

	if((*info)->gnattr)
	  fs_give((void **)&(*info)->gnattr);

	if((*info)->cnattr)
	  fs_give((void **)&(*info)->cnattr);

	fs_give((void **)info);
	*info = NULL;
    }
}


LDAP_SERV_S *
copy_ldap_serv_info(src)
LDAP_SERV_S *src;
{
    LDAP_SERV_S *info = NULL;

    if(src){
	info = (LDAP_SERV_S *)fs_get(sizeof(LDAP_SERV_S));

	/*
	 * Initialize to defaults.
	 */
	memset((void *)info, 0, sizeof(*info));

	info->serv     = src->serv ? cpystr(src->serv) : NULL;
	info->base     = src->base ? cpystr(src->base) : NULL;
	info->cust     = src->cust ? cpystr(src->cust) : NULL;
	info->nick     = src->nick ? cpystr(src->nick) : NULL;
	info->mail     = src->mail ? cpystr(src->mail) : NULL;
	info->mailattr = cpystr((src->mailattr && src->mailattr[0])
			    ? src->mailattr : DEF_LDAP_MAILATTR);
	info->snattr = cpystr((src->snattr && src->snattr[0])
			    ? src->snattr : DEF_LDAP_SNATTR);
	info->gnattr = cpystr((src->gnattr && src->gnattr[0])
			    ? src->gnattr : DEF_LDAP_GNATTR);
	info->cnattr = cpystr((src->cnattr && src->cnattr[0])
			    ? src->cnattr : DEF_LDAP_CNATTR);

	info->port  = (src->port < 0)  ? LDAP_PORT      : src->port;
	info->time  = (src->time < 0)  ? DEF_LDAP_TIME  : src->time;
	info->size  = (src->size < 0)  ? DEF_LDAP_SIZE  : src->size;
	info->type  = (src->type < 0)  ? DEF_LDAP_TYPE  : src->type;
	info->srch  = (src->srch < 0)  ? DEF_LDAP_SRCH  : src->srch;
	info->scope = (src->scope < 0) ? DEF_LDAP_SCOPE : src->scope;
	info->impl  = src->impl;
	info->rhs   = src->rhs;
	info->ref   = src->ref;
	info->nosub = src->nosub;
	info->ldap_v3_ok = src->ldap_v3_ok;
    }

    return(info);
}


void
free_ldap_result_list(head)
    LDAP_SERV_RES_S *head;
{
    LDAP_SERV_RES_S *rr, *next;

    for(rr = head; rr; rr = next){
	if(rr->res)
	  ldap_msgfree(rr->res);
	if(rr->ld)
	  ldap_unbind(rr->ld);
	if(rr->info_used)
	  free_ldap_server_info(&rr->info_used);
	if(rr->serv)
	  fs_give((void **)&rr->serv);
	
	next = rr->next;
	fs_give((void **)&rr);
    }
}


/*
 * Mask API differences. 
 */
void
our_ldap_memfree(a)
    void *a;
{
#if (LDAPAPI >= 15)
    if(a)
      ldap_memfree(a);
#endif
}


/*
 * Mask API differences.
 */
void
our_ldap_dn_memfree(a)
    void *a;
{
#if defined(_WINDOWS)
    if(a)
      ldap_memfree(a);
#else
#if (LDAPAPI >= 15)
    if(a)
      ldap_memfree(a);
#else
    if(a)
      free(a);
#endif
#endif
}


/*
 * More API masking.
 */
int
our_ldap_get_lderrno(ld, m, s)
    LDAP  *ld;
    char **m;
    char **s;
{
    int ret = 0;

#if (LDAPAPI >= 2000)
    if(ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, (void *)&ret) == 0){
	if(s)
	  ldap_get_option(ld, LDAP_OPT_ERROR_STRING, (void *)s);
    }
#elif (LDAPAPI >= 15)
    ret = ldap_get_lderrno(ld, m, s);
#else
    ret = ld->ld_errno;
    if(s)
      *s = ld->ld_error;
#endif

    return(ret);
}


/*
 * More API masking.
 */
int
our_ldap_set_lderrno(ld, e, m, s)
    LDAP *ld;
    int   e;
    char *m;
    char *s;
{
    int ret;

#if (LDAPAPI >= 2000)
    if(ldap_set_option(ld, LDAP_OPT_ERROR_NUMBER, (void *)&e) == 0)
      ret = LDAP_SUCCESS;
    else
      (void)ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, (void *)&ret);
#elif (LDAPAPI >= 15)
    ret = ldap_set_lderrno(ld, e, m, s);
#else
    /* this is all we care about */
    ld->ld_errno = e;
    ret = LDAP_SUCCESS;
#endif

    return(ret);
}


/*
 * More API masking.
 */
int
our_ldap_set_option(ld, option, optdata)
    LDAP *ld;
    int   option;
    void *optdata;
{
    int ret;

#if (LDAPAPI >= 15)
    ret = ldap_set_option(ld, option, optdata);
#else
    switch(option){
      case LDAP_OPT_TIMELIMIT:
	ld->ld_timelimit = *(int *)optdata;
	break;
	
      case LDAP_OPT_SIZELIMIT:
	ld->ld_sizelimit = *(int *)optdata;
	break;
    
      case LDAP_OPT_RESTART:
	if((int)optdata)
	  ld->ld_options |= LDAP_OPT_RESTART;
	else
	  ld->ld_options &= ~LDAP_OPT_RESTART;

	break;
    
      /*
       * Does nothing here. There is only one protocol version supported.
       */
      case LDAP_OPT_PROTOCOL_VERSION:
	ret = -1;
	break;
    
      default:
	panic("LDAP function not implemented");
    }
#endif

    return(ret);
}


/*
 * Returns 1 if we can use LDAP version 3 protocol.
 */
int
ldap_v3_is_supported(ld, info)
    LDAP        *ld;
    LDAP_SERV_S *info;
{
    int         v3_is_supported_by_server = 0;

    if(info && info->ldap_v3_ok)
      return(1);

#ifdef NO_VERSION3_PROTO_YET
/*
 * When we use version 3 protocol we will be getting back utf8 results.
 * That means that we need some way to convert from utf8 to whatever our
 * display needs. We don't have that yet. We could do the ugly hack of
 * assuming it is 8859-1, but that's too ugly to think about. We could
 * come up with an ldap-charset variable and use it to do the conversion.
 * That's reasonable, but since the effort is non-trivial and we eventually
 * want to support a display character set more generally (not just when
 * displaying ldap results), we are going to put this off for a while.
 * So comment out the next part which is just determining whether the
 * server supports v3 and telling the server we would like to talk v3.
 */
    LDAPVersion ver;
    int         our_proto_vers;

    /*
     * It shouldn't really be necessary to figure out that _we_ support
     * version 3, we know we do if LDAPAPI >= 15. But we're just making sure.
     */
    (void)ldap_version(&ver);
    our_proto_vers = (int)(ver.protocol_version/100.0);
    /* we can talk LDAPv3 instead of LDAPv2, but can server? */
    if(our_proto_vers > 2){
	LDAPMessage *result, *e;
	int          i;
	char        *attrs[2];
	char        *a;
	char       **vals;
	BerElement  *ber;

	attrs[0] = "supportedLDAPVersion";
	attrs[1] = NULL;

	/*
	 * Figure out if the server supports v3. This code is derived from
	 * the code in the Netscape SDK programmer's manual.
	 *
	 * Set automatic referral processing off,
	 * search for the root DSE,
	 * get the supportedLDAPVersion attribute,
	 * and see if version "3" is supported.
	 */
	if(our_ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF) == 0 &&
	   ldap_search_ext_s(ld, "", LDAP_SCOPE_BASE, "(objectclass=*)", attrs,
			     0, NULL, NULL, NULL, 0, &result) == LDAP_SUCCESS){
	    /* only one entry should have matched, get that entry */
	    if((e = ldap_first_entry(ld, result)) != NULL &&
	       (a = ldap_first_attribute(ld, e, &ber)) != NULL &&
	       (vals = ldap_get_values(ld, e, a)) != NULL){
		for(i = 0; vals[i] != NULL; i++){
		    if(!strcmp("3", vals[i])){
		      v3_is_supported_by_server++;
		      break;
		    }
		}

		/* free memory */
		ldap_value_free(vals);
		our_ldap_memfree(a);
		if(ber)
		  ber_free(ber, 0);
	    }

	    ldap_msgfree(result);
	}

	/* Turn automatic referral processing back on. */
	(void)our_ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_ON);
    }
#endif /* NO_VERSION3_PROTO_YET */

    return(v3_is_supported_by_server);
}
#endif	/* ENABLE_LDAP */


/*
 * Run through the adrlist "adr" and strip off any enclosing quotes
 * around personal names.  That is, change "Joe L. Normal" to
 * Joe L. Normal.
 */
void
strip_personal_quotes(adr)
    ADDRESS *adr;
{
    int   len;
    register char *p, *q;

    while(adr){
	if(adr->personal){
	    len = strlen(adr->personal);
	    if(len > 1
	       && adr->personal[0] == '"'
	       && adr->personal[len-1] == '"'){
		adr->personal[len-1] = '\0';
		p = adr->personal;
		q = p + 1;
		while(*p++ = *q++)
		  ;
	    }
	}

	adr = adr->next;
    }
}


/*
 * Given an address, try to find the first nickname that goes with it.
 * Copies that nickname into the passed in buffer, which is assumed to
 * be at least MAX_NICKNAME+1 in length.  Returns NULL if it can't be found,
 * else it returns a pointer to the buffer.
 */
char *
get_nickname_from_addr(adr, buffer, buflen)
    ADDRESS *adr;
    char    *buffer;
    size_t   buflen;
{
    AdrBk_Entry *abe;
    char        *ret = NULL;
    SAVE_STATE_S state;
    jmp_buf	 save_jmp_buf;
    int         *save_nesting_level;
    ADDRESS     *copied_adr;

    state.savep = NULL;
    state.stp = NULL;
    state.dlc_to_warp_to = NULL;
    copied_adr = copyaddr(adr);

    if(ps_global->remote_abook_validity > 0)
      (void)adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0);

    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	ret = NULL;
	if(state.savep)
	  fs_give((void **)&(state.savep));
	if(state.stp)
	  fs_give((void **)&(state.stp));
	if(state.dlc_to_warp_to)
	  fs_give((void **)&(state.dlc_to_warp_to));

	q_status_message(SM_ORDER, 3, 5, "Resetting address book...");
	dprint(1, (debugfile,
	    "RESETTING address book... get_nickname_from_addr()!\n"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    init_ab_if_needed();
    save_state(&state);

    abe = address_to_abe(copied_adr);

    if(copied_adr)
      mail_free_address(&copied_adr);

    if(abe && abe->nickname && abe->nickname[0]){
	strncpy(buffer, abe->nickname, buflen-1);
	buffer[buflen-1] = '\0';
	ret = buffer;
    }

    restore_state(&state);
    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    ab_nesting_level--;

    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    return(ret);
}


/*
 * Given an address, try to find the first fcc that goes with it.
 * Copies that fcc into the passed in buffer, which is assumed to
 * be at least MAXFOLDER+1 in length.  Returns NULL if it can't be found,
 * else it returns a pointer to the buffer.
 */
char *
get_fcc_from_addr(adr, buffer, buflen)
    ADDRESS *adr;
    char    *buffer;
    size_t   buflen;
{
    AdrBk_Entry *abe;
    char        *ret = NULL;
    SAVE_STATE_S state;
    jmp_buf	 save_jmp_buf;
    int         *save_nesting_level;
    ADDRESS     *copied_adr;

    state.savep = NULL;
    state.stp = NULL;
    state.dlc_to_warp_to = NULL;
    copied_adr = copyaddr(adr);

    if(ps_global->remote_abook_validity > 0)
      (void)adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0);

    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	ret = NULL;
	if(state.savep)
	  fs_give((void **)&(state.savep));
	if(state.stp)
	  fs_give((void **)&(state.stp));
	if(state.dlc_to_warp_to)
	  fs_give((void **)&(state.dlc_to_warp_to));

	q_status_message(SM_ORDER, 3, 5, "Resetting address book...");
	dprint(1, (debugfile,
	    "RESETTING address book... get_fcc_from_addr()!\n"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    init_ab_if_needed();
    save_state(&state);

    abe = address_to_abe(copied_adr);

    if(copied_adr)
      mail_free_address(&copied_adr);

    if(abe && abe->fcc && abe->fcc[0]){
	if(!strcmp(abe->fcc, "\"\""))
	  buffer[0] = '\0';
	else{
	    strncpy(buffer, abe->fcc, buflen-1);
	    buffer[buflen-1] = '\0';
	}

	ret = buffer;
    }

    restore_state(&state);
    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    ab_nesting_level--;

    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    return(ret);
}


/*
 * This is a very special-purpose routine.
 * It implements the From or Reply-To address is in the Address Book
 * part of Pattern matching.
 */
void
from_or_replyto_in_abook(stream, searchset, abookfrom, abooks)
    MAILSTREAM *stream;
    SEARCHSET  *searchset;
    int         abookfrom;
    PATTERN_S  *abooks;
{
    char         *savebits;
    MESSAGECACHE *mc;
    long          i;
    SEARCHSET    *s, *ss, **sset;
    ADDRESS      *from, *reply_to;
    int           is_there, adrbknum, *abooklist = NULL, positive_match;
    PATTERN_S    *pat;
    PerAddrBook  *pab;
    ENVELOPE     *e;
    SAVE_STATE_S  state;
    jmp_buf	  save_jmp_buf;
    int          *save_nesting_level;

    if(!stream)
      return;

    /* everything that matches remains a match */
    if(abookfrom == AFRM_EITHER)
      return;

    state.savep          = NULL;
    state.stp            = NULL;
    state.dlc_to_warp_to = NULL;

    /*
     * This may call build_header_line recursively because we may be in
     * build_header_line now. So we have to preserve and restore the
     * sequence bits since we want to use them here.
     */
    savebits = (char *) fs_get((stream->nmsgs+1) * sizeof(char));

    for(i = 1L; i <= stream->nmsgs; i++){
	if((mc = mail_elt(stream, i)) != NULL){
	    savebits[i] = mc->sequence;
	    mc->sequence = 0;
	}
    }

    /*
     * Build a searchset so we can look at all the envelopes
     * we need to look at but only those we need to look at.
     * Everything with the searched bit set is still a
     * possibility, so restrict to that set.
     */

    for(s = searchset; s; s = s->next)
      for(i = s->first; i <= s->last; i++)
	if(i > 0L && i <= stream->nmsgs
	   && (mc=mail_elt(stream, i)) && mc->searched)
	  mc->sequence = 1;

    ss = build_searchset(stream);

    /*
     * We save the address book state here so we don't have to do it
     * each time through the loop below.
     */
    if(ss){
	if(ps_global->remote_abook_validity > 0)
	  (void)adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0);

	save_nesting_level = cpyint(ab_nesting_level);
	memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
	if(setjmp(addrbook_changed_unexpectedly)){
	    if(state.savep)
	      fs_give((void **)&(state.savep));
	    if(state.stp)
	      fs_give((void **)&(state.stp));
	    if(state.dlc_to_warp_to)
	      fs_give((void **)&(state.dlc_to_warp_to));

	    q_status_message(SM_ORDER, 3, 5, "Resetting address book...");
	    dprint(1, (debugfile,
		"RESETTING address book... from_or_replyto()!\n"));
	    addrbook_reset();
	    ab_nesting_level = *save_nesting_level;
	}

	ab_nesting_level++;
	init_ab_if_needed();
	save_state(&state);

	if(as.n_addrbk > 0){
	    abooklist = (int *) fs_get(as.n_addrbk * sizeof(*abooklist));
	    memset((void *) abooklist, 0, as.n_addrbk * sizeof(*abooklist));
	}

	if(abooklist)
	  switch(abookfrom){
	    case AFRM_YES:
	    case AFRM_NO:
	      for(adrbknum = 0; adrbknum < as.n_addrbk; adrbknum++)
	        abooklist[adrbknum] = 1;

	      break;

	    case AFRM_SPEC_YES:
	    case AFRM_SPEC_NO:
	      /* figure out which address books we're going to look in */
	      for(adrbknum = 0; adrbknum < as.n_addrbk; adrbknum++){
		  pab = &as.adrbks[adrbknum];
		  /*
		   * For each address book, check all of the address books
		   * in the pattern's list to see if they are it.
		   */
		  for(pat = abooks; pat; pat = pat->next){
		      if(!strcmp(pab->nickname, pat->substring)
			 || !strcmp(pab->filename, pat->substring)){
			  abooklist[adrbknum] = 1;
			  break;
		      }
		  }
	      }

	      break;
	   }

	switch(abookfrom){
	  case AFRM_YES:
	  case AFRM_SPEC_YES:
	    positive_match = 1;
	    break;

	  case AFRM_NO:
	  case AFRM_SPEC_NO:
	    positive_match = 0;
	    break;
	}
    }

    for(s = ss; s; s = s->next){
	for(i = s->first; i <= s->last; i++){
	    if(i <= 0L || i > stream->nmsgs)
	      continue;

	    /*
	     * This causes the lookahead to fetch precisely
	     * the messages we want (in the searchset) instead
	     * of just fetching the next 20 sequential
	     * messages. If the searching so far has caused
	     * a sparse searchset in a large mailbox, the
	     * difference can be substantial.
	     */
	    sset = (SEARCHSET **) mail_parameters(stream,
						  GET_FETCHLOOKAHEAD,
						  (void *) stream);
	    if(sset)
	      *sset = s;

	    e = pine_mail_fetchenvelope(stream, i);

	    from = e ? e->from : NULL;
	    reply_to = e ? e->reply_to : NULL;

	    is_there = 0;
	    for(adrbknum = 0; !is_there && adrbknum < as.n_addrbk; adrbknum++){
		if(!abooklist[adrbknum])
		  continue;
		
		pab = &as.adrbks[adrbknum];
		is_there = addr_is_in_addrbook(pab, from, reply_to);
	    }

	    if(positive_match){
		/*
		 * We matched up until now. If it isn't there, then it
		 * isn't a match. If it is there, leave the searched bit
		 * set.
		 */
		if(!is_there && i > 0L && i <= stream->nmsgs
		   && (mc = mail_elt(stream, i)))
		  mc->searched = NIL;
	    }
	    else{
		if(is_there && i > 0L && i <= stream->nmsgs
		   && (mc = mail_elt(stream, i)))
		  mc->searched = NIL;
	    }
	}
    }

    if(ss){
	restore_state(&state);
	memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
	ab_nesting_level--;

	if(save_nesting_level)
	  fs_give((void **)&save_nesting_level);
    }

    /* restore sequence bits */
    for(i = 1L; i <= stream->nmsgs; i++)
      if((mc = mail_elt(stream, i)) != NULL)
        mc->sequence = savebits[i];

    fs_give((void **) &savebits);

    if(ss)
      mail_free_searchset(&ss);

    if(abooklist)
      fs_give((void **) &abooklist);
}


/*
 * Given two addresses, check to see if either is in the address book.
 * Returns 1 if yes, 0 if not found.
 */
int
addr_is_in_addrbook(pab, adr1, adr2)
    PerAddrBook *pab;
    ADDRESS     *adr1,
                *adr2;
{
    AdrBk_Entry *abe = NULL;
    int          ret = 0;
    char         abuf[MAX_ADDR_FIELD+1];

    if(!(pab && (adr1 && adr1->mailbox || adr2 && adr2->mailbox)))
      return(ret);

    if(adr1){
	strncpy(abuf, adr1->mailbox, MAX_ADDR_FIELD);
	abuf[MAX_ADDR_FIELD] = '\0';
	if(adr1->host && adr1->host[0]){
	    strncat(abuf, "@", MAX_ADDR_FIELD-strlen(abuf));
	    strncat(abuf, adr1->host, MAX_ADDR_FIELD-strlen(abuf));
	}

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);
	
	abe = adrbk_lookup_by_addr(pab->address_book, abuf,
				   (adrbk_cntr_t *) NULL);
    }

    if(!abe && adr2 && !address_is_same(adr1, adr2)){
	strncpy(abuf, adr2->mailbox, MAX_ADDR_FIELD);
	abuf[MAX_ADDR_FIELD] = '\0';
	if(adr2->host && adr2->host[0]){
	    strncat(abuf, "@", MAX_ADDR_FIELD-strlen(abuf));
	    strncat(abuf, adr2->host, MAX_ADDR_FIELD-strlen(abuf));
	}

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);
	
	abe = adrbk_lookup_by_addr(pab->address_book, abuf,
				   (adrbk_cntr_t *) NULL);
    }

    ret = abe ? 1 : 0;

    return(ret);
}


static char *last_fcc_used;
/*
 * Returns alloc'd fcc.
 */
char *
get_fcc(fcc_arg)
    char *fcc_arg;
{
    char *fcc;

    /*
     * Use passed in arg unless it is the same as default (and then
     * may use that anyway below).
     */
    if(fcc_arg && strcmp(fcc_arg, ps_global->VAR_DEFAULT_FCC))
      fcc = cpystr(fcc_arg);
    else{
	if(ps_global->fcc_rule == FCC_RULE_LAST && last_fcc_used)
	  fcc = cpystr(last_fcc_used);
	else if(ps_global->fcc_rule == FCC_RULE_CURRENT
		&& ps_global->mail_stream
		&& !sp_flagged(ps_global->mail_stream, SP_INBOX)
		&& !IS_NEWS(ps_global->mail_stream)
		&& ps_global->cur_folder
		&& ps_global->cur_folder[0]){
	    CONTEXT_S *cntxt = ps_global->context_current;
	    char *rs = NULL;

	    if(((cntxt->use) & CNTXT_SAVEDFLT))
	      rs = ps_global->cur_folder;
	    else
	      rs = ps_global->mail_stream->mailbox;

	    fcc = cpystr((rs&&*rs) ? rs : ps_global->VAR_DEFAULT_FCC);
	}
	else
	  fcc = cpystr(ps_global->VAR_DEFAULT_FCC);
    }
    
    return(fcc);
}


/*
 * Save the fcc for use with next composition.
 */
void
set_last_fcc(fcc)
    char *fcc;
{
    if(fcc){
	if(!last_fcc_used)
	  last_fcc_used = cpystr(fcc);
	else if(strcmp(last_fcc_used, fcc)){
	    if(strlen(last_fcc_used) >= strlen(fcc))
	      strcpy(last_fcc_used, fcc);
	    else{
		fs_give((void **)&last_fcc_used);
		last_fcc_used = cpystr(fcc);
	    }
	}
    }
}


/*
 * Figure out what the fcc is based on the given to address.
 *
 * Returns an allocated copy of the fcc, to be freed by the caller, or NULL.
 */
char *
get_fcc_based_on_to(to)
    ADDRESS *to;
{
    ADDRESS    *next_addr;
    char       *str, *bufp;
    BUILDER_ARG barg;

    if(!to || !to->host || to->host[0] == '.')
      return(NULL);

    /* pick off first address */
    next_addr = to->next;
    to->next = NULL;
    bufp = (char *)fs_get((size_t)est_size(to));
    bufp[0] = '\0';
    str = cpystr(addr_string(to, bufp));
    fs_give((void **)&bufp);
    to->next = next_addr;
    memset((void *)&barg, 0, sizeof(barg));
    (void)build_address(str, NULL, NULL, &barg, NULL);

    if(str)
      fs_give((void **)&str);
    
    return(barg.tptr);
}


/*
 * Free storage in headerentry.bldr_private.
 */
void
free_privatetop(pt)
    PrivateTop **pt;
{
    if(pt && *pt){
	if((*pt)->affector)
	  fs_give((void **)&(*pt)->affector);
	
	free_privateencoded(&(*pt)->encoded);
	fs_give((void **)pt);
    }
}


void
free_privateencoded(enc)
    PrivateEncoded **enc;
{
    if(enc && *enc){
	if((*enc)->etext)
	  fs_give((void **)&(*enc)->etext);
	
	fs_give((void **)enc);
    }
}


/*
 * Look through addrbooks for nickname, opening addrbooks first
 * if necessary.  It is assumed that the caller will restore the
 * state of the addrbooks if desired.
 *
 * Args: nickname       -- the nickname to lookup
 *       clearrefs      -- clear reference bits before lookup
 *       which_addrbook -- If matched, addrbook number it was found in.
 *       not_here       -- If non-negative, skip looking in this abook.
 *
 * Results: A pointer to an AdrBk_Entry is returned, or NULL if not found.
 * Stop at first match (so order of addrbooks is important).
 */
AdrBk_Entry *
adrbk_lookup_with_opens_by_nick(nickname, clearrefs, which_addrbook, not_here)
    char *nickname;
    int   clearrefs;
    int  *which_addrbook;
    int   not_here;
{
    AdrBk_Entry *abe = (AdrBk_Entry *)NULL;
    int i;
    PerAddrBook *pab;

    dprint(5, (debugfile, "- adrbk_lookup_with_opens_by_nick(%s) -\n",
	nickname ? nickname : "?"));

    for(i = 0; i < as.n_addrbk; i++){

	if(i == not_here)
	  continue;

	pab = &as.adrbks[i];

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);

	if(clearrefs)
	  adrbk_clearrefs(pab->address_book);

	abe = adrbk_lookup_by_nick(pab->address_book,
		    nickname,
		    (adrbk_cntr_t *)NULL);
	if(abe)
	  break;
    }

    if(abe && which_addrbook)
      *which_addrbook = i;

    return(abe);
}


/*
 * Find the addressbook entry that matches the argument address.
 * Searches through all addressbooks looking for the match.
 * Opens addressbooks if necessary.  It is assumed that the caller
 * will restore the state of the addrbooks if desired.
 *
 * Args: addr -- the address we're trying to match
 *
 * Returns:  NULL -- no match found
 *           abe  -- a pointer to the addrbook entry that matches
 */
AdrBk_Entry *
address_to_abe(addr)
    ADDRESS *addr;
{
    register PerAddrBook *pab;
    int adrbk_number;
    AdrBk_Entry *abe = NULL;
    char *abuf = NULL;

    if(!(addr && addr->mailbox))
      return (AdrBk_Entry *)NULL;

    abuf = (char *)fs_get((size_t)(MAX_ADDR_FIELD + 1));

    strncpy(abuf, addr->mailbox, MAX_ADDR_FIELD);
    abuf[MAX_ADDR_FIELD] = '\0';
    if(addr->host && addr->host[0]){
	strncat(abuf, "@", MAX_ADDR_FIELD-strlen(abuf));
	strncat(abuf, addr->host, MAX_ADDR_FIELD-strlen(abuf));
    }

    /* for each addressbook */
    for(adrbk_number = 0; adrbk_number < as.n_addrbk; adrbk_number++){

	pab = &as.adrbks[adrbk_number];

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);

	abe = adrbk_lookup_by_addr(pab->address_book,
				   abuf,
				   (adrbk_cntr_t *)NULL);
	if(abe)
	  break;
    }

    if(abuf)
      fs_give((void **)&abuf);

    return(abe);
}


/*
 * Turn an AdrBk_Entry into an address list
 *
 * Args: abe      -- the AdrBk_Entry
 *       dl       -- the corresponding dl
 *       abook    -- which addrbook the abe is in (only used for type ListEnt)
 *       how_many -- The number of addresses is returned here
 *
 * Result:  allocated address list or NULL
 */
ADDRESS *
abe_to_address(abe, dl, abook, how_many)
    AdrBk_Entry   *abe;
    AddrScrn_Disp *dl;
    AdrBk         *abook;
    int           *how_many;
{
    char          *fullname, *tmp_a_string;
    char          *list, *l1, **l2;
    char          *fakedomain = "@";
    ADDRESS       *addr = NULL;
    size_t         length;
    int            count = 0;

    if(!dl || !abe)
      return(NULL);

    fullname = (abe->fullname && abe->fullname[0]) ? abe->fullname : NULL;

    switch(dl->type){
      case Simple:
	/* rfc822_parse_adrlist feels free to destroy input so send copy */
	tmp_a_string = cpystr(abe->addr.addr);
	rfc822_parse_adrlist(&addr, tmp_a_string, fakedomain);

	if(tmp_a_string)
	  fs_give((void **)&tmp_a_string);

	if(addr && fullname){
#ifdef	ENABLE_LDAP
	    if(strncmp(addr->mailbox, RUN_LDAP, LEN_RL) != 0)
#endif	/* ENABLE_LDAP */
	    {
		if(addr->personal)
		  fs_give((void **)&addr->personal);

		addr->personal = adrbk_formatname(fullname, NULL, NULL);
	    }
	}

	if(addr)
	  count++;

        break;

      case ListEnt:
	/* rfc822_parse_adrlist feels free to destroy input so send copy */
	tmp_a_string = cpystr(listmem_from_dl(abook, dl));
	rfc822_parse_adrlist(&addr, tmp_a_string, fakedomain);
	if(tmp_a_string)
	  fs_give((void **)&tmp_a_string);

	if(addr)
	  count++;

        break;

      case ListHead:
	length = 0;
	for(l2 = abe->addr.list; *l2; l2++)
	  length += (strlen(*l2) + 1);

	list = (char *)fs_get(length + 1);
	l1 = list;
	for(l2 = abe->addr.list; *l2; l2++){
	    if(l1 != list)
	      *l1++ = ',';
	    strcpy(l1, *l2);
	    l1 += strlen(l1);
	    count++;
	}

	rfc822_parse_adrlist(&addr, list, fakedomain);
	if(list)
	  fs_give((void **)&list);

        break;
      
      default:
	dprint(1,
	   (debugfile, "default case in abe_to_address, shouldn't happen\n"));
	break;
    } 

    if(how_many)
      *how_many = count;

    return(addr);
}


/*
 * Turn an AdrBk_Entry into a nickname (if it has a nickname) or a
 * formatted addr_string which has been rfc1522 decoded.
 *
 * Args: abe      -- the AdrBk_Entry
 *       dl       -- the corresponding dl
 *       abook    -- which addrbook the abe is in (only used for type ListEnt)
 *
 * Result:  allocated string is returned
 */
char *
abe_to_nick_or_addr_string(abe, dl, abook_num)
    AdrBk_Entry   *abe;
    AddrScrn_Disp *dl;
    int            abook_num;
{
    ADDRESS       *addr;
    char          *a_string;
    int            return_nick = 1;

    if(!dl || !abe)
      return(cpystr(""));

    if(!(dl->type == Simple || dl->type == ListHead)
       || !(abe->nickname && abe->nickname[0]))
      return_nick = 0;

    if(return_nick){
	char *fname = NULL;
	AdrBk_Entry *found_abe = NULL;
	int which_addrbook, found_it;

	/*
	 * We may be able to return the nickname instead of the
	 * whole address.
	 *
	 * We prefer to pass back the nickname since that allows the
	 * caller to keep track of which entry the address came from.
	 * This is useful in build_address so that the fcc line can
	 * be kept correct. However, if the nickname appears in the
	 * addressbooks more than once then we have to be careful.
	 * If the other entry with the same nickname is an in
	 * addressbook that comes before this one then passing back
	 * the nickname will cause the wrong entry to get used.
	 * So check for that and pass back the addr_string in that case.
	 * Also, if the other entry with the same nickname is in this
	 * same addressbook and comes before this entry, same deal.
	 */
	fname = addr_lookup(abe->nickname, &which_addrbook, -1, &found_abe);
	found_it = (fname != NULL);
	if(fname)
	  fs_give((void **) &fname);

	if(found_it && which_addrbook < abook_num)
	  return_nick = 0;

	/* if the returned abe is not the same as abe don't use the nickname */
	if(return_nick && found_it && which_addrbook == abook_num){
	    if(found_abe
	       && ((found_abe->tag != abe->tag)
	           || (found_abe->fullname && !abe->fullname)
	           || (!found_abe->fullname && abe->fullname)
	           || strcmp(found_abe->fullname, abe->fullname)
	           || (found_abe->fcc && !abe->fcc)
	           || (!found_abe->fcc && abe->fcc)
	           || strcmp(found_abe->fcc, abe->fcc)
	           || (found_abe->extra && !abe->extra)
	           || (!found_abe->extra && abe->extra)
	           || strcmp(found_abe->extra, abe->extra)
	           || (abe->tag == Single && strcmp(found_abe->addr.addr ? found_abe->addr.addr : "", abe->addr.addr ? abe->addr.addr : ""))))
	      return_nick = 0;

	    /* I suppose we ought to check for the lists being the same */
	    if(return_nick && abe->tag == List && found_abe){
		char **p, **q;
		int    i, n1, n2;

		for(p = abe->addr.list; p && *p; p++)
		  ;

		if(p == NULL)
		  n1 = 0;
		else
		  n1 = p - abe->addr.list;

		for(p = found_abe->addr.list; p && *p; p++)
		  ;

		if(p == NULL)
		  n2 = 0;
		else
		  n2 = p - found_abe->addr.list;

		if(n1 == n2){
		    for(i = 0; i < n1 && return_nick; i++)
		      if(strcmp(abe->addr.list[i], found_abe->addr.list[i]))
		        return_nick = 0;
		}
		else
		  return_nick = 0;
	    }
	}

	if(found_abe)
	  free_ae(&found_abe);
    }

    if(return_nick)
      return(cpystr(abe->nickname));
    else{
	addr = abe_to_address(abe, dl, as.adrbks[abook_num].address_book, NULL);

	/* always returns a string */
	a_string = addr_list_string(addr, NULL, 0, 0);

	if(addr)
	  mail_free_address(&addr);
	
	return(a_string);
    }
}


/*
 * Turn a list of address structures into a formatted string
 *
 * Args: adrlist -- An adrlist
 *       f       -- Function to use to print one address in list.  If NULL,
 *                  use rfc822_write_address_decode to print whole list.
 *       verbose -- Include [charset] string if charset doesn't match ours.
 *      do_quote -- Quote quotes and dots (only used if f == NULL).
 * Result:  comma separated list of addresses which is
 *                                     malloced here and returned
 *		(the list is rfc1522 decoded unless f is *not* NULL)
 */
char *
addr_list_string(adrlist, f, verbose, do_quote)
    ADDRESS *adrlist;
    char    *(*f) PROTO((ADDRESS *, char *, size_t));
    int      verbose;
    int      do_quote;
{
    int               len;
    char             *list, *s, string[MAX_ADDR_EXPN+1];
    register ADDRESS *a;

    if(!adrlist)
      return(cpystr(""));
    
    if(f){
	len = 0;
	for(a = adrlist; a; a = a->next)
	  len += (strlen((*f)(a, string, sizeof(string))) + 2);

	list = (char *)fs_get((size_t)len);
	s    = list;
	s[0] = '\0';

	for(a = adrlist; a; a = a->next){
	    sstrcpy(&s, (*f)(a, string, sizeof(string)));
	    if(a->next){
		*s++ = ',';
		*s++ = SPACE;
	    }
	}
    }
    else{
	char *charset = NULL;

	list = (char *)fs_get((size_t)est_size(adrlist));
	list[0] = '\0';
	rfc822_write_address_decode(list, adrlist,
				    verbose ? NULL : &charset, do_quote);
	removing_leading_and_trailing_white_space(list);
	if(charset)
	  fs_give((void **)&charset);
    }

    return(list);
}


/*
 * Check to see if address is that of the current user running pine
 *
 * Args: a  -- Address to check
 *       ps -- The pine_state structure
 *
 * Result: returns 1 if it matches, 0 otherwise.
 *
 * The mailbox must match the user name and the hostname must match.
 * In matching the hostname, matches occur if the hostname in the address
 * is blank, or if it matches the local hostname, or the full hostname
 * with qualifying domain, or the qualifying domain without a specific host.
 * Note, there is a very small chance that we will err on the
 * non-conservative side here.  That is, we may decide two addresses are
 * the same even though they are different (because we do case-insensitive
 * compares on the mailbox).  That might cause a reply not to be sent to
 * somebody because they look like they are us.  This should be very,
 * very rare.
 *
 * It is also considered a match if any of the addresses in alt-addresses
 * matches a.  The check there is simpler.  It parses each address in
 * the list, adding maildomain if there wasn't a domain, and compares
 * mailbox and host in the ADDRESS's for equality.
 */
int
address_is_us(a, ps)
    ADDRESS     *a;
    struct pine *ps;
{
    char **t;
    int ret;

    if(!a || a->mailbox == NULL)
      ret = 0;

    /* at least LHS must match, but case-independent */
    else if(strucmp(a->mailbox, ps->VAR_USER_ID) == 0

                      && /* and hostname matches */

    /* hostname matches if it's not there, */
    (a->host == NULL ||
       /* or if hostname and userdomain (the one user sets) match exactly, */
      ((ps->userdomain && a->host && strucmp(a->host,ps->userdomain) == 0) ||

              /*
               * or if(userdomain is either not set or it is set to be
               * the same as the localdomain or hostname) and (the hostname
               * of the address matches either localdomain or hostname)
               */
             ((ps->userdomain == NULL ||
               strucmp(ps->userdomain, ps->localdomain) == 0 ||
               strucmp(ps->userdomain, ps->hostname) == 0) &&
              (strucmp(a->host, ps->hostname) == 0 ||
               strucmp(a->host, ps->localdomain) == 0)))))
      ret = 1;

    /*
     * If no match yet, check to see if it matches any of the alternate
     * addresses the user has specified.
     */
    else if(!ps_global->VAR_ALT_ADDRS ||
	    !ps_global->VAR_ALT_ADDRS[0] ||
	    !ps_global->VAR_ALT_ADDRS[0][0])
	ret = 0;  /* none defined */

    else{
	ret = 0;
	for(t = ps_global->VAR_ALT_ADDRS; t[0] && t[0][0]; t++){
	    ADDRESS *alt_addr;
	    char    *alt;

	    alt  = cpystr(*t);
	    alt_addr = NULL;
	    rfc822_parse_adrlist(&alt_addr, alt, ps_global->maildomain);
	    if(alt)
	      fs_give((void **)&alt);

	    if(address_is_same(a, alt_addr)){
		if(alt_addr)
		  mail_free_address(&alt_addr);

		ret = 1;
		break;
	    }

	    if(alt_addr)
	      mail_free_address(&alt_addr);
	}
    }

    return(ret);
}


/*
 * Compare the two addresses, and return true if they're the same,
 * false otherwise
 *
 * Args: a -- First address for comparison
 *       b -- Second address for comparison
 *
 * Result: returns 1 if it matches, 0 otherwise.
 */
int
address_is_same(a, b)
    ADDRESS *a, *b;
{
    return(a && b && a->mailbox && b->mailbox && a->host && b->host
	   && strucmp(a->mailbox, b->mailbox) == 0
           && strucmp(a->host, b->host) == 0);
}


/*
 * Compute an upper bound on the size of the array required by
 * rfc822_write_address for this list of addresses.
 *
 * Args: adrlist -- The address list.
 *
 * Returns -- an integer giving the upper bound
 */
int
est_size(a)
    ADDRESS *a;
{
    int cnt = 0;

    for(; a; a = a->next){

	/* two times personal for possible quoting */
	cnt   += 2 * (a->personal  ? (strlen(a->personal)+1)  : 0);
	cnt   += (a->mailbox  ? (strlen(a->mailbox)+1)    : 0);
	cnt   += (a->adl      ? strlen(a->adl)      : 0);
	cnt   += (a->host     ? strlen(a->host)     : 0);

	/*
	 * add room for:
         *   possible single space between fullname and addr
         *   left and right brackets
         *   @ sign
         *   possible : for route addr
         *   , <space>
	 *
	 * So I really think that adding 7 is enough.  Instead, I'll add 10.
	 */
	cnt   += 10;
    }

    return(max(cnt, 50));  /* just making sure */
}


/*
 * Returns the number of addresses in the list.
 */
int
count_addrs(adrlist)
    ADDRESS *adrlist;
{
    int cnt = 0;

    while(adrlist){
	if(adrlist->mailbox && adrlist->mailbox[0])
	  cnt++;

	adrlist = adrlist->next;
    }
    
    return(cnt);
}


/*
 * Returns nonzero if the two address book entries are equal.
 * Returns zero if not equal.
 */
int
abes_are_equal(a, b)
    AdrBk_Entry *a,
	        *b;
{
    int    result = 0;
    char **alist, **blist;
    char  *addra, *addrb;

    if(a && b &&
       a->tag == b->tag &&
       (!a->nickname && !b->nickname ||
	a->nickname && b->nickname && strcmp(a->nickname,b->nickname) == 0) &&
       (!a->fullname && !b->fullname ||
	a->fullname && b->fullname && strcmp(a->fullname,b->fullname) == 0) &&
       (!a->fcc && !b->fcc ||
	a->fcc && b->fcc && strcmp(a->fcc,b->fcc) == 0) &&
       (!a->extra && !b->extra ||
	a->extra && b->extra && strcmp(a->extra,b->extra) == 0)){

	/* If we made it in here, they still might be equal. */
	if(a->tag == Single)
	  result = strcmp(a->addr.addr,b->addr.addr) == 0;
	else{
	    alist = a->addr.list;
	    blist = b->addr.list;
	    if(!alist && !blist)
	      result = 1;
	    else{
		/* compare the whole lists */
		addra = *alist;
		addrb = *blist;
		while(addra && addrb && strcmp(addra,addrb) == 0){
		    alist++;
		    blist++;
		    addra = *alist;
		    addrb = *blist;
		}

		if(!addra && !addrb)
		  result = 1;
	    }
	}
    }

    return(result);
}


/*
 * Parses a string of comma-separated addresses or nicknames into an
 * array.
 *
 * Returns an allocated, null-terminated list, or NULL.
 */
char **
parse_addrlist(addrfield)
    char *addrfield;
{
#define LISTCHUNK  500   /* Alloc this many addresses for list at a time */
    char **al, **ad;
    char *next_addr, *cur_addr, *p, *q;
    int slots = LISTCHUNK;

    if(!addrfield)
      return((char **)NULL);

    /* allocate first chunk */
    slots = LISTCHUNK;
    al    = (char **)fs_get(sizeof(char *) * (slots+1));
    ad    = al;

    p = addrfield;

    /* skip any leading whitespace */
    for(q = p; *q && *q == SPACE; q++)
      ;/* do nothing */

    next_addr = (*q) ? q : NULL;

    /* Loop adding each address in list to array al */
    for(cur_addr = next_addr; cur_addr; cur_addr = next_addr){

	next_addr = skip_to_next_addr(cur_addr);

	q = cur_addr;
	SKIP_SPACE(q);

	/* allocate more space */
	if((ad-al) >= slots){
	    slots += LISTCHUNK;
	    fs_resize((void **)&al, sizeof(char *) * (slots+1));
	    ad = al + slots - LISTCHUNK;
	}

	if(*q)
	  *ad++ = cpystr(q);
    }

    *ad++ = NULL;

    /* free up any excess we've allocated */
    fs_resize((void **)&al, sizeof(char *) * (ad - al));
    return(al);
}


/*
 * Add entries specified by system administrator.  If the nickname already
 * exists, it is not touched.
 */
void
add_forced_entries(abook)
    AdrBk *abook;
{
    AdrBk_Entry *abe;
    char *nickname, *fullname, *address;
    char *end_of_nick, *end_of_full, **t;


    if(!ps_global->VAR_FORCED_ABOOK_ENTRY ||
       !ps_global->VAR_FORCED_ABOOK_ENTRY[0] ||
       !ps_global->VAR_FORCED_ABOOK_ENTRY[0][0])
	return;

    for(t = ps_global->VAR_FORCED_ABOOK_ENTRY; t[0] && t[0][0]; t++){
	nickname = *t;

	/*
	 * syntax for each element is
	 * nick[whitespace]|[whitespace]Fullname[WS]|[WS]Address
	 */

	/* find end of nickname */
	end_of_nick = nickname;
	while(*end_of_nick
	      && !isspace((unsigned char)*end_of_nick)
	      && *end_of_nick != '|')
	  end_of_nick++;

	/* find the pipe character between nickname and fullname */
	fullname = end_of_nick;
	while(*fullname && *fullname != '|')
	  fullname++;
	
	if(*fullname)
	  fullname++;

	*end_of_nick = '\0';
	abe = adrbk_lookup_by_nick(abook, nickname, NULL);

	if(!abe){  /* If it isn't there, add it */

	    /* skip whitespace before fullname */
	    fullname = skip_white_space(fullname);

	    /* find the pipe character between fullname and address */
	    end_of_full = fullname;
	    while(*end_of_full && *end_of_full != '|')
	      end_of_full++;

	    if(!*end_of_full){
		dprint(2, (debugfile,
		    "missing | in forced-abook-entry \"%s\"\n",
		    nickname ? nickname : "?"));
		continue;
	    }

	    address = end_of_full + 1;

	    /* skip whitespace before address */
	    address = skip_white_space(address);

	    if(*address == '('){
		dprint(2, (debugfile,
		   "no lists allowed in forced-abook-entry \"%s\"\n",
		   address ? address : "?"));
		continue;
	    }

	    /* go back and remove trailing white space from fullname */
	    while(*end_of_full == '|' || isspace((unsigned char)*end_of_full)){
		*end_of_full = '\0';
		end_of_full--;
	    }

	    dprint(2, (debugfile,
	       "Adding forced abook entry \"%s\"\n", nickname ? nickname : ""));

	    (void)adrbk_add(abook,
			   NO_NEXT,
			   nickname,
			   fullname,
			   address,
			   NULL,
			   NULL,
			   Single,
			   (adrbk_cntr_t *)NULL,
			   (int *)NULL,
			   1,
			   0,
			   1);
	}
    }
}
