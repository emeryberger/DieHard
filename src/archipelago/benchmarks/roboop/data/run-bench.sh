#!/bin/bash

timestamp=`date +%F_%T`

#No QIH, time
#/usr/bin/time -o bench_${timestamp}_nodh.time ../bench-nodh 

#QIH, time
/usr/bin/time -o bench_${timestamp}_dh.time   ../bench-dh 

#No QIH, memory
#../bench-nodh &
#sleep 0.25
#top -b -p `cat /tmp/bench.pid` -d 0.04 -n 75 | grep bench > bench_${timestamp}_nodh.mem

#QIH, memory
../bench-dh &
sleep 0.25
top -b -p `cat /tmp/bench.pid` -d 0.10 -n 75 | grep bench > bench_${timestamp}_dh.mem

#Diehard time
#/usr/bin/time -o bench_${timestamp}_dho.time   ../bench-dho 

#Dieahrd, memory
#../bench-dho &
#sleep 0.25
#top -b -p `cat /tmp/bench.pid` -d 0.04 -n 75 | grep bench > bench_${timestamp}_dho.mem
