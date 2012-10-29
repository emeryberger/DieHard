/*
 * Program:	GSSAPI authenticator
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	12 January 1998
 * Last Edited:	15 March 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


long auth_gssapi_valid (void);
long auth_gssapi_client (authchallenge_t challenger,authrespond_t responder,
			 char *service,NETMBX *mb,void *stream,
			 unsigned long *trial,char *user);
long auth_gssapi_client_work (authchallenge_t challenger,gss_buffer_desc chal,
			      authrespond_t responder,char *service,NETMBX *mb,
			      void *stream,char *user,kinit_t ki);
char *auth_gssapi_server (authresponse_t responder,int argc,char *argv[]);


AUTHENTICATOR auth_gss = {
  AU_SECURE | AU_AUTHUSER,	/* secure authenticator */
  "GSSAPI",			/* authenticator name */
  auth_gssapi_valid,		/* check if valid */
  auth_gssapi_client,		/* client method */
  auth_gssapi_server,		/* server method */
  NIL				/* next authenticator */
};

#define AUTH_GSSAPI_P_NONE 1
#define AUTH_GSSAPI_P_INTEGRITY 2
#define AUTH_GSSAPI_P_PRIVACY 4

#define AUTH_GSSAPI_C_MAXSIZE 8192

#define SERVER_LOG(x,y) syslog (LOG_ALERT,x,y)

/* Check if GSSAPI valid on this system
 * Returns: T if valid, NIL otherwise
 */

long auth_gssapi_valid (void)
{
  char tmp[MAILTMPLEN];
  OM_uint32 smn;
  gss_buffer_desc buf;
  gss_name_t name;
				/* make service name */
  sprintf (tmp,"%s@%s",(char *) mail_parameters (NIL,GET_SERVICENAME,NIL),
	   mylocalhost ());
  buf.length = strlen (buf.value = tmp);
				/* see if can build a name */
  if (gss_import_name (&smn,&buf,GSS_C_NT_HOSTBASED_SERVICE,&name) !=
      GSS_S_COMPLETE) return NIL;
				/* remove server method if no keytab */
  if (!kerberos_server_valid ()) auth_gss.server = NIL;
  gss_release_name (&smn,&name);/* finished with name */
  return LONGT;
}

/* Client authenticator
 * Accepts: challenger function
 *	    responder function
 *	    SASL service name
 *	    parsed network mailbox structure
 *	    stream argument for functions
 *	    pointer to current trial count
 *	    returned user name
 * Returns: T if success, NIL otherwise, number of trials incremented if retry
 */

long auth_gssapi_client (authchallenge_t challenger,authrespond_t responder,
			 char *service,NETMBX *mb,void *stream,
			 unsigned long *trial,char *user)
{
  gss_buffer_desc chal;
  kinit_t ki = (kinit_t) mail_parameters (NIL,GET_KINIT,NIL);
  long ret = NIL;
  *trial = 65535;		/* never retry */
				/* get initial (empty) challenge */
  if (chal.value = (*challenger) (stream,(unsigned long *) &chal.length)) {
    if (chal.length) {		/* abort if challenge non-empty */
      mm_log ("Server bug: non-empty initial GSSAPI challenge",WARN);
      (*responder) (stream,NIL,0);
      ret = LONGT;		/* will get a BAD response back */
    }
    else if (mb->authuser[0] && strcmp (mb->authuser,myusername ())) {
      mm_log ("Can't use Kerberos: invalid /authuser",WARN);
      (*responder) (stream,NIL,0);
      ret = LONGT;		/* will get a BAD response back */
    }
    else ret = auth_gssapi_client_work (challenger,chal,responder,service,mb,
					stream,user,ki);
  }
  return ret;
}

/* Client authenticator worker function
 * Accepts: challenger function
 *	    responder function
 *	    SASL service name
 *	    parsed network mailbox structure
 *	    stream argument for functions
 *	    returned user name
 *	    kinit function pointer if should retry with kinit
 * Returns: T if success, NIL otherwise
 */

long auth_gssapi_client_work (authchallenge_t challenger,gss_buffer_desc chal,
			      authrespond_t responder,char *service,NETMBX *mb,
			      void *stream,char *user,kinit_t ki)
{
  char tmp[MAILTMPLEN];
  OM_uint32 smj,smn,dsmj,dsmn;
  OM_uint32 mctx = 0;
  gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
  gss_buffer_desc resp,buf;
  long i;
  int conf;
  gss_qop_t qop;
  gss_name_t crname = NIL;
  blocknotify_t bn = (blocknotify_t) mail_parameters (NIL,GET_BLOCKNOTIFY,NIL);
  void *data;
  long ret = NIL;
  sprintf (tmp,"%s@%s",service,mb->host);
  buf.length = strlen (buf.value = tmp);
				/* get service name */
  if (gss_import_name (&smn,&buf,GSS_C_NT_HOSTBASED_SERVICE,&crname) !=
       GSS_S_COMPLETE) {
    mm_log ("Can't import Kerberos service name",WARN);
    (*responder) (stream,NIL,0);
  }
  else {
    data = (*bn) (BLOCK_SENSITIVE,NIL);
				/* negotiate with KDC */
    smj = gss_init_sec_context (&smn,GSS_C_NO_CREDENTIAL,&ctx,crname,NIL,
				GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG,0,
				GSS_C_NO_CHANNEL_BINDINGS,GSS_C_NO_BUFFER,NIL,
				&resp,NIL,NIL);
    (*bn) (BLOCK_NONSENSITIVE,data);

				/* while continuation needed */
    while (smj == GSS_S_CONTINUE_NEEDED) {
      if (chal.value) fs_give ((void **) &chal.value);
				/* send response, get next challenge */
      i = (*responder) (stream,resp.value,resp.length) &&
	(chal.value = (*challenger) (stream,(unsigned long *) &chal.length));
      gss_release_buffer (&smn,&resp);
      if (i) {			/* negotiate continuation with KDC */
	data = (*bn) (BLOCK_SENSITIVE,NIL);
	switch (smj =		/* make sure continuation going OK */
		gss_init_sec_context (&smn,GSS_C_NO_CREDENTIAL,&ctx,
				      crname,GSS_C_NO_OID,
				      GSS_C_MUTUAL_FLAG|GSS_C_REPLAY_FLAG,0,
				      GSS_C_NO_CHANNEL_BINDINGS,&chal,NIL,
				      &resp,NIL,NIL)) {
	case GSS_S_CONTINUE_NEEDED:
	case GSS_S_COMPLETE:
	  break;
	default:		/* error, don't need context any more */
	  gss_delete_sec_context (&smn,&ctx,NIL);
	}
	(*bn) (BLOCK_NONSENSITIVE,data);
      }
      else {			/* error in continuation */
	mm_log ("Error in negotiating Kerberos continuation",WARN);
	(*responder) (stream,NIL,0);
				/* don't need context any more */
	gss_delete_sec_context (&smn,&ctx,NIL);
	break;
      }
    }

    switch (smj) {		/* done - deal with final condition */
    case GSS_S_COMPLETE:
      if (chal.value) fs_give ((void **) &chal.value);
				/* get prot mechanisms and max size */
      if ((*responder) (stream,resp.value ? resp.value : "",resp.length) &&
	  (chal.value = (*challenger) (stream,(unsigned long *)&chal.length))&&
	  (gss_unwrap (&smn,ctx,&chal,&resp,&conf,&qop) == GSS_S_COMPLETE) &&
	  (resp.length >= 4) && (*((char *) resp.value) & AUTH_GSSAPI_P_NONE)){
				/* make copy of flags and length */
	memcpy (tmp,resp.value,4);
	gss_release_buffer (&smn,&resp);
				/* no session protection */
	tmp[0] = AUTH_GSSAPI_P_NONE;
				/* install user name */
	strcpy (tmp+4,strcpy (user,mb->user[0] ? mb->user : myusername ()));
	buf.value = tmp; buf.length = strlen (user) + 4;
				/* successful negotiation */
	switch (smj = gss_wrap (&smn,ctx,NIL,qop,&buf,&conf,&resp)) {
	case GSS_S_COMPLETE:
	  if ((*responder) (stream,resp.value,resp.length)) ret = T;
	  gss_release_buffer (&smn,&resp);
	  break;
	default:
	  do switch (dsmj = gss_display_status (&dsmn,smj,GSS_C_GSS_CODE,
						GSS_C_NO_OID,&mctx,&resp)) {
	  case GSS_S_COMPLETE:
	    mctx = 0;
	  case GSS_S_CONTINUE_NEEDED:
	    sprintf (tmp,"Unknown gss_wrap failure: %s",(char *) resp.value);
	    mm_log (tmp,WARN);
	    gss_release_buffer (&dsmn,&resp);
	  }
	  while (dsmj == GSS_S_CONTINUE_NEEDED);
	  do switch (dsmj = gss_display_status (&dsmn,smn,GSS_C_MECH_CODE,
						GSS_C_NO_OID,&mctx,&resp)) {
	  case GSS_S_COMPLETE:
	  case GSS_S_CONTINUE_NEEDED:
	    sprintf (tmp,"GSSAPI mechanism status: %s",(char *) resp.value);
	    mm_log (tmp,WARN);
	    gss_release_buffer (&dsmn,&resp);
	  }
	  while (dsmj == GSS_S_CONTINUE_NEEDED);
	  (*responder) (stream,NIL,0);
	}
      }
				/* flush final challenge */
      if (chal.value) fs_give ((void **) &chal.value);
				/* don't need context any more */
      gss_delete_sec_context (&smn,&ctx,NIL);
      break;

    case GSS_S_CREDENTIALS_EXPIRED:
      if (chal.value) fs_give ((void **) &chal.value);
				/* retry if application kinits */
      if (ki && (*ki) (mb->host,"Kerberos credentials expired"))
	ret = auth_gssapi_client_work (challenger,chal,responder,service,mb,
				       stream,user,NIL);
      else {			/* application can't kinit */
	sprintf (tmp,"Kerberos credentials expired (try running kinit) for %s",
		 mb->host);
	mm_log (tmp,WARN);
	(*responder) (stream,NIL,0);
      }
      break;
    case GSS_S_FAILURE:
      if (chal.value) fs_give ((void **) &chal.value);
      do switch (dsmj = gss_display_status (&dsmn,smn,GSS_C_MECH_CODE,
					    GSS_C_NO_OID,&mctx,&resp)) {
      case GSS_S_COMPLETE:	/* end of message, can kinit? */
	if (ki && kerberos_try_kinit (smn) &&
	    (*ki) (mb->host,(char *) resp.value)) {
	  gss_release_buffer (&dsmn,&resp);
	  ret = auth_gssapi_client_work (challenger,chal,responder,service,mb,
					 stream,user,NIL);
	  break;		/* done */
	}
	else (*responder) (stream,NIL,0);
      case GSS_S_CONTINUE_NEEDED:
	sprintf (tmp,kerberos_try_kinit (smn) ?
		 "Kerberos error: %.80s (try running kinit) for %.80s" :
		 "GSSAPI failure: %s for %.80s",(char *) resp.value,mb->host);
	mm_log (tmp,WARN);
	gss_release_buffer (&dsmn,&resp);
      } while (dsmj == GSS_S_CONTINUE_NEEDED);
      break;

    default:			/* miscellaneous errors */
      if (chal.value) fs_give ((void **) &chal.value);
      do switch (dsmj = gss_display_status (&dsmn,smj,GSS_C_GSS_CODE,
					    GSS_C_NO_OID,&mctx,&resp)) {
      case GSS_S_COMPLETE:
	mctx = 0;
      case GSS_S_CONTINUE_NEEDED:
	sprintf (tmp,"Unknown GSSAPI failure: %s",(char *) resp.value);
	mm_log (tmp,WARN);
	gss_release_buffer (&dsmn,&resp);
      }
      while (dsmj == GSS_S_CONTINUE_NEEDED);
      do switch (dsmj = gss_display_status (&dsmn,smn,GSS_C_MECH_CODE,
					    GSS_C_NO_OID,&mctx,&resp)) {
      case GSS_S_COMPLETE:
      case GSS_S_CONTINUE_NEEDED:
	sprintf (tmp,"GSSAPI mechanism status: %s",(char *) resp.value);
	mm_log (tmp,WARN);
	gss_release_buffer (&dsmn,&resp);
      }
      while (dsmj == GSS_S_CONTINUE_NEEDED);
      (*responder) (stream,NIL,0);
      break;
    }
				/* finished with credentials name */
    if (crname) gss_release_name (&smn,&crname);
  }
  return ret;			/* return status */
}

/* Server authenticator
 * Accepts: responder function
 *	    argument count
 *	    argument vector
 * Returns: authenticated user name or NIL
 */

char *auth_gssapi_server (authresponse_t responder,int argc,char *argv[])
{
  char *ret = NIL;
  char tmp[MAILTMPLEN];
  unsigned long maxsize = htonl (AUTH_GSSAPI_C_MAXSIZE);
  int conf;
  OM_uint32 smj,smn,dsmj,dsmn,flags;
  OM_uint32 mctx = 0;
  gss_name_t crname,name;
  gss_OID mech;
  gss_buffer_desc chal,resp,buf;
  gss_cred_id_t crd;
  gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
  gss_qop_t qop = GSS_C_QOP_DEFAULT;
				/* make service name */
  sprintf (tmp,"%s@%s",(char *) mail_parameters (NIL,GET_SERVICENAME,NIL),
	   tcp_serverhost ());
  buf.length = strlen (buf.value = tmp);
				/* acquire credentials */
  if ((gss_import_name (&smn,&buf,GSS_C_NT_HOSTBASED_SERVICE,&crname)) ==
      GSS_S_COMPLETE) {
    if ((smj = gss_acquire_cred (&smn,crname,0,NIL,GSS_C_ACCEPT,&crd,NIL,NIL))
	== GSS_S_COMPLETE) {
      if (resp.value = (*responder) ("",0,(unsigned long *) &resp.length)) {
	do {			/* negotiate authentication */
	  smj = gss_accept_sec_context (&smn,&ctx,crd,&resp,
					GSS_C_NO_CHANNEL_BINDINGS,&name,&mech,
					&chal,&flags,NIL,NIL);
				/* don't need response any more */
	  fs_give ((void **) &resp.value);
	  switch (smj) {	/* how did it go? */
	  case GSS_S_COMPLETE:	/* successful */
	  case GSS_S_CONTINUE_NEEDED:
	    if (chal.value) {	/* send challenge, get next response */
	      resp.value = (*responder) (chal.value,chal.length,
					 (unsigned long *) &resp.length);
	      gss_release_buffer (&smn,&chal);
	    }
	    break;
	  }
	}
	while (resp.value && resp.length && (smj == GSS_S_CONTINUE_NEEDED));

				/* successful exchange? */
	if ((smj == GSS_S_COMPLETE) &&
	    (gss_display_name (&smn,name,&buf,&mech) == GSS_S_COMPLETE)) {
				/* send security and size */
	  memcpy (resp.value = tmp,(void *) &maxsize,resp.length = 4);
	  tmp[0] = AUTH_GSSAPI_P_NONE;
	  if (gss_wrap (&smn,ctx,NIL,qop,&resp,&conf,&chal) == GSS_S_COMPLETE){
	    resp.value = (*responder) (chal.value,chal.length,
				       (unsigned long *) &resp.length);
	    gss_release_buffer (&smn,&chal);
	    if (gss_unwrap (&smn,ctx,&resp,&chal,&conf,&qop)==GSS_S_COMPLETE) {
				/* client request valid */
	      if (chal.value && (chal.length > 4) &&
		  (chal.length < (MAILTMPLEN - 1)) &&
		  memcpy (tmp,chal.value,chal.length) &&
		  (tmp[0] & AUTH_GSSAPI_P_NONE)) {
				/* tie off authorization ID */
		tmp[chal.length] = '\0';
		ret = kerberos_login (tmp+4,buf.value,argc,argv);
	      }
				/* done with user name */
	      gss_release_buffer (&smn,&chal);
	    }
				/* finished with response */
	    fs_give ((void **) &resp.value);
	  }
				/* don't need name buffer any more */
	  gss_release_buffer (&smn,&buf);
	}
				/* don't need client name any more */
	gss_release_name (&smn,&name);
				/* don't need context any more */
	if (ctx != GSS_C_NO_CONTEXT) gss_delete_sec_context (&smn,&ctx,NIL);
      }
				/* finished with credentials */
      gss_release_cred (&smn,&crd);
    }

    else {			/* can't acquire credentials! */
      if (gss_display_name (&dsmn,crname,&buf,&mech) == GSS_S_COMPLETE)
	SERVER_LOG ("Failed to acquire credentials for %s",buf.value);
      if (smj != GSS_S_FAILURE) do
	switch (dsmj = gss_display_status (&dsmn,smj,GSS_C_GSS_CODE,
					   GSS_C_NO_OID,&mctx,&resp)) {
	case GSS_S_COMPLETE:
	  mctx = 0;
	case GSS_S_CONTINUE_NEEDED:
	  SERVER_LOG ("Unknown GSSAPI failure: %s",resp.value);
	  gss_release_buffer (&dsmn,&resp);
	}
      while (dsmj == GSS_S_CONTINUE_NEEDED);
      do switch (dsmj = gss_display_status (&dsmn,smn,GSS_C_MECH_CODE,
					    GSS_C_NO_OID,&mctx,&resp)) {
      case GSS_S_COMPLETE:
      case GSS_S_CONTINUE_NEEDED:
	SERVER_LOG ("GSSAPI mechanism status: %s",resp.value);
	gss_release_buffer (&dsmn,&resp);
      }
      while (dsmj == GSS_S_CONTINUE_NEEDED);
    }
				/* finished with credentials name */
    gss_release_name (&smn,&crname);
  }
  return ret;			/* return status */
}
