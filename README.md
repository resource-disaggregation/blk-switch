# blk-switch: Rearchitecting Linux Storage Stack for μs Latency and High Throughput
blk-switch is a redesign of the Linux kernel storage stack that achieves μs-scale latencies while saturating 100Gbps links, even when tens of applications are colocated on a server. The key insight in blk-switch is that the multi-queue storage and network I/O architecture makes storage stacks conceptually similar to network switches. blk-switch has three design contributions:
- Multiple egress queues: enable isolating request types (prioritization)
- Request steering: avoids transient congestion (load valancing)
- Application steering: avoids persistent congestion (switch scheduling)

## Setup instructions (with root)
blk-switch has been successfully tested on Ubuntu 16.04 LTS with kernel [5.4.43](https://mirrors.edge.kernel.org/pub/linux/kernel/v5.x/linux-5.4.43.tar.gz). We assume the kernel source tree is downloaded in /usr/src/linux-5.4.43/. You should overwrite the blk-switch code to the kernel source tree and compile the kernel to install blk-switch.

1. Download blk-switch kernel source code and copy to the kernel source tree:

   ```
   git clone https://github.com/resource-disaggregation/blk-switch.git
   cd blk-switch
   cp -rf block drivers include /usr/src/linux-5.4.43/
   cd /usr/src/linux-5.4.43/
   ```

2. Update kernel configuration:

   ```
   cp /boot/config-x.xx.x .config (=> your current kernel version)
   make oldconfig
   ```

3. Make sure NVMe-over-TCP and i10 modules are included in the kernel configuration:

   ```
   make menuconfig

   - Device Drivers ---> NVME Support ---> <M> NVM Express over Fabrics TCP host driver
   - Device Drivers ---> NVME Support ---> <M>   NVMe over Fabrics TCP target support
   - Device Drivers ---> NVME Support ---> <M> i10: A New Remote Storage I/O Stack (host)
   - Device Drivers ---> NVME Support ---> <M> i10: A New Remote Storage I/O Stack (target)
   ```

4. Compile and install:

   ```
   make -j24 bzImage
   make -j24 modules
   make modules_install
   make install
   ```

5. Reboot with the new kernel.

6. Repeat above 1--5 steps in a target server.

## Running blk-switch

### Configure i10 host/target servers
We assume that the target server has storage devices such as NVMe SSD (/dev/nvme0n1) or RAM block device (/dev/ram0) via i10.

- Please refer to "[HowTo Configure NVMe over Fabrics](https://community.mellanox.com/s/article/howto-configure-nvme-over-fabrics)" for more information about NVMe-over-Fabrics configurations.
- Please refer to "[HowTo Configure i10 host/target](https://github.com/i10-kernel/i10-implementation/edit/master/README.md)"

### CFS configuration
We set the "target latency" parameter to 100μs:

  ```
  echo 100000000 > /proc/sys/kernel/sched_latency_ns
  ```

### Run test scripts: refer to "[osdi21_artifact/](https://github.com/resource-disaggregation/blk-switch/tree/master/osdi21_artifact)".
