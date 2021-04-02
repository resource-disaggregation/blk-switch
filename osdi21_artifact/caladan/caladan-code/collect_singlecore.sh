#!/bin/bash

config=$1

prefix=~/shenango-eval/$config

echo "lat-apps iops latavg lattail hostcpu"
for num_lat in 1 2 4 8; do
    paste <(echo $num_lat) <(cat $prefix-lat$num_lat.thru1.txt | grep "iops" | awk '{print $2}') <(cat $prefix-lat$num_lat.lat*.txt | grep "iops" | awk 'BEGIN{x=0;y=0} {x += $8; y += $12;}END{print x/NR,y/NR;}') <(cat $prefix-lat$num_lat.util | grep "Average:          8" | awk '{print $8}');
done
