# blk-switch: Rearchitecting Linux Storage Stack for μs Latency and High Throughput
blk-switch is a redesign of the Linux kernel storage stack that achieves μs-scale latencies while saturating 100Gbps links, even when tens of applications are colocated on a server. The key insight in blk-switch is that the multi-queue storage and network I/O architecture makes storage stacks conceptually similar to network switches. The main design contributions of blk-switch are:
- Multiple egress queues: enable isolating request types (prioritization)
- Request steering: avoids transient congestion (load valancing)
- Application steering: avoids persistent congestion (switch scheduling)

## 1. Overview
### Repository overview
- *block/* includes blk-switch core implementation. 
- *drivers/nvme/* includes remote storage stack kernel modules such as i10 and NVMe-over-TCP (nvme-tcp).
- *include/linux* includes small changes in some headers for blk-switch and i10.
- *osdi21_artifact/* includes scripts for OSDI21 artifact evaluation.
- *scripts/* includes scripts for getting started instructions.

### System overview
For simplicity, we assume that users have two physical servers (Host and Target) connected with each other over networks. Target server has actual storage devices (e.g., RAM block device, NVMe SSD, etc.), and Host server accesses the Target-side storage devices via the remote storage stack (e.g., i10, nvme-tcp) over the networks. Then Host server runs latency-sensitive applications (L-apps) and throughput-bound applications (T-apps) using standard I/O APIs (e.g., Linux AIO). Meanwhile, blk-switch plays a role for providing μs-latency and high throughput at the kernel block device layer.

### Getting Started Guide
Through the following three sections, we provide getting started instructions to install blk-switch and to run experiments.

   - **Build blk-switch Kernel (10 human-mins + 30 compute-mins + 5 reboot-mins):**  
blk-switch is currently implemented in the core part of Linux kernel storage stack (blk-mq at block device layer), so it requires kernel compilation and system reboot into the blk-switch kernel. This section covers how to build the blk-switch kernel and i10/nvme-tcp kernel modules. 
   - **Setup Remote Storage Devices (5 human-mins):**  
This section covers how to setup remote storage devices using i10/nvme-tcp kernel modules.
   - **Run Toy-experiments (5-10 compute-mins):**  
This section covers how to run experiments with the blk-switch kernel. 

The detailed instructions to reproduce all individual results presented in our OSDI21 paper is provided in the "[osdi21_artifact](https://github.com/resource-disaggregation/blk-switch/tree/master/osdi21_artifact)" directory.

## 2. Build blk-switch Kernel (with root)
blk-switch has been successfully tested on Ubuntu 16.04 LTS with kernel 5.4.43. Building the blk-switch kernel should be done on both Host and Target servers.

**(Don't forget to be root)**
1. Download Linux kernel source tree:
   ```
   cd ~
   wget https://mirrors.edge.kernel.org/pub/linux/kernel/v5.x/linux-5.4.43.tar.gz
   tar xzvf linux-5.4.43.tar.gz
   ```

2. Download blk-switch source code and copy to the kernel source tree:

   ```
   git clone https://github.com/resource-disaggregation/blk-switch.git
   cd blk-switch
   cp -rf block drivers include ~/linux-5.4.43/
   cd ~/linux-5.4.43/
   ```

3. Update kernel configuration:

   ```
   cp /boot/config-x.x.x .config
   make olddefconfig
   ```
   "x.x.x" is a kernel version. It can be your current kernel version or latest version your system has. Type "uname -r" to see your current kernel version.  
 
   Edit ".config" file to include your name in the kernel version.
   ```
   vi .config
   (in the file)
   ...
   CONFIG_LOCALVERSION="-jaehyun"
   ...
   ```
   Save the .config file and exit.   

4. Make sure i10 and nvme-tcp modules are included in the kernel configuration:

   ```
   make menuconfig

   - Device Drivers ---> NVME Support ---> <M> NVM Express over Fabrics TCP host driver
   - Device Drivers ---> NVME Support ---> <M>   NVMe over Fabrics TCP target support
   - Device Drivers ---> NVME Support ---> <M> i10: A New Remote Storage I/O Stack (host)
   - Device Drivers ---> NVME Support ---> <M> i10: A New Remote Storage I/O Stack (target)
   ```
   Press "Save" and "Exit"

5. Compile and install:

   ```
   make -j24 bzImage
   make -j24 modules
   make modules_install
   make install
   ```
   The number 24 means the number of threads created for compilation. Set it to be the total number of cores of your system to reduce the compilation time. Type "lscpu | grep 'CPU(s)'" to see the total number of cores:
   
   ```
   CPU(s):                24
   On-line CPU(s) list:   0-23
   ```

6. Edit "/etc/default/grub" to boot with your new kernel by default. For example:

   ```
   ...
   #GRUB_DEFAULT=0 
   GRUB_DEFAULT="1>Ubuntu, with Linux 5.4.43-jaehyun"
   ...
   ```

7. Update the grub configuration and reboot into the new kernel.

   ```
   update-grub && reboot
   ```

8. Do the same steps 1--7 for both Host and Target servers.

9. When systems are rebooted, check the kernel version: Type "uname -r". It should be "5.4.43-(your name)".

## 3. Setup Remote Storage Devices
We will compare two systems in the toy-experiment section, "blk-switch" and "Linux". We implemented a part of blk-switch (multi-egress support of i10) in the nvme-tcp kernel module. Therefore, we use
- nvme-tcp module for "blk-switch"
- i10 module for "Linux"

We now configure RAM null-blk device as a remote storage device at Target server. 

### Target configuration
(In step 4, we provide a script that covers steps 1--3, so please go to step 4 if you want to skip steps 1--3)

1. Create null-block devices (10GB):

   ```
   modprobe null-blk gb=10 bs=4096 irqmode=1 hw_queue_depth=1024 submit-queues=24
   ```
   Use the number of cores of your system for "submit-queues".
   
2. Load i10/nvme-tcp target kernel modules:

   ```
   modprobe nvmet nvmet-tcp i10-target
   ```

3. Create an nvme-of subsystem:

   ```
   mkdir /sys/kernel/config/nvmet/subsystems/(subsystem name)
   cd /sys/kernel/config/nvmet/subsystems/(subsystem name)
   
   echo 1 > attr_allow_any_host
   mkdir namespaces/10
   cd namespaces/10
   echo -n (device name) > device_path
   echo 1 > enable
   
   mkdir /sys/kernel/config/nvmet/ports/1
   cd /sys/kernel/config/nvmet/ports/1
   echo xxx.xxx.xxx.xxx > addr_traddr
   echo (protocol name) > addr_trtype
   echo 4420 > addr_trsvcid
   echo ipv4 > addr_adrfam
   
   ln -s /sys/kernel/config/nvmet/subsystems/(subsystem name) /sys/kernel/config/nvmet/ports/1/subsystems/(subsystem name)
   ```

   - device name: "/dev/nullb0" for null-blk, "/dev/nvme0n1" for NVMe SSD
   - xxx.xxx.xxx.xxx: Target IP address
   - protocol name: "tcp", "i10", etc.

4. Or, you can use our script for a quick setup (for RAM null-blk devices):

   ```
   cd ~
   cd blk-switch/scripts/
   (see NOTE below)
   ./target_null.sh
   ```

   **NOTE: please edit "system_env.sh" to specify target IP address and number of cores before running "target_null.sh".**  
   You can type "lscpu | grep 'CPU(s)'" to get the number of cores of your system.  


### Host configuration
(Go to step 4 if you want to skip steps 2--3. You still need to run step 1.)

1. Install NVMe utility (nvme-cli):

   ```
   cd ~
   git clone https://github.com/linux-nvme/nvme-cli.git
   cd nvme-cli
   make
   make install
   ```
   
2. Load i10/nvme-tcp host kernel modules:

   ```
   modprobe nvme-tcp i10-host
   ```

3. Connect to the target subsystem:

   ```
   nvme connect -t (protocol name) -n (subsystem name) -a (target IP address) -s 4420 -q nvme_tcp_host -W (num of cores)
   ```
   
4. Or, you can use our script for a quick setup:

      ```
      cd ~
      cd blk-switch/scripts/
      (see NOTE below)
      ./host_tcp_null.sh
      ```
      **NOTE: please edit "system_env.sh" to specify target IP address and number of cores before using it.**  
      You can type "lscpu | grep 'CPU(s)'" to get the number of cores of your system.  

4. Check the remote storage device name you just created (e.g., /dev/nvme0n1):

   ```
   nvme list
   ```

## 4. Run Toy-experiments
At Host, we run FIO to test blk-switch using the remote null-blk device (/dev/nvme0n1). 

1. Install FIO

   ```
   sudo apt-get install fio
   ```
   Or refer to https://github.com/axboe/fio to install the latest version.

2. Run one L-app and one T-app on a core:

   ```
   cd ~
   cd blk-switch/scripts/
   (see NOTE below)
   ./toy_example_blk-switch.sh
   ```
   NOTE: Edit "**toy_example_blk-switch.sh**" if your remote null-blk device created above for blk-switch is not "**/dev/nvme0n1**".
  
3. Compare with Linux (pure i10 without blk-switch):

   ```
   cd ~
   cd blk-switch/scripts/
   ./host_i10_null.sh
   (see NOTE below)
   ./toy_example_linux.sh
   ```
   NOTE: Check the remote storage device name newly added after executing "**host_i10_null.sh**". We assume it is "**/dev/nvme1n1**". Edit "**toy_example_linux.sh**" if not.
  
4. Validate results (see output files on the same directory):

   If system has multiple cores per socket,
      - L-app is isolated by blk-switch achieving lower latency than Linux.
      - T-app uses more CPU resources by blk-switch achieving higher throughput than Linux.

**To run more evaluation scripts:** refer to "[osdi21_artifact/](https://github.com/resource-disaggregation/blk-switch/tree/master/osdi21_artifact)".
