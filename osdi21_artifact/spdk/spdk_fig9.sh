#!/bin/bash

# Example usage: ./run_varycores.sh varycores-remoteram 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 60 $((128*1024)) 8

config=$1
thru_target="$(cat config/ram_disk_addr.txt)"
lat_target="$(cat config/ram_disk_addr.txt)"
duration="$(cat config/duration.txt)"
thru_sz="$(cat config/thru_sz.txt)"
thru_qd="$(cat config/thru_qd.txt)"
lat_sz=$((4*1024))
lat_qd=1


for num_cores in 1 2 3 4 5 6; do

    echo "Starting $config-cores$num_cores";
    ./run_apps.sh $config-cores$num_cores "$thru_target" "$lat_target" $duration $thru_sz $thru_qd $num_cores $num_cores 0 $num_cores;
    sleep 5;

done
