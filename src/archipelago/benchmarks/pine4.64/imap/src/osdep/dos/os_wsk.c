/*
 * Program:	Operating-system dependent routines -- Winsock version
 *
 * Author:	Mike Seibel from Unix version by Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MikeS@CAC.Washington.EDU
 *
 * Date:	11 April 1989
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include "tcp_wsk.h"		/* must be before osdep includes tcp.h */
#undef	ERROR			/* quell conflicting def warning */
#include "mail.h"
#include "osdep.h"
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <sys\timeb.h>
#include "misc.h"


#include "fs_dos.c"
#include "ftl_dos.c"
#include "nl_dos.c"
#include "env_dos.c"
#include "tcp_wsk.c"
