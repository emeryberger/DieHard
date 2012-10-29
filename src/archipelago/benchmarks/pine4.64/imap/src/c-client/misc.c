/*
 * Program:	Miscellaneous utility routines
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	5 July 1988
 * Last Edited:	27 April 2004
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 1988-2004 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 *
 * This original version of this file is
 * Copyright 1988 Stanford University
 * and was developed in the Symbolic Systems Resources Group of the Knowledge
 * Systems Laboratory at Stanford University in 1987-88, and was funded by the
 * Biomedical Research Technology Program of the NationalInstitutes of Health
 * under grant number RR-00785.
 */


#include <ctype.h>
#include "mail.h"
#include "osdep.h"
#include "misc.h"

/* Convert string to all uppercase
 * Accepts: string pointer
 * Returns: string pointer
 */

unsigned char *ucase (unsigned char *s)
{
  unsigned char *t;
				/* if lowercase covert to upper */
  for (t = s; *t; t++) if (!(*t & 0x80) && islower (*t)) *t = toupper (*t);
  return s;			/* return string */
}


/* Convert string to all lowercase
 * Accepts: string pointer
 * Returns: string pointer
 */

unsigned char *lcase (unsigned char *s)
{
  unsigned char *t;
				/* if uppercase covert to lower */
  for (t = s; *t; t++) if (!(*t & 0x80) && isupper (*t)) *t = tolower (*t);
  return s;			/* return string */
}

/* Copy string to free storage
 * Accepts: source string
 * Returns: free storage copy of string
 */

char *cpystr (const char *string)
{
  return string ? strcpy ((char *) fs_get (1 + strlen (string)),string) : NIL;
}


/* Copy text/size to free storage as sized text
 * Accepts: destination sized text
 *	    pointer to source text
 *	    size of source text
 * Returns: text as a char *
 */

char *cpytxt (SIZEDTEXT *dst,char *text,unsigned long size)
{
				/* flush old space */
  if (dst->data) fs_give ((void **) &dst->data);
				/* copy data in sized text */
  memcpy (dst->data = (unsigned char *)
	  fs_get ((size_t) (dst->size = size) + 1),text,(size_t) size);
  dst->data[size] = '\0';	/* tie off text */
  return (char *) dst->data;	/* convenience return */
}

/* Copy sized text to free storage as sized text
 * Accepts: destination sized text
 *	    source sized text
 * Returns: text as a char *
 */

char *textcpy (SIZEDTEXT *dst,SIZEDTEXT *src)
{
				/* flush old space */
  if (dst->data) fs_give ((void **) &dst->data);
				/* copy data in sized text */
  memcpy (dst->data = (unsigned char *)
	  fs_get ((size_t) (dst->size = src->size) + 1),
	  src->data,(size_t) src->size);
  dst->data[dst->size] = '\0';	/* tie off text */
  return (char *) dst->data;	/* convenience return */
}


/* Copy stringstruct to free storage as sized text
 * Accepts: destination sized text
 *	    source stringstruct
 * Returns: text as a char *
 */

char *textcpystring (SIZEDTEXT *text,STRING *bs)
{
  unsigned long i = 0;
				/* clear old space */
  if (text->data) fs_give ((void **) &text->data);
				/* make free storage space in sized text */
  text->data = (unsigned char *) fs_get ((size_t) (text->size = SIZE (bs)) +1);
  while (i < text->size) text->data[i++] = SNX (bs);
  text->data[i] = '\0';		/* tie off text */
  return (char *) text->data;	/* convenience return */
}


/* Copy stringstruct from offset to free storage as sized text
 * Accepts: destination sized text
 *	    source stringstruct
 *	    offset into stringstruct
 *	    size of source text
 * Returns: text as a char *
 */

char *textcpyoffstring (SIZEDTEXT *text,STRING *bs,unsigned long offset,
			unsigned long size)
{
  unsigned long i = 0;
				/* clear old space */
  if (text->data) fs_give ((void **) &text->data);
  SETPOS (bs,offset);		/* offset the string */
				/* make free storage space in sized text */
  text->data = (unsigned char *) fs_get ((size_t) (text->size = size) + 1);
  while (i < size) text->data[i++] = SNX (bs);
  text->data[i] = '\0';		/* tie off text */
  return (char *) text->data;	/* convenience return */
}

/* Returns index of rightmost bit in word
 * Accepts: pointer to a 32 bit value
 * Returns: -1 if word is 0, else index of rightmost bit
 *
 * Bit is cleared in the word
 */

unsigned long find_rightmost_bit (unsigned long *valptr)
{
  unsigned long value = *valptr;
  unsigned long bit = 0;
  if (!(value & 0xffffffff)) return 0xffffffff;
				/* binary search for rightmost bit */
  if (!(value & 0xffff)) value >>= 16, bit += 16;
  if (!(value & 0xff)) value >>= 8, bit += 8;
  if (!(value & 0xf)) value >>= 4, bit += 4;
  if (!(value & 0x3)) value >>= 2, bit += 2;
  if (!(value & 0x1)) value >>= 1, bit += 1;
  *valptr ^= (1 << bit);	/* clear specified bit */
  return bit;
}


/* Return minimum of two integers
 * Accepts: integer 1
 *	    integer 2
 * Returns: minimum
 */

long min (long i,long j)
{
  return ((i < j) ? i : j);
}


/* Return maximum of two integers
 * Accepts: integer 1
 *	    integer 2
 * Returns: maximum
 */

long max (long i,long j)
{
  return ((i > j) ? i : j);
}

/* Search, case-insensitive for ASCII characters
 * Accepts: base string
 *	    length of base string
 *	    pattern string
 *	    length of pattern string
 * Returns: T if pattern exists inside base, else NIL
 */

long search (unsigned char *base,long basec,unsigned char *pat,long patc)
{
  long i,j,k;
  int c;
  unsigned char mask[256];
  static unsigned char alphatab[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,223,223,223,223,223,223,223,223,223,223,223,223,223,223,223,
    223,223,223,223,223,223,223,223,223,223,223,255,255,255,255,255,
    255,223,223,223,223,223,223,223,223,223,223,223,223,223,223,223,
    223,223,223,223,223,223,223,223,223,223,223,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
    };
				/* validate arguments */
  if (base && (basec > 0) && pat && (basec >= patc)) {
    if (patc <= 0) return T;	/* empty pattern always succeeds */
    memset (mask,0,256);	/* initialize search validity mask */
    for (i = 0; i < patc; i++) if (!mask[c = pat[i]]) {
				/* mark single character if non-alphabetic */
      if (alphatab[c] & 0x20) mask[c] = T;
				/* else mark both cases */
      else mask[c & 0xdf] = mask[c | 0x20] = T;
    }
				/* Boyer-Moore type search */
    for (i = --patc; i < basec; i += (mask[c] ? 1 : (j + 1)))
      for (j = patc,c = base[k = i]; !((c ^ pat[j]) & alphatab[c]);
	   j--,c = base[--k])
	if (!j) return T;	/* found a match! */
  }
  return NIL;			/* pattern not found */
}

/* Create a hash table
 * Accepts: size of new table (note: should be a prime)
 * Returns: hash table
 */

HASHTAB *hash_create (size_t size)
{
  size_t i = sizeof (size_t) + size * sizeof (HASHENT *);
  HASHTAB *ret = (HASHTAB *) memset (fs_get (i),0,i);
  ret->size = size;
  return ret;
}


/* Destroy hash table
 * Accepts: hash table
 */

void hash_destroy (HASHTAB **hashtab)
{
  if (*hashtab) {
    hash_reset (*hashtab);	/* reset hash table */
    fs_give ((void **) hashtab);
  }
}


/* Reset hash table
 * Accepts: hash table
 */

void hash_reset (HASHTAB *hashtab)
{
  size_t i;
  HASHENT *ent,*nxt;
				/* free each hash entry */
  for (i = 0; i < hashtab->size; i++) if (ent = hashtab->table[i])
    for (hashtab->table[i] = NIL; ent; ent = nxt) {
      nxt = ent->next;		/* get successor */
      fs_give ((void **) &ent);	/* flush this entry */
    }
}

/* Calculate index into hash table
 * Accepts: hash table
 *	    entry name
 * Returns: index
 */

unsigned long hash_index (HASHTAB *hashtab,char *key)
{
  unsigned long i,ret;
				/* polynomial of letters of the word */
  for (ret = 0; i = (unsigned int) *key++; ret += i) ret *= HASHMULT;
  return ret % (unsigned long) hashtab->size;
}


/* Look up name in hash table
 * Accepts: hash table
 *	    key
 * Returns: associated data
 */

void **hash_lookup (HASHTAB *hashtab,char *key)
{
  HASHENT *ret;
  for (ret = hashtab->table[hash_index (hashtab,key)]; ret; ret = ret->next)
    if (!strcmp (key,ret->name)) return ret->data;
  return NIL;
}

/* Add entry to hash table
 * Accepts: hash table
 *	    key
 *	    associated data
 *	    number of extra data slots
 * Returns: hash entry
 * Caller is responsible for ensuring that entry isn't already in table
 */

HASHENT *hash_add (HASHTAB *hashtab,char *key,void *data,long extra)
{
  unsigned long i = hash_index (hashtab,key);
  size_t j = sizeof (HASHENT) + (extra * sizeof (void *));
  HASHENT *ret = (HASHENT *) memset (fs_get (j),0,j);
  ret->next = hashtab->table[i];/* insert as new head in this index */
  ret->name = key;		/* set up hash key */
  ret->data[0] = data;		/* and first data */
  return hashtab->table[i] = ret;
}


/* Look up name in hash table
 * Accepts: hash table
 *	    key
 *	    associated data
 *	    number of extra data slots
 * Returns: associated data
 */

void **hash_lookup_and_add (HASHTAB *hashtab,char *key,void *data,long extra)
{
  HASHENT *ret;
  unsigned long i = hash_index (hashtab,key);
  size_t j = sizeof (HASHENT) + (extra * sizeof (void *));
  for (ret = hashtab->table[i]; ret; ret = ret->next)
    if (!strcmp (key,ret->name)) return ret->data;
  ret = (HASHENT *) memset (fs_get (j),0,j);
  ret->next = hashtab->table[i];/* insert as new head in this index */
  ret->name = key;		/* set up hash key */
  ret->data[0] = data;		/* and first data */
  return (hashtab->table[i] = ret)->data;
}

/* Compare two unsigned longs
 * Accepts: first value
 *	    second value
 * Returns: -1 if l1 < l2, 0 if l1 == l2, 1 if l1 > l2
 */

int compare_ulong (unsigned long l1,unsigned long l2)
{
  if (l1 < l2) return -1;
  if (l1 > l2) return 1;
  return 0;
}


/* Compare two case-independent strings
 * Accepts: first string
 *	    second string
 * Returns: -1 if s1 < s2, 0 if s1 == s2, 1 if s1 > s2
 */

int compare_cstring (unsigned char *s1,unsigned char *s2)
{
  int i;
  if (!s1) return s2 ? -1 : 0;	/* empty string cases */
  else if (!s2) return 1;
  for (; *s1 && *s2; s1++,s2++)
    if (i = (compare_ulong (islower (*s1) ? toupper (*s1) : *s1,
			    islower (*s2) ? toupper (*s2) : *s2)))
      return i;			/* found a difference */
  if (*s1) return 1;		/* first string is longer */
  return *s2 ? -1 : 0;		/* second string longer : strings identical */
}


/* Compare case-independent string with sized text
 * Accepts: first string
 *	    sized text
 * Returns: -1 if s1 < s2, 0 if s1 == s2, 1 if s1 > s2
 */

int compare_csizedtext (unsigned char *s1,SIZEDTEXT *s2)
{
  int i;
  unsigned char *s;
  unsigned long j;
  if (!s1) return s2 ? -1 : 0;	/* null string cases */
  else if (!s2) return 1;
  for (s = (char *) s2->data,j = s2->size; *s1 && j; ++s1,++s,--j)
    if (i = (compare_ulong (isupper (*s1) ? tolower (*s1) : *s1,
			    isupper (*s) ? tolower (*s) : *s)))
      return i;			/* found a difference */
  if (*s1) return 1;		/* first string is longer */
  return j ? -1 : 0;		/* second string longer : strings identical */
}
