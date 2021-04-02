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
```
sudo ethtool -K <interface> tso on gso on gro on
sudo ifconfig <interface> mtu 9000
sudo ./enable_arfs.sh <interface>
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

