# Caladan Evaluation

**Note:** This README is self-contained. It contains instructions to setup and evaluate against Caladan (one of the baselines used in the paper). You do not need to build/setup blk-switch for this. Just the instructions here are sufficient.

**Note:** Our evaluation in the submitted paper was against a verison the caladan codebase that the authors shared with us before their OSDI'20 artifact was ready. We have included a copy of this codebase (`caladan-code`) inline for reproducibility.

**Caveat:** For our evaluation we run Caladan in directpath mode on both machines. For directpath to work, Caladan specifically requires a _Mellanox ConnectX-5_ NIC. (In the setup that we used, both the machines had 100Gbps ConnectX-5 NICs). The precise requirements are as follows (as per Caladan docs):
> Directpath is currently only supported with Mellanox ConnectX-5 using Mellanox OFED v4.6 or newer. NIC firmware must include support for User Context Objects (DEVX) and Software Managed Steering Tables. For the ConnectX-5, the firmware version must be at least 16.26.1040. Additionally, directpath requires Linux kernel version 5.0.0 or newer.

### Build and setup Caladan
The below commands to setup, build, configure, and prepare SPDK need to be run on both machines (Host & Target).

_(Several of these steps are from the Caladan README, and have been put here for convenience)_

Install needed prerequisites
```
sudo apt install build-essential libnuma-dev clang autoconf autotools-dev m4 automake libevent-dev  libpcre++-dev libtool ragel libev-dev moreutils parallel cmake python3 python3-pip libjemalloc-dev libaio-dev libdb5.3++-dev numactl hwloc libmnl-dev libnl-3-dev libnl-route-3-dev uuid-dev libssl-dev libcunit1-dev pkg-config
```

**Note:** If you are using Ubuntu 16.04 or below (we have tested on 16.04) then you need to install gcc/g++ 7. See here for instructions on how to do so: [https://askubuntu.com/a/887791](https://askubuntu.com/a/887791)

Setup dependencies
```
cd caladan-code
./dpdk.sh
./spdk.sh
./rdma-core.sh
cd ..
```

Build Caladan + storage server + apps
```
cd caladan-code
./build-all.sh
cd ..
```

### System configuration

Disable hyperthreading (if on). If the below does not work, then refer to this: [https://askubuntu.com/a/942843](https://askubuntu.com/a/942843)
```
echo off | sudo tee /sys/devices/system/cpu/smt/control
```

**Important**: First, edit the `config.sh` file based on your setup. (there are detailed comments in the file)


### Prepare environment for running experiments
```
sudo ./caladan-code/scripts/setup_machine.sh
python gen_configs.py 24 1 0 10 1
```

### Run storage servers on the target machine
The following command needs to be run on the Target machine to start the storage servers (which will service storage IO requests made by application on the Host)
```
sudo ./run_servers.sh 6 6 ram
```

It can take upto 30 seconds for the servers to start up fully. The script will print "Ready" when complete. You can also use a CPU monitoring tool like `htop` to see what is happening. Once the server start, you should see 100% utilization on several of the cores.

### Run experiments
Below are the steps to run the evaluation section experiments in our paper.

**Note:** We recommend restarting the storage servers on the Target machine for every experiment.

**Caveat:** Note that, for some of the configurations, the system is not fully stable. Hence, one may need to run an experiment multiple times to obtain a meaningful result. Keeping an eye on `htop` would be a good idea to monitor what is going on. You should see high utilization on several cores while the experiment is running. If the utiliization of some core(s) abruptly drops to 0 (and remains that way) in the middle of the experiment, then it is likely an indication of unstable behavior, and it would be good to re-run the experiment afresh.

#### Vary L-app load (Figure 7)

Run storage servers on Target machine (if already running, then close first using Ctrl^C)
```
sudo ./run_servers.sh 6 6 ram
```

Run experiment on Host machine & collect results
```
sudo ./caladan_fig7.sh
sudo ./collect_fig7.sh
```


#### Vary T-app load (Figure 8)
Run storage servers on Target machine (if already running, then close first using Ctrl^C)
```
sudo ./run_servers.sh 6 6 ram
```

Run experiment on Host machine & collect results
```
sudo ./caladan_fig8.sh
sudo ./collect_fig8.sh
```

#### Varying number of cores (Figure 9)
Run storage servers on Target machine (if already running, then close first using Ctrl^C)
```
sudo ./run_servers.sh 10 10 ram
```

Run experiment on Host machine & collect results
```
sudo ./caladan_fig9.sh
sudo ./collect_fig9.sh
```

#### SSD experiment (Figure 10)
**Note:** To run this experiment, an NVMe SSD is required on the Target machine. If such an SSD is present, then the Caladan runtime should automatically detect it.

Run storage servers on Target machine (if already running, then close first using Ctrl^C)
Note the change in last argument to SSD.
```
sudo ./run_servers.sh 6 6 ssd
```

Run experiment on Host machine & collect results
```
sudo ./caladan_fig10.sh
sudo ./collect_fig10.sh
```

#### Varying read ratio (Fig 11)
Run storage servers on Target machine (if already running, then close first using Ctrl^C)
```
sudo ./run_servers.sh 6 6 ram
```

Run experiment on Host machine & collect results
```
sudo ./caladan_fig11.sh
sudo ./collect_fig11.sh
```