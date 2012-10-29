/*
 * Program:	Network message (SMTP/NNTP/POP2/POP3) routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	8 June 1995
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


				/* stream must be void* for use as readfn_t */
long netmsg_read (void *stream,unsigned long count,char *buffer);
FILE *netmsg_slurp (NETSTREAM *stream,unsigned long *size,unsigned long *hsiz);
