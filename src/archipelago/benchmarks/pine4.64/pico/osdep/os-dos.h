#line 2 "osdep\os-dos.h"
#ifndef _PICO_OS_INCLUDED
#define _PICO_OS_INCLUDED


/*----------------------------------------------------------------------

   OS dependencies, DOS version.  See also the os-dos.c file.
   The following stuff may need to be changed for a new port, but once
   the port is done, it won't change.  At the bottom of the file are a few
   constants that you may want to configure differently than they
   are configured, but probably not.

 ----*/



/*----------------- Are we ANSI? ---------------------------------------*/
#define ANSI          /* this is an ANSI compiler */

/*------ If our compiler doesn't understand type void ------------------*/
/* #define void char */  /* no void in compiler */

/*-------- Standard ANSI functions usually defined in stdlib.h ---------*/
#include	<stdlib.h>
#include	<string.h>
#include	<dos.h>
#include	<direct.h>
#include	<search.h>
#undef	CTRL
#include	<sys/types.h>
#include	<sys/stat.h>

#include <io.h>
#include <sys/stat.h>
#ifdef	PCTCP
#include <4bsddefs.h>			/* PC/TCP Include Files */
#include <netdb.h>
#undef	signal				/* override their stupid def */
#define	sleep	_sleep			/* use pico's sleep function */
#undef	write				/* make way:c-client def's write */
#undef	SIGCHLD
#else
#ifdef	LWP
#include <sys/socket.h>			/* LanWorkplace Include Files */
#include <netinet/in.h>
#include <netdb.h>
#else
#ifdef	PCNFS
#include <tklib.h>
#include <netdb.h>
#define	ENAMETOOLONG	0		/* unsupported errno value */
#else
#include <tcp.h>			/* WATTCP Include File */
#define	ENAMETOOLONG	0		/* unsupported errno value */
#define	word	p_word			/* compensate for bad args labels */
#endif
#endif
#endif
#include <dos.h>
#include <direct.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/timeb.h>

#undef ERROR



/*----------------- locale.h -------------------------------------------*/
/* #include <locale.h> */ /* To make matching and sorting work right */
#define collator strucmp



/*----------------- time.h ---------------------------------------------*/
#include <time.h>
/* plain time.h isn't enough on some systems */
/* #include <sys/time.h> */ /* For struct timeval usually in time.h */ 



/*--------------- signal.h ---------------------------------------------*/
#include <signal.h>      /* sometimes both required, sometimes */
/* #include <sys/signal.h> */ /* only one or the other */

#define SigType void     /* value returned by sig handlers is void */
/* #define SigType int  */  /* value returned by sig handlers is int */

/* #define POSIX_SIGNALS */ /* use POSIX signal semantics (ttyin.c) */
/* #define SYSV_SIGNALS */ /* use System-V signal semantics (ttyin.c) */

#define	SIG_PROTO(args) args



/*-------------- A couple typedef's for integer sizes ------------------*/
typedef unsigned long usign32_t;
typedef unsigned short usign16_t;



/*-------------- qsort argument type -----------------------------------*/
#define QSType void  /* qsort arg is of type void * */
/* #define QSType char */



/*-------- Use terminfo database instead of termcap --------------------*/
/* #define USE_TERMINFO */ /* use terminfo instead of termcap */
/* #define USE_TERMCAP */ /* use termcap */



/*-------- Is window resizing available? -------------------------------*/
/* #define RESIZING */ /* SIGWINCH and friends */
	/* Actually, under windows it is, but RESIZING compiles in UNIX
	 * signals code for determining when the window resized.  Window's
	 * works differently. */



/*-------- If no vfork, use regular fork -------------------------------*/
/* #define vfork fork */ /* vfork is just a lightweight fork, so can use fork */



/*---- When no screen size can be discovered this is the size used -----*/
#define DEFAULT_LINES_ON_TERMINAL	(25)
#define DEFAULT_COLUMNS_ON_TERMINAL	(80)
#define NROW	DEFAULT_LINES_ON_TERMINAL
#define NCOL	DEFAULT_COLUMNS_ON_TERMINAL


/*----------------------------------------------------------------------

   Pico OS dependencies.

 ----*/


/*
 * File name separator, as a char and string
 */
#define C_FILESEP	'\\'
#define S_FILESEP	"\\"

/*
 * What and where the tool that checks spelling is located.  If this is
 * undefined, then the spelling checker is not compiled into pico.
 */
#undef SPELLER

/*
 * Mode passed chmod() to make tmp files exclusively user read/write-able
 */
#define	MODE_READONLY	(S_IREAD | S_IWRITE)

#define IBMPC   1

#ifdef  PCTCP
#define sleep   _sleep
#define _O_RDONLY O_RDONLY
#endif

/*
 *  IBM PC ROM BIOS Services used
 */
#define BIOS_VIDEO      0x10
#define BIOS_KEYBRD     0x16
#define BIOS_MOUSE      0x33
#define DOS_MULTIPLEX   0x2f

#ifdef  maindef
/*      possible names and paths of help files under different OSs      */

char *pathname[] = {
	"picorc",
	"pico.hlp",
	"\\usr\\local\\",
	"\\usr\\lib\\",
	""
};

#define NPNAMES (sizeof(pathname)/sizeof(char *))

/*
 * Make sys_errlist visible
 */
extern char *sys_errlist[];
extern int   sys_nerr;

jmp_buf got_hup;

#endif /* _PICO_OS_INCLUDED */
