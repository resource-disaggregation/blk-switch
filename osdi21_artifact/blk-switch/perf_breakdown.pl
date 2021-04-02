#!/usr/bin/perl

$argc = @ARGV;
if ($argc < 1) {exit;}

####################################
# Script parameters
####################################
$nvme_dev = $ARGV[0];
$total_nr_cpus = 24;
$nr_cpus = 6;

#######################
# 1. Run
#######################
system("fio --filename=${nvme_dev} --name=random_read --ioengine=libaio --direct=1 --rw=randread --gtod_reduce=0 --cpus_allowed_policy=split --size=1G --bs=4k --time_based --runtime=7 --iodepth=1 --cpus_allowed=0 --numjobs=1 --prioclass=1 --group_reporting --output-format=terse --terse-version=3 > result_lapp &");

system("fio --filename=${nvme_dev} --name=random_read --ioengine=libaio --direct=1 --rw=randread --gtod_reduce=0 --cpus_allowed_policy=split --size=1G --bs=64k --time_based --runtime=7 --iodepth=64 --cpus_allowed=0 --numjobs=1 --prioclass=0 --group_reporting --output-format=terse --terse-version=3 > result_tapp0 &");
system("fio --filename=${nvme_dev} --name=random_read --ioengine=libaio --direct=1 --rw=randread --gtod_reduce=0 --cpus_allowed_policy=split --size=1G --bs=64k --time_based --runtime=7 --iodepth=32 --cpus_allowed=4 --numjobs=1 --prioclass=0 --group_reporting --output-format=terse --terse-version=3 > result_tapp1 &");
system("fio --filename=${nvme_dev} --name=random_read --ioengine=libaio --direct=1 --rw=randread --gtod_reduce=0 --cpus_allowed_policy=split --size=1G --bs=64k --time_based --runtime=7 --iodepth=16 --cpus_allowed=8 --numjobs=1 --prioclass=0 --group_reporting --output-format=terse --terse-version=3 > result_tapp2 &");
system("fio --filename=${nvme_dev} --name=random_read --ioengine=libaio --direct=1 --rw=randread --gtod_reduce=0 --cpus_allowed_policy=split --size=1G --bs=64k --time_based --runtime=7 --iodepth=8 --cpus_allowed=12 --numjobs=1 --prioclass=0 --group_reporting --output-format=terse --terse-version=3 > result_tapp3 &");
system("fio --filename=${nvme_dev} --name=random_read --ioengine=libaio --direct=1 --rw=randread --gtod_reduce=0 --cpus_allowed_policy=split --size=1G --bs=64k --time_based --runtime=7 --iodepth=4 --cpus_allowed=16 --numjobs=1 --prioclass=0 --group_reporting --output-format=terse --terse-version=3 > result_tapp4 &");
system("fio --filename=${nvme_dev} --name=random_read --ioengine=libaio --direct=1 --rw=randread --gtod_reduce=0 --cpus_allowed_policy=split --size=1G --bs=64k --time_based --runtime=7 --iodepth=2 --cpus_allowed=20 --numjobs=1 --prioclass=0 --group_reporting --output-format=terse --terse-version=3 > result_tapp5 &");

system("sleep 1");
system("sar -u 5 1 -P ALL > result_cpu &");
system("sleep 10");
system("grep Average result_cpu > result_cpu2");

###########################
# 2. Read results
###########################
my $f = "result_lapp";
my $result = open my $fh, "<", $f;
my $result_lapp = readline $fh;
close $fh;

my $f = "result_tapp0";
my $result = open my $fh, "<", $f;
my $result_tapp0 = readline $fh;
close $fh;

my $f = "result_tapp1";
my $result = open my $fh, "<", $f;
my $result_tapp1 = readline $fh;
close $fh;

my $f = "result_tapp2";
my $result = open my $fh, "<", $f;
my $result_tapp2 = readline $fh;
close $fh;

my $f = "result_tapp3";
my $result = open my $fh, "<", $f;
my $result_tapp3 = readline $fh;
close $fh;

my $f = "result_tapp4";
my $result = open my $fh, "<", $f;
my $result_tapp4 = readline $fh;
close $fh;

my $f = "result_tapp5";
my $result = open my $fh, "<", $f;
my $result_tapp5 = readline $fh;
close $fh;

my $f = "result_cpu2";
my $result = open my $fh, "<", $f;
my $result_cpu = readline $fh;
close $fh;

system("rm result_lapp result_tapp0 result_tapp1 result_tapp2 result_tapp3 result_tapp4 result_tapp5 result_cpu result_cpu2");

#######################
# 3. Parse and Print
#######################

# result_lapp
@attr = split(/;/, $result_lapp);
$avg_lat = $attr[15];
$the99p_lat = $attr[29];
($temp, $tail_lat) = split(/=/, $the99p_lat);
$lapp_thru = $attr[6] * 8 / 1000000;

# result_tapp
@attr = split(/;/, $result_tapp0);
$tapp0_thru = $attr[6] * 8 / 1000000;

@attr = split(/;/, $result_tapp1);
$tapp1_thru = $attr[6] * 8 / 1000000;

@attr = split(/;/, $result_tapp2);
$tapp2_thru = $attr[6] * 8 / 1000000;

@attr = split(/;/, $result_tapp3);
$tapp3_thru = $attr[6] * 8 / 1000000;

@attr = split(/;/, $result_tapp4);
$tapp4_thru = $attr[6] * 8 / 1000000;

@attr = split(/;/, $result_tapp5);
$tapp5_thru = $attr[6] * 8 / 1000000;

$total_thru = $tapp0_thru + $tapp1_thru + $tapp2_thru + $tapp3_thru + $tapp4_thru + $tapp5_thru;

# result_cpu
@attr = split(/\s+/, $result_cpu);
$cpu_all = (100.0 - $attr[7]) * $total_nr_cpus;

printf("========= Configuration ===========\n");
printf("#L-apps: 1 (core0)\n");
printf("#T-apps: 6 (cores,4,8,12,16,20)\n", $nr_tapps);
printf("========= Result ==================\n");
printf("Avg. latency       : %7.1f (us)\n", $avg_lat);
printf("Tail latency       : %7.1f (us)\n", $tail_lat);
printf("Total Throughput   : %7.1f (Gbps)\n", $total_thru);
printf("  - T-app0         : %7.1f (Gbps)\n", $tapp0_thru);
printf("  - T-app1         : %7.1f (Gbps)\n", $tapp1_thru);
printf("  - T-app2         : %7.1f (Gbps)\n", $tapp2_thru);
printf("  - T-app3         : %7.1f (Gbps)\n", $tapp3_thru);
printf("  - T-app4         : %7.1f (Gbps)\n", $tapp4_thru);
printf("  - T-app5         : %7.1f (Gbps)\n", $tapp5_thru);
printf("===================================\n\n");

