#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: filter.c 14075 2005-08-30 00:08:19Z hubert@u.washington.edu $";
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
     filter.c

     This code provides a generalized, flexible way to allow
     piping of data thru filters.  Each filter is passed a structure
     that it will use to hold its static data while it operates on
     the stream of characters that are passed to it.  After processing
     it will either return or call the next filter in
     the pipe with any character (or characters) it has ready to go. This
     means some terminal type of filter has to be the last in the
     chain (i.e., one that writes the passed char someplace, but doesn't
     call another filter).

     See below for more details.

     The motivation is to handle MIME decoding, richtext conversion,
     iso_code stripping and anything else that may come down the
     pike (e.g., PEM) in an elegant fashion.  mikes (920811)

   TODO:
       reasonable error handling

  ====*/


#include "headers.h"


/*
 * Internal prototypes
 */
int	gf_so_readc PROTO((unsigned char *));
int	gf_so_writec PROTO((int));
int	gf_sreadc PROTO((unsigned char *));
int	gf_swritec PROTO((int));
int	gf_freadc PROTO((unsigned char *));
int	gf_fwritec PROTO((int));
int	gf_preadc PROTO((unsigned char *));
int	gf_pwritec PROTO((int));
void	gf_terminal PROTO((FILTER_S *, int));
char   *gf_filter_puts PROTO((char *));
void	gf_filter_eod PROTO((void));
void    gf_error PROTO((char *));
void	gf_8bit_put PROTO((FILTER_S *, int));
int	so_reaquire PROTO((STORE_S *));
void   *so_file_open PROTO((STORE_S *));
int	so_cs_writec PROTO((int, STORE_S *));
int	so_pico_writec PROTO((int, STORE_S *));
int	so_file_writec PROTO((int, STORE_S *));
int	so_cs_readc PROTO((unsigned char *, STORE_S *));
int	so_pico_readc PROTO((unsigned char *, STORE_S *));
int	so_file_readc PROTO((unsigned char *, STORE_S *));
int	so_cs_puts PROTO((STORE_S *, char *));
int	so_pico_puts PROTO((STORE_S *, char *));
int	so_file_puts PROTO((STORE_S *, char *));


/*
 * GENERALIZED STORAGE FUNCTIONS.  Idea is to allow creation of
 * storage objects that can be written into and read from without
 * the caller knowing if the storage is core or in a file
 * or whatever.
 */
#define	MSIZE_INIT	8192
#define	MSIZE_INC	4096


/*
 * System specific options
 */
#if	defined(DOS) || defined(OS2)
#define CRLF_NEWLINES
#endif

/*
 * Various parms for opening and creating files directly and via stdio.
 * NOTE: If "binary" mode file exists, use it.
 */
#ifdef	O_BINARY
#define STDIO_READ	"rb"
#define STDIO_APPEND	"a+b"
#else
#define	O_BINARY	0
#define	STDIO_READ	"r"
#define	STDIO_APPEND	"a+"
#endif

#define	OP_FL_RDWR	(O_RDWR | O_CREAT | O_APPEND | O_BINARY)
#define	OP_FL_RDONLY	(O_RDONLY | O_BINARY)

#ifdef	S_IREAD
#define	OP_MD_USER	(S_IREAD | S_IWRITE)
#else
#define	OP_MD_USER	0600
#endif

#ifdef	S_IRUSR
#define	OP_MD_ALL	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | \
			 S_IROTH | S_IWOTH)
#else
#define	OP_MD_ALL	0666
#endif


/*
 * allocate resources associated with the specified type of
 * storage.  If requesting a named file object, open it for
 * appending, else just open a temp file.
 *
 * return the filled in storage object
 */
STORE_S *
so_get(source, name, rtype)
    SourceType  source;			/* requested storage type */
    char       *name;			/* file name 		  */
    int		rtype;			/* file access type	  */
{
    STORE_S *so = (STORE_S *)fs_get(sizeof(STORE_S));

    memset(so, 0, sizeof(STORE_S));
    so->flags |= rtype;

    if(name)					/* stash the name */
      so->name = cpystr(name);
#ifdef	DOS
    else if(source == TmpFileStar || source == FileStar){
	/*
	 * Coerce to TmpFileStar.  The MSC library's "tmpfile()"
	 * doesn't observe the "TMP" or "TEMP" environment vars and
	 * always wants to write "\".  This is problematic in shared,
	 * networked environments.
	 */
	source   = TmpFileStar;
	so->name = temp_nam(NULL, "pi");
    }
#else
    else if(source == TmpFileStar)		/* make one up! */
      so->name = temp_nam(NULL, "pine-tmp");
#endif

    so->src = source;
    if(so->src == FileStar || so->src == TmpFileStar){
	so->writec = so_file_writec;
	so->readc  = so_file_readc;
	so->puts   = so_file_puts;

	/*
	 * The reason for both FileStar and TmpFileStar types is
	 * that, named or unnamed, TmpFileStar's are unlinked
	 * when the object is given back to the system.  This is
	 * useful for keeping us from running out of file pointers as
	 * the pointer associated with the object can be temporarily
	 * returned to the system without destroying the object.
	 *
	 * The programmer is warned to be careful not to assign the
	 * TmpFileStar type to any files that are expected to remain
	 * after the dust has settled!
	 */
	if(so->name){
	    if(!(so->txt = so_file_open(so))){
		dprint(1, (debugfile, "so_get error: %s : %s",
		       so->name ? so->name : "?",
		       error_description(errno)));
		if(source == TmpFileStar)
		  (void)unlink(so->name);

		fs_give((void **)&so->name);
		fs_give((void **)&so); 		/* so freed & set to NULL */
	    }
	}
	else{
	    if(!(so->txt = (void *) create_tmpfile())){
		dprint(1, (debugfile, "so_get error: tmpfile : %s",
			   error_description(errno)));
		fs_give((void **)&so);		/* so freed & set to NULL */
	    }
	}
    }
    else if(so->src == PicoText){
	so->writec = so_pico_writec;
	so->readc  = so_pico_readc;
	so->puts   = so_pico_puts;
	if(!(so->txt = pico_get())){
	    dprint(1, (debugfile, "so_get error: alloc of pico text space"));
	    if(so->name)
	      fs_give((void **)&so->name);
	    fs_give((void **)&so);		/* so freed & set to NULL */
	}
    }
    else{
	so->writec = so_cs_writec;
	so->readc  = so_cs_readc;
	so->puts   = so_cs_puts;
	so->txt	   = (void *)fs_get((size_t) MSIZE_INIT * sizeof(char));
	so->dp	   = so->eod = (unsigned char *) so->txt;
	so->eot	   = so->dp + MSIZE_INIT;
	memset(so->eod, 0, so->eot - so->eod);
    }

    return(so);
}


/*
 * so_give - free resources associated with a storage object and then
 *           the object itself.
 */
int
so_give(so)
STORE_S **so;
{
    int ret = 0;

    if(!so)
      return(ret);

    if((*so)->src == FileStar || (*so)->src == TmpFileStar){
        if((*so)->txt)			/* disassociate from storage */
	  ret = fclose((FILE *)(*so)->txt) == EOF ? -1 : 0;

	if((*so)->name && (*so)->src == TmpFileStar)
	  (void)unlink((*so)->name);	/* really disassociate! */
    }
    else if((*so)->txt && (*so)->src == PicoText)
      pico_give((*so)->txt);
    else if((*so)->txt)
      fs_give((void **)&((*so)->txt));

    if((*so)->name)
      fs_give((void **)&((*so)->name));	/* blast the name            */

    fs_give((void **)so);		/* release the object        */

    return(ret);
}


/*
 * so_file_open
 */
void *
so_file_open(so)
    STORE_S *so;
{
    char *type  = ((so->flags) & WRITE_ACCESS) ? STDIO_APPEND : STDIO_READ;
    int   flags = ((so->flags) & WRITE_ACCESS) ? OP_FL_RDWR : OP_FL_RDONLY,
	  mode  = (((so->flags) & OWNER_ONLY) || so->src == TmpFileStar)
		   ? OP_MD_USER : OP_MD_ALL,
	  fd;

    /*
     * Use open instead of fopen so we can make temp files private.
     */
    return(((fd = open(so->name, flags, mode)) >= 0)
	     ? (so->txt = (void *) fdopen(fd, type)) : NULL);
}


/*
 * put a character into the specified storage object,
 * expanding if neccessary
 *
 * return 1 on success and 0 on failure
 */
int
so_cs_writec(c, so)
    int      c;
    STORE_S *so;
{
    if(so->dp >= so->eot){
	size_t incr;
	size_t cur_o  = so->dp - (unsigned char *) so->txt;
	size_t data_o = so->eod - (unsigned char *) so->txt;
	size_t size   = (so->eot - (unsigned char *) so->txt);

	/*
	 * We estimate the size we're going to need at the beginning
	 * so it shouldn't normally happen that we run out of space and
	 * come here. If we do come here, it is probably because there
	 * are lots of handles in the message or lots of quote coloring.
	 * In either case, the overhead of those is high so we don't want
	 * to keep adding little bits and resizing. Add 50% each time
	 * instead.
	 */
	incr = max(size/2, MSIZE_INC);
	size += incr;

	fs_resize(&so->txt, size * sizeof(char));
	so->dp   = (unsigned char *) so->txt + cur_o;
	so->eod  = (unsigned char *) so->txt + data_o;
	so->eot  = (unsigned char *) so->txt + size;
	memset(so->eod, 0, so->eot - so->eod);
    }

    *so->dp++ = (unsigned char) c;
    if(so->dp > so->eod)
      so->eod = so->dp;

    return(1);
}

int
so_pico_writec(c, so)
    int      c;
    STORE_S *so;
{
    unsigned char ch = (unsigned char) c;

    return(pico_writec(so->txt, ch));
}

int
so_file_writec(c, so)
    int      c;
    STORE_S *so;
{
    unsigned char ch = (unsigned char) c;
    int rv = 0;

    if(so->txt || so_reaquire(so))
      do
	rv = fwrite(&ch,sizeof(unsigned char),(size_t)1,(FILE *)so->txt);
      while(!rv && ferror((FILE *)so->txt) && errno == EINTR);

    return(rv);
}


/*
 * get a character from the specified storage object.
 *
 * return 1 on success and 0 on failure
 */
int
so_cs_readc(c, so)
    unsigned char *c;
    STORE_S       *so;
{
    return((so->dp < so->eod) ? *c = *(so->dp)++, 1 : 0);
}

int
so_pico_readc(c, so)
    unsigned char *c;
    STORE_S       *so;
{
    return(pico_readc(so->txt, c));
}

int
so_file_readc(c, so)
    unsigned char *c;
    STORE_S       *so;
{
    int rv = 0;

    if(so->txt || so_reaquire(so))
      do
	rv = fread(c, sizeof(char), (size_t)1, (FILE *)so->txt);
      while(!rv && ferror((FILE *)so->txt) && errno == EINTR);

    return(rv);
}


/*
 * write a string into the specified storage object,
 * expanding if necessary (and cheating if the object
 * happens to be a file!)
 *
 * return 1 on success and 0 on failure
 */
int
so_cs_puts(so, s)
    STORE_S *so;
    char    *s;
{
    int slen = strlen(s);

    if(so->dp + slen >= so->eot){
	register size_t cur_o  = so->dp - (unsigned char *) so->txt;
	register size_t data_o = so->eod - (unsigned char *) so->txt;
	register size_t len   = so->eot - (unsigned char *) so->txt;
	while(len <= cur_o + slen + 1){
	    size_t incr;

	    incr = max(len/2, MSIZE_INC);
	    len += incr;
	}

	fs_resize(&so->txt, len * sizeof(char));
	so->dp	 = (unsigned char *)so->txt + cur_o;
	so->eod	 = (unsigned char *)so->txt + data_o;
	so->eot	 = (unsigned char *)so->txt + len;
	memset(so->eod, 0, so->eot - so->eod);
    }

    memcpy(so->dp, s, slen);
    so->dp += slen;
    if(so->dp > so->eod)
      so->eod = so->dp;

    return(1);
}

int
so_pico_puts(so, s)
    STORE_S *so;
    char    *s;
{
    return(pico_puts(so->txt, s));
}

int
so_file_puts(so, s)
    STORE_S *so;
    char    *s;
{
    int rv = *s ? 0 : 1;

    if(!rv && (so->txt || so_reaquire(so)))
      do
	rv = fwrite(s, strlen(s)*sizeof(char), (size_t)1, (FILE *)so->txt);
      while(!rv && ferror((FILE *)so->txt) && errno == EINTR);

    return(rv);
}


/*
 *
 */
int
so_nputs(so, s, n)
    STORE_S *so;
    char    *s;
    long     n;
{
    while(n--)
      if(!so_writec((unsigned char) *s++, so))
	return(0);		/* ERROR putting char ! */

    return(1);
}



/*
 * Position the storage object's pointer to the given offset
 * from the start of the object's data.
 */
int
so_seek(so, pos, orig)
    STORE_S *so;
    long     pos;
    int      orig;
{
    if(so->src == CharStar){
	switch(orig){
	    case 0 :				/* SEEK_SET */
	      return((pos < so->eod - (unsigned char *) so->txt)
		      ? so->dp = (unsigned char *)so->txt + pos, 0 : -1);
	    case 1 :				/* SEEK_CUR */
	      return((pos > 0)
		       ? ((pos < so->eod - so->dp) ? so->dp += pos, 0: -1)
		       : ((pos < 0)
			   ? ((-pos < so->dp - (unsigned char *)so->txt)
			        ? so->dp += pos, 0 : -1)
			   : 0));
	    case 2 :				/* SEEK_END */
	      return((pos > 0)
		       ? -1
		       : ((-pos <= so->eod - (unsigned char *) so->txt)
			    ? so->dp = so->eod + pos, 0 : -1));
	    default :
	      return(-1);
	}
    }
    else if(so->src == PicoText)
      return(pico_seek(so->txt, pos, orig));
    else			/* FileStar or TmpFileStar */
      return((so->txt || so_reaquire(so))
		? fseek((FILE *)so->txt,pos,orig)
		: -1);
}


/*
 * Change the given storage object's size to that specified.  If size
 * is less than the current size, the internal pointer is adjusted and
 * all previous data beyond the given size is lost.
 *
 * Returns 0 on failure.
 */
int
so_truncate(so, size)
    STORE_S *so;
    long     size;
{
    if(so->src == CharStar){
	if(so->eod < (unsigned char *) so->txt + size){	/* alloc! */
	    unsigned char *newtxt = (unsigned char *) so->txt;
	    register size_t len   = so->eot - (unsigned char *) so->txt;

	    while(len <= size)
	      len += MSIZE_INC;		/* need to resize! */

	    if(len > so->eot - (unsigned char *) newtxt){
		fs_resize((void **) &newtxt, len * sizeof(char));
		so->eot = newtxt + len;
		so->eod = newtxt + (so->eod - (unsigned char *) so->txt);
		memset(so->eod, 0, so->eot - so->eod);
	    }

	    so->eod = newtxt + size;
	    so->dp  = newtxt + (so->dp - (unsigned char *) so->txt);
	    so->txt = newtxt;
	}
	else if(so->eod > (unsigned char *) so->txt + size){
	    if(so->dp > (so->eod = (unsigned char *)so->txt + size))
	      so->dp = so->eod;

	    memset(so->eod, 0, so->eot - so->eod);
	}

	return(1);
    }
    else if(so->src == PicoText){
	fatal("programmer botch: unsupported so_truncate call");
	/*NOTREACHED*/
	return(0); /* suppress dumb compiler warnings */
    }
    else			/* FileStar or TmpFileStar */
      return(fflush((FILE *) so->txt) != EOF
	     && fseek((FILE *) so->txt, size, 0) == 0
	     && ftruncate(fileno((FILE *)so->txt), size) == 0);
}


/*
 * Report given storage object's position indicator.
 * Returns 0 on failure.
 */
long
so_tell(so)
    STORE_S *so;
{
    if(so->src == CharStar){
	return((long) (so->dp - (unsigned char *) so->txt));
    }
    else if(so->src == PicoText){
	fatal("programmer botch: unsupported so_truncate call");
	/*NOTREACHED*/
	return(0); /* suppress dumb compiler warnings */
    }
    else			/* FileStar or TmpFileStar */
      return(ftell((FILE *) so->txt));
}


/*
 * so_release - a rather misnamed function.  the idea is to release
 *              what system resources we can (e.g., open files).
 *              while maintaining a reference to it.
 *              it's up to the functions that deal with this object
 *              next to re-aquire those resources.
 */
int
so_release(so)
STORE_S *so;
{
    if(so->txt && so->name && (so->src == FileStar || so->src == TmpFileStar)){
	if(fget_pos((FILE *)so->txt, (fpos_t *)&(so->used)) == 0){
	    fclose((FILE *)so->txt);		/* free the handle! */
	    so->txt = NULL;
	}
    }

    return(1);
}


/*
 * so_reaquire - get any previously released system resources we
 *               may need for the given storage object.
 *       NOTE: at the moment, only FILE * types of objects are
 *             effected, so it only needs to be called before
 *             references to them.
 *
 */
so_reaquire(so)
STORE_S *so;
{
    int   rv = 1;

    if(!so->txt && (so->src == FileStar || so->src == TmpFileStar)){
	if(!(so->txt = so_file_open(so))){
	    q_status_message2(SM_ORDER,3,5, "ERROR reopening %.200s : %.200s",
			      so->name, error_description(errno));
	    rv = 0;
	}
	else if(fset_pos((FILE *)so->txt, (fpos_t *)&(so->used))){
	    q_status_message2(SM_ORDER, 3, 5,
			      "ERROR positioning in %.200s : %.200s",
			      so->name, error_description(errno));
	    rv = 0;
	}
    }

    return(rv);
}


/*
 * so_text - return a pointer to the text the store object passed
 */
void *
so_text(so)
STORE_S *so;
{
    return((so) ? so->txt : NULL);
}


/*
 * END OF GENERALIZE STORAGE FUNCTIONS
 */


/*
 * Start of filters, pipes and various support functions
 */

/*
 * pointer to first function in a pipe, and pointer to last filter
 */
FILTER_S         *gf_master = NULL;
static	gf_io_t   last_filter;
static	char     *gf_error_string;
static	long	  gf_byte_count;
static	jmp_buf   gf_error_state;


/*
 * A list of states used by the various filters.  Reused in many filters.
 */
#define	DFL	0
#define	EQUAL	1
#define	HEX	2
#define	WSPACE	3
#define	CCR	4
#define	CLF	5
#define	TOKEN	6
#define	TAG	7
#define	HANDLE	8
#define	HDATA	9
#define	ESC	10
#define	ESCDOL	11
#define	ESCPAR	12
#define	EUC	13
#define	BOL	14
#define	FL_QLEV	15
#define	FL_STF	16
#define	FL_SIG	17
#define	STOP_DECODING	18
#define	SPACECR	19



/*
 * Macros to reduce function call overhead associated with calling
 * each filter for each byte filtered, and to minimize filter structure
 * dereferences.  NOTE: "queuein" has to do with putting chars into the
 * filter structs data queue.  So, writing at the queuein offset is
 * what a filter does to pass processed data out of itself.  Ditto for
 * queueout.  This explains the FI --> queueout init stuff below.
 */
#define	GF_QUE_START(F)	(&(F)->queue[0])
#define	GF_QUE_END(F)	(&(F)->queue[GF_MAXBUF - 1])

#define	GF_IP_INIT(F)	ip  = (F) ? &(F)->queue[(F)->queuein] : NULL
#define	GF_IP_INIT_GLO(F)  (*ipp)  = (F) ? &(F)->queue[(F)->queuein] : NULL
#define	GF_EIB_INIT(F)	eib = (F) ? GF_QUE_END(F) : NULL
#define	GF_EIB_INIT_GLO(F)  (*eibp) = (F) ? GF_QUE_END(F) : NULL
#define	GF_OP_INIT(F)	op  = (F) ? &(F)->queue[(F)->queueout] : NULL
#define	GF_EOB_INIT(F)	eob = (F) ? &(F)->queue[(F)->queuein] : NULL

#define	GF_IP_END(F)	(F)->queuein  = ip - GF_QUE_START(F)
#define	GF_IP_END_GLO(F)  (F)->queuein  = (unsigned char *)(*ipp) - (unsigned char *)GF_QUE_START(F)
#define	GF_OP_END(F)	(F)->queueout = op - GF_QUE_START(F)

#define	GF_INIT(FI, FO)	unsigned char *GF_OP_INIT(FI);	 \
			unsigned char *GF_EOB_INIT(FI); \
			unsigned char *GF_IP_INIT(FO);  \
			unsigned char *GF_EIB_INIT(FO);

#define	GF_CH_RESET(F)	(op = eob = GF_QUE_START(F), \
					    (F)->queueout = (F)->queuein = 0)

#define	GF_END(FI, FO)	(GF_OP_END(FI), GF_IP_END(FO))

#define	GF_FLUSH(F)	((int)(GF_IP_END(F), (*(F)->f)((F), GF_DATA), \
			       GF_IP_INIT(F), GF_EIB_INIT(F)))
#define	GF_FLUSH_GLO(F)	((int)(GF_IP_END_GLO(F), (*(F)->f)((F), GF_DATA), \
			       GF_IP_INIT_GLO(F), GF_EIB_INIT_GLO(F)))

#define	GF_PUTC(F, C)	((int)(*ip++ = (C), (ip >= eib) ? GF_FLUSH(F) : 1))
#define	GF_PUTC_GLO(F, C) ((int)(*(*ipp)++ = (C), ((*ipp) >= (*eibp)) ? GF_FLUSH_GLO(F) : 1))
/*
 * Introducing the *_GLO macros for use in splitting the big macros out
 * into functions (wrap_flush, wrap_eol).  The reason we need a
 * separate macro is because of the vars ip, eib, op, and eob, which are
 * set up locally in a call to GF_INIT.  To preserve these variables
 * in the new functions, we now pass pointers to these four vars.  Each
 * of these new functions expects the presence of pointer vars
 * ipp, eibp, opp, and eobp.  
 */

#define	GF_GETC(F, C)	((op < eob) ? (((C) = *op++), 1) : GF_CH_RESET(F))

#define GF_COLOR_PUTC(F, C) {                                            \
                              char *p;                                   \
                              GF_PUTC_GLO((F)->next, TAG_EMBED);         \
                              GF_PUTC_GLO((F)->next, TAG_FGCOLOR);       \
                              p = color_to_asciirgb((C)->fg);            \
                              for(; *p; p++)                             \
                                GF_PUTC_GLO((F)->next, *p);              \
                              GF_PUTC_GLO((F)->next, TAG_EMBED);         \
                              GF_PUTC_GLO((F)->next, TAG_BGCOLOR);       \
                              p = color_to_asciirgb((C)->bg);            \
                              for(; *p; p++)                             \
                                GF_PUTC_GLO((F)->next, *p);              \
                            }

/*
 * Generalized getc and putc routines.  provided here so they don't
 * need to be re-done elsewhere to
 */

/*
 * pointers to objects to be used by the generic getc and putc
 * functions
 */
static struct gf_io_struct {
    FILE          *file;
    PIPE_S        *pipe;
    char          *txtp;
    unsigned long  n;
} gf_in, gf_out;

#define	GF_SO_STACK	struct gf_so_stack
static GF_SO_STACK {
    STORE_S	*so;
    GF_SO_STACK *next;
} *gf_so_in, *gf_so_out;



/*
 * Returns 1 if pc will write into a PicoText object, 0 otherwise.
 *
 * The purpose of this routine is so that we can avoid setting SIGALARM
 * when writing into a PicoText object, because that type of object uses
 * unprotected malloc/free/realloc, which can't be interrupted.
 */
int
pc_is_picotext(pc)
    gf_io_t pc;
{
    return(pc == gf_so_writec && gf_so_out && gf_so_out->so &&
	   gf_so_out->so->src == PicoText);
}



/*
 * setup to use and return a pointer to the generic
 * getc function
 */
void
gf_set_readc(gc, txt, len, src)
    gf_io_t       *gc;
    void          *txt;
    unsigned long  len;
    SourceType     src;
{
    gf_in.n = len;
    if(src == FileStar){
	gf_in.file = (FILE *)txt;
	fseek(gf_in.file, 0L, 0);
	*gc = gf_freadc;
    }
    else if(src == PipeStar){
	gf_in.pipe = (PIPE_S *)txt;
	*gc = gf_preadc;
    }
    else{
	gf_in.txtp = (char *)txt;
	*gc = gf_sreadc;
    }
}


/*
 * setup to use and return a pointer to the generic
 * putc function
 */
void
gf_set_writec(pc, txt, len, src)
    gf_io_t       *pc;
    void          *txt;
    unsigned long  len;
    SourceType     src;
{
    gf_out.n = len;
    if(src == FileStar){
	gf_out.file = (FILE *)txt;
	*pc = gf_fwritec;
    }
    else if(src == PipeStar){
	gf_out.pipe = (PIPE_S *)txt;
	*pc = gf_pwritec;
    }
    else{
	gf_out.txtp = (char *)txt;
	*pc = gf_swritec;
    }
}


/*
 * setup to use and return a pointer to the generic
 * getc function
 */
void
gf_set_so_readc(gc, so)
    gf_io_t *gc;
    STORE_S *so;
{
    GF_SO_STACK *sp = (GF_SO_STACK *) fs_get(sizeof(GF_SO_STACK));

    sp->so   = so;
    sp->next = gf_so_in;
    gf_so_in = sp;
    *gc      = gf_so_readc;
}


void
gf_clear_so_readc(so)
    STORE_S *so;
{
    GF_SO_STACK *sp;

    if(sp = gf_so_in){
	if(so == sp->so){
	    gf_so_in = gf_so_in->next;
	    fs_give((void **) &sp);
	}
	else
	  panic("Programmer botch: Can't unstack store readc");
    }
    else
      panic("Programmer botch: NULL store clearing store readc");
}


/*
 * setup to use and return a pointer to the generic
 * putc function
 */
void
gf_set_so_writec(pc, so)
    gf_io_t *pc;
    STORE_S *so;
{
    GF_SO_STACK *sp = (GF_SO_STACK *) fs_get(sizeof(GF_SO_STACK));

    sp->so    = so;
    sp->next  = gf_so_out;
    gf_so_out = sp;
    *pc       = gf_so_writec;
}


void
gf_clear_so_writec(so)
    STORE_S *so;
{
    GF_SO_STACK *sp;

    if(sp = gf_so_out){
	if(so == sp->so){
	    gf_so_out = gf_so_out->next;
	    fs_give((void **) &sp);
	}
	else
	  panic("Programmer botch: Can't unstack store writec");
    }
    else
      panic("Programmer botch: NULL store clearing store writec");
}


/*
 * put the character to the object previously defined
 */
int
gf_so_writec(c)
int c;
{
    return(so_writec(c, gf_so_out->so));
}


/*
 * get a character from an object previously defined
 */
int
gf_so_readc(c)
unsigned char *c;
{
    return(so_readc(c, gf_so_in->so));
}


/* get a character from a file */
/* assumes gf_out struct is filled in */
int
gf_freadc(c)
unsigned char *c;
{
    int rv = 0;

    do {
	errno = 0;
	clearerr(gf_in.file);
	rv = fread(c, sizeof(unsigned char), (size_t)1, gf_in.file);
    } while(!rv && ferror(gf_in.file) && errno == EINTR);

    return(rv);
}


/* put a character to a file */
/* assumes gf_out struct is filled in */
int
gf_fwritec(c)
    int c;
{
    unsigned char ch = (unsigned char)c;
    int rv = 0;

    do
      rv = fwrite(&ch, sizeof(unsigned char), (size_t)1, gf_out.file);
    while(!rv && ferror(gf_out.file) && errno == EINTR);

    return(rv);
}

int
gf_preadc(c)
unsigned char *c;
{
    return(pipe_readc(c, gf_in.pipe));
}

int
gf_pwritec(c)
    int c;
{
    return(pipe_writec(c, gf_out.pipe));
}

/* get a character from a string, return nonzero if things OK */
/* assumes gf_out struct is filled in */
int
gf_sreadc(c)
unsigned char *c;
{
    return((gf_in.n) ? *c = *(gf_in.txtp)++, gf_in.n-- : 0);
}


/* put a character into a string, return nonzero if things OK */
/* assumes gf_out struct is filled in */
int
gf_swritec(c)
    int c;
{
    return((gf_out.n) ? *(gf_out.txtp)++ = c, gf_out.n-- : 0);
}


/*
 * output the given string with the given function
 */
int
gf_puts(s, pc)
    register char *s;
    gf_io_t        pc;
{
    while(*s != '\0')
      if(!(*pc)((unsigned char)*s++))
	return(0);		/* ERROR putting char ! */

    return(1);
}


/*
 * output the given string with the given function
 */
int
gf_nputs(s, n, pc)
    register char *s;
    long	   n;
    gf_io_t        pc;
{
    while(n--)
      if(!(*pc)((unsigned char)*s++))
	return(0);		/* ERROR putting char ! */

    return(1);
}


/*
 * Start of generalized filter routines
 */

/*
 * initializing function to make sure list of filters is empty.
 */
void
gf_filter_init()
{
    FILTER_S *flt, *fltn = gf_master;

    while((flt = fltn) != NULL){	/* free list of old filters */
	fltn = flt->next;
	fs_give((void **)&flt);
    }

    gf_master = NULL;
    gf_error_string = NULL;		/* clear previous errors */
    gf_byte_count = 0L;			/* reset counter */
}



/*
 * link the given filter into the filter chain
 */
void
gf_link_filter(f, data)
    filter_t  f;
    void     *data;
{
    FILTER_S *new, *tail;

#ifdef	CRLF_NEWLINES
    /*
     * If the system's native EOL convention is CRLF, then there's no
     * point in passing data thru a filter that's not doing anything
     */
    if(f == gf_nvtnl_local || f == gf_local_nvtnl)
      return;
#endif

    new = (FILTER_S *)fs_get(sizeof(FILTER_S));
    memset(new, 0, sizeof(FILTER_S));

    new->f = f;				/* set the function pointer     */
    new->opt = data;			/* set any optional parameter data */
    (*f)(new, GF_RESET);		/* have it setup initial state  */

    if(tail = gf_master){		/* or add it to end of existing  */
	while(tail->next)		/* list  */
	  tail = tail->next;

	tail->next = new;
    }
    else				/* attach new struct to list    */
      gf_master = new;			/* start a new list */
}


/*
 * terminal filter, doesn't call any other filters, typically just does
 * something with the output
 */
void
gf_terminal(f, flg)
    FILTER_S *f;
    int       flg;
{
    if(flg == GF_DATA){
	GF_INIT(f, f);

	while(op < eob)
	  if((*last_filter)(*op++) <= 0) /* generic terminal filter */
	    gf_error(errno ? error_description(errno) : "Error writing pipe");

	GF_CH_RESET(f);
    }
    else if(flg == GF_RESET)
      errno = 0;			/* prepare for problems */
}


/*
 * set some outside gf_io_t function to the terminal function
 * for example: a function to write a char to a file or into a buffer
 */
void
gf_set_terminal(f)			/* function to set generic filter */
    gf_io_t f;
{
    last_filter = f;
}


/*
 * common function for filter's to make it known that an error
 * has occurred.  Jumps back to gf_pipe with error message.
 */
void
gf_error(s)
    char *s;
{
    /* let the user know the error passed in s */
    gf_error_string = s;
    longjmp(gf_error_state, 1);
}


/*
 * The routine that shoves each byte through the chain of
 * filters.  It sets up error handling, and the terminal function.
 * Then loops getting bytes with the given function, and passing
 * it on to the first filter in the chain.
 */
char *
gf_pipe(gc, pc)
    gf_io_t gc, pc;			/* how to get a character */
{
    unsigned char c;

#if	defined(DOS) && !defined(_WINDOWS)
    MoveCursor(0, 1);
    StartInverse();
#endif

    dprint(4, (debugfile, "-- gf_pipe: "));

    /*
     * set up for any errors a filter may encounter
     */
    if(setjmp(gf_error_state)){
#if	defined(DOS) && !defined(_WINDOWS)
	ibmputc(' ');
	EndInverse();
#endif
	dprint(4, (debugfile, "ERROR: %s\n",
		   gf_error_string ? gf_error_string : "NULL"));
	return(gf_error_string); 	/*  */
    }

    /*
     * set and link in the terminal filter
     */
    gf_set_terminal(pc);
    gf_link_filter(gf_terminal, NULL);

    /*
     * while there are chars to process, send them thru the pipe.
     * NOTE: it's necessary to enclose the loop below in a block
     * as the GF_INIT macro calls some automatic var's into
     * existence.  It can't be placed at the start of gf_pipe
     * because its useful for us to be called without filters loaded
     * when we're just being used to copy bytes between storage
     * objects.
     */
    {
	GF_INIT(gf_master, gf_master);

	while((*gc)(&c)){
	    gf_byte_count++;
#ifdef	DOS
	    if(!(gf_byte_count & 0x3ff))
#ifdef	_WINDOWS
	      /* Under windows we yeild to allow event processing.
	       * Progress display is handled throught the alarm()
	       * mechinism.
	       */
	      mswin_yeild ();
#else
	      /* Poor PC still needs spinning bar */
	      ibmputc("/-\\|"[((int) gf_byte_count >> 10) % 4]);
	      MoveCursor(0, 1);
#endif
#endif

	    GF_PUTC(gf_master, c & 0xff);
	}

	/*
	 * toss an end-of-data marker down the pipe to give filters
	 * that have any buffered data the opportunity to dump it
	 */
	GF_FLUSH(gf_master);
	(*gf_master->f)(gf_master, GF_EOD);
    }

#if	defined(DOS) && !defined(_WINDOWS)
    ibmputc(' ');
    EndInverse();
#endif

    dprint(4, (debugfile, "done.\n"));
    return(NULL);			/* everything went OK */
}


/*
 * return the number of bytes piped so far
 */
long
gf_bytes_piped()
{
    return(gf_byte_count);
}


/*
 * filter the given input with the given command
 *
 *  Args: cmd -- command string to execute
 *	prepend -- string to prepend to filtered input
 *	source_so -- storage object containing data to be filtered
 *	pc -- function to write filtered output with
 *	aux_filters -- additional filters to pass data thru after "cmd"
 *
 *  Returns: NULL on sucess, reason for failure (not alloc'd!) on error
 */
char *
gf_filter(cmd, prepend, source_so, pc, aux_filters, disable_reset)
    char       *cmd, *prepend;
    STORE_S    *source_so;
    gf_io_t	pc;
    FILTLIST_S *aux_filters;
    int         disable_reset;
{
    unsigned char c;
    int	     flags;
    char   *errstr = NULL, buf[MAILTMPLEN];
    PIPE_S *fpipe;

    dprint(4, (debugfile, "so_filter: \"%s\"\n", cmd ? cmd : "?"));

    gf_filter_init();
    for( ; aux_filters && aux_filters->filter; aux_filters++)
      gf_link_filter(aux_filters->filter, aux_filters->data);

    gf_set_terminal(pc);
    gf_link_filter(gf_terminal, NULL);

    /*
     * Spawn filter feeding it data, and reading what it writes.
     */
    so_seek(source_so, 0L, 0);
    flags = PIPE_WRITE | PIPE_READ | PIPE_NOSHELL |
			    (!disable_reset ? PIPE_RESET : 0);

    if(fpipe = open_system_pipe(cmd, NULL, NULL, flags, 0)){
#ifdef	NON_BLOCKING_IO
	int     n;

	if(fcntl(fileno(fpipe->in.f), F_SETFL, NON_BLOCKING_IO) == -1)
	  errstr = "Can't set up non-blocking IO";

	if(prepend && (fputs(prepend, fpipe->out.f) == EOF
		       || fputc('\n', fpipe->out.f) == EOF))
	  errstr = error_description(errno);

	while(!errstr){
	    /* if the pipe can't hold a K we're sunk (too bad PIPE_MAX
	     * isn't ubiquitous ;).
	     */
	    for(n = 0; !errstr && fpipe->out.f && n < 1024; n++)
	      if(!so_readc(&c, source_so)){
		  fclose(fpipe->out.f);
		  fpipe->out.f = NULL;
	      }
	      else if(fputc(c, fpipe->out.f) == EOF)
		errstr = error_description(errno);

	    /*
	     * Note: We clear errno here and test below, before ferror,
	     *	     because *some* stdio implementations consider
	     *	     EAGAIN and EWOULDBLOCK equivalent to EOF...
	     */
	    errno = 0;
	    clearerr(fpipe->in.f); /* fix from <cananian@cananian.mit.edu> */

	    while(!errstr && fgets(buf, sizeof(buf), fpipe->in.f))
	      errstr = gf_filter_puts(buf);

	    /* then fgets failed! */
	    if(!errstr && !(errno == EAGAIN || errno == EWOULDBLOCK)){
		if(feof(fpipe->in.f))		/* nothing else interesting! */
		  break;
		else if(ferror(fpipe->in.f))	/* bummer. */
		  errstr = error_description(errno);
	    }
	    else if(errno == EAGAIN || errno == EWOULDBLOCK)
	      clearerr(fpipe->in.f);
	}
#else
	if(prepend && (pipe_puts(prepend, fpipe) == EOF
		       || pipe_putc('\n', fpipe) == EOF))
	  errstr = error_description(errno);

	/*
	 * Well, do the best we can, and hope the pipe we're writing
	 * doesn't fill up before we start reading...
	 */
	while(!errstr && so_readc(&c, source_so))
	  if(pipe_putc(c, fpipe) == EOF)
	    errstr = error_description(errno);

	if(pipe_close_write(fpipe))
	  errstr = "Pipe command returned error.";
	while(!errstr && pipe_gets(buf, sizeof(buf), fpipe))
	  errstr = gf_filter_puts(buf);
#endif /* NON_BLOCKING */

	if(close_system_pipe(&fpipe, NULL, 0) && !errstr)
	  errstr = "Pipe command returned error.";

	gf_filter_eod();
    }
    else
      errstr = "Error setting up pipe command.";

    return(errstr);
}


/*
 * gf_filter_puts - write the given string down the filter's pipe
 */
char *
gf_filter_puts(s)
    register char *s;
{
    GF_INIT(gf_master, gf_master);

    /*
     * set up for any errors a filter may encounter
     */
    if(setjmp(gf_error_state)){
	dprint(4, (debugfile, "ERROR: gf_filter_puts: %s\n",
		   gf_error_string ? gf_error_string : "NULL"));
	return(gf_error_string);
    }

    while(*s)
      GF_PUTC(gf_master, (*s++) & 0xff);

    GF_END(gf_master, gf_master);
    return(NULL);
}


/*
 * gf_filter_eod - flush pending data filter's input queue and deliver
 *		   the GF_EOD marker.
 */
void
gf_filter_eod()
{
    GF_INIT(gf_master, gf_master);
    GF_FLUSH(gf_master);
    (*gf_master->f)(gf_master, GF_EOD);
}




/*
 * END OF PIPE SUPPORT ROUTINES, BEGINNING OF FILTERS
 *
 * Filters MUST use the specified interface (pointer to filter
 * structure, the unsigned character buffer in that struct, and a
 * cmd flag), and pass each resulting octet to the next filter in the
 * chain.  Only the terminal filter need not call another filter.
 * As a result, filters share a pretty general structure.
 * Typically three main conditionals separate initialization from
 * data from end-of-data command processing.
 *
 * Lastly, being character-at-a-time, they're a little more complex
 * to write than filters operating on buffers because some state
 * must typically be kept between characters.  However, for a
 * little bit of complexity here, much convenience is gained later
 * as they can be arbitrarily chained together at run time and
 * consume few resources (especially memory or disk) as they work.
 * (NOTE 951005: even less cpu now that data between filters is passed
 *  via a vector.)
 *
 * A few notes about implementing filters:
 *
 *  - A generic filter template looks like:
 *
 *    void
 *    gf_xxx_filter(f, flg)
 *        FILTER_S *f;
 *        int       flg;
 *    {
 *	  GF_INIT(f, f->next);		// def's var's to speed queue drain
 *
 *        if(flg == GF_DATA){
 *	      register unsigned char c;
 *
 *	      while(GF_GETC(f, c)){	// macro taking data off input queue
 *	          // operate on c and pass it on here
 *                GF_PUTC(f->next, c);	// macro writing output queue
 *	      }
 *
 *	      GF_END(f, f->next);	// macro to sync pointers/offsets
 *	      //WARNING: DO NOT RETURN BEFORE ALL INCOMING DATA'S PROCESSED
 *        }
 *        else if(flg == GF_EOD){
 *            // process any buffered data here and pass it on
 *	      GF_FLUSH(f->next);	// flush pending data to next filter
 *            (*f->next->f)(f->next, GF_EOD);
 *        }
 *        else if(flg == GF_RESET){
 *            // initialize any data in the struct here
 *        }
 *    }
 *
 *  - Any free storage allocated during initialization (typically tied
 *    to the "line" pointer in FILTER_S) is the filter's responsibility
 *    to clean up when the GF_EOD command comes through.
 *
 *  - Filter's must pass GF_EOD they receive on to the next
 *    filter in the chain so it has the opportunity to flush
 *    any buffered data.
 *
 *  - All filters expect NVT end-of-lines.  The idea is to prepend
 *    or append either the gf_local_nvtnl or gf_nvtnl_local
 *    os-dependant filters to the data on the appropriate end of the
 *    pipe for the task at hand.
 *
 *  - NOTE: As of 951004, filters no longer take their input as a single
 *    char argument, but rather get data to operate on via a vector
 *    representing the input queue in the FILTER_S structure.
 *
 */



/*
 * BASE64 TO BINARY encoding and decoding routines below
 */


/*
 * BINARY to BASE64 filter (encoding described in rfc1341)
 */
void
gf_binary_b64(f, flg)
    FILTER_S *f;
    int       flg;
{
    static char *v =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register unsigned char t = f->t;
	register long n = f->n;

	while(GF_GETC(f, c)){

	    switch(n++){
	      case 0 : case 3 : case 6 : case 9 : case 12: case 15: case 18:
	      case 21: case 24: case 27: case 30: case 33: case 36: case 39:
	      case 42: case 45:
		GF_PUTC(f->next, v[c >> 2]);
					/* byte 1: high 6 bits (1) */
		t = c << 4;		/* remember high 2 bits for next */
		break;

	      case 1 : case 4 : case 7 : case 10: case 13: case 16: case 19:
	      case 22: case 25: case 28: case 31: case 34: case 37: case 40:
	      case 43:
		GF_PUTC(f->next, v[(t|(c>>4)) & 0x3f]);
		t = c << 2;
		break;

	      case 2 : case 5 : case 8 : case 11: case 14: case 17: case 20:
	      case 23: case 26: case 29: case 32: case 35: case 38: case 41:
	      case 44:
		GF_PUTC(f->next, v[(t|(c >> 6)) & 0x3f]);
		GF_PUTC(f->next, v[c & 0x3f]);
		break;
	    }

	    if(n == 45){			/* start a new line? */
		GF_PUTC(f->next, '\015');
		GF_PUTC(f->next, '\012');
		n = 0L;
	    }
	}

	f->n = n;
	f->t = t;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){		/* no more data */
	switch (f->n % 3) {		/* handle trailing bytes */
	  case 0:			/* no trailing bytes */
	    break;

	  case 1:
	    GF_PUTC(f->next, v[(f->t) & 0x3f]);
	    GF_PUTC(f->next, '=');	/* byte 3 */
	    GF_PUTC(f->next, '=');	/* byte 4 */
	    break;

	  case 2:
	    GF_PUTC(f->next, v[(f->t) & 0x3f]);
	    GF_PUTC(f->next, '=');	/* byte 4 */
	    break;
	}

	/* end with CRLF */
	if(f->n){
	    GF_PUTC(f->next, '\015');
	    GF_PUTC(f->next, '\012');
	}

	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset binary_b64\n"));
	f->n = 0L;
    }
}



/*
 * BASE64 to BINARY filter (encoding described in rfc1341)
 */
void
gf_b64_binary(f, flg)
    FILTER_S *f;
    int       flg;
{
    static char v[] = {65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,
		       65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,
		       65,65,65,65,65,65,65,65,65,65,65,62,65,65,65,63,
		       52,53,54,55,56,57,58,59,60,61,65,65,65,64,65,65,
		       65, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
		       15,16,17,18,19,20,21,22,23,24,25,65,65,65,65,65,
		       65,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
		       41,42,43,44,45,46,47,48,49,50,51,65,65,65,65,65};
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register unsigned char t = f->t;
	register int n = (int) f->n;
	register int state = f->f1;

	while(GF_GETC(f, c)){

	    if(state){
		state = 0;
		if (c != '=') {
		    gf_error("Illegal '=' in base64 text");
		    /* NO RETURN */
		}
	    }

	    /* in range, and a valid value? */
	    if((c & ~0x7f) || (c = v[c]) > 63){
		if(c == 64){
		    switch (n++) {	/* check quantum position */
		      case 2:
			state++;	/* expect an equal as next char */
			break;

		      case 3:
			n = 0L;		/* restart quantum */
			break;

		      default:		/* impossible quantum position */
			gf_error("Internal base64 decoder error");
			/* NO RETURN */
		    }
		}
	    }
	    else{
		switch (n++) {		/* install based on quantum position */
		  case 0:		/* byte 1: high 6 bits */
		    t = c << 2;
		    break;

		  case 1:		/* byte 1: low 2 bits */
		    GF_PUTC(f->next, (t|(c >> 4)));
		    t = c << 4;		/* byte 2: high 4 bits */
		    break;

		  case 2:		/* byte 2: low 4 bits */
		    GF_PUTC(f->next, (t|(c >> 2)));
		    t = c << 6;		/* byte 3: high 2 bits */
		    break;

		  case 3:
		    GF_PUTC(f->next, t | c);
		    n = 0L;		/* reinitialize mechanism */
		    break;
		}
	    }
	}

	f->f1 = state;
	f->t = t;
	f->n = n;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset b64_binary\n"));
	f->n  = 0L;			/* quantum position */
	f->f1 = 0;			/* state holder: equal seen? */
    }
}




/*
 * QUOTED-PRINTABLE ENCODING AND DECODING filters below.
 * encoding described in rfc1341
 */

#define	GF_MAXLINE	80		/* good buffer size */

/*
 * default action for QUOTED-PRINTABLE to 8BIT decoder
 */
#define	GF_QP_DEFAULT(f, c)	{ \
				    if((c) == ' '){ \
					state = WSPACE; \
						/* reset white space! */ \
					(f)->linep = (f)->line; \
					*((f)->linep)++ = ' '; \
				    } \
				    else if((c) == '='){ \
					state = EQUAL; \
				    } \
				    else \
				      GF_PUTC((f)->next, (c)); \
				}


/*
 * QUOTED-PRINTABLE to 8BIT filter
 */
void
gf_qp_8bit(f, flg)
    FILTER_S *f;
    int       flg;
{

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;

	while(GF_GETC(f, c)){

	    switch(state){
	      case DFL :		/* default case */
	      default:
		GF_QP_DEFAULT(f, c);
		break;

	      case CCR    :		/* non-significant space */
		state = DFL;
		if(c == '\012')
		  continue;		/* go on to next char */

		GF_QP_DEFAULT(f, c);
		break;

	      case EQUAL  :
		if(c == '\015'){	/* "=\015" is a soft EOL */
		    state = CCR;
		    break;
		}

		if(c == '='){		/* compatibility clause for old guys */
		    GF_PUTC(f->next, '=');
		    state = DFL;
		    break;
		}

		if(!isxdigit((unsigned char)c)){	/* must be hex! */
		    /*
		     * First character after '=' not a hex digit.
		     * This ain't right, but we're going to treat it as
		     * plain old text instead of an '=' followed by hex.
		     * In other words, they forgot to encode the '='.
		     * Before 4.60 we just bailed with an error here, but now
		     * we keep going as long as we are just displaying
		     * the result (and not saving it or something).
		     *
		     * Wait! The users don't like that. They want to be able
		     * to use it even if it might be wrong. So just plow
		     * ahead even if displaying.
		     *
		     * Better have this be a constant string so that if we
		     * get multiple instances of it in a single message we
		     * can avoid the too many error messages problem. It
		     * better be the same message as the one a few lines
		     * below, as well.
		     *
		     * Turn off decoding after encountering such an error and
		     * just dump the rest of the text as is.
		     */
		    state = STOP_DECODING;
		    GF_PUTC(f->next, '=');
		    GF_PUTC(f->next, c);
		    q_status_message(SM_ORDER,3,3,
			"Warning: Non-hexadecimal character in QP encoding!");

		    dprint(2, (debugfile, "gf_qp_8bit: warning: non-hex char in QP encoding: char \"%c\" (%d) follows =\n", c, c));
		    break;
		}

		if (isdigit ((unsigned char)c))
		  f->t = c - '0';
		else
		  f->t = c - (isupper((unsigned char)c) ? 'A' - 10 : 'a' - 10);
		
		f->f2 = c;	/* store character in case we have to
				   back out in !isxdigit below */

		state = HEX;
		break;

	      case HEX :
		state = DFL;
		if(!isxdigit((unsigned char)c)){	/* must be hex! */
		    state = STOP_DECODING;
		    GF_PUTC(f->next, '=');
		    GF_PUTC(f->next, f->f2);
		    GF_PUTC(f->next, c);
		    q_status_message(SM_ORDER,3,3,
			"Warning: Non-hexadecimal character in QP encoding!");

		    dprint(2, (debugfile, "gf_qp_8bit: warning: non-hex char in QP encoding: char \"%c\" (%d) follows =%c\n", c, c, f->f2));
		    break;
		}

		if (isdigit((unsigned char)c))
		  c -= '0';
		else
		  c -= (isupper((unsigned char)c) ? 'A' - 10 : 'a' - 10);

		GF_PUTC(f->next, c + (f->t << 4));
		break;

	      case WSPACE :
		if(c == ' '){		/* toss it in with other spaces */
		    if(f->linep - f->line < GF_MAXLINE)
		      *(f->linep)++ = ' ';
		    break;
		}

		state = DFL;
		if(c == '\015'){	/* not our white space! */
		    f->linep = f->line;	/* reset buffer */
		    GF_PUTC(f->next, '\015');
		    break;
		}

		/* the spaces are ours, write 'em */
		f->n = f->linep - f->line;
		while((f->n)--)
		  GF_PUTC(f->next, ' ');

		GF_QP_DEFAULT(f, c);	/* take care of 'c' in default way */
		break;

	      case STOP_DECODING :
		GF_PUTC(f->next, c);
		break;
	    }
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	fs_give((void **)&(f->line));
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset qp_8bit\n"));
	f->f1 = DFL;
	f->linep = f->line = (char *)fs_get(GF_MAXLINE * sizeof(char));
    }
}



/*
 * USEFUL MACROS TO HELP WITH QP ENCODING
 */

#define	QP_MAXL	75			/* 76th place only for continuation */

/*
 * Macro to test and wrap long quoted printable lines
 */
#define	GF_8BIT_WRAP(f)		{ \
				    GF_PUTC((f)->next, '='); \
				    GF_PUTC((f)->next, '\015'); \
				    GF_PUTC((f)->next, '\012'); \
				}

/*
 * write a quoted octet in QUOTED-PRINTABLE encoding, adding soft
 * line break if needed.
 */
#define	GF_8BIT_PUT_QUOTE(f, c)	{ \
				    if(((f)->n += 3) > QP_MAXL){ \
					GF_8BIT_WRAP(f); \
					(f)->n = 3;	/* set line count */ \
				    } \
				    GF_PUTC((f)->next, '='); \
				     GF_PUTC((f)->next, HEX_CHAR1(c)); \
				     GF_PUTC((f)->next, HEX_CHAR2(c)); \
				 }

/*
 * just write an ordinary octet in QUOTED-PRINTABLE, wrapping line
 * if needed.
 */
#define	GF_8BIT_PUT(f, c)	{ \
				      if((++(f->n)) > QP_MAXL){ \
					  GF_8BIT_WRAP(f); \
					  f->n = 1L; \
				      } \
				      if(f->n == 1L && c == '.'){ \
					  GF_8BIT_PUT_QUOTE(f, c); \
					  f->n = 3; \
				      } \
				      else \
					GF_PUTC(f->next, c); \
				 }


/*
 * default action for 8bit to quoted printable encoder
 */
#define	GF_8BIT_DEFAULT(f, c)	if((c) == ' '){ \
				     state = WSPACE; \
				 } \
				 else if(c == '\015'){ \
				     state = CCR; \
				 } \
				 else if(iscntrl(c & 0x7f) || (c == 0x7f) \
					 || (c & 0x80) || (c == '=')){ \
				     GF_8BIT_PUT_QUOTE(f, c); \
				 } \
				 else{ \
				   GF_8BIT_PUT(f, c); \
				 }


/*
 * 8BIT to QUOTED-PRINTABLE filter
 */
void
gf_8bit_qp(f, flg)
    FILTER_S *f;
    int       flg;
{
    short dummy_dots = 0, dummy_dmap = 1;
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	 register unsigned char c;
	 register int state = f->f1;

	 while(GF_GETC(f, c)){

	     /* keep track of "^JFrom " */
	     Find_Froms(f->t, dummy_dots, f->f2, dummy_dmap, c);

	     switch(state){
	       case DFL :		/* handle ordinary case */
		 GF_8BIT_DEFAULT(f, c);
		 break;

	       case CCR :		/* true line break? */
		 state = DFL;
		 if(c == '\012'){
		     GF_PUTC(f->next, '\015');
		     GF_PUTC(f->next, '\012');
		     f->n = 0L;
		 }
		 else{			/* nope, quote the CR */
		     GF_8BIT_PUT_QUOTE(f, '\015');
		     GF_8BIT_DEFAULT(f, c); /* and don't forget about c! */
		 }
		 break;

	       case WSPACE:
		 state = DFL;
		 if(c == '\015' || f->t){ /* handle the space */
		     GF_8BIT_PUT_QUOTE(f, ' ');
		     f->t = 0;		/* reset From flag */
		 }
		 else
		   GF_8BIT_PUT(f, ' ');

		 GF_8BIT_DEFAULT(f, c);	/* handle 'c' in the default way */
		 break;
	     }
	 }

	 f->f1 = state;
	 GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 switch(f->f1){
	   case CCR :
	     GF_8BIT_PUT_QUOTE(f, '\015'); /* write the last cr */
	     break;

	   case WSPACE :
	     GF_8BIT_PUT_QUOTE(f, ' ');	/* write the last space */
	     break;
	 }

	 GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint(9, (debugfile, "-- gf_reset 8bit_qp\n"));
	 f->f1 = DFL;			/* state from last character        */
	 f->f2 = 1;			/* state of "^NFrom " bitmap        */
	 f->t  = 0;
	 f->n  = 0L;			/* number of chars in current line  */
    }
}



/*
 * ISO-2022-JP to EUC (on Unix) or Shift-JIS (on PC) filter
 *
 * The routine is call ..._to_euc but it is really to either euc (unix Pine)
 * or to Shift-JIS (if PC-Pine).
 */
void
gf_2022_jp_to_euc(f, flg)
    FILTER_S *f;
    int       flg;
{
    register unsigned char c;
    register int state = f->f1;

    /*
     * f->f2 keeps track of first character of pair for Shift-JIS.
     * f->f1 is the state.
     */

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	while(GF_GETC(f, c)){
	    switch(state){
	      case ESC:				/* saw ESC */
	        if(c == '$')
		  state = ESCDOL;
	        else if(c == '(')
		  state = ESCPAR;
		else{
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, c);
		    state = DFL;
		}

	        break;

	      case ESCDOL:			/* saw ESC $ */
	        if(c == 'B' || c == '@'){
		    state = EUC;
		    f->f2 = -1;			/* first character of pair */
		}
		else{
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, '$');
		    GF_PUTC(f->next, c);
		    state = DFL;
		}

	        break;

	      case ESCPAR:			/* saw ESC ( */
		/*
		 * Mark suggests that ESC ( anychar should be treated as an
		 * end of the escape sequence (as opposed to just ESC ( B or
		 * ESC ( @). So that's what we'll do. We know it's not quite
		 * correct, but it shouldn't come up in real life.
		 * Hubert 2004-12-07
		 */
		state = DFL;
	        break;

	      case EUC:				/* filtering into euc */
		if(c == '\033')
		  state = ESC;
		else{
#ifdef _WINDOWS					/* Shift-JIS */
		    c &= 0x7f;			/* 8-bit can't win */
		    if (f->f2 >= 0){		/* second of a pair? */
			int rowOffset = (f->f2 < 95) ? 112 : 176;
			int cellOffset = (f->f2 % 2) ? ((c > 95) ? 32 : 31)
						     : 126;

			GF_PUTC(f->next, ((f->f2 + 1) >> 1) + rowOffset);
			GF_PUTC(f->next, c + cellOffset);
			f->f2 = -1;		/* restart */
		    }
		    else if(c > 0x20 && c < 0x7f)
		      f->f2 = c;		/* first of pair */
		    else{
			GF_PUTC(f->next, c);	/* write CTL as itself */
			f->f2 = -1;
		    }
#else						/* EUC */
		    GF_PUTC(f->next, (c > 0x20 && c < 0x7f) ? c | 0x80 : c);
#endif
		}

	        break;

	      case DFL:
	      default:
		if(c == '\033')
		  state = ESC;
		else
		  GF_PUTC(f->next, c);

		break;
	    }
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	switch(state){
	  case ESC:
	    GF_PUTC(f->next, '\033');
	    break;

	  case ESCDOL:
	    GF_PUTC(f->next, '\033');
	    GF_PUTC(f->next, '$');
	    break;

	  case ESCPAR:
	    GF_PUTC(f->next, '\033');	/* Don't set hibit for     */
	    GF_PUTC(f->next, '(');	/* escape sequences.       */
	    break;
	}

	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset jp_to_euc\n"));
	f->f1 = DFL;		/* state */
    }
}



/*
 * EUC (on Unix) or Shift-JIS (on PC) to ISO-2022-JP filter
 *
 * The routine is call euc_to... but it is really from either euc (unix Pine)
 * or from Shift-JIS (if PC-Pine).
 */
void
gf_euc_to_2022_jp(f, flg)
    FILTER_S *f;
    int       flg;
{
    register unsigned char c;

    /*
     * f->t lit means we've sent the start esc seq but not the end seq.
     * f->f2 keeps track of first character of pair for Shift-JIS.
     */

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	while(GF_GETC(f, c)){
	    if(f->t){
		if(f->f2 >= 0){			/* second of a pair? */
		    int adjust = c < 159;
		    int rowOffset = f->f2 < 160 ? 112 : 176;
		    int cellOffset = adjust ? (c > 127 ? 32 : 31) : 126;

		    GF_PUTC(f->next, ((f->f2 - rowOffset) << 1) - adjust);
		    GF_PUTC(f->next, c - cellOffset);
		    f->f2 = -1;
		}
		else if(c & 0x80){
#ifdef _WINDOWS					/* Shift-JIS */
		    f->f2 = c;			/* remember first of pair */
#else						/* EUC */
		    GF_PUTC(f->next, c & 0x7f);
#endif
		}
		else{
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, '(');
		    GF_PUTC(f->next, 'B');
		    GF_PUTC(f->next, c);
		    f->f2 = -1;
		    f->t = 0;
		}
	    }
	    else{
		if(c & 0x80){
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, '$');
		    GF_PUTC(f->next, 'B');
#ifdef _WINDOWS
		    f->f2 = c;
#else
		    GF_PUTC(f->next, c & 0x7f);
#endif
		    f->t = 1;
		}
		else{
		    GF_PUTC(f->next, c);
		}
	    }
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	if(f->t){
	    GF_PUTC(f->next, '\033');
	    GF_PUTC(f->next, '(');
	    GF_PUTC(f->next, 'B');
	    f->t = 0;
	    f->f2 = -1;
	}

	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset euc_to_jp\n"));
	f->t = 0;
	f->f2 = -1;
    }
}

/*
 * This filter converts characters in one character set (the character
 * set of a message, for example) to another (the user's character set).
 */
void
gf_convert_8bit_charset(f, flg)
    FILTER_S *f;
    int       flg;
{
    static unsigned char *conv_table = NULL;
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	 register unsigned char c;

	 while(GF_GETC(f, c)){
	   GF_PUTC(f->next, conv_table ? conv_table[c] : c);
	 }

	 GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint(9, (debugfile, "-- gf_reset convert_8bit_charset\n"));
	 conv_table = (f->opt) ? (unsigned char *) (f->opt) : NULL;

    }
}


/*
 * This filter converts characters in UTF-8 to an 8-bit or 16-bit charset.
 * Characters missing from the destination set, and invalid UTF-8 sequences,
 * will be converted to "?".
 */
void
gf_convert_utf8_charset(f, flg)
    FILTER_S *f;
    int       flg;
{
    static unsigned short *conv_table = NULL;
    register int more = f->f2;
    register long u = f->n;

    /*
     * "more" is the number of subsequent octets needed to complete a character,
     * it is stored in f->f2.
     * "u" is the accumulated Unicode character, it is stored in f->n
     */

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;

	while(GF_GETC(f, c)) {
	    if(!conv_table) {	/* can't do much if no conversion table */
		GF_PUTC(f->next, c);
	    }
				/* UTF-8 continuation? */
	    else if((c > 0x7f) && (c < 0xc0)) {
		if (more) {
		    u <<= 6;	/* shift current value by 6 bits */
		    u |= c & 0x3f;
		    if (!--more){ /* last octet? */
			if (u < 0xffff){ /* translate if in the BMP */
			    if((u = conv_table[u]) > 0xff) {
				c = (unsigned char) (u >> 8);
				GF_PUTC(f->next, c);
			    }
			    c = (unsigned char) u & 0xff;
			}
			else c = '?'; /* non-BMP character */
			GF_PUTC(f->next, c);
		    }
		}
		else {		/* continuation when not in progress */
		    GF_PUTC(f->next, '?');
		}
	    }
	    else{
	        if(more) {	/* incomplete UTF-8 character */
		    GF_PUTC(f->next, '?');
		    more = 0;
		}
		if(c < 0x80){ /* U+0000 - U+007f */
		    GF_PUTC(f->next, c);
		}
		else if(c < 0xe0){ /* U+0080 - U+07ff */
		    u = c & 0x1f; /* first 5 bits of 12 */
		    more = 1;
		}
		else if(c < 0xf0){ /* U+1000 - U+ffff */
		    u = c & 0x0f; /* first 4 bits of 16 */
		    more = 2;
		}
				/* in case we ever support non-BMP Unicode */
		else if (c < 0xf8){ /* U+10000 - U+10ffff */
		    u = c & 0x07; /* first 3 bits of 20.5 */
		    more = 3;
		}
#if 0	/* ISO 10646 not in Unicode */
		else if (c < 0xfc){ /* ISO 10646 20000 - 3ffffff */
		    u = c & 0x03; /* first 2 bits of 26 */
		    more = 4;
		}
		else if (c < 0xfe){ /* ISO 10646 4000000 - 7fffffff */
		    u = c & 0x03; /* first 2 bits of 26 */
		    more = 5;
		}
#endif
		else{		/* not in Unicode */
		    GF_PUTC(f->next, '?');
		}
	    }
	}

	f->f2 = more;
	f->n = u;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint(9, (debugfile, "-- gf_reset convert_utf8_charset\n"));
	 conv_table = f->opt;
	 f->f2 = 0;
	 f->n = 0L;
    }
}


/*
 * RICHTEXT-TO-PLAINTEXT filter
 */

/*
 * option to be used by rich2plain (NOTE: if this filter is ever
 * used more than once in a pipe, all instances will have the same
 * option value)
 */


/*----------------------------------------------------------------------
      richtext to plaintext filter

 Args: f --
	flg  --

  This basically removes all richtext formatting. A cute hack is used
  to get bold and underlining to work.
  Further work could be done to handle things like centering and right
  and left flush, but then it could no longer be done in place. This
  operates on text *with* CRLF's.

  WARNING: does not wrap lines!
 ----*/
void
gf_rich2plain(f, flg)
    FILTER_S *f;
    int       flg;
{
    static int rich_bold_on = 0, rich_uline_on = 0;

/* BUG: qoute incoming \255 values */
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	 register unsigned char c;
	 register int state = f->f1;

	 while(GF_GETC(f, c)){

	     switch(state){
	       case TOKEN :		/* collect a richtext token */
		 if(c == '>'){		/* what should we do with it? */
		     state       = DFL;	/* return to default next time */
		     *(f->linep) = '\0';	/* cap off token */
		     if(f->line[0] == 'l' && f->line[1] == 't'){
			 GF_PUTC(f->next, '<'); /* literal '<' */
		     }
		     else if(f->line[0] == 'n' && f->line[1] == 'l'){
			 GF_PUTC(f->next, '\015');/* newline! */
			 GF_PUTC(f->next, '\012');
		     }
		     else if(!strcmp("comment", f->line)){
			 (f->f2)++;
		     }
		     else if(!strcmp("/comment", f->line)){
			 f->f2 = 0;
		     }
		     else if(!strcmp("/paragraph", f->line)) {
			 GF_PUTC(f->next, '\r');
			 GF_PUTC(f->next, '\n');
			 GF_PUTC(f->next, '\r');
			 GF_PUTC(f->next, '\n');
		     }
		     else if(!f->opt /* gf_rich_plain */){
			 if(!strcmp(f->line, "bold")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_BOLDON);
			     rich_bold_on = 1;
			 } else if(!strcmp(f->line, "/bold")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_BOLDOFF);
			     rich_bold_on = 0;
			 } else if(!strcmp(f->line, "italic")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_ULINEON);
			     rich_uline_on = 1;
			 } else if(!strcmp(f->line, "/italic")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_ULINEOFF);
			     rich_uline_on = 0;
			 } else if(!strcmp(f->line, "underline")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_ULINEON);
			     rich_uline_on = 1;
			 } else if(!strcmp(f->line, "/underline")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_ULINEOFF);
			     rich_uline_on = 0;
			 }
		     }
		     /* else we just ignore the token! */

		     f->linep = f->line;	/* reset token buffer */
		 }
		 else{			/* add char to token */
		     if(f->linep - f->line > 40){
			 /* What? rfc1341 says 40 char tokens MAX! */
			 fs_give((void **)&(f->line));
			 gf_error("Richtext token over 40 characters");
			 /* NO RETURN */
		     }

		     *(f->linep)++ = isupper((unsigned char)c) ? c-'A'+'a' : c;
		 }
		 break;

	       case CCR   :
		 state = DFL;		/* back to default next time */
		 if(c == '\012'){	/* treat as single space?    */
		     GF_PUTC(f->next, ' ');
		     break;
		 }
		 /* fall thru to process c */

	       case DFL   :
	       default:
		 if(c == '<')
		   state = TOKEN;
		 else if(c == '\015')
		   state = CCR;
		 else if(!f->f2)		/* not in comment! */
		   GF_PUTC(f->next, c);

		 break;
	     }
	 }

	 f->f1 = state;
	 GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 if(f->f1 = (f->linep != f->line)){
	     /* incomplete token!! */
	     gf_error("Incomplete token in richtext");
	     /* NO RETURN */
	 }

	 if(rich_uline_on){
	     GF_PUTC(f->next, TAG_EMBED);
	     GF_PUTC(f->next, TAG_ULINEOFF);
	     rich_uline_on = 0;
	 }
	 if(rich_bold_on){
	     GF_PUTC(f->next, TAG_EMBED);
	     GF_PUTC(f->next, TAG_BOLDOFF);
	     rich_bold_on = 0;
	 }

	 fs_give((void **)&(f->line));
	 GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint(9, (debugfile, "-- gf_reset rich2plain\n"));
	 f->f1 = DFL;			/* state */
	 f->f2 = 0;			/* set means we're in a comment */
	 f->linep = f->line = (char *)fs_get(45 * sizeof(char));
    }
}


/*
 * function called from the outside to set
 * richtext filter's options
 */
void *
gf_rich2plain_opt(plain)
    int plain;
{
    return((void *) plain);
}



/*
 * ENRICHED-TO-PLAIN text filter
 */

#define	TEF_QUELL	0x01
#define	TEF_NOFILL	0x02



/*----------------------------------------------------------------------
      enriched text to plain text filter (ala rfc1523)

 Args: f -- state and input data
	flg --

  This basically removes all enriched formatting. A cute hack is used
  to get bold and underlining to work.

  Further work could be done to handle things like centering and right
  and left flush, but then it could no longer be done in place. This
  operates on text *with* CRLF's.

  WARNING: does not wrap lines!
 ----*/
void
gf_enriched2plain(f, flg)
    FILTER_S *f;
    int       flg;
{
    static int enr_uline_on = 0, enr_bold_on = 0;

/* BUG: qoute incoming \255 values */
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	 register unsigned char c;
	 register int state = f->f1;

	 while(GF_GETC(f, c)){

	     switch(state){
	       case TOKEN :		/* collect a richtext token */
		 if(c == '>'){		/* what should we do with it? */
		     int   off   = *f->line == '/';
		     char *token = f->line + (off ? 1 : 0);
		     state	= DFL;
		     *f->linep   = '\0';
		     if(!strcmp("param", token)){
			 if(off)
			   f->f2 &= ~TEF_QUELL;
			 else
			   f->f2 |= TEF_QUELL;
		     }
		     else if(!strcmp("nofill", token)){
			 if(off)
			   f->f2 &= ~TEF_NOFILL;
			 else
			   f->f2 |= TEF_NOFILL;
		     }
		     else if(!f->opt /* gf_enriched_plain */){
			 /* Following is a cute hack or two to get
			    bold and underline on the screen.
			    See Putline0n() where these codes are
			    interpreted */
			 if(!strcmp("bold", token)) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, off ? TAG_BOLDOFF : TAG_BOLDON);
			     enr_bold_on = off ? 0 : 1;
			 } else if(!strcmp("italic", token)) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, off ? TAG_ULINEOFF : TAG_ULINEON);
			     enr_uline_on = off ? 0 : 1;
			 } else if(!strcmp("underline", token)) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, off ? TAG_ULINEOFF : TAG_ULINEON);
			     enr_uline_on = off ? 0 : 1;
			 }
		     }
		     /* else we just ignore the token! */

		     f->linep = f->line;	/* reset token buffer */
		 }
		 else if(c == '<'){		/* literal '<'? */
		     if(f->linep == f->line){
			 GF_PUTC(f->next, '<');
			 state = DFL;
		     }
		     else{
			 fs_give((void **)&(f->line));
			 gf_error("Malformed Enriched text: unexpected '<'");
			 /* NO RETURN */
		     }
		 }
		 else{			/* add char to token */
		     if(f->linep - f->line > 60){ /* rfc1523 says 60 MAX! */
			 fs_give((void **)&(f->line));
			 gf_error("Malformed Enriched text: token too long");
			 /* NO RETURN */
		     }

		     *(f->linep)++ = isupper((unsigned char)c) ? c-'A'+'a' : c;
		 }
		 break;

	       case CCR   :
		 if(c != '\012'){	/* treat as single space?    */
		     state = DFL;	/* lone cr? */
		     f->f2 &= ~TEF_QUELL;
		     GF_PUTC(f->next, '\015');
		     goto df;
		 }

		 state = CLF;
		 break;

	       case CLF   :
		 if(c == '\015'){	/* treat as single space?    */
		     state = CCR;	/* repeat crlf's mean real newlines */
		     f->f2 |= TEF_QUELL;
		     GF_PUTC(f->next, '\r');
		     GF_PUTC(f->next, '\n');
		     break;
		 }
		 else{
		     state = DFL;
		     if(!((f->f2) & TEF_QUELL))
		       GF_PUTC(f->next, ' ');

		     f->f2 &= ~TEF_QUELL;
		 }

		 /* fall thru to take care of 'c' */

	       case DFL   :
	       default :
	       df :
		 if(c == '<')
		   state = TOKEN;
		 else if(c == '\015' && (!((f->f2) & TEF_NOFILL)))
		   state = CCR;
		 else if(!((f->f2) & TEF_QUELL))
		   GF_PUTC(f->next, c);

		 break;
	     }
	 }

	 f->f1 = state;
	 GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 if(f->f1 = (f->linep != f->line)){
	     /* incomplete token!! */
	     gf_error("Incomplete token in richtext");
	     /* NO RETURN */
	 }
	 if(enr_uline_on){
	     GF_PUTC(f->next, TAG_EMBED);
	     GF_PUTC(f->next, TAG_ULINEOFF);
	     enr_uline_on = 0;
	 }
	 if(enr_bold_on){
	     GF_PUTC(f->next, TAG_EMBED);
	     GF_PUTC(f->next, TAG_BOLDOFF);
	     enr_bold_on = 0;
	 }

	 /* Make sure we end with a newline so everything gets flushed */
	 GF_PUTC(f->next, '\015');
	 GF_PUTC(f->next, '\012');

	 fs_give((void **)&(f->line));

	 GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint(9, (debugfile, "-- gf_reset enriched2plain\n"));
	 f->f1 = DFL;			/* state */
	 f->f2 = 0;			/* set means we're in a comment */
	 f->linep = f->line = (char *)fs_get(65 * sizeof(char));
    }
}


/*
 * function called from the outside to set
 * richtext filter's options
 */
void *
gf_enriched2plain_opt(plain)
    int plain;
{
    return((void *) plain);
}



/*
 * HTML-TO-PLAIN text filter
 */


/* OK, here's the plan:

 * a universal output function handles writing  chars and worries
 *    about wrapping.

 * a unversal element collector reads chars and collects params
 * and dispatches the appropriate element handler.

 * element handlers are stacked.  The most recently dispatched gets
 * first crack at the incoming character stream.  It passes bytes it's
 * done with or not interested in to the next

 * installs that handler as the current one collecting data...

 * stacked handlers take their params from the element collector and
 * accept chars or do whatever they need to do.  Sort of a vertical
 * piping? recursion-like? hmmm.

 * at least I think this is how it'll work. tres simple, non?

 */


/*
 * Some important constants
 */
#define	HTML_BUF_LEN	2048		/* max scratch buffer length */
#define	MAX_ENTITY	20		/* maximum length of an entity */
#define	MAX_ELEMENT	72		/* maximum length of an element */
#define	HTML_BADVALUE	0x0100		/* good data, but bad entity value */
#define	HTML_BADDATA	0x0200		/* bad data found looking for entity */
#define	HTML_LITERAL	0x0400		/* Literal character value */
#define HTML_EXTVALUE   0x0800          /* good data, extended value (Unicode) */
#define	HTML_NEWLINE	0x010A		/* hard newline */
#define	HTML_DOBOLD	0x0400		/* Start Bold display */
#define	HTML_ID_GET	0		/* indent func: return current val */
#define	HTML_ID_SET	1		/* indent func: set to absolute val */
#define	HTML_ID_INC	2		/* indent func: increment by val */
#define	HTML_HX_CENTER	0x0001
#define	HTML_HX_ULINE	0x0002


/*
 * Types used to manage HTML parsing
 */
typedef int (*html_f) PROTO(());

/*
 * Handler data, state information including function that uses it
 */
typedef struct handler_s {
    FILTER_S	     *html_data;
    struct handler_s *below;
    html_f	      f;
    long	      x, y, z;
    unsigned char    *s;
} HANDLER_S;

static void html_handoff PROTO((HANDLER_S *, int));


/*
 * to help manage line wrapping.
 */
typedef	struct _wrap_line {
    char *buf;				/* buf to collect wrapped text */
    int	  used,				/* number of chars in buf */
	   width,			/* text's width as displayed  */
	   len;				/* length of allocated buf */
} WRAPLINE_S;


/*
 * to help manage centered text
 */
typedef	struct _center_s {
    WRAPLINE_S line;			/* buf to assembled centered text */
    WRAPLINE_S word;			/* word being to append to Line */
    int	       anchor;
    short      embedded;
    short      space;
} CENTER_S;


/*
 * Collector data and state information
 */
typedef	struct collector_s {
    char        buf[HTML_BUF_LEN];	/* buffer to collect data */
    int		len;			/* length of that buffer  */
    unsigned    end_tag:1;		/* collecting a closing tag */
    unsigned    hit_equal:1;		/* collecting right half of attrib */
    unsigned	mkup_decl:1;		/* markup declaration */
    unsigned	start_comment:1;	/* markup declaration comment */
    unsigned	end_comment:1;		/* legit comment format */
    unsigned	hyphen:1;		/* markup hyphen read */
    unsigned	badform:1;		/* malformed markup element */
    unsigned	overrun:1;		/* Overran buf above */
    char	quoted;			/* quoted element param value */
    char       *element;		/* element's collected name */
    PARAMETER  *attribs;		/* element's collected attributes */
    PARAMETER  *cur_attrib;		/* attribute now being collected */
} CLCTR_S;


/*
 * State information for all element handlers
 */
typedef struct html_data {
    HANDLER_S  *h_stack;		/* handler list */
    CLCTR_S    *el_data;		/* element collector data */
    CENTER_S   *centered;		/* struct to manage centered text */
    int	      (*token) PROTO((FILTER_S *, int));
    char	quoted;			/* quoted, by either ' or ", text */
    short	indent_level;		/* levels of indention */
    int		in_anchor;		/* text now being written to anchor */
    int		blanks;			/* Consecutive blank line count */
    int		wrapcol;		/* column to wrap lines on */
    int	       *prefix;			/* buffer containing Anchor prefix */
    int		prefix_used;
    long        line_bufsize;           /* current size of the line buffer */
    COLOR_PAIR *color;
    unsigned	wrapstate:1;		/* whether or not to wrap output */
    unsigned	li_pending:1;		/* <LI> next token expected */
    unsigned	de_pending:1;		/* <DT> or <DD> next token expected */
    unsigned	bold_on:1;		/* currently bolding text */
    unsigned	uline_on:1;		/* currently underlining text */
    unsigned	center:1;		/* center output text */
    unsigned	bitbucket:1;		/* Ignore input */
    unsigned	head:1;			/* In doc's HEAD */
    unsigned	alt_entity:1;		/* use alternative entity values */
    unsigned	wrote:1;		/* anything witten yet? */
} HTML_DATA_S;


/*
 * HTML filter options
 */
typedef	struct _html_opts {
    char      *base;			/* Base URL for this html file */
    int	       columns,			/* Display columns */
	       indent;			/* Right margin */
    HANDLE_S **handlesp;		/* Head of handles */
    unsigned   strip:1;			/* Hilite TAGs allowed */
    unsigned   handles_loc:1;		/* Local handles requested? */
    unsigned   outputted:1;		/* any */
} HTML_OPT_S;


/*
 * Some macros to make life a little easier
 */
#define	WRAP_COLS(X)	((X)->opt ? ((HTML_OPT_S *)(X)->opt)->columns : 80)
#define	HTML_INDENT(X)	((X)->opt ? ((HTML_OPT_S *)(X)->opt)->indent : 0)
#define	HTML_WROTE(X)	(HD(X)->wrote)
#define	HTML_BASE(X)	((X)->opt ? ((HTML_OPT_S *)(X)->opt)->base : NULL)
#define	STRIP(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->strip)
#define	HANDLESP(X)	(((HTML_OPT_S *)(X)->opt)->handlesp)
#define	DO_HANDLES(X)	((X)->opt && HANDLESP(X))
#define	HANDLES_LOC(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->handles_loc)
#define	MAKE_LITERAL(C)	(HTML_LITERAL | ((C) & 0xff))
#define	IS_LITERAL(C)	(HTML_LITERAL & (C))
#define	HD(X)		((HTML_DATA_S *)(X)->data)
#define	ED(X)		(HD(X)->el_data)
#define	HTML_ISSPACE(C)	(IS_LITERAL(C) == 0 && isspace((unsigned char) (C)))
#define	NEW_CLCTR(X)	{						\
			   ED(X) = (CLCTR_S *)fs_get(sizeof(CLCTR_S));  \
			   memset(ED(X), 0, sizeof(CLCTR_S));	\
			   HD(X)->token = html_element_collector;	\
			 }

#define	FREE_CLCTR(X)	{						\
			   if(ED(X)->attribs){				\
			       PARAMETER *p;				\
			       while(p = ED(X)->attribs){		\
				   ED(X)->attribs = ED(X)->attribs->next; \
				   if(p->attribute)			\
				     fs_give((void **)&p->attribute);	\
				   if(p->value)				\
				     fs_give((void **)&p->value);	\
				   fs_give((void **)&p);		\
			       }					\
			   }						\
			   if(ED(X)->element)				\
			     fs_give((void **) &ED(X)->element);	\
			   fs_give((void **) &ED(X));			\
			   HD(X)->token = NULL;				\
			 }
#define	HANDLERS(X)	(HD(X)->h_stack)
#define	BOLD_BIT(X)	(HD(X)->bold_on)
#define	ULINE_BIT(X)	(HD(X)->uline_on)
#define	CENTER_BIT(X)	(HD(X)->center)
#define	HTML_FLUSH(X)	{						    \
			   html_write(X, (X)->line, (X)->linep - (X)->line); \
			   (X)->linep = (X)->line;			    \
			   (X)->f2 = 0L;   				    \
			 }
#define	HTML_BOLD(X, S) if(! STRIP(X)){					\
			   if(S){					\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_BOLDON);		\
			   }						\
			   else if(!(S)){				\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_BOLDOFF);		\
			   }						\
			 }
#define	HTML_ULINE(X, S)						\
			 if(! STRIP(X)){				\
			   if(S){					\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_ULINEON);		\
			   }						\
			   else if(!(S)){				\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_ULINEOFF);		\
			   }						\
			 }
#define WRAPPED_LEN(X)	((HD(f)->centered) \
			    ? (HD(f)->centered->line.width \
				+ HD(f)->centered->word.width \
				+ ((HD(f)->centered->line.width \
				    && HD(f)->centered->word.width) \
				    ? 1 : 0)) \
			    : 0)
#define	HTML_DUMP_LIT(F, S, L)	{					    \
				   int i, c;				    \
				   for(i = 0; i < (L); i++){		    \
				       c = isspace((unsigned char)(S)[i])   \
					     ? (S)[i]			    \
					     : MAKE_LITERAL((S)[i]);	    \
				       HTML_TEXT(F, c);			    \
				   }					    \
				 }
#define	HTML_PROC(F, C) {						    \
			   if(HD(F)->token){				    \
			       int i;					    \
			       if(i = (*(HD(F)->token))(F, C)){		    \
				   if(i < 0){				    \
				       HTML_DUMP_LIT(F, "<", 1);	    \
				       if(HD(F)->el_data->element){	    \
					   HTML_DUMP_LIT(F,		    \
					    HD(F)->el_data->element,	    \
					    strlen(HD(F)->el_data->element));\
				       }				    \
				       if(HD(F)->el_data->len){		    \
					   HTML_DUMP_LIT(F,		    \
						    HD(F)->el_data->buf,    \
						    HD(F)->el_data->len);   \
				       }				    \
				       HTML_TEXT(F, C);			    \
				   }					    \
				   FREE_CLCTR(F);			    \
			       }					    \
			    }						    \
			    else if((C) == '<'){			    \
				NEW_CLCTR(F);				    \
			    }						    \
			    else					    \
			      HTML_TEXT(F, C);				    \
			  }
#define HTML_LINEP_PUTC(F, C) {						    \
		   if((F)->linep - (F)->line >= (HD(F)->line_bufsize - 1)){ \
		       size_t offset = (F)->linep - (F)->line;		    \
		       fs_resize((void **) &(F)->line,			    \
				 (HD(F)->line_bufsize * 2) * sizeof(char)); \
		       HD(F)->line_bufsize *= 2;			    \
		       (F)->linep = &(F)->line[offset];			    \
		   }							    \
		   *(F)->linep++ = (C);					    \
	       }
#define	HTML_TEXT(F, C)	switch((F)->f1){				    \
			     case WSPACE :				    \
			       if(HTML_ISSPACE(C)) /* ignore repeated WS */  \
				 break;					    \
			       HTML_TEXT_OUT(F, ' ');	    \
			       (F)->f1 = DFL;/* stop sending chars here */   \
			       /* fall thru to process 'c' */		    \
			     case DFL:					    \
			       if(HD(F)->bitbucket)			    \
				 (F)->f1 = DFL;	/* no op */		    \
			       else if(HTML_ISSPACE(C) && HD(F)->wrapstate)  \
				 (F)->f1 = WSPACE;/* coalesce white space */ \
			       else HTML_TEXT_OUT(F, C);		    \
			       break;					    \
			 }
#define	HTML_TEXT_OUT(F, C) if(HANDLERS(F)) /* let handlers see C */	    \
			       (*HANDLERS(F)->f)(HANDLERS(F),(C),GF_DATA);   \
			     else					    \
			       html_output(F, C);
#ifdef	DEBUG
#define	HTML_DEBUG_EL(S, D)   {						    \
				 dprint(5, (debugfile, "-- html %s: %s\n",  \
					    S ? S : "?",		    \
					    (D)->element		    \
						 ? (D)->element : "NULL")); \
				 if(debug > 5){				    \
				     PARAMETER *p;			    \
				     for(p = (D)->attribs;		    \
					 p && p->attribute;		    \
					 p = p->next)			    \
				       dprint(6, (debugfile,		    \
						  " PARM: %s%s%s\n",	    \
						  p->attribute		    \
						    ? p->attribute : "NULL",\
						  p->value ? "=" : "",	    \
						  p->value ? p->value : ""));\
				 }					    \
			       }
#else
#define	HTML_DEBUG_EL(S, D)
#endif

#ifndef SYSTEM_PINE_INFO_PATH
#define SYSTEM_PINE_INFO_PATH "/usr/local/lib/pine.info"
#endif
#define CHTML_VAR_EXPAND(S) (!strcmp(S, "PINE_INFO_PATH")   \
			     ? SYSTEM_PINE_INFO_PATH : S)

/*
 * Protos for Tag handlers
 */
int	html_head PROTO((HANDLER_S *, int, int));
int	html_base PROTO((HANDLER_S *, int, int));
int	html_title PROTO((HANDLER_S *, int, int));
int	html_a PROTO((HANDLER_S *, int, int));
int	html_br PROTO((HANDLER_S *, int, int));
int	html_hr PROTO((HANDLER_S *, int, int));
int	html_p PROTO((HANDLER_S *, int, int));
int	html_tr PROTO((HANDLER_S *, int, int));
int	html_td PROTO((HANDLER_S *, int, int));
int	html_b PROTO((HANDLER_S *, int, int));
int	html_i PROTO((HANDLER_S *, int, int));
int	html_img PROTO((HANDLER_S *, int, int));
int	html_form PROTO((HANDLER_S *, int, int));
int	html_ul PROTO((HANDLER_S *, int, int));
int	html_ol PROTO((HANDLER_S *, int, int));
int	html_menu PROTO((HANDLER_S *, int, int));
int	html_dir PROTO((HANDLER_S *, int, int));
int	html_li PROTO((HANDLER_S *, int, int));
int	html_h1 PROTO((HANDLER_S *, int, int));
int	html_h2 PROTO((HANDLER_S *, int, int));
int	html_h3 PROTO((HANDLER_S *, int, int));
int	html_h4 PROTO((HANDLER_S *, int, int));
int	html_h5 PROTO((HANDLER_S *, int, int));
int	html_h6 PROTO((HANDLER_S *, int, int));
int	html_blockquote PROTO((HANDLER_S *, int, int));
int	html_address PROTO((HANDLER_S *, int, int));
int	html_pre PROTO((HANDLER_S *, int, int));
int	html_center PROTO((HANDLER_S *, int, int));
int	html_div PROTO((HANDLER_S *, int, int));
int	html_dl PROTO((HANDLER_S *, int, int));
int	html_dt PROTO((HANDLER_S *, int, int));
int	html_dd PROTO((HANDLER_S *, int, int));
int	html_style PROTO((HANDLER_S *, int, int));
int	html_script PROTO((HANDLER_S *, int, int));

/*
 * Proto's for support routines
 */
void	html_pop PROTO((FILTER_S *, html_f));
void	html_push PROTO((FILTER_S *, html_f));
int	html_element_collector PROTO((FILTER_S *, int));
int	html_element_flush PROTO((CLCTR_S *));
void	html_element_comment PROTO((FILTER_S *, char *));
void	html_element_output PROTO((FILTER_S *, int));
int	html_entity_collector PROTO((FILTER_S *, int, char **));
void	html_a_prefix PROTO((FILTER_S *));
void	html_a_finish PROTO((HANDLER_S *));
void	html_a_output_prefix PROTO((FILTER_S *, int));
void	html_a_relative PROTO((char *, char *, HANDLE_S *));
int	html_indent PROTO((FILTER_S *, int, int));
void	html_blank PROTO((FILTER_S *, int));
void	html_newline PROTO((FILTER_S *));
void	html_output PROTO((FILTER_S *, int));
void	html_output_flush PROTO((FILTER_S *));
void	html_output_centered PROTO((FILTER_S *, int));
void	html_centered_handle PROTO((int *, char *, int));
void	html_centered_putc PROTO((WRAPLINE_S *, int));
void	html_centered_flush PROTO((FILTER_S *));
void	html_centered_flush_line PROTO((FILTER_S *));
void	html_write_anchor PROTO((FILTER_S *, int));
void	html_write_newline PROTO((FILTER_S *));
void	html_write_indent PROTO((FILTER_S *, int));
void	html_write PROTO((FILTER_S *, char *, int));
void	html_putc PROTO((FILTER_S *, int));


/*
 * Named entity table -- most from HTML 2.0 (rfc1866) plus some from
 *			 W3C doc "Additional named entities for HTML"
 */
static struct html_entities {
    char *name;			/* entity name */
    unsigned int   value;       /* entity value */
    char  *plain;		/* plain text representation */
    unsigned char altvalue;     /* non-Unicode equivalent */
} entity_tab[] = {
    {"quot",	042},		/* Double quote sign */
    {"amp",	046},		/* Ampersand */
    {"bull",	052},		/* Bullet */
    {"ndash",	055},		/* Dash */
    {"mdash",	055},		/* Dash */
    {"lt",	074},		/* Less than sign */
    {"gt",	076},		/* Greater than sign */
    {"nbsp",	0240, " "},	/* no-break space */
    {"iexcl",	0241},		/* inverted exclamation mark */
    {"cent",	0242},		/* cent sign */
    {"pound",	0243},		/* pound sterling sign */
    {"curren",	0244, "CUR"},	/* general currency sign */
    {"yen",	0245},		/* yen sign */
    {"brvbar",	0246, "|"},	/* broken (vertical) bar */
    {"sect",	0247},		/* section sign */
    {"uml",	0250, "\""},		/* umlaut (dieresis) */
    {"copy",	0251, "(C)"},	/* copyright sign */
    {"ordf",	0252, "a"},	/* ordinal indicator, feminine */
    {"laquo",	0253, "<<"},	/* angle quotation mark, left */
    {"not",	0254, "NOT"},	/* not sign */
    {"shy",	0255, "-"},	/* soft hyphen */
    {"reg",	0256, "(R)"},	/* registered sign */
    {"macr",	0257},		/* macron */
    {"deg",	0260, "DEG"},	/* degree sign */
    {"plusmn",	0261, "+/-"},	/* plus-or-minus sign */
    {"sup2",	0262},		/* superscript two */
    {"sup3",	0263},		/* superscript three */
    {"acute",	0264, "'"},	/* acute accent */
    {"micro",	0265},		/* micro sign */
    {"para",	0266},		/* pilcrow (paragraph sign) */
    {"middot",	0267},		/* middle dot */
    {"cedil",	0270},		/* cedilla */
    {"sup1",	0271},		/* superscript one */
    {"ordm",	0272, "o"},	/* ordinal indicator, masculine */
    {"raquo",	0273, ">>"},	/* angle quotation mark, right */
    {"frac14",	0274, " 1/4"},	/* fraction one-quarter */
    {"frac12",	0275, " 1/2"},	/* fraction one-half */
    {"frac34",	0276, " 3/4"},	/* fraction three-quarters */
    {"iquest",	0277},		/* inverted question mark */
    {"Agrave",	0300, "A"},	/* capital A, grave accent */
    {"Aacute",	0301, "A"},	/* capital A, acute accent */
    {"Acirc",	0302, "A"},	/* capital A, circumflex accent */
    {"Atilde",	0303, "A"},	/* capital A, tilde */
    {"Auml",	0304, "AE"},	/* capital A, dieresis or umlaut mark */
    {"Aring",	0305, "A"},	/* capital A, ring */
    {"AElig",	0306, "AE"},	/* capital AE diphthong (ligature) */
    {"Ccedil",	0307, "C"},	/* capital C, cedilla */
    {"Egrave",	0310, "E"},	/* capital E, grave accent */
    {"Eacute",	0311, "E"},	/* capital E, acute accent */
    {"Ecirc",	0312, "E"},	/* capital E, circumflex accent */
    {"Euml",	0313, "E"},	/* capital E, dieresis or umlaut mark */
    {"Igrave",	0314, "I"},	/* capital I, grave accent */
    {"Iacute",	0315, "I"},	/* capital I, acute accent */
    {"Icirc",	0316, "I"},	/* capital I, circumflex accent */
    {"Iuml",	0317, "I"},	/* capital I, dieresis or umlaut mark */
    {"ETH",	0320, "DH"},	/* capital Eth, Icelandic */
    {"Ntilde",	0321, "N"},	/* capital N, tilde */
    {"Ograve",	0322, "O"},	/* capital O, grave accent */
    {"Oacute",	0323, "O"},	/* capital O, acute accent */
    {"Ocirc",	0324, "O"},	/* capital O, circumflex accent */
    {"Otilde",	0325, "O"},	/* capital O, tilde */
    {"Ouml",	0326, "OE"},	/* capital O, dieresis or umlaut mark */
    {"times",	0327, "x"},	/* multiply sign */
    {"Oslash",	0330, "O"},	/* capital O, slash */
    {"Ugrave",	0331, "U"},	/* capital U, grave accent */
    {"Uacute",	0332, "U"},	/* capital U, acute accent */
    {"Ucirc",	0333, "U"},	/* capital U, circumflex accent */
    {"Uuml",	0334, "UE"},	/* capital U, dieresis or umlaut mark */
    {"Yacute",	0335, "Y"},	/* capital Y, acute accent */
    {"THORN",	0336, "P"},	/* capital THORN, Icelandic */
    {"szlig",	0337, "ss"},	/* small sharp s, German (sz ligature) */
    {"agrave",	0340, "a"},	/* small a, grave accent */
    {"aacute",	0341, "a"},	/* small a, acute accent */
    {"acirc",	0342, "a"},	/* small a, circumflex accent */
    {"atilde",	0343, "a"},	/* small a, tilde */
    {"auml",	0344, "ae"},	/* small a, dieresis or umlaut mark */
    {"aring",	0345, "a"},	/* small a, ring */
    {"aelig",	0346, "ae"},	/* small ae diphthong (ligature) */
    {"ccedil",	0347, "c"},	/* small c, cedilla */
    {"egrave",	0350, "e"},	/* small e, grave accent */
    {"eacute",	0351, "e"},	/* small e, acute accent */
    {"ecirc",	0352, "e"},	/* small e, circumflex accent */
    {"euml",	0353, "e"},	/* small e, dieresis or umlaut mark */
    {"igrave",	0354, "i"},	/* small i, grave accent */
    {"iacute",	0355, "i"},	/* small i, acute accent */
    {"icirc",	0356, "i"},	/* small i, circumflex accent */
    {"iuml",	0357, "i"},	/* small i, dieresis or umlaut mark */
    {"eth",	0360, "dh"},	/* small eth, Icelandic */
    {"ntilde",	0361, "n"},	/* small n, tilde */
    {"ograve",	0362, "o"},	/* small o, grave accent */
    {"oacute",	0363, "o"},	/* small o, acute accent */
    {"ocirc",	0364, "o"},	/* small o, circumflex accent */
    {"otilde",	0365, "o"},	/* small o, tilde */
    {"ouml",	0366, "oe"},	/* small o, dieresis or umlaut mark */
    {"divide",	0367, "/"},	/* divide sign */
    {"oslash",	0370, "o"},	/* small o, slash */
    {"ugrave",	0371, "u"},	/* small u, grave accent */
    {"uacute",	0372, "u"},	/* small u, acute accent */
    {"ucirc",	0373, "u"},	/* small u, circumflex accent */
    {"uuml",	0374, "ue"},	/* small u, dieresis or umlaut mark */
    {"yacute",	0375, "y"},	/* small y, acute accent */
    {"thorn",	0376, "p"},	/* small thorn, Icelandic */
    {"yuml",	0377, "y"},	/* small y, dieresis or umlaut mark */
    {"#8192", 0x2000, NULL, 040},  /* 2000 en quad */
    {"#8193", 0x2001, NULL, 040},  /* 2001 em quad */
    {"#8194", 0x2002, NULL, 040},  /* 2002 en space  */
    {"#8195", 0x2003, NULL, 040},  /* 2003 em space  */
    {"#8196", 0x2004, NULL, 040},  /* 2004 thick space */
    {"#8197", 0x2005, NULL, 040},  /* 2005 mid space */
    {"#8198", 0x2006, NULL, 040},  /* 2006 thin space */
    {"#8199", 0x2007, NULL, 040},  /* 2007 figure space */
    {"#8200", 0x2008, NULL, 040},  /* 2008 punctuation space */
    {"#8201", 0x2009, NULL, 040},  /* 2009 thin space */
    {"#8202", 0x200A, NULL, 0},    /* 200A hair space */
    {"#8203", 0x200B, NULL, 0},    /* 200B zero width space */
    {"#8204", 0x200C, NULL, 0},    /* 200C zero width non-joiner */
    {"#8205", 0x200D, NULL, 0},    /* 200D zero width joiner */
    {"#8206", 0x200E, NULL, 0},    /* 200E left-to-right mark */
    {"#8207", 0x200F, NULL, 0},    /* 200F right to left mark */
    {"#8208", 0x2010, NULL, 055},  /* 2010 hyphen */
    {"#8209", 0x2011, NULL, 055},  /* 2011 no-break hyphen */
    {"#8210", 0x2012, NULL, 055},  /* 2012 figure dash */
    {"#8211", 0x2013, NULL, 055},  /* 2013 en dash */
    {"#8212", 0x2014, NULL, 055},  /* 2014 em dash */
    {"#8213", 0x2015, "--", 0},    /* 2015 horizontal bar */
    {"#8214", 0x2016, "||", 0},    /* 2016 double vertical line */
    {"#8215", 0x2017, "__", 0},    /* 2017 double low line */
    {"#8216", 0x2018, NULL, 0140}, /* 2018 left single quotation mark */
    {"#8217", 0x2019, NULL, 047},  /* 2019 right single quotation mark */
    {"#8218", 0x201A, NULL, 054},  /* 201A single low-9 quotation mark */
    {"#8219", 0x201B, NULL, 0140}, /* 201B single high reversed-9 quotation mark */
    {"#8220", 0x201C, NULL, 042},  /* 201C left double quote */
    {"#8221", 0x201D, NULL, 042},  /* 201D right double quote */
    {"#8222", 0x201E, ",,", 0},    /* 201E double low-9 quotation mark */
    {"#8223", 0x201F, "``", 0},    /* 201F double high reversed-9 quotation mark  */
    {"#8224", 0x2020, NULL, 0},    /* 2020 dagger */
    {"#8225", 0x2021, NULL, 0},    /* 2021 double dagger */
    {"#8226", 0x2022, NULL, 0267}, /* 2022 bullet */
    {"#8227", 0x2023, NULL, 0},    /* 2023 triangular bullet */
    {"#8228", 0x2024, NULL, 056},  /* 2024 one dot leader */
    {"#8229", 0x2025, "..", 0},    /* 2025 two dot leader */
    {"#8230", 0x2026, "...", 0},   /* 2026 ellipsis */
    {"#8231", 0x2027, "-", 0267},  /* 2027 hyphenation point */
    {"#8232", 0x2028, NULL, 0},    /* 2028 line separator */
    {"#8233", 0x2029, NULL, 0266}, /* 2029 paragraph separator */
    {"#8234", 0x202A, NULL, 0},    /* 202A left-to-right embedding */
    {"#8235", 0x202B, NULL, 0},    /* 202B right-to-left embedding */
    {"#8236", 0x202C, NULL, 0},    /* 202C pop directional formatting */
    {"#8237", 0x202D, NULL, 0},    /* 202D left-to-right override */
    {"#8238", 0x202E, NULL, 0},    /* 202E right-to-left override */
    {"#8239", 0x202F, NULL, 040},  /* 202F narrow no-break space */
    {"#8240", 0x2030, "%.", 0},    /* 2030 per mille */
    {"#8241", 0x2031, "%..", 0},   /* 2031 per ten thousand */
    {"#8242", 0x2032, NULL, 047},  /* 2032 prime */
    {"#8243", 0x2033, "\'\'", 0},  /* 2033 double prime */
    {"#8244", 0x2034, "\'\'\'", 0}, /* 2034 triple prime */
    {"#8245", 0x2035, NULL, 0140}, /* 2035 reversed prime */
    {"#8246", 0x2036, "``", 0},    /* 2036 reversed double prime */
    {"#8247", 0x2037, "```", 0},   /* 2037 reversed triple prime */
    {"#8248", 0x2038, NULL, 0136}, /* 2038 caret */
    {"#8249", 0x2039, NULL, 074},  /* 2039 single left angle quotation mark */
    {"#8250", 0x203A, NULL, 076},  /* 203A single right angle quotation mark  */
    {"#8251", 0x203B, NULL, 0},    /* 203B reference mark */
    {"#8252", 0x203C, "!!", 0},    /* 203C double exclamation mark */
    {"#8253", 0x203D, NULL, 041},  /* 203D interrobang */
    {"#8254", 0x203E, "-", 0257},  /* 203E overline */
    {"#8255", 0x203F, NULL, 0137}, /* 203F undertie */
    {"#8256", 0x2040, NULL, 0},    /* 2040 character tie */
    {"#8257", 0x2041, NULL, 0},    /* 2041 caret insertion point */
    {"#8258", 0x2042, NULL, 0},    /* 2042 asterism */
    {"#8259", 0x2043, NULL, 0},    /* 2043 hyphen bullet */
    {"#8260", 0x2044, NULL, 057},  /* 2044 fraction slash */
    {"#8261", 0x2045, NULL, 0133}, /* 2045 left square bracket w/quill */
    {"#8262", 0x2046, NULL, 0135}, /* 2046 right square bracket w/quill */
    {"#8263", 0x2047, "??", 0},    /* 2047 double question mark */
    {"#8264", 0x2048, "?!", 0},    /* 2048 question exclamation mark */
    {"#8265", 0x2049, "!?", 0},    /* 2049 exclamation question mark */
    {"#8266", 0x204A, NULL, 0},    /* 204A tironian sign et */
    {"#8267", 0x204B, NULL, 0},    /* 204B reverse pilcrow */
    {"#8268", 0x204C, NULL, 0},    /* 204C black left bullet */
    {"#8269", 0x204D, NULL, 0},    /* 204D black right bullet */
    {"#8270", 0x204E, NULL, 0},    /* 204E low asterisk */
    {"#8271", 0x204F, NULL, 0},    /* 204F reversed semicolon */
    {"#8272", 0x2050, NULL, 0},    /* 2050 close up */
    {"#8273", 0x2051, NULL, 0},    /* 2051 two vertical asterisks */
    {"#8274", 0x2052, NULL, 0},    /* 2052 commercial minus */
    {"#8275", 0x2053, NULL, 0},    /* 2053 reserved */
    {"#8276", 0x2054, NULL, 0},    /* 2054 reserved */
    {"#8277", 0x2055, NULL, 0},    /* 2055 reserved */
    {"#8278", 0x2056, NULL, 0},    /* 2056 reserved */
    {"#8279", 0x2057, "\'\'\'\'", 0}, /* 2057 quad prime */
    {"#8280", 0x2058, NULL, 0},    /* 2058 reserved */
    {"#8281", 0x2059, NULL, 0},    /* 2059 reserved */
    {"#8282", 0x205A, NULL, 0},    /* 205A reserved */
    {"#8283", 0x205B, NULL, 0},    /* 205B reserved */
    {"#8284", 0x205C, NULL, 0},    /* 205C reserved */
    {"#8285", 0x205D, NULL, 0},    /* 205D reserved */
    {"#8286", 0x205E, NULL, 0},    /* 205E reserved */
    {"#8287", 0x205F, NULL, 040},  /* 205F medium math space */
    {"#8288", 0x2060, NULL, 0},    /* 2060 word joiner */
    {"#8289", 0x2061, NULL, 0},    /* 2061 function application */
    {"#8290", 0x2062, NULL, 0},    /* 2062 invisible times */
    {"#8291", 0x2063, NULL, 0},    /* 2063 invisible separator */
    {"#8364", 0x20AC, "EUR", 0},   /* 20AC euro symbol */
    {"#8482", 0x2122, "[tm]", 0},  /* 2122 trademark symbol */
    {NULL,	0}
};


/*
 * Table of supported elements and corresponding handlers
 */
static struct element_table {
    char      *element;
    int	     (*handler) PROTO(());
} element_table[] = {
    {"HTML",		NULL},			/* HTML ignore if seen? */
    {"HEAD",		html_head},		/* slurp until <BODY> ? */
    {"TITLE",		html_title},		/* Document Title */
    {"BASE",		html_base},		/* HREF base */
    {"BODY",		NULL},			/* (NO OP) */
    {"A",		html_a},		/* Anchor */
    {"IMG",		html_img},		/* Image */
    {"HR",		html_hr},		/* Horizontal Rule */
    {"BR",		html_br},		/* Line Break */
    {"P",		html_p},		/* Paragraph */
    {"OL",		html_ol},		/* Ordered List */
    {"UL",		html_ul},		/* Unordered List */
    {"MENU",		html_menu},		/* Menu List */
    {"DIR",		html_dir},		/* Directory List */
    {"LI",		html_li},		/*  ... List Item */
    {"DL",		html_dl},		/* Definition List */
    {"DT",		html_dt},		/*  ... Def. Term */
    {"DD",		html_dd},		/*  ... Def. Definition */
    {"I",		html_i},		/* Italic Text */
    {"EM",		html_i},		/* Typographic Emphasis */
    {"STRONG",		html_i},		/* STRONG Typo Emphasis */
    {"VAR",		html_i},		/* Variable Name */
    {"B",		html_b},		/* Bold Text */
    {"BLOCKQUOTE", 	html_blockquote}, 	/* Blockquote */
    {"ADDRESS",		html_address},		/* Address */
    {"CENTER",		html_center},		/* Centered Text v3.2 */
    {"DIV",		html_div},		/* Document Division 3.2 */
    {"H1",		html_h1},		/* Headings... */
    {"H2",		html_h2},
    {"H3",		html_h3},
    {"H4",		html_h4},
    {"H5",		html_h5},
    {"H6",		html_h6},
    {"PRE",		html_pre},		/* Preformatted Text */
    {"KBD",		NULL},			/* Keyboard Input (NO OP) */
    {"TT",		NULL},			/* Typetype (NO OP) */
    {"SAMP",		NULL},			/* Sample Text (NO OP) */

/*----- Handlers below are NOT DONE OR CHECKED OUT YET -----*/

    {"CITE",		NULL},			/* Citation */
    {"CODE",		NULL},			/* Code Text */

/*----- Handlers below UNIMPLEMENTED (and won't until later) -----*/

    {"FORM",		html_form},		/* form within a document */
    {"INPUT",		NULL},			/* One input field, options */
    {"OPTION",		NULL},			/* One option within Select */
    {"SELECT",		NULL},			/* Selection from a set */
    {"TEXTAREA",	NULL},			/* A multi-line input field */
    {"SCRIPT",		html_script},		/* Embedded scripting statements */
    {"STYLE",		html_style},		/* Embedded CSS data */

/*----- Handlers below provide limited support for RFC 1942 Tables -----*/

    {"CAPTION",	html_center},		/* Table Caption */
    {"TR",		html_tr},		/* Table Table Row */
    {"TD",		html_td},		/* Table Table Data */

    {NULL,		NULL}
};



/*
 * Initialize the given handler, and add it to the stack if it
 * requests it.
 */
void
html_push(fd, hf)
    FILTER_S *fd;
    html_f    hf;
{
    HANDLER_S *new;

    new = (HANDLER_S *)fs_get(sizeof(HANDLER_S));
    memset(new, 0, sizeof(HANDLER_S));
    new->html_data = fd;
    new->f	   = hf;
    if((*hf)(new, 0, GF_RESET)){	/* stack the handler? */
	 new->below   = HANDLERS(fd);
	 HANDLERS(fd) = new;		/* push */
    }
    else
      fs_give((void **) &new);
}


/*
 * Remove the most recently installed the given handler
 * after letting it accept its demise.
 */
void
html_pop(fd, hf)
    FILTER_S *fd;
    html_f    hf;
{
    HANDLER_S *tp;

    for(tp = HANDLERS(fd); tp && hf != tp->f; tp = tp->below)
      ;

    if(tp){
	(*tp->f)(tp, 0, GF_EOD);	/* may adjust handler list */
	if(tp != HANDLERS(fd)){
	    HANDLER_S *p;

	    for(p = HANDLERS(fd); p->below != tp; p = p->below)
	      ;

	    if(p)
	      p->below = tp->below;	/* remove from middle of stack */
	    /* BUG: else programming botch and we should die */
	}
	else
	  HANDLERS(fd) = tp->below;	/* pop */

	fs_give((void **)&tp);
    }
    else if(hf == html_p || hf == html_li || hf == html_dt || hf == html_dd){
	/*
	 * Possible "special case" tag handling here.
	 * It's for such tags as Paragraph (`</P>'), List Item
	 * (`</LI>'), Definition Term (`</DT>'), and Definition Description
	 * (`</DD>') elements, which may be omitted...
	 */
	HANDLER_S hd;

	memset(&hd, 0, sizeof(HANDLER_S));
	hd.html_data = fd;
	hd.f	     = hf;

	(*hf)(&hd, 0, GF_EOD);
    }
    /* BUG: else, we should bitch */
}


/*
 * Deal with data passed a hander in its GF_DATA state
 */
static void
html_handoff(hd, ch)
    HANDLER_S *hd;
    int	       ch;
{
    if(hd->below)
      (*hd->below->f)(hd->below, ch, GF_DATA);
    else
      html_output(hd->html_data, ch);
}


/*
 * HTML <BR> element handler
 */
int
html_br(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET)
      html_output(hd->html_data, HTML_NEWLINE);

    return(0);				/* don't get linked */
}


/*
 * HTML <HR> (Horizontal Rule) element handler
 */
int
html_hr(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){
	int	   i, old_wrap, width, align;
	PARAMETER *p;

	width = WRAP_COLS(hd->html_data);
	align = 0;
	for(p = HD(hd->html_data)->el_data->attribs;
	    p && p->attribute;
	    p = p->next)
	  if(p->value){
	      if(!strucmp(p->attribute, "ALIGN")){
		  if(!strucmp(p->value, "LEFT"))
		    align = 1;
		  else if(!strucmp(p->value, "RIGHT"))
		    align = 2;
	      }
	      else if(!strucmp(p->attribute, "WIDTH")){
		  char *cp;

		  width = 0;
		  for(cp = p->value; *cp; cp++)
		    if(*cp == '%'){
			width = (WRAP_COLS(hd->html_data)*min(100,width))/100;
			break;
		    }
		    else if(isdigit((unsigned char) *cp))
		      width = (width * 10) + (*cp - '0');

		  width = min(width, WRAP_COLS(hd->html_data));
	      }
	  }

	html_blank(hd->html_data, 1);	/* at least one blank line */

	old_wrap = HD(hd->html_data)->wrapstate;
	HD(hd->html_data)->wrapstate = 0;
	if((i = max(0, WRAP_COLS(hd->html_data) - width))
	   && ((align == 0) ? i /= 2 : (align == 2)))
	  for(; i > 0; i--)
	    html_output(hd->html_data, ' ');

	for(i = 0; i < width; i++)
	  html_output(hd->html_data, '_');

	html_blank(hd->html_data, 1);
	HD(hd->html_data)->wrapstate = old_wrap;
    }

    return(0);				/* don't get linked */
}


/*
 * HTML <P> (paragraph) element handler
 */
int
html_p(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){
	/* Make sure there's at least 1 blank line */
	html_blank(hd->html_data, 1);

	/* adjust indent level if needed */
	if(HD(hd->html_data)->li_pending){
	    html_indent(hd->html_data, 4, HTML_ID_INC);
	    HD(hd->html_data)->li_pending = 0;
	}
    }
    else if(cmd == GF_EOD)
      /* Make sure there's at least 1 blank line */
      html_blank(hd->html_data, 1);

    return(0);				/* don't get linked */
}


/*
 * HTML Table <TR> (paragraph) table row
 */
int
html_tr(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET || cmd == GF_EOD)
      /* Make sure there's at least 1 blank line */
      html_blank(hd->html_data, 0);

    return(0);				/* don't get linked */
}


/*
 * HTML Table <TD> (paragraph) table data
 */
int
html_td(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){
	PARAMETER *p;

	for(p = HD(hd->html_data)->el_data->attribs;
	    p && p->attribute;
	    p = p->next)
	  if(!strucmp(p->attribute, "nowrap")
	     && (hd->html_data->f2 || hd->html_data->n)){
	      HTML_DUMP_LIT(hd->html_data, " | ", 3);
	      break;
	  }
    }

    return(0);				/* don't get linked */
}


/*
 * HTML <I> (italic text) element handler
 */
int
html_i(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	/* include LITERAL in spaceness test! */
	if(hd->x && !isspace((unsigned char) (ch & 0xff))){
	    HTML_ULINE(hd->html_data, 1);
	    hd->x = 0;
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	hd->x = 1;
    }
    else if(cmd == GF_EOD){
	if(!hd->x)
	  HTML_ULINE(hd->html_data, 0);
    }

    return(1);				/* get linked */
}


/*
 * HTML <b> (Bold text) element handler
 */
int
html_b(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	/* include LITERAL in spaceness test! */
	if(hd->x && !isspace((unsigned char) (ch & 0xff))){
	    HTML_ULINE(hd->html_data, 1);
	    hd->x = 0;
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	hd->x = 1;
    }
    else if(cmd == GF_EOD){
	if(!hd->x)
	  HTML_ULINE(hd->html_data, 0);
    }

    return(1);				/* get linked */
}


/*
 * HTML <IMG> element handler
 */
int
html_img(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){
	PARAMETER *p;
	char	  *s = NULL;

	for(p = HD(hd->html_data)->el_data->attribs;
	    p && p->attribute;
	    p = p->next)
	  if(!strucmp(p->attribute, "alt")){
	      if(p->value && p->value[0]){
		  HTML_DUMP_LIT(hd->html_data, p->value, strlen(p->value));
		  HTML_TEXT(hd->html_data, ' ');
		  return(0);
	      }
	  }

	for(p = HD(hd->html_data)->el_data->attribs;
	    p && p->attribute;
	    p = p->next)
	  if(!strucmp(p->attribute, "src") && p->value)
	    if((s = strrindex(p->value, '/')) && *++s != '\0'){
		HTML_TEXT(hd->html_data, '[');
		HTML_DUMP_LIT(hd->html_data, s, strlen(s));
		HTML_TEXT(hd->html_data, ']');
		HTML_TEXT(hd->html_data, ' ');
		return(0);
	    }

	HTML_DUMP_LIT(hd->html_data, "[IMAGE] ", 7);
    }

    return(0);				/* don't get linked */
}


/*
 * HTML <FORM> (Form) element handler
 */
int
html_form(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){

	html_blank(hd->html_data, 0);

	HTML_DUMP_LIT(hd->html_data, "[FORM]", 6);

	html_blank(hd->html_data, 0);
    }

    return(0);				/* don't get linked */
}


/*
 * HTML <HEAD> element handler
 */
int
html_head(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	HD(hd->html_data)->head = 1;
    }
    else if(cmd == GF_EOD){
	HD(hd->html_data)->head = 0;
    }

    return(1);				/* get linked */
}


/*
 * HTML <BASE> element handler
 */
int
html_base(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){
	if(HD(hd->html_data)->head && !HTML_BASE(hd->html_data)){
	    PARAMETER *p;

	    for(p = HD(hd->html_data)->el_data->attribs;
		p && p->attribute && strucmp(p->attribute, "HREF");
		p = p->next)
	      ;

	    if(p && p->value && !((HTML_OPT_S *)(hd->html_data)->opt)->base)
	      ((HTML_OPT_S *)(hd->html_data)->opt)->base = cpystr(p->value);
	}
    }

    return(0);				/* DON'T get linked */
}


/*
 * HTML <TITLE> element handler
 */
int
html_title(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	if(hd->x + 1 >= hd->y){
	    hd->y += 80;
	    fs_resize((void **)&hd->s, (size_t)hd->y * sizeof(unsigned char));
	}

	hd->s[hd->x++] = (unsigned char) ch;
    }
    else if(cmd == GF_RESET){
	hd->x = 0L;
	hd->y = 80L;
	hd->s = (unsigned char *)fs_get((size_t)hd->y * sizeof(unsigned char));
    }
    else if(cmd == GF_EOD){
	/* Down the road we probably want to give these bytes to
	 * someone...
	 */
	hd->s[hd->x] = '\0';
	fs_give((void **)&hd->s);
    }

    return(1);				/* get linked */
}


/*
 * HTML <A> (Anchor) element handler
 */
int
html_a(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	int	   i, n, x;
	char	   buf[256];
	HANDLE_S  *h;
	PARAMETER *p, *href = NULL, *name = NULL;

	/*
	 * Pending Anchor!?!?
	 * space insertion/line breaking that's yet to get done...
	 */
	if(HD(hd->html_data)->prefix){
	    dprint(1, (debugfile, "-- html_a: NESTED/UNTERMINATED ANCHOR!\n"));
	    html_a_finish(hd);
	}

	/*
	 * Look for valid Anchor data vis the filter installer's parms
	 * (e.g., Only allow references to our internal URLs if asked)
	 */
	for(p = HD(hd->html_data)->el_data->attribs;
	    p && p->attribute;
	    p = p->next)
	  if(!strucmp(p->attribute, "HREF")
	     && p->value
	     && (HANDLES_LOC(hd->html_data)
		 || struncmp(p->value, "x-pine-", 7)))
	    href = p;
	  else if(!strucmp(p->attribute, "NAME"))
	    name = p;

	if(DO_HANDLES(hd->html_data) && (href || name)){
	    h = new_handle(HANDLESP(hd->html_data));

	    /*
	     * Enhancement: we might want to get fancier and parse the
	     * href a bit further such that we can launch images using
	     * our image viewer, or browse local files or directories
	     * with our internal tools.  Of course, having the jump-off
	     * point into text/html always be the defined "web-browser",
	     * just might be the least confusing UI-wise...
	     */
	    h->type = URL;

	    if(name && name->value)
	      h->h.url.name = cpystr(name->value);

	    /*
	     * Prepare to build embedded prefix...
	     */
	    HD(hd->html_data)->prefix = (int *) fs_get(64 * sizeof(int));
	    x = 0;

	    /*
	     * Is this something that looks like a URL?  If not and
	     * we were giving some "base" string, proceed ala RFC1808...
	     */
	    if(href){
		if(HTML_BASE(hd->html_data) && !rfc1738_scan(href->value, &n))
		  html_a_relative(HTML_BASE(hd->html_data), href->value, h);
		else
		  h->h.url.path = cpystr(href->value);

		if(pico_usingcolor()){
		    char *fg = NULL, *bg = NULL, *q;

		    if(ps_global->VAR_SLCTBL_FORE_COLOR
		       && colorcmp(ps_global->VAR_SLCTBL_FORE_COLOR,
				   ps_global->VAR_NORM_FORE_COLOR))
		      fg = ps_global->VAR_SLCTBL_FORE_COLOR;

		    if(ps_global->VAR_SLCTBL_BACK_COLOR
		       && colorcmp(ps_global->VAR_SLCTBL_BACK_COLOR,
				   ps_global->VAR_NORM_BACK_COLOR))
		      bg = ps_global->VAR_SLCTBL_BACK_COLOR;

		    if(fg || bg){
			COLOR_PAIR *tmp;

			/*
			 * The blacks are just known good colors for testing
			 * whether the other color is good.
			 */
			tmp = new_color_pair(fg ? fg : colorx(COL_BLACK),
					     bg ? bg : colorx(COL_BLACK));
			if(pico_is_good_colorpair(tmp)){
			    q = color_embed(fg, bg);

			    for(i = 0; q[i]; i++)
			      HD(hd->html_data)->prefix[x++] = q[i];
			}

			if(tmp)
			  free_color_pair(&tmp);
		    }

		    if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global))
		      HD(hd->html_data)->prefix[x++] = HTML_DOBOLD;
		}
		else
		  HD(hd->html_data)->prefix[x++] = HTML_DOBOLD;
	    }

	    HD(hd->html_data)->prefix[x++] = TAG_EMBED;
	    HD(hd->html_data)->prefix[x++] = TAG_HANDLE;

	    sprintf(buf, "%d", h->key);
	    HD(hd->html_data)->prefix[x++] = n = strlen(buf);
	    for(i = 0; i < n; i++)
	      HD(hd->html_data)->prefix[x++] = buf[i];

	    HD(hd->html_data)->prefix_used = x;
	}
    }
    else if(cmd == GF_EOD){
	html_a_finish(hd);
    }

    return(1);				/* get linked */
}


void
html_a_prefix(f)
    FILTER_S *f;
{
    int *prefix, n;

    /* Do this so we don't visit from html_output... */
    prefix = HD(f)->prefix;
    HD(f)->prefix = NULL;

    for(n = 0; n < HD(f)->prefix_used; n++)
      html_a_output_prefix(f, prefix[n]);

    fs_give((void **) &prefix);
}


/*
 * html_a_finish - house keeping associated with end of link tag
 */
void
html_a_finish(hd)
    HANDLER_S *hd;
{
    if(DO_HANDLES(hd->html_data)){
	if(HD(hd->html_data)->prefix){
	    char *empty_link = "[LINK]";
	    int   i;

	    html_a_prefix(hd->html_data);
	    for(i = 0; empty_link[i]; i++)
	      html_output(hd->html_data, empty_link[i]);
	}

	if(pico_usingcolor()){
	    char *fg = NULL, *bg = NULL, *p;
	    int   i;

	    if(ps_global->VAR_SLCTBL_FORE_COLOR
	       && colorcmp(ps_global->VAR_SLCTBL_FORE_COLOR,
			   ps_global->VAR_NORM_FORE_COLOR))
	      fg = ps_global->VAR_NORM_FORE_COLOR;

	    if(ps_global->VAR_SLCTBL_BACK_COLOR
	       && colorcmp(ps_global->VAR_SLCTBL_BACK_COLOR,
			   ps_global->VAR_NORM_BACK_COLOR))
	      bg = ps_global->VAR_NORM_BACK_COLOR;

	    if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global))
	      HTML_BOLD(hd->html_data, 0);	/* turn OFF bold */

	    if(fg || bg){
		COLOR_PAIR *tmp;

		/*
		 * The blacks are just known good colors for testing
		 * whether the other color is good.
		 */
		tmp = new_color_pair(fg ? fg : colorx(COL_BLACK),
				     bg ? bg : colorx(COL_BLACK));
		if(pico_is_good_colorpair(tmp)){
		    p = color_embed(fg, bg);

		    for(i = 0; p[i]; i++)
		      html_output(hd->html_data, p[i]);
		}

		if(tmp)
		  free_color_pair(&tmp);
	    }
	}
	else
	  HTML_BOLD(hd->html_data, 0);	/* turn OFF bold */

	html_output(hd->html_data, TAG_EMBED);
	html_output(hd->html_data, TAG_HANDLEOFF);
    }
}


/*
 * html_output_a_prefix - dump Anchor prefix data
 */
void
html_a_output_prefix(f, c)
    FILTER_S *f;
    int	      c;
{
    switch(c){
      case HTML_DOBOLD :
	HTML_BOLD(f, 1);
	break;

      default :
	html_output(f, c);
	break;
    }
}



/*
 * relative_url - put full url path in h based on base and relative url
 */
void
html_a_relative(base_url, rel_url, h)
    char     *base_url, *rel_url;
    HANDLE_S *h;
{
    size_t  len;
    char    tmp[MAILTMPLEN], *p, *q;
    char   *scheme = NULL, *net = NULL, *path = NULL,
	   *parms = NULL, *query = NULL, *frag = NULL,
	   *base_scheme = NULL, *base_net_loc = NULL,
	   *base_path = NULL, *base_parms = NULL,
	   *base_query = NULL, *base_frag = NULL,
	   *rel_scheme = NULL, *rel_net_loc = NULL,
	   *rel_path = NULL, *rel_parms = NULL,
	   *rel_query = NULL, *rel_frag = NULL;

    /* Rough parse of base URL */
    rfc1808_tokens(base_url, &base_scheme, &base_net_loc, &base_path,
		   &base_parms, &base_query, &base_frag);

    /* Rough parse of this URL */
    rfc1808_tokens(rel_url, &rel_scheme, &rel_net_loc, &rel_path,
		   &rel_parms, &rel_query, &rel_frag);

    scheme = rel_scheme;	/* defaults */
    net    = rel_net_loc;
    path   = rel_path;
    parms  = rel_parms;
    query  = rel_query;
    frag   = rel_frag;
    if(!scheme && base_scheme){
	scheme = base_scheme;
	if(!net){
	    net = base_net_loc;
	    if(path){
		if(*path != '/'){
		    if(base_path){
			for(p = q = base_path;	/* Drop base path's tail */
			    p = strchr(p, '/');
			    q = ++p)
			  ;

			len = q - base_path;
		    }
		    else
		      len = 0;

		    if(len + strlen(rel_path) < sizeof(tmp)-1){
			if(len)
			  sprintf(path = tmp, "%.*s", len, base_path);

			strcpy(tmp + len, rel_path);

			/* Follow RFC 1808 "Step 6" */
			for(p = tmp; p = strchr(p, '.'); )
			  switch(*(p+1)){
			      /*
			       * a) All occurrences of "./", where "." is a
			       *    complete path segment, are removed.
			       */
			    case '/' :
			      if(p > tmp)
				for(q = p; *q = *(q+2); q++)
				  ;
			      else
				p++;

			      break;

			      /*
			       * b) If the path ends with "." as a
			       *    complete path segment, that "." is
			       *    removed.
			       */
			    case '\0' :
			      if(p == tmp || *(p-1) == '/')
				*p = '\0';
			      else
				p++;

			      break;

			      /*
			       * c) All occurrences of "<segment>/../",
			       *    where <segment> is a complete path
			       *    segment not equal to "..", are removed.
			       *    Removal of these path segments is
			       *    performed iteratively, removing the
			       *    leftmost matching pattern on each
			       *    iteration, until no matching pattern
			       *    remains.
			       *
			       * d) If the path ends with "<segment>/..",
			       *    where <segment> is a complete path
			       *    segment not equal to "..", that
			       *    "<segment>/.." is removed.
			       */
			    case '.' :
			      if(p > tmp + 1){
				  for(q = p - 2; q > tmp && *q != '/'; q--)
				    ;

				  if(*q == '/')
				    q++;

				  if(q + 1 == p		/* no "//.." */
				     || (*q == '.'	/* and "../.." */
					 && *(q+1) == '.'
					 && *(q+2) == '/')){
				      p += 2;
				      break;
				  }

				  switch(*(p+2)){
				    case '/' :
				      len = (p - q) + 3;
				      p = q;
				      for(; *q = *(q+len); q++)
					;

				      break;

				    case '\0':
				      *(p = q) = '\0';
				      break;

				    default:
				      p += 2;
				      break;
				  }
			      }
			      else
				p += 2;

			      break;

			    default :
			      p++;
			      break;
			  }
		    }
		    else
		      path = "";		/* lame. */
		}
	    }
	    else{
		path = base_path;
		if(!parms){
		    parms = base_parms;
		    if(!query)
		      query = base_query;
		}
	    }
	}
    }

    len = (scheme ? strlen(scheme) : 0) + (net ? strlen(net) : 0)
	  + (path ? strlen(path) : 0) + (parms ? strlen(parms) : 0)
	  + (query ? strlen(query) : 0) + (frag  ? strlen(frag ) : 0) + 8;

    h->h.url.path = (char *) fs_get(len * sizeof(char));
    sprintf(h->h.url.path, "%s%s%s%s%s%s%s%s%s%s%s%s",
	    scheme ? scheme : "", scheme ? ":" : "",
	    net ? "//" : "", net ? net : "",
	    (path && *path == '/') ? "" : ((path && net) ? "/" : ""),
	    path ? path : "",
	    parms ? ";" : "", parms ? parms : "",
	    query ? "?" : "", query ? query : "",
	    frag ? "#" : "", frag ? frag : "");

    if(base_scheme)
      fs_give((void **) &base_scheme);

    if(base_net_loc)
      fs_give((void **) &base_net_loc);

    if(base_path)
      fs_give((void **) &base_path);

    if(base_parms)
      fs_give((void **) &base_parms);

    if(base_query)
      fs_give((void **) &base_query);

    if(base_frag)
      fs_give((void **) &base_frag);

    if(rel_scheme)
      fs_give((void **) &rel_scheme);

    if(rel_net_loc)
      fs_give((void **) &rel_net_loc);

    if(rel_parms)
      fs_give((void **) &rel_parms);

    if(rel_query)
      fs_give((void **) &rel_query);

    if(rel_frag)
      fs_give((void **) &rel_frag);

    if(rel_path)
      fs_give((void **) &rel_path);
}


/*
 * HTML <UL> (Unordered List) element handler
 */
int
html_ul(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	HD(hd->html_data)->li_pending = 1;
	html_blank(hd->html_data, 0);
    }
    else if(cmd == GF_EOD){
	html_blank(hd->html_data, 0);

	if(!HD(hd->html_data)->li_pending)
	  html_indent(hd->html_data, -4, HTML_ID_INC);
	else
	  HD(hd->html_data)->li_pending = 0;
    }

    return(1);				/* get linked */
}


/*
 * HTML <OL> (Ordered List) element handler
 */
int
html_ol(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * Signal that we're expecting to see <LI> as our next elemnt
	 * and set the the initial ordered count.
	 */
	HD(hd->html_data)->li_pending = 1;
	hd->x = 1L;
	html_blank(hd->html_data, 0);
    }
    else if(cmd == GF_EOD){
	html_blank(hd->html_data, 0);

	if(!HD(hd->html_data)->li_pending)
	  html_indent(hd->html_data, -4, HTML_ID_INC);
	else
	  HD(hd->html_data)->li_pending = 0;
    }

    return(1);				/* get linked */
}


/*
 * HTML <MENU> (Menu List) element handler
 */
int
html_menu(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	HD(hd->html_data)->li_pending = 1;
    }
    else if(cmd == GF_EOD){
	html_blank(hd->html_data, 0);

	if(!HD(hd->html_data)->li_pending)
	  html_indent(hd->html_data, -4, HTML_ID_INC);
	else
	  HD(hd->html_data)->li_pending = 0;
    }

    return(1);				/* get linked */
}


/*
 * HTML <DIR> (Directory List) element handler
 */
int
html_dir(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	HD(hd->html_data)->li_pending = 1;
    }
    else if(cmd == GF_EOD){
	html_blank(hd->html_data, 0);

	if(!HD(hd->html_data)->li_pending)
	  html_indent(hd->html_data, -4, HTML_ID_INC);
	else
	  HD(hd->html_data)->li_pending = 0;
    }

    return(1);				/* get linked */
}


/*
 * HTML <LI> (List Item) element handler
 */
int
html_li(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){
	HANDLER_S *p, *found = NULL;

	/*
	 * There better be a an unordered list, ordered list,
	 * Menu or Directory handler installed
	 * or else we crap out...
	 */
	for(p = HANDLERS(hd->html_data); p; p = p->below)
	  if(p->f == html_ul || p->f == html_ol
	     || p->f == html_menu || p->f == html_dir){
	      found = p;
	      break;
	  }

	if(found){
	    char buf[8], *p;
	    int  wrapstate;

	    /* Start a new line */
	    html_blank(hd->html_data, 0);

	    /* adjust indent level if needed */
	    if(HD(hd->html_data)->li_pending){
		html_indent(hd->html_data, 4, HTML_ID_INC);
		HD(hd->html_data)->li_pending = 0;
	    }

	    if(found->f == html_ul){
		int l = html_indent(hd->html_data, 0, HTML_ID_GET);

		strcpy(buf, "   ");
		buf[1] = (l < 5) ? '*' : (l < 9) ? '+' : (l < 17) ? 'o' : '#';
	    }
	    else if(found->f == html_ol)
	      sprintf(buf, "%2ld.", found->x++);
	    else if(found->f == html_menu)
	      strcpy(buf, " ->");

	    html_indent(hd->html_data, -4, HTML_ID_INC);

	    /* So we don't munge whitespace */
	    wrapstate = HD(hd->html_data)->wrapstate;
	    HD(hd->html_data)->wrapstate = 0;

	    html_write_indent(hd->html_data, HD(hd->html_data)->indent_level);
	    for(p = buf; *p; p++)
	      html_output(hd->html_data, (int) *p);

	    HD(hd->html_data)->wrapstate = wrapstate;
	    html_indent(hd->html_data, 4, HTML_ID_INC);
	}
	/* BUG: should really bitch about this */
    }

    return(0);				/* DON'T get linked */
}



/*
 * HTML <DL> (Definition List) element handler
 */
int
html_dl(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * Set indention level for definition terms and definitions...
	 */
	hd->x = html_indent(hd->html_data, 0, HTML_ID_GET);
	hd->y = hd->x + 2;
	hd->z = hd->y + 4;
    }
    else if(cmd == GF_EOD){
	html_indent(hd->html_data, (int) hd->x, HTML_ID_SET);
	html_blank(hd->html_data, 1);
    }

    return(1);				/* get linked */
}


/*
 * HTML <DT> (Definition Term) element handler
 */
int
html_dt(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){
	HANDLER_S *p;

	/*
	 * There better be a Definition Handler installed
	 * or else we crap out...
	 */
	for(p = HANDLERS(hd->html_data); p && p->f != html_dl; p = p->below)
	  ;

	if(p){				/* adjust indent level if needed */
	    html_indent(hd->html_data, (int) p->y, HTML_ID_SET);
	    html_blank(hd->html_data, 1);
	}
	/* BUG: else should really bitch about this */
    }

    return(0);				/* DON'T get linked */
}


/*
 * HTML <DD> (Definition Definition) element handler
 */
int
html_dd(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_RESET){
	HANDLER_S *p;

	/*
	 * There better be a Definition Handler installed
	 * or else we crap out...
	 */
	for(p = HANDLERS(hd->html_data); p && p->f != html_dl; p = p->below)
	  ;

	if(p){				/* adjust indent level if needed */
	    html_indent(hd->html_data, (int) p->z, HTML_ID_SET);
	    html_blank(hd->html_data, 0);
	}
	/* BUG: should really bitch about this */
    }

    return(0);				/* DON'T get linked */
}


/*
 * HTML <H1> (Headings 1) element handler.
 *
 * Bold, very-large font, CENTERED. One or two blank lines
 * above and below.  For our silly character cell's that
 * means centered and ALL CAPS...
 */
int
html_h1(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/* turn ON the centered bit */
	CENTER_BIT(hd->html_data) = 1;
    }
    else if(cmd == GF_EOD){
	/* turn OFF the centered bit, add blank line */
	CENTER_BIT(hd->html_data) = 0;
	html_blank(hd->html_data, 1);
    }

    return(1);				/* get linked */
}


/*
 * HTML <H2> (Headings 2) element handler
 */
int
html_h2(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	if((hd->x & HTML_HX_ULINE) && !isspace((unsigned char) (ch & 0xff))){
	    HTML_ULINE(hd->html_data, 1);
	    hd->x ^= HTML_HX_ULINE;	/* only once! */
	}

	html_handoff(hd, (ch < 128 && islower((unsigned char) ch))
			   ? toupper((unsigned char) ch) : ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * Bold, large font, flush-left. One or two blank lines
	 * above and below.
	 */
	if(CENTER_BIT(hd->html_data)) /* stop centering for now */
	  hd->x = HTML_HX_CENTER;
	else
	  hd->x = 0;

	hd->x |= HTML_HX_ULINE;
	    
	CENTER_BIT(hd->html_data) = 0;
	hd->y = html_indent(hd->html_data, 0, HTML_ID_SET);
	hd->z = HD(hd->html_data)->wrapcol;
	HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	html_blank(hd->html_data, 1);
    }
    else if(cmd == GF_EOD){
	/*
	 * restore previous centering, and indent level
	 */
	if(!(hd->x & HTML_HX_ULINE))
	  HTML_ULINE(hd->html_data, 0);

	html_indent(hd->html_data, hd->y, HTML_ID_SET);
	html_blank(hd->html_data, 1);
	CENTER_BIT(hd->html_data)  = (hd->x & HTML_HX_CENTER) != 0;
	HD(hd->html_data)->wrapcol = hd->z;
    }

    return(1);				/* get linked */
}


/*
 * HTML <H3> (Headings 3) element handler
 */
int
html_h3(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	if((hd->x & HTML_HX_ULINE) && !isspace((unsigned char) (ch & 0xff))){
	    HTML_ULINE(hd->html_data, 1);
	    hd->x ^= HTML_HX_ULINE;	/* only once! */
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * Italic, large font, slightly indented from the left
	 * margin. One or two blank lines above and below.
	 */
	if(CENTER_BIT(hd->html_data)) /* stop centering for now */
	  hd->x = HTML_HX_CENTER;
	else
	  hd->x = 0;

	hd->x |= HTML_HX_ULINE;
	CENTER_BIT(hd->html_data) = 0;
	hd->y = html_indent(hd->html_data, 2, HTML_ID_SET);
	hd->z = HD(hd->html_data)->wrapcol;
	HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	html_blank(hd->html_data, 1);
    }
    else if(cmd == GF_EOD){
	/*
	 * restore previous centering, and indent level
	 */
	if(!(hd->x & HTML_HX_ULINE))
	  HTML_ULINE(hd->html_data, 0);

	html_indent(hd->html_data, hd->y, HTML_ID_SET);
	html_blank(hd->html_data, 1);
	CENTER_BIT(hd->html_data)  = (hd->x & HTML_HX_CENTER) != 0;
	HD(hd->html_data)->wrapcol = hd->z;
    }

    return(1);				/* get linked */
}


/*
 * HTML <H4> (Headings 4) element handler
 */
int
html_h4(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * Bold, normal font, indented more than H3. One blank line
	 * above and below.
	 */
	hd->x = CENTER_BIT(hd->html_data); /* stop centering for now */
	CENTER_BIT(hd->html_data) = 0;
	hd->y = html_indent(hd->html_data, 4, HTML_ID_SET);
	hd->z = HD(hd->html_data)->wrapcol;
	HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	html_blank(hd->html_data, 1);
    }
    else if(cmd == GF_EOD){
	/*
	 * restore previous centering, and indent level
	 */
	html_indent(hd->html_data, (int) hd->y, HTML_ID_SET);
	html_blank(hd->html_data, 1);
	CENTER_BIT(hd->html_data)  = hd->x;
	HD(hd->html_data)->wrapcol = hd->z;
    }

    return(1);				/* get linked */
}


/*
 * HTML <H5> (Headings 5) element handler
 */
int
html_h5(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * Italic, normal font, indented as H4. One blank line
	 * above.
	 */
	hd->x = CENTER_BIT(hd->html_data); /* stop centering for now */
	CENTER_BIT(hd->html_data) = 0;
	hd->y = html_indent(hd->html_data, 6, HTML_ID_SET);
	hd->z = HD(hd->html_data)->wrapcol;
	HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	html_blank(hd->html_data, 1);
    }
    else if(cmd == GF_EOD){
	/*
	 * restore previous centering, and indent level
	 */
	html_indent(hd->html_data, (int) hd->y, HTML_ID_SET);
	html_blank(hd->html_data, 1);
	CENTER_BIT(hd->html_data)  = hd->x;
	HD(hd->html_data)->wrapcol = hd->z;
    }

    return(1);				/* get linked */
}


/*
 * HTML <H6> (Headings 6) element handler
 */
int
html_h6(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * Bold, indented same as normal text, more than H5. One
	 * blank line above.
	 */
	hd->x = CENTER_BIT(hd->html_data); /* stop centering for now */
	CENTER_BIT(hd->html_data) = 0;
	hd->y = html_indent(hd->html_data, 8, HTML_ID_SET);
	hd->z = HD(hd->html_data)->wrapcol;
	HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	html_blank(hd->html_data, 1);
    }
    else if(cmd == GF_EOD){
	/*
	 * restore previous centering, and indent level
	 */
	html_indent(hd->html_data, (int) hd->y, HTML_ID_SET);
	html_blank(hd->html_data, 1);
	CENTER_BIT(hd->html_data)  = hd->x;
	HD(hd->html_data)->wrapcol = hd->z;
    }

    return(1);				/* get linked */
}


/*
 * HTML <BlockQuote> element handler
 */
int
html_blockquote(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    int	 j;
#define	HTML_BQ_INDENT	6

    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * A typical rendering might be a slight extra left and
	 * right indent, and/or italic font. The Blockquote element
	 * causes a paragraph break, and typically provides space
	 * above and below the quote.
	 */
	html_indent(hd->html_data, HTML_BQ_INDENT, HTML_ID_INC);
	j = HD(hd->html_data)->wrapstate;
	HD(hd->html_data)->wrapstate = 0;
	html_blank(hd->html_data, 1);
	HD(hd->html_data)->wrapstate = j;
	HD(hd->html_data)->wrapcol  -= HTML_BQ_INDENT;
    }
    else if(cmd == GF_EOD){
	html_blank(hd->html_data, 1);

	j = HD(hd->html_data)->wrapstate;
	HD(hd->html_data)->wrapstate = 0;
	html_indent(hd->html_data, -(HTML_BQ_INDENT), HTML_ID_INC);
	HD(hd->html_data)->wrapstate = j;
	HD(hd->html_data)->wrapcol  += HTML_BQ_INDENT;
    }

    return(1);				/* get linked */
}


/*
 * HTML <Address> element handler
 */
int
html_address(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    int	 j;
#define	HTML_ADD_INDENT	2

    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/*
	 * A typical rendering might be a slight extra left and
	 * right indent, and/or italic font. The Blockquote element
	 * causes a paragraph break, and typically provides space
	 * above and below the quote.
	 */
	html_indent(hd->html_data, HTML_ADD_INDENT, HTML_ID_INC);
	j = HD(hd->html_data)->wrapstate;
	HD(hd->html_data)->wrapstate = 0;
	html_blank(hd->html_data, 1);
	HD(hd->html_data)->wrapstate = j;
    }
    else if(cmd == GF_EOD){
	html_blank(hd->html_data, 1);

	j = HD(hd->html_data)->wrapstate;
	HD(hd->html_data)->wrapstate = 0;
	html_indent(hd->html_data, -(HTML_ADD_INDENT), HTML_ID_INC);
	HD(hd->html_data)->wrapstate = j;
    }

    return(1);				/* get linked */
}


/*
 * HTML <PRE> (Preformatted Text) element handler
 */
int
html_pre(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	/*
	 * remove CRLF after '>' in element.
	 * We see CRLF because wrapstate is off.
	 */
	switch(hd->y){
	  case 2 :
	    if(ch == '\012'){
		hd->y = 3;
		return(1);
	    }
	    else
	      html_handoff(hd, '\015');

	    break;

	  case 1 :
	    if(ch == '\015'){
		hd->y = 2;
		return(1);
	    }

	  default :
	    hd->y = 0;
	    break;
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	html_blank(hd->html_data, 1);
	hd->x = HD(hd->html_data)->wrapstate;
	HD(hd->html_data)->wrapstate = 0;
	hd->y = 1;
    }
    else if(cmd == GF_EOD){
	HD(hd->html_data)->wrapstate = (hd->x != 0);
	html_blank(hd->html_data, 0);
    }

    return(1);
}




/*
 * HTML <CENTER> (Centerd Text) element handler
 */
int
html_center(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	/* turn ON the centered bit */
	CENTER_BIT(hd->html_data) = 1;
    }
    else if(cmd == GF_EOD){
	/* turn OFF the centered bit */
	CENTER_BIT(hd->html_data) = 0;
    }

    return(1);
}



/*
 * HTML <DIV> (Document Divisions) element handler
 */
int
html_div(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	PARAMETER *p;

	for(p = HD(hd->html_data)->el_data->attribs;
	    p && p->attribute;
	    p = p->next)
	  if(!strucmp(p->attribute, "ALIGN")){
	      if(p->value){
		  /* remember previous values */
		  hd->x = CENTER_BIT(hd->html_data);
		  hd->y = html_indent(hd->html_data, 0, HTML_ID_GET);

		  html_blank(hd->html_data, 0);
		  CENTER_BIT(hd->html_data) = !strucmp(p->value, "CENTER");
		  html_indent(hd->html_data, 0, HTML_ID_SET);
		  /* NOTE: "RIGHT" not supported yet */
	      }
	  }
    }
    else if(cmd == GF_EOD){
	/* restore centered bit and indentiousness */
	CENTER_BIT(hd->html_data) = hd->y;
	html_indent(hd->html_data, hd->y, HTML_ID_SET);
	html_blank(hd->html_data, 0);
    }

    return(1);
}



/*
 * HTML <SCRIPT> (Inline Script) element handler
 */
int
html_script(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    /* ignore everything */
    return(1);
}



/*
 * HTML <STYLE> (Embedded CSS data) element handler
 */
int
html_style(hd, ch, cmd)
    HANDLER_S *hd;
    int	       ch, cmd;
{
    /* ignore everything */
    return(1);
}



/*
 * return the function associated with the given element name
 */
html_f
html_element_func(el_name)
    char *el_name;
{
    register int i;

    for(i = 0; element_table[i].element; i++)
      if(!strucmp(el_name, element_table[i].element))
	return(element_table[i].handler);

    return(NULL);
}


/*
 * collect element's name and any attribute/value pairs then
 * dispatch to the appropriate handler.
 *
 * Returns 1 : got what we wanted
 *	   0 : we need more data
 *	  -1 : bogus input
 */
int
html_element_collector(fd, ch)
    FILTER_S *fd;
    int	      ch;
{
    if(ch == '>'){
	if(ED(fd)->overrun){
	    /*
	     * If problem processing, don't bother doing anything
	     * internally, just return such that none of what we've
	     * digested is displayed.
	     */
	    HTML_DEBUG_EL("too long", ED(fd));
	    return(1);			/* Let it go, Jim */
	}
	else if(ED(fd)->mkup_decl){
	    if(ED(fd)->badform){
		dprint(2, (debugfile, "-- html <!-- BAD: %.*s\n",
			   ED(fd)->len, ED(fd)->buf ? ED(fd)->buf : "?"));
		/*
		 * Invalid comment -- make some guesses as
		 * to whether we should stop with this greater-than...
		 */
		if(ED(fd)->buf[0] != '-'
		   || ED(fd)->len < 4
		   || (ED(fd)->buf[1] == '-'
		       && ED(fd)->buf[ED(fd)->len - 1] == '-'
		       && ED(fd)->buf[ED(fd)->len - 2] == '-'))
		  return(1);
	    }
	    else{
		dprint(5, (debugfile, "-- html <!-- OK: %.*s\n",
			   ED(fd)->len, ED(fd)->buf ? ED(fd)->buf : "?"));
		if(ED(fd)->start_comment == ED(fd)->end_comment){
		    if(ED(fd)->len > 10){
			ED(fd)->buf[ED(fd)->len - 2] = '\0';
			html_element_comment(fd, ED(fd)->buf + 2);
		    }

		    return(1);
		}
		/* else keep collecting comment below */
	    }
	}
	else if(!ED(fd)->quoted || ED(fd)->badform){
	    html_f f;

	    /*
	     * We either have the whole thing or all that we could
	     * salvage from it.  Try our best...
	     */

	    if(HD(fd)->bitbucket)
	      return(1);		/* element inside chtml clause! */

	    if(!ED(fd)->badform && html_element_flush(ED(fd)))
	      return(1);		/* return without display... */

	    /*
	     * If we ran into an empty tag or we don't know how to deal
	     * with it, just go on, ignoring it...
	     */
	    if(ED(fd)->element && (f = html_element_func(ED(fd)->element))){
		/* dispatch the element's handler */
		if(ED(fd)->end_tag)
		  html_pop(fd, f);	/* remove it's handler */
		else
		  html_push(fd, f);	/* add it's handler */

		HTML_DEBUG_EL(ED(fd)->end_tag ? "POP" : "PUSH", ED(fd));
	    }
	    else{			/* else, empty or unrecognized */
		HTML_DEBUG_EL("?", ED(fd));
	    }

	    return(1);			/* all done! see, that didn't hurt */
	}
    }

    if(ED(fd)->mkup_decl){
	if((ch &= 0xff) == '-'){
	    if(ED(fd)->hyphen){
		ED(fd)->hyphen = 0;
		if(ED(fd)->start_comment)
		  ED(fd)->end_comment = 1;
		else
		  ED(fd)->start_comment = 1;
	    }
	    else
	      ED(fd)->hyphen = 1;
	}
	else{
	    if(ED(fd)->end_comment)
	      ED(fd)->start_comment = ED(fd)->end_comment = 0;

	    /*
	     * no "--" after ! or non-whitespace between comments - bad
	     */
	    if(ED(fd)->len < 2 || (!ED(fd)->start_comment
				   && !isspace((unsigned char) ch)))
	      ED(fd)->badform = 1;	/* non-comment! */

	    ED(fd)->hyphen = 0;
	}

	/*
	 * Remember the comment for possible later processing, if
	 * it get's too long, remember first and last few chars
	 * so we know when to terminate (and throw some garbage
	 * in between when we toss out what's between.
	 */
	if(ED(fd)->len == HTML_BUF_LEN){
	    ED(fd)->buf[2] = ED(fd)->buf[3] = 'X';
	    ED(fd)->buf[4] = ED(fd)->buf[ED(fd)->len - 2];
	    ED(fd)->buf[5] = ED(fd)->buf[ED(fd)->len - 1];
	    ED(fd)->len    = 6;
	}

	ED(fd)->buf[(ED(fd)->len)++] = ch;
	return(0);			/* comments go in the bit bucket */
    }
    else if(ED(fd)->overrun || ED(fd)->badform){
	return(0);			/* swallow char's until next '>' */
    }
    else if(!ED(fd)->element && !ED(fd)->len){
	if(ch == '/'){		/* validate leading chars */
	    ED(fd)->end_tag = 1;
	    return(0);
	}
	else if(ch == '!'){
	    ED(fd)->mkup_decl = 1;
	    return(0);
	}
	else if(!isalpha((unsigned char) ch))
	  return(-1);			/* can't be a tag! */
    }
    else if(ch == '\"' || ch == '\''){
	if(!ED(fd)->hit_equal){
	    ED(fd)->badform = 1;	/* quote in element name?!? */
	    return(0);
	}

	if(ED(fd)->quoted){
	    if(ED(fd)->quoted == (char) ch){
		ED(fd)->quoted = 0;
		return(0);		/* continue collecting chars */
	    }
	    /* else fall thru writing other quoting char */
	}
	else{
	    ED(fd)->quoted = (char) ch;
	    return(0);			/* need more data */
	}
    }

    ch &= 0xff;			/* strip any "literal" high bits */
    if(ED(fd)->quoted
       || isalnum(ch)
       || strchr("-.!", ch)
       || (ED(fd)->hit_equal && !isspace((unsigned char) ch))){
	if(ED(fd)->len < ((ED(fd)->element || !ED(fd)->hit_equal)
			       ? HTML_BUF_LEN:MAX_ELEMENT)){
	    ED(fd)->buf[(ED(fd)->len)++] = ch;
	}
	else
	  ED(fd)->overrun = 1;		/* flag it broken */
    }
    else if(isspace((unsigned char) ch) || ch == '='){
	if(html_element_flush(ED(fd))){
	    ED(fd)->badform = 1;
	    return(0);		/* else, we ain't done yet */
	}

	if(!ED(fd)->hit_equal)
	  ED(fd)->hit_equal = (ch == '=');
    }
    else
      ED(fd)->badform = 1;		/* unrecognized data?? */

    return(0);				/* keep collecting */
}


/*
 * Element collector found complete string, integrate it and reset
 * internal collection buffer.
 *
 * Returns zero if element collection buffer flushed, error flag otherwise
 */
int
html_element_flush(el_data)
    CLCTR_S *el_data;
{
    int rv = 0;

    if(el_data->hit_equal){		/* adding a value */
	el_data->hit_equal = 0;
	if(el_data->cur_attrib){
	    if(!el_data->cur_attrib->value){
		el_data->cur_attrib->value = cpystr(el_data->len
						    ? el_data->buf : "");
	    }
	    else{
		dprint(2, (debugfile,
			   "** element: unexpected value: %.10s...\n",
			   (el_data->len && el_data->buf) ? el_data->buf : "\"\""));
		rv = 1;
	    }
	}
	else{
	    dprint(2, (debugfile,
		       "** element: missing attribute name: %.10s...\n",
		       (el_data->len && el_data->buf) ? el_data->buf : "\"\""));
	    rv = 2;
	}

	el_data->len = 0;
	memset(el_data->buf, 0, HTML_BUF_LEN);
    }
    else if(el_data->len){
	if(!el_data->element){
	    el_data->element = cpystr(el_data->buf);
	}
	else{
	    PARAMETER *p = (PARAMETER *)fs_get(sizeof(PARAMETER));
	    memset(p, 0, sizeof(PARAMETER));
	    if(el_data->attribs){
		el_data->cur_attrib->next = p;
		el_data->cur_attrib = p;
	    }
	    else
	      el_data->attribs = el_data->cur_attrib = p;

	    p->attribute = cpystr(el_data->buf);
	}

	el_data->len = 0;
	memset(el_data->buf, 0, HTML_BUF_LEN);
    }

    return(rv);			/* report whatever happened above */
}


/*
 * html_element_comment - "Special" comment handling here
 */
void
html_element_comment(f, s)
    FILTER_S *f;
    char     *s;
{
    char *p;

    while(*s && isspace((unsigned char) *s))
      s++;

    /*
     * WARNING: "!--chtml" denotes "Conditional HTML", a UW-ism.
     */
    if(!struncmp(s, "chtml ", 6)){
	s += 6;
	if(!struncmp(s, "if ", 3)){
	    HD(f)->bitbucket = 1;	/* default is failure! */
	    switch(*(s += 3)){
	      case 'P' :
	      case 'p' :
		if(!struncmp(s + 1, "inemode=", 8)){
		    if(!strucmp(s = removing_quotes(s + 9), "function_key")
		       && F_ON(F_USE_FK, ps_global))
		      HD(f)->bitbucket = 0;
		    else if(!strucmp(s, "running"))
		      HD(f)->bitbucket = 0;
		    else if(!strucmp(s, "phone_home") && ps_global->phone_home)
		      HD(f)->bitbucket = 0;
#ifdef	_WINDOWS
		    else if(!strucmp(s, "os_windows"))
		      HD(f)->bitbucket = 0;
#endif
		}

		break;

	      case '[' :	/* test */
		if(p = strindex(++s, ']')){
		    *p = '\0';		/* tie off test string */
		    removing_leading_white_space(s);
		    removing_trailing_white_space(s);
		    if(*s == '-' && *(s+1) == 'r'){ /* readable file? */
			for(s += 2; *s && isspace((unsigned char) *s); s++)
			  ;


			HD(f)->bitbucket = (can_access(CHTML_VAR_EXPAND(removing_quotes(s)),
						       READ_ACCESS) != 0);
		    }
		}

		break;

	      default :
		break;
	    }
	}
	else if(!strucmp(s, "else")){
	    HD(f)->bitbucket = !HD(f)->bitbucket;
	}
	else if(!strucmp(s, "endif")){
	    /* Clean up after chtml here */
	    HD(f)->bitbucket = 0;
	}
    }
    else if(!HD(f)->bitbucket){
	if(!struncmp(s, "#include ", 9)){
	    char  buf[MAILTMPLEN], *bufp;
	    int   len, end_of_line;
	    FILE *fp;

	    /* Include the named file */
	    if(!struncmp(s += 9, "file=", 5)
	       && (fp = fopen(CHTML_VAR_EXPAND(removing_quotes(s+5)), "r"))){
		html_element_output(f, HTML_NEWLINE);

		while(fgets(buf, sizeof(buf), fp)){
		    if((len = strlen(buf)) && buf[len-1] == '\n'){
			end_of_line = 1;
			buf[--len]  = '\0';
		    }
		    else
		      end_of_line = 0;

		    for(bufp = buf; len; bufp++, len--)
		      html_element_output(f, (int) *bufp);

		    if(end_of_line)
		      html_element_output(f, HTML_NEWLINE);
		}

		fclose(fp);
		html_element_output(f, HTML_NEWLINE);
		HD(f)->blanks = 0;
		if(f->f1 == WSPACE)
		  f->f1 = DFL;
	    }
	}
	else if(!struncmp(s, "#echo ", 6)){
	    if(!struncmp(s += 6, "var=", 4)){
		char	*p, buf[MAILTMPLEN];
		ADDRESS *adr;
		extern char datestamp[];

		if(!strcmp(s = removing_quotes(s + 4), "PINE_VERSION")){
		    p = pine_version;
		}
		else if(!strcmp(s, "PINE_COMPILE_DATE")){
		    p = datestamp;
		}
		else if(!strcmp(s, "PINE_TODAYS_DATE")){
		    rfc822_date(p = buf);
		}
		else if(!strcmp(s, "_LOCAL_FULLNAME_")){
		    p = (ps_global->VAR_LOCAL_FULLNAME
			 && ps_global->VAR_LOCAL_FULLNAME[0])
			    ? ps_global->VAR_LOCAL_FULLNAME
			    : "Local Support";
		}
		else if(!strcmp(s, "_LOCAL_ADDRESS_")){
		    p = (ps_global->VAR_LOCAL_ADDRESS
			 && ps_global->VAR_LOCAL_ADDRESS[0])
			   ? ps_global->VAR_LOCAL_ADDRESS
			   : "postmaster";
		    adr = rfc822_parse_mailbox(&p, ps_global->maildomain);
		    sprintf(p = buf, "%.*s@%.*s",
			    sizeof(buf)/2 - 3, adr->mailbox,
			    sizeof(buf)/2 - 3, adr->host);
		    mail_free_address(&adr);
		}
		else if(!strcmp(s, "_BUGS_FULLNAME_")){
		    p = (ps_global->VAR_BUGS_FULLNAME
			 && ps_global->VAR_BUGS_FULLNAME[0])
			    ? ps_global->VAR_BUGS_FULLNAME
			    : "Place to report Pine Bugs";
		}
		else if(!strcmp(s, "_BUGS_ADDRESS_")){
		    p = (ps_global->VAR_BUGS_ADDRESS
			 && ps_global->VAR_BUGS_ADDRESS[0])
			    ? ps_global->VAR_BUGS_ADDRESS : "postmaster";
		    adr = rfc822_parse_mailbox(&p, ps_global->maildomain);
		    sprintf(p = buf, "%.*s@%.*s",
			    sizeof(buf)/2 - 3, adr->mailbox,
			    sizeof(buf)/2 - 3, adr->host);
		    mail_free_address(&adr);
		}
		else if(!strcmp(s, "CURRENT_DIR")){
		    getcwd(p = buf, sizeof(buf));
		}
		else if(!strcmp(s, "HOME_DIR")){
		    p = ps_global->home_dir;
		}
		else if(!strcmp(s, "PINE_CONF_PATH")){
#if defined(_WINDOWS) || !defined(SYSTEM_PINERC)
		    p = "/usr/local/lib/pine.conf";
#else
		    p = SYSTEM_PINERC;
#endif
		}
		else if(!strcmp(s, "PINE_CONF_FIXED_PATH")){
#ifdef SYSTEM_PINERC_FIXED
		    p = SYSTEM_PINERC_FIXED;
#else
		    p = "/usr/local/lib/pine.conf.fixed";
#endif
		}
		else if(!strcmp(s, "PINE_INFO_PATH")){
		    p = SYSTEM_PINE_INFO_PATH;
		}
		else if(!strcmp(s, "MAIL_SPOOL_PATH")){
		    p = sysinbox();
		}
		else if(!strcmp(s, "MAIL_SPOOL_LOCK_PATH")){
		    /* Don't put the leading /tmp/. */
		    int i, j;

		    p = sysinbox();
		    if(p){
			for(j = 0, i = 0; p[i] && j < MAILTMPLEN - 1; i++){
			    if(p[i] == '/')
				buf[j++] = '\\';
			    else
			      buf[j++] = p[i];
			}
			buf[j++] = '\0';
			p = buf;
		    }
		}
		else
		  p = NULL;

		if(p){
		    if(f->f1 == WSPACE){
			html_element_output(f, ' ');
			f->f1 = DFL;			/* clear it */
		    }

		    while(*p)
		      html_element_output(f, (int) *p++);
		}
	    }
	}
    }
}


void
html_element_output(f, ch)
    FILTER_S *f;
    int	      ch;
{
    if(HANDLERS(f))
      (*HANDLERS(f)->f)(HANDLERS(f), ch, GF_DATA);
    else
      html_output(f, ch);
}


/*
 * collect html entities and return its value when done.
 *
 * Returns 0		 : we need more data
 *	   1-255	 : char value of entity collected
 *	   HTML_BADVALUE : good data, but no named match or out of range
 *	   HTML_BADDATA  : invalid input
 *
 * NOTES:
 *  - entity format is "'&' tag ';'" and represents a literal char
 *  - named entities are CASE SENSITIVE.
 *  - numeric char references (where the tag is prefixed with a '#')
 *    are a char with that numbers value
 *  - numeric vals are 0-255 except for the ranges: 0-8, 11-31, 127-159.
 */
int
html_entity_collector(f, ch, alternate)
    FILTER_S  *f;
    int	       ch;
    char     **alternate;
{
    static char len = 0;
    static char buf[MAX_ENTITY];
    int rv = 0, i;

    if((len == 0)
	 ? (isalpha((unsigned char) ch) || ch == '#')
	 : ((isdigit((unsigned char) ch)
	     || (isalpha((unsigned char) ch) && buf[0] != '#'))
	    && len < MAX_ENTITY - 1)){
	buf[len++] = ch;
    }
    else if((isspace((unsigned char) ch) || ch == ';') && len &&
	    len < MAX_ENTITY){
	buf[len] = '\0';		/* got something! */
	switch(buf[0]){
	  case '#' :
	    rv = atoi(&buf[1]);
	    if(ps_global->pass_ctrl_chars
	       || (ps_global->pass_c1_ctrl_chars && rv >= 0x80 && rv < 0xA0)
	       || (rv == '\t' || rv == '\n' || rv == '\r'
		   || (rv > 31 && rv < 127) || (rv > 159 && rv < 256)
		   || (rv >= 0x2000 && rv <=0x2063)
		   || (rv == 0x20AC) || (rv == 0x2122))){
		if(alternate || rv > 0377){
		    if(alternate)
		      *alternate = NULL;
		    for(i = 0; entity_tab[i].name; i++)
		      if(entity_tab[i].value == rv){
			  if(alternate)
			    *alternate = entity_tab[i].plain;
			  if(entity_tab[i].altvalue)
			    rv = entity_tab[i].altvalue;
			  break;
		      }
		    if(rv > 0377){
			if(entity_tab[i].name && entity_tab[i].plain)
			  rv = HTML_EXTVALUE;
			else
			  rv = HTML_BADVALUE;
		    }
		}
	    }
	    else
	      rv = HTML_BADVALUE;

	    break;

	  default :
	    rv = HTML_BADVALUE;		/* in case we fail below */
	    for(i = 0; entity_tab[i].name; i++)
	      if(strcmp(entity_tab[i].name, buf) == 0){
		  rv = entity_tab[i].value;
		  if(alternate)
		    *alternate = entity_tab[i].plain;

		  break;
	      }

	    break;
	}
    }
    else
      rv = HTML_BADDATA;		/* bogus input! */

    if(rv){				/* nonzero return, clean up */
	if(rv > 0xff && alternate
	    && rv != HTML_EXTVALUE){	/* provide bogus data to caller */
	    if(len < MAX_ENTITY)
	      buf[len] = '\0';

	    *alternate = buf;
	}

	len = 0;
    }

    return(rv);
}


/*----------------------------------------------------------------------
  HTML text to plain text filter

  This basically tries to do the best it can with HTML 2.0 (RFC1866)
  with bits of RFC 1942 (plus some HTML 3.2 thrown in as well) text
  formatting.

 ----*/
void
gf_html2plain(f, flg)
    FILTER_S *f;
    int       flg;
{
/* BUG: qoute incoming \255 values (see "yuml" above!) */
    if(flg == GF_DATA){
	register int c;
	GF_INIT(f, f->next);

	if(!HTML_WROTE(f)){
	    int ii;

	    for(ii = HTML_INDENT(f); ii > 0; ii--)
	      html_putc(f, ' ');

	    HTML_WROTE(f) = 1;
	}

	while(GF_GETC(f, c)){
	    /*
	     * First we have to collect any literal entities...
	     * that is, IF we're not already collecting one
	     * AND we're not in element's text or, if we are, we're
	     * not in quoted text.  Whew.
	     */
	    if(f->t){
		int   i;
		char *alt = NULL;

		switch(i = html_entity_collector(f, c, &alt)){
		  case 0:		/* more data required? */
		    continue;		/* go get another char */

		  case HTML_BADVALUE :
		  case HTML_BADDATA :
		    /* if supplied, process bogus data */
		    HTML_PROC(f, '&');
		    for(; *alt; alt++)
		      HTML_PROC(f, *alt);

		    if(c == '&' && !HD(f)->quoted){
			f->t = '&';
			continue;
		    }
		    else
		      f->t = 0;		/* don't come back next time */

		    break;

		  default :		/* thing to process */
		    f->t = 0;		/* don't come back */

		    /*
		     * Map some of the undisplayable entities?
		     */
		    if(((HD(f)->alt_entity && i > 127)
			   || i == HTML_EXTVALUE) && alt && alt[0]){
			for(; *alt; alt++){
			    c = MAKE_LITERAL(*alt);
			    HTML_PROC(f, c);
			}

			continue;
		    }

		    c = MAKE_LITERAL(i);
		    break;
		}
	    }
	    else if(c == '&' && !HD(f)->quoted){
		f->t = '&';
		continue;
	    }

	    /*
	     * then we process whatever we got...
	     */

	    HTML_PROC(f, c);
	}

	GF_OP_END(f);			/* clean up our input pointers */
    }
    else if(flg == GF_EOD){
	while(HANDLERS(f))
	  /* BUG: should complain about "Unexpected end of HTML text." */
	  html_pop(f, HANDLERS(f)->f);

	html_output(f, HTML_NEWLINE);
	if(ULINE_BIT(f))
	  HTML_ULINE(f, ULINE_BIT(f) = 0);
	if(BOLD_BIT(f))
	  HTML_BOLD(f, BOLD_BIT(f) = 0);
	HTML_FLUSH(f);
	fs_give((void **)&f->line);
	if(HD(f)->color)
	  free_color_pair(&HD(f)->color);

	fs_give(&f->data);
	if(f->opt){
	    if(((HTML_OPT_S *)f->opt)->base)
	      fs_give((void **) &((HTML_OPT_S *)f->opt)->base);

	    fs_give(&f->opt);
	}

	(*f->next->f)(f->next, GF_DATA);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset html2plain\n"));
	f->data  = (HTML_DATA_S *) fs_get(sizeof(HTML_DATA_S));
	memset(f->data, 0, sizeof(HTML_DATA_S));
	HD(f)->wrapstate = 1;		/* start with flowing text */
	HD(f)->wrapcol   = WRAP_COLS(f) - 8;
	f->f1    = DFL;			/* state */
	f->f2    = 0;			/* chars in wrap buffer */
	f->n     = 0L;			/* chars on line so far */
	f->linep = f->line = (char *)fs_get(HTML_BUF_LEN * sizeof(char));
	HD(f)->line_bufsize = HTML_BUF_LEN; /* initial bufsize of line */
	HD(f)->alt_entity =  (!ps_global->VAR_CHAR_SET
			      || strucmp(ps_global->VAR_CHAR_SET, "iso-8859-1"));
    }
}



/*
 * html_indent - do the requested indent level function with appropriate
 *		 flushing and such.
 *
 *   Returns: indent level prior to set/increment
 */
int
html_indent(f, val, func)
    FILTER_S *f;
    int	      val, func;
{
    int old = HD(f)->indent_level;

    /* flush pending data at old indent level */
    switch(func){
      case HTML_ID_INC :
	html_output_flush(f);
	if((HD(f)->indent_level += val) < 0)
	  HD(f)->indent_level = 0;

	break;

      case HTML_ID_SET :
	html_output_flush(f);
	HD(f)->indent_level = val;
	break;

      default :
	break;
    }

    return(old);
}



/*
 * html_blanks - Insert n blank lines into output
 */
void
html_blank(f, n)
    FILTER_S *f;
    int	      n;
{
    /* Cap off any flowing text, and then write blank lines */
    if(f->f2 || f->n || CENTER_BIT(f) || HD(f)->centered || WRAPPED_LEN(f))
      html_output(f, HTML_NEWLINE);

    if(HD(f)->wrapstate)
      while(HD(f)->blanks < n)	/* blanks inc'd by HTML_NEWLINE */
	html_output(f, HTML_NEWLINE);
}



/*
 *  html_newline -- insert a newline mindful of embedded tags
 */
void
html_newline(f)
    FILTER_S *f;
{
    html_write_newline(f);		/* commit an actual newline */

    if(f->n){				/* and keep track of blank lines */
	HD(f)->blanks = 0;
	f->n = 0L;
    }
    else
      HD(f)->blanks++;
}


/*
 * output the given char, handling any requested wrapping.
 * It's understood that all whitespace handed us is written.  In other
 * words, junk whitespace is weeded out before it's given to us here.
 *
 */
void
html_output(f, ch)
    FILTER_S *f;
    int	      ch;
{
    if(CENTER_BIT(f)){			/* center incoming text */
	html_output_centered(f, ch);
    }
    else{
	static short embedded = 0; /* BUG: reset on entering filter */
	static char *color_ptr = NULL;

	if(HD(f)->centered){
	    html_centered_flush(f);
	    fs_give((void **) &HD(f)->centered->line.buf);
	    fs_give((void **) &HD(f)->centered->word.buf);
	    fs_give((void **) &HD(f)->centered);
	}

	if(HD(f)->wrapstate){
	    if(ch == HTML_NEWLINE){		/* hard newline */
		html_output_flush(f);
		html_newline(f);
	    }
	    else
	      HD(f)->blanks = 0;		/* reset blank line counter */

	    if(ch == TAG_EMBED){	/* takes up no space */
		embedded = 1;
		HTML_LINEP_PUTC(f, TAG_EMBED);
	    }
	    else if(embedded){	/* ditto */
		if(ch == TAG_HANDLE)
		  embedded = -1;	/* next ch is length */
		else if(ch == TAG_FGCOLOR || ch == TAG_BGCOLOR){
		    if(!HD(f)->color)
		      HD(f)->color = new_color_pair(NULL, NULL);

		    if(ch == TAG_FGCOLOR)
		      color_ptr = HD(f)->color->fg;
		    else
		      color_ptr = HD(f)->color->bg;

		    embedded = 11;
		}
		else if(embedded < 0){
		    embedded = ch;	/* number of embedded chars */
		}
		else{
		    embedded--;
		    if(color_ptr)
		      *color_ptr++ = ch;

		    if(embedded == 0 && color_ptr){
			*color_ptr = '\0';
			color_ptr = NULL;
		    }
		}

		HTML_LINEP_PUTC(f, ch);
	    }
	    else if(HTML_ISSPACE(ch)){
		html_output_flush(f);
	    }
	    else{
		if(HD(f)->prefix)
		  html_a_prefix(f);

		if(++f->f2 >= WRAP_COLS(f)){
		    HTML_LINEP_PUTC(f, ch & 0xff);
		    HTML_FLUSH(f);
		    html_newline(f);
		    if(HD(f)->in_anchor)
		      html_write_anchor(f, HD(f)->in_anchor);
		}
		else
		  HTML_LINEP_PUTC(f, ch & 0xff);
	    }
	}
	else{
	    if(HD(f)->prefix)
	      html_a_prefix(f);

	    html_output_flush(f);

	    switch(embedded){
	      case 0 :
		switch(ch){
		  default :
		    f->n++;			/* inc displayed char count */
		    HD(f)->blanks = 0;		/* reset blank line counter */
		    html_putc(f, ch & 0xff);
		    break;

		  case TAG_EMBED :	/* takes up no space */
		    html_putc(f, TAG_EMBED);
		    embedded = -2;
		    break;

		  case HTML_NEWLINE :		/* newline handling */
		    if(!f->n)
		      break;

		  case '\n' :
		    html_newline(f);

		  case '\r' :
		    break;
		}

		break;

	      case -2 :
		embedded = 0;
		switch(ch){
		  case TAG_HANDLE :
		    embedded = -1;	/* next ch is length */
		    break;

		  case TAG_BOLDON :
		    BOLD_BIT(f) = 1;
		    break;

		  case TAG_BOLDOFF :
		    BOLD_BIT(f) = 0;
		    break;

		  case TAG_ULINEON :
		    ULINE_BIT(f) = 1;
		    break;

		  case TAG_ULINEOFF :
		    ULINE_BIT(f) = 0;
		    break;

		  case TAG_FGCOLOR :
		    if(!HD(f)->color)
		      HD(f)->color = new_color_pair(NULL, NULL);

		    color_ptr = HD(f)->color->fg;
		    embedded = 11;
		    break;

		  case TAG_BGCOLOR :
		    if(!HD(f)->color)
		      HD(f)->color = new_color_pair(NULL, NULL);

		    color_ptr = HD(f)->color->bg;
		    embedded = 11;
		    break;

		  case TAG_HANDLEOFF :
		    ch = TAG_INVOFF;
		    HD(f)->in_anchor = 0;
		    break;

		  default :
		    break;
		}

		html_putc(f, ch);
		break;

	      case -1 :
		embedded = ch;	/* number of embedded chars */
		html_putc(f, ch);
		break;

	      default :
		embedded--;
		if(color_ptr)
		  *color_ptr++ = ch;

		if(embedded == 0 && color_ptr){
		    *color_ptr = '\0';
		    color_ptr = NULL;
		}

		html_putc(f, ch);
		break;
	    }
	}
    }
}


/*
 * flush any buffered chars waiting for wrapping.
 */
void
html_output_flush(f)
    FILTER_S *f;
{
    if(f->f2){
	if(f->n && ((int) f->n) + f->f2 > HD(f)->wrapcol)
	  html_newline(f);		/* wrap? */

	if(f->n){			/* text already on the line? */
	    html_putc(f, ' ');
	    f->n++;			/* increment count */
	}
	else{
	    /* write at start of new line */
	    html_write_indent(f, HD(f)->indent_level);

	    if(HD(f)->in_anchor)
	      html_write_anchor(f, HD(f)->in_anchor);
	}

	f->n += f->f2;
	HTML_FLUSH(f);
    }
}



/*
 * html_output_centered - managed writing centered text
 */
void
html_output_centered(f, ch)
    FILTER_S *f;
    int	      ch;
{
    if(!HD(f)->centered){		/* new text? */
	html_output_flush(f);
	if(f->n)			/* start on blank line */
	  html_newline(f);

	HD(f)->centered = (CENTER_S *) fs_get(sizeof(CENTER_S));
	memset(HD(f)->centered, 0, sizeof(CENTER_S));
	/* and grab a buf to start collecting centered text */
	HD(f)->centered->line.len  = WRAP_COLS(f);
	HD(f)->centered->line.buf  = (char *) fs_get(HD(f)->centered->line.len
							      * sizeof(char));
	HD(f)->centered->line.used = HD(f)->centered->line.width = 0;
	HD(f)->centered->word.len  = 32;
	HD(f)->centered->word.buf  = (char *) fs_get(HD(f)->centered->word.len
							       * sizeof(char));
	HD(f)->centered->word.used = HD(f)->centered->word.width = 0;
    }

    if(ch == HTML_NEWLINE){		/* hard newline */
	html_centered_flush(f);
    }
    else if(ch == TAG_EMBED){		/* takes up no space */
	HD(f)->centered->embedded = 1;
	html_centered_putc(&HD(f)->centered->word, TAG_EMBED);
    }
    else if(HD(f)->centered->embedded){
	static char *color_ptr = NULL;

	if(ch == TAG_HANDLE){
	    HD(f)->centered->embedded = -1; /* next ch is length */
	}
	else if(ch == TAG_FGCOLOR || ch == TAG_BGCOLOR){
	    if(!HD(f)->color)
	      HD(f)->color = new_color_pair(NULL, NULL);
	    
	    if(ch == TAG_FGCOLOR)
	      color_ptr = HD(f)->color->fg;
	    else
	      color_ptr = HD(f)->color->bg;

	    HD(f)->centered->embedded = 11;
	}
	else if(HD(f)->centered->embedded < 0){
	    HD(f)->centered->embedded = ch; /* number of embedded chars */
	}
	else{
	    HD(f)->centered->embedded--;
	    if(color_ptr)
	      *color_ptr++ = ch;
	    
	    if(HD(f)->centered->embedded == 0 && color_ptr){
		*color_ptr = '\0';
		color_ptr = NULL;
	    }
	}

	html_centered_putc(&HD(f)->centered->word, ch);
    }
    else if(isspace((unsigned char) ch)){
	if(!HD(f)->centered->space++){	/* end of a word? flush! */
	    int i;

	    if(WRAPPED_LEN(f) > HD(f)->wrapcol){
		html_centered_flush_line(f);
		/* fall thru to put current "word" on blank "line" */
	    }
	    else if(HD(f)->centered->line.width){
		/* put space char between line and appended word */
		html_centered_putc(&HD(f)->centered->line, ' ');
		HD(f)->centered->line.width++;
	    }

	    for(i = 0; i < HD(f)->centered->word.used; i++)
	      html_centered_putc(&HD(f)->centered->line,
				 HD(f)->centered->word.buf[i]);

	    HD(f)->centered->line.width += HD(f)->centered->word.width;
	    HD(f)->centered->word.used  = 0;
	    HD(f)->centered->word.width = 0;
	}
    }
    else{
	if(HD(f)->prefix)
	  html_a_prefix(f);

	/* ch is start of next word */
	HD(f)->centered->space = 0;
	if(HD(f)->centered->word.width >= WRAP_COLS(f))
	  html_centered_flush(f);

	html_centered_putc(&HD(f)->centered->word, ch);
	HD(f)->centered->word.width++;
    }
}


/*
 * html_centered_putc -- add given char to given WRAPLINE_S
 */
void
html_centered_putc(wp, ch)
    WRAPLINE_S *wp;
    int		ch;
{
    if(wp->used + 1 >= wp->len){
	wp->len += 64;
	fs_resize((void **) &wp->buf, wp->len * sizeof(char));
    }

    wp->buf[wp->used++] = ch;
}



/*
 * html_centered_flush - finish writing any pending centered output
 */
void
html_centered_flush(f)
    FILTER_S *f;
{
    int i;

    /*
     * If word present (what about line?) we need to deal with
     * appending it...
     */
    if(HD(f)->centered->word.width && WRAPPED_LEN(f) > HD(f)->wrapcol)
      html_centered_flush_line(f);

    if(WRAPPED_LEN(f)){
	/* figure out how much to indent */
	if((i = (WRAP_COLS(f) - WRAPPED_LEN(f))/2) > 0)
	  html_write_indent(f, i);

	if(HD(f)->centered->anchor)
	  html_write_anchor(f, HD(f)->centered->anchor);

	html_centered_handle(&HD(f)->centered->anchor,
			     HD(f)->centered->line.buf,
			     HD(f)->centered->line.used);
	html_write(f, HD(f)->centered->line.buf, HD(f)->centered->line.used);

	if(HD(f)->centered->word.used){
	    if(HD(f)->centered->line.width)
	      html_putc(f, ' ');

	    html_centered_handle(&HD(f)->centered->anchor,
				 HD(f)->centered->word.buf,
				 HD(f)->centered->word.used);
	    html_write(f, HD(f)->centered->word.buf,
		       HD(f)->centered->word.used);
	}

	HD(f)->centered->line.used  = HD(f)->centered->word.used  = 0;
	HD(f)->centered->line.width = HD(f)->centered->word.width = 0;
    }
    else{
      if(HD(f)->centered->word.used){
	html_write(f, HD(f)->centered->word.buf,
		   HD(f)->centered->word.used);
	HD(f)->centered->line.used  = HD(f)->centered->word.used  = 0;
	HD(f)->centered->line.width = HD(f)->centered->word.width = 0;
      }
      HD(f)->blanks++;			/* advance the blank line counter */
    }

    html_newline(f);			/* finish the line */
}


/*
 * html_centered_handle - scan the line for embedded handles
 */
void
html_centered_handle(h, line, len)
    int  *h;
    char *line;
    int   len;
{
    int n;

    while(len-- > 0)
      if(*line++ == TAG_EMBED && len-- > 0)
	switch(*line++){
	  case TAG_HANDLE :
	    if((n = *line++) >= --len){
		*h = 0;
		len -= n;
		while(n--)
		  *h = (*h * 10) + (*line++ - '0');
	    }
	    break;

	  case TAG_HANDLEOFF :
	  case TAG_INVOFF :
	    *h = 0;		/* assumption 23,342: inverse off ends tags */
	    break;

	  default :
	    break;
	}
}



/*
 * html_centered_flush_line - flush the centered "line" only
 */
void
html_centered_flush_line(f)
    FILTER_S *f;
{
    if(HD(f)->centered->line.used){
	int i, j;

	/* hide "word" from flush */
	i = HD(f)->centered->word.used;
	j = HD(f)->centered->word.width;
	HD(f)->centered->word.used  = 0;
	HD(f)->centered->word.width = 0;
	html_centered_flush(f);

	HD(f)->centered->word.used  = i;
	HD(f)->centered->word.width = j;
    }
}


/*
 * html_write_indent - write indention mindful of display attributes
 */
void
html_write_indent(f, indent)
    FILTER_S *f;
    int	      indent;
{
    if(! STRIP(f)){
	if(BOLD_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_BOLDOFF);
	}

	if(ULINE_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_ULINEOFF);
	}
    }

    f->n = indent;
    while(indent-- > 0)
      html_putc(f, ' ');		/* indent as needed */

    /*
     * Resume any previous embedded state
     */
    if(! STRIP(f)){
	if(BOLD_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_BOLDON);
	}

	if(ULINE_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_ULINEON);
	}
    }
}


/*
 *
 */
void
html_write_anchor(f, anchor)
    FILTER_S *f;
    int	      anchor;
{
    char buf[256];
    int  i;

    html_putc(f, TAG_EMBED);
    html_putc(f, TAG_HANDLE);
    sprintf(buf, "%d", anchor);
    html_putc(f, (int) strlen(buf));

    for(i = 0; buf[i]; i++)
      html_putc(f, buf[i]);
}


/*
 * html_write_newline - write a newline mindful of display attributes
 */
void
html_write_newline(f)
    FILTER_S *f;
{
    int i;

    if(! STRIP(f)){			/* First tie, off any embedded state */
	if(HD(f)->in_anchor){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_INVOFF);
	}

	if(BOLD_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_BOLDOFF);
	}

	if(ULINE_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_ULINEOFF);
	}

	if(HD(f)->color && (HD(f)->color->fg[0] || HD(f)->color->bg[0])){
	    char        *p;
	    int          i;

	    p = color_embed(ps_global->VAR_NORM_FORE_COLOR,
			    ps_global->VAR_NORM_BACK_COLOR);
	    for(i = 0; i < 2 * (RGBLEN + 2); i++)
	      html_putc(f, p[i]);
	}
    }

    html_write(f, "\015\012", 2);
    for(i = HTML_INDENT(f); i > 0; i--)
      html_putc(f, ' ');

    if(! STRIP(f)){			/* First tie, off any embedded state */
	if(BOLD_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_BOLDON);
	}

	if(ULINE_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_ULINEON);
	}

	if(HD(f)->color && (HD(f)->color->fg[0] || HD(f)->color->bg[0])){
	    char        *p, *tfg, *tbg;
	    int          i;
	    COLOR_PAIR  *tmp;

	    tfg = HD(f)->color->fg;
	    tbg = HD(f)->color->bg;
	    tmp = new_color_pair(tfg[0] ? tfg 
	      : color_to_asciirgb(ps_global->VAR_NORM_FORE_COLOR),
	      tbg[0] ? tbg
	      : color_to_asciirgb(ps_global->VAR_NORM_BACK_COLOR));
	    if(pico_is_good_colorpair(tmp)){
		p = color_embed(tfg[0] ? tfg 
				: ps_global->VAR_NORM_FORE_COLOR,
				tbg[0] ? tbg 
				: ps_global->VAR_NORM_BACK_COLOR);
		for(i = 0; i < 2 * (RGBLEN + 2); i++)
		  html_putc(f, p[i]);
	    }

	    if(tmp)
	      free_color_pair(&tmp);
	}
    }
}


/*
 * html_write - write given n-length string to next filter
 */
void
html_write(f, s, n)
    FILTER_S *f;
    char     *s;
    int	      n;
{
    GF_INIT(f, f->next);

    while(n-- > 0){
	/* keep track of attribute state?  Not if last char! */
	if(!STRIP(f) && *s == TAG_EMBED && n-- > 0){
	    GF_PUTC(f->next, TAG_EMBED);
	    switch(*++s){
	      case TAG_BOLDON :
		BOLD_BIT(f) = 1;
		break;
	      case TAG_BOLDOFF :
		BOLD_BIT(f) = 0;
		break;
	      case TAG_ULINEON :
		ULINE_BIT(f) = 1;
		break;
	      case TAG_ULINEOFF :
		ULINE_BIT(f) = 0;
		break;
	      case TAG_HANDLEOFF :
		HD(f)->in_anchor = 0;
		GF_PUTC(f->next, TAG_INVOFF);
		s++;
		continue;
	      case TAG_HANDLE :
		if(n-- > 0){
		    int i = *++s;

		    GF_PUTC(f->next, TAG_HANDLE);
		    if(i <= n){
			n -= i;
			GF_PUTC(f->next, i);
			HD(f)->in_anchor = 0;
			while(1){
			    HD(f)->in_anchor = (HD(f)->in_anchor * 10)
								+ (*++s - '0');
			    if(--i)
			      GF_PUTC(f->next, *s);
			    else
			      break;
			}
		    }
		}

		break;
	      default:
		break;
	    }
	}

	GF_PUTC(f->next, (*s++) & 0xff);
    }

    GF_IP_END(f->next);			/* clean up next's input pointers */
}


/*
 * html_putc -- actual work of writing to next filter.
 *		NOTE: Small opt not using full GF_END since our input
 *		      pointers don't need adjusting.
 */
void
html_putc(f, ch)
    FILTER_S *f;
    int	      ch;
{
    GF_INIT(f, f->next);
    GF_PUTC(f->next, ch & 0xff);
    GF_IP_END(f->next);			/* clean up next's input pointers */
}



/*
 * Only current option is to turn on embedded data stripping for text
 * bound to a printer or composer.
 */
void *
gf_html2plain_opt(base, columns, margin, handlesp, flags)
    char      *base;
    int	       columns;
    int	      *margin;
    HANDLE_S **handlesp;
    int	       flags;
{
    HTML_OPT_S *op;
    int		margin_l, margin_r;

    op = (HTML_OPT_S *) fs_get(sizeof(HTML_OPT_S));

    op->base	    = cpystr(base);
    margin_l	    = (margin) ? margin[0] : 0;
    margin_r	    = (margin) ? margin[1] : 0;
    op->indent	    = margin_l;
    op->columns	    = columns - (margin_l + margin_r);
    op->strip	    = ((flags & GFHP_STRIPPED) == GFHP_STRIPPED);
    op->handlesp    = handlesp;
    op->handles_loc = ((flags & GFHP_LOCAL_HANDLES) == GFHP_LOCAL_HANDLES);
    return((void *) op);
}


/* END OF HTML-TO-PLAIN text filter */

/*
 * ESCAPE CODE FILTER - remove unknown and possibly dangerous escape codes
 * from the text stream.
 */

#define	MAX_ESC_LEN	5

/*
 * the simple filter, removes unknown escape codes from the stream
 */
void
gf_escape_filter(f, flg)
    FILTER_S *f;
    int       flg;
{
    register char *p;
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;

	while(GF_GETC(f, c)){

	    if(state){
		if(c == '\033' || f->n == MAX_ESC_LEN){
		    f->line[f->n] = '\0';
		    f->n = 0L;
		    if(!match_escapes(f->line)){
			GF_PUTC(f->next, '^');
			GF_PUTC(f->next, '[');
		    }
		    else
		      GF_PUTC(f->next, '\033');

		    p = f->line;
		    while(*p)
		      GF_PUTC(f->next, *p++);

		    if(c == '\033')
		      continue;
		    else
		      state = 0;			/* fall thru */
		}
		else{
		    f->line[f->n++] = c;		/* collect */
		    continue;
		}
	    }

	    if(c == '\033')
	      state = 1;
	    else
	      GF_PUTC(f->next, c);
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	if(f->f1){
	    if(!match_escapes(f->line)){
		GF_PUTC(f->next, '^');
		GF_PUTC(f->next, '[');
	    }
	    else
	      GF_PUTC(f->next, '\033');
	}

	for(p = f->line; f->n; f->n--, p++)
	  GF_PUTC(f->next, *p);

	fs_give((void **)&(f->line));	/* free temp line buffer */
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset escape\n"));
	f->f1    = 0;
	f->n     = 0L;
	f->linep = f->line = (char *)fs_get((MAX_ESC_LEN + 1) * sizeof(char));
    }
}



/*
 * CONTROL CHARACTER FILTER - transmogrify control characters into their
 * corresponding string representations (you know, ^blah and such)...
 */

/*
 * the simple filter transforms unknown control characters in the stream
 * into harmless strings.
 */
void
gf_control_filter(f, flg)
    FILTER_S *f;
    int       flg;
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int filt_only_c0;

	filt_only_c0 = f->opt ? (*(int *) f->opt) : 0;

	while(GF_GETC(f, c)){

	    /*
	     * Compare to FILTER_THIS macro. Very similar except that we
	     * allow space through and we have to check for filtering
	     * based on filt_only_c0.
	     */
	    if(((c < 0x20 || c == 0x7f)
		|| (c >= 0x80 && c < 0xA0 && !filt_only_c0))
	       && !isspace((unsigned char) c)){
		GF_PUTC(f->next, c >= 0x80 ? '~' : '^');
		GF_PUTC(f->next, (c == 0x7f) ? '?' :
				  (c == 0x1b) ? '[' : (c & 0x1f) + '@');
	    }
	    else
	      GF_PUTC(f->next, c);
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
}


/*
 * TAG FILTER - quote all TAG_EMBED characters by doubling them.
 * This prevents the possibility of embedding other tags.
 * We assume that this filter should only be used for something
 * that is eventually writing to a display, which has the special
 * knowledge of quoted TAG_EMBEDs.
 */
void
gf_tag_filter(f, flg)
    FILTER_S *f;
    int       flg;
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;

	while(GF_GETC(f, c)){

	    if((c & 0xff) == (TAG_EMBED & 0xff)){
		GF_PUTC(f->next, TAG_EMBED);
		GF_PUTC(f->next, c);
	    }
	    else
	      GF_PUTC(f->next, c);
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
}


/*
 * LINEWRAP FILTER - insert CRLF's at end of nearest whitespace before
 * specified line width
 */


typedef struct wrap_col_s {
    unsigned	bold:1;
    unsigned	uline:1;
    unsigned	inverse:1;
    unsigned	tags:1;
    unsigned	do_indent:1;
    unsigned	on_comma:1;
    unsigned	flowed:1;
    unsigned	delsp:1;
    unsigned	quoted:1;
    unsigned	allwsp:1;
    unsigned	hard_nl:1;
    unsigned	leave_flowed:1;
    unsigned    use_color:1;
    COLOR_PAIR *color;
    STORE_S    *spaces;
    short	embedded,
		space_len;
    char       *lineendp;
    int		anchor,
		prefbrk,
		prefbrkn,
		quote_depth,
		quote_count,
		sig,
		state,
		wrap_col,
		wrap_max,
		margin_l,
		margin_r,
		indent;
    char	special[256];
} WRAP_S;

#define	WRAP_MARG_L(F)	(((WRAP_S *)(F)->opt)->margin_l)
#define	WRAP_MARG_R(F)	(((WRAP_S *)(F)->opt)->margin_r)
#define	WRAP_COL(F)	(((WRAP_S *)(F)->opt)->wrap_col - WRAP_MARG_R(F) - ((((WRAP_S *)(F)->opt)->leave_flowed) ? 1 : 0))
#define	WRAP_MAX_COL(F)	(((WRAP_S *)(F)->opt)->wrap_max - WRAP_MARG_R(F) - ((((WRAP_S *)(F)->opt)->leave_flowed) ? 1 : 0))
#define	WRAP_INDENT(F)	(((WRAP_S *)(F)->opt)->indent)
#define	WRAP_DO_IND(F)	(((WRAP_S *)(F)->opt)->do_indent)
#define	WRAP_COMMA(F)	(((WRAP_S *)(F)->opt)->on_comma)
#define	WRAP_FLOW(F)	(((WRAP_S *)(F)->opt)->flowed)
#define	WRAP_DELSP(F)	(((WRAP_S *)(F)->opt)->delsp)
#define	WRAP_FL_QD(F)	(((WRAP_S *)(F)->opt)->quote_depth)
#define	WRAP_FL_QC(F)	(((WRAP_S *)(F)->opt)->quote_count)
#define	WRAP_FL_SIG(F)	(((WRAP_S *)(F)->opt)->sig)
#define	WRAP_HARD(F)	(((WRAP_S *)(F)->opt)->hard_nl)
#define	WRAP_LV_FLD(F)	(((WRAP_S *)(F)->opt)->leave_flowed)
#define	WRAP_USE_CLR(F)	(((WRAP_S *)(F)->opt)->use_color)
#define	WRAP_STATE(F)	(((WRAP_S *)(F)->opt)->state)
#define	WRAP_QUOTED(F)	(((WRAP_S *)(F)->opt)->quoted)
#define	WRAP_TAGS(F)	(((WRAP_S *)(F)->opt)->tags)
#define	WRAP_BOLD(F)	(((WRAP_S *)(F)->opt)->bold)
#define	WRAP_ULINE(F)	(((WRAP_S *)(F)->opt)->uline)
#define	WRAP_INVERSE(F)	(((WRAP_S *)(F)->opt)->inverse)
#define	WRAP_LASTC(F)	(((WRAP_S *)(F)->opt)->lineendp)
#define	WRAP_EMBED(F)	(((WRAP_S *)(F)->opt)->embedded)
#define	WRAP_ANCHOR(F)	(((WRAP_S *)(F)->opt)->anchor)
#define	WRAP_PB_OFF(F)	(((WRAP_S *)(F)->opt)->prefbrk)
#define	WRAP_PB_LEN(F)	(((WRAP_S *)(F)->opt)->prefbrkn)
#define	WRAP_ALLWSP(F)	(((WRAP_S *)(F)->opt)->allwsp)
#define	WRAP_SPC_LEN(F)	(((WRAP_S *)(F)->opt)->space_len)
#define	WRAP_SPEC(F, C)	((WRAP_S *) (F)->opt)->special[C]
#define	WRAP_COLOR(F)	(((WRAP_S *)(F)->opt)->color)
#define	WRAP_COLOR_SET(F)  ((WRAP_COLOR(F)) && (WRAP_COLOR(F)->fg[0]))
#define	WRAP_SPACES(F)	(((WRAP_S *)(F)->opt)->spaces)
#define	WRAP_PUTC(F,C,V) {						\
			    if((F)->linep == WRAP_LASTC(F)){		\
				size_t offset = (F)->linep - (F)->line;	\
				fs_resize((void **) &(F)->line,		\
					  (2 * offset) * sizeof(char)); \
				(F)->linep = &(F)->line[offset];	\
				WRAP_LASTC(F) = &(F)->line[2*offset-1];	\
			    }						\
			    *(F)->linep++ = (C);			\
			    if (V) (F)->f2++;				\
			}

#define	WRAP_EMBED_PUTC(F,C) {						\
			    if((F)->f2){				\
			        WRAP_PUTC((F), C, 0);			\
			    }						\
			    else					\
			      so_writec(C, WRAP_SPACES(F));		\
}

#define	WRAP_COLOR_UNSET(F)	{					\
			    if(WRAP_COLOR_SET(F)){			\
			      WRAP_COLOR(F)->fg[0] = '\0';		\
			    }						\
			}

/*
 * wrap_flush_embed flags
 */
#define	WFE_NONE	0		/* Nothing special */
#define	WFE_CNT_HANDLE	1		/* account for/don't write handles */


int     wrap_flush PROTO((FILTER_S *, unsigned char **, unsigned char **,
			  unsigned char **, unsigned char **));
int     wrap_flush_embed PROTO((FILTER_S *, unsigned char **, unsigned char **,
				unsigned char **, unsigned char **));
int     wrap_flush_s PROTO((FILTER_S *,char *, int, int, unsigned char **,
			    unsigned char **, unsigned char **,
			    unsigned char **, int));
int     wrap_eol PROTO((FILTER_S *, int, unsigned char **, unsigned char **,
			  unsigned char **, unsigned char **));
int     wrap_bol PROTO((FILTER_S *, int, int, unsigned char **,
			unsigned char **, unsigned char **, unsigned char **));
int     wrap_quote_insert PROTO((FILTER_S *, unsigned char **,
				 unsigned char **, unsigned char **, 
				 unsigned char **));

/*
 * the no longer simple filter, breaks lines at end of white space nearest
 * to global "gf_wrap_width" in length
 * It also supports margins, indents (inverse indenting, really) and
 * flowed text (ala RFC 3676)
 *
 */
void
gf_wrap(f, flg)
    FILTER_S *f;
    int       flg;
{
    register long i;
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;
	register int x;

	while(GF_GETC(f, c)){

	    switch(state){
	      case CCR :				/* CRLF or CR in text ? */
		state = BOL;				/* either way, handle start */

		if(WRAP_FLOW(f)){
		    if(f->f2 == 0 && WRAP_SPC_LEN(f)){	/* wrapped line */
			/*
			 * whack trailing space char, but be aware
			 * of embeds in space buffer.  grok them just
			 * in case they contain a 0x20 value
			 */
			if(WRAP_DELSP(f)){
			    char *sb, *sbp, *scp = NULL;
			    int   x;

			    for(sb = sbp = (char *)so_text(WRAP_SPACES(f)); *sbp; sbp++){
				switch(*sbp){
				  case ' ' :
				    scp = sbp;
				    break;

				  case TAG_EMBED :
				    sbp++;
				    switch (*sbp++){
				      case TAG_HANDLE :
					x = (int) *sbp++;
					if(strlen(sbp) >= x)
					  sbp += (x - 1);

					break;

				      case TAG_FGCOLOR :
				      case TAG_BGCOLOR :
					if(strlen(sbp) >= RGBLEN)
					  sbp += (RGBLEN - 1);

					break;

				      default :
					break;
				    }

				    break;

				  default :
				    break;
				}
			    }

			    /* replace space buf without trailing space char */
			    if(scp){
				STORE_S *ns = so_get(CharStar, NULL, EDIT_ACCESS);

				*scp++ = '\0';
				WRAP_SPC_LEN(f)--;

				so_puts(ns, sb);
				so_puts(ns, scp);

				so_give(&WRAP_SPACES(f));
				WRAP_SPACES(f) = ns;
			    }
			}
		    }
		    else{				/* fixed line */
			WRAP_HARD(f) = 1;
			wrap_flush(f, &ip, &eib, &op, &eob);
			wrap_eol(f, 0, &ip, &eib, &op, &eob);

			/*
			 * When we get to a real end of line, we don't need to
			 * remember what the special color was anymore because
			 * we aren't going to be changing back to it. We unset it
			 * so that we don't keep resetting the color to normal.
			 */
			WRAP_COLOR_UNSET(f);
		    }

		    if(c == '\012'){			/* get c following LF */
		      break;
		    }
		    /* else c is first char of new line, fall thru */
		}
		else{
		    wrap_flush(f, &ip, &eib, &op, &eob);
		    wrap_eol(f, 0, &ip, &eib, &op, &eob);
		    WRAP_COLOR_UNSET(f);		/* see note above */
		    if(c == '\012'){
			break;
		    }
		    /* else fall thru to deal with beginning of line */
		}

	      case BOL :
		if(WRAP_FLOW(f)){
		    if(c == '>'){
			WRAP_FL_QC(f) = 1;		/* init it */
			state = FL_QLEV;		/* go collect it */
		    }
		    else {
			/* if EMBEDed, process it and return here */
			if(c == (unsigned char) TAG_EMBED){
			    WRAP_EMBED_PUTC(f, TAG_EMBED);
			    WRAP_STATE(f) = state;
			    state = TAG;
			    continue;
			}

			/* quote level change implies new paragraph */
			if(WRAP_FL_QD(f)){
			    WRAP_FL_QD(f) = 0;
			    if(WRAP_HARD(f) == 0){
				WRAP_HARD(f) = 1;
				wrap_flush(f, &ip, &eib, &op, &eob);
				wrap_eol(f, 0, &ip, &eib, &op, &eob);
				WRAP_COLOR_UNSET(f);	/* see note above */
			    }
			}

			if(WRAP_HARD(f)){
			    wrap_bol(f, 0, 1, &ip, &eib, &op,
				     &eob);   /* write quoting prefix */
			    WRAP_HARD(f) = 0;			    
			}

			switch (c) {
			  case '\015' :			/* a blank line? */
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    state = CCR;		/* go collect it */
			    break;

			  case ' ' :			/* space stuffed */
			    state = FL_STF;		/* just eat it */
			    break;

			  case '-' :			/* possible sig-dash */
			    WRAP_FL_SIG(f) = 1;	        /* init state */
			    state = FL_SIG;		/* go collect it */
			    break;

			  default :
			    state = DFL;		/* go back to normal */
			    goto case_dfl;		/* handle c like DFL case */
			}
		    }
		}
		else{
		    state = DFL;
		    if(WRAP_COMMA(f) && c == TAB){
			wrap_bol(f, 1, 0, &ip, &eib, &op,
				 &eob);    /* convert to normal indent */
			break;
		    }

		    wrap_bol(f,0,0, &ip, &eib, &op, &eob);
		    goto case_dfl;			/* handle c like DFL case */
		}

		break;

	      case  FL_QLEV :
		if(c == '>'){				/* another level */
		    WRAP_FL_QC(f)++;
		}
		else {
		    /* if EMBEDed, process it and return here */
		    if(c == (unsigned char) TAG_EMBED){
			WRAP_EMBED_PUTC(f, TAG_EMBED);
			WRAP_STATE(f) = state;
			state = TAG;
			continue;
		    }

		    /* quote level change signals new paragraph */
		    if(WRAP_FL_QC(f) != WRAP_FL_QD(f)){
			WRAP_FL_QD(f) = WRAP_FL_QC(f);
			if(WRAP_HARD(f) == 0){		/* add hard newline */ 
			    WRAP_HARD(f) = 1;		/* hard newline */
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    wrap_eol(f, 0, &ip, &eib, &op, &eob);
			    WRAP_COLOR_UNSET(f);	/* see note above */
			}
		    }

		    if(WRAP_HARD(f)){
			wrap_bol(f,0,1, &ip, &eib, &op, &eob);
			WRAP_HARD(f) = 0;
		    }

		    switch (c) {
		      case '\015' :			/* a blank line? */
			wrap_flush(f, &ip, &eib, &op, &eob);
			state = CCR;			/* go collect it */
			break;

		      case ' ' :			/* space-stuffed! */
			state = FL_STF;			/* just eat it */
			break;

		      case '-' :			/* sig dash? */
			WRAP_FL_SIG(f) = 1;
			state = FL_SIG;
			break;

		      default :				/* something else */
			state = DFL;
			goto case_dfl;			/* handle c like DFL */
		    }
		}

		break;

	      case FL_STF :				/* space stuffed */
		switch (c) {
		  case '\015' :				/* a blank line? */
		    wrap_flush(f, &ip, &eib, &op, &eob);
		    state = CCR;			/* go collect it */
		    break;

		  case (unsigned char) TAG_EMBED :	/* process TAG data */
		    WRAP_EMBED_PUTC(f, TAG_EMBED);
		    WRAP_STATE(f) = state;		/* and return */
		    state = TAG;
		    continue;

		  case '-' :				/* sig dash? */
		    WRAP_FL_SIG(f) = 1;
		    WRAP_ALLWSP(f) = 0;
		    state = FL_SIG;
		    break;

		  default :				/* something else */
		    state = DFL;
		    goto case_dfl;			/* handle c like DFL */
		}

		break;

	      case FL_SIG :				/* sig-dash collector */
		switch (WRAP_FL_SIG(f)){		/* possible sig-dash? */
		  case 1 :
		    if(c != '-'){			/* not a sigdash */
			if((f->n + WRAP_SPC_LEN(f) + 1) > WRAP_COL(f)){
			    wrap_flush_embed(f, &ip, &eib, &op,
					     &eob);      /* note any embedded*/
			    wrap_eol(f, 1, &ip, &eib,
				     &op, &eob);       /* plunk down newline */
			    wrap_bol(f, 1, 1, &ip, &eib,
				     &op, &eob);         /* write any prefix */
			}

			WRAP_PUTC(f,'-', 1);		/* write what we got */

			WRAP_FL_SIG(f) = 0;
			state = DFL;
			goto case_dfl;
		    }

		    /* don't put anything yet until we know to wrap or not */
		    WRAP_FL_SIG(f) = 2;
		    break;

		  case 2 :
		    if(c != ' '){			    /* not a sigdash */
			WRAP_PUTC(f, '-', 1);
			if((f->n + WRAP_SPC_LEN(f) + 2) > WRAP_COL(f)){
			    wrap_flush_embed(f, &ip, &eib, &op,
					     &eob);      /* note any embedded*/
			    wrap_eol(f, 1, &ip, &eib,
				     &op, &eob);       /* plunk down newline */
			    wrap_bol(f, 1, 1, &ip, &eib, &op, 
				     &eob);   	         /* write any prefix */
			}

			WRAP_PUTC(f,'-', 1);		/* write what we got */

			WRAP_FL_SIG(f) = 0;
			state = DFL;
			goto case_dfl;
		    }

		    /* don't put anything yet until we know to wrap or not */
		    WRAP_FL_SIG(f) = 3;
		    break;

		  case 3 :
		    if(c == '\015'){			/* success! */
			/* known sigdash, newline if soft nl */
			if(WRAP_SPC_LEN(f)){
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    wrap_eol(f, 0, &ip, &eib, &op, &eob);
			    wrap_bol(f, 0, 1, &ip, &eib, &op, &eob);
			}
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,' ',1);

			state = CCR;
			break;
		    }
		    else{
			WRAP_FL_SIG(f) = 4;		/* possible success */
		    }

		  case 4 :
		    switch(c){
		      case (unsigned char) TAG_EMBED :
			/*
			 * At this point we're almost 100% sure that we've got
			 * a sigdash.  Putc it (adding newline if previous
			 * was a soft nl) so we get it the right color
			 * before we store this new embedded stuff
			 */
			if(WRAP_SPC_LEN(f)){
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    wrap_eol(f, 0, &ip, &eib, &op, &eob);
			    wrap_bol(f, 0, 1, &ip, &eib, &op, &eob);
			}
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,' ',1);

			WRAP_FL_SIG(f) = 5;
			break;

		      case '\015' :			/* success! */
			/* 
			 * We shouldn't get here, but in case we do, we have
			 * not yet put the sigdash
			 */
			if(WRAP_SPC_LEN(f)){
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    wrap_eol(f, 0, &ip, &eib, &op, &eob);
			    wrap_bol(f, 0, 1, &ip, &eib, &op, &eob);
			}
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,' ',1);

			state = CCR;
			break;

		      default :				/* that's no sigdash! */
			/* write what we got but didn't put yet */
			WRAP_PUTC(f,'-', 1);
			WRAP_PUTC(f,'-', 1);
			WRAP_PUTC(f,' ', 1);

			WRAP_FL_SIG(f) = 0;
			wrap_flush(f, &ip, &eib, &op, &eob);
			WRAP_SPC_LEN(f) = 1;
			state = DFL;			/* set normal state */
			goto case_dfl;			/* and go do "c" */
		    }

		    break;

		  case 5 :
		    WRAP_STATE(f) = FL_SIG;		/* come back here */
		    WRAP_FL_SIG(f) = 6;			/* and seek EOL */
		    WRAP_EMBED_PUTC(f, TAG_EMBED);
		    state = TAG;			/* process embed */
		    goto case_tag;

		  case 6 :
		    /* 
		     * at this point we've already putc the sigdash in case 4
		     */
		    switch(c){
		      case (unsigned char) TAG_EMBED :
			WRAP_FL_SIG(f) = 5;
			break;

		      case '\015' :			/* success! */
			state = CCR;
			break;

		      default :				/* that's no sigdash! */
			/* 
			 * probably never reached (fake sigdash with embedded
			 * stuff) but if this did get reached, then we
			 * might have accidentally disobeyed a soft nl
			 */
			WRAP_FL_SIG(f) = 0;
			wrap_flush(f, &ip, &eib, &op, &eob);
			WRAP_SPC_LEN(f) = 1;
			state = DFL;			/* set normal state */
			goto case_dfl;			/* and go do "c" */
		    }

		    break;


		  default :
		    dprint(2, (debugfile, "-- gf_wrap: BROKEN FLOW STATE: %d\n",
			       WRAP_FL_SIG(f)));
		    WRAP_FL_SIG(f) = 0;
		    state = DFL;			/* set normal state */
		    goto case_dfl;			/* and go process "c" */
		}

		break;

	      case_dfl :
	      case DFL :
		if(WRAP_SPEC(f, c)){
		    switch(c){
		      default :
			if(WRAP_QUOTED(f))
			  break;

			if(f->f2){			/* any non-lwsp to flush? */
			    if(WRAP_COMMA(f)){
				/* remember our second best break point */
				WRAP_PB_OFF(f) = f->linep - f->line;
				WRAP_PB_LEN(f) = f->f2;
				break;
			    }
			    else
			      wrap_flush(f, &ip, &eib, &op, &eob);
			}

			switch(c){			/* remember separator */
			  case ' ' :
			    WRAP_SPC_LEN(f)++;
			    so_writec(' ',WRAP_SPACES(f));
			    break;

			  case TAB :
			  {
			      int i = (int) f->n + WRAP_SPC_LEN(f);

			      do
				WRAP_SPC_LEN(f)++;
			      while(++i & 0x07);

			      so_writec(TAB,WRAP_SPACES(f));
			  }

			  break;

			  default :			/* some control char? */
			    WRAP_SPC_LEN(f) += 2;
			    break;
			}

			continue;

		      case '\"' :
			WRAP_QUOTED(f) = !WRAP_QUOTED(f);
			break;

		      case '\015' :			/* already has newline? */
			state = CCR;
			continue;

		      case '\012' :			 /* bare LF in text? */
			wrap_flush(f, &ip, &eib, &op, &eob); /* they must've */
			wrap_eol(f, 0, &ip, &eib, &op, &eob);       /* meant */
			wrap_bol(f,1,1, &ip, &eib, &op, &eob); /* newline... */
			continue;

		      case (unsigned char) TAG_EMBED :
			WRAP_EMBED_PUTC(f, TAG_EMBED);
			WRAP_STATE(f) = state;
			state = TAG;
			continue;

		      case ',' :
			if(!WRAP_QUOTED(f)){
			    if(f->n + WRAP_SPC_LEN(f) + f->f2 + 1 > WRAP_COL(f)){
				if(WRAP_ALLWSP(f))    /* if anything visible */
				  wrap_flush(f, &ip, &eib, &op,
					     &eob);  /* ... blat buf'd chars */

				wrap_eol(f, 1, &ip, &eib, &op,
					 &eob);  /* plunk down newline */
				wrap_bol(f, 1, 1, &ip, &eib, &op,
					 &eob);    /* write any prefix */
			    }

			    WRAP_PUTC(f, ',', 1);	/* put out comma */
			    wrap_flush(f, &ip, &eib, &op,
				       &eob);       /* write buf'd chars */
			    continue;
			}

			break;
		    }
		}

		/* what do we have to write? */
		if(c == TAB){
		    int i = (int) f->n;

		    while(i & 0x07)
		      i++;

		    x = i - f->n;
		}
		else if(iscntrl((unsigned char) c))
		  x = 2;
		else
		  x = 1;

		if(WRAP_ALLWSP(f)){			/* if anything visible */
		    if(f->n + WRAP_SPC_LEN(f) + f->f2 + x > WRAP_MAX_COL(f)){
			/*
			 * a little reaching behind the curtain here.
			 * if there's at least a preferable break point, use
			 * it and stuff what's left back into the wrap buffer.
			 * note, the "vis" variable is just a count, visibility
			 * is only tied to a particular character in order to 
			 * maintain the count to determine display length.
			 * it's not a problem that we're arbitraily assigning
			 * visibility in WRAP_PUTC below.
			 * also, the "nwsp" latch is used to skip leading
			 * whitespace
			 */
			if(WRAP_PB_OFF(f)){
			    char *p1 = f->line + WRAP_PB_OFF(f);
			    char *p2 = f->linep;
			    int   nwsp = 0, vis = f->f2 - WRAP_PB_LEN(f);

			    f->linep = p1;

			    wrap_flush(f, &ip, &eib, &op,
				       &eob); /* flush shortened buf */
			    
			    while(p1 < p2){
				char c2 = *p1++;
				if(c2 != ' ' || nwsp){
				    WRAP_PUTC(f, c2, (vis > 0));
				    nwsp = 1;
				}

				vis--;
			    }
			}
			else
			  wrap_flush(f, &ip, &eib, &op, &eob);

			wrap_eol(f, 1, &ip, &eib, &op,
				 &eob);     /* plunk down newline */
			wrap_bol(f,1,1, &ip, &eib, &op,
				 &eob);      /* write any prefix */
		    }
		}
		else if((f->n + WRAP_SPC_LEN(f) + f->f2 + x) > WRAP_COL(f)){
		    wrap_flush_embed(f, &ip, &eib, &op, &eob);
		    wrap_eol(f, 1, &ip, &eib, &op,
			     &eob);	    /* plunk down newline */
		    wrap_bol(f,1,1, &ip, &eib, &op,
			     &eob);	      /* write any prefix */
		}

		WRAP_PUTC(f, c, 1);
		break;

	      case_tag :
	      case TAG :
		WRAP_EMBED_PUTC(f, c);
		switch(c){
		  case TAG_HANDLE :
		    WRAP_EMBED(f) = -1;
		    state = HANDLE;
		    break;

		  case TAG_FGCOLOR :
		  case TAG_BGCOLOR :
		    WRAP_EMBED(f) = RGBLEN;
		    state = HDATA;
		    break;

		  default :
		    state = WRAP_STATE(f);
		    break;
		}

		break;

	      case HANDLE :
		WRAP_EMBED_PUTC(f, c);
		WRAP_EMBED(f) = c;
		state = HDATA;
		break;

	      case HDATA :
		if(f->f2){
		  WRAP_PUTC(f, c, 0);
		}
		else
		  so_writec(c, WRAP_SPACES(f));

		if(!(WRAP_EMBED(f) -= 1)){
		    state = WRAP_STATE(f);
		}

		break;
	    }
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	wrap_flush(f, &ip, &eib, &op, &eob);
	if(WRAP_COLOR(f))
	  free_color_pair(&WRAP_COLOR(f));

	fs_give((void **) &f->line);	/* free temp line buffer */
	so_give(&WRAP_SPACES(f));
	fs_give((void **) &f->opt);	/* free wrap widths struct */
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset wrap\n"));
	f->f1    = BOL;
	f->n     = 0L;		/* displayed length of line so far */
	f->f2	 = 0;		/* displayed length of buffered chars */
	WRAP_HARD(f) = 1;	/* starting at beginning of line */
	if(! (WRAP_S *) f->opt)
	  f->opt = gf_wrap_filter_opt(75, 80, NULL, 0, 0);

	while(WRAP_INDENT(f) >= WRAP_MAX_COL(f))
	  WRAP_INDENT(f) /= 2;

	f->line  = (char *) fs_get(WRAP_MAX_COL(f) * sizeof(char));
	f->linep = f->line;
	WRAP_LASTC(f) = &f->line[WRAP_MAX_COL(f) - 1];

	for(i = 0; i < 256; i++)
	  ((WRAP_S *) f->opt)->special[i] = ((i == '\"' && WRAP_COMMA(f))
					     || i == '\015'
					     || i == '\012'
					     || (i == (unsigned char) TAG_EMBED
						 && WRAP_TAGS(f))
					     || (i == ',' && WRAP_COMMA(f)
						 && !WRAP_QUOTED(f))
					     || isspace((unsigned char) i));
	WRAP_SPACES(f) = so_get(CharStar, NULL, EDIT_ACCESS);
    }
}

int
wrap_flush(f, ipp, eibp, opp, eobp)
    FILTER_S *f;
    unsigned char **ipp;
    unsigned char **eibp;
    unsigned char **opp;
    unsigned char **eobp;
{
    register char *s;
    register int   n;
    s = (char *)so_text(WRAP_SPACES(f));
    n = so_tell(WRAP_SPACES(f));
    so_seek(WRAP_SPACES(f), 0L, 0);
    wrap_flush_s(f, s, n, 1, ipp, eibp, opp, eobp, WFE_NONE);
    so_truncate(WRAP_SPACES(f), 0L);
    WRAP_SPC_LEN(f) = 0;
    s = f->line;
    n = f->linep - f->line;
    wrap_flush_s(f, s, n, 1, ipp, eibp, opp, eobp, WFE_NONE);
    f->f2    = 0;
    f->linep = f->line;
    WRAP_PB_OFF(f) = 0;
    WRAP_PB_LEN(f) = 0;

    return 0;
}

int
wrap_flush_embed(f, ipp, eibp, opp, eobp)
    FILTER_S    *f;
    unsigned char **ipp;
    unsigned char **eibp;
    unsigned char **opp;
    unsigned char **eobp;
{
  register char *s;
  register int   n;
  s = (char *)so_text(WRAP_SPACES(f));
  n = so_tell(WRAP_SPACES(f));
  so_seek(WRAP_SPACES(f), 0L, 0);
  wrap_flush_s(f, s, n, 0, ipp, eibp, opp, eobp, WFE_CNT_HANDLE);
  so_truncate(WRAP_SPACES(f), 0L);
  WRAP_SPC_LEN(f) = 0;

  return 0;
}

int
wrap_flush_s(f, s, n, v, ipp, eibp, opp, eobp, flags)
    FILTER_S    *f;
    char        *s;
    int          n;
    int          v;
    unsigned char **ipp;
    unsigned char **eibp;
    unsigned char **opp;
    unsigned char **eobp;
    int		  flags;
{
    for(; n > 0; n--,s++){
	if(*s == TAG_EMBED){
	    if(n-- > 0){
		switch(*++s){
		  case TAG_BOLDON :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_BOLDON);
		    WRAP_BOLD(f) = 1;
		    break;
		  case TAG_BOLDOFF :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_BOLDOFF);
		    WRAP_BOLD(f) = 0;
		    break;
		  case TAG_ULINEON :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_ULINEON);
		    WRAP_ULINE(f) = 1;
		    break;
		  case TAG_ULINEOFF :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_ULINEOFF);
		    WRAP_ULINE(f) = 0;
		    break;
		  case TAG_INVOFF :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_INVOFF);
		    WRAP_ANCHOR(f) = 0;
		    break;
		  case TAG_HANDLE :
		    if((flags & WFE_CNT_HANDLE) == 0)
		      GF_PUTC_GLO(f->next,TAG_EMBED);

		    if(n-- > 0){
			int i = *++s;

			if((flags & WFE_CNT_HANDLE) == 0)
			  GF_PUTC_GLO(f->next, TAG_HANDLE);

			if(i <= n){
			    n -= i;

			    if((flags & WFE_CNT_HANDLE) == 0)
			      GF_PUTC_GLO(f->next, i);

			    WRAP_ANCHOR(f) = 0;
			    while(i-- > 0){
				WRAP_ANCHOR(f) = (WRAP_ANCHOR(f) * 10) + (*++s-'0');

				if((flags & WFE_CNT_HANDLE) == 0)
				  GF_PUTC_GLO(f->next,*s);
			    }

			}
		    }
		    break;
		  case TAG_FGCOLOR :
		    if(pico_usingcolor() && n >= RGBLEN){
			int i;
			GF_PUTC_GLO(f->next,TAG_EMBED);
			GF_PUTC_GLO(f->next,TAG_FGCOLOR);
			if(!WRAP_COLOR(f))
			  WRAP_COLOR(f)=new_color_pair(NULL,NULL);
			strncpy(WRAP_COLOR(f)->fg, s+1, RGBLEN);
			WRAP_COLOR(f)->fg[RGBLEN]='\0';
			i = RGBLEN;
			n -= i;
			while(i-- > 0)
			  GF_PUTC_GLO(f->next,
				  (*++s) & 0xff);
		    }
		    break;
		  case TAG_BGCOLOR :
		    if(pico_usingcolor() && n >= RGBLEN){
			int i;
			GF_PUTC_GLO(f->next,TAG_EMBED);
			GF_PUTC_GLO(f->next,TAG_BGCOLOR);
			if(!WRAP_COLOR(f))
			  WRAP_COLOR(f)=new_color_pair(NULL,NULL);
			strncpy(WRAP_COLOR(f)->bg, s+1, RGBLEN);
			WRAP_COLOR(f)->bg[RGBLEN]='\0';
			i = RGBLEN;
			n -= i;
			while(i-- > 0)
			  GF_PUTC_GLO(f->next,
				  (*++s) & 0xff);
		    }
		    break;
		  default :
		    break;
		}
	    }
	}
	else if(v){
	    if(*s == TAB)
	      while(++f->n & 0x07) ;
	    else
	      f->n += (iscntrl((unsigned char) *s) ? 2 : 1);
	    if(f->n <= WRAP_MAX_COL(f)){
		GF_PUTC_GLO(f->next, (*s) & 0xff);
	    }
	    else{
		dprint(2, (debugfile, "-- gf_wrap: OVERRUN: %c\n", (*s) & 0xff));
	    }
	    WRAP_ALLWSP(f) = 0;
	}
    }

    return 0;
}

int
wrap_eol(f, c, ipp, eibp, opp, eobp)
    FILTER_S *f;
    int c;
    unsigned char **ipp;
    unsigned char **eibp;
    unsigned char **opp;
    unsigned char **eobp;
{
    if(c && WRAP_LV_FLD(f))
      GF_PUTC_GLO(f->next, ' ');
    if(WRAP_BOLD(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_BOLDOFF);
    }
    if(WRAP_ULINE(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_ULINEOFF);
    }
    if(WRAP_INVERSE(f) || WRAP_ANCHOR(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_INVOFF);
    }
    if(WRAP_COLOR_SET(f)){
	char *p;
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_FGCOLOR);
	p = color_to_asciirgb(ps_global->VAR_NORM_FORE_COLOR);
	for(; *p; p++)
	  GF_PUTC_GLO(f->next, *p);
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_BGCOLOR);
	p = color_to_asciirgb(ps_global->VAR_NORM_BACK_COLOR);
	for(; *p; p++)
	  GF_PUTC_GLO(f->next, *p);
    }
    GF_PUTC_GLO(f->next, '\015');
    GF_PUTC_GLO(f->next, '\012');
    f->n = 0L;
    so_truncate(WRAP_SPACES(f), 0L);
    WRAP_SPC_LEN(f) = 0;

    return 0;
}

int
wrap_bol(f,ivar,q, ipp, eibp, opp, eobp)
    FILTER_S *f;
    int ivar;
    int q;
    unsigned char **ipp;
    unsigned char **eibp;
    unsigned char **opp;
    unsigned char **eobp;
{
    int n = WRAP_MARG_L(f) + (ivar ? WRAP_INDENT(f) : 0);
    while(n-- > 0){
	f->n++;
	GF_PUTC_GLO(f->next, ' ');
    }
    WRAP_ALLWSP(f) = 1;
    if(q)
      wrap_quote_insert(f, ipp, eibp, opp, eobp);

    if(WRAP_BOLD(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_BOLDON);
    }
    if(WRAP_ULINE(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_ULINEON);
    }
    if(WRAP_INVERSE(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_INVON);
    }
    if(WRAP_COLOR_SET(f)){
	char *p;
	if(WRAP_COLOR(f)->fg[0]){
	    GF_PUTC_GLO(f->next, TAG_EMBED);
	    GF_PUTC_GLO(f->next, TAG_FGCOLOR);
	    p = color_to_asciirgb(WRAP_COLOR(f)->fg);
	    for(; *p; p++)
	      GF_PUTC_GLO(f->next, *p);
	}
	if(WRAP_COLOR(f)->bg[0]){
	    GF_PUTC_GLO(f->next, TAG_EMBED);
	    GF_PUTC_GLO(f->next, TAG_BGCOLOR);
	    p = color_to_asciirgb(WRAP_COLOR(f)->bg);
	    for(; *p; p++)
	      GF_PUTC_GLO(f->next, *p);
	}
    }
    if(WRAP_ANCHOR(f)){
	char buf[64]; int i;
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_HANDLE);
	sprintf(buf, "%d", WRAP_ANCHOR(f));
	GF_PUTC_GLO(f->next, (int) strlen(buf));
	for(i = 0; buf[i]; i++)
	  GF_PUTC_GLO(f->next, buf[i]);
    }

    return 0;
}

int
wrap_quote_insert(f, ipp, eibp, opp, eobp)
    FILTER_S *f;
    unsigned char **ipp;
    unsigned char **eibp;
    unsigned char **opp;
    unsigned char **eobp;
{
    int j, i;
    COLOR_PAIR *col = NULL;
    char *prefix = NULL, *last_prefix = NULL;

    if(ps_global->VAR_QUOTE_REPLACE_STRING){
	get_pair(ps_global->VAR_QUOTE_REPLACE_STRING, &prefix, &last_prefix, 0, 0);
	if(!prefix && last_prefix){
	    prefix = last_prefix;
	    last_prefix = NULL;
	}
    }

    for(j = 0; j < WRAP_FL_QD(f); j++){
	if(WRAP_USE_CLR(f)){
	    if((j % 3) == 0
	       && ps_global->VAR_QUOTE1_FORE_COLOR
	       && ps_global->VAR_QUOTE1_BACK_COLOR
	       && (col = new_color_pair(ps_global->VAR_QUOTE1_FORE_COLOR,
					ps_global->VAR_QUOTE1_BACK_COLOR))
	       && pico_is_good_colorpair(col)){
                GF_COLOR_PUTC(f, col);
            }
	    else if((j % 3) == 1
		    && ps_global->VAR_QUOTE2_FORE_COLOR
		    && ps_global->VAR_QUOTE2_BACK_COLOR
		    && (col = new_color_pair(ps_global->VAR_QUOTE2_FORE_COLOR,
					     ps_global->VAR_QUOTE2_BACK_COLOR))
		    && pico_is_good_colorpair(col)){
	        GF_COLOR_PUTC(f, col);
            }
	    else if((j % 3) == 2
		    && ps_global->VAR_QUOTE3_FORE_COLOR
		    && ps_global->VAR_QUOTE3_BACK_COLOR
		    && (col = new_color_pair(ps_global->VAR_QUOTE3_FORE_COLOR,
					     ps_global->VAR_QUOTE3_BACK_COLOR))
		    && pico_is_good_colorpair(col)){
	        GF_COLOR_PUTC(f, col);
            }
	    if(col){
		free_color_pair(&col);
		col = NULL;
	    }
	}

	if(!WRAP_LV_FLD(f)){
	    if(ps_global->VAR_QUOTE_REPLACE_STRING && prefix){
		for(i = 0; prefix[i]; i++)
		  GF_PUTC_GLO(f->next, prefix[i]);
		f->n += strlen(prefix);
	    }
	    else if(ps_global->VAR_REPLY_STRING
		    && (!strcmp(ps_global->VAR_REPLY_STRING, ">")
			|| !strcmp(ps_global->VAR_REPLY_STRING, "\">\""))){
		GF_PUTC_GLO(f->next, '>');
		f->n += 1;
	    }
	    else{
		GF_PUTC_GLO(f->next, '>');
		GF_PUTC_GLO(f->next, ' ');
		f->n += 2;
	    }
	}
	else{
	    GF_PUTC_GLO(f->next, '>');
	    f->n += 1;
	}
    }
    if(j && WRAP_LV_FLD(f)){
	GF_PUTC_GLO(f->next, ' ');
	f->n++;
    }
    else if(j && last_prefix){
	for(i = 0; last_prefix[i]; i++)
	  GF_PUTC_GLO(f->next, last_prefix[i]);
	f->n += strlen(last_prefix);	
    }

    if(prefix)
      fs_give((void **)&prefix);
    if(last_prefix)
      fs_give((void **)&last_prefix);

    return 0;
}


/*
 * function called from the outside to set
 * wrap filter's width option
 */
void *
gf_wrap_filter_opt(width, width_max, margin, indent, flags)
    int width, width_max;
    int *margin;
    int indent, flags;
{
    WRAP_S *wrap;

    /* NOTE: variables MUST be sanity checked before they get here */
    wrap = (WRAP_S *) fs_get(sizeof(WRAP_S));
    memset(wrap, 0, sizeof(WRAP_S));
    wrap->wrap_col     = width;
    wrap->wrap_max     = width_max;
    wrap->indent       = indent;
    wrap->margin_l     = (margin) ? margin[0] : 0;
    wrap->margin_r     = (margin) ? margin[1] : 0;
    wrap->tags	       = (GFW_HANDLES & flags) == GFW_HANDLES;
    wrap->on_comma     = (GFW_ONCOMMA & flags) == GFW_ONCOMMA;
    wrap->flowed       = (GFW_FLOWED & flags) == GFW_FLOWED;
    wrap->leave_flowed = (GFW_FLOW_RESULT & flags) == GFW_FLOW_RESULT;
    wrap->delsp	       = (GFW_DELSP & flags) == GFW_DELSP;
    wrap->use_color    = (GFW_USECOLOR & flags) == GFW_USECOLOR;

    return((void *) wrap);
}


#define	PF_QD(F)	(((PREFLOW_S *)(F)->opt)->quote_depth)
#define	PF_QC(F)	(((PREFLOW_S *)(F)->opt)->quote_count)
#define	PF_SIG(F)	(((PREFLOW_S *)(F)->opt)->sig)

typedef struct preflow_s {
    int		quote_depth,
		quote_count,
		sig;
} PREFLOW_S;

/*
 * This would normally be handled in gf_wrap. If there is a possibility
 * that a url we want to recognize is cut in half by a soft newline we
 * want to fix that up by putting the halves back together. We do that
 * by deleting the soft newline and putting it all in one line. It will
 * still get wrapped later in gf_wrap. It isn't pretty with all the
 * goto's, but whatta ya gonna do?
 */
void
gf_preflow(f, flg)
    FILTER_S *f;
    int       flg;
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state  = f->f1;
	register int pending = f->f2;

	while(GF_GETC(f, c)){
	    switch(state){
	      case DFL:
default_case:
		switch(c){
		  case ' ':
		    state = WSPACE;
		    break;
		  
		  case '\015':
		    state = CCR;
		    break;
		  
		  default:
		    GF_PUTC(f->next, c);
		    break;
		}

	        break;

	      case CCR:
		switch(c){
		  case '\012':
		    pending = 1;
		    state = BOL;
		    break;
		  
		  default:
		    GF_PUTC(f->next, '\012');
		    state = DFL;
		    goto default_case;
		    break;
		}

	        break;

	      case WSPACE:
		switch(c){
		  case '\015':
		    state = SPACECR;
		    break;
		  
		  default:
		    GF_PUTC(f->next, ' ');
		    state = DFL;
		    goto default_case;
		    break;
		}

	        break;

	      case SPACECR:
		switch(c){
		  case '\012':
		    pending = 2;
		    state = BOL;
		    break;
		  
		  default:
		    GF_PUTC(f->next, ' ');
		    GF_PUTC(f->next, '\012');
		    state = DFL;
		    goto default_case;
		    break;
		}

	        break;

	      case BOL:
		PF_QC(f) = 0;
		if(c == '>'){		/* count quote level */
		    PF_QC(f)++;
		    state = FL_QLEV;
		}
		else{
done_counting_quotes:
		    if(c == ' '){	/* eat stuffed space */
			state = FL_STF;
			break;
		    }

done_with_stuffed_space:
		    if(c == '-'){	/* look for signature */
			PF_SIG(f) = 1;
			state = FL_SIG;
			break;
		    }

done_with_sig:
		    if(pending == 2){
			if(PF_QD(f) == PF_QC(f) && PF_SIG(f) < 4){
			    /* delete pending */

			    PF_QD(f) = PF_QC(f);

			    /* suppress quotes, too */
			    PF_QC(f) = 0;
			}
			else{
			    /*
			     * This should have been a hard new line
			     * instead so leave out the trailing space.
			     */
			    GF_PUTC(f->next, '\015');
			    GF_PUTC(f->next, '\012');

			    PF_QD(f) = PF_QC(f);
			}
		    }
		    else if(pending == 1){
			GF_PUTC(f->next, '\015');
			GF_PUTC(f->next, '\012');
			PF_QD(f) = PF_QC(f);
		    }
		    else{
			PF_QD(f) = PF_QC(f);
		    }

		    pending = 0;
		    state = DFL;
		    while(PF_QC(f)-- > 0)
		      GF_PUTC(f->next, '>');

		    switch(PF_SIG(f)){
		      case 0:
		      default:
		        break;

		      case 1:
			GF_PUTC(f->next, '-');
		        break;

		      case 2:
			GF_PUTC(f->next, '-');
			GF_PUTC(f->next, '-');
		        break;

		      case 3:
		      case 4:
			GF_PUTC(f->next, '-');
			GF_PUTC(f->next, '-');
			GF_PUTC(f->next, ' ');
		        break;
		    }

		    PF_SIG(f) = 0;
		    goto default_case;		/* to handle c */
		}

		break;

	      case FL_QLEV:		/* count quote level */
		if(c == '>')
		  PF_QC(f)++;
		else
		  goto done_counting_quotes;

		break;

	      case FL_STF:		/* eat stuffed space */
		goto done_with_stuffed_space;
	        break;

	      case FL_SIG:		/* deal with sig indicator */
		switch(PF_SIG(f)){
		  case 1:		/* saw '-' */
		    if(c == '-')
		      PF_SIG(f) = 2;
		    else
		      goto done_with_sig;

		    break;

		  case 2:		/* saw '--' */
		    if(c == ' ')
		      PF_SIG(f) = 3;
		    else
		      goto done_with_sig;

		    break;

		  case 3:		/* saw '-- ' */
		    if(c == '\015')
		      PF_SIG(f) = 4;	/* it really is a sig line */

		    goto done_with_sig;
		    break;
		}

	        break;
	    }
	}

	f->f1 = state;
	f->f2 = pending;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	fs_give((void **) &f->opt);
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	PREFLOW_S *pf;

	pf = (PREFLOW_S *) fs_get(sizeof(*pf));
	memset(pf, 0, sizeof(*pf));
	f->opt = (void *) pf;

	f->f1     = BOL;	/* state */
	f->f2     = 0;		/* pending */
	PF_QD(f)  = 0;		/* quote depth */
	PF_QC(f)  = 0;		/* quote count */
	PF_SIG(f) = 0;		/* sig level */
    }
}




/*
 * LINE PREFIX FILTER - insert given text at beginning of each
 * line
 */


#define	GF_PREFIX_WRITE(s)	{ \
				    register char *p; \
				    if(p = (s)) \
				      while(*p) \
					GF_PUTC(f->next, *p++); \
				}


/*
 * the simple filter, prepends each line with the requested prefix.
 * if prefix is null, does nothing, and as with all filters, assumes
 * NVT end of lines.
 */
void
gf_prefix(f, flg)
    FILTER_S *f;
    int       flg;
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;
	register int first = f->f2;

	while(GF_GETC(f, c)){

	    if(first){			/* write initial prefix!! */
		first = 0;		/* but just once */
		GF_PREFIX_WRITE((char *) f->opt);
	    }
	    else if(state){
		state = 0;
		GF_PUTC(f->next, '\015');
		if(c == '\012'){
		    GF_PUTC(f->next, '\012');
		    GF_PREFIX_WRITE((char *) f->opt);
		    continue;
		}
		/* else fall thru to handle 'c' */
	    }

	    if(c == '\015')		/* already has newline? */
	      state = 1;
	    else
	      GF_PUTC(f->next, c);
	}

	f->f1 = state;
	f->f2 = first;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset prefix\n"));
	f->f1   = 0;
	f->f2   = 1;			/* nothing written yet */
    }
}


/*
 * function called from the outside to set
 * prefix filter's prefix string
 */
void *
gf_prefix_opt(prefix)
    char *prefix;
{
    return((void *) prefix);
}


/*
 * function called from the outside to set
 * control filter's option, which says to filter C0 control characters
 * but not C1 control chars. We don't call it at all if we don't want
 * to filter C0 chars either.
 */
void *
gf_control_filter_opt(filt_only_c0)
    int *filt_only_c0;
{
    return((void *) filt_only_c0);
}


/*
 * LINE TEST FILTER - accumulate lines and offer each to the provided
 * test function.
 */

typedef struct _linetest_s {
    linetest_t	f;
    void       *local;
} LINETEST_S;


/* accumulator growth increment */
#define	LINE_TEST_BLOCK	1024

#define	GF_LINE_TEST_EOB(f) \
			((f)->line + ((f)->f2 - 1))

#define	GF_LINE_TEST_ADD(f, c) \
			{ \
				    if(p >= eobuf){ \
					f->f2 += LINE_TEST_BLOCK; \
					fs_resize((void **)&f->line, \
					      (size_t) f->f2 * sizeof(char)); \
					eobuf = GF_LINE_TEST_EOB(f); \
					p = eobuf - LINE_TEST_BLOCK; \
				    } \
				    *p++ = c; \
				}

#define	GF_LINE_TEST_TEST(F, D) \
			{ \
			    unsigned char  c; \
			    register char *cp; \
			    register int   l; \
			    LT_INS_S	  *ins = NULL, *insp; \
			    *p = '\0'; \
			    (D) = (*((LINETEST_S *) (F)->opt)->f)((F)->n++, \
					   (F)->line, &ins, \
					   ((LINETEST_S *) (F)->opt)->local); \
			    if((D) < 2){ \
				for(insp = ins, cp = (F)->line; cp < p; ){ \
				  if(insp && cp == insp->where){ \
				    if(insp->len > 0){ \
				      while(insp && cp == insp->where){ \
					for(l = 0; l < insp->len; l++){ \
					  c = (unsigned char) insp->text[l]; \
					  GF_PUTC((F)->next, c);  \
					}  \
					insp = insp->next; \
				      } \
				    } else if(insp->len < 0){ \
					cp -= insp->len; \
					insp = insp->next; \
					continue; \
				    } \
				  } \
				  GF_PUTC((F)->next, *cp); \
				  cp++; \
				} \
				while(insp){ \
				    for(l = 0; l < insp->len; l++){ \
					c = (unsigned char) insp->text[l]; \
					GF_PUTC((F)->next, c); \
				    } \
				    insp = insp->next; \
				} \
				gf_line_test_free_ins(&ins); \
			    } \
			}



/*
 * this simple filter accumulates characters until a newline, offers it
 * to the provided test function, and then passes it on.  It assumes
 * NVT EOLs.
 */
void
gf_line_test(f, flg)
    FILTER_S *f;
    int	      flg;
{
    register char *p = f->linep;
    register char *eobuf = GF_LINE_TEST_EOB(f);
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;

	while(GF_GETC(f, c)){

	    if(state){
		state = 0;
		if(c == '\012'){
		    int done;

		    GF_LINE_TEST_TEST(f, done);

		    p = (f)->line;

		    if(done == 2)	/* skip this line! */
		      continue;

		    GF_PUTC(f->next, '\015');
		    GF_PUTC(f->next, '\012');
		    /*
		     * if the line tester returns TRUE, it's
		     * telling us its seen enough and doesn't
		     * want to see any more.  Remove ourself
		     * from the pipeline...
		     */
		    if(done){
			if(gf_master == f){
			    gf_master = f->next;
			}
			else{
			    FILTER_S *fprev;

			    for(fprev = gf_master;
				fprev && fprev->next != f;
				fprev = fprev->next)
			      ;

			    if(fprev)		/* wha??? */
			      fprev->next = f->next;
			    else
			      continue;
			}

			while(GF_GETC(f, c))	/* pass input */
			  GF_PUTC(f->next, c);

			GF_FLUSH(f->next);	/* and drain queue */
			fs_give((void **)&f->line);
			fs_give((void **)&f);	/* wax our data */
			return;
		    }
		    else
		      continue;
		}
		else			/* add CR to buffer */
		  GF_LINE_TEST_ADD(f, '\015');
	    } /* fall thru to handle 'c' */

	    if(c == '\015')		/* newline? */
	      state = 1;
	    else
	      GF_LINE_TEST_ADD(f, c);
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	int i;

	GF_LINE_TEST_TEST(f, i);	/* examine remaining data */
	fs_give((void **) &f->line);	/* free line buffer */
	fs_give((void **) &f->opt);	/* free test struct */
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset line_test\n"));
	f->f1 = 0;			/* state */
	f->n  = 0L;			/* line number */
	f->f2 = LINE_TEST_BLOCK;	/* size of alloc'd line */
	f->line = p = (char *) fs_get(f->f2 * sizeof(char));
    }

    f->linep = p;
}


/*
 * function called from the outside to operate on accumulated line.
 */
void *
gf_line_test_opt(test_f, local)
    linetest_t  test_f;
    void       *local;
{
    LINETEST_S *ltp;

    ltp = (LINETEST_S *) fs_get(sizeof(LINETEST_S));
    memset(ltp, 0, sizeof(LINETEST_S));
    ltp->f     = test_f;
    ltp->local = local;
    return((void *) ltp);
}



LT_INS_S **
gf_line_test_new_ins(ins, p, s, n)
    LT_INS_S **ins;
    char      *p, *s;
    int	       n;
{
    *ins = (LT_INS_S *) fs_get(sizeof(LT_INS_S));
    if(((*ins)->len = n) > 0)
      strncpy((*ins)->text = (char *) fs_get(n * sizeof(char)), s, n);
    else
      (*ins)->text = NULL;

    (*ins)->where = p;
    (*ins)->next  = NULL;
    return(&(*ins)->next);
}


void
gf_line_test_free_ins(ins)
    LT_INS_S **ins;
{
    if(ins && *ins){
	if((*ins)->next)
	  gf_line_test_free_ins(&(*ins)->next);

	if((*ins)->text)
	  fs_give((void **) &(*ins)->text);

	fs_give((void **) ins);
    }
}


/*
 * Network virtual terminal to local newline convention filter
 */
void
gf_nvtnl_local(f, flg)
    FILTER_S *f;
    int       flg;
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;

	while(GF_GETC(f, c)){
	    if(state){
		state = 0;
		if(c == '\012'){
		    GF_PUTC(f->next, '\012');
		    continue;
		}
		else
		  GF_PUTC(f->next, '\015');
		/* fall thru to deal with 'c' */
	    }

	    if(c == '\015')
	      state = 1;
	    else
	      GF_PUTC(f->next, c);
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset nvtnl_local\n"));
	f->f1 = 0;
    }
}


/*
 * local to network newline convention filter
 */
void
gf_local_nvtnl(f, flg)
    FILTER_S *f;
    int       flg;
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;

	while(GF_GETC(f, c)){
	    if(c == '\012'){
		GF_PUTC(f->next, '\015');
		GF_PUTC(f->next, '\012');
	    }
	    else
	      GF_PUTC(f->next, c);
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(GF_RESET){
	dprint(9, (debugfile, "-- gf_reset local_nvtnl\n"));
	/* no op */
    }

}


/*
 * display something indicating we're chewing on something
 *
 * NOTE : IF ANY OTHER FILTERS WRITE THE DISPLAY, THIS WILL NEED FIXING
 */
void
gf_busy(f, flg)
    FILTER_S *f;
    int       flg;
{
    static short x = 0;
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;

	while(GF_GETC(f, c)){

	    if(!((++(f->f1))&0x7ff)){ 	/* ding the bell every 2K chars */
		MoveCursor(0, 1);
		f->f1 = 0;
		if((++x)&0x04) x = 0;
		Writechar((x == 0) ? '/' : 	/* CHEATING! */
			  (x == 1) ? '-' :
			  (x == 2) ? '\\' : '|', 0);
	    }

	    GF_PUTC(f->next, c);
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	MoveCursor(0, 1);
	Writechar(' ', 0);
	EndInverse();
	GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint(9, (debugfile, "-- gf_reset busy\n"));
	f->f1 = 0;
        x = 0;
	StartInverse();
    }

    fflush(stdout);
}
