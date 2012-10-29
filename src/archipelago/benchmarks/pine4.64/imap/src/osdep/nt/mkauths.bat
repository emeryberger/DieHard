@ECHO OFF
REM Program:	Authenticator Linkage Generator for DOS
REM
REM Author:	Mark Crispin
REM		Networks and Distributed Computing
REM		Computing & Communications
REM		University of Washington
REM		Administration Building, AG-44
REM		Seattle, WA  98195
REM		Internet: MRC@CAC.Washington.EDU
REM
REM Date:	6 December 1995
REM Last Edited:24 October 2000
REM
REM The IMAP toolkit provided in this Distribution is
REM Copyright 2000 University of Washington.
REM
REM The full text of our legal notices is contained in the file called
REM CPYRIGHT, included with this Distribution.

REM Erase old authenticators list
IF EXIST AUTHS.C DEL AUTHS.C

REM Now define the new list
FOR %%D IN (%1 %2 %3 %4 %5 %6 %7 %8 %9) DO CALL MKAUTAUX %%D

EXIT 0
