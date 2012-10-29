/*
 * Program:	Operating-system dependent routines -- Apollo Domain/OS sr10.4
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

#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/dir.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/file.h>
#ifndef htons
#include <netinet/in.h>		/* needed for htons() prototypes */
#endif

extern int daylight;		/* local timzone uses daylight savings time */
extern long altzone;		/* seconds west of UTC during daylight time */

#include "env_unix.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
