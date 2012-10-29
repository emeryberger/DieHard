/*
 *
 * $Id: context.h 13727 2004-07-01 17:27:20Z hubert $
 *
 * Program:	Mailbox Context Management
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 *
 * Pine and Pico are registered trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior written
 * permission of the University of Washington.
 *
 * Pine, Pico, and Pilot software and its included text are Copyright
 * 1989-2004 by the University of Washington.
 *
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this distribution.
 */

#ifndef _CONTEXT_INCLUDED
#define _CONTEXT_INCLUDED

extern char *current_context;
typedef void  (*find_return) PROTO(());

/*
 * Prototypes
 */
void	    context_mailbox PROTO((char *));
void	    context_bboard PROTO((char *));
void	    context_fqn_to_an PROTO((find_return, char *, char *));
int	    context_isambig PROTO((char *));
int	    context_isremote PROTO((CONTEXT_S *));
char	   *context_digest PROTO((char *, char *, char *, char *, char *,
				  size_t));
char	   *context_apply PROTO((char *, CONTEXT_S *, char *, size_t));
long	    context_create PROTO((CONTEXT_S *, MAILSTREAM *, char *));
MAILSTREAM *context_open PROTO((CONTEXT_S *, MAILSTREAM *, char *, long,
				long *));
long	    context_status PROTO((CONTEXT_S *, MAILSTREAM *, char *, long));
long	    context_status_full PROTO((CONTEXT_S *, MAILSTREAM *, char *,
				       long, unsigned long *, unsigned long *));
long	    context_status_streamp PROTO((CONTEXT_S *, MAILSTREAM **,
					  char *, long));
long	    context_status_streamp_full PROTO((CONTEXT_S *, MAILSTREAM **,
					  char *, long,
					  unsigned long *, unsigned long *));
long	    context_rename PROTO((CONTEXT_S *, MAILSTREAM *, char *, char *));
long	    context_delete PROTO((CONTEXT_S *, MAILSTREAM *, char *));
long	    context_append PROTO((CONTEXT_S *, MAILSTREAM *, char *, \
				  STRING *));
long	    context_append_full PROTO((CONTEXT_S *, MAILSTREAM *, char *, \
				       char *, char *, STRING *));
long	    context_append_multiple PROTO((CONTEXT_S *, MAILSTREAM *, char *, \
					   append_t, void *, MAILSTREAM *));
long	    context_copy PROTO((CONTEXT_S *, MAILSTREAM *, char *, char *));
MAILSTREAM *context_same_stream PROTO((CONTEXT_S *, char *, MAILSTREAM *));
MAILSTREAM *context_already_open_stream PROTO((CONTEXT_S *, char *, int));

#endif /* _CONTEXT_INCLUDED */
