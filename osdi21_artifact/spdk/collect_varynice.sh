#!/bin/bash

prefix=results/varynice;
num_cores=1;

# echo "lat-apps iops thru latavg lattail latiops"
# for num_lat in 1 2 4 8; do
#     paste <(echo $num_lat) <(cat $prefix-nice$lat_nice.thru*.txt | grep "Total        " | awk '{x += $3;} END {print x, x*128*1024*8/1e9;}') <(cat $prefix-nice$lat_nice.lat*.txt | grep "Total        " | awk 'BEGIN{x=0;} {x += $5;} END{print x/NR;}') <(cat $prefix-nice$lat_nice.lat*.txt | grep "99.00000" | tr -d 'us' | awk 'BEGIN{x=0;} {x += $3;} END{print x/NR;}') <(cat $prefix-nice$lat_nice.lat*.txt | grep "Total        " | awk '{x += $3;} END {print x, x*4*1024*8/1e9;}');
# done
# echo ""

echo "L-apps average-latency tail-latency";
for lat_nice in 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 -17 -18 -19 -20; do
    paste <(echo $lat_nice) <(cat $prefix-nice$lat_nice.lat*.txt | grep "Total        " | awk 'BEGIN{x=0;} {x += $5;} END{print x/NR;}') <(cat $prefix-nice$lat_nice.lat*.txt | grep "99.00000" | tr -d 'us' | awk 'BEGIN{x=0;} {x += $3;} END{print x/NR;}');
done
echo ""


echo "T-apps total-throughput throughput-per-core"
for lat_nice in 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 -17 -18 -19 -20; do
    tapp_xput=$(cat $prefix-nice$lat_nice.thru*.txt | grep "Total        " | awk '{x += $3;} END {print x*128*1024*8/1e9;}');
    lapp_xput=$(cat $prefix-nice$lat_nice.lat*.txt | grep "Total        " | awk '{x += $3;} END {print x*4*1024*8/1e9;}');
    paste <(echo $lat_nice) <(awk -v txp=$tapp_xput -v lxp=$lapp_xput -v n=$num_cores 'BEGIN {print (txp), (txp)/n}');
done