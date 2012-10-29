/*
 * Program:	flock() emulation via fcntl() locking
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	10 April 2001
 * Last Edited:	25 April 2003
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2003 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */

/* Cygwin does not seem to have the design flaw in fcntl() locking that
 * most other systems do (see flocksim.c for details).  If some cretin
 * decides to implement that design flaw, then Cygwin will have to use
 * flocksim.  Also, we don't test NFS either
 */


#define flock flocksim		/* use ours instead of theirs */

#undef LOCK_SH
#define LOCK_SH 1		/* shared lock */
#undef LOCK_EX
#define LOCK_EX 2		/* exclusive lock */
#undef LOCK_NB
#define LOCK_NB 4		/* no blocking */
#undef LOCK_UN
#define LOCK_UN 8		/* unlock */


/* Function prototypes */

int flocksim (int fd,int operation);
