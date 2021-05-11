#!/bin/bash

# Example usage: ./run_apps.sh singlecore-remoteram 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 'trtype:TCP traddr:192.168.10.115 adrfam:IPv4 trsvcid:4420 subnqn:nqn.2020-07.com.midhul:null0' 60 $((128*1024)) 8 1 1 0 1

config=$1
thru_target="$2"
lat_target="$3"
duration=$4
thru_sz=$5
thru_qd=$6
lat_sz=$((4*1024))
lat_qd=1
num_thru=$7
num_lat=$8
lat_z=$9
lat_y=10
mapfile -t use_cores < config/use_cores.txt
# use_cores=('0x1' '0x10' '0x100' '0x1000' '0x10000' '0x100000' '0x2' '0x20' '0x200' '0x2000' '0x20000' '0x200000' '0x4' '0x40' '0x400' '0x4000' '0x40000' '0x400000' '0x8' '0x80' '0x800' '0x8000' '0x80000' '0x800000')
num_use_cores="${10}"
read_p="${11}"
nice_lat="${12}"

buffer_duration=10

function cleanup() {
    killall spdk/build/examples/perf > /dev/null 2>&1;
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

outlabel="$config"
echo "Starting $outlabel";


# Start apps
# Thru-apps
for ((i = 1 ; i <= $num_thru ; i++)); do
    if [ -z "$read_p" ]; 
    then 
        spdk/build/examples/perf -c ${use_cores[$((($i-1)%$num_use_cores))]} -r "$thru_target" -q $thru_qd -o $thru_sz -w randread -t 60 -L > results/$outlabel.thru$i.txt 2>&1 &
    else 
        spdk/build/examples/perf -c ${use_cores[$((($i-1)%$num_use_cores))]} -r "$thru_target" -q $thru_qd -o $thru_sz -w rw -M $read_p -t 60 -L > results/$outlabel.thru$i.txt 2>&1 &
    fi
    pids+=($!);
    echo "Launched thru$i";
done

# Lat-apps
for ((i = 1 ; i <= $num_lat ; i++)); do
    if [ -z "$nice_lat" ];
    then
        spdk/build/examples/perf -c ${use_cores[$((($i-1)%$num_use_cores))]}  -r "$lat_target" -q $lat_qd -o $lat_sz -w randread -t 60 -L > results/$outlabel.lat$i.txt 2>&1 &
        
    else
        nice -n $nice_lat spdk/build/examples/perf -c ${use_cores[$((($i-1)%$num_use_cores))]}  -r "$lat_target" -q $lat_qd -o $lat_sz -w randread -t 60 -L > results/$outlabel.lat$i.txt 2>&1 &
    fi
    pids+=($!);
    echo "Launched lat$i";
done

# Wait for run to complete
sleep $(($duration+$buffer_duration));
cleanup;
sleep 2;









