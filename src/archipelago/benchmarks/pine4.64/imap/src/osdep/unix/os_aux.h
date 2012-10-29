/*
 * Program:	Operating-system dependent routines -- A/UX version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	11 May 1989
 * Last Edited:	10 April 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


#include <sys/types.h>
#include <sys/dir.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <syslog.h>


extern int errno;

char *strerror (int n);
unsigned long strtoul (char *s,char **endp,int base);
void *memmove (void *s,void *ct,size_t n);

#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "flocksim.h"
