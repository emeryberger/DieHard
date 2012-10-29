/*
 * Program:	Operating-system dependent routines -- NT version
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
 * Last Edited:	23 December 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys\types.h>
#include <time.h>
#include <io.h>
#include <conio.h>
#include <process.h>
#undef ERROR			/* quell conflicting defintion warning */
#include <windows.h>
#include <lm.h>
#undef ERROR
#define ERROR (long) 2		/* must match mail.h */


#include "env_nt.h"
#include "fs.h"
#include "ftl.h"
#include "nl.h"
#include "tcp.h"
#include "yunchan.h"

#undef noErr
#undef MAC
