#ifndef _PICO_OS_INCLUDED
#define _PICO_OS_INCLUDED


/*----------------------------------------------------------------------

   OS dependencies, SCO version.  See also the os-sco.c file.
   The following stuff may need to be changed for a new port, but once
   the port is done, it won't change.  At the bottom of the file are a few
   constants that you may want to configure differently than they
   are configured, but probably not.

 ----*/



/*----------------- Are we ANSI? ---------------------------------------*/
#define ANSI          /* this is an ANSI compiler */

/*------ If our compiler doesn't understand type void ------------------*/
/* #define void char */  /* no void in compiler */


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define  USE_DIRENT
#include <dirent.h>
#include <sys/stream.h>
#include <sys/ptem.h>


/*------- Some more includes that should usually be correct ------------*/
#include <pwd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>



/*----------------- locale.h -------------------------------------------*/
/* #include <locale.h> */ /* To make matching and sorting work right */
#define collator strucmp



/*----------------- time.h ---------------------------------------------*/
#include <time.h>
/* plain time.h isn't enough on some systems */
#include <sys/time.h>  /* For struct timeval usually in time.h */ 



/*--------------- signal.h ---------------------------------------------*/
#include <signal.h>      /* sometimes both required, sometimes */
/* signal handling is broken in the delivered SCO shells, so punt on
   suspending the current session */
#if defined(SIGTSTP)
#undef SIGTSTP
#endif

#define SigType void     /* value returned by sig handlers is void */
/* #define SigType int */   /* value returned by sig handlers is int */

/* #define POSIX_SIGNALS */ /* use POSIX signal semantics (ttyin.c) */
#define SYSV_SIGNALS    /* use System-V signal semantics (ttyin.c) */

#define	SIG_PROTO(args) ()



/*-------------- A couple typedef's for integer sizes ------------------*/
typedef unsigned int usign32_t;
typedef unsigned short usign16_t;



/*-------------- qsort argument type -----------------------------------*/
#define QSType void  /* qsort arg is of type void * */
/* #define QSType char */



/*-------------- fcntl flag to set non-blocking IO ---------------------*/
/*#define	NON_BLOCKING_IO	O_NONBLOCK */		/* POSIX style */
/*#define	NON_BLOCKING_IO	FNDELAY	*/	/* good ol' bsd style  */



/*
 * Choose one of the following three terminal drivers
 */

/*--------- Good 'ol BSD -----------------------------------------------*/
/* #include <sgtty.h> */   /* BSD-based systems */

/*--------- System V terminal driver -----------------------------------*/
#define HAVE_TERMIO     /* this is for pure System V */
#include <termio.h>     /* Sys V */

/*--------- POSIX terminal driver --------------------------------------*/
/* #define HAVE_TERMIOS */ /* this is an alternative */
/* #include <termios.h> */ /* POSIX */



/* Don't need to define this but do need to use either read.sel or read.pol
 * in osdep. */
/*-------- Use poll system call instead of select ----------------------*/
/* #define USE_POLL */     /* use the poll() system call instead of select() */



/*-------- Use terminfo database instead of termcap --------------------*/
#define USE_TERMINFO    /* use terminfo instead of termcap */
/* #define USE_TERMCAP */ /* use termcap */



/*-- What argument does wait(2) take? Define this if it is a union -----*/
/* #define HAVE_WAIT_UNION */ /* the arg to wait is a union wait * */



/*-------- Is window resizing available? -------------------------------*/
/* NOTE: Current reports are that ioctl(TIOCGWINSZ) either fails or */
/*       reports constant values, so there is no utility in having  */
/*       RESIZING enabled for SCO Unix. */
/* #if defined(TIOCGWINSZ) && defined(SIGWINCH) */
/* #define RESIZING  */
/* #endif */



/*-------- If no vfork, use regular fork -------------------------------*/
#define vfork fork  /* vfork is just a lightweight fork, so can use fork */



/*---- When no screen size can be discovered this is the size used -----*/
#define DEFAULT_LINES_ON_TERMINAL	(24)
#define DEFAULT_COLUMNS_ON_TERMINAL	(80)
#define NROW	DEFAULT_LINES_ON_TERMINAL
#define NCOL	DEFAULT_COLUMNS_ON_TERMINAL


/*----------------------------------------------------------------------

   Pico OS dependencies.

 ----*/


/*
 * File name separator, as a char and string
 */
#define C_FILESEP	'/'
#define S_FILESEP	"/"

/*
 * Place where mail gets delivered (for pico's new mail checking)
 */
#define MAILDIR		"/usr/spool/mail"

/*
 * What and where the tool that checks spelling is located.  If this is
 * undefined, then the spelling checker is not compiled into pico.
 */
#define SPELLER		"/usr/bin/spell"

#ifdef	MOUSE
#define	XTERM_MOUSE_ON	"\033[?1000h"	/* DECSET with parm 1000 */
#define	XTERM_MOUSE_OFF	"\033[?1000l"	/* DECRST with parm 1000  */
#endif

/*
 * Mode passed chmod() to make tmp files exclusively user read/write-able
 */
#define	MODE_READONLY	(0600)


/* memcpy() is no good for overlapping blocks.  If that's a problem, use
 * the memmove() in ../c-client
 */
#define bcopy(a,b,s) memcpy (b, a, s)

/*
 * ftruncate is missing on SCO UNIX (availabe from OpenServer 5 on)
 */
#define	ftruncate	chsize


#endif /* _PICO_OS_INCLUDED */
