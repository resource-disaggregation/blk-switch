#!/bin/bash

# Example usage: ./run_latthru.sh remoteram 192.168.10.115:5000 60 thru1 

config=$1
target=$2
duration=$3
app=$4

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

for sz in 4 16 32 64 128; do 
    for qd in 1 2 4 8 16 32 64 128; do

        # Single iteration

        pids=()

        outlabel="$config-sz$sz-qd$qd"
        echo "Starting $outlabel";

        # Spawn IOkernel
        sudo ./iokerneld 2>&1 | tee $outlabel.iokernel &
        pids+=($!);
        echo "Launched IOKernel";

        # Give IOKernel time to start
        sleep 10;

        # Start app
        sudo apps/bench/storage_client "$app.config" $target $(($sz*1024)) $qd $duration 1 > ~/shenango-eval/$outlabel.txt 2>&1 &
        pids+=($!);
        (sleep $record_sleep; sar -u 1 $record_duration -P $record_cores > ~/shenango-eval/$outlabel.util) &
        (sleep $record_sleep; pidstat -u -hl -p $(pgrep storage_client),$(pgrep -d, -P $(pgrep storage_client)) 1 $record_duration > ~/shenango-eval/$outlabel.apputil) &
        echo "Launched $app";

        # Wait for run to complete
        sleep $(($duration+$buffer_duration));
        cleanup;
        sleep 2;

    done
done




