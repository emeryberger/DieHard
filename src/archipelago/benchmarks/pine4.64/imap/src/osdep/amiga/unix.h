/*
 * Program:	UNIX mail routines, Amiga version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	20 December 1989
 * Last Edited:	14 October 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/*				DEDICATION
 *
 *  This file is dedicated to my dog, Unix, also known as Yun-chan and
 * Unix J. Terwilliker Jehosophat Aloysius Monstrosity Animal Beast.  Unix
 * passed away at the age of 11 1/2 on September 14, 1996, 12:18 PM PDT, after
 * a two-month bout with cirrhosis of the liver.
 *
 *  He was a dear friend, and I miss him terribly.
 *
 *  Lift a leg, Yunie.  Luv ya forever!!!!
 */

/* Validate line
 * Accepts: pointer to candidate string to validate as a From header
 *	    return pointer to end of date/time field
 *	    return pointer to offset from t of time (hours of ``mmm dd hh:mm'')
 *	    return pointer to offset from t of time zone (if non-zero)
 * Returns: t,ti,zn set if valid From string, else ti is NIL
 */

#define VALID(s,x,ti,zn) {						\
  int remote = 0;							\
  ti = 0;								\
  if ((*s == 'F') && (s[1] == 'r') && (s[2] == 'o') && (s[3] == 'm') &&	\
      (s[4] == ' ')) {							\
    for (x = s + 5; *x && *x != '\012'; x++);				\
    if (*x) {								\
      if (x[-1] == '\015') --x;						\
      if (x - s >= 41) {						\
	for (zn = -1; x[zn] != ' '; zn--);				\
	if ((x[zn-1] == 'm') && (x[zn-2] == 'o') && (x[zn-3] == 'r') &&	\
	    (x[zn-4] == 'f') && (x[zn-5] == ' ') && (x[zn-6] == 'e') &&	\
	    (x[zn-7] == 't') && (x[zn-8] == 'o') && (x[zn-9] == 'm') &&	\
	    (x[zn-10] == 'e') && (x[zn-11] == 'r') && (x[zn-12] == ' '))\
	  {								\
	    while (x[zn-13] == ' ') zn--;				\
	    x += zn - 12;						\
	    remote = 1;							\
	  }								\
      }									\
      if (x - s >= 27) {						\
	if (x[-5] == ' ') {						\
	  if (x[-8] == ':') zn = 0,ti = -5;				\
	  else if (x[-9] == ' ') ti = zn = -9;				\
	  else if ((x[-11] == ' ') && ((x[-10]=='+') || (x[-10]=='-')))	\
	    ti = zn = -11;						\
	}								\
	else if (x[-4] == ' ') {					\
	  if (x[-9] == ' ') zn = -4,ti = -9;				\
	  else if ( (x[-13] == ' ') && (x[-16] == ' ')			\
		&& (x[-20] ==' ') &&					\
		( ((x[-22] == ' ') && (x[-23] == ',')) ||		\
		  ((x[-23] == ' ') && (x[-24] == ',')) ) ) {		\
	    char weekday[4]={0,}, month[4]={0,}, time[11]={0,};		\
	    char tzone[4]={0,}; 					\
	    char realtime[80];						\
	    int day,year,start=-26;					\
	    if (x[-23] == ' ') x--;					\
	      sscanf(&x[start],"%3c, %d %s %d %s %s",			\
		weekday,&day,month,&year,time,tzone);			\
	      sprintf(realtime,"%s %s %2d %s %d %s",			\
		weekday,month,day,time, 				\
		( (year < 100) ? year+1900 : year),tzone);		\
	      if (remote)						\
		strcat(realtime," remote from ");			\
	      else							\
		strcat(realtime,"\n");					\
	      strncpy(&x[start],realtime,strlen(realtime));		\
	      zn = -2;							\
	      ti = -7;							\
	  }								\
	}								\
	else if (x[-6] == ' ') {					\
	  if ((x[-11] == ' ') && ((x[-5] == '+') || (x[-5] == '-')))	\
	    zn = -6,ti = -11;						\
	}								\
	else if (x[-9] == ' ') {					\
	    if ( ( (x[-12] == ' ') && (x[-16] == ' ') &&		\
		  ( ((x[-18] == ' ') && (x[-19] == ',') )  ||		\
		    ((x[-19] == ' ') && (x[-20] == ',')) )		\
		||							\
		((x[-14] == ' ') && (x[-18] == ' ') &&			\
		  ( ((x[-20] == ' ') && (x[-21] == ',') )  ||		\
		    ((x[-21] == ' ') && (x[-22] == ',')) ) ) ) ) {	\
	      char weekday[4]={0,}, month[4]={0,},time[11]={0,};	\
	      int day,year,start=-24;					\
	      char realtime[80];					\
	      if (x[-12] == ' ') x++;					\
	      if (x[-19] == ' ') x++;					\
	      sscanf(&x[start],"%3c, %d %3c %d %s",weekday,		\
		     &day,month,&year,time);				\
	      sprintf(realtime,"%s %s %2d %s %d",weekday,month,day,time,\
		 ( (year < 100) ? year+1900 : year));			\
	      if (remote)						\
		strcat(realtime," remote from ");			\
	      else							\
		strcat(realtime,"\n");					\
	      strncpy(&x[start],realtime,strlen(realtime));		\
	      ti=-5;							\
	      zn=0;							\
	    }								\
	}								\
	if (ti && !((x[ti - 3] == ':') &&				\
		    (x[ti -= ((x[ti - 6] == ':') ? 9 : 6)] == ' ') &&	\
		    (x[ti - 3] == ' ') && (x[ti - 7] == ' ') &&		\
		    (x[ti - 11] == ' '))) ti = 0;			\
      }									\
    }									\
  }									\
}

/* You are not expected to understand this macro, but read the next page if
 * you are not faint of heart.
 *
 * Known formats to the VALID macro are:
 *		From user Wed Dec  2 05:53 1992
 * BSD		From user Wed Dec  2 05:53:22 1992
 * SysV		From user Wed Dec  2 05:53 PST 1992
 * rn		From user Wed Dec  2 05:53:22 PST 1992
 *		From user Wed Dec  2 05:53 -0700 1992
 * emacs	From user Wed Dec  2 05:53:22 -0700 1992
 *		From user Wed Dec  2 05:53 1992 PST
 *		From user Wed Dec  2 05:53:22 1992 PST
 *		From user Wed Dec  2 05:53 1992 -0700
 * Solaris	From user Wed Dec  2 05:53:22 1992 -0700
 *
 * Amiga	From user Wed, 6 Dec 92 05:53:22 who did this !!!
 *		CHANGED in place to
 *		From user Wed Dec  2 05:53:22 1992
 *
 * Plus all of the above with `` remote from xxx'' after it. Thank you very
 * much, smail and Solaris, for making my life considerably more complicated.
 */

/*
 * What?  You want to understand the VALID macro anyway?  Alright, since you
 * insist.  Actually, it isn't really all that difficult, provided that you
 * take it step by step.
 *
 * Line 1	Initializes the return ti value to failure (0);
 * Lines 2-3	Validates that the 1st-5th characters are ``From ''.
 * Lines 4-6	Validates that there is an end of line and points x at it.
 * Lines 7-14	First checks to see if the line is at least 41 characters long.
 *		If so, it scans backwards to find the rightmost space.  From
 *		that point, it scans backwards to see if the string matches
 *		`` remote from''.  If so, it sets x to point to the space at
 *		the start of the string.
 * Line 15	Makes sure that there are at least 27 characters in the line.
 * Lines 16-21	Checks if the date/time ends with the year (there is a space
 *		five characters back).  If there is a colon three characters
 *		further back, there is no timezone field, so zn is set to 0
 *		and ti is set in front of the year.  Otherwise, there must
 *		either to be a space four characters back for a three-letter
 *		timezone, or a space six characters back followed by a + or -
 *		for a numeric timezone; in either case, zn and ti become the
 *		offset of the space immediately before it.
 * Lines 22-24	Are the failure case for line 14.  If there is a space four
 *		characters back, it is a three-letter timezone; there must be a
 *		space for the year nine characters back.  zn is the zone
 *		offset; ti is the offset of the space.
 * Lines 25-28	Are the failure case for line 20.  If there is a space six
 *		characters back, it is a numeric timezone; there must be a
 *		space eleven characters back and a + or - five characters back.
 *		zn is the zone offset; ti is the offset of the space.
 * Line 29-32	If ti is valid, make sure that the string before ti is of the
 *		form www mmm dd hh:mm or www mmm dd hh:mm:ss, otherwise
 *		invalidate ti.  There must be a colon three characters back
 *		and a space six or nine	characters back (depending upon
 *		whether or not the character six characters back is a colon).
 *		There must be a space three characters further back (in front
 *		of the day), one seven characters back (in front of the month),
 *		and one eleven characters back (in front of the day of week).
 *		ti is set to be the offset of the space before the time.
 *
 * Why a macro?  It gets invoked a *lot* in a tight loop.  On some of the
 * newer pipelined machines it is faster being open-coded than it would be if
 * subroutines are called.
 *
 * Why does it scan backwards from the end of the line, instead of doing the
 * much easier forward scan?  There is no deterministic way to parse the
 * ``user'' field, because it may contain unquoted spaces!  Yes, I tested it to
 * see if unquoted spaces were possible.  They are, and I've encountered enough
 * evil mail to be totally unwilling to trust that ``it will never happen''.
 */

/* Build parameters */

#define KODRETRY 15		/* kiss-of-death retry in seconds */
#define LOCKTIMEOUT 5		/* lock timeout in minutes */
#define CHUNK 16384		/* read-in chunk size */
