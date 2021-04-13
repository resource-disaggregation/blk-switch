#!/usr/bin/perl

$argc = @ARGV;
if ($argc < 4) {exit;}

####################################
# Script parameters
####################################
$nvme_dev = $ARGV[0];
$total_nr_cpus = 24;
$nr_cpus = 6;
$cpus = "0,4,8,12,16,20";
$runtime = 60;
$runtime_cpu = $runtime - 2;
$runtime_total = $runtime + 5;
$nr_lapps = 6;
$nr_tapps = 6;
$tapp_bs = $ARGV[1];
$tapp_qd = $ARGV[2];
$prio_on = $ARGV[3];

##############################################
# Script commands
##############################################
$fio_lapps = "fio --filename=${nvme_dev} " .
	"--name=random_read " .
	"--ioengine=libaio " .
	"--direct=1 " .
	"--rw=randread " .
	"--gtod_reduce=0 " .
	"--cpus_allowed_policy=split " .
	"--time_based " .
	"--size=1G " .
	"--runtime=${runtime} " .
	"--cpus_allowed=${cpus} " .
	"--numjobs=${nr_lapps} " .
	"--bs=4k " .
	"--iodepth=1 ". 
	"--prioclass=${prio_on} " .
	"--group_reporting " .
	"--output-format=terse " .
	"--terse-version=3 " .
	"> result_lapp";

$fio_tapps = "fio --filename=${nvme_dev} " .
	"--name=random_read " .
	"--ioengine=libaio " .
	"--direct=1 " .
	"--rw=randread " .
	"--gtod_reduce=0 " .
	"--cpus_allowed_policy=split " .
	"--time_based " .
	"--size=1G " .
	"--runtime=${runtime} " .
	"--cpus_allowed=${cpus} " .
	"--numjobs=${nr_tapps} " .
	"--bs=${tapp_bs} " .
	"--iodepth=${tapp_qd} ".
	"--prioclass=0 " .
	"--group_reporting " .
	"--output-format=terse " .
	"--terse-version=3 " .
	"> result_tapp";

$cpu_util = "sar -u ${runtime_cpu} 1 > result_cpu";

#######################
# 1. Run
#######################
system("$fio_lapps &");
system("$fio_tapps &");
system("sleep 1");
system("$cpu_util &");
system("sleep ${runtime_total}");
system("grep Average result_cpu > result_cpu2");

###########################
# 2. Read results
###########################
my $f = "result_lapp";
my $result = open my $fh, "<", $f;
my $result_lapp = readline $fh;
close $fh;

my $f = "result_tapp";
my $result = open my $fh, "<", $f;
my $result_tapp = readline $fh;
close $fh;

my $f = "result_cpu2";
my $result = open my $fh, "<", $f;
my $result_cpu = readline $fh;
close $fh;

system("rm result_lapp result_tapp result_cpu result_cpu2");

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
@attr = split(/;/, $result_tapp);
$tapp_thru = $attr[6] * 8 / 1000000;

# result_cpu
@attr = split(/\s+/, $result_cpu);
$cpu_all = (100.0 - $attr[7]) * $total_nr_cpus;

$total_thru = $lapp_thru + $tapp_thru;
$thru_per_core = $total_thru / $cpu_all * 100;

printf("========= Configuration ===========\n");
printf("#L-apps: %2d\n", $nr_lapps);
printf("#T-apps: %2d\n", $nr_tapps);
printf("T-app I/O depth: %2d\n", $tapp_qd);
printf("========= Result ==================\n");
printf("Avg. latency       : %7.1f (us)\n", $avg_lat);
printf("Tail latency       : %7.1f (us)\n", $tail_lat);
printf("Total Throughput   : %7.1f (Gbps)\n", $total_thru);
printf("Throughput-per-core: %7.1f (Gbps)\n", $thru_per_core);
printf("===================================\n\n");

