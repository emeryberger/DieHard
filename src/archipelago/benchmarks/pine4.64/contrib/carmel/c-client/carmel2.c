#define TESTING
/*----------------------------------------------------------------------

    T H E   C A R M E L   M A I L   F I L E   D R I V E R

 Author(s):   Laurence Lundblade
              Baha'i World Centre
              Data Processing
              Haifa, Israel
              Internet: lgl@cac.washington.edu or laurence@bwc.org
              September 1992

 Last edited: Aug 31, 1994

The Carmel2 mail file stores messages in individual files and
implements folders or mailboxes with index files that contain
references to the files a nd a full c-client envelope in an easily
parsed form. It was written as a needed part of the pod mail file
driver with hopes that it might be useful otherwise some day. It has
only been run with the carmel driver.

Advantages over Berkeley format and driver:
  + Opening mail folder is very fast
  + Expunge is fast
  + Check point is very fast
  + Memory usage is much lower
  + Search of message headers is fast
Disadvantages:
  - Fetching a large message is slow
  - Searching the message bodies is slow
  - Sorting the mailbox is slow

         Index File Format
         -----------------

The first line of the file is always:
  "\254\312--CARMEL2-MAIL-FILE-INDEX--\n"

It is followed by index entries which are of the format:
---START-OF-MESSAGE---\007\001nnnnnnnnnn     
Ufrads______________________________
Zxxxxxx
D..... <Many fields here, labeled by the first letter in the line>
       <Fields may be repeated>


The index is almost an ASCII file. With the first version of this
driver it is not advisable to edit this file, lest the byte counts get
disrupted.  In the future it will be editable. The file starts with
two binary bytes so the file(1) utility will report it as "data" to
discourage would be hackers from messing with it. The ^G is also in
each index entry for the same reason. If you are reading this source you
probably know enough to edit the index file without destroying it.
Other parts of the format are designed for the easiest possible
parsing. The idea was to have a file format that was reasonable to
fiddle for debugging, to discourage inexperienced folks for fiddling
it and to be efficient for use by the program.


      Routines and data structures
      ----------------------------

C-CLIENT INTERFACE FUCTIONS
  carmel2_valid       - check to see if a mailbox is valid for carmel2 mail files
  carmel2_isvalid     - actual work of checking
  carmel2_find        - generate list of carmel2 mailboxes
  carmel2_sift_files  - select mailboxes out of list, used with scandir
  carmel2_find_bboars - dummy routine, doesn't do anything

  carmel2_open        - initial phase of opening a mailbox
  carmel2_open2       - real work of opening mailbox, shared with carmel driver
  carmel2_close       - close a mail stream

  carmel2_fetchfast   - fetch "fast" message info, noop for this driver
  carmel2_fetchflags  - fetch the flags, a noop for this driver
  carmel2_fetchstructure - fetch and envelope and possibly body  

  carmel2_fetchheader - fetch the text header of the message
  carmel2_fetchtext   - fetch the text of the message (no header included)
  carmel2_fetchbody   - fetch the text of a body part of a message

  carmel2_setflag     - Set a flag for a message sequence
  carmel2_clearflag   - Clear a flag for a message sequence
  
  carmel2_search      - Invoke the search facilities

  carmel2_ping        - Check for new mail and see if still alive
  carmel2_check       - Checkpoint the message statuses to the disk
  carmel2_expunge     - Delete all the messages marked for delete 
  
  carmel2_copy        - Copy a message to another mailbox
  carmel2_move        - Move a message to another mailbox
  
  carmel_gc          - Garbage collection, a noop for this driver
  carmel_cache       - The required c-client cache handler, doesn't do much

  carmel2_append      - Append a message to a mail folder

SUPPORT FUNCTIONS
  carmel_bodystruct  - Fetch the body structure for a carmel message
  carmel_parse_address - Parse the address out of a carmel index
  carmel_parse_addr_field - Parse individual address field out of carmel index
  carmel2_write_index - Write an entry into a carmel index
  carmel2_index_address - Write an address into a carmel index

  carmel_readmsg     - Read a message file into memory, header text or both
  carmel_spool_mail  - Get new mail out of spoold mail file
  carmel_copy_msg_file - Make copy of messsage file when copying (unused)
  
  carmel2_lock        - Lock a carmel index for read or write
  carmel2_unlock      - Unlock a carmel index
  carmel2_update_lock - Touch lock mod time, marking it as active
  carmel2_bezerk_lock - Lock the spool mail file Berkeley style
  carmel2_bezerk_unlock - Unlock the spool mail file
  
  carmel2_check_dir   - Check that directory exists and is writeable 
  carmel2_new_data_file - Get file number for new message file
  carmel2_calc_paths  - Calculate path names for carmel driver

  carmel_parse_bezerk_status - Parse the "Status: OR" field in mail headers
  carmel2_getflags    - Turn the named flags into a bit mask
  carmel_new_mc      - Get pointer to new MESSAGECACHE, allocating if needed

  carmel_append2     - The real work of append a message to a mailbox
  
  carmel2_search-*    - A bunch of search support routines
 
  month_abbrev2      - Returns three letter month abbreviation given a name
  carmel2_rfc822_date - Parse a date string, into MESSAGECACHE structure
  carmel2_date2string - Generate a date string from MESSAGECACHE
  carmel2_parse_date  - Parse date out of a carmel index
  next_num           - Called to parse dates
 
  strucmp2           - Case insensitive strcmp()
  struncmp2          - Case insensitive strncmp()
  
 ----------------------------------------------------------------------*/

#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include <errno.h>
extern int errno;               /* just in case */
#include "mail.h"
#include "osdep.h"
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "carmel2.h"
#include "rfc822.h"
#include "misc.h"


char *strindex2(), *strchr();

#ifdef ANSI
static int      carmel2_sift_files(struct direct *);
static BODY    *carmel2_bodystruct(MAILSTREAM *, MESSAGECACHE *);
static ADDRESS *carmel2_parse_address(char *, ADDRESS *, char *);
static char    *carmel2_parse_addr_field(char **);
static int      carmel2_index_address(ADDRESS *, int , FILE *);
static int      carmel2_parse_mail(MAILSTREAM *, long);
void            carmel2_spool_mail(MAILSTREAM *, char *, char *, int);
static int      carmel2_copy_msg_file(MAILSTREAM *, int, char *);
static int      carmel2_bezerk_lock(char *, char *);
static void     carmel2_bezerk_unlock(int, char *);
static int      carmel2_new_data_file(CARMEL2LOCAL *, char *);
static char    *carmel2_calc_paths(int, char *, int);
static short    carmel2_getflags(MAILSTREAM *, char *);
static MESSAGECACHE *carmel2_new_mc(MAILSTREAM *, int);
static char     carmel2_search_all(MAILSTREAM *, long, char *, long);
static char     carmel2_search_answered(MAILSTREAM *, long, char *, long);
static char     carmel2_search_deleted(MAILSTREAM *, long, char *, long);
static char     carmel2_search_flagged(MAILSTREAM *, long, char *, long);
static char     carmel2_search_keyword(MAILSTREAM *, long, char *, long);
static char     carmel2_search_new(MAILSTREAM *, long, char *, long);
static char     carmel2_search_old(MAILSTREAM *, long, char *, long);
static char     carmel2_search_recent(MAILSTREAM *, long, char *, long);
static char     carmel2_search_seen(MAILSTREAM *, long, char *, long);
static char     carmel2_search_unanswered(MAILSTREAM *, long, char *, long);
static char     carmel2_search_undeleted(MAILSTREAM *, long, char *, long);
static char     carmel2_search_unflagged(MAILSTREAM *, long, char *, long);
static char     carmel2_search_unkeyword(MAILSTREAM *, long, char *, long);
static char     carmel2_search_unseen(MAILSTREAM *, long, char *, long);
static char     carmel2_search_before(MAILSTREAM *, long, char *, long);
static char     carmel2_search_on(MAILSTREAM *, long, char *, long);
static char     carmel2_search_since(MAILSTREAM *, long, char *, long);
static unsigned long carmel2_msgdate(MAILSTREAM *, long);
static char     carmel2_search_body(MAILSTREAM *, long, char *, long);
static char     carmel2_search_subject(MAILSTREAM *, long, char *, long);
static char     carmel2_search_text(MAILSTREAM *, long, char *, long);
static char     carmel2_search_bcc(MAILSTREAM *, long, char *, long);
static char     carmel2_search_cc(MAILSTREAM *, long, char *, long);
static char     carmel2_search_from(MAILSTREAM *, long, char *, long);
static char     carmel2_search_to(MAILSTREAM *, long, char *, long);
typedef char (*search_t)  ();
static search_t carmel2_search_date(search_t, long *);
static search_t carmel2_search_flag(search_t, char **);
static search_t carmel2_search_string(search_t, char **, long *);
static void     carmel2_date2string(char *, MESSAGECACHE *);
static void     carmel2_parse_date(MESSAGECACHE *, char *);
static int      next_num(char **);
#else
static int      carmel2_sift_files();
static BODY    *carmel2_bodystruct();
static ADDRESS *carmel2_parse_address();
static char    *carmel2_parse_addr_field();
static int      carmel2_index_address();
static int      carmel2_parse_mail();
void            carmel2_spool_mail();
static int      carmel2_copy_msg_file();
static int      carmel2_bezerk_lock();
static void     carmel2_bezerk_unlock();
static int      carmel2_new_data_file();
static char    *carmel2_calc_paths();
static short    carmel2_getflags();
static MESSAGECACHE *carmel2_new_mc();
static char     carmel2_search_all();
static char     carmel2_search_answered();
static char     carmel2_search_deleted();
static char     carmel2_search_flagged();
static char     carmel2_search_keyword();
static char     carmel2_search_new();
static char     carmel2_search_old();
static char     carmel2_search_recent();
static char     carmel2_search_seen();
static char     carmel2_search_unanswered();
static char     carmel2_search_undeleted();
static char     carmel2_search_unflagged();
static char     carmel2_search_unkeyword();
static char     carmel2_search_unseen();
static char     carmel2_search_before();
static char     carmel2_search_on();
static char     carmel2_search_since();
static unsigned long carmel2_msgdate();
static char     carmel2_search_body();
static char     carmel2_search_subject();
static char     carmel2_search_text();
static char     carmel2_search_bcc();
static char     carmel2_search_cc();
static char     carmel2_search_from();
static char     carmel2_search_to();
typedef char (*search_t)  ();
static search_t carmel2_search_date();
static search_t carmel2_search_flag();
static search_t carmel2_search_string();
static void     carmel2_date2string();
static void     carmel2_parse_date();
static int      next_num();
#endif


/*------ Driver dispatch used by MAIL -------*/

DRIVER carmel2driver = {
  "carmel2",
  (DRIVER *) NIL,               /* next driver */
  carmel2_valid,                /* mailbox is valid for us */
  carmel2_parameters,           /* manipulate parameters */
  carmel2_find,                 /* find mailboxes */
  carmel2_find_bboards,         /* find bboards */
  carmel2_find_all,             /* find all mailboxes */
  carmel2_find_bboards,         /* find all bboards ; just a noop here */
  carmel2_subscribe,            /* subscribe to mailbox */
  carmel2_unsubscribe,          /* unsubscribe from mailbox */
  carmel2_subscribe_bboard,     /* subscribe to bboard */
  carmel2_unsubscribe_bboard,   /* unsubscribe from bboard */
  carmel2_create,
  carmel2_delete,
  carmel2_rename,
  carmel2_open,                 /* open mailbox */
  carmel2_close,                /* close mailbox */
  carmel2_fetchfast,            /* fetch message "fast" attributes */
  carmel2_fetchflags,           /* fetch message flags */
  carmel2_fetchstructure,       /* fetch message envelopes */
  carmel2_fetchheader,          /* fetch message header only */
  carmel2_fetchtext,            /* fetch message body only */
  carmel2_fetchbody,            /* fetch message body section */
  carmel2_setflag,              /* set message flag */
  carmel2_clearflag,            /* clear message flag */
  carmel2_search,               /* search for message based on criteria */
  carmel2_ping,                 /* ping mailbox to see if still alive */
  carmel2_check,                /* check for new messages */
  carmel2_expunge,              /* expunge deleted messages */
  carmel2_copy,                 /* copy messages to another mailbox */
  carmel2_move,                 /* move messages to another mailbox */
  carmel2_append,                /* Append message to a mailbox */
  carmel2_gc,                   /* garbage collect stream */
};

MAILSTREAM carmel2proto ={&carmel2driver};/*HACK for default_driver in pine.c*/

/*-- Some string constants used in carmel2 indexes --*/
char *carmel2_s_o_m     = "---START-OF-MESSAGE---";
int   carmel2_s_o_m_len = 22;
char *carmel2_s_o_f     =   "\254\312--CARMEL2-MAIL-FILE-INDEX--\n";



/*-- Buffers used for various reasons, also used by carmel driver --*/
char carmel_20k_buf[20000], carmel_path_buf[CARMEL_PATHBUF_SIZE],
     carmel_error_buf[200];


/*----------------------------------------------------------------------
    Carmel mail validate mailbox

Args: - mailbox name

Returns: our driver if name is valid, otherwise calls valid in next driver
 ---*/

DRIVER *carmel2_valid (name)
        char *name;
{
    return (carmel2_isvalid (name) ? &carmel2driver : NIL);
}



/*----------------------------------------------------------------------
  Open the mailbox and see if it's the correct format

Args: name -- name of the mailbox, not fully qualified path

Returns: 0 if is is not valid, 1 if it is valid

The file must be a regular file and start with the string in the
variable carmel2_s_o_f. It has a magic number of sorts
  ----*/
int
carmel2_isvalid (name)
        char *name;
{
    char           *carmel_index_file;
    struct stat     sbuf;
    int             fd;
    struct carmel_mb_name *parsed_name;

    /*---- Must match FQN name of carmel mailboxes ----*/
    parsed_name = carmel_parse_mb_name(name, '2');
    if(parsed_name == NULL)
      return;
    carmel_free_mb_name(parsed_name);
       
    carmel_index_file = carmel2_calc_paths(CalcPathCarmel2Index, name, 0);
    if(carmel_index_file == NULL)
      return(0); /* Will get two error messages here, one from dummy driver */
    
    if(stat(carmel_index_file, &sbuf) < 0)
      return(1); /* If the names match and it doesn't exist, it's valid */

    if(!(sbuf.st_mode & S_IFREG))
      return(0);

    fd = open(carmel_index_file, O_RDONLY);
    if(fd < 0)
      return(0);

    if(read(fd, carmel_20k_buf, 200) <= 0)
      return(0);

    close(fd);

    if(strncmp(carmel_20k_buf, carmel2_s_o_f, strlen(carmel2_s_o_f)))
       return(0);

    return(1);
}



/*----------------------------------------------------------------------
   Set parameters for the carmel driver.
 
   Currently does nothing
  ----*/
void *
carmel2_parameters()
{
}




/*----------------------------------------------------------------------
   Carmel mail find list of mailboxes

Args:  stream -- mail stream to find mailboxes for
       pat    -- wildcard pattern to match (currently unused)

Returns nothing, the results are passed back by calls to mm_log

This needs testing
 ----*/

void 
carmel2_find (stream, pat)
    MAILSTREAM *stream;
    char *pat;
{
    char                   tmp[MAILTMPLEN];
    struct direct        **namelist, **n;
    int                    num;
    extern int             alphasort();
    struct carmel_mb_name *parsed_name;
    struct passwd         *pw;

    parsed_name = carmel_parse_mb_name(pat, '2');
    if(parsed_name == NULL)
      return;

    if(parsed_name->user == NULL) {
        sprintf(tmp, "%s/%s", myhomedir(), CARMEL2_INDEX_DIR);
    } else {
        pw = getpwnam(parsed_name->user);
        if(pw == NULL) {
            sprintf(carmel_error_buf,
                  "Error accessing mailbox \"%s\". No such user name \"%s\"\n",
                    pat, parsed_name->user);
            mm_log(carmel_error_buf, ERROR);
            return;
        }
        sprintf(tmp, "%s/%s", pw->pw_dir, CARMEL2_INDEX_DIR);
    }

    sprintf(tmp, "%s/%s", myhomedir(), CARMEL2_INDEX_DIR);

    num = scandir(tmp, &namelist, carmel2_sift_files, alphasort);

    if(num <= 0) {
/*  Seems this error message should not be logged
        sprintf(carmel_error_buf, "Error finding mailboxes \"%s\": %s",
                pat, strerror(errno));
        mm_log(carmel_error_buf, ERROR); */
        return;
    }

    for(n = namelist; num > 0; num--, n++) {
        if(parsed_name->user == NULL) {
            sprintf(tmp, "%s%s%c%s", CARMEL_NAME_PREFIX, parsed_name->version,
                    CARMEL_NAME_CHAR, (*n)->d_name);
        } else {
            sprintf(tmp, "%s%s%c%s%c%s", CARMEL_NAME_PREFIX,
                    parsed_name->version, CARMEL_NAME_CHAR,
                    parsed_name->user, CARMEL_NAME_CHAR,
                    (*n)->d_name);
        }
        mm_mailbox(tmp);
        free(*n); 
    }
    free(namelist);   
    carmel_free_mb_name(parsed_name);
}



/*----------------------------------------------------------------------
   Find_all is the same as find for the carmel2 format -- no notion
   of subscribed and unsubscribed
  ---*/
void 
carmel2_find_all (stream, pat)
    MAILSTREAM *stream;
    char *pat;
{
  carmel2_find(stream, pat);
}



/*----------------------------------------------------------------------
  Carmel2 mail find list of bboards; always NULL, no bboards 
 ----*/
void carmel2_find_bboards (stream,pat)
        MAILSTREAM *stream;
        char *pat;
{
  /* Always a no-op, Carmel2 file format doesn't do news */
}



/*----------------------------------------------------------------------
  Carmel2 mail find list of bboards; always NULL, no bboards 
 ----*/
void carmel2_find_all_bboards (stream,pat)
        MAILSTREAM *stream;
        char *pat;
{
  /* Always a no-op, Carmel2 file format doesn't do news */
}



/*----------------------------------------------------------------------
   This is used by scandir to determine which files in the directory
 are treated as mail files
  ----*/
static int
carmel2_sift_files(dir)
     struct direct *dir;
{
    if(dir->d_name[0] == '.')
      return(0);
    else
      return(1);
}



/*----------------------------------------------------------------------
    Dummy subscribe/unsubscribe routines.  
    Carmel driver does support this. Maybe it should support the UNIX
    sm sometime
  ----*/
long carmel2_subscribe() {}
long carmel2_unsubscribe() {}
long carmel2_subscribe_bboard() {}
long carmel2_unsubscribe_bboard() {}



/*----------------------------------------------------------------------
  ----*/
long
carmel2_create(stream, mailbox)
     MAILSTREAM *stream;
     char       *mailbox;
{
}



/*----------------------------------------------------------------------
  ----*/
long
carmel2_delete(stream, mailbox)
     MAILSTREAM *stream;
     char       *mailbox;
{
}



/*----------------------------------------------------------------------
  ----*/
long
carmel2_rename(stream, orig_mailbox, new_mailbox)
     MAILSTREAM *stream;
     char       *orig_mailbox, *new_mailbox;
{
}


/*----------------------------------------------------------------------
  Open a carmel2 mail folder/stream

Args: stream -- stream to by fully opened

Returns: it's argument or NULL of the open fails

This needs testing and more code, see pod_open for examples
  ----*/

MAILSTREAM *
carmel2_open (stream)
        MAILSTREAM *stream;
{
    char tmp[MAILTMPLEN];
    struct hostent *host_name;

    if(!stream) return &carmel2proto; /* Must be a OP_PROTOTYPE call */

    /* close old file if stream being recycled */
    if (LOCAL) {
        carmel2_close (stream);         /* dump and save the changes */
        stream->dtb = &carmel2driver;   /* reattach this driver */
        mail_free_cache (stream);       /* clean up cache */
    }

    mailcache = carmel2_cache;

    /* Allocate local stream */
    stream->local = fs_get (sizeof (CARMEL2LOCAL));

    stream->readonly = 1; /* Read-only till more work is done */

    LOCAL->msg_buf             = NULL;
    LOCAL->msg_buf_size        = 0;
    LOCAL->buffered_file       = NULL;
    LOCAL->msg_buf_text_start  = NULL;
    LOCAL->msg_buf_text_offset = 0;
    LOCAL->stdio_buf           = NULL;
    stream->msgno              = -1;
    stream->env                = NULL;
    stream->body               = NULL;
    stream->scache             = 1;
    LOCAL->calc_paths          = carmel2_calc_paths;
    LOCAL->aux_copy            = NULL;
    LOCAL->new_file_on_copy    = 1;

    if(carmel_open2(stream,
       (*(LOCAL->calc_paths))(CalcPathCarmel2Index, stream->mailbox,0)) < 0)
      return(NULL);

    mail_exists (stream,stream->nmsgs);
    mail_recent (stream,stream->recent);

    return(stream);
}



/*----------------------------------------------------------------------
    Do the real work of opening a Carmel folder. 

Args: stream -- The mail stream being opened
      file   -- Path name of the actual Carmel index file

Returns: -1 if the open fails
          0 if it succeeds

This is shared between the Carmel driver and the Pod driver.

Here, the status, size and date (fast info) of a message entry
is read out of the index file into the MESSAGECACHE structures.
To make the reading efficient these items are at the beginning 
of each entry and there is a byte offset to the next entry.
If the byte offset is wrong (as detected by looking for the
start of message string) then the index is read line by line
until it synchs up. This allows manual editing of the index.
However, the first two lines of an index entry cannot be
edited because mail_check() writes them in place. If these
lines have been edited it is detected here and the folder is
deemed corrupt.
  ---*/
carmel_open2(stream, file)
     MAILSTREAM *stream;
     char       *file;
{
    long          file_pos, recent;
    MESSAGECACHE *mc;
    struct stat   sb;

    if(stat(file, &sb) < 0) {
        sprintf(carmel_error_buf, "Can't open mailbox: %s", strerror(errno));
        mm_log(carmel_error_buf, ERROR);
        return(-1);
    }

    LOCAL->index_stream = fopen(file, stream->readonly ? "r" : "r+");
    LOCAL->stdio_buf    = fs_get(CARMEL2_INDEX_BUF_SIZE);
    setbuffer(LOCAL->index_stream, LOCAL->stdio_buf, CARMEL2_INDEX_BUF_SIZE);
    if(LOCAL->index_stream == NULL) {
        sprintf(carmel_error_buf, "Can't open mailbox: %s", strerror(errno));
        mm_log(carmel_error_buf, ERROR);
        return(-1);
    }

    recent            = 0;
    stream->nmsgs     = 0;
    LOCAL->cache_size = 0;
    LOCAL->mc_blocks  = NULL;
    LOCAL->index_size = sb.st_size;

    /*---- Read line with magic number, which we already checked ------*/
    if(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),LOCAL->index_stream)==NULL){
        fclose(LOCAL->index_stream);
        mm_log("Mailbox is corrupt. Open failed", ERROR);
        return(-1);
    }

    file_pos = ftell(LOCAL->index_stream);  
    if(carmel2_parse_mail(stream, file_pos) < 0) {
        fclose(LOCAL->index_stream);
        mm_log("Mailbox is corrupt. Open failed", ERROR);
        return(-1);
    }

    /* Bug, this doesn't really do the recent correctly */

    stream->recent    = recent;
    return(0);
}



/*----------------------------------------------------------------------
     Carmel mail close

Args: stream -- stream to close

 ----*/

void
carmel2_close (stream)
        MAILSTREAM *stream;
{
    if (LOCAL) {                        /* only if a file is open */
        carmel2_check (stream);         /* dump final checkpoint */
        if(LOCAL->msg_buf)
          fs_give ((void **) &LOCAL->msg_buf);
        if(LOCAL->stdio_buf)
          fs_give ((void **) &LOCAL->stdio_buf);
                                    /* nuke the local data */
        fs_give ((void **) &stream->local);
        stream->dtb = NIL;              /* log out the DTB */
    }
}



/*----------------------------------------------------------------------
    Carmel mail fetch fast information.

This is a no-op because the data is always available as it is read in to
the message cache blocks when the folder is opened
----*/
void
carmel2_fetchfast (stream, sequence)
        MAILSTREAM *stream;
        char *sequence;
{
    return;
}



/*----------------------------------------------------------------------
    Carmel2 mail fetch flags.

This is a no-op because the data is always available as it is read in to
the message cache blocks when the folder is opened
----*/
void
carmel2_fetchflags(stream, sequence)
        MAILSTREAM *stream;
        char *sequence;
{
    return;                     /* no-op for local mail */
}



/*----------------------------------------------------------------------
  Carmel mail fetch message structure
 Args: stream -- stream to get structure for
       msgno  -- Message number to fetch data for
       body   -- Pointer to place to return body strucuture, may be NULL

If the request is the for the same msgno as last time, the saved copy
of the envelope and/or body structure is returned.

To get the envelope the Carmel index file itself must be read and parsed,
but this is fast because it is easy to parse (by design) and the amount of
data is small.

To get the body, the whole message is read into memory and then parsed
by routines called from here. Doing this for a large message can be slow,
so it is best not to request the body if it is not needed. (body == NULL)
 ----*/

ENVELOPE *
carmel2_fetchstructure (stream, msgno, body)
        MAILSTREAM *stream;
        long        msgno;
        BODY      **body;
{
    MESSAGECACHE *mc;
    ENVELOPE    **env;
    BODY        **b;
  
    env = &stream->env;         
    b   = &stream->body;

    if (msgno != stream->msgno){
        /* flush old poop if a different message */
        mail_free_envelope (env);
        mail_free_body (b);
    }
    stream->msgno = msgno;

    mc = MC(msgno);

    if(*env == NULL) {
        *env = mail_newenvelope();
      
        fseek(LOCAL->index_stream, mc->data1, 0);
        fgets(carmel_20k_buf, sizeof(carmel_20k_buf), LOCAL->index_stream);
        if(strncmp(carmel_20k_buf, carmel2_s_o_m, carmel2_s_o_m_len))
          return(NULL); /* Oh ooo */
      
         carmel2_date2string(carmel_20k_buf, mc);
         (*env)->date = cpystr(carmel_20k_buf);
      
         while(fgets(carmel_20k_buf, sizeof(carmel_20k_buf),
                     LOCAL->index_stream) != NULL &&
               strncmp(carmel_20k_buf, carmel2_s_o_m, carmel2_s_o_m_len)) {
             carmel_20k_buf[strlen(carmel_20k_buf) - 1] = '\0';
             switch(*carmel_20k_buf) {
               case 'F':
                 (*env)->from = carmel2_parse_address(carmel_20k_buf,
                                                     (*env)->from,
                                                     mylocalhost());
                 break;
               case 'T':
                 (*env)->to = carmel2_parse_address(carmel_20k_buf,
                                                   (*env)->to,
                                                   mylocalhost());
                 break;
               case 'C':
                 (*env)->cc = carmel2_parse_address(carmel_20k_buf,
                                                   (*env)->cc,
                                                   mylocalhost());
                 break;
               case 'B':
                 (*env)->bcc = carmel2_parse_address(carmel_20k_buf,
                                                    (*env)->bcc,
                                                    mylocalhost());
                 break;
               case 'E':
                 (*env)->sender = carmel2_parse_address(carmel_20k_buf,
                                                       (*env)->sender,
                                                       mylocalhost());
                 break;
               case 'R':
                 (*env)->reply_to = carmel2_parse_address(carmel_20k_buf,
                                                         (*env)->reply_to,
                                                         mylocalhost());
                 break;
               case 'H':
                 (*env)->return_path =carmel2_parse_address(carmel_20k_buf,
                                                           (*env)->return_path,
                                                           mylocalhost());
                 break;
               case 'J':
                 (*env)->subject     = cpystr(carmel_20k_buf+1);
                 break;
               case 'I':
                 (*env)->message_id  = cpystr(carmel_20k_buf+1);
                 break;
               case 'L':
                 (*env)->in_reply_to = cpystr(carmel_20k_buf+1);
                 break;
               case 'N':
                 (*env)->newsgroups  = cpystr(carmel_20k_buf+1);
                 break;
               case 'r':
                 (*env)->remail      = cpystr(carmel_20k_buf+1);
                 break;
               default:
                 break;
             }
         }
    }

    if(body != NULL) {
        if(*b == NULL) {
            /* Have to fetch the body structure too */
            *b  = carmel2_bodystruct(stream, mc);
        }
        *body = *b;
    }
  
    return(*env);    /* return the envelope */
}




/*----------------------------------------------------------------------
  Carmel mail fetch message header

Args: stream -- 
      msgno

Returns: pointer to text of mail header. Returned string is null terminated
         and consists of internel storage. It will be valid till the next
         call to mail_fetchheader() or mail_fetchtext(). An empty
         string is returned if the fetch fails.
 ----*/

char *
carmel2_fetchheader (stream, msgno)
        MAILSTREAM *stream;
        long msgno;
{
    char          *header;
    MESSAGECACHE  *mc;

    mc     = MC(msgno);
    header = carmel2_readmsg(stream, 1, 0, mc->data2);

    return(header == NULL ? "" : header);
}



/*----------------------------------------------------------------------
  Carmel mail fetch message text (only)

Args: stream -- 
      msgno

Returns: pointer to text of mail message. Returned string is null terminated
         and consists of internel storage. It will be valid till the next
         call to mail_fetchheader() or mail_fetchtext(). An empty
         string is returned if the fetch fails.
 ----*/

char *
carmel2_fetchtext(stream, msgno)
        MAILSTREAM *stream;
        long msgno;
{
    MESSAGECACHE *mc;
    char         *m;

    mc = MC(msgno);

    if (!mc->seen) {            /* if message not seen before */
        mc->seen = T;           /* mark as seen */
        LOCAL->dirty = T;       /* and that stream is now dirty */
    }

    m = carmel2_readmsg(stream, 0, mc->rfc822_size, mc->data2);

    return(m);
}



/*----------------------------------------------------------------------
    Fetch MIME body parts

Args: stream
      msgno
      section -- string section number spec. i.e. "1.3.4"
      len     -- pointer to return len in

Returns: The contents of the body part, or NULL
 ---*/

char *
carmel2_fetchbody (stream, msgno, section, len)
        MAILSTREAM    *stream;
        long           msgno;
        char          *section;
        unsigned long *len;
{
    char         *base;
    BODY         *b;
    PART         *part;
    int           part_num;
    long          offset;
    MESSAGECACHE *mc;

    if (carmel2_fetchstructure(stream, msgno, &b) == NULL || b == NULL)
      return(NULL);

    if(section == NULL || *section == '\0')
      return(NULL);

    part_num = strtol(section, &section, 10);
    if(part_num <= 0)
      return(NULL);

    base   = carmel2_fetchtext(stream, msgno);
    if(base == NULL)
      base = "";
    offset = 0;

    do {                         /* until find desired body part */
        if (b->type == TYPEMULTIPART) {
            part = b->contents.part;    /* yes, find desired part */
            while (--part_num && (part = part->next));
            if (!part) 
              return (NIL);     /* bad specifier */

            /* Found part, get ready to go further */
            b = &part->body;
            if(b->type == TYPEMULTIPART && *section == '\0')
              return(NULL); /* Ran out of section numbers, needed more */
               
            offset = part->offset;      /* and its offset */

        } else if (part_num != 1) {
            return NIL; /* Not Multipart, better be part 1 */
        }

                                    /* need to go down further? */

        if(*section == 0) {
            break;
        } else { 
            switch (b->type) {
              case TYPEMESSAGE: /* embedded message, calculate new base */
                offset = b->contents.msg.offset;
                b = b->contents.msg.body;
                /* got its body, drop into multipart case*/
  
              case TYPEMULTIPART:            /* multipart, get next section */
                if ((*section++ == '.') &&
                    (part_num = strtol (section, &section,10)) > 0)
                  break; /* Found the part */
  
              default:                  /* bogus subpart specification */
                return(NULL);
            }
        }
    } while (part_num);

    mc = MC(msgno);
                                  /* lose if body bogus */
    if ((!b) || b->type == TYPEMULTIPART)
      return(NULL);

    if (!mc->seen) {            /* if message not seen before */
      mc->seen = T;             /* mark as seen */
      LOCAL->dirty = T;         /* and that stream is now dirty */
    }

    *len = b->size.ibytes;
    return(base + offset);
}



/*----------------------------------------------------------------------
 *  Carmel mail set flag
 * Accepts: MAIL stream
 *          sequence
 *          flag(s)
 */

void
carmel2_setflag (stream,sequence,flag)
        MAILSTREAM *stream;
        char *sequence;
        char *flag;
{
  MESSAGECACHE *elt;
  long i;
  short f = carmel2_getflags (stream,flag);
  if (!f) return;               /* no-op if no flags to modify */
                                /* get sequence and loop on it */
  if (mail_sequence (stream,sequence))
    for (i = 1; i <= stream->nmsgs; i++){
        elt = MC(i);
        if (elt->sequence) {
                                    /* set all requested flags */
          if (f&fSEEN) elt->seen = T;
          if (f&fDELETED) elt->deleted = T;
          if (f&fFLAGGED) elt->flagged = T;
          if (f&fANSWERED) elt->answered = T;
          /* Could be more creative about keeping track to see if something
             really changed */
          LOCAL->dirty = T;
        }
    }
}



/*----------------------------------------------------------------------
 * Carmel mail clear flag
 * Accepts: MAIL stream
 *          sequence
 *          flag(s)
 */

void
carmel2_clearflag (stream,sequence,flag)
        MAILSTREAM *stream;
        char *sequence;
        char *flag;
{
  MESSAGECACHE *elt;
  long i = stream->nmsgs;
  short f = carmel2_getflags (stream,flag);
  if (!f) return;               /* no-op if no flags to modify */
                                /* get sequence and loop on it */
  if (mail_sequence (stream,sequence))
    for(i = 1; i <= stream->nmsgs; i++) {
        elt = MC(i);
        if (elt->sequence) {
                                    /* clear all requested flags */
          if (f&fSEEN) elt->seen = NIL;
          if (f&fDELETED) elt->deleted = NIL;
          if (f&fFLAGGED) elt->flagged = NIL;
          if (f&fANSWERED) elt->answered = NIL;
                            /* clearing either seen or deleted does this */
          /* Could be more creative about keeping track to see if something
             really changed */
          LOCAL->dirty = T;     /* mark stream as dirty */
        }
    }
}



/* Carmel mail search for messages
 * Accepts: MAIL stream
 *          search criteria
 */

void 
carmel2_search (stream,criteria)
        MAILSTREAM *stream;
        char *criteria;
{
  long i,n;
  char *d;
  search_t f;

                                /* initially all searched */
  for (i = 1; i <= stream->nmsgs; ++i) mail_elt (stream,i)->searched = T;
                                /* get first criterion */
  if (criteria && (criteria = strtok (criteria," "))) {
                                /* for each criterion */
    for (; criteria; (criteria = strtok (NIL," "))) {
      f = NIL; d = NIL; n = 0;  /* init then scan the criterion */
      switch (*ucase (criteria)) {
      case 'A':                 /* possible ALL, ANSWERED */
        if (!strcmp (criteria+1,"LL")) f = carmel2_search_all;
        else if (!strcmp (criteria+1,"NSWERED")) f = carmel2_search_answered;
        break;
      case 'B':                 /* possible BCC, BEFORE, BODY */
        if (!strcmp (criteria+1,"CC"))
          f = carmel2_search_string (carmel2_search_bcc,&d,&n);
        else if (!strcmp (criteria+1,"EFORE"))
          f = carmel2_search_date (carmel2_search_before,&n);
        else if (!strcmp (criteria+1,"ODY"))
          f = carmel2_search_string (carmel2_search_body,&d,&n);
        break;
      case 'C':                 /* possible CC */
        if (!strcmp (criteria+1,"C"))
          f = carmel2_search_string (carmel2_search_cc,&d,&n);
        break;
      case 'D':                 /* possible DELETED */
        if (!strcmp (criteria+1,"ELETED")) f = carmel2_search_deleted;
        break;
      case 'F':                 /* possible FLAGGED, FROM */
        if (!strcmp (criteria+1,"LAGGED")) f = carmel2_search_flagged;
        else if (!strcmp (criteria+1,"ROM"))
          f = carmel2_search_string (carmel2_search_from,&d,&n);
        break;
      case 'K':                 /* possible KEYWORD */
        if (!strcmp (criteria+1,"EYWORD"))
          f = carmel2_search_flag (carmel2_search_keyword,&d);
        break;
      case 'N':                 /* possible NEW */
        if (!strcmp (criteria+1,"EW")) f = carmel2_search_new;
        break;

      case 'O':                 /* possible OLD, ON */
        if (!strcmp (criteria+1,"LD")) f = carmel2_search_old;
        else if (!strcmp (criteria+1,"N"))
          f = carmel2_search_date (carmel2_search_on,&n);
        break;
      case 'R':                 /* possible RECENT */
        if (!strcmp (criteria+1,"ECENT")) f = carmel2_search_recent;
        break;
      case 'S':                 /* possible SEEN, SINCE, SUBJECT */
        if (!strcmp (criteria+1,"EEN")) f = carmel2_search_seen;
        else if (!strcmp (criteria+1,"INCE"))
          f = carmel2_search_date (carmel2_search_since,&n);
        else if (!strcmp (criteria+1,"UBJECT"))
          f = carmel2_search_string (carmel2_search_subject,&d,&n);
        break;
      case 'T':                 /* possible TEXT, TO */
        if (!strcmp (criteria+1,"EXT"))
          f = carmel2_search_string (carmel2_search_text,&d,&n);
        else if (!strcmp (criteria+1,"O"))
          f = carmel2_search_string (carmel2_search_to,&d,&n);
        break;
      case 'U':                 /* possible UN* */
        if (criteria[1] == 'N') {
          if (!strcmp (criteria+2,"ANSWERED")) f = carmel2_search_unanswered;
          else if (!strcmp (criteria+2,"DELETED")) f = carmel2_search_undeleted;
          else if (!strcmp (criteria+2,"FLAGGED")) f = carmel2_search_unflagged;
          else if (!strcmp (criteria+2,"KEYWORD"))
            f = carmel2_search_flag (carmel2_search_unkeyword,&d);
          else if (!strcmp (criteria+2,"SEEN")) f = carmel2_search_unseen;
        }
        break;
      default:                  /* we will barf below */
        break;
      }
      if (!f) {                 /* if can't determine any criteria */
        sprintf(carmel_error_buf,"Unknown search criterion: %s",criteria);
        mm_log (carmel_error_buf,ERROR);
        return;
      }
                                /* run the search criterion */
      for (i = 1; i <= stream->nmsgs; ++i)
        if (mail_elt (stream,i)->searched && !(*f) (stream,i,d,n))
          mail_elt (stream,i)->searched = NIL;
    }
                                /* report search results to main program */
    for (i = 1; i <= stream->nmsgs; ++i)
      if (mail_elt (stream,i)->searched) mail_searched (stream,i);
  }
}




/*----------------------------------------------------------------------
 * Carmel mail ping mailbox
 * Accepts: MAIL stream
 * Returns: T if stream alive, else NIL
 */

long
carmel2_ping (stream)
    MAILSTREAM *stream;
{
    struct stat sb;
    char path[1000], *mailfile;
#ifdef TESTING
    char debug_buf[1000];
#endif

    if(!stream->readonly &&
       carmel2_update_lock(stream->local, stream->mailbox, READ_LOCK) < 0) {
        /* Yuck! Someone messed up our lock file, this stream is hosed */
        mm_log("Mailbox updated unexpectedly! Mailbox closed", ERROR);
        stream->readonly = 1;
        return NIL;
    }

    /*--- First check to see if the Carmel index grew ----*/
    /* BUG, will this really work on NFS since the file is held open? */
    stat((*LOCAL->calc_paths)(CalcPathCarmel2Index, stream->mailbox, 0), &sb);
    if(sb.st_size > LOCAL->index_size) {
#ifdef TESTING
        mm_log("!!!!! Carmel index grew", NIL);
#endif
        /* Pull in the new mail.... */
#ifdef MAY_NEED_FOR_NFS
        /*---- First close and reopen the mail file -----*/
        fclose(LOCAL->index_stream);
        LOCAL->index_stream = fopen((*LOCAL->calc_paths)(CalcPathCarmel2Index,
                                                         stream->mailbox, 0),
                                    stream->readonly ? "r" : "r+");
        if(LOCAL->index_stream == NULL) {
            mm_log("Mailbox stolen. Mailbox closed", ERROR);
            stream->readonly = 1;
            return NIL;
        } 
#endif
        if(carmel2_parse_mail(stream, LOCAL->index_size) < 0) {
            mm_log("Mailbox corrupted. Mailbox closed", ERROR);
            stream->readonly = 1;
            return NIL;
        }
        LOCAL->index_size = sb.st_size;
    }

    if(sb.st_size < LOCAL->index_size) {
        /* Something bad happened, mail box shrunk on us, shutdown */
        stream->readonly = 1;
        mm_log("Mailbox shrank unexpectedly! Mailbox closed", ERROR);
        return NIL;
    }

#ifdef TESTING
    sprintf(debug_buf, "!!!!! Mailbox:\"%s\"  pretty_mailbox:\"%s\"",
            stream->mailbox, carmel_pretty_mailbox(stream->mailbox));
    mm_log(debug_buf, NIL);
    sprintf(debug_buf, "!!!! Readonly:%d, Carmel:%d", stream->readonly,
            LOCAL->carmel);
    mm_log(debug_buf, NIL);
#endif    

    if(!stream->readonly &&
       ((LOCAL->carmel &&
         strcmp(carmel_pretty_mailbox(stream->mailbox), "MAIL") == 0) ||
        strucmp2(carmel_pretty_mailbox(stream->mailbox), "inbox") == 0)) {
        /* If it's the inbox we've got to check the spool file */
        mailfile = getenv("MAIL") == NULL ? MAILFILE : getenv("MAIL");
        sprintf(path, mailfile, myhomedir());
#ifdef TESTING
        sprintf(debug_buf, "!!!!! Checking spool mail\"%s\"", path);
        mm_log(debug_buf, NIL);
#endif    
        if(stat(path, &sb) < 0)
          return(T);  /* No new mail...*/
        if(sb.st_size > 0) {
            mm_critical(stream);
            if(carmel2_lock(stream->local, stream->mailbox, WRITE_LOCK) < 0) {
                mm_nocritical(stream);
                return(T);
            }
            mm_log("!!!! Inbox locked, sucking in mail", NIL);
            carmel2_spool_mail(stream, path, stream->mailbox, 1); /*go get it*/
            carmel2_unlock(stream->local, stream->mailbox, WRITE_LOCK);
            mm_nocritical(stream);
            stat((*LOCAL->calc_paths)(CalcPathCarmel2Index, stream->mailbox,0),
                 &sb);
            LOCAL->index_size = sb.st_size;
        }
    } 
    
    return T;                   
}





/*----------------------------------------------------------------------
    This checks for new mail and checkpoints the mail file

Args -- The stream to check point

 ----*/
void 
carmel2_check(stream)
        MAILSTREAM *stream;
{
    (void)carmel2_check2(stream);
}



/*----------------------------------------------------------------------
    Do actual work of a check on carmel2 index, returning status

Returns: 0 if no checkpoint was done, 1 if it was done.
  ----*/
carmel2_check2(stream)
     MAILSTREAM *stream;
{
    int msgno;
    MESSAGECACHE *mc;

    if(stream->readonly || LOCAL == NULL)
      return(0); /* Nothing to do in readonly or closed case */

    carmel2_ping(stream); /* check for new mail (Ping always checks) */

    if(!LOCAL->dirty)
      return(0); /* Nothing to do */

    mm_critical(stream);
    if(carmel2_lock(stream->local, stream->mailbox, WRITE_LOCK) < 0) {
        mm_nocritical(stream);
        return(0); /* Couldn't get a write lock, nothing we can do */
    }

    for(msgno = 1; msgno <= stream->nmsgs; msgno++) {
        mc = MC(msgno);
        fseek(LOCAL->index_stream, mc->data1 +  carmel2_s_o_m_len  + 13, 0);
        if(fprintf(LOCAL->index_stream,
                   "U%c%c%c%c%c____________________________\n",
                   mc->flagged  ? 'F' : 'f',
                   mc->recent   ? 'R' : 'r',
                   mc->answered ? 'A' : 'a',
                   mc->deleted  ? 'D' : 'd',
                   mc->seen     ? 'S' : 's') == EOF) {
            /* BUG .. Need some error check here */
        }
    }
    fflush(LOCAL->index_stream);

    carmel2_unlock(stream->local, stream->mailbox, WRITE_LOCK);
    mm_nocritical(stream);
    mm_log("Check completed", NIL);
     return(1);
}
    




/*----------------------------------------------------------------------
  Carmel2 mail expunge mailbox

Args: stream  -- mail stream to expunge

 ----*/

void carmel2_expunge (stream)
        MAILSTREAM *stream;
{
    char        *index_file;
    long         msgno, new_msgno;
    FILE         *new_index;
    MESSAGECACHE *mc, *new_mc;
    ENVELOPE     *envelope;
    int           save_e;
    struct stat   sb;
    
    if (stream->readonly) {
        if(!stream->silent)
          mm_log ("Expunge ignored on readonly mailbox",NIL);
        return;
    }
    
    mm_critical(stream);
    carmel2_lock(stream->local, stream->mailbox, WRITE_LOCK);

    strcpy(carmel_path_buf,
           (*LOCAL->calc_paths)(CalcPathCarmel2Expunge, stream->mailbox, 0));

    new_index = fopen(carmel_path_buf, "w");
    if(new_index == NULL) {
        goto fail;
    }
    if(fprintf(new_index, carmel2_s_o_f) == EOF)
      goto fail;

    new_msgno = 1;
    for(msgno = 1; msgno <= stream->nmsgs; msgno++) {
        mc = MC(msgno);
        if(mc->deleted) {
            mm_expunged(stream, new_msgno);
            continue;
        }

        if(msgno != new_msgno) {
            new_mc        = MC(new_msgno);
            *new_mc       = *mc;
            new_mc->msgno = new_msgno;
            mc            = new_mc;
        }

        envelope = carmel2_fetchstructure(stream, msgno, NULL);
        if(envelope == NULL)
          goto fail;

        /* get this after envelope to offset is still valid in old file */
        mc->data1 = ftell(new_index); 

        if(carmel2_write_index(envelope, mc, new_index) < 0)
          goto fail;
        new_msgno++;
    }

    index_file = (*LOCAL->calc_paths)(CalcPathCarmel2Index, stream->mailbox, 0);

    /*--- Close it to make sure bits are really on disk across NFS. ---*/
    if(fclose(new_index) != EOF) {
        /*--- copy of index was a success, rename it ---*/
        unlink(index_file);
        link(carmel_path_buf, index_file); 

        /*--- Save the mail index size ----*/
        stat(index_file, &sb);
        LOCAL->index_size = sb.st_size;

        stream->nmsgs = new_msgno - 1;
        mm_exists(stream, stream->nmsgs);
        
        fclose(LOCAL->index_stream);
        LOCAL->index_stream = fopen(index_file, "r+");
    }
    unlink(carmel_path_buf); 
    carmel2_unlock(stream->local, stream->mailbox, WRITE_LOCK);
    mm_nocritical(stream);
    return;

  fail:
    save_e = errno;
    if(new_index != NULL)
      fclose(new_index);
    unlink(carmel_path_buf); 
    sprintf(carmel_error_buf, "Expunge failed: %s", strerror(save_e));
    mm_log(carmel_error_buf, WARN);
    carmel2_unlock(stream->local, stream->mailbox, WRITE_LOCK);
    mm_nocritical(stream);
    return;
}



/*----------------------------------------------------------------------
   Carmel2 mail copy message(s)

 Args: stream    - mail stream
       sequence  - message sequence
       mailbox   - destination mailbox, FQN

 Returns: T if copy successful, else NIL
  ----*/
long 
carmel2_copy(stream, sequence, mailbox)
        MAILSTREAM *stream;
        char       *sequence;
        char       *mailbox;
{
    ENVELOPE     *e;
    MESSAGECACHE *mc, new_mc;
    long          msgno, file_no, file_pos;
    int           fail;
    char         *file_name, *line;
    FILE         *dest_stream;
    struct stat   sb;

    if (!mail_sequence (stream,sequence)) /* get sequence to do */
      return(NIL);

    /*--- Get the file open (creating if needed) first ----*/
    file_name = (*LOCAL->calc_paths)(CalcPathCarmel2Index, mailbox, 0, 0);
    if(file_name == NULL) 
      return(NIL);

    if(stat(file_name, &sb) < 0) {
        mm_notify (stream,"[TRYCREATE] Must create mailbox before copy", NIL);
        return(NIL);
    }

    dest_stream = fopen(file_name, "a+");
    if(dest_stream == NULL) {
        sprintf(carmel_error_buf, "Can't copy message to %s: %s",
                mailbox, strerror(errno));
        mm_log(carmel_error_buf, ERROR);
        return(NIL);
    }

    mm_critical(stream);

    if(carmel2_lock(stream->local, mailbox, WRITE_LOCK) < 0) {
        mm_nocritical(stream);
        sprintf(carmel_error_buf,
                "Mailbox %s locked, can't copy messages to it",
                mailbox);
        mm_log(carmel_error_buf, ERROR);
        fclose(dest_stream);
        return(NIL);
    }


    /*--- Get the destination Carmel index open ----*/
    file_pos = ftell(dest_stream);
    fail = 0;


    for(msgno = 1; msgno <= stream->nmsgs; msgno++) {
        mc = MC(msgno);
        if(!mc->sequence)
          continue;

        new_mc = *mc;

        if(LOCAL->new_file_on_copy) {
            new_mc.data2 = carmel2_copy_msg_file(stream, mc->data2, mailbox);
            if((long)new_mc.data2 < 0) {
                fail = 1;
                break;
            }
        }

        e = carmel2_fetchstructure(stream, msgno, NULL);
        if(carmel2_write_index(e, &new_mc, dest_stream) < 0) {
            fail = 1;
            break;
        }

        if(LOCAL->carmel && LOCAL->aux_copy != NULL) {
            if((*LOCAL->aux_copy)(stream->local, mailbox, e, &new_mc)) {
                fail = 1;
                break;
            }
        }
    }

    if(fail) {
        ftruncate(fileno(dest_stream), file_pos);
    }

    fclose(dest_stream);

    carmel2_unlock(stream->local, mailbox, WRITE_LOCK);

    mm_nocritical(stream);

    return(fail ? NIL : T);
}



/*----------------------------------------------------------------------
   Carmel2 mail move message(s)


  Returns: T if move successful, else NIL
 ----*/

long
carmel2_move (stream,sequence,mailbox)
        MAILSTREAM *stream;
        char *sequence;
        char *mailbox;
{
  long          i;
  MESSAGECACHE *elt;

  if (!(mail_sequence (stream,sequence) &&
        carmel2_copy (stream,sequence,mailbox))) return NIL;
                                /* delete all requested messages */
  for (i = 1; i <= stream->nmsgs; i++)
    elt = MC(i);
    if (elt->sequence) {
      elt->deleted = T;         /* mark message deleted */
      LOCAL->dirty = T;         /* mark mailbox as dirty */
    }
  return T;
}



/*----------------------------------------------------------------------
 * Carmel2 garbage collect stream
 * Accepts: Mail stream
 *          garbage collection flags
 */

void carmel2_gc (stream, gcflags)
        MAILSTREAM *stream;
        long gcflags;
{
    /* No garbage collection in Carmel2 driver, not much to collect */
}



/*----------------------------------------------------------------------
    Handle MessageCache for carmel2 mail driver

Args: stream --
      msgno -- message number
      op    -- operation code

The carmel2 format keeps MESSAGECACHE entries in core for all messages
in the open mail folder so there isn't any cache flushing and rereading
that has to go on.
  ----*/
void *
carmel2_cache(stream, msgno, op)
        MAILSTREAM *stream;
        long msgno;
        long op;
{
  /* It's a carmel driver if first 6 letters of name are carmel */
  if(stream->dtb == NULL)
    return(0);

  if(struncmp2(stream->dtb->name, "carmel", 6) == 0) { 
    if(op == CH_MAKEELT)
      return(MC(msgno));
    else
      return(0);
  }

  /* Not a carmel2 or carmel driver, call the standard function. This works
     as long as there is only one other driver since we know it must be
     mm_cache().
   */
  return(mm_cache(stream, msgno, op));
}
        


/*----------------------------------------------------------------------
    Append a message to a mailbox

Args: mailbox -- The message to append the mail folder too
      message -- Message header and text in rfc822 \r\n format to append

Returns: T if all went OK, NIL if not
 ----*/
long
carmel2_append(stream, mailbox, flags, date, message)
     MAILSTREAM *stream;
     char       *mailbox, *flags, *date;
     STRING     *message;
{
    CARMEL2LOCAL local;

    /*---- A fake local data structure to pass to other functions---*/
    local.calc_paths = carmel2_calc_paths;
    local.carmel     = 0;
    local.aux_copy   = NULL;

    return(carmel2_append2(stream, &local, mailbox, flags, date, message));
}
      



/*----------------------------------------------------------------------
    Fetch the body structure for a camel message

Args: stream -- MAILSTREAM
      mc     -- The incore message cache entry for the message

Returns: body structure

Note, the envelope that results from the parse here is ignored. Only
the envelope from the index is actually used in the Carmel2 format.
 ----*/
static BODY *
carmel2_bodystruct(stream, mc)
     MESSAGECACHE *mc;
     MAILSTREAM   *stream;
{
    char     *header, *text, *file, *p;
    ENVELOPE *e_place_holder;
    BODY     *b;
    STRING    string_struct;

    header = carmel2_readmsg(stream, 1,  mc->rfc822_size, mc->data2);
    if(header == NULL)
      return(NULL);

    text = carmel2_readmsg(stream, 0, mc->rfc822_size, mc->data2);
    if(text == NULL)
      return(NULL);

    INIT(&string_struct, mail_string, (void *)text, strlen(text));
    rfc822_parse_msg(&e_place_holder, &b, header, strlen(header),
                     &string_struct, mylocalhost(), carmel_20k_buf);

    mail_free_envelope(&e_place_holder);

#ifdef BWC 
    /* If there's no content type in the header and it's the carmel
       driver at the BWC set the type X-BWC-Glyph
     */
    for(p = header; *p; p++) 
      if(*p=='\n' && (*(p+1)=='C' || *(p+1)=='c') &&
        struncmp2(p+1,"content-type:", 13) == 0)
          break;

    if(!*p && LOCAL->carmel &&    /* Carmel, not Carmel2 */
       b->type == TYPETEXT &&     /* Type text */
       (b->subtype == NULL || strcmp(b->subtype,"PLAIN") == 0) && 
       ((int)mc->year) + 1969 < 1994) {
        /* Most mail in a pod mail store is in the BWC-Glyph character
           set, but there is no tag in the data on disk, so we fake it here.
           We expect after 1994 all mail generated in BWC-Glyph format
           will have proper tags!
         */
        b->subtype = cpystr("X-BWC-Glyph");
    }
#endif 

    return(b);
}



/*----------------------------------------------------------------------
    Parse an address in a Carmel2 format mail index

Args: line  -- The line from the index to parse
      addr  -- The address list to add to (if there is one)

Returns: address list
 ----*/
static ADDRESS *
carmel2_parse_address(line, addr, localhost)
     char    *line, *localhost;
     ADDRESS *addr;
{
    ADDRESS *a;

    if(addr == NULL) {
        addr = mail_newaddr();
        a = addr;
    } else {
        for(a = addr; a!= NULL && a->next != NULL; a = a->next);
        a->next = mail_newaddr();
        a = a->next;
    }

    line++;  /* Skip the tag chacater */
    a->personal = carmel2_parse_addr_field(&line);
    a->mailbox  = carmel2_parse_addr_field(&line);
    a->host     = carmel2_parse_addr_field(&line);
    a->adl      = carmel2_parse_addr_field(&line);
/*    if(a->host == NULL)
      a->host = cpystr(localhost); */ /* host can't be NULL */
      /* Yes it can for Internet group syntax */
    return(addr);
}



/*----------------------------------------------------------------------
   Parse the next address field out of a carmel address index entry

Args: string -- pointer to pointer to string

Returns: field in malloced string or NULL

Input string is a bunch of substrings separated by ^A. This function scans 
for the next ^A or end of string, cuts it out and returns it. The original
strings passed in is mangled
----*/
static char *
carmel2_parse_addr_field(string)
     char **string;
{
    char *p, end, *start;

    start = p  = *string;
    while(*p > '\001')  /* Find end of sub string or string */
      p++;
    if((p - *string) == 0) {
        if(*p) p++;
        *string = p;
        return(NULL); /* If nothing found return nothing */
    }
    end = *p;       /* Save terminating character (^A or \0) */
    *p = '\0';     
    if(end)         /* If not end of string, get ready for next call */
      p++;
    *string = p;    /* Return pointer to next substring */
    return(cpystr(start));
}


    
    
/*----------------------------------------------------------------------
     Write an entry into a carmel2 index

Args: e      -- Envelope
      mc     -- Message Cache element
      stream -- File stream to write to

Returns: 0 if OK, -1 if failed
----*/
carmel2_write_index(e, mc, stream)
     ENVELOPE     *e;
     MESSAGECACHE *mc;
     FILE         *stream;
{
    long f_start, f_end;

    f_start = ftell(stream);

    if(fprintf(stream, "%s\007\001xxxxxxxxxx\n", carmel2_s_o_m) == EOF)
      goto blah;
    if(fprintf(stream, "U%c%c%c%c%c____________________________\n",
            mc->flagged  ? 'F' : 'f',
            mc->recent   ? 'R' : 'r',
            mc->answered ? 'A' : 'a',
            mc->deleted  ? 'D' : 'd',
            mc->seen     ? 'S' : 's') == EOF)
      goto blah;
    if(fprintf(stream, "Z%d\n", mc->rfc822_size) == EOF)
      goto blah;
    if(fprintf(stream, "D%d\001%d\001%d\001%d\001%d\001%d\001%d\001%d\n",
            mc->year+1969, mc->month, mc->day, mc->hours, mc->minutes,
            mc->seconds, mc->zhours * (mc->zoccident ? 1 : -1),
               mc->zminutes) == EOF)
       goto blah;
    if(fprintf(stream, "Svmail\n") == EOF)
       goto blah;
    if(fprintf(stream, "P%d\n",mc->data2) == EOF)
       goto blah;
    if(carmel2_index_address(e->to,   'T', stream) < 0)
       goto blah;
    if(carmel2_index_address(e->from, 'F', stream) < 0)
       goto blah;
    if(carmel2_index_address(e->cc,   'C', stream) < 0)
       goto blah;
    if(carmel2_index_address(e->bcc,  'B', stream) < 0)
       goto blah;
#ifdef HAVE_RESENT
    if(carmel2_index_address(e->resent_to,   't', stream) < 0)
       goto blah;
    if(carmel2_index_address(e->resent_from, 'f', stream) < 0)
       goto blah;
    if(carmel2_index_address(e->resent_cc,   'c', stream) < 0)
       goto blah;
    if(carmel2_index_address(e->resent_bcc,  'b', stream) < 0)
       goto blah;
#endif
    if(carmel2_index_address(e->return_path, 'H', stream) < 0)
       goto blah;
    if(carmel2_index_address(e->sender,      'E', stream) < 0)
       goto blah;
    if(carmel2_index_address(e->reply_to,    'R', stream) < 0)
       goto blah;
    if(e->in_reply_to != NULL)
      if(fprintf(stream, "L%s\n", e->in_reply_to) == EOF)
       goto blah;
    if(e->remail != NULL)
      if(fprintf(stream, "r%s\n", e->remail) == EOF)
       goto blah;
    if(e->message_id != NULL)
      if(fprintf(stream, "I%s\n", e->message_id) == EOF)
       goto blah;
    if(e->newsgroups != NULL)
      if(fprintf(stream, "N%s\n", e->newsgroups) == EOF)
       goto blah;
    if(e->subject != NULL)
      if(fprintf(stream, "J%s\n", e->subject) == EOF)
        goto blah;

    /*--- figure out and write the offset ---*/
    f_end = ftell(stream);
    if(fseek(stream, f_start, 0) < 0)
      goto blah;
    if(fprintf(stream, "%s\007\001%10d\n", carmel2_s_o_m, f_end - f_start)==EOF)
      goto blah;
    if(fseek(stream, f_end, 0) < 0)
      goto blah;

    return(0);

  blah:
    /* Index entry is a bit of a mess now. Maybe we should try to truncate */
    return(-1);
}



/*----------------------------------------------------------------------
    Write an address entry into a carmel2 index

Args: addr   -- addresslist
      field  -- Field character specifier
      stream -- File stream to write to

Returns 0 if OK, -1 on error writing
 ---*/ 
static
carmel2_index_address(addr, field, stream)
    ADDRESS *addr;
    int      field;
    FILE    *stream;
{
    ADDRESS *a;

    for(a = addr; a != NULL; a = a->next) {
        if(fprintf(stream, "%c%s\001%s\001%s\001%s\n",
                   field,
                   a->personal == NULL ? "" : a->personal,
                   a->mailbox  == NULL ? "" : a->mailbox,
                   a->host     == NULL ? "" : a->host,
                   a->adl      == NULL ? "" : a->adl) == EOF)
          return(-1);
    }
    return(0);
}
    


/*----------------------------------------------------------------------
   Real work of reading mail data files

Args: stream 
      header_only -- return header if set, text if not
      file_size   -- The file size if known (saves a stat)
      file        -- name of the file to read

Returns buffer with text stored in internel buffer. The Carmel2 format
buffers the text of the current message and header in an internal
buffer. The buffer never shrinks and is expanded as needed, up to a
maximum. The text in the buffer is in CRLF format and is read in line
by line using stdio. It is believed this will be efficient on whatever
machine it is running on and will not use too much memory.  (There's
some extra memory used for buffering in stdio.) If a request is made
first for only the header, then only the header will be read in.  This
is a big efficiency win when the file is large and only the header is
needed. (In the Carmel2 format the header is genera lly not used, and
when it is it is with the body to do a MIME parse, but the pod format
does read the headers in to create the Carmel2 index.) An estimate is
made of the size needed to expand the file to convert the line ends
from LF to CRLF. The estimate alloca tes 10% extra space. If it
doesn't fit, the whole buffer will be expanded by 50% and the whole
read done over. When the header is read in a 30K buffer is allocated
initially (if the buffer is smaller than that initially). The 50%
increase is applied to the buffer when reading only the header.
  ----*/

char *
carmel2_readmsg(stream, header_only, file_size, file_num)
     MAILSTREAM *stream;
     int         header_only;
     int         file_num;
     long        file_size;
{
    FILE       *msg_stream;
    char       *p, *p_end, *file_name;
    int         past_header, not_eof;
    long        max_read;
    struct stat st;
#define DEBUGDEBUG 1
#ifdef DEBUGDEBUG
    char        debug_buf[500];
#endif

    file_name = (*LOCAL->calc_paths)(CalcPathCarmel2Data,
                                     stream->mailbox, file_num);
    if(file_name == NULL)
      return(NULL); /* Just in case; should never be invalid if open */
#ifdef DEBUGDEBUG
    sprintf(debug_buf, "READ RQ:\"%s\" HV:\"%s\"  RQ_HD:%d  HV_TXT:%d\n",
            file_name,
            LOCAL->buffered_file == NULL ? "" : LOCAL->buffered_file,
            header_only, LOCAL->buffered_header_and_text);
    mm_log(debug_buf, NIL);
#endif

    /*---- Check out what we have read already -----*/
    if(LOCAL->buffered_file != NULL &&
       strcmp(LOCAL->buffered_file, file_name) == 0) {
        /* The file is the same. Now have we read in the part
           that is wanted? If so return it.
         */
        if(header_only || LOCAL->buffered_header_and_text) {
            if(header_only) {
#ifdef DEBUGDEBUG
                mm_log("....Returning buffered header\n", NIL);
#endif
                return(LOCAL->msg_buf);
            } else {
#ifdef DEBUGDEBUG
                mm_log("....Returning buffered text\n", NIL);
#endif
                return(LOCAL->msg_buf_text_start);
            }
        }
    } else {
        /*-- Different file than last time, reset a few things --*/
        LOCAL->buffered_header_and_text = 0;
        LOCAL->msg_buf_text_offset      = 0L;
        if(LOCAL->buffered_file != NULL)
          fs_give((void **)&(LOCAL->buffered_file));
    }

#ifdef DEBUGDEBUG
     mm_log("....Reading file\n", NIL);
#endif

    /*----- Open the file ------*/
    /* Would actually be more efficient not to use stdio here */
    msg_stream = fopen(file_name, "r");
    if(msg_stream == NULL) {
        strcat(file_name, ".wid");
        msg_stream = fopen(file_name, "r");
        if(msg_stream == NULL)
           return(NULL);
    }

    /*---- Check the size of the file ----*/
    if(file_size == 0 && stat(file_name, &st) >= 0)
      file_size = st.st_size;


    /* ------Pick an amount to read -------
       Assume the header is less than MAX_HEADER. We don't want to
       allocate buffer for the whole message if we are just getting
       the header. 
     */
    max_read    = (file_size * 11) / 10;
    past_header = 0;
    p           = LOCAL->msg_buf;
    if(header_only) {
        max_read = min(max_read, CARMEL_MAX_HEADER);
    } else if(LOCAL->msg_buf_text_offset > 0) {
        past_header = 1;
        p = LOCAL->msg_buf_text_start;
        fseek(msg_stream, LOCAL->msg_buf_text_offset, 0);
    }
    if(max_read > CARMEL_MAXMESSAGESIZE)
      max_read = CARMEL_MAXMESSAGESIZE;
    if(max_read == 0)
      max_read = 1;  /* Forces buffer allocation below */


    /*----- Loop (re)reading the whole file 'till it fits ---*/
    /* This will fit the first time for all but the 
       strangest cases.
     */
    do {
        /*---- Make sure buffer is the right size ----*/              
        if(LOCAL->msg_buf_size < max_read) {
            /* Buffer not big, enough start whole read over */
            if(LOCAL->msg_buf != NULL)
              fs_give((void **)&(LOCAL->msg_buf));
            LOCAL->msg_buf      = fs_get(max_read);
            LOCAL->msg_buf_size = max_read;
            fseek(msg_stream, 0, 0);
            past_header = 0;
            p = LOCAL->msg_buf;
        }
    
        p_end = LOCAL->msg_buf + LOCAL->msg_buf_size - 3;

        while(p < p_end && (not_eof =(fgets(p, p_end-p, msg_stream) != NULL))){
            if(*p == '\n' && !past_header) {
                *p++ = '\r';
                *p++ = '\n';
                *p++ = '\0';
                past_header = 1;
                LOCAL->msg_buf_text_offset = ftell(msg_stream);
                LOCAL->msg_buf_text_start  = p;
                if(header_only)
                  goto done;
                else
                  continue;
            }
            p += strlen(p) - 1;    
            *p++ = '\r';
            *p++ = '\n';  
        }
        *p = '\0';
        if(!not_eof)
          goto done;

        /* If we're here, the buffer wasn't big enough, which
           is due to a message with most lines less than 10 characters 
           (the 10% addition for turning LF to CRLF wasn't enough).
           Increase it by 50% and try again, till we hit
           the largest message we can read 
          */
        max_read = min(CARMEL_MAXMESSAGESIZE, (max_read * 15) / 10);
        fseek(msg_stream, 0, 0);
    } while (1);
  done:
    if(p == p_end) 
      mm_log("Message truncated. It's is too large", WARN);

    fclose(msg_stream);

    /*---- Save the name of the file buffered file ----*/
    LOCAL->buffered_file             = cpystr(file_name);
    LOCAL->buffered_header_and_text |= !header_only;

    return(header_only ? LOCAL->msg_buf : LOCAL->msg_buf_text_start);
}


/*----------------------------------------------------------------------
    Parse basic/quick entries in a Carmel mailbox

Args: stream -- stream for mailbox
      file_pos -- File position in the index to start parsing at

Returns:  0 if parse succeeds
         -1 if it fails
 ----*/
static int
carmel2_parse_mail(stream, file_pos)
     MAILSTREAM *stream;
     long        file_pos;
{
    MESSAGECACHE *mc;
    long          nmsgs, next, last_line_read;
    int           found;

    nmsgs = stream->nmsgs;

    /*---- Get the start-of-message record ------*/
    fseek(LOCAL->index_stream, file_pos, 0);
    if(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),LOCAL->index_stream)==NULL)
       goto done_reading;
    if(strncmp(carmel_20k_buf, carmel2_s_o_m, carmel2_s_o_m_len) != 0)
      goto bomb;

    while(1) {
        if(strlen(carmel_20k_buf) != carmel2_s_o_m_len + 13)
          goto bomb;

        nmsgs++;
        next      = atol(carmel_20k_buf+24);
        mc        = carmel2_new_mc(stream, nmsgs);
        mc->data1 = file_pos;

        /*-- Get the status line, It must be the first line in the entry ----*/
        if(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),
                 LOCAL->index_stream) == NULL)
          goto done_reading;
        if(*carmel_20k_buf != 'U' || strlen(carmel_20k_buf) != 35)
          goto bomb; /* Invalid format! */
        mc->flagged  = carmel_20k_buf[1] == 'F';
        mc->answered = carmel_20k_buf[3] == 'A';
        mc->deleted  = carmel_20k_buf[4] == 'D';
        mc->seen     = carmel_20k_buf[5] == 'S';
        mc->recent   = 0;

        /*---- Get the rest of the other interesting entries -----*/
        found = 0;
        while(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),
                    LOCAL->index_stream) != NULL &&
              found < 3 &&
              strncmp(carmel_20k_buf, carmel2_s_o_m, carmel2_s_o_m_len))
          if (*carmel_20k_buf == 'Z') {
              mc->rfc822_size = atol(carmel_20k_buf+1);
              found++;
          } else if(*carmel_20k_buf == 'D') {
              carmel2_parse_date(mc, carmel_20k_buf+1);
              found++;
          } else if(*carmel_20k_buf == 'P') {
              mc->data2 = atoi(carmel_20k_buf+1);
              found++;
          }

        /*-------- Now find the next index entry ---------*/
        last_line_read = ftell(LOCAL->index_stream);
        file_pos += next;
        fseek(LOCAL->index_stream, file_pos, 0); /* try the offset first */
        if(fgets(carmel_20k_buf, sizeof(carmel_20k_buf),
                 LOCAL->index_stream) == NULL) 
           break;
        if(strncmp(carmel_20k_buf, carmel2_s_o_m, carmel2_s_o_m_len) != 0){
            /*-- Didn't match what was seeked to, back off and read lines --*/ 
            fseek(LOCAL->index_stream, last_line_read, 0);
            do {
                file_pos = ftell(LOCAL->index_stream);
                if(fgets(carmel_20k_buf, sizeof(carmel_20k_buf),
                         LOCAL->index_stream) == NULL)
                  goto done_reading;
            }while(strncmp(carmel_20k_buf,carmel2_s_o_m,carmel2_s_o_m_len)!=0);
        }
    }
  done_reading:
    if(stream->nmsgs != nmsgs) {
        stream->nmsgs = nmsgs;
        mm_exists(stream, nmsgs);
    }
    return(0);

  bomb:
   return(-1);
}



/* This killer macro is from bezerk.h. It's only needed at sites that don't
 * escape "From " lines with ">From " unless absolutely necessary (The UW).
 */

/* Validate line known to start with ``F''
 * Accepts: pointer to candidate string to validate as a From header
 *          return pointer to end of date/time field
 *          return pointer to offset from t of time (hours of ``mmm dd hh:mm'')
 *          return pointer to offset from t of time zone (if non-zero)
 * Returns: T if valid From string, t,ti,zn set; else NIL
 */

#define VALID(s,x,ti,zn) \
  (s[1] == 'r') && (s[2] == 'o') && (s[3] == 'm') && (s[4] == ' ') && \
  (x = strchr (s+5,'\n')) && \
  ((x-s < 41) || ((ti = ((x[-2] == ' ') ? -14 : (x[-3] == ' ') ? -15 : \
                         (x[-4] == ' ') ? -16 : (x[-5] == ' ') ? -17 : \
                         (x[-6] == ' ') ? -18 : (x[-7] == ' ') ? -19 : \
                         (x[-8] == ' ') ? -20 : (x[-9] == ' ') ? -21 : \
                         (x[-10]== ' ') ? -22 : (x[-11]== ' ') ? -23 : 0)) && \
                  (x[ti]   == ' ') && (x[ti+1] == 'r') && (x[ti+2] == 'e') && \
                  (x[ti+3] == 'm') && (x[ti+4] == 'o') && (x[ti+5] == 't') && \
                  (x[ti+6] == 'e') && (x[ti+7] == ' ') && (x[ti+8] == 'f') && \
                  (x[ti+9] == 'r') && (x[ti+10]== 'o') && (x[ti+11]== 'm') && \
                  (x += ti)) || T) && \
  (x-s >= 27) && \
  ((x[ti = -5] == ' ') ? ((x[-8] == ':') ? !(zn = 0) : \
                          ((x[ti = zn = -9] == ' ') || \
                           ((x[ti = zn = -11] == ' ') && \
                            ((x[-10] == '+') || (x[-10] == '-'))))) : \
   ((x[zn = -4] == ' ') ? (x[ti = -9] == ' ') : \
    ((x[zn = -6] == ' ') && ((x[-5] == '+') || (x[-5] == '-')) && \
     (x[ti = -11] == ' ')))) && \
  (x[ti - 3] == ':') && (x[ti -= ((x[ti - 6] == ':') ? 9 : 6)] == ' ') && \
  (x[ti - 3] == ' ') && (x[ti - 7] == ' ') && (x[ti - 11] == ' ')


/* You are not expected to understand this macro, but read the next page if
 * you are not faint of heart.
 *
 * Known formats to the VALID macro are:
 *              From user Wed Dec  2 05:53 1992
 * BSD          From user Wed Dec  2 05:53:22 1992
 * SysV         From user Wed Dec  2 05:53 PST 1992
 * rn           From user Wed Dec  2 05:53:22 PST 1992
 *              From user Wed Dec  2 05:53 -0700 1992
 *              From user Wed Dec  2 05:53:22 -0700 1992
 *              From user Wed Dec  2 05:53 1992 PST
 *              From user Wed Dec  2 05:53:22 1992 PST
 *              From user Wed Dec  2 05:53 1992 -0700
 * Solaris      From user Wed Dec  2 05:53:22 1992 -0700
 *
 * Plus all of the above with `` remote from xxx'' after it. Thank you very
 * much, smail and Solaris, for making my life considerably more complicated.
 */

/*
 * What?  You want to understand the VALID macro anyway?  Alright, since you
 * insist.  Actually, it isn't really all that difficult, provided that you
 * take it step by step.
 *
 * Line 1       Validates that the 2-5th characters are ``rom ''.
 * Line 2       Sets x to point to the end of the line.
 * Lines 3-12   First checks to see if the line is at least 41 characters long.
 *              If so, it scans backwards up to 10 characters (the UUCP system
 *              name length limit due to old versions of UNIX) to find a space.
 *              If one is found, it backs up 12 characters from that point, and
 *              sees if the string from there is `` remote from''.  If so, it
 *              sets x to that position.  The ``|| T'' is there so the parse
 *              will still continue.
 * Line 13      Makes sure that there are at least 27 characters in the line.
 * Lines 14-17  Checks if the date/time ends with the year.  If so, It sees if
 *              there is a colon 3 characters further back; this would mean
 *              that there is no timezone field and zn is set to 0 and ti is
 *              left in front of the year.  If not, then it expects there to
 *              either to be a space four characters back for a three-letter
 *              timezone, or a space six characters back followed by a + or -
 *              for a numeric timezone.  If a timezone is found, both zn and
 *              ti are the offset of the space immediately before it.
 * Lines 18-20  Are the failure case for a date/time not ending with a year in
 *              line 14.  If there is a space four characters back, it is a
 *              three-letter timezone; there must be a space for the year nine
 *              characters back.  Otherwise, there must be a space six
 *              characters back and a + or - five characters back to indicate a
 *              numeric timezone and a space eleven characters back to indicate
 *              a year.  zn and ti are set appropriately.
 * Line 21      Make sure that the string before ti is of the form hh:mm or
 *              hh:mm:ss.  There must be a colon three characters back, and a
 *              space six or nine characters back (depending upon whether or
 *              not the character six characters back is a colon).  ti is set
 *              to be the offset of the space immediately before the time.
 * Line 22      Make sure the string before ti is of the form www mmm dd.
 *              There must be a space three characters back (in front of the
 *              day), one seven characters back (in front of the month), and
 *              one eleven characters back (in front of the day of week).
 *
 * Why a macro?  It gets invoked a *lot* in a tight loop.  On some of the
 * newer pipelined machines it is faster being open-coded than it would be if
 * subroutines are called.
 *
 * Why does it scan backwards from the end of the line, instead of doing the
 * much easier forward scan?  There is no deterministic way to parse the
 * ``user'' field, because it may contain unquoted spaces!  Yes, I tested it to
 * see if unquoted spaces were possible.  They are, and I've encountered enough
 * evil mail to be totally unwilling to trust that ``it will never happen''.
 */


/*----------------------------------------------------------------------
    Get the new mail out of the spooled mail file
  
 Args: stream  -- The inbox stream to add mail too
       spool   -- The path name of the spool file
       mailbox -- Name user sees for this, used only for error reporting

 Result:

 - Lock the spool mail file with bezerk_lock
 - Get the carmel2 index open and remember where we started in it
 - Make buffer big enough for biggest header we'll mess with
 - Loop reading all the message out of the spool file:
   - Get a new data file for the message and open it
   - Read the header of the message into the big buffer while...
    - writing the message into the new data file
   - finish writing the text of the message into the data file
   - If all went well bump the message count and say it exists
   - Parse the header of the message to get an envelope, date and size
   - Write an entry into the carmel2 index
 - Unlink and create (to zero) the spool file and unlock it

The carmel inbox should be locked when this is called and mm_critical
should be called around this function.
 ----*/
void
carmel2_spool_mail(stream, spool, mailbox, clear_spool_file) 
     MAILSTREAM *stream;
     char       *spool, *mailbox;
     int         clear_spool_file;
{
    char         *p, *eof, *from_p;
    int           n, size, fd, in_header, from_i1, from_i2;
    long          file_pos, index_file_pos, byte_count, start_of_append;
    FILE         *spool_stream, *data_stream;
    ENVELOPE     *envelope;
    BODY         *b;
    MESSAGECACHE *mc;
    STRING        string_struct;
#ifdef BWC
    int           is_wide;
#endif    

    /*--- Get the locks set and files open-----*/
    fd = carmel2_bezerk_lock(spool, mailbox);
    if(fd < 0) {
        return;
    }
    spool_stream = fdopen(fd, "r");
    fseek(LOCAL->index_stream, 0L, 2);
    start_of_append = ftell(LOCAL->index_stream);

    /*--- Make buffer big enough for largest allowable header ----*/
    if(LOCAL->msg_buf_size < CARMEL_MAX_HEADER) {
        if(LOCAL->msg_buf != NULL)
          fs_give((void **)&(LOCAL->msg_buf));
        LOCAL->msg_buf_size = CARMEL_MAX_HEADER;
        LOCAL->msg_buf = fs_get(CARMEL_MAX_HEADER);
    }
    LOCAL->buffered_header_and_text = 0;
    LOCAL->msg_buf_text_offset      = 0L;
    if(LOCAL->buffered_file != NULL)
      fs_give((void **)&(LOCAL->buffered_file));


    /*---- Read (and ignore) the first line with the "From " in it ----*/
    eof = fgets(carmel_20k_buf, sizeof(carmel_20k_buf), spool_stream); 

    /*----- Loop getting messages ----*/
    while(eof != NULL) {

        /*----- get a data file for the new message ----*/
        n = carmel2_new_data_file(stream->local, stream->mailbox);
        data_stream = fopen((*LOCAL->calc_paths)(CalcPathCarmel2Data,
                                                 stream->mailbox, n),"w");
        if(data_stream == NULL)
          goto backout;
        file_pos  = ftell(spool_stream);
        p         = LOCAL->msg_buf;
        in_header = 1;
        byte_count = 0L;
#ifdef BWC
        is_wide = 0;
#endif
        

        /*--------------------------------------------------------------------
            Read the message in line by line, writing it out to the
            new data file. Also acculamuate a copy of the header in
            a buffer for parsing
          ---*/
        eof = fgets(carmel_20k_buf, sizeof(carmel_20k_buf), spool_stream);
        while(eof != NULL){
            if(VALID(carmel_20k_buf, from_p, from_i1, from_i2))
              break;
    
            if(in_header) {
#ifdef BWC
                is_wide |= carmel_match_glyph_wide(carmel_20k_buf);
#endif
                if(*carmel_20k_buf == '\n') {
                    /* Encountered first blank line, end of header */
                    in_header = 0;
                    *p = '\0';
                } else {
                    if(p - LOCAL->msg_buf + strlen(carmel_20k_buf) >
                       LOCAL->msg_buf_size){
                        /* out of room in buffer, end it */
                        in_header = 0;
                        *p = '\0';
                    } else {
                        strcpy(p, carmel_20k_buf);
                        p +=strlen(p);
                    }
                }
            }

            /*----- Write the message into the file -----*/
            byte_count += strlen(carmel_20k_buf);
            if(carmel_20k_buf[0] == '>' && carmel_20k_buf[1] == 'F' &&
               carmel_20k_buf[2] == 'r' && carmel_20k_buf[3] == 'o' &&
               carmel_20k_buf[4] == 'm' && carmel_20k_buf[5] == ' ')  {
                if(fputs(carmel_20k_buf + 1, data_stream) == EOF)
                  goto backout;
                byte_count -= 1;
            } else {
                if(fputs(carmel_20k_buf, data_stream) == EOF)
                  goto backout;
            }
            eof = fgets(carmel_20k_buf, sizeof(carmel_20k_buf), spool_stream);
        }
        fclose(data_stream);
#ifdef BWC
        if(is_wide) {
            sprintf(carmel_path_buf, "%s.wid",
                    (*LOCAL->calc_paths)(CalcPathCarmel2Data,stream->mailbox,n)
                    );
            rename((*LOCAL->calc_paths)(CalcPathCarmel2Data,stream->mailbox,n),
                   carmel_path_buf);
        }
#endif

        /*---- get a new MESSAGECACHE to store it in -----*/
        mc = carmel2_new_mc(stream, stream->nmsgs + 1);

        /*------ Parse the message header ------*/
        INIT(&string_struct, mail_string, (void *)"", 0);
        rfc822_parse_msg(&envelope, &b, LOCAL->msg_buf, strlen(LOCAL->msg_buf),
                         &string_struct, mylocalhost(), carmel_20k_buf);
        carmel2_parse_bezerk_status(mc, LOCAL->msg_buf);
        carmel2_rfc822_date(mc, LOCAL->msg_buf);
        mc->rfc822_size = byte_count; 
        mc->data2 = n;
        mc->data1 = ftell(LOCAL->index_stream);
        mc->recent = 1;

        /*----- Now add the message to the Carmel2 index ------*/
        if(carmel2_write_index(envelope, mc, LOCAL->index_stream) < 0)
          goto backout;

        /*----- Write message into auxiliary index (plain carmel) ----*/
        if(LOCAL->carmel && LOCAL->aux_copy != NULL) {
            if((*LOCAL->aux_copy)(stream->local, mailbox, envelope, mc)) {
                /* BUG - this error may leave things half done, but will
                   only result in duplicated mail */
                goto backout; 
            }
        }

        /*---- All went well, let the user know -----*/
        stream->nmsgs++;
        mm_exists(stream, stream->nmsgs);

        mail_free_envelope(&envelope);
    }

    fflush(LOCAL->index_stream); /* Force new index entries onto disk */
    fclose(spool_stream);
    if(clear_spool_file) {
        unlink(spool);
        close(creat(spool, 0600));
    }
    carmel2_bezerk_unlock(fd, spool);
    return;

  backout:
    sprintf(carmel_error_buf, "Error incorporating new mail into \"%s\": %s",
            carmel_parse_mb_name(mailbox,'\0')->mailbox, strerror(errno));
    /* bug in above call to parse_mb -- should have version passed in */
    mm_log(carmel_error_buf, ERROR);
    fflush(LOCAL->index_stream);
    ftruncate(fileno(LOCAL->index_stream), start_of_append);
    carmel2_bezerk_unlock(fd, spool);
}



/*----------------------------------------------------------------------
   Copy the actual data file when copying a message

Returns: -1 for failure
            otherwise the number of the new file, 

  ----*/        
static  
carmel2_copy_msg_file(stream, orig_file_number, new_mailbox)
     MAILSTREAM *stream;
     int         orig_file_number;
     char       *new_mailbox;
{
    char *file_name;
    int   wide, e, new_file_num, n, orig_fd, new_fd;

    /*---- Open the orginal file ----*/
    wide = 0;
    file_name = (*LOCAL->calc_paths)(CalcPathCarmel2Data,
                                     new_mailbox,orig_file_number);
    if(file_name == NULL)
      return(-1);

    orig_fd = open(file_name, O_RDONLY);
    if(orig_fd < 0 && LOCAL->carmel) {
        strcat(file_name, ".wid");
        orig_fd = open(file_name, O_RDONLY);
        if(orig_fd < 0)
          goto bomb;
        wide = 1;
    } else {
        goto bomb;
    }

    /*----- Open and create the new file ------*/
    new_file_num = carmel2_new_data_file(stream->local, new_mailbox);
    if(new_file_num < 0)
      goto bomb;
    file_name = (*LOCAL->calc_paths)(CalcPathCarmel2Data,
                                     new_mailbox, new_file_num);
    if(wide)
      strcat(file_name, ".wid");
    new_fd = open(file_name, O_WRONLY | O_CREAT, 0600);
    if(new_fd < 0) {
        goto bomb;
    }

    /*---- Copy the bits ------*/
    e = 0;
    while((n = read(orig_fd, carmel_20k_buf, sizeof(carmel_20k_buf))) >0) {
        if(write(new_fd, carmel_20k_buf, n) < 0) {
            e = errno;
            break;
        }
    }

    /*---- Close the streams and handle any errors ----*/
    close(orig_fd);
    close(new_fd);

    if(e == 0)
      return(new_file_num);  /* All is OK */
    
    /*--- something didn't go right ---*/
  bomb:
    unlink(file_name);
    sprintf(carmel_error_buf, "Error copying message to %s: %s",
            new_mailbox, strerror(errno));
    mm_log(carmel_error_buf, ERROR);
    return(-1);
}






/*----------------------------------------------------------------------
       Locking for Carmel and Pod formats

Args: stream --  Mail stream we're operating on
      file   --  Mail file name
      write  --  Flag indicating we want write access

Retuns: -1 if lock fails, 0 if it succeeds

- There are two locks. Plain locks and write locks. They are created
  about the same way, but have different names. The time out on the write
  locks is much shorter, because it should never be held for very long.

- Hitching (links in file system) post locking is used so it will work
  across NFS. Flock() could be used as it has two advantages. First it
  is more efficient, second the locks will disolve automatically when the 
  process dies. The efficiency is not of great concern, and the
  process should not (theoretically) die unless it crashes due to a bug
  or it is abnormally terminated. The advantage of locking across NFS
  is considered greater than the advantages of flock().

- The mod time of the lock file is updated everytime mail_check()
  or mail_ping() is called and the mod time on the lock file is recorded.
  This is so it can be determined that the lock file is current.

- When a read lock file over 30 or a write lock over 5 minutes old is
  encountered it is assumed the lock is old and should be overridden
  (because the process crashed or was killed). 

- Everytime the mod time on the lock file is updated (on calls to
  mail_check() and mail_ping()), the mod time of the lock file is
  checked against the record of what it was last set to. If the mod times 
  don't match the lock has been broken and overridden. Then the original
  Pine should go into read-only mode.... This is only likely to happen if
  someone suspends (^Z's) the process for more than 30 minutes, and another
  process is started.
  ----*/
int
carmel2_lock(local, file, write)
     CARMEL2LOCAL *local;
     char         *file;
     int           write;
{
    char        *hitch, lock[CARMEL_PATHBUF_SIZE], error_mess[200], *l_path;
    struct stat sb;
    int         n, link_result, hitch_fd, timer_set, l;
    long        override, timer;

    /* Set the length of time for override. It is 5 minutes for a 
       write lock (no process should have it for that long) and
       30 minutes for a read lock, that's 30 minutes since the
       lock file was last touched. */
    override = 60 * (write ? 5 : 30);
    timer    = -1;

    /*----- Get the lock file and hitch names -----*/
    l_path = (*local->calc_paths)(write ? CalcPathCarmel2WriteLock:
                                              CalcPathCarmel2ReadLock,
                                      file, 0);
    if(l_path == NULL)
      return(-1);
    strcpy(lock, l_path);
    /* copy lock file into bufferl call it hitch, unique thing added below */
    hitch = carmel_path_buf;
    hitch = strcpy(hitch, lock);
    l = strlen(hitch);
    
    do {
        /*--- First create hitching post ----*/
        for(n = time(0) % 6400; ; n += 10007) {
            /* Get random file name, that's not too long */
            sprintf(hitch + l, "_%c%c", '0' + (n % 80) , '0' + (n/80));
            if(stat(hitch, &sb) < 0)
              break; /* Got a name that doesn't exist */
        }
        hitch_fd = open(hitch, O_CREAT, 0666);
        if(hitch_fd < 0) {
            sprintf(error_mess, "Error creating lock file \"%s\": %s",
                    hitch, strerror(errno));
            mm_log(error_mess, WARN);
            return(-1);
        }
        close(hitch_fd);
    
        /*----- Got a hitching post, now try link -----*/
        link_result = link(hitch, lock);
        stat(lock, &sb);
        unlink(hitch);
        if(link_result == 0 && sb.st_nlink == 2) {
            /*------ Got the lock! ------*/
            stat(lock, &sb);
            if(write)
              local->write_lock_mod_time = sb.st_mtime;
            else
              local->read_lock_mod_time = sb.st_mtime;
            return(0);
        }

        /*----- Check and override if lock is too old -----*/
        if(sb.st_mtime + override < time(0)) {
            unlink(lock); /* Lock is old, override it */
            timer = 100; /* Get us around the loop again */
            continue;
        } else {
            if(timer < 0) /* timer not set */
              timer = sb.st_mtime + override - time(0);
        }

        /*----- Will user wait till time for override? -----*/
        if(!write || timer > 5 * 60) {
            return(-1); /* Not more that 5 minutes */
        }

        /*----- Try again, and tell user we're trying -------*/
        if(!(timer % 15)) {
            sprintf(error_mess,
                    "Please wait. Mailbox %s is locked for %d more seconds",
                    file, timer);
            mm_log(error_mess, WARN);
        }
        timer--;
        sleep(1);
    } while(timer > 0);

    return(-1);
}



/*----------------------------------------------------------------------
   Unlock a carmel mail stream

Args: stream -- The mailstream that is locked
      mailbox -- FQN of mailbox to lock ( e.g. #carmel#foo )
      write   -- flag to set if it is a write lock

Nothing is returned
  ----*/
void
carmel2_unlock(local, mailbox, write)
     CARMEL2LOCAL *local;
     char         *mailbox;
     int           write;
{
    char        lock[CARMEL_PATHBUF_SIZE];
    struct stat sb;

    strcpy(lock, (*local->calc_paths)(write ? CalcPathCarmel2WriteLock:
                                              CalcPathCarmel2ReadLock,
                                      mailbox, 0));

    if(stat(lock, &sb) < 0)
      return; /* Hmmm... lock already broken */

    if(sb.st_mtime !=
       (write ? local->write_lock_mod_time : local->read_lock_mod_time))
      return; /* Hmmm... not our lock */
    
    unlink(lock);
}

    
    
/*----------------------------------------------------------------------
    Keep the mod date on the lock file fresh 

Args: stream --
      file   -- the name of the mailbox the lock is for
      write  -- set if this is a write lock

Returns: 0 if update was OK
        -1 if something bad happened, like the lock was stolen
 ----*/
static int 
carmel2_update_lock(local, file, write)
     CARMEL2LOCAL *local;
     char         *file;
     int           write;
{
    char        lock[CARMEL_PATHBUF_SIZE];
    struct      timeval tp[2];
    struct stat sb;

    strcpy(lock, (*local->calc_paths)(write ? CalcPathCarmel2WriteLock:
                                              CalcPathCarmel2ReadLock,
                                      file, 0));

    if(stat(lock, &sb) < 0) {
        /* Lock file stolen, oh oh */
        return(-1);
    }

    if(sb.st_mtime !=
       (write ? local->write_lock_mod_time : local->read_lock_mod_time)) {
        /* Not our lock anymore , oh oh */
        return(-1);
    }

    gettimeofday (&tp[0],NIL);  /* set atime to now */
    gettimeofday (&tp[1],NIL);  /* set mtime to now */
    utimes(lock, tp);

    if(write)
      local->write_lock_mod_time = tp[1].tv_sec;
    else
      local->read_lock_mod_time = tp[1].tv_sec;
    return(0);
}



/*----------------------------------------------------------------------
   Berkeley open and lock mailbox

This is mostly ripped off from the Bezerk driver
 ----*/

static int 
carmel2_bezerk_lock (spool, file)
        char *spool, *file;
{
  int fd,ld,j;
  int i = BEZERKLOCKTIMEOUT * 60 - 1;
  struct timeval tp;
  struct stat sb;
  char *hitch, *lock;

  lock = carmel_path_buf;
  sprintf(lock, "%s.lock", spool);
  do {                          /* until OK or out of tries */
    gettimeofday (&tp,NIL);     /* get the time now */
#ifdef NFSKLUDGE
  /* SUN-OS had an NFS, As kludgy as an albatross;
   * And everywhere that it was installed, It was a total loss.  -- MRC 9/25/91
   */
                                /* build hitching post file name */
    hitch = carmel_20k_buf;
    sprintf(hitch, "%s.%d.%d.",carmel_path_buf,time (0),getpid ());
    j = strlen (hitch); /* append local host name */
    gethostname (hitch + j,(MAILTMPLEN - j) - 1);
                                /* try to get hitching-post file */
    if ((ld = open (hitch, O_WRONLY|O_CREAT|O_EXCL,0666)) < 0) {
                                /* prot fail & non-ex, don't use lock files */
      if ((errno == EACCES) && (stat (hitch, &sb))) *lock = '\0';
      else {                    /* otherwise something strange is going on */
        sprintf (carmel_20k_buf,"Error creating %s: %s",lock,strerror (errno));
        mm_log (carmel_20k_buf, WARN);  /* this is probably not good */
                                /* don't use lock files if not one of these */
        if ((errno != EEXIST) && (errno != EACCES)) *lock = '\0';
      }
    }
    else {                      /* got a hitching-post */
      chmod (hitch,0666);       /* make sure others can break the lock */
      close (ld);               /* close the hitching-post */
      link (hitch,lock);        /* tie hitching-post to lock, ignore failure */
      stat (hitch, &sb);        /* get its data */
      unlink (hitch);           /* flush hitching post */
      /* If link count .ne. 2, hitch failed.  Set ld to -1 as if open() failed
         so we try again.  If extant lock file and time now is .gt. file time
         plus timeout interval, flush the lock so can win next time around. */
      if ((ld = (sb.st_nlink != 2) ? -1 : 0) && (!stat (lock,&sb)) &&
          (tp.tv_sec > sb.st_ctime + BEZERKLOCKTIMEOUT * 60)) unlink (lock);
    }

#else
  /* This works on modern Unix systems which are not afflicted with NFS mail.
   * "Modern" means that O_EXCL works.  I think that NFS mail is a terrible
   * idea -- that's what IMAP is for -- but some people insist upon losing...
   */
                                /* try to get the lock */
    if ((ld = open (lock,O_WRONLY|O_CREAT|O_EXCL,0666)) < 0) switch (errno) {
    case EEXIST:                /* if extant and old, grab it for ourselves */
      if ((!stat (lock,&sb)) && tp.tv_sec > sb.st_ctime + LOCKTIMEOUT * 60)
        ld = open (lock,O_WRONLY|O_CREAT,0666);
      break;
    case EACCES:                /* protection fail, ignore if non-ex or old */
      if (stat (lock,&sb) || (tp.tv_sec > sb.st_ctime + LOCKTIMEOUT * 60))
        *lock = '\0';           /* assume no world write mail spool dir */
      break;
    default:                    /* some other failure */
      sprintf (tmp,"Error creating %s: %s",lock,strerror (errno));
      mm_log (tmp,WARN);        /* this is probably not good */
      *lock = '\0';             /* don't use lock files */
      break;
    }
    if (ld >= 0) {              /* if made a lock file */
      chmod (tmp,0666);         /* make sure others can break the lock */
      close (ld);               /* close the lock file */
    }
#endif
    if ((ld < 0) && *lock) {    /* if failed to make lock file and retry OK */
      if (!(i%15)) {
        sprintf (carmel_20k_buf,"Mailbox %s is locked, will override in %d seconds...",
                 file,i);
        mm_log (carmel_20k_buf, WARN);
      }
      sleep (1);                /* wait 1 second before next try */
    }
  } while (i-- && ld < 0 && *lock);
                                /* open file */
  if ((fd = open (spool, O_RDONLY)) >= 0) flock (fd, LOCK_SH);
  else {                        /* open failed */
    j = errno;                  /* preserve error code */
    if (*lock) unlink (lock);   /* flush the lock file if any */
    errno = j;                  /* restore error code */
  }
  return(fd);
}



/*----------------------------------------------------------------------
   Berkeley unlock and close mailbox
 ----*/
static void 
carmel2_bezerk_unlock (fd, spool)
        int fd;
        char *spool;
{
    sprintf(carmel_path_buf, "%s.lock", spool);
    
    flock (fd, LOCK_UN);          /* release flock'ers */
    close (fd);                   /* close the file */
                                  /* flush the lock file if any */
    if (*carmel_path_buf) unlink (carmel_path_buf);
}



/*----------------------------------------------------------------------
    Make sure directory exists and is writable

Args: dir  - directory to check, should be full path
      
Result: returns -1 if not OK along with mm_logging a message
                 0 if OK
----*/

carmel2_check_dir(dir)
     char *dir;
{
    struct stat sb;
    char   error_mess[150];

    if(stat(dir, &sb) < 0) {
        if(mkdir(dir, 0700) < 0) {
            sprintf(error_mess, "Error creating directory %-.30s %s",
                    dir, strerror(errno));
            mm_log(error_mess, WARN);
            return(-1);
        }
    } else if(!(sb.st_mode & S_IFDIR)) {
        sprintf(error_mess, "Warning: %s is not a directory",dir);
        mm_log(error_mess, WARN);
        return(-1);

    } else if(access(dir, W_OK) != 0) {
        sprintf(error_mess, "Warning: unable to write to %-.30s %s",
                dir, strerror(errno));
        mm_log(error_mess, WARN);
        return(-1);
    }
    return(0);
}

    
    
/*----------------------------------------------------------------------
    Return the number for the next message file

Args: stream  -- the Mail stream for the new data file
      mailbox -- The FQN of mailbox data file is for

Returns: file number or -1
  ----*/
static 
carmel2_new_data_file(local, mailbox)
     CARMEL2LOCAL *local;
     char         *mailbox;
{
    char *path, num_buf[50], e_buf[100];
    int   fd, num, bytes_read;

    /*---- Get the full path of the .MAXNAME file for index ----*/
    path = (*local->calc_paths)(CalcPathCarmel2MAXNAME, mailbox, 0);
    if(path == NULL)
      return(-1);

    fd = open(path, O_RDWR|O_CREAT, 0666);
    if(fd < 0) {
        sprintf(e_buf, "Error getting next number of mail file: %s",
                strerror(errno));
        mm_log(e_buf, ERROR);
        return(-1);
    }

    bytes_read = read(fd, num_buf, sizeof(num_buf));
    if(bytes_read < 6) {
        num  = 100000;
    } else {
        num = atoi(num_buf) + 1;
        if(num >= 999999)
          num = 100000;
    }
    sprintf(num_buf, "%-6d\n", num);
    lseek(fd, 0, 0);
    write(fd, num_buf, strlen(num_buf));
    close(fd);
    return(num);
}



/*----------------------------------------------------------------------
   Do all the file name generation for carmel2 driver

   The mailbox passed in is in either the format: #carmel2#folder or
                                                  #carmel2#user%folder
This generates that absolute paths for an index, or a data file or
the .MAXNAME file.

Bug: This is untested!
  ----*/
static char *
carmel2_calc_paths(operation, mailbox, data_file_num)
     int         operation;
     char       *mailbox;
     int         data_file_num;
{
    static char            path[CARMEL_PATHBUF_SIZE], num[20];
    char                  *p, *home_dir;
    struct carmel_mb_name *parsed_name;
    struct passwd         *pw;

    parsed_name = carmel_parse_mb_name(mailbox,'2');

    if(parsed_name == NULL) {
        mm_log("Internal error (bug). Invalid Carmel folder name",ERROR);
        return(NULL);
    }
    

    if(parsed_name->user != NULL) {
        /*---- has a user in mailbox name -----*/
        pw = getpwnam(parsed_name->user);
        if(pw == NULL) {
            sprintf(carmel_error_buf,
                  "Error accessing mailbox \"%s\". No such user name \"%s\"\n",
                    mailbox, parsed_name->user);
            mm_log(carmel_error_buf, ERROR);
            carmel_free_mb_name(parsed_name);
            return(0);
        }
        home_dir = pw->pw_dir;
    } else {
        home_dir = myhomedir();
    }
    mailbox  = parsed_name->mailbox;

    switch(operation) {

      case CalcPathCarmel2Data:
        sprintf(path, "%s/%s/%d", home_dir, CARMEL2_MSG_DIR, data_file_num);
        

      case CalcPathCarmel2Index:
        sprintf(path, "%s/%s/%s", home_dir, CARMEL2_INDEX_DIR, mailbox);
        break;

      case CalcPathCarmel2MAXNAME:
        sprintf(path, "%s/%s", home_dir, CARMEL2_MAXFILE);
        break;

      case CalcPathCarmel2WriteLock:
        sprintf(path, "%s/%s/.%s.wl", home_dir, CARMEL2_INDEX_DIR, mailbox);
        break;

      case CalcPathCarmel2ReadLock:
        sprintf(path, "%s/%s/.%s.rl", home_dir, CARMEL2_INDEX_DIR, mailbox);
        break;
    }

    carmel_free_mb_name(parsed_name);
    return(path);
}



/*----------------------------------------------------------------------
   Find and parse the status line in a mail header

Args:  mc     -- the message cache where status is to be stored
       header -- the message header to parsen
 ----*/
void
carmel2_parse_bezerk_status(mc, header)
     MESSAGECACHE *mc;
     char *header;
{
    register char *p;
    for(p = header; *p; p++) {
        if(*p != '\n')
          continue;
        p++;
        if(*p != 'S' && *p != 'X')
          continue;
        if(strncmp(p, "Status: ",  8) == 0 || strncmp(p,"X-Status: ",10)== 0) {
            mc->recent = 1;
            for(p += *p == 'X' ? 10: 8; *p && *p != '\n'; p++)
              switch(*p) {
                case 'R': mc->seen    = 1; break;
                case 'O': mc->recent  = 0; break;
                case 'D': mc->deleted = 1; break;
                case 'F': mc->flagged = 1; break;
                case 'A': mc->flagged = 1; break;
              }
        }
    }
}
        


/*----------------------------------------------------------------------
  Turn a string of describing flags into a bit mask

Args:  stream -- mail stream, unused
       flag   -- string flag list

Returns:  returns short with bits set; bits are defined in carmel2.h
 ----*/
static short
carmel2_getflags (stream, flag)
        MAILSTREAM *stream;
        char *flag;
{
  char *t, tmp[100];
  short f = 0;
  short i,j;
  if (flag && *flag) {          /* no-op if no flag string */
                                /* check if a list and make sure valid */
    if ((i = (*flag == '(')) ^ (flag[strlen (flag)-1] == ')')) {
      mm_log ("Bad flag list",ERROR);
      return NIL;
    }
                                /* copy the flag string w/o list construct */
    strncpy (tmp,flag+i,(j = strlen (flag) - (2*i)));
    tmp[j] = '\0';
    t = ucase (tmp);    /* uppercase only from now on */

    while (*t) {                /* parse the flags */
      if (*t == '\\') {         /* system flag? */
        switch (*++t) {         /* dispatch based on first character */
        case 'S':               /* possible \Seen flag */
          if (t[1] == 'E' && t[2] == 'E' && t[3] == 'N') i = fSEEN;
          t += 4;               /* skip past flag name */
          break;
        case 'D':               /* possible \Deleted flag */
          if (t[1] == 'E' && t[2] == 'L' && t[3] == 'E' && t[4] == 'T' &&
              t[5] == 'E' && t[6] == 'D') i = fDELETED;
          t += 7;               /* skip past flag name */
          break;
        case 'F':               /* possible \Flagged flag */
          if (t[1] == 'L' && t[2] == 'A' && t[3] == 'G' && t[4] == 'G' &&
              t[5] == 'E' && t[6] == 'D') i = fFLAGGED;
          t += 7;               /* skip past flag name */
          break;
        case 'A':               /* possible \Answered flag */
          if (t[1] == 'N' && t[2] == 'S' && t[3] == 'W' && t[4] == 'E' &&
              t[5] == 'R' && t[6] == 'E' && t[7] == 'D') i = fANSWERED;
          t += 8;               /* skip past flag name */
          break;
        case 'R':               /* possible \Answered flag */
          if (t[1] == 'E' && t[2] == 'C' && t[3] == 'E' && t[4] == 'N' &&
              t[5] == 'T') i = fRECENT;
          t += 6;               /* skip past flag name */
          break;
        default:                /* unknown */
          i = 0;
          break;
        }
                                /* add flag to flags list */
        if (i && ((*t == '\0') || (*t++ == ' '))) f |= i;
        else {                  /* bitch about bogus flag */
          mm_log ("Unknown system flag",ERROR);
          return NIL;
        }
      }
      else {                    /* no user flags yet */
        mm_log ("Unknown flag",ERROR);
        return NIL;
      }
    }
  }
  return f;
}


/*----------------------------------------------------------------------
   Get a pointer to a MESSAGECACHE entry, allocating if needed
 
Args: stream -- message stream
      num    -- Message number to allocate on for

Returns: The MESSAGECACHE entry

The message caches are allocated in blocks of 256 to save space taken up by
pointers in a linked list and allocation overhead. The mc_blocks 
data structure is an array that points to each block. The macro MC() 
defined in carmel.h returns a pointer to a MESSAGECACHE given
a message number. This function here can be called when a MESSAGECACHE
is needed to a message number that might be new. It allocates a new
block if needed and clears the MESSAGECACHE returned.

The MESSAGECACHE entries are currently about 28 bytes which implies 28Kb
must be used per 1000 messages. If memory is limited this driver will be 
limited in the number of messages it can handle, and the limit is due to
the fact that these MESSAGECACHEs must be in core.

It might be possible to reduce the size of each entry by a few bytes if
the message numbers were reduced to a short, and the mostly unused keywords
were removed. It would also be nice to add a day of the week (3 bits)
  ----*/
static MESSAGECACHE *
carmel2_new_mc(stream, num)
     MAILSTREAM *stream;
     int num;
{
    MESSAGECACHE *mc;
      
    /* Make sure we've got a cache_entry */
    if(num >= LOCAL->cache_size) {
        if(LOCAL->mc_blocks == NULL)
          LOCAL->mc_blocks=(MESSAGECACHE **)fs_get(sizeof(MESSAGECACHE *));
        else
          fs_resize((void **)&(LOCAL->mc_blocks), ((num >>8) + 1) *
                                                  sizeof(MESSAGECACHE *));
        LOCAL->mc_blocks[num >> 8] = (MESSAGECACHE *)
          fs_get(256 * sizeof(MESSAGECACHE));
        LOCAL->cache_size = ((num >> 8) + 1) * 256;
    }
    
    mc = MC(num);

    mc->user_flags = 0;
    mc->lockcount  = 0;
    mc->seen       = 0;
    mc->deleted    = 0;
    mc->flagged    = 0;
    mc->answered   = 0;
    mc->recent     = 0;
    mc->searched   = 0;
    mc->sequence   = 0;
    mc->spare      = 0;
    mc->spare2     = 0;
    mc->spare3     = 0;
    mc->msgno      = num;
    
   /* Don't set the date, the size and the extra data,
      assume they will be set 
    */
    return(mc);
}



/*----------------------------------------------------------------------
  Do the real work of appending a message to a mailbox
 
Args: local -- The carmel2 local data structure, (a some what fake incomplete
               one, set up for the purposes here)
      mailbox -- Name of mailbox to append to
      message -- The rfc822 format of the message with \r\n's

Returns: T if all went OK, NIL if it did not

 - Make sure index exists or can be created
 - lock index for write
 - get a data file name
 - Put the text in the file
 - Parse the string to get envelope and message cache
 - Write the entry into the index
 - Unlock the index

BUG: This needs some locking and some error messages

  ----*/
carmel2_append2(stream, local, mailbox, flags, date, message)
     char         *mailbox, *flags, *date;
     CARMEL2LOCAL *local;
     STRING       *message;
     MAILSTREAM   *stream;
{
    char         *index_name, *data_name, *p, c, *header_string;
    ENVELOPE     *envelope;
    BODY         *b;
    MESSAGECACHE  mc;
    FILE         *index_file, *data_file;
    struct stat   sb;
    int           last_was_crlf, saved_errno;
    STRING        string_struct;
    long          size;
    short         flagbits;

    /*------ Lock the mailbox for write ------*/
    if(carmel2_lock(local, mailbox, WRITE_LOCK) < 0) {
        sprintf(carmel_error_buf,
                "Mailbox \"%s\" is locked. Can't append to it.",
                mailbox);
        mm_log(carmel_error_buf, ERROR);
        return(NIL);
    }

    /*----- Get the file name of carmel2 index -----*/
    index_name = (*local->calc_paths)(CalcPathCarmel2Index, mailbox, 0);
    if(index_name == NULL) {
        saved_errno = 0;
        goto bomb;
    }

    /*------ See if it exists already or not ------*/
    if(stat(index_name, &sb) < 0) {
        mm_notify (stream,"[TRYCREATE] Must create mailbox before copy", NIL);
        carmel2_unlock(local, mailbox, WRITE_LOCK);
        return(NIL);
    }

    index_file = fopen(index_name, "a");
    if(index_file == NULL)
      goto bomb;

    mc.data2 = carmel2_new_data_file(local, mailbox);

    flagbits = carmel2_getflags(NULL, flags);

    if(flagbits & fSEEN)     mc.seen     = T;
    if(flagbits & fDELETED)  mc.deleted  = T;
    if(flagbits & fFLAGGED)  mc.flagged  = T;
    if(flagbits & fANSWERED) mc.answered = T;
    if(flagbits & fRECENT)   mc.recent   = T;
    mc.user_flags  = 0;

    /*----- Open the data file -----*/
    data_name = (*local->calc_paths)(CalcPathCarmel2Data, mailbox, mc.data2);
    if(data_name == NULL)  {
        errno = 0; /* Don't generate an error message at all */
        goto bomb;
    }
    data_file = fopen(data_name, "w");
    if(data_file == NULL)
      goto bomb; 

    /*--- accumulate header as we go for later parsing ---*/
    header_string = carmel_20k_buf;

    /*------ Write the message to the file, and get header in a string -----*/
    for(size = SIZE(message); size > 0; size--){
        c = SNX(message);
        if(c == '\r' && size > 1) {
            /* Turn CRLF into LF for UNIX */
            c = SNX(message);
            size--;
            if(c != '\n') {
                if(fputc('\r', data_file) < 0 || fputc(c, data_file) < 0) 
                  goto bomb;
                if(header_string != NULL) {
                    *header_string++ = '\r';
                    *header_string++ = c;
                }
            } else {
                if(fputc('\n', data_file) < 0)
                  goto bomb;
                if(header_string != NULL) {
                    if(last_was_crlf) {
                        *header_string = '\0';
                        header_string = NULL;
                    }  else {
                      *header_string++ = '\r';
                      *header_string++ = '\n';
                    }
                }
                last_was_crlf = 1;
              }
        } else {
            if(fputc(c, data_file) == EOF) 
              goto bomb;
            if(header_string != NULL)
              *header_string++ = c;
            last_was_crlf = 0;
        }
    }
    if(fclose(data_file) == EOF) 
      goto bomb;
    data_file = NULL;

    
    /*----Get the size that we actually wrote -----*/
    stat(data_name, &sb);
    mc.rfc822_size = sb.st_size;

    /* This blows the nice tight memory usage for the carmel2 driver :-( */
    header_string = cpystr(carmel_20k_buf); 

#ifdef BWC
    /*--- For MIME type x-bwc-glyph-wide, store in a nnnn.wid file ----*/
    for(p = header_string; *p; p++) {
        if((p == header_string  && carmel_match_glyph_wide(p)) ||
           (*p == '\r' && *(p+1) == '\n' && carmel_match_glyph_wide(p+2))) {
            sprintf(carmel_path_buf, "%s.wid", data_name);
            rename(data_name, carmel_path_buf);
            break;
        }
    }
#endif

    /*------ Parse the message to get envelope and message cache ----*/
    INIT(&string_struct, mail_string, (void *)"", 0);
    rfc822_parse_msg(&envelope, &b, header_string, strlen(header_string),
                     &string_struct, mylocalhost(), carmel_20k_buf);
    carmel2_parse_bezerk_status(&mc, header_string);
    carmel2_rfc822_date(&mc, header_string);

    /*------ Write the entry into the index ------*/
    if(carmel2_write_index(envelope, &mc, index_file) < 0)
      goto bomb;

    if(local->aux_copy != NULL) /* Write carmel index if needed */
      (*local->aux_copy)(local, mailbox, envelope, &mc);

    mail_free_envelope(&envelope);
    fs_give((void **)&header_string);

    if(fclose(index_file) == EOF)
      goto bomb;
    carmel2_unlock(local, mailbox, WRITE_LOCK);
    return(T);

  bomb:
    saved_errno = errno;
    if(index_file != NULL) {
        fclose(index_file);
    }
    if(data_file != NULL) {
        fclose(data_file);
        unlink(data_name);
    }
    carmel2_unlock(local, mailbox, WRITE_LOCK);
    if(saved_errno != 0) {
        sprintf(carmel_error_buf,"Message append failed: %s",
                strerror(saved_errno));
        mm_log(carmel_error_buf, ERROR);
    }
    return(NIL);
}




/* Search support routines
 * Accepts: MAIL stream
 *          message number
 *          pointer to additional data
 *          pointer to temporary buffer
 * Returns: T if search matches, else NIL
 */

static char 
carmel2_search_all (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return T;                     /* ALL always succeeds */
}


static char 
carmel2_search_answered (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->answered ? T : NIL;
}


static char 
carmel2_search_deleted (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->deleted ? T : NIL;
}


static char 
carmel2_search_flagged (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->flagged ? T : NIL;
}


static char 
carmel2_search_keyword (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return NIL;                   /* keywords not supported yet */
}


static char 
carmel2_search_new (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  MESSAGECACHE *elt = MC(msgno);
  return (elt->recent && !elt->seen) ? T : NIL;
}

static char 
carmel2_search_old (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->recent ? NIL : T;
}


static char 
carmel2_search_recent (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->recent ? T : NIL;
}


static char 
carmel2_search_seen (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->seen ? T : NIL;
}


static char 
carmel2_search_unanswered (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->answered ? NIL : T;
}


static char 
carmel2_search_undeleted (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->deleted ? NIL : T;
}


static char 
carmel2_search_unflagged (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->flagged ? NIL : T;
}


static char 
carmel2_search_unkeyword (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return T;                     /* keywords not supported yet */
}


static char 
carmel2_search_unseen (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return MC(msgno)->seen ? NIL : T;
}

static char 
carmel2_search_before (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return (char) (carmel2_msgdate (stream,msgno) < n);
}


static char 
carmel2_search_on (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  return (char) (carmel2_msgdate (stream,msgno) == n);
}


static char 
carmel2_search_since (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
                                /* everybody interprets "since" as .GE. */
  return (char) (carmel2_msgdate (stream,msgno) >= n);
}


static unsigned long 
carmel2_msgdate (stream,msgno)
        MAILSTREAM *stream;
        long msgno;
{
  struct stat sbuf;
  struct tm *tm;
  MESSAGECACHE *elt = MC(msgno);

  return (long) (elt->year << 9) + (elt->month << 5) + elt->day;
}

/*----------------------------------------------------------------------
    Search only the body of the text.
  BUG, probably need to unencode before searching
  ---*/
static char 
carmel2_search_body (stream,msgno, pat, pat_len)
        MAILSTREAM *stream;
        long msgno,pat_len;
        char *pat;
{
  char *t =  carmel2_fetchtext(stream, msgno);
  return (t && search (t,strlen(t), pat, pat_len));
}


/*----------------------------------------------------------------------
    Search the subject field
  ----*/
static char 
carmel2_search_subject (stream,msgno, pat, pat_len)
        MAILSTREAM *stream;
        long msgno, pat_len;
        char *pat;
{
  char *t = carmel2_fetchstructure (stream,msgno,NULL)->subject;
  return t ? search (t, strlen(t), pat, pat_len) : NIL;
}


/*----------------------------------------------------------------------
    Search the full header and body text of the message
  ---*/
static char 
carmel2_search_text (stream, msgno, pat, pat_len)
        MAILSTREAM *stream;
        long        msgno, pat_len;
        char       *pat;
{
  char *t = carmel2_fetchheader(stream,msgno);
  return (t && search(t, strlen(t), pat, pat_len)) ||
    carmel2_search_body(stream,msgno, pat, pat_len);
}


/*----------------------------------------------------------------------
   Search the Bcc field
  ---*/
static char 
carmel2_search_bcc (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  carmel_20k_buf[0] = '\0';
                                /* get text for address */
  rfc822_write_address (carmel_20k_buf,
                        carmel2_fetchstructure (stream,msgno,NULL)->bcc);
  return search (carmel_20k_buf, strlen(carmel_20k_buf),d,n);
}


static char 
carmel2_search_cc (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  carmel_20k_buf[0] = '\0';
                                /* get text for address */
  rfc822_write_address (carmel_20k_buf,
                        carmel2_fetchstructure (stream, msgno, NULL)->cc);
  return search (carmel_20k_buf, strlen(carmel_20k_buf),d,n);
}


static char 
carmel2_search_from (stream,msgno,d,n)
        MAILSTREAM *stream;
        long msgno;
        char *d;
        long n;
{
  carmel_20k_buf[0] = '\0';
                                /* get text for address */
  rfc822_write_address (carmel_20k_buf,
                        carmel2_fetchstructure (stream,msgno,NULL)->from);
  return search (carmel_20k_buf, strlen(carmel_20k_buf),d,n);
}


static char 
carmel2_search_to (stream,msgno, pat, pat_len)
        MAILSTREAM *stream;
        long msgno;
        char *pat;
        long pat_len;
{
  carmel_20k_buf[0] = '\0';
                                /* get text for address */
  rfc822_write_address (carmel_20k_buf,
                        carmel2_fetchstructure (stream, msgno, NULL)->to);
  return(search(carmel_20k_buf,strlen(carmel_20k_buf), pat, pat_len));
}

/* Search parsers */


/* Parse a date
 * Accepts: function to return
 *          pointer to date integer to return
 * Returns: function to return
 */

static search_t
carmel2_search_date (f, n)
        search_t f;
        long *n;
{
  long i;
  char *s;
  MESSAGECACHE elt;
                                /* parse the date and return fn if OK */
  return (carmel2_search_string (f,&s,&i) && mail_parse_date (&elt,s) &&
          (*n = (elt.year << 9) + (elt.month << 5) + elt.day)) ? f : NIL;
}

/* Parse a flag
 * Accepts: function to return
 *          pointer to string to return
 * Returns: function to return
 */

static search_t 
carmel2_search_flag (f,d)
        search_t f;
        char **d;
{
                                /* get a keyword, return if OK */
  return (*d = strtok (NIL," ")) ? f : NIL;
}


/* Parse a string
 * Accepts: function to return
 *          pointer to string to return
 *          pointer to string length to return
 * Returns: function to return
 */

static search_t 
carmel2_search_string (f,d,n)
        search_t f;
        char **d;
        long *n;
{
  char *c = strtok (NIL,"");    /* remainder of criteria */
  if (c) {                      /* better be an argument */
    switch (*c) {               /* see what the argument is */
    case '\0':                  /* catch bogons */
    case ' ':
      return NIL;
    case '"':                   /* quoted string */
      if (!(strchr (c+1,'"') && (*d = strtok (c,"\"")) && (*n = strlen (*d))))
        return NIL;
      break;
    case '{':                   /* literal string */
      *n = strtol (c+1,&c,10);  /* get its length */
      if (*c++ != '}' || *c++ != '\015' || *c++ != '\012' ||
          *n > strlen (*d = c)) return NIL;
      c[*n] = '\255';           /* write new delimiter */
      strtok (c,"\255");        /* reset the strtok mechanism */
      break;
    default:                    /* atomic string */
      *n = strlen (*d = strtok (c," "));
      break;
    }
    return f;
  }
  else return NIL;
}





   
/*----------------------------------------------------------------------
   Some stuff to help out with the date parsing
 ---*/  
char *xdays2[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};

char *
month_abbrev2(month_num)
     int month_num;
{
    static char *xmonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
    if(month_num < 1 || month_num > 12)
      return("xxx");
    return(xmonths[month_num - 1]);
}

struct time_zone_names {
    char *name;
    int   hours;
    int    minutes;
} tz_names[]  = {
    {"GMT",   0,   0},
    {"PST",  -8,   0},
    {"PDT",  -7,   0},
    {"MST",  -7,   0},
    {"MDT",  -6,   0},
    {"CST",  -6,   0},
    {"CDT",  -5,   0},
    {"EST",  -5,   0},
    {"EDT",  -4,   0},
    {"JST",   9,   0},
    {"IST",   2,   0},
    {"IDT",   3,   0},
    {NULL,    0,   0}};
    
/*----------------------------------------------------------------------
   Parse a date string into into a structure
 
Args: mc -- mesage cache to with structure to receive data
      given_date -- full header with date string somewhere to be found

This parses a number of date formats and produces a cannonical date in
a structure. The basic format is:
   
 dd mm yy hh:mm:ss.t tz   

It will also handle:
  ww dd mm yy hh:mm:ss.t tz     mm dd yy hh:mm:ss.t tz
  ww dd mm hh:mm:ss.t yy tz     mm dd hh:mm:ss.t yy tz

It knows many abbreviations for timezones, but is not complete.
In general absolute time zones in hours +/- GMT are best.
  ----*/
void
carmel2_rfc822_date(mc, given_date)
     char        *given_date;
     MESSAGECACHE *mc;
{
    char *p, **i, *q;
    int   month, year;
    struct time_zone_names *tz;

    mc->seconds  = 0;
    mc->minutes  = 0;
    mc->hours    = 30;
    mc->day      = 0;
    mc->month    = 0;
    mc->year     = 0;
    mc->zhours   = 0;
    mc->zminutes = 0;

    if(given_date == NULL)
      return;

    p = given_date;

    if(*p != 'D' && strncmp(p, "Date:",5))
      while(*p) {
          if(*p == '\n' && *(p+1) == 'D' && strncmp(p+1, "Date:", 5) == 0)
            break;
         p++;
      }
    if(!*p)
      return;

    p += 6; /* Skip "\nDate: " */
    while(isspace(*p))
      p++;
           
    /* Start with month, weekday or day ? */
    for(i = xdays2; *i != NULL; i++) 
      if(struncmp2(p, *i, 3) == 0) /* Match first 3 letters */
        break;
    if(*i != NULL) {
        /* Started with week day .. buz over it*/
        while(*p && !isspace(*p) && *p != ',')
          p++;
        while(*p && (isspace(*p) || *p == ','))
          p++;
    }
    if(isdigit(*p)) {
        mc->day = atoi(p);
        while(*p && isdigit(*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace(*p)))
          p++;
    }
    for(month = 1; month <= 12; month++)
      if(struncmp2(p, month_abbrev2(month), 3) == 0)
        break;
    if(month < 13) {
        mc->month = month;

    } 
    /* Move over month, (or whatever is there) */
    while(*p && !isspace(*p) && *p != ',' && *p != '-')
       p++;
    while(*p && (isspace(*p) || *p == ',' || *p == '-'))
       p++;

    /* Check again for day */
    if(isdigit(*p) && mc->day == -1) {
        mc->day = atoi(p);
        while(*p && isdigit(*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace(*p)))
          p++;
    }

    /*-- Check for time --*/
    for(q = p; *q && isdigit(*q); q++);
    if(*q == ':') {
        /* It's the time (out of place) */
        mc->hours = atoi(p);
        while(*p && *p != ':' && !isspace(*p))
          p++;
        if(*p == ':') {
            mc->minutes = atoi(p);
            while(*p && *p != ':' && !isspace(*p))
              p++;
            if(*p == ':') {
                mc->seconds = atoi(p);
                while(*p && !isspace(*p))
                  p++;
            }
        }
        while(*p && isspace(*p))
          p++;
    }
    
    /* Get the year 0-50 is 2000-2050; 50-100 is 1950-1999 and
                                           101-9999 is 101-9999 */
    if(isdigit(*p)) {
        year = atoi(p);
        if(year < 50)   
          year += 2000;
        else if(year < 100)
          year += 1900;
        mc->year = year - 1969; 
        while(*p && isdigit(*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace(*p)))
          p++;
    } else {
        /* Something wierd, skip it and try to resynch */
        while(*p && !isspace(*p) && *p != ',' && *p != '-')
          p++;
        while(*p && (isspace(*p) || *p == ',' || *p == '-'))
          p++;
    }

    /*-- Now get hours minutes, seconds and ignore tenths --*/
    for(q = p; *q && isdigit(*q); q++);
    if(*q == ':' && mc->hours == 30) {
        mc->hours = atoi(p);
        while(*p && *p != ':' && !isspace(*p))
          p++;
        if(*p == ':') {
            p++;
            mc->minutes = atoi(p);
            while(*p && *p != ':' && !isspace(*p))
              p++;
            if(*p == ':') {
                p++;
                mc->seconds = atoi(p);
                while(*p && !isspace(*p) && *p != '+' && *p != '-')
                  p++;
            }
        }
    }
    while(*p && isspace(*p))
      p++;


    /*-- The time zone --*/
    if(*p) {
        if(*p == '+' || *p == '-') {
            char tmp[3];
            mc->zoccident = (*p == '+' ? 1 : 0);
            p++;
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            mc->zhours = atoi(tmp);
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            mc->zminutes *= atoi(tmp);
        } else {
            for(tz = tz_names; tz->name != NULL; tz++) {
                if(struncmp2(p, tz->name, strlen(tz->name)) ==0) {
                    if(tz->hours >= 0) {
                        mc->zhours = tz->hours;
                        mc->zoccident = 1;
                    } else {
                        mc->zhours    = -tz->hours;
                        mc->zoccident = 0;
                    }
                    mc->zminutes = tz->minutes;
                    break;
                }
            }
        }
    }
    if(mc->hours == 30)
      mc->hours = 0;
}


      
/*----------------------------------------------------------------------
    Print the date from the MESSAGECACHE into the string
 ----*/
static void
carmel2_date2string(string, mc)
     char         *string;
     MESSAGECACHE *mc;
{
    sprintf(string, "%d %s %d %d:%02d:%02d %s%04d",
            mc->day, month_abbrev2(mc->month), 1969+mc->year,
            mc->hours, mc->minutes, mc->seconds, mc->zoccident ? "-" : "",
            mc->zhours * 100 + mc->zminutes);
}



/*----------------------------------------------------------------------
    Read the date into a structure from line in Carmel index

 Args: mc     -- The structure to contain the date (and other things)
       string -- String to be parsed. Format is:
                  "yyyy^Amm^Add^Ahh^Amm^Ass^Azh^Azm"

 ----*/
static void
carmel2_parse_date(mc, string)
     MESSAGECACHE *mc;
     char         *string;
{
    int n;
    mc->year    = next_num(&string) - 1969;
    mc->month   = next_num(&string);
    mc->day     = next_num(&string);
    mc->hours   = next_num(&string);
    mc->minutes = next_num(&string);
    mc->seconds = next_num(&string);

    n = next_num(&string);
    if(n < 0) {
        mc->zoccident = 0;
        mc->zhours    = -n;
    } else {
        mc->zoccident = 1;
        mc->zhours    = n;
    }
    mc->zminutes = next_num(&string);      
}



/*----------------------------------------------------------------------
   Read the next number out of string and return it, advancing the string
  ----*/
static
next_num(string)
     char **string;
{
    int   n;
    char *p;

    if(string == NULL)
      return(0);

    p = *string;
    n = atoi(p);
    while(*p > '\001')
      p++;
    if(*p)
      *string = p+1;
    else
      *string = NULL;
    return(n);
}


/*----------------------------------------------------------------------
     Take a (slightly ugly) FQ mailbox and and return the prettier 
  last part of it
  ----*/
char *
carmel_pretty_mailbox(mailbox)
     char *mailbox;
{
    char *pretty_mb;
 
    for(pretty_mb = mailbox + strlen(mailbox) - 1;
        *pretty_mb != '#' && pretty_mb > mailbox;
        pretty_mb--)
      ;
    if(*pretty_mb == '#')
      pretty_mb++;
    return(pretty_mb);
}

/*----------------------------------------------------------------------
    Parse a carmel mailbox name into its parts

Args: fq_name: The name to parse
      given_version: The version that must match; currently either \0 or '2'

Returns: NULL if not a valid carmel name, version of name and given_version
         do not match, or a malloced structure if it is.
  ----*/
struct carmel_mb_name *
carmel_parse_mb_name(fq_name, given_version)
     char *fq_name;
     char  given_version;
{
    char                  *p, *q, version[2];
    struct carmel_mb_name *parsed_name;

    if(struncmp2(fq_name, CARMEL_NAME_PREFIX, strlen(CARMEL_NAME_PREFIX))!= 0){
        return(0);   /* BUG -- we won't work with non-FQN names for now */
        p = fq_name;
	version[0] = given_version;
	version[1] = '\0';
    } else {
	if(fq_name[7] == CARMEL_NAME_CHAR) {
	    version[0] = '\0';
	    p = fq_name + 8;
	} else if(fq_name[8] == CARMEL_NAME_CHAR) {
	    version[0] = fq_name[7];
	    version[1] = '\0';
	    p = fq_name + 9;
	} else {
	    return(NULL);
	}
    }

    if(given_version != version[0])
      return(NULL);

    parsed_name=(struct carmel_mb_name *)fs_get(sizeof(struct carmel_mb_name));
    parsed_name->version[0] = version[0];
    parsed_name->version[1] = version[1];

    /*---- Find second # if there is one ---*/
    for(q = p; *q && *q != CARMEL_NAME_CHAR; q++);

    if(*q == CARMEL_NAME_CHAR) {
        /*----- There is a user name -----*/
        parsed_name->user = fs_get((q - p) + 1);
        strncpy(parsed_name->user, p, q - p);
        parsed_name->user[q - p] = '\0';
        p = q + 1;
    } else {
        parsed_name->user = NULL;
    }

    parsed_name->mailbox = cpystr(p);

    return(parsed_name);
}


void
carmel_free_mb_name(mb_name)
     struct carmel_mb_name *mb_name;
{
    if(mb_name->user != NULL)
      fs_give((void **)(&(mb_name->user)));
    fs_give((void **)(&(mb_name->mailbox)));
    fs_give((void **)&mb_name);
}
    
    

    
    
        
        
    

    
/*--------------------------------------------------
     A case insensitive strcmp()     
  
   Args: o, r -- The two strings to compare

 Result: integer indicating which is greater
  ---*/
strucmp2(o, r)
     register char *r, *o;
{
    if(r == NULL && o == NULL)
      return(0);
    if(o == NULL)
      return(1);
    if(r == NULL)
      return(-1);

    while(*o && *r && (isupper(*o) ? tolower(*o) : *o) ==
                      (isupper(*r) ? tolower(*r) : *r)) {
        o++, r++;
    }
    return((isupper(*o) ? tolower(*o): *o)-(isupper(*r) ? tolower(*r) : *r));
}



/*----------------------------------------------------------------------
     A case insensitive strncmp()     
  
   Args: o, r -- The two strings to compare
         n    -- length to stop comparing strings at

 Result: integer indicating which is greater
   
  ----*/
struncmp2(o, r, n)
     register char *r, *o;
     register int   n;
{
    if(r == NULL && o == NULL)
      return(0);
    if(o == NULL)
      return(1);
    if(r == NULL)
      return(-1);

    n--;
    while(n && *o && *r &&
          (isupper(*o)? tolower(*o): *o) == (isupper(*r)? tolower(*r): *r)) {
        o++; r++; n--;
    }
    return((isupper(*o)? tolower(*o): *o) - (isupper(*r)? tolower(*r): *r));
}

/*----------------------------------------------------------------------
    A replacement for strchr or index ...

 ....so we don't have to worry if it's there or not. We bring our own.
If we really care about efficiency and think the local one is more
efficient the local one can be used, but most of the things that take
a long time are in the c-client and not in pine.
 ----*/
char *
strindex2(buffer, ch)
    char *buffer;
    int ch;
{
    /** Returns a pointer to the first occurance of the character
        'ch' in the specified string or NULL if it doesn't occur **/

    do
      if(*buffer == ch)
        return(buffer);
    while (*buffer++ != '\0');

    return(NULL);
}


/*======================================================================
 */

xopen(file, mode)
     char *file, *mode;
{}


xclose(stream)
     FILE *stream;
{}

xread() {}

xwrite() {}

xlseek() {}

#ifdef BWC
carmel_match_glyph_wide(string)
     char *string;
{
    extern char *ptspecials;
    char *s;

    if(struncmp2(string, "content-type", 12))
      return(0);  /* Nope */
    string += 12;
    while(*string && isspace(*string)) string++;
    if(*string != ':')
      return(0);
    string++;
    while(*string && isspace(*string)) string++;
    s = string;
    string = rfc822_parse_word(string, ptspecials);
    if(string == NULL)
      return(0);
    if(struncmp2(s, "text", 4))
      return(0);
    while(*string && isspace(*string)) string++;
    if(*string != '/')
      return;
    string++;
    while(*string && isspace(*string)) string++;
    s = string;
    string = rfc822_parse_word(string, ptspecials);
    if(string == NULL)
      return(0);
    if(struncmp2(s, "x-bwc-glyph-wide", 16))
      return(0);
    return(1);
}
#endif    
    
