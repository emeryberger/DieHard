/*
 * Program:	Pseudo Header Strings
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	26 September 1996
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Local sites may wish to alter this text */

char *pseudo_from = "MAILER-DAEMON";
char *pseudo_name = "Mail System Internal Data";
char *pseudo_subject = "DON'T DELETE THIS MESSAGE -- FOLDER INTERNAL DATA";
char *pseudo_msg =
  "This text is part of the internal format of your mail folder, and is not\na real message.  It is created automatically by the mail system software.\nIf deleted, important folder data will be lost, and it will be re-created\nwith the data reset to initial values."
  ;
