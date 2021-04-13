#!/usr/bin/perl

######################################
# Figure 13 setting
######################################
# Please edit @nvme_dev if needed
# /dev/nvme0n1: null-blk for blk-switch
# /dev/nvme1n1: null-blk for Linux

@nvme_dev = ("/dev/nvme1n1", "/dev/nvme0n1", "/dev/nvme0n1", "/dev/nvme0n1", "/dev/nvme0n1");
@scheme = ("Linux", "Linux+P", "Linux+P+RS", "Linux+P+RS+AS", "(Extra)");
@kern_parm = (0, 0, 1, 2, 2);

######################################
# blk-switch setting
######################################

######################################
# script variables
######################################
$n_input = @nvme_dev;
$repeat = 1;

# Run
system("echo 1 > /sys/module/blk_mq/parameters/blk_switch_debug");
system("echo 0 > /sys/module/nvme_tcp/parameters/i10_thru_nice");
system("echo 8 > /sys/module/blk_mq/parameters/blk_switch_thresh_B");

for($i=0; $i<$n_input; $i++)
{
	system("echo $kern_parm[$i] > /sys/module/blk_mq/parameters/blk_switch_on");
	print("## Figure 13. $scheme[$i] ##\n\n");

        for($j=0; $j<$repeat; $j++)
        {
                system("./perf_breakdown.pl $nvme_dev[$i]");
        }
}

system("echo 0 > /sys/module/blk_mq/parameters/blk_switch_debug");
system("echo -19 > /sys/module/nvme_tcp/parameters/i10_thru_nice");
print("All done.\n");
