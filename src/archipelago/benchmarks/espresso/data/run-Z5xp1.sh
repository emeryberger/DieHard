#!/bin/bash

#../espresso-nodh -t ../Input/Z5xp1.espresso > Z5xp1_nodh.out
/usr/bin/time -o Z5xp1_nodh.time ../espresso-nodh -t ../Input/Z5xp1.espresso > /dev/null
#../espresso-dh -t ../Input/Z5xp1.espresso > Z5xp1_dh.out
/usr/bin/time -o Z5xp1_dh.time ../espresso-dh -t ../Input/Z5xp1.espresso > /dev/null
#/usr/bin/time -o Z5xp1_noop.time ../espresso-noop -t 
#../Input/Z5xp1.espresso > /dev/null
