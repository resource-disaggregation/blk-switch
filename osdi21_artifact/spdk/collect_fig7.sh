#!/bin/bash

prefix=results/fig7

echo "lat-apps iops thru latavg lattail latiops"
for num_lat in 1 2 4 8; do
    paste <(echo $num_lat) <(cat $prefix-lat$num_lat.thru*.txt | grep "Total        " | awk '{x += $3;} END {print x, x*128*1024*8/1e9;}') <(cat $prefix-lat$num_lat.lat*.txt | grep "Total        " | awk 'BEGIN{x=0;} {x += $5;} END{print x/NR;}') <(cat $prefix-lat$num_lat.lat*.txt | grep "99.00000" | tr -d 'us' | awk 'BEGIN{x=0;} {x += $3;} END{print x/NR;}') <(cat $prefix-lat$num_lat.lat*.txt | grep "Total        " | awk '{x += $3;} END {print x, x*4*1024*8/1e9;}');
done