/*----------------------------------------------------------------------
  $Id: adrbklib.h 14092 2005-09-27 21:27:55Z hubert@u.washington.edu $

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

#ifndef _ADRBKLIB_INCLUDED
#define _ADRBKLIB_INCLUDED

/*
 *  Some notes:
 *
 * There is considerable indirection going on to get to an address book
 * entry. The idea is that we don't want to have to read in the entire
 * address book (takes too long and uses too much memory) so we have a
 * quick way to get to any entry. There are "count" entries in the addrbook.
 * For each entry, there is an EntryRef element. So there is an
 * array of count EntryRef elements, 0, ..., count-1. These EntryRef elements
 * aren't all kept in memory. Instead, there is a cache of EntryRef
 * elements. The size of that cache is set with adrbk_set_nominal_cachesize().
 * That cache is fronted by a hashtable with the hashvalue just the element
 * number mod size_of_hashtable. The size of the hashtable is just set so
 * that every hash bucket has 10 entries in it, so we have a constant short
 * time for looking for a cache hit. EntryRef entries are prebuilt from
 * the addrbook file and stored in the EntryRef section of the addrbook.lu
 * file. An EntryRef element consists of:
 *      uid_nick, uid_addr  Unique id's computed from nickname and address
 *      offset              Offset into addrbook to start of nickname
 *      next_nick           Next entry for nickname with same hash value
 *      next_addr           Next entry for address with same hash value
 *      ae                  Pointer to cached AdrBk_Entry (not stored on disk)
 * To get an AdrBk_Entry, say for element 13, adrbk_get_entryref(13) is
 * called. That looks in the appropriate hash bucket for 13 and runs
 * through the list looking for a match on element 13. If it doesn't find
 * it, it reads the entry from the addrbook.lu file as above. That entry
 * can be found easily because it is at a calculable offset from the
 * start of the addrbook.lu file. Then adrbk_init_ae() is called to parse
 * the entry and fill in an ae, which is pointed to by the cached EntryRef.
 * That's how you look up a particular entry *number* in the addrbook. That
 * would be used for browsing the addrbook or searching the addrbook.
 *
 * The order that the address book is stored in on disk is the order that
 * the entries will be displayed in. When an address book is opened,
 * if it is ReadWrite the sort order is checked and it is sorted if it is
 * out of order. If an addrbook is already correctly sorted but doesn't
 * have a .lu file yet, the .lu file will be created, then the sort will be
 * checked by going through all N entries. The sort_rule will be stored
 * into the .lu file so that the next time this happens the check can
 * be avoided. That is, once the addrbook is sorted once, all operations
 * on it preserve the sort order. So unless it is changed externally, or
 * the .lu file is removed, or the sort_rule is changed, this check will
 * never have to look at any of the entries again.
 *
 * You also want to be able to lookup an entry based on the nickname. The
 * composer does this all the time by calling build_address(). That needs
 * to be very fast, so is done through another hashtable. This table is
 * also precomputed and stored in the addrbook.lu file. When the addrbook
 * is first opened, that hash table is read into memory. Each entry in the
 * hashtable is a adrbk_cntr_t element number, which points to the head
 * of the list of entries with the same hash value. That list is what
 * uses the next_nick variable in the EntryRef. The hashtable is not very
 * large. Just four (or 2) bytes per entry and size of hashtable entries. The
 * size of the table varies with the size of the addrbook, and is set by
 * the function hashtable_size(). It is also stored in the addrbook.lu
 * file so need not be the value returned by hashtable_size. That hashtable
 * is kept open the whole time Pine is running, once it has been opened. To
 * look up an entry for nickname "nick", you compute the hash value (ab_hash)
 * of "nick" and the uid (ab_uid) of "nick". Look in the hash array at
 * element number hash_value and find a pointer (an index into the EntryRef
 * array). Now you would get that EntryRef just like above, since you
 * are now working with the element number. Check to see if that EntryRef
 * has the same uid as "nick". If so, that's the one you want. If not,
 * next_nick is the index of the next entry with the same hash value.
 * Follow that list checking uid's until you find it or hit the end.
 * There is an entirely analogous hash table for lookup by address instead
 * of lookup by nickname. That only applies to addresses of Single entries,
 * not Lists.
 *
 * Some sizes:
 *
 *  AdrHash is an array of adrbk_cntr_t of length hashtable_size.
 *  This size is usually set by the hashtable_size function.
 *  There are two of those, one for nicknames and one for addresses.
 *  These are kept open the whole time the program is being run (once opened).
 *  Those are also stored on disk in addrbook.lu. They take up more
 *  space there since they are written out in ASCII for portability.
 *
 *  An EntryRef cache element is size 16-20. The size of this cache is usually
 *  kept fairly small (200) but sorting cries out for it to be larger.
 *  When sorting in Unix, it is set equal to the "count" of the addrbook,
 *  smaller for DOS. Each valid EntryRef cache element points to an
 *  EntryRef in memory. Each of those is 20-24 fixed bytes. Part of that is
 *  a pointer to a possible cached AdrBk_Entry. An AdrBk_Entry is 22 bytes
 *  of fixed space, but it also has pointers to variable amounts of memory
 *  for nickname, fullname, addresses, fcc, and comments. A typical
 *  entry probably uses about 50 bytes of that extra space, so altogether,
 *  a cached EntryRef with a fully-filled in ae takes around 100 bytes.
 *  If you had a 30,000 entry addrbook, setting the cache size to "count"
 *  for sorting takes up about 3,000,000 bytes of memory.
 *
 * That's the story for adrbklib.c. There is also some allocation happening
 * in addrbook.c. In particular, the display is a window into an array
 * of rows, at least one row per addrbook entry plus more for lists.
 * Each row is an AddrScrn_Disp structure and those should typically take
 * up 6 or 8 bytes. A cached copy of addrbook entries is not kept, just
 * the element number to look it up (and often get it out of the EntryRef
 * cache). In order to avoid having to allocate all those rows, this is
 * also in the form of a cache. Only 3 * screen_length rows are kept in
 * the cache, and the cache is always a simple interval of rows. That is,
 * rows between valid_low and valid_high are all in the cache. Row numbers
 * in the display list are a little mysterious. There is no origin. That
 * is, you don't necessarily know where the start or end of the display
 * is. You only know how to go forward and backward and how to "warp"
 * to new locations in the display and go forward and backward from there.
 * This is because we don't know how many rows there are all together. It
 * is also a way to avoid searching through everything to get somewhere.
 * If you want to go to the End, you warp there and start backwards instead
 * of reading through all the entries to get there. If you edit an entry
 * so that it sorts into a new location, you warp to that new location to
 * save processing all of the entries in between.
 *
 *
 * Notes about RFC1522 encoding:
 *
 * If the fullname field contains other than US-ASCII characters, it is
 * encoded using the rules of RFC1522 or its successor. The value actually
 * stored in the file is encoded, even if it matches the character set of
 * the user. This is so it is easy to pass the entry around or to change
 * character sets without invalidating entries in the address book. When
 * a fullname is displayed, it is first decoded. If the fullname field is
 * encoded as being from a character set other than the user's character
 * set, that will be retained until the user edits the field. Then it will
 * change over to the user's character set. The comment field works in
 * the same way, as do the "phrase" fields of any addresses. On outgoing
 * mail, the correct character set will be retained if you use ComposeTo
 * from the address book screen. However, if you use a nickname from the
 * composer or ^T from the composer, the character set will be lost if it
 * is different from the user's character set.
 *
 *
 * Notes about RemoteViaImap address books:
 *
 * There are currently two types of address books, Local and Imap. Local means
 * it is in a local file. Imap means it is stored in a folder on a remote
 * IMAP server. The folder is a regular folder containing mail messages but
 * the messages are special. The first message is a header message. The last
 * message is the address book data. In between messages are old versions of
 * the address book data. The address book data is stored in the message as
 * it would be on disk, with no fancy mime encoding or anything. When it is
 * used the data from the last message in the folder is copied to a local file
 * and then it is used just like a local file. The local file is a cache for
 * the remote data. We can tell the remote data has been changed by looking
 * at the Date of the last message in the remote folder. If we see it has
 * changed we copy the whole file over again and replace the cache file.
 * A possibly quicker way to tell if it has changed is if the UID has
 * changed or the number of messages in the folder has changed. We use those
 * methods if possible since they don't require opening a new stream and
 * selecting the folder.  There is one metadata file for address book data.
 * The name of that file is stored in the pinerc file. It contains the names
 * of the cache files for RemoveViaImap address books plus other caching
 * information for those address books (uid...).
 */

#define NFIELDS 11 /* one more than num of data fields in addrbook entry */

/*
 * Disk file takes up more space when HUGE is defined, but we think it is
 * not enough more to offset the convenience of no limits. So it is always
 * defined. Should still work if you want to undefine it.
 */
#define HUGE_ADDRBOOKS

/*
 * The type a_c_arg_t is a little confusing. It's the type we use in
 * place of adrbk_cntr_t when we want to pass an adrbk_cntr_t in an argument.
 * We were running into problems with the integral promotion of adrbk_cntr_t
 * args. A_c_arg_t has to be large enough to hold a promoted adrbk_cntr_t.
 * So, if adrbk_cntr_t is unsigned short, then a_c_arg_t needs to be int if
 * int is larger than short, or unsigned int if short is same size as int.
 * Since usign16_t always fits in a short, a_c_arg_t of unsigned int should
 * always work for !HUGE. For HUGE, usign32_t will be either an unsigned int
 * or an unsigned long. If it is an unsigned long, then a_c_arg_t better be
 * an unsigned long, too. If it is an unsigned int, then a_c_arg_t could
 * be an unsigned int, too. However, if we just make it unsigned long, then
 * it will be the same in all cases and big enough in all cases.
 *
 * In the HUGE case, we could use usign32_t for the a_c_arg_t typedef.
 * There is no actual advantage to be gained, though. The only place it
 * would make a difference is on machines where an int is 32 bits and a
 * long is 64 bits, in which case the 64-bit long is probably the more
 * efficient size to use anyway.
 */

#ifdef HUGE_ADDRBOOKS

typedef usign32_t adrbk_cntr_t;  /* addrbook counter type                */
typedef unsigned long a_c_arg_t;     /* type of arg passed for adrbk_cntr_t  */
#define NO_NEXT ((adrbk_cntr_t)-1)
#define MAX_ADRBK_SIZE (2000000000L) /* leave room for extra display lines   */
#define MAX_HASHTABLE_SIZE 150000

# else /* !HUGE_ADDRBOOKS */

typedef usign16_t adrbk_cntr_t; /* addrbook counter type                */
typedef unsigned a_c_arg_t;          /* type of arg passed for addrbk_cntr_t */
#define NO_NEXT ((adrbk_cntr_t)-1)
#define MAX_ADRBK_SIZE ((long)(NO_NEXT - 2))
#define MAX_HASHTABLE_SIZE 60000

# endif /* !HUGE_ADDRBOOKS */

/*
 * The value NO_NEXT is reserved to mean that there is no next address, or that
 * there is no address number to return. This is similar to getc returning
 * -1 when there is no char to get, but since we've defined this to be
 * unsigned we need to reserve one of the valid values for this purpose.
 * With current implementation it needs to be all 1's, so memset initialization
 * will work correctly.
 */

typedef long adrbk_uid_t;   /* the UID of a name or address */

typedef enum {NotSet, Single, List} Tag;
typedef enum {Normal, Internal, Delete, SaveDelete, Lock, Unlock} Handling;

/* This is what is actually used by the routines that manipulate things */
typedef struct adrbk_entry {
    char *nickname;
    char *fullname;    /* of simple addr or list                        */
    union addr {
        char *addr;    /* for simple Single entries                     */
        char **list;   /* for distribution lists                        */
    } addr;
    char *fcc;         /* fcc specific for when sending to this address */
    char *extra;       /* comments field                                */
    char  referenced;  /* for detecting loops during lookup             */
    Tag   tag;         /* single addr (Single) or a list (List)         */
} AdrBk_Entry;

/*
 * This points to the data in a file. It gives us a smaller way to store
 * data and a fast way to lookup a particular entry. When we need the
 * actual data we look in the file and produce an AdrBk_Entry.
 */
typedef struct entry_ref {
    adrbk_uid_t  uid_nick;  /* uid(nickname) */
    adrbk_uid_t  uid_addr;  /* uid(address) */
    long         offset;    /* offset into file where this entry starts    */
    adrbk_cntr_t next_nick; /* index of next nickname with same hash value */
    adrbk_cntr_t next_addr;
    int          is_deleted;
    AdrBk_Entry *ae;        /* cached ae */
} EntryRef;

typedef struct er_cache_elem {
    EntryRef *entry;            /* the cached entryref */
    struct er_cache_elem *next; /* pointers to other cached entryrefs */
    struct er_cache_elem *prev;
    adrbk_cntr_t elem;          /* the index in the entryref array of entry */
    Handling handling;          /* handling instructions */
    unsigned char lock_ref_count; /* when reaches zero, it's unlocked */
}ER_CACHE_ELEM_S;

/*
 * hash(nickname) -> index into harray
 * harray(index) is an index into an array of EntryRef's.
 */
typedef struct adrhash {
    adrbk_cntr_t *harray;   /* the hash array, alloc'd (hash(name) points
				   into this array and the value in this
				   array is an index into the EntryRef array */
} AdrHash;

/* information useful for displaying the addrbook */
typedef struct width_stuff {
    int max_nickname_width;
    int max_fullname_width;
    int max_addrfield_width;
    int max_fccfield_width;
    int third_biggest_fullname_width;
    int third_biggest_addrfield_width;
    int third_biggest_fccfield_width;
} WIDTH_INFO_S;

typedef struct expanded_list {
    adrbk_cntr_t          ent;
    struct expanded_list *next;
} EXPANDED_S;

typedef enum {Local, Imap} AdrbkType;


#ifndef IMAP_IDLE_TIMEOUT
#define IMAP_IDLE_TIMEOUT	(10L * 60L)	/* seconds */
#endif
#ifndef FILE_VALID_CHK_INTERVAL
#define FILE_VALID_CHK_INTERVAL (      15L)	/* seconds */
#endif
#ifndef LOW_FREQ_CHK_INTERVAL
#define LOW_FREQ_CHK_INTERVAL	(240)		/* minutes */
#endif
#ifndef SORTING_LOOP_AVOID_TIMEOUT
#define SORTING_LOOP_AVOID_TIMEOUT (3L * 60L * 60L) /* seconds */
#endif

typedef struct adrbk {
    AdrbkType	  type;                /* type of address book               */
    char         *orig_filename;       /* passed in filename                 */
    char         *filename;            /* addrbook filename                  */
    char         *our_filecopy;        /* session copy of filename contents  */
    char         *hashfile;            /* lookup index file (.lu file)       */
    char         *our_hashcopy;        /* session copy of hashfile contents  */
    FILE         *fp;                  /* fp for our_filecopy                */
    FILE         *fp_hash;             /* fp for our_hashcopy                */
    AdrHash      *hash_by_nick;
    AdrHash      *hash_by_addr;
    AccessType    hashfile_access;     /* access permission for hashfile     */
    adrbk_cntr_t  htable_size;         /* how many entries in AdrHash tables */
    adrbk_cntr_t  count;               /* how many entries in addrbook       */
    adrbk_cntr_t  phantom_count;       /* how many not counting appended_aes */
    long          deleted_cnt;         /* how many #DELETED entries in abook */
    adrbk_cntr_t  orig_count;          /* how many last time we wrote        */
    time_t        last_change_we_know_about;/* to look for others changing it*/
    time_t        last_local_valid_chk;/* when valid check was done          */
    ER_CACHE_ELEM_S **head_cache_elem; /* array of cache elem heads          */
    ER_CACHE_ELEM_S **tail_cache_elem; /*   and tails                        */
    int          *n_ae_cached_in_this_bucket; /* and n cached in each bucket */
    long          nominal_max_cached;
    adrbk_cntr_t  er_hashsize;
    unsigned      flags;               /* see defines in pine.h (DEL_FILE...)*/
    WIDTH_INFO_S  widths;              /* helps addrbook.c format columns    */
    int           sort_rule;
    EXPANDED_S   *exp;                 /* this is for addrbook.c to use.  A
			       list of expanded list entry nums is kept here */
    EXPANDED_S   *checks;              /* this is for addrbook.c to use.  A
			       list of checked entry nums is kept here */
    EXPANDED_S   *selects;             /* this is for addrbook.c to use.  A
			       list of selected entry nums is kept here */
    REMDATA_S    *rd;
} AdrBk;


/*
 * The definitions starting here have to do with the virtual scrolling
 * region view into the addressbooks. That is, the display.
 *
 * We could make every use of an AdrBk_Entry go through a function call
 * like adrbk_get_ae(). Instead, we try to be smart and avoid the extra
 * function calls by knowing when the addrbook entry is still valid, either
 * because we haven't called any functions that could invalidate it or because
 * we have locked it in the cache. If we do lock it, we need to be careful
 * that it eventually gets unlocked. That can be done by an explicit
 * adrbk_get_ae(Unlock) call, or it is done implicitly when the address book
 * is written out. The reason it can get invalidated is that the abe that
 * we get returned to us is just a pointer to a cached addrbook entry, and
 * that entry can be flushed from the cache by other addrbook activity.
 * So we need to be careful to make sure the abe is certain to be valid
 * before using it.
 *
 * Data structures for the display of the address book. There's one
 * of these structures per line on the screen.
 *
 * Types: Title -- The title line for the different address books. It has
 *		   a ptr to the text of the Title line.
 * ClickHereCmb -- This is the line that says to click here to
 *                 expand.  It changes types into the individual expanded
 *                 components once it is expanded.  It doesn't have any data
 *                 other than an implicit title. This is only used with the
 *                 combined-style addrbook display.
 * ListClickHere --This is the line that says to click here to
 *                 expand the members of a distribution list. It changes
 *                 types into the individual expanded ListEnt's (if any)
 *                 when it is expanded. It has a ptr to an AdrBk_Entry.
 *    ListEmpty -- Line that says this is an empty distribution list. No data.
 *        Empty -- Line that says this is an empty addressbook. No data.
 *    ZoomEmpty -- Line that says no addrs in zoomed view. No data.
 * AddFirstGLob -- Place holder for adding an abook. No data.
 * AddFirstPers -- Place holder for adding an abook. No data.
 *   DirServers -- Place holder for accessing directory servers. No data.
 *       Simple -- A single addressbook entry. It has a ptr to an AdrBk_Entry.
 *                 When it is displayed, the fields are usually:
 *                 <nickname>       <fullname>       <address or another nic>
 *     ListHead -- The head of an address list. This has a ptr to an
 *		   AdrBk_Entry.
 *                 <blank line> followed by
 *                 <nickname>       <fullname>       "DISTRIBUTION LIST:"
 *      ListEnt -- The rest of an address list. It has a pointer to its
 *		   ListHead element and a ptr (other) to this specific address
 *		   (not a ptr to another AdrBk_Entry).
 *                 <blank>          <blank>          <address or another nic>
 *         Text -- A ptr to text. For example, the ----- lines and
 *		   whitespace lines.
 *    NoAbooks  -- There are no address books at all.
 *   Beginnning -- The (imaginary) elements before the first real element
 *          End -- The (imaginary) elements after the last real element
 */
typedef enum {DlNotSet, Empty, ZoomEmpty, AddFirstPers, AddFirstGlob,
	      AskServer, Title, Simple, ListHead, ListClickHere,
	      ListEmpty, ListEnt, Text, Beginning, End, NoAbooks,
	      ClickHereCmb, TitleCmb} LineType;
/* each line of address book display is one of these structures */
typedef struct addrscrn_disp {
    union {
        struct {
            adrbk_cntr_t  ab_element_number; /* which addrbook entry     */
	    adrbk_cntr_t  ab_list_offset;    /* which member of the list */
        }addrbook_entry;
        char        *text_ptr;
    }union_to_save_space;
    LineType       type;
} AddrScrn_Disp;
#define txt union_to_save_space.text_ptr
#define elnum union_to_save_space.addrbook_entry.ab_element_number
#define l_offset union_to_save_space.addrbook_entry.ab_list_offset

#define entry_is_checked    exp_is_expanded
#define entry_get_next      exp_get_next
#define entry_set_checked   exp_set_expanded
#define entry_unset_checked exp_unset_expanded
#define any_checked         exp_any_expanded
#define howmany_checked     exp_howmany_expanded

#define entry_is_selected   exp_is_expanded
#define entry_set_selected  exp_set_expanded
#define entry_unset_selected exp_unset_expanded
#define any_selected        exp_any_expanded
#define howmany_selected    exp_howmany_expanded

#define entry_is_deleted    exp_is_expanded
#define entry_set_deleted   exp_set_expanded
#define howmany_deleted     exp_howmany_expanded

#define entry_is_added      exp_is_expanded
#define entry_set_added     exp_set_expanded

/*
 * Argument to expand_address and build_address_internal is a BuildTo,
 * which is either a char * address or an AdrBk_Entry * (if we have already
 * looked it up in an addrbook).
 */
typedef enum {Str, Abe} Build_To_Arg_Type;
typedef struct build_to {
    Build_To_Arg_Type type;
    union {
	char        *str;  /* normal looking address string */
	AdrBk_Entry *abe;  /* addrbook entry */
    }arg;
} BuildTo;


/* Display lines used up by each top-level addrbook, counting blanks */
#define LINES_PER_ABOOK (3)
/* How many of those lines are visible (not blank) */
#define VIS_LINES_PER_ABOOK (2)
/* How many extra lines are between the personal and global sections */
#define XTRA_LINES_BETWEEN (1)
/* How many lines does the PerAdd line take, counting blank line */
#define LINES_PER_ADD_LINE (2)
/* Extra title lines above first entry that are shown when the combined-style
   display is turned on. */
#define XTRA_TITLE_LINES_IN_OLD_ABOOK_DISP (4)

typedef enum {DlcNotSet,
	      DlcPersAdd,		/* config screen only */
	      DlcGlobAdd,		/*    "      "    "   */

	      DlcTitle,			/* top level displays */
	      DlcTitleNoPerm,		/*  "    "      "     */
	      DlcSubTitle,		/*  "    "      "     */
	      DlcTitleBlankTop,		/*  "    "      "     */
	      DlcGlobDelim1,		/*  "    "      "     */
	      DlcGlobDelim2,		/*  "    "      "     */
	      DlcDirDelim1,		/*  "    "      "     */
	      DlcDirDelim2,		/*  "    "      "     */
	      DlcDirAccess,		/*  "    "      "     */
	      DlcDirSubTitle,		/*  "    "      "     */
	      DlcDirBlankTop,		/*  "    "      "     */

	      DlcTitleDashTopCmb,	/* combined-style top level display */
	      DlcTitleCmb,		/*     "      "    "    "      "    */
	      DlcTitleDashBottomCmb,	/*     "      "    "    "      "    */
	      DlcTitleBlankBottomCmb,	/*     "      "    "    "      "    */
	      DlcClickHereCmb,		/*     "      "    "    "      "    */
	      DlcTitleBlankTopCmb,	/*     "      "    "    "      "    */
	      DlcDirDelim1a,		/*     "      "    "    "      "    */
	      DlcDirDelim1b,		/*     "      "    "    "      "    */
	      DlcDirDelim1c,		/*     "      "    "    "      "    */

	      DlcEmpty,			/* display of a single address book */
	      DlcZoomEmpty,		/*    "                             */
	      DlcNoPermission,		/*    "                             */
	      DlcSimple,		/*    "                             */
	      DlcListHead,		/*    "                             */
	      DlcListClickHere,		/*    "                             */
	      DlcListEmpty,		/*    "                             */
	      DlcListEnt,		/*    "                             */
	      DlcListBlankTop,		/*    "                             */
	      DlcListBlankBottom,	/*    "                             */
	      DlcNoAbooks,		/*    "                             */

	      DlcOneBeforeBeginning,	/* used in both */
	      DlcTwoBeforeBeginning,	/*  "   "   "   */
	      DlcBeginning,		/*  "   "   "   */
	      DlcEnd} DlCacheType;

typedef enum {Initialize, FirstEntry, LastEntry, ArbitraryStartingPoint,
	      DoneWithCache, FlushDlcFromCache, Lookup} DlMgrOps;
typedef enum {Warp, DontWarp} HyperType;
/*
 * The DlCacheTypes are the types that a dlcache element can be labeled.
 * The idea is that there needs to be enough information in the single
 * cache element by itself so that you can figure out what the next and
 * previous dl rows are by just knowing this one row.
 *
 * In the top-level display, there are DlcTitle lines or DlcTitleNoPerm
 * lines, which are the same except we know that we can't access the
 * address book in the latter case. DlcSubTitle lines follow each of the
 * types of Title lines, and Titles within a section are separated by
 * DlcTitleBlankTop lines, which belong to (have the same adrbk_num as)
 * the Title they are above.
 * If there are no address books and no directory servers defined, we
 * have a DlcNoAbooks line. When we are displaying an individual address
 * book (not in the top-level display) there is another set of types. An
 * empty address book consists of one line of type DlcEmpty. An address
 * book without read permission is a DlcNoPermission. Simple address book
 * entries consist of a single DlcSimple line. Lists begin with a
 * DlcListHead. If the list is not expanded the DlcListHead is followed by
 * a DlcListClickHere. If it is known to be a list with no members the
 * DlcListHead is followed by a DlcListEmpty. If there are members and
 * the list is expanded, each list member is a single line of type
 * DlcListEnt. Two lists are separated by a DlcListBlankBottom belonging
 * to the first list. A list followed or preceded by a DlcSimple address
 * row has a DlcListBlank(Top or Bottom) separating it from the
 * DlcSimple. Above the top row of the display is an imaginary line of
 * type DlcOneBeforeBeginning. Before that is a DlcTwoBeforeBeginning. And
 * before that all the lines are just DlcBeginning lines. After the last
 * display line is a DlcEnd.
 *
 * The DlcDirAccess's are indexed by adrbk_num (re-used for this).
 * Adrbk_num -1 means access all of the servers.
 * Adrbk_num 0 ... n_serv -1 means access all a particular server.
 * Adrbk_num n_serv means access as if from composer using config setup.
 *
 * Here are the types of lines and where they fall in the top-level display:
 *
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcTwoBeforeBeginning
 * (not a visible line)			DlcOneBeforeBeginning
 *     Title				DlcTitle (or TitleNoPerm)
 *       Subtitle			DlcSubTitle
 * ---this is blank----------------	DlcTitleBlankTop
 *     Title				DlcTitle (or TitleNoPerm)
 *       Subtitle			DlcSubTitle
 * ---this is blank----------------	DlcGlobDelim1
 * ---this is blank----------------	DlcGlobDelim2
 *     Title				DlcTitle (or TitleNoPerm)
 *       Subtitle			DlcSubTitle
 * ---this is blank----------------	DlcTitleBlankTop
 *     Title				DlcTitle (or TitleNoPerm)
 *       Subtitle			DlcSubTitle
 * ---this is blank----------------	DlcDirDelim1
 * ---this is blank----------------	DlcDirDelim2
 *     Directory (query server 1)	DlcDirAccess (adrbk_num 0)
 *       Subtitle 			DlcDirSubTitle (adrbk_num 0)
 * ---this is blank----------------	DlcDirBlankTop
 *     Directory (query server 2)	DlcDirAccess (adrbk_num 1)
 *       Subtitle 			DlcDirSubTitle (adrbk_num 1)
 * (not a visible line)			DlcEnd
 * (not a visible line)			DlcEnd
 *
 *
 * There is a combined-style display triggered by the F_CMBND_ABOOK_DISP
 * feature. It's a mixture of the top-level and open addrbook displays. When an
 * addrbook is opened the rest of the addrbooks don't disappear from the
 * screen. In this view, the ClickHere lines can be replaced with the entire
 * contents of the addrbook, but the other stuff remains on the screen, too.
 * Here are the types of lines and where they fall in the
 * combined-style display:
 *
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcTwoBeforeBeginning
 * (not a visible line)			DlcOneBeforeBeginning
 * --------------------------------	DlcTitleDashTopOld
 *     Title				DlcTitleOld
 * --------------------------------	DlcTitleDashBottomOld
 * ---this is blank----------------	DlcTitleBlankBottom
 *     ClickHere			DlcClickHereOld
 * ---this is blank----------------	DlcTitleBlankTop
 * --------------------------------	DlcTitleDashTopOld
 *     Title				DlcTitleOld
 * --------------------------------	DlcTitleDashBottomOld
 * ---this is blank----------------	DlcTitleBlankBottom
 *     ClickHere			DlcClickHereOld
 * ---this is blank----------------	DlcDirDelim1
 * --------------------------------	DlcDirDelim1a
 * Directories                      	DlcDirDelim1b
 * --------------------------------	DlcDirDelim1c
 * ---this is blank----------------	DlcDirDelim2
 *     Directory (query server 1)	DlcDirAccess (adrbk_num 0)
 *       Subtitle 			DlcDirSubTitle (adrbk_num 0)
 * ---this is blank----------------	DlcDirBlankTop
 *     Directory (query server 2)	DlcDirAccess (adrbk_num 1)
 *       Subtitle 			DlcDirSubTitle (adrbk_num 1)
 * (not a visible line)			DlcEnd
 * (not a visible line)			DlcEnd
 *
 * If there are no addrbooks in either of the two sections, or no Directory
 * servers, then that section is left out of the display. If there is only
 * one address book and no Directories, then the user goes directly into the
 * single addressbook view which looks like:
 *
 * if(no entries in addrbook)
 *
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcTwoBeforeBeginning
 * (not a visible line)			DlcOneBeforeBeginning
 *     Empty or NoPerm or NoAbooks	DlcEmpty, DlcZoomEmpty, DlcNoPermission,
 *					or DlcNoAbooks
 * (not a visible line)			DlcEnd
 * (not a visible line)			DlcEnd
 *
 * else
 *
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcBeginning
 * (not a visible line)			DlcTwoBeforeBeginning
 * (not a visible line)			DlcOneBeforeBeginning
 *     Simple Entry			DlcSimple
 *     Simple Entry			DlcSimple
 *     Simple Entry			DlcSimple
 *					DlcListBlankTop
 *     List Header			DlcListHead
 *          Unexpanded List		DlcListClickHere
 *          or
 *          Empty List			DlcListEmpty
 *          or
 *          List Entry 1		DlcListEnt
 *          List Entry 2		DlcListEnt
 *					DlcListBlankBottom
 *     List Header			DlcListHead
 *          List Entry 1		DlcListEnt
 *          List Entry 2		DlcListEnt
 *          List Entry 3		DlcListEnt
 *					DlcListBlankBottom
 *     Simple Entry			DlcSimple
 *					DlcListBlankTop
 *     List Header			DlcListHead
 *          Unexpanded List		DlcListClickHere
 * (not a visible line)			DlcEnd
 * (not a visible line)			DlcEnd
 *
 * The config screen view is similar to the top-level view except there
 * is no directory section (it has it's own config screen) and if there
 * are zero personal addrbooks or zero global addrbooks then a placeholder
 * line of type DlcPersAdd or DlcGlobAdd takes the place of the DlcTitle
 * line.
 */
typedef struct dl_cache {
    long         global_row; /* disp_list row number */
    adrbk_cntr_t dlcelnum;   /* which elnum from that addrbook */
    adrbk_cntr_t dlcoffset;  /* offset in a list, only for ListEnt rows */
    short        adrbk_num;  /* which address book we're related to */
    DlCacheType  type;       /* type of this row */
    AddrScrn_Disp dl;	     /* the actual dl that goes with this row */
} DL_CACHE_S;

typedef enum {Nickname, Fullname, Addr, Filecopy, Comment, Notused,
	      Def, WhenNoAddrDisplayed, Checkbox, Selected} ColumnType;
/*
 * Users can customize the addrbook display, so this tells us which data
 * is in a particular column and how wide the column is. There is an
 * array of these per addrbook, of length NFIELDS (number of possible cols).
 */
typedef struct column_description {
    ColumnType type;
    WidthType  wtype;
    int        req_width; /* requested width (for fixed and percent types) */
    int        width;     /* actual width to use */
    int        old_width;
} COL_S;

/* address book attributes for peraddrbook type */
#define GLOBAL		0x1	/* else it is personal */
#define REMOTE_VIA_IMAP	0x2	/* else it is a local file  */

typedef enum {TotallyClosed, /* hash tables not even set up yet               */
	      Closed,     /* data not read in, no display list                */
	      NoDisplay,  /* data is accessible, no display list              */
	      HalfOpen,   /* data not accessible, initial display list is set */
	      ThreeQuartOpen, /* like HalfOpen without partial_close          */
	      Open        /* data is accessible and display list is set       */
	     } OpenStatus;
/*
 * There is one of these per addressbook.
 */
typedef struct peraddrbook {
    unsigned        	type;
    AccessType          access;
    OpenStatus          ostatus;
    char               *nickname,
		       *filename;
    AdrBk              *address_book;        /* the address book handle */
    int                 gave_parse_warnings;
    time_t              when_we_sorted;
    COL_S               disp_form[NFIELDS];  /* display format */
    int			nick_is_displayed;   /* these are for convenient, */
    int			full_is_displayed;   /* fast access.  Could get   */
    int			addr_is_displayed;   /* same info from disp_form. */
    int			fcc_is_displayed;
    int			comment_is_displayed;
    STORE_S	       *so;                  /* storage obj for addrbook
						temporarily stored here   */
} PerAddrBook;

/*
 * This keeps track of the state of the screen and information about all
 * the address books. We usually only have one of these but sometimes
 * we save a version of this state (with save_state) and re-call the
 * address book functions. Then when we pop back up to where we were
 * (with restore_state) the screen and the state of the address books
 * is restored to what it was.
 */
typedef struct addrscreenstate {
    PerAddrBook   *adrbks;       /* array of addrbooks                    */
    int		   initialized,  /* have we done at least simple init?    */
                   n_addrbk,     /* how many addrbooks are there          */
                   how_many_personals, /* how many of those are personal? */
                   cur,          /* current addrbook                      */
                   cur_row,      /* currently selected line               */
                   old_cur_row,  /* previously selected line              */
                   l_p_page;	 /* lines per (screen) page               */
    long           top_ent;      /* index in disp_list of top entry on screen */
    int            ro_warning,   /* whether or not to give warning        */
                   checkboxes,   /* whether or not to display checkboxes  */
                   selections,   /* whether or not to display selections  */
                   do_bold,      /* display selections in bold            */
                   no_op_possbl, /* user can't do anything with current conf */
                   zoomed,       /* zoomed into view only selected entries */
                   config,       /* called from config screen             */
                   n_serv,       /* how many directory servers are there  */
                   n_impl,       /* how many of those have impl bit set   */
                   selected_is_history;
#ifdef	_WINDOWS
    long	   last_ent;	 /* index of last known entry		  */
#endif
} AddrScrState;

/*
 * AddrBookScreen and AddrBookConfig are the maintenance screens, all the
 * others are selection screens. The AddrBookConfig screen is an entry
 * point from the Setup/Addressbooks command in the main menu. Those that
 * end in Com are called from the pico HeaderEditor, either while in the
 * composer or while editing an address book entry.  SelectManyNicks
 * returns a comma-separated list of nicknames. SelectAddrLccCom and
 * SelectNicksCom return a comma-separated list of nicknames.
 * SelectNickTake, SelectNickCom, and SelectNick all return a single
 * nickname.  The ones that returns multiple nicknames or multiple
 * addresses all allow ListMode. They are SelectAddrLccCom,
 * SelectNicksCom, and SelectMultNoFull.
 */
typedef enum {AddrBookScreen,	   /* maintenance screen                     */
	      AddrBookConfig,	   /* config screen                          */
	      SelectAddrLccCom,	   /* returns list of nicknames of lists     */
	      SelectNicksCom,	   /* just like SelectAddrLccCom, but allows
				      selecting simple *and* list entries    */
	      SelectNick,	   /* returns single nickname                */
	      SelectNickTake,	   /* Same as SelectNick but different help  */
	      SelectNickCom,	   /* Same as SelectNick but from composer   */
	      SelectManyNicks,	   /* Returns list of nicks */
	      SelectAddr,	   /* Returns single address */
	      SelectAddrNoFull,	   /* Returns single address without fullname */
	      SelectMultNoFull	   /* Returns mult addresses without fullname */
	     } AddrBookArg;

typedef struct save_state_struct {
    AddrScrState *savep;
    OpenStatus   *stp;
    DL_CACHE_S   *dlc_to_warp_to;
} SAVE_STATE_S;

typedef struct act_list {
    PerAddrBook *pab;
    adrbk_cntr_t num,
		 num_in_dst;
    unsigned int skip:1,
		 dup:1;
} ACTION_LIST_S;

typedef struct ta_abook_state {
    PerAddrBook  *pab;
    SAVE_STATE_S  state;
} TA_STATE_S;


/*
 * Many of these should really only have a single value but we give them
 * an array for uniformity.
 */
typedef struct _vcard_info {
    char **nickname;
    char **fullname;
    char  *first;
    char  *middle;
    char  *last;
    char **fcc;
    char **note;
    char **title;
    char **tel;
    char **email;
} VCARD_INFO_S;


extern AddrScrState as;
extern jmp_buf      addrbook_changed_unexpectedly;
extern char        *trouble_filename;
extern long         msgno_for_pico_callback;
extern BODY        *body_for_pico_callback;
extern ENVELOPE    *env_for_pico_callback;
extern int          ab_nesting_level;


#define DING    1
#define NO_DING 0

/*
 * These constants are supposed to be suitable for use as longs where the longs
 * are representing a line number or message number.
 * These constants aren't suitable for use with type adrbk_cntr_t. There is
 * a constant called NO_NEXT which you probably want for that.
 */
#define NO_LINE         (2147483645L)
#define CHANGED_CURRENT (NO_LINE + 1L)

/*
 * The do-while stuff is so these are statements and can be written with
 * a following ; like a regular statement without worrying about braces and all.
 */
#define SKIP_SPACE(p) do{while(*p && *p == SPACE)p++;}while(0)
#define SKIP_TO_TAB(p) do{while(*p && *p != TAB)p++;}while(0)
#define RM_END_SPACE(start,end)                                             \
	    do{char *_ptr = end;                                            \
	       while(--_ptr >= start && *_ptr == SPACE)*_ptr = '\0';}while(0)
#define REPLACE_NEWLINES_WITH_SPACE(p)                                      \
		do{register char *_qq;                                      \
		   for(_qq = p; *_qq; _qq++)                                \
		       if(*_qq == '\n' || *_qq == '\r')                     \
			   *_qq = SPACE;}while(0)
#define NO_UID  ((adrbk_uid_t)0)
#define DEFAULT_HTABLE_SIZE 100
#define MAX_CHARS_IN_HASH 75
#define DELETED "#DELETED-"
#define DELETED_LEN 9


#define ONE_HUNDRED_DAYS (60L * 60L * 24L * 100L)
/*
 * When address book entries are deleted, they are left in the file
 * with the nickname prepended with a string like #DELETED-96/01/25#, 
 * which stands for year 96, month 1, day 25 of the month. When one of
 * these entries is more than ABOOK_DELETED_EXPIRE_TIME seconds old,
 * then it will be totally removed from the address book the next time
 * an adrbk_write() is done. This is for emergencies where somebody
 * deletes something from their address book and would like to get it
 * back. You get it back by editing the nickname field manually to remove
 * the extra 18 characters off the front.
 */
#ifndef ABOOK_DELETED_EXPIRE_TIME
#define ABOOK_DELETED_EXPIRE_TIME   ONE_HUNDRED_DAYS
#endif

#ifdef	ENABLE_LDAP
typedef struct _cust_filt {
    char *filt;
    int   combine;
} CUSTOM_FILT_S;

#define RUN_LDAP	"LDAP: "
#define LEN_RL		6
#define QRUN_LDAP	"\"LDAP: "
#define LEN_QRL		7
#define LDAP_DISP	"[ LDAP Lookup ]"
#endif


/*
 * There are no restrictions on the length of any of the fields, except that
 * there are some restrictions in the current input routines.
 * There can be no more than 65534 entries (unless HUGE) in a single addrbook.
 */

/*
 * The on-disk address book has entries that look like:
 *
 * Nickname TAB Fullname TAB Address_Field TAB Fcc TAB Comment
 *
 * An entry may be broken over more than one line but only at certain
 * spots. A continuation line starts with spaces (spaces, not white space).
 * One place a line break can occur is after any of the TABs. The other
 * place is in the middle of a list of addresses, between addresses.
 * The Address_Field may be either a simple address without the fullname
 * or brackets, or it may be an address list. An address list is
 * distinguished by the fact that it begins with "(" and ends with ")".
 * Addresses within a list are comma separated and each address in the list
 * may be a full rfc822 address, including Fullname and so on.
 *
 * Examples:
 * fred TAB Flintstone, Fred TAB fred@bedrock.net TAB fcc-flintstone TAB comment
 * or
 * fred TAB Flintstone, Fred TAB \n
 *    fred@bedrock.net TAB fcc-flintstone TAB \n
 *    comment
 * somelist TAB Some List TAB (fred, \n
 *    Barney Rubble <barney@bedrock.net>, wilma@bedrock.net) TAB \n
 *    fcc-for-some-list TAB comment
 *
 * There is also an on-disk file (called hashfile in the structure)
 * which is useful for quick access to particular entries. It has the
 * form:
 *                Header
 *                EntryRef Array
 *                HashTable by Nickname
 *                HashTable by Address
 *                Trailer
 *
 * This file is written in ASCII so that it will be portable to multiple
 * clients. It is named addrbook.lu, where addrbook is the name of the
 * addrbook file. lu stands for LookUp.
 *
 *       Header -- magic number "P # * E @"
 *            <SPACE> two character version string
 *            <SPACE> hash table size "\n" integer padded on left with spaces
 * ifdef HUGE_ADDRBOOKS
 *	It takes up 10 character slots
 * else
 *	Value restricted to take up 5 character slots
 *
 * EntryRef Arr -- An array of N OnDiskEntryRef's, where N is the number
 *                     of entries in the address book. Each entry is followed
 *                     by "\n".
 *
 *    HashTables -- Arrays of hash table size integers, each of which is
 *                     padded on left with spaces so it takes up 5 character
 *                     (10 if HUGE_ADDRBOOKS)
 *                     slots. Each integer ends with a "\n" to make it
 *                     easier to use debugging tools.
 *    and another one of those for by address hash table
 *
 *      Trailer -- magic number "P # * E @"
 *            <SPACE> N  "\n" the same N as the number of EntryRef entries
 *        (N is 10 wide if HUGE_ADDRBOOKS, 5 otherwise)
 *           DELETED_CNT "\n"  (11 chars wide, count of #DELETED lines)
 *           W1 W2 W3 W4 W5 W6 W7 "\n"   These W's are widths for helping
 *                             to make a nice display of the addrbook. Each
 *                             consists of a SPACE followed by a one or two
 *                             digit number.
 *           time created        10 bytes, seconds since Jan 1 70
 *           <SPACE> sort_rule "\n"  Sort rule used to sort last time
 *                                    (from AB_SORT... in pine.h)
 *                                    2 bytes, decimal number.
 *                    W1 = max_nickname_width
 *                    W2 = max_fullname_width
 *                    W3 = max_addrfield_width
 *                    W4 = max_fccfield_width
 *                    W5 = third_biggest_fullname_width
 *                    W6 = third_biggest_addrfield_width
 *                    W7 = third_biggest_fccfield_width
 *
 * Each OnDiskEntryRef looks like:
 *      next_nick  -- 5 bytes, positive integer padded on left with spaces.
 *      next_addr  -- 5 bytes, positive integer padded on left with spaces.
 *                   (each of those is 10 with HUGE_ADDRBOOKS)
 *      uid_nick   -- 11 bytes, integer padded on left with spaces.
 *      uid_addr   -- 11 bytes, integer padded on left with spaces.
 *      offset     -- 10 bytes, positive integer padded on left with spaces.
 * Each of these is separated by an extra SPACE, as well, so there are
 * 47 bytes per array element (counting the newline).
 */

/* Some of the definitions (e.g., PMAGIC) are in pine.h. */
#define ADRHASH_FILE_SUFFIX      ".lu"
#ifdef HUGE_ADDRBOOKS
#define ADRHASH_FILE_VERSION_NUM "14"
#else
#define ADRHASH_FILE_VERSION_NUM "13"
#endif
#define LEGACY_PMAGIC            "P#*@ "  /* sorry about that */

#define SIZEOF_NEWLINE      (1)
#define SIZEOF_SORT_RULE    (2)
#define SIZEOF_WIDTH        (2)
#define SIZEOF_ASCII_USHORT (5)
#define SIZEOF_ASCII_ULONG  (10)
#define SIZEOF_ASCII_LONG   (11)  /* because of possible "-" sign */
#define SIZEOF_UID          SIZEOF_ASCII_LONG
#define SIZEOF_FILEOFFSET   SIZEOF_ASCII_ULONG
#define SIZEOF_TIMESTAMP    SIZEOF_ASCII_ULONG
#define SIZEOF_DELETED_CNT  SIZEOF_ASCII_LONG

#ifdef HUGE_ADDRBOOKS

#define SIZEOF_HASH_SIZE    SIZEOF_ASCII_ULONG
#define SIZEOF_HASH_INDEX   SIZEOF_ASCII_ULONG

#else /* !HUGE_ADDRBOOKS */

#define SIZEOF_HASH_SIZE    SIZEOF_ASCII_USHORT
#define SIZEOF_HASH_INDEX   SIZEOF_ASCII_USHORT

#endif /* !HUGE_ADDRBOOKS */

#define SIZEOF_COUNT        SIZEOF_HASH_INDEX

#define TO_FIND_HTABLE_SIZE (TO_FIND_VERSION_NUM + \
			     SIZEOF_VERSION_NUM + SIZEOF_SPACE)
#define SIZEOF_HDR (TO_FIND_HTABLE_SIZE + \
		    SIZEOF_HASH_SIZE + SIZEOF_NEWLINE)

#define TO_FIND_SORT_RULE (-(SIZEOF_SORT_RULE + SIZEOF_NEWLINE))
#define TO_FIND_TIMESTAMP (TO_FIND_SORT_RULE + \
                        (-(SIZEOF_TIMESTAMP + SIZEOF_SPACE)))
#define TO_FIND_WIDTHS (TO_FIND_TIMESTAMP + \
		    (-(7 * (SIZEOF_SPACE + SIZEOF_WIDTH) + SIZEOF_NEWLINE)))
#define TO_FIND_DELETED_CNT (TO_FIND_WIDTHS + \
			(-(SIZEOF_DELETED_CNT + SIZEOF_NEWLINE)))
#define TO_FIND_COUNT (TO_FIND_DELETED_CNT + \
			(-(SIZEOF_COUNT + SIZEOF_NEWLINE)))
#define TO_FIND_TRLR_PMAGIC (TO_FIND_COUNT + \
			(-(SIZEOF_PMAGIC + SIZEOF_SPACE)))
#define SIZEOF_TRLR (-(TO_FIND_TRLR_PMAGIC))

#define SIZEOF_ENTRYREF_ENTRY (SIZEOF_HASH_INDEX + SIZEOF_SPACE + \
			       SIZEOF_HASH_INDEX + SIZEOF_SPACE + \
			       SIZEOF_UID        + SIZEOF_SPACE + \
			       SIZEOF_UID        + SIZEOF_SPACE + \
			       SIZEOF_FILEOFFSET + SIZEOF_NEWLINE)
#define SIZEOF_HTABLE_ENTRY (SIZEOF_HASH_INDEX + SIZEOF_NEWLINE)


/*
 * Prototypes
 */
int            ab_add_abook PROTO((int, int));
int            ab_compose_to_addr PROTO((long, int, int));
int            ab_save PROTO((struct pine *, AdrBk *, long, int, int));
int            ab_del_abook PROTO((long, int, char **));
int            ab_agg_delete PROTO((struct pine *, int));
int            ab_edit_abook PROTO((int, int, char *, char *, char *));
int            ab_forward PROTO((struct pine *, long, int));
int            ab_print PROTO((int));
int            ab_shuffle PROTO((PerAddrBook *, int *, int, char **));
char          *abe_to_nick_or_addr_string PROTO((AdrBk_Entry *,
						 AddrScrn_Disp *, int));
char          *addr_book_change_list PROTO((char **));
int            adrbk_check_and_fix PROTO((PerAddrBook *, int, int, int));
int            adrbk_check_and_fix_all PROTO((int, int, int));
void           adrbk_check_validity PROTO((AdrBk *, long));
int            adrbk_check_all_validity_now PROTO((void));
char          *addr_book_nick_for_edit PROTO((char **));
int            adrbk_write PROTO((AdrBk *, adrbk_cntr_t *, int, int, int));
ADDRESS       *abe_to_address PROTO((AdrBk_Entry *, AddrScrn_Disp *,
                                     AdrBk *, int *));
int            abes_are_equal PROTO((AdrBk_Entry *, AdrBk_Entry *));
char          *addr_book_selnick PROTO((void));
char          *addr_book_takeaddr PROTO((void));
char          *addr_lookup PROTO((char *, int *, int, AdrBk_Entry **));
AccessType     adrbk_access PROTO((PerAddrBook *));
int            adrbk_add PROTO((AdrBk *, a_c_arg_t, char *, char *, char *, \
		    char *, char *, Tag, adrbk_cntr_t *, int *, int, int, int));
int            adrbk_append PROTO((AdrBk *, char *, char *, char *, \
		    char *, char *, Tag, adrbk_cntr_t *));
void           adrbk_clearrefs PROTO((AdrBk *));
void           adrbk_close PROTO((AdrBk *));
void           adrbk_partial_close PROTO((AdrBk *));
adrbk_cntr_t   adrbk_count PROTO((AdrBk *));
int            adrbk_delete PROTO((AdrBk *, a_c_arg_t, int, int, int, int));
char          *adrbk_formatname PROTO((char *, char **, char **));
AdrBk_Entry   *adrbk_get_ae PROTO((AdrBk *, a_c_arg_t, Handling));
int            adrbk_is_in_sort_order PROTO((AdrBk *, int, int));
int            adrbk_listadd PROTO((AdrBk *, a_c_arg_t, char *));
int            adrbk_nlistadd PROTO((AdrBk *, a_c_arg_t, char **,int,int,int));
int            adrbk_listdel PROTO((AdrBk *, a_c_arg_t, char *));
int            adrbk_listdel_all PROTO((AdrBk *, a_c_arg_t, int));
AdrBk_Entry   *adrbk_lookup_by_addr PROTO((AdrBk *, char *, adrbk_cntr_t *));
AdrBk_Entry   *adrbk_lookup_by_nick PROTO((AdrBk *, char *, adrbk_cntr_t *));
AdrBk_Entry   *adrbk_newentry PROTO((void));
AdrBk         *adrbk_open PROTO((PerAddrBook *, char *, char *, int,int,int));
long           adrbk_get_nominal_cachesize PROTO((AdrBk *));
long           adrbk_set_nominal_cachesize PROTO((AdrBk *, long));
int            adrbk_sort PROTO((AdrBk *, a_c_arg_t, adrbk_cntr_t *, int));
AdrBk_Entry   *ae PROTO((long));
int            any_ab_open PROTO((void));
int            build_address_internal PROTO((BuildTo, char **, char **,
				    char **, int *, char **, int, int, int *));
AdrBk_Entry   *copy_ae PROTO((AdrBk_Entry *));
int            count_addrs PROTO((ADDRESS *));
int           *cpyint PROTO((int));
int            cur_is_open PROTO((void));
char          *decode_fullname_of_addrstring PROTO((char *, int));
AddrScrn_Disp *dlist PROTO((long));
void           edit_entry PROTO((AdrBk *, AdrBk_Entry *, a_c_arg_t, Tag,
							int, int *, char *));
char          *encode_fullname_of_addrstring PROTO((char *, char *));
int            est_size PROTO((ADDRESS *));
int            exp_any_expanded PROTO((EXPANDED_S *));
void           exp_free PROTO((EXPANDED_S *));
adrbk_cntr_t   exp_get_next PROTO ((EXPANDED_S **));
int            exp_howmany_expanded PROTO((EXPANDED_S *));
int            exp_is_expanded PROTO((EXPANDED_S *, a_c_arg_t));
void           exp_set_expanded PROTO((EXPANDED_S *, a_c_arg_t));
void           exp_unset_expanded PROTO((EXPANDED_S *, a_c_arg_t));
void           fill_in_dl_field PROTO((DL_CACHE_S *));
void           flush_dlc_from_cache PROTO((DL_CACHE_S *));
void           free_ae PROTO((AdrBk_Entry **));
time_t         get_adj_time PROTO((void));
char          *get_display_line PROTO((long, int, int *, int *, int *, char *));
DL_CACHE_S    *get_dlc PROTO((long));
void           init_ab_if_needed PROTO((void));
void           init_abook PROTO((PerAddrBook *, OpenStatus));
int            init_addrbooks PROTO((OpenStatus, int, int, int));
char          *listmem PROTO((long));
adrbk_cntr_t   listmem_count_from_abe PROTO((AdrBk_Entry *));
char          *listmem_from_dl PROTO((AdrBk *, AddrScrn_Disp *));
int            matching_dlcs PROTO((DL_CACHE_S *, DL_CACHE_S *));
int            our_build_address PROTO((BuildTo, char **, char **, char **,
								    int));
char         **parse_addrlist PROTO((char *));
char          *query_server PROTO((struct pine *, int, int *, int, char **));
void           restore_state PROTO((SAVE_STATE_S *));
void           rfc822_write_address_decode PROTO((char *, ADDRESS *, char **,
						  int));
void           save_state PROTO((SAVE_STATE_S *));
PerAddrBook   *setup_for_addrbook_add PROTO((SAVE_STATE_S *, int, char *));
int            single_entry_delete PROTO((AdrBk *, long, int *));
void           take_this_one_entry PROTO((struct pine *, TA_STATE_S **,
					  AdrBk *, long));
void           view_abook_entry PROTO((struct pine *, long));
void           warp_to_dlc PROTO((DL_CACHE_S *, long));
void           warp_to_beginning PROTO((void));
void           warp_to_end PROTO((void));
void           warp_to_top_of_abook PROTO((int));
ADDRESS       *wp_lookups PROTO((char *, WP_ERR_S *, int));
#ifdef	ENABLE_LDAP
int            ldap_lookup_all PROTO((char *, int, int, LDAPLookupStyle,
				      CUSTOM_FILT_S *, LDAP_SERV_RES_S **,
				      WP_ERR_S *, LDAP_SERV_RES_S **));
void           free_ldap_result_list PROTO((LDAP_SERV_RES_S *));
STORE_S       *prep_ldap_for_viewing PROTO((struct pine *, LDAP_SERV_RES_S *,
					    SourceType, HANDLE_S **));
ADDRESS       *address_from_ldap PROTO((LDAP_SERV_RES_S *));
#endif

#endif /* _ADRBKLIB_INCLUDED */
