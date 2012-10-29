! Program:	Portable C client build for TOPS-20
!
! Author:	Mark Crispin
!		Networks and Distributed Computing
!		Computing & Communications
!		University of Washington
!		Administration Building, AG-44
!		Seattle, WA  98195
!		Internet: MRC@CAC.Washington.EDU
!
! Date:		11 May 1989
! Last Edited:	15 October 2003
!
! The IMAP toolkit provided in this Distribution is
! Copyright 2003 University of Washington.
!
! The full text of our legal notices is contained in the file called
! CPYRIGHT, included with this Distribution.

!
! Build c-client library
!
@COPY OS_T20.* OSDEP.*
@CC -c -m -w=note MAIL.C DUMMYT20.C IMAP4R1.C SMTP.C NNTP.C POP3.C RFC822.C MISC.C OSDEP.C FLSTRING.C SMANAGER.C NEWSRC.C NETMSG.C UTF8.C
!
! Build c-client library file.  Should use MAKLIB, but MAKLIB barfs on MAIL.REL
!
@DELETE CCLINT.REL
@APPEND MAIL.REL,DUMMYT20.REL,IMAP4R1.REL,SMTP.REL,NNTP.REL,POP3.REL,RFC822.REL,MISC.REL,OSDEP.REL,FLSTRING.REL,SMANAGER.REL,NEWSRC.REL,NETMSG.REL,UTF8.REL CCLINT.REL
!
! Build MTEST library test program
!
@CC -c -m -w=note MTEST.C
@LINK
*/SET:.HIGH.:200000
*C:LIBCKX.REL
*MTEST.REL
*CCLINT.REL
*MTEST/SAVE
*/GO
!
! Build MAILUTIL
!
@CC -c -m -w=note MAILUTIL.C
@RENAME MAILUTIL.REL MUTIL.REL
@LINK
*/SET:.HIGH.:200000
*C:LIBCKX.REL
*MUTIL.REL
*CCLINT.REL
*MUTIL/SAVE
*/GO
@RESET
@RENAME MUTIL.EXE MAILUTIL.EXE
