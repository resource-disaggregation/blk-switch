#!/bin/bash

prefix=results/fig9;

# echo "lat-apps iops thru latavg lattail latiops"
# for cores in 1 2 4 8; do
#     paste <(echo $cores) <(cat $prefix-cores$cores.thru*.txt | grep "Total        " | awk '{x += $3;} END {print x, x*128*1024*8/1e9;}') <(cat $prefix-cores$cores.lat*.txt | grep "Total        " | awk 'BEGIN{x=0;} {x += $5;} END{print x/NR;}') <(cat $prefix-cores$cores.lat*.txt | grep "99.00000" | tr -d 'us' | awk 'BEGIN{x=0;} {x += $3;} END{print x/NR;}') <(cat $prefix-cores$cores.lat*.txt | grep "Total        " | awk '{x += $3;} END {print x, x*4*1024*8/1e9;}');
# done
# echo ""

echo "L-apps average-latency tail-latency";
for cores in 1 2 3 4 5 6 7 8 9 10; do
    paste <(echo $cores) <(cat $prefix-cores$cores.lat*.txt | grep "Total        " | awk 'BEGIN{x=0;} {x += $5;} END{print x/NR;}') <(cat $prefix-cores$cores.lat*.txt | grep "99.00000" | tr -d 'us' | awk 'BEGIN{x=0;} {x += $3;} END{print x/NR;}');
done
echo ""


echo "L-apps total-throughput throughput-per-core"
for cores in 1 2 3 4 5 6 7 8 9 10; do
    tapp_xput=$(cat $prefix-cores$cores.thru*.txt | grep "Total        " | awk '{x += $3;} END {print x*128*1024*8/1e9;}');
    lapp_xput=$(cat $prefix-cores$cores.lat*.txt | grep "Total        " | awk '{x += $3;} END {print x*4*1024*8/1e9;}');
    paste <(echo $cores) <(awk -v txp=$tapp_xput -v lxp=$lapp_xput -v n=$cores 'BEGIN {print (txp+lxp), (txp+lxp)/n}');
done