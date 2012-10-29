/*
 * Program:	Standard login
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
  uid_t uid = pw->pw_uid;
  char *name = cpystr (pw->pw_name);
  long ret = !(setgid (pw->pw_gid) || initgroups (name,pw->pw_gid) ||
	       setuid (uid));
  fs_give ((void **) &name);
  return ret;
}
