#!/usr/bin/perl

######################################
# Figure 7 setting
######################################
@nr_lapps = (6, 12, 24);

######################################
# Linux setting
######################################
$nvme_dev = "/dev/nvme1n1";
$ssd_dev = "/dev/nvme3n1";
$tapp_bs = "64k";
$tapp_qd = 32;
$prio_on = 0;

######################################
# script variables
######################################
$n_input = @nr_lapps;
$repeat = 1;

# Run
system("echo 0 > /sys/module/blk_mq/parameters/blk_switch_on");
print("## Figure 10. Linux ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./nr_lapp_ssd.pl $nvme_dev $ssd_dev $tapp_bs $tapp_qd $nr_lapps[$i] $prio_on");
        }
}

print("All done.\n");
