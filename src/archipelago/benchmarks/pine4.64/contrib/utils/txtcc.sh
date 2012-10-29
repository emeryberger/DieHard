#!/bin/sh

# On ultrix, this will compile string constants into the text segment
# where they can be shared.  Use to build pine by saying:
#
#	build CC=txtcc gul
#
# As of Pine4.03, this moves about 675k per process into sharable memory
#
# Corey Satten, corey@cac.washington.edu, 9/11/98

TMP1=/tmp/cc$$.s
TMP2=/tmp/cc$$.delayed
trap "rm -f $TMP1 $TMP2" 0 1 2 13 15


for i in "$@"; do
    case "$oflag//$i" in
	 //-c)  cflag=1;;
	 //-o)  oflag=1;;
	 //-g)	gflag=1;;
	1//*)   ofile="$i"; oflag=;;
	 //*.c) cfile="$i"; : ${ofile=`basename $i .c`.o};;
    esac
done
    
case "$cflag" in
     1) sfile=`basename $ofile .o`.s
	gcc -S "$@"

	# Postprocess assembly language to move strings to .text segment.
	#
	# Use sed to collect .ascii statements and their preceding label
	# and .align and emit those to $TMP2, the rest to $TMP1.
	#
	# Start in state0
	# In state0:  if .align is seen  -> state1; else print, -> state0
	# In state1:  if a label is seen -> state2; else -> punt
	# In state2:  if .ascii is seen  -> state3; else -> punt
	# In state3:  if .ascii is seen  -> state4; else write TMP2, -> state0
	# In state4:  append to buffer,  -> state3
	# In state punt:  print unprinted lines, -> state0
	# 
	sed '
	    : state0
	    /^[ 	][ 	]*\.align/ b state1
	    b

	    : state1
	    h
	    N; s/^[^\n]*\n//; H
	    /^[!-~]*:/ b state2
	    b punt

	    : state2
	    N; s/^[^\n]*\n//; H
	    /^[ 	][ 	]*\.ascii/ b state3
	    b punt

	    : state3
	    N; s/^[^\n]*\n//
	    /^[ 	][ 	]*\.ascii/ b state4
	    x
	    w '"$TMP2"'
	    x
	    b state0

	    : state4
	    H
	    b state3

	    : punt
	    x
	    ' $sfile > $TMP1

	# now add the deferred .ascii statements to .text segment in $TMP1
	echo '	.text' >> $TMP1
	cat $TMP2 >> $TMP1

	rm `basename $ofile .o`.s
	gcc ${gflag+"-g"} -c -o $ofile $TMP1
	;;
    *)  gcc "$@"
	;;
esac
