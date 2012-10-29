# Program:	C client makefile for Amiga
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
# Last Edited:	14 October 2003
#
# The IMAP toolkit provided in this Distribution is
# Copyright 2003 University of Washington.
#
# The full text of our legal notices is contained in the file called
# CPYRIGHT, included with this Distribution.


# Command line build parameters

EXTRAAUTHENTICATORS=
EXTRADRIVERS=mbox
PASSWDTYPE=std


# Build parameters normally set by the individual port

AMICFLAGS=-O -DNO_INLINE_STDARG -Dunix
AMILDFLAGS=/pine/libc.a -lamiga -lauto
CHECKPW=std
LOGINPW=std
ACTIVEFILE=/UULib/News/Active
SPOOLDIR=/usr/spool
MAILSPOOL=/AmiTCP/Mail
NEWSSPOOL=/UUNews


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
LN=cp
RANLIB=ranlib
RM=rm -f


# Standard distribution build parameters

DEFAULTAUTHENTICATORS=md5 pla log
DEFAULTDRIVERS=imap nntp pop3 mh mx mbx tenex mtx mmdf unix news phile


# Normally no need to change any of these

ARCHIVE=c-client.a
BINARIES=mail.o misc.o newsrc.o smanager.o osdep.o utf8.o \
 dummy.o pseudo.o netmsg.o flstring.o fdstring.o \
 rfc822.o nntp.o smtp.o imap4r1.o pop3.o \
 unix.o mbx.o mmdf.o tenex.o mtx.o news.o phile.o mh.o mx.o
CFLAGS=$(BASECFLAGS) $(EXTRACFLAGS)
MAKE=make
MV=mv
SHELL=/bin/sh


# Primary build command

BUILDOPTIONS= EXTRACFLAGS=$(EXTRACFLAGS) EXTRALDFLAGS=$(EXTRALDFLAGS)\
 EXTRADRIVERS=$(EXTRADRIVERS) EXTRAAUTHENTICATORS=$(EXTRAAUTHENTICATORS)\
 PASSWDTYPE=$(PASSWDTYPE)
BUILD=$(MAKE) build $(BUILDOPTIONS) $(SPECIALS)


# Here if no make argument established

missing: osdep.h
	$(MAKE) $(ARCHIVE) CC=`cat CCTYPE` CFLAGS="`cat CFLAGS`"

osdep.h:
	@echo You must specify what type of system
	@false


# Current ports

ami:	# AmigaDOS
	$(BUILD) OS=$@ \
	 BASECFLAGS="-DOLD $(AMICFLAGS)" \
	 BASELDFLAGS="$(AMILDFLAGS) -lamitcp000" \
	 CC=gcc

am2:	# AmigaDOS with a 68020+
	$(BUILD) OS=ami \
	 BASECFLAGS="-DOLD -m68020 $(AMICFLAGS)" \
	 BASELDFLAGS="$(AMILDFLAGS) -lamitcp" \
	 CC=gcc

amn:	# AmigaDOS with a 680x0 using "new" socket library
	$(BUILD) OS=ami \
	 BASELDFLAGS="$(AMILDFLAGS) -lnewamitcp000" \
	 CC=gcc

ama:	# AmigaDOS using AS225R2
	$(BUILD) OS=ami \
	 MAILSPOOL=/INet/Mail \
	 BASECFLAGS="-m68020 $(AMICFLAGS)" \
	 BASELDFLAGS="$(AMILDFLAGS) -las225r2" \
	 CC=gcc

# Build it!

build:	clean once ckp$(PASSWDTYPE) $(EXTRAAUTHENTICATORS) $(ARCHIVE)

$(ARCHIVE): $(BINARIES)
	$(RM) $(ARCHIVE) || true
	$(ARRC) $(ARCHIVE) $(BINARIES)
	$(RANLIB) $(ARCHIVE)

# Cleanup

clean:
	$(RM) *.o linkage.[ch] auths.c $(ARCHIVE) osdep.* *TYPE *FLAGS || true


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
pop3.o: mail.h misc.h osdep.h pop3.h rfc822.h
smanager.o: mail.h misc.h osdep.h
smtp.o: mail.h misc.h osdep.h smtp.h rfc822.h
rfc822.o: mail.h misc.h osdep.h rfc822.h
tenex.o: mail.h misc.h osdep.h dummy.h
unix.o: mail.h misc.h osdep.h unix.h pseudo.h dummy.h
utf8.o: mail.h misc.h osdep.h utf8.h


# OS-dependent

osdep.o:mail.h misc.h env.h fs.h ftl.h nl.h tcp.h \
	osdep.h env_ami.h tcp_ami.h \
	osdep.c env_ami.c fs_ami.c ftl_ami.c nl_ami.c tcp_ami.c \
	auths.c gethstid.c \
	gr_waitp.c \
	auth_log.c auth_md5.c auth_pla.c \
	pmatch.c scandir.c \
	tz_bsd.c \
	write.c \
	strerror.c strpbrk.c strstr.c strtok.c strtoul.c \
	OSCFLAGS
	$(CC) $(CFLAGS) `cat OSCFLAGS` -c osdep.c

osdep.c: osdepbas.c osdepckp.c osdeplog.c osdepssl.c
	$(RM) osdep.c || true
	cat osdepbas.c osdepckp.c osdeplog.c osdepssl.c > osdep.c


# Once-only environment setup

once:
	@echo Once-only environment setup...
	./drivers $(EXTRADRIVERS) $(DEFAULTDRIVERS) dummy
	./mkauths $(EXTRAAUTHENTICATORS) $(DEFAULTAUTHENTICATORS)
	echo $(CC) > CCTYPE
	echo $(CFLAGS) > CFLAGS
	echo -DCREATEPROTO=$(CREATEPROTO) -DEMPTYPROTO=$(EMPTYPROTO) \
	 -DMAILSPOOL=\"$(MAILSPOOL)\" \
	 -DACTIVEFILE=\"$(ACTIVEFILE)\" -DNEWSSPOOL=\"$(NEWSSPOOL)\" \
	 -DANONYMOUSHOME=\"$(MAILSPOOL)/anonymous\" > OSCFLAGS
	echo $(BASELDFLAGS) $(EXTRALDFLAGS) > LDFLAGS
	$(LN) os_$(OS).h osdep.h
	$(LN) os_$(OS).c osdepbas.c
	$(LN) log_$(LOGINPW).c osdeplog.c
	$(LN) ssl_none.c osdepssl.c


# Password checkers

ckpstd:	# Port standard
	$(LN) ckp_$(CHECKPW).c osdepckp.c


# A monument to a hack of long ago and far away...

love:
	@echo not war?
