/*
 * Program:	Operating-system dependent routines -- SUN-OS 4.0 version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	11 May 1989
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

void *malloc (size_t byteSize);
void free (void *ptr);
void *realloc (void *oldptr,size_t newsize);

#include "os_sun.h"		/* now use regular SUN-OS file */
