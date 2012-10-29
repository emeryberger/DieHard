/*
 * Program:	File descriptor string routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	15 April 1997
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Driver-dependent data passed to init method */

typedef struct fd_data {
  int fd;			/* file descriptor */
  unsigned long pos;		/* initial position */
  char *chunk;			/* I/O buffer chunk */
  unsigned long chunksize;	/* I/O buffer chunk length */
} FDDATA;


extern STRINGDRIVER fd_string;
