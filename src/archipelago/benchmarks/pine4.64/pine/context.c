#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: context.c 13727 2004-07-01 17:27:20Z hubert $";
#endif
/*
 * Program:	Mailbox Context Management
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
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
 *
 * USENET News reading additions in part by L Lundblade / NorthWestNet, 1993
 * lgl@nwnet.net
 */

#include "headers.h"


char    *context_percent_quote PROTO((char *));


/* Context Manager context format digester
 * Accepts: context string and buffers for sprintf-suitable context,
 *          remote host (for display), remote context (for display), 
 *          and view string
 * Returns: NULL if successful, else error string
 *
 * Comments: OK, here's what we expect a fully qualified context to 
 *           look like:
 *
 * [*] [{host ["/"<proto>] [:port]}] [<cntxt>] "[" [<view>] "]" [<cntxt>]
 *
 *         2) It's understood that double "[" or "]" are used to 
 *            quote a literal '[' or ']' in a context name.
 *
 *         3) an empty view in context implies a view of '*', so that's 
 *            what get's put in the view string
 *
 */
char *
context_digest (context, scontext, host, rcontext, view, len)
    char *context, *scontext, *host, *rcontext, *view;
    size_t len;
{
    char *p, *viewp = view, tmp[MAILTMPLEN];
    char *scontextp = scontext;
    int   i = 0;

    if((p = context) == NULL || *p == '\0'){
	if(scontext)			/* so the caller can apply */
	  strcpy(scontext, "%s");	/* folder names as is.     */

	return(NULL);			/* no error, just empty context */
    }

    if(!rcontext && scontext)		/* place to collect rcontext ? */
      rcontext = tmp;

    /* find hostname if requested and exists */
    if(*p == '{' || (*p == '*' && *++p == '{')){
	for(p++; *p && *p != '}' ; p++)
	  if(host && p-context < len-1)
	    *host++ = *p;		/* copy host if requested */

	while(*p && *p != '}')		/* find end of imap host */
	  p++;

	if(*p == '\0')
	  return("Unbalanced '}'");	/* bogus. */
	else
	  p++;				/* move into next field */
    }

    for(; *p ; p++){			/* get thru context */
	if(rcontext && i < len-1)
	  rcontext[i] = *p;		/* copy if requested */
	
	i++;

	if(*p == '['){			/* done? */
	    if(*++p == '\0')		/* look ahead */
	      return("Unbalanced '['");

	    if(*p != '[')		/* not quoted: "[[" */
	      break;
	}
    }

    if(*p == '\0')
      return("No '[' in context");

    for(; *p ; p++){			/* possibly in view portion ? */
	if(*p == ']'){
	    if(*(p+1) == ']')		/* is quoted */
	      p++;
	    else
	      break;
	}

	if(viewp && viewp-view < len-1)
	  *viewp++ = *p;
    }

    if(*p != ']')
      return("No ']' in context");

    for(; *p ; p++){			/* trailing context ? */
	if(rcontext && i < len-1)
	  rcontext[i] = *p;
	i++;
    }

    if(host) *host = '\0';
    if(rcontext && i < len) rcontext[i] = '\0';
    if(viewp) {
/* MAIL_LIST: dealt with it in new_context since could be either '%' or '*'
	if(viewp == view && viewp-view < len-1)
	  *viewp++ = '*';
*/

	*viewp = '\0';
    }

    if(scontextp){			/* sprint'able context request ? */
	if(*context == '*'){
	    if(scontextp-scontext < len-1)
	      *scontextp++ = *context;

	    context++;
	}

	if(*context == '{'){
	    while(*context && *context != '}'){
		if(scontextp-scontext < len-1)
		  *scontextp++ = *context;
		
		context++;
	    }

	    *scontextp++ = '}';
	}

	for(p = rcontext; *p ; p++){
	    if(*p == '[' && *(p+1) == ']'){
		if(scontextp-scontext < len-2){
		    *scontextp++ = '%';	/* replace "[]" with "%s" */
		    *scontextp++ = 's';
		}

		p++;			/* skip ']' */
	    }
	    else if(scontextp-scontext < len-1)
	      *scontextp++ = *p;
	}

	*scontextp = '\0';
    }

    return(NULL);			/* no problems to report... */
}


/* Context Manager apply name to context
 * Accepts: buffer to write, context to apply, ambiguous folder name
 * Returns: buffer filled with fully qualified name in context
 *          No context applied if error
 */
char *
context_apply(b, c, name, len)
    char      *b;
    CONTEXT_S *c;
    char      *name;
    size_t     len;
{
    if(!c || IS_REMOTE(name) ||
       (!IS_REMOTE(c->context) && is_absolute_path(name))){
	strncpy(b, name, len-1);			/* no context! */
    }
    else if(name[0] == '#'){
	if(IS_REMOTE(c->context)){
	    char *p = strchr(c->context, '}');	/* name specifies namespace */
	    sprintf(b, "%.*s", min(p - c->context + 1, len-1), c->context);
	    b[min(p - c->context + 1, len-1)] = '\0';
	    sprintf(b+strlen(b), "%.*s", len-1-strlen(b), name);
	}
	else{
	    strncpy(b, name, len-1);
	}
    }
    else if(c->dir && c->dir->ref){		/* has reference string! */
	sprintf(b, "%.*s", len-1, c->dir->ref);
	b[len-1] = '\0';
	sprintf(b+strlen(b), "%.*s", len-1-strlen(b), name);
    }
    else{					/* no ref, apply to context */
	char *pq = NULL;

	/*
	 * Have to quote %s for the sprintf because we're using context
	 * as a format string.
	 */
	pq = context_percent_quote(c->context);

	if(strlen(c->context) + strlen(name) < len)
	  sprintf(b, pq, name);
	else{
	    char *t;

	    t = (char *)fs_get((strlen(pq)+strlen(name))*sizeof(char));
	    sprintf(t, pq, name);
	    strncpy(b, t, len-1);
	    fs_give((void **)&t);
	}

	if(pq)
	  fs_give((void **) &pq);
    }

    b[len-1] = '\0';
    return(b);
}


/*
 * Insert % before existing %'s so printf will print a real %.
 * This is a special routine just for contexts. It only does the % stuffing
 * for %'s inside of the braces of the context name, not for %'s to
 * the right of the braces, which we will be using for printf format strings.
 * Returns a malloced string which the caller is responsible for.
 */
char *
context_percent_quote(context)
    char *context;
{
    char *pq = NULL;

    if(!context || !*context)
      pq = cpystr("");
    else{
	if(IS_REMOTE(context)){
	    char *end, *p, *q;

	    /* don't worry about size efficiency, just allocate double */
	    pq = (char *) fs_get((2*strlen(context) + 1) * sizeof(char));

	    end = strchr(context, '}');
	    p = context;
	    q = pq;
	    while(*p){
		if(*p == '%' && p < end)
		  *q++ = '%';
		
		*q++ = *p++;
	    }

	    *q = '\0';
	}
	else
	  pq = cpystr(context);
    }

    return(pq);
}


/* Context Manager check if name is ambiguous
 * Accepts: candidate string
 * Returns: T if ambiguous, NIL if fully-qualified
 */

int
context_isambig (s)
    char *s;
{
    return(!(*s == '{' || *s == '#'));
}

/* Context Manager create mailbox
 * Accepts: context
 *	    mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long
context_create (context, stream, mailbox)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *mailbox;
{
  char tmp[MAILTMPLEN];		/* must be within context */

  return(cntxt_allowed(context_apply(tmp, context, mailbox, sizeof(tmp)))
	 ? pine_mail_create(stream,tmp) : 0L);
}


/* Context Manager open
 * Accepts: context
 *	    candidate stream for recycling
 *	    mailbox name
 *	    open options
 * Returns: stream to use on success, NIL on failure
 */

MAILSTREAM *
context_open (context, old, name, opt, retflags)
    CONTEXT_S  *context;
    MAILSTREAM *old;
    char       *name;
    long	opt;
    long       *retflags;
{
  char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

  if(!cntxt_allowed(context_apply(tmp, context, name, sizeof(tmp)))){
      /*
       * If a stream is passed to context_open, we will either re-use
       * it or close it.
       */
      if(old)
	pine_mail_close(old);

      return((MAILSTREAM *)NULL);
  }

  return(pine_mail_open(old, tmp, opt, retflags));
}


/* Context Manager status
 * Accepts: context
 *	    candidate stream for recycling
 *	    mailbox name
 *	    open options
 * Returns: T if call succeeds, NIL on failure
 */

long
context_status (context, stream, name, opt)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *name;
    long	opt;
{
    return(context_status_full(context, stream, name, opt, NULL, NULL));
}


long
context_status_full (context, stream, name, opt, uidvalidity, uidnext)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *name;
    long	opt;
    unsigned long *uidvalidity, *uidnext;
{
    char tmp[MAILTMPLEN];	/* build FQN from ambiguous name */
    long flags = opt;

    return(cntxt_allowed(context_apply(tmp, context, name, sizeof(tmp)))
	    ? pine_mail_status_full(stream,tmp,flags,uidvalidity,uidnext) : 0L);
}


/* Context Manager status
 *
 * This is very similar to context_status. Instead of a stream pointer we
 * receive a pointer to a pointer so that we can return a stream that we
 * opened for further use by the caller.
 *
 * Accepts: context
 *	    candidate stream for recycling
 *	    mailbox name
 *	    open options
 * Returns: T if call succeeds, NIL on failure
 */

long
context_status_streamp (context, streamp, name, opt)
    CONTEXT_S   *context;
    MAILSTREAM **streamp;
    char        *name;
    long	 opt;
{
    return(context_status_streamp_full(context,streamp,name,opt,NULL,NULL));
}


long
context_status_streamp_full (context, streamp, name, opt, uidvalidity, uidnext)
    CONTEXT_S   *context;
    MAILSTREAM **streamp;
    char        *name;
    long	 opt;
    unsigned long *uidvalidity, *uidnext;
{
    MAILSTREAM *stream;
    char        tmp[MAILTMPLEN];	/* build FQN from ambiguous name */

    if(!cntxt_allowed(context_apply(tmp, context, name, sizeof(tmp))))
      return(0L);

    if(!streamp)
      stream = NULL;
    else{
	if(!*streamp && IS_REMOTE(tmp)){
	    *streamp = pine_mail_open(NULL, tmp,
				      OP_SILENT|OP_HALFOPEN|SP_USEPOOL, NULL);
	}

	stream = *streamp;
    }

    return(pine_mail_status_full(stream, tmp, opt, uidvalidity, uidnext));
}


/* Context Manager rename
 * Accepts: context
 *	    mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long
context_rename (context, stream, old, new)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *old, *new;
{
    char tmp[MAILTMPLEN],tmp2[MAILTMPLEN];

    return((cntxt_allowed(context_apply(tmp, context, old, sizeof(tmp)))
	    && cntxt_allowed(context_apply(tmp2, context, new, sizeof(tmp2))))
	     ? pine_mail_rename(stream,tmp,tmp2) : 0L);
}


MAILSTREAM *
context_already_open_stream(context, name, flags)
    CONTEXT_S  *context;
    char       *name;
    int         flags;
{
    char tmp[MAILTMPLEN];

    return(already_open_stream(context_apply(tmp, context, name, sizeof(tmp)),
			       flags));
}


/* Context Manager delete mailbox
 * Accepts: context
 *	    mail stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long
context_delete (context, stream, name)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *name;
{
    char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    return(cntxt_allowed(context_apply(tmp, context, name, sizeof(tmp)))
	     ? pine_mail_delete(stream, tmp) : 0L);
}


/* Context Manager append message string
 * Accepts: context
 *	    mail stream
 *	    destination mailbox
 *	    stringstruct of message to append
 * Returns: T on success, NIL on failure
 */

long
context_append (context, stream, name, msg)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *name;
    STRING     *msg;
{
    char tmp[MAILTMPLEN];	/* build FQN from ambiguous name */

    return(cntxt_allowed(context_apply(tmp, context, name, sizeof(tmp)))
	     ? pine_mail_append(stream, tmp, msg) : 0L);
}


/* Context Manager append message string with flags
 * Accepts: context
 *	    mail stream
 *	    destination mailbox
 *          flags to assign message being appended
 *          date of message being appended
 *	    stringstruct of message to append
 * Returns: T on success, NIL on failure
 */

long
context_append_full (context, stream, name, flags, date, msg)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *name, *flags, *date;
    STRING     *msg;
{
    char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    return(cntxt_allowed(context_apply(tmp, context, name, sizeof(tmp)))
	     ? pine_mail_append_full(stream, tmp, flags, date, msg) : 0L);
}


/* Context Manager append multiple message
 * Accepts: context
 *	    mail stream
 *	    destination mailbox
 *	    append data callback
 *	    arbitrary ata for callback use
 * Returns: T on success, NIL on failure
 */

long
context_append_multiple (context, stream, name, af, data, not_this_stream)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *name;
    append_t   af;
    void       *data;
    MAILSTREAM *not_this_stream;
{
    char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    return(cntxt_allowed(context_apply(tmp, context, name, sizeof(tmp)))
     ? pine_mail_append_multiple(stream, tmp, af, data, not_this_stream) : 0L);
}


/* Mail copy message(s)
 * Accepts: context
 *	    mail stream
 *	    sequence
 *	    destination mailbox
 */

long
context_copy (context, stream, sequence, name)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char *sequence;
    char *name;
{
    char *s, tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    if(context_apply(tmp, context, name, sizeof(tmp))[0] == '{'){
	if(s = strindex(tmp, '}'))
	  s++;
	else
	  return(0L);
    }
    else
      s = tmp;

    if(!*s)
      strcpy(s = tmp, "INBOX");		/* presume "inbox" ala c-client */

    return(cntxt_allowed(s) ? pine_mail_copy(stream, sequence, s) : 0L);
}


/*
 * Context manager stream usefulness test
 * Accepts: context
 *	    mail name
 *	    mailbox name
 *	    mail stream to test against mailbox name
 * Returns: stream if useful, else NIL
 */
MAILSTREAM *
context_same_stream(context, name, stream)
    CONTEXT_S  *context;
    MAILSTREAM *stream;
    char       *name;
{
    extern MAILSTREAM *same_stream();
    char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    return(same_stream(context_apply(tmp, context, name, sizeof(tmp)), stream));
}
