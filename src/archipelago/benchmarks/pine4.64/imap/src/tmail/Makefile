# Program:	tmail Makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		5 April 1993
# Last Edited:	18 November 2002
#
# The IMAP toolkit software provided in this Distribution is
# Copyright 2002 University of Washington.
# The full text of our legal notices is contained in the file called
# CPYRIGHT, included with this Distribution.


C = ../c-client
CCLIENTLIB = $C/c-client.a
SHELL = /bin/sh

# Get local definitions from c-client directory

CC = `cat $C/CCTYPE`
CFLAGS = -I$C `cat $C/CFLAGS`
LDFLAGS = $(CCLIENTLIB) `cat $C/LDFLAGS`

tmail: $(CCLIENTLIB) tmail.o
	$(CC) $(CFLAGS) -o tmail tmail.o $(LDFLAGS)

tmail.o: $C/mail.h $C/misc.h $C/osdep.h

$(CCLIENTLIB):
	cd $C;make

clean:
	rm -f *.o tmail

# A monument to a hack of long ago and far away...
love:
	@echo 'not war?'
