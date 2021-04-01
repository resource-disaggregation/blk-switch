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
# blk-switch setting
######################################
$nvme_dev = "/dev/nvme0n1";
$tapp_bs = "64k";
$tapp_qd = 16;

######################################
# script variables
######################################
@kernel_conf = (4, 8, 12, 16, 20, 24, 24, 24, 24, 24);
$n_input = @nr_cpus;
$repeat = 1;

# Run
print("## Figure 9. blk-switch ##\n\n");

for($i=0; $i<$n_input; $i++)
{
	system("echo $kernel_conf[$i] > /sys/module/blk_mq/parameters/blk_switch_nr_cpus; sleep 1");
 
        for($j=0; $j<$repeat; $j++)
        {
                system("./nr_cpus.pl $nvme_dev $tapp_bs $tapp_qd $cpus[$i] $nr_cpus[$i]");
        }
}

print("All done.\n");
