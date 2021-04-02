#!/bin/bash
source system_env.sh

###########################################################################
# Target-side configurations
###########################################################################
target_ip=$TARGET_IP
nr_cores=$NR_CORES

# Create null block devices (10GB)
# Use number of cores of the system for "submit-queues" parameter
modprobe null-blk gb=10 bs=4096 irqmode=1 hw_queue_depth=1024 submit-queues=$nr_cores

# Load i10 and nvme-tcp target kernel modules
modprobe nvmet nvmet-tcp i10-target


# Create nvme-tcp subsystem
# - subsystem name: nvme_null
# - device: /dev/nullb0
# - protocol: tcp
# - listening port: 4425
mkdir /sys/kernel/config/nvmet/subsystems/nvme_null
cd /sys/kernel/config/nvmet/subsystems/nvme_null

echo 1 > attr_allow_any_host
mkdir namespaces/10
cd namespaces/10
echo -n /dev/nullb0 > device_path
echo 1 > enable

mkdir /sys/kernel/config/nvmet/ports/5
cd /sys/kernel/config/nvmet/ports/5
echo $target_ip > addr_traddr
echo tcp > addr_trtype
echo 4425 > addr_trsvcid
echo ipv4 > addr_adrfam

ln -s /sys/kernel/config/nvmet/subsystems/nvme_null /sys/kernel/config/nvmet/ports/5/subsystems/nvme_null


# Create i10_null subsystem
# - subsystem name: i10_null
# - device: /dev/nullb0
# - protocol: i10
# - listening port: 5620
mkdir /sys/kernel/config/nvmet/subsystems/i10_null
cd /sys/kernel/config/nvmet/subsystems/i10_null

echo 1 > attr_allow_any_host
mkdir namespaces/10
cd namespaces/10
echo -n /dev/nullb0 > device_path
echo 1 > enable

mkdir /sys/kernel/config/nvmet/ports/51
cd /sys/kernel/config/nvmet/ports/51
echo $target_ip > addr_traddr
echo i10 > addr_trtype
echo 5620 > addr_trsvcid
echo ipv4 > addr_adrfam

ln -s /sys/kernel/config/nvmet/subsystems/i10_null /sys/kernel/config/nvmet/ports/51/subsystems/i10_null

