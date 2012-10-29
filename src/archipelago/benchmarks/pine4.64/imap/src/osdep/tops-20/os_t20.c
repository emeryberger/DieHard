/*
 * Program:	Operating-system dependent routines -- TOPS-20 version
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
 * Last Edited:	1 July 2002
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2002 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* Dedication:
 * This file is dedicated with affection to the TOPS-20 operating system, which
 * set standards for user and programmer friendliness that have still not been
 * equaled by more `modern' operating systems.
 * Wasureru mon ka!!!!
 */

#include "mail.h"
#include <jsys.h>		/* must be before tcp_t20.h */
#include "tcp_t20.h"		/* must be before osdep include tcp.h */
#include <time.h>
#include "osdep.h"
#include <sys/time.h>
#include "misc.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>

#include "fs_t20.c"
#include "ftl_t20.c"
#include "nl_t20.c"
#include "env_t20.c"
#include "tcp_t20.c"
#include "log_t20.c"

#define MD5ENABLE "PS:<SYSTEM>CRAM-MD5.PWD"
#include "auth_md5.c"
#include "auth_pla.c"
#include "auth_log.c"

/* Emulator for UNIX gethostid() call
 * Returns: host id
 */

long gethostid ()
{
  int argblk[5];
#ifndef _APRID
#define _APRID 28
#endif
  argblk[1] = _APRID;
  jsys (GETAB,argblk);
  return (long) argblk[1];
}


/* Emulator for UNIX getpass() call
 * Accepts: prompt
 * Returns: password
 */

#define PWDLEN 128		/* used by Linux */

char *getpass (const char *prompt)
{
  char *s;
  static char pwd[PWDLEN];
  int argblk[5],mode;
  argblk[1] = (int) (prompt-1);	/* prompt user */
  jsys (PSOUT,argblk);
  argblk[1] = _PRIIN;		/* get current TTY mode */
  jsys (RFMOD,argblk);
  mode = argblk[2];		/* save for later */
  argblk[2] &= ~06000;
  jsys (SFMOD,argblk);
  jsys (STPAR,argblk);
  fgets (pwd,PWDLEN-1,stdin);
  pwd[PWDLEN-1] = '\0';
  if (s = strchr (pwd,'\n')) *s = '\0';
  putchar ('\n');
  argblk[2] = mode;
  jsys (SFMOD,argblk);
  jsys (STPAR,argblk);
  return pwd;
}
