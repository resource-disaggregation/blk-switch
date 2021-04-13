#!/usr/bin/perl

######################################
# Figure 11 setting
######################################
@read_ratio = (0, 25, 50, 75, 100);

######################################
# blk-switch setting
######################################
$nvme_dev = "/dev/nvme0n1";
$tapp_bs = "64k";
$tapp_qd = 16;
$prio_on = 1;

######################################
# script variables
######################################
$n_input = @read_ratio;
$repeat = 1;

# Run
system("echo 2 > /sys/module/blk_mq/parameters/blk_switch_on");
system("echo 16 > /sys/module/blk_mq/parameters/blk_switch_thresh_B");
print("## Figure 11. blk-switch ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./read_ratio.pl $nvme_dev $tapp_bs $tapp_qd $read_ratio[$i] $prio_on");
        }
}

print("All done.\n");
