/*
 * Program:	Operating-system dependent routines -- Amiga version
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

#include <proto/socket.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/file.h>

/* Different names, equivalent things in BSD and SysV */

#define direct dirent
#define utime portable_utime
int portable_utime (char *file,time_t timep[2]);

#include "env_ami.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"

long gethostid (void);
typedef int (*select_t) (struct direct *name);
typedef int (*compar_t) (void *d1,void *d2);
int scandir (char *dirname,struct direct ***namelist,select_t select,
	     compar_t compar);
