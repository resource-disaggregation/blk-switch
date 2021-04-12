# SPDK Evaluation

**Note:** This README is self-contained. It contains instructions to setup and evaluate against SPDK (one of the baselines used in the paper). You do not need to build/setup blk-switch for this. Just the instructions here are sufficient.

The below commands to setup, build, configure, and prepare SPDK need to be run on both machines (Host & Target).
### Fetch and build SPDK
```
git submodule update --init --recursive
cd spdk
sudo scripts/pkgdep.sh
./configure
make
cd ..
```

### System configuration
**Important**: First, edit the `config.sh` file based on your setup. (there are detailed comments in the file)

Network stack configuration

The below commands configure the network stack to enable TSO/GRO, Jumbo frames, and aRFS
(set iface to the interface name of the NIC to be used)

**Note:** `enable_arfs.sh` enables aRFS on Mellanox ConnextX-5 NICs. For different NICs, you may need to follow a different procedure to enable aRFS (please refer to the NIC documentation). If the NIC does not support aRFS, you could skip this step and proceed, however, the results that you observe could be significantly different. (We have not experimented with setups where aRFS is disabled).

```
source config.sh
sudo ethtool -K $IFACE tso on gso on gro on
sudo ifconfig $IFACE mtu 9000
sudo ./enable_arfs.sh $IFACE
```

Upon running `ethtool`, you may see errors of the following form: `Cannot get device udp-fragmentation-offload settings: Operation not supported`. It is normal; you can ignore them.

Reset CFS config to default (if you ran blk-switch experiments before, it would have changed)
```
echo 24000000 | sudo tee /proc/sys/kernel/sched_latency_ns
```

Disable hyperthreading (if on)
```
echo off | sudo tee /sys/devices/system/cpu/smt/control
```
### Prepare environment for running experiements
```
sudo ./prepare_env.sh
```

### Run storage server on the target machine
The following command needs to be run on the Target machine to start the NVMe-oF target service (which will service storage IO requests made by application on the Host)
```
sudo ./run_target.sh 0x333333
```

### Run experiments

Now everything should be setup, and you can run experiments on the Host machine.
Following are the steps to run each of the evaluation section experiments in the paper.

Vary L-app load (Figure 7)
```
sudo ./spdk_fig7.sh # This runs the experiment
sudo ./collect_fig7.sh # This print the results
```

Vary T-app load (Figure 8)
```
sudo ./spdk_fig8.sh
sudo ./collect_fig8.sh
```

Varying number of cores (Figure 9)
For this one experiment, we need to provision a larger number of cores on the Target machine.
Hence, we need to restart the target service on the Target machine with a different argument.
Run the following commands on the Target machine:
```
sudo ./stop_target.sh # Stop existing target service
sudo ./run_target.sh 0xffffff # Restart target service with larger number of cores
```

After, this back on the Host machine, you can run the experiment as usual:
```
sudo ./spdk_fig9.sh
sudo ./collect_fig9.sh
```

Once, you are done with the experiment, need to restart the target service with default configuration.
Run the following commands on the Target machine for this:
```
sudo ./stop_target.sh
sudo ./run_target.sh 0x333333
```

SSD experiment (Figure 10)
You can run this experiment only if you have configured the SSD address in `config.sh` before.
Otherwise skip it.
```
sudo ./spdk_fig10.sh
sudo ./collect_fig10.sh
```

Varying Read/Write ratio
```
sudo ./spdk_fig11.sh
sudo ./collect_fi1.sh
```
