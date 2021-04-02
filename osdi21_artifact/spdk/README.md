# SPDK Evaluation

Fetch and build SPDK
```
git submodule update --init --recursive
cd spdk
sudo scripts/pkgdep.sh
./configure
make
```

Network stack configuration
[TODO: Enable TSO/GRO, Jumbo frames, aRFS]

Prepare environment for running experiements
```
./prepare_env.sh
TODO: put the following in the above script
sudo sysctl -w net.core.rmem_max=268435456
sudo sysctl -w net.core.wmem_max=268435456 
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 134217728"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 134217728"
echo 0 | sudo tee /proc/sys/kernel/sched_autogroup_enabled
sudo scripts/setup.sh
```

Run storage server on the target-side

Run experiments

