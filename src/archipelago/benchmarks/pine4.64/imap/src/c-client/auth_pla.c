/*
 * Program:	Plain authenticator
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	22 September 1998
 * Last Edited:	15 March 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

long auth_plain_valid (void);
long auth_plain_client (authchallenge_t challenger,authrespond_t responder,
			char *service,NETMBX *mb,void *stream,
			unsigned long *trial,char *user);
char *auth_plain_server (authresponse_t responder,int argc,char *argv[]);

AUTHENTICATOR auth_pla = {
  AU_AUTHUSER,			/* allow authuser */
  "PLAIN",			/* authenticator name */
  auth_plain_valid,		/* check if valid */
  auth_plain_client,		/* client method */
  NIL,				/* server method */
  NIL				/* next authenticator */
};

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

long auth_plain_client (authchallenge_t challenger,authrespond_t responder,
			char *service,NETMBX *mb,void *stream,
			unsigned long *trial,char *user)
{
  char *u,pwd[MAILTMPLEN];
  void *challenge;
  unsigned long clen;
  long ret = NIL;
				/* snarl if not SSL/TLS session */
  if (!mb->sslflag && !mb->tlsflag)
    mm_log ("SECURITY PROBLEM: insecure server advertised AUTH=PLAIN",WARN);
				/* get initial (empty) challenge */
  if (challenge = (*challenger) (stream,&clen)) {
    fs_give ((void **) &challenge);
    if (clen) {			/* abort if challenge non-empty */
      mm_log ("Server bug: non-empty initial PLAIN challenge",WARN);
      (*responder) (stream,NIL,0);
      ret = LONGT;		/* will get a BAD response back */
    }
    pwd[0] = NIL;		/* prompt user if empty challenge */
    mm_login (mb,user,pwd,*trial);
    if (!pwd[0]) {		/* empty challenge or user requested abort */
      (*responder) (stream,NIL,0);
      *trial = 0;		/* cancel subsequent attempts */
      ret = LONGT;		/* will get a BAD response back */
    }
    else {
      unsigned long rlen = 
	strlen (mb->authuser) + strlen (user) + strlen (pwd) + 2;
      char *response = (char *) fs_get (rlen);
      char *t = response;	/* copy authorization id */
      if (mb->authuser[0]) for (u = user; *u; *t++ = *u++);
      *t++ = '\0';		/* delimiting NUL */
				/* copy authentication id */
      for (u = mb->authuser[0] ? mb->authuser : user; *u; *t++ = *u++);
      *t++ = '\0';		/* delimiting NUL */
				/* copy password */
      for (u = pwd; *u; *t++ = *u++);
				/* send credentials */
      if ((*responder) (stream,response,rlen)) {
	if (challenge = (*challenger) (stream,&clen))
	  fs_give ((void **) &challenge);
	else {
	  ++*trial;		/* can try again if necessary */
	  ret = LONGT;		/* check the authentication */
	}
      }
      memset (response,0,rlen);	/* erase credentials */
      fs_give ((void **) &response);
    }
  }
  memset (pwd,0,MAILTMPLEN);	/* erase password */
  if (!ret) *trial = 65535;	/* don't retry if bad protocol */
  return ret;
}

/* Check if PLAIN valid on this system
 * Returns: T, always
 */

long auth_plain_valid (void)
{
  return T;			/* PLAIN is valid */
}


/* Server authenticator
 * Accepts: responder function
 *	    argument count
 *	    argument vector
 * Returns: authenticated user name or NIL
 */

char *auth_plain_server (authresponse_t responder,int argc,char *argv[])
{
  char *ret = NIL;
  char *user,*aid,*pass;
  unsigned long len;
				/* get user name */
  if (aid = (*responder) ("",0,&len)) {
				/* note: responders null-terminate */
    if ((((unsigned long) ((user = aid + strlen (aid) + 1) - aid)) < len) &&
	(((unsigned long) ((pass = user + strlen (user) + 1) - aid)) < len) &&
	(((unsigned long) ((pass + strlen (pass)) - aid)) == len) &&
	(*aid ? server_login (aid,pass,user,argc,argv) :
	 server_login (user,pass,NIL,argc,argv))) ret = myusername ();
    fs_give ((void **) &aid);
  }
  return ret;
}
