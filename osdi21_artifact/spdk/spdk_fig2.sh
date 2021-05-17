#!/bin/bash

# Example usage: ./run_varylatapps.sh singlecore-remoteram 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 60 $((128*1024)) 8

config="fig2"
thru_target="$(cat config/ram_disk_addr.txt)"
lat_target="$(cat config/ram_disk_addr.txt)"
duration="$(cat config/duration.txt)"
thru_sz="$(cat config/thru_sz.txt)"
thru_qd="$(cat config/thru_qd.txt)"
lat_sz=$((4*1024))
lat_qd=1

# isolated L-app
echo "Starting $config-lat0 (isolated L-app)";
./run_apps.sh $config-lat0 "$thru_target" "$lat_target" $duration $thru_sz $thru_qd 0 1 0 1;
sleep 5;

# isolated T-app
echo "Starting $config-lat0 (isolated T-app)";
./run_apps.sh $config-lat0 "$thru_target" "$lat_target" $duration $thru_sz $thru_qd 1 0 0 1;
sleep 5;

for num_lat in 1 2 4; do

    echo "Starting $config-lat$num_lat";
    ./run_apps.sh $config-lat$num_lat "$thru_target" "$lat_target" $duration $thru_sz $thru_qd 1 $num_lat 0 1;
    sleep 5;

done
