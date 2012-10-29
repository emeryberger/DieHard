@ECHO OFF
REM Program:	Authenticator Linkage Generator auxillary for DOS
REM
REM Author:	Mark Crispin
REM		Networks and Distributed Computing
REM		Computing & Communications
REM		University of Washington
REM		Administration Building, AG-44
REM		Seattle, WA  98195
REM		Internet: MRC@CAC.Washington.EDU
REM
REM Date:	7 December 1995
REM Last Edited:24 October 2000
REM
REM The IMAP toolkit provided in this Distribution is
REM Copyright 2000 University of Washington.
REM
REM The full text of our legal notices is contained in the file called
REM CPYRIGHT, included with this Distribution.

ECHO extern AUTHENTICATOR auth_%1; >> LINKAGE.H
ECHO   auth_link (&auth_%1);		/* link in the %1 authenticator */ >> LINKAGE.C
ECHO #include "auth_%1.c" >> AUTHS.C
