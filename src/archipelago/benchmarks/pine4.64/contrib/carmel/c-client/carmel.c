/*----------------------------------------------------------------------

         T H E   C A R M E L   M A I L   D R I V E R

  Author(s):   Laurence Lundblade
               Baha'i World Centre
               Data Processing
               Haifa, Israel
 	       Internet: lgl@cac.washington.edu or laurence@bwc.org
               September 1992

  Last Edited: Aug 31, 1994

The "Carmel" mail format is cutsom mail file format that has messages
saved in individual files and an index file that references them. It
has also been called the "pod" or "vmail" format. This is probably not
of interest to anyone away from it's origin, though the Carmel2 format
might be.

This driver reads and writes the format in conjunction with the carmel2
driver. The carmel2 file driver does most of the work as most of the
operations are done on the created auxiliary carmel2 index and the
carmel index is updated when the mail file is closed.

  ----------------------------------------------------------------------*/


#include <stdio.h>
#include <ctype.h>
#include <pwd.h>
#include <netdb.h>
#include <errno.h>
extern int errno;		/* just in case */
#include "mail.h"
#include "osdep.h"
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "carmel2.h"
#include "carmel.h"
#include "rfc822.h"
#include "misc.h"


/* Driver dispatch used by MAIL. The carmel 2driver shares most of
   its data structures and functions with the carmel driver
 */

DRIVER carmeldriver = {
  "carmel",
  (DRIVER *) NIL,		/* next driver */
  carmel_valid,			/* mailbox is valid for us */
  carmel_parameters,            /* Set parameters */
  carmel_find,			/* find mailboxes */
  carmel_find_bboards,		/* find bboards */
  carmel_find_all,              /* find all mailboxes */
  carmel_find_bboards,          /* find all the bboards ; just a noop here  */
  carmel_subscribe,	        /* subscribe to mailbox */
  carmel_unsubscribe,	        /* unsubscribe from mailbox */
  carmel_subscribe_bboard,	/* subscribe to bboard */
  carmel_unsubscribe_bboard,    /* unsubscribe from bboard */
  carmel_create,		/* create mailbox */
  carmel_delete,		/* delete mailbox */
  carmel_rename,		/* rename mailbox */
  carmel_open,			/* open mailbox */
  carmel_close,			/* close mailbox */
  carmel2_fetchfast,		/* fetch message "fast" attributes */
  carmel2_fetchflags,		/* fetch message flags */
  carmel2_fetchstructure,	/* fetch message envelopes */
  carmel2_fetchheader,		/* fetch message header only */
  carmel2_fetchtext,		/* fetch message body only */
  carmel2_fetchbody,		/* fetch message body section */
  carmel2_setflag,		/* set message flag */
  carmel2_clearflag,		/* clear message flag */
  carmel2_search,		/* search for message based on criteria */
  carmel2_ping,			/* ping mailbox to see if still alive */
  carmel_check,	                /* check for new messages */
  carmel_expunge,		/* expunge deleted messages */
  carmel2_copy,			/* copy messages to another mailbox */
  carmel2_move,			/* move messages to another mailbox */
  carmel_append,                /* Append message to a mailbox */
  carmel2_gc,			/* garbage collect stream */
};

MAILSTREAM carmelproto ={&carmeldriver}; /* HACK for default_driver in pine.c*/

#ifdef ANSI
static int   carmel_sift_files(struct direct *); 
static void  carmel_recreate_index(MAILSTREAM *);
static char  *carmel_calc_paths(int, char *, int);
static int   vmail2carmel(char *, char *, MAILSTREAM *, char *, char *);
static int   write_carmel_index(FILE *, MESSAGECACHE *, ENVELOPE *);
static int   carmel_reset_index_list();
static void  carmel_kill_locks(char *);
#else
static int   carmel_sift_files(); 
static void  carmel_recreate_index();
static char  *carmel_calc_paths();
static int   vmail2carmel();
static int   write_carmel_index();
static int   carmel_reset_index_list();
static void  carmel_kill_locks();
#endif



extern char carmel_20k_buf[20000], carmel_path_buf[], carmel_error_buf[];

static int   disk_setup_checked = 0;

/*----------------------------------------------------------------------
  Carmel mail validate mailbox

Args: name -- name to check

Returns: our driver if name is valid, otherwise calls valid in next driver
 ---*/

DRIVER *carmel_valid (name)
	char *name;
{
    return(carmel_isvalid(name) ? &carmeldriver : NIL);
}



/*----------------------------------------------------------------------
    Check and see if a named mailbox is a valid carmel mail index

Args: name -- name or path of mail file. It is a FQN, e.g. #carmel#sent-mail

Returns:  0 if it is not a valid mailbox
          1 if it is.
      
   A carmel index must be a regular file, readable, and have the second word in
the file be "index", upper or lower case.
  ----*/
int 
carmel_isvalid (name)
	char *name;
{
    struct stat     sbuf;
    int             fd;
    char           *p, *carmel_index_file;
    struct carmel_mb_name *parsed_name;

    if(!disk_setup_checked){
       disk_setup_checked = 1;
       carmel_init(NULL);
    }

    /* Don't accept plain "inbox".  If we do then the carmel driver
       takes over all other drivers linked after it.  This way the 
       inbox-path in Pine can be set to #carmel#MAIL to make the
       inbox the carmel one 
      */

    parsed_name = carmel_parse_mb_name(name, '\0');
    if(parsed_name == NULL)
      return(0);

    carmel_free_mb_name(parsed_name);

    carmel_index_file = carmel_calc_paths(CalcPathCarmelIndex, name, 0);

    if(carmel_index_file == NULL)
      return(0); /* Will get two error messages here, one from dummy driver */

    if(stat(carmel_index_file, &sbuf) < 0)
      return(1); /* If the name matches and it doesn't exist, it's OK */
  
    if(!(sbuf.st_mode & S_IFREG))
      return(0);
  
    fd = open(carmel_index_file, O_RDONLY);
    if(fd < 0)
      return(0);
  
    if(read(fd, carmel_20k_buf, 200) <= 0)
      return(0);
    carmel_20k_buf[199] = '\0';
  
    close(fd);
  
    for(p = carmel_20k_buf; *p && !isspace(*p); p++); /* frist word */
    for(; *p && isspace(*p); p++);  /* space */
    lcase(p);                       /* lower case the "index" */
    if(!*p || strncmp(p, "index", 5))  
      return(0);
  
    return(1);
}




/*----------------------------------------------------------------------

  ----*/
void *
carmel_parameters(function, value)
     long  function;
     char *value;
{}



/*----------------------------------------------------------------------
   This is used by scandir to determine which files in the directory
 are treated as mail files
  ----*/
static char *sift_pattern = NULL;
static int
carmel_sift_files(dir)
     struct direct *dir;
{
    if(dir->d_name[0] == '.')
      return(0);
    else if(strcmp(dir->d_name, "MAIL") == 0) 
     /* Never return the inbox. "INBOX" is always included by default */
      return(0);
    else if(pmatch(dir->d_name, sift_pattern))
      return(1);
    else
      return(0);
}

int alphasort();

/* ----------------------------------------------------------------------
  Carmel find list of mail boxes
  Args: mail stream
        pattern to search
 
  This scans the ".vmail/index" directory for a list of folders
  (indexes in vmail terms).

BUG -- doesn't really match patterns (yet)
 ----*/
void 
carmel_find(stream, pat)
	MAILSTREAM *stream;
	char *pat;
{
    char            tmp[MAILTMPLEN];
    struct direct **namelist, **n;
    int             num;
    struct carmel_mb_name *parsed_name;
    struct passwd         *pw;

    parsed_name = carmel_parse_mb_name(pat, '\0');

    if(parsed_name == NULL)
      return;

    if(parsed_name->user == NULL) {
        sprintf(tmp, "%s/%s", myhomedir(), CARMEL_INDEX_DIR);
    } else {
        pw = getpwnam(parsed_name->user);
        if(pw == NULL) {
            sprintf(carmel_error_buf,
                  "Error accessing mailbox \"%s\". No such user name \"%s\"\n",
                    pat, parsed_name->user);
            mm_log(carmel_error_buf, ERROR);
            return;
        }
        sprintf(tmp, "%s/%s", pw->pw_dir, CARMEL_INDEX_DIR);
    }

    sift_pattern = parsed_name->mailbox;

    num = scandir(tmp, &namelist, carmel_sift_files, alphasort);

    if(num <= 0) {
/*        sprintf(carmel_error_buf, "Error finding mailboxes \"%s\": %s",
                pat, strerror(errno));
        mm_log(carmel_error_buf, ERROR);*/
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
     Find_all is the same for find in the carmel driver; there is no 
difference between subscribed and unsubscribed mailboxes
  ----*/     
void 
carmel_find_all (stream, pat)
	MAILSTREAM *stream;
	char *pat;
{
   carmel_find(stream, pat);
}


/*----------------------------------------------------------------------
   Find bulliten boards is a no-op for carmel, there are none (yet)
 */
void 
carmel_find_bboards (stream,pat)
     MAILSTREAM *stream;
     char *pat;
{
  /* Always a no-op */
}


/*----------------------------------------------------------------------
   No subscription management for the carmel format. Probably should work
 with the .mailbox list format, but that's probably implemented in mail.c
 ----*/
long carmel_subscribe() {}
long carmel_unsubscribe() {}
long carmel_subscribe_bboard() {}
long carmel_unsubscribe_bboard() {}



/*----------------------------------------------------------------------
    Create a carmel folder

Args:  stream --  Unused here
       mailbox -- the FQN of mailbox to create

Returns:  T on success, NIL on failure. 

An error message is logged on failure.
  ----*/
long
carmel_create(stream, mailbox)
     MAILSTREAM *stream;
     char       *mailbox;
{
    char       *carmel_index_file, *carmel2_index_file;
    struct stat sb;
    FILE       *f;

    carmel_index_file = carmel_calc_paths(CalcPathCarmelIndex, mailbox, 0);

    if(carmel_index_file == NULL)
      return(NIL);

    /*--- Make sure it doesn't already exist ---*/
    if(stat(carmel_index_file, &sb) >= 0)
      return(NIL);

    /*--- The carmel index ----*/
    f = fopen(carmel_index_file, "w");
    if(f == NULL) 
      goto bomb;

    if(fprintf(f, "%-13.13s index.....\n",carmel_pretty_mailbox(mailbox))==EOF)
      goto bomb;

    if(fclose(f) == EOF)
      goto bomb;

    /*--- The carmel2 index ----*/
    carmel2_index_file = carmel_calc_paths(CalcPathCarmel2Index, mailbox,0);
    f = fopen(carmel2_index_file, "w");
    if(f == NULL)
      goto bomb;

    if(fprintf(f, "\254\312--CARMEL-MAIL-FILE-INDEX--\n") == EOF)
      goto bomb;

    if(fclose(f) == EOF)
      goto bomb;


    carmel_kill_locks(mailbox); /* Get rid of any left over locks */
    carmel_reset_index_list();
    return(T);

  bomb:
    unlink(carmel_calc_paths(CalcPathCarmelIndex, mailbox, 0));
    sprintf(carmel_error_buf, "Error creating mailbox %s: %s",
	    carmel_pretty_mailbox(mailbox), strerror(errno));
    mm_log(carmel_error_buf, ERROR);
    return(NIL);
}



/*----------------------------------------------------------------------
    Delete a carmel mailbox, and it's carmel2 index

Args: stream --  unsed here
      mailbox -- FQN of mailbox to delete

Returns: NIL -- if delete fails 
         T   -- if successful
  ----*/
long
carmel_delete(stream, mailbox)
     MAILSTREAM *stream;
     char       *mailbox;
{
    char *carmel_index_file, *carmel2_index_file;

    carmel_index_file = carmel_calc_paths(CalcPathCarmelIndex, mailbox, 0);
    if(carmel_index_file == NULL)
      return(NIL);

    if(unlink(carmel_index_file) < 0) {
	sprintf(carmel_error_buf, "Error deleting mailbox %s: %s",
		carmel_pretty_mailbox(mailbox), strerror(errno));
	mm_log(carmel_error_buf, ERROR); 
	return(NIL);
    }

    /*------ Second the carmel2 index, quietly -----*/
    carmel2_index_file = carmel_calc_paths(CalcPathCarmel2Index, mailbox, 0);

    unlink(carmel2_index_file);

    carmel_kill_locks(mailbox); /* Make sure all locks are clear */
    carmel_reset_index_list();
    return(T);
}



/*----------------------------------------------------------------------
   Rename a carmel index, and its carmel2 index

Args:  stream -- unused
       orig   -- FQN of original mailbox 
       new    -- FQN of new name

Returns: NIL if rename failed, T if it succeeded.

BUG: theoretically this could rename a folder from one user name 
to another. Either that should be disallowed here, or code to make
it work should be written.
  ----*/
long
carmel_rename(stream, orig, new)
     MAILSTREAM *stream;
     char       *orig;
     char       *new;
{
    char path_buf[CARMEL_PATHBUF_SIZE], *new_path, *orig_index_file;

    /*---- First the Carmel index -----*/
    orig_index_file = carmel_calc_paths(CalcPathCarmelIndex, orig, 0);
    if(orig_index_file == NULL)
      return(NIL);
    strcpy(path_buf, orig_index_file);
    new_path = carmel_calc_paths(CalcPathCarmelIndex, new, 0);
    if(new_path == NULL)
      return(NIL);

    if(rename(path_buf, new_path) < 0) {
	sprintf(carmel_error_buf, "Error renaming mailbox %s: %s",
		carmel_pretty_mailbox(orig), strerror(errno));
	mm_log(carmel_error_buf, ERROR); 
	return(NIL);
    }

    /*----- Next the Carmel index, quietly ------*/
    strcpy(path_buf, carmel_calc_paths(CalcPathCarmel2Index, orig, 0));
    new_path = carmel_calc_paths(CalcPathCarmel2Index, new, 0);

    rename(path_buf, new_path);

    carmel_reset_index_list();
    return(T);
}



/*----------------------------------------------------------------------
  Carmel mail open

 Args: stream 

 Returns: stream on success, NIL on failure.

The complex set of comparison determine if we get it for read, write or not
at all.

The folder will be open for write if:
   - It is not locked
   - The carmel index is writeable 
   - The carmel index is writeable
   - The carmel index is openable and not corrupt

The folder open will fail if:
   - The carmel index does exist
   - The carmel carmel index is missing
        AND the folder is locked or unwritable

The folder is opened readonly if:
   - It is locked
   - The carmel index is not writable
   - The carmel index is not writable

 ----*/

MAILSTREAM *
carmel_open(stream)
	MAILSTREAM *stream;
{
    char            carmel_index_path[CARMEL_PATHBUF_SIZE];
    char            carmel2_index_path[CARMEL_PATHBUF_SIZE];
    char            tmp[MAILTMPLEN], tmp2[MAILTMPLEN];
    struct stat     carmel_stat, carmel2_stat;
    int             carmel2_up_to_date;

    if(!stream) 
      return(&carmelproto); /* Must be a OP_PROTOTYPE call */

    mailcache = carmel2_cache;

    /* close old file if stream being recycled */
    if (LOCAL) {
        carmel_close (stream);		/* dump and save the changes */
        stream->dtb = &carmeldriver;	/* reattach this driver */
        mail_free_cache (stream);	/* clean up cache */
    }

    /* Allocate local stream */
    stream->local              = fs_get (sizeof (CARMEL2LOCAL));
    LOCAL->carmel              = 1;
    LOCAL->msg_buf             = NULL;
    LOCAL->msg_buf_size        = 0;
    LOCAL->buffered_file       = NULL;
    LOCAL->msg_buf_text_start  = NULL;
    LOCAL->msg_buf_text_offset = 0;
    LOCAL->stdio_buf           = NULL;
    LOCAL->dirty               = NIL;
    stream->msgno              = -1;
    stream->env                = NULL;
    stream->body               = NULL;
    stream->scache             = 1;
    LOCAL->calc_paths          = carmel_calc_paths;
    LOCAL->aux_copy            = carmel_copy;
    LOCAL->index_stream        = NULL;
    LOCAL->new_file_on_copy    = 0;

    /*------ Figure out the file paths -------*/
    if(carmel_calc_paths(CalcPathCarmelIndex,stream->mailbox,0) == NULL) 
      return(NULL);
    strcpy(carmel_index_path,
	   carmel_calc_paths(CalcPathCarmelIndex,stream->mailbox,0));
    strcpy(carmel2_index_path,
	   carmel_calc_paths(CalcPathCarmel2Index,stream->mailbox,0));

    /*------ Does the CARMEL index exist? Fail if not ------*/
    if(stat(carmel_index_path, &carmel_stat) < 0) {
	sprintf(carmel_error_buf, "Can't open mailbox: %s", strerror(errno));
	mm_log(carmel_error_buf, ERROR);
        return(NULL);  /* FAIL, no carmel index */
    }

    /*----- Determine if carmel index is up to date -----*/
    if(stat(carmel2_index_path, &carmel2_stat) >= 0 &&
       carmel2_stat.st_mtime >= carmel_stat.st_mtime)
      carmel2_up_to_date = 1;
    else
      carmel2_up_to_date = 0;

    /*------ Do we have write access to the Carmel mail index? ----*/
    if(access(carmel2_index_path, W_OK) != 0 && errno != ENOENT)
      stream->readonly = 1; /* fail later if dir index is uncreatable */

    /*------ If CARMEL index R/O and Carmel out of date then fail -----*/
    if(stream->readonly && !carmel2_up_to_date) {
	mm_log("Can't open mailbox; Unable to write carmel index", ERROR);
        return(NULL);
    }

    /*-------- Case for R/O stream or failed lock ---------*/
    if(stream->readonly ||
       carmel2_lock(stream->local, stream->mailbox, READ_LOCK)< 0){
	/* If carmel is out of date here that's OK, we just will see old data*/
	if(access(carmel_index_path, R_OK) == 0) {
	    stream->readonly = 1;
  	    goto open_it;
	} else {
	    /*-- Can't lock, and there's no carmel index, best to fail -*/
	    mm_log("Can't open mailbox, can't lock to create carmel index",
		   ERROR);
  	    return(NULL);
        }
    } 
    /* Index is now read locked */

    /*---- Got an up to date carmel index and write access too ----*/
    if(carmel2_up_to_date)
      if(access(carmel2_index_path, W_OK) == 0) {
          goto open_it;
      } else {
	  stream->readonly = 1;
	  goto open_it;
      }

    /*---- If needed recreation of carmel index fails, go readonly -----*/
    if(vmail2carmel(carmel_index_path, carmel2_index_path, stream,
                     stream->mailbox) < 0) {
	carmel2_unlock(stream->local, stream->mailbox, READ_LOCK);
        stream->readonly = 1;
    }

  open_it:
    /*-- carmel_open2 shouldn't fail after all this check, but just in case -*/
    if(carmel_open2(stream, carmel2_index_path) < 0) {
	/* carmel_open2 will take care of the error message */
	carmel2_unlock(stream->local, stream->mailbox, READ_LOCK);
	if (LOCAL->msg_buf) fs_give ((void **) &LOCAL->msg_buf);
				    /* nuke the local data */
	fs_give ((void **) &stream->local);
        return(NULL);
    }

    if(stream->readonly)
	mm_log("Mailbox opened READONLY", WARN);

    mail_exists (stream,stream->nmsgs);
    mail_recent (stream,stream->recent);

    return(stream);
}



/*----------------------------------------------------------------------
   Create a carmel2 index for a carmel index
  
 Args: folder        -- the full UNIX path name existing carmel folder
       carmel_folder -- the full UNIX path name of carmel index to create
       stream        -- MAILSTREAM to operate on
       
 Returns: 0 if all is OK
         -1 if it failed 

  This reads the Carmel index, and the headers of the carmel messages to 
construct entries for a carmel index.
 ----*/
static
vmail2carmel(folder, carmel2_folder, stream, mailbox)
     MAILSTREAM *stream;
     char       *folder, *carmel2_folder;
     char       *mailbox;
{
    FILE        *carmel_stream;
    FILE        *carmel2_stream;
    ENVELOPE    *e;
    BODY        *b;
    MESSAGECACHE mc;
    char         pathbuf[CARMEL_PATHBUF_SIZE], *message, *p, *save_subject;
    struct stat  st;
    STRING       string_struct;

    sprintf(pathbuf, "Recreating Carmel2 index for \"%s\"...",
            carmel_pretty_mailbox(mailbox));
    mm_log(pathbuf, WARN);

    mm_critical(stream);
    if(carmel2_lock(stream->local, mailbox, WRITE_LOCK) < 0)
      return(-1);
    
    carmel_stream = fopen(folder,"r");
    if(carmel_stream == NULL) {
	carmel2_unlock(stream->local, mailbox, WRITE_LOCK);
        mm_nocritical(stream);
        return(-1);
    }

    carmel2_stream = fopen(carmel2_folder, "w");
    if(carmel2_stream == NULL) {
	carmel2_unlock(stream->local, mailbox, WRITE_LOCK);	
        mm_nocritical(stream);
        return(-1);
    }

    fprintf(carmel2_stream, "\254\312--CARMEL-MAIL-FILE-INDEX--\n");

    /* The first ....  line */
    fgets(carmel_20k_buf, sizeof(carmel_20k_buf), carmel_stream); 

    /*---- Loop through all entries in carmel index -----*/
    while(fgets(carmel_20k_buf,sizeof(carmel_20k_buf),carmel_stream) != NULL){

	mc.data1      = 0L;
	mc.data2      = atol(carmel_20k_buf); /* The file name/number */
	mc.deleted    = 0;
	mc.seen       = 1;
	mc.flagged    = 0;
	mc.answered   = 0;
	mc.recent     = 0;
	mc.user_flags = 0;

	strcpy(pathbuf,carmel_calc_paths(CalcPathCarmel2Data,
					 mailbox, mc.data2));

	if(stat(pathbuf, &st) >= 0 ||
	   stat(strcat(pathbuf, ".wid"), &st) >= 0) {
	    save_subject = cpystr(carmel_20k_buf + 27);
	    save_subject[strlen(save_subject) - 1] = '\0';
            mc.rfc822_size = st.st_size;
            message = carmel2_readmsg(stream, 1, 0, mc.data2);
            INIT(&string_struct, mail_string, (void *)"", 0);
	    rfc822_parse_msg(&e, &b, message, strlen(message), &string_struct,
			     mylocalhost(), carmel_20k_buf);
	    carmel2_parse_bezerk_status(&mc, message);
	    carmel2_rfc822_date(&mc, message);
	    if(e->subject == NULL ||
	     strncmp(save_subject,e->subject,strlen(save_subject))){
		if(e->subject != NULL)
		  fs_give((void **)(&(e->subject)));
		if(strlen(save_subject))
		  e->subject = cpystr(save_subject);
		else
		  e->subject = NULL;
	    }
	} else {
            /* Data file is missing; copy record as best we can */
	    carmel_20k_buf[strlen(carmel_20k_buf) - 1] = '\0';
            e = mail_newenvelope();
	    e->subject = cpystr(carmel_20k_buf+27);
	    e->from  = mail_newaddr();
	    e->from->mailbox = cpystr(carmel_20k_buf+17);
	    for(p = e->from->mailbox; *p && !isspace(*p); p++);
	    *p = '\0';
	    mc.hours     = 1;
	    mc.minutes   = 0;
	    mc.seconds   = 0;
	    mc.zoccident = 0;
	    mc.zhours    = 0;
	    mc.zminutes  = 0;
	    mc.day       = 1;
	    mc.month     = 1;
	    mc.year      = 1;
	}
	carmel2_write_index(e, &mc, carmel2_stream);
	mail_free_envelope(&e);
    }

    fclose(carmel_stream);
    fclose(carmel2_stream);

    carmel2_unlock(stream->local, mailbox, WRITE_LOCK);
    mm_nocritical(stream);

    mm_log("Recreating Carmel2 index.....Done", WARN);

    return(1);
}



#define TESTING /* Make a copy of carmel index before rewriting */

/*----------------------------------------------------------------------`
   Close a carmel mail folder

Args: stream -- Mail stream to close

This will cause the carmel index to be recreated

  ----*/
void 
carmel_close(stream)
	MAILSTREAM *stream;
{
    if (LOCAL && LOCAL->index_stream != NULL) {
        /* only if a file is open */
        if(!stream->readonly) {
    	    carmel_check (stream);		/* dump final checkpoint */
            carmel2_unlock(stream->local, stream->mailbox, READ_LOCK);
        }

        fclose(LOCAL->index_stream);
        if (LOCAL->msg_buf) fs_give ((void **) &LOCAL->msg_buf);
          if(LOCAL->stdio_buf)
          fs_give ((void **) &LOCAL->stdio_buf);
	fs_give ((void **) &stream->local);

    }
    stream->dtb = NIL;		/* log out the DTB */
}



/*----------------------------------------------------------------------
  Check for new mail and write out the indexes as a checkpoint

Args -- The stream to checkpoint
 ----*/
void
carmel_check(stream)
     MAILSTREAM *stream;
{
    if(carmel2_check2(stream))
      carmel_recreate_index(stream); /* only if carmel2 index changed */
}




/*----------------------------------------------------------------------
   Expunge the mail stream

Args -- The stream to checkpoint

Here we expunge the carmel2 index and then recreate the carmel index from
it. 
 ----*/
void
carmel_expunge(stream)
     MAILSTREAM *stream;
{
    carmel2_expunge(stream);
    carmel_recreate_index(stream); 
}



/*----------------------------------------------------------------------
   Rewrite the carmel index from the carmel2 index
 
Args: -- stream  to be recreated

BUG: could probably use some error checking here
  ----*/
static void
carmel_recreate_index(stream)
     MAILSTREAM *stream;
{
#ifdef TESTING
    char          copy1[CARMEL_PATHBUF_SIZE];
    char          copy2[CARMEL_PATHBUF_SIZE];
#define MAXBACKUPS 4
    int nback;
#endif
    long          msgno;
    MESSAGECACHE *mc;
    ENVELOPE     *envelope;
    FILE         *carmel_index;
    struct stat   sb;
    char         *cm_index_file;
    struct timeval tp[2];

    mm_critical(stream);

    /*---- calculate the carmel index path -----*/
    strcpy(carmel_path_buf,
           (*(LOCAL->calc_paths))(CalcPathCarmelIndex, stream->mailbox, 0));

    /*---- Get the first index line out of old index ----*/
    *carmel_20k_buf = '\0';
    carmel_index = fopen(carmel_path_buf, "r");
    if(carmel_index != NULL) {
        fgets(carmel_20k_buf,sizeof(carmel_20k_buf), carmel_index);
        fclose(carmel_index);
    }

#ifdef TESTING
    /*--- rotate Backup copies of the index --- crude*/
    for (nback = MAXBACKUPS; 1 < nback; --nback) {
      strcpy(copy2,
           (*(LOCAL->calc_paths))(CalcPathCarmelBackup, stream->mailbox, nback));
      strcpy(copy1,
           (*(LOCAL->calc_paths))(CalcPathCarmelBackup, stream->mailbox, nback-1));
      unlink(copy2);
      link(copy1,copy2);
      }
    unlink(copy1);
    link(carmel_path_buf, copy1);
    unlink(carmel_path_buf);
#endif		

    /*---- Reopen the carmel index for write, truncating it ------*/	    
    carmel_index = fopen(carmel_path_buf, "w");

    if(carmel_index == NULL) 
	goto bomb;
	
    if(fputs(carmel_20k_buf, carmel_index) == EOF)
      goto bomb;

    /*---- Loop through all the message the stream has ----*/	    
    for(msgno = 1; msgno <= stream->nmsgs; msgno++) {
        mc = MC(msgno);
    	envelope = carmel2_fetchstructure(stream, msgno, NULL);
    	if(write_carmel_index(carmel_index, mc, envelope) < 0)
    	  goto bomb;
    }
    if(fclose(carmel_index) == EOF)
      goto bomb;

    /*---- Get mod time of carmel index ---*/
    cm_index_file = (*(LOCAL->calc_paths))(CalcPathCarmelIndex,
					   stream->mailbox, 0);
    if(stat(cm_index_file, &sb) >= 0) {
	tp[0].tv_sec  = sb.st_atime;
	tp[0].tv_usec = 0;
	tp[1].tv_sec  = sb.st_mtime;
	tp[1].tv_usec = 0;
    
	/*---- Set Carmel index to have same mod time ---*/
	utimes(stream->mailbox, tp);
    }
    mm_nocritical(stream);

#ifdef CHATTY
    sprintf(carmel_error_buf, "Rewrote Carmel index \"%s\" with %d messages",
	    carmel_pretty_mailbox(stream->mailbox), msgno-1);
    mm_log(carmel_error_buf, WARN);
#endif

    return;

  bomb:
    mm_nocritical(stream);
    sprintf(carmel_error_buf, "Error recreating carmel index: %s",
   	    strerror(errno));
    mm_log(carmel_error_buf, ERROR);
}



/*----------------------------------------------------------------------
  Do all the file name generation for the carmel driver

Args: operation -- The type of calculation to perform
      mailbox   -- The canonical mailbox name
      data_file_num -- For calculating the name of a file storing a message

Returns:  string with the path name in static storage or NULL on error

The Path passed in will be in the format #carmel#user-id#folder or
                                         #carmel#folder

The only that error occurs is in looking up the user-id. A error is logged
when that happens.
  ----*/
static char *
carmel_calc_paths(operation, mailbox, data_file_num)
     char *mailbox;
     int   data_file_num, operation;
{
    static char    path[CARMEL_PATHBUF_SIZE];
    char          *home_dir, *end_user_name;
    struct passwd *pw;
    struct carmel_mb_name *parsed_name;

    parsed_name = carmel_parse_mb_name(mailbox, '\0');

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
            return(NULL);
        }
        home_dir = pw->pw_dir;
    } else {
        home_dir = myhomedir();
    }
    mailbox  = parsed_name->mailbox;

    if(strucmp2(mailbox, "inbox") == 0) /* Map "inbox" into "MAIL" */
      mailbox = "MAIL";
    

    switch(operation) {

      case CalcPathCarmelIndex:
        sprintf(path, "%s/%s/%s", home_dir, CARMEL_INDEX_DIR, mailbox);
        break;

      case CalcPathCarmelBackup:
        sprintf(path, "%s/%s/.%s.COD-FUR-%d", home_dir, CARMEL_INDEX_DIR,
                mailbox, data_file_num);
        break;

      case CalcPathCarmel2Data:
	sprintf(path, "%s/%s/%d", home_dir, CARMEL_MSG_DIR, data_file_num);
	break;

      case CalcPathCarmel2Index:
        sprintf(path, "%s/%s/.%s.cx", home_dir, CARMEL_INDEX_DIR, mailbox);
        break;

      case CalcPathCarmel2MAXNAME:
        sprintf(path, "%s/%s", home_dir, CARMEL_MAXFILE);
        break;

      case CalcPathCarmel2WriteLock:
        sprintf(path, "%s/%s/.%s.wl", home_dir, CARMEL_INDEX_DIR, mailbox);
        break;

      case CalcPathCarmel2ReadLock:
        sprintf(path, "%s/%s/.%s.rl", home_dir, CARMEL_INDEX_DIR, mailbox);
        break;
    }

/*    sprintf(carmel_debug, "CARMEL: calc_paths returning \"%s\"\n", path);
    mm_dlog(carmel_debug);   */

    carmel_free_mb_name(parsed_name);

    return(path);
}



/*----------------------------------------------------------------------
    Copy carmel messages, this is the auxiliary copy, called from carmel2_copy

Args:  local -- Fake CARMELLOCAL structure
       mailbox -- Destination mailbox, in FQN format, e.g. #carmel#fooo
       e       -- envelope
       mc      -- MESSAGECACHE entry

Retuns: -1 on failure
  ----*/
long
carmel_copy(local, mailbox, e, mc)
     CARMEL2LOCAL *local;
     char         *mailbox;
     ENVELOPE     *e;
     MESSAGECACHE *mc;
{
    FILE       *carmel_index;
    int         new;
    struct stat sb;
    char        *carmel_index_file;

    carmel_index_file = (*(local->calc_paths))(CalcPathCarmelIndex, mailbox,0);
    if(carmel_index_file == NULL)
      return(-1);
    
    if(stat(carmel_index_file, &sb) < 0)
      new = 1;
    else
      new = 0;

    carmel_index = fopen(carmel_index_file, "a+");
    if(carmel_index == NULL)
      return(-1);
    
    if(new)
      fprintf(carmel_index, "%-13.13s index.....\n",
              carmel_pretty_mailbox(mailbox));

    if(write_carmel_index(carmel_index, mc, e) < 0) {
	fclose(carmel_index);
	return(-1);
    }

    if(fclose(carmel_index) == EOF)
      return(-1);

    return(0);
}


    
/*----------------------------------------------------------------------
    Make sure all the directories are there and writable for carmel

Args: folders_dir   - suggested directory, ignored by carmel

Result: returns 0 if OK, -1 if not
 ----*/
carmel_init(folders_dir)
     char *folders_dir;
{
    char  *p;
    FILE  *mail_file;
    struct stat sb;

    /*----- The ~/.vmail directory ----*/
    sprintf(carmel_path_buf, "%s/%s", myhomedir(), CARMEL_DIR);
    carmel2_check_dir(carmel_path_buf);

    /*---- The ~/.vmail/.MAXNAME file ------*/
    sprintf(carmel_path_buf,"%s/%s", myhomedir(), CARMEL_MAXFILE);
    if(stat(carmel_path_buf, &sb) < 0) {
	mail_file = fopen(carmel_path_buf, "w");
	if(mail_file == NULL) {
	    mm_log("Error creating .MAXNAME file", ERROR);
	} else {
	    fprintf(mail_file, "100000");
	    fclose(mail_file);
	}
    }

    /*----- The ~/.vmail/index directory -----*/
    sprintf(carmel_path_buf,"%s/%s",myhomedir(),CARMEL_INDEX_DIR);
    carmel2_check_dir(carmel_path_buf);

    /*----- The ~/.vmail/msg directory -----/*
    sprintf(carmel_path_buf,"%s/%s",myhomedir(),CARMEL_MSG_DIR);
    carmel2_check_dir(carmel_path_buf);

    /*----- The ~/.vmail/MAIL file -----*/
    sprintf(carmel_path_buf, "%s/%s/MAIL", myhomedir(),  CARMEL_INDEX_DIR);
    if(stat(carmel_path_buf, &sb) < 0) {
        mail_file = fopen(carmel_path_buf, "w");
	if(mail_file == NULL) {
	    mm_log("Error creating \"MAIL\" folder", WARN);
	} else {
 	    fprintf(mail_file, "MAIL          index.....\n");
	    fclose(mail_file);
	}
    }

    return(0);
}



/*----------------------------------------------------------------------
   Write an entry into a carmel index
 
Args: file -- The open file stream
      mc   -- the MESSAGECACHE 
      envelope -- The envelope to write

Returns: 0 if all is OK, -1 if not
  ----*/	
static 	
write_carmel_index(file, mc, envelope)
     FILE         *file;
     MESSAGECACHE *mc;
     ENVELOPE     *envelope;
{
    if(fprintf(file, "%6d %02d %-.3s %2d %-9.9s %-.50s\n",
	       mc->data2, mc->day,
	       month_abbrev2(mc->month),
	       (mc->year + 1969) % 100,
	       envelope != NULL && envelope->from != NULL &&
	       envelope->from->mailbox != NULL ?
	         envelope->from->mailbox :  "",
	       envelope != NULL && envelope->subject != NULL ?
	         envelope->subject : "") == EOF)
      return(-1);
    else
      return(0);
}



/*----------------------------------------------------------------------
    Append a message to a carmel mailbox

Args: stream  --  a suggested stream, ignored here
      mailbox --  FQN of mailbox to append message to
      message --  Text string of message

Returns: T on success, NIL on failure
  ----*/    
long
carmel_append(stream, mailbox, flags, date, message)
     MAILSTREAM *stream;
     char       *mailbox, *flags, *date;
     STRING     *message;
{
    CARMEL2LOCAL local;

    /*---- A fake local data structure to pass to other functions---*/
    local.calc_paths = carmel_calc_paths;
    local.carmel     = 1;
    local.aux_copy   = carmel_copy;


    return(carmel2_append2(stream, &local, mailbox, flags, date, message));
}





/*----------------------------------------------------------------------
  This really just removes the file Carmel uses for the list of 
 folders, because Carmel will just recreate it from the indexes themselves.
  ----*/
static 
carmel_reset_index_list()
{
    sprintf(carmel_path_buf, "%s/.inda", myhomedir());
    unlink(carmel_path_buf);

    sprintf(carmel_path_buf, "%s/.indf", myhomedir());
    unlink(carmel_path_buf);
}



/*----------------------------------------------------------------------
    Used to make sure there are no old locks of any sort left around 
 when a folder is deleted or when one is created. 
  --*/
static void   
carmel_kill_locks(mailbox)
     char *mailbox;
{
    unlink(carmel_calc_paths(CalcPathCarmel2WriteLock, mailbox, 0));
    unlink(carmel_calc_paths(CalcPathCarmel2ReadLock, mailbox, 0));
    unlink(carmel_calc_paths(CalcPathCarmel2Expunge, mailbox, 0));
}
