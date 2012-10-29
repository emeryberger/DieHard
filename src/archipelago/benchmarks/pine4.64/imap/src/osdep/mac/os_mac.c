/*
 * Program:	Operating-system dependent routines -- Macintosh version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	26 January 1992
 * Last Edited:	7 February 2001
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/*  This is a totally new operating-system dependent module for the Macintosh,
 * written using THINK C on my Mac PowerBook-100 in my free time.
 * Unlike earlier efforts, this version requires no external TCP library.  It
 * also takes advantage of the Map panel in System 7 for the timezone.
 */

/* PPC cretins broke the MachineLocation struct */

#define gmtFlags u

#include <limits.h>
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#define tcp_port MacTCP_port
#include <MacTCPCommonTypes.h>
#include <AddressXlation.h>
#include <TCPPB.h>
#include <Desk.h>
#include <Devices.h>
#include <Errors.h>
#include <Files.h>
#include <Fonts.h>
#include <Menus.h>
#include <Script.h>
#include <ToolUtils.h>
#include <Windows.h>
#undef tcp_port

#include "tcp_mac.h"		/* must be before osdep.h */
#include "mail.h"
#include "osdep.h"
#include "misc.h"

static short TCPdriver = 0;	/* MacTCP's reference number */
short resolveropen = 0;		/* TCP's resolver open */


#include "env_mac.c"
#include "fs_mac.c"
#include "ftl_mac.c"
#include "nl_mac.c"
#include "tcp_mac.c"

#define open(a,b,c) open (a,b)
#define server_login(user,pass,authuser,argc,argv) NIL
#define authserver_login(user,authuser,argc,argv) NIL
#define myusername() ""		/* dummy definition to prevent build errors */
#define MD5ENABLE ""

#include "auth_md5.c"
#include "auth_pla.c"
#include "auth_log.c"
