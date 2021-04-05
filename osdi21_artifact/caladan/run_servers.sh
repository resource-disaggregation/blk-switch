#!/bin/bash

source config.sh

# Example usage: ./run_servers.sh <l-cores> <t-cores> <ram/ssd>
l_cores=$1
t_cores=$2
storage=$3


function cleanup() {
    killall storage_server;
    killall storage_server_ssd;
    killall iokerneld;
    echo "Cleaned up";
}

trap cleanup EXIT

function arr_to_str() {
    data=$1
    printf -v joined '%s,' "${data[@]}";
    echo "${joined%,}";
}

IFS=',' read -r -a all_cores <<< "$CORE_LIST";
core_count=${#all_cores[@]}
core_count=$(($core_count-2))

if [ $(($t_cores+1)) -gt $core_count ]; then
    echo "Error: insufficient number of cores";
    exit 1;
fi

l_cores=$((l_cores < ($core_count-$t_cores) ? l_cores : ($core_count-$t_cores)))

cleanup;


pids=()

# Spawn IOkernel
NUMA_LIMIT=$NUMA_CAP ./caladan-code/iokerneld numa &
pids+=($!);
echo "Launched IOKernel";

# Give IOKernel time to start
echo "Giving IOKernel time to start"
sleep 10;

# Generate config for T server
python gen_config_tservers.py $t_cores

# Start T servers
for ((i = 1 ; i <= $t_cores ; i++)); do
    NUMA_LIMIT=$NUMA_CAP caladan-code/apps/storage_service/storage_server thru_server$i.config 5000 &
    pids+=($!);
    echo "Launched thru_server$i";
done

echo "Giving T servers time to start";
sleep 10;

# Generate config for L server
python gen_config_lserver.py $l_cores $storage

# Start L server
if [ "$storage" = "ssd" ]; then
    NUMA_LIMIT=$NUMA_CAP caladan-code/apps/storage_service/storage_server_ssd lat_server.config 5000 &
else
    NUMA_LIMIT=$NUMA_CAP caladan-code/apps/storage_service/storage_server lat_server.config 5000 &
fi
echo "Launched lat_server";

sleep 5;
echo "Ready";

sleep infinity;







