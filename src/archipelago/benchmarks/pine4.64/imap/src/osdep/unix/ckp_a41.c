/*
 * Program:	AIX 4.1 check password
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
 
#include <login.h>

/* Check password
 * Accepts: login passwd struct
 *	    password string
 *	    argument count
 *	    argument vector
 * Returns: passwd struct if password validated, NIL otherwise
 */

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
  int reenter = 0;
  char *msg = NIL;
  char *user = cpystr (pw->pw_name);
				/* validate password */
  struct passwd *ret = (pw && !loginrestrictions (user,S_RLOGIN,NIL,&msg) &&
			!authenticate (user,pass,&reenter,&msg)) ?
			  getpwnam (user) : NIL;
				/* clean up any message returned */
  if (msg) fs_give ((void **) &msg);
  if (user) fs_give ((void **) &user);
  return ret;
}
