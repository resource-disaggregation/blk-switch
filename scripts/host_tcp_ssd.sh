#!/bin/bash
source system_env.sh

modprobe nvme-tcp
nvme connect -t tcp -n nvme_null -a $TARGET_IP -s 4425 -q nvme_null_host -W $NR_CORES
