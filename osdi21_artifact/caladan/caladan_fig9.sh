#!/bin/bash

# Example usage: ./run_varycores.sh varycores-remoteram 192.168.10.115:5000 192.168.10.116:5000 60 $((32*1024)) 2

config="fig9"
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

for num_cores in 3 4 5 6 7 8 9 10; do

    echo "Starting $config-cores$num_cores";
    echo $(core_list $num_cores);
    ./run_apps.sh $config-cores$num_cores $thru_target $lat_target $duration $thru_sz $thru_qd $num_cores $num_cores $num_cores;
    sleep 10;

done
