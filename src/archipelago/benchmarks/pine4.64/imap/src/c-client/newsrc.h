/*
 * Program:	Newsrc manipulation routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	12 September 1994
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Function prototypes */

long newsrc_error (char *fmt,char *text,long errflg);
long newsrc_write_error (char *name,FILE *f1,FILE *f2);
FILE *newsrc_create (MAILSTREAM *stream,int notify);
long newsrc_newstate (FILE *f,char *group,char state,char *nl);
long newsrc_newmessages (FILE *f,MAILSTREAM *stream,char *nl);
void newsrc_lsub (MAILSTREAM *stream,char *pattern);
long newsrc_update (MAILSTREAM *stream,char *group,char state);
long newsrc_read (char *group,MAILSTREAM *stream);
long newsrc_write (char *group,MAILSTREAM *stream);
char *newsrc_state (MAILSTREAM *stream,char *group);
void newsrc_check_uid (unsigned char *state,unsigned long uid,
		       unsigned long *recent,unsigned long *unseen);
