/*
 * Program:	Dummy routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	9 May 1991
 * Last Edited:	14 October 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Exported function prototypes */

void dummy_scan (MAILSTREAM *stream,char *ref,char *pat,char *contents);
void dummy_list (MAILSTREAM *stream,char *ref,char *pat);
void dummy_lsub (MAILSTREAM *stream,char *ref,char *pat);
long dummy_create (MAILSTREAM *stream,char *mailbox);
long dummy_create_path (MAILSTREAM *stream,char *path,long dirmode);
long dummy_delete (MAILSTREAM *stream,char *mailbox);
long dummy_rename (MAILSTREAM *stream,char *old,char *newname);
char *dummy_file (char *dst,char *name);
long dummy_canonicalize (char *tmp,char *ref,char *pat);
