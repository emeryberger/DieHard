$! $Id: vmsbuild.com 5467 1996-03-15 07:41:11Z hubert $
$!
$! Yehavi Bourvine
$! Hebrew University of Jeruselem
$! +972-2-585684
$! YEHAVI@vms.huji.ac.il
$!
$! Please address all bugs and comments to "pine-bugs@cac.washington.edu"
$!
$! Pine and Pico are registered trademarks of the University of Washington.
$! No commercial use of these trademarks may be made without prior written
$! permission of the University of Washington.
$!
$! Pine, Pico, and Pilot software and its included text are Copyright
$! 1989-1996 by the University of Washington.
$!
$! The full text of our legal notices is contained in the file called
$! CPYRIGHT, included with this distribution.
$!
$!
$ COPY/LOG OS_VMS.H OSDEP.H;
$ CC_DEF = "/define=(ANSI_DRIVER,ATTACHMENTS)"
$ IF F$LOCATE("VAX", F$GETSYI("HW_NAME")) .EQS. F$LENGTH(F$GETSYI("HW_NAME"))
$ THEN
$	CC_DEF = "''CC_DEF'/STANDARD=VAXC"
$ ELSE
$	DEFINE SYS SYS$LIBRARY	! For the Include to not fail on VAX.
$!
$ ENDIF
$ SET VERIFY
$ CC/NOOPTIMIZE'CC_DEF' attach
$ CC/NOOPTIMIZE'CC_DEF' ansi
$ CC/NOOPTIMIZE'CC_DEF' basic
$ CC/NOOPTIMIZE'CC_DEF' bind
$ CC/NOOPTIMIZE'CC_DEF' browse
$ CC/NOOPTIMIZE'CC_DEF' buffer
$ CC/NOOPTIMIZE'CC_DEF' composer 
$ CC/NOOPTIMIZE'CC_DEF' display
$ CC/NOOPTIMIZE'CC_DEF' file 
$ CC/NOOPTIMIZE'CC_DEF' fileio
$ CC/NOOPTIMIZE'CC_DEF' line
$ CC/NOOPTIMIZE'CC_DEF' pico
$ CC/NOOPTIMIZE'CC_DEF' random 
$ CC/NOOPTIMIZE'CC_DEF' region 
$ CC/NOOPTIMIZE'CC_DEF' search
$ CC/NOOPTIMIZE'CC_DEF' spell 
$ CC/NOOPTIMIZE'CC_DEF' window 
$ CC/NOOPTIMIZE'CC_DEF' word
$ CC/NOOPTIMIZE'CC_DEF' main
$ CC/NOOPTIMIZE'CC_DEF' os_VMS
$!
$ LIBRARY/OBJECT/CREATE/INSERT PICO *.OBJ
$!
$ IF F$EXTRACT(0, 3, F$GETSYI("HW_NAME")) .EQS. "VAX"
$ THEN
$	LINK/EXE=PICO MAIN,PICO/LIBR,SYS$INPUT:/OPT
	SYS$SHARE:VAXCRTL/SHARE
$ ELSE
$	LINK/EXE=PICO MAIN,PICO/LIBR
$ ENDIF
$ DELETE *.OBJ;*
$ SET NOVER
