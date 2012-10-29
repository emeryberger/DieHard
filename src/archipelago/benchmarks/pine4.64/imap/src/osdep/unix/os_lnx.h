/*
 * Program:	Operating-system dependent routines -- Linux version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	10 September 1993
 * Last Edited:	20 April 2005
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2005 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/*
 *** These lines are claimed to be necessary to build on Debian Linux on an
 *** Alpha.
 */

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 1
#endif /* _XOPEN_SOURCE */
#ifndef _BSD_SOURCE
#define _BSD_SOURCE 1
#endif /* _BSD_SOURCE */

/* end Debian Linux on Alpha strangeness */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>		/* for struct tm */
#include <fcntl.h>
#include <syslog.h>
#include <sys/file.h>


/* Linux gets this wrong */

#define setpgrp setpgid

#define direct dirent

#define flock safe_flock


#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
