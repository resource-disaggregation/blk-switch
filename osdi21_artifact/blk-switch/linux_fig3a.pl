#!/usr/bin/perl

######################################
# Figure 2 setting
######################################
@tapp_bs = ("4k", "16k", "32k", "64k", "128k");

######################################
# Linux setting
######################################
$nvme_dev = "/dev/nvme1n1";
$tapp_qd = 32;
$prio_on = 0;

######################################
# script variables
######################################
$n_input = @tapp_bs;
$repeat = 1;

# Run
system("echo 0 > /sys/module/blk_mq/parameters/blk_switch_on");
print("## Figure 3(a). Linux ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./size_tapp.pl $nvme_dev $tapp_bs[$i] $tapp_qd $prio_on");
        }
}

print("All done.\n");
