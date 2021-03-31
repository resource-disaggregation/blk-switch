#!/usr/bin/perl

######################################
# Figure 11 setting
######################################
@read_ratio = (100, 75, 50, 25, 0);

######################################
# Linux setting
######################################
$nvme_dev = "/dev/nvme1n1";
$tapp_bs = "64k";
$tapp_qd = 32;

######################################
# script variables
######################################
$n_input = @read_ratio;
$repeat = 1;

# Run
print("## Figure 11. blk-switch ##\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./read_ratio.pl $nvme_dev $tapp_bs $tapp_qd $read_ratio[$i]");
        }
}

print("All done.\n");
