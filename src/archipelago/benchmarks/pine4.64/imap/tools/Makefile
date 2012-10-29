# Program:	Tools Makefile
#
# Author:	Mark Crispin
#		Networks and Distributed Computing
#		Computing & Communications
#		University of Washington
#		Administration Building, AG-44
#		Seattle, WA  98195
#		Internet: MRC@CAC.Washington.EDU
#
# Date:		2 June 1995
# Last Edited:	24 October 2000
#
# The IMAP toolkit provided in this Distribution is
# Copyright 2000 University of Washington.
#
# The full text of our legal notices is contained in the file called
# CPYRIGHT, included with this Distribution.


CC=cc
RM=rm -f


uahelper:
	$(CC) -o uahelper uahelper.c $(LDFLAGS)

clean:
	sh -c '$(RM) *.o uahelper || true'

# A monument to a hack of long ago and far away...
love:
	@echo 'not war?'
