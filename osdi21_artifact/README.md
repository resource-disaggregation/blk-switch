# USENIX OSDI 2021 Artifact Evaluation

## 1. Hardware Configurations
Our hardware configurations used in the paper are:
- CPU: 4-socket Intel Xeon Gold 6128 3.4GHz with 6 cores per socket
- RAM: 256GB
- NIC: Mellanox ConnectX-5 Ex VPI (100Gbps)
- NVMe SSD: 1.6TB Samsung PM1725a

## 2. Detailed Instructions
Now we provide how to use our scripts to reproduce the results in the paper. 

### blk-switch Evaluation
The default remote device names for blk-switch are "**/dev/nvme0n1**" for null-blk and "**/dev/nvme2n1**" for NVMe SSD. These can be configured with the "**$nvme_dev**" and "**$ssd_dev**" variables in the scripts.

1. Increasing L-app load (Figure 7):

   ```
   ./blk-switch/blk-switch_fig7.pl
   ```
   Configurable parameters:  
      - \@nr_lapps = (6, 12, 24)

2. Increasing T-app load (Figure 8):

   ```
   ./blk-switch/blk-switch_fig8.pl
   ```
   Configurable parameters:
      - \@tapp_qd = (1, 2, 4, 8, 16, 32)

3. Varying number of cores (Figure 9)

   ```
   ./blk-switch/blk-switch_fig9.pl
   ```
   Configurable parameters:
      - \@cpus = ("0", "0,4", "0,4,8", "0,4,8,12", "0,4,8,12,16", "0,4,8,12,16,20", ... )
      - \@nr_cpus = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10)

4. SSD results corresponding to Figure 7 (Figure 10)

   ```
   ./blk-switch/blk-switch_fig10.pl
   ```
   Configurable parameters:
      - \@nr_lapps = (6, 12, 24)

5. Increasing read ratio (Figure 11)
  
   ```
   ./blk-switch/blk-switch_fig11.pl
   ```
   Configurable parameters:
      - Read ratio: \@read_ratio = (100, 75, 50, 25, 0)

### Linux Evaluation
(To be updated)

### Caladan Evaluation
(To be updated)

### SPD Evaluation
(To be updated)
