#!/bin/bash

source config.sh

# Example usage: ./run_apps.sh multicore-remoteram 192.168.10.115:5000 192.168.10.116:5000 60 $((32*1024)) 2 6 6 6

config=$1
thru_target=$2
lat_target=$3
duration=$4
thru_sz=$5
thru_qd=$6
lat_sz=$((4*1024))
lat_qd=1
num_thru=$7
num_lat=$8
num_use_cores=$9
read_p="${10}"

IFS=',' read -r -a all_cores <<< "$CORE_LIST";

function arr_to_str() {
    data=("$@")
    printf -v joined '%s,' "${data[@]}";
    echo "${joined%,}";
}

function core_list() {
    ncores=$1
    core_arr=()
    for ((i = 0 ; i < $ncores ; i++)); do
        core_arr+=("${all_cores[$i]}");
    done
    echo $(arr_to_str "${core_arr[@]}");
}

use_cores=$(core_list $num_use_cores);
echo "Use cores";
echo $use_cores;

buffer_duration=20
record_sleep=25
record_duration=5
record_cores=$use_cores

function cleanup() {
    killall storage_client;
    killall iokerneld;
    echo "Cleaned up";
}

trap cleanup EXIT


cleanup;


pids=()

outlabel="$config"
echo "Starting $outlabel";

# Spawn IOkernel
NUMA_LIMIT=$NUMA_CAP ./caladan-code/iokerneld numa $use_cores 2>&1 | tee results/$outlabel.iokernel &
pids+=($!);
echo "Launched IOKernel";

# Give IOKernel time to start
sleep 10;

# Start apps
# Thru-apps
for ((i = 1 ; i <= $num_thru ; i++)); do
    if [ -z "$read_p" ]; 
    then 
        NUMA_LIMIT=$NUMA_CAP caladan-code/apps/bench/storage_client "thru$i.config" 192.168.10.$((150+$i)):5000 $thru_sz $thru_qd $duration 1 100 > results/$outlabel.thru$i.txt 2>&1 &
    else 
        NUMA_LIMIT=$NUMA_CAP caladan-code/apps/bench/storage_client "thru$i.config" 192.168.10.$((150+$i)):5000 $thru_sz $thru_qd $duration 1 $read_p > results/$outlabel.thru$i.txt 2>&1 &
    fi
    pids+=($!);
    echo "Launched thru$i";
done

# Lat-apps
for ((i = 1 ; i <= $num_lat ; i++)); do
    NUMA_LIMIT=$NUMA_CAP caladan-code/apps/bench/storage_client "lat$i.config" $lat_target $lat_sz $lat_qd $duration 1 100 > results/$outlabel.lat$i.txt 2>&1 &
    pids+=($!);
    echo "Launched lat$i";
done
(sleep $record_sleep; sar -u 1 $record_duration -P $record_cores > results/$outlabel.util) &


# Wait for run to complete
sleep $(($duration+$buffer_duration));
cleanup;
sleep 2;






