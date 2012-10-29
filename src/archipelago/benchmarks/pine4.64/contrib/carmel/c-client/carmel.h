/*----------------------------------------------------------------------

         T H E   C A R M E L    M A I L   D R I V E R

  Author(s):   Laurence Lundblade
               Baha'i World Centre
               Data Processing
               Haifa, Israel
 	       Internet: lgl@cac.washington.edu or laurence@bwc.org
               September 1992
 Last edited:  Aug 31, 1994               
  
  ----------------------------------------------------------------------*/


/* Most of the real stuff is defined in carmel2.h */

/* Function prototypes */

#ifdef ANSI
DRIVER	   *carmel_valid(char *);
int	    carmel_isvalid(char *);
void       *carmel_parameters();
char	   *carmel_file  (char *, char *);
void        carmel_find(MAILSTREAM *, char *);
void        carmel_find_all(MAILSTREAM *, char *);
void        carmel_find_bboards(MAILSTREAM *, char *);
long	    carmel_subscribe();
long	    carmel_unsubscribe();
long	    carmel_subscribe_bboard();
long	    carmel_unsubscribe_bboard();
long        carmel_create(MAILSTREAM *, char *);
long        carmel_delete(MAILSTREAM *, char *);
long        carmel_rename(MAILSTREAM *, char *, char *);
MAILSTREAM *carmel_open(MAILSTREAM *);
void        carmel_close(MAILSTREAM *);
void        carmel_check(MAILSTREAM *);
void        carmel_void(MAILSTREAM *);
long        carmel_copy(CARMEL2LOCAL *, char *, ENVELOPE *, MESSAGECACHE *);
int         carmel_init(char *);
long        carmel_append(MAILSTREAM *, char *, char *, char *, STRING *);
#else
DRIVER	   *carmel_valid();
int	    carmel_isvalid();
void       *carmel_parameters();
char	   *carmel_file  ();
void        carmel_find();
void        carmel_find_all();
void        carmel_find_bboards();
long	    carmel_subscribe();
long	    carmel_unsubscribe();
long	    carmel_subscribe_bboard();
long	    carmel_unsubscribe_bboard();
long        carmel_create();
long        carmel_delete();
long        carmel_rename();
MAILSTREAM *carmel_open();
void        carmel_close();
void        carmel_check();
void        carmel_expunge();
long        carmel_copy();
int         carmel_init();
long        carmel_append();
#endif


/* These are all relative to the users home directory */
#define CARMEL_INDEX_DIR  ".vmail/index"
#define CARMEL_DIR        ".vmail"
#define CARMEL_MSG_DIR    ".vmail/msg"
#define CARMEL_MAXFILE    ".vmail/.MAXNAME"

/* operations for carmel_calc_path -- must mesh with operations for 
   carmel2_calc_path in carmel2.h
 */
#define CalcPathCarmelIndex  100
#define CalcPathCarmelBackup 101
