#!/usr/bin/perl

$argc = @ARGV;
if ($argc < 4) {exit;}

####################################
# Script parameters
####################################
$target = "jaehyun\@128.84.155.115";
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
$read_ratio = $ARGV[3];

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
	"--prioclass=1 " .
	"--group_reporting " .
	"--output-format=terse " .
	"--terse-version=3 " .
	"> result_lapp";

$fio_tapps = "fio --filename=${nvme_dev} " .
	"--name=random_read " .
	"--ioengine=libaio " .
	"--direct=1 " .
	"--rw=randrw " .
	"--rwmixread=${read_ratio} " .
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
$cpu_util_t = "ssh ${target} 'sar -u ${runtime_cpu} 1' > result_cpu_t";

#######################
# 1. Run
#######################
system("$fio_lapps &");
system("$fio_tapps &");
system("sleep 1");
system("$cpu_util &");
system("$cpu_util_t &");
system("sleep ${runtime_total}");
system("grep Average result_cpu > result_cpu2");
system("grep Average result_cpu_t > result_cpu_t2");

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

my $f = "result_cpu_t2";
my $result = open my $fh, "<", $f;
my $result_cpu_t = readline $fh;
close $fh;

system("rm result_lapp result_tapp result_cpu result_cpu2 result_cpu_t result_cpu_t2");

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
$tapp_thru_read = $attr[6] * 8 / 1000000;
$tapp_thru_write = $attr[47] * 8 / 1000000;
$tapp_thru = $tapp_thru_read + $tapp_thru_write;

# result_cpu
@attr = split(/\s+/, $result_cpu);
$cpu_all_h = (100.0 - $attr[7]) * $total_nr_cpus;
@attr = split(/\s+/, $result_cpu_t);
$cpu_all_t = (100.0 - $attr[7]) * $total_nr_cpus;

if ($cpu_all_h >= $cpu_all_t) {
	$cpu_all = $cpu_all_h;
} else {
	$cpu_all = $cpu_all_t;
}

$total_thru = $lapp_thru + $tapp_thru;
$thru_per_core = $total_thru / $cpu_all * 100;

printf("========= Configuration ===========\n");
printf("#L-apps: %2d\n", $nr_lapps);
printf("#T-apps: %2d\n", $nr_tapps);
printf("Read(%): %2d\n", $read_ratio);
printf("========= Result ==================\n");
printf("Avg. latency       : %7.1f (us)\n", $avg_lat);
printf("Tail latency       : %7.1f (us)\n", $tail_lat);
printf("Total Throughput   : %7.1f (Gbps)\n", $total_thru);
printf("Throughput-per-core: %7.1f (Gbps)\n", $thru_per_core);
printf("===================================\n\n");

