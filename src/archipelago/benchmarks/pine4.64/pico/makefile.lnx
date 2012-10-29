# $Id: makefile.lnx 11894 2001-10-23 22:18:36Z hubert $
#
#   Michael Seibel
#   Networks and Distributed Computing
#   Computing and Communications
#   University of Washington
#   Administration Builiding, AG-44
#   Seattle, Washington, 98195, USA
#   Internet: mikes@cac.washington.edu
#
#   Please address all bugs and comments to "pine-bugs@cac.washington.edu"
#
#
#   Pine and Pico are registered trademarks of the University of Washington.
#   No commercial use of these trademarks may be made without prior written
#   permission of the University of Washington.
#
#   Pine, Pico, and Pilot software and its included text are Copyright
#   1989-1998 by the University of Washington.
#
#   The full text of our legal notices is contained in the file called
#   CPYRIGHT, included with this distribution.
#

#
# Makefile for Linux version of the PINE composer library and 
# stand-alone editor pico.
#
#    NOTE:  We are told that it may be advantageous to use the -q flag
#           when running pico or pilot on a Linux console, and to turn on the
#           feature "termdef-takes-precedence" when running pine. That
#           feature doesn't show up in the Config screen, you need to turn
#           it on in the pine.conf file or by editing the .pinerc file.
#

RM=          rm -f
LN=          ln -s
MAKE=        make
OPTIMIZE=    # -O
PROFILE=     # -pg
DEBUG=       -g -DDEBUG

STDCFLAGS=	-Dlnx -DJOB_CONTROL -DPOSIX -DMOUSE
CFLAGS=         $(OPTIMIZE) $(PROFILE) $(DEBUG) $(EXTRACFLAGS) $(STDCFLAGS)

# switches for library building
LIBCMD=		ar
LIBARGS=	ru
RANLIB=		ranlib

LIBS=		$(EXTRALDFLAGS) -lncurses

OFILES=		attach.o basic.o bind.o browse.o buffer.o \
		composer.o display.o file.o fileio.o line.o pico_os.o \
		pico.o random.o region.o search.o \
		window.o word.o

HFILES=		headers.h estruct.h edef.h efunc.h pico.h os.h

#
# dependencies for the Unix versions of pico and libpico.a
#
all:		pico pilot
pico pilot:	libpico.a

pico:		main.o
		$(CC) $(CFLAGS) main.o libpico.a $(LIBS) -o pico

pilot:		pilot.o
		$(CC) $(CFLAGS) pilot.o libpico.a $(LIBS) -o pilot

libpico.a:	$(OFILES)
		$(LIBCMD) $(LIBARGS) libpico.a $(OFILES)
		$(RANLIB) libpico.a

clean:
		rm -f *.a *.o *~ pico_os.c os.h pico pilot
		cd osdep; $(MAKE) clean; cd ..

os.h:		osdep/os-lnx.h
		$(RM) os.h
		$(LN) osdep/os-lnx.h os.h

pico_os.c:	osdep/os-lnx.c
		$(RM) pico_os.c
		$(LN) osdep/os-lnx.c pico_os.c

$(OFILES) main.o pilot.o:	$(HFILES)
pico.o:				ebind.h

osdep/os-lnx.c:	osdep/header osdep/unix osdep/read.sel osdep/raw.io \
		osdep/spell.unx osdep/term.inf \
		osdep/os-lnx.ic
		cd osdep; $(MAKE) includer os-lnx.c; cd ..
