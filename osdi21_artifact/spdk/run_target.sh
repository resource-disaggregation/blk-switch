#!/bin/bash

core_bitmap=$1
ip_addr=$2
ssd_addr=${3-none}


echo "SSD: $ssd_addr";

function cleanup() {
    killall spdk/build/bin/nvmf_tgt;
    echo "Cleaned up";
}


function arr_to_str() {
    data=$1
    printf -v joined '%s,' "${data[@]}";
    echo "${joined%,}";
}

cleanup;

spdk/build/bin/nvmf_tgt -m $core_bitmap &

echo "Waiting for target to start";

sleep 10; 

# Null bdev
spdk/scripts/rpc.py bdev_null_create Null0 $((16777216)) 4096
spdk/scripts/rpc.py bdev_null_create Null1 $((16777216)) 4096

# NVMe bdev
if [ "$ssd_addr" != "none" ]; then
  spdk/scripts/rpc.py construct_nvme_bdev -t PCIe -b Nvme0 -a $ssd_addr
fi

spdk/scripts/rpc.py nvmf_create_transport -t TCP -c $((8*1024)) -n 2047

if [ "$ssd_addr" != "none" ]; then
    spdk/scripts/rpc.py nvmf_create_subsystem nqn.2020-07.com.midhul:nvme0 -a -s SPDK00000000000001 -d SPDK_Controller1
    spdk/scripts/rpc.py nvmf_subsystem_add_ns nqn.2020-07.com.midhul:nvme0 Nvme0n1
    spdk/scripts/rpc.py nvmf_subsystem_add_listener nqn.2020-07.com.midhul:nvme0 -t TCP -s 4420 -a $ip_addr
fi

spdk/scripts/rpc.py nvmf_create_subsystem nqn.2020-07.com.midhul:null0 -a -s SPDK00000000000003 -d SPDK_Controller3
spdk/scripts/rpc.py nvmf_subsystem_add_ns nqn.2020-07.com.midhul:null0 Null0
spdk/scripts/rpc.py nvmf_subsystem_add_listener nqn.2020-07.com.midhul:null0 -t TCP -s 4420 -a $ip_addr

echo "Target running"
echo "Run sudo ./stop_target.sh to stop it"

