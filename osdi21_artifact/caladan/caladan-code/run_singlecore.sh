#!/bin/bash

# Example usage: ./run_singlecore.sh singlecore-remoteram 192.168.10.115:5000 192.168.10.116:5000 60 $((32*1024)) 2 

config=$1
thru_target=$2
lat_target=$3
duration=$4
thru_sz=$5
thru_qd=$6
lat_sz=$((4*1024))
lat_qd=1

buffer_duration=20
record_sleep=25
record_duration=5
record_cores="0,4,8,12,16,20"

function cleanup() {
    sudo killall storage_client;
    sudo killall iokerneld;
    echo "Cleaned up";
}

trap cleanup EXIT

function arr_to_str() {
    data=$1
    printf -v joined '%s,' "${data[@]}";
    echo "${joined%,}";
}

cleanup;

for num_lat in 1 2 4 8; do 

    # Single iteration

    pids=()

    outlabel="$config-lat$num_lat"
    echo "Starting $outlabel";

    # Spawn IOkernel
    sudo ./iokerneld 2>&1 | tee $outlabel.iokernel &
    pids+=($!);
    echo "Launched IOKernel";

    # Give IOKernel time to start
    sleep 10;

    # Start apps
    # Thru-app
    sudo apps/bench/storage_client thru1.config $thru_target $thru_sz $thru_qd $duration 1 > ~/shenango-eval/$outlabel.thru1.txt 2>&1 &
    pids+=($!);
    echo "Launched thru1";
    # Lat-apps
    for ((i = 1 ; i <= $num_lat ; i++)); do
        sudo apps/bench/storage_client "lat$i.config" $lat_target $lat_sz $lat_qd $duration 1 > ~/shenango-eval/$outlabel.lat$i.txt 2>&1 &
        pids+=($!);
        echo "Launched lat$i";
    done
    (sleep $record_sleep; sar -u 1 $record_duration -P $record_cores > ~/shenango-eval/$outlabel.util) &
    (sleep $record_sleep; pidstat -u -hl -p $(pgrep -d, storage_client) 1 $record_duration > ~/shenango-eval/$outlabel.apputil) &
    

    # Wait for run to complete
    sleep $(($duration+$buffer_duration));
    cleanup;
    sleep 2;

done




