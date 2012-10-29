			 BUILD AND INSTALLATION NOTES
			Last Updated: 5 January 2004

Table of Contents:
1. UNIX Build Notes
2. UNIX Installation Notes
3. Win32 Build Notes
4. Win32 Installation Notes
5. Inactive Ports (TOPS-20, VMS)
6. Other ports (Macintosh, DOS/Win16, Windows CE, Amiga, OS/2)


			       UNIX BUILD NOTES

     The default build is with IPv4 only.  To build with IPv6, add "IP=6" to
the make command line, e.g.
	make lrh IP=6

     The default build is to build with SSL and disabling plaintext passwords
unless SSL/TLS encryption is in effect (SSLTYPE=nopwd).  This means that
OpenSSL MUST be installed before building the IMAP toolkit.  Please refer to
the SSLBUILD file for more information.

     To build without SSL, add "SSLTYPE=none" to the make command line.
Note that doing so will produce an IMAP server which is NON-COMPLIANT with
current IESG security requirements.

     You must build through the top-level imap-2004/Makefile, which will run
a "process" step the first time and create the imap-2004/c-client,
imap-2004/ipopd, and imap-2004/imapd directories in which building actually
takes place.

     Before doing a make on UNIX, you should read imap-2004/Makefile and see
if your system type is known.  The various system types are three-letter codes.
If your system type is known, then use this as the make option.  After the
first time you do a make, this option is remembered in a file called OSTYPE,
so just typing "make" suffices.

     For example, if you are using a more or less modern Linux system, your
system type is either slx (shadow passwords only) or lnp (PAM), and the
appropriate command is:
	make slx	(or make lnp)

     There are other make options, described in imap-2004/src/osdep/Makefile.

     It's probably best to see if an existing port will work on your system
before inventing a new port.  Try:
	sv4		generic SVR4, non-ANSI compiler
	a32		modern SVR4
	bsd		basic 4.3 BSD, non-ANSI compiler
	nxt, bsf	modern BSD

     If you must invent a new port, you need to create an entry in
imap-2004/Makefile and imap-2004/src/osdep/Makefile for your new port, as
well as osdep/os_???.h and osdep/os_???.c files with the appropriate
OS-dependent support for that system.  You also need to determine which setup
process to use.  You should use the ua process unless you are sure that your
compiler supports *ALL* aspects of ANSI C prototyping.  Note that some
compilers, such as Ultrix, support some aspects of ANSI C but not others;
c-client really beats on the full prototyping capability of ANSI C so you
have to use the non-ANSI source tree for such systems.

     If you send a new port back to us, we will make it available for others
who use your particular system type.

     The mbox driver is now enabled by default.  If the file "mbox" exists on
the user's home directory and is in UNIX mailbox format, then when INBOX is
opened this file will be selected as INBOX instead of the mail spool file.
Messages will be automatically transferred from the mail spool file into the
mbox file.  To disable this behavior, delete "mbox" from the EXTRADRIVERS list
in the top-level Makefile and rebuild.

     WARNING: The SVR2 (sv2) port is *incomplete*.  SVR2 does not appear to
have any way to do ftruncate(), which is needed by the mbox, mbx, mmdf, mtx,
tenex, and unix drivers.

			   UNIX INSTALLATION NOTES

     Binaries from the build are:
	imap-2004/mtest/mtest		c-client testbed program
	imap-2004/ipopd/ipop2d		POP2 daemon
	imap-2004/ipopd/ipop3d		POP3 daemon
	imap-2004/imapd/imapd		IMAP4rev1 daemon

     mtest is normally not used except by c-client developers.

STEP 1:	inetd setup

     The ipop2d, ipop3d, and imapd daemons should be installed in a system
daemon directory (in the following examples, /usr/local/etc is used), and
invoked by your /etc/inetd.conf file with lines such as:

pop	stream	tcp	nowait	root	/usr/local/etc/ipop2d	ipop2d
pop3	stream	tcp	nowait	root	/usr/local/etc/ipop3d	ipop3d
imap	stream	tcp	nowait	root	/usr/local/etc/imapd	imapd

     Note that different variants of UNIX have different versions of inetd,
so you should verify the precise form of these commands (for example, some
versions of inetd do not require the "nowait").

     IMPORTANT NOTE: inetd has a limit of how many new connections it will
allow in a certain interval, and when this limit is exceeded, it shuts down
the server.  If you have anything beyond a small-scale server, you are
probably going to run up against this limit.  You'll know when it happens;
your syslog will give the misleading message "imap/tcp server failing
(looping), service terminated" and users will complain that IMAP service is
unavailable for the next 10 minutes.  Similarly with "pop3/tcp server
failing"...

     It must be emphasized that this is *NOT* a bug in the IMAP or POP
servers, nor is it anything that I can "fix".  It is an inetd problem, and
the only way to fix it is to change inetd's behavior.

     By default, the parameters of this limit are (from inetd.c source code):

#define TOOMANY         40              /* don't start more than TOOMANY */
#define CNT_INTVL       60              /* servers in CNT_INTVL sec. */
#define RETRYTIME       (60*10)         /* retry after bind or server fail */

That is, no more than 40 connections (TOOMANY) in 60 seconds (CNT_INTL), and
if exceeded, shut down the server for 10 minutes (RETRYTIME).  This was a
good setting in the 1980s ARPAnet, but is much too small today.

     Some versions of inetd allow you to see a higher maximum in the
/etc/inetd.conf file.  Read "man inetd" and see if you see something like
this in the text:

     The wait/nowait entry is applicable to datagram sockets only [...]
     [...]  The optional ``max'' suffix (separated from
     ``wait'' or ``nowait'' by a dot) specifies the maximum number of server
     instances that may be spawned from inetd within an interval of 60 sec-
     onds. When omitted, ``max'' defaults to 40.

If you see this, then edit the /etc/inetd.conf entry for imapd to be
something like:

imap	stream	tcp	nowait.100	root	/usr/local/etc/imapd	imapd
 (or, if you use TCP wrappers)
imap	stream	tcp	nowait.100	root	/usr/local/etc/tcpd	imapd

     Otherwise, you'll need to edit the inetd source code to set TOOMANY to a
higher value, then rebuild inetd.


STEP 2:	services setup

     You may also have to edit your /etc/services (or Yellow Pages,
NetInfo, etc. equivalent) to register these services, such as:

pop		109/tcp
pop3		110/tcp
imap		143/tcp


STEP 3:	optional rimap setup

     If you want to enable the rimap capability, which allows users with a
suitable client and .rhosts file on the server to access IMAP services
without transmitting her password in the clear over the network, you need
to have /etc/rimapd as a link to the real copy of imapd.  Assuming you have
imapd installed on /usr/local/etc as above:
	% ln -s /usr/local/etc/imapd /etc/rimapd

     Technical note: rimap works by having the client routine tcp_aopen()
invoke `rsh _host_ exec /etc/rimapd' in an child process, and then returning
pipes to that process' standard I/O instead of a TCP socket.  You can set up
`e-mail only accounts' by making the shell be something which accepts only
that string and not ordinary UNIX shell commands.


STEP 4:	notes on privileges

     Neither user "root", not any other UID 0 account, can log in via IMAP or
POP.  "That's not a bug, it's a feature!"

     This software is designed to run without privileges.  The mail spool
directory must be protected 1777; that is, with world write and the sticky
bit.  Of course, mail *files* should be protected 600!

     An alternative to having the mail spool directory protected 1777, at the
cost of some performance, is to use the external "mlock" program, available
as part of the imap-utils package.  With mlock installed as /etc/mlock and
setgid mail, the spool directory can be protected 775 with group mail.
Please disregard this paragraph if you don't understand it COMPLETELY, and
know EXACTLY what to do without question.


STEP 5:	SVR4 specific setup

     There is one "gotcha" on System V Release 4 based systems such as
Solaris.  These systems do not use the standard UNIX mail format, but rather a
variant of that format that depends upon a bogus "Content-Length:" message
header.  This is widely recognized to have been a terrible mistake.  One
symptom of the problem is that under certain circumstances, a message may get
broken up into several messages.  I'm also aware of security bugs caused by
programs that foolishly trust "Content-Length:" headers with evil values.

    To fix your system, edit your sendmail.cf to change the Mlocal line to
have the -E flag.  A typical entry will lool like:

Mlocal, P=/usr/lib/mail.local, F=flsSDFMmnPE, S=10, R=20, A=mail.local -d $u

			      WIN32 BUILD NOTES

     Visual C++ 6.0 along with the current Microsoft Platform SDK
(specifically the CORE SDK and the Internet Development SDK) is required
to build on Windows 9x/Me/NT/2K/XP.  If you do not have the Platform SDK
installed or in your path properly, you'll get errors when building os_nt.c,
typically in env_nt.c, ssl_nt.c, ssl_w2k.c, or gss_shim.c.  You can download
the Microsoft Platform SDK from Microsoft's web site.

     There is also considerable debate about how new mail is to be snarfed.
I am currently using something that seems to work with WinSMTP.  Look at
the definition of MAILFILE in imap-2004/src/osdep/nt/mailfile.h and at the
sysinbox() function in imap-2004/src/osdep/nt/env_nt.c to see what's there
now, so you have a clue about how to hack it.

     To build under Windows 95/98/NT, connect to the imap-2004 directory
and do:
	nmake -f makefile.nt
The resulting binaries will support SSL if either schannel.dll or
security.dll is installed in Windows, using the old, undocumented, SSL
interfaces.  You can also use this to build under Me/2000/XP, but it is
not the preferred build on this platform.

     To build with MIT Kerberos support, connect to the imap-2004 directory
and do:
	nmake -f makefile.ntk
The resulting binaries will support SSL if either schannel.dll or
security.dll is installed in Windows, using the old, undocumented SSL
interfaces.  They will also support MIT Kerberos.  Note, however, that
these binaries will only run on systems which have the MIT Kerberos DLLs
installed, and will not run otherwise.

     To build under Windows Me/2000/XP, connect to the imap-2004 directory
and do:
	nmake -f makefile.w2k
The resulting binaries will support SSL and Microsoft Kerberos, using the
official, documented, Microsoft interfaces.  Note, however, that these
binaries will not run under Windows 95, Windows 98, or Windows NT4.

			   WIN32 INSTALLATION NOTES

     The resulting binaries will be:
	imap-2004\mtest\mtest.exe	(testbed client)
	imap-2004\ipopd\ipop2d.exe	POP2 server
	imap-2004\ipopd\ipop3d.exe	POP3 server
	imap-2004\imapd\imapd.exe	IMAP4rev1 server

     These servers are stdio servers.  I wrote a simple network listener
for NT called inetlisn; currently it is available as:
	ftp://ftp.cac.washington.edu/mail/nt/inetlisn.tar
To build this, use "nmake" after connecting to the inetlisn directory.
inetlisn takes two arguments, the first being the port number and the second
being the binary to run to serve a connection on that port, e.g.
	c:\bin\inetlisn 143 c:\mail_daemons\imapd

     Note that NT imapd must be started as SYSTEM in order to be recognized as
being "not logged in"; otherwise it will preauth as whatever user it is
running as which is probably not what you want.  One way to have it run as
system is to have inetlisn run by an AT command, e.g. if the time now is
2:05PM, try something like:
	AT 14:06 "c:\bin\inetlisn 143 c:\mail_daemons\imapd"

     A more advanced network lister called wsinetd is available on:
	http://wsinetd.sourceforge.net
It is based on inetlisn, and essentially is a "completed" version of inetlisn.

     Bottom line: this is not plug-and-play.  If you're not a hacker and/or
are unwilling to invest the time to do some programming, you probably want to
buy a commercial server product.

				INACTIVE PORTS

     The TOPS-20 and VMS ports were developed at one time or another, but are
no longer actively developed.  However, from time to time I test build both
of these to make sure that they compile without errors and that mtest runs,
and will continue doing so as long as I have access to systems running these
operating systems.


			     TOPS-20 BUILD NOTES

     I have provided a c-client port for TOPS-20 systems, but you're on your
own in terms of a nice TOPS-20 like main program.  Maybe someday some nice
person will try porting Pine to TOPS-20.  This assumes the use of KCC 6, and
probably will not build with other compilers or older versions of KCC.

     You do not use imap-2004/Makefile under TOPS-20, nor do you build any
components other than c-client and mtest.  Merge the contents of
imap-2004/src/c-client, imap-2004/src/charset, imap-2004/src/mtest, and
imap-2004/src/osdep/tops-20 onto a single directory on TOPS-20 and build from
that.  The command:
	DO BUILD.CTL
will build the sources.  If you don't have MIC, then SUBMIT BUILD.CTL and let
BATCON execute it.


			       VMS BUILD NOTES

      You do not use imap-2004/Makefile under VMS, nor do you build any
components other than c-client and mtest.  Merge the contents of
imap-2004/src/c-client, imap-2004/src/charset, imap-2004/src/mtest, and
imap-2004/src/osdep/vms onto a single directory on VMS and build from that.
The command to build it is:
	@BUILD MULTINET
or	@BUILD NETLIB
If you just do @BUILD it will build with dummy TCP code, and since only TCP
based drivers are provided here this isn't too useful.

     If you aren't on the Pacific coast of the US or Canada, you probably will
need to change the wired-in timezone in the BUILD.COM file.  Apparently, the
wonderful VMS system that DEC loves so much doesn't maintain any concept of
time zone; the VMS C compiler returns a null pointer from gmtime()!

     Otherwise you're pretty much on your own here.

				 OTHER PORTS

     The following ports were developed at one time or another, but are no
longer actively developed or tested.  It is not known if they still work or
not.

  Port		Status
  ----		------
Macintosh	Obsolete; Mac OS X uses UNIX port
DOS/Win16	Obsolete; modern PCs use Win32 port
Windows CE	Never completed
Amiga		Unknown
OS/2		Unknown

			     MACINTOSH BUILD NOTES

     If you are building a Macintosh client, you will need MacTCP installed on
your system as well as the MacTCP C includes and libraries.

     You do not use imap-2004/Makefile on the Mac, nor do you build any
components other than c-client and mtest.  Merge the contents of
imap-2004/src/c-client, imap-2004/src/charset, imap-2004/src/mtest, and
imap-2004/src/osdep/mac onto a single directory on the Mac and build from
that.  mtext.sit.hqx is a THINK C project file and cute icon for building
mtest, encoded with Binhex and StuffIt.

     THINK C is a truly wretched product which help make me understand why
Macintosh has lost most of its market share.  Not only does it do cretinous
things such as barf about a cast in front of an lvalue, it also limits the size
of code *or* data in a single file to 32K!  So much for having large character
set tables.  Symantec says that "MacOS requires it, break up your files into
smaller pieces" yet somehow gcc under MachTen contrives to compile C programs
without subjecting the programmer to this idiocy.

     As a result of this, I found myself obliged to comment out the #includes
of the East Asian character sets in utf8.c in order to get it to build.  It's
also necessary to break up some of the files, at least mail.c and imap4r1.c.
Maybe you don't have to do this in CodeWarrior or whatever the new compiler is
called, but I've pretty much given up on Macintosh.

     If you use precompiled headers, you may get some compilation errors since
some Apple symbols need to be redefined in order to get it to build under all
versions of MacOS.  Try turning off the precompiled headers (so it will
re-read the .h files) and see if it builds any better.

     If you use a Mac C compiler with 2-byte ints (such as THINK C's normal
mode) you will need to fix some bugs in the MacTCP C includes and libraries to
prevent it from generating bad code, since those MacTCP files violate Apple's
standards of always using explicit shorts or longs, never ints.  You could
avoid this if you set 4-byte ints in THINK C; however, the ANSI and UNIX
libraries in THINK C use 2-byte ints so you will also need to build 4-byte int
versions of these.  c-client itself is 2-byte int or 4-byte int clean; it can
be used in either mode.

     The most important bug in the MacTCP files that you need to fix is in the
file AddressXlation.h, you need to change the definition of the rtnCode member
of the hostInfo structure to be long instead of int.  There are several other
changes you need to make if you decide to compile dnr.c under THINK C instead
of using the Apple-supplied object file; see me for details if you decide to
undertake such an effort.  This is fixed in newer versions from Apple.


			     DOS/WIN16 BUILD NOTES

     If you are building a DOS client, you will need a TCP/IP stack installed
on your DOS system along with its development environment.  The currently
supported stacks are Beame & Whiteside, PC-NFS, Novell, PC/IP, Waterloo, and
Winsock.  mtest and a version of Pine called PC Pine run under DOS.

      You do not use imap-2004/Makefile under DOS, nor do you build any
components other than c-client and mtest.  Merge the contents of
imap-2004/src/c-client, imap-2004/src/charset, imap-2004/src/mtest, and
imap-2004/src/osdep/dos onto a single directory on DOS and build from that.
The MAKE command on DOS takes an argument identifying the TCP/IP stack in use.
For example, do:
	MAKE MAKEFILE OS=WSK   (or MAKE -F MAKEFILE OS=WSK)
to build for Winsock.  

     If you write a program for DOS/Win16, you will probably have to write a
replacement cache manager (look at mm_cache()) and otherwise disable most of
c-client's caching.  Even so, memory limitations will be an ongoing problem,
particularly with DOS, and you will have some severe performance problems.
It's a bit better on Win16, but in my opinion you are better off writing a
32-bit program and telling your Win16 customers to upgrade to Windows 95 or at
least install Win32s.


			    WINDOWS CE BUILD NOTES

     I build using Visual C++ 6.0 with the WCE extensions.  The current code
has SH3 wired in for the compiler building.

     To build under NT, connect to the imap-2004 directory and do:
	nmake -f makefile.wce

     The only binary produced is a cclient.lib file.  I haven't gotten as far
as building mtest on WCE, mainly because I don't have a stdlib library.


		      AMIGA BUILD AND INSTALLATION NOTES

     The Amiga port was contributed.  Maybe the UNIX notes will help.


			       OS2 BUILD NOTES

     The OS2 port was contributed.  Maybe the Win32 Build Notes will help.
