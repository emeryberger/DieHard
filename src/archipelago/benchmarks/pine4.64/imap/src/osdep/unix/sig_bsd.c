/*
 * Program:	BSD Signals
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

/* Arm a signal
 * Accepts: signal number
 *	    desired action
 * Returns: old action
 */

void *arm_signal (int sig,void *action)
{
  return (void *) signal (sig,action);
}
