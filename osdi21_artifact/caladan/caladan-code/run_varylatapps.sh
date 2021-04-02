#!/bin/bash

# Example usage: ./run_varylatapps.sh varylatapps-remoteram 192.168.10.115:5000 192.168.10.116:5000 60 $((32*1024)) 2

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

for num_lat in 1 2 4 8; do

    echo "Starting $config-lat$num_lat";
    ./run_apps.sh $config-lat$num_lat $thru_target $lat_target $duration $thru_sz $thru_qd 6 $((6*$num_lat)) "0,4,8,12,16,20";
    sleep 10;

done
