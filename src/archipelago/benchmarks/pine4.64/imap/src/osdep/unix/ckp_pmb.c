/*
 * Program:	Pluggable Authentication Modules login services, buggy systems
 *		(use this instead of ckp_pam.c on Solaris)
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
 * Last Edited:	21 June 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


#include <security/pam_appl.h>

static char *pam_uname;		/* user name */
static char *pam_pass;		/* password */

/* PAM conversation function
 * Accepts: number of messages
 *	    vector of messages
 *	    pointer to response return
 *	    application data
 * Returns: PAM_SUCCESS if OK, response vector filled in, else PAM_CONV_ERR
 */

static int checkpw_conv (int num_msg,const struct pam_message **msg,
			 struct pam_response **resp,void *appdata_ptr)
{
  int i;
  struct pam_response *reply = fs_get (sizeof (struct pam_response) * num_msg);
  for (i = 0; i < num_msg; i++) switch (msg[i]->msg_style) {
  case PAM_PROMPT_ECHO_ON:	/* assume want user name */
    reply[i].resp_retcode = PAM_SUCCESS;
    reply[i].resp = cpystr (pam_uname);
    break;
  case PAM_PROMPT_ECHO_OFF:	/* assume want password */
    reply[i].resp_retcode = PAM_SUCCESS;
    reply[i].resp = cpystr (pam_pass);
    break;
  case PAM_TEXT_INFO:
  case PAM_ERROR_MSG:
    reply[i].resp_retcode = PAM_SUCCESS;
    reply[i].resp = NULL;
    break;
  default:			/* unknown message style */
    fs_give ((void **) &reply);
    return PAM_CONV_ERR;
  }
  *resp = reply;
  return PAM_SUCCESS;
}


/* PAM cleanup
 * Accepts: handle
 */

static void checkpw_cleanup (pam_handle_t *hdl)
{
#if 0	/* see checkpw() for why this is #if 0 */
  pam_close_session (hdl,NIL);	/* close session [uw]tmp */
#endif
  pam_setcred (hdl,PAM_DELETE_CRED);
  pam_end (hdl,PAM_SUCCESS);
}

/* Server log in
 * Accepts: user name string
 *	    password string
 * Returns: T if password validated, NIL otherwise
 */

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
  pam_handle_t *hdl;
  struct pam_conv conv;
  conv.conv = &checkpw_conv;
  pam_uname = pw->pw_name;
  pam_pass = pass;
  if ((pam_start ((char *) mail_parameters (NIL,GET_SERVICENAME,NIL),
		  pw->pw_name,&conv,&hdl) != PAM_SUCCESS) ||
      (pam_set_item (hdl,PAM_RHOST,tcp_clientaddr ()) != PAM_SUCCESS) ||
      (pam_authenticate (hdl,NIL) != PAM_SUCCESS) ||
      (pam_acct_mgmt (hdl,NIL) != PAM_SUCCESS) ||
      (pam_setcred (hdl,PAM_ESTABLISH_CRED) != PAM_SUCCESS)) {
				/* clean up */
    pam_setcred (hdl,PAM_DELETE_CRED);
    pam_end (hdl,PAM_AUTH_ERR);	/* failed */
    return NIL;
  }
#if 0
  /*
   * Some people have reported that this causes a SEGV in strncpy() from
   * pam_unix.so.1
   */
  /*
   * This pam_open_session() call is inconsistant with how we handle other
   * platforms, where we don't write [uw]tmp records.  However, unlike our
   * code on other platforms, pam_acct_mgmt() will check those records for
   * inactivity and deny the authentication.
   */
  pam_open_session (hdl,NIL);	/* make sure account doesn't go inactive */
#endif
				/* arm hook to delete credentials */
  mail_parameters (NIL,SET_LOGOUTHOOK,(void *) checkpw_cleanup);
  mail_parameters (NIL,SET_LOGOUTDATA,(void *) hdl);
  return pw;
}
