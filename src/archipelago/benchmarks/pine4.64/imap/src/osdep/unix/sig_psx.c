/*
 * Program:	POSIX Signals
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	29 April 1997
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */
 
#include <signal.h>
#include <string.h>

#ifndef SA_RESTART
#define SA_RESTART 0
#endif

/* Arm a signal
 * Accepts: signal number
 *	    desired action
 * Returns: old action
 */

void *arm_signal (int sig,void *action)
{
  struct sigaction nact,oact;
  memset (&nact,0,sizeof (struct sigaction));
  sigemptyset (&nact.sa_mask);	/* no signals blocked */
  nact.sa_handler = action;	/* set signal handler */
  nact.sa_flags = SA_RESTART;	/* needed on Linux, nice on SVR4 */
  sigaction (sig,&nact,&oact);	/* do the signal action */
  return (void *) oact.sa_handler;
}
