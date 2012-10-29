# Program:	IMAPD Makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		5 November 1990
# Last Edited:	29 June 2004
#
# The IMAP toolkit provided in this Distribution is
# Copyright 1988-2004 University of Washington.
#
# The full text of our legal notices is contained in the file called
# CPYRIGHT, included with this Distribution.


ALERT=/etc/imapd.alert
USERALERT=.imapalert
SHUTDOWN=/etc/nologin
ANO=/etc/anonymous.newsgroups
NNTP=/etc/imapd.nntp
SHELL= /bin/sh


# Un-comment this to get somewhat better interoperability with Netscape.  It
# causes the "Manage Mail" menu item to open the given URL, e.g. to point to
# an alternative IMAP client (e.g. Pine) or perhaps to a homebrew mail
# account management page.
#NSBD= -DNETSCAPE_BRAIN_DAMAGE=\"http://www.washington.edu/pine\"



# Un-comment this to get possibly better interoperability with Microsoft
# Entourage.  It disables an untagged NO which happens when that extremely
# broken program does a STATUS on the selected mailbox.  This shouldn't be
# necessary, and it's been reported that Entourage works without having to
# un-comment this.
#ENBD= -DENTOURAGE_BRAIN_DAMAGE


# Get local definitions from c-client directory

C = ../c-client
CCLIENTLIB = $C/c-client.a
CC = `cat $C/CCTYPE`
CFLAGS = -I$C `cat $C/CFLAGS` $(NSBD) $(ENBD) -DANOFILE=\"$(ANO)\" \
	-DALERTFILE=\"$(ALERT)\" -DNNTPFILE=\"$(NNTP)\" \
	-DUSERALERTFILE=\"$(USERALERT)\" -DSHUTDOWNFILE=\"$(SHUTDOWN)\"
LDFLAGS = $(CCLIENTLIB) `cat $C/LDFLAGS`

all:	imapd

imapd: $(CCLIENTLIB) imapd.o
	$(CC) $(CFLAGS) -o imapd imapd.o $(LDFLAGS)

imapd.o: $C/mail.h $C/misc.h $C/osdep.h

$(CCLIENTLIB):
	cd $C;make

clean:
	rm -f *.o imapd || true

# A monument to a hack of long ago and far away...
love:
	@echo 'not war?'
