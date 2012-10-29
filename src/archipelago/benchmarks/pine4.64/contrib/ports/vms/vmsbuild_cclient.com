$! Program:	Operating-system dependent routines -- VMS version
$!
$! Author:	Yehavi Bourvine, The Hebrew University of Jerusalem.
$!		Internet: Yehavi@VMS.huji.ac.il
$!
$! Date:	2 August 1994
$! Last Edited:	2 August 1994
$!
$! Copyright 1994 by the University of Washington
$!
$!  Permission to use, copy, modify, and distribute this software and its
$! documentation for any purpose and without fee is hereby granted, provided
$! that the above copyright notice appears in all copies and that both the
$! above copyright notice and this permission notice appear in supporting
$! documentation, and that the name of the University of Washington not be
$! used in advertising or publicity pertaining to distribution of the software
$! without specific, written prior permission.	This software is made available
$! "as is", and
$! THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
$! WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
$! WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
$! NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
$! INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
$! LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
$! (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
$! WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
$!
$! VMSBUILD.COM for C-CLIENT.
$ CREATE LINKAGE.H
$ DECK
extern DRIVER imapdriver, nntpdriver, vmsmaildriver;
$ EOD
$ CREATE LINKAGE.C
$ DECK
    mail_link((DRIVER *)&imapdriver);
    mail_link((DRIVER *)&nntpdriver);
    mail_link((DRIVER *)&vmsmaildriver);
$ EOD
$!
$ DEFINE SYS SYS$LIBRARY:		! Normal .H location.
$ DEFINE NETINET SYS$LIBRARY:
$ DEFINE ARPA SYS$LIBRARY:
$!
$ COPY OS_VMS.H OSDEP.H;
$ COPY TCP_VMSN.C TCP_VMS.C;	! Default - no TcpIp support.
$!
$ CC_DEF = ",''P1'"
$ LINK_OPT = ""
$ IF P1 .EQS. "" THEN CC_DEF=""
$ IF F$LOCATE("MULTINET", P1) .LT. F$LENGTH(P1)
$ THEN
$	DEFINE SYS MULTINET_ROOT:[MULTINET.INCLUDE.SYS],sys$library
$	DEFINE NETINET MULTINET_ROOT:[MULTINET.INCLUDE.NETINET]
$	DEFINE ARPA MULTINET_ROOT:[MULTINET.INCLUDE.ARPA]
$	COPY TCP_VMSM.C TCP_VMS.C;	! Multinet support.
$	LINK_OPT = ",[-.CONTRIB.VMS]VMS_MULTINET_LINK/OPTION"
$ ENDIF
$ IF F$LOCATE("NETLIB", P1) .LT. F$LENGTH(P1)
$ THEN
$	LINK_OPT = ",[-.CONTRIB.VMS]VMS_NETLIB_LINK/OPTION"
$	COPY TCP_VMSL.C TCP_VMS.C;	! Netlib support.
$ ENDIF
$!
$ CC_PREF = ""
$ IF F$LOCATE("VAX", F$GETSYI("HW_NAME")) .EQS. F$LENGTH(F$GETSYI("HW_NAME"))
$ THEN
$	CC_PREF = "/PREFIX=(ALL,EXCEPT=(SOCKET,CONNECT,BIND,LISTEN,SOCKET_READ,SOCKET_WRITE,SOCKET_CLOSE,SELECT,ACCEPT,BCMP,BCOPY,BZERO,GETHOSTBYNAME,"
$	CC_PREF = CC_PREF + "GETHOSTBYADDR,GETPEERNAME,GETDTABLESIZE,HTONS,HTONL,NTOHS,NTOHL,SEND,SENDTO,RECV,RECVFROM))"
$	CC_PREF = CC_PREF + "/STANDARD=VAXC"
$ ELSE
$	CC_PREF = "/INCLUDE=[]"
$	LINK_OPT = LINK_OPT + ",[-.CONTRIB.VMS]VMS_LINK/OPTION"
$	COPY SYS$LIBRARY:CTYPE.H *.*;
$	EDIT/EDT CTYPE.H
s/readonly// w
exit
$ ENDIF
$ SET VERIFY
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') OS_VMS
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') vms_mail
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') MAIL
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') SMTP
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') RFC822
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') NNTP
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') nntpcvms
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') MISC
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') IMAP2
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def', -
  L_SET=0) SM_VMS
$!
$ CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') MTEST
$! CC/NOOPTIMIZE'CC_PREF'/define=("readonly"="ReadOnly"'cc_def') IMAPD
$!
$ LIBRARY/OBJECT/CREATE/INSERT C-CLIENT OS_VMS,vms_mail,MAIL,SMTP,RFC822,-
	NNTP,nntpcvms,MISC,IMAP2,SM_VMS
$!
$ SET NOVERIFY
$ LINK MTEST,IMAP2,MAIL,MISC,NNTP,nntpcvms,OS_VMS,RFC822,SMTP,-
  SM_VMS,VMS_MAIL,SYS$INPUT:/OPTION'LINK_OPT'
PSECT=_CTYPE_,NOWRT
$! LINK IMAPD,imapd_vms,IMAP2,MAIL,MISC,NNTP,nntpcvms,OS_VMS,RFC822,SMTP,-
$!   SM_VMS,VMS_MAIL'LINK_OPT'
$ EXIT
