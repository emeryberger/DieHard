/*
 * Program:	unANSIify
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		4545 15th Ave NE
 *		Seattle, WA  98105-4527
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	1 June 1995
 * Last Edited:	2 July 2001
 *
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2001 University of Washington.
 *
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */


/* This program is designed to make traditional C sources out of my source
 * files which use ANSI syntax.  This program depends heavily upon knowledge
 * of the way I write code, and is not a general purpose tool.  I hope that
 * someday someone will provide a general purpose tool...
 */

#include <stdio.h>

#define LINELENGTH 1000


/* Find first non-whitespace
 * Accepts: string
 * Returns: 0 if no non-whitespace found, or pointer to non-whitespace
 */

char *fndnws (s)
     char *s;
{
  while ((*s == ' ') || (*s == '\t'))
    if ((*s++ == '\n') || !*s) return (char *) 0;
  return s;
}


/* Find first whitespace
 * Accepts: string
 * Returns: 0 if no whitespace found, or pointer to whitespace
 */

char *fndws (s)
     char *s;
{
  while ((*s != ' ') && (*s != '\t'))
    if ((*s++ == '\n') || !*s) return (char *) 0;
  return s;
}


/* Find end of commend
 * Accepts: string
 * Returns: -1 if end of comment found, else 0
 */

int fndcmt (s)
     char *s;
{
  while (*s && (*s != '\n'))
    if ((*s++ == '*') && (*s == '/')) return -1;
  return 0;
}

/* Output a line
 * Accepts: string
 */

void poot (s)
     char *s;
{
  if (s) fputs (s,stdout);
}


/* Skip prefix
 * Accepts: string
 * Returns: updated string
 */

char *skipfx (s)
     char *s;
{
  char c,*t = s,*tt;
				/* skip leading whitespace too */
  while ((*s == ' ') || (*s == '\t')) *s++;
  if (s != t) {
    c = *s;			/* save character */
    *s = '\0';			/* tie off prefix */
    poot (t);			/* output prefix */
    *s = c;			/* restore character */
  }
				/* a typedef? */
  if (((s[0] == 't') && (s[1] == 'y') && (s[2] == 'p') && (s[3] == 'e') &&
       (s[4] == 'd') && (s[5] == 'e') && (s[6] == 'f')) &&
      (t = fndws (s)) && (t = fndnws (t))) {
    if ((t[0] == 'u') && (t[1] == 'n') && (t[2] == 's') && (t[3] == 'i') &&
	(t[4] == 'g') && (t[5] == 'n') && (t[6] == 'e') && (t[7] == 'd') &&
	(tt = fndws (t+7)) && (tt = fndnws (tt))) t = tt;
    c = *t;			/* save character */
    *t = '\0';			/* tie off prefix */
    poot (s);			/* output prefix */
    *t = c;			/* restore character */
    s = t;			/* new string pointer */
  }
				/* static with known prefix */
  else if (((s[0] == 's') && (s[1] == 't') && (s[2] == 'a') &&
	    (s[3] == 't') && (s[4] == 'i') && (s[5] == 'c') &&
	    (s[6] == ' ')) &&
	   (((s[7] == 'u') && (s[8] == 'n') && (s[9] == 's') &&
	     (s[10] == 'i') && (s[11] == 'g') && (s[12] == 'n') &&
	     (s[13] == 'e') && (s[14] == 'd')) ||
	    ((s[7] == 's') && (s[8] == 't') && (s[9] == 'r') &&
	     (s[10] == 'u') && (s[11] == 'c') && (s[12] == 't')) ||
	    ((s[7] == 'd') && (s[8] == 'o')) ||
	    ((s[9] == 'e') && (s[10] == 'l') && (s[11] == 's') &&
	     (s[12] == 'e'))) &&
	   (t = fndws (s)) && (t = fndnws (t)) &&
	   (t = fndws (t)) && (t = fndnws (t))) {
    c = *t;			/* save character */
    *t = '\0';			/* tie off prefix */
    poot (s);			/* output prefix */
    *t = c;			/* restore character */
    s = t;			/* new string pointer */
  }
				/* one of the known prefixes? */
  else if ((((s[0] == 'u') && (s[1] == 'n') && (s[2] == 's') &&
	     (s[3] == 'i') && (s[4] == 'g') && (s[5] == 'n') &&
	     (s[6] == 'e') && (s[7] == 'd')) ||
	    ((s[0] == 's') && (s[1] == 't') && (s[2] == 'r') &&
	     (s[3] == 'u') && (s[4] == 'c') && (s[5] == 't')) ||
	    ((s[0] == 's') && (s[1] == 't') && (s[2] == 'a') &&
	     (s[3] == 't') && (s[4] == 'i') && (s[5] == 'c')) ||
	    ((s[0] == 'd') && (s[1] == 'o')) ||
	    ((s[0] == 'e') && (s[1] == 'l') && (s[2] == 's') &&
	     (s[3] == 'e'))) &&
	   (t = fndws (s)) && (t = fndnws (t))) {
    c = *t;			/* save character */
    *t = '\0';			/* tie off prefix */
    poot (s);			/* output prefix */
    *t = c;			/* restore character */
    s = t;			/* new string pointer */
  }
				/* may look like a proto, but isn't */
  else if ((s[0] == 'r') && (s[1] == 'e') && (s[2] == 't') && (s[3] == 'u') &&
	   (s[4] == 'r') && (s[5] == 'n')) {
    poot (s);
    return 0;
  }
  return s;
}

/* UnANSI a line
 * Accepts: string
 */

void unansi (s)
     char *s;
{
  char c,*t = s,*u,*v;
  while (t[1] && (t[1] != '\n')) t++;
  switch (*t) {
  case ',':			/* continued on next line? */
				/* slurp remainder of line */
    fgets (t + 1,LINELENGTH - (t + 1 - s),stdin);
    unansi (s);			/* try again */
    break;
  case ';':			/* function prototype? */
				/* yes, tie it off */
    *(fndws (fndnws (fndws (fndnws (s))))) = '\0';
    printf ("%s ();\n",s);	/* and output non-ANSI form */
    break;
  case ')':
    *t = '\0';			/* tie off args */
    if (*(t = fndnws (fndws (fndnws (fndws (fndnws (s)))))) == '(') {
      *t++ = '\0';		/* tie off */
      while ((*t == ' ') || (*t == '\t')) t++;
      if ((t[0] == 'v') && (t[1] == 'o') && (t[2] == 'i') && (t[3] == 'd') &&
	  !t[4]) *t = '\0';	/* make void be same as null */
      printf ("%s(",s);		/* output start of function */
      s = t;
      while (*s) {		/* for each argument */
	while ((*s == ' ') || (*s == '\t')) s++;
	for (u = v = s; (*u && (*u != ',') && (*u != '[')); u++)
	  if ((*u == ' ') || (*u == '\t')) v = u;
	c = *u;			/* remember delimiter */
	*u = '\0';		/* tie off argument name */
	while (*++v == '*');	/* remove leading pointer indication */
	fputs (v,stdout);	/* write variable name */
	*(s = u) = c;		/* restore delimiter */
	while (*s && (*s != ',')) *s++;
	if (*s) fputc (*s++,stdout);
      }
      puts (")");		/* end of function */
      while (*t) {		/* for each argument */
	fputs ("     ",stdout);
	while ((*t == ' ') || (*t == '\t')) t++;
	while (*t && (*t != ',')) fputc (*t++,stdout);
	puts (";");
	if (*t == ',') t++;
      }
    }
    else printf ("%s)",s);	/* say what?? */
    break;
  default:			/* doesn't look like a function */
    poot (s);
  }
}

main ()
{
  char *s,*t,line[LINELENGTH];
  int c;
  while (s = fgets (line,LINELENGTH,stdin)) switch (line[0]) {
  case '/':			/* comment */
    if ((s[1] != '*') || fndcmt (s+2)) poot (line);
    else do poot (line);
    while (!fndcmt (line) && (s = fgets (line,LINELENGTH,stdin)));
    break;
  case '{':			/* open function */
  case '}':			/* close function */
  case '\f':			/* formfeed */
  case '\n':			/* newline */
  case '#':			/* preprocessor command */
    poot (line);
    break;
  case '\t':			/* whitespace */
  case ' ':
				/* look like function arg def in structure? */
    if ((t = skipfx (line)) && (s = fndws (t)) && (s = fndnws (s)) &&
	(((*s == '(') && (s[1] == '*')) ||
	 ((*s == '*') && (s[1] == '(') && (s[2] == '*'))) &&
	(s = fndws (s)) && (s[-1] == ')') && (s = fndnws (s)) && (*s == '('))
      unansi (t);
    else poot (t);
    break;
  default:			/* begins with anything else */
				/* look like function proto or def? */
    if ((t = skipfx (line)) && (s = fndws (t)) && (s = fndnws (s)) &&
	(s = fndws (s)) && (s = fndnws (s)) && (*s == '('))
      unansi (t);
    else poot (t);
    break;
  }
}
