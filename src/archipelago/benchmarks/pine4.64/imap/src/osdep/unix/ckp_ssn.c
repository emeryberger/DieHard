/*
 * Program:	Secure SUN-OS check password
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

#include <sys/label.h>
#include <sys/audit.h>
#include <pwdadj.h>


/* Check password
 * Accepts: login passwd struct
 *	    password string
 *	    argument count
 *	    argument vector
 * Returns: passwd struct if password validated, NIL otherwise
 */

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
  struct passwd_adjunct *pa;
  char *user = cpystr (pw->pw_name);
				/* validate user and password */
  struct passwd *ret =
    ((pw->pw_passwd && pw->pw_passwd[0] && pw->pw_passwd[1] &&
      !strcmp (pw->pw_passwd,(char *) crypt (pass,pw->pw_passwd))) ||
     ((pa = getpwanam (pw->pw_name)) &&
      pa->pwa_passwd && pa->pwa_passwd[0] && pa->pwa_passwd[1] &&
      !strcmp (pa->pwa_passwd,(char *) crypt (pass,pa->pwa_passwd)))) ?
	getpwnam (user) : NIL;
  if (user) fs_give ((void **) &user);
  return ret;
}
