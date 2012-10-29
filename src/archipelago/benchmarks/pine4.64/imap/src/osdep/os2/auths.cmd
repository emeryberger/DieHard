/* rexx */
/*
 * Program:	Authenticator Linkage Generator for OS/2
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
/* Erase old authenticators list */
'if exist auths.c del auths.c'
parse arg args
n=words(args)
a_file='auths.c'
c_file='linkage.c'
h_file='linkage.h'
call stream a_file, 'C', 'open write'
call stream c_file, 'C', 'open write'
call stream h_file, 'C', 'open write'
do i=1 to n
  arg=word(args,i)
  call lineout a_file, '#include "auth_'arg'.c"'
  call lineout h_file, 'extern AUTHENTICATOR auth_'arg';'
  call lineout c_file, '  auth_link (&auth_'arg');	/* link in the 'arg' authenticator */'
  end
call stream h_file, 'C', 'close'
call stream c_file, 'C', 'close'
call stream a_file, 'C', 'close'
exit 0
