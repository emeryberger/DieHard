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

/* Hash table operations */

#define HASHMULT 29		/* hash polynomial multiplier */

#define HASHENT struct hash_entry

HASHENT {
  HASHENT *next;		/* next entry with same hash code */
  char *name;			/* name of this hash entry */
  void *data[1];		/* data of this hash entry */
};


#define HASHTAB struct hash_table

HASHTAB {
  size_t size;			/* size of this table */
  HASHENT *table[1];		/* table */
};


/* KLUDGE ALERT!!!
 *
 * Yes, write() is overridden here instead of in osdep.  This
 * is because misc.h is one of the last files that most things #include, so
 * this should avoid problems with some system #include file.
 */

#define write safe_write


/* Some C compilers have these as macros */

#undef min
#undef max


/* And some C libraries have these as int functions */

#define min Min
#define max Max


/* Compatibility definitions */

#define pmatch(s,pat) \
  pmatch_full (s,pat,NIL)

/* Function prototypes */

unsigned char *ucase (unsigned char *string);
unsigned char *lcase (unsigned char *string);
char *cpystr (const char *string);
char *cpytxt (SIZEDTEXT *dst,char *text,unsigned long size);
char *textcpy (SIZEDTEXT *dst,SIZEDTEXT *src);
char *textcpystring (SIZEDTEXT *text,STRING *bs);
char *textcpyoffstring (SIZEDTEXT *text,STRING *bs,unsigned long offset,
			unsigned long size);
unsigned long find_rightmost_bit (unsigned long *valptr);
long min (long i,long j);
long max (long i,long j);
long search (unsigned char *base,long basec,unsigned char *pat,long patc);
HASHTAB *hash_create (size_t size);
void hash_destroy (HASHTAB **hashtab);
void hash_reset (HASHTAB *hashtab);
unsigned long hash_index (HASHTAB *hashtab,char *key);
void **hash_lookup (HASHTAB *hashtab,char *key);
HASHENT *hash_add (HASHTAB *hashtab,char *key,void *data,long extra);
void **hash_lookup_and_add (HASHTAB *hashtab,char *key,void *data,long extra);
int compare_ulong (unsigned long l1,unsigned long l2);
int compare_cstring (unsigned char *s1,unsigned char *s2);
int compare_csizedtext (unsigned char *s1,SIZEDTEXT *s2);
