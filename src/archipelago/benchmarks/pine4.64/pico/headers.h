/*======================================================================
  $Id: headers.h 9714 1999-01-27 22:14:21Z hubert $

       headers.h

   The include file to always include that includes a few other things
     -  includes the most general system files and other pine include files
     -  declares the global variables
       
 ====*/
         

#ifndef _PICO_HEADERS_INCLUDED
#define _PICO_HEADERS_INCLUDED

/*----------------------------------------------------------------------
           Include files
 
 System specific includes and defines are in os.h, the source for which
is os-xxx.h. (Don't edit os.h; edit os-xxx.h instead.)
 ----*/

#ifdef	TERMCAP_WINS
   Do not use TERMCAP WINS anymore. This is now set at runtime with
   pico -q, pilot -q, or the pine feature called termdef-takes-precedence.
   You do not need to do anything special while compiling.
#endif	/* TERMCAP_WINS */

#include <stdio.h>

#include "os.h"

/*
 * [Re]Define signal functions as needed...
 */
#ifdef	POSIX_SIGNALS
/*
 * Redefine signal call to our wrapper of POSIX sigaction
 */
#define	signal(SIG,ACT)		posix_signal(SIG,ACT)
#define	our_sigunblock(SIG)	posix_sigunblock(SIG)
#else	/* !POSIX_SIGNALS */
#ifdef	SYSV_SIGNALS
/*
 * Redefine signal calls to SYSV style call.
 */
#define	signal(SIG,ACT)		sigset(SIG,ACT)
#define	our_sigunblock(SIG)	sigrelse(SIG)
#else	/* !SYSV_SIGNALS */
#ifdef	WIN32
#define	signal(SIG,ACT)		mswin_signal(SIG,ACT)
#define	our_sigunblock(SIG)
#else	/* !WIN32 */
/*
 * Good ol' BSD signals.
 */
#define	our_sigunblock(SIG)
#endif /* !WIN32 */
#endif /* !SYSV_SIGNALS */
#endif /* !POSIX_SIGNALS */


/* These includes are all ANSI, and OK with all other compilers (so far) */
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>


#ifdef	ANSI
#define	PROTO(args)	args
#else
#define	PROTO(args)	()
#endif

#include "estruct.h"
#include "pico.h"
#include "edef.h"
#include "efunc.h"

#endif /* _PICO_HEADERS_INCLUDED */
