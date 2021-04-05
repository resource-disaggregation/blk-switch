#!/bin/bash

config="fig8"

prefix=results/$config
num_cores=6

echo "Queue-depth Average-latency Tail-latency Total-throughput Throughput-per-core"
for qd in 1 2 4 8; do
    if [ $(cat $prefix-qd$qd.thru*.txt | grep "iops" | wc -l) -lt $num_cores ] || [ $(cat $prefix-qd$qd.lat*.txt | grep "iops" | wc -l) -lt $(($num_cores)) ]; then
        paste <(echo $(($qd))) <(echo "[Not all apps ran. Either there was an error or system is unstable]");
    else
        tapp_xput=$(cat $prefix-qd$qd.thru*.txt | grep "iops" | awk '{x += $2} END {print x*32*1024*8/1e9;}');
        lapp_xput=$(cat $prefix-qd$qd.lat*.txt | grep "iops" | awk '{x += $2} END {print x*4*1024*8/1e9;}');
        util=$(cat $prefix-qd$qd.util | grep "Average:   " | awk -v n=$num_cores '$2 != "CPU" {sum += $8} END {print (n*100 - sum)/100;}')
        paste <(echo $(($qd))) <(cat $prefix-qd$qd.lat*.txt | grep "iops" | awk 'BEGIN{x=0;y=0} {x += $8; y += $12;}END{print x/NR,y/NR;}') <(awk -v txp=$tapp_xput -v lxp=$lapp_xput -v n=$util 'BEGIN {print (txp+lxp), (txp+lxp)/n}');
    fi
    
done
