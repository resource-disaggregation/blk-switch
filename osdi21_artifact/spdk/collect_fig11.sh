#!/bin/bash

prefix=results/fig11;
num_cores=6;

# echo "lat-apps iops thru latavg lattail latiops"
# for rw in 1 2 4 8; do
#     paste <(echo $rw) <(cat $prefix-rw$rw.thru*.txt | grep "Total        " | awk '{x += $3;} END {print x, x*128*1024*8/1e9;}') <(cat $prefix-rw$rw.lat*.txt | grep "Total        " | awk 'BEGIN{x=0;} {x += $5;} END{print x/NR;}') <(cat $prefix-rw$rw.lat*.txt | grep "99.00000" | tr -d 'us' | awk 'BEGIN{x=0;} {x += $3;} END{print x/NR;}') <(cat $prefix-rw$rw.lat*.txt | grep "Total        " | awk '{x += $3;} END {print x, x*4*1024*8/1e9;}');
# done
# echo ""

echo "L-apps average-latency tail-latency";
for rw in 0 25 50 75 100; do
    paste <(echo $rw) <(cat $prefix-rw$rw.lat*.txt | grep "Total        " | awk 'BEGIN{x=0;} {x += $5;} END{print x/NR;}') <(cat $prefix-rw$rw.lat*.txt | grep "99.00000" | tr -d 'us' | awk 'BEGIN{x=0;} {x += $3;} END{print x/NR;}');
done
echo ""


echo "L-apps total-throughput throughput-per-core"
for rw in 0 25 50 75 100; do
    tapp_xput=$(cat $prefix-rw$rw.thru*.txt | grep "Total        " | awk '{x += $3;} END {print x*128*1024*8/1e9;}');
    lapp_xput=$(cat $prefix-rw$rw.lat*.txt | grep "Total        " | awk '{x += $3;} END {print x*4*1024*8/1e9;}');
    paste <(echo $rw) <(awk -v txp=$tapp_xput -v lxp=$lapp_xput -v n=$num_cores 'BEGIN {print (txp+lxp), (txp+lxp)/n}');
done