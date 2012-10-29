/*
 * Program:	Old SVR4 check password
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

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
  char tmp[MAILTMPLEN];
  struct spwd *sp = NIL;
  time_t left;
  time_t now = time (0);
  struct tm *t = gmtime (&now);
  int zone = t->tm_hour * 60 + t->tm_min;
  int julian = t->tm_yday;
  t = localtime (&now);		/* get local time now */
				/* minus UTC minutes since midnight */
  zone = t->tm_hour * 60 + t->tm_min - zone;
  /* julian can be one of:
   *  36x  local time is December 31, UTC is January 1, offset -24 hours
   *    1  local time is 1 day ahead of UTC, offset +24 hours
   *    0  local time is same day as UTC, no offset
   *   -1  local time is 1 day behind UTC, offset -24 hours
   * -36x  local time is January 1, UTC is December 31, offset +24 hours
   */
  if (julian = t->tm_yday -julian)
    zone += ((julian < 0) == (abs (julian) == 1)) ? -24*60 : 24*60;
				/* days since 1/1/1970 local time */
  now = ((now /60) + zone) / (60*24);
				/* non-shadow authentication */
  if (!pw->pw_passwd || !pw->pw_passwd[0] || !pw->pw_passwd[1] ||
      strcmp (pw->pw_passwd,(char *) crypt (pass,pw->pw_passwd))) {
    /* As far as I've been able to determine, here is how the expiration
     * fields in the shadow authentication data work:
     *  lstchg	last password change date if non-negative.  If zero, the
     *		user can not log in without changing password.
     *  max	number of days a password is valid if positive
     * The expiration day is the *last* day that the password is valid.
     */
				/* shadow authentication */
    if ((sp = getspnam (pw->pw_name)) && sp->sp_lstchg &&
	((sp->sp_lstchg < 0) || (sp->sp_max <= 0) ||
	 ((sp->sp_lstchg + sp->sp_max) >= now)) &&
	sp->sp_pwdp && sp->sp_pwdp[0] && sp->sp_pwdp[1] &&
	!strcmp (sp->sp_pwdp,(char *) crypt (pass,sp->sp_pwdp))) {
      if ((sp->sp_lstchg > 0) && (sp->sp_max > 0) &&
	  ((left = (sp->sp_lstchg + sp->sp_max) - now) <= 28)) {
	if (left) {
	  sprintf (tmp,"[ALERT] Password expires in %ld day(s)",(long) left);
	  mm_notify (NIL,tmp,NIL);
	}
	else mm_notify (NIL,"[ALERT] Password expires today!",WARN);
      }
      endspent ();		/* don't need shadow password data any more */
    }
    else pw = NIL;		/* password failed */
  }
  return pw;
}
