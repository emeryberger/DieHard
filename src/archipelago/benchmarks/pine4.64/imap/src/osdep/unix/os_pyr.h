/*
 * Program:	Operating-system dependent routines -- Pyramid OSx 4.4c version
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

#include <strings.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/file.h>


char *strtok (char *s,char *ct);
char *strstr (char *cs,char *ct);
char *strpbrk (char *cs,char *ct);
char *strerror (int n);
void *memmove (void *s,void *ct,size_t n);
void *memset (void *s,int c,size_t n);
void *malloc (size_t byteSize);
void free (void *ptr);
void *realloc (void *oldptr,size_t newsize);

int errno;

#define memcpy memmove
#define strchr index
#define strrchr rindex

#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
