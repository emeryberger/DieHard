#!/bin/sh
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
#   Copyright 1989, 1990, 1991, 1992  University of Washington
#
#    Permission to use, copy, modify, and distribute this software and its
#   documentation for any purpose and without fee to the University of
#   Washington is hereby granted, provided that the above copyright notice
#   appears in all copies and that both the above copyright notice and this
#   permission notice appear in supporting documentation, and that the name of
#   the University of Washington not be used in advertising or publicity
#   pertaining to distribution of the software without specific, written prior
#   permission.  This software is made available "as is", and
#   THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
#   WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
#   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
#   NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
#   INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
#   LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
#   (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
#   WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#  
#
#   Pine is in part based on The Elm Mail System:
#    ***********************************************************************
#    *  The Elm Mail System  -  $Revision: 2.13 $   $State: Exp $          *
#    *                                                                     *
#    * 			Copyright (c) 1986, 1987 Dave Taylor              *
#    * 			Copyright (c) 1988, 1989 USENET Community Trust   *
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
