#!/bin/bash

# Example usage: ./run_varyload.sh varyload-remoteram 192.168.10.115:5000 192.168.10.116:5000 60 $((32*1024)) 2

config=$1
thru_target=$2
lat_target=$3
duration=$4
thru_sz=$5
thru_qd=$6
lat_sz=$((4*1024))
lat_qd=1

# Warmup
echo "Starting warmup"
./run_apps.sh $config-warmup $thru_target $lat_target $duration $thru_sz $thru_qd 6 0 "0,4,8,12,16,20";

for qd in 1 2 4 8 16 32 64; do

    echo "Starting $config-qd$qd";
    ./run_apps.sh $config-qd$qd $thru_target $lat_target $duration $thru_sz $qd 6 6 "0,4,8,12,16,20";
    sleep 10;

done
