#!/bin/bash

# Example usage: ./run_varycores.sh varycores-remoteram 192.168.10.115:5000 192.168.10.116:5000 60 $((32*1024)) 2

config=$1
thru_target=$2
lat_target=$3
duration=$4
thru_sz=$5
thru_qd=$6
lat_sz=$((4*1024))
lat_qd=1
use_cores="0,4,8,12,16,20";

function arr_to_str() {
    data=("$@")
    printf -v joined '%s,' "${data[@]}";
    echo "${joined%,}";
}

function core_list() {
    ncores=$1
    core_arr=()
    for ((i = 0 ; i < $ncores ; i++)); do
        core_arr+=($((4*$i)));
    done
    echo $(arr_to_str "${core_arr[@]}");
}

echo $(core_list 1);
echo $(core_list 2);
echo $(core_list 6);

# Warmup
echo "Starting warmup"
./run_apps.sh $config-warmup $thru_target $lat_target $duration $thru_sz $thru_qd 6 0 $use_cores;

for num_cores in 1 2 3 4 5 6; do

    echo "Starting $config-cores$num_cores";
    echo $(core_list $num_cores);
    ./run_apps.sh $config-cores$num_cores $thru_target $lat_target $duration $thru_sz $thru_qd $num_cores $num_cores $(core_list $num_cores);
    sleep 10;

done
