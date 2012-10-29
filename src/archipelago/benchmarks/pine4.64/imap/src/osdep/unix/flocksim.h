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
 * Last Edited:	22 January 2002
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2002 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
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

/* Safe locking definitions */

extern int lockslavep;		/* non-zero means slave process */

#undef SAFE_DELETE
#define SAFE_DELETE(dtb,stream,mbx) (dtb->flags & DR_LOCKING) ? \
  safe_delete (dtb,stream,mbx) : (*dtb->mbxdel) (stream,mbx)
#undef SAFE_RENAME
#define SAFE_RENAME(dtb,stream,old,newname) (dtb->flags & DR_LOCKING) ? \
  safe_rename (dtb,stream,old,newname) : (*dtb->mbxren) (stream,old,newname)
#undef SAFE_STATUS
#define SAFE_STATUS(dtb,stream,mbx,bits) (dtb->flags & DR_LOCKING) ? \
  safe_status (dtb,stream,mbx,bits) : (*dtb->status) (stream,mbx,bits)
#undef SAFE_SCAN_CONTENTS
#define SAFE_SCAN_CONTENTS(dtb,name,contents,csiz,fsiz) \
  (!dtb || (dtb->flags & DR_LOCKING)) ? \
   safe_scan_contents (name,contents,csiz,fsiz) : \
   dummy_scan_contents (name,contents,csiz,fsiz)
#undef SAFE_COPY
#define SAFE_COPY(dtb,stream,seq,mbx,bits) (dtb->flags & DR_LOCKING) ? \
  safe_copy (dtb,stream,seq,mbx,bits) : (*dtb->copy) (stream,seq,mbx,bits)
#undef SAFE_APPEND
#define SAFE_APPEND(dtb,stream,mbx,af,data) (dtb->flags & DR_LOCKING) ? \
  safe_append (dtb,stream,mbx,af,data) : (*dtb->append) (stream,mbx,af,data)

#undef MM_EXISTS
#define MM_EXISTS (lockslavep ? slave_exists : mm_exists)
#undef MM_EXPUNGED
#define MM_EXPUNGED (lockslavep ? slave_expunged : mm_expunged)
#undef MM_FLAGS
#define MM_FLAGS (lockslavep ? slave_flags : mm_flags)
#undef MM_NOTIFY
#define MM_NOTIFY (lockslavep ? slave_notify : mm_notify)
#undef MM_STATUS
#define MM_STATUS (lockslavep ? slave_status : mm_status)
#undef MM_LOG
#define MM_LOG (lockslavep ? slave_log : mm_log)
#undef MM_CRITICAL
#define MM_CRITICAL (lockslavep ? slave_critical : mm_critical)
#undef MM_NOCRITICAL
#define MM_NOCRITICAL (lockslavep ? slave_nocritical : mm_nocritical)
#undef MM_DISKERROR
#define MM_DISKERROR (lockslavep ? slave_diskerror : mm_diskerror)
#undef MM_FATAL
#define MM_FATAL (lockslavep ? slave_fatal : mm_fatal)
#undef MM_APPEND
#define MM_APPEND(af) (lockslavep ? slave_append : (*af))

/* Function prototypes */

int flocksim (int fd,int operation);

long safe_delete (DRIVER *dtb,MAILSTREAM *stream,char *mbx);
long safe_rename (DRIVER *dtb,MAILSTREAM *stream,char *old,char *newname);
long safe_status (DRIVER *dtb,MAILSTREAM *stream,char *mbx,long flags);
long safe_scan_contents (char *name,char *contents,unsigned long csiz,
			 unsigned long fsiz);
long safe_copy (DRIVER *dtb,MAILSTREAM *stream,char *seq,char *mbx,long flags);
long safe_append (DRIVER *dtb,MAILSTREAM *stream,char *mbx,append_t af,
		  void *data);

void slave_exists (MAILSTREAM *stream,unsigned long number);
void slave_expunged (MAILSTREAM *stream,unsigned long number);
void slave_flags (MAILSTREAM *stream,unsigned long number);
void slave_notify (MAILSTREAM *stream,char *string,long errflg);
void slave_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status);
void slave_log (char *string,long errflg);
void slave_critical (MAILSTREAM *stream);
void slave_nocritical (MAILSTREAM *stream);
long slave_diskerror (MAILSTREAM *stream,long errcode,long serious);
void slave_fatal (char *string);
long slave_append (MAILSTREAM *stream,void *data,char **flags,char **date,
		   STRING **message);
long slaveproxycopy (MAILSTREAM *stream,char *sequence,char *mailbox,
		     long options);
