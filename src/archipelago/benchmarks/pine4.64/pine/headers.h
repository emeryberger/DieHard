/*----------------------------------------------------------------------
  $Id: headers.h 13927 2004-12-21 23:56:53Z hubert $

            T H E    P I N E    M A I L   S Y S T E M

   Laurence Lundblade and Mike Seibel
   Networks and Distributed Computing
   Computing and Communications
   University of Washington
   Administration Builiding, AG-44
   Seattle, Washington, 98195, USA
   Internet: lgl@CAC.Washington.EDU
             mikes@CAC.Washington.EDU

   Please address all bugs and comments to "pine-bugs@cac.washington.edu"


   Pine and Pico are registered trademarks of the University of Washington.
   No commercial use of these trademarks may be made without prior written
   permission of the University of Washington.

   Pine, Pico, and Pilot software and its included text are Copyright
   1989-2000 by the University of Washington.

   The full text of our legal notices is contained in the file called
   CPYRIGHT, included with this distribution.


   Pine is in part based on The Elm Mail System:
    ***********************************************************************
    *  The Elm Mail System  -  Revision: 2.13                             *
    *                                                                     *
    * 			Copyright (c) 1986, 1987 Dave Taylor              *
    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
    ***********************************************************************
 

  ----------------------------------------------------------------------*/

/*======================================================================
       headers.h

   The include file to always include that includes a few other things
     -  includes the most general system files and other pine include files
     -  declares the global variables
       
 ====*/
         

#ifndef _HEADERS_INCLUDED
#define _HEADERS_INCLUDED

/*----------------------------------------------------------------------
           Include files
 
 System specific includes and defines are in os.h, the source for which
is os-xxx.h. (Don't edit osdep.h; edit os-xxx.h instead.)
 ----*/
#include "../pico/headers.h"


#include "../c-client/mail.h"

#include "os.h"

#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
    /*
     * If LEAVEOUTFIFO is set in os.h, then we leave it out.
     * If it isn't set, we still might leave it out. We'll decide
     * based on whether or not O_NONBLOCK is defined or not.
     * It's just a guess. Safer would be to change the polarity of the
     * test and define something like INCLUDEFIFO instead of LEAVEOUTFIFO
     * and only define it where we know. Since we don't really know
     * we'd rather run the risk of being wrong and finding out that
     * way instead of just having people not know about it.
     */
#if !defined(O_NONBLOCK)
#define LEAVEOUTFIFO 1
#endif
#endif

#include "../c-client/rfc822.h"
#include "../c-client/misc.h"

#ifdef  ENABLE_LDAP

#include <lber.h>
#include <ldap.h>

#ifndef LDAPAPI
#if defined(LDAP_API_VERSION)		/* draft-ietf-ldapext-ldap-c-api-04 */
#define LDAPAPI LDAP_API_VERSION
#elif defined(LDAP_OPT_SIZELIMIT)
#define LDAPAPI 15			/* Netscape SDK */
#elif defined(LDAP_BEGIN_DECL)
#define LDAPAPI 11			/* OpenLDAP 1.x */
#else					/* older version */
#define LDAPAPI 10			/* Umich */
#endif

#ifndef LDAP_OPT_ON
#define LDAP_OPT_ON ((void *)1)
#endif
#ifndef LDAP_OPT_OFF
#define LDAP_OPT_OFF ((void *)0)
#endif
#ifndef LDAP_OPT_SIZELIMIT
#define LDAP_OPT_SIZELIMIT 1134  /* we're hacking now! */
#endif
#ifndef LDAP_OPT_TIMELIMIT
#define LDAP_OPT_TIMELIMIT 1135
#endif
#ifndef LDAP_OPT_PROTOCOL_VERSION
#define LDAP_OPT_PROTOCOL_VERSION 1136
#endif

#ifndef LDAP_MSG_ONE
#define LDAP_MSG_ONE (0x00)
#define LDAP_MSG_ALL (0x01)
#define LDAP_MSG_RECEIVED (0x02)
#endif
#endif
#endif  /* ENABLE_LDAP */

#include "helptext.h"

#include "pine.h"

#include "context.h"



/*----------------------------------------------------------------------
    The few global variables we use in Pine
  ----*/

extern struct pine *ps_global;

extern char	   *pine_version;	/* pointer to version string	     */

#define SIZEOF_20KBUF (20480)
extern char         tmp_20k_buf[];

#ifdef DEBUG
extern FILE        *debugfile;		/* file for debug output	  */
extern int          debug;		/* debugging level or none (zero) */
#endif

#endif /* _HEADERS_INCLUDED */
