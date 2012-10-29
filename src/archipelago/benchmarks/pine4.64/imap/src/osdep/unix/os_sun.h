/*
 * Program:	Operating-system dependent routines -- SUN-OS version
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
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <sys/types.h>
#include <sys/dir.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/file.h>


#define strstr Strstr		/* override system definition */

#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"

char *Strstr (char *cs,char *ct);
char *strerror (int n);
unsigned long strtoul (char *s,char **endp,int base);
#define memcpy memmove
void *memmove (void *s,void *ct,size_t n);
void *memset (void *s,int c,size_t n);
