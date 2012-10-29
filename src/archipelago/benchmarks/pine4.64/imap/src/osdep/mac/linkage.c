/*
 * Program:	Default driver linkage
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	13 June 1995
 * Last Edited:	7 February 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

  mail_link (&imapdriver);		/* link in the imap driver */
  mail_link (&nntpdriver);		/* link in the nntp driver */
  mail_link (&pop3driver);		/* link in the pop3 driver */
  mail_link (&dummydriver);		/* link in the dummy driver */
  auth_link (&auth_md5);		/* link in the md5 authenticator */
  auth_link (&auth_pla);		/* link in the plain authenticator */
  auth_link (&auth_log);		/* link in the log authenticator */
