/*
 * Program:	UNIX Grim PID Reaper -- wait4() version
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	30 November 1993
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */
 
/* Grim PID reaper
 * Accepts: process ID
 *	    kill request flag
 *	    status return value
 */

void grim_pid_reap_status (int pid,int killreq,void *status)
{
  if (killreq) kill(pid,SIGHUP);/* kill if not already dead */
  while ((wait4 (pid,status,NIL,NIL) < 0) && (errno != ECHILD));
}
