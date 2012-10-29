/*----------------------------------------------------------------------

    T H E   C A R M E L 2   M A I L   F I L E   D R I V E R

 Author(s):   Laurence Lundblade
              Baha'i World Centre
              Data Processing
              Haifa, Israel
	      Internet: lgl@cac.washington.edu or laurence@bwc.org
              September 1992

 Last Edited: Aug 31, 1994
  ----------------------------------------------------------------------*/

/* Command bits from carmel2_getflags() */

#define fSEEN     1
#define fDELETED  2
#define fFLAGGED  4
#define fANSWERED 8
#define fRECENT  16

/* Kinds of locks for carmel2_lock() */

#define READ_LOCK   0
#define WRITE_LOCK  1


/* Carmel2 I/O stream local data */

typedef struct _local2 {
  MESSAGECACHE **mc_blocks;
  long           cache_size;
  FILE          *index_stream;
  char          *stdio_buf;
  char          *msg_buf;
  unsigned long  msg_buf_size;
  unsigned long  msg_buf_text_offset;
  char          *msg_buf_text_start;
  char          *buffered_file;
  long           read_lock_mod_time, write_lock_mod_time;
  long           index_size;
  unsigned int   dirty:1;
  unsigned int   carmel:1;  /* It's a carmel file instead of carmel2 */
  unsigned int   buffered_header_and_text:1;
  unsigned int   new_file_on_copy:1;
  char        *(*calc_paths)();
  long         (*aux_copy)();
} CARMEL2LOCAL;


struct carmel_mb_name {
    char  version[2]; /* \0 for version 1, ASCII digit  and \0 for other */
    char *user;    /* String of userid for other user names */
    char *mailbox; /* Mailbox name */
};


#define MC(x) (&(LOCAL->mc_blocks[(x) >> 8][(x) & 0xff]))
#define LOCAL ((CARMEL2LOCAL *) stream->local)
#define CARMEL_MAXMESSAGESIZE (20000000)  /* 20Mb */
#define CARMEL_MAX_HEADER        (64000)  /* 64K for DOS (someday?) */
#define CARMEL_PATHBUF_SIZE       (1024)
#define CARMEL2_INDEX_BUF_SIZE   (20000)  /* Size for carmel2 index FILE buf */
#define CARMEL_NAME_CHAR           ('#')  /* Separator for carmel names */
#define CARMEL_NAME_PREFIX     "#carmel"  /* Prefix for all mailbox names */


/* Kinds of paths that for carmel_calc_path */

#define CalcPathCarmel2Index     1
#define CalcPathCarmel2Data      2
#define CalcPathCarmel2MAXNAME   3
#define CalcPathCarmel2WriteLock 4
#define CalcPathCarmel2ReadLock  5
#define CalcPathCarmel2Expunge   6


/* These are all relative to the users home directory */

#define CARMEL2_INDEX_DIR  "Carmel2Mail"
#define CARMEL2_DIR        "Carmel2Mail"
#define CARMEL2_MSG_DIR    "Carmel2Mail/.Messages"
#define CARMEL2_MAXFILE    "Carmel2Mail/.MAXNAME"


#define BEZERKLOCKTIMEOUT 5

/* Function prototypes */

#ifdef ANSI
DRIVER	    *carmel2_valid(char *);
int	     carmel2_isvalid(char *);
char	    *carmel2_file(char *, char *);
void        *carmel2_parameters();
void	     carmel2_find(MAILSTREAM *, char *);
void	     carmel2_find_bboards(MAILSTREAM *, char *);
void         carmel2_find_all(MAILSTREAM *, char *);
void         carmel2_find_all_bboards(MAILSTREAM *, char *);
long         carmel2_subscribe();
long         carmel2_unsubscribe();
long         carmel2_subscribe_bboard();
long         carmel2_unsubscribe_bboard();
long         carmel2_create(MAILSTREAM *, char *);
long         carmel2_delete(MAILSTREAM *, char *);
long         carmel2_rename(MAILSTREAM *, char *, char *);
MAILSTREAM  *carmel2_open(MAILSTREAM *);
int          carmel2_open2(MAILSTREAM *, char *);
void         carmel2_close(MAILSTREAM *);
void         carmel2_fetchfast(MAILSTREAM *, char *);
void         carmel2_fetchflags(MAILSTREAM *, char *);
ENVELOPE    *carmel2_fetchstructure(MAILSTREAM *, long, BODY **);
char        *carmel2_fetchheader(MAILSTREAM *, long);
char        *carmel2_fetchtext(MAILSTREAM *, long);
char        *carmel2_fetchbody(MAILSTREAM *, long, char *, unsigned long *);
void         carmel2_setflag(MAILSTREAM *, char *, char *);
void         carmel2_clearflag(MAILSTREAM *, char *, char *);
void         carmel2_search(MAILSTREAM *, char *);
long         carmel2_ping(MAILSTREAM *);
void         carmel2_check(MAILSTREAM *);
void         carmel2_expunge(MAILSTREAM *);
long         carmel2_copy(MAILSTREAM *, char *, char *);
long         carmel2_move(MAILSTREAM *, char *, char *);
void         carmel2_gc(MAILSTREAM *, long);
void        *carmel2_cache(MAILSTREAM *, long, long);
long         carmel2_append(MAILSTREAM *, char *, STRING *);
int          carmel2_write_index(ENVELOPE *, MESSAGECACHE *, FILE *);
char        *carmel2_readmsg(MAILSTREAM *, int, long, int);
int          carmel2_lock(CARMEL2LOCAL *, char *, int);
void         carmel2_unlock(CARMEL2LOCAL *, char *, int);
int          carmel2_update_lock(CARMEL2LOCAL *, char *, int);
int          carmel2_check_dir(char *);
void         carmel2_parse_bezerk_status(MESSAGECACHE *, char *);
int          carmel2_append2(MAILSTREAM *, CARMEL2LOCAL *, char *, char *,
                             char *, STRING *);
char        *month_abbrev2(int);
void         carmel2_rfc822_date(MESSAGECACHE *, char *);
char        *carmel_pretty_mailbox(char *);
struct carmel_mb_name *
             carmel_parse_mb_name(char *, char);
void         carmel_free_mb_name(struct carmel_mb_name *);
int          strucmp2(char *, char *);
int          struncmp2(char *, char *, int);

#else  /* ANSI */


DRIVER	    *carmel2_valid();
int	     carmel2_isvalid();
char	    *carmel2_file();
void        *carmel2_parameters();
void	     carmel2_find();
void	     carmel2_find_bboards();
void         carmel2_find_all();
void         carmel2_find_all_bboards();
long         carmel2_subscribe();
long         carmel2_unsubscribe();
long         carmel2_subscribe_bboard();
long         carmel2_unsubscribe_bboard();
long         carmel2_create();
long         carmel2_delete();
long         carmel2_rename();
MAILSTREAM  *carmel2_open();
int          carmel2_open2();
void         carmel2_close();
void         carmel2_fetchfast();
void         carmel2_fetchflags();
ENVELOPE    *carmel2_fetchstructure();
char        *carmel2_fetchheader();
char        *carmel2_fetchtext();
char        *carmel2_fetchbody();
void         carmel2_setflag();
void         carmel2_clearflag();
void         carmel2_search();
long         carmel2_ping();
void         carmel2_check();
void         carmel2_expunge();
long         carmel2_copy();
long         carmel2_move();
void         carmel2_gc();
void        *carmel2_cache();
long         carmel2_append();
int          carmel2_write_index();
char        *carmel2_readmsg();
int          carmel2_lock();
void         carmel2_unlock();
int          carmel2_update_lock();
int          carmel2_check_dir();
void         carmel2_parse_bezerk_status();
int          carmel2_append2();
char        *month_abbrev2();
void         carmel2_rfc822_date();
char        *carmel_pretty_mailbox();
struct carmel_mb_name *
             carmel_parse_mb_name();
void         carmel_free_mb_name();
int          strucmp2();
int          struncmp2();

#endif  /* ANSI */
