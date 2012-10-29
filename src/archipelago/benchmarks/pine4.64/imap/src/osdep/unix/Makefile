# Program:	C client makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		11 May 1989
# Last Edited:	30 April 2005
#
# The IMAP toolkit provided in this Distribution is
# Copyright 1988-2005 University of Washington.
#
# The full text of our legal notices is contained in the file called
# CPYRIGHT, included with this Distribution.


# Command line build parameters

EXTRAAUTHENTICATORS=
EXTRADRIVERS=mbox
PASSWDTYPE=std
SSLTYPE=nopwd
IP=4


# The optimization level here for GCC ports is set here for a reason.  It's
# to get you to read this text.
# The general concensus seems to be that -O2 is the one to use.
# Over the years, I've been told to use many different settings, including -O6.
# In recent versions of GCC [as of 2/2005], -O6 generates bad code that, among
# other ill effects, causes infinite loops.
# -O3 seems to be safe, but empirical observation from our local expert
# indicates that in some (many?) cases -O3 code runs slower than -O2.

GCCOPTLEVEL= -O2


# Extended flags needed for SSL.  You may need to modify.

SSLDIR=/usr/local/ssl
SSLCERTS=$(SSLDIR)/certs
SSLKEYS=$(SSLCERTS)
SSLINCLUDE=$(SSLDIR)/include
SSLLIB=$(SSLDIR)/lib

SSLCRYPTO=-lcrypto

# Older versions of MIT Kerberos also have a libcrypto.  If so, you may need
# to use this instead
#SSLCRYPTO=$(SSLLIB)/libcrypto.a

# RSA Security Inc. released the RSA public key encryption algorithm into
# the public domain on September 6, 2000.  There is no longer any need to
# use RSAREF.
SSLRSA= # -lRSAglue -lrsaref

SSLCFLAGS= -I$(SSLINCLUDE) -I$(SSLINCLUDE)/openssl\
 -DSSL_CERT_DIRECTORY=\"$(SSLCERTS)\" -DSSL_KEY_DIRECTORY=\"$(SSLKEYS)\"
SSLLDFLAGS= -L$(SSLLIB) -lssl $(SSLCRYPTO) $(SSLRSA)


# Extended flags needed for non-standard passwd types.  You may need to modify.

AFSDIR=/usr/afsws
AFSCFLAGS=-I$(AFSDIR)/include
AFSLIB=$(AFSDIR)/lib
AFSLDFLAGS=-L$(AFSLIB)/afs -L$(AFSLIB) -L$(AFSDIR)/domestic/lib\
 -lkauth -lprot -lubik -lauth -lrxkad -lrx -llwp -ldes -lcom_err\
 $(AFSLIB)/afs/util.a -laudit -lsys
# AFSLDFLAGS may also need -L/usr/ucblib -lucb
DCECFLAGS= -DDCE_MINIMAL -DPASSWD_OVERRIDE=\"/opt/pop3/passwd/passwd\"
DCELDFLAGS= -ldce
PAMLDFLAGS= -lpam -ldl


# Build parameters normally set by the individual port

CHECKPW=std
LOGINPW=std
SIGTYPE=bsd
CRXTYPE=std
ACTIVEFILE=/usr/lib/news/active
SPOOLDIR=/usr/spool
MAILSPOOL=$(SPOOLDIR)/mail
NEWSSPOOL=$(SPOOLDIR)/news
RSHPATH=/usr/ucb/rsh
LOCKPGM=/etc/mlock


# Default formats for creating new mailboxes and for empty mailboxes in the
# default namespace; must be set to the associated driver's prototype.
#
# The CREATEPROTO is the default format for new mailbox creation.
# The EMPTYPROTO is the default format for handling zero-byte files.
#
# Normally, this is set by the individual port.
#
# NOTE: namespace formats (e.g. mh and news) can not be set as a default format
# since they do not exist in the default namespace.  Also, it is meaningless to
# set certain other formats (e.g. mbx and mx) as the EMPTYPROTO since these
# formats can never be empty files.

CREATEPROTO=unixproto
EMPTYPROTO=unixproto


# Commands possibly overriden by the individual port

ARRC=ar rc
CC=cc
LN=ln -s
RANLIB=ranlib


# Standard distribution build parameters

DEFAULTAUTHENTICATORS=md5 pla log
DEFAULTDRIVERS=imap nntp pop3 mh mx mbx tenex mtx mmdf unix news phile


# Normally no need to change any of these

ARCHIVE=c-client.a
BINARIES=osdep.o mail.o misc.o newsrc.o smanager.o utf8.o siglocal.o \
 dummy.o pseudo.o netmsg.o flstring.o fdstring.o \
 rfc822.o nntp.o smtp.o imap4r1.o pop3.o \
 unix.o mbx.o mmdf.o tenex.o mtx.o news.o phile.o mh.o mx.o
CFLAGS=-g

CAT=cat
MAKE=make
MV=mv
RM=rm -rf
SH=sh


# Primary build command

BUILD=$(MAKE) build EXTRACFLAGS='$(EXTRACFLAGS)'\
 EXTRALDFLAGS='$(EXTRALDFLAGS)'\
 EXTRADRIVERS='$(EXTRADRIVERS)' EXTRAAUTHENTICATORS='$(EXTRAAUTHENTICATORS)'\
 PASSWDTYPE=$(PASSWDTYPE) SSLTYPE=$(SSLTYPE) IP=$(IP)


# Here if no make argument established

missing: osdep.h
	$(MAKE) all `$(CAT) SPECIALS`

osdep.h:
	@echo You must specify what type of system
	@false


# Current ports

a32:	# AIX 3.2 for RS/6000
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -Dunix=1 -D_BSD" \
	 BASELDFLAGS="-lbsd"

a41:	# AIX 4.1 for RS/6000
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=a41 CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -Dunix=1 -D_BSD -qro -qroconst" \
	 BASELDFLAGS="-ls"

aix:	# AIX/370
	@echo You are building for AIX on an S/370 class machine
	@echo If you want AIX on an RS/6000 you need to use a32 or a41 instead!
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g" \
	 BASELDFLAGS="-lbsd"

aos:	# AOS for RT
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g -Dconst="

art:	# AIX 2.2.1 for RT
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 \
	 SPOOLDIR=/var \
	 ACTIVEFILE=/usr/local/news/control/active \
	 RSHPATH=/bin/rsh \
	 BASECFLAGS="-g -Dconst= -Dvoid=char" \
	 RANLIB=true 

asv:	# Altos SVR4
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 LOGINPW=old \
	 ACTIVEFILE=/usr/spool/news/active \
	 RSHPATH=/usr/bin/rcmd \
	 BASECFLAGS="-Dconst= -DSIGSTOP=SIGKILL" \
	 BASELDFLAGS="-lsocket -lrpc -lgen -lcrypt -lxenix" \
	 RANLIB=true

aux:	# A/UX
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 MAILSPOOL=/usr/mail \
	 BASECFLAGS="-g -B/usr/lib/big/ -Dvoid=char -Dconst=" \
	 RANLIB=true ARRC="ar -rc"

bs3:	# BSD/i386 3.0 or higher
	$(BUILD) `$(CAT) SPECIALS` OS=bsi \
	 CHECKPW=bsi LOGINPW=bsi CRXTYPE=nfs \
	 SPOOLDIR=/var NEWSSPOOL=/var/news/spool \
	 ACTIVEFILE=/var/news/etc/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g $(GCCOPTLEVEL) -pipe" CC=shlicc

bsd:	# BSD UNIX
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g -Dconst="

bsf:	# FreeBSD
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CRXTYPE=nfs \
	 SPOOLDIR=/var \
	 ACTIVEFILE=/usr/local/news/lib/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -O -pipe" \
	 BASELDFLAGS="-lcrypt"

bsi:	# BSD/i386
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 SPOOLDIR=/var NEWSSPOOL=/var/news/spool \
	 ACTIVEFILE=/var/news/etc/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g $(GCCOPTLEVEL) -pipe"

bso:	# OpenBSD
	$(BUILD) `$(CAT) SPECIALS` OS=bsi \
	 SIGTYPE=psx CRXTYPE=nfs \
	 SPOOLDIR=/var \
	 ACTIVEFILE=/usr/local/news/lib/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="$(GCCOPTLEVEL) -pipe"

cvx:	# Convex
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-O -ext -Dconst="

cyg:	# Cygwin - note that most local file drivers don't work!!
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	DEFAULTDRIVERS="imap nntp pop3 mbx unix phile" \
	SIGTYPE=psx CHECKPW=cyg LOGINPW=cyg CRXTYPE=std \
	SPOOLDIR=/var \
	ACTIVEFILE=/usr/local/news/lib/active \
	RSHPATH=/usr/bin/rsh \
	BASECFLAGS="-g -O" \
	BASELDFLAGS="-lcrypt" \
	CC=gcc

d-g:	# Data General DG/UX
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 CRXTYPE=nfs \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/local/news/active \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-g -Dconst=" \
	 BASELDFLAGS="-lnsl -lsocket" \
	 RANLIB=true

d54:	# Data General DG/UX 5.4
	$(BUILD) `$(CAT) SPECIALS` OS=d-g \
	 SIGTYPE=sv4 CRXTYPE=nfs \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/local/news/active \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-g -Dconst=" \
	 BASELDFLAGS="-lnsl -lsocket" \
	 RANLIB=true

dpx:	# Bull DPX/2
	$(BUILD) `$(CAT) SPECIALS` OS=sv4 \
	 SIGTYPE=sv4 CHECKPW=sv4 LOGINPW=sv4 \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-Dconst= -DSYSTEM5 -DSHORT_IDENT" \
	 BASELDFLAGS="-linet" \
	 RANLIB=true LN=ln

drs:	# ICL DRS/NX
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 CHECKPW=sv4 LOGINPW=sv4 CRXTYPE=nfs \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-O" \
	 BASELDFLAGS="-lsocket -lgen" \
	 RANLIB=true

do4:	# Apollo Domain/OS sr10.4
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-A systype,bsd4.3 -D_APOLLO_SOURCE" \
	 RANLIB=true

dyn:	# Dynix
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g -Dconst="

epx:	# EP/IX
	$(BUILD) `$(CAT) SPECIALS` OS=sv4 \
	 SIGTYPE=sv4 CHECKPW=sv4 LOGINPW=sv4 \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/usr/share/news/active \
	 RSHPATH=/usr/net/rsh \
	 BASECFLAGS="-g -systype svr4" \
	 BASELDFLAGS="-lsocket -lnsl -lgen" \
	 RANLIB=true

ga4:	# GCC AIX 4.1 for RS/6000
	$(BUILD) `$(CAT) SPECIALS` OS=a41 \
	 SIGTYPE=psx CHECKPW=a41 CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -Dunix=1 -D_BSD" \
	 BASELDFLAGS="-ls"

gas:	# GCC Altos SVR4
	$(BUILD) `$(CAT) SPECIALS` OS=asv \
	 SIGTYPE=sv4 LOGINPW=old \
	 ACTIVEFILE=/usr/spool/news/active \
	 RSHPATH=/usr/bin/rcmd \
	 BASECFLAGS="-g -O -DALTOS_SYSTEM_V -DSIGSTOP=SIGKILL" \
	 BASELDFLAGS="-lsocket -lrpc -lgen -lcrypt -lxenix" \
	 RANLIB=true CC=gcc

gh9:	# GCC HP-UX 9.x
	$(BUILD) `$(CAT) SPECIALS` OS=hpp \
	 SIGTYPE=psx CRXTYPE=nfs \
	 MAILSPOOL=/usr/mail \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-g $(GCCOPTLEVEL)" \
	 RANLIB=true CC=gcc

ghp:	# GCC HP-UX 10.x
	$(BUILD) `$(CAT) SPECIALS` OS=hpp \
	 SIGTYPE=psx CRXTYPE=nfs \
	 SPOOLDIR=/var \
	 ACTIVEFILE=/var/news/active \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-g $(GCCOPTLEVEL)" \
	 RANLIB=true CC=gcc

ghs:	# GCC HP-UX with Trusted Computer Base
	$(BUILD) `$(CAT) SPECIALS` OS=shp \
	 SIGTYPE=psx CHECKPW=sec CRXTYPE=nfs \
	 SPOOLDIR=/var \
	 ACTIVEFILE=/var/news/active \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-g $(GCCOPTLEVEL)" \
	 BASELDFLAGS="-lnet -lV3 -lsec" \
	 RANLIB=true CC=gcc

go5:	# GCC 2.7.1 (95q4) SCO Open Server 5.0.x
	$(BUILD) `$(CAT) SPECIALS` OS=sc5 \
	 SIGTYPE=psx CHECKPW=sec LOGINPW=sec \
	 CREATEPROTO=mmdfproto EMPTYPROTO=mmdfproto \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rcmd \
	 BASECFLAGS="$(GCCOPTLEVEL) -I/usr/include -L/lib" \
	 BASELDFLAGS="-lsocket -lprot -lx -ltinfo -lm" \
	 RANLIB=true CC=gcc

gsc:	# Santa Cruz Operation
	$(BUILD) `$(CAT) SPECIALS` OS=sco \
	 SIGTYPE=sv4 CHECKPW=sec LOGINPW=sec \
	 CREATEPROTO=mmdfproto EMPTYPROTO=mmdfproto \
	 RSHPATH=/usr/bin/rcmd \
	 BASECFLAGS="$(GCCOPTLEVEL)" \
	 BASELDFLAGS="-lsocket -lprot -lcrypt_i -lx -los" \
	 RANLIB=true LN=ln CC=gcc

gsg:	# GCC Silicon Graphics
	$(BUILD) `$(CAT) SPECIALS` OS=sgi \
	 SIGTYPE=sv4 CRXTYPE=nfs \
	 MAILSPOOL=/usr/mail \
	 RSHPATH=/usr/bsd/rsh \
	 BASECFLAGS="-g $(GCCOPTLEVEL)" \
	 RANLIB=true CC=gcc

gso:	os_sol.h	# GCC Solaris
	$(BUILD) `$(CAT) SPECIALS` OS=sol \
	 SIGTYPE=psx CHECKPW=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/usr/share/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g $(GCCOPTLEVEL)" \
	 BASELDFLAGS="-lsocket -lnsl -lgen" \
	 RANLIB=true CC=gcc

gsu:	# GCC SUN-OS
	$(BUILD) `$(CAT) SPECIALS` OS=sun \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g $(GCCOPTLEVEL)" \
	 BASELDFLAGS="-ldl" \
	 CC=gcc

gul:	# GCC Ultrix
	$(BUILD) `$(CAT) SPECIALS` OS=ult \
	 SIGTYPE=psx CHECKPW=ult CRXTYPE=nfs \
	 BASECFLAGS="-g -O" \
	 BASELDFLAGS="-lauth -lc" \
	 CC=gcc

hpp:	# HP-UX 9.x
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CRXTYPE=nfs \
	 MAILSPOOL=/usr/mail \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-g -Aa -D_HPUX_SOURCE" \
	 BASELDFLAGS="-lnet -lV3" \
	 RANLIB=true

hpx:	# HP-UX 10.x
	$(BUILD) `$(CAT) SPECIALS` OS=hpp \
	 SIGTYPE=psx CRXTYPE=nfs \
	 SPOOLDIR=/var \
	 ACTIVEFILE=/var/news/active \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-g -Ae" \
	 BASELDFLAGS="-lnet -lV3" \
	 RANLIB=true

isc:	# Interactive
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 CHECKPW=sv4 LOGINPW=sv4 \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/var/spool/news/active \
	 BASECFLAGS="-Xp -D_SYSV3" \
	 BASELDFLAGS="-linet -lnsl_s -lgen -lx -lsec -liberty" \
	 RANLIB=true

lnp:	# Linux Pluggable Authentication modules
	$(BUILD) `$(CAT) SPECIALS` OS=lnx \
	 SIGTYPE=psx CHECKPW=pam CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -fno-omit-frame-pointer $(GCCOPTLEVEL)" \
	 BASELDFLAGS="$(PAMLDFLAGS)"

lnx:	# Linux non-shadow passwords
	@echo You are building for traditional Linux *without* shadow
	@echo passwords and with the crypt function in the C library.
	@echo If your system has shadow passwords, or if crypt is not
	@echo in the C library, you must use slx, sl4, or sl5 instead!
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -O"

lyn:	# LynxOS
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 RSHPATH=/bin/rsh \
	 BASECFLAGS="-g -O -pipe" \
	 BASELDFLAGS=-lbsd \
	 CC=gcc

mct:	# MachTen - CRXTYPE=nfs doesn't work (at least not on 2.2)
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SPOOLDIR=/var/spool \
	 BASECFLAGS="-g -O -pipe"

mnt:	# Mint
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -O"

neb:	# NetBSD
	$(BUILD) `$(CAT) SPECIALS` OS=bsi \
	 CRXTYPE=nfs \
	 SPOOLDIR=/var \
	 ACTIVEFILE=/var/db/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -O -pipe" \
	 BASELDFLAGS="-lcrypt"

nec:	# NEC UX
	$(BUILD) `$(CAT) SPECIALS` OS=sv4 \
	 SIGTYPE=sv4 CHECKPW=sv4 \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/var/news/lib/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -Kopt=2 -KOlimit=2000" \
	 BASELDFLAGS="-lsocket -lnsl -lgen" \
	 RANLIB=true CC=/usr/abiccs/bin/cc

nto:	# QNX Neutrino RTP
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -O"

nxt:	# NEXTSTEP
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g -O -pipe"

nx3:	# NEXTSTEP 3.0 single threaded
	$(BUILD) `$(CAT) SPECIALS` OS=nxt \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g -O -pipe -Wall"
	echo "void malloc_singlethreaded (void);" >> linkage.h
	echo "  malloc_singlethreaded ();" >> linkage.c

osf:	# OSF/1
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 BASECFLAGS="-g3 -w -O2 -Olimit 1500"

# Note: sia_become_user() used by LOGINPW=os4 doesn't seem to work right.  The
# user doesn't get proper file access, and the process can't be killed.

os4:	# OSF/1 (Digital UNIX) 4
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=os4 LOGINPW=os4 CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 BASECFLAGS="-g3 -w -std0 -O2"

osx:	# Mac OS X
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 BASECFLAGS="-g -O -Wno-pointer-sign"

ptx:	# PTX
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=svo LOGINPW=sv4 CRXTYPE=nfs \
	 MAILSPOOL=/usr/mail \
	 RSHPATH=/usr/bin/resh \
	 BASECFLAGS="-Wc,-O3 -Wc,-seq -Dprivate=PRIVATE" \
	 BASELDFLAGS="-lseq -lsec -lsocket -linet -lnsl -lgen" \
	 RANLIB=true

pyr:	# Pyramid
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g -Dconst="

qnx:	# QNX
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CHECKPW=psx LOGINPW=old \
	 RSHPATH=/usr/ucb/rsh \
	 BASECFLAGS="-Otax -g -Dunix=1 -D_POSIX_SOURCE" \
	 BASELDFLAGS="-g -N128k -llogin -lsocket -lunix"

s40:	# SUN-OS 4.0
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g -Dconst="

sc5:	# SCO Open Server 5.0
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=sec LOGINPW=sec \
	 CREATEPROTO=mmdfproto EMPTYPROTO=mmdfproto \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rcmd \
	 BASECFLAGS="-O3 -s -belf" \
	 BASELDFLAGS="-lsocket -lprot -lx -ltinfo -lm" \
	 RANLIB=true

sco:	# Santa Cruz Operation
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 CHECKPW=sec LOGINPW=sec \
	 CREATEPROTO=mmdfproto EMPTYPROTO=mmdfproto \
	 RSHPATH=/usr/bin/rcmd \
	 BASECFLAGS="-O3" \
	 BASELDFLAGS="-lsocket -lprot -lcrypt_i -lx -los" \
	 RANLIB=true LN=ln

# Note: setting _POSIX_SOURCE doesn't seem to build it as of SGI IRIX 5.3

sgi:	# Silicon Graphics
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 CRXTYPE=nfs \
	 MAILSPOOL=/usr/mail \
	 RSHPATH=/usr/bsd/rsh \
	 BASECFLAGS="-g3 -O2 -Olimit 8192" \
	 RANLIB=true

sg6:	# Silicon Graphics, IRIX 6.5
	MAKEFLAGS= $(BUILD) `$(CAT) SPECIALS` OS=sgi \
	 SIGTYPE=sv4 CRXTYPE=nfs \
	 MAILSPOOL=/usr/mail \
	 RSHPATH=/usr/bsd/rsh \
	 BASECFLAGS="-g3 -O2 -OPT:Olimit=0 -woff 1110,1116" \
	 RANLIB=true

# Note: Mark Kaesling says that setluid() isn't in HP-UX with SecureWare.

shp:	# HP-UX with Trusted Computer Base
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=sec CRXTYPE=nfs \
	 SPOOLDIR=/var \
	 ACTIVEFILE=/var/news/active \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-g -Ae" \
	 BASELDFLAGS="-lnet -lV3 -lsec" \
	 RANLIB=true

slx:	# Secure Linux
	@echo You are building for libc6/glibc versions of Secure Linux
	@echo If you want libc5 versions you must use sl5 instead!
	@echo If you want libc4 versions you must use sl4 instead!
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -fno-omit-frame-pointer $(GCCOPTLEVEL)" \
	 BASELDFLAGS="-lcrypt"

sl4:	# Secure Linux using libc4
	@echo You are building for libc4 versions of Secure Linux
	@echo If you want libc6/glibc versions you must use slx instead!
	@echo If you want libc5 versions you must use sl5 instead!
	$(BUILD) `$(CAT) SPECIALS` OS=slx \
	 SIGTYPE=psx CHECKPW=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -fno-omit-frame-pointer $(GCCOPTLEVEL)" \
	 BASELDFLAGS="-lshadow"

sl5:	# Secure Linux using libc5
	@echo You are building for libc5 versions of Secure Linux
	@echo If you want libc6/glibc versions you must use slx instead!
	@echo If you want libc4 versions you must use sl4 instead!
	$(BUILD) `$(CAT) SPECIALS` OS=slx \
	 SIGTYPE=psx CHECKPW=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool \
	 ACTIVEFILE=/var/lib/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -fno-omit-frame-pointer $(GCCOPTLEVEL)"

snx:	# Siemens Nixdorf SINIX and Reliant UNIX
	$(BUILD) `$(CAT) SPECIALS` OS=sv4 \
	 SIGTYPE=psx CHECKPW=sv4 \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/usr/share/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -D_SYS_CLOCK_H -Dconst=" \
	 BASELDFLAGS="-lsocket -lnsl -lgen" \
	 RANLIB=true

# Sorry about the -w, but the cretinous SUN Workshop Pro C compiler barfs on
# implicit casts between char and unsigned char.

soc:	os_sol.h	# Solaris with cc
	$(BUILD) `$(CAT) SPECIALS` OS=sol \
	 SIGTYPE=psx CHECKPW=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/usr/share/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -O -w" \
	 BASELDFLAGS="-lsocket -lnsl -lgen" \
	 RANLIB=true CC=/opt/SUNWspro/bin/cc


# Note: It is a long and disgusting story about why cc is set to ucbcc.  You
# need to invoke the C compiler so that it links with the SVR4 libraries and
# not the BSD libraries, otherwise readdir() will return the wrong information.
# Of all the names in the most common path, ucbcc is the only name to be found
# (on /usr/ccs/bin) that points to a suitable compiler.  cc is likely to be
# /usr/ucb/cc which is absolutely not the compiler that you want.  The real
# SVR4 cc is probably something like /opt/SUNWspro/bin/cc which is rarely in
# anyone's path.
#
# ucbcc is probably a link to acc, e.g. /opt/SUNWspro/SC4.0/bin/acc, and is
# the UCB C compiler using the SVR4 libraries.
#
# If ucbcc isn't on your system, then punt on the SUN C compiler and use gcc
# instead (the gso port instead of the sol port).
# 
# If, in spite of all the above warnings, you choose to use the "soc" port
# instead of the "sol" port, be sure to check the behavior of the LIST command
# in imapd.  Also, note that the "soc" port uses -O.  If you want to use the
# real SVR4 compiler, you must use -O.  If it works to compile with -O2, then
# cc is probably using the UCB compiler with BSD libraries, and will not build
# a good binary
#
# To recap:
# 1) The sol port is designed to be built using the UCB compiler using the
#    SVR4 libraries.  This compiler is "ucbcc", which is lunk to acc.  You
#    use -O2 as one of the CFLAGS.
# 2) If you build the sol port with the UCB compiler using the BSD libraries,
#    you will get no error messages but you will get bad binaries (the most
#    obvious symptom is dropping the first two characters return filenames
#    from the imapd LIST command.  This compiler also uses -O2, and is very
#    often what the user gets from "cc".  BEWARE!!!
# 3) If you build the sol port with the real SVR4 compiler, which is often
#    hidden away or unavailable on many systems, then you will get errors
#    from -O2 and you need to change that to -O.  But you will get a good
#    binary.  However, you should try it with -O2 first, to make sure that
#    you got this compiler and not the UCB compiler using BSD libraries.

sol:	os_sol.h	# Solaris
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=psx CRXTYPE=nfs \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/usr/share/news/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g -O2" \
	 BASELDFLAGS="-lsocket -lnsl -lgen" \
	 RANLIB=true CC=ucbcc

sos:	# Secure OSF/1
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=sce LOGINPW=sec CRXTYPE=nfs \
	 BASECFLAGS="-g3 -w -O2 -Olimit 1500" \
	 BASELDFLAGS="-lsecurity -laud"

ssn:	# Secure SUN-OS
	$(BUILD) `$(CAT) SPECIALS` OS=sun \
	 CHECKPW=ssn CRXTYPE=nfs \
	 BASECFLAGS="-g -Dconst=" \
	 BASELDFLAGS="-ldl"

sun:	# SUN-OS
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-g -Dconst=" \
	 BASELDFLAGS="-ldl"

sv2:	# SVR2
	@echo You are being *very* optimistic!
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 LOGINPW=old \
	 MAILSPOOL=/usr/mail \
	 RSHPATH=/usr/bin/remsh \
	 BASECFLAGS="-Dconst= -DSYSTEM5 -DSHORT_IDENT -I/usr/ethernet/include" \
	 BASELDFLAGS="-lnet" \
	 RANLIB=true LN=ln

sv4:	# SVR4
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=sv4 CHECKPW=sv4 LOGINPW=sv4 \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/usr/share/news/active \
	 RSHPATH=/usr/bin/resh \
	 BASECFLAGS="-g -Dconst=" \
	 BASELDFLAGS="-lsocket -lnsl -lgen" \
	 RANLIB=true

ult:	# Ultrix
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 SIGTYPE=psx CHECKPW=ult CRXTYPE=nfs \
	 BASECFLAGS="-g3 -O2 -Olimit 1500 -Dconst=" \
	 BASELDFLAGS="-lauth -lc"

uw2:	# UnixWare SVR4.2
	$(BUILD) `$(CAT) SPECIALS` OS=sv4 \
	 SIGTYPE=sv4 CHECKPW=sv4 \
	 SPOOLDIR=/var/spool MAILSPOOL=/var/mail \
	 ACTIVEFILE=/var/news/lib/active \
	 RSHPATH=/usr/bin/rsh \
	 BASECFLAGS="-g" \
	 BASELDFLAGS="-lsocket -lnsl -lgen" \
	 RANLIB=true

vul:	# VAX Ultrix
	$(BUILD) `$(CAT) SPECIALS` OS=ult \
	 SIGTYPE=psx CHECKPW=ult CRXTYPE=nfs \
	 BASECFLAGS="-O2 -Dconst=" \
	 BASELDFLAGS="-lauth -lc"

vu2:	# VAX Ultrix 2.3, etc.
	$(BUILD) `$(CAT) SPECIALS` OS=$@ \
	 CRXTYPE=nfs \
	 BASECFLAGS="-O2 -Dconst= -Dvoid=char"


# Build it!

build:	clean once $(ARCHIVE)

all:	$(ARCHIVE)

$(ARCHIVE): $(BINARIES)
	sh -c '$(RM) $(ARCHIVE) || true'
	@$(CAT) ARCHIVE
	@$(SH) ARCHIVE

.c.o:
	`$(CAT) CCTYPE` -c `$(CAT) CFLAGS` $*.c


# Cleanup

clean:
	sh -c '$(RM) auths.c crexcl.c nfstest.c linkage.[ch] siglocal.c osdep*.[ch] *.o ARCHIVE *FLAGS *TYPE $(ARCHIVE) || true'


# Dependencies

dummy.o: mail.h misc.h osdep.h dummy.h
fdstring.o: mail.h misc.h osdep.h fdstring.h
flstring.o: mail.h misc.h osdep.h flstring.h
imap4r1.o: mail.h misc.h osdep.h imap4r1.h rfc822.h
mail.o: mail.h misc.h osdep.h rfc822.h linkage.h
mbx.o: mail.h misc.h osdep.h mbx.h dummy.h
mh.o: mail.h misc.h osdep.h mh.h dummy.h
mx.o: mail.h misc.h osdep.h mx.h dummy.h
misc.o: mail.h misc.h osdep.h
mmdf.o: mail.h misc.h osdep.h pseudo.h dummy.h
mtx.o: mail.h misc.h osdep.h dummy.h
netmsg.o: mail.h misc.h osdep.h netmsg.h
news.o: mail.h misc.h osdep.h
newsrc.o: mail.h misc.h osdep.h newsrc.h
nntp.o: mail.h misc.h osdep.h netmsg.h smtp.h nntp.h rfc822.h
phile.o: mail.h misc.h osdep.h rfc822.h dummy.h
pseudo.o: pseudo.h
pop3.o: mail.h misc.h osdep.h rfc822.h
smanager.o: mail.h misc.h osdep.h
smtp.o: mail.h misc.h osdep.h smtp.h rfc822.h
rfc822.o: mail.h misc.h osdep.h rfc822.h
tenex.o: mail.h misc.h osdep.h dummy.h
unix.o: mail.h misc.h osdep.h unix.h pseudo.h dummy.h
utf8.o: mail.h misc.h osdep.h utf8.h


# OS-dependent

osdep.o:mail.h misc.h env.h fs.h ftl.h nl.h tcp.h \
	osdep.h env_unix.h tcp_unix.h \
	osdep.c env_unix.c fs_unix.c ftl_unix.c nl_unix.c tcp_unix.c ip_unix.c\
	auths.c crexcl.c flockcyg.c flocklnx.c flocksim.c nfstest.c fsync.c \
	gethstid.c getspnam.c \
	gr_wait.c gr_wait4.c gr_waitp.c \
	kerb_mit.c \
	auth_gss.c auth_log.c auth_md5.c auth_pla.c \
	pmatch.c scandir.c setpgrp.c strerror.c truncate.c write.c \
	memmove.c memmove2.c memset.c \
	tz_bsd.c tz_nul.c tz_sv4.c \
	write.c sslstdio.c \
	strerror.c strpbrk.c strstr.c strtok.c strtoul.c \
	OSCFLAGS
	@echo Building OS-dependent module
	@echo If you get No such file error messages for files x509.h, ssl.h,
	@echo pem.h, buffer.h, bio.h, and crypto.h, that means that OpenSSL
	@echo is not installed on your system.  Either install OpenSSL first
	@echo or build with command: make `$(CAT) OSTYPE` SSLTYPE=none
	`$(CAT) CCTYPE` -c `$(CAT) CFLAGS` `$(CAT) OSCFLAGS` -c osdep.c

osdep.c: osdepbas.c osdepckp.c osdeplog.c osdepssl.c
	$(CAT) osdepbas.c osdepckp.c osdeplog.c osdepssl.c > osdep.c

osdepbas.c:
	@echo osdepbas.c not found...try make clean and new make
	@false

osdepckp.c:
	@echo osdepckp.c not found...try make clean and new make
	@false

osdeplog.c:
	@echo osdeplog.c not found...try make clean and new make
	@false

osdepssl.c:
	@echo osdepssl.c not found...try make clean and new make
	@false

nfstest.c:
	@echo nfstest.c not found...try make clean and new make
	@false

siglocal.c:
	@echo siglocal.c not found...try make clean and new make
	@false

crexcl.c:
	@echo crexcl.c not found...do make clean and new make
	@false

ip_unix.c:
	@echo ip_unix.c not found...do make clean and new make
	@false

os_sol.h:
	sh -c '(strings /lib/libc.a | grep getpassphrase > /dev/null) && $(LN) os_soln.h os_sol.h || $(LN) os_solo.h os_sol.h'


# Once-only environment setup

once:	onceenv ckp$(PASSWDTYPE) ssl$(SSLTYPE) osdep.c

onceenv:
	@echo Once-only environment setup...
	echo $(CC) > CCTYPE
	echo $(BASECFLAGS) '$(EXTRACFLAGS)' > CFLAGS
	echo -DCREATEPROTO=$(CREATEPROTO) -DEMPTYPROTO=$(EMPTYPROTO) \
	 -DMAILSPOOL=\"$(MAILSPOOL)\" \
	 -DANONYMOUSHOME=\"$(MAILSPOOL)/anonymous\" \
	 -DACTIVEFILE=\"$(ACTIVEFILE)\" -DNEWSSPOOL=\"$(NEWSSPOOL)\" \
	 -DRSHPATH=\"$(RSHPATH)\" -DLOCKPGM=\"$(LOCKPGM)\" > OSCFLAGS
	echo $(BASELDFLAGS) $(EXTRALDFLAGS) > LDFLAGS
	echo "$(ARRC) $(ARCHIVE) $(BINARIES);$(RANLIB) $(ARCHIVE)" > ARCHIVE
	echo $(OS) > OSTYPE
	./drivers $(EXTRADRIVERS) $(DEFAULTDRIVERS) dummy
	./mkauths $(EXTRAAUTHENTICATORS) $(DEFAULTAUTHENTICATORS)
	$(LN) os_$(OS).h osdep.h
	$(LN) os_$(OS).c osdepbas.c
	$(LN) log_$(LOGINPW).c osdeplog.c
	$(LN) sig_$(SIGTYPE).c siglocal.c
	$(LN) crx_$(CRXTYPE).c crexcl.c
	$(LN) ip$(IP)_unix.c ip_unix.c
	sh -c '(test -f /usr/include/sys/statvfs.h -a $(OS) != sc5 -a $(OS) != sco) && $(LN) nfstnew.c nfstest.c || $(LN) nfstold.c nfstest.c'


# Password checkers

ckpafs:	# AFS
	@echo AFS password authentication
	echo $(AFSCFLAGS) >> OSCFLAGS
#	echo $(AFSLDFLAGS) >> LDFLAGS
# Note: Steve Roseman says that AFS libraries have to be lunk before SSL
	echo $(AFSLDFLAGS) `$(CAT) LDFLAGS` > LDFLAGS.tmp
	mv LDFLAGS.tmp LDFLAGS
	$(LN) ckp_afs.c osdepckp.c

ckpdce:	# DCE
	@echo DCE password authentication
	echo $(DCECFLAGS) >> OSCFLAGS
	echo $(DCELDFLAGS) >> LDFLAGS
	$(LN) ckp_dce.c osdepckp.c

ckpgss:	# Kerberos V (must have gss EXTRAAUTHENTICATOR as well)
	@echo Kerberos V password authentication
	$(LN) ckp_gss.c osdepckp.c

ckpnul:	# NUL authenticator (disables all plaintext authentication)
	@echo Plaintext authentication prohibited
	echo "  mail_parameters (NIL,SET_DISABLEPLAINTEXT,(void *) 1);" >> linkage.c
	$(LN) ckp_nul.c osdepckp.c

ckppam:	# Pluggable Authentication Modules authenticator
	@echo PAM password authentication
	echo $(PAMLDFLAGS) >> LDFLAGS
	$(LN) ckp_pam.c osdepckp.c

ckppmb:	# Broken (e.g. SUN) Pluggable Authentication Modules authenticator
	@echo Broken PAM password authentication
	echo $(PAMLDFLAGS) >> LDFLAGS
	$(LN) ckp_pmb.c osdepckp.c

ckpstd:	# Port standard
	@echo Standard password authentication
	$(LN) ckp_$(CHECKPW).c osdepckp.c

ckptwo:	# Something plus standard
	@echo $(CHECKPWALT) password authentication first, then standard
	$(CAT) ckp_1st.c ckp_$(CHECKPWALT).c ckp_2nd.c ckp_$(CHECKPW).c \
	 ckp_3rd.c > osdepckp.c


# SSL support

sslnone:# No SSL
	@echo Building without SSL support
	$(LN) ssl_none.c osdepssl.c

sslnopwd: sslunix snopwd

sslunix.nopwd: sslnopwd

sslsco.nopwd: sslsco snopwd

sslunix: sbasic sldunix

sslsco:	sbasic sldsco

sbasic:	# UNIX OpenSSL
	@echo Building with SSL
	$(LN) ssl_unix.c osdepssl.c
	echo $(SSLCFLAGS) >> OSCFLAGS
	echo "  ssl_onceonlyinit ();" >> linkage.c

snopwd:	# Plaintext disable
	@echo Building with SSL and plaintext passwords disabled unless SSL/TLS
	echo "  mail_parameters (NIL,SET_DISABLEPLAINTEXT,(void *) 2);" >> linkage.c

sldunix:# Normal UNIX SSL load flags
	echo $(SSLLDFLAGS) >> LDFLAGS


sldsco:	# SCO SSL load flags
# Note: Tim Rice says that SSL has to be lunk before other libraries on SCO.
	echo $(SSLLDFLAGS) `cat LDFLAGS` > LDFLAGS.tmp
	mv LDFLAGS.tmp LDFLAGS


# A monument to a hack of long ago and far away...

love:
	@echo not war?
