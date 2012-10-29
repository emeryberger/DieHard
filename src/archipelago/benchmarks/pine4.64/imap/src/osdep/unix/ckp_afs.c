/*
 * Program:	AFS check password
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
 * Last Edited:	22 June 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* AFS cleanup
 * Accepts: data
 */

void checkpw_cleanup (void *data)
{
  ktc_ForgetAllTokens ();
}


/* Check password
 * Accepts: login passwd struct
 *	    password string
 *	    argument count
 *	    argument vector
 * Returns: passwd struct if password validated, NIL otherwise
 */

#undef INIT
#define min AFS_MIN
#define max AFS_MAX
#include <afs/param.h>
#include <afs/kautils.h>

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
  char *reason;
				/* faster validation for POP servers */
  if (!strcmp ((char *) mail_parameters (NIL,GET_SERVICENAME,NIL),"pop")) {
    struct ktc_encryptionKey key;
    struct ktc_token token;
				/* just check the password */
    ka_StringToKey (pass,NIL,&key);
    if (ka_GetAdminToken (pw->pw_name,"","",&key,600,&token,1)) return NIL;
  }
				/* check password and get AFS token */
  else if (ka_UserAuthenticateGeneral
	   (KA_USERAUTH_VERSION + KA_USERAUTH_DOSETPAG,pw->pw_name,NIL,NIL,
	    pass,0,0,0,&reason)) return NIL;
				/* arm hook to delete credentials */
  mail_parameters (NIL,SET_LOGOUTHOOK,(void *) checkpw_cleanup);
  return pw;
}
