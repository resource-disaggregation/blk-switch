modprobe i10-host
nvme connect -t i10 -n i10_ssd -a $TARGET_IP -s 4422 -q i10_ssd_host
