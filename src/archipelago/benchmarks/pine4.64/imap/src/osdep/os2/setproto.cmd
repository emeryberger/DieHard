/* rexx */
/*
 * Program:	Set default prototype for OS/2
 *
 * Author:	Mark Crispin
 *		Networks and Distributed Computing
 *		Computing & Communications
 *		University of Washington
 *		Administration Building, AG-44
 *		Seattle, WA  98195
 *		Internet: MRC@CAC.Washington.EDU
 *
 * Date:	10 June 1999
 * Last Edited:	24 October 2000
 * 
 * The IMAP toolkit provided in this Distribution is
 * Copyright 2000 University of Washington.
 * The full text of our legal notices is contained in the file called
 * CPYRIGHT, included with this Distribution.
 */
'@echo off'
parse arg default args
h_file='linkage.h'
call stream h_file, 'C', 'open write'
call lineout h_file, '#define DEFAULTPROTO' default'proto'
call stream h_file, 'C', 'close'
exit 0
