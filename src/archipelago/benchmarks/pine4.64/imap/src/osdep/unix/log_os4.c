/*
 * Program:	OSF/1 (Digital UNIX) 4 login
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
  int i;
  char *s;
  char *name = cpystr (pw->pw_name);
  char *host = cpystr (tcp_clienthost ());
  uid_t uid = pw->pw_uid;
  long ret = NIL;
				/* tie off address */
  if (s = strchr (host,' ')) *s = '\0';
  if (*host == '[') {		/* convert [a.b.c.d] to a.b.c.d */
    memmove (host,host+1,i = strlen (host + 2));
    host[i] = '\0';
  }
  if (sia_become_user (checkpw_collect,argc,argv,host,name,NIL,NIL,NIL,NIL,
		       SIA_BEU_REALLOGIN|SIA_BEU_OKROOTDIR) != SIASUCCESS)
    setreuid (0,0);		/* make sure have root again */
				/* probable success, complete login */
  else ret = (!setreuid (0,0) && !setuid (uid));
  fs_give ((void **) &name);
  fs_give ((void **) &host);
  return ret;
}
