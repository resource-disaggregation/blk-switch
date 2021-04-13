#!/usr/bin/perl

######################################
# Figure 8 setting
######################################
#@tapp_qd = (1, 2, 4, 8, 16, 32);
@tapp_qd = (32);

######################################
# blk-switch setting
######################################
$nvme_dev = "/dev/nvme0n1";
$tapp_bs = "64k";
$prio_on = 1;

######################################
# script variables
######################################
$n_input = @tapp_qd;
$repeat = 1;

# Run
system("echo 2 > /sys/module/blk_mq/parameters/blk_switch_on");
system("echo 16 > /sys/module/blk_mq/parameters/blk_switch_thresh_B");
print("## Figure 8. blk-switch ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./load_tapp.pl $nvme_dev $tapp_bs $tapp_qd[$i] $prio_on");
        }
}

print("All done.\n");
