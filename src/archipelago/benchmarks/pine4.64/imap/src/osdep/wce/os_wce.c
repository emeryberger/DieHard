/*
 * Program:	Operating-system dependent routines -- WCE version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	11 April 1989
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include "tcp_wce.h"		/* must be before osdep includes tcp.h */
#include "mail.h"
#include "osdep.h"
#include <winsock.h>
#include <stdio.h>
#include <time.h>
#include <sys\timeb.h>
#include <fcntl.h>
#include <sys\stat.h>
#include "misc.h"

#include "fs_wce.c"
#include "ftl_wce.c"
#include "nl_wce.c"
#include "env_wce.c"
#include "tcp_wce.c"
#include "auths.c"
