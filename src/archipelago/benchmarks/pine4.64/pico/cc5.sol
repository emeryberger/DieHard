#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)cc.sh	1.8	92/05/26 SMI"	/* SVr4.0 1.4	*/

#		PROPRIETARY NOTICE (Combined)
#
#This source code is unpublished proprietary information
#constituting, or derived under license from AT&T's UNIX(r) System V.
#In addition, portions of such source code were derived from Berkeley
#4.3 BSD under license from the Regents of the University of
#California.
#
#
#
#		Copyright Notice 
#
#Notice of copyright on this source code product does not indicate 
#publication.
#
#	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
#	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
#	          All rights reserved.

# cc command for BSD compatibility package:
#
#	BSD compatibility package header files (/usr/ucbinclude)
#	are included before SVr4 default (/usr/include) files but 
#       after any directories specified on the command line via 
#	the -I option.  Thus, the BSD header files are included
#	next to last, and SVr4 header files are searched last.
#	
#	BSD compatibility package libraries (/usr/ucblib) are
#	searched next to third to last.  SVr4 default libraries 
#	(/usr/ccs/lib and /usr/lib) are searched next to last
#
#	Because the BSD compatibility package C library does not 
#	contain all the C library routines of /usr/ccs/lib/libc.a, 
#	the BSD package C library is named /usr/ucblib/libucb.a
#	and is passed explicitly to cc.  This ensures that libucb.a 
#	will be searched first for routines and that 
#	/usr/ccs/lib/libc.a will be searched afterwards for routines 
#	not found in /usr/ucblib/libucb.a.  Also because sockets is    
#       provided in libc under BSD, /usr/lib/libsocket and /usr/lib/nsl
#       are also included as default libraries.
#
#	NOTE: the -Y L, and -Y U, options of cc are not valid 

if [ "$CC" ] ; then
	$CC "$@"
	exit 0
fi

if [ -f /usr/ccs/bin/ucbcc ]
then
	# get the directory where ucbcc points to and set the LD_LIBRARY_PATH
	# to that directory so as to get the necessary libraries.
	cclink=`/usr/bin/ls -ln /usr/ccs/bin/ucbcc | awk '{print $11}'`
	ccdir=`/usr/bin/dirname $cclink`

	/usr/ccs/bin/ucbcc \
	-YP,:$ccdir:/usr/ccs/lib:/usr/lib "$@" \
	-lsocket -lnsl -lelf -laio
	ret=$?
	exit $ret
else
	echo "/usr/ucb/cc:  language optional software package not installed"
	exit 1
fi
