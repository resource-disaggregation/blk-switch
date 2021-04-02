# USENIX OSDI 2021 Artifact Evaluation

## 1. Hardware Configurations
Our hardware configurations used in the paper are:
- CPU: 4-socket Intel Xeon Gold 6128 3.4GHz with 6 cores per socket (with hyperthreading disabled)
- RAM: 256GB
- NIC: Mellanox ConnectX-5 Ex VPI (100Gbps)
- NVMe SSD: 1.6TB Samsung PM1725a

## 2. Detailed Instructions
Now we provide how to use our scripts to reproduce the results in the paper. 
If you miss the [getting started instruction](https://github.com/resource-disaggregation/blk-switch#getting-started-guide), please complete "Building blk-swith Kernel" section first and come back.

### Run configuration scripts (with root)
You should be root from now on. If you already ran some configuration scripts below while doing the getting started instruction, you can skip those scripts.

1. At Target:  
 Check if your system has NVMe SSD devices. Type "nvme list" and see if there is "/dev/nvme0n1".  
 We will skip "target_ssd.sh" and use only RAM device (null-blk) if your system does not have "/dev/nvme0n1".

   ```
   cd ~
   cd blk-switch
   (Edit "system_env.sh" first)
   ./scripts/system_setup.sh
   ./scripts/target_null.sh
   (Run below only when your system has NVMe SSD)
   ./scripts/target_ssd.sh
   ```
   
2. At Host:  
 We will skip "host_tcp_ssd.sh" and "host_i10_ssd.sh" if your TARGET server does not have NVMe SSD.
 After running the scripts below, you will see that 2-4 more remote storage devices are created via "nvme list".
   ```
   cd ~
   cd blk-switch
   (Edit "system_env.sh" first)
   ./scripts/system_setup.sh
   ./scripts/host_tcp_null.sh
   ./scripts/host_i10_null.sh
   (Run below only when your target has NVMe SSD)
   ./scripts/host_tcp_ssd.sh
   ./scripts/host_i10_ssd.sh
   ```

### Linux and blk-switch Evaluation (with root)
Now you will run evaluation scripts at Host server. We need to identify newly added remote devices to use the right one for each script.  

If your Host server has no NVMe SSD, then your remote devices are:
- **/dev/nvme0n1**: null-blk device for blk-switch
- **/dev/nvme1n1**: null-blk device for Linux
- **/dev/nvme2n1**: SSD device for blk-switch
- **/dev/nvme3n1**: SSD device for Linux

If your Host server has already one NVMe SSD (i.e., /dev/nvme0n1), then your remote devices are:
- **/dev/nvme1n1**: null-blk device for blk-switch
- **/dev/nvme2n1**: null-blk device for Linux
- **/dev/nvme3n1**: SSD device for blk-switch
- **/dev/nvme4n1**: SSD device for Linux

In our scripts, we assume that there's no NVMe SSD at Host server. So the default configuration in our scripts is:  

For Figures 7, 8, 9, 11 (null-blk scenario):
- blk-switch: "$nvme_dev = /dev/nvme0n1"
- Linux: "$nvme_dev = /dev/nvme1n1"

For Figure 10 (SSD scenario):
- blk-switch:
   - "$nvme_dev = /dev/nvme0n1"
   - "$ssd_dev = /dev/nvme2n1"
- Linux:
   - "$nvme_dev = /dev/nvme1n1"
   - "$ssd_dev = /dev/nvme3n1"

If this is your case, then you are safe to go. If this is not the case for your Host system (e.g., your Host has an NVMe SSD), please edit the scripts below with right device names before running them.

1. Increasing L-app load (Figure 7):

   ```
   ./blk-switch/linux_fig7.pl
   ./blk-switch/blk-switch_fig7.pl
   ```

2. Increasing T-app load (Figure 8):

   ```
   ./blk-switch/linux_fig8.pl
   ./blk-switch/blk-switch_fig8.pl
   ```

3. Varying number of cores (Figure 9)

   ```
   ./blk-switch/linux_fig9.pl
   ./blk-switch/blk-switch_fig9.pl
   ```

4. SSD results corresponding to Figure 7 (Figure 10)

   ```
   ./blk-switch/linux_fig10.pl
   ./blk-switch/blk-switch_fig10.pl
   ```

5. Increasing read ratio (Figure 11)
  
   ```
   ./blk-switch/linux_fig11.pl
   ./blk-switch/blk-switch_fig11.pl
   ```

### SPDK Evaluation
Please refer to README in spdk/ folder
