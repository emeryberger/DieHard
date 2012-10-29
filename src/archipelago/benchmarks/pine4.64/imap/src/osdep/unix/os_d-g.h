/*
 * Program:	Operating-system dependent routines -- D-G version
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
 * Last Edited:	10 April 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/dir.h>
#include <time.h>		/* for struct tm */
#include <fcntl.h>
#define _USEC_UTIME_FLAVOR	/* break it for compatibility with */
#include <utime.h>		/*  the incompatible past */
#include <syslog.h>
#include <sys/file.h>


/* D-G gets this wrong */

#define setpgrp setpgrp2


#define utime portable_utime
int portable_utime (char *file,time_t timep[2]);

#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "flocksim.h"
