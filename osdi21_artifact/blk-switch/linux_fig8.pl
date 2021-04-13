#!/usr/bin/perl

######################################
# Figure 8 setting
######################################
@tapp_qd = (1, 2, 4, 8, 16, 32);

######################################
# Linux setting
######################################
$nvme_dev = "/dev/nvme1n1";
$tapp_bs = "64k";
$prio_on = 0;

######################################
# script variables
######################################
$n_input = @tapp_qd;
$repeat = 1;

# Run
system("echo 0 > /sys/module/blk_mq/parameters/blk_switch_on");
print("## Figure 8. Linux ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./load_tapp.pl $nvme_dev $tapp_bs $tapp_qd[$i] $prio_on");
        }
}

print("All done.\n");
