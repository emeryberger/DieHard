$! VMSBUILD.COM - Calls the subdirectories VMSBUILD.
$! Can be called with options separated with commas and no spaces. The
$! options are:
$!
$! NETLIB - For linking with Netlib. NETLIB.OLB must be pre-loaded into
$!          [.NETLIb]NETLIB.OLB
$! MULTINET - Used with Multinet transport (no NETLIB is used).
$! HEBREW - Build the Hebrew version.
$!
$!
$ IF F$EXISTS("[.IMAP.ANSI]C-CLIENT.DIR") THEN RENAME [.IMAP.ANSI]C-CLIENT.DIR [];
$ IF F$EXISTS("[.CONTRIB.VMS]VMSBUILD_CCLIENT.COM") THEN RENAME [.CONTRIB.VMS]VMSBUILD_CCLIENT.COM [.C-CLIENT]VMSBUILD.COM;
$ SET DEF [.PICO]
$@VMSBUILD 'P1'
$ SET DEF [-.C-CLIENT]
$@VMSBUILD 'P1'
$ SET DEF [-.PINE]
$@VMSBUILD 'P1'
$ SET DEF [-]
$!
$ TYPE SYS$INPUT:
Executables can be found in:

PICO/PICO.EXE - The stand-alone editor (not needed).
C-CLIENT/MTEST.EXE - Interactive testing program (not needed).
C-CLIENT/IMAPD.EXE - the IMAP daemon. Should be copied elsewhere and defined
                     in the INETD.CONF file or equivalent.
PINE/PINE.EXE - What you waited for...

$!
$ EXIT
