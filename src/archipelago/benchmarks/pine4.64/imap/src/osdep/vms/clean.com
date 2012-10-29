$! Program:	Portable c-client cleanup for VMS
$!
$! Author:	Mark Crispin
$!		Networks and Distributed Computing
$!		Computing & Communications
$!		University of Washington
$!		Administration Building, AG-44
$!		Seattle, WA  98195
$!		Internet: MRC@CAC.Washington.EDU
$!
$! Date:	14 June 1995
$! Last Edited:	6 February 2004
$!
$! The IMAP toolkit provided in this Distribution is
$! Copyright 2004 University of Washington.
$!
$! The full text of our legal notices is contained in the file called
$! CPYRIGHT, included with this Distribution.
$!
$ DELETE *.OBJ;*,OSDEP.*;*,TCP_VMS.C;*,MTEST.EXE;*,MAILUTIL.EXE;*
