#!/bin/bash
# This script uses SpecFileCreator.java to generate the
# directories and files used in the benchmarking tests
# note that only 10 directories and sets of files are 
# actually created, the rest are mere symlinks
# Written By: Matt Meehan
# Created On: Oct 30 2006
#

CREATE_SPEC_FILE="java SpecFileCreator"
MAKE_DIR="mkdir -p"
LINK="ln -fs "

if (( $# < 1 ))
then
    echo "Usage: working_set <destination_dir>"
    exit
fi

for i in {0,1,2,3,4,5,6,7,8,9}
do
 ${MAKE_DIR} -p ${1}/dir0000$i/
 ${CREATE_SPEC_FILE} ${1}/dir0000$i
done

cd ${1}

for i in {0,1,2,3,4,5,6}{0,1,2,3,4,5,6,7,8,9}{0,1,2,3,4,5,6,7,8,9}
do
  if [ $i -gt 9 ]
  then 
    ${LINK} dir0000${i:2:1} dir00$i
  fi
done
