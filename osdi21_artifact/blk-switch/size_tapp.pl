#!/usr/bin/perl

$argc = @ARGV;
if ($argc < 4) {exit;}

####################################
# Script parameters
####################################
$nvme_dev = $ARGV[0];
$total_nr_cpus = 24;
$nr_cpus = 0;
$cpus = "0";
$runtime = 60;
$runtime_cpu = $runtime - 2;
$runtime_total = $runtime + 5;
$nr_lapps = 1;
$nr_tapps = 1;
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

#######################
# 1. Run
#######################
system("$fio_lapps &");
system("$fio_tapps &");
system("sleep ${runtime_total}");

###########################
# 2. Read results
###########################
my $f = "result_lapp";
my $result = open my $fh, "<", $f;
my $result_lapp = readline $fh;
close $fh;

system("rm result_lapp result_tapp");

#######################
# 3. Parse and Print
#######################
# result_lapp
@attr = split(/;/, $result_lapp);
$the99p_lat = $attr[29];
($temp, $tail_lat) = split(/=/, $the99p_lat);

printf("========= Configuration ===========\n");
printf("#L-apps: %2d\n", $nr_lapps);
printf("#T-apps: %2d\n", $nr_tapps);
printf("T-app I/O size: %2d\n", $tapp_bs);
printf("========= Result ==================\n");
printf("Tail latency       : %7.1f (us)\n", $tail_lat);
printf("===================================\n\n");

