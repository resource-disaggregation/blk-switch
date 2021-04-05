#!/bin/bash

config="fig11"

prefix=results/$config
num_cores=6

echo "Queue-depth Average-latency Tail-latency Total-throughput Throughput-per-core"
for rw in 0 25 50 75 100; do
    if [ $(cat $prefix-rw$rw.thru*.txt | grep "iops" | wc -l) -lt $num_cores ] || [ $(cat $prefix-rw$rw.lat*.txt | grep "iops" | wc -l) -lt $(($num_cores)) ]; then
        paste <(echo $(($rw))) <(echo "[Not all apps ran. Either there was an error or system is unstable]");
    else
        tapp_xput=$(cat $prefix-rw$rw.thru*.txt | grep "iops" | awk '{x += $2} END {print x*32*1024*8/1e9;}');
        lapp_xput=$(cat $prefix-rw$rw.lat*.txt | grep "iops" | awk '{x += $2} END {print x*4*1024*8/1e9;}');
        util=$(cat $prefix-rw$rw.util | grep "Average:   " | awk -v n=$num_cores '$2 != "CPU" {sum += $8} END {print (n*100 - sum)/100;}')
        paste <(echo $(($rw))) <(cat $prefix-rw$rw.lat*.txt | grep "iops" | awk 'BEGIN{x=0;y=0} {x += $8; y += $12;}END{print x/NR,y/NR;}') <(awk -v txp=$tapp_xput -v lxp=$lapp_xput -v n=$util 'BEGIN {print (txp+lxp), (txp+lxp)/n}');
    fi
    
done
