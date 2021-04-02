# USENIX OSDI 2021 Artifact Evaluation

## 1. Hardware Configurations
Our hardware configurations used in the paper are:
- CPU: 4-socket Intel Xeon Gold 6128 3.4GHz with 6 cores per socket (with hyperthreading disabled)
- RAM: 256GB
- NIC: Mellanox ConnectX-5 Ex VPI (100Gbps)
- NVMe SSD: 1.6TB Samsung PM1725a

## 2. Detailed Instructions
Now we provide how to use our scripts to reproduce the results in the paper. 

### Run configuration scripts
If you already ran these scripts before, skip this.

1. At Target:
   ```
   cd ~
   cd blk-switch
   (Edit env_setup before using it)
   ./scripts/env_setup
   ./scripts/system_setup.sh
   ./scripts/target_null.sh
   (Run below only when your system has an NVMe SSD - check with 'nvme list')
   ./scripts/target_ssd.sh
   ```
   
2. At Host:
   ```
   cd ~
   cd blk-switch
   (Edit env_setup before using it)
   ./scripts/env_setup
   ./scripts/system_setup.sh
   ./scripts/host_tcp_null.sh
   ./scripts/host_i10_null.sh
   (Run below only when your target has an NVMe SSD)
   ./scripts/host_tcp_ssd.sh
   ./scripts/host_i10_ssd.sh
   ```

### Linux and blk-switch Evaluation
The default remote device names for blk-switch are "**/dev/nvme0n1**" for null-blk and "**/dev/nvme2n1**" for NVMe SSD. These can be configured with the "**$nvme_dev**" and "**$ssd_dev**" variables in each script.

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

### Caladan Evaluation
(To be updated)

### SPDK Evaluation
(To be updated)

### blk-switch Performance Breakdown (Figure 13)
(To be updated)
