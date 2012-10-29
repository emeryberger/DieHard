#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: attach.c 13711 2004-06-15 22:22:35Z hubert $";
#endif
/*
 * Program:	Routines to support attachments in the Pine composer 
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 *
 * Pine and Pico are registered trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior written
 * permission of the University of Washington.
 * 
 * Pine, Pico, and Pilot software and its included text are Copyright
 * 1989-2004 by the University of Washington.
 * 
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 *
 */
#include "headers.h"
#include <math.h>

#ifdef	ATTACHMENTS


#ifdef	ANSI
    int    ParseAttach(struct hdr_line **,int *,char *,
		       int,char *,char *,int,int *);
    PATMT *NewAttach(char *, long, char *);
    void   ZotAttach(struct pico_atmt *);
    void   sinserts(char *, int, char *, int);
    int	   AttachUpload(char *, char *);
    int	   AttachCancel(char *);
#else
    int    ParseAttach();
    PATMT *NewAttach();
    void   ZotAttach();
    void   sinserts();
    int	   AttachUpload();
    int	   AttachCancel();
#endif

#define	HIBIT_WARN	"Only ASCII characters allowed in attachment comments"


/*
 * AskAttach - ask for attachment fields and build resulting structure
 *             return pointer to that struct if OK, NULL otherwise
 */
AskAttach(cmnt, lm)
char    *cmnt;
LMLIST **lm;
{
    int	    i, status, fbrv, upload = 0;
    int     fb_flags = FB_READ | FB_ATTACH;
    off_t   attsz = 0;
    char    bfn[NLINE];
    char    fn[NLINE], sz[32];
    LMLIST *new;

    i = 2;
    fn[0] = '\0';
    sz[0] = '\0';
    cmnt[0] = '\0';

    while(i){
	if(i == 2){
	    EXTRAKEYS menu_attach[4];
	    int	      n;

	    menu_attach[n = 0].name  = "^T";
	    menu_attach[n].label     = "To Files";
	    menu_attach[n].key	     = (CTRL|'T');

	    if(gmode & MDCMPLT){
		menu_attach[++n].name = "TAB";
		menu_attach[n].label  = "Complete";
		menu_attach[n].key    = (CTRL|'I');
	    }

#if	!defined(DOS) && !defined(MAC)
	    if(Pmaster && Pmaster->upload){
		/*
		 * The Plan: ^R prompts for uploaded file's name which
		 * is passed to the defined upload command when the user
		 * hits Return to confirm the name.
		 * NOTE: this is different than upload into message
		 * text in which case the uploaded name isn't useful so
		 * a temp file is ok (be sure to fix the file mode).
		 */
		menu_attach[++n].name = "^Y";
		menu_attach[n].key    = (CTRL|'Y');
		menu_attach[n].label  = upload ? "Read File" : "RcvUpload";
	    }
#endif

	    menu_attach[++n].name  = NULL;
	    KS_OSDATASET(&menu_attach[0], KS_NONE);
	    status = mlreply(upload ? "Name to give uploaded attachment: "
				    : "File to attach: ",
			     fn, NLINE, QNORML, menu_attach);
	}
	else
	  status = mlreply("Attachment comment: ", cmnt, NLINE, QNODQT, NULL);

	switch(status){
	  case HELPCH:
	    if(Pmaster){
		VARS_TO_SAVE *saved_state;

		saved_state = save_pico_state();
		(*Pmaster->helper)(Pmaster->attach_help, "Attach Help", 1);
		if(saved_state){
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		}

		pico_refresh(FALSE, 1);
		update();
		continue;
	    }
	    else{
		emlwrite("No Attachment %s help yet!",
			 (i == 2) ? "file" : "comment");
		sleep(3);
	    }

	    break;

	  case (CTRL|'I') :
	    if(i == 2){
		char *fname, *p;
		int   dirlen, l = NLINE;  /* length of buffer space for bfn */
	       
		bfn[0] = '\0';
		if(*fn && (p = strrchr(fn, C_FILESEP))){
		    fname = p + 1;
		    l -= fname - fn;
		    dirlen = p - fn;
		    if(p == fn)
		      strcpy(bfn, S_FILESEP);
#ifdef	DOS
		    else if(fn[0] == C_FILESEP
			    || (isalpha((unsigned char)fn[0])
				&& fn[1] == ':')){
		        if(fn[1] == ':' && p == fn+2)
			  dirlen = fname - fn;
#else
		    else if (fn[0] == C_FILESEP || fn[0] == '~') {
#endif
			strncpy(bfn, fn, dirlen);
			bfn[dirlen] = '\0';
		    }
		    else
		      sprintf(bfn, "%s%c%.*s", 
			      (gmode & MDCURDIR)
				? "."
				: ((gmode & MDTREE) || opertree[0])
				    ? opertree : gethomedir(NULL),
			      C_FILESEP, p - fn, fn);
		    }
		    else{
			fname = fn;
			strcpy(bfn, (gmode & MDCURDIR)
				       ? "."
				       : ((gmode & MDTREE) || opertree[0])
					    ? opertree : gethomedir(NULL));
		    }

		    if(!pico_fncomplete(bfn, fname, l - 1))
		      (*term.t_beep)();
		}
		else
		  (*term.t_beep)();

		break;

	  case (CTRL|'T'):
	    if(i != 2){
		(*term.t_beep)();
		break;
	    }

	    *bfn = '\0';
	    if(*fn == '\0' || !isdir(fn, NULL, NULL))
	      strcpy(fn, (gmode & MDCURDIR)
		     ? (browse_dir[0] ? browse_dir : ".")
		     : ((gmode & MDTREE) || opertree[0])
		     ? opertree 
		     : (browse_dir[0] ? browse_dir
			: gethomedir(NULL)));
	    if((fbrv = FileBrowse(fn, NLINE, bfn, NLINE, sz,
				  upload ? fb_flags
				         : fb_flags|FB_LMODEPOS,
				  upload ? NULL
					 : lm)) == 1){
	      if (upload && (strlen(fn)+strlen(S_FILESEP)+strlen(bfn)) < NLINE){

		if((new=(LMLIST *)malloc(sizeof(*new))) == NULL
		   || (new->fname=malloc(strlen(bfn)+1)) == NULL
		   || (new->dir=malloc(strlen(fn)+1)) == NULL){
		    emlwrite("\007Can't malloc space for filename", NULL);
		    return(-1);
		}

		strcpy(new->fname, bfn);
		strcpy(new->dir, fn);
		strncpy(new->size, sz, sizeof(new->size)-1);
		new->size[sizeof(new->size)-1] = '\0';
		new->next = NULL;
		*lm = new;

		strcat(fn, S_FILESEP);
		strcat(fn, bfn);
		if(!AttachUpload(fn, sz)){              
		  i = 2;                  /* keep prompting for file */
		  sleep(3);                       /* problem, show error! */
		}
		else{
		  i--;                    /* go prompt for comment */
		}
	      }
	      else if(!upload){
		  if(lm && *lm && !(*lm)->next)	/* get comment */
		    i--;
		  else{			/* no comments if multiple files */
		      update();
		      return(1);
		  }
	      }
	      else{                           /* trouble */
		*fn = '\0';
		AttachCancel(fn);
		pico_refresh(FALSE,1);
		update();
		emlwrite("\007File name too BIG, cannot select!", NULL);
	        sleep(3);
	      }
	    }
	    else if (!fbrv)
	      *fn = '\0';
	    else{
	      *fn = '\0';
	      AttachCancel(fn);
	      pico_refresh(FALSE, 1);
	      update();
	      emlwrite("\007File name too big, cannot select!", NULL);         
	      sleep(3);
	    }

	    /* fall thru to clean up the screen */

	  case (CTRL|'L'):
	    pico_refresh(FALSE, 1);
	    update();
	    continue;

#if	!defined(DOS) && !defined(MAC)
	  case (CTRL|'Y'):			/* upload? */
	    if(i == 2)
	      upload ^= 1;			/* flip mode */
	    else
	      (*term.t_beep)();

	    break;
#endif

	  case ABORT:
	    return(AttachCancel((upload && i == 1) ? fn : NULL));

	  case TRUE:				/* some comment */
	  case FALSE:				/* No comment */
	    if(i-- == 2){
		if(upload){
		    fixpath(fn, NLINE);		/* names relative to ~ */
		    status = AttachUpload(fn, sz);
		    pico_refresh(FALSE, 1);
		    update();
		    if(!status){
			i = 2;			/* keep prompting for file */
			sleep(3);		/* problem, show error! */
		    }
		}
		else {
		    if(*fn == '\"' && fn[strlen(fn)-1] == '\"'){
			int j;

			for(j = 0; fn[j] = fn[j+1]; j++)
			  ;

			fn[j-1] = '\0';
		    }

		    if(fn[0]){
			if((gmode & MDTREE)
			   && !compresspath(opertree, fn, NLINE)){
			    emlwrite(
	    "Restricted mode allows attachments from %s only: too many ..'s",
				 (gmode&MDSCUR) ? "home directory" : opertree);
			    return(0);
			}
			else{
			    fixpath(fn, NLINE);	/* names relative to ~ */
			    if((gmode&MDTREE) && !in_oper_tree(fn)){
				emlwrite(
			"\007Restricted mode allows attachments from %s only",
			(gmode&MDSCUR) ? "home directory" : opertree);
				return(0);
			    }
			}

			if((status = fexist(fn, "r", &attsz)) != FIOSUC){
			    fioperr(status, fn); /* file DOESN'T exist! */
			    return(0);
			}

			if((new=(LMLIST *)malloc(sizeof(*new))) == NULL
			   || (new->fname=malloc(strlen(fn)+1)) == NULL){
			    emlwrite("\007Can't malloc space for filename", NULL);
			    return(-1);
			}

			new->dir = NULL;
			strcpy(new->fname, fn);
			strcpy(new->size, prettysz(attsz));
			new->next = NULL;
			*lm = new;
		    }
		    else
		      return(AttachCancel((upload && i == 1) ? fn : NULL));
		}
	    }
	    else{
		mlerase();
		return(1);			/* mission accomplished! */
	    }

	    break;
	  default:
	    break;
	}
    }

    return(0);
}


/*
 * AttachUpload - Use call back to run the external upload command.
 */
int
AttachUpload(fn, sz)
    char *fn, *sz;
{
    long l;

    if(gmode&MDSCUR){
	emlwrite("\007Restricted mode disallows uploaded command", NULL);
	return(0);
    }

    if(Pmaster && Pmaster->upload && (*Pmaster->upload)(fn, &l)){
	strcpy(sz, prettysz((off_t)l));
	return(1);
    }

    return(0);
}


/*
 * AttachCancel - 
 */
int
AttachCancel(fn)
    char *fn;
{
    emlwrite("Attach cancelled", NULL);
    if(fn && fn[0])
      unlink(fn);				/* blast uploaded file */

    return(0);
}


extern struct headerentry *headents;

/*
 * SyncAttach - given a pointer to a linked list of attachment structures,
 *              return with that structure sync'd with what's displayed.
 *              delete any attachments in list of structs that's not on 
 *              the display, and add any that aren't in list but on display.
 */
SyncAttach()
{
    int offset = 0,				/* the offset to begin       */
        rv = 0,
        ki = 0,					/* number of known attmnts   */
        bi = 0,					/* build array index         */
        nbld = 0,		                /* size of build array       */
        na,					/* old number of attachmnt   */
        status, i, j, n = 0;
    char file[NLINE],				/* buffers to hold it all    */
         size[32],
         comment[1024];
    struct hdr_line *lp;			/* current line in header    */
    struct headerentry *entry;
    PATMT *tp, **knwn = NULL, **bld;

    for(entry = headents; entry->name != NULL; entry++) {
      if(entry->is_attach)
	break;
    }

    if(Pmaster == NULL)
      return(-1);

    for(tp = Pmaster->attachments; tp; tp = tp->next)
      ki++;			                /* Count known attachments */

    if(ki){
	if((knwn = (PATMT **)malloc((ki+1) * (sizeof(PATMT *)))) == NULL){
	    emlwrite("\007Can't allocate space for %d known attachment array entries", 
		     (void *) (ki + 1));
	    rv = -1;
	    goto exit_early;
	}
	for(i=0, tp = Pmaster->attachments; i < ki; i++, tp = tp->next){
	    knwn[i] = tp;			/* fill table of     */
	                                        /* known attachments */
	}
    }

    
    /*
     * As a quick hack to avoid too many reallocs, check to see if
     * there are more header lines than known attachments.
     */
    for(lp = entry->hd_text; lp ; lp = lp->next)
      nbld++;			                /* count header lines */
    nbld = nbld > ki ? nbld : ki + 1;           

    if((bld = (PATMT **)malloc(nbld * (sizeof(PATMT *)))) == NULL){
	emlwrite("\007Can't allocate space for %d build array entries",
		 (void *) nbld);
	rv = -1;
	goto exit_early;
    }

    lp = entry->hd_text;
    while(lp != NULL){
	char fn[NLINE];
	na = ++n;

	if(bi == nbld){		                /* need to grow build array? */
	    if((bld = (PATMT **)realloc(bld, ++nbld * sizeof(PATMT *))) == NULL){
		emlwrite("\007Can't resize build array to %d entries ",
			 (void *) nbld);
		rv = -1;
		goto exit_early;
	    }
	}
	
	if(status = ParseAttach(&lp, &offset, file, NLINE, size, 
				comment, 1024, &na))
	    rv = (rv < 0) ? rv : status ;       /* remember worst case */

	if(*file == '\0'){
	    if(n != na && na > 0 && na <= ki && (knwn[na-1]->flags & A_FLIT)){
		bld[bi++] = knwn[na-1];
		knwn[na-1] = NULL;
	    }
	    continue;
	}

	if((gmode&MDTREE)
	   && file[0] != '['
	   && (!in_oper_tree(file)
	       || !compresspath(file, fn, NLINE)))
	  /* no attachments outside ~ in secure mode! */
	  continue;

	tp = NULL;
	for(i = 0; i < ki; i++){		/* already know about it? */
	    /*
	     * this is kind of gruesome. what we want to do is keep track
	     * of literal attachment entries because they may not be 
	     * actual files we can access or that the user can readily 
	     * access.
	     */
	    if(knwn[i]
	       && ((!(knwn[i]->flags&A_FLIT)
		    && !strcmp(file, knwn[i]->filename))
		   || ((knwn[i]->flags&A_FLIT) && i+1 == na))){
		tp = knwn[i];
		knwn[i] = NULL;			/* forget we know about it */

		if(status == -1)		/* ignore garbage! */
		  break;

		if((tp->flags&A_FLIT) && strcmp(file, tp->filename)){
		    rv = 1;
		    if((j=strlen(file)) > strlen(tp->filename)){
			if((tp->filename = (char *)realloc(tp->filename,
					        sizeof(char)*(j+1))) == NULL){
			    emlwrite("\007Can't realloc filename space",NULL);
			    rv = -1;
			    goto exit_early;
			}
		    }

		    strcpy(tp->filename, file);
		}
		else if(tp->size && strcmp(tp->size, size)){
		    rv = 1;
		    if((j=strlen(size)) > strlen(tp->size)){
			if((tp->size=(char *)realloc(tp->size,
					        sizeof(char)*(j+1))) == NULL){
			    emlwrite("\007Can't realloc space for size", NULL);
			    rv = -1;
			    goto exit_early;
			}
		    }

		    strcpy(tp->size, size);
		}

		if(strcmp(tp->description, comment)){	/* new comment */
		    rv = 1;
		    if((j=strlen(comment)) > strlen(tp->description)){
			if((tp->description=(char *)realloc(tp->description,
						sizeof(char)*(j+1))) == NULL){
			    emlwrite("\007Can't realloc description", NULL);
			    rv = -1;
			    goto exit_early;
			}
		    }
		      
		    strcpy(tp->description, comment);
		}
		break;
	    }
	}

	if(tp){
	    bld[bi++] = tp;
	}
	else{
	    if(file[0] != '['){
		if((tp = NewAttach(file, atol(size), comment)) == NULL){
		  rv = -1;
		  goto exit_early;
		}
		bld[bi++] = tp;
	    }
	    else break;
	}

	if(status < 0)
	  tp->flags |= A_ERR;		/* turn ON error bit */
	else
	  tp->flags &= ~(A_ERR);	/* turn OFF error bit */
    }

    if(bi){
	for(i=0; i < bi-1; i++)		/* link together newly built list */
	  bld[i]->next = bld[i+1];

	bld[i]->next = NULL;	        /* tie it off */
	Pmaster->attachments = bld[0];
    }
    else
      Pmaster->attachments = NULL;

exit_early:
    if(knwn){
	for(i = 0; i < ki; i++){	/* kill old/unused references */
	    if(knwn[i]){
		ZotAttach(knwn[i]);
		free((char *) knwn[i]);
	    }
	}
	free((void *)knwn);
    }

    if(bld)
      free((void *)bld);
	
    return(rv);
}




/*
 * ParseAttach - given a header line and an offset into it, return with 
 *		 the three given fields filled in.  Size of fn and cmnt 
 *		 buffers should be passed in fnlen and cmntlen.
 *		 Always updates header fields that have changed or are 
 *		 fixed.  An error advances offset to next attachment.
 *
 *		returns: 1 if a field changed
 *                       0 nothing changed
 *                      -1 on error
 */
ParseAttach(lp, off, fn, fnlen, sz, cmnt, cmntlen, no)
struct hdr_line **lp;				/* current header line      */
int  *off;					/* offset into that line    */
char *fn;                                       /* return file name field   */
int  fnlen;                                     /* fn buffer size           */
char *sz, *cmnt;				/* places to return fields  */
int  cmntlen, *no;				/* attachment number        */
{
    int  j, status, bod, eod = -1,
         rv = 0,				/* return value             */
	 orig_offset,
         lbln  = 0,				/* label'd attachment	    */
	 hibit = 0,
	 quoted = 0,
	 escaped = 0;
    off_t attsz;				/* attachment length        */
    char tmp[1024],
	 c,
	 c_lookahead,
        *p,
        *lblsz = NULL,				/* label'd attchmnt's size  */
         number[8];
    register struct hdr_line  *lprev;
    enum {					/* parse levels             */
	LWS,					/* leading white space      */
	NUMB,					/* attachment number        */
	WSN,					/* white space after number */
	TAG,					/* attachments tag (fname)  */
	WST,					/* white space after tag    */
	ASIZE,					/* attachments size         */
	SWS,					/* white space after size   */
	CMMNT,					/* attachment comment       */
	TG} level;				/* trailing garbage         */

    *fn = *sz = *cmnt = '\0';			/* initialize return strings */
    p   = tmp;
    orig_offset = bod = *off;

    level = LWS;				/* start at beginning */
    while(*lp != NULL){

	if((c=(*lp)->text[*off]) == '\0'){	/* end of display line */
	    if(level == LWS && bod != *off){
	      (*lp)->text[bod] = '\0';
	      rv = 1;
	    }
	    lprev = *lp;
	    if((*lp = (*lp)->next) != NULL)
	      c = (*lp)->text[*off = bod = 0];	/* reset offset */
	}

	if(c != '\0')
	  c_lookahead = (*lp)->text[*off + 1];

	switch(level){
	  case LWS:				/* skip leading white space */
	    if(isspace((unsigned char)c) || c == ','){
		c = ' ';
		break;
	    }
	    
	    if(c == '\0'){
		if(bod > 0 && *off >= bod && lprev){
		    lprev->text[bod - 1] = '\0';
		    rv = 1;
		}
	    }
	    else if(*off > bod && *lp){         /* wipe out whitespace */
		(void)bcopy(&(*lp)->text[*off], &(*lp)->text[bod],
		            strlen(&(*lp)->text[*off]) + 1); 
		*off = bod;	                /* reset pointer */
		rv = 1;
	    }
	    
	    if(c == '\0')
	      break;

	    if(!isdigit((unsigned char)c)){     /* add a number */
		sprintf(number, "%d. ", *no);
		*no = 0;			/* no previous number! */
		sinserts((*lp == NULL) ? &lprev->text[*off]
			               : &(*lp)->text[*off],
			     0, number, j=strlen(number));
		*off += j - 1;
		rv = 1;
		level = TAG;			/* interpret the name */
		break;
	    }
	    level = NUMB;
	  case NUMB:				/* attachment number */
	    if(c == '\0' || c == ','){		/* got to end, no number yet */
		*p = '\0';
		sprintf(number, "%d. ", *no);
		*no = 0;			/* no previous number! */
		if(c == '\0')
		  *lp = lprev;			/* go back and look at prev */

		c = (*lp)->text[*off = orig_offset];	/* reset offset */
		sinserts((*lp == NULL) ? &lprev->text[*off]
			               : &(*lp)->text[*off],
			     0, number, j=strlen(number));
		*off += j - 1;
		rv = 1;
		p = tmp;
		level = WSN;			/* what's next... */
		break;
	    }
	    else if(c == '.' && isspace((unsigned char)c_lookahead)){
					    /* finished grabbing number   */
					    /* if not space is not number */
		/*
		 * replace number if it's not right
		 */
		*p = '\0';
		sprintf(number, "%d", *no);	/* record the current...  */
		*no = atoi(tmp);		/* and the old place in list */
		if(strcmp(number, tmp)){
		    if(p-tmp > *off){		/* where to begin replacemnt */
			j = (p-tmp) - *off;
			sinserts((*lp)->text, *off, "", 0);
			sinserts(&lprev->text[strlen(lprev->text)-j], j, 
				 number, strlen(number));
			*off = 0;
		    }
		    else{
			j = (*off) - (p-tmp);
			sinserts((*lp == NULL) ? &lprev->text[j] 
				               : &(*lp)->text[j], 
				 p-tmp , number, strlen(number));
			*off += strlen(number) - (p-tmp);
		    }
		    rv = 1;
		}

		p = tmp;
		level = WSN;			/* what's next... */
	    }
	    else if(c < '0' || c > '9'){	/* Must be part of tag */
		sprintf(number, "%d. ", *no);
		sinserts((*lp == NULL) ? &lprev->text[(*off) - (p - tmp)]
			               : &(*lp)->text[(*off) - (p - tmp)],
			     0, number, j=strlen(number));
		*off += j;
		level = TAG;			/* interpret the name */
		goto process_tag;	/* in case already past end of tag */
	    }
	    else
	      *p++ = c;

	    break;

	  case WSN:				/* blast whitespace */
	    if(isspace((unsigned char)c) || c == '\0'){
		break;
	    }
	    else if(c == '['){			/* labeled attachment */
		lbln++;
	    }
	    else if(c == ',' || c == ' '){
		emlwrite("\007Attchmnt: '%c' not allowed in file name", 
			  (void *)(int)c);
		rv = -1;
		level = TG;			/* eat rest of garbage */
		break;
	    }
	    level = TAG;
	  case TAG:				/* get and check filename */
						/* or labeled attachment  */
process_tag:					/* enclosed in []         */
	    if(c == '\0'
	       || (lbln && c == ']')
	       || (quoted && p != tmp && c == '\"')
	       || (!(lbln || quoted)
		   && (isspace((unsigned char) c) || strchr(",(\"", c)))){
		if(p == tmp){
		    if(c == '\"')
		      quoted++;
		}
		else{
		    *p = '\0';			/* got something */

		    if (strlen(tmp) > fnlen)
		      emlwrite("File name too big!",NULL);
		    strncpy(fn, tmp, fnlen);	/* store file name */
		    if(!lbln){			/* normal file attachment */
			if((gmode & MDTREE)
			   && !compresspath(opertree, fn, fnlen)){
			    emlwrite(
			    "Attachments allowed only from %s: too many ..'s",
				(gmode&MDSCUR) ? "home directory" : opertree);
			    rv = -1;
			    level = TG;
			    break;
			}
			else{
			    fixpath(fn, fnlen);
			    if((status=fexist(fn, "r", &attsz)) != FIOSUC){
				fioperr(status, fn);
				rv = -1;
				level = TG;	/* munch rest of garbage */
				break;
			    }
			      
			    if((gmode & MDTREE) && !in_oper_tree(fn)){
				emlwrite("\007Attachments allowed only from %s",
				    (gmode&MDSCUR) ? "home directory"
						   : opertree);
				rv = -1;
				level = TG;
				break;
			    }
			}

			if(strcmp(fn, tmp)){ 	/* fn changed: display it */
			    if(*off >=  p - tmp){	/* room for it? */
				sinserts((*lp == NULL)? 
					 &lprev->text[(*off)-(p-tmp)] :
					 &(*lp)->text[(*off)-(p-tmp)],
					 p-tmp, fn, j=strlen(fn));
				*off += j - (p - tmp);	/* advance offset */
				rv = 1;
			    }
			    else{
				emlwrite("\007Attchmnt: Problem displaying real file path", NULL);
			    }
			}
		    }
		    else{			/* labelled attachment! */
			/*
			 * should explain about labelled attachments:
			 * these are attachments that came into the composer
			 * with meaningless file names (up to caller of 
			 * composer to decide), for example, attachments
			 * being forwarded from another message.  here, we
			 * just make sure the size stays what was passed
			 * to us.  The user is SOL if they change the label
			 * since, as it is now, after changed, it will
			 * just get dropped from the list of what gets 
			 * passed back to the caller.
			 */
			PATMT *tp;

			if(c != ']'){		/* legit label? */
			    emlwrite("\007Attchmnt: Expected ']' after \"%s\"",
				     fn);
			    rv = -1;
			    level = TG;
			    break;
			}
			strcat(fn, "]");

			/*
			 * This is kind of cheating since otherwise
			 * ParseAttach doesn't know about the attachment
			 * struct.  OK if filename's not found as it will
			 * get taken care of later...
			 */
			tp = Pmaster->attachments; /* caller check Pmaster! */
			j = 0;
			while(tp != NULL){
			    if(++j == *no){
				lblsz = tp->size;
				break;
			    }
			    tp = tp->next;
			}

			if(tp == NULL){
			    emlwrite("\007Attchmnt: Unknown reference: %s",fn);
			    lblsz =  "XXX";
			}
		    }

		    p = tmp;			/* reset p in tmp */
		    level = WST;
		}

		if(!lbln && c == '(')		/* no space 'tween file, size*/
		  level = ASIZE;
		else if(c == '\0'
			|| (!(lbln || quoted) && (c == ',' || c == '\"'))){
		    strcpy(sz, (lblsz) ? lblsz : prettysz(attsz));
		    sprintf(tmp, " (%s) %s", sz, (c == '\"') ? "" : "\"\"");
		    sinserts((*lp == NULL) ? &lprev->text[*off] 
			                   : &(*lp)->text[*off],
			     0, tmp, j = strlen(tmp));
		    *off += j;
		    rv = 1;
		    level = (c == '\"') ? CMMNT : TG;/* cmnt or eat trash */
		}
	    }
	    else if(!(lbln || quoted)
		    && (c == ',' || c == ' ' || c == '[' || c == ']')){
		emlwrite("\007Attchmnt: '%c' not allowed in file name",
			  (void *)(int)c);
		rv = -1;			/* bad char in file name */
		level = TG;			/* gobble garbage */
	    }
	    else
	      *p++ = c;				/* add char to name */

	    break;

	  case WST:				/* skip white space */
	    if(!isspace((unsigned char)c)){
		/*
		 * whole attachment, comment or done! 
		 */
		if(c == ',' || c == '\0' || c == '\"'){
		    strcpy(sz, (lblsz) ? lblsz : prettysz(attsz));
		    sprintf(tmp, " (%s) %s", sz, 
				           (c == '\"') ? "" : "\"\"");
		    sinserts((*lp == NULL) ? &lprev->text[*off]
				           : &(*lp)->text[*off],
			     0, tmp, j = strlen(tmp));
		    *off += j;
		    rv = 1;
		    level = (c == '\"') ? CMMNT : TG;
		    lbln = 0;			/* reset flag */
		}
		else if(c == '('){		/* get the size */
		    level = ASIZE;
		}
		else{
		    emlwrite("\007Attchmnt: Expected '(' or '\"' after %s",fn);
		    rv = -1;			/* bag it all */
		    level = TG;
		}
	    }
	    break;

	  case ASIZE:				/* check size */
	    if(c == ')'){			/* finished grabbing size */
		*p = '\0';
		/*
		 * replace sizes if they don't match!
		 */
		strcpy(sz, tmp);
		if(strcmp(sz, (lblsz) ? lblsz : prettysz(attsz))){
		    strcpy(sz, (lblsz) ? lblsz : prettysz(attsz));
		    if(p-tmp > *off){		/* where to begin replacemnt */
			j = (p-tmp) - *off;
			sinserts((*lp)->text, *off, "", 0);
			sinserts(&lprev->text[strlen(lprev->text)-j], j, 
				 sz, strlen(sz));
			*off = 0;
		    }
		    else{
			j = (*off) - (p-tmp);
			sinserts((*lp == NULL) ? &lprev->text[j]
				               : &(*lp)->text[j],
				 p-tmp , sz, strlen(sz));
			*off += strlen(sz) - (p-tmp);
		    }
		    rv = 1;
		}

		p = tmp;
		level = SWS;			/* what's next... */
	    }
	    else if(c == '\0' || c == ','){
		*p = '\0';
		emlwrite("\007Attchmnt: Size field missing ')': \"%s\"", tmp);
		rv = -1;
		level = TG;
	    }
	    else
	      *p++ = c;

	    break;

	  case SWS:				/* skip white space */
	    if(!isspace((unsigned char)c)){
		if(c == ','){			/* no description */
		    level = TG;			/* munch rest of garbage */
		    lbln = 0;			/* reset flag */
		}
		else if(c != '\"' && c != '\0'){
		    emlwrite("\007Attchmnt: Malformed comment, quotes required", NULL);
		    rv = -1;
		    level = TG;
		}
		else
		  level = CMMNT;
	    }
	    break;

	  case CMMNT:				/* slurp up comment */
	    if((c == '\"' && !escaped) || c == '\0'){
		*p = '\0';			/* cap it off */
		p = tmp;			/* reset p */
		if (strlen(tmp) > cmntlen){
		  emlwrite("Comment too long!",NULL);
		}

		strncpy(cmnt,tmp,cmntlen-1);	/* copy the comment  */
		if(c == '\0'){
		    emlwrite("\007Attchmnt: Closing quote required at end of comment", NULL);
		    rv = -1;
		}
		level = TG;			/* prepare for next one */
		lbln = 0;			/* reset flag */
	    }
	    else if(c == '\\' && !escaped){	/* something escaped? */
		escaped = 1;
	    }
	    else{
		if(escaped){
		    if(c != '\"')		/* we only quote escapes */
		      *p++ = '\\';

		    escaped = 0;
		}

		if(((*p++ = c) & 0x80) && (gmode & MDHBTIGN) && !hibit++)
		  emlwrite(HIBIT_WARN, NULL);
	    }

	    break;

	  case TG:				/* get comma or final EOL */
	    if(eod < 0) 
	      eod = *off;
	    if(!isspace((unsigned char)c)){
		switch(c){
		  case '\0':
		    if(eod != *off)
		      lprev->text[*off = eod] = '\0';
		    break;
		  case ',':
		    if(eod != *off){
			(void)bcopy(&(*lp)->text[*off], &(*lp)->text[eod],
		                    strlen(&(*lp)->text[*off]) + 1);
			*off = eod;
			rv = 1;
		    }
		    break;
		  default:
		    if(rv != -1)
		      emlwrite("\007Attchmnt: Comma must separate attachments", NULL);
		    rv = -1;
		}
	    }
	    break;

	  default:				/* something's very wrong */
	    emlwrite("\007Attchmnt: Weirdness in ParseAttach", NULL);
	    return(-1);				/* just give up */
	}

	if(c == '\0')				/* we're done */
	  break;

	(*off)++;

	/*
	 * not in comment or label name? done. 
	 */
	if(c == ',' && (level != TAG && level != CMMNT && !lbln))
	  break;				/* put offset past ',' */
    }

    return(rv);
}



/*
 * NewAttach - given a filename (assumed to accessible) and comment, creat
 */
PATMT *NewAttach(f, l, c)
char *f;
long l;
char *c;
{
    PATMT  *tp;

    if((tp=(PATMT *)malloc(sizeof(PATMT))) == NULL){
	emlwrite("No memory to add attachment", NULL);
	return(NULL);
    }
    else
      memset(tp, 0, sizeof(PATMT));

    /* file and size malloc */
    if((tp->filename = (char *)malloc(strlen(f)+1)) == NULL){
	emlwrite("Can't malloc name for attachment", NULL);
	free((char *) tp);
	return(NULL);
    }
    strcpy(tp->filename, f);

    if(l > -1){
	tp->size = (char *)malloc(sizeof(char)*(strlen(prettysz((off_t)l))+1));
	if(tp->size == NULL){
	    emlwrite("Can't malloc size for attachment", NULL);
	    free((char *) tp->filename);
	    free((char *) tp);
	    return(NULL);
	}
	else
	  strcpy(tp->size, prettysz((off_t)l));
    }

    /* description malloc */
    if((tp->description = (char *)malloc(strlen(c)+1)) == NULL){
	emlwrite("Can't malloc description for attachment", NULL);
	free((char *) tp->size);
	free((char *) tp->filename);
	free((char *) tp);
	return(NULL);
    }
    strcpy(tp->description, c);

    /* callback to show user the mime type that will be used for attachment */
    if(Pmaster->mimetype  && (*Pmaster->mimetype)(f) > 0){
	int rv ;
	
	clearcursor();
	mlerase();
	rv = (*Pmaster->showmsg)('x');
	ttresize();
	picosigs();
	if(rv)			/* Did showmsg corrupt the screen? */
	  PaintBody(0);		/* Yes, repaint it */
	mpresf = 1;
    }

    return(tp);
}



/*
 * AttachError - Sniff list of attachments, returning TRUE if there's
 *               any sign of trouble...
 */
int
AttachError()
{
    PATMT *ap;

    if(!Pmaster)
      return(0);

    ap = Pmaster->attachments;
    while(ap){
	if((ap->flags) & A_ERR)
	  return(1);

	ap = ap->next;
    }

    return(FALSE);
}


char *
QuoteAttach(fn)
char *fn;
{
    char *p;

    if(*fn && strpbrk(fn, " \t,(\"")){		/* Quote it? */
	p = &fn[strlen(fn)];
	*(p+2) = '\0';
	*(p+1) = '\"';
	do
	  *p = *(p-1);
	while(--p != fn);
	*p = '\"';
    }

    return(fn);
}



void ZotAttach(p)
PATMT *p;
{
    if(!p)
      return;
    if(p->description)
      free((char *)p->description);
    if(p->filename){
	if(p->flags & A_TMP)
	  unlink(p->filename);

	free((char *)p->filename);
    }
    if(p->size)
      free((char *)p->size);
    if(p->id)
      free((char *)p->id);
    p->next = NULL;
}
#endif	/* ATTACHMENTS */


/*
 * intag - return TRUE if i is in a column that makes up an
 *         attachment line number
 */
intag(s, i)
char *s;
int   i;
{
    char *p = s;
    int n = 0;

    while(*p != '\0' && (p-s) < 5){		/* is there a tag? it */
	if(n && *p == '.')			/* can't be more than 4 */
	  return(i <= p-s);				/* chars long! */

	if(*p < '0' || *p > '9')
	  break;
	else
	  n = (n * 10) + (*p - '0');

	p++;
    }

    return(FALSE);
}


/*
 * prettysz - return pointer to string containing nice
 */
char *prettysz(l)
    off_t l;
{
    static char b[32];
    long sz, left, right;

    sz = (long) l;
    b[0] = '\0';

    if(sz < 1000L){
	sprintf(b, "%ld B", sz);				/* xxx B */
    }
    else if(sz < 9950L){
	left = (sz + 50L) / 1000L;
	right = ((sz + 50L) - left * 1000L) / 100L;
	sprintf(b, "%ld.%ld KB", left, right);			/* x.x KB */
    }
    else if(sz < 999500L){
	sprintf(b, "%ld KB", (sz + 500L) / 1000L);		/* xxx KB */
    }
    else if(sz <  9950000L){
	left = (sz + 50000L) / 1000000L;
	right = ((sz + 50000L) - left * 1000000L) / 100000L;
	sprintf(b, "%ld.%ld MB", left, right);			/* x.x MB */
    }
    else{
	sprintf(b, "%ld MB", (sz + 500000L) / 1000000L);	/* xxx MB */
    }

    return(b);
}


/*
 * sinserts - s insert into another string
 */
void
sinserts(ds, dl, ss, sl)
    char *ds, *ss;				/* dest. and source strings */
    int   dl, sl;				/* their lengths */
{
    char *dp, *edp;				/* pointers into dest. */
    int  j;					/* jump difference */

    if(sl >= dl){				/* source bigger than dest. */
	dp = ds + dl;				/* shift dest. to make room */
	if((edp = strchr(dp, '\0')) != NULL){
	    j = sl - dl;

	    for( ;edp >= dp; edp--)
	      edp[j] = *edp;

	    while(sl--)
	       *ds++ = *ss++;
	}
	else
	  emlwrite("\007No end of line???", NULL);	/* can this happen? */
    }
    else{					/* dest is longer, shrink it */
	j = dl - sl;				/* difference in lengths */

	while(sl--)				/* copy ss onto ds */
	  *ds++ = *ss++;

	if(strlen(ds) > j){			/* shuffle the rest left */
	    do
	      *ds = ds[j];
	    while(*ds++ != '\0');
	}
	else
	  *ds = '\0';
    }
}
