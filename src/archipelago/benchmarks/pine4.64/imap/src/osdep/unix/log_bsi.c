/*
 * Program:	BSI login
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	1 August 1988
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Log in
 * Accepts: login passwd struct
 *	    argument count
 *	    argument vector
 * Returns: T if success, NIL otherwise
 */

long loginpw (struct passwd *pw,int argc,char *argv[])
{
  long ret = NIL;
  uid_t euid = geteuid ();
  login_cap_t *lc = login_getclass (pw->pw_class);
				/* have class and can become user? */
  if (lc && !seteuid (pw->pw_uid)) {
				/* ask for approval */
    if (auth_approve (lc,pw->pw_name,
		      (char *) mail_parameters (NIL,GET_SERVICENAME,NIL)) <= 0)
      seteuid (euid);		/* not approved, restore root euid */
    else {			/* approved */
      seteuid (euid);		/* restore former root euid first */
      setsid ();		/* ensure we are session leader */
				/* log the guy in */
      ret = !setusercontext (lc,pw,pw->pw_uid,LOGIN_SETALL&~LOGIN_SETPATH);
    }
  }
  return ret;
}
