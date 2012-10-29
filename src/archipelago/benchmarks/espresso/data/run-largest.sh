#!/bin/bash

INPUT="-q -t ../Input/largest.espresso"

if [[ $1 == -d ]] ; then
    DIR=$2
    shift;shift
else
    DIR=.
fi

if (( $# > 0 )) ; then 
    _prefix=_${1}
else
    _prefix=sparse
fi

timestamp=`date +%F_%T`

#No QIH, runtime
for i in `seq 1 10`
do
  /usr/bin/time -o ${DIR}/largest_native_${timestamp}_${i}.time ../espresso-native ${INPUT}
done

#QIH, runtime
for i in `seq 1 10`
do
  /usr/bin/time -o ${DIR}/largest${_prefix}_${timestamp}_${i}.time ../espresso-qih ${INPUT}
done

# DieHard, runtime
#for i in `seq 1 10`
#do
#  /usr/bin/time -o ${DIR}/largest_diehard_${timestamp}_${i}.time    
#../espresso-dh ${INPUT}
#done


