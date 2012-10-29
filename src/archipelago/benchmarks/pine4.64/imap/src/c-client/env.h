/*
 * Program:	Environment routines
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
 * Last Edited:	27 April 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Function prototypes */

long pmatch_full (unsigned char *s,unsigned char *pat,unsigned char delim);
long dmatch (unsigned char *s,unsigned char *pat,unsigned char delim);
void *env_parameters (long function,void *value);
void rfc822_date (char *date);
void rfc822_timezone (char *s,void *t);
void internal_date (char *date);
long server_input_wait (long seconds);
void server_init (char *server,char *service,char *sasl,
		  void *clkint,void *kodint,void *hupint,void *trmint);
long server_login (char *user,char *pass,char *authuser,int argc,char *argv[]);
long authserver_login (char *user,char *authuser,int argc,char *argv[]);
long anonymous_login (int argc,char *argv[]);
char *mylocalhost (void);
char *myhomedir (void);
char *mailboxfile (char *dst,char *name);
MAILSTREAM *default_proto (long type);
