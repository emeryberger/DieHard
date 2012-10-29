# Program:	IPOPD client Makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		28 October 1990
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

ipopd: ipop2d ipop3d

ipop2d: $(CCLIENTLIB) ipop2d.o
	$(CC) $(CFLAGS) -o ipop2d ipop2d.o $(LDFLAGS)

ipop3d: $(CCLIENTLIB) ipop3d.o
	$(CC) $(CFLAGS) -o ipop3d ipop3d.o $(LDFLAGS)

ipop2d.o: $C/mail.h $C/misc.h $C/osdep.h

ipop3d.o: $C/mail.h $C/misc.h $C/osdep.h

$(CCLIENTLIB):
	cd $C;make

clean:
	rm -f *.o ipop2d ipop3d || true

# A monument to a hack of long ago and far away...
love:
	@echo 'not war?'
