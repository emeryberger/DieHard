$! Program:	Portable c-client build for VMS
$!
$! Author:	Mark Crispin
$!		Networks and Distributed Computing
$!		Computing & Communications
$!		University of Washington
$!		Administration Building, AG-44
$!		Seattle, WA  98195
$!		Internet: MRC@CAC.Washington.EDU
$!
$! Date:	2 August 1994
$! Last Edited:	6 February 2004
$!
$! The IMAP toolkit provided in this Distribution is
$! Copyright 2004 University of Washington.
$!
$! The full text of our legal notices is contained in the file called
$! CPYRIGHT, included with this Distribution.
$!
$!
$!  Change this to your local timezone.  This value is the number of minutes
$! east of UTC (formerly known as GMT).  Sample values: -300 (US east coast),
$! -480 (US west coast), 540 (Japan), 60 (western Europe).
$!  VAX C's HELP information says that you should be able to use gmtime(), but
$! it returns 0 for the struct.  ftime(), you ask?  It, too, returns 0 for a
$! timezone.  Nothing sucks like a VAX!
$!
$ CC_TIMEZONE=-480
$!
$! CC options
$!
$ CC_PREF = "/OPTIMIZE/INCLUDE=[]"
$ CC_PREF = CC_PREF + "/DEFINE=net_getbuffer=NET_GETBUF"
$ CC_PREF = CC_PREF + "/DEFINE=LOCALTIMEZONE='CC_TIMEZONE'"
$!
$! Determine TCP type
$!
$ TCP_TYPE = "VMSN"		! default to none
$ IF F$LOCATE("MULTINET", P1) .LT. F$LENGTH(P1)
$ THEN
$	DEFINE SYS MULTINET_ROOT:[MULTINET.INCLUDE.SYS],sys$library
$	DEFINE NETINET MULTINET_ROOT:[MULTINET.INCLUDE.NETINET]
$	DEFINE ARPA MULTINET_ROOT:[MULTINET.INCLUDE.ARPA]
$	TCP_TYPE = "VMSM"	! Multinet
$	LINK_OPT = ",LINK_MNT/OPTION"
$ ENDIF
$ IF F$LOCATE("NETLIB", P1) .LT. F$LENGTH(P1)
$ THEN
$	DEFINE SYS SYS$LIBRARY:	! normal .H location
$	DEFINE NETINET SYS$LIBRARY:
$	DEFINE ARPA SYS$LIBRARY:
$	LINK_OPT = ",LINK_NLB/OPTION"
$	TCP_TYPE = "VMSL"	! NETLIB
$ ENDIF
$ IF TCP_TYPE .EQS. "VMSN"
$ THEN
$	DEFINE SYS SYS$LIBRARY:	! normal .H location
$	DEFINE NETINET SYS$LIBRARY:
$	DEFINE ARPA SYS$LIBRARY:
$	LINK_OPT = ""
$ ENDIF
$!
$ COPY TCP_'TCP_TYPE'.C TCP_VMS.C;
$!
$ COPY OS_VMS.H OSDEP.H;
$ SET VERIFY
$ CC'CC_PREF' MAIL
$ CC'CC_PREF' IMAP4R1
$ CC'CC_PREF' SMTP
$ CC'CC_PREF' NNTP
$ CC'CC_PREF' POP3
$ CC'CC_PREF' DUMMYVMS
$ CC'CC_PREF' RFC822
$ CC'CC_PREF' MISC
$ CC'CC_PREF' OS_VMS
$ CC'CC_PREF' SMANAGER
$ CC'CC_PREF' FLSTRING
$ CC'CC_PREF' NEWSRC
$ CC'CC_PREF' NETMSG
$ CC'CC_PREF' UTF8
$ CC'CC_PREF' MTEST
$ CC'CC_PREF' MAILUTIL
$!
$ LINK MTEST,OS_VMS,MAIL,IMAP4R1,SMTP,NNTP,POP3,DUMMYVMS,RFC822,MISC,UTF8,-
	SMANAGER,FLSTRING,NEWSRC,NETMSG,SYS$INPUT:/OPTION'LINK_OPT',LINK/OPTION
PSECT=_CTYPE_,NOWRT
$ LINK MAILUTIL,OS_VMS,MAIL,IMAP4R1,SMTP,NNTP,POP3,DUMMYVMS,RFC822,MISC,UTF8,-
	SMANAGER,FLSTRING,NEWSRC,NETMSG,SYS$INPUT:/OPTION'LINK_OPT',LINK/OPTION
PSECT=_CTYPE_,NOWRT
$ SET NOVERIFY
$ EXIT
