/*
 * Program:	Operating-system dependent routines -- VMS version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	2 August 1994
 * Last Edited:	1 May 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unixio.h>
#include <file.h>
#include <stat.h>

#define L_SET SEEK_SET
#define L_INCR SEEK_CUR
#define L_XTND SEEK_END

#include "env_vms.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"

#define	gethostid clock
#define random rand
#define unlink delete

char *getpass (const char *prompt);
