/*
 * Program:	DCE check password
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

/* Check password
 * Accepts: login passwd struct
 *	    password string
 *	    argument count
 *	    argument vector
 * Returns: passwd struct if password validated, NIL otherwise
 */

#include <dce/rpc.h>
#include <dce/sec_login.h>

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
  sec_passwd_rec_t pwr;
  sec_login_handle_t lhdl;
  boolean32 rstpwd;
  sec_login_auth_src_t asrc;
  error_status_t status;
  FILE *fd;
				/* easy case */
  if (pw->pw_passwd && pw->pw_passwd[0] && pw->pw_passwd[1] &&
      !strcmp (pw->pw_passwd,(char *) crypt (pass,pw->pw_passwd))) return pw;
				/* try DCE password cache file */
  if (fd = fopen (PASSWD_OVERRIDE,"r")) {
    char *usr = cpystr (pw->pw_name);
    while ((pw = fgetpwent (fd)) && strcmp (usr,pw->pw_name));
    fclose (fd);		/* finished with cache file */
				/* validate cached password */
    if (pw && pw->pw_passwd && pw->pw_passwd[0] && pw->pw_passwd[1] &&
	!strcmp (pw->pw_passwd,(char *) crypt (pass,pw->pw_passwd))) {
      fs_give ((void **) &usr);
      return pw;
    }
    if (!pw) pw = getpwnam (usr);
    fs_give ((void **) &usr);
  }
  if (pw) {			/* try S-L-O-W DCE... */
    sec_login_setup_identity ((unsigned_char_p_t) pw->pw_name,
			      sec_login_no_flags,&lhdl,&status);
    if (status == error_status_ok) {
      pwr.key.tagged_union.plain = (idl_char *) pass;
      pwr.key.key_type = sec_passwd_plain;
      pwr.pepper = NIL;
      pwr.version_number = sec_passwd_c_version_none;
				/* validate password with login context */
      sec_login_validate_identity (lhdl,&pwr,&rstpwd,&asrc,&status);
      if (!rstpwd && (asrc == sec_login_auth_src_network) &&
	  (status == error_status_ok)) {
	sec_login_purge_context (&lhdl,&status);
	if (status == error_status_ok) return pw;
      }
    }
  }
  return NIL;			/* password validation failed */
}
