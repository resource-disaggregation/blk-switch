#!/bin/bash

# Example usage: ./run_varyload.sh varyload-remoteram 192.168.10.115:5000 192.168.10.116:5000 60 $((32*1024)) 2

config="fig11"
thru_target="192.168.10.115:5000"
lat_target="192.168.10.116:5000"
duration=60
thru_sz=$((32*1024))
thru_qd=2
lat_sz=$((4*1024))
lat_qd=1
num_use_cores=6

# Warmup
echo "Starting warmup"
./run_apps.sh $config-warmup $thru_target $lat_target $duration $thru_sz $thru_qd 6 0 $num_use_cores;

for rw in 0 25 50 75 100; do

    echo "Starting $config-rw$rw";
    ./run_apps.sh $config-rw$rw $thru_target $lat_target $duration $thru_sz $thru_qd 6 6 $num_use_cores $rw;
    sleep 10;

done
