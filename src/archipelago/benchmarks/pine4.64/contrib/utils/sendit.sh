#! /bin/sh
#
# $Id:$
#
#            T H E    P I N E    M A I L   S Y S T E M
#
#   Mike Seibel
#   Networks and Distributed Computing
#   Computing and Communications
#   University of Washington
#   Administration Building, AG-44
#   Seattle, Washington, 98195, USA
#   Internet: mikes@CAC.Washington.EDU
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
#  Simple script to expedite mail posting at the expense of timely
#  error reporting and 8BITMIME support.
#
# NOTE: If for any reason POSTFILE below is created on an nfs mounted
#       file system, the trap statement below must get removed or
#       altered, and the last line must get replaced with:
#
#       ( ${POSTTOOL} ${POSTARGS} < ${POSTFILE} ; rm -f ${POSTFILE} ) &
#

POSTFILE=/tmp/send$$
POSTTOOL=/usr/lib/sendmail
POSTARGS="-oi -oem -t"

umask 077
trap "rm -f ${POSTFILE}; exit 0" 0 1 2 13 15

cat > ${POSTFILE}
${POSTTOOL} ${POSTARGS} < ${POSTFILE} & 
