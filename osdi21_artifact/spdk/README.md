# SPDK Evaluation

### Fetch and build SPDK
```
git submodule update --init --recursive
cd spdk
sudo scripts/pkgdep.sh
./configure
make
cd ..
```

### Sytems configuration
Network stack configuration
Enable TSO/GRO, Jumbo frames, aRFS
(set iface to the interface name of the NIC to be used)
[TODO: enable arfs script is specific to Mellanox NIC. Need to check for other NICs]
```
export iface=ens2f0
sudo ethtool -K $iface tso on gso on gro on
sudo ifconfig $iface mtu 9000
sudo ./enable_arfs.sh $iface
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
```
sudo ./run_target.sh 0x333333 192.168.10.115
```

### Run experiments

```
sudo ./spdk_fig7.sh
sudo ./collect_fig7.sh
```

```
sudo ./spdk_fig8.sh
sudo ./collect_fig8.sh
```

```
sudo ./spdk_fig9.sh
sudo ./collect_fig9.sh
```

```
sudo ./spdk_fig10.sh
sudo ./collect_fig10.sh
```

```
sudo ./spdk_fig11.sh
sudo ./collect_fi1.sh
```