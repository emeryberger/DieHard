/*
 * Program:	c-client master include for application programs
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	19 May 2000
 * Last Edited:	20 October 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

#ifndef __CCLIENT_H		/* nobody should include this twice... */
#define __CCLIENT_H

#ifdef __cplusplus		/* help out people who use C++ compilers */
extern "C" {
  /* If you use gcc, you may also have to use -fno-operator-names */
#define private cclientPrivate	/* private to c-client */
#define and cclientAnd		/* C99 doesn't realize that ISO 646 is dead */
#define or cclientOr
#define not cclientNot
#endif

#include "mail.h"		/* primary interfaces */
#include "osdep.h"		/* OS-dependent routines */
#include "rfc822.h"		/* RFC822 and MIME routines */
#include "smtp.h"		/* SMTP sending routines */
#include "nntp.h"		/* NNTP sending routines */
#include "misc.h"		/* miscellaneous utility routines */

#ifdef __cplusplus		/* undo the C++ mischief */
#undef private
}
#endif

#endif
