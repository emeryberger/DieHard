/*
 * Program:	Kerberos 5 check password
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
 * Last Edited:	5 June 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
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
  char tmp[MAILTMPLEN];
  krb5_context ctx;
  krb5_timestamp now;
  krb5_principal service;
  krb5_creds *crd = (krb5_creds *) memset (fs_get (sizeof (krb5_creds)),0,
						   sizeof (krb5_creds));
  struct passwd *ret = NIL;
  if (*pass) {			/* only if password non-empty */
				/* make service name */
    sprintf (tmp,"%s@%s",(char *) mail_parameters (NIL,GET_SERVICENAME,NIL),
	     tcp_serverhost ());
    krb5_init_context (&ctx);	/* get a context */
				/* get time, client and server principals */
    if (!krb5_timeofday (ctx,&now) &&
	!krb5_parse_name (ctx,pw->pw_name,&crd->client) &&
	!krb5_parse_name (ctx,tmp,&service) &&
	!krb5_build_principal_ext (ctx,&crd->server,
				   krb5_princ_realm (ctx,crd->client)->length,
				   krb5_princ_realm (ctx,crd->client)->data,
				   KRB5_TGS_NAME_SIZE,KRB5_TGS_NAME,
				   krb5_princ_realm (ctx,crd->client)->length,
				   krb5_princ_realm (ctx,crd->client)->data,
				   0)) {
				/* expire in 3 minutes */
      crd->times.endtime = now + (3 * 60);
      if (!krb5_get_in_tkt_with_password (ctx,NIL,NIL,NIL,NIL,pass,0,crd,0) &&
	  !krb5_verify_init_creds (ctx,crd,service,0,0,0))
	ret = pw;
      krb5_free_creds (ctx,crd);/* flush creds and service principal */
      krb5_free_principal (ctx,service);
    }
    krb5_free_context (ctx);	/* don't need context any more */
  }
  return ret;
}
