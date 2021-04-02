# USENIX OSDI 2021 Artifact Evaluation

## 1. Hardware Configurations
Our hardware configurations used in the paper are:
- CPU: 4-socket Intel Xeon Gold 6128 3.4GHz with 6 cores per socket (with hyperthreading disabled)
- RAM: 256GB
- NIC: Mellanox ConnectX-5 Ex VPI (100Gbps)
- NVMe SSD: 1.6TB Samsung PM1725a

[**Caveats of our work**]
- Our work has been evaluated with 100Gbps NICs and 4-socket multi-core CPUs. Performance degradation is expected if the above hardware configuration is not available.
- **system_setup.sh** includes Mellanox NIC-specific configuration (e.g., enabling aRFS).
- As described in the paper, we mainly use 6 cores in NUMA0 and their core numbers, 0, 4, 8, 12, 16, 20, are used through the evaluation scripts. These numbers should be changed if the systems have different number of NUMA nodes:
   ```
   lscpu | grep 'CPU(s)'
   
   CPU(s):                24
   On-line CPU(s) list:   0-23
   NUMA node0 CPU(s):     0,4,8,12,16,20
   ...
   ```

## 2. Detailed Instructions
Now we provide how to use our scripts to reproduce the results in the paper. 

**[NOTE]:**
- If you miss the [getting started instruction](https://github.com/resource-disaggregation/blk-switch#getting-started-guide), please complete "Building blk-swith Kernel" section first and come back.
- If you get an error while running the "Run configuration scripts", please reboot the both servers and restart from the "Run configuration scripts" section.

### Run configuration scripts (with root)
You should be root from now on. If you already ran some configuration scripts below while doing the getting started instruction, you **SHOULD NOT** run those scripts -- **target_null.sh**, **host_tcp_null.sh**, and **host_i10_null.sh**.

**(Don't forget to be root)**

1. At Target:  
 Check if your Target has physical NVMe SSD devices. Type "nvme list" and see if there is "**/dev/nvme0n1**".  
 If your Target does not have "**/dev/nvme0n1**", we will skip "**target_ssd.sh**" and will configure only RAM device (null-blk) below.

   ```
   sudo -s
   cd ~/blk-switch/scripts/
   (See NOTE below first)
   ./system_setup.sh
   ./target_null.sh
   (Run below only when your system has NVMe SSD)
   ./target_ssd.sh
   ```
   **NOTE: please edit "system_env.sh" to specify Target IP address, interface name, and number of cores before running the following scripts.**
   You can type "lscpu | grep 'CPU(s)'" to get the number of cores of your system.
   
   The below error messages from **system_setup.sh** is normal. Please ignore them.
   ```
   Cannot get device udp-fragmentation-offload settings: Operation not supported
   Cannot get device udp-fragmentation-offload settings: Operation not supported
   ```
   
   If you ran "**target_null.sh**" twice by mistake and got several errors like "Permission denied", please reboot the both servers and restart from "Run configuration scripts".
   
   
2. At Host:  
 Also we will skip "**host_tcp_ssd.sh**" and "**host_i10_ssd.sh**" if your Target server does not have physical NVMe SSD devices.
 After running the scripts below, you will see that 2-4 remote storage devices are created (type "nvme list").
   ```
   sudo -s
   cd ~/blk-switch/scripts/
   (See NOTE below first)
   ./system_setup.sh
   ./host_tcp_null.sh
   ./host_i10_null.sh
   (Run below only when your target has NVMe SSD)
   ./host_tcp_ssd.sh
   ./host_i10_ssd.sh
   ```
   **NOTE: please edit "system_env.sh" to specify Target IP address, interface name, and number of cores before running the following scripts.**
   You can type "lscpu | grep 'CPU(s)'" to get the number of cores of your system.

### Linux and blk-switch Evaluation (with root)
Now you will run evaluation scripts at Host server. We need to identify newly added remote devices to use the right one for each script.  

If your Host server has no NVMe SSD, then your remote devices are:
- **/dev/nvme0n1**: null-blk device for blk-switch
- **/dev/nvme1n1**: null-blk device for Linux
- **/dev/nvme2n1**: SSD device for blk-switch
- **/dev/nvme3n1**: SSD device for Linux

If your Host server has already one NVMe SSD (i.e., **/dev/nvme0n1**), then your remote devices are:
- **/dev/nvme1n1**: null-blk device for blk-switch
- **/dev/nvme2n1**: null-blk device for Linux
- **/dev/nvme3n1**: SSD device for blk-switch
- **/dev/nvme4n1**: SSD device for Linux

In our scripts, we assume that there's no NVMe SSD at Host server. So the default configuration in our scripts is:  

For Figures 7, 8, 9, 11 (null-blk scenario):
- blk-switch: "**$nvme_dev = /dev/nvme0n1**"
- Linux: "**$nvme_dev = /dev/nvme1n1**"

For Figure 10 (SSD scenario):
- blk-switch:
   - "**$nvme_dev = /dev/nvme0n1**"
   - "**$ssd_dev = /dev/nvme2n1**"
- Linux:
   - "**$nvme_dev = /dev/nvme1n1**"
   - "**$ssd_dev = /dev/nvme3n1**"

If this is your case, then you are safe to go. If this is not the case for your Host system (e.g., your Host has an NVMe SSD), please EDIT the scripts below with right device names before running them.

1. Figure 7: Increasing L-app load (6 mins):

   ```
   cd ~/blk-switch/osdi21_artifact/blk-switch/
   ./linux_fig7.pl
   ./blk-switch_fig7.pl
   ```

2. Figure 8: Increasing T-app load (12 mins):

   ```
   cd ~/blk-switch/osdi21_artifact/blk-switch/
   ./linux_fig8.pl
   ./blk-switch_fig8.pl
   ```

3. Figure 9: Varying number of cores (20 mins):

   ```
   cd ~/blk-switch/osdi21_artifact/blk-switch/
   ./linux_fig9.pl
   ./blk-switch_fig9.pl
   ```

4. Figure 10: SSD results corresponding to Figure 7 (6 mins):

   ```
   cd ~/blk-switch/osdi21_artifact/blk-switch/
   ./linux_fig10.pl
   ./blk-switch_fig10.pl
   ```

5. Figure 11: Increasing read ratio (10 mins):
  
   ```
   cd ~/blk-switch/osdi21_artifact/blk-switch/
   ./linux_fig11.pl
   ./blk-switch_fig11.pl
   ```

### Figure 13: blk-switch Performance Breakdown (~1 min)
To reproduce Figure 13 results, we will run four experiments named "**Linux**", "**Linux+P**", "**Linux+P+RS**", "**Linux+P+RS+AS**", and "**(Extra)**". The (Extra) is nothing but performed to print out kernel logs as the request-steering logs appear when a new experiment starts.
   ```
   cd ~/blk-switch/osdi21_artifact/blk-switch/
   ./blk-switch_fig13.pl
   ```

After all is done, type "dmesg" to see the kernel logs. The last 6 lines are for "**Linux+P+RS+AS**" (Figure 13f) and the 7th line shows how L-app moves. The next last 6 lines are for "**Linux+P+RS**" (Figure 13e). For each core, the kernel logs mean:
- gen: how many T-app requests are generated on that core.
- str: how many T-app requests are steered to other cores on that core.
- prc: how many T-app requests came from other cores are processed on that core.

### SPDK Evaluation
Please refer to README in spdk/ folder
