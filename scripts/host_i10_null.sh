modprobe i10-host
nvme connect -t i10 -n i10_null -a $TARGET_IP -s 5620 -q i10_null_host
