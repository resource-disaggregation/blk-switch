#!/usr/bin/perl

######################################
# Figure 7 setting
######################################
@nr_lapps = (6, 12, 24);

######################################
# Linux setting
######################################
$nvme_dev = "/dev/nvme1n1";
$tapp_bs = "64k";
$tapp_qd = 32;

######################################
# script variables
######################################
$n_input = @nr_lapps;
$repeat = 1;

# Run
print("## Figure 7. Linux ##\n\n");

for($i=0; $i<$n_input; $i++)
{
        for($j=0; $j<$repeat; $j++)
        {
                system("./nr_lapp.pl $nvme_dev $tapp_bs $tapp_qd $nr_lapps[$i]");
        }
}

print("All done.\n");
