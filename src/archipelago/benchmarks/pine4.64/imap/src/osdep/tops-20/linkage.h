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

extern DRIVER imapdriver;
extern DRIVER nntpdriver;
extern DRIVER pop3driver;
extern DRIVER dummydriver;
extern AUTHENTICATOR auth_log;
extern AUTHENTICATOR auth_md5;
extern AUTHENTICATOR auth_pla;
