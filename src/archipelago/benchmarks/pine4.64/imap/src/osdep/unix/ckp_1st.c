/*
 * Program:	Dual check password part 1
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

struct passwd *checkpw_alt(struct passwd *pw,char *pass,int argc,char *argv[]);
struct passwd *checkpw_std(struct passwd *pw,char *pass,int argc,char *argv[]);

/* Check password
 * Accepts: login passwd struct
 *	    password string
 *	    argument count
 *	    argument vector
 * Returns: passwd struct if password validated, NIL otherwise
 */

struct passwd *checkpw (struct passwd *pw,char *pass,int argc,char *argv[])
{
  struct passwd *ret;
				/* in case first checker smashes it */
  char *user = cpystr (pw->pw_name);
  if (!(ret = checkpw_alt (pw,pass,argc,argv)))
    ret = checkpw_std (getpwnam (user),pass,argc,argv);
  fs_give ((void **) &user);
  return ret;
}

/* Redefine alt checker's routine name */

#define checkpw checkpw_alt
