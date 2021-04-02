#!/bin/bash

sysctl -w net.core.rmem_max=268435456
sysctl -w net.core.wmem_max=268435456 
sysctl -w net.ipv4.tcp_rmem="4096 87380 134217728"
sysctl -w net.ipv4.tcp_wmem="4096 65536 134217728"
echo 0 | tee /proc/sys/kernel/sched_autogroup_enabled

spdk/scripts/setup.sh