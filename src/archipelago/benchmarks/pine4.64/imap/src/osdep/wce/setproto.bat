@ECHO OFF
REM Program:	Set default prototype for DOS/NT
REM
REM Author:	Mark Crispin
REM		Networks and Distributed Computing
REM		Computing & Communications
REM		University of Washington
REM		Administration Building, AG-44
REM		Seattle, WA  98195
REM		Internet: MRC@CAC.Washington.EDU
REM
REM Date:	 9 October 1995
REM Last Edited: 6 July 2004
REM
REM The IMAP toolkit provided in this Distribution is
REM Copyright 1988-2004 University of Washington.
REM
REM The full text of our legal notices is contained in the file called
REM CPYRIGHT, included with this Distribution.

REM Set the default drivers
ECHO #define CREATEPROTO %1proto >> LINKAGE.H
ECHO #define APPENDPROTO %2proto >> LINKAGE.H
