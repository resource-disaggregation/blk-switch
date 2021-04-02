#!/bin/sh

intf=$TARGET_INF

## Enable aRFS ##

ethtool -K $intf ntuple on
if [ $? -gt 0 ]; then
echo "ERROR to enble ntuple"
# exit
fi

echo 32768 > /proc/sys/net/core/rps_sock_flow_entries
for f in /sys/class/net/$intf/queues/rx-*/rps_flow_cnt; do echo 32768 > $f; done

/usr/sbin/set_irq_affinity.sh $intf

## Network-reated configuration
service irqbalance stop
ethtool -C $intf adaptive-rx off adaptive-tx off
ethtool -K $intf tso on gso on gro on

## blk-switch configuration
echo 100000000 > /proc/sys/kernel/sched_latency_ns
