#!/bin/bash

# Example usage: ./run_varylatapps.sh singlecore-remoteram 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 60 $((128*1024)) 8

config="fig7"
thru_target="$(cat config/ram_disk_addr.txt)"
lat_target="$(cat config/ram_disk_addr.txt)"
duration="$(cat config/duration.txt)"
thru_sz="$(cat config/thru_sz.txt)"
thru_qd="$(cat config/thru_qd.txt)"
lat_sz=$((4*1024))
lat_qd=1


for num_lat in 1 2 4; do

    echo "Starting $config-lat$num_lat";
    ./run_apps.sh $config-lat$num_lat "$thru_target" "$lat_target" $duration $thru_sz $thru_qd 6 $((6*$num_lat)) 0 6;
    sleep 5;

done
