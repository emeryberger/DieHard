#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: adrbklib.c 14092 2005-09-27 21:27:55Z hubert@u.washington.edu $";
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
#include "adrbklib.h"

/*
 * We don't want any end of line fixups to occur, so include "b" in DOS modes.
 */
#if defined(DOS) || defined(OS2)
#define	ADRBK_NAME		"addrbook"
#define	OPEN_WRITE_MODE		(O_TRUNC|O_WRONLY|O_CREAT|O_BINARY)
#define	FOPEN_WRITE_MODE	"wb"
#define	READ_MODE		"rb"
#else
#define	ADRBK_NAME		".addressbook"
#define	OPEN_WRITE_MODE		(O_TRUNC|O_WRONLY|O_CREAT)
#define	FOPEN_WRITE_MODE	"w"
#define	READ_MODE		"r"
#endif


#ifndef MAXPATH
#define MAXPATH 1000    /* Longest file path we can deal with */
#endif

#define MAXLINE 1000    /* Longest line in addrbook */

#define TABWIDTH 8
#define INDENTSTR "   "
#define INDENTXTRA " : "
#define INDENT 3  /* length of INDENTSTR */


/*
 * MSC ver 7.0 and less times are since 1900, everybody else's time so far
 * is since 1970. sheesh.
 */
#if	defined(DOS) && (_MSC_VER == 700)
#define EPOCH_ADJ	((time_t)((time_t)(70*365 + 18) * (time_t)86400))
#endif

#define SLOP 3

static int forced_rebuilds = 0;  /* forced even though mtime looks right  */
static int trouble_rebuilds = 0; /* all rebuilds caused by goto trouble   */
#define MAX_FORCED_REBUILDS 3
#define MAX_TROUBLE_REBUILDS 10
static int writing; /* so we can give understandable error message */

typedef struct ae_list {
    AdrBk_Entry    *ae;
    adrbk_cntr_t    ent;
    struct ae_list *next;
} AE_LIST_S;
/*
 * Deleted_elems are deleted but not yet committed to disk.
 * Deleted_aes are saved ae's to be written to top of addrbook file as comments.
 * Appended_aes are entries which have been appended to the end of the addrbook
 *  but not yet committed to disk.
 * Edited_abook is the address book that has these edits applied to it, others
 *  are unchanged.
 */
static EXPANDED_S   *deleted_elems;
static AE_LIST_S    *deleted_aes;
static AE_LIST_S    *appended_aes;
AdrBk               *edited_abook;

static adrbk_cntr_t insert_before=NO_NEXT; /* pretend inserted_entryref comes */
static EntryRef inserted_entryref;         /* before insert_before            */
static adrbk_cntr_t moved_elem=NO_NEXT;    /* pretend moved_elem is moved to  */
static adrbk_cntr_t move_elem_before;      /* right before move_elem_before   */

static char empty[]       = "";

jmp_buf jump_over_qsort;

/*
 * The addrbook entry cache is stored in a hash table.  Each list in the
 * table has up to CACHE_PER_ER_BUCKET entries in it.  The number of hash
 * buckets is adjusted to make this true.  So, for example, if you set the
 * nominal_max_cache_size to 200 and CACHE_PER_ER_BUCKET to 10, there would
 * be a hash array of 20 lists, each with 10 cache entries stored in it.
 * This gives us a way to have constant lookup time (look through no more
 * than 10 entries) regardless of the size of the cache.
 */
#define CACHE_PER_ER_BUCKET 10


adrbk_cntr_t   ab_hash PROTO((char *, a_c_arg_t));
adrbk_cntr_t   ab_hash_addr PROTO((char *, a_c_arg_t));
adrbk_uid_t    ab_uid PROTO((char *));
adrbk_uid_t    ab_uid_addr PROTO((char *));
void           adrbk_check_local_validity PROTO((AdrBk *, long));
EntryRef      *adrbk_get_entryref PROTO((AdrBk *, a_c_arg_t, Handling));
int            bld_hash_from_ondisk_hash PROTO((AdrBk *));
int            build_ondisk_hash_from_abook PROTO((AdrBk *, char *));
void           clear_entryref_cache PROTO((AdrBk *));
void           clearrefs_in_cached_aes PROTO((AdrBk *));
int            cmp_addr PROTO((const QSType *, const QSType *));
int            cmp_cntr_by_full PROTO((const QSType *, const QSType *));
int            cmp_cntr_by_full_lists_last PROTO((const QSType *,
							    const QSType *));
int            cmp_cntr_by_nick PROTO((const QSType *, const QSType *));
int            cmp_cntr_by_nick_lists_last PROTO((const QSType *,
							    const QSType *));
int            cmp_ae_by_full PROTO((const QSType *, const QSType *));
int            cmp_ae_by_full_lists_last PROTO((const QSType *,const QSType *));
int            cmp_ae_by_nick PROTO((const QSType *, const QSType *));
int            cmp_ae_by_nick_lists_last PROTO((const QSType *,const QSType *));
int            copy_abook_to_tempfile PROTO((AdrBk *, int, int, char *));
char          *dir_containing PROTO((char *));
void           exp_add_nth PROTO((EXPANDED_S *, a_c_arg_t));
void           exp_del_nth PROTO((EXPANDED_S *, a_c_arg_t));
void           fix_sort_rule_in_hash PROTO((AdrBk *));
void           free_ab_adrhash PROTO((AdrHash **));
void           free_ab_entryref PROTO((EntryRef *));
char          *get_entryref_line_from_disk PROTO((FILE *, char *, a_c_arg_t));
int            get_sort_rule_from_disk PROTO((FILE *));
time_t         get_adj_fp_file_mtime PROTO((FILE *));
time_t         get_adj_name_file_mtime PROTO((char *));
time_t         get_timestamp_from_disk PROTO((FILE *));
adrbk_cntr_t   hashtable_size PROTO((a_c_arg_t));
void           init_adrhash_array PROTO((AdrHash *, a_c_arg_t));
AdrBk_Entry   *init_ae_entry PROTO((AdrBk *, EntryRef *));
void           init_entryref_cache PROTO((AdrBk *));
int            length_of_entry PROTO((FILE *, long));
AdrHash       *new_adrhash PROTO((a_c_arg_t));
EntryRef      *new_entryref PROTO((adrbk_uid_t, adrbk_uid_t, long));
int            ok_to_blast_it PROTO((FILE *));
PerAddrBook   *pab_from_ab PROTO((AdrBk *));
int            percent_abook_saved PROTO((void));
adrbk_cntr_t   re_sort_particular_entryref PROTO((AdrBk *, a_c_arg_t));
void           set_inserted_entryref PROTO((AdrBk *, a_c_arg_t, AdrBk_Entry *));
void           set_moved_entryref PROTO((a_c_arg_t, a_c_arg_t));
char          *skip_to_next_nickname PROTO((FILE *, long *, char **, long *,
								int, int *));
void           sort_addr_list PROTO((char **));
void           strip_addr_string PROTO((char *, char **, char **));
int            valid_hfile PROTO((FILE *));
int            write_hash_header PROTO((FILE *, a_c_arg_t));
int            write_hash_table PROTO((AdrHash *, FILE *, a_c_arg_t));
int            write_hash_trailer PROTO((AdrBk *, FILE *, int));
int            write_single_abook_entry PROTO((AdrBk_Entry *, FILE *, int *,
							int *, int *, int *));
int            write_single_entryref PROTO((EntryRef *, FILE *));


/*
 *  Open, read, and parse an address book.
 *
 * Args: pab      -- the PerAddrBook structure
 *       homedir  -- the user's home directory if specified
 *       warning  -- put "why failed" message to user here
 *                   (provide space of at least 201 chars)
 *
 * If filename is NULL, the default will be used in the homedir
 * passed in.  If homedir is NULL, the current dir will be used.
 * If filename is not NULL and is an absolute path, just the filename
 * will be used.  Otherwise, it will be used relative to the homedir, or
 * to the current dir depending on whether or not homedir is NULL.
 *
 * Expected addressbook file format is:
 *  <nickname>\t<fullname>\t<address_field>\t<fcc>\t<comment>
 *
 * The last two fields (\t<fcc>\t<comment>) are optional.
 *
 * Lines that start with SPACE are continuation lines.  Ends of lines are
 * treated as if they were spaces.  The address field is either a single
 * address or a list of comma-separated addresses inside parentheses.
 *
 * Fields missing from the end of an entry are considered blank.
 *
 * Commas in the address field will cause problems, as will tabs in any
 * field.
 *
 * There may be some deleted entries in the addressbook that don't get
 * used.  They look like regular entries except their nicknames start
 * with the string "#DELETED-YY/MM/DD#".
 */
AdrBk *
adrbk_open(pab, homedir, warning, sort_rule, just_create_lu, lu_not_valid)
    PerAddrBook *pab;
    char *homedir,
	 *warning;
    int   sort_rule,
	  just_create_lu,  /* for special use, create .lu file and that's it */
	  lu_not_valid;
{
    char   path[MAXPATH], *filename;
    AdrBk *ab;
    int    abook_validity_was_minusone = 0;


    filename = pab ? pab->filename : NULL;

    dprint(2, (debugfile, "- adrbk_open(%s)%s -\n", filename ? filename : "",
	   lu_not_valid ? " (lu_not_valid set)" : ""));

    ab = (AdrBk *)fs_get(sizeof(AdrBk));
    memset(ab, 0, sizeof(*ab));

    ab->orig_filename = filename ? cpystr(filename) : NULL;

    if(pab->type & REMOTE_VIA_IMAP){
	int try_cache;

	ab->type           = Imap;

	if(!ab->orig_filename || *(ab->orig_filename) != '{'){
	    dprint(1, (debugfile, "adrbk_open: remote: filename=%s\n",
		   ab->orig_filename ? ab->orig_filename : "NULL"));
	    goto bail_out;
	}

	if(!(ab->rd = rd_new_remdata(RemImap, ab->orig_filename,
				     (void *)REMOTE_ABOOK_SUBTYPE))){
	    dprint(1, (debugfile,
		   "adrbk_open: remote: new_remdata failed: %s\n",
		   ab->orig_filename ? ab->orig_filename : "NULL"));
	    goto bail_out;
	}
	
	/* Transfer responsibility for the storage object */
	ab->rd->so = pab->so;
	pab->so = NULL;

	try_cache = rd_read_metadata(ab->rd);

	if(ab->rd->lf)
	  ab->filename = cpystr(ab->rd->lf);

	/* Transfer responsibility for removal of temp file */
	if(ab->rd->flags & DEL_FILE){
	    ab->flags |= (DEL_FILE | DEL_HASHFILE);
	    ab->rd->flags &= ~DEL_FILE;
	}

	if(pab->access == MaybeRorW){
	    if(ab->rd->read_status == 'R')
	      pab->access = ab->rd->access = ReadOnly;
	    else
	      pab->access = ab->rd->access = ReadWrite;
	}
	else if(pab->access == ReadOnly){
	    /*
	     * Pass on readonly-ness from being a global addrbook.
	     * This should cause us to open the remote folder readonly,
	     * avoiding error messages about readonly-ness.
	     */
	    ab->rd->access = ReadOnly;
	}

	/*
	 * The plan is to fetch addrbook data and copy into local file.
	 * Then we open the local copy for reading. We use the IMAP STATUS
	 * command to tell us if we need to update from the remote addrbook.
	 *
	 * If access is NoExists, that probably means we had trouble
	 * opening the remote folder in the adrbk_access routine.
	 * In that case we'll use a cached copy if we have one.
	 */
	if(pab->access != NoExists){

bootstrap_nocheck_policy:
	    if(try_cache && ps_global->remote_abook_validity == -1 &&
	       !abook_validity_was_minusone)
	      abook_validity_was_minusone++;
	    else{
		rd_check_remvalid(ab->rd, 1L);
		abook_validity_was_minusone = 0;
	    }

	    /*
	     * If the cached info on this addrbook says it is readonly but
	     * it looks like it's been fixed now, change it to readwrite.
	     */
	    if(!(pab->type & GLOBAL) && ab->rd->read_status == 'R'){
		/*
		 * We go to this trouble since readonly addrbooks
		 * are likely a mistake. They are usually supposed to be
		 * readwrite. So we open it and check if it's been fixed.
		 */
		rd_check_readonly_access(ab->rd);
		if(ab->rd->read_status == 'W'){
		    pab->access = ab->rd->access = ReadWrite;
		    ab->rd->flags |= REM_OUTOFDATE;
		}
		else
		  pab->access = ab->rd->access = ReadOnly;
	    }
	      
	    if(ab->rd->flags & REM_OUTOFDATE){
		if(rd_update_local(ab->rd) != 0){
		    dprint(1, (debugfile,
			   "adrbk_open: remote: rd_update_local failed\n"));
		    /*
		     * Don't give up altogether. We still may be
		     * able to use a cached copy.
		     */
		}
		else{
		    dprint(7, (debugfile,
			   "%s: copied remote to local (%ld)\n",
			   ab->rd->rn ? ab->rd->rn : "?",
			   (long)ab->rd->last_use));
		}
	    }

	    if(pab->access == ReadWrite)
	      ab->rd->flags |= DO_REMTRIM;
	}

	/* If we couldn't get to remote folder, try using the cached copy */
	if(pab->access == NoExists || ab->rd->flags & REM_OUTOFDATE){
	    if(try_cache){
		pab->access = ab->rd->access = ReadOnly;
		ab->rd->flags |= USE_OLD_CACHE;
		q_status_message(SM_ORDER, 3, 4,
		 "Can't contact remote address book server, using cached copy");
		dprint(2, (debugfile,
	"Can't open remote addrbook %s, using local cached copy %s readonly\n",
		       ab->rd->rn ? ab->rd->rn : "?",
		       ab->rd->lf ? ab->rd->lf : "?"));
	    }
	    else
	      goto bail_out;
	}
    }
    else{
	ab->type = Local;

	/*------------ figure out and save name of file to open ---------*/
	if(filename == NULL){
	    if(homedir != NULL){
		build_path(path, homedir, ADRBK_NAME, sizeof(path));
		ab->filename = cpystr(path);
	    }
	    else
	      ab->filename = cpystr(ADRBK_NAME);
	}
	else{
	    if(is_absolute_path(filename)){
		ab->filename = cpystr(filename);
	    }
	    else{
		if(homedir != NULL){
		    build_path(path, homedir, filename, sizeof(path));
		    ab->filename = cpystr(path);
		}
		else
		  ab->filename = cpystr(filename);
	    }
	}
    }

    if(ab->filename && ab->filename[0]){
	char buf[MAXPATH];

	strncpy(buf, ab->filename, sizeof(buf)-4);
	buf[sizeof(buf)-4] = '\0';
	ab->hashfile = cpystr(strcat(buf, ADRHASH_FILE_SUFFIX));

	/*
	 * Our_filecopy and our_hashcopy are used in _WINDOWS to allow
	 * multiple pines to update the address book. The problem is
	 * that if a file is open it can't be deleted, so we need to keep
	 * the main filename and hashfile closed most of the time.
	 * In Unix, our_filecopy and our_hashcopy just point to filename
	 * and hashfile.
	 */

#ifdef	_WINDOWS
	/*
	 * If we can't write in the same directory as filename is in, put
	 * the copies in /tmp instead.
	 */
	if(!(ab->our_filecopy = tempfile_in_same_dir(ab->filename, "a3", NULL)))
	  ab->our_filecopy = temp_nam(NULL, "a3");

	if(!(ab->our_hashcopy = tempfile_in_same_dir(ab->filename, "a4", NULL))
	   && !just_create_lu)
	  ab->our_hashcopy = temp_nam(NULL, "a4");
#endif	/* _WINDOWS */

	/*
	 * We don't need the copies on Unix because we can rename/delete
	 * open files. Turn the feature off by making the copies point to
	 * the originals.
	 */
	if(!ab->our_filecopy)
	  ab->our_filecopy = ab->filename;
	if(!ab->our_hashcopy)
	  ab->our_hashcopy = ab->hashfile;

	/*
	 * If the hashfile is out-of-date, or if we modify the abook, then
	 * we'll need temp_hashfile in the same dir as our_hashcopy.
	 * If we find we need to update our_hashcopy (in copy_abook_to_tempfile)
	 * but can't, then we will move them to a temp directory.
	 */
	if(just_create_lu){
	    char *temp_hashfile;

	    /*
	     * This call actually creates the temp file, which we don't
	     * want right now. It might be better to change this to a
	     * simpler test.
	     */
	    temp_hashfile = tempfile_in_same_dir(ab->our_hashcopy, "a2", NULL);
	    if(!temp_hashfile){
		if(warning && !*warning)
		  (void)strncpy(warning, "Can't create address book index",200);

		goto bail_out;
	    }
	    else{
		/* that was just a dumb test, get rid of it */
		(void)unlink(temp_hashfile);
		fs_give((void **)&temp_hashfile);
	    }
	}
    }
    else{
	dprint(1, (debugfile, "adrbk_open: ab->filename is NULL???\n"));
	goto bail_out;
    }


    /*
     * We're going to make our own copy of the address book file so that
     * we won't conflict with other instances of pine trying to change it.
     * In particular, on Windows the address book file cannot be deleted
     * or renamed into if it is open in another process.
     * We'll also make sure the hashfile is up-to-date in this function.
     */
    ab->flags |= FILE_OUTOFDATE;
    if(copy_abook_to_tempfile(ab, just_create_lu, lu_not_valid, warning) < 0){
	dprint(1, (debugfile, "adrbk_open: copy_file failed\n"));
	if(abook_validity_was_minusone){
	    /*
	     * The file copy failed when it shouldn't have. If the user has
	     * remote_abook_validity == -1 then we'll go back and try to
	     * do the validity check in case that can get us the file we
	     * need to copy. Without the validity check first time we won't
	     * contact the imap server.
	     */
	    dprint(1, (debugfile, "adrbk_open: trying to bootstrap\n"));

	    if(ab->our_hashcopy){
		if(ab->our_hashcopy != ab->hashfile){
		    (void)unlink(ab->our_hashcopy);
		    fs_give((void **)&ab->our_hashcopy);
		}
		
		ab->our_hashcopy = NULL;
	    }

	    if(ab->hashfile)
	      fs_give((void **)&ab->hashfile);

	    if(ab->our_filecopy){
		if(ab->our_filecopy != ab->filename){
		    (void)unlink(ab->our_filecopy);
		    fs_give((void **)&ab->our_filecopy);
		}
		
		ab->our_filecopy = NULL;
	    }

	    goto bootstrap_nocheck_policy;
	}

	goto bail_out;
    }

    if(!ab->fp || !ab->fp_hash)
      goto bail_out;

    if(ab->hashfile_access == ReadOnly || ab->hashfile_access == NoAccess)
      ab->sort_rule = AB_SORT_RULE_NONE;
    else
      ab->sort_rule = sort_rule;

    if(ab){
	init_entryref_cache(ab);

	/* allocate header for expanded lists list */
	ab->exp      = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
	/* first real element is NULL */
	ab->exp->next = (EXPANDED_S *)NULL;

	/* allocate header for checked entries list */
	ab->checks       = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
	/* first real element is NULL */
	ab->checks->next = (EXPANDED_S *)NULL;

	/* allocate header for selected entries list */
	ab->selects       = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
	/* first real element is NULL */
	ab->selects->next = (EXPANDED_S *)NULL;
	ab->flags |= FIRST_CHANGE;

	return(ab);
    }

bail_out:
    dprint(2, (debugfile, "adrbk_open: bailing: filenames=%s %s %s %s %s fp=%s fp_hash=%s\n",
	   ab->orig_filename ? ab->orig_filename : "NULL",
	   ab->filename ? ab->filename : "NULL",
	   ab->our_filecopy ? ab->our_filecopy : "NULL",
	   ab->hashfile ? ab->hashfile : "NULL",
	   ab->our_hashcopy ? ab->our_hashcopy : "NULL",
	   ab->fp ? "open" : "NULL",
	   ab->fp_hash ? "open" : "NULL"));

    if(ab->rd){
	ab->rd->flags &= ~DO_REMTRIM;
	rd_close_remdata(&ab->rd);
    }

    if(ab->fp)
      (void)fclose(ab->fp);

    if(ab->fp_hash)
      (void)fclose(ab->fp_hash);

    if(ab->our_hashcopy && ab->our_hashcopy != ab->hashfile){
	(void)unlink(ab->our_hashcopy);
	fs_give((void **)&ab->our_hashcopy);
    }

    if(ab->hashfile){
	(void)unlink(ab->hashfile);
	fs_give((void **)&ab->hashfile);
    } 

    if(ab->orig_filename)
      fs_give((void **)&ab->orig_filename);

    if(ab->our_filecopy && ab->our_filecopy != ab->filename){
	(void)unlink(ab->our_filecopy);
	fs_give((void **)&ab->our_filecopy);
    }

    if(ab->filename)
      fs_give((void **)&ab->filename);

    if(pab->so){
	so_give(&(pab->so));
	pab->so = NULL;
    }

    fs_give((void **)&ab);

    return NULL;
}


/*
 * Copy the address book file to the temporary session copy. Also copy
 * the hashfile. Any of these files which don't exist will be created.
 *
 * Returns  0 success
 *         -1 failure
 */
int
copy_abook_to_tempfile(ab, just_create_lu, lu_not_valid, warning)
    AdrBk *ab;
    int    just_create_lu,
	   lu_not_valid;
    char  *warning;
{
    int    got_it, fd, c,
	   ret = -1,
	   tried_shortname = 0,
	   tried_tmpfile = 0,
	   we_cancel = 0;
    FILE  *fp_read = (FILE *)NULL,
          *fp_hash = (FILE *)NULL,
	  *fp_write = (FILE *)NULL;
    char  *lc, *p;
    time_t mtime, timestamp;


    dprint(3, (debugfile, "copy_file(%s, %d) -\n",
	       (ab && ab->filename) ? ab->filename : "", just_create_lu));

    if(!ab || !ab->filename || !ab->filename[0] ||
       !ab->hashfile || !ab->hashfile[0])
      goto get_out;

    if(!(ab->flags & FILE_OUTOFDATE))
      return(0);
    
    /* open filename for reading */
    fp_read = fopen(ab->filename, READ_MODE);
    if(fp_read == NULL){

	/*
	 * filename probably doesn't exist so we try to create it
	 */

	/* don't want to create in these cases, should already be there */
	if(just_create_lu || ab->type == Imap){
	    if(warning){
		if(ab->type == Imap)
		  (void)sprintf(warning,
			        "Temp addrbook file can't be opened: %.150s",
				ab->filename);
		else
		  (void)strncpy(warning, "Address book doesn't exist", 200);
	    }

	    goto get_out;
	}

	q_status_message1(SM_INFO, 0, 3,
	                  "Address book %.200s doesn't exist, creating",
	                  (lc=last_cmpnt(ab->filename)) ? lc : ab->filename);
	dprint(2, (debugfile, "Address book %s doesn't exist, creating\n",
	       ab->filename ? ab->filename : "?"));

	/*
	 * Just use user's umask for permissions. Mode is "w" so the create
	 * will happen. We close it right after creating and open it in
	 * read mode again later.
	 */
        fp_read = fopen(ab->filename, "w");  /* create */
        if(fp_read == NULL        ||
	   fclose(fp_read) == EOF ||
	   (fp_read = fopen(ab->filename, READ_MODE)) == NULL){
            /*--- Create failed, bail out ---*/
	    if(warning)
	      (void)strncpy(warning, error_description(errno), 200);

	    dprint(2, (debugfile, "create failed: %s\n",
		error_description(errno)));

	    goto get_out;
        }
    }

    /* record new change date of addrbook file */
    if(just_create_lu)
      ab->last_change_we_know_about = (time_t)(-1);
    else
      ab->last_change_we_know_about = get_adj_name_file_mtime(ab->filename);

    ab->last_local_valid_chk = get_adj_time();


    /* now there is an ab->filename and we have it open for reading */

try_again:

    got_it = 0;

    if(can_access(ab->hashfile, ACCESS_EXISTS) == 0){
	if(can_access(ab->hashfile, EDIT_ACCESS) == 0)
	  ab->hashfile_access = ReadWrite;
	else if(can_access(ab->hashfile, READ_ACCESS) == 0)
	  ab->hashfile_access = ReadOnly;
	else
	  ab->hashfile_access = NoAccess;
    }
    else
      ab->hashfile_access = NoExists;

    if(ab->hashfile_access == NoExists){    /* no hashfile, create it */
	FILE *fp;
	int fd;

	q_status_message1(SM_INFO, 0, 3,
	    "Index file \"%.200s\" doesn't exist, creating",
	    (lc=last_cmpnt(ab->hashfile)) ? lc : ab->hashfile);
	display_message('x');
	dprint(5, (debugfile, "Index file %s doesn't exist, creating\n",
	    ab->hashfile ? ab->hashfile : "?"));
	errno = 0;

	/*
	 * Create new lookup files with mode 0600, because they might
	 * be sitting in a temp filesystem somewhere. (We retain old
	 * permissions if user changes lookup file permissions.)
	 */
	if((fd = open(ab->hashfile, OPEN_WRITE_MODE, 0600)) < 0)
	  fp = NULL;
	else
	  fp = fdopen(fd, FOPEN_WRITE_MODE);

	if(fp == NULL){
	    if(errno == ENAMETOOLONG && !tried_shortname){

		/*
		 * We know that the addressbook name, say "filename", is
		 * short enough, and that "filename.lu" is too long.
		 * Try a name the same length as "filename".
		 * "filename.lu" -> "filen_.l".
		 * Note that this may not be the best of ideas.  If more
		 * than one address book maps to the same name, we're
		 * probably in for some trouble.
		 */
		 tried_shortname++;
		 p    = &(ab->hashfile[strlen(ab->hashfile) - 6]);
		 *p++ = '_';
		 *p++ = '.';
		 *p++ = 'l';
		 *p   = '\0';
		 q_status_message1(SM_INFO, 0, 3,
		     "filename too long, using \"%.200s\"",
		     (lc=last_cmpnt(ab->hashfile)) ? lc : ab->hashfile);
		 dprint(2, (debugfile, "name too long, trying %s\n",
		     ab->hashfile ? ab->hashfile : "?"));
		 goto try_again;
	    }
	    else{
		/* have to try /tmp file below */
		if(tried_tmpfile || just_create_lu){
		    q_status_message(SM_ORDER, 0, 3,
			"problems accessing addressbook");
		    display_message('x');
		    goto get_out;
		}
	    }
	}
	else{
	    (void)fclose(fp);
	    ab->hashfile_access = ReadWrite;
	}
    }

    /*
     * If we can't create the hashfile in the right place, put it in
     * a temporary file for the duration of this session.
     */
    if((ab->hashfile_access == NoExists ||
        ab->hashfile_access == NoAccess) && !tried_tmpfile){
	char savename[MAXPATH];

try_tempfile:
	if(just_create_lu){
	    if(warning && !*warning)
	      (void)strncpy(warning, "Can't create address book index file",
			    200);

	    goto get_out;
	}

	if(tried_tmpfile++){
	    q_status_message(SM_ORDER, 0, 3, "problems accessing addressbook");
	    display_message('x');
	    goto get_out;
	}

	(void)strncpy(savename, ab->hashfile ? ab->hashfile : "?",
		      sizeof(savename)-1);
	savename[sizeof(savename)-1] = '\0';
	if(ab->our_hashcopy && ab->our_hashcopy != ab->hashfile){
	    (void)unlink(ab->our_hashcopy);
	    fs_give((void **)&ab->our_hashcopy);
	}

	if(ab->hashfile)
	  fs_give((void **)&ab->hashfile);

	if(!(ab->hashfile = temp_nam(NULL, "a5")))
	  goto get_out;

	dprint(2, (debugfile, "trying tmpfile %s\n",
	       ab->hashfile ? ab->hashfile : "?"));

	/*
	 * We don't need a separate copy in this case since hashfile is
	 * now already a separate temporary file used only by us.
	 */
	ab->our_hashcopy = ab->hashfile;

	ab->flags |= DEL_HASHFILE;
	q_status_message1(SM_ORDER, 3, 3,
		"Can't create \"%.200s\", using temp file",
		(lc=last_cmpnt(savename)) ? lc : savename);

	goto try_again;
    }


    /*
     * Now there is an ab->hashfile and an ab->filename. We have
     * ab->filename open for reading, and ab->hashfile is not open.
     */

    /* check to see if the hashfile is valid and up to date */
    if(ab->hashfile_access == ReadWrite || ab->hashfile_access == ReadOnly){
	/* check to see if hashfile is up-to-date */
	mtime = get_adj_name_file_mtime(ab->filename);
	if(mtime != (time_t)(-1)){
	    fp_hash = fopen(ab->hashfile, READ_MODE);

	    if(!lu_not_valid && valid_hfile(fp_hash)){
		/*
		 * Prior to pine4 the timestamp that went into the lu
		 * file (the one we're getting here) was the current time
		 * (plus three seconds, actually) instead of the mtime of
		 * the actual addrbook file. That was dumb. We'd like to
		 * just check timestamp == mtime now, but we don't want to
		 * break old scripts that use pine3.9 to build the index
		 * file. Therefore, we keep the >=.
		 *
		 * We've seen cases on Windows where a script copies
		 * an addrbook file into place, then runs -create_lu
		 * on that, then afterwards the OS flushes some state
		 * for the addrbook file and increases its mtime by a
		 * second. The result is that the mtime of the addrbook
		 * file ended up being one second newer than the mtime
		 * stored in the lu file, due to OS error. So we put some
		 * slop into the comparison to hopefully avoid that problem.
		 */
	        if((timestamp=get_timestamp_from_disk(fp_hash)) >= mtime-SLOP){
		    /* Ok, hashfile is up-to-date, use it */
		    got_it++;
	        }
	    }
	    else{
		if(!just_create_lu && !ok_to_blast_it(fp_hash)){
		    int ans;
		    char prompt[200];

		    if(ab->hashfile_access == ReadWrite){
		        dprint(2, (debugfile, "ask if ok to blast %s\n",
			    ab->hashfile ? ab->hashfile : "?"));
			sprintf(prompt,
			    "Pine needs to update index file %.100s, ok",
			    ab->hashfile);
			ans = want_to(prompt, 'y', 'n', NO_HELP, WT_NORM);
			if(ans != 'y'){
			    dprint(2, (debugfile,
				"user says not ok to blast, use temp file\n"));

			    if(fp_hash){
				(void)fclose(fp_hash);
				fp_hash = (FILE *)NULL;
			    }

			    goto try_tempfile;
			}
			else{
			    dprint(2, (debugfile, "user says ok to blast\n"));
			}
		    }
		}
	    }

	    if(!got_it && fp_hash){
		(void)fclose(fp_hash);
		fp_hash = (FILE *)NULL;
	    }
	}
	
	if(!got_it){
	    char  *dir1, *dir2 = NULL;
	    int    do_temp = 0;
	    char  *temp_hashfile;

	    /*
	     * The lu file is out of date. Check the filenames to
	     * see if it is possible that we're going to be able
	     * to create and rename. If not, switch to using a
	     * temporary hash file instead.
	     */
	    dir1 = dir_containing(ab->our_hashcopy);
	    if(dir1){
		/*
		 * This call actually creates the temp file, which we don't
		 * want right now. It might be better to change this to a
		 * simpler test.
		 */
		temp_hashfile = tempfile_in_same_dir(ab->our_hashcopy, "a2",
						     NULL);
		if(temp_hashfile){
		    dir2 = dir_containing(temp_hashfile);
		    (void)unlink(temp_hashfile);
		    fs_give((void **)&temp_hashfile);
		}
	    }

	    if(!dir1 || !dir2 || strcmp(dir1, dir2))
	      do_temp++;

	    if(dir1)
	      fs_give((void **)&dir1);
	    if(dir2)
	      fs_give((void **)&dir2);

	    if(do_temp)
	      goto try_tempfile;

	    dprint(5, (debugfile,
		   "lu is out of date: timestamp=%lu file_mtime=%lu\n",
		   (unsigned long)timestamp, (unsigned long)mtime));
	}
    }

    /* copy ab->filename to ab->our_filecopy, preserving mtime */
    if(ab->filename != ab->our_filecopy){
	struct stat sbuf;
	time_t tp[2];
	int valid_stat = 0;

	dprint(7, (debugfile, "Before abook copies\n"));
	if((fd = open(ab->our_filecopy, OPEN_WRITE_MODE, 0600)) < 0)
	  goto get_out;
	
	fp_write = fdopen(fd, FOPEN_WRITE_MODE);
	rewind(fp_read);
	if(fstat(fileno(fp_read), &sbuf)){
	    q_status_message1(SM_INFO, 0, 3,
		 "Error: can't stat addrbook \"%.200s\"",
		 (lc=last_cmpnt(ab->filename)) ? lc : ab->filename);
	    dprint(2, (debugfile, "Error: can't stat addrbook \"%s\"\n",
		   ab->filename ? ab->filename : "?"));
	}
	else{
	    valid_stat++;
	    tp[0] = sbuf.st_atime;
	    tp[1] = sbuf.st_mtime;
	}

	while((c = getc(fp_read)) != EOF)
	  if(putc(c, fp_write) == EOF)
	    goto get_out;
	
	(void)fclose(fp_write);
	fp_write = (FILE *)NULL;
	if(valid_stat && utime(ab->our_filecopy, tp)){
	    q_status_message1(SM_INFO, 0, 3,
		 "Error: can't set mtime for \"%.200s\"",
		 (lc=last_cmpnt(ab->filename)) ? lc : ab->our_filecopy);
	    dprint(2, (debugfile, "Error: can't set mtime for \"%s\"\n",
		   ab->our_filecopy ? ab->our_filecopy : "?"));
	}

	(void)fclose(fp_read);
	fp_read = (FILE *)NULL;
	if(!(ab->fp = fopen(ab->our_filecopy, READ_MODE)))
	  goto get_out;

	dprint(7, (debugfile, "After abook file copy\n"));
    }
    else{  /* already open to the right file */
	ab->fp = fp_read;
	fp_read = (FILE *)NULL;
    }

    /* if valid, copy ab->hashfile to ab->our_hashcopy */
    if(ab->hashfile != ab->our_hashcopy){
	if(got_it){	/* valid, copy it */
	    if((fd = open(ab->our_hashcopy, OPEN_WRITE_MODE, 0600)) < 0)
	      goto get_out;
	    
	    fp_write = fdopen(fd, FOPEN_WRITE_MODE);
	    rewind(fp_hash);
	    while((c = getc(fp_hash)) != EOF)
	      if(putc(c, fp_write) == EOF)
		goto get_out;
	    
	    (void)fclose(fp_write);
	    fp_write = (FILE *)NULL;
	    (void)fclose(fp_hash);
	    fp_hash = (FILE *)NULL;
	    if(!(ab->fp_hash = fopen(ab->our_hashcopy, READ_MODE)))
	      goto get_out;
	}
	else{		/* not valid, create zero-length our_hashcopy */
	    if((fd = open(ab->our_hashcopy, OPEN_WRITE_MODE, 0600)) < 0)
	      goto get_out;
	    
	    (void)close(fd);
	}

	dprint(7, (debugfile, "After index file copy\n"));
    }
    else{
	ab->fp_hash = fp_hash;
	fp_hash = (FILE *)NULL;
    }

    /*
     * Now we've copied filename to our_filecopy and either copied
     * hashfile to our_hashcopy or created an empty our_hashcopy.
     * Operate on the copies now. Ab->fp is open readonly on
     * our_filecopy. Ab->fp_hash is open readonly on our_hashcopy if
     * got_it was true.
     */

    got_it = 0;
    mtime = get_adj_name_file_mtime(ab->our_filecopy);
    if(!ab->fp_hash)
      ab->fp_hash = fopen(ab->our_hashcopy, READ_MODE);

    if(lu_not_valid)
      dprint(5, (debugfile, "lu forced not valid\n"));

    if(!lu_not_valid && valid_hfile(ab->fp_hash)){
	if(mtime != (time_t)(-1) &&
	   (timestamp=get_timestamp_from_disk(ab->fp_hash)) >= mtime-SLOP){
	    /* Ok, our_hashcopy is up-to-date, use it */
	    we_cancel = busy_alarm(1, NULL, NULL, 0);
	    if(bld_hash_from_ondisk_hash(ab) == 0)
	      got_it++;

	    if(we_cancel)
	      cancel_busy_alarm(-1);
	}
	else
	  dprint(5, (debugfile,
	      "-lu is out of date: timestamp=%lu file_mtime=%lu\n",
	      (unsigned long)timestamp, (unsigned long)mtime));
    }

    if(!got_it && ab->fp_hash){
	(void)fclose(ab->fp_hash);
	ab->fp_hash = (FILE *)NULL;
    }


    /* our_hashcopy needs to be (re)built from the abook file */
    if(!got_it){
	char b[400];

	dprint(5, (debugfile, "%s is not valid, rebuilding\n",
	     ab->our_hashcopy ? ab->our_hashcopy : "?"));
	sprintf(b, "Updating addrbook index file (%.200s)",
		(lc=last_cmpnt(ab->hashfile)) ? lc : ab->hashfile);

	if(!just_create_lu){
	    display_message('x');
	    we_cancel = busy_alarm(1, b, NULL, 0);
	}

	if(build_ondisk_hash_from_abook(ab,
				(warning && !*warning) ? warning : NULL)){
	    if(we_cancel)
	      cancel_busy_alarm(-1);

	    dprint(2,
	     (debugfile, "failed in build_ondisk_hash_from_abook\n"));
	    goto get_out;
	}

	if(we_cancel)
	  cancel_busy_alarm(-1);
    }

    ab->flags &= ~FILE_OUTOFDATE;  /* turn off out of date flag */
    ret = 0;

get_out:
    if(fp_read)
      (void)fclose(fp_read);
    if(fp_hash)
      (void)fclose(fp_hash);
    if(fp_write)
      (void)fclose(fp_write);

    if(ret < 0 && ab){
	if(ab->our_filecopy && ab->our_filecopy != ab->filename)
	  (void)unlink(ab->our_filecopy);
	if(ab->our_hashcopy && ab->our_hashcopy != ab->hashfile)
	  (void)unlink(ab->our_hashcopy);
    }

    return(ret);
}


/*
 * Returns an allocated copy of the directory which contains filename.
 */
char *
dir_containing(filename)
    char *filename;
{
    char  dir[MAXPATH+1];
    char *dirp = NULL;

    if(filename){
	char *lc;

	if((lc = last_cmpnt(filename)) != NULL){
	    int to_copy;

	    to_copy = (lc - filename > 1) ? (lc - filename - 1) : 1;
	    strncpy(dir, filename, min(to_copy, sizeof(dir)-1));
	    dir[min(to_copy, sizeof(dir)-1)] = '\0';
	}
	else{
	    dir[0] = '.';
	    dir[1] = '\0';
	}

	dirp = cpystr(dir);
    }

    return(dirp);
}


/*
 * Return the name of a file in the same directory as filename.
 * Same as temp_nam except it figures out a name in the same directory.
 * It also returns the name of the directory in ret_dir if ret_dir is
 * not NULL. That has to be freed by caller. If return is not NULL the
 * empty file has been created.
 */
char *
tempfile_in_same_dir(filename, prefix, ret_dir)
    char  *filename;
    char  *prefix;
    char **ret_dir;
{
    char  dir[MAXPATH+1];
    char *dirp = NULL;
    char *ret_file = NULL;

    if(filename){
	char *lc;

	if((lc = last_cmpnt(filename)) != NULL){
	    int to_copy;

	    to_copy = (lc - filename > 1) ? (lc - filename - 1) : 1;
	    strncpy(dir, filename, min(to_copy, sizeof(dir)-1));
	    dir[min(to_copy, sizeof(dir)-1)] = '\0';
	}
	else{
	    dir[0] = '.';
	    dir[1] = '\0';
	}

	dirp = dir;
    }


    /* temp_nam creates ret_file */
    ret_file = temp_nam(dirp, prefix);

    /*
     * If temp_nam can't write in dirp it puts the file in a temp directory
     * anyway. We don't want that to happen to us.
     */
    if(dirp && ret_file && !in_dir(dirp, ret_file)){
	(void)unlink(ret_file);
	fs_give((void **)&ret_file);  /* sets it to NULL */
    }

    if(ret_file && ret_dir && dirp)
      *ret_dir = cpystr(dirp);
      

    return(ret_file);
}


/*
 * Checks whether or not the addrbook is sorted correctly according to
 * the SortType.  Returns 1 if is sorted correctly, 0 otherwise.
 */
int
adrbk_is_in_sort_order(ab, force_check, be_quiet)
    AdrBk *ab;
    int    force_check;
    int    be_quiet;
{
    adrbk_cntr_t entry;
    AdrBk_Entry *ae, *ae_prev;
    int (*cmp_func)();
    int last_time_sorted_rule;
    int we_cancel = 0;

    dprint(9, (debugfile, "- adrbk_is_in_sort_order -\n"));

    if(!ab)
      return 0;

    if(ab->sort_rule == AB_SORT_RULE_NONE)
      return 1;
    
    if(ab->count < 2){
	if(ab->sort_rule != get_sort_rule_from_disk(ab->fp_hash))
	  fix_sort_rule_in_hash(ab);

	return 1;
    }

    /*
     * If it's the same, we can assume it is in sort order.
     * This is only actually true if the client is playing by the
     * rules.  The rule that matters here is that you have to sort
     * the addrbook before you can do stuff like add to it or delete
     * from it.  Those operations assume it is already sorted and
     * will record that fact in the variable we're checking below.
     * We're ok because addrbook.c always sorts the addrbook when it
     * finds it is out of order.
     *
     * When we can go wrong is if something changes externally. If the
     * user changes the collation rule by changing their locale we can
     * lose.
     */
    last_time_sorted_rule = get_sort_rule_from_disk(ab->fp_hash);
    if(!force_check &&
       last_time_sorted_rule != -1 &&
       ab->sort_rule == last_time_sorted_rule)
      return 1;

    cmp_func = (ab->sort_rule == AB_SORT_RULE_FULL_LISTS) ?
					    cmp_ae_by_full_lists_last :
               (ab->sort_rule == AB_SORT_RULE_FULL) ?
					    cmp_ae_by_full :
               (ab->sort_rule == AB_SORT_RULE_NICK_LISTS) ?
					    cmp_ae_by_nick_lists_last :
            /* (ab->sort_rule == AB_SORT_RULE_NICK) */
					    cmp_ae_by_nick;

    ae_prev = adrbk_get_ae(ab, (a_c_arg_t)0, Normal);

    if(!be_quiet)
      we_cancel = busy_alarm(1, NULL, NULL, 0);

    for(entry = 1, ae = adrbk_get_ae(ab, (a_c_arg_t)entry, Normal);
	ae != (AdrBk_Entry *)NULL;
	ae = adrbk_get_ae(ab, (a_c_arg_t)(++entry), Normal)){

	    if((*cmp_func)((QSType *)&ae_prev, (QSType *)&ae) > 0){
		if(we_cancel)
		  cancel_busy_alarm(-1);

		return 0;
	    }

	    ae_prev = ae;
    }

    /*
     * Do this so that we won't have to go through the whole addrbook
     * to check next time we open it.
     */
    if(!force_check || ab->sort_rule != last_time_sorted_rule)
      fix_sort_rule_in_hash(ab);

    if(we_cancel)
      cancel_busy_alarm(-1);

    return 1;
}


/*
 * Fix the ondisk sort rule so we won't have to sort next time. Fix it
 * in both the real hashfile and the temporary copy.
 */
void
fix_sort_rule_in_hash(ab)
    AdrBk *ab;
{
    FILE *fp_for_old_hash,
	 *fp_for_new_hash;
    int   c, fd, files_to_fix, which;
    long  filesize, all_but_sort_rule;
    char  format[50];

    if(!ab || ab->fp_hash == (FILE *)NULL)
      return;

    if(ab->our_hashcopy && ab->our_hashcopy != ab->hashfile)
      files_to_fix = 2;
    else
      files_to_fix = 1;

    filesize = (SIZEOF_HDR + ab->count * SIZEOF_ENTRYREF_ENTRY +
	2 * ab->htable_size * SIZEOF_HTABLE_ENTRY + SIZEOF_TRLR);
    
    for(which = 0; which < files_to_fix; which++){
	char *temp_hashfile;

	temp_hashfile = NULL;
	if(!(ab->our_hashcopy && ab->our_hashcopy[0])
	   || !(temp_hashfile=tempfile_in_same_dir(ab->our_hashcopy,"a2",NULL))
	   || (fd = open(temp_hashfile, OPEN_WRITE_MODE, 0600)) < 0){
	    if(temp_hashfile){
		(void)unlink(temp_hashfile);
		fs_give((void **)&temp_hashfile);
	    }

	    return;
	}

	fp_for_new_hash = fdopen(fd, FOPEN_WRITE_MODE);
	
	fp_for_old_hash = ab->fp_hash;
	rewind(fp_for_old_hash);

	all_but_sort_rule = filesize - SIZEOF_SORT_RULE - SIZEOF_NEWLINE;

	/*
	 * Straight copy of all but the sort rule at the end.
	 * Everything else is ok.  The in core stuff is ok, too.  This is
	 * because the sort rule in the file is used only for the purpose
	 * of checking to see if it's already sorted.
	 */
	while(all_but_sort_rule-- > 0L){
	    if((c = getc(fp_for_old_hash)) == EOF ||
	       putc(c, fp_for_new_hash) == EOF){

		/* shouldn't happen */
		(void)fclose(fp_for_new_hash);
		(void)unlink(temp_hashfile);
		fs_give((void **)&temp_hashfile);
		return;
	    }
	}

	sprintf(format, "%%%dd\n", SIZEOF_SORT_RULE);
	if(fprintf(fp_for_new_hash, format, ab->sort_rule) == EOF){
	    (void)fclose(fp_for_new_hash);
	    (void)unlink(temp_hashfile);
	    fs_give((void **)&temp_hashfile);
	    return;
	}

	if(fclose(fp_for_new_hash) == EOF){
	    (void)unlink(temp_hashfile);
	    fs_give((void **)&temp_hashfile);
	    return;
	}

	if(which == 0){
	    (void)fclose(ab->fp_hash);
	    if(rename_file(temp_hashfile, ab->our_hashcopy) < 0)
	      (void)unlink(temp_hashfile);

	    fs_give((void **)&temp_hashfile);
	    ab->fp_hash = fopen(ab->our_hashcopy, READ_MODE);
	}
	else{
	    file_attrib_copy(temp_hashfile, ab->hashfile);
	    if(rename_file(temp_hashfile, ab->hashfile) < 0)
	      (void)unlink(temp_hashfile);

	    fs_give((void **)&temp_hashfile);
	}
    }
}


/*
 * Returns 1 if it is ok to overwrite the file.
 */
int
ok_to_blast_it(fp)
    FILE *fp;
{
    char buf[SIZEOF_PMAGIC + 1];
    long filesize;

    if(fp == (FILE *)NULL)
      return 0;

    /* check if file is empty */
    if((filesize = fp_file_size(fp)) == -1L)
      return 0;

    if(filesize == 0L)
      return 1;

    /* check for header PMAGIC (or LEGACY_PMAGIC) */
    if(fseek(fp, (long)TO_FIND_HDR_PMAGIC, 0))
      return 0;

    if(fread(buf, sizeof(char), (unsigned)SIZEOF_PMAGIC, fp) != SIZEOF_PMAGIC)
      return 0;

    buf[SIZEOF_PMAGIC] = '\0';
    if(strcmp(buf, PMAGIC) == 0 || strcmp(buf, LEGACY_PMAGIC) == 0)
      return 1;

    return 0;
}


/*
 * Sanity checks on hashfile.
 * Returns 1 if ok.
 */
int
valid_hfile(fp)
    FILE *fp;
{
    char buf[SIZEOF_ASCII_LONG + 1];	/* this is the max of the sizes */
    long hashsize, num_elements, filesize;

    dprint(9, (debugfile, "- valid_hfile -\n"));

    if(fp == (FILE *)NULL)
      return 0;

    /* check for header PMAGIC */
    if(fseek(fp, (long)TO_FIND_HDR_PMAGIC, 0)){
	dprint(2, (debugfile, "lu not valid - can't seek to PMAGIC\n"));
	return 0;
    }

    if(fread(buf, sizeof(char), (unsigned)SIZEOF_PMAGIC, fp) != SIZEOF_PMAGIC){
	dprint(2, (debugfile, "lu not valid - can't read PMAGIC\n"));
	return 0;
    }

    buf[SIZEOF_PMAGIC] = '\0';
    if(strcmp(buf, PMAGIC) != 0){
	dprint(2, (debugfile, "lu not valid - PMAGIC is %s\n", buf ? buf : "?"));
	return 0;
    }

    /* check for matching version number */
    if(fseek(fp, (long)TO_FIND_VERSION_NUM, 0)){
	dprint(2, (debugfile, "lu not valid - can't seek to VERS_NUM\n"));
	return 0;
    }

    if(fread(buf, sizeof(char), (unsigned)SIZEOF_VERSION_NUM, fp) !=
	SIZEOF_VERSION_NUM){
	dprint(2, (debugfile, "lu not valid - can't read VERS_NUM\n"));
	return 0;
    }

    buf[SIZEOF_VERSION_NUM] = '\0';
    if(strcmp(buf, ADRHASH_FILE_VERSION_NUM) != 0){
	dprint(2, (debugfile, "lu not valid - VERS_NUM is %s not %s\n",
	    buf ? buf : "?",
	    ADRHASH_FILE_VERSION_NUM ? ADRHASH_FILE_VERSION_NUM : "?"));
	return 0;
    }

    /* check for reasonable hashtable size */
    if(fseek(fp, (long)TO_FIND_HTABLE_SIZE, 0)){
	dprint(2, (debugfile, "lu not valid - can't seek to HTABLE_SIZE\n"));
	return 0;
    }

    if(fread(buf, sizeof(char), (unsigned)SIZEOF_HASH_SIZE, fp) !=
							SIZEOF_HASH_SIZE){
	dprint(2, (debugfile, "lu not valid - can't read HTABLE_SIZE\n"));
	return 0;
    }

    buf[SIZEOF_HASH_SIZE] = '\0';
    hashsize = atol(buf);
    if(hashsize <= 10L || hashsize > MAX_HASHTABLE_SIZE){
	dprint(2, (debugfile, "lu not valid - hashsize is %s\n",
	       buf ? buf : "?"));
	return 0;
    }

    /* check for trailer PMAGIC */
    if(fseek(fp, (long)TO_FIND_TRLR_PMAGIC, 2)){
	dprint(2, (debugfile, "lu not valid - can't seek to TRL_PMAGIC\n"));
	return 0;
    }

    if(fread(buf, sizeof(char), (unsigned)SIZEOF_PMAGIC, fp) != SIZEOF_PMAGIC){
	dprint(2, (debugfile, "lu not valid - can't read TRL_PMAGIC\n"));
	return 0;
    }

    buf[SIZEOF_PMAGIC] = '\0';
    if(strcmp(buf, PMAGIC) != 0){
	dprint(2, (debugfile, "lu not valid - TRL_PMAGIC is %s\n",
	       buf ? buf : "?"));
	return 0;
    }

    /* check for reasonable number of entries */
    if(fseek(fp, (long)TO_FIND_COUNT, 2)){
	dprint(2, (debugfile, "lu not valid - can't seek to COUNT\n"));
	return 0;
    }

    if(fread(buf, sizeof(char), (unsigned)SIZEOF_COUNT, fp) != SIZEOF_COUNT){
	dprint(2, (debugfile, "lu not valid - can't read COUNT\n"));
	return 0;
    }

    buf[SIZEOF_COUNT] = '\0';
    num_elements = atol(buf);
    if(num_elements < 0L || num_elements > MAX_ADRBK_SIZE){
	dprint(2, (debugfile, "lu not valid - COUNT is %s\n", buf ? buf : "?"));
	return 0;
    }

    /* check size of file */
    if((filesize = fp_file_size(fp)) == -1L){
	dprint(2, (debugfile, "lu not valid - fp_file_size failed\n"));
	return 0;
    }

    if(filesize != (SIZEOF_HDR + num_elements * SIZEOF_ENTRYREF_ENTRY +
	2 * hashsize * SIZEOF_HTABLE_ENTRY + SIZEOF_TRLR)){
	dprint(2, (debugfile, "lu not valid - filesize is %ld\n", filesize));
	return 0;
    }

    return 1;
}


int
bld_hash_from_ondisk_hash(ab)
    AdrBk *ab;
{
    long               cnt;
    char               buf[SIZEOF_HASH_SIZE + 1];
    char              *p;
    register char     *q;
    long               nick_hash_offset;
    size_t             adrhashtable_size;
    adrbk_cntr_t       i;
    size_t             psize;
    adrbk_cntr_t      *array;
    WIDTH_INFO_S      *widths;

    dprint(9, (debugfile, "- bld_hash_from_ondisk_hash -\n"));

    if(!ab || !ab->fp_hash)
      return -1;

    /* get htable size */
    if(fseek(ab->fp_hash, (long)TO_FIND_HTABLE_SIZE, 0) == 0 &&
       fread(buf, sizeof(char), (unsigned)SIZEOF_HASH_SIZE, ab->fp_hash) ==
							SIZEOF_HASH_SIZE){
	buf[SIZEOF_HASH_SIZE] = '\0';
	ab->htable_size = atoi(buf);
    }
    else
      return -1;

    adrhashtable_size = ab->htable_size * SIZEOF_HTABLE_ENTRY;

    psize = max(2 * adrhashtable_size, SIZEOF_TRLR);
    p     = (char *)fs_get(psize);

    if(fseek(ab->fp_hash, (long)TO_FIND_TRLR_PMAGIC, 2))
      return -1;

    if(fread(p, sizeof(char), (unsigned)SIZEOF_TRLR, ab->fp_hash) !=
								SIZEOF_TRLR)
      return -1;

    q = p + SIZEOF_PMAGIC + SIZEOF_SPACE;
    cnt = atol(q);
    if(cnt < 0L)
      return -1;
    else
      ab->count = (adrbk_cntr_t)cnt;
    
    ab->phantom_count = ab->count;
    ab->orig_count    = ab->count;

    q += (SIZEOF_COUNT + SIZEOF_NEWLINE);
    ab->deleted_cnt = atol(q);

    q += (SIZEOF_DELETED_CNT + SIZEOF_NEWLINE + SIZEOF_SPACE);
    widths = &ab->widths;
    widths->max_nickname_width = min(atoi(q), 99);
    q += (SIZEOF_WIDTH + SIZEOF_SPACE);
    widths->max_fullname_width = min(atoi(q), 99);
    q += (SIZEOF_WIDTH + SIZEOF_SPACE);
    widths->max_addrfield_width = min(atoi(q), 99);
    q += (SIZEOF_WIDTH + SIZEOF_SPACE);
    widths->max_fccfield_width = min(atoi(q), 99);
    q += (SIZEOF_WIDTH + SIZEOF_SPACE);
    widths->third_biggest_fullname_width = min(atoi(q), 99);
    q += (SIZEOF_WIDTH + SIZEOF_SPACE);
    widths->third_biggest_addrfield_width = min(atoi(q), 99);
    q += (SIZEOF_WIDTH + SIZEOF_SPACE);
    widths->third_biggest_fccfield_width = min(atoi(q), 99);

    ab->hash_by_nick = new_adrhash((a_c_arg_t)ab->htable_size);
    ab->hash_by_addr = new_adrhash((a_c_arg_t)ab->htable_size);

    nick_hash_offset = SIZEOF_HDR + ab->count * SIZEOF_ENTRYREF_ENTRY;

    if(fseek(ab->fp_hash, nick_hash_offset, 0) == 0 &&
       fread(p, sizeof(char), (2 * adrhashtable_size), ab->fp_hash) ==
						   2 * adrhashtable_size){

	dprint(9, (debugfile, "initializing hash_by_nick\n"));
	/* initialize hash_by_nick array */
	array = ab->hash_by_nick->harray;
	q = p;
	for(i = 0; i < ab->htable_size; i++){
	    array[i] = (adrbk_cntr_t)strtoul(q, (char **)NULL, 10);
	    q += SIZEOF_HTABLE_ENTRY;
	}

	dprint(9, (debugfile, "initializing hash_by_addr\n"));
	/* initialize hash_by_addr array */
	array = ab->hash_by_addr->harray;
	for(i = 0; i < ab->htable_size; i++){
	    array[i] = (adrbk_cntr_t)strtoul(q, (char **)NULL, 10);
	    q += SIZEOF_HTABLE_ENTRY;
	}
    }
    else{
	free_ab_adrhash(&ab->hash_by_nick);
	free_ab_adrhash(&ab->hash_by_addr);
	fs_give((void **)&p);
	return -1;
    }

    fs_give((void **)&p);

    return 0;
}


char *
get_entryref_line_from_disk(fp, buf, entry_num)
    FILE     *fp;
    char      buf[];
    a_c_arg_t entry_num;
{
    long seek_position;
    size_t rv;

    if(!fp){
	dprint(2, (debugfile, "get_entryref_line_from_disk returning NULL!\n"));
	dprint(2, (debugfile, "    fp was NULL\n"));
	return NULL;
    }

    seek_position = SIZEOF_HDR + (long)entry_num * SIZEOF_ENTRYREF_ENTRY;

    if(fseek(fp, seek_position, 0)){
	dprint(2, (debugfile, "get_entryref_line_from_disk returning NULL!\n"));
	dprint(2, (debugfile,
		    "    fseek failed, seek_position=%ld, entry_num=%lu, %s\n",
		    seek_position, (unsigned long)entry_num,
		    error_description(errno)));
	return NULL;
    }

    errno = 0;
    clearerr(fp);
    if((rv=fread(buf, sizeof(char), (unsigned)SIZEOF_ENTRYREF_ENTRY, fp)) !=
	SIZEOF_ENTRYREF_ENTRY){
	int saverrno = errno;

	dprint(2, (debugfile,
	    "get_entryref_line_from_disk returning NULL!\n"));
	dprint(2, (debugfile,
	    "    fread returned %ld instead of %d, %s (errno=%d)\n",
	    (long)rv, SIZEOF_ENTRYREF_ENTRY, error_description(saverrno),
	    saverrno));
	dprint(2, (debugfile, "    seek_position=%ld, entry_num=%lu\n",
	    seek_position, (unsigned long)entry_num));
	if(rv == 0)
	  dprint(2, (debugfile, "    ferror(fp)=%d, feof(fp)=%d\n",
	    ferror(fp), feof(fp)));

	return NULL;
    }

    buf[SIZEOF_ENTRYREF_ENTRY] = '\0';

    return(buf);
}


time_t
get_timestamp_from_disk(fp)
    FILE *fp;
{
    char buf[SIZEOF_TIMESTAMP + 1];

    dprint(9, (debugfile, "- get_timestamp_from_disk -\n"));

    if(fp == (FILE *)NULL)
      return (time_t)0;

    if(fseek(fp, (long)TO_FIND_TIMESTAMP, 2))
      return (time_t)0;

    if(fread(buf, sizeof(char), (unsigned)SIZEOF_TIMESTAMP, fp) !=
							    SIZEOF_TIMESTAMP)
      return (time_t)0;

    buf[SIZEOF_TIMESTAMP] = '\0';
    return((time_t)strtoul(buf, (char **)NULL, 10));
}


/*
 * Adjust the mtime to return time since Unix epoch.  DOS is off by 70 years.
 */
time_t
get_adj_fp_file_mtime(fp)
    FILE *fp;
{
    time_t mtime;

    mtime = fp_file_mtime(fp);

#ifdef EPOCH_ADJ
    if(mtime != (time_t)(-1))
      mtime -= EPOCH_ADJ;
#endif

    return(mtime);
}

time_t
get_adj_name_file_mtime(name)
    char *name;
{
    time_t mtime;

    mtime = name_file_mtime(name);

#ifdef EPOCH_ADJ
    if(mtime != (time_t)(-1))
      mtime -= EPOCH_ADJ;
#endif

    return(mtime);
}

time_t
get_adj_time()
{
    time_t tt;

    tt = time((time_t *)0);

#ifdef EPOCH_ADJ
    tt -= EPOCH_ADJ;
#endif

    return(tt);
}


int
get_sort_rule_from_disk(fp)
    FILE *fp;
{
    char buf[SIZEOF_SORT_RULE + 1];

    dprint(9, (debugfile, "- get_sort_rule_from_disk -\n"));

    if(!fp)
      return -1;

    if(fseek(fp, (long)TO_FIND_SORT_RULE, 2))
      return -1;

    if(fread(buf, sizeof(char), (unsigned)SIZEOF_SORT_RULE, fp) !=
							    SIZEOF_SORT_RULE)
      return -1;

    buf[SIZEOF_SORT_RULE] = '\0';
    return(atoi(buf));
}


/*
 * Builds the ondisk (and incore) hash file from the ondisk address book.
 * This only happens if the hash file is missing or corrupt.
 */
int
build_ondisk_hash_from_abook(ab, warning)
    AdrBk *ab;
    char  *warning;
{
    FILE          *fp_for_hash;
    FILE          *fp_in;
    EntryRef       e;
    char          *nickname;
    char          *address;
    char          *lc, *temp_hashfile = NULL;
    adrbk_cntr_t   used;
    adrbk_cntr_t   hash;
    long           offset;
    int            max_nick = 0,
		   max_addr = 0, addr_two = 0, addr_three = 0,
		   this_nick_width, this_addr_width;
    int		   longline = 0, rew = 1, fd;
    WIDTH_INFO_S  *widths;
    unsigned long  filesize;

    dprint(9, (debugfile, "- build_ondisk_hash_from_abook -\n"));

    if(!ab || !ab->hashfile || !ab->our_hashcopy || !ab->fp)
      return -1;

    errno = 0;

    if(!(ab->our_hashcopy && ab->our_hashcopy[0])
       || !(temp_hashfile=tempfile_in_same_dir(ab->our_hashcopy,"a2",NULL))
       || (fd = open(temp_hashfile, OPEN_WRITE_MODE, 0600)) < 0){
	if(warning && errno != 0)
	  (void)strncpy(warning, error_description(errno), 200);

	dprint(1, (debugfile, "build_ondisk: open(%s): %s\n",
	       temp_hashfile ? temp_hashfile : "?", error_description(errno)));

	if(temp_hashfile){
	    (void)unlink(temp_hashfile);
	    fs_give((void **)&temp_hashfile);
	}

	return -1;
    }

    fp_for_hash = fdopen(fd, FOPEN_WRITE_MODE);

    fp_in = ab->fp;

    /* get size of file to estimate good hashtable_size */
    if((filesize = (unsigned long)fp_file_size(fp_in)) == (unsigned long)-1L)
      ab->htable_size  = DEFAULT_HTABLE_SIZE;
    else{
	a_c_arg_t     approx_number_of_entries;

	approx_number_of_entries = (a_c_arg_t)(filesize / 50);
	ab->htable_size  = hashtable_size(approx_number_of_entries);
    }

    ab->deleted_cnt  = 0L;  /* number #DELETED- */
    ab->hash_by_nick = new_adrhash((a_c_arg_t)ab->htable_size);
    ab->hash_by_addr = new_adrhash((a_c_arg_t)ab->htable_size);

    if(write_hash_header(fp_for_hash, (a_c_arg_t)ab->htable_size))
      goto io_err;

    used = 0;

    while((nickname =
      skip_to_next_nickname(fp_in,&offset,&address,NULL,
			    rew,&longline)) != NULL){

	rew = 0;

	if(strncmp(nickname, DELETED, DELETED_LEN) == 0
	   && isdigit((unsigned char)nickname[DELETED_LEN])
	   && isdigit((unsigned char)nickname[DELETED_LEN+1])
	   && nickname[DELETED_LEN+2] == '/'
	   && isdigit((unsigned char)nickname[DELETED_LEN+3])
	   && isdigit((unsigned char)nickname[DELETED_LEN+4])
	   && nickname[DELETED_LEN+5] == '/'
	   && isdigit((unsigned char)nickname[DELETED_LEN+6])
	   && isdigit((unsigned char)nickname[DELETED_LEN+7])
	   && nickname[DELETED_LEN+8] == '#'){
	    ab->deleted_cnt++;
	    continue;
	}

	ALARM_BLIP();
	if((long)used > MAX_ADRBK_SIZE){
	    q_status_message2(SM_ORDER | SM_DING, 4, 5,
		    "Max addrbook size is %.200s, %.200s too large, giving up",
			    long2string(MAX_ADRBK_SIZE),
			    (lc=last_cmpnt(ab->filename)) ? lc : ab->filename);
	    dprint(1, (debugfile, "build_ondisk: used=%ld > %s\n",
		   (long)used, long2string(MAX_ADRBK_SIZE)));
	    goto io_err;
	}

	e.uid_nick  = ab_uid(nickname);
	this_nick_width = strlen(nickname);
	e.offset    = offset;
	e.ae        = (AdrBk_Entry *)NULL;
	hash        = ab_hash(nickname, (a_c_arg_t)ab->htable_size);
	e.next_nick = ab->hash_by_nick->harray[hash];
	ab->hash_by_nick->harray[hash] = used;
	if(address && *address != '('){ /* not a list */
	    e.uid_addr  = ab_uid_addr(address);
	    this_addr_width = strlen(address);
	    hash        = ab_hash_addr(address, (a_c_arg_t)ab->htable_size);
	    e.next_addr = ab->hash_by_addr->harray[hash];
	    ab->hash_by_addr->harray[hash] = used;
	}
	else{
	    /*
	     * It doesn't really matter what we put here. The list may or
	     * may not be expanded, so we don't really know what we're
	     * trying to display. The numbers we're generating here are
	     * just to bootstrap us anyway, so just choose an arbitrary
	     * number that will look ok if we use it.
	     */
	    this_addr_width = 25;
	    e.uid_addr  = NO_UID;
	    e.next_addr = NO_NEXT;
	}

	used++;
	if(write_single_entryref(&e, fp_for_hash))
	  goto io_err;

	/*
	 * Keep track of widths.  These are only approximate.  If we ever
	 * do an adrbk_write we'll get the exact numbers.  We don't have
	 * any idea of fullname widths so we'll just use the same as the
	 * addrfield widths to get the drawing off the ground.  Same for
	 * the fcc widths.
	 */
	max_nick = max(max_nick, this_nick_width);
	if(this_addr_width > max_addr){
	    addr_three = addr_two;
	    addr_two   = max_addr;
	    max_addr   = this_addr_width;
	}
	else if(this_addr_width > addr_two){
	    addr_three = addr_two;
	    addr_two   = this_addr_width;
	}
	else if(this_addr_width > addr_three){
	    addr_three = this_addr_width;
	}
    }

    if(longline){
	if(warning)
	  (void)strncpy(warning, "line too long: must be fixed by hand", 200);

	dprint(1, (debugfile, "build_ondisk: line too long\n"));
	errno = 0;
	goto io_err;
    }
    
    ab->count = used;
    ab->phantom_count = ab->count;
    ab->orig_count    = ab->count;

    widths = &ab->widths;
    widths->max_nickname_width  = min(max_nick, 99);
    widths->max_addrfield_width = min(max_addr, 99);
    widths->third_biggest_addrfield_width = min(addr_three, 99);
    widths->max_fullname_width  = min(max_addr, 99);
    widths->max_fccfield_width = min(max_addr, 99);
    widths->third_biggest_fullname_width  = min(addr_three, 99);
    widths->third_biggest_fccfield_width = min(addr_three, 99);

    if(write_hash_table(ab->hash_by_nick, fp_for_hash,
	(a_c_arg_t)ab->htable_size))
      goto io_err;

    if(write_hash_table(ab->hash_by_addr, fp_for_hash,
	(a_c_arg_t)ab->htable_size))
      goto io_err;

    if(write_hash_trailer(ab, fp_for_hash, 0))
      goto io_err;

    if(fclose(fp_for_hash) == EOF){
	dprint(1, (debugfile, "build_ondisk: fclose: %s\n",
	       error_description(errno)));
	goto io_err;
    }

    file_attrib_copy(temp_hashfile, ab->our_hashcopy);
    if(ab->fp_hash){
	(void)fclose(ab->fp_hash);
	ab->fp_hash = NULL;		/* in case of problems */
    }

    if(rename_file(temp_hashfile, ab->our_hashcopy) < 0){
	(void)unlink(temp_hashfile);
	dprint(1, (debugfile, "build_ondisk: rename_file(%s,%s): %s\n",
	       temp_hashfile ? temp_hashfile : "?",
	       ab->our_hashcopy ? ab->our_hashcopy : "?",
	       error_description(errno)));
	goto io_err;
    }
    
    ab->fp_hash = fopen(ab->our_hashcopy, READ_MODE);

    if(temp_hashfile){
	(void)unlink(temp_hashfile);
	fs_give((void **)&temp_hashfile);
    }

    /*
     * Now copy our_hashcopy back to hashfile.
     * If this fails we don't worry about it, because the next user
     * can rebuild it themselves.
     */
    if(ab->hashfile != ab->our_hashcopy &&
       ab->fp_hash &&
       ab->hashfile_access == ReadWrite){
	int   c, err = 0;

	if(!(ab->hashfile && ab->hashfile[0])
	   || !(temp_hashfile=tempfile_in_same_dir(ab->hashfile,"a2",NULL))
	   || (fd = open(temp_hashfile, OPEN_WRITE_MODE, 0600)) < 0){
	    dprint(1, (debugfile, "build_ondisk: open2(%s): %s\n",
		   temp_hashfile ? temp_hashfile : "?",
		   error_description(errno)));
	    err++;
	}

	if(!err && !(fp_for_hash = fdopen(fd, FOPEN_WRITE_MODE))){
	    dprint(1, (debugfile, "build_ondisk: fdopen: %s\n",
		   error_description(errno)));
	    err++;
	}

	if(!err){
	    rewind(ab->fp_hash);
	    while((c = getc(ab->fp_hash)) != EOF)
	      if(putc(c, fp_for_hash) == EOF){
		  dprint(1, (debugfile, "build_ondisk: putc: %s\n",
			 error_description(errno)));
		  err++;
		  break;
	      }
	}

	if(!err && fclose(fp_for_hash) == EOF){
	    dprint(1, (debugfile, "build_ondisk: fclose2: %s\n",
		   error_description(errno)));
	    err++;
	}

	if(!err){
	    file_attrib_copy(temp_hashfile, ab->hashfile);
	    if(rename_file(temp_hashfile, ab->hashfile) < 0){
		dprint(1, (debugfile, "build_ondisk: rename_file(%s,%s): %s\n",
		       temp_hashfile ? temp_hashfile : "?",
		       ab->hashfile ? ab->hashfile : "?",
		       error_description(errno)));
		err++;
	    }
	}

	if(err){
	    if(temp_hashfile){
		(void)unlink(temp_hashfile);
		fs_give((void **)&temp_hashfile);
	    }

	    dprint(2, (debugfile, "build_ondisk: failed copying our_hashcopy back to hashfile, continuing\n"));
	}
    }

    if(temp_hashfile){
	(void)unlink(temp_hashfile);
	fs_give((void **)&temp_hashfile);
    }

    return 0;

io_err:
    if(warning && errno != 0)
      (void)strncpy(warning, error_description(errno), 200);

    dprint(1, (debugfile, "build_ondisk: io_err: %s\n",
	   error_description(errno)));

    if(ab->fp_hash){
	(void)fclose(ab->fp_hash);
	ab->fp_hash = (FILE *)NULL;
    }

    free_ab_adrhash(&ab->hash_by_nick);
    free_ab_adrhash(&ab->hash_by_addr);

    if(temp_hashfile){
	(void)unlink(temp_hashfile);
	fs_give((void **)&temp_hashfile);
    }

    return -1;
}


static char space[] = " ";

/*
 * Returns next nickname, or NULL
 *
 * The offset arg is the offset of the nickname in the file, returned to caller.
 * The address arg is a pointer to the address, returned to caller.
 * The length arg is returned to caller.  It is length of entire entry.
 * If rew is set, rewind the file and start over at beginning.
 * Longline is returned equal to 1 if an input line is too long.
 */
char *
skip_to_next_nickname(fp, offset, address, length, rew, longline)
    FILE  *fp;
    long  *offset;
    char **address;
    long  *length;
    int    rew;
    int   *longline;
{
    char *line = NULL;
    static char linebuf[MAXLINE+INDENT+1];
    static char this_nickname[MAX_NICKNAME+1];
    static char next_nickname[MAX_NICKNAME+1];
    static char this_address[MAXLINE+1];
    static char next_address[MAXLINE+1];
    char *p;
    int   c;
    char *nickname;
    char *addr;
    static long next_nickname_offset = 0L;
    int ok_so_far = 0;
    size_t totsize;
#define CHUNKSIZE 500


    line = linebuf;
    totsize = sizeof(linebuf);

    if(address)
      *address = this_address;

    if(rew){
	rewind(fp);
	/* skip leading (bogus) continuation lines */
	do{
	    *offset  = ftell(fp);
	    memset(line, 0, totsize);
	    p = fgets(line, totsize, fp);

	    if(p == NULL){
		if(line != linebuf)
		  fs_give((void **) &line);

		return NULL;
	    }

	    /* line is too long to fit */
	    while(line[totsize-2] != '\0' && line[totsize-2] != '\n'
		  && p != NULL){
		if(totsize == sizeof(linebuf)){		/* first time */
		    line = (char *) fs_get(totsize+CHUNKSIZE);
		    memset(line, 0, totsize+CHUNKSIZE);
		    strncpy(line, linebuf, totsize);
		}
		else{
		    fs_resize((void **) &line, totsize+CHUNKSIZE);
		    memset(line+totsize, 0, CHUNKSIZE);
		}

		/* get next CHUNKSIZE characters */
		p = fgets(line+totsize-1, CHUNKSIZE+1, fp);
		totsize += CHUNKSIZE;
	    }
	}while(line[0] == SPACE);

	p = line;
	nickname = p;
	SKIP_TO_TAB(p);
	/* This *should* be true. */
	if(*p == TAB)
	  ok_so_far++;

	*p = '\0';
	/*
	 * We want nickname of "" to be treated as an empty nickname, but
	 * not to end the addrbook.
	 */
	if(!*nickname)
	  nickname = space;

	strncpy(next_nickname, nickname, sizeof(next_nickname)-1);
	next_nickname[sizeof(next_nickname)-1] = '\0';
	next_nickname_offset = *offset;
	/* locate address field */
	if(!ok_so_far) /* no tab after nickname */
	  goto no_address_initially;

	p++;
	if(*p == '\n' || *p == '\r'){ /* get a continuation line */
	    c = getc(fp);
	    if(c == '\n' && *p == '\r')  /* handle CRLF's */
	      c = getc(fp);

	    if(c != SPACE)
	      ok_so_far = 0;

	    (void)ungetc(c, fp);
	    if(ok_so_far){
		memset(line, 0, totsize);
		p = fgets(line, totsize, fp);

		/* line is too long to fit */
		while(line[totsize-2] != '\0' && line[totsize-2] != '\n'
		      && p != NULL){
		    if(totsize == sizeof(linebuf)){		/* first time */
			line = (char *) fs_get(totsize+CHUNKSIZE);
			memset(line, 0, totsize+CHUNKSIZE);
			strncpy(line, linebuf, totsize);
		    }
		    else{
			fs_resize((void **) &line, totsize+CHUNKSIZE);
			memset(line+totsize, 0, CHUNKSIZE);
		    }

		    /* get next CHUNKSIZE characters */
		    p = fgets(line+totsize-1, CHUNKSIZE+1, fp);
		    totsize += CHUNKSIZE;
		}
	    }

	    p = line;
	    if(!ok_so_far || *p == '\0'){
		ok_so_far = 0;
		goto no_address_initially;
	    }
	}

	/* skip fullname field */
	SKIP_TO_TAB(p);
	if(*p != TAB){
	    ok_so_far = 0;
	    goto no_address_initially;
	}

	p++;
	if(*p == '\n' || *p == '\r'){ /* get a continuation line */
	    c = getc(fp);
	    if(c == '\n' && *p == '\r')  /* handle CRLF's */
	      c = getc(fp);

	    if(c != SPACE)
	      ok_so_far = 0;

	    (void)ungetc(c, fp);
	    if(ok_so_far){
		memset(line, 0, totsize);
		p = fgets(line, totsize, fp);

		/* line is too long to fit */
		while(line[totsize-2] != '\0' && line[totsize-2] != '\n'
		      && p != NULL){
		    if(totsize == sizeof(linebuf)){		/* first time */
			line = (char *) fs_get(totsize+CHUNKSIZE);
			memset(line, 0, totsize+CHUNKSIZE);
			strncpy(line, linebuf, totsize);
		    }
		    else{
			fs_resize((void **) &line, totsize+CHUNKSIZE);
			memset(line+totsize, 0, CHUNKSIZE);
		    }

		    /* get next CHUNKSIZE characters */
		    p = fgets(line+totsize-1, CHUNKSIZE+1, fp);
		    totsize += CHUNKSIZE;
		}
	    }

	    p = line;
	    if(!ok_so_far || *p == '\0'){
		ok_so_far = 0;
	        goto no_address_initially;
	    }
	}

	SKIP_SPACE(p);

no_address_initially:

	if(ok_so_far){
	    addr = p;
	    SKIP_TO_TAB(p);
	    *p = '\0';
	    strncpy(next_address, addr, sizeof(next_address)-1);
	    next_address[sizeof(next_address)-1] = '\0';
	}
	else
	  next_address[0] = '\0';  /* won't happen with good input data */
    }

    if(next_nickname[0] == '\0'){
	if(line != linebuf)
	  fs_give((void **) &line);

	return NULL;
    }

    strcpy(this_nickname, next_nickname);
    *offset = next_nickname_offset;
    strcpy(this_address, next_address);

    /* skip continuation lines */
    do{
	next_nickname_offset = ftell(fp);
	memset(line, 0, totsize);
	p = fgets(line, totsize, fp);

	/* line is too long to fit */
	while(line[totsize-2] != '\0' && line[totsize-2] != '\n'
	      && p != NULL){
	    if(totsize == sizeof(linebuf)){		/* first time */
		line = (char *) fs_get(totsize+CHUNKSIZE);
		memset(line, 0, totsize+CHUNKSIZE);
		strncpy(line, linebuf, totsize);
	    }
	    else{
		fs_resize((void **) &line, totsize+CHUNKSIZE);
		memset(line+totsize, 0, CHUNKSIZE);
	    }

	    /* get next CHUNKSIZE characters */
	    p = fgets(line+totsize-1, CHUNKSIZE+1, fp);
	    totsize += CHUNKSIZE;
	}
    }while(line[0] == SPACE);

    p = line;
    if(*p){
	nickname = p;
	SKIP_TO_TAB(p);
	if(*p == TAB)  /* this should always happen */
	  ok_so_far++;
	else
	  ok_so_far = 0;

	*p = '\0';
	/*
	 * We want nickname of "" to be treated as an empty nickname, but
	 * not to end the addrbook.
	 */
	if(!*nickname)
	  nickname = space;

	strncpy(next_nickname, nickname, sizeof(next_nickname)-1);
	next_nickname[sizeof(next_nickname)-1] = '\0';
	/* locate address field */
	if(!ok_so_far) /* no tab after nickname */
	  goto no_address;

	p++;
	if(*p == '\n' || *p == '\r'){ /* get a continuation line */
	    c = getc(fp);
	    if(c == '\n' && *p == '\r')  /* handle CRLF's */
	      c = getc(fp);

	    if(c != SPACE)
	      ok_so_far = 0;

	    (void)ungetc(c, fp);
	    if(ok_so_far){
		memset(line, 0, totsize);
		p = fgets(line, totsize, fp);

		/* line is too long to fit */
		while(line[totsize-2] != '\0' && line[totsize-2] != '\n'
		      && p != NULL){
		    if(totsize == sizeof(linebuf)){		/* first time */
			line = (char *) fs_get(totsize+CHUNKSIZE);
			memset(line, 0, totsize+CHUNKSIZE);
			strncpy(line, linebuf, totsize);
		    }
		    else{
			fs_resize((void **) &line, totsize+CHUNKSIZE);
			memset(line+totsize, 0, CHUNKSIZE);
		    }

		    /* get next CHUNKSIZE characters */
		    p = fgets(line+totsize-1, CHUNKSIZE+1, fp);
		    totsize += CHUNKSIZE;
		}
	    }

	    p = line;
	    if(!ok_so_far || *p == '\0'){
		ok_so_far = 0;
		goto no_address;
	    }
	}

	/* skip fullname field */
	SKIP_TO_TAB(p);
	if(*p != TAB){
	    ok_so_far = 0;
	    goto no_address;
	}

	p++;
	if(*p == '\n' || *p == '\r'){ /* get a continuation line */
	    c = getc(fp);
	    if(c == '\n' && *p == '\r')  /* handle CRLF's */
	      c = getc(fp);

	    if(c != SPACE)
	      ok_so_far = 0;

	    (void)ungetc(c, fp);
	    if(ok_so_far){
		memset(line, 0, totsize);
		p = fgets(line, totsize, fp);

		/* line is too long to fit */
		while(line[totsize-2] != '\0' && line[totsize-2] != '\n'
		      && p != NULL){
		    if(totsize == sizeof(linebuf)){		/* first time */
			line = (char *) fs_get(totsize+CHUNKSIZE);
			memset(line, 0, totsize+CHUNKSIZE);
			strncpy(line, linebuf, totsize);
		    }
		    else{
			fs_resize((void **) &line, totsize+CHUNKSIZE);
			memset(line+totsize, 0, CHUNKSIZE);
		    }

		    /* get next CHUNKSIZE characters */
		    p = fgets(line+totsize-1, CHUNKSIZE+1, fp);
		    totsize += CHUNKSIZE;
		}
	    }

	    p = line;
	    if(!ok_so_far || *p == '\0'){
		ok_so_far = 0;
	        goto no_address;
	    }
	}

	SKIP_SPACE(p);

no_address:

	if(ok_so_far){
	    addr = p;
	    SKIP_TO_TAB(p);
	    *p = '\0';
	    strncpy(next_address, addr, sizeof(next_address)-1);
	    next_address[sizeof(next_address)-1] = '\0';
	}
	else
	  next_address[0] = '\0';  /* shouldn't happen */
    }
    else
      next_nickname[0] = '\0';

    if(length)
      *length = next_nickname_offset - *offset;

    if(line != linebuf)
      fs_give((void **) &line);

    return(this_nickname);
}


EntryRef *
new_entryref(uid_nickname, uid_address, offset)
    adrbk_uid_t uid_nickname;
    adrbk_uid_t uid_address;
    long        offset;
{
    EntryRef *e;

    e            =  (EntryRef *)fs_get(sizeof(EntryRef));
    e->uid_nick  =  uid_nickname;
    e->uid_addr  =  uid_address;
    e->offset    =  offset;
    e->next_nick =  NO_NEXT;
    e->next_addr =  NO_NEXT;
    e->ae        =  (AdrBk_Entry *)NULL;
    e->is_deleted = 0;

    return(e);
}


/*
 * Returns the hash value of name, which will be in the range 0 ... size-1.
 * This is a standard hash function that should be as evenly distributed
 * as possible.  I haven't done much research to try to find a good one.
 * Most important is the distribution of hashing of all the nicknames in
 * big addrbooks.  Hashing of all the Single addresses is also important.
 */
adrbk_cntr_t
ab_hash(name, size)
    char     *name;
    a_c_arg_t size;
{
    register usign32_t h = 0L;
    register usign32_t c;
    int at_most_this_many_chars_in_hash = MAX_CHARS_IN_HASH;
    int four_counter = 0;
    int two_counter = 1;

    if(!name)
      return((adrbk_cntr_t)h);

    /* Make the hash case independent */
    while((c = *name++) && at_most_this_many_chars_in_hash-- > 0){
	if(isspace((unsigned char)c))
	  continue;  /* so we don't have to worry about trimming spaces */

	if(isupper((unsigned char)c))
	  c = tolower((unsigned char)c);

	/*
	 * We're relying on usign32_t being a true 32 bit unsigned.  If it
	 * is larger than 32 bits, we'll get different hash values on
	 * that system than on a true 32 bit system.  We could probably
	 * figure out some sort of masking strategy to protect ourselves.
	 */
	switch(four_counter){
	  case 0:
	    h += (two_counter ? (c << 24) : (c << 25));
	    break;
	  case 1:
	    h += (two_counter ? (c << 16) : (c << 17));
	    break;
	  case 2:
	    h += (two_counter ? (c << 8) : (c << 9));
	    break;
	  case 3:
	    h += (two_counter ? c : (c << 1));
	    break;
	}
	four_counter = (four_counter + 1) % 4;
	if(four_counter == 0)
	  two_counter = (two_counter + 1) % 2;
    }
    
    return(h % (adrbk_cntr_t)size);
}


/*
 * This is the same as ab_hash above, but it assumes that name is an address
 * string and strips off the non-address part before computing the hash value.
 * That is, it turns Joe College <joe@college.edu> into joe@college.edu
 * for hashing purposes.
 */
adrbk_cntr_t
ab_hash_addr(name, size)
    char     *name;
    a_c_arg_t size;
{
    char *start_addr = NULL;
    char *end_addr   = NULL;
    adrbk_cntr_t h = (adrbk_cntr_t)0L;
    char  save;

    if(!name)
      return(h);
    
    strip_addr_string(name, &start_addr, &end_addr);
    if(start_addr){
	save = *end_addr;
	*end_addr = '\0';
	h = ab_hash(start_addr, size);
	*end_addr = save;
    }

    return(h);
}


AdrHash *
new_adrhash(size)
    a_c_arg_t size;
{
    AdrHash *a;

    a = (AdrHash *)fs_get(sizeof(AdrHash));
    a->harray = (adrbk_cntr_t *)fs_get((size_t)size * sizeof(adrbk_cntr_t));

    /*
     * The ff initialization causes the next_nick and next_addr pointers to
     * be set to NO_NEXT.
     */
    memset(a->harray, 0xff, (size_t)size * sizeof(adrbk_cntr_t));

    return(a);
}


void
init_adrhash_array(a, size)
    AdrHash     *a;
    a_c_arg_t    size;
{
    dprint(9, (debugfile, "- init_adrhash_array -\n"));

    /*
     * The ff initialization causes the next_nick and next_addr pointers to
     * be set to NO_NEXT.
     */
    memset(a->harray, 0xff, (size_t)size * sizeof(adrbk_cntr_t));
}


void
free_ab_adrhash(a)
    AdrHash **a;
{
    if(!(*a))
      return;

    if((*a)->harray)
      fs_give((void **)&((*a)->harray));

    fs_give((void **)a);
}


/*
 * Returns a value which is probably unique for name.  That is, if name1 and
 * name2 are not the same, then uid(name1) probably not equal to uid(name2).
 * Actually, they only have to be unique within a given hash bucket.  That
 * is, we don't want both ab_uid(name1) == ab_uid(name2) and
 *                       ab_hash(name1) == ab_hash(name2).
 *
 * Uid should not be NO_UID so we can tell when it hasn't been initialized.
 */
adrbk_uid_t
ab_uid(name)
    char *name;
{
    register adrbk_uid_t u = (adrbk_uid_t)0;
    int at_most_this_many_chars_in_uid = MAX_CHARS_IN_HASH;
    int c;

    if(!name)
      return(u);

    /* Make the uid case independent and only depend on first N chars */
    while((c = *name++) && at_most_this_many_chars_in_uid-- > 0){
	if(isspace((unsigned char)c))
	  continue;  /* so we don't have to worry about trimming spaces */

	if(isupper((unsigned char)c))
	  c = tolower((unsigned char)c);

	/* this comes from emacs, I think */
	u = ((((u << 4) & 0xffffffff) + (u >> 24)) & 0x0fffffff) + c;
    }
    
    if(u == NO_UID)
      u++;

    return(u);
}


/*
 * This is the same as ab_uid above, but it assumes that name is an address
 * string and strips off the non-address part before computing the uid value.
 * That is, it turns Joe College <joe@college.edu> into joe@college.edu
 * for uid purposes.
 */
adrbk_uid_t
ab_uid_addr(name)
    char     *name;
{
    char *start_addr = NULL;
    char *end_addr   = NULL;
    adrbk_uid_t u    = NO_UID;
    char  save;

    if(!name)
      return(NO_UID);
    
    strip_addr_string(name, &start_addr, &end_addr);
    if(start_addr){
	save = *end_addr;
	*end_addr = '\0';
	u = ab_uid(start_addr);
	*end_addr = save;
    }

    return(u);
}


/*
 * Returns a pointer to the start of the mailbox@host part of this
 * address string, and a pointer to the end + 1.  The caller can then
 * replace the end char with \0 and call the hash function, then put
 * back the end char.  Start_addr and end_addr are assumed to be non-null.
 */
void
strip_addr_string(addrstr, start_addr, end_addr)
    char  *addrstr;
    char **start_addr;
    char **end_addr;
{
    register char *q;
    int in_quotes  = 0,
        in_comment = 0;
    char prev_char = '\0';

    if(!addrstr || !*addrstr){
	*start_addr = NULL;
	*end_addr = NULL;
	return;
    }

    *start_addr = addrstr;

    for(q = addrstr; *q; q++){
	switch(*q){
	  case '<':
	    if(!in_quotes && !in_comment){
		if(*++q){
		    *start_addr = q;
		    /* skip to > */
		    while(*q && *q != '>')
		      q++;

		    /* found > */
		    if(*q){
			*end_addr = q;
			return;
		    }
		    else
		      q--;
		}
	    }

	    break;

	  case LPAREN:
	    if(!in_quotes && !in_comment)
	      in_comment = 1;
	    break;

	  case RPAREN:
	    if(in_comment && prev_char != BSLASH)
	      in_comment = 0;
	    break;

	  case QUOTE:
	    if(in_quotes && prev_char != BSLASH)
	      in_quotes = 0;
	    else if(!in_quotes && !in_comment)
	      in_quotes = 1;
	    break;

	  default:
	    break;
	}

	prev_char = *q;
    }

    *end_addr = q;
}


/*
 * Given an EntryRef, return the AdrBk_Entry that it points to.  It may
 * already be cached.
 */
AdrBk_Entry *
init_ae_entry(ab, entry)
    AdrBk    *ab;
    EntryRef *entry;
{
    char *p, *lc;
    char *buf; /* read entry in here */
    int   ret, length;
    int   first_entry = 0;
    char *addrfield = (char *)NULL;
    char *addrfield_end;
    AdrBk_Entry *a = (AdrBk_Entry *)NULL;
    char *nickname, *fullname, *fcc, *extra;
    long  offset_of_prev_char;
    time_t mtime;

    if(!entry) /* shouldn't ever happen */
      return(a);

    /* already cached earlier */
    if(entry->ae)
      return(entry->ae);

    a = adrbk_newentry();
    entry->ae = a;

    if(!ab){
	dprint(2, (debugfile, "init_ae_entry: found trouble: ab is NULL\n"));
	goto trouble;
    }

    length = length_of_entry(ab->fp, entry->offset);
    if(length <= 0){
	dprint(2, (debugfile, "init_ae_entry: found trouble: length=%d\n",
	    length));
	goto trouble;
    }

    offset_of_prev_char = entry->offset;
    if(offset_of_prev_char > 0L){
	offset_of_prev_char--;
	length++;
    }
    else
      first_entry++;

    if(fseek(ab->fp, offset_of_prev_char, 0)){
	dprint(2, (debugfile,
	    "init_ae_entry: found trouble: fseek to %ld failed\n",
	    offset_of_prev_char));
	goto trouble;
    }

    /* now pointing at the entry (or one before the entry if not first) */
    buf = (char *)fs_get(length * sizeof(char) + 1);
    ret = fread(buf, sizeof(char), (unsigned)length, ab->fp);
    if(ret != length){
	dprint(2, (debugfile,
	    "init_ae_entry: found trouble: fread returned %d instead of %d\n",
	    ret, length));
	goto trouble;
    }

    buf[length] = '\0';
    /*
     * Check to see if things look ok at this offset.
     */
    p = buf;
    if(!first_entry){
	if(!(*p == '\n' || *p == '\r')){
	    dprint(2, (debugfile,
	       "init_ae_entry: trouble: char before nick at %ld not CR or NL\n",
		entry->offset));
	    dprint(2, (debugfile, "             : buf = >%s<\n",
		   buf ? buf : "?"));
	    goto trouble;
	}

	p++;
    }

    /* done checking for trouble */

    REPLACE_NEWLINES_WITH_SPACE(p);

    nickname = p;
    SKIP_TO_TAB(p);
    if(!*p){
	RM_END_SPACE(nickname, p);
	a->nickname = cpystr(nickname);
    }
    else{
	*p = '\0';
	RM_END_SPACE(nickname, p);
	a->nickname = cpystr(nickname);
	p++;
	SKIP_SPACE(p);
	fullname = p;
	SKIP_TO_TAB(p);
	if(!*p){
	    RM_END_SPACE(fullname, p);
	    a->fullname = cpystr(fullname);
	}
	else{
	    *p = '\0';
	    RM_END_SPACE(fullname, p);
	    a->fullname = cpystr(fullname);
	    p++;
	    SKIP_SPACE(p);
	    addrfield = p;
	    SKIP_TO_TAB(p);
	    if(!*p){
		RM_END_SPACE(addrfield, p);
	    }
	    else{
		*p = '\0';
		RM_END_SPACE(addrfield, p);
		p++;
		SKIP_SPACE(p);
		fcc = p;
		SKIP_TO_TAB(p);
		if(!*p){
		    RM_END_SPACE(fcc, p);
		    a->fcc = cpystr(fcc);
		}
		else{
		    char *src, *dst;

		    *p = '\0';
		    RM_END_SPACE(fcc, p);
		    a->fcc = cpystr(fcc);
		    p++;
		    SKIP_SPACE(p);
		    extra = p;
		    p = extra + strlen(extra);
		    RM_END_SPACE(extra, p);
		    /*
		     * When we wrap long comments we insert an extra colon
		     * in the wrap so we can spot it and take it back out.
		     * Pretty much a hack since we thought of it a long
		     * time after designing it, but it eliminates the limit
		     * on length of comments. Here we are looking for
		     * <SP> <SP> : <SP> or
		     * <SP> <SP> <SP> : <SP> and replacing it with <SP>.
		     * There could have been a single \n or \r\n, so that
		     * is why we check for 2 or 3 spaces before the colon.
		     * (This was another mistake.)
		     */
		    dst = src = extra;
		    while(*src != '\0'){
			if(*src == SPACE && *(src+1) == SPACE &&
			   *(src+2) == ':' && *(src+3) == SPACE){
			    /*
			     * If there was an extra space because of the
			     * CRLF (instead of LF) then we already put
			     * a SP in dst last time through the loop
			     * and don't need to add another.
			     */
			    if(src == extra || *(src-1) != SPACE)
			      *dst++ = *src;

			    src += 4;
			}
			else
			  *dst++ = *src++;
		    }

		    *dst = '\0';
		    a->extra = cpystr(extra);
		}
	    }
	}
    }

    /* parse addrfield */
    if(addrfield){
	if(*addrfield == '('){  /* it's a list */
	    a->tag = List;
	    p = addrfield;
	    addrfield_end = p + strlen(p);

	    /*
	     * Get rid of the parens.
	     * If this isn't true the input file is messed up.
	     */
	    if(p[strlen(p)-1] == ')'){
		p[strlen(p)-1] = '\0';
		p++;
		a->addr.list = parse_addrlist(p);
	    }
	    else{
		/* put back what was there to start with */
		*addrfield_end = ')';
		a->addr.list = (char **)fs_get(sizeof(char *) * 2);
		a->addr.list[0] = cpystr(addrfield);
		a->addr.list[1] = NULL;
		dprint(2,
		    (debugfile,
		 "parsing error reading addressbook: missing right paren: %s\n",
		    addrfield ? addrfield : "?"));
	    }
	}
	else{  /* A plain, single address */

	    a->addr.addr = cpystr(addrfield);
	    a->tag       = Single;
	}
    }
    else{
	/*
	 * If no addrfield, assume an empty Single.
	 */
	a->addr.addr = cpystr("");
	a->tag       = Single;
    }

    fs_give((void **)&buf);

    return(a);

trouble:
    /*
     * Some other process must have changed the on-disk addrbook out from
     * under us.  Either that, or the hash file must be messed up.  We need
     * to close down the files and re-open them, else our pointers will
     * point at crazy places.
     *
     * Attempt to verify the change by stat'ing the files.  If mtime didn't
     * change we'll hope for the best instead of restarting.  We don't want
     * to get into a restart loop when the file really isn't changing
     * but the trouble is being triggered for some other reason.
     *
     * However, we do want to try to rebuild at least once even if the mtime
     * looks ok, because somebody may have copied a valid looking .lu file
     * onto our .lu file (one with the same number of entries).  So we have
     * this special ad hoc counter (forced_rebuilds) that lets us try to
     * rebuild a few times regardless of the mtimes.  We also have the
     * safety net (trouble_rebuilds) to stop us eventually if we get looping
     * somehow.
     */
    dprint(1, (debugfile,
      "\n\n ADDR    ::: the addressbook file %s and its lookup file %s\n",
      (ab && ab->filename) ? ab->filename : "?",
      (ab && ab->hashfile) ? ab->hashfile : "?"));
    dprint(1, (debugfile,
      " BOOK    ::: are not consistent with one another.  The lookup\n"));
    dprint(1, (debugfile,
      " TROUBLE ::: file may have to be removed and rebuilt.\n"));
    dprint(1, (debugfile,
     "         ::: Usually it will fix itself, but if it doesn't, or if it\n"));
    dprint(1, (debugfile,
      "         ::: is building temporary lookup files for each user,\n"));
    dprint(1, (debugfile,
      "         ::: the sys admin should rebuild it (%s).\n\n",
      (ab && ab->hashfile) ? ab->hashfile : "?"));
    if(((ab && ab->last_change_we_know_about != (time_t)(-1) &&
	 (mtime=get_adj_name_file_mtime(ab->filename)) != (time_t)(-1) &&
	 ab->last_change_we_know_about != mtime) ||
	forced_rebuilds < MAX_FORCED_REBUILDS) &&
		 trouble_rebuilds < MAX_TROUBLE_REBUILDS){

	q_status_message(SM_ORDER | SM_DING, 5, 5,
	   "Addrbook has changed unexpectedly, need to re-sync...");
	if(writing){
	   writing = 0;
	   q_status_message(SM_ORDER, 3, 5,
	       "Aborting our change to avoid damage...");
	}

	dprint(1, (debugfile,
	    "addrbook %s changed while we had it open, longjmp\n",
	    (ab && ab->filename) ? ab->filename : "?"));
        if(ab && ab->last_change_we_know_about == (time_t)(-1) ||
	   mtime == (time_t)(-1) ||
	   ab->last_change_we_know_about == mtime)
	    forced_rebuilds++;

	trouble_rebuilds++;
	/* jump back to a safe place */
	trouble_filename = (ab && ab->orig_filename)
			    ? cpystr(ab->orig_filename)
			    : cpystr("");
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }

    dprint(1, (debugfile,
	"addrbook trouble (%s), but we're returning null nickname\n",
	(ab && ab->filename) ? ab->filename : "?"));
    dprint(1, (debugfile,
	"Advised user to remove %s and restart pine\n",
	(ab && ab->hashfile) ? ab->hashfile : "?"));
    dprint(1, (debugfile,
	"If %s not owned by user, sys admin may have to rebuild it\n",
	(ab && ab->hashfile) ? ab->hashfile : "?"));
    if(trouble_rebuilds++ < MAX_TROUBLE_REBUILDS + 3)
      q_status_message1(SM_ORDER, 3, 10,
	"Index file %.200s inconsistent...remove it and restart Pine",
	(ab && ab->hashfile) ? ((lc=last_cmpnt(ab->hashfile))
				     ? lc : ab->hashfile)
			     : "?");

    fs_give((void **)&a);
    entry->ae = (AdrBk_Entry *)NULL;
    return((AdrBk_Entry *)NULL);
}


/*
 * returns length of the address book entry starting at offset
 */
int
length_of_entry(fp, offset)
    FILE *fp;
    long  offset;
{
    char line[MAXLINE+INDENT+1];
    char *p;
    int  first_char;
    long new_offset;

    errno = 0;
    line[0] = '\0';
    if(!fp){
	dprint(2, (debugfile, "length_of_entry: fp is NULL\n"));
	return -1;
    }

    if(fseek(fp, offset, 0)){
	dprint(2, (debugfile, "length_of_entry: fseek at %ld failed\n",
	       offset));
	return -1;
    }

    clearerr(fp);
    errno = 0;
    memset(line, 0, sizeof(line));
    /* at least one line belongs to this entry */
    p = fgets(line, sizeof(line), fp);
    new_offset = ftell(fp);
    /* read until end of long line, keep resetting offset */
    while(line[sizeof(line)-2] != '\0' && line[sizeof(line)-2] != '\n'){
	memset(line, 0, sizeof(line));
	p = fgets(line, sizeof(line), fp);
	new_offset = ftell(fp);
    }

    /*
     * Get more lines and keep counting them as part of this entry if
     * they are continuation lines.
     */
    do{
	memset(line, 0, sizeof(line));
	p = fgets(line, sizeof(line), fp);
	first_char = p ? *p : '\0';
	if(first_char == SPACE)
	  new_offset = ftell(fp);

	/* read until end of long line, keep resetting offset */
	while(first_char == SPACE
	      && line[sizeof(line)-2] != '\0' && line[sizeof(line)-2] != '\n'){
	    memset(line, 0, sizeof(line));
	    p = fgets(line, sizeof(line), fp);
	    new_offset = ftell(fp);
	}
    }while(first_char == SPACE);
    
    if(new_offset <= offset){
	dprint(2,(debugfile,"length_of_entry: trouble: return length=%ld, %s\n",
	    new_offset-offset, error_description(errno)));
	dprint(2, (debugfile, "    offset=%ld, p=%s\n", offset,
	    p ? (*p ? p : "<empty>") : "<null>"));
	dprint(2, (debugfile, "    line=%s\n", *line ? line : "<empty>"));
	if(p == NULL)
	  dprint(2, (debugfile, "    ferror(fp)=%d, feof(fp)=%d\n",
	    ferror(fp), feof(fp)));
    }

    return((int)(new_offset - offset));
}


/*
 * Args  cur -- pointer to the start of the current addr in list.
 *
 * Returns a pointer to the start of the next addr or NULL if there are
 * no more addrs.
 *
 * Side effect: current addr has trailing white space removed
 * and is null terminated.
 */
char *
skip_to_next_addr(cur)
    char *cur;
{
    register char *p,
		  *q;
    char          *ret_pointer;
    int in_quotes  = 0,
        in_comment = 0;
    char prev_char = '\0';

    /*
     * Find delimiting comma or end.
     * Quoted commas and commented commas don't count.
     */
    for(q = cur; *q; q++){
	switch(*q){
	  case COMMA:
	    if(!in_quotes && !in_comment)
	      goto found_comma;
	    break;

	  case LPAREN:
	    if(!in_quotes && !in_comment)
	      in_comment = 1;
	    break;

	  case RPAREN:
	    if(in_comment && prev_char != BSLASH)
	      in_comment = 0;
	    break;

	  case QUOTE:
	    if(in_quotes && prev_char != BSLASH)
	      in_quotes = 0;
	    else if(!in_quotes && !in_comment)
	      in_quotes = 1;
	    break;

	  default:
	    break;
	}

	prev_char = *q;
    }
    
found_comma:
    if(*q){  /* trailing comma case */
	*q = '\0';
	ret_pointer = q + 1;
    }
    else
      ret_pointer = NULL;  /* no more addrs after cur */

    /* remove trailing white space from cur */
    for(p = q - 1; p >= cur && isspace((unsigned char)*p); p--)
      *p = '\0';
    
    return(ret_pointer);
}


/*
 * Return the size of the address book 
 */
adrbk_cntr_t
adrbk_count(ab)
    AdrBk *ab;
{
    return(ab ? ab->count : (adrbk_cntr_t)0);
}


/*
 * Get the ae that has index number "entry_num" in "handling" mode.
 *
 * Handling - Normal - Means it may be deleted from cache out from under us.
 *            Lock   - Means it may not be deleted from the cache (so that
 *                     we can continue to use pointers to it) until we
 *                     Unlock it explicitly or until adrbk_write is called.
 *            Unlock - Usually just used to unlock a locked entry.  Adrbk_write
 *                     also does this unlocking.  It also returns the ae.
 *   Returns NULL if entry_num is out of range.  Otherwise, it returns an ae.
 *   It never returns NULL when it should return an ae.  Instead, if it can't
 *   figure out what the entry is, it returns an empty, Single entry.  This
 *   means that the users of adrbk_get_ae don't have to check for NULL.
 *   Note, however, that it is possible that a caller will get back an empty
 *   Single while expecting a List.
 */
AdrBk_Entry *
adrbk_get_ae(ab, entry_num, handling)
    AdrBk    *ab;
    a_c_arg_t entry_num;
    Handling  handling;
{
    EntryRef *entry = (EntryRef *)NULL;
    AdrBk_Entry *ae = (AdrBk_Entry *)NULL;

    dprint(9, (debugfile, "- adrbk_get_ae -\n"));

    if(!ab)
      return(ae);

    /*
     * May be asking for one of the appended elements.
     */
    if(edited_abook == ab && appended_aes &&
       entry_num >= ab->phantom_count && entry_num < ab->count){
	AE_LIST_S *e;

	e = appended_aes;
	while(e){
	    if(e->ent == entry_num)
	      break;

	    e = e->next;
	}

	if(e){
	    if(!e->ae)
	      panic("No adrbk entry to go with appended entry");
	    
	    return(e->ae);
	}
    }

    if(entry_num >= (a_c_arg_t)ab->phantom_count)
      return(ae);

    entry = adrbk_get_entryref(ab, entry_num, handling);
    if(entry != NULL && entry->uid_nick != NO_UID)
      ae = init_ae_entry(ab, entry);

#ifdef DEBUG
    if(ae == (AdrBk_Entry *)NULL){
	dprint(2, (debugfile, "adrbk_get_ae (%s): returning NULL!\n",
	    ab->filename ? ab->filename : "?"));
	dprint(2, (debugfile, "   : count %ld l_c_w_k_a %ld cur_time %lu\n",
	    (long)ab->count, (long)ab->last_change_we_know_about,
	    (unsigned long)get_adj_time()));
	dprint(2, (debugfile, "   : requested entry_num %ld\n",
	    (long)entry_num));
	if(entry == NULL){
	    dprint(2,
	     (debugfile, "   : got back NULL entry from adrbk_get_entryref\n"));
	}
	else{
	    dprint(2, (debugfile, "   : uid_nick %ld uid_addr %ld offset %ld\n",
		(long)entry->uid_nick, (long)entry->uid_addr, entry->offset));
	}
    }
#endif /* DEBUG */

    /* This assigns a non-null value to ae. */
    if(ae == (AdrBk_Entry *)NULL){
	ae = adrbk_newentry();
	ae->tag       = Single;
	ae->nickname  = cpystr("");
	ae->addr.addr = cpystr("");
	if(entry)
	  entry->ae = ae;
	/* else, memory leak that shouldn't happen often */
    }

    return(ae);
}


/*
 * Look up an entry in the address book given a nickname
 *
 * Args: ab       -- the address book
 *       nickname -- nickname to match
 *      entry_num -- if matched, return entry_num of match here
 *
 * Result: A pointer to an AdrBk_Entry is returned, or NULL if not found.
 *
 * Lookups usually need to be recursive in case the address
 * book references itself.  This is left to the next level up.
 * adrbk_clearrefs() is provided to clear all the reference tags in
 * the address book for loop detection.
 * When there are duplicates of the same nickname we return the first.
 * This can only happen if addrbook was edited externally.
 */
AdrBk_Entry *
adrbk_lookup_by_nick(ab, nickname, entry_num)
    AdrBk *ab;
    char  *nickname;
    adrbk_cntr_t *entry_num;
{
    adrbk_cntr_t hash;
    adrbk_uid_t uid;
    adrbk_cntr_t ind, last_ind;
    EntryRef *entry, *last_one;
    AdrBk_Entry *ae;

    dprint(5, (debugfile, "- adrbk_lookup_by_nick(%s) (in %s) -\n",
	   nickname ? nickname : "?",
	   (ab && ab->filename) ? ab->filename : "?"));

    if(!ab || !nickname || !nickname[0])
      return NULL;

    hash = ab_hash(nickname, (a_c_arg_t)ab->htable_size);
    uid  = ab_uid(nickname);

    last_one = (EntryRef *)NULL;

    ind = ab->hash_by_nick->harray[hash];
    while(ind != NO_NEXT &&
	  (entry = adrbk_get_entryref(ab, (a_c_arg_t)ind, Internal))){

	if(entry->is_deleted){
	    ind = entry->next_nick;
	    free_ab_entryref(entry);
	}
	else{
	    if(entry->uid_nick == uid){
		ae = adrbk_get_ae(ab, (a_c_arg_t)ind, Internal);
		/*
		 * Guard against the unlikely case where two nicknames have the
		 * same hash and uid, but are actually different.
		 */
		if(ae && strucmp(ae->nickname, nickname) == 0){
		    last_one = entry;
		    last_ind = ind;
		}
	    }

	    ind = entry->next_nick;
	}
    }

    /* no such nickname */
    if(last_one == (EntryRef *)NULL)
      return((AdrBk_Entry *)NULL);

    if(entry_num)
      *entry_num = last_ind;

    return(adrbk_get_ae(ab, (a_c_arg_t)last_ind, Internal));
}


/*
 * Look up an entry in the address book given an address
 *
 * Args: ab       -- the address book
 *       address  -- address to match
 *      entry_num -- if matched, return entry_num of match here
 *
 * Result: A pointer to an AdrBk_Entry is returned, or NULL if not found.
 *
 * Note:  When there are multiple occurrences of an address in an addressbook,
 * which there will be if more than one nickname points to same address, then
 * we want this to match the first occurrence so that the fcc you get will
 * be predictable.  Because of the way the hash table is built (and needs to
 * be built) we need to look for the last occurrence of uid within the list
 * a hash table entry points to instead of the first occurrence.
 */
AdrBk_Entry *
adrbk_lookup_by_addr(ab, address, entry_num)
    AdrBk *ab;
    char  *address;
    adrbk_cntr_t *entry_num;
{
    adrbk_cntr_t hash;
    adrbk_uid_t uid;
    adrbk_cntr_t ind, last_ind;
    EntryRef *entry, *last_one;

    dprint(5, (debugfile, "- adrbk_lookup_by_addr(%s) (in %s) -\n",
	   address ? address : "?",
	   (ab && ab->filename) ? ab->filename : "?"));

    if(!ab || !address || !address[0])
      return NULL;

    hash = ab_hash_addr(address, (a_c_arg_t)ab->htable_size);
    uid  = ab_uid_addr(address);

    last_one = (EntryRef *)NULL;

    ind = ab->hash_by_addr->harray[hash];
    while(ind != NO_NEXT &&
	  (entry = adrbk_get_entryref(ab, (a_c_arg_t)ind, Internal))){

	if(entry->is_deleted){
	    ind = entry->next_addr;
	    free_ab_entryref(entry);
	}
	else{
	    if(entry->uid_addr == uid){
		last_one = entry;
		last_ind = ind;
	    }

	    ind = entry->next_addr;
	}
    }

    /* no such address */
    if(last_one == (EntryRef *)NULL)
      return((AdrBk_Entry *)NULL);

    if(entry_num)
      *entry_num = last_ind;

    return(adrbk_get_ae(ab, (a_c_arg_t)last_ind, Internal));
}


/*
 * Format a full name.
 *
 * Args: fullname -- full name out of address book for formatting
 *          first -- Return a pointer to decoded first name here.
 *           last -- Return a pointer to decoded last name here.
 *
 * Result:  Returns pointer to name formatted for a mail header. Space is
 *          allocated here and should be freed by caller.
 *
 * We need this because we store full names as Last, First.
 * If the name has no comma, then no change is made.
 * Otherwise the text before the first comma is moved to the end and
 * the comma is deleted.
 *
 * Last and first have to be freed by caller.
 */
char *
adrbk_formatname(fullname, first, last)
    char  *fullname;
    char **first, **last;
{
    char       *comma, *p, *charset = NULL;
    char       *new_name;
    int         need_to_encode = 0;

    if(first)
      *first = NULL;
    if(last)
      *last = NULL;

    /*
     * It's difficult to find comma when it is encoded, so decode it
     * and search for comma.  Note, we can't handle a fullname with
     * multiple charsets in it.
     */
    p = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
			       SIZEOF_20KBUF, fullname, &charset);
    if(p == fullname){
	if(charset)
	  fs_give((void **)&charset);

	charset = NULL;
    }
    else
      need_to_encode++;

    fullname = p;  /* overloading fullname, now decoded */

    if(fullname[0] != '"'  && (comma = strindex(fullname, ',')) != NULL){
        int last_name_len = comma - fullname;
        comma++;
        while(*comma && isspace((unsigned char)*comma))
	  comma++;

	if(first)
	  *first = cpystr(comma);

	if(last){
	    *last = (char *)fs_get((last_name_len + 1) * sizeof(char));
	    strncpy(*last, fullname, last_name_len); 
	    (*last)[last_name_len] = '\0';
	}

	new_name = (char *)fs_get((strlen(comma) + 1 + last_name_len + 1) *
								sizeof(char));
        strcpy(new_name, comma);
        strcat(new_name, " ");
        strncat(new_name, fullname, last_name_len); 
    }
    else
      new_name = cpystr(fullname);
    
    if(need_to_encode){
	char *s;

	/* re-encode in original charset */
	s = rfc1522_encode(tmp_20k_buf+10000,
			   SIZEOF_20KBUF-10000, (unsigned char *)new_name,
	    charset ? charset : ps_global->VAR_CHAR_SET);
	if(s != new_name){
	    if(strlen(s) > strlen(new_name)){
		fs_give((void **)&new_name);
		new_name = cpystr(s);
	    }
	    else
	      strcpy(new_name, s);
	}
	
	if(charset)
	  fs_give((void **)&charset);
    }

    return(new_name);
}


/*
 * Clear reference flags in preparation for a recursive lookup.
 *
 * For loop detection during address book look up.  This clears all the 
 * referenced flags, then as the lookup proceeds the referenced flags can 
 * be checked and set.
 */
void
adrbk_clearrefs(ab)
AdrBk *ab;
    {
    dprint(9, (debugfile, "- adrbk_clearrefs -\n"));

    if(!ab)
      return;

    /*
     * We only have to clear the references in cached ae's, since newly
     * created ae's start out with cleared references.  This should speed
     * things up considerably for large addrbooks.
     */
    clearrefs_in_cached_aes(ab);
}


/*
 *  Allocate a new AdrBk_Entry
 */
AdrBk_Entry *
adrbk_newentry()
{
    AdrBk_Entry *a;

    a = (AdrBk_Entry *)fs_get(sizeof(AdrBk_Entry));
    a->nickname    = empty;
    a->fullname    = empty;
    a->addr.addr   = empty;
    a->fcc         = empty;
    a->extra       = empty;
    a->tag         = NotSet;
    a->referenced  = 0;

    return(a);
}


AdrBk_Entry *
copy_ae(src)
    AdrBk_Entry *src;
{
    AdrBk_Entry *a;

    a = adrbk_newentry();
    a->tag = src->tag;
    a->nickname = cpystr(src->nickname ? src->nickname : "");
    a->fullname = cpystr(src->fullname ? src->fullname : "");
    a->fcc      = cpystr(src->fcc ? src->fcc : "");
    a->extra    = cpystr(src->extra ? src->extra : "");
    if(a->tag == Single)
      a->addr.addr = cpystr(src->addr.addr ? src->addr.addr : "");
    else if(a->tag == List){
	char **p;
	int    i, n;

	/* count list */
	for(p = src->addr.list; p && *p; p++)
	  ;/* do nothing */
	
	if(p == NULL)
	  n = 0;
	else
	  n = p - src->addr.list;

	a->addr.list = (char **)fs_get((n+1) * sizeof(char *));
	for(i = 0; i < n; i++)
	  a->addr.list[i] = cpystr(src->addr.list[i]);
	
	a->addr.list[n] = NULL;
    }

    return(a);
}



/*
 * Add an entry to the address book, or modify an existing entry
 *
 * Args: ab       -- address book to add to
 *  old_entry_num -- the entry we want to modify.  If this is NO_NEXT, then
 *                    we look up the nickname passed in to see if that's the
 *                    entry to modify, else it is a new entry.
 *       nickname -- the nickname for new entry
 *       fullname -- the fullname for new entry
 *       address  -- the address for new entry
 *       fcc      -- the fcc for new entry
 *       extra    -- the extra field for new entry
 *       tag      -- the type of new entry
 *  new_entry_num -- return entry_num of new or modified entry here
 * resort_happened -- means that more than just the current entry changed,
 *                     either something was added or order was changed
 *     enable_intr -- tell adrbk_write to enable interrupt handling
 *        be_quiet -- tell adrbk_write to not do percent done messages
 *        write_it -- only do adrbk_write if this is set
 *
 * Result: return code:  0 all went well
 *                      -2 error writing address book, check errno
 *		        -3 no modification, the tag given didn't match
 *                         existing tag
 *                      -4 tabs are in one of the fields passed in
 *
 * If the nickname exists in the address book already, the operation is
 * considered a modification even if the case does not match exactly,
 * otherwise it is an add.  The entry the operation occurs on is returned
 * in new.  All fields are set to those passed in; that is, passing in NULL
 * even on a modification will set those fields to NULL as opposed to leaving
 * them unchanged.  It is acceptable to pass in the current strings
 * in the entry in the case of modification.  For address lists, the
 * structure passed in is what is used, so the storage has to all have
 * come from fs_get().  If the pointer passed in is the same as
 * the current field, no change is made.
 */
int
adrbk_add(ab, old_entry_num, nickname, fullname, address, fcc, extra, tag,
	new_entry_num, resort_happened, enable_intr, be_quiet, write_it)
    AdrBk        *ab;
    a_c_arg_t     old_entry_num;
    char         *nickname,
		 *fullname,
		 *address, /* address can be char **, too */
		 *fcc,
		 *extra;
    Tag           tag;
    adrbk_cntr_t *new_entry_num;
    int          *resort_happened;
    int           enable_intr, be_quiet, write_it;
{
    AdrBk_Entry *a;
    AdrBk_Entry *ae;
    adrbk_cntr_t old_enum;
    adrbk_cntr_t new_enum;
    int (*cmp_func)();
    int retval = 0;
    int need_write = 0;

    dprint(3, (debugfile, "- adrbk_add(%s) -\n", nickname ? nickname : ""));

    if(!ab)
      return -2;

    /* ---- Make sure there are no tabs in the stuff to add ------*/
    if((nickname != NULL && strindex(nickname, TAB) != NULL) ||
       (fullname != NULL && strindex(fullname, TAB) != NULL) ||
       (fcc != NULL && strindex(fcc, TAB) != NULL) ||
       (tag == Single && address != NULL && strindex(address, TAB) != NULL))
        return -4;

    /*
     * Are we adding or updating ?
     *
     * If old_entry_num was passed in, we're updating that.  If nickname
     * already exists, we're updating that entry.  Otherwise, this is an add.
     */
    if((adrbk_cntr_t)old_entry_num != NO_NEXT){
	ae = adrbk_get_ae(ab, old_entry_num, Normal);
	if(ae)
	  old_enum = (adrbk_cntr_t)old_entry_num;
    }
    else
      ae = adrbk_lookup_by_nick(ab, nickname, &old_enum);

    if(ae == NULL){  /*----- adding a new entry ----*/

        ae            = adrbk_newentry();
        ae->nickname  = nickname ? cpystr(nickname) : nickname;
        ae->fullname  = fullname ? cpystr(fullname) : fullname;
        ae->fcc       = fcc      ? cpystr(fcc)      : fcc;
        ae->extra     = extra    ? cpystr(extra)    : extra;
	ae->tag       = tag;

	if(tag == Single)
          ae->addr.addr = cpystr(address);
	else
	  ae->addr.list = (char **)NULL;

	cmp_func = (ab->sort_rule == AB_SORT_RULE_FULL_LISTS) ?
						cmp_ae_by_full_lists_last :
		   (ab->sort_rule == AB_SORT_RULE_FULL) ?
						cmp_ae_by_full :
		   (ab->sort_rule == AB_SORT_RULE_NICK_LISTS) ?
						cmp_ae_by_nick_lists_last :
		/* (ab->sort_rule == AB_SORT_RULE_NICK) */
						cmp_ae_by_nick;

	if(ab->sort_rule == AB_SORT_RULE_NONE)  /* put it last */
	  new_enum = ab->count;
	else  /* Find slot for it */
	  for(new_enum = 0, a = adrbk_get_ae(ab, (a_c_arg_t)new_enum, Normal);
	      a != (AdrBk_Entry *)NULL;
	      a = adrbk_get_ae(ab, (a_c_arg_t)(++new_enum), Normal)){
		    if((*cmp_func)((QSType *)&a, (QSType *)&ae) >= 0)
			break;
	  }
	/* Insert ae before entry new_enum. */
	set_inserted_entryref(ab, (a_c_arg_t)new_enum, ae);

        /*---- return in pointer if requested -----*/
        if(new_entry_num)
	  *new_entry_num = new_enum;

        if(resort_happened)
	  *resort_happened = 1;

	if(write_it)
	  retval = adrbk_write(ab, NULL, enable_intr, be_quiet, 1);

	if(retval == 0){
	    if(F_OFF(F_EXPANDED_DISTLISTS,ps_global))
	      exp_add_nth(ab->exp, (a_c_arg_t)new_enum);

	    exp_add_nth(ab->selects, (a_c_arg_t)new_enum);
	}
    }
    else{
        /*----- Updating an existing entry ----*/

	if(ae->tag != tag)
	  return -3;

	/* Lock this entry in the entryref cache */
	(void)adrbk_get_entryref(ab, (a_c_arg_t)old_enum, Lock);

        /*
	 * Instead of just freeing and reallocating here we attempt to re-use
	 * the space that was already allocated if possible.
	 */
	if(ae->nickname != nickname
	   && ae->nickname != NULL
	   && nickname != NULL
	   && strcmp(nickname, ae->nickname) != 0){
	    need_write++;
	    /* can use already alloc'd space */
            if(ae->nickname != NULL && nickname != NULL &&
	       strlen(nickname) <= strlen(ae->nickname)){

                strcpy(ae->nickname, nickname);
            }
	    else{
                if(ae->nickname != NULL && ae->nickname != empty)
                  fs_give((void **)&ae->nickname);

                ae->nickname = nickname ? cpystr(nickname) : nickname;
	    }
        }

	if(ae->fullname != fullname
	   && ae->fullname != NULL
	   && fullname != NULL
	   && strcmp(fullname, ae->fullname) != 0){
	    need_write++;
            if(ae->fullname != NULL && fullname != NULL &&
	       strlen(fullname) <= strlen(ae->fullname)){

                strcpy(ae->fullname, fullname);
            }
	    else{
                if(ae->fullname != NULL && ae->fullname != empty)
                  fs_give((void **)&ae->fullname);

                ae->fullname = fullname ? cpystr(fullname) : fullname;
	    }
        }

	if(ae->fcc != fcc
	   && ae->fcc != NULL
	   && fcc != NULL
	   && strcmp(fcc, ae->fcc) != 0){
	    need_write++;
            if(ae->fcc != NULL && fcc != NULL &&
	       strlen(fcc) <= strlen(ae->fcc)){

                strcpy(ae->fcc, fcc);
            }
	    else{
                if(ae->fcc != NULL && ae->fcc != empty)
                  fs_give((void **)&ae->fcc);

                ae->fcc = fcc ? cpystr(fcc) : fcc;
	    }
        }

	if(ae->extra != extra
	   && ae->extra != NULL
	   && extra != NULL
	   && strcmp(extra, ae->extra) != 0){
	    need_write++;
            if(ae->extra != NULL && extra != NULL &&
	       strlen(extra) <= strlen(ae->extra)){

                strcpy(ae->extra, extra);
            }
	    else{
                if(ae->extra != NULL && ae->extra != empty)
                  fs_give((void **)&ae->extra);

                ae->extra = extra ? cpystr(extra) : extra;
	    }
        }

	if(tag == Single){
            /*---- Single ----*/
	    if(ae->addr.addr != address
	       && ae->addr.addr != NULL
	       && address != NULL
	       && strcmp(address, ae->addr.addr) != 0){
		need_write++;
		if(ae->addr.addr != NULL && address != NULL &&
		   strlen(address) <= strlen(ae->addr.addr)){

		    strcpy(ae->addr.addr, address);
		}
		else{
		    if(ae->addr.addr != NULL && ae->addr.addr != empty)
		      fs_give((void **)&ae->addr.addr);

		    ae->addr.addr = address ? cpystr(address) : address;
		}
	    }
	}
	else{
            /*---- List -----*/
            /*
	     * We don't mess with lists here.
	     * The caller has to do it with adrbk_listadd().
	     */
	    ;/* do nothing */
	}


        /*---------- Make sure it's still in order ---------*/
	/*
	 * old_enum is where ae is currently located
	 * put it where it belongs
	 */
	if(need_write)
	  new_enum = re_sort_particular_entryref(ab, (a_c_arg_t)old_enum);
	else
	  new_enum = old_enum;

        /*---- return in pointer if requested -----*/
        if(new_entry_num)
	  *new_entry_num = new_enum;

        if(resort_happened)
	  *resort_happened = (old_enum != new_enum);

	if(write_it && need_write)
	  retval = adrbk_write(ab, NULL, enable_intr, be_quiet, 1);
	else{
	    (void)adrbk_get_entryref(ab, (a_c_arg_t)old_enum, Unlock);
	    retval = 0;
	}

	/*
	 * If it got re-sorted we just throw in the towel and unexpand
	 * all the lists and dump the selected state.
	 * Maybe someday we'll fix it to try to track
	 * the numbers of the expanded lists.
	 */
	if(old_enum != new_enum && retval == 0){
	    exp_free(ab->exp);
	    as.selected_is_history = 1;
	}
    }

    return(retval);
}


/*
 * Similar to adrbk_add, but lower cost. No sorting is done, the new entry
 * goes on the end. This won't work if it is an edit instead of an append.
 * The address book is not committed to disk.
 *
 * Args: ab       -- address book to add to
 *       nickname -- the nickname for new entry
 *       fullname -- the fullname for new entry
 *       address  -- the address for new entry
 *       fcc      -- the fcc for new entry
 *       extra    -- the extra field for new entry
 *       tag      -- the type of new entry
 *
 * Result: return code:  0 all went well
 *                      -2 error writing address book, check errno
 *		        -3 no modification, the tag given didn't match
 *                         existing tag
 *                      -4 tabs are in one of the fields passed in
 */
int
adrbk_append(ab, nickname, fullname, address, fcc, extra, tag, new_entry_num)
    AdrBk        *ab;
    char         *nickname,
		 *fullname,
		 *address, /* address can be char **, too */
		 *fcc,
		 *extra;
    Tag           tag;
    adrbk_cntr_t *new_entry_num;
{
    AdrBk_Entry *ae;

    dprint(3, (debugfile, "- adrbk_append(%s) -\n", nickname ? nickname : ""));

    if(!ab)
      return -2;

    /* ---- Make sure there are no tabs in the stuff to add ------*/
    if((nickname != NULL && strindex(nickname, TAB) != NULL) ||
       (fullname != NULL && strindex(fullname, TAB) != NULL) ||
       (fcc != NULL && strindex(fcc, TAB) != NULL) ||
       (tag == Single && address != NULL && strindex(address, TAB) != NULL))
        return -4;

    ae            = adrbk_newentry();
    ae->nickname  = nickname ? cpystr(nickname) : nickname;
    ae->fullname  = fullname ? cpystr(fullname) : fullname;
    ae->fcc       = fcc      ? cpystr(fcc)      : fcc;
    ae->extra     = extra    ? cpystr(extra)    : extra;
    ae->tag       = tag;

    if(tag == Single)
      ae->addr.addr = cpystr(address);
    else
      ae->addr.list = (char **)NULL;

    if(ae){
	AE_LIST_S *end;
	AE_LIST_S *new;

	/* add it to the end of the appended_ae list */
	for(end = appended_aes; end && end->next; end = end->next)
	  ;

	new = (AE_LIST_S *)fs_get(sizeof(AE_LIST_S));
	new->ae = ae;
	new->ent = ab->count;
	new->next = NULL;
	if(end)
	  end->next = new;
	else
	  appended_aes = new;

	ab->count++;
	if(new_entry_num)
	  *new_entry_num = new->ent;
	
	edited_abook = ab;
    }

    return(0);
}


/*
 * The entire address book is assumed sorted correctly except perhaps for
 * entry number cur.  Put it in the correct place.  Return the new entry
 * number for cur.
 */
adrbk_cntr_t
re_sort_particular_entryref(ab, cur)
    AdrBk *ab;
    a_c_arg_t cur;
{
    AdrBk_Entry  *ae_cur, *ae_prev, *ae_next, *ae_small_enough, *ae_big_enough;
    long small_enough;
    adrbk_cntr_t big_enough;
    adrbk_cntr_t new_entry_num;
    int (*cmp_func)();

    cmp_func = (ab->sort_rule == AB_SORT_RULE_FULL_LISTS) ?
					    cmp_ae_by_full_lists_last :
	       (ab->sort_rule == AB_SORT_RULE_FULL) ?
					    cmp_ae_by_full :
	       (ab->sort_rule == AB_SORT_RULE_NICK_LISTS) ?
					    cmp_ae_by_nick_lists_last :
	    /* (ab->sort_rule == AB_SORT_RULE_NICK) */
					    cmp_ae_by_nick;

    new_entry_num = (adrbk_cntr_t)cur;

    if(ab->sort_rule == AB_SORT_RULE_NONE)
      return(new_entry_num);

    ae_cur = adrbk_get_ae(ab, cur, Lock);

    if(cur > 0)
      ae_prev  = adrbk_get_ae(ab, cur - 1, Lock);

    if(cur < ab->count -1)
      ae_next  = adrbk_get_ae(ab, cur + 1, Lock);

    /*
     * A possible optimization here would be to implement some sort of
     * binary search to find where it goes instead of stepping through the
     * entries one at a time.  That way, it might be faster, and, we wouldn't
     * have to page in as much of the entryref array.  Another optimization
     * is a way to access cached stuff only, so we could look through all
     * the cached stuff first before looking at stuff on disk.
     */
    if(cur > 0 &&
       (*cmp_func)((QSType *)&ae_cur,(QSType *)&ae_prev) < 0){
	/*--- Out of order, needs to be moved up ----*/
	for(small_enough = (long)cur - 2; small_enough >= 0L; small_enough--){
	  ae_small_enough = adrbk_get_ae(ab,(a_c_arg_t)small_enough,Normal);
	  if((*cmp_func)((QSType *)&ae_cur, (QSType *)&ae_small_enough) >= 0)
	    break;
	}
	new_entry_num = (adrbk_cntr_t)(small_enough + 1L);
	set_moved_entryref(cur, (a_c_arg_t)new_entry_num);
    }
    else if(cur < ab->count - 1 &&
	(*cmp_func)((QSType *)&ae_cur, (QSType *)&ae_next) > 0){
	/*---- Out of order needs, to be moved towards end of list ----*/
	for(big_enough = (adrbk_cntr_t)(cur + 2);
	    big_enough < ab->count;
	    big_enough++){
	  ae_big_enough = adrbk_get_ae(ab, (a_c_arg_t)big_enough, Normal);
	  if((*cmp_func)((QSType *)&ae_cur, (QSType *)&ae_big_enough) <= 0)
	    break;
	}
	new_entry_num = big_enough - 1;
	set_moved_entryref(cur, (a_c_arg_t)big_enough);
    }

    if(cur > 0)
      (void)adrbk_get_ae(ab, cur - 1, Unlock);

    if(cur < ab->count -1)
      (void)adrbk_get_ae(ab, cur + 1, Unlock);

    return(new_entry_num);
}


/*
 * Delete an entry from the address book
 *
 * Args: ab        -- the address book
 *       entry_num -- entry to delete
 *    save_deleted -- save deleted as a #DELETED- entry
 *     enable_intr -- tell adrbk_write to enable interrupt handling
 *        be_quiet -- tell adrbk_write to not do percent done messages
 *        write_it -- only do adrbk_write if this is set
 *
 * Result: returns:  0 if all went well
 *                  -1 if there is no such entry
 *                  -2 error writing address book, check errno
 */
int
adrbk_delete(ab, entry_num, save_deleted, enable_intr, be_quiet, write_it)
    AdrBk    *ab;
    a_c_arg_t entry_num;
    int       save_deleted;
    int       enable_intr, be_quiet, write_it;
{
    int retval = 0;

    dprint(3, (debugfile, "- adrbk_delete(%ld) -\n", (long)entry_num));

    if(!ab)
      return -2;

    (void)adrbk_get_entryref(ab, entry_num, save_deleted ? SaveDelete : Delete);

    if(write_it)
      retval = adrbk_write(ab, NULL, enable_intr, be_quiet, 1);

    if(retval == 0){
	if(F_OFF(F_EXPANDED_DISTLISTS,ps_global))
	  exp_del_nth(ab->exp, entry_num);

      exp_del_nth(ab->selects, entry_num);
    }
    
    return(retval);
}


/*
 * Delete an address out of an address list
 *
 * Args: ab    -- the address book
 *       entry -- the address list we are deleting from
 *       addr  -- address in above list to be deleted
 *
 * Result: 0: Deletion complete, address book written
 *        -1: Address for deletion not found
 *        -2: Error writing address book. Check errno.
 *
 * The address to be deleted is located by matching the string.
 *
 * This doesn't invalidate any of the AEMgr cache or anything since this
 * entry is still there when we're done and we've updated the cache
 * entry by deleting it below.
 */
int
adrbk_listdel(ab, entry_num, addr)
    AdrBk       *ab;
    a_c_arg_t   entry_num;
    char        *addr;
{
    char **p, *to_free;
    AdrBk_Entry *entry;

    dprint(3, (debugfile, "- adrbk_listdel(%ld) -\n", (long)entry_num));

    if(!ab || entry_num >= ab->count)
      return -2;

    if(!addr)
      return -1;

    entry = adrbk_get_ae(ab, entry_num, Lock);

    if(entry->tag != List){
	(void)adrbk_get_entryref(ab, entry_num, Unlock);
        return -1;
    }

    for(p = entry->addr.list; *p; p++) 
      if(strcmp(*p, addr) == 0)
        break;

    if(*p == NULL)
      return -1;

    /* note storage to be freed */
    if(*p != empty)
      to_free = *p;
    else
      to_free = NULL;

    /* slide all the entries below up (including NULL) */
    for(; *p; p++)
      *p = *(p+1);

    if(to_free)
      fs_give((void **)&to_free);

    return(adrbk_write(ab, NULL, 1, 0, 1));
}


/*
 * Delete all addresses out of an address list
 *
 * Args: ab    -- the address book
 *       entry -- the address list we are deleting from
 *    write_it -- call adrbk_write when done changing
 *
 * Result: 0: Deletion complete, address book written
 *        -1: Address for deletion not found
 *        -2: Error writing address book. Check errno.
 *
 * This doesn't invalidate any of the AEMgr cache or anything since this
 * entry is still there when we're done and we've updated the cache
 * entry by deleting it below.
 */
int
adrbk_listdel_all(ab, entry_num, write_it)
    AdrBk      *ab;
    a_c_arg_t   entry_num;
    int         write_it;
{
    char **p;
    AdrBk_Entry *entry;

    dprint(3, (debugfile, "- adrbk_listdel_all(%ld) -\n", (long)entry_num));

    if(!ab || entry_num >= ab->count)
      return -2;

    entry = adrbk_get_ae(ab, entry_num, Lock);

    if(entry->tag != List){
	(void)adrbk_get_entryref(ab, entry_num, Unlock);
        return -1;
    }

    /* free old list */
    for(p = entry->addr.list; p && *p; p++) 
      if(*p != empty)
        fs_give((void **)p);
    
    if(entry->addr.list)
      fs_give((void **)&(entry->addr.list));

    entry->addr.list = NULL;

    return(write_it ? adrbk_write(ab, NULL, 1, 0, 1) : 0);
}


/*
 * Add a list of addresses to an already existing address list
 *
 * Args: ab        -- the address book
 *       entry     -- the address list we are adding to
 *       addrs     -- address list to be added
 *     enable_intr -- tell adrbk_write to enable interrupt handling
 *        be_quiet -- tell adrbk_write to not do percent done messages
 *        write_it -- only do adrbk_write if this is set
 *
 * Result: returns 0 : addition made, address book written
 *                -1 : addition to non-list attempted
 *                -2 : error writing address book -- check errno
 */
int
adrbk_nlistadd(ab, entry_num, addrs, enable_intr, be_quiet, write_it)
    AdrBk       *ab;
    a_c_arg_t    entry_num;
    char       **addrs;
    int          enable_intr, be_quiet, write_it;
{
    char **p;
    int    cur_size, size_of_additional_list, new_size;
    int    i, rc = 0;
    AdrBk_Entry *entry;

    dprint(3, (debugfile, "- adrbk_nlistadd(%ld) -\n", (long)entry_num));

    if(!ab || entry_num >= ab->count)
      return -2;

    entry = adrbk_get_ae(ab, entry_num, Lock);

    if(entry->tag != List){
	(void)adrbk_get_entryref(ab, entry_num, Unlock);
        return -1;
    }

    /* count up size of existing list */    
    for(p = entry->addr.list; p != NULL && *p != NULL; p++)
      ;/* do nothing */

    cur_size = p - entry->addr.list;

    /* count up size of new list */    
    for(p = addrs; p != NULL && *p != NULL; p++)
      ;/* do nothing */

    size_of_additional_list = p - addrs;
    new_size = cur_size + size_of_additional_list;

    /* make room at end of list for it */
    if(cur_size == 0)
      entry->addr.list = (char **)fs_get(sizeof(char *) * (new_size + 1));
    else
      fs_resize((void **)&entry->addr.list, sizeof(char *) * (new_size + 1));

    /* Put new list at the end */
    for(i = cur_size; i < new_size; i++)
      (entry->addr.list)[i] = cpystr(addrs[i - cur_size]);

    (entry->addr.list)[new_size] = NULL;

    /*---- sort it into the correct place ------*/
    if(ab->sort_rule != AB_SORT_RULE_NONE)
      sort_addr_list(entry->addr.list);

    if(write_it)
      rc = adrbk_write(ab, NULL, enable_intr, be_quiet, 1);

    return(rc);
}


/*
 * Set the valid variable if we determine that the address book has
 * been changed by something other than us. This means we should update
 * our view of the address book when next possible.
 *
 * Args    ab -- AdrBk handle
 *  do_it_now -- If > 0, check now regardless
 *               If = 0, check if time since last chk more than default
 *               If < 0, check if time since last chk more than -do_it_now
 */
void
adrbk_check_validity(ab, do_it_now)
    AdrBk *ab;
    long   do_it_now;
{
    dprint(9, (debugfile, "- adrbk_check_validity(%s) -\n",
	   (ab && ab->filename) ? ab->filename : ""));

    if(!ab || ab->flags & FILE_OUTOFDATE)
      return;

    adrbk_check_local_validity(ab, do_it_now);

    if(ab->type == Imap && !(ab->flags & FILE_OUTOFDATE ||
			     ab->rd->flags & REM_OUTOFDATE))
      rd_check_remvalid(ab->rd, do_it_now);
}


/*
 * Set the valid variable if we determine that the address book has
 * been changed by something other than us. This means we should update
 * our view of the address book when next possible.
 *
 * Args    ab -- AdrBk handle
 *  do_it_now -- If > 0, check now regardless
 *               If = 0, check if time since last chk more than default
 *               If < 0, check if time since last chk more than -do_it_now
 */
void
adrbk_check_local_validity(ab, do_it_now)
    AdrBk *ab;
    long   do_it_now;
{
    time_t mtime, chk_interval;

    dprint(9, (debugfile, "- adrbk_check_local_validity(%s) -\n",
	   (ab && ab->filename) ? ab->filename : ""));

    if(!ab)
      return;

    if(do_it_now < 0L){
	chk_interval = -1L * do_it_now;
	do_it_now = 0L;
    }
    else
      chk_interval = FILE_VALID_CHK_INTERVAL;

    if(!do_it_now &&
       get_adj_time() <= ab->last_local_valid_chk + chk_interval)
      return;
    
    ab->last_local_valid_chk = get_adj_time();

    /*
     * Check local file for a modification time change.
     * If this is out of date, don't even bother checking for the remote
     * folder being out of date. That will get fixed when we reopen.
     */
    if(!(ab->flags & FILE_OUTOFDATE) &&
       ab->last_change_we_know_about != (time_t)(-1) &&
       (mtime=get_adj_name_file_mtime(ab->filename)) != (time_t)(-1) &&
       ab->last_change_we_know_about != mtime){

	dprint(2,
	    (debugfile, "adrbk_check_local_validity: addrbook %s has changed\n",
			 ab->filename ? ab->filename : "?"));
	ab->flags |= FILE_OUTOFDATE;
    }
}


/*
 * Given an AdrBk, return it's pab.
 * If we have to use this much it should just be part of the AdrBk struct.
 */
PerAddrBook *
pab_from_ab(ab)
    AdrBk *ab;
{
    int          i;
    PerAddrBook *retpab = NULL, *pab;

    for(i = 0; !retpab && i < as.n_addrbk; i++){

	pab = &as.adrbks[i];
	if(pab->address_book && pab->address_book == ab)
	  retpab = pab;
    }

    return(retpab);
}


/*
 * See if we can re-use an existing stream.
 * 
 * [ We don't believe we need this anymore now that the stuff in pine.c ]
 * [ is recycling streams for us, but we haven't thought it through all ]
 * [ the way enough to get rid of this. Hubert 2003-07-09               ]
 *
 * Args  name     -- Name of folder we want a stream for.
 *
 * Returns -- A mail stream suitable for status cmd or append cmd.
 *            NULL if none were available.
 */
MAILSTREAM *
adrbk_handy_stream(name)
    char  *name;
{
    MAILSTREAM *stat_stream = NULL;
    int         i;

    dprint(9, (debugfile, "- adrbk_handy_stream(%s) -\n", name ? name : "?"));

    stat_stream = sp_stream_get(name, SP_SAME);
    
    /*
     * Look through our imap streams to see if there is one we can use.
     */
    for(i = 0; !stat_stream && i < as.n_addrbk; i++){
	PerAddrBook *pab;

	pab = &as.adrbks[i];

	if(pab->address_book &&
	   pab->address_book->type == Imap &&
	   pab->address_book->rd &&
	   pab->address_book->rd->type == RemImap &&
	   same_stream(name, pab->address_book->rd->t.i.stream)){
	    stat_stream = pab->address_book->rd->t.i.stream;
	    pab->address_book->rd->last_use = get_adj_time();
	    dprint(7, (debugfile,
		   "%s: used other abook stream for status (%ld)\n",
		   pab->address_book->orig_filename
		     ? pab->address_book->orig_filename : "?",
		   (long)pab->address_book->rd->last_use));
	}
    }

    dprint(9, (debugfile, "adrbk_handy_stream: returning %s\n",
	   stat_stream ? "good stream" : "NULL"));

    return(stat_stream);
}


/*
 * Close address book
 *
 * All that is done here is to free the storage, since the address book is 
 * rewritten on every change.
 */
void
adrbk_close(ab)
    AdrBk *ab;
{
    dprint(4, (debugfile, "- adrbk_close(%s) -\n",
	   (ab && ab->filename) ? ab->filename : ""));

    if(!ab)
      return;

    if(ab->rd)
      rd_close_remdata(&ab->rd);

    clear_entryref_cache(ab);
    if(ab->fp)
      (void)fclose(ab->fp);

    if(ab->fp_hash)
      (void)fclose(ab->fp_hash);
    
    if(ab->head_cache_elem)
      fs_give((void **)&ab->head_cache_elem);

    if(ab->tail_cache_elem)
      fs_give((void **)&ab->tail_cache_elem);

    if(ab->n_ae_cached_in_this_bucket)
      fs_give((void **)&ab->n_ae_cached_in_this_bucket);

    if(ab->hash_by_nick)
      free_ab_adrhash(&ab->hash_by_nick);

    if(ab->hash_by_addr)
      free_ab_adrhash(&ab->hash_by_addr);

    if(ab->our_filecopy && ab->filename != ab->our_filecopy){
	(void)unlink(ab->our_filecopy);
	fs_give((void**)&ab->our_filecopy);
    }

    if(ab->filename){
	if(ab->flags & DEL_FILE)
	  (void)unlink(ab->filename);

	fs_give((void**)&ab->filename);
    }

    if(ab->orig_filename)
      fs_give((void**)&ab->orig_filename);

    if(ab->our_hashcopy && ab->hashfile != ab->our_hashcopy){
	(void)unlink(ab->our_hashcopy);
	fs_give((void**)&ab->our_hashcopy);
    }

    if(ab->hashfile){
	if(ab->flags & DEL_HASHFILE)
	  (void)unlink(ab->hashfile);

	fs_give((void**)&ab->hashfile);
    }

    if(ab->exp){
	exp_free(ab->exp);
	fs_give((void **)&ab->exp);  /* free head of list, too */
    }

    if(ab->checks){
	exp_free(ab->checks);
	fs_give((void **)&ab->checks);  /* free head of list, too */
    }

    if(ab->selects){
	exp_free(ab->selects);
	fs_give((void **)&ab->selects);  /* free head of list, too */
    }

    fs_give((void **)&ab);
}


void
adrbk_partial_close(ab)
    AdrBk *ab;
{
    dprint(4, (debugfile, "- adrbk_partial_close(%s) -\n",
	   (ab && ab->filename) ? ab->filename : ""));

    init_entryref_cache(ab);
    exp_free(ab->exp);		/* leaves head of list */
    exp_free(ab->checks);	/* leaves head of list */
}


/*
 * It has been noticed that this stream is dead and it is about to
 * be removed from the stream pool. We may have a pointer to it for one
 * of the remote address books. We have to note that the pointer is stale.
 */
void
note_closed_adrbk_stream(stream)
    MAILSTREAM *stream;
{
    PerAddrBook *pab;
    int i;

    if(!stream)
      return;

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->address_book
	   && pab->address_book->type == Imap
	   && pab->address_book->rd
	   && pab->address_book->rd->type == RemImap
	   && (stream == pab->address_book->rd->t.i.stream)){
	    dprint(4, (debugfile, "- note_closed_adrbk_stream(%s) -\n",
		   (pab->address_book && pab->address_book->orig_filename)
		       ? pab->address_book->orig_filename : ""));
	    pab->address_book->rd->t.i.stream = NULL;
	}
    }
}


void
free_ab_entryref(entry)
    EntryRef *entry;
{
    if(entry->ae)
      free_ae(&entry->ae);

    fs_give((void **)&entry);
}


static adrbk_cntr_t tot_for_percent;
static adrbk_cntr_t entry_num_for_percent;
#define AB_RENAMED_ABOOK  0x1
#define AB_RENAMED_HASH   0x2

/*
 * Write out the address book.
 *
 * If the addressbook is remote, only do the remote write part of the
 * operation when write_to_remote is set.
 *
 * If be_quiet is set, don't turn on busy_alarm.
 *
 * If enable_intr_handling is set, turn on and off interrupt handling.
 *
 * Format is as in comment in the adrbk_open routine.  Lines are wrapped
 * to be under 80 characters.  This is called on every change to the
 * address book.  Write is first to a temporary file,
 * which is then renamed to be the real address book so that we won't
 * destroy the real address book in case of something like a full file
 * system.
 *
 * Writing a temp file and then renaming has the bad side affect of
 * destroying links.  It also overrides any read only permissions on
 * the mail file since rename ignores such permissions.  However, we
 * handle readonly-ness in addrbook.c before we call this.
 * We retain the permissions by doing a stat on the old file and a
 * chmod on the new one (with file_attrib_copy).
 *
 * Returns:   0 write was successful
 *           -2 write failed
 *           -5 interrupted
 */
int
adrbk_write(ab, sort_array, enable_intr_handling, be_quiet, write_to_remote)
    AdrBk *ab;
    adrbk_cntr_t *sort_array;
    int enable_intr_handling, be_quiet, write_to_remote;
{
    FILE                  *ab_stream = NULL,
                          *fp_for_hash = NULL;
    EntryRef              *entry;
    AdrBk_Entry           *ae = NULL;
    adrbk_cntr_t           entry_num;
    long                   saved_deleted_cnt;
    adrbk_cntr_t           hash,
                           new_htable_size;
#ifndef	DOS
    void                  (*save_sighup)();
#endif
    int                   max_nick = 0,
			  max_full = 0, full_two = 0, full_three = 0,
			  max_fcc = 0, fcc_two = 0, fcc_three = 0,
			  max_addr = 0, addr_two = 0, addr_three = 0,
			  this_nick_width, this_full_width, this_addr_width,
			  this_fcc_width, fd, i;
    int                   progress = 0, interrupt_happened = 0, we_cancel = 0;
    char                 *temp_filename = NULL, *temp_hashfile = NULL;
    WIDTH_INFO_S         *widths;

    if(!ab)
      return -2;

    dprint(2, (debugfile, "- adrbk_write(\"%s\") - writing %lu entries\n",
	ab->filename ? ab->filename : "", (unsigned long)ab->count));
    
    errno = 0;

    adrbk_check_local_validity(ab, 1L);
    /* verify that file has not been changed by something else */
    if(ab->flags & FILE_OUTOFDATE){
	/* It has changed! */
	q_status_message(SM_ORDER | SM_DING, 5, 15,
	    "Addrbook changed by another process, aborting our change to avoid damage...");
	dprint(1,
	    (debugfile,
"adrbk_write: addrbook %s changed while we had it open, aborting write\n",
	    ab->filename ? ab->filename : "?"));
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }

    /*
     * Verify that remote folder has not been
     * changed by something else (not if checked in last 5 seconds).
     *
     * The -5 is so we won't check every time on a series of quick writes.
     */
    rd_check_remvalid(ab->rd, -5L);
    if(ab->type == Imap){
	int  ro;

	/*
	 * We'll eventually need this open to write to remote, so see if
	 * we can open it now.
	 */
	rd_open_remote(ab->rd);

	/*
	 * Did someone else change the remote copy?
	 */
	if((ro=rd_remote_is_readonly(ab->rd)) || ab->rd->flags & REM_OUTOFDATE){
	    if(ro == 1){
		q_status_message(SM_ORDER | SM_DING, 5, 15,
			    "Can't access remote addrbook, aborting change...");
		dprint(1, (debugfile,
			"adrbk_write: Can't write to remote addrbook %s, aborting write: open failed\n",
			ab->rd->rn ? ab->rd->rn : "?"));
	    }
	    else if(ro == 2){
		if(!(ab->rd->flags & NO_META_UPDATE)){
		    unsigned long save_chk_nmsgs;

		    /*
		     * Should have some non-type-specific method of doing this.
		     */
		    switch(ab->rd->type){
		      case RemImap:
			save_chk_nmsgs = ab->rd->t.i.chk_nmsgs;
			ab->rd->t.i.chk_nmsgs = 0;/* cause it to be OUTOFDATE */
			rd_write_metadata(ab->rd, 0);
			ab->rd->t.i.chk_nmsgs = save_chk_nmsgs;
			break;

		      default:
			q_status_message(SM_ORDER | SM_DING, 3, 5,
					 "Adrbk_write: Type not supported");
			break;
		    }
		}

		q_status_message(SM_ORDER | SM_DING, 5, 15,
	     "No write permission for remote addrbook, aborting change...");
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 5, 15,
	     "Remote addrbook changed, aborting our change to avoid damage...");
		dprint(1, (debugfile,
			"adrbk_write: remote addrbook %s changed while we had it open, aborting write\n",
			ab->orig_filename ? ab->orig_filename : "?"));
	    }

	    rd_close_remote(ab->rd);

	    longjmp(addrbook_changed_unexpectedly, 1);
	    /*NOTREACHED*/
	}
    }

#ifndef	DOS
    save_sighup = (void (*)())signal(SIGHUP, SIG_IGN);
#endif

    /*
     * If we want to be able to modify the address book, we will
     * need a temp_filename in the same directory as our_filecopy.
     */
    if(!(ab->our_filecopy && ab->our_filecopy[0])
       || !(temp_filename = tempfile_in_same_dir(ab->our_filecopy,"a1",NULL))
       || (fd = open(temp_filename, OPEN_WRITE_MODE, 0600)) < 0){
	dprint(1,
	    (debugfile,
	    "adrbk_write(%s): failed opening temp file (%s)\n",
	    ab->filename ? ab->filename : "?",
	    temp_filename ? temp_filename : "NULL"));
        goto io_error;
    }

    ab_stream = fdopen(fd, FOPEN_WRITE_MODE);
    if(ab_stream == NULL){
	dprint(1,
	    (debugfile,
	    "adrbk_write(%s): fdopen failed\n",
	    temp_filename ? temp_filename : "?"));
        goto io_error;
    }

    /* Rebuild on-disk hash tables from incore hash tables */
    if(!(ab->our_hashcopy && ab->our_hashcopy[0])
       || !(temp_hashfile = tempfile_in_same_dir(ab->our_hashcopy,"a2",NULL))
       || (fd = open(temp_hashfile, OPEN_WRITE_MODE, 0600)) < 0){
	dprint(1,
	    (debugfile,
	    "adrbk_write(%s): failed opening temp file (%s)\n",
	    ab->filename ? ab->filename : "?",
	    temp_hashfile ? temp_hashfile : "NULL"));
        goto io_error;
    }

    fp_for_hash = fdopen(fd, FOPEN_WRITE_MODE);

    /* clear hash tables */
    new_htable_size = hashtable_size((a_c_arg_t)ab->count);
    if(new_htable_size == ab->htable_size){
	init_adrhash_array(ab->hash_by_nick, (a_c_arg_t)ab->htable_size);
	init_adrhash_array(ab->hash_by_addr, (a_c_arg_t)ab->htable_size);
    }
    else{
	free_ab_adrhash(&ab->hash_by_nick);
	free_ab_adrhash(&ab->hash_by_addr);
	ab->htable_size  = new_htable_size;
	ab->hash_by_nick = new_adrhash((a_c_arg_t)ab->htable_size);
	ab->hash_by_addr = new_adrhash((a_c_arg_t)ab->htable_size);
    }

    saved_deleted_cnt = ab->deleted_cnt;

    /* accept keyboard interrupts */
    if(enable_intr_handling)
      intr_handling_on();

    if(!be_quiet){
	tot_for_percent = max(ab->count, 1);
	entry_num_for_percent = 0;
	we_cancel = busy_alarm(1, "Saving address book",percent_abook_saved,1);
    }

    if(write_hash_header(fp_for_hash, (a_c_arg_t)ab->htable_size)){
	dprint(1,
	    (debugfile,
	    "adrbk_write(%s): write_hash_header failed\n",
	    temp_hashfile ? temp_hashfile : "?"));
	goto io_error;
    }

    writing = 1;

    /*
     * If there was an entry that just got deleted, add it at top.
     */
    if(deleted_aes){
	struct tm *tm_now;
	time_t     now;
	AE_LIST_S *e;

	now = time((time_t *)0);
	tm_now = localtime(&now);
	for(e = deleted_aes; e; e = e->next){
	    fprintf(ab_stream, "%s%02d/%02d/%02d#",
		    DELETED, (tm_now->tm_year)%100, tm_now->tm_mon+1,
		    tm_now->tm_mday);
	    if(write_single_abook_entry(e->ae, ab_stream, &this_nick_width,
		   &this_full_width, &this_addr_width, &this_fcc_width) == EOF){
		dprint(1,
		    (debugfile,
		    "adrbk_write(%s): failed writing deleted_aes\n",
		    temp_filename ? temp_filename : "?"));
		goto io_error;
	    }

	    ab->deleted_cnt++;
	}
    }

    /*
     * If there are any old deleted entries, copy them to new file.
     * We do so by scanning through the raw file looking for nicknames
     * which start with the deleted string.
     */
    if(saved_deleted_cnt > 0L){
	int rew = 1, c;
	FILE *fp_in;
	long offset, cnt_down, length;
	char *nickname;

	fp_in = ab->fp;
	if(!fp_in){
	    dprint(1, (debugfile,
		   "adrbk_write(%s): fp_in is NULL\n",
		   ab->filename ? ab->filename : "?"));
	    goto io_error;
	}

	cnt_down = saved_deleted_cnt;
	while(cnt_down > 0L
	      && (nickname = skip_to_next_nickname(fp_in,&offset,NULL,&length,
						   rew,NULL)) != NULL){
	    rew = 0;

	    if(strncmp(nickname, DELETED, DELETED_LEN) == 0
	       && isdigit((unsigned char)nickname[DELETED_LEN])
	       && isdigit((unsigned char)nickname[DELETED_LEN+1])
	       && nickname[DELETED_LEN+2] == '/'
	       && isdigit((unsigned char)nickname[DELETED_LEN+3])
	       && isdigit((unsigned char)nickname[DELETED_LEN+4])
	       && nickname[DELETED_LEN+5] == '/'
	       && isdigit((unsigned char)nickname[DELETED_LEN+6])
	       && isdigit((unsigned char)nickname[DELETED_LEN+7])
	       && nickname[DELETED_LEN+8] == '#'){
		long save_offset;
		int year, month, day;
		struct tm *tm_before;
		time_t     now, before;

		cnt_down--;
		now = time((time_t *)0);
		before = now - (time_t)ABOOK_DELETED_EXPIRE_TIME;
		tm_before = localtime(&before);
		tm_before->tm_mon++;

		/*
		 * Check to see if it is older than 100 days.
		 */
		year  = atoi(&nickname[DELETED_LEN]);
		month = atoi(&nickname[DELETED_LEN+3]);
		day   = atoi(&nickname[DELETED_LEN+6]);
		if(year < 95)
		  year += 100;  /* this breaks in year 2095 */
		
		/*
		 * only write it if it is less than 100 days old
		 */
		if(year > tm_before->tm_year
		   || (year == tm_before->tm_year
		       && month > tm_before->tm_mon)
		   || (year == tm_before->tm_year
		       && month == tm_before->tm_mon
		       && day >= tm_before->tm_mday)){

		    save_offset = ftell(fp_in);
		    if(fseek(fp_in, offset, 0)){
			dprint(1, (debugfile,
			       "adrbk_write(%s): fseek on fp_in1 failed, cnt_down = %ld\n",
			       ab->filename ? ab->filename : "?", cnt_down));
			goto io_error;
		    }

		    while(length-- > 0L)
		      if((c = getc(fp_in)) == EOF || putc(c, ab_stream) == EOF){
			  dprint(1, (debugfile,
	         "adrbk_write(%s): getc on fp_in or putc on ab_stream failed, cnt_down = %ld\n",
			       ab->filename ? ab->filename : "?", cnt_down));
			  goto io_error;
		      }

		    if(fseek(fp_in, save_offset, 0)){
			dprint(1, (debugfile,
			       "adrbk_write(%s): fseek on fp_in2 failed NULL, cnt_down = %ld\n",
			       ab->filename ? ab->filename : "?", cnt_down));
			goto io_error;
		    }
		}
		else
		  ab->deleted_cnt--;
	    }
	}

	if(cnt_down != 0L)
	  dprint(1, (debugfile,
		"adrbk_write(%s): Can't find %d commented deleteds\n",
		ab->filename ? ab->filename : "?", cnt_down));
    }

    if(ab->deleted_cnt)
      dprint(4, (debugfile, "  adrbk_write: saving %ld deleted entries\n",
			     (long)ab->deleted_cnt));

    for(entry_num = 0; entry_num < ab->count; entry_num++){
	adrbk_cntr_t actual_enum;
	int          free_the_ae;
	EntryRef     fake_entry;

	entry_num_for_percent++;

	ALARM_BLIP();

	actual_enum = sort_array ? sort_array[entry_num] : entry_num;
	if(appended_aes && actual_enum >= ab->phantom_count){
	    AE_LIST_S *e;

	    entry = &fake_entry;

	    e = appended_aes;
	    while(e){
		if(e->ent == actual_enum)
		  break;

		e = e->next;
	    }

	    ae = (AdrBk_Entry *)NULL;
	    if(e)
	      ae = e->ae;

	    if(ae == (AdrBk_Entry *)NULL){
		dprint(1,
		    (debugfile,
		    "adrbk_write(%s): can't find entry %ld in appended_aes while writing addrbook\n",
		    ab->filename ? ab->filename : "?", (long)actual_enum));
		goto io_error;
	    }

	    free_the_ae = 0;
	}
	else{
	    entry = adrbk_get_entryref(ab, (a_c_arg_t)actual_enum, Normal);

	    if(entry == NULL || entry->uid_nick == NO_UID){
		dprint(1, (debugfile,
		"adrbk_write(%s): premature end while writing addrbook (%s)\n",
		    ab->filename ? ab->filename : "?",
		    (entry == NULL) ? "entry is NULL" : "uid_nick is NO_UID"));
		goto io_error;
	    }

	    ae = init_ae_entry(ab, entry); /* free it ourselves below */
	    free_the_ae = 1;
	}

	if(ae == (AdrBk_Entry *)NULL){
	    dprint(1,
		(debugfile,
		"adrbk_write(%s): can't find ae while writing addrbook, entry_num = %ld actual_enum = %ld\n",
		ab->filename ? ab->filename : "?",
		(long)entry_num, (long)actual_enum));
	    goto io_error;
	}

	/*
	 * This works because the information is still correct for the
	 * old file.  The offsets for the old file are still the correct
	 * values, the offsets that get set below are for the temporary
	 * file, which will become the new file at the end.  There may
	 * be some ae's that don't have offsets set yet, but they should
	 * have cached ae values set if we did everything correctly.
	 */

	/* adjust EntryRef for this entry */
	entry->uid_nick  = ab_uid(ae->nickname);
	entry->offset    = ftell(ab_stream);
	entry->ae        = (AdrBk_Entry *)NULL;
	hash             = ab_hash(ae->nickname, (a_c_arg_t)ab->htable_size);
	entry->next_nick = ab->hash_by_nick->harray[hash];
	ab->hash_by_nick->harray[hash] = entry_num;
	if(ae->tag == Single){
	    entry->uid_addr  = ab_uid_addr(ae->addr.addr);
	    hash             = ab_hash_addr(ae->addr.addr,
					(a_c_arg_t)ab->htable_size);
	    entry->next_addr = ab->hash_by_addr->harray[hash];
	    ab->hash_by_addr->harray[hash] = entry_num;
	}
	else{
	    entry->uid_addr  = NO_UID;
	    entry->next_addr = NO_NEXT;
	}

	/* this writes to the OnDisk hashtable */
	if(write_single_entryref(entry, fp_for_hash)){
	    dprint(1,
		(debugfile,
		"adrbk_write(%s): failed writing hash for entry %ld\n",
		temp_hashfile ? temp_hashfile : "?",
		(long)entry_num));
	    goto io_error;
	}

	/* write to temp file */
	if(write_single_abook_entry(ae, ab_stream, &this_nick_width,
		&this_full_width, &this_addr_width, &this_fcc_width) == EOF){
	    dprint(1,
		(debugfile,
		"adrbk_write(%s): failed writing for entry %ld\n",
		temp_filename ? temp_filename : "?",
		(long)entry_num));
	    goto io_error;
	}

	if(free_the_ae)
	  free_ae(&ae);

	/* keep track of widths */
	max_nick = max(max_nick, this_nick_width);
	if(this_full_width > max_full){
	    full_three = full_two;
	    full_two   = max_full;
	    max_full   = this_full_width;
	}
	else if(this_full_width > full_two){
	    full_three = full_two;
	    full_two   = this_full_width;
	}
	else if(this_full_width > full_three){
	    full_three = this_full_width;
	}

	if(this_addr_width > max_addr){
	    addr_three = addr_two;
	    addr_two   = max_addr;
	    max_addr   = this_addr_width;
	}
	else if(this_addr_width > addr_two){
	    addr_three = addr_two;
	    addr_two   = this_addr_width;
	}
	else if(this_addr_width > addr_three){
	    addr_three = this_addr_width;
	}

	if(this_fcc_width > max_fcc){
	    fcc_three = fcc_two;
	    fcc_two   = max_fcc;
	    max_fcc   = this_fcc_width;
	}
	else if(this_fcc_width > fcc_two){
	    fcc_three = fcc_two;
	    fcc_two   = this_fcc_width;
	}
	else if(this_fcc_width > fcc_three){
	    fcc_three = this_fcc_width;
	}

	/*
	 * Check to see if we've been interrupted.  We check at the bottom
	 * of the loop so that we can handle an interrupt in the last
	 * iteration.
	 */
	if(enable_intr_handling && ps_global->intr_pending){
	    interrupt_happened++;
	    goto io_error;
	}
    }

    if(we_cancel){
	cancel_busy_alarm(-1);
	we_cancel = 0;
    }

    if(enable_intr_handling)
      intr_handling_off();

    if(fclose(ab_stream) == EOF){
	dprint(1, (debugfile, "adrbk_write: fclose for %s failed\n",
	       temp_filename ? temp_filename : "?"));
	goto io_error;
    }

    ab_stream = (FILE *)NULL;

    file_attrib_copy(temp_filename, ab->our_filecopy);
    if(ab->fp){
	(void)fclose(ab->fp);
	ab->fp = NULL;			/* in case of problems */
    }

    if((i=rename_file(temp_filename, ab->our_filecopy)) < 0){
	dprint(1, (debugfile, "adrbk_write: rename(%s, %s) failed\n",
	       temp_filename ? temp_filename : "?",
	       ab->our_filecopy ? ab->our_filecopy : "?"));
#ifdef	_WINDOWS
	if(i == -5){
	    q_status_message2(SM_ORDER | SM_DING, 5, 7,
			  "Can't replace address book %.200sfile \"%.200s\"",
			      (ab->type == Imap) ? "cache " : "",
			      ab->filename);
	    q_status_message(SM_ORDER | SM_DING, 5, 7,
    "If another Pine is running, quit that Pine before updating address book.");
	}
#endif	/* _WINDOWS */
	goto io_error;
    }

    /* reopen fp to new file */
    if(!(ab->fp = fopen(ab->our_filecopy, READ_MODE))){
	dprint(1, (debugfile, "adrbk_write: can't reopen %s\n",
	       ab->our_filecopy ? ab->our_filecopy : "?"));
	goto io_error;
    }

    if(temp_filename){
	(void)unlink(temp_filename);
	fs_give((void **)&temp_filename);
    }

    /*
     * Now copy our_filecopy back to filename.
     */
    if(ab->filename != ab->our_filecopy){
	int   c, err = 0;
	char *lc;

	if(!(ab->filename && ab->filename[0])
	   || !(temp_filename = tempfile_in_same_dir(ab->filename,"a1",NULL))
	   || (fd = open(temp_filename, OPEN_WRITE_MODE, 0600)) < 0){
	    dprint(1,
		(debugfile,
		"adrbk_write(%s): failed opening temp file (%s)\n",
		ab->filename ? ab->filename : "?",
		temp_filename ? temp_filename : "NULL"));
	    err++;
	}

	if(!err)
	  ab_stream = fdopen(fd, FOPEN_WRITE_MODE);
	  
	if(ab_stream == NULL){
	    dprint(1,
		(debugfile,
		"adrbk_write(%s): fdopen failed\n",
		temp_filename ? temp_filename : "?"));
	    err++;
	}

	if(!err){
	    rewind(ab->fp);
	    while((c = getc(ab->fp)) != EOF)
	      if(putc(c, ab_stream) == EOF){
		  err++;
		  break;
	      }
	}

	if(!err && fclose(ab_stream) == EOF)
	  err++;

	if(!err){
#ifdef	_WINDOWS
	    int tries = 3;
#endif

	    file_attrib_copy(temp_filename, ab->filename);
	    /*
	     * This could fail if somebody else has it open, but they should
	     * only have it open for a short time.
	     */
	    while(!err && rename_file(temp_filename, ab->filename) < 0){
#ifdef	_WINDOWS
		if(i == -5){
		    if(--tries <= 0)
		      err++;

		    if(!err){
			q_status_message2(SM_ORDER, 0, 3,
			      "Replace of \"%.200s\" failed, trying %.200s",
			      (lc=last_cmpnt(ab->filename)) ? lc : ab->filename,
			      (tries > 1) ? "again" : "one more time");
			display_message('x');
			sleep(3);
		    }
		}
#else	/* UNIX */
		err++;
#endif	/* UNIX */
	    }
	}

	if(err){
	    q_status_message1(SM_ORDER | SM_DING, 5, 5,
	       "Copy of addrbook to \"%.200s\" failed, changes NOT saved!",
	       (lc=last_cmpnt(ab->filename)) ? lc : ab->filename);
	    dprint(2, (debugfile, "adrbk_write: failed copying our_filecopy (%s)back to filename (%s): %s, continuing without a net\n",
		   ab->our_filecopy ? ab->our_filecopy : "?",
		   ab->filename ? ab->filename : "?",
		   error_description(errno)));
	}
    }

    progress |= AB_RENAMED_ABOOK;

    widths = &ab->widths;
    widths->max_nickname_width  = min(max_nick, 99);
    widths->max_fullname_width  = min(max_full, 99);
    widths->max_addrfield_width = min(max_addr, 99);
    widths->max_fccfield_width  = min(max_fcc, 99);
    widths->third_biggest_fullname_width  = min(full_three, 99);
    widths->third_biggest_addrfield_width = min(addr_three, 99);
    widths->third_biggest_fccfield_width  = min(fcc_three, 99);

    /* record new change date of addrbook file */
    ab->last_change_we_know_about = get_adj_name_file_mtime(ab->filename);
    
    if(write_hash_table(ab->hash_by_nick, fp_for_hash,
	(a_c_arg_t)ab->htable_size))
      goto io_error;

    if(write_hash_table(ab->hash_by_addr, fp_for_hash,
	(a_c_arg_t)ab->htable_size))
      goto io_error;

    if(write_hash_trailer(ab, fp_for_hash, (ab->flags & OUT_OF_SORTS) ? 0 : 1))
      goto io_error;

    if(fclose(fp_for_hash) == EOF){
	dprint(1, (debugfile, "adrbk_write: fclose for %s failed\n",
	       temp_hashfile ? temp_hashfile : "?"));
	goto io_error;
    }

    fp_for_hash = (FILE *)NULL;

    file_attrib_copy(temp_hashfile, ab->our_hashcopy);
    if(ab->fp_hash){
	(void)fclose(ab->fp_hash);
	ab->fp_hash = NULL;		/* in case there's a problem */
    }

    if(rename_file(temp_hashfile, ab->our_hashcopy) < 0){
	dprint(1, (debugfile, "adrbk_write: rename(%s, %s) failed\n",
	       temp_hashfile ? temp_hashfile : "?",
	       ab->our_hashcopy ? ab->our_hashcopy : "?"));
	goto io_error;
    }

    if(!(ab->fp_hash = fopen(ab->our_hashcopy, READ_MODE))){
	dprint(1, (debugfile, "adrbk_write: fopen(our_hashcopy=%s) failed\n",
	       ab->our_hashcopy ? ab->our_hashcopy : "?"));
	goto io_error;
    }

    if(temp_hashfile){
	(void)unlink(temp_hashfile);
	fs_give((void **)&temp_hashfile);
    }

    /*
     * Now copy our_hashcopy back to hashfile.
     */
    if(ab->hashfile != ab->our_hashcopy){
	int   c, err = 0;

	if(!(ab->hashfile && ab->hashfile[0])
	   || !(temp_hashfile = tempfile_in_same_dir(ab->hashfile,"a2",NULL))
	   || (fd = open(temp_hashfile, OPEN_WRITE_MODE, 0600)) < 0){
	    dprint(1,
		(debugfile,
		"adrbk_write(%s): failed opening temp file (%s)\n",
		ab->filename ? ab->filename : "?",
		temp_hashfile ? temp_hashfile : "NULL"));
	    err++;
	}

	if(!err && !(fp_for_hash = fdopen(fd, FOPEN_WRITE_MODE)))
	  err++;

	if(!err){
	    rewind(ab->fp_hash);
	    while((c = getc(ab->fp_hash)) != EOF)
	      if(putc(c, fp_for_hash) == EOF){
		  err++;
		  break;
	      }
	}

	if(!err && fclose(fp_for_hash) == EOF)
	  err++;

	if(!err){
	    file_attrib_copy(temp_hashfile, ab->hashfile);
	    if(rename_file(temp_hashfile, ab->hashfile) < 0)
	      err++;
	}

	if(err){
	    dprint(2, (debugfile, "adrbk_write: failed copying our_hashcopy (%s)back to hashfile (%s): %s, continuing\n",
		   ab->our_hashcopy ? ab->our_hashcopy : "?",
		   ab->hashfile ? ab->hashfile : "?",
		   error_description(errno)));
	}
    }

    progress |= AB_RENAMED_HASH;

    ab->phantom_count = ab->count;
    ab->orig_count    = ab->count;

#ifndef	DOS
    (void)signal(SIGHUP, save_sighup);
#endif

    /*
     * If this is a remote addressbook, copy the file over.
     * If it fails we warn but continue to operate on the changed,
     * locally cached addressbook file.
     */
    if(write_to_remote && ab->type == Imap){
	int   e;
	char datebuf[200];

	datebuf[0] = '\0';
	if(we_cancel)
	  cancel_busy_alarm(-1);

	we_cancel = busy_alarm(1, "Copying to remote addressbook", NULL, 0);
	/*
	 * We don't want a cookie upgrade to blast our data in rd->lf by
	 * copying back the remote data before doing the upgrade, and then
	 * proceeding to finish with that data. So we tell rd_upgrade_cookie
	 * about that here.
	 */
	ab->rd->flags |= BELIEVE_CACHE;
	if((e = rd_update_remote(ab->rd, datebuf)) != 0){
	    if(e == -1){
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
			"Error opening temporary addrbook file %.200s: %.200s",
				ab->rd->lf, error_description(errno));
		dprint(1, (debugfile,
		       "adrbk_write: error opening temp file %s\n",
		       ab->rd->lf ? ab->rd->lf : "?"));
	    }
	    else{
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				"Error copying to %.200s: %.200s",
				ab->rd->rn, error_description(errno));
		dprint(1, (debugfile,
		       "adrbk_write: error copying from %s to %s\n",
		       ab->rd->lf ? ab->rd->lf : "?",
		       ab->rd->rn ? ab->rd->rn : "?"));
	    }
	    
	    q_status_message(SM_ORDER | SM_DING, 5, 5,
       "Copy of addrbook to remote folder failed, changes NOT saved remotely");
	}
	else{
	    rd_update_metadata(ab->rd, datebuf);
	    ab->rd->read_status = 'W';
	    dprint(7, (debugfile,
		   "%s: copied local to remote in adrbk_write (%ld)\n",
		   ab->rd->rn ? ab->rd->rn : "?",
		   (long)ab->rd->last_use));
	}

	ab->rd->flags &= ~BELIEVE_CACHE;
    }

    /* this clears the Locked state as well as making the cache correct */
    init_entryref_cache(ab);
    writing = 0;

    if(ab->flags & FIRST_CHANGE){
	int save_sort_rule;

	ab->flags &= ~FIRST_CHANGE;  /* turn off first change flag */

	/*
	 * This shouldn't happen. This means the address book is not
	 * in order but should be. We are going to mark the (ondisk) sort order
	 * wrong so that the next time we open we'll check again and sort
	 * if it's wrong. It's too hard to stop and sort here, because
	 * we'll screw up our callers, so we're just marking that we'll
	 * check next time we open, which is a spot where we can safely
	 * sort. This will be next time we run pine, so the sort order will
	 * stay messed up for this whole session. The only time this should
	 * happen is if the user changes their locale so that the collation
	 * order invalidates the sort we've done before.
	 */
	if(ab->sort_rule != -1 &&  !adrbk_is_in_sort_order(ab, 1, 1)){
	    ab->flags |= OUT_OF_SORTS;
	    save_sort_rule = ab->sort_rule;
	    ab->sort_rule = -1;
	    fix_sort_rule_in_hash(ab);
	    ab->sort_rule = save_sort_rule;
	}
    }

    if(we_cancel)
      cancel_busy_alarm(0);

    if(temp_filename){
	(void)unlink(temp_filename);
	fs_give((void **)&temp_filename);
    }

    if(temp_hashfile){
	(void)unlink(temp_hashfile);
	fs_give((void **)&temp_hashfile);
    }

    return 0;


io_error:
    if(we_cancel)
      cancel_busy_alarm(-1);

    if(enable_intr_handling)
      intr_handling_off();

#ifndef	DOS
    (void)signal(SIGHUP, save_sighup);
#endif
    if(interrupt_happened){
	q_status_message(0, 1, 2, "Interrupt!  Reverting to previous version");
	display_message('x');
	dprint(1,
	    (debugfile, "adrbk_write(%s): Interrupt\n",
	    ab->filename ? ab->filename : "?"));
    }
    else
      dprint(1,
	(debugfile, "adrbk_write(%s) (%s): some sort of io_error\n",
	ab->filename ? ab->filename : "?",
	ab->our_filecopy ? ab->our_filecopy : "?"));

    writing = 0;

    if(ae)
      free_ae(&ae);

    if(ab_stream){
	fclose(ab_stream);
	ab_stream = (FILE *)NULL;
    }

    if(fp_for_hash){
	fclose(fp_for_hash);
	fp_for_hash = (FILE *)NULL;
    }

    if(temp_filename){
	(void)unlink(temp_filename);
	fs_give((void **)&temp_filename);
    }

    if(temp_hashfile){
	(void)unlink(temp_hashfile);
	fs_give((void **)&temp_hashfile);
    }

    ab->deleted_cnt = saved_deleted_cnt; /* is this necessary? */
    adrbk_partial_close(ab);

    /*
     * If both the address book and hash files have been replaced, this
     * means that the open failed.  This really shouldn't happen.  We don't
     * try to recover from it here.  Instead, we'll let the address book
     * trouble detection code catch it if need be.
     */
    if(progress & AB_RENAMED_HASH){
	dprint(1,
	(debugfile, "adrbk_write(%s): already renamed hashfile: %s\n",
	ab->filename ? ab->filename : "?", error_description(errno)));
    }
    /*
     * We're kind of in no man's land with this case.  The null fp_hash
     * should trigger an lu rebuild in adrbk_get_entryref.
     */
    else if(progress & AB_RENAMED_ABOOK){
	ab->fp_hash = (FILE *)NULL;
	free_ab_adrhash(&ab->hash_by_nick);
	free_ab_adrhash(&ab->hash_by_addr);
    }
    /*
     * We'd come here on an i/o error in the
     * main loop.  We revert to the previous version.
     */
    else{
	free_ab_adrhash(&ab->hash_by_nick);
	free_ab_adrhash(&ab->hash_by_addr);
	/* htable size is reset from file in bld... */
	(void)bld_hash_from_ondisk_hash(ab);
    }

    if(interrupt_happened){
	errno = EINTR;  /* for nicer error message */
	return -5;
    }
    else
      return -2;
}


/*
 * Writes one addrbook entry with wrapping.  Fills in widths.
 * Returns 0, or EOF on error.
 *
 * Continuation lines always start with spaces.  Tabs are treated as
 * separators, never as whitespace.  When we output tab separators we
 * always put them on the ends of lines, never on the start of a line
 * after a continuation.  That is, there is always something printable
 * after continuation spaces.
 */
int
write_single_abook_entry(ae, fp, ret_nick_width, ret_full_width,
			 ret_addr_width, ret_fcc_width)
    AdrBk_Entry *ae;
    FILE        *fp;
    int         *ret_nick_width, *ret_full_width,
		*ret_addr_width, *ret_fcc_width;
{
    register int len;
    int nick_width = 0, full_width = 0, addr_width = 0, fcc_width = 0;
    int tmplen, this_width;
    char *p;

    if(fp == (FILE *)NULL){
	dprint(1, (debugfile, "write_single_abook_entry: fp is NULL\n"));
	return(EOF);
    }

    /* write to temp file */
    if(fputs(ae->nickname, fp) == EOF){
	dprint(1, (debugfile, "write_single_abook_entry: fputs failed\n"));
	return(EOF);
    }

    nick_width = strlen(ae->nickname);
    putc(TAB, fp);
    len = nick_width + (TABWIDTH - nick_width % TABWIDTH);
    if(ae->fullname){
	p = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				   SIZEOF_20KBUF, ae->fullname, NULL);
	full_width = strlen(p);
	if(p == ae->fullname)
	  this_width = full_width;
	else
	  this_width = strlen(ae->fullname);

	len += (this_width + (TABWIDTH - this_width % TABWIDTH));
	if(len > 80){
	    if(fprintf(fp, "\n%s", INDENTSTR) == EOF){
		dprint(1, (debugfile,
		    "write_single_abook_entry: fputs ind1 failed\n"));
		return(EOF);
	    }

	    tmplen = this_width + INDENT;
	    len = tmplen + (TABWIDTH - tmplen % TABWIDTH);
	}

	if(fputs(ae->fullname, fp) == EOF){
	    dprint(1, (debugfile,
		"write_single_abook_entry: fputs full failed\n"));
	    return(EOF);
	}
    }
    else{
	full_width = 0;
	len += TABWIDTH;
    }

    putc(TAB, fp);
    /* special case, make sure empty list has () */
    if(ae->tag == List && ae->addr.list == NULL){
	addr_width = 0;
	putc('(', fp);
	putc(')', fp);
    }
    else if(ae->addr.addr != NULL || ae->addr.list != NULL){
	if(ae->tag == Single){
	    /*----- Single: just one address ----*/
	    p = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				       SIZEOF_20KBUF, ae->addr.addr, NULL);
	    addr_width = strlen(p);
	    if(p == ae->addr.addr)
	      this_width = addr_width;
	    else
	      this_width = strlen(ae->addr.addr);

	    len += (this_width + (TABWIDTH - this_width % TABWIDTH));
	    if(len > 80){
		if(fprintf(fp, "\n%s", INDENTSTR) == EOF){
		    dprint(1, (debugfile,
			"write_single_abook_entry: fputs ind2 failed\n"));
		    return(EOF);
		}

		tmplen = this_width + INDENT;
		len = tmplen + (TABWIDTH - tmplen % TABWIDTH);
	    }

	    if(fputs(ae->addr.addr, fp) == EOF){
		dprint(1, (debugfile,
		    "write_single_abook_entry: fputs addr failed\n"));
		return(EOF);
	    }
	}
	else if(ae->tag == List){
	    register char **a2;

	    /*----- List: a distribution list ------*/
	    putc('(', fp);
	    len++;
	    addr_width = 0;
	    for(a2 = ae->addr.list; *a2 != NULL; a2++){
		if(a2 != ae->addr.list){
		    putc(',', fp);
		    len++;
		}

		/*
		 * comma or ) also follows, so we're breaking at
		 * no more than 79 chars
		 */
		p = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					   SIZEOF_20KBUF, *a2, NULL);
		tmplen = strlen(p);
		if(p == *a2)
		  this_width = tmplen;
		else
		  this_width = strlen(*a2);

		addr_width = max(addr_width, tmplen);
		if(len + this_width > 78 && len != INDENT){
		    /*--- break up long lines ----*/
		    if(fprintf(fp, "\n%s", INDENTSTR) == EOF){
			dprint(1, (debugfile,
			    "write_single_abook_entry: fputs ind3 failed\n"));
			return(EOF);
		    }

		    len = INDENT;
		}

		if(fputs(*a2, fp) == EOF){
		    dprint(1, (debugfile,
			"write_single_abook_entry: fputs a2 failed\n"));
		    return(EOF);
		}

		len += this_width;
	    }

	    putc(')', fp);
	}
    }

    /* If either fcc or extra exists, output both, otherwise, neither */
    if((ae->fcc && ae->fcc[0]) || (ae->extra && ae->extra[0])){
	putc(TAB, fp);
	len += (TABWIDTH - len % TABWIDTH);
	if(ae->fcc && ae->fcc[0]){
	    fcc_width = strlen(ae->fcc);
	    len += (fcc_width + (TABWIDTH - fcc_width%TABWIDTH));
	    if(len > 80){
		if(fprintf(fp, "\n%s", INDENTSTR) == EOF){
		    dprint(1, (debugfile,
			"write_single_abook_entry: fprintf ind failed\n"));
		    return(EOF);
		}

		tmplen = fcc_width + INDENT;
		len = tmplen + (TABWIDTH - tmplen % TABWIDTH);
	    }

	    if(fputs(ae->fcc, fp) == EOF){
		dprint(1, (debugfile,
		    "write_single_abook_entry: fprintf fcc failed\n"));
		return(EOF);
	    }
	}
	else
	  fcc_width = 0;

	putc(TAB, fp);
	if(ae->extra && ae->extra[0]){
	    int space;
	    char *cur, *end;
	    char *extra_copy;
	    
	    /*
	     * Copy ae->extra and replace newlines with spaces.
	     * The reason we do this is because the continuation lines
	     * produced by rfc1522_encode may interact with the
	     * continuation lines produced below in a bad way.
	     * In particular, what can happen is that the 1522 continuation
	     * newline can be followed immediately by a newline produced
	     * below. That breaks the continuation since that is a
	     * line with nothing on it. Just turn 1522 continuations into
	     * spaces, which work fine with 1522_decode.
	     */
	    extra_copy = cpystr(ae->extra);
	    REPLACE_NEWLINES_WITH_SPACE(extra_copy);
	   
	    tmplen = strlen(extra_copy);
	    if(len+tmplen > 80){
		if(fprintf(fp, "\n%s", INDENTSTR) == EOF){
		    dprint(1, (debugfile,
			"write_single_abook_entry: fprintf indent failed\n"));
		    return(EOF);
		}

		len = INDENT;
	    }

	    space = max(70 - len, 5);
	    cur = extra_copy;
	    end = cur + tmplen;
	    while(cur < end){
		if(end-cur > space){
		    int i;

		    /* find first space after spot we want to break */
		    for(i = space; cur+i < end && cur[i] != SPACE; i++) 
		      ;
		    
		    cur[i] = '\0';
		    if(fputs(cur, fp) == EOF){
			dprint(1, (debugfile,
			    "write_single_abook_entry: fputs extra failed\n"));
			return(EOF);
		    }

		    cur += (i+1);

		    if(cur < end){
			if(fprintf(fp, "\n%s", INDENTXTRA) == EOF){
			    dprint(1, (debugfile,
			"write_single_abook_entry: fprintf indent failed\n"));
			    return(EOF);
			}

			space = 70 - INDENT;
		    }
		}
		else{
		    if(fputs(cur, fp) == EOF){
			dprint(1, (debugfile,
			    "write_single_abook_entry: fputs extra failed\n"));
			return(EOF);
		    }

		    cur = end;
		}
	    }

	    if(extra_copy)
	      fs_give((void **)&extra_copy);
	}
    }
    else
      fcc_width = 0;

    putc('\n', fp);
    
    if(ret_nick_width)
      *ret_nick_width = nick_width;
    if(ret_full_width)
      *ret_full_width = full_width;
    if(ret_addr_width)
      *ret_addr_width = addr_width;
    if(ret_fcc_width)
      *ret_fcc_width = fcc_width;

    return(0);
}


int
percent_abook_saved()
{
    return((int)(((unsigned long)entry_num_for_percent * (unsigned long)100) /
	(unsigned long)tot_for_percent));
}


/*
 * Free memory associated with entry ae.
 *
 * Args:  ae  -- Address book entry to be freed.
 */
void
free_ae(ae)
    AdrBk_Entry **ae;
{
    char **p;

    if(!ae || !(*ae))
      return;

    if((*ae)->nickname && (*ae)->nickname != empty)
      fs_give((void **)&(*ae)->nickname);

    if((*ae)->fullname && (*ae)->fullname != empty)
      fs_give((void **)&(*ae)->fullname);

    if((*ae)->tag == Single){
        if((*ae)->addr.addr && (*ae)->addr.addr != empty)
          fs_give((void **)&(*ae)->addr.addr);
    }
    else if((*ae)->tag == List){
        if((*ae)->addr.list){
            for(p = (*ae)->addr.list; *p; p++) 
              if(*p != empty)
	        fs_give((void **)p);

            fs_give((void **)&(*ae)->addr.list);
        }
    }

    if((*ae)->fcc && (*ae)->fcc != empty)
      fs_give((void **)&(*ae)->fcc);

    if((*ae)->extra && (*ae)->extra != empty)
      fs_give((void **)&(*ae)->extra);

    fs_give((void **)ae);
}


/*
 * Writes the Header part of on-disk hash table.
 * Returns 0 on success, -1 on failure.
 */
int
write_hash_header(fp, size)
    FILE        *fp;
    a_c_arg_t    size;
{
    char format[50];

    dprint(9, (debugfile, "- write_hash_header -\n"));

    if(fp == (FILE *)NULL){
	dprint(1, (debugfile, "write_hash_header: fp is NULL\n"));
	return -1;
    }

    sprintf(format, "%%s %%s %%%dlu\n", SIZEOF_HASH_SIZE);
    if(fprintf(fp, format, PMAGIC, ADRHASH_FILE_VERSION_NUM,
	       (unsigned long)size) == EOF){
	dprint(1, (debugfile, "write_hash_header: fprintf returned EOF\n"));
	return -1;
    }
    
    return 0;
}


int
write_single_entryref(entry, fp)
    EntryRef *entry;
    FILE    *fp;
{
    char format[50];

    if(fp == (FILE *)NULL){
	dprint(1, (debugfile, "write_single_entryref: fp is NULL\n"));
	return -1;
    }

    if(entry == (EntryRef *)NULL || entry->uid_nick == NO_UID){
	dprint(1, (debugfile,
	       "write_single_entryref: %s is %s\n",
	       entry ? "uid_nick" : "entry",
	       entry ? "NO_UID" : "NULL"));
	return -1;
    }

    /*
     * The first two widths below should be the same as SIZEOF_HASH_INDEX.
     * The next two the same as SIZEOF_UID.
     * The last one the same as SIZEOF_FILEOFFSET.
     */
    sprintf(format, "%%%dlu %%%dlu %%%dld %%%dld %%%dld\n",
	    SIZEOF_HASH_INDEX, SIZEOF_HASH_INDEX, SIZEOF_UID, SIZEOF_UID,
	    SIZEOF_FILEOFFSET);
    if(fprintf(fp, format,
	    (unsigned long)entry->next_nick,
	    (unsigned long)entry->next_addr,
	    (long)entry->uid_nick,
	    (long)entry->uid_addr,
	    (long)entry->offset) == EOF){
	dprint(1, (debugfile, "write_single_entryref: fprintf failed\n"));
	return -1;
    }

    return 0;
}


/*
 * Writes the HashTable part of on-disk hash table.
 * Returns 0 on success, -1 on failure.
 */
int
write_hash_table(ahash, fp, size_arg)
    AdrHash *ahash;
    FILE    *fp;
    a_c_arg_t size_arg;
{
    adrbk_cntr_t i;
    adrbk_cntr_t size;
    char format[50];

    dprint(9, (debugfile, "- write_hash_table -\n"));

    size = (adrbk_cntr_t)size_arg;

    if(fp == (FILE *)NULL){
	dprint(1, (debugfile, "write_hash_table: fp is NULL\n"));
	return -1;
    }

    sprintf(format, "%%%dlu\n", SIZEOF_HASH_INDEX);

    for(i = 0; i < size; i++)
      if(fprintf(fp, format, (unsigned long)(ahash->harray[i])) == EOF){
	  dprint(1, (debugfile, "write_hash_table: fprintf failed\n"));
	  return -1;
      }

    return 0;
}


/*
 * Writes the Trailer part of on-disk hash table.
 * Returns 0 on success, -1 on failure.
 */
int
write_hash_trailer(ab, fp, know_its_sorted)
    AdrBk *ab;
    FILE  *fp;
    int    know_its_sorted;
{
    WIDTH_INFO_S *widths;
    char format[50];

    dprint(9, (debugfile, "- write_hash_trailer -\n"));

    if(fp == (FILE *)NULL){
	dprint(1, (debugfile, "write_hash_trailer: fp is NULL\n"));
	return -1;
    }

    sprintf(format, "%%s %%%dlu\n", SIZEOF_COUNT);
    if(fprintf(fp, format, PMAGIC, (unsigned long)(ab->count)) == EOF){
	dprint(1, (debugfile, "write_hash_trailer: fprintf1 failed\n"));
	return -1;
    }
    
    sprintf(format, "%%%dld\n", SIZEOF_DELETED_CNT);
    if(fprintf(fp, format, ab->deleted_cnt) == EOF){
	dprint(1, (debugfile, "write_hash_trailer: fprintf2 failed\n"));
	return -1;
    }

    widths = &ab->widths;
    sprintf(format, " %%%dd %%%dd %%%dd %%%dd %%%dd %%%dd %%%dd\n",
	    SIZEOF_WIDTH, SIZEOF_WIDTH, SIZEOF_WIDTH, SIZEOF_WIDTH, 
	    SIZEOF_WIDTH, SIZEOF_WIDTH, SIZEOF_WIDTH);
    if(fprintf(fp, format,
	    min(widths->max_nickname_width, 99),
	    min(widths->max_fullname_width, 99),
	    min(widths->max_addrfield_width, 99),
	    min(widths->max_fccfield_width, 99),
	    min(widths->third_biggest_fullname_width, 99),
	    min(widths->third_biggest_addrfield_width, 99),
	    min(widths->third_biggest_fccfield_width, 99)) == EOF){
	dprint(1, (debugfile, "write_hash_trailer: fprintf3 failed\n"));
	return -1;
    }

    sprintf(format, "%%%dlu %%%dd\n", SIZEOF_TIMESTAMP, SIZEOF_SORT_RULE);
    if(fprintf(fp, format, (unsigned long)get_adj_name_file_mtime(ab->filename),
	know_its_sorted ? ab->sort_rule : -1) == EOF){
	dprint(1, (debugfile, "write_hash_trailer: fprintf4 failed\n"));
	return -1;
    }
    
    return 0;
}


/*
 * Use this size hashtables for count addrbook entries.
 */
adrbk_cntr_t
hashtable_size(count)
    a_c_arg_t count;
{
    long size;

    /*
     * These are picked almost totally out of the blue.  Higher values cause
     * larger hashtables so longer time to write and seek through them on
     * disk (and more diskspace).  Higher also implies fewer entries per
     * hash bucket so fewer seeks through the file.  Higher is probably
     * better if we can afford it.
     */
    if(count < 100)
      size = 100L;
    else if(count < 300)
      size = 300L;
    else if(count < 600)
      size = 600L;
    else if(count < 1000)
      size = 1000L;
    else if(count < 4000)
      size = 2000L;
    else if(count < 10000)
      size = 5000L;
    else if(count < 20000)
      size = 10000L;
    else if(count < 40000)
      size = 20000L;
    else if(count < 80000)
      size = 40000L;
    else if(count < 150000)
      size = 70000L;
    else if(count < 300000)
      size = 120000L;
    else
      size = 150000L;
    
    if(size > MAX_HASHTABLE_SIZE)
      size = MAX_HASHTABLE_SIZE;

    return((adrbk_cntr_t)size);
}


/*
 * This manages a cache of entryrefs and their corresponding addrbook
 * entries.  The addrbook entries could be split off to either be cached
 * or not separately, but we aren't doing that now.  It is possible for an
 * entryref to be cached without its addrbook entry being instantiated, but
 * if the cached entryref does have an addrbook entry to go with it, then
 * that cached entry is freed when the entryref is removed from the cache.
 * The virtual entryref array has ab->count entries, so may be too big to keep
 * in memory.  The current caching is LRU, except for the locked entries.
 * If the referenced flag is set in the
 * associated addrbook entry, then we leave the entry in the cache.  That's
 * because the referenced flag is not stored when it is removed from the
 * cache.  If the handling==Lock, we also leave that entry in the cache.
 *
 * Handling: Normal - Will be cached by the cache mgr (this function) and may
 *                    disappear when subsequent calls to the mgr are made.
 *         Internal - This is similar to Normal, but deleted entries are not
 *                    ignored like they are with Normal. We need this because
 *                    the hash entries and next_nick and next_addr still point
 *                    to the old entry numbers, not the adjusted numbers after
 *                    the Delete has happened. Adrbk_lookup_by_nick needs to
 *                    be able to walk those lists past the deleted entries.
 *           Delete - Will be removed from cache if there, and will be marked
 *                    as deleted so adrbk_write will skip it.
 *       SaveDelete - Will be removed from cache if there, and will be marked
 *                    as deleted so adrbk_write will skip it.  Will also be
 *                    saved in file as #DELETED- entry.
 *            Lock  - Tells the mgr to lock it in cache until told it is ok
 *                    to release from cache.  A call with handling set to
 *                    Unlock tells the mgr it is ok to release.  Actually, a
 *                    lock_ref_count is kept so that nested locks will work
 *                    properly.  Each call to Lock increments the ref count
 *                    by one and each call to Unlock decrements.  The Unlock
 *                    only changes handling to Normal when ref count reaches 0.
 *           Unlock - See Lock description.
 */
EntryRef *
adrbk_get_entryref(ab, elem_arg, handling)
    AdrBk       *ab;
    a_c_arg_t    elem_arg;
    Handling     handling;
{
    adrbk_cntr_t elem;
    EntryRef *return_entry = (EntryRef *)NULL;
    EntryRef *new_e = (EntryRef *)NULL;
    char *p;
    char line[SIZEOF_ENTRYREF_ENTRY+1];
    register ER_CACHE_ELEM_S *cptr;
    adrbk_cntr_t hash_bucket;
    ER_CACHE_ELEM_S *new_cache_element, *old_first_entry;
    ER_CACHE_ELEM_S *deleted;
    ER_CACHE_ELEM_S *head, *tail;
    ER_CACHE_ELEM_S *preceding_elem, *following_elem, *previous_top_dog;
    AdrBk_Entry *ae;
    int entryref_returned_null = 0;

    elem = (adrbk_cntr_t)elem_arg;

    dprint(9, (debugfile,
	"adrbk_get_entryref(%s) - elem=%lu (%s)\n",
	ab->filename ? ab->filename : "?",
	(unsigned long)elem,
	handling==Normal ? "Normal" :
	 handling==Delete ? "Delete" :
	  handling==SaveDelete ? "SaveDelete" :
	   handling==Lock ? "Lock" :
	    handling==Internal ? "Internal" :
	     handling==Unlock ? "Unlock" :
				"Unknown"));

    if(!ab || !ab->fp_hash)
      goto big_trouble;
    
    /*
     * Deleted_elem just marks that element as deleted, but it still shows
     * up there. So, we have to skip over it. The caller won't know about
     * deleted_elem, so the caller will be using element numbers as if
     * deleted wasn't there. Adjust for that. This is only used for
     * short times before we rewrite the adrbk to disk with adrbk_write.
     * It gives us a way to delete an entry.
     *
     * Late breaking news: Now there is a whole list of deleted_elems instead
     * of just a single deleted_elem, but the idea is the same.
     *
     *  0  1  D  3  4  5  Looks like this to us, deleted_elem = 2.
     *  0  1     2  3  4  Treat like this.  Note, count = 5, not 6.
     *
     * insert_before is like deleted_elem, but the opposite. If insert_before
     * is 5, that means it logically belongs before elem number 5. Have to
     * adjust element numbers to reflect that.
     *  0  1  2  3  4  5
     *             ^
     *             insert_before = 4.
     *  0  1  2  3  5  6
     *             ^
     *             inserted_entryref goes here when you ask for 4.
     *
     * Can't have both but there is also moved_elem to move a single element
     * from one place to another.
     *  0  1  2  3  4  5  6
     *       ^         ^  moved_elem = 5, move_elem_before = 2
     *  0  1  5  2  3  4  6
     *
     *       or
     *
     *  0  1  2  3  4  5  6
     *        ^       ^   moved_elem = 2, move_elem_before = 5
     *  0  1  3  4  2  5  6
     *
     *       or          special case, move_elem_before = count
     *
     *  0  1  2  3  4  5  6
     *        ^             ^  moved_elem = 2, move_elem_before = 7
     *  0  1  3  4  5  6  2
     */
    if((handling == Delete || handling == SaveDelete) &&
       (insert_before != NO_NEXT || moved_elem != NO_NEXT)){
	panic("Programming botch: Delete with insert or moved already set");
    }

    if(moved_elem != NO_NEXT &&
       (insert_before != NO_NEXT || (deleted_elems && deleted_elems->next))){
	panic("Programming botch: Moved with deleted or insert already set");
    }

    if(insert_before != NO_NEXT){
	if(elem == insert_before)
	  return(&inserted_entryref);

	if(elem > insert_before)
	  elem--;
    }

    if(ab == edited_abook && deleted_elems && deleted_elems->next &&
       handling != Internal){
	adrbk_cntr_t nextnum;
	EXPANDED_S *next_one;

	if(elem >= ab->orig_count)
	  panic("Botch: Asked for undeleted entry >= orig_count");
	
	next_one = deleted_elems;
	while((nextnum = entry_get_next(&next_one)) != NO_NEXT &&
	      nextnum <= elem)
	  elem++;

	/* this should not happen */
	if(elem >= ab->orig_count)
	  return((EntryRef *)NULL);
    }

    if(moved_elem != NO_NEXT){
	adrbk_cntr_t el_small, el_large;

	el_small = min(moved_elem, move_elem_before);
	el_large = max(moved_elem, move_elem_before);

	if(elem < el_small ||
	   elem > el_large ||
	   moved_elem == move_elem_before ||
	   moved_elem == move_elem_before - 1){
	    /* no change */
        }
	else if(moved_elem > move_elem_before){
	    if(elem == move_elem_before)
	      elem = moved_elem;
	    else
	      elem--;
	}
	else{ /* moved_elem < move_elem_before */
	    if(elem == move_elem_before - 1)
	      elem = moved_elem;
	    else if(elem < move_elem_before)
	      elem++;
	}
    }


    if(handling == Delete || handling == SaveDelete){
	AE_LIST_S *e;

	/*
	 * Before we continue, we first want to get the deleted entry
	 * and store it for use in adrbk_write.
	 */
	if(handling == SaveDelete){
	    ae = adrbk_get_ae(ab, elem_arg, Normal);
	    /* add it to the beginning of the deleted_ae list */
	    if(ae){
	      AE_LIST_S *new;

	      new = (AE_LIST_S *)fs_get(sizeof(AE_LIST_S));
	      new->ae = copy_ae(ae);
	      new->next = deleted_aes;
	      deleted_aes = new;
	    }
	}

	dprint(6, (debugfile,
	   "adrbk_get_entryref: entry marked deleted, count was %lu, now %lu\n",
	   (unsigned long)ab->count, (unsigned long)(ab->count - 1)));
	ab->count--;
	ab->phantom_count--;
	if(!deleted_elems){
	    deleted_elems = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
	    deleted_elems->next = (EXPANDED_S *)NULL;
	}

	entry_set_deleted(deleted_elems, (a_c_arg_t)elem);

	edited_abook = ab;

	/*
	 * Any entry nums in the appended list have to be decreased by one.
	 */
	for(e = appended_aes; e; e = e->next)
	  e->ent--;
    }

    hash_bucket = elem % ab->er_hashsize;

    head = ab->head_cache_elem[hash_bucket];
    tail = ab->tail_cache_elem[hash_bucket];

    /* Look for element number elem in cache */
    cptr = head;
    while(cptr && cptr->elem != elem)
      cptr = cptr->next;

    if(cptr){  /* it was in cache */

	if(handling == Delete || handling == SaveDelete){
	    dprint(9, (debugfile, "deleting %u from cache\n", elem));
	    deleted              = cptr;
	    preceding_elem       = deleted->prev;
	    following_elem       = deleted->next;
	    if(preceding_elem)
	      preceding_elem->next = following_elem;
	    else{
		ab->head_cache_elem[hash_bucket] = following_elem;
		head = ab->head_cache_elem[hash_bucket];
	    }

	    if(following_elem)
	      following_elem->prev = preceding_elem;
	    else{
		ab->tail_cache_elem[hash_bucket] = preceding_elem;
		tail = ab->tail_cache_elem[hash_bucket];
	    }

	    free_ab_entryref(deleted->entry);
	    ab->n_ae_cached_in_this_bucket[hash_bucket]--;
	    fs_give((void **)&deleted);
	    return((EntryRef *)NULL);
	}
	else{
	    if(handling == Unlock){
		cptr->lock_ref_count--;
		if(cptr->lock_ref_count <= 0)
		  cptr->handling = Normal;
	    }
	    else if(handling == Lock){
		cptr->handling = Lock;
		cptr->lock_ref_count++;
	    }

	    return_entry = cptr->entry;
	}

	/*
	 * Put this entry back at head of cache so that we remain LRU (within
	 * each hash bucket).
	 */
	 if(cptr->prev != (ER_CACHE_ELEM_S *)NULL){
	    preceding_elem         = cptr->prev;
	    following_elem         = cptr->next;
	    previous_top_dog       = head;

	    preceding_elem->next   = following_elem;
	    if(following_elem)
	      following_elem->prev = preceding_elem;
	    else
	      ab->tail_cache_elem[hash_bucket] = preceding_elem;

	    ab->head_cache_elem[hash_bucket] = cptr;
	    previous_top_dog->prev = cptr;
	    cptr->next             = previous_top_dog;
	    cptr->prev             = (ER_CACHE_ELEM_S *)NULL;
	 }
    }
    else{  /* it was not in cache */
      if(handling != Delete && handling != SaveDelete){
	p = get_entryref_line_from_disk(ab->fp_hash, line, (a_c_arg_t)elem);

	if(p){

	    /* fill in entryref from disk */

	    new_e = new_entryref(NO_UID, NO_UID, -1L);
	    new_e->next_nick = (adrbk_cntr_t)strtoul(p, (char **)NULL, 10);
	    p += (SIZEOF_HASH_INDEX + SIZEOF_SPACE);
	    new_e->next_addr = (adrbk_cntr_t)strtoul(p, (char **)NULL, 10);
	    p += (SIZEOF_HASH_INDEX + SIZEOF_SPACE);
	    new_e->uid_nick  = (adrbk_uid_t)atol(p);
	    p += (SIZEOF_UID + SIZEOF_SPACE);
	    new_e->uid_addr  = (adrbk_uid_t)atol(p);
	    p += (SIZEOF_UID + SIZEOF_SPACE);
	    new_e->offset    = atol(p);
	    new_e->ae        = (AdrBk_Entry *)NULL;

	    if(handling == Internal && ab == edited_abook &&
	       deleted_elems && deleted_elems->next){
		adrbk_cntr_t nextnum;
		EXPANDED_S  *next_one;

		next_one = deleted_elems;
		while((nextnum = entry_get_next(&next_one)) != NO_NEXT)
		  if(elem == nextnum){
		      new_e->is_deleted = 1;
		      return(new_e);	/* have to free this in caller! */
		  }
	    }

	    new_cache_element = (ER_CACHE_ELEM_S *)NULL;

	    /* Remove some old cache entries */
	    if(ab->n_ae_cached_in_this_bucket[hash_bucket] >=
		CACHE_PER_ER_BUCKET){

		/*
		 * Only delete if hasn't been referenced, for loop detect.
		 * This is because the reference count will go away if we
		 * delete it from the cache.
		 *
		 * Don't delete Locked entries.
		 *
		 * Find entries we can delete until we get down to the
		 * number we want in cache.
		 */
		for(cptr = tail; cptr; cptr = cptr->prev){

		    if(cptr->handling == Lock)
		      continue;

		    if(cptr->entry && cptr->entry->ae){
			ae = cptr->entry->ae;
			if(ae->referenced != 0)
			    continue;
		    }

		    deleted              = cptr;
		    preceding_elem       = deleted->prev;
		    following_elem       = deleted->next;
		    if(preceding_elem)
		      preceding_elem->next = following_elem;
		    else{
			ab->head_cache_elem[hash_bucket] = following_elem;
			head = ab->head_cache_elem[hash_bucket];
		    }

		    if(following_elem)
		      following_elem->prev = preceding_elem;
		    else{
			ab->tail_cache_elem[hash_bucket] = preceding_elem;
			tail = ab->tail_cache_elem[hash_bucket];
		    }

		    free_ab_entryref(deleted->entry);
		    ab->n_ae_cached_in_this_bucket[hash_bucket]--;
		    if(ab->n_ae_cached_in_this_bucket[hash_bucket] >=
			CACHE_PER_ER_BUCKET){
			fs_give((void **)&deleted);
		    }
		    else{
			new_cache_element = deleted;
			break;
		    }
		}
	    }

	    if(new_cache_element == (ER_CACHE_ELEM_S *)NULL)
		new_cache_element =
		    (ER_CACHE_ELEM_S *)fs_get(sizeof(ER_CACHE_ELEM_S));

	    /* insert new entry at head of cache */

	    /*
	     * Unlock happening here would be a mistake, because Unlock should
	     * only be called after a Lock, and a Locked entry would have
	     * been in the cache.  But no harm in making it act just like
	     * a Normal.
	     */
	    new_cache_element->lock_ref_count = 0;
	    if(handling == Normal || handling == Internal || handling == Unlock)
	      new_cache_element->handling = Normal;
	    else if(handling == Lock){
		new_cache_element->handling = Lock;
		new_cache_element->lock_ref_count++;
	    }

	    new_cache_element->elem     = elem;
	    new_cache_element->entry    = new_e;
	    old_first_entry             = head;
	    ab->head_cache_elem[hash_bucket] = new_cache_element;
	    if(old_first_entry)
	      old_first_entry->prev       = new_cache_element;
	    else
	      ab->tail_cache_elem[hash_bucket] = new_cache_element;

	    new_cache_element->next     = old_first_entry;
	    new_cache_element->prev     = (ER_CACHE_ELEM_S *)NULL;
	    ab->n_ae_cached_in_this_bucket[hash_bucket]++;

	    return_entry = new_e;
	}
	else{
	    /*
	     * This shouldn't happen.  It is possible that it might happen
	     * if we get a stale NFS handle, in which case we want to reset
	     * the addrbook and retry the open.
	     */
	    entryref_returned_null = 1;
	}
      }
    }

big_trouble:
    if(return_entry == (EntryRef *)NULL){
	if(!ab){
	    dprint(1, (debugfile,
	      "adrbk_get_entryref: ab is NULL, should not happen.\n"));
	}
	else if(!ab->fp_hash){
	    dprint(1, (debugfile,
	      "adrbk_get_entryref: ab->fp_hash is NULL, should not happen.\n"));
	}
	else if(elem >= ab->count){
	  dprint(1, (debugfile,
     "adrbk_get_entryref: elem >= ab->count (%ld >= %ld), should not happen.\n",
     (long)elem, (long)ab->count));
	}
	else if(entryref_returned_null){
	    dprint(1, (debugfile,
      "\n\n ADDR    ::: the addressbook lookup file %s\n",
	      ab->hashfile ? ab->hashfile : "?"));
	    dprint(1, (debugfile,
      " BOOK    ::: seems to be unreadable.  The lookup\n"));
	    dprint(1, (debugfile,
      " TROUBLE ::: file may have to be removed and rebuilt.\n"));
	    dprint(1, (debugfile,
     "         ::: Usually it will fix itself, but if it doesn't, or if it\n"));
	    dprint(1, (debugfile,
      "         ::: is building temporary lookup files for each user,\n"));
	    dprint(1, (debugfile,
      "         ::: the sys admin should rebuild it (%s).\n\n",
	      ab->hashfile ? ab->hashfile : "?"));
	}
	else{
	    dprint(1, (debugfile,
	      "adrbk_get_entryref: returned NULL\n"));
	}

	dprint(1, (debugfile,
	  "There must have been a problem opening or closing or something.\n"));

	q_status_message1(SM_ORDER | SM_DING, 5, 5, "Addrbook problems%.200s",
			  (trouble_rebuilds<MAX_TROUBLE_REBUILDS)
			    ? ", will attempt to resync..." : "");

	if(trouble_rebuilds < MAX_TROUBLE_REBUILDS){
	    dprint(1, (debugfile,
	      "Will attempt to longjmp to safe place and try again.\n"));

	    if(writing){
	       writing = 0;
	       q_status_message(SM_ORDER, 3, 5,
		   "Aborting our change to avoid damage...");
	    }

	    trouble_rebuilds++;
	    /* jump back to a safe place */
	    trouble_filename = (ab && ab->orig_filename)
				? cpystr(ab->orig_filename)
				: cpystr("");
	    longjmp(addrbook_changed_unexpectedly, 1);
	    /*NOTREACHED*/
	}
    }

    return(return_entry);
}


/*
 * It is safe to set this higher than the number of entries in the
 * addrbook, in the sense that it will only use the number of entries.  It
 * will, however, use up lots of memory if that number is big.
 */
long
adrbk_set_nominal_cachesize(ab, new_size)
    AdrBk *ab;
    long   new_size;
{
    long old_size;

    old_size = ab->nominal_max_cached;

    dprint(9, (debugfile, "- adrbk_set_nominal_cachesize - was %ld now %ld\n",
	old_size, new_size));

    ab->nominal_max_cached = new_size;

    init_entryref_cache(ab);

    return(old_size);
}


long
adrbk_get_nominal_cachesize(ab)
    AdrBk *ab;
{
    return(ab->nominal_max_cached);
}


/*
 * Adds a new entryref which points to new_ae before put_it_before_this.
 */
void
set_inserted_entryref(ab, put_it_before_this, new_ae)
    AdrBk *ab;
    a_c_arg_t put_it_before_this;
    AdrBk_Entry *new_ae;
{
    dprint(5, (debugfile,
       "adrbk_add: entry marked inserted, count was %lu, now %lu\n",
       (unsigned long)ab->count, (unsigned long)(ab->count + 1)));

    ab->count++;
    ab->phantom_count++;
    insert_before              = (adrbk_cntr_t)put_it_before_this;
    inserted_entryref.uid_nick = !NO_UID;
    inserted_entryref.ae       = new_ae;
}

/*
 * Moves element move_this_one before element put_it_before_this.
 */
void
set_moved_entryref(move_this_one, put_it_before_this)
    a_c_arg_t move_this_one;
    a_c_arg_t put_it_before_this;
{
    dprint(7, (debugfile, "- set_moved_entryref -\n"));

    moved_elem       = (adrbk_cntr_t)move_this_one;
    move_elem_before = (adrbk_cntr_t)put_it_before_this;
}


void
init_entryref_cache(ab)
    AdrBk *ab;
{
    adrbk_cntr_t i;
    adrbk_cntr_t new_hashsize;

    dprint(9, (debugfile, "- init_entryref_cache -\n"));

    if(ab->er_hashsize != 0) /* an indication we've init'd before */
      clear_entryref_cache(ab);

    new_hashsize = min((adrbk_cntr_t)(ab->nominal_max_cached /
						    CACHE_PER_ER_BUCKET),
		       (adrbk_cntr_t)30000);

    if(new_hashsize == 0)
      new_hashsize = 1;

    if(new_hashsize != ab->er_hashsize){

	ab->er_hashsize = new_hashsize;

	/* hash array of head pointers */
	if(ab->head_cache_elem)
	  fs_give((void **)&ab->head_cache_elem);

	if(ab->tail_cache_elem)
	  fs_give((void **)&ab->tail_cache_elem);

	if(ab->n_ae_cached_in_this_bucket)
	  fs_give((void **)&ab->n_ae_cached_in_this_bucket);

	ab->head_cache_elem =
	  (ER_CACHE_ELEM_S **)fs_get((size_t)ab->er_hashsize *
				      sizeof(ER_CACHE_ELEM_S *));
	ab->tail_cache_elem =
	  (ER_CACHE_ELEM_S **)fs_get((size_t)ab->er_hashsize *
				      sizeof(ER_CACHE_ELEM_S *));
	ab->n_ae_cached_in_this_bucket =
	  (int *)fs_get((size_t)ab->er_hashsize * sizeof(int));
	memset(ab->n_ae_cached_in_this_bucket, 0,
	    (size_t)ab->er_hashsize * sizeof(int));
    }

    for(i = 0; i < ab->er_hashsize; i++){
	ab->head_cache_elem[i] = (ER_CACHE_ELEM_S *)NULL;
	ab->tail_cache_elem[i] = (ER_CACHE_ELEM_S *)NULL;
    }

    edited_abook = NULL;

    if(deleted_elems){
	exp_free(deleted_elems);
	fs_give((void **)&deleted_elems);  /* free head of list, too */
    }

    if(deleted_aes){
	AE_LIST_S *e, *next_one;

	e = deleted_aes;
	while(e){
	    next_one = e->next;
	    free_ae(&(e->ae));
	    fs_give((void **)&e);
	    e = next_one;
	}

	deleted_aes = NULL;
    }

    if(appended_aes){
	AE_LIST_S *e, *next_one;

	e = appended_aes;
	while(e){
	    next_one = e->next;
	    free_ae(&(e->ae));
	    fs_give((void **)&e);
	    e = next_one;
	}

	appended_aes = NULL;
    }

    moved_elem       = NO_NEXT;
    move_elem_before = NO_NEXT;
    if(insert_before != NO_NEXT){
	insert_before    = NO_NEXT;
	if(inserted_entryref.ae != (AdrBk_Entry *)NULL){
	    free_ae(&(inserted_entryref.ae));
	    inserted_entryref.ae = (AdrBk_Entry *)NULL;
	}
    }
}


/*
 * Clear the entire cache.
 */
void
clear_entryref_cache(ab)
    AdrBk *ab;
{
    ER_CACHE_ELEM_S *cptr, *next;
    adrbk_cntr_t i;

    dprint(9, (debugfile, "- clear_entryref_cache -\n"));

    for(i = 0; i < ab->er_hashsize; i++){
	ab->n_ae_cached_in_this_bucket[i] = 0;
	cptr = ab->head_cache_elem[i];
	while(cptr){
	    free_ab_entryref(cptr->entry);
	    next = cptr->next;
	    fs_give((void **)&cptr);
	    cptr = next;
	}
    }
}


void
clearrefs_in_cached_aes(ab)
    AdrBk *ab;
{
    ER_CACHE_ELEM_S *cptr;
    adrbk_cntr_t i;

    for(i = 0; i < ab->er_hashsize; i++){
	cptr = ab->head_cache_elem[i];
	while(cptr){
	    if(cptr->entry && cptr->entry->ae)
	      cptr->entry->ae->referenced = 0;

	    cptr = cptr->next;
	}
    }
}


/*
 * Free the list of distribution lists which have been expanded.
 * Leaves the head of the list alone.
 *
 * Args:  exp_head -- Head of the expanded list.
 */
void
exp_free(exp_head)
    EXPANDED_S *exp_head;
{
    EXPANDED_S *e, *the_next_one;

    e = exp_head ? exp_head->next : NULL;

    if(!e)
      return;

    while(e){
	the_next_one = e->next;
	fs_give((void **)&e);
	e = the_next_one;
    }

    exp_head->next = (EXPANDED_S *)NULL;
}


/*
 * Is entry n expanded?
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num to check
 */
int
exp_is_expanded(exp_head, n)
    EXPANDED_S *exp_head;
    a_c_arg_t   n;
{
    register EXPANDED_S *e;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;

    e = exp_head ? exp_head->next : NULL;

    /*
     * The list is kept ordered, so we search until we find it or are
     * past it.
     */
    while(e){
	if(e->ent >= nn)
	  break;
	
	e = e->next;
    }
    
    return(e && e->ent == nn);
}


/*
 * How many entries expanded in this addrbook.
 *
 * Args:  exp_head -- Head of the expanded list.
 */
int
exp_howmany_expanded(exp_head)
    EXPANDED_S *exp_head;
{
    register EXPANDED_S *e;
    int                  cnt = 0;

    e = exp_head ? exp_head->next : NULL;

    while(e){
	cnt++;
	e = e->next;
    }
    
    return(cnt);
}


/*
 * Are any entries expanded?
 *
 * Args:  exp_head -- Head of the expanded list.
 */
int
exp_any_expanded(exp_head)
    EXPANDED_S *exp_head;
{
    return(exp_head && exp_head->next != NULL);
}


/*
 * Return next entry num in list.
 *
 * Args:  cur -- Current position in the list.
 *
 * Result: Returns the number of the next entry, or NO_NEXT if there is
 *	   no next entry.  As a side effect, the cur pointer is incremented.
 */
adrbk_cntr_t
exp_get_next(cur)
    EXPANDED_S **cur;
{
    adrbk_cntr_t ret = NO_NEXT;

    if(cur && *cur && (*cur)->next){
	ret  = (*cur)->next->ent;
	*cur = (*cur)->next;
    }

    return(ret);
}


/*
 * Mark entry n as being expanded.
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num to mark
 */
void
exp_set_expanded(exp_head, n)
    EXPANDED_S *exp_head;
    a_c_arg_t   n;
{
    register EXPANDED_S *e;
    EXPANDED_S *new;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;
    if(!exp_head)
      panic("exp_head not set in exp_set_expanded");

    for(e = exp_head; e->next; e = e->next)
      if(e->next->ent >= nn)
	break;
    
    if(e->next && e->next->ent == nn) /* already there */
      return;

    /* add new after e */
    new       = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
    new->ent  = nn;
    new->next = e->next;
    e->next   = new;
}


/*
 * Mark entry n as being *not* expanded.
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num to mark
 */
void
exp_unset_expanded(exp_head, n)
    EXPANDED_S *exp_head;
    a_c_arg_t   n;
{
    register EXPANDED_S *e;
    EXPANDED_S *delete_this_one = NULL;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;
    if(!exp_head)
      panic("exp_head not set in exp_unset_expanded");

    for(e = exp_head; e->next; e = e->next)
      if(e->next->ent >= nn)
	break;
    
    if(e->next && e->next->ent == nn){
	delete_this_one = e->next;
	e->next = e->next->next;
    }

    if(delete_this_one)
      fs_give((void **)&delete_this_one);
}


/*
 * Adjust the "expanded" list to correspond to addrbook entry n being
 * deleted.
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num being deleted
 */
void
exp_del_nth(exp_head, n)
    EXPANDED_S *exp_head;
    a_c_arg_t   n;
{
    register EXPANDED_S *e;
    int delete_when_done = 0;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;
    if(!exp_head)
      panic("exp_head not set in exp_del_nth");

    e = exp_head->next;
    while(e && e->ent < nn)
      e = e->next;
    
    if(e){
	if(e->ent == nn){
	    delete_when_done++;
	    e = e->next;
	}

	while(e){
	    e->ent--; /* adjust entry nums */
	    e = e->next;
	}

	if(delete_when_done)
	  exp_unset_expanded(exp_head, n);
    }
}


/*
 * Adjust the "expanded" list to correspond to a new addrbook entry being
 * added between current entries n-1 and n.
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num being added
 *
 * The new entry is not marked expanded.
 */
void
exp_add_nth(exp_head, n)
    EXPANDED_S *exp_head;
    a_c_arg_t   n;
{
    register EXPANDED_S *e;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;
    if(!exp_head)
      panic("exp_head not set in exp_add_nth");

    e = exp_head->next;
    while(e && e->ent < nn)
      e = e->next;
    
    while(e){
	e->ent++; /* adjust entry nums */
	e = e->next;
    }
}


static AdrBk *ab_for_sort;

/*
 * Compare two address book entries.  Args are AdrBk_Entry **'s.
 * Sorts lists after simple addresses and then sorts on Fullname field.
 */
int
cmp_ae_by_full_lists_last(a, b)
    const QSType *a,
		 *b;
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;
    int result;

    if((*x)->tag == List && (*y)->tag == Single)
      result = 1;
    else if((*x)->tag == Single && (*y)->tag == List)
      result = -1;
    else{
	register char *p, *q, *r, *s;
	char *dummy = NULL;

	p = (*x)->fullname;
	if(*p == '"' && *(p+1))
	  p++;

	q = (*y)->fullname;
	if(*q == '"' && *(q+1))
	  q++;

	r = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				   SIZEOF_20KBUF, p, &dummy);
	if(dummy){
	    fs_give((void **)&dummy);
	    dummy = NULL;
	}
	
	s = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf+10000,
				   SIZEOF_20KBUF-10000, q, &dummy);
	if(dummy)
	  fs_give((void **)&dummy);

	result = (*pcollator)(r, s);
	if(result == 0)
	  result = (*pcollator)((*x)->nickname, (*y)->nickname);
    }
      
    return(result);
}


/*
 * Compare two address book entries.  Args are adrbk_cntr_t *'s (element #'s).
 * Sorts lists after simple addresses and then sorts on Fullname field.
 */
int
cmp_cntr_by_full_lists_last(a, b)
    const QSType *a,
		 *b;
{
    adrbk_cntr_t *x = (adrbk_cntr_t *)a,  /* *x is an element_number */
                 *y = (adrbk_cntr_t *)b;
    AdrBk_Entry  *x_ae,
		 *y_ae;

    if(ps_global->intr_pending)
      longjmp(jump_over_qsort, 1);

    ALARM_BLIP();

    x_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*x), Normal);
    y_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*y), Normal);

    return(cmp_ae_by_full_lists_last((const QSType *) &x_ae,
				     (const QSType *) &y_ae));
}


/*
 * Compare two address book entries.  Args are AdrBk_Entry **'s.
 * Sorts on Fullname field.
 */
int
cmp_ae_by_full(a, b)
    const QSType *a,
		 *b;
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;
    int result;
    register char *p, *q, *r, *s;
    char *dummy = NULL;

    p = (*x)->fullname;
    if(*p == '"' && *(p+1))
      p++;

    q = (*y)->fullname;
    if(*q == '"' && *(q+1))
      q++;

    r = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
			       SIZEOF_20KBUF, p, &dummy);
    if(dummy){
	fs_give((void **)&dummy);
	dummy = NULL;
    }
    
    s = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf+10000,
			       SIZEOF_20KBUF-10000, q, &dummy);
    if(dummy)
      fs_give((void **)&dummy);

    result = (*pcollator)(r, s);
    if(result == 0)
      result = (*pcollator)((*x)->nickname, (*y)->nickname);
      
    return(result);
}


/*
 * Compare two address book entries.  Args are adrbk_cntr_t *'s (element #'s).
 * Sorts on Fullname field.
 */
int
cmp_cntr_by_full(a, b)
    const QSType *a,
		 *b;
{
    adrbk_cntr_t *x = (adrbk_cntr_t *)a,  /* *x is an element_number */
                 *y = (adrbk_cntr_t *)b;
    AdrBk_Entry  *x_ae,
		 *y_ae;

    if(ps_global->intr_pending)
      longjmp(jump_over_qsort, 1);

    ALARM_BLIP();

    x_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*x), Normal);
    y_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*y), Normal);

    return(cmp_ae_by_full((const QSType *) &x_ae, (const QSType *) &y_ae));
}


/*
 * Compare two address book entries.  Args are AdrBk_Entry **'s.
 * Sorts lists after simple addresses and then sorts on Nickname field.
 */
int
cmp_ae_by_nick_lists_last(a, b)
    const QSType *a,
		 *b;
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;
    int result;

    if((*x)->tag == List && (*y)->tag == Single)
      result = 1;
    else if((*x)->tag == Single && (*y)->tag == List)
      result = -1;
    else
      result = (*pcollator)((*x)->nickname, (*y)->nickname);

    return(result);
}


/*
 * Compare two address book entries.  Args are adrbk_cntr_t *'s (element #'s).
 * Sorts lists after simple addresses and then sorts on Nickname field.
 */
int
cmp_cntr_by_nick_lists_last(a, b)
    const QSType *a,
		 *b;
{
    adrbk_cntr_t *x = (adrbk_cntr_t *)a,  /* *x is an element_number */
                 *y = (adrbk_cntr_t *)b;
    AdrBk_Entry  *x_ae,
		 *y_ae;

    if(ps_global->intr_pending)
      longjmp(jump_over_qsort, 1);

    ALARM_BLIP();

    x_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*x), Normal);
    y_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*y), Normal);

    return(cmp_ae_by_nick_lists_last((const QSType *) &x_ae,
				     (const QSType *) &y_ae));
}


/*
 * Compare two address book entries.  Args are AdrBk_Entry **'s.
 * Sorts on Nickname field.
 */
int
cmp_ae_by_nick(a, b)
    const QSType *a,
		 *b;
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;

    return((*pcollator)((*x)->nickname, (*y)->nickname));
}


/*
 * Compare two address book entries.  Args are adrbk_cntr_t *'s (element #'s).
 * Sorts on Nickname field.
 */
int
cmp_cntr_by_nick(a, b)
    const QSType *a,
		 *b;
{
    adrbk_cntr_t *x = (adrbk_cntr_t *)a,  /* *x is an element_number */
                 *y = (adrbk_cntr_t *)b;
    AdrBk_Entry  *x_ae,
		 *y_ae;

    if(ps_global->intr_pending)
      longjmp(jump_over_qsort, 1);

    ALARM_BLIP();

    x_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*x), Normal);
    y_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*y), Normal);

    return(cmp_ae_by_nick((const QSType *) &x_ae, (const QSType *) &y_ae));
}


/*
 * For sorting a simple list of pointers to addresses (skip initial quotes)
 */
int
cmp_addr(a1, a2)
    const QSType *a1, *a2;
{
    char *x = *(char **)a1, *y = *(char **)a2;
    char *r, *s;
    char *dummy = NULL;

    if(x && *x == '"')
      x++;

    if(y && *y == '"')
      y++;
    
    r = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
			       SIZEOF_20KBUF, x, &dummy);
    if(dummy){
	fs_give((void **)&dummy);
	dummy = NULL;
    }
    
    s = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf+10000,
			       SIZEOF_20KBUF-10000, y, &dummy);
    if(dummy)
      fs_give((void **)&dummy);

    return((*pcollator)(r, s));
}


/*
 * Sort an array of strings, except skip initial quotes.
 */
void
sort_addr_list(list)
    char **list;
{
    register char **p;

    /* find size of list */
    for(p = list; *p != NULL; p++)
      ;/* do nothing */

    qsort((QSType *)list,
#ifdef DYN
          (p - list),
#else          
          (size_t)(p - list),
#endif          
	      sizeof(char *), cmp_addr);
}


/*
 * Sort this address book.
 *
 * Args: ab            -- address book to sort
 *   current_entry_num -- see next description
 *   new_entry_num     -- return new entry_num of current_entry_num here
 *
 * Result: return code:  0 all went well
 *                      -2 error writing address book, check errno
 *
 * The sorting strategy is to allocate an array of length ab->count which
 * contains the element numbers 0, 1, ..., ab->count - 1, representing the
 * entries in the addrbook, of course.  Sort the array, then write it out in
 * the new order and start over from there.
 */
int
adrbk_sort(ab, current_entry_num, new_entry_num, be_quiet)
    AdrBk        *ab;
    a_c_arg_t     current_entry_num;
    adrbk_cntr_t *new_entry_num;
    int           be_quiet;
{
    adrbk_cntr_t *sort_array;
    long i;
    long count;
    int result;
    int skip_the_sort = 0;
    int we_cancel = 0;

    dprint(5, (debugfile, "- adrbk_sort -\n"));

    count = (long)(ab->count);

    if(!ab)
      return -2;

    if(ab->sort_rule == AB_SORT_RULE_NONE)
      return 0;
    
    if(count < 2)
      return 0;

    sort_array = (adrbk_cntr_t *)fs_get((size_t)count * sizeof(adrbk_cntr_t));
    
    for(i = 0L; i < count; i++)
      sort_array[i] = (adrbk_cntr_t)i;
	
    ab_for_sort = ab;


    if(setjmp(jump_over_qsort))
      skip_the_sort = 1;

    if(!skip_the_sort){
	intr_handling_on();
	if(!be_quiet)
	  we_cancel = busy_alarm(1, "Sorting address book", NULL, 1);

	qsort((QSType *)sort_array,
	    (size_t)count,
	    sizeof(adrbk_cntr_t),
	    (ab->sort_rule == AB_SORT_RULE_FULL_LISTS) ?
						cmp_cntr_by_full_lists_last :
	    (ab->sort_rule == AB_SORT_RULE_FULL) ?
						cmp_cntr_by_full :
	    (ab->sort_rule == AB_SORT_RULE_NICK_LISTS) ?
						cmp_cntr_by_nick_lists_last :
	    /* (ab->sort_rule == AB_SORT_RULE_NICK) */
						cmp_cntr_by_nick);
    }

    if(we_cancel)
      cancel_busy_alarm(0);

    intr_handling_off();

    if(skip_the_sort){
	q_status_message(SM_ORDER, 3, 3,
	    "Address book sort cancelled, using old order for now");
	goto skip_the_write_too;
    }

    dprint(5,
        (debugfile, "- adrbk_sort (%s)\n",
	  ab->sort_rule==AB_SORT_RULE_FULL_LISTS ? "FullListsLast" :
	   ab->sort_rule==AB_SORT_RULE_FULL ? "Fullname" :
	    ab->sort_rule==AB_SORT_RULE_NICK_LISTS ? "NickListLast" :
	     ab->sort_rule==AB_SORT_RULE_NICK ? "Nickname" : "unknown"));

    result = adrbk_write(ab, sort_array, 1, be_quiet, 1);

    if(result == 0){
	exp_free(ab->exp);
	exp_free(ab->selects);
    }
    else if(result == -2)
      q_status_message(SM_ORDER, 3, 4, "address book sort failed, can't save");

    /*
     * Look through the sort_array to find where current_entry_num moved to.
     */
    if(result == 0 && new_entry_num){
	for(i = 0L; i < count; i++)
	  if((adrbk_cntr_t)current_entry_num == sort_array[i]){
	      *new_entry_num = (adrbk_cntr_t)i;
	      break;
	  }
    }

skip_the_write_too:
    fs_give((void **)&sort_array);

    return(result);
}
