#!/bin/bash

# Example usage: ./run_servers.sh



function cleanup() {
    sudo killall storage_server;
    echo "Cleaned up";
}

trap cleanup EXIT

function arr_to_str() {
    data=$1
    printf -v joined '%s,' "${data[@]}";
    echo "${joined%,}";
}

cleanup;


pids=()


# Start servers
for ((i = 1 ; i <= 6 ; i++)); do
    sudo STORAGE_DEVICE="trtype:PCIe traddr:0000:db:00.0" apps/storage_service/storage_server thru_server$i.config 5000 &
    pids+=($!);
    echo "Launched thru_server$i";
done

sleep infinity;







