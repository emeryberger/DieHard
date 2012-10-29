#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: reply.c 14081 2005-09-12 22:04:25Z hubert@u.washington.edu $";
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
    reply.c
   
   Code here for forward and reply to mail
   A few support routines as well

  This code will forward and reply to MIME messages. The Pine composer
at this time will only support non-text segments at the end of a
message so, things don't always come out as one would like. If you
always forward a message in MIME format, all will be correct.  Forwarding
of nested MULTIPART messages will work.  There's still a problem with
MULTIPART/ALTERNATIVE as the "first text part" rule doesn't allow modifying
the equivalent parts.  Ideally, we should probably such segments as a 
single attachment when forwarding/replying.  It would also be real nice to
flatten out the nesting in the composer so pieces inside can get snipped.

The evolution continues...

  =====*/


#include "headers.h"



/*
 * Internal Prototypes
 */
int	 reply_body_text PROTO((BODY *, BODY **));
void	 reply_forward_header PROTO((MAILSTREAM *, long, char *,
				     ENVELOPE *, gf_io_t, char *));
char	*reply_signature PROTO((ACTION_S *, ENVELOPE *, REDRAFT_POS_S **,
				int *));
void	 reply_append_addr PROTO((ADDRESS **, ADDRESS *));
ADDRESS *reply_resent PROTO((struct pine *, long, char *));
void	 reply_fish_personal PROTO((ENVELOPE *, ENVELOPE *));
int	 addr_in_env PROTO((ADDRESS *, ENVELOPE *));
int	 addr_lists_same PROTO((ADDRESS *, ADDRESS *));
int	 reply_poster_followup PROTO((ENVELOPE *));
void	 forward_delimiter PROTO((gf_io_t));
BODY	 *forward_multi_alt PROTO((MAILSTREAM *, ENVELOPE *, BODY *,
				   long, char *, void *, gf_io_t, int));
void	 bounce_mask_header PROTO((char **, char *));
int      post_quote_space PROTO((long, char *, LT_INS_S **, void *));
int	 quote_fold PROTO((long, char *, LT_INS_S **, void *));
int	 twsp_strip PROTO((long, char *, LT_INS_S **, void *));
int	 sigdash_strip PROTO((long, char *, LT_INS_S **, void *));
int      sigdashes_are_present PROTO((char *));
char	*sigedit_exit_for_pico PROTO((struct headerentry *, void (*)(), int));
char    *get_reply_data PROTO((ENVELOPE *, ACTION_S *, IndexColType,
			       char *, size_t));
void     get_addr_data PROTO((ENVELOPE *, IndexColType, char *, size_t));
void     get_news_data PROTO((ENVELOPE *, IndexColType, char *, size_t));
char    *pico_sendexit_for_roles PROTO((struct headerentry *, void(*)()));
char    *pico_cancelexit_for_roles PROTO((void (*)()));
char    *detoken_guts PROTO((char *, int, ENVELOPE *, ACTION_S *,
			     REDRAFT_POS_S **, int, int *));
char    *get_signature_lit PROTO((char *, int, int, int, int));
void     a_little_addr_string PROTO((ADDRESS *, char *, size_t));
char    *handle_if_token PROTO((char *, char *, int, ENVELOPE *,
				ACTION_S *, char **));
char    *get_token_arg PROTO((char *, char **));
int      reply_quote_str_contains_tokens PROTO((void));
char    *rot13 PROTO((char *));



/*
 * Little defs to keep the code a bit neater...
 */
#define	FRM_PMT	"Use \"Reply-To:\" address instead of \"From:\" address"
#define	ALL_PMT		"Reply to all recipients"
#define	NEWS_PMT	"Follow-up to news group(s), Reply via email to author or Both? "


/*
 * standard type of storage object used for body parts...
 */
#if	defined(DOS) && !defined(WIN32)
#define		  PART_SO_TYPE	TmpFileStar
#else
#define		  PART_SO_TYPE	CharStar
#endif



/*----------------------------------------------------------------------
        Fill in an outgoing message for reply and pass off to send

    Args: pine_state -- The usual pine structure

  Result: Reply is formatted and passed off to composer/mailer

Reply

   - put senders address in To field
   - search to and cc fields to see if we aren't the only recipients
   - if other than us, ask if we should reply to all.
   - if answer yes, fill out the To and Cc fields
   - fill in the fcc argument
   - fill in the subject field
   - fill out the body and the attachments
   - pass off to pine_send()
  ---*/
void
reply(pine_state, role_arg)
     struct pine *pine_state;
     ACTION_S    *role_arg;
{
    ADDRESS    *saved_from, *saved_to, *saved_cc, *saved_resent;
    ENVELOPE   *env, *outgoing;
    BODY       *body, *orig_body = NULL;
    REPLY_S     reply;
    void       *msgtext = NULL;
    char       *tmpfix = NULL, *prefix = NULL;
    long        msgno, j, totalm, rflags, *seq = NULL;
    int         i, include_text = 0, times = -1, warned = 0,
		flags = RSF_QUERY_REPLY_ALL, reply_raw_body = 0;
    gf_io_t     pc;
    PAT_STATE   dummy;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S   *role = NULL, *nrole;
    BUILDER_ARG fcc;
#if	defined(DOS) && !defined(_WINDOWS)
    char *reserve;
#endif

    outgoing = mail_newenvelope();
    memset((void *)&fcc, 0, sizeof(BUILDER_ARG));
    totalm   = mn_total_cur(pine_state->msgmap);
    seq	     = (long *)fs_get(((size_t)totalm + 1) * sizeof(long));

    dprint(4, (debugfile,"\n - reply (%s msgs) -\n", comatose(totalm)));

    saved_from		  = (ADDRESS *) NULL;
    saved_to		  = (ADDRESS *) NULL;
    saved_cc		  = (ADDRESS *) NULL;
    saved_resent	  = (ADDRESS *) NULL;
    outgoing->subject	  = NULL;

    memset((void *)&reply, 0, sizeof(reply));

    if(ps_global->full_header == 2
       && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
      reply_raw_body = 1;

    /*
     * We may have to loop through first to figure out what default
     * reply-indent-string to offer...
     */
    if(mn_total_cur(pine_state->msgmap) > 1 &&
       F_ON(F_ENABLE_EDIT_REPLY_INDENT, pine_state) &&
       reply_quote_str_contains_tokens()){
	for(msgno = mn_first_cur(pine_state->msgmap);
	    msgno > 0L && !tmpfix;
	    msgno = mn_next_cur(pine_state->msgmap)){

	    env = pine_mail_fetchstructure(pine_state->mail_stream,
					   mn_m2raw(pine_state->msgmap, msgno),
					   NULL);
	    if(!env) {
		q_status_message1(SM_ORDER,3,4,
			    "Error fetching message %.200s. Can't reply to it.",
				long2string(msgno));
		goto done_early;
	    }

	    if(!tmpfix){			/* look for prefix? */
		tmpfix = reply_quote_str(env);
		if(prefix){
		    i = strcmp(tmpfix, prefix);
		    fs_give((void **) &tmpfix);
		    if(i){		/* don't check back if dissimilar */
			fs_give((void **) &prefix);
			/*
			 * We free prefix, not tmpfix. We set tmpfix to prefix
			 * so that we won't come check again.
			 */
			tmpfix = prefix = cpystr("> ");
		    }
		}
		else{
		    prefix = tmpfix;
		    tmpfix = NULL;		/* check back later? */
		}
	    }
	}

	tmpfix = prefix;
    }

    /*
     * Loop thru the selected messages building the
     * outgoing envelope's destinations...
     */
    for(msgno = mn_first_cur(pine_state->msgmap);
	msgno > 0L;
	msgno = mn_next_cur(pine_state->msgmap)){

	/*--- Grab current envelope ---*/
	env = pine_mail_fetchstructure(pine_state->mail_stream,
			    seq[++times] = mn_m2raw(pine_state->msgmap, msgno),
			    NULL);
	if(!env) {
	    q_status_message1(SM_ORDER,3,4,
			  "Error fetching message %.200s. Can't reply to it.",
			      long2string(msgno));
	    goto done_early;
	}

	/*
	 * We check for the prefix here if we didn't do it in the first
	 * loop above. This is just to save having to go through the loop
	 * twice in the cases where we don't need to.
	 */
	if(!tmpfix){
	    tmpfix = reply_quote_str(env);
	    if(prefix){
		i = strcmp(tmpfix, prefix);
		fs_give((void **) &tmpfix);
		if(i){			/* don't check back if dissimilar */
		    fs_give((void **) &prefix);
		    tmpfix = prefix = cpystr("> ");
		}
	    }
	    else{
		prefix = tmpfix;
		tmpfix = NULL;		/* check back later? */
	    }
	}

	/*
	 * For consistency, the first question is always "include text?"
	 */
	if(!times){		/* only first time */
	    char *p = cpystr(prefix);

	    if((include_text=reply_text_query(pine_state,totalm,&prefix)) < 0)
	      goto done_early;
	    
	    /* edited prefix? */
	    if(strcmp(p, prefix))
	      tmpfix = prefix;		/* stop looking */
	    
	    fs_give((void **)&p);
	}
	
	/*
	 * If we're agg-replying or there's a newsgroup and the user want's
	 * to post to news *and* via email, add relevant addresses to the
	 * outgoing envelope...
	 *
	 * The single message case get's us around the aggregate reply
	 * to messages in a mixed mail-news archive where some might
	 * have newsgroups and others not or whatever.
	 */
	if(totalm > 1L || ((i = reply_news_test(env, outgoing)) & 1)){
	    if(totalm > 1)
	      flags |= RSF_FORCE_REPLY_TO;

	    if(!reply_harvest(pine_state, seq[times], NULL, env,
			      &saved_from, &saved_to, &saved_cc,
			      &saved_resent, &flags))
	      goto done_early;
	}
	else if(i == 0)
	  goto done_early;

	/*------------ Format the subject line ---------------*/
	if(outgoing->subject){
	    /*
	     * if reply to more than one message, and all subjects
	     * match, so be it.  otherwise set it to something generic...
	     */
	    if(strucmp(outgoing->subject,
		       reply_subject(env->subject,tmp_20k_buf,SIZEOF_20KBUF))){
		fs_give((void **)&outgoing->subject);
		outgoing->subject = cpystr("Re: several messages");
	    }
	}
	else
	  outgoing->subject = reply_subject(env->subject, NULL, 0);
    }


    reply_seed(pine_state, outgoing, env, saved_from,
	       saved_to, saved_cc, saved_resent,
	       &fcc, flags & RSF_FORCE_REPLY_ALL);

    if(sp_expunge_count(pine_state->mail_stream))	/* cur msg expunged */
      goto done_early;

    /* Setup possible role */
    if(role_arg)
      role = copy_action(role_arg);

    if(!role){
	rflags = ROLE_REPLY;
	if(nonempty_patterns(rflags, &dummy)){
	    /* setup default role */
	    nrole = NULL;
	    j = mn_first_cur(pine_state->msgmap);
	    do {
		role = nrole;
		nrole = set_role_from_msg(pine_state, rflags,
					  mn_m2raw(pine_state->msgmap, j),
					  NULL);
	    } while(nrole && (!role || nrole == role)
		    && (j=mn_next_cur(pine_state->msgmap)) > 0L);

	    if(!role || nrole == role)
	      role = nrole;
	    else
	      role = NULL;

	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{				/* cancel reply */
		role = NULL;
		cmd_cancelled("Reply");
		goto done_early;
	    }
	}
    }

    /*
     * Reply_seed may call c-client in get_fcc_based_on_to, so env may
     * no longer be valid. Get it again.
     * Similarly for set_role_from_message.
     */
    env = pine_mail_fetchstructure(pine_state->mail_stream, seq[times], NULL);

    if(role){
	q_status_message1(SM_ORDER, 3, 4,
			  "Replying using role \"%.200s\"", role->nick);

	/* override fcc gotten in reply_seed */
	if(role->fcc){
	    if(fcc.tptr)
	      fs_give((void **)&fcc.tptr);

	    memset((void *)&fcc, 0, sizeof(BUILDER_ARG));
	}
    }

    seq[++times] = -1L;		/* mark end of sequence list */

    /*==========  Other miscelaneous fields ===================*/   
    outgoing->in_reply_to = reply_in_reply_to(env);
    outgoing->references = reply_build_refs(env);
    outgoing->message_id = generate_message_id();

    if(!outgoing->to &&
       !outgoing->cc &&
       !outgoing->bcc &&
       !outgoing->newsgroups)
      q_status_message(SM_ORDER | SM_DING, 3, 6,
		       "Warning: no valid addresses to reply to!");

#if	defined(DOS) && !defined(_WINDOWS)
#if	defined(LWP) || defined(PCTCP) || defined(PCNFS)
#define	IN_RESERVE	8192
#else
#define	IN_RESERVE	16384
#endif
    if((reserve=(char *)malloc(IN_RESERVE)) == NULL){
        q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Insufficient memory for message text");
	goto done_early;
    }
#endif

   /*==================== Now fix up the message body ====================*/

    /* 
     * create storage object to be used for message text
     */
    if((msgtext = (void *)so_get(PicoText, NULL, EDIT_ACCESS)) == NULL){
        q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Error allocating message text");
        goto done_early;
    }

    gf_set_so_writec(&pc, (STORE_S *) msgtext);

    /*---- Include the original text if requested ----*/
    if(include_text && totalm > 1L){
	char *sig;
	int   impl, template_len = 0, leave_cursor_at_top = 0;


	env = NULL;
        if(role && role->template){
	    char *filtered;

	    impl = 0;
	    filtered = detoken(role, env, 0,
			       F_ON(F_SIG_AT_BOTTOM, ps_global) ? 1 : 0,
			       0, &redraft_pos, &impl);
	    if(filtered){
		if(*filtered){
		    so_puts((STORE_S *)msgtext, filtered);
		    if(impl == 1)
		      template_len = strlen(filtered);
		    else if(impl == 2)
		      leave_cursor_at_top++;
		}

		fs_give((void **)&filtered);
	    }
	    else
	      impl = 1;
	}
	else
	  impl = 1;

	if((sig = reply_signature(role, env, &redraft_pos, &impl)) &&
	   F_OFF(F_SIG_AT_BOTTOM, ps_global)){

	    /*
	     * If CURSORPOS was set explicitly in sig_file, and there was a
	     * template file before that, we need to adjust the offset by the
	     * length of the template file. However, if the template had
	     * a set CURSORPOS in it then impl was 2 before getting to the
	     * signature, so offset wouldn't have been reset by the signature
	     * CURSORPOS and offset would already be correct. That case will
	     * be ok here because template_len will be 0 and adding it does
	     * nothing. If template
	     * didn't have CURSORPOS in it, then impl was 1 and got set to 2
	     * by the CURSORPOS in the sig. In that case we have to adjust the
	     * offset. That's what the next line does. It adjusts it if
	     * template_len is nonzero and if CURSORPOS was set in sig_file.
	     */
	    if(impl == 2)
	      redraft_pos->offset += template_len;
	    
	    if(*sig)
	      so_puts((STORE_S *)msgtext, sig);
	    
	    /*
	     * Set sig to NULL because we've already used it. If SIG_AT_BOTTOM
	     * is set, we won't have used it yet and want it to be non-NULL.
	     */
	    fs_give((void **)&sig);
	}

	/*
	 * Only put cursor in sig if there is a cursorpos there but not
	 * one in the template, and sig-at-bottom.
	 */
	 if(!(sig && impl == 2 && !leave_cursor_at_top))
	   leave_cursor_at_top++;

	body                  = mail_newbody();
	body->type            = TYPETEXT;
	body->contents.text.data = msgtext;

	for(msgno = mn_first_cur(pine_state->msgmap);
	    msgno > 0L;
	    msgno = mn_next_cur(pine_state->msgmap)){

	    if(env){			/* put 2 between messages */
		gf_puts(NEWLINE, pc);
		gf_puts(NEWLINE, pc);
	    }

	    /*--- Grab current envelope ---*/
	    env = pine_mail_fetchstructure(pine_state->mail_stream,
					   mn_m2raw(pine_state->msgmap, msgno),
					   &orig_body);
	    if(!env){
		q_status_message1(SM_ORDER,3,4,
			  "Error fetching message %.200s. Can't reply to it.",
			      long2string(mn_get_cur(pine_state->msgmap)));
		goto done_early;
	    }

	    if(orig_body == NULL || orig_body->type == TYPETEXT || reply_raw_body) {
		reply_delimiter(env, role, pc);
		if(F_ON(F_INCLUDE_HEADER, pine_state))
		  reply_forward_header(pine_state->mail_stream,
				       mn_m2raw(pine_state->msgmap,msgno),
				       NULL, env, pc, prefix);

		get_body_part_text(pine_state->mail_stream, reply_raw_body ? NULL : orig_body,
				   mn_m2raw(pine_state->msgmap, msgno),
				   reply_raw_body ? NULL : "1", pc, prefix);
	    }
	    else if(orig_body->type == TYPEMULTIPART) {
		if(!warned++)
		  q_status_message(SM_ORDER,3,7,
		      "WARNING!  Attachments not included in multiple reply.");

		if(orig_body->nested.part
		   && orig_body->nested.part->body.type == TYPETEXT) {
		    /*---- First part of the message is text -----*/
		    reply_delimiter(env, role, pc);
		    if(F_ON(F_INCLUDE_HEADER, pine_state))
		      reply_forward_header(pine_state->mail_stream,
					   mn_m2raw(pine_state->msgmap,
						    msgno),
					   NULL, env, pc, prefix);

		    get_body_part_text(pine_state->mail_stream,
				       &orig_body->nested.part->body,
				       mn_m2raw(pine_state->msgmap, msgno),
				       "1", pc, prefix);
		}
		else{
		    q_status_message(SM_ORDER,0,3,
				     "Multipart with no leading text part.");
		}
	    }
	    else{
		/*---- Single non-text message of some sort ----*/
		q_status_message(SM_ORDER,3,3,
				 "Non-text message not included.");
	    }
	}

	if(!leave_cursor_at_top){
	    long  cnt = 0L;
	    unsigned char c;

	    /* rewind and count chars to start of sig file */
	    so_seek((STORE_S *)msgtext, 0L, 0);
	    while(so_readc(&c, (STORE_S *)msgtext))
	      cnt++;

	    if(!redraft_pos){
		redraft_pos = (REDRAFT_POS_S *)fs_get(sizeof(*redraft_pos));
		memset((void *)redraft_pos, 0,sizeof(*redraft_pos));
		redraft_pos->hdrname = cpystr(":");
	    }

	    /*
	     * If explicit cursor positioning in sig file,
	     * add offset to start of sig file plus offset into sig file.
	     * Else, just offset to start of sig file.
	     */
	    redraft_pos->offset += cnt;
	}

	if(sig){
	    if(*sig)
	      so_puts((STORE_S *)msgtext, sig);
	    
	    fs_give((void **)&sig);
	}
    }
    else{
	msgno = mn_m2raw(pine_state->msgmap,
			 mn_get_cur(pine_state->msgmap));

	/*--- Grab current envelope ---*/
	env = pine_mail_fetchstructure(pine_state->mail_stream, msgno,
				       &orig_body);

	/*
	 * If the charset of the body part is different from ascii and
	 * charset conversion is _not_ happening, then preserve the original
	 * charset from the message so that if we don't enter any new
	 * chars with the hibit set we can use the original charset.
	 * If not all those things, then don't try to preserve it.
	 */
	if(orig_body){
	    char *charset;

	    charset = rfc2231_get_param(orig_body->parameter,
					"charset", NULL, NULL);
	    if(charset && strucmp(charset, "us-ascii") != 0){
		CONV_TABLE *ct;

		/*
		 * There is a non-ascii charset, is there conversion happening?
		 */
		if(F_ON(F_DISABLE_CHARSET_CONVERSIONS, ps_global)
		   || !(ct=conversion_table(charset, ps_global->VAR_CHAR_SET))
		   || !ct->table){
		    reply.orig_charset = charset;
		    charset = NULL;
		}
	    }

	    if(charset)
	      fs_give((void **) &charset);
	}

	if(env) {
	    if(!(body = reply_body(pine_state->mail_stream, env, orig_body,
				   msgno, NULL, msgtext, prefix,
				   include_text, role, 1, &redraft_pos)))
	       goto done_early;
	}
	else{
	    q_status_message1(SM_ORDER,3,4,
			  "Error fetching message %.200s. Can't reply to it.",
			      long2string(mn_get_cur(pine_state->msgmap)));
	    goto done_early;
	}
    }

    /* fill in reply structure */
    reply.prefix	= prefix;
    reply.mailbox	= cpystr(pine_state->mail_stream->mailbox);
    reply.origmbox	= cpystr(pine_state->mail_stream->original_mailbox
				    ? pine_state->mail_stream->original_mailbox
				    : pine_state->mail_stream->mailbox);
    reply.data.uid.msgs = (unsigned long *) fs_get((times + 1)
						      * sizeof(unsigned long));
    if(reply.data.uid.validity = pine_state->mail_stream->uid_validity){
	reply.flags = REPLY_UID;
	for(i = 0; i < times ; i++)
	  reply.data.uid.msgs[i] = mail_uid(pine_state->mail_stream, seq[i]);
    }
    else{
	reply.flags = REPLY_MSGNO;
	for(i = 0; i < times ; i++)
	  reply.data.uid.msgs[i] = (unsigned long) seq[i];
    }

    reply.data.uid.msgs[i] = 0;			/* tie off list */

#if	defined(DOS) && !defined(_WINDOWS)
    free((void *)reserve);
#endif

    /* partially formatted outgoing message */
    pine_send(outgoing, &body, "COMPOSE MESSAGE REPLY",
	      role, fcc.tptr, &reply, redraft_pos, NULL, NULL, 0);
  done:
    pine_free_body(&body);
    if(reply.mailbox)
      fs_give((void **) &reply.mailbox);
    if(reply.origmbox)
      fs_give((void **) &reply.origmbox);
    if(reply.orig_charset)
      fs_give((void **) &reply.orig_charset);
    fs_give((void **) &reply.data.uid.msgs);
  done_early:
    if((STORE_S *) msgtext)
      gf_clear_so_writec((STORE_S *) msgtext);

    mail_free_envelope(&outgoing);
    mail_free_address(&saved_from);
    mail_free_address(&saved_to);
    mail_free_address(&saved_cc);
    mail_free_address(&saved_resent);

    fs_give((void **)&seq);

    if(prefix)
      fs_give((void **)&prefix);

    if(fcc.tptr)
      fs_give((void **)&fcc.tptr);

    free_redraft_pos(&redraft_pos);
    free_action(&role);
}



/*
 * reply_harvest - 
 *
 *  Returns: 1 if addresses successfully copied
 *	     0 on user cancel or error
 *
 *  Input flags: 
 *		 RSF_FORCE_REPLY_TO
 *		 RSF_QUERY_REPLY_ALL
 *		 RSF_FORCE_REPLY_ALL
 *
 *  Output flags:
 *		 RSF_FORCE_REPLY_ALL
 * 
 */
int
reply_harvest(ps, msgno, section, env, saved_from, saved_to,
	      saved_cc, saved_resent, flags)
    struct pine  *ps;
    long	  msgno;
    char	 *section;
    ENVELOPE	 *env;
    ADDRESS	**saved_from, **saved_to, **saved_cc, **saved_resent;
    int		 *flags;
{
    ADDRESS *ap, *ap2;
    int	     ret = 0, sniff_resent = 0;

      /*
       * If Reply-To is same as From just treat it like it was From.
       * Otherwise, always use the reply-to if we're replying to more
       * than one msg or say ok to using it, even if it's us.
       * If there's no reply-to or it's the same as the from, assume
       * that the user doesn't want to reply to himself, unless there's
       * nobody else.
       */
    if(env->reply_to && !addr_lists_same(env->reply_to, env->from)
       && (F_ON(F_AUTO_REPLY_TO, ps_global)
	   || ((*flags) & RSF_FORCE_REPLY_TO)
	   || (ret = want_to(FRM_PMT,'y','x',NO_HELP,WT_SEQ_SENSITIVE)) == 'y'))
      ap = reply_cp_addr(ps, msgno, section, "reply-to", *saved_from,
			 (ADDRESS *) NULL, env->reply_to, 1);
    else
      ap = reply_cp_addr(ps, msgno, section, "From", *saved_from,
			 (ADDRESS *) NULL, env->from, 0);

    if(ret == 'x') {
	cmd_cancelled("Reply");
	return(0);
    }

    reply_append_addr(saved_from, ap);

    /*--------- check for other recipients ---------*/
    if(((*flags) & (RSF_FORCE_REPLY_ALL | RSF_QUERY_REPLY_ALL))){

	if(ap = reply_cp_addr(ps, msgno, section, "To", *saved_to,
			      *saved_from, env->to, 0))
	  reply_append_addr(saved_to, ap);

	if(ap = reply_cp_addr(ps, msgno, section, "Cc", *saved_cc,
			      *saved_from, env->cc, 0))
	  reply_append_addr(saved_cc, ap);

	/*
	 * In these cases, we either need to look at the resent headers
	 * to include in the reply-to-all, or to decide whether or not
	 * we need to ask the reply-to-all question.
	 */
	if(((*flags) & RSF_FORCE_REPLY_ALL)
	   || (((*flags) & RSF_QUERY_REPLY_ALL)
	       && ((!(*saved_from) && !(*saved_cc))
		   || (*saved_from && !(*saved_to) && !(*saved_cc))))){

	    sniff_resent++;
	    if(ap2 = reply_resent(ps, msgno, section)){
		/*
		 * look for bogus addr entries and replace
		 */
		if(ap = reply_cp_addr(ps, 0, NULL, NULL, *saved_resent,
				      *saved_from, ap2, 0))

		  reply_append_addr(saved_resent, ap);

		mail_free_address(&ap2);
	    }
	}

	/*
	 * It makes sense to ask reply-to-all now.
	 */
	if(((*flags) & RSF_QUERY_REPLY_ALL)
	   && ((*saved_from && (*saved_to || *saved_cc || *saved_resent))
	       || (*saved_cc || *saved_resent))){
	    *flags &= ~RSF_QUERY_REPLY_ALL;
	    if((ret=want_to(ALL_PMT,'n','x',NO_HELP,WT_SEQ_SENSITIVE)) == 'x'){
		cmd_cancelled("Reply");
		return(0);
	    }
	    else if(ret == 'y')
	      *flags |= RSF_FORCE_REPLY_ALL;
	}

	/*
	 * If we just answered yes to the reply-to-all question and
	 * we still haven't collected the resent headers, do so now.
	 */
	if(((*flags) & RSF_FORCE_REPLY_ALL) && !sniff_resent
	   && (ap2 = reply_resent(ps, msgno, section))){
	    /*
	     * look for bogus addr entries and replace
	     */
	    if(ap = reply_cp_addr(ps, 0, NULL, NULL, *saved_resent,
				  *saved_from, ap2, 0))
	      reply_append_addr(saved_resent, ap);

	    mail_free_address(&ap2);
	}
    }

    return(1);
}


ACTION_S *
set_role_from_msg(ps, rflags, msgno, section)
    struct pine *ps;
    long         rflags;
    long         msgno;
    char        *section;
{
    ACTION_S      *role = NULL;
    PAT_S         *pat = NULL;
    SEARCHSET     *ss = NULL;
    PAT_STATE      pstate;

    if(!nonempty_patterns(rflags, &pstate))
      return(role);

    if(msgno > 0L){
	ss = mail_newsearchset();
	ss->first = ss->last = (unsigned long)msgno;
    }

    /* Go through the possible roles one at a time until we get a match. */
    pat = first_pattern(&pstate);

    /* calculate this message's score if needed */
    if(ss && pat && scores_are_used(SCOREUSE_GET) & SCOREUSE_ROLES &&
       get_msg_score(ps->mail_stream, msgno) == SCORE_UNDEF)
      (void)calculate_some_scores(ps->mail_stream, ss, 0);

    while(!role && pat){
	if(match_pattern(pat->patgrp, ps->mail_stream, ss, section,
			 get_msg_score, SO_NOSERVER|SE_NOPREFETCH)){
	    if(!pat->action || pat->action->bogus)
	      break;

	    role = pat->action;
	}
	else
	  pat = next_pattern(&pstate);
    }

    if(ss)
      mail_free_searchset(&ss);

    return(role);
}


/*
 * Ask user to confirm role choice, or choose another role.
 *
 * Args    role -- A pointer into the pattern_h space at the default
 *                    role to use. This can't be a copy, the comparison
 *                    relies on it pointing at the actual role.
 *                    This arg is also used to return a pointer to the
 *                    chosen role.
 *
 * Returns   1 -- Yes, use role which is now in *role. This may not be
 *                the same as the role passed in and it may even be NULL.
 *           0 -- Cancel reply.
 */
int
confirm_role(rflags, role)
    long            rflags;
    ACTION_S      **role;
{
    ACTION_S       *role_p = NULL;
    char            prompt[80], *prompt_fodder;
    int             cmd, done, ret = 1;
    void (*prev_screen)() = ps_global->prev_screen,
	 (*redraw)() = ps_global->redrawer;
    PAT_S          *curpat, *pat;
    PAT_STATE       pstate;
    HelpType        help;
    ESCKEY_S        ekey[4];

    if(!nonempty_patterns(ROLE_DO_ROLES, &pstate) || !role)
      return(ret);
    
    /*
     * If this is a reply or forward and the role doesn't require confirmation,
     * then we just return with what was passed in.
     */
    if(((rflags & ROLE_REPLY) &&
	*role && (*role)->repl_type == ROLE_REPL_NOCONF) ||
       ((rflags & ROLE_FORWARD) &&
	*role && (*role)->forw_type == ROLE_FORW_NOCONF) ||
       ((rflags & ROLE_COMPOSE) &&
	*role && (*role)->comp_type == ROLE_COMP_NOCONF) ||
       (!*role && F_OFF(F_ROLE_CONFIRM_DEFAULT, ps_global) &&
	 !(rflags & ROLE_DEFAULTOK)))
      return(ret);

    /*
     * Check that there is at least one role available. This is among all
     * roles, not just the reply roles or just the forward roles. That's
     * because we have ^T take us to all the roles, not the category-specific
     * roles.
     */
    if(!(pat = last_pattern(&pstate)))
      return(ret);
    
    ekey[0].ch    = 'y';
    ekey[0].rval  = 'y';

    ekey[1].ch    = 'n';
    ekey[1].rval  = 'n';

    ekey[2].ch    = ctrl('T');
    ekey[2].rval  = 2;
    ekey[2].name  = "^T";

    ekey[3].ch    = -1;

    /* check for more than one role available (or no role set) */
    if(pat == first_pattern(&pstate) && *role)	/* no ^T */
      ekey[2].ch    = -1;
    
    /* Setup default */
    curpat = NULL;
    if(*role){
	for(pat = first_pattern(&pstate);
	    pat;
	    pat = next_pattern(&pstate)){
	    if(pat->action == *role){
		curpat = pat;
		break;
	    }
	}
    }

    if(rflags & ROLE_REPLY)
      prompt_fodder = "Reply";
    else if(rflags & ROLE_FORWARD)
      prompt_fodder = "Forward";
    else
      prompt_fodder = "Compose";

    done = 0;
    while(!done){
	if(curpat){
	    char buf[100];

	    help = h_role_confirm;
	    ekey[0].name  = "Y";
	    ekey[0].label = "Yes";
	    ekey[1].name  = "N";
	    ekey[1].label = "No, use default settings";
	    ekey[2].label = "To Select Alternate Role";
	    if(curpat->patgrp && curpat->patgrp->nick)
	      sprintf(prompt, "Use role \"%.50s\" for %s? ",
		      short_str(curpat->patgrp->nick, buf, 50, MidDots),
		      prompt_fodder);
	    else
	      sprintf(prompt,
		      "Use role \"<a role without a nickname>\" for %s? ",
		      prompt_fodder);
	}
	else{
	    help = h_norole_confirm;
	    ekey[0].name  = "Ret";
	    ekey[0].label = prompt_fodder;
	    ekey[1].name  = "";
	    ekey[1].label = "";
	    ekey[2].label = "To Select Role";
	    sprintf(prompt,
		    "Press Return to %s using no role, or ^T to select a role ",
		    prompt_fodder);
	}

	cmd = radio_buttons(prompt, -FOOTER_ROWS(ps_global), ekey,
			    'y', 'x', help, RB_NORM, NULL);

	switch(cmd){
	  case 'y':					/* Accept */
	    done++;
	    *role = curpat ? curpat->action : NULL;
	    break;

	  case 'x':					/* Cancel */
	    ret = 0;
	    /* fall through */

	  case 'n':					/* NoRole */
	    done++;
	    *role = NULL;
	    break;

	  case 2:					/* ^T */
	    if(role_select_screen(ps_global, &role_p, 0) >= 0){
		if(role_p){
		    for(pat = first_pattern(&pstate);
			pat;
			pat = next_pattern(&pstate)){
			if(pat->action == role_p){
			    curpat = pat;
			    break;
			}
		    }
		}
		else
		  curpat = NULL;
	    }

	    ClearBody();
	    ps_global->mangled_body = 1;
	    ps_global->prev_screen = prev_screen;
	    ps_global->redrawer = redraw;
	    break;
	}
    }

    return(ret);
}


/*
 * reply_seed - 
 * 
 */
void
reply_seed(ps, outgoing, env, saved_from, saved_to, saved_cc,
	   saved_resent, fcc, replytoall)
    struct pine *ps;
    ENVELOPE	*outgoing, *env;
    ADDRESS	*saved_from, *saved_to, *saved_cc, *saved_resent;
    BUILDER_ARG	*fcc;
    int		 replytoall;
{
    ADDRESS **to_tail, **cc_tail;
    
    to_tail = &outgoing->to;
    cc_tail = &outgoing->cc;

    if(saved_from){
	/* Put Reply-To or From in To. */
	*to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				 (ADDRESS *) NULL, saved_from, 1);
	/* and the rest in cc */
	if(replytoall){
	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_to, 1);
	    while(*cc_tail)		/* stay on last address */
	      cc_tail = &(*cc_tail)->next;

	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_cc, 1);
	    while(*cc_tail)
	      cc_tail = &(*cc_tail)->next;

	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_resent, 1);
	}
    }
    else if(saved_to){
	/* No From (maybe from us), put To in To. */
	*to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				 (ADDRESS *)NULL, saved_to, 1);
	/* and the rest in cc */
	if(replytoall){
	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_cc, 1);
	    while(*cc_tail)
	      cc_tail = &(*cc_tail)->next;

	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_resent, 1);
	}
    }
    else{
	/* No From or To, put everything else in To if replytoall, */
	if(replytoall){
	    *to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				     (ADDRESS *) NULL, saved_cc, 1);
	    while(*to_tail)
	      to_tail = &(*to_tail)->next;

	    *to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				     (ADDRESS *) NULL, saved_resent, 1);
	}
	/* else, reply to original From which must be us */
	else{
	    /*
	     * Put self in To if in original From.
	     */
	    if(!outgoing->newsgroups)
	      *to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				       (ADDRESS *) NULL, env->from, 1);
	}
    }

    /* add any missing personal data */
    reply_fish_personal(outgoing, env);

    /* get fcc */
    if(fcc && outgoing->to && outgoing->to->host[0] != '.'){
	fcc->tptr = get_fcc_based_on_to(outgoing->to);
    }
    else if(fcc && outgoing->newsgroups){
	char *errmsg = NULL, *newsgroups_returned = NULL;
	int ret_val;

	ret_val = news_build(outgoing->newsgroups, &newsgroups_returned,
			     &errmsg, fcc, NULL);
	if(errmsg){
	    if(*errmsg){
		q_status_message1(SM_ORDER, 3, 3, "%.200s", errmsg);
		display_message(NO_OP_COMMAND);
	    }
	    fs_give((void **)&errmsg);
	}
	if(ret_val != -1 &&
	    strcmp(outgoing->newsgroups, newsgroups_returned)){
	    fs_give((void **)&outgoing->newsgroups);
	    outgoing->newsgroups = newsgroups_returned;
	}
	else
	  fs_give((void **) &newsgroups_returned);
    }
}



/*----------------------------------------------------------------------
    Return a pointer to a copy of the given address list
  filtering out those already in the "mask" lists and ourself.

Args:  mask1  -- Don't copy if in this list
       mask2  --  or if in this list
       source -- List to be copied
       us_too -- Don't filter out ourself.

  ---*/
ADDRESS *
reply_cp_addr(ps, msgno, section, field, mask1, mask2, source, us_too)
     struct pine *ps;
     long	  msgno;
     char	 *section;
     char	 *field;
     ADDRESS     *mask1, *mask2, *source;
     int	  us_too;
{
    ADDRESS *tmp1, *tmp2, *ret = NULL, **ret_tail;

    for(tmp1 = source; msgno && tmp1; tmp1 = tmp1->next)
      if(tmp1->host && tmp1->host[0] == '.'){
	  char *h, *fields[2];

	  fields[0] = field;
	  fields[1] = NULL;
	  if(h = pine_fetchheader_lines(ps ? ps->mail_stream : NULL,
				        msgno, section, fields)){
	      char *p, fname[32];

	      strncpy(fname, field, sizeof(fname)-2);
	      fname[sizeof(fname)-2] = '\0';
	      strcat(fname, ":");
	      for(p = h; p = strstr(p, fname); )
		rplstr(p, strlen(fname), "");	/* strip field strings */

	      sqznewlines(h);			/* blat out CR's & LF's */
	      for(p = h; p = strchr(p, TAB); )
		*p++ = ' ';			/* turn TABs to whitespace */

	      if(*h){
		  long l;

		  ret = (ADDRESS *) fs_get(sizeof(ADDRESS));
		  memset(ret, 0, sizeof(ADDRESS));

		  /* get rid of leading white space */
		  for(p = h; *p == SPACE; p++)
		    ;
		  
		  if(p != h){
		      memmove(h, p, l = strlen(p));
		      h[l] = '\0';
		  }

		  /* base64 armor plate the gunk to protect against
		   * c-client quoting in output.
		   */
		  p = (char *) rfc822_binary(h, strlen(h),
					     (unsigned long *) &l);
		  sqznewlines(p);
		  fs_give((void **) &h);
		  /*
		   * Seems like the 4 ought to be a 2, but I'll leave it
		   * to be safe, in case something else adds 2 chars later.
		   */
		  ret->mailbox = (char *) fs_get(strlen(p) + 4);
		  sprintf(ret->mailbox, "&%s", p);
		  fs_give((void **) &p);
		  ret->host = cpystr(".RAW-FIELD.");
	      }
	  }

	  return(ret);
      }

    ret_tail = &ret;
    for(source = first_addr(source); source; source = source->next){
	for(tmp1 = first_addr(mask1); tmp1; tmp1 = tmp1->next)
	  if(address_is_same(source, tmp1))
	    break;

	for(tmp2 = first_addr(mask2); !tmp1 && tmp2; tmp2 = tmp2->next)
	  if(address_is_same(source, tmp2))
	    break;

	/*
	 * If there's no match in masks *and* this address isn't us, copy...
	 */
	if(!tmp1 && !tmp2 && (us_too || !ps || !address_is_us(source, ps))){
	    tmp1         = source->next;
	    source->next = NULL;	/* only copy one addr! */
	    *ret_tail    = rfc822_cpy_adr(source);
	    ret_tail     = &(*ret_tail)->next;
	    source->next = tmp1;	/* restore rest of list */
	}
    }

    return(ret);
}



void
reply_append_addr(dest, src)
    ADDRESS  **dest, *src;
{
    for( ; *dest; dest = &(*dest)->next)
      ;

    *dest = src;
}



/*----------------------------------------------------------------------
    Test the given address lists for equivalence

Args:  x -- First address list for comparison
       y -- Second address for comparison

  ---*/
int
addr_lists_same(x, y)
     ADDRESS *x, *y;
{
    for(x = first_addr(x), y = first_addr(y);
	x && y;
	x = first_addr(x->next), y = first_addr(y->next)){
	if(!address_is_same(x, y))
	  return(0);
    }

    return(!x && !y);			/* true if ran off both lists */
}



/*----------------------------------------------------------------------
    Test the given address against those in the given envelope's to, cc

Args:  addr -- address for comparison
       env  -- envelope to compare against

  ---*/
int
addr_in_env(addr, env)
    ADDRESS  *addr;
    ENVELOPE *env;
{
    ADDRESS *ap;

    for(ap = env ? env->to : NULL; ap; ap = ap->next)
      if(address_is_same(addr, ap))
	return(1);

    for(ap = env ? env->cc : NULL; ap; ap = ap->next)
      if(address_is_same(addr, ap))
	return(1);

    return(0);				/* not found! */
}



/*----------------------------------------------------------------------
    Add missing personal info dest from src envelope

Args:  dest -- envelope to add personal info to
       src  -- envelope to get personal info from

NOTE: This is just kind of a courtesy function.  It's really not adding
      anything needed to get the mail thru, but it is nice for the user
      under some odd circumstances.
  ---*/
void
reply_fish_personal(dest, src)
     ENVELOPE *dest, *src;
{
    ADDRESS *da, *sa;

    for(da = dest ? dest->to : NULL; da; da = da->next){
	if(da->personal && !da->personal[0])
	  fs_give((void **)&da->personal);

	for(sa = src ? src->to : NULL; sa && !da->personal ; sa = sa->next)
	  if(address_is_same(da, sa) && sa->personal && sa->personal[0])
	    da->personal = cpystr(sa->personal);

	for(sa = src ? src->cc : NULL; sa && !da->personal; sa = sa->next)
	  if(address_is_same(da, sa) && sa->personal && sa->personal[0])
	    da->personal = cpystr(sa->personal);
    }

    for(da = dest ? dest->cc : NULL; da; da = da->next){
	if(da->personal && !da->personal[0])
	  fs_give((void **)&da->personal);

	for(sa = src ? src->to : NULL; sa && !da->personal; sa = sa->next)
	  if(address_is_same(da, sa) && sa->personal && sa->personal[0])
	    da->personal = cpystr(sa->personal);

	for(sa = src ? src->cc : NULL; sa && !da->personal; sa = sa->next)
	  if(address_is_same(da, sa) && sa->personal && sa->personal[0])
	    da->personal = cpystr(sa->personal);
    }
}


/*----------------------------------------------------------------------
   Given a header field and envelope, build "References: " header data

Args:  

Returns: 
  ---*/
char *
reply_build_refs(env)
    ENVELOPE *env;
{
    int   len, id_len, first_ref_len = 0, foldslop;
    char *p, *refs = NULL, *h = env->references;
    char *first_ref = NULL, *tail_refs = NULL;


    if(!(env->message_id && (id_len = strlen(env->message_id))))
      return(NULL);

    if(h){
	/*
	 * The length we have to work with doesn't seem to appear in any
	 * standards. Steve Jones says that in comp.news discussions he
	 * has seen 1024 as the longest length of a header value.
	 * In the inn news source we find MAXHEADERSIZE = 1024. It appears
	 * that is the maximum length of the header value, including 
	 * newlines for folded lines (that is, the newlines are counted).
	 * We'll be conservative and figure every reference will take up a
	 * line of its own when we fold. We'll also count 2 for CRLF instead
	 * of just one for LF just to be safe.       hubert 2001-jan
	 * J.B. Moreno <planb@newsreaders.com> says "The server limit is
	 * more commonly encountered at 999/1000 bytes [...]". So we'll
	 * back off to 999 instead of 1024.
	 */
#define MAXHEADERSIZE (999)

	/* count the total number of potential folds, max of 2 bytes each */
	for(foldslop = 2, p = h; (p = strstr(p+1, "> <")); )
	  foldslop += 2;
	
	if((len=strlen(h)) + 1+id_len + foldslop >= MAXHEADERSIZE
	   && (p = strstr(h, "> <"))){
	    /*
	     * If the references line is so long that we are going to have
	     * to delete some of the references, delete the 2nd, 3rd, ...
	     * We don't want to delete the first message in the thread.
	     */
	    p[1] = '\0';
	    first_ref = cpystr(h);
	    first_ref_len = strlen(first_ref)+1;	/* len includes space */
	    p[1] = ' ';
	    tail_refs = p+2;
	    /* get rid of 2nd, 3rd, ... until it fits */
	    while((len=strlen(tail_refs)) + first_ref_len + 1+id_len +
						    foldslop >= MAXHEADERSIZE
		  && (p = strstr(tail_refs, "> <"))){
		tail_refs = p + 2;
		foldslop -= 2;
	    }

	    /*
	     * If the individual references are seriously long, somebody
	     * is messing with us and we don't care if it works right.
	     */
	    if((len=strlen(tail_refs)) + first_ref_len + 1+id_len +
						    foldslop >= MAXHEADERSIZE){
		first_ref_len = len = 0;
		tail_refs = NULL;
		if(first_ref)
		  fs_give((void **)&first_ref);
	    }
	}
	else
	  tail_refs = h;

	refs = (char *)fs_get((first_ref_len + 1+id_len + len + 1) *
							sizeof(char));
	sprintf(refs, "%s%s%s%s%s",
		first_ref ? first_ref : "",
		first_ref ? " " : "",
		tail_refs ? tail_refs : "",
		tail_refs ? " " : "",
		env->message_id);
    }

    if(!refs && id_len)
      refs = cpystr(env->message_id);

    if(first_ref)
      fs_give((void **)&first_ref);

    return(refs);
}



/*----------------------------------------------------------------------
   Snoop for any Resent-* headers, and return an ADDRESS list

Args:  stream -- 
       msgno -- 

Returns: either NULL if no Resent-* or parsed ADDRESS struct list
  ---*/
ADDRESS *
reply_resent(pine_state, msgno, section)
    struct pine *pine_state;
    long	 msgno;
    char	*section;
{
#define RESENTFROM 0
#define RESENTTO   1
#define RESENTCC   2
    ADDRESS	*rlist = NULL, **a, **b;
    char	*hdrs, *values[RESENTCC+1];
    int		 i;
    static char *fields[] = {"Resent-From", "Resent-To", "Resent-Cc", NULL};
    static char *fakedomain = "@";

    if(hdrs = pine_fetchheader_lines(pine_state->mail_stream,
				     msgno, section, fields)){
	memset(values, 0, (RESENTCC+1) * sizeof(char *));
	simple_header_parse(hdrs, fields, values);
	for(i = RESENTFROM; i <= RESENTCC; i++)
	  rfc822_parse_adrlist(&rlist, values[i],
			       (F_ON(F_COMPOSE_REJECTS_UNQUAL, pine_state))
				 ? fakedomain : pine_state->maildomain);

	/* pare dup's ... */
	for(a = &rlist; *a; a = &(*a)->next)	/* compare every address */
	  for(b = &(*a)->next; *b; )		/* to the others	 */
	    if(address_is_same(*a, *b)){
		ADDRESS *t = *b;

		if(!(*a)->personal){		/* preserve personal name */
		    (*a)->personal = (*b)->personal;
		    (*b)->personal = NULL;
		}

		*b	= t->next;
		t->next = NULL;
		mail_free_address(&t);
	    }
	    else
	      b = &(*b)->next;
    }

    if(hdrs)
      fs_give((void **) &hdrs);
    
    return(rlist);
}


/*----------------------------------------------------------------------
    Format and return subject suitable for the reply command

Args:  subject -- subject to build reply subject for
       buf -- buffer to use for writing.  If non supplied, alloc one.
       buflen -- length of buf if supplied, else ignored

Returns: with either "Re:" prepended or not, if already there.
         returned string is allocated.
  ---*/
char *
reply_subject(subject, buf, buflen)
     char *subject;
     char *buf;
     size_t buflen;
{
    size_t  l   = (subject && *subject) ? strlen(subject) : 10;
    char   *tmp = fs_get(l + 1), *decoded, *p;

    if(!buf){
	buflen = l + 5;
	buf = fs_get(buflen);
    }

    /* decode any 8bit into tmp buffer */
    decoded = (char *) rfc1522_decode((unsigned char *)tmp, l+1, subject, NULL);

    buf[0] = '\0';
    if(decoded					/* already "re:" ? */
       && (decoded[0] == 'R' || decoded[0] == 'r')
       && (decoded[1] == 'E' || decoded[1] == 'e')){

        if(decoded[2] == ':')
	  sprintf(buf, "%.*s", buflen-1, subject);
	else if((decoded[2] == '[') && (p = strchr(decoded, ']'))){
	    p++;
	    while(*p && isspace((unsigned char)*p)) p++;
	    if(p[0] == ':')
	      sprintf(buf, "%.*s", buflen-1, subject);
	}
    }
    if(!buf[0])
      sprintf(buf, "Re: %.*s", buflen-1,
	      (subject && *subject) ? subject : "your mail");

    fs_give((void **) &tmp);
    return(buf);
}


/*----------------------------------------------------------------------
    return initials for the given personal name

Args:  name -- Personal name to extract initials from

Returns: pointer to name overwritten with initials
  ---*/
char *
reply_quote_initials(name)
    char *name;
{
    char *s = name,
         *w = name;
    
    /* while there are still characters to look at */
    while(s && *s){
	/* skip to next initial */
	while(*s && isspace((unsigned char) *s))
	  s++;
	
	/* skip over cruft like single quotes */
	while(*s && !isalnum((unsigned char) *s))
	  s++;

	/* copy initial */
	if(*s)
	  *w++ = *s++;
	
	/* skip to end of this piece of name */
	while(*s && !isspace((unsigned char) *s))
	  s++;
    }

    if(w)
      *w = '\0';

    return(name);
}

/*
 * There is an assumption that MAX_SUBSTITUTION is <= the size of the
 * tokens being substituted for (only in the size of buf below).
 */
#define MAX_SUBSTITUTION 6
#define MAX_PREFIX 63
static char *from_token = "_FROM_";
static char *nick_token = "_NICK_";
static char *init_token = "_INIT_";

/*----------------------------------------------------------------------
    return a quoting string, "> " by default, for replied text

Args:  env -- envelope of message being replied to

Returns: malloc'd array containing quoting string, freed by caller
  ---*/
char *
reply_quote_str(env)
    ENVELOPE *env;
{
    char *prefix, *repl, *p, buf[MAX_PREFIX+1], pbf[MAX_SUBSTITUTION+1];

    strncpy(buf, ps_global->VAR_REPLY_STRING, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    /* set up the prefix to quote included text */
    if(p = strstr(buf, from_token)){
	repl = (env && env->from && env->from->mailbox) ? env->from->mailbox
							: "";
	strncpy(pbf, repl, sizeof(pbf)-1);
	pbf[sizeof(pbf)-1] = '\0';;
	rplstr(p, strlen(from_token), pbf);
    }
    
    if(p = strstr(buf, nick_token)){
	repl = (env &&
		env->from &&
		env->from &&
		get_nickname_from_addr(env->from, tmp_20k_buf,
				       1000)) ? tmp_20k_buf : "";
	strncpy(pbf, repl, sizeof(pbf)-1);
	pbf[sizeof(pbf)-1] = '\0';;
	rplstr(p, strlen(nick_token), pbf);
    }

    if(p = strstr(buf, init_token)){
	char *q = NULL;
	char *dummy = NULL;
	char  buftmp[MAILTMPLEN];

	sprintf(buftmp, "%.200s",
	 (env && env->from && env->from->personal) ? env->from->personal : "");

	repl = (env && env->from && env->from->personal)
		 ? reply_quote_initials(q = cpystr((char *)rfc1522_decode(
						(unsigned char *)tmp_20k_buf, 
						SIZEOF_20KBUF,
						buftmp, &dummy)))
		 : "";

	if(dummy)
	  fs_give((void **)&dummy);

	istrncpy(pbf, repl, sizeof(pbf)-1);
	pbf[sizeof(pbf)-1] = '\0';;
	rplstr(p, strlen(init_token), pbf);
	if(q)
	  fs_give((void **)&q);
    }
    
    removing_double_quotes(prefix = cpystr(buf));

    return(prefix);
}

int
reply_quote_str_contains_tokens()
{
    return(ps_global->VAR_REPLY_STRING && ps_global->VAR_REPLY_STRING[0] &&
	   (strstr(ps_global->VAR_REPLY_STRING, from_token) ||
	    strstr(ps_global->VAR_REPLY_STRING, nick_token) ||
	    strstr(ps_global->VAR_REPLY_STRING, init_token)));
}

/*
 * reply_text_query - Ask user about replying with text...
 *
 * Returns:  1 if include the text
 *	     0 if we're NOT to include the text
 *	    -1 on cancel or error
 */
int
reply_text_query(ps, many, prefix)
    struct pine *ps;
    long	 many;
    char       **prefix;
{
    int ret, edited = 0;
    static ESCKEY_S rtq_opts[] = {
	{'y', 'y', "Y", "Yes"},
	{'n', 'n', "N", "No"},
	{-1, 0, NULL, NULL},	                  /* may be overridden below */
	{-1, 0, NULL, NULL}
    };

    if(F_ON(F_AUTO_INCLUDE_IN_REPLY, ps)
       && F_OFF(F_ENABLE_EDIT_REPLY_INDENT, ps))
      return(1);

    while(1){
	sprintf(tmp_20k_buf, "Include %s%soriginal message%s in Reply%s%s%s? ",
		(many > 1L) ? comatose(many) : "",
		(many > 1L) ? " " : "",
		(many > 1L) ? "s" : "",
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? " (using \"" : "",
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? *prefix : "",
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? "\")" : "");

	if(F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps)){
	    rtq_opts[2].ch    = ctrl('R');
	    rtq_opts[2].rval  = 'r';
	    rtq_opts[2].name  = "^R";
	    rtq_opts[2].label = "Edit Indent String";
	}
	else
	  rtq_opts[2].ch    = -1;

	switch(ret = radio_buttons(tmp_20k_buf, 
				   ps->ttyo->screen_rows > 4
				     ? -FOOTER_ROWS(ps_global) : -1,
				   rtq_opts,
				   (edited || F_ON(F_AUTO_INCLUDE_IN_REPLY, ps))
				       ? 'y' : 'n',
				   'x', NO_HELP, RB_SEQ_SENSITIVE, NULL)){
	  case 'x':
	    cmd_cancelled("Reply");
	    return(-1);

	  case 'r':
	    if(prefix && *prefix){
		int  done = 0;
		char buf[64];
		int  flags;

		while(!done){
		    strncpy(buf, *prefix, sizeof(buf)-1);
		    buf[sizeof(buf)-1] = '\0';

		    flags = OE_APPEND_CURRENT |
			    OE_KEEP_TRAILING_SPACE |
			    OE_DISALLOW_HELP |
			    OE_SEQ_SENSITIVE;

		    switch(optionally_enter(buf, ps->ttyo->screen_rows > 4
					    ? -FOOTER_ROWS(ps_global) : -1,
					    0, sizeof(buf), "Reply prefix : ", 
					    NULL, NO_HELP, &flags)){
		      case 0:		/* entry successful, continue */
			if(flags & OE_USER_MODIFIED){
			    fs_give((void **)prefix);
			    removing_double_quotes(*prefix = cpystr(buf));
			    edited = 1;
			}

			done++;
			break;

		      case 1:
			cmd_cancelled("Reply");

		      case -1:
			return(-1);

		      case 4:
			EndInverse();
			ClearScreen();
			redraw_titlebar();
			if(ps_global->redrawer != NULL)
			  (*ps_global->redrawer)();

			redraw_keymenu();
			break;

		      case 3:
			break;

		      case 2:
		      default:
			q_status_message(SM_ORDER, 3, 4,
				 "Programmer botch in reply_text_query()");
			return(-1);
		    }
		}
	    }

	    break;

	  case 'y':
	    return(1);

	  case 'n':
	    return(0);

	  default:
	    q_status_message1(SM_ORDER, 3, 4,
			      "Invalid rval \'%.200s\'", pretty_command(ret));
	    return(-1);
	}
    }
}



/*----------------------------------------------------------------------
  Build the body for the message number/part being replied to

    Args: 

  Result: BODY structure suitable for sending

  ----------------------------------------------------------------------*/
BODY *
reply_body(stream, env, orig_body, msgno, sect_prefix, msgtext, prefix,
	   plustext, role, toplevel, redraft_pos)
    MAILSTREAM *stream;
    ENVELOPE   *env;
    BODY       *orig_body;
    long	msgno;
    char       *sect_prefix;
    void       *msgtext;
    char       *prefix;
    int		plustext;
    ACTION_S   *role;
    int		toplevel;
    REDRAFT_POS_S **redraft_pos;
{
    char     *p, *sig = NULL, *section, sect_buf[256];
    BODY     *body = NULL, *tmp_body = NULL;
    PART     *part;
    gf_io_t   pc;
    int       impl, template_len = 0, leave_cursor_at_top = 0, reply_raw_body = 0;

    if(sect_prefix)
      sprintf(section = sect_buf, "%.*s.1", sizeof(sect_buf)-1, sect_prefix);
    else
      section = "1";

    if(ps_global->full_header == 2
       && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
      reply_raw_body = 1;

    gf_set_so_writec(&pc, (STORE_S *) msgtext);

    if(toplevel){
	char *filtered;

	impl = 0;
	filtered = detoken(role, env, 0,
			   F_ON(F_SIG_AT_BOTTOM, ps_global) ? 1 : 0,
			   0, redraft_pos, &impl);
	if(filtered){
	    if(*filtered){
		so_puts((STORE_S *)msgtext, filtered);
		if(impl == 1)
		  template_len = strlen(filtered);
		else if(impl == 2)
		  leave_cursor_at_top++;
	    }
	    
	    fs_give((void **)&filtered);
	}
	else
	  impl = 1;
    }
    else
      impl = 1;

    if(toplevel &&
       (sig = reply_signature(role, env, redraft_pos, &impl)) &&
       F_OFF(F_SIG_AT_BOTTOM, ps_global)){

	/*
	 * If CURSORPOS was set explicitly in sig_file, and there was a
	 * template file before that, we need to adjust the offset by the
	 * length of the template file. However, if the template had
	 * a set CURSORPOS in it then impl was 2 before getting to the
	 * signature, so offset wouldn't have been reset by the signature
	 * CURSORPOS and offset would already be correct. That case will
	 * be ok here because template_len will be 0 and adding it does
	 * nothing. If template
	 * didn't have CURSORPOS in it, then impl was 1 and got set to 2
	 * by the CURSORPOS in the sig. In that case we have to adjust the
	 * offset. That's what the next line does. It adjusts it if
	 * template_len is nonzero and if CURSORPOS was set in sig_file.
	 */
	if(impl == 2)
	  (*redraft_pos)->offset += template_len;
	
	if(*sig)
	  so_puts((STORE_S *)msgtext, sig);
	
	/*
	 * Set sig to NULL because we've already used it. If SIG_AT_BOTTOM
	 * is set, we won't have used it yet and want it to be non-NULL.
	 */
	fs_give((void **)&sig);
    }

    /*
     * Only put cursor in sig if there is a cursorpos there but not
     * one in the template, and sig-at-bottom.
     */
    if(!(sig && impl == 2 && !leave_cursor_at_top))
      leave_cursor_at_top++;

    if(plustext){
	if(!orig_body
	   || orig_body->type == TYPETEXT
	   || reply_raw_body
	   || F_OFF(F_ATTACHMENTS_IN_REPLY, ps_global)){
	    /*------ Simple text-only message ----*/
	    body		     = mail_newbody();
	    body->type		     = TYPETEXT;
	    body->contents.text.data = msgtext;
	    reply_delimiter(env, role, pc);
	    if(F_ON(F_INCLUDE_HEADER, ps_global))
	      reply_forward_header(stream, msgno, sect_prefix,
				   env, pc, prefix);

	    if(!orig_body || reply_raw_body || reply_body_text(orig_body, &tmp_body)){
		get_body_part_text(stream, reply_raw_body ? NULL : tmp_body, msgno,
				   reply_raw_body ? sect_prefix
				   : (p = body_partno(stream, msgno, tmp_body)),
				   pc, prefix);
		if(!reply_raw_body && p)
		  fs_give((void **) &p);
	    }
	    else{
		gf_puts(NEWLINE, pc);
		gf_puts("  [NON-Text Body part not included]", pc);
		gf_puts(NEWLINE, pc);
	    }
	}
	else if(orig_body->type == TYPEMULTIPART){
	    /*------ Message is Multipart ------*/
	    if(orig_body->subtype
	       && !strucmp(orig_body->subtype, "signed")
	       && orig_body->nested.part){
		/* operate only on the signed part */
		body = reply_body(stream, env,
				  &orig_body->nested.part->body,
				  msgno, section, msgtext, prefix,
				  plustext, role, 0, redraft_pos);
	    }
	    else if(orig_body->subtype
		    && !strucmp(orig_body->subtype, "alternative")){
		/* Set up the simple text reply */
		body			 = mail_newbody();
		body->type		 = TYPETEXT;
		body->contents.text.data = msgtext;

		if(reply_body_text(orig_body, &tmp_body)){
		    reply_delimiter(env, role, pc);
		    if(F_ON(F_INCLUDE_HEADER, ps_global))
		      reply_forward_header(stream, msgno, sect_prefix,
					   env, pc, prefix);

		    get_body_part_text(stream, tmp_body, msgno,
				       p = body_partno(stream,msgno,tmp_body),
				       pc, prefix);
		    fs_give((void **) &p);
		}
		else
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
			    "No suitable multipart text found for inclusion!");
	    }
	    else{
		body = copy_body(NULL, orig_body);

		/*
		 * whatever subtype it is, demote it
		 * to plain old MIXED.
		 */
		if(body->subtype)
		  fs_give((void **) &body->subtype);

		body->subtype = cpystr("Mixed");

		if(orig_body->nested.part &&
		   orig_body->nested.part->body.type == TYPETEXT) {
		    /*---- First part of the message is text -----*/
		    body->nested.part->body.contents.text.data = msgtext;
		    if(body->nested.part->body.subtype &&
		       strucmp(body->nested.part->body.subtype, "Plain")){
		        fs_give((void **)&body->nested.part->body.subtype);
		        body->nested.part->body.subtype = cpystr("Plain");
		    }
		    reply_delimiter(env, role, pc);
		    if(F_ON(F_INCLUDE_HEADER, ps_global))
		      reply_forward_header(stream, msgno, sect_prefix,
					   env, pc, prefix);

		    if(!(get_body_part_text(stream,
					    &orig_body->nested.part->body,
					    msgno, section, pc, prefix)
			 && fetch_contents(stream, msgno, sect_prefix, body)))
		      q_status_message(SM_ORDER | SM_DING, 3, 4,
				       "Error including all message parts");
		}
		else if(orig_body->nested.part->body.type == TYPEMULTIPART
			&& orig_body->nested.part->body.subtype
			&& !strucmp(orig_body->nested.part->body.subtype,
				    "alternative")
			&& reply_body_text(&orig_body->nested.part->body,
					   &tmp_body)){
		    int partnum;

		    reply_delimiter(env, role, pc);
		    if(F_ON(F_INCLUDE_HEADER, ps_global))
		      reply_forward_header(stream, msgno, sect_prefix,
					   env, pc, prefix);

		    sprintf(sect_buf, "%.*s%s%.*s",
			    sizeof(sect_buf)/2-2,
			    sect_prefix ? sect_prefix : "",
			    sect_prefix ? "." : "",
			    sizeof(sect_buf)/2-2,
			    p = partno(orig_body, tmp_body));
		    fs_give((void **) &p);
		    get_body_part_text(stream, tmp_body, msgno,
				       sect_buf, pc, prefix);

		    part = body->nested.part->next;
		    body->nested.part->next = NULL;
		    mail_free_body_part(&body->nested.part);
		    body->nested.part = mail_newbody_part();
		    body->nested.part->body.type = TYPETEXT;
		    body->nested.part->body.subtype = cpystr("Plain");
		    body->nested.part->body.contents.text.data = msgtext;
		    body->nested.part->next = part;

		    for(partnum = 2; part != NULL; part = part->next){
			sprintf(sect_buf, "%.*s%s%d",
				sizeof(sect_buf)/2,
				sect_prefix ? sect_prefix : "",
				sect_prefix ? "." : "", partnum++);

			if(!fetch_contents(stream, msgno,
					   sect_buf, &part->body)){
			    break;
			}
		    }
		}
		else {
		    /*--- Fetch the original pieces ---*/
		    if(!fetch_contents(stream, msgno, sect_prefix, body))
		      q_status_message(SM_ORDER | SM_DING, 3, 4,
				       "Error including all message parts");

		    /*--- No text part, create a blank one ---*/
		    part			  = mail_newbody_part();
		    part->next			  = body->nested.part;
		    body->nested.part		  = part;
		    part->body.contents.text.data = msgtext;
		}
	    }
	}
	else{
	    /*---- Single non-text message of some sort ----*/
	    body              = mail_newbody();
	    body->type        = TYPEMULTIPART;
	    part              = mail_newbody_part();
	    body->nested.part = part;
    
	    /*--- The first part, a blank text part to be edited ---*/
	    part->body.type		  = TYPETEXT;
	    part->body.contents.text.data = msgtext;

	    /*--- The second part, what ever it is ---*/
	    part->next    = mail_newbody_part();
	    part          = part->next;
	    part->body.id = generate_message_id();
	    copy_body(&(part->body), orig_body);

	    /*
	     * the idea here is to fetch part into storage object
	     */
	    if(part->body.contents.text.data = (void *) so_get(PART_SO_TYPE,
							    NULL,EDIT_ACCESS)){
#if	defined(DOS) && !defined(WIN32)
		mail_parameters(ps_global->mail_stream, SET_GETS,
				(void *)dos_gets); /* fetched to disk */
		append_file = (FILE *)so_text(
				    (STORE_S *) part->body.contents.text.data);

		if(!mail_fetchbody(stream, msgno, section,
				   &part->body.size.bytes))
		  mail_free_body(&body);

		mail_parameters(stream, SET_GETS,(void *)NULL);
		append_file = NULL;
		mail_gc(stream, GC_TEXTS);
#else
		if(p = pine_mail_fetch_body(stream, msgno, section,
					    &part->body.size.bytes, NIL)){
		    so_nputs((STORE_S *)part->body.contents.text.data,
			     p, part->body.size.bytes);
		}
		else
		  mail_free_body(&body);
#endif
	    }
	    else
	      mail_free_body(&body);
	}
    }
    else{
	/*--------- No text included --------*/
	body			 = mail_newbody();
	body->type		 = TYPETEXT;
	body->contents.text.data = msgtext;
    }

    if(!leave_cursor_at_top){
	long  cnt = 0L;
	unsigned char c;

	/* rewind and count chars to start of sig file */
	so_seek((STORE_S *)msgtext, 0L, 0);
	while(so_readc(&c, (STORE_S *)msgtext))
	  cnt++;

	if(!*redraft_pos){
	    *redraft_pos = (REDRAFT_POS_S *)fs_get(sizeof(**redraft_pos));
	    memset((void *)*redraft_pos, 0,sizeof(**redraft_pos));
	    (*redraft_pos)->hdrname = cpystr(":");
	}

	/*
	 * If explicit cursor positioning in sig file,
	 * add offset to start of sig file plus offset into sig file.
	 * Else, just offset to start of sig file.
	 */
	(*redraft_pos)->offset += cnt;
    }

    if(sig){
	if(*sig)
	  so_puts((STORE_S *)msgtext, sig);
	
	fs_give((void **)&sig);
    }

    gf_clear_so_writec((STORE_S *) msgtext);

    return(body);
}



/*
 * reply_part - first replyable multipart of a multipart.
 */
int
reply_body_text(body, new_body)
    BODY *body, **new_body;
{
    if(body){
	switch(body->type){
	  case TYPETEXT :
	    *new_body = body;
	    return(1);

	  case TYPEMULTIPART :
	    if(body->subtype && !strucmp(body->subtype, "alternative")){
		PART *part;

		/* Pick out the interesting text piece */
		for(part = body->nested.part; part; part = part->next)
		  if((!part->body.type || part->body.type == TYPETEXT)
		     && (!part->body.subtype
			 || !strucmp(part->body.subtype, "plain"))){
		      *new_body = &part->body;
		      return(1);
		  }
	    }
	    else if(body->nested.part)
	      /* NOTE: we're only interested in "first" part of mixed */
	      return(reply_body_text(&body->nested.part->body, new_body));

	    break;

	  default:
	    break;
	}
    }

    return(0);
}


char *
reply_signature(role, env, redraft_pos, impl)
    ACTION_S       *role;
    ENVELOPE       *env;
    REDRAFT_POS_S **redraft_pos;
    int            *impl;
{
    char *sig;

    sig = detoken(role, env,
		  2, F_ON(F_SIG_AT_BOTTOM, ps_global) ? 0 : 1, 1,
		  redraft_pos, impl);

    if(F_OFF(F_SIG_AT_BOTTOM, ps_global) && (!sig || !*sig)){
	if(sig)
	  fs_give((void **)&sig);

	sig = (char *)fs_get((strlen(NEWLINE) * 2 + 1) * sizeof(char));
	strcpy(sig, NEWLINE);
	strcat(sig, NEWLINE);
	return(sig);
    }

    return(sig);
}


/*
 * Buf is at least size maxlen+1
 */
void
get_addr_data(env, type, buf, maxlen)
    ENVELOPE    *env;
    IndexColType type;
    char         buf[];
    size_t       maxlen;
{
    ADDRESS *addr = NULL;
    ADDRESS *last_to = NULL;
    ADDRESS *first_addr = NULL, *second_addr = NULL;
    ADDRESS *third_addr = NULL, *fourth_addr = NULL;
    int      cntaddr, l;
    size_t   orig_maxlen;
    char    *p;

    buf[0] = '\0';

    switch(type){
      case iFrom:
	addr = env ? env->from : NULL;
	break;

      case iTo:
	addr = env ? env->to : NULL;
	break;

      case iCc:
	addr = env ? env->cc : NULL;
	break;

      case iSender:
	addr = env ? env->sender : NULL;
	break;

      /*
       * Recips is To and Cc togeter. We hook the two adrlists together
       * temporarily.
       */
      case iRecips:
	addr = env ? env->to : NULL;
	/* Find end of To list */
	for(last_to = addr; last_to && last_to->next; last_to = last_to->next)
	  ;
	
	/* Make the end of To list point to cc list */
	if(last_to)
	  last_to->next = (env ? env->cc : NULL);
	  
	break;

      /*
       * Initials.
       */
      case iInit:
	if(env && env->from && env->from->personal){
	    char *name, *initials = NULL, *dummy = NULL;

	    name = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
					  SIZEOF_20KBUF, env->from->personal,
					  &dummy);
	    if(dummy)
	      fs_give((void **)&dummy);

	    if(name == env->from->personal){
		strncpy(tmp_20k_buf, name, SIZEOF_20KBUF-1);
		tmp_20k_buf[SIZEOF_20KBUF - 1] = '\0';
		name = (char *)tmp_20k_buf;
	    }

	    if(name && *name){
		initials = reply_quote_initials(name);
		istrncpy(buf, initials, maxlen);
		buf[maxlen] = '\0';
	    }
	}

	return;
    }

    orig_maxlen = maxlen;

    first_addr = addr;
    /* skip over rest of c-client group addr */
    if(first_addr && first_addr->mailbox && !first_addr->host){
	for(second_addr = first_addr->next;
	    second_addr && second_addr->host;
	    second_addr = second_addr->next)
	  ;
	
	if(second_addr && !second_addr->host)
	  second_addr = second_addr->next;
    }
    else if(!(first_addr && first_addr->host && first_addr->host[0] == '.'))
      second_addr = first_addr ? first_addr->next : NULL;

    if(second_addr && second_addr->mailbox && !second_addr->host){
	for(third_addr = second_addr->next;
	    third_addr && third_addr->host;
	    third_addr = third_addr->next)
	  ;
	
	if(third_addr && !third_addr->host)
	  third_addr = third_addr->next;
    }
    else if(!(second_addr && second_addr->host && second_addr->host[0] == '.'))
      third_addr = second_addr ? second_addr->next : NULL;

    if(third_addr && third_addr->mailbox && !third_addr->host){
	for(fourth_addr = third_addr->next;
	    fourth_addr && fourth_addr->host;
	    fourth_addr = fourth_addr->next)
	  ;
	
	if(fourth_addr && !fourth_addr->host)
	  fourth_addr = fourth_addr->next;
    }
    else if(!(third_addr && third_addr->host && third_addr->host[0] == '.'))
      fourth_addr = third_addr ? third_addr->next : NULL;

    /* Just attempting to make a nice display */
    if(first_addr && ((first_addr->personal && first_addr->personal[0]) ||
		      (first_addr->mailbox && first_addr->mailbox[0]))){
      if(second_addr){
	if((second_addr->personal && second_addr->personal[0]) ||
	   (second_addr->mailbox && second_addr->mailbox[0])){
	  if(third_addr){
	    if((third_addr->personal && third_addr->personal[0]) ||
	       (third_addr->mailbox && third_addr->mailbox[0])){
	      if(fourth_addr)
	        cntaddr = 4;
	      else
		cntaddr = 3;
	    }
	    else
	      cntaddr = -1;
	  }
	  else
	    cntaddr = 2;
	}
	else
	  cntaddr = -1;
      }
      else
	cntaddr = 1;
    }
    else
      cntaddr = -1;

    p = buf;
    if(cntaddr == 1)
      a_little_addr_string(first_addr, p, maxlen);
    else if(cntaddr == 2){
	a_little_addr_string(first_addr, p, maxlen);
	maxlen -= (l=strlen(p));
	p += l;
	if(maxlen > 7){
	    strcpy(p, " and ");
	    maxlen -= 5;
	    p += 5;
	    a_little_addr_string(second_addr, p, maxlen);
	}
    }
    else if(cntaddr == 3){
	a_little_addr_string(first_addr, p, maxlen);
	maxlen -= (l=strlen(p));
	p += l;
	if(maxlen > 7){
	    strcpy(p, ", ");
	    maxlen -= 2;
	    p += 2;
	    a_little_addr_string(second_addr, p, maxlen);
	    maxlen -= (l=strlen(p));
	    p += l;
	    if(maxlen > 7){
		strcpy(p, ", and ");
		maxlen -= 6;
		p += 6;
		a_little_addr_string(third_addr, p, maxlen);
	    }
	}
    }
    else if(cntaddr > 3){
	a_little_addr_string(first_addr, p, maxlen);
	maxlen -= (l=strlen(p));
	p += l;
	if(maxlen > 7){
	    strcpy(p, ", ");
	    maxlen -= 2;
	    p += 2;
	    a_little_addr_string(second_addr, p, maxlen);
	    maxlen -= (l=strlen(p));
	    p += l;
	    if(maxlen > 7){
		strcpy(p, ", ");
		maxlen -= 2;
		p += 2;
		a_little_addr_string(third_addr, p, maxlen);
		maxlen -= (l=strlen(p));
		p += l;
		if(maxlen >= 12)
		  strcpy(p, ", and others");
		else if(maxlen >= 3)
		  strcpy(p, "...");
	    }
	}
    }
    else if(addr){
	char *a_string;

	a_string = addr_list_string(addr, NULL, 0, 0);
	istrncpy(buf, a_string, maxlen);

	fs_give((void **)&a_string);
    }

    if(last_to)
      last_to->next = NULL;

    buf[orig_maxlen] = '\0';
}


/*
 * Buf is at least size maxlen+1
 */
void
get_news_data(env, type, buf, maxlen)
    ENVELOPE    *env;
    IndexColType type;
    char         buf[];
    size_t       maxlen;
{
    int      cntnews = 0, orig_maxlen;
    char    *news = NULL, *p, *q;

    switch(type){
      case iNews:
      case iNewsAndTo:
      case iToAndNews:
      case iNewsAndRecips:
      case iRecipsAndNews:
	news = env ? env->newsgroups : NULL;
	break;

      case iCurNews:
	if(ps_global->mail_stream && IS_NEWS(ps_global->mail_stream))
	  news = ps_global->cur_folder;

	break;
    }

    orig_maxlen = maxlen;

    if(news){
	q = news;
	while(isspace((unsigned char)*q))
	  q++;

	if(*q)
	  cntnews++;
	
	while((q = strindex(q, ',')) != NULL){
	    q++;
	    while(isspace((unsigned char)*q))
	      q++;
	    
	    if(*q)
	      cntnews++;
	    else
	      break;
	}
    }

    if(cntnews == 1){
	istrncpy(buf, news, maxlen);
	buf[maxlen] = '\0';
	removing_leading_and_trailing_white_space(buf);
    }
    else if(cntnews == 2){
	p = buf;
	q = news;
	while(isspace((unsigned char)*q))
	  q++;
	
	while(maxlen > 0 && *q && !isspace((unsigned char)*q) && *q != ','){
	    *p++ = *q++;
	    maxlen--;
	}
	
	if(maxlen > 7){
	    strncpy(p, " and ", maxlen);
	    p += 5;
	    maxlen -= 5;
	}

	while(isspace((unsigned char)*q) || *q == ',')
	  q++;

	while(maxlen > 0 && *q && !isspace((unsigned char)*q) && *q != ','){
	    *p++ = *q++;
	    maxlen--;
	}

	*p = '\0';

	istrncpy(tmp_20k_buf, buf, 10000);
	strncpy(buf, tmp_20k_buf, orig_maxlen);
    }
    else if(cntnews > 2){
	char b[100];

	p = buf;
	q = news;
	while(isspace((unsigned char)*q))
	  q++;
	
	while(maxlen > 0 && *q && !isspace((unsigned char)*q) && *q != ','){
	    *p++ = *q++;
	    maxlen--;
	}
	
	*p = '\0';
	sprintf(b, " and %d other newsgroups", cntnews-1);
	if(maxlen >= strlen(b))
	  strcpy(p, b);
	else if(maxlen >= 3)
	  strcpy(p, "...");

	istrncpy(tmp_20k_buf, buf, 10000);
	strncpy(buf, tmp_20k_buf, orig_maxlen);
    }

    buf[orig_maxlen] = '\0';
}


/*
 * Buf is at least size maxlen+1
 */
void
a_little_addr_string(addr, buf, maxlen)
    ADDRESS *addr;
    char     buf[];
    size_t   maxlen;
{
    char *dummy = NULL;

    buf[0] = '\0';
    if(addr){
	if(addr->personal && addr->personal[0]){
	    char tmp[MAILTMPLEN];

	    sprintf(tmp, "%.200s", addr->personal);
	    istrncpy(buf, (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
						 SIZEOF_20KBUF, tmp, &dummy),
		     maxlen);
	    if(dummy)
	      fs_give((void **)&dummy);
	}
	else if(addr->mailbox && addr->mailbox[0]){
	    strncpy(buf, addr->mailbox, maxlen);
	    buf[maxlen] = '\0';
	    if(addr->host && addr->host[0] && addr->host[0] != '.'){
		strncat(buf, "@", maxlen-1-strlen(buf));
		strncat(buf, addr->host, maxlen-1-strlen(buf));
	    }
	}
    }

    buf[maxlen] = '\0';
}


/*
 * Buf is at least size maxlen+1
 */
char *
get_reply_data(env, role, type, buf, maxlen)
    ENVELOPE    *env;
    ACTION_S    *role;
    IndexColType type;
    char         buf[];
    size_t       maxlen;
{
    char        *space = NULL;
    IndexColType addrtype;

    buf[0] = '\0';

    switch(type){
      case iRDate:
      case iSDate:
      case iSTime:
      case iSDateTime:
      case iS1Date:
      case iS2Date:
      case iS3Date:
      case iS4Date:
      case iDateIso:
      case iDateIsoS:
      case iTime24:
      case iTime12:
      case iDay:
      case iDayOrdinal:
      case iDay2Digit:
      case iMonAbb:
      case iMonLong:
      case iMon:
      case iMon2Digit:
      case iYear:
      case iYear2Digit:
      case iDate:
      case iLDate:
      case iTimezone:
      case iDayOfWeekAbb:
      case iDayOfWeek:
	if(env && env->date && env->date[0] && maxlen >= 20)
	  date_str(env->date, type, 1, buf);

	break;

      case iCurDate:
      case iCurDateIso:
      case iCurDateIsoS:
      case iCurTime24:
      case iCurTime12:
      case iCurDay:
      case iCurDay2Digit:
      case iCurDayOfWeek:
      case iCurDayOfWeekAbb:
      case iCurMon:
      case iCurMon2Digit:
      case iCurMonLong:
      case iCurMonAbb:
      case iCurYear:
      case iCurYear2Digit:
      case iLstMon:
      case iLstMon2Digit:
      case iLstMonLong:
      case iLstMonAbb:
      case iLstMonYear:
      case iLstMonYear2Digit:
      case iLstYear:
      case iLstYear2Digit:
	if(maxlen >= 20)
	  date_str(NULL, type, 1, buf);

	break;

      case iFrom:
      case iTo:
      case iCc:
      case iSender:
      case iRecips:
      case iInit:
	get_addr_data(env, type, buf, maxlen);
	break;

      case iRoleNick:
	if(role && role->nick){
	    strncpy(buf, role->nick, maxlen);
	    buf[maxlen] = '\0';
	}
	break;

      case iAddress:
      case iMailbox:
	if(env && env->from && env->from->mailbox && env->from->mailbox[0] &&
	   strlen(env->from->mailbox) <= maxlen){
	    strcpy(buf, env->from->mailbox);
	    if(type == iAddress &&
	       env->from->host &&
	       env->from->host[0] &&
	       env->from->host[0] != '.' &&
	       strlen(buf) + strlen(env->from->host) + 1 <= maxlen){
		strcat(buf, "@");
		strcat(buf, env->from->host);
	    }
	}

	break;

      case iNews:
      case iCurNews:
	get_news_data(env, type, buf, maxlen);
	break;

      case iToAndNews:
      case iNewsAndTo:
      case iRecipsAndNews:
      case iNewsAndRecips:
	if(type == iToAndNews || type == iNewsAndTo)
	  addrtype = iTo;
	else
	  addrtype = iRecips;

	if(env && env->newsgroups){
	    space = (char *)fs_get((maxlen+1) * sizeof(char));
	    get_news_data(env, type, space, maxlen);
	}

	get_addr_data(env, addrtype, buf, maxlen);

	if(space && *space && *buf){
	    if(strlen(space) + strlen(buf) + 5 > maxlen){
		if(strlen(space) > maxlen/2)
		  get_news_data(env, type, space, maxlen - strlen(buf) - 5);
		else
		  get_addr_data(env, addrtype, buf, maxlen - strlen(space) - 5);
	    }

	    if(type == iToAndNews || type == iRecipsAndNews)
	      sprintf(tmp_20k_buf, "%s and %s", buf, space);
	    else
	      sprintf(tmp_20k_buf, "%s and %s", space, buf);

	    strncpy(buf, tmp_20k_buf, maxlen);
	    buf[maxlen] = '\0';
	}
	else if(space && *space){
	    strncpy(buf, space, maxlen);
	    buf[maxlen] = '\0';
	}
	
	if(space)
	  fs_give((void **)&space);

	break;

      case iSubject:
	if(env && env->subject){
	    size_t n, len;
	    unsigned char *p, *tmp = NULL;
	    char  *dummy = NULL;

	    if((n = strlen(env->subject)) > SIZEOF_20KBUF-1){
		len = n+1;
		p = tmp = (unsigned char *)fs_get(len * sizeof(char));
	    }
	    else{
		len = SIZEOF_20KBUF;
		p = (unsigned char *)tmp_20k_buf;
	    }
	  
	    istrncpy(buf, (char *)rfc1522_decode(p, len, env->subject, &dummy),
		     maxlen);

	    buf[maxlen] = '\0';
	    if(dummy)
	      fs_give((void **)&dummy);
	    if(tmp)
	      fs_give((void **)&tmp);
	}

	break;
    
      case iMsgID:
	if(env && env->message_id){
	    strncpy(buf, env->message_id, maxlen);
	    buf[maxlen] = '\0';
	}

	break;
    
      default:
	break;
    }

    buf[maxlen] = '\0';
    return(buf);
}


/*
 * reply_delimiter - output formatted reply delimiter for given envelope
 *		     with supplied character writing function.
 */
void
reply_delimiter(env, role, pc)
    ENVELOPE *env;
    ACTION_S *role;
    gf_io_t   pc;
{
#define MAX_DELIM 2000
    char           buf[MAX_DELIM+1];
    char          *p;
    char          *filtered = NULL;
    int            len;


    if(!env)
      return;

    strncpy(buf, ps_global->VAR_REPLY_INTRO, MAX_DELIM);
    buf[MAX_DELIM] = '\0';
    /* preserve exact default behavior from before */
    if(!strcmp(buf, DEFAULT_REPLY_INTRO)){
	struct date d;

	parse_date(env->date, &d);
	gf_puts("On ", pc);			/* All delims have... */
	if(d.wkday != -1){			/* "On day, date month year" */
	    gf_puts(week_abbrev(d.wkday), pc);	/* in common */
	    gf_puts(", ", pc);
	}

	gf_puts(int2string(d.day), pc);
	(*pc)(' ');
	gf_puts(month_abbrev(d.month), pc);
	(*pc)(' ');
	gf_puts(int2string(d.year), pc);

	if(env->from
	   && ((env->from->personal && env->from->personal[0])
	       || (env->from->mailbox && env->from->mailbox[0]))){
	    char buftmp[MAILTMPLEN];

	    a_little_addr_string(env->from, buftmp, sizeof(buftmp)-1);
	    gf_puts(", ", pc);
	    gf_puts(buftmp, pc);
	    gf_puts(" wrote:", pc);
	}
	else
	  gf_puts(", it was written:", pc);

    }
    else{
	filtered = detoken_src(buf, FOR_REPLY_INTRO, env, role,
			       NULL, NULL);

	/* try to truncate if too long */
	if(filtered && (len = strlen(filtered)) > 80){
	    int ended_with_colon = 0;
	    int ended_with_quote = 0;
	    int ended_with_quote_colon = 0;

	    if(filtered[len-1] == ':'){
		ended_with_colon = ':';
		if(filtered[len-2] == QUOTE || filtered[len-2] == '\'')
		  ended_with_quote_colon = filtered[len-2];
	    }
	    else if(filtered[len-1] == QUOTE || filtered[len-1] == '\'')
	      ended_with_quote = filtered[len-1];

	    /* try to find space to break at */
	    for(p = &filtered[75]; p > &filtered[60] && 
		  !isspace((unsigned char)*p); p--)
	      ;
	    
	    if(!isspace((unsigned char)*p))
	      p = &filtered[75];

	    *p++ = '.';
	    *p++ = '.';
	    *p++ = '.';
	    if(ended_with_quote_colon){
		*p++ = ended_with_quote_colon;
		*p++ = ':';
	    }
	    else if(ended_with_colon)
	      *p++ = ended_with_colon;
	    else if(ended_with_quote)
	      *p++ = ended_with_quote;

	    *p = '\0';
	}

	if(filtered && *filtered)
	  gf_puts(filtered, pc);
    }

    /* and end with two newlines unless no leadin at all */
    if(!strcmp(buf, DEFAULT_REPLY_INTRO) || (filtered && *filtered)){
	gf_puts(NEWLINE, pc);
	gf_puts(NEWLINE, pc);
    }

    if(filtered)
      fs_give((void **)&filtered);
}


/*
 * reply_poster_followup - return TRUE if "followup-to" set to "poster"
 *
 * NOTE: queues status message indicating such
 */
int
reply_poster_followup(e)
    ENVELOPE *e;
{
    if(e && e->followup_to && !strucmp(e->followup_to, "poster")){
	q_status_message(SM_ORDER, 2, 3,
			 "Replying to Poster as specified in \"Followup-To\"");
	return(1);
    }

    return(0);
}



/*
 * reply_news_test - Test given envelope for newsgroup data and copy
 *		     it at the users request
 *	RETURNS:
 *	     0  if error or cancel
 *	     1     reply via email
 *           2     follow-up via news
 *	     3     do both
 */
int
reply_news_test(env, outgoing)
    ENVELOPE *env, *outgoing;
{
    int    ret = 1;
    static ESCKEY_S news_opt[] = { {'f', 'f', "F", "Follow-up"},
				   {'r', 'r', "R", "Reply"},
				   {'b', 'b', "B", "Both"},
				   {-1, 0, NULL, NULL} };

    if(env->newsgroups && *env->newsgroups && !reply_poster_followup(env))
      /*
       * Now that we know a newsgroups field is present, 
       * ask if the user is posting a follow-up article...
       */
      switch(radio_buttons(NEWS_PMT, -FOOTER_ROWS(ps_global),
			   news_opt, 'r', 'x', NO_HELP, RB_NORM, NULL)){
	case 'r' :		/* Reply */
	  ret = 1;
	  break;

	case 'f' :		/* Follow-Up via news ONLY! */
	  ret = 2;
	  break;

	case 'b' :		/* BOTH */
	  ret = 3;
	  break;

	case 'x' :		/* cancel or unknown response */
	default  :
	  cmd_cancelled("Reply");
	  ret = 0;
	  break;
      }

    if(ret > 1){
	if(env->followup_to){
	    q_status_message(SM_ORDER, 2, 3,
			     "Posting to specified Followup-To groups");
	    outgoing->newsgroups = cpystr(env->followup_to);
	}
	else if(!outgoing->newsgroups)
	  outgoing->newsgroups = cpystr(env->newsgroups);
	if(!IS_NEWS(ps_global->mail_stream))
	  q_status_message(SM_ORDER, 2, 3,
 "Replying to message that MAY or MAY NOT have been posted to newsgroup");
    }

    return(ret);
}


/*
 * Detokenize signature or template files.
 *
 * If is_sig, we always use literal sigs before sigfiles if they are
 * defined. So, check for role->litsig and use it. If it doesn't exist, use
 * the global literal sig if defined. Else the role->sig file or the
 * global signature file.
 *
 * If !is_sig, use role->template.
 *
 * So we start with a literal signature or a signature or template file.
 * If that's a file, we read it first. The file could be remote.
 * Then we detokenize the literal signature or file contents and return
 * an allocated string which the caller frees.
 *
 * Args    role -- See above about what happens depending on is_sig.
 *		   relative to the pinerc dir.
 *          env -- The envelope to use for detokenizing. May be NULL.
 *  prenewlines -- How many blank lines should be included at start.
 * postnewlines -- How many blank lines should be included after.
 *       is_sig -- This is a signature (not a template file).
 *  redraft_pos -- This is a return value. If it is non-NULL coming in,
 *		   then the cursor position is returned here.
 *         impl -- This is a combination argument which is both an input
 *		   argument and a return value. If it is non-NULL and = 0,
 *		   that means that we want the cursor position returned here,
 *		   even if that position is set implicitly to the end of
 *		   the output string. If it is = 1 coming in, that means
 *		   we only want the cursor position to be set if it is set
 *		   explicitly. If it is 2, or if redraft_pos is NULL,
 *                 we don't set it at all.
 *		   If the cursor position gets set explicitly by a
 *		   _CURSORPOS_ token in the file then this is set to 2
 *		   on return. If the cursor position is set implicitly to
 *		   the end of the included file, then this is set to 1
 *                 on return.
 *
 * Returns -- An allocated string is returned.
 */
char *
detoken(role, env, prenewlines, postnewlines, is_sig, redraft_pos, impl)
    ACTION_S       *role;
    ENVELOPE       *env;
    int             prenewlines, postnewlines, is_sig;
    REDRAFT_POS_S **redraft_pos;
    int            *impl;
{
    char *ret = NULL,
         *src = NULL,
         *literal_sig = NULL,
         *sigfile = NULL;

    if(is_sig){
	/*
	 * If role->litsig is set, we use it;
	 * Else, if VAR_LITERAL_SIG is set, we use that;
	 * Else, if role->sig is set, we use that;
	 * Else, if VAR_SIGNATURE_FILE is set, we use that.
	 * This can be a little surprising if you set the VAR_LITERAL_SIG
	 * and don't set a role->litsig but do set a role->sig. The
	 * VAR_LITERAL_SIG will be used, not role->sig. The reason for this
	 * is mostly that it is much easier to display the right stuff
	 * in the various config screens if we do it that way. Besides,
	 * people will typically use only literal sigs or only sig files,
	 * there is no reason to mix them, so we don't provide support to
	 * do so.
	 */
	if(role && role->litsig)
	  literal_sig = role->litsig;
	else if(ps_global->VAR_LITERAL_SIG)
	  literal_sig = ps_global->VAR_LITERAL_SIG;
	else if(role && role->sig)
	  sigfile = role->sig;
	else
	  sigfile = ps_global->VAR_SIGNATURE_FILE;
    }
    else if(role && role->template)
      sigfile = role->template;

    if(literal_sig)
      src = get_signature_lit(literal_sig, prenewlines, postnewlines, is_sig,1);
    else if(sigfile)
      src = get_signature_file(sigfile, prenewlines, postnewlines, is_sig);
	
    if(src){
	if(*src)
	  ret = detoken_src(src, FOR_TEMPLATE, env, role, redraft_pos, impl);

	fs_give((void **)&src);
    }

    return(ret);
}


/*
 * Filter the source string from the template file and return an allocated
 * copy of the result with text replacements for the tokens.
 * Fill in offset in redraft_pos.
 *
 * This is really inefficient but who cares? It's just cpu time.
 */
char *
detoken_src(src, for_what, env, role, redraft_pos, impl)
    char           *src;
    int             for_what;
    ENVELOPE       *env;
    ACTION_S       *role;
    REDRAFT_POS_S **redraft_pos;
    int            *impl;
{
    int   loopcnt = 25;		/* just in case, avoid infinite loop */
    char *ret, *str1, *str2;
    int   done = 0;

    if(!src)
      return(src);
    
    /*
     * We keep running it through until it stops changing so user can
     * nest calls to token stuff.
     */
    str1 = src;
    do {
	/* short-circuit if no chance it will change */
	if(strindex(str1, '_'))
	  str2 = detoken_guts(str1, for_what, env, role, NULL, 0, NULL);
	else
	  str2 = str1;

	if(str1 && str2 && (str1 == str2 || !strcmp(str1, str2))){
	    done++;				/* It stopped changing */
	    if(str1 && str1 != src && str1 != str2)
	      fs_give((void **)&str1);
	}
	else{					/* Still changing */
	    if(str1 && str1 != src && str1 != str2)
	      fs_give((void **)&str1);
	    
	    str1 = str2;
	}

    } while(str2 && !done && loopcnt-- > 0);

    /*
     * Have to run it through once more to get the redraft_pos and
     * to remove any backslash escape for a token.
     */
    if((str2 && strindex(str2, '_')) ||
       (impl && *impl == 0 && redraft_pos && !*redraft_pos)){
	ret = detoken_guts(str2, for_what, env, role, redraft_pos, 1, impl);
	if(str2 != src)
	  fs_give((void **)&str2);
    }
    else if(str2){
	if(str2 == src)
	  ret = cpystr(str2);
	else
	  ret = str2;
    }

    return(ret);
}


/*
 * The guts of the detokenizing routines. Filter the src string looking for
 * tokens and replace them with the appropriate text. In the case of the
 * cursor_pos token we set redraft_pos instead.
 *
 * Args        src --  The source string
 *        for_what --  
 *             env --  Envelope to look in for token replacements.
 *     redraft_pos --  Return the redraft offset here, if non-zero.
 *       last_pass --  This is a flag to tell detoken_guts whether or not to do
 *                     the replacement for _CURSORPOS_. Leave it as is until
 *                     the last pass. We need this because we want to defer
 *                     cursor placement until the very last call to detoken,
 *                     otherwise we'd have to keep track of the cursor
 *                     position as subsequent text replacements (nested)
 *                     take place.
 *                     This same flag is also used to decide when to eliminate
 *                     backslash escapes from in front of tokens. The only
 *                     use of backslash escapes is to escape an entire token.
 *                     That is, \_DATE_ is a literal _DATE_, but any other
 *                     backslash is a literal backslash. That way, nobody
 *                     but wackos will have to worry about backslashes.
 *         impl -- This is a combination argument which is both an input
 *		   argument and a return value. If it is non-NULL and 0
 *		   coming in, that means that we should set redraft_pos,
 *		   even if that position is set implicitly to the end of
 *		   the output string. If it is 1 coming in, that means
 *		   we only want the cursor position to be set if it is set
 *		   explicitly. If it is 2 coming in (or if
 *                 redraft_pos is NULL) then we don't set it at all.
 *		   If the cursor position gets set explicitly by a
 *		   _CURSORPOS_ token in the file then this is set to 2
 *		   on return. If the cursor position is set implicitly to
 *		   the end of the included file, then this is set to 1
 *                 on return.
 *
 * Returns   pointer to alloced result
 */
char *
detoken_guts(src, for_what, env, role, redraft_pos, last_pass, impl)
    char           *src;
    int             for_what;
    ENVELOPE       *env;
    ACTION_S       *role;
    REDRAFT_POS_S **redraft_pos;
    int             last_pass;
    int            *impl;
{
#define MAXSUB 500
    char          *p, *q, *dst = NULL;
    char           subbuf[MAXSUB+1], *repl;
    INDEX_PARSE_T *pt;
    long           l, cnt = 0L;
    int            sizing_pass = 1, suppress_tokens = 0;

    if(!src)
      return(NULL);

top:

    /*
     * The tokens we look for begin with _. The only escaping mechanism
     * is a backslash in front of a token. This will give you the literal
     * token. So \_DATE_ is a literal _DATE_.
     * Tokens like _word_ are replaced with the appropriate text if
     * word is recognized. If _word_ is followed immediately by a left paren
     * it is an if-else thingie. _word_(match_this,if_text,else_text) means to
     * replace that with either the if_text or else_text depending on whether
     * what _word_ (without the paren) would produce matches match_this or not.
     */
    p = src;
    while(*p){
	switch(*p){
	  case '_':		/* possible start of token */
	    if(!suppress_tokens &&
	       (pt = itoktype(p+1, for_what | DELIM_USCORE)) != NULL){
		char *free_this = NULL;

		p += (strlen(pt->name) + 2);	/* skip over token */

		repl = subbuf;
		subbuf[0] = '\0';

		if(pt->ctype == iCursorPos){
		    if(!last_pass){	/* put it back */
			subbuf[0] = '_';
			strncpy(subbuf+1, pt->name, sizeof(subbuf)-2);
			strcat(subbuf, "_");
		    }

		    if(!sizing_pass){
			*q = '\0';
			l = strlen(dst);
			if(redraft_pos && impl && *impl != 2){
			    if(!*redraft_pos){
				*redraft_pos =
				 (REDRAFT_POS_S *)fs_get(sizeof(**redraft_pos));
				memset((void *)*redraft_pos, 0,
				       sizeof(**redraft_pos));
				(*redraft_pos)->hdrname = cpystr(":");
			    }

			    (*redraft_pos)->offset  = l;
			    *impl = 2;	/* set explicitly */
			}
		    }
		}
		else if(pt->what_for & FOR_REPLY_INTRO)
		  repl = get_reply_data(env, role, pt->ctype,
					subbuf, sizeof(subbuf)-1);

		if(*p == LPAREN){		/* if-else construct */
		    char *skip_ahead;

		    repl = free_this = handle_if_token(repl, p, for_what,
						       env, role,
						       &skip_ahead);
		    p = skip_ahead;
		}

		if(repl && repl[0]){
		    if(sizing_pass)
		      cnt += (long)strlen(repl);
		    else{
			strcpy(q, repl);
			q += strlen(repl);
		    }
		}

		if(free_this)
		  fs_give((void **)&free_this);
	    }
	    else{	/* unrecognized token, treat it just like text */
		suppress_tokens = 0;
		if(sizing_pass)
		  cnt++;
		else
		  *q++ = *p;

		p++;
	    }

	    break;

	  case BSLASH:
	    /*
	     * If a real token follows the backslash, then the backslash
	     * is here to escape the token. Otherwise, it's just a
	     * regular character.
	     */
	    if(*(p+1) == '_' &&
	       ((pt = itoktype(p+2, for_what | DELIM_USCORE)) != NULL)){
		/*
		 * Backslash is escape for literal token.
		 * If we're on the last pass we want to eliminate the
		 * backslash, otherwise we keep it.
		 * In either case, suppress_tokens will cause the token
		 * lookup to be skipped above so that the token will
		 * be treated as literal text.
		 */
		suppress_tokens++;
		if(last_pass){
		    p++;
		    break;
		}
		/* else, fall through and keep backslash */
	    }
	    /* this is a literal backslash, fall through */
	
	  default:
	    if(sizing_pass)
	      cnt++;
	    else
	      *q++ = *p;	/* copy the character */

	    p++;
	    break;
	}
    }

    if(!sizing_pass)
      *q = '\0';

    if(sizing_pass){
	sizing_pass = 0;
	/*
	 * Now we're done figuring out how big the answer will be. We
	 * allocate space for it and go back through filling it in.
	 */
	cnt = max(cnt, 0L);
	q = dst = (char *)fs_get((cnt + 1) * sizeof(char));
	goto top;
    }

    /*
     * Set redraft_pos to character following the template, unless
     * it has already been set.
     */
    if(dst && impl && *impl == 0 && redraft_pos && !*redraft_pos){
	*redraft_pos = (REDRAFT_POS_S *)fs_get(sizeof(**redraft_pos));
	memset((void *)*redraft_pos, 0, sizeof(**redraft_pos));
	(*redraft_pos)->offset  = strlen(dst);
	(*redraft_pos)->hdrname = cpystr(":");
	*impl = 1;
    }

    return(dst);
}


/*
 * Do the if-else part of the detokenization for one case of if-else.
 * The input src should like (match_this, if_matched, else)...
 * 
 * Args expands_to -- This is what the token to the left of the paren
 *                    expanded to, and this is the thing we're going to
 *                    compare with the match_this part.
 *             src -- The source string beginning with the left paren.
 *        for_what -- 
 *             env -- 
 *      skip_ahead -- Tells caller how long the (...) part was so caller can
 *                    skip over that part of the source.
 *
 * Returns -- an allocated string which is the answer, or NULL if nothing.
 */
char *
handle_if_token(expands_to, src, for_what, env, role, skip_ahead)
    char     *expands_to;
    char     *src;
    int       for_what;
    ENVELOPE *env;
    ACTION_S *role;
    char    **skip_ahead;
{
    char *ret = NULL;
    char *skip_to;
    char *match_this, *if_matched, *else_part;

    if(skip_ahead)
      *skip_ahead = src;

    if(!src || *src != LPAREN){
	dprint(1, (debugfile,"botch calling handle_if_token, missing paren\n"));
	return(ret);
    }

    if(!*++src){
	q_status_message(SM_ORDER, 3, 3,
	    "Unexpected end of token string in Reply-LeadIn, Sig, or template");
	return(ret);
    }

    match_this = get_token_arg(src, &skip_to);
    
    /*
     * If the match_this argument is a token, detokenize it first.
     */
    if(match_this && *match_this == '_'){
	char *exp_match_this;

	exp_match_this = detoken_src(match_this, for_what, env,
				     role, NULL, NULL);
	fs_give((void **)&match_this);
	match_this = exp_match_this;
    }

    if(!match_this)
      match_this = cpystr("");

    if(!expands_to)
      expands_to = "";

    src = skip_to;
    while(src && *src && (isspace((unsigned char)*src) || *src == ','))
      src++;

    if_matched = get_token_arg(src, &skip_to);
    src = skip_to;
    while(src && *src && (isspace((unsigned char)*src) || *src == ','))
      src++;

    else_part = get_token_arg(src, &skip_to);
    src = skip_to;
    while(src && *src && *src != RPAREN)
      src++;

    if(src && *src == RPAREN)
      src++;

    if(skip_ahead)
      *skip_ahead = src;

    if(!strcmp(match_this, expands_to)){
	ret = if_matched;
	if(else_part)
	  fs_give((void **)&else_part);
    }
    else{
	ret = else_part;
	if(if_matched)
	  fs_give((void **)&if_matched);
    }

    fs_give((void **)&match_this);

    return(ret);
}


char *
get_token_arg(src, skip_to)
    char  *src;
    char **skip_to;
{
    int   quotes = 0, done = 0;
    char *ret = NULL, *p;

    while(*src && isspace((unsigned char)*src))	/* skip space before string */
      src++;

    if(*src == RPAREN){
	if(skip_to)
	  *skip_to = src;

	return(ret);
    }

    p = ret = (char *)fs_get((strlen(src) + 1) * sizeof(char));
    while(!done){
	switch(*src){
	  case QUOTE:
	    if(++quotes == 2)
	      done++;

	    src++;
	    break;

	  case BSLASH:	/* don't count \" as a quote, just copy */
	    if(*(src+1) == BSLASH || *(src+1) == QUOTE){
		src++;  /* skip backslash */
		*p++ = *src++;
	    }
	    else
	      src++;

	    break;
	
	  case SPACE:
	  case TAB:
	  case RPAREN:
	  case COMMA:
	    if(quotes)
	      *p++ = *src++;
	    else
	      done++;
	    
	    break;

	  case '\0':
	    done++;
	    break;

	  default:
	    *p++ = *src++;
	    break;
	}
    }

    *p = '\0';
    if(skip_to)
      *skip_to = src;
    
    return(ret);
}


void
free_redraft_pos(redraft_pos)
    REDRAFT_POS_S **redraft_pos;
{
    if(redraft_pos && *redraft_pos){
	if((*redraft_pos)->hdrname)
	  fs_give((void **)&(*redraft_pos)->hdrname);
	
	fs_give((void **)redraft_pos);
    }
}


/*
 * forward_delimiter - return delimiter for forwarded text
 */
void
forward_delimiter(pc)
    gf_io_t pc;
{
    gf_puts(NEWLINE, pc);
    gf_puts("---------- Forwarded message ----------", pc);
    gf_puts(NEWLINE, pc);
}



/*----------------------------------------------------------------------
    Wrapper for header formatting tool

    Args: stream --
	  msgno --
	  env --
	  pc --
	  prefix --

  Result: header suitable for reply/forward text written using "pc"

  ----------------------------------------------------------------------*/
void
reply_forward_header(stream, msgno, part, env, pc, prefix)
    MAILSTREAM *stream;
    long	msgno;
    char       *part;
    ENVELOPE   *env;
    gf_io_t	pc;
    char       *prefix;
{
    int      rv;
    HEADER_S h;
    char   **list, **new_list = NULL;

    list = ps_global->VAR_VIEW_HEADERS;

    /*
     * If VIEW_HEADERS is set, we should remove BCC from the list so that
     * the user doesn't inadvertently forward the BCC header.
     */
    if(list && list[0]){
	int    i, cnt = 0;
	char **p;

	while(list[cnt++])
	  ;

	p = new_list = (char **) fs_get((cnt+1) * sizeof(char *));

	for(i=0; list[i]; i++)
	  if(strucmp(list[i], "bcc"))
	    *p++ = cpystr(list[i]);
	
	*p = NULL;

	if(new_list && new_list[0])
	  list = new_list;

    }

    HD_INIT(&h, list, ps_global->view_all_except, FE_DEFAULT & ~FE_BCC);
    if(rv = format_header(stream, msgno, part, env, &h, prefix, NULL,
			  FM_NOINDENT, pc)){
	if(rv == 1)
	  gf_puts("  [Error fetching message header data]", pc);
    }
    else
      gf_puts(NEWLINE, pc);		/* write header delimiter */
    
    if(new_list)
      free_list_array(&new_list);
}
    


/*----------------------------------------------------------------------
       Partially set up message to forward and pass off to composer/mailer

    Args: pine_state -- The usual pine structure

  Result: outgoing envelope and body created and passed off to composer/mailer

   Create the outgoing envelope for the mail being forwarded, which is 
not much more than filling in the subject, and create the message body
of the outgoing message which requires formatting the header from the
envelope of the original messasge.
  ----------------------------------------------------------------------*/
void
forward(ps, role_arg)
    struct pine *ps;
    ACTION_S    *role_arg;
{
    char	  *sig;
    int		   ret, forward_raw_body = 0;
    long	   msgno, j, totalmsgs, rflags;
    ENVELOPE	  *env, *outgoing;
    BODY	  *orig_body, *body = NULL;
    REPLY_S        reply;
    void	  *msgtext = NULL;
    gf_io_t	   pc;
    int            impl, template_len = 0;
    PAT_STATE      dummy;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S      *role = NULL, *nrole;
#if	defined(DOS) && !defined(_WINDOWS)
    char	  *reserve;
#endif

    dprint(4, (debugfile, "\n - forward -\n"));

    memset((void *)&reply, 0, sizeof(reply));
    outgoing              = mail_newenvelope();
    outgoing->message_id  = generate_message_id();

    if(ps_global->full_header == 2
       && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
      forward_raw_body = 1;

    if((totalmsgs = mn_total_cur(ps->msgmap)) > 1L){
	sprintf(tmp_20k_buf, "%s forwarded messages...", comatose(totalmsgs));
	outgoing->subject = cpystr(tmp_20k_buf);
    }
    else{
	/*---------- Get the envelope of message we're forwarding ------*/
	msgno = mn_m2raw(ps->msgmap, mn_get_cur(ps->msgmap));
	if(!((env = pine_mail_fetchstructure(ps->mail_stream, msgno, NULL))
	     && (outgoing->subject = forward_subject(env, 0)))){
	    q_status_message1(SM_ORDER,3,4,
			  "Error fetching message %.200s. Can't forward it.",
			      long2string(msgno));
	    goto clean;
	}
    }

    /*
     * as with all text bound for the composer, build it in 
     * a storage object of the type it understands...
     */
    if((msgtext = (void *)so_get(PicoText, NULL, EDIT_ACCESS)) == NULL){
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Error allocating message text");
	goto clean;
    }

    ret = (totalmsgs > 1L)
	   ? want_to("Forward messages as a MIME digest", 'y', 'x',
		     NO_HELP, WT_SEQ_SENSITIVE)
	   : (ps->full_header == 2)
	     ? want_to("Forward message as an attachment", 'n', 'x',
		       NO_HELP, WT_SEQ_SENSITIVE)
	     : 0;

    if(ret == 'x'){
	cmd_cancelled("Forward");
	so_give((STORE_S **)&msgtext);
	goto clean;
    }

    /* Setup possible role */
    if(role_arg)
      role = copy_action(role_arg);

    if(!role){
	rflags = ROLE_FORWARD;
	if(nonempty_patterns(rflags, &dummy)){
	    /* setup default role */
	    nrole = NULL;
	    j = mn_first_cur(ps->msgmap);
	    do {
		role = nrole;
		nrole = set_role_from_msg(ps, rflags,
					  mn_m2raw(ps->msgmap, j), NULL);
	    } while(nrole && (!role || nrole == role)
		    && (j=mn_next_cur(ps->msgmap)) > 0L);

	    if(!role || nrole == role)
	      role = nrole;
	    else
	      role = NULL;

	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{				/* cancel reply */
		role = NULL;
		cmd_cancelled("Forward");
		so_give((STORE_S **)&msgtext);
		goto clean;
	    }
	}
    }

    if(role)
      q_status_message1(SM_ORDER, 3, 4,
			"Forwarding using role \"%.200s\"", role->nick);

    if(role && role->template){
	char *filtered;

	impl = 1;
	filtered = detoken(role, (totalmsgs == 1L) ? env : NULL,
			   0, 0, 0, &redraft_pos, &impl);
	if(filtered){
	    if(*filtered){
		so_puts((STORE_S *)msgtext, filtered);
		if(impl == 1)
		  template_len = strlen(filtered);
	    }
	    
	    fs_give((void **)&filtered);
	}
    }
    else
      impl = 1;
     
    if(sig = detoken(role, NULL, 2, 0, 1, &redraft_pos, &impl)){
	if(impl == 2)
	  redraft_pos->offset += template_len;

	so_puts((STORE_S *)msgtext, *sig ? sig : NEWLINE);

	fs_give((void **)&sig);
    }
    else
      so_puts((STORE_S *)msgtext, NEWLINE);

    gf_set_so_writec(&pc, (STORE_S *)msgtext);

#if	defined(DOS) && !defined(_WINDOWS)
#if	defined(LWP) || defined(PCTCP) || defined(PCNFS)
#define	IN_RESERVE	8192
#else
#define	IN_RESERVE	16384
#endif
    if((reserve=(char *)malloc(IN_RESERVE)) == NULL){
	gf_clear_so_writec((STORE_S *) msgtext);
	so_give((STORE_S **)&msgtext);
        q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Insufficient memory for message text");
	goto clean;
    }
#endif

    /*
     * If we're forwarding multiple messages *or* the forward-as-mime
     * is turned on and the users wants it done that way, package things
     * up...
     */
    if(ret == 'y'){			/* attach message[s]!!! */
	PART **pp;
	long   totalsize = 0L;

	/*---- New Body to start with ----*/
        body	   = mail_newbody();
        body->type = TYPEMULTIPART;

        /*---- The TEXT part/body ----*/
        body->nested.part                       = mail_newbody_part();
        body->nested.part->body.type            = TYPETEXT;
        body->nested.part->body.contents.text.data = msgtext;

	if(totalmsgs > 1L){
	    /*---- The MULTIPART/DIGEST part ----*/
	    body->nested.part->next		  = mail_newbody_part();
	    body->nested.part->next->body.type	  = TYPEMULTIPART;
	    body->nested.part->next->body.subtype = cpystr("Digest");
	    sprintf(tmp_20k_buf, "Digest of %s messages", comatose(totalmsgs));
	    body->nested.part->next->body.description = cpystr(tmp_20k_buf);
	    pp = &(body->nested.part->next->body.nested.part);
	}
	else
	  pp = &(body->nested.part->next);

	/*---- The Message body subparts ----*/
	for(msgno = mn_first_cur(ps->msgmap);
	    msgno > 0L;
	    msgno = mn_next_cur(ps->msgmap)){

	    msgno = mn_m2raw(ps->msgmap, msgno);
	    env   = pine_mail_fetchstructure(ps->mail_stream, msgno, NULL);

	    if(forward_mime_msg(ps->mail_stream,msgno,NULL,env,pp,msgtext)){
		totalsize += (*pp)->body.size.bytes;
		pp = &((*pp)->next);
	    }
	    else
	    goto bomb;
	}

	if(totalmsgs > 1L)
	  body->nested.part->next->body.size.bytes = totalsize;
    }
    else if(totalmsgs > 1L){
	int		        warned = 0;
	body                  = mail_newbody();
	body->type            = TYPETEXT;
	body->contents.text.data = msgtext;
	env		      = NULL;

	for(msgno = mn_first_cur(ps->msgmap);
	    msgno > 0L;
	    msgno = mn_next_cur(ps->msgmap)){

	    if(env){			/* put 2 between messages */
		gf_puts(NEWLINE, pc);
		gf_puts(NEWLINE, pc);
	    }

	    /*--- Grab current envelope ---*/
	    env = pine_mail_fetchstructure(ps->mail_stream,
					   mn_m2raw(ps->msgmap, msgno),
					   &orig_body);
	    if(!env || !orig_body){
		q_status_message1(SM_ORDER,3,4,
			   "Error fetching message %.200s. Can't forward it.",
			       long2string(msgno));
		goto bomb;
	    }

	    if(orig_body == NULL || orig_body->type == TYPETEXT || forward_raw_body) {
		forward_delimiter(pc);
		reply_forward_header(ps->mail_stream,
				     mn_m2raw(ps->msgmap, msgno),
				     NULL, env, pc, "");

		if(!get_body_part_text(ps->mail_stream, forward_raw_body ? NULL : orig_body,
				       mn_m2raw(ps->msgmap, msgno),
				       forward_raw_body ? NULL : "1", pc, NULL))
		  goto bomb;
	    } else if(orig_body->type == TYPEMULTIPART) {
		if(!warned++)
		  q_status_message(SM_ORDER,3,7,
		    "WARNING!  Attachments not included in multiple forward.");

		if(orig_body->nested.part &&
		   orig_body->nested.part->body.type == TYPETEXT) {
		    /*---- First part of the message is text -----*/
		    forward_delimiter(pc);
		    reply_forward_header(ps->mail_stream,
					 mn_m2raw(ps->msgmap,msgno),
					 NULL, env, pc, "");

		    if(!get_body_part_text(ps->mail_stream,
					   &orig_body->nested.part->body,
					   mn_m2raw(ps->msgmap, msgno),
					   "1", pc, NULL))
		      goto bomb;
		} else {
		    q_status_message(SM_ORDER,0,3,
				     "Multipart with no leading text part!");
		}
	    } else {
		/*---- Single non-text message of some sort ----*/
		q_status_message(SM_ORDER,0,3,
				 "Non-text message not included!");
	    }
	}
    }
    else if(!((env = pine_mail_fetchstructure(ps->mail_stream, msgno,
					      &orig_body))
	      && (body = forward_body(ps->mail_stream, env, orig_body, msgno,
				      NULL, msgtext,
				      FWD_NONE)))){
	q_status_message1(SM_ORDER,3,4,
		      "Error fetching message %.200s. Can't forward it.",
			  long2string(msgno));
	goto clean;
    }

    if(ret != 'y' && totalmsgs == 1L && orig_body){
	char *charset;

	charset = rfc2231_get_param(orig_body->parameter,
				    "charset", NULL, NULL);
	if(charset && strucmp(charset, "us-ascii") != 0){
	    CONV_TABLE *ct;

	    /*
	     * There is a non-ascii charset, is there conversion happening?
	     */
	    if(F_ON(F_DISABLE_CHARSET_CONVERSIONS, ps_global)
	       || !(ct=conversion_table(charset, ps_global->VAR_CHAR_SET))
	       || !ct->table){
		reply.orig_charset = charset;
		charset = NULL;
	    }
	}

	if(charset)
	  fs_give((void **) &charset);

	if(reply.orig_charset)
	  reply.flags = REPLY_FORW;
    }

#if	defined(DOS) && !defined(_WINDOWS)
    free((void *)reserve);
#endif
    pine_send(outgoing, &body, "FORWARD MESSAGE",
	      role, NULL, reply.flags ? &reply : NULL, redraft_pos,
	      NULL, NULL, FALSE);

  clean:
    if(body)
      pine_free_body(&body);

    if((STORE_S *) msgtext)
      gf_clear_so_writec((STORE_S *) msgtext);

    mail_free_envelope(&outgoing);
    free_redraft_pos(&redraft_pos);
    free_action(&role);

    if(reply.orig_charset)
      fs_give((void **)&reply.orig_charset);

    return;

  bomb:
#if	defined(DOS) && !defined(WIN32)
    mail_parameters(ps->mail_stream, SET_GETS, (void *) NULL);
    append_file = NULL;
    mail_gc(ps->mail_stream, GC_TEXTS);
#endif
    q_status_message(SM_ORDER | SM_DING, 4, 5,
		   "Error fetching message contents.  Can't forward message.");
    goto clean;
}



/*----------------------------------------------------------------------
  Build the subject for the message number being forwarded

    Args: pine_state -- The usual pine structure
          msgno      -- The message number to build subject for

  Result: malloc'd string containing new subject or NULL on error

  ----------------------------------------------------------------------*/
char *
forward_subject(env, flags)
    ENVELOPE   *env;
    int         flags;
{
    size_t l;
    char  *p, buftmp[MAILTMPLEN];
    
    if(!env)
      return(NULL);

    dprint(9, (debugfile, "checking subject: \"%s\"\n",
	       env->subject ? env->subject : "NULL"));

    if(env->subject && env->subject[0]){		/* add (fwd)? */
	sprintf(buftmp, "%.200s", env->subject);
	/* decode any 8bit (copy to the temp buffer if decoding doesn't) */
	if(rfc1522_decode((unsigned char *) tmp_20k_buf,
		      SIZEOF_20KBUF, buftmp, NULL) == (unsigned char *) buftmp)
	  strcpy(tmp_20k_buf, buftmp);

	removing_trailing_white_space(tmp_20k_buf);
	if((l=strlen(tmp_20k_buf)) < 1000 &&
	   (l < 5 || strcmp(tmp_20k_buf+l-5,"(fwd)"))){
	    sprintf(tmp_20k_buf+2000, "%s (fwd)", env->subject);
	    strcpy(tmp_20k_buf, tmp_20k_buf+2000);
	}

	/*
	 * HACK:  composer can't handle embedded double quotes in attachment
	 * comments so we substitute two single quotes.
	 */
	if(flags & FS_CONVERT_QUOTES)
	  while(p = strchr(tmp_20k_buf, QUOTE))
	    (void)rplstr(p, 1, "''");
	  
	return(cpystr(tmp_20k_buf));

    }

    return(cpystr("Forwarded mail...."));
}


/*----------------------------------------------------------------------
  Build the body for the message number/part being forwarded

    Args: 

  Result: BODY structure suitable for sending

  ----------------------------------------------------------------------*/
BODY *
forward_body(stream, env, orig_body, msgno, sect_prefix, msgtext, flags)
    MAILSTREAM *stream;
    ENVELOPE   *env;
    BODY       *orig_body;
    long	msgno;
    char       *sect_prefix;
    void       *msgtext;
    int		flags;
{
    BODY    *body = NULL, *text_body, *tmp_body;
    PART    *part;
    gf_io_t  pc;
    char    *tmp_text, *section, sect_buf[256];
    int      forward_raw_body = 0;

    /*
     * Check to see if messages got expunged out from underneath us. This
     * could have happened during the prompt to the user asking whether to
     * include the message as an attachment. Either the message is gone or
     * it might be at a different sequence number. We'd better bail.
     */
    if(ps_global->full_header == 2
       && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
      forward_raw_body = 1;
    if(sp_expunge_count(stream))
      return(NULL);

    if(sect_prefix && forward_raw_body == 0)
      sprintf(section = sect_buf, "%s.1", sect_prefix);
    else if(sect_prefix && forward_raw_body)
      section = sect_prefix;
    else if(!sect_prefix && forward_raw_body)
      section = NULL;
    else
      section = "1";

    gf_set_so_writec(&pc, (STORE_S *) msgtext);
    if(!orig_body || orig_body->type == TYPETEXT || forward_raw_body) {
	/*---- Message has a single text part -----*/
	body			 = mail_newbody();
	body->type		 = TYPETEXT;
	body->contents.text.data = msgtext;
	if(!(flags & FWD_ANON)){
	    forward_delimiter(pc);
	    reply_forward_header(stream, msgno, sect_prefix, env, pc, "");
	}

	if(!get_body_part_text(stream, forward_raw_body ? NULL : orig_body, msgno, section, pc, NULL)){
	    mail_free_body(&body);
	    return(NULL);
	}
    }
    else if(orig_body->type == TYPEMULTIPART) {
	if(orig_body->subtype && !strucmp(orig_body->subtype, "signed")
	   && orig_body->nested.part){
	    /* only operate on the signed data (not the signature) */
	    body = forward_body(stream, env, &orig_body->nested.part->body,
				msgno, sect_prefix, msgtext, flags);
	}
	/*---- Message is multipart ----*/
	else if(!(orig_body->subtype && !strucmp(orig_body->subtype,
						 "alternative")
		  && (body = forward_multi_alt(stream, env, orig_body, msgno,
					       sect_prefix, msgtext,
					       pc, flags)))){
	    /*--- Copy the body and entire structure  ---*/
	    body = copy_body(NULL, orig_body);

	    /*
	     * whatever subtype it is, demote it
	     * to plain old MIXED.
	     */
	    if(body->subtype)
	      fs_give((void **) &body->subtype);

	    body->subtype = cpystr("Mixed");

	    /*--- The text part of the message ---*/
	    if(!orig_body->nested.part){
		q_status_message(SM_ORDER | SM_DING, 3, 6,
				 "Error referencing body part 1");
		mail_free_body(&body);
	    }
	    else if(orig_body->nested.part->body.type == TYPETEXT) {
		/*--- The first part is text ----*/
		text_body		      = &body->nested.part->body;
		text_body->contents.text.data = msgtext;
		if(text_body->subtype && strucmp(text_body->subtype, "Plain")){
		    /* this text is going to the composer, it should be Plain */
		    fs_give((void **)&text_body->subtype);
		    text_body->subtype = cpystr("PLAIN");
		}
		if(!(flags & FWD_ANON)){
		    forward_delimiter(pc);
		    reply_forward_header(stream, msgno,
					 sect_prefix, env, pc, "");
		}

		if(!(get_body_part_text(stream, &orig_body->nested.part->body,
					msgno, section, pc, NULL)
		     && fetch_contents(stream, msgno, sect_prefix, body)))
		  mail_free_body(&body);
/* BUG: ? matter that we're not setting body.size.bytes */
	    }
	    else if(body->nested.part->body.type == TYPEMULTIPART
		    && body->nested.part->body.subtype
		    && !strucmp(body->nested.part->body.subtype, "alternative")
		    && (tmp_body = forward_multi_alt(stream, env,
						     &body->nested.part->body,
						     msgno, sect_prefix,
						     msgtext, pc,
						     flags | FWD_NESTED))){
	      /* for the forward_multi_alt call above, we want to pass
	       * sect_prefix instead of section so we can obtain the header.
	       * Set the FWD_NESTED flag so we fetch the right body_part.
	       */ 
		int partnum;

		part = body->nested.part->next;
		body->nested.part->next = NULL;
		mail_free_body_part(&body->nested.part);
		body->nested.part = mail_newbody_part();
		body->nested.part->body = *tmp_body;
		body->nested.part->next = part;

		for(partnum = 2; part != NULL; part = part->next){
		    sprintf(sect_buf, "%s%s%d",
			    sect_prefix ? sect_prefix : "",
			    sect_prefix ? "." : "", partnum++);

		    if(!fetch_contents(stream, msgno, sect_buf, &part->body)){
			mail_free_body(&body);
			break;
		    }
		}
	    }
	    else {
		if(fetch_contents(stream, msgno, sect_prefix, body)){
		    /*--- Create a new blank text part ---*/
		    part			  = mail_newbody_part();
		    part->next			  = body->nested.part;
		    body->nested.part		  = part;
		    part->body.contents.text.data = msgtext;
		}
		else
		  mail_free_body(&body);
	    }
	}
    }
    else {
	/*---- A single part message, not of type text ----*/
	body                     = mail_newbody();
	body->type               = TYPEMULTIPART;
	part                     = mail_newbody_part();
	body->nested.part      = part;

	/*--- The first part, a blank text part to be edited ---*/
	part->body.type            = TYPETEXT;
	part->body.contents.text.data = msgtext;

	/*--- The second part, what ever it is ---*/
	part->next               = mail_newbody_part();
	part                     = part->next;
	part->body.id            = generate_message_id();
	copy_body(&(part->body), orig_body);

	/*
	 * the idea here is to fetch part into storage object
	 */
	if(part->body.contents.text.data = (void *) so_get(PART_SO_TYPE, NULL,
							   EDIT_ACCESS)){
#if	defined(DOS) && !defined(WIN32)
	    /* fetched text to disk */
	    mail_parameters(stream, SET_GETS, (void *)dos_gets);
	    append_file = (FILE *)so_text(
				   (STORE_S *) part->body.contents.text.data);

	    if(mail_fetchbody(stream, msgno, part, &part->body.size.bytes)){
		/* next time body may stay in core */
		mail_parameters(stream, SET_GETS, (void *)NULL);
		append_file = NULL;
		mail_gc(stream, GC_TEXTS);
	    }
	    else
	      mail_free_body(&body);
#else
	    if(tmp_text = pine_mail_fetch_body(stream, msgno, section,
					 &part->body.size.bytes, NIL))
	      so_nputs((STORE_S *)part->body.contents.text.data, tmp_text,
		       part->body.size.bytes);
	    else
	      mail_free_body(&body);
#endif
	}
	else
	  mail_free_body(&body);
    }

    gf_clear_so_writec((STORE_S *) msgtext);

    return(body);
}


/*----------------------------------------------------------------------
  Build the body for the multipart/alternative part

    Args: 

  Result: 

  ----------------------------------------------------------------------*/
BODY *
forward_multi_alt(stream, env, orig_body, msgno,
		  sect_prefix, msgtext, pc, flags)
    MAILSTREAM *stream;
    ENVELOPE   *env;
    BODY       *orig_body;
    long	msgno;
    char       *sect_prefix;
    void       *msgtext;
    gf_io_t	pc;
    int		flags;
{
    BODY *body = NULL;
    PART *part;
    char  tmp_buf[256];
    int   partnum;

    /* Pick out the interesting text piece */
    for(part = orig_body->nested.part, partnum = 1;
	part;
	part = part->next, partnum++)
      if((!part->body.type || part->body.type == TYPETEXT)
	 && (!part->body.subtype
	     || !strucmp(part->body.subtype, "plain")))
	break;

    /*
     * IF something's interesting insert it
     * AND forget the rest of the multipart
     */
    if(part){
	body			 =  mail_newbody();
	body->type		 = TYPETEXT;
	body->contents.text.data = msgtext;

	if(!(flags & FWD_ANON)){
	    forward_delimiter(pc);
	    reply_forward_header(stream, msgno, sect_prefix, env, pc, "");
	}

	sprintf(tmp_buf, "%.*s%s%s%d",
		sizeof(tmp_buf)/2, sect_prefix ? sect_prefix : "",
		sect_prefix ? "." : "", flags & FWD_NESTED ? "1." : "",
		partnum);
	get_body_part_text(stream, body, msgno, tmp_buf, pc, NULL);
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3,
		       "No suitable part found.  Forwarding as attachment");

    return(body);
}


/*----------------------------------------------------------------------
  Build the body for the message number/part being forwarded as ATTACHMENT

    Args: 

  Result: PARTS suitably attached to body

  ----------------------------------------------------------------------*/
int
forward_mime_msg(stream, msgno, section, env, partp, msgtext)
    MAILSTREAM	*stream;
    long	 msgno;
    char	*section;
    ENVELOPE	*env;
    PART       **partp;
    void	*msgtext;
{
    char	   *tmp_text;
    unsigned long   len;
    BODY	   *b;

    *partp	   = mail_newbody_part();
    b		   = &(*partp)->body;
    b->type	   = TYPEMESSAGE;
    b->id	   = generate_message_id();
    b->description = forward_subject(env, FS_CONVERT_QUOTES);
    b->nested.msg  = mail_newmsg();

    /*---- Package each message in a storage object ----*/
    if((b->contents.text.data = (void *) so_get(PART_SO_TYPE,NULL,EDIT_ACCESS))
       && (tmp_text = mail_fetch_header(stream,msgno,section,NIL,NIL,FT_PEEK))
       && *tmp_text){
	so_puts((STORE_S *) b->contents.text.data, tmp_text);

#if	defined(DOS) && !defined(WIN32)
	/* write fetched text to disk */
	mail_parameters(stream, SET_GETS, (void *) dos_gets);
	append_file = (FILE *) so_text((STORE_S *)b->contents.text.data);

	/* HACK!  See mailview.c:format_message for details... */
	stream->text = NULL;

	/* write the body */
	if(pine_mail_fetch_text (stream,msgno,section,&len,NIL)){
	    b->size.bytes = ftell(append_file);
	    /* next time body may stay in core */
	    mail_parameters(stream, SET_GETS, (void *)NULL);
	    append_file   = NULL;
	    mail_gc(ps->mail_stream, GC_TEXTS);
	    so_release((STORE_S *)b->contents.text.data);
	    return(1);
	}
#else
	b->size.bytes = strlen(tmp_text);
	so_puts((STORE_S *) b->contents.text.data, "\015\012");
	if(tmp_text = pine_mail_fetch_text (stream,msgno,section,&len,NIL)){
	    so_nputs((STORE_S *)b->contents.text.data,tmp_text,(long) len);
	    b->size.bytes += len;
	    return(1);
	}
#endif
    }

    return(0);
}


/*----------------------------------------------------------------------
       Partially set up message to forward and pass off to composer/mailer

    Args: pine_state -- The usual pine structure
          message    -- The MESSAGECACHE of entry to reply to 

  Result: outgoing envelope and body created and passed off to composer/mailer

   Create the outgoing envelope for the mail being forwarded, which is 
not much more than filling in the subject, and create the message body
of the outgoing message which requires formatting the header from the
envelope of the original messasge.
  ----------------------------------------------------------------------*/
void
forward_text(pine_state, text, source)
     struct pine *pine_state;
     void        *text;
     SourceType   source;
{
    ENVELOPE *env;
    BODY     *body;
    gf_io_t   pc, gc;
    STORE_S  *msgtext;
    char     *enc_error, *sig;
    ACTION_S *role = NULL;
    PAT_STATE dummy;
    long      rflags = ROLE_COMPOSE;

    if(msgtext = so_get(PicoText, NULL, EDIT_ACCESS)){
	env                   = mail_newenvelope();
	env->message_id       = generate_message_id();
	body                  = mail_newbody();
	body->type            = TYPETEXT;
	body->contents.text.data = (void *) msgtext;

	if(nonempty_patterns(rflags, &dummy)){
	    /*
	     * This is really more like Compose, even though it
	     * is called Forward.
	     */
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{			/* cancel */
		cmd_cancelled("Composition");
		display_message('x');
		mail_free_envelope(&env);
		pine_free_body(&body);
		return;
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4, "Composing using role \"%.200s\"",
			    role->nick);

	sig = detoken(role, NULL, 2, 0, 1, NULL, NULL);
	so_puts(msgtext, (sig && *sig) ? sig : NEWLINE);
	so_puts(msgtext, NEWLINE);
	so_puts(msgtext, "----- Included text -----");
	so_puts(msgtext, NEWLINE);
	if(sig)
	  fs_give((void **)&sig);

	gf_filter_init();
	gf_set_so_writec(&pc, msgtext);
	gf_set_readc(&gc,text,(source == CharStar) ? strlen((char *)text) : 0L,
		     source);

	if((enc_error = gf_pipe(gc, pc)) == NULL){
	    pine_send(env, &body, "SEND MESSAGE", role, NULL, NULL, NULL,
		      NULL, NULL, FALSE);
	    pine_state->mangled_screen = 1;
	}
	else{
	    q_status_message1(SM_ORDER | SM_DING, 3, 5,
			      "Error reading text \"%.200s\"",enc_error);
	    display_message('x');
	}

	gf_clear_so_writec(msgtext);
	mail_free_envelope(&env);
	pine_free_body(&body);
    }
    else {
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Error allocating message text");
	display_message('x');
    }

    free_action(&role);
}



/*----------------------------------------------------------------------
       Partially set up message to resend and pass off to mailer

    Args: pine_state -- The usual pine structure

  Result: outgoing envelope and body created and passed off to mailer

   Create the outgoing envelope for the mail being resent, which is 
not much more than filling in the subject, and create the message body
of the outgoing message which requires formatting the header from the
envelope of the original messasge.
  ----------------------------------------------------------------------*/
void
bounce(pine_state, role)
    struct pine   *pine_state;
    ACTION_S      *role;
{
    ENVELOPE	  *env;
    long           msgno, rawno;
    char          *save_to = NULL, **save_toptr = NULL, *errstr = NULL,
		  *prmpt_who = NULL, *prmpt_cnf = NULL;

    dprint(4, (debugfile, "\n - bounce -\n"));

    if(mn_total_cur(pine_state->msgmap) > 1L){
	save_toptr = &save_to;
	sprintf(tmp_20k_buf, "BOUNCE (redirect) %d messages to : ",
		mn_total_cur(pine_state->msgmap));
	prmpt_who = cpystr(tmp_20k_buf);
	sprintf(tmp_20k_buf, "Send %d messages ",
		mn_total_cur(pine_state->msgmap));
	prmpt_cnf = cpystr(tmp_20k_buf);
    }

    for(msgno = mn_first_cur(pine_state->msgmap);
	msgno > 0L;
	msgno = mn_next_cur(pine_state->msgmap)){

	rawno = mn_m2raw(pine_state->msgmap, msgno);
	if(env = pine_mail_fetchstructure(pine_state->mail_stream, rawno, NULL))
	  errstr = bounce_msg(pine_state->mail_stream, rawno, NULL, role,
			      save_toptr, env->subject, prmpt_who, prmpt_cnf);
	else
	  errstr = "Can't fetch Subject for Bounce";


	if(errstr){
	    if(*errstr)
	      q_status_message(SM_ORDER | SM_DING, 4, 7, errstr);

	    break;
	}
    }

    if(save_to)
      fs_give((void **)&save_to);

    if(prmpt_who)
      fs_give((void **) &prmpt_who);

    if(prmpt_cnf)
      fs_give((void **) &prmpt_cnf);
}


char *
bounce_msg(stream, rawno, part, role, to, subject, pmt_who, pmt_cnf)
    MAILSTREAM	*stream;
    long	 rawno;
    char	*part;
    ACTION_S    *role;
    char       **to;
    char	*subject;
    char	*pmt_who, *pmt_cnf;
{
    char     *h, *p, *errstr = NULL;
    int	      i, was_seen = -1;
    void     *msgtext;
    gf_io_t   pc;
    ENVELOPE *outgoing;
    BODY     *body = NULL;
    MESSAGECACHE *mc;

    outgoing		 = mail_newenvelope();
    outgoing->message_id = generate_message_id();
    outgoing->subject    = cpystr(subject ? subject : "Resent mail....");

    /*
     * Fill in destination if we were given one.  If so, note that we
     * call p_s_s() below such that it won't prompt...
     */
    if(to && *to){
	static char *fakedomain = "@";
	char	    *tmp_a_string;

	/* rfc822_parse_adrlist feels free to destroy input so copy */
	tmp_a_string = cpystr(*to);
	rfc822_parse_adrlist(&outgoing->to, tmp_a_string,
			     (F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global))
			     ? fakedomain : ps_global->maildomain);
	fs_give((void **) &tmp_a_string);
    }

    /* build remail'd header */
    if(h = mail_fetch_header(stream, rawno, part, NULL, 0, FT_PEEK)){
	for(p = h, i = 0; p = strchr(p, ':'); p++)
	  i++;

	/* allocate it */
	outgoing->remail = (char *) fs_get(strlen(h) + (2 * i) + 1);

	/*
	 * copy it, "X-"ing out transport headers bothersome to
	 * software but potentially useful to the human recipient...
	 */
	p = outgoing->remail;
	bounce_mask_header(&p, h);
	do
	  if(*h == '\015' && *(h+1) == '\012'){
	      *p++ = *h++;		/* copy CR LF */
	      *p++ = *h++;
	      bounce_mask_header(&p, h);
	  }
	while(*p++ = *h++);
    }
    /* BUG: else complain? */
	     
    /*
     * as with all text bound for the composer, build it in 
     * a storage object of the type it understands...
     */
    if(!(msgtext = (void *) so_get(PicoText, NULL, EDIT_ACCESS))){
	mail_free_envelope(&outgoing);
	return("Error allocating message text");
    }

    /*
     * Build a fake body description.  It's ignored by pine_rfc822_header,
     * but we need to set it to something that makes set_mime_types
     * not sniff it and pine_rfc822_output_body not re-encode it.
     * Setting the encoding to (ENCMAX + 1) will work and shouldn't cause
     * problems unless something tries to access body_encodings[] using
     * it without proper precautions.  We don't want to use ENCOTHER
     * cause that tells set_mime_types to sniff it, and we don't want to
     * use ENC8BIT since that tells pine_rfc822_output_body to qp-encode
     * it.  When there's time, it'd be nice to clean this interaction
     * up...
     */
    body		     = mail_newbody();
    body->type		     = TYPETEXT;
    body->encoding	     = ENCMAX + 1;
    body->subtype	     = cpystr("Plain");
    body->contents.text.data = msgtext;
    gf_set_so_writec(&pc, (STORE_S *) msgtext);

    if(rawno > 0L && stream && rawno <= stream->nmsgs
       && (mc = mail_elt(stream,  rawno)))
      was_seen = mc->seen;

    /* pass NULL body to force mail_fetchtext */
    if(!get_body_part_text(stream, NULL, rawno, part, pc, NULL))
      errstr = "Error fetching message contents. Can't Bounce message";

    gf_clear_so_writec((STORE_S *) msgtext);

    if(pine_simple_send(outgoing, &body, role, pmt_who, pmt_cnf, to,
			!(to && *to) ? SS_PROMPTFORTO : 0) < 0){
	errstr = "";		/* p_s_s() better have explained! */
	/* clear seen flag */
	if(was_seen == 0 && rawno > 0L
	   && stream && rawno <= stream->nmsgs
	   && (mc = mail_elt(stream,  rawno)) && mc->seen)
	  mail_flag(stream, long2string(rawno), "\\SEEN", 0);
    }

    /* Just for good measure... */
    mail_free_envelope(&outgoing);
    pine_free_body(&body);

    return(errstr);		/* no problem-o */
}




/*----------------------------------------------------------------------
    Mask off any header entries we don't want xport software to see

Args:  d -- destination string pointer pointer
       s -- source string pointer pointer

       Postfix uses Delivered-To to detect loops.
       Received line counting is also used to detect loops in places.

  ----*/
void
bounce_mask_header(d, s)
    char **d, *s;
{
    if(((*s == 'R' || *s == 'r')
        && (!struncmp(s+1, "esent-", 6) || !struncmp(s+1, "eceived:", 8)
	    || !struncmp(s+1, "eturn-Path", 10)))
       || !struncmp(s, "Delivered-To:", 13)){
	*(*d)++ = 'X';				/* write mask */
	*(*d)++ = '-';
    }
}


        
/*----------------------------------------------------------------------
    Fetch and format text for forwarding

Args:  stream      -- Mail stream to fetch text for
       message_no  -- Message number of text for foward
       part_number -- Part number of text to forward
       env         -- Envelope of message being forwarded
       body        -- Body structure of message being forwarded

Returns:  true if OK, false if problem occured while filtering

If the text is richtext, it will be converted to plain text, since there's
no rich text editing capabilities in Pine (yet). The character sets aren't
really handled correctly here. Theoretically editing should not be allowed
if text character set doesn't match what term-character-set is set to.

It's up to calling routines to plug in signature appropriately

As with all internal text, NVT end-of-line conventions are observed.
DOESN'T sanity check the prefix given!!!
  ----*/
int
get_body_part_text(stream, body, msg_no, part_no, pc, prefix)
    MAILSTREAM *stream;
    BODY       *body;
    long        msg_no;
    char       *part_no;
    gf_io_t     pc;
    char       *prefix;
{
    int		i, we_cancel = 0, dashdata, wrapflags = 0, flow_res = 0;
    FILTLIST_S  filters[11];
    long	len;
    char       *err, *charset, *prefix_p = NULL;
#if	defined(DOS) && !defined(WIN32)
    char     *tmpfile_name = NULL;
#endif

    memset(filters, 0, sizeof(filters));
    if(!pc_is_picotext(pc))
      we_cancel = busy_alarm(1, NULL, NULL, 0);

    /* if null body, we must be talking to a non-IMAP2bis server.
     * No MIME parsing provided, so we just grab the message text...
     */
    if(body == NULL){
	char         *text, *decode_error;
#if	defined(DOS) && !defined(WIN32)
	MESSAGECACHE *mc;
#endif
	gf_io_t       gc;
	SourceType    src = CharStar;
	int           rv = 0;

	(void) pine_mail_fetchstructure(stream, msg_no, NULL);

#if	defined(DOS) && !defined(WIN32)
	mc = (msgno > 0L && stream && rawno <= stream->nmsgs)
		? mail_elt(stream, msg_no) : NULL;
	if((mc && mc->rfc822_size > MAX_MSG_INCORE)
	   || (ps_global->context_current->type & FTYPE_BBOARD)){
	    src = FileStar;		/* write fetched text to disk */
	    if(!(tmpfile_name = temp_nam(NULL, "pt"))
	       || !(append_file = fopen(tmpfile_name, "w+b"))){
		if(tmpfile_name){
		    (void)unlink(tmpfile_name);
		    fs_give((void **)&tmpfile_name);
		}

		q_status_message1(SM_ORDER,3,4,"Can't build tmpfile: %.200s",
				  error_description(errno));
		if(we_cancel)
		  cancel_busy_alarm(-1);

		return(rv);
	    }

	    mail_parameters(stream, SET_GETS, (void *)dos_gets);
	}
	else				/* message stays in core */
	  mail_parameters(stream, SET_GETS, (void *)NULL);
#endif

	if(text = pine_mail_fetch_text(stream, msg_no, part_no, NULL, 0)){
#if	defined(DOS) && !defined(WIN32)
	    if(src == FileStar)
	      gf_set_readc(&gc, append_file, 0L, src);
	    else
#endif
	    gf_set_readc(&gc, text, (unsigned long)strlen(text), src);

	    gf_filter_init();		/* no filters needed */
	    if(prefix)
	      gf_link_filter(gf_prefix, gf_prefix_opt(prefix));
	    if(decode_error = gf_pipe(gc, pc)){
		sprintf(tmp_20k_buf, "%s%s    [Formatting error: %s]%s",
			NEWLINE, NEWLINE,
			decode_error, NEWLINE);
		gf_puts(tmp_20k_buf, pc);
		rv++;
	    }
	}
	else{
	    gf_puts(NEWLINE, pc);
	    gf_puts("    [ERROR fetching text of message]", pc);
	    gf_puts(NEWLINE, pc);
	    gf_puts(NEWLINE, pc);
	    rv++;
	}

#if	defined(DOS) && !defined(WIN32)
	/* clean up tmp file created for dos_gets ?  If so, trash
	 * cached knowledge, and make sure next fetch stays in core
	 */
	if(src == FileStar){
	    fclose(append_file);
	    append_file = NULL;
	    unlink(tmpfile_name);
	    fs_give((void **)&tmpfile_name);
	    mail_parameters(stream, SET_GETS, (void *) NULL);
	    mail_gc(stream, GC_TEXTS);
	}
#endif
	if(we_cancel)
	  cancel_busy_alarm(-1);

	return(rv == 0);
    }

    i = 0;			/* for start of filter list */

    charset = rfc2231_get_param(body->parameter, "charset", NULL, NULL);

    if(F_OFF(F_DISABLE_2022_JP_CONVERSIONS, ps_global)
       && (!charset
	   || !strucmp(charset, "US-ASCII")
	   || !strucmp(charset, "ISO-2022-JP")))
      filters[i++].filter = gf_2022_jp_to_euc;

    if(charset){
	if(F_OFF(F_DISABLE_CHARSET_CONVERSIONS, ps_global)){
	    CONV_TABLE *ct;

	    ct = conversion_table(charset, ps_global->VAR_CHAR_SET);
	    if(ct && ct->convert && ct->table){
		filters[i].filter = ct->convert;
		filters[i++].data = ct->table;
	    }
	}

	fs_give((void **) &charset);
    }

    /*
     * just use detach, but add an auxiliary filter to insert prefix,
     * and, perhaps, digest richtext
     */
    if(ps_global->full_header != 2
       && !ps_global->postpone_no_flow
       && (!body->subtype || !strucmp(body->subtype, "plain"))){
	char *parmval;

	flow_res = (F_OFF(F_QUELL_FLOWED_TEXT, ps_global)
		     && F_OFF(F_STRIP_WS_BEFORE_SEND, ps_global)
		     && (!prefix || (strucmp(prefix,"> ") == 0)
			 || strucmp(prefix, ">") == 0));
	if(parmval = rfc2231_get_param(body->parameter,
				       "format", NULL, NULL)){
	    if(!strucmp(parmval, "flowed")){
		wrapflags |= GFW_FLOWED;

		fs_give((void **) &parmval);
		if(parmval = rfc2231_get_param(body->parameter,
					       "delsp", NULL, NULL)){
		    if(!strucmp(parmval, "yes")){
			filters[i++].filter = gf_preflow;
			wrapflags |= GFW_DELSP;
		    }

		    fs_give((void **) &parmval);
		}

 		/*
 		 * if there's no prefix we're forwarding text
 		 * otherwise it's reply text.  unless the user's
 		 * tied our hands, alter the prefix to continue flowed
 		 * formatting...
 		 */
 		if(flow_res)
		  wrapflags |= GFW_FLOW_RESULT;

		filters[i].filter = gf_wrap;
		/* 
		 * The 80 will cause longer lines than what is likely
		 * set by composer_fillcol, so we'll have longer
		 * quoted and forwarded lines than the lines we type.
		 * We're fine with that since the alternative is the
		 * possible wrapping of lines we shouldn't have, which
		 * is mitigated by this higher 80 limit.
		 *
		 * If we were to go back to using composer_fillcol,
		 * the correct value is composer_fillcol + 1, pine
		 * is off-by-one from pico.
		 */
		filters[i++].data = gf_wrap_filter_opt(
		    max(min(ps_global->ttyo->screen_cols
			    - (prefix ? strlen(prefix) : 0),
			    80 - (prefix ? strlen(prefix) : 0)),
			30), /* doesn't have to be 30 */
		    990, /* 998 is the SMTP limit */
		    NULL, 0, wrapflags);
	    }
	}

 	/*
 	 * if not flowed, remove trailing whitespace to reduce
 	 * confusion, since we're sending out as flowed if we
	 * can.  At some future point, we might try 
 	 * plugging in a user-option-controlled heuristic
 	 * flowing filter 
	 *
	 * We also want to fold "> " quotes so we get the
	 * attributions correct.
 	 */
	if(flow_res && prefix && !strucmp(prefix, "> "))
	  *(prefix_p = prefix + 1) = '\0';
	if(!(wrapflags & GFW_FLOWED)
	   && flow_res){
	    filters[i].filter = gf_line_test;
	    filters[i++].data = gf_line_test_opt(twsp_strip, NULL);

	    filters[i].filter = gf_line_test;
	    filters[i++].data = gf_line_test_opt(quote_fold, NULL);
	}
    }
    else if(body->subtype){
	if(strucmp(body->subtype,"richtext") == 0){
	    filters[i].filter = gf_rich2plain;
	    filters[i++].data = gf_rich2plain_opt(1);
	}
	else if(strucmp(body->subtype,"enriched") == 0){
	    filters[i].filter = gf_enriched2plain;
	    filters[i++].data = gf_enriched2plain_opt(1);
	}
	else if(strucmp(body->subtype,"html") == 0){
	    filters[i].filter = gf_html2plain;
	    filters[i++].data = gf_html2plain_opt(NULL,
						  ps_global->ttyo->screen_cols,
						  NULL, NULL, GFHP_STRIPPED);
	}
    }

    if(prefix){
	if(F_ON(F_ENABLE_SIGDASHES, ps_global) ||
	   F_ON(F_ENABLE_STRIP_SIGDASHES, ps_global)){
	    dashdata = 0;
	    filters[i].filter = gf_line_test;
	    filters[i++].data = gf_line_test_opt(sigdash_strip, &dashdata);
	}

	filters[i].filter = gf_prefix;
	filters[i++].data = gf_prefix_opt(prefix);

	if(wrapflags & GFW_FLOWED || flow_res){
	    filters[i].filter = gf_line_test;
	    filters[i++].data = gf_line_test_opt(post_quote_space, NULL);
	}
    }

    err = detach(stream, msg_no, part_no, &len, pc,
		 filters[0].filter ? filters : NULL, 0);

    if(prefix_p)
      *prefix_p = ' ';

    if (err != (char *) NULL)
       q_status_message2(SM_ORDER, 3, 4, "%.200s: message number %ld",
			 err, (void *) msg_no);

    if(we_cancel)
      cancel_busy_alarm(-1);

    return((int)len);
}

int
post_quote_space(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    char *p;

    for(p = line; *p; p++)
      if(*p != '>'){
	  if(p != line && *p != ' ')
	    ins = gf_line_test_new_ins(ins, p, " ", 1);

	  break;
      }

    return(0);
}

int
quote_fold(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    char *p;

    if(*line == '>'){
	for(p = line; *p; p++){
	    if(isspace((unsigned char) *p)){
		if(*(p+1) == '>')
		  ins = gf_line_test_new_ins(ins, p, "", -1);
	    }
	    else if(*p != '>')
	      break;
	}
    }

    return(0);
}

int
twsp_strip(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    char *p, *ws = NULL;

    if(line[0] == '-' && line[1] == '-' && line[2] == ' ' && line[3] == '\0')
      return(0);

    for(p = line; *p; p++)
      if(isspace((unsigned char) *p)){
	  if(!ws)
	    ws = p;
      }
      else
	ws = NULL;

    if(ws)
      ins = gf_line_test_new_ins(ins, ws, "", -(p - ws));

    return(0);
}


int
sigdash_strip(linenum, line, ins, local)
    long       linenum;
    char      *line;
    LT_INS_S **ins;
    void      *local;
{
    if(*((int *)local)
       || (*line == '-' && *(line+1) == '-'
	   && *(line+2) == ' ' && !*(line+3))){
	*((int *) local) = 1;
	return(2);		/* skip this line! */
    }

    return(0);
}



/*----------------------------------------------------------------------
  return the c-client reference name for the given end_body part
  ----*/
char *
body_partno(stream, msgno, end_body)
    MAILSTREAM *stream;
    long	msgno;
    BODY       *end_body;
{
    BODY *body;

    (void) pine_mail_fetchstructure(stream, msgno, &body);
    return(partno(body, end_body));
}



/*----------------------------------------------------------------------
  return the c-client reference name for the given end_body part
  ----*/
char *
partno(body, end_body)
     BODY *body, *end_body;
{
    PART *part;
    int   num = 0;
    char  tmp[64], *p = NULL;

    if(body && body->type == TYPEMULTIPART) {
	part = body->nested.part;	/* first body part */

	do {				/* for each part */
	    num++;
	    if(&part->body == end_body || (p = partno(&part->body, end_body))){
		sprintf(tmp, "%d%s%.*s", num, (p) ? "." : "",
			sizeof(tmp)-10, (p) ? p : "");
		if(p)
		  fs_give((void **)&p);

		return(cpystr(tmp));
	    }
	} while (part = part->next);	/* until done */

	return(NULL);
    }
    else if(body && body->type == TYPEMESSAGE && body->subtype 
	    && !strucmp(body->subtype, "rfc822")){
	return(partno(body->nested.msg->body, end_body));
    }

    return((body == end_body) ? cpystr("1") : NULL);
}



/*----------------------------------------------------------------------
   Fill in the contents of each body part

Args: stream      -- Stream the message is on
      msgno       -- Message number the body structure is for
      section	  -- body section associated with body pointer
      body        -- Body pointer to fill in

Result: 1 if all went OK, 0 if there was a problem

This function copies the contents from an original message/body to
a new message/body.  It recurses down all multipart levels.

If one or more part (but not all) can't be fetched, a status message
will be queued.
 ----*/
int
fetch_contents(stream, msgno, section, body)
     MAILSTREAM *stream;
     long        msgno;
     char	*section;
     BODY       *body;
{
    char *tp;
    int   got_one = 0;

    if(!body->id)
      body->id = generate_message_id();
          
    if(body->type == TYPEMULTIPART){
	char  subsection[256], *subp;
	int   n, last_one = 10;		/* remember worst case */
	PART *part     = body->nested.part;

	if(!(part = body->nested.part))
	  return(0);

	subp = subsection;
	if(section && *section){
	    for(n = 0;
		n < sizeof(subsection)-20 && (*subp = section[n]); n++, subp++)
	      ;

	    *subp++ = '.';
	}

	n = 1;
	do {
	    sprintf(subp, "%d", n++);
	    got_one  = fetch_contents(stream, msgno, subsection, &part->body);
	    last_one = min(last_one, got_one);
	}
	while(part = part->next);

	return(last_one);
    }

    if(body->contents.text.data)
      return(1);			/* already taken care of... */

    if(body->type == TYPEMESSAGE){
	if(body->subtype && strucmp(body->subtype,"external-body")){
	    /*
	     * the idea here is to fetch everything into storage objects
	     */
	    body->contents.text.data = (void *) so_get(PART_SO_TYPE, NULL,
						    EDIT_ACCESS);
#if	defined(DOS) && !defined(WIN32)
	    if(body->contents.text.data){
		/* fetch text to disk */
		mail_parameters(stream, SET_GETS, (void *)dos_gets);
		append_file =(FILE *)so_text((STORE_S *)body->contents.text.data);

		if(mail_fetchbody(stream, msgno, section, &body->size.bytes)){
		    so_release((STORE_S *)body->contents.text.data);
		    got_one = 1;
		}
		else
		  q_status_message1(SM_ORDER | SM_DING, 3, 3,
				    "Error fetching part %.200s", section);

		/* next time body may stay in core */
		mail_parameters(stream, SET_GETS, (void *)NULL);
		append_file = NULL;
		mail_gc(stream, GC_TEXTS);
	    }
#else
	    if(body->contents.text.data
	       && (tp = pine_mail_fetch_body(stream, msgno, section,
				       &body->size.bytes, NIL))){
	        so_truncate((STORE_S *)body->contents.text.data, 
			    body->size.bytes + 2048);
		so_nputs((STORE_S *)body->contents.text.data, tp,
			 body->size.bytes);
		got_one = 1;
	    }
#endif
	    else
	      q_status_message1(SM_ORDER | SM_DING, 3, 3,
				"Error fetching part %.200s", section);
	} else {
	    got_one = 1;
	}
    } else {
	/*
	 * the idea here is to fetch everything into storage objects
	 * so, grab one, then fetch the body part
	 */
	body->contents.text.data = (void *)so_get(PART_SO_TYPE,NULL,EDIT_ACCESS);
#if	defined(DOS) && !defined(WIN32)
	if(body->contents.text.data){
	    /* write fetched text to disk */
	    mail_parameters(stream, SET_GETS, (void *)dos_gets);
	    append_file = (FILE *)so_text((STORE_S *)body->contents.text.data);
	    if(mail_fetchbody(stream, msgno, section, &body->size.bytes)){
		so_release((STORE_S *)body->contents.text.data);
		got_one = 1;
	    }
	    else
	      q_status_message1(SM_ORDER | SM_DING, 3, 3,
				"Error fetching part %.200s", section);

	    /* next time body may stay in core */
	    mail_parameters(stream, SET_GETS, (void *)NULL);
	    append_file = NULL;
	    mail_gc(stream, GC_TEXTS);
	}
#else
	if(body->contents.text.data
	   && (tp=pine_mail_fetch_body(stream, msgno, section,
				       &body->size.bytes, NIL))){
	    so_truncate((STORE_S *)body->contents.text.data, 
			    body->size.bytes + 2048);
	    so_nputs((STORE_S *)body->contents.text.data, tp,
		     body->size.bytes);
	    got_one = 1;
	}
#endif
	else
	  q_status_message1(SM_ORDER | SM_DING, 3, 3,
			    "Error fetching part %.200s", section);
    }

    return(got_one);
}



/*----------------------------------------------------------------------
    Copy the body structure

Args: new_body -- Pointer to already allocated body, or NULL, if none
      old_body -- The Body to copy


 This is traverses the body structure recursively copying all elements.
The new_body parameter can be NULL in which case a new body is
allocated. Alternatively it can point to an already allocated body
structure. This is used when copying body parts since a PART includes a 
BODY. The contents fields are *not* filled in.
  ----*/

BODY *
copy_body(new_body, old_body)
     BODY *old_body, *new_body;
{
    if(old_body == NULL)
      return(NULL);

    if(new_body == NULL)
      new_body = mail_newbody();

    new_body->type = old_body->type;
    new_body->encoding = old_body->encoding;

    if(old_body->subtype)
      new_body->subtype = cpystr(old_body->subtype);

    new_body->parameter = copy_parameters(old_body->parameter);

    if(old_body->id)
      new_body->id = cpystr(old_body->id);

    if(old_body->description)
      new_body->description = cpystr(old_body->description);

    if(old_body->disposition.type)
      new_body->disposition.type = cpystr(old_body->disposition.type);

    new_body->disposition.parameter
			    = copy_parameters(old_body->disposition.parameter);
    
    new_body->size = old_body->size;

    if(new_body->type == TYPEMESSAGE
       && new_body->subtype && !strucmp(new_body->subtype, "rfc822")){
	new_body->nested.msg = mail_newmsg();
	new_body->nested.msg->body
				 = copy_body(NULL, old_body->nested.msg->body);
    }
    else if(new_body->type == TYPEMULTIPART) {
	PART **new_partp, *old_part;

	new_partp = &new_body->nested.part;
	for(old_part = old_body->nested.part;
	    old_part != NULL;
	    old_part = old_part->next){
	    *new_partp = mail_newbody_part();
            copy_body(&(*new_partp)->body, &old_part->body);
	    new_partp = &(*new_partp)->next;
        }
    }

    return(new_body);
}



/*----------------------------------------------------------------------
    Copy the MIME parameter list
 
 Allocates storage for new part, and returns pointer to new paramter
list. If old_p is NULL, NULL is returned.
 ----*/

PARAMETER *
copy_parameters(old_p)
     PARAMETER *old_p;
{
    PARAMETER *new_p, *p1, *p2;

    if(old_p == NULL)
      return((PARAMETER *)NULL);

    new_p = p2 = NULL;
    for(p1 = old_p; p1 != NULL; p1 = p1->next) {
        if(new_p == NULL) {
            p2 = mail_newbody_parameter();
            new_p = p2;
        } else {
            p2->next = mail_newbody_parameter();
            p2 = p2->next;
        }
        p2->attribute = cpystr(p1->attribute);
        p2->value     = cpystr(p1->value);
    }
    return(new_p);
}
    
    

/*----------------------------------------------------------------------
    Make a complete copy of an envelope and all it's fields

Args:    e -- the envelope to copy

Result:  returns the new envelope, or NULL, if the given envelope was NULL

  ----*/

ENVELOPE *    
copy_envelope(e)
     register ENVELOPE *e;
{
    register ENVELOPE *e2;

    if(!e)
      return(NULL);

    e2		    = mail_newenvelope();
    e2->remail      = e->remail	     ? cpystr(e->remail)	      : NULL;
    e2->return_path = e->return_path ? rfc822_cpy_adr(e->return_path) : NULL;
    e2->date        = e->date	     ? (unsigned char *)cpystr((char *) e->date)
								      : NULL;
    e2->from        = e->from	     ? rfc822_cpy_adr(e->from)	      : NULL;
    e2->sender      = e->sender	     ? rfc822_cpy_adr(e->sender)      : NULL;
    e2->reply_to    = e->reply_to    ? rfc822_cpy_adr(e->reply_to)    : NULL;
    e2->subject     = e->subject     ? cpystr(e->subject)	      : NULL;
    e2->to          = e->to          ? rfc822_cpy_adr(e->to)	      : NULL;
    e2->cc          = e->cc          ? rfc822_cpy_adr(e->cc)	      : NULL;
    e2->bcc         = e->bcc         ? rfc822_cpy_adr(e->bcc)	      : NULL;
    e2->in_reply_to = e->in_reply_to ? cpystr(e->in_reply_to)	      : NULL;
    e2->newsgroups  = e->newsgroups  ? cpystr(e->newsgroups)	      : NULL;
    e2->message_id  = e->message_id  ? cpystr(e->message_id)	      : NULL;
    e2->references  = e->references  ? cpystr(e->references)          : NULL;
    e2->followup_to = e->followup_to ? cpystr(e->references)          : NULL;
    return(e2);
}


/*----------------------------------------------------------------------
     Generate the "In-reply-to" text from message header

  Args: message -- Envelope of original message

  Result: returns an alloc'd string or NULL if there is a problem
 ----*/
char *
reply_in_reply_to(env)
    ENVELOPE *env;
{
    return((env && env->message_id) ? cpystr(env->message_id) : NULL);
}


/*----------------------------------------------------------------------
        Generate a unique message id string.

   Args: ps -- The usual pine structure

  Result: Alloc'd unique string is returned

Uniqueness is gaurenteed by using the host name, process id, date to the
second and a single unique character
*----------------------------------------------------------------------*/
char *
generate_message_id()
{
    static short osec = 0, cnt = 0;
    char        *id;
    time_t       now;
    struct tm   *now_x;
    char        *hostpart = NULL;

    now   = time((time_t *)0);
    now_x = localtime(&now);
    id    = (char *)fs_get(128 * sizeof(char));

    if(now_x->tm_sec == osec)
      cnt++;
    else{
	cnt = 0;
	osec = now_x->tm_sec;
    }

    hostpart = F_ON(F_ROT13_MESSAGE_ID, ps_global)
		 ? rot13(ps_global->hostname)
		 : cpystr(ps_global->hostname);
    
    if(!hostpart)
      hostpart = cpystr("huh");

    sprintf(id,"<Pine.%.4s.%.20s.%02d%02d%02d%02d%02d%02d%X.%d@%.50s>",
	    SYSTYPE, pine_version, (now_x->tm_year) % 100, now_x->tm_mon + 1,
	    now_x->tm_mday, now_x->tm_hour, now_x->tm_min, now_x->tm_sec, 
	    cnt, getpid(), hostpart);

    if(hostpart)
      fs_give((void **) &hostpart);

    return(id);
}


char *
rot13(src)
    char *src;
{
    char byte, cap, *p, *ret = NULL;

    if(src && *src){
	ret = (char *) fs_get((strlen(src)+1) * sizeof(char));
	p = ret;
	while(byte = *src++){
	    cap   = byte & 32;
	    byte &= ~cap;
	    *p++ = ((byte >= 'A') && (byte <= 'Z')
		    ? ((byte - 'A' + 13) % 26 + 'A') : byte) | cap;
	}

	*p = '\0';
    }

    return(ret);
}


/*----------------------------------------------------------------------
  Return the first true address pointer (modulo group syntax allowance)

  Args: addr  -- Address list

 Result: First real address pointer, or NULL
  ----------------------------------------------------------------------*/
ADDRESS *
first_addr(addr)
    ADDRESS *addr;
{
    while(addr && !addr->host)
      addr = addr->next;

    return(addr);
}


/*----------------------------------------------------------------------
  Acquire the pinerc defined signature file
  It is allocated here and freed by the caller.

          file -- use this file
   prenewlines -- prefix the file contents with this many newlines
  postnewlines -- postfix the file contents with this many newlines
        is_sig -- this is a signature (not a template)
  ----*/
char *
get_signature_file(file, prenewlines, postnewlines, is_sig)
    char *file;
    int   prenewlines,
	  postnewlines,
	  is_sig;
{
    char     *sig, *tmp_sig = NULL, sig_path[MAXPATH+1];
    int       len, do_the_pipe_thang = 0;
    long      sigsize = 0L, cntdown;

    sig_path[0] = '\0';
    if(!signature_path(file, sig_path, MAXPATH))
      return(NULL);

    dprint(5, (debugfile, "get_signature(%s)\n", sig_path));

    if(sig_path[(len=strlen(sig_path))-1] == '|'){
	if(is_sig && F_ON(F_DISABLE_PIPES_IN_SIGS, ps_global)){
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Pipes for signatures are administratively disabled");
	    return(NULL);
	}
	else if(!is_sig && F_ON(F_DISABLE_PIPES_IN_TEMPLATES, ps_global)){
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Pipes for templates are administratively disabled");
	    return(NULL);
	}
	    
	sig_path[len-1] = '\0';
	removing_trailing_white_space(sig_path);
	do_the_pipe_thang++;
    }

    if(!IS_REMOTE(sig_path) && ps_global->VAR_OPER_DIR &&
       !in_dir(ps_global->VAR_OPER_DIR, sig_path)){
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Can't read file outside %.200s: %.200s",
			  ps_global->VAR_OPER_DIR, file);
	
	return(NULL);
    }

    if(IS_REMOTE(sig_path) || can_access(sig_path, ACCESS_EXISTS) == 0){
	if(do_the_pipe_thang){
	    if(can_access(sig_path, EXECUTE_ACCESS) == 0){
		STORE_S  *store;
		int       flags;
		PIPE_S   *syspipe;
		gf_io_t   pc, gc;
		long      start;

		if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){

		    flags = PIPE_READ | PIPE_STDERR | PIPE_NOSHELL;

		    start = time(0);

		    if(syspipe = open_system_pipe(sig_path,NULL,NULL,flags,5)){
			unsigned char c;
			char         *error, *q;

			gf_set_so_writec(&pc, store);
			gf_set_readc(&gc, (void *)syspipe, 0, PipeStar);
			gf_filter_init();

			if((error = gf_pipe(gc, pc)) != NULL){
			    (void)close_system_pipe(&syspipe, NULL, 0);
			    gf_clear_so_writec(store);
			    so_give(&store);
			    q_status_message1(SM_ORDER | SM_DING, 3, 4,
					      "Can't get file: %.200s", error);
			    return(NULL);
			}

			if(close_system_pipe(&syspipe, NULL, 0)){
			    long now;

			    now = time(0);
			    q_status_message2(SM_ORDER, 3, 4,
				    "Error running program \"%.200s\"%.200s",
				    file,
				    (now - start > 4) ? ": timed out" : "");
			}

			gf_clear_so_writec(store);

			/* rewind and count chars */
			so_seek(store, 0L, 0);
			while(so_readc(&c, store) && sigsize < 100000L)
			  sigsize++;

			/* allocate space */
			tmp_sig = fs_get((sigsize + 1) * sizeof(char));
			tmp_sig[0] = '\0';
			q = tmp_sig;

			/* rewind and copy chars, no prenewlines... */
			so_seek(store, 0L, 0);
			cntdown = sigsize;
			while(so_readc(&c, store) && cntdown-- > 0L)
			  *q++ = c;
			
			*q = '\0';
			so_give(&store);
		    }
		    else{
			so_give(&store);
			q_status_message1(SM_ORDER | SM_DING, 3, 4,
				     "Error running program \"%.200s\"",
				     file);
		    }
		}
		else
		  q_status_message(SM_ORDER | SM_DING, 3, 4,
			  "Error allocating space for sig or template program");
	    }
	    else
	      q_status_message1(SM_ORDER | SM_DING, 3, 4,
				"Can't execute \"%.200s\": Permission denied",
				sig_path);
	}
	else if((IS_REMOTE(sig_path) &&
		 (tmp_sig = read_remote_sigfile(sig_path))) ||
		(tmp_sig = read_file(sig_path)))
	  sigsize = strlen(tmp_sig);
	else
	  q_status_message2(SM_ORDER | SM_DING, 3, 4,
			    "Error \"%.200s\" reading file \"%.200s\"",
			    error_description(errno), sig_path);
    }

    sig = get_signature_lit(tmp_sig, prenewlines, postnewlines, is_sig, 0);
    if(tmp_sig)
      fs_give((void **)&tmp_sig);

    return(sig);
}


/*----------------------------------------------------------------------
           lit -- this is the source
   prenewlines -- prefix the file contents with this many newlines
  postnewlines -- postfix the file contents with this many newlines
        is_sig -- this is a signature (not a template)
  decode_constants -- change C-style constants into their values
  ----*/
char *
get_signature_lit(lit, prenewlines, postnewlines, is_sig, decode_constants)
    char *lit;
    int   prenewlines,
	  postnewlines,
	  is_sig,
	  decode_constants;
{
    char *sig = NULL;

    /*
     * Should make this smart enough not to do the copying and double
     * allocation of space.
     */
    if(lit){
	char  *tmplit = NULL, *p, *q, *d, save;
	size_t len;

	if(decode_constants){
	    tmplit = (char *) fs_get((strlen(lit)+1) * sizeof(char));
	    tmplit[0] = '\0';
	    cstring_to_string(lit, tmplit);
	}
	else
	  tmplit = cpystr(lit);

	sig = (char *) fs_get(len=(strlen(tmplit) + 5 +
				   (prenewlines+postnewlines) *
				     strlen(NEWLINE) + 1) * sizeof(char));
	memset(sig, 0, len);
	d = sig;
	while(prenewlines--)
	  sstrcpy(&d, NEWLINE);

	if(is_sig && F_ON(F_ENABLE_SIGDASHES, ps_global) &&
	   !sigdashes_are_present(tmplit)){
	    sstrcpy(&d, SIGDASHES);
	    sstrcpy(&d, NEWLINE);
	}

	p = tmplit;
	while(*p){
	    /* get a line */
	    q = strpbrk(p, "\n\r");
	    if(q){
		save = *q;
		*q = '\0';
	    }

	    /*
	     * Strip trailing space if we are doing a signature and
	     * this line is not sigdashes.
	     */
	    if(is_sig && strcmp(p, SIGDASHES))
	      removing_trailing_white_space(p);

	    while((*d = *p++) != '\0')
	      d++;

	    if(q){
		*d++ = save;
		p = q+1;
	    }
	    else
	      break;
	}

	while(postnewlines--)
	  sstrcpy(&d, NEWLINE);

	*d = '\0';
	
	if(tmplit)
	  fs_give((void **) &tmplit);
    }

    return(sig);
}


int
sigdashes_are_present(sig)
    char *sig;
{
    char *p;

    p = srchstr(sig, SIGDASHES);
    while(p && !((p == sig || (p[-1] == '\n' || p[-1] == '\r')) &&
	         (p[3] == '\0' || p[3] == '\n' || p[3] == '\r')))
      p = srchstr(p+1, SIGDASHES);
    
    return(p ? 1 : 0);
}


/*----------------------------------------------------------------------
  Acquire the pinerc defined signature file pathname

  ----*/
char *
signature_path(sname, sbuf, len)
    char   *sname, *sbuf;
    size_t  len;
{
    *sbuf = '\0';
    if(sname && *sname){
	size_t spl = strlen(sname);
	if(IS_REMOTE(sname)){
	    if(spl < len - 1)
	      strncpy(sbuf, sname, len-1);
	}
	else if(is_absolute_path(sname)){
	    strncpy(sbuf, sname, len-1);
	    sbuf[len-1] = '\0';
	    fnexpand(sbuf, len);
	}
	else if(ps_global->VAR_OPER_DIR){
	    if(strlen(ps_global->VAR_OPER_DIR) + spl < len - 1)
	      build_path(sbuf, ps_global->VAR_OPER_DIR, sname, len);
	}
	else{
	    char *lc = last_cmpnt(ps_global->pinerc);

	    sbuf[0] = '\0';
	    if(lc != NULL){
		strncpy(sbuf,ps_global->pinerc,min(len-1,lc-ps_global->pinerc));
		sbuf[min(len-1,lc-ps_global->pinerc)] = '\0';
	    }

	    strncat(sbuf, sname, max(len-1-strlen(sbuf), 0));
	}
    }

    return(*sbuf ? sbuf : NULL);
}


char *
read_remote_sigfile(name)
    char *name;
{
    int        try_cache;
    REMDATA_S *rd;
    char      *file = NULL;


    dprint(7, (debugfile, "read_remote_sigfile \"%s\"\n", name ? name : "?"));

    /*
     * We could parse the name here to find what type it is. So far we
     * only have type RemImap.
     */
    rd = rd_create_remote(RemImap, name, (void *)REMOTE_SIG_SUBTYPE,
			  NULL, "Error: ", "Can't fetch remote configuration.");
    if(!rd)
      goto bail_out;
    
    try_cache = rd_read_metadata(rd);

    if(rd->access == MaybeRorW){
	if(rd->read_status == 'R')
	  rd->access = ReadOnly;
	else
	  rd->access = ReadWrite;
    }

    if(rd->access != NoExists){

	rd_check_remvalid(rd, 1L);

	/*
	 * If the cached info says it is readonly but
	 * it looks like it's been fixed now, change it to readwrite.
	 */
        if(rd->read_status == 'R'){
	    /*
	     * We go to this trouble since readonly sigfiles
	     * are likely a mistake. They are usually supposed to be
	     * readwrite so we open it and check if it's been fixed.
	     */
	    rd_check_readonly_access(rd);
	    if(rd->read_status == 'W'){
		rd->access = ReadWrite;
		rd->flags |= REM_OUTOFDATE;
	    }
	    else
	      rd->access = ReadOnly;
	}

	if(rd->flags & REM_OUTOFDATE){
	    if(rd_update_local(rd) != 0){

		dprint(1, (debugfile,
		       "read_remote_sigfile: rd_update_local failed\n"));
		/*
		 * Don't give up altogether. We still may be
		 * able to use a cached copy.
		 */
	    }
	    else{
		dprint(7, (debugfile,
		       "%s: copied remote to local (%ld)\n",
		       rd->rn ? rd->rn : "?", (long)rd->last_use));
	    }
	}

	if(rd->access == ReadWrite)
	  rd->flags |= DO_REMTRIM;
    }

    /* If we couldn't get to remote folder, try using the cached copy */
    if(rd->access == NoExists || rd->flags & REM_OUTOFDATE){
	if(try_cache){
	    rd->access = ReadOnly;
	    rd->flags |= USE_OLD_CACHE;
	    q_status_message(SM_ORDER, 3, 4,
	     "Can't contact remote sig server, using cached copy");
	    dprint(2, (debugfile,
    "Can't open remote sigfile %s, using local cached copy %s readonly\n",
		   rd->rn ? rd->rn : "?",
		   rd->lf ? rd->lf : "?"));
	}
	else{
	    rd->flags &= ~DO_REMTRIM;
	    goto bail_out;
	}
    }

    file = read_file(rd->lf);

bail_out:
    if(rd)
      rd_close_remdata(&rd);

    return(file);
}


/*----------------------------------------------------------------------
    Serve up the current signature within pico for editing

    Args: file to edit

  Result: signature changed or not.
  ---*/
char *
signature_edit(sigfile, title)
    char  *sigfile;
    char  *title;
{
    int	     editor_result;
    char     sig_path[MAXPATH+1], errbuf[2000], *errstr = NULL;
    char    *ret = NULL;
    STORE_S *msgso, *tmpso = NULL;
    gf_io_t  gc, pc;
    PICO     pbf;
    struct variable *vars = ps_global->vars;
    REMDATA_S *rd = NULL;

    if(!signature_path(sigfile, sig_path, MAXPATH))
      return(cpystr("No signature file defined."));
    
    if(IS_REMOTE(sigfile)){
	rd = rd_create_remote(RemImap, sig_path, (void *)REMOTE_SIG_SUBTYPE,
			      NULL, "Error: ",
			      "Can't access remote configuration.");
	if(!rd)
	  return(cpystr("Error attempting to edit remote configuration"));
	
	(void)rd_read_metadata(rd);

	if(rd->access == MaybeRorW){
	    if(rd->read_status == 'R')
	      rd->access = ReadOnly;
	    else
	      rd->access = ReadWrite;
	}

	if(rd->access != NoExists){

	    rd_check_remvalid(rd, 1L);

	    /*
	     * If the cached info says it is readonly but
	     * it looks like it's been fixed now, change it to readwrite.
	     */
	    if(rd->read_status == 'R'){
		rd_check_readonly_access(rd);
		if(rd->read_status == 'W'){
		    rd->access = ReadWrite;
		    rd->flags |= REM_OUTOFDATE;
		}
		else
		  rd->access = ReadOnly;
	    }
	}

	if(rd->flags & REM_OUTOFDATE){
	    if(rd_update_local(rd) != 0){

		dprint(1, (debugfile,
		       "signature_edit: rd_update_local failed\n"));
		rd_close_remdata(&rd);
		return(cpystr("Can't access remote sig"));
	    }
	}
	else
	  rd_open_remote(rd);

	if(rd->access != ReadWrite || rd_remote_is_readonly(rd)){
	    rd_close_remdata(&rd);
	    return(cpystr("Can't get write permission for remote sig"));
	}

	rd->flags |= DO_REMTRIM;

	strncpy(sig_path, rd->lf, sizeof(sig_path)-1);
	sig_path[sizeof(sig_path)-1] = '\0';
    }

    standard_picobuf_setup(&pbf);
    pbf.tty_fix       = PineRaw;
    pbf.composer_help = h_composer_sigedit;
    pbf.exittest      = sigedit_exit_for_pico;
    pbf.upload	       = (VAR_UPLOAD_CMD && VAR_UPLOAD_CMD[0])
			   ? upload_msg_to_pico : NULL;
    pbf.alt_ed        = (VAR_EDITOR && VAR_EDITOR[0] && VAR_EDITOR[0][0])
			    ? VAR_EDITOR : NULL;
    pbf.alt_spell     = (VAR_SPELLER && VAR_SPELLER[0]) ? VAR_SPELLER : NULL;
    pbf.always_spell_check = F_ON(F_ALWAYS_SPELL_CHECK, ps_global);
    pbf.strip_ws_before_send = F_ON(F_STRIP_WS_BEFORE_SEND, ps_global);
    pbf.allow_flowed_text = 0;

    pbf.pine_anchor   = set_titlebar(title,
				      ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,
				      ps_global->msgmap,
				      0, FolderName, 0, 0, NULL);

    /* NOTE: at this point, alot of pico struct fields are null'd out
     * thanks to the leading memset; in particular "headents" which tells
     * pico to behave like a normal editor (though modified slightly to
     * let the caller dictate the file to edit and such)...
     */

    if(VAR_OPER_DIR && !in_dir(VAR_OPER_DIR, sig_path)){
	ret = (char *)fs_get((strlen(VAR_OPER_DIR) + 100) * sizeof(char));
	sprintf(ret, "Can't edit file outside of %s", VAR_OPER_DIR);
	return(ret);
    }

    /*
     * Now alloc and init the text to pass pico
     */
    if(!(msgso = so_get(PicoText, NULL, EDIT_ACCESS))){
	ret = cpystr("Error allocating space for file");
	dprint(1, (debugfile, "Can't alloc space for signature_edit"));
	return(ret);
    }
    else
      pbf.msgtext = so_text(msgso);

    if(can_access(sig_path, READ_ACCESS) == 0
       && !(tmpso = so_get(FileStar, sig_path, READ_ACCESS))){
	char *problem = error_description(errno);

	sprintf(errbuf, "Error editing \"%.500s\": %.500s",
		sig_path, problem ? problem : "<NULL>");
	ret = cpystr(errbuf);

	dprint(1, (debugfile, "signature_edit: can't open %s: %s", sig_path,
		   problem ? problem : "<NULL>"));
	return(ret);
    }
    else if(tmpso){			/* else, fill pico's edit buffer */
	gf_set_so_readc(&gc, tmpso);	/* read from file, write pico buf */
	gf_set_so_writec(&pc, msgso);
	gf_filter_init();		/* no filters needed */
	if(errstr = gf_pipe(gc, pc)){
	    sprintf(errbuf, "Error reading file: \"%.500s\"", errstr);
	    ret = cpystr(errbuf);
	}

	gf_clear_so_readc(tmpso);
	gf_clear_so_writec(msgso);
	so_give(&tmpso);
    }

    if(!errstr){
#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_COMPOSER);
#endif

	/*------ OK, Go edit the signature ------*/
	editor_result = pico(&pbf);

#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_DEFAULT);
#endif
	if(editor_result & COMP_GOTHUP){
	    hup_signal();		/* do what's normal for a hup */
	}
	else{
	    fix_windsize(ps_global);
	    init_signals();
	}

	if(editor_result & (COMP_SUSPEND | COMP_GOTHUP | COMP_CANCEL)){
	}
	else{
            /*------ Must have an edited buffer, write it to .sig -----*/
	    unlink(sig_path);		/* blast old copy */
	    if(tmpso = so_get(FileStar, sig_path, WRITE_ACCESS)){
		so_seek(msgso, 0L, 0);
		gf_set_so_readc(&gc, msgso);	/* read from pico buf */
		gf_set_so_writec(&pc, tmpso);	/* write sig file */
		gf_filter_init();		/* no filters needed */
		if(errstr = gf_pipe(gc, pc)){
		    sprintf(errbuf, "Error writing file: \"%.500s\"",
				      errstr);
		    ret = cpystr(errbuf);
		}

		gf_clear_so_readc(msgso);
		gf_clear_so_writec(tmpso);
		if(so_give(&tmpso)){
		    errstr = error_description(errno);
		    sprintf(errbuf, "Error writing file: \"%.500s\"",
				      errstr);
		    ret = cpystr(errbuf);
		}

		if(IS_REMOTE(sigfile)){
		    int   e, we_cancel;
		    char datebuf[200];

		    datebuf[0] = '\0';

		    we_cancel = busy_alarm(1, "Copying to remote sig", NULL, 0);
		    if((e = rd_update_remote(rd, datebuf)) != 0){
			if(e == -1){
			    q_status_message2(SM_ORDER | SM_DING, 3, 5,
			      "Error opening temporary sig file %.200s: %.200s",
				rd->lf, error_description(errno));
			    dprint(1, (debugfile,
			       "write_remote_sig: error opening temp file %s\n",
			       rd->lf ? rd->lf : "?"));
			}
			else{
			    q_status_message2(SM_ORDER | SM_DING, 3, 5,
					    "Error copying to %.200s: %.200s",
					    rd->rn, error_description(errno));
			    dprint(1, (debugfile,
			      "write_remote_sig: error copying from %s to %s\n",
			      rd->lf ? rd->lf : "?", rd->rn ? rd->rn : "?"));
			}
			
			q_status_message(SM_ORDER | SM_DING, 5, 5,
	   "Copy of sig to remote folder failed, changes NOT saved remotely");
		    }
		    else{
			rd_update_metadata(rd, datebuf);
			rd->read_status = 'W';
		    }

		    rd_close_remdata(&rd);

		    if(we_cancel)
		      cancel_busy_alarm(-1);
		}
	    }
	    else{
		sprintf(errbuf, "Error writing \"%.500s\"", sig_path);
		ret = cpystr(errbuf);
		dprint(1, (debugfile, "signature_edit: can't write %s",
			   sig_path));
	    }
	}
    }
    
    standard_picobuf_teardown(&pbf);
    so_give(&msgso);
    return(ret);
}


/*----------------------------------------------------------------------
    Serve up the current signature within pico for editing

    Args: literal signature to edit

  Result: raw edited signature is returned in result arg
  ---*/
char *
signature_edit_lit(litsig, result, title, composer_help)
    char  *litsig;
    char **result;
    char  *title;
    HelpType composer_help;
{
    int	     editor_result;
    char    *errstr = NULL;
    char    *ret = NULL;
    STORE_S *msgso;
    PICO     pbf;
    struct variable *vars = ps_global->vars;

    standard_picobuf_setup(&pbf);
    pbf.tty_fix       = PineRaw;
    pbf.search_help   = h_sigedit_search;
    pbf.composer_help = composer_help;
    pbf.exittest      = sigedit_exit_for_pico;
    pbf.upload	       = (VAR_UPLOAD_CMD && VAR_UPLOAD_CMD[0])
			   ? upload_msg_to_pico : NULL;
    pbf.alt_ed        = (VAR_EDITOR && VAR_EDITOR[0] && VAR_EDITOR[0][0])
			    ? VAR_EDITOR : NULL;
    pbf.alt_spell     = (VAR_SPELLER && VAR_SPELLER[0]) ? VAR_SPELLER : NULL;
    pbf.always_spell_check = F_ON(F_ALWAYS_SPELL_CHECK, ps_global);
    pbf.strip_ws_before_send = F_ON(F_STRIP_WS_BEFORE_SEND, ps_global);
    pbf.allow_flowed_text = 0;

    pbf.pine_anchor   = set_titlebar(title,
				      ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,
				      ps_global->msgmap,
				      0, FolderName, 0, 0, NULL);

    /* NOTE: at this point, alot of pico struct fields are null'd out
     * thanks to the leading memset; in particular "headents" which tells
     * pico to behave like a normal editor (though modified slightly to
     * let the caller dictate the file to edit and such)...
     */

    /*
     * Now alloc and init the text to pass pico
     */
    if(!(msgso = so_get(PicoText, NULL, EDIT_ACCESS))){
	ret = cpystr("Error allocating space");
	dprint(1, (debugfile, "Can't alloc space for signature_edit_lit"));
	return(ret);
    }
    else
      pbf.msgtext = so_text(msgso);
    
    so_puts(msgso, litsig ? litsig : "");


    if(!errstr){
#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_COMPOSER);
#endif

	/*------ OK, Go edit the signature ------*/
	editor_result = pico(&pbf);

#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_DEFAULT);
#endif
	if(editor_result & COMP_GOTHUP){
	    hup_signal();		/* do what's normal for a hup */
	}
	else{
	    fix_windsize(ps_global);
	    init_signals();
	}

	if(editor_result & (COMP_SUSPEND | COMP_GOTHUP | COMP_CANCEL)){
	    ret = cpystr("Signature Edit Cancelled");
	}
	else{
            /*------ Must have an edited buffer, write it to .sig -----*/
	    unsigned char c;
	    int cnt = 0;
	    char *p;

	    so_seek(msgso, 0L, 0);
	    while(so_readc(&c, msgso))
	      cnt++;
	    
	    *result = (char *)fs_get((cnt+1) * sizeof(char));
	    p = *result;
	    so_seek(msgso, 0L, 0);
	    while(so_readc(&c, msgso))
	      *p++ = c;
	    
	    *p = '\0';
	}
    }
    
    standard_picobuf_teardown(&pbf);
    so_give(&msgso);
    return(ret);
}


/*
 *
 */
char *
sigedit_exit_for_pico(he, redraw_pico, allow_flowed)
    struct headerentry *he;
    void (*redraw_pico)();
    int  allow_flowed;
{
    int	      rv;
    char     *rstr = NULL;
    void    (*redraw)() = ps_global->redrawer;
    static ESCKEY_S opts[] = {
	{'y', 'y', "Y", "Yes"},
	{'n', 'n', "N", "No"},
	{-1, 0, NULL, NULL}
    };

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);

    while(1){
	rv = radio_buttons("Exit editor and apply changes? ",
			   -FOOTER_ROWS(ps_global), opts,
			   'y', 'x', NO_HELP, RB_NORM, NULL);
	if(rv == 'y'){				/* user ACCEPTS! */
	    break;
	}
	else if(rv == 'n'){			/* Declined! */
	    rstr = "No Changes Saved";
	    break;
	}
	else if(rv == 'x'){			/* Cancelled! */
	    rstr = "Exit Cancelled";
	    break;
	}
    }

    ps_global->redrawer = redraw;
    return(rstr);
}


/*
 * Common stuff we almost always want to set when calling pico.
 */
void
standard_picobuf_setup(pbf)
    PICO *pbf;
{
    memset(pbf, 0, sizeof(*pbf));

    pbf->pine_version	= pine_version;
    pbf->fillcolumn	= ps_global->composer_fillcol;
    pbf->menu_rows	= FOOTER_ROWS(ps_global) - 1;
    pbf->colors		= colors_for_pico();
    pbf->helper		= helper;
    pbf->showmsg	= display_message_for_pico;
    pbf->suspend	= do_suspend;
    pbf->keybinput	= cmd_input_for_pico;
    pbf->tty_fix	= ttyfix;		/* watch out for this one */
    pbf->newmail	= new_mail_for_pico;
    pbf->ckptdir	= checkpoint_dir_for_pico;
    pbf->resize		= resize_for_pico;
    pbf->winch_cleanup	= winch_cleanup;
    pbf->search_help	= h_composer_search;
    pbf->ins_help	= h_composer_ins;
    pbf->ins_m_help	= h_composer_ins_m;
    pbf->composer_help	= h_composer;
    pbf->browse_help	= h_composer_browse;
    pbf->attach_help	= h_composer_ctrl_j;

    pbf->pine_flags =
       ( (F_ON(F_CAN_SUSPEND,ps_global)			? P_SUSPEND	: 0L)
       | (F_ON(F_USE_FK,ps_global)			? P_FKEYS	: 0L)
       | (ps_global->restricted				? P_SECURE	: 0L)
       | (F_ON(F_ALT_ED_NOW,ps_global)			? P_ALTNOW	: 0L)
       | (F_ON(F_USE_CURRENT_DIR,ps_global)		? P_CURDIR	: 0L)
       | (F_ON(F_SUSPEND_SPAWNS,ps_global)		? P_SUBSHELL	: 0L)
       | (F_ON(F_COMPOSE_MAPS_DEL,ps_global)		? P_DELRUBS	: 0L)
       | (F_ON(F_ENABLE_TAB_COMPLETE,ps_global)		? P_COMPLETE	: 0L)
       | (F_ON(F_SHOW_CURSOR,ps_global)			? P_SHOCUR	: 0L)
       | (F_ON(F_DEL_FROM_DOT,ps_global)		? P_DOTKILL	: 0L)
       | (F_ON(F_ENABLE_DOT_FILES,ps_global)		? P_DOTFILES	: 0L)
       | (F_ON(F_ALLOW_GOTO,ps_global)			? P_ALLOW_GOTO	: 0L)
       | (F_ON(F_ENABLE_SEARCH_AND_REPL,ps_global)	? P_REPLACE	: 0L)
       | (!ps_global->pass_ctrl_chars
          && !ps_global->pass_c1_ctrl_chars		? P_HICTRL	: 0L)
       | ((F_ON(F_ENABLE_ALT_ED,ps_global)
           || F_ON(F_ALT_ED_NOW,ps_global)
	   || (ps_global->VAR_EDITOR
	       && ps_global->VAR_EDITOR[0]
	       && ps_global->VAR_EDITOR[0][0]))
							? P_ADVANCED	: 0L)
       | ((!ps_global->VAR_CHAR_SET
           || !strucmp(ps_global->VAR_CHAR_SET, "US-ASCII"))
							? P_HIBITIGN	: 0L));

    if(ps_global->VAR_OPER_DIR){
	pbf->oper_dir    = ps_global->VAR_OPER_DIR;
	pbf->pine_flags |= P_TREE;
    }
    pbf->home_dir = ps_global->home_dir;
}


void
standard_picobuf_teardown(pbf)
    PICO *pbf;
{
    if(pbf){
	if(pbf->colors)
	  free_pcolors(&pbf->colors);
    }
}
