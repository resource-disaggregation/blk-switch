#!/bin/bash

# Example usage: ./run_varylatapps.sh singlecore-remoteram 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 60 $((128*1024)) 8

config="varynice"
thru_target="$(cat config/ram_disk_addr.txt)"
lat_target="$(cat config/ram_disk_addr.txt)"
duration="$(cat config/duration.txt)"
thru_sz="$(cat config/thru_sz.txt)"
thru_qd="$(cat config/thru_qd.txt)"
lat_sz=$((4*1024))
lat_qd=1


for lat_nice in 0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -13 -14 -15 -16 -17 -18 -19 -20; do

    echo "Starting $config-nice$lat_nice";
    ./run_apps.sh $config-nice$lat_nice "$thru_target" "$lat_target" $duration $thru_sz $thru_qd 1 1 0 1 100 $lat_nice;
    sleep 5;

done
