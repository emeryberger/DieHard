# Program:	MLOCK Makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		8 February 1999
# Last Edited:	18 November 2002
#
# The IMAP toolkit provided in this Distribution is
# Copyright 2002 University of Washington.
# The full text of our legal notices is contained in the file called
# CPYRIGHT, included with this Distribution.


C = ../c-client
SHELL = /bin/sh

# Get local definitions from c-client directory

CC = `cat $C/CCTYPE`
CFLAGS = `cat $C/CFLAGS`

all:	mlock

mlock:	mlock.o
	$(CC) $(CFLAGS) -o mlock mlock.o

install: mlock
	chgrp mail mlock
	chmod 3711 mlock
	cp -p mlock /etc/mlock

clean:
	rm -f *.o mlock || true

# A monument to a hack of long ago and far away...
love:
	@echo 'not war?'
