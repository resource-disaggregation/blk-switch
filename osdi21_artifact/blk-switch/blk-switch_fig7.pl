#!/usr/bin/perl

######################################
# Figure 7 setting
######################################
@nr_lapps = (6, 12, 24);

######################################
# blk-switch setting
######################################
$nvme_dev = "/dev/nvme0n1";
$tapp_bs = "64k";
$tapp_qd = 16;

######################################
# script variables
######################################
$n_input = @nr_lapps;
$repeat = 1;

# Run
system("echo 2 > /sys/module/blk_mq/parameters/blk_switch_on");
print("## Figure 7. blk-switch ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./nr_lapp.pl $nvme_dev $tapp_bs $tapp_qd $nr_lapps[$i]");
        }
}

print("All done.\n");
