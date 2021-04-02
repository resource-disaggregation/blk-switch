#!/bin/bash
source system_env.sh

modprobe nvme-tcp
nvme connect -t tcp -n nvme_ssd -a $TARGET_IP -s 4421 -q nvme_ssd_host -W $NR_CORES
