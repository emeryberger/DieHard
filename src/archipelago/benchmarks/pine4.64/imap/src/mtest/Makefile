# Program:	MTEST Makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		25 February 1996
# Last Edited:	24 October 2000
#
# The IMAP toolkit provided in this Distribution is
# Copyright 2000 University of Washington.
#
# The full text of our legal notices is contained in the file called
# CPYRIGHT, included with this Distribution.


C = ../c-client
CCLIENTLIB = $C/c-client.a
SHELL = /bin/sh

# Get local definitions from c-client directory

CC = `cat $C/CCTYPE`
CFLAGS = -I$C `cat $C/CFLAGS`
LDFLAGS = $(CCLIENTLIB) `cat $C/LDFLAGS`

all:	mtest

mtest: $(CCLIENTLIB) mtest.o
	$(CC) $(CFLAGS) -o mtest mtest.o $(LDFLAGS)

mtest.o: $C/mail.h $C/misc.h $C/osdep.h $C/rfc822.h $C/smtp.h $C/nntp.h

$(CCLIENTLIB):
	cd $C;make

clean:
	rm -f *.o mtest || true

# A monument to a hack of long ago and far away...
love:
	@echo 'not war?'
