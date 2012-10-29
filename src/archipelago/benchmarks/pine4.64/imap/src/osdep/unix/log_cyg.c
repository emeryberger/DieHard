/*
 * Program:	Cygwin login
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
 * Last Edited:	25 April 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2003 University of Washington.
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
  uid_t uid = pw->pw_uid;
				/* must be same user name as last checkpw() */
  if (!(cyg_user && !strcmp (pw->pw_name,cyg_user))) return NIL;
				/* do the ImpersonateLoggedOnUser() */
  cygwin_set_impersonation_token (cyg_hdl);

  return !(setgid (pw->pw_gid) || initgroups (cyg_user,pw->pw_gid) ||
	   setuid (uid));
}
