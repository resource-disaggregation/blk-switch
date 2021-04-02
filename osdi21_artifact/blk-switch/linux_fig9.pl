#!/usr/bin/perl

######################################
# Figure 9 setting
######################################
@cpus = ("0", "0,4", "0,4,8", "0,4,8,12", "0,4,8,12,16", "0,4,8,12,16,20",
        "0,2,4,8,12,16,20",
        "0,2,4,6,8,12,16,20",
        "0,2,4,6,8,10,12,16,20",
        "0,2,4,6,8,10,12,14,16,20");
@nr_cpus = (1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

######################################
# Linux setting
######################################
$nvme_dev = "/dev/nvme1n1";
$tapp_bs = "64k";
$tapp_qd = 32;

######################################
# script variables
######################################
$n_input = @nr_cpus;
$repeat = 1;

# Run
system("echo 0 > /sys/module/blk_mq/parameters/blk_switch_on");
print("## Figure 9. Linux ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./nr_cpus.pl $nvme_dev $tapp_bs $tapp_qd $cpus[$i] $nr_cpus[$i]");
        }
}

print("All done.\n");
