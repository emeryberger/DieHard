#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: strings.c 14075 2005-08-30 00:08:19Z hubert@u.washington.edu $";
#endif
/*----------------------------------------------------------------------

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
   1989-2005 by the University of Washington.

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
    strings.c
    Misc extra and useful string functions
      - rplstr         replace a substring with another string
      - sqzspaces      Squeeze out the extra blanks in a string
      - sqznewlines    Squeeze out \n and \r.
      - removing_trailing_white_space 
      - short_str      Replace part of string with ... for display
      - removing_leading_white_space 
		       Remove leading or trailing white space
      - removing_double_quotes 
		       Remove surrounding double quotes
      - strclean       
                       both of above plus convert to lower case
      - skip_white_space
		       return pointer to first non-white-space char
      - skip_to_white_space
		       return pointer to first white-space char
      - srchstr        Search a string for first occurrence of a sub string
      - srchrstr       Search a string for last occurrence of a sub string
      - strindex       Replacement for strchr/index
      - strrindex      Replacement for strrchr/rindex
      - sstrcpy        Copy one string onto another, advancing dest'n pointer
      - istrncpy       Copy n chars between bufs, making ctrl chars harmless
      - month_abbrev   Return three letter abbreviations for months
      - month_num      Calculate month number from month/year string
      - cannon_date    Formalize format of a some what formatted date
      - pretty_command Return nice string describing character
      - repeat_char    Returns a string n chars long
      - comatose       Format number with nice commas
      - fold           Inserts newlines for folding at whitespace.
      - byte_string    Format number of bytes with Kb, Mb, Gb or bytes
      - enth-string    Format number i.e. 1: 1st, 983: 983rd....
      - string_to_cstring  Convert string to C-style constant string with \'s
      - cstring_to_hexstring  Convert cstring to hex string
      - cstring_to_string  Convert C-style string to string
      - add_backslash_escapes    Escape / and \ with \
      - remove_backslash_escapes Undo the \ escaping, and stop string at /.

 ====*/

#include "headers.h"
#include "../c-client/utf8.h"

typedef struct role_args {
    char    *ourcharset;
    int      multi;
    char   **cset;
} ROLE_ARGS_T;

char       *pattern_to_config PROTO((PATTERN_S *));
PATTERN_S  *config_to_pattern PROTO((char *));
char       *address_to_configstr PROTO((ADDRESS *));
ADDRESS    *configstr_to_address PROTO((char *));
char       *add_escapes PROTO((char *, char *, int, char *, char *));
char       *dollar_escape_dollars PROTO((char *));
char       *add_pat_escapes PROTO((char *));
void        char_to_octal_triple PROTO((int, char *));
int         read_octal PROTO((char **));
char       *copy_quoted_string_asis PROTO((char *));
void        open_any_patterns PROTO((long));
void        sub_open_any_patterns PROTO((long));
int         sub_any_patterns PROTO((long, PAT_STATE *));
void        sub_close_patterns PROTO((long));
int         sub_write_patterns PROTO((long));
PAT_S      *first_any_pattern PROTO((PAT_STATE *));
PAT_S      *last_any_pattern PROTO((PAT_STATE *));
PAT_S      *prev_any_pattern PROTO((PAT_STATE *));
PAT_S      *next_any_pattern PROTO((PAT_STATE *));
int         write_pattern_file PROTO((char **, PAT_LINE_S *));
int         write_pattern_lit PROTO((char **, PAT_LINE_S *));
int         write_pattern_inherit PROTO((char **, PAT_LINE_S *));
char       *data_for_patline PROTO((PAT_S *));
PAT_LINE_S *parse_pat_lit PROTO((char *));
PAT_LINE_S *parse_pat_inherit PROTO((void));
PAT_S      *parse_pat PROTO((char *));
void        parse_patgrp_slash PROTO((char *, PATGRP_S *));
void        parse_action_slash PROTO((char *, ACTION_S *));
void        free_patline PROTO((PAT_LINE_S **));
void        free_patgrp PROTO((PATGRP_S **));
ARBHDR_S   *parse_arbhdr PROTO((char *));
void        free_arbhdr PROTO((ARBHDR_S **));
void        free_intvl PROTO((INTVL_S **));
void        set_up_search_pgm PROTO((char *, PATTERN_S *, SEARCHPGM *,
				     ROLE_ARGS_T *));
SEARCHPGM  *next_not PROTO((SEARCHPGM *));
SEARCHOR   *next_or PROTO((SEARCHOR **));
char       *next_arb PROTO((char *));
void        add_type_to_pgm PROTO((char *, PATTERN_S *, SEARCHPGM *,
				   ROLE_ARGS_T *));
void        set_srch PROTO((char *, char *, SEARCHPGM *, ROLE_ARGS_T *));
void        set_srch_hdr PROTO((char *, char *, SEARCHPGM *, ROLE_ARGS_T *));
void        set_search_by_age PROTO((INTVL_S *, SEARCHPGM *, int));
void        set_search_by_size PROTO((INTVL_S *, SEARCHPGM *));
int	    non_eh PROTO((char *));
void        add_eh PROTO((char **, char **, char *, int *));
void        set_extra_hdrs PROTO((char *));
int         is_ascii_string PROTO((char *));
KEYWORD_S  *new_keyword_s PROTO((char *, char *));
int	    rfc2369_parse PROTO((char *, RFC2369_S *));
int         test_message_with_cmd PROTO((MAILSTREAM *, long, char *, long,
					 int *));
int         charsets_present_in_msg PROTO((MAILSTREAM *, unsigned long,
					   STRLIST_S *));
void        collect_charsets_from_subj PROTO((ENVELOPE *, STRLIST_S **));
void        collect_charsets_from_body PROTO((BODY *, STRLIST_S **));
ACTION_S   *combine_inherited_role_guts PROTO((ACTION_S *));



/*
 * Useful def's to help with HEX string conversions
 */
#define	XDIGIT2C(C)	((C) - (isdigit((unsigned char) (C)) \
			  ? '0' : (isupper((unsigned char)(C))? '7' : 'W')))
#define	X2C(S)		((XDIGIT2C(*(S)) << 4) | XDIGIT2C(*((S)+1)))
#define	C2XPAIR(C, S)	{ \
			    *(S)++ = HEX_CHAR1(C); \
			    *(S)++ = HEX_CHAR2(C); \
			}





/*----------------------------------------------------------------------
       Replace n characters in one string with another given string

   args: os -- the output string
         dl -- the number of character to delete from start of os
         is -- The string to insert
  
 Result: returns pointer in originl string to end of string just inserted
         First 
  ---*/
char *
rplstr(os,dl,is)
char *os,*is;
int dl;
{   
    register char *x1,*x2,*x3;
    int           diff;

    if(os == NULL)
        return(NULL);
       
    for(x1 = os; *x1; x1++);
    if(dl > x1 - os)
        dl = x1 - os;
        
    x2 = is;      
    if(is != NULL){
        while(*x2++);
        x2--;
    }

    if((diff = (x2 - is) - dl) < 0){
        x3 = os; /* String shrinks */
        if(is != NULL)
            for(x2 = is; *x2; *x3++ = *x2++); /* copy new string in */
        for(x2 = x3 - diff; *x2; *x3++ = *x2++); /* shift for delete */
        *x3 = *x2;
    } else {                
        /* String grows */
        for(x3 = x1 + diff; x3 >= os + (x2 - is); *x3-- = *x1--); /* shift*/
        for(x1 = os, x2 = is; *x2 ; *x1++ = *x2++);
        while(*x3) x3++;                 
    }
    return(x3);
}



/*----------------------------------------------------------------------
     Squeeze out blanks 
  ----------------------------------------------------------------------*/
void
sqzspaces(string)
     char *string;
{
    char *p = string;

    while(*string = *p++)		   /* while something to copy       */
      if(!isspace((unsigned char)*string)) /* only really copy if non-blank */
	string++;
}



/*----------------------------------------------------------------------
     Squeeze out CR's and LF's 
  ----------------------------------------------------------------------*/
void
sqznewlines(string)
    char *string;
{
    char *p = string;

    while(*string = *p++)		      /* while something to copy  */
      if(*string != '\r' && *string != '\n')  /* only copy if non-newline */
	string++;
}



/*----------------------------------------------------------------------  
       Remove leading white space from a string in place
  
  Args: string -- string to remove space from
  ----*/
void
removing_leading_white_space(string)
     char *string;
{
    register char *p;

    if(!string)
      return;

    for(p = string; *p; p++)		/* find the first non-blank  */
      if(!isspace((unsigned char) *p)){
	  while(*string++ = *p++)	/* copy back from there... */
	    ;

	  return;
      }
}



/*----------------------------------------------------------------------  
       Remove trailing white space from a string in place
  
  Args: string -- string to remove space from
  ----*/
void
removing_trailing_white_space(string)
    char *string;
{
    char *p = NULL;

    if(!string)
      return;

    for(; *string; string++)		/* remember start of whitespace */
      p = (!isspace((unsigned char)*string)) ? NULL : (!p) ? string : p;

    if(p)				/* if whitespace, blast it */
      *p = '\0';
}


void
removing_leading_and_trailing_white_space(string)
     char *string;
{
    register char *p, *q = NULL;

    if(!string)
      return;

    for(p = string; *p; p++)		/* find the first non-blank  */
      if(!isspace((unsigned char)*p)){
	  while(*string = *p++){	/* copy back from there... */
	      q = (!isspace((unsigned char)*string)) ? NULL : (!q) ? string : q;
	      string++;
	  }

	  if(q)
	    *q = '\0';
	    
	  return;
      }

    if(*string != '\0')
      *string = '\0';
}


/*----------------------------------------------------------------------  
       Remove one set of double quotes surrounding string in place
       Returns 1 if quotes were removed
  
  Args: string -- string to remove quotes from
  ----*/
int
removing_double_quotes(string)
     char *string;
{
    register char *p;
    int ret = 0;

    if(string && string[0] == '"' && string[1] != '\0'){
	p = string + strlen(string) - 1;
	if(*p == '"'){
	    ret++;
	    *p = '\0';
	    for(p = string; *p; p++) 
	      *p = *(p+1);
	}
    }

    return(ret);
}



/*----------------------------------------------------------------------  
  return a pointer to first non-whitespace char in string
  
  Args: string -- string to scan
  ----*/
char *
skip_white_space(string)
     char *string;
{
    while(*string && isspace((unsigned char) *string))
      string++;

    return(string);
}



/*----------------------------------------------------------------------  
  return a pointer to first whitespace char in string
  
  Args: string -- string to scan
  ----*/
char *
skip_to_white_space(string)
     char *string;
{
    while(*string && !isspace((unsigned char) *string))
      string++;

    return(string);
}



/*----------------------------------------------------------------------  
       Remove quotes from a string in place
  
  Args: string -- string to remove quotes from
  Rreturns: string passed us, but with quotes gone
  ----*/
char *
removing_quotes(string)
    char *string;
{
    register char *p, *q;

    /* 
     * Wart/bug/feature: strings with 2 consecutive quotes turn
     * the two quotes into one.  This was biting reply-indent-string
     * with a value of "", but the fix was to call 
     * removing_double_quotes instead.
     * Easy enough to fix, but who's to say what fixing it will break
     */
    if(*(p = q = string) == '\"'){
	do
	  if(*q == '\"' || *q == '\\')
	    q++;
	while(*p++ = *q++);
    }

    return(string);
}



/*---------------------------------------------------
     Remove leading whitespace, trailing whitespace and convert 
     to lowercase

   Args: s, -- The string to clean

 Result: the cleaned string
  ----*/
char *
strclean(string)
     char *string;
{
    char *s = string, *sc = NULL, *p = NULL;

    for(; *s; s++){				/* single pass */
	if(!isspace((unsigned char)*s)){
	    p = NULL;				/* not start of blanks   */
	    if(!sc)				/* first non-blank? */
	      sc = string;			/* start copying */
	}
	else if(!p)				/* it's OK if sc == NULL */
	  p = sc;				/* start of blanks? */

	if(sc)					/* if copying, copy */
	  *sc++ = isupper((unsigned char)(*s))
			  ? (unsigned char)tolower((unsigned char)(*s))
			  : (unsigned char)(*s);
    }

    if(p)					/* if ending blanks  */
      *p = '\0';				/* tie off beginning */
    else if(!sc)				/* never saw a non-blank */
      *string = '\0';				/* so tie whole thing off */

    return(string);
}


/*
 * Returns a pointer to a short version of the string.
 * If src is not longer than len, pointer points to src.
 * If longer than len, a version which is len long is made in
 * buf and the pointer points there.
 *
 * Args  src -- The string to be shortened
 *       buf -- A place to put the short version, length should be >= len+1
 *       len -- Desired length of shortened string
 *     where -- Where should the dots be in the shortened string. Can be
 *              FrontDots, MidDots, EndDots.
 *
 *     FrontDots           ...stuvwxyz
 *     EndDots             abcdefgh...
 *     MidDots             abcd...wxyz
 */
char *
short_str(src, buf, len, where)
    char     *src;
    char     *buf;
    int       len;
    WhereDots where;
{
    char *ans;
    int   alen, first, second;

    if(len <= 0)
      ans = "";
    else if((alen = strlen(src)) <= len)
      ans = src;
    else{
	ans = buf;
	if(len < 5){
	    strncpy(buf, "....", len);
	    buf[len] = '\0';
	}
	else{
	    /*
	     * first == length of preellipsis text
	     * second == length of postellipsis text
	     */
	    if(where == FrontDots){
		first = 0;
		second = len - 3;
	    }
	    else if(where == MidDots){
		first = (len - 3)/2;
		second = len - 3 - first;
	    }
	    else if(where == EndDots){
		first = len - 3;
		second = 0;
	    }

	    if(first)
	      strncpy(buf, src, first);

	    strcpy(buf+first, "...");
	    if(second)
	      strncpy(buf+first+3, src+alen-second, second);

	    buf[len] = '\0';
	}
    }
    
    return(ans);
}



/*----------------------------------------------------------------------
        Search one string for another

   Args:  is -- The string to search in, the larger string
          ss -- The string to search for, the smaller string

   Search for first occurrence of ss in the is, and return a pointer
   into the string is when it is found. The search is case indepedent.
  ----*/
char *	    
srchstr(is, ss)
    char *is, *ss;
{
    register char *p, *q;

    if(ss && is)
      for(; *is; is++)
	for(p = ss, q = is; ; p++, q++){
	    if(!*p)
	      return(is);			/* winner! */
	    else if(!*q)
	      return(NULL);			/* len(ss) > len(is)! */
	    else if(*p != *q && !CMPNOCASE(*p, *q))
	      break;
	}

    return(NULL);
}



/*----------------------------------------------------------------------
        Search one string for another, from right

   Args:  is -- The string to search in, the larger string
          ss -- The string to search for, the smaller string

   Search for last occurrence of ss in the is, and return a pointer
   into the string is when it is found. The search is case indepedent.
  ----*/

char *	    
srchrstr(is, ss)
register char *is, *ss;
{                    
    register char *sx, *sy;
    char          *ss_store, *rv;
    char          *begin_is;
    char           temp[251];
    
    if(is == NULL || ss == NULL)
      return(NULL);

    if(strlen(ss) > sizeof(temp) - 2)
      ss_store = (char *)fs_get(strlen(ss) + 1);
    else
      ss_store = temp;

    for(sx = ss, sy = ss_store; *sx != '\0' ; sx++, sy++)
      *sy = isupper((unsigned char)(*sx))
		      ? (unsigned char)tolower((unsigned char)(*sx))
		      : (unsigned char)(*sx);
    *sy = *sx;

    begin_is = is;
    is = is + strlen(is) - strlen(ss_store);
    rv = NULL;
    while(is >= begin_is){
        for(sx = is, sy = ss_store;
	    ((*sx == *sy)
	      || ((isupper((unsigned char)(*sx))
		     ? (unsigned char)tolower((unsigned char)(*sx))
		     : (unsigned char)(*sx)) == (unsigned char)(*sy))) && *sy;
	    sx++, sy++)
	   ;

        if(!*sy){
            rv = is;
            break;
        }

        is--;
    }

    if(ss_store != temp)
      fs_give((void **)&ss_store);

    return(rv);
}



/*----------------------------------------------------------------------
    A replacement for strchr or index ...

    Returns a pointer to the first occurrence of the character
    'ch' in the specified string or NULL if it doesn't occur

 ....so we don't have to worry if it's there or not. We bring our own.
If we really care about efficiency and think the local one is more
efficient the local one can be used, but most of the things that take
a long time are in the c-client and not in pine.
 ----*/
char *
strindex(buffer, ch)
    char *buffer;
    int ch;
{
    do
      if(*buffer == ch)
	return(buffer);
    while (*buffer++ != '\0');

    return(NULL);
}


/* Returns a pointer to the last occurrence of the character
 * 'ch' in the specified string or NULL if it doesn't occur
 */
char *
strrindex(buffer, ch)
    char *buffer;
    int   ch;
{
    char *address = NULL;

    do
      if(*buffer == ch)
	address = buffer;
    while (*buffer++ != '\0');
    return(address);
}



/*----------------------------------------------------------------------
  copy the source string onto the destination string returning with
  the destination string pointer at the end of the destination text

  motivation for this is to avoid twice passing over a string that's
  being appended to twice (i.e., strcpy(t, x); t += strlen(t))
 ----*/
void
sstrcpy(d, s)
    char **d;
    char *s;
{
    while((**d = *s++) != '\0')
      (*d)++;
}


void
sstrncpy(d, s, n)
    char **d;
    char *s;
    int n;
{
    while(n-- > 0 && (**d = *s++) != '\0')
      (*d)++;
}


/*----------------------------------------------------------------------
  copy at most n chars of the source string onto the destination string
  returning pointer to start of destination and converting any undisplayable
  characters to harmless character equivalents.
 ----*/
char *
istrncpy(d, s, n)
    char *d, *s;
    int n;
{
    char *rv = d, *p, *src = s, *trans = NULL;
    unsigned char c;

    if(!d || !s)
      return(NULL);
    
    /*
     * Check to see if we can skip the translation thing.
     */
    if(F_OFF(F_DISABLE_2022_JP_CONVERSIONS, ps_global)){
	for(p = s; *p && (p-s+1) < n; p++)
	  if(*p == ESCAPE && *(p+1) && match_escapes(p+1))
	    break;

	/* it looks like we may have to translate */
	if(*p && (p-s+1) < n){
	    unsigned int limit = n;

	    trans = trans_2022_jp_to_euc(s, &limit);
	    if(trans && strcmp(trans, s))
	      src = trans;
	}
    }

    /* src is either original source or the translation string */
    s = src;

    /* copy while escaping evil chars */
    do
      if(*s && FILTER_THIS(*s)){
	if(n-- > 0){
	    c = (unsigned char) *s;
	    *d++ = c >= 0x80 ? '~' : '^';

	    if(n-- > 0){
		s++;
		*d = (c == 0x7f) ? '?' :
		      (c == 0x1b) ? '[' : (c & 0x1f) + '@';
	    }
	}
      }
      else{
	  if(n-- > 0)
	    *d = *s++;
      }
    while(n > 0 && *d++);

    if(trans)
      fs_give((void **) &trans);

    return(rv);
}


/*
 * Copies the source string into allocated space with the 8-bit EUC codes
 * (on Unix) or the Shift-JIS (on PC) converted into ISO-2022-JP.
 * Caller is responsible for freeing the result.
 */
unsigned char *
trans_euc_to_2022_jp(src)
    unsigned char *src;
{
    size_t len, alloc;
    unsigned char *rv, *p, *q;
    int    inside_esc_seq = 0;
    int    c1 = -1;		/* remembers first of pair for Shift-JIS */

    if(!src)
      return(NULL);
    
    if(F_ON(F_DISABLE_2022_JP_CONVERSIONS, ps_global))
      return((unsigned char *) cpystr((char *) src));

    len = strlen((char *) src);

    /*
     * Worst possible increase is every other character an 8-bit character.
     * In that case, each of those gets 6 extra charactes for the escape
     * sequences. We're not too concerned about the extra length because
     * these are relatively short strings.
     */
    alloc = len + 1 + ((len+1)/2) * 6;
    rv = (unsigned char *) fs_get(alloc * sizeof(char));

    for(p = src, q = rv; *p; p++){
	if(inside_esc_seq){
	    if(c1 >= 0){			/* second of a pair? */
		int adjust = *p < 159;
		int rowOffset = c1 < 160 ? 112 : 176;
		int cellOffset = adjust ? (*p > 127 ? 32 : 31) : 126;

		*q++ = ((c1 - rowOffset) << 1) - adjust;
		*q++ = *p - cellOffset;
		c1 = -1;
	    }
	    else if(*p & 0x80){
#ifdef _WINDOWS
		c1 = *p;			/* remember first of pair */
#else						/* EUC */
		*q++ = (*p & 0x7f);
#endif
	    }
	    else{
		*q++ = '\033';
		*q++ = '(';
		*q++ = 'B';
		*q++ = (*p);
		c1 = -1;
		inside_esc_seq = 0;
	    }
	}
	else{
	    if(*p & 0x80){
		*q++ = '\033';
		*q++ = '$';
		*q++ = 'B';
#ifdef _WINDOWS
		c1 = *p;
#else
		*q++ = (*p & 0x7f);
#endif
		inside_esc_seq = 1;
	    }
	    else{
		*q++ = (*p);
	    }
	}
    }

    if(inside_esc_seq){
	*q++ = '\033';
	*q++ = '(';
	*q++ = 'B';
    }

    *q = '\0';

    return(rv);
}


/*
 * Copies the source string into allocated space with the ISO-2022-JP
 * converted into 8-bit EUC codes (on Unix) or into Shift-JIS (on PC).
 * Caller is responsible for freeing the result.
 *
 * If limit is a NULL pointer, we consider all of src until a null terminator.
 * If it is not NULL, then the length of the returned string will be no
 * more than limit bytes.
 */
unsigned char *
trans_2022_jp_to_euc(src, limit)
    unsigned char *src;
    unsigned int  *limit;
{
    size_t len;
    unsigned char *rv, *p, *q, c;
    int    c1 = -1;		/* remembers first of pair for Shift-JIS */
#define DFL	0
#define ESC	1	/* saw ESCAPE */
#define ESCDOL	2	/* saw ESCAPE $ */
#define ESCPAR	3	/* saw ESCAPE ( */
#define EUC	4	/* filtering into EUC */
    int    state = DFL;

    if(!src)
      return(NULL);
    
    if(F_ON(F_DISABLE_2022_JP_CONVERSIONS, ps_global)){
	unsigned char *d;

	if(limit && strlen(src) > (*limit) - 1){
	    d = (unsigned char *) fs_get((*limit + 1) * sizeof(char));
	    strncpy((char *) d, src, *limit);
	    d[*limit] = '\0';
	}
	else
	  d = (unsigned char *) cpystr((char *) src);

	return(d);
    }

    len = strlen((char *) src);
    if(limit)
      len = (*limit);

    /* 5 there so we don't have to worry about all the *q++'s */ 
    rv = (unsigned char *) fs_get((len + 5 + 1) * sizeof(char));

    /*
     * The state machine is dumb because it is copied from the same
     * state machine in gf_2022_jp_to_euc where we only have access to
     * one character at a time with no lookahead. Obviously, we could
     * look ahead here, but why make it different?
     */
    for(p = src, q = rv; *p && (!limit || (q-rv) < len); p++){
	switch(state){
	  case ESC:				/* saw ESC */
	    if(*p == '$')
	      state = ESCDOL;
	    else if(*p == '(')
	      state = ESCPAR;
	    else{
		*q++ = '\033';
		*q++ = (*p);
		state = DFL;
	    }

	    break;

	  case ESCDOL:			/* saw ESC $ */
	    if(*p == 'B' || *p == '@'){
		state = EUC;
		c1 = -1;			/* first character of pair */
	    }
	    else{
		*q++ = '\033';
		*q++ = '$';
		*q++ = (*p);
		state = DFL;
	    }

	    break;

	  case ESCPAR:			/* saw ESC ( */
	    /*
	     * Mark suggests that ESC ( anychar should be treated as an
	     * end of the escape sequence (as opposed to just ESC ( B or
	     * ESC ( @). So that's what we'll do. We know it's not quite
	     * correct, but it shouldn't come up in real life.
	     * Hubert 2004-12-07
	     */
	    state = DFL;
	    break;

	  case EUC:				/* filtering into euc */
	    if(*p == '\033')
	      state = ESC;
	    else{
#ifdef _WINDOWS					/* Shift-JIS */
		c = (*p) & 0x7f;		/* 8-bit can't win */
		if(c1 >= 0){			/* second of a pair? */
		    int rowOffset = (c1 < 95) ? 112 : 176;
		    int cellOffset = (c1 % 2) ? ((c > 95) ? 32 : 31)
					      : 126;

		    *q++ = ((c1 + 1) >> 1) + rowOffset;
		    *q++ = c + cellOffset;
		    c1 = -1;			/* restart */
		}
		else if(c > 0x20 && c < 0x7f)
		  c1 = c;			/* first of pair */
		else{
		    *q++ = c;			/* write CTL as itself */
		    c1 = -1;
		}
#else						/* EUC */
		*q++ = (*p > 0x20 && *p < 0x7f) ? *p | 0x80 : *p;
#endif
	    }

	    break;

	  case DFL:
	  default:
	    if(*p == '\033')
	      state = ESC;
	    else
	      *q++ = (*p);

	    break;
	}
    }

    switch(state){
      case ESC:
	*q++ = '\033';
	break;

      case ESCDOL:
	*q++ = '\033';
	*q++ = '$';
	break;

      case ESCPAR:
	*q++ = '\033';		/* Don't set hibit for */
	*q++ = '(';		/* escape sequences.   */
	break;
    }

    *q = '\0';

    if(limit && *limit < q-rv)
      rv[*limit] = '\0';

    return(rv);
}


char *xdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};

char *
month_abbrev(month_num)
     int month_num;
{
    static char *xmonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
    if(month_num < 1 || month_num > 12)
      return("xxx");
    return(xmonths[month_num - 1]);
}

char *
month_name(month_num)
     int month_num;
{
    static char *months[] = {"January", "February", "March", "April",
		"May", "June", "July", "August", "September", "October",
		"November", "December", NULL};
    if(month_num < 1 || month_num > 12)
      return("");
    return(months[month_num - 1]);
}

char *
week_abbrev(week_day)
     int week_day;
{
    if(week_day < 0 || week_day > 6)
      return("???");
    return(xdays[week_day]);
}


/*----------------------------------------------------------------------
      Return month number of month named in string
  
   Args: s -- string with 3 letter month abbreviation of form mmm-yyyy
 
 Result: Returns month number with January, year 1900, 2000... being 0;
         -1 if no month/year is matched
 ----*/
int
month_num(s)
     char *s;
{
    int month = -1, year;
    int i;

    if(F_ON(F_PRUNE_USES_ISO,ps_global)){
	char save, *p;
	char digmon[3];

	if(s && strlen(s) > 4 && s[4] == '-'){
	    save = s[4];
	    s[4] = '\0';
	    year = atoi(s);
	    s[4] = save;
	    if(year == 0)
	      return(-1);
	    
	    p = s + 5;
	    for(i = 0; i < 12; i++){
		digmon[0] = ((i+1) < 10) ? '0' : '1';
		digmon[1] = '0' + (i+1) % 10;
		digmon[2] = '\0';
		if(strcmp(digmon, p) == 0)
		  break;
	    }

	    if(i == 12)
	      return(-1);
	    
	    month = year * 12 + i;
	}
    }
    else{
	if(s && strlen(s) > 3 && s[3] == '-'){
	    for(i = 0; i < 12; i++){
		if(struncmp(month_abbrev(i+1), s, 3) == 0)
		  break;
	    }

	    if(i == 12)
	      return(-1);

	    year = atoi(s + 4);
	    if(year == 0)
	      return(-1);

	    month = year * 12 + i;
	}
    }
    
    return(month);
}


/*
 * Structure containing all knowledge of symbolic time zones.
 * To add support for a given time zone, add it here, but make sure
 * the zone name is in upper case.
 */
static struct {
    char  *zone;
    short  len,
    	   hour_offset,
	   min_offset;
} known_zones[] = {
    {"PST", 3, -8, 0},			/* Pacific Standard */
    {"PDT", 3, -7, 0},			/* Pacific Daylight */
    {"MST", 3, -7, 0},			/* Mountain Standard */
    {"MDT", 3, -6, 0},			/* Mountain Daylight */
    {"CST", 3, -6, 0},			/* Central Standard */
    {"CDT", 3, -5, 0},			/* Central Daylight */
    {"EST", 3, -5, 0},			/* Eastern Standard */
    {"EDT", 3, -4, 0},			/* Eastern Daylight */
    {"JST", 3,  9, 0},			/* Japan Standard */
    {"GMT", 3,  0, 0},			/* Universal Time */
    {"UT",  2,  0, 0},			/* Universal Time */
#ifdef	IST_MEANS_ISREAL
    {"IST", 3,  2, 0},			/* Israel Standard */
#else
#ifdef	IST_MEANS_INDIA
    {"IST", 3,  5, 30},			/* India Standard */
#endif
#endif
    {NULL, 0, 0},
};

/*----------------------------------------------------------------------
  Parse date in or near RFC-822 format into the date structure

Args: given_date -- The input string to parse
      d          -- Pointer to a struct date to place the result in
 
Returns nothing

The following date fomrats are accepted:
  WKDAY DD MM YY HH:MM:SS ZZ
  DD MM YY HH:MM:SS ZZ
  WKDAY DD MM HH:MM:SS YY ZZ
  DD MM HH:MM:SS YY ZZ
  DD MM WKDAY HH:MM:SS YY ZZ
  DD MM WKDAY YY MM HH:MM:SS ZZ

All leading, intervening and trailing spaces tabs and commas are ignored.
The prefered formats are the first or second ones.  If a field is unparsable
it's value is left as -1. 

  ----*/
void
parse_date(given_date, d)
     char        *given_date;
     struct date *d;
{
    char *p, **i, *q, n;
    int   month;

    d->sec   = -1;
    d->minute= -1;
    d->hour  = -1;
    d->day   = -1;
    d->month = -1;
    d->year  = -1;
    d->wkday = -1;
    d->hours_off_gmt = -1;
    d->min_off_gmt   = -1;

    if(given_date == NULL)
      return;

    p = given_date;
    while(*p && isspace((unsigned char)*p))
      p++;

    /* Start with month, weekday or day ? */
    for(i = xdays; *i != NULL; i++) 
      if(struncmp(p, *i, 3) == 0) /* Match first 3 letters */
        break;

    if(*i != NULL) {
        /* Started with week day */
        d->wkday = i - xdays;
        while(*p && !isspace((unsigned char)*p) && *p != ',')
          p++;
        while(*p && (isspace((unsigned char)*p) || *p == ','))
          p++;
    }

    if(isdigit((unsigned char)*p)) {
        d->day = atoi(p);
        while(*p && isdigit((unsigned char)*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace((unsigned char)*p)))
          p++;
    }
    for(month = 1; month <= 12; month++)
      if(struncmp(p, month_abbrev(month), 3) == 0)
        break;
    if(month < 13) {
        d->month = month;

    } 
    /* Move over month, (or whatever is there) */
    while(*p && !isspace((unsigned char)*p) && *p != ',' && *p != '-')
       p++;
    while(*p && (isspace((unsigned char)*p) || *p == ',' || *p == '-'))
       p++;

    /* Check again for day */
    if(isdigit((unsigned char)*p) && d->day == -1) {
        d->day = atoi(p);
        while(*p && isdigit((unsigned char)*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace((unsigned char)*p)))
          p++;
    }

    /*-- Check for time --*/
    for(q = p; *q && isdigit((unsigned char)*q); q++);
    if(*q == ':') {
        /* It's the time (out of place) */
        d->hour = atoi(p);
        while(*p && *p != ':' && !isspace((unsigned char)*p))
          p++;
        if(*p == ':') {
            p++;
            d->minute = atoi(p);
            while(*p && *p != ':' && !isspace((unsigned char)*p))
              p++;
            if(*p == ':') {
                d->sec = atoi(p);
                while(*p && !isspace((unsigned char)*p))
                  p++;
            }
        }
        while(*p && isspace((unsigned char)*p))
          p++;
    }
    

    /* Get the year 0-49 is 2000-2049; 50-100 is 1950-1999 and
                                           101-9999 is 101-9999 */
    if(isdigit((unsigned char)*p)) {
        d->year = atoi(p);
        if(d->year < 50)   
          d->year += 2000;
        else if(d->year < 100)
          d->year += 1900;
        while(*p && isdigit((unsigned char)*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace((unsigned char)*p)))
          p++;
    } else {
        /* Something wierd, skip it and try to resynch */
        while(*p && !isspace((unsigned char)*p) && *p != ',' && *p != '-')
          p++;
        while(*p && (isspace((unsigned char)*p) || *p == ',' || *p == '-'))
          p++;
    }

    /*-- Now get hours minutes, seconds and ignore tenths --*/
    for(q = p; *q && isdigit((unsigned char)*q); q++);
    if(*q == ':' && d->hour == -1) {
        d->hour = atoi(p);
        while(*p && *p != ':' && !isspace((unsigned char)*p))
          p++;
        if(*p == ':') {
            p++;
            d->minute = atoi(p);
            while(*p && *p != ':' && !isspace((unsigned char)*p))
              p++;
            if(*p == ':') {
                p++;
                d->sec = atoi(p);
                while(*p && !isspace((unsigned char)*p))
                  p++;
            }
        }
    }
    while(*p && isspace((unsigned char)*p))
      p++;


    /*-- The time zone --*/
    d->hours_off_gmt = 0;
    d->min_off_gmt = 0;
    if(*p) {
        if((*p == '+' || *p == '-')
	   && isdigit((unsigned char)p[1])
	   && isdigit((unsigned char)p[2])
	   && isdigit((unsigned char)p[3])
	   && isdigit((unsigned char)p[4])
	   && !isdigit((unsigned char)p[5])) {
            char tmp[3];
            d->min_off_gmt = d->hours_off_gmt = (*p == '+' ? 1 : -1);
            p++;
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            d->hours_off_gmt *= atoi(tmp);
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            d->min_off_gmt *= atoi(tmp);
        } else {
	    for(n = 0; known_zones[n].zone; n++)
	      if(struncmp(p, known_zones[n].zone, known_zones[n].len) == 0){
		  d->hours_off_gmt = (int) known_zones[n].hour_offset;
		  d->min_off_gmt   = (int) known_zones[n].min_offset;
		  break;
	      }
        }
    }

    if(d->wkday == -1){
	MESSAGECACHE elt;
	struct tm   *tm;
	time_t       t;

	/*
	 * Not sure why we aren't just using this from the gitgo, but
	 * since not sure will just use it to repair wkday.
	 */
	if(mail_parse_date(&elt, (unsigned char *) given_date)){
	    t = mail_longdate(&elt);
	    tm = localtime(&t);

	    if(tm)
	      d->wkday = tm->tm_wday;
	}
    }
}



/*----------------------------------------------------------------------
     Map some of the special characters into sensible strings for human
   consumption.
  ----*/
char *
pretty_command(c)
     int c;
{
    static char  buf[10];
    char	*s;

    buf[0] = '\0';
    s = buf;

    switch(c){
      case '\033'    : s = "ESC";		break;
      case '\177'    : s = "DEL";		break;
      case ctrl('I') : s = "TAB";		break;
      case ctrl('J') : s = "LINEFEED";		break;
      case ctrl('M') : s = "RETURN";		break;
      case ctrl('Q') : s = "XON";		break;
      case ctrl('S') : s = "XOFF";		break;
      case KEY_UP    : s = "Up Arrow";		break;
      case KEY_DOWN  : s = "Down Arrow";	break;
      case KEY_RIGHT : s = "Right Arrow";	break;
      case KEY_LEFT  : s = "Left Arrow";	break;
      case KEY_PGUP  : s = "Prev Page";		break;
      case KEY_PGDN  : s = "Next Page";		break;
      case KEY_HOME  : s = "Home";		break;
      case KEY_END   : s = "End";		break;
      case KEY_DEL   : s = "Delete";		break; /* Not necessary DEL! */
      case PF1	     :
      case PF2	     :
      case PF3	     :
      case PF4	     :
      case PF5	     :
      case PF6	     :
      case PF7	     :
      case PF8	     :
      case PF9	     :
      case PF10	     :
      case PF11	     :
      case PF12	     :
        sprintf(s = buf, "F%d", c - PF1 + 1);
	break;

      default:
	if(c < ' ')
	  sprintf(s = buf, "^%c", c + 'A' - 1);
	else
	  sprintf(s = buf, "%c", c);

	break;
    }

    return(s);
}
        
    

/*----------------------------------------------------------------------
     Create a little string of blanks of the specified length.
   Max n is 511.
  ----*/
char *
repeat_char(n, c)
     int  n;
     int  c;
{
    static char bb[512];
    if(n > sizeof(bb))
       n = sizeof(bb) - 1;
    bb[n--] = '\0';
    while(n >= 0)
      bb[n--] = c;
    return(bb);
}


/*----------------------------------------------------------------------
        Turn a number into a string with comma's

   Args: number -- The long to be turned into a string. 

  Result: pointer to static string representing number with commas
  ---*/
char *
comatose(number) 
    long number;
{
#ifdef	DOS
    static char buf[3][16];
    static int whichbuf = 0;
    char *b;
    short i;

    if(!number)
	return("0");

    whichbuf = (whichbuf + 1) % 3;

    if(number < 0x7FFFFFFFL){		/* largest DOS signed long */
        buf[whichbuf][15] = '\0';
        b = &buf[whichbuf][14];
        i = 2;
	while(number){
 	    *b-- = (number%10) + '0';
	    if((number /= 10) && i-- == 0 ){
		*b-- = ',';
		i = 2;
	    }
	}
    }
    else
      return("Number too big!");		/* just fits! */

    return(++b);
#else
    long        i, x, done_one;
    static char buf[3][50];
    static int whichbuf = 0;
    char       *b;

    whichbuf = (whichbuf + 1) % 3;
    if(number == 0){
        strcpy(buf[whichbuf], "0");
        return(buf[whichbuf]);
    }
    
    done_one = 0;
    b = buf[whichbuf];
    for(i = 1000000000; i >= 1; i /= 1000) {
	x = number / i;
	number = number % i;
	if(x != 0 || done_one) {
	    if(b != buf[whichbuf])
	      *b++ = ',';
	    sprintf(b, done_one ? "%03ld" : "%d", x);
	    b += strlen(b);
	    done_one = 1;
	}
    }
    *b = '\0';

    return(buf[whichbuf]);
#endif	/* DOS */
}



/*----------------------------------------------------------------------
   Format number as amount of bytes, appending Kb, Mb, Gb, bytes

  Args: bytes -- number of bytes to format

 Returns pointer to static string. The numbers are divided to produce a 
nice string with precision of about 2-4 digits
    ----*/
char *
byte_string(bytes)
     long bytes;
{
    char       *a, aa[5];
    char       *abbrevs = "GMK";
    long        i, ones, tenths;
    static char string[10];

    ones   = 0L;
    tenths = 0L;

    if(bytes == 0L){
        strcpy(string, "0 bytes");
    } else {
        for(a = abbrevs, i = 1000000000; i >= 1; i /= 1000, a++) {
            if(bytes > i) {
                ones = bytes/i;
                if(ones < 10L && i > 10L)
                  tenths = (bytes - (ones * i)) / (i / 10L);
                break;
            }
        }
    
        aa[0] = *a;  aa[1] = '\0'; 
    
        if(tenths == 0)
          sprintf(string, "%ld%s%s", ones, aa, *a ? "B" : "bytes");
        else
          sprintf(string, "%ld.%ld%s%s", ones, tenths, aa, *a ? "B" : "bytes");
    }

    return(string);
}



/*----------------------------------------------------------------------
    Print a string corresponding to the number given:
      1st, 2nd, 3rd, 105th, 92342nd....
 ----*/

char *
enth_string(i)
     int i;
{
    static char enth[10];

    enth[0] = '\0';

    switch (i % 10) {
        
      case 1:
        if( (i % 100 ) == 11)
          sprintf(enth,"%dth", i);
        else
          sprintf(enth,"%dst", i);
        break;

      case 2:
        if ((i % 100) == 12)
          sprintf(enth, "%dth",i);
        else
          sprintf(enth, "%dnd",i);
        break;

      case 3:
        if(( i % 100) == 13)
          sprintf(enth, "%dth",i);
        else
          sprintf(enth, "%drd",i);
        break;

      default:
        sprintf(enth,"%dth",i);
        break;
    }
    return(enth);
}


/*
 * Inserts newlines for folding at whitespace.
 *
 * Args          src -- The source text.
 *             width -- Approximately where the fold should happen.
 *          maxwidth -- Maximum width we want to fold at.
 *                cr -- End of line is \r\n instead of just \n.
 *       preserve_ws -- Preserve whitespace when folding. This is for vcard
 *                       folding where CRLF SPACE is removed when unfolding, so
 *                       we need to leave the space in. With rfc822 unfolding
 *                       only the CRLF is removed when unfolding.
 *      first_indent -- String to use as indent on first line.
 *            indent -- String to use as indent for subsequent folded lines.
 *
 * Returns   An allocated string which caller should free.
 */
char *
fold(src, width, maxwidth, cr, preserve_ws, first_indent, indent)
    char *src;
    int   width,
	  maxwidth,
	  cr,
	  preserve_ws;
    char *first_indent,
	 *indent;
{
    char *next_piece, *res, *p;
    int   i, len, nb, starting_point, shorter, longer, winner, eol;
    int   indent1 = 0, indent2 = 0;
    char  save_char;

    if(indent)
      indent2 = strlen(indent);

    if(first_indent)
      indent1 = strlen(first_indent);

    nb = indent1;
    len = indent1;
    next_piece = src;
    eol = cr ? 2 : 1;
    if(!src || !*src)
      nb += eol;

    /*
     * We can't tell how much space is going to be needed without actually
     * passing through the data to see.
     */
    while(next_piece && *next_piece){
	if(next_piece != src && indent2){
	    len += indent2;
	    nb += indent2;
	}

	if(strlen(next_piece) + len <= width){
	    nb += (strlen(next_piece) + eol);
	    break;
	}
	else{ /* fold it */
	    starting_point = width - len;	/* space left on this line */
	    /* find a good folding spot */
	    winner = -1;
	    for(i = 0;
		winner == -1
		  && (starting_point - i > len + 8
		      || starting_point + i < maxwidth - width);
		i++){

		if((shorter=starting_point-i) > len + 8
		   && isspace((unsigned char)next_piece[shorter]))
		  winner = shorter;
		else if((longer=starting_point+i) < maxwidth - width
			&& (!next_piece[longer]
		            || isspace((unsigned char)next_piece[longer])))
		  winner = longer;
	    }

	    if(winner == -1) /* if no good folding spot, fold at width */
	      winner = starting_point;
	    
	    nb += (winner + eol);
	    next_piece += winner;
	    if(!preserve_ws && isspace((unsigned char)next_piece[0]))
	      next_piece++;
	}

	len = 0;
    }

    res = (char *)fs_get((nb+1) * sizeof(char));
    p = res;
    sstrcpy(&p, first_indent);
    len = indent1;
    next_piece = src;

    while(next_piece && *next_piece){
	if(next_piece != src && indent2){
	    sstrcpy(&p, indent);
	    len += indent2;
	}

	if(strlen(next_piece) + len <= width){
	    sstrcpy(&p, next_piece);
	    if(cr)
	      *p++ = '\r';

	    *p++ = '\n';
	    break;
	}
	else{ /* fold it */
	    starting_point = width - len;	/* space left on this line */
	    /* find a good folding spot */
	    winner = -1;
	    for(i = 0;
		winner == -1
		  && (starting_point - i > len + 8
		      || starting_point + i < maxwidth - width);
		i++){

		if((shorter=starting_point-i) > len + 8
		   && isspace((unsigned char)next_piece[shorter]))
		  winner = shorter;
		else if((longer=starting_point+i) < maxwidth - width
			&& (!next_piece[longer]
		            || isspace((unsigned char)next_piece[longer])))
		  winner = longer;
	    }

	    if(winner == -1) /* if no good folding spot, fold at width */
	      winner = starting_point;
	    
	    save_char = next_piece[winner];
	    next_piece[winner] = '\0';
	    sstrcpy(&p, next_piece);
	    if(cr)
	      *p++ = '\r';

	    *p++ = '\n';
	    next_piece[winner] = save_char;
	    next_piece += winner;
	    if(!preserve_ws && isspace((unsigned char)next_piece[0]))
	      next_piece++;
	}

	len = 0;
    }

    if(!src || !*src){
	if(cr)
	  *p++ = '\r';

	*p++ = '\n';
    }

    *p = '\0';

    return(res);
}


/*
 * strsquish - fancifies a string into the given buffer if it's too
 *	       long to fit in the given width
 */
char *
strsquish(buf, s, width)
    char *buf, *s;
    int   width;
{
    int i, offset;

    if(width > 0){
	if((i = strlen(s)) <= width)
	  return(s);

	if(width > 14){
	    strncpy(buf, s, offset = ((width / 2) - 2));
	    strcpy(buf + offset, "...");
	    offset += 3;
	}
	else if(width > 3){
	    strcpy(buf, "...");
	    offset = 3;
	}
	else
	  offset = 0;

	strcpy(buf + offset, s + (i - width) + offset);
	return(buf);
    }
    else
      return("");
}


char *
long2string(l)
     long l;
{
    static char string[20];
    sprintf(string, "%ld", l);
    return(string);
}

char *
int2string(i)
     int i;
{
    static char string[20];
    sprintf(string, "%d", i);
    return(string);
}


/*
 * strtoval - convert the given string to a positive integer.
 */
char *
strtoval(s, val, minmum, maxmum, otherok, errbuf, varname)
    char *s;
    int  *val;
    int   minmum, maxmum, otherok;
    char *errbuf, *varname;
{
    int   i = 0, neg = 1;
    char *p = s, *errstr = NULL;

    removing_leading_and_trailing_white_space(p);
    for(; *p; p++)
      if(isdigit((unsigned char) *p)){
	  i = (i * 10) + (*p - '0');
      }
      else if(*p == '-' && i == 0){
	  neg = -1;
      }
      else{
	  sprintf(errstr = errbuf,
		  "Non-numeric value ('%c' in \"%.8s\") in %s. Using \"%d\"",
		  *p, s, varname, *val);
	  return(errbuf);
      }

    i *= neg;

    /* range describes acceptable values */
    if(maxmum > minmum && (i < minmum || i > maxmum) && i != otherok)
      sprintf(errstr = errbuf,
	      "%s of %d not supported (M%s %d). Using \"%d\"",
	      varname, i, (i > maxmum) ? "ax" : "in",
	      (i > maxmum) ? maxmum : minmum, *val);
    /* range describes unacceptable values */
    else if(minmum > maxmum && !(i < maxmum || i > minmum))
      sprintf(errstr = errbuf, "%s of %d not supported. Using \"%d\"",
	      varname, i, *val);
    else
      *val = i;

    return(errstr);
}


/*
 * strtolval - convert the given string to a positive _long_ integer.
 */
char *
strtolval(s, val, minmum, maxmum, otherok, errbuf, varname)
    char *s;
    long *val;
    long  minmum, maxmum, otherok;
    char *errbuf, *varname;
{
    long  i = 0, neg = 1L;
    char *p = s, *errstr = NULL;

    removing_leading_and_trailing_white_space(p);
    for(; *p; p++)
      if(isdigit((unsigned char) *p)){
	  i = (i * 10L) + (*p - '0');
      }
      else if(*p == '-' && i == 0L){
	  neg = -1L;
      }
      else{
	  sprintf(errstr = errbuf,
		  "Non-numeric value ('%c' in \"%.8s\") in %s. Using \"%ld\"",
		  *p, s, varname, *val);
	  return(errbuf);
      }

    i *= neg;

    /* range describes acceptable values */
    if(maxmum > minmum && (i < minmum || i > maxmum) && i != otherok)
      sprintf(errstr = errbuf,
	      "%s of %ld not supported (M%s %ld). Using \"%ld\"",
	      varname, i, (i > maxmum) ? "ax" : "in",
	      (i > maxmum) ? maxmum : minmum, *val);
    /* range describes unacceptable values */
    else if(minmum > maxmum && !(i < maxmum || i > minmum))
      sprintf(errstr = errbuf, "%s of %ld not supported. Using \"%ld\"",
	      varname, i, *val);
    else
      *val = i;

    return(errstr);
}


/*
 *  Function to parse the given string into two space-delimited fields
 *  Quotes may be used to surround labels or values with spaces in them.
 *  Backslash negates the special meaning of a quote.
 *  Unescaping of backslashes only happens if the pair member is quoted,
 *    this provides for backwards compatibility.
 *
 * Args -- string -- the source string
 *          label -- the first half of the string, a return value
 *          value -- the last half of the string, a return value
 *        firstws -- if set, the halves are delimited by the first unquoted
 *                    whitespace, else by the last unquoted whitespace
 *   strip_internal_label_quotes -- unescaped quotes in the middle of the label
 *                                   are removed. This is useful for vars
 *                                   like display-filters and url-viewers
 *                                   which may require quoting of an arg
 *                                   inside of a _TOKEN_.
 */
void
get_pair(string, label, value, firstws, strip_internal_label_quotes)
    char *string, **label, **value;
    int   firstws;
    int   strip_internal_label_quotes;
{
    char *p, *q, *tmp, *token = NULL;
    int	  quoted = 0;

    *label = *value = NULL;

    /*
     * This for loop just finds the beginning of the value. If firstws
     * is set, then it begins after the first whitespace. Otherwise, it begins
     * after the last whitespace. Quoted whitespace doesn't count as
     * whitespace. If there is no unquoted whitespace, then there is no
     * label, there's just a value.
     */
    for(p = string; p && *p;){
	if(*p == '"')				/* quoted label? */
	  quoted = (quoted) ? 0 : 1;

	if(*p == '\\' && *(p+1) == '"')		/* escaped quote? */
	  p++;					/* skip it... */

	if(isspace((unsigned char)*p) && !quoted){	/* if space,  */
	    while(*++p && isspace((unsigned char)*p))	/* move past it */
	      ;

	    if(!firstws || !token)
	      token = p;			/* remember start of text */
	}
	else
	  p++;
    }

    if(token){					/* copy label */
	*label = p = (char *)fs_get(((token - string) + 1) * sizeof(char));

	/* make a copy of the string */
	tmp = (char *)fs_get(((token - string) + 1) * sizeof(char));
	strncpy(tmp, string, token - string);
	tmp[token-string] = '\0';

	removing_leading_and_trailing_white_space(tmp);
	quoted = removing_double_quotes(tmp);
	
	for(q = tmp; *q; q++){
	    if(quoted && *q == '\\' && (*(q+1) == '"' || *(q+1) == '\\'))
	      *p++ = *++q;
	    else if(!(strip_internal_label_quotes && *q == '"'))
	      *p++ = *q;
	}

	*p = '\0';				/* tie off label */
	fs_give((void **)&tmp);
	if(*label == '\0')
	  fs_give((void **)label);
    }
    else
      token = string;

    if(token){					/* copy value */
	*value = p = (char *)fs_get((strlen(token) + 1) * sizeof(char));

	tmp = cpystr(token);
	removing_leading_and_trailing_white_space(tmp);
	quoted = removing_double_quotes(tmp);

	for(q = tmp; *q ; q++){
	    if(quoted && *q == '\\' && (*(q+1) == '"' || *(q+1) == '\\'))
	      *p++ = *++q;
	    else
	      *p++ = *q;
	}

	*p = '\0';				/* tie off value */
	fs_give((void **)&tmp);
    }
}


/*
 *  This is sort of the inverse of get_pair.
 *
 * Args --  label -- the first half of the string
 *          value -- the last half of the string
 *
 * Returns -- an allocated string which is "label" SPACE "value"
 *
 *  Label and value are quoted separately. If quoting is needed (they contain
 *  whitespace) then backslash escaping is done inside the quotes for
 *  " and for \. If quoting is not needed, no escaping is done.
 */
char *
put_pair(label, value)
    char *label, *value;
{
    char *result, *lab = label, *val = value;

    if(label && *label)
      lab = quote_if_needed(label);

    if(value && *value)
      val = quote_if_needed(value);

    result = (char *)fs_get((strlen(lab) + strlen(val) +1 +1) * sizeof(char));
    
    sprintf(result, "%s%s%s",
	    lab ? lab : "",
	    (lab && lab[0] && val && val[0]) ? " " : "",
	    val ? val : "");

    if(lab && lab != label)
      fs_give((void **)&lab);
    if(val && val != value)
      fs_give((void **)&val);

    return(result);
}


/*
 * This is for put_pair type uses. It returns either an allocated
 * string which is the quoted src string or it returns a pointer to
 * the src string if no quoting is needed.
 */
char *
quote_if_needed(src)
    char *src;
{
    char *result = src, *qsrc = NULL;

    if(src && *src){
	/* need quoting? */
	if(strpbrk(src, " \t") != NULL)
	  qsrc = add_escapes(src, "\\\"", '\\', "", "");

	if(qsrc && !*qsrc)
	  fs_give((void **)&qsrc);

	if(qsrc){
	    result = (char *)fs_get((strlen(qsrc)+2+1) * sizeof(char));
	    sprintf(result, "\"%s\"", qsrc);
	    fs_give((void **)&qsrc);
	}
    }

    return(result);
}


/*
 * Convert a 1, 2, or 3-digit octal string into an 8-bit character.
 * Only the first three characters of s will be used, and it is ok not
 * to null-terminate it.
 */
int
read_octal(s)
    char **s;
{
    register int i, j;

    i = 0;
    for(j = 0; j < 3 && **s >= '0' && **s < '8' ; (*s)++, j++)
      i = (i * 8) + (int)(unsigned char)**s - '0';

    return(i);
}


/*
 * Convert two consecutive HEX digits to an integer.  First two
 * chars pointed to by "s" MUST already be tested for hexness.
 */
int
read_hex(s)
    char *s;
{
    return(X2C(s));
}


/*
 * Given a character c, put the 3-digit ascii octal value of that char
 * in the 2nd argument, which must be at least 3 in length.
 */
void
char_to_octal_triple(c, octal)
    int   c;
    char *octal;
{
    c &= 0xff;

    octal[2] = (c % 8) + '0';
    c /= 8;
    octal[1] = (c % 8) + '0';
    c /= 8;
    octal[0] = c + '0';
}


/*
 * Convert in memory string s to a C-style string, with backslash escapes
 * like they're used in C character constants.
 * Also convert leading spaces because read_pinerc deletes those
 * if not quoted.
 *
 * Returns allocated C string version of s.
 */
char *
string_to_cstring(s)
    char *s;
{
    char *b, *p;
    int   n, i, all_space_so_far = 1;

    if(!s)
      return(cpystr(""));

    n = 20;
    b = (char *)fs_get((n+1) * sizeof(char));
    p  = b;
    *p = '\0';
    i  = 0;

    while(*s){
	if(*s != SPACE)
	  all_space_so_far = 0;

	if(i + 4 > n){
	    /*
	     * The output string may overflow the output buffer.
	     * Make more room.
	     */
	    n += 20;
	    fs_resize((void **)&b, (n+1) * sizeof(char));
	    p = &b[i];
	}
	else{
	    switch(*s){
	      case '\n':
		*p++ = '\\';
		*p++ = 'n';
		i += 2;
		break;

	      case '\r':
		*p++ = '\\';
		*p++ = 'r';
		i += 2;
		break;

	      case '\t':
		*p++ = '\\';
		*p++ = 't';
		i += 2;
		break;

	      case '\b':
		*p++ = '\\';
		*p++ = 'b';
		i += 2;
		break;

	      case '\f':
		*p++ = '\\';
		*p++ = 'f';
		i += 2;
		break;

	      case '\\':
		*p++ = '\\';
		*p++ = '\\';
		i += 2;
		break;

	      case SPACE:
		if(all_space_so_far){	/* use octal output */
		    *p++ = '\\';
		    char_to_octal_triple(*s, p);
		    p += 3;
		    i += 4;
		    break;
		}
		else{
		    /* fall through */
		}


	      default:
		if(*s >= SPACE && *s < '~' && *s != '\"' && *s != '$'){
		    *p++ = *s;
		    i++;
		}
		else{  /* use octal output */
		    *p++ = '\\';
		    char_to_octal_triple(*s, p);
		    p += 3;
		    i += 4;
		}

		break;
	    }

	    s++;
	}
    }

    *p = '\0';
    return(b);
}


/*
 * Convert C-style string, with backslash escapes, into a hex string, two
 * hex digits per character.
 *
 * Returns allocated hexstring version of s.
 */
char *
cstring_to_hexstring(s)
    char *s;
{
    char *b, *p;
    int   n, i, c;

    if(!s)
      return(cpystr(""));

    n = 20;
    b = (char *)fs_get((n+1) * sizeof(char));
    p  = b;
    *p = '\0';
    i  = 0;

    while(*s){
	if(i + 2 > n){
	    /*
	     * The output string may overflow the output buffer.
	     * Make more room.
	     */
	    n += 20;
	    fs_resize((void **)&b, (n+1) * sizeof(char));
	    p = &b[i];
	}
	else{
	    if(*s == '\\'){
		s++;
		switch(*s){
		  case 'n':
		    c = '\n';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'r':
		    c = '\r';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 't':
		    c = '\t';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'v':
		    c = '\v';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'b':
		    c = '\b';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'f':
		    c = '\f';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'a':
		    c = '\007';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case '\\':
		    c = '\\';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case '?':
		    c = '?';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case '\'':
		    c = '\'';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case '\"':
		    c = '\"';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 0: /* reached end of s too early */
		    c = 0;
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  /* hex number */
		  case 'x':
		    s++;
		    if(isxpair(s)){
			c = X2C(s);
			s += 2;
		    }
		    else if(isxdigit((unsigned char)*s)){
			c = XDIGIT2C(*s);
			s++;
		    }
		    else
		      c = 0;

		    C2XPAIR(c, p);
		    i += 2;

		    break;

		  /* octal number */
		  default:
		    c = read_octal(&s);
		    C2XPAIR(c, p);
		    i += 2;

		    break;
		}
	    }
	    else{
		C2XPAIR(*s, p);
		i += 2;
		s++;
	    }
	}
    }

    *p = '\0';
    return(b);
}


/*
 * Convert C-style string, with backslash escapes, into a regular string.
 * Result goes in dst, which should be as big as src.
 *
 */
void
cstring_to_string(src, dst)
    char *src;
    char *dst;
{
    char *p;
    int   c;

    dst[0] = '\0';
    if(!src)
      return;

    p  = dst;

    while(*src){
	if(*src == '\\'){
	    src++;
	    switch(*src){
	      case 'n':
		*p++ = '\n';
		src++;
		break;

	      case 'r':
		*p++ = '\r';
		src++;
		break;

	      case 't':
		*p++ = '\t';
		src++;
		break;

	      case 'v':
		*p++ = '\v';
		src++;
		break;

	      case 'b':
		*p++ = '\b';
		src++;
		break;

	      case 'f':
		*p++ = '\f';
		src++;
		break;

	      case 'a':
		*p++ = '\007';
		src++;
		break;

	      case '\\':
		*p++ = '\\';
		src++;
		break;

	      case '?':
		*p++ = '?';
		src++;
		break;

	      case '\'':
		*p++ = '\'';
		src++;
		break;

	      case '\"':
		*p++ = '\"';
		src++;
		break;

	      case 0: /* reached end of s too early */
		src++;
		break;

	      /* hex number */
	      case 'x':
		src++;
		if(isxpair(src)){
		    c = X2C(src);
		    src += 2;
		}
		else if(isxdigit((unsigned char)*src)){
		    c = XDIGIT2C(*src);
		    src++;
		}
		else
		  c = 0;

		*p++ = c;

		break;

	      /* octal number */
	      default:
		c = read_octal(&src);
		*p++ = c;
		break;
	    }
	}
	else
	  *p++ = *src++;
    }

    *p = '\0';
}


/*
 * Quotes /'s and \'s with \
 *
 * Args: src -- The source string.
 *
 * Returns: A string with backslash quoting added. Any / in the string is
 *          replaced with \/ and any \ is replaced with \\, and any
 *          " is replaced with \".
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
add_backslash_escapes(src)
    char *src;
{
    return(add_escapes(src, "/\\\"", '\\', "", ""));
}


/*
 * Does pattern quoting. Takes the string that the user sees and converts
 * it to the config file string.
 *
 * Args: src -- The source string.
 *
 * The last arg to add_escapes causes \, and \\ to be replaced with hex
 * versions of comma and backslash. That's so we can embed commas in
 * list variables without having them act as separators. If the user wants
 * a literal comma, they type backslash comma.
 * If /, \, or " appear (other than the special cases in previous sentence)
 * they are backslash-escaped like \/, \\, or \".
 *
 * Returns: An allocated string with proper necessary added.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
add_pat_escapes(src)
    char *src;
{
    return(add_escapes(src, "/\\\"", '\\', "", ",\\"));
}


/*
 * This takes envelope data and adds the backslash escapes that the user
 * would have been responsible for adding if editing manually.
 * It just escapes commas and backslashes.
 *
 * Caller must free result.
 */
char *
add_roletake_escapes(src)
    char *src;
{
    return(add_escapes(src, ",\\", '\\', "", ""));
}

/*
 * This function only escapes commas.
 */
char *
add_comma_escapes(src)
     char *src;
{
    return(add_escapes(src, ",", '\\', "", ""));
}

/*
 * Quote values for viewer-hdr-colors. We quote backslash, comma, and slash.
 *  Also replaces $ with $$.
 *
 * Args: src -- The source string.
 *
 * Returns: A string with backslash quoting added.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
add_viewerhdr_escapes(src)
    char *src;
{
    char *tmp, *ans = NULL;

    tmp = add_escapes(src, "/\\", '\\', ",", "");

    if(tmp){
	ans = dollar_escape_dollars(tmp);
	fs_give((void **) &tmp);
    }

    return(ans);
}


/*
 * Quote dollar sign by preceding it with another dollar sign. We use $$
 * instead of \$ so that it will work for both PC-Pine and unix.
 *
 * Args: src -- The source string.
 *
 * Returns: A string with $$ quoting added.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
dollar_escape_dollars(src)
    char *src;
{
    return(add_escapes(src, "$", '$', "", ""));
}


/*
 * This adds the quoting for vcard backslash quoting.
 * That is, commas are backslashed, backslashes are backslashed, 
 * semicolons are backslashed, and CRLFs are \n'd.
 * This is thought to be correct for draft-ietf-asid-mime-vcard-06.txt, Apr 98.
 */
char *
vcard_escape(src)
    char *src;
{
    char *p, *q;

    q = add_escapes(src, ";,\\", '\\', "", "");
    if(q){
	/* now do CRLF -> \n in place */
	for(p = q; *p != '\0'; p++)
	  if(*p == '\r' && *(p+1) == '\n'){
	      *p++ = '\\';
	      *p = 'n';
	  }
    }

    return(q);
}


/*
 * This undoes the vcard backslash quoting.
 *
 * In particular, it turns \n into newline, \, into ',', \\ into \, \; -> ;.
 * In fact, \<anything_else> is also turned into <anything_else>. The ID
 * isn't clear on this.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
vcard_unescape(src)
    char *src;
{
    char *ans = NULL, *p;
    int done = 0;

    if(src){
	p = ans = (char *)fs_get(strlen(src) + 1);

	while(!done){
	    switch(*src){
	      case '\\':
		src++;
		if(*src == 'n' || *src == 'N'){
		    *p++ = '\n';
		    src++;
		}
		else if(*src)
		  *p++ = *src++;

		break;
	    
	      case '\0':
		done++;
		break;

	      default:
		*p++ = *src++;
		break;
	    }
	}

	*p = '\0';
    }

    return(ans);
}


/*
 * Turn folded lines into long lines in place.
 *
 * CRLF whitespace sequences are removed, the space is not preserved.
 */
void
vcard_unfold(string)
    char *string;
{
    char *p = string;

    while(*string)		      /* while something to copy  */
      if(*string == '\r' &&
         *(string+1) == '\n' &&
	 (*(string+2) == SPACE || *(string+2) == TAB))
	string += 3;
      else
	*p++ = *string++;
    
    *p = '\0';
}


/*
 * Quote specified chars with escape char.
 *
 * Args:          src -- The source string.
 *  quote_these_chars -- Array of chars to quote
 *       quoting_char -- The quoting char to be used (e.g., \)
 *    hex_these_chars -- Array of chars to hex escape
 *    hex_these_quoted_chars -- Array of chars to hex escape if they are
 *                              already quoted with quoting_char (that is,
 *                              turn \, into hex comma)
 *
 * Returns: An allocated copy of string with quoting added.
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
add_escapes(src, quote_these_chars, quoting_char, hex_these_chars,
	    hex_these_quoted_chars)
    char *src;
    char *quote_these_chars;
    int   quoting_char;
    char *hex_these_chars;
    char *hex_these_quoted_chars;
{
    char *ans = NULL;

    if(!quote_these_chars)
      panic("bad arg to add_escapes");

    if(src){
	char *q, *p, *qchar;

	p = q = (char *)fs_get(2*strlen(src) + 1);

	while(*src){
	    if(*src == quoting_char)
	      for(qchar = hex_these_quoted_chars; *qchar != '\0'; qchar++)
		if(*(src+1) == *qchar)
		  break;

	    if(*src == quoting_char && *qchar){
		src++;	/* skip quoting_char */
		*p++ = '\\';
		*p++ = 'x';
		C2XPAIR(*src, p);
		src++;	/* skip quoted char */
	    }
	    else{
		for(qchar = quote_these_chars; *qchar != '\0'; qchar++)
		  if(*src == *qchar)
		    break;

		if(*qchar){		/* *src is a char to be quoted */
		    *p++ = quoting_char;
		    *p++ = *src++;
		}
		else{
		    for(qchar = hex_these_chars; *qchar != '\0'; qchar++)
		      if(*src == *qchar)
			break;

		    if(*qchar){		/* *src is a char to be escaped */
			*p++ = '\\';
			*p++ = 'x';
			C2XPAIR(*src, p);
			src++;
		    }
		    else			/* a regular char */
		      *p++ = *src++;
		}
	    }

	}

	*p = '\0';

	ans = cpystr(q);
	fs_give((void **)&q);
    }

    return(ans);
}


/*
 * Undoes backslash quoting of source string.
 *
 * Args: src -- The source string.
 *
 * Returns: A string with backslash quoting removed or NULL. The string starts
 *          at src and goes until the end of src or until a / is reached. The
 *          / is not included in the string. /'s may be quoted by preceding
 *          them with a backslash (\) and \'s may also be quoted by
 *          preceding them with a \. In fact, \ quotes any character.
 *          Not quite, \nnn is octal escape, \xXX is hex escape.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
remove_backslash_escapes(src)
    char *src;
{
    char *ans = NULL, *q, *p;
    int done = 0;

    if(src){
	p = q = (char *)fs_get(strlen(src) + 1);

	while(!done){
	    switch(*src){
	      case '\\':
		src++;
		if(*src){
		    if(isdigit((unsigned char)*src))
		      *p++ = (char)read_octal(&src);
		    else if((*src == 'x' || *src == 'X') &&
			    *(src+1) && *(src+2) && isxpair(src+1)){
			*p++ = (char)read_hex(src+1);
			src += 3;
		    }
		    else
		      *p++ = *src++;
		}

		break;
	    
	      case '\0':
	      case '/':
		done++;
		break;

	      default:
		*p++ = *src++;
		break;
	    }
	}

	*p = '\0';

	ans = cpystr(q);
	fs_give((void **)&q);
    }

    return(ans);
}


/*
 * Undoes the escape quoting done by add_pat_escapes.
 *
 * Args: src -- The source string.
 *
 * Returns: A string with backslash quoting removed or NULL. The string starts
 *          at src and goes until the end of src or until a / is reached. The
 *          / is not included in the string. /'s may be quoted by preceding
 *          them with a backslash (\) and \'s may also be quoted by
 *          preceding them with a \. In fact, \ quotes any character.
 *          Not quite, \nnn is octal escape, \xXX is hex escape.
 *          Hex escapes are undone but left with a backslash in front.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
remove_pat_escapes(src)
    char *src;
{
    char *ans = NULL, *q, *p;
    int done = 0;

    if(src){
	p = q = (char *)fs_get(strlen(src) + 1);

	while(!done){
	    switch(*src){
	      case '\\':
		src++;
		if(*src){
		    if(isdigit((unsigned char)*src)){	/* octal escape */
			*p++ = '\\';
			*p++ = (char)read_octal(&src);
		    }
		    else if((*src == 'x' || *src == 'X') &&
			    *(src+1) && *(src+2) && isxpair(src+1)){
			*p++ = '\\';
			*p++ = (char)read_hex(src+1);
			src += 3;
		    }
		    else
		      *p++ = *src++;
		}

		break;
	    
	      case '\0':
	      case '/':
		done++;
		break;

	      default:
		*p++ = *src++;
		break;
	    }
	}

	*p = '\0';

	ans = cpystr(q);
	fs_give((void **)&q);
    }

    return(ans);
}


/*
 * Copy a string enclosed in "" without fixing \" or \\. Skip past \"
 * but copy it as is, removing only the enclosing quotes.
 */
char *
copy_quoted_string_asis(src)
    char *src;
{
    char *q, *p;
    int   done = 0, quotes = 0;

    if(src){
	p = q = (char *)fs_get(strlen(src) + 1);

	while(!done){
	    switch(*src){
	      case QUOTE:
		if(++quotes == 2)
		  done++;
		else
		  src++;

		break;

	      case BSLASH:	/* don't count \" as a quote, just copy */
		if(*(src+1) == QUOTE){
		    if(quotes == 1){
			*p++ = *src;
			*p++ = *(src+1);
		    }

		    src += 2;
		}
		else{
		    if(quotes == 1)
		      *p++ = *src;
		    
		    src++;
		}

		break;
	    
	      case '\0':
		fs_give((void **)&q);
		return(NULL);

	      default:
		if(quotes == 1)
		  *p++ = *src;
		
		src++;

		break;
	    }
	}

	*p = '\0';
    }

    return(q);
}


/*
 * Returns non-zero if dir is a prefix of path.
 *         zero     if dir is not a prefix of path, or if dir is empty.
 */
int
in_dir(dir, path)
    char *dir;
    char *path;
{
    return(*dir ? !strncmp(dir, path, strlen(dir)) : 0);
}


/*
 * isxpair -- return true if the first two chars in string are
 *	      hexidecimal characters
 */
int
isxpair(s)
    char *s;
{
    return(isxdigit((unsigned char) *s) && isxdigit((unsigned char) *(s+1)));
}





/*
 *  * * * * * *  something to help managing lists of strings   * * * * * * * *
 */


STRLIST_S *
new_strlist()
{
    STRLIST_S *sp = (STRLIST_S *) fs_get(sizeof(STRLIST_S));
    memset(sp, 0, sizeof(STRLIST_S));
    return(sp);
}


STRLIST_S *
copy_strlist(src)
    STRLIST_S *src;
{
    STRLIST_S *ret = NULL, *sl, *ss, *new_sl;

    if(src){
	ss = NULL;
	for(sl = src; sl; sl = sl->next){
	    new_sl = (STRLIST_S *) fs_get(sizeof(*new_sl));
	    memset((void *) new_sl, 0, sizeof(*new_sl));
	    if(sl->name)
	      new_sl->name = cpystr(sl->name);

	    if(ss){
		ss->next = new_sl;
		ss = ss->next;
	    }
	    else{
		ret = new_sl;
		ss = ret;
	    }
	}
    }

    return(ret);
}


void
free_strlist(strp)
    STRLIST_S **strp;
{
    if(strp && *strp){
	if((*strp)->next)
	  free_strlist(&(*strp)->next);

	if((*strp)->name)
	  fs_give((void **) &(*strp)->name);

	fs_give((void **) strp);
    }
}


KEYWORD_S *
init_keyword_list(keywordarray)
    char **keywordarray;
{
    char     **t, *nickname, *keyword;
    KEYWORD_S *head = NULL, *new, *kl = NULL;

    for(t = keywordarray; t && *t && **t; t++){
	nickname = keyword = NULL;
	get_pair(*t, &nickname, &keyword, 0, 0);
	new = new_keyword_s(keyword, nickname);
	if(keyword)
	  fs_give((void **) &keyword);

	if(nickname)
	  fs_give((void **) &nickname);

	if(kl)
	  kl->next = new;

	kl = new;

	if(!head)
	  head = kl;
    }

    return(head);
}


KEYWORD_S *
new_keyword_s(keyword, nickname)
    char *keyword;
    char *nickname;
{
    KEYWORD_S *kw = NULL;

    kw = (KEYWORD_S *) fs_get(sizeof(*kw));
    memset(kw, 0, sizeof(*kw));

    if(keyword && *keyword)
      kw->kw = cpystr(keyword);

    if(nickname && *nickname)
      kw->nick = cpystr(nickname);
    
    return(kw);
}


void
free_keyword_list(kl)
    KEYWORD_S **kl;
{
    if(kl && *kl){
	if((*kl)->next)
	  free_keyword_list(&(*kl)->next);

	if((*kl)->kw)
	  fs_give((void **) &(*kl)->kw);

	if((*kl)->nick)
	  fs_give((void **) &(*kl)->nick);

	fs_give((void **) kl);
    }
}


/*
 * Return a pointer to the keyword associated with a nickname, or the
 * input itself if no match.
 */
char *
nick_to_keyword(nick)
    char *nick;
{
    KEYWORD_S *kw;
    char      *ret;

    ret = nick;
    for(kw = ps_global->keywords; kw; kw = kw->next)
      if(!strcmp(nick, kw->nick ? kw->nick : kw->kw ? kw->kw : "")){
	  if(kw->nick)
	    ret = kw->kw;

	  break;
      }
    
    return(ret);
}


/*
 * Return a pointer to the nickname associated with a keyword, or the
 * input itself if no match.
 */
char *
keyword_to_nick(keyword)
    char *keyword;
{
    KEYWORD_S *kw;
    char      *ret;

    ret = keyword;
    for(kw = ps_global->keywords; kw; kw = kw->next)
      if(!strcmp(keyword, kw->kw ? kw->kw : "")){
	  if(kw->nick)
	    ret = kw->nick;

	  break;
      }
    
    return(ret);
}



/*
 *  * * * * * * * *      RFC 1522 support routines      * * * * * * * *
 *
 *   RFC 1522 support is *very* loosely based on code contributed
 *   by Lars-Erik Johansson <lej@cdg.chalmers.se>.  Thanks to Lars-Erik,
 *   and appologies for taking such liberties with his code.
 */


#define	RFC1522_INIT	"=?"
#define	RFC1522_INIT_L	2
#define RFC1522_TERM	"?="
#define	RFC1522_TERM_L	2
#define	RFC1522_DLIM	"?"
#define	RFC1522_DLIM_L	1
#define	RFC1522_MAXW	75
#define	ESPECIALS	"()<>@,;:\"/[]?.="
#define	RFC1522_OVERHEAD(S)	(RFC1522_INIT_L + RFC1522_TERM_L +	\
				 (2 * RFC1522_DLIM_L) + strlen(S) + 1);
#define	RFC1522_ENC_CHAR(C)	(((C) & 0x80) || !rfc1522_valtok(C)	\
				 || (C) == '_' )


int	       rfc1522_token PROTO((char *, int (*) PROTO((int)), char *,
				    char **));
int	       rfc1522_valtok PROTO((int));
int	       rfc1522_valenc PROTO((int));
int	       rfc1522_valid PROTO((char *, char **, char **, char **,
				    char **));
char	      *rfc1522_8bit PROTO((void *, int));
char	      *rfc1522_binary PROTO((void *, int));
unsigned char *rfc1522_encoded_word PROTO((unsigned char *, int, char *));


/*
 * rfc1522_decode - try to decode the given source string ala RFC 2047
 *                  (obsoleted RFC 1522) into the given destination buffer.
 *
 * The 'charset' parameter works this way:
 *
 * If charset is NULL and an 'encoded-word' which does not match pine's
 * charset is found, a description of the charset of the encoded word
 * is inserted in that place into the destination buffer, eg: [iso-2022-jp]
 *
 * If charset is non-NULL, instead of the above, a copy of the
 * charset string is allocated and a pointer to this string is written
 * at the address where charset points to, but of course this allocate
 * and copy is only done for the first not matching charset. remaining
 * 'encoded-word's are also decoded silently, even if they are encoded
 * with different charsets.
 *
 * In all cases, charset translation is only attempted from the first
 * not matching charset, so translation may be wrong for the remaining.
 *
 * If charset is set and no non-matching charset is found,
 * NULL is written at the address pointed by 'charset'.
 *
 * Returns: pointer to either the destination buffer containing the
 *	    decoded text, or a pointer to the source buffer if there was
 *          no valid 'encoded-word' found during scanning.
 *          In addition, '*charset' is set as described above.
 */
unsigned char *
rfc1522_decode(d, len, s, charset)
    unsigned char  *d;
    size_t          len;	/* length of d */
    char	   *s;
    char	  **charset;
{
    unsigned char *rv = NULL, *p;
    char	  *start = s, *sw, *enc, *txt, *ew, **q, *lang;
    char          *cset, *cs = NULL;
    unsigned long  l;
    int		   i, described_charset_once = 0;
    int            translate_2022_jp = 0;

    *d = '\0';					/* init destination */
    if(charset)
      *charset = NULL;

    while(s && (sw = strstr(s, RFC1522_INIT))){
	/* validate the rest of the encoded-word */
	if(rfc1522_valid(sw, &cset, &enc, &txt, &ew)){
	    if(!rv)
	      rv = d;				/* remember start of dest */

	    /*
	     * We may have been putting off copying the first part of the
	     * source while waiting to see if we have to copy at all.
	     */
	    if(rv == d && s != start){
		strncpy((char *) d, start,
			(int) min((l = (sw - start)), len-1));
		d += l;				/* advance d, tie off text */
		if(d-rv > len-1)
		  d = rv+len-1;
		*d = '\0';
		s = sw;
	    }

	    /* copy everything between s and sw to destination */
	    for(i = 0; &s[i] < sw; i++)
	      if(!isspace((unsigned char)s[i])){ /* if some non-whitespace */
		  while(s < sw && d-rv<len-1)
		    *d++ = (unsigned char) *s++;

		  described_charset_once = 0;
		  break;
	      }

	    enc[-1] = txt[-1] = ew[0] = '\0';	/* tie off token strings */

	    if(lang = strchr(cset, '*'))
	      *lang++ = '\0';

	    /* Insert text explaining charset if we don't know what it is */
	    if(!strucmp((char *) cset, "iso-2022-jp")){
		translate_2022_jp++;
		dprint(5, (debugfile, "RFC1522_decode: translating %s\n",
		       cset ? cset : "?"));
		if(!ps_global->VAR_CHAR_SET
		   || strucmp(ps_global->VAR_CHAR_SET, "iso-2022-jp")){
		    if(charset){
			if(!*charset)		/* only write first charset */
			  *charset = cpystr(cset);
		    }
		    else if(!described_charset_once++){
			if(F_OFF(F_QUELL_CHARSET_WARNING, ps_global)){
			    if(d-rv<len-1)
			      *d++ = '[';

			    sstrncpy((char **) &d, cset, len-1-(d-rv));
			    if(d-rv<len-1)
			      *d++ = ']';
			    if(d-rv<len-1)
			      *d++ = SPACE;
			}
		    }
		}
		/* else, just translate it silently */
	    }
	    else if((!ps_global->VAR_CHAR_SET
		     || strucmp((char *) cset, ps_global->VAR_CHAR_SET))
	            && strucmp((char *) cset, "US-ASCII")){
		dprint(5, (debugfile, "RFC1522_decode: charset mismatch: %s\n",
		       cset ? cset : "?"));
		if(!cs)
		  cs = cpystr(cset);

		if(charset){
		    if(!*charset)		/* only write first charset */
		      *charset = cpystr(cset);
		}
		else if(!described_charset_once++){
		    if(F_OFF(F_QUELL_CHARSET_WARNING, ps_global)){
			if(d-rv<len-1)
			  *d++ = '[';

			sstrncpy((char **) &d, cset, len-1-(d-rv));
			if(d-rv<len-1)
			  *d++ = ']';
			if(d-rv<len-1)
			  *d++ = SPACE;
		    }
		}
	    }

	    /* based on encoding, write the encoded text to output buffer */
	    switch(*enc){
	      case 'Q' :			/* 'Q' encoding */
	      case 'q' :
		/* special hocus-pocus to deal with '_' exception, too bad */
		for(l = 0L, i = 0; txt[l]; l++)
		  if(txt[l] == '_')
		    i++;

		if(i){
		    q = (char **) fs_get((i + 1) * sizeof(char *));
		    for(l = 0L, i = 0; txt[l]; l++)
		      if(txt[l] == '_'){
			  q[i++] = &txt[l];
			  txt[l] = SPACE;
		      }

		    q[i] = NULL;
		}
		else
		  q = NULL;

		if(p = rfc822_qprint((unsigned char *)txt, strlen(txt), &l)){
		    strncpy((char *) d, (char *) p, min(l,len-1-(d-rv)));
		    d[min(l,len-1-(d-rv))] = '\0';
		    fs_give((void **)&p);	/* free encoded buf */
		    d += l;			/* advance dest ptr to EOL */
		    if(d-rv > len-1)
		      d = rv+len-1;
		}
		else{
		    if(q)
		      fs_give((void **) &q);

		    goto bogus;
		}

		if(q){				/* restore underscores */
		    for(i = 0; q[i]; i++)
		      *(q[i]) = '_';

		    fs_give((void **)&q);
		}

		break;

	      case 'B' :			/* 'B' encoding */
	      case 'b' :
		if(p = rfc822_base64((unsigned char *) txt, strlen(txt), &l)){
		    /*
		     * C-client's rfc822_base64 was changed so that it now
		     * does do null termination of the returned value.
		     * As long as there are no nulls in the rest of the
		     * string, we could now get rid of worrying about the
		     * l length arg in the next two lines. In fact, since
		     * embedded nulls don't make sense in this context and
		     * won't work correctly anyway, it is really a no-op.
		     */
		    strncpy((char *) d, (char *) p, min(l,len-1-(d-rv)));
		    d[min(l,len-1-(d-rv))] = '\0';
		    fs_give((void **)&p);	/* free encoded buf */
		    d += l;			/* advance dest ptr to EOL */
		    if(d-rv > len-1)
		      d = rv+len-1;
		}
		else
		  goto bogus;

		break;

	      default:
		sstrncpy((char **) &d, txt, len-1-(d-rv));
		dprint(1, (debugfile, "RFC1522_decode: Unknown ENCODING: %s\n",
		       enc ? enc : "?"));
		break;
	    }

	    /* restore trompled source string */
	    enc[-1] = txt[-1] = '?';
	    ew[0]   = RFC1522_TERM[0];

	    /* advance s to start of text after encoded-word */
	    s = ew + RFC1522_TERM_L;

	    if(lang)
	      lang[-1] = '*';
	}
	else{

	    /*
	     * Found intro, but bogus data followed, treat it as normal text.
	     */

	    /* if already copying to destn, copy it */
	    if(rv){
		strncpy((char *) d, s,
			(int) min((l = (sw - s) + RFC1522_INIT_L),
			len-1-(d-rv)));
		d += l;				/* advance d, tie off text */
		if(d-rv > len-1)
		  d = rv+len-1;
		*d = '\0';
		s += l;				/* advance s beyond intro */
	    }
	    else	/* probably won't have to copy it at all, wait */
	      s += ((sw - s) + RFC1522_INIT_L);
	}
    }

    if(rv && *s)				/* copy remaining text */
      strncat((char *) rv, s, len - 1 - strlen((char *) rv));

    if(translate_2022_jp){
	char *qq;

	qq = cpystr(rv);
	istrncpy(rv, (char *) qq, len);
	rv[len - 1] = '\0';
	fs_give((void **) &qq);
    }
    else if(cs){
	if(rv && F_OFF(F_DISABLE_CHARSET_CONVERSIONS, ps_global)
	   && ps_global->VAR_CHAR_SET
	   && strucmp(ps_global->VAR_CHAR_SET, cs)){
	    CONV_TABLE *ct;
	    char *dcs = ps_global->VAR_CHAR_SET;
	    
	    /* Convert ISO-2022-JP to EUC (UNIX) or Shift-JIS (PC) */
	    if(F_OFF(F_DISABLE_2022_JP_CONVERSIONS, ps_global) &&
	       !strucmp(dcs, "iso-2022-jp")) {
#ifdef _WINDOWS
		dcs = "shift-jis";
#else
		dcs = "euc-jp";
#endif
	    }

	    /*
	     * If we know how to do the translation from cs
	     * to VAR_CHAR_SET, do it in place.
	     */
	    ct = conversion_table(cs, ps_global->VAR_CHAR_SET);
	    if(ct && ct->table){
		if(ct->convert == gf_convert_8bit_charset){
		    unsigned char *table = (unsigned char *) ct->table;
		    for (p = rv; len-- > 0 && *p; p++)
			*p = table[*p];
		}
		else if(ct->convert == gf_convert_utf8_charset){
		    SIZEDTEXT src,dst;
				/* determine length of source */
		    for(src.data = rv, src.size = 0;
			(src.size < len) && src.data[src.size]; ++src.size);
				/* convert charset */
		    if(utf8_cstext (&src, dcs, &dst,'?')) {
				/* should always fit, but be paranoid */
			if(dst.size <= len) {
			    memcpy(rv, dst.data, dst.size);
			    rv[dst.size] = '\0';
			}
			fs_give((void **) &dst.data);
		    }
		}
	    }
	}
    }

    if(cs)
      fs_give((void **) &cs);

    return(rv ? rv : (unsigned char *) start);

  bogus:
    dprint(1, (debugfile, "RFC1522_decode: BOGUS INPUT: -->%s<--\n",
	   start ? start : "?"));
    return((unsigned char *) start);
}


/*
 * rfc1522_token - scan the given source line up to the end_str making
 *		   sure all subsequent chars are "valid" leaving endp
 *		   a the start of the end_str.
 * Returns: TRUE if we got a valid token, FALSE otherwise
 */
int
rfc1522_token(s, valid, end_str, endp)
    char  *s;
    int	 (*valid) PROTO((int));
    char  *end_str;
    char **endp;
{
    while(*s){
	if((char) *s == *end_str		/* test for matching end_str */
	   && ((end_str[1])
	        ? !strncmp((char *)s + 1, end_str + 1, strlen(end_str + 1))
	        : 1)){
	    *endp = s;
	    return(TRUE);
	}

	if(!(*valid)(*s++))			/* test for valid char */
	  break;
    }

    return(FALSE);
}


/*
 * rfc1522_valtok - test for valid character in the RFC 1522 encoded
 *		    word's charset and encoding fields.
 */
int
rfc1522_valtok(c)
    int c;
{
    return(!(c == SPACE || iscntrl(c & 0x7f) || strindex(ESPECIALS, c)));
}


/*
 * rfc1522_valenc - test for valid character in the RFC 1522 encoded
 *		    word's encoded-text field.
 */
int
rfc1522_valenc(c)
    int c;
{
    return(!(c == '?' || c == SPACE) && isprint((unsigned char)c));
}


/*
 * rfc1522_valid - validate the given string as to it's rfc1522-ness
 */
int
rfc1522_valid(s, charset, enc, txt, endp)
    char  *s;
    char **charset;
    char **enc;
    char **txt;
    char **endp;
{
    char *c, *e, *t, *p;
    int   rv;

    rv = rfc1522_token(c = s+RFC1522_INIT_L, rfc1522_valtok, RFC1522_DLIM, &e)
	   && rfc1522_token(++e, rfc1522_valtok, RFC1522_DLIM, &t)
	   && rfc1522_token(++t, rfc1522_valenc, RFC1522_TERM, &p)
	   && p - s <= RFC1522_MAXW;

    if(charset)
      *charset = c;

    if(enc)
      *enc = e;

    if(txt)
      *txt = t;

    if(endp)
      *endp = p;

    return(rv);
}


/*
 * rfc1522_encode - encode the given source string ala RFC 1522,
 *		    IF NECESSARY, into the given destination buffer.
 *		    Don't bother copying if it turns out encoding
 *		    isn't necessary.
 *
 * Returns: pointer to either the destination buffer containing the
 *	    encoded text, or a pointer to the source buffer if we didn't
 *          have to encode anything.
 */
char *
rfc1522_encode(d, len, s, charset)
    char	  *d;
    size_t         len;		/* length of d */
    unsigned char *s;
    char	  *charset;
{
    unsigned char *p, *q;
    int		   n;

    if(!s)
      return((char *) s);

    if(!charset)
      charset = UNKNOWN_CHARSET;

    /* look for a reason to encode */
    for(p = s, n = 0; *p; p++)
      if((*p) & 0x80){
	  n++;
      }
      else if(*p == RFC1522_INIT[0]
	      && !strncmp((char *) p, RFC1522_INIT, RFC1522_INIT_L)){
	  if(rfc1522_valid((char *) p, NULL, NULL, NULL, (char **) &q))
	    p = q + RFC1522_TERM_L - 1;		/* advance past encoded gunk */
      }
      else if(*p == ESCAPE && match_escapes((char *)(p+1))){
	  n++;
      }

    if(n){					/* found, encoding to do */
	char *rv  = d, *t,
	      enc = (n > (2 * (p - s)) / 3) ? 'B' : 'Q';

	while(*s){
	    if(d-rv < len-1-(RFC1522_INIT_L+2*RFC1522_DLIM_L+1)){
		sstrcpy(&d, RFC1522_INIT);	/* insert intro header, */
		sstrcpy(&d, charset);		/* character set tag, */
		sstrcpy(&d, RFC1522_DLIM);	/* and encoding flavor */
		*d++ = enc;
		sstrcpy(&d, RFC1522_DLIM);
	    }

	    /*
	     * feed lines to encoder such that they're guaranteed
	     * less than RFC1522_MAXW.
	     */
	    p = rfc1522_encoded_word(s, enc, charset);
	    if(enc == 'B')			/* insert encoded data */
	      sstrncpy(&d, t = rfc1522_binary(s, p - s), len-1-(d-rv));
	    else				/* 'Q' encoding */
	      sstrncpy(&d, t = rfc1522_8bit(s, p - s), len-1-(d-rv));

	    sstrncpy(&d, RFC1522_TERM, len-1-(d-rv));	/* insert terminator */
	    fs_give((void **) &t);
	    if(*p)				/* more src string follows */
	      sstrncpy(&d, "\015\012 ", len-1-(d-rv));	/* insert cont. line */

	    s = p;				/* advance s */
	}

	rv[len-1] = '\0';
	return(rv);
    }
    else
      return((char *) s);			/* no work for us here */
}



/*
 * rfc1522_encoded_word -- cut given string into max length encoded word
 *
 * Return: pointer into 's' such that the encoded 's' is no greater
 *	   than RFC1522_MAXW
 *
 *  NOTE: this line break code is NOT cognizant of any SI/SO
 *  charset requirements nor similar strategies using escape
 *  codes.  Hopefully this will matter little and such
 *  representation strategies don't also include 8bit chars.
 */
unsigned char *
rfc1522_encoded_word(s, enc, charset)
    unsigned char *s;
    int		   enc;
    char	  *charset;
{
    int goal = RFC1522_MAXW - RFC1522_OVERHEAD(charset);

    if(enc == 'B')			/* base64 encode */
      for(goal = ((goal / 4) * 3) - 2; goal && *s; goal--, s++)
	;
    else				/* special 'Q' encoding */
      for(; goal && *s; s++)
	if((goal -= RFC1522_ENC_CHAR(*s) ? 3 : 1) < 0)
	  break;

    return(s);
}



/*
 * rfc1522_8bit -- apply RFC 1522 'Q' encoding to the given 8bit buffer
 *
 * Return: alloc'd buffer containing encoded string
 */
char *
rfc1522_8bit(src, slen)
    void *src;
    int   slen;
{
    char *ret = (char *) fs_get ((size_t) (3*slen + 2));
    char *d = ret;
    unsigned char c;
    unsigned char *s = (unsigned char *) src;

    while (slen--) {				/* for each character */
	if (((c = *s++) == '\015') && (*s == '\012') && slen) {
	    *d++ = '\015';			/* true line break */
	    *d++ = *s++;
	    slen--;
	}
	else if(c == SPACE){			/* special encoding case */
	    *d++ = '_';
	}
	else if(RFC1522_ENC_CHAR(c)){
	    *d++ = '=';				/* quote character */
	    C2XPAIR(c, d);
	}
	else
	  *d++ = (char) c;			/* ordinary character */
    }

    *d = '\0';					/* tie off destination */
    return(ret);
}


/*
 * rfc1522_binary -- apply RFC 1522 'B' encoding to the given 8bit buffer
 *
 * Return: alloc'd buffer containing encoded string
 */
char *
rfc1522_binary (src, srcl)
    void *src;
    int   srcl;
{
    static char *v =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char *s = (unsigned char *) src;
    char *ret, *d;

    d = ret = (char *) fs_get ((size_t) ((((srcl + 2) / 3) * 4) + 1));
    for (; srcl; s += 3) {	/* process tuplets */
				/* byte 1: high 6 bits (1) */
	*d++ = v[s[0] >> 2];
				/* byte 2: low 2 bits (1), high 4 bits (2) */
	*d++ = v[((s[0] << 4) + (--srcl ? (s[1] >> 4) : 0)) & 0x3f];
				/* byte 3: low 4 bits (2), high 2 bits (3) */
	*d++ = srcl ? v[((s[1] << 2) + (--srcl ? (s[2] >> 6) :0)) & 0x3f] :'=';
				/* byte 4: low 6 bits (3) */
	*d++ = srcl ? v[s[2] & 0x3f] : '=';
	if(srcl)
	  srcl--;		/* count third character if processed */
    }

    *d = '\0';			/* tie off string */
    return(ret);		/* return the resulting string */
}


/*
 * Checks if charset conversion is possible and which quality could be achieved
 *
 * args: from_cs -- charset to convert from
 *       to_cs   -- charset to convert to
 *
 * Results:
 * CONV_TABLE->table   -- conversion table, NULL if conversion not needed
 *                        or not supported
 * CONV_TABLE->quality -- conversion quality (conversion not supported, not
 *                        needed, loses special chars, or loses letters
 *
 * The other entries of CONV_TABLE are used inside this function only
 * and may not be used outside unless this documentation is updated.
 */
CONV_TABLE *
conversion_table(from_cs, to_cs)
    char *from_cs,
         *to_cs;
{
    int               i, j;
    unsigned char    *p = NULL;
    unsigned short   *fromtab, *totab;
    CONV_TABLE       *ct = NULL;
    CHARSET          *from, *to;
    static CONV_TABLE null_tab;

    if(!(from_cs && *from_cs && to_cs && *to_cs) || !strucmp(from_cs, to_cs)){
	memset(&null_tab, 0, sizeof(null_tab));
	null_tab.quality = CV_NO_TRANSLATE_NEEDED;
	return(&null_tab);
    }

    if(F_OFF(F_DISABLE_2022_JP_CONVERSIONS, ps_global) &&
       !strucmp(to_cs,"ISO-2022-JP") && utf8_charset("EUC-JP")) {
#ifdef _WINDOWS
	to_cs = "SHIFT-JIS";	/* Windows uses Shift-JIS internally */
#else
	to_cs = "EUC-JP";	/* assume EUC-JP internally */
#endif
    }

    /*
     * First check to see if we are already set up for this pair of charsets.
     */
    if((ct = ps_global->conv_table) != NULL
       && ct->from_charset && ct->to_charset
       && !strucmp(ct->from_charset, from_cs)
       && !strucmp(ct->to_charset, to_cs))
      return(ct);

    /*
     * No such luck. Get rid of the cache of the previous translation table
     * and build a new one.
     */
    if(ct){
	if(ct->table && (ct->convert != gf_convert_utf8_charset))
	  fs_give((void **) &ct->table);
	
	if(ct->from_charset)
	  fs_give((void **) &ct->from_charset);
	
	if(ct->to_charset)
	  fs_give((void **) &ct->to_charset);
    }
    else
      ct = ps_global->conv_table = (CONV_TABLE *) fs_get(sizeof(*ct));
    
    memset(ct, 0, sizeof(*ct));

    ct->from_charset = cpystr(from_cs);
    ct->to_charset   = cpystr(to_cs);
    ct->quality = CV_NO_TRANSLATE_POSSIBLE;

    /*
     * Check to see if a translation is feasible.
     */
    from = utf8_charset(from_cs);
    to =   utf8_charset(to_cs);

    if(from && to){		/* if both charsets found */
				/* no mapping if same or from is ASCII */
	if((from->type == to->type && from->tab == to->tab)
	   || (from->type == CT_ASCII))
	    ct->quality = CV_NO_TRANSLATE_NEEDED;
	else switch(from->type){
	case CT_1BYTE0:		/* 1 byte no table */
	case CT_1BYTE:		/* 1 byte ASCII + table 0x80-0xff */
	case CT_1BYTE8:		/* 1 byte table 0x00 - 0xff */
	    switch(to->type){
	    case CT_1BYTE0:	/* 1 byte no table */
	    case CT_1BYTE:	/* 1 byte ASCII + table 0x80-0xff */
	    case CT_1BYTE8:	/* 1 byte table 0x00 - 0xff */
	        ct->quality = (from->script & to->script) ?
		  CV_LOSES_SOME_LETTERS : CV_LOSES_SPECIAL_CHARS;
		break;
	    }
	    break;
	case CT_UTF8:		/* variable UTF-8 encoded Unicode no table */
	/* If source is UTF-8, see if destination charset has an 8 or 16 bit
	 * coded character set that we can translate to.  By special
	 * dispensation, kludge ISO-2022-JP to EUC or Shift-JIS, but don't
	 * try to do any other ISO 2022 charsets or UTF-7.
	 */
	    switch (to->type){
	    case CT_SJIS:	/* 2 byte Shift-JIS */
				/* only win if can get EUC-JP chartab */
		if(utf8_charset("EUC-JP"))
		    ct->quality = CV_LOSES_SOME_LETTERS;
		break;
	    case CT_ASCII:	/* 7-bit ASCII no table */
	    case CT_1BYTE0:	/* 1 byte no table */
	    case CT_1BYTE:	/* 1 byte ASCII + table 0x80-0xff */
	    case CT_1BYTE8:	/* 1 byte table 0x00 - 0xff */
	    case CT_EUC:	/* 2 byte ASCII + utf8_eucparam base/CS2/CS3 */
	    case CT_DBYTE:	/* 2 byte ASCII + utf8_eucparam */
	    case CT_DBYTE2:	/* 2 byte ASCII + utf8_eucparam plane1/2 */
		ct->quality = CV_LOSES_SOME_LETTERS;
		break;
	    }
	    break;
	}

	switch (ct->quality) {	/* need to map? */
	case CV_NO_TRANSLATE_POSSIBLE:
	case CV_NO_TRANSLATE_NEEDED:
	  break;		/* no mapping needed */
	default:		/* do mapping */
	    switch (from->type) {
	    case CT_UTF8:	/* UTF-8 to legacy character set */
	      if(ct->table = utf8_rmap (to_cs))
		ct->convert = gf_convert_utf8_charset;
	      break;

	    case CT_1BYTE0:	/* ISO 8859-1 */
	    case CT_1BYTE:	/* low part ASCII, high part other */
	    case CT_1BYTE8:	/* low part has some non-ASCII */
	    /*
	     * The fromtab and totab tables are mappings from the 128 character
	     * positions 128-255 to their Unicode values (so unsigned shorts).
	     * The table we are creating is such that if
	     *
	     *    from_char_value -> unicode_value
	     *    to_char_value   -> same_unicode_value
	     *
	     *  then we want to map from_char_value -> to_char_value
	     *
	     * To simplify conversions we create the whole 256 element array,
	     * with the first 128 positions just the identity. If there is no
	     * conversion for a particular from_char_value (that is, no
	     * to_char_value maps to the same unicode character) then we put
	     *  '?' in that character. We may want to output blob on the PC,
	     * but don't so far.
	     *
	     * If fromtab or totab are NULL, that means the mapping is simply
	     * the identity mapping. Since that is still useful to us, we
	     * create it on the fly.
	     */
		fromtab = (unsigned short *) from->tab;
		totab   = (unsigned short *) to->tab;

		ct->convert = gf_convert_8bit_charset;
		p = ct->table = (unsigned char *)
		  fs_get(256 * sizeof(unsigned char));
		for(i = 0; i < 256; i++){
		    unsigned int fc;
		    p[i] = '?';
		    switch(from->type){	/* get "from" UCS-2 codepoint */
		    case CT_1BYTE0:	/* ISO 8859-1 */
			fc = i;
			break;
		    case CT_1BYTE:	/* low part ASCII, high part other */
			fc = (i < 128) ? i : fromtab[i-128];
			break;
		    case CT_1BYTE8:	/* low part has some non-ASCII */
			fc = fromtab[i];
			break;
		    }
		    switch(to->type){ /* match against "to" UCS-2 codepoint */
		    case CT_1BYTE0: /* identity match for ISO 8859-1*/
			if(fc < 256)
			  p[i] = fc;
			break;
		    case CT_1BYTE: /* ASCII is identity, search high part */
			if(fc < 128) p[i] = fc;
			else for(j = 0; j < 128; j++){
			    if(fc == totab[j]){
				p[i] = 128 + j;
				break;
			    }
			}
			break;
		    case CT_1BYTE8: /* search all codepoints */
			for(j = 0; j < 256; j++){
			    if(fc == totab[j]){
			      p[i] = j;
			      break;
			    }
			}
			break;
		    }
		}
		break;
	    }
	}
    }

    return(ct);
}



/*
 *  * * * * * * * *      RFC 1738 support routines      * * * * * * * *
 */


/*
 * Various helpful definitions
 */
#define	RFC1738_SAFE	"$-_.+"			/* "safe" */
#define	RFC1738_EXTRA	"!*'(),"		/* "extra" */
#define	RFC1738_RSVP	";/?:@&="		/* "reserved" */
#define	RFC1738_NEWS	"-.+_"			/* valid for "news:" URL */
#define	RFC1738_FUDGE	"#{}|\\^~[]"		/* Unsafe, but popular */
#define	RFC1738_ESC(S)	(*(S) == '%' && isxpair((S) + 1))


int   rfc1738uchar PROTO((char *));
int   rfc1738xchar PROTO((char *));
char *rfc1738_scheme_part PROTO((char *));



/*
 * rfc1738_scan -- Scan the given line for possible URLs as defined
 *		   in RFC1738
 */
char *
rfc1738_scan(line, len)
    char *line;
    int  *len;
{
    char *colon, *start, *end;
    int   n;

    /* process each : in the line */
    for(; colon = strindex(line, ':'); line = end){
	end = colon + 1;
	if(colon == line)		/* zero length scheme? */
	  continue;

	/*
	 * Valid URL (ala RFC1738 BNF)?  First, first look to the
	 * left to make sure there are valid "scheme" chars...
	 */
	start = colon - 1;
	while(1)
	  if(!(isdigit((unsigned char) *start)
	       || isalpha((unsigned char) *start)
	       || strchr("+-.", *start))){
	      start++;			/* advance over bogus char */
	      break;
	  }
	  else if(start > line)
	    start--;
	  else
	    break;

	/*
	 * Make sure everyhing up to the colon is a known scheme...
	 */
	if(start && (n = colon - start) && !isdigit((unsigned char) *start)
	   && (((n == 3
		 && (*start == 'F' || *start == 'f')
		 && !struncmp(start+1, "tp", 2))
		|| (n == 4
		    && (((*start == 'H' || *start == 'h') 
			 && !struncmp(start + 1, "ttp", 3))
			|| ((*start == 'N' || *start == 'n')
			    && !struncmp(start + 1, "ews", 3))
			|| ((*start == 'N' || *start == 'n')
			    && !struncmp(start + 1, "ntp", 3))
			|| ((*start == 'W' || *start == 'w')
			    && !struncmp(start + 1, "ais", 3))
#ifdef	ENABLE_LDAP
			|| ((*start == 'L' || *start == 'l')
			    && !struncmp(start + 1, "dap", 3))
#endif
			|| ((*start == 'I' || *start == 'i')
			    && !struncmp(start + 1, "map", 3))
			|| ((*start == 'F' || *start == 'f')
			    && !struncmp(start + 1, "ile", 3))))
		|| (n == 5
		    && (*start == 'H' || *start == 'h')
		    && !struncmp(start+1, "ttps", 4))
		|| (n == 6
		    && (((*start == 'G' || *start == 'g')
			 && !struncmp(start+1, "opher", 5))
			|| ((*start == 'M' || *start == 'm')
			    && !struncmp(start + 1, "ailto", 5))
			|| ((*start == 'T' || *start == 't')
			    && !struncmp(start + 1, "elnet", 5))))
		|| (n == 8
		    && (*start == 'P' || *start == 'p')
		    && !struncmp(start + 1, "rospero", 7)))
	       || url_external_specific_handler(start, n))){
		/*
		 * Second, make sure that everything to the right of the
		 * colon is valid for a "schemepart"...
		 */

	    if((end = rfc1738_scheme_part(colon + 1)) - colon > 1){
		int i, j;

		/* make sure something useful follows colon */
		for(i = 0, j = end - colon; i < j; i++)
		  if(!strchr(RFC1738_RSVP, colon[i]))
		    break;

		if(i != j){
		    *len = end - start;

		    /*
		     * Special case handling for comma.
		     * See the problem is comma's valid, but if it's the
		     * last character in the url, it's likely intended
		     * as a delimiter in the text rather part of the URL.
		     * In most cases any way, that's why we have the
		     * exception.
		     */
		    if(*(end - 1) == ','
		       || (*(end - 1) == '.' && (!*end  || *end == ' ')))
		      (*len)--;

		    if(*len - (colon - start) > 0)
		      return(start);
		}
	    }
	}
    }

    return(NULL);
}


/*
 * rfc1738_scheme_part - make sure what's to the right of the
 *			 colon is valid
 *
 * NOTE: we have a problem matching closing parens when users 
 *       bracket the url in parens.  So, lets try terminating our
 *	 match on any closing paren that doesn't have a coresponding
 *       open-paren.
 */
char *
rfc1738_scheme_part(s)
    char *s;
{
    int n, paren = 0, bracket = 0;

    while(1)
      switch(*s){
	default :
	  if(n = rfc1738xchar(s)){
	      s += n;
	      break;
	  }

	case '\0' :
	  return(s);

	case '[' :
	  bracket++;
	  s++;
	  break;

	case ']' :
	  if(bracket--){
	      s++;
	      break;
	  }

	  return(s);

	case '(' :
	  paren++;
	  s++;
	  break;

	case ')' :
	  if(paren--){
	      s++;
	      break;
	  }

	  return(s);
      }
}



/*
 * rfc1738_str - convert rfc1738 escaped octets in place
 */
char *
rfc1738_str(s)
    char *s;
{
    register char *p = s, *q = s;

    while(1)
      switch(*q = *p++){
	case '%' :
	  if(isxpair(p)){
	      *q = X2C(p);
	      p += 2;
	  }

	default :
	  q++;
	  break;

	case '\0':
	  return(s);
      }
}


/*
 * rfc1738uchar - returns TRUE if the given char fits RFC 1738 "uchar" BNF
 */
int
rfc1738uchar(s)
    char *s;
{
    return((RFC1738_ESC(s))		/* "escape" */
	     ? 2
	     : (isalnum((unsigned char) *s)	/* alphanumeric */
		|| strchr(RFC1738_SAFE, *s)	/* other special stuff */
		|| strchr(RFC1738_EXTRA, *s)));
}


/*
 * rfc1738xchar - returns TRUE if the given char fits RFC 1738 "xchar" BNF
 */
int
rfc1738xchar(s)
    char *s;
{
    int n;

    return((n = rfc1738uchar(s))
	    ? n
	    : (strchr(RFC1738_RSVP, *s) != NULL
	       || strchr(RFC1738_FUDGE, *s)));
}


/*
 * rfc1738_num - return long value of a string of digits, possibly escaped
 */
long
rfc1738_num(s)
    char **s;
{
    register char *p = *s;
    long n = 0L;

    for(; *p; p++)
      if(*p == '%' && isxpair(p+1)){
	  int c = X2C(p+1);
	  if(isdigit((unsigned char) c)){
	      n = (c - '0') + (n * 10);
	      p += 2;
	  }
	  else
	    break;
      }
      else if(isdigit((unsigned char) *p))
	n = (*p - '0') + (n * 10);
      else
	break;

    *s = p;
    return(n);
}


int
rfc1738_group(s)
    char *s;
{
    return(isalnum((unsigned char) *s)
	   || RFC1738_ESC(s)
	   || strchr(RFC1738_NEWS, *s));
}


/*
 * Encode (hexify) a mailto url.
 *
 * Args  s -- src url
 *
 * Returns  An allocated string which is suitably encoded.
 *          Result should be freed by caller.
 *
 * Since we don't know here which characters are reserved characters (? and &)
 * for use in delimiting the pieces of the url and which are just those
 * characters contained in the data that should be encoded, we always encode
 * them. That's because we know we don't use those as reserved characters.
 * If you do use those as reserved characters you have to encode each part
 * separately.
 */
char *
rfc1738_encode_mailto(s)
    char *s;
{
    char *d, *ret = NULL;

    if(s){
	/* Worst case, encode every character */
	ret = d = (char *)fs_get((3*strlen(s) + 1) * sizeof(char));
	while(*s){
	    if(isalnum((unsigned char)*s)
	       || strchr(RFC1738_SAFE, *s)
	       || strchr(RFC1738_EXTRA, *s))
	      *d++ = *s++;
	    else{
		*d++ = '%';
		C2XPAIR(*s, d);
		s++;
	    }
	}

	*d = '\0';
    }

    return(ret);
}


/*
 *  * * * * * * * *      RFC 1808 support routines      * * * * * * * *
 */


int
rfc1808_tokens(url, scheme, net_loc, path, parms, query, frag)
    char  *url;
    char **scheme, **net_loc, **path, **parms, **query, **frag;
{
    char *p, *q, *start, *tmp = cpystr(url);

    start = tmp;
    if(p = strchr(start, '#')){		/* fragment spec? */
	*p++ = '\0';
	if(*p)
	  *frag = cpystr(p);
    }

    if((p = strchr(start, ':')) && p != start){ /* scheme part? */
	for(q = start; q < p; q++)
	  if(!(isdigit((unsigned char) *q)
	       || isalpha((unsigned char) *q)
	       || strchr("+-.", *q)))
	    break;

	if(p == q){
	    *p++ = '\0';
	    *scheme = cpystr(start);
	    start = p;
	}
    }

    if(*start == '/' && *(start+1) == '/'){ /* net_loc */
	if(p = strchr(start+2, '/'))
	  *p++ = '\0';

	*net_loc = cpystr(start+2);
	if(p)
	  start = p;
	else *start = '\0';		/* End of parse */
    }

    if(p = strchr(start, '?')){
	*p++ = '\0';
	*query = cpystr(p);
    }

    if(p = strchr(start, ';')){
	*p++ = '\0';
	*parms = cpystr(p);
    }

    if(*start)
      *path = cpystr(start);

    fs_give((void **) &tmp);

    return(1);
}



/*
 * web_host_scan -- Scan the given line for possible web host names
 *
 * NOTE: scan below is limited to DNS names ala RFC1034
 */
char *
web_host_scan(line, len)
    char *line;
    int  *len;
{
    char *end, last = '\0';

    for(; *line; last = *line++)
      if((*line == 'w' || *line == 'W')
	 && (!last || !(isalnum((unsigned char) last)
			|| last == '.' || last == '-'))
	 && (((*(line + 1) == 'w' || *(line + 1) == 'W')	/* "www" */
	      && (*(line + 2) == 'w' || *(line + 2) == 'W'))
	     || ((*(line + 1) == 'e' || *(line + 1) == 'E')	/* "web." */
		 && (*(line + 2) == 'b' || *(line + 2) == 'B')
		 && *(line + 3) == '.'))){
	  end = rfc1738_scheme_part(line + 3);
	  if((*len = end - line) > ((*(line+3) == '.') ? 4 : 3)){
	      /* Dread comma exception, see note in rfc1738_scan */
	      if(strchr(",:", *(line + (*len) - 1))
		 || (*(line + (*len) - 1) == '.'
		     && (!*(line + (*len)) || *(line + (*len)) == ' ')))
		(*len)--;

	      return(line);
	  }
	  else
	    line += 3;
      }

    return(NULL);
}


/*
 * mail_addr_scan -- Scan the given line for possible RFC822 addr-spec's
 *
 * NOTE: Well, OK, not strictly addr-specs since there's alot of junk
 *	 we're tying to sift thru and we'd like to minimize false-pos
 *	 matches.
 */
char *
mail_addr_scan(line, len)
    char *line;
    int  *len;
{
    char *amp, *start, *end;

    /* process each : in the line */
    for(; amp = strindex(line, '@'); line = end){
	end = amp + 1;
	/* zero length addr? */
	if(amp == line || !isalnum((unsigned char) *(start = amp - 1)))
	  continue;

	/*
	 * Valid address (ala RFC822 BNF)?  First, first look to the
	 * left to make sure there are valid "scheme" chars...
	 */
	while(1)
	  /* NOTE: we're not doing quoted-strings */
	  if(!(isalnum((unsigned char) *start) || strchr(".-_+", *start))){
	      /* advance over bogus char, and erase leading punctuation */
	      for(start++;
		  *start == '.' || *start == '-' || *start == '_';
		  start++)
		;

	      break;
	  }
	  else if(start > line)
	    start--;
	  else
	    break;

	/*
	 * Make sure everyhing up to the colon is a known scheme...
	 */
	if(start && (amp - start) > 0){
	    /*
	     * Second, make sure that everything to the right of
	     * amp is valid for a "domain"...
	     */
	    if(*(end = amp + 1) == '['){ /* domain literal */
		int dots = 3;

		for(++end; *end ; end++)
		  if(*end == ']'){
		      if(!dots){
			  *len = end - start + 1;
			  return(start);
		      }
		      else
			break;		/* bogus */
		  }
		  else if(*end == '.'){
		      if(--dots < 0)
			break;		/* bogus */
		  }
		  else if(!isdigit((unsigned char) *end))
		    break;		/* bogus */
	    }
	    else if(isalnum((unsigned char) *end)){ /* domain name? */
		for(++end; ; end++)
		  if(!(*end && (isalnum((unsigned char) *end)
				|| *end == '-'
				|| *end == '.'
				|| *end == '_'))){
		      /* can't end with dash, dot or underscore */
		      while(!isalnum((unsigned char) *(end - 1)))
			end--;

		      *len = end - start;
		      return(start);
		  }
	    }
	}
    }

    return(NULL);
}



/*
 *  * * * * * * * *      RFC 2231 support routines      * * * * * * * *
 */


/* Useful def's */
#define	RFC2231_MAX	64


char *
rfc2231_get_param(parms, name, charset, lang)
    PARAMETER *parms;
    char      *name, **charset, **lang;
{
    char *buf, *p;
    int	  decode = 0, name_len, i;
    unsigned n;

    name_len = strlen(name);
    for(; parms ; parms = parms->next)
      if(!struncmp(name, parms->attribute, name_len))
	if(parms->attribute[name_len] == '*'){
	    for(p = &parms->attribute[name_len + 1], n = 0; *(p+n); n++)
	      ;

	    decode = *(p + n - 1) == '*';

	    if(isdigit((unsigned char) *p)){
		char *pieces[RFC2231_MAX];
		int   count = 0, len;

		memset(pieces, 0, RFC2231_MAX * sizeof(char *));

		while(parms){
		    n = 0;
		    do
		      n = (n * 10) + (*p - '0');
		    while(isdigit(*++p) && n < RFC2231_MAX);

		    if(n < RFC2231_MAX){
			pieces[n] = parms->value;
			if(n > count)
			  count = n;
		    }
		    else{
			q_status_message1(SM_ORDER | SM_DING, 0, 3,
		   "Invalid attachment parameter segment number: %.25s",
					  name);
			return(NULL);		/* Too many segments! */
		    }

		    while(parms = parms->next)
		      if(!struncmp(name, parms->attribute, name_len)){
			  if(*(p = &parms->attribute[name_len]) == '*'
			      && isdigit((unsigned char) *++p))
			    break;
			  else
			    return(NULL);	/* missing segment no.! */
		      }
		}

		for(i = len = 0; i <= count; i++)
		  if(pieces[i])
		    len += strlen(pieces[i]);
		  else{
		      q_status_message1(SM_ORDER | SM_DING, 0, 3,
		        "Missing attachment parameter sequence: %.25s",
					name);

		    return(NULL);		/* hole! */
		  }

		buf = (char *) fs_get((len + 1) * sizeof(char));

		for(i = len = 0; i <= count; i++){
		    if(n = *(p = pieces[i]) == '\"') /* quoted? */
		      p++;

		    while(*p && !(n && *p == '\"' && !*(p+1)))
		      buf[len++] = *p++;
		}

		buf[len] = '\0';
	    }
	    else
	      buf = cpystr(parms->value);

	    /* Do any RFC 2231 decoding? */
	    if(decode){
		n = 0;

		if(p = strchr(buf, '\'')){
		    n = (p - buf) + 1;
		    if(charset){
			*p = '\0';
			*charset = cpystr(buf);
			*p = '\'';
		    }

		    if(p = strchr(&buf[n], '\'')){
			n = (p - buf) + 1;
			if(lang){
			    *p = '\0';
			    *lang = cpystr(p);
			    *p = '\'';
			}
		    }
		}

		if(n){
		    /* Suck out the charset & lang while decoding hex */
		    p = &buf[n];
		    for(i = 0; buf[i] = *p; i++)
		      if(*p++ == '%' && isxpair(p)){
			  buf[i] = X2C(p);
			  p += 2;
		      }
		}
		else
		  fs_give((void **) &buf);	/* problems!?! */
	    }
	    
	    return(buf);
	}
	else
	  return(cpystr(parms->value ? parms->value : ""));

    return(NULL);
}


int
rfc2231_output(so, attrib, value, specials, charset)
    STORE_S *so;
    char    *attrib, *value, *specials, *charset;
{
    int  i, line = 0, encode = 0, quote = 0;

    /*
     * scan for hibit first since encoding clue has to
     * come on first line if any parms are broken up...
     */
    for(i = 0; value && value[i]; i++)
      if(value[i] & 0x80){
	  encode++;
	  break;
      }

    for(i = 0; ; i++){
	if(!(value && value[i]) || i > 80){	/* flush! */
	    if((line++ && !so_puts(so, ";\015\012        "))
	       || !so_puts(so, attrib))
		return(0);

	    if(value){
		if(((value[i] || line > 1) /* more lines or already lines */
		    && !(so_writec('*', so)
			 && so_puts(so, int2string(line - 1))))
		   || (encode && !so_writec('*', so))
		   || !so_writec('=', so)
		   || (quote && !so_writec('\"', so))
		   || ((line == 1 && encode)
		       && !(so_puts(so, ((ps_global->VAR_CHAR_SET
					  && strucmp(ps_global->VAR_CHAR_SET,
						     "us-ascii"))
					    ? ps_global->VAR_CHAR_SET
					    : UNKNOWN_CHARSET))
			     && so_puts(so, "''"))))
		  return(0);

		while(i--){
		    if(*value & 0x80){
			char tmp[3], *p;

			p = tmp;
			C2XPAIR(*value, p);
			*p = '\0';
			if(!(so_writec('%', so) && so_puts(so, tmp)))
			  return(0);
		    }
		    else if(((*value == '\\' || *value == '\"')
			     && !so_writec('\\', so))
			    || !so_writec(*value, so))
		      return(0);

		    value++;
		}

		if(quote && !so_writec('\"', so))
		  return(0);

		if(*value)			/* more? */
		  i = quote = 0;		/* reset! */
		else
		  return(1);			/* done! */
	    }
	    else
	      return(1);
	}

	if(!quote && strchr(specials, value[i]))
	  quote++;
    }
}


PARMLIST_S *
rfc2231_newparmlist(params)
    PARAMETER *params;
{
    PARMLIST_S *p = NULL;

    if(params){
	p = (PARMLIST_S *) fs_get(sizeof(PARMLIST_S));
	memset(p, 0, sizeof(PARMLIST_S));
	p->list = params;
    }

    return(p);
}


void
rfc2231_free_parmlist(p)
    PARMLIST_S **p;
{
    if(*p){
	if((*p)->value)
	  fs_give((void **) &(*p)->value);

	mail_free_body_parameter(&(*p)->seen);
	fs_give((void **) p);
    }
}


int
rfc2231_list_params(plist)
    PARMLIST_S *plist;
{
    PARAMETER *pp, **ppp;
    int	       i;

    if(plist->value)
      fs_give((void **) &plist->value);

    for(pp = plist->list; pp; pp = pp->next){
      /* get a name */
      for(i = 0; i < 32; i++)
	if(!(plist->attrib[i] = pp->attribute[i]) ||  pp->attribute[i] == '*'){
	    plist->attrib[i] = '\0';

	    for(ppp = &plist->seen;
		*ppp && strucmp((*ppp)->attribute, plist->attrib);
		ppp = &(*ppp)->next)
	      ;

	    if(!*ppp){
		plist->list = pp->next;
		*ppp = mail_newbody_parameter();	/* add to seen list */
		(*ppp)->attribute = cpystr(plist->attrib);
		plist->value = rfc2231_get_param(pp,plist->attrib,NULL,NULL);
		return(TRUE);
	    }

	    break;
	}
      if(i >= 32)
	q_status_message1(SM_ORDER | SM_DING, 0, 3,
		       "Overly long attachment parameter ignored: %.25s...",
			  pp->attribute);
    }

    return(FALSE);
}



/*
 * These are the global pattern handles which all of the pattern routines
 * use. Once we open one of these we usually leave it open until exiting
 * pine. The _any versions are only used if we are altering our configuration,
 * the _ne (NonEmpty) versions are used routinely. We open the patterns by
 * calling either nonempty_patterns (normal use) or any_patterns (config).
 *
 * There are eight different pinerc variables which contain patterns. They are
 * patterns-filters2, patterns-roles, patterns-scores2, patterns-indexcolors,
 * patterns-other, and the old patterns, patterns-filters, and patterns-other.
 * The first five are the active patterns variables and the old variable are
 * kept around so that we can convert old patterns to new. The reason we
 * split it into five separate variables is so that each can independently
 * be controlled by the main pinerc or by the exception pinerc. The reason
 * for the change to filters2 and scores2 was so we could change the semantics
 * of how rules work when there are pieces in the rule that we don't
 * understand. We added a rule to detect 8bitSubjects. So a user might have
 * a filter that deletes messages with 8bitSubjects. The problem was that
 * that same filter in a old patterns-filters pine would match because it
 * would ignore the 8bitSubject part of the pattern and match on the rest.
 * So we changed the semantics so that rules with unknown pieces would be
 * ignored instead of used. We had to change variable names at the same time
 * because we were adding the 8bit thing and the old pines are still out
 * there. Filters and Scores can both be dangerous. Roles, Colors, and Other
 * seem less dangerous so not worth adding a new variable for them.
 *
 * Each of the eight variables has its own handle and status variables below.
 * That means that they operate independently.
 *
 * Looking at just a single one of those variables, it has four possible
 * values. In normal use, we use the current_val of the variable to set
 * up the patterns. We do that by calling nonempty_patterns with the
 * appropriate rflags. When editing configurations, we have the other two
 * variables to deal with: main_user_val  and post_user_val.
 * We only ever deal with one of those at a time, so we re-use the variables.
 * However, we do sometimes want to deal with one of those and at the same
 * time refer to the current current_val. For example, if we are editing
 * the post or main user_val for the filters variable, we still want
 * to check for new mail. If we find new mail we'll want to call
 * process_filter_patterns which uses the current_val for filter patterns.
 * That means we have to provide for the case where we are using current_val
 * at the same time as we're using one of the user_vals. That's why we have
 * both the _ne variables (NonEmpty) and the _any variables.
 *
 * In any_patterns (and first_pattern...) use_flags may only be set to
 * one value at a time, whereas rflags may be more than one value OR'd together.
 */
PAT_HANDLE	       **cur_pat_h;
static PAT_HANDLE	*pattern_h_roles_ne,    *pattern_h_roles_any,
			*pattern_h_scores_ne,   *pattern_h_scores_any,
			*pattern_h_filts_ne,    *pattern_h_filts_any,
			*pattern_h_incol_ne,    *pattern_h_incol_any,
			*pattern_h_other_ne,    *pattern_h_other_any,
			*pattern_h_oldpat_ne,   *pattern_h_oldpat_any,
			*pattern_h_oldfilt_ne,  *pattern_h_oldfilt_any,
			*pattern_h_oldscore_ne, *pattern_h_oldscore_any;

/*
 * These contain the PAT_OPEN_MASK open status and the PAT_USE_MASK use status.
 */
static long		*cur_pat_status;
static long	  	 pat_status_roles_ne,    pat_status_roles_any,
			 pat_status_scores_ne,   pat_status_scores_any,
			 pat_status_filts_ne,    pat_status_filts_any,
			 pat_status_incol_ne,    pat_status_incol_any,
			 pat_status_other_ne,    pat_status_other_any,
			 pat_status_oldpat_ne,   pat_status_oldpat_any,
			 pat_status_oldfilt_ne,  pat_status_oldfilt_any,
			 pat_status_oldscore_ne, pat_status_oldscore_any;

#define SET_PATTYPE(rflags)						\
    set_pathandle(rflags);						\
    cur_pat_status =							\
      ((rflags) & PAT_USE_CURRENT)					\
	? (((rflags) & ROLE_DO_INCOLS) ? &pat_status_incol_ne :		\
	    ((rflags) & ROLE_DO_OTHER)  ? &pat_status_other_ne :	\
	     ((rflags) & ROLE_DO_FILTER) ? &pat_status_filts_ne :	\
	      ((rflags) & ROLE_DO_SCORES) ? &pat_status_scores_ne :	\
	       ((rflags) & ROLE_DO_ROLES)  ? &pat_status_roles_ne :	\
	        ((rflags) & ROLE_OLD_FILT)  ? &pat_status_oldfilt_ne :	\
	         ((rflags) & ROLE_OLD_SCORE) ? &pat_status_oldscore_ne :\
					        &pat_status_oldpat_ne)	\
	: (((rflags) & ROLE_DO_INCOLS) ? &pat_status_incol_any :	\
	    ((rflags) & ROLE_DO_OTHER)  ? &pat_status_other_any :	\
	     ((rflags) & ROLE_DO_FILTER) ? &pat_status_filts_any :	\
	      ((rflags) & ROLE_DO_SCORES) ? &pat_status_scores_any :	\
	       ((rflags) & ROLE_DO_ROLES)  ? &pat_status_roles_any :	\
	        ((rflags) & ROLE_OLD_FILT)  ? &pat_status_oldfilt_any :	\
	         ((rflags) & ROLE_OLD_SCORE) ? &pat_status_oldscore_any:\
					        &pat_status_oldpat_any);
#define CANONICAL_RFLAGS(rflags)	\
    ((((rflags) & (ROLE_DO_ROLES | ROLE_REPLY | ROLE_FORWARD | ROLE_COMPOSE)) \
					? ROLE_DO_ROLES  : 0) |		   \
     (((rflags) & (ROLE_DO_INCOLS | ROLE_INCOL))			   \
					? ROLE_DO_INCOLS : 0) |		   \
     (((rflags) & (ROLE_DO_SCORES | ROLE_SCORE))			   \
					? ROLE_DO_SCORES : 0) |		   \
     (((rflags) & (ROLE_DO_FILTER))					   \
					? ROLE_DO_FILTER : 0) |		   \
     (((rflags) & (ROLE_DO_OTHER))					   \
					? ROLE_DO_OTHER  : 0) |		   \
     (((rflags) & (ROLE_OLD_FILT))					   \
					? ROLE_OLD_FILT  : 0) |		   \
     (((rflags) & (ROLE_OLD_SCORE))					   \
					? ROLE_OLD_SCORE : 0) |		   \
     (((rflags) & (ROLE_OLD_PAT))					   \
					? ROLE_OLD_PAT  : 0))

#define SETPGMSTATUS(val,yes,no)	\
    switch(val){			\
      case PAT_STAT_YES:		\
	(yes) = 1;			\
	break;				\
      case PAT_STAT_NO:			\
	(no) = 1;			\
	break;				\
      case PAT_STAT_EITHER:		\
      default:				\
        break;				\
    }

#define SET_STATUS(srchin,srchfor,assignto)				\
    {char *qq, *pp;							\
     int   ii;								\
     NAMEVAL_S *vv;							\
     if((qq = srchstr(srchin, srchfor)) != NULL){			\
	if((pp = remove_pat_escapes(qq+strlen(srchfor))) != NULL){	\
	    for(ii = 0; vv = role_status_types(ii); ii++)		\
	      if(!strucmp(pp, vv->shortname)){				\
		  assignto = vv->value;					\
		  break;						\
	      }								\
									\
	    fs_give((void **)&pp);					\
	}								\
     }									\
    }

#define SET_MSGSTATE(srchin,srchfor,assignto)				\
    {char *qq, *pp;							\
     int   ii;								\
     NAMEVAL_S *vv;							\
     if((qq = srchstr(srchin, srchfor)) != NULL){			\
	if((pp = remove_pat_escapes(qq+strlen(srchfor))) != NULL){	\
	    for(ii = 0; vv = msg_state_types(ii); ii++)			\
	      if(!strucmp(pp, vv->shortname)){				\
		  assignto = vv->value;					\
		  break;						\
	      }								\
									\
	    fs_give((void **)&pp);					\
	}								\
     }									\
    }

#define PATTERN_N (8)


void
set_pathandle(rflags)
    long rflags;
{
    cur_pat_h = (rflags & PAT_USE_CURRENT)
		? ((rflags & ROLE_DO_INCOLS) ? &pattern_h_incol_ne :
		    (rflags & ROLE_DO_OTHER)  ? &pattern_h_other_ne :
		     (rflags & ROLE_DO_FILTER) ? &pattern_h_filts_ne :
		      (rflags & ROLE_DO_SCORES) ? &pattern_h_scores_ne :
		       (rflags & ROLE_DO_ROLES)  ? &pattern_h_roles_ne :
					            &pattern_h_oldpat_ne)
	        : ((rflags & ROLE_DO_INCOLS) ? &pattern_h_incol_any :
		    (rflags & ROLE_DO_OTHER)  ? &pattern_h_other_any :
		     (rflags & ROLE_DO_FILTER) ? &pattern_h_filts_any :
		      (rflags & ROLE_DO_SCORES) ? &pattern_h_scores_any :
		       (rflags & ROLE_DO_ROLES)  ? &pattern_h_roles_any :
					            &pattern_h_oldpat_any);
}


/*
 * Rflags may be more than one pattern type OR'd together. It also contains
 * the "use" parameter.
 */
void
open_any_patterns(rflags)
    long rflags;
{
    long canon_rflags;

    dprint(7, (debugfile, "open_any_patterns(0x%x)\n", rflags));

    canon_rflags = CANONICAL_RFLAGS(rflags);

    if(canon_rflags & ROLE_DO_INCOLS)
      sub_open_any_patterns(ROLE_DO_INCOLS | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_FILTER)
      sub_open_any_patterns(ROLE_DO_FILTER | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_OTHER)
      sub_open_any_patterns(ROLE_DO_OTHER  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_SCORES)
      sub_open_any_patterns(ROLE_DO_SCORES | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_ROLES)
      sub_open_any_patterns(ROLE_DO_ROLES  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_FILT)
      sub_open_any_patterns(ROLE_OLD_FILT  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_SCORE)
      sub_open_any_patterns(ROLE_OLD_SCORE | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_PAT)
      sub_open_any_patterns(ROLE_OLD_PAT   | (rflags & PAT_USE_MASK));
}


/*
 * This should only be called with a single pattern type (plus use flags).
 * We assume that patterns of this type are closed before this is called.
 * This always succeeds unless we run out of memory, in which case fs_get
 * never returns.
 */
void
sub_open_any_patterns(rflags)
    long rflags;
{
    PAT_LINE_S *patline = NULL, *pl = NULL;
    char      **t = NULL;
    struct variable *var;

    SET_PATTYPE(rflags);

    *cur_pat_h = (PAT_HANDLE *)fs_get(sizeof(**cur_pat_h));
    memset((void *)*cur_pat_h, 0, sizeof(**cur_pat_h));

    if(rflags & ROLE_DO_ROLES)
      var = &ps_global->vars[V_PAT_ROLES];
    else if(rflags & ROLE_DO_FILTER)
      var = &ps_global->vars[V_PAT_FILTS];
    else if(rflags & ROLE_DO_OTHER)
      var = &ps_global->vars[V_PAT_OTHER];
    else if(rflags & ROLE_DO_SCORES)
      var = &ps_global->vars[V_PAT_SCORES];
    else if(rflags & ROLE_DO_INCOLS)
      var = &ps_global->vars[V_PAT_INCOLS];
    else if(rflags & ROLE_OLD_FILT)
      var = &ps_global->vars[V_PAT_FILTS_OLD];
    else if(rflags & ROLE_OLD_SCORE)
      var = &ps_global->vars[V_PAT_SCORES_OLD];
    else if(rflags & ROLE_OLD_PAT)
      var = &ps_global->vars[V_PATTERNS];

    switch(rflags & PAT_USE_MASK){
      case PAT_USE_CURRENT:
	t = var->current_val.l;
	break;
      case PAT_USE_MAIN:
	t = var->main_user_val.l;
	break;
      case PAT_USE_POST:
	t = var->post_user_val.l;
	break;
    }

    if(t){
	for(; t[0] && t[0][0]; t++){
	    if(*t && !strncmp("LIT:", *t, 4))
	      patline = parse_pat_lit(*t + 4);
	    else if(*t && !strncmp("FILE:", *t, 5))
	      patline = parse_pat_file(*t + 5);
	    else if(rflags & (PAT_USE_MAIN | PAT_USE_POST) &&
		    patline == NULL && *t && !strcmp(INHERIT, *t))
	      patline = parse_pat_inherit();
	    else
	      patline = NULL;

	    if(patline){
		if(pl){
		    pl->next      = patline;
		    patline->prev = pl;
		    pl = pl->next;
		}
		else{
		    (*cur_pat_h)->patlinehead = patline;
		    pl = patline;
		}
	    }
	    else
	      q_status_message1(SM_ORDER, 0, 3,
				"Invalid patterns line \"%.200s\"", *t);
	}
    }

    *cur_pat_status = PAT_OPENED | (rflags & PAT_USE_MASK);
}


void
close_every_pattern()
{
    close_patterns(ROLE_DO_INCOLS | ROLE_DO_FILTER | ROLE_DO_SCORES
		   | ROLE_DO_OTHER | ROLE_DO_ROLES
		   | ROLE_OLD_FILT | ROLE_OLD_SCORE | ROLE_OLD_PAT
		   | PAT_USE_CURRENT);
    /*
     * Since there is only one set of variables for the other three uses
     * we can just close any one of them. There can only be one open at
     * a time.
     */
    close_patterns(ROLE_DO_INCOLS | ROLE_DO_FILTER | ROLE_DO_SCORES
		   | ROLE_DO_OTHER | ROLE_DO_ROLES
		   | ROLE_OLD_FILT | ROLE_OLD_SCORE | ROLE_OLD_PAT
		   | PAT_USE_MAIN);
}


/*
 * Can be called with more than one pattern type.
 */
void
close_patterns(rflags)
    long rflags;
{
    long canon_rflags;

    dprint(7, (debugfile, "close_patterns(0x%x)\n", rflags));

    canon_rflags = CANONICAL_RFLAGS(rflags);

    if(canon_rflags & ROLE_DO_INCOLS)
      sub_close_patterns(ROLE_DO_INCOLS | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_OTHER)
      sub_close_patterns(ROLE_DO_OTHER  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_FILTER)
      sub_close_patterns(ROLE_DO_FILTER | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_SCORES)
      sub_close_patterns(ROLE_DO_SCORES | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_ROLES)
      sub_close_patterns(ROLE_DO_ROLES  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_FILT)
      sub_close_patterns(ROLE_OLD_FILT  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_SCORE)
      sub_close_patterns(ROLE_OLD_SCORE | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_PAT)
      sub_close_patterns(ROLE_OLD_PAT   | (rflags & PAT_USE_MASK));
}


/*
 * Can be called with only a single pattern type.
 */
void
sub_close_patterns(rflags)
    long rflags;
{
    SET_PATTYPE(rflags);

    if(*cur_pat_h != NULL){
	free_patline(&(*cur_pat_h)->patlinehead);
	fs_give((void **)cur_pat_h);
    }

    *cur_pat_status = PAT_CLOSED;

    scores_are_used(SCOREUSE_INVALID);
}


/*
 * Can be called with more than one pattern type.
 * Nonempty always uses PAT_USE_CURRENT (the current_val).
 */
int
nonempty_patterns(rflags, pstate)
    long       rflags;
    PAT_STATE *pstate;
{
    return(any_patterns((rflags & ROLE_MASK) | PAT_USE_CURRENT, pstate));
}


/*
 * Initializes pstate and parses and sets up appropriate pattern variables.
 * May be called with more than one pattern type OR'd together in rflags.
 * Pstate will keep track of that and next_pattern et. al. will increment
 * through all of those pattern types.
 */
int
any_patterns(rflags, pstate)
    long       rflags;
    PAT_STATE *pstate;
{
    int  ret = 0;
    long canon_rflags;

    dprint(7, (debugfile, "any_patterns(0x%x)\n", rflags));

    memset((void *)pstate, 0, sizeof(*pstate));
    pstate->rflags    = rflags;

    canon_rflags = CANONICAL_RFLAGS(pstate->rflags);

    if(canon_rflags & ROLE_DO_INCOLS)
      ret += sub_any_patterns(ROLE_DO_INCOLS, pstate);
    if(canon_rflags & ROLE_DO_OTHER)
      ret += sub_any_patterns(ROLE_DO_OTHER, pstate);
    if(canon_rflags & ROLE_DO_FILTER)
      ret += sub_any_patterns(ROLE_DO_FILTER, pstate);
    if(canon_rflags & ROLE_DO_SCORES)
      ret += sub_any_patterns(ROLE_DO_SCORES, pstate);
    if(canon_rflags & ROLE_DO_ROLES)
      ret += sub_any_patterns(ROLE_DO_ROLES, pstate);
    if(canon_rflags & ROLE_OLD_FILT)
      ret += sub_any_patterns(ROLE_OLD_FILT, pstate);
    if(canon_rflags & ROLE_OLD_SCORE)
      ret += sub_any_patterns(ROLE_OLD_SCORE, pstate);
    if(canon_rflags & ROLE_OLD_PAT)
      ret += sub_any_patterns(ROLE_OLD_PAT, pstate);

    return(ret);
}


int
sub_any_patterns(rflags, pstate)
    long       rflags;
    PAT_STATE *pstate;
{
    SET_PATTYPE(rflags | (pstate->rflags & PAT_USE_MASK));

    if(*cur_pat_h &&
       (((pstate->rflags & PAT_USE_MASK) == PAT_USE_CURRENT &&
	 (*cur_pat_status & PAT_USE_MASK) != PAT_USE_CURRENT) ||
        ((pstate->rflags & PAT_USE_MASK) != PAT_USE_CURRENT &&
         ((*cur_pat_status & PAT_OPEN_MASK) != PAT_OPENED ||
	  (*cur_pat_status & PAT_USE_MASK) !=
	   (pstate->rflags & PAT_USE_MASK)))))
      close_patterns(rflags | (pstate->rflags & PAT_USE_MASK));
    
    /* open_any always succeeds */
    if(!*cur_pat_h && ((*cur_pat_status & PAT_OPEN_MASK) == PAT_CLOSED))
      open_any_patterns(rflags | (pstate->rflags & PAT_USE_MASK));
    
    if(!*cur_pat_h){		/* impossible */
	*cur_pat_status = PAT_CLOSED;
	return(0);
    }

    /*
     * Opening nonempty can fail. That just means there aren't any
     * patterns of that type.
     */
    if((pstate->rflags & PAT_USE_MASK) == PAT_USE_CURRENT &&
       !(*cur_pat_h)->patlinehead)
      *cur_pat_status = (PAT_OPEN_FAILED | PAT_USE_CURRENT);
       
    return(((*cur_pat_status & PAT_OPEN_MASK) == PAT_OPENED) ? 1 : 0);
}


PAT_LINE_S *
parse_pat_lit(litpat)
    char *litpat;
{
    PAT_LINE_S *patline;
    PAT_S      *pat;

    patline = (PAT_LINE_S *)fs_get(sizeof(*patline));
    memset((void *)patline, 0, sizeof(*patline));
    patline->type = Literal;


    if((pat = parse_pat(litpat)) != NULL){
	pat->patline   = patline;
	patline->first = pat;
	patline->last  = pat;
    }

    return(patline);
}


/*
 * This always returns a patline even if we can't read the file. The patline
 * returned will say readonly in the worst case and there will be no patterns.
 * If the file doesn't exist, this creates it if possible.
 */
PAT_LINE_S *
parse_pat_file(filename)
    char *filename;
{
#define BUF_SIZE 5000
    PAT_LINE_S *patline;
    PAT_S      *pat, *p;
    char        path[MAXPATH+1], buf[BUF_SIZE];
    char       *dir, *q;
    FILE       *fp;
    int         ok = 0, some_pats = 0;
    struct variable *vars = ps_global->vars;

    signature_path(filename, path, MAXPATH);

    if(VAR_OPER_DIR && !in_dir(VAR_OPER_DIR, path)){
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  "Can't use Roles file outside of %.200s",
			  VAR_OPER_DIR);
	return(NULL);
    }

    patline = (PAT_LINE_S *)fs_get(sizeof(*patline));
    memset((void *)patline, 0, sizeof(*patline));
    patline->type     = File;
    patline->filename = cpystr(filename);
    patline->filepath = cpystr(path);

    if(q = last_cmpnt(path)){
	int save;

	save = *--q;
	*q = '\0';
	dir  = cpystr(*path ? path : "/");
	*q = save;
    }
    else
      dir = cpystr(".");

#if	defined(DOS) || defined(OS2)
    /*
     * If the dir has become a drive letter and : (e.g. "c:")
     * then append a "\".  The library function access() in the
     * win 16 version of MSC seems to require this.
     */
    if(isalpha((unsigned char) *dir)
       && *(dir+1) == ':' && *(dir+2) == '\0'){
	*(dir+2) = '\\';
	*(dir+3) = '\0';
    }
#endif	/* DOS || OS2 */

    /*
     * Even if we can edit the file itself, we aren't going
     * to be able to change it unless we can also write in
     * the directory that contains it (because we write into a
     * temp file and then rename).
     */
    if(can_access(dir, EDIT_ACCESS) != 0)
      patline->readonly = 1;

    if(can_access(path, EDIT_ACCESS) == 0){
	if(patline->readonly)
	  q_status_message1(SM_ORDER, 0, 3,
			    "Pattern file directory (%.200s) is ReadOnly", dir);
    }
    else if(can_access(path, READ_ACCESS) == 0)
      patline->readonly = 1;

    if(can_access(path, ACCESS_EXISTS) == 0){
	if((fp = fopen(path, "r")) != NULL){
	    /* Check to see if this is a valid patterns file */
	    if(fp_file_size(fp) <= 0L)
	      ok++;
	    else{
		size_t len;

		len = strlen(PATTERN_MAGIC);
	        if(fread(buf, sizeof(char), len+3, fp) == len+3){
		    buf[len+3] = '\0';
		    buf[len] = '\0';
		    if(strcmp(buf, PATTERN_MAGIC) == 0){
			if(atoi(PATTERN_FILE_VERS) < atoi(buf + len + 1))
			  q_status_message1(SM_ORDER, 0, 4,
  "Pattern file \"%.200s\" is made by newer Pine, will try to use it anyway",
					    filename);

			ok++;
			some_pats++;
			/* toss rest of first line */
			(void)fgets(buf, BUF_SIZE, fp);
		    }
		}
	    }
		
	    if(!ok){
		patline->readonly = 1;
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  "\"%.200s\" is not a Pattern file", path);
	    }

	    p = NULL;
	    while(some_pats && fgets(buf, BUF_SIZE, fp) != NULL){
		if((pat = parse_pat(buf)) != NULL){
		    pat->patline = patline;
		    if(!patline->first)
		      patline->first = pat;

		    patline->last  = pat;

		    if(p){
			p->next   = pat;
			pat->prev = p;
			p = p->next;
		    }
		    else
		      p = pat;
		}
	    }

	    (void)fclose(fp);
	}
	else{
	    patline->readonly = 1;
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Error \"%.200s\" reading pattern file \"%.200s\"",
			      error_description(errno), path);
	}
    }
    else{		/* doesn't exist yet, try to create it */
	if(patline->readonly)
	  q_status_message1(SM_ORDER, 0, 3,
			    "Pattern file directory (%.200s) is ReadOnly", dir);
	else{
	    /*
	     * We try to create it by making up an empty patline and calling
	     * write_pattern_file.
	     */
	    patline->dirty = 1;
	    if(write_pattern_file(NULL, patline) != 0){
		patline->readonly = 1;
		patline->dirty = 0;
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  "Error creating pattern file \"%.200s\"",
				  path);
	    }
	}
    }

    if(dir)
      fs_give((void **)&dir);

    return(patline);
}


PAT_LINE_S *
parse_pat_inherit()
{
    PAT_LINE_S *patline;
    PAT_S      *pat;

    patline = (PAT_LINE_S *)fs_get(sizeof(*patline));
    memset((void *)patline, 0, sizeof(*patline));
    patline->type = Inherit;

    pat = (PAT_S *)fs_get(sizeof(*pat));
    memset((void *)pat, 0, sizeof(*pat));
    pat->inherit = 1;

    pat->patline = patline;
    patline->first = pat;
    patline->last  = pat;

    return(patline);
}


/*
 * There are three forms that a PATTERN_S has at various times. There is
 * the actual PATTERN_S struct which is used internally and is used whenever
 * we are actually doing something with the pattern, like filtering or
 * something. There is the version that goes in the config file. And there
 * is the version the user edits.
 *
 * To go between these three forms we have the helper routines
 * 
 *   pattern_to_config
 *   config_to_pattern
 *   pattern_to_editlist
 *   editlist_to_pattern
 *
 * Here's what is supposed to be happening. A PATTERN_S is a linked list
 * of strings with nothing escaped. That is, a backslash or a comma is
 * just in there as a backslash or comma.
 *
 * The version the user edits is very similar. Because we have historically
 * used commas as separators the user has always had to enter a \, in order
 * to put a real comma in one of the items. That is the only difference
 * between a PATTERN_S string and the editlist strings. Note that backslashes
 * themselves are not escaped. A backslash which is not followed by a comma
 * is a backslash. It doesn't escape the following character. That's a bit
 * odd, it is that way because most people will never know about this
 * backslash stuff but PC-Pine users may have backslashes in folder names.
 *
 * The version that goes in the config file has a few transformations made.
 *      PATTERN_S   intermediate_form   Config string
 *         ,              \,               \x2C
 *         \              \\               \x5C
 *         /                               \/
 *         "                               \"
 *
 * The commas are turned into hex commas so that we can tell the separators
 * in the comma-separated lists from those commas.
 * The backslashes are escaped because they escape commas.
 * The /'s are escaped because they separate pattern pieces.
 * The "'s are escaped because they are significant to parse_list when
 *   parsing the config file.
 *                            hubert - 2004-04-01
 *                                     (date is only coincidental!)
 *
 * Addendum. The not's are handled separately from all the strings. Not sure
 * why that is or if there is a good reason. Nevertheless, now is not the
 * time to figure it out so leave it that way.
 *                            hubert - 2004-07-14
 */
PAT_S *
parse_pat(str)
    char *str;
{
    PAT_S *pat = NULL;
    char  *p, *q, *astr, *pstr;
    int    backslashed;
#define PTRN "pattern="
#define PTRNLEN 8
#define ACTN "action="
#define ACTNLEN 7

    if(str)
      removing_trailing_white_space(str);

    if(!str || !*str || *str == '#')
      return(pat);

    pat = (PAT_S *)fs_get(sizeof(*pat));
    memset((void *)pat, 0, sizeof(*pat));

    if((p = srchstr(str, PTRN)) != NULL){
	pat->patgrp = (PATGRP_S *)fs_get(sizeof(*pat->patgrp));
	memset((void *)pat->patgrp, 0, sizeof(*pat->patgrp));
	pat->patgrp->fldr_type = FLDR_DEFL;
	pat->patgrp->abookfrom = AFRM_DEFL;
	pat->patgrp->cat_lim   = -1L;

	if((pstr = copy_quoted_string_asis(p+PTRNLEN)) != NULL){
	    /* move to next slash */
	    for(q=pstr, backslashed=0; *q; q++){
		switch(*q){
		  case '\\':
		    backslashed = !backslashed;
		    break;

		  case '/':
		    if(!backslashed){
			parse_patgrp_slash(q, pat->patgrp);
			if(pat->patgrp->bogus && !pat->raw)
			  pat->raw = cpystr(str);
		    }

		  /* fall through */

		  default:
		    backslashed = 0;
		    break;
		}
	    }

	    /* we always force a nickname */
	    if(!pat->patgrp->nick)
	      pat->patgrp->nick = cpystr("Alternate Role");

	    fs_give((void **)&pstr);
	}
    }

    if((p = srchstr(str, ACTN)) != NULL){
	pat->action = (ACTION_S *)fs_get(sizeof(*pat->action));
	memset((void *)pat->action, 0, sizeof(*pat->action));
	pat->action->startup_rule = IS_NOTSET;
	pat->action->repl_type = ROLE_REPL_DEFL;
	pat->action->forw_type = ROLE_FORW_DEFL;
	pat->action->comp_type = ROLE_COMP_DEFL;
	pat->action->nick = cpystr((pat->patgrp && pat->patgrp->nick
				    && pat->patgrp->nick[0])
				       ? pat->patgrp->nick : "Alternate Role");

	if((astr = copy_quoted_string_asis(p+ACTNLEN)) != NULL){
	    /* move to next slash */
	    for(q=astr, backslashed=0; *q; q++){
		switch(*q){
		  case '\\':
		    backslashed = !backslashed;
		    break;

		  case '/':
		    if(!backslashed){
			parse_action_slash(q, pat->action);
			if(pat->action->bogus && !pat->raw)
			  pat->raw = cpystr(str);
		    }

		  /* fall through */

		  default:
		    backslashed = 0;
		    break;
		}
	    }

	    fs_give((void **)&astr);

	    if(!pat->action->is_a_score)
	      pat->action->scoreval = 0L;
	    
	    if(pat->action->is_a_filter)
	      pat->action->kill = (pat->action->folder
				   || pat->action->kill == -1) ? 0 : 1;
	    else{
		if(pat->action->folder)
		  free_pattern(&pat->action->folder);
	    }

	    if(!pat->action->is_a_role){
		pat->action->repl_type = ROLE_NOTAROLE_DEFL;
		pat->action->forw_type = ROLE_NOTAROLE_DEFL;
		pat->action->comp_type = ROLE_NOTAROLE_DEFL;
		if(pat->action->from)
		  mail_free_address(&pat->action->from);
		if(pat->action->replyto)
		  mail_free_address(&pat->action->replyto);
		if(pat->action->fcc)
		  fs_give((void **)&pat->action->fcc);
		if(pat->action->litsig)
		  fs_give((void **)&pat->action->litsig);
		if(pat->action->sig)
		  fs_give((void **)&pat->action->sig);
		if(pat->action->template)
		  fs_give((void **)&pat->action->template);
		if(pat->action->cstm)
		  free_list_array(&pat->action->cstm);
		if(pat->action->smtp)
		  free_list_array(&pat->action->smtp);
		if(pat->action->nntp)
		  free_list_array(&pat->action->nntp);
		if(pat->action->inherit_nick)
		  fs_give((void **)&pat->action->inherit_nick);
	    }

	    if(!pat->action->is_a_incol){
		if(pat->action->incol)
		  free_color_pair(&pat->action->incol);
	    }

	    if(!pat->action->is_a_other){
		pat->action->sort_is_set = 0;
		pat->action->sortorder = 0;
		pat->action->revsort = 0;
		pat->action->startup_rule = IS_NOTSET;
		if(pat->action->index_format)
		  fs_give((void **)&pat->action->index_format);
	    }
	}
    }

    return(pat);
}


/*
 * Fill in one member of patgrp from str.
 *
 * The multiple constant strings are lame but it evolved this way from
 * previous versions and isn't worth fixing.
 */
void
parse_patgrp_slash(str, patgrp)
    char *str;
    PATGRP_S *patgrp;
{
    char  *p;

    if(!patgrp)
      panic("NULL patgrp to parse_patgrp_slash");
    else if(!(str && *str)){
	panic("NULL or empty string to parse_patgrp_slash");
	patgrp->bogus = 1;
    }
    else if(!strncmp(str, "/NICK=", 6))
      patgrp->nick = remove_pat_escapes(str+6);
    else if(!strncmp(str, "/COMM=", 6))
      patgrp->comment = remove_pat_escapes(str+6);
    else if(!strncmp(str, "/TO=", 4) || !strncmp(str, "/!TO=", 5))
      patgrp->to = parse_pattern("TO", str, 1);
    else if(!strncmp(str, "/CC=", 4) || !strncmp(str, "/!CC=", 5))
      patgrp->cc = parse_pattern("CC", str, 1);
    else if(!strncmp(str, "/RECIP=", 7) || !strncmp(str, "/!RECIP=", 8))
      patgrp->recip = parse_pattern("RECIP", str, 1);
    else if(!strncmp(str, "/PARTIC=", 8) || !strncmp(str, "/!PARTIC=", 9))
      patgrp->partic = parse_pattern("PARTIC", str, 1);
    else if(!strncmp(str, "/FROM=", 6) || !strncmp(str, "/!FROM=", 7))
      patgrp->from = parse_pattern("FROM", str, 1);
    else if(!strncmp(str, "/SENDER=", 8) || !strncmp(str, "/!SENDER=", 9))
      patgrp->sender = parse_pattern("SENDER", str, 1);
    else if(!strncmp(str, "/NEWS=", 6) || !strncmp(str, "/!NEWS=", 7))
      patgrp->news = parse_pattern("NEWS", str, 1);
    else if(!strncmp(str, "/SUBJ=", 6) || !strncmp(str, "/!SUBJ=", 7))
      patgrp->subj = parse_pattern("SUBJ", str, 1);
    else if(!strncmp(str, "/ALL=", 5) || !strncmp(str, "/!ALL=", 6))
      patgrp->alltext = parse_pattern("ALL", str, 1);
    else if(!strncmp(str, "/BODY=", 6) || !strncmp(str, "/!BODY=", 7))
      patgrp->bodytext = parse_pattern("BODY", str, 1);
    else if(!strncmp(str, "/KEY=", 5) || !strncmp(str, "/!KEY=", 6))
      patgrp->keyword = parse_pattern("KEY", str, 1);
    else if(!strncmp(str, "/CHAR=", 6) || !strncmp(str, "/!CHAR=", 7))
      patgrp->charsets = parse_pattern("CHAR", str, 1);
    else if(!strncmp(str, "/FOLDER=", 8) || !strncmp(str, "/!FOLDER=", 9))
      patgrp->folder = parse_pattern("FOLDER", str, 1);
    else if(!strncmp(str, "/ABOOKS=", 8) || !strncmp(str, "/!ABOOKS=", 9))
      patgrp->abooks = parse_pattern("ABOOKS", str, 1);
    /*
     * A problem with arbhdrs is that more than one of them can appear in
     * the string. We come back here the second time, but we already took
     * care of the whole thing on the first pass. Hence the check for
     * arbhdr already set.
     */
    else if(!strncmp(str, "/ARB", 4) || !strncmp(str, "/!ARB", 5)
	    || !strncmp(str, "/EARB", 5) || !strncmp(str, "/!EARB", 6)){
	if(!patgrp->arbhdr)
	  patgrp->arbhdr = parse_arbhdr(str);
	/* else do nothing */
    }
    else if(!strncmp(str, "/SENTDATE=", 10))
      patgrp->age_uses_sentdate = 1;
    else if(!strncmp(str, "/SCOREI=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){
	    if((patgrp->score = parse_intvl(p)) != NULL)
	      patgrp->do_score = 1;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/AGE=", 5)){
	if((p = remove_pat_escapes(str+5)) != NULL){
	    if((patgrp->age = parse_intvl(p)) != NULL)
	      patgrp->do_age  = 1;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/SIZE=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    if((patgrp->size = parse_intvl(p)) != NULL)
	      patgrp->do_size  = 1;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/CATCMD=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){
	    int   commas = 0;
	    char *q;

	    /* count elements in list */
	    for(q = p; q && *q; q++)
	      if(*q == ',')
		commas++;

	    patgrp->category_cmd = parse_list(p, commas+1, PL_REMSURRQUOT,NULL);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/CATVAL=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){
	    if((patgrp->cat = parse_intvl(p)) != NULL)
	      patgrp->do_cat = 1;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/CATLIM=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){
	    long i;

	    i = atol(p);
	    patgrp->cat_lim = i;
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/FLDTYPE=", 9)){
	if((p = remove_pat_escapes(str+9)) != NULL){
	    int        i;
	    NAMEVAL_S *v;

	    for(i = 0; v = pat_fldr_types(i); i++)
	      if(!strucmp(p, v->shortname)){
		  patgrp->fldr_type = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/AFROM=", 7)){
	if((p = remove_pat_escapes(str+7)) != NULL){
	    int        i;
	    NAMEVAL_S *v;

	    for(i = 0; v = abookfrom_fldr_types(i); i++)
	      if(!strucmp(p, v->shortname)){
		  patgrp->abookfrom = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/STATN=", 7)){
	SET_STATUS(str,"/STATN=",patgrp->stat_new);
    }
    else if(!strncmp(str, "/STATR=", 7)){
	SET_STATUS(str,"/STATR=",patgrp->stat_rec);
    }
    else if(!strncmp(str, "/STATI=", 7)){
	SET_STATUS(str,"/STATI=",patgrp->stat_imp);
    }
    else if(!strncmp(str, "/STATA=", 7)){
	SET_STATUS(str,"/STATA=",patgrp->stat_ans);
    }
    else if(!strncmp(str, "/STATD=", 7)){
	SET_STATUS(str,"/STATD=",patgrp->stat_del);
    }
    else if(!strncmp(str, "/8BITS=", 7)){
	SET_STATUS(str,"/8BITS=",patgrp->stat_8bitsubj);
    }
    else if(!strncmp(str, "/BOM=", 5)){
	SET_STATUS(str,"/BOM=",patgrp->stat_bom);
    }
    else if(!strncmp(str, "/BOY=", 5)){
	SET_STATUS(str,"/BOY=",patgrp->stat_boy);
    }
    else{
	char save;

	patgrp->bogus = 1;

	if((p = strindex(str, '=')) != NULL){
	    save = *(p+1);
	    *(p+1) = '\0';
	}

	dprint(1, (debugfile,
	       "parse_patgrp_slash(%.20s): unrecognized in \"%s\"\n",
	       str ? str : "?",
	       (patgrp && patgrp->nick) ? patgrp->nick : ""));
	q_status_message4(SM_ORDER, 1, 3,
	      "Warning: unrecognized pattern element \"%.20s\"%.20s%.20s%.20s",
	      str, patgrp->nick ? " in rule \"" : "",
	      patgrp->nick ? patgrp->nick : "", patgrp->nick ? "\"" : "");

	if(p)
	  *(p+1) = save;
    }
}


/*
 * Fill in one member of action struct from str.
 *
 * The multiple constant strings are lame but it evolved this way from
 * previous versions and isn't worth fixing.
 */
void
parse_action_slash(str, action)
    char *str;
    ACTION_S *action;
{
    char      *p;
    int        stateval, i;
    NAMEVAL_S *v;

    if(!action)
      panic("NULL action to parse_action_slash");
    else if(!(str && *str))
      panic("NULL or empty string to parse_action_slash");
    else if(!strncmp(str, "/ROLE=1", 7))
      action->is_a_role = 1;
    else if(!strncmp(str, "/OTHER=1", 8))
      action->is_a_other = 1;
    else if(!strncmp(str, "/ISINCOL=1", 10))
      action->is_a_incol = 1;
    /*
     * This is unfortunate. If a new filter is set to only set
     * state bits it will be interpreted by an older pine which
     * doesn't have that feature like a filter that is set to Delete.
     * So we change the filter indicator to FILTER=2 to disable the
     * filter for older versions.
     */
    else if(!strncmp(str, "/FILTER=1", 9) || !strncmp(str, "/FILTER=2", 9))
      action->is_a_filter = 1;
    else if(!strncmp(str, "/ISSCORE=1", 10))
      action->is_a_score = 1;
    else if(!strncmp(str, "/SCORE=", 7)){
	if((p = remove_pat_escapes(str+7)) != NULL){
	    long i;

	    i = atol(p);
	    if(i >= SCORE_MIN && i <= SCORE_MAX)
	      action->scoreval = i;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/FOLDER=", 8))
      action->folder = parse_pattern("FOLDER", str, 1);
    else if(!strncmp(str, "/KEYSET=", 8))
      action->keyword_set = parse_pattern("KEYSET", str, 1);
    else if(!strncmp(str, "/KEYCLR=", 8))
      action->keyword_clr = parse_pattern("KEYCLR", str, 1);
    else if(!strncmp(str, "/NOKILL=", 8))
      action->kill = -1;
    else if(!strncmp(str, "/NOTDEL=", 8))
      action->move_only_if_not_deleted = 1;
    else if(!strncmp(str, "/NONTERM=", 9))
      action->non_terminating = 1;
    else if(!strncmp(str, "/STATI=", 7)){
	stateval = ACT_STAT_LEAVE;
	SET_MSGSTATE(str,"/STATI=",stateval);
	switch(stateval){
	  case ACT_STAT_LEAVE:
	    break;
	  case ACT_STAT_SET:
	    action->state_setting_bits |= F_FLAG;
	    break;
	  case ACT_STAT_CLEAR:
	    action->state_setting_bits |= F_UNFLAG;
	    break;
	}
    }
    else if(!strncmp(str, "/STATD=", 7)){
	stateval = ACT_STAT_LEAVE;
	SET_MSGSTATE(str,"/STATD=",stateval);
	switch(stateval){
	  case ACT_STAT_LEAVE:
	    break;
	  case ACT_STAT_SET:
	    action->state_setting_bits |= F_DEL;
	    break;
	  case ACT_STAT_CLEAR:
	    action->state_setting_bits |= F_UNDEL;
	    break;
	}
    }
    else if(!strncmp(str, "/STATA=", 7)){
	stateval = ACT_STAT_LEAVE;
	SET_MSGSTATE(str,"/STATA=",stateval);
	switch(stateval){
	  case ACT_STAT_LEAVE:
	    break;
	  case ACT_STAT_SET:
	    action->state_setting_bits |= F_ANS;
	    break;
	  case ACT_STAT_CLEAR:
	    action->state_setting_bits |= F_UNANS;
	    break;
	}
    }
    else if(!strncmp(str, "/STATN=", 7)){
	stateval = ACT_STAT_LEAVE;
	SET_MSGSTATE(str,"/STATN=",stateval);
	switch(stateval){
	  case ACT_STAT_LEAVE:
	    break;
	  case ACT_STAT_SET:
	    action->state_setting_bits |= F_UNSEEN;
	    break;
	  case ACT_STAT_CLEAR:
	    action->state_setting_bits |= F_SEEN;
	    break;
	}
    }
    else if(!strncmp(str, "/RTYPE=", 7)){
	/* reply type */
	action->repl_type = ROLE_REPL_DEFL;
	if((p = remove_pat_escapes(str+7)) != NULL){
	    for(i = 0; v = role_repl_types(i); i++)
	      if(!strucmp(p, v->shortname)){
		  action->repl_type = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/FTYPE=", 7)){
	/* forward type */
	action->forw_type = ROLE_FORW_DEFL;
	if((p = remove_pat_escapes(str+7)) != NULL){
	    for(i = 0; v = role_forw_types(i); i++)
	      if(!strucmp(p, v->shortname)){
		  action->forw_type = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/CTYPE=", 7)){
	/* compose type */
	action->comp_type = ROLE_COMP_DEFL;
	if((p = remove_pat_escapes(str+7)) != NULL){
	    for(i = 0; v = role_comp_types(i); i++)
	      if(!strucmp(p, v->shortname)){
		  action->comp_type = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/FROM=", 6))
      action->from = configstr_to_address(str+6);
    else if(!strncmp(str, "/REPL=", 6))
      action->replyto = configstr_to_address(str+6);
    else if(!strncmp(str, "/FCC=", 5))
      action->fcc = remove_pat_escapes(str+5);
    else if(!strncmp(str, "/LSIG=", 6))
      action->litsig = remove_pat_escapes(str+6);
    else if(!strncmp(str, "/SIG=", 5))
      action->sig = remove_pat_escapes(str+5);
    else if(!strncmp(str, "/TEMPLATE=", 10))
      action->template = remove_pat_escapes(str+10);
    /* get the custom headers */
    else if(!strncmp(str, "/CSTM=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    int   commas = 0;
	    char *q;

	    /* count elements in list */
	    for(q = p; q && *q; q++)
	      if(*q == ',')
		commas++;

	    action->cstm = parse_list(p, commas+1, 0, NULL);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/SMTP=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    int   commas = 0;
	    char *q;

	    /* count elements in list */
	    for(q = p; q && *q; q++)
	      if(*q == ',')
		commas++;

	    action->smtp = parse_list(p, commas+1, PL_REMSURRQUOT, NULL);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/NNTP=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    int   commas = 0;
	    char *q;

	    /* count elements in list */
	    for(q = p; q && *q; q++)
	      if(*q == ',')
		commas++;

	    action->nntp = parse_list(p, commas+1, PL_REMSURRQUOT, NULL);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/INICK=", 7))
      action->inherit_nick = remove_pat_escapes(str+7);
    else if(!strncmp(str, "/INCOL=", 7)){
	if((p = remove_pat_escapes(str+7)) != NULL){
	    char *fg = NULL, *bg = NULL, *z;

	    /*
	     * Color should look like
	     * /FG=white/BG=red
	     */
	    if((z = srchstr(p, "/FG=")) != NULL)
	      fg = remove_pat_escapes(z+4);
	    if((z = srchstr(p, "/BG=")) != NULL)
	      bg = remove_pat_escapes(z+4);

	    if(fg && *fg && bg && *bg)
	      action->incol = new_color_pair(fg, bg);

	    if(fg)
	      fs_give((void **)&fg);
	    if(bg)
	      fs_give((void **)&bg);
	    fs_give((void **)&p);
	}
    }
    /* per-folder sort */
    else if(!strncmp(str, "/SORT=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    SortOrder def_sort;
	    int       def_sort_rev;

	    if(decode_sort(p, &def_sort, &def_sort_rev) != -1){
		action->sort_is_set = 1;
		action->sortorder = def_sort;
		action->revsort   = (def_sort_rev ? 1 : 0);
	    }

	    fs_give((void **)&p);
	}
    }
    /* per-folder index-format */
    else if(!strncmp(str, "/IFORM=", 7))
      action->index_format = remove_pat_escapes(str+7);
    /* per-folder startup-rule */
    else if(!strncmp(str, "/START=", 7)){
	if((p = remove_pat_escapes(str+7)) != NULL){
	    for(i = 0; v = startup_rules(i); i++)
	      if(!strucmp(p, S_OR_L(v))){
		  action->startup_rule = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else{
	char save;

	action->bogus = 1;

	if((p = strindex(str, '=')) != NULL){
	    save = *(p+1);
	    *(p+1) = '\0';
	}

	dprint(1, (debugfile,
	       "parse_action_slash(%.20s): unrecognized in \"%s\"\n",
	       str ? str : "?",
	       (action && action->nick) ? action->nick : ""));
	q_status_message4(SM_ORDER, 1, 3,
	      "Warning: unrecognized pattern action \"%.20s\"%.20s%.20s%.20s",
	      str, action->nick ? " in rule \"" : "",
	      action->nick ? action->nick : "", action->nick ? "\"" : "");

	if(p)
	  *(p+1) = save;
    }
}


/*
 * Str looks like (min,max) or a comma-separated list of these.
 *
 * Parens are optional if unambiguous, whitespace is ignored.
 * If min is left out it is -INF. If max is left out it is INF.
 * If only one number and no comma number is min and max is INF.
 *
 * Returns the INTVL_S list.
 */
INTVL_S *
parse_intvl(str)
    char *str;
{
    char *q;
    long  left, right;
    INTVL_S *ret = NULL, **next;

    if(!str)
      return(ret);

    q = str;

    for(;;){
	left = right = INTVL_UNDEF;

	/* skip to first number */
	while(isspace((unsigned char) *q) || *q == LPAREN)
	  q++;
	
	/* min number */
	if(*q == COMMA || !struncmp(q, "-INF", 4))
	  left = - INTVL_INF;
	else if(*q == '-' || isdigit((unsigned char) *q))
	  left = atol(q);

	if(left != INTVL_UNDEF){
	    /* skip to second number */
	    while(*q && *q != COMMA && *q != RPAREN)
	      q++;
	    if(*q == COMMA)
	      q++;
	    while(isspace((unsigned char) *q))
	      q++;

	    /* max number */
	    if(*q == '\0' || *q == RPAREN || !struncmp(q, "INF", 3))
	      right = INTVL_INF;
	    else if(*q == '-' || isdigit((unsigned char) *q))
	      right = atol(q);
	}
	
	if(left == INTVL_UNDEF || right == INTVL_UNDEF
	   || left > right){
	    if(left != INTVL_UNDEF || right != INTVL_UNDEF || *q){
		if(left != INTVL_UNDEF && right != INTVL_UNDEF
		   && left > right)
		  q_status_message1(SM_ORDER, 3, 5,
				"Error: Interval \"%.200s\", min > max", str);
		else
		  q_status_message1(SM_ORDER, 3, 5,
		      "Error: Interval \"%.200s\": syntax is (min,max)", str);
	      
		if(ret)
		  free_intvl(&ret);
		
		ret = NULL;
	    }

	    break;
	}
	else{
	    if(!ret){
		ret = (INTVL_S *) fs_get(sizeof(*ret));
		memset((void *) ret, 0, sizeof(*ret));
		ret->imin = left;
		ret->imax = right;
		next = &ret->next;
	    }
	    else{
		*next = (INTVL_S *) fs_get(sizeof(*ret));
		memset((void *) *next, 0, sizeof(*ret));
		(*next)->imin = left;
		(*next)->imax = right;
		next = &(*next)->next;
	    }

	    /* skip to next interval in list */
	    while(*q && *q != COMMA && *q != RPAREN)
	      q++;
	    if(*q == RPAREN)
	      q++;
	    while(*q && *q != COMMA)
	      q++;
	    if(*q == COMMA)
	      q++;
	}
    }

    return(ret);
}


/*
 * Returns string that looks like "(left,right),(left2,right2)".
 * Caller is responsible for freeing memory.
 */
char *
stringform_of_intvl(intvl)
    INTVL_S *intvl;
{
    char *res = NULL;

    if(intvl && intvl->imin != INTVL_UNDEF && intvl->imax != INTVL_UNDEF
       && intvl->imin <= intvl->imax){
	char     lbuf[20], rbuf[20], buf[45], *p;
	INTVL_S *iv;
	int      count = 0;
	size_t   reslen;

	/* find a max size and allocate it for the result */
	for(iv = intvl;
	    (iv && iv->imin != INTVL_UNDEF && iv->imax != INTVL_UNDEF
	     && iv->imin <= iv->imax);
	    iv = iv->next)
	  count++;

	reslen = count * 50 * sizeof(char) + 1;
	res = (char *) fs_get(reslen);
	memset((void *) res, 0, reslen);
	p = res;

	for(iv = intvl;
	    (iv && iv->imin != INTVL_UNDEF && iv->imax != INTVL_UNDEF
	     && iv->imin <= iv->imax);
	    iv = iv->next){

	    if(iv->imin == - INTVL_INF)
	      strncpy(lbuf, "-INF", sizeof(lbuf));
	    else
	      sprintf(lbuf, "%ld", iv->imin);

	    if(iv->imax == INTVL_INF)
	      strncpy(rbuf, "INF", sizeof(rbuf));
	    else
	      sprintf(rbuf, "%ld", iv->imax);

	    sprintf(buf, "%.1s(%.20s,%.20s)", (p == res) ? "" : ",",
		    lbuf, rbuf);

	    sstrncpy(&p, buf, reslen - strlen(res) - 1);
	}
    }

    return(res);
}


/*
 * Args -- flags  - SCOREUSE_INVALID  Mark scores_in_use invalid so that we'll
 *					recalculate if we want to use it again.
 *		  - SCOREUSE_GET    Return whether scores are being used or not.
 *
 * Returns -- 0 - Scores not being used at all.
 *	     >0 - Scores are used. The return value consists of flag values
 *		    OR'd together. Possible values are:
 * 
 *			SCOREUSE_INCOLS  - scores needed for index line colors
 *			SCOREUSE_ROLES   - scores needed for roles
 *			SCOREUSE_FILTERS - scores needed for filters
 *			SCOREUSE_OTHER   - scores needed for other stuff
 *			SCOREUSE_INDEX   - scores needed for index drawing
 *
 *			SCOREUSE_STATEDEP - scores depend on message state
 */
int
scores_are_used(flags)
    int flags;
{
    static int  scores_in_use = -1;
    long        type1, type2;
    int         scores_are_defined, scores_are_used_somewhere = 0;
    PAT_STATE   pstate1, pstate2;

    if(flags & SCOREUSE_INVALID) /* mark invalid so we recalculate next time */
      scores_in_use = -1;
    else if(scores_in_use == -1){

	/*
	 * Check the patterns to see if scores are potentially
	 * being used.
	 * The first_pattern() in the if checks whether there are any
	 * non-zero scorevals. The loop checks whether any patterns
	 * use those non-zero scorevals.
	 */
	type1 = ROLE_SCORE;
	type2 = (ROLE_REPLY | ROLE_FORWARD | ROLE_COMPOSE |
		 ROLE_INCOL | ROLE_DO_FILTER);
	scores_are_defined = nonempty_patterns(type1, &pstate1)
			     && first_pattern(&pstate1);
	if(scores_are_defined)
	  scores_are_used_somewhere =
	     ((nonempty_patterns(type2, &pstate2) && first_pattern(&pstate2))
	      || ps_global->a_format_contains_score
	      || mn_get_sort(ps_global->msgmap) == SortScore);

	if(scores_are_used_somewhere){
	    PAT_S *pat;

	    /*
	     * Careful. nonempty_patterns() may call close_pattern()
	     * which will set scores_in_use to -1! So we have to be
	     * sure to reset it after we call nonempty_patterns().
	     */
	    scores_in_use = 0;
	    if(ps_global->a_format_contains_score
	       || mn_get_sort(ps_global->msgmap) == SortScore)
	      scores_in_use |= SCOREUSE_INDEX;

	    if(nonempty_patterns(type2, &pstate2))
	      for(pat = first_pattern(&pstate2);
		  pat;
		  pat = next_pattern(&pstate2))
	        if(pat->patgrp && !pat->patgrp->bogus && pat->patgrp->do_score){
		    if(pat->action && pat->action->is_a_incol)
		      scores_in_use |= SCOREUSE_INCOLS;
		    if(pat->action && pat->action->is_a_role)
		      scores_in_use |= SCOREUSE_ROLES;
		    if(pat->action && pat->action->is_a_filter)
		      scores_in_use |= SCOREUSE_FILTERS;
		    if(pat->action && pat->action->is_a_other)
		      scores_in_use |= SCOREUSE_OTHER;
	        }
	    
	    /*
	     * Note whether scores depend on message state or not.
	     */
	    if(scores_in_use)
	      for(pat = first_pattern(&pstate1);
		  pat;
		  pat = next_pattern(&pstate1))
		if(patgrp_depends_on_active_state(pat->patgrp)){
		    scores_in_use |= SCOREUSE_STATEDEP;
		    break;
		}
	      
	}
	else
	  scores_in_use = 0;
    }

    return((scores_in_use == -1) ? 0 : scores_in_use);
}


int
patgrp_depends_on_state(patgrp)
    PATGRP_S *patgrp;
{
    return(patgrp && (patgrp_depends_on_active_state(patgrp)
		      || patgrp->stat_rec != PAT_STAT_EITHER));
}


/*
 * Recent doesn't count for this function because it doesn't change while
 * the mailbox is open.
 */
int
patgrp_depends_on_active_state(patgrp)
    PATGRP_S *patgrp;
{
    return(patgrp && !patgrp->bogus
           && (patgrp->stat_new  != PAT_STAT_EITHER ||
	       patgrp->stat_del  != PAT_STAT_EITHER ||
	       patgrp->stat_imp  != PAT_STAT_EITHER ||
	       patgrp->stat_ans  != PAT_STAT_EITHER ||
	       patgrp->keyword));
}


/*
 * Look for label in str and return a pointer to parsed string.
 * Actually, we look for "label=" or "!label=", the second means NOT.
 * Converts from string from patterns file which looks like
 *       /NEWS=comp.mail.,comp.mail.pine/TO=...
 * This is the string that came from pattern="string" with the pattern=
 * and outer quotes removed.
 * This converts the string to a PATTERN_S list and returns
 * an allocated copy.
 */
PATTERN_S *
parse_pattern(label, str, hex_to_backslashed)
    char *label;
    char *str;
    int   hex_to_backslashed;
{
    char       copy[50];	/* local copy of label */
    char       copynot[50];	/* local copy of label, NOT'ed */
    char      *q, *labeled_str;
    PATTERN_S *head = NULL;

    if(!label || !str)
      return(NULL);
    
    q = copy;
    sstrncpy(&q, "/", sizeof(copy));
    sstrncpy(&q, label, sizeof(copy) - (q-copy));
    sstrncpy(&q, "=", sizeof(copy) - (q-copy));
    copy[sizeof(copy)-1] = '\0';
    q = copynot;
    sstrncpy(&q, "/!", sizeof(copynot));
    sstrncpy(&q, label, sizeof(copynot) - (q-copynot));
    sstrncpy(&q, "=", sizeof(copynot) - (q-copynot));
    copynot[sizeof(copynot)-1] = '\0';

    if(hex_to_backslashed){
	if((q = srchstr(str, copy)) != NULL){
	    head = config_to_pattern(q+strlen(copy));
	}
	else if((q = srchstr(str, copynot)) != NULL){
	    head = config_to_pattern(q+strlen(copynot));
	    head->not = 1;
	}
    }
    else{
	if((q = srchstr(str, copy)) != NULL){
	    if((labeled_str =
			remove_backslash_escapes(q+strlen(copy))) != NULL){
		head = string_to_pattern(labeled_str);
		fs_give((void **)&labeled_str);
	    }
	}
	else if((q = srchstr(str, copynot)) != NULL){
	    if((labeled_str =
			remove_backslash_escapes(q+strlen(copynot))) != NULL){
		head = string_to_pattern(labeled_str);
		head->not = 1;
		fs_give((void **)&labeled_str);
	    }
	}
    }

    return(head);
}


/*
 * Look for /ARB's in str and return a pointer to parsed ARBHDR_S.
 * Actually, we look for /!ARB and /!EARB as well. Those mean NOT.
 * Converts from string from patterns file which looks like
 *       /ARB<fieldname1>=pattern/.../ARB<fieldname2>=pattern...
 * This is the string that came from pattern="string" with the pattern=
 * and outer quotes removed.
 * This converts the string to a ARBHDR_S list and returns
 * an allocated copy.
 */
ARBHDR_S *
parse_arbhdr(str)
    char *str;
{
    char      *q, *s, *equals, *noesc;
    int        not, empty, skip;
    ARBHDR_S  *ahdr = NULL, *a, *aa;
    PATTERN_S *p = NULL;

    if(!str)
      return(NULL);

    aa = NULL;
    for(s = str; q = next_arb(s); s = q+1){
	not = (q[1] == '!') ? 1 : 0;
	empty = (q[not+1] == 'E') ? 1 : 0;
	skip = 4 + not + empty;
	if((noesc = remove_pat_escapes(q+skip)) != NULL){
	    if(*noesc != '=' && (equals = strindex(noesc, '=')) != NULL){
		a = (ARBHDR_S *)fs_get(sizeof(*a));
		memset((void *)a, 0, sizeof(*a));
		*equals = '\0';
		a->isemptyval = empty;
		a->field = cpystr(noesc);
		if(empty)
		  a->p     = string_to_pattern("");
		else if(*(equals+1) &&
			(p = string_to_pattern(equals+1)) != NULL)
		  a->p     = p;
		
		if(not && a->p)
		  a->p->not = 1;

		/* keep them in the same order */
		if(aa){
		    aa->next = a;
		    aa = aa->next;
		}
		else{
		    ahdr = a;
		    aa = ahdr;
		}
	    }

	    fs_give((void **)&noesc);
	}
    }

    return(ahdr);
}


char *
next_arb(start)
    char *start;
{
    char *q1, *q2, *q3, *q4, *p;

    q1 = srchstr(start, "/ARB");
    q2 = srchstr(start, "/!ARB");
    q3 = srchstr(start, "/EARB");
    q4 = srchstr(start, "/!EARB");

    p = q1;
    if(!p || (q2 && q2 < p))
      p = q2;
    if(!p || (q3 && q3 < p))
      p = q3;
    if(!p || (q4 && q4 < p))
      p = q4;
    
    return(p);
}


/*
 * Converts a string to a PATTERN_S list and returns an
 * allocated copy. The source string looks like
 *        string1,string2,...
 * Commas and backslashes may be backslash-escaped in the original string
 * in order to include actual commas and backslashes in the pattern.
 * So \, is an actual comma and , is the separator character.
 */
PATTERN_S *
string_to_pattern(str)
    char *str;
{
    char      *q, *s, *workspace;
    PATTERN_S *p, *head = NULL, **nextp;

    if(!str)
      return(head);
    
    /*
     * We want an empty string to cause an empty substring in the pattern
     * instead of returning a NULL pattern. That can be used as a way to
     * match any header. For example, if all the patterns but the news
     * pattern were null and the news pattern was a substring of "" then
     * we use that to match any message with a newsgroups header.
     */
    if(!*str){
	head = (PATTERN_S *)fs_get(sizeof(*p));
	memset((void *)head, 0, sizeof(*head));
	head->substring = cpystr("");
    }
    else{
	nextp = &head;
	workspace = (char *)fs_get((strlen(str)+1) * sizeof(char));
	s = workspace;
	*s = '\0';
	q = str;
	do {
	    switch(*q){
	      case COMMA:
	      case '\0':
		*s = '\0';
		removing_leading_and_trailing_white_space(workspace);
		p = (PATTERN_S *)fs_get(sizeof(*p));
		memset((void *)p, 0, sizeof(*p));
		p->substring = cpystr(workspace);
		*nextp = p;
		nextp = &p->next;
		s = workspace;
		*s = '\0';
		break;
		
	      case BSLASH:
		if(*(q+1) == COMMA || *(q+1) == BSLASH)
		  *s++ = *(++q);
		else
		  *s++ = *q;

		break;

	      default:
		*s++ = *q;
		break;
	    }
	} while(*q++);

	fs_give((void **)&workspace);
    }

    return(head);
}

    
/*
 * Converts a PATTERN_S list to a string.
 * The resulting string is allocated here and looks like
 *        string1,string2,...
 * Commas and backslashes in the original pattern
 * end up backslash-escaped in the string.
 */
char *
pattern_to_string(pattern)
    PATTERN_S *pattern;
{
    PATTERN_S *p;
    char      *result = NULL, *q, *s;
    size_t     n;

    if(!pattern)
      return(result);

    /* how much space is needed? */
    n = 0;
    for(p = pattern; p; p = p->next){
	n += (p == pattern) ? 0 : 1;
	for(s = p->substring; s && *s; s++){
	    if(*s == COMMA || *s == BSLASH)
	      n++;

	    n++;
	}
    }

    q = result = (char *)fs_get(++n);
    for(p = pattern; p; p = p->next){
	if(p != pattern)
	  *q++ = COMMA;

	for(s = p->substring; s && *s; s++){
	    if(*s == COMMA || *s == BSLASH)
	      *q++ = '\\';

	    *q++ = *s;
	}
    }

    *q = '\0';

    return(result);
}


/*
 * Do the escaping necessary to take a string for a pattern into a comma-
 * separated string with escapes suitable for the config file.
 * Returns an allocated copy of that string.
 * In particular
 *     ,  ->  \,  ->  \x2C
 *     \  ->  \\  ->  \x5C
 *     "       ->      \"
 *     /       ->      \/
 */
char *
pattern_to_config(pat)
    PATTERN_S *pat;
{
    char *s, *res = NULL;
    
    s = pattern_to_string(pat);
    if(s){
	res = add_pat_escapes(s);
	fs_give((void **) &s);
    }

    return(res);
}

/*
 * Opposite of pattern_to_config.
 */
PATTERN_S *
config_to_pattern(str)
    char *str;
{
    char      *s;
    PATTERN_S *pat = NULL;
    
    s = remove_pat_escapes(str);
    if(s){
	pat = string_to_pattern(s);
	fs_give((void **) &s);
    }

    return(pat);
}


/*
 * Converts an array of strings to a PATTERN_S list and returns an
 * allocated copy.
 * The list strings may not contain commas directly, because the UI turns
 * those into separate list members. Instead, the user types \, and
 * that backslash comma is converted to a comma here.
 * It is a bit odd. Backslash itself is not escaped. A backslash which is
 * not followed by a comma is a literal backslash, a backslash followed by
 * a comma is a comma.
 */
PATTERN_S *
editlist_to_pattern(list)
    char **list;
{
    PATTERN_S *head = NULL;

    if(!(list && *list))
      return(head);
    
    /*
     * We want an empty string to cause an empty substring in the pattern
     * instead of returning a NULL pattern. That can be used as a way to
     * match any header. For example, if all the patterns but the news
     * pattern were null and the news pattern was a substring of "" then
     * we use that to match any message with a newsgroups header.
     */
    if(!list[0][0]){
	head = (PATTERN_S *) fs_get(sizeof(*head));
	memset((void *) head, 0, sizeof(*head));
	head->substring = cpystr("");
    }
    else{
	char      *str, *s, *q, *workspace = NULL;
	size_t     l = 0;
	PATTERN_S *p, **nextp;
	int        i;

	nextp = &head;
	for(i = 0; (str = list[i]); i++){
	    if(str[0]){
		if(!workspace){
		    l = strlen(str) + 1;
		    workspace = (char *) fs_get(l * sizeof(char));
		}
		else if(strlen(str) + 1 > l){
		    l = strlen(str) + 1;
		    fs_give((void **) &workspace);
		    workspace = (char *) fs_get(l * sizeof(char));
		}

		s = workspace;
		*s = '\0';
		q = str;
		do {
		    switch(*q){
		      case '\0':
			*s = '\0';
			removing_leading_and_trailing_white_space(workspace);
			p = (PATTERN_S *) fs_get(sizeof(*p));
			memset((void *) p, 0, sizeof(*p));
			p->substring = cpystr(workspace);
			*nextp = p;
			nextp = &p->next;
			s = workspace;
			*s = '\0';
			break;
			
		      case BSLASH:
			if(*(q+1) == COMMA)
			  *s++ = *(++q);
			else
			  *s++ = *q;

			break;

		      default:
			*s++ = *q;
			break;
		    }
		} while(*q++);
	    }
	}
    }

    return(head);
}


/*
 * Converts a PATTERN_S to an array of strings and returns an allocated copy.
 * Commas are converted to backslash-comma, because the text_tool UI uses
 * commas to separate items.
 * It is a bit odd. Backslash itself is not escaped. A backslash which is
 * not followed by a comma is a literal backslash, a backslash followed by
 * a comma is a comma.
 */
char **
pattern_to_editlist(pat)
    PATTERN_S *pat;
{
    int        cnt, i;
    PATTERN_S *p;
    char     **list = NULL;

    if(!pat)
      return(list);

    /* how many in list? */
    for(cnt = 0, p = pat; p; p = p->next)
      cnt++;
    
    list = (char **) fs_get((cnt + 1) * sizeof(*list));
    memset((void *) list, 0, (cnt + 1) * sizeof(*list));

    for(i = 0, p = pat; p; p = p->next, i++)
      list[i] = add_comma_escapes(p->substring);
    
    return(list);
}


ADDRESS *
configstr_to_address(str)
    char *str;
{
    char    *s;
    ADDRESS *a = NULL;
    
    s = remove_pat_escapes(str);
    if(s){
	removing_double_quotes(s);
	rfc822_parse_adrlist(&a, s, ps_global->maildomain);
	fs_give((void **) &s);
    }

    return(a);
}


char *
address_to_configstr(address)
    ADDRESS *address;
{
    char *space = NULL, *result = NULL;

    if(!address)
      return(result);

    /* how much space is needed? */
    space = (char *) fs_get((size_t) est_size(address));
    if(space){
	space[0] = '\0';
	rfc822_write_address(space, address);
	result = add_pat_escapes(space);
	fs_give((void **) &space);
    }

    return(result);
}


/*
 * Must be called with a pstate, we don't check for it.
 * It respects the cur_rflag_num in pstate. That is, it doesn't start over
 * at i=1, it starts at cur_rflag_num.
 */
PAT_S *
first_any_pattern(pstate)
    PAT_STATE *pstate;
{
    PAT_LINE_S *patline = NULL;
    int         i;
    long        local_rflag;

    /*
     * The rest of pstate should be set before coming here.
     * In particular, the rflags should be set by a call to nonempty_patterns
     * or any_patterns, and cur_rflag_num should be set.
     */
    pstate->patlinecurrent = NULL;
    pstate->patcurrent     = NULL;

    /*
     * The order of these is important. It is the same as the order
     * used for next_any_pattern and opposite of the order used by
     * last and prev. For next_any's benefit, we allow cur_rflag_num to
     * start us out past the first set.
     */
    for(i = pstate->cur_rflag_num; i <= PATTERN_N; i++){

	local_rflag = 0L;

	switch(i){
	  case 1:
	    local_rflag = ROLE_DO_INCOLS & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 2:
	    local_rflag = ROLE_DO_ROLES & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 3:
	    local_rflag = ROLE_DO_FILTER & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 4:
	    local_rflag = ROLE_DO_SCORES & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 5:
	    local_rflag = ROLE_DO_OTHER & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 6:
	    local_rflag = ROLE_OLD_FILT & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 7:
	    local_rflag = ROLE_OLD_SCORE & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case PATTERN_N:
	    local_rflag = ROLE_OLD_PAT & CANONICAL_RFLAGS(pstate->rflags);
	    break;
	}

	if(local_rflag){
	    SET_PATTYPE(local_rflag | (pstate->rflags & PAT_USE_MASK));

	    if(*cur_pat_h){
		/* Find first patline with a pat */
		for(patline = (*cur_pat_h)->patlinehead;
		    patline && !patline->first;
		    patline = patline->next)
		  ;
	    }

	    if(patline){
		pstate->cur_rflag_num  = i;
		pstate->patlinecurrent = patline;
		pstate->patcurrent     = patline->first;
	    }
	}

	if(pstate->patcurrent)
	  break;
    }

    return(pstate->patcurrent);
}


/*
 * Return first pattern of the specified types. These types were set by a
 * previous call to any_patterns or nonempty_patterns.
 *
 * Args --  pstate  pattern state. This is set here and passed back for
 *                  use by next_pattern. Must be non-null.
 *                  It must have been initialized previously by a call to
 *                  nonempty_patterns or any_patterns.
 */
PAT_S *
first_pattern(pstate)
    PAT_STATE *pstate;
{
    PAT_S           *pat;
    struct variable *vars = ps_global->vars;
    long             rflags;

    pstate->cur_rflag_num = 1;

    rflags = pstate->rflags;

    for(pat = first_any_pattern(pstate);
	pat && !((pat->action &&
		  ((rflags & ROLE_DO_ROLES && pat->action->is_a_role) ||
	           (rflags & (ROLE_DO_INCOLS|ROLE_INCOL) &&
		    pat->action->is_a_incol) ||
	           (rflags & ROLE_DO_OTHER && pat->action->is_a_other) ||
	           (rflags & ROLE_DO_SCORES && pat->action->is_a_score) ||
		   (rflags & ROLE_SCORE && pat->action->scoreval) ||
		   (rflags & ROLE_DO_FILTER && pat->action->is_a_filter) ||
	           (rflags & ROLE_REPLY &&
		    (pat->action->repl_type == ROLE_REPL_YES ||
		     pat->action->repl_type == ROLE_REPL_NOCONF)) ||
	           (rflags & ROLE_FORWARD &&
		    (pat->action->forw_type == ROLE_FORW_YES ||
		     pat->action->forw_type == ROLE_FORW_NOCONF)) ||
	           (rflags & ROLE_COMPOSE &&
		    (pat->action->comp_type == ROLE_COMP_YES ||
		     pat->action->comp_type == ROLE_COMP_NOCONF)) ||
	           (rflags & ROLE_OLD_FILT) ||
	           (rflags & ROLE_OLD_SCORE) ||
	           (rflags & ROLE_OLD_PAT)))
		||
		 pat->inherit);
	pat = next_any_pattern(pstate))
      ;
    
    return(pat);
}


/*
 * Just like first_any_pattern.
 */
PAT_S *
last_any_pattern(pstate)
    PAT_STATE *pstate;
{
    PAT_LINE_S *patline = NULL;
    int         i;
    long        local_rflag;

    /*
     * The rest of pstate should be set before coming here.
     * In particular, the rflags should be set by a call to nonempty_patterns
     * or any_patterns, and cur_rflag_num should be set.
     */
    pstate->patlinecurrent = NULL;
    pstate->patcurrent     = NULL;

    for(i = pstate->cur_rflag_num; i >= 1; i--){

	local_rflag = 0L;

	switch(i){
	  case 1:
	    local_rflag = ROLE_DO_INCOLS & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 2:
	    local_rflag = ROLE_DO_ROLES & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 3:
	    local_rflag = ROLE_DO_FILTER & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 4:
	    local_rflag = ROLE_DO_SCORES & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 5:
	    local_rflag = ROLE_DO_OTHER & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 6:
	    local_rflag = ROLE_OLD_FILT & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 7:
	    local_rflag = ROLE_OLD_SCORE & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case PATTERN_N:
	    local_rflag = ROLE_OLD_PAT & CANONICAL_RFLAGS(pstate->rflags);
	    break;
	}

	if(local_rflag){
	    SET_PATTYPE(local_rflag | (pstate->rflags & PAT_USE_MASK));

	    pstate->patlinecurrent = NULL;
	    pstate->patcurrent     = NULL;

	    if(*cur_pat_h){
		/* Find last patline with a pat */
		for(patline = (*cur_pat_h)->patlinehead;
		    patline;
		    patline = patline->next)
		  if(patline->last)
		    pstate->patlinecurrent = patline;
		
		if(pstate->patlinecurrent)
		  pstate->patcurrent = pstate->patlinecurrent->last;
	    }

	    if(pstate->patcurrent)
	      pstate->cur_rflag_num = i;

	    if(pstate->patcurrent)
	      break;
	}
    }

    return(pstate->patcurrent);
}


/*
 * Return last pattern of the specified types. These types were set by a
 * previous call to any_patterns or nonempty_patterns.
 *
 * Args --  pstate  pattern state. This is set here and passed back for
 *                  use by prev_pattern. Must be non-null.
 *                  It must have been initialized previously by a call to
 *                  nonempty_patterns or any_patterns.
 */
PAT_S *
last_pattern(pstate)
    PAT_STATE *pstate;
{
    PAT_S           *pat;
    struct variable *vars = ps_global->vars;
    long             rflags;

    pstate->cur_rflag_num = PATTERN_N;

    rflags = pstate->rflags;

    for(pat = last_any_pattern(pstate);
	pat && !((pat->action &&
		  ((rflags & ROLE_DO_ROLES && pat->action->is_a_role) ||
	           (rflags & (ROLE_DO_INCOLS|ROLE_INCOL) &&
		    pat->action->is_a_incol) ||
	           (rflags & ROLE_DO_OTHER && pat->action->is_a_other) ||
	           (rflags & ROLE_DO_SCORES && pat->action->is_a_score) ||
		   (rflags & ROLE_SCORE && pat->action->scoreval) ||
		   (rflags & ROLE_DO_FILTER && pat->action->is_a_filter) ||
	           (rflags & ROLE_REPLY &&
		    (pat->action->repl_type == ROLE_REPL_YES ||
		     pat->action->repl_type == ROLE_REPL_NOCONF)) ||
	           (rflags & ROLE_FORWARD &&
		    (pat->action->forw_type == ROLE_FORW_YES ||
		     pat->action->forw_type == ROLE_FORW_NOCONF)) ||
	           (rflags & ROLE_COMPOSE &&
		    (pat->action->comp_type == ROLE_COMP_YES ||
		     pat->action->comp_type == ROLE_COMP_NOCONF)) ||
	           (rflags & ROLE_OLD_FILT) ||
	           (rflags & ROLE_OLD_SCORE) ||
	           (rflags & ROLE_OLD_PAT)))
		||
		 pat->inherit);
	pat = prev_any_pattern(pstate))
      ;
    
    return(pat);
}

    
/*
 * This assumes that pstate is valid.
 */
PAT_S *
next_any_pattern(pstate)
    PAT_STATE *pstate;
{
    PAT_LINE_S *patline;

    if(pstate->patlinecurrent){
	if(pstate->patcurrent && pstate->patcurrent->next)
	  pstate->patcurrent = pstate->patcurrent->next;
	else{
	    /* Find next patline with a pat */
	    for(patline = pstate->patlinecurrent->next;
		patline && !patline->first;
		patline = patline->next)
	      ;
	    
	    if(patline){
		pstate->patlinecurrent = patline;
		pstate->patcurrent     = patline->first;
	    }
	    else{
		pstate->patlinecurrent = NULL;
		pstate->patcurrent     = NULL;
	    }
	}
    }

    /* we've reached the last, try the next rflag_num (the next pattern type) */
    if(!pstate->patcurrent){
	pstate->cur_rflag_num++;
	pstate->patcurrent = first_any_pattern(pstate);
    }

    return(pstate->patcurrent);
}


/*
 * Return next pattern of the specified types. These types were set by a
 * previous call to any_patterns or nonempty_patterns.
 *
 * Args -- pstate  pattern state. This is set by first_pattern or last_pattern.
 */
PAT_S *
next_pattern(pstate)
    PAT_STATE  *pstate;
{
    PAT_S           *pat;
    struct variable *vars = ps_global->vars;
    long             rflags;

    rflags = pstate->rflags;

    for(pat = next_any_pattern(pstate);
	pat && !((pat->action &&
		  ((rflags & ROLE_DO_ROLES && pat->action->is_a_role) ||
	           (rflags & (ROLE_DO_INCOLS|ROLE_INCOL) &&
		    pat->action->is_a_incol) ||
	           (rflags & ROLE_DO_OTHER && pat->action->is_a_other) ||
	           (rflags & ROLE_DO_SCORES && pat->action->is_a_score) ||
		   (rflags & ROLE_SCORE && pat->action->scoreval) ||
		   (rflags & ROLE_DO_FILTER && pat->action->is_a_filter) ||
	           (rflags & ROLE_REPLY &&
		    (pat->action->repl_type == ROLE_REPL_YES ||
		     pat->action->repl_type == ROLE_REPL_NOCONF)) ||
	           (rflags & ROLE_FORWARD &&
		    (pat->action->forw_type == ROLE_FORW_YES ||
		     pat->action->forw_type == ROLE_FORW_NOCONF)) ||
	           (rflags & ROLE_COMPOSE &&
		    (pat->action->comp_type == ROLE_COMP_YES ||
		     pat->action->comp_type == ROLE_COMP_NOCONF)) ||
	           (rflags & ROLE_OLD_FILT) ||
	           (rflags & ROLE_OLD_SCORE) ||
	           (rflags & ROLE_OLD_PAT)))
		||
		 pat->inherit);
	pat = next_any_pattern(pstate))
      ;
    
    return(pat);
}

    
/*
 * This assumes that pstate is valid.
 */
PAT_S *
prev_any_pattern(pstate)
    PAT_STATE *pstate;
{
    PAT_LINE_S *patline;

    if(pstate->patlinecurrent){
	if(pstate->patcurrent && pstate->patcurrent->prev)
	  pstate->patcurrent = pstate->patcurrent->prev;
	else{
	    /* Find prev patline with a pat */
	    for(patline = pstate->patlinecurrent->prev;
		patline && !patline->last;
		patline = patline->prev)
	      ;
	    
	    if(patline){
		pstate->patlinecurrent = patline;
		pstate->patcurrent     = patline->last;
	    }
	    else{
		pstate->patlinecurrent = NULL;
		pstate->patcurrent     = NULL;
	    }
	}
    }

    if(!pstate->patcurrent){
	pstate->cur_rflag_num--;
	pstate->patcurrent = last_any_pattern(pstate);
    }

    return(pstate->patcurrent);
}


/*
 * Return prev pattern of the specified types. These types were set by a
 * previous call to any_patterns or nonempty_patterns.
 *
 * Args -- pstate  pattern state. This is set by first_pattern or last_pattern.
 */
PAT_S *
prev_pattern(pstate)
    PAT_STATE  *pstate;
{
    PAT_S           *pat;
    struct variable *vars = ps_global->vars;
    long             rflags;

    rflags = pstate->rflags;

    for(pat = prev_any_pattern(pstate);
	pat && !((pat->action &&
		  ((rflags & ROLE_DO_ROLES && pat->action->is_a_role) ||
	           (rflags & (ROLE_DO_INCOLS|ROLE_INCOL) &&
		    pat->action->is_a_incol) ||
	           (rflags & ROLE_DO_OTHER && pat->action->is_a_other) ||
	           (rflags & ROLE_DO_SCORES && pat->action->is_a_score) ||
		   (rflags & ROLE_SCORE && pat->action->scoreval) ||
		   (rflags & ROLE_DO_FILTER && pat->action->is_a_filter) ||
	           (rflags & ROLE_REPLY &&
		    (pat->action->repl_type == ROLE_REPL_YES ||
		     pat->action->repl_type == ROLE_REPL_NOCONF)) ||
	           (rflags & ROLE_FORWARD &&
		    (pat->action->forw_type == ROLE_FORW_YES ||
		     pat->action->forw_type == ROLE_FORW_NOCONF)) ||
	           (rflags & ROLE_COMPOSE &&
		    (pat->action->comp_type == ROLE_COMP_YES ||
		     pat->action->comp_type == ROLE_COMP_NOCONF)) ||
	           (rflags & ROLE_OLD_FILT) ||
	           (rflags & ROLE_OLD_SCORE) ||
	           (rflags & ROLE_OLD_PAT)))
		||
		 pat->inherit);
	pat = prev_any_pattern(pstate))
      ;
    
    return(pat);
}

    
/*
 * Rflags may be more than one pattern type OR'd together.
 */
int
write_patterns(rflags)
    long rflags;
{
    int canon_rflags;
    int err = 0;

    dprint(7, (debugfile, "write_patterns(0x%x)\n", rflags));

    canon_rflags = CANONICAL_RFLAGS(rflags);

    if(canon_rflags & ROLE_DO_INCOLS)
      err += sub_write_patterns(ROLE_DO_INCOLS | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_OTHER)
      err += sub_write_patterns(ROLE_DO_OTHER  | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_FILTER)
      err += sub_write_patterns(ROLE_DO_FILTER | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_SCORES)
      err += sub_write_patterns(ROLE_DO_SCORES | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_ROLES)
      err += sub_write_patterns(ROLE_DO_ROLES  | (rflags & PAT_USE_MASK));

    if(!err)
      write_pinerc(ps_global, (rflags & PAT_USE_MAIN) ? Main : Post, WRP_NONE);

    return(err);
}


int
sub_write_patterns(rflags)
    long rflags;
{
    int            err = 0, lineno = 0;
    char         **lvalue = NULL;
    PAT_LINE_S    *patline;

    SET_PATTYPE(rflags);

    if(!(*cur_pat_h)){
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			  "Unknown error saving patterns");
	return(-1);
    }

    if((*cur_pat_h)->dirtypinerc){
	/* Count how many lines will be in patterns variable */
	for(patline = (*cur_pat_h)->patlinehead;
	    patline;
	    patline = patline->next)
	  lineno++;
    
	lvalue = (char **)fs_get((lineno+1)*sizeof(char *));
	memset(lvalue, 0, (lineno+1) * sizeof(char *));
    }

    for(patline = (*cur_pat_h)->patlinehead, lineno = 0;
	!err && patline;
	patline = patline->next, lineno++){
	if(patline->type == File)
	  err = write_pattern_file((*cur_pat_h)->dirtypinerc
				      ? &lvalue[lineno] : NULL, patline);
	else if(patline->type == Literal && (*cur_pat_h)->dirtypinerc)
	  err = write_pattern_lit(&lvalue[lineno], patline);
	else if(patline->type == Inherit)
	  err = write_pattern_inherit((*cur_pat_h)->dirtypinerc
				      ? &lvalue[lineno] : NULL, patline);
    }

    if((*cur_pat_h)->dirtypinerc){
	if(err)
	  free_list_array(&lvalue);
	else{
	    char ***alval;
	    struct variable *var;

	    if(rflags & ROLE_DO_ROLES)
	      var = &ps_global->vars[V_PAT_ROLES];
	    else if(rflags & ROLE_DO_OTHER)
	      var = &ps_global->vars[V_PAT_OTHER];
	    else if(rflags & ROLE_DO_FILTER)
	      var = &ps_global->vars[V_PAT_FILTS];
	    else if(rflags & ROLE_DO_SCORES)
	      var = &ps_global->vars[V_PAT_SCORES];
	    else if(rflags & ROLE_DO_INCOLS)
	      var = &ps_global->vars[V_PAT_INCOLS];

	    alval = ALVAL(var, (rflags & PAT_USE_MAIN) ? Main : Post);
	    if(*alval)
	      free_list_array(alval);
	    
	    *alval = lvalue;

	    set_current_val(var, TRUE, TRUE);
	}
    }

    if(!err)
      (*cur_pat_h)->dirtypinerc = 0;

    return(err);
}


/*
 * Write pattern lines into a file.
 *
 * Args  lvalue -- Pointer to char * to fill in variable value
 *      patline -- 
 *
 * Returns  0 -- all is ok, lvalue has been filled in, file has been written
 *       else -- error, lvalue untouched, file not written
 */
int
write_pattern_file(lvalue, patline)
    char      **lvalue;
    PAT_LINE_S *patline;
{
    char  *p, *tfile;
    int    fd = -1, err = 0;
    FILE  *fp_new;
    PAT_S *pat;

    dprint(7, (debugfile, "write_pattern_file(%s)\n",
	   (patline && patline->filepath) ? patline->filepath : "?"));

    if(lvalue){
	p = (char *)fs_get((strlen(patline->filename) + 6) * sizeof(char));
	strcat(strcpy(p, "FILE:"), patline->filename);
	*lvalue = p;
    }

    if(patline->readonly || !patline->dirty)	/* doesn't need writing */
      return(err);

    /* Get a tempfile to write the patterns into */
    if(((tfile = tempfile_in_same_dir(patline->filepath, ".pt", NULL)) == NULL)
       || ((fd = open(tfile, O_TRUNC|O_WRONLY|O_CREAT, 0600)) < 0)
       || ((fp_new = fdopen(fd, "w")) == NULL)){
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  "Can't write in directory containing file \"%.200s\"",
			  patline->filepath);
	if(tfile){
	    (void)unlink(tfile);
	    fs_give((void **)&tfile);
	}
	
	if(fd >= 0)
	  close(fd);

	return(-1);
    }

    dprint(9, (debugfile, "write_pattern_file: writing into %s\n",
	   tfile ? tfile : "?"));
    
    if(fprintf(fp_new, "%s %s\n", PATTERN_MAGIC, PATTERN_FILE_VERS) == EOF)
      err--;

    for(pat = patline->first; !err && pat; pat = pat->next){
	if((p = data_for_patline(pat)) != NULL){
	    if(fprintf(fp_new, "%s\n", p) == EOF)
	      err--;
	    
	    fs_give((void **)&p);
	}
    }

    if(err || fclose(fp_new) == EOF){
	if(err)
	  (void)fclose(fp_new);

	err--;
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "I/O error: \"%.200s\": %.200s",
			  tfile, error_description(errno));
    }

    if(!err && rename_file(tfile, patline->filepath) < 0){
	err--;
	q_status_message3(SM_ORDER | SM_DING, 3, 4,
			  "Error renaming \"%.200s\" to \"%.200s\": %.200s",
			  tfile, patline->filepath, error_description(errno));
	dprint(2, (debugfile,
	       "write_pattern_file: Error renaming (%s,%s): %s\n",
	       tfile ? tfile : "?",
	       (patline && patline->filepath) ? patline->filepath : "?",
	       error_description(errno)));
    }

    if(tfile){
	(void)unlink(tfile);
	fs_give((void **)&tfile);
    }

    if(!err)
      patline->dirty = 0;

    return(err);
}


/*
 * Write literal pattern lines into lvalue (pinerc variable).
 *
 * Args  lvalue -- Pointer to char * to fill in variable value
 *      patline -- 
 *
 * Returns  0 -- all is ok, lvalue has been filled in, file has been written
 *       else -- error, lvalue untouched, file not written
 */
int
write_pattern_lit(lvalue, patline)
    char      **lvalue;
    PAT_LINE_S *patline;
{
    char  *p = NULL;
    int    err = 0;
    PAT_S *pat;

    pat = patline ? patline->first : NULL;
    
    if(pat && lvalue && (p = data_for_patline(pat)) != NULL){
	*lvalue = (char *)fs_get((strlen(p) + 5) * sizeof(char));
	strcat(strcpy(*lvalue, "LIT:"), p);
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Unknown error saving pattern variable");
	err--;
    }
    
    if(p)
      fs_give((void **)&p);

    return(err);
}


int
write_pattern_inherit(lvalue, patline)
    char      **lvalue;
    PAT_LINE_S *patline;
{
    int    err = 0;

    if(patline && patline->type == Inherit && lvalue)
      *lvalue = cpystr(INHERIT);
    else
      err--;
    
    return(err);
}



char *
data_for_patline(pat)
    PAT_S *pat;
{
    char          *p = NULL, *q, *to_pat = NULL,
		  *news_pat = NULL, *from_pat = NULL,
		  *sender_pat = NULL, *cc_pat = NULL, *subj_pat = NULL,
		  *arb_pat = NULL, *fldr_type_pat = NULL, *fldr_pat = NULL,
		  *afrom_type_pat = NULL, *abooks_pat = NULL,
		  *alltext_pat = NULL, *scorei_pat = NULL, *recip_pat = NULL,
		  *keyword_pat = NULL, *charset_pat = NULL,
		  *bodytext_pat = NULL, *age_pat = NULL, *sentdate = NULL,
		  *size_pat = NULL,
		  *category_cmd = NULL, *category_pat = NULL,
		  *category_lim = NULL,
		  *partic_pat = NULL, *stat_new_val = NULL,
		  *stat_rec_val = NULL,
		  *stat_imp_val = NULL, *stat_del_val = NULL,
		  *stat_ans_val = NULL, *stat_8bit_val = NULL,
		  *stat_bom_val = NULL, *stat_boy_val = NULL,
		  *from_act = NULL, *replyto_act = NULL, *fcc_act = NULL,
		  *sig_act = NULL, *nick = NULL, *templ_act = NULL,
		  *litsig_act = NULL, *cstm_act = NULL, *smtp_act = NULL,
                  *nntp_act = NULL, *comment = NULL,
		  *repl_val = NULL, *forw_val = NULL, *comp_val = NULL,
		  *incol_act = NULL, *inherit_nick = NULL, *score_act = NULL,
		  *sort_act = NULL, *iform_act = NULL, *start_act = NULL,
		  *folder_act = NULL, *filt_ifnotdel = NULL,
		  *filt_nokill = NULL, *filt_del_val = NULL,
		  *filt_imp_val = NULL, *filt_ans_val = NULL,
		  *filt_new_val = NULL, *filt_nonterm = NULL,
		  *keyword_set = NULL, *keyword_clr = NULL;
    int            to_not = 0, news_not = 0, from_not = 0,
		   sender_not = 0, cc_not = 0, subj_not = 0,
		   partic_not = 0, recip_not = 0, alltext_not, bodytext_not,
		   keyword_not = 0, charset_not = 0;
    ACTION_S      *action = NULL;
    NAMEVAL_S     *f;

    if(!pat)
      return(p);

    if((pat->patgrp && pat->patgrp->bogus)
       || (pat->action && pat->action->bogus)){
	if(pat->raw)
	  p = cpystr(pat->raw);

	return(p);
    }

    if(pat->patgrp){
	if(pat->patgrp->nick)
	  if((nick = add_pat_escapes(pat->patgrp->nick)) && !*nick)
	    fs_give((void **) &nick);

	if(pat->patgrp->comment)
	  if((comment = add_pat_escapes(pat->patgrp->comment)) && !*comment)
	    fs_give((void **) &comment);

	if(pat->patgrp->to){
	    to_pat = pattern_to_config(pat->patgrp->to);
	    to_not = pat->patgrp->to->not;
	}

	if(pat->patgrp->from){
	    from_pat = pattern_to_config(pat->patgrp->from);
	    from_not = pat->patgrp->from->not;
	}

	if(pat->patgrp->sender){
	    sender_pat = pattern_to_config(pat->patgrp->sender);
	    sender_not = pat->patgrp->sender->not;
	}

	if(pat->patgrp->cc){
	    cc_pat = pattern_to_config(pat->patgrp->cc);
	    cc_not = pat->patgrp->cc->not;
	}

	if(pat->patgrp->recip){
	    recip_pat = pattern_to_config(pat->patgrp->recip);
	    recip_not = pat->patgrp->recip->not;
	}

	if(pat->patgrp->partic){
	    partic_pat = pattern_to_config(pat->patgrp->partic);
	    partic_not = pat->patgrp->partic->not;
	}

	if(pat->patgrp->news){
	    news_pat = pattern_to_config(pat->patgrp->news);
	    news_not = pat->patgrp->news->not;
	}

	if(pat->patgrp->subj){
	    subj_pat = pattern_to_config(pat->patgrp->subj);
	    subj_not = pat->patgrp->subj->not;
	}

	if(pat->patgrp->alltext){
	    alltext_pat = pattern_to_config(pat->patgrp->alltext);
	    alltext_not = pat->patgrp->alltext->not;
	}

	if(pat->patgrp->bodytext){
	    bodytext_pat = pattern_to_config(pat->patgrp->bodytext);
	    bodytext_not = pat->patgrp->bodytext->not;
	}

	if(pat->patgrp->keyword){
	    keyword_pat = pattern_to_config(pat->patgrp->keyword);
	    keyword_not = pat->patgrp->keyword->not;
	}

	if(pat->patgrp->charsets){
	    charset_pat = pattern_to_config(pat->patgrp->charsets);
	    charset_not = pat->patgrp->charsets->not;
	}

	if(pat->patgrp->arbhdr){
	    ARBHDR_S *a;
	    char     *p1 = NULL, *p2 = NULL, *p3 = NULL, *p4 = NULL;
	    int       len = 0;

	    /* This is brute force dumb, but who cares? */
	    for(a = pat->patgrp->arbhdr; a; a = a->next){
		if(a->field && a->field[0]){
		    p1 = pattern_to_string(a->p);
		    p1 = p1 ? p1 : cpystr("");
		    p2 = (char *)fs_get((strlen(a->field)+strlen(p1)+2) *
							    sizeof(char));
		    sprintf(p2, "%s=%s", a->field, p1);
		    p3 = add_pat_escapes(p2);
		    p4 = (char *)fs_get((strlen(p3)+7) * sizeof(char));
		    sprintf(p4, "/%s%sARB%s",
			    (a->p && a->p->not) ? "!" : "",
			    a->isemptyval ? "E" : "", p3);
		    len += strlen(p4);

		    if(p1)
		      fs_give((void **)&p1);
		    if(p2)
		      fs_give((void **)&p2);
		    if(p3)
		      fs_give((void **)&p3);
		    if(p4)
		      fs_give((void **)&p4);
		}
	    }

	    p = arb_pat = (char *)fs_get((len + 1) * sizeof(char));

	    for(a = pat->patgrp->arbhdr; a; a = a->next){
		if(a->field && a->field[0]){
		    p1 = pattern_to_string(a->p);
		    p1 = p1 ? p1 : cpystr("");
		    p2 = (char *)fs_get((strlen(a->field)+strlen(p1)+2) *
							    sizeof(char));
		    sprintf(p2, "%s=%s", a->field, p1);
		    p3 = add_pat_escapes(p2);
		    p4 = (char *)fs_get((strlen(p3)+7) * sizeof(char));
		    sprintf(p4, "/%s%sARB%s",
			    (a->p && a->p->not) ? "!" : "",
			    a->isemptyval ? "E" : "", p3);
		    sstrcpy(&p, p4);

		    if(p1)
		      fs_give((void **)&p1);
		    if(p2)
		      fs_give((void **)&p2);
		    if(p3)
		      fs_give((void **)&p3);
		    if(p4)
		      fs_give((void **)&p4);
		}
	    }
	}

	if(pat->patgrp->age_uses_sentdate)
	  sentdate = cpystr("/SENTDATE=1");

	if(pat->patgrp->do_score){
	    p = stringform_of_intvl(pat->patgrp->score);
	    if(p){
		scorei_pat = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(pat->patgrp->do_age){
	    p = stringform_of_intvl(pat->patgrp->age);
	    if(p){
		age_pat = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(pat->patgrp->do_size){
	    p = stringform_of_intvl(pat->patgrp->size);
	    if(p){
		size_pat = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(pat->patgrp->category_cmd && pat->patgrp->category_cmd[0]){
	    size_t sz;
	    char **l, *q;

	    /* concatenate into string with commas first */
	    sz = 0;
	    for(l = pat->patgrp->category_cmd; l[0] && l[0][0]; l++)
	      sz += strlen(l[0]) + 1;

	    if(sz){
		char *p;
		int   first_one = 1;

		q = (char *)fs_get(sz);
		memset(q, 0, sz);
		p = q;
		for(l = pat->patgrp->category_cmd; l[0] && l[0][0]; l++){
		    if(!first_one)
		      sstrcpy(&p, ",");

		    first_one = 0;
		    sstrcpy(&p, l[0]);
		}

		category_cmd = add_pat_escapes(q);
		fs_give((void **)&q);
	    }
	}

	if(pat->patgrp->do_cat){
	    p = stringform_of_intvl(pat->patgrp->cat);
	    if(p){
		category_pat = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(pat->patgrp->cat_lim != -1L){
	    category_lim = (char *) fs_get(20 * sizeof(char));
	    sprintf(category_lim, "%ld", pat->patgrp->cat_lim);
	}

	if((f = pat_fldr_types(pat->patgrp->fldr_type)) != NULL)
	  fldr_type_pat = f->shortname;

	if(pat->patgrp->folder)
	  fldr_pat = pattern_to_config(pat->patgrp->folder);

	if((f = abookfrom_fldr_types(pat->patgrp->abookfrom)) != NULL
	   && f->value != AFRM_DEFL)
	  afrom_type_pat = f->shortname;

	if(pat->patgrp->abooks)
	  abooks_pat = pattern_to_config(pat->patgrp->abooks);

	if(pat->patgrp->stat_new != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_new)) != NULL)
	  stat_new_val = f->shortname;

	if(pat->patgrp->stat_rec != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_rec)) != NULL)
	  stat_rec_val = f->shortname;

	if(pat->patgrp->stat_del != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_del)) != NULL)
	  stat_del_val = f->shortname;

	if(pat->patgrp->stat_ans != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_ans)) != NULL)
	  stat_ans_val = f->shortname;

	if(pat->patgrp->stat_imp != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_imp)) != NULL)
	  stat_imp_val = f->shortname;

	if(pat->patgrp->stat_8bitsubj != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_8bitsubj)) != NULL)
	  stat_8bit_val = f->shortname;

	if(pat->patgrp->stat_bom != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_bom)) != NULL)
	  stat_bom_val = f->shortname;

	if(pat->patgrp->stat_boy != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_boy)) != NULL)
	  stat_boy_val = f->shortname;
    }

    if(pat->action){
	action = pat->action;

	if(action->is_a_score && action->scoreval != 0L &&
	   action->scoreval >= SCORE_MIN && action->scoreval <= SCORE_MAX){
	    score_act = (char *) fs_get(5 * sizeof(char));
	    sprintf(score_act, "%ld", pat->action->scoreval);
	}

	if(action->is_a_role){
	    if(action->inherit_nick)
	      inherit_nick = add_pat_escapes(action->inherit_nick);
	    if(action->fcc)
	      fcc_act = add_pat_escapes(action->fcc);
	    if(action->litsig)
	      litsig_act = add_pat_escapes(action->litsig);
	    if(action->sig)
	      sig_act = add_pat_escapes(action->sig);
	    if(action->template)
	      templ_act = add_pat_escapes(action->template);

	    if(action->cstm){
		size_t sz;
		char **l, *q;

		/* concatenate into string with commas first */
		sz = 0;
		for(l = action->cstm; l[0] && l[0][0]; l++)
		  sz += strlen(l[0]) + 1;

		if(sz){
		    char *p;
		    int   first_one = 1;

		    q = (char *)fs_get(sz);
		    memset(q, 0, sz);
		    p = q;
		    for(l = action->cstm; l[0] && l[0][0]; l++){
			if((!struncmp(l[0], "from", 4) &&
			   (l[0][4] == ':' || l[0][4] == '\0')) ||
			   (!struncmp(l[0], "reply-to", 8) &&
			   (l[0][8] == ':' || l[0][8] == '\0')))
			  continue;

			if(!first_one)
			  sstrcpy(&p, ",");

		        first_one = 0;
			sstrcpy(&p, l[0]);
		    }

		    cstm_act = add_pat_escapes(q);
		    fs_give((void **)&q);
		}
	    }

	    if(action->smtp){
		size_t sz;
		char **l, *q;

		/* concatenate into string with commas first */
		sz = 0;
		for(l = action->smtp; l[0] && l[0][0]; l++)
		  sz += strlen(l[0]) + 1;

		if(sz){
		    char *p;
		    int   first_one = 1;

		    q = (char *)fs_get(sz);
		    memset(q, 0, sz);
		    p = q;
		    for(l = action->smtp; l[0] && l[0][0]; l++){
			if(!first_one)
			  sstrcpy(&p, ",");

		        first_one = 0;
			sstrcpy(&p, l[0]);
		    }

		    smtp_act = add_pat_escapes(q);
		    fs_give((void **)&q);
		}
	    }

	    if(action->nntp){
		size_t sz;
		char **l, *q;

		/* concatenate into string with commas first */
		sz = 0;
		for(l = action->nntp; l[0] && l[0][0]; l++)
		  sz += strlen(l[0]) + 1;

		if(sz){
		    char *p;
		    int   first_one = 1;

		    q = (char *)fs_get(sz);
		    memset(q, 0, sz);
		    p = q;
		    for(l = action->nntp; l[0] && l[0][0]; l++){
			if(!first_one)
			  sstrcpy(&p, ",");

		        first_one = 0;
			sstrcpy(&p, l[0]);
		    }

		    nntp_act = add_pat_escapes(q);
		    fs_give((void **)&q);
		}
	    }

	    if((f = role_repl_types(action->repl_type)) != NULL)
	      repl_val = f->shortname;

	    if((f = role_forw_types(action->forw_type)) != NULL)
	      forw_val = f->shortname;
	    
	    if((f = role_comp_types(action->comp_type)) != NULL)
	      comp_val = f->shortname;
	}
	
	if(action->is_a_incol && action->incol){
	    char *ptr, buf[256], *p1, *p2;

	    ptr = buf;
	    memset(buf, 0, sizeof(buf));
	    sstrcpy(&ptr, "/FG=");
	    sstrcpy(&ptr, (p1=add_pat_escapes(action->incol->fg)));
	    sstrcpy(&ptr, "/BG=");
	    sstrcpy(&ptr, (p2=add_pat_escapes(action->incol->bg)));
	    /* the colors will be doubly escaped */
	    incol_act = add_pat_escapes(buf);
	    if(p1)
	      fs_give((void **)&p1);
	    if(p2)
	      fs_give((void **)&p2);
	}

	if(action->is_a_other){
	    char buf[256];

	    if(action->sort_is_set){
		sprintf(buf, "%.50s%.50s",
			sort_name(action->sortorder),
			action->revsort ? "/Reverse" : "");
		sort_act = add_pat_escapes(buf);
	    }

	    if(action->index_format)
	      iform_act = add_pat_escapes(action->index_format);

	    if(action->startup_rule != IS_NOTSET &&
	       (f = startup_rules(action->startup_rule)) != NULL)
	      start_act = S_OR_L(f);
	}

	if(action->is_a_role && action->from)
	  from_act = address_to_configstr(action->from);

	if(action->is_a_role && action->replyto)
	  replyto_act = address_to_configstr(action->replyto);

	if(action->is_a_filter){
	    if(action->folder){
		if(folder_act = pattern_to_config(action->folder)){
		    if(action->move_only_if_not_deleted)
		      filt_ifnotdel = cpystr("/NOTDEL=1");
		}
	    }

	    if(action->keyword_set)
	      keyword_set = pattern_to_config(action->keyword_set);

	    if(action->keyword_clr)
	      keyword_clr = pattern_to_config(action->keyword_clr);

	    if(!action->kill)
	      filt_nokill = cpystr("/NOKILL=1");
	    
	    if(action->non_terminating)
	      filt_nonterm = cpystr("/NONTERM=1");
	    
	    if(action->state_setting_bits){
		char buf[256];
		int  dval, nval, ival, aval;

		buf[0] = '\0';
		p = buf;

		convert_statebits_to_vals(action->state_setting_bits,
					  &dval, &aval, &ival, &nval);
		if(dval != ACT_STAT_LEAVE &&
		   (f = msg_state_types(dval)) != NULL)
		  filt_del_val = f->shortname;

		if(aval != ACT_STAT_LEAVE &&
		   (f = msg_state_types(aval)) != NULL)
		  filt_ans_val = f->shortname;

		if(ival != ACT_STAT_LEAVE &&
		   (f = msg_state_types(ival)) != NULL)
		  filt_imp_val = f->shortname;

		if(nval != ACT_STAT_LEAVE &&
		   (f = msg_state_types(nval)) != NULL)
		  filt_new_val = f->shortname;
	    }
	}
    }

    p = (char *)fs_get((strlen(nick ? nick : "Alternate Role") +
			strlen(comment ? comment : "") +
			strlen(to_pat ? to_pat : "") +
			strlen(from_pat ? from_pat : "") +
			strlen(sender_pat ? sender_pat : "") +
			strlen(cc_pat ? cc_pat : "") +
			strlen(recip_pat ? recip_pat : "") +
			strlen(partic_pat ? partic_pat : "") +
			strlen(news_pat ? news_pat : "") +
			strlen(subj_pat ? subj_pat : "") +
			strlen(alltext_pat ? alltext_pat : "") +
			strlen(bodytext_pat ? bodytext_pat : "") +
			strlen(arb_pat ? arb_pat : "") +
			strlen(scorei_pat ? scorei_pat : "") +
			strlen(keyword_pat ? keyword_pat : "") +
			strlen(charset_pat ? charset_pat : "") +
			strlen(age_pat ? age_pat : "") +
			strlen(size_pat ? size_pat : "") +
			strlen(category_cmd ? category_cmd : "") +
			strlen(category_pat ? category_pat : "") +
			strlen(category_lim ? category_lim : "") +
			strlen(fldr_pat ? fldr_pat : "") +
			strlen(abooks_pat ? abooks_pat : "") +
			strlen(sentdate ? sentdate : "") +
			strlen(inherit_nick ? inherit_nick : "") +
			strlen(score_act ? score_act : "") +
			strlen(from_act ? from_act : "") +
			strlen(replyto_act ? replyto_act : "") +
			strlen(fcc_act ? fcc_act : "") +
			strlen(litsig_act ? litsig_act : "") +
			strlen(cstm_act ? cstm_act : "") +
			strlen(smtp_act ? smtp_act : "") +
			strlen(nntp_act ? nntp_act : "") +
			strlen(sig_act ? sig_act : "") +
			strlen(incol_act ? incol_act : "") +
			strlen(sort_act ? sort_act : "") +
			strlen(iform_act ? iform_act : "") +
			strlen(start_act ? start_act : "") +
			strlen(filt_ifnotdel ? filt_ifnotdel : "") +
			strlen(filt_nokill ? filt_nokill : "") +
			strlen(filt_nonterm ? filt_nonterm : "") +
			(folder_act ? (strlen(folder_act) + 8) : 0) +
			strlen(keyword_set ? keyword_set : "") +
			strlen(keyword_clr ? keyword_clr : "") +
			strlen(templ_act ? templ_act : "") + 520)*sizeof(char));

    q = p;
    sstrcpy(&q, "pattern=\"/NICK=");

    if(nick){
	sstrcpy(&q, nick);
	fs_give((void **) &nick);
    }
    else
      sstrcpy(&q, "Alternate Role");

    if(comment){
	sstrcpy(&q, "/");
	sstrcpy(&q, "COMM=");
	sstrcpy(&q, comment);
	fs_give((void **) &comment);
    }

    if(to_pat){
	sstrcpy(&q, "/");
	if(to_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "TO=");
	sstrcpy(&q, to_pat);
	fs_give((void **) &to_pat);
    }

    if(from_pat){
	sstrcpy(&q, "/");
	if(from_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "FROM=");
	sstrcpy(&q, from_pat);
	fs_give((void **) &from_pat);
    }

    if(sender_pat){
	sstrcpy(&q, "/");
	if(sender_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "SENDER=");
	sstrcpy(&q, sender_pat);
	fs_give((void **) &sender_pat);
    }

    if(cc_pat){
	sstrcpy(&q,"/");
	if(cc_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q,"CC=");
	sstrcpy(&q, cc_pat);
	fs_give((void **) &cc_pat);
    }

    if(recip_pat){
	sstrcpy(&q, "/");
	if(recip_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "RECIP=");
	sstrcpy(&q, recip_pat);
	fs_give((void **) &recip_pat);
    }

    if(partic_pat){
	sstrcpy(&q, "/");
	if(partic_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "PARTIC=");
	sstrcpy(&q, partic_pat);
	fs_give((void **) &partic_pat);
    }

    if(news_pat){
	sstrcpy(&q, "/");
	if(news_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "NEWS=");
	sstrcpy(&q, news_pat);
	fs_give((void **) &news_pat);
    }

    if(subj_pat){
	sstrcpy(&q, "/");
	if(subj_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "SUBJ=");
	sstrcpy(&q, subj_pat);
	fs_give((void **)&subj_pat);
    }

    if(alltext_pat){
	sstrcpy(&q, "/");
	if(alltext_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "ALL=");
	sstrcpy(&q, alltext_pat);
	fs_give((void **) &alltext_pat);
    }

    if(bodytext_pat){
	sstrcpy(&q, "/");
	if(bodytext_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "BODY=");
	sstrcpy(&q, bodytext_pat);
	fs_give((void **) &bodytext_pat);
    }

    if(keyword_pat){
	sstrcpy(&q, "/");
	if(keyword_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "KEY=");
	sstrcpy(&q, keyword_pat);
	fs_give((void **) &keyword_pat);
    }

    if(charset_pat){
	sstrcpy(&q, "/");
	if(charset_not)
	  sstrcpy(&q, "!");

	sstrcpy(&q, "CHAR=");
	sstrcpy(&q, charset_pat);
	fs_give((void **) &charset_pat);
    }

    if(arb_pat){
	sstrcpy(&q, arb_pat);
	fs_give((void **)&arb_pat);
    }

    if(scorei_pat){
	sstrcpy(&q, "/SCOREI=");
	sstrcpy(&q, scorei_pat);
	fs_give((void **) &scorei_pat);
    }

    if(age_pat){
	sstrcpy(&q, "/AGE=");
	sstrcpy(&q, age_pat);
	fs_give((void **) &age_pat);
    }

    if(size_pat){
	sstrcpy(&q, "/SIZE=");
	sstrcpy(&q, size_pat);
	fs_give((void **) &size_pat);
    }

    if(category_cmd){
	sstrcpy(&q, "/CATCMD=");
	sstrcpy(&q, category_cmd);
	fs_give((void **) &category_cmd);
    }

    if(category_pat){
	sstrcpy(&q, "/CATVAL=");
	sstrcpy(&q, category_pat);
	fs_give((void **) &category_pat);
    }

    if(category_lim){
	sstrcpy(&q, "/CATLIM=");
	sstrcpy(&q, category_lim);
	fs_give((void **) &category_lim);
    }

    if(sentdate){
	sstrcpy(&q, sentdate);
	fs_give((void **) &sentdate);
    }

    if(fldr_type_pat){
	sstrcpy(&q, "/FLDTYPE=");
	sstrcpy(&q, fldr_type_pat);
    }

    if(fldr_pat){
	sstrcpy(&q, "/FOLDER=");
	sstrcpy(&q, fldr_pat);
	fs_give((void **) &fldr_pat);
    }

    if(afrom_type_pat){
	sstrcpy(&q, "/AFROM=");
	sstrcpy(&q, afrom_type_pat);
    }

    if(abooks_pat){
	sstrcpy(&q, "/ABOOKS=");
	sstrcpy(&q, abooks_pat);
	fs_give((void **) &abooks_pat);
    }

    if(stat_new_val){
	sstrcpy(&q, "/STATN=");
	sstrcpy(&q, stat_new_val);
    }

    if(stat_rec_val){
	sstrcpy(&q, "/STATR=");
	sstrcpy(&q, stat_rec_val);
    }

    if(stat_del_val){
	sstrcpy(&q, "/STATD=");
	sstrcpy(&q, stat_del_val);
    }

    if(stat_imp_val){
	sstrcpy(&q, "/STATI=");
	sstrcpy(&q, stat_imp_val);
    }

    if(stat_ans_val){
	sstrcpy(&q, "/STATA=");
	sstrcpy(&q, stat_ans_val);
    }

    if(stat_8bit_val){
	sstrcpy(&q, "/8BITS=");
	sstrcpy(&q, stat_8bit_val);
    }

    if(stat_bom_val){
	sstrcpy(&q, "/BOM=");
	sstrcpy(&q, stat_bom_val);
    }

    if(stat_boy_val){
	sstrcpy(&q, "/BOY=");
	sstrcpy(&q, stat_boy_val);
    }

    sstrcpy(&q, "\" action=\"");

    if(inherit_nick && *inherit_nick){
	sstrcpy(&q, "/INICK=");
	sstrcpy(&q, inherit_nick);
	fs_give((void **)&inherit_nick);
    }

    if(action){
	if(action->is_a_role)
	  sstrcpy(&q, "/ROLE=1");

	if(action->is_a_incol)
	  sstrcpy(&q, "/ISINCOL=1");

	if(action->is_a_score)
	  sstrcpy(&q, "/ISSCORE=1");

	if(action->is_a_filter){
	    /*
	     * Older pine will interpret a filter that has no folder
	     * as a Delete, even if we set it up here to be a Just Set
	     * State filter. Disable the filter for older versions in that
	     * case. If kill is set then Delete is what is supposed to
	     * happen, so that's ok. If folder is set then Move is what is
	     * supposed to happen, so ok.
	     */
	    if(!action->kill && !action->folder)
	      sstrcpy(&q, "/FILTER=2");
	    else
	      sstrcpy(&q, "/FILTER=1");
	}

	if(action->is_a_other)
	  sstrcpy(&q, "/OTHER=1");
    }

    if(score_act){
	sstrcpy(&q, "/SCORE=");
	sstrcpy(&q, score_act);
	fs_give((void **)&score_act);
    }

    if(from_act){
	sstrcpy(&q, "/FROM=");
	sstrcpy(&q, from_act);
      fs_give((void **) &from_act);
    }

    if(replyto_act){
	sstrcpy(&q, "/REPL=");
	sstrcpy(&q, replyto_act);
	fs_give((void **)&replyto_act);
    }

    if(fcc_act){
	sstrcpy(&q, "/FCC=");
	sstrcpy(&q, fcc_act);
	fs_give((void **)&fcc_act);
    }

    if(litsig_act){
	sstrcpy(&q, "/LSIG=");
	sstrcpy(&q, litsig_act);
	fs_give((void **)&litsig_act);
    }

    if(sig_act){
	sstrcpy(&q, "/SIG=");
	sstrcpy(&q, sig_act);
	fs_give((void **)&sig_act);
    }

    if(templ_act){
	sstrcpy(&q, "/TEMPLATE=");
	sstrcpy(&q, templ_act);
	fs_give((void **)&templ_act);
    }

    if(cstm_act){
	sstrcpy(&q, "/CSTM=");
	sstrcpy(&q, cstm_act);
	fs_give((void **)&cstm_act);
    }

    if(smtp_act){
	sstrcpy(&q, "/SMTP=");
	sstrcpy(&q, smtp_act);
	fs_give((void **)&smtp_act);
    }

    if(nntp_act){
	sstrcpy(&q, "/NNTP=");
	sstrcpy(&q, nntp_act);
	fs_give((void **)&nntp_act);
    }

    if(repl_val){
	sstrcpy(&q, "/RTYPE=");
	sstrcpy(&q, repl_val);
    }

    if(forw_val){
	sstrcpy(&q, "/FTYPE=");
	sstrcpy(&q, forw_val);
    }

    if(comp_val){
	sstrcpy(&q, "/CTYPE=");
	sstrcpy(&q, comp_val);
    }

    if(incol_act){
	sstrcpy(&q, "/INCOL=");
	sstrcpy(&q, incol_act);
	fs_give((void **)&incol_act);
    }

    if(sort_act){
	sstrcpy(&q, "/SORT=");
	sstrcpy(&q, sort_act);
	fs_give((void **)&sort_act);
    }

    if(iform_act){
	sstrcpy(&q, "/IFORM=");
	sstrcpy(&q, iform_act);
	fs_give((void **)&iform_act);
    }

    if(start_act){
	sstrcpy(&q, "/START=");
	sstrcpy(&q, start_act);
    }

    if(folder_act){
	sstrcpy(&q, "/FOLDER=");
	sstrcpy(&q, folder_act);
	fs_give((void **) &folder_act);
    }

    if(filt_ifnotdel){
	sstrcpy(&q, filt_ifnotdel);
	fs_give((void **) &filt_ifnotdel);
    }

    if(filt_nonterm){
	sstrcpy(&q, filt_nonterm);
	fs_give((void **) &filt_nonterm);
    }

    if(filt_nokill){
	sstrcpy(&q, filt_nokill);
	fs_give((void **) &filt_nokill);
    }

    if(filt_new_val){
	sstrcpy(&q, "/STATN=");
	sstrcpy(&q, filt_new_val);
    }

    if(filt_del_val){
	sstrcpy(&q, "/STATD=");
	sstrcpy(&q, filt_del_val);
    }

    if(filt_imp_val){
	sstrcpy(&q, "/STATI=");
	sstrcpy(&q, filt_imp_val);
    }

    if(filt_ans_val){
	sstrcpy(&q, "/STATA=");
	sstrcpy(&q, filt_ans_val);
    }

    if(keyword_set){
	sstrcpy(&q, "/KEYSET=");
	sstrcpy(&q, keyword_set);
	fs_give((void **) &keyword_set);
    }

    if(keyword_clr){
	sstrcpy(&q, "/KEYCLR=");
	sstrcpy(&q, keyword_clr);
	fs_give((void **) &keyword_clr);
    }

    *q++ = '\"';
    *q   = '\0';

    return(p);
}


void
convert_statebits_to_vals(bits, dval, aval, ival, nval)
    long bits;
    int *dval,
        *aval,
	*ival,
	*nval;
{
    if(dval)
      *dval = ACT_STAT_LEAVE;
    if(aval)
      *aval = ACT_STAT_LEAVE;
    if(ival)
      *ival = ACT_STAT_LEAVE;
    if(nval)
      *nval = ACT_STAT_LEAVE;

    if(ival){
	if(bits & F_FLAG)
	  *ival = ACT_STAT_SET;
	else if(bits & F_UNFLAG)
	  *ival = ACT_STAT_CLEAR;
    }

    if(aval){
	if(bits & F_ANS)
	  *aval = ACT_STAT_SET;
	else if(bits & F_UNANS)
	  *aval = ACT_STAT_CLEAR;
    }

    if(dval){
	if(bits & F_DEL)
	  *dval = ACT_STAT_SET;
	else if(bits & F_UNDEL)
	  *dval = ACT_STAT_CLEAR;
    }

    if(nval){
	if(bits & F_UNSEEN)
	  *nval = ACT_STAT_SET;
	else if(bits & F_SEEN)
	  *nval = ACT_STAT_CLEAR;
    }
}

    
/*
 * The "searched" bit will be set for each message which matches.
 *
 * Args:   patgrp -- Pattern to search with
 *         stream --
 *      searchset -- Restrict search to this set
 *        section -- Searching a section of the message, not the whole thing
 *      get_score -- Function to return the score for a message
 *          flags -- Most of these are flags to mail_search_full. However, we
 *                   overload the flags namespace and pass some flags of our
 *                   own in here that we pick off before calling mail_search.
 *                   Danger, danger, don't overlap with flag values defined
 *                   for c-client (that we want to use). Flags that we will
 *                   use here are:
 *                     MP_IN_CCLIENT_CB
 *                       If this is set we are in a callback from c-client
 *                       because some imap data arrived. We don't want to
 *                       call c-client again because it isn't re-entrant safe.
 *                       This is only a problem if we need to get the text of
 *                       a message to do the search, the envelope is cached
 *                       already.
 *                     MP_NOT
 *                       We want a ! of the patgrp in the search.
 *                   We also throw in SE_FREE for free, since we create
 *                   the search program here.
 *
 * Returns:   1 if any message in the searchset matches this pattern
 *            0 if no matches
 *           -1 if couldn't perform search because of no_fetch restriction
 */
int
match_pattern(patgrp, stream, searchset, section, get_score, flags)
    PATGRP_S   *patgrp;
    MAILSTREAM *stream;
    SEARCHSET  *searchset;
    char       *section;
    long        (*get_score) PROTO((MAILSTREAM *, long));
    long        flags;
{
    char         *charset = NULL;
    SEARCHPGM    *pgm;
    SEARCHSET    *s;
    MESSAGECACHE *mc;
    long          i, msgno = 0L;
    int           in_client_callback = 0, not = 0;

    dprint(7, (debugfile, "match_pattern\n"));

    /*
     * Is the current folder the right type and possibly the right specific
     * folder for a match?
     */
    if(!(patgrp && !patgrp->bogus && match_pattern_folder(patgrp, stream)))
      return(0);

    /*
     * NULL searchset means that there is no message to compare against.
     * This is a match if the folder type matches above (that gets
     * us here), and there are no patterns to match against.
     *
     * It is not totally clear what should be done in the case of an empty
     * search set.  If there is search criteria, and someone does something
     * that is not specific to any messages (composing from scratch,
     * forwarding an attachment), then we can't be sure what a user would
     * expect.  The original way was to just use the role, which we'll
     * preserve here.
     */
    if(!searchset)
      return(1);

    /*
     * change by sderr : match_pattern_folder will sometimes
     * accept NULL streams, but if we are not in a folder-type-only
     * match test, we don't
     */
    if(!stream)
      return(0);

    if(flags & MP_IN_CCLIENT_CB){
	in_client_callback++;
	flags &= ~MP_IN_CCLIENT_CB;
    }

    if(flags & MP_NOT){
	not++;
	flags &= ~MP_NOT;
    }

    flags |= SE_FREE;

    if(patgrp->stat_bom != PAT_STAT_EITHER){
	if(patgrp->stat_bom == PAT_STAT_YES){
	    if(!ps_global->beginning_of_month){
		return(0);
	    }
	}
	else if(patgrp->stat_bom == PAT_STAT_NO){
	    if(ps_global->beginning_of_month){
		return(0);
	    }
	}
    }

    if(patgrp->stat_boy != PAT_STAT_EITHER){
	if(patgrp->stat_boy == PAT_STAT_YES){
	    if(!ps_global->beginning_of_year){
		return(0);
	    }
	}
	else if(patgrp->stat_boy == PAT_STAT_NO){
	    if(ps_global->beginning_of_year){
		return(0);
	    }
	}
    }

    if(in_client_callback && is_imap_stream(stream)
       && (patgrp->alltext || patgrp->bodytext))
      return(-1);

    pgm = match_pattern_srchpgm(patgrp, stream, &charset, searchset);
    if(not && !(is_imap_stream(stream) && !modern_imap_stream(stream))){
	SEARCHPGM *srchpgm;

	srchpgm = pgm;
	pgm = mail_newsearchpgm();
	pgm->not = mail_newsearchpgmlist();
	pgm->not->pgm = srchpgm;
    }

    if((patgrp->alltext || patgrp->bodytext)
       && (!is_imap_stream(stream) || modern_imap_stream(stream)))
      /*
       * Cache isn't going to work. Search on server.
       * Except that is likely to not work on an old imap server because
       * the OR criteria won't work and we are likely to have some ORs.
       * So turn off the NOSERVER flag (and search on server if remote)
       * unless the server is an old server. It doesn't matter if we
       * turn if off if it's not an imap stream, but we do it anyway.
       */
      flags &= ~SO_NOSERVER;

    if(section){
	int charset_unknown = 0;

	/*
	 * Mail_search_full only searches the top-level msg. We want to
	 * search an attached msg instead. First do the stuff
	 * that mail_search_full would have done before calling
	 * mail_search_msg, then call mail_search_msg with a section number.
	 * Mail_search_msg does take a section number even though
	 * mail_search_full doesn't.
	 */

	/*
	 * We'll only ever set section if the searchset is a single message.
	 */
	if(pgm->msgno->next == NULL && pgm->msgno->first == pgm->msgno->last)
	  msgno = pgm->msgno->first;

	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->searched = NIL;

	if(charset && *charset &&  /* convert if charset not ASCII or UTF-8 */
	  !(((charset[0] == 'U') || (charset[0] == 'u')) &&
	    ((((charset[1] == 'S') || (charset[1] == 's')) &&
	      (charset[2] == '-') &&
	      ((charset[3] == 'A') || (charset[3] == 'a')) &&
	      ((charset[4] == 'S') || (charset[4] == 's')) &&
	      ((charset[5] == 'C') || (charset[5] == 'c')) &&
	      ((charset[6] == 'I') || (charset[6] == 'i')) &&
	      ((charset[7] == 'I') || (charset[7] == 'i')) && !charset[8]) ||
	     (((charset[1] == 'T') || (charset[1] == 't')) &&
	      ((charset[2] == 'F') || (charset[2] == 'f')) &&
	      (charset[3] == '-') && (charset[4] == '8') && !charset[5])))){
	    if(utf8_text(NIL,charset,NIL,T))
	      utf8_searchpgm (pgm,charset);
	    else
	      charset_unknown++;
	}

	if(!charset_unknown && mail_search_msg(stream,msgno,section,pgm)
	   && msgno > 0L && msgno <= stream->nmsgs
	   && (mc = mail_elt(stream, msgno)))
	  mc->searched = T;

	if(flags & SE_FREE)
	  mail_free_searchpgm(&pgm);
    }
    else{
	/* 
	 * Here we could be checking on the return value to see if
	 * the search was "successful" or not.  It may be the case
	 * that we'd want to stop trying filtering if we got some
	 * sort of error, but for now we would just continue on
	 * to the next filter.
	 */
	pine_mail_search_full(stream, charset, pgm, flags);
    }

    /* we searched without the not, reverse it */
    if(not && is_imap_stream(stream) && !modern_imap_stream(stream)){
	for(msgno = 1L; msgno < mn_get_total(sp_msgmap(stream)); msgno++)
	  if(stream && msgno && msgno <= stream->nmsgs
	     && (mc=mail_elt(stream,msgno)) && mc->searched)
	    mc->searched = NIL;
	  else
	    mc->searched = T;
    }

    if(charset)
      fs_give((void **)&charset);

    /* check scores */
    if(get_score && scores_are_used(SCOREUSE_GET) && patgrp->do_score){
	char      *savebits;
	SEARCHSET *ss;

	/*
	 * Get_score may call build_header_line recursively (we may
	 * be in build_header_line now) so we have to preserve and
	 * restore the sequence bits.
	 */
	savebits = (char *)fs_get((stream->nmsgs+1) * sizeof(char));

	for(i = 1L; i <= stream->nmsgs; i++){
	    if((mc = mail_elt(stream, i)) != NULL){
		savebits[i] = mc->sequence;
		mc->sequence = 0;
	    }
	}

	/*
	 * Build a searchset which will get all the scores that we
	 * need but not more.
	 */
	for(s = searchset; s; s = s->next)
	  for(msgno = s->first; msgno <= s->last; msgno++)
	    if(msgno > 0L && msgno <= stream->nmsgs
	       && (mc = mail_elt(stream, msgno)) && mc->searched
	       && get_msg_score(stream, msgno) == SCORE_UNDEF)
	      mc->sequence = 1;
	
	if((ss = build_searchset(stream)) != NULL){
	    (void)calculate_some_scores(stream, ss, in_client_callback);
	    mail_free_searchset(&ss);
	}

	/*
	 * Now check the scores versus the score intervals to see if
	 * any of the messages which have matched up to this point can
	 * be tossed because they don't match the score interval.
	 */
	for(s = searchset; s; s = s->next)
	  for(msgno = s->first; msgno <= s->last; msgno++)
	    if(msgno > 0L && msgno <= stream->nmsgs
	       && (mc = mail_elt(stream, msgno)) && mc->searched){
		long score;

		score = (*get_score)(stream, msgno);

		/*
		 * If the score is outside all of the intervals,
		 * turn off the searched bit.
		 * So that means we check each interval and if
		 * it is inside any interval we stop and leave
		 * the bit set. If it is outside we keep checking.
		 */
		if(score != SCORE_UNDEF){
		    INTVL_S *iv;

		    for(iv = patgrp->score; iv; iv = iv->next)
		      if(score >= iv->imin && score <= iv->imax)
			break;
		    
		    if(!iv)
		      mc->searched = NIL;
		}
	    }

	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->sequence = savebits[i];
    
	fs_give((void **)&savebits);
    }

    /* if there are still matches, check for 8bit subject match */
    if(patgrp->stat_8bitsubj != PAT_STAT_EITHER)
      find_8bitsubj_in_messages(stream, searchset, patgrp->stat_8bitsubj, 1);

    /* if there are still matches, check for charset matches */
    if(patgrp->charsets)
      find_charsets_in_messages(stream, searchset, patgrp, 1);

    /* Still matches, check addrbook */
    if(patgrp->abookfrom != AFRM_EITHER)
      from_or_replyto_in_abook(stream, searchset, patgrp->abookfrom,
			       patgrp->abooks);

    /* Still matches? Run the categorization command on each msg. */
    if(patgrp->category_cmd && patgrp->category_cmd[0]){
	char **l;
	int exitval;
	char *cmd = NULL;
	char *just_arg0 = NULL;
	char *cmd_start, *cmd_end;
	int just_one = !(patgrp->category_cmd[1]);

	/* find the first command that exists on this host */
	for(l = patgrp->category_cmd; l && *l; l++){
	    cmd = cpystr(*l);
	    removing_quotes(cmd);
	    if(cmd){
		for(cmd_start = cmd;
		    *cmd_start && isspace(*cmd_start); cmd_start++)
		  ;
		
		for(cmd_end = cmd_start+1;
		    *cmd_end && !isspace(*cmd_end); cmd_end++)
		  ;
		
		just_arg0 = (char *) fs_get((cmd_end-cmd_start+1)
						    * sizeof(char));
		strncpy(just_arg0, cmd_start, cmd_end - cmd_start);
		just_arg0[cmd_end - cmd_start] = '\0';
	    }

	    if(valid_filter_command(&just_arg0))
	      break;
	    else{
		if(just_one){
		    if(can_access(just_arg0, ACCESS_EXISTS) != 0)
		      q_status_message1(SM_ORDER, 0, 3,
					"\"%s\" does not exist",
					just_arg0);
		    else
		      q_status_message1(SM_ORDER, 0, 3,
					"\"%s\" is not executable",
					just_arg0);
		}

		if(just_arg0)
		  fs_give((void **) &just_arg0);
		if(cmd)
		  fs_give((void **) &cmd);
	    }
	}

	if(!just_arg0 && !just_one)
	  q_status_message(SM_ORDER, 0, 3,
	    "None of the category cmds exists and is executable");

	/*
	 * If category_cmd isn't executable, it isn't a match.
	 */
	if(!just_arg0 || !cmd){
	    /* If we couldn't run the pipe command, we declare no match */
	    for(s = searchset; s; s = s->next)
	      for(msgno = s->first; msgno <= s->last; msgno++)
		if(msgno > 0L && msgno <= stream->nmsgs
		   && (mc=mail_elt(stream, msgno)) && mc->searched)
		  mc->searched = NIL;
	}
	else
	  for(s = searchset; s; s = s->next)
	    for(msgno = s->first; msgno <= s->last; msgno++)
	      if(msgno > 0L && msgno <= stream->nmsgs
	         && (mc=mail_elt(stream, msgno)) && mc->searched){

		/*
		 * If there was an error, or the exitval is out of
		 * range, then it is not a match.
		 * Default range is (0,0), which is right for matching
		 * bogofilter spam.
		 */
		if(test_message_with_cmd(stream, msgno, cmd,
					 patgrp->cat_lim, &exitval) != 0)
		  mc->searched = NIL;

		/* test exitval */
		if(mc->searched){
		  INTVL_S *iv;

		  if(patgrp->cat){
		    for(iv = patgrp->cat; iv; iv = iv->next)
		      if((long) exitval >= iv->imin
			 && (long) exitval <= iv->imax)
			break;
			
		    if(!iv)
		      mc->searched = NIL;  /* not in any of the intervals */
		  }
		  else{
		    /* default to interval containing only zero */
		    if(exitval != 0)
		      mc->searched = NIL;
		  }
		}
	      }

	if(just_arg0)
	  fs_give((void **) &just_arg0);
	if(cmd)
	  fs_give((void **) &cmd);
    }

    for(s = searchset; s; s = s->next)
      for(msgno = s->first; msgno > 0L && msgno <= s->last; msgno++)
        if(msgno > 0L && msgno <= stream->nmsgs
	   && (mc = mail_elt(stream, msgno)) && mc->searched)
	  return(1);

    return(0);
}


/*
 * Look through messages in searchset to see if they contain 8bit
 * characters in their subjects. All of the messages in
 * searchset should initially have the searched bit set. Turn off the
 * searched bit where appropriate.
 */
void
find_8bitsubj_in_messages(stream, searchset, stat_8bitsubj, saveseqbits)
    MAILSTREAM *stream;
    SEARCHSET  *searchset;
    int         stat_8bitsubj;
    int         saveseqbits;
{
    char         *savebits = NULL;
    SEARCHSET    *s, *ss = NULL;
    MESSAGECACHE *mc;
    unsigned long msgno;

    /*
     * If we are being called while in build_header_line we may
     * call build_header_line recursively. So save and restore the
     * sequence bits.
     */
    if(saveseqbits)
      savebits = (char *) fs_get((stream->nmsgs+1) * sizeof(char));

    for(msgno = 1L; msgno <= stream->nmsgs; msgno++){
	if((mc = mail_elt(stream, msgno)) != NULL){
	    if(savebits)
	      savebits[msgno] = mc->sequence;

	    mc->sequence = 0;
	}
    }

    /*
     * Build a searchset so we can look at all the envelopes
     * we need to look at but only those we need to look at.
     * Everything with the searched bit set is still a
     * possibility, so restrict to that set.
     */

    for(s = searchset; s; s = s->next)
      for(msgno = s->first; msgno <= s->last; msgno++)
	if(msgno > 0L && msgno <= stream->nmsgs
	   && (mc = mail_elt(stream, msgno)) && mc->searched)
	  mc->sequence = 1;

    ss = build_searchset(stream);

    for(s = ss; s; s = s->next){
	for(msgno = s->first; msgno <= s->last; msgno++){
	    ENVELOPE   *e;
	    SEARCHSET **sset;

	    if(!stream || msgno <= 0L || msgno > stream->nmsgs)
	      continue;

	    /*
	     * This causes the lookahead to fetch precisely
	     * the messages we want (in the searchset) instead
	     * of just fetching the next 20 sequential
	     * messages. If the searching so far has caused
	     * a sparse searchset in a large mailbox, the
	     * difference can be substantial.
	     */
	    sset = (SEARCHSET **) mail_parameters(stream,
						  GET_FETCHLOOKAHEAD,
						  (void *) stream);
	    if(sset)
	      *sset = s;

	    e = pine_mail_fetchenvelope(stream, msgno);
	    if(stat_8bitsubj == PAT_STAT_YES){
		if(e && e->subject){
		    char *p;

		    for(p = e->subject; *p; p++)
		      if(*p & 0x80)
			break;

		    if(!*p && msgno > 0L && msgno <= stream->nmsgs
		       && (mc = mail_elt(stream, msgno)))
		      mc->searched = NIL;
		}
		else if(msgno > 0L && msgno <= stream->nmsgs
			&& (mc = mail_elt(stream, msgno)))
		  mc->searched = NIL;
	    }
	    else if(stat_8bitsubj == PAT_STAT_NO){
		if(e && e->subject){
		    char *p;

		    for(p = e->subject; *p; p++)
		      if(*p & 0x80)
			break;

		    if(*p && msgno > 0L && msgno <= stream->nmsgs
		       && (mc = mail_elt(stream, msgno)))
		      mc->searched = NIL;
		}
	    }
	}
    }

    if(savebits){
	for(msgno = 1L; msgno <= stream->nmsgs; msgno++)
	  if((mc = mail_elt(stream, msgno)) != NULL)
	    mc->sequence = savebits[msgno];

	fs_give((void **) &savebits);
    }

    if(ss)
      mail_free_searchset(&ss);
}


/*
 * Returns 0 if ok, -1 if not ok.
 * If ok then exitval contains the exit value of the cmd.
 */
int
test_message_with_cmd(stream, msgno, cmd, char_limit, exitval)
    MAILSTREAM *stream;
    long        msgno;
    char       *cmd;
    long        char_limit;  /* limit testing to this many chars from body */
    int        *exitval;
{
    PIPE_S  *tpipe;
    gf_io_t  pc;
    int      status = 0, flags, err = 0;
    char    *resultfile = NULL, *pipe_err;

    if(cmd && cmd[0]){
	
	flags = PIPE_WRITE | PIPE_NOSHELL | PIPE_STDERR;

	dprint(7, (debugfile, "test_message_with_cmd(msgno=%ld cmd=%s)\n",
		msgno, cmd));

	if((tpipe = cmd_pipe_open(cmd, &resultfile, flags, &pc))){

	    prime_raw_pipe_getc(stream, msgno, char_limit, FT_PEEK);
	    gf_filter_init();
	    gf_link_filter(gf_nvtnl_local, NULL);
	    if(pipe_err = gf_pipe(raw_pipe_getc, pc)){
		q_status_message1(SM_ORDER|SM_DING, 3, 3,
				  "Internal Error: %.200s", pipe_err);
		err++;
	    }

	    /*
	     * Don't call new_mail in close_system_pipe because we're probably
	     * already here from new_mail and we don't want to get loopy.
	     */
	    status = close_system_pipe(&tpipe, exitval, 1);

	    /*
	     * This is a place where the command can put its output, which we
	     * are not interested in.
	     */
	    if(resultfile){
		(void) unlink(resultfile);
		fs_give((void **) &resultfile);
	    }

	    return((err || status) ? -1 : 0);
	}
    }

    return(-1);
}


/*
 * Look through messages in searchset to see if they contain any of the
 * charsets or scripts listed in charsets pattern. All of the messages in
 * searchset should initially have the searched bit set. Turn off the
 * searched bit where appropriate.
 */
void
find_charsets_in_messages(stream, searchset, patgrp, saveseqbits)
    MAILSTREAM *stream;
    SEARCHSET  *searchset;
    PATGRP_S   *patgrp;
    int         saveseqbits;
{
    char         *savebits = NULL;
    unsigned long msgno;
    MESSAGECACHE *mc;
    SEARCHSET    *s, *ss;

    if(!stream || !patgrp)
      return;

    /*
     * When we actually want to use charsets, we convert it into a list
     * of charsets instead of the mixed list of scripts and charsets and
     * we eliminate duplicates. This is more efficient when we actually
     * do the lookups and compares.
     */
    if(!patgrp->charsets_list){
	PATTERN_S    *cs;
	CHARSET      *cset;
	STRLIST_S    *sl = NULL, *newsl;
	unsigned long scripts = 0L;
	SCRIPT       *script;

	for(cs = patgrp->charsets; cs; cs = cs->next){
	    /*
	     * Run through the charsets pattern looking for
	     * scripts and set the corresponding script bits.
	     * If it isn't a script, it is a character set.
	     */
	    if(cs->substring && (script = utf8_script(cs->substring)))
	      scripts |= script->script;
	    else{
		/* add it to list as a specific character set */
		newsl = new_strlist();
		newsl->name = cpystr(cs->substring);
		if(compare_strlists_for_match(sl, newsl))  /* already in list */
		  free_strlist(&newsl);
		else{
		    newsl->next = sl;
		    sl = newsl;
		}
	    }
	}

	/*
	 * Now scripts has a bit set for each script the user
	 * specified in the charsets pattern. Go through all of
	 * the known charsets and include ones in these scripts.
	 */
	if(scripts){
	    for(cset = utf8_charset(NIL); cset && cset->name; cset++){
		if(cset->script & scripts){

		    /* filter this out of each script, not very useful */
		    if(!strucmp("ISO-2022-JP-2", cset->name)
		       || !strucmp("UTF-7", cset->name)
		       || !strucmp("UTF-8", cset->name))
		      continue;

		    /* add cset->name to the list */
		    newsl = new_strlist();
		    newsl->name = cpystr(cset->name);
		    if(compare_strlists_for_match(sl, newsl))
		      free_strlist(&newsl);
		    else{
			newsl->next = sl;
			sl = newsl;
		    }
		}
	    }
	}

	patgrp->charsets_list = sl;
    }

    /*
     * This may call build_header_line recursively because we may be in
     * build_header_line now. So we have to preserve and restore the
     * sequence bits since we want to use them here.
     */
    if(saveseqbits)
      savebits = (char *) fs_get((stream->nmsgs+1) * sizeof(char));

    for(msgno = 1L; msgno <= stream->nmsgs; msgno++){
	if((mc = mail_elt(stream, msgno)) != NULL){
	    if(savebits)
	      savebits[msgno] = mc->sequence;

	    mc->sequence = 0;
	}
    }


    /*
     * Build a searchset so we can look at all the bodies
     * we need to look at but only those we need to look at.
     * Everything with the searched bit set is still a
     * possibility, so restrict to that set.
     */

    for(s = searchset; s; s = s->next)
      for(msgno = s->first; msgno <= s->last; msgno++)
	if(msgno > 0L && msgno <= stream->nmsgs
	   && (mc = mail_elt(stream, msgno)) && mc->searched)
	  mc->sequence = 1;

    ss = build_searchset(stream);

    for(s = ss; s; s = s->next){
	for(msgno = s->first; msgno <= s->last; msgno++){
	    SEARCHSET **sset;

	    if(msgno <= 0L || msgno > stream->nmsgs)
	      continue;

	    /*
	     * This causes the lookahead to fetch precisely
	     * the messages we want (in the searchset) instead
	     * of just fetching the next 20 sequential
	     * messages. If the searching so far has caused
	     * a sparse searchset in a large mailbox, the
	     * difference can be substantial.
	     */
	    sset = (SEARCHSET **) mail_parameters(stream,
						  GET_FETCHLOOKAHEAD,
						  (void *) stream);
	    if(sset)
	      *sset = s;

	    if(patgrp->charsets_list
	       && charsets_present_in_msg(stream,msgno,patgrp->charsets_list)){
		if(patgrp->charsets->not){
		    if((mc = mail_elt(stream, msgno)))
		      mc->searched = NIL;
		}
		/* else leave it */
	    }
	    else{		/* charset isn't in message */
		if(!patgrp->charsets->not){
		    if((mc = mail_elt(stream, msgno)))
		      mc->searched = NIL;
		}
		/* else leave it */
	    }
	}
    }

    if(savebits){
	for(msgno = 1L; msgno <= stream->nmsgs; msgno++)
	  if((mc = mail_elt(stream, msgno)) != NULL)
	    mc->sequence = savebits[msgno];

	fs_give((void **) &savebits);
    }

    if(ss)
      mail_free_searchset(&ss);
}


/*
 * Look for any of the charsets in this particular message.
 *
 * Returns 1 if there is a match, 0 otherwise.
 */
int
charsets_present_in_msg(stream, rawmsgno, charsets)
    MAILSTREAM   *stream;
    unsigned long rawmsgno;
    STRLIST_S    *charsets;
{
    BODY       *body = NULL;
    ENVELOPE   *env = NULL;
    STRLIST_S  *msg_charsets = NULL;
    int         ret = 0;

    if(charsets && stream && rawmsgno > 0L && rawmsgno <= stream->nmsgs){
	env = pine_mail_fetchstructure(stream, rawmsgno, &body);
	collect_charsets_from_subj(env, &msg_charsets);
	collect_charsets_from_body(body, &msg_charsets);
	if(msg_charsets){
	    ret = compare_strlists_for_match(msg_charsets, charsets);
	    free_strlist(&msg_charsets);
	}
    }

    return(ret);
}


void
collect_charsets_from_subj(env, listptr)
    ENVELOPE *env;
    STRLIST_S **listptr;
{
    STRLIST_S *newsl;
    char      *text, *e;

    if(listptr && env && env->subject){
	/* find encoded word */
	for(text = env->subject; *text; text++){
	    if((*text == '=') && (text[1] == '?') && isalpha(text[2]) &&
	       (e = strchr(text+2,'?'))){
		*e = '\0';			/* tie off charset name */

		newsl = new_strlist();
		newsl->name = cpystr(text+2);
		*e = '?';

		if(compare_strlists_for_match(*listptr, newsl))
		  free_strlist(&newsl);
		else{
		    newsl->next = *listptr;
		    *listptr = newsl;
		}
	    }
	}
    }
}


/*
 * Check for any of the charsets in any of the charset params in
 * any of the text parts of the body of a message. Put them in the list
 * pointed to by listptr.
 */
void
collect_charsets_from_body(body, listptr)
    BODY       *body;
    STRLIST_S **listptr;
{
    PART      *part;
    char      *cset;

    if(listptr && body){
	switch(body->type){
          case TYPEMULTIPART:
	    for(part = body->nested.part; part; part = part->next)
	      collect_charsets_from_body(&part->body, listptr);

	    break;

          case TYPEMESSAGE:
	    if(!strucmp(body->subtype, "RFC822")){
	        collect_charsets_from_subj(body->nested.msg->env, listptr);
		collect_charsets_from_body(body->nested.msg->body, listptr);
		break;
	    }
	    /* else fall through to text case */

	  case TYPETEXT:
	    cset = rfc2231_get_param(body->parameter, "charset", NULL, NULL);
	    if(cset){
		STRLIST_S *newsl;

		newsl = new_strlist();
		newsl->name = cpystr(cset);

		if(compare_strlists_for_match(*listptr, newsl))
		  free_strlist(&newsl);
		else{
		    newsl->next = *listptr;
		    *listptr = newsl;
		}

	        fs_give((void **) &cset);
	    }

	    break;

	  default:			/* non-text terminal mode */
	    break;
	}
    }
}


/*
 * If any of the names in list1 is the same as any of the names in list2
 * then return 1, else return 0. Comparison is case independent.
 */
int
compare_strlists_for_match(list1, list2)
    STRLIST_S *list1, *list2;
{
    int        ret = 0;
    STRLIST_S *cs1, *cs2;

    for(cs1 = list1; !ret && cs1; cs1 = cs1->next)
      for(cs2 = list2; !ret && cs2; cs2 = cs2->next)
        if(cs1->name && cs2->name && !strucmp(cs1->name, cs2->name))
	  ret = 1;

    return(ret);
}


int
match_pattern_folder(patgrp, stream)
   PATGRP_S   *patgrp;
   MAILSTREAM *stream;
{
    int	       is_news;
    
    /* change by sderr : we match FLDR_ANY even if stream is NULL */
    return((patgrp->fldr_type == FLDR_ANY)
	   || (stream
	       && (((is_news = IS_NEWS(stream))
	            && patgrp->fldr_type == FLDR_NEWS)
	           || (!is_news && patgrp->fldr_type == FLDR_EMAIL)
	           || (patgrp->fldr_type == FLDR_SPECIFIC
		       && match_pattern_folder_specific(patgrp->folder,
						       stream, FOR_PATTERN)))));
}


/* 
 * Returns positive if this stream is open on one of the folders in the
 * folders argument, 0 otherwise.
 *
 * If FOR_PATTERN is set, this interprets simple names as nicknames in
 * the incoming collection, otherwise it treats simple names as being in
 * the primary collection.
 * If FOR_FILT is set, the folder names are detokenized before being used.
 */
int
match_pattern_folder_specific(folders, stream, flags)
    PATTERN_S  *folders;
    MAILSTREAM *stream;
    int         flags;
{
    PATTERN_S *p;
    int        match = 0;
    char      *patfolder, *free_this = NULL;

    dprint(8, (debugfile, "match_pattern_folder_specific\n"));

    if(!(stream && stream->mailbox && stream->mailbox[0]))
      return(0);

    /*
     * For each of the folders in the pattern, see if we get
     * a match. We're just looking for any match. If none match,
     * we return 0, otherwise we fall through and check the rest
     * of the pattern. The fact that the string is called "substring"
     * is not meaningful. We're just using the convenient pattern
     * structure to store a list of folder names. They aren't
     * substrings of names, they are the whole name.
     */
    for(p = folders; !match && p; p = p->next){
	free_this = NULL;
	if(flags & FOR_FILT)
	  patfolder = free_this = detoken_src(p->substring, FOR_FILT, NULL,
					      NULL, NULL, NULL);
	else
	  patfolder = p->substring;

	if(patfolder
	   && (!strucmp(patfolder, ps_global->inbox_name)
	       || !strcmp(patfolder, ps_global->VAR_INBOX_PATH))){
	    if(sp_flagged(stream, SP_INBOX))
	      match++;
	}
	else{
	    char      *fname;
	    char      *t, *streamfolder;
	    char       tmp1[MAILTMPLEN], tmp2[max(MAILTMPLEN,NETMAXMBX)];
	    CONTEXT_S *cntxt = NULL;

	    if(flags & FOR_PATTERN){
		/*
		 * See if patfolder is a nickname in the incoming collection.
		 * If so, use its real name instead.
		 */
		if(patfolder[0] &&
		   (ps_global->context_list->use & CNTXT_INCMNG) &&
		   (fname = (folder_is_nick(patfolder,
					    FOLDERS(ps_global->context_list),
					    0))))
		  patfolder = fname;
	    }
	    else{
		char *save_ref = NULL;

		/*
		 * If it's an absolute pathname, we treat is as a local file
		 * instead of interpreting it in the primary context.
		 */
		if(!is_absolute_path(patfolder)
		   && !(cntxt = default_save_context(ps_global->context_list)))
		  cntxt = ps_global->context_list;
		
		/*
		 * Because this check is independent of where the user is
		 * in the folder hierarchy and has nothing to do with that,
		 * we want to ignore the reference field built into the
		 * context. Zero it out temporarily here.
		 */
		if(cntxt && cntxt->dir){
		    save_ref = cntxt->dir->ref;
		    cntxt->dir->ref = NULL;
		}

		patfolder = context_apply(tmp1, cntxt, patfolder, sizeof(tmp1));
		if(save_ref)
		  cntxt->dir->ref = save_ref;
	    }

	    switch(patfolder[0]){
	      case '{':
		if(stream->mailbox[0] == '{' &&
		   same_stream(patfolder, stream) &&
		   (streamfolder = strindex(&stream->mailbox[1], '}')) &&
		   (t = strindex(&patfolder[1], '}')) &&
		   !strcmp(t+1, streamfolder+1))
		  match++;

		break;
	      
	      case '#':
	        if(!strcmp(patfolder, stream->mailbox))
		  match++;

		break;

	      default:
		t = (strlen(patfolder) < (MAILTMPLEN/2))
				? mailboxfile(tmp2, patfolder) : NULL;
		if(t && *t && !strcmp(t, stream->mailbox))
		  match++;

		break;
	    }
	}

	if(free_this)
	  fs_give((void **) &free_this);
    }

    return(match);
}


/*
 * generate a search program corresponding to the provided patgrp
 */
SEARCHPGM *
match_pattern_srchpgm(patgrp, stream, charsetp, searchset)
    PATGRP_S	*patgrp;
    MAILSTREAM	*stream;
    char       **charsetp;
    SEARCHSET	*searchset;
{
    SEARCHPGM	 *pgm, *tmppgm;
    SEARCHOR     *or;
    SEARCHSET	**sp;
    ROLE_ARGS_T	  rargs;

    rargs.multi = 0;
    rargs.cset = charsetp;
    rargs.ourcharset = (ps_global->VAR_CHAR_SET
			&& ps_global->VAR_CHAR_SET[0])
			 ? ps_global->VAR_CHAR_SET : NULL;

    pgm = mail_newsearchpgm();

    sp = &pgm->msgno;
    /* copy the searchset */
    while(searchset){
	SEARCHSET *s;

	s = mail_newsearchset();
	s->first = searchset->first;
	s->last  = searchset->last;
	searchset = searchset->next;
	*sp = s;
	sp = &s->next;
    }

    if(!patgrp)
      return(pgm);

    if(patgrp->subj){
	if(patgrp->subj->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("subject", patgrp->subj, tmppgm, &rargs);
    }

    if(patgrp->cc){
	if(patgrp->cc->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("cc", patgrp->cc, tmppgm, &rargs);
    }

    if(patgrp->from){
	if(patgrp->from->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("from", patgrp->from, tmppgm, &rargs);
    }

    if(patgrp->to){
	if(patgrp->to->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("to", patgrp->to, tmppgm, &rargs);
    }

    if(patgrp->sender){
	if(patgrp->sender->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("sender", patgrp->sender, tmppgm, &rargs);
    }

    if(patgrp->news){
	if(patgrp->news->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("newsgroups", patgrp->news, tmppgm, &rargs);
    }

    /* To OR Cc */
    if(patgrp->recip){
	if(patgrp->recip->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;
	
	or = next_or(&tmppgm->or);

	set_up_search_pgm("to", patgrp->recip, or->first, &rargs);
	set_up_search_pgm("cc", patgrp->recip, or->second, &rargs);
    }

    /* To OR Cc OR From */
    if(patgrp->partic){
	if(patgrp->partic->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	or = next_or(&tmppgm->or);

	set_up_search_pgm("to", patgrp->partic, or->first, &rargs);

	or->second->or = mail_newsearchor();
	set_up_search_pgm("cc", patgrp->partic, or->second->or->first, &rargs);
	set_up_search_pgm("from", patgrp->partic, or->second->or->second,
			  &rargs);
    }

    if(patgrp->arbhdr){
	ARBHDR_S *a;

	for(a = patgrp->arbhdr; a; a = a->next)
	  if(a->field && a->field[0] && a->p){
	      if(a->p->not)
	        tmppgm = next_not(pgm);
	      else
	        tmppgm = pgm;

	      set_up_search_pgm(a->field, a->p, tmppgm, &rargs);
	  }
    }

    if(patgrp->alltext){
	if(patgrp->alltext->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("alltext", patgrp->alltext, tmppgm, &rargs);
    }
    
    if(patgrp->bodytext){
	if(patgrp->bodytext->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("bodytext", patgrp->bodytext, tmppgm, &rargs);
    }

    if(patgrp->keyword){
	PATTERN_S *p_old, *p_new, *new_pattern = NULL, **nextp;
	char      *q;

	if(patgrp->keyword->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	/*
	 * The keyword entries may be nicknames instead of the actual
	 * keywords, so those need to be converted to actual keywords.
	 *
	 * If we search for keywords that are not defined for a folder
	 * we may get error messages back that we don't want instead of
	 * just no match. We will build a replacement pattern here which
	 * contains only the defined subset of the keywords.
	 */
	
	nextp = &new_pattern;

	for(p_old = patgrp->keyword; p_old; p_old = p_old->next){
	    q = nick_to_keyword(p_old->substring);
	    if(user_flag_index(stream, q) >= 0){
		p_new = (PATTERN_S *) fs_get(sizeof(*p_new));
		memset(p_new, 0, sizeof(*p_new));
		p_new->substring = cpystr(q);
		*nextp = p_new;
		nextp = &p_new->next;
	    }
	}

	/*
	 * If there are some matching keywords that are defined in
	 * the folder, then we are ok because we will match only if
	 * we match one of those. However, if the list is empty, then
	 * we can't just leave this part of the search program empty.
	 * That would result in a match instead of not a match.
	 * We can fake our way around the problem with NOT. If the
	 * list is empty we want the opposite, so we insert a NOT in
	 * front of an empty program. We may end up with NOT NOT if
	 * this was already NOT'd, but that's ok, too. Alternatively,
	 * we could undo the first NOT instead.
	 */

	if(new_pattern){
	    set_up_search_pgm("keyword", new_pattern, tmppgm, &rargs);
	    free_pattern(&new_pattern);
	}
	else
	  (void) next_not(tmppgm);	/* add NOT of something that matches,
					   so the NOT thing doesn't match   */
    }

    if(patgrp->do_age && patgrp->age){
	INTVL_S  *iv;
	SEARCHOR *or;

	tmppgm = pgm;

	for(iv = patgrp->age; iv; iv = iv->next){
	    if(iv->next){
		or = next_or(&tmppgm->or);
		set_search_by_age(iv, or->first, patgrp->age_uses_sentdate);
		tmppgm = or->second;
	    }
	    else
	      set_search_by_age(iv, tmppgm, patgrp->age_uses_sentdate);
	}
    }
    
    if(patgrp->do_size && patgrp->size){
	INTVL_S  *iv;
	SEARCHOR *or;

	tmppgm = pgm;

	for(iv = patgrp->size; iv; iv = iv->next){
	    if(iv->next){
		or = next_or(&tmppgm->or);
		set_search_by_size(iv, or->first);
		tmppgm = or->second;
	    }
	    else
	      set_search_by_size(iv, tmppgm);
	}
    }
    
    SETPGMSTATUS(patgrp->stat_new,pgm->unseen,pgm->seen);
    SETPGMSTATUS(patgrp->stat_rec,pgm->recent,pgm->old);
    SETPGMSTATUS(patgrp->stat_del,pgm->deleted,pgm->undeleted);
    SETPGMSTATUS(patgrp->stat_imp,pgm->flagged,pgm->unflagged);
    SETPGMSTATUS(patgrp->stat_ans,pgm->answered,pgm->unanswered);

    return(pgm);
}


SEARCHPGM *
next_not(pgm)
    SEARCHPGM *pgm;
{
    SEARCHPGMLIST *not, **not_ptr;

    if(!pgm)
      return(NULL);

    /* find next unused not slot */
    for(not = pgm->not; not && not->next; not = not->next)
      ;
    
    if(not)
      not_ptr = &not->next;
    else
      not_ptr = &pgm->not;
    
    /* allocate */
    *not_ptr = mail_newsearchpgmlist();

    return((*not_ptr)->pgm);
}


SEARCHOR *
next_or(startingor)
    SEARCHOR **startingor;
{
    SEARCHOR *or, **or_ptr;

    /* find next unused or slot */
    for(or = (*startingor); or && or->next; or = or->next)
      ;
    
    if(or)
      or_ptr = &or->next;
    else
      or_ptr = startingor;
    
    /* allocate */
    *or_ptr = mail_newsearchor();

    return(*or_ptr);
}


void
set_up_search_pgm(field, pattern, pgm, rargs)
    char        *field;
    PATTERN_S   *pattern;
    SEARCHPGM   *pgm;
    ROLE_ARGS_T *rargs;
{
    SEARCHOR *or;

    if(field && pattern && rargs && pgm){

	/*
	 * To is special because we want to use the ReSent-To header instead
	 * of the To header if it exists.  We set up something like:
	 *
	 * if((resent-to matches pat1 or pat2...)
	 *                  OR
	 *    (<resent-to doesn't exist> AND (to matches pat1 or pat2...)))
	 *
	 *  Some servers (Exchange, apparently) seem to have trouble with
	 *  the search for the empty string to decide if the header exists
	 *  or not. So, we will search for either the empty string OR the
	 *  header with a SPACE in it.
	 */
	if(!strucmp(field, "to")){
	    ROLE_ARGS_T local_args;

	    or = next_or(&pgm->or);

	    local_args = *rargs;
	    local_args.cset = rargs->cset;
	    add_type_to_pgm("resent-to", pattern, or->first, &local_args);

	    /* check for resent-to doesn't exist */
	    or->second->not = mail_newsearchpgmlist();

	    local_args.cset = NULL;
	    or->second->not->pgm->or = mail_newsearchor();
	    set_srch("resent-to", " ", or->second->not->pgm->or->first,
		     &local_args);
	    local_args.cset = NULL;
	    set_srch("resent-to",  "", or->second->not->pgm->or->second,
		     &local_args);

	    /* now add the real To search to second */
	    local_args.cset = rargs->cset;
	    add_type_to_pgm(field, pattern, or->second, &local_args);
	}
	else
	  add_type_to_pgm(field, pattern, pgm, rargs);
    }
}


void
add_type_to_pgm(field, pattern, pgm, rargs)
    char        *field;
    PATTERN_S   *pattern;
    SEARCHPGM   *pgm;
    ROLE_ARGS_T *rargs;
{
    PATTERN_S *p;
    SEARCHOR  *or;
    SEARCHPGM *notpgm, *tpgm;
    int        cnt = 0;

    if(field && pattern && rargs && pgm){
	/*
	 * Here is a weird bit of logic. What we want here is simply
	 *       A or B or C or D
	 * for all of the elements of pattern. Ors are a bit complicated.
	 * The list of ORs in the SEARCHPGM structure are ANDed together,
	 * not ORd together. It's for things like
	 *      Subject A or B  AND  From C or D
	 * The Subject part would be one member of the OR list and the From
	 * part would be another member of the OR list. Instead we want
	 * a big OR which may have more than two members (first and second)
	 * but the structure just has two members. So we have to build an
	 * OR tree and we build it by going down one branch of the tree
	 * instead of by balancing the branches.
	 *
	 *            or
	 *           /  \
	 *    first==A   second
	 *                /  \
	 *          first==B  second
	 *                     /  \
	 *               first==C  second==D
	 *
	 * There is an additional problem. Some servers don't like deeply
	 * nested logic in the SEARCH command. The tree above produces a
	 * fairly deeply nested command if the user wants to match on
	 * several different From addresses or Subjects...
	 * We use the tried and true equation
	 *
	 *        (A or B) == !(!A and !B)
	 *
	 * to change the deeply nested OR tree into ANDs which aren't nested.
	 * Right now we're only doing that if the nesting is fairly deep.
	 * We can think of some reasons to do that. First, we know that the
	 * OR thing works, that's what we've been using for a while and the
	 * only problem is the deep nesting. 2nd, it is easier to understand.
	 * 3rd, it looks dumb to use  NOT NOT A   instead of  A.
	 * It is probably dumb to mix the two, but what the heck.
	 * Hubert 2003-04-02
	 */
	for(p = pattern; p; p = p->next)
	  cnt++;

	if(cnt < 10){				/* use ORs if count is low */
	    for(p = pattern; p; p = p->next){
		if(p->next){
		    or = next_or(&pgm->or);

		    set_srch(field, p->substring ? p->substring : "",
			     or->first, rargs);
		    pgm = or->second;
		}
		else
		  set_srch(field, p->substring ? p->substring : "", pgm, rargs);
	    }
	}
	else{					/* else use ANDs */
	    /* ( A or B or C )  <=>  ! ( !A and !B and !C ) */

	    /* first, NOT of the whole thing */
	    notpgm = next_not(pgm);

	    /* then the not list is ANDed together */
	    for(p = pattern; p; p = p->next){
		tpgm = next_not(notpgm);
		set_srch(field, p->substring ? p->substring : "", tpgm, rargs);
	    }
	}
    }
}


void
set_srch(field, value, pgm, rargs)
    char        *field;
    char        *value;
    SEARCHPGM   *pgm;
    ROLE_ARGS_T *rargs;
{
    char        *decoded, *cs = NULL, *charset = NULL;
    STRINGLIST **list;

    if(!(field && value && rargs && pgm))
      return;

    if(!strucmp(field, "subject"))
      list = &pgm->subject;
    else if(!strucmp(field, "from"))
      list = &pgm->from;
    else if(!strucmp(field, "to"))
      list = &pgm->to;
    else if(!strucmp(field, "cc"))
      list = &pgm->cc;
    else if(!strucmp(field, "sender"))
      list = &pgm->sender;
    else if(!strucmp(field, "reply-to"))
      list = &pgm->reply_to;
    else if(!strucmp(field, "in-reply-to"))
      list = &pgm->in_reply_to;
    else if(!strucmp(field, "message-id"))
      list = &pgm->message_id;
    else if(!strucmp(field, "newsgroups"))
      list = &pgm->newsgroups;
    else if(!strucmp(field, "followup-to"))
      list = &pgm->followup_to;
    else if(!strucmp(field, "alltext"))
      list = &pgm->text;
    else if(!strucmp(field, "bodytext"))
      list = &pgm->body;
    else if(!strucmp(field, "keyword"))
      list = &pgm->keyword;
    else{
	set_srch_hdr(field, value, pgm, rargs);
	return;
    }

    if(!list)
      return;

    *list = mail_newstringlist();
    decoded = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				     SIZEOF_20KBUF, value, &cs);

    (*list)->text.data = (unsigned char *)cpystr(decoded);
    (*list)->text.size = strlen(decoded);

    if(rargs->cset && !rargs->multi){
	if(decoded != value)
	  charset = (cs && cs[0]) ? cs : rargs->ourcharset;
	else if(!is_ascii_string(decoded))
	  charset = rargs->ourcharset;

	if(charset){
	    if(*rargs->cset){
		if(strucmp(*rargs->cset, charset) != 0){
		    rargs->multi = 1;
		    if(rargs->ourcharset &&
		       strucmp(rargs->ourcharset, *rargs->cset) != 0){
			fs_give((void **)rargs->cset);
			*rargs->cset = cpystr(rargs->ourcharset);
		    }
		}
	    }
	    else
	      *rargs->cset = cpystr(charset);
	}
    }

    if(cs)
      fs_give((void **)&cs);
}


void
set_srch_hdr(field, value, pgm, rargs)
    char        *field;
    char        *value;
    SEARCHPGM   *pgm;
    ROLE_ARGS_T *rargs;
{
    char *decoded, *cs = NULL, *charset = NULL;
    SEARCHHEADER  **hdr;

    if(!(field && value && rargs && pgm))
      return;

    hdr = &pgm->header;
    if(!hdr)
      return;

    decoded = (char *)rfc1522_decode((unsigned char *)tmp_20k_buf,
				     SIZEOF_20KBUF, value, &cs);
    while(*hdr && (*hdr)->next)
      *hdr = (*hdr)->next;
      
    if(*hdr)
      (*hdr)->next = mail_newsearchheader(field, decoded);
    else
      *hdr = mail_newsearchheader(field, decoded);

    if(rargs->cset && !rargs->multi){
	if(decoded != value)
	  charset = (cs && cs[0]) ? cs : rargs->ourcharset;
	else if(!is_ascii_string(decoded))
	  charset = rargs->ourcharset;

	if(charset){
	    if(*rargs->cset){
		if(strucmp(*rargs->cset, charset) != 0){
		    rargs->multi = 1;
		    if(rargs->ourcharset &&
		       strucmp(rargs->ourcharset, *rargs->cset) != 0){
			fs_give((void **)rargs->cset);
			*rargs->cset = cpystr(rargs->ourcharset);
		    }
		}
	    }
	    else
	      *rargs->cset = cpystr(charset);
	}
    }

    if(cs)
      fs_give((void **)&cs);
}


void
set_search_by_age(age, pgm, age_uses_sentdate)
    INTVL_S   *age;
    SEARCHPGM *pgm;
    int        age_uses_sentdate;
{
    time_t         now, comparetime;
    struct tm     *tm;
    unsigned short i;

    if(!(age && pgm))
      return;

    now = time(0);

    if(age->imin >= 0L && age->imin == age->imax){
	comparetime = now;
	comparetime -= (age->imin * 86400L);
	tm = localtime(&comparetime);
	if(tm && tm->tm_year >= 70){
	    i = mail_shortdate(tm->tm_year - 70, tm->tm_mon + 1,
			       tm->tm_mday);
	    if(age_uses_sentdate)
	      pgm->senton = i;
	    else
	      pgm->on = i;
	}
    }
    else{
	/*
	 * The 20000's are just protecting against overflows.
	 * That's back past the start of email time, anyway.
	 */
	if(age->imin > 0L && age->imin < 20000L){
	    comparetime = now;
	    comparetime -= ((age->imin - 1L) * 86400L);
	    tm = localtime(&comparetime);
	    if(tm && tm->tm_year >= 70){
		i = mail_shortdate(tm->tm_year - 70, tm->tm_mon + 1,
				   tm->tm_mday);
		if(age_uses_sentdate)
		  pgm->sentbefore = i;
		else
		  pgm->before = i;
	    }
	}

	if(age->imax >= 0L && age->imax < 20000L){
	    comparetime = now;
	    comparetime -= (age->imax * 86400L);
	    tm = localtime(&comparetime);
	    if(tm && tm->tm_year >= 70){
		i = mail_shortdate(tm->tm_year - 70, tm->tm_mon + 1,
				   tm->tm_mday);
		if(age_uses_sentdate)
		  pgm->sentsince = i;
		else
		  pgm->since = i;
	    }
	}
    }
}


void
set_search_by_size(size, pgm)
    INTVL_S   *size;
    SEARCHPGM *pgm;
{
    time_t         now, comparetime;
    struct tm     *tm;
    unsigned short i;

    if(!(size && pgm))
      return;
    
    /*
     * INTVL_S intervals include the endpoints, pgm larger and smaller
     * do not include the endpoints.
     */
    if(size->imin != INTVL_UNDEF && size->imin > 0L)
      pgm->larger  = size->imin - 1L;

    if(size->imax != INTVL_UNDEF && size->imax >= 0L && size->imax != INTVL_INF)
      pgm->smaller = size->imax + 1L;
}


static char *extra_hdrs;

/*
 * Run through the patterns and note which headers we'll need to ask for
 * which aren't normally asked for and so won't be cached.
 */
void
calc_extra_hdrs()
{
    PAT_S    *pat = NULL;
    int       alloced_size;
    long      type = (ROLE_INCOL | ROLE_SCORE);
    ARBHDR_S *a;
    PAT_STATE pstate;
    char     *q, *p = NULL, *hdrs[MLCMD_COUNT + 1], **pp;
#define INITIALSIZE 1000

    q = (char *)fs_get((INITIALSIZE+1) * sizeof(char));
    q[0] = '\0';
    alloced_size = INITIALSIZE;
    p = q;

    /*
     * *ALWAYS* make sure Resent-To is in the set of
     * extra headers getting fetched.
     *
     * This is because we *will* reference it when we're
     * building header lines and thus want it fetched with
     * the standard envelope data.  Worse, in the IMAP case
     * we're called back from c-client with the envelope data
     * so we can format and display the index lines as they
     * arrive, so we have to ensure the resent-to field
     * is in the cache so we don't reenter c-client
     * to look for it from the callback.  Yeouch.
     */
    add_eh(&q, &p, "resent-to", &alloced_size);
    add_eh(&q, &p, "resent-date", &alloced_size);
    add_eh(&q, &p, "resent-from", &alloced_size);
    add_eh(&q, &p, "resent-cc", &alloced_size);
    add_eh(&q, &p, "resent-subject", &alloced_size);

    /*
     * Sniff at viewer-hdrs too so we can include them
     * if there are any...
     */
    for(pp = ps_global->VAR_VIEW_HEADERS; pp && *pp; pp++)
      if(non_eh(*pp))
	add_eh(&q, &p, *pp, &alloced_size);

    /*
     * Be sure to ask for List management headers too
     * since we'll offer their use in the message view
     */
    for(pp = rfc2369_hdrs(hdrs); *pp; pp++)
      add_eh(&q, &p, *pp, &alloced_size);

    if(nonempty_patterns(type, &pstate))
      for(pat = first_pattern(&pstate);
	  pat;
	  pat = next_pattern(&pstate)){
	  /*
	   * This section wouldn't be necessary if sender was retreived
	   * from the envelope. But if not, we do need to add it.
	   */
	  if(pat->patgrp && pat->patgrp->sender)
	    add_eh(&q, &p, "sender", &alloced_size);

	  if(pat->patgrp && pat->patgrp->arbhdr)
	    for(a = pat->patgrp->arbhdr; a; a = a->next)
	      if(a->field && a->field[0] && a->p && non_eh(a->field))
		add_eh(&q, &p, a->field, &alloced_size);
      }
    
    set_extra_hdrs(q);
    if(q)
      fs_give((void **)&q);
}


int
non_eh(field)
    char *field;
{
    char **t;
    static char *existing[] = {"subject", "from", "to", "cc", "sender",
			       "reply-to", "in-reply-to", "message-id",
			       "path", "newsgroups", "followup-to",
			       "references", NULL};

    /*
     * If it is one of these, we should already have it
     * from the envelope or from the extra headers c-client
     * already adds to the list (hdrheader and hdrtrailer
     * in imap4r1.c, Aug 99, slh).
     */
    for(t = existing; *t; t++)
      if(!strucmp(field, *t))
	return(FALSE);

    return(TRUE);
}


/*
 * Add field to extra headers string if not already there.
 */
void
add_eh(start, ptr, field, asize)
    char **start;
    char **ptr;
    char  *field;
    int   *asize;
{
      char *s;

      /* already there? */
      for(s = *start; s = srchstr(s, field); s++)
	if(s[strlen(field)] == SPACE || s[strlen(field)] == '\0')
	  return;
    
      /* enough space for it? */
      while(strlen(field) + (*ptr - *start) + 1 > *asize){
	  (*asize) *= 2;
	  fs_resize((void **)start, (*asize)+1);
	  *ptr = *start + strlen(*start);
      }

      if(*ptr > *start)
	sstrcpy(ptr, " ");

      sstrcpy(ptr, field);
}


void
set_extra_hdrs(hdrs)
    char *hdrs;
{
    free_extra_hdrs();
    if(hdrs && *hdrs)
      extra_hdrs = cpystr(hdrs);
}


char *
get_extra_hdrs()
{
    return(extra_hdrs);
}


void
free_extra_hdrs()
{
    if(extra_hdrs)
      fs_give((void **)&extra_hdrs);
}


int
is_ascii_string(str)
    char *str;
{
    if(!str)
      return(0);
    
    while(*str && isascii(*str))
      str++;
    
    return(*str == '\0');
}


void
free_patline(patline)
    PAT_LINE_S **patline;
{
    if(patline && *patline){
	free_patline(&(*patline)->next);
	if((*patline)->filename)
	  fs_give((void **)&(*patline)->filename);
	if((*patline)->filepath)
	  fs_give((void **)&(*patline)->filepath);
	free_pat(&(*patline)->first);
	fs_give((void **)patline);
    }
}


void
free_pat(pat)
    PAT_S **pat;
{
    if(pat && *pat){
	free_pat(&(*pat)->next);
	free_patgrp(&(*pat)->patgrp);
	free_action(&(*pat)->action);
	if((*pat)->raw)
	  fs_give((void **)&(*pat)->raw);

	fs_give((void **)pat);
    }
}


void
free_patgrp(patgrp)
    PATGRP_S **patgrp;
{
    if(patgrp && *patgrp){
	if((*patgrp)->nick)
	  fs_give((void **) &(*patgrp)->nick);

	if((*patgrp)->comment)
	  fs_give((void **) &(*patgrp)->comment);

	if((*patgrp)->category_cmd)
	  free_list_array(&(*patgrp)->category_cmd);

	if((*patgrp)->charsets_list)
	  free_strlist(&(*patgrp)->charsets_list);

	free_pattern(&(*patgrp)->to);
	free_pattern(&(*patgrp)->cc);
	free_pattern(&(*patgrp)->recip);
	free_pattern(&(*patgrp)->partic);
	free_pattern(&(*patgrp)->from);
	free_pattern(&(*patgrp)->sender);
	free_pattern(&(*patgrp)->news);
	free_pattern(&(*patgrp)->subj);
	free_pattern(&(*patgrp)->alltext);
	free_pattern(&(*patgrp)->bodytext);
	free_pattern(&(*patgrp)->keyword);
	free_pattern(&(*patgrp)->charsets);
	free_pattern(&(*patgrp)->folder);
	free_arbhdr(&(*patgrp)->arbhdr);
	free_intvl(&(*patgrp)->score);
	free_intvl(&(*patgrp)->age);
	fs_give((void **) patgrp);
    }
}


void
free_pattern(pattern)
    PATTERN_S **pattern;
{
    if(pattern && *pattern){
	free_pattern(&(*pattern)->next);
	if((*pattern)->substring)
	  fs_give((void **)&(*pattern)->substring);
	fs_give((void **)pattern);
    }
}


void
free_arbhdr(arbhdr)
    ARBHDR_S **arbhdr;
{
    if(arbhdr && *arbhdr){
	free_arbhdr(&(*arbhdr)->next);
	if((*arbhdr)->field)
	  fs_give((void **)&(*arbhdr)->field);
	free_pattern(&(*arbhdr)->p);
	fs_give((void **)arbhdr);
    }
}


void
free_intvl(intvl)
    INTVL_S **intvl;
{
    if(intvl && *intvl){
	free_intvl(&(*intvl)->next);
	fs_give((void **) intvl);
    }
}


void
free_action(action)
    ACTION_S **action;
{
    if(action && *action){
	if((*action)->from)
	  mail_free_address(&(*action)->from);
	if((*action)->replyto)
	  mail_free_address(&(*action)->replyto);
	if((*action)->fcc)
	  fs_give((void **)&(*action)->fcc);
	if((*action)->litsig)
	  fs_give((void **)&(*action)->litsig);
	if((*action)->sig)
	  fs_give((void **)&(*action)->sig);
	if((*action)->template)
	  fs_give((void **)&(*action)->template);
	if((*action)->cstm)
	  free_list_array(&(*action)->cstm);
	if((*action)->smtp)
	  free_list_array(&(*action)->smtp);
	if((*action)->nntp)
	  free_list_array(&(*action)->nntp);
	if((*action)->nick)
	  fs_give((void **)&(*action)->nick);
	if((*action)->inherit_nick)
	  fs_give((void **)&(*action)->inherit_nick);
	if((*action)->incol)
	  free_color_pair(&(*action)->incol);
	if((*action)->folder)
	  free_pattern(&(*action)->folder);
	if((*action)->index_format)
	  fs_give((void **)&(*action)->index_format);
	if((*action)->keyword_set)
	  free_pattern(&(*action)->keyword_set);
	if((*action)->keyword_clr)
	  free_pattern(&(*action)->keyword_clr);

	fs_give((void **)action);
    }
}


/*
 * Returns an allocated copy of the pat.
 *
 * Args   pat -- the source pat
 *
 * Returns a copy of pat.
 */
PAT_S *
copy_pat(pat)
    PAT_S *pat;
{
    PAT_S *new_pat = NULL;

    if(pat){
	new_pat = (PAT_S *)fs_get(sizeof(*new_pat));
	memset((void *)new_pat, 0, sizeof(*new_pat));

	new_pat->patgrp = copy_patgrp(pat->patgrp);
	new_pat->action = copy_action(pat->action);
    }

    return(new_pat);
}


/*
 * Returns an allocated copy of the patgrp.
 *
 * Args   patgrp -- the source patgrp
 *
 * Returns a copy of patgrp.
 */
PATGRP_S *
copy_patgrp(patgrp)
    PATGRP_S *patgrp;
{
    char     *p;
    PATGRP_S *new_patgrp = NULL;

    if(patgrp){
	new_patgrp = (PATGRP_S *)fs_get(sizeof(*new_patgrp));
	memset((void *)new_patgrp, 0, sizeof(*new_patgrp));

	if(patgrp->nick)
	  new_patgrp->nick = cpystr(patgrp->nick);
	
	if(patgrp->comment)
	  new_patgrp->comment = cpystr(patgrp->comment);
	
	if(patgrp->to){
	    p = pattern_to_string(patgrp->to);
	    new_patgrp->to = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->to->not = patgrp->to->not;
	}
	
	if(patgrp->from){
	    p = pattern_to_string(patgrp->from);
	    new_patgrp->from = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->from->not = patgrp->from->not;
	}
	
	if(patgrp->sender){
	    p = pattern_to_string(patgrp->sender);
	    new_patgrp->sender = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->sender->not = patgrp->sender->not;
	}
	
	if(patgrp->cc){
	    p = pattern_to_string(patgrp->cc);
	    new_patgrp->cc = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->cc->not = patgrp->cc->not;
	}
	
	if(patgrp->recip){
	    p = pattern_to_string(patgrp->recip);
	    new_patgrp->recip = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->recip->not = patgrp->recip->not;
	}
	
	if(patgrp->partic){
	    p = pattern_to_string(patgrp->partic);
	    new_patgrp->partic = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->partic->not = patgrp->partic->not;
	}
	
	if(patgrp->news){
	    p = pattern_to_string(patgrp->news);
	    new_patgrp->news = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->news->not = patgrp->news->not;
	}
	
	if(patgrp->subj){
	    p = pattern_to_string(patgrp->subj);
	    new_patgrp->subj = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->subj->not = patgrp->subj->not;
	}
	
	if(patgrp->alltext){
	    p = pattern_to_string(patgrp->alltext);
	    new_patgrp->alltext = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->alltext->not = patgrp->alltext->not;
	}
	
	if(patgrp->bodytext){
	    p = pattern_to_string(patgrp->bodytext);
	    new_patgrp->bodytext = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->bodytext->not = patgrp->bodytext->not;
	}
	
	if(patgrp->keyword){
	    p = pattern_to_string(patgrp->keyword);
	    new_patgrp->keyword = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->keyword->not = patgrp->keyword->not;
	}
	
	if(patgrp->charsets){
	    p = pattern_to_string(patgrp->charsets);
	    new_patgrp->charsets = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->charsets->not = patgrp->charsets->not;
	}

	if(patgrp->charsets_list)
	  new_patgrp->charsets_list = copy_strlist(patgrp->charsets_list);
	
	if(patgrp->arbhdr){
	    ARBHDR_S *aa, *a, *new_a;

	    aa = NULL;
	    for(a = patgrp->arbhdr; a; a = a->next){
		new_a = (ARBHDR_S *)fs_get(sizeof(*new_a));
		memset((void *)new_a, 0, sizeof(*new_a));

		if(a->field)
		  new_a->field = cpystr(a->field);

		if(a->p){
		    p = pattern_to_string(a->p);
		    new_a->p = string_to_pattern(p);
		    fs_give((void **)&p);
		    new_a->p->not = a->p->not;
		}

		new_a->isemptyval = a->isemptyval;

		if(aa){
		    aa->next = new_a;
		    aa = aa->next;
		}
		else{
		    new_patgrp->arbhdr = new_a;
		    aa = new_patgrp->arbhdr;
		}
	    }
	}

	new_patgrp->fldr_type = patgrp->fldr_type;

	if(patgrp->folder){
	    p = pattern_to_string(patgrp->folder);
	    new_patgrp->folder = string_to_pattern(p);
	    fs_give((void **)&p);
	}

	new_patgrp->abookfrom = patgrp->abookfrom;

	if(patgrp->abooks){
	    p = pattern_to_string(patgrp->abooks);
	    new_patgrp->abooks = string_to_pattern(p);
	    fs_give((void **)&p);
	}

	new_patgrp->do_score  = patgrp->do_score;
	if(patgrp->score){
	    INTVL_S *intvl, *iv, *new_iv;

	    intvl = NULL;
	    for(iv = patgrp->score; iv; iv = iv->next){
		new_iv = (INTVL_S *) fs_get(sizeof(*new_iv));
		memset((void *) new_iv, 0, sizeof(*new_iv));

		new_iv->imin = iv->imin;
		new_iv->imax = iv->imax;

		if(intvl){
		    intvl->next = new_iv;
		    intvl = intvl->next;
		}
		else{
		    new_patgrp->score = new_iv;
		    intvl = new_patgrp->score;
		}
	    }
	}

	new_patgrp->do_age    = patgrp->do_age;
	if(patgrp->age){
	    INTVL_S *intvl, *iv, *new_iv;

	    intvl = NULL;
	    for(iv = patgrp->age; iv; iv = iv->next){
		new_iv = (INTVL_S *) fs_get(sizeof(*new_iv));
		memset((void *) new_iv, 0, sizeof(*new_iv));

		new_iv->imin = iv->imin;
		new_iv->imax = iv->imax;

		if(intvl){
		    intvl->next = new_iv;
		    intvl = intvl->next;
		}
		else{
		    new_patgrp->age = new_iv;
		    intvl = new_patgrp->age;
		}
	    }
	}

	new_patgrp->age_uses_sentdate = patgrp->age_uses_sentdate;

	new_patgrp->do_size    = patgrp->do_size;
	if(patgrp->size){
	    INTVL_S *intvl, *iv, *new_iv;

	    intvl = NULL;
	    for(iv = patgrp->size; iv; iv = iv->next){
		new_iv = (INTVL_S *) fs_get(sizeof(*new_iv));
		memset((void *) new_iv, 0, sizeof(*new_iv));

		new_iv->imin = iv->imin;
		new_iv->imax = iv->imax;

		if(intvl){
		    intvl->next = new_iv;
		    intvl = intvl->next;
		}
		else{
		    new_patgrp->size = new_iv;
		    intvl = new_patgrp->size;
		}
	    }
	}

	new_patgrp->stat_new  = patgrp->stat_new;
	new_patgrp->stat_rec  = patgrp->stat_rec;
	new_patgrp->stat_del  = patgrp->stat_del;
	new_patgrp->stat_imp  = patgrp->stat_imp;
	new_patgrp->stat_ans  = patgrp->stat_ans;

	new_patgrp->stat_8bitsubj = patgrp->stat_8bitsubj;
	new_patgrp->stat_bom  = patgrp->stat_bom;
	new_patgrp->stat_boy  = patgrp->stat_boy;

	new_patgrp->do_cat    = patgrp->do_cat;
	if(patgrp->cat){
	    INTVL_S *intvl, *iv, *new_iv;

	    intvl = NULL;
	    for(iv = patgrp->cat; iv; iv = iv->next){
		new_iv = (INTVL_S *) fs_get(sizeof(*new_iv));
		memset((void *) new_iv, 0, sizeof(*new_iv));

		new_iv->imin = iv->imin;
		new_iv->imax = iv->imax;

		if(intvl){
		    intvl->next = new_iv;
		    intvl = intvl->next;
		}
		else{
		    new_patgrp->cat = new_iv;
		    intvl = new_patgrp->cat;
		}
	    }
	}

	if(patgrp->category_cmd)
	  new_patgrp->category_cmd = copy_list_array(patgrp->category_cmd);
    }

    return(new_patgrp);
}


/*
 * Returns an allocated copy of the action.
 *
 * Args   action -- the source action
 *
 * Returns a copy of action.
 */
ACTION_S *
copy_action(action)
    ACTION_S *action;
{
    ACTION_S *newaction = NULL;
    char     *p;

    if(action){
	newaction = (ACTION_S *)fs_get(sizeof(*newaction));
	memset((void *)newaction, 0, sizeof(*newaction));

	newaction->is_a_role   = action->is_a_role;
	newaction->is_a_incol  = action->is_a_incol;
	newaction->is_a_score  = action->is_a_score;
	newaction->is_a_filter = action->is_a_filter;
	newaction->is_a_other  = action->is_a_other;
	newaction->repl_type   = action->repl_type;
	newaction->forw_type   = action->forw_type;
	newaction->comp_type   = action->comp_type;
	newaction->scoreval    = action->scoreval;
	newaction->kill        = action->kill;
	newaction->state_setting_bits = action->state_setting_bits;
	newaction->move_only_if_not_deleted = action->move_only_if_not_deleted;
	newaction->non_terminating = action->non_terminating;
	newaction->sort_is_set = action->sort_is_set;
	newaction->sortorder   = action->sortorder;
	newaction->revsort     = action->revsort;
	newaction->startup_rule = action->startup_rule;

	if(action->from)
	  newaction->from = copyaddrlist(action->from);
	if(action->replyto)
	  newaction->replyto = copyaddrlist(action->replyto);
	if(action->cstm)
	  newaction->cstm = copy_list_array(action->cstm);
	if(action->smtp)
	  newaction->smtp = copy_list_array(action->smtp);
	if(action->nntp)
	  newaction->nntp = copy_list_array(action->nntp);
	if(action->fcc)
	  newaction->fcc = cpystr(action->fcc);
	if(action->litsig)
	  newaction->litsig = cpystr(action->litsig);
	if(action->sig)
	  newaction->sig = cpystr(action->sig);
	if(action->template)
	  newaction->template = cpystr(action->template);
	if(action->nick)
	  newaction->nick = cpystr(action->nick);
	if(action->inherit_nick)
	  newaction->inherit_nick = cpystr(action->inherit_nick);
	if(action->incol)
	  newaction->incol = new_color_pair(action->incol->fg,
					    action->incol->bg);
	if(action->folder){
	    p = pattern_to_string(action->folder);
	    newaction->folder = string_to_pattern(p);
	    fs_give((void **) &p);
	}

	if(action->keyword_set){
	    p = pattern_to_string(action->keyword_set);
	    newaction->keyword_set = string_to_pattern(p);
	    fs_give((void **) &p);
	}

	if(action->keyword_clr){
	    p = pattern_to_string(action->keyword_clr);
	    newaction->keyword_clr = string_to_pattern(p);
	    fs_give((void **) &p);
	}

	if(action->index_format)
	  newaction->index_format = cpystr(action->index_format);
    }

    return(newaction);
}


/*
 * Given a role, return an allocated role. If this role inherits from
 * another role, then do the correct inheriting so that the result is
 * the role we want to use. The inheriting that is done is just the set
 * of set- actions. This is for role stuff, no inheriting happens for scores
 * or for colors.
 *
 * Args   role -- The source role
 *
 * Returns a role.
 */
ACTION_S *
combine_inherited_role(role)
    ACTION_S *role;
{
    PAT_STATE pstate;
    PAT_S    *pat;

    /*
     * Protect against loops in the role inheritance.
     */
    if(role && role->is_a_role && nonempty_patterns(ROLE_DO_ROLES, &pstate))
      for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate))
	if(pat->action){
	    if(pat->action == role)
	      pat->action->been_here_before = 1;
	    else
	      pat->action->been_here_before = 0;
	}

    return(combine_inherited_role_guts(role));
}


ACTION_S *
combine_inherited_role_guts(role)
    ACTION_S *role;
{
    ACTION_S *newrole = NULL, *inherit_role = NULL;
    PAT_STATE pstate;

    if(role && role->is_a_role){
	newrole = (ACTION_S *)fs_get(sizeof(*newrole));
	memset((void *)newrole, 0, sizeof(*newrole));

	newrole->repl_type  = role->repl_type;
	newrole->forw_type  = role->forw_type;
	newrole->comp_type  = role->comp_type;
	newrole->is_a_role  = role->is_a_role;

	if(role->inherit_nick && role->inherit_nick[0] &&
	   nonempty_patterns(ROLE_DO_ROLES, &pstate)){
	    PAT_S    *pat;

	    /* find the inherit_nick pattern */
	    for(pat = first_pattern(&pstate);
		pat;
		pat = next_pattern(&pstate)){
		if(pat->patgrp &&
		   pat->patgrp->nick &&
		   !strucmp(role->inherit_nick, pat->patgrp->nick)){
		    /* found it, if it has a role, use it */
		    if(!pat->action->been_here_before){
			pat->action->been_here_before = 1;
			inherit_role = pat->action;
		    }

		    break;
		}
	    }

	    /*
	     * inherit_role might inherit further from other roles.
	     * In any case, we copy it so that we'll consistently have
	     * an allocated copy.
	     */
	    if(inherit_role){
		if(inherit_role->inherit_nick && inherit_role->inherit_nick[0])
		  inherit_role = combine_inherited_role_guts(inherit_role);
		else
		  inherit_role = copy_action(inherit_role);
	    }
	}

	if(role->from)
	  newrole->from = copyaddrlist(role->from);
	else if(inherit_role && inherit_role->from)
	  newrole->from = copyaddrlist(inherit_role->from);

	if(role->replyto)
	  newrole->replyto = copyaddrlist(role->replyto);
	else if(inherit_role && inherit_role->replyto)
	  newrole->replyto = copyaddrlist(inherit_role->replyto);

	if(role->fcc)
	  newrole->fcc = cpystr(role->fcc);
	else if(inherit_role && inherit_role->fcc)
	  newrole->fcc = cpystr(inherit_role->fcc);

	if(role->litsig)
	  newrole->litsig = cpystr(role->litsig);
	else if(inherit_role && inherit_role->litsig)
	  newrole->litsig = cpystr(inherit_role->litsig);

	if(role->sig)
	  newrole->sig = cpystr(role->sig);
	else if(inherit_role && inherit_role->sig)
	  newrole->sig = cpystr(inherit_role->sig);

	if(role->template)
	  newrole->template = cpystr(role->template);
	else if(inherit_role && inherit_role->template)
	  newrole->template = cpystr(inherit_role->template);

	if(role->cstm)
	  newrole->cstm = copy_list_array(role->cstm);
	else if(inherit_role && inherit_role->cstm)
	  newrole->cstm = copy_list_array(inherit_role->cstm);

	if(role->smtp)
	  newrole->smtp = copy_list_array(role->smtp);
	else if(inherit_role && inherit_role->smtp)
	  newrole->smtp = copy_list_array(inherit_role->smtp);

	if(role->nntp)
	  newrole->nntp = copy_list_array(role->nntp);
	else if(inherit_role && inherit_role->nntp)
	  newrole->nntp = copy_list_array(inherit_role->nntp);

	if(role->nick)
	  newrole->nick = cpystr(role->nick);

	if(inherit_role)
	  free_action(&inherit_role);
    }

    return(newrole);
}


/*
 * Allow user to choose a single item from a list of strings.
 *
 * Args    list -- Array of strings to choose from, NULL terminated.
 *        title -- For conf_scroll_screen
 *        pdesc -- For conf_scroll_screen
 *         help -- For conf_scroll_screen
 *       htitle -- For conf_scroll_screen
 *
 * Returns an allocated copy of the chosen item or NULL.
 */
char *
choose_item_from_list(list, title, pdesc, help, htitle)
    char   **list;
    char    *title;
    char    *pdesc;
    HelpType help;
    char    *htitle;
{
    LIST_SEL_S *listhead, *ls, *p;
    char      **t;
    char       *ret = NULL, *choice = NULL;

    /* build the LIST_SEL_S list */
    p = listhead = NULL;
    for(t = list; *t; t++){
	ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
	memset(ls, 0, sizeof(*ls));
	ls->item = cpystr(*t);
	
	if(p){
	    p->next = ls;
	    p = p->next;
	}
	else
	  listhead = p = ls;
    }

    if(!listhead)
      return(ret);

    if(!select_from_list_screen(listhead, SFL_NONE, title, pdesc,
				help, htitle))
      for(p = listhead; !choice && p; p = p->next)
	if(p->selected)
	  choice = p->item;

    if(choice)
      ret = cpystr(choice);
      
    free_list_sel(&listhead);

    return(ret);
}


void
free_list_sel(lsel)
    LIST_SEL_S **lsel;
{
    if(lsel && *lsel){
	free_list_sel(&(*lsel)->next);
	if((*lsel)->item)
	  fs_give((void **) &(*lsel)->item);

	if((*lsel)->display_item)
	  fs_give((void **) &(*lsel)->display_item);
	
	fs_give((void **) lsel);
    }
}


/*
 *  * * * * * * * *      RFC 2369 support routines      * * * * * * * *
 */

/*
 * * NOTE * These have to remain in sync with the MLCMD_* macros
 *	    in pine.h.  Sorry.
 */

static RFC2369FIELD_S rfc2369_fields[] = {
    {"List-Help",
     "get information about the list and instructions on how to join",
     "seek help"},
    {"List-Unsubscribe",
     "remove yourself from the list (Unsubscribe)",
     "UNsubscribe"},
    {"List-Subscribe",
     "add yourself to the list (Subscribe)",
     "Subscribe"},
    {"List-Post",
     "send a message to the entire list (Post)",
     "post a message"},
    {"List-Owner",
     "send a message to the list owner",
     "contact the list owner"},
    {"List-Archive",
     "view archive of messages sent to the list",
     "view the archive"}
};




char **
rfc2369_hdrs(hdrs)
    char **hdrs;
{
    int i;

    for(i = 0; i < MLCMD_COUNT; i++)
      hdrs[i] = rfc2369_fields[i].name;

    hdrs[i] = NULL;
    return(hdrs);
}



int
rfc2369_parse_fields(h, data)
    char      *h;
    RFC2369_S *data;
{
    char *ep, *nhp, *tp;
    int	  i, rv = FALSE;

    for(i = 0; i < MLCMD_COUNT; i++)
      data[i].field = rfc2369_fields[i];

    for(nhp = h; h; h = nhp){
	/* coerce h to start of field */
	for(ep = h;;)
	  if(tp = strpbrk(ep, "\015\012")){
	      if(strindex(" \t", *((ep = tp) + 2))){
		  *ep++ = ' ';		/* flatten continuation */
		  *ep++ = ' ';
		  for(; *ep; ep++)	/* advance past whitespace */
		    if(*ep == '\t')
		      *ep = ' ';
		    else if(*ep != ' ')
		      break;
	      }
	      else{
		  *ep = '\0';		/* tie off header data */
		  nhp = ep + 2;		/* start of next header */
		  break;
	      }
	  }
	  else{
	      while(*ep)		/* find the end of this line */
		ep++;

	      nhp = NULL;		/* no more header fields */
	      break;
	  }

	/* if length is within reason, see if we're interested */
	if(ep - h < MLCMD_REASON && rfc2369_parse(h, data))
	  rv = TRUE;
    }

    return(rv);
}


int
rfc2369_parse(h, data)
    char      *h;
    RFC2369_S *data;
{
    int   l, ifield, idata = 0;
    char *p, *p1, *url, *comment;

    /* look for interesting name's */
    for(ifield = 0; ifield < MLCMD_COUNT; ifield++)
      if(!struncmp(h, rfc2369_fields[ifield].name,
		   l = strlen(rfc2369_fields[ifield].name))
	 && *(h += l) == ':'){
	  /* unwrap any transport encodings */
	  if((p = (char *) rfc1522_decode((unsigned char *) tmp_20k_buf,
					  SIZEOF_20KBUF,
					  ++h, NULL)) == tmp_20k_buf)
	    strcpy(h, p);		/* assumption #383: decoding shrinks */

	  url = comment = NULL;
	  while(*h){
	      while(*h == ' ')
		h++;

	      switch(*h){
		case '<' :		/* URL */
		  if(p = strindex(h, '>')){
		      url = ++h;	/* remember where it starts */
		      *p = '\0';	/* tie it off */
		      h  = p + 1;	/* advance h */
		      for(p = p1 = url; *p1 = *p; p++)
			if(*p1 != ' ')
			  p1++;		/* remove whitespace ala RFC */
		  }
		  else
		    *h = '\0';		/* tie off junk */

		  break;

		case '(' :			/* Comment */
		  comment = rfc822_skip_comment(&h, LONGT);
		  break;

		case 'N' :			/* special case? */
		case 'n' :
		  if(ifield == MLCMD_POST
		     && (*(h+1) == 'O' || *(h+1) == 'o')
		     && (!*(h+2) || *(h+2) == ' ')){
		      ;			/* yup! */

		      url = h;
		      *(h + 2) = '\0';
		      h += 3;
		      break;
		  }

		default :
		  removing_trailing_white_space(h);
		  if(!url
		     && (url = rfc1738_scan(h, &l))
		     && url == h && l == strlen(h)){
		      removing_trailing_white_space(h);
		      data[ifield].data[idata].value = url;
		  }
		  else
		    data[ifield].data[idata].error = h;

		  return(1);		/* return junk */
	      }

	      while(*h == ' ')
		h++;

	      switch(*h){
		case ',' :
		  h++;

		case '\0':
		  if(url || (comment && *comment)){
		      data[ifield].data[idata].value = url;
		      data[ifield].data[idata].comment = comment;
		      url = comment = NULL;
		  }

		  if(++idata == MLCMD_MAXDATA)
		    *h = '\0';

		default :
		  break;
	      }
	  }
      }

    return(idata);
}
