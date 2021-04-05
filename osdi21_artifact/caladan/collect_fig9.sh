#!/bin/bash

config="fig9"

prefix=results/$config

echo "Cores Average-latency Tail-latency Total-throughput Throughput-per-core"
for num_cores in 3 4 5 6 7 8 9 10; do
    if [ $(cat $prefix-cores$num_cores.thru*.txt | grep "iops" | wc -l) -lt $num_cores ] || [ $(cat $prefix-cores$num_cores.lat*.txt | grep "iops" | wc -l) -lt $(($num_cores)) ]; then
        paste <(echo $(($num_cores))) <(echo "[Not all apps ran. Either there was an error or system is unstable]");
    else
        tapp_xput=$(cat $prefix-cores$num_cores.thru*.txt | grep "iops" | awk '{x += $2} END {print x*32*1024*8/1e9;}');
        lapp_xput=$(cat $prefix-cores$num_cores.lat*.txt | grep "iops" | awk '{x += $2} END {print x*4*1024*8/1e9;}');
        util=$(cat $prefix-cores$num_cores.util | grep "Average:   " | awk -v n=$num_cores '$2 != "CPU" {sum += $8} END {print (n*100 - sum)/100;}')
        paste <(echo $(($num_cores))) <(cat $prefix-cores$num_cores.lat*.txt | grep "iops" | awk 'BEGIN{x=0;y=0} {x += $8; y += $12;}END{print x/NR,y/NR;}') <(awk -v txp=$tapp_xput -v lxp=$lapp_xput -v n=$util 'BEGIN {print (txp+lxp), (txp+lxp)/n}');
    fi
    
done
