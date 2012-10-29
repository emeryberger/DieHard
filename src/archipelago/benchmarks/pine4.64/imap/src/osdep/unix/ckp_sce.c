/*
 * Program:	SecureWare check password
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
 * Last Edited:	29 April 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
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

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
  struct es_passwd *pp;
  set_auth_parameters (argc,argv);
  if ((pw->pw_passwd && pw->pw_passwd[0] && pw->pw_passwd[1] &&
       !strcmp (pw->pw_passwd,(char *) crypt (pass,pw->pw_passwd))) ||
      ((pp = getespwnam (pw->pw_name)) && !pp->ufld->fd_lock &&
       !pp->ufld->fd_psw_chg_reqd &&
       pp->ufld->fd_encrypt && pp->ufld->fd_encrypt[0] &&
       pp->ufld->fd_encrypt[1] &&
       !strcmp (pp->ufld->fd_encrypt,bigcrypt (pass,pp->ufld->fd_encrypt))))
    endprpwent ();		/* don't need shadow password data any more */
  else pw = NIL;		/* password failed */
  return pw;
}
