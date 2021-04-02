#!/bin/bash

core_bitmap=$1
ip_addr=$2
ssd_addr=${3-none}


echo "SSD: $ssd_addr";

function cleanup() {
    killall nvmf_tgt;
    echo "Cleaned up";
}

trap cleanup EXIT

function arr_to_str() {
    data=$1
    printf -v joined '%s,' "${data[@]}";
    echo "${joined%,}";
}

cleanup;

build/bin/nvmf_tgt -m $core_bitmap &

# Null bdev
scripts/rpc.py bdev_null_create Null0 $((16777216)) 4096
scripts/rpc.py bdev_null_create Null1 $((16777216)) 4096

# NVMe bdev
if [ "$ssd_addr" != "none" ]; then
  scripts/rpc.py construct_nvme_bdev -t PCIe -b Nvme0 -a $ssd_addr
fi

scripts/rpc.py nvmf_create_transport -t TCP -c $((8*1024)) -n 2047

if [ "$ssd_addr" != "none" ]; then
    scripts/rpc.py nvmf_create_subsystem nqn.2020-07.com.midhul:nvme0 -a -s SPDK00000000000001 -d SPDK_Controller1
    scripts/rpc.py nvmf_subsystem_add_ns nqn.2020-07.com.midhul:nvme0 Nvme0n1
    scripts/rpc.py nvmf_subsystem_add_listener nqn.2020-07.com.midhul:nvme0 -t TCP -s 4420 -a $ip_addr
fi

scripts/rpc.py nvmf_create_subsystem nqn.2020-07.com.midhul:null0 -a -s SPDK00000000000003 -d SPDK_Controller3
scripts/rpc.py nvmf_subsystem_add_ns nqn.2020-07.com.midhul:null0 Null0
scripts/rpc.py nvmf_subsystem_add_listener nqn.2020-07.com.midhul:null0 -t TCP -s 4420 -a $ip_addr

echo "Target running"
echo "Run sudo ./stop_target.sh to stop it"

