/*
 * Program:	String to unsigned long
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *
 * Date:	14 February 1995
 * Last Edited:	27 April 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/*
 * Turn a string unsigned long into the real thing
 * Accepts: source string
 *	    pointer to place to return end pointer
 *	    base
 * Returns: parsed unsigned long integer, end pointer is updated
 */

unsigned long strtoul (char *t,char **endp,int base)
{
  unsigned long value = 0;	/* the accumulated value */
  int negative = 0;		/* this a negative number? */
  unsigned char c,*s = t;
  if (base && (base < 2 || base > 36)) {
    errno = EINVAL;		/* insist upon valid base */
    return value;
  }
  while (isspace (*s)) s++;	/* skip leading whitespace */
  switch (*s) {			/* check for leading sign char */
  case '-':
    negative = 1;		/* yes, negative #.  fall into '+' */
  case '+':
    s++;			/* skip the sign character */
  }
  if (!base) {			/* base not specified? */
    if (*s != '0') base = 10;	/* must be decimal if doesn't start with 0 */
				/* starts with 0x? */
    else if (tolower (*++s) == 'x') {
      base = 16;		/* yes, is hex */
      s++;			/* skip the x */
    }
    else base = 8;		/* ...or octal */
  }
  do {				/* convert to numeric form if digit */
    if (isdigit (*s)) c = *s - '0';
				/* alphabetic conversion */
    else if (isalpha (*s)) c = *s - (isupper (*s) ? 'A' : 'a') + 10;
    else break;			/* else no way it's valid */
    if (c >= base) break;	/* digit out of range for base? */
    value = value * base + c;	/* accumulate the digit */
  } while (*++s);		/* loop until non-numeric character */
  if (tolower (*s) == 'l') s++;	/* ignore 'l' or 'L' marker */
  if (endp) *endp = s;		/* save users endp to after number */
				/* negate number if needed */
  return negative ? -value : value;
}
