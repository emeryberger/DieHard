/*
 * Program:	Operating-system dependent routines -- Cygwin version
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
 * Last Edited:	19 April 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/file.h>
#include <time.h>
#include <sys/time.h>

#define direct dirent


#define CYGKLUDGEOFFSET 1	/* don't write 1st byte of shared-lock files */

/* Cygwin gets this wrong */

#define setpgrp setpgid

#define SYSTEMUID 18		/* Cygwin returns this for SYSTEM */
#define geteuid Geteuid
uid_t Geteuid (void);

/* Now Cygwin has reportedly joined this madness.  Use ifndef in case it shares
   the SVR4 <sys/file.h> silliness too */
#ifndef L_SET
#define L_SET SEEK_SET
#endif
#ifndef L_INCR
#define L_INCR SEEK_CUR
#endif
#ifndef L_XTND
#define L_XTND SEEK_END
#endif


#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "flockcyg.h"
