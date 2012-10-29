/*
 * Program:	Operating-system dependent routines -- SGI version
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
 * Last Edited:	14 June 2001
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
#include <time.h>		/* for struct tm */
#include <utime.h>
#include <fcntl.h>
#include <sys/syslog.h>
#include <sys/file.h>
#include <ustat.h>

/* Many versions of SysV get this wrong */

#define setpgrp(a,b) Setpgrp(a,b)


#define direct dirent

#define fatal cclient_fatal

#define utime portable_utime
int portable_utime (char *file,time_t timep[2]);

#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "flocksim.h"
