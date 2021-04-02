###########################################################################
# Target-side configurations
###########################################################################
target_ip=$TARGET_IP
nr_cores=$NR_CORES

# Load i10 and nvme-tcp target kernel modules
modprobe nvmet nvmet-tcp i10-target

# Create nvme-tcp subsystem
# - subsystem name: nvme_ssd
# - device: /dev/nvme0n1
# - protocol: tcp
# - listening port: 4421
mkdir /sys/kernel/config/nvmet/subsystems/nvme_ssd
cd /sys/kernel/config/nvmet/subsystems/nvme_ssd

echo 1 > attr_allow_any_host
mkdir namespaces/10
cd namespaces/10
echo -n /dev/nvme0n1 > device_path
echo 1 > enable

mkdir /sys/kernel/config/nvmet/ports/2
cd /sys/kernel/config/nvmet/ports/2
echo $target_ip > addr_traddr
echo tcp > addr_trtype
echo 4421 > addr_trsvcid
echo ipv4 > addr_adrfam

ln -s /sys/kernel/config/nvmet/subsystems/nvme_ssd /sys/kernel/config/nvmet/ports/2/subsystems/nvme_ssd


# Create i10_null subsystem
# - subsystem name: i10_ssd
# - device: /dev/nvme0n1
# - protocol: i10
# - listening port: 4422
mkdir /sys/kernel/config/nvmet/subsystems/i10_ssd
cd /sys/kernel/config/nvmet/subsystems/i10_ssd

echo 1 > attr_allow_any_host
mkdir namespaces/10
cd namespaces/10
echo -n /dev/nvme0n1 > device_path
echo 1 > enable

mkdir /sys/kernel/config/nvmet/ports/22
cd /sys/kernel/config/nvmet/ports/22
echo $target_ip > addr_traddr
echo i10 > addr_trtype
echo 4422 > addr_trsvcid
echo ipv4 > addr_adrfam

ln -s /sys/kernel/config/nvmet/subsystems/i10_ssd /sys/kernel/config/nvmet/ports/22/subsystems/i10_ssd
