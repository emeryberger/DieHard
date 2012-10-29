# $Id: Makefile 8332 1998-05-11 18:33:22Z skramer $
#
#            T H E    P I N E    M A I L   S Y S T E M
#
#   Laurence Lundblade and Mike Seibel
#   Networks and Distributed Computing
#   Computing and Communications
#   University of Washington
#   Administration Building, AG-44
#   Seattle, Washington, 98195, USA
#   Internet: lgl@CAC.Washington.EDU
#             mikes@CAC.Washington.EDU
#
#   Please address all bugs and comments to "pine-bugs@cac.washington.edu"
#
#
#   Pine and Pico are registered trademarks of the University of Washington.
#   No commercial use of these trademarks may be made without prior written
#   permission of the University of Washington.
#
#   Pine, Pico, and Pilot software and its included text are Copyright
#   1989-1996 by the University of Washington.
#
#   The full text of our legal notices is contained in the file called
#   CPYRIGHT, included with this distribution.
#
#
#   Pine is in part based on The Elm Mail System:
#    ***********************************************************************
#    *  The Elm Mail System  -  Revision: 2.13                             *
#    *                                                                     *
#    * 			Copyright (c) 1986, 1987 Dave Taylor               *
#    * 			Copyright (c) 1988, 1989 USENET Community Trust    *
#    ***********************************************************************
# 
#
# These variables are specific to the University of Washington's 
#   WWW server environment -- modify as needed for your own:
# WWWDIR: location for HTML files on WWW server
# PNUTS: script to run pnuts navigation bar generator; for details on PNUTS,
#        see: http://hopf.math.nwu.edu/docs/utility.html
#        If you do not have the PNUTS program, set variable to /bin/true

SOURCES= index.html introduction.html background.html installation.html \
	cmd-line.html config.html config-notes.html low-level.html \
	porting.html

ALLSRC= $(SOURCES) for.pnuts

TXTS= index.txt introduction.txt background.txt installation.txt \
	cmd-line.txt config.txt config-notes.txt low-level.txt \
	porting.txt

HTML2TXT= lynx -underscore -nolist -dump

WWWDIR= /usr/local/wwwdev/world/pine/tech-notes

PNUTS= $(WWWDIR)/pn4tn

.SUFFIXES: .html .txt

tech-notes.txt:	$(TXTS)
	cat $(TXTS) > tech-notes.txt
	rm $(TXTS)

www:	$(ALLSRC)
	cp $(ALLSRC) $(WWWDIR)
	( cd $(WWWDIR) ; $(PNUTS) )

.html.txt:
	$(HTML2TXT) $< > $@

