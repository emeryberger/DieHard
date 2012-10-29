/*
 * Program:	Cygwin check password
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

/* This module is against my better judgement.  If you want to run an imapd
 * or ipop[23]d you should use the native NT or W2K ports intead of this
 * Cygwin port.  There is no surety that this module works right or will work
 * right in the future.
 */

#undef ERROR
#include <windows.h>
#include <sys/cygwin.h>


/* Check password
 * Accepts: login passwd struct
 *	    password string
 *	    argument count
 *	    argument vector
 * Returns: passwd struct if password validated, NIL otherwise
 */

static char *cyg_user = NIL;
static HANDLE cyg_hdl = NIL;

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
				/* flush last pw-checked user */
  if (cyg_user) fs_give ((void **) &cyg_user);
				/* forbid if UID 0 or SYSTEM uid */
  if (!pw->pw_uid || (pw->pw_uid == SYSTEMUID) ||
      ((cyg_hdl = cygwin_logon_user (pw,pass)) == INVALID_HANDLE_VALUE))
    return NIL;			/* bad UID or password */
				/* remember user for this handle */
  cyg_user = cpystr (pw->pw_name);
  return pw;
}

