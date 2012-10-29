# Don't edit makefile, edit makefile.bas instead.
#

RM=  rm -f

ALL=	os-a32.c os-a41.c os-aix.c \
	os-aux.c os-bs2.c os-bsd.c os-bsf.c os-bsi.c os-bso.c \
	os-cvx.c os-dos.c os-dpx.c os-dyn.c \
	os-gen.c os-hpp.c os-isc.c os-lnx.c \
	os-lyn.c os-mnt.c os-neb.c os-nxt.c \
	os-os2.c os-osf.c os-osx.c os-pt1.c os-ptx.c \
	os-s40.c os-sco.c os-sgi.c os-sun.c \
	os-sv4.c os-ult.c os-win.c os-wnt.c \
	os-3b1.c os-att.c os-sc5.c os-nto.c

.SUFFIXES: .ic

.ic.c:
		./includer < $*.ic > $*.c

all:		includer $(ALL)

includer:	includer.c
		$(CC) -o includer includer.c

clean:
		$(RM) $(ALL) includer

# You don't have to run this unless you change a .ic file.
depend:
		./makedep

# Makedep only catches 1-level deep includes.  If something depends on a
# 2nd-level include, put it here.

