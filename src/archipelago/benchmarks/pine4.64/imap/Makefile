# Program:	IMAP Toolkit Makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		7 December 1989
# Last Edited:	30 April 2005
#
# The IMAP toolkit provided in this Distribution is
# Copyright 1988-2005 University of Washington.
#
# The full text of our legal notices is contained in the file called
# CPYRIGHT, included with this Distribution.


# Normal command to build IMAP toolkit:
#  make <port> [EXTRAAUTHENTICATORS=xxx] [EXTRADRIVERS=xxx] [EXTRACFLAGS=xxx]
#	       [PASSWDTYPE=xxx] [SSLTYPE=xxx] [IP=n]


# Port name.  These refer to the *standard* compiler on the given system.
# This means, for example, that the hpx port is for HP's compiler and not for
# a non-standard compiler such as gcc.
#
# If you are using gcc and it is not the standard compiler on your system, try
# using an ANSI port that is close to what you have.  For example, if your
# system is SVR4ish, try a32 or lnx; if it's more BSDish, try nxt, mct, or bsi.
#
# The following ports are bundled:
# a32	AIX 3.2 for RS/6000
# a41	AIX 4.1 for RS/6000
# aix	AIX/370 (not RS/6000!!)
# ami	AmigaDOS
# am2	AmigaDOS with a 68020+
# ama	AmigaDOS using AS225R2
# amn	AmigaDOS with a 680x0 using "new" socket library
# aos	AOS for RT
# art	AIX 2.2.1 for RT
# asv	Altos SVR4
# aux	A/UX
# bs3	BSD/i386 3.0 and higher
# bsd	generic BSD 4.3 (as in ancient 1980s version)
# bsf	FreeBSD
# bsi	BSD/i386
# bso	OpenBSD (yes, yet another one...)
# cvx	Convex
# cyg	Cygwin
# d-g	Data General DG/UX prior to 5.4 (d41 port no longer exists)
# d54	Data General DG/UX 5.4
# do4	Apollo Domain/OS sr10.4
# dpx	Bull DPX/2 B.O.S.
# drs	ICL DRS/NX
# dyn	Dynix
# epx	EP/IX
# ga4	GCC AIX 4.x for RS/6000
# gas	GCC Altos SVR4
# gcs	GCC Solaris with Blastwave Community Open Source Software
# gh9   GCC HP-UX 9.x
# ghp	GCC HP-UX 10.x
# ghs	GCC HP-UX 10.x with Trusted Computer Base
# go5	GCC 2.7.1 (95q4 from Skunkware _not_ 98q2!) SCO Open Server 5.0.x
# gsc	GCC Santa Cruz Operation
# gsg	GCC SGI
# gso	GCC Solaris
# gsu	GCC SUN-OS
# gul	GCC RISC Ultrix (DEC-5000)
# hpp	HP-UX 9.x (see gh9)
# hpx	HP-UX 10.x (see ghp, ghs, hxd, and shp)
# hxd	HP-UX 10.x with DCE security (see shp)
# isc	Interactive Systems
# ldb	Debian Linux
# lnx	Linux with traditional passwords and crypt() in the C library
#	 (see lnp, sl4, sl5, and slx)
# lnp	Linux with Pluggable Authentication Modules (PAM)
# lmd	Mandrake Linux
# lrh	RedHat Linux 7.2 and later
# lsu	SuSE Linux
# lyn	LynxOS
# mct	MachTen
# mnt	Atari ST Mint (not MacMint)
# neb	NetBSD/FreeBSD
# nec	NEC UX
# nto	QNX Neutrine RTP
# nxt	NEXTSTEP
# nx3	NEXTSTEP 3.x
# osf	OSF/1 (see sos, os4)
# os4	OSF/1 (Digital UNIX) 4
# osx	Mac OS X
# oxp	Mac OS X with Pluggable Authentication Modules (PAM)
# ptx	PTX
# pyr	Pyramid
# qnx	QNX 4
# s40	SUN-OS 4.0 (*not* Solaris)
# sc5	SCO Open Server 5.0.x (see go5)
# sco	Santa Cruz Operation (see sc5, go5)
# shp	HP-UX with Trusted Computer Base
# sgi	Silicon Graphics IRIX
# sg6	Silicon Graphics IRIX 6.5
# sl4	Linux using -lshadow to get the crypt() function
# sl5	Linux with shadow passwords, no extra libraries
# slx	Linux using -lcrypt to get the crypt() function
# snx	Siemens Nixdorf SININX or Reliant UNIX
# soc	Solaris with /opt/SUNWspro/bin/cc
# sol	Solaris (won't work unless "ucbcc" works -- use gso instead)
# sos	OSF/1 with SecureWare
# ssn	SUN-OS with shadow password security
# sun	SUN-OS 4.1 or better (*not* Solaris) (see ssn)
# sv2	SVR2 on AT&T PC-7300 (incomplete port)
# sv4	generic SVR4
# ult	RISC Ultrix (DEC-5000)
# uw2	UnixWare SVR4.2
# vul	VAX Ultrix
# vu2	VAX Ultrix 2.3 (e.g. for VAXstation-2000 or similar old version)


# Extra authenticators (e.g. OTP, Kerberos, etc.).  Adds linkage for
# auth_xxx.c and executes Makefile.xxx, where xxx is the name of the
# authenticator.  Some authenticators are only available from third parties.
#
# The following extra authenticators are bundled:
# gss	Kerberos V

EXTRAAUTHENTICATORS=


# Additional mailbox drivers.  Add linkage for xxxdriver.  Some drivers are
# only available from third parties.
#
# The following extra drivers are bundled:
# mbox	if file "mbox" exists on the home directory, automatically moves mail
#	 from the spool directory to "mbox" and uses "mbox" as INBOX.

EXTRADRIVERS=mbox


# Plaintext password type.  Defines how plaintext password authentication is
# done on this system.
#
# The following plaintext login types are bundled:
# afs	AFS authentication database
# dce	DCE authentication database
# gss	Kerberos V
# nul	plaintext authentication never permitted
# pam	PAM authentication (note: for Linux, you should use the "lnp" port
#	 instead of setting this...also, you may have to modify PAMLDFLAGS
#	 in the imap-[]/src/osdep/unix/Makefile
# pmb	PAM authentication for broken implementations such as Solaris.
#	 you may have to modify PAMLDFLAGS
# std	system standard (typically passwd file), determined by port
# two	try alternative (defined by CHECKPWALT), then std

PASSWDTYPE=std


# SSL type.  Defines whether or not SSL support is on this system
#
# The following SSL types are bundled:
# none	no SSL support
# unix	SSL support using OpenSSL
# nopwd	SSL support using OpenSSL, and plaintext authentication permitted only
#	in SSL/TLS sessions
# sco	link SSL before other libraries (for SCO systems)
# unix.nopwd	same as nopwd
# sco.nopwd	same as nopwd, plaintext authentication in SSL/TLS only
#
# SSLTYPE=nopwd is now the default as required by RFC 3501

SSLTYPE=nopwd


# IP protocol version
#
# The following IP protocol versions are defined:
# o	IPv4 support, no DNS (truly ancient systems)
# 4	(default) IPv4 support only
# 6	IPv6 and IPv4 support

IP=4


# The following extra compilation flags are defined.  None of these flags are
# recommended.  If you use these, include them in the EXTRACFLAGS.
#
# -DDISABLE_POP_PROXY
#	By default, the ipop[23]d servers offer POP->IMAP proxy access,
#	which allow a POP client to access mail on an IMAP server by using the
#	POP server as a go-between.  Setting this option disables this
#	facility.
#
# -DOLDFILESUFFIX=\"xxx\"
#	Change the default suffix appended to the backup .newsrc file from
#	"old".
#
# -DSTRICT_RFC822_TIMEZONES
#	Disable recognition of the non-standard UTC (0000), MET (+0100),
#	EET (+0200), JST (+0900), ADT (-0300), AST (-0400), YDT (-0800),
#	YST (-0900), and HST (-1000) symbolic timezones.
#
# -DBRITISH_SUMMER_TIME
#	Enables recognition of non-standard symbolic timezone BST as +0100.
#
# -DBERING_STANDARD_TIME
#	Enables recognition of non-standard symbolic timezone BST as -1100.
#
# -DNEWFOUNDLAND_STANDARD_TIME
#	Enables recognition of non-standard symbolic timezone NST as -0330.
#
# -DNOME_STANDARD_TIME
#	Enables recognition of non-standard symbolic timezone NST as -1100.
#
# -DSAMOA_STANDARD_TIME
#	Enables recognition of non-standard symbolic timezone SST as -1100.
#				
# -DY4KBUGFIX
#	Turn on the Y4K bugfix (yes, that's year 4000).  It isn't well-known,
#	but century years evenly divisible by 4000 are *not* leap years in the
#	Gregorian calendar.  A lot of "Y2K compilant" software does not know
#	about this rule.  Remember to turn this on sometime in the next 2000
#	years.
#
# -DUSEORTHODOXCALENDAR
#	Use the more accurate Eastern Orthodox calendar instead of the
#	Gregorian calendar.  The century years which are leap years happen
#	at alternating 400 and 500 year intervals without shifts every 4000
#	years.  The Orthodox and Gregorian calendars diverge by 1 day for
#	gradually-increasing intervals, starting at 2800-2900, and becoming
#	permanent at 48,300.
#
# -DUSEJULIANCALENDAR
#	Use the less accurate Julian calendar instead of the Gregorian
#	calendar.  Leap years are every 4 years, including century years.
#	My apologies to those in the English-speaking world who object to
#	the reform of September 2, 1752 -> September 14, 1752, since this
#	code still uses January 1 (which Julius Ceasar decreed as the start
#	of the year, which since 153 BCE was the day that Roman consuls
#	took office), rather than the traditional March 25 used by the
#	British.  As of 2005, the Julian calendar and the Gregorian calendar
#	diverge by 15 days.

EXTRACFLAGS=


# Extra linker flags (additional/alternative libraries, etc.)

EXTRALDFLAGS=


# Special make flags (e.g. to override make environment variables)

EXTRASPECIALS=
SPECIALS=


# Normal commands

CAT=cat
CD=cd
LN=ln -s
MAKE=make
MKDIR=mkdir
BUILDTYPE=rebuild
RM=rm -rf
SH=sh
SYSTEM=unix
TOOLS=tools
TOUCH=touch


# Primary build command

BUILD=$(MAKE) build EXTRACFLAGS='$(EXTRACFLAGS)'\
 EXTRALDFLAGS='$(EXTRALDFLAGS)'\
 EXTRADRIVERS='$(EXTRADRIVERS)'\
 EXTRAAUTHENTICATORS='$(EXTRAAUTHENTICATORS)'\
 PASSWDTYPE=$(PASSWDTYPE) SSLTYPE=$(SSLTYPE) IP=$(IP)\
 EXTRASPECIALS='$(EXTRASPECIALS)'


# Make the IMAP Toolkit

all:	SPECIALS c-client rebuild bundled

c-client:
	@echo Not processed yet.  In a first-time build, you must specify
	@echo the system type so that the sources are properly processed.
	@false


SPECIALS:
	echo $(SPECIALS) > SPECIALS

# Note on SCO you may have to set LN to "ln".

a32 a41 aix bs3 bsi d-g d54 do4 drs epx ga4 gas gh9 ghp ghs go5 gsc gsg gso gul hpp hpx lnp lyn mct mnt neb nec nto nxt nx3 osf os4 ptx qnx sc5 sco sgi sg6 shp sl4 sl5 slx snx soc sol sos uw2: an
	$(BUILD) BUILDTYPE=$@

# If you use sv4, you may find that it works to move it to use the an process.
# If so, you probably will want to delete the "-Dconst=" from the sv4 CFLAGS in
# the c-client Makefile.

aos art asv aux bsd cvx dpx dyn isc pyr sv4 ult vul vu2: ua
	$(BUILD) BUILDTYPE=$@

# Knotheads moved Kerberos and SSL locations on these platforms


bsf bso:	an
	$(BUILD) BUILDTYPE=$@ \
	SPECIALS="GSSDIR=/usr SSLDIR=/usr SSLINCLUDE=/usr/include/openssl SSLCERTS=/etc/ssl/certs SSLKEYS=/etc/ssl/private LOCKPGM=/usr/sbin/mlock"

cyg:	an
	$(BUILD) BUILDTYPE=cyg \
	SPECIALS="SSLDIR=/usr/ssl SSLINCLUDE=/usr/include/openssl SSLLIB=/usr/lib"

gcs:	an
	$(BUILD) BUILDTYPE=gso \
	SPECIALS="SSLDIR=/opt/csw/ssl SSLINCLUDE=/opt/csw/include/openssl SSLLIB=/opt/csw/lib"

ldb:	an
	$(BUILD) BUILDTYPE=lnp \
	SPECIALS="GSSDIR=/usr SSLDIR=/usr SSLINCLUDE=/usr/include/openssl SSLCERTS=/etc/ssl/certs SSLKEYS=/etc/ssl/private LOCKPGM=/usr/sbin/mlock"

lmd:	an
	$(BUILD) BUILDTYPE=lnp \
	SPECIALS="SSLDIR=/usr/lib/ssl SSLINCLUDE=/usr/include/openssl SSLLIB=/usr/lib LOCKPGM=/usr/sbin/mlock"

lrh:	an
	$(BUILD) BUILDTYPE=lnp \
	SPECIALS="GSSDIR=/usr/kerberos SSLDIR=/usr/share/ssl SSLINCLUDE=/usr/include/openssl SSLLIB=/usr/lib LOCKPGM=/usr/sbin/mlock" \
	EXTRACFLAGS="$(EXTRACFLAGS) -I/usr/kerberos/include"

lsu:	an
	$(BUILD) BUILDTYPE=lnp \
	SPECIALS="SSLDIR=/usr/share/ssl SSLINCLUDE=/usr/include/openssl SSLLIB=/usr/lib"

osx:	an
	$(BUILD) BUILDTYPE=osx \
	IP=6 EXTRAAUTHENTICATORS="$(EXTRAAUTHENTICATORS) gss" \
	SPECIALS="SSLDIR=/System/Library/OpenSSL SSLINCLUDE=/usr/include/openssl SSLLIB=/usr/lib"

oxp:	an
	$(BUILD) BUILDTYPE=osx \
	IP=6 EXTRAAUTHENTICATORS="$(EXTRAAUTHENTICATORS) gss" \
	SPECIALS="SSLDIR=/System/Library/OpenSSL SSLINCLUDE=/usr/include/openssl SSLLIB=/usr/lib PAMLDFLAGS=-lpam" \
	PASSWDTYPE=pam \
	EXTRACFLAGS="$(EXTRACFLAGS) -DMAC_OSX_KLUDGE=1"


# Linux shadow password support doesn't build on traditional systems, but most
# Linux systems are shadow these days.

lnx:	lnxnul an
	$(BUILD) BUILDTYPE=$@

lnxnul:
	@$(SH) -c '(test $(PASSWDTYPE) = nul) || make lnxok'

lnxok:
	@echo You are building for traditional Linux.  Most modern Linux
	@echo systems require that you build using make slx.  Do you want
	@echo to continue this build?  Type y or n please:
	@$(SH) -c 'read x; case "$$x" in y) exit 0;; *) exit 1;; esac'
	@echo OK, I will remember that you really want to build for
	@echo traditional Linux.  You will not see this message again.
	@echo If you discover that you can not log in to the POP and IMAP
	@echo servers, then do the following commands:
	@echo % rm lnxok
	@echo % make clean
	@echo % make slx
	@echo If slx does not work, try sl4 or sl5.  Be sure to do a
	@echo make clean between each try!
	@$(TOUCH) lnxok


# SUN-OS C compiler makes you load libdl by hand...

ssn sun: sunok suntools ua
	$(BUILD) BUILDTYPE=$@

suntools:
	$(CD) tools;$(MAKE) LDFLAGS=-ldl

gsu:	sunok an
	$(BUILD) BUILDTYPE=$@

s40:	sunok ua
	$(BUILD) BUILDTYPE=$@

sunok:
	@echo You are building for the old BSD-based SUN-OS.  This is NOT
	@echo the modern SVR4-based Solaris.  If you want to build for
	@echo Solaris, you should use make gso or make sol or make soc.  Do
	@echo you want to continue this build?  Type y or n please:
	@$(SH) -c 'read x; case "$$x" in y) exit 0;; *) exit 1;; esac'
	@echo OK, I will remember that you really want to build for the old
	@echo BSD-based SUN-OS.  You will not see this message again.
	@echo If the build fails and you realize that you really wanted to
	@echo build for Solaris, then do the following commands:
	@echo % rm sunok
	@echo % make clean
	@echo % make gso
	@echo If gso does not work, try sol.  Be sure to do a make clean
	@echo between each try!
	@$(TOUCH) sunok


# SVR2 doesn't have symbolic links (at least my SVR2 system doesn't)

sv2:
	$(MAKE) ua LN=ln
	$(BUILD) BUILDTYPE=$@ LN=ln


# Pine port names, not distinguished in c-client

bs2:	an
	$(BUILD) BUILDTYPE=bsi

pt1:	an
	$(BUILD) BUILDTYPE=ptx


# Compatibility

hxd:
	$(BUILD) BUILDTYPE=hpx PASSWDTYPE=dce

# Amiga

ami am2 ama amn:
	$(MAKE) an LN=cp SYSTEM=amiga
	$(BUILD) BUILDTYPE=$@ LN=cp


# Courtesy entries for Microsoft systems

nt:
	nmake /nologo /f makefile.nt

ntk:
	nmake /nologo /f makefile.ntk

w2k:
	nmake /nologo /f makefile.w2k

wce:
	nmake /nologo /f makefile.wce


# SSL build choices

sslnopwd sslunix.nopwd sslsco.nopwd:
	@echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	@echo + Building in full compliance with RFC 3501 security
	@echo + requirements:
	@echo ++ TLS/SSL encryption is supported
	@echo ++ Unencrypted plaintext passwords are prohibited
	@echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

sslunix sslsco:
	@echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	@echo + Building in PARTIAL compliance with RFC 3501 security
	@echo + requirements:
	@echo + Compliant:
	@echo ++ TLS/SSL encryption is supported
	@echo + Non-compliant:
	@echo ++ Unencrypted plaintext passwords are permitted
	@echo +
	@echo + In order to rectify this problem, you MUST build with:
	@echo ++ SSLTYPE=$(SSLTYPE).nopwd
	@echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	@echo
	@echo Do you want to continue this build anyway?  Type y or n please:
	@$(SH) -c 'read x; case "$$x" in y) exit 0;; *) exit 1;; esac'

sslnone:
	@echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	@echo + Building in NON-COMPLIANCE with RFC 3501 security requirements:
	@echo + Non-compliant:
	@echo ++ TLS/SSL encryption is NOT supported
	@echo ++ Unencrypted plaintext passwords are permitted
	@echo +
	@echo + In order to rectify this problem, you MUST build with:
	@echo ++ SSLTYPE=nopwd
	@echo + You must also have OpenSSL or equivalent installed.
	@echo +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	@echo
	@echo Do you want to continue this build anyway?  Type y or n please:
	@$(SH) -c 'read x; case "$$x" in y) exit 0;; *) exit 1;; esac'


# C compiler types

an ua:
	$(MAKE) ssl$(SSLTYPE)
	@echo Applying $@ process to sources...
	$(TOOLS)/$@ "$(LN)" src/c-client c-client
	$(TOOLS)/$@ "$(LN)" src/ansilib c-client
	$(TOOLS)/$@ "$(LN)" src/charset c-client
	$(TOOLS)/$@ "$(LN)" src/osdep/$(SYSTEM) c-client
	$(TOOLS)/$@ "$(LN)" src/mtest mtest
	$(TOOLS)/$@ "$(LN)" src/ipopd ipopd
	$(TOOLS)/$@ "$(LN)" src/imapd imapd
	$(TOOLS)/$@ "$(LN)" src/mailutil mailutil
	$(TOOLS)/$@ "$(LN)" src/mlock mlock
	$(TOOLS)/$@ "$(LN)" src/dmail dmail
	$(TOOLS)/$@ "$(LN)" src/tmail tmail
	$(LN) $(TOOLS)/$@ .

build:	OSTYPE rebuild rebuildclean bundled

OSTYPE:
	@echo Building c-client for $(BUILDTYPE)...
	@$(TOUCH) SPECIALS
	echo `$(CAT) SPECIALS` $(EXTRASPECIALS) > c-client/SPECIALS
	$(CD) c-client;$(MAKE) $(BUILDTYPE) EXTRACFLAGS='$(EXTRACFLAGS)'\
	 EXTRALDFLAGS='$(EXTRALDFLAGS)'\
	 EXTRADRIVERS='$(EXTRADRIVERS)'\
	 EXTRAAUTHENTICATORS='$(EXTRAAUTHENTICATORS)'\
	 PASSWDTYPE=$(PASSWDTYPE) SSLTYPE=$(SSLTYPE) IP=$(IP)\
	 $(SPECIALS) $(EXTRASPECIALS)
	echo $(BUILDTYPE) > OSTYPE
	$(TOUCH) rebuild

rebuild:
	@$(SH) -c '(test $(BUILDTYPE) = rebuild -o $(BUILDTYPE) = `$(CAT) OSTYPE`) || (echo Already built for `$(CAT) OSTYPE` -- you must do \"make clean\" first && exit 1)'
	@echo Rebuilding c-client for `$(CAT) OSTYPE`...
	@$(TOUCH) SPECIALS
	$(CD) c-client;$(MAKE) all CC=`$(CAT) CCTYPE` \
	 CFLAGS="`$(CAT) CFLAGS`" `$(CAT) SPECIALS`

rebuildclean:
	$(SH) -c '$(RM) rebuild || true'

bundled:
	@echo Building bundled tools...
	$(CD) mtest;$(MAKE)
	$(CD) ipopd;$(MAKE)
	$(CD) imapd;$(MAKE)
	$(CD) mailutil;$(MAKE)
	@$(SH) -c '(test -f /usr/include/sysexits.h ) || make sysexitwarn'
	$(CD) mlock;$(MAKE) || true
	$(CD) dmail;$(MAKE) || true
	$(CD) tmail;$(MAKE) || true


sysexitwarn:
	@echo Hmm...it does not look like /usr/include/sysexits.h exists.
	@echo Either your system is too ancient to have the sysexits.h
	@echo include, or your C compiler gets it from some other location
	@echo than /usr/include.  If your system is too old to have the
	@echo sysexits.h include, you will not be able to build the
	@echo following programs.

clean:
	@echo Removing old processed sources and binaries...
	$(SH) -c '$(RM) an ua OSTYPE SPECIALS c-client mtest imapd ipopd mailutil mlock dmail tmail || true'
	$(CD) tools;$(MAKE) clean


# A monument to a hack of long ago and far away...
love:
	@echo not war?
