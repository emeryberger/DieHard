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
 * Last Edited:	7 April 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */
 
#include "tcp_vms.h"		/* must be before osdep includes tcp.h */
#include "mail.h"
#include "osdep.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
extern int errno;		/* just in case */
#include "misc.h"


#include "fs_vms.c"
#include "ftl_vms.c"
#include "nl_vms.c"
#include "env_vms.c"
#include "tcp_vms.c"

#define server_login(user,pass,authuser,argc,argv) NIL
#define authserver_login(user,authuser,argc,argv) NIL
#define myusername() ""		/* dummy definition to prevent build errors */
#define MD5ENABLE ""

#include "auth_md5.c"
#include "auth_pla.c"
#include "auth_log.c"


/* Emulator for UNIX getpass() call
 * Accepts: prompt
 * Returns: password
 */

#define PWDLEN 128		/* used by Linux */

char *getpass (const char *prompt)
{
  char *s;
  static char pwd[PWDLEN];
  fputs (prompt,stdout);
  fgets (pwd,PWDLEN-1,stdin);
  if (s = strchr (pwd,'\n')) *s = '\0';
  return pwd;
}
