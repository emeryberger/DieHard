#!/bin/sh
#
# $Id: brk2pine.sh 5466 1996-03-15 07:17:22Z hubert $
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


#
# A filter to convert personal mail aliases in a .mailrc file into
# pine address book format.
#
# Usage: program [.mailrc] >> .addressbook
#
# Corey Satten, corey@cac.washington.edu, 9/25/91
#
sed -n '
# first fold continued lines (ending in \) into a single long line
    /\\[ 	]*$/ {
	    : more
	    s/\\//g
	    N
	    s/\n/ /
	    /\\/b more
	    }
# next convert all sequences of whitespace into single space
    s/[ 	][ 	]*/ /g
# finally, reformat and print lines containing alias as the first word
    /^ *alias / {
	    s/^ *alias \([!-~][!-~]*\) \(.*\)$/\1	\1	(\2)/
	    s/ /,/g
	    s/(\([^,]*\))/\1/
	    p
	    }
' ${*-$HOME/.mailrc}
