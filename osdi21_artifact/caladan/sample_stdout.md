### Sample output

Here is a sample output from a successful run on one of our machines. (You can expect similar output during both `run_servers.sh` and `caladan_fig*.sh`):

```
EAL: Detected 24 lcore(s)
EAL: Detected 4 NUMA nodes
EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
EAL: Selected IOVA mode 'PA'
EAL: No free hugepages reported in hugepages-2048kB
EAL: No free hugepages reported in hugepages-2048kB
EAL: No free hugepages reported in hugepages-2048kB
EAL: No available hugepages reported in hugepages-1048576kB
EAL: Probing VFIO support...
EAL: PCI device 0000:01:00.0 on NUMA socket 0
EAL:   probe driver: 8086:1521 net_e1000_igb
EAL: PCI device 0000:01:00.1 on NUMA socket 0
EAL:   probe driver: 8086:1521 net_e1000_igb
EAL: PCI device 0000:17:00.0 on NUMA socket 0
EAL:   probe driver: 8086:10fb net_ixgbe
EAL: PCI device 0000:17:00.1 on NUMA socket 0
EAL:   probe driver: 8086:10fb net_ixgbe
EAL: PCI device 0000:25:00.0 on NUMA socket 0
EAL:   probe driver: 15b3:1019 net_mlx5
net_mlx5: Failed to create TIS using DevX
net_mlx5: TIS allocation failure
net_mlx5: probe of PCI device 0000:25:00.0 aborted after encountering an error: Cannot allocate memory
EAL: Requested device 0000:25:00.0 cannot be used
EAL: PCI device 0000:25:00.1 on NUMA socket 0
EAL:   probe driver: 15b3:1019 net_mlx5
[  1.277275] CPU 00| <5> dpdk: driver: net_mlx5 port 0 MAC: 98 03 9b 64 0c 3b
[  1.408048] CPU 00| <5> mlx5: device cycles / us: 78.1250
[  1.408070] CPU 00| <5> main: core 0 running dataplane. [Ctrl+C to quit]
```
