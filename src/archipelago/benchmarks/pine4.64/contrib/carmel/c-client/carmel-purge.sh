#!/bin/csh -f
# vmail.purge - zaps all unindexed mail messages.
#
# The_Epoch             vick created this.
# 1988 Nov 10 Thu 12:00 bob added code to allow a pathname for $1.
# 1988 Nov 14 Mon 10:14 bob rewrote the code that was breaking inside `quotes`.
# 1988 Nov 15 Tue 16:44 bob added code to track spurious .vmail files
# 1988 Nov 16 Wed 10:44 bob redirected error messages to stderr.
# 1989 Feb 14 Tue 13:37 bob add exceptions for corrupt indexes
# 1991 Sun Nov 24 10:30 Shoa add removal of RECOVER index.
# 1992 Oct 21 Wed 17:01 bob check for Pine read lock files, and abort.

#
# Usage: vmail.purge [ username | somepath/username/.vmail ]


set pname = $0
set pname = ${pname:t}	    # name of this script for error messages

set indexed =	/tmp/VMprg_idx.$$	# a list of indexed messages
set messages =	/tmp/VMprg_msg.$$	# a list of all messages
set unindexed = /tmp/VMprg_Csh.$$	# script to remove unindexed messages

if ( 1 <= $#argv ) then
    if ( ".vmail" == $1:t ) then	# assume /u/dp/bob/.vmail
        set vpath = $1:h		# /u/dp/bob
	setenv USER $vpath:t		# bob
    else
        setenv USER $1
    endif
endif

if ( ! -d ~$USER/.vmail ) then
    echo -n "${pname}: "
    if ( 1 <= $#argv ) then
    	sh -c "echo 1>&2  ${pname}: invalid argument $1"
    else
    	sh -c "echo 1>&2  ${pname}: $USER has no .vmail directory"
    endif
    exit 1
endif

cd ~$USER/.vmail

if (!( -d index && -d msg)) then
    sh -c "echo 1>&2 ${pname}: $cwd is missing required components"
    exit 1
endif
#
# check for Pine inbox read lock file
if ( -e index/.MAIL.rl ) then
    sh -c "echo 1>&2 ${pname}: $cwd is now running Pine"
    exit 1
endif
#
#
#remove the temporary RECOVER index *must* be done before deletion of messages
# start.
if ( -e index/RECOVER ) then
   rm index/RECOVER
   if ( -e  ~$USER/.inda ) rm ~$USER/.inda
   if ( -e  ~$USER/.indf ) rm ~$USER/.indf
endif
#
#
# create the shell script
#
awk '$1 ~ /^[0-9]/ {print  substr($1,1,6)}' index/* | sort | uniq > $indexed
if ( $status ) then
    	sh -c "echo 1>&2  ${pname}: some index probably corrupt"
	exit 1
endif

ls msg | awk '{ print substr($1,1,6) }' > $messages
comm -23 $messages $indexed | awk '{ print "rm -f " $1 " " $1 ".wid" }' > $unindexed

# provide verbose statistics
#
echo $USER " total:" `wc -l < $messages` " indexed:" `wc -l < $indexed`\
  " purging: " `wc -l < $unindexed`

# do the work
#
cd msg
csh -f $unindexed

# cleanup
#
rm $messages $indexed $unindexed
