#!/usr/bin/perl

######################################
# Figure 2 setting
######################################
@nr_lapps = (1, 1, 2, 4);
@isolated = (1, 0, 0, 0);

######################################
# Linux setting
######################################
$nvme_dev = "/dev/nvme1n1";
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
print("## Figure 2. Linux ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./nr_lapp_single.pl $nvme_dev $tapp_bs $tapp_qd $nr_lapps[$i] $prio_on $isolated[$i]");
        }
}

print("All done.\n");
