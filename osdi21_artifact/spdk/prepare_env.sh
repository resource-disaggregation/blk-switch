#!/bin/bash

sysctl -w net.core.rmem_max=268435456
sysctl -w net.core.wmem_max=268435456 
sysctl -w net.ipv4.tcp_rmem="4096 87380 134217728"
sysctl -w net.ipv4.tcp_wmem="4096 65536 134217728"
echo 0 | tee /proc/sys/kernel/sched_autogroup_enabled

spdk/scripts/setup.sh

source config.sh
echo -n "trtype:TCP traddr:$IP_ADDR adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0" > config/ram_disk_addr.txt
echo -n "trtype:TCP traddr:$IP_ADDR adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:nvme0" > config/ssd_addr.txt

echo "Configuration complete"